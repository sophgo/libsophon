
DEBUG        ?= 0
CHIP         ?= bm1684
SUBTYPE      ?= asic
PRODUCTFORM  ?= soc
ABI_FLAG	 ?= 1
INSTALL_DIR    ?= $(OUT_DIR)

NATIVE_SDK_DIR ?= $(PWD)/../install/$(PRODUCTFORM)_$(CHIP)_$(SUBTYPE)
SDK_DIR        ?= $(NATIVE_SDK_DIR)
BMCV_SDK_DIR   ?= $(NATIVE_SDK_DIR)/bmcv
BMLIB_DIR      ?= $(PWD)/../3rdparty/libbmcv

# TODO add more samples
SUB_DIRS := media_version

.PHONY: all clean install uninstall

PARAMETERS := CHIP=$(CHIP) \
	SDK_DIR=$(SDK_DIR) \
	BMCV_SDK_DIR=$(BMCV_SDK_DIR) \
	BMLIB_DIR=$(BMLIB_DIR) \
	INSTALL_DIR=${INSTALL_DIR} \
	ABI_FLAG=${ABI_FLAG} \

all:
	$(foreach N, $(SUB_DIRS), make $(PARAMETERS) -C $(N) || exit 1;)

clean:
	$(foreach N, $(SUB_DIRS), make $(PARAMETERS) -C $(N) clean;)

install:
	$(foreach N, $(SUB_DIRS), make $(PARAMETERS) -C $(N) install || exit 1;)

uninstall:
	$(foreach N, $(SUB_DIRS), make $(PARAMETERS) -C $(N) uninstall;)