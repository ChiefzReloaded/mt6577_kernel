#include <mach/mt_typedefs.h>
#include <linux/module.h>      /* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/mt_gpio.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>      // might need to get fuel gauge info
#include <linux/power_supply.h>     // might need to get fuel gauge info
//#include "../staging/android//timed_output.h"
#include "charger_rt9536.h"
#include <linux/xlog.h>

#include <cust_gpio_usage.h>
#include <cust_gpio_boot.h>

#if defined ( LGE_BSP_LGBM ) //                                              
#define ENABLE_RT9536_DEBUG

#define RT9536_TAG "LGBM"

#define RT9536_ERR(fmt, args...)    xlog_printk(ANDROID_LOG_ERROR, RT9536_TAG, "[ERROR] %s() line=%d : "fmt, __FUNCTION__, __LINE__, ##args)

#if defined ( ENABLE_RT9536_DEBUG )
/* You need to select proper loglevel to see the log what you want. */
#if 1 //                                              
#define RT9536_ENTRY(f)     ;
#define RT9536_EXIT(f)      ;
#else
#define RT9536_ENTRY(f)         xlog_printk(ANDROID_LOG_ERROR, RT9536_TAG, "ENTER : %s()\n", __FUNCTION__)
#define RT9536_EXIT(f)      xlog_printk(ANDROID_LOG_ERROR, RT9536_TAG, "EXIT : %s()\n", __FUNCTION__)
#endif //                                              
#define RT9536_LOG(fmt, args...)    xlog_printk(ANDROID_LOG_ERROR, RT9536_TAG, fmt, ##args)
#define RT9536_DBG(fmt, args...)    xlog_printk(ANDROID_LOG_ERROR, RT9536_TAG, fmt, ##args)
#else
#define RT9536_ENTRY(f)             xlog_printk(ANDROID_LOG_DEBUG, RT9536_TAG, "ENTER : %s()\n", __FUNCTION__)
#define RT9536_EXIT(f)          xlog_printk(ANDROID_LOG_DEBUG, RT9536_TAG, "EXIT : %s()\n", __FUNCTION__)
#define RT9536_LOG(fmt, args...)    xlog_printk(ANDROID_LOG_INFO, RT9536_TAG, fmt, ##args)
#define RT9536_DBG(fmt, args...)    xlog_printk(ANDROID_LOG_DEBUG, RT9536_TAG, fmt, ##args)
#endif

static DEFINE_SPINLOCK(rt9536_spin);

void RT9536_WriteCommand( RT9536_ChargingMode mode )
{
    int pulseCount = 0;
    int i= 0;

    RT9536_ENTRY();

    switch ( mode )
    {
        case RT9536_CM_OFF:
            RT9536_LOG("CHARGING_MODE = RT9536_CM_OFF\n");
            pulseCount = 0;
            break;
        case RT9536_CM_USB_100:
            RT9536_LOG("CHARGING_MODE = RT9536_CM_USB_100\n");
            pulseCount = 2;
            break;
        case RT9536_CM_USB_500:
            RT9536_LOG("CHARGING_MODE = RT9536_CM_USB_500\n");
            pulseCount = 4;
            break;
        case RT9536_CM_I_SET:
            RT9536_LOG("CHARGING_MODE = RT9536_CM_I_SET\n");
            pulseCount = 1;
            break;
        case RT9536_CM_FACTORY:
            RT9536_LOG("CHARGING_MODE = RT9536_CM_FACTORY\n");
            pulseCount = 3;
            break;
        default:
            RT9536_ERR("Invalid Charging Mode ( %d )\n", mode);
            break;

    }

    if( mode != RT9536_CM_OFF && mode != RT9536_CM_UNKNOWN )
    {
        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
        msleep(32);
        //udelay(110);

        spin_lock(&rt9536_spin);

        for( i=0; i<pulseCount ; i++)
        {
            mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
            udelay(150);
            mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
            if( i < ( pulseCount - 1 ) )
            {
                udelay(150);
            }
        }

        spin_unlock(&rt9536_spin);

        udelay(1800);

        spin_lock(&rt9536_spin);

        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
        udelay(850);
        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);

        spin_unlock(&rt9536_spin);

        msleep(2);

    }
    else if( mode == RT9536_CM_OFF )
    {
        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
        msleep(2);
        //mdelay(3);
    }
    else
    {
        RT9536_ERR("Invalid Charging Mode ( %d )\n", mode);
    }
}

void RT9536_SetChargingMode( RT9536_ChargingMode newMode )
{
    static RT9536_ChargingMode prevMode = RT9536_CM_UNKNOWN;

    if( prevMode != newMode )
    {
        if( newMode == RT9536_CM_OFF )
        {
            RT9536_WriteCommand(newMode);
        }
        else
        {
            if( prevMode != RT9536_CM_OFF )
            {
                RT9536_WriteCommand(RT9536_CM_OFF);
            }

            RT9536_WriteCommand(newMode);
        }

        prevMode = newMode;

    }

}
EXPORT_SYMBOL(RT9536_SetChargingMode);

static int charging_ic_probe(struct platform_device *dev)
{
    return 0;
}

static int charging_ic_remove(struct platform_device *dev)
{
    return 0;
}

#else
/*                                                                                                              */

//                                                                                    
#if 1
#define SINGLE_CHARGER_CONTROL_USING_SPIN_LOCK
#else
#define SINGLE_CHARGER_CONTOL_USING_GPT_TIMER                           1
#endif

#if defined(SINGLE_CHARGER_CONTROL_USING_SPIN_LOCK)
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#endif
//                                                                                    

#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <mach/mt_gpt.h>
#endif
/*                                                                                                              */

static DEFINE_MUTEX(charging_lock);

/*                                                                                                              */
#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER

static DEFINE_SPINLOCK(g_single_charger_lock);
static unsigned long g_single_flags = 0;

static DEFINE_MUTEX(charging_timer_api_lock);

static GPT_CONFIG config = {0, };
/* GPT3 Timer is assigned to single charger */
/* To use GPT4 Timer, it must be activated interrupt service in mt_gpt.c */
/* GPT5 Timer is used in the battery thread. So it can not be used. */
static GPT_NUM  gpt_num = GPT3;

/* -------------------------------------------------------------------------- */

#define CHARGING_UNKNOWN_COMMAND                    -1

#define CHARGING_SET_USB_500_COMMAND                0
#define CHARGING_SET_TA_MODE_COMMAND                1
#define CHARGING_SET_USB_100_COMMAND                2
#define CHARGING_SET_FACTORY_MODE_COMMAND           3
#define CHARGING_DEACTIVE_COMMAND                   4
#define CHARGING_MAX_COMMAND                        4

#define CMD_INTERVAL_MAX_COUNT                      20
typedef struct Charger_Command_Interval
{
    int is_high;
    int delay_count;
};


typedef struct Charger_Command
{
    int command;
    struct Charger_Command_Interval cmd_interval[CMD_INTERVAL_MAX_COUNT];
};

/*
  Using 13 MHz clock
  1 us    : 13
  10 us   : 130
  100 us  : 1300
  1 ms    : 13000

  1 clock : 100 us < t(low) or t(high) < 700 us
  400 us  : 5200

  last low delay > 1.5 ms
  1.8 ms  : 23400

  set 4.35V : 750 us < t(set) < 1ms
  850 us  : 11050

  32 ms   : 416000
*/
#if 1   /* 4.35V battery */
struct Charger_Command charger_command_list[CHARGING_MAX_COMMAND + 1] =
{
    /*                                   wait 32ms  1 clock           2 clock           3 clock           4 clock  1.8 ms    set 4.35V     */
    { CHARGING_SET_USB_500_COMMAND,      0, 416000, 1, 5200, 0, 5200, 1, 5200, 0, 5200, 1, 5200, 0, 5200, 1, 5200, 0, 23400, 1, 11050, 0, 5200, 0, 0},
    /*                                   wait 32ms  1 clock  1.8 ms    set 4.35V     */
    { CHARGING_SET_TA_MODE_COMMAND,      0, 416000, 1, 5200, 0, 23400, 1, 11050, 0, 5200, 0, 0},
    /*                                   wait 32ms  1 clock           2 clock  1.8 ms    set 4.35V     */
    { CHARGING_SET_USB_100_COMMAND,      0, 416000, 1, 5200, 0, 5200, 1, 5200, 0, 23400, 1, 11050, 0, 5200, 0, 0},
    /*                                   wait 32ms  1 clock           2 clock           3 clock  1.8 ms    set 4.35V     */
    { CHARGING_SET_FACTORY_MODE_COMMAND, 0, 416000, 1, 5200, 0, 5200, 1, 5200, 0, 5200, 1, 5200, 0, 23400, 1, 11050, 0, 5200, 0, 0},
    /*                                   wait 32ms  2 ms    */
    { CHARGING_DEACTIVE_COMMAND,         0, 416000, 1, 26000, 0, 0},
};
#else   /* 4.2V battery */
struct Charger_Command charger_command_list[CHARGING_MAX_COMMAND + 1] =
{
    /*                                   wait 32ms  1 clock           2 clock           3 clock           4 clock  1.8 ms */
    { CHARGING_SET_USB_500_COMMAND,      0, 416000, 1, 5200, 0, 5200, 1, 5200, 0, 5200, 1, 5200, 0, 5200, 1, 5200, 0, 23400, 0, 0},
    /*                                   wait 32ms  1 clock  1.8 ms    */
    { CHARGING_SET_TA_MODE_COMMAND,      0, 416000, 1, 5200, 0, 23400, 0, 0},
    /*                                   wait 32ms  1 clock           2 clock  1.8 ms */
    { CHARGING_SET_USB_100_COMMAND,      0, 416000, 1, 5200, 0, 5200, 1, 5200, 0, 23400, 0, 0},
    /*                                   wait 32ms  1 clock           2 clock           3 clock  1.8 ms */
    { CHARGING_SET_FACTORY_MODE_COMMAND, 0, 416000, 1, 5200, 0, 5200, 1, 5200, 0, 5200, 1, 5200, 0, 23400, 0, 0},
    /*                                   wait 32ms  2 ms    */
    { CHARGING_DEACTIVE_COMMAND,         0, 416000, 1, 26000, 0, 0},
};
#endif

enum enum_CHARGER_COMMAND_STATUS
{
    CHARGER_COMMAND_STOP        = 0,
    CHARGER_COMMAND_RUN         = 1
};

static int charger_command = CHARGING_UNKNOWN_COMMAND;
static int charger_command_execute_index = 0;
static int charger_command_status = CHARGER_COMMAND_STOP;

int Set_Charging_Commmand(int nCommand);

#endif
/*                                                                                                              */


enum power_supply_type charging_ic_status;

/* Fuction Prototype */
static void charging_ic_initialize(void);
static irqreturn_t charging_ic_interrupt_handler(int irq, void *data);

struct timer_list charging_timer;

//                                                                                    
#if defined(SINGLE_CHARGER_CONTROL_USING_SPIN_LOCK)
static DEFINE_SPINLOCK(rt9536_spin);

enum charging_ic_charging_mode {
    CHR_USB_500_MODE,
    CHR_ISET_MODE,
    CHR_USB_100_MODE,
    CHR_FACTORY_MODE,
    CHR_DEACTIVE_MODE,
    CHR_UNKNOWN_MODE
};

//static DECLARE_WAIT_QUEUE_HEAD(chrger_set_gpio_ctrl_waiter);

int charging_ic_set_chargingmode(enum charging_ic_charging_mode mode)
{
    int i = 0;
    int pulse_cnt = 0;

    //xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: %s\n", __func__);

    switch(mode)
    {
        case CHR_USB_500_MODE :
            pulse_cnt = 4;
            break;
        case CHR_ISET_MODE :
            pulse_cnt = 1;
            break;
        case CHR_USB_100_MODE :
            pulse_cnt = 2;
            break;
        case CHR_FACTORY_MODE :
            pulse_cnt = 3;
            break;
        case CHR_DEACTIVE_MODE :
            pulse_cnt = 0;
            break;
        case CHR_UNKNOWN_MODE :
        default :
            break;
    }

    if((mode != CHR_DEACTIVE_MODE) && (mode != CHR_UNKNOWN_MODE))
    {
        spin_lock(&rt9536_spin);

        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);

        for(i = 0; i < pulse_cnt; i++)
        {
            udelay(110);
            mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
            udelay(110);
            mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
        }

        spin_unlock(&rt9536_spin);

        udelay(1520);  // delay over 1.5ms

        spin_lock(&rt9536_spin);

        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
        udelay(770);
        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);

        spin_unlock(&rt9536_spin);
    }
    else
    {
        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
        udelay(2000);
    }
}
#endif
//                                                                                    

enum power_supply_type get_charging_ic_status()
{
    return charging_ic_status;
}
EXPORT_SYMBOL(get_charging_ic_status);

/*                                                                                                              */
#ifndef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER
/*                                                                                       */
void high_volt_setting()
{
    // 4.35V setting
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(850);  // about 750 us ~ 1000 us
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
}
/*                                                                                       */
#endif
/*                                                                                                              */


// USB500 mode charging
void charging_ic_active_default()
{
    u32 wait;

    if(charging_ic_status == POWER_SUPPLY_TYPE_USB)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: it's already %s mode!!\n", __func__);
        return;
    }

    if(charging_ic_status != POWER_SUPPLY_TYPE_BATTERY)
    {
        charging_ic_deactive();
    }

/*                                                                                                              */
#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER
    mutex_lock(&charging_lock);
    wait = 0;
    while (wait < 10)
    {
        if (Set_Charging_Commmand(CHARGING_SET_USB_500_COMMAND) == KAL_TRUE)
        {
            break;
        }
        wait++;
        mdelay(10);
    }
    charging_ic_status = POWER_SUPPLY_TYPE_USB;
    mutex_unlock(&charging_lock);
#else
    mutex_lock(&charging_lock);

//                                                                                    
#if defined(SINGLE_CHARGER_CONTROL_USING_SPIN_LOCK)
    charging_ic_set_chargingmode(CHR_USB_500_MODE);
#else
    // USB500 mode
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);

    /*                                                                                       */
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(1800);

    high_volt_setting();
    /*                                                                                       */
#endif
//                                                                                    


    charging_ic_status = POWER_SUPPLY_TYPE_USB;

    mutex_unlock(&charging_lock);
#endif
/*                                                                                                              */

    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: %s : \n", __func__);

}
EXPORT_SYMBOL(charging_ic_active_default);

// TA connection, ISET mode
void charging_ic_set_ta_mode()
{
    u32 wait;

    if(charging_ic_status == POWER_SUPPLY_TYPE_MAINS)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: it's already %s mode!! : \n", __func__);
        return;
    }

    if(charging_ic_status != POWER_SUPPLY_TYPE_BATTERY)
    {
        charging_ic_deactive();
    }
/*                                                                                                              */
#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER
    mutex_lock(&charging_lock);
    wait = 0;
    while (wait < 10)
    {
        if (Set_Charging_Commmand(CHARGING_SET_TA_MODE_COMMAND) == KAL_TRUE)
        {
            break;
        }
        wait++;
        mdelay(10);
    }
    charging_ic_status = POWER_SUPPLY_TYPE_MAINS;
    mutex_unlock(&charging_lock);
#else

    mutex_lock(&charging_lock);

//                                                                                    
#if defined(SINGLE_CHARGER_CONTROL_USING_SPIN_LOCK)
    charging_ic_set_chargingmode(CHR_ISET_MODE);
#else
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);  // about 400 us
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(1800);

    /*                                                                                       */
    high_volt_setting();
    /*                                                                                       */
#endif
//                                                                                    

    charging_ic_status = POWER_SUPPLY_TYPE_MAINS;

    mutex_unlock(&charging_lock);
#endif
/*                                                                                                              */

    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: %s : \n", __func__);
}
EXPORT_SYMBOL(charging_ic_set_ta_mode);

void charging_ic_set_usb_mode()
{
    charging_ic_active_default();
}
EXPORT_SYMBOL(charging_ic_set_usb_mode);

void charging_ic_set_factory_mode()
{
    u32 wait;

    if(charging_ic_status == POWER_SUPPLY_TYPE_FACTORY)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: it's already %s mode!! : \n", __func__);
        return;
    }

/*                                                                                                     */
#if 0
    if(charging_ic_status != POWER_SUPPLY_TYPE_BATTERY)
    {
        charging_ic_deactive();
    }
#endif
/*                                                                                                     */

/*                                                                                                              */
#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER
    mutex_lock(&charging_lock);
    wait = 0;
    while (wait < 10)
    {
        if (Set_Charging_Commmand(CHARGING_SET_FACTORY_MODE_COMMAND) == KAL_TRUE)
        {
            break;
        }
        wait++;
        mdelay(10);
    }
    charging_ic_status = POWER_SUPPLY_TYPE_FACTORY;
    mutex_unlock(&charging_lock);
#else

    mutex_lock(&charging_lock);

//                                                                                    
#if defined(SINGLE_CHARGER_CONTROL_USING_SPIN_LOCK)
    charging_ic_set_chargingmode(CHR_FACTORY_MODE);
#else
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(400);
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    udelay(1800);

    /*                                                                                       */
    high_volt_setting();
    /*                                                                                       */
#endif
//                                                                                    

    charging_ic_status = POWER_SUPPLY_TYPE_FACTORY;

    mutex_unlock(&charging_lock);
#endif
/*                                                                                                              */

    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: %s : \n", __func__);
}
EXPORT_SYMBOL(charging_ic_set_factory_mode);

void charging_ic_deactive()
{
    u32 wait = 0;

    if(charging_ic_status == POWER_SUPPLY_TYPE_BATTERY)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: it's already %s mode!! : \n", __func__);
        return;
    }
/*                                                                                                              */
#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER
    mutex_lock(&charging_lock);
    wait = 0;
    while (wait < 10)
    {
        if (Set_Charging_Commmand(CHARGING_DEACTIVE_COMMAND) == KAL_TRUE)
        {
            break;
        }
        wait++;
        mdelay(10);
    }
    charging_ic_status = POWER_SUPPLY_TYPE_BATTERY;
    mutex_unlock(&charging_lock);
#else

    mutex_lock(&charging_lock);

//                                                                                    
#if defined(SINGLE_CHARGER_CONTROL_USING_SPIN_LOCK)
    charging_ic_set_chargingmode(CHR_DEACTIVE_MODE);
#else
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    udelay(2000);
#endif
//                                                                                    

    charging_ic_status = POWER_SUPPLY_TYPE_BATTERY;

    mutex_unlock(&charging_lock);
#endif
/*                                                                                                              */

    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: %s : \n", __func__);
}
EXPORT_SYMBOL(charging_ic_deactive);

kal_bool is_charging_ic_enable(void)
{
    if(mt_get_gpio_out(CHG_EN_SET_N))
    {
        return KAL_FALSE;  // charging disable
    }
    else
    {
        return KAL_TRUE;  // charging enable
    }
}
EXPORT_SYMBOL(is_charging_ic_enable);



static void charging_ic_initialize(void)
{
    charging_ic_status = POWER_SUPPLY_TYPE_BATTERY;
}

static irqreturn_t charging_ic_interrupt_handler(int irq, void *data)
{
    ;
}

static void charging_timer_work(struct work_struct *work)
{
    ;
}

/*                                                                                                              */
#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER
int Set_Charging_Commmand(int nCommand)
{
    int nRet = KAL_FALSE;
    mutex_lock(&charging_timer_api_lock);

    if (charger_command_status == CHARGER_COMMAND_STOP)
    {
        switch (nCommand)
        {
            case CHARGING_SET_USB_500_COMMAND :
            case CHARGING_SET_TA_MODE_COMMAND :
            case CHARGING_SET_USB_100_COMMAND :
            case CHARGING_SET_FACTORY_MODE_COMMAND :
            case CHARGING_DEACTIVE_COMMAND :
                charger_command = nCommand;
                charger_command_status = CHARGER_COMMAND_RUN;
                charger_command_execute_index = 0;
                nRet = KAL_TRUE;
                break;
            default :
                charger_command = CHARGING_UNKNOWN_COMMAND;
                charger_command_status = CHARGER_COMMAND_STOP;
                charger_command_execute_index = 0;
                nRet = KAL_FALSE;
                break;
        }
        if (nRet == KAL_TRUE)
        {
            if (charger_command_list[charger_command].cmd_interval[charger_command_execute_index].is_high)
            {
                mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
            }
            else
            {
                mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
            }
            config.u4CompareH = 0;
            config.u4CompareL = charger_command_list[charger_command].cmd_interval[charger_command_execute_index].delay_count;
            charger_command_execute_index++;

/*                                                                                                           */
            GPT_Config(config);
/*                                                                                                           */

            GPT_ClearCount(gpt_num);
            GPT_SetCompare(gpt_num, config.u4CompareL);
            GPT_Restart(gpt_num);
        }
    }

    mutex_unlock(&charging_timer_api_lock);

    return nRet;
}



void gpt_timer_irq_handler(UINT16 i)
{
    /*
        Do not added printf or log print in the function because it is interrupt service routine.
    */
    spin_lock_irqsave(&g_single_charger_lock, g_single_flags);

    // end command
    if (charger_command_list[charger_command].cmd_interval[charger_command_execute_index].delay_count == 0)
    {
        charger_command = CHARGING_UNKNOWN_COMMAND;
        charger_command_status = CHARGER_COMMAND_STOP;
        charger_command_execute_index = 0;
        spin_unlock_irqrestore(&g_single_charger_lock, g_single_flags);
        return;
    }

    if (charger_command_list[charger_command].cmd_interval[charger_command_execute_index].is_high)
    {
/*                                                                                 */
        mt_set_gpio_pull_select(CHG_EN_SET_N, GPIO_PULL_UP);
/*                                                                                 */
        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ONE);
    }
    else
    {
/*                                                                                 */
        mt_set_gpio_pull_select(CHG_EN_SET_N, GPIO_PULL_DOWN);
/*                                                                                 */
        mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    }
    config.u4CompareH = 0;
    config.u4CompareL = charger_command_list[charger_command].cmd_interval[charger_command_execute_index].delay_count;
    charger_command_execute_index++;

/*                                                                                                           */
    GPT_Config(config);
/*                                                                                                           */

    GPT_ClearCount(gpt_num);
    GPT_SetCompare(gpt_num, config.u4CompareL);
    GPT_Restart(gpt_num);

    spin_unlock_irqrestore(&g_single_charger_lock, g_single_flags);
}
#endif
/*                                                                                                              */

static int charging_ic_probe(struct platform_device *dev)
{
    int ret = 0;
/*                                                                                                              */
#ifdef SINGLE_CHARGER_CONTOL_USING_GPT_TIMER

    /* 1 Counter : about 0.0769 us */
    GPT_CLK_SRC clkSrc = GPT_CLK_SRC_SYS;
    GPT_CLK_DIV clkDiv = GPT_CLK_DIV_1;

    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: charging_ic_probe is done\n");

    GPT_Init (gpt_num, gpt_timer_irq_handler);

    config.num = gpt_num;
    config.mode = GPT_ONE_SHOT;
    config.clkSrc = clkSrc;
    config.clkDiv = clkDiv;
    config.u4CompareL = 13000000;           // 1 S
    config.u4CompareH = 0;
    config.bIrqEnable = TRUE;

    if (GPT_Config(config) == FALSE)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: charging_ic_probe is failed because gpt timer failed !!!\n");
        return -1;
    }

#endif
/*                                                                                                              */

    mt_set_gpio_mode(CHG_EN_SET_N, CHG_EN_MODE);
    mt_set_gpio_dir(CHG_EN_SET_N, CHG_EN_DIR);
    mt_set_gpio_out(CHG_EN_SET_N, CHG_EN_DATA_OUT);

/*                                                                                 */
    mt_set_gpio_out(CHG_EN_SET_N, GPIO_OUT_ZERO);
    mt_set_gpio_pull_enable(CHG_EN_SET_N, CHG_EOC_PULL_ENABLE);
    mt_set_gpio_pull_select(CHG_EN_SET_N, GPIO_PULL_DOWN);
/*                                                                                 */

    mt_set_gpio_mode(CHG_EOC_N, CHG_EOC_MODE);
    mt_set_gpio_dir(CHG_EOC_N, CHG_EOC_DIR);
    mt_set_gpio_pull_enable(CHG_EOC_N, CHG_EOC_PULL_ENABLE);
    mt_set_gpio_pull_select(CHG_EOC_N, CHG_EOC_PULL_SELECT);

    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: charging IC Initialization is done\n");

    return 0;
}

static int charging_ic_remove(struct platform_device *dev)
{
    charging_ic_deactive();

    return 0;
}
#endif //                                              

static int charging_ic_suspend(struct platform_device *dev, pm_message_t state)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: charging_ic_suspend \n");
    dev->dev.power.power_state = state;
    return 0;
}

static int charging_ic_resume(struct platform_device *dev)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "[charger_rt9536] :: charging_ic_resume \n");
    dev->dev.power.power_state = PMSG_ON;
    return 0;
}

static struct platform_driver charging_ic_driver = {
    .probe = charging_ic_probe,
    .remove = charging_ic_remove,
//    .suspend = charging_ic_suspend,
//    .resume = charging_ic_resume,
    .driver = {
        .name = "single_charger",
    },
};


static int __init charging_ic_init(void)
{
   xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "LGE : Charging IC Driver Init \n");
    return platform_driver_register(&charging_ic_driver);
}

static void __exit charging_ic_exit(void)
{
   xlog_printk(ANDROID_LOG_INFO, "Power/Charger", "LGE : Charging IC Driver Exit \n");
    platform_driver_unregister(&charging_ic_driver);
}

module_init(charging_ic_init);
module_exit(charging_ic_exit);

MODULE_AUTHOR("jongwoo82.lee@lge.com");
MODULE_DESCRIPTION("charging_ic Driver");
MODULE_LICENSE("GPL");
