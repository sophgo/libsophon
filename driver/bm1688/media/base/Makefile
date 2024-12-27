include $(PWD)/../Makefile.interdrv.param

# Specify the kernel version
KVERSION := $(shell uname -r)

# Specify the kernel build directory
KERNEL_DIR := /lib/modules/$(KVERSION)/build
#KERNEL_DIR := /lib/modules/5.4.0-200-generic/build
CONFIG_LOG = 1

$(CVIARCH_L)_base-objs = base.o \
				base_common.o \
				bind.o \
				callback.o \
				ion.o \
				vb.o \
				vbq.o \
				proc/log_proc.o \
				proc/sys_proc.o \
				proc/vb_proc.o



obj-m += $(CVIARCH_L)_base.o

ccflags-y += -I$(srctree)/drivers/tee \
             -I$(PWD)/../include/common/uapi/ \
             -I$(PWD)/../include/common/kapi/ \
             -I$(PWD)/../base/ \
             -I$(PWD)/../include/chip/$(CVIARCH_L)/uapi/ \
             -I$(PWD)/../sys/common/ \
             -I$(PWD)/../sys/common/uapi \
             -I$(srctree)/drivers/staging/android \
             -I$(PWD)/proc \

ifeq ($(CONFIG_LOG), 1)
ccflags-y += -DCONFIG_LOG
endif

ifeq ($(SUBTYPE), fpga)
ccflags-y += -DDRV_FPGA_PORTING
endif

ccflags-y += -DARCH_X86

ccflags-y += $(INTRERDRV_FLAGS)

ccflags-y +=-Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare


all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	@rm -f *.o .*.cmd modules.order Module.symvers *.ko *.mod *.mod.c
	@rm -rf .tmp_versions
	@find . -name \*.o* -type f -delete
