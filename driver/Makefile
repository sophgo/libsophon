PLATFORM ?= asic
CLEAN_BUILD ?= 0

PWD := $(shell pwd)

BM_KERNEL_DRIVER_OUT_DIR ?= $(PWD)/../build/driver/$(TARGET_PROJECT)
BM_MODULE_NAME = bmtpu

ccflags-y += -Os -Wall -Werror -Wno-unused-function -Wunused-variable -Wunused -Werror=unused
ifeq ("$(shell uname -m)", "loongarch64")
ccflags-y += -Wno-implicit-function-declaration
endif
ccflags-y += -I$(src)

FW_SIMPLE ?= 0
ifeq ($(FW_SIMPLE),1)
  ccflags-y += -DFW_SIMPLE
endif

obj-m += $(BM_MODULE_NAME).o

$(BM_MODULE_NAME)-y += bm_soc_drv.o bm_drv.o bm_fops.o \
	bm_ctl.o bm_gmem.o \
	bm_bgm.o bm_genalloc.o \
	bm_common.o bm_clkrst.o

ifndef SOC_LINUX_DIR
    $(error "subtype of soc kernel to build not set!")
endif
LINUX_SRC := $(SOC_LINUX_DIR)
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
