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
INSTALL_DIR ?= ./release
ION_DIR ?= ./release
DEBUG ?= 0
BMVID_ROOT ?= $(BMVID_TOP_DIR)

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
CFLAGS += -I./inc -I./src -I$(INSTALL_DIR)/include  -I$(BMVID_ROOT)/3rdparty/libbmcv/include

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

LDFLAGS += -L./$(PRODUCTFORM) -L$(INSTALL_DIR)/lib  -L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM)

LIBS0 = -lbmjpulite
# TODO
ifeq ($(CHIP), bm1684)
    CFLAGS += -DUSE_ION_MEMORY -I$(ION_DIR)/include
    LIBS0  += -lbmlib -ldl
    LDFLAGS += -L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM)/
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


TARGET_NAME=libbmjpuapi.so
TESTDEC_NAME=bmjpegdec
TESTENC_NAME=bmjpegenc
TESTMULTI_NAME=bmjpegmulti
TESTDEC_SEQ_NAME=bmjpegdec_seq
TESTENC_SEQ_NAME=bmjpegenc_seq

TARGET=$(PRODUCTFORM)/$(TARGET_NAME)
TESTDEC=$(PRODUCTFORM)/$(TESTDEC_NAME)
TESTENC=$(PRODUCTFORM)/$(TESTENC_NAME)
TESTMULTI=$(PRODUCTFORM)/$(TESTMULTI_NAME)
TESTDEC_SEQ=$(PRODUCTFORM)/$(TESTDEC_SEQ_NAME)
TESTENC_SEQ=$(PRODUCTFORM)/$(TESTENC_SEQ_NAME)

-include $(BMVID_ROOT)/build/version.mak
ifneq ($(SO_NAME),)
    TARGET_SONAME=$(TARGET)$(SO_NAME)
    TARGET_SOVERSION=$(TARGET)$(SO_VERSION)
endif

MAKEFILE=Makefile
OBJDIR=$(PRODUCTFORM)/obj
ALLOBJS=*.o
ALLDEPS=*.dep
RM ?= rm -rf
CP ?= cp -f
MKDIR ?= mkdir -p

LIB_SRC = src/bmjpuapi.c \
		  src/bmjpuapi_jpeg.c \
		  src/bmjpuapi_jpulib.c \
		  src/bmjpuapi_parse_jpeg.c

LIB_OBJS=$(patsubst %.c,%.o,$(LIB_SRC))
LIB_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(LIB_OBJS)))

DEC_SRC = example/jpeg_dec.c \
		  example/jpeg_common.c \
		  example/getopt.c

DEC_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(DEC_SRC)))
DEC_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(DEC_OBJS)))

ENC_SRC = example/jpeg_enc.c \
		  example/jpeg_common.c \
		  example/getopt.c

ENC_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(ENC_SRC)))
ENC_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(ENC_OBJS)))

MULTI_SRC = example/jpeg_multi.c \
		  example/jpeg_common.c \
		  example/bmjpurun.c

MULTI_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(MULTI_SRC)))
MULTI_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(MULTI_OBJS)))

DEC_SEQ_SRC = example/jpeg_dec_seq.c \
			  example/jpeg_common.c

DEC_SEQ_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(DEC_SEQ_SRC)))
DEC_SEQ_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(DEC_SEQ_OBJS)))

ENC_SEQ_SRC = example/jpeg_enc_seq.c \
			  example/jpeg_common.c

ENC_SEQ_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(ENC_SEQ_SRC)))
ENC_SEQ_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(ENC_SEQ_OBJS)))

OBJECTPATHS=$(LIB_PATHS) $(DEC_PATHS) $(ENC_PATHS) $(MULTI_PATHS) $(DEC_SEQ_PATHS) $(ENC_SEQ_PATHS)

all: $(OBJDIR) $(TARGET) $(TESTDEC) $(TESTENC) $(TESTMULTI) $(TESTDEC_SEQ) $(TESTENC_SEQ)

$(TESTDEC): $(TARGET) $(DEC_PATHS)
	$(CC) $(CFLAGS) -o $@ $(DEC_PATHS) $(LDFLAGS) $(LIBS)

$(TESTENC): $(TARGET) $(ENC_PATHS)
	$(CC) $(CFLAGS) -o $@ $(ENC_PATHS) $(LDFLAGS) $(LIBS)

$(TESTMULTI): $(TARGET) $(MULTI_PATHS)
	$(CC) $(CFLAGS) -o $@ $(MULTI_PATHS) $(LDFLAGS) $(LIBS)

$(TESTDEC_SEQ): $(TARGET) $(DEC_SEQ_PATHS)
	$(CC) $(CFLAGS) -o $@ $(DEC_SEQ_PATHS) $(LDFLAGS) $(LIBS)

$(TESTENC_SEQ): $(TARGET) $(ENC_SEQ_PATHS)
	$(CC) $(CFLAGS) -o $@ $(ENC_SEQ_PATHS) $(LDFLAGS) $(LIBS)

$(TARGET): $(LIB_PATHS)
	$(CC) $(CFLAGS) -o $@ -shared -Wl,-soname,$(TARGET_NAME)$(SO_NAME) $(LIB_PATHS) -L$(INSTALL_DIR)/lib -L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM) $(LIBS0)
ifneq ($(TARGET_SOVERSION), )
	mv $@ $(TARGET_SOVERSION)
	ln -sf $(TARGET_NAME)$(SO_VERSION) $(TARGET_SONAME)
	ln -sf $(TARGET_NAME)$(SO_NAME) $@
endif

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TESTDEC)
	$(RM) $(TESTENC)
	$(RM) $(TESTMULTI)
	$(RM) $(TESTDEC_SEQ)
	$(RM) $(TESTENC_SEQ)
	$(RM) $(TARGET) $(TARGET_SOVERSION) $(TARGET_SONAME)
	$(RM) $(OBJDIR)/$(ALLDEPS)
	$(RM) $(OBJDIR)/$(ALLOBJS)

install: $(TARGET) $(TESTDEC) $(TESTENC) $(TESTMULTI) $(TESTENC_SEQ)
	install -d $(INSTALL_DIR)/bin
	install -d $(INSTALL_DIR)/lib
	install -d $(INSTALL_DIR)/include
	install $(TESTDEC) $(INSTALL_DIR)/bin
	install $(TESTENC) $(INSTALL_DIR)/bin
	install $(TESTMULTI) $(INSTALL_DIR)/bin
	install $(TESTDEC_SEQ) $(INSTALL_DIR)/bin
	install $(TESTENC_SEQ) $(INSTALL_DIR)/bin
	install $(TARGET)  $(INSTALL_DIR)/lib
	install -m 0644 inc/bm_jpeg_interface.h $(INSTALL_DIR)/include
ifeq ($(TARGET_SOVERSION), )
	install $(TARGET) $(INSTALL_DIR)/lib
else
	install $(TARGET_SOVERSION) $(INSTALL_DIR)/lib
	cp -ap $(TARGET_SONAME) $(TARGET) $(INSTALL_DIR)/lib
endif

uninstall:
	$(RM) $(INSTALL_DIR)/bin/$(TESTDEC_NAME)
	$(RM) $(INSTALL_DIR)/bin/$(TESTENC_NAME)
	$(RM) $(INSTALL_DIR)/bin/$(TESTMULTI_NAME)
	$(RM) $(INSTALL_DIR)/bin/$(TESTDEC_SEQ_NAME)
	$(RM) $(INSTALL_DIR)/bin/$(TESTENC_SEQ_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_VERSION)
	$(RM) $(INSTALL_DIR)/include/bm_jpeg_interface.h

$(OBJDIR)/bmjpuapi.o : src/bmjpuapi.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmjpuapi_jpeg.o : src/bmjpuapi_jpeg.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmjpuapi_jpulib.o : src/bmjpuapi_jpulib.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmjpuapi_parse_jpeg.o : src/bmjpuapi_parse_jpeg.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_common.o : example/jpeg_common.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_dec.o : example/jpeg_dec.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_enc.o : example/jpeg_enc.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmjpurun.o : example/bmjpurun.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_multi.o : example/jpeg_multi.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_dec_seq.o : example/jpeg_dec_seq.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpeg_enc_seq.o : example/jpeg_enc_seq.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/getopt.o : example/getopt.c
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR):
	-mkdir -p $(OBJDIR)

force_dependency:
	true
