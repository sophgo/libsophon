
DEBUG        ?= 0
CHIP         ?= bm1684
SUBTYPE      ?= asic
PRODUCTFORM  ?= soc

INSTALL_DIR    ?= release

ifeq ($(PRODUCTFORM),pcie) # pcie mode
    CROSS_CC_PREFIX =
endif

ifeq ($(PRODUCTFORM),soc) # soc mode
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif

ifeq ($(PRODUCTFORM),pcie_arm64) # pcie_arm64
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif

ifeq ($(PRODUCTFORM),pcie_loongarch64)
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
endif

ifeq ($(PRODUCTFORM),pcie_loongarch64_anolis)
    CROSS_CC_PREFIX = loongarch64-unknown-linux-gnu-
endif

ifeq ($(PRODUCTFORM),pcie_mips64)
    CROSS_CC_PREFIX = mips-linux-gnu-
    CPPFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
endif

ifeq ($(PRODUCTFORM),pcie_sw64)
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
    CPPFLAGS += --sysroot=$(CROSS_COMPILE_PATH)
endif

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++

CPPFLAGS += -fPIC -Wall -Wl,--fatal-warning -g -std=c++11
ifeq ($(DEBUG), 0)
    CPPFLAGS += -O2
else
    CPPFLAGS += -g
endif

ifneq ($(PRODUCTFORM),soc)
    CPPFLAGS += -DBM_PCIE_MODE
endif

# NATIVE API SDK
NATIVE_SDK_HEADERS:=-I$(SDK_DIR)/include/ -I$(BMCV_SDK_DIR)/include/ -I$(BMLIB_DIR)/include/
NATIVE_SDK_LDFLAGS:=-L$(SDK_DIR)/lib/ -L$(BMCV_SDK_DIR)/lib/ -L$(BMLIB_DIR)/lib/$(PRODUCTFORM) \
                    -Wl,-rpath,$(SDK_DIR)/lib/ -Wl,-rpath,$(BMCV_SDK_DIR)/lib/
NATIVE_SDK_LDLIBS :=-lbmion -lbmjpulite -lbmjpuapi -lbmvpulite -lbmvpuapi -lbmvideo -lbmcv -lbmlib

CPPFLAGS += $(NATIVE_SDK_HEADERS)
LDFLAGS  := $(NATIVE_SDK_LDFLAGS)
LDLIBS   := $(NATIVE_SDK_LDLIBS) -lstdc++ -lpthread

ifeq ($(PRODUCTFORM),pcie_loongarch64)
LDLIBS   += -lm  -lresolv  -lrt  -ldl
endif

TARGET=media_version
MAKEFILE=Makefile
ALLOBJS=*.o
ALLDEPS=*.dep
RM=rm -rf
CP=cp -f

SOURCES := media_version.c

OBJECTPATHS:=$(patsubst %.cpp,%.o,$(SOURCES))

.phony: all clean

all: $(TARGET)

$(TARGET): $(OBJECTPATHS)
	$(CC) -o $@ $(OBJECTPATHS) $(LDFLAGS) $(LDLIBS) $(CPPFLAGS)

install: $(TARGET)
	install -d $(INSTALL_DIR)/bin
	install $(TARGET) $(INSTALL_DIR)/bin

uninstall:
	$(RM) $(INSTALL_DIR)/bin/$(TARGET)

clean:
	$(RM) $(TARGET)
	$(RM) $(ALLDEPS)
	$(RM) $(ALLOBJS)

media_version.o : media_version.c $(MAKEFILE)
	$(CXX) $(CPPFLAGS) -c $< -o $@ -MD -MF $(@:.o=.dep)
