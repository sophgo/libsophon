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
############### setting variable #############################
SHELL:=/bin/bash
BMVID_ROOT ?= $(BMVID_TOP_DIR)
OUT_DIR ?= $(BMVID_OUTPUT_DIR)
JPU_DIR ?= $(OUT_DIR)/decode
VPP_DIR ?= $(OUT_DIR)/vpp
BMION_DIR ?= $(OUT_DIR)/decode
USING_CMODEL ?= 0
CHIP ?= bm1684

BMCV_SRC_DIR = $(BMVID_ROOT)/bmcv/

ifneq ($(PRODUCTFORM), soc)
PCIE_MODE_ENABLE_CPU := 1
SOC_MODE := 0
else
PCIE_MODE_ENABLE_CPU := 0
SOC_MODE := 1
endif

include $(BMCV_SRC_DIR)/Makefile_common.mk
include $(BMCV_SRC_DIR)/test/Makefile

############# compiling option ###############################
ifeq ($(PRODUCTFORM),pcie) # pcie mode
    CROSS_COMPILE =
else ifeq ($(PRODUCTFORM),pcie_mips64)
    CROSS_COMPILE = mips-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_sw64) # pcie mode
    CROSS_COMPILE = sw_64-sunway-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_loongarch64)
    CROSS_COMPILE = loongarch64-linux-gnu-
else # pcie_arm64 and soc mode
    CROSS_COMPILE = aarch64-linux-gnu-
endif

CC = $(CROSS_COMPILE)gcc
CXX= $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar

CFLAGS          = -Wall -Werror -Wno-error=deprecated-declarations $(CONFIG_FLAGS)\
 				  -ffunction-sections -fdata-sections \
 				  -fPIC -Wno-unused-function \
 				  -funwind-tables -fno-short-enums \
				  -ffp-contract=off

CXXFLAGS        += -Wextra -funwind-tables -fno-short-enums $(CONFIG_FLAGS)\
 				   -Wall -Werror -Wno-error=deprecated-declarations \
 				   -ffunction-sections -fdata-sections -fno-strict-aliasing \
 				   -std=c++11 -Wno-unused-function \
 				   -fPIC -ffp-contract=off -fexceptions

ifeq ($(PRODUCTFORM), soc)
CFLAGS += -fsigned-char
CXXFLAGS += -fsigned-char
else ifeq ($(PRODUCTFORM), pcie_arm64)
CFLAGS += -fsigned-char
CXXFLAGS += -fsigned-char
else ifeq ($(PRODUCTFORM), pcie_mips64)
CFLAGS +=  -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
CXXFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
else ifeq ($(PRODUCTFORM), pcie_loongarch64)
CFLAGS +=
CXXFLAGS += -Wno-format-truncation -Wno-stringop-truncation
else ifeq ($(PRODUCTFORM), pcie_sw64)
CFLAGS +=
CXXFLAGS += -Wno-format-truncation -Wno-stringop-truncation --machine=sw6b
else  # pcie_x86
CFLAGS +=
CXXFLAGS +=
endif

LDFLAGS         += 	-pthread \
			-Wl,--check-sections \
			-Wl,--gc-sections
ifeq ($(PRODUCTFORM), pcie_mips64)
LDFLAGS         += -mips64r2 -mabi=64 -march=gs464e
endif

DYN_LDFLAGS     +=  -L$(JPU_DIR)/lib/ \
					-L$(BMION_DIR)/lib \
					-L$(OUT_DIR)/lib/ \
					-L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM) \
					-lbmion -lbmlib -lbmjpuapi -lbmjpulite -ldl

ifeq ($(DEBUG),1)
CFLAGS          += -g -DDEBUG
CXXFLAGS        += -g -DDEBUG
LDFLAGS         += -rdynamic
else
CFLAGS          += -O3
CXXFLAGS        += -O3
endif

ifeq ($(PCIE_MODE_ENABLE_CPU), 1)
CFLAGS += -DPCIE_MODE_ENABLE_CPU
CXXFLAGS += -DPCIE_MODE_ENABLE_CPU
endif

# bmlib

INCLUDE_DIR += -I$(BMVID_ROOT)/3rdparty/libbmcv/include
INCLUDE_DIR += -I$(BMCV_SRC_DIR)/include
INCLUDE_DIR += -I$(BMCV_SRC_DIR)/src
INCLUDE_DIR += -I$(JPU_DIR)/include
INCLUDE_DIR += -I$(VPP_DIR)/include
INCLUDE_DIR += -I$(BMION_DIR)/include

ifeq ($(USING_CMODEL),0)
INCLUDE_DIR += -I$(BMCV_SRC_DIR)/include/turbo-jpeg
else
INCLUDE_DIR += -I$(BMVID_ROOT)/vpp/driver/include/bm1684/
endif

ifeq ($(SOC_MODE), 0)
    JPU_FLAG = -DBM168X_PCIE_JPU -DBM_PCIE_MODE
else
    JPU_FLAG =
endif

define SOURCES_TO_OBJS
    $(patsubst %.$(2),%.o,$(filter %.$(2),$(1)))
endef

############## main part for makefile #########################
BMCV_SRCS_CXX    = $(wildcard $(BMCV_SRC_DIR)/src/*.cpp $(BMCV_SRC_DIR)/src/bm1684/*.cpp $(BMCV_SRC_DIR)/src/bm1684x/*.cpp)

BMCV_CXX_OBJS1 = $(call SOURCES_TO_OBJS,$(BMCV_SRCS_CXX),cpp)
BMCV_CXX_OBJS2 = $(patsubst $(BMCV_SRC_DIR)/%.o,%.o, $(BMCV_CXX_OBJS1))
BMCV_CXX_OBJS = $(addprefix $(BMCV_SRC_DIR)/obj/,$(BMCV_CXX_OBJS2))

BMCV_OBJ_DIR = $(sort $(dir $(BMCV_CXX_OBJS)))

INCLUDE_DIR += -I$(OUT_DIR)/include/

BMVPP_INSTALL = $(VPP_DIR)/lib/libvpp.a
BMCV_STATIC = $(OUT_DIR)/bmcv/lib/libbmcv.a
TMP_BMCV_STATIC = $(OUT_DIR)/bmcv/lib/tmpbmcv.a
BMCV_DYN = $(OUT_DIR)/bmcv/lib/libbmcv.so
BMCV_DYN_NAME = libbmcv.so
BMCV_DYN_REL = $(OUT_DIR)/bmcv/lib/libbmcv_rel.so
INSTALL_LIBS += $(BMCV_DYN)
RELEASE_LIBS += $(BMCV_DYN)
RELEASE_HEADERS += $(BMCV_SRC_DIR)/include/bmcv_api_ext.h $(BMCV_SRC_DIR)/include/bmcv_api.h $(BMCV_SRC_DIR)/include/bmcv_api_ext_c.h
DYNAMIC_LIBRARYS = $(BMVID_ROOT)/3rdparty/tpu_kernel_module/libbm1684x_kernel_module.so

-include $(BMVID_ROOT)/build/version.mak
ifneq ($(SO_NAME),)
    BMCV_DYN_SONAME=$(BMCV_DYN)$(SO_NAME)
    BMCV_DYN_SOVERSION=$(BMCV_DYN)$(SO_VERSION)
endif

ifeq ($(PRODUCTFORM), soc)
LIB_TURBO_JPEG_PATH =  $(BMVID_ROOT)/3rdparty/libjpeg-turbo/binary/lib/soc/libturbojpeg.a
else ifeq ($(PRODUCTFORM), pcie_arm64)
LIB_TURBO_JPEG_PATH =  $(BMVID_ROOT)/3rdparty/libjpeg-turbo/binary/lib/soc/libturbojpeg.a
else ifeq ($(PRODUCTFORM), pcie_mips64)
LIB_TURBO_JPEG_PATH =  $(BMVID_ROOT)/3rdparty/libjpeg-turbo/binary/lib/mips/libturbojpeg.a
else ifeq ($(PRODUCTFORM), pcie_loongarch64)
LIB_TURBO_JPEG_PATH =  $(BMVID_ROOT)/3rdparty/libjpeg-turbo/binary/lib/loongarch64/libturbojpeg.a
else ifeq ($(PRODUCTFORM), pcie_sw64)
LIB_TURBO_JPEG_PATH =  $(BMVID_ROOT)/3rdparty/libjpeg-turbo/binary/lib/sw64/libturbojpeg.a
else  # pcie_x86
LIB_TURBO_JPEG_PATH =  $(BMVID_ROOT)/3rdparty/libjpeg-turbo/binary/lib/pcie/libturbojpeg.a
endif

$(BMCV_OBJ_DIR):
	@mkdir -p $@

ifeq ($(USING_CMODEL),1)
$(BMCV_CXX_OBJS): $(BMCV_SRC_DIR)/obj/%.o:$(BMCV_SRC_DIR)/%.cpp | $(BMCV_OBJ_DIR)
	@echo cc -c $< -o $@
	@$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) -Wextra -c $< -o $@

$(BMCV_STATIC): $(BMCV_CXX_OBJS)
	@echo ar $(BMCV_STATIC)
	@mkdir -p $(OUT_DIR)/bmcv/lib
	@$(AR) rcs $(BMCV_STATIC) $(BMCV_CXX_OBJS)

$(BMCV_DYN): $(BMCV_STATIC)
	@echo cc $(BMCV_DYN)
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -Wl,-soname,$(BMCV_DYN_NAME)$(SO_NAME) -o $(BMCV_DYN) -Wl,--whole-archive $(BMCV_STATIC) -Wl,--no-whole-archive
	@mv $(BMCV_DYN) $(OUT_DIR)/bmcv/lib/cmodel/
	@mv $(BMCV_STATIC) $(OUT_DIR)/bmcv/lib/cmodel/
ifneq ($(BMCV_DYN_SOVERSION), )
	mv $(OUT_DIR)/bmcv/lib/cmodel/$(BMCV_DYN_NAME) $(OUT_DIR)/bmcv/lib/cmodel/$(BMCV_DYN_NAME)$(SO_VERSION)
	ln -sf $(BMCV_DYN_NAME)$(SO_VERSION) $(OUT_DIR)/bmcv/lib/cmodel/$(BMCV_DYN_NAME)$(SO_NAME)
	ln -sf $(BMCV_DYN_NAME)$(SO_NAME) $(OUT_DIR)/bmcv/lib/cmodel/$(BMCV_DYN_NAME)
endif

else
jpeg-turbo:
	@if [ ! -d $(BMCV_SRC_DIR)/include/turbo-jpeg ]; then \
	mkdir -p $(BMCV_SRC_DIR)/include/turbo-jpeg; \
	fi
	@cp -f $(BMVID_ROOT)/3rdparty/libjpeg-turbo/binary/include/* $(BMCV_SRC_DIR)/include/turbo-jpeg

$(BMCV_CXX_OBJS): $(BMCV_SRC_DIR)/obj/%.o:$(BMCV_SRC_DIR)/%.cpp jpeg-turbo | $(BMCV_OBJ_DIR)
	@echo cc -c $< -o $@
	@$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) $(JPU_FLAG) -c $< -o $@

$(BMCV_STATIC): $(BMCV_CXX_OBJS)
	@echo ar $(BMCV_STATIC)
	@mkdir -p $(OUT_DIR)/bmcv/lib
	@$(AR) x $(BMVPP_INSTALL)
	@$(AR) rcs $(BMCV_STATIC) $(BMCV_CXX_OBJS)

$(BMCV_DYN): $(BMCV_CXX_OBJS)
	@echo cc $(BMCV_DYN)
	@mkdir -p $(OUT_DIR)/bmcv/lib
	@$(CXX) $(BMCV_CXX_OBJS) $(CXXFLAGS)  $(DYN_LDFLAGS) $(LDFLAGS) -shared -Wl,-soname,$(BMCV_DYN_NAME)$(SO_NAME) -o $(BMCV_DYN) -Wl,--whole-archive $(BMVPP_INSTALL) -Wl,--whole-archive $(LIB_TURBO_JPEG_PATH) -Wl,--no-whole-archive
ifneq ($(BMCV_DYN_SOVERSION), )
	mv $@ $(BMCV_DYN_SOVERSION)
	ln -sf $(BMCV_DYN_NAME)$(SO_VERSION) $(BMCV_DYN_SONAME)
	ln -sf $(BMCV_DYN_NAME)$(SO_NAME) $@
endif

endif

ifneq ($(USING_CMODEL),1)
ifneq ($(SOC_MODE),1)
BMCV_CPU_LIB = $(OUT_DIR)/bmcv/lib/libbmcv_cpu_func.so
BMCV_CPU_NAME = libbmcv_cpu_func.so
RELEASE_LIBS += $(BMCV_CPU_LIB)
BMCV_CPU_CXX = $(wildcard $(BMCV_SRC_DIR)/src/pcie_cpu/*.cpp)
BMCV_CPU_COMPILE := $(BMCPU_CROSS_COMPILE)
ifeq ($(BMCV_CPU_COMPILE), )
$(error "please give aarch64-linux-gnu-g++ path")
exit 1
endif

ifneq ($(SO_NAME),)
    BMCV_CPU_SONAME=$(BMCV_CPU_LIB)$(SO_NAME)
    BMCV_CPU_SOVERSION=$(BMCV_CPU_LIB)$(SO_VERSION)
endif

$(BMCV_CPU_LIB): $(BMCV_CPU_CXX)
	@$(BMCV_CPU_COMPILE) -O3 -fPIC -shared -Wl,-soname,$(BMCV_CPU_NAME)$(SO_NAME) -o $@ $^
ifneq ($(BMCV_CPU_SOVERSION), )
	mv $@ $(BMCV_CPU_SOVERSION)
	ln -sf $(BMCV_CPU_NAME)$(SO_VERSION) $(BMCV_CPU_SONAME)
	ln -sf $(BMCV_CPU_NAME)$(SO_NAME) $@
endif

endif
endif

ifeq ($(CHIP), bm1682)
bmcv:
	@mkdir -p $(OUT_DIR)/bmcv/include
	@cp -f $(RELEASE_HEADERS) $(OUT_DIR)/bmcv/include

clean:
	@echo rm -rf $(OUT_DIR)/bmcv/include
	@rm -rf $(OUT_DIR)/bmcv/include

else
bmcv: $(BMCV_DYN) $(BMCV_CPU_LIB) $(BMCV_STATIC)
	@mkdir -p $(OUT_DIR)/bmcv/include
	@mkdir -p $(OUT_DIR)/bmcv/lib/tpu_module
	@cp -f $(RELEASE_HEADERS)  $(OUT_DIR)/bmcv/include
	@cp -f $(DYNAMIC_LIBRARYS) $(OUT_DIR)/bmcv/lib/tpu_module

clean:
	@echo rm -rf $(OUT_DIR)/bmcv/lib
	@rm -rf $(OUT_DIR)/bmcv/lib
	@rm -rf $(BMCV_OBJ_DIR) $(BMCV_DYN) $(BMCV_CPU_LIB) $(BMCV_STATIC)
	@rm -rf $(BMCV_DYN_SOVERSION) $(BMCV_DYN_SONAME)

endif
BMCV_DEP = $(patsubst %.o,%.d,$(BMCV_CXX_OBJS))
-include $(BMCV_DEP)
