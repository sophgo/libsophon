SHELL:=/bin/bash

ifeq ($(PRODUCTFORM),pcie) # pcie mode
    CROSS_COMPILE =
else ifeq ($(PRODUCTFORM),pcie_mips64)
    CROSS_COMPILE = mips-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_sw64) # pcie mode
    CROSS_COMPILE = sw_64-sunway-linux-gnu-
else ifeq ($(PRODUCTFORM),pcie_loongarch64)
    CROSS_COMPILE = loongarch64-linux-gnu-
else # pcie_arm64 and soc mode
    CROSS_COMPILE = aarch64-linux-gnu-
endif

CXX= $(CROSS_COMPILE)g++

$(warning $(CXX))

CXXFLAGS        += -Wextra -funwind-tables -fno-short-enums $(CONFIG_FLAGS)\
 				   -Wall -Wno-error=deprecated-declarations \
 				   -ffunction-sections -fdata-sections -fno-strict-aliasing \
 				   -Wno-unused-function \
 				   -fPIC -ffp-contract=off -fexceptions

ifeq ($(PRODUCTFORM), soc)
CXXFLAGS += -fsigned-char
else ifeq ($(PRODUCTFORM), pcie_arm64)
CXXFLAGS += -fsigned-char
else ifeq ($(PRODUCTFORM), pcie_mips64)
CXXFLAGS += -mips64r2 -mabi=64 -march=gs464e -D_GLIBCXX_USE_CXX11_ABI=0
else ifeq ($(PRODUCTFORM), pcie_loongarch64)
CXXFLAGS += -Wno-format-truncation -Wno-stringop-truncation
else ifeq ($(PRODUCTFORM), pcie_sw64)
CXXFLAGS += -Wno-format-truncation -Wno-stringop-truncation --machine=sw6b
else  # pcie_x86
CXXFLAGS +=
endif

LDFLAGS         += 	-pthread \
			-Wl,--check-sections \
			-Wl,--gc-sections
ifeq ($(PRODUCTFORM), pcie_mips64)
LDFLAGS         += -mips64r2 -mabi=64 -march=gs464e
endif

ifeq ($(DEBUG),1)
CXXFLAGS          += -g -DDEBUG
LDFLAGS         += -rdynamic
else
CXXFLAGS          += -O3
endif

ifeq ($(PCIE_MODE_ENABLE_CPU), 1)
CXXFLAGS += -DPCIE_MODE_ENABLE_CPU
endif


libvpp_cmodel.so: bmcv_1684x_vpp_cmodel.cpp 1684vppcmodel.c
	$(CXX) -fPIC -shared bmcv_1684x_vpp_cmodel.cpp 1684vppcmodel.c -I . $(CXXFLAGS) $(LDFLAGS) -o libvpp_cmodel.so
	mv libvpp_cmodel.so ../../3rdparty/libbmcv/lib/$(PRODUCTFORM)

cmodel_bmcv: libvpp_cmodel.so