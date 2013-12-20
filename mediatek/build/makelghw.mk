
$(call codebase-path)

# project name
ifeq ($(findstring luv10,$(TARGET_PRODUCT)),luv10)
LGE_DWS      := _luv10
else
LGE_DWS      := _luv90
endif

# single/dual/triple SIM
ifeq ($(MTK_SHARE_MODEM_CURRENT),1)
LGE_DWS      := $(LGE_DWS)_1
else
  ifeq ($(MTK_SHARE_MODEM_CURRENT),2)
    LGE_DWS  := $(LGE_DWS)_2
    ifeq ($(findstring vee4ts, $(TARGET_PRODUCT)),vee4ts)
      LGE_DWS:= _v4_3
    endif
  endif
endif

# NFC support
ifeq ($(MTK_NFC_SUPPORT),yes)
LGE_DWS      := $(LGE_DWS)_nfc
endif

# DOMEKEY support
ifeq ($(LGE_USE_DOME_KEY),yes)
LGE_DWS      := $(LGE_DWS)_dome
endif

ifeq ($(HW_REVA), TRUE)
  LGE_DWS     := $(LGE_DWS)_a
else
  ifeq ($(HW_REVB), TRUE)
    LGE_DWS     := $(LGE_DWS)_b
  else
    ifeq ($(HW_REVC), TRUE)
      LGE_DWS     := $(LGE_DWS)_c
    else
      LGE_DWS     := $(LGE_DWS)_1
    endif
  endif
endif

# modify ProjectConfig.mk
ifeq ($(findstring luv10,$(TARGET_PRODUCT)),luv10)
BASE_PROJECT=lge77_v5_reva_jb
else
  ifeq ($(findstring vee4ts, $(TARGET_PRODUCT)),vee4ts)
    BASE_PROJECT=lge75_v4_triple_evb_jb
  else
    BASE_PROJECT=lge75_v4_jb
  endif
endif
