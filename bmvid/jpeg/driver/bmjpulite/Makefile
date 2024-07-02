CHIP ?= bm1682
INSTALL_DIR ?= ./release
DEBUG ?= 0
BMVID_ROOT ?= $(BMVID_TOP_DIR)

ifneq ($(PRODUCTFORM),pcie)
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif
ifeq ($(PRODUCTFORM),pcie_mips64)
    CROSS_CC_PREFIX = mips-linux-gnu-
endif
ifeq ($(PRODUCTFORM),pcie_loongarch64) # pcie mode
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
endif
ifeq ($(PRODUCTFORM),pcie_sw64)
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
endif
ifeq ($(PRODUCTFORM),pcie_riscv64) # pcie mode
    CROSS_CC_PREFIX =
endif



ifeq ($(CHIP), bm1682)
    chip_type = CHIP_BM1682
else ifeq ($(CHIP), bm1684)
    chip_type = CHIP_BM1684
else ifeq ($(CHIP), bm1880)
    chip_type = CHIP_BM1880
endif

CC  = $(CROSS_CC_PREFIX)gcc
CXX = $(CROSS_CC_PREFIX)g++
AR  = $(CROSS_CC_PREFIX)ar

INCLUDES = -Iinc -Ilib -I../include -I../jdi -I../jpuapi
DEFINES += -D$(chip_type) -DLIBBMJPULITE

ifeq ($(PRODUCTFORM),pcie)
DEFINES += -DBM_PCIE_MODE
endif
ifeq ($(PRODUCTFORM),pcie_arm64)
DEFINES += -DBM_PCIE_MODE
endif

CFLAGS  += -fPIC -Wall -Wl,--fatal-warning $(INCLUDES) $(DEFINES) -std=c99

ifeq ($(DEBUG), 1)
CFLAGS += -O0 -g
endif
ifeq ($(PRODUCTFORM),pcie_mips64)

DEFINES += -DBM_PCIE_MODE

CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
LDFLAGS += -Wl,-melf64ltsmip
endif

ifeq ($(PRODUCTFORM), pcie_loongarch64)
DEFINES += -DBM_PCIE_MODE
LDFLAGS += -Wl,-melf64loongarch
LDLIBS += -lstdc++
endif

ifeq ($(PRODUCTFORM),pcie_sw64)
DEFINES += -DBM_PCIE_MODE
endif

ifeq ($(PRODUCTFORM),pcie_riscv64)
DEFINES += -DBM_PCIE_MODE
endif

LDLIBS   = -lpthread -lm -lrt

TARGET_NAME=libbmjpulite.so
TARGET=$(PRODUCTFORM)/$(TARGET_NAME)

STATIC_TARGET_NAME=libbmjpulite.a
STATIC_TARGET=$(PRODUCTFORM)/$(STATIC_TARGET_NAME)

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

SOURCES = src/jpu_lib.c \
		  src/jpu_io.c \
		  src/jpu_lib_dec.c \
		  src/jpu_lib_enc.c \
		  src/jpu_logging.c \
		  ../jdi/linux/jdi.c \
		  ../jdi/mm.c \
		  ../jpuapi/jpuapi.c \
		  ../jpuapi/jpuapifunc.c \
          src/jpulog.c


OBJECTNAMES=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES)))
OBJECTPATHS=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES)))

all: $(OBJDIR) $(TARGET) $(STATIC_TARGET)

$(TARGET): $(OBJECTPATHS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -shared -Wl,-soname,$(TARGET_NAME)$(SO_NAME) $(OBJECTPATHS)
ifneq ($(TARGET_SOVERSION), )
	mv $@ $(TARGET_SOVERSION)
	ln -sf $(TARGET_NAME)$(SO_VERSION) $(TARGET_SONAME)
	ln -sf $(TARGET_NAME)$(SO_NAME) $@
endif

$(STATIC_TARGET): $(OBJECTPATHS)
	$(AR) rcs $@ $(OBJECTPATHS)

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TARGET) $(TARGET_SONAME) $(TARGET_SOVERSION)
	$(RM) $(STATIC_TARGET)
	$(RM) $(OBJDIR)/$(ALLDEPS)
	$(RM) $(OBJDIR)/$(ALLOBJS)

install: $(TARGET) $(STATIC_TARGET)
	install -d $(INSTALL_DIR)/lib
	install -d $(INSTALL_DIR)/include
	install -m 0644 inc/jpu_lib.h     $(INSTALL_DIR)/include
	install -m 0644 inc/jpu_io.h      $(INSTALL_DIR)/include
	install -m 0644 inc/jpu_logging.h $(INSTALL_DIR)/include
ifeq ($(TARGET_SOVERSION), )
	install $(TARGET) $(INSTALL_DIR)/lib
else
	install $(TARGET_SOVERSION) $(INSTALL_DIR)/lib
	cp -ap $(TARGET_SONAME) $(TARGET) $(INSTALL_DIR)/lib
endif
	install $(STATIC_TARGET) $(INSTALL_DIR)/lib


uninstall:
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_NAME)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_NAME)$(SO_VERSION)
	$(RM) $(INSTALL_DIR)/lib/$(STATIC_TARGET_NAME)
	$(RM) $(INSTALL_DIR)/include/jpu_lib.h
	$(RM) $(INSTALL_DIR)/include/jpu_io.h
	$(RM) $(INSTALL_DIR)/include/jpu_logging.h

$(OBJDIR)/jpu_lib.o : src/jpu_lib.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpu_io.o : src/jpu_io.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpu_lib_dec.o : src/jpu_lib_dec.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpu_lib_enc.o : src/jpu_lib_enc.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpu_logging.o : src/jpu_logging.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jdi.o : ../jdi/linux/jdi.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/mm.o : ../jdi/mm.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpuapi.o : ../jpuapi/jpuapi.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpuapifunc.o : ../jpuapi/jpuapifunc.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/jpulog.o : src/jpulog.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR):
	-mkdir -p $(OBJDIR)

force_dependency :
	true
