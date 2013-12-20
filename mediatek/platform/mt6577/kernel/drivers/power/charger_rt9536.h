#define CHG_EN_SET_N                  GPIO35
#define CHG_EN_MODE                  GPIO35_MODE
#define CHG_EN_DIR                      GPIO35_DIR
#define CHG_EN_DATA_OUT            GPIO35_DATAOUT
 //#define CHG_EN_PULL_ENABLE        GPIO35_PULLEN
 //#define CHG_EN_PULL_SELECT        GPIO35_PULL


 #define CHG_EOC_N                      GPIO_EOC_PIN
 #define CHG_EOC_MODE                GPIO_EOC_PIN_M_GPIO
 #define CHG_EOC_DIR                    GPIO73_DIR
 #define CHG_EOC_PULL_ENABLE      GPIO73_PULLEN
 #define CHG_EOC_PULL_SELECT      GPIO73_PULL

#if defined ( LGE_BSP_LGBM )
typedef enum RT9536_ChargingModeTag
{
	RT9536_CM_OFF = 0,
	RT9536_CM_USB_100,
	RT9536_CM_USB_500,
	RT9536_CM_I_SET,
	RT9536_CM_FACTORY,
	
	RT9536_CM_UNKNOWN
}
RT9536_ChargingMode;

void RT9536_SetChargingMode( RT9536_ChargingMode newMode );

#endif

/* Function Prototype */
enum power_supply_type get_charging_ic_status(void);

void charging_ic_active_default(void);
void charging_ic_set_ta_mode(void);
void charging_ic_set_usb_mode(void);
void charging_ic_deactive(void);
void charging_ic_set_factory_mode(void);
kal_bool is_charging_ic_enable(void);
