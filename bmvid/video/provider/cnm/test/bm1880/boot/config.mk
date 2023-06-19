ifneq ($(RUN_ENV), )
ifeq ($(RUN_ENV), SRAM)
LINK := $(ROOT)/boot/bm1880_sram.lds
else
LINK := $(ROOT)/boot/bm1880_ddr.lds
endif
A_SRC += $(ROOT)/boot/start_alone.S
else
LINK := $(ROOT)/boot/bm1880.lds
A_SRC += $(ROOT)/boot/start.S
A_SRC += $(ROOT)/boot/cache.S
endif
