#
# Copyright (c) 2013-2017, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

include lib/libfdt/libfdt.mk

PLAT_INCLUDES		:=	-Iinclude/plat/arm/common/		\
				-Iinclude/plat/arm/common/aarch64/	\
				-Iplat/bitmain/bm1682_common/include	\
				-Iplat/bitmain/bm1682_asic/include	\
				-Iinclude/common/tbbr

PLAT_BL_COMMON_SOURCES	:=	plat/bitmain/bm1682_common/bm_common.c		\
				drivers/console/aarch64/console.S		\
				drivers/ti/uart/aarch64/16550_console.S		\
				lib/xlat_tables/xlat_tables_common.c		\
				lib/xlat_tables/aarch64/xlat_tables.c

BL1_SOURCES		+=	drivers/io/io_storage.c	\
				drivers/io/io_fip.c	\
				drivers/io/io_memmap.c	\
				plat/bitmain/bm1682_common/bm_io_storage.c	\
				lib/cpus/aarch64/aem_generic.S			\
				lib/cpus/aarch64/cortex_a53.S			\
				plat/bitmain/bm1682_common/aarch64/plat_helpers.S	\
				plat/bitmain/bm1682_common/bm_bl1_setup.c

BL2_SOURCES		+=	drivers/io/io_storage.c	\
				drivers/io/io_fip.c	\
				drivers/io/io_memmap.c	\
				plat/bitmain/bm1682_common/bm_io_storage.c		\
				plat/bitmain/bm1682_common/aarch64/plat_helpers.S	\
				plat/bitmain/bm1682_common/bm_bl2_setup.c

BL31_SOURCES		+=	lib/cpus/aarch64/aem_generic.S		\
				lib/cpus/aarch64/cortex_a53.S		\
				drivers/arm/gic/v2/gicv2_helpers.c	\
				drivers/arm/gic/v2/gicv2_main.c		\
				drivers/arm/gic/common/gic_common.c	\
				plat/common/aarch64/plat_psci_common.c	\
				plat/bitmain/bm1682_common/bm_pm.c	\
				plat/bitmain/bm1682_common/topology.c	\
				plat/bitmain/bm1682_common/aarch64/plat_helpers.S	\
				plat/bitmain/bm1682_common/bm_bl31_setup.c		\
				plat/bitmain/bm1682_asic/bm_bl31_sub_setup.c		\
				plat/bitmain/bm1682_common/bm_gic.c

# Disable the PSCI platform compatibility layer
ENABLE_PLAT_COMPAT	:= 	0
