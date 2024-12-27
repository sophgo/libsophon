include $(PWD)/../Makefile.interdrv.param

# Specify the kernel version
KVERSION := $(shell uname -r)

# Specify the kernel build directory
KERNEL_DIR := /lib/modules/$(KVERSION)/build
#KERNEL_DIR := /lib/modules/5.4.0-200-generic/build
VC_DRV_HEADER := -I$(PWD)									\
				-I$(PWD)/common								\
				-I$(PWD)/common/rcKernel					\
				-I$(PWD)/common/rcKernel/float_point	\
				-I$(PWD)/module/include						\
				-I$(PWD)/include

COMMON_DRV_HEADER := -I$(PWD)/../include/common/uapi 			\
					-I$(PWD)/../include/chip/$(CVIARCH_L)/uapi/ \
					-I$(PWD)/../include/common/kapi/ 			\
					-I$(PWD)/../base/

JPU_INC := -I$(PWD)/cnm_driver/jpeg/jpuapi \
			-I$(PWD)/cnm_driver/jpeg/jdi \
			-I$(PWD)/cnm_driver/jpeg/sample \
			-I$(PWD)/cnm_driver/jpeg/sample/helper \
			-I$(PWD)/cnm_driver/jpu/bmjpuapi/inc \
			-I$(PWD)/cnm_driver/jpu \
			-I$(PWD)/cnm_driver/jpu/jpuapi \
			-I$(PWD)/cnm_driver/jpu/jdi \
			-I$(PWD)/cnm_driver/jpu/include


VPU_INC := -I$(PWD)/cnm_driver/vpu			\
				-I$(PWD)/cnm_driver/vpu/vdi			\
				-I$(PWD)/cnm_driver/vpu/vdi/linux/driver \
				-I$(PWD)/cnm_driver/vpu/sample_v2/helper	\
				-I$(PWD)/cnm_driver/vpu/sample_v2/helper/misc	\
				-I$(PWD)/cnm_driver/vpu/sample_v2/component	\
				-I$(PWD)/cnm_driver/vpu/sample_v2/component_decoder \
				-I$(PWD)/cnm_driver/vpu/sample_v2/component_encoder \
				-I$(PWD)/cnm_driver/vpu/vpuapi		\
				-I$(PWD)/cnm_driver/vpu/vpuapi/coda9\
				-I$(PWD)/cnm_driver/vpu/vpuapi/wave \
				-I$(PWD)/cnm_driver/vpu/cvi \
				-I$(PWD)/cnm_driver/vpu/fw/wave_521 \
				-I$(PWD)/cnm_driver/vpu/fw/wave_517


MODULE_SOURCES = vc_drv.c 	\
		module/src/venc.c 	\
		module/src/vdec.c	\
		module/src/enc_ctx.c	\
		module/src/venc_api.c \
		module/src/vdec_api.c \
		module/src/jpeg_api.c \
		module/src/venc_help.c \
		module/src/vc_drv_proc.c \
		module/src/venc_rc.c \
		common/rcKernel/float_point/float_point.c \
		common/rcKernel/float_point/soft_float.c \
		common/drv_file.c \
		common/vc_getopt.c \
		common/datastructure.c


VPU_SOURCES = cnm_driver/vpu/vdi/linux/vdi.c \
		cnm_driver/vpu/vdi/linux/vdi_osal.c \
		cnm_driver/vpu/vdi/linux/driver/vpu.c \
		cnm_driver/vpu/vdi/vdi_debug.c \
		cnm_driver/vpu/vdi/vdi_debug.c \
		cnm_driver/vpu/vpuapi/vpuapi.c \
		cnm_driver/vpu/vpuapi/vpuapifunc.c \
		cnm_driver/vpu/vpuapi/product.c \
		cnm_driver/vpu/vpuapi/wave/wave5.c \
		cnm_driver/vpu/vpuapi/wave/wave6.c \
		cnm_driver/vpu/vpuapi/coda9/coda9.c \
		cnm_driver/vpu/sample_v2/helper/main_helper.c \
		cnm_driver/vpu/sample_v2/helper/misc/pbu.c \
		cnm_driver/vpu/sample_v2/helper/misc/debug.c


JPU_SOURCES =	cnm_driver/jpeg/jpuapi/jpuapi.c 	\
				cnm_driver/jpeg/jpuapi/jpuapifunc.c	\
				cnm_driver/jpeg/jdi/mm.c			\
				cnm_driver/jpeg/jdi/linux/jdi.c		\
				cnm_driver/jpeg/jdi/linux/driver/jpu_core_res.c	\
				cnm_driver/jpeg/jdi/linux/driver/jpeg.c	\
				cnm_driver/jpeg/sample/helper/jpulog.c	\
				cnm_driver/jpeg/sample/helper/jpuhelper.c


MODULE_OBJS=$(patsubst %.c,%.o,$(MODULE_SOURCES))
VPU_OBJS=$(patsubst %.c,%.o,$(VPU_SOURCES))
JPU_OBJS=$(patsubst %.c,%.o,$(JPU_SOURCES))

VC_DRV_HEADER += $(COMMON_DRV_HEADER)
VC_DRV_HEADER += $(JPU_INC)
VC_DRV_HEADER += $(VPU_INC)

KBUILD_EXTRA_SYMBOLS += $(PWD)/../base/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(PWD)/../sys/Module.symvers

ccflags-y += $(VC_DRV_HEADER) -DUSE_KERNEL_MODE -DKERNEL_MODE -DENABLE_DEC -DMJPEG_INTERFACE_API -DCNM_WAVE_SERIES

ccflags-y += -DRV_TEST

ifneq ($(KERNELRELEASE),)
# call from kernel build system


$(CVIARCH_L)_vc_drv-objs :=	$(MODULE_OBJS)
$(CVIARCH_L)_vc_drv-objs +=	$(VPU_OBJS)
$(CVIARCH_L)_vc_drv-objs +=	$(JPU_OBJS)
obj-m	:= $(CVIARCH_L)_vc_drv.o

else

# ifeq ($(KERNELDIR), )
#   $(error "kernel header path is empty, please input it!")
# endif

PWD	  := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

endif



clean:
	rm -rf *.o *~ core .depend *.mod *.order *.symvers .*.cmd *.ko
	rm -rf *.mod.c .tmp_versions *.o *.o.d module/src/*.o*
	rm -rf base/.*.o.cmd*
	rm -rf common/rcKernel/float_point/*.o*
	rm -rf common/rcKernel/float_point/*.o*
	rm -rf common/rcKernel/float_point/.*.o.cmd
	rm -rf common/rcKernel/*.o*
	rm -rf common/*.o*
	rm -rf common/.*.o.cmd*
	rm -rf module/src/.*.o.cmd
	rm -rf common/rcKernel/.*.o.cmd
	rm -rf cnm_driver/vpu/cvi/.*.o.cmd*
	rm -rf cnm_driver/vpu/vdi/.*.o.cmd*
	rm -rf cnm_driver/vpu/vdi/linux/driver/*.ko
	rm -rf cnm_driver/vpu/vdi/linux/driver/*.o*
	rm -rf cnm_driver/vpu/vdi/linux/driver/.*.o*
	rm -rf cnm_driver/vpu/vdi/linux/driver/.*.cmd*
	rm -rf cnm_driver/vpu/vdi/linux/.*.o.cmd*
	rm -rf cnm_driver/vpu/vpuapi/coda9/.*.o.cmd*
	rm -rf cnm_driver/vpu/vpuapi/wave/.*.o.cmd*
	rm -rf cnm_driver/vpu/vpuapi/.*.o.cmd*
	rm -rf cnm_driver/vpu/encoder/src/.*.o.cmd*
	rm -rf cnm_driver/vpu/sample_v2/.*.o.cmd*
	rm -rf cnm_driver/vpu/sample_v2/helper/misc/.*.o.cmd*
	rm -rf cnm_driver/vpu/sample_v2/helper/.*.o.cmd*
	rm -rf cnm_driver/jpeg/jpuapi/.*.o.cmd
	rm -rf cnm_driver/jpeg/jdi/.*.o.cmd
	rm -rf cnm_driver/jpeg/jdi/linux/.*.o.cmd
	rm -rf cnm_driver/jpeg/jdi/linux/driver/.*.cmd*
	rm -rf cnm_driver/jpeg/jdi/linux/driver/*.ko
	rm -rf cnm_driver/jpeg/jdi/linux/driver/*.o*
	rm -rf cnm_driver/jpeg/jdi/linux/driver/.*.o*
	rm -rf cnm_driver/jpeg/sample/.*.o.cmd
	rm -rf cnm_driver/jpeg/sample/helper/.*.o.cmd
	rm -rf .*.o.d
	rm -rf $(MODULE_OBJS) $(VPU_OBJS) $(JPU_OBJS)

depend .depend dep:
	$(CC) $(CFLAGS)	-M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include	.depend
endif
