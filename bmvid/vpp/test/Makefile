#compile for testing video,vpp,jpeg on SA3 or SE3 platform
# README
# CHIP: bm1682, bm1880
# SUBTYPE:
#     for bm1682:
#             asic: for EVB, single chip board and high density server and other 8g memory layout
#             *_4g: for 4g memory layout
#     for bm1880:
#             asic: for real chip
# PRODUCTFORM:
#     for bm1682:
#             soc: default, for soc products, includeing box and hds
#             pcie: pcie board
#     for bm1684:
#             soc:
#
#chip=bm1880
#subtype=asic
vpp_dir = $(PWD)/$(CHIP)

productform ?= soc

.PHONY:all clean install

exclude_dirs := Makefile

ifeq ($(productform),pcie) # pcie mode
    CROSS_CC_PREFIX =
else ifeq ($(productform),pcie_mips64)
    CROSS_CC_PREFIX = mips-linux-gnu-
else ifeq ($(productform),pcie_sw64) # pcie mode
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
else ifeq ($(productform),pcie_loongarch64)
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
else # pcie_arm64 and soc mode
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif

ifeq ($(CHIP),bm1684)
productform_mode=${productform}

ifeq ($(productform_mode),pcie_arm64)
productform_mode=pcie
endif

ifeq ($(productform_mode),pcie_loongarch64)
productform_mode=pcie
endif

ifeq ($(SUBTYPE), cmodel)
productform_mode=cmodel
endif
dirs := $(shell ls $(vpp_dir)/${productform_mode})
dirs := $(filter-out $(exclude_dirs),$(dirs))

all:
	$(foreach N,$(dirs),make CROSS_CC_PREFIX=${CROSS_CC_PREFIX} DEBUG=${DEBUG} DESTDIR=${DESTDIR} VPP_LIB_DIR=${VPP_LIB_DIR} productform=${productform} -C $(vpp_dir)/${productform_mode}/$(N);)
clean:	
	$(foreach N,$(dirs),make CROSS_CC_PREFIX=${CROSS_CC_PREFIX} DEBUG=${DEBUG} DESTDIR=${DESTDIR} VPP_LIB_DIR=${VPP_LIB_DIR} -C $(vpp_dir)/${productform_mode}/$(N) clean;)
install:
	$(foreach N,$(dirs),make CROSS_CC_PREFIX=${CROSS_CC_PREFIX} DEBUG=${DEBUG} DESTDIR=${DESTDIR} VPP_LIB_DIR=${VPP_LIB_DIR} -C $(vpp_dir)/${productform_mode}/$(N) install;)
endif


ifeq ($(CHIP),bm1682)
dirs := $(shell ls $(vpp_dir)/)
exclude_dirs += vppwithOpencvVideoDecode vppwithOpencvImageDecode
dirs := $(filter-out $(exclude_dirs),$(dirs))

all:
	$(foreach N,$(dirs),make DEBUG=${DEBUG} DESTDIR=${DESTDIR} VPP_LIB_DIR=${VPP_LIB_DIR} productform=${productform} -C $(vpp_dir)/$(N);)
clean:	
	$(foreach N,$(dirs),make DEBUG=${DEBUG} DESTDIR=${DESTDIR} VPP_LIB_DIR=${VPP_LIB_DIR} -C $(vpp_dir)/$(N) clean;)
install:
	$(foreach N,$(dirs),make DEBUG=${DEBUG} DESTDIR=${DESTDIR} VPP_LIB_DIR=${VPP_LIB_DIR} -C $(vpp_dir)/$(N) install;)
endif
