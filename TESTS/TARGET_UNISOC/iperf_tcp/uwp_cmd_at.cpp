#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uwp_cmd_at.h"
#include "console.h"
#include "uwpWiFiInterface.h"
#include "lwiperf.h"

UWPWiFiInterface wifi;
char conn_flag = 0;
static char ssid[32+1];
static char pw[64+1];

int conn(char *ssid, char *pw, char *bssid)
{
    int ret;
	WiFiAccessPoint res;
	const char *ip_addr;
	ret = wifi.init();
	if(ret != NSAPI_ERROR_OK){
        RDA_AT_PRINT("wifi init failed\r\n");
        return -1;
    }
    //wifi.scan(&res,1);
    ret = wifi.connect(ssid, pw, NSAPI_SECURITY_NONE, 0);
    if(ret != 0){
        RDA_AT_PRINT("wifi connect failed: %s\n", ret);
        return -1;
    }
    ip_addr = wifi.get_ip_address();
	
    if (ip_addr) {
        RDA_AT_PRINT("Client IP Address is %s\r\n", ip_addr);
        return 0;
    } else {
        RDA_AT_PRINT("No Client IP Address\r\n");
        return -1;
    }
}
int do_wsscan( cmd_tbl_t *cmd, int argc, char *argv[])
{
    wifi.scan(NULL,0);

    return 0;
}

int do_wsconn( cmd_tbl_t *cmd, int argc, char *argv[])
{
    int ret, flag;

    if (argc < 1) {
        RESP_ERROR(ERROR_ARG);
        return ERROR_ARG;
    }

    if(conn_flag == 1){
        RESP_ERROR(ERROR_ABORT);
        RDA_AT_PRINT("error! Has been connected!");
        return ERROR_ABORT;
    }

    memset(ssid, 0, sizeof(ssid));
    memset(pw, 0, sizeof(pw));

    if(argc > 1)
        memcpy(ssid, argv[1], strlen(argv[1]));

    if(argc > 2)
        memcpy(pw, argv[2], strlen(argv[2]));

    if (strlen(ssid) == 0) {
        RESP_ERROR(ERROR_ARG);
        return ERROR_ARG;
    }

    RDA_AT_PRINT("ssid %s pw %s\r\n", ssid, pw);

    ret = conn(ssid, pw, NULL);
	
    if(ret == 0){
		conn_flag = 1;
        RESP_OK();
    }else{
        RESP_ERROR(ERROR_FAILE);
    }
    return 0;
}

int do_disconn( cmd_tbl_t *cmd, int argc, char *argv[])
{
	int ret;
	
    if(conn_flag == 0){
        RESP_ERROR(ERROR_ABORT);
        return 0;
    }
	conn_flag = 0;
    ret = wifi.disconnect();
	if(ret != 0){
		RESP_ERROR(ERROR_FAILE);
	}else{	
    	RESP_OK();
	}
    return 0;
}

int do_h( cmd_tbl_t *cmd, int argc, char *argv[])
{
    cmd_tbl_t *cmdtemp = NULL;
    int i;

    for (i = 0; i<(int)cmd_cntr; i++) {
        cmdtemp = &cmd_list[i];
        if(cmdtemp->usage) {
            printf("%s\r\n", cmdtemp->usage);
        }
    }
    return 0;
}

int do_nstart(cmd_tbl_t *cmd, int argc, char *argv[]) {
    if (conn_flag == 0) {
        printf("Please connect ap!\r\n");
        RESP_ERROR(ERROR_ABORT);
        return 0;
    }
    lwiperf_start_tcp_server_default(NULL,NULL);

    return 0;
}

int do_cstart(cmd_tbl_t *cmd, int argc, char *argv[])
{
    char SERVER_ADDR[20];
    int timeout;

     if (argc < 1) {
        printf("Please set server ip and server port!\r\n");
        RESP_ERROR(ERROR_ARG);
        return 0;
    }
    if (conn_flag == 0) {
        printf("Please connect ap!\r\n");
        RESP_ERROR(ERROR_ABORT);
        return 0;
    }
    memcpy(SERVER_ADDR, argv[1], strlen(argv[1]));

    timeout = atoi(argv[2]);
    if (timeout < 0) {
        RESP_ERROR(ERROR_ARG);
        return 0;
    }
    lwiperf_start_tcp_client(SERVER_ADDR , timeout);

    return 0;

}

void add_cmd()
{
    int i, j;
    cmd_tbl_t cmd_list[] = {
        /*Basic CMD*/
        {
            "AT+H",             1,   do_h,
            "AT+H               - check AT help"
        },
        /*WIFI CMD*/
        {
            "AT+WSSCAN",        1,   do_wsscan,
            "AT+WSSCAN          - start wifi scan"
        },
        {
            "AT+WSCONN",        3,   do_wsconn,
            "AT+WSCONN          - start wifi connect"
        },
        {
            "AT+WSDISCONN",     1,   do_disconn,
            "AT+WSDISCONN       - disconnect"
        },
        /*NET CMD*/
        {
            "AT+NSSTART",        1,   do_nstart,
            "AT+NSSTART          - start tcp server iperf speed test"
        },
        
        {
            "AT+NCSTART",        3,   do_cstart,
            "AT+NCSTART          - start tcp client iperf speed test"
        },
    };
    j = sizeof(cmd_list)/sizeof(cmd_tbl_t);
    for(i=0; i<j; i++){
        if(0 != console_cmd_add(&cmd_list[i])) {
            RDA_AT_PRINT("Add cmd failed\r\n");
        }
    }
}

void start_at(void)
{
    console_init();
    add_cmd();
}

