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
BMRUNTIME_ROOT      = $(shell pwd)
NETCOMPILER_TOP    ?= $(BMRUNTIME_ROOT)/..
BMODEL_INCLUDE     = $(NETCOMPILER_TOP)/out/install_bmodel/include
BMODEL_OBJ         = $(NETCOMPILER_TOP)/out/install_bmodel/obj/bmodel.o
#CPULAYER_INCLUDE := $(NETCOMPILER_TOP)/out/install_bmcpu/include
#CPULAYER_LIB     := $(NETCOMPILER_TOP)/out/install_bmcpu/lib
BMLIB_INCLUDE      = $(NETCOMPILER_TOP)/out/install/include
BMLIB_LIB          = $(NETCOMPILER_TOP)/out/install/lib
BMCOMPILER_FP16_SUPPORT = $(NETCOMPILER_TOP)/bmcompiler/include/common
export              BMRUNTIME_ROOT

##############################################################################
# Include common definitions.
##############################################################################

include Makefile.def
#CONFIG_FLAGS    += -DUSING_FULLNET

OUT_DIR       ?= $(BMRUNTIME_ROOT)/build
TGT           ?= $(OUT_DIR)/libbmrt.so
APP           ?= $(OUT_DIR)/bmrt_test
OBJ_DIR       ?= $(BMRUNTIME_ROOT)/obj
INSTALL_PATH  ?= $(NETCOMPILER_TOP)/out/install_bmruntime

SRCS_C      = $(wildcard src/*.c)
SRCS_C     += $(wildcard src/middleware/*.c)
SRCS_C     += $(wildcard src/bmfunc/*.c)
SRCS_C     += $(wildcard src/bmtap2/*.c)
SRCS_C     += $(wildcard src/cpp/*.c)
SRCS_CXX    = $(wildcard src/*.cpp)
SRCS_CXX   += $(wildcard src/middleware/*.cpp)
SRCS_CXX   += $(wildcard src/bmfunc/*.cpp)
SRCS_CXX   += $(wildcard src/bmtap2/*.cpp)
SRCS_CXX   += $(wildcard src/cpp/*.cpp)
APP_C       = $(wildcard app/*.c)
APP_CXX    += $(wildcard app/*.cpp)

########translate the *cpp path to *.o path under the obj path ################
OBJS_C      = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_C:.c=.o)))
OBJS_CXX    = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_CXX:.cpp=.o)))

DEPS_C      = $(OBJS_C:.o=.d)
DEPS_CXX    = $(OBJS_CXX:.o=.d)

#TGTS_C      = $(addprefix $(OUT_DIR)/, $(notdir $(APP_C:.c=)))
#TGTS_CXX    = $(addprefix $(OUT_DIR)/, $(notdir $(APP_CXX:.cpp=)))

INCLUDE_DIR = -I./include -I./include/bmtap2 -I$(BMODEL_INCLUDE) -I$(BMLIB_INCLUDE) -I$(BMCOMPILER_FP16_SUPPORT) -I./obj

LDFLAGS     += -L$(BMLIB_LIB) -lbmlib -ldl -pthread
APP_LDFLAGS  = -L$(OUT_DIR) $(LDFLAGS) -lbmrt

CFLAGS      += $(INCLUDE_DIR)
CXXFLAGS    += $(INCLUDE_DIR)
CXXFLAGS    += '-DVER="3.1.0"'

KERNEL_DIR = $(wildcard lib/libbm1684x_kernel_module.so)
KERNEL_OUT = $(BMRUNTIME_ROOT)/obj/kernel_module.h

all: $(KERNEL_OUT) $(OBJ_DIR) $(OUT_DIR) $(TGT) $(APP)

.PHONY : clean install

$(KERNEL_OUT):$(OBJ_DIR)
	echo "const unsigned char kernel_module_data[] = {" > $(KERNEL_OUT)
	hexdump -v -e '8/1 "0x%02x," "\n"' $(KERNEL_DIR) >> $(KERNEL_OUT);\
	echo "};" >> $(KERNEL_OUT)

$(OBJ_DIR):
	$(MKDIR) $@

$(OUT_DIR):
	$(MKDIR) $@

-include $(DEPS_C)
-include $(DEPS_CXX)

$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/middleware/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/bmtap2/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/cpp/%.c $(KERNEL_OUT)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/%.cpp $(KERNEL_OUT)
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/middleware/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/bmfunc/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/bmtap2/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: src/cpp/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(TGT): $(OBJS_C) $(OBJS_CXX) $(BMODEL_OBJ)
	$(CXX) $(LDFLAGS) -shared -o $@ $(OBJS_C) $(OBJS_CXX) $(BMODEL_OBJ) $(LDFLAGS)

$(APP): $(APP_C) $(APP_CXX) $(OUT_DIR) $(TGT)
	$(CXX) $(CXXFLAGS) -o $@ $(APP_C) $(APP_CXX) $(APP_LDFLAGS)

clean:
	$(RM) $(OBJ_DIR) $(OUT_DIR)
	$(RM) $(INSTALL_PATH)/lib/*
	$(RM) $(INSTALL_PATH)/include/*
	$(RM) $(KERNEL_OUT)

install:
	$(MKDIR)  $(INSTALL_PATH)
	$(MKDIR)  $(INSTALL_PATH)/include
	$(MKDIR)  $(INSTALL_PATH)/lib
	$(MKDIR)  $(INSTALL_PATH)/app
	$(CP) -u ./include/bmblob.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmcnnctx.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmnet.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmdef.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmruntime_cpp.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmruntime_interface.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmruntime_legacy.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmtap2/bmruntime_bmnet.h $(INSTALL_PATH)/include
	$(CP) -u ./include/bmtap2/bmruntime.hpp $(INSTALL_PATH)/include
	$(CP) -u $(TGT) $(INSTALL_PATH)/lib
	$(CP) -u $(APP) $(INSTALL_PATH)/app
ifeq ($(COVERAGE),1)
	$(MKDIR)  $(INSTALL_PATH)/cov
	$(CP) -u $(OBJ_DIR)/*.gcno $(INSTALL_PATH)/cov
endif
