/* drivers/input/touchscreen/justin_synaptics_ts.c
 *
 * Copyright (C) 2011 LG Electironics, Inc.
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
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#if 0 //*v5_add start */
#include <mach/gpio.h>
#else
#include <linux/gpio.h>
//#ifdef MT6575
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
//#endif
#include <mach/mt_wdt.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>
#include <linux/rtpm_prio.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#include <linux/spinlock.h>
#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif
#include "tpd.h"
#include <cust_eint.h>
#endif //*v5_add end*/


#include <linux/workqueue.h>
#if 0 //*v5_add start */
#ifdef CONFIG_MACH_LGE_JUSTIN
#include "PR715491-tm1743-001.h"
#define SYNAPTICS_SUPPORT_FW_UPGRADE
#else
//                                                                
//#include "synaptics_ts_firmware.h"
#include "PR704352-tm1702_burst.h"
//#include "PR715491-tm1743-001.h"
//                                                              
#endif
#endif //*v5_add end*/
 


#define SYNAPTICS_TOUCH_DEBUG 1

#if SYNAPTICS_TOUCH_DEBUG
#define ts_printk(args...)  printk(args)
#else
#define ts_printk(args...)
#endif

//*v5_add start */
static struct i2c_board_info __initdata i2c_Synaptics={ I2C_BOARD_INFO("mtk-tpd", 0x20)};
/*v5_add end*/

#define FEATURE_LGE_TOUCH_GHOST_FINGER_IMPROVE
/*===========================================================================
                DEFINITIONS AND DECLARATIONS FOR MODULE

This section contains definitions for constants, macros, types, variables
and other items needed by this module.
===========================================================================*/

static struct workqueue_struct *synaptics_wq;
static struct i2c_client *hub_ts_client = NULL;

extern struct tpd_device *tpd;

static u8 ButtonDataBaseReg = 0;

struct synaptics_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	bool has_relative_report;
	struct hrtimer timer;
	struct work_struct  work;
	uint16_t max[2];

	uint32_t flags;
	int reported_finger_count;
	int8_t sensitivity_adjust;
	int (*power)(int on);

	unsigned int count;
	int x_lastpt;
	int y_lastpt;

	struct early_suspend early_suspend;

	struct delayed_work init_delayed_work;
};

//*v5_add start */
struct synaptics_ts_data *ts_hub;
//*v5_add end*/

static int init_stabled = -1;

static void synaptics_ts_early_suspend(struct early_suspend *h);
static void synaptics_ts_late_resume(struct early_suspend *h);



#if 1 //*v5_add start */
#define TOUCH_INT_N_GPIO						75
#else
//#define TOUCH_INT_N_GPIO						35
#endif //*v5_add end*/





/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                                 Macros                                  */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define TS_SNTS_GET_FINGER_STATE_0(finger_status_reg) \
		(finger_status_reg&0x03)
#define TS_SNTS_GET_FINGER_STATE_1(finger_status_reg) \
		((finger_status_reg&0x0C)>>2)
#define TS_SNTS_GET_FINGER_STATE_2(finger_status_reg) \
		((finger_status_reg&0x30)>>4)
#define TS_SNTS_GET_FINGER_STATE_3(finger_status_reg) \
      ((finger_status_reg&0xC0)>>6)
#define TS_SNTS_GET_FINGER_STATE_4(finger_status_reg) \
      (finger_status_reg&0x03)

#define TS_SNTS_GET_X_POSITION(high_reg, low_reg) \
		((int)(high_reg*0x10) + (int)(low_reg&0x0F))
#define TS_SNTS_GET_Y_POSITION(high_reg, low_reg) \
		((int)(high_reg*0x10) + (int)((low_reg&0xF0)/0x10))

#define TS_SNTS_HAS_PINCH(gesture_reg) \
		((gesture_reg&0x40)>>6)
#define TS_SNTS_HAS_FLICK(gesture_reg) \
		((gesture_reg&0x10)>>4)
#define TS_SNTS_HAS_DOUBLE_TAP(gesture_reg) \
		((gesture_reg&0x04)>>2)

#define TS_SNTS_GET_REPORT_RATE(device_control_reg) \
		((device_control_reg&0x40)>>6)
// 1st bit : '0' - Allow sleep mode, '1' - Full power without sleeping
// 2nd and 3rd bit : 0x00 - Normal Operation, 0x01 - Sensor Sleep
#define TS_SNTS_GET_SLEEP_MODE(device_control_reg) \
		(device_control_reg&0x07)


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                       CONSTANTS DATA DEFINITIONS                        */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define TOUCH_EVENT_NULL						0
#define TOUCH_EVENT_BUTTON						1
#define TOUCH_EVENT_ABS							2



#define SYNAPTICS_FINGER_MAX					5


#define SYNAPTICS_TM1576_PRODUCT_ID				"TM1576"
#define SYNAPTICS_TM1576_RESOLUTION_X			1036
#define SYNAPTICS_TM1576_RESOLUTION_Y			1976
#define SYNAPTICS_TM1576_LCD_ACTIVE_AREA		1728
#define SYNAPTICS_TM1576_BUTTON_ACTIVE_AREA		1828

#define SYNAPTICS_TM1702_PRODUCT_ID				"TM1702"
#define SYNAPTICS_TM1702_RESOLUTION_X			1036
#define SYNAPTICS_TM1702_RESOLUTION_Y			1896
#define SYNAPTICS_TM1702_LCD_ACTIVE_AREA		1728
#define SYNAPTICS_TM1702_BUTTON_ACTIVE_AREA		1805

//                                                                    
#define SYNAPTICS_TM1743_PRODUCT_ID				"TM1743"
#define SYNAPTICS_TM1743_RESOLUTION_X			(1123-70)
#define SYNAPTICS_TM1743_RESOLUTION_Y			(1872-90)
#define SYNAPTICS_TM1743_LCD_ACTIVE_AREA		(1872-90)
#define SYNAPTICS_TM1743_BUTTON_ACTIVE_AREA		1805
//                                                                  



/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                    REGISTER ADDR & SETTING VALUE                        */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define SYNAPTICS_FLASH_CONTROL_REG				0x12
#define SYNAPTICS_DATA_BASE_REG					0x13
#define SYNAPTICS_INT_STATUS_REG				0x14

#define SYNAPTICS_CONTROL_REG					0x4F
#define SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE	0x4D	//20101227 seven added to prevent interrupt in booting time
#define SYNAPTICS_REDUCE_MODE_REG				0x51
#define SYNAPTICS_DELTA_X_THRES_REG				0x53
#define SYNAPTICS_DELTA_Y_THRES_REG				0x54

#define SYNAPTICS_FW_REVISION_REG				0xA1	//0xAD

#define SYNAPTICS_RMI_QUERY_BASE_REG			0xE3
#define SYNAPTICS_RMI_CMD_BASE_REG				0xE4
#define SYNAPTICS_FLASH_QUERY_BASE_REG			0xE9
#define SYNAPTICS_FLASH_DATA_BASE_REG			0xEC

#define SYNAPTICS_INT_FLASH						1<<0
#define SYNAPTICS_INT_STATUS					1<<1
#define SYNAPTICS_INT_ABS0						1<<2

#define SYNAPTICS_CONTROL_SLEEP					0x01
#define SYNAPTICS_CONTROL_NOSLEEP				0x04
#define SYNAPTICS_CONTROL_CONFIGURED			1<<7

#ifdef SYNAPTICS_SUPPORT_FW_UPGRADE
#define SYNAPTICS_FLASH_CMD_FW_CRC				0x01
#define SYNAPTICS_FLASH_CMD_FW_WRITE			0x02
#define SYNAPTICS_FLASH_CMD_ERASEALL			0x03
#define SYNAPTICS_FLASH_CMD_CONFIG_READ			0x05
#define SYNAPTICS_FLASH_CMD_CONFIG_WRITE		0x06
#define SYNAPTICS_FLASH_CMD_CONFIG_ERASE		0x07
#define SYNAPTICS_FLASH_CMD_ENABLE				0x0F
#define SYNAPTICS_FLASH_NORMAL_RESULT			0x80
#endif /* SYNAPTICS_SUPPORT_FW_UPGRADE */

#define SYNAPTICS_TS_SENSITYVITY_REG		0x9B
#define SYNAPTICS_TS_SENSITYVITY_VALUE		0x00

#define FEATURE_LGE_TOUCH_GRIP_SUPPRESSION
//                                           

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                         DATA DEFINITIONS                                */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct {
	unsigned char m_QueryBase;
	unsigned char m_CommandBase;
	unsigned char m_ControlBase;
	unsigned char m_DataBase;
	unsigned char m_IntSourceCount;
	unsigned char m_FunctionExists;
} T_RMI4FuncDescriptor;




typedef struct
{
	unsigned char device_status_reg;						//0x13
	unsigned char interrupt_status_reg;					//0x14
	unsigned char finger_state_reg[3];					//0x15~0x17

	unsigned char fingers_data[SYNAPTICS_FINGER_MAX][5];	//0x18 ~ 0x49
	/* 5 data per 1 finger, support 10 fingers data
	fingers_data[x][0] : xth finger's X high position
	fingers_data[x][1] : xth finger's Y high position
	fingers_data[x][2] : xth finger's XY low position
	fingers_data[x][3] : xth finger's XY width
	fingers_data[x][4] : xth finger's Z (pressure)
	*/
	// Etc...
	//unsigned char gesture_flag0;							//0x4A
	//unsigned char gesture_flag1;							//0x4B
	//unsigned char pinch_motion_X_flick_distance;			//0x4C
	//unsigned char rotation_motion_Y_flick_distance;		//0x4D
	//unsigned char finger_separation_flick_time;			//0x4E
} ts_sensor_data;

typedef struct {
	unsigned char touch_status[SYNAPTICS_FINGER_MAX];
	unsigned int X_position[SYNAPTICS_FINGER_MAX];
	unsigned int Y_position[SYNAPTICS_FINGER_MAX];
	unsigned char width[SYNAPTICS_FINGER_MAX];
	unsigned char pressure[SYNAPTICS_FINGER_MAX];
} ts_finger_data;

static ts_sensor_data ts_reg_data;
static ts_finger_data prev_ts_data;
static ts_finger_data curr_ts_data;

static u8 mode_melt=0;
static uint16_t SYNAPTICS_PANEL_MAX_X;
static uint16_t SYNAPTICS_PANEL_MAX_Y;
static uint16_t SYNAPTICS_PANEL_LCD_MAX_Y;
static uint16_t SYNAPTICS_PANEL_BUTTON_MIN_Y;

unsigned char  touch_fw_version = 0;

u8 ts_keytouch_lock=0;
u8 ts_reset_flag=0;

#if 0 //*v5_add start */
extern void keyled_touch_on(void);
extern u8 key_led_flag_twl;
extern u8 backlight_keyled_flag;
extern int lcd_off_boot;
extern int lcd_backlight_status;

extern int init_keyts;
#endif //*v5_add end*/
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*                                                                         */
/*                           Local Functions                               */
/*                                                                         */
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#ifdef FEATURE_LGE_TOUCH_GHOST_FINGER_IMPROVE
#define MELT_CONTROL			0xF0
#define NO_MELT 				0x00
#define MELT					0x01
#define AUTO_MELT				0x10

static u8 melt_mode = 0;
static u8 melt_flag = 0;
//static int ts_pre_state = 0; /* for checking the touch state */
static u8 ghost_finger_1 = 0; // remove for ghost finger
static u8 ghost_finger_2 = 0;
static u8 pressed = 0;
static unsigned long pressed_time;
#endif
#if 1 //*v5_add start */
static int boot_mode = 0;

extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif //*v5_add end*/

#if 0 //*v5_add start */
extern u8 synaptics_touch_power;
extern u8 synaptics_keytouch_power;
#endif //*v5_add end*/

void synaptics_touch_power_on(void)
{
#if 0 //*v5_add start */
	if(!synaptics_touch_power && !synaptics_keytouch_power)
#endif //*v5_add end*/
	{

#if 1 //*v5_add start */
	//clubsh test
	    	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_3000, "TP");
		mdelay(30);
  		hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");      
	//for power on sequence
		mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
		mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);	
#else		
		gpio_direction_output(59, 1);	// touch_ldo
		mdelay(30);
		gpio_direction_output(162, 1);	// Touch_i2c_sw
#endif //*v5_add end*/
	}
#if 0 //*v5_add start */
	synaptics_touch_power=1;
	printk("[touch] %s : touch=%d, keytouch=%d\n",__func__,synaptics_touch_power,synaptics_keytouch_power);
#endif //*v5_add end*/
	return;
}

void synaptics_touch_power_off(void)
{
#if 0 //*v5_add start */
	synaptics_touch_power=0;

	if(!synaptics_touch_power && !synaptics_keytouch_power)
#endif //*v5_add end*/
	{
		gpio_direction_output(162, 0);
		mdelay(30);
		gpio_direction_output(59, 0);
	}
#if 0 //*v5_add start */
	printk("[touch] %s : touch=%d, keytouch=%d\n",__func__,synaptics_touch_power,synaptics_keytouch_power);
#endif //*v5_add end*/
	return;
}

static void synaptics_ts_init_delayed_work(struct work_struct *work)
{
	int ret;

	i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE, 0x00); //interrupt disable
#if 0 //*v5_add start */
	disable_irq(hub_ts_client->irq);
#else
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

#endif //*v5_add end*/
	printk("[touch] %s\n",__func__);
	//ret = i2c_smbus_read_i2c_block_data(hub_ts_client, SYNAPTICS_DATA_BASE_REG, sizeof(ts_reg_data), (u8 *)&ts_reg_data);
	ret = i2c_smbus_read_i2c_block_data(hub_ts_client, 0x14, 1, (u8 *)&ts_reg_data);
	if(ret<0)
	{
		printk("[touch] i2c read fail. SYNAPTICS_DATA_BASE_REG\n");
	}

	ret = i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_CONTROL_REG, SYNAPTICS_CONTROL_NOSLEEP); /* wake up */
	if(ret<0)
	{
		printk("[touch] i2c read fail. SYNAPTICS_CONTROL_NOSLEEP\n");
	}

	ret = i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_DELTA_X_THRES_REG, 0x04);
	if(ret<0)
	{
		printk("[touch] i2c read fail. SYNAPTICS_DELTA_X_THRES_REG\n");
	}

	ret = i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_DELTA_Y_THRES_REG, 0x04);
	if(ret<0)
	{
		printk("[touch] i2c read fail. SYNAPTICS_DELTA_Y_THRES_REG\n");
	}

	ret = i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_REDUCE_MODE_REG, 0x09); // countinous mode
	if(ret<0)
	{
		printk("[touch] i2c read fail. SYNAPTICS_REDUCE_MODE_REG\n");
	}

	ret = i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_TS_SENSITYVITY_REG, SYNAPTICS_TS_SENSITYVITY_VALUE);
	if(ret<0)
	{
		printk("[touch] i2c read fail. SYNAPTICS_TS_SENSITYVITY_REG\n");
	}

#ifdef FEATURE_LGE_TOUCH_GHOST_FINGER_IMPROVE
	ret = i2c_smbus_write_byte_data(hub_ts_client, MELT_CONTROL, MELT);
	if(ret<0)
	{
		mdelay(20);
		ret = i2c_smbus_write_byte_data(hub_ts_client, MELT_CONTROL, MELT);
		printk("[touch] i2c read fail. SYNAPTICS_TS_SENSITYVITY_REG\n");
	}
#endif

	init_stabled = 1;
	melt_flag=1;
#if 0 //*v5_add start */
	enable_irq(hub_ts_client->irq);
#else
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif //*v5_add end*/
	i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE, 0x07); //interrupt enable
}

#ifdef FEATURE_LGE_TOUCH_GRIP_SUPPRESSION
static int g_gripIgnoreRangeValue = 0;
static int g_receivedPixelValue = 0;

static int touch_ConvertPixelToRawData(int pixel)
{
	int result = 0;

	result = (SYNAPTICS_PANEL_MAX_X * pixel) /480;

	return result;
}

void synaptics_ts_ldo_write(u8 reg , u8 val)
{
  int ret;

  ret = i2c_smbus_write_byte_data(hub_ts_client, reg , val);

  return;
}

ssize_t touch_gripsuppression_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	sprintf(buf, "%d\n", g_receivedPixelValue);
	pr_debug("[KERNEL] [touch] SHOW (%d) \n", g_receivedPixelValue);

	return (ssize_t)(strlen(buf)+1);
}

ssize_t touch_gripsuppression_store(struct device *dev, struct device_attribute *attr, const char *buffer, size_t count)
{
	sscanf(buffer, "%d", &g_receivedPixelValue);
	g_gripIgnoreRangeValue = touch_ConvertPixelToRawData(g_receivedPixelValue);
	pr_debug("[KERNEL] [touch] STORE  pixel(%d) convet (%d) \n", g_receivedPixelValue, g_gripIgnoreRangeValue);

	return count;
}

DEVICE_ATTR(gripsuppression, 0666, touch_gripsuppression_show, touch_gripsuppression_store);
#endif

static u8 ts_cnt;
static u8 ts_check=0;
static u8 ts_finger=0;
static u8 ts_i2c_ret=0;
static u8 finger_count=0;
static u8 ghost_count=0;
static u8 dummy=0;

static int GetTouchKey(u8 p_Value)
{
	switch(p_Value)
	{
	case 1:
		return 	KEY_MENU;
	case 2:
		return	KEY_HOMEPAGE;
	case 4:
		return	KEY_BACK;
	case 8:
		return	KEY_SEARCH;
	}
	return -1;		
}

static void synaptics_ts_work_func(struct work_struct *work)
{
	struct synaptics_ts_data *ts = container_of(work, struct synaptics_ts_data, work);

	printk("synaptics_ts_work_func");

do{
	ts_keytouch_lock=5;

	if(!ts_reset_flag)
	{	static u8 button_value_prev = 0;
		static u8 button_value_curr = 0;
		
//		printk("if(!ts_reset_flag)");
		
		ts_i2c_ret=i2c_smbus_read_i2c_block_data(ts->client, SYNAPTICS_DATA_BASE_REG, sizeof(ts_reg_data), (u8 *)&ts_reg_data);

		i2c_smbus_read_i2c_block_data(ts->client, ButtonDataBaseReg, sizeof(u8), (u8*)&button_value_curr);

		if( button_value_prev != button_value_curr )
		{
			if( button_value_curr == 0 )
			{	// Released
				input_report_key(ts->input_dev, GetTouchKey(button_value_prev), 0);				
				printk("[chiwon] Released!\n");
			}
			else
			{	// Pressed
				input_report_key(ts->input_dev, GetTouchKey(button_value_curr), 1);				
				printk("[chiwon] Pressed!\n");
			}
		
}

		button_value_prev = button_value_curr;
		
//		printk("[chiwon] button %d\n", button_value_curr);

	//reduce_mode=i2c_smbus_read_byte_data(ts->client, 0x51);
	//i2c_smbus_read_byte_data(ts->client, 0x14);

	for(ts_cnt = 0; ts_cnt < SYNAPTICS_FINGER_MAX; ts_cnt++)
	{
		ts_check = 1 << ((ts_cnt%4)*2);
		ts_finger = (u8)(ts_cnt/4);

		//printk("for(ts_cnt = 0; ts_cnt < SYNAPTICS_FINGER_MAX; ts_cnt++) : %d, %d\n", ts_check, ts_finger);

		if((ts_reg_data.finger_state_reg[ts_finger] & ts_check) == ts_check)
		{
			//printk("if((ts_reg_data.finger_state_reg[ts_finger] & ts_check) == ts_check)\n");
			
			curr_ts_data.X_position[ts_cnt] = (int)TS_SNTS_GET_X_POSITION(ts_reg_data.fingers_data[ts_cnt][0], ts_reg_data.fingers_data[ts_cnt][2]);
			curr_ts_data.Y_position[ts_cnt] = (int)TS_SNTS_GET_Y_POSITION(ts_reg_data.fingers_data[ts_cnt][1], ts_reg_data.fingers_data[ts_cnt][2]);

#ifdef FEATURE_LGE_TOUCH_JITTERING_IMPROVE
				if(!(abs(curr_ts_data.X_position[ts_cnt]-prev_ts_data.X_position[ts_cnt]) > 3 && abs(curr_ts_data.Y_position[ts_cnt]-prev_ts_data.Y_position[ts_cnt]) > 3))
				{
					curr_ts_data.X_position[ts_cnt] = prev_ts_data.X_position[ts_cnt];
					curr_ts_data.Y_position[ts_cnt] = prev_ts_data.Y_position[ts_cnt];
				}
#endif

#ifdef FEATURE_LGE_TOUCH_GRIP_SUPPRESSION
			if ( (g_gripIgnoreRangeValue > 0) && ( (curr_ts_data.X_position[ts_cnt] <= g_gripIgnoreRangeValue ) ||
															(curr_ts_data.X_position[ts_cnt] >= (SYNAPTICS_PANEL_MAX_X - g_gripIgnoreRangeValue) )) )
			{
				pr_debug("[touch] girp region pressed. ignore!!!\n" );
			}
			else
			{
#endif	
				//printk("else ( (g_gripIgnoreRangeValue > 0) && ( (curr_ts_data.X_position[ts_cnt] <= g_gripIgnoreRangeValue )\n");

				if ((((ts_reg_data.fingers_data[ts_cnt][3] & 0xf0) >> 4) - (ts_reg_data.fingers_data[ts_cnt][3] & 0x0f)) > 0)
					curr_ts_data.width[ts_cnt] = (ts_reg_data.fingers_data[ts_cnt][3] & 0xf0) >> 4;
				else
					curr_ts_data.width[ts_cnt] = ts_reg_data.fingers_data[ts_cnt][3] & 0x0f;

				curr_ts_data.pressure[ts_cnt] = ts_reg_data.fingers_data[ts_cnt][4];
				curr_ts_data.touch_status[ts_cnt] = 1;
				finger_count++;
			}
		}
		else
		{	//printk("else ((ts_reg_data.finger_state_reg[ts_finger] & ts_check) == ts_check)\n");
			curr_ts_data.touch_status[ts_cnt] = 0;	

		}


		if(curr_ts_data.touch_status[ts_cnt])
		{
			//printk("if(curr_ts_data.touch_status[ts_cnt])\n");
			if(finger_count == 1 && !ts_cnt)
			{
#ifdef FEATURE_LGE_TOUCH_GHOST_FINGER_IMPROVE
				if(!pressed) {
					pressed_time = jiffies;
					ghost_finger_1 = 1;
					pressed++;
				}
#endif

				input_report_abs(ts->input_dev, ABS_PRESSURE, curr_ts_data.pressure[ts_cnt]);
				input_report_key(ts->input_dev, BTN_TOUCH, 1);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, curr_ts_data.pressure[ts_cnt]);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[ts_cnt]);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[ts_cnt]);

				input_mt_sync(ts->input_dev);

				printk("[touch#1]*multifinger* finger_count=%d  ts_i=%d (X, Y) = (%d, %d), z = %d, w = %d\n",finger_count, ts_cnt, curr_ts_data.X_position[ts_cnt], curr_ts_data.Y_position[ts_cnt], curr_ts_data.pressure[ts_cnt], curr_ts_data.width[ts_cnt]);
			}
			else // multi-finger
			{


				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, curr_ts_data.X_position[ts_cnt]);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, curr_ts_data.Y_position[ts_cnt]);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, curr_ts_data.pressure[ts_cnt]);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, curr_ts_data.width[ts_cnt]);
#if 1
				input_report_abs(ts->input_dev, ABS_HAT0X, curr_ts_data.X_position[ts_cnt]);
  			    input_report_abs(ts->input_dev, ABS_HAT0Y, curr_ts_data.Y_position[ts_cnt]);
				input_report_key(ts->input_dev, BTN_2, 1);
#endif

				
				input_mt_sync(ts->input_dev);

				if(curr_ts_data.X_position[1] || curr_ts_data.Y_position[1]) ghost_count=0;

				printk("[touch#2]*multifinger* finger_count=%d  ts_i=%d (X, Y) = (%d, %d), z = %d, w = %d\n",finger_count, ts_cnt, curr_ts_data.X_position[ts_cnt], curr_ts_data.Y_position[ts_cnt], curr_ts_data.pressure[ts_cnt], curr_ts_data.width[ts_cnt]);

			}
		}
#if 0
		else
		{
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
					input_report_key(ts->input_dev, BTN_TOUCH, 0);
					input_report_key(ts->input_dev, BTN_2, 0);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
					input_mt_sync(ts->input_dev);
		}

#endif
		
	}
#ifdef FEATURE_LGE_TOUCH_GHOST_FINGER_IMPROVE
		if(!melt_mode && melt_flag)
		{
			//printk("if(!melt_mode && melt_flag)\n");
			if(pressed)
			{
				//printk("if(pressed)");
				if( TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg[0]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_2(ts_reg_data.finger_state_reg[0]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_3(ts_reg_data.finger_state_reg[0]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg[1]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg[1]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_2(ts_reg_data.finger_state_reg[1]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_3(ts_reg_data.finger_state_reg[1]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg[2]) == 1 ||
					TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg[2]) == 1 )
				{
					//printk("if( TS_SNTS_GET_FINGER_STATE_1(ts_reg_dat\n");	
					ghost_finger_2 = 1;
				}
			}
            if((TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg[0]) == 0) && ghost_finger_1 == 1 && ghost_finger_2 == 0 && pressed == 1)
            {
	            if(jiffies - pressed_time < 2 * HZ)
	            {
                	ghost_count++;

                    if(ghost_count > 2)
                    {
                    	ts_i2c_ret=i2c_smbus_write_byte_data(ts->client, MELT_CONTROL, NO_MELT);
						if(ts_i2c_ret<0)
						{
							//printk("[touch] NO_MELT MODE retry.\n");
							ts_i2c_ret=i2c_smbus_write_byte_data(ts->client, MELT_CONTROL, NO_MELT);
						}
                        ghost_count = 0;
                        melt_mode++;
                        mode_melt=0;
                        //printk("[touch] NO_MELT MODE\n");
                    }
                    //printk("%s() mode change, ghost count : %d\n", __func__, ghost_count);
				}
            	ghost_finger_1 = 0;
            	pressed = 0;
			}
			if( !TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg[0]) &&
				!TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg[0]) &&
				!TS_SNTS_GET_FINGER_STATE_2(ts_reg_data.finger_state_reg[0]) &&
				!TS_SNTS_GET_FINGER_STATE_3(ts_reg_data.finger_state_reg[0]) &&
				!TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg[1]) &&
				!TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg[1]) &&
				!TS_SNTS_GET_FINGER_STATE_2(ts_reg_data.finger_state_reg[1]) &&
				!TS_SNTS_GET_FINGER_STATE_3(ts_reg_data.finger_state_reg[1]) &&
				!TS_SNTS_GET_FINGER_STATE_0(ts_reg_data.finger_state_reg[2]) &&
				!TS_SNTS_GET_FINGER_STATE_1(ts_reg_data.finger_state_reg[2]))
			{
				ghost_finger_1 = 0;
				ghost_finger_2 = 0;
				pressed = 0;
			}
	}
#endif
	}
	finger_count=0;

	input_mt_sync(ts->input_dev);
	input_sync(ts->input_dev);

	if(ts_reset_flag==2)
	{
		printk("[touch] reset & init.\n");

		ts_reset_flag=0;
#if 0 //*v5_add start */
		disable_irq(ts->client->irq);
#else
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

#endif //*v5_add end*/

		melt_mode = 0;
		melt_flag=1;
		ghost_finger_1 = 0;
		ghost_finger_2 = 0;
		pressed = 0;
		ghost_count=0;
		schedule_delayed_work(&ts->init_delayed_work, msecs_to_jiffies(500));
#if 0 //*v5_add start */		
		enable_irq(ts->client->irq);
#else
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif //*v5_add end*/
	}
	else
	{
#if 0 //*v5_add start */
		if(!key_led_flag_twl && backlight_keyled_flag)		keyled_touch_on();
#endif //*v5_add end*/
	}

}while(/*!gpio_get_value(TOUCH_INT_N_GPIO)*/1);

SYNAPTICS_TS_IDLE:
#if 0 //*v5_add start */

	if (ts->use_irq) {
		enable_irq(ts->client->irq);
#else
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif //*v5_add end*/
#if 0 //*v5_add start */
	}
#endif //*v5_add end*/
}
static enum hrtimer_restart synaptics_ts_timer_func(struct hrtimer *timer)
{
	struct synaptics_ts_data *ts = container_of(timer, struct synaptics_ts_data, timer);

	queue_work(synaptics_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL); /* 12.5 msec */

	return HRTIMER_NORESTART;
}

static irqreturn_t synaptics_ts_irq_handler(int irq, void *dev_id)
{




#if 0 //*v5_add start */
		struct synaptics_ts_data *ts = dev_id;
#else
	        struct synaptics_ts_data *ts = ts_hub;
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif //*v5_add end*/

	//                                             
#if 0 //*v5_add start */
	disable_irq_nosync(ts->client->irq);
#endif //*v5_add end*/
	queue_work(synaptics_wq, &ts->work);
	return IRQ_HANDLED;
}


static unsigned char synaptics_ts_check_fwver(struct i2c_client *client)
{
	unsigned char RMI_Query_BaseAddr;
	unsigned char FWVersion_Addr;

	unsigned char SynapticsFirmVersion;

	RMI_Query_BaseAddr = i2c_smbus_read_byte_data(client, SYNAPTICS_RMI_QUERY_BASE_REG);
	FWVersion_Addr = RMI_Query_BaseAddr+3;

	SynapticsFirmVersion = i2c_smbus_read_byte_data(client, FWVersion_Addr);
    printk(KERN_INFO"[touch] Touch controller Firmware Version = %x\n", SynapticsFirmVersion);

	return SynapticsFirmVersion;
}


static ssize_t hub_ts_FW_show(struct device *dev,  struct device_attribute *attr,  char *buf)
{
	//                                                                       
	int r;
	char product_id[6];
	uint8_t product_id_addr;

	r = snprintf(buf, PAGE_SIZE,
		"%d\n", touch_fw_version);
	product_id_addr = (i2c_smbus_read_byte_data(hub_ts_client, SYNAPTICS_RMI_QUERY_BASE_REG)) + 11;
	i2c_smbus_read_i2c_block_data(hub_ts_client, product_id_addr, sizeof(product_id), (u8 *)&product_id[0]);
	printk("Product ID : < %s >\n", product_id);

	return r;
	//                                                                     
}
static DEVICE_ATTR(fw, 0666, hub_ts_FW_show, NULL);




#ifdef SYNAPTICS_SUPPORT_FW_UPGRADE
static unsigned long ExtractLongFromHeader(const unsigned char *SynaImage)  // Endian agnostic
{
  return((unsigned long)SynaImage[0] +
         (unsigned long)SynaImage[1]*0x100 +
         (unsigned long)SynaImage[2]*0x10000 +
         (unsigned long)SynaImage[3]*0x1000000);
}

static void CalculateChecksum(uint16_t *data, uint16_t len, uint32_t *dataBlock)
{
  unsigned long temp = *data++;
  unsigned long sum1;
  unsigned long sum2;

  *dataBlock = 0xffffffff;

  sum1 = *dataBlock & 0xFFFF;
  sum2 = *dataBlock >> 16;

  while (len--)
  {
    sum1 += temp;
    sum2 += sum1;
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
  }

  *dataBlock = sum2 << 16 | sum1;
}

static void SpecialCopyEndianAgnostic(uint8_t *dest, uint16_t src)
{
  dest[0] = src%0x100;  //Endian agnostic method
  dest[1] = src/0x100;
}


static bool synaptics_ts_fw_upgrade(struct i2c_client *client)
{
	int i;
	int j;

	uint8_t FlashQueryBaseAddr, FlashDataBaseAddr;
	uint8_t RMICommandBaseAddr;

	uint8_t BootloaderIDAddr;
	uint8_t BlockSizeAddr;
	uint8_t FirmwareBlockCountAddr;
	uint8_t ConfigBlockCountAddr;

	uint8_t BlockNumAddr;
	uint8_t BlockDataStartAddr;

	uint8_t current_fw_ver;

	uint8_t bootloader_id[2];

	uint8_t temp_array[2], temp_data, flashValue, m_firmwareImgVersion;
	uint8_t checkSumCode;

	uint16_t ts_block_size, ts_config_block_count, ts_fw_block_count;
	uint16_t m_bootloadImgID;

	uint32_t ts_config_img_size;
	uint32_t ts_fw_img_size;
	uint32_t pinValue, m_fileSize, m_firmwareImgSize, m_configImgSize, m_FirmwareImgFile_checkSum;

	////////////////////////////

	printk("[touch] synaptics_upgrade firmware [START]\n");

/*
	if(!(synaptics_ts_check_fwver(client) < SynapticsFirmware[0x1F]))
	{
		// Firmware Upgrade does not necessary!!!!
		printk("[Touch Driver] Synaptics_UpgradeFirmware does not necessary!!!!\n");
		return true;
	}
*/
	current_fw_ver = synaptics_ts_check_fwver(client);
#if 0 //*v5_add start */
	//                                                                       
	if((current_fw_ver >= 0x64 && SynaFirmware[0x1F] >= 0x64) || (current_fw_ver < 0x64 && SynaFirmware[0x1F] < 0x64))
	{
		if(!(current_fw_ver < SynaFirmware[0x1F]))
		{
			// Firmware Upgrade does not necessary!!!!
			printk(KERN_INFO"[touch] synaptics_upgrade firmware does not necessary!!!!\n");
			return true;
		}
	}
	//                                                                     
#endif //*v5_add end*/

	//                                                                       
	/*if((current_fw_ver >= 0x64 && SynapticsFirmware[0x1F] >= 0x64) || (current_fw_ver < 0x64 && SynapticsFirmware[0x1F] < 0x64))
	{
		if(!(current_fw_ver < SynapticsFirmware[0x1F]))
		{
			// Firmware Upgrade does not necessary!!!!
			printk("[Touch Driver] Synaptics_UpgradeFirmware does not necessary!!!!\n");
			return true;
		}
	}*/
	//                                                                     

	// Address Configuration
	FlashQueryBaseAddr = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_QUERY_BASE_REG);

	BootloaderIDAddr = FlashQueryBaseAddr;
	BlockSizeAddr = FlashQueryBaseAddr + 3;
	FirmwareBlockCountAddr = FlashQueryBaseAddr + 5;
	ConfigBlockCountAddr = FlashQueryBaseAddr + 7;


	FlashDataBaseAddr = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_DATA_BASE_REG);

	BlockNumAddr = FlashDataBaseAddr;
	BlockDataStartAddr = FlashDataBaseAddr + 2;

	// Get New Firmware Information from Header
	//                                                                                              
	m_fileSize = sizeof(SynaFirmware) -1;

	checkSumCode         = ExtractLongFromHeader(&(SynaFirmware[0]));
	m_bootloadImgID      = (unsigned int)SynaFirmware[4] + (unsigned int)SynaFirmware[5]*0x100;
	m_firmwareImgVersion = SynaFirmware[7];
	m_firmwareImgSize    = ExtractLongFromHeader(&(SynaFirmware[8]));
	m_configImgSize      = ExtractLongFromHeader(&(SynaFirmware[12]));

	CalculateChecksum((uint16_t*)&(SynaFirmware[4]), (uint16_t)(m_fileSize-4)>>1, &m_FirmwareImgFile_checkSum);

	// Get Current Firmware Information
	i2c_smbus_read_i2c_block_data(client, BlockSizeAddr, sizeof(temp_array), (u8 *)&temp_array[0]);
	ts_block_size = temp_array[0] + (temp_array[1] << 8);

	i2c_smbus_read_i2c_block_data(client, FirmwareBlockCountAddr, sizeof(temp_array), (u8 *)&temp_array[0]);
	ts_fw_block_count = temp_array[0] + (temp_array[1] << 8);
	ts_fw_img_size = ts_block_size * ts_fw_block_count;

	i2c_smbus_read_i2c_block_data(client, ConfigBlockCountAddr, sizeof(temp_array), (u8 *)&temp_array[0]);
	ts_config_block_count = temp_array[0] + (temp_array[1] << 8);
	ts_config_img_size = ts_block_size * ts_config_block_count;

	i2c_smbus_read_i2c_block_data(client, BootloaderIDAddr, sizeof(bootloader_id), (u8 *)&bootloader_id[0]);
	printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: BootloaderID %02x %02x\n", bootloader_id[0], bootloader_id[1]);

	// Compare
	if (m_fileSize != (0x100+m_firmwareImgSize+m_configImgSize))
	{
		printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Error : Invalid FileSize\n");
		return true;
	}

	if (m_firmwareImgSize != ts_fw_img_size)
	{
		printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Error : Invalid Firmware Image Size\n");
		return true;
	}

	if (m_configImgSize != ts_config_img_size)
	{
		printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Error : Invalid Config Image Size\n");
		return true;
	}

	// Flash Write Ready - Flash Command Enable & Erase
	//i2c_smbus_write_block_data(client, BlockDataStartAddr, sizeof(bootloader_id), &bootloader_id[0]);
	// How can i use 'i2c_smbus_write_block_data'
	for(i = 0; i < sizeof(bootloader_id); i++)
	{
		if(i2c_smbus_write_byte_data(client, BlockDataStartAddr+i, bootloader_id[i]))
			printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Address %02x, Value %02x\n", BlockDataStartAddr+i, bootloader_id[i]);
	}

	do
	{
		flashValue = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_INT_STATUS_REG);
	} while((flashValue & 0x0f) != 0x00);

	i2c_smbus_write_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_ENABLE);

	do
	{
		pinValue = gpio_get_value(TOUCH_INT_N_GPIO);
		mdelay(1);
	} while(pinValue);
	do
	{
		flashValue = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_INT_STATUS_REG);
	} while(flashValue != 0x80);
	flashValue = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG);

	printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Flash Program Enable Setup Complete\n");

	//i2c_smbus_write_block_data(client, BlockDataStartAddr, sizeof(bootloader_id), &bootloader_id[0]);
	// How can i use 'i2c_smbus_write_block_data'
	for(i = 0; i < sizeof(bootloader_id); i++)
	{
		if(i2c_smbus_write_byte_data(client, BlockDataStartAddr+i, bootloader_id[i]))
			printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Address %02x, Value %02x\n", BlockDataStartAddr+i, bootloader_id[i]);
	}

	if(m_firmwareImgVersion == 0 && ((unsigned int)bootloader_id[0] + (unsigned int)bootloader_id[1]*0x100) != m_bootloadImgID)
	{
		printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Error : Invalid Bootload Image\n");
		return true;
	}

	i2c_smbus_write_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_ERASEALL);

	printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: SYNAPTICS_FLASH_CMD_ERASEALL\n");

	do
	{
		pinValue = gpio_get_value(TOUCH_INT_N_GPIO);
		mdelay(1);
	} while(pinValue);
	do
	{
		flashValue = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_INT_STATUS_REG);
	} while(flashValue != 0x80);

	printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Flash Erase Complete\n");

	// Flash Firmware Data Write
	for(i = 0; i < ts_fw_block_count; ++i)
	{
		temp_array[0] = i & 0xff;
		temp_array[1] = (i & 0xff00) >> 8;

		// Write Block Number
		//i2c_smbus_write_block_data(client, BlockNumAddr, sizeof(temp_array), &temp_array[0]);
		// How can i use 'i2c_smbus_write_block_data'
		for(j = 0; j < sizeof(temp_array); j++)
		{
			i2c_smbus_write_byte_data(client, BlockNumAddr+j, temp_array[j]);
		}

		// Write Data Block&SynapticsFirmware[0]
		//i2c_smbus_write_block_data(client, BlockDataStartAddr, ts_block_size, &SynapticsFirmware[0x100+i*ts_block_size]);
		// How can i use 'i2c_smbus_write_block_data'
		for(j = 0; j < ts_block_size; j++)
		{
			i2c_smbus_write_byte_data(client, BlockDataStartAddr+j, SynaFirmware[0x100+i*ts_block_size+j]);
		}

		// Issue Write Firmware Block command
		i2c_smbus_write_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_FW_WRITE);
		do
		{
			pinValue = gpio_get_value(TOUCH_INT_N_GPIO);
			mdelay(1);
		} while(pinValue);
		do
		{
			flashValue = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG);
			temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_INT_STATUS_REG);
		} while(flashValue != 0x80);
	} //for

	printk(KERN_WARNING"[touch] Synaptics_UpgradeFirmware :: Flash Firmware Write Complete\n");

	// Flash Firmware Config Write
	for(i = 0; i < ts_config_block_count; i++)
	{
		SpecialCopyEndianAgnostic(&temp_array[0], i);

		// Write Configuration Block Number
		i2c_smbus_write_block_data(client, BlockNumAddr, sizeof(temp_array), &temp_array[0]);
		// How can i use 'i2c_smbus_write_block_data'
		for(j = 0; j < sizeof(temp_array); j++)
		{
			i2c_smbus_write_byte_data(client, BlockNumAddr+j, temp_array[j]);
		}

		// Write Data Block
		//i2c_smbus_write_block_data(client, BlockDataStartAddr, ts_block_size, &SynapticsFirmware[0x100+m_firmwareImgSize+i*ts_block_size]);
		// How can i use 'i2c_smbus_write_block_data'
		for(j = 0; j < ts_block_size; j++)
		{
			i2c_smbus_write_byte_data(client, BlockDataStartAddr+j, SynaFirmware[0x100+m_firmwareImgSize+i*ts_block_size+j]);
		}

		// Issue Write Configuration Block command to flash command register
		i2c_smbus_write_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG, SYNAPTICS_FLASH_CMD_CONFIG_WRITE);
		do
		{
			pinValue = gpio_get_value(TOUCH_INT_N_GPIO);
			mdelay(1);
		} while(pinValue);
		do
		{
			flashValue = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG);
			temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_INT_STATUS_REG);
		} while(flashValue != 0x80);
	}

	printk("[touch] synaptics_upgrade firmware :: flash config write complete\n");


	RMICommandBaseAddr = i2c_smbus_read_byte_data(client, SYNAPTICS_RMI_CMD_BASE_REG);
	i2c_smbus_write_byte_data(client, RMICommandBaseAddr, 0x01);
	mdelay(100);

	do
	{
		pinValue = gpio_get_value(TOUCH_INT_N_GPIO);
		mdelay(1);
	} while(pinValue);
	do
	{
		flashValue = i2c_smbus_read_byte_data(client, SYNAPTICS_FLASH_CONTROL_REG);
		temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_INT_STATUS_REG);
	} while((flashValue & 0x0f) != 0x00);

	// Clear the attention assertion by reading the interrupt status register
	temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_INT_STATUS_REG);

	// Read F01 Status flash prog, ensure the 6th bit is '0'
	do
	{
		temp_data = i2c_smbus_read_byte_data(client, SYNAPTICS_DATA_BASE_REG);
	} while((temp_data & 0x40) != 0);

	return true;
}
#endif

#if 1 //*v5_add start */
void tpd_eint_interrupt_handler(void){ 
	synaptics_ts_irq_handler(0,0);

} 
#endif //*v5_add end*/

struct function_descriptor {
	u8 	query_base;
	u8 	command_base;
	u8 	control_base;
	u8 	data_base;
	u8 	int_source_count;
	u8 	id;
};

#define	PAGE_MAX_NUM				30
#define DESCRIPTION_TABLE_START		0xe9
#define PAGE_SELECT_REG				0xFF

#define CAPACITIVE_BUTTON_SENSORS	0x19

static int read_page_description_table(struct i2c_client* client)
{
	struct function_descriptor buffer;
	unsigned short u_address = 0;
	unsigned short page_num = 0;

	memset(&buffer, 0x0, sizeof(struct function_descriptor));

	for(page_num = 0; page_num < PAGE_MAX_NUM; page_num++) 
	{
		if (i2c_smbus_write_byte_data(client, PAGE_SELECT_REG, page_num) < 0) 
		{
			printk("[chiwon] i2c_smbus_write_byte_data error %d", page_num);
			return -1;
		}

		for(u_address = DESCRIPTION_TABLE_START; u_address > 10; u_address -= sizeof(struct function_descriptor)) 
		{
		//	ret=i2c_smbus_read_i2c_block_data(hub_ts_client, product_id_addr, sizeof(product_id), (u8 *)&product_id[0]);
			if (i2c_smbus_read_i2c_block_data(client, u_address, sizeof(buffer), (unsigned char *)&buffer) < 0) 
			{	printk("RMI4 Function Descriptor read fail\n");
				return -1;
			}

			printk("[chiwon] id=%x,int=%x,dat=%x,ctl=%x,cmd=%x,qur=%x\n", buffer.id, buffer.int_source_count, buffer.data_base , buffer.control_base , buffer.command_base, buffer.query_base );

			if (buffer.id == 0)
				break;

			if(buffer.id == CAPACITIVE_BUTTON_SENSORS )
			{
				ButtonDataBaseReg = buffer.data_base;
				printk("[chiwon] ButtonDataBaseReg = %d\n", ButtonDataBaseReg);
			}			
		}
	}

	if (i2c_smbus_write_byte_data(client, PAGE_SELECT_REG, 0x00) < 0) 
	{	printk("[chiwon] i2c_smbus_write_byte_data error %d", page_num);
		return -1;
	}

	return 0;
}



/*************************************************************************************************
 * 1. Set interrupt configuration
 * 2. Disable interrupt
 * 3. Power up
 * 4. Read RMI Version
 * 5. Read Firmware version & Upgrade firmware automatically
 * 6. Read Data To Initialization Touch IC
 * 7. Set some register
 * 8. Enable interrupt
*************************************************************************************************/
static int synaptics_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct synaptics_ts_data *ts;
	int ret = 0;
	uint16_t max_x;
	uint16_t max_y;
	uint8_t max_pressure;
	uint8_t max_width;

	char product_id[6];
	uint8_t product_id_addr;

	synaptics_touch_power_on();
#if 0 //*v5_add start */
	if(lcd_off_boot ==1)
	{
		printk("[touch] No Device LCD\n");
		ret = -ENODEV;
		return ret;
	}
	ts_printk("%s() -- start\n\n\n", __func__);
#endif //*v5_add end*/



	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "synaptics_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	hub_ts_client = client;
	i2c_set_clientdata(client, ts);
#if 1 //*v5_add start */
        ts_hub = ts;
#endif //*v5_add end*/
	INIT_WORK(&ts->work, synaptics_ts_work_func);
	INIT_DELAYED_WORK(&ts->init_delayed_work, synaptics_ts_init_delayed_work);

#ifdef FEATURE_LGE_TOUCH_GRIP_SUPPRESSION
	ret = device_create_file(&client->dev, &dev_attr_gripsuppression);
	if (ret) {
		pr_err("synaptics_ts_probe: grip suppression device_create_file failed\n");
		goto err_check_functionality_failed;
	}
#endif /*                                    */

  	memset(&ts_reg_data, 0x0, sizeof(ts_sensor_data));
	memset(&prev_ts_data, 0x0, sizeof(ts_finger_data));
  	memset(&curr_ts_data, 0x0, sizeof(ts_finger_data));

	// device check
	product_id_addr = (i2c_smbus_read_byte_data(hub_ts_client, SYNAPTICS_RMI_QUERY_BASE_REG)) + 11;
	ret=i2c_smbus_read_i2c_block_data(hub_ts_client, product_id_addr, sizeof(product_id), (u8 *)&product_id[0]);

	if (ret<0) {
		printk("synaptics_ts_probe: No Such Device\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	if(strncmp(product_id, SYNAPTICS_TM1576_PRODUCT_ID, 6) == 0)
	{
		printk(KERN_ERR "synaptics_ts_probe: product ID : TM1576\n");
		SYNAPTICS_PANEL_MAX_X = SYNAPTICS_TM1576_RESOLUTION_X;
		SYNAPTICS_PANEL_MAX_Y = SYNAPTICS_TM1576_RESOLUTION_Y;
		SYNAPTICS_PANEL_LCD_MAX_Y = SYNAPTICS_TM1576_LCD_ACTIVE_AREA;
		SYNAPTICS_PANEL_BUTTON_MIN_Y = SYNAPTICS_TM1576_BUTTON_ACTIVE_AREA;
	}
	else if(strncmp(product_id, SYNAPTICS_TM1702_PRODUCT_ID, 6) == 0)
	{
		printk(KERN_ERR "synaptics_ts_probe: product ID : TM1702\n");
		SYNAPTICS_PANEL_MAX_X = SYNAPTICS_TM1702_RESOLUTION_X;
		SYNAPTICS_PANEL_MAX_Y = SYNAPTICS_TM1702_RESOLUTION_Y;
		SYNAPTICS_PANEL_LCD_MAX_Y = SYNAPTICS_TM1702_LCD_ACTIVE_AREA;
		SYNAPTICS_PANEL_BUTTON_MIN_Y = SYNAPTICS_TM1702_BUTTON_ACTIVE_AREA;
#if 0 //*v5_add start */
		synaptics_ts_fw_upgrade(hub_ts_client);
#endif //*v5_add end*/
	}
	//                                                            
	else if(strncmp(product_id, SYNAPTICS_TM1743_PRODUCT_ID, 6) == 0)
	{
		printk("synaptics_ts_probe: product ID : TM1743\n");
		SYNAPTICS_PANEL_MAX_X = SYNAPTICS_TM1743_RESOLUTION_X;
		SYNAPTICS_PANEL_MAX_Y = SYNAPTICS_TM1743_RESOLUTION_Y;
		SYNAPTICS_PANEL_LCD_MAX_Y = SYNAPTICS_TM1743_LCD_ACTIVE_AREA;
		SYNAPTICS_PANEL_BUTTON_MIN_Y = SYNAPTICS_TM1743_BUTTON_ACTIVE_AREA;
#if 0 //*v5_add start */	
                if(ret>=0)	synaptics_ts_fw_upgrade(hub_ts_client);
#endif //*v5_add end*/
	}
	//                                                          
	else
	{
		printk(KERN_ERR "synaptics_ts_probe: product ID : %s error\n", product_id);
		SYNAPTICS_PANEL_MAX_X = SYNAPTICS_TM1743_RESOLUTION_X;
		SYNAPTICS_PANEL_MAX_Y = SYNAPTICS_TM1743_RESOLUTION_Y;
		SYNAPTICS_PANEL_LCD_MAX_Y = SYNAPTICS_TM1743_LCD_ACTIVE_AREA;
		SYNAPTICS_PANEL_BUTTON_MIN_Y = SYNAPTICS_TM1743_BUTTON_ACTIVE_AREA;
	}

	if(ret>=0)	touch_fw_version=synaptics_ts_check_fwver(hub_ts_client);


	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		printk(KERN_ERR "synaptics_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "synaptics_v5";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);

	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_HOMEPAGE, ts->input_dev->keybit);
	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
	
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	max_x = SYNAPTICS_PANEL_MAX_X;
	max_y = SYNAPTICS_PANEL_MAX_Y;
	max_pressure = 0xFF;
	max_width = 0x0F;

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, max_pressure, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, max_width, 0, 0);

	ts_printk("synaptics_ts_probe: max_x %d, max_y %d\n", max_x, max_y);

	ts->input_dev->name = client->name;
	ts->input_dev->id.bustype = BUS_HOST;
	ts->input_dev->dev.parent = &client->dev;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;

	input_set_drvdata(ts->input_dev, ts);

	/* ts->input_dev->name = ts->keypad_info->name; */
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk("synaptics_ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	printk("########## irq [%d], irqflags[0x%x]\n", client->irq, IRQF_TRIGGER_FALLING);

	//i2c_smbus_write_byte_data(hub_ts_client, SYNAPTICS_RIM_CONTROL_INTERRUPT_ENABLE, 0x00); //interrupt disable
#if 0 //*v5_add start */
	if (client->irq) {
		ret = request_irq(client->irq, synaptics_ts_irq_handler, IRQF_TRIGGER_FALLING, client->name, ts);

		if (ret == 0) {
			ts->use_irq = 1;
			ts_printk("request_irq\n");
			}
		else
			dev_err(&client->dev, "request_irq failed\n");
	}
	if (!ts->use_irq) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = synaptics_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

	ret = device_create_file(&client->dev, &dev_attr_fw);
	if (ret) {
		printk( "JUSTIN Touchscreen : touch screen_probe: Fail\n");
		device_remove_file(&client->dev, &dev_attr_fw);
		return ret;
	}
#else 
	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

#endif //*v5_add end*/
	schedule_delayed_work(&ts->init_delayed_work, msecs_to_jiffies(15000));

//	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
//	ts->early_suspend.suspend = synaptics_ts_early_suspend;
//	ts->early_suspend.resume = synaptics_ts_late_resume;
//	register_early_suspend(&ts->early_suspend);

	ts_printk("[chiwon] tpd_load_status = 1\n");
	tpd_load_status = 1;

	input_set_capability(ts->input_dev,EV_KEY,KEY_MENU);
	input_set_capability(ts->input_dev,EV_KEY,KEY_HOMEPAGE);
	input_set_capability(ts->input_dev,EV_KEY,KEY_BACK);
	input_set_capability(ts->input_dev,EV_KEY,KEY_SEARCH);
	
	read_page_description_table(ts->client);
	

	ts_printk("synaptics_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");
	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

void synaptics_ts_disable_irq()
{
#if 0 //*v5_add start */
  	if(lcd_off_boot == 1)	return;
	disable_irq(hub_ts_client->irq);
#else
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif //*v5_add end*/

}
EXPORT_SYMBOL(synaptics_ts_disable_irq);

static int synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);

#ifdef FEATURE_LGE_TOUCH_GRIP_SUPPRESSION
	device_remove_file(&client->dev, &dev_attr_gripsuppression);
#endif
	kfree(ts);
	return 0;
}

static int synaptics_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	int ret;
	init_stabled = 0;

	if (ts->use_irq)
#if 0 //*v5_add start */		
		disable_irq(client->irq);
#else
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif //*v5_add end*/
	else
		hrtimer_cancel(&ts->timer);

	ret = cancel_work_sync(&ts->work);

	//if (ret && ts->use_irq) /* if work was pending disable-count is now 2 */
	//	enable_irq(client->irq);

#ifdef FEATURE_LGE_TOUCH_GHOST_FINGER_IMPROVE
	melt_mode = 0;
	ghost_finger_1 = 0;
	ghost_finger_2 = 0;
	pressed = 0;
	ghost_count=0;
#endif

	//synaptics_touch_power_off();

	return 0;
}

static int synaptics_ts_resume(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	init_stabled = 1;

	if (ts->use_irq)
#if 0 //*v5_add start */		
		enable_irq(client->irq);
#else
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif //*v5_add end*/

	if (!ts->use_irq)
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	memset(&ts_reg_data, 0x0, sizeof(ts_sensor_data));
	memset(&prev_ts_data, 0x0, sizeof(ts_finger_data));
  	memset(&curr_ts_data, 0x0, sizeof(ts_finger_data));

	schedule_delayed_work(&ts->init_delayed_work, msecs_to_jiffies(400));

	return 0;
}

static void synaptics_ts_early_suspend(struct early_suspend *h)
{
	struct synaptics_ts_data *ts;
	ts = container_of(h, struct synaptics_ts_data, early_suspend);

	synaptics_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void synaptics_ts_late_resume(struct early_suspend *h)
{
	struct synaptics_ts_data *ts;
	ts = container_of(h, struct synaptics_ts_data, early_suspend);



#if 0 //*v5_add start */		
	if(!synaptics_touch_power && !synaptics_keytouch_power)
#endif //*v5_add end*/
	{
		synaptics_touch_power_on();
		mdelay(500);
	}
	synaptics_ts_resume(ts->client);
}

static const struct i2c_device_id synaptics_ts_id[] = {
#if 1 //*v5_add start */
	{ "mtk-tpd", 0 },
#else
	{ "hub_synaptics_ts", 0 },
#endif //*v5_add end*/
	{ },
};

static struct i2c_driver synaptics_ts_driver = {
	.probe		= synaptics_ts_probe,
	.remove		= synaptics_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= synaptics_ts_suspend,
	.resume		= synaptics_ts_resume,
#endif
	.id_table	= synaptics_ts_id,
	.driver = {
#if 1 //*v5_add start */
		.name	= "mtk-tpd",
#else
		.name	= "hub_synaptics_ts",
#endif
		.owner = THIS_MODULE,
	},
};

#if 1 //*v5_add start */
int tpd_local_init(void) {
	boot_mode = get_boot_mode();
	
    // Software reset mode will be treated as normal boot
    if(boot_mode==3) boot_mode = NORMAL_BOOT;
	if(i2c_add_driver(&synaptics_ts_driver)!=0) {
      printk("unable to add i2c driver.\n");
      return -1;
    }


#if 0 //seven  
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	memcpy(tpd_calmat, tpd_calmat_local, 8*4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;
#endif // end of seven
	return 0;
}

/* Function to manage low power suspend */
void tpd_suspend(struct early_suspend *h)
{
#if 0
	tpd_halt = 1;

	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	uint8_t bfffer[2] = {0x01,0x00};
	tpd_i2c_master_rs_send(i2c_client,bfffer,1<<8|1);
	TPD_DMESG("tpd_suspend, the old mode is  %x\n",bfffer[0]);	

	bfffer[0]=0x01;
	bfffer[1] &= 0x00;
	tpd_i2c_master_send(i2c_client,bfffer,2);
	
#ifdef TPD_HAVE_POWER_ON_OFF
    tpd_hw_disable();
#endif
#endif 

}

/* Function to manage power-on resume */
void tpd_resume(struct early_suspend *h) 
{
#if 0
	uint8_t bfffer[2] = {0x01,0x02};
	TPD_DMESG("tpd_resume\n");	
	
#ifdef TPD_HAVE_POWER_ON_OFF
	tpd_hw_enable();
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
 	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	
	tpd_i2c_master_send(i2c_client,bfffer,2);
	msleep(10);
#endif
	tpd_i2c_master_rs_send(i2c_client,bfffer,1<<8|1);
	bfffer[1] &= ~0x0E;
	bfffer[1] |= 0x02;
	tpd_i2c_master_send(i2c_client,bfffer,2);
	msleep(5);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
	tpd_halt = 0;
#endif
}


static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "synaptics_v5", 
		.tpd_local_init = tpd_local_init,
		.suspend = tpd_suspend,
		.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		.tpd_have_button = 1,
#else
		.tpd_have_button = 0,
#endif		
};
#endif  //*v5_add end*/

static struct early_suspend synaptics_early_suspend_desc = {
        .level          = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1,
        .suspend        = synaptics_ts_early_suspend,
        .resume         = synaptics_ts_late_resume,
};

static int __devinit synaptics_ts_init(void)
{
#if 1 //*v5_add start */
	synaptics_wq = create_singlethread_workqueue("synaptics_wq");
#else
	synaptics_wq = create_rt_workqueue("synaptics_wq");
#endif //*v5_add end*/

   	printk("LGE: Justin Synaptics ts_init\n");
	if (!synaptics_wq)
		return -ENOMEM;
#if 1 //*v5_add start */
	/*v5 add start*/
	i2c_register_board_info(0, &i2c_Synaptics, 1);
	/*v5 add end*/\

	if(tpd_driver_add(&tpd_device_driver) < 0)
		printk("[chiwon] add tp mms136 driver failed\n");

//	register_early_suspend(&synaptics_early_suspend_desc);
	
    return 0;
#else	

	return i2c_add_driver(&synaptics_ts_driver);
	
#endif //*v5_add end*/
}

static void __exit synaptics_ts_exit(void)
{
#if 0 //*v5_add start */
i2c_del_driver(&synaptics_ts_driver);
#else
i2c_del_driver(&synaptics_ts_driver);
tpd_driver_remove(&tpd_device_driver);
#endif //*v5_add end*/

	if (synaptics_wq)
		destroy_workqueue(synaptics_wq);
}

module_init(synaptics_ts_init);
module_exit(synaptics_ts_exit);

MODULE_DESCRIPTION("Synaptics Touchscreen Driver");
MODULE_AUTHOR("Choi Daewan <ntdeaewan.choi@lge.com>");
MODULE_LICENSE("GPL");


