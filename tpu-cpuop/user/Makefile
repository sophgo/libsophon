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

SHELL              := /bin/bash
CPU_ROOT            = $(shell pwd)
##############################################################################
# Include common definitions.
##############################################################################
include Makefile.def

OUT_DIR       ?= $(CPU_ROOT)/build
TGT           ?= $(OUT_DIR)/libusercpu.so
OBJ_DIR       ?= $(CPU_ROOT)/obj

SRCS_C      = $(wildcard ./*.c)
SRCS_CXX    = $(wildcard ./*.cpp)

########translate the *cpp path to *.o path under the obj path ################
OBJS_C      = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_C:.c=.o)))
OBJS_CXX    = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_CXX:.cpp=.o)))

DEPS_C      = $(OBJS_C:.o=.d)
DEPS_CXX    = $(OBJS_CXX:.o=.d)

TGTS_C      = $(addprefix $(OUT_DIR)/, $(notdir $(APP_C:.c=)))
TGTS_CXX    = $(addprefix $(OUT_DIR)/, $(notdir $(APP_CXX:.cpp=)))

INCLUDE_DIR = -I./ 

ifneq (,$(CROSS_COMPILE))
CXXFLAGS    += -DCROSS_COMPILE
endif

CXXFLAGS    += -Wno-sign-compare

CFLAGS      += $(INCLUDE_DIR)
CXXFLAGS    += $(INCLUDE_DIR)

all:  $(OBJ_DIR) $(OUT_DIR) $(TGT) $(TGTS_C) $(TGTS_CXX)

.PHONY : clean install

$(OBJ_DIR):
	$(MKDIR) $@

$(OUT_DIR):
	$(MKDIR) $@

-include $(DEPS_C)
-include $(DEPS_CXX)


$(OBJ_DIR)/%.o: ./%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<


$(OBJ_DIR)/%.o: ./%.cpp 
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(TGT): $(OBJS_C) $(OBJS_CXX) 
	$(CXX) -shared -o $@ $(OBJS_C) $(OBJS_CXX) 

clean:
	$(RM) $(OBJ_DIR) $(OUT_DIR)
	$(RM) $(INSTALL_LIB_PATH)/*user*.so
	$(RM) $(INSTALL_INCLUDE_PATH)/*user*.h

install:
	$(MKDIR)  $(INSTALL_LIB_PATH)
	$(MKDIR)  $(INSTALL_INCLUDE_PATH)
	$(CP) -u ./user_bmcpu.h        $(INSTALL_INCLUDE_PATH)/user_bmcpu.h
	$(CP) -u ./user_bmcpu_common.h $(INSTALL_INCLUDE_PATH)/user_bmcpu_common.h	
	$(CP) -u $(TGT) $(INSTALL_LIB_PATH)
