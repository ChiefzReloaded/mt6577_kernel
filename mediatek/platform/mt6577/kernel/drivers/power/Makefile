obj-$(CONFIG_MTK_PMIC)          += pmic_mt6329.o pmic_mt6329_bank1.o upmu_common.o

ifeq ($(CUSTOM_EXT_CHARGER_IC_SUPPORT),yes)
    obj-$(CONFIG_MTK_SMART_BATTERY) += mt_fuel_gauge.o
else
	obj-$(CONFIG_MTK_SMART_BATTERY) += mt_battery.o mt_fuel_gauge.o
endif

ifeq ($(CONFIG_MINIABB_CHARGER),yes) 
    obj-y += lge_miniabb.o
endif

ifeq ($(CONFIG_SINGLE_CHARGER),yes) 
    obj-y += charger_rt9536.o
endif

obj-$(CONFIG_MAX8971_CHARGER) += max8971_charger.o