/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "mbed_retarget.h"
#include "uwp_wifi_cmdevt.h"
#include "uwp_netif.h"
#include "uwp_psk.h"

//#define WIFI_LOG_DBG
//#define WIFI_DUMP
#include "uwp_log.h"

#define RECV_BUF_SIZE (128)
#define ALL_2_4_GHZ_CHANNELS (0X3FFF)
#define CHANNEL_2_4_GHZ_BIT(n) (1 << (n - 1))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

extern struct wifi_priv uwp_wifi_ap_priv;
extern void *g_wifi_mgmt_queue;
extern struct list_head g_scan_list;


static unsigned char recv_buf[RECV_BUF_SIZE];
static unsigned int recv_len;
static unsigned int seq = 1;
static void *cmd_sem = NULL;
static int scan_ap_cnt = 0;

static const u16_t CRC_table[] = {
	0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00,
	0x2800, 0xE401, 0xA001, 0x6C00, 0x7800, 0xB401,
	0x5000, 0x9C01, 0x8801, 0x4400,
};

/* 5G channel list. */
static const u16_t channels_5g_scan_table[] = {
	36, 40, 44, 48, 52,
	56, 60, 64, 100, 104,
	108, 112, 116, 120, 124,
	128, 132, 136, 140, 144,
	149, 153, 157, 161, 165
};

static u16_t CRC16(const u8_t *buf, u16_t len)
{
	u16_t CRC = 0xFFFF;
	u16_t i;
	u8_t ch_char;

	for (i = 0; i < len; i++) {
		ch_char = *buf++;
		CRC = CRC_table[(ch_char ^ CRC) & 15] ^ (CRC >> 4);
		CRC = CRC_table[((ch_char >> 4) ^ CRC) & 15] ^ (CRC >> 4);
	}
	return CRC;
}

static inline int check_cmdevt_len(int input_len, int expected_len)
{
	if (input_len != expected_len) {
		LOG_ERR("Invalid len %d, expected len %d",
				input_len, expected_len);
		return -EINVAL;
	}

	return 0;
}

int wifi_cmd_load_ini(const u8_t *data, u32_t len, u8_t sec_num)
{
	int ret;
	u16_t crc = 0;
	int cmd_len;
	struct cmd_download_ini *cmd;

	/* Calculate total command length. */
	cmd_len = sizeof(struct cmd_download_ini) + len + sizeof(crc);

	cmd = malloc(cmd_len);
	if (!cmd) {
		LOG_ERR("Cmd is null");
		return -ENOMEM;
	}

	/* Calc CRC value. */
	crc = CRC16(data, len);
	LOG_DBG("Sec: %d, len: %d cmd_len:%d, CRC value: 0x%x",
		    sec_num, len, cmd_len, crc);

	memset(cmd, 0, cmd_len);
	cmd->sec_num = sec_num;

	/* Copy data after section num. */
	memcpy(cmd->data, data, len);
	/* Put CRC value at the tail of INI data. */
	memcpy(cmd->data + len, &crc, sizeof(crc));

	ret = wifi_cmd_send(WIFI_CMD_DOWNLOAD_INI, (char *)cmd,
			    cmd_len, NULL, 0);
	if (ret) {
		LOG_ERR("Load ini send cmd fail");
		free(cmd);
		return ret;
	}

	free(cmd);

	return 0;
}

int wifi_cmd_scan(struct wifi_device *wifi_dev, struct wifi_drv_scan_params *params)
{
    int ret;
    int ssid_len = 0;
    int cmd_len = 0;
    u8_t *cmd = NULL; //	use a u8_t* instead of struct cmd_scan*, because the len of ssid[] is unfixed.
    u16_t channels_5g_cnt = 0;
    u8_t band = params->band;
    u8_t channel = params->channel;
    u32_t *u2g_channel_list;
    u16_t *u5g_channel_cnt;
    u16_t *u5g_channel_list;

    if(params->ssid_len)
        ssid_len = 1 + params->ssid_len;
	cmd_len = sizeof(struct cmd_scan) + ssid_len;

    if (band != WIFI_BAND_2_4G) {
        if ((band != WIFI_BAND_5G) && (channel != 0)) {
            LOG_ERR("Invalid band %d, channel %d.",
					band, channel);
			return -EINVAL;
        }
        if (channel == 0) //all 5g channels
            cmd_len +=  sizeof(channels_5g_scan_table);
        else
            cmd_len += 1;
    }

	cmd = malloc(cmd_len);
	if (!cmd) {
		LOG_ERR("Cmd is null");
		return -ENOMEM;
	}
    memset(cmd, 0, cmd_len);

    if (params->ssid_len) {
        printf("%s: ssid:%s strlen(ssid):%d\r\n", __func__, params->ssid, params->ssid_len);
        ((struct cmd_scan*)cmd)->ssid_len = ssid_len;
        ((struct cmd_scan*)cmd)->ssid[0] = params->ssid_len;
        memcpy(((struct cmd_scan*)cmd)->ssid+1, params->ssid, params->ssid_len);
    } else
        printf("%s: ssid:NULL\r\n", __func__);

    u2g_channel_list = (u32_t*)(cmd + sizeof(struct trans_hdr));
    u5g_channel_cnt = (u16_t*)(((struct cmd_scan*)cmd)->ssid + ssid_len);
    u5g_channel_list = u5g_channel_cnt+1;

	switch (band) {
	case WIFI_BAND_2_4G:

		if (channel == 0) { /* All 2.4GH channel */
			*u2g_channel_list = ALL_2_4_GHZ_CHANNELS;
		} else { /* One channel */
			*u2g_channel_list = CHANNEL_2_4_GHZ_BIT(channel);
		}
		break;
	case WIFI_BAND_5G:
		if (channel == 0) { /* All 5GHZ channel */
			*u5g_channel_cnt = ARRAY_SIZE(channels_5g_scan_table);
			memcpy(u5g_channel_list, channels_5g_scan_table,
					sizeof(channels_5g_scan_table));
		} else { /* One channel */
			*u5g_channel_cnt = 1;
			u5g_channel_list[0] = channel;
		}
		break;
	default:
		if (channel == 0) { /* All channels */
			*u5g_channel_cnt = ARRAY_SIZE(channels_5g_scan_table);
			memcpy(u5g_channel_list, channels_5g_scan_table,
					sizeof(channels_5g_scan_table));
			*u2g_channel_list = ALL_2_4_GHZ_CHANNELS;
		} else {
			LOG_ERR("Invalid band %d, channel %d.",
					band, channel);
			return -EINVAL;
		}
		break;
	}

	ret = wifi_cmd_send(WIFI_CMD_SCAN, (char *)cmd,
			    cmd_len, NULL, NULL);
	if (ret) {
		LOG_ERR("Scan send cmd fail");
		free(cmd);
		return ret;
	}

	free(cmd);

	return 0;
}

int wifi_cmd_connect(struct wifi_device *wifi_dev,
		     struct wifi_drv_connect_params *params)
{
	int ret;
	struct cmd_connect cmd;
	u8_t wpa_psk[WIFI_PMK_LEN] = {0};

	memset(&cmd, 0, sizeof(cmd));

	if (!params->ssid) {
		LOG_ERR("Invalid SSID.");
		return -EINVAL;
	}
	if (params->psk_length && params->psk) {
		ret = pbkdf2_sha1(params->psk, params->ssid, WIFI_PMK_ITER,
			wpa_psk, WIFI_PMK_LEN);
		if (ret) {
			LOG_ERR("failed to calculate PSK! %d\n", ret);
			return ret;
		}
	}

	cmd.channel = params->channel;
	cmd.ssid_len = params->ssid_length;
	cmd.psk_len = params->psk_length? WIFI_PMK_LEN:0;

	memcpy(cmd.ssid, params->ssid, cmd.ssid_len);
	if (params->psk_length && params->psk)
		memcpy(cmd.psk, wpa_psk, WIFI_PMK_LEN);

    DUMP_DATA(&cmd, sizeof(struct cmd_connect));

	ret = wifi_cmd_send(WIFI_CMD_CONNECT, (char *)&cmd,
			    sizeof(cmd), NULL, NULL);
	if (ret) {
		LOG_ERR("Connect send cmd fail");
		return ret;
	}

	return 0;
}

int wifi_cmd_disconnect(struct wifi_device *wifi_dev)
{
	int ret;
	struct cmd_disconnect cmd;

	cmd.reason_code = 0;

	ret = wifi_cmd_send(WIFI_CMD_DISCONNECT, (char *)&cmd,
			    sizeof(cmd), NULL, NULL);
	if (ret) {
		LOG_ERR("Disconnect send cmd fail");
		return ret;
	}

	return 0;
}

int wifi_cmd_get_cp_info(struct wifi_priv *priv)
{
	struct cmd_get_cp_info cmd;
	int ret;
	int rlen;

	memset(&cmd, 0, sizeof(cmd));

	ret = wifi_cmd_send(WIFI_CMD_GET_CP_INFO, (char *)&cmd,
			sizeof(struct trans_hdr), (char *)&cmd, &rlen);
	if (ret) {
		LOG_ERR("Get cp info send cmd fail");
		return ret;
	}

	priv->cp_version = cmd.version;

	/* Set sta mac */
	memcpy(priv->wifi_dev[WIFI_DEV_STA].mac, cmd.mac, ETH_ALEN);
	/* Set softap mac */
	cmd.mac[4] ^= 0x80;
	memcpy(priv->wifi_dev[WIFI_DEV_AP].mac, cmd.mac, ETH_ALEN);

	/* Store maximum station on softap. */
	priv->wifi_dev[WIFI_DEV_AP].max_sta_num = cmd.max_ap_assoc_sta_num;

	/* Store maximum stations in blacklist on softap. */
	priv->wifi_dev[WIFI_DEV_AP].max_blacklist_num =
		cmd.max_ap_blacklist_sta_num;

	LOG_INF("CP version: 0x%x", priv->cp_version);

	LOG_DBG("Max sta num: %d", cmd.max_ap_assoc_sta_num);
	LOG_DBG("Max blacklist num: %d", cmd.max_ap_blacklist_sta_num);

	return 0;
}

int wifi_cmd_open(struct wifi_device *wifi_dev)
{
	struct cmd_open cmd;
	int ret;

	LOG_DBG("Open mode %d", wifi_dev->mode);

	memset(&cmd, 0, sizeof(cmd));
	cmd.mode = wifi_dev->mode;
	memcpy(cmd.mac, wifi_dev->mac, ETH_ALEN);
    DUMP_DATA(&cmd, sizeof(cmd));
	ret = wifi_cmd_send(WIFI_CMD_OPEN, (char *)&cmd, sizeof(cmd),
			    NULL, NULL);
	if (ret) {
		LOG_ERR("Open mode %d send cmd fail", wifi_dev->mode);
		return ret;
	}
	LOG_DBG("Open mode success.");

	return 0;
}
#if 0
int wifi_cmd_close(struct wifi_device *wifi_dev)
{
	struct cmd_stop cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.mode = wifi_dev->mode;
	memcpy(cmd.mac, wifi_dev->mac, 6);

	ret = wifi_cmd_send(WIFI_CMD_CLOSE, (char *)&cmd, sizeof(cmd),
			    NULL, NULL);
	if (ret) {
		LOG_ERR("Stop mode:%d send cmd fail", wifi_dev->mode);
		return ret;
	}

	return 0;
}

int wifi_cmd_start_ap(struct wifi_device *wifi_dev,
		struct wifi_drv_start_ap_params *params)
{
	struct cmd_start_ap cmd;
	int ret;

	LOG_DBG("Start ap at channel: %d.", params->channel);
	memset(&cmd, 0, sizeof(cmd));

	if (params->ssid_length > 0) {
		memcpy(cmd.ssid, params->ssid, params->ssid_length);
		cmd.ssid_len = params->ssid_length;
		LOG_DBG("Ssid: %s(%d).", cmd.ssid, cmd.ssid_len);
	}
	if (params->psk_length > 0) {
		memcpy(cmd.password, params->psk, params->psk_length);
		cmd.password_len = params->psk_length;
		LOG_DBG("Psk: %s(%d).", cmd.password, cmd.password_len);
	}

	cmd.channel = params->channel;
	ret = wifi_cmd_send(WIFI_CMD_START_AP, (char *)&cmd,
			    sizeof(cmd), NULL, NULL);
	if (ret) {
		LOG_ERR("Ap start send cmd fail %d", ret);
		return ret;
	}
	LOG_DBG("Start ap ok.");

	return 0;
}

int wifi_cmd_stop_ap(struct wifi_device *wifi_dev)
{
	/* Stop ap is not supported by CP */
	return 0;
}


/*
 * @param r_buf: address of return value
 * @param r_len: length of return value
 */
int wifi_cmd_npi_send(struct device *dev, int ictx_id,
		char *t_buf, u32_t t_len, char *r_buf,
		u32_t *r_len)
{
	struct cmd_npi *cmd;
	int ret;
	int cmd_len;

	/* Calculate total command length. */
	cmd_len = sizeof(*cmd) + t_len;

	cmd = k_malloc(cmd_len);
	if (!cmd) {
		LOG_ERR("Cmd is null");
		return -ENOMEM;
	}

	memset(cmd, 0, cmd_len);

	memcpy(cmd->data, t_buf, t_len);

	ret = wifi_cmd_send(WIFI_CMD_NPI_MSG, (char *)cmd,
			cmd_len, r_buf, r_len);
	if (ret) {
		LOG_ERR("Npi send cmd fail %d", ret);
		k_free(cmd);
		return ret;
	}

	if (r_buf && r_len) {
		*r_len = *r_len - sizeof(*cmd);
		/* No need to copy trans_hdr. */
		memcpy(r_buf, r_buf + sizeof(*cmd), *r_len);
	}

	k_free(cmd);

	return 0;
}

int wifi_cmd_npi_get_mac(struct device *dev, char *buf)
{
	struct wifi_device *wifi_dev;

	if (!dev || !buf) {
		return -EINVAL;
	}

	wifi_dev = get_wifi_dev_by_dev(dev);
	if (!wifi_dev) {
		LOG_ERR("Unable to find wifi dev by dev %p", dev);
		return -EINVAL;
	}

	memcpy(buf, wifi_dev->mac, ETH_ALEN);

	return 0;
}


int wifi_cmd_set_ip(struct wifi_device *wifi_dev, u8_t *ip_addr, u8_t len)
{
	int ret;
	struct cmd_set_ip cmd;

	memset(&cmd, 0, sizeof(cmd));
	if (len == IPV4_LEN) {
		/*
		 * Temporarily supported 4-byte ipv4 address.
		 * TODO: support ipv6 address, need to reserve more bytes.
		 */
		memcpy(cmd.ip, ip_addr, len);
		/* Store ipv4 address in wifi device. */
		memcpy(wifi_dev->ipv4_addr, ip_addr, len);
	} else {
		LOG_WRN("Currently only ipv4, 4 bytes.");
		return -EINVAL;
	}

	ret = wifi_cmd_send(WIFI_CMD_SET_IP, (char *)&cmd,
			    sizeof(cmd), NULL, NULL);
	if (ret) {
		LOG_ERR("Set ip send cmd fail");
		return ret;
	}

	LOG_DBG("Set ip ok.");

	return 0;
}
#endif
// TODO: temporary for scan
struct event_scan_result scan_ret[20];
int scan_cnt;
extern void *scan_done_sem;

static int wifi_evt_scan_result(struct wifi_device *wifi_dev,
		char *data, int len)
{
	struct event_scan_result *event = (struct event_scan_result *)data;

	if (check_cmdevt_len(len, sizeof(struct event_scan_result))) {
		return -EINVAL;
	}

    scan_result_info_t *temp = NULL;
    temp = (scan_result_info_t *)malloc(sizeof(scan_result_info_t));
    if(temp == NULL){
        LOG_ERR("malloc failed");
        return -ENOMEM;
    }

    memcpy(&(temp->res), event, len);
    list_add_tail(&temp->res_list, &g_scan_list);
    scan_ap_cnt ++;
    LOG_DBG("scan malloc:%p", temp);
    //LOG_DBG("%s %d %d",temp->res.ssid, temp->res.encrypt_mode, temp->res.rssi);

	return 0;
}

static int wifi_evt_scan_done(struct wifi_device *wifi_dev, char *data, int len)
{
	struct event_scan_done *event = (struct event_scan_done *)data;

	if (check_cmdevt_len(len, sizeof(struct event_scan_done))) {
		return -EINVAL;
	}

    uwp_wifi_msg_t msg = malloc(sizeof(struct UWP_MSG_STRUCT));
    if(msg == NULL){
        LOG_ERR("mlloc failed");
        return -ENOMEM;
    }
    msg->type = STA_SCAN_TYPE;
    msg->arg1 = scan_ap_cnt;
    k_msg_put(g_wifi_mgmt_queue, &msg, osWaitForever);
    scan_ap_cnt = 0;

    LOG_DBG("STA SCAN DONE");

    return 0;
}

static int wifi_evt_connect(struct wifi_device *wifi_dev, char *data, int len)
{
	struct event_connect *event =
		(struct event_connect *)data;

	if (check_cmdevt_len(len, sizeof(struct event_connect))) {
		return -EINVAL;
	}
    LOG_DBG("%s",__func__);
    if(!event->status){
        LOG_DBG("connect successfully");
        wifi_dev->connect_cb(1);
    }
#if 0
	if (wifi_dev->connect_cb) {
		wifi_dev->connect_cb(wifi_dev->iface, event->status);
	} else {
		LOG_WRN("No connect callback.");
	}

	wifi_dev->connected = true;
#endif
	return 0;
}

static int wifi_evt_disconnect(struct wifi_device *wifi_dev,
		char *data, int len)
{
	struct event_disconnect *event =
		(struct event_disconnect *)data;

	if (check_cmdevt_len(len, sizeof(struct event_disconnect))) {
		return -EINVAL;
	}
    LOG_DBG("%s reason code:%d",__func__,event->reason_code);
	if (wifi_dev->disconnect_cb) {
		wifi_dev->disconnect_cb(0);
	} else {
		LOG_WRN("No disconnect callback.");
	}

	wifi_dev->connected = false;

	return 0;
}
#if 0
static int wifi_evt_new_sta(struct wifi_device *wifi_dev, char *data, int len)
{
	struct event_new_station *event =
		(struct event_new_station *)data;

	if (check_cmdevt_len(len, sizeof(struct event_new_station))) {
		return -EINVAL;
	}

	if (wifi_dev->new_station_cb) {
		wifi_dev->new_station_cb(wifi_dev->iface,
				event->is_connect,
				event->mac);
	} else {
		LOG_WRN("No new_station callback.");
	}

	return 0;
}
#endif

int wifi_cmdevt_process(struct wifi_priv *priv, char *data, int len)
{
	struct trans_hdr *hdr = (struct trans_hdr *)data;

	if (len > RECV_BUF_SIZE) {
		LOG_ERR("Invalid data len %d.", len);
		return -EINVAL;
	}

	/* Receive command response. */
	if (hdr->response == 1) {
		if (len > 0) {
			memcpy(recv_buf, data, len);
			recv_len = len;
		}

		k_sem_release(cmd_sem);

		/*
		 * Release command wait semaphore, and switch current thread to
		 * command process thread. This routine could prevent the send
		 * command timeout if there are many data recived from CP.
		 */
	    osThreadYield();

		return 0;
	}

	LOG_DBG("Receive event type 0x%x.", hdr->type);

	len -= sizeof(*hdr);

	/* Receive Events */
	switch (hdr->type) {
	case WIFI_EVENT_SCAN_RESULT:
		wifi_evt_scan_result(&priv->wifi_dev[WIFI_DEV_STA],
				hdr->data, len);
		break;
	case WIFI_EVENT_SCAN_DONE:
		wifi_evt_scan_done(&priv->wifi_dev[WIFI_DEV_STA],
				hdr->data, len);
		break;
	case WIFI_EVENT_DISCONNECT:
		wifi_evt_disconnect(&priv->wifi_dev[WIFI_DEV_STA],
				hdr->data, len);
		break;
	case WIFI_EVENT_CONNECT:
		wifi_evt_connect(&priv->wifi_dev[WIFI_DEV_STA],
				hdr->data, len);
		break;
	case WIFI_EVENT_NEW_STATION:
		LOG_DBG("wifi_evt_new_sta");
		/*wifi_evt_new_sta(&priv->wifi_dev[WIFI_DEV_AP],
				hdr->data, len);*/
		break;
	default:
		break;
	}
	return 0;
}

int wifi_cmd_send(u8_t cmd, char *data, int len, char *rbuf, int *rlen)
{
	int ret = 0;

	struct trans_hdr *hdr = (struct trans_hdr *)data;

	if (cmd > WIFI_CMD_MAX || cmd < WIFI_CMD_BEGIN) {
		LOG_ERR("Invalid command %d ", cmd);
		return -EINVAL;
	}

	if (data && len == 0) {
		LOG_ERR("Data len Invalid,data=%p,len=%d", data, len);
		return -EINVAL;
	}

	hdr->len = len;
	hdr->type = cmd;
	hdr->seq = seq++;

	ret = wifi_tx_cmd(data, len);
	if (ret < 0) {
		LOG_ERR("tx cmd fail %d", ret);
		return ret;
	}

	ret = k_sem_acquire(cmd_sem, 3000);
	if (ret) {
		LOG_ERR("Wait cmd(%d) timeout.", cmd);
		return ret;
	}

	hdr = (struct trans_hdr *)recv_buf;
	if (hdr->status != 0) {
		LOG_ERR("Invalid cmd status: %i", hdr->status);
		return hdr->status;
	}

	if (rbuf) {
		memcpy(rbuf, recv_buf, recv_len);
	}
	if (rlen) {
		*rlen = recv_len;
	}

	LOG_DBG("Get command response success");
	return 0;
}

int wifi_cmdevt_init(void)
{
	if(cmd_sem == NULL)
		cmd_sem = k_sem_create(1, 0);
	else {
		LOG_DBG("wifi cmd sem initilized");
		return -1;
	}
	if(cmd_sem == NULL){
        LOG_ERR("wifi cmd sem initilize failed\r\n");
		return -1;
	}
	return 0;
}
