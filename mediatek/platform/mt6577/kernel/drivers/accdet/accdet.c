#include "accdet.h"
#include "mach/mt_boot.h"
#include <cust_eint.h>
#include <mach/mt_gpio.h>


/*                                                                                  */
extern unsigned char g_qem_check;
/*                                                                                  */
/*----------------------------------------------------------------------
static variable defination
----------------------------------------------------------------------*/
#define REGISTER_VALUE(x)   (x - 1)

static struct switch_dev accdet_data;
static struct input_dev *kpd_accdet_dev;
static struct cdev *accdet_cdev;
static struct class *accdet_class = NULL;
static struct device *accdet_nor_device = NULL;

static dev_t accdet_devno;

static int pre_status = 0;
static int pre_state_swctrl = 0;
static int accdet_status = PLUG_OUT;
static int cable_type = 0;
static s64 long_press_time_ns = 0 ;

static int g_accdet_first = 1;
static bool IRQ_CLR_FLAG = FALSE;
static volatile int call_status =0;
static volatile int button_status = 0;
static int tv_out_debounce = 0;

struct wake_lock accdet_suspend_lock;
struct wake_lock accdet_irq_lock;
struct wake_lock accdet_key_lock;

static struct work_struct accdet_work;
static struct workqueue_struct * accdet_workqueue = NULL;
static int g_accdet_working_in_suspend =0;

//#define SUPPORT_FACTORY_MODE
#ifdef SUPPORT_FACTORY_MODE
/* For support factory mode */
extern unsigned char g_qem_check;
#endif /* SUPPORT_FACTORY_MODE */

#ifdef ACCDET_EINT
//static int g_accdet_working_in_suspend =0;

static struct work_struct accdet_eint_work;
static struct workqueue_struct * accdet_eint_workqueue = NULL;

static inline void accdet_init(void);

extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif


#define ENABLE_ACCDET_DEBUG

#define ACCDET_TAG "[ACCDET] "

#define ACCDET_ERR(fmt, args...)    printk(KERN_ERR ACCDET_TAG"[ERROR] %s() line=%d : "fmt, __FUNCTION__, __LINE__, ##args)

#if defined ( ENABLE_ACCDET_DEBUG )
/* You need to select proper loglevel to see the log what you want. ( Currently, you can't see "KERN_INFO" level ) */
#define ACCDET_FUN(f)  	   printk(KERN_ERR ACCDET_TAG"%s()\n", __FUNCTION__)
#define ACCDET_LOG(fmt, args...)    printk(KERN_ERR ACCDET_TAG fmt, ##args)
#define ACCDET_DBG(fmt, args...)    printk(KERN_INFO ACCDET_TAG fmt, ##args)
#else
#define ACCDET_FUN(f)  	   printk(KERN_INFO ACCDET_TAG"%s()\n", __FUNCTION__)
#define ACCDET_LOG(fmt, args...)    printk(KERN_INFO ACCDET_TAG fmt, ##args)
#define ACCDET_DBG(fmt, args...)    printk(KERN_INFO ACCDET_TAG fmt, ##args)
#endif



static volatile int double_check_flag = 0;
static bool tv_headset_icon = false;
static int button_press_debounce = 0x400;

#define DEBUG_THREAD 1
static struct platform_driver accdet_driver;

static bool jack_detect_line_status = 1 ; /* 1 = high ( plugged-out ) */
static int jack_type = 0; /* 0 = no headset , 1 = headset with mic, 2 = headset without mic */
static bool hook_key_pressed = 0; /* 1 = pressed, 0 = released */
static bool is_ready_to_detect_jack_type = 0;
#define HOOK_DETECT_DEBOUNCE_DOWN 0x1000 /* 0x400 = 32ms */
#define HOOK_DETECT_DEBOUNCE_UP 0x400 /* 0x400 = 32ms */
#define JACK_DETECT_DEBOUNCE 0x2000 /* 0x2000 = 250ms */
#define ACCDET_EINT_LEVEL 0
static inline void clear_accdet_interrupt(void);
int check_headset_type(void);

/****************************************************************/
/***export function, for tv out driver                                                                     **/
/****************************************************************/
void switch_asw_to_tv(bool tv_enable)
{
	ACCDET_LOG("[Accdet]switch analog switch to tv is %d\n",tv_enable);
	if(tv_enable)
	{
		SETREG32(ACCDET_STATE_SWCTRL,TV_DET_BIT);
		hwSPKClassABAnalogSwitchSelect(ACCDET_TV_CHA);
	}
	else
	{
		CLRREG32(ACCDET_STATE_SWCTRL,TV_DET_BIT);
		hwSPKClassABAnalogSwitchSelect(ACCDET_MIC_CHA);
	}
}
EXPORT_SYMBOL(switch_asw_to_tv);

void switch_NTSC_to_PAL(int mode)
{

#ifdef TV_OUT_SUPPORT
    if((mode < 0)||(mode > 1))
    {
        ACCDET_LOG("[Accdet]switch_NTSC_to_PAL:tv mode is invalid: %d!\n", mode);
    }
    else
    {
        ACCDET_LOG("[Accdet]switch_NTSC_to_PAL:%s MODE!\n", (mode? "PAL":"NSTC"));

        // init the TV out cable detection relative register
        OUTREG32(ACCDET_TV_START_LINE0,cust_tv_settings[mode].start_line0);
	    OUTREG32(ACCDET_TV_END_LINE0,cust_tv_settings[mode].end_line0);
	    OUTREG32(ACCDET_TV_START_LINE1,cust_tv_settings[mode].start_line1);
	    OUTREG32(ACCDET_TV_END_LINE1,cust_tv_settings[mode].end_line1);
	    OUTREG32(ACCDET_TV_PRE_LINE,cust_tv_settings[mode].pre_line);
	    OUTREG32(ACCDET_TV_START_PXL,cust_tv_settings[mode].start_pixel);
	    OUTREG32(ACCDET_TV_END_PXL,cust_tv_settings[mode].end_pixel);

	    OUTREG32(ACCDET_TV_EN_DELAY_NUM,
            (cust_tv_settings[mode].fall_delay << 10|cust_tv_settings[mode].rise_delay));
      	
        //set div and debounce in TV-out mode
        OUTREG32(ACCDET_TV_DIV_RATE,cust_tv_settings[mode].div_rate);
        OUTREG32(ACCDET_DEBOUNCE2, cust_tv_settings[mode].debounce);
		tv_out_debounce = cust_tv_settings[mode].debounce;
    }

#endif

    return;

}
EXPORT_SYMBOL(switch_NTSC_to_PAL);


void accdet_detect(void)
{
	int ret = 0 ;

	ACCDET_LOG("[Accdet]accdet_detect\n");

	accdet_status = PLUG_OUT;
    ret = queue_work(accdet_workqueue, &accdet_work);
    if(!ret)
    {
  		ACCDET_LOG("[Accdet]accdet_detect:accdet_work return:%d!\n", ret);
    }

	return;
}
EXPORT_SYMBOL(accdet_detect);

void accdet_state_reset(void)
{

	ACCDET_LOG("[Accdet]accdet_state_reset\n");

	accdet_status = PLUG_OUT;
	cable_type = NO_DEVICE;
#if 1 //                                              
#else
	enable_tv_allwhite_signal(false);
	enable_tv_detect(false);
#endif //                                              
#ifdef ACCDET_LOW_POWER
   //  __accdet_state_reset();
#endif
	return;
}
EXPORT_SYMBOL(accdet_state_reset);


/****************************************************************/
/*******static function defination  **/
/****************************************************************/

void inline disable_accdet(void)
{
	ACCDET_FUN();
	
	OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
	OUTREG32(ACCDET_STATE_SWCTRL, 0);
}

void inline enable_accdet(u32 state_swctrl)
{
	ACCDET_FUN();
	
	OUTREG32(ACCDET_RSTB, RSTB_BIT);
	OUTREG32(ACCDET_RSTB, RSTB_FINISH_BIT);
	
	OUTREG32(ACCDET_STATE_SWCTRL, state_swctrl);
	OUTREG32(ACCDET_CTRL, ACCDET_ENABLE);
}

void accdet_eint_work_callback(struct work_struct *work)
{
	int type = 0;
	
	ACCDET_FUN();

    kal_bool gpio_state = KAL_FALSE;

    wake_lock(&accdet_suspend_lock);

    gpio_state = mt_get_gpio_in(GPIO_ACCDET_EINT_PIN); /* 0 = low ( plugged-in ) */
    ACCDET_LOG("GPIO_ACCDET_EINT = %d\n", gpio_state);

	if ( jack_detect_line_status != gpio_state )
	{
		jack_detect_line_status = gpio_state;
		
		if ( jack_detect_line_status == 0 )
		{
		    ACCDET_LOG("Jack detect line changed to low\n");

			is_ready_to_detect_jack_type = 0;
			
			enable_accdet(ACCDET_SWCTRL_EN); /* enable hook detection */

			type = check_headset_type();

			if ( type == HEADSET_MIC )
			{
				ACCDET_LOG("JACK_TYPE = HEADSET_MIC\n");
			}
			else
			{
				ACCDET_LOG("JACK_TYPE = HEADSET_NO_MIC\n");
			}

		    OUTREG32(ACCDET_DEBOUNCE0, HOOK_DETECT_DEBOUNCE_DOWN);
		    OUTREG32(ACCDET_DEBOUNCE1, HOOK_DETECT_DEBOUNCE_UP);
			
		}
		else
		{
		    ACCDET_LOG("Jack detect line changed to high\n");
			
			if ( ( jack_type == HEADSET_MIC ) && ( hook_key_pressed == 1 ) )
			{
				ACCDET_LOG("HOOK_SWITCH = KEY_UP ( forced release caused by jack plugged-out\n");
				
				hook_key_pressed = 0;
				
                input_report_key(kpd_accdet_dev, KEY_MEDIA, hook_key_pressed);
                input_sync(kpd_accdet_dev);
			}
			
			type = NO_DEVICE;

			ACCDET_LOG("JACK_TYPE = NO_DEVICE\n");
			
		    OUTREG32(ACCDET_DEBOUNCE0, JACK_DETECT_DEBOUNCE);
		    OUTREG32(ACCDET_DEBOUNCE1, JACK_DETECT_DEBOUNCE);

			disable_accdet(); /* disable hook detection */
			
		}

		jack_type = type;
		
		switch_set_state((struct switch_dev *)&accdet_data, (int)type);

		mt65xx_eint_set_polarity(CUST_EINT_ACCDET_NUM, !jack_detect_line_status);
		
	}
	else
	{
		ACCDET_LOG("Useless interrupt caused by noise, so skip\n");
	}
	
	mt65xx_eint_unmask(CUST_EINT_ACCDET_NUM);  

    wake_unlock(&accdet_suspend_lock);

}


void accdet_eint_func(void)
{
	int ret=0;
	ACCDET_FUN();

	ret = queue_work(accdet_eint_workqueue, &accdet_eint_work);	
    if(!ret)
    {
  		ACCDET_ERR("failed to add work in eint work queue ( err = %d )\n", ret);  		
    }
}


static inline int accdet_setup_eint(void)
{
	ACCDET_FUN();
	
	mt_set_gpio_mode(GPIO_ACCDET_EINT_PIN, GPIO_ACCDET_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ACCDET_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ACCDET_EINT_PIN, GPIO_PULL_DISABLE); //To disable GPIO PULL.
	
    mt65xx_eint_set_sens(CUST_EINT_ACCDET_NUM, CUST_EINT_LEVEL_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ACCDET_NUM, CUST_EINT_POLARITY_LOW);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ACCDET_NUM, 30);
	mt65xx_eint_registration(CUST_EINT_ACCDET_NUM, CUST_EINT_ACCDET_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, accdet_eint_func, 0);
	mt65xx_eint_unmask(CUST_EINT_ACCDET_NUM);  
	
    return 0;

}

static inline void clear_accdet_interrupt(void)
{
	int loop_count = 0;
	
	ACCDET_FUN();

	SETREG32(ACCDET_IRQ_STS, (IRQ_CLR_BIT));
}

int check_headset_type(void)
{
    int regVal = 0;
	int type = HEADSET_MIC;
	int loop_count = 0;

	ACCDET_FUN();

	while( is_ready_to_detect_jack_type == 0 && loop_count < 30 ) /* need to set maximun wait time is under 5 sec */
	{
	    ACCDET_LOG("ACCDET_IRQ_STS = 0x%x, SI = %d, CI = %d, AB = %d, Loop Count = %d\n", INREG32(ACCDET_IRQ_STS), INREG32(ACCDET_SAMPLE_IN), INREG32(ACCDET_CURR_IN), INREG32(ACCDET_MEMORIZED_IN) & 0x3, loop_count);
		msleep(50);
		loop_count++;
	}

	if( is_ready_to_detect_jack_type == 1 )
	{
		ACCDET_LOG("ACCDET is ready so read status to check jack type\n");
		regVal = INREG32(ACCDET_MEMORIZED_IN) & 0x3; //A=bit1; B=bit0
	    if ( regVal == 0 )
	    {
			type = HEADSET_NO_MIC;
		    ACCDET_LOG("HEADSET_NO_MIC was detected\n");
	    }
		else if ( regVal == 1 )
		{
	     	type = HEADSET_MIC;
		    ACCDET_LOG("HEADSET_MIC was detected\n");
		}
		else
		{
	     	type = HEADSET_MIC;
		    ACCDET_ERR("Invalid register value of AB = %d, but assume that HEADSET_MIC was detected\n", regVal);
		}
	}
	else
	{
     	type = HEADSET_MIC;
	    ACCDET_ERR("ACCDET is not ready than failed to detect headset type, but assume that HEADSET_MIC was detected\n", regVal);
	}
	
	return type;
	
}

void check_hook_key(void)
{
    int current_status = 0;
	int type = 0;

	ACCDET_FUN();
	
    current_status = INREG32(ACCDET_MEMORIZED_IN) & 0x3; //A=bit1; B=bit0
    ACCDET_LOG("ACCDET_MEMORIZED_IN : AB = %d\n", current_status);

	if ( jack_type == NO_DEVICE )
	{
		is_ready_to_detect_jack_type = 1;
		ACCDET_LOG("Now ready to detect headset type\n");
	}	
	else if ( jack_type == HEADSET_MIC )
	{
	    
	    if ( current_status == 0 )
	    {
			if ( hook_key_pressed == 0 )
			{
				hook_key_pressed = 1;
                input_report_key(kpd_accdet_dev, KEY_MEDIA, hook_key_pressed);
                input_sync(kpd_accdet_dev);
				ACCDET_LOG("HOOK_SWITCH = KEY_DOWN\n");
				
				wake_lock_timeout(&accdet_key_lock, 2*HZ);    //set the wake lock
			}
			else
			{
				ACCDET_LOG("Unknown state ( Duplicated KEY_DOWN )\n");
			}
	    }
		else if ( current_status == 1 )
		{
			if ( hook_key_pressed == 1 )
			{
				hook_key_pressed = 0;
                input_report_key(kpd_accdet_dev, KEY_MEDIA, hook_key_pressed);
                input_sync(kpd_accdet_dev);
				ACCDET_LOG("HOOK_SWITCH = KEY_UP\n");
			}
			else
			{
				ACCDET_LOG("Unknown state ( Duplicated KEY_UP )\n");
			}
		}
		else
		{
			if ( hook_key_pressed == 1 )
			{
				hook_key_pressed = 0;
                input_report_key(kpd_accdet_dev, KEY_MEDIA, hook_key_pressed);
                input_sync(kpd_accdet_dev);
				ACCDET_LOG("HOOK_SWITCH = KEY_UP\n");
			}
			else
			{
				ACCDET_LOG("Unknown state ( AB = %d )\n", current_status);
			}
		}
	}
	//                                                                             
	else if ( jack_type == HEADSET_NO_MIC)
	{
		if ( current_status == 1 )
		{
			 jack_type = HEADSET_MIC;
			 switch_set_state((struct switch_dev *)&accdet_data, (int)NO_DEVICE);
			 switch_set_state((struct switch_dev *)&accdet_data, (int)jack_type);
			 ACCDET_LOG("assume that jack_type is HEADSET_MIC\n");
		}
	}
	//                                                                             
	else
	{
		ACCDET_LOG("Unknown state ( jack_type = %d, AB = %d )\n", jack_type, current_status);
	}

	/* clear accdet interrupt */
    while(INREG32(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
	{
		ACCDET_LOG("ACCDET_IRQ_STS = 0x%x (   clearing )\n", INREG32(ACCDET_IRQ_STS));
		msleep(10);
	}
	CLRREG32(ACCDET_IRQ_STS, IRQ_CLR_BIT);
	
}


void accdet_work_callback(struct work_struct *work)
{
	ACCDET_FUN();
	
    wake_lock(&accdet_irq_lock);

    check_hook_key();

    wake_unlock(&accdet_irq_lock);
}


static irqreturn_t accdet_irq_handler(int irq,void *dev_id)
{
    int ret = 0 ;

	ACCDET_FUN();

    clear_accdet_interrupt();
	
    ret = queue_work(accdet_workqueue, &accdet_work);	
    if(!ret)
    {
  		ACCDET_ERR("failed to add work in accdet work queue ( err = %d )\n", ret);  		
    }

    return IRQ_HANDLED;
}


static inline void accdet_init(void)
{
	ACCDET_FUN();
    //kal_uint8 val;

    //resolve MT6573 ACCDET hardware issue: ear bias supply can make Vref drop obviously
    //during plug in/out headset then cause modem exception or kernel panic
    //solution: set bit3 of PMIC_RESERVE_CON2 to force MIC voltage to 0 when MIC is drop to negative voltage
    //SETREG32(PMIC_RESERVE_CON2, MIC_FORCE_LOW);

    enable_clock(MT65XX_PDN_PERI_ACCDET,"ACCDET");
	
    //reset the accdet unit
	OUTREG32(ACCDET_RSTB, RSTB_BIT);
	OUTREG32(ACCDET_RSTB, RSTB_FINISH_BIT);

    //init  pwm frequency and duty
    OUTREG32(ACCDET_PWM_WIDTH, REGISTER_VALUE(cust_headset_settings.pwm_width));
    OUTREG32(ACCDET_PWM_THRESH, REGISTER_VALUE(cust_headset_settings.pwm_thresh));

    OUTREG32(ACCDET_EN_DELAY_NUM,
		(cust_headset_settings.fall_delay << 15 | cust_headset_settings.rise_delay));

    // init the debounce time
	#if 1 //                                              
    OUTREG32(ACCDET_DEBOUNCE0, JACK_DETECT_DEBOUNCE);
    OUTREG32(ACCDET_DEBOUNCE1, JACK_DETECT_DEBOUNCE);
	#else
    OUTREG32(ACCDET_DEBOUNCE0, cust_headset_settings.debounce0);
    OUTREG32(ACCDET_DEBOUNCE1, cust_headset_settings.debounce1);
	#endif //                                              
    OUTREG32(ACCDET_DEBOUNCE3, cust_headset_settings.debounce3);	
	
		
    //init TV relative register settings, default setting is NTSC standard
    switch_NTSC_to_PAL(0);

    #ifdef ACCDET_EINT
    // disable ACCDET unit
    OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
    OUTREG32(ACCDET_STATE_SWCTRL, 0);
	#else
    // enable ACCDET unit
    OUTREG32(ACCDET_STATE_SWCTRL, ACCDET_SWCTRL_EN);
    OUTREG32(ACCDET_CTRL, ACCDET_ENABLE);
	#endif

	
	if(get_chip_eco_ver()==CHIP_E1)
	{
	    //************analog switch*******************/
	    //**MT6577 EVB**: 1, MIC channel; 0, TV channel (default)
	    //**MT6577 Phone**: 1, TV channel; 0, MIC channel(default)
	    #ifndef MT6575_EVB_MIC_TV_CHA
	    hwSPKClassABAnalogSwitchSelect(ACCDET_MIC_CHA);
	    #else
	    hwSPKClassABAnalogSwitchSelect(0x1);
	    #endif
	    //pmic_bank1_read_interface(0x5F, &val, 1, 0);
	    ACCDET_LOG("eco version E1\n");
	}
	else if(get_chip_eco_ver()==CHIP_E2)
	{
		hwSPKClassABAnalogSwitchSelect(ACCDET_MIC_CHA);
		ACCDET_LOG("eco version E2\n");
	}
	else
	{
		hwSPKClassABAnalogSwitchSelect(ACCDET_MIC_CHA);
		ACCDET_LOG("unknown eco version\n");
	}

}

static long accdet_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
//	bool ret = true;
		
    switch(cmd)
    {
        case ACCDET_INIT :

/* the old ACCDE driver legacy */
#if 0
 		    ACCDET_LOG("[Accdet]accdet_ioctl : ACCDET_INIT\n");
			if (g_accdet_first == 1)
			{	
				long_press_time_ns = (s64)long_press_time * NSEC_PER_MSEC;
				
				//Accdet Hardware Init
				accdet_init();

				//mt6577_irq_set_sens(MT6577_ACCDET_IRQ_ID, MT65xx_EDGE_SENSITIVE);
				mt6577_irq_set_sens(MT6577_ACCDET_IRQ_ID, MT65xx_LEVEL_SENSITIVE);
				mt6577_irq_set_polarity(MT6577_ACCDET_IRQ_ID, MT65xx_POLARITY_LOW);
				//register accdet interrupt
				ret =  request_irq(MT6577_ACCDET_IRQ_ID, accdet_irq_handler, 0, "ACCDET", NULL);
				if(ret)
				{
					ACCDET_LOG("[Accdet]accdet register interrupt error\n");
				}

				queue_work(accdet_workqueue, &accdet_work); //schedule a work for the first detection					
				g_accdet_first = 0;
			}
#endif
     		break;

		case SET_CALL_STATE :
			call_status = (int)arg;
			ACCDET_LOG("[Accdet]accdet_ioctl : CALL_STATE=%d \n", call_status);
			/*
            if(call_status != 0)
            {
                wake_lock(&accdet_suspend_lock);
                ACCDET_LOG("[Accdet]accdet_ioctl : CALL_STATE=%d,require wake lock\n", call_status);
            }
            else
            {
                wake_unlock(&accdet_suspend_lock);
                ACCDET_LOG("[Accdet]accdet_ioctl : CALL_STATE=%d,release wake lock\n", call_status);
            }
			*/
			
			break;

		case GET_BUTTON_STATUS :
			ACCDET_LOG("[Accdet]accdet_ioctl : Button_Status=%d (state:%d)\n", button_status, accdet_data.state);	
			return button_status;

		default:
   		    ACCDET_LOG("[Accdet]accdet_ioctl : default\n");
            break;
  }
    return 0;
}

static int accdet_open(struct inode *inode, struct file *file)
{
   	return 0;
}

static int accdet_release(struct inode *inode, struct file *file)
{
    return 0;
}
static struct file_operations accdet_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl		= accdet_unlocked_ioctl,
	.open		= accdet_open,
	.release	= accdet_release,	
};

#if DEBUG_THREAD
int g_start_debug_thread =0;
static struct task_struct *thread = NULL;
int g_dump_register=0;

static int dump_register(void)
{

   int i=0;
   for (i=0; i<= 120; i+=4)
   {
     ACCDET_LOG(" ACCDET_BASE + %x=%x\n",i,INREG32(ACCDET_BASE + i));
   }

 OUTREG32(0xf0007420,0x0111);
 // wirte c0007424 x0d01
 OUTREG32(0xf0007424,0x0d01);

   ACCDET_LOG(" 0xc1016d0c =%x\n",INREG32(0xf1016d0c));
   ACCDET_LOG(" 0xc1016d10 =%x\n",INREG32(0xf1016d10));
   ACCDET_LOG(" 0xc209e070 =%x\n",INREG32(0xf209e070));
   ACCDET_LOG(" 0xc0007160 =%x\n",INREG32(0xf0007160));
   ACCDET_LOG(" 0xc00071a8 =%x\n",INREG32(0xf00071a8));
   ACCDET_LOG(" 0xc0007440 =%x\n",INREG32(0xf0007440));

   return 0;
}


extern int mt_i2c_polling_writeread(int port, unsigned char addr, unsigned char *buffer, int write_len, int read_len);

static int dump_pmic_register(void)
{
	int ret = 0;
	unsigned char data[2];
	
	//u8 data = 0x5f;
	data[0] = 0x5f;
	ret = mt_i2c_polling_writeread(2,0xc2,data,1,1);
	if(ret > 0)
	{
       ACCDET_LOG("dump_pmic_register i2c error");
	}
    ACCDET_LOG("dump_pmic_register 0x5f= %x",data[0]);

    data[0] = 0xc8;
	ret = mt_i2c_polling_writeread(2,0xc0,data,1,1);
	if(ret > 0)
	{
       ACCDET_LOG("dump_pmic_register i2c error");
	}
    ACCDET_LOG("dump_pmic_register 0xc8= %x",data[0]);

	return 0;
}
static int dbug_thread(void *unused)
{
   while(g_start_debug_thread)
   	{
      ACCDET_LOG("dbug_thread INREG32(ACCDET_BASE + 0x0008)=%x\n",INREG32(ACCDET_BASE + 0x0008));
	  ACCDET_LOG("[Accdet]dbug_thread:sample_in:%x!\n", INREG32(ACCDET_SAMPLE_IN));	
	  ACCDET_LOG("[Accdet]dbug_thread:curr_in:%x!\n", INREG32(ACCDET_CURR_IN));
	  ACCDET_LOG("[Accdet]dbug_thread:mem_in:%x!\n", INREG32(ACCDET_MEMORIZED_IN));
	  ACCDET_LOG("[Accdet]dbug_thread:FSM:%x!\n", INREG32(ACCDET_BASE + 0x0050));
      ACCDET_LOG("[Accdet]dbug_thread:IRQ:%x!\n", INREG32(ACCDET_IRQ_STS));
      if(g_dump_register)
	  {
	    dump_register();
		dump_pmic_register();
      }

	  msleep(500);

   	}
   return 0;
}
//static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)


static ssize_t store_accdet_start_debug_thread(struct device_driver *ddri, const char *buf, size_t count)
{
	
	unsigned int start_flag;
	int error;

	if (sscanf(buf, "%u", &start_flag) != 1) {
		ACCDET_LOG("accdet: Invalid values\n");
		return -EINVAL;
	}

	ACCDET_LOG("[Accdet] start flag =%d \n",start_flag);

	g_start_debug_thread = start_flag;

    if(1 == start_flag)
    {
	   thread = kthread_run(dbug_thread, 0, "ACCDET");
       if (IS_ERR(thread))
	   {
          error = PTR_ERR(thread);
          ACCDET_LOG( " failed to create kernel thread: %d\n", error);
       }
    }

	return count;
}

static ssize_t store_accdet_set_headset_mode(struct device_driver *ddri, const char *buf, size_t count)
{

    unsigned int value;
	//int error;

	if (sscanf(buf, "%u", &value) != 1) {
		ACCDET_LOG("accdet: Invalid values\n");
		return -EINVAL;
	}

	ACCDET_LOG("[Accdet]store_accdet_set_headset_mode value =%d \n",value);

	return count;
}

static ssize_t store_accdet_dump_register(struct device_driver *ddri, const char *buf, size_t count)
{
    unsigned int value;
//	int error;

	if (sscanf(buf, "%u", &value) != 1)
	{
		ACCDET_LOG("accdet: Invalid values\n");
		return -EINVAL;
	}

	g_dump_register = value;

	ACCDET_LOG("[Accdet]store_accdet_dump_register value =%d \n",value);

	return count;
}




/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(dump_register,      S_IWUSR | S_IRUGO, NULL,         store_accdet_dump_register);

static DRIVER_ATTR(set_headset_mode,      S_IWUSR | S_IRUGO, NULL,         store_accdet_set_headset_mode);

static DRIVER_ATTR(start_debug,      S_IWUSR | S_IRUGO, NULL,         store_accdet_start_debug_thread);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_start_debug,
	&driver_attr_set_headset_mode,
	&driver_attr_dump_register,
};

static int accdet_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(accdet_attr_list)/sizeof(accdet_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, accdet_attr_list[idx])))
		{
			ACCDET_LOG("driver_create_file (%s) = %d\n", accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
#if 0
static int accdet_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(accdet_attr_list)/sizeof(accdet_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, accdet_attr_list[idx]);
	}
	

	return err;
}

#endif

#endif

#if 0
void  temp_func()
{
  //unsigned int start_flag;
	int error;

	

	ACCDET_LOG("[Accdet] start flag =%d \n",start_flag);

	

    if(1)
    {
	   thread = kthread_run(dbug_thread, 0, "ACCDET");
       if (IS_ERR(thread))
	   {
          error = PTR_ERR(thread);
          ACCDET_LOG( " failed to create kernel thread: %d\n", error);
       }
    }
}

#endif

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
		

static int accdet_probe(struct platform_device *dev)	
{
	int ret = 0;
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
     struct task_struct *keyEvent_thread = NULL;
	 int error=0;
#endif
	ACCDET_LOG("[Accdet]accdet_probe begin!\n");

	//------------------------------------------------------------------
	// 							below register accdet as switch class
	//------------------------------------------------------------------	
	accdet_data.name = "h2w";
	accdet_data.index = 0;
	accdet_data.state = NO_DEVICE;
			
	ret = switch_dev_register(&accdet_data);
	if(ret)
	{
		ACCDET_LOG("[Accdet]switch_dev_register returned:%d!\n", ret);
		return 1;
	}
		
	//------------------------------------------------------------------
	// 							Create normal device for auido use
	//------------------------------------------------------------------
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret)
	{
		ACCDET_LOG("[Accdet]alloc_chrdev_region: Get Major number error!\n");			
	}
		
	accdet_cdev = cdev_alloc();
    accdet_cdev->owner = THIS_MODULE;
    accdet_cdev->ops = &accdet_fops;
    ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if(ret)
	{
		ACCDET_LOG("[Accdet]accdet error: cdev_add\n");
	}
	
	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);

    // if we want auto creat device node, we must call this
	accdet_nor_device = device_create(accdet_class, NULL, accdet_devno, NULL, ACCDET_DEVNAME);
	
	//------------------------------------------------------------------
	// 							Create input device
	//------------------------------------------------------------------
	kpd_accdet_dev = input_allocate_device();
	if (!kpd_accdet_dev)
	{
		ACCDET_LOG("[Accdet]kpd_accdet_dev : fail!\n");
		return -ENOMEM;
	}
	__set_bit(EV_KEY, kpd_accdet_dev->evbit);
	__set_bit(KEY_CALL, kpd_accdet_dev->keybit);
	__set_bit(KEY_ENDCALL, kpd_accdet_dev->keybit);
	__set_bit(KEY_NEXTSONG, kpd_accdet_dev->keybit);
	__set_bit(KEY_PREVIOUSSONG, kpd_accdet_dev->keybit);
	__set_bit(KEY_PLAYPAUSE, kpd_accdet_dev->keybit);
	__set_bit(KEY_STOPCD, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, kpd_accdet_dev->keybit);
	__set_bit(KEY_VOLUMEUP, kpd_accdet_dev->keybit);

/*                                                                                      */
#if 1 //                                  
    __set_bit(KEY_MEDIA, kpd_accdet_dev->keybit);
#endif
/*                                                                                      */

#ifdef CONFIG_LGE_AAT_KEY
	__set_bit(KEY_F10, kpd_accdet_dev->keybit);
#endif
	kpd_accdet_dev->id.bustype = BUS_HOST;
	kpd_accdet_dev->name = "ACCDET";
	if(input_register_device(kpd_accdet_dev))
	{
		ACCDET_LOG("[Accdet]kpd_accdet_dev register : fail!\n");
	}else
	{
		ACCDET_LOG("[Accdet]kpd_accdet_dev register : success!!\n");
	}
	//------------------------------------------------------------------
	// 							Create workqueue
	//------------------------------------------------------------------	
	accdet_workqueue = create_singlethread_workqueue("accdet");
	INIT_WORK(&accdet_work, accdet_work_callback);



#ifdef SW_WORK_AROUND_ACCDET_DEBOUNCE_HANG
	init_timer(&check_timer);
	check_timer.expires	= jiffies + 5000/(1000/HZ);
	check_timer.function = accdet_timer_fn;

    accdet_check_workqueue = create_singlethread_workqueue("accdet_check");
	INIT_WORK(&accdet_check_work, accdet_check_work_callback);

#endif
    //------------------------------------------------------------------
	//							wake lock
	//------------------------------------------------------------------
	wake_lock_init(&accdet_suspend_lock, WAKE_LOCK_SUSPEND, "accdet wakelock");
    wake_lock_init(&accdet_irq_lock, WAKE_LOCK_SUSPEND, "accdet irq wakelock");
    wake_lock_init(&accdet_key_lock, WAKE_LOCK_SUSPEND, "accdet key wakelock");
#ifdef SW_WORK_AROUND_ACCDET_REMOTE_BUTTON_ISSUE
     init_waitqueue_head(&send_event_wq);
     //start send key event thread
	 keyEvent_thread = kthread_run(sendKeyEvent, 0, "keyEvent_send");
     if (IS_ERR(keyEvent_thread))
	 {
        error = PTR_ERR(keyEvent_thread);
        ACCDET_LOG( " failed to create kernel thread: %d\n", error);
     }
#endif
	
	#if DEBUG_THREAD

	if((ret = accdet_create_attr(&accdet_driver.driver)))
	{
		ACCDET_LOG("create attribute err = %d\n", ret);
		
	}

	#endif

	/* For early porting before audio driver add */
	//temp_func();
	ACCDET_LOG("[Accdet]accdet_probe : ACCDET_INIT\n");
	if (g_accdet_first == 1)
	{	
		long_press_time_ns = (s64)long_press_time * NSEC_PER_MSEC;
				
		//Accdet Hardware Init
		accdet_init();

		//mt6577_irq_set_sens(MT6577_ACCDET_IRQ_ID, MT65xx_EDGE_SENSITIVE);
#if 1 //                                              
		//mt_irq_set_sens(MT_ACCDET_IRQ_ID, MT65xx_EDGE_SENSITIVE);
#else
		mt_irq_set_sens(MT_ACCDET_IRQ_ID, MT65xx_LEVEL_SENSITIVE);
#endif //                                              
		mt_irq_set_polarity(MT_ACCDET_IRQ_ID, MT65xx_POLARITY_LOW);
		//register accdet interrupt
		ret =  request_irq(MT_ACCDET_IRQ_ID, accdet_irq_handler, 0, "ACCDET", NULL);
		if(ret)
		{
			ACCDET_LOG("[Accdet]accdet register interrupt error\n");
		}

#if 1 //                                              
#else
		queue_work(accdet_workqueue, &accdet_work); //schedule a work for the first detection					
#endif //                                              
		#ifdef ACCDET_EINT
             accdet_eint_workqueue = create_singlethread_workqueue("accdet_eint");
	      INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);

			//ACCDET_LOG("[Accdet]accdet_setup_eint active [%s] \n", accdet_eint_level? "high" : "low");
	      accdet_setup_eint();
       #endif
		g_accdet_first = 0;
	}
	#ifdef ACCDET_MULTI_KEY_FEATURE
	//get adc mic channle number
		g_adcMic_channel_num = IMM_get_adc_channel_num("ADC_MIC",7);
		ACCDET_LOG("[Accdet]g_adcMic_channel_num=%d!\n",g_adcMic_channel_num);
	#endif

        ACCDET_LOG("[Accdet]accdet_probe done!\n");
	return 0;
}

static int accdet_remove(struct platform_device *dev)	
{
	ACCDET_LOG("[Accdet]accdet_remove begin!\n");
	if(g_accdet_first == 0)
	{
		free_irq(MT_ACCDET_IRQ_ID,NULL);
	}

	//cancel_delayed_work(&accdet_work);
	#ifdef ACCDET_EINT
	destroy_workqueue(accdet_eint_workqueue);
	#endif
	destroy_workqueue(accdet_workqueue);
	switch_dev_unregister(&accdet_data);
	device_del(accdet_nor_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno,1);	
	input_unregister_device(kpd_accdet_dev);
	ACCDET_LOG("[Accdet]accdet_remove Done!\n");

	return 0;
}

static int accdet_suspend(struct platform_device *dev, pm_message_t state)  // only one suspend mode
{
#if 1 //                                              
#else
#ifdef ACCDET_MULTI_KEY_FEATURE
	 ACCDET_LOG("[Accdet]check_cable_type in suspend1: ACCDET_IRQ_STS = 0x%x\n", INREG32(ACCDET_IRQ_STS));

#else
    int i=0;
    //ACCDET_LOG("[Accdet]accdet_suspend\n");
    //close vbias
    //SETREG32(0xf0007500, (1<<7));
    //before close accdet clock we must clear IRQ done
    while(INREG32(ACCDET_IRQ_STS) & IRQ_STATUS_BIT)
	{
           ACCDET_LOG("[Accdet]check_cable_type: Clear interrupt on-going3....\n");
		   msleep(10);
	}
	while(INREG32(ACCDET_IRQ_STS))
	{
	  msleep(10);
	  CLRREG32(ACCDET_IRQ_STS, IRQ_CLR_BIT);
	  IRQ_CLR_FLAG = TRUE;
      ACCDET_LOG("[Accdet]check_cable_type:Clear interrupt:Done[0x%x]!\n", INREG32(ACCDET_IRQ_STS));	
	}
    while(i<50 && (INREG32(ACCDET_BASE + 0x0050)!=0x0))
	{
	  // wake lock
	  wake_lock(&accdet_suspend_lock);
	  msleep(10); //wait for accdet finish IRQ generation
	  g_accdet_working_in_suspend =1;
	  ACCDET_LOG("[Accdet] suspend wake lock %d\n",i);
	  i++;
	}
	if(1 == g_accdet_working_in_suspend)
	{
		ACCDET_LOG("[Accdet] Headset state is %d\n", accdet_status);
	  	wake_unlock(&accdet_suspend_lock);
	  	g_accdet_working_in_suspend =0;
        ACCDET_LOG("[Accdet] suspend wake unlock\n");
	}
/*
	ACCDET_LOG("[Accdet]suspend:sample_in:%x, curr_in:%x, mem_in:%x, FSM:%x\n"
		, INREG32(ACCDET_SAMPLE_IN)
		,INREG32(ACCDET_CURR_IN)
		,INREG32(ACCDET_MEMORIZED_IN)
		,INREG32(ACCDET_BASE + 0x0050));
*/

#ifdef ACCDET_EINT
    if(INREG32(ACCDET_CTRL)&& call_status == 0)
    {
        ACCDET_LOG("[Accdet]accdet_suspend : headset state is plug in : accdet_eint_state[%d] \n", accdet_eint_state);
        g_accdet_working_in_suspend = 1;
        pre_state_swctrl = INREG32(ACCDET_STATE_SWCTRL);
    }
    else
    {
	   //record accdet status
        ACCDET_LOG("[Accdet]accdet_suspend : headset state is plug out : accdet_eint_state[%d] \n", accdet_eint_state);
	    g_accdet_working_in_suspend = 1;

        if ( ACCDET_EINT_LEVEL != accdet_eint_state )
        {
            ACCDET_LOG("[Accdet]accdet_suspend >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
            ACCDET_LOG("[Accdet]accdet_suspend 1: eint plug-in: accdet_eint_state[%d] \n", accdet_eint_state);
            OUTREG32(ACCDET_STATE_SWCTRL, pre_state_swctrl);
            OUTREG32(ACCDET_CTRL, ACCDET_ENABLE);
        }
        else
        {
            ACCDET_LOG("[Accdet]accdet_suspend 2: eint plug-out: accdet_eint_state[%d] \n", accdet_eint_state);
            // disable ACCDET unit
			pre_state_swctrl = INREG32(ACCDET_STATE_SWCTRL);
			OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
			OUTREG32(ACCDET_STATE_SWCTRL, 0);
		}
	}
#else
    // disable ACCDET unit
    if(call_status == 0)
    {
       pre_state_swctrl = INREG32(ACCDET_STATE_SWCTRL);
       OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
       OUTREG32(ACCDET_STATE_SWCTRL, 0);
       disable_clock(MT65XX_PDN_PERI_ACCDET,"ACCDET");
    }
#endif	

    ACCDET_LOG("[Accdet]accdet_suspend: ACCDET_CTRL=[0x%x], STATE=[0x%x]->[0x%x]\n", INREG32(ACCDET_CTRL), pre_state_swctrl,INREG32(ACCDET_STATE_SWCTRL));
    //ACCDET_LOG("[Accdet]accdet_suspend ok\n");
#endif
#endif //                                              
	return 0;
}

static int accdet_resume(struct platform_device *dev) // wake up
{
#if 1 //                                              
#else
#ifdef ACCDET_MULTI_KEY_FEATURE
	ACCDET_LOG("[Accdet]check_cable_type in resume1: ACCDET_IRQ_STS = 0x%x\n", INREG32(ACCDET_IRQ_STS));
    //ACCDET_LOG("[Accdet]accdet_resume\n");
#else
#ifdef ACCDET_EINT
	//if(1==g_accdet_working_in_suspend &&  0== call_status)
	if ( 1 == g_accdet_working_in_suspend)
	{
		if( INREG32(ACCDET_CTRL) && call_status == 0 )
		{
            ACCDET_LOG("[Accdet]accdet_resume : headset state is plug in : accdet_eint_state[%d] \n", accdet_eint_state);
            g_accdet_working_in_suspend =0;
    	}
        else
        {
            ACCDET_LOG("[Accdet]accdet_resume : headset state is plug out : accdet_eint_state[%d] \n", accdet_eint_state);
            if( ACCDET_EINT_LEVEL != accdet_eint_state )
            {
                ACCDET_LOG("[Accdet]accdet_resume 1: eint plug-in(%d)\n", accdet_eint_state);
                OUTREG32(ACCDET_STATE_SWCTRL, pre_state_swctrl);
                OUTREG32(ACCDET_CTRL, ACCDET_ENABLE);
            }
            else
            {
                ACCDET_LOG("[Accdet]accdet_resume 2: eint plug-out(%d)\n", accdet_eint_state);
				// enable ACCDET unit
            	OUTREG32(ACCDET_CTRL, ACCDET_DISABLE);
                OUTREG32(ACCDET_STATE_SWCTRL, 0);
            }
			//clear g_accdet_working_in_suspend
			g_accdet_working_in_suspend =0;
        }
	}
#else
	if(call_status == 0)
	{
       enable_clock(MT65XX_PDN_PERI_ACCDET,"ACCDET");

       // enable ACCDET unit	

       OUTREG32(ACCDET_STATE_SWCTRL, pre_state_swctrl);
       OUTREG32(ACCDET_CTRL, ACCDET_ENABLE);
	}
#endif
    ACCDET_LOG("[Accdet]accdet_resume: ACCDET_CTRL=[0x%x], STATE_SWCTRL=[0x%x]\n", INREG32(ACCDET_CTRL), INREG32(ACCDET_STATE_SWCTRL));
/*
    ACCDET_LOG("[Accdet]resum:sample_in:%x, curr_in:%x, mem_in:%x, FSM:%x\n"
		,INREG32(ACCDET_SAMPLE_IN)
		,INREG32(ACCDET_CURR_IN)
		,INREG32(ACCDET_MEMORIZED_IN)
		,INREG32(ACCDET_BASE + 0x0050));
*/
    //ACCDET_LOG("[Accdet]accdet_resume ok\n");
#endif
#endif //                                              
    return 0;
}

#if 0
struct platform_device accdet_device = {
	.name	  ="Accdet_Driver",		
	.id		  = -1,	
	.dev    ={
	.release = accdet_dumy_release,
	}
};
#endif

static struct platform_driver accdet_driver = {
	.probe		= accdet_probe,	
	.suspend	= accdet_suspend,
	.resume		= accdet_resume,
	.remove   = accdet_remove,
	.driver     = {
	.name       = "Accdet_Driver",
	},
};

static int accdet_mod_init(void)
{
	int ret = 0;

	ACCDET_LOG("[Accdet]accdet_mod_init begin!\n");

	//------------------------------------------------------------------
	// 							Accdet PM
	//------------------------------------------------------------------
	//remove platform device register
	#if 0
	ret = platform_device_register(&accdet_device);
	if (ret) {
		ACCDET_LOG("[Accdet]platform_device_register error:(%d)\n", ret);	
		return ret;
	}
	else
	{
		ACCDET_LOG("[Accdet]platform_device_register done!\n");
	}
	#endif
	
	ret = platform_driver_register(&accdet_driver);
	if (ret) {
		ACCDET_LOG("[Accdet]platform_driver_register error:(%d)\n", ret);
		return ret;
	}
	else
	{
		ACCDET_LOG("[Accdet]platform_driver_register done!\n");
	}

    ACCDET_LOG("[Accdet]accdet_mod_init done!\n");
    return 0;

}

static void  accdet_mod_exit(void)
{
	ACCDET_LOG("[Accdet]accdet_mod_exit\n");
	platform_driver_unregister(&accdet_driver);
//	platform_device_unregister(&accdet_device);

	ACCDET_LOG("[Accdet]accdet_mod_exit Done!\n");
}

module_init(accdet_mod_init);
module_exit(accdet_mod_exit);


module_param(debug_enable,int,0644);

MODULE_DESCRIPTION("MTK MT6577 ACCDET driver");
MODULE_AUTHOR("Anny <Anny.Hu@mediatek.com>");
MODULE_LICENSE("GPL");

