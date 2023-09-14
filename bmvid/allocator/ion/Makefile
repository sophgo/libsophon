
CHIP ?= bm1682
PRODUCTFORM ?= soc
INSTALL_DIR ?= ./release
DEBUG ?= 0
BMVID_ROOT ?= $(BMVID_TOP_DIR)
#$(info $(CHIP))

ifeq ($(PRODUCTFORM),pcie)
CROSS_CC_PREFIX =
else ifeq ($(PRODUCTFORM),pcie_mips64)
CROSS_CC_PREFIX = mips-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_sw64)
CROSS_CC_PREFIX = sw_64-sunway-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_loongarch64)
CROSS_CC_PREFIX = loongarch64-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_riscv64)
CROSS_CC_PREFIX =
else
CROSS_CC_PREFIX = aarch64-linux-gnu-
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

INCLUDES = -I./inc -I$(INSTALL_DIR)/include
DEFINES += -D$(chip_type)
CFLAGS  += -fPIC -Wno-deprecated-declarations -Wall -Wl,--fatal-warning $(INCLUDES) $(DEFINES) 
LDFLAGS += -L. -L$(INSTALL_DIR)/lib
LIBS = -lm -lrt -lbmion -lpthread 

ifeq ($(DEBUG), 1)
CFLAGS += -g -ggdb
endif

ifneq ($(PRODUCTFORM), soc)
CFLAGS += -DBM_PCIE_MODE
endif

ifeq ($(PRODUCTFORM), pcie_mips64)
CFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
LDFLAGS += -Wl,-melf64ltsmip
endif

ifeq ($(PRODUCTFORM), pcie_sw64)
CFLAGS += --machine=sw6b
LDFLAGS +=
endif

ifeq ($(PRODUCTFORM), pcie_loongarch64)
LDFLAGS += -Wl,-melf64loongarch
endif

TARGET=libbmion.so
IONTEST=test_ion
MAKEFILE=Makefile
OBJDIR=obj
ALLOBJS=*.o
ALLDEPS=*.dep
RM ?= rm -rf
CP ?= cp -f
MKDIR ?= mkdir -p

-include $(BMVID_ROOT)/build/version.mak
ifneq ($(SO_NAME),)
    TARGET_SONAME=$(TARGET)$(SO_NAME)
    TARGET_SOVERSION=$(TARGET)$(SO_VERSION)
endif

LIB_SRC := src/bmionapi.c
ifneq ($(PRODUCTFORM),soc)
    LIB_SRC += src/bm_pcie_buffer.c
else
    LIB_SRC += src/bm_ion.c
endif

LIB_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(LIB_SRC)))
LIB_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(LIB_OBJS)))

TEST_SRC = example/test_ion.c

TEST_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(TEST_SRC)))
TEST_PATHS=$(addprefix $(OBJDIR)/,$(notdir $(TEST_OBJS)))

OBJECTPATHS=$(LIB_PATHS) $(TEST_PATHS)

all: $(OBJDIR) $(TARGET) $(IONTEST)

$(IONTEST): $(TARGET) $(TEST_PATHS)
	$(CC) $(CFLAGS) -o $@ $(TEST_PATHS) $(LDFLAGS) ${LIBS}

$(TARGET): $(LIB_PATHS)
	$(CC) $(CFLAGS)  -o $@ -shared -Wl,-soname,$@$(SO_NAME) $(LIB_PATHS) $(LDFLAGS) -L$(INSTALL_DIR)/lib
ifneq ($(TARGET_SOVERSION), )
	mv $@ $(TARGET_SOVERSION)
	ln -sf $(TARGET_SOVERSION) $(TARGET_SONAME)
	ln -sf $(TARGET_SONAME) $@
endif

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(IONTEST)
	$(RM) $(TARGET) $(TARGET_SOVERSION) $(TARGET_SONAME)
	$(RM) $(OBJDIR)/$(ALLDEPS)
	$(RM) $(OBJDIR)/$(ALLOBJS)

install: $(TARGET) $(IONTEST)
	install -d $(INSTALL_DIR)/bin
	install -d $(INSTALL_DIR)/lib
	install -d $(INSTALL_DIR)/include
	install $(IONTEST) $(INSTALL_DIR)/bin
	install -m 0644 inc/bm_ion.h $(INSTALL_DIR)/include
ifeq ($(TARGET_SOVERSION), )
	install $(TARGET) $(INSTALL_DIR)/lib
else
	install $(TARGET_SOVERSION) $(INSTALL_DIR)/lib
	cp -ap $(TARGET_SONAME) $(TARGET) $(INSTALL_DIR)/lib
endif


uninstall:
	$(RM) $(INSTALL_DIR)/bin/$(IONTEST)
	$(RM) $(INSTALL_DIR)/lib/$(TARGET)
ifneq ($(TARGET_SOVERSION), )
	$(RM) $(INSTALL_DIR)/lib/$(TARGET_SONAME) $(INSTALL_DIR)/lib/$(TARGET_SOVERSION)
endif
	$(RM) $(INSTALL_DIR)/include/bm_ion.h

$(OBJDIR)/bmionapi.o : src/bmionapi.c
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bm_ion.o : src/bm_ion.c
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/bm_pcie_buffer.o : src/bm_pcie_buffer.c
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR)/test_ion.o : example/test_ion.c
	$(CC) $(CFLAGS) -Wall -c $< -o $@ -MD -MF $(@:.o=.dep)

$(OBJDIR):
	-mkdir -p $(OBJDIR)

force_dependency:
	true
