SOC_MODE ?= 0
PLATFORM ?= asic
PCIE_MODE_ENABLE_CPU ?= 1
CLEAN_BUILD ?= 0

ifeq ($(PLATFORM), palladium)
ccflags-y += -DPLATFORM_PALLADIUM
else ifeq ($(PLATFORM), fpga)
ccflags-y += -DPLATFORM_FPGA
else
ccflags-y += -DPLATFORM_ASIC
endif

ifneq ($(SOC_MODE), 1)
ifeq ($(PCIE_MODE_ENABLE_CPU), 1)
ccflags-y += -DPCIE_MODE_ENABLE_CPU
endif
endif

PWD := $(shell pwd)
BM1682_DIR ?= bm1682
ifeq ($(SOC_MODE), 1)
else
BM1682_OBJ += bm1682_pcie.o bm1682_irq.o
endif
BM1682_OBJ += bm1682_ddr.o bm1682_gmem.o bm1682_cdma.o bm1682_msgfifo.o bm1682_smmu.o bm1682_card.o
BM1682_OBJ := $(addprefix $(BM1682_DIR)/, $(BM1682_OBJ))


BM1684_DIR ?= bm1684
ifeq ($(SOC_MODE), 1)
else
BM1684_OBJ += bm1684_pcie.o bm1684_irq.o bm1684_ddr.o bm1684_lpddr4.o \
			  bm1684_pld_ddr.o bm1684_base64.o bm1684_jpu.o bm1684_smbus.o bm1684_ce.o
endif
BM1684_OBJ += bm1684_gmem.o bm1684_cdma.o bm1684_smmu.o bm1684_clkrst.o bm1684_card.o \
			  bm1684_msgfifo.o bm1684_perf.o bm1684_flash.o
BM1684_OBJ := $(addprefix $(BM1684_DIR)/, $(BM1684_OBJ))

BM1688_DIR ?= bm1688
ifeq ($(SOC_MODE), 1)
else
BM1688_OBJ += bm1688_pcie.o bm1688_irq.o bm1688_smbus.o  bm1688_vcodec.o
endif
BM1688_OBJ += bm1688_gmem.o bm1688_cdma.o bm1688_clkrst.o bm1688_card.o \
			  bm1688_msgfifo.o bm1688_perf.o bm1688_flash.o bm1688_timer.o bm1688_base64.o
BM1688_OBJ := $(addprefix $(BM1688_DIR)/, $(BM1688_OBJ))

VPP_DIR ?= vpp
ifeq ($(SOC_MODE), 1)
else
VPP_OBJ += vpp_platform.o bm1688_vpp.o
endif
VPP_OBJ := $(addprefix $(VPP_DIR)/, $(VPP_OBJ))

VPSS_DIR ?= bm1688/media/vpss
ccflags-y += -I$(src)/bm1688/media/vpss/common -I$(src)/bm1688/media/vpss/chip/soph
ccflags-y += -I$(src)/bm1688/media/include/common/uapi -I$(src)/bm1688/media/include/common/kapi
ifeq ($(SOC_MODE), 1)
else
ccflags-y += -I$(src)/vpp
VPSS_OBJ += common/vpss_proc.o common/vpss.o common/vpss_ioctl.o
VPSS_OBJ += chip/soph/vpss_platform.o chip/soph/vpss_core.o chip/soph/vpss_hal.o chip/soph/scaler.o
VPSS_OBJ := $(addprefix $(VPSS_DIR)/, $(VPSS_OBJ))
VPP_OBJ += $(VPSS_OBJ)
endif

BASE_DIR ?= bm1688/media/base
ifeq ($(SOC_MODE), 0)
ccflags-y += 						\
	-I$(src)/bm1688/media/include/common/uapi	\
	-I$(src)/bm1688/media/include/common/kapi	\
	-I$(src)/bm1688/media/base			\
	-I$(src)/bm1688/media/base/proc

BASE_OBJ += base.o	\
	base_common.o	\
	bind.o		\
	callback.o	\
	ion.o		\
	vb.o		\
	vbq.o

BASE_OBJ := $(addprefix $(BASE_DIR)/, $(BASE_OBJ))
endif


VPU_DIR ?= bm1688/media/vc_drv
ifeq ($(SOC_MODE), 1)
else
ccflags-y += -DENABLE_DEC
ccflags-y +=								\
	-I$(src)/$(VPU_DIR)						\
	-I$(src)/$(VPU_DIR)/common					\
	-I$(src)/$(VPU_DIR)/common/rcKernel				\
	-I$(src)/$(VPU_DIR)/common/rcKernel/float_point			\
	-I$(src)/$(VPU_DIR)/module/include				\
	-I$(src)/$(VPU_DIR)/include					\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpeg/jpuapi			\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpeg/jdi				\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpeg/sample			\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpeg/sample/helper		\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpu/bmjpuapi/inc			\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpu				\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpu/jpuapi			\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpu/jdi				\
	-I$(src)/$(VPU_DIR)/cnm_driver/jpu/include			\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu				\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/vdi				\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/vdi/linux/driver		\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/sample_v2/helper		\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/sample_v2/helper/misc	\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/sample_v2/component		\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/sample_v2/component_decoder	\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/sample_v2/component_encoder	\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/vpuapi			\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/vpuapi/coda9			\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/vpuapi/wave			\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/cvi				\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/fw/wave_521			\
	-I$(src)/$(VPU_DIR)/cnm_driver/vpu/fw/wave_517

VPU_OBJ += vc_drv.o							\
	module/src/venc.o						\
	module/src/vdec.o						\
	module/src/enc_ctx.o						\
	module/src/venc_api.o						\
	module/src/vdec_api.o						\
	module/src/jpeg_api.o						\
	module/src/venc_help.o						\
	module/src/vc_drv_proc.o					\
	module/src/venc_rc.o						\
	common/rcKernel/float_point/float_point.o			\
	common/rcKernel/float_point/soft_float.o			\
	common/drv_file.o						\
	common/vc_getopt.o						\
	common/datastructure.o						\
	cnm_driver/vpu/vdi/linux/vdi.o					\
	cnm_driver/vpu/vdi/linux/vdi_osal.o				\
	cnm_driver/vpu/vdi/linux/driver/vpu.o				\
	cnm_driver/vpu/vdi/vdi_debug.o					\
	cnm_driver/vpu/vpuapi/vpuapi.o					\
	cnm_driver/vpu/vpuapi/vpuapifunc.o				\
	cnm_driver/vpu/vpuapi/product.o					\
	cnm_driver/vpu/vpuapi/wave/wave5.o				\
	cnm_driver/vpu/vpuapi/wave/wave6.o				\
	cnm_driver/vpu/vpuapi/coda9/coda9.o				\
	cnm_driver/vpu/sample_v2/helper/main_helper.o			\
	cnm_driver/vpu/sample_v2/helper/misc/pbu.o			\
	cnm_driver/vpu/sample_v2/helper/misc/debug.o			\
	cnm_driver/jpeg/jpuapi/jpuapi.o					\
	cnm_driver/jpeg/jpuapi/jpuapifunc.o				\
	cnm_driver/jpeg/jdi/mm.o					\
	cnm_driver/jpeg/jdi/linux/jdi.o					\
	cnm_driver/jpeg/jdi/linux/driver/jpu_core_res.o			\
	cnm_driver/jpeg/jdi/linux/driver/jpeg.o				\
	cnm_driver/jpeg/sample/helper/jpulog.o				\
	cnm_driver/jpeg/sample/helper/jpuhelper.o

VPU_OBJ := $(addprefix $(VPU_DIR)/, $(VPU_OBJ))
endif


BM_KERNEL_DRIVER_OUT_DIR ?= $(PWD)/../build/driver/$(TARGET_PROJECT)
ifeq ($(SOC_MODE), 1)
  BM_MODULE_NAME = bmtpu
else
  BM_MODULE_NAME = bmsophon
endif

ccflags-y += -O3
ccflags-y += -Werror -Wall -Wno-implicit-fallthrough -Wno-unused-function -fdata-sections -Wl,-gc-sections
ifeq ($(PLATFORM), pcie_arm64)
ccflags-y += -mno-outline-atomics
endif
ifeq ("$(shell uname -m)", "loongarch64")
ccflags-y += -Wno-implicit-function-declaration
endif
ccflags-y += -I$(src) -I$(src)/bm1682 -I$(src)/bm1684 -I$(src)/bm1688

ifeq ($(SOC_MODE), 1)
  ccflags-y += -I$(src)/../../osdrv/interdrv/v2/include/common/kapi/
  ccflags-y += -DSOC_MODE
  cflags-y += -DSOC_MODE
endif

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
ifeq ($(SOC_MODE), 1)
	$(BM_MODULE_NAME)-y += bm_soc_drv.o
else
	$(BM_MODULE_NAME)-y += bm_pcie_drv.o spi.o vpu/vpu.o bm_wdt.o \
	bm_boot_info.o xmodem.o console.o bm_napi.o bm_pt.o sg_comm.o efuse.o
endif
$(BM_MODULE_NAME)-y += bm_drv.o bm_fops.o bm_irq.o i2c.o pwm.o  \
	bm_memcpy.o bm_ctl.o bm_fw.o bm_cdma.o \
	bm_msgfifo.o bm_thread.o bm_api.o \
	bm_timer.o bm_io.o bm_trace.o bm_attr.o bm_gmem.o \
	bm_bgm.o bm_genalloc.o gpio.o uart.o bm_debug.o bm_card.o bm_monitor.o \
	bm_pm.o

$(BM_MODULE_NAME)-y += $(BM1682_OBJ)
$(BM_MODULE_NAME)-y += $(BM1684_OBJ)
$(BM_MODULE_NAME)-y += $(BM1688_OBJ)
$(BM_MODULE_NAME)-y += $(VPP_OBJ)
ifeq ($(SOC_MODE), 0)
$(BM_MODULE_NAME)-y += $(BASE_OBJ)
$(BM_MODULE_NAME)-y += $(VPU_OBJ)
endif

ifeq ($(SOC_MODE), 1)
  ifndef SOC_LINUX_DIR
      $(error "subtype of soc kernel to build not set!")
  endif
  LINUX_SRC := $(SOC_LINUX_DIR)
  KBUILD_CFLAGS += -DSOC_MODE
else
  ifndef CROSS_COMPILE
    ifeq ($(findstring Debian, $(shell uname -v)),Debian)
      LINUX_SRC = /usr/src/linux-headers-`uname -r`
    else ifeq ($(findstring Ubuntu,$(shell uname -v)),Ubuntu)
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
    else ifeq ($(findstring Ubuntu,$(shell cat /etc/issue)),Ubuntu)
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
    else ifeq ($(findstring Kylin,$(shell lsb_release -a)),Kylin)
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
    else ifeq ($(findstring CentOS,$(shell cat /etc/redhat-release)),CentOS)
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
      CENTOS_KERNEL = $(shell uname -r |cut -d - -f1)
      CENTOS_RELEASE =  $(shell uname -r |cut -d - -f 2 |cut -d . -f 1-3)
      CENTOS_RELEASE_FIX = $(shell echo $(CENTOS_RELEASE)|cut -d . -f1)
      cflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
      ccflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
    else ifeq ($(findstring CentOS,$(shell cat /etc/issue)),CentOS)
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
      CENTOS_KERNEL = $(shell uname -r |cut -d - -f1)
      CENTOS_RELEASE =  $(shell uname -r |cut -d - -f 2 |cut -d . -f 1-3)
      CENTOS_RELEASE_FIX = $(shell echo $(CENTOS_RELEASE)|cut -d . -f1)
      cflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
      ccflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
    else ifeq ($(findstring Red Hat,$(shell cat /etc/redhat-release)),Red Hat)
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
      CENTOS_KERNEL = $(shell uname -r |cut -d - -f1)
      CENTOS_RELEASE =  $(shell uname -r |cut -d - -f 2 |cut -d . -f 1-3)
      CENTOS_RELEASE_FIX = $(shell echo $(CENTOS_RELEASE)|cut -d . -f1)
      cflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
      ccflags-y += -DCENTOS_KERNEL_FIX=$(CENTOS_RELEASE_FIX)
    else
      LINUX_SRC = /lib/modules/$(shell uname -r)/build
    endif
  else
    ifndef LINUX_SRC
      $(error "LINUX_SRC not set!")
    endif
    ifndef ARCH
      $(error "ARCH not set!")
    endif
  endif
endif

all: clean
ifeq ($(SOC_MODE), 1)
	mkdir -p $(BM_KERNEL_DRIVER_OUT_DIR)
	make ARCH=arm64 -C $(LINUX_SRC) M=$(PWD) modules
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
	@make -C $(LINUX_SRC) M=$(PWD) clean
else
ifndef CROSS_COMPILE
	make -C $(LINUX_SRC) M=$(PWD) modules
else
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SRC) M=$(PWD) modules
endif
ifeq ($(CLEAN_BUILD), 1)
ifneq ($(TARGET_PROJECT),)
	mkdir -p $(BM_KERNEL_DRIVER_OUT_DIR)
	mv *.ko $(BM_KERNEL_DRIVER_OUT_DIR)/
else
	mv *.ko $(PWD)/../
endif
	@make -C $(LINUX_SRC) M=$(PWD) clean
endif
endif

clean:
ifeq ($(SOC_MODE), 1)
	make -C $(LINUX_SRC) M=$(PWD) clean
else
	make -C $(LINUX_SRC) M=$(PWD) clean
endif

install:
	cp -u $(BM_MODULE_NAME).ko $(BM_KERNEL_DRIVER_OUT_DIR)
