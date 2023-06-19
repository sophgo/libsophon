
INSTALL_DIR ?= ./release
BMCV_PATH ?= $(BMVID_TOP_DIR)/3rdparty/libbmcv
ION_DIR ?= ./release
BMVID_ROOT ?= $(BMVID_TOP_DIR)

CHIP := bm1684
SUBTYPE := asic
PRODUCTFORM := soc
DEBUG := 0

ifeq ($(CHIP), bm1684)
    CHIP_TYPE = CHIP_BM1684
else
    $(error "only support BM1684")
endif

ifeq ($(PRODUCTFORM),pcie) # pcie mode
    CROSS_CC_PREFIX =
else ifeq ($(PRODUCTFORM),pcie_mips64)
    CROSS_CC_PREFIX = mips-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_sw64) # pcie mode
    CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_loongarch64)
    CROSS_CC_PREFIX = loongarch64-linux-gnu-
else # pcie_arm64 and soc mode
    CROSS_CC_PREFIX = aarch64-linux-gnu-
endif

ifneq ($(PRODUCTFORM), soc)
    DEFINES += -DBM_PCIE_MODE
endif

CC = $(CROSS_CC_PREFIX)gcc

INCLUDES = -I. -I./inc/ -I$(INSTALL_DIR)/include -I$(BMCV_PATH)/include -I../driver/include/$(CHIP) -I$(ION_DIR)/include
INCLUDES += -I../driver/include
DEFINES += -D$(CHIP_TYPE)

CFLAGS  += -I. -fPIC -Wall -Wl,--fatal-warning $(INCLUDES) $(DEFINES) -std=c99

ifneq ($(DEBUG), 0)
CFLAGS += -O0 -g
endif

ifeq ($(PRODUCTFORM), pcie_loongarch64)
LDFLAGS += -Wl,-melf64loongarch
LDLIBS += -lm -lstdc++ -lpthread
endif

ifeq ($(PRODUCTFORM), pcie_sw64)
LDFLAGS +=
LDLIBS += -lstdc++
endif

LDFLAGS += -L$(INSTALL_DIR)/lib -L$(BMCV_PATH)/lib/$(PRODUCTFORM) -L../driver/release -L. -L$(ION_DIR)/lib

ifneq ($(PRODUCTFORM), soc)
	TEST_LDLIBS += -lbmvppapi -lbmion -lbmlib -ldl
    TARGET_LDLIBS += -lbmion -Wl,-Bstatic -lvpp -Wl,-Bdynamic
else
	TEST_LDLIBS += -lbmvppapi -lbmion
	TARGET_LDLIBS += -lbmion -Wl,-Bstatic -lvpp -Wl,-Bdynamic -lm
endif

ifeq ($(PRODUCTFORM),pcie_mips64)
CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
LDFLAGS += -Wl,-melf64ltsmip
endif

TEST=test_bmvpp
TARGET=libbmvppapi.so
MAKEFILE=Makefile
OBJDIR=obj
ALLOBJS=*.o
ALLDEPS=*.dep
RM=rm -rf
MKDIR=mkdir -p

-include $(BMVID_ROOT)/build/version.mak
ifneq ($(SO_NAME),)
    TARGET_SONAME=$(TARGET)$(SO_NAME)
    TARGET_SOVERSION=$(TARGET)$(SO_VERSION)
endif

TEST_SRC := test.c
TEST_OBJS=$(patsubst %.c,%.o,$(TEST_SRC))
TEST_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(TEST_OBJS)))

LIB_SRC := bmvpp_1684.c bmvpp_common.c bmvpp_logging.c
LIB_OBJS=$(patsubst %.c,%.o,$(LIB_SRC))
LIB_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(LIB_OBJS)))

OBJECTPATHS=$(LIB_PATHS) $(TEST_PATHS)

all: CREATE_OBJDIR $(TARGET) $(TEST)

$(TEST): $(TARGET) $(TEST_PATHS)
	$(CC) -o $@ $(TEST_PATHS) $(CFLAGS) $(LDFLAGS) $(TEST_LDLIBS) $(LDLIBS)

$(TARGET): $(LIB_PATHS)
	$(CC) $(LIB_PATHS) $(CFLAGS) $(LDFLAGS) $(TARGET_LDLIBS) -Wall -fPIC -shared -Wl,-soname,$(TARGET)$(SO_NAME) -o $(TARGET)
ifneq ($(TARGET_SOVERSION), )
	mv $@ $(TARGET_SOVERSION)
	ln -sf $(TARGET_SOVERSION) $(TARGET_SONAME)
	ln -sf $(TARGET_SONAME) $@
endif

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TARGET) $(TARGET_SOVERSION) $(TARGET_SONAME)
	$(RM) $(OBJDIR)

install:
	install -d $(INSTALL_DIR)/bin
	install -d $(INSTALL_DIR)/lib
	install -d $(INSTALL_DIR)/include
	install $(TEST) $(INSTALL_DIR)/bin
	install -m 644 inc/bmvppapi.h $(INSTALL_DIR)/include
ifeq ($(TARGET_SOVERSION), )
	install -m 644 $(TARGET) $(INSTALL_DIR)/lib
else
	install -m 644 $(TARGET_SOVERSION) $(INSTALL_DIR)/lib
	cp -ap $(TARGET_SONAME) $(TARGET) $(INSTALL_DIR)/lib
endif


uninstall:
	$(RM) $(INSTALL_DIR)/include/bmvppapi.h
	$(RM) $(INSTALL_DIR)/lib/$(TARGET)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_SOVERSION)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_SONAME)
	$(RM) $(INSTALL_DIR)/bin/$(TEST)

$(OBJDIR)/test.o : test/test.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmvpp_common.o : src/bmvpp_common.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmvpp_1684.o : src/bmvpp_1684.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bmvpp_logging.o : src/bmvpp_logging.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

CREATE_OBJDIR:
	-mkdir -p $(OBJDIR)

force_dependency :
	true
