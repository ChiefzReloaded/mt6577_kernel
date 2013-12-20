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
 * applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/
#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <platform/mt_pwm.h>
	#ifdef LCD_DEBUG
		#define LCM_DEBUG(format, ...)   printf("uboot ssd2825" format "\n", ## __VA_ARGS__)
	#else
		#define LCM_DEBUG(format, ...)
	#endif

#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt6577_gpio.h>
	#include <asm/arch/mt6577_pwm.h>

	#ifdef LCD_DEBUG
		#define LCM_DEBUG(format, ...)   printf("uboot ssd2825" format "\n", ## __VA_ARGS__)
	#else
		#define LCM_DEBUG(format, ...)
	#endif
#else
	#include <mach/mt_gpio.h>
	#include <mach/mt_pwm.h>
	#include <mach/mt_pm_ldo.h>
	#ifdef LCD_DEBUG
		#define LCM_DEBUG(format, ...)   printk("kernel ssd2825" format "\n", ## __VA_ARGS__)
	#else
		#define LCM_DEBUG(format, ...)
	#endif
#endif

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(320)
#define FRAME_HEIGHT 										(480)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									1

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif
static unsigned int lcm_esd_test = TRUE;      ///only for ESD test

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

//static unsigned int lcm_read(void);
static unsigned int lcm_read_IC_name(void);

static struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},

    // Sleep Mode On
	{0x10, 1, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

//                                               
#if 1  // GOLF SEOSANGCHEOL 121217 ,
//ili9487
static struct LCM_setting_table lcm_init_setting[] = {

    {0xc1, 1, {0x41}}, // pwr ctrl 2
//    {0xc1, 2, {0x41,0x01}}, // pwr ctrl 2

    {0xc0, 2, {0x0C, 0x0C}}, //pwr ctrl 1

    {0xc2, 1, {0x33}}, //pwr ctrl 1

// 9488
  //{0xe0, 15, {0x00, 0x12, 0x18, 0x05, 0x10, 0x06, 0x40, 0xCA, 0x54, 0x06, 0x0b, 0x09, 0x33, 0x37, 0x0f}},
  //{0xe1, 15, {0x00, 0x08, 0x0d, 0x03, 0x04, 0x09, 0x2a, 0x53, 0x3f, 0x09, 0x0f, 0x0a, 0x27, 0x2d, 0x0f}},

// 9487
   {0xe0, 15, {0x0f, 0x37, 0x33, 0x09, 0x0b, 0x06, 0x54, 0xca, 0x40, 0x06, 0x10, 0x05, 0x18, 0x12, 0x00}}, // pos gamma
   {0xe1, 15, {0x0f, 0x2d, 0x27, 0x0a, 0x0f, 0x09, 0x3f, 0x53, 0x2a, 0x09, 0x04, 0x03, 0x0d, 0x08, 0x00}}, // neg gamma

    {0xc5, 3, {0x00, 0x60, 0x80}}, // vcom

    {0xB4, 1, {0x00}}, //inversion

    {0xB1, 2, {0xD0,0x17}},//                                                    

    {0x21, 1, {0x00}},

    {0xB6, 3, {0x02, 0x22, 0x3B}}, //displaycontrol  0x22 -> 0x42 ->0x22 reverted 20130426

    {0xE9, 1, {0x00}}, //24BITEN

//    {0xF2, 5, {0x58, 0x10, 0x12, 0x02, 0x92}},

    {0xF7, 4, {0xA9, 0x51, 0x2C, 0x82}}, // frame rate ctrl

    {0x3A, 1, {0x66}}, // pixel format
    //{0x3A, 1, {0x77}}, // pixel format

    {0x36, 1, {0x08}}, // mem acc

//                                                                                       

    {0x11, 1, {0x00}},

//    {0xD1, 3, {0x55,0xAA,0x66}},

    {REGFLAG_DELAY, 120, {}},

    {0x29, 1, {0x00}},

    {REGFLAG_DELAY, 20, {}},

    {0x2C, 1, {0x00}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
//ili9488
#if 1  //                                            
static struct LCM_setting_table lcm_init_setting_2[] = {

    {0xC1, 1, {0x41}}, // pwr ctrl 2
    //{0xC1, 2, {0x40,0x01}}, // pwr ctrl 2

    {0xC0, 2, {0x0C, 0x0C}}, //pwr ctrl 1

//                                                                  
    {0xE0, 15, {0x00, 0x11, 0x13, 0x05, 0x10, 0x06, 0x3C, 0x88, 0x54, 0x0A, 0x0D, 0x0B, 0x30, 0x36, 0x0F}},
    {0xE1, 15, {0x00, 0x07, 0x0D, 0x04, 0x12, 0x04, 0x29, 0x44, 0x42, 0x09, 0x0F, 0x0A, 0x2B, 0x2D, 0x0F}},

    {0xC5, 3, {0x00, 0x60, 0x80}}, // vcom

    /* mtkfb.c lcd_fps = 6500; is changed by below for passing CTS test*/
    {0xB1, 2, {0xD0,0x17}},//                                                       
//                                                              

//    {0xB5, 4, {0x0f,0x0f,0x0a,0x04}},

    {0xB4, 1, {0x00}}, //inversion

    {0x21, 1, {0x00}},

    {0xB6, 3, {0x02, 0x22, 0x3B}}, //displaycontrol  0x22 -> 0x42 ->0x22 reverted 20130426

    {0xE9, 1, {0x00}}, //24BITEN

//    {0xF2, 5, {0x58, 0x10, 0x12, 0x02, 0x92}},

    {0xF7, 4, {0xA9, 0x51, 0x2C, 0x82}}, // frame rate ctrl

    {0x3A, 1, {0x66}}, // pixel format 0x77->0x66 add
    //{0x3A, 1, {0x77}}, // pixel format

    {0x36, 1, {0x08}}, // mem acc

    {0x35, 1, {0x00}}, //                                                              

    {0x11, 1, {0x00}},

//    {0xD1, 3, {0x55,0xAA,0x66}},

    {REGFLAG_DELAY, 100, {}},

    {0x29, 1, {0x00}},

    {REGFLAG_DELAY, 20, {}},

    {0x2C, 1, {0x00}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

#else
static struct LCM_setting_table lcm_init_setting_2[] = {

    {0xC1, 2, {0x41,0x01}}, // pwr ctrl 2

    {0xC0, 2, {0x0C, 0x0C}}, //pwr ctrl 1

//    {0xc2, 1, {0x33}}, //pwr ctrl 1

//                                                                  
    {0xE0, 15, {0x00, 0x11, 0x13, 0x05, 0x10, 0x06, 0x3C, 0x88, 0x54, 0x0A, 0x0D, 0x0B, 0x30, 0x36, 0x0F}},
    {0xE1, 15, {0x00, 0x07, 0x0D, 0x04, 0x12, 0x04, 0x29, 0x44, 0x42, 0x09, 0x0F, 0x0A, 0x2B, 0x2D, 0x0F}},

// 9487
    //{0xe0, 15, {0x0f, 0x37, 0x33, 0x09, 0x0b, 0x06, 0x54, 0xca, 0x40, 0x06, 0x10, 0x05, 0x18, 0x12, 0x00}}, // pos gamma
    //{0xe1, 15, {0x0f, 0x2d, 0x27, 0x0a, 0x0f, 0x09, 0x3f, 0x53, 0x2a, 0x09, 0x04, 0x03, 0x0d, 0x08, 0x00}}, // neg gamma

    {0xC5, 3, {0x00, 0x60, 0x80}}, // vcom

    {0xB1, 2, {0xB0,0x11}}, //                                      

    {0xB4, 1, {0x00}}, //inversion

    {0x21, 1, {0x00}},

    {0xB6, 3, {0x0A, 0x02, 0x3B}}, //displaycontrol 22 ->02

    {0xE9, 1, {0x00}}, //24BITEN

    {0xF2, 5, {0x58, 0x10, 0x12, 0x02, 0x92}},

    {0xF7, 4, {0xA9, 0x51, 0x2C, 0x82}}, // frame rate ctrl

   {0xfc, 2, {0x00, 0x09}},

    {0x3A, 1, {0x66}}, // pixel format

    {0xbf, 1, {0x21}},

    {0x36, 1, {0x88}}, // mem acc 08->48

    {0x35, 1, {0x00}}, //                                                              

    {0x11, 1, {0x00}},

    {REGFLAG_DELAY, 120, {}},

    {0x29, 1, {0x00}},

    {REGFLAG_DELAY, 20, {}},

    {0x2C, 1, {0x00}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

#else
static struct LCM_setting_table lcm_init_setting[] = {

    {0x36, 1, {0x48}},
    {0x3a, 1, {0x66}},

    {0xe0, 15, {0x0f, 0x2b, 0x21, 0x06, 0x08, 0x07, 0x53, 0x65, 0x40, 0x06, 0x0f, 0x02, 0x0a, 0x04, 0x00}},
    {0xe1, 15, {0x0f, 0x37, 0x32, 0x0b, 0x0f, 0x04, 0x46, 0x31, 0x34, 0x00, 0x0a, 0x00, 0x21, 0x1a, 0x00}},

    {0xf7, 5, {0xa9, 0x91, 0x2d, 0x8a, 0x4c}},
    //{0xf7, 5, {0xa9, 0x91, 0x2d, 0x8a, 0x4f}},

    {0xF8, 2, {0x21, 0x04}},
    //{0xF8, 2, {0x21, 0x06}},

    {0xc5, 4, {0x00, 0x3f, 0x80, 0x00}},
    //{0xc5, 4, {0x00, 0x4e, 0x80, 0x00}},

    {0xc0, 2, {0x10, 0x14}},
    //{0xc0, 2, {0x12, 0x12}},

    {0xc1, 1, {0x47}},
    //{0xc1, 1, {0x42}},

    {0xb6, 1, {0x00, 0x42, 0x3b}},
    //{0xb6, 1, {0x02, 0x22, 0x3b}},

    {0xb4, 1, {0x02}},

    {0xb1, 2, {0xa0, 0x10}},
    //{0xb1, 2, {0xa0, 0x11}},

    {REGFLAG_DELAY, 10, {}},
    {0x11, 1, {0x00}},

    {0x29, 1, {0x00}},
    {0x2A,	4, {0x00, 0x00, 0x01, 0x3F}},
    {0x2B,	4, {0x00, 0x00, 0x01, 0xDF}},

#if 0
    {0xF2, 8, {0x18, 0xa3, 0x12, 0x02, 0xb2, 0xff, 0x10, 0x00}},

    {0xc2, 1, {0x02}},

    {0x2a, 4, {0x00, 0x00, 0x01, 0x3f}},
    {0x2b, 4, {0x00, 0x00, 0x01, 0xdf}},

    {0xb7, 1, {0x06}},
    {0xb5, 4, {0x06, 0x06, 0x0a, 0x04}},
#endif

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
//                                               

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {

        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {

            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;

            case REGFLAG_END_OF_TABLE :
                break;

            default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
       	}
    }

}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));

		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
                //                                                          
#if defined(BUILD_UBOOT ) || defined(BUILD_LK)
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dsi.lcm_ext_te_monitor		= 0;
#else
		//params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dsi.lcm_ext_te_monitor		= 1;
#endif

		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_ONE_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB666;
		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;
		// Video mode setting
		params->dsi.intermediat_buffer_num = 2;
		params->dsi.PS=LCM_PACKED_PS_18BIT_RGB666;
		params->dsi.vertical_sync_active				= 3;
		params->dsi.vertical_backporch					= 8; // ssc 5->8
		params->dsi.vertical_frontporch 				= 8;
		params->dsi.vertical_active_line				= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active				= 10;
		params->dsi.horizontal_backporch				= 20;// 20; ssc 30->20
		params->dsi.horizontal_frontporch				= 40;// 20; ssc 30->40
		params->dsi.horizontal_active_pixel 			= FRAME_WIDTH;

		// Bit rate calculation
		//                                                                          
		params->dsi.pll_div1=22;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		//                                               
		params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)

#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
			printk("\n[LCM] [%s] \n",__func__);
#else
			printf("\n[LCM] [%s] \n",__func__);
#endif
}

unsigned char get_driver_ic = 0;
unsigned char driver_ic_ili9487 = 0;

static void lcm_init(void)
{

	unsigned int data_array[16];

#ifdef BUILD_UBOOT
#elif defined(BUILD_LK)
#else
	hwPowerOn(MT65XX_POWER_LDO_VCAM_IO, VOL_1800, "1V8_LCD_VIO_MTK_S" );
	MDELAY(1);
	hwPowerOn(MT65XX_POWER_LDO_VCAMD, VOL_2800, "2V8_LCD_VCC_MTK_S" );
#endif
	MDELAY(1);
	mt_set_gpio_mode(GPIO18, GPIO_MODE_00); /* LCD ENABLE PIN -> LOW */
	mt_set_gpio_pull_enable(GPIO18, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO18, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO18,GPIO_OUT_ONE);
	MDELAY(1);
	mt_set_gpio_out(GPIO18,GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_out(GPIO18,GPIO_OUT_ONE);
	MDELAY(120);

	/*For DI Issue, Use dsi_set_cmdq() instead of dsi_set_cmdq_v2()*/

//                                                                  
#if 1
#if 1 //                                                                         
    if (get_driver_ic < 2)
    {
    	unsigned char lcd_ic_name = 0xff;

	lcd_ic_name = 	lcm_read_IC_name();
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))  /*                                                     */
    printk("\n[LCM]%s, LCD ID = [0x%x]\n", __func__, lcd_ic_name);
#else
    printf("\n[LCM]%s, LCD ID = [0x%x]\n", __func__, lcd_ic_name);
#endif

	if(lcd_ic_name == 0x87)
	{
		driver_ic_ili9487= 1;
	}
	else if(lcd_ic_name == 0x88)
	{
		driver_ic_ili9487=0;
	}
	else
	{

#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))  /*                                                     */
		printk("\n[LCM]lcm_esd_test = %d \n", lcm_esd_test);
#else
		printf("\n[LCM]lcm_esd_test = %d \n", lcm_esd_test);
#endif
		lcm_esd_test = FALSE;
		driver_ic_ili9487=0;
	}

	get_driver_ic++;
    }
	if (driver_ic_ili9487 == 1)
	push_table(lcm_init_setting, sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table), 1);
	else
	push_table(lcm_init_setting_2, sizeof(lcm_init_setting_2) / sizeof(struct LCM_setting_table), 1);

/*                                                     */
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
    printk("\n[LCM]%s, get_driver_ic[%d] \n", __func__,get_driver_ic);
    printk("\n[LCM]%s, [IC=%s] \n", __func__,lcm_esd_test?(driver_ic_ili9487?"ILI9487":"ILI9488"):"lcd no connection");
#else
    printf("\n[LCM]%s, get_driver_ic[%d]\n", __func__,get_driver_ic);
    printf("\n[LCM]%s, [IC=%s] \n", __func__,lcm_esd_test?(driver_ic_ili9487?"ILI9487":"ILI9488"):"lcd no connection");
#endif
/*                                                     */
#else
	push_table(lcm_init_setting, sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table), 1);
#endif

#else
	data_array[0] = 0x00023902;
	data_array[1] = 0x00000836; // mem acc
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x0000663A; // pixel format
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00103902;
	data_array[1] = 0x1E1600E0; // positive gamma
	data_array[2] = 0x42081406;
	data_array[3] = 0x0F095078;
	data_array[4] = 0x0F1F1B0C;
	dsi_set_cmdq(&data_array, 5, 1);

	data_array[0] = 0x00103902;
	data_array[1] = 0x232000E1; // negative gamma
	data_array[2] = 0x39061004;
	data_array[3] = 0x0F064945;
	data_array[4] = 0x0F2B280B;
	dsi_set_cmdq(&data_array, 5, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x2D91A9F7; // Frame rate ctrl
	data_array[2] = 0x00004F8A;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000621F8; // repair ctrl
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x804E00C5; // vcom
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001212C0; // power ctrl 1
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000042C1; // power ctrl 2
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x3B2202B6; // display func
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000002B4; // inversion cont
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x0011B0B1; // frame rate
	dsi_set_cmdq(&data_array, 2, 1);

	MDELAY(10);

	data_array[0] = 0x00013902;
	data_array[1] = 0x00000011; // sleep out
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00013902;
	data_array[1] = 0x00000029; // display On
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0] = 0x00053902;
	data_array[1] = 0x0100002A; // column address set
	data_array[2] = 0x0000003F;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x0100002B; // page address set
	data_array[2] = 0x000000DF;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0] = 0x002c3909; // Memory Write
	dsi_set_cmdq(&data_array, 1, 1);
	//MDELAY(120);
#endif

}

static void lcm_suspend(void)
{
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
	printk("\n[LCM] [%s] \n",__func__);
#else
	printf("\n[LCM] [%s] \n",__func__);
#endif

#ifdef BUILD_UBOOT
#elif defined(BUILD_LK)
	mt_set_gpio_mode(GPIO18, GPIO_MODE_00); /* LCD RESET, mipi sequence issue.(temporary code) */
	mt_set_gpio_pull_enable(GPIO18, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO18, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO18,GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO18,GPIO_OUT_ONE);
	MDELAY(120);
#else
	mt_set_gpio_mode(GPIO18, GPIO_MODE_00); /* LCD RESET, mipi sequence issue.(temporary code) */
	mt_set_gpio_pull_enable(GPIO18, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO18, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO18,GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO18,GPIO_OUT_ONE);
	MDELAY(120);

	hwPowerDown(MT65XX_POWER_LDO_VCAMD, "2V8_LCD_VCC_MTK_S" );
	hwPowerDown(MT65XX_POWER_LDO_VCAM_IO, "1V8_LCD_VIO_MTK_S" );

    mt_set_gpio_mode(GPIO18, GPIO_MODE_00);
    mt_set_gpio_pull_enable(GPIO18, GPIO_PULL_ENABLE);
    mt_set_gpio_dir(GPIO18, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO18,GPIO_OUT_ZERO);/* lcd reset_n is low */
#endif
}


static void lcm_resume(void)
{
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
	printk("\n[LCM] [%s] \n",__func__);
#else
	printf("\n[LCM] [%s] \n",__func__);
#endif
	lcm_init();
	//MDELAY(15);
}

int lcd_log_cnt = 0;
static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	data_array[3]= 0x00053902;
	data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[5]= (y1_LSB);
	data_array[6]= 0x002c3909;

	dsi_set_cmdq(&data_array, 7, 0);
#if 0
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))  /*                                                     */
if(lcd_log_cnt > 100)
{
    printk("\n[LCM]%s[x:%d, y:%d, width:%d, height:%d]\n ", __func__,   x, y, width, height);
    lcd_log_cnt = 0;
}
else{
	lcd_log_cnt++;
}
#else
if(lcd_log_cnt > 100)
{
	printf("\n[LCM]%s[x:%d, y:%d, width:%d, height:%d]\n ", __func__,   x, y, width, height);
	lcd_log_cnt = 0;
}
else{
	lcd_log_cnt++;
}
#endif
#endif
}


static void lcm_setbacklight(unsigned int level)
{
	unsigned int default_level = 145;
	unsigned int mapped_level = 0;

	//                                  
	if(level > 255)
			level = 255;

	if(level >0)
			mapped_level = default_level+(level)*(255-default_level)/(255);
	else
			mapped_level=0;

	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = mapped_level;

	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_setpwm(unsigned int divider)
{
	// TBD
}


static unsigned int lcm_getpwm(unsigned int divider)
{
	// ref freq = 15MHz, B0h setting 0x80, so 80.6% * freq is pwm_clk;
	// pwm_clk / 255 / 2(lcm_setpwm() 6th params) = pwm_duration = 23706
	unsigned int pwm_clk = 23706 / (1<<divider);
	return pwm_clk;
}
#if 1  /*                                                     */
int esd_lcd_reg = 0x00;
static unsigned int lcm_esd_check(void)
{
    #if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
    int reg_val = 0;

    int buffer0[3]={0x88};
    unsigned int array[16];

	if(lcm_esd_test == FALSE)
	{
		return FALSE;
	}


    array[0] = 0x00033700;// read id return two byte,version and id
    dsi_set_cmdq(array, 1, 1);

    read_reg_v2(0xC0, buffer0, 3);

    esd_lcd_reg = buffer0[0] & 0x0000FFFF;

    if((buffer0[0] & 0x0000FFFF) == 0xc0c)
    {
	return FALSE;
    }
    else
    {
    	return TRUE;
    }
    #endif
}

static unsigned int lcm_esd_recover(void)
{
    #if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
    printk("\n[LCM] lcm_esd_recover[0x0c0c == 0x%x] \n",esd_lcd_reg);
    printk("\n[LCM] LCD re-initialize \n");
    #else
    printf("\n[LCM] lcm_esd_recover[0x0c0c == 0x%x] \n",esd_lcd_reg);
    printf("\n[LCM] LCD re-initialize \n");
    #endif
    lcm_suspend();
    MDELAY(200);
    lcm_resume();

    return TRUE;
}

#endif /*              */
static unsigned int lcm_read_IC_name(void)
{
	unsigned int id = 0;
	unsigned char buffer[4]={0x88};
	unsigned int array[16];

	push_table(lcm_init_setting_2, sizeof(lcm_init_setting_2) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00033700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
//	id = read_reg(0xF4);
	read_reg_v2(0xD3, buffer, 3);
	id = buffer[2]; //we only need ID

    #if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))  /*                                                     */
    printk("\n[LCM]%s, LCD ID = [0x%x]\n", __func__,  buffer[2]);
    #else
    printf("\n[LCM]%s, LCD ID = [0x%x]\n", __func__,  buffer[2]);
    #endif

	return id;
    //return (LCM_ID == id)?1:0;
}


LCM_DRIVER ili9486_hvga_lcm_drv =
{
    .name			= "ili9486_hvga_dsi_cmd",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,

#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))  /*                                                     */
	.esd_check	 = lcm_esd_check,
	.esd_recover   = lcm_esd_recover,
#endif /*              */
#endif

};

