/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009-2011 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>

//#include <mach/htc_sleep_clk.h>
//#include <mach/htc_fast_clk.h>

//#include "board-protou.h"

#define PROTOU_GPIO_BT_UART1_RTS        (43)
#define PROTOU_GPIO_BT_UART1_CTS        (44)
#define PROTOU_GPIO_BT_UART1_RX         (45)
#define PROTOU_GPIO_BT_UART1_TX         (46)
#define PROTOU_GPIO_BT_RESET_N          (57)
#define PROTOU_GPIO_BT_HOST_WAKE_XC     (28)
#define PROTOU_GPIO_BT_HOST_WAKE_XA     (83)
#define PROTOU_GPIO_BT_WAKE             (107)
#define PROTOU_GPIO_BT_SD_N             (112)


static struct rfkill *bt_rfk;
static const char bt_name[] = "bcm4330";

/* bt on configuration */
static uint32_t protou_bt_on_table[] = {

	/* BT_RTS */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_RTS,
				2,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_8MA),
	/* BT_CTS */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_CTS,
				2,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_8MA),
	/* BT_RX */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_RX,
				2,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_8MA),
	/* BT_TX */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_TX,
				2,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_8MA),

#if 0
	/* BT_HOST_WAKE */
	GPIO_CFG(PROTOU_GPIO_BT_HOST_WAKE,
				0,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_4MA),
#endif
	/* BT_CHIP_WAKE */
	GPIO_CFG(PROTOU_GPIO_BT_WAKE,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_4MA),
	/* BT_RESET_N */
	GPIO_CFG(PROTOU_GPIO_BT_RESET_N,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_4MA),
	/* BT_EN */
	GPIO_CFG(PROTOU_GPIO_BT_SD_N,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_4MA),
};

/* bt off configuration */
static uint32_t protou_bt_off_table[] = {

	/* BT_RTS */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_RTS,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_8MA),
	/* BT_CTS */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_CTS,
				0,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_8MA),
	/* BT_RX */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_RX,
				0,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_8MA),
	/* BT_TX */
	GPIO_CFG(PROTOU_GPIO_BT_UART1_TX,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_8MA),

	/* BT_RESET_N */
	GPIO_CFG(PROTOU_GPIO_BT_RESET_N,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_4MA),
	/* BT_EN */
	GPIO_CFG(PROTOU_GPIO_BT_SD_N,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_4MA),
#if 0
	/* BT_HOST_WAKE */
	GPIO_CFG(PROTOU_GPIO_BT_HOST_WAKE,
				0,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_4MA),
#endif
	/* BT_CHIP_WAKE */
	GPIO_CFG(PROTOU_GPIO_BT_WAKE,
				0,
				GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL,
				GPIO_CFG_4MA),
};


static uint32_t protou_bt_host_wake_table_xa[] = {
	GPIO_CFG(PROTOU_GPIO_BT_HOST_WAKE_XA,
				0,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_4MA),
};

static uint32_t protou_bt_host_wake_table_xc[] = {
	GPIO_CFG(PROTOU_GPIO_BT_HOST_WAKE_XC,
				0,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_UP,
				GPIO_CFG_4MA),
};

static void config_bt_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("[BT]%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static void protou_config_bt_on(void)
{
	printk(KERN_INFO "[BT]== R ON ==\n");

	/* set bt on configuration*/
	config_bt_table(protou_bt_on_table,
				ARRAY_SIZE(protou_bt_on_table));

	if (system_rev == 0) {  //XA
		printk(KERN_INFO "rfkill check system_rev: %d\n", system_rev);
		config_bt_table(protou_bt_host_wake_table_xa,
					ARRAY_SIZE(protou_bt_host_wake_table_xa));
	} else {
		config_bt_table(protou_bt_host_wake_table_xc,
					ARRAY_SIZE(protou_bt_host_wake_table_xc));
	}

	mdelay(2);

	/* BT_RESET_N */
	gpio_set_value(PROTOU_GPIO_BT_RESET_N, 0);
	mdelay(1);

	/* BT_EN */
	gpio_set_value(PROTOU_GPIO_BT_SD_N, 0);
	mdelay(5);

	/* BT_EN */
	gpio_set_value(PROTOU_GPIO_BT_SD_N, 1);
	mdelay(1);

	/* BT_RESET_N */
	gpio_set_value(PROTOU_GPIO_BT_RESET_N, 1);
	mdelay(2);

	//temp set low before sleep mode ready
	gpio_set_value(PROTOU_GPIO_BT_WAKE, 0);
}

static void protou_config_bt_off(void)
{
	/* BT_RESET_N */
	gpio_set_value(PROTOU_GPIO_BT_RESET_N, 0);

	/* BT_SHUTDOWN_N */
	gpio_set_value(PROTOU_GPIO_BT_SD_N, 0);

	/* set bt off configuration*/
	config_bt_table(protou_bt_off_table,
				ARRAY_SIZE(protou_bt_off_table));

	if (system_rev == 0) {  //XA
		config_bt_table(protou_bt_host_wake_table_xa,
					ARRAY_SIZE(protou_bt_host_wake_table_xa));
	} else {
		config_bt_table(protou_bt_host_wake_table_xc,
					ARRAY_SIZE(protou_bt_host_wake_table_xc));
	}

	/* BT_RTS */
	gpio_set_value(PROTOU_GPIO_BT_UART1_RTS, 1);

	/* BT_CTS */

	/* BT_TX */
	gpio_set_value(PROTOU_GPIO_BT_UART1_TX, 1);

	/* BT_RX */


	/* BT_HOST_WAKE */

	/* BT_CHIP_WAKE */
	gpio_set_value(PROTOU_GPIO_BT_WAKE, 0);

	printk(KERN_INFO "[BT]== R OFF ==\n");}

static int bluetooth_set_power(void *data, bool blocked)
{
	if (!blocked)
		protou_config_bt_on();
	else
		protou_config_bt_off();

	return 0;
}

static struct rfkill_ops protou_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int protou_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;  /* off */

	/* always turn on clock? */
	/* htc_wifi_bt_sleep_clk_ctl(CLK_ON, ID_BT);
	mdelay(2); */

	bluetooth_set_power(NULL, default_state);

	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
				&protou_rfkill_ops, NULL);
	if (!bt_rfk) {
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_set_states(bt_rfk, default_state, false);

	/* userspace cannot take exclusive control */

	rc = rfkill_register(bt_rfk);
	if (rc)
		goto err_rfkill_reg;

	return 0;

err_rfkill_reg:
	rfkill_destroy(bt_rfk);
err_rfkill_alloc:
	return rc;
}

static int protou_rfkill_remove(struct platform_device *dev)
{
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);
	return 0;
}

static struct platform_driver protou_rfkill_driver = {
	.probe = protou_rfkill_probe,
	.remove = protou_rfkill_remove,
	.driver = {
		.name = "protou_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init protou_rfkill_init(void)
{
//	if (!machine_is_protou() || machine_is_protodug() || machine_is_magnids() || machine_is_protodcg())
//		return 0;

	return platform_driver_register(&protou_rfkill_driver);
}

static void __exit protou_rfkill_exit(void)
{
	platform_driver_unregister(&protou_rfkill_driver);
}

module_init(protou_rfkill_init);
module_exit(protou_rfkill_exit);
MODULE_DESCRIPTION("protou rfkill");
MODULE_AUTHOR("htc_ssdbt <htc_ssdbt@htc.com>");
MODULE_LICENSE("GPL");
