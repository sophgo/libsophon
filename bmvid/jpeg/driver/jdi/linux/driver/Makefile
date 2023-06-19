
CHIP=${CHIP:-bm1682}
DEBUG=${DEBUG:-0}
ION=${ION:-0}

ifeq ($(CHIP), bm1682)
  chip_type = CHIP_BM1682
else ifeq ($(CHIP), bm1880)
  chip_type = CHIP_BM1880
else ifeq ($(CHIP), bm1684)
  chip_type = CHIP_BM1684
else
  $(error DO NOT DEFINE CHIP)
endif

ifeq ($(patsubst %_4g,_4g, $(SUBTYPE)), _4g)
  sub_type = ASIC_BOX
else 
  sub_type = ASIC_EVB
endif

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG), 1)
  DEBFLAGS = -Wall -O	-g # "-O" is needed to expand inlines
else
  DEBFLAGS = -Wall -O2
endif

export CC="$(CROSS_COMPILE)gcc"
export AR="$(CROSS_COMPILE)ar"
export CXX="${CROSS_COMPILE}g++"
export AS="${CROSS_COMPILE}as"
export LD="${CROSS_COMPILE}ld"
export RANLIB="${CROSS_COMPILE}ranlib"
export READELF="${CROSS_COMPILE}readelf"
export STRIP="${CROSS_COMPILE}strip"

EXTRA_CFLAGS +=	$(DEBFLAGS) -I$(LDDINCDIR)
EXTRA_CFLAGS += -D$(chip_type) -D$(sub_type)
ifeq ($(ION), 1)
  EXTRA_CFLAGS += -DUSE_ION_MEMORY
endif

ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= jpu.o

else

KERNELDIR ?= /home/jinf/MyWorks/Bitmain/BM1682/linux-linaro-stable/linux/bm1682_asic
#KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD	  := $(shell pwd)
ifeq ($(KERNELDIR), )
  $(error "kernel header path is empty, please input it!")
endif


default:
	$(MAKE)	-C $(KERNELDIR)	M=$(PWD) LDDINCDIR=$(PWD)/../include modules

endif


clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
	rm -rf Module.symvers
	rm -rf modules.order

depend .depend dep:
	$(CC) $(CFLAGS)	-M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include	.depend
endif
