########################################################################
#
# The API of BM JPU SDK in Linux
#
# CHIP:        bm1684
# PRODUCTFORM: soc, pcie, pcie_arm64
# SUBTYPE:     palladium, fpga, asic
########################################################################

CHIP ?= bm1684
PRODUCTFORM ?= soc
SUBTYPE ?= asic
INSTALL_DIR ?= ./install
ION_DIR ?= ../../allocator/ion
DEBUG ?= 0
BMVID_ROOT ?= ../..

ifeq ($(PRODUCTFORM),pcie) # pcie mode
    CROSS_CC_PREFIX =
else ifeq ($(PRODUCTFORM),pcie_mips64) # pcie mode
    CROSS_CC_PREFIX = mips-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_sw64) # pcie mode
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_loongarch64) # pcie mode
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_riscv64)
    CROSS_CC_PREFIX =
else # pcie_arm64 and soc mode
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif


CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar

CFLAGS += -fPIC -Wall -Wl,--fatal-warning -std=c11
CFLAGS += -I$(BMVID_ROOT)/jpeg/binary/$(PRODUCTFORM)/include -I$(BMVID_ROOT)/3rdparty/libbmcv/include

ifeq ($(CHIP), bm1682)
    CFLAGS += -DCHIP_BM1682
else ifeq ($(CHIP), bm1684)
    CFLAGS += -DCHIP_BM1684
else ifeq ($(CHIP), bm1880)
    CFLAGS += -DCHIP_BM1880
else
    $(error UNDEFINED CHIP)
endif

ifneq ($(PRODUCTFORM),soc)
    CFLAGS += -DBM_PCIE_MODE
endif
ifeq ($(DEBUG), 0)
    CFLAGS += -O2
else
    CFLAGS += -g
endif

LDFLAGS += -L$(BMVID_ROOT)/jpeg/binary/$(PRODUCTFORM)/lib -L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM)

LIBS0 = -lbmjpulite
ifeq ($(CHIP), bm1684)
    CFLAGS += -DUSE_ION_MEMORY -I$(ION_DIR)/inc
    LIBS0  += -lbmlib -ldl
    LDFLAGS += -L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM)
endif
ifeq ($(PRODUCTFORM),pcie_mips64)
    CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
    LDFLAGS += -Wl,-melf64ltsmip
endif
ifeq ($(PRODUCTFORM), pcie_loongarch64)
    LDFLAGS += -Wl,-melf64loongarch
    LIBS0 += -lstdc++
endif
ifeq ($(PRODUCTFORM), pcie_sw64)
    LIBS0 += -lstdc++
endif
LIBS = -lpthread -lm -lrt -lbmjpuapi $(LIBS0)


TESTDEC_NAME=bmjpegdec
TESTENC_NAME=bmjpegenc
TESTMULTI_NAME=bmjpegmulti

TESTDEC=$(PRODUCTFORM)/$(TESTDEC_NAME)
TESTENC=$(PRODUCTFORM)/$(TESTENC_NAME)
TESTMULTI=$(PRODUCTFORM)/$(TESTMULTI_NAME)

MAKEFILE=Makefile
OBJDIR=$(PRODUCTFORM)/obj
ALLOBJS=*.o
ALLDEPS=*.dep
RM ?= rm -rf
CP ?= cp -f
MKDIR ?= mkdir -p

DEC_SRC = jpeg_dec.c \
	  jpeg_common.c
DEC_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(DEC_SRC)))
DEC_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(DEC_OBJS)))

ENC_SRC = jpeg_enc.c \
	  jpeg_common.c
ENC_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(ENC_SRC)))
ENC_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(ENC_OBJS)))

MULTI_SRC = jpeg_multi.c \
	    jpeg_common.c \
	    bmjpurun.c
MULTI_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(MULTI_SRC)))
MULTI_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(MULTI_OBJS)))


OBJECTPATHS=$(DEC_PATHS) $(ENC_PATHS) $(MULTI_PATHS)

all: $(OBJDIR) $(TESTDEC) $(TESTENC) $(TESTMULTI)

$(TESTDEC): $(DEC_PATHS)
	$(CC) $(CFLAGS) -o $@ $(DEC_PATHS) $(LDFLAGS) $(LIBS)

$(TESTENC): $(DEC_PATHS) $(ENC_PATHS)
	$(CC) $(CFLAGS) -o $@ $(ENC_PATHS) $(LDFLAGS) $(LIBS)

$(TESTMULTI): $(DEC_PATHS) $(MULTI_PATHS)
	$(CC) $(CFLAGS) -o $@ $(MULTI_PATHS) $(LDFLAGS) $(LIBS)

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TESTDEC)
	$(RM) $(TESTENC)
	$(RM) $(TESTMULTI)
	$(RM) $(OBJDIR)/$(ALLDEPS)
	$(RM) $(OBJDIR)/$(ALLOBJS)

install: $(TESTDEC) $(TESTENC) $(TESTMULTI)
	$(MKDIR) $(INSTALL_DIR)
	install $(TESTDEC) $(INSTALL_DIR)/
	install $(TESTENC) $(INSTALL_DIR)/
	install $(TESTMULTI) $(INSTALL_DIR)/

uninstall:
	$(RM) $(INSTALL_DIR)/$(TESTDEC_NAME)
	$(RM) $(INSTALL_DIR)/$(TESTENC_NAME)
	$(RM) $(INSTALL_DIR)/$(TESTMULTI_NAME)

$(OBJDIR)/jpeg_common.o : jpeg_common.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_dec.o : jpeg_dec.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_enc.o : jpeg_enc.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmjpurun.o : bmjpurun.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_multi.o : jpeg_multi.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR):
	-mkdir -p $(OBJDIR)

force_dependency:
	true
