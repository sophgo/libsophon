##############################################################################
#
#    Copyright (c) 2016-2022 by Bitmain Technologies Inc. All rights reserved.
#
#    The material in this file is confidential and contains trade secrets
#    of Bitmain Technologies Inc. This is proprietary information owned by
#    Bitmain Technologies Inc. No part of this work may be disclosed,
#    reproduced, copied, transmitted, or used in any way for any purpose,
#    without the express written permission of Bitmain Technologies Inc.
#
##############################################################################

ifeq ($(CHIP_NAME),bm1684)
COMMON_DIR = $(BM_ROOT)/common/bm1684

else ifeq ($(CHIP_NAME),bm1686)
COMMON_DIR = $(BM_ROOT)/common/bm1686
endif

COMMON_SRCS_C  = $(wildcard $(COMMON_DIR)/src/*.c)

INCLUDE_DIR += -I$(COMMON_DIR)/include -I$(BM_ROOT)/config

RELEASE_HEADERS += $(COMMON_DIR)/include/op_code.h
NNTC_RELEASE_HEADERS += $(COMMON_DIR)/include/op_code.h
