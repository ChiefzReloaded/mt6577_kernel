/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>

#include <linux/spinlock.h>
//#include <linux/watchdog.h>
#include <mach/mt_wdt.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>


#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>

#ifdef MT6575
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#endif



#include "tpd.h"
#include <cust_eint.h>

#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif

//#include "bin_tsp.h"
//#include "core_bin_V01.c"	//f/w binary file

//#include "mms100_ISP_download.c"
#include "mms100_ISP_download.h"


//if the TP has external power with GPIO pin,need define TPD_HAVE_POWER_ON_OFF in tpd_custom_mcs6024.h
#define TPD_HAVE_POWER_ON_OFF

#define MAX_POINT 5
#define TPD_POINT_INFO_LEN      6
//                                                               
#define MAX_TRANSACTION_LENGTH 32	// 8
//                                                                
#define I2C_DEVICE_ADDRESS_LEN 2
#define I2C_MASTER_CLOCK       400
#define TPD_POINT_INFO_REG_BASE       0x10
#define TS_READ_HW_VER_ADDR 0xF1 //Model Dependent
#define TS_READ_SW_VER_ADDR 0xF0 //Model Dependent
#define I2C_RETRY_CNT 5
#define MELFAS_DOWNLOAD 0
#define TPD_KEY_COUNT 4
#define TPD_HAVE_BUTTON 


#define MELFAS_FW_VERSION 0xF5 //Model Dependent

#define GPT_IRQEN       (APMCU_GPTIMER_BASE + 0x0000)


struct touch_info {
    int x[MAX_POINT], y[MAX_POINT];
    int p[MAX_POINT];
	int TouchpointFlag;
    int VirtualKeyFlag;
    int FingerCount;
};

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = {KEY_MENU,KEY_HOMEPAGE,KEY_BACK,KEY_SEARCH };
#endif

struct touch_info cinfo;
struct touch_info sinfo;

extern struct tpd_device *tpd;
extern int tpd_show_version;

static int boot_mode = 0;
static int tpd_flag = 0;
static int tpd_halt=0;
static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

static void tpd_eint_interrupt_handler(void);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);

static struct i2c_client *i2c_client = NULL;
static const struct i2c_device_id tpd_i2c_id[] = {{"mtk-tpd",0},{}};
static struct i2c_board_info __initdata i2c_MMS136={ I2C_BOARD_INFO("mtk-tpd", 0x48)};


struct i2c_driver tpd_i2c_driver = {                       
    .probe = tpd_i2c_probe,                                   
    .remove = tpd_i2c_remove,                           
    .detect = tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = tpd_i2c_id,                             
}; 

static void tpd_hw_enable(void){
    //CTP  LDO
    hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_3000, "TP");  
}

static void tpd_hw_disable(void){
     //CTP  LDO
    hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP"); 
}

static void tpd_i2c_master_send(struct i2c_client *client,const char *buf ,int count){
	client->addr = client->addr & I2C_MASK_FLAG;
	i2c_master_send(client,(const char*)buf,count);
}

static  int tpd_i2c_master_rs_send(struct i2c_client *client,const char *buf ,int count){
	int ret;

	client->addr = (client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_RS_FLAG; 
	ret = i2c_master_send(client,(const char*)buf,count);
	return ret;
}

static int tpd_i2c_read_bytes( struct i2c_client *client, u16 addr, u8 *rxbuf, int len ){
		int ret = 0;
		u16 left = len;
		u16 offset = 0;
		
		
		while ( left > 0 ){
			rxbuf[offset] = addr;
			client->addr = (client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG |I2C_RS_FLAG;
			
			if ( left > MAX_TRANSACTION_LENGTH ){
				ret = i2c_master_send(client, (rxbuf+offset), MAX_TRANSACTION_LENGTH<<8 |1);
				left -= MAX_TRANSACTION_LENGTH;
				offset += MAX_TRANSACTION_LENGTH;
			}
			else{
				ret = i2c_master_send(client, rxbuf, len<<8 |1);
				left=0;
			}		
			
			client->addr = (client->addr& I2C_MASK_FLAG);
			}

	
		if (ret < 0){
			TPD_DMESG("Error in %s, reg is 0x%02X.\n",__FUNCTION__,addr);
			return -EIO;
		}
		
		TPD_DMESG("ok in %s, reg is 0x%02X.\n",__FUNCTION__,addr);
		return 0;
	}

static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, "mtk-tpd");
    return 0;
}

static DEFINE_SPINLOCK(touch_reg_operation_spinlock);

static unsigned short g_wdt_original_data;

/* This function will disable watch dog */
void mtk_touch_wdt_disable(void)
        {
	unsigned short tmp;

	spin_lock(&touch_reg_operation_spinlock);

	tmp = DRV_Reg16(MTK_WDT_MODE);
	g_wdt_original_data = tmp;
	tmp |= MTK_WDT_MODE_KEY;
	tmp &= ~MTK_WDT_MODE_ENABLE;

	DRV_WriteReg16(MTK_WDT_MODE,tmp);

	spin_unlock(&touch_reg_operation_spinlock);

        }

void mtk_touch_wdt_restart(void)
        {
	unsigned short tmp;

    // Reset WatchDogTimer's counting value to time out value
    // ie., keepalive()

//    DRV_WriteReg16(MTK_WDT_RESTART, MTK_WDT_RESTART_KEY);

	spin_lock(&touch_reg_operation_spinlock);

	DRV_WriteReg16(MTK_WDT_MODE,g_wdt_original_data);
	tmp = DRV_Reg16(MTK_WDT_MODE);

	spin_unlock(&touch_reg_operation_spinlock);
	
        }

static DEFINE_SPINLOCK(touch_gpt_lock);
static unsigned long touch_gpt_flags;

const UINT32 touch_gpt_mask[GPT_TOTAL_COUNT+1] = {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0
};

void TOUCH_GPT_DisableIRQ(GPT_NUM numGPT)
            {
    if (numGPT == GPT1 || numGPT == GPT2)
        return;

    spin_lock_irqsave(&touch_gpt_lock, touch_gpt_flags);
    
    DRV_ClrReg32(GPT_IRQEN, touch_gpt_mask[numGPT]);

    spin_unlock_irqrestore(&touch_gpt_lock, touch_gpt_flags);
}

void TOUCH_GPT_EnableIRQ(GPT_NUM numGPT)
{
    if (numGPT == GPT1 || numGPT == GPT2)
        return;
 
    spin_lock_irqsave(&touch_gpt_lock, touch_gpt_flags);

    DRV_SetReg32(GPT_IRQEN, touch_gpt_mask[numGPT]);

    spin_unlock_irqrestore(&touch_gpt_lock, touch_gpt_flags);
}

static int mms_ts_fw_load(struct i2c_client *client)//void *context)
{
    //struct melfas_ts_data *info = context;
    //struct i2c_client *client = info->client;
    //struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    int ret = 0;
    int ver;
	int i;
    
    pr_info("[TSP] %s: \n", __func__);

   	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	mtk_touch_wdt_disable();
	TOUCH_GPT_DisableIRQ(GPT5);
	#if 1
		ret = mms100_ISP_download_binary_data(MELFAS_ISP_DOWNLOAD);
		if (ret)
			printk("<MELFAS> SET Download ISP Fail\n");		
	#else
		ret = isc_fw_download(client, MELFAS_binary, MELFAS_binary_nLength);
		
    if (ret < 0)
    {
        ret = isp_fw_download(MELFAS_MMS100_Initial_binary, MELFAS_MMS100_Initial_nLength);
        if (ret < 0)
        {
            pr_info("[TSP] error updating firmware to version 0x%02x \n", MELFAS_FW_VERSION);
        }
        else
        {
            ret = isc_fw_download(info, MELFAS_binary, MELFAS_binary_nLength);
        }
    }
	#endif
	TOUCH_GPT_EnableIRQ(GPT5);
	mtk_touch_wdt_restart();	
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);	
    return ret;
}


static int check_firmware(u8 *val){
    int ret = 0;
    uint8_t i = 0;
	uint8_t tp_config[4] = {0x00,0x00,0x00,0X00};
	
	for(i = 0; i < I2C_RETRY_CNT; i++){
		TPD_DMESG("[JHLog]read hw version\n");
		ret = tpd_i2c_read_bytes(i2c_client, TS_READ_HW_VER_ADDR, &val[0],1);
		TPD_DMESG("[JHLog]read sw version\n");
		ret = tpd_i2c_read_bytes(i2c_client, TS_READ_SW_VER_ADDR, &val[1],1);
		if(ret>=0){		 	
			TPD_DMESG("HW Revision[0x%02x] SW Version[0x%02x] \n", val[0], val[1]);
		}

//                                                                               
#if 0 // ori 
        ret = i2c_smbus_read_i2c_block_data(i2c_client, 0x1, 4, tp_config);
		
		if (ret >= 0){
			TPD_DMESG(" mms136 read  4 bytes from 0x01 is %x, %x, %x, %x\n", tp_config[0],tp_config[1],tp_config[2],tp_config[3]);
			TPD_DMESG(" mms136 mode control is %x\n",tp_config[0]);
			TPD_DMESG(" mms136 x resolution is %d\n",(((tp_config[1]&0x0f)<<8)|tp_config[2]));
			TPD_DMESG(" mms136 y resolution is %d\n",(((tp_config[1]&0xf0)<<4)|tp_config[3]));
			break; // i2c success
        }
		//                                                                                   
		if(ret <0)
			mms_ts_fw_load(i2c_client);
		//                                                                                   
#else // Touch activation code at first boot
              ret = tpd_i2c_read_bytes(i2c_client, 0x01, &tp_config[0],1);
              ret = tpd_i2c_read_bytes(i2c_client, 0x02, &tp_config[1],1);
              ret = tpd_i2c_read_bytes(i2c_client, 0x03, &tp_config[2],1);
              ret = tpd_i2c_read_bytes(i2c_client, 0x04, &tp_config[3],1);
		if (ret >= 0){
			TPD_DMESG(" mms136 read  4 bytes from 0x01 is %x, %x, %x, %x\n", tp_config[0],tp_config[1],tp_config[2],tp_config[3]);
			TPD_DMESG(" mms136 mode control is %x\n",tp_config[0]);
			TPD_DMESG(" mms136 x resolution is %d\n",(((tp_config[1]&0x0f)<<8)|tp_config[2]));
			TPD_DMESG(" mms136 y resolution is %d\n",(((tp_config[1]&0xf0)<<4)|tp_config[3]));
			break; // i2c success
        }
		//                                                                                   
		if(ret <0)
			mms_ts_fw_load(i2c_client);
		//                                                                                   
#endif
 //                                                                               

    }

	if (ret < 0){
        TPD_DMESG("[TSP] %s,%d: i2c read fail[%d] \n", __FUNCTION__, __LINE__, ret);
        return ret;
    }

}

static int firmware_update(struct i2c_client *client)
{
	int ret = 0;
	uint8_t fw_ver[2] = {0, };

	ret = check_firmware(fw_ver);
	if (ret < 0)
		TPD_DMESG("[TSP] check_firmware fail! [%d]", ret);
    else{
#if 1
        if (fw_ver[1] < MELFAS_FW_VERSION)
        {
            ret = mms_ts_fw_load(client);
        }
#endif
    }

    return ret;
}

//                                                                               
#if 0 // ori
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {             
	int err = 0,i;
	uint8_t Firmware_version[3] = {0x20,0x00,0x00};
	int ret = 0;

	i2c_client = client;

	#ifdef TPD_HAVE_POWER_ON_OFF
	tpd_hw_enable();

	//for power on sequence
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	
	#endif
	msleep(200);
	msleep(300);

#ifdef TPD_HAVE_BUTTON
	for(i =0; i < TPD_KEY_COUNT; i ++){
    	input_set_capability(tpd->dev,EV_KEY,tpd_keys_local[i]);
    }
#endif

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) { 
		err = PTR_ERR(thread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }    

	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	
	//disable_irq(client->irq);

//	ret = firmware_update(client);

	//enable_irq(client->irq);
	#ifdef TPD_HAVE_POWER_ON_OFF // to avoid hang up when booted up
	tpd_hw_disable();
	tpd_hw_enable();
    #endif	
   tpd_load_status = 1;

	return 0;
}
#else // Touch activation code at first boot
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {             
	int err = 0,i;
	uint8_t Firmware_version[3] = {0x20,0x00,0x00};
	int ret = 0;

	i2c_client = client;

	#if 1 //def TPD_HAVE_POWER_ON_OFF
	tpd_hw_disable();
       msleep(100);

       TPD_DMESG(TPD_DEVICE "[tpd_hw_enable]\n", err);
	tpd_hw_enable();
       msleep(200);

	//for power on sequence
       TPD_DMESG(TPD_DEVICE "[gpio_mode]\n", err);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	#endif
	
#ifdef TPD_HAVE_BUTTON
	for(i =0; i < TPD_KEY_COUNT; i ++){
    	input_set_capability(tpd->dev,EV_KEY,tpd_keys_local[i]);
    }
#endif

       TPD_DMESG(TPD_DEVICE "[kthread_run]\n", err);
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) { 
		err = PTR_ERR(thread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }    

       TPD_DMESG(TPD_DEVICE "[mt65xx_eint]\n", err);
	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	//disable_irq(client->irq);

	ret = firmware_update(client);

	//enable_irq(client->irq);

	tpd_load_status = 1;

	return 0;
}
#endif
//                                                                               

void tpd_eint_interrupt_handler(void){ 
	TPD_DEBUG_PRINT_INT; tpd_flag=1; 
	wake_up_interruptible(&waiter);
} 
static int tpd_i2c_remove(struct i2c_client *client) {return 0;}

void tpd_down(int x, int y, int p) {	
	input_report_abs(tpd->dev, ABS_PRESSURE, p);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, p);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	TPD_DMESG("D[%4d %4d %4d] ", x, y, p);
	input_mt_sync(tpd->dev);
	TPD_DOWN_DEBUG_TRACK(x,y);

	TPD_EM_PRINT(x, y, x, y, p, 1);
}

int tpd_up(int x, int y) {
	input_report_abs(tpd->dev, ABS_PRESSURE, 0);
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	TPD_DMESG("U[%4d %4d %4d] ", x, y, 0);
	input_mt_sync(tpd->dev);
	TPD_UP_DEBUG_TRACK(x,y);
	TPD_EM_PRINT(x, y, x, y, 0, 0);
	return 1;
}


int tpd_gettouchinfo(struct touch_info *cinfo) {
	uint8_t pkt_size= 0x0f;
	static u8 buffer[TPD_POINT_INFO_LEN*MAX_POINT];	
//                                                                    
// Desc : patch for no touch activation when 6 fingers touch on
	static u8 buffer1[TPD_POINT_INFO_LEN*MAX_POINT];
//                                           
	int finger_num = 0;
	uint8_t status = 0x10;
	uint8_t i;
	uint8_t key_index;
	int touch_key_state;

	sinfo.VirtualKeyFlag = cinfo->VirtualKeyFlag;
	cinfo->TouchpointFlag = 0;
	cinfo->VirtualKeyFlag= 0;

   i2c_smbus_read_i2c_block_data(i2c_client, pkt_size, 1, &pkt_size);
	finger_num = pkt_size/6;

//                                                                          
#if 0
	if(finger_num > MAX_POINT) {
		TPD_DMESG("finger_num is %d, abnormal!\n",finger_num);
		return -1;
		}
	TPD_DMESG("pkt_size is %d, finger_num is %d!\n",pkt_size,finger_num);
	
    cinfo->FingerCount = finger_num;

   i2c_smbus_read_i2c_block_data(i2c_client, TPD_POINT_INFO_REG_BASE, (TPD_POINT_INFO_LEN*finger_num), buffer);
#else // Desc : patch for no touch activation when 6 fingers touch on
	TPD_DMESG("pkt_size is %d, finger_num is %d!\n",pkt_size,finger_num);
    cinfo->FingerCount = finger_num;

	if(finger_num > MAX_POINT) {

		i2c_smbus_read_i2c_block_data(i2c_client, TPD_POINT_INFO_REG_BASE, (TPD_POINT_INFO_LEN*MAX_POINT), buffer);

		// read remain i2c, so touch will clear INT
		i2c_smbus_read_i2c_block_data(i2c_client, TPD_POINT_INFO_REG_BASE, (TPD_POINT_INFO_LEN*(finger_num-MAX_POINT)), buffer1);

	}
	else{
		//                                                               
		#if 0
		tpd_i2c_read_bytes( i2c_client, TPD_POINT_INFO_REG_BASE, buffer, (TPD_POINT_INFO_LEN*finger_num));
		#else
		i2c_smbus_read_i2c_block_data(i2c_client, TPD_POINT_INFO_REG_BASE, (TPD_POINT_INFO_LEN*finger_num), buffer);
		#endif
		//                                                               
	}
#endif
//                                           
	for(i=0; i<finger_num; i++){

//                                                           
		TPD_DMESG("finger_num %d info:\n",i);
//                                           
		TPD_DMESG("%0x, %0x, %0x, %0x, %0x,%0x\n",buffer[i],buffer[i+1],buffer[i+2],buffer[i+3],buffer[4],buffer[5]);
		
		if((buffer[i*6]&0x60)>>5 == 1){//touch event
			cinfo->x[i]=((buffer[i*6+1]&0x0f)<<8)|buffer[i*6+2];	
			cinfo->y[i]=((buffer[i*6+1]&0xf0)<<4)|buffer[i*6+3];	
			cinfo->p[i]=buffer[i*6+5];
			if((buffer[i*6]&0x80)>> 7 ==1) //touch on
			     cinfo->TouchpointFlag |= ( 1 << i );
			TPD_DMESG("touch event info: \n");
			TPD_DMESG("fingure %d, x is %d, y is %d, p is %d\n",i,cinfo->x[i],cinfo->y[i],cinfo->p[i]);
		}else if((buffer[i*6]&0x60)>>5 == 2){ //key event
		    key_index = buffer[i*6]&0x0f;
			TPD_DMESG("key_index is %d\n",key_index);
			if( (buffer[i] & 0x80)>>7)
				cinfo->VirtualKeyFlag = ( 1 << (key_index-1));
			TPD_DMESG("current VirtualKeyFlag is %d\n",cinfo->VirtualKeyFlag);
		}
		}
	
	return 0;	
}


static int touch_event_handler(void *unused) {
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

    int index;

    sched_setscheduler(current, SCHED_RR, &param);
	
	TPD_DMESG("touch_event_handler\n");

    do {
        set_current_state(TASK_INTERRUPTIBLE);
        if (!kthread_should_stop()) {
            TPD_DEBUG_CHECK_NO_RESPONSE;
            do {
				while (tpd_halt) {tpd_flag = 0;sinfo.TouchpointFlag=0; msleep(20);}
               		wait_event_interruptible(waiter,tpd_flag!=0);
					tpd_flag = 0;
            } while(0);

            TPD_DEBUG_SET_TIME;
        }

		set_current_state(TASK_RUNNING);

		if(tpd_gettouchinfo(&cinfo))
		 	continue; 
	
		TPD_DMESG("sinfo.TouchpointFlag = %d\n",sinfo.TouchpointFlag);
		TPD_DMESG("cinfo.TouchpointFlag = %d\n",cinfo.TouchpointFlag);
		TPD_DMESG("sinfo.VirtualKeyFlag = %d\n",sinfo.VirtualKeyFlag);
		TPD_DMESG("cinfo.VirtualKeyFlag = %d\n",cinfo.VirtualKeyFlag);

       #ifdef TPD_HAVE_BUTTON
		if((sinfo.VirtualKeyFlag!= 0) || (cinfo.VirtualKeyFlag!= 0)){
            for(index = 0; index < TPD_KEY_COUNT; index++){             
            	input_report_key(tpd->dev, tpd_keys_local[index],!!(cinfo.VirtualKeyFlag &(0x01<<index)));
				TPD_DMESG("input_report_key, code is %d, and state is %d\n",tpd_keys_local[index],!!(cinfo.VirtualKeyFlag &(0x01<<index)));				
			}
        }
		#endif
		for(index = 0;index<cinfo.FingerCount;index++){
			if(cinfo.TouchpointFlag&(1<<index)){
				tpd_down(cinfo.x[index],cinfo.y[index], cinfo.p[index]);
				sinfo.x[index] = cinfo.x[index];
				sinfo.y[index] = cinfo.y[index];
				sinfo.p[index] = cinfo.p[index];					
				sinfo.TouchpointFlag |=(1<<index);
				}
			else{
				if(sinfo.TouchpointFlag&(1<<index)){
					tpd_up(sinfo.x[index],sinfo.y[index]);
					sinfo.TouchpointFlag &=~(1<<index);
					}
				}
		 	}
		input_sync(tpd->dev);
		
    } while (!kthread_should_stop());
    return 0;
}


int tpd_local_init(void) {
	boot_mode = get_boot_mode();
	
    // Software reset mode will be treated as normal boot
    if(boot_mode==3) boot_mode = NORMAL_BOOT;
	if(i2c_add_driver(&tpd_i2c_driver)!=0) {
      TPD_DMESG("unable to add i2c driver.\n");
      return -1;
    }
  
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

	return 0;
}

/* Function to manage low power suspend */
void tpd_suspend(struct early_suspend *h)
{
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

}

/* Function to manage power-on resume */
void tpd_resume(struct early_suspend *h) 
{
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
}

static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "mms136",
		.tpd_local_init = tpd_local_init,
		.suspend = tpd_suspend,
		.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		.tpd_have_button = 1,
#else
		.tpd_have_button = 0,
#endif		
};

/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
	TPD_DMESG("MediaTek mms136 touch panel driver init\n");
	i2c_register_board_info(1, &i2c_MMS136, 1);

	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add tp mms136 driver failed\n");
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
    TPD_DMESG("MediaTek mms136 touch panel driver exit\n");
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

