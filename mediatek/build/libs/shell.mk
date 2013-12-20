# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.
#
# MediaTek Inc. (C) 2010. All rights reserved.
#
# BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
# AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
# NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
# SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
# SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
# THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
# THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
# CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
# SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
# STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
# CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
# AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
# OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
# MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
#
# The following software/firmware and/or related documentation ("MediaTek Software")
# have been modified by MediaTek Inc. All revisions are subject to any receiver's
# applicable license agreements with MediaTek Inc.


include $(_prefix_)/mediatek/build/Makefile
$(call codebase-path,$(_postfix_),$(_prefix_))

# export variables to shell environments
ifeq ($(strip $(MTK_HYBRID_PRODUCTFILE)),)
    $(eval ADDITIONAL_LIST := $(strip $(foreach p,$(MTK_PROJECT_CONFIGS),\
        $(shell cat $p | grep -v "^\s*#" | sed 's/\s*=\s*.*//g'))))
else
    $(eval ADDITIONAL_LIST := $(strip $(foreach p,$(MTK_PROJECT_CONFIGS),\
        $(shell cat $p | grep -P "\b\s*=\s*\b" | grep -v -P "^\s*#|:|\+" | sed 's/\s*=\s*.*//g'))))
    ADDITIONAL_LIST += MTK_HYBRID_PRODUCTFILE
endif

# TODO this should be done automatically to avoid issues
VARIABLE_LIST := \
    TO_ROOT MTK_PROJECT MTK_ROOT MTK_ROOT_CUSTOM_OUT MKT_ROOT_CONFIG_OUT MTK_ROOT_PLATFORM MTK_ROOT_CONFIG \
    MTK_ROOT_BUILD MTK_ROOT_SOURCE MTK_ROOT_CUSTOM MTK_PATH_PLATFORM \
    MTK_PATH_BUILD MTK_PATH_SOURCE MTK_PATH_CUSTOM MTK_ROOT_GEN_CONFIG \
    MTK_CUSTOM_FOLDERS MTK_PROJECT_CONFIGS FULL_PROJECT \
    $(ADDITIONAL_LIST)

$(foreach v,$(VARIABLE_LIST), $(info export $(v)="$($v)"))
ifneq ($(strip $(MTK_HYBRID_PRODUCTFILE)),)
    $(foreach f,$(strip $(shell cat $(call relative-path,$(addprefix ../../../,$(strip $(MTK_HYBRID_PRODUCTFILE)))) | \
        grep -P "\b\s*=\s*\b" | grep -v -P "^\s*#|:|\+" | sed 's/\s*=\s*/=/g')), $(eval export $(f)))
endif

all:
	@echo "export MTK_LOAD_CONFIG=1"