INC += \
	-I$(ROOT)/test/$(TEST_CASE) 

C_SRC += 

LIB_OBJ += \
        $(ROOT)/test/$(TEST_CASE)/libbmvideo.a
#        $(ROOT)/test/$(TEST_CASE)/libtheoraparser.a

ifeq ($(RUN_ENV), ALONE)
CFLAGS += -mstrict-align
endif

SUBTYPE ?= asic

CXXFLAGS +=

LDFLAGS +=

LIB_VID_EXIST = $(wildcard $(ROOT)/test/$(TEST_CASE)/libbmvideo.a)

ifeq ($(LIB_VID_EXIST), )
vid_lib: $(ROOT)/../../driver/Makefile
	make -C $(ROOT)/../../driver -f $^ clean
	# SUBTYPE: palladium, fpga, asic *_4g
	make -C $(ROOT)/../../driver -f $^ CHIP=bm1880 PRODUCT=boda OS=none BUILD_ORG_TEST=yes DEBUG=0 SUBTYPE=${SUBTYPE}
	cp -f $(ROOT)/../../driver/release/libbmvideo.a $(ROOT)/test/$(TEST_CASE)
	cp -f $(ROOT)/../../driver/bin/dec_test.h $(ROOT)/test/$(TEST_CASE)
endif

