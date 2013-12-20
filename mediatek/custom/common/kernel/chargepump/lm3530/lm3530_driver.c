/*  Date: 2011/8/8 11:00:00
 *  Revision: 1.6
 */

/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file lm3530.c
   brief This file contains all function implementations for the lm3530 in linux

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

#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

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

#define lm3530_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)


#define lm3530_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))


#define CPD_TAG                  "[ChargePump] "
#define CPD_FUN(f)               printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    printk(CPD_TAG fmt, ##args)

// I2C variable
static struct i2c_client *new_client = NULL;


#if 0   // old-style
static unsigned short lm3530_force[] = {0x00, 0x30, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short *const lm3530_forces[] = { lm3530_force, NULL };
static struct i2c_client_address_data lm3530_i2c_addr_data = { .forces = lm3530_forces };
#else   // new style
static const struct i2c_device_id lm3530_i2c_id[] = {{"charge-pump",0},{}};
//static struct i2c_board_info __initdata i2c_lm3530={ I2C_BOARD_INFO("charge-pump", 0x30)};
//static struct i2c_board_info __initdata i2c_lm3530={ I2C_BOARD_INFO("charge-pump", (0xEC>>1))};
static struct i2c_board_info __initdata i2c_lm3530={ I2C_BOARD_INFO("charge-pump", 0x38)};
#endif


/* generic */
#define lm3530_MAX_RETRY_I2C_XFER (100)
#define lm3530_I2C_WRITE_DELAY_TIME 1

/*	i2c read routine for API*/
static char lm3530_i2c_read(struct i2c_client *client, u8 reg_addr,
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
            printk("send dummy is %d", dummy);
			return -1;
		}

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0)
		{
            printk("recv dummy is %d", dummy);
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

	for (retry = 0; retry < lm3530_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(lm3530_I2C_WRITE_DELAY_TIME);
	}

	if (lm3530_MAX_RETRY_I2C_XFER <= retry) {
		CPD_ERR("I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

/*	i2c write routine for */
static char lm3530_i2c_write(struct i2c_client *client, u8 reg_addr,
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
#if 1//def BMM_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			printk("ksw error writing i2c bus");
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
		for (retry = 0; retry < lm3530_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				mdelay(lm3530_I2C_WRITE_DELAY_TIME);
			}
		}
		if (lm3530_MAX_RETRY_I2C_XFER <= retry) {
			printk("ksw I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
#endif
}



static int lm3530_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	return lm3530_i2c_read(client,reg_addr,data,1);
}

static int lm3530_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{

	return lm3530_i2c_write(client,reg_addr,data,1);;
}

static int lm3530_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	return lm3530_i2c_read(client,reg_addr,data,len);
}

int chargepump_set_backlight_level(unsigned int level)
{
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
 
 lm3530_smbus_write_byte(new_client, 0xA0, &data); 
}

static int lm3530_reset()
{
 int err = 0;
 int i = 0;
 unsigned char data1;

 mt_set_gpio_mode(GPIO_BL_EN, GPIO_BL_EN_M_GPIO); /* GPIO mode */
 mt_set_gpio_dir(GPIO_BL_EN, GPIO_DIR_OUT);
 
 mt_set_gpio_out(GPIO_BL_EN,GPIO_OUT_ONE);
 udelay(100);
 
 /* EN set to LOW(shutdown) -> HIGH(enable) */
 mt_set_gpio_out(GPIO_BL_EN,GPIO_OUT_ZERO);
 for (i =0; i < 1000; i++)
 	udelay(100);
 	
 mt_set_gpio_out(GPIO_BL_EN,GPIO_OUT_ONE);
 
 udelay(10);
		   
 CPD_LOG("err num[%d]", err);
 
}

static int lm3530_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{     
	int ret;
	unsigned char data = 0x01;
	new_client = client;

    CPD_FUN();
    printk("%s : kenneth.kang  \n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CPD_LOG("i2c_check_functionality error\n");
		return -1;
	}

	if (client == NULL) 
		printk("%s client is NULL\n", __func__);
	else
	{
		printk("%s %x %x %x\n", __func__, client->adapter, client->addr, client->flags);
	}
#if 0 // if power has controled on uboot, it is not neccessory.
	mt_set_gpio_mode(GPIO_BL_EN, GPIO_BL_EN_M_GPIO); /* GPIO mode */
	mt_set_gpio_dir(GPIO_BL_EN, GPIO_DIR_OUT);
	
	mt_set_gpio_out(GPIO_BL_EN,GPIO_OUT_ONE);
	data = 0x15;
	ret = lm3530_smbus_write_byte(new_client, 0x10, &data);
#endif	
	return 0;
}


static int lm3530_remove(struct i2c_client *client)
{
    new_client = NULL;
	return 0;
}


static int lm3530_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{ 
	return 0;
}

static struct i2c_driver lm3530_driver = {
	.driver = {
//		.owner	= THIS_MODULE,
		.name	= "charge-pump",
	},
	.probe		= lm3530_probe,
	.remove		= lm3530_remove,
//	.detect		= lm3530_detect,
	.id_table	= lm3530_i2c_id,
//	.address_data = &lm3530250_i2c_addr_data,
};

#if 0 // sys file system BLU i2c proto type
static ssize_t show_lm3530_i2c(struct device *dev,struct device_attribute *attr, const char *buf)
{
	unsigned char reg_num;
	unsigned char data;

	if(!strncmp(buf, "0x00", 4))
		reg_num = 0x00;
	else if (!strncmp(buf,"0x01", 4))
		reg_num = 0x01;
	else if (!strncmp(buf,"0x02", 4))
		reg_num = 0x02;
	else if (!strncmp(buf,"0x03", 4))
		reg_num = 0x03;
	else if (!strncmp(buf,"0x04", 4))
		reg_num = 0x04;
	else if (!strncmp(buf,"0x05", 4))
		reg_num = 0x05;
	else if (!strncmp(buf,"0x06", 4))
		reg_num = 0x06;
	else if (!strncmp(buf,"0x07", 4))
		reg_num = 0x07;
	else if (!strncmp(buf,"0x08", 4))
		reg_num = 0x08;
	else if (!strncmp(buf,"0x1F", 4))
		reg_num = 0x1F;

	lm3530_smbus_read_byte(new_client,reg_num, &data);

	return data;
}

static ssize_t store_lm3530_i2c(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char reg_num;
	unsigned char data[10];
	unsigned char w_data;

	while(strncmp(buf, "\n", 1));
	{
		if(strncmp(buf, " ", 1));
			i++;

		strcpy(data[i], buf);
	}
	reg_num = unsigned char(AtoI(data[0]));
	w_data = unsigned char(AtoI(data[2]));
	
	lm3530_smbus_write_byte(new_client, reg_num, &data+2);
}

static DEVICE_ATTR(lm3530_ic2, 0664, show_lm3530_i2c, store_lm3530_i2c);
#endif

static int lm3530_pd_probe(struct platform_device *pdev) 
{
//	int ret;
	unsigned char data;
	CPD_FUN();
	if(i2c_add_driver(&lm3530_driver))
	{
	 CPD_ERR("failed add i2c driver");
	 return -1; 
	}

#if 0
	ret = device_create_file(&(pdev->dev), &dev_attr_lm3530_ic2);
    if(ret)
    {
        printk("[lm3530]device_create_file vibr_on fail! \n");
    }
#endif	
	return 0;
}

static int lm3530_pd_remove(struct platform_device *pdev)
{
    CPD_FUN();
    i2c_del_driver(&lm3530_driver);
    return 0;
}

//#ifdef CONFIG_HAS_EARLYSUSPEND
static void lm3530_early_suspend(struct early_suspend *h)
{
	int err = 0;
	unsigned char data1;
	

	data1 = 0x00;
	err = lm3530_smbus_write_byte(new_client, 0xA0, &data1);
	data1 = 0x00;
	err = lm3530_smbus_write_byte(new_client, 0x10, &data1); 
	printk("[kkonan] lm3530_early_suspend err register 0xA0 : [%d]\n", err);

	//mt_set_gpio_out(GPIO_BL_EN,GPIO_OUT_ZERO);
		 
}

static void lm3530_late_resume(struct early_suspend *h)
{
	int err = 0;
	unsigned char data1;

	mt_set_gpio_mode(GPIO_BL_EN, GPIO_BL_EN_M_GPIO); /* GPIO mode */
	mt_set_gpio_dir(GPIO_BL_EN, GPIO_DIR_OUT);
	
	mt_set_gpio_out(GPIO_BL_EN,GPIO_OUT_ONE);

	data1 = 0x17;
	err = lm3530_smbus_write_byte(new_client, 0x10, &data1);
	printk("[kkonan] lm3530_early_resume err register 0x10 : [%d]\n", err);

}

static struct early_suspend lm3530_early_suspend_desc = {
	.level		= EARLY_SUSPEND_LEVEL_BLANK_SCREEN +1,
	.suspend	= lm3530_early_suspend,
	.resume		= lm3530_late_resume,
};
//#endif


static struct platform_driver lm3530_backlight_driver = {
	.probe      = lm3530_pd_probe,
	.remove     = lm3530_pd_remove,
	.driver     = {
		.name  = "charge-pump",
		.owner = THIS_MODULE,
	}
};      

static int __init lm3530_init(void)
{
    CPD_FUN();
   
	i2c_register_board_info(1, &i2c_lm3530, 1); 

	register_early_suspend(&lm3530_early_suspend_desc);
	
	if(platform_driver_register(&lm3530_backlight_driver))
	{
	 CPD_ERR("failed to register driver");
	 return -1;
	}

	return 0;
}

static void __exit lm3530_exit(void)
{
	platform_driver_unregister(&lm3530_backlight_driver);
}

MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("charge-pump driver");
MODULE_LICENSE("GPL");

module_init(lm3530_init);
module_exit(lm3530_exit);

