#######################
# Makefile
#######################
# README
# target
TARGET := vpp_cmodel
SOURCES = vppcmodel.c
OBJ = vppcmodel.o

# compile and lib parameter

CROSS_CC_PREFIX = $(CROSS_COMPILE)

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar
LDFLAGS := -L$(VPP_LIB_DIR)/release -L$(VPP_LIB_DIR)/lib -L$(BMVID_TOP_DIR)/3rdparty/libbmcv/lib/soc/
LIBS    := -lvpp_cmodel -lvpp
INCLUDE := -I$(VPP_LIB_DIR)/include/
INCLUDE += -I$(VPP_LIB_DIR)/include/$(CHIP)
INCLUDE += -I$(VPP_LIB_DIR)/../..//3rdparty/libbmcv/include/
DEFINES :=

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
