/*
 * Charging IC driver (rt9524)
 *
 * Copyright (C) 2010 LGE, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

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

/* Function Prototype */
enum power_supply_type get_charging_ic_status(void);

void charging_ic_active_default(void);
void charging_ic_set_ta_mode(void);
void charging_ic_set_usb_mode(void);
void charging_ic_deactive(void);
void charging_ic_set_factory_mode(void);
