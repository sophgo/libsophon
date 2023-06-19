# ----------------------------------------------------------------------
#
# Project: C&M Video decoder sample
#
# ----------------------------------------------------------------------
.PHONY: TARGETLIBTHEORA CREATE_DIR
DESTDIR ?= /usr/local/lib
CHIP ?= bm1682
USE_FFMPEG  = no
USE_PTHREAD = yes
BUILD_CONFIGURATION ?= EmbeddedLinux
INSTALL_DIR ?= ./release
DEBUG ?= 0
BMVID_ROOT ?= $(BMVID_TOP_DIR)

ifeq ("$(BUILD_CONFIGURATION)", "x86Linux") # pcie mode
    CROSS_CC_PREFIX = 
else ifeq ("$(BUILD_CONFIGURATION)", "mipsLinux") # mips_pcie mode
    CROSS_CC_PREFIX = mips-linux-gnu-
else ifeq ("$(BUILD_CONFIGURATION)", "sunwayLinux") # sw64_pcie mode
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
else ifeq ("$(BUILD_CONFIGURATION)", "loongLinux") # loong_pcie mode
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
else # pcie_arm64 and soc mode
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif

ifeq ("$(BUILD_CONFIGURATION)", "NonOS")
    CROSS_CC_PREFIX = aarch64-elf-
    VDI_C           = jdi/linux/jdi.c
    MM_C            = jdi.c/mm.c
    USE_FFMPEG      = no
    USE_PTHREAD     = no
    PLATFORM        = none
    DEFINES         = -DLIB_C_STUB
    VDI_VPATH       = jdi/nonos
    PLATFORM_FLAGS  = -nostdlib -ftabstop=4
endif

#CROSS_CC_PREFIX = $(CROSS_COMPILE)
ifeq ("$(BUILD_CONFIGURATION)", "EmbeddedLinux")
    PLATFORM        = armlinux
else
 ifeq ($(SUBTYPE), asic)
  DEFINES += -DPCIE_MODE
 endif
endif

ifeq ("$(BUILD_CONFIGURATION)", "x86Linux")
    PLATFORM        = x86linux
ifeq ($(SUBTYPE), cmodel)
    DEFINES += -DUSING_CMODEL
endif
endif

ifeq ($(CHIP), bm1682)
header = vpplib.h
chip_type = CHIP_BM1682
DEFINES += -DVPP_BM1682
endif

ifeq ($(CHIP), bm1684)
header = bmvpp.h
chip_type = CHIP_BM1684
DEFINES += -DVPP_BM1684
SOURCES += 1684vppcmodel.c
endif
DEFINES += -DION_CACHE

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar

INCLUDES = -I. -I ./include/ -I ./include/$(CHIP) -I ../../3rdparty/libbmcv/include/
INCLUDES += -I./src/$(CHIP)
DEFINES += -D$(chip_type) -D_GNU_SOURCE
CFLAGS  += -I. -fPIC -Wno-implicit-function-declaration -Wno-unused-result -Wno-format -Wl,--fatal-warning $(INCLUDES) $(DEFINES) -std=c99
ARFLAGS = cr

ifeq ($(DEBUG), 0)
CFLAGS +=
else
CFLAGS += -O0 -g -DBMDEBUG_V
endif

ifeq ("$(BUILD_CONFIGURATION)", "mipsLinux")
CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
endif

ifeq ("$(BUILD_CONFIGURATION)", "loongLinux")
CFLAGS += -Wl,-melf64loongarch
endif

LDLIBS   += -lpthread -lm -lrt

TARGET =$(INSTALL_DIR)/libvpp.a
TARGET2 =$(INSTALL_DIR)/libvpp.so
TARGET2_NAME =libvpp.so
TEST =$(INSTALL_DIR)/bm_vpp_test
MAKEFILE=makefile_lib.mak
OBJDIR=obj
ALLOBJS=*.o
ALLDEPS=*.dep
RM=rm -rf
MKDIR=mkdir -p

-include $(BMVID_ROOT)/build/version.mak
ifneq ($(SO_NAME),)
    TARGET2_SONAME=$(TARGET2)$(SO_NAME)
    TARGET2_SOVERSION=$(TARGET2)$(SO_VERSION)
endif

SOURCES += src/vppion.c                               \
          src/$(CHIP)/vpplib.c

SOURCES += test/vpptest.c

OBJECTNAMES=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES)))
OBJECTPATHS=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES)))

all: CREATE_OBJDIR $(TARGET) $(TEST)

test: $(TARGET) $(TEST)

$(TEST): $(OBJECTPATHS)
	#$(CC) -o $@ $(LDFLAGS) -Wl,-gc-section -Wl,--start-group $(TARGET) $(LDFLAGS) $(LDLIBS) -Wl,--end-group

$(TARGET): $(OBJECTPATHS)
	$(MKDIR) $(INSTALL_DIR)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJECTPATHS)
	cp -f include/vppion.h $(INSTALL_DIR)
	cp -f include/$(CHIP)/vpplib.h $(INSTALL_DIR)
	cp -f include/$(CHIP)/$(header) $(INSTALL_DIR)
	$(CC) $(OBJECTPATHS) $(CFLAGS) -Wall -fPIC -shared -Wl,-soname,$(TARGET2_NAME)$(SO_NAME) -o $(TARGET2)
ifneq ($(TARGET2_SOVERSION), )
	mv $(TARGET2) $(TARGET2_SOVERSION)
	ln -sf $(TARGET2_NAME)$(SO_VERSION) $(TARGET2_SONAME)
	ln -sf $(TARGET2_NAME)$(SO_NAME) $(TARGET2)
endif


-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TARGET) $(TARGET2) $(TARGET2_SOVERSION) $(TARGET2_SONAME)
	$(RM) -rf $(OBJDIR)
	$(RM) -rf $(INSTALL_DIR)

install:
	install -d $(DESTDIR)/bin
	install -d $(DESTDIR)/lib
	install -d $(DESTDIR)/include
	#install -m 755 $(TEST) $(DESTDIR)/bin
	install -m 644 $(TARGET) $(DESTDIR)/lib
	install -m 644 include/vppion.h $(DESTDIR)/include
	install -m 644 include/$(CHIP)/vpplib.h $(DESTDIR)/include
	install -m 644 include/$(CHIP)/$(header) $(DESTDIR)/include
ifeq ($(TARGET2_SOVERSION), )
	install -m 644 $(TARGET2) $(DESTDIR)/lib
else
	install -m 644 $(TARGET2_SOVERSION) $(DESTDIR)/lib
	cp -ap $(TARGET2_SONAME) $(TARGET2) $(DESTDIR)/lib
endif


$(OBJDIR)/vppion.o : src/vppion.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/1684vppcmodel.o : src/bm1684/1684vppcmodel.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/vpplib.o : src/$(CHIP)/vpplib.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/vpptest.o : test/vpptest.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)
CREATE_OBJDIR:
	-mkdir -p $(OBJDIR)

force_dependency :
	true
