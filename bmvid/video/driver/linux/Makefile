# Comment/uncomment the	following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O	-g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

export CC="$(CROSS_COMPILE)gcc"
export AR="$(CROSS_COMPILE)ar"
export CXX="${CROSS_COMPILE}g++"
export AS="${CROSS_COMPILE}as"
export LD="${CROSS_COMPILE}ld"
export RANLIB="${CROSS_COMPILE}ranlib"
export READELF="${CROSS_COMPILE}readelf"
export STRIP="${CROSS_COMPILE}strip"

ifeq ($(CHIP), bm1880)
  DEBFLAGS += -DCHIP_BM1880
endif

ifeq ($(CHIP), bm1682)
  DEBFLAGS += -DCHIP_BM1682
endif

ifeq ($(ION), 1)
  DEBFLAGS += -DBM_ION_MEM
endif

ifeq ($(CHIP), bm1684)
  DEBFLAGS += -DCHIP_BM1684
endif

ifeq ($(patsubst %_4g,_4g, $(SUBTYPE)), _4g)
  DEBFLAGS += -DASIC_BOX
else
  DEBFLAGS += -DASIC_EVB
endif

LDDINCDIR = -I$(BMVID_TOP_DIR)/video/driver
EXTRA_CFLAGS +=	$(DEBFLAGS) $(LDDINCDIR)
ifeq ($(PRODUCTFORM),pcie_mips64)
  CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
  LDFLAGS += -Wl,-melf64ltsmip
endif

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= vpu.o

else

KERNELDIR ?= /home/kutey/work/bm1880/linux-linaro-stable/linux/bm1880_palladium
//KERNELDIR ?= /lib/modules/$(shell uname -r)/build
ifeq ($(KERNELDIR), )
  $(error "kernel header path is empty, please input it!")
endif

PWD	  := $(shell pwd)

default:
	$(MAKE)	-C $(KERNELDIR)	M=$(PWD) modules

endif



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(CFLAGS)	-M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include	.depend
endif
