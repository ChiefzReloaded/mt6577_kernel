#include <cust_leds.h>
#include <mach/mt_pwm.h>

#include <linux/kernel.h>
#include <mach/pmic_mt6329_hw_bank1.h> 
#include <mach/pmic_mt6329_sw_bank1.h> 
#include <mach/pmic_mt6329_hw.h>
#include <mach/pmic_mt6329_sw.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>

extern int mtkfb_set_backlight_level(unsigned int level);
extern int mtkfb_set_backlight_pwm(int div);
extern int chargepump_set_backlight_level(unsigned int level);
/*                                                             */
#if 1 //ndef MTK_NFC_SUPPORT
unsigned int Cust_SetKeyled(int level, int div)
{
    return 0;
}
#else
extern void Button_LED_control(int on_off);
unsigned int Cust_SetKeyled(int level, int div)
{
	Button_LED_control(level);
    return 0;
}
#endif
/*                                                             */

unsigned int brightness_mapping(unsigned int level)
{
    unsigned int mapped_level;
    
    mapped_level = level;
       
	return mapped_level;
}

unsigned int Cust_SetBacklight(int level, int div)
{
//                                                                                               
//                                                                                                                      
	chargepump_set_backlight_level(level);
    return 0;
}

static struct cust_mt65xx_led cust_led_list[MT65XX_LED_TYPE_TOTAL] = {
	{"red",               MT65XX_LED_MODE_NONE, -1,{0}},
	{"green",             MT65XX_LED_MODE_NONE, -1,{0}},
	{"blue",              MT65XX_LED_MODE_NONE, -1,{0}},
	{"jogball-backlight", MT65XX_LED_MODE_NONE, -1,{0}},
	{"keyboard-backlight",MT65XX_LED_MODE_NONE, -1,{0}},
#if 0 //                                                          
	{"button-backlight",  MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_BUTTON,{0}},
#else
	{"button-backlight",  MT65XX_LED_MODE_CUST, (int)Cust_SetKeyled,{0}},
#endif
	{"lcd-backlight",     MT65XX_LED_MODE_CUST, (int)Cust_SetBacklight,{0}},
};

struct cust_mt65xx_led *get_cust_led_list(void)
{
	return cust_led_list;
}

