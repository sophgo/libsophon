# ----------------------------------------------------------------------
#
# Project: C&M Video decoder sample
#
# ----------------------------------------------------------------------
.PHONY: CREATE_DIR

# ----------------------------------------------------------------------
#configurable parameter
#   CHIP:           bm1682, bm1880, bm1684
#   PRODUCT:        boda, wave
#   PRODUCTFORM: 	soc, pcie, pcie_sw64, pcie_arm64, pcie_mips64, pcie_loongarch64
#   SUBTYPE:        fvp, fpga, asic, box, *_4g, palladium
#   OS:             linux, none
#   BUILD_ORG_TEST: yes/no
#   DEBUG:          0/1
# ----------------------------------------------------------------------

CHIP ?= bm1682
USE_FFMPEG  = no
USE_PTHREAD = yes
USE_RTL_SIM = no
BUILD_ORG_TEST ?= no
SUBTYPE ?= asic
PRODUCT ?= boda
INSTALL_DIR ?= ./release
DEBUG ?= 0
OS ?= linux
BMVID_TOP_DIR ?= ../../../
BMVID_ROOT ?= $(BMVID_TOP_DIR)

DEFINES =
DESTDIR ?= /usr/local/lib

ifeq ($(PRODUCT),boda)
product := CODA960
ifeq ($(SUBTYPE), fvp)      
sub_type := CODA960_FVP    
else ifeq ($(SUBTYPE), palladium)
sub_type := CODA960_PALLADIUM
else
sub_type := CODA960_FPGA   # default asic subtype
endif
endif

ifeq ($(PRODUCT),wave)
product := WAVE512
ifeq ($(SUBTYPE), fvp)
sub_type := WAVE512_FVP
else ifeq ($(SUBTYPE), palladium)
sub_type := WAVE512_PALLADIUM
ifeq ($(USE_MEMSET), 1)
DEFINES += -DUSE_MEMSET
endif
else
sub_type := WAVE512_FPGA       # default asic subtype
endif
endif

$(shell cp $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component_list_all.h $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component/component_list.h)
ifeq ($(OS),linux)
ifeq ($(PRODUCTFORM),pcie)
BUILD_CONFIGURATION = NativeLinux
else ifeq ($(PRODUCTFORM),pcie_mips64)
BUILD_CONFIGURATION = MipsLinux
else ifeq ($(PRODUCTFORM),pcie_sw64)
BUILD_CONFIGURATION = SunwayLinux
else ifeq ($(PRODUCTFORM),pcie_loongarch64)
BUILD_CONFIGURATION = LoongLinux
else # pcie_arm64 and soc
BUILD_CONFIGURATION = EmbeddedLinux
endif
endif
ifeq ($(OS),none)
ifeq ($(MAKECMDGOALS), test)
$(error test is only supported in linux environment)
endif
BUILD_CONFIGURATION = NonOS
endif

ifeq ($(CHIP), bm1682)
chip_type = CHIP_BM1682
ifeq ($(patsubst %_4g,_4g, $(SUBTYPE)), _4g)
card_type := ASIC_BOX
else
card_type := ASIC_EVB   # default asic mode
endif
endif
ifeq ($(CHIP), bm1880)
chip_type = CHIP_BM1880
ifeq ($(PRODUCT), wave)
$(error wave is not supported in chip BM1880)
endif
endif
ifeq ($(CHIP), bm1684)
chip_type = CHIP_BM1684
ifeq ($(PRODUCT), boda)
$(error boda is not supported in chip BM1684)
endif
endif

ifneq ($(card_type),)
DEFINES += -D$(card_type)
endif

UNAME = $(shell uname -a)
ifneq (,$(findstring i386, $(UNAME)))
    USE_32BIT = yes
endif

REFC    := 0

ifeq ($(USE_32BIT), yes)
PLATFORM    = nativelinux
else
PLATFORM    = nativelinux_64bit
endif

CROSS_CC_PREFIX = 
VDI_C           = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/linux/vdi.c
VDI_OSAL_C      = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/linux/vdi_osal.c
VDI_OSAL_O      = vdi_osal.o
MM_C            = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/mm.c
VDI_VPATH       = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/linux

PLATFORM_FLAGS  = 

ifeq ("$(BUILD_CONFIGURATION)", "NonOS")
    CROSS_CC_PREFIX = aarch64-elf-
    VDI_C           = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/nonos/vdi.c $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/nonos/vdi_mmu.c
    VDI_OSAL_C      = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/nonos/vdi_osal.c
    MM_C            = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/mm.c
    USE_FFMPEG      = no
    USE_PTHREAD     = no
    PLATFORM        = none
    DEFINES        += -DLIB_C_STUB -D$(sub_type)
    VDI_VPATH       = $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi/nonos
    PLATFORM_FLAGS  = -ftabstop=4 -nostdlib $(VPU_WMMU_DEFINES)
    SLIBNAME        = libbmvideo.a
include config.mk
endif
ifeq ("$(BUILD_CONFIGURATION)", "EmbeddedLinux")
    CROSS_CC_PREFIX = aarch64-linux-gnu-
    PLATFORM        = armlinux
    SLIBNAME        = libbmvideo.so
endif
ifeq ("$(BUILD_CONFIGURATION)", "NativeLinux")
    CROSS_CC_PREFIX =
    PLATFORM    = nativelinux_64bit
    SLIBNAME    = libbmvideo.so
endif
ifeq ("$(BUILD_CONFIGURATION)", "MipsLinux")
    CROSS_CC_PREFIX = mips-linux-gnu-
    PLATFORM        = mipslinux
    SLIBNAME        = libbmvideo.so
endif
ifeq ("$(BUILD_CONFIGURATION)", "LoongLinux")
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
    PLATFORM        = loongLinux
    SLIBNAME        = libbmvideo.so
endif
ifeq ("$(BUILD_CONFIGURATION)", "SunwayLinux")
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
    PLATFORM        = sunwaylinux
    SLIBNAME        = libbmvideo.so
endif

ifneq ($(PRODUCTFORM),soc)
    CFLAGS     += -DBM_PCIE_MODE
endif
ifeq ($(PRODUCTFORM),pcie_mips64)
    CFLAGS     += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
endif

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar

INCLUDES = -I$(BMVID_TOP_DIR)/video/provider/cnm/decoder/vpuapi -I$(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi -I$(BMVID_TOP_DIR)/video/driver -I$(BMVID_TOP_DIR)/3rdparty/libbmcv/include
INCLUDES += -I$(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component -I$(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component_decoder -I$(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component_encoder -I$(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/helper
INCLUDES += -I$(BMVID_TOP_DIR)/video/decoder/bm_dec_api/inc
INCLUDES += -I$(BMVID_TOP_DIR)/video/provider/cnm/ffmpeg/include
ifeq ($(ION), 1)
INCLUDES +=  -I$(ION_DIR)/include
DEFINES += -DBM_ION_MEM
endif
ifeq ("$(BUILD_CONFIGURATION)", "NonOS")
#INCLUDES += -I../test/bm1684/drivers/include
endif
DEFINES += -D$(product) -D$(chip_type) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

CFLAGS  += -I. -fPIC -Wall -Wl,--fatal-warning $(INCLUDES) $(DEFINES) $(PLATFORM_FLAGS)

ifeq ($(SLT_TEST), slt)
CFLAGS += -DSLT_TEST
endif

ifeq ($(DEBUG), 0)
CFLAGS += 
else ifeq ($(DEBUG), 1)
CFLAGS += -g -O0
endif
ARFLAGS = cru
SOFLAGS = -shared -o

LDFLAGS  = $(PLATFORM_FLAGS) -L./bin/ -L$(INSTALL_DIR)
LDLIBS   = -lbmvideo

ifeq ($(USE_PTHREAD), yes)
LDLIBS  += -lpthread
endif
LDLIBS  += -lm -ldl



LDFLAGS += -L$(BMVID_TOP_DIR)/3rdparty/libbmcv/lib/$(PRODUCTFORM)
LDLIBS  += -lbmlib

ifeq ($(USE_FFMPEG), yes)
LDLIBS  += -lavformat -lavcodec -lavutil
LDFLAGS += -L$(BMVID_TOP_DIR)/video/provider/cnm/ffmpeg/lib/$(PLATFORM)
ifneq ($(USE_32BIT), yes)
LDLIBS  += -lz
endif #USE_32BIT
endif #USE_FFMPEG

ifeq ($(PRODUCTFORM),pcie_mips64)
LDFLAGS +=  -Wl,-melf64ltsmip
endif
ifeq ($(PRODUCTFORM),pcie_loongarch64)
LDFLAGS +=  -Wl,-melf64loongarch
endif
ifeq ($(PRODUCTFORM),pcie_sw64)
LDLIBS  +=  -lstdc++
endif
DECTEST=$(INSTALL_DIR)/$(SLIBNAME)
BUILDLIST=$(DECTEST)
VPUCHECK_NAME=vpureset
VPUCHECK=$(INSTALL_DIR)/$(VPUCHECK_NAME)
VPUINFO_NAME=vpuinfo
VPUINFO=$(INSTALL_DIR)/$(VPUINFO_NAME)
ifeq ("$(BUILD_CONFIGURATION)", "EmbeddedLinux")
BUILDLIST += $(VPUCHECK) $(VPUINFO)
endif
ifeq ("$(BUILD_CONFIGURATION)", "NativeLinux")
BUILDLIST += $(VPUCHECK) $(VPUINFO)
endif
ifeq ("$(BUILD_CONFIGURATION)", "MipsLinux")
BUILDLIST += $(VPUCHECK) $(VPUINFO)
endif
ifeq ("$(BUILD_CONFIGURATION)", "LoongLinux")
BUILDLIST += $(VPUCHECK) $(VPUINFO)
endif
ifeq ("$(BUILD_CONFIGURATION)", "SunwayLinux")
BUILDLIST += $(VPUCHECK) $(VPUINFO)
endif

ifeq ("$(BUILD_ORG_TEST)", "yes")
ifeq ("$(PRODUCT)", "boda")
#TEST =$(INSTALL_DIR)/boda960_dec_test
#SOURCES_DECTEST = sample/main_coda960_dec_test.c
endif
ifeq ("$(PRODUCT)", "wave")
TEST =$(INSTALL_DIR)/w5_dec_test
SOURCES_DECTEST = $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/main_dec_test.c
ifeq ("$(BUILD_CONFIGURATION)", "NonOS")
SOURCES_DECTEST += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/main_enc_test.c
endif
endif
else
TEST =$(INSTALL_DIR)/bm_test
SOURCES_DECTEST = $(BMVID_TOP_DIR)/example/bm_test.c
endif
ifeq ($(OS),linux)
BUILDLIST += $(TEST)
endif
SOURCES_VPUCHECK = $(BMVID_TOP_DIR)/example/vpu_reset.c
SOURCES_VPUINFO = $(BMVID_TOP_DIR)/example/vpu_info.c
OBJDIR ?= obj
ALLOBJS=*.o
ALLDEPS=*.dep
ALLLIBS=*.a
RM=rm -f
MKDIR=mkdir -p

SOURCES_COMMON =main_helper.c                   vpuhelper.c                 bitstreamfeeder.c           \
                bitstreamreader.c               bsfeeder_fixedsize_impl.c   bsfeeder_framesize_impl.c   \
                bsfeeder_size_plus_es_impl.c    bin_comparator_impl.c       comparator.c                \
                md5_comparator_impl.c           yuv_comparator_impl.c       \
                cfgParser.c                     decoder_listener.c          encoder_listener.c          \
                cnm_video_helper.c              container.c                 \
                datastructure.c                 debug.c                     \
                yuvfeeder.c                     yuvLoaderfeeder.c           yuvCfbcfeeder.c             \
                bw_monitor.c                    pf_monitor.c                md5.c                       \
                cnm_app.c                       cnm_task.c                  component.c                 \
                component_enc_encoder.c         component_enc_feeder.c      component_enc_reader.c      \
                component_dec_decoder.c         component_dec_feeder.c      component_dec_renderer.c    \
                product.c                       vpuapifunc.c                vpuapi.c                    \
                coda9.c                         wave5.c                     \
                $(VDI_C)                        $(VDI_OSAL_C)               $(MM_C)

ifeq ("$(BUILD_CONFIGURATION)", "EmbeddedLinux")
SOURCES_COMMON += $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_interface.c \
                  $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_syscxt.c
endif
ifeq ("$(BUILD_CONFIGURATION)", "NativeLinux")
SOURCES_COMMON += $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_interface.c \
                  $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_syscxt.c
endif
ifeq ("$(BUILD_CONFIGURATION)", "MipsLinux")
SOURCES_COMMON += $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_interface.c \
                  $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_syscxt.c
endif
ifeq ("$(BUILD_CONFIGURATION)", "LoongLinux")
SOURCES_COMMON += $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_interface.c \
                  $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_syscxt.c
endif
ifeq ("$(BUILD_CONFIGURATION)", "SunwayLinux")
SOURCES_COMMON += $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_interface.c \
                  $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src/bm_video_syscxt.c
endif

VPATH  = $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component_encoder:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/component_decoder:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/helper:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/helper/bitstream:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/helper/comparator:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/helper/misc:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/sample_v2/helper/yuv:
VPATH += $(BMVID_TOP_DIR)/video/driver:
VPATH += $(BMVID_TOP_DIR)/video/provider/cnm/decoder/vdi:
VPATH += $(VDI_VPATH):$(BMVID_TOP_DIR)/video/provider/cnm/decoder/vpuapi:$(BMVID_TOP_DIR)/video/provider/cnm/decoder/vpuapi/coda9:$(BMVID_TOP_DIR)/video/provider/cnm/decoder/vpuapi/wave:
VPATH += $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/src:$(BMVID_TOP_DIR)/example

VPATH2=$(patsubst %,-I%,$(subst :, ,$(VPATH)))

OBJECTNAMES_COMMON=$(patsubst %.S,%.o,$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES_COMMON))))
OBJECTPATHS_COMMON=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES_COMMON)))

OBJECTNAMES_DECTEST=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES_DECTEST))) 
OBJECTPATHS_DECTEST=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES_DECTEST))) $(OBJECTPATHS_COMMON)

-include $(BMVID_ROOT)/build/version.mak
ifneq ($(SO_NAME),)
    TARGET_SONAME=$(DECTEST)$(SO_NAME)
    TARGET_SOVERSION=$(DECTEST)$(SO_VERSION)
endif

all: $(BUILDLIST)

test: CREATE_DIR $(TEST)

$(TEST): $(DECTEST) $(SOURCES_DECTEST)
	$(CC) -o $@ $(LDFLAGS) -O2 $(SOURCES_DECTEST) $(LDLIBS) $(CFLAGS)

$(VPUCHECK): $(SOURCES_VPUCHECK) $(DECTEST)
	$(CC) -o $@ $(LDFLAGS) $(SOURCES_VPUCHECK) $(LDLIBS) $(CFLAGS)

$(VPUINFO): $(SOURCES_VPUINFO) $(DECTEST)
	$(CC) -o $@ $(LDFLAGS) $(SOURCES_VPUINFO) $(LDLIBS) $(CFLAGS)

$(DECTEST): CREATE_DIR $(OBJECTPATHS_DECTEST)
ifeq ("$(BUILD_CONFIGURATION)", "NonOS")
	$(AR) $(ARFLAGS) $(DECTEST) $(OBJECTPATHS_DECTEST)
else
	$(CC) $(LDFLAGS) $(CFLAGS) -Wl,-soname,$(SLIBNAME)$(SO_NAME) $(SOFLAGS) $(DECTEST) $(OBJECTPATHS_COMMON)
ifneq ($(TARGET_SOVERSION), )
	mv $@ $(TARGET_SOVERSION)
	ln -sf $(SLIBNAME)$(SO_VERSION) $(TARGET_SONAME)
	ln -sf $(SLIBNAME)$(SO_NAME) $@
endif

endif
	@cp $(BMVID_TOP_DIR)/video/decoder/bm_dec_api/inc/bm_video_interface.h $(INSTALL_DIR)/

install-lib:
	install -d $(DESTDIR)/lib
	install -d $(DESTDIR)/include
	ln -sf "$(SLIBNAME)" "$(DESTDIR)/lib/libvideo_bm.so"
	install -m 755 $(INSTALL_DIR)/bm_video_interface.h $(DESTDIR)/include
ifeq ($(TARGET_SOVERSION), )
	install -m 755 $(DECTEST) "$(DESTDIR)/lib/$(SLIBNAME)"
else
	install -m 755 $(DECTEST) "$(DESTDIR)/lib/$(SLIBNAME)$(SO_VERSION)"
	cp -ap $(TARGET_SONAME) $(DECTEST) $(DESTDIR)/lib/
endif

install-bin:
	install -d $(DESTDIR)
	install -m 755 $(VPUCHECK) "$(DESTDIR)/"
	install -m 755 $(VPUINFO) "$(DESTDIR)/"
	install -m 755 $(TEST) "$(DESTDIR)/"

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(DECTEST) $(TARGET_SONAME) $(TARGET_SOVERSION)
	$(RM) $(TEST)
	$(RM) -rf $(INSTALL_DIR)
	$(RM) $(OBJDIR)/$(ALLOBJS)
	$(RM) $(OBJDIR)/$(ALLDEPS)

CREATE_DIR:
	-mkdir -p $(OBJDIR)
	-mkdir -p $(INSTALL_DIR)

${OBJDIR}/%.o: %.c
	$(CC) $(LDFLAGS) $(CFLAGS)  $(LDLIBS) -c $< -o $@ -MD -MF $(@:.o=.dep)

