# ----------------------------------------------------------------------
#
# Project: C&M Video decoder sample
#
# ----------------------------------------------------------------------
.PHONY: CREATE_DIR

PRODUCT := WAVE511
ifneq ($(PRODUCTFORM), pcie)
    BUILD_CONFIGURATION = EmbeddedLinux
else
    BUILD_CONFIGURATION = NativeLinux
endif
#BUILD_CONFIGURATION = NonOS
$(shell cp sample_v2/component_list_decoder.h sample_v2/component/component_list.h)

USE_FFMPEG  = no
USE_PTHREAD = yes
USE_RTL_SIM = no
LINT_HOME   = etc/lint

UNAME = $(shell uname -a)
ifneq (,$(findstring i386, $(UNAME)))
    USE_32BIT = yes
endif

ifeq ($(RTL_SIM), 1)
USE_RTL_SIM = yes
endif

REFC    := 0

ifeq ($(USE_32BIT), yes)
PLATFORM    = nativelinux
else
PLATFORM    = nativelinux_64bit
endif

CROSS_CC_PREFIX = 
VDI_C           = decoder/vdi/linux/vdi.c
VDI_OSAL_C      = decoder/vdi/linux/vdi_osal.c
MM_C            = 
PLATFORM_FLAGS  = 

VDI_VPATH       = decoder/vdi/linux
ifeq ("$(BUILD_CONFIGURATION)", "NonOS")
    CROSS_CC_PREFIX = aarch64-elf-
    VDI_C           = decoder/vdi/nonos/vdi.c
    VDI_OSAL_C      = decoder/vdi/nonos/vdi_osal.c
    MM_C            = decoder/vdi/mm.c
    USE_FFMPEG      = no
    USE_PTHREAD     = no
    PLATFORM        = none
    DEFINES         = -DLIB_C_STUB
    PLATFORM_FLAGS  = 
    VDI_VPATH       = decoder/vdi/nonos
    NONOS_RULE      = options_nonos.lnt
endif
ifeq ("$(BUILD_CONFIGURATION)", "EmbeddedLinux")
    CROSS_CC_PREFIX = aarch64-linux-gnu-
    PLATFORM        = armlinux
else
    CROSS_CC_PREFIX =
    PLATFORM        = x86linux
endif

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
LINKER=$(CC)
AR  = $(CROSS_CC_PREFIX)ar

INCLUDES = -I./decoder/vpuapi -I./ffmpeg/include -I./sample_v2/helper -I./sample_v2/helper/misc -I./sample_v2/component -I./decoder/vdi
INCLUDES += -I./sample_v2/component_decoder
ifeq ($(USE_RTL_SIM), yes)
DEFINES += -DCNM_SIM_PLATFORM -DCNM_SIM_DPI_INTERFACE -DSUPPORT_DECODER -DCHIP_BM1684
DEFINES += -D$(PRODUCT) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE 
else
DEFINES += -D$(PRODUCT) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DCHIP_BM1684 
endif	# USE_SIM_PLATFORM

LDFLAGS  = $(PLATFORM_FLAGS) 

ifneq ($(PRODUCTFORM), soc)
    DEFINES += -DBM_PCIE_MODE
else
    INCLUDES +=  -I../../../install/soc_bm1684_asic/decode/include
    DEFINES += -DBM_ION_MEM
    LDFLAGS +=  -L../../../install/soc_bm1684_asic/decode/lib
    LDLIBS  += -lbmion
endif

CFLAGS  += -g -I. -Wno-implicit-function-declaration -Wno-format -Wl,--fatal-warning $(INCLUDES) $(DEFINES) $(PLATFORM_FLAGS) 
ifeq ($(USE_RTL_SIM), yes)
ifeq ($(IUS), 1)
CFLAGS  += -fPIC # ncverilog is 64bit version
endif
endif
ARFLAGS += cru

ifeq ($(USE_FFMPEG), yes)
LDLIBS  += -lavformat -lavcodec -lavutil
LDFLAGS += -L./ffmpeg/lib/$(PLATFORM)
ifneq ($(USE_32BIT), yes)
LDLIBS  += -lz
endif #USE_32BIT
endif #USE_FFMPEG

ifeq ($(USE_PTHREAD), yes)
LDLIBS  += -lpthread -ldl
endif
LDLIBS  += -lm

BUILDLIST=DECTEST
MAKEFILE=Wave5xxDecV2.mak
DECTEST=w5_dec_test

OBJDIR=obj
ALLOBJS=*.o
ALLDEPS=*.dep
ALLLIBS=*.a
RM=rm -f
MKDIR=mkdir -p

SOURCES_COMMON =main_helper.c                   vpuhelper.c                 bitstreamfeeder.c           \
                bitstreamreader.c               bsfeeder_fixedsize_impl.c   bsfeeder_framesize_impl.c   \
                bsfeeder_size_plus_es_impl.c    bin_comparator_impl.c       comparator.c                \
                md5_comparator_impl.c           yuv_comparator_impl.c       \
                cfgParser.c                     decoder_listener.c          \
                cnm_video_helper.c              container.c                 \
                datastructure.c                 debug.c                     \
                bw_monitor.c                    pf_monitor.c                \
                cnm_app.c                       cnm_task.c                  component.c                 \
                component_dec_decoder.c         component_dec_feeder.c      component_dec_renderer.c    \
                product.c                       vpuapifunc.c                vpuapi.c                    \
                coda9.c                         wave5.c                     \
                $(VDI_C)                        $(VDI_OSAL_C)               $(MM_C) 



VPATH  = sample_v2:
VPATH += sample_v2/component:
VPATH += sample_v2/component_encoder:
VPATH += sample_v2/component_decoder:
VPATH += sample_v2/helper:
VPATH += sample_v2/helper/bitstream:
VPATH += sample_v2/helper/comparator:
VPATH += sample_v2/helper/misc:
VPATH += sample_v2/helper/yuv:
VPATH += decoder/vdi:
VPATH += $(VDI_VPATH):decoder/vpuapi:decoder/vpuapi/coda9:decoder/vpuapi/wave

VPATH2=$(patsubst %,-I%,$(subst :, ,$(VPATH)))

OBJECTNAMES_COMMON=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES_COMMON)))
OBJECTPATHS_COMMON=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES_COMMON)))

SOURCES_DECTEST = sample_v2/main_dec_test.c
ifeq ($(USE_RTL_SIM), yes)
	#SOURCES_DECTEST += sample/main_sim.c
endif

OBJECTNAMES_DECTEST=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES_DECTEST))) 
OBJECTPATHS_DECTEST=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES_DECTEST))) $(OBJECTPATHS_COMMON)

all: $(BUILDLIST) 

ifeq ($(USE_RTL_SIM), yes)
DECTEST: CREATE_DIR $(OBJECTPATHS_DECTEST) 
else
DECTEST: CREATE_DIR $(OBJECTPATHS_DECTEST) 
	$(LINKER) -o $(DECTEST) $(LDFLAGS) -Wl,-gc-section -Wl,--start-group $(OBJECTPATHS_DECTEST) $(LDLIBS) -Wl,--end-group
endif

-include $(OBJECTPATHS:.o=.dep)

clean: 
	$(RM) $(DECTEST)
	$(RM) $(OBJDIR)/$(ALLOBJS)
	$(RM) $(OBJDIR)/$(ALLDEPS)

CREATE_DIR:
	-mkdir -p $(OBJDIR)

obj/%.o: %.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -Werror -c $< -o $@ -MD -MF $(@:.o=.dep)


lint: 
	"$(LINT_HOME)/flint" -i"$(LINT_HOME)" $(DEFINES) $(INCLUDES) $(VPATH2) linux_std.lnt $(HAPS_RULE) $(NONOS_RULE)  $(SOURCES_COMMON)

