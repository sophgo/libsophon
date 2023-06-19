OPTFLAGS := \
	-std=gnu99 \
	-O2 \
	-Wall \
	-Werror

COMMONFLAGS := \
	-march=armv8-a

ifeq ($(BOARD), ASIC)
DEFS += -DBOARD_ASIC
endif

ifeq ($(BOARD), FPGA)
DEFS += -DBOARD_FPGA
endif

ifeq ($(BOARD), PALLADIUM)
DEFS += -DBOARD_PALLADIUM
endif

ifeq ($(BOARD), QEMU)
DEFS += -DBOARD_QEMU
endif

ifeq ($(DEBUG), 1)
DEFS += -g -DENABLE_DEBUG
endif

ifeq ($(CHIP), BM1682)
DEFS += -DCONFIG_CHIP_BM1682
else ifeq ($(CHIP), BM1880)
DEFS += -DCONFIG_CHIP_BM1880
endif

ifneq ($(NPU_EN), 0)
DEFS += -DUSE_NPU_LIB
endif

DEFS += -DUSE_BMTAP
DEFS += -DBUILD_TEST_CASE_$(TEST_CASE)

ASFLAGS := \
	$(OPTFLAGS) \
	$(COMMONFLAGS)

CFLAGS := \
	$(COMMONFLAGS) \
	$(OPTFLAGS) \
	-ffunction-sections \
	-fdata-sections \
	-nostdlib

CXXFLAGS := \
	$(COMMONFLAGS) \
	$(OPTFLAGS) \
	-ffunction-sections \
	-fdata-sections \
	-nostdlib \
	-fno-threadsafe-statics \
	-fno-rtti \
	-fno-exceptions

LDFLAGS := \
	$(COMMONFLAGS) \
	-Wl,--build-id=none \
	-Wl,--cref \
	-Wl,--check-sections \
	-Wl,--gc-sections \
	-Wl,--unresolved-symbols=report-all

INC += \
	-I$(ROOT)/include \
	-I$(ROOT)/test

C_SRC += \
	$(ROOT)/common/main.c          \
	$(ROOT)/common/cli.c           \
	$(ROOT)/common/command.c       \
	$(ROOT)/common/syscalls.c      \
	$(ROOT)/common/pll.c           \
	$(ROOT)/common/timer.c         \
	$(ROOT)/common/ddr.c           \
	$(ROOT)/common/common.c
     
ifneq ($(NPU_EN), 0)
C_SRC += \
	$(ROOT)/common/bmkernel.c      \
	$(ROOT)/common/engine_io.c     \
	$(ROOT)/common/net_test.c      \
	$(ROOT)/common/gmem_utils.c
endif

ifneq ($(RUN_ENV), )
DEFS += -DRUN_ALONE
ifeq ($(RUN_ENV), SRAM)
DEFS += -DRUN_IN_SRAM
else
DEFS += -DRUN_IN_DDR
endif
C_SRC += $(ROOT)/common/system.c
include $(ROOT)/common/stdlib/stdlib.mk
else
C_SRC += $(ROOT)/common/cache.c
endif

ifeq ($(BOARD), QEMU)
C_SRC += $(ROOT)/common/uart_pl01x.c
else
C_SRC += $(ROOT)/common/uart_dw.c
endif

A_SRC +=

ifneq ($(BMTAP_SRC_PATH), $(wildcard $(BMTAP_SRC_PATH)))
$(error invalid BMTAP_SRC_PATH "$(BMTAP_SRC_PATH)")
endif

INC += \
	-I$(BMTAP_INC_PATH) \
	-I$(BMTAP_INC_PATH)/../../libbmutils/include \
	-I$(BMTAP_INC_PATH)/../../bmkernel/common/include/arch/bm188x

ifneq ($(NPU_EN), 0)
LIB_OBJ += \
	$(BMTAP_LIB_PATH)/libbmkernel.a \
	$(BMTAP_LIB_PATH)/libbmutils.a
endif

include $(ROOT)/boot/config.mk

