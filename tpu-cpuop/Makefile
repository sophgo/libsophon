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
NETCOMPILER_TOP     ?= $(CPU_ROOT)/..
export              CPU_ROOT

##############################################################################
# Include common definitions.
##############################################################################

include Makefile.def

OUT_DIR       ?= $(CPU_ROOT)/build
TGT           ?= $(OUT_DIR)/libbmcpu.so
OBJ_DIR       ?= $(CPU_ROOT)/obj
INSTALL_PATH  ?= $(NETCOMPILER_TOP)/out/install_bmcpu

SRCS_C      = $(wildcard src/*.c)
SRCS_CXX    = $(wildcard src/*.cpp)


########translate the *cpp path to *.o path under the obj path ################
OBJS_C      = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_C:.c=.o)))
OBJS_CXX    = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_CXX:.cpp=.o)))

DEPS_C      = $(OBJS_C:.o=.d)
DEPS_CXX    = $(OBJS_CXX:.o=.d)

TGTS_C      = $(addprefix $(OUT_DIR)/, $(notdir $(APP_C:.c=)))
TGTS_CXX    = $(addprefix $(OUT_DIR)/, $(notdir $(APP_CXX:.cpp=)))

INCLUDE_DIR = -I./include -I./

ifneq (,$(CROSS_COMPILE))
CXXFLAGS    += -fexceptions -DCROSS_COMPILE
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


$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<


$(OBJ_DIR)/%.o: src/%.cpp 
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(TGT): $(OBJS_C) $(OBJS_CXX) 
	$(CXX) ${LDFLAGS} -shared -o $@ $(OBJS_C) $(OBJS_CXX)

clean:
	$(RM) $(OBJ_DIR) $(OUT_DIR)
	$(RM) $(INSTALL_PATH)/lib/*
	$(RM) $(INSTALL_PATH)/include/*

install:
	$(MKDIR)  $(INSTALL_PATH)
	$(MKDIR)  $(INSTALL_PATH)/include
	$(MKDIR)  $(INSTALL_PATH)/lib
	$(CP) -u ./include/bmcpu.h        $(INSTALL_PATH)/include/bmcpu.h
	$(CP) -u ./include/bmcpu_common.h $(INSTALL_PATH)/include/bmcpu_common.h
	$(CP) -u $(TGT) $(INSTALL_PATH)/lib
