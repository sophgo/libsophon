#######################
# Makefile
#######################
# target
TARGET := i420tonv12
SOURCES = i420tonv12.c
OBJ = i420tonv12.o

# compile and lib parameter

ifeq ("$(productform)", "pcie")
    CROSS_CC_PREFIX =
endif

ifeq ("$(productform)", "soc")
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif
ifeq ("$(productform)", "pcie_mips64")
    CROSS_CC_PREFIX = mips-linux-gnu-
endif
ifeq ("$(productform)", "pcie_sw64")
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
endif

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar
LDFLAGS := -L$(VPP_LIB_DIR)/release -L$(VPP_LIB_DIR)/lib
LIBS    := -lvpp -static
DEFINES :=
INCLUDE := -I$(VPP_LIB_DIR)/include
CFLAGS  :=
CXXFLAGS:=
INCLUDE += -I$(VPP_LIB_DIR)/include/$(CHIP)
ifeq ($(CHIP), bm1682)
DEFINES += -DVPP_BM1682
endif
ifeq ($(CHIP), bm1684)
DEFINES += -DVPP_BM1684
endif

ifeq ($(PRODUCTFORM), pcie_mips64)
CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
LDFLAGS += -Wl,-melf64ltsmip
endif

# link
$(TARGET):
	$(CC) $(DEFINES) -o $@  $(SOURCES) $(CFLAGS) $(LDFLAGS) $(INCLUDE) -lvpp

#compile
$(OBJ):$(SOURCES)
	$(CC) $(INCLUDE) -c $^

clean:
	rm -fr *.o
	rm -fr $(TARGET)
	rm -fr $(DESTDIR)/$(TARGET)

install:
	cp $(TARGET) $(DESTDIR)
