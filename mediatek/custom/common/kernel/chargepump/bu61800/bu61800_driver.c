/*  Date: 2011/8/8 11:00:00
 *  Revision: 1.6
 */

/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file bu61800.c
   brief This file contains all function implementations for the bu61800 in linux

*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/earlysuspend.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#ifdef MT6516
#include <mach/mt6516_devs.h>
#include <mach/mt6516_typedefs.h>
#include <mach/mt6516_gpio.h>
#include <mach/mt6516_pll.h>
#endif

#ifdef MT6573
#include <mach/mt6573_devs.h>
#include <mach/mt6573_typedefs.h>
#include <mach/mt6573_gpio.h>
#include <mach/mt6573_pll.h>
#endif

#ifdef MT6577
#include <mach/mt6577_devs.h>
#include <mach/mt6577_typedefs.h>
#include <mach/mt6577_gpio.h>
#include <mach/mt6577_pm_ldo.h>
#endif

#ifdef MT6516
#define POWER_NONE_MACRO MT6516_POWER_NONE
#endif

#ifdef MT6573
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#ifdef MT6575
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#include <linux/platform_device.h>
#include <cust_acc.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>

#define BU61800_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)


#define BU61800_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define CPD_TAG                  "[ChargePump] "
#define CPD_FUN(f)               printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    printk(CPD_TAG fmt, ##args)

// I2C variable
static struct i2c_client *new_client = NULL;


#if 0   // old-style
static unsigned short bu61800_force[] = {0x00, 0x30, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short *const bu61800_forces[] = { bu61800_force, NULL };
static struct i2c_client_address_data bu61800_i2c_addr_data = { .forces = bu61800_forces };
#else   // new style
static const struct i2c_device_id bu61800_i2c_id[] = {{"charge-pump",0},{}};
static struct i2c_board_info __initdata i2c_BU61800={ I2C_BOARD_INFO("charge-pump", (0xEC>>1))};
#endif


/* generic */
#define BU61800_MAX_RETRY_I2C_XFER (100)
#define BU61800_I2C_WRITE_DELAY_TIME 1


/*	i2c read routine for API*/
static char bu61800_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;
	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMA_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			CPD_ERR("i2c bus read error");
			return -1;
		}
		*data = (u8)(dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0)
		{
			return -1;
		}

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0)
		{
			return -1;
		}
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < BU61800_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(BU61800_I2C_WRITE_DELAY_TIME);
	}

	if (BU61800_MAX_RETRY_I2C_XFER <= retry) {
		CPD_ERR("I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

/*	i2c write routine for */
static char bu61800_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMA_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef BMA_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMM_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			CPD_ERR("error writing i2c bus");
			return -1;
		}

	}
	return 0;
#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BU61800_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				mdelay(BU61800_I2C_WRITE_DELAY_TIME);
			}
		}
		if (BU61800_MAX_RETRY_I2C_XFER <= retry) {
			PERR("I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
#endif
}



static int BU61800_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	return bu61800_i2c_read(client,reg_addr,data,1);
}

static int BU61800_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{

	return bu61800_i2c_write(client,reg_addr,data,1);;
}

static int BU61800_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	return bu61800_i2c_read(client,reg_addr,data,len);
}

int chargepump_set_backlight_level(unsigned int level)
{
#if 0
 unsigned char data = 0x7F;

 if(level>0x7F)
  data = (unsigned char)0x7F;
 else
  data = (unsigned char)level;
#else
unsigned char data;

unsigned char max_brightness = 0x7F;
unsigned char min_brightness = 0x40;
unsigned char brightness_control;
  

unsigned int brightness = level /2;

brightness_control = min_brightness + brightness/2;

if(brightness > max_brightness)
 data = (unsigned char)max_brightness;
else if(brightness < min_brightness)
 data = (unsigned char)min_brightness;
else
 data = (unsigned char)brightness_control;
  
#endif
 BU61800_smbus_write_byte(new_client,
			   0x03, &data); //Main group Current setting 25.6mA

 CPD_LOG("level[%d]\n", data);
 
}

static int bu61800_reset()
{
 int err = 0;
 unsigned char data1;

 data1 = 0x01;
 err = BU61800_smbus_write_byte(new_client,
 				0x00, &data1);  // software reset 
 
 data1 = 0x06;  
 err = BU61800_smbus_write_byte(new_client,
 				0x01, &data1); // LED4,5 main group control 
 
 data1 = 0x81;
 err = BU61800_smbus_write_byte(new_client,
 				0x02, &data1); // external PWM input valid main group control on 
  
 CPD_LOG("err[%d]\n", err);	  
}

static int bu61800_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;

	unsigned char data1 = 0x01;
	new_client = client;

	CPD_FUN();

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		goto exit;
	}

    bu61800_reset();
    
	return 0;

exit:
	return err;
}


static int bu61800_remove(struct i2c_client *client)
{
#if 0
	struct bu61800_data *data = i2c_get_clientdata(client);

	bu61800_set_enable(&client->dev, 0);
#endif
    new_client = NULL;
	return 0;
}


static int bu61800_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{ 
#if 1
	strcpy(info->type, "charge-pump");
#endif
	return 0;
}

static struct i2c_driver bu61800_driver = {
	.driver = {
//		.owner	= THIS_MODULE,
		.name	= "charge-pump",
	},
	.probe		= bu61800_probe,
	.remove		= bu61800_remove,
	.detect		= bu61800_detect,
	.id_table	= bu61800_i2c_id,
//	.address_data = &bu61800250_i2c_addr_data,
};

static int bu61800_pd_probe(struct platform_device *pdev) 
{
	int err = 0;

	CPD_FUN();
	
	err = i2c_add_driver(&bu61800_driver);
	return err;
}


static int bu61800_pd_remove(struct platform_device *pdev)
{
    i2c_del_driver(&bu61800_driver);

	CPD_FUN();

    return 0;
}


static struct platform_driver BU61800_backlight_driver = {
	.probe      = bu61800_pd_probe,
	.remove     = bu61800_pd_remove,    
	.driver     = {
		.name  = "charge-pump",
		.owner = THIS_MODULE,
	}
};      

//#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu61800_early_suspend(struct early_suspend *h)
{
	int err = 0;
	unsigned char data1;
	
	
	//data1 = 0x81;
	data1 = 0x00;
	err = BU61800_smbus_write_byte(new_client,
				   0x02, &data1); 
}

static void bu61800_early_resume(struct early_suspend *h)
{
	int err = 0;
	unsigned char data1;
	
	data1 = 0x81;
	err = BU61800_smbus_write_byte(new_client,
				   0x02, &data1); 
}

static struct early_suspend bu61800_early_suspend_desc = {
	.level		= EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend	= bu61800_early_suspend,
	.resume		= bu61800_early_resume,
};
//#endif


static int __init BU61800_init(void)
{
	int err = 0;
    CPD_FUN();
	i2c_register_board_info(1, &i2c_BU61800, 1);

	register_early_suspend(&bu61800_early_suspend_desc);
	
	err = platform_driver_register(&BU61800_backlight_driver);
	return err;
}

static void __exit BU61800_exit(void)
{
	platform_driver_unregister(&BU61800_backlight_driver);
}

MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("charge-pump driver");
MODULE_LICENSE("GPL");

module_init(BU61800_init);
module_exit(BU61800_exit);

