/*
 * Copyright (c) 2016, Spreadtrum Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "gpio_api.h"

/**  zephyr reference */
#if 0
static inline int gpio_uwp_config(u32_t base, int access_op, u32_t pin, int flags)
{
	u32_t int_type;

	if (access_op != GPIO_ACCESS_BY_PIN) {
		return -ENOTSUP;
	}

	if (flags & GPIO_DIR_OUT)
		uwp_gpio_set_dir(base, BIT(pin), GPIO_DIR_OUT);
	else
		uwp_gpio_set_dir(base, BIT(pin), GPIO_DIR_IN);

	if (flags & GPIO_INT) {
		if (flags & GPIO_INT_EDGE) {
			int_type = GPIO_TRIGGER_BOTH_EDGE;
		} else { /* GPIO_INT_LEVEL */
			if (flags & GPIO_INT_ACTIVE_HIGH) {
				int_type = GPIO_TRIGGER_LEVEL_HIGH;
			} else {
				int_type = GPIO_TRIGGER_LEVEL_LOW;
			}
		}

		uwp_gpio_int_set_type(base, BIT(pin), int_type);
		uwp_gpio_int_clear(base, BIT(pin));
		uwp_gpio_int_enable(base, BIT(pin));
	}

	uwp_gpio_enable(base, BIT(pin));

	return 0;
}

static inline int gpio_uwp_write(u32_t base, int access_op, u32_t pin, u32_t value)
{
	if (access_op != GPIO_ACCESS_BY_PIN) {
		return -ENOTSUP;
	}

	uwp_gpio_write(base, BIT(pin), value);

	return 0;
}

static inline int gpio_uwp_read(u32_t base, int access_op, u32_t pin, u32_t *value)
{
	u32_t status;

	if (access_op != GPIO_ACCESS_BY_PIN) {
		return -ENOTSUP;
	}

	status = uwp_gpio_read(base, BIT(pin));
	*value = status >> pin;

	return 0;
}
#endif

unsigned char aon_port0_inited = 0; 
// TODO: porting interrupt
#if 0
static int gpio_uwp_manage_callback(struct device *dev,
				    struct gpio_callback *callback, bool set)
{
	struct gpio_uwp_data *data = DEV_DATA(dev);

	_gpio_manage_callback(&data->callbacks, callback, set);

	return 0;
}

static int gpio_uwp_enable_callback(struct device *dev,
				    int access_op, u32_t pin)
{
	struct gpio_uwp_data *data = DEV_DATA(dev);
	const struct gpio_uwp_config *gpio_config = DEV_CFG(dev);
	u32_t base = gpio_config->port_base;

	if (access_op == GPIO_ACCESS_BY_PIN) {
		data->pin_callback_enables |= (1 << pin);
		uwp_gpio_input_enable(base, BIT(pin));
	} else {
		data->pin_callback_enables = 0xFFFFFFFF;
		uwp_gpio_input_enable(base, 0xFFFF);
	}

	return 0;
}


static int gpio_uwp_disable_callback(struct device *dev,
				     int access_op, u32_t pin)
{
	struct gpio_uwp_data *data = DEV_DATA(dev);
	const struct gpio_uwp_config *gpio_config = DEV_CFG(dev);
	u32_t base = gpio_config->port_base;

	if (access_op == GPIO_ACCESS_BY_PIN) {
		data->pin_callback_enables &= ~(1 << pin);
		uwp_gpio_input_disable(base, BIT(pin));
	} else {
		data->pin_callback_enables = 0;
		uwp_gpio_input_disable(base, 0xFFFF);
	}

	return 0;
}

#ifdef CONFIG_AON_INTC_UWP
static void gpio_uwp_isr(int ch, void *arg)
{
	struct device *dev = arg;
	const struct gpio_uwp_config *gpio_config = DEV_CFG(dev);
	struct gpio_uwp_data *data = DEV_DATA(dev);
	u32_t base = gpio_config->port_base;
	u32_t enabled_int, int_status;

	int_status  = uwp_gpio_int_status(base, 1);

	enabled_int = int_status & data->pin_callback_enables;

	uwp_gpio_int_disable(base, int_status);
	uwp_gpio_int_clear(base, int_status);

	_gpio_fire_callbacks(&data->callbacks, (struct device *)dev,
			     enabled_int);

	uwp_gpio_int_enable(base, int_status);
}
#endif
#endif

int gpio_uwp_p0_init(u32_t base, PinName pin)
{
	uwp_aon_enable(BIT(AON_EB_GPIO0));
	uwp_aon_reset(BIT(AON_RST_GPIO0));
	/* enable all gpio read/write by default */
	uwp_gpio_enable(base, 0xFFFF);

	uwp_gpio_int_disable(base, 0xFFFF);
	uwp_gpio_disable(base, 0xFFFF);

	uwp_aon_irq_enable(AON_INT_GPIO0);

    aon_port0_inited = 1;
	return 0;
}

static inline int gpio_uwp_p1_init(void)
{
	u32_t base = BASE_AON_GPIOP1;
 	uwp_aon_enable(BIT(AON_EB_GPIO1));
	uwp_aon_reset(BIT(AON_RST_GPIO1));
	/* enable all gpio read/write by default */
	uwp_gpio_enable(base, 0xFFFF);
 	uwp_gpio_int_disable(base, 0xFFFF);
	uwp_gpio_disable(base, 0xFFFF);
 #ifdef CONFIG_AON_INTC_UWP
	uwp_aon_intc_set_irq_callback(AON_INT_GPIO1, gpio_uwp_isr, dev);
	uwp_aon_irq_enable(AON_INT_GPIO1);
#endif
 	return 0;
}

static inline int gpio_uwp_p2_init(void)
{
	u32_t base = BASE_AON_GPIOP2;
 	uwp_aon_enable(BIT(AON_EB_GPIO2));
	uwp_aon_reset(BIT(AON_RST_GPIO2));
	/* enable all gpio read/write by default */
	uwp_gpio_enable(base, 0xFFFF);
 	uwp_gpio_int_disable(base, 0xFFFF);
	uwp_gpio_disable(base, 0xFFFF);
 #ifdef CONFIG_AON_INTC_UWP
	uwp_aon_intc_set_irq_callback(AON_INT_GPIO2, gpio_uwp_isr, dev);
	uwp_aon_irq_enable(AON_INT_GPIO2);
#endif
 	return 0;
}

void gpio_dir(gpio_t *obj, PinDirection direction){
    uwp_gpio_set_dir(obj->port_base, BIT(obj->pin), direction);
	uwp_gpio_enable(obj->port_base, BIT(obj->pin));
}

void gpio_write(gpio_t *obj, int value){
	uwp_gpio_write(obj->port_base, BIT(obj->pin), value);
}

int gpio_read(gpio_t *obj){
	return ((uwp_gpio_read(obj->port_base, BIT(obj->pin))) >> (obj->pin));
}

void gpio_init(gpio_t *obj, PinName pin){
    if(!aon_port0_inited){
        gpio_uwp_p0_init(obj->port_base, pin);
    }
}

void gpio_mode(gpio_t *obj, PinMode mode)
{
    pin_mode(obj->pin, mode);
}


/*
void gpio_init_out_ex(gpio_t *obj, PinName pin, int value){
    gpio_uwp_p0_init();
	gpio_dir(obj,PIN_OUTPUT);
	uwp_gpio_enable(obj->port_base, BIT(obj->pin));
	gpio_write(obj, value);
}

void gpio_init_out(gpio_t *obj, PinName pin){
    gpio_init_out_ex(obj, pin, 0);
}*/

#ifdef __cplusplus
extern "C" {
#endif


