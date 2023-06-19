########################################################################
#
# The lite version of BM VPU SDK in Linux
#
# CHIP:        bm1684
# PRODUCTFORM: soc, pcie, pcie_arm64
# SUBTYPE:     palladium, fpga, asic
########################################################################

.PHONY: CREATE_OBJDIR

CHIP ?= bm1684
PRODUCTFORM ?= soc
SUBTYPE ?= asic
INSTALL_DIR ?= ./release
DEBUG ?= 0
PRO ?= 0
BMVID_TOP_DIR ?= ../../../../
BMVID_ROOT ?= $(BMVID_TOP_DIR)

ifeq ($(PRODUCTFORM),pcie) # pcie mode
    CROSS_CC_PREFIX =
else ifeq ($(PRODUCTFORM), pcie_mips64) # pcie mode
    CROSS_CC_PREFIX = mips-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_loongarch64) # pcie mode
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
else ifeq ($(PRODUCTFORM), pcie_sw64) # pcie mode
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
CFLAGS += -I$(BMVID_TOP_DIR)/video/provider/cnm/encoder/inc -I$(BMVID_TOP_DIR)/video/driver
CFLAGS += -D$(BM_CHIP_TYPE) -D$(BM_SUB_TYPE)
ifneq ($(PRODUCTFORM), soc)
    CFLAGS += -DBM_PCIE_MODE
    ifneq ($(PRO), 0)
        CFLAGS += -DPRO_VERSION
    endif
endif
ifeq ($(DEBUG), 0)
    CFLAGS += -O2
else
    CFLAGS += -g
endif

LDFLAGS += -L$(INSTALL_DIR)/lib
LDLIBS  += -lpthread -lm -lrt -ldl

ifeq ($(PRODUCTFORM), soc) #vpu.h need DBM_ION_MEM if using ion
    CFLAGS += -DBM_ION_MEM
endif

ifeq ($(PRODUCTFORM), pcie_mips64)
CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
LDFLAGS += -Wl,-melf64ltsmip
endif

ifeq ($(PRODUCTFORM), pcie_loongarch64)
LDFLAGS += -Wl,-melf64loongarch
endif

TARGET_NAME=libbmvpulite.so
TARGET=$(PRODUCTFORM)/$(TARGET_NAME)

-include $(BMVID_ROOT)/build/version.mak
ifneq ($(SO_NAME),)
    TARGET_SONAME=$(TARGET)$(SO_NAME)
    TARGET_SOVERSION=$(TARGET)$(SO_VERSION)
endif

MAKEFILE=Makefile
OBJDIR=$(PRODUCTFORM)/obj
ALLOBJS=*.o
ALLDEPS=*.dep
RM=rm -rf
CP=cp -f
MKDIR=mkdir -p

SOURCES = src/common.c \
          src/logging.c \
          src/enc.c \
          src/vdi.c \
          src/debug.c

OBJECTNAMES=$(patsubst %.c,%.o,$(SOURCES))
OBJECTPATHS=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES)))

all: CREATE_OBJDIR $(TARGET)

$(TARGET): $(OBJECTPATHS)
	$(CC) $(CFLAGS) -o $@ -shared -Wl,-soname,$(TARGET_NAME)$(SO_NAME) $(OBJECTPATHS) $(LDFLAGS) $(LDLIBS)
ifneq ($(TARGET_SOVERSION), )
	mv $@ $(TARGET_SOVERSION)
	ln -sf $(TARGET_NAME)$(SO_VERSION) $(TARGET_SONAME)
	ln -sf $(TARGET_NAME)$(SO_NAME) $@
endif

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TARGET) $(TARGET_SOVERSION) $(TARGET_SONAME)
	$(RM) $(OBJDIR)/$(ALLDEPS)
	$(RM) $(OBJDIR)/$(ALLOBJS)

install: $(TARGET)
	install -d $(INSTALL_DIR)/lib
	install -d $(INSTALL_DIR)/include
	install -m 0644 inc/bmvpu.h         $(INSTALL_DIR)/include
	install -m 0644 inc/bmvpu_types.h   $(INSTALL_DIR)/include
	install -m 0644 inc/bmvpu_logging.h $(INSTALL_DIR)/include
ifeq ($(TARGET_SOVERSION), )
	install $(TARGET) $(INSTALL_DIR)/lib
else
	install $(TARGET_SOVERSION) $(INSTALL_DIR)/lib
	cp -ap $(TARGET_SONAME) $(TARGET) $(INSTALL_DIR)/lib
endif

uninstall:
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_VERSION)
	$(RM) $(INSTALL_DIR)/include/bmvpu.h
	$(RM) $(INSTALL_DIR)/include/bmvpu_types.h
	$(RM) $(INSTALL_DIR)/include/bmvpu_logging.h


$(OBJDIR)/debug.o : src/debug.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/common.o : src/common.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/io.o : src/io.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/enc.o : src/enc.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/logging.o : src/logging.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/vdi.o : src/vdi.c $(MAKEFILE)
	$(CC) $(CFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)

CREATE_OBJDIR:
	-mkdir -p $(OBJDIR)

force_dependency :
	true
