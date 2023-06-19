#######################
# Makefile
#######################
# target
TARGET := vpp_border_nodevfd
SOURCES = vppborder_nodevfd.c
OBJ = vppborder_nodevfd.o

# compile and lib parameter

ifeq ("$(productform)", "pcie")
    CROSS_CC_PREFIX =
endif

ifeq ("$(productform)", "soc")
    CROSS_CC_PREFIX = aarch64-linux-gnu-
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

# link
$(TARGET):
	$(CC) $(DEFINES) -o $@  $(SOURCES) $(LDFLAGS) $(INCLUDE) -lvpp

#compile
$(OBJ):$(SOURCES)
	$(CC) $(INCLUDE) -c $^

clean:
	rm -fr *.o
	rm -fr $(TARGET)
	rm -fr $(DESTDIR)/$(TARGET)

install:
	cp $(TARGET) $(DESTDIR)
