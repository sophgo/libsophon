INC += \
	-I$(ROOT)/test/$(TEST_CASE) 

C_SRC += 

DEFS += -DWAVE512_FPGA -DWAVE512
LIB_OBJ += \
        $(ROOT)/test/$(TEST_CASE)/libbmvideo.a
#        $(ROOT)/test/$(TEST_CASE)/libtheoraparser.a

ifeq ($(RUN_ENV), ALONE)
CFLAGS += -mstrict-align
endif

SUBTYPE ?= fpga

CXXFLAGS +=

LDFLAGS +=

LIB_VID_EXIST = $(wildcard $(ROOT)/test/$(TEST_CASE)/libbmvideo.a)

##############################################################
# WMMU
##############################################################
# MMU Config
SYSTEM_ID  ?= 0
CHANNEL_ID ?= 0
ENABLE_MMU ?= 0
REMAP_MMU  ?= 0
MULTI_CHAN ?= 0
MMU_PAGE   ?= 0

WMMU_DEFINES := SYSTEM_ID=${SYSTEM_ID} CHANNEL_ID=${CHANNEL_ID} ENABLE_MMU=${ENABLE_MMU} REMAP_MMU=${REMAP_MMU} MULTI_CHAN=${MULTI_CHAN} MMU_PAGE=${MMU_PAGE} USE_MEMSET=0

ifeq ($(LIB_VID_EXIST), )
PHONY: vid_lib all
vid_lib: $(ROOT)/../../driver/Makefile
	make -C $(ROOT)/../../driver -f $^ clean
	make -C $(ROOT)/../../driver -f $^ CHIP=bm1684 PRODUCT=wave OS=none BUILD_ORG_TEST=yes PRODUCTFORM=soc DEBUG=1 SUBTYPE=${SUBTYPE} ${WMMU_DEFINES}
	cp -f $(ROOT)/../../driver/release/libbmvideo.a $(ROOT)/test/$(TEST_CASE)
endif

