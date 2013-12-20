/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
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
 * applicable license agreements with MediaTek Inc. */


/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 * Zhijie Yuan (MTK70715)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/io.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "hi351yuv_Sensor.h"
#include "hi351yuv_Camera_Sensor_para.h"
#include "hi351yuv_CameraCustomized.h"

#define HI351YUV_DEBUG
#ifdef HI351YUV_DEBUG
#define SENSORDB printk
#else
#define SENSORDB(x,...)
#endif

struct
{
  kal_bool    NightMode;
  kal_bool    VidoeMode;
  kal_bool    IsPreview;
  kal_uint8   ZoomFactor; /* Zoom Index */
  kal_uint16  Banding;
  kal_uint32  PvShutter;
  kal_uint32  PVDummyPixels;
  kal_uint32  PVDummyLines;
  kal_uint32  CPDummyPixels;
  kal_uint32  CPDummyLines;
  kal_uint32  PvOpClk;
  kal_uint32  CapOpClk;
  
  /* Video frame rate 300 means 30.0fps. Unit Multiple 10. */
  kal_uint32  MaxFrameRate; 
  kal_uint32  MiniFrameRate; 
  /* Sensor Register backup. */
  kal_uint8   VDOCTL2; /* P0.0x11. */
  kal_uint8   AECTL1;  /* PC4.0x10. */
  kal_uint8   AWBCTL1; /* PC5.0x10. */
  kal_uint8   MODEFZY1; /* P20.0x10. */
} HI351Sensor;

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

#define Sleep(ms) mdelay(ms)

static void HI351SetAeMode(kal_bool AeEnable);

kal_uint16 HI351WriteCmosSensor(kal_uint32 Addr, kal_uint32 Para)
{
  char pSendCmd[2] = {(char)(Addr & 0xFF) ,(char)(Para & 0xFF)};
  
  //SENSORDB("[HI351]HI351WriteCmosSensor,Addr:%x;Para:%x \n",Addr,Para);
  iWriteRegI2C(pSendCmd , 2,HI351_WRITE_ID);
}

kal_uint16 HI351ReadCmosSensor(kal_uint32 Addr)
{
  char pGetByte=0;
  char pSendCmd = (char)(Addr & 0xFF);
  
  iReadRegI2C(&pSendCmd , 1, &pGetByte,1,HI351_WRITE_ID);
  //SENSORDB("[HI351]HI351ReadCmosSensor,Addr:%x;pGetByte:%x \n",Addr,pGetByte);  
  return pGetByte;
}

__inline void HI351SetPage(kal_uint8 Page)
{
  HI351WriteCmosSensor(0x03, Page);
}
void HI351InitSetting(void)
    {
            ///////////////////////////////////////////
            // 0 Page PLL setting
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x07, 0x25); //24/(5+1) = 4Mhz
        HI351WriteCmosSensor(0x08, 0x48); // 72Mhz
        HI351WriteCmosSensor(0x09, 0x82);
        HI351WriteCmosSensor(0x07, 0xa5);
        HI351WriteCmosSensor(0x07, 0xa5);
        HI351WriteCmosSensor(0x09, 0xa2);
            
        HI351WriteCmosSensor(0x0A, 0x01); // MCU hardware reset
        HI351WriteCmosSensor(0x0A, 0x00);
        HI351WriteCmosSensor(0x0A, 0x01);
        HI351WriteCmosSensor(0x0A, 0x00);
            
            ///////////////////////////////////////////
            // 20 Page OTP/ROM LSC download select setting
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x20);  
        HI351WriteCmosSensor(0x3a, 0x00); 
        HI351WriteCmosSensor(0x3b, 0x00); 
        HI351WriteCmosSensor(0x3c, 0x00); 
            
            
            ///////////////////////////////////////////
            // 0 Page 
            ///////////////////////////////////////////        
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x50, 0x01);  
        //HI351WriteCmosSensor(0x51, 0x20);
        HI351WriteCmosSensor(0x51, 0x90); //will
            
        HI351WriteCmosSensor(0x52, 0x00); //VBLANK = 33
        HI351WriteCmosSensor(0x53, 0x32);
            
            ///////////////////////////////////////////
            // 30 Page MCU reset, enable setting
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x30);
        HI351WriteCmosSensor(0x30, 0x86);
        HI351WriteCmosSensor(0x31, 0x00);
        HI351WriteCmosSensor(0x32, 0x0c);
        HI351WriteCmosSensor(0xe0, 0x02);// CLK INVERSION
        HI351WriteCmosSensor(0x24, 0x02);// PCON WRITE SET
        HI351WriteCmosSensor(0x25, 0x1e);// PCON WAKE NORMAL
        HI351WriteCmosSensor(0x10, 0x81); // mcu reset high
        HI351WriteCmosSensor(0x10, 0x89); // mcu enable high
        HI351WriteCmosSensor(0x11, 0x08); // xdata memory reset high
        HI351WriteCmosSensor(0x11, 0x00); // xdata memory reset low
            
            ///////////////////////////////////////////
            // 7 Page OTP/ROM color ratio download select setting
            /////////////////////////////////////////// 
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x12, 0x07); 
        HI351WriteCmosSensor(0x40, 0x0E); 
        HI351WriteCmosSensor(0x47, 0x03); 
        HI351WriteCmosSensor(0x2e, 0x00);               
        HI351WriteCmosSensor(0x2f, 0x20); 
        HI351WriteCmosSensor(0x30, 0x00);   
        HI351WriteCmosSensor(0x31, 0xD6); 
        HI351WriteCmosSensor(0x32, 0x00); 
        HI351WriteCmosSensor(0x33, 0xFF); 
        HI351WriteCmosSensor(0x10, 0x02);
            
        HI351WriteCmosSensor(0x03, 0x07); //delay
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x03, 0x07);
            
        HI351WriteCmosSensor(0x2e, 0x03); // color ratio reg down
        HI351WriteCmosSensor(0x2f, 0x20);
        HI351WriteCmosSensor(0x30, 0x20);
        HI351WriteCmosSensor(0x31, 0xa6);
        HI351WriteCmosSensor(0x32, 0x01);
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x10, 0x02);
            
            
        HI351WriteCmosSensor(0x03, 0x07); //delay
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x03, 0x07);
            
        HI351WriteCmosSensor(0x12, 0x00); 
        HI351WriteCmosSensor(0x98, 0x00); 
        HI351WriteCmosSensor(0x97, 0x01); 
                       
        HI351WriteCmosSensor(0x93, 0x54); 
        HI351WriteCmosSensor(0x95, 0x11);
            ///////////////////////////////////////////
            // 30 Page MCU reset, enable setting
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x30);
        HI351WriteCmosSensor(0x10, 0x09); // mcu reset low  = mcu start!!
            
            ///////////////////////////////////////////
            // 0 Page 
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x0B, 0x02); //PLL lock time
        HI351WriteCmosSensor(0x10, 0x00);
        HI351WriteCmosSensor(0x11, 0x80); // STEVE 0frame skip, XY flip 
        HI351WriteCmosSensor(0x13, 0x80);
        HI351WriteCmosSensor(0x14, 0x70);
        HI351WriteCmosSensor(0x15, 0x03);
        HI351WriteCmosSensor(0x17, 0x04); //Parallel, MIPI : 04, JPEG : 0c
            
        HI351WriteCmosSensor(0x20, 0x00); //Start Width
        HI351WriteCmosSensor(0x21, 0x01);
        HI351WriteCmosSensor(0x22, 0x00); //Start Height
        HI351WriteCmosSensor(0x23, 0x03);
            
        HI351WriteCmosSensor(0x24, 0x06); //Widht Size
        HI351WriteCmosSensor(0x25, 0x00);
        HI351WriteCmosSensor(0x26, 0x08); //Height Size
        HI351WriteCmosSensor(0x27, 0x00);
            
            //BLC
        HI351WriteCmosSensor(0x80, 0x02);
        HI351WriteCmosSensor(0x81, 0x87);
        HI351WriteCmosSensor(0x82, 0x28);
        HI351WriteCmosSensor(0x83, 0x08);
        HI351WriteCmosSensor(0x84, 0x8c);
        HI351WriteCmosSensor(0x85, 0x0c);//blc on
        HI351WriteCmosSensor(0x86, 0x00);
        HI351WriteCmosSensor(0x87, 0x00);
        HI351WriteCmosSensor(0x88, 0x98);
        HI351WriteCmosSensor(0x89, 0x10);
        HI351WriteCmosSensor(0x8a, 0x80);
        HI351WriteCmosSensor(0x8b, 0x00);
        HI351WriteCmosSensor(0x8e, 0x80);
        HI351WriteCmosSensor(0x8f, 0x0f);
        HI351WriteCmosSensor(0x90, 0x0c); //BLC_TIME_TH_ON
        HI351WriteCmosSensor(0x91, 0x0c); //BLC_TIME_TH_OFF 
        HI351WriteCmosSensor(0x92, 0xC8); //BLC_AG_TH_ON  // STEVE AGC 0xd0
        HI351WriteCmosSensor(0x93, 0xC0); //BLC_AG_TH_OFF // STEVE AGC 0xd0
        HI351WriteCmosSensor(0x96, 0xfe); //BLC_OUT_TH
        HI351WriteCmosSensor(0x97, 0xfd); //BLC_OUT_TH
        HI351WriteCmosSensor(0x98, 0x20);
            
            
            
        HI351WriteCmosSensor(0xa0, 0x85); //odd_adj_normal       
        HI351WriteCmosSensor(0xa1, 0x85); //out r
        HI351WriteCmosSensor(0xa2, 0x85); //in
        HI351WriteCmosSensor(0xa3, 0x87); //dark
        HI351WriteCmosSensor(0xa4, 0x85); //even_adj_normal       
        HI351WriteCmosSensor(0xa5, 0x85); //out b 
        HI351WriteCmosSensor(0xa6, 0x85); //in
        HI351WriteCmosSensor(0xa7, 0x87); //dark     
            
            
        HI351WriteCmosSensor(0xbb, 0x20); 
            ///////////////////////////////////////////
            // 2 Page                                  
            ///////////////////////////////////////////
            
        HI351WriteCmosSensor(0x03, 0x02);
        HI351WriteCmosSensor(0x10, 0x00);
        HI351WriteCmosSensor(0x13, 0x00);
        HI351WriteCmosSensor(0x14, 0x00);
        HI351WriteCmosSensor(0x15, 0x08);
        HI351WriteCmosSensor(0x1a, 0x00);//ncp adaptive off
        HI351WriteCmosSensor(0x1b, 0x00);
        HI351WriteCmosSensor(0x1c, 0xc0);
        HI351WriteCmosSensor(0x1d, 0x00);//MCU update bit[4]
        HI351WriteCmosSensor(0x20, 0x44);
        HI351WriteCmosSensor(0x21, 0x02);
        HI351WriteCmosSensor(0x22, 0x22);
        HI351WriteCmosSensor(0x23, 0x30);//clamp on 10 -30
        HI351WriteCmosSensor(0x24, 0x77);
        HI351WriteCmosSensor(0x2b, 0x00);
        HI351WriteCmosSensor(0x2c, 0x0C);
        HI351WriteCmosSensor(0x2d, 0x80);
        HI351WriteCmosSensor(0x2e, 0x00);
        HI351WriteCmosSensor(0x2f, 0x00);
        HI351WriteCmosSensor(0x30, 0x00);
        HI351WriteCmosSensor(0x31, 0xf0);
        HI351WriteCmosSensor(0x32, 0x22);
        HI351WriteCmosSensor(0x33, 0x42); // STEVE01 0x02 -)0x42 DV3 fix 
        HI351WriteCmosSensor(0x34, 0x30);
        HI351WriteCmosSensor(0x35, 0x00);
        HI351WriteCmosSensor(0x36, 0x08);
        HI351WriteCmosSensor(0x37, 0x40); // STEVE01 0x20 -) 0x40 DV3 fix
        HI351WriteCmosSensor(0x38, 0x14);
        HI351WriteCmosSensor(0x39, 0x02); 
        HI351WriteCmosSensor(0x3a, 0x00);
            
        HI351WriteCmosSensor(0x3d, 0x70);
        HI351WriteCmosSensor(0x3e, 0x04);
        HI351WriteCmosSensor(0x3f, 0x00);
        HI351WriteCmosSensor(0x40, 0x01);
        HI351WriteCmosSensor(0x41, 0x8a);
        HI351WriteCmosSensor(0x42, 0x00);
        HI351WriteCmosSensor(0x43, 0x25);
        HI351WriteCmosSensor(0x44, 0x00);
        HI351WriteCmosSensor(0x46, 0x00);
        HI351WriteCmosSensor(0x47, 0x00);
        HI351WriteCmosSensor(0x48, 0x3C);
        HI351WriteCmosSensor(0x49, 0x10);
        HI351WriteCmosSensor(0x4a, 0x00);
        HI351WriteCmosSensor(0x4b, 0x10);
        HI351WriteCmosSensor(0x4c, 0x08);
        HI351WriteCmosSensor(0x4d, 0x70);
        HI351WriteCmosSensor(0x4e, 0x04);
        HI351WriteCmosSensor(0x4f, 0x38);
        HI351WriteCmosSensor(0x50, 0xa0);
        HI351WriteCmosSensor(0x51, 0x00);
        HI351WriteCmosSensor(0x52, 0x70);  
        HI351WriteCmosSensor(0x53, 0x00);
        HI351WriteCmosSensor(0x54, 0xc0);
        HI351WriteCmosSensor(0x55, 0x40);
        HI351WriteCmosSensor(0x56, 0x11);
        HI351WriteCmosSensor(0x57, 0x00);
        HI351WriteCmosSensor(0x58, 0x10);
        HI351WriteCmosSensor(0x59, 0x0E);
        HI351WriteCmosSensor(0x5a, 0x00);
        HI351WriteCmosSensor(0x5b, 0x00);
        HI351WriteCmosSensor(0x5c, 0x00);
        HI351WriteCmosSensor(0x5d, 0x00);
        HI351WriteCmosSensor(0x60, 0x04);
        HI351WriteCmosSensor(0x61, 0xe2);
        HI351WriteCmosSensor(0x62, 0x00);
        HI351WriteCmosSensor(0x63, 0xc8);
        HI351WriteCmosSensor(0x64, 0x00);
        HI351WriteCmosSensor(0x65, 0x00);
        HI351WriteCmosSensor(0x66, 0x00);
        HI351WriteCmosSensor(0x67, 0x3f);
        HI351WriteCmosSensor(0x68, 0x3f);
        HI351WriteCmosSensor(0x69, 0x3f);
        HI351WriteCmosSensor(0x6a, 0x04);
        HI351WriteCmosSensor(0x6b, 0x38);
        HI351WriteCmosSensor(0x6c, 0x00);
        HI351WriteCmosSensor(0x6d, 0x00);
        HI351WriteCmosSensor(0x6e, 0x00);
        HI351WriteCmosSensor(0x6f, 0x00);
        HI351WriteCmosSensor(0x70, 0x00);
        HI351WriteCmosSensor(0x71, 0x50);
        HI351WriteCmosSensor(0x72, 0x05);
        HI351WriteCmosSensor(0x73, 0xa5);
        HI351WriteCmosSensor(0x74, 0x00);
        HI351WriteCmosSensor(0x75, 0x50);
        HI351WriteCmosSensor(0x76, 0x02);
        HI351WriteCmosSensor(0x77, 0xfa);
        HI351WriteCmosSensor(0x78, 0x01);
        HI351WriteCmosSensor(0x79, 0xb4);
        HI351WriteCmosSensor(0x7a, 0x01);
        HI351WriteCmosSensor(0x7b, 0xb8);
        HI351WriteCmosSensor(0x7c, 0x00);
        HI351WriteCmosSensor(0x7d, 0x00);
        HI351WriteCmosSensor(0x7e, 0x00);
        HI351WriteCmosSensor(0x7f, 0x00);
        HI351WriteCmosSensor(0xa0, 0x00);
        HI351WriteCmosSensor(0xa1, 0xEB);
        HI351WriteCmosSensor(0xa2, 0x02);
        HI351WriteCmosSensor(0xa3, 0x2D);
        HI351WriteCmosSensor(0xa4, 0x02);
        HI351WriteCmosSensor(0xa5, 0xB9);
        HI351WriteCmosSensor(0xa6, 0x05);
        HI351WriteCmosSensor(0xa7, 0xED);
        HI351WriteCmosSensor(0xa8, 0x00);
        HI351WriteCmosSensor(0xa9, 0xEB);
        HI351WriteCmosSensor(0xaa, 0x01);
        HI351WriteCmosSensor(0xab, 0xED);
        HI351WriteCmosSensor(0xac, 0x02);
        HI351WriteCmosSensor(0xad, 0x79);
        HI351WriteCmosSensor(0xae, 0x04);
        HI351WriteCmosSensor(0xaf, 0x2D);
        HI351WriteCmosSensor(0xb0, 0x00);
        HI351WriteCmosSensor(0xb1, 0x56);
        HI351WriteCmosSensor(0xb2, 0x01);
        HI351WriteCmosSensor(0xb3, 0x08);
        HI351WriteCmosSensor(0xb4, 0x00);
        HI351WriteCmosSensor(0xb5, 0x2B);
        HI351WriteCmosSensor(0xb6, 0x03);
        HI351WriteCmosSensor(0xb7, 0x2B);
        HI351WriteCmosSensor(0xb8, 0x00);
        HI351WriteCmosSensor(0xb9, 0x56);
        HI351WriteCmosSensor(0xba, 0x00);
        HI351WriteCmosSensor(0xbb, 0xC8);
        HI351WriteCmosSensor(0xbc, 0x00);
        HI351WriteCmosSensor(0xbd, 0x2B);
        HI351WriteCmosSensor(0xbe, 0x01);
        HI351WriteCmosSensor(0xbf, 0xAB);
        HI351WriteCmosSensor(0xc0, 0x00);
        HI351WriteCmosSensor(0xc1, 0x54);
        HI351WriteCmosSensor(0xc2, 0x01);
        HI351WriteCmosSensor(0xc3, 0x0A);
        HI351WriteCmosSensor(0xc4, 0x00);
        HI351WriteCmosSensor(0xc5, 0x29);
        HI351WriteCmosSensor(0xc6, 0x03);
        HI351WriteCmosSensor(0xc7, 0x2D);
        HI351WriteCmosSensor(0xc8, 0x00);
        HI351WriteCmosSensor(0xc9, 0x54);
        HI351WriteCmosSensor(0xca, 0x00);
        HI351WriteCmosSensor(0xcb, 0xCA);
        HI351WriteCmosSensor(0xcc, 0x00);
        HI351WriteCmosSensor(0xcd, 0x29);
        HI351WriteCmosSensor(0xce, 0x01);
        HI351WriteCmosSensor(0xcf, 0xAD);
        HI351WriteCmosSensor(0xd0, 0x10);
        HI351WriteCmosSensor(0xd1, 0x14);
        HI351WriteCmosSensor(0xd2, 0x20);
        HI351WriteCmosSensor(0xd3, 0x00);
        HI351WriteCmosSensor(0xd4, 0x0C);//DCDC_TIME_TH_ON // STEVE
        HI351WriteCmosSensor(0xd5, 0x0C);//DCDC_TIME_TH_OFF // STEVE
        HI351WriteCmosSensor(0xd6, 0xC8);//DCDC_AG_TH_ON     // STEVE AGC 0xD0
        HI351WriteCmosSensor(0xd7, 0xC0);//DCDC_AG_TH_OFF  // STEVE AGC 0xD0
        HI351WriteCmosSensor(0xE0, 0xf0);//ncp adaptive
        HI351WriteCmosSensor(0xE1, 0xf0);//ncp adaptive
        HI351WriteCmosSensor(0xE2, 0xf0);//ncp adaptive
        HI351WriteCmosSensor(0xE3, 0xf0);//ncp adaptive
        HI351WriteCmosSensor(0xE4, 0xd0);//ncp adaptive
        HI351WriteCmosSensor(0xE5, 0x00);//ncp adaptive
        HI351WriteCmosSensor(0xE6, 0x00);
        HI351WriteCmosSensor(0xE7, 0x00);
        HI351WriteCmosSensor(0xE8, 0x00);
        HI351WriteCmosSensor(0xE9, 0x00);
        HI351WriteCmosSensor(0xEA, 0x15);
        HI351WriteCmosSensor(0xEB, 0x15);
        HI351WriteCmosSensor(0xEC, 0x15);
        HI351WriteCmosSensor(0xED, 0x05);
        HI351WriteCmosSensor(0xEE, 0x05);
        HI351WriteCmosSensor(0xEF, 0x65);
        HI351WriteCmosSensor(0xF0, 0x0c);
        HI351WriteCmosSensor(0xF3, 0x05);
        HI351WriteCmosSensor(0xF4, 0x0a);
        HI351WriteCmosSensor(0xF5, 0x05);
        HI351WriteCmosSensor(0xF6, 0x05);
        HI351WriteCmosSensor(0xF7, 0x15);
        HI351WriteCmosSensor(0xF8, 0x15);
        HI351WriteCmosSensor(0xF9, 0x15);
        HI351WriteCmosSensor(0xFA, 0x15);
        HI351WriteCmosSensor(0xFB, 0x15);
        HI351WriteCmosSensor(0xFC, 0x55);
        HI351WriteCmosSensor(0xFD, 0x55);
        HI351WriteCmosSensor(0xFE, 0x05);
        HI351WriteCmosSensor(0xFF, 0x00);
            ///////////////////////////////////////////
            //3Page
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x03);
        HI351WriteCmosSensor(0x10, 0x00);
        HI351WriteCmosSensor(0x11, 0x64);
        HI351WriteCmosSensor(0x12, 0x00);
        HI351WriteCmosSensor(0x13, 0x32);
        HI351WriteCmosSensor(0x14, 0x02);
        HI351WriteCmosSensor(0x15, 0x51);
        HI351WriteCmosSensor(0x16, 0x02);
        HI351WriteCmosSensor(0x17, 0x59);
        HI351WriteCmosSensor(0x18, 0x00);
        HI351WriteCmosSensor(0x19, 0x97);
        HI351WriteCmosSensor(0x1a, 0x01);
        HI351WriteCmosSensor(0x1b, 0x7C);
        HI351WriteCmosSensor(0x1c, 0x00);
        HI351WriteCmosSensor(0x1d, 0x97);
        HI351WriteCmosSensor(0x1e, 0x01);
        HI351WriteCmosSensor(0x1f, 0x7C);
        HI351WriteCmosSensor(0x20, 0x00);
        HI351WriteCmosSensor(0x21, 0x97);
        HI351WriteCmosSensor(0x22, 0x00);
        HI351WriteCmosSensor(0x23, 0xe3); //cds 2 off time sunspot
        HI351WriteCmosSensor(0x24, 0x00);
        HI351WriteCmosSensor(0x25, 0x97);
        HI351WriteCmosSensor(0x26, 0x00);
        HI351WriteCmosSensor(0x27, 0xe3); //cds 2 off time  sunspot
                       
        HI351WriteCmosSensor(0x28, 0x00);
        HI351WriteCmosSensor(0x29, 0x97);
        HI351WriteCmosSensor(0x2a, 0x00);
        HI351WriteCmosSensor(0x2b, 0xE6);
        HI351WriteCmosSensor(0x2c, 0x00);
        HI351WriteCmosSensor(0x2d, 0x97);
        HI351WriteCmosSensor(0x2e, 0x00);
        HI351WriteCmosSensor(0x2f, 0xE6);
        HI351WriteCmosSensor(0x30, 0x00);
        HI351WriteCmosSensor(0x31, 0x0a);
        HI351WriteCmosSensor(0x32, 0x03);
        HI351WriteCmosSensor(0x33, 0x31);
        HI351WriteCmosSensor(0x34, 0x00);
        HI351WriteCmosSensor(0x35, 0x0a);
        HI351WriteCmosSensor(0x36, 0x03);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x38, 0x00);
        HI351WriteCmosSensor(0x39, 0x0A);
        HI351WriteCmosSensor(0x3a, 0x01);
        HI351WriteCmosSensor(0x3b, 0xB0);
        HI351WriteCmosSensor(0x3c, 0x00);
        HI351WriteCmosSensor(0x3d, 0x0A);
        HI351WriteCmosSensor(0x3e, 0x01);
        HI351WriteCmosSensor(0x3f, 0xB0);
        HI351WriteCmosSensor(0x40, 0x00);
        HI351WriteCmosSensor(0x41, 0x04);
        HI351WriteCmosSensor(0x42, 0x00);
        HI351WriteCmosSensor(0x43, 0x1c);
        HI351WriteCmosSensor(0x44, 0x00);
        HI351WriteCmosSensor(0x45, 0x02);
        HI351WriteCmosSensor(0x46, 0x00);
        HI351WriteCmosSensor(0x47, 0x34);
        HI351WriteCmosSensor(0x48, 0x00);
        HI351WriteCmosSensor(0x49, 0x06);
        HI351WriteCmosSensor(0x4a, 0x00);
        HI351WriteCmosSensor(0x4b, 0x1a);
        HI351WriteCmosSensor(0x4c, 0x00);
        HI351WriteCmosSensor(0x4d, 0x06);
        HI351WriteCmosSensor(0x4e, 0x00);
        HI351WriteCmosSensor(0x4f, 0x1a);
        HI351WriteCmosSensor(0x50, 0x00);
        HI351WriteCmosSensor(0x51, 0x08);
        HI351WriteCmosSensor(0x52, 0x00);
        HI351WriteCmosSensor(0x53, 0x18);
        HI351WriteCmosSensor(0x54, 0x00);
        HI351WriteCmosSensor(0x55, 0x08);
        HI351WriteCmosSensor(0x56, 0x00);
        HI351WriteCmosSensor(0x57, 0x18);
        HI351WriteCmosSensor(0x58, 0x00);
        HI351WriteCmosSensor(0x59, 0x08);
        HI351WriteCmosSensor(0x5A, 0x00);
        HI351WriteCmosSensor(0x5b, 0x18);
        HI351WriteCmosSensor(0x5c, 0x00);
        HI351WriteCmosSensor(0x5d, 0x06);
        HI351WriteCmosSensor(0x5e, 0x00);
        HI351WriteCmosSensor(0x5f, 0x1c);
        HI351WriteCmosSensor(0x60, 0x00);
        HI351WriteCmosSensor(0x61, 0x00);
        HI351WriteCmosSensor(0x62, 0x00);
        HI351WriteCmosSensor(0x63, 0x00);
        HI351WriteCmosSensor(0x64, 0x00);
        HI351WriteCmosSensor(0x65, 0x00);
        HI351WriteCmosSensor(0x66, 0x00);
        HI351WriteCmosSensor(0x67, 0x00);
        HI351WriteCmosSensor(0x68, 0x00);
        HI351WriteCmosSensor(0x69, 0x02);
        HI351WriteCmosSensor(0x6A, 0x00);
        HI351WriteCmosSensor(0x6B, 0x1e);
        HI351WriteCmosSensor(0x6C, 0x00);
        HI351WriteCmosSensor(0x6D, 0x00);
        HI351WriteCmosSensor(0x6E, 0x00);
        HI351WriteCmosSensor(0x6F, 0x00);
        HI351WriteCmosSensor(0x70, 0x00);
        HI351WriteCmosSensor(0x71, 0x66);
        HI351WriteCmosSensor(0x72, 0x01);
        HI351WriteCmosSensor(0x73, 0x86);
        HI351WriteCmosSensor(0x74, 0x00);
        HI351WriteCmosSensor(0x75, 0x6B);
        HI351WriteCmosSensor(0x76, 0x00);
        HI351WriteCmosSensor(0x77, 0x93);
        HI351WriteCmosSensor(0x78, 0x01);
        HI351WriteCmosSensor(0x79, 0x84);
        HI351WriteCmosSensor(0x7a, 0x01);
        HI351WriteCmosSensor(0x7b, 0x88);
        HI351WriteCmosSensor(0x7c, 0x01);
        HI351WriteCmosSensor(0x7d, 0x84);
        HI351WriteCmosSensor(0x7e, 0x01);
        HI351WriteCmosSensor(0x7f, 0x88);
        HI351WriteCmosSensor(0x80, 0x01);
        HI351WriteCmosSensor(0x81, 0x13);
        HI351WriteCmosSensor(0x82, 0x01);
        HI351WriteCmosSensor(0x83, 0x3B);
        HI351WriteCmosSensor(0x84, 0x01);
        HI351WriteCmosSensor(0x85, 0x84);
        HI351WriteCmosSensor(0x86, 0x01);
        HI351WriteCmosSensor(0x87, 0x88);
        HI351WriteCmosSensor(0x88, 0x01);
        HI351WriteCmosSensor(0x89, 0x84);
        HI351WriteCmosSensor(0x8a, 0x01);
        HI351WriteCmosSensor(0x8b, 0x88);
        HI351WriteCmosSensor(0x8c, 0x01);
        HI351WriteCmosSensor(0x8d, 0x16);
        HI351WriteCmosSensor(0x8e, 0x01);
        HI351WriteCmosSensor(0x8f, 0x42);
        HI351WriteCmosSensor(0x90, 0x00);
        HI351WriteCmosSensor(0x91, 0x68);
        HI351WriteCmosSensor(0x92, 0x01);
        HI351WriteCmosSensor(0x93, 0x80);
        HI351WriteCmosSensor(0x94, 0x00);
        HI351WriteCmosSensor(0x95, 0x68);
        HI351WriteCmosSensor(0x96, 0x01);
        HI351WriteCmosSensor(0x97, 0x80);
        HI351WriteCmosSensor(0x98, 0x01);
        HI351WriteCmosSensor(0x99, 0x80);
        HI351WriteCmosSensor(0x9a, 0x00);
        HI351WriteCmosSensor(0x9b, 0x68);
        HI351WriteCmosSensor(0x9c, 0x01);
        HI351WriteCmosSensor(0x9d, 0x80);
        HI351WriteCmosSensor(0x9e, 0x00);
        HI351WriteCmosSensor(0x9f, 0x68);
        HI351WriteCmosSensor(0xa0, 0x00);
        HI351WriteCmosSensor(0xa1, 0x08);
        HI351WriteCmosSensor(0xa2, 0x00);
        HI351WriteCmosSensor(0xa3, 0x04);
        HI351WriteCmosSensor(0xa4, 0x00);
        HI351WriteCmosSensor(0xa5, 0x08);
        HI351WriteCmosSensor(0xa6, 0x00);
        HI351WriteCmosSensor(0xa7, 0x04);
        HI351WriteCmosSensor(0xa8, 0x00);
        HI351WriteCmosSensor(0xa9, 0x73);
        HI351WriteCmosSensor(0xaa, 0x00);
        HI351WriteCmosSensor(0xab, 0x64);
        HI351WriteCmosSensor(0xac, 0x00);
        HI351WriteCmosSensor(0xad, 0x73);
        HI351WriteCmosSensor(0xae, 0x00);
        HI351WriteCmosSensor(0xaf, 0x64);
        HI351WriteCmosSensor(0xc0, 0x00);
        HI351WriteCmosSensor(0xc1, 0x1d);
        HI351WriteCmosSensor(0xc2, 0x00);
        HI351WriteCmosSensor(0xc3, 0x2f);
        HI351WriteCmosSensor(0xc4, 0x00);
        HI351WriteCmosSensor(0xc5, 0x1d);
        HI351WriteCmosSensor(0xc6, 0x00);
        HI351WriteCmosSensor(0xc7, 0x2f);
        HI351WriteCmosSensor(0xc8, 0x00);
        HI351WriteCmosSensor(0xc9, 0x1f);
        HI351WriteCmosSensor(0xca, 0x00);
        HI351WriteCmosSensor(0xcb, 0x2d);
        HI351WriteCmosSensor(0xcc, 0x00);
        HI351WriteCmosSensor(0xcd, 0x1f);
        HI351WriteCmosSensor(0xce, 0x00);
        HI351WriteCmosSensor(0xcf, 0x2d);
        HI351WriteCmosSensor(0xd0, 0x00);
        HI351WriteCmosSensor(0xd1, 0x21);
        HI351WriteCmosSensor(0xd2, 0x00);
        HI351WriteCmosSensor(0xd3, 0x2b);
        HI351WriteCmosSensor(0xd4, 0x00);
        HI351WriteCmosSensor(0xd5, 0x21);
        HI351WriteCmosSensor(0xd6, 0x00);
        HI351WriteCmosSensor(0xd7, 0x2b);
        HI351WriteCmosSensor(0xd8, 0x00);
        HI351WriteCmosSensor(0xd9, 0x23);
        HI351WriteCmosSensor(0xdA, 0x00);
        HI351WriteCmosSensor(0xdB, 0x29);
        HI351WriteCmosSensor(0xdC, 0x00);
        HI351WriteCmosSensor(0xdD, 0x23);
        HI351WriteCmosSensor(0xdE, 0x00);
        HI351WriteCmosSensor(0xdF, 0x29);
        HI351WriteCmosSensor(0xe0, 0x00);
        HI351WriteCmosSensor(0xe1, 0x6B);
        HI351WriteCmosSensor(0xe2, 0x00);
        HI351WriteCmosSensor(0xe3, 0xE8);
        HI351WriteCmosSensor(0xe4, 0x00);
        HI351WriteCmosSensor(0xe5, 0xEB);
        HI351WriteCmosSensor(0xe6, 0x01);
        HI351WriteCmosSensor(0xe7, 0x7E);
        HI351WriteCmosSensor(0xe8, 0x00);
        HI351WriteCmosSensor(0xe9, 0x95);
        HI351WriteCmosSensor(0xea, 0x00);
        HI351WriteCmosSensor(0xeb, 0xF1);
        HI351WriteCmosSensor(0xec, 0x00);
        HI351WriteCmosSensor(0xed, 0xdd);
        HI351WriteCmosSensor(0xee, 0x00);
        HI351WriteCmosSensor(0xef, 0x00);
                       
        HI351WriteCmosSensor(0xf0, 0x00);
        HI351WriteCmosSensor(0xf1, 0x34);
        HI351WriteCmosSensor(0xf2, 0x00);
            
            ///////////////////////////////////////////
            // 10 Page
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x10);
        HI351WriteCmosSensor(0xe0, 0xff); 
        HI351WriteCmosSensor(0xe1, 0x3f); // don't touch update
        HI351WriteCmosSensor(0xe2, 0xff); // don't touch update
        HI351WriteCmosSensor(0xe3, 0xff); // don't touch update
        HI351WriteCmosSensor(0xe4, 0xf7); // don't touch update
        HI351WriteCmosSensor(0xe5, 0x79); // don't touch update
        HI351WriteCmosSensor(0xe6, 0xce); // don't touch update
        HI351WriteCmosSensor(0xe7, 0x1f); // don't touch update
        HI351WriteCmosSensor(0xe8, 0x5f); // don't touch update
        HI351WriteCmosSensor(0xe9, 0x00); // don't touch update
        HI351WriteCmosSensor(0xea, 0x00); // don't touch update
        HI351WriteCmosSensor(0xeb, 0x00); // don't touch update
        HI351WriteCmosSensor(0xec, 0x00); // don't touch update
        HI351WriteCmosSensor(0xed, 0x00); // don't touch update
        HI351WriteCmosSensor(0xf0, 0x3f);
        HI351WriteCmosSensor(0xf1, 0x00); // don't touch update
        HI351WriteCmosSensor(0xf2, 0x40); // don't touch update
            
        HI351WriteCmosSensor(0x10, 0x03); //YUV422-YUYV
        HI351WriteCmosSensor(0x12, 0x10); //Y,DY offset Enb
        HI351WriteCmosSensor(0x13, 0x02); //Bright2, Contrast Enb
        HI351WriteCmosSensor(0x20, 0x80);
            
        HI351WriteCmosSensor(0x60, 0x03); //Sat, Trans Enb
        HI351WriteCmosSensor(0x61, 0x80);
        HI351WriteCmosSensor(0x62, 0x80);
            //Desat - Chroma 
            // STEVE for achromatic color   
        HI351WriteCmosSensor(0x03, 0x10);
        HI351WriteCmosSensor(0x70, 0x08);
        HI351WriteCmosSensor(0x71, 0x18);
        HI351WriteCmosSensor(0x72, 0xac);
        HI351WriteCmosSensor(0x73, 0xe1);
        HI351WriteCmosSensor(0x74, 0x99);
        HI351WriteCmosSensor(0x75, 0x00);
        HI351WriteCmosSensor(0x76, 0x0c);
        HI351WriteCmosSensor(0x77, 0x1e);
                       
        HI351WriteCmosSensor(0x78, 0xb8);
        HI351WriteCmosSensor(0x79, 0x30);
        HI351WriteCmosSensor(0x7a, 0x00);
        HI351WriteCmosSensor(0x7b, 0x40);
        HI351WriteCmosSensor(0x7c, 0x00);
        HI351WriteCmosSensor(0x7d, 0x04);
        HI351WriteCmosSensor(0x7e, 0x0a);
        HI351WriteCmosSensor(0x7f, 0x14);
            
            ///////////////////////////////////////////
            // 11 page D-LPF
            ///////////////////////////////////////////  
            //DLPF
        HI351WriteCmosSensor(0x03, 0x11);
        HI351WriteCmosSensor(0xf0, 0x00);
        HI351WriteCmosSensor(0xf1, 0x00);
        HI351WriteCmosSensor(0xf2, 0xb8);
        HI351WriteCmosSensor(0xf3, 0xb0);
        HI351WriteCmosSensor(0xf4, 0xfe);
        HI351WriteCmosSensor(0xf5, 0xfd);
        HI351WriteCmosSensor(0xf6, 0x00);
        HI351WriteCmosSensor(0xf7, 0x00);
            
            // STEVE Luminanace level setting (Add to DMA)
        HI351WriteCmosSensor(0x32, 0x8b);
        HI351WriteCmosSensor(0x33, 0x54);
        HI351WriteCmosSensor(0x34, 0x2c);
        HI351WriteCmosSensor(0x35, 0x29);
        HI351WriteCmosSensor(0x36, 0x18);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x38, 0x17);
        
            ///////////////////////////////////////////
            // 12 page DPC / GBGR /LensDebulr
            ///////////////////////////////////////////             
        HI351WriteCmosSensor(0x03, 0x12);                    
        HI351WriteCmosSensor(0x10, 0x57);
        HI351WriteCmosSensor(0x11, 0x29);
        HI351WriteCmosSensor(0x12, 0x08);
        HI351WriteCmosSensor(0x13, 0x00);
        HI351WriteCmosSensor(0x14, 0x00);
        HI351WriteCmosSensor(0x15, 0x00);
        HI351WriteCmosSensor(0x16, 0x00);
        HI351WriteCmosSensor(0x17, 0x00);
                            
        HI351WriteCmosSensor(0x18, 0xc8);
        HI351WriteCmosSensor(0x19, 0x7d);
        HI351WriteCmosSensor(0x1a, 0x32);
        HI351WriteCmosSensor(0x1b, 0x02);
        HI351WriteCmosSensor(0x1c, 0x77);
        HI351WriteCmosSensor(0x1d, 0x1e);
        HI351WriteCmosSensor(0x1e, 0x28);
        HI351WriteCmosSensor(0x1f, 0x28);
                       
        HI351WriteCmosSensor(0x20, 0x14);
        HI351WriteCmosSensor(0x21, 0x11);
        HI351WriteCmosSensor(0x22, 0x0f);
        HI351WriteCmosSensor(0x23, 0x16);
        HI351WriteCmosSensor(0x24, 0x15);
        HI351WriteCmosSensor(0x25, 0x14);
        HI351WriteCmosSensor(0x26, 0x28);
        HI351WriteCmosSensor(0x27, 0x3c);
                       
        HI351WriteCmosSensor(0x28, 0x78);
        HI351WriteCmosSensor(0x29, 0xa0);
        HI351WriteCmosSensor(0x2a, 0xb4);
        HI351WriteCmosSensor(0x2b, 0x08);//DPC threshold
        HI351WriteCmosSensor(0x2c, 0x08);//DPC threshold
        HI351WriteCmosSensor(0x2d, 0x08);//DPC threshold
        HI351WriteCmosSensor(0x2e, 0x06);//DPC threshold
        HI351WriteCmosSensor(0x2f, 0x64);
                       
        HI351WriteCmosSensor(0x30, 0x64);
        HI351WriteCmosSensor(0x31, 0x64);
        HI351WriteCmosSensor(0x32, 0x64);  
            //GBGR          
        HI351WriteCmosSensor(0x33, 0xaa);
        HI351WriteCmosSensor(0x34, 0x96);
        HI351WriteCmosSensor(0x35, 0x04);
        HI351WriteCmosSensor(0x36, 0x0e);
        HI351WriteCmosSensor(0x37, 0x0c);
                       
        HI351WriteCmosSensor(0x38, 0x04);
        HI351WriteCmosSensor(0x39, 0x04);
        HI351WriteCmosSensor(0x3a, 0x03);
        HI351WriteCmosSensor(0x3b, 0x0c);
        HI351WriteCmosSensor(0x3C, 0x00);
        HI351WriteCmosSensor(0x3D, 0x00);
        HI351WriteCmosSensor(0x3E, 0x00);
        HI351WriteCmosSensor(0x3F, 0x00);
            
        HI351WriteCmosSensor(0x40, 0x33);
        HI351WriteCmosSensor(0xE0, 0x0c);
        HI351WriteCmosSensor(0xE1, 0x58);
        HI351WriteCmosSensor(0xEC, 0x10);
        HI351WriteCmosSensor(0xEE, 0x03);
            
            ///////////////////////////////////////////
            // 13 page YC2D LPF
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x13);
        HI351WriteCmosSensor(0x10, 0x33); //Don't touch
        HI351WriteCmosSensor(0xa0, 0x0f); //Don't touch
        HI351WriteCmosSensor(0xe1, 0x07);
            
            ///////////////////////////////////////////
            // 14 page Sharpness
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x14);
        HI351WriteCmosSensor(0x10, 0x27); //Don't touch
        HI351WriteCmosSensor(0x11, 0x02); //Don't touch
        HI351WriteCmosSensor(0x12, 0x40); //Don't touch
        HI351WriteCmosSensor(0x20, 0x82); //Don't touch
        HI351WriteCmosSensor(0x30, 0x82); //Don't touch
        HI351WriteCmosSensor(0x40, 0x84); //Don't touch
        HI351WriteCmosSensor(0x50, 0x84); //Don't touch
            
            ///////////////////////////////////////////
            // 15 Page LSC off
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x15);
        HI351WriteCmosSensor(0x10, 0x82); //lsc off
        
        HI351WriteCmosSensor(0x03, 0xFE); //need to merge for solving preview rainbow problem
        HI351WriteCmosSensor(0xFE, 0x0A);                      
                          
            ///////////////////////////////////////////
            // 7 Page LSC data (STEVE 75p)
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x07);
        HI351WriteCmosSensor(0x12, 0x04);//07
        HI351WriteCmosSensor(0x34, 0x00);
        HI351WriteCmosSensor(0x35, 0x00);
        HI351WriteCmosSensor(0x13, 0x85);
        HI351WriteCmosSensor(0x13, 0x05);
            
            //================ LSC set start
            //start
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x40);
        HI351WriteCmosSensor(0x37, 0x46);
        HI351WriteCmosSensor(0x37, 0x48);
        HI351WriteCmosSensor(0x37, 0x4a);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x3f);
        HI351WriteCmosSensor(0x37, 0x43);
        HI351WriteCmosSensor(0x37, 0x43);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x3a);
        HI351WriteCmosSensor(0x37, 0x40);
        HI351WriteCmosSensor(0x37, 0x40);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x3a);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x38);
        HI351WriteCmosSensor(0x37, 0x3a);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x3e);
        HI351WriteCmosSensor(0x37, 0x3f);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x42);
        HI351WriteCmosSensor(0x37, 0x44);
        HI351WriteCmosSensor(0x37, 0x3f);
        HI351WriteCmosSensor(0x37, 0x3e);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x38);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x3e);
        HI351WriteCmosSensor(0x37, 0x43);
        HI351WriteCmosSensor(0x37, 0x48);
        HI351WriteCmosSensor(0x37, 0x4b);
        HI351WriteCmosSensor(0x37, 0x4d);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x42);
        HI351WriteCmosSensor(0x37, 0x46);
        HI351WriteCmosSensor(0x37, 0x46);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x42);
        HI351WriteCmosSensor(0x37, 0x43);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x3e);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x3f);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3d);
        HI351WriteCmosSensor(0x37, 0x43);
        HI351WriteCmosSensor(0x37, 0x44);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x41);
        HI351WriteCmosSensor(0x37, 0x47);
        HI351WriteCmosSensor(0x37, 0x48);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x3f);
        HI351WriteCmosSensor(0x37, 0x41);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x39);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x3a);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3a);
        HI351WriteCmosSensor(0x37, 0x3d);
        HI351WriteCmosSensor(0x37, 0x3d);
        HI351WriteCmosSensor(0x37, 0x41);
        HI351WriteCmosSensor(0x37, 0x3e);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x38);
        HI351WriteCmosSensor(0x37, 0x3d);
        HI351WriteCmosSensor(0x37, 0x43);
        HI351WriteCmosSensor(0x37, 0x47);
        HI351WriteCmosSensor(0x37, 0x4a);
        HI351WriteCmosSensor(0x37, 0x3a);
        HI351WriteCmosSensor(0x37, 0x38);
        HI351WriteCmosSensor(0x37, 0x34);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x41);
        HI351WriteCmosSensor(0x37, 0x42);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x3d);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x1d);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x11);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x2a);
        HI351WriteCmosSensor(0x37, 0x2d);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x03);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x37, 0x01);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x08);
        HI351WriteCmosSensor(0x37, 0x0e);
        HI351WriteCmosSensor(0x37, 0x16);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x20);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x07);
        HI351WriteCmosSensor(0x37, 0x05);
        HI351WriteCmosSensor(0x37, 0x04);
        HI351WriteCmosSensor(0x37, 0x06);
        HI351WriteCmosSensor(0x37, 0x09);
        HI351WriteCmosSensor(0x37, 0x0d);
        HI351WriteCmosSensor(0x37, 0x13);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x19);
        HI351WriteCmosSensor(0x37, 0x15);
        HI351WriteCmosSensor(0x37, 0x10);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x0b);
        HI351WriteCmosSensor(0x37, 0x0a);
        HI351WriteCmosSensor(0x37, 0x0c);
        HI351WriteCmosSensor(0x37, 0x0f);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x1a);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x2e);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x29);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x17);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x12);
        HI351WriteCmosSensor(0x37, 0x14);
        HI351WriteCmosSensor(0x37, 0x18);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x35);
        HI351WriteCmosSensor(0x37, 0x3b);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x26);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x1f);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x1b);
        HI351WriteCmosSensor(0x37, 0x1c);
        HI351WriteCmosSensor(0x37, 0x1e);
        HI351WriteCmosSensor(0x37, 0x21);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x30);
        HI351WriteCmosSensor(0x37, 0x37);
        HI351WriteCmosSensor(0x37, 0x3d);
        HI351WriteCmosSensor(0x37, 0x42);
        HI351WriteCmosSensor(0x37, 0x42);
        HI351WriteCmosSensor(0x37, 0x33);
        HI351WriteCmosSensor(0x37, 0x32);
        HI351WriteCmosSensor(0x37, 0x2f);
        HI351WriteCmosSensor(0x37, 0x2b);
        HI351WriteCmosSensor(0x37, 0x27);
        HI351WriteCmosSensor(0x37, 0x25);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x22);
        HI351WriteCmosSensor(0x37, 0x23);
        HI351WriteCmosSensor(0x37, 0x24);
        HI351WriteCmosSensor(0x37, 0x28);
        HI351WriteCmosSensor(0x37, 0x2c);
        HI351WriteCmosSensor(0x37, 0x31);
        HI351WriteCmosSensor(0x37, 0x36);
        HI351WriteCmosSensor(0x37, 0x3c);
        HI351WriteCmosSensor(0x37, 0x41);
        HI351WriteCmosSensor(0x37, 0x46);
        HI351WriteCmosSensor(0x37, 0x46);
            //END
            
            //================ LSC set end
            
        HI351WriteCmosSensor(0x12, 0x00);
        HI351WriteCmosSensor(0x13, 0x00);
            
        HI351WriteCmosSensor(0x03, 0x15);
        HI351WriteCmosSensor(0x10, 0x83); // LSC ON
            
            ///////////////////////////////////////////
            // 16 Page CMC
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x16);
            
        HI351WriteCmosSensor(0x10, 0x0f); //cmc
        HI351WriteCmosSensor(0x17, 0x2f); //CMC SIGN
        HI351WriteCmosSensor(0x60, 0x3f); //mcmc steve MCMC ON 20111221 
            
            // STEVE automatic saturation according Y level
        HI351WriteCmosSensor(0x8a, 0x5c);
        HI351WriteCmosSensor(0x8b, 0x73);
        HI351WriteCmosSensor(0x8c, 0x7b);
        HI351WriteCmosSensor(0x8d, 0x7f);
        HI351WriteCmosSensor(0x8e, 0x7f);
        HI351WriteCmosSensor(0x8f, 0x7f);
        HI351WriteCmosSensor(0x90, 0x7f);
        HI351WriteCmosSensor(0x91, 0x7f);
        HI351WriteCmosSensor(0x92, 0x7f);
        HI351WriteCmosSensor(0x93, 0x7f);
        HI351WriteCmosSensor(0x94, 0x7f);
        HI351WriteCmosSensor(0x95, 0x7f);
        HI351WriteCmosSensor(0x96, 0x7f);
        HI351WriteCmosSensor(0x97, 0x7f);
        HI351WriteCmosSensor(0x98, 0x7f);
        HI351WriteCmosSensor(0x99, 0x7c);
        HI351WriteCmosSensor(0x9a, 0x78);
            
            //Dgain
        HI351WriteCmosSensor(0xa0, 0x81); //Manual WB gain enable
        HI351WriteCmosSensor(0xa1, 0x00);
            
        HI351WriteCmosSensor(0xa2, 0x68); //R_dgain_byr
        HI351WriteCmosSensor(0xa3, 0x70); //B_dgain_byr
            
        HI351WriteCmosSensor(0xa6, 0xa0); //r max
        HI351WriteCmosSensor(0xa8, 0xa0); //b max
                    // Pre WB gain setting(after AWB setting)
        HI351WriteCmosSensor(0xF0, 0x09);//Pre WB gain enable Gain resolution_2x
        HI351WriteCmosSensor(0xF1, 0x80);
        HI351WriteCmosSensor(0xF2, 0x80);
        HI351WriteCmosSensor(0xF3, 0x80);
        HI351WriteCmosSensor(0xF4, 0x80); 
            ///////////////////////////////////////////
            // 17 Page Gamma
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x17);
        HI351WriteCmosSensor(0x10, 0x01);
            
            ///////////////////////////////////////////
            // 18 Page Histogram
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x18);
        HI351WriteCmosSensor(0x10, 0x00);
        HI351WriteCmosSensor(0xc0, 0x01);
        HI351WriteCmosSensor(0xc4, 0x7e);//110927
        HI351WriteCmosSensor(0xc5, 0x69);
            
            ///////////////////////////////////////////
            // 20 Page AE
            ///////////////////////////////////////////             
        HI351WriteCmosSensor(0x03, 0x20);
        HI351WriteCmosSensor(0x10, 0xcf);//auto flicker auto 60hz select
        HI351WriteCmosSensor(0x12, 0x2d); // STEVE Dgain off (2d)
        HI351WriteCmosSensor(0x17, 0xa0);
        HI351WriteCmosSensor(0x1f, 0x1f);
            
        HI351WriteCmosSensor(0x03, 0x20); //Page 20
        HI351WriteCmosSensor(0x20, 0x00); //EXP Normal 33.33 fps 
        HI351WriteCmosSensor(0x21, 0x10); 
        HI351WriteCmosSensor(0x22, 0x79); 
        HI351WriteCmosSensor(0x23, 0x10); 
        HI351WriteCmosSensor(0x24, 0x00); //EXP Max 10.00 fps   
        HI351WriteCmosSensor(0x25, 0x36);                              
        HI351WriteCmosSensor(0x26, 0xe8);                              
        HI351WriteCmosSensor(0x27, 0xe0);                              
        HI351WriteCmosSensor(0x28, 0x00); //EXPMin 25210.08 fps
        HI351WriteCmosSensor(0x29, 0x0b); 
        HI351WriteCmosSensor(0x2a, 0x28); 
        HI351WriteCmosSensor(0x30, 0x05); //EXP100 
        HI351WriteCmosSensor(0x31, 0x7d); 
        HI351WriteCmosSensor(0x32, 0xb0); 
        HI351WriteCmosSensor(0x33, 0x04); //EXP120 
        HI351WriteCmosSensor(0x34, 0x93); 
        HI351WriteCmosSensor(0x35, 0x68); 
        HI351WriteCmosSensor(0x36, 0x00); //EXP Unit 
        HI351WriteCmosSensor(0x37, 0x05); 
        HI351WriteCmosSensor(0x38, 0x94);
            
                         
                
        HI351WriteCmosSensor(0x40, 0x00); //exp 12000
        HI351WriteCmosSensor(0x41, 0x04); 
        HI351WriteCmosSensor(0x42, 0x93); 
        HI351WriteCmosSensor(0x43, 0x04);
            
        HI351WriteCmosSensor(0x51, 0xD0); //pga_max_total A0 -) D0 STEVE
        HI351WriteCmosSensor(0x52, 0x28); //pga_min_total
            
        HI351WriteCmosSensor(0x71, 0x80); //DG MAX 0x80 STEVE
        HI351WriteCmosSensor(0x72, 0x80); //DG MIN
            
        HI351WriteCmosSensor(0x80, 0x36); //AE target 34 -) 36 STEVE
            
            ///////////////////////////////////////////
            // Preview Setting
            ///////////////////////////////////////////
            
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x10, 0x13); //Pre2
            
        
        
            
        HI351WriteCmosSensor(0x03, 0x15);  //Shading
        HI351WriteCmosSensor(0x10, 0x81);  //
        HI351WriteCmosSensor(0x20, 0x04);  //Shading Width 2048
        HI351WriteCmosSensor(0x21, 0x00);
        HI351WriteCmosSensor(0x22, 0x03);  //Shading Height 768
        HI351WriteCmosSensor(0x23, 0x00);
            
        
            
            ///////////////////////////////////////////
            // 30 Page DMA address set
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x30); //DMA
        HI351WriteCmosSensor(0x7c, 0x2c); //Extra str
        HI351WriteCmosSensor(0x7d, 0xce);
        HI351WriteCmosSensor(0x7e, 0x2c); //Extra end
        HI351WriteCmosSensor(0x7f, 0xd1);
        HI351WriteCmosSensor(0x80, 0x24); //Outdoor str
        HI351WriteCmosSensor(0x81, 0x70); 
        HI351WriteCmosSensor(0x82, 0x24); //Outdoor end 
        HI351WriteCmosSensor(0x83, 0x73);  
        HI351WriteCmosSensor(0x84, 0x21); //Indoor str 
        HI351WriteCmosSensor(0x85, 0xa6);  
        HI351WriteCmosSensor(0x86, 0x21); //Indoor end 
        HI351WriteCmosSensor(0x87, 0xa9); 
        HI351WriteCmosSensor(0x88, 0x27); //Dark1 str
        HI351WriteCmosSensor(0x89, 0x3a);  
        HI351WriteCmosSensor(0x8a, 0x27); //Dark1 end  
        HI351WriteCmosSensor(0x8b, 0x3d); 
        HI351WriteCmosSensor(0x8c, 0x2a); //Dark2 str 
        HI351WriteCmosSensor(0x8d, 0x04);  
        HI351WriteCmosSensor(0x8e, 0x2a); //Dark2 end 
        HI351WriteCmosSensor(0x8f, 0x07);
                   
        HI351WriteCmosSensor(0x03, 0xC0);
        HI351WriteCmosSensor(0x2F, 0xf0); //DMA busy flag check
        HI351WriteCmosSensor(0x31, 0x20); //Delay before DMA write
        HI351WriteCmosSensor(0x33, 0x20); //DMA full stuck mode
        HI351WriteCmosSensor(0x32, 0x01); //DMA on first 
            
        HI351WriteCmosSensor(0x03, 0xC0);
        HI351WriteCmosSensor(0x2F, 0xf0); //DMA busy flag check
        HI351WriteCmosSensor(0x31, 0x20); //Delay before DMA write
        HI351WriteCmosSensor(0x33, 0x20);
        HI351WriteCmosSensor(0x32, 0x01); //DMA on second
            
            
        HI351WriteCmosSensor(0x03, 0xC0);
        HI351WriteCmosSensor(0xe1, 0x80);// PCON Enable option
        HI351WriteCmosSensor(0xe1, 0x80);// PCON MODE ON
            
            //MCU Set
        HI351WriteCmosSensor(0x03, 0x30);
        HI351WriteCmosSensor(0x12, 0x00);
        HI351WriteCmosSensor(0x20, 0x08);
        HI351WriteCmosSensor(0x50, 0x00);
        HI351WriteCmosSensor(0xE0, 0x02);
        HI351WriteCmosSensor(0xF0, 0x00);
        HI351WriteCmosSensor(0x11, 0x05);// M2i Hold
        HI351WriteCmosSensor(0x03, 0xc0);
        HI351WriteCmosSensor(0xe4, 0x64); //delay
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x01, 0xF0); // sleep off
            
            ///////////////////////////////////////////
            // CD Page Adaptive Mode(Color ratio)
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xCD);
        HI351WriteCmosSensor(0x47, 0x00);
        HI351WriteCmosSensor(0x12, 0x80);
        HI351WriteCmosSensor(0x13, 0x80); //Ratio WB R gain min
        HI351WriteCmosSensor(0x14, 0x90); //Ratio WB R gain max
        HI351WriteCmosSensor(0x15, 0x80); //Ratio WB B gain min
        HI351WriteCmosSensor(0x16, 0x90); //Ratio WB B gain max
        HI351WriteCmosSensor(0x10, 0x38); // STEVE b9 -) 38 Disable
            
            ///////////////////////////////////////////
            // 1F Page SSD
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x1f); //1F page
        HI351WriteCmosSensor(0x11, 0x00); //bit[5:4]: debug mode
        HI351WriteCmosSensor(0x12, 0x60);
        HI351WriteCmosSensor(0x13, 0x14);
        HI351WriteCmosSensor(0x14, 0x10);
        HI351WriteCmosSensor(0x15, 0x00);
        HI351WriteCmosSensor(0x20, 0x18); //ssd_x_start_pos
        HI351WriteCmosSensor(0x21, 0x14); //ssd_y_start_pos
        HI351WriteCmosSensor(0x22, 0x8C); //ssd_blk_width
        HI351WriteCmosSensor(0x23, 0x9c); //ssd_blk_height
        HI351WriteCmosSensor(0x28, 0x18);
        HI351WriteCmosSensor(0x29, 0x02);
        HI351WriteCmosSensor(0x3B, 0x18);
        HI351WriteCmosSensor(0x3C, 0x8C);
        HI351WriteCmosSensor(0x10, 0x19); //SSD enable
            
            ///////////////////////////////////////////
            
            ///////////////////////////////////////////
            // C4 Page MCU AE
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xc4);
        HI351WriteCmosSensor(0x11, 0x30); // ae speed B[7:6] 0 (SLOW) ~ 3 (FAST), 0x70 - 0x30   
        HI351WriteCmosSensor(0x12, 0x10);       
        HI351WriteCmosSensor(0x19, 0x30); // band0 gain 40fps 0x2d
        HI351WriteCmosSensor(0x1a, 0x38); // band1 gain 20fps original 34 Steve
        HI351WriteCmosSensor(0x1b, 0x4c); // band2 gain 12fps
        HI351WriteCmosSensor(0x1c, 0x04);
        HI351WriteCmosSensor(0x1d, 0x80);
        HI351WriteCmosSensor(0x1e, 0x00); // band1 min exposure time    1/40s // correction point
        HI351WriteCmosSensor(0x1f, 0x0d); 
        HI351WriteCmosSensor(0x20, 0xbb);
                       
        HI351WriteCmosSensor(0x21, 0xa0);
        HI351WriteCmosSensor(0x22, 0x00); // band2 min exposure time    1/20s
        HI351WriteCmosSensor(0x23, 0x1b);
        HI351WriteCmosSensor(0x24, 0x77);
        HI351WriteCmosSensor(0x25, 0xa0);
        HI351WriteCmosSensor(0x26, 0x00);// band3 min exposure time  1/12s
        HI351WriteCmosSensor(0x27, 0x2d);
        HI351WriteCmosSensor(0x28, 0xc6);
                       
        HI351WriteCmosSensor(0x29, 0xc0);
            
        HI351WriteCmosSensor(0x36, 0x22); // AE Yth     
        HI351WriteCmosSensor(0x03, 0x20);
        HI351WriteCmosSensor(0x12, 0x2d); // STEVE 6d -) 2d (AE digital gain OFF)
            
            ///////////////////////////////////////////
            // c3 Page MCU AE Weight
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xc3);
        HI351WriteCmosSensor(0x10, 0x00);
        HI351WriteCmosSensor(0x38, 0xFE);
        HI351WriteCmosSensor(0x39, 0x79);
                            
        HI351WriteCmosSensor(0x3A, 0x11);
        HI351WriteCmosSensor(0x3B, 0x11);
        HI351WriteCmosSensor(0x3C, 0x11);
        HI351WriteCmosSensor(0x3D, 0x11);
        HI351WriteCmosSensor(0x3E, 0x11);
        HI351WriteCmosSensor(0x3F, 0x11);
        HI351WriteCmosSensor(0x40, 0x11);
        HI351WriteCmosSensor(0x41, 0x11);
                       
        HI351WriteCmosSensor(0x42, 0x11);
        HI351WriteCmosSensor(0x43, 0x11);
        HI351WriteCmosSensor(0x44, 0x11);
        HI351WriteCmosSensor(0x45, 0x11);
        HI351WriteCmosSensor(0x46, 0x11);
        HI351WriteCmosSensor(0x47, 0x11);
        HI351WriteCmosSensor(0x48, 0x11);
        HI351WriteCmosSensor(0x49, 0x11);
                       
        HI351WriteCmosSensor(0x4A, 0x11);
        HI351WriteCmosSensor(0x4B, 0x11);
        HI351WriteCmosSensor(0x4C, 0x11);
        HI351WriteCmosSensor(0x4D, 0x31);
        HI351WriteCmosSensor(0x4E, 0x77);
        HI351WriteCmosSensor(0x4F, 0x77);
        HI351WriteCmosSensor(0x50, 0x13);
        HI351WriteCmosSensor(0x51, 0x11);
                       
        HI351WriteCmosSensor(0x52, 0x11);
        HI351WriteCmosSensor(0x53, 0x32);
        HI351WriteCmosSensor(0x54, 0x77);
        HI351WriteCmosSensor(0x55, 0x77);
        HI351WriteCmosSensor(0x56, 0x23);
        HI351WriteCmosSensor(0x57, 0x11);
        HI351WriteCmosSensor(0x58, 0x21);
        HI351WriteCmosSensor(0x59, 0x66);
                       
        HI351WriteCmosSensor(0x5A, 0x76);
        HI351WriteCmosSensor(0x5B, 0x67);
        HI351WriteCmosSensor(0x5C, 0x66);
        HI351WriteCmosSensor(0x5D, 0x12);
        HI351WriteCmosSensor(0x5E, 0x21);
        HI351WriteCmosSensor(0x5F, 0x66);
        HI351WriteCmosSensor(0x60, 0x76);
        HI351WriteCmosSensor(0x61, 0x67);
                       
        HI351WriteCmosSensor(0x62, 0x66);
        HI351WriteCmosSensor(0x63, 0x12);
        HI351WriteCmosSensor(0x64, 0x21);
        HI351WriteCmosSensor(0x65, 0x55);
        HI351WriteCmosSensor(0x66, 0x55);
        HI351WriteCmosSensor(0x67, 0x55);
        HI351WriteCmosSensor(0x68, 0x55);
        HI351WriteCmosSensor(0x69, 0x12);
                       
        HI351WriteCmosSensor(0x6A, 0x11);
        HI351WriteCmosSensor(0x6B, 0x11);
        HI351WriteCmosSensor(0x6C, 0x11);
        HI351WriteCmosSensor(0x6D, 0x11);
        HI351WriteCmosSensor(0x6E, 0x11);
        HI351WriteCmosSensor(0x6F, 0x11);           
        HI351WriteCmosSensor(0x70, 0x00);
        HI351WriteCmosSensor(0x71, 0x00);
                       
        HI351WriteCmosSensor(0x72, 0x00);
        HI351WriteCmosSensor(0x73, 0x00);
        HI351WriteCmosSensor(0x74, 0x00);
        HI351WriteCmosSensor(0x75, 0x00);
        HI351WriteCmosSensor(0x76, 0x00);
        HI351WriteCmosSensor(0x77, 0x00);
        HI351WriteCmosSensor(0x78, 0x00);
        HI351WriteCmosSensor(0x79, 0x00);
                       
        HI351WriteCmosSensor(0x7A, 0x00);
        HI351WriteCmosSensor(0x7B, 0x00);
        HI351WriteCmosSensor(0x7C, 0x11);
        HI351WriteCmosSensor(0x7D, 0x11);
        HI351WriteCmosSensor(0x7E, 0x11);
        HI351WriteCmosSensor(0x7F, 0x11);
        HI351WriteCmosSensor(0x80, 0x11);
        HI351WriteCmosSensor(0x81, 0x11);
                       
        HI351WriteCmosSensor(0x82, 0x11);
        HI351WriteCmosSensor(0x83, 0x71);
        HI351WriteCmosSensor(0x84, 0x77);
        HI351WriteCmosSensor(0x85, 0x77);
        HI351WriteCmosSensor(0x86, 0x17);
        HI351WriteCmosSensor(0x87, 0x11);
        HI351WriteCmosSensor(0x88, 0x11);
        HI351WriteCmosSensor(0x89, 0x22);
                       
        HI351WriteCmosSensor(0x8A, 0x86);
        HI351WriteCmosSensor(0x8B, 0x68);
        HI351WriteCmosSensor(0x8C, 0x22);
        HI351WriteCmosSensor(0x8D, 0x11);
        HI351WriteCmosSensor(0x8E, 0x21);
        HI351WriteCmosSensor(0x8F, 0x65);
        HI351WriteCmosSensor(0x90, 0x77);
        HI351WriteCmosSensor(0x91, 0x77);
                       
        HI351WriteCmosSensor(0x92, 0x56);
        HI351WriteCmosSensor(0x93, 0x12);
        HI351WriteCmosSensor(0x94, 0x21);
        HI351WriteCmosSensor(0x95, 0x66);
        HI351WriteCmosSensor(0x96, 0x66);
        HI351WriteCmosSensor(0x97, 0x66);
        HI351WriteCmosSensor(0x98, 0x66);
        HI351WriteCmosSensor(0x99, 0x12);
                       
        HI351WriteCmosSensor(0x9A, 0x21);
        HI351WriteCmosSensor(0x9B, 0x52);
        HI351WriteCmosSensor(0x9C, 0x55);
        HI351WriteCmosSensor(0x9D, 0x55);
        HI351WriteCmosSensor(0x9E, 0x25);
        HI351WriteCmosSensor(0x9F, 0x12);
        HI351WriteCmosSensor(0xA0, 0x11);
        HI351WriteCmosSensor(0xA1, 0x11);
                       
        HI351WriteCmosSensor(0xA2, 0x11);
        HI351WriteCmosSensor(0xA3, 0x11);
        HI351WriteCmosSensor(0xA4, 0x11);
        HI351WriteCmosSensor(0xA5, 0x11);
            
        HI351WriteCmosSensor(0x03, 0xc3);
        HI351WriteCmosSensor(0xe1, 0x30); //STEVE OUT AG MAX 
        HI351WriteCmosSensor(0xe2, 0x03); //flicker option
            
            ///////////////////////////////////////////
            // Capture Setting
            ///////////////////////////////////////////
                        
        HI351WriteCmosSensor(0x03, 0xd5);
        HI351WriteCmosSensor(0x11, 0xb9); //manual sleep onoff  
        HI351WriteCmosSensor(0x14, 0xfd); // STEVE EXPMIN x2
        HI351WriteCmosSensor(0x1e, 0x02); //capture clock set
        HI351WriteCmosSensor(0x86, 0x02); //preview clock set
          
        
          // STEVE When capture process, decrease Green
          HI351WriteCmosSensor(0x1f, 0x00);                      
           HI351WriteCmosSensor(0x20, 0xDC); // Capture Hblank 220 -) one line 2400
        
        HI351WriteCmosSensor(0x21, 0x09);
        HI351WriteCmosSensor(0x22, 0xc4);// Capture Line unit 2180+226 = 2500
            
            ///////////////////////////////////////////
            // Capture Mode option D6
            ///////////////////////////////////////////   
        HI351WriteCmosSensor(0x03, 0xd6);    
            
        HI351WriteCmosSensor(0x03, 0xd6); 
        HI351WriteCmosSensor(0x10, 0x28); // ISO 100
        HI351WriteCmosSensor(0x11, 0x38); // ISO 200
        HI351WriteCmosSensor(0x12, 0x78); // ISO 400
        HI351WriteCmosSensor(0x13, 0xa0); // ISO 800
        HI351WriteCmosSensor(0x14, 0xe0); // ISO 1600
        HI351WriteCmosSensor(0x15, 0xf0); // ISO 3200
            ///////////////////////////////////////////
            // C0 Page Firmware system
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xc0);
        HI351WriteCmosSensor(0x16, 0x81); //MCU main roof holding on
            
            ///////////////////////////////////////////
            // C5 Page AWB
            ///////////////////////////////////////////
            
        HI351WriteCmosSensor(0x03, 0xc5); 
        HI351WriteCmosSensor(0x10, 0xb0); //bCtl1_a00_n00
        HI351WriteCmosSensor(0x11, 0xa1); // Steve [4] bit must 0 for MWB
        HI351WriteCmosSensor(0x12, 0x17); // STEVE 97 -) 9f YNorm -) 1f -) 17 near pt chek, Ynorm OFF
        HI351WriteCmosSensor(0x13, 0x19); //bCtl4_a00_n00
        HI351WriteCmosSensor(0x14, 0x24); //bLockTh_a00_n00
        HI351WriteCmosSensor(0x15, 0x04);
        HI351WriteCmosSensor(0x16, 0x0a);
        HI351WriteCmosSensor(0x17, 0x14); //bBlkPtBndWdhTh_a00_n00
                       
        HI351WriteCmosSensor(0x18, 0x28); //bBlkPtBndCntTh_a00_n00
        HI351WriteCmosSensor(0x19, 0x03);
        HI351WriteCmosSensor(0x1a, 0xa0);//awb max ylvl
        HI351WriteCmosSensor(0x1b, 0x18);//awb min ylvl
        HI351WriteCmosSensor(0x1c, 0x0a);//awb frame skip when min max
        HI351WriteCmosSensor(0x1d, 0x40);
        HI351WriteCmosSensor(0x1e, 0x01);
        HI351WriteCmosSensor(0x1f, 0x04);//sky limit
                       
        HI351WriteCmosSensor(0x20, 0x00); // out2 Angle MIN
        HI351WriteCmosSensor(0x21, 0xa0); // out2 Angle MIN steve outdoor awb angle min (for tree) 160
        HI351WriteCmosSensor(0x22, 0x01); // out2 Anble Max
        HI351WriteCmosSensor(0x23, 0x0e); // out2 Anble Max              sky limit
        HI351WriteCmosSensor(0x24, 0x00); // out1 Angle MIN
        HI351WriteCmosSensor(0x25, 0xa0); // out1 Angle MIN //steve 
        HI351WriteCmosSensor(0x26, 0x01); // out1 Anble Max //iInAglMaxLmt_a00_n00
        HI351WriteCmosSensor(0x27, 0x04); // out1 Anble Max //iInAglMaxLmt_a00_n01
                       
        HI351WriteCmosSensor(0x28, 0x00);
        HI351WriteCmosSensor(0x29, 0x73); //64(100) -) 73(115) yellow prev.
          HI351WriteCmosSensor(0x2a, 0x01); //iDakAglMaxLmt_a00_n00
          HI351WriteCmosSensor(0x2b, 0x04); //iDakAglMaxLmt_a00_n01
          HI351WriteCmosSensor(0x2c, 0x00); //iDakAglMinLmt_a00_n00
          HI351WriteCmosSensor(0x2d, 0x64); //iDakAglMinLmt_a00_n01
        HI351WriteCmosSensor(0x2e, 0x00);
        HI351WriteCmosSensor(0x2f, 0x00);
                       
        HI351WriteCmosSensor(0x30, 0x66);
        HI351WriteCmosSensor(0x31, 0x8a);
        HI351WriteCmosSensor(0x32, 0x00);
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x34, 0x8b);
        HI351WriteCmosSensor(0x35, 0x29);
        HI351WriteCmosSensor(0x36, 0x00);
        HI351WriteCmosSensor(0x37, 0x00); //dwOut1LmtTh_a00_n01
                       
        HI351WriteCmosSensor(0x38, 0xdd);
        HI351WriteCmosSensor(0x39, 0xd0);
        HI351WriteCmosSensor(0x3a, 0x00);
          HI351WriteCmosSensor(0x3b, 0x08); //dwOut1StrLmtTh_a00_n01
          HI351WriteCmosSensor(0x3c, 0xd9); //dwOut1StrLmtTh_a00_n02
          HI351WriteCmosSensor(0x3d, 0xa0); //dwOut1StrLmtTh_a00_n03
        HI351WriteCmosSensor(0x3e, 0x00);
        HI351WriteCmosSensor(0x3f, 0xdb);
                       
        HI351WriteCmosSensor(0x40, 0xba);
        HI351WriteCmosSensor(0x41, 0x00);
        HI351WriteCmosSensor(0x42, 0x00);
        HI351WriteCmosSensor(0x43, 0xe9);
        HI351WriteCmosSensor(0x44, 0xe9); //dwDakLmtTh_a00_n02
        HI351WriteCmosSensor(0x45, 0xa0);
        HI351WriteCmosSensor(0x46, 0x00);
        HI351WriteCmosSensor(0x47, 0x04);
                       
        HI351WriteCmosSensor(0x48, 0x93);
        HI351WriteCmosSensor(0x49, 0xE0);
        HI351WriteCmosSensor(0x4a, 0x00);  // steve H outdoor -) indoor(EV) 
        HI351WriteCmosSensor(0x4b, 0x06);  // steve M1 outdoor -) indoor(EV)
        HI351WriteCmosSensor(0x4c, 0x1a);  // steve M2 outdoor -) indoor(EV)
        HI351WriteCmosSensor(0x4d, 0x80);  // steve L outdoor -) indoor(EV) 
        HI351WriteCmosSensor(0x4e, 0x00);  // white region shift X
        HI351WriteCmosSensor(0x4f, 0x00);  // white region shift Y
                       
        HI351WriteCmosSensor(0x50, 0x55);
        HI351WriteCmosSensor(0x51, 0x55);
        HI351WriteCmosSensor(0x52, 0x55);
        HI351WriteCmosSensor(0x53, 0x55);
        HI351WriteCmosSensor(0x54, 0x55);
        HI351WriteCmosSensor(0x55, 0x55);
        HI351WriteCmosSensor(0x56, 0x55);
        HI351WriteCmosSensor(0x57, 0x55);
                       
        HI351WriteCmosSensor(0x58, 0x55);
        HI351WriteCmosSensor(0x59, 0x55);
        HI351WriteCmosSensor(0x5a, 0x55);
        HI351WriteCmosSensor(0x5b, 0x55);
        HI351WriteCmosSensor(0x5c, 0x55);
        HI351WriteCmosSensor(0x5d, 0x55);
        HI351WriteCmosSensor(0x5e, 0x55);
        HI351WriteCmosSensor(0x5f, 0x55);
                       
        HI351WriteCmosSensor(0x60, 0x55);
        HI351WriteCmosSensor(0x61, 0x55);
        HI351WriteCmosSensor(0x62, 0x55);
        HI351WriteCmosSensor(0x63, 0x55);
        HI351WriteCmosSensor(0x64, 0x55);
        HI351WriteCmosSensor(0x65, 0x55);
        HI351WriteCmosSensor(0x66, 0x55);
        HI351WriteCmosSensor(0x67, 0x55);
                       
        HI351WriteCmosSensor(0x68, 0x55);
        HI351WriteCmosSensor(0x69, 0x55);
        HI351WriteCmosSensor(0x6a, 0x55);
          HI351WriteCmosSensor(0x6b, 0x24); //aInWhtRgnBg_a00_n00
          HI351WriteCmosSensor(0x6c, 0x2a); //aInWhtRgnBg_a01_n00
          HI351WriteCmosSensor(0x6d, 0x31); //aInWhtRgnBg_a02_n00
          HI351WriteCmosSensor(0x6e, 0x38); //aInWhtRgnBg_a03_n00
          HI351WriteCmosSensor(0x6f, 0x3e); //aInWhtRgnBg_a04_n00
          HI351WriteCmosSensor(0x70, 0x42); //aInWhtRgnBg_a05_n00
          HI351WriteCmosSensor(0x71, 0x4a); //aInWhtRgnBg_a06_n00
          HI351WriteCmosSensor(0x72, 0x53); //aInWhtRgnBg_a07_n00
          HI351WriteCmosSensor(0x73, 0x5c); //aInWhtRgnBg_a08_n00
          HI351WriteCmosSensor(0x74, 0x69); //aInWhtRgnBg_a09_n00
          HI351WriteCmosSensor(0x75, 0x75); //aInWhtRgnBg_a10_n00
          HI351WriteCmosSensor(0x76, 0x86); //aInWhtRgnRgLeftLmt_a00_n00
          HI351WriteCmosSensor(0x77, 0x79); //aInWhtRgnRgLeftLmt_a01_n00
          HI351WriteCmosSensor(0x78, 0x69); //aInWhtRgnRgLeftLmt_a02_n00
          HI351WriteCmosSensor(0x79, 0x5b); //aInWhtRgnRgLeftLmt_a03_n00
          HI351WriteCmosSensor(0x7a, 0x53); //aInWhtRgnRgLeftLmt_a04_n00
          HI351WriteCmosSensor(0x7b, 0x4e); //aInWhtRgnRgLeftLmt_a05_n00
          HI351WriteCmosSensor(0x7c, 0x48); //aInWhtRgnRgLeftLmt_a06_n00
          HI351WriteCmosSensor(0x7d, 0x43); //aInWhtRgnRgLeftLmt_a07_n00
          HI351WriteCmosSensor(0x7e, 0x40); //aInWhtRgnRgLeftLmt_a08_n00
          HI351WriteCmosSensor(0x7f, 0x3c); //aInWhtRgnRgLeftLmt_a09_n00
          HI351WriteCmosSensor(0x80, 0x3c); //aInWhtRgnRgLeftLmt_a10_n00
          HI351WriteCmosSensor(0x81, 0x95); //aInWhtRgnRgRightLmt_a00_n00
          HI351WriteCmosSensor(0x82, 0x8f); //aInWhtRgnRgRightLmt_a01_n00
          HI351WriteCmosSensor(0x83, 0x88); //aInWhtRgnRgRightLmt_a02_n00
          HI351WriteCmosSensor(0x84, 0x81); //aInWhtRgnRgRightLmt_a03_n00
          HI351WriteCmosSensor(0x85, 0x79); //aInWhtRgnRgRightLmt_a04_n00
          HI351WriteCmosSensor(0x86, 0x71); //aInWhtRgnRgRightLmt_a05_n00
          HI351WriteCmosSensor(0x87, 0x6b); //aInWhtRgnRgRightLmt_a06_n00
          HI351WriteCmosSensor(0x88, 0x62); //aInWhtRgnRgRightLmt_a07_n00
          HI351WriteCmosSensor(0x89, 0x5a); //aInWhtRgnRgRightLmt_a08_n00
          HI351WriteCmosSensor(0x8a, 0x52); //aInWhtRgnRgRightLmt_a09_n00
          HI351WriteCmosSensor(0x8b, 0x4e); //aInWhtRgnRgRightLmt_a10_n00
          HI351WriteCmosSensor(0x8c, 0x25); //aInWhtLineBg_a00_n00
          HI351WriteCmosSensor(0x8d, 0x30); //aInWhtLineBg_a01_n00
          HI351WriteCmosSensor(0x8e, 0x35); //aInWhtLineBg_a02_n00
          HI351WriteCmosSensor(0x8f, 0x3b); //aInWhtLineBg_a03_n00
          HI351WriteCmosSensor(0x90, 0x40); //aInWhtLineBg_a04_n00
          HI351WriteCmosSensor(0x91, 0x44); //aInWhtLineBg_a05_n00
          HI351WriteCmosSensor(0x92, 0x4c); //aInWhtLineBg_a06_n00
          HI351WriteCmosSensor(0x93, 0x55); //aInWhtLineBg_a07_n00
          HI351WriteCmosSensor(0x94, 0x60); //aInWhtLineBg_a08_n00
          HI351WriteCmosSensor(0x95, 0x69); //aInWhtLineBg_a09_n00
          HI351WriteCmosSensor(0x96, 0x75); //aInWhtLineBg_a10_n00
          HI351WriteCmosSensor(0x97, 0x8e); //aInWhtLineRg_a00_n00
          HI351WriteCmosSensor(0x98, 0x7c); //aInWhtLineRg_a01_n00
          HI351WriteCmosSensor(0x99, 0x73); //aInWhtLineRg_a02_n00
          HI351WriteCmosSensor(0x9a, 0x6a); //aInWhtLineRg_a03_n00
          HI351WriteCmosSensor(0x9b, 0x61); //aInWhtLineRg_a04_n00
          HI351WriteCmosSensor(0x9c, 0x5c); //aInWhtLineRg_a05_n00
          HI351WriteCmosSensor(0x9d, 0x55); //aInWhtLineRg_a06_n00
          HI351WriteCmosSensor(0x9e, 0x4f); //aInWhtLineRg_a07_n00
          HI351WriteCmosSensor(0x9f, 0x4a); //aInWhtLineRg_a08_n00
          HI351WriteCmosSensor(0xa0, 0x47); //aInWhtLineRg_a09_n00
          HI351WriteCmosSensor(0xa1, 0x46); //aInWhtLineRg_a10_n00
        HI351WriteCmosSensor(0xa2, 0x32); //aInTgtAngle_a00_n00
        HI351WriteCmosSensor(0xa3, 0x3c); //aInTgtAngle_a01_n00
        HI351WriteCmosSensor(0xa4, 0x46); //aInTgtAngle_a02_n00
        HI351WriteCmosSensor(0xa5, 0x50); //aInTgtAngle_a03_n00
          HI351WriteCmosSensor(0xa6, 0x50); //aInTgtAngle_a04_n00
        HI351WriteCmosSensor(0xa7, 0x64); //aInTgtAngle_a05_n00
        HI351WriteCmosSensor(0xa8, 0x6e); //aInTgtAngle_a06_n00
        HI351WriteCmosSensor(0xa9, 0x78); //aInTgtAngle_a07_n00
        HI351WriteCmosSensor(0xaa, 0x06); //aInRgTgtOfs_a00_n00
        HI351WriteCmosSensor(0xab, 0x02); //aInRgTgtOfs_a01_n00
        HI351WriteCmosSensor(0xac, 0x01); //aInRgTgtOfs_a02_n00
        HI351WriteCmosSensor(0xad, 0x00); //aInRgTgtOfs_a03_n00
        HI351WriteCmosSensor(0xae, 0x00); //aInRgTgtOfs_a04_n00
        HI351WriteCmosSensor(0xaf, 0x00); //aInRgTgtOfs_a05_n00
        HI351WriteCmosSensor(0xb0, 0x00); //aInRgTgtOfs_a06_n00
        HI351WriteCmosSensor(0xb1, 0x82); //aInRgTgtOfs_a07_n00
        HI351WriteCmosSensor(0xb2, 0x86); //aInBgTgtOfs_a00_n00
        HI351WriteCmosSensor(0xb3, 0x82); //aInBgTgtOfs_a01_n00
        HI351WriteCmosSensor(0xb4, 0x81); //aInBgTgtOfs_a02_n00
        HI351WriteCmosSensor(0xb5, 0x00); //aInBgTgtOfs_a03_n00
        HI351WriteCmosSensor(0xb6, 0x00); //aInBgTgtOfs_a04_n00
        HI351WriteCmosSensor(0xb7, 0x00); //aInBgTgtOfs_a05_n00
        
        HI351WriteCmosSensor(0xb8, 0x00); //aInBgTgtOfs_a06_n00
        HI351WriteCmosSensor(0xb9, 0x02); //aInBgTgtOfs_a07_n00           
        HI351WriteCmosSensor(0xba, 0x00);
        HI351WriteCmosSensor(0xbb, 0x00);
        HI351WriteCmosSensor(0xbc, 0x00);
        HI351WriteCmosSensor(0xbd, 0x00);
        HI351WriteCmosSensor(0xbe, 0x00);
        HI351WriteCmosSensor(0xbf, 0x00);
                       
        HI351WriteCmosSensor(0xc0, 0x00);
        HI351WriteCmosSensor(0xc1, 0x00);
        HI351WriteCmosSensor(0xc2, 0x00);
        HI351WriteCmosSensor(0xc3, 0x00);
        HI351WriteCmosSensor(0xc4, 0x00);
        HI351WriteCmosSensor(0xc5, 0x00);
        HI351WriteCmosSensor(0xc6, 0x00);
        HI351WriteCmosSensor(0xc7, 0x00);
                       
        HI351WriteCmosSensor(0xc8, 0x00);
        HI351WriteCmosSensor(0xc9, 0x00);
        HI351WriteCmosSensor(0xca, 0x00);
        HI351WriteCmosSensor(0xcb, 0x00);
        HI351WriteCmosSensor(0xcc, 0x00);
        HI351WriteCmosSensor(0xcd, 0x00);
        HI351WriteCmosSensor(0xce, 0x00);
        HI351WriteCmosSensor(0xcf, 0x00);
                       
        HI351WriteCmosSensor(0xd0, 0x00);
        HI351WriteCmosSensor(0xd1, 0x00);
        HI351WriteCmosSensor(0xd2, 0x0a);
        HI351WriteCmosSensor(0xd3, 0x0e);
        HI351WriteCmosSensor(0xd4, 0x14);
        HI351WriteCmosSensor(0xd5, 0x1e);
        HI351WriteCmosSensor(0xd6, 0x28);
        HI351WriteCmosSensor(0xd7, 0x22);
                       
        HI351WriteCmosSensor(0xd8, 0x1e);
        HI351WriteCmosSensor(0xd9, 0x1b);
        HI351WriteCmosSensor(0xda, 0x18);
        HI351WriteCmosSensor(0xdb, 0x14);
        HI351WriteCmosSensor(0xdc, 0x10);
        HI351WriteCmosSensor(0xdd, 0x0d);
        HI351WriteCmosSensor(0xde, 0x0a);
        HI351WriteCmosSensor(0xdf, 0x0a);
                       
        HI351WriteCmosSensor(0xe0, 0x0a);
        HI351WriteCmosSensor(0xe1, 0x0a);
        HI351WriteCmosSensor(0xe2, 0x28);
        HI351WriteCmosSensor(0xe3, 0x28);
        HI351WriteCmosSensor(0xe4, 0x28);
        HI351WriteCmosSensor(0xe5, 0x28);
        HI351WriteCmosSensor(0xe6, 0x28);
        HI351WriteCmosSensor(0xe7, 0x24);
                       
        HI351WriteCmosSensor(0xe8, 0x20);
        HI351WriteCmosSensor(0xe9, 0x1c);
        HI351WriteCmosSensor(0xea, 0x18);
        HI351WriteCmosSensor(0xeb, 0x14);
        HI351WriteCmosSensor(0xec, 0x14);
        HI351WriteCmosSensor(0xed, 0x0a);
        HI351WriteCmosSensor(0xee, 0x0a);
        HI351WriteCmosSensor(0xef, 0x0a);
                       
        HI351WriteCmosSensor(0xf0, 0x0a);
        HI351WriteCmosSensor(0xf1, 0x09);
        HI351WriteCmosSensor(0xf2, 0x08);
        HI351WriteCmosSensor(0xf3, 0x07);
        HI351WriteCmosSensor(0xf4, 0x07);
        HI351WriteCmosSensor(0xf5, 0x06);
        HI351WriteCmosSensor(0xf6, 0x06);
        HI351WriteCmosSensor(0xf7, 0x05);
                       
        HI351WriteCmosSensor(0xf8, 0x64); //aInHiTmpWgtRatio_a00_n00
        HI351WriteCmosSensor(0xf9, 0x20); //bInDyAglDiffMin_a00_n00
        HI351WriteCmosSensor(0xfa, 0xc0); //bInDyAglDiffMax_a00_n00
        HI351WriteCmosSensor(0xfb, 0x19); //bInDyMinMaxTempWgt_a00_n00
        HI351WriteCmosSensor(0xfc, 0xc8); //96 (100(96) -) 200(c8)deg  //bInSplTmpAgl_a00_n00
        HI351WriteCmosSensor(0xfd, 0x0a); //bInSplTmpAglOfs_a00_n00
        
            
        HI351WriteCmosSensor(0x03, 0xc6); 
        HI351WriteCmosSensor(0x10, 0x14); //bInSplTmpBpCntTh_a00_n00
        HI351WriteCmosSensor(0x11, 0x32); //bInSplTmpPtCorWgt_a00_n00
        HI351WriteCmosSensor(0x12, 0x1e); //bInSplTmpPtWgtRatio_a00_n00
        HI351WriteCmosSensor(0x13, 0x14); //bInSplTmpAglMinLmt_a00_n00
        HI351WriteCmosSensor(0x14, 0xb4); //bInSplTmpAglMaxLmt_a00_n00
        HI351WriteCmosSensor(0x15, 0x1e);
        HI351WriteCmosSensor(0x16, 0x04);
        HI351WriteCmosSensor(0x17, 0xf8);
                       
        HI351WriteCmosSensor(0x18, 0x40);
        HI351WriteCmosSensor(0x19, 0xf0); //bInRgainMax_a00_n00
        HI351WriteCmosSensor(0x1a, 0x40);
        HI351WriteCmosSensor(0x1b, 0xf0); //bInBgainMax_a00_n00
        HI351WriteCmosSensor(0x1c, 0x08);
        HI351WriteCmosSensor(0x1d, 0x00);
          HI351WriteCmosSensor(0x1e, 0x35); //aOutWhtRgnBg_a00_n00
          HI351WriteCmosSensor(0x1f, 0x3a); //aOutWhtRgnBg_a01_n00
          HI351WriteCmosSensor(0x20, 0x3f); //aOutWhtRgnBg_a02_n00
          HI351WriteCmosSensor(0x21, 0x43); //aOutWhtRgnBg_a03_n00
          HI351WriteCmosSensor(0x22, 0x49); //aOutWhtRgnBg_a04_n00
          HI351WriteCmosSensor(0x23, 0x4f); //aOutWhtRgnBg_a05_n00
          HI351WriteCmosSensor(0x24, 0x55); //aOutWhtRgnBg_a06_n00
          HI351WriteCmosSensor(0x25, 0x5e); //aOutWhtRgnBg_a07_n00
          HI351WriteCmosSensor(0x26, 0x66); //aOutWhtRgnBg_a08_n00
          HI351WriteCmosSensor(0x27, 0x6e); //aOutWhtRgnBg_a09_n00
          HI351WriteCmosSensor(0x28, 0x78); //aOutWhtRgnBg_a10_n00
          HI351WriteCmosSensor(0x29, 0x5f); //aOutWhtRgnRgLeftLmt_a00_n00
          HI351WriteCmosSensor(0x2a, 0x5a); //aOutWhtRgnRgLeftLmt_a01_n00
          HI351WriteCmosSensor(0x2b, 0x54); //aOutWhtRgnRgLeftLmt_a02_n00
          HI351WriteCmosSensor(0x2c, 0x4e); //aOutWhtRgnRgLeftLmt_a03_n00
          HI351WriteCmosSensor(0x2d, 0x4b); //aOutWhtRgnRgLeftLmt_a04_n00
          HI351WriteCmosSensor(0x2e, 0x46); //aOutWhtRgnRgLeftLmt_a05_n00
          HI351WriteCmosSensor(0x2f, 0x43); //aOutWhtRgnRgLeftLmt_a06_n00
          HI351WriteCmosSensor(0x30, 0x3e); //aOutWhtRgnRgLeftLmt_a07_n00
          HI351WriteCmosSensor(0x31, 0x3d); //aOutWhtRgnRgLeftLmt_a08_n00
          HI351WriteCmosSensor(0x32, 0x3c); //aOutWhtRgnRgLeftLmt_a09_n00
          HI351WriteCmosSensor(0x33, 0x3b); //aOutWhtRgnRgLeftLmt_a10_n00
          HI351WriteCmosSensor(0x34, 0x6f); //aOutWhtRgnRgRightLmt_a00_n00
          HI351WriteCmosSensor(0x35, 0x6b); //aOutWhtRgnRgRightLmt_a01_n00
          HI351WriteCmosSensor(0x36, 0x68); //aOutWhtRgnRgRightLmt_a02_n00
          HI351WriteCmosSensor(0x37, 0x63); //aOutWhtRgnRgRightLmt_a03_n00
          HI351WriteCmosSensor(0x38, 0x60); //aOutWhtRgnRgRightLmt_a04_n00
          HI351WriteCmosSensor(0x39, 0x5c); //aOutWhtRgnRgRightLmt_a05_n00
          HI351WriteCmosSensor(0x3a, 0x58); //aOutWhtRgnRgRightLmt_a06_n00
          HI351WriteCmosSensor(0x3b, 0x53); //aOutWhtRgnRgRightLmt_a07_n00
          HI351WriteCmosSensor(0x3c, 0x4f); //aOutWhtRgnRgRightLmt_a08_n00
          HI351WriteCmosSensor(0x3d, 0x4c); //aOutWhtRgnRgRightLmt_a09_n00
          HI351WriteCmosSensor(0x3e, 0x4a); //aOutWhtRgnRgRightLmt_a10_n00
          HI351WriteCmosSensor(0x3f, 0x34); //aOutWhtLineBg_a00_n00
          HI351WriteCmosSensor(0x40, 0x3b); //aOutWhtLineBg_a01_n00
          HI351WriteCmosSensor(0x41, 0x41); //aOutWhtLineBg_a02_n00
          HI351WriteCmosSensor(0x42, 0x46); //aOutWhtLineBg_a03_n00
          HI351WriteCmosSensor(0x43, 0x4b); //aOutWhtLineBg_a04_n00
          HI351WriteCmosSensor(0x44, 0x51); //aOutWhtLineBg_a05_n00
          HI351WriteCmosSensor(0x45, 0x56); //aOutWhtLineBg_a06_n00
          HI351WriteCmosSensor(0x46, 0x5f); //aOutWhtLineBg_a07_n00
          HI351WriteCmosSensor(0x47, 0x6a); //aOutWhtLineBg_a08_n00
          HI351WriteCmosSensor(0x48, 0x71); //aOutWhtLineBg_a09_n00
          HI351WriteCmosSensor(0x49, 0x78); //aOutWhtLineBg_a10_n00
          HI351WriteCmosSensor(0x4a, 0x66); //aOutWhtLineRg_a00_n00
          HI351WriteCmosSensor(0x4b, 0x61); //aOutWhtLineRg_a01_n00
          HI351WriteCmosSensor(0x4c, 0x5c); //aOutWhtLineRg_a02_n00
          HI351WriteCmosSensor(0x4d, 0x59); //aOutWhtLineRg_a03_n00
          HI351WriteCmosSensor(0x4e, 0x55); //aOutWhtLineRg_a04_n00
          HI351WriteCmosSensor(0x4f, 0x51); //aOutWhtLineRg_a05_n00
          HI351WriteCmosSensor(0x50, 0x4e); //aOutWhtLineRg_a06_n00
          HI351WriteCmosSensor(0x51, 0x49); //aOutWhtLineRg_a07_n00
          HI351WriteCmosSensor(0x52, 0x44); //aOutWhtLineRg_a08_n00
          HI351WriteCmosSensor(0x53, 0x43); //aOutWhtLineRg_a09_n00
          HI351WriteCmosSensor(0x54, 0x43); //aOutWhtLineRg_a10_n00
          HI351WriteCmosSensor(0x55, 0x50); //aOutTgtAngle_a00_n00
          HI351WriteCmosSensor(0x56, 0x5a); //aOutTgtAngle_a01_n00
          HI351WriteCmosSensor(0x57, 0x64); //aOutTgtAngle_a02_n00
                       
        HI351WriteCmosSensor(0x58, 0x6e);
        HI351WriteCmosSensor(0x59, 0x78);
        HI351WriteCmosSensor(0x5a, 0x82);
        HI351WriteCmosSensor(0x5b, 0x8c);
        HI351WriteCmosSensor(0x5c, 0x96);
        HI351WriteCmosSensor(0x5d, 0x00);
        HI351WriteCmosSensor(0x5e, 0x00);
        HI351WriteCmosSensor(0x5f, 0x00);
        
        HI351WriteCmosSensor(0x60, 0x82); //aOutRgTgtOfs_a03_n00
        HI351WriteCmosSensor(0x61, 0x84); //aOutRgTgtOfs_a04_n00
        HI351WriteCmosSensor(0x62, 0x86); //aOutRgTgtOfs_a05_n00
        HI351WriteCmosSensor(0x63, 0x00);
        HI351WriteCmosSensor(0x64, 0x00);
        HI351WriteCmosSensor(0x65, 0x00);
        HI351WriteCmosSensor(0x66, 0x00);
        HI351WriteCmosSensor(0x67, 0x00);
        
        HI351WriteCmosSensor(0x68, 0x02); //aOutBgTgtOfs_a03_n00
        HI351WriteCmosSensor(0x69, 0x04); //aOutBgTgtOfs_a04_n00
        HI351WriteCmosSensor(0x6a, 0x06); //aOutBgTgtOfs_a05_n00
        HI351WriteCmosSensor(0x6b, 0x00);
        HI351WriteCmosSensor(0x6c, 0x00);
        HI351WriteCmosSensor(0x6d, 0x00);
        HI351WriteCmosSensor(0x6e, 0x00);
        HI351WriteCmosSensor(0x6f, 0x00);
                       
        HI351WriteCmosSensor(0x70, 0x00);
        HI351WriteCmosSensor(0x71, 0x00);
        HI351WriteCmosSensor(0x72, 0x00);
        HI351WriteCmosSensor(0x73, 0x00);
        HI351WriteCmosSensor(0x74, 0x00);
        HI351WriteCmosSensor(0x75, 0x00);
        HI351WriteCmosSensor(0x76, 0x00);
        HI351WriteCmosSensor(0x77, 0x00);
                       
        HI351WriteCmosSensor(0x78, 0x00);
        HI351WriteCmosSensor(0x79, 0x00);
        HI351WriteCmosSensor(0x7a, 0x00);
        HI351WriteCmosSensor(0x7b, 0x00);
        HI351WriteCmosSensor(0x7c, 0x00);
        HI351WriteCmosSensor(0x7d, 0x00);
        HI351WriteCmosSensor(0x7e, 0x00);
        HI351WriteCmosSensor(0x7f, 0x00);
                       
        HI351WriteCmosSensor(0x80, 0x00);
        HI351WriteCmosSensor(0x81, 0x00);
        HI351WriteCmosSensor(0x82, 0x00);
        HI351WriteCmosSensor(0x83, 0x00);
        HI351WriteCmosSensor(0x84, 0x00);
        HI351WriteCmosSensor(0x85, 0x0a);
        HI351WriteCmosSensor(0x86, 0x0a);
        HI351WriteCmosSensor(0x87, 0x0a);
                       
        HI351WriteCmosSensor(0x88, 0x0a);
        HI351WriteCmosSensor(0x89, 0x0a);
        HI351WriteCmosSensor(0x8a, 0x0a);
        HI351WriteCmosSensor(0x8b, 0x0a);
        HI351WriteCmosSensor(0x8c, 0x28);
        HI351WriteCmosSensor(0x8d, 0x28);
        HI351WriteCmosSensor(0x8e, 0x28);
        HI351WriteCmosSensor(0x8f, 0x28);
                       
        HI351WriteCmosSensor(0x90, 0x1e);
        HI351WriteCmosSensor(0x91, 0x1e);
        HI351WriteCmosSensor(0x92, 0x0a);
        HI351WriteCmosSensor(0x93, 0x0a);
        HI351WriteCmosSensor(0x94, 0x0a);
        HI351WriteCmosSensor(0x95, 0x20); //aOutHiTmpWgtHiLmt_a00_n00
        HI351WriteCmosSensor(0x96, 0x1e); //01
        HI351WriteCmosSensor(0x97, 0x1c); //02
        HI351WriteCmosSensor(0x98, 0x1e); //03
        HI351WriteCmosSensor(0x99, 0x1e); //04
        HI351WriteCmosSensor(0x9a, 0x20); //05
        HI351WriteCmosSensor(0x9b, 0x23); //06
        HI351WriteCmosSensor(0x9c, 0x24); //07
        HI351WriteCmosSensor(0x9d, 0x27); //08
        HI351WriteCmosSensor(0x9e, 0x28); //09
        HI351WriteCmosSensor(0x9f, 0x29); //10
                       
        HI351WriteCmosSensor(0xa0, 0x08); //aOutHiTmpWgtLoLmt_a00_n00
        HI351WriteCmosSensor(0xa1, 0x08); //01
        HI351WriteCmosSensor(0xa2, 0x08); //02
        HI351WriteCmosSensor(0xa3, 0x0d); //03
        HI351WriteCmosSensor(0xa4, 0x10); //04
        HI351WriteCmosSensor(0xa5, 0x12); //05
        HI351WriteCmosSensor(0xa6, 0x12); //06
        HI351WriteCmosSensor(0xa7, 0x12); //07
        HI351WriteCmosSensor(0xa8, 0x13); //08
        HI351WriteCmosSensor(0xa9, 0x13); //09
        HI351WriteCmosSensor(0xaa, 0x14); //10
        HI351WriteCmosSensor(0xab, 0x64);
        HI351WriteCmosSensor(0xac, 0x01);
        HI351WriteCmosSensor(0xad, 0x14);
        HI351WriteCmosSensor(0xae, 0x19);
        HI351WriteCmosSensor(0xaf, 0x64);//kjh out limit 64 -) 76 sky
                       
        HI351WriteCmosSensor(0xb0, 0x14);
        HI351WriteCmosSensor(0xb1, 0x1e);
        HI351WriteCmosSensor(0xb2, 0x20); //50 -) 20 sky outdoor 
        HI351WriteCmosSensor(0xb3, 0x32); //1e -) 32(50%)
        HI351WriteCmosSensor(0xb4, 0x14);
        HI351WriteCmosSensor(0xb5, 0x3c);
        HI351WriteCmosSensor(0xb6, 0x1e);
        HI351WriteCmosSensor(0xb7, 0x08);
                       
        HI351WriteCmosSensor(0xb8, 0xd2);
        HI351WriteCmosSensor(0xb9, 0x50); // steve OutRgainMin  
        HI351WriteCmosSensor(0xba, 0xf0); // steve OutRgainMax  
        HI351WriteCmosSensor(0xbb, 0x40); // steve OutBgainMin  
        HI351WriteCmosSensor(0xbc, 0x80); // steve OutBgainMax  
            
        HI351WriteCmosSensor(0xbd, 0x04);
        HI351WriteCmosSensor(0xbe, 0x00);
        HI351WriteCmosSensor(0xbf, 0xcd);
            
            ///////////////////////////////////////////
            // CD Page (Color ratio)
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xCD);
        HI351WriteCmosSensor(0x47, 0x06);
        HI351WriteCmosSensor(0x10, 0x38); //STEVE B8 -) 38 disable
            
            ///////////////////////////////////////////
            //Adaptive mode : Page Mode = 0xCF
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xcf);
            
        HI351WriteCmosSensor(0x10, 0x00);
        HI351WriteCmosSensor(0x11, 0x84); // STEVE 04 -) 84  //cmc + - , adaptive lsc
        HI351WriteCmosSensor(0x12, 0x01);
                       
        HI351WriteCmosSensor(0x13, 0x01);//Y_LUM_MAX 10fps, AG 0xA0
        HI351WriteCmosSensor(0x14, 0x01);
        HI351WriteCmosSensor(0x15, 0x7d);
        HI351WriteCmosSensor(0x16, 0xf8);
                       
        HI351WriteCmosSensor(0x17, 0x00);  //Y_LUM middle 1 //72mhz 14.58fps, AG 0x4c 
        HI351WriteCmosSensor(0x18, 0x3d);  
        HI351WriteCmosSensor(0x19, 0x39);  
        HI351WriteCmosSensor(0x1a, 0xd4);  
                       
        HI351WriteCmosSensor(0x1b, 0x00);  //Y_LUM middle 2 //72mhz 120fps, AG 0x30  0.5(0x10) x 1(0x80) = 10,000(0x0186a0)
        HI351WriteCmosSensor(0x1c, 0x06);  
        HI351WriteCmosSensor(0x1d, 0xdd);  
        HI351WriteCmosSensor(0x1e, 0xd0);  
                       
        HI351WriteCmosSensor(0x1f, 0x00);  //Y_LUM min //72mhz 6000fps,AG 0x30  
        HI351WriteCmosSensor(0x20, 0x00);
        HI351WriteCmosSensor(0x21, 0x1d);
        HI351WriteCmosSensor(0x22, 0x4c);
                       
        HI351WriteCmosSensor(0x23, 0x9a);  //CTEM high
        HI351WriteCmosSensor(0x24, 0x54);  //ctemp middler
        HI351WriteCmosSensor(0x25, 0x35);  //CTEM low 
                       
        HI351WriteCmosSensor(0x26, 0x50);  //YCON high   
        HI351WriteCmosSensor(0x27, 0x20);  //YCON middle 
        HI351WriteCmosSensor(0x28, 0x01);  //YCON low    
                       
        HI351WriteCmosSensor(0x29, 0x00); //Y_LUM max_TH 
        HI351WriteCmosSensor(0x2a, 0x00);
        HI351WriteCmosSensor(0x2b, 0x00);
        HI351WriteCmosSensor(0x2c, 0x00);
                       
        HI351WriteCmosSensor(0x2d, 0x00);  //Y_LUM middle1_TH
        HI351WriteCmosSensor(0x2e, 0x00);
        HI351WriteCmosSensor(0x2f, 0x00);
        HI351WriteCmosSensor(0x30, 0x00);
                       
        HI351WriteCmosSensor(0x31, 0x00);  //Y_LUM middle_TH
        HI351WriteCmosSensor(0x32, 0x00);
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x34, 0x00);
                       
        HI351WriteCmosSensor(0x35, 0x00); //Y_LUM min_TH
        HI351WriteCmosSensor(0x36, 0x00);
        HI351WriteCmosSensor(0x37, 0x00);
        HI351WriteCmosSensor(0x38, 0x00);
                       
        HI351WriteCmosSensor(0x39, 0x00);  //CTEM high_TH  
        HI351WriteCmosSensor(0x3a, 0x00);  //CTEM middle_TH
        HI351WriteCmosSensor(0x3b, 0x00); //CTEM low_TH
                       
        HI351WriteCmosSensor(0x3c, 0x00); //YCON high_TH
        HI351WriteCmosSensor(0x3d, 0x00); //YCON middle_TH
        HI351WriteCmosSensor(0x3e, 0x00); //YCON low_TH
                       
            /////////////BURST_TYPE///////////////////////
            // CF Page Adaptive Y Target
            /////////////BURST_TYPE//////////////////////
                       
        HI351WriteCmosSensor(0x3f, 0x38);  //YLVL_00
        HI351WriteCmosSensor(0x40, 0x38);  //YLVL_01
        HI351WriteCmosSensor(0x41, 0x38);  //YLVL_02
        HI351WriteCmosSensor(0x42, 0x36);  //YLVL_03
        HI351WriteCmosSensor(0x43, 0x36);  //YLVL_04
        HI351WriteCmosSensor(0x44, 0x36);  //YLVL_05
        HI351WriteCmosSensor(0x45, 0x36);  //YLVL_06     
        HI351WriteCmosSensor(0x46, 0x36);  //YLVL_07     
        HI351WriteCmosSensor(0x47, 0x36);  //YLVL_08     
        HI351WriteCmosSensor(0x48, 0x34);  //YLVL_09     
        HI351WriteCmosSensor(0x49, 0x34);  //YLVL_10     
        HI351WriteCmosSensor(0x4a, 0x34);  //36 YLVL_11
                       
        HI351WriteCmosSensor(0x4b, 0x80);  //YCON_00
        HI351WriteCmosSensor(0x4c, 0x80);  //YCON_01
        HI351WriteCmosSensor(0x4d, 0x80);  //YCON_02
        HI351WriteCmosSensor(0x4e, 0x80);  //Contrast 3 
        HI351WriteCmosSensor(0x4f, 0x80);  //Contrast 4 
        HI351WriteCmosSensor(0x50, 0x80);  //Contrast 5 
        HI351WriteCmosSensor(0x51, 0x80);  //Contrast 6 
        HI351WriteCmosSensor(0x52, 0x80);  //Contrast 7 
        HI351WriteCmosSensor(0x53, 0x80);  //Contrast 8 
        HI351WriteCmosSensor(0x54, 0x80);  //Contrast 9 
        HI351WriteCmosSensor(0x55, 0x80);  //Contrast 10
        HI351WriteCmosSensor(0x56, 0x80);  //Contrast 11
                       
            /////////////BURST_TYPE//////////////////////
            // CF Page AdBURST_TYPE OFFSET
            /////////////BURST_TYPE//////////////////////
                       
        HI351WriteCmosSensor(0x57, 0x08); // dark offset for noise 
        HI351WriteCmosSensor(0x58, 0x08);
        HI351WriteCmosSensor(0x59, 0x08);
        HI351WriteCmosSensor(0x5a, 0x00);
        HI351WriteCmosSensor(0x5b, 0x00);
        HI351WriteCmosSensor(0x5c, 0x00);
        HI351WriteCmosSensor(0x5d, 0x00);
        HI351WriteCmosSensor(0x5e, 0x00);
        HI351WriteCmosSensor(0x5f, 0x00);
                       
        HI351WriteCmosSensor(0x60, 0x80);
        HI351WriteCmosSensor(0x61, 0x80);
        HI351WriteCmosSensor(0x62, 0x80);  
            /////////////BURST_TYPE//////////////////////
            // CF~D0~D1 PBURST_TYPEtive GAMMA
            /////////////BURST_TYPE//////////////////////
                       
                       
        HI351WriteCmosSensor(0x63, 0x00);//GMA00
        HI351WriteCmosSensor(0x64, 0x07);
        HI351WriteCmosSensor(0x65, 0x0E);
        HI351WriteCmosSensor(0x66, 0x19);
        HI351WriteCmosSensor(0x67, 0x20);
        HI351WriteCmosSensor(0x68, 0x2E);
        HI351WriteCmosSensor(0x69, 0x3B);
        HI351WriteCmosSensor(0x6a, 0x45);
        HI351WriteCmosSensor(0x6b, 0x4D);
        HI351WriteCmosSensor(0x6c, 0x54);
        HI351WriteCmosSensor(0x6d, 0x5A);
        HI351WriteCmosSensor(0x6e, 0x63);
        HI351WriteCmosSensor(0x6f, 0x69);
        HI351WriteCmosSensor(0x70, 0x70);
        HI351WriteCmosSensor(0x71, 0x76);
        HI351WriteCmosSensor(0x72, 0x7C);
        HI351WriteCmosSensor(0x73, 0x81);
        HI351WriteCmosSensor(0x74, 0x86);
        HI351WriteCmosSensor(0x75, 0x8C);
        HI351WriteCmosSensor(0x76, 0x90);
        HI351WriteCmosSensor(0x77, 0x95);
        HI351WriteCmosSensor(0x78, 0x9E);
        HI351WriteCmosSensor(0x79, 0xA7);
        HI351WriteCmosSensor(0x7a, 0xAF);
        HI351WriteCmosSensor(0x7b, 0xBD);
        HI351WriteCmosSensor(0x7c, 0xC9);
        HI351WriteCmosSensor(0x7d, 0xD4);
        HI351WriteCmosSensor(0x7e, 0xDD);
        HI351WriteCmosSensor(0x7f, 0xE5);
        HI351WriteCmosSensor(0x80, 0xEB);
        HI351WriteCmosSensor(0x81, 0xF1);
        HI351WriteCmosSensor(0x82, 0xF6);
        HI351WriteCmosSensor(0x83, 0xFB);
        HI351WriteCmosSensor(0x84, 0xFF);
                       
        HI351WriteCmosSensor(0x85, 0x00);//GMA01
        HI351WriteCmosSensor(0x86, 0x07);
        HI351WriteCmosSensor(0x87, 0x0E);
        HI351WriteCmosSensor(0x88, 0x19);
        HI351WriteCmosSensor(0x89, 0x20);
        HI351WriteCmosSensor(0x8a, 0x2E);
        HI351WriteCmosSensor(0x8b, 0x3B);
        HI351WriteCmosSensor(0x8c, 0x45);
        HI351WriteCmosSensor(0x8d, 0x4D);
        HI351WriteCmosSensor(0x8e, 0x54);
        HI351WriteCmosSensor(0x8f, 0x5A);
        HI351WriteCmosSensor(0x90, 0x63);
        HI351WriteCmosSensor(0x91, 0x69);
        HI351WriteCmosSensor(0x92, 0x70);
        HI351WriteCmosSensor(0x93, 0x76);
        HI351WriteCmosSensor(0x94, 0x7C);
        HI351WriteCmosSensor(0x95, 0x81);
        HI351WriteCmosSensor(0x96, 0x86);
        HI351WriteCmosSensor(0x97, 0x8C);
        HI351WriteCmosSensor(0x98, 0x90);
        HI351WriteCmosSensor(0x99, 0x95);
        HI351WriteCmosSensor(0x9a, 0x9E);
        HI351WriteCmosSensor(0x9b, 0xA7);
        HI351WriteCmosSensor(0x9c, 0xAF);
        HI351WriteCmosSensor(0x9d, 0xBD);
        HI351WriteCmosSensor(0x9e, 0xC9);
        HI351WriteCmosSensor(0x9f, 0xD4);
        HI351WriteCmosSensor(0xa0, 0xDD);
        HI351WriteCmosSensor(0xa1, 0xE5);
        HI351WriteCmosSensor(0xa2, 0xEB);
        HI351WriteCmosSensor(0xa3, 0xF1);
        HI351WriteCmosSensor(0xa4, 0xF6);
        HI351WriteCmosSensor(0xa5, 0xFB);
        HI351WriteCmosSensor(0xa6, 0xFF);
                       
        HI351WriteCmosSensor(0xa7, 0x00);//GMA02
        HI351WriteCmosSensor(0xa8, 0x07);
        HI351WriteCmosSensor(0xa9, 0x0E);
        HI351WriteCmosSensor(0xaa, 0x19);
        HI351WriteCmosSensor(0xab, 0x20);
        HI351WriteCmosSensor(0xac, 0x2E);
        HI351WriteCmosSensor(0xad, 0x3B);
        HI351WriteCmosSensor(0xae, 0x45);
        HI351WriteCmosSensor(0xaf, 0x4D);
        HI351WriteCmosSensor(0xb0, 0x54);
        HI351WriteCmosSensor(0xb1, 0x5A);
        HI351WriteCmosSensor(0xb2, 0x63);
        HI351WriteCmosSensor(0xb3, 0x69);
        HI351WriteCmosSensor(0xb4, 0x70);
        HI351WriteCmosSensor(0xb5, 0x76);
        HI351WriteCmosSensor(0xb6, 0x7C);
        HI351WriteCmosSensor(0xb7, 0x81);
        HI351WriteCmosSensor(0xb8, 0x86);
        HI351WriteCmosSensor(0xb9, 0x8C);
        HI351WriteCmosSensor(0xba, 0x90);
        HI351WriteCmosSensor(0xbb, 0x95);
        HI351WriteCmosSensor(0xbc, 0x9E);
        HI351WriteCmosSensor(0xbd, 0xA7);
        HI351WriteCmosSensor(0xbe, 0xAF);
        HI351WriteCmosSensor(0xbf, 0xBD);
        HI351WriteCmosSensor(0xc0, 0xC9);
        HI351WriteCmosSensor(0xc1, 0xD4);
        HI351WriteCmosSensor(0xc2, 0xDD);
        HI351WriteCmosSensor(0xc3, 0xE5);
        HI351WriteCmosSensor(0xc4, 0xEB);
        HI351WriteCmosSensor(0xc5, 0xF1);
        HI351WriteCmosSensor(0xc6, 0xF6);
        HI351WriteCmosSensor(0xc7, 0xFB);
        HI351WriteCmosSensor(0xc8, 0xFF);
                       
        HI351WriteCmosSensor(0xc9, 0x00);//GMA03
        HI351WriteCmosSensor(0xca, 0x03);
        HI351WriteCmosSensor(0xcb, 0x08);
        HI351WriteCmosSensor(0xcc, 0x12);
        HI351WriteCmosSensor(0xcd, 0x19);
        HI351WriteCmosSensor(0xce, 0x25);
        HI351WriteCmosSensor(0xcf, 0x32);
        HI351WriteCmosSensor(0xd0, 0x3E);
        HI351WriteCmosSensor(0xd1, 0x4B);
        HI351WriteCmosSensor(0xd2, 0x56);
        HI351WriteCmosSensor(0xd3, 0x62);
        HI351WriteCmosSensor(0xd4, 0x6A);
        HI351WriteCmosSensor(0xd5, 0x71);
        HI351WriteCmosSensor(0xd6, 0x78);
        HI351WriteCmosSensor(0xd7, 0x7F);
        HI351WriteCmosSensor(0xd8, 0x85);
        HI351WriteCmosSensor(0xd9, 0x8A);
        HI351WriteCmosSensor(0xda, 0x90);
        HI351WriteCmosSensor(0xdb, 0x95);
        HI351WriteCmosSensor(0xdc, 0x9A);
        HI351WriteCmosSensor(0xdd, 0x9F);
        HI351WriteCmosSensor(0xde, 0xA9);
        HI351WriteCmosSensor(0xdf, 0xB1);
        HI351WriteCmosSensor(0xe0, 0xB9);
        HI351WriteCmosSensor(0xe1, 0xC6);
        HI351WriteCmosSensor(0xe2, 0xD0);
        HI351WriteCmosSensor(0xe3, 0xD8);
        HI351WriteCmosSensor(0xe4, 0xDF);
        HI351WriteCmosSensor(0xe5, 0xE6);
        HI351WriteCmosSensor(0xe6, 0xEC);
        HI351WriteCmosSensor(0xe7, 0xF1);
        HI351WriteCmosSensor(0xe8, 0xF7);
        HI351WriteCmosSensor(0xe9, 0xFB);
        HI351WriteCmosSensor(0xea, 0xFF);
                       
        HI351WriteCmosSensor(0xeb, 0x00);//GMA04 
        HI351WriteCmosSensor(0xec, 0x03);         
        HI351WriteCmosSensor(0xed, 0x08);         
        HI351WriteCmosSensor(0xee, 0x12);         
        HI351WriteCmosSensor(0xef, 0x19);         
        HI351WriteCmosSensor(0xf0, 0x25);         
        HI351WriteCmosSensor(0xf1, 0x32);         
        HI351WriteCmosSensor(0xf2, 0x3E);         
        HI351WriteCmosSensor(0xf3, 0x4B);         
        HI351WriteCmosSensor(0xf4, 0x56);         
        HI351WriteCmosSensor(0xf5, 0x62);         
        HI351WriteCmosSensor(0xf6, 0x6A);         
        HI351WriteCmosSensor(0xf7, 0x71);         
        HI351WriteCmosSensor(0xf8, 0x78);         
        HI351WriteCmosSensor(0xf9, 0x7F);         
        HI351WriteCmosSensor(0xfa, 0x85);         
        HI351WriteCmosSensor(0xfb, 0x8A);         
        HI351WriteCmosSensor(0xfc, 0x90);         
        HI351WriteCmosSensor(0xfd, 0x95);         
        HI351WriteCmosSensor(0x03, 0xd0);//Page d0  
        HI351WriteCmosSensor(0x10, 0x9A);         
        HI351WriteCmosSensor(0x11, 0x9F);         
        HI351WriteCmosSensor(0x12, 0xA9);         
        HI351WriteCmosSensor(0x13, 0xB1);         
        HI351WriteCmosSensor(0x14, 0xB9);         
        HI351WriteCmosSensor(0x15, 0xC6);         
        HI351WriteCmosSensor(0x16, 0xD0);         
        HI351WriteCmosSensor(0x17, 0xD8);         
        HI351WriteCmosSensor(0x18, 0xDF);         
        HI351WriteCmosSensor(0x19, 0xE6);         
        HI351WriteCmosSensor(0x1a, 0xEC);         
        HI351WriteCmosSensor(0x1b, 0xF1);         
        HI351WriteCmosSensor(0x1c, 0xF7);         
        HI351WriteCmosSensor(0x1d, 0xFB);         
        HI351WriteCmosSensor(0x1e, 0xFF);
                       
        HI351WriteCmosSensor(0x1f, 0x00);//GMA05
        HI351WriteCmosSensor(0x20, 0x03);
        HI351WriteCmosSensor(0x21, 0x08);
        HI351WriteCmosSensor(0x22, 0x12);
        HI351WriteCmosSensor(0x23, 0x19);
        HI351WriteCmosSensor(0x24, 0x25);
        HI351WriteCmosSensor(0x25, 0x32);
        HI351WriteCmosSensor(0x26, 0x3E);
        HI351WriteCmosSensor(0x27, 0x4B);
        HI351WriteCmosSensor(0x28, 0x56);
        HI351WriteCmosSensor(0x29, 0x62);
        HI351WriteCmosSensor(0x2a, 0x6A);
        HI351WriteCmosSensor(0x2b, 0x71);
        HI351WriteCmosSensor(0x2c, 0x78);
        HI351WriteCmosSensor(0x2d, 0x7F);
        HI351WriteCmosSensor(0x2e, 0x85);
        HI351WriteCmosSensor(0x2f, 0x8A);
        HI351WriteCmosSensor(0x30, 0x90);
        HI351WriteCmosSensor(0x31, 0x95);
        HI351WriteCmosSensor(0x32, 0x9A);
        HI351WriteCmosSensor(0x33, 0x9F);
        HI351WriteCmosSensor(0x34, 0xA9);
        HI351WriteCmosSensor(0x35, 0xB1);
        HI351WriteCmosSensor(0x36, 0xB9);
        HI351WriteCmosSensor(0x37, 0xC6);
        HI351WriteCmosSensor(0x38, 0xD0);
        HI351WriteCmosSensor(0x39, 0xD8);
        HI351WriteCmosSensor(0x3a, 0xDF);
        HI351WriteCmosSensor(0x3b, 0xE6);
        HI351WriteCmosSensor(0x3c, 0xEC);
        HI351WriteCmosSensor(0x3d, 0xF1);
        HI351WriteCmosSensor(0x3e, 0xF7);
        HI351WriteCmosSensor(0x3f, 0xFB);
        HI351WriteCmosSensor(0x40, 0xFF);
                       
        HI351WriteCmosSensor(0x41, 0x00);//GMA06
        HI351WriteCmosSensor(0x42, 0x03);
        HI351WriteCmosSensor(0x43, 0x08);
        HI351WriteCmosSensor(0x44, 0x12);
        HI351WriteCmosSensor(0x45, 0x19);
        HI351WriteCmosSensor(0x46, 0x25);
        HI351WriteCmosSensor(0x47, 0x32);
        HI351WriteCmosSensor(0x48, 0x3E);
        HI351WriteCmosSensor(0x49, 0x4B);
        HI351WriteCmosSensor(0x4a, 0x56);
        HI351WriteCmosSensor(0x4b, 0x62);
        HI351WriteCmosSensor(0x4c, 0x6A);
        HI351WriteCmosSensor(0x4d, 0x71);
        HI351WriteCmosSensor(0x4e, 0x78);
        HI351WriteCmosSensor(0x4f, 0x7F);
        HI351WriteCmosSensor(0x50, 0x85);
        HI351WriteCmosSensor(0x51, 0x8A);
        HI351WriteCmosSensor(0x52, 0x90);
        HI351WriteCmosSensor(0x53, 0x95);
        HI351WriteCmosSensor(0x54, 0x9A);
        HI351WriteCmosSensor(0x55, 0x9F);
        HI351WriteCmosSensor(0x56, 0xA9);
        HI351WriteCmosSensor(0x57, 0xB1);
        HI351WriteCmosSensor(0x58, 0xB9);
        HI351WriteCmosSensor(0x59, 0xC6);
        HI351WriteCmosSensor(0x5a, 0xD0);
        HI351WriteCmosSensor(0x5b, 0xD8);
        HI351WriteCmosSensor(0x5c, 0xDF);
        HI351WriteCmosSensor(0x5d, 0xE6);
        HI351WriteCmosSensor(0x5e, 0xEC);
        HI351WriteCmosSensor(0x5f, 0xF1);
        HI351WriteCmosSensor(0x60, 0xF7);
        HI351WriteCmosSensor(0x61, 0xFB);
        HI351WriteCmosSensor(0x62, 0xFF);
                       
        HI351WriteCmosSensor(0x63, 0x00);//GMA07
        HI351WriteCmosSensor(0x64, 0x03);
        HI351WriteCmosSensor(0x65, 0x08);
        HI351WriteCmosSensor(0x66, 0x12);
        HI351WriteCmosSensor(0x67, 0x19);
        HI351WriteCmosSensor(0x68, 0x25);
        HI351WriteCmosSensor(0x69, 0x32);
        HI351WriteCmosSensor(0x6a, 0x3E);
        HI351WriteCmosSensor(0x6b, 0x4B);
        HI351WriteCmosSensor(0x6c, 0x56);
        HI351WriteCmosSensor(0x6d, 0x62);
        HI351WriteCmosSensor(0x6e, 0x6A);
        HI351WriteCmosSensor(0x6f, 0x71);
        HI351WriteCmosSensor(0x70, 0x78);
        HI351WriteCmosSensor(0x71, 0x7F);
        HI351WriteCmosSensor(0x72, 0x85);
        HI351WriteCmosSensor(0x73, 0x8A);
        HI351WriteCmosSensor(0x74, 0x90);
        HI351WriteCmosSensor(0x75, 0x95);
        HI351WriteCmosSensor(0x76, 0x9A);
        HI351WriteCmosSensor(0x77, 0x9F);
        HI351WriteCmosSensor(0x78, 0xA9);
        HI351WriteCmosSensor(0x79, 0xB1);
        HI351WriteCmosSensor(0x7a, 0xB9);
        HI351WriteCmosSensor(0x7b, 0xC6);
        HI351WriteCmosSensor(0x7c, 0xD0);
        HI351WriteCmosSensor(0x7d, 0xD8);
        HI351WriteCmosSensor(0x7e, 0xDF);
        HI351WriteCmosSensor(0x7f, 0xE6);
        HI351WriteCmosSensor(0x80, 0xEC);
        HI351WriteCmosSensor(0x81, 0xF1);
        HI351WriteCmosSensor(0x82, 0xF7);
        HI351WriteCmosSensor(0x83, 0xFB);
        HI351WriteCmosSensor(0x84, 0xFF);
                       
        HI351WriteCmosSensor(0x85, 0x00);//GMA08
        HI351WriteCmosSensor(0x86, 0x03);
        HI351WriteCmosSensor(0x87, 0x08);
        HI351WriteCmosSensor(0x88, 0x12);
        HI351WriteCmosSensor(0x89, 0x19);
        HI351WriteCmosSensor(0x8a, 0x25);
        HI351WriteCmosSensor(0x8b, 0x32);
        HI351WriteCmosSensor(0x8c, 0x3E);
        HI351WriteCmosSensor(0x8d, 0x4B);
        HI351WriteCmosSensor(0x8e, 0x56);
        HI351WriteCmosSensor(0x8f, 0x62);
        HI351WriteCmosSensor(0x90, 0x6A);
        HI351WriteCmosSensor(0x91, 0x71);
        HI351WriteCmosSensor(0x92, 0x78);
        HI351WriteCmosSensor(0x93, 0x7F);
        HI351WriteCmosSensor(0x94, 0x85);
        HI351WriteCmosSensor(0x95, 0x8A);
        HI351WriteCmosSensor(0x96, 0x90);
        HI351WriteCmosSensor(0x97, 0x95);
        HI351WriteCmosSensor(0x98, 0x9A);
        HI351WriteCmosSensor(0x99, 0x9F);
        HI351WriteCmosSensor(0x9a, 0xA9);
        HI351WriteCmosSensor(0x9b, 0xB1);
        HI351WriteCmosSensor(0x9c, 0xB9);
        HI351WriteCmosSensor(0x9d, 0xC6);
        HI351WriteCmosSensor(0x9e, 0xD0);
        HI351WriteCmosSensor(0x9f, 0xD8);
        HI351WriteCmosSensor(0xa0, 0xDF);
        HI351WriteCmosSensor(0xa1, 0xE6);
        HI351WriteCmosSensor(0xa2, 0xEC);
        HI351WriteCmosSensor(0xa3, 0xF1);
        HI351WriteCmosSensor(0xa4, 0xF7);
        HI351WriteCmosSensor(0xa5, 0xFB);
        HI351WriteCmosSensor(0xa6, 0xFF);
                       
        HI351WriteCmosSensor(0xa7, 0x00);//GMA09
        HI351WriteCmosSensor(0xa8, 0x03);
        HI351WriteCmosSensor(0xa9, 0x08);
        HI351WriteCmosSensor(0xaa, 0x12);
        HI351WriteCmosSensor(0xab, 0x19);
        HI351WriteCmosSensor(0xac, 0x25);
        HI351WriteCmosSensor(0xad, 0x32);
        HI351WriteCmosSensor(0xae, 0x3E);
        HI351WriteCmosSensor(0xaf, 0x4B);
        HI351WriteCmosSensor(0xb0, 0x56);
        HI351WriteCmosSensor(0xb1, 0x62);
        HI351WriteCmosSensor(0xb2, 0x6A);
        HI351WriteCmosSensor(0xb3, 0x71);
        HI351WriteCmosSensor(0xb4, 0x78);
        HI351WriteCmosSensor(0xb5, 0x7F);
        HI351WriteCmosSensor(0xb6, 0x85);
        HI351WriteCmosSensor(0xb7, 0x8A);
        HI351WriteCmosSensor(0xb8, 0x90);
        HI351WriteCmosSensor(0xb9, 0x95);
        HI351WriteCmosSensor(0xba, 0x9A);
        HI351WriteCmosSensor(0xbb, 0x9F);
        HI351WriteCmosSensor(0xbc, 0xA9);
        HI351WriteCmosSensor(0xbd, 0xB1);
        HI351WriteCmosSensor(0xbe, 0xB9);
        HI351WriteCmosSensor(0xbf, 0xC6);
        HI351WriteCmosSensor(0xc0, 0xD0);
        HI351WriteCmosSensor(0xc1, 0xD8);
        HI351WriteCmosSensor(0xc2, 0xDF);
        HI351WriteCmosSensor(0xc3, 0xE6);
        HI351WriteCmosSensor(0xc4, 0xEC);
        HI351WriteCmosSensor(0xc5, 0xF1);
        HI351WriteCmosSensor(0xc6, 0xF7);
        HI351WriteCmosSensor(0xc7, 0xFB);
        HI351WriteCmosSensor(0xc8, 0xFF);
                       
        HI351WriteCmosSensor(0xc9, 0x00);//GMA10
        HI351WriteCmosSensor(0xca, 0x03);
        HI351WriteCmosSensor(0xcb, 0x08);
        HI351WriteCmosSensor(0xcc, 0x12);
        HI351WriteCmosSensor(0xcd, 0x19);
        HI351WriteCmosSensor(0xce, 0x25);
        HI351WriteCmosSensor(0xcf, 0x32);
        HI351WriteCmosSensor(0xd0, 0x3E);
        HI351WriteCmosSensor(0xd1, 0x4B);
        HI351WriteCmosSensor(0xd2, 0x56);
        HI351WriteCmosSensor(0xd3, 0x62);
        HI351WriteCmosSensor(0xd4, 0x6A);
        HI351WriteCmosSensor(0xd5, 0x71);
        HI351WriteCmosSensor(0xd6, 0x78);
        HI351WriteCmosSensor(0xd7, 0x7F);
        HI351WriteCmosSensor(0xd8, 0x85);
        HI351WriteCmosSensor(0xd9, 0x8A);
        HI351WriteCmosSensor(0xda, 0x90);
        HI351WriteCmosSensor(0xdb, 0x95);
        HI351WriteCmosSensor(0xdc, 0x9A);
        HI351WriteCmosSensor(0xdd, 0x9F);
        HI351WriteCmosSensor(0xde, 0xA9);
        HI351WriteCmosSensor(0xdf, 0xB1);
        HI351WriteCmosSensor(0xe0, 0xB9);
        HI351WriteCmosSensor(0xe1, 0xC6);
        HI351WriteCmosSensor(0xe2, 0xD0);
        HI351WriteCmosSensor(0xe3, 0xD8);
        HI351WriteCmosSensor(0xe4, 0xDF);
        HI351WriteCmosSensor(0xe5, 0xE6);
        HI351WriteCmosSensor(0xe6, 0xEC);
        HI351WriteCmosSensor(0xe7, 0xF1);
        HI351WriteCmosSensor(0xe8, 0xF7);
        HI351WriteCmosSensor(0xe9, 0xFB);
        HI351WriteCmosSensor(0xea, 0xFF);
                       
        HI351WriteCmosSensor(0xeb, 0x00);//GMA11  
        HI351WriteCmosSensor(0xec, 0x03);         
        HI351WriteCmosSensor(0xed, 0x08);         
        HI351WriteCmosSensor(0xee, 0x12);         
        HI351WriteCmosSensor(0xef, 0x19);         
        HI351WriteCmosSensor(0xf0, 0x25);         
        HI351WriteCmosSensor(0xf1, 0x32);         
        HI351WriteCmosSensor(0xf2, 0x3E);         
        HI351WriteCmosSensor(0xf3, 0x4B);         
        HI351WriteCmosSensor(0xf4, 0x56);         
        HI351WriteCmosSensor(0xf5, 0x62);         
        HI351WriteCmosSensor(0xf6, 0x6A);         
        HI351WriteCmosSensor(0xf7, 0x71);         
        HI351WriteCmosSensor(0xf8, 0x78);         
        HI351WriteCmosSensor(0xf9, 0x7F);         
        HI351WriteCmosSensor(0xfa, 0x85);         
        HI351WriteCmosSensor(0xfb, 0x8A);         
        HI351WriteCmosSensor(0xfc, 0x90);         
        HI351WriteCmosSensor(0xfd, 0x95);         
        HI351WriteCmosSensor(0x03, 0xd1);//Page d1  
        HI351WriteCmosSensor(0x10, 0x9A);         
        HI351WriteCmosSensor(0x11, 0x9F);         
        HI351WriteCmosSensor(0x12, 0xA9);         
        HI351WriteCmosSensor(0x13, 0xB1);         
        HI351WriteCmosSensor(0x14, 0xB9);         
        HI351WriteCmosSensor(0x15, 0xC6);         
        HI351WriteCmosSensor(0x16, 0xD0);         
        HI351WriteCmosSensor(0x17, 0xD8);         
        HI351WriteCmosSensor(0x18, 0xDF);         
        HI351WriteCmosSensor(0x19, 0xE6);         
        HI351WriteCmosSensor(0x1a, 0xEC);         
        HI351WriteCmosSensor(0x1b, 0xF1);         
        HI351WriteCmosSensor(0x1c, 0xF7);         
        HI351WriteCmosSensor(0x1d, 0xFB);         
        HI351WriteCmosSensor(0x1e, 0xFF);
            
            ///////////////////////////////////////////
            // D1 Page Adaptive Y Target delta
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x1f, 0x80);//Y target delta 0 
        HI351WriteCmosSensor(0x20, 0x80);//Y target delta 1
        HI351WriteCmosSensor(0x21, 0x80);//Y target delta 2
        HI351WriteCmosSensor(0x22, 0x80);//Y target delta 3
        HI351WriteCmosSensor(0x23, 0x80);//Y target delta 4
        HI351WriteCmosSensor(0x24, 0x80);//Y target delta 5
        HI351WriteCmosSensor(0x25, 0x80);//Y target delta 6
        HI351WriteCmosSensor(0x26, 0x80);//Y target delta 7
        HI351WriteCmosSensor(0x27, 0x80);//Y target delta 8
        HI351WriteCmosSensor(0x28, 0x80);//Y target delta 9
        HI351WriteCmosSensor(0x29, 0x80);//Y target delta 10
        HI351WriteCmosSensor(0x2a, 0x80);//Y target delta 11
            ///////////////////////////////////////////
            // D1 Page Adaptive R/B saturation
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x2b, 0x70);//SATB_00
        HI351WriteCmosSensor(0x2c, 0x70);//SATB_01
        HI351WriteCmosSensor(0x2d, 0x70);//SATB_02
        HI351WriteCmosSensor(0x2e, 0x80);//SATB_03
        HI351WriteCmosSensor(0x2f, 0x80);//SATB_04
        HI351WriteCmosSensor(0x30, 0x80);//SATB_05
        HI351WriteCmosSensor(0x31, 0x88);//SATB_06
        HI351WriteCmosSensor(0x32, 0x88);//SATB_07
        HI351WriteCmosSensor(0x33, 0x88);//SATB_08
        HI351WriteCmosSensor(0x34, 0x88);//SATB_09
        HI351WriteCmosSensor(0x35, 0x88);//SATB_10
        HI351WriteCmosSensor(0x36, 0x88);//SATB_11 
            
            //Cr        
        
        HI351WriteCmosSensor(0x37, 0x70);//SATR_00
        HI351WriteCmosSensor(0x38, 0x70);//SATR_01
        HI351WriteCmosSensor(0x39, 0x70);//SATR_02
        HI351WriteCmosSensor(0x3a, 0x80);//SATR_03
        HI351WriteCmosSensor(0x3b, 0x80);//SATR_04
        HI351WriteCmosSensor(0x3c, 0x80);//SATR_05
        HI351WriteCmosSensor(0x3d, 0x88);//SATR_06
        HI351WriteCmosSensor(0x3e, 0x88);//SATR_07
        HI351WriteCmosSensor(0x3f, 0x88);//SATR_08
        HI351WriteCmosSensor(0x40, 0x88);//SATR_09
        HI351WriteCmosSensor(0x41, 0x88);//SATR_10
        HI351WriteCmosSensor(0x42, 0x88);//SATR_11
        
            
            ///////////////////////////////////////////
            // D1 Page Adaptive CMC
            ///////////////////////////////////////////
            
        HI351WriteCmosSensor(0x43, 0x2f);//CMC_00
        HI351WriteCmosSensor(0x44, 0x74);
        HI351WriteCmosSensor(0x45, 0x3f);
        HI351WriteCmosSensor(0x46, 0x0b);
        HI351WriteCmosSensor(0x47, 0x1c);
        HI351WriteCmosSensor(0x48, 0x76);
        HI351WriteCmosSensor(0x49, 0x1a);
        HI351WriteCmosSensor(0x4a, 0x04);
        HI351WriteCmosSensor(0x4b, 0x2f);
        HI351WriteCmosSensor(0x4c, 0x73);
                       
        HI351WriteCmosSensor(0x4d, 0x2f);//CMC_01
        HI351WriteCmosSensor(0x4e, 0x74);
        HI351WriteCmosSensor(0x4f, 0x3f);
        HI351WriteCmosSensor(0x50, 0x0b);
        HI351WriteCmosSensor(0x51, 0x1c);
        HI351WriteCmosSensor(0x52, 0x76);
        HI351WriteCmosSensor(0x53, 0x1a);
        HI351WriteCmosSensor(0x54, 0x04);
        HI351WriteCmosSensor(0x55, 0x2f);
        HI351WriteCmosSensor(0x56, 0x73);
                       
        HI351WriteCmosSensor(0x57, 0x2f);//CMC_02
        HI351WriteCmosSensor(0x58, 0x74);
        HI351WriteCmosSensor(0x59, 0x3f);
        HI351WriteCmosSensor(0x5a, 0x0b);
        HI351WriteCmosSensor(0x5b, 0x1c);
        HI351WriteCmosSensor(0x5c, 0x76);
        HI351WriteCmosSensor(0x5d, 0x1a);
        HI351WriteCmosSensor(0x5e, 0x04);
        HI351WriteCmosSensor(0x5f, 0x2f);
        HI351WriteCmosSensor(0x60, 0x73);
                       
        HI351WriteCmosSensor(0x61, 0x2f);//CMC_03
        HI351WriteCmosSensor(0x62, 0x6a);
        HI351WriteCmosSensor(0x63, 0x32);
        HI351WriteCmosSensor(0x64, 0x08);
        HI351WriteCmosSensor(0x65, 0x1a);
        HI351WriteCmosSensor(0x66, 0x6c);
        HI351WriteCmosSensor(0x67, 0x12);
        HI351WriteCmosSensor(0x68, 0x03);
        HI351WriteCmosSensor(0x69, 0x30);
        HI351WriteCmosSensor(0x6a, 0x73);
                       
        HI351WriteCmosSensor(0x6b, 0x2f);//CMC_04
        HI351WriteCmosSensor(0x6c, 0x65);
        HI351WriteCmosSensor(0x6d, 0x2b);
        HI351WriteCmosSensor(0x6e, 0x06);
        HI351WriteCmosSensor(0x6f, 0x19);
        HI351WriteCmosSensor(0x70, 0x6c);
        HI351WriteCmosSensor(0x71, 0x13);
        HI351WriteCmosSensor(0x72, 0x09);
        HI351WriteCmosSensor(0x73, 0x2a);
        HI351WriteCmosSensor(0x74, 0x73);
                       
        HI351WriteCmosSensor(0x75, 0x2f);//CMC_05
        HI351WriteCmosSensor(0x76, 0x65);
        HI351WriteCmosSensor(0x77, 0x2b);
        HI351WriteCmosSensor(0x78, 0x06);
        HI351WriteCmosSensor(0x79, 0x19);
        HI351WriteCmosSensor(0x7a, 0x6c);
        HI351WriteCmosSensor(0x7b, 0x13);
        HI351WriteCmosSensor(0x7c, 0x09);
        HI351WriteCmosSensor(0x7d, 0x2a);
        HI351WriteCmosSensor(0x7e, 0x73);
                       
        HI351WriteCmosSensor(0x7f, 0x2f);//CMC_06
        HI351WriteCmosSensor(0x80, 0x6a);
        HI351WriteCmosSensor(0x81, 0x32);
        HI351WriteCmosSensor(0x82, 0x08);
        HI351WriteCmosSensor(0x83, 0x1a);
        HI351WriteCmosSensor(0x84, 0x6c);
        HI351WriteCmosSensor(0x85, 0x12);
        HI351WriteCmosSensor(0x86, 0x03);
        HI351WriteCmosSensor(0x87, 0x30);
        HI351WriteCmosSensor(0x88, 0x73);
                       
        HI351WriteCmosSensor(0x89, 0x2f);//CMC_07
        HI351WriteCmosSensor(0x8a, 0x6a);
        HI351WriteCmosSensor(0x8b, 0x32);
        HI351WriteCmosSensor(0x8c, 0x08);
        HI351WriteCmosSensor(0x8d, 0x1a);
        HI351WriteCmosSensor(0x8e, 0x6c);
        HI351WriteCmosSensor(0x8f, 0x12);
        HI351WriteCmosSensor(0x90, 0x03);
        HI351WriteCmosSensor(0x91, 0x30);
        HI351WriteCmosSensor(0x92, 0x73);
                       
        HI351WriteCmosSensor(0x93, 0x2f);//CMC_08
        HI351WriteCmosSensor(0x94, 0x6a);
        HI351WriteCmosSensor(0x95, 0x32);
        HI351WriteCmosSensor(0x96, 0x08);
        HI351WriteCmosSensor(0x97, 0x1a);
        HI351WriteCmosSensor(0x98, 0x6c);
        HI351WriteCmosSensor(0x99, 0x12);
        HI351WriteCmosSensor(0x9a, 0x03);
        HI351WriteCmosSensor(0x9b, 0x30);
        HI351WriteCmosSensor(0x9c, 0x73);
                       
        HI351WriteCmosSensor(0x9d, 0x2f);//CMC_09
        HI351WriteCmosSensor(0x9e, 0x6a);
        HI351WriteCmosSensor(0x9f, 0x32);
        HI351WriteCmosSensor(0xa0, 0x08);
        HI351WriteCmosSensor(0xa1, 0x1a);
        HI351WriteCmosSensor(0xa2, 0x6c);
        HI351WriteCmosSensor(0xa3, 0x12);
        HI351WriteCmosSensor(0xa4, 0x03);
        HI351WriteCmosSensor(0xa5, 0x30);
        HI351WriteCmosSensor(0xa6, 0x73);
                       
        HI351WriteCmosSensor(0xa7, 0x2f);//CMC_10
        HI351WriteCmosSensor(0xa8, 0x6a);
        HI351WriteCmosSensor(0xa9, 0x32);
        HI351WriteCmosSensor(0xaa, 0x08);
        HI351WriteCmosSensor(0xab, 0x1a);
        HI351WriteCmosSensor(0xac, 0x6c);
        HI351WriteCmosSensor(0xad, 0x12);
        HI351WriteCmosSensor(0xae, 0x03);
        HI351WriteCmosSensor(0xaf, 0x30);
        HI351WriteCmosSensor(0xb0, 0x73);
                       
        HI351WriteCmosSensor(0xb1, 0x2f);//CMC_11
        HI351WriteCmosSensor(0xb2, 0x6a);
        HI351WriteCmosSensor(0xb3, 0x32);
        HI351WriteCmosSensor(0xb4, 0x08);
        HI351WriteCmosSensor(0xb5, 0x1a);
        HI351WriteCmosSensor(0xb6, 0x6c);
        HI351WriteCmosSensor(0xb7, 0x12);
        HI351WriteCmosSensor(0xb8, 0x03);
        HI351WriteCmosSensor(0xb9, 0x30);
        HI351WriteCmosSensor(0xba, 0x73);
            //<A - init>
            /////////////////////////////////////////////
            // D1~D2~D3 Page Adaptive Multi-CMC
            /////////////////////////////////////////////             
            //MCMC_00
        HI351WriteCmosSensor(0xbb, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0xbc, 0x01);//GLB_HUE  
        HI351WriteCmosSensor(0xbd, 0x73);//0_GAIN   
        HI351WriteCmosSensor(0xbe, 0x81);//0_HUE      
        HI351WriteCmosSensor(0xbf, 0x35);//0_CENTER
        HI351WriteCmosSensor(0xc0, 0x1a);//0_DELTA  
        HI351WriteCmosSensor(0xc1, 0x6c);//1_GAIN   
        HI351WriteCmosSensor(0xc2, 0x12);//1_HUE      
        HI351WriteCmosSensor(0xc3, 0x76);//1_CENTER 
        HI351WriteCmosSensor(0xc4, 0x34);//1_DELTA  
        HI351WriteCmosSensor(0xc5, 0x88);//2_GAIN   
        HI351WriteCmosSensor(0xc6, 0x8c);//2_HUE      
        HI351WriteCmosSensor(0xc7, 0xa9);//2_CENTER 
        HI351WriteCmosSensor(0xc8, 0x10);//2_DELTA  
        HI351WriteCmosSensor(0xc9, 0x57);//3_GAIN   
        HI351WriteCmosSensor(0xca, 0x88);//3_HUE      
        HI351WriteCmosSensor(0xcb, 0x52);//3_CENTER
        HI351WriteCmosSensor(0xcc, 0x16);//3_DELTA  
        HI351WriteCmosSensor(0xcd, 0x80);//4_GAIN
        HI351WriteCmosSensor(0xce, 0x0c);//4_HUE      
        HI351WriteCmosSensor(0xcf, 0x76);//4_CENTER
        HI351WriteCmosSensor(0xd0, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0xd1, 0xb2);//5_GAIN   
        HI351WriteCmosSensor(0xd2, 0x8a);//5_HUE      
        HI351WriteCmosSensor(0xd3, 0x52);//5_CENTER
        HI351WriteCmosSensor(0xd4, 0x1c);//5_DELTA  
            //MCMC_01  
        HI351WriteCmosSensor(0xd5, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0xd6, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0xd7, 0x66);//0_GAIN  
        HI351WriteCmosSensor(0xd8, 0x00);//0_HUE
        HI351WriteCmosSensor(0xd9, 0x35);//0_CENTER
        HI351WriteCmosSensor(0xda, 0x13);//0_DELTA
        HI351WriteCmosSensor(0xdb, 0x67);//1_GAIN  
        HI351WriteCmosSensor(0xdc, 0x04);//1_HUE     
        HI351WriteCmosSensor(0xdd, 0x6e);//1_CENTER
        HI351WriteCmosSensor(0xde, 0x1c);//1_DELTA
        HI351WriteCmosSensor(0xdf, 0x67);//2_GAIN  
        HI351WriteCmosSensor(0xe0, 0x8f);//2_HUE     
        HI351WriteCmosSensor(0xe1, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0xe2, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0xe3, 0x9a);//3_GAIN  
        HI351WriteCmosSensor(0xe4, 0x86);//3_HUE     
        HI351WriteCmosSensor(0xe5, 0x52);//3_CENTER
        HI351WriteCmosSensor(0xe6, 0x1c);//3_DELTA
        HI351WriteCmosSensor(0xe7, 0x90);//4_GAIN      
        HI351WriteCmosSensor(0xe8, 0x94);//4_HUE     
        HI351WriteCmosSensor(0xe9, 0x93);//4_CENTER
        HI351WriteCmosSensor(0xea, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0xeb, 0xb2);//5_GAIN  
        HI351WriteCmosSensor(0xec, 0x8a);//5_HUE     
        HI351WriteCmosSensor(0xed, 0x52);//5_CENTER
        HI351WriteCmosSensor(0xee, 0x1d);//5_DELTA
            //MCMC_02  
        HI351WriteCmosSensor(0xef, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0xf0, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0xf1, 0x66);//0_GAIN  
        HI351WriteCmosSensor(0xf2, 0x00);//0_HUE
        HI351WriteCmosSensor(0xf3, 0x35);//0_CENTER
        HI351WriteCmosSensor(0xf4, 0x13);//0_DELTA
        HI351WriteCmosSensor(0xf5, 0x67);//1_GAIN  
        HI351WriteCmosSensor(0xf6, 0x04);//1_HUE     
        HI351WriteCmosSensor(0xf7, 0x6e);//1_CENTER
        HI351WriteCmosSensor(0xf8, 0x1c);//1_DELTA
        HI351WriteCmosSensor(0xf9, 0x67);//2_GAIN  
        HI351WriteCmosSensor(0xfa, 0x92);//2_HUE     
        HI351WriteCmosSensor(0xfb, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0xfc, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0xfd, 0x9a);//3_GAIN  
        
        HI351WriteCmosSensor(0x03, 0xd2);//Page d2
        HI351WriteCmosSensor(0x10, 0x86);//3_HUE     
        HI351WriteCmosSensor(0x11, 0x52);//3_CENTER
        HI351WriteCmosSensor(0x12, 0x1c);//3_DELTA
        HI351WriteCmosSensor(0x13, 0x90);//4_GAIN  
        HI351WriteCmosSensor(0x14, 0x94);//4_HUE     
        HI351WriteCmosSensor(0x15, 0x93);//4_CENTER
        HI351WriteCmosSensor(0x16, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0x17, 0xb2);//5_GAIN  
        HI351WriteCmosSensor(0x18, 0x8a);//5_HUE     
        HI351WriteCmosSensor(0x19, 0x52);//5_CENTER
        HI351WriteCmosSensor(0x1a, 0x1d);//5_DELTA
            //MCMC_03  
        HI351WriteCmosSensor(0x1b, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0x1c, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0x1d, 0x70);//0_GAIN
        HI351WriteCmosSensor(0x1e, 0x04);//0_HUE
        HI351WriteCmosSensor(0x1f, 0x36);//0_CENTER
        HI351WriteCmosSensor(0x20, 0x0d);//0_DELTA
        HI351WriteCmosSensor(0x21, 0xa0);//1_GAIN
        HI351WriteCmosSensor(0x22, 0x08);//1_HUE
        HI351WriteCmosSensor(0x23, 0x76);//1_CENTER
        HI351WriteCmosSensor(0x24, 0x10);//1_DELTA
        HI351WriteCmosSensor(0x25, 0x80);//2_GAIN
        HI351WriteCmosSensor(0x26, 0x00);//2_HUE
        HI351WriteCmosSensor(0x27, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0x28, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0x29, 0x80);//3_GAIN
        HI351WriteCmosSensor(0x2a, 0x87);//3_HUE
        HI351WriteCmosSensor(0x2b, 0x51);//3_CENTER
        HI351WriteCmosSensor(0x2c, 0x1c);//3_DELTA
        HI351WriteCmosSensor(0x2d, 0x80);//4_GAIN
        HI351WriteCmosSensor(0x2e, 0x0c);//4_HUE
        HI351WriteCmosSensor(0x2f, 0x76);//4_CENTER
        HI351WriteCmosSensor(0x30, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0x31, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0x32, 0x8a);//5_HUE
        HI351WriteCmosSensor(0x33, 0x52);//5_CENTER
        HI351WriteCmosSensor(0x34, 0x1d);//5_DELTA
            //MCMC_04  
        HI351WriteCmosSensor(0x35, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0x36, 0x82);//GLB_HUE
        HI351WriteCmosSensor(0x37, 0x80);//0_GAIN
        HI351WriteCmosSensor(0x38, 0x80);//0_HUE
        HI351WriteCmosSensor(0x39, 0x36);//0_CENTER
        HI351WriteCmosSensor(0x3a, 0x0a);//0_DELTA
        HI351WriteCmosSensor(0x3b, 0x80);//1_GAIN
        HI351WriteCmosSensor(0x3c, 0x10);//1_HUE
        HI351WriteCmosSensor(0x3d, 0x80);//1_CENTER
        HI351WriteCmosSensor(0x3e, 0x0a);//1_DELTA
        HI351WriteCmosSensor(0x3f, 0x80);//2_GAIN
        HI351WriteCmosSensor(0x40, 0x00);//2_HUE
        HI351WriteCmosSensor(0x41, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0x42, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0x43, 0x80);//3_GAIN
        HI351WriteCmosSensor(0x44, 0x88);//3_HUE
        HI351WriteCmosSensor(0x45, 0x51);//3_CENTER
        HI351WriteCmosSensor(0x46, 0x1c);//3_DELTA
        HI351WriteCmosSensor(0x47, 0x80);//4_GAIN
        HI351WriteCmosSensor(0x48, 0x0c);//4_HUE
        HI351WriteCmosSensor(0x49, 0x76);//4_CENTER
        HI351WriteCmosSensor(0x4a, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0x4b, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0x4c, 0x8a);//5_HUE
        HI351WriteCmosSensor(0x4d, 0x52);//5_CENTER
        HI351WriteCmosSensor(0x4e, 0x1d);//5_DELTA
            //MCMC_05  
        HI351WriteCmosSensor(0x4f, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0x50, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0x51, 0x80);//0_GAIN
        HI351WriteCmosSensor(0x52, 0x80);//0_HUE
        HI351WriteCmosSensor(0x53, 0x36);//0_CENTER
        HI351WriteCmosSensor(0x54, 0x0a);//0_DELTA
        HI351WriteCmosSensor(0x55, 0x80);//1_GAIN
        HI351WriteCmosSensor(0x56, 0x10);//1_HUE
        HI351WriteCmosSensor(0x57, 0x80);//1_CENTER
        HI351WriteCmosSensor(0x58, 0x0a);//1_DELTA
        HI351WriteCmosSensor(0x59, 0x80);//2_GAIN
        HI351WriteCmosSensor(0x5a, 0x00);//2_HUE
        HI351WriteCmosSensor(0x5b, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0x5c, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0x5d, 0x80);//3_GAIN
        HI351WriteCmosSensor(0x5e, 0x88);//3_HUE
        HI351WriteCmosSensor(0x5f, 0x51);//3_CENTER
        HI351WriteCmosSensor(0x60, 0x1c);//3_DELTA
        HI351WriteCmosSensor(0x61, 0x80);//4_GAIN
        HI351WriteCmosSensor(0x62, 0x0c);//4_HUE
        HI351WriteCmosSensor(0x63, 0x76);//4_CENTER
        HI351WriteCmosSensor(0x64, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0x65, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0x66, 0x8a);//5_HUE
        HI351WriteCmosSensor(0x67, 0x52);//5_CENTER
        HI351WriteCmosSensor(0x68, 0x1d);//5_DELTA
            //MCMC_06  
        HI351WriteCmosSensor(0x69, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0x6a, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0x6b, 0x80);//0_GAIN
        HI351WriteCmosSensor(0x6c, 0x04);//0_HUE
        HI351WriteCmosSensor(0x6d, 0x36);//0_CENTER
        HI351WriteCmosSensor(0x6e, 0x13);//0_DELTA
        HI351WriteCmosSensor(0x6f, 0x80);//1_GAIN   
        HI351WriteCmosSensor(0x70, 0x0c);//1_HUE      
        HI351WriteCmosSensor(0x71, 0x62);//1_CENTER 
        HI351WriteCmosSensor(0x72, 0x10);//1_DELTA           
        HI351WriteCmosSensor(0x73, 0x73);//2_GAIN
        HI351WriteCmosSensor(0x74, 0x8a);//2_HUE
        HI351WriteCmosSensor(0x75, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0x76, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0x77, 0x80);//3_GAIN
        HI351WriteCmosSensor(0x78, 0x86);//3_HUE      
        HI351WriteCmosSensor(0x79, 0x51);//3_CENTER
        HI351WriteCmosSensor(0x7a, 0x14);//3_DELTA  
        HI351WriteCmosSensor(0x7b, 0x80);//4_GAIN
        HI351WriteCmosSensor(0x7c, 0x0c);//4_HUE
        HI351WriteCmosSensor(0x7d, 0x76);//4_CENTER
        HI351WriteCmosSensor(0x7e, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0x7f, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0x80, 0x8a);//5_HUE
        HI351WriteCmosSensor(0x81, 0x52);//5_CENTER
        HI351WriteCmosSensor(0x82, 0x1d);//5_DELTA
            //MCMC_07  
        HI351WriteCmosSensor(0x83, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0x84, 0x82);//GLB_HUE
        HI351WriteCmosSensor(0x85, 0x80);//0_GAIN
        HI351WriteCmosSensor(0x86, 0x04);//0_HUE
        HI351WriteCmosSensor(0x87, 0x36);//0_CENTER
        HI351WriteCmosSensor(0x88, 0x13);//0_DELTA
        HI351WriteCmosSensor(0x89, 0x80);//1_GAIN  
        HI351WriteCmosSensor(0x8a, 0x0c);//1_HUE     
        HI351WriteCmosSensor(0x8b, 0x62);//1_CENTER
        HI351WriteCmosSensor(0x8c, 0x10);//1_DELTA 
        HI351WriteCmosSensor(0x8d, 0x73);//2_GAIN
        HI351WriteCmosSensor(0x8e, 0x8a);//2_HUE
        HI351WriteCmosSensor(0x8f, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0x90, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0x91, 0x80);//3_GAIN
        HI351WriteCmosSensor(0x92, 0x86);//3_HUE     
        HI351WriteCmosSensor(0x93, 0x51);//3_CENTER
        HI351WriteCmosSensor(0x94, 0x14);//3_DELTA 
        HI351WriteCmosSensor(0x95, 0x80);//4_GAIN
        HI351WriteCmosSensor(0x96, 0x0c);//4_HUE
        HI351WriteCmosSensor(0x97, 0x76);//4_CENTER
        HI351WriteCmosSensor(0x98, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0x99, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0x9a, 0x8a);//5_HUE
        HI351WriteCmosSensor(0x9b, 0x52);//5_CENTER
        HI351WriteCmosSensor(0x9c, 0x1d);//5_DELTA
            //MCMC_08  
        HI351WriteCmosSensor(0x9d, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0x9e, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0x9f, 0x80);//0_GAIN
        HI351WriteCmosSensor(0xa0, 0x04);//0_HUE
        HI351WriteCmosSensor(0xa1, 0x36);//0_CENTER
        HI351WriteCmosSensor(0xa2, 0x13);//0_DELTA
        HI351WriteCmosSensor(0xa3, 0x80);//1_GAIN  
        HI351WriteCmosSensor(0xa4, 0x0c);//1_HUE     
        HI351WriteCmosSensor(0xa5, 0x62);//1_CENTER
        HI351WriteCmosSensor(0xa6, 0x10);//1_DELTA 
        HI351WriteCmosSensor(0xa7, 0x73);//2_GAIN
        HI351WriteCmosSensor(0xa8, 0x8a);//2_HUE
        HI351WriteCmosSensor(0xa9, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0xaa, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0xab, 0x80);//3_GAIN
        HI351WriteCmosSensor(0xac, 0x86);//3_HUE     
        HI351WriteCmosSensor(0xad, 0x51);//3_CENTER
        HI351WriteCmosSensor(0xae, 0x14);//3_DELTA 
        HI351WriteCmosSensor(0xaf, 0x80);//4_GAIN
        HI351WriteCmosSensor(0xb0, 0x0c);//4_HUE
        HI351WriteCmosSensor(0xb1, 0x76);//4_CENTER
        HI351WriteCmosSensor(0xb2, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0xb3, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0xb4, 0x8a);//5_HUE
        HI351WriteCmosSensor(0xb5, 0x52);//5_CENTER
        HI351WriteCmosSensor(0xb6, 0x1d);//5_DELTA
            //MCMC_09  
        HI351WriteCmosSensor(0xb7, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0xb8, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0xb9, 0x80);//0_GAIN
        HI351WriteCmosSensor(0xba, 0x04);//0_HUE
        HI351WriteCmosSensor(0xbb, 0x36);//0_CENTER
        HI351WriteCmosSensor(0xbc, 0x13);//0_DELTA
        HI351WriteCmosSensor(0xbd, 0x80);//1_GAIN   
        HI351WriteCmosSensor(0xbe, 0x0c);//1_HUE      
        HI351WriteCmosSensor(0xbf, 0x62);//1_CENTER 
        HI351WriteCmosSensor(0xc0, 0x10);//1_DELTA  
        HI351WriteCmosSensor(0xc1, 0x73);//2_GAIN
        HI351WriteCmosSensor(0xc2, 0x8a);//2_HUE
        HI351WriteCmosSensor(0xc3, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0xc4, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0xc5, 0x80);//3_GAIN
        HI351WriteCmosSensor(0xc6, 0x86);//3_HUE      
        HI351WriteCmosSensor(0xc7, 0x51);//3_CENTER
        HI351WriteCmosSensor(0xc8, 0x14);//3_DELTA  
        HI351WriteCmosSensor(0xc9, 0x80);//4_GAIN
        HI351WriteCmosSensor(0xca, 0x0c);//4_HUE
        HI351WriteCmosSensor(0xcb, 0x76);//4_CENTER
        HI351WriteCmosSensor(0xcc, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0xcd, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0xce, 0x8a);//5_HUE
        HI351WriteCmosSensor(0xcf, 0x52);//5_CENTER
        HI351WriteCmosSensor(0xd0, 0x1d);//5_DELTA
            //MCMC_10  
        HI351WriteCmosSensor(0xd1, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0xd2, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0xd3, 0x80);//0_GAIN
        HI351WriteCmosSensor(0xd4, 0x04);//0_HUE
        HI351WriteCmosSensor(0xd5, 0x36);//0_CENTER
        HI351WriteCmosSensor(0xd6, 0x13);//0_DELTA
        HI351WriteCmosSensor(0xd7, 0x80);//1_GAIN  
        HI351WriteCmosSensor(0xd8, 0x0c);//1_HUE     
        HI351WriteCmosSensor(0xd9, 0x62);//1_CENTER
        HI351WriteCmosSensor(0xda, 0x10);//1_DELTA 
        HI351WriteCmosSensor(0xdb, 0x73);//2_GAIN
        HI351WriteCmosSensor(0xdc, 0x8a);//2_HUE
        HI351WriteCmosSensor(0xdd, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0xde, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0xdf, 0x80);//3_GAIN
        HI351WriteCmosSensor(0xe0, 0x86);//3_HUE     
        HI351WriteCmosSensor(0xe1, 0x51);//3_CENTER
        HI351WriteCmosSensor(0xe2, 0x14);//3_DELTA 
        HI351WriteCmosSensor(0xe3, 0x80);//4_GAIN
        HI351WriteCmosSensor(0xe4, 0x0c);//4_HUE
        HI351WriteCmosSensor(0xe5, 0x76);//4_CENTER
        HI351WriteCmosSensor(0xe6, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0xe7, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0xe8, 0x8a);//5_HUE
        HI351WriteCmosSensor(0xe9, 0x52);//5_CENTER
        HI351WriteCmosSensor(0xea, 0x1d);//5_DELTA
            //MCMC_11  
        HI351WriteCmosSensor(0xeb, 0x80);//GLB_GAIN
        HI351WriteCmosSensor(0xec, 0x01);//GLB_HUE
        HI351WriteCmosSensor(0xed, 0x80);//0_GAIN
        HI351WriteCmosSensor(0xee, 0x04);//0_HUE
        HI351WriteCmosSensor(0xef, 0x36);//0_CENTER
        HI351WriteCmosSensor(0xf0, 0x13);//0_DELTA
        HI351WriteCmosSensor(0xf1, 0x80);//1_GAIN  
        HI351WriteCmosSensor(0xf2, 0x0c);//1_HUE     
        HI351WriteCmosSensor(0xf3, 0x62);//1_CENTER
        HI351WriteCmosSensor(0xf4, 0x10);//1_DELTA 
        HI351WriteCmosSensor(0xf5, 0x73);//2_GAIN
        HI351WriteCmosSensor(0xf6, 0x8a);//2_HUE
        HI351WriteCmosSensor(0xf7, 0xaf);//2_CENTER
        HI351WriteCmosSensor(0xf8, 0x1c);//2_DELTA
        HI351WriteCmosSensor(0xf9, 0x80);//3_GAIN
        HI351WriteCmosSensor(0xfa, 0x86);//3_HUE     
        HI351WriteCmosSensor(0xfb, 0x51);//3_CENTER
        HI351WriteCmosSensor(0xfc, 0x14);//3_DELTA 
        HI351WriteCmosSensor(0xfd, 0x80);//4_GAIN
                       
        
            
        HI351WriteCmosSensor(0x03, 0xd3);//Page d3
            
        HI351WriteCmosSensor(0x10, 0x0c);//4_HUE
        HI351WriteCmosSensor(0x11, 0x76);//4_CENTER
        HI351WriteCmosSensor(0x12, 0x1c);//4_DELTA
        HI351WriteCmosSensor(0x13, 0xb2);//5_GAIN
        HI351WriteCmosSensor(0x14, 0x8a);//5_HUE
        HI351WriteCmosSensor(0x15, 0x52);//5_CENTER
        HI351WriteCmosSensor(0x16, 0x1d);//5_DELTA
            
            ///////////////////////////////////////////
            // D3 Page Adaptive LSC
            ///////////////////////////////////////////
            
        HI351WriteCmosSensor(0x17, 0x00); //LSC 00 ofs GB
        HI351WriteCmosSensor(0x18, 0x00); //LSC 00 ofs B
        HI351WriteCmosSensor(0x19, 0x00); //LSC 00 ofs R
        HI351WriteCmosSensor(0x1a, 0x00); //LSC 00 ofs GR
                       
        HI351WriteCmosSensor(0x1b, 0x30);//LSC 00 Gain GB
        HI351WriteCmosSensor(0x1c, 0x30);//LSC 00 Gain B 
        HI351WriteCmosSensor(0x1d, 0x30); //LSC 00 Gain R
        HI351WriteCmosSensor(0x1e, 0x30);//LSC 00 Gain GR
                       
        HI351WriteCmosSensor(0x1f, 0x00); //LSC 01 ofs GB 
        HI351WriteCmosSensor(0x20, 0x00); //LSC 01 ofs B    
        HI351WriteCmosSensor(0x21, 0x00); //LSC 01 ofs R    
        HI351WriteCmosSensor(0x22, 0x00); //LSC 01 ofs GR 
        HI351WriteCmosSensor(0x23, 0x30);//LSC 01 Gain GB
        HI351WriteCmosSensor(0x24, 0x30);//LSC 01 Gain B 
        HI351WriteCmosSensor(0x25, 0x30); //LSC 01 Gain R 
        HI351WriteCmosSensor(0x26, 0x30);//LSC 01 Gain GR
                       
        HI351WriteCmosSensor(0x27, 0x00); //LSC 02 ofs GB 
        HI351WriteCmosSensor(0x28, 0x00); //LSC 02 ofs B    
        HI351WriteCmosSensor(0x29, 0x00); //LSC 02 ofs R    
        HI351WriteCmosSensor(0x2a, 0x00); //LSC 02 ofs GR 
        HI351WriteCmosSensor(0x2b, 0x30);//LSC 02 Gain GB
        HI351WriteCmosSensor(0x2c, 0x30);//LSC 02 Gain B 
        HI351WriteCmosSensor(0x2d, 0x30); //LSC 02 Gain R 
        HI351WriteCmosSensor(0x2e, 0x30);//LSC 02 Gain GR
                       
        HI351WriteCmosSensor(0x2f, 0x00); //LSC 03 ofs GB 
        HI351WriteCmosSensor(0x30, 0x00); //LSC 03 ofs B    
        HI351WriteCmosSensor(0x31, 0x00); //LSC 03 ofs R    
        HI351WriteCmosSensor(0x32, 0x00); //LSC 03 ofs GR 
        HI351WriteCmosSensor(0x33, 0x80); //LSC 03 Gain GB
        HI351WriteCmosSensor(0x34, 0x80); //LSC 03 Gain B 
        HI351WriteCmosSensor(0x35, 0x80); //LSC 03 Gain R 
        HI351WriteCmosSensor(0x36, 0x80); //LSC 03 Gain GR
                       
        HI351WriteCmosSensor(0x37, 0x00); //LSC 04 ofs GB 
        HI351WriteCmosSensor(0x38, 0x00); //LSC 04 ofs B    
        HI351WriteCmosSensor(0x39, 0x00); //LSC 04 ofs R    
        HI351WriteCmosSensor(0x3a, 0x00); //LSC 04 ofs GR 
        HI351WriteCmosSensor(0x3b, 0x80); //LSC 04 Gain GB
        HI351WriteCmosSensor(0x3c, 0x80); //LSC 04 Gain B 
        HI351WriteCmosSensor(0x3d, 0x80); //LSC 04 Gain R 
        HI351WriteCmosSensor(0x3e, 0x80); //LSC 04 Gain GR
                       
        HI351WriteCmosSensor(0x3f, 0x00); //LSC 05 ofs GB 
        HI351WriteCmosSensor(0x40, 0x00); //LSC 05 ofs B    
        HI351WriteCmosSensor(0x41, 0x00); //LSC 05 ofs R    
        HI351WriteCmosSensor(0x42, 0x00); //LSC 05 ofs GR 
        HI351WriteCmosSensor(0x43, 0x80);//LSC 05 Gain GB
        HI351WriteCmosSensor(0x44, 0x80);//LSC 05 Gain B 
        HI351WriteCmosSensor(0x45, 0x80); //LSC 05 Gain R 
        HI351WriteCmosSensor(0x46, 0x80);//LSC 05 Gain GR
                       
        HI351WriteCmosSensor(0x47, 0x00); //LSC 06 ofs GB 
        HI351WriteCmosSensor(0x48, 0x00); //LSC 06 ofs B    
        HI351WriteCmosSensor(0x49, 0x00); //LSC 06 ofs R    
        HI351WriteCmosSensor(0x4a, 0x00); //LSC 06 ofs GR 
        HI351WriteCmosSensor(0x4b, 0x80);//78 LSC 06 Gain GB
        HI351WriteCmosSensor(0x4c, 0x80);//7c LSC 06 Gain B 
        HI351WriteCmosSensor(0x4d, 0x80);//80 LSC 06 Gain R 
        HI351WriteCmosSensor(0x4e, 0x80);//78 LSC 06 Gain GR
                       
        HI351WriteCmosSensor(0x4f, 0x00); //LSC 07 ofs GB 
        HI351WriteCmosSensor(0x50, 0x00); //LSC 07 ofs B    
        HI351WriteCmosSensor(0x51, 0x00); //LSC 07 ofs R    
        HI351WriteCmosSensor(0x52, 0x00); //LSC 07 ofs GR 
        HI351WriteCmosSensor(0x53, 0x80);//78 LSC 07 Gain GB 
        HI351WriteCmosSensor(0x54, 0x80);//7c LSC 07 Gain B  
        HI351WriteCmosSensor(0x55, 0x80);//80 LSC 07 Gain R  
        HI351WriteCmosSensor(0x56, 0x80);//78 LSC 07 Gain GR 
                       
        HI351WriteCmosSensor(0x57, 0x00); //LSC 08 ofs GB 
        HI351WriteCmosSensor(0x58, 0x00); //LSC 08 ofs B    
        HI351WriteCmosSensor(0x59, 0x00); //LSC 08 ofs R    
        HI351WriteCmosSensor(0x5a, 0x00); //LSC 08 ofs GR 
        HI351WriteCmosSensor(0x5b, 0x80); //78 LSC 08 Gain GB
        HI351WriteCmosSensor(0x5c, 0x80); //7c LSC 08 Gain B 
        HI351WriteCmosSensor(0x5d, 0x80); //80 LSC 08 Gain R 
        HI351WriteCmosSensor(0x5e, 0x80); //78 LSC 08 Gain GR
                       
        HI351WriteCmosSensor(0x5f, 0x00); //LSC 09 ofs GB 
        HI351WriteCmosSensor(0x60, 0x00); //LSC 09 ofs B    
        HI351WriteCmosSensor(0x61, 0x00); //LSC 09 ofs R    
        HI351WriteCmosSensor(0x62, 0x00); //LSC 09 ofs GR 
        HI351WriteCmosSensor(0x63, 0x80); //78 LSC 09 Gain GB
        HI351WriteCmosSensor(0x64, 0x80); //7c LSC 09 Gain B 
        HI351WriteCmosSensor(0x65, 0x80); //80 LSC 09 Gain R 
        HI351WriteCmosSensor(0x66, 0x80); //78 LSC 09 Gain GR
                       
        HI351WriteCmosSensor(0x67, 0x00); //LSC 10 ofs GB 
        HI351WriteCmosSensor(0x68, 0x00); //LSC 10 ofs B    
        HI351WriteCmosSensor(0x69, 0x00); //LSC 10 ofs R    
        HI351WriteCmosSensor(0x6a, 0x00); //LSC 10 ofs GR 
        HI351WriteCmosSensor(0x6b, 0x80); //78 LSC 10 Gain GB
        HI351WriteCmosSensor(0x6c, 0x80); //7c LSC 10 Gain B 
        HI351WriteCmosSensor(0x6d, 0x80); //80 LSC 10 Gain R 
        HI351WriteCmosSensor(0x6e, 0x80); //78 LSC 10 Gain GR
                       
        HI351WriteCmosSensor(0x6f, 0x00); //LSC 11 ofs GB 
        HI351WriteCmosSensor(0x70, 0x00); //LSC 11 ofs B    
        HI351WriteCmosSensor(0x71, 0x00); //LSC 11 ofs R    
        HI351WriteCmosSensor(0x72, 0x00); //LSC 11 ofs GR 
        HI351WriteCmosSensor(0x73, 0x80); //78 LSC 11 Gain GB
        HI351WriteCmosSensor(0x74, 0x80); //7c LSC 11 Gain B 
        HI351WriteCmosSensor(0x75, 0x80); //80 LSC 11 Gain R 
        HI351WriteCmosSensor(0x76, 0x80); //78 LSC 11 Gain GR
            ///////////////////////////////////////////
            // D3 Page OTP, ROM Select TH
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x77, 0x60); //2 ROM High
        HI351WriteCmosSensor(0x78, 0x20); //2 ROM Low
        HI351WriteCmosSensor(0x79, 0x60); //3 OTP High
        HI351WriteCmosSensor(0x7a, 0x40); //3 OTP Mid
        HI351WriteCmosSensor(0x7b, 0x20); //3 OTP Low
            ///////////////////////////////////////////
            // D3 Page Adaptive DNP
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x7c, 0x00); //LSC EV max
        HI351WriteCmosSensor(0x7d, 0x00);
        HI351WriteCmosSensor(0x7e, 0x07);
        HI351WriteCmosSensor(0x7f, 0xf1);
                       
        HI351WriteCmosSensor(0x80, 0x00); //LSC EV min
        HI351WriteCmosSensor(0x81, 0x00);
        HI351WriteCmosSensor(0x82, 0x07);
        HI351WriteCmosSensor(0x83, 0xf1);
        HI351WriteCmosSensor(0x84, 0x20); //CTEM max
        HI351WriteCmosSensor(0x85, 0x20); //CTEM min
        HI351WriteCmosSensor(0x86, 0x20); //Y STD max
        HI351WriteCmosSensor(0x87, 0x20); //Y STD min
                       
        HI351WriteCmosSensor(0x88, 0x00); //LSC offset
        HI351WriteCmosSensor(0x89, 0x00);
        HI351WriteCmosSensor(0x8a, 0x00);
        HI351WriteCmosSensor(0x8b, 0x00);
        HI351WriteCmosSensor(0x8c, 0x80); //LSC gain
        HI351WriteCmosSensor(0x8d, 0x80);
        HI351WriteCmosSensor(0x8e, 0x80);
        HI351WriteCmosSensor(0x8f, 0x80);
                       
        HI351WriteCmosSensor(0x90, 0x80); //DNP CB
        HI351WriteCmosSensor(0x91, 0x80); //DNP CR
            
            ///////////////////////////////////
            //Page 0xD9 DMA EXTRA
            ///////////////////////////////////
            
        HI351WriteCmosSensor(0x03, 0xd9);
            
        HI351WriteCmosSensor(0x10, 0x03);
        HI351WriteCmosSensor(0x11, 0x10);//Page 10
        HI351WriteCmosSensor(0x12, 0x61);
        HI351WriteCmosSensor(0x13, 0x80);
        HI351WriteCmosSensor(0x14, 0x62);
        HI351WriteCmosSensor(0x15, 0x80);
        HI351WriteCmosSensor(0x16, 0x40);
        HI351WriteCmosSensor(0x17, 0x00);
                       
        HI351WriteCmosSensor(0x18, 0x48);
        HI351WriteCmosSensor(0x19, 0x80);
        HI351WriteCmosSensor(0x1a, 0x03);//Page 16
        HI351WriteCmosSensor(0x1b, 0x16);
        HI351WriteCmosSensor(0x1c, 0x30);
        HI351WriteCmosSensor(0x1d, 0x7f);
        HI351WriteCmosSensor(0x1e, 0x31);
        HI351WriteCmosSensor(0x1f, 0x42);
                       
        HI351WriteCmosSensor(0x20, 0x32);
        HI351WriteCmosSensor(0x21, 0x03);
        HI351WriteCmosSensor(0x22, 0x33);
        HI351WriteCmosSensor(0x23, 0x22);
        HI351WriteCmosSensor(0x24, 0x34);
        HI351WriteCmosSensor(0x25, 0x7b);
        HI351WriteCmosSensor(0x26, 0x35);
        HI351WriteCmosSensor(0x27, 0x19);           
        HI351WriteCmosSensor(0x28, 0x36);
        HI351WriteCmosSensor(0x29, 0x01);
        HI351WriteCmosSensor(0x2a, 0x37);
        HI351WriteCmosSensor(0x2b, 0x43);
        HI351WriteCmosSensor(0x2c, 0x38);
        HI351WriteCmosSensor(0x2d, 0x84);
        HI351WriteCmosSensor(0x2e, 0x70);
        HI351WriteCmosSensor(0x2f, 0x80);
                       
        HI351WriteCmosSensor(0x30, 0x71);
        HI351WriteCmosSensor(0x31, 0x00);
        HI351WriteCmosSensor(0x32, 0x72);
        HI351WriteCmosSensor(0x33, 0x9b);
        HI351WriteCmosSensor(0x34, 0x73);
        HI351WriteCmosSensor(0x35, 0x05);
        HI351WriteCmosSensor(0x36, 0x74);
        HI351WriteCmosSensor(0x37, 0x34);
                       
        HI351WriteCmosSensor(0x38, 0x75);
        HI351WriteCmosSensor(0x39, 0x1e);
        HI351WriteCmosSensor(0x3a, 0x76);
        HI351WriteCmosSensor(0x3b, 0xa6);
        HI351WriteCmosSensor(0x3c, 0x77);
        HI351WriteCmosSensor(0x3d, 0x10);
        HI351WriteCmosSensor(0x3e, 0x78);
        HI351WriteCmosSensor(0x3f, 0x69);
                       
        HI351WriteCmosSensor(0x40, 0x79);
        HI351WriteCmosSensor(0x41, 0x1e);
        HI351WriteCmosSensor(0x42, 0x7a);
        HI351WriteCmosSensor(0x43, 0x80);
        HI351WriteCmosSensor(0x44, 0x7b);
        HI351WriteCmosSensor(0x45, 0x80);
        HI351WriteCmosSensor(0x46, 0x7c);
        HI351WriteCmosSensor(0x47, 0xad);
                       
        HI351WriteCmosSensor(0x48, 0x7d);
        HI351WriteCmosSensor(0x49, 0x1e);
        HI351WriteCmosSensor(0x4a, 0x7e);
        HI351WriteCmosSensor(0x4b, 0x98);
        HI351WriteCmosSensor(0x4c, 0x7f);
        HI351WriteCmosSensor(0x4d, 0x80);
        HI351WriteCmosSensor(0x4e, 0x80);
        HI351WriteCmosSensor(0x4f, 0x51);
                       
        HI351WriteCmosSensor(0x50, 0x81);
        HI351WriteCmosSensor(0x51, 0x1e);
        HI351WriteCmosSensor(0x52, 0x82);
        HI351WriteCmosSensor(0x53, 0x80);
        HI351WriteCmosSensor(0x54, 0x83);
        HI351WriteCmosSensor(0x55, 0x0c);
        HI351WriteCmosSensor(0x56, 0x84);
        HI351WriteCmosSensor(0x57, 0x23);
                       
        HI351WriteCmosSensor(0x58, 0x85);
        HI351WriteCmosSensor(0x59, 0x1e);
        HI351WriteCmosSensor(0x5a, 0x86);
        HI351WriteCmosSensor(0x5b, 0xb3);
        HI351WriteCmosSensor(0x5c, 0x87);
        HI351WriteCmosSensor(0x5d, 0x8a);
        HI351WriteCmosSensor(0x5e, 0x88);
        HI351WriteCmosSensor(0x5f, 0x52);
                       
        HI351WriteCmosSensor(0x60, 0x89);
        HI351WriteCmosSensor(0x61, 0x1e);
        HI351WriteCmosSensor(0x62, 0x03);//Page 17 Gamma
        HI351WriteCmosSensor(0x63, 0x17);
        HI351WriteCmosSensor(0x64, 0x20);
        HI351WriteCmosSensor(0x65, 0x00);
        HI351WriteCmosSensor(0x66, 0x21);
        HI351WriteCmosSensor(0x67, 0x02);
                       
        HI351WriteCmosSensor(0x68, 0x22);
        HI351WriteCmosSensor(0x69, 0x04);
        HI351WriteCmosSensor(0x6a, 0x23);
        HI351WriteCmosSensor(0x6b, 0x09);
        HI351WriteCmosSensor(0x6c, 0x24);
        HI351WriteCmosSensor(0x6d, 0x12);
        HI351WriteCmosSensor(0x6e, 0x25);
        HI351WriteCmosSensor(0x6f, 0x23);
                       
        HI351WriteCmosSensor(0x70, 0x26);
        HI351WriteCmosSensor(0x71, 0x37);
        HI351WriteCmosSensor(0x72, 0x27);
        HI351WriteCmosSensor(0x73, 0x47);
        HI351WriteCmosSensor(0x74, 0x28);
        HI351WriteCmosSensor(0x75, 0x57);
        HI351WriteCmosSensor(0x76, 0x29);
        HI351WriteCmosSensor(0x77, 0x61);
                       
        HI351WriteCmosSensor(0x78, 0x2a);
        HI351WriteCmosSensor(0x79, 0x6b);
        HI351WriteCmosSensor(0x7a, 0x2b);
        HI351WriteCmosSensor(0x7b, 0x71);
        HI351WriteCmosSensor(0x7c, 0x2c);
        HI351WriteCmosSensor(0x7d, 0x76);
        HI351WriteCmosSensor(0x7e, 0x2d);
        HI351WriteCmosSensor(0x7f, 0x7a);
                       
        HI351WriteCmosSensor(0x80, 0x2e);
        HI351WriteCmosSensor(0x81, 0x7f);
        HI351WriteCmosSensor(0x82, 0x2f);
        HI351WriteCmosSensor(0x83, 0x84);
        HI351WriteCmosSensor(0x84, 0x30);
        HI351WriteCmosSensor(0x85, 0x88);
        HI351WriteCmosSensor(0x86, 0x31);
        HI351WriteCmosSensor(0x87, 0x8c);
                       
        HI351WriteCmosSensor(0x88, 0x32);
        HI351WriteCmosSensor(0x89, 0x91);
        HI351WriteCmosSensor(0x8a, 0x33);
        HI351WriteCmosSensor(0x8b, 0x94);
        HI351WriteCmosSensor(0x8c, 0x34);
        HI351WriteCmosSensor(0x8d, 0x98);
        HI351WriteCmosSensor(0x8e, 0x35);
        HI351WriteCmosSensor(0x8f, 0x9f);
                       
        HI351WriteCmosSensor(0x90, 0x36);
        HI351WriteCmosSensor(0x91, 0xa6);
        HI351WriteCmosSensor(0x92, 0x37);
        HI351WriteCmosSensor(0x93, 0xae);
        HI351WriteCmosSensor(0x94, 0x38);
        HI351WriteCmosSensor(0x95, 0xbb);
        HI351WriteCmosSensor(0x96, 0x39);
        HI351WriteCmosSensor(0x97, 0xc9);
                       
        HI351WriteCmosSensor(0x98, 0x3a);
        HI351WriteCmosSensor(0x99, 0xd3);
        HI351WriteCmosSensor(0x9a, 0x3b);
        HI351WriteCmosSensor(0x9b, 0xdc);
        HI351WriteCmosSensor(0x9c, 0x3c);
        HI351WriteCmosSensor(0x9d, 0xe2);
        HI351WriteCmosSensor(0x9e, 0x3d);
        HI351WriteCmosSensor(0x9f, 0xe8);
                       
        HI351WriteCmosSensor(0xa0, 0x3e);
        HI351WriteCmosSensor(0xa1, 0xed);
        HI351WriteCmosSensor(0xa2, 0x3f);
        HI351WriteCmosSensor(0xa3, 0xf4);
        HI351WriteCmosSensor(0xa4, 0x40);
        HI351WriteCmosSensor(0xa5, 0xfa);
        HI351WriteCmosSensor(0xa6, 0x41);
        HI351WriteCmosSensor(0xa7, 0xff);
                       
        HI351WriteCmosSensor(0xa8, 0x03);//page 20 AE
        HI351WriteCmosSensor(0xa9, 0x20);
        HI351WriteCmosSensor(0xaa, 0x39);
        HI351WriteCmosSensor(0xab, 0x40);
        HI351WriteCmosSensor(0xac, 0x03);//Page 15 SHD
        HI351WriteCmosSensor(0xad, 0x15);
        HI351WriteCmosSensor(0xae, 0x24);
        HI351WriteCmosSensor(0xaf, 0x00);
                       
        HI351WriteCmosSensor(0xb0, 0x25);
        HI351WriteCmosSensor(0xb1, 0x00);
        HI351WriteCmosSensor(0xb2, 0x26);
        HI351WriteCmosSensor(0xb3, 0x00);
        HI351WriteCmosSensor(0xb4, 0x27);
        HI351WriteCmosSensor(0xb5, 0x00);
        HI351WriteCmosSensor(0xb6, 0x28);
        HI351WriteCmosSensor(0xb7, 0x80);
                       
        HI351WriteCmosSensor(0xb8, 0x29);
        HI351WriteCmosSensor(0xb9, 0x80);
        HI351WriteCmosSensor(0xba, 0x2a);
        HI351WriteCmosSensor(0xbb, 0x7a);
        HI351WriteCmosSensor(0xbc, 0x2b);
        HI351WriteCmosSensor(0xbd, 0x80);
        HI351WriteCmosSensor(0xbe, 0x11);
        HI351WriteCmosSensor(0xbf, 0x40);
            ///////////////////////////////////
            // Page 0xDA(DMA Outdoor)
            ///////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xda);
            
        HI351WriteCmosSensor(0x10, 0x03);
        HI351WriteCmosSensor(0x11, 0x11);//11 page
        HI351WriteCmosSensor(0x12, 0x10);
        HI351WriteCmosSensor(0x13, 0x1f);
        HI351WriteCmosSensor(0x14, 0x11);
        HI351WriteCmosSensor(0x15, 0x25);
        HI351WriteCmosSensor(0x16, 0x12);
        HI351WriteCmosSensor(0x17, 0x22);
                            
        HI351WriteCmosSensor(0x18, 0x13);
        HI351WriteCmosSensor(0x19, 0x11);
        HI351WriteCmosSensor(0x1a, 0x14);
        HI351WriteCmosSensor(0x1b, 0x3a);
        HI351WriteCmosSensor(0x1c, 0x30);
        HI351WriteCmosSensor(0x1d, 0x20);
        HI351WriteCmosSensor(0x1e, 0x31);
        HI351WriteCmosSensor(0x1f, 0x20);
                            
        HI351WriteCmosSensor(0x20, 0x32); //outdoor 0x1132 //STEVE Lum. Level. in DLPF 
        HI351WriteCmosSensor(0x21, 0x8b); //52                                         
        HI351WriteCmosSensor(0x22, 0x33); //outdoor 0x1133                             
        HI351WriteCmosSensor(0x23, 0x54); //3b                                         
        HI351WriteCmosSensor(0x24, 0x34); //outdoor 0x1134                             
        HI351WriteCmosSensor(0x25, 0x2c); //1d                                         
        HI351WriteCmosSensor(0x26, 0x35); //outdoor 0x1135                             
        HI351WriteCmosSensor(0x27, 0x29);   //21                                                        
        HI351WriteCmosSensor(0x28, 0x36); //outdoor 0x1136                             
        HI351WriteCmosSensor(0x29, 0x18); //1b                                         
        HI351WriteCmosSensor(0x2a, 0x37); //outdoor 0x1137                             
        HI351WriteCmosSensor(0x2b, 0x1e); //21                                         
        HI351WriteCmosSensor(0x2c, 0x38); //outdoor 0x1138                             
        HI351WriteCmosSensor(0x2d, 0x17); //18                                         
            
        HI351WriteCmosSensor(0x2e, 0x39);
        HI351WriteCmosSensor(0x2f, 0x28);               
        HI351WriteCmosSensor(0x30, 0x3a);
        HI351WriteCmosSensor(0x31, 0x28);
        HI351WriteCmosSensor(0x32, 0x3b);
        HI351WriteCmosSensor(0x33, 0x28);
        HI351WriteCmosSensor(0x34, 0x3c);
        HI351WriteCmosSensor(0x35, 0x28);
        HI351WriteCmosSensor(0x36, 0x3d);
        HI351WriteCmosSensor(0x37, 0x28);
                            
        HI351WriteCmosSensor(0x38, 0x3e);
        HI351WriteCmosSensor(0x39, 0x34);
        HI351WriteCmosSensor(0x3a, 0x3f);
        HI351WriteCmosSensor(0x3b, 0x38);
        HI351WriteCmosSensor(0x3c, 0x40);
        HI351WriteCmosSensor(0x3d, 0x3c);
        HI351WriteCmosSensor(0x3e, 0x41);
        HI351WriteCmosSensor(0x3f, 0x28);
                            
        HI351WriteCmosSensor(0x40, 0x42);
        HI351WriteCmosSensor(0x41, 0x28);
        HI351WriteCmosSensor(0x42, 0x43);
        HI351WriteCmosSensor(0x43, 0x28);
        HI351WriteCmosSensor(0x44, 0x44);
        HI351WriteCmosSensor(0x45, 0x28);
        HI351WriteCmosSensor(0x46, 0x45);
        HI351WriteCmosSensor(0x47, 0x28);
                            
        HI351WriteCmosSensor(0x48, 0x46);
        HI351WriteCmosSensor(0x49, 0x28);
        HI351WriteCmosSensor(0x4a, 0x47);
        HI351WriteCmosSensor(0x4b, 0x28);
        HI351WriteCmosSensor(0x4c, 0x48);
        HI351WriteCmosSensor(0x4d, 0x28);
        HI351WriteCmosSensor(0x4e, 0x49);
        HI351WriteCmosSensor(0x4f, 0xf0);
                            
        HI351WriteCmosSensor(0x50, 0x4a);
        HI351WriteCmosSensor(0x51, 0xf0);
        HI351WriteCmosSensor(0x52, 0x4b);
        HI351WriteCmosSensor(0x53, 0xf0);
        HI351WriteCmosSensor(0x54, 0x4c);
        HI351WriteCmosSensor(0x55, 0xf0);
        HI351WriteCmosSensor(0x56, 0x4d);
        HI351WriteCmosSensor(0x57, 0xf0);
                            
        HI351WriteCmosSensor(0x58, 0x4e);
        HI351WriteCmosSensor(0x59, 0xf0);
        HI351WriteCmosSensor(0x5a, 0x4f);
        HI351WriteCmosSensor(0x5b, 0xf0);
        HI351WriteCmosSensor(0x5c, 0x50);
        HI351WriteCmosSensor(0x5d, 0xf0);
        HI351WriteCmosSensor(0x5e, 0x51);
        HI351WriteCmosSensor(0x5f, 0xf0);
                            
        HI351WriteCmosSensor(0x60, 0x52);
        HI351WriteCmosSensor(0x61, 0xf0);
        HI351WriteCmosSensor(0x62, 0x53);
        HI351WriteCmosSensor(0x63, 0xf0);
        HI351WriteCmosSensor(0x64, 0x54);
        HI351WriteCmosSensor(0x65, 0xf0);
        HI351WriteCmosSensor(0x66, 0x55);
        HI351WriteCmosSensor(0x67, 0xf0);
                            
        HI351WriteCmosSensor(0x68, 0x56);
        HI351WriteCmosSensor(0x69, 0xf0);
        HI351WriteCmosSensor(0x6a, 0x57);
        HI351WriteCmosSensor(0x6b, 0xe8);
        HI351WriteCmosSensor(0x6c, 0x58);
        HI351WriteCmosSensor(0x6d, 0xe0);
        HI351WriteCmosSensor(0x6e, 0x59);
        HI351WriteCmosSensor(0x6f, 0xfc);
                            
        HI351WriteCmosSensor(0x70, 0x5a);
        HI351WriteCmosSensor(0x71, 0xf8);
        HI351WriteCmosSensor(0x72, 0x5b);
        HI351WriteCmosSensor(0x73, 0xf2);
        HI351WriteCmosSensor(0x74, 0x5c);
        HI351WriteCmosSensor(0x75, 0xf0);
        HI351WriteCmosSensor(0x76, 0x5d);
        HI351WriteCmosSensor(0x77, 0xf0);
                            
        HI351WriteCmosSensor(0x78, 0x5e);
        HI351WriteCmosSensor(0x79, 0xec);
        HI351WriteCmosSensor(0x7a, 0x5f);
        HI351WriteCmosSensor(0x7b, 0xe8);
        HI351WriteCmosSensor(0x7c, 0x60);
        HI351WriteCmosSensor(0x7d, 0xe4);
        HI351WriteCmosSensor(0x7e, 0x61);
        HI351WriteCmosSensor(0x7f, 0xf0);
                            
        HI351WriteCmosSensor(0x80, 0x62);
        HI351WriteCmosSensor(0x81, 0xfc);
        HI351WriteCmosSensor(0x82, 0x63);
        HI351WriteCmosSensor(0x83, 0x60);
        HI351WriteCmosSensor(0x84, 0x64);
        HI351WriteCmosSensor(0x85, 0x20);
        HI351WriteCmosSensor(0x86, 0x65);
        HI351WriteCmosSensor(0x87, 0x30);
                            
        HI351WriteCmosSensor(0x88, 0x66);
        HI351WriteCmosSensor(0x89, 0x24);
        HI351WriteCmosSensor(0x8a, 0x67);
        HI351WriteCmosSensor(0x8b, 0x1a);
        HI351WriteCmosSensor(0x8c, 0x68);
        HI351WriteCmosSensor(0x8d, 0x5a);
        HI351WriteCmosSensor(0x8e, 0x69);
        HI351WriteCmosSensor(0x8f, 0x24);
                            
        HI351WriteCmosSensor(0x90, 0x6a);
        HI351WriteCmosSensor(0x91, 0x30);
        HI351WriteCmosSensor(0x92, 0x6b);
        HI351WriteCmosSensor(0x93, 0x24);
        HI351WriteCmosSensor(0x94, 0x6c);
        HI351WriteCmosSensor(0x95, 0x1a);
        HI351WriteCmosSensor(0x96, 0x6d);
        HI351WriteCmosSensor(0x97, 0x5c);
                            
        HI351WriteCmosSensor(0x98, 0x6e);
        HI351WriteCmosSensor(0x99, 0x20);
        HI351WriteCmosSensor(0x9a, 0x6f);
        HI351WriteCmosSensor(0x9b, 0x34);
        HI351WriteCmosSensor(0x9c, 0x70);
        HI351WriteCmosSensor(0x9d, 0x28);
        HI351WriteCmosSensor(0x9e, 0x71);
        HI351WriteCmosSensor(0x9f, 0x20);
                            
        HI351WriteCmosSensor(0xa0, 0x72);
        HI351WriteCmosSensor(0xa1, 0x5c);
        HI351WriteCmosSensor(0xa2, 0x73);
        HI351WriteCmosSensor(0xa3, 0x20);
        HI351WriteCmosSensor(0xa4, 0x74);
        HI351WriteCmosSensor(0xa5, 0x64);
        HI351WriteCmosSensor(0xa6, 0x75);
        HI351WriteCmosSensor(0xa7, 0x60);
                            
        HI351WriteCmosSensor(0xa8, 0x76);
        HI351WriteCmosSensor(0xa9, 0x42);
        HI351WriteCmosSensor(0xaa, 0x77);
        HI351WriteCmosSensor(0xab, 0x40);
        HI351WriteCmosSensor(0xac, 0x78);
        HI351WriteCmosSensor(0xad, 0x26);
        HI351WriteCmosSensor(0xae, 0x79);
        HI351WriteCmosSensor(0xaf, 0x88);
                            
        HI351WriteCmosSensor(0xb0, 0x7a);
        HI351WriteCmosSensor(0xb1, 0x80);
        HI351WriteCmosSensor(0xb2, 0x7b);
        HI351WriteCmosSensor(0xb3, 0x30);
        HI351WriteCmosSensor(0xb4, 0x7c);
        HI351WriteCmosSensor(0xb5, 0x38);
        HI351WriteCmosSensor(0xb6, 0x7d);
        HI351WriteCmosSensor(0xb7, 0x1c);
                            
        HI351WriteCmosSensor(0xb8, 0x7e);
        HI351WriteCmosSensor(0xb9, 0x38);
        HI351WriteCmosSensor(0xba, 0x7f);
        HI351WriteCmosSensor(0xbb, 0x34);
        HI351WriteCmosSensor(0xbc, 0x80);
        HI351WriteCmosSensor(0xbd, 0x30);
        HI351WriteCmosSensor(0xbe, 0x81);
        HI351WriteCmosSensor(0xbf, 0x32);
                            
        HI351WriteCmosSensor(0xc0, 0x82);
        HI351WriteCmosSensor(0xc1, 0x10);
        HI351WriteCmosSensor(0xc2, 0x83);
        HI351WriteCmosSensor(0xc3, 0x18);
        HI351WriteCmosSensor(0xc4, 0x84);
        HI351WriteCmosSensor(0xc5, 0x14);
        HI351WriteCmosSensor(0xc6, 0x85);
        HI351WriteCmosSensor(0xc7, 0x10);
                            
        HI351WriteCmosSensor(0xc8, 0x86);
        HI351WriteCmosSensor(0xc9, 0x1c);
        HI351WriteCmosSensor(0xca, 0x87);
        HI351WriteCmosSensor(0xcb, 0x08);
        HI351WriteCmosSensor(0xcc, 0x88);
        HI351WriteCmosSensor(0xcd, 0x38);
        HI351WriteCmosSensor(0xce, 0x89);
        HI351WriteCmosSensor(0xcf, 0x34);
                            
        HI351WriteCmosSensor(0xd0, 0x8a);
        HI351WriteCmosSensor(0xd1, 0x30);
        HI351WriteCmosSensor(0xd2, 0x90);
        HI351WriteCmosSensor(0xd3, 0x02);
        HI351WriteCmosSensor(0xd4, 0x91);
        HI351WriteCmosSensor(0xd5, 0x48);
        HI351WriteCmosSensor(0xd6, 0x92);
        HI351WriteCmosSensor(0xd7, 0x00);
                            
        HI351WriteCmosSensor(0xd8, 0x93);
        HI351WriteCmosSensor(0xd9, 0x04);
        HI351WriteCmosSensor(0xda, 0x94);
        HI351WriteCmosSensor(0xdb, 0x02);
        HI351WriteCmosSensor(0xdc, 0x95);
        HI351WriteCmosSensor(0xdd, 0x64);
        HI351WriteCmosSensor(0xde, 0x96);
        HI351WriteCmosSensor(0xdf, 0x14);
                            
        HI351WriteCmosSensor(0xe0, 0x97);
        HI351WriteCmosSensor(0xe1, 0x90);
        HI351WriteCmosSensor(0xe2, 0xb0);
        HI351WriteCmosSensor(0xe3, 0x60);
        HI351WriteCmosSensor(0xe4, 0xb1);
        HI351WriteCmosSensor(0xe5, 0x90);
        HI351WriteCmosSensor(0xe6, 0xb2);
        HI351WriteCmosSensor(0xe7, 0x10);
                            
        HI351WriteCmosSensor(0xe8, 0xb3);
        HI351WriteCmosSensor(0xe9, 0x08);
        HI351WriteCmosSensor(0xea, 0xb4);
        HI351WriteCmosSensor(0xeb, 0x04);
        HI351WriteCmosSensor(0xec, 0x03);//12 page
        HI351WriteCmosSensor(0xed, 0x12);
        HI351WriteCmosSensor(0xee, 0x10);
        HI351WriteCmosSensor(0xef, 0x03);
                            
        HI351WriteCmosSensor(0xf0, 0x11);
        HI351WriteCmosSensor(0xf1, 0x29);
        HI351WriteCmosSensor(0xf2, 0x12);
        HI351WriteCmosSensor(0xf3, 0x08);
        HI351WriteCmosSensor(0xf4, 0x40);
        HI351WriteCmosSensor(0xf5, 0x37);
        HI351WriteCmosSensor(0xf6, 0x41);
        HI351WriteCmosSensor(0xf7, 0x24);
                            
        HI351WriteCmosSensor(0xf8, 0x42);
        HI351WriteCmosSensor(0xf9, 0x00);
        HI351WriteCmosSensor(0xfa, 0x43);
        HI351WriteCmosSensor(0xfb, 0x62);
        HI351WriteCmosSensor(0xfc, 0x44);
        HI351WriteCmosSensor(0xfd, 0x02);
        
            
            // Page 0xdb          
        HI351WriteCmosSensor(0x03, 0xdb);
                            
        HI351WriteCmosSensor(0x10, 0x45);
        HI351WriteCmosSensor(0x11, 0x0a);
        HI351WriteCmosSensor(0x12, 0x46);
        HI351WriteCmosSensor(0x13, 0x40);
        HI351WriteCmosSensor(0x14, 0x60);
        HI351WriteCmosSensor(0x15, 0x02);
        HI351WriteCmosSensor(0x16, 0x61);
        HI351WriteCmosSensor(0x17, 0x04);
                            
        HI351WriteCmosSensor(0x18, 0x62);
        HI351WriteCmosSensor(0x19, 0x4b);
        HI351WriteCmosSensor(0x1a, 0x63);
        HI351WriteCmosSensor(0x1b, 0x41);
        HI351WriteCmosSensor(0x1c, 0x64);
        HI351WriteCmosSensor(0x1d, 0x14);
        HI351WriteCmosSensor(0x1e, 0x65);
        HI351WriteCmosSensor(0x1f, 0x00);
                            
        HI351WriteCmosSensor(0x20, 0x68);
        HI351WriteCmosSensor(0x21, 0x0a);
        HI351WriteCmosSensor(0x22, 0x69);
        HI351WriteCmosSensor(0x23, 0x04);
        HI351WriteCmosSensor(0x24, 0x6a);
        HI351WriteCmosSensor(0x25, 0x0a);
        HI351WriteCmosSensor(0x26, 0x6b);
        HI351WriteCmosSensor(0x27, 0x0a);
                            
        HI351WriteCmosSensor(0x28, 0x6c);
        HI351WriteCmosSensor(0x29, 0x24);
        HI351WriteCmosSensor(0x2a, 0x6d);
        HI351WriteCmosSensor(0x2b, 0x01);
        HI351WriteCmosSensor(0x2c, 0x70);
        HI351WriteCmosSensor(0x2d, 0x21);
        HI351WriteCmosSensor(0x2e, 0x71);
        HI351WriteCmosSensor(0x2f, 0x3d);
                            
        HI351WriteCmosSensor(0x30, 0x80);
        HI351WriteCmosSensor(0x31, 0x30);
        HI351WriteCmosSensor(0x32, 0x81);
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x34, 0x82);
        HI351WriteCmosSensor(0x35, 0x90);
        HI351WriteCmosSensor(0x36, 0x83);
        HI351WriteCmosSensor(0x37, 0x03);
                            
        HI351WriteCmosSensor(0x38, 0x84);
        HI351WriteCmosSensor(0x39, 0x28);
        HI351WriteCmosSensor(0x3a, 0x85);
        HI351WriteCmosSensor(0x3b, 0x80);
        HI351WriteCmosSensor(0x3c, 0x86);
        HI351WriteCmosSensor(0x3d, 0xa0);
        HI351WriteCmosSensor(0x3e, 0x87);
        HI351WriteCmosSensor(0x3f, 0x00);
                            
        HI351WriteCmosSensor(0x40, 0x88);
        HI351WriteCmosSensor(0x41, 0xa0);
        HI351WriteCmosSensor(0x42, 0x89);
        HI351WriteCmosSensor(0x43, 0x00);
        HI351WriteCmosSensor(0x44, 0x8a);
        HI351WriteCmosSensor(0x45, 0x40);
        HI351WriteCmosSensor(0x46, 0x8b);
        HI351WriteCmosSensor(0x47, 0x10);
                            
        HI351WriteCmosSensor(0x48, 0x8c);
        HI351WriteCmosSensor(0x49, 0x01);
        HI351WriteCmosSensor(0x4a, 0x8d);
        HI351WriteCmosSensor(0x4b, 0x01);
        HI351WriteCmosSensor(0x4c, 0xe6);
        HI351WriteCmosSensor(0x4d, 0xff);
        HI351WriteCmosSensor(0x4e, 0xe7);
        HI351WriteCmosSensor(0x4f, 0x18);
                            
        HI351WriteCmosSensor(0x50, 0xe8);
        HI351WriteCmosSensor(0x51, 0x0a);
        HI351WriteCmosSensor(0x52, 0xe9);
        HI351WriteCmosSensor(0x53, 0x04);
        HI351WriteCmosSensor(0x54, 0x03);
        HI351WriteCmosSensor(0x55, 0x13);//13 page
        HI351WriteCmosSensor(0x56, 0x10);
        HI351WriteCmosSensor(0x57, 0x3f);
                            
        HI351WriteCmosSensor(0x58, 0x20);
        HI351WriteCmosSensor(0x59, 0x20);
        HI351WriteCmosSensor(0x5a, 0x21);
        HI351WriteCmosSensor(0x5b, 0x30);
        HI351WriteCmosSensor(0x5c, 0x22);
        HI351WriteCmosSensor(0x5d, 0x36);
        HI351WriteCmosSensor(0x5e, 0x23);
        HI351WriteCmosSensor(0x5f, 0x6a);
                            
        HI351WriteCmosSensor(0x60, 0x24);
        HI351WriteCmosSensor(0x61, 0xa0);
        HI351WriteCmosSensor(0x62, 0x25);
        HI351WriteCmosSensor(0x63, 0xc0);
        HI351WriteCmosSensor(0x64, 0x26);
        HI351WriteCmosSensor(0x65, 0xe0);
        HI351WriteCmosSensor(0x66, 0x27);
        HI351WriteCmosSensor(0x67, 0x0a);
                            
        HI351WriteCmosSensor(0x68, 0x28);
        HI351WriteCmosSensor(0x69, 0x0a);
        HI351WriteCmosSensor(0x6a, 0x29);
        HI351WriteCmosSensor(0x6b, 0x0a);
        HI351WriteCmosSensor(0x6c, 0x2a);
        HI351WriteCmosSensor(0x6d, 0x0a);
        HI351WriteCmosSensor(0x6e, 0x2b);
        HI351WriteCmosSensor(0x6f, 0x08);
                            
        HI351WriteCmosSensor(0x70, 0x2c);
        HI351WriteCmosSensor(0x71, 0x08);
        HI351WriteCmosSensor(0x72, 0x2d);
        HI351WriteCmosSensor(0x73, 0x08);
        HI351WriteCmosSensor(0x74, 0x2e);
        HI351WriteCmosSensor(0x75, 0x08);
        HI351WriteCmosSensor(0x76, 0x2f);
        HI351WriteCmosSensor(0x77, 0x04);
                            
        HI351WriteCmosSensor(0x78, 0x30);
        HI351WriteCmosSensor(0x79, 0x02);
        HI351WriteCmosSensor(0x7a, 0x31);
        HI351WriteCmosSensor(0x7b, 0x78);
        HI351WriteCmosSensor(0x7c, 0x32);
        HI351WriteCmosSensor(0x7d, 0x01);
        HI351WriteCmosSensor(0x7e, 0x33);
        HI351WriteCmosSensor(0x7f, 0xc0);
                            
        HI351WriteCmosSensor(0x80, 0x34);
        HI351WriteCmosSensor(0x81, 0xf0);
        HI351WriteCmosSensor(0x82, 0x35);
        HI351WriteCmosSensor(0x83, 0x10);
        HI351WriteCmosSensor(0x84, 0x36);
        HI351WriteCmosSensor(0x85, 0x40);
        HI351WriteCmosSensor(0x86, 0xa0);
        HI351WriteCmosSensor(0x87, 0x07);
                            
        HI351WriteCmosSensor(0x88, 0xa8);
        HI351WriteCmosSensor(0x89, 0x20);
        HI351WriteCmosSensor(0x8a, 0xa9);
        HI351WriteCmosSensor(0x8b, 0x20);
        HI351WriteCmosSensor(0x8c, 0xaa);
        HI351WriteCmosSensor(0x8d, 0x0a);
        HI351WriteCmosSensor(0x8e, 0xab);
        HI351WriteCmosSensor(0x8f, 0x02);
                            
        HI351WriteCmosSensor(0x90, 0xc0);
        HI351WriteCmosSensor(0x91, 0x27);
        HI351WriteCmosSensor(0x92, 0xc2);
        HI351WriteCmosSensor(0x93, 0x08);
        HI351WriteCmosSensor(0x94, 0xc3);
        HI351WriteCmosSensor(0x95, 0x08);
        HI351WriteCmosSensor(0x96, 0xc4);
        HI351WriteCmosSensor(0x97, 0x46);
                            
        HI351WriteCmosSensor(0x98, 0xc5);
        HI351WriteCmosSensor(0x99, 0x78);
        HI351WriteCmosSensor(0x9a, 0xc6);
        HI351WriteCmosSensor(0x9b, 0xf0);
        HI351WriteCmosSensor(0x9c, 0xc7);
        HI351WriteCmosSensor(0x9d, 0x10);
        HI351WriteCmosSensor(0x9e, 0xc8);
        HI351WriteCmosSensor(0x9f, 0x44);
                            
        HI351WriteCmosSensor(0xa0, 0xc9);
        HI351WriteCmosSensor(0xa1, 0x87);
        HI351WriteCmosSensor(0xa2, 0xca);
        HI351WriteCmosSensor(0xa3, 0xff);
        HI351WriteCmosSensor(0xa4, 0xcb);
        HI351WriteCmosSensor(0xa5, 0x20);
        HI351WriteCmosSensor(0xa6, 0xcc);
        HI351WriteCmosSensor(0xa7, 0x61);
                            
        HI351WriteCmosSensor(0xa8, 0xcd);
        HI351WriteCmosSensor(0xa9, 0x87);
        HI351WriteCmosSensor(0xaa, 0xce);
        HI351WriteCmosSensor(0xab, 0x8a);
        HI351WriteCmosSensor(0xac, 0xcf);
        HI351WriteCmosSensor(0xad, 0xa5);
        HI351WriteCmosSensor(0xae, 0x03);//14 page
        HI351WriteCmosSensor(0xaf, 0x14);
                            
        HI351WriteCmosSensor(0xb0, 0x10);
        HI351WriteCmosSensor(0xb1, 0x27);
        HI351WriteCmosSensor(0xb2, 0x11);
        HI351WriteCmosSensor(0xb3, 0x02);
        HI351WriteCmosSensor(0xb4, 0x12);
        HI351WriteCmosSensor(0xb5, 0x50);
        HI351WriteCmosSensor(0xb6, 0x13);
        HI351WriteCmosSensor(0xb7, 0x88);               
        HI351WriteCmosSensor(0xb8, 0x14);
        HI351WriteCmosSensor(0xb9, 0x28);
        HI351WriteCmosSensor(0xba, 0x15);
        HI351WriteCmosSensor(0xbb, 0x32);
        HI351WriteCmosSensor(0xbc, 0x16);
        HI351WriteCmosSensor(0xbd, 0x30);
        HI351WriteCmosSensor(0xbe, 0x17);
        HI351WriteCmosSensor(0xbf, 0x2d);               
        HI351WriteCmosSensor(0xc0, 0x18);
        HI351WriteCmosSensor(0xc1, 0x60);
        HI351WriteCmosSensor(0xc2, 0x19);
        HI351WriteCmosSensor(0xc3, 0x62);
        HI351WriteCmosSensor(0xc4, 0x1a);
        HI351WriteCmosSensor(0xc5, 0x62);
        HI351WriteCmosSensor(0xc6, 0x20);
        HI351WriteCmosSensor(0xc7, 0x82);           
        HI351WriteCmosSensor(0xc8, 0x21);
        HI351WriteCmosSensor(0xc9, 0x03);
        HI351WriteCmosSensor(0xca, 0x22);
        HI351WriteCmosSensor(0xcb, 0x05);
        HI351WriteCmosSensor(0xcc, 0x23);
        HI351WriteCmosSensor(0xcd, 0x06);
        HI351WriteCmosSensor(0xce, 0x24);
        HI351WriteCmosSensor(0xcf, 0x08);       
        HI351WriteCmosSensor(0xd0, 0x25);
        HI351WriteCmosSensor(0xd1, 0x38);
        HI351WriteCmosSensor(0xd2, 0x26);
        HI351WriteCmosSensor(0xd3, 0x30);
        HI351WriteCmosSensor(0xd4, 0x27);
        HI351WriteCmosSensor(0xd5, 0x20);
        HI351WriteCmosSensor(0xd6, 0x28);
        HI351WriteCmosSensor(0xd7, 0x10);   
        HI351WriteCmosSensor(0xd8, 0x29);
        HI351WriteCmosSensor(0xd9, 0x00);
        HI351WriteCmosSensor(0xda, 0x2a);
        HI351WriteCmosSensor(0xdb, 0x16);
        HI351WriteCmosSensor(0xdc, 0x2b);
        HI351WriteCmosSensor(0xdd, 0x16);
        HI351WriteCmosSensor(0xde, 0x2c);
        HI351WriteCmosSensor(0xdf, 0x16);   
        HI351WriteCmosSensor(0xe0, 0x2d);
        HI351WriteCmosSensor(0xe1, 0x4c);
        HI351WriteCmosSensor(0xe2, 0x2e);
        HI351WriteCmosSensor(0xe3, 0x4e);
        HI351WriteCmosSensor(0xe4, 0x2f);
        HI351WriteCmosSensor(0xe5, 0x50);
        HI351WriteCmosSensor(0xe6, 0x30);
        HI351WriteCmosSensor(0xe7, 0x82);   
        HI351WriteCmosSensor(0xe8, 0x31);
        HI351WriteCmosSensor(0xe9, 0x02);
        HI351WriteCmosSensor(0xea, 0x32);
        HI351WriteCmosSensor(0xeb, 0x04);
        HI351WriteCmosSensor(0xec, 0x33);
        HI351WriteCmosSensor(0xed, 0x04);
        HI351WriteCmosSensor(0xee, 0x34);
        HI351WriteCmosSensor(0xef, 0x0a);
                            
        HI351WriteCmosSensor(0xf0, 0x35);
        HI351WriteCmosSensor(0xf1, 0x46);
        HI351WriteCmosSensor(0xf2, 0x36);
        HI351WriteCmosSensor(0xf3, 0x32);
        HI351WriteCmosSensor(0xf4, 0x37);
        HI351WriteCmosSensor(0xf5, 0x2c);
        HI351WriteCmosSensor(0xf6, 0x38);
        HI351WriteCmosSensor(0xf7, 0x18);
        HI351WriteCmosSensor(0xf8, 0x39);
        HI351WriteCmosSensor(0xf9, 0x00);
        HI351WriteCmosSensor(0xfa, 0x3a);
        HI351WriteCmosSensor(0xfb, 0x28);
        HI351WriteCmosSensor(0xfc, 0x3b);
        HI351WriteCmosSensor(0xfd, 0x28);
        
                   
        HI351WriteCmosSensor(0x03, 0xdc);
        HI351WriteCmosSensor(0x10, 0x3c);
        HI351WriteCmosSensor(0x11, 0x28);
        HI351WriteCmosSensor(0x12, 0x3d);
        HI351WriteCmosSensor(0x13, 0x10);
        HI351WriteCmosSensor(0x14, 0x3e);
        HI351WriteCmosSensor(0x15, 0x22);
        HI351WriteCmosSensor(0x16, 0x3f);
        HI351WriteCmosSensor(0x17, 0x20);                   
        HI351WriteCmosSensor(0x18, 0x40);
        HI351WriteCmosSensor(0x19, 0x84);
        HI351WriteCmosSensor(0x1a, 0x41);
        HI351WriteCmosSensor(0x1b, 0x28);
        HI351WriteCmosSensor(0x1c, 0x42);
        HI351WriteCmosSensor(0x1d, 0xa0);
        HI351WriteCmosSensor(0x1e, 0x43);
        HI351WriteCmosSensor(0x1f, 0x28);                   
        HI351WriteCmosSensor(0x20, 0x44);
        HI351WriteCmosSensor(0x21, 0x20);
        HI351WriteCmosSensor(0x22, 0x45);
        HI351WriteCmosSensor(0x23, 0x16);
        HI351WriteCmosSensor(0x24, 0x46);
        HI351WriteCmosSensor(0x25, 0x1a);
        HI351WriteCmosSensor(0x26, 0x47);
        HI351WriteCmosSensor(0x27, 0x0a);                   
        HI351WriteCmosSensor(0x28, 0x48);
        HI351WriteCmosSensor(0x29, 0x0a);
        HI351WriteCmosSensor(0x2a, 0x49);
        HI351WriteCmosSensor(0x2b, 0x0a);
        HI351WriteCmosSensor(0x2c, 0x50);
        HI351WriteCmosSensor(0x2d, 0x34);
        HI351WriteCmosSensor(0x2e, 0x51);
        HI351WriteCmosSensor(0x2f, 0x34);                   
        HI351WriteCmosSensor(0x30, 0x52);
        HI351WriteCmosSensor(0x31, 0x50);
        HI351WriteCmosSensor(0x32, 0x53);
        HI351WriteCmosSensor(0x33, 0x24);
        HI351WriteCmosSensor(0x34, 0x54);
        HI351WriteCmosSensor(0x35, 0x32);
        HI351WriteCmosSensor(0x36, 0x55);
        HI351WriteCmosSensor(0x37, 0x32);                   
        HI351WriteCmosSensor(0x38, 0x56);
        HI351WriteCmosSensor(0x39, 0x32);
        HI351WriteCmosSensor(0x3a, 0x57);
        HI351WriteCmosSensor(0x3b, 0x10);
        HI351WriteCmosSensor(0x3c, 0x58);
        HI351WriteCmosSensor(0x3d, 0x14);
        HI351WriteCmosSensor(0x3e, 0x59);
        HI351WriteCmosSensor(0x3f, 0x12);                   
        HI351WriteCmosSensor(0x40, 0x60);
        HI351WriteCmosSensor(0x41, 0x03);
        HI351WriteCmosSensor(0x42, 0x61);
        HI351WriteCmosSensor(0x43, 0xa0);
        HI351WriteCmosSensor(0x44, 0x62);
        HI351WriteCmosSensor(0x45, 0x98);
        HI351WriteCmosSensor(0x46, 0x63);
        HI351WriteCmosSensor(0x47, 0xe4);
                            
        HI351WriteCmosSensor(0x48, 0x64);
        HI351WriteCmosSensor(0x49, 0xa4);
        HI351WriteCmosSensor(0x4a, 0x65);
        HI351WriteCmosSensor(0x4b, 0x7d);
        HI351WriteCmosSensor(0x4c, 0x66);
        HI351WriteCmosSensor(0x4d, 0x4b);
        HI351WriteCmosSensor(0x4e, 0x70);
        HI351WriteCmosSensor(0x4f, 0x10);
                       
        HI351WriteCmosSensor(0x50, 0x71);
        HI351WriteCmosSensor(0x51, 0x10);
        HI351WriteCmosSensor(0x52, 0x72);
        HI351WriteCmosSensor(0x53, 0x10);
        HI351WriteCmosSensor(0x54, 0x73);
        HI351WriteCmosSensor(0x55, 0x10);
        HI351WriteCmosSensor(0x56, 0x74);
        HI351WriteCmosSensor(0x57, 0x10);
                            
        HI351WriteCmosSensor(0x58, 0x75);
        HI351WriteCmosSensor(0x59, 0x10);
        HI351WriteCmosSensor(0x5a, 0x76);
        HI351WriteCmosSensor(0x5b, 0x60);
        HI351WriteCmosSensor(0x5c, 0x77);
        HI351WriteCmosSensor(0x5d, 0x58);
        HI351WriteCmosSensor(0x5e, 0x78);
        HI351WriteCmosSensor(0x5f, 0x5a);
                            
        HI351WriteCmosSensor(0x60, 0x79);
        HI351WriteCmosSensor(0x61, 0x60);
        HI351WriteCmosSensor(0x62, 0x7a);
        HI351WriteCmosSensor(0x63, 0x5a);
        HI351WriteCmosSensor(0x64, 0x7b);
        HI351WriteCmosSensor(0x65, 0x50);
        
            //////////////////
            // dd Page (DMA Indoor)
            //////////////////
        HI351WriteCmosSensor(0x03, 0xdd);
        HI351WriteCmosSensor(0x10, 0x03);//Indoor Page11
        HI351WriteCmosSensor(0x11, 0x11);
        HI351WriteCmosSensor(0x12, 0x10);//Indoor 0x1110
        HI351WriteCmosSensor(0x13, 0x13);
        HI351WriteCmosSensor(0x14, 0x11);//Indoor 0x1111
        HI351WriteCmosSensor(0x15, 0x0c);
        HI351WriteCmosSensor(0x16, 0x12);//Indoor 0x1112
        HI351WriteCmosSensor(0x17, 0x22);
        HI351WriteCmosSensor(0x18, 0x13);//Indoor 0x1113
        HI351WriteCmosSensor(0x19, 0x22);
        HI351WriteCmosSensor(0x1a, 0x14);//Indoor 0x1114
        HI351WriteCmosSensor(0x1b, 0x3a);
        HI351WriteCmosSensor(0x1c, 0x30);//Indoor 0x1130
        HI351WriteCmosSensor(0x1d, 0x20);
        HI351WriteCmosSensor(0x1e, 0x31);//Indoor 0x1131
        HI351WriteCmosSensor(0x1f, 0x20);
        
        HI351WriteCmosSensor(0x20, 0x32); //Indoor 0x1132 //STEVE Lum. Level. in DLPF                    
        HI351WriteCmosSensor(0x21, 0x8b); //52                                                          
        HI351WriteCmosSensor(0x22, 0x33); //Indoor 0x1133                                               
        HI351WriteCmosSensor(0x23, 0x54); //3b                                                          
        HI351WriteCmosSensor(0x24, 0x34); //Indoor 0x1134                                               
        HI351WriteCmosSensor(0x25, 0x2c); //1d                                                          
        HI351WriteCmosSensor(0x26, 0x35); //Indoor 0x1135                                               
        HI351WriteCmosSensor(0x27, 0x29); //21                                                          
        HI351WriteCmosSensor(0x28, 0x36); //Indoor 0x1136                                               
        HI351WriteCmosSensor(0x29, 0x18); //1b                                                          
        HI351WriteCmosSensor(0x2a, 0x37); //Indoor 0x1137                                               
        HI351WriteCmosSensor(0x2b, 0x1e); //21                                                          
        HI351WriteCmosSensor(0x2c, 0x38); //Indoor 0x1138                                               
        HI351WriteCmosSensor(0x2d, 0x17); //18                                                          
            
        HI351WriteCmosSensor(0x2e, 0x39);//Indoor 0x1139 gain 1           
        HI351WriteCmosSensor(0x2f, 0x34);    //r2 1
        HI351WriteCmosSensor(0x30, 0x3a);//Indoor 0x113a
        HI351WriteCmosSensor(0x31, 0x38);
        HI351WriteCmosSensor(0x32, 0x3b);//Indoor 0x113b
        HI351WriteCmosSensor(0x33, 0x3a);
        HI351WriteCmosSensor(0x34, 0x3c);//Indoor 0x113c
        HI351WriteCmosSensor(0x35, 0x38);   //18
        HI351WriteCmosSensor(0x36, 0x3d);//Indoor 0x113d
        HI351WriteCmosSensor(0x37, 0x2a);   //18
        HI351WriteCmosSensor(0x38, 0x3e);//Indoor 0x113e
        HI351WriteCmosSensor(0x39, 0x26);   //18
        HI351WriteCmosSensor(0x3a, 0x3f);//Indoor 0x113f
        HI351WriteCmosSensor(0x3b, 0x22);
        HI351WriteCmosSensor(0x3c, 0x40);//Indoor 0x1140 gain 8
        HI351WriteCmosSensor(0x3d, 0x20);
        HI351WriteCmosSensor(0x3e, 0x41);//Indoor 0x1141
        HI351WriteCmosSensor(0x3f, 0x50);
        HI351WriteCmosSensor(0x40, 0x42);//Indoor 0x1142
        HI351WriteCmosSensor(0x41, 0x50);
        HI351WriteCmosSensor(0x42, 0x43);//Indoor 0x1143
        HI351WriteCmosSensor(0x43, 0x50);
        HI351WriteCmosSensor(0x44, 0x44);//Indoor 0x1144
        HI351WriteCmosSensor(0x45, 0x80);
        HI351WriteCmosSensor(0x46, 0x45);//Indoor 0x1145
        HI351WriteCmosSensor(0x47, 0x80);
        HI351WriteCmosSensor(0x48, 0x46);//Indoor 0x1146
        HI351WriteCmosSensor(0x49, 0x80);
        HI351WriteCmosSensor(0x4a, 0x47);//Indoor 0x1147
        HI351WriteCmosSensor(0x4b, 0x80);
        HI351WriteCmosSensor(0x4c, 0x48);//Indoor 0x1148
        HI351WriteCmosSensor(0x4d, 0x80);
        HI351WriteCmosSensor(0x4e, 0x49);//Indoor 0x1149
        HI351WriteCmosSensor(0x4f, 0xfc); //high_clip_start
        HI351WriteCmosSensor(0x50, 0x4a);//Indoor 0x114a
        HI351WriteCmosSensor(0x51, 0xfc);
        HI351WriteCmosSensor(0x52, 0x4b);//Indoor 0x114b
        HI351WriteCmosSensor(0x53, 0xfc);
        HI351WriteCmosSensor(0x54, 0x4c);//Indoor 0x114c
        HI351WriteCmosSensor(0x55, 0xfc);
        HI351WriteCmosSensor(0x56, 0x4d);//Indoor 0x114d
        HI351WriteCmosSensor(0x57, 0xfc);
        HI351WriteCmosSensor(0x58, 0x4e);//Indoor 0x114e
        HI351WriteCmosSensor(0x59, 0xf0);   //Lv 6 h_clip
        HI351WriteCmosSensor(0x5a, 0x4f);//Indoor 0x114f
        HI351WriteCmosSensor(0x5b, 0xf0);   //Lv 7 h_clip 
        HI351WriteCmosSensor(0x5c, 0x50);//Indoor 0x1150 clip 8
        HI351WriteCmosSensor(0x5d, 0xf0);
        HI351WriteCmosSensor(0x5e, 0x51);//Indoor 0x1151
        HI351WriteCmosSensor(0x5f, 0x08); //color gain start
        HI351WriteCmosSensor(0x60, 0x52);//Indoor 0x1152
        HI351WriteCmosSensor(0x61, 0x08);
        HI351WriteCmosSensor(0x62, 0x53);//Indoor 0x1153
        HI351WriteCmosSensor(0x63, 0x08);
        HI351WriteCmosSensor(0x64, 0x54);//Indoor 0x1154
        HI351WriteCmosSensor(0x65, 0x08);
        HI351WriteCmosSensor(0x66, 0x55);//Indoor 0x1155
        HI351WriteCmosSensor(0x67, 0x08);
        HI351WriteCmosSensor(0x68, 0x56);//Indoor 0x1156
        HI351WriteCmosSensor(0x69, 0x08);
        HI351WriteCmosSensor(0x6a, 0x57);//Indoor 0x1157
        HI351WriteCmosSensor(0x6b, 0x08);
        HI351WriteCmosSensor(0x6c, 0x58);//Indoor 0x1158
        HI351WriteCmosSensor(0x6d, 0x08); //color gain end
        HI351WriteCmosSensor(0x6e, 0x59);//Indoor 0x1159
        HI351WriteCmosSensor(0x6f, 0x10); //color ofs lmt start
        HI351WriteCmosSensor(0x70, 0x5a);//Indoor 0x115a
        HI351WriteCmosSensor(0x71, 0x10);
        HI351WriteCmosSensor(0x72, 0x5b);//Indoor 0x115b
        HI351WriteCmosSensor(0x73, 0x10);
        HI351WriteCmosSensor(0x74, 0x5c);//Indoor 0x115c
        HI351WriteCmosSensor(0x75, 0x10);
        HI351WriteCmosSensor(0x76, 0x5d);//Indoor 0x115d
        HI351WriteCmosSensor(0x77, 0x10);
        HI351WriteCmosSensor(0x78, 0x5e);//Indoor 0x115e
        HI351WriteCmosSensor(0x79, 0x10);
        HI351WriteCmosSensor(0x7a, 0x5f);//Indoor 0x115f
        HI351WriteCmosSensor(0x7b, 0x10);
        HI351WriteCmosSensor(0x7c, 0x60);//Indoor 0x1160
        HI351WriteCmosSensor(0x7d, 0x10);//color ofs lmt end
        HI351WriteCmosSensor(0x7e, 0x61);//Indoor 0x1161
        HI351WriteCmosSensor(0x7f, 0xc0);
        HI351WriteCmosSensor(0x80, 0x62);//Indoor 0x1162
        HI351WriteCmosSensor(0x81, 0xf0);
        HI351WriteCmosSensor(0x82, 0x63);//Indoor 0x1163
        HI351WriteCmosSensor(0x83, 0x80);
        HI351WriteCmosSensor(0x84, 0x64);//Indoor 0x1164
        HI351WriteCmosSensor(0x85, 0x40);
        HI351WriteCmosSensor(0x86, 0x65);//Indoor 0x1165
        HI351WriteCmosSensor(0x87, 0x60);
        HI351WriteCmosSensor(0x88, 0x66);//Indoor 0x1166
        HI351WriteCmosSensor(0x89, 0x60);
        HI351WriteCmosSensor(0x8a, 0x67);//Indoor 0x1167
        HI351WriteCmosSensor(0x8b, 0x60);
        HI351WriteCmosSensor(0x8c, 0x68);//Indoor 0x1168
        HI351WriteCmosSensor(0x8d, 0x80);
        HI351WriteCmosSensor(0x8e, 0x69);//Indoor 0x1169
        HI351WriteCmosSensor(0x8f, 0x40);
        HI351WriteCmosSensor(0x90, 0x6a);//Indoor 0x116a     //Imp Lv2 High Gain
        HI351WriteCmosSensor(0x91, 0x60);
        HI351WriteCmosSensor(0x92, 0x6b);//Indoor 0x116b     //Imp Lv2 Middle Gain
        HI351WriteCmosSensor(0x93, 0x60);
        HI351WriteCmosSensor(0x94, 0x6c);//Indoor 0x116c     //Imp Lv2 Low Gain
        HI351WriteCmosSensor(0x95, 0x60);
        HI351WriteCmosSensor(0x96, 0x6d);//Indoor 0x116d
        HI351WriteCmosSensor(0x97, 0x80);
        HI351WriteCmosSensor(0x98, 0x6e);//Indoor 0x116e
        HI351WriteCmosSensor(0x99, 0x40);
        HI351WriteCmosSensor(0x9a, 0x6f);//Indoor 0x116f    //Imp Lv3 Hi Gain
        HI351WriteCmosSensor(0x9b, 0x60);
        HI351WriteCmosSensor(0x9c, 0x70);//Indoor 0x1170    //Imp Lv3 Middle Gain
        HI351WriteCmosSensor(0x9d, 0x60);
        HI351WriteCmosSensor(0x9e, 0x71);//Indoor 0x1171    //Imp Lv3 Low Gain
        HI351WriteCmosSensor(0x9f, 0x60);
        HI351WriteCmosSensor(0xa0, 0x72);//Indoor 0x1172
        HI351WriteCmosSensor(0xa1, 0x6e);
        HI351WriteCmosSensor(0xa2, 0x73);//Indoor 0x1173
        HI351WriteCmosSensor(0xa3, 0x3a);
        HI351WriteCmosSensor(0xa4, 0x74);//Indoor 0x1174    //Imp Lv4 Hi Gain
        HI351WriteCmosSensor(0xa5, 0x60);
        HI351WriteCmosSensor(0xa6, 0x75);//Indoor 0x1175    //Imp Lv4 Middle Gain
        HI351WriteCmosSensor(0xa7, 0x60);
        HI351WriteCmosSensor(0xa8, 0x76);//Indoor 0x1176    //Imp Lv4 Low Gain
        HI351WriteCmosSensor(0xa9, 0x60);//18
        HI351WriteCmosSensor(0xaa, 0x77);//Indoor 0x1177    //Imp Lv5 Hi Th
        HI351WriteCmosSensor(0xab, 0x6e);
        HI351WriteCmosSensor(0xac, 0x78);//Indoor 0x1178    //Imp Lv5 Middle Th
        HI351WriteCmosSensor(0xad, 0x66);
        HI351WriteCmosSensor(0xae, 0x79);//Indoor 0x1179    //Imp Lv5 Hi Gain
        HI351WriteCmosSensor(0xaf, 0x50);
        HI351WriteCmosSensor(0xb0, 0x7a);//Indoor 0x117a    //Imp Lv5 Middle Gain
        HI351WriteCmosSensor(0xb1, 0x50);
        HI351WriteCmosSensor(0xb2, 0x7b);//Indoor 0x117b    //Imp Lv5 Low Gain
        HI351WriteCmosSensor(0xb3, 0x50);
        HI351WriteCmosSensor(0xb4, 0x7c);//Indoor 0x117c    //Imp Lv6 Hi Th
        HI351WriteCmosSensor(0xb5, 0x5c);
        HI351WriteCmosSensor(0xb6, 0x7d);//Indoor 0x117d    //Imp Lv6 Middle Th
        HI351WriteCmosSensor(0xb7, 0x30);
        HI351WriteCmosSensor(0xb8, 0x7e);//Indoor 0x117e    //Imp Lv6 Hi Gain
        HI351WriteCmosSensor(0xb9, 0x44);
        HI351WriteCmosSensor(0xba, 0x7f);//Indoor 0x117f    //Imp Lv6 Middle Gain
        HI351WriteCmosSensor(0xbb, 0x44);
        HI351WriteCmosSensor(0xbc, 0x80);//Indoor 0x1180    //Imp Lv6 Low Gain
        HI351WriteCmosSensor(0xbd, 0x44); 
        HI351WriteCmosSensor(0xbe, 0x81);//Indoor 0x1181
        HI351WriteCmosSensor(0xbf, 0x62);
        HI351WriteCmosSensor(0xc0, 0x82);//Indoor 0x1182
        HI351WriteCmosSensor(0xc1, 0x26);
        HI351WriteCmosSensor(0xc2, 0x83);//Indoor 0x1183    //Imp Lv7 Hi Gain
        HI351WriteCmosSensor(0xc3, 0x3e);
        HI351WriteCmosSensor(0xc4, 0x84);//Indoor 0x1184    //Imp Lv7 Middle Gain
        HI351WriteCmosSensor(0xc5, 0x3e);
        HI351WriteCmosSensor(0xc6, 0x85);//Indoor 0x1185    //Imp Lv7 Low Gain
        HI351WriteCmosSensor(0xc7, 0x3e);
        HI351WriteCmosSensor(0xc8, 0x86);//Indoor 0x1186
        HI351WriteCmosSensor(0xc9, 0x62);
        HI351WriteCmosSensor(0xca, 0x87);//Indoor 0x1187
        HI351WriteCmosSensor(0xcb, 0x26);
        HI351WriteCmosSensor(0xcc, 0x88);//Indoor 0x1188
        HI351WriteCmosSensor(0xcd, 0x30);
        HI351WriteCmosSensor(0xce, 0x89);//Indoor 0x1189
        HI351WriteCmosSensor(0xcf, 0x30);
        HI351WriteCmosSensor(0xd0, 0x8a);//Indoor 0x118a
        HI351WriteCmosSensor(0xd1, 0x30);
        HI351WriteCmosSensor(0xd2, 0x90);//Indoor 0x1190
        HI351WriteCmosSensor(0xd3, 0x00);
        HI351WriteCmosSensor(0xd4, 0x91);//Indoor 0x1191
        HI351WriteCmosSensor(0xd5, 0x4e);
        HI351WriteCmosSensor(0xd6, 0x92);//Indoor 0x1192
        HI351WriteCmosSensor(0xd7, 0x00);
        HI351WriteCmosSensor(0xd8, 0x93);//Indoor 0x1193
        HI351WriteCmosSensor(0xd9, 0x16);
        HI351WriteCmosSensor(0xda, 0x94);//Indoor 0x1194
        HI351WriteCmosSensor(0xdb, 0x01);
        HI351WriteCmosSensor(0xdc, 0x95);//Indoor 0x1195
        HI351WriteCmosSensor(0xdd, 0x80);
        HI351WriteCmosSensor(0xde, 0x96);//Indoor 0x1196
        HI351WriteCmosSensor(0xdf, 0x55);
        HI351WriteCmosSensor(0xe0, 0x97);//Indoor 0x1197
        HI351WriteCmosSensor(0xe1, 0x8d);
        HI351WriteCmosSensor(0xe2, 0xb0);//Indoor 0x11b0
        HI351WriteCmosSensor(0xe3, 0x60);
        HI351WriteCmosSensor(0xe4, 0xb1);//Indoor 0x11b1
        HI351WriteCmosSensor(0xe5, 0x99);
        HI351WriteCmosSensor(0xe6, 0xb2);//Indoor 0x11b2
        HI351WriteCmosSensor(0xe7, 0x19);
        HI351WriteCmosSensor(0xe8, 0xb3);//Indoor 0x11b3
        HI351WriteCmosSensor(0xe9, 0x00);
        HI351WriteCmosSensor(0xea, 0xb4);//Indoor 0x11b4
        HI351WriteCmosSensor(0xeb, 0x00);
        HI351WriteCmosSensor(0xec, 0x03); //12 page
        HI351WriteCmosSensor(0xed, 0x12);
        HI351WriteCmosSensor(0xee, 0x10); //Indoor 0x1210
        HI351WriteCmosSensor(0xef, 0x03);
        HI351WriteCmosSensor(0xf0, 0x11); //Indoor 0x1211
        HI351WriteCmosSensor(0xf1, 0x29);
        HI351WriteCmosSensor(0xf2, 0x12); //Indoor 0x1212
        HI351WriteCmosSensor(0xf3, 0x08);
        HI351WriteCmosSensor(0xf4, 0x40);//Indoor 0x1240
        HI351WriteCmosSensor(0xf5, 0x33); //07
        HI351WriteCmosSensor(0xf6, 0x41);//Indoor 0x1241
        HI351WriteCmosSensor(0xf7, 0x0a); //32
        HI351WriteCmosSensor(0xf8, 0x42);//Indoor 0x1242
        HI351WriteCmosSensor(0xf9, 0x6a); //8c
        HI351WriteCmosSensor(0xfa, 0x43);//Indoor 0x1243
        HI351WriteCmosSensor(0xfb, 0x80);
        HI351WriteCmosSensor(0xfc, 0x44); //Indoor 0x1244
        HI351WriteCmosSensor(0xfd, 0x02);
            
        HI351WriteCmosSensor(0x03, 0xde);
        HI351WriteCmosSensor(0x10, 0x45); //Indoor 0x1245
        HI351WriteCmosSensor(0x11, 0x0a);
        HI351WriteCmosSensor(0x12, 0x46); //Indoor 0x1246
        HI351WriteCmosSensor(0x13, 0x80);
        HI351WriteCmosSensor(0x14, 0x60); //Indoor 0x1260
        HI351WriteCmosSensor(0x15, 0x02);
        HI351WriteCmosSensor(0x16, 0x61); //Indoor 0x1261
        HI351WriteCmosSensor(0x17, 0x04);
        HI351WriteCmosSensor(0x18, 0x62); //Indoor 0x1262
        HI351WriteCmosSensor(0x19, 0x4b);
        HI351WriteCmosSensor(0x1a, 0x63); //Indoor 0x1263
        HI351WriteCmosSensor(0x1b, 0x41);
        HI351WriteCmosSensor(0x1c, 0x64); //Indoor 0x1264
        HI351WriteCmosSensor(0x1d, 0x14);
        HI351WriteCmosSensor(0x1e, 0x65); //Indoor 0x1265
        HI351WriteCmosSensor(0x1f, 0x00);
        HI351WriteCmosSensor(0x20, 0x68); //Indoor 0x1268
        HI351WriteCmosSensor(0x21, 0x0a);
        HI351WriteCmosSensor(0x22, 0x69); //Indoor 0x1269
        HI351WriteCmosSensor(0x23, 0x04);
        HI351WriteCmosSensor(0x24, 0x6a); //Indoor 0x126a
        HI351WriteCmosSensor(0x25, 0x0a);
        HI351WriteCmosSensor(0x26, 0x6b); //Indoor 0x126b
        HI351WriteCmosSensor(0x27, 0x0a);
        HI351WriteCmosSensor(0x28, 0x6c); //Indoor 0x126c
        HI351WriteCmosSensor(0x29, 0x24);
        HI351WriteCmosSensor(0x2a, 0x6d); //Indoor 0x126d
        HI351WriteCmosSensor(0x2b, 0x01);
        HI351WriteCmosSensor(0x2c, 0x70); //Indoor 0x1270
        HI351WriteCmosSensor(0x2d, 0x25);
        HI351WriteCmosSensor(0x2e, 0x71);//Indoor 0x1271
        HI351WriteCmosSensor(0x2f, 0x7f);
        HI351WriteCmosSensor(0x30, 0x80);//Indoor 0x1280
        HI351WriteCmosSensor(0x31, 0x82);//88           
        HI351WriteCmosSensor(0x32, 0x81);//Indoor 0x1281
        HI351WriteCmosSensor(0x33, 0x86); //05          
        HI351WriteCmosSensor(0x34, 0x82);//Indoor 0x1282
        HI351WriteCmosSensor(0x35, 0x06);//13           
        HI351WriteCmosSensor(0x36, 0x83);//Indoor 0x1283
        HI351WriteCmosSensor(0x37, 0x04);//40           
        HI351WriteCmosSensor(0x38, 0x84);//Indoor 0x1284
        HI351WriteCmosSensor(0x39, 0x10);
        HI351WriteCmosSensor(0x3a, 0x85);//Indoor 0x1285
        HI351WriteCmosSensor(0x3b, 0x86);
        HI351WriteCmosSensor(0x3c, 0x86);//Indoor 0x1286
        HI351WriteCmosSensor(0x3d, 0x90);//15           
        HI351WriteCmosSensor(0x3e, 0x87);//Indoor 0x1287
        HI351WriteCmosSensor(0x3f, 0x10);
        HI351WriteCmosSensor(0x40, 0x88);//Indoor 0x1288
        HI351WriteCmosSensor(0x41, 0x3a);
        HI351WriteCmosSensor(0x42, 0x89);//Indoor 0x1289
        HI351WriteCmosSensor(0x43, 0x80);//c0
        HI351WriteCmosSensor(0x44, 0x8a);//Indoor 0x128a
        HI351WriteCmosSensor(0x45, 0xa0);//18
        HI351WriteCmosSensor(0x46, 0x8b); //Indoor 0x128b
        HI351WriteCmosSensor(0x47, 0x03);//05
        HI351WriteCmosSensor(0x48, 0x8c); //Indoor 0x128c
        HI351WriteCmosSensor(0x49, 0x02);
        HI351WriteCmosSensor(0x4a, 0x8d); //Indoor 0x128d
        HI351WriteCmosSensor(0x4b, 0x02);
        HI351WriteCmosSensor(0x4c, 0xe6); //Indoor 0x12e6
        HI351WriteCmosSensor(0x4d, 0xff);
        HI351WriteCmosSensor(0x4e, 0xe7); //Indoor 0x12e7
        HI351WriteCmosSensor(0x4f, 0x18);
        HI351WriteCmosSensor(0x50, 0xe8); //Indoor 0x12e8
        HI351WriteCmosSensor(0x51, 0x0a);
        HI351WriteCmosSensor(0x52, 0xe9); //Indoor 0x12e9
        HI351WriteCmosSensor(0x53, 0x04);
        HI351WriteCmosSensor(0x54, 0x03);//Indoor Page13
        HI351WriteCmosSensor(0x55, 0x13);
        HI351WriteCmosSensor(0x56, 0x10);//Indoor 0x1310
        HI351WriteCmosSensor(0x57, 0x33);
        HI351WriteCmosSensor(0x58, 0x20);//Indoor 0x1320
        HI351WriteCmosSensor(0x59, 0x20);
        HI351WriteCmosSensor(0x5a, 0x21);//Indoor 0x1321
        HI351WriteCmosSensor(0x5b, 0x30);
        HI351WriteCmosSensor(0x5c, 0x22);//Indoor 0x1322
        HI351WriteCmosSensor(0x5d, 0x36);
        HI351WriteCmosSensor(0x5e, 0x23);//Indoor 0x1323
        HI351WriteCmosSensor(0x5f, 0x6a);
        HI351WriteCmosSensor(0x60, 0x24);//Indoor 0x1324
        HI351WriteCmosSensor(0x61, 0xa0);
        HI351WriteCmosSensor(0x62, 0x25);//Indoor 0x1325
        HI351WriteCmosSensor(0x63, 0xc0);
        HI351WriteCmosSensor(0x64, 0x26);//Indoor 0x1326
        HI351WriteCmosSensor(0x65, 0xe0);
        HI351WriteCmosSensor(0x66, 0x27);//Indoor 0x1327
        HI351WriteCmosSensor(0x67, 0x02);
        HI351WriteCmosSensor(0x68, 0x28);//Indoor 0x1328
        HI351WriteCmosSensor(0x69, 0x03);
        HI351WriteCmosSensor(0x6a, 0x29);//Indoor 0x1329
        HI351WriteCmosSensor(0x6b, 0x03);
        HI351WriteCmosSensor(0x6c, 0x2a);//Indoor 0x132a
        HI351WriteCmosSensor(0x6d, 0x03);
        HI351WriteCmosSensor(0x6e, 0x2b);//Indoor 0x132b
        HI351WriteCmosSensor(0x6f, 0x03);
        HI351WriteCmosSensor(0x70, 0x2c);//Indoor 0x132c
        HI351WriteCmosSensor(0x71, 0x03);
        HI351WriteCmosSensor(0x72, 0x2d);//Indoor 0x132d
        HI351WriteCmosSensor(0x73, 0x03);
        HI351WriteCmosSensor(0x74, 0x2e);//Indoor 0x132e
        HI351WriteCmosSensor(0x75, 0x03);
        HI351WriteCmosSensor(0x76, 0x2f);//Indoor 0x132f
        HI351WriteCmosSensor(0x77, 0x03);
        HI351WriteCmosSensor(0x78, 0x30);//Indoor 0x1330
        HI351WriteCmosSensor(0x79, 0x03);
        HI351WriteCmosSensor(0x7a, 0x31);//Indoor 0x1331
        HI351WriteCmosSensor(0x7b, 0x03);
        HI351WriteCmosSensor(0x7c, 0x32);//Indoor 0x1332
        HI351WriteCmosSensor(0x7d, 0x03);
        HI351WriteCmosSensor(0x7e, 0x33);//Indoor 0x1333
        HI351WriteCmosSensor(0x7f, 0x40);
        HI351WriteCmosSensor(0x80, 0x34);//Indoor 0x1334
        HI351WriteCmosSensor(0x81, 0x80);
        HI351WriteCmosSensor(0x82, 0x35);//Indoor 0x1335
        HI351WriteCmosSensor(0x83, 0x00);
        HI351WriteCmosSensor(0x84, 0x36);//Indoor 0x1336
        HI351WriteCmosSensor(0x85, 0xf0);
        HI351WriteCmosSensor(0x86, 0xa0);//Indoor 0x13a0
        HI351WriteCmosSensor(0x87, 0x0f);
        HI351WriteCmosSensor(0x88, 0xa8);//Indoor 0x13a8
        HI351WriteCmosSensor(0x89, 0x10);
        HI351WriteCmosSensor(0x8a, 0xa9);//Indoor 0x13a9
        HI351WriteCmosSensor(0x8b, 0x16);
        HI351WriteCmosSensor(0x8c, 0xaa);//Indoor 0x13aa
        HI351WriteCmosSensor(0x8d, 0x0a);
        HI351WriteCmosSensor(0x8e, 0xab);//Indoor 0x13ab
        HI351WriteCmosSensor(0x8f, 0x02);
        HI351WriteCmosSensor(0x90, 0xc0);//Indoor 0x13c0
        HI351WriteCmosSensor(0x91, 0x27);
        HI351WriteCmosSensor(0x92, 0xc2);//Indoor 0x13c2
        HI351WriteCmosSensor(0x93, 0x08);
        HI351WriteCmosSensor(0x94, 0xc3);//Indoor 0x13c3
        HI351WriteCmosSensor(0x95, 0x08);
        HI351WriteCmosSensor(0x96, 0xc4);//Indoor 0x13c4
        HI351WriteCmosSensor(0x97, 0x40);
        HI351WriteCmosSensor(0x98, 0xc5);//Indoor 0x13c5
        HI351WriteCmosSensor(0x99, 0x38);
        HI351WriteCmosSensor(0x9a, 0xc6);//Indoor 0x13c6
        HI351WriteCmosSensor(0x9b, 0xf0);
        HI351WriteCmosSensor(0x9c, 0xc7);//Indoor 0x13c7
        HI351WriteCmosSensor(0x9d, 0x10);
        HI351WriteCmosSensor(0x9e, 0xc8);//Indoor 0x13c8
        HI351WriteCmosSensor(0x9f, 0x44);
        HI351WriteCmosSensor(0xa0, 0xc9);//Indoor 0x13c9
        HI351WriteCmosSensor(0xa1, 0x87);
        HI351WriteCmosSensor(0xa2, 0xca);//Indoor 0x13ca
        HI351WriteCmosSensor(0xa3, 0xff);
        HI351WriteCmosSensor(0xa4, 0xcb);//Indoor 0x13cb
        HI351WriteCmosSensor(0xa5, 0x20);
        HI351WriteCmosSensor(0xa6, 0xcc);//Indoor 0x13cc
        HI351WriteCmosSensor(0xa7, 0x61);
        HI351WriteCmosSensor(0xa8, 0xcd);//Indoor 0x13cd
        HI351WriteCmosSensor(0xa9, 0x87);
        HI351WriteCmosSensor(0xaa, 0xce);//Indoor 0x13ce
        HI351WriteCmosSensor(0xab, 0x8a);//07
        HI351WriteCmosSensor(0xac, 0xcf);//Indoor 0x13cf
        HI351WriteCmosSensor(0xad, 0xa5);//07
        HI351WriteCmosSensor(0xae, 0x03);//Indoor Page14
        HI351WriteCmosSensor(0xaf, 0x14);
        HI351WriteCmosSensor(0xb0, 0x10);//Indoor 0x1410
        HI351WriteCmosSensor(0xb1, 0x27);
        HI351WriteCmosSensor(0xb2, 0x11);//Indoor 0x1411
        HI351WriteCmosSensor(0xb3, 0x02);
        HI351WriteCmosSensor(0xb4, 0x12);//Indoor 0x1412
        HI351WriteCmosSensor(0xb5, 0x40);
        HI351WriteCmosSensor(0xb6, 0x13);//Indoor 0x1413
        HI351WriteCmosSensor(0xb7, 0x98);
        HI351WriteCmosSensor(0xb8, 0x14);//Indoor 0x1414
        HI351WriteCmosSensor(0xb9, 0x3a);
        HI351WriteCmosSensor(0xba, 0x15);//Indoor 0x1415
        HI351WriteCmosSensor(0xbb, 0x24);
        HI351WriteCmosSensor(0xbc, 0x16);//Indoor 0x1416
        HI351WriteCmosSensor(0xbd, 0x1a);
        HI351WriteCmosSensor(0xbe, 0x17);//Indoor 0x1417
        HI351WriteCmosSensor(0xbf, 0x1a);
        HI351WriteCmosSensor(0xc0, 0x18);//Indoor 0x1418    Negative High Gain
        HI351WriteCmosSensor(0xc1, 0x60);//3a
        HI351WriteCmosSensor(0xc2, 0x19);//Indoor 0x1419    Negative Middle Gain
        HI351WriteCmosSensor(0xc3, 0x68);//3a
        HI351WriteCmosSensor(0xc4, 0x1a);//Indoor 0x141a    Negative Low Gain
        HI351WriteCmosSensor(0xc5, 0x68); //
        HI351WriteCmosSensor(0xc6, 0x20);//Indoor 0x1420
        HI351WriteCmosSensor(0xc7, 0x82);  // s_diff L_clip
        HI351WriteCmosSensor(0xc8, 0x21);//Indoor 0x1421
        HI351WriteCmosSensor(0xc9, 0x03);
        HI351WriteCmosSensor(0xca, 0x22);//Indoor 0x1422
        HI351WriteCmosSensor(0xcb, 0x05);
        HI351WriteCmosSensor(0xcc, 0x23);//Indoor 0x1423
        HI351WriteCmosSensor(0xcd, 0x07);
        HI351WriteCmosSensor(0xce, 0x24);//Indoor 0x1424
        HI351WriteCmosSensor(0xcf, 0x0a);
        HI351WriteCmosSensor(0xd0, 0x25);//Indoor 0x1425
        HI351WriteCmosSensor(0xd1, 0x46); //19
        HI351WriteCmosSensor(0xd2, 0x26);//Indoor 0x1426
        HI351WriteCmosSensor(0xd3, 0x32);
        HI351WriteCmosSensor(0xd4, 0x27);//Indoor 0x1427
        HI351WriteCmosSensor(0xd5, 0x1e);
        HI351WriteCmosSensor(0xd6, 0x28);//Indoor 0x1428
        HI351WriteCmosSensor(0xd7, 0x10);
        HI351WriteCmosSensor(0xd8, 0x29);//Indoor 0x1429
        HI351WriteCmosSensor(0xd9, 0x00);
        HI351WriteCmosSensor(0xda, 0x2a);//Indoor 0x142a
        HI351WriteCmosSensor(0xdb, 0x18);//40
        HI351WriteCmosSensor(0xdc, 0x2b);//Indoor 0x142b
        HI351WriteCmosSensor(0xdd, 0x18);
        HI351WriteCmosSensor(0xde, 0x2c);//Indoor 0x142c
        HI351WriteCmosSensor(0xdf, 0x18);
        HI351WriteCmosSensor(0xe0, 0x2d);//Indoor 0x142d
        HI351WriteCmosSensor(0xe1, 0x30);
        HI351WriteCmosSensor(0xe2, 0x2e);//Indoor 0x142e
        HI351WriteCmosSensor(0xe3, 0x30);
        HI351WriteCmosSensor(0xe4, 0x2f);//Indoor 0x142f
        HI351WriteCmosSensor(0xe5, 0x30);
        HI351WriteCmosSensor(0xe6, 0x30);//Indoor 0x1430
        HI351WriteCmosSensor(0xe7, 0x82);   //Ldiff_L_cip
        HI351WriteCmosSensor(0xe8, 0x31);//Indoor 0x1431
        HI351WriteCmosSensor(0xe9, 0x02);
        HI351WriteCmosSensor(0xea, 0x32);//Indoor 0x1432
        HI351WriteCmosSensor(0xeb, 0x04);
        HI351WriteCmosSensor(0xec, 0x33);//Indoor 0x1433
        HI351WriteCmosSensor(0xed, 0x04);
        HI351WriteCmosSensor(0xee, 0x34);//Indoor 0x1434
        HI351WriteCmosSensor(0xef, 0x0a);
        HI351WriteCmosSensor(0xf0, 0x35);//Indoor 0x1435
        HI351WriteCmosSensor(0xf1, 0x46);//12
        HI351WriteCmosSensor(0xf2, 0x36);//Indoor 0x1436
        HI351WriteCmosSensor(0xf3, 0x32);
        HI351WriteCmosSensor(0xf4, 0x37);//Indoor 0x1437
        HI351WriteCmosSensor(0xf5, 0x32);
        HI351WriteCmosSensor(0xf6, 0x38);//Indoor 0x1438
        HI351WriteCmosSensor(0xf7, 0x22);
        HI351WriteCmosSensor(0xf8, 0x39);//Indoor 0x1439
        HI351WriteCmosSensor(0xf9, 0x00);
        HI351WriteCmosSensor(0xfa, 0x3a);//Indoor 0x143a
        HI351WriteCmosSensor(0xfb, 0x48);
        HI351WriteCmosSensor(0xfc, 0x3b);//Indoor 0x143b
        HI351WriteCmosSensor(0xfd, 0x30);
            
        HI351WriteCmosSensor(0x03, 0xdf);
        HI351WriteCmosSensor(0x10, 0x3c);//Indoor 0x143c
        HI351WriteCmosSensor(0x11, 0x30);
        HI351WriteCmosSensor(0x12, 0x3d);//Indoor 0x143d
        HI351WriteCmosSensor(0x13, 0x20);
        HI351WriteCmosSensor(0x14, 0x3e);//Indoor 0x143e
        HI351WriteCmosSensor(0x15, 0x22);//12
        HI351WriteCmosSensor(0x16, 0x3f);//Indoor 0x143f
        HI351WriteCmosSensor(0x17, 0x10);
        HI351WriteCmosSensor(0x18, 0x40);//Indoor 0x1440
        HI351WriteCmosSensor(0x19, 0x84);
        HI351WriteCmosSensor(0x1a, 0x41);//Indoor 0x1441
        HI351WriteCmosSensor(0x1b, 0x10);//20
        HI351WriteCmosSensor(0x1c, 0x42);//Indoor 0x1442
        HI351WriteCmosSensor(0x1d, 0xb0);//20
        HI351WriteCmosSensor(0x1e, 0x43);//Indoor 0x1443
        HI351WriteCmosSensor(0x1f, 0x40);//20
        HI351WriteCmosSensor(0x20, 0x44);//Indoor 0x1444
        HI351WriteCmosSensor(0x21, 0x14);
        HI351WriteCmosSensor(0x22, 0x45);//Indoor 0x1445
        HI351WriteCmosSensor(0x23, 0x10);
        HI351WriteCmosSensor(0x24, 0x46);//Indoor 0x1446
        HI351WriteCmosSensor(0x25, 0x14);
        HI351WriteCmosSensor(0x26, 0x47);//Indoor 0x1447
        HI351WriteCmosSensor(0x27, 0x04);
        HI351WriteCmosSensor(0x28, 0x48);//Indoor 0x1448
        HI351WriteCmosSensor(0x29, 0x04);
        HI351WriteCmosSensor(0x2a, 0x49);//Indoor 0x1449
        HI351WriteCmosSensor(0x2b, 0x04);
        HI351WriteCmosSensor(0x2c, 0x50);//Indoor 0x1450
        HI351WriteCmosSensor(0x2d, 0x84);//19
        HI351WriteCmosSensor(0x2e, 0x51);//Indoor 0x1451
        HI351WriteCmosSensor(0x2f, 0x30);//60
        HI351WriteCmosSensor(0x30, 0x52);//Indoor 0x1452
        HI351WriteCmosSensor(0x31, 0xb0);
        HI351WriteCmosSensor(0x32, 0x53);//Indoor 0x1453
        HI351WriteCmosSensor(0x33, 0x37);//58
        HI351WriteCmosSensor(0x34, 0x54);//Indoor 0x1454
        HI351WriteCmosSensor(0x35, 0x44);
        HI351WriteCmosSensor(0x36, 0x55);//Indoor 0x1455
        HI351WriteCmosSensor(0x37, 0x44);
        HI351WriteCmosSensor(0x38, 0x56);//Indoor 0x1456
        HI351WriteCmosSensor(0x39, 0x44);
        HI351WriteCmosSensor(0x3a, 0x57);//Indoor 0x1457
        HI351WriteCmosSensor(0x3b, 0x10);//03
        HI351WriteCmosSensor(0x3c, 0x58);//Indoor 0x1458
        HI351WriteCmosSensor(0x3d, 0x14);
        HI351WriteCmosSensor(0x3e, 0x59);//Indoor 0x1459
        HI351WriteCmosSensor(0x3f, 0x14);
        HI351WriteCmosSensor(0x40, 0x60);//Indoor 0x1460
        HI351WriteCmosSensor(0x41, 0x02);
        HI351WriteCmosSensor(0x42, 0x61);//Indoor 0x1461
        HI351WriteCmosSensor(0x43, 0xa0);
        HI351WriteCmosSensor(0x44, 0x62);//Indoor 0x1462
        HI351WriteCmosSensor(0x45, 0x98);
        HI351WriteCmosSensor(0x46, 0x63);//Indoor 0x1463
        HI351WriteCmosSensor(0x47, 0xe4);
        HI351WriteCmosSensor(0x48, 0x64);//Indoor 0x1464
        HI351WriteCmosSensor(0x49, 0xa4);
        HI351WriteCmosSensor(0x4a, 0x65);//Indoor 0x1465
        HI351WriteCmosSensor(0x4b, 0x7d);
        HI351WriteCmosSensor(0x4c, 0x66);//Indoor 0x1466
        HI351WriteCmosSensor(0x4d, 0x4b);
        HI351WriteCmosSensor(0x4e, 0x70);//Indoor 0x1470
        HI351WriteCmosSensor(0x4f, 0x10);
        HI351WriteCmosSensor(0x50, 0x71);//Indoor 0x1471
        HI351WriteCmosSensor(0x51, 0x10);
        HI351WriteCmosSensor(0x52, 0x72);//Indoor 0x1472
        HI351WriteCmosSensor(0x53, 0x10);
        HI351WriteCmosSensor(0x54, 0x73);//Indoor 0x1473
        HI351WriteCmosSensor(0x55, 0x10);
        HI351WriteCmosSensor(0x56, 0x74);//Indoor 0x1474
        HI351WriteCmosSensor(0x57, 0x10);
        HI351WriteCmosSensor(0x58, 0x75);//Indoor 0x1475
        HI351WriteCmosSensor(0x59, 0x10);
        HI351WriteCmosSensor(0x5a, 0x76);//Indoor 0x1476      //green sharp pos High          
        HI351WriteCmosSensor(0x5b, 0x10);                                                  
        HI351WriteCmosSensor(0x5c, 0x77);//Indoor 0x1477      //green sharp pos Middle        
        HI351WriteCmosSensor(0x5d, 0x20);                                                  
        HI351WriteCmosSensor(0x5e, 0x78);//Indoor 0x1478      //green sharp pos Low           
        HI351WriteCmosSensor(0x5f, 0x18);                                                  
        HI351WriteCmosSensor(0x60, 0x79);//Indoor 0x1479       //green sharp nega High        
        HI351WriteCmosSensor(0x61, 0x60);                                                  
        HI351WriteCmosSensor(0x62, 0x7a);//Indoor 0x147a       //green sharp nega Middle      
        HI351WriteCmosSensor(0x63, 0x60);                                                  
        HI351WriteCmosSensor(0x64, 0x7b);//Indoor 0x147b       //green sharp nega Low         
        HI351WriteCmosSensor(0x65, 0x60);
        
            
            
            //////////////////
            // e0 Page (DMA Dark1)
            //////////////////
            
            //Page 0xe0
        HI351WriteCmosSensor(0x03, 0xe0);
        HI351WriteCmosSensor(0x10, 0x03);
        HI351WriteCmosSensor(0x11, 0x11); //11 page
        HI351WriteCmosSensor(0x12, 0x10); //Dark1 0x1110
        HI351WriteCmosSensor(0x13, 0x1f); 
        HI351WriteCmosSensor(0x14, 0x11); //Dark1 0x1111
        HI351WriteCmosSensor(0x15, 0x3f);
        HI351WriteCmosSensor(0x16, 0x12); //Dark1 0x1112
        HI351WriteCmosSensor(0x17, 0x32);
        HI351WriteCmosSensor(0x18, 0x13); //Dark1 0x1113
        HI351WriteCmosSensor(0x19, 0x21);
        HI351WriteCmosSensor(0x1a, 0x14); //Dark1 0x1114
        HI351WriteCmosSensor(0x1b, 0x3a);
        HI351WriteCmosSensor(0x1c, 0x30); //Dark1 0x1130
        HI351WriteCmosSensor(0x1d, 0x24); //20
        HI351WriteCmosSensor(0x1e, 0x31); //Dark1 0x1131
        HI351WriteCmosSensor(0x1f, 0x24); //20
            
        HI351WriteCmosSensor(0x20, 0x32); //Dark1 0x1132 //STEVE Lum. Level. in DLPF
        HI351WriteCmosSensor(0x21, 0x8b); //52                                      
        HI351WriteCmosSensor(0x22, 0x33); //Dark1 0x1133                            
        HI351WriteCmosSensor(0x23, 0x54); //3b                                      
        HI351WriteCmosSensor(0x24, 0x34); //Dark1 0x1134                            
        HI351WriteCmosSensor(0x25, 0x2c); //1d                                      
        HI351WriteCmosSensor(0x26, 0x35); //Dark1 0x1135                            
        HI351WriteCmosSensor(0x27, 0x29); //21                                      
        HI351WriteCmosSensor(0x28, 0x36); //Dark1 0x1136                            
        HI351WriteCmosSensor(0x29, 0x18); //1b                                      
        HI351WriteCmosSensor(0x2a, 0x37); //Dark1 0x1137                            
        HI351WriteCmosSensor(0x2b, 0x1e); //21                                      
        HI351WriteCmosSensor(0x2c, 0x38); //Dark1 0x1138                            
        HI351WriteCmosSensor(0x2d, 0x17); //18                                      
                                                    
        HI351WriteCmosSensor(0x2e, 0x39); //Dark1 0x1139
        HI351WriteCmosSensor(0x2f, 0x84);
        HI351WriteCmosSensor(0x30, 0x3a); //Dark1 0x113a
        HI351WriteCmosSensor(0x31, 0x84);
        HI351WriteCmosSensor(0x32, 0x3b); //Dark1 0x113b
        HI351WriteCmosSensor(0x33, 0x84);
        HI351WriteCmosSensor(0x34, 0x3c); //Dark1 0x113c
        HI351WriteCmosSensor(0x35, 0x84);
        HI351WriteCmosSensor(0x36, 0x3d); //Dark1 0x113d
        HI351WriteCmosSensor(0x37, 0x84);
        HI351WriteCmosSensor(0x38, 0x3e); //Dark1 0x113e
        HI351WriteCmosSensor(0x39, 0x84);
        HI351WriteCmosSensor(0x3a, 0x3f); //Dark1 0x113f
        HI351WriteCmosSensor(0x3b, 0x84);
        HI351WriteCmosSensor(0x3c, 0x40); //Dark1 0x1140
        HI351WriteCmosSensor(0x3d, 0x84);
        HI351WriteCmosSensor(0x3e, 0x41); //Dark1 0x1141
        HI351WriteCmosSensor(0x3f, 0x3a);
        HI351WriteCmosSensor(0x40, 0x42); //Dark1 0x1142
        HI351WriteCmosSensor(0x41, 0x3a);
        HI351WriteCmosSensor(0x42, 0x43); //Dark1 0x1143
        HI351WriteCmosSensor(0x43, 0x3a);
        HI351WriteCmosSensor(0x44, 0x44); //Dark1 0x1144
        HI351WriteCmosSensor(0x45, 0x3a);
        HI351WriteCmosSensor(0x46, 0x45); //Dark1 0x1145
        HI351WriteCmosSensor(0x47, 0x3a);
        HI351WriteCmosSensor(0x48, 0x46); //Dark1 0x1146
        HI351WriteCmosSensor(0x49, 0x3a);
        HI351WriteCmosSensor(0x4a, 0x47); //Dark1 0x1147
        HI351WriteCmosSensor(0x4b, 0x3a);
        HI351WriteCmosSensor(0x4c, 0x48); //Dark1 0x1148
        HI351WriteCmosSensor(0x4d, 0x3a);
        HI351WriteCmosSensor(0x4e, 0x49); //Dark1 0x1149
        HI351WriteCmosSensor(0x4f, 0x80);
        HI351WriteCmosSensor(0x50, 0x4a); //Dark1 0x114a
        HI351WriteCmosSensor(0x51, 0x80);
        HI351WriteCmosSensor(0x52, 0x4b); //Dark1 0x114b
        HI351WriteCmosSensor(0x53, 0x80);
        HI351WriteCmosSensor(0x54, 0x4c); //Dark1 0x114c
        HI351WriteCmosSensor(0x55, 0x80);
        HI351WriteCmosSensor(0x56, 0x4d); //Dark1 0x114d
        HI351WriteCmosSensor(0x57, 0x80);
        HI351WriteCmosSensor(0x58, 0x4e); //Dark1 0x114e
        HI351WriteCmosSensor(0x59, 0x80);
        HI351WriteCmosSensor(0x5a, 0x4f); //Dark1 0x114f
        HI351WriteCmosSensor(0x5b, 0x80);
        HI351WriteCmosSensor(0x5c, 0x50); //Dark1 0x1150
        HI351WriteCmosSensor(0x5d, 0x80);
        HI351WriteCmosSensor(0x5e, 0x51); //Dark1 0x1151
        HI351WriteCmosSensor(0x5f, 0xd8);
        HI351WriteCmosSensor(0x60, 0x52); //Dark1 0x1152
        HI351WriteCmosSensor(0x61, 0xd8);
        HI351WriteCmosSensor(0x62, 0x53); //Dark1 0x1153
        HI351WriteCmosSensor(0x63, 0xd8);
        HI351WriteCmosSensor(0x64, 0x54); //Dark1 0x1154
        HI351WriteCmosSensor(0x65, 0xd0);
        HI351WriteCmosSensor(0x66, 0x55); //Dark1 0x1155
        HI351WriteCmosSensor(0x67, 0xd0);
        HI351WriteCmosSensor(0x68, 0x56); //Dark1 0x1156
        HI351WriteCmosSensor(0x69, 0xc8);
        HI351WriteCmosSensor(0x6a, 0x57); //Dark1 0x1157
        HI351WriteCmosSensor(0x6b, 0xc0);
        HI351WriteCmosSensor(0x6c, 0x58); //Dark1 0x1158
        HI351WriteCmosSensor(0x6d, 0xc0);
        HI351WriteCmosSensor(0x6e, 0x59); //Dark1 0x1159
        HI351WriteCmosSensor(0x6f, 0xf0);
        HI351WriteCmosSensor(0x70, 0x5a); //Dark1 0x115a
        HI351WriteCmosSensor(0x71, 0xf0);
        HI351WriteCmosSensor(0x72, 0x5b); //Dark1 0x115b
        HI351WriteCmosSensor(0x73, 0xf0);
        HI351WriteCmosSensor(0x74, 0x5c); //Dark1 0x115c
        HI351WriteCmosSensor(0x75, 0xe8);
        HI351WriteCmosSensor(0x76, 0x5d); //Dark1 0x115d
        HI351WriteCmosSensor(0x77, 0xe8);
        HI351WriteCmosSensor(0x78, 0x5e); //Dark1 0x115e
        HI351WriteCmosSensor(0x79, 0xe0);
        HI351WriteCmosSensor(0x7a, 0x5f); //Dark1 0x115f
        HI351WriteCmosSensor(0x7b, 0xe0);
        HI351WriteCmosSensor(0x7c, 0x60); //Dark1 0x1160
        HI351WriteCmosSensor(0x7d, 0xe0);
        HI351WriteCmosSensor(0x7e, 0x61); //Dark1 0x1161
        HI351WriteCmosSensor(0x7f, 0xf0);
        HI351WriteCmosSensor(0x80, 0x62); //Dark1 0x1162
        HI351WriteCmosSensor(0x81, 0xf0);
        HI351WriteCmosSensor(0x82, 0x63); //Dark1 0x1163
        HI351WriteCmosSensor(0x83, 0x80);
        HI351WriteCmosSensor(0x84, 0x64); //Dark1 0x1164
        HI351WriteCmosSensor(0x85, 0x40);
        HI351WriteCmosSensor(0x86, 0x65); //Dark1 0x1165
        HI351WriteCmosSensor(0x87, 0x08);
        HI351WriteCmosSensor(0x88, 0x66); //Dark1 0x1166
        HI351WriteCmosSensor(0x89, 0x08);
        HI351WriteCmosSensor(0x8a, 0x67); //Dark1 0x1167
        HI351WriteCmosSensor(0x8b, 0x08);
        HI351WriteCmosSensor(0x8c, 0x68); //Dark1 0x1168
        HI351WriteCmosSensor(0x8d, 0x80);
        HI351WriteCmosSensor(0x8e, 0x69); //Dark1 0x1169
        HI351WriteCmosSensor(0x8f, 0x40);
        HI351WriteCmosSensor(0x90, 0x6a); //Dark1 0x116a
        HI351WriteCmosSensor(0x91, 0x08);
        HI351WriteCmosSensor(0x92, 0x6b); //Dark1 0x116b
        HI351WriteCmosSensor(0x93, 0x08);
        HI351WriteCmosSensor(0x94, 0x6c); //Dark1 0x116c
        HI351WriteCmosSensor(0x95, 0x08);
        HI351WriteCmosSensor(0x96, 0x6d); //Dark1 0x116d
        HI351WriteCmosSensor(0x97, 0x80);
        HI351WriteCmosSensor(0x98, 0x6e); //Dark1 0x116e
        HI351WriteCmosSensor(0x99, 0x40);
        HI351WriteCmosSensor(0x9a, 0x6f); //Dark1 0x116f
        HI351WriteCmosSensor(0x9b, 0x02);
        HI351WriteCmosSensor(0x9c, 0x70); //Dark1 0x1170
        HI351WriteCmosSensor(0x9d, 0x02);
        HI351WriteCmosSensor(0x9e, 0x71); //Dark1 0x1171
        HI351WriteCmosSensor(0x9f, 0x02);
        HI351WriteCmosSensor(0xa0, 0x72); //Dark1 0x1172
        HI351WriteCmosSensor(0xa1, 0x6e);
        HI351WriteCmosSensor(0xa2, 0x73); //Dark1 0x1173
        HI351WriteCmosSensor(0xa3, 0x3a);
        HI351WriteCmosSensor(0xa4, 0x74); //Dark1 0x1174
        HI351WriteCmosSensor(0xa5, 0x02);
        HI351WriteCmosSensor(0xa6, 0x75); //Dark1 0x1175
        HI351WriteCmosSensor(0xa7, 0x02);
        HI351WriteCmosSensor(0xa8, 0x76); //Dark1 0x1176
        HI351WriteCmosSensor(0xa9, 0x02);
        HI351WriteCmosSensor(0xaa, 0x77); //Dark1 0x1177
        HI351WriteCmosSensor(0xab, 0x6e);
        HI351WriteCmosSensor(0xac, 0x78); //Dark1 0x1178
        HI351WriteCmosSensor(0xad, 0x3a);
        HI351WriteCmosSensor(0xae, 0x79); //Dark1 0x1179
        HI351WriteCmosSensor(0xaf, 0x02);
        HI351WriteCmosSensor(0xb0, 0x7a); //Dark1 0x117a
        HI351WriteCmosSensor(0xb1, 0x02);
        HI351WriteCmosSensor(0xb2, 0x7b); //Dark1 0x117b
        HI351WriteCmosSensor(0xb3, 0x02);
        HI351WriteCmosSensor(0xb4, 0x7c); //Dark1 0x117c
        HI351WriteCmosSensor(0xb5, 0x5c);
        HI351WriteCmosSensor(0xb6, 0x7d); //Dark1 0x117d
        HI351WriteCmosSensor(0xb7, 0x30);
        HI351WriteCmosSensor(0xb8, 0x7e); //Dark1 0x117e
        HI351WriteCmosSensor(0xb9, 0x02);
        HI351WriteCmosSensor(0xba, 0x7f); //Dark1 0x117f
        HI351WriteCmosSensor(0xbb, 0x02);
        HI351WriteCmosSensor(0xbc, 0x80); //Dark1 0x1180
        HI351WriteCmosSensor(0xbd, 0x02);
        HI351WriteCmosSensor(0xbe, 0x81); //Dark1 0x1181
        HI351WriteCmosSensor(0xbf, 0x62);
        HI351WriteCmosSensor(0xc0, 0x82); //Dark1 0x1182
        HI351WriteCmosSensor(0xc1, 0x26);
        HI351WriteCmosSensor(0xc2, 0x83); //Dark1 0x1183
        HI351WriteCmosSensor(0xc3, 0x02);
        HI351WriteCmosSensor(0xc4, 0x84); //Dark1 0x1184
        HI351WriteCmosSensor(0xc5, 0x02);
        HI351WriteCmosSensor(0xc6, 0x85); //Dark1 0x1185
        HI351WriteCmosSensor(0xc7, 0x02);
        HI351WriteCmosSensor(0xc8, 0x86); //Dark1 0x1186
        HI351WriteCmosSensor(0xc9, 0x62);
        HI351WriteCmosSensor(0xca, 0x87); //Dark1 0x1187
        HI351WriteCmosSensor(0xcb, 0x26);
        HI351WriteCmosSensor(0xcc, 0x88); //Dark1 0x1188
        HI351WriteCmosSensor(0xcd, 0x02);
        HI351WriteCmosSensor(0xce, 0x89); //Dark1 0x1189
        HI351WriteCmosSensor(0xcf, 0x02);
        HI351WriteCmosSensor(0xd0, 0x8a); //Dark1 0x118a
        HI351WriteCmosSensor(0xd1, 0x02);
        HI351WriteCmosSensor(0xd2, 0x90); //Dark1 0x1190
        HI351WriteCmosSensor(0xd3, 0x03);
        HI351WriteCmosSensor(0xd4, 0x91); //Dark1 0x1191
        HI351WriteCmosSensor(0xd5, 0xff);
        HI351WriteCmosSensor(0xd6, 0x92); //Dark1 0x1192
        HI351WriteCmosSensor(0xd7, 0x0a);
        HI351WriteCmosSensor(0xd8, 0x93); //Dark1 0x1193
        HI351WriteCmosSensor(0xd9, 0x80);
        HI351WriteCmosSensor(0xda, 0x94); //Dark1 0x1194
        HI351WriteCmosSensor(0xdb, 0x03);
        HI351WriteCmosSensor(0xdc, 0x95); //Dark1 0x1195
        HI351WriteCmosSensor(0xdd, 0x64);
        HI351WriteCmosSensor(0xde, 0x96); //Dark1 0x1196
        HI351WriteCmosSensor(0xdf, 0x90);
        HI351WriteCmosSensor(0xe0, 0x97); //Dark1 0x1197
        HI351WriteCmosSensor(0xe1, 0xa0);
        HI351WriteCmosSensor(0xe2, 0xb0); //Dark1 0x11b0
        HI351WriteCmosSensor(0xe3, 0x64);
        HI351WriteCmosSensor(0xe4, 0xb1); //Dark1 0x11b1
        HI351WriteCmosSensor(0xe5, 0xd8);
        HI351WriteCmosSensor(0xe6, 0xb2); //Dark1 0x11b2
        HI351WriteCmosSensor(0xe7, 0x50);
        HI351WriteCmosSensor(0xe8, 0xb3); //Dark1 0x11b3
        HI351WriteCmosSensor(0xe9, 0x10);
        HI351WriteCmosSensor(0xea, 0xb4); //Dark1 0x11b4
        HI351WriteCmosSensor(0xeb, 0x03);
                       
        HI351WriteCmosSensor(0xec, 0x03);
        HI351WriteCmosSensor(0xed, 0x12);//12 page
        HI351WriteCmosSensor(0xee, 0x10); //Dark1 0x1210
        HI351WriteCmosSensor(0xef, 0x03);
        HI351WriteCmosSensor(0xf0, 0x11); //Dark1 0x1211
        HI351WriteCmosSensor(0xf1, 0x29);
        HI351WriteCmosSensor(0xf2, 0x12); //Dark1 0x1212
        HI351WriteCmosSensor(0xf3, 0x08);
        HI351WriteCmosSensor(0xf4, 0x40); //Dark1 0x1240
        HI351WriteCmosSensor(0xf5, 0x33); //07
        HI351WriteCmosSensor(0xf6, 0x41); //Dark1 0x1241
        HI351WriteCmosSensor(0xf7, 0x0a); //32
        HI351WriteCmosSensor(0xf8, 0x42); //Dark1 0x1242
        HI351WriteCmosSensor(0xf9, 0x6a); //8c
        HI351WriteCmosSensor(0xfa, 0x43); //Dark1 0x1243
        HI351WriteCmosSensor(0xfb, 0x80);
        HI351WriteCmosSensor(0xfc, 0x44); //Dark1 0x1244
        HI351WriteCmosSensor(0xfd, 0x02);
            
        HI351WriteCmosSensor(0x03, 0xe1);
        HI351WriteCmosSensor(0x10, 0x45); //Dark1 0x1245
        HI351WriteCmosSensor(0x11, 0x0a);
        HI351WriteCmosSensor(0x12, 0x46); //Dark1 0x1246
        HI351WriteCmosSensor(0x13, 0x80);
        HI351WriteCmosSensor(0x14, 0x60); //Dark1 0x1260
        HI351WriteCmosSensor(0x15, 0x02);
        HI351WriteCmosSensor(0x16, 0x61); //Dark1 0x1261
        HI351WriteCmosSensor(0x17, 0x04);
        HI351WriteCmosSensor(0x18, 0x62); //Dark1 0x1262
        HI351WriteCmosSensor(0x19, 0x4b);
        HI351WriteCmosSensor(0x1a, 0x63); //Dark1 0x1263
        HI351WriteCmosSensor(0x1b, 0x41);
        HI351WriteCmosSensor(0x1c, 0x64); //Dark1 0x1264
        HI351WriteCmosSensor(0x1d, 0x14);
        HI351WriteCmosSensor(0x1e, 0x65); //Dark1 0x1265
        HI351WriteCmosSensor(0x1f, 0x00);
        HI351WriteCmosSensor(0x20, 0x68); //Dark1 0x1268
        HI351WriteCmosSensor(0x21, 0x0a);
        HI351WriteCmosSensor(0x22, 0x69); //Dark1 0x1269
        HI351WriteCmosSensor(0x23, 0x04);
        HI351WriteCmosSensor(0x24, 0x6a); //Dark1 0x126a
        HI351WriteCmosSensor(0x25, 0x0a);
        HI351WriteCmosSensor(0x26, 0x6b); //Dark1 0x126b
        HI351WriteCmosSensor(0x27, 0x0a);
        HI351WriteCmosSensor(0x28, 0x6c); //Dark1 0x126c
        HI351WriteCmosSensor(0x29, 0x24);
        HI351WriteCmosSensor(0x2a, 0x6d); //Dark1 0x126d
        HI351WriteCmosSensor(0x2b, 0x01);
        HI351WriteCmosSensor(0x2c, 0x70); //Dark1 0x1270
        HI351WriteCmosSensor(0x2d, 0x18);
        HI351WriteCmosSensor(0x2e, 0x71); //Dark1 0x1271
        HI351WriteCmosSensor(0x2f, 0xbf);
        HI351WriteCmosSensor(0x30, 0x80); //Dark1 0x1280
        HI351WriteCmosSensor(0x31, 0x64);
        HI351WriteCmosSensor(0x32, 0x81); //Dark1 0x1281
        HI351WriteCmosSensor(0x33, 0xb1);
        HI351WriteCmosSensor(0x34, 0x82); //Dark1 0x1282
        HI351WriteCmosSensor(0x35, 0x2c);
        HI351WriteCmosSensor(0x36, 0x83); //Dark1 0x1283
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x38, 0x84); //Dark1 0x1284
        HI351WriteCmosSensor(0x39, 0x30);
        HI351WriteCmosSensor(0x3a, 0x85); //Dark1 0x1285
        HI351WriteCmosSensor(0x3b, 0x90);
        HI351WriteCmosSensor(0x3c, 0x86); //Dark1 0x1286
        HI351WriteCmosSensor(0x3d, 0x10);
        HI351WriteCmosSensor(0x3e, 0x87); //Dark1 0x1287
        HI351WriteCmosSensor(0x3f, 0x01);
        HI351WriteCmosSensor(0x40, 0x88); //Dark1 0x1288
        HI351WriteCmosSensor(0x41, 0x3a);
        HI351WriteCmosSensor(0x42, 0x89); //Dark1 0x1289
        HI351WriteCmosSensor(0x43, 0x90);
        HI351WriteCmosSensor(0x44, 0x8a); //Dark1 0x128a
        HI351WriteCmosSensor(0x45, 0x0e);
        HI351WriteCmosSensor(0x46, 0x8b); //Dark1 0x128b
        HI351WriteCmosSensor(0x47, 0x0c);
        HI351WriteCmosSensor(0x48, 0x8c); //Dark1 0x128c
        HI351WriteCmosSensor(0x49, 0x05);
        HI351WriteCmosSensor(0x4a, 0x8d); //Dark1 0x128d
        HI351WriteCmosSensor(0x4b, 0x03);
        HI351WriteCmosSensor(0x4c, 0xe6); //Dark1 0x12e6
        HI351WriteCmosSensor(0x4d, 0xff);
        HI351WriteCmosSensor(0x4e, 0xe7); //Dark1 0x12e7
        HI351WriteCmosSensor(0x4f, 0x18);
        HI351WriteCmosSensor(0x50, 0xe8); //Dark1 0x12e8
        HI351WriteCmosSensor(0x51, 0x0a);
        HI351WriteCmosSensor(0x52, 0xe9); //Dark1 0x12e9
        HI351WriteCmosSensor(0x53, 0x04);
        HI351WriteCmosSensor(0x54, 0x03);
        HI351WriteCmosSensor(0x55, 0x13);//13 page
        HI351WriteCmosSensor(0x56, 0x10); //Dark1 0x1310
        HI351WriteCmosSensor(0x57, 0x3f);
        HI351WriteCmosSensor(0x58, 0x20); //Dark1 0x1320
        HI351WriteCmosSensor(0x59, 0x20);
        HI351WriteCmosSensor(0x5a, 0x21); //Dark1 0x1321
        HI351WriteCmosSensor(0x5b, 0x30);
        HI351WriteCmosSensor(0x5c, 0x22); //Dark1 0x1322
        HI351WriteCmosSensor(0x5d, 0x36);
        HI351WriteCmosSensor(0x5e, 0x23); //Dark1 0x1323
        HI351WriteCmosSensor(0x5f, 0x6a);
        HI351WriteCmosSensor(0x60, 0x24); //Dark1 0x1324
        HI351WriteCmosSensor(0x61, 0xa0);
        HI351WriteCmosSensor(0x62, 0x25); //Dark1 0x1325
        HI351WriteCmosSensor(0x63, 0xc0);
        HI351WriteCmosSensor(0x64, 0x26); //Dark1 0x1326
        HI351WriteCmosSensor(0x65, 0xe0);
        HI351WriteCmosSensor(0x66, 0x27); //Dark1 0x1327
        HI351WriteCmosSensor(0x67, 0x04);
        HI351WriteCmosSensor(0x68, 0x28); //Dark1 0x1328
        HI351WriteCmosSensor(0x69, 0x05);
        HI351WriteCmosSensor(0x6a, 0x29); //Dark1 0x1329
        HI351WriteCmosSensor(0x6b, 0x06);
        HI351WriteCmosSensor(0x6c, 0x2a); //Dark1 0x132a
        HI351WriteCmosSensor(0x6d, 0x08);
        HI351WriteCmosSensor(0x6e, 0x2b); //Dark1 0x132b
        HI351WriteCmosSensor(0x6f, 0x0a);
        HI351WriteCmosSensor(0x70, 0x2c); //Dark1 0x132c
        HI351WriteCmosSensor(0x71, 0x0c);
        HI351WriteCmosSensor(0x72, 0x2d); //Dark1 0x132d
        HI351WriteCmosSensor(0x73, 0x12);
        HI351WriteCmosSensor(0x74, 0x2e); //Dark1 0x132e
        HI351WriteCmosSensor(0x75, 0x16);
        HI351WriteCmosSensor(0x76, 0x2f); //Dark1 0x132f       //weight skin
        HI351WriteCmosSensor(0x77, 0x04);
        HI351WriteCmosSensor(0x78, 0x30); //Dark1 0x1330       //weight blue
        HI351WriteCmosSensor(0x79, 0x04);
        HI351WriteCmosSensor(0x7a, 0x31); //Dark1 0x1331       //weight green
        HI351WriteCmosSensor(0x7b, 0x04);
        HI351WriteCmosSensor(0x7c, 0x32); //Dark1 0x1332       //weight strong color
        HI351WriteCmosSensor(0x7d, 0x04);
        HI351WriteCmosSensor(0x7e, 0x33); //Dark1 0x1333
        HI351WriteCmosSensor(0x7f, 0x40);
        HI351WriteCmosSensor(0x80, 0x34); //Dark1 0x1334
        HI351WriteCmosSensor(0x81, 0x80);
        HI351WriteCmosSensor(0x82, 0x35); //Dark1 0x1335
        HI351WriteCmosSensor(0x83, 0x00);
        HI351WriteCmosSensor(0x84, 0x36); //Dark1 0x1336
        HI351WriteCmosSensor(0x85, 0x80);
        HI351WriteCmosSensor(0x86, 0xa0); //Dark1 0x13a0
        HI351WriteCmosSensor(0x87, 0x07);
        HI351WriteCmosSensor(0x88, 0xa8); //Dark1 0x13a8       //Dark1 Cb-filter 0x20
        HI351WriteCmosSensor(0x89, 0x30);
        HI351WriteCmosSensor(0x8a, 0xa9); //Dark1 0x13a9       //Dark1 Cr-filter 0x20
        HI351WriteCmosSensor(0x8b, 0x30);
        HI351WriteCmosSensor(0x8c, 0xaa); //Dark1 0x13aa
        HI351WriteCmosSensor(0x8d, 0x30);
        HI351WriteCmosSensor(0x8e, 0xab); //Dark1 0x13ab
        HI351WriteCmosSensor(0x8f, 0x02);
        HI351WriteCmosSensor(0x90, 0xc0); //Dark1 0x13c0
        HI351WriteCmosSensor(0x91, 0x27);
        HI351WriteCmosSensor(0x92, 0xc2); //Dark1 0x13c2
        HI351WriteCmosSensor(0x93, 0x08);
        HI351WriteCmosSensor(0x94, 0xc3); //Dark1 0x13c3
        HI351WriteCmosSensor(0x95, 0x08);
        HI351WriteCmosSensor(0x96, 0xc4); //Dark1 0x13c4
        HI351WriteCmosSensor(0x97, 0x46);
        HI351WriteCmosSensor(0x98, 0xc5); //Dark1 0x13c5
        HI351WriteCmosSensor(0x99, 0x78);
        HI351WriteCmosSensor(0x9a, 0xc6); //Dark1 0x13c6
        HI351WriteCmosSensor(0x9b, 0xf0);
        HI351WriteCmosSensor(0x9c, 0xc7); //Dark1 0x13c7
        HI351WriteCmosSensor(0x9d, 0x10);
        HI351WriteCmosSensor(0x9e, 0xc8); //Dark1 0x13c8
        HI351WriteCmosSensor(0x9f, 0x44);
        HI351WriteCmosSensor(0xa0, 0xc9); //Dark1 0x13c9
        HI351WriteCmosSensor(0xa1, 0x87);
        HI351WriteCmosSensor(0xa2, 0xca); //Dark1 0x13ca
        HI351WriteCmosSensor(0xa3, 0xff);
        HI351WriteCmosSensor(0xa4, 0xcb); //Dark1 0x13cb
        HI351WriteCmosSensor(0xa5, 0x20);
        HI351WriteCmosSensor(0xa6, 0xcc); //Dark1 0x13cc       //skin range_cb_l
        HI351WriteCmosSensor(0xa7, 0x61);
        HI351WriteCmosSensor(0xa8, 0xcd); //Dark1 0x13cd       //skin range_cb_h
        HI351WriteCmosSensor(0xa9, 0x87);
        HI351WriteCmosSensor(0xaa, 0xce); //Dark1 0x13ce       //skin range_cr_l
        HI351WriteCmosSensor(0xab, 0x8a);
        HI351WriteCmosSensor(0xac, 0xcf); //Dark1 0x13cf       //skin range_cr_h
        HI351WriteCmosSensor(0xad, 0xa5);
        HI351WriteCmosSensor(0xae, 0x03); //14 page
        HI351WriteCmosSensor(0xaf, 0x14);
        HI351WriteCmosSensor(0xb0, 0x10); //Dark1 0x1410
        HI351WriteCmosSensor(0xb1, 0x06);
        HI351WriteCmosSensor(0xb2, 0x11); //Dark1 0x1411
        HI351WriteCmosSensor(0xb3, 0x00);
        HI351WriteCmosSensor(0xb4, 0x12); //Dark1 0x1412
        HI351WriteCmosSensor(0xb5, 0x40); //Top H_Clip
        HI351WriteCmosSensor(0xb6, 0x13); //Dark1 0x1413
        HI351WriteCmosSensor(0xb7, 0xc8);
        HI351WriteCmosSensor(0xb8, 0x14); //Dark1 0x1414
        HI351WriteCmosSensor(0xb9, 0x50);
        HI351WriteCmosSensor(0xba, 0x15); //Dark1 0x1415       //sharp positive hi
        HI351WriteCmosSensor(0xbb, 0x19);
        HI351WriteCmosSensor(0xbc, 0x16); //Dark1 0x1416       //sharp positive mi
        HI351WriteCmosSensor(0xbd, 0x19);
        HI351WriteCmosSensor(0xbe, 0x17); //Dark1 0x1417       //sharp positive low
        HI351WriteCmosSensor(0xbf, 0x19);
        HI351WriteCmosSensor(0xc0, 0x18); //Dark1 0x1418       //sharp negative hi
        HI351WriteCmosSensor(0xc1, 0x33);
        HI351WriteCmosSensor(0xc2, 0x19); //Dark1 0x1419       //sharp negative mi
        HI351WriteCmosSensor(0xc3, 0x33);
        HI351WriteCmosSensor(0xc4, 0x1a); //Dark1 0x141a       //sharp negative low
        HI351WriteCmosSensor(0xc5, 0x33);
        HI351WriteCmosSensor(0xc6, 0x20); //Dark1 0x1420
        HI351WriteCmosSensor(0xc7, 0x80);
        HI351WriteCmosSensor(0xc8, 0x21); //Dark1 0x1421
        HI351WriteCmosSensor(0xc9, 0x03);
        HI351WriteCmosSensor(0xca, 0x22); //Dark1 0x1422
        HI351WriteCmosSensor(0xcb, 0x05);
        HI351WriteCmosSensor(0xcc, 0x23); //Dark1 0x1423
        HI351WriteCmosSensor(0xcd, 0x07);
        HI351WriteCmosSensor(0xce, 0x24); //Dark1 0x1424
        HI351WriteCmosSensor(0xcf, 0x0a);
        HI351WriteCmosSensor(0xd0, 0x25); //Dark1 0x1425
        HI351WriteCmosSensor(0xd1, 0x46);
        HI351WriteCmosSensor(0xd2, 0x26); //Dark1 0x1426
        HI351WriteCmosSensor(0xd3, 0x32);
        HI351WriteCmosSensor(0xd4, 0x27); //Dark1 0x1427
        HI351WriteCmosSensor(0xd5, 0x1e);
        HI351WriteCmosSensor(0xd6, 0x28); //Dark1 0x1428
        HI351WriteCmosSensor(0xd7, 0x19);
        HI351WriteCmosSensor(0xd8, 0x29); //Dark1 0x1429
        HI351WriteCmosSensor(0xd9, 0x00);
        HI351WriteCmosSensor(0xda, 0x2a); //Dark1 0x142a
        HI351WriteCmosSensor(0xdb, 0x10);
        HI351WriteCmosSensor(0xdc, 0x2b); //Dark1 0x142b
        HI351WriteCmosSensor(0xdd, 0x10);
        HI351WriteCmosSensor(0xde, 0x2c); //Dark1 0x142c
        HI351WriteCmosSensor(0xdf, 0x10);
        HI351WriteCmosSensor(0xe0, 0x2d); //Dark1 0x142d
        HI351WriteCmosSensor(0xe1, 0x80);
        HI351WriteCmosSensor(0xe2, 0x2e); //Dark1 0x142e
        HI351WriteCmosSensor(0xe3, 0x80);
        HI351WriteCmosSensor(0xe4, 0x2f); //Dark1 0x142f
        HI351WriteCmosSensor(0xe5, 0x80);
        HI351WriteCmosSensor(0xe6, 0x30); //Dark1 0x1430
        HI351WriteCmosSensor(0xe7, 0x80);
        HI351WriteCmosSensor(0xe8, 0x31); //Dark1 0x1431
        HI351WriteCmosSensor(0xe9, 0x02);
        HI351WriteCmosSensor(0xea, 0x32); //Dark1 0x1432
        HI351WriteCmosSensor(0xeb, 0x04);
        HI351WriteCmosSensor(0xec, 0x33); //Dark1 0x1433
        HI351WriteCmosSensor(0xed, 0x04);
        HI351WriteCmosSensor(0xee, 0x34); //Dark1 0x1434
        HI351WriteCmosSensor(0xef, 0x0a);
        HI351WriteCmosSensor(0xf0, 0x35); //Dark1 0x1435
        HI351WriteCmosSensor(0xf1, 0x46);
        HI351WriteCmosSensor(0xf2, 0x36); //Dark1 0x1436
        HI351WriteCmosSensor(0xf3, 0x32);
        HI351WriteCmosSensor(0xf4, 0x37); //Dark1 0x1437
        HI351WriteCmosSensor(0xf5, 0x28);
        HI351WriteCmosSensor(0xf6, 0x38); //Dark1 0x1438
        HI351WriteCmosSensor(0xf7, 0x12);//2d
        HI351WriteCmosSensor(0xf8, 0x39); //Dark1 0x1439
        HI351WriteCmosSensor(0xf9, 0x00);//23
        HI351WriteCmosSensor(0xfa, 0x3a); //Dark1 0x143a
        HI351WriteCmosSensor(0xfb, 0x18); //dr gain
        HI351WriteCmosSensor(0xfc, 0x3b); //Dark1 0x143b
        HI351WriteCmosSensor(0xfd, 0x20);
            
        HI351WriteCmosSensor(0x03, 0xe2);
        HI351WriteCmosSensor(0x10, 0x3c); //Dark1 0x143c
        HI351WriteCmosSensor(0x11, 0x18);
        HI351WriteCmosSensor(0x12, 0x3d); //Dark1 0x143d
        HI351WriteCmosSensor(0x13, 0x20); //nor gain
        HI351WriteCmosSensor(0x14, 0x3e); //Dark1 0x143e
        HI351WriteCmosSensor(0x15, 0x22);
        HI351WriteCmosSensor(0x16, 0x3f); //Dark1 0x143f
        HI351WriteCmosSensor(0x17, 0x10);
        HI351WriteCmosSensor(0x18, 0x40); //Dark1 0x1440
        HI351WriteCmosSensor(0x19, 0x80);
        HI351WriteCmosSensor(0x1a, 0x41); //Dark1 0x1441
        HI351WriteCmosSensor(0x1b, 0x12);
        HI351WriteCmosSensor(0x1c, 0x42); //Dark1 0x1442
        HI351WriteCmosSensor(0x1d, 0xb0);
        HI351WriteCmosSensor(0x1e, 0x43); //Dark1 0x1443
        HI351WriteCmosSensor(0x1f, 0x20);
        HI351WriteCmosSensor(0x20, 0x44); //Dark1 0x1444
        HI351WriteCmosSensor(0x21, 0x20);
        HI351WriteCmosSensor(0x22, 0x45); //Dark1 0x1445
        HI351WriteCmosSensor(0x23, 0x20);
        HI351WriteCmosSensor(0x24, 0x46); //Dark1 0x1446
        HI351WriteCmosSensor(0x25, 0x20);
        HI351WriteCmosSensor(0x26, 0x47); //Dark1 0x1447
        HI351WriteCmosSensor(0x27, 0x08);
        HI351WriteCmosSensor(0x28, 0x48); //Dark1 0x1448
        HI351WriteCmosSensor(0x29, 0x08);
        HI351WriteCmosSensor(0x2a, 0x49); //Dark1 0x1449
        HI351WriteCmosSensor(0x2b, 0x08);
        HI351WriteCmosSensor(0x2c, 0x50); //Dark1 0x1450
        HI351WriteCmosSensor(0x2d, 0x80);
        HI351WriteCmosSensor(0x2e, 0x51); //Dark1 0x1451
        HI351WriteCmosSensor(0x2f, 0x32); //
        HI351WriteCmosSensor(0x30, 0x52); //Dark1 0x1452
        HI351WriteCmosSensor(0x31, 0x40);
        HI351WriteCmosSensor(0x32, 0x53); //Dark1 0x1453
        HI351WriteCmosSensor(0x33, 0x19);
        HI351WriteCmosSensor(0x34, 0x54); //Dark1 0x1454
        HI351WriteCmosSensor(0x35, 0x60);
        HI351WriteCmosSensor(0x36, 0x55); //Dark1 0x1455
        HI351WriteCmosSensor(0x37, 0x60);
        HI351WriteCmosSensor(0x38, 0x56); //Dark1 0x1456
        HI351WriteCmosSensor(0x39, 0x60);
        HI351WriteCmosSensor(0x3a, 0x57); //Dark1 0x1457
        HI351WriteCmosSensor(0x3b, 0x20);
        HI351WriteCmosSensor(0x3c, 0x58); //Dark1 0x1458
        HI351WriteCmosSensor(0x3d, 0x20);
        HI351WriteCmosSensor(0x3e, 0x59); //Dark1 0x1459
        HI351WriteCmosSensor(0x3f, 0x20);
        HI351WriteCmosSensor(0x40, 0x60); //Dark1 0x1460
        HI351WriteCmosSensor(0x41, 0x03); //skin opt en
        HI351WriteCmosSensor(0x42, 0x61); //Dark1 0x1461
        HI351WriteCmosSensor(0x43, 0xa0);
        HI351WriteCmosSensor(0x44, 0x62); //Dark1 0x1462
        HI351WriteCmosSensor(0x45, 0x98);
        HI351WriteCmosSensor(0x46, 0x63); //Dark1 0x1463
        HI351WriteCmosSensor(0x47, 0xe4); //skin_std_th_h
        HI351WriteCmosSensor(0x48, 0x64); //Dark1 0x1464
        HI351WriteCmosSensor(0x49, 0xa4); //skin_std_th_l
        HI351WriteCmosSensor(0x4a, 0x65); //Dark1 0x1465
        HI351WriteCmosSensor(0x4b, 0x7d); //sharp_std_th_h
        HI351WriteCmosSensor(0x4c, 0x66); //Dark1 0x1466
        HI351WriteCmosSensor(0x4d, 0x4b); //sharp_std_th_l
        HI351WriteCmosSensor(0x4e, 0x70); //Dark1 0x1470
        HI351WriteCmosSensor(0x4f, 0x10);
        HI351WriteCmosSensor(0x50, 0x71); //Dark1 0x1471
        HI351WriteCmosSensor(0x51, 0x10);
        HI351WriteCmosSensor(0x52, 0x72); //Dark1 0x1472
        HI351WriteCmosSensor(0x53, 0x10);
        HI351WriteCmosSensor(0x54, 0x73); //Dark1 0x1473
        HI351WriteCmosSensor(0x55, 0x10);
        HI351WriteCmosSensor(0x56, 0x74); //Dark1 0x1474
        HI351WriteCmosSensor(0x57, 0x10);
        HI351WriteCmosSensor(0x58, 0x75); //Dark1 0x1475
        HI351WriteCmosSensor(0x59, 0x10);
        HI351WriteCmosSensor(0x5a, 0x76); //Dark1 0x1476
        HI351WriteCmosSensor(0x5b, 0x28);
        HI351WriteCmosSensor(0x5c, 0x77); //Dark1 0x1477
        HI351WriteCmosSensor(0x5d, 0x28);
        HI351WriteCmosSensor(0x5e, 0x78); //Dark1 0x1478
        HI351WriteCmosSensor(0x5f, 0x28);
        HI351WriteCmosSensor(0x60, 0x79); //Dark1 0x1479
        HI351WriteCmosSensor(0x61, 0x28);
        HI351WriteCmosSensor(0x62, 0x7a); //Dark1 0x147a
        HI351WriteCmosSensor(0x63, 0x28);
        HI351WriteCmosSensor(0x64, 0x7b); //Dark1 0x147b
        HI351WriteCmosSensor(0x65, 0x28);
              
        
            //////////////////
            // e3 Page (DMA Dark2)
            //////////////////
            
        HI351WriteCmosSensor(0x03, 0xe3);
        HI351WriteCmosSensor(0x10, 0x03);//Dark2 Page11
        HI351WriteCmosSensor(0x11, 0x11);
        HI351WriteCmosSensor(0x12, 0x10);//Dark2 0x1110
        HI351WriteCmosSensor(0x13, 0x1f);
        HI351WriteCmosSensor(0x14, 0x11);//Dark2 0x1111
        HI351WriteCmosSensor(0x15, 0x3f);
        HI351WriteCmosSensor(0x16, 0x12);//Dark2 0x1112
        HI351WriteCmosSensor(0x17, 0x32);
        HI351WriteCmosSensor(0x18, 0x13);//Dark2 0x1113
        HI351WriteCmosSensor(0x19, 0x21);
        HI351WriteCmosSensor(0x1a, 0x14);//Dark2 0x1114
        HI351WriteCmosSensor(0x1b, 0x3a);
        HI351WriteCmosSensor(0x1c, 0x30);//Dark2 0x1130
        HI351WriteCmosSensor(0x1d, 0x26);
        HI351WriteCmosSensor(0x1e, 0x31);//Dark2 0x1131
        HI351WriteCmosSensor(0x1f, 0x20);
        HI351WriteCmosSensor(0x20, 0x32);  //Dark2 0x1132 //STEVE Lum. Level. in DLPF               
        HI351WriteCmosSensor(0x21, 0x8b);  //52                    82                               
        HI351WriteCmosSensor(0x22, 0x33);  //Dark2 0x1133                                           
        HI351WriteCmosSensor(0x23, 0x54);  //3b                    5d                               
        HI351WriteCmosSensor(0x24, 0x34);  //Dark2 0x1134                                           
        HI351WriteCmosSensor(0x25, 0x2c);  //1d                    37                               
        HI351WriteCmosSensor(0x26, 0x35);  //Dark2 0x1135                                           
        HI351WriteCmosSensor(0x27, 0x29);  //21                    30                               
        HI351WriteCmosSensor(0x28, 0x36);  //Dark2 0x1136                                           
        HI351WriteCmosSensor(0x29, 0x18);  //1b                    18                               
        HI351WriteCmosSensor(0x2a, 0x37);  //Dark2 0x1137                                           
        HI351WriteCmosSensor(0x2b, 0x1e);  //21                    24                               
        HI351WriteCmosSensor(0x2c, 0x38);  //Dark2 0x1138                                           
        HI351WriteCmosSensor(0x2d, 0x17);  //18                    18                               
        HI351WriteCmosSensor(0x2e, 0x39);//Dark2 0x1139 gain 1
                       
        HI351WriteCmosSensor(0x2f, 0x80);    //r2 1
        HI351WriteCmosSensor(0x30, 0x3a);//Dark2 0x113a
        HI351WriteCmosSensor(0x31, 0x80);
        HI351WriteCmosSensor(0x32, 0x3b);//Dark2 0x113b
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x34, 0x3c);//Dark2 0x113c
        HI351WriteCmosSensor(0x35, 0x00);   //18
        HI351WriteCmosSensor(0x36, 0x3d);//Dark2 0x113d
        HI351WriteCmosSensor(0x37, 0x00);   //18
        HI351WriteCmosSensor(0x38, 0x3e);//Dark2 0x113e
        HI351WriteCmosSensor(0x39, 0x00);   //18
        HI351WriteCmosSensor(0x3a, 0x3f);//Dark2 0x113f
        HI351WriteCmosSensor(0x3b, 0x00);
        HI351WriteCmosSensor(0x3c, 0x40);//Dark2 0x1140 gain 8
        HI351WriteCmosSensor(0x3d, 0x00);
        HI351WriteCmosSensor(0x3e, 0x41);//Dark2 0x1141
        HI351WriteCmosSensor(0x3f, 0xff);
        HI351WriteCmosSensor(0x40, 0x42);//Dark2 0x1142
        HI351WriteCmosSensor(0x41, 0xff);
        HI351WriteCmosSensor(0x42, 0x43);//Dark2 0x1143
        HI351WriteCmosSensor(0x43, 0xff);
        HI351WriteCmosSensor(0x44, 0x44);//Dark2 0x1144
        HI351WriteCmosSensor(0x45, 0xff);
        HI351WriteCmosSensor(0x46, 0x45);//Dark2 0x1145
        HI351WriteCmosSensor(0x47, 0xff);
        HI351WriteCmosSensor(0x48, 0x46);//Dark2 0x1146
        HI351WriteCmosSensor(0x49, 0xff);
        HI351WriteCmosSensor(0x4a, 0x47);//Dark2 0x1147
        HI351WriteCmosSensor(0x4b, 0xff);
        HI351WriteCmosSensor(0x4c, 0x48);//Dark2 0x1148
        HI351WriteCmosSensor(0x4d, 0xff);
        HI351WriteCmosSensor(0x4e, 0x49);//Dark2 0x1149
        HI351WriteCmosSensor(0x4f, 0x00); //high_clip_start
        HI351WriteCmosSensor(0x50, 0x4a);//Dark2 0x114a
        HI351WriteCmosSensor(0x51, 0x00);
        HI351WriteCmosSensor(0x52, 0x4b);//Dark2 0x114b
        HI351WriteCmosSensor(0x53, 0x00);
        HI351WriteCmosSensor(0x54, 0x4c);//Dark2 0x114c
        HI351WriteCmosSensor(0x55, 0x00);
        HI351WriteCmosSensor(0x56, 0x4d);//Dark2 0x114d
        HI351WriteCmosSensor(0x57, 0x00);
        HI351WriteCmosSensor(0x58, 0x4e);//Dark2 0x114e
        HI351WriteCmosSensor(0x59, 0x00);   //Lv 6 h_clip
        HI351WriteCmosSensor(0x5a, 0x4f);//Dark2 0x114f
        HI351WriteCmosSensor(0x5b, 0x00);   //Lv 7 h_clip 
        HI351WriteCmosSensor(0x5c, 0x50);//Dark2 0x1150 clip 8
        HI351WriteCmosSensor(0x5d, 0x00);
        HI351WriteCmosSensor(0x5e, 0x51);//Dark2 0x1151
        HI351WriteCmosSensor(0x5f, 0xd8); //color gain start
        HI351WriteCmosSensor(0x60, 0x52);//Dark2 0x1152
        HI351WriteCmosSensor(0x61, 0xd8);
        HI351WriteCmosSensor(0x62, 0x53);//Dark2 0x1153
        HI351WriteCmosSensor(0x63, 0xd8);
        HI351WriteCmosSensor(0x64, 0x54);//Dark2 0x1154
        HI351WriteCmosSensor(0x65, 0xd0);
        HI351WriteCmosSensor(0x66, 0x55);//Dark2 0x1155
        HI351WriteCmosSensor(0x67, 0xd0);
        HI351WriteCmosSensor(0x68, 0x56);//Dark2 0x1156
        HI351WriteCmosSensor(0x69, 0xc8);
        HI351WriteCmosSensor(0x6a, 0x57);//Dark2 0x1157
        HI351WriteCmosSensor(0x6b, 0xc0);
        HI351WriteCmosSensor(0x6c, 0x58);//Dark2 0x1158
        HI351WriteCmosSensor(0x6d, 0xc0); //color gain end
        HI351WriteCmosSensor(0x6e, 0x59);//Dark2 0x1159
        HI351WriteCmosSensor(0x6f, 0xf0); //color ofs lmt start
        HI351WriteCmosSensor(0x70, 0x5a);//Dark2 0x115a
        HI351WriteCmosSensor(0x71, 0xf0);
        HI351WriteCmosSensor(0x72, 0x5b);//Dark2 0x115b
        HI351WriteCmosSensor(0x73, 0xf0);
        HI351WriteCmosSensor(0x74, 0x5c);//Dark2 0x115c
        HI351WriteCmosSensor(0x75, 0xe8);
        HI351WriteCmosSensor(0x76, 0x5d);//Dark2 0x115d
        HI351WriteCmosSensor(0x77, 0xe8);
        HI351WriteCmosSensor(0x78, 0x5e);//Dark2 0x115e
        HI351WriteCmosSensor(0x79, 0xe0);
        HI351WriteCmosSensor(0x7a, 0x5f);//Dark2 0x115f
        HI351WriteCmosSensor(0x7b, 0xe0);
        HI351WriteCmosSensor(0x7c, 0x60);//Dark2 0x1160
        HI351WriteCmosSensor(0x7d, 0xe0);//color ofs lmt end
        HI351WriteCmosSensor(0x7e, 0x61);//Dark2 0x1161
        HI351WriteCmosSensor(0x7f, 0xf0);
        HI351WriteCmosSensor(0x80, 0x62);//Dark2 0x1162
        HI351WriteCmosSensor(0x81, 0xf0);
        HI351WriteCmosSensor(0x82, 0x63);//Dark2 0x1163
        HI351WriteCmosSensor(0x83, 0x80);
        HI351WriteCmosSensor(0x84, 0x64);//Dark2 0x1164
        HI351WriteCmosSensor(0x85, 0x40);
        HI351WriteCmosSensor(0x86, 0x65);//Dark2 0x1165
        HI351WriteCmosSensor(0x87, 0x00);
        HI351WriteCmosSensor(0x88, 0x66);//Dark2 0x1166
        HI351WriteCmosSensor(0x89, 0x00);
        HI351WriteCmosSensor(0x8a, 0x67);//Dark2 0x1167
        HI351WriteCmosSensor(0x8b, 0x00);
        HI351WriteCmosSensor(0x8c, 0x68);//Dark2 0x1168
        HI351WriteCmosSensor(0x8d, 0x80);
        HI351WriteCmosSensor(0x8e, 0x69);//Dark2 0x1169
        HI351WriteCmosSensor(0x8f, 0x40);
        HI351WriteCmosSensor(0x90, 0x6a);//Dark2 0x116a
        HI351WriteCmosSensor(0x91, 0x00);
        HI351WriteCmosSensor(0x92, 0x6b);//Dark2 0x116b
        HI351WriteCmosSensor(0x93, 0x00);
        HI351WriteCmosSensor(0x94, 0x6c);//Dark2 0x116c
        HI351WriteCmosSensor(0x95, 0x00);
        HI351WriteCmosSensor(0x96, 0x6d);//Dark2 0x116d
        HI351WriteCmosSensor(0x97, 0x80);
        HI351WriteCmosSensor(0x98, 0x6e);//Dark2 0x116e
        HI351WriteCmosSensor(0x99, 0x40);
        HI351WriteCmosSensor(0x9a, 0x6f);//Dark2 0x116f
        HI351WriteCmosSensor(0x9b, 0x00);
        HI351WriteCmosSensor(0x9c, 0x70);//Dark2 0x1170
        HI351WriteCmosSensor(0x9d, 0x00);
        HI351WriteCmosSensor(0x9e, 0x71);//Dark2 0x1171
        HI351WriteCmosSensor(0x9f, 0x00);
        HI351WriteCmosSensor(0xa0, 0x72);//Dark2 0x1172
        HI351WriteCmosSensor(0xa1, 0x6e);
        HI351WriteCmosSensor(0xa2, 0x73);//Dark2 0x1173
        HI351WriteCmosSensor(0xa3, 0x3a);
        HI351WriteCmosSensor(0xa4, 0x74);//Dark2 0x1174
        HI351WriteCmosSensor(0xa5, 0x00);
        HI351WriteCmosSensor(0xa6, 0x75);//Dark2 0x1175
        HI351WriteCmosSensor(0xa7, 0x00);
        HI351WriteCmosSensor(0xa8, 0x76);//Dark2 0x1176
        HI351WriteCmosSensor(0xa9, 0x00);//18
        HI351WriteCmosSensor(0xaa, 0x77);//Dark2 0x1177
        HI351WriteCmosSensor(0xab, 0x6e);
        HI351WriteCmosSensor(0xac, 0x78);//Dark2 0x1178
        HI351WriteCmosSensor(0xad, 0x3a);
        HI351WriteCmosSensor(0xae, 0x79);//Dark2 0x1179
        HI351WriteCmosSensor(0xaf, 0x00);
        HI351WriteCmosSensor(0xb0, 0x7a);//Dark2 0x117a
        HI351WriteCmosSensor(0xb1, 0x00);
        HI351WriteCmosSensor(0xb2, 0x7b);//Dark2 0x117b
        HI351WriteCmosSensor(0xb3, 0x00);
        HI351WriteCmosSensor(0xb4, 0x7c);//Dark2 0x117c
        HI351WriteCmosSensor(0xb5, 0x5c);
        HI351WriteCmosSensor(0xb6, 0x7d);//Dark2 0x117d
        HI351WriteCmosSensor(0xb7, 0x30);
        HI351WriteCmosSensor(0xb8, 0x7e);//Dark2 0x117e
        HI351WriteCmosSensor(0xb9, 0x00);
        HI351WriteCmosSensor(0xba, 0x7f);//Dark2 0x117f
        HI351WriteCmosSensor(0xbb, 0x00);
        HI351WriteCmosSensor(0xbc, 0x80);//Dark2 0x1180
        HI351WriteCmosSensor(0xbd, 0x00);
        HI351WriteCmosSensor(0xbe, 0x81);//Dark2 0x1181
        HI351WriteCmosSensor(0xbf, 0x62);
        HI351WriteCmosSensor(0xc0, 0x82);//Dark2 0x1182
        HI351WriteCmosSensor(0xc1, 0x26);
        HI351WriteCmosSensor(0xc2, 0x83);//Dark2 0x1183
        HI351WriteCmosSensor(0xc3, 0x00);
        HI351WriteCmosSensor(0xc4, 0x84);//Dark2 0x1184
        HI351WriteCmosSensor(0xc5, 0x00);
        HI351WriteCmosSensor(0xc6, 0x85);//Dark2 0x1185
        HI351WriteCmosSensor(0xc7, 0x00);
        HI351WriteCmosSensor(0xc8, 0x86);//Dark2 0x1186
        HI351WriteCmosSensor(0xc9, 0x62);
        HI351WriteCmosSensor(0xca, 0x87);//Dark2 0x1187
        HI351WriteCmosSensor(0xcb, 0x26);
        HI351WriteCmosSensor(0xcc, 0x88);//Dark2 0x1188
        HI351WriteCmosSensor(0xcd, 0x00);
        HI351WriteCmosSensor(0xce, 0x89);//Dark2 0x1189
        HI351WriteCmosSensor(0xcf, 0x00);
        HI351WriteCmosSensor(0xd0, 0x8a);//Dark2 0x118a
        HI351WriteCmosSensor(0xd1, 0x00);
        HI351WriteCmosSensor(0xd2, 0x90);//Dark2 0x1190
        HI351WriteCmosSensor(0xd3, 0x03);
        HI351WriteCmosSensor(0xd4, 0x91);//Dark2 0x1191
        HI351WriteCmosSensor(0xd5, 0xff);
        HI351WriteCmosSensor(0xd6, 0x92);//Dark2 0x1192
        HI351WriteCmosSensor(0xd7, 0x00);
        HI351WriteCmosSensor(0xd8, 0x93);//Dark2 0x1193
        HI351WriteCmosSensor(0xd9, 0x00);
        HI351WriteCmosSensor(0xda, 0x94);//Dark2 0x1194
        HI351WriteCmosSensor(0xdb, 0x03);
        HI351WriteCmosSensor(0xdc, 0x95);//Dark2 0x1195
        HI351WriteCmosSensor(0xdd, 0x64);
        HI351WriteCmosSensor(0xde, 0x96);//Dark2 0x1196
        HI351WriteCmosSensor(0xdf, 0x00);
        HI351WriteCmosSensor(0xe0, 0x97);//Dark2 0x1197
        HI351WriteCmosSensor(0xe1, 0x00);
        HI351WriteCmosSensor(0xe2, 0xb0);//Dark2 0x11b0
        HI351WriteCmosSensor(0xe3, 0x64);
        HI351WriteCmosSensor(0xe4, 0xb1);//Dark2 0x11b1
        HI351WriteCmosSensor(0xe5, 0x80);
        HI351WriteCmosSensor(0xe6, 0xb2);//Dark2 0x11b2
        HI351WriteCmosSensor(0xe7, 0x00);
        HI351WriteCmosSensor(0xe8, 0xb3);//Dark2 0x11b3
        HI351WriteCmosSensor(0xe9, 0x00);
        HI351WriteCmosSensor(0xea, 0xb4);//Dark2 0x11b4
        HI351WriteCmosSensor(0xeb, 0x03);
        HI351WriteCmosSensor(0xec, 0x03);
        HI351WriteCmosSensor(0xed, 0x12);//12 page
        HI351WriteCmosSensor(0xee, 0x10); //Dark2 0x1210
        HI351WriteCmosSensor(0xef, 0x03);
        HI351WriteCmosSensor(0xf0, 0x11); //Dark2 0x1211
        HI351WriteCmosSensor(0xf1, 0x29);
        HI351WriteCmosSensor(0xf2, 0x12); //Dark2 0x1212
        HI351WriteCmosSensor(0xf3, 0x08);
        HI351WriteCmosSensor(0xf4, 0x40); //Dark2 0x1240
        HI351WriteCmosSensor(0xf5, 0x33); //07
        HI351WriteCmosSensor(0xf6, 0x41); //Dark2 0x1241
        HI351WriteCmosSensor(0xf7, 0x0a); //32
        HI351WriteCmosSensor(0xf8, 0x42); //Dark2 0x1242
        HI351WriteCmosSensor(0xf9, 0x6a); //8c
        HI351WriteCmosSensor(0xfa, 0x43); //Dark2 0x1243
        HI351WriteCmosSensor(0xfb, 0x80);
        HI351WriteCmosSensor(0xfc, 0x44); //Dark2 0x1244
        HI351WriteCmosSensor(0xfd, 0x02);
            
        HI351WriteCmosSensor(0x03, 0xe4);
        HI351WriteCmosSensor(0x10, 0x45); //Dark2 0x1245
        HI351WriteCmosSensor(0x11, 0x0a);
        HI351WriteCmosSensor(0x12, 0x46); //Dark2 0x1246
        HI351WriteCmosSensor(0x13, 0x80);
        HI351WriteCmosSensor(0x14, 0x60); //Dark2 0x1260
        HI351WriteCmosSensor(0x15, 0x02);
        HI351WriteCmosSensor(0x16, 0x61); //Dark2 0x1261
        HI351WriteCmosSensor(0x17, 0x04);
        HI351WriteCmosSensor(0x18, 0x62); //Dark2 0x1262
        HI351WriteCmosSensor(0x19, 0x4b);
        HI351WriteCmosSensor(0x1a, 0x63); //Dark2 0x1263
        HI351WriteCmosSensor(0x1b, 0x41);
        HI351WriteCmosSensor(0x1c, 0x64); //Dark2 0x1264
        HI351WriteCmosSensor(0x1d, 0x14);
        HI351WriteCmosSensor(0x1e, 0x65); //Dark2 0x1265
        HI351WriteCmosSensor(0x1f, 0x00);
        HI351WriteCmosSensor(0x20, 0x68); //Dark2 0x1268
        HI351WriteCmosSensor(0x21, 0x0a);
        HI351WriteCmosSensor(0x22, 0x69); //Dark2 0x1269
        HI351WriteCmosSensor(0x23, 0x04);
        HI351WriteCmosSensor(0x24, 0x6a); //Dark2 0x126a
        HI351WriteCmosSensor(0x25, 0x0a);
        HI351WriteCmosSensor(0x26, 0x6b); //Dark2 0x126b
        HI351WriteCmosSensor(0x27, 0x0a);
        HI351WriteCmosSensor(0x28, 0x6c); //Dark2 0x126c
        HI351WriteCmosSensor(0x29, 0x24);
        HI351WriteCmosSensor(0x2a, 0x6d); //Dark2 0x126d
        HI351WriteCmosSensor(0x2b, 0x01);
        HI351WriteCmosSensor(0x2c, 0x70); //Dark2 0x1270
        HI351WriteCmosSensor(0x2d, 0x38); 
        HI351WriteCmosSensor(0x2e, 0x71); //Dark2 0x1271
        HI351WriteCmosSensor(0x2f, 0xbf);
        HI351WriteCmosSensor(0x30, 0x80); //Dark2 0x1280
        HI351WriteCmosSensor(0x31, 0x64);
        HI351WriteCmosSensor(0x32, 0x81); //Dark2 0x1281
        HI351WriteCmosSensor(0x33, 0x80);
        HI351WriteCmosSensor(0x34, 0x82); //Dark2 0x1282
        HI351WriteCmosSensor(0x35, 0x80);
        HI351WriteCmosSensor(0x36, 0x83); //Dark2 0x1283
        HI351WriteCmosSensor(0x37, 0x02);
        HI351WriteCmosSensor(0x38, 0x84); //Dark2 0x1284
        HI351WriteCmosSensor(0x39, 0x30);
        HI351WriteCmosSensor(0x3a, 0x85); //Dark2 0x1285
        HI351WriteCmosSensor(0x3b, 0x80);
        HI351WriteCmosSensor(0x3c, 0x86); //Dark2 0x1286
        HI351WriteCmosSensor(0x3d, 0x80);
        HI351WriteCmosSensor(0x3e, 0x87); //Dark2 0x1287
        HI351WriteCmosSensor(0x3f, 0x01);
        HI351WriteCmosSensor(0x40, 0x88); //Dark2 0x1288
        HI351WriteCmosSensor(0x41, 0x3a);
        HI351WriteCmosSensor(0x42, 0x89); //Dark2 0x1289
        HI351WriteCmosSensor(0x43, 0x80);
        HI351WriteCmosSensor(0x44, 0x8a); //Dark2 0x128a
        HI351WriteCmosSensor(0x45, 0x80);
        HI351WriteCmosSensor(0x46, 0x8b); //Dark2 0x128b
        HI351WriteCmosSensor(0x47, 0x0c);
        HI351WriteCmosSensor(0x48, 0x8c); //Dark2 0x128c
        HI351WriteCmosSensor(0x49, 0x05);
        HI351WriteCmosSensor(0x4a, 0x8d); //Dark2 0x128d
        HI351WriteCmosSensor(0x4b, 0x03);
        HI351WriteCmosSensor(0x4c, 0xe6); //Dark2 0x12e6
        HI351WriteCmosSensor(0x4d, 0xff);
        HI351WriteCmosSensor(0x4e, 0xe7); //Dark2 0x12e7
        HI351WriteCmosSensor(0x4f, 0x18);
        HI351WriteCmosSensor(0x50, 0xe8); //Dark2 0x12e8
        HI351WriteCmosSensor(0x51, 0x0a);
        HI351WriteCmosSensor(0x52, 0xe9); //Dark2 0x12e9
        HI351WriteCmosSensor(0x53, 0x04);
        HI351WriteCmosSensor(0x54, 0x03);
        HI351WriteCmosSensor(0x55, 0x13);//13 page
        HI351WriteCmosSensor(0x56, 0x10); //Dark2 0x1310
        HI351WriteCmosSensor(0x57, 0x3f);
        HI351WriteCmosSensor(0x58, 0x20); //Dark2 0x1320
        HI351WriteCmosSensor(0x59, 0x20);
        HI351WriteCmosSensor(0x5a, 0x21); //Dark2 0x1321
        HI351WriteCmosSensor(0x5b, 0x30);
        HI351WriteCmosSensor(0x5c, 0x22); //Dark2 0x1322
        HI351WriteCmosSensor(0x5d, 0x36);
        HI351WriteCmosSensor(0x5e, 0x23); //Dark2 0x1323
        HI351WriteCmosSensor(0x5f, 0x6a);
        HI351WriteCmosSensor(0x60, 0x24); //Dark2 0x1324
        HI351WriteCmosSensor(0x61, 0xa0);
        HI351WriteCmosSensor(0x62, 0x25); //Dark2 0x1325
        HI351WriteCmosSensor(0x63, 0xc0);
        HI351WriteCmosSensor(0x64, 0x26); //Dark2 0x1326
        HI351WriteCmosSensor(0x65, 0xe0);
        HI351WriteCmosSensor(0x66, 0x27); //Dark2 0x1327 lum 0
        HI351WriteCmosSensor(0x67, 0x04);
        HI351WriteCmosSensor(0x68, 0x28); //Dark2 0x1328
        HI351WriteCmosSensor(0x69, 0x00);
        HI351WriteCmosSensor(0x6a, 0x29); //Dark2 0x1329
        HI351WriteCmosSensor(0x6b, 0x00);
        HI351WriteCmosSensor(0x6c, 0x2a); //Dark2 0x132a
        HI351WriteCmosSensor(0x6d, 0x00);
        HI351WriteCmosSensor(0x6e, 0x2b); //Dark2 0x132b
        HI351WriteCmosSensor(0x6f, 0x00);
        HI351WriteCmosSensor(0x70, 0x2c); //Dark2 0x132c
        HI351WriteCmosSensor(0x71, 0x00);
        HI351WriteCmosSensor(0x72, 0x2d); //Dark2 0x132d
        HI351WriteCmosSensor(0x73, 0x00);
        HI351WriteCmosSensor(0x74, 0x2e); //Dark2 0x132e lum7
        HI351WriteCmosSensor(0x75, 0x00);
        HI351WriteCmosSensor(0x76, 0x2f); //Dark2 0x132f       //weight skin
        HI351WriteCmosSensor(0x77, 0x00);
        HI351WriteCmosSensor(0x78, 0x30); //Dark2 0x1330       //weight blue
        HI351WriteCmosSensor(0x79, 0x00);
        HI351WriteCmosSensor(0x7a, 0x31); //Dark2 0x1331       //weight green
        HI351WriteCmosSensor(0x7b, 0x00);
        HI351WriteCmosSensor(0x7c, 0x32); //Dark2 0x1332       //weight strong color
        HI351WriteCmosSensor(0x7d, 0x00);
        HI351WriteCmosSensor(0x7e, 0x33); //Dark2 0x1333
        HI351WriteCmosSensor(0x7f, 0x00);
        HI351WriteCmosSensor(0x80, 0x34); //Dark2 0x1334
        HI351WriteCmosSensor(0x81, 0x00);
        HI351WriteCmosSensor(0x82, 0x35); //Dark2 0x1335
        HI351WriteCmosSensor(0x83, 0x00);
        HI351WriteCmosSensor(0x84, 0x36); //Dark2 0x1336
        HI351WriteCmosSensor(0x85, 0x00);
        HI351WriteCmosSensor(0x86, 0xa0); //Dark2 0x13a0
        HI351WriteCmosSensor(0x87, 0x07);
        HI351WriteCmosSensor(0x88, 0xa8); //Dark2 0x13a8       //Dark2 Cb-filter 0x20
        HI351WriteCmosSensor(0x89, 0x30);
        HI351WriteCmosSensor(0x8a, 0xa9); //Dark2 0x13a9       //Dark2 Cr-filter 0x20
        HI351WriteCmosSensor(0x8b, 0x30);
        HI351WriteCmosSensor(0x8c, 0xaa); //Dark2 0x13aa
        HI351WriteCmosSensor(0x8d, 0x30);
        HI351WriteCmosSensor(0x8e, 0xab); //Dark2 0x13ab
        HI351WriteCmosSensor(0x8f, 0x02);
        HI351WriteCmosSensor(0x90, 0xc0); //Dark2 0x13c0
        HI351WriteCmosSensor(0x91, 0x27);
        HI351WriteCmosSensor(0x92, 0xc2); //Dark2 0x13c2
        HI351WriteCmosSensor(0x93, 0x08);
        HI351WriteCmosSensor(0x94, 0xc3); //Dark2 0x13c3
        HI351WriteCmosSensor(0x95, 0x08);
        HI351WriteCmosSensor(0x96, 0xc4); //Dark2 0x13c4
        HI351WriteCmosSensor(0x97, 0x46);
        HI351WriteCmosSensor(0x98, 0xc5); //Dark2 0x13c5
        HI351WriteCmosSensor(0x99, 0x78);
        HI351WriteCmosSensor(0x9a, 0xc6); //Dark2 0x13c6
        HI351WriteCmosSensor(0x9b, 0xf0);
        HI351WriteCmosSensor(0x9c, 0xc7); //Dark2 0x13c7
        HI351WriteCmosSensor(0x9d, 0x10);
        HI351WriteCmosSensor(0x9e, 0xc8); //Dark2 0x13c8
        HI351WriteCmosSensor(0x9f, 0x44);
        HI351WriteCmosSensor(0xa0, 0xc9); //Dark2 0x13c9
        HI351WriteCmosSensor(0xa1, 0x87);
        HI351WriteCmosSensor(0xa2, 0xca); //Dark2 0x13ca
        HI351WriteCmosSensor(0xa3, 0xff);
        HI351WriteCmosSensor(0xa4, 0xcb); //Dark2 0x13cb
        HI351WriteCmosSensor(0xa5, 0x20);
        HI351WriteCmosSensor(0xa6, 0xcc); //Dark2 0x13cc       //skin range_cb_l
        HI351WriteCmosSensor(0xa7, 0x61);
        HI351WriteCmosSensor(0xa8, 0xcd); //Dark2 0x13cd       //skin range_cb_h
        HI351WriteCmosSensor(0xa9, 0x87);
        HI351WriteCmosSensor(0xaa, 0xce); //Dark2 0x13ce       //skin range_cr_l
        HI351WriteCmosSensor(0xab, 0x8a);
        HI351WriteCmosSensor(0xac, 0xcf); //Dark2 0x13cf       //skin range_cr_h
        HI351WriteCmosSensor(0xad, 0xa5);
        HI351WriteCmosSensor(0xae, 0x03); //14 page
        HI351WriteCmosSensor(0xaf, 0x14);
        HI351WriteCmosSensor(0xb0, 0x10); //Dark2 0x1410
        HI351WriteCmosSensor(0xb1, 0x00);
        HI351WriteCmosSensor(0xb2, 0x11); //Dark2 0x1411
        HI351WriteCmosSensor(0xb3, 0x00);
        HI351WriteCmosSensor(0xb4, 0x12); //Dark2 0x1412
        HI351WriteCmosSensor(0xb5, 0x40); //Top H_Clip
        HI351WriteCmosSensor(0xb6, 0x13); //Dark2 0x1413
        HI351WriteCmosSensor(0xb7, 0xc8);
        HI351WriteCmosSensor(0xb8, 0x14); //Dark2 0x1414
        HI351WriteCmosSensor(0xb9, 0x50);
        HI351WriteCmosSensor(0xba, 0x15); //Dark2 0x1415       //sharp positive hi
        HI351WriteCmosSensor(0xbb, 0x19);
        HI351WriteCmosSensor(0xbc, 0x16); //Dark2 0x1416       //sharp positive mi
        HI351WriteCmosSensor(0xbd, 0x19);
        HI351WriteCmosSensor(0xbe, 0x17); //Dark2 0x1417       //sharp positive low
        HI351WriteCmosSensor(0xbf, 0x19);
        HI351WriteCmosSensor(0xc0, 0x18); //Dark2 0x1418       //sharp negative hi
        HI351WriteCmosSensor(0xc1, 0x33);
        HI351WriteCmosSensor(0xc2, 0x19); //Dark2 0x1419       //sharp negative mi
        HI351WriteCmosSensor(0xc3, 0x33);
        HI351WriteCmosSensor(0xc4, 0x1a); //Dark2 0x141a       //sharp negative low
        HI351WriteCmosSensor(0xc5, 0x33);
        HI351WriteCmosSensor(0xc6, 0x20); //Dark2 0x1420
        HI351WriteCmosSensor(0xc7, 0x80);
        HI351WriteCmosSensor(0xc8, 0x21); //Dark2 0x1421
        HI351WriteCmosSensor(0xc9, 0x03);
        HI351WriteCmosSensor(0xca, 0x22); //Dark2 0x1422
        HI351WriteCmosSensor(0xcb, 0x05);
        HI351WriteCmosSensor(0xcc, 0x23); //Dark2 0x1423
        HI351WriteCmosSensor(0xcd, 0x07);
        HI351WriteCmosSensor(0xce, 0x24); //Dark2 0x1424
        HI351WriteCmosSensor(0xcf, 0x0a);
        HI351WriteCmosSensor(0xd0, 0x25); //Dark2 0x1425
        HI351WriteCmosSensor(0xd1, 0x46);
        HI351WriteCmosSensor(0xd2, 0x26); //Dark2 0x1426
        HI351WriteCmosSensor(0xd3, 0x32);
        HI351WriteCmosSensor(0xd4, 0x27); //Dark2 0x1427
        HI351WriteCmosSensor(0xd5, 0x1e);
        HI351WriteCmosSensor(0xd6, 0x28); //Dark2 0x1428
        HI351WriteCmosSensor(0xd7, 0x19);
        HI351WriteCmosSensor(0xd8, 0x29); //Dark2 0x1429
        HI351WriteCmosSensor(0xd9, 0x00);
        HI351WriteCmosSensor(0xda, 0x2a); //Dark2 0x142a
        HI351WriteCmosSensor(0xdb, 0x10);
        HI351WriteCmosSensor(0xdc, 0x2b); //Dark2 0x142b
        HI351WriteCmosSensor(0xdd, 0x10);
        HI351WriteCmosSensor(0xde, 0x2c); //Dark2 0x142c
        HI351WriteCmosSensor(0xdf, 0x10);
        HI351WriteCmosSensor(0xe0, 0x2d); //Dark2 0x142d
        HI351WriteCmosSensor(0xe1, 0x80);
        HI351WriteCmosSensor(0xe2, 0x2e); //Dark2 0x142e
        HI351WriteCmosSensor(0xe3, 0x80);
        HI351WriteCmosSensor(0xe4, 0x2f); //Dark2 0x142f
        HI351WriteCmosSensor(0xe5, 0x80);
        HI351WriteCmosSensor(0xe6, 0x30); //Dark2 0x1430
        HI351WriteCmosSensor(0xe7, 0x80);
        HI351WriteCmosSensor(0xe8, 0x31); //Dark2 0x1431
        HI351WriteCmosSensor(0xe9, 0x02);
        HI351WriteCmosSensor(0xea, 0x32); //Dark2 0x1432
        HI351WriteCmosSensor(0xeb, 0x04);
        HI351WriteCmosSensor(0xec, 0x33); //Dark2 0x1433
        HI351WriteCmosSensor(0xed, 0x04);
        HI351WriteCmosSensor(0xee, 0x34); //Dark2 0x1434
        HI351WriteCmosSensor(0xef, 0x0a);
        HI351WriteCmosSensor(0xf0, 0x35); //Dark2 0x1435
        HI351WriteCmosSensor(0xf1, 0x46);
        HI351WriteCmosSensor(0xf2, 0x36); //Dark2 0x1436
        HI351WriteCmosSensor(0xf3, 0x32);
        HI351WriteCmosSensor(0xf4, 0x37); //Dark2 0x1437
        HI351WriteCmosSensor(0xf5, 0x28);
        HI351WriteCmosSensor(0xf6, 0x38); //Dark2 0x1438
        HI351WriteCmosSensor(0xf7, 0x12);//2d
        HI351WriteCmosSensor(0xf8, 0x39); //Dark2 0x1439
        HI351WriteCmosSensor(0xf9, 0x00);//23
        HI351WriteCmosSensor(0xfa, 0x3a); //Dark2 0x143a
        HI351WriteCmosSensor(0xfb, 0x18); //dr gain
        HI351WriteCmosSensor(0xfc, 0x3b); //Dark2 0x143b
        HI351WriteCmosSensor(0xfd, 0x20);
            
            
        HI351WriteCmosSensor(0x03, 0xe5);
        HI351WriteCmosSensor(0x10, 0x3c); //Dark2 0x143c
        HI351WriteCmosSensor(0x11, 0x18);
        HI351WriteCmosSensor(0x12, 0x3d); //Dark2 0x143d
        HI351WriteCmosSensor(0x13, 0x20); //nor gain
        HI351WriteCmosSensor(0x14, 0x3e); //Dark2 0x143e
        HI351WriteCmosSensor(0x15, 0x22);
        HI351WriteCmosSensor(0x16, 0x3f); //Dark2 0x143f
        HI351WriteCmosSensor(0x17, 0x10);
        HI351WriteCmosSensor(0x18, 0x40); //Dark2 0x1440
        HI351WriteCmosSensor(0x19, 0x80);
        HI351WriteCmosSensor(0x1a, 0x41); //Dark2 0x1441
        HI351WriteCmosSensor(0x1b, 0x12);
        HI351WriteCmosSensor(0x1c, 0x42); //Dark2 0x1442
        HI351WriteCmosSensor(0x1d, 0xb0);
        HI351WriteCmosSensor(0x1e, 0x43); //Dark2 0x1443
        HI351WriteCmosSensor(0x1f, 0x20);
        HI351WriteCmosSensor(0x20, 0x44); //Dark2 0x1444
        HI351WriteCmosSensor(0x21, 0x20);
        HI351WriteCmosSensor(0x22, 0x45); //Dark2 0x1445
        HI351WriteCmosSensor(0x23, 0x20);
        HI351WriteCmosSensor(0x24, 0x46); //Dark2 0x1446
        HI351WriteCmosSensor(0x25, 0x20);
        HI351WriteCmosSensor(0x26, 0x47); //Dark2 0x1447
        HI351WriteCmosSensor(0x27, 0x08);
        HI351WriteCmosSensor(0x28, 0x48); //Dark2 0x1448
        HI351WriteCmosSensor(0x29, 0x08);
        HI351WriteCmosSensor(0x2a, 0x49); //Dark2 0x1449
        HI351WriteCmosSensor(0x2b, 0x08);
        HI351WriteCmosSensor(0x2c, 0x50); //Dark2 0x1450
        HI351WriteCmosSensor(0x2d, 0x80);
        HI351WriteCmosSensor(0x2e, 0x51); //Dark2 0x1451
        HI351WriteCmosSensor(0x2f, 0x32); //
        HI351WriteCmosSensor(0x30, 0x52); //Dark2 0x1452
        HI351WriteCmosSensor(0x31, 0x40);
        HI351WriteCmosSensor(0x32, 0x53); //Dark2 0x1453
        HI351WriteCmosSensor(0x33, 0x19);
        HI351WriteCmosSensor(0x34, 0x54); //Dark2 0x1454
        HI351WriteCmosSensor(0x35, 0x60);
        HI351WriteCmosSensor(0x36, 0x55); //Dark2 0x1455
        HI351WriteCmosSensor(0x37, 0x60);
        HI351WriteCmosSensor(0x38, 0x56); //Dark2 0x1456
        HI351WriteCmosSensor(0x39, 0x60);
        HI351WriteCmosSensor(0x3a, 0x57); //Dark2 0x1457
        HI351WriteCmosSensor(0x3b, 0x20);
        HI351WriteCmosSensor(0x3c, 0x58); //Dark2 0x1458
        HI351WriteCmosSensor(0x3d, 0x20);
        HI351WriteCmosSensor(0x3e, 0x59); //Dark2 0x1459
        HI351WriteCmosSensor(0x3f, 0x20);
        HI351WriteCmosSensor(0x40, 0x60); //Dark2 0x1460
        HI351WriteCmosSensor(0x41, 0x03); //skin opt en
        HI351WriteCmosSensor(0x42, 0x61); //Dark2 0x1461
        HI351WriteCmosSensor(0x43, 0xa0);
        HI351WriteCmosSensor(0x44, 0x62); //Dark2 0x1462
        HI351WriteCmosSensor(0x45, 0x98);
        HI351WriteCmosSensor(0x46, 0x63); //Dark2 0x1463
        HI351WriteCmosSensor(0x47, 0xe4); //skin_std_th_h
        HI351WriteCmosSensor(0x48, 0x64); //Dark2 0x1464
        HI351WriteCmosSensor(0x49, 0xa4); //skin_std_th_l
        HI351WriteCmosSensor(0x4a, 0x65); //Dark2 0x1465
        HI351WriteCmosSensor(0x4b, 0x7d); //sharp_std_th_h
        HI351WriteCmosSensor(0x4c, 0x66); //Dark2 0x1466
        HI351WriteCmosSensor(0x4d, 0x4b); //sharp_std_th_l
        HI351WriteCmosSensor(0x4e, 0x70); //Dark2 0x1470
        HI351WriteCmosSensor(0x4f, 0x10);
        HI351WriteCmosSensor(0x50, 0x71); //Dark2 0x1471
        HI351WriteCmosSensor(0x51, 0x10);
        HI351WriteCmosSensor(0x52, 0x72); //Dark2 0x1472
        HI351WriteCmosSensor(0x53, 0x10);
        HI351WriteCmosSensor(0x54, 0x73); //Dark2 0x1473
        HI351WriteCmosSensor(0x55, 0x10);
        HI351WriteCmosSensor(0x56, 0x74); //Dark2 0x1474
        HI351WriteCmosSensor(0x57, 0x10);
        HI351WriteCmosSensor(0x58, 0x75); //Dark2 0x1475
        HI351WriteCmosSensor(0x59, 0x10);
        HI351WriteCmosSensor(0x5a, 0x76); //Dark2 0x1476
        HI351WriteCmosSensor(0x5b, 0x28);
        HI351WriteCmosSensor(0x5c, 0x77); //Dark2 0x1477
        HI351WriteCmosSensor(0x5d, 0x28);
        HI351WriteCmosSensor(0x5e, 0x78); //Dark2 0x1478
        HI351WriteCmosSensor(0x5f, 0x28);
        HI351WriteCmosSensor(0x60, 0x79); //Dark2 0x1479
        HI351WriteCmosSensor(0x61, 0x28);
        HI351WriteCmosSensor(0x62, 0x7a); //Dark2 0x147a
        HI351WriteCmosSensor(0x63, 0x28);
        HI351WriteCmosSensor(0x64, 0x7b); //Dark2 0x147b
        HI351WriteCmosSensor(0x65, 0x28);
        
        
        
            // DMA END
            
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x01, 0xF1); //Sleep mode on
            
        HI351WriteCmosSensor(0x03, 0xc0);
        HI351WriteCmosSensor(0x16, 0x80); //MCU main roof holding off
            
        HI351WriteCmosSensor(0x03, 0xC0);
        HI351WriteCmosSensor(0x33, 0x01); //DMA hand shake mode set
        HI351WriteCmosSensor(0x32, 0x01); //DMA off
        HI351WriteCmosSensor(0x03, 0x30);
        HI351WriteCmosSensor(0x11, 0x04); //Bit[0]: MCU hold off
            
        HI351WriteCmosSensor(0x03, 0xc0);
        HI351WriteCmosSensor(0xe1, 0x00);
            
        HI351WriteCmosSensor(0x03, 0x30);
        HI351WriteCmosSensor(0x25, 0x0e);
        HI351WriteCmosSensor(0x25, 0x1e);
            ///////////////////////////////////////////
            // 1F Page SSD
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x1f); //1F page
        HI351WriteCmosSensor(0x11, 0x00); //bit[5:4]: debug mode
        HI351WriteCmosSensor(0x12, 0x60); 
        HI351WriteCmosSensor(0x13, 0x14); 
        HI351WriteCmosSensor(0x14, 0x10); 
        HI351WriteCmosSensor(0x15, 0x00); 
        HI351WriteCmosSensor(0x20, 0x18); //ssd_x_start_pos
        HI351WriteCmosSensor(0x21, 0x14); //ssd_y_start_pos
        HI351WriteCmosSensor(0x22, 0x8C); //ssd_blk_width
        HI351WriteCmosSensor(0x23, 0x9C); //ssd_blk_height
        HI351WriteCmosSensor(0x28, 0x18);
        HI351WriteCmosSensor(0x29, 0x02);
        HI351WriteCmosSensor(0x3B, 0x18);
        HI351WriteCmosSensor(0x3C, 0x8C);
        HI351WriteCmosSensor(0x10, 0x19); //SSD enable
            
        HI351WriteCmosSensor(0x03, 0xc4); //AE en
        HI351WriteCmosSensor(0x10, 0xe3);
            
        HI351WriteCmosSensor(0x03, 0xc3); //AE Static en
        HI351WriteCmosSensor(0x10, 0x84);
            
            ///////////////////////////////////////////
            // 30 Page DMA address set
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x30); //DMA
        HI351WriteCmosSensor(0x7c, 0x2c); //Extra str
        HI351WriteCmosSensor(0x7d, 0xce);
        HI351WriteCmosSensor(0x7e, 0x2d); //Extra end
        HI351WriteCmosSensor(0x7f, 0xbb);
        HI351WriteCmosSensor(0x80, 0x24); //Outdoor str
        HI351WriteCmosSensor(0x81, 0x70);
        HI351WriteCmosSensor(0x82, 0x27); //Outdoor end
        HI351WriteCmosSensor(0x83, 0x39);
        HI351WriteCmosSensor(0x84, 0x21); //Indoor str
        HI351WriteCmosSensor(0x85, 0xa6);
        HI351WriteCmosSensor(0x86, 0x24); //Indoor end
        HI351WriteCmosSensor(0x87, 0x6f);
        HI351WriteCmosSensor(0x88, 0x27); //Dark1 str
        HI351WriteCmosSensor(0x89, 0x3a);
        HI351WriteCmosSensor(0x8a, 0x2a); //Dark1 end
        HI351WriteCmosSensor(0x8b, 0x03);
        HI351WriteCmosSensor(0x8c, 0x2a); //Dark2 str
        HI351WriteCmosSensor(0x8d, 0x04);
        HI351WriteCmosSensor(0x8e, 0x2c); //Dark2 end
        HI351WriteCmosSensor(0x8f, 0xcd);
            
            
            
            ///////////////////////////////////////////
            // CD Page (Color ratio)
            ///////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0xCD);
        HI351WriteCmosSensor(0x10, 0x38); //ColorRatio disable ¨ö?¢¥
            
            
        HI351WriteCmosSensor(0x03, 0xc9); //AWB Start Point
        HI351WriteCmosSensor(0x2a, 0x00);
        HI351WriteCmosSensor(0x2b, 0xb2);
        HI351WriteCmosSensor(0x2c, 0x00);
        HI351WriteCmosSensor(0x2d, 0x82);
        HI351WriteCmosSensor(0x2e, 0x00);
        HI351WriteCmosSensor(0x2f, 0xb2);
        HI351WriteCmosSensor(0x30, 0x00);
        HI351WriteCmosSensor(0x31, 0x82);
            
        HI351WriteCmosSensor(0x03, 0xc5); //AWB en
        HI351WriteCmosSensor(0x10, 0xb1);
            
        HI351WriteCmosSensor(0x03, 0xcf); //Adative en
        HI351WriteCmosSensor(0x10, 0xaf); // STEVE 8f -) af ON :Ytar, Gam, CCM, Sat, LSC, MCMC / OFF: Yoffs, Contrast
            
            
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x01, 0xf0); //sleep off
            
        HI351WriteCmosSensor(0x03, 0xC0);
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x32, 0x01); //DMA on
            
            //////////////////////////////////////////////
            // Delay
            //////////////////////////////////////////////
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x00, 0x00);
            
        //END
        //[END] 
            
}

void HI351InitPara(void)
{
	kal_uint32  EXP100, EXP120  ; 
	HI351Sensor.NightMode = KAL_FALSE;
	HI351Sensor.VidoeMode = KAL_FALSE; 
	HI351Sensor.IsPreview = KAL_FALSE;   
	HI351Sensor.ZoomFactor = 0;
	HI351Sensor.Banding = AE_FLICKER_MODE_50HZ;
	//HI351Sensor.PvShutter = 0x17c40;
	HI351Sensor.MaxFrameRate = HI351_MAX_FPS;
	HI351Sensor.MiniFrameRate = HI351_FPS(10);
	HI351Sensor.PVDummyPixels = PREVIEW_DEFAULT_HBLANK;
	HI351Sensor.PVDummyLines = PREVIEW_DEFAULT_VBLANK;
	HI351Sensor.PvOpClk = 36; // Pclk is 72M
	HI351Sensor.CapOpClk = 36;  
	HI351Sensor.VDOCTL2 = 0x90;
	HI351Sensor.AECTL1 = 0xe1;
	HI351Sensor.AWBCTL1 = 0xb1;
	HI351Sensor.MODEFZY1 = 0xdf;  //p20:0x10 disable AFC

	EXP100 = HI351Sensor.PvOpClk * 1000000  / 100 ; 
	EXP120 = HI351Sensor.PvOpClk * 1000000  / 120 ; 

	HI351WriteCmosSensor(0x30, (EXP100>> 16) & 0xFF);
	HI351WriteCmosSensor(0x31, (EXP100 >> 8) & 0xFF);
	HI351WriteCmosSensor(0x32, EXP100 & 0xFF);  
	HI351WriteCmosSensor(0x33, (EXP120>> 16) & 0xFF);
	HI351WriteCmosSensor(0x34, (EXP120 >> 8) & 0xFF);
	HI351WriteCmosSensor(0x35, EXP120 & 0xFF);  

	HI351SetAeMode(KAL_TRUE);
}

/*************************************************************************
* FUNCTION
*  HI351SetMirror
*
* DESCRIPTION
*  This function mirror, flip or mirror & flip the sensor output image.
*
*  IMPORTANT NOTICE: For some sensor, it need re-set the output order Y1CbY2Cr after
*  mirror or flip.
*
* PARAMETERS
*  1. kal_uint16 : horizontal mirror or vertical flip direction.
*
* RETURNS
*  None
*
*************************************************************************/
static void HI351SetMirror(kal_uint16 ImageMirror)
{
  HI351Sensor.VDOCTL2 &= 0xfc;   
  switch (ImageMirror)
  {
    case IMAGE_H_MIRROR:
      HI351Sensor.VDOCTL2 |= 0x01;
      break;
    case IMAGE_V_MIRROR:
      HI351Sensor.VDOCTL2 |= 0x02; 
      break;
    case IMAGE_HV_MIRROR:
      HI351Sensor.VDOCTL2 |= 0x00; // inverse direction            
      break;
    case IMAGE_NORMAL:
    default:      
      HI351Sensor.VDOCTL2 |= 0x03; // inverse direction
  }
  HI351SetPage(0x00);
  HI351WriteCmosSensor(0x11,HI351Sensor.VDOCTL2);  

  SENSORDB("[HI351]Read Set Mirror :%x;\n",HI351ReadCmosSensor(0x11));
}

static void HI351SetAeMode(kal_bool AeEnable)
{
  SENSORDB("[HI351]HI351SetAeMode AeEnable:%d;\n",AeEnable);

  if (AeEnable == KAL_TRUE)
  {
    HI351Sensor.AECTL1 |= 0x80;
  }
  else
  {
    HI351Sensor.AECTL1 &= (~0x80);
  }
  HI351SetPage(0xc4);
  HI351WriteCmosSensor(0x10,HI351Sensor.AECTL1);  
  SENSORDB("[HI351]HI351Sensor.AECTL1:%d;\n",HI351Sensor.AECTL1);
  SENSORDB("[HI351]ReadAECTL1:%d;\n",HI351ReadCmosSensor(0x10));
  
}


static void HI351SetAwbMode(kal_bool AwbEnable)
{
  SENSORDB("[HI351]HI351SetAwbMode AwbEnable:%d;\n",AwbEnable);
  if (AwbEnable == KAL_TRUE)
  {
    HI351Sensor.AWBCTL1 |= 0x80;
  }
  else
  {
    HI351Sensor.AWBCTL1 &= (~0x80);
  }
  HI351SetPage(0xc5);
  HI351WriteCmosSensor(0x10,HI351Sensor.AWBCTL1);  
}


/*************************************************************************
* FUNCTION
*  HI351FixFrameRate
*
* DESCRIPTION
*  This function fix the senor frame rate or not.
*
* PARAMETERS
*  1. kal_bool : Enable or disable.
*
* RETURNS
*  None
*
*************************************************************************/
#if 0
static void HI351FixFrameRate(kal_bool Enable)
{
	kal_uint32 MaxShutter;
	kal_uint32 FixShutter;
	kal_uint16 Pixel_Length = HI351Sensor.PVDummyPixels + HI351_PV_PERIOD_PIXEL_NUMS;
	kal_uint16 Frame_length ;

	HI351Sensor.VDOCTL2 &=0xfb; 
	HI351Sensor.VDOCTL2 |= Enable ? 0x4 : 0x0;

	SENSORDB("[HI351]HI351FixFrameRate Enable:%d;\n",Enable);

	HI351SetPage(0x00);
	//HI351WriteCmosSensor(0x01, 0xf1); // Sleep ON

	HI351WriteCmosSensor(0x11,HI351Sensor.VDOCTL2);  

	HI351SetPage(0x20);
	MaxShutter = HI351Sensor.PvOpClk * 1000000 / HI351Sensor.MiniFrameRate * HI351_FPS(1);
	SENSORDB("[HI351]MaxShutter:%d;\n",MaxShutter);
	if (KAL_TRUE == Enable)
	{
		FixShutter = HI351Sensor.PvOpClk * 1000000 / HI351Sensor.MiniFrameRate * HI351_FPS(1)  ;
		SENSORDB("[HI351]FixShutter:%d;\n",FixShutter);
		HI351WriteCmosSensor(0x3C, (FixShutter>>24)&(0xff)); 
		HI351WriteCmosSensor(0x3D, (FixShutter>>16)&(0xff)); 
		HI351WriteCmosSensor(0x3E, (FixShutter>>8)&(0xff)); 
		HI351WriteCmosSensor(0x3F, (FixShutter>>0)&(0xff)); 
	}

	HI351WriteCmosSensor(0x24, (MaxShutter>>24)&(0xff)); 
	HI351WriteCmosSensor(0x25, (MaxShutter>>16)&(0xff)); 
	HI351WriteCmosSensor(0x26, (MaxShutter>>8)&(0xff)); 
	HI351WriteCmosSensor(0x27, (MaxShutter>>0)&(0xff)); 

    HI351SetAeMode(KAL_TRUE);   //MAXshutter take effect should enable AE again
}
#endif

static void HI351WriteDummy(kal_uint16 Pixels, kal_uint16 Lines)
{
  SENSORDB("[HI351]HI351WriteDummy Pixels: %d; Lines: %d\n",Pixels, Lines);
  HI351SetPage(0x00); 
  HI351WriteCmosSensor(0x50, (Pixels>>8)&0xff);
  HI351WriteCmosSensor(0x51, (Pixels>>0)&0xff);
  HI351WriteCmosSensor(0x52, (Lines>>8)&0xff);
  HI351WriteCmosSensor(0x53, (Lines>>0)&0xff); 
}   /*  HI351WriteDummy    */

/*************************************************************************
* FUNCTION
* HI351NightMode
*
* DESCRIPTION
* This function night mode of HI351.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HI351NightMode(kal_bool Enable)
{
	SENSORDB("[HI351]HI351NightMode Enable:%d;\n",Enable);

	if(HI351Sensor.VidoeMode == KAL_TRUE)
		return ;

	if (Enable)
	{
		HI351Sensor.MiniFrameRate = HI351_FPS(5);
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x01, 0xf1); //Sleep on

        HI351WriteCmosSensor(0x03, 0x30); //DMA & Adaptive Off
        HI351WriteCmosSensor(0x36, 0xa3);
        //delay 10ms
        HI351WriteCmosSensor(0x03, 0xFE);
        mdelay(10);

        HI351WriteCmosSensor(0x03, 0xc4);
        HI351WriteCmosSensor(0x10, 0x6E); //AE Off

        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0x00); //AWB off

        //FPS Auto
        HI351WriteCmosSensor(0x03, 0x20);
        HI351WriteCmosSensor(0x24, 0x00); //EXP Max 5.00 fps 
        HI351WriteCmosSensor(0x25, 0x6d); 
        HI351WriteCmosSensor(0x26, 0xd1); 
        HI351WriteCmosSensor(0x27, 0xc0);  
        		 

        //AE On
        HI351WriteCmosSensor(0x03, 0xc4);
        HI351WriteCmosSensor(0x10, 0xef); //ae on & reset

        HI351WriteCmosSensor(0x03, 0x00);
        //HI351WriteCmosSensor(0xFE, 0x14); //Delay 20ms
        mdelay(20);

        //AWB On
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0xb1);



        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x01, 0xf0); //sleep off

        HI351WriteCmosSensor(0x03, 0xcf); //Adaptive On
        HI351WriteCmosSensor(0x10, 0xaf);

        HI351WriteCmosSensor(0x03, 0xc0); 
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x32, 0x01); //DMA On
	}
	else
	{
		HI351Sensor.MiniFrameRate = HI351_FPS(10); 
        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x01, 0xf1); //Sleep on
                           
        HI351WriteCmosSensor(0x03, 0x30); //DMA&Adaptive Off                    
        HI351WriteCmosSensor(0x36, 0xa3);                                       
                             
        HI351WriteCmosSensor(0x03, 0xc4); //AE off
        HI351WriteCmosSensor(0x10, 0x60);
        HI351WriteCmosSensor(0x03, 0xc5); //AWB off
        HI351WriteCmosSensor(0x10, 0x30);

        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x10, 0x13); //Sub1/2 + Pre2
        HI351WriteCmosSensor(0x11, 0x80); //Fix Frame Off, XY Flip
        HI351WriteCmosSensor(0x13, 0x80); //Fix AE Set Off
        HI351WriteCmosSensor(0x14, 0x70); // for Pre2mode

        HI351WriteCmosSensor(0x03, 0xFE);             
        //HI351WriteCmosSensor(0xFE, 0x0A); //Delay 10ms
        mdelay(10);

        HI351WriteCmosSensor(0x03, 0x15);  //Shading
        HI351WriteCmosSensor(0x10, 0x81);
        HI351WriteCmosSensor(0x20, 0x04);  //Shading Width 1024 (pre2)
        HI351WriteCmosSensor(0x21, 0x00);
        HI351WriteCmosSensor(0x22, 0x03);  //Shading Height 768
        HI351WriteCmosSensor(0x23, 0x00);

        HI351WriteCmosSensor(0x03, 0x30);
        HI351WriteCmosSensor(0x36, 0x28); //Preview set

        HI351WriteCmosSensor(0x03, 0x40);
        HI351WriteCmosSensor(0x10, 0x01); // JPEG Pclk inversion

        HI351WriteCmosSensor(0x03, 0xFE);
        mdelay(10);

        HI351WriteCmosSensor(0x03, 0x11);
        HI351WriteCmosSensor(0xf0, 0x0d); // STEVE Dark mode for Sawtooth


        HI351WriteCmosSensor(0x03, 0xc4); //AE en
        HI351WriteCmosSensor(0x10, 0xe1);

        HI351WriteCmosSensor(0x03, 0xFE);
        mdelay(10);

        HI351WriteCmosSensor(0x03, 0xc5); //AWB en
        HI351WriteCmosSensor(0x10, 0xb1);

        HI351WriteCmosSensor(0x03, 0x00);
        HI351WriteCmosSensor(0x01, 0xf0); //sleep off

        HI351WriteCmosSensor(0x03, 0xcf); //Adaptive On                         
        HI351WriteCmosSensor(0x10, 0xaf);                                       
        HI351WriteCmosSensor(0x03, 0xc0); 
        HI351WriteCmosSensor(0x33, 0x00);
        HI351WriteCmosSensor(0x32, 0x01); //DMA On

	}
	//HI351FixFrameRate(KAL_FALSE); 
	HI351SetPage(0x00);
	HI351WriteCmosSensor(0x11,HI351Sensor.VDOCTL2);  
    
    SENSORDB("[HI351]HI351NightMode End VDOCTL2:%x;\n",HI351Sensor.VDOCTL2);
} /* HI351NightMode */


/*************************************************************************
* FUNCTION
* HI351Open
*
* DESCRIPTION
* this function initialize the registers of CMOS sensor
*
* PARAMETERS
* none
*
* RETURNS
*  none
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI351Open(void)
{
  kal_uint16 SensorId = 0;
  //1 software reset sensor and wait (to sensor)
  HI351SetPage(0x00);
  HI351WriteCmosSensor(0x01,0xf1);
  HI351WriteCmosSensor(0x01,0xf3);
  HI351WriteCmosSensor(0x01,0xf1);

  SensorId = HI351ReadCmosSensor(0x04);
  Sleep(3);
  SENSORDB("[HI351]HI351Open: Sensor ID %x\n",SensorId);
  
  if(SensorId != HI351_SENSOR_ID)
  {
    return ERROR_SENSOR_CONNECT_FAIL;
  }
  HI351InitSetting();
  HI351InitPara();
  return ERROR_NONE;

}
/* HI351Open() */

/*************************************************************************
* FUNCTION
*   HI351GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID 
*
* PARAMETERS
*   *sensorID : return the sensor ID 
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI351GetSensorID(UINT32 *sensorID) 
{
	//1 software reset sensor and wait (to sensor)
	HI351SetPage(0x00);
	HI351WriteCmosSensor(0x01,0xf1);
	HI351WriteCmosSensor(0x01,0xf3);
	HI351WriteCmosSensor(0x01,0xf1);
	
	*sensorID = HI351ReadCmosSensor(0x04);
	Sleep(3);
	SENSORDB("[HI351]HI351GetSensorID: Sensor ID %x\n",*sensorID);
	
	if(*sensorID != HI351_SENSOR_ID)
	{
	  return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
* HI351Close
*
* DESCRIPTION
* This function is to turn off sensor module power.
*
* PARAMETERS
* None
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI351Close(void)
{
  return ERROR_NONE;
} /* HI351Close() */

void HI351PreviewSetting(void)
{
    HI351WriteCmosSensor(0x03, 0x00);
    HI351WriteCmosSensor(0x01, 0xf1); //Sleep on
                                                     
    HI351WriteCmosSensor(0x03, 0x30); //DMA&Adaptive Off                    
    HI351WriteCmosSensor(0x36, 0xa3);                                       
                                                       
    HI351WriteCmosSensor(0x03, 0xc4); //AE off
    HI351WriteCmosSensor(0x10, 0x60);

    HI351WriteCmosSensor(0x03, 0xc5); //AWB off
    HI351WriteCmosSensor(0x10, 0x30);

    HI351WriteCmosSensor(0x03, 0x00);
    HI351WriteCmosSensor(0x10, 0x13); //Sub1/2 + Pre2    
    
    HI351WriteCmosSensor(0x13, 0x80); //Fix AE Set Off
    HI351WriteCmosSensor(0x14, 0x70); // for Pre2mode

    HI351WriteCmosSensor(0x03, 0xFE);             
    mdelay(10);

    HI351WriteCmosSensor(0x03, 0x00);
    HI351WriteCmosSensor(0x20, 0x00);
    HI351WriteCmosSensor(0x21, 0x01); //preview row start set.

    HI351WriteCmosSensor(0x03, 0x15);  //Shading
    HI351WriteCmosSensor(0x10, 0x81);
    HI351WriteCmosSensor(0x20, 0x04);  //Shading Width 1024 (pre2)
    HI351WriteCmosSensor(0x21, 0x00);
    HI351WriteCmosSensor(0x22, 0x03);  //Shading Height 768
    HI351WriteCmosSensor(0x23, 0x00);

    HI351WriteDummy(HI351Sensor.PVDummyPixels,HI351Sensor.PVDummyLines);

    HI351WriteCmosSensor(0x03, 0x30);
    HI351WriteCmosSensor(0x36, 0x28); //Preview set

    HI351WriteCmosSensor(0x03, 0xFE);
    mdelay(10);

    HI351WriteCmosSensor(0x03, 0x11);
    HI351WriteCmosSensor(0xf0, 0x0d); // STEVE Dark mode for Sawtooth

    HI351WriteCmosSensor(0x03, 0xc4); //AE en
    HI351WriteCmosSensor(0x10, 0xe1);

    HI351WriteCmosSensor(0x03, 0xFE);
    mdelay(10);

    HI351WriteCmosSensor(0x03, 0xc5); //AWB en
    HI351WriteCmosSensor(0x10, 0xb1);

    HI351WriteCmosSensor(0x03, 0x00);
    HI351WriteCmosSensor(0x01, 0xf0); //sleep off

    HI351WriteCmosSensor(0x03, 0xcf); //Adaptive On                         
    HI351WriteCmosSensor(0x10, 0xaf);                                       

    HI351WriteCmosSensor(0x03, 0xc0); 
    HI351WriteCmosSensor(0x33, 0x00);
    HI351WriteCmosSensor(0x32, 0x01); //DMA On
    HI351WriteCmosSensor(0x00, 0x00);

}

void HI351CaptureSetting(void)
{
    HI351WriteCmosSensor(0x03, 0x00);
    HI351WriteCmosSensor(0x01, 0xf1); //Sleep on
        
    HI351WriteCmosSensor(0x03, 0x30); //DMA&Adaptive Off                    
    HI351WriteCmosSensor(0x36, 0xa3);                                       
                                                           
    HI351WriteCmosSensor(0x03, 0xc4); //AE off
    HI351WriteCmosSensor(0x10, 0x60);
    
    HI351WriteCmosSensor(0x03, 0xc5); //AWB off
    HI351WriteCmosSensor(0x10, 0x30);
    
	HI351WriteDummy(HI351Sensor.CPDummyPixels,HI351Sensor.CPDummyLines);
    
    HI351WriteCmosSensor(0x03, 0x00);
    HI351WriteCmosSensor(0x14, 0x20); // for Full mode
    HI351WriteCmosSensor(0x10, 0x00); //Full
    
    HI351WriteCmosSensor(0x03, 0xFE);                        
        
    mdelay(10);
    HI351WriteCmosSensor(0x03, 0x00);
    HI351WriteCmosSensor(0x20, 0x00);
    HI351WriteCmosSensor(0x21, 0x03); //preview row start set.
    
    HI351WriteCmosSensor(0x03, 0x15); //Shading
    HI351WriteCmosSensor(0x10, 0x83); // 00 -> 83 LSC ON
    HI351WriteCmosSensor(0x20, 0x07);
    HI351WriteCmosSensor(0x21, 0xf8);
    HI351WriteCmosSensor(0x22, 0x05);
    HI351WriteCmosSensor(0x23, 0xf8);
    
    HI351WriteCmosSensor(0x03, 0x11);
    HI351WriteCmosSensor(0xf0, 0x00); // STEVE Dark mode for Sawtooth
        
    HI351WriteCmosSensor(0x03, 0x30);
    HI351WriteCmosSensor(0x36, 0x29); //Capture
    
    HI351WriteCmosSensor(0x03, 0xFE);
        
    mdelay(20);    
    
    HI351WriteCmosSensor(0x03,0x00);
    HI351WriteCmosSensor(0x01,0xf0); //sleep off
    
    HI351WriteCmosSensor(0x03,0xcf); //Adaptive On                         
    HI351WriteCmosSensor(0x10,0xaf);                                       
        
    HI351WriteCmosSensor(0x03,0xc0); 
    HI351WriteCmosSensor(0x33,0x00);
    HI351WriteCmosSensor(0x32,0x01); //DMA On

	
}
/*************************************************************************
* FUNCTION
* HI351Preview
*
* DESCRIPTION
* This function start the sensor preview.
*
* PARAMETERS
* *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI351Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	SENSORDB("[HI351]HI351Preview   \n");

	HI351Sensor.VidoeMode = KAL_FALSE;

	HI351Sensor.PVDummyPixels = PREVIEW_DEFAULT_HBLANK;
	HI351Sensor.PVDummyLines  = PREVIEW_DEFAULT_VBLANK;

	HI351PreviewSetting();

    HI351SetMirror(sensor_config_data->SensorImageMirror);

	return ERROR_NONE;
}/* HI351Preview() */

UINT32 HI351Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint16 CapDummyPixel  = 240;
	kal_uint32 Shutter = 0, Preview_line = 0;

	SENSORDB("\n\n\n\n\n\n");
	SENSORDB("[HI351]HI351Capture!!!!!!!!!!!!!\n");
	SENSORDB("[HI351]Image Target Width: %d; Height: %d\n",image_window->ImageTargetWidth, image_window->ImageTargetHeight);

	HI351Sensor.IsPreview = KAL_FALSE;  

	// 2048 *1536 capture have differnt shading parameter from preview.
	HI351CaptureSetting();

	mdelay(5);
	return ERROR_NONE;
} /* HI351Capture() */

UINT32 HI351GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
  pSensorResolution->SensorFullWidth = HI351_FULL_WIDTH;
  pSensorResolution->SensorFullHeight = HI351_FULL_HEIGHT;
  pSensorResolution->SensorPreviewWidth = HI351_PV_WIDTH;
  pSensorResolution->SensorPreviewHeight = HI351_PV_HEIGHT;
  return ERROR_NONE;
} /* HI351GetResolution() */

UINT32 HI351GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                    MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
  pSensorInfo->SensorPreviewResolutionX=HI351_PV_WIDTH;
  pSensorInfo->SensorPreviewResolutionY=HI351_PV_HEIGHT;
  pSensorInfo->SensorFullResolutionX=HI351_FULL_WIDTH;
  pSensorInfo->SensorFullResolutionY=HI351_FULL_HEIGHT;

  pSensorInfo->SensorCameraPreviewFrameRate=30;
  pSensorInfo->SensorVideoFrameRate=30;
  pSensorInfo->SensorStillCaptureFrameRate=10;
  pSensorInfo->SensorWebCamCaptureFrameRate=15;
  pSensorInfo->SensorResetActiveHigh=FALSE;
  pSensorInfo->SensorResetDelayCount=1;
  pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_YUYV;
  pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorInterruptDelayLines = 1;
  pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_PARALLEL;

  pSensorInfo->CaptureDelayFrame = 2; 
  pSensorInfo->PreviewDelayFrame = 3;  //don't change delay frame, otherwise black screen
  pSensorInfo->VideoDelayFrame = 10; 
  pSensorInfo->SensorMasterClockSwitch = 0; 
  pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA; 

  switch (ScenarioId)
  {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
    case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
    default:
      pSensorInfo->SensorClockFreq=24;
      pSensorInfo->SensorClockDividCount=3;
      pSensorInfo->SensorClockRisingCount=0;
      pSensorInfo->SensorClockFallingCount=2;
      pSensorInfo->SensorPixelClockCount=3;
      pSensorInfo->SensorDataLatchCount=2;
      pSensorInfo->SensorGrabStartX = HI351_GRAB_START_X; 
      pSensorInfo->SensorGrabStartY = HI351_GRAB_START_Y;
      break;
  }
  return ERROR_NONE;
} /* HI351GetInfo() */


UINT32 HI351Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
  switch (ScenarioId)
  {
  case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
  case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
  case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
    HI351Preview(pImageWindow, pSensorConfigData);
    break;
  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
    HI351Capture(pImageWindow, pSensorConfigData);
    break;
  default:
    break; 
  }
  return TRUE;
} /* HI351Control() */


BOOL HI351SetWb(UINT16 Para)
{
  SENSORDB("[HI351]HI351SetWb Para:%d;\n",Para);
  switch (Para)
  {
    case AWB_MODE_OFF:
      HI351SetAwbMode(KAL_FALSE);
      break;                     
    case AWB_MODE_AUTO:
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0x30); //AWB Off
        HI351WriteCmosSensor(0x03, 0xc6);
        HI351WriteCmosSensor(0x18, 0x40); //bInRgainMin_a00_n00 
        HI351WriteCmosSensor(0x19, 0xf0); //bInRgainMax_a00_n00 
        HI351WriteCmosSensor(0x1a, 0x40); //bInBgainMin_a00_n00 
        HI351WriteCmosSensor(0x1b, 0xf0); //bInBgainMax_a00_n00 
        HI351WriteCmosSensor(0xb9, 0x50); //bOutRgainMin_a00_n00                              
        HI351WriteCmosSensor(0xba, 0xf0); //bOutRgainMax_a00_n00    
        HI351WriteCmosSensor(0xbb, 0x40); //bOutBgainMin_a00_n00    
        HI351WriteCmosSensor(0xbc, 0x80); //bOutBgainMax_a00_n00                              
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0xb1); //AWB On

      break;
    case AWB_MODE_CLOUDY_DAYLIGHT: //cloudy
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0x30);
        HI351WriteCmosSensor(0x03, 0xc6);
        HI351WriteCmosSensor(0x18, 0x75); //bInRgainMin_a00_n00                               
        HI351WriteCmosSensor(0x19, 0x8F); //bInRgainMax_a00_n00                               

        HI351WriteCmosSensor(0x1a, 0x40); //bInBgainMin_a00_n00 
        HI351WriteCmosSensor(0x1b, 0x56); //bInBgainMax_a00_n00                               

        HI351WriteCmosSensor(0xb9, 0x75); //bOutRgainMin_a00_n00                              
        HI351WriteCmosSensor(0xba, 0x8F); //bOutRgainMax_a00_n00                              

        HI351WriteCmosSensor(0xbb, 0x40); //bOutBgainMin_a00_n00  
        HI351WriteCmosSensor(0xbc, 0x56); //bOutBgainMax_a00_n00                              
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0xb1);
      break;
    case AWB_MODE_DAYLIGHT: //sunny
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0x30); //AWB Off
        HI351WriteCmosSensor(0x03, 0xc6);
        HI351WriteCmosSensor(0x18, 0x4B); //bInRgainMin_a00_n00                               
        HI351WriteCmosSensor(0x19, 0x6A);//bInRgainMax_a00_n00                                

        HI351WriteCmosSensor(0x1a, 0x5d); //bInBgainMin_a00_n00                               
        HI351WriteCmosSensor(0x1b, 0x78); //bInBgainMax_a00_n00                               

        HI351WriteCmosSensor(0xb9, 0x5d); //bOutRgainMin_a00_n00                              
        HI351WriteCmosSensor(0xba, 0x6A); //bOutRgainMax_a00_n00                              

        HI351WriteCmosSensor(0xbb, 0x62); //bOutBgainMin_a00_n00                              
        HI351WriteCmosSensor(0xbc, 0x78); //bOutBgainMax_a00_n00                              
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0xb1); //AWB On

      break;
    case AWB_MODE_INCANDESCENT: //office
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0x30); //AWB Off
        HI351WriteCmosSensor(0x03, 0xc6);
        HI351WriteCmosSensor(0x18, 0x3C); //bInRgainMin_a00_n00                               
        HI351WriteCmosSensor(0x19, 0x59); //bInRgainMax_a00_n00                               

        HI351WriteCmosSensor(0x1a, 0x90); //bInBgainMin_a00_n00                               
        HI351WriteCmosSensor(0x1b, 0xAC); //bInBgainMax_a00_n00                               

        HI351WriteCmosSensor(0xb9, 0x3C); //bOutRgainMin_a00_n00                              
        HI351WriteCmosSensor(0xba, 0x59); //bOutRgainMax_a00_n00                              

        HI351WriteCmosSensor(0xbb, 0x90); //bOutBgainMin_a00_n00                              
        HI351WriteCmosSensor(0xbc, 0xAC); //bOutBgainMax_a00_n00                              
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0xb1); //AWB On
      break;
    case AWB_MODE_TUNGSTEN: //home
		HI351SetAwbMode(KAL_FALSE);
		HI351SetPage(0xc6);
		HI351WriteCmosSensor(0x18, 0x25);	 //Indoor R Gain Min
		HI351WriteCmosSensor(0x19, 0x35);	 //Indoor R Gain Max
		HI351WriteCmosSensor(0x1a, 0x85);	 //Indoor B Gain Min
		HI351WriteCmosSensor(0x1b, 0xb0);	 //Indoor B Gain Max

		HI351WriteCmosSensor(0xb9, 0x25);	 //outdoor R Gain Min
		HI351WriteCmosSensor(0xba, 0x25);	 //outdoor R Gain Max
		HI351WriteCmosSensor(0xbb, 0x85);	 //outdoor B Gain Min
		HI351WriteCmosSensor(0xbc, 0xb0);	 //outdoor B Gain Max

		HI351SetAwbMode(KAL_TRUE);
     
      break;
    case AWB_MODE_FLUORESCENT:
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0x30); //AWB Off
        HI351WriteCmosSensor(0x03, 0xc6);
        HI351WriteCmosSensor(0x18, 0x43); //bInRgainMin_a00_n00                               
        HI351WriteCmosSensor(0x19, 0x60); //bInRgainMax_a00_n00                               

        HI351WriteCmosSensor(0x1a, 0x69); //bInBgainMin_a00_n00                               
        HI351WriteCmosSensor(0x1b, 0x90); //bInBgainMax_a00_n00                               

        HI351WriteCmosSensor(0xb9, 0x43); //bOutRgainMin_a00_n00                              
        HI351WriteCmosSensor(0xba, 0x60); //bOutRgainMax_a00_n00                              

        HI351WriteCmosSensor(0xbb, 0x69); //bOutBgainMin_a00_n00                              
        HI351WriteCmosSensor(0xbc, 0x90); //bOutBgainMax_a00_n00                              
        HI351WriteCmosSensor(0x03, 0xc5);
        HI351WriteCmosSensor(0x10, 0xb1); //AWB On
    default:
      return KAL_FALSE;
  }
  return KAL_TRUE;      
} /* HI351SetWb */

BOOL HI351SetEffect(UINT16 Para)
{
	SENSORDB("[HI351]HI351SetEffect Para:%d;\n",Para);

	HI351SetPage(0x10);
	switch (Para)
	{
		case MEFFECT_OFF:
            HI351WriteCmosSensor(0x11, 0x03);
            HI351WriteCmosSensor(0x12, 0xf0);
            HI351WriteCmosSensor(0x13, 0x0a);
            HI351WriteCmosSensor(0x42, 0x00);
            HI351WriteCmosSensor(0x43, 0x00);
            HI351WriteCmosSensor(0x44, 0x80);
            HI351WriteCmosSensor(0x45, 0x80);

			break;
		case MEFFECT_SEPIA:
			HI351WriteCmosSensor(0x11, 0x03);
			HI351WriteCmosSensor(0x12, 0x13);
			HI351WriteCmosSensor(0x13, 0x0a);
			HI351WriteCmosSensor(0x42, 0x00);
			HI351WriteCmosSensor(0x43, 0x00);
			HI351WriteCmosSensor(0x44, 0x60);
			HI351WriteCmosSensor(0x45, 0xa3);

			break;
		case MEFFECT_NEGATIVE://----datasheet
			HI351WriteCmosSensor(0x11, 0x03);
			HI351WriteCmosSensor(0x12, 0x18);
			HI351WriteCmosSensor(0x13, 0x0a);
			HI351WriteCmosSensor(0x42, 0x00);
			HI351WriteCmosSensor(0x43, 0x00);
			HI351WriteCmosSensor(0x44, 0x80);
			HI351WriteCmosSensor(0x45, 0x80);

			break;
		case MEFFECT_SEPIAGREEN://----datasheet aqua
			HI351WriteCmosSensor(0x11, 0x03);	
			HI351WriteCmosSensor(0x12, 0x23);	
			HI351WriteCmosSensor(0x13, 0x00);  
			HI351WriteCmosSensor(0x40, 0x00);
			HI351WriteCmosSensor(0x44, 0x80);	
			HI351WriteCmosSensor(0x45, 0x04);	
			HI351WriteCmosSensor(0x47, 0x7f);  
			HI351SetPage(0x13);
			HI351WriteCmosSensor(0x20, 0x07);
			HI351WriteCmosSensor(0x21, 0x07);

			break;
		case MEFFECT_SEPIABLUE:
			HI351WriteCmosSensor(0x11, 0x03); //solar off        
			HI351WriteCmosSensor(0x12, 0xf0); //constant off  
			HI351WriteCmosSensor(0x13, 0x0a);   
			HI351WriteCmosSensor(0x42, 0x1a); //cb_offset        
			HI351WriteCmosSensor(0x43, 0xc0); //cr_offset        
			HI351WriteCmosSensor(0x44, 0x80); //cb_constant      
			HI351WriteCmosSensor(0x45, 0x80); //cr_constant 
			break;
		case MEFFECT_MONO: //----datasheet black & white
            HI351WriteCmosSensor(0x11, 0x03);
            HI351WriteCmosSensor(0x12, 0x33);
            HI351WriteCmosSensor(0x13, 0x00);
            HI351WriteCmosSensor(0x40, 0x00);
            HI351WriteCmosSensor(0x44, 0x80);
            HI351WriteCmosSensor(0x45, 0x80);
		    break;
		default:
		return KAL_FALSE;
	}
	return KAL_TRUE;

} /* HI351SetEffect */

BOOL HI351SetBanding(UINT16 Para)
{
  SENSORDB("[HI351]HI351SetBanding Para:%d;\n",Para);
  HI351Sensor.Banding = Para;
  
  HI351SetPage(0x20);  
  if (HI351Sensor.Banding == AE_FLICKER_MODE_50HZ) 
  {
    HI351Sensor.MODEFZY1 |= 0x10;
  }
  else
	{
	    HI351Sensor.MODEFZY1 &= (~0x10); 
	}
  
  HI351WriteCmosSensor(0x10,HI351Sensor.MODEFZY1);
  return TRUE;
} /* HI351SetBanding */

BOOL HI351SetExposure(UINT16 Para)
{
	SENSORDB("[HI351]HI351SetExposure Para:%d;\n",Para);
	HI351SetPage(0x10);  
	HI351WriteCmosSensor(0x13,0x0A);

	switch (Para)
	{
		case AE_EV_COMP_n13:              /* EV -2 */
			HI351WriteCmosSensor(0x4A,0x30);
			break;
		case AE_EV_COMP_n10:              /* EV -1.5 */
			HI351WriteCmosSensor(0x4A,0x44);
			break;
		case AE_EV_COMP_n07:              /* EV -1 */
			HI351WriteCmosSensor(0x4A,0x58);
			break;
		case AE_EV_COMP_n03:              /* EV -0.5 */
			HI351WriteCmosSensor(0x4A,0x6c);
			break;
		case AE_EV_COMP_00:                /* EV 0 */
			HI351WriteCmosSensor(0x4A,0x80);
			break;
		case AE_EV_COMP_03:              /* EV +0.5 */
			HI351WriteCmosSensor(0x4A,0x90);
			break;
		case AE_EV_COMP_07:              /* EV +1 */
			HI351WriteCmosSensor(0x4A,0xA0);
			break;
		case AE_EV_COMP_10:              /* EV +1.5 */
			HI351WriteCmosSensor(0x4A,0xB0);
			break;
		case AE_EV_COMP_13:              /* EV +2 */
			HI351WriteCmosSensor(0x4A,0xC0);
			break;
		default:
		return KAL_FALSE;
	}
	return KAL_TRUE;
} /* HI351SetExposure */

UINT32 HI351YUVSensorSetting(FEATURE_ID Cmd, UINT32 Para)
{
  switch (Cmd) {
    case FID_SCENE_MODE:
      if (Para == SCENE_MODE_OFF)
      {
        HI351NightMode(KAL_FALSE); 
      }
      else if (Para == SCENE_MODE_NIGHTSCENE)
      {
        HI351NightMode(KAL_TRUE); 
      }  
      break; 
    case FID_AWB_MODE:
      HI351SetWb(Para);
      break;
    case FID_COLOR_EFFECT:
      HI351SetEffect(Para);
      break;
    case FID_AE_EV:
      HI351SetExposure(Para);
      break;
    case FID_AE_FLICKER:
      HI351SetBanding(Para);
      break;
    case FID_AE_SCENE_MODE: 
      if (Para == AE_MODE_OFF) 
      {
        HI351SetAeMode(KAL_FALSE);
      }
      else 
      {
        HI351SetAeMode(KAL_TRUE);
      }
      break; 
    case FID_ZOOM_FACTOR:
      SENSORDB("[HI351]ZoomFactor :%d;\n",Para);
      HI351Sensor.ZoomFactor = Para;
      break;
    default:
      break;
  }
  return TRUE;
}   /* HI351YUVSensorSetting */

UINT32 HI351YUVSetVideoMode(UINT16 FrameRate)
{
  kal_uint32 LineLength = HI351_PV_PERIOD_PIXEL_NUMS + HI351Sensor.PVDummyPixels;

 
  SENSORDB("[HI351]HI351YUVSetVideoMode FrameRate:%d;\n",FrameRate);
  //if (FrameRate * HI351_FRAME_RATE_UNIT > HI351_MAX_FPS)
  //  return -1;

  /* to fix VSYNC, to fix frame rate */
  if (FrameRate == 30)
  {
    HI351Sensor.MiniFrameRate = HI351_FPS(30);  
	
	HI351SetPage(0x30); 
	HI351WriteCmosSensor(0x36,0xa3); //dma & adaptive off
	HI351SetPage(0x00);
	HI351WriteCmosSensor(0x01,0xf1);
	HI351SetAeMode(KAL_FALSE);  
	HI351Sensor.VDOCTL2 &=0xfb; 
	HI351Sensor.VDOCTL2 |= 0x4 ;
		
	HI351SetPage(0x00);
	HI351WriteCmosSensor(0x11,HI351Sensor.VDOCTL2);  
	
	HI351SetPage(0x11);
	HI351WriteCmosSensor(0xf0, 0x00);
	
	HI351SetPage(0x20); 
	HI351WriteCmosSensor(0x20,0x00);  //exp max
	HI351WriteCmosSensor(0x21,0x11);
	HI351WriteCmosSensor(0x22,0xda);
	HI351WriteCmosSensor(0x23,0x50);
	HI351WriteCmosSensor(0x24,0x00);  //exp max
	HI351WriteCmosSensor(0x25,0x11);
	HI351WriteCmosSensor(0x26,0xda);
	HI351WriteCmosSensor(0x27,0x50);

	HI351WriteCmosSensor(0x28,0x00);  //exp max
	HI351WriteCmosSensor(0x29,0x05);
	HI351WriteCmosSensor(0x2a,0xdc);
	
	HI351WriteCmosSensor(0x30,0x05);   //EXP 100
	HI351WriteCmosSensor(0x31,0xf3);  
	HI351WriteCmosSensor(0x32,0x70);
	HI351WriteCmosSensor(0x33,0x04);   //EXP 120
	HI351WriteCmosSensor(0x34,0xf1);
	HI351WriteCmosSensor(0x35,0xa0);
	HI351WriteCmosSensor(0x3C,0x00);   //EXP fix 30.94fps
	HI351WriteCmosSensor(0x3D,0x13);   
	HI351WriteCmosSensor(0x3e,0xd2);
	HI351WriteCmosSensor(0x3F,0x38);

	HI351WriteCmosSensor(0x36,0x00);   //EXP uint
	HI351WriteCmosSensor(0x37,0x05);   
	HI351WriteCmosSensor(0x38,0xdc);

	//PCON IDLE
		HI351SetPage(0x30);
	HI351WriteCmosSensor(0x24,0x02);
	HI351SetPage(0xc0);
//	HI351WriteCmosSensor(0x03,0xc0);
	HI351WriteCmosSensor(0xe1,0x80);	// PCON Enable option
	HI351WriteCmosSensor(0xe1,0x80);	// PCON MODE ON
	HI351SetPage(0xCF);
//	HI351WriteCmosSensor(0x03,0xcf);
	HI351WriteCmosSensor(0x13,0x00);	//Y_LUM_MAX 40fps, AG 0xA0
	HI351WriteCmosSensor(0x14,0x4d);
	HI351WriteCmosSensor(0x15,0x3f);
	HI351WriteCmosSensor(0x16,0x64);
	HI351WriteCmosSensor(0x17,0x00);	//Y_LUM_middle1  62.5fps, AG 0x4c
	HI351WriteCmosSensor(0x18,0x14);
	HI351WriteCmosSensor(0x19,0xe2);
	HI351WriteCmosSensor(0x1a,0xae);
	HI351SetPage(0xC0);
//	HI351WriteCmosSensor(0x03,0xc0);
	HI351WriteCmosSensor(0xe1,0x00);	// PCON Enable option
	HI351SetPage(0x30);
//	HI351WriteCmosSensor(0x03,0x30);
	HI351WriteCmosSensor(0x25,0x0e); // PCON IIC ON
	HI351WriteCmosSensor(0x25,0x1e); // PCON IIC OFF
	HI351WriteCmosSensor(0x24,0x00);
	//PCON IDLE END
	HI351SetAeMode(KAL_TRUE);
	
		HI351SetPage(0x30);
	HI351WriteCmosSensor(0x36,0xa2); //dma & adaptive on  
	HI351WriteCmosSensor(0x36,0x28); //preview function
					  
	  HI351SetPage(0x00);
	HI351WriteCmosSensor(0x01,0xf0);	
	
  }
  else if (FrameRate == 15)
  {
    HI351Sensor.MiniFrameRate = HI351_FPS(15); 
	HI351SetPage(0x30); 
	HI351WriteCmosSensor(0x36,0xa3); //dma & adaptive off
	HI351SetPage(0x00);
	HI351WriteCmosSensor(0x01,0xf1);
	HI351SetAeMode(KAL_FALSE);  
	HI351Sensor.VDOCTL2 &=0xfb; 
	HI351Sensor.VDOCTL2 |= 0x4 ;
		
	HI351SetPage(0x00);
	HI351WriteCmosSensor(0x11,HI351Sensor.VDOCTL2);  
	
	HI351SetPage(0x11);
	HI351WriteCmosSensor(0xf0, 0x20);
	
	HI351SetPage(0x20); 
	HI351WriteCmosSensor(0x20, 0x00); //EXP Normal 33.33 fps 
	HI351WriteCmosSensor(0x21, 0x11); 
	HI351WriteCmosSensor(0x22, 0xda); 
	HI351WriteCmosSensor(0x23, 0x50); 
	HI351WriteCmosSensor(0x24, 0x00); //EXP Max 15.00 fps 
	HI351WriteCmosSensor(0x25, 0x27); 
	HI351WriteCmosSensor(0x26, 0xaa); 
	HI351WriteCmosSensor(0x27, 0x4c); 
	HI351WriteCmosSensor(0x28, 0x00); //EXPMin 26000.00 fps
	HI351WriteCmosSensor(0x29, 0x05); 
	HI351WriteCmosSensor(0x2a, 0xdc); 
	HI351WriteCmosSensor(0x30, 0x05); //EXP100 
	HI351WriteCmosSensor(0x31, 0xf3); 
	HI351WriteCmosSensor(0x32, 0x70); 
	HI351WriteCmosSensor(0x33, 0x04); //EXP120 
	HI351WriteCmosSensor(0x34, 0xf1); 
	HI351WriteCmosSensor(0x35, 0xa0); 
	HI351WriteCmosSensor(0x3c, 0x00); //EXP Fix 15.00 fps
	HI351WriteCmosSensor(0x3d, 0x27); 
	HI351WriteCmosSensor(0x3e, 0xaa); 
	HI351WriteCmosSensor(0x3f, 0x4c); 
	HI351WriteCmosSensor(0x36, 0x00); //EXP Unit 
	HI351WriteCmosSensor(0x37, 0x05); 
	HI351WriteCmosSensor(0x38, 0xdc); 
	//PCON IDLE
	HI351SetPage(0x30);
	HI351WriteCmosSensor(0x24,0x02);
	HI351SetPage(0xC0);
//	HI351WriteCmosSensor(0x03,0xc0);
	HI351WriteCmosSensor(0xe1,0x80);	// PCON Enable option
	HI351WriteCmosSensor(0xe1,0x80);	// PCON MODE ON
	HI351SetPage(0xCF);
//	HI351WriteCmosSensor(0x03,0xcf);
	HI351WriteCmosSensor(0x13,0x00);	//Y_LUM_MAX 40fps, AG 0xA0
	HI351WriteCmosSensor(0x14,0x4d);
	HI351WriteCmosSensor(0x15,0x3f);
	HI351WriteCmosSensor(0x16,0x64);
	HI351WriteCmosSensor(0x17,0x00);	//Y_LUM_middle1  62.5fps, AG 0x4c
	HI351WriteCmosSensor(0x18,0x14);
	HI351WriteCmosSensor(0x19,0xe2);
	HI351WriteCmosSensor(0x1a,0xae);
	HI351SetPage(0xC0);
//	HI351WriteCmosSensor(0x03,0xc0);
	HI351WriteCmosSensor(0xe1,0x00);	// PCON Enable option
	HI351SetPage(0x30);
	//HI351WriteCmosSensor(0x03,0x30);
	HI351WriteCmosSensor(0x25,0x0e); // PCON IIC ON
	HI351WriteCmosSensor(0x25,0x1e); // PCON IIC OFF
	HI351WriteCmosSensor(0x24,0x00);
	//PCON IDLE END
	HI351SetAeMode(KAL_TRUE);
	
		HI351SetPage(0x30);
	HI351WriteCmosSensor(0x36,0xa2); //dma & adaptive on  
	HI351WriteCmosSensor(0x36,0x28); //preview function
					  
	  HI351SetPage(0x00);
	HI351WriteCmosSensor(0x01,0xf0);	
  }
  else
  {
    SENSORDB("Wrong frame rate setting %d\n", FrameRate);
    return KAL_FALSE;
  }
  /*
  HI351Sensor.DummyLines = HI351Sensor.PvOpClk * 1000000 * HI351_FRAME_RATE_UNIT / LineLength / HI351Sensor.MiniFrameRate; 
  if(HI351Sensor.DummyLines > HI351_PV_PERIOD_LINE_NUMS + 4)
    HI351Sensor.DummyLines -= HI351_PV_PERIOD_LINE_NUMS; 
  else
    HI351Sensor.DummyLines = 4;*/
  /* modify DummyPixel must gen AE table again */
  //HI351WriteDummy(HI351Sensor.DummyPixels, HI351Sensor.DummyLines); 
  //HI351FixFrameRate(KAL_TRUE);  
  
    //MAXshutter take effect should enable AE again
  HI351Sensor.VidoeMode = KAL_TRUE;
  
  return TRUE;
}

UINT32 HI351FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                        UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
  UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
  UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
  UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
  UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
  MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

  switch (FeatureId)
  {
    case SENSOR_FEATURE_GET_RESOLUTION:
      *pFeatureReturnPara16++=HI351_FULL_WIDTH;
      *pFeatureReturnPara16=HI351_FULL_HEIGHT;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_GET_PERIOD:
      *pFeatureReturnPara16++=HI351_PV_PERIOD_PIXEL_NUMS+HI351Sensor.PVDummyPixels;
      *pFeatureReturnPara16=HI351_PV_PERIOD_LINE_NUMS+HI351Sensor.PVDummyLines;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
      *pFeatureReturnPara32 = HI351Sensor.PvOpClk*2;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_SET_ESHUTTER:
      break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
      HI351NightMode((BOOL) *pFeatureData16);
      break;
    case SENSOR_FEATURE_SET_GAIN:
      case SENSOR_FEATURE_SET_FLASHLIGHT:
      break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
      break;
    case SENSOR_FEATURE_SET_REGISTER:
      HI351WriteCmosSensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
      break;
    case SENSOR_FEATURE_GET_REGISTER:
      pSensorRegData->RegData = HI351ReadCmosSensor(pSensorRegData->RegAddr);
      break;
    case SENSOR_FEATURE_GET_CONFIG_PARA:
      *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
      break;
    case SENSOR_FEATURE_SET_CCT_REGISTER:
    case SENSOR_FEATURE_GET_CCT_REGISTER:
    case SENSOR_FEATURE_SET_ENG_REGISTER:
    case SENSOR_FEATURE_GET_ENG_REGISTER:
    case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
    case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
    case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
    case SENSOR_FEATURE_GET_GROUP_INFO:
    case SENSOR_FEATURE_GET_ITEM_INFO:
    case SENSOR_FEATURE_SET_ITEM_INFO:
    case SENSOR_FEATURE_GET_ENG_INFO:
      break;
    case SENSOR_FEATURE_GET_GROUP_COUNT:
      *pFeatureReturnPara32++=0;
      *pFeatureParaLen=4;
      break; 
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
      // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
      // if EEPROM does not exist in camera module.
      *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_SET_YUV_CMD:
      HI351YUVSensorSetting((FEATURE_ID)*pFeatureData32, *(pFeatureData32+1));
      break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
      HI351YUVSetVideoMode(*pFeatureData16);
      break; 
//  case SENSOR_FEATURE_CHECK_SENSOR_ID:
//	  HI351GetSensorID(pFeatureReturnPara32); 
//	  break; 
    default:
      break;
  }
  return ERROR_NONE;
} /* HI351FeatureControl() */



UINT32 HI351_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
  static SENSOR_FUNCTION_STRUCT SensorFuncHI351=
  {
    HI351Open,
    HI351GetInfo,
    HI351GetResolution,
    HI351FeatureControl,
    HI351Control,
    HI351Close
  };

  /* To Do : Check Sensor status here */
  if (pfFunc!=NULL)
    *pfFunc=&SensorFuncHI351;

  return ERROR_NONE;
} /* SensorInit() */
