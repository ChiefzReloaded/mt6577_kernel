/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include <linux/platform_device.h>
#include <linux/rtpm_prio.h>
#include <cust_eint.h>

#include "cust_gpio_usage.h"
#include "ist30xx.h"

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>

/******************************************************************************
 * Return value of Error
 * EPERM  : 1
 * ENOENT : 2
 * EIO    : 5
 * EINVAL : 22
 *****************************************************************************/
extern struct ist30xx_data *ts_data;
extern bool get_zvalue_mode;
#if IST30XX_NOISE_MODE
extern bool get_event_mode;
#endif
const u32 pos_cmd = cpu_to_be32(CMD_GET_COORD);
struct i2c_msg pos_msg[READ_CMD_MSG_LEN] = {
	{
		.flags = 0,
		.len = IST30XX_ADDR_LEN,
		.buf = (u8 *)&pos_cmd,
		.timing = 100   // 100KHz
	},
	{
		.flags = I2C_M_RD,
		.timing = 100   // 100KHz
	},
};


int ist30xx_get_position(struct i2c_client *client, u32 *buf, u16 len)
{
	int ret, i;

	pos_msg[0].addr = client->addr;
	pos_msg[1].addr = client->addr;
	pos_msg[1].len = len * IST30XX_DATA_LEN,
	pos_msg[1].buf = (u8 *)buf,

	ret = i2c_transfer(client->adapter, pos_msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		pr_err("[ TSP ] %s: i2c failed (%d)\n", __func__, ret);
		return -EIO;
	}

	for (i = 0; i < len; i++)
		buf[i] = cpu_to_be32(buf[i]);

	return 0;
}

int ist30xx_cmd_run_device(struct i2c_client *client)
{
	int ret = -EIO;

	ist30xx_reset();
	ret = ist30xx_write_cmd(client, CMD_RUN_DEVICE, 0);
	if (ret) return ret;
	msleep(10);

#if IST30XX_NOISE_MODE
	get_event_mode = true;
#endif
	msleep(10);

	return ret;
}

int ist30xx_cmd_start_scan(struct i2c_client *client)
{
	int ret;

#if IST30XX_NOISE_MODE
	get_event_mode = true;
#endif

	ret = ist30xx_write_cmd(client, CMD_START_SCAN, 0);

	msleep(100);

	return ret;
}

int ist30xx_cmd_calibrate(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, CMD_CALIBRATE, 0);

	TPD_FUN();

	msleep(100);

	return ret;
}

int ist30xx_cmd_check_calib(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, CMD_CHECK_CALIB, 0);

	DMSG("[ TSP ] *** Check Calibration cmd ***\n");

	msleep(20);

	return ret;
}

int ist30xx_cmd_update(struct i2c_client *client, int cmd)
{
	u32 val = (cmd == CMD_ENTER_FW_UPDATE ? CMD_FW_UPDATE_MAGIC : 0);
	int ret = ist30xx_write_cmd(client, cmd, val);

	msleep(10);

	return ret;
}

int ist30xx_cmd_reg(struct i2c_client *client, int cmd)
{
	int ret = ist30xx_write_cmd(client, cmd, 0);

	if (cmd == CMD_ENTER_REG_ACCESS) {
#if IST30XX_NOISE_MODE
		get_event_mode = false;
#endif
		msleep(50);
	}
#if IST30XX_NOISE_MODE
	else if (cmd == CMD_EXIT_REG_ACCESS) {
		get_event_mode = true;
	}
#endif

	return ret;
}


int ist30xx_read_cmd(struct i2c_client *client, u32 cmd, u32 *buf)
{
	int ret;
	u32 le_reg = cpu_to_be32(cmd);

	struct i2c_msg msg[READ_CMD_MSG_LEN] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = IST30XX_ADDR_LEN,
			.buf = (u8 *)&le_reg,
			.timing = 100
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = IST30XX_DATA_LEN,
			.buf = (u8 *)buf,
			.timing = 100
		},
	};

	ret = i2c_transfer(client->adapter, msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		pr_err("[ TSP ] %s: i2c failed (%d)\n", __func__, ret);
		return -EIO;
	}

	*buf = cpu_to_be32(*buf);

	return 0;
}


int ist30xx_write_cmd(struct i2c_client *client, u32 cmd, u32 val)
{
	int ret;
	struct i2c_msg msg;
	u8 msg_buf[IST30XX_ADDR_LEN + IST30XX_DATA_LEN];

	put_unaligned_be32(cmd, msg_buf);
	put_unaligned_be32(val, msg_buf + IST30XX_ADDR_LEN);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = IST30XX_ADDR_LEN + IST30XX_DATA_LEN;
	msg.buf = msg_buf;
	msg.timing = 100;

	ret = i2c_transfer(client->adapter, &msg, WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN) {
		pr_err("[ TSP ] %s: i2c failed (%d)\n", __func__, ret);
		return -EIO;
	}

	return 0;
}


int ist30xx_power_on(void)
{
	if (ts_data->status.power != 1) {
		// TODO : Implements this function to meet your system.
		hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_3000, "TP");
		msleep(50);
		ts_data->status.power = 1;

		TPD_FUN();
	}

	return 0;
}


int ist30xx_power_off(void)
{
	if (ts_data->status.power != 0) {
		// TODO : Implements this function to meet your system.
		hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
		msleep(100);
		ts_data->status.power = 0;
		get_zvalue_mode = false;

		TPD_FUN();
	}

	return 0;
}


int ist30xx_reset(void)
{
	// TODO : Implements this function to meet your system.
	ist30xx_power_off();
	ist30xx_power_on();

	ts_data->status.power = 1;
	TPD_FUN();
	return 0;
}


int ist30xx_internal_suspend(struct ist30xx_data *data)
{
	ist30xx_power_off();
	return 0;
}


int ist30xx_internal_resume(struct ist30xx_data *data)
{
	ist30xx_power_on();
	ist30xx_write_cmd(data->client, CMD_RUN_DEVICE, 0);
	msleep(10);
	return 0;
}


int ist30xx_init_system(void)
{
	int ret;

	ret = ist30xx_power_on();
	if (ret) {
		pr_err("[ TSP ] %s: ist30xx_power_on failed (%d)\n", __func__, ret);
		return -EIO;
	}

	ret = ist30xx_reset();
	if (ret) {
		pr_err("[ TSP ] %s: ist30xx_reset failed (%d)\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

/*                                    */
/*                                                                */
#if 0 // GOLF SEOSANGCHEOL 130123 , from DV1 should enable this function
void touch_keylock_enable(int key_lock)
{
	if (!key_lock) {
		__set_bit(ABS_X, tpd->dev->absbit);
		__set_bit(ABS_Y, tpd->dev->absbit);
		__set_bit(ABS_PRESSURE, tpd->dev->absbit);
		__set_bit(ABS_MT_POSITION_X, tpd->dev->absbit);
		__set_bit(ABS_MT_POSITION_Y, tpd->dev->absbit);
		__set_bit(ABS_MT_TOUCH_MAJOR, tpd->dev->absbit);
		__set_bit(ABS_MT_TRACKING_ID, tpd->dev->absbit);
		__set_bit(ABS_MT_PRESSURE, tpd->dev->absbit);
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	} else {
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		__clear_bit(ABS_X, tpd->dev->absbit);
		__clear_bit(ABS_Y, tpd->dev->absbit);
		__clear_bit(ABS_PRESSURE, tpd->dev->absbit);
		__clear_bit(ABS_MT_POSITION_X, tpd->dev->absbit);
		__clear_bit(ABS_MT_POSITION_Y, tpd->dev->absbit);
		__clear_bit(ABS_MT_TOUCH_MAJOR, tpd->dev->absbit);
		__clear_bit(ABS_MT_TRACKING_ID, tpd->dev->absbit);
		__clear_bit(ABS_MT_PRESSURE, tpd->dev->absbit);
	}
	return;
}
EXPORT_SYMBOL(touch_keylock_enable);
#endif
/*                                    */
