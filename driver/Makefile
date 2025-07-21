PLATFORM ?= asic
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


BM_KERNEL_DRIVER_OUT_DIR ?= $(PWD)/../build/driver/$(TARGET_PROJECT)
BM_MODULE_NAME = bmtpu

ccflags-y += -Os -Wall -Werror -Wno-unused-function
ifeq ("$(shell uname -m)", "loongarch64")
ccflags-y += -Wno-implicit-function-declaration
endif
ccflags-y += -I$(src)  -I$(src)/SGTPUV8 \
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
	bm_ctl.o bm_io.o bm_attr.o bm_gmem.o \
	bm_bgm.o bm_genalloc.o \
	bm_common.o bm_clkrst.o

ifndef SOC_LINUX_DIR
    $(error "subtype of soc kernel to build not set!")
endif
LINUX_SRC := $(SOC_LINUX_DIR)
KBUILD_CFLAGS += -DSOC_MODE
KBUILD_CFLAGS += -g


all: clean
	mkdir -p $(BM_KERNEL_DRIVER_OUT_DIR)
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SRC) M=$(PWD) modules
	# $(CROSS_COMPILE)strip --strip-unneeded $(BM_MODULE_NAME).ko
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
	@make -C $(LINUX_SRC) M=$(PWD) clean

clean:
	make -C $(LINUX_SRC) M=$(PWD) clean

install:
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
