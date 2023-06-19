#######################
# Makefile
#######################
# README
# target
TARGET := vpp_matrix
SOURCES = vppmatrix.c
OBJ = vppmatrix.o

# compile and lib parameter

CROSS_CC_PREFIX ?= $(CROSS_COMPILE)
BMCV_PATH ?= ${OPENCV_PATH}/3rdparty/libbmcv

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar
LDFLAGS := -L$(VPP_LIB_DIR)/release -L$(VPP_LIB_DIR)/lib
LIBS    := -lvpp
INCLUDE := -I$(VPP_LIB_DIR)/include/
INCLUDE += -I$(VPP_LIB_DIR)/include/$(CHIP)

DEFINES :=

ifeq ($(patsubst pcie%,pcie,$(productform)),pcie)
LDFLAGS += -L${BMCV_PATH}/lib/${productform}/
LIBS    += -lbmlib -lpthread
INCLUDE += -I${BMCV_PATH}/include/
DEFINES += -DPCIE_MODE
endif

#DEFINES :=
CFLAGS  :=
CXXFLAGS:=

ifeq ($(CHIP), bm1682)
DEFINES += -DVPP_BM1682
endif
ifeq ($(CHIP), bm1684)
DEFINES += -DVPP_BM1684
endif

# link
$(TARGET):
	$(CC) $(DEFINES) -o $@  $(SOURCES) $(LDFLAGS) $(LIBS) $(INCLUDE)

#compile
$(OBJ):$(SOURCES)
	$(CC) $(INCLUDE) -c $^

clean:
	rm -fr *.o
	rm -fr $(TARGET)
	rm -fr $(DESTDIR)/$(TARGET)

install:
	cp $(TARGET) $(DESTDIR)
