########################################################################
#
# The API of BM VPU SDK in Linux
#
# CHIP:        bm1684
# PRODUCTFORM: soc, pcie, pcie_arm64
# SUBTYPE:     palladium, fpga, asic
########################################################################

CHIP ?= bm1684
PRODUCTFORM ?= soc
SUBTYPE ?= asic
INSTALL_DIR ?= ./release
DEBUG ?= 0
BMVID_TOP_DIR ?= ../../../
BMVID_ROOT ?= $(BMVID_TOP_DIR)

ifeq ($(PRODUCTFORM),pcie) # pcie mode
    CROSS_CC_PREFIX =
else ifeq ($(PRODUCTFORM),pcie_mips64) # pcie mode
    CROSS_CC_PREFIX = mips-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_loongarch64) # pcie mode
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_sw64) # pcie mode
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
else # pcie_arm64 and soc mode
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif

ifeq ($(CHIP), bm1684)
    BM_CHIP_TYPE = CHIP_BM1684
else
    $(error Only BM1684 is supported now)
endif

ifeq ($(SUBTYPE), palladium)
    BM_SUB_TYPE = PALLADIUM_SIM
else ifeq ($(SUBTYPE), fpga)
    BM_SUB_TYPE = FPGA_SIM
else ifeq ($(SUBTYPE), asic)
    BM_SUB_TYPE = BM_ASIC
else
    $(error Only palladium/fpga/asic are valid sub-types)
endif

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar

CFLAGS += -fPIC -Wall -Wl,--fatal-warning
CFLAGS += -I$(BMVID_TOP_DIR)/video/encoder/bm_enc_api/inc -I$(INSTALL_DIR)/include -I$(BMVID_TOP_DIR)/provider/cnm/encoder/inc -I$(BMVID_ROOT)/3rdparty/libbmcv/include
CFLAGS += -D$(BM_CHIP_TYPE) -D$(BM_SUB_TYPE)
ifneq ($(PRODUCTFORM), soc)
    CFLAGS += -DBM_PCIE_MODE
endif
ifeq ($(DEBUG), 0)
    CFLAGS += -O2
else
    CFLAGS += -g
endif

LDLIBS   = -lpthread -lm -lrt -ldl
LDLIBS  += -lbmvpulite -lbmvpuapi
LIBS0    = -lbmvpulite

LDFLAGS += -L./$(PRODUCTFORM) -L$(INSTALL_DIR)/lib -L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM)

# for using bmlib to manage physical memory
LIBS0  += -lbmlib
LDLIBS += -lbmlib

ifeq ($(PRODUCTFORM),pcie_mips64)
    CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
    LDFLAGS += -Wl,-melf64ltsmip
endif

ifeq ($(PRODUCTFORM), pcie_loongarch64)
    LDFLAGS += -Wl,-melf64loongarch
    LDLIBS += -lstdc++
endif

ifeq ($(PRODUCTFORM), pcie_sw64)
    LDLIBS += -lstdc++
endif

TARGET_NAME=libbmvpuapi.so
TESTENC_NAME=bmvpuenc

TARGET=$(PRODUCTFORM)/$(TARGET_NAME)
TESTENC=$(PRODUCTFORM)/$(TESTENC_NAME)

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

LIB_SRC = src/encoder.c \
          src/log.c \
          src/queue.c \
          src/misc.c

LIB_OBJS=$(patsubst %.c,%.o,$(LIB_SRC))
LIB_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(LIB_OBJS)))

ENC_SRC = $(BMVID_TOP_DIR)/example/bmvpuenc.c

ENC_OBJS=$(patsubst %.c,%.o,$(ENC_SRC))
ENC_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(ENC_OBJS)))

OBJECTPATHS=$(LIB_PATHS) $(ENC_PATHS)

all: $(OBJDIR) $(TARGET) $(TESTENC)

$(TESTENC): $(TARGET) $(ENC_PATHS)
	$(CC) $(CFLAGS) -o $@ $(ENC_PATHS) $(LDFLAGS) $(LDLIBS)

$(TARGET): $(LIB_PATHS)
	$(CC) $(CFLAGS) -o $@ -shared -Wl,-soname,$(TARGET_NAME)$(SO_NAME) $(LIB_PATHS) $(LDFLAGS) $(LIBS0)
ifneq ($(TARGET_SOVERSION), )
	mv $@ $(TARGET_SOVERSION)
	ln -sf $(TARGET_NAME)$(SO_VERSION) $(TARGET_SONAME)
	ln -sf $(TARGET_NAME)$(SO_NAME) $@
endif

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TARGET) $(TARGET_SOVERSION) $(TARGET_SONAME)
	$(RM) $(TESTENC)
	$(RM) $(OBJDIR)/$(ALLDEPS)
	$(RM) $(OBJDIR)/$(ALLOBJS)

install: $(TARGET)
	install -d $(INSTALL_DIR)/bin
	install -d $(INSTALL_DIR)/lib
	install -d $(INSTALL_DIR)/include
	install $(TESTENC) $(INSTALL_DIR)/bin
	install -m 0644 inc/bmqueue.h $(INSTALL_DIR)/include
	install -m 0644 inc/bmvpuapi.h $(INSTALL_DIR)/include
	install -m 0644 inc/bmvpuapi_common.h $(INSTALL_DIR)/include
	install -m 0644 inc/bmvpuapi_enc.h $(INSTALL_DIR)/include
ifeq ($(TARGET_SOVERSION), )
	install $(TARGET) $(INSTALL_DIR)/lib
else
	install $(TARGET_SOVERSION) $(INSTALL_DIR)/lib
	cp -ap $(TARGET_SONAME) $(TARGET) $(INSTALL_DIR)/lib
endif

uninstall:
	$(RM) $(INSTALL_DIR)/bin/$(TESTENC_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_VERSION)
	$(RM) $(INSTALL_DIR)/include/bmqueue.h
	$(RM) $(INSTALL_DIR)/include/bmvpuapi.h
	$(RM) $(INSTALL_DIR)/include/bmvpuapi_common.h
	$(RM) $(INSTALL_DIR)/include/bmvpuapi_enc.h

$(OBJDIR)/encoder.o : src/encoder.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/log.o : src/log.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/queue.o : src/queue.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/misc.o : src/misc.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmvpuenc.o : $(BMVID_TOP_DIR)/example/bmvpuenc.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR):
	-mkdir -p $(OBJDIR)

force_dependency:
	true
