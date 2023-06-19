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

################################################################
#Version Number
################################################################
MAJOR_VERSION	?= 1
MINOR_VERSION	?= 0

################################################################
# C_model mode or Device mode (needed in soc version, always to be 0 in soc version)
################################################################
#USING_CMODEL             ?= 1
FAST_GEN_CMD             ?= 1
################################################################
# General Configuration
################################################################

#If you want to use gdb debug, this should be set to 1 (needed in soc version)
DEBUG                    ?= 1
ARM_DEBUG                ?= 0

#This is the size of the global memory which can be allocated to nps, we use this to allocate array of global
#memory in our c_model, if you are using tv_gen, this size should be as small as poosible (needed in soc version)
GLOBAL_MEM_SIZE          ?= 0x100000000

ifeq ($(CHIP),bm1684)
NPU_SHIFT                     ?= 6
EU_SHIFT                      ?= 5
LOCAL_MEM_ADDRWIDTH           ?= 19
L2_SRAM_SIZE                  ?= 0x400000
KERNEL_MEM_SIZE               ?= 0
else ifeq ($(CHIP),bm1686)
NPU_SHIFT                     ?= 6
EU_SHIFT                      ?= 4
LOCAL_MEM_ADDRWIDTH           ?= 19
L2_SRAM_SIZE                  ?= 0x1000000
KERNEL_MEM_SIZE               ?= 0x800000
endif

#This is the mem size of host allocated memory in host DDR (needed in soc version)
HOST_REALMEM_SIZE        ?= 0x400000

#This is the size of the ARM reserved memory in ddr (needed in soc version)
COUNT_RESERVED_DDR_ARM   ?= 0x1000000

###############################################################
#MODE CONFIGURATION
###############################################################
#Using fullnet mode (needed in soc version, always to be 1 in soc mode)
#only in cmodel and compile backend, USING_FULLNET must not define
USING_FULLNET                  ?= 0

#enable the timer print in fullnet mode
ENABLE_TIMER_FOR_FULLNET       ?= 1

#if enable, compiler will generate each layer result, runtime will compare each layer result
USING_RUNTIME_COMPARE          ?= 0

#if enable, print dynamic compile flow
DYNAMIC_CONTEXT_DEBUG          ?= 0

#If this option is opened, the gdma / bdc are running in
#different threads , this can check some errors(such as error in sync id)
USING_MULTI_THREAD_ENGINE      ?= 1

#BLAS is is used
USING_BLAS                     ?= 0

#This option should be opened when you are providing firmware for ASIC simulation
#It will disable printf/malloc and enable fw tracing
USING_FW_SIMULATION            ?= 0

#when this macro is opened, only md scalar and fullnet api is used
FW_SIMPLE                      ?= 0

#No real atomic operation in cmodel
NO_ATOMIC                      ?= 0

#Constant table for blas lib
USING_INDEX_TABLE              ?= 1

#Disable the denorm floating operation (Our asic does not support denorm feature, we should keep the same with it)
DISABLE_DENORM                 ?= 1

#Eanable this macro to enable the printf operation
USING_FW_PRINT                 ?= 1
USING_FW_DEBUG                 ?= 0
USING_FW_API_PERF              ?= 0

#Enable this macro to disable the printf operation in assert(This will save code_size )
NO_PRINTF_IN_ASSERT            ?= 0

#If using memory pool recording
MEMPOOL_RECORD		       ?= 1

#Enable the macro to use access bdc and gdma by fast reg access
FAST_REG_ACCESS                ?= 1

#Enable the macro to use access bdc and gdma by fast reg access
FAST_GEN_IGNORE_CHECK          ?= 0

###############################################################
#API mode
###############################################################
#When this option is enabled, api is running in an async mode, the api returned immediately after calling
#without done in the chip (needed in soc version)
USING_NO_WAIT_API	       ?= 1

#When this option is opened, interrupt is used instead of polling for CDMA copy (needed in soc version)
USING_INT_CDMA                 ?= 0

#When this option is opened, interrupt is used instead of polling for polling empty slot, (needed in soc version)
USING_INT_MSGFIFO         ?= 0

###############################################################
# TV_GEN CONFIGURATION
###############################################################
#Dump test vector
TV_GEN                  ?= 0

#Dump mask for local mem status
TV_GEN_DUMP_MASK        ?= 1

#Dump binary mode
DUMP_BIN                ?= 0

#Do not dump the data file
NO_DUMP                 ?= 0

#Do not dump the gddr.dat
NO_GDDR_DUMP            ?= 0

#Do not dump the cddr.dat
NO_CDDR_DUMP            ?= 0

PRINT_CDMA_TO_STATUS    ?= 1
#Only dump the difference
DIFF_DUMP               ?= 0

#Dump local mem into different sections
LOCAL_MEM_DUMP_BY_STEP  ?= 0

#Initial fill_data for global memory
GLOBAL_DATA_INITIAL     ?= 0xdeadbeef

#Initial fill_data for local memory
LOCAL_DATA_INITIAL      ?= 0xdeadbeef

#Dump the command buffer
TV_GEN_CMD              ?= 0

#Dump the command buffer
SOC_MODE                ?= 0

#log gdma operation detail
TV_GEN_GDMA_LOG         ?= 0

#driver intr mode
SYNC_API_INT_MODE       ?= 1

#is support lpddr4x
LPDDR4X_SUPPORT         ?= 1

#Pcie mode enable CPU
PCIE_MODE_ENABLE_CPU    ?= 0

#############################################################
# TEST CONFIGURATION
#############################################################
FIXED_TEST              ?= 0
PIO_DESC_INTERLEAVE     ?= 0

################################################################
#STAS_GEN CONFIGURATION
################################################################
STAS_GEN                ?= 0
DDR_BW                  ?= 64
STA_SHOW_BY_ROW         ?= 0
BM_STAS_VISUAL          ?= 0
BM_HW_RT                ?= 0
TIMELINE_GRIDS          ?= 128

################################################################
# RM CP MKDIR
################################################################
RM     := rm -rf
CP     := cp -rf
MV     := mv
MKDIR  := mkdir -p

Q  ?= 1
ifeq ($(Q), 1)
QCOMPILE = @
else
QCOMPILE =
endif

##############################################################################
# Global flags.
##############################################################################

ifeq ($(CONFIG_FLAGS), )
  CONFIG_FLAGS  =
endif

#CONFIG_FLAGS += -MMD -MP
CONFIG_FLAGS += -DMAJOR_VERSION=$(MAJOR_VERSION)
CONFIG_FLAGS += -DMINOR_VERSION=$(MINOR_VERSION)
CONFIG_FLAGS += -DCONFIG_GLOBAL_MEM_SIZE=$(GLOBAL_MEM_SIZE)
CONFIG_FLAGS += -DCONFIG_LOCAL_MEM_ADDRWIDTH=$(LOCAL_MEM_ADDRWIDTH)
CONFIG_FLAGS += -DCONFIG_COUNT_RESERVED_DDR_ARM=$(COUNT_RESERVED_DDR_ARM)
CONFIG_FLAGS += -DCONFIG_DDR_BW=$(DDR_BW)
CONFIG_FLAGS += -DCONFIG_DDR_LATENCY=$(DDR_LATENCY)
CONFIG_FLAGS += -DCONFIG_TIMELINE_GRIDS=$(TIMELINE_GRIDS)
CONFIG_FLAGS += -DCONFIG_LOCAL_MEM_LATENCY=$(LOCAL_MEM_LATENCY)
CONFIG_FLAGS += -DCONFIG_GLOBAL_DATA_INITIAL=$(GLOBAL_DATA_INITIAL)
CONFIG_FLAGS += -DCONFIG_LOCAL_DATA_INITIAL=$(LOCAL_DATA_INITIAL)
CONFIG_FLAGS += -DCONFIG_HOST_REALMEM_SIZE=$(HOST_REALMEM_SIZE)
CONFIG_FLAGS += -DCONFIG_L2_SRAM_SIZE=$(L2_SRAM_SIZE)
CONFIG_FLAGS += -DCONFIG_KERNEL_MEM_SIZE=$(KERNEL_MEM_SIZE)
CONFIG_FLAGS += -DCONFIG_NPU_SHIFT=$(NPU_SHIFT)
CONFIG_FLAGS += -DCONFIG_EU_SHIFT=$(EU_SHIFT)


ifeq ($(USING_INDEX_TABLE), 1)
CONFIG_FLAGS   += -DUSING_INDEX_TABLE
endif

ifeq ($(USING_FW_SIMULATION), 1)
CONFIG_FLAGS   += -DUSING_FW_SIMULATION
endif

ifeq ($(TV_GEN),1)
CONFIG_FLAGS    += -DBM_TV_GEN
endif

ifeq ($(TV_GEN_DUMP_MASK), 1)
CONFIG_FLAGS    += -DBM_TV_GEN_DUMP_MASK
endif

ifeq ($(STAS_GEN),1)
CONFIG_FLAGS    += -DBM_STAS_GEN
endif

ifeq ($(DUMP_BIN),1)
CONFIG_FLAGS    += -DDUMP_BIN
endif

ifeq ($(NO_DUMP),1)
CONFIG_FLAGS    += -DNO_DUMP
endif

ifeq ($(NO_GDDR_DUMP),1)
CONFIG_FLAGS    += -DNO_GDDR_DUMP
endif

ifeq ($(NO_CDDR_DUMP),1)
CONFIG_FLAGS    += -DNO_CDDR_DUMP
endif

ifeq ($(NO_INIT_TABLE),1)
CONFIG_FLAGS    += -DNO_INIT_TABLE
endif

ifeq ($(USING_CMODEL),1)
CONFIG_FLAGS    += -DUSING_CMODEL
ifeq ($(CHIP_NAME),bm1684)
CONFIG_FLAGS    += -DCMODEL_CHIPID=0x1684
else
CONFIG_FLAGS    += -DCMODEL_CHIPID=0x1686
endif
endif

ifeq ($(FAST_GEN_CMD),1)
CONFIG_FLAGS    += -DFAST_GEN_CMD
endif

ifeq ($(DISABLE_DENORM) ,1)
CONFIG_FLAGS    += -DDISABLE_DENORM
endif

ifeq ($(USING_BLAS), 1)
CONFIG_FLAGS    += -DUSING_BLAS
endif


ifeq ($(STA_SHOW_BY_ROW), 1)
CONFIG_FLAGS    += -DSTA_SHOW_BY_ROW
endif

ifeq ($(BM_STAS_VISUAL), 1)
CONFIG_FLAGS    += -DBM_STAS_VISUAL
endif

ifeq ($(BM_HW_RT), 1)
CONFIG_FLAGS += -DBM_HW_RT
endif

ifeq ($(NO_ATOMIC), 1)
CONFIG_FLAGS    += -DCONFIG_NO_ATOMIC
endif

ifeq ($(PRINT_CDMA_TO_STATUS), 1)
CONFIG_FLAGS    += -DPRINT_CDMA_TO_STATUS
endif

ifeq ($(USING_NO_WAIT_API),1)
  ifeq ($(TV_GEN), 0)
    CONFIG_FLAGS    += -DBM_MSG_API_NO_WAIT_RESULT
  endif
endif

ifeq ($(USING_INT_CDMA),1)
  CONFIG_FLAGS    += -DUSING_INT_CDMA
endif

ifeq ($(USING_INT_MSGFIFO),1)
  ifeq ($(TV_GEN), 0)
    CONFIG_FLAGS    += -DUSING_INT_MSGFIFO
  endif
endif

ifeq ($(USING_FULLNET), 1)
CONFIG_FLAGS    += -DUSING_FULLNET
CONFIG_FLAGS    += -DUSING_MULTI_THREAD_ENGINE
endif

ifeq ($(USING_RUNTIME_COMPARE), 1)
CONFIG_FLAGS   += -DUSING_RUNTIME_COMPARE
endif

ifeq ($(DYNAMIC_CONTEXT_DEBUG), 1)
CONFIG_FLAGS   += -DDYNAMIC_CONTEXT_DEBUG
endif

ifeq ($(PIO_DESC_INTERLEAVE), 1)
  CONFIG_FLAGS    += -DPIO_DESC_INTERLEAVE
endif

ifeq ($(ENABLE_TIMER_FOR_FULLNET), 1)
  CONFIG_FLAGS    += -DENABLE_TIMER_FOR_FULLNET
endif

ifeq ($(FW_SIMPLE), 1)
CONFIG_FLAGS    += -DFW_SIMPLE
endif

ifeq ($(FAST_REG_ACCESS), 1)
CONFIG_FLAGS    += -DFAST_REG_ACCESS
endif

ifeq ($(USING_MULTI_THREAD_ENGINE), 1)
CONFIG_FLAGS    += -DUSING_MULTI_THREAD_ENGINE
endif


ifeq ($(DIFF_DUMP), 1)
CONFIG_FLAGS    += -DDIFF_DUMP
endif

ifeq ($(LOCAL_MEM_DUMP_BY_STEP), 1)
CONFIG_FLAGS    += -DLOCAL_MEM_DUMP_BY_STEP
endif

ifeq ($(TV_GEN_CMD), 1)
CONFIG_FLAGS    += -DTV_GEN_CMD
endif

ifeq ($(TV_GEN_GDMA_LOG), 1)
CONFIG_FLAGS    += -DTV_GEN_GDMA_LOG
endif

ifeq ($(FIXED_TEST), 1)
CONFIG_FLAGS    += -DFIXED_TEST
endif

ifeq ($(PIO_DESC_INTERLEAVE), 1)
CONFIG_FLAGS    += -DPIO_DESC_INTERLEAVE
endif

ifeq ($(USING_FW_PRINT), 1)
CONFIG_FLAGS    += -DUSING_FW_PRINT
ifeq ($(USING_FW_DEBUG), 1)
  CONFIG_FLAGS    += -DUSING_FW_DEBUG
endif
endif

ifeq ($(USING_FW_API_PERF), 1)
CONFIG_FLAGS    += -DUSING_FW_API_PERF
endif

ifeq ($(SOC_MODE), 1)
CONFIG_FLAGS    += -DSOC_MODE
endif

ifeq ($(NO_PRINTF_IN_ASSERT), 1)
CONFIG_FLAGS    += -DNO_PRINTF_IN_ASSERT
endif

ifeq ($(MEMPOOL_RECORD), 1)
CONFIG_FLAGS	+= -DMEMPOOL_RECORD
endif

ifeq ($(FAST_GEN_IGNORE_CHECK), 1)
CONFIG_FLAGS	+= -DFAST_GEN_IGNORE_CHECK
endif

ifeq ($(PCIE_MODE_ENABLE_CPU), 1)
CONFIG_FLAGS    += -DPCIE_MODE_ENABLE_CPU
endif

CFLAGS          = -Wall -Werror -Wno-error=deprecated-declarations $(CONFIG_FLAGS)
CXXFLAGS        = -std=c++11 -Wall -Werror -Wno-error=deprecated-declarations $(CONFIG_FLAGS)
