#include "cust_ext_charger.h"
#include <linux/i2c.h>
#include <mach/mt_gpio.h>
#include <linux/delay.h>

void Cust_Button_LED_control(int on_off)
{
    //Button_LED_control(1)
    return 0;
}
EXPORT_SYMBOL(Cust_Button_LED_control);

void Cust_hwPowerOn(void)
{
    return 0;
}
EXPORT_SYMBOL(Cust_hwPowerOn);

void Cust_hwPowerOff(void)
{
    return 0;    
}
EXPORT_SYMBOL(Cust_hwPowerOff);

void Cust_ChargingTurnOn(void)
{
    return 0;
}
EXPORT_SYMBOL(Cust_ChargingTurnOn);

void Cust_ChargingTurnOff(void)
{
    return 0;    
}
EXPORT_SYMBOL(Cust_ChargingTurnOff);
