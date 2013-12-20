#ifndef MINIABB_H
#define MINIABB_H

#define MINIABB_BUFSIZE     256
#define MINIABB_DEV_NAME    "ext_charger_i2c"


#define CHG_CTRL1        0x08
#define CHGEN           0x80
#define EXPDETEN        0x40
#define PTM             0x20
#define CHG_OFF         0x10
#define CHG_CTRL2        0x09

#define CONTROL1        0x01
#define CP_EN           0x01

#define CONTROL2        0x02
#define INTPOL          0x80
#define INT_EN          0x40
#define MIC_LP          0x20
#define CP_AUD          0x10
#define MB_200          0x08
#define INT2_EN         0x04
#define CHG_TYP         0x02
#define USB_DET_DIS     0x01

#define INTSTATUS1       0x04
#define CHGDET          0x80
#define MR_COMP     0x40
#define SENDEND     0x20
#define VBUS        0x10
#define IDNO_MASK   0x0F

#define INTSTATUS2      0x05
#define CHG             0x80


typedef enum
{
    CHG_USB = 400,
    CHG_TA = 700,
}CHG_TYPE;

typedef enum
{
    IPRECHG_40mA = 0x00,
    IPRECHG_60mA,
    IPRECHG_80mA,
    IPRECHG_100mA,
}IPRECHG_TYPE;

typedef enum
{
    CHG_CURR_90mA = 0x00,
    CHG_CURR_100mA = 0x10,
    CHG_CURR_400mA,
    CHG_CURR_450mA,
    CHG_CURR_500mA,
    CHG_CURR_600mA,
    CHG_CURR_700mA,
    CHG_CURR_800mA,
    CHG_CURR_900mA,
    CHG_CURR_1000mA,
}CHG_SET_TYPE;

typedef enum
{
    EOC_LVL_5PER = 0x00,
    EOC_LVL_10PER,  
    EOC_LVL_16PER,  
    EOC_LVL_20PER,  
    EOC_LVL_25PER,  
    EOC_LVL_33PER,  
    EOC_LVL_50PER,  
}IMIN_SET_TYPE;

enum {
    OFF,
    ON,
};

#define TRUE    1
#define FALSE   0

#define EN_CTRL         0x0A
//LED setting
#define LED1_EN         0x08
#define LED2_EN         0x04
#define LED3_EN         0x02
#define LED4_EN         0x01

#define LED_SET         0x0D
#define LED_DIM         0x70

//LDO setting
#define LDO1_EN         0x80
#define LDO2_EN         0x40
#define LDO3_EN         0x20
#define LDO4_EN         0x10

#define LDO_VSET1       0x0B 
#define LDO1_VSET       0x90 //2.8V
//#define LDO2_VSET       0x02 //1.5V for hi542 //DVDD
//#define LDO2_VSET       0x01 //1.2V for hi543  //DVDD
#define LDO2_VSET       0x03 //1.8V for Rev.A hi543  //DVDD

#define LDO_VSET2       0x0C 
#define LDO3_VSET       0x30 //1.8V
#define LDO4_VSET       0x09 //2.8V


/* Function Prototype */
int check_EOC_status();
void set_charger_start_mode(int value);
void set_charger_factory_mode();
void set_charger_stop_mode();

#endif /* MINIABB_H */
