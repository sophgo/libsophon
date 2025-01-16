PLATFORM ?= asic
PCIE_MODE_ENABLE_CPU ?= 1
CLEAN_BUILD ?= 0


ifeq ($(PLATFORM), asic)
ccflags-y += -DPLATFORM_ASIC
endif
ifeq ($(PLATFORM), palladium)
ccflags-y += -DPLATFORM_PALLADIUM
endif
ifeq ($(PLATFORM), fpga)
ccflags-y += -DPLATFORM_FPGA
endif


PWD := $(shell pwd)

define ADD_SOC_OBJECTS
  $(eval $(1)_DIR ?= $(1))
  $(if $(filter 1,1),\
    ,\
    $(eval $(1)_OBJ += $(2))\
  )
  $(eval $(1)_OBJ += $(3))
  $(eval $(1)_OBJ := $(addprefix $($(1)_DIR)/, $($(1)_OBJ)))
endef


# SGTPUV8
$(eval $(call ADD_SOC_OBJECTS,SGTPUV8,\
SGTPUV8_irq.o ddr/ddr.o ddr/ddr_sys_bring_up_pld.o ddr/ddr_sys_pld.o ddr/ddr_config/lp4_x16_4ch_pld/ddrc_init.o,\
SGTPUV8_gmem.o SGTPUV8_clkrst.o SGTPUV8_card.o SGTPUV8_perf.o SGTPUV8_timer.o))


BM_KERNEL_DRIVER_OUT_DIR ?= $(PWD)/../build/driver/$(TARGET_PROJECT)
BM_MODULE_NAME = bmtpu

ccflags-y += -O3 -Wall -Werror -Wno-unused-function
ifeq ("$(shell uname -m)", "loongarch64")
ccflags-y += -Wno-implicit-function-declaration
endif
ccflags-y += -I$(src)  -I$(src)/SGTPUV8 \
       -I$(src)/SGTPUV8/ddr \
       -I$(src)/SGTPUV8/ddr/include \
       -I$(src)/SGTPUV8/ddr/ddr_config/lp4_x16_4ch_pld \
       -I$(src)/../../osdrv/interdrv/v2/include/common/kapi/
ccflags-y += -DSOC_MODE
cflags-y += -DSOC_MODE

FW_SIMPLE ?= 0
ifeq ($(FW_SIMPLE),1)
  ccflags-y += -DFW_SIMPLE
endif

SYNC_API_INT_MODE ?= 1
ifeq ($(SYNC_API_INT_MODE),1)
  ccflags-y += -DSYNC_API_INT_MODE=1
else
  ccflags-y += -DSYNC_API_INT_MODE=0
endif
obj-m += $(BM_MODULE_NAME).o

$(BM_MODULE_NAME)-y += bm_soc_drv.o bm_drv.o bm_fops.o bm_irq.o \
	bm_memcpy.o bm_ctl.o \
	bm_thread.o bm_api.o \
	bm_timer.o bm_io.o bm_trace.o bm_attr.o bm_gmem.o \
	bm_bgm.o bm_genalloc.o bm_debug.o bm_card.o bm_monitor.o bm_common.o

$(BM_MODULE_NAME)-y += $(SGTPUV8_OBJ)
$(BM_MODULE_NAME)-y += $(vpp_OBJ)

ifndef SOC_LINUX_DIR
    $(error "subtype of soc kernel to build not set!")
endif
LINUX_SRC := $(SOC_LINUX_DIR)
KBUILD_CFLAGS += -DSOC_MODE

all: clean
	mkdir -p $(BM_KERNEL_DRIVER_OUT_DIR)
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SRC) M=$(PWD) modules
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
	@make -C $(LINUX_SRC) M=$(PWD) clean

clean:
	make -C $(LINUX_SRC) M=$(PWD) clean

install:
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
