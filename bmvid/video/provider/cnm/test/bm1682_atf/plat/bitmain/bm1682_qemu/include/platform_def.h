/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLATFORM_DEF_H__
#define __PLATFORM_DEF_H__

#include <arch.h>
#include <common_def.h>
#include <tbbr_img_def.h>

//#define BL1_USE_CLI

/* Special value used to verify platform parameters from BL2 to BL3-1 */
#define BM_BL31_PLAT_PARAM_VAL    0x0f1e2d3c4b5a6978ULL

#define PLATFORM_STACK_SIZE 0x1000

#define PLATFORM_MAX_CPUS_PER_CLUSTER    4
#define PLATFORM_CLUSTER_COUNT        2
#define PLATFORM_CLUSTER0_CORE_COUNT    PLATFORM_MAX_CPUS_PER_CLUSTER
#define PLATFORM_CLUSTER1_CORE_COUNT    PLATFORM_MAX_CPUS_PER_CLUSTER
#define PLATFORM_CORE_COUNT        (PLATFORM_CLUSTER0_CORE_COUNT + \
                     PLATFORM_CLUSTER1_CORE_COUNT)

#define BM_PRIMARY_CPU            0

#define PLAT_NUM_PWR_DOMAINS        (PLATFORM_CLUSTER_COUNT + \
                    PLATFORM_CORE_COUNT)
#define PLAT_MAX_PWR_LVL        MPIDR_AFFLVL1

#define PLAT_MAX_RET_STATE        1
#define PLAT_MAX_OFF_STATE        2

/* Local power state for power domains in Run state. */
#define PLAT_LOCAL_STATE_RUN        0
/* Local power state for retention. Valid only for CPU power domains */
#define PLAT_LOCAL_STATE_RET        1
/*
 * Local power state for OFF/power-down. Valid for CPU and cluster power
 * domains.
 */
#define PLAT_LOCAL_STATE_OFF        2

/*
 * Macros used to parse state information from State-ID if it is using the
 * recommended encoding for State-ID.
 */
#define PLAT_LOCAL_PSTATE_WIDTH        4
#define PLAT_LOCAL_PSTATE_MASK        ((1 << PLAT_LOCAL_PSTATE_WIDTH) - 1)

/*
 * Some data must be aligned on the biggest cache line size in the platform.
 * This is known only to the platform as it might have a combination of
 * integrated and external caches.
 */
#define CACHE_WRITEBACK_SHIFT        6
#define CACHE_WRITEBACK_GRANULE        (1 << CACHE_WRITEBACK_SHIFT)

#define PLAT_PHY_ADDR_SPACE_SIZE    (1ull << 36)
#define PLAT_VIRT_ADDR_SPACE_SIZE    (1ull << 36)
#define MAX_MMAP_REGIONS        8
#define MAX_XLAT_TABLES            8 // varies when memory layout changes
#define MAX_IO_DEVICES            3
#define MAX_IO_HANDLES            4

/*
 * Partition memory into secure ROM, non-secure SRAM, secure SRAM.
 * the "SRAM" region is actually ARM926's ITCM, 512KB total.
 *
 */
// all in DDR, check qemu/hw/arm/virt.c for QEMU memory layout
#define NS_DRAM0_BASE            0x100000000
#define NS_DRAM0_SIZE            0x10000000 // 256MB

#define SEC_SRAM_BASE            0x45100000 // don't be too far from ROM, may get a buildtime relocation error
#define SEC_SRAM_SIZE            0x00040000 // 256KB

#define NS_IMAGE_OFFSET            0x108000000 // BL33 (u-boot) image loading address

#define SEC_ROM_BASE            0x00000000
#define SEC_ROM_SIZE            0x00020000 // 128KB

#define BM_FLASH0_BASE            0x110000000
#define BM_FLASH0_SIZE            0x00080000 // 512KB

/*
 * ARM-TF lives in SRAM, partition it here
 */
#define SHARED_RAM_BASE            SEC_SRAM_BASE
#define SHARED_RAM_SIZE            0x00001000 // 4KB

#define PLAT_BM_TRUSTED_MAILBOX_BASE    SHARED_RAM_BASE
#define PLAT_BM_TRUSTED_MAILBOX_SIZE    (8 + PLAT_BM_HOLD_SIZE)
#define PLAT_BM_HOLD_BASE        (PLAT_BM_TRUSTED_MAILBOX_BASE + 8)
#define PLAT_BM_HOLD_SIZE        (PLATFORM_CORE_COUNT * \
                      PLAT_BM_HOLD_ENTRY_SIZE)
#define PLAT_BM_HOLD_ENTRY_SIZE    8
#define PLAT_BM_HOLD_STATE_WAIT    0
#define PLAT_BM_HOLD_STATE_GO    1

#define BL_RAM_BASE            (SHARED_RAM_BASE + SHARED_RAM_SIZE)
#define BL_RAM_SIZE            (SEC_SRAM_SIZE - SHARED_RAM_SIZE)

/*
 * BL1 specific defines.
 *
 * BL1 RW data is relocated from ROM to RAM at runtime so we need 2 sets of
 * addresses.
 * Put BL1 RW at the top of the Secure SRAM. BL1_RW_BASE is calculated using
 * the current BL1 RW debug size plus a little space for growth.
 */
#define BL1_RO_BASE            SEC_ROM_BASE
#define BL1_RO_LIMIT            (SEC_ROM_BASE + SEC_ROM_SIZE)
#define BL1_RW_BASE            (BL1_RW_LIMIT - 0x12000) // 72KB
#define BL1_RW_LIMIT            (BL_RAM_BASE + BL_RAM_SIZE)

/*
 * BL2 specific defines.
 *
 * Put BL2 just below BL3-1. BL2_BASE is calculated using the current BL2 debug
 * size plus a little space for growth.
 */
#define BL2_BASE            (BL31_BASE - 0x1D000) // 116KB
#define BL2_LIMIT            BL31_BASE

/*
 * BL3-1 specific defines.
 *
 * Put BL3-1 at the top of the Trusted SRAM. BL31_BASE is calculated using the
 * current BL3-1 debug size plus a little space for growth.
 */
#define BL31_BASE            (BL31_LIMIT - 0x20000) // 128KB
#define BL31_LIMIT            (BL_RAM_BASE + BL_RAM_SIZE)
#define BL31_PROGBITS_LIMIT        BL1_RW_BASE

/*
 * FIP binary defines.
 */
#define PLAT_BM_FIP_BASE    BM_FLASH0_BASE
#define PLAT_BM_FIP_MAX_SIZE    BM_FLASH0_SIZE

/*
 * device register defines.
 */
#define DEVICE0_BASE            0x08000000 // GIC
#define DEVICE0_SIZE            0x00021000
#define DEVICE1_BASE            0x09000000 // UART
#define DEVICE1_SIZE            0x00011000

/*
 * PL011 related constants
 */
#define UART0_BASE            0x09000000
#define UART1_BASE            0x09040000
#define UART0_CLK_IN_HZ            1
#define UART1_CLK_IN_HZ            1

#define PLAT_BM_BOOT_UART_BASE        UART0_BASE
#define PLAT_BM_BOOT_UART_CLK_IN_HZ    UART0_CLK_IN_HZ

#define PLAT_BM_CRASH_UART_BASE        UART1_BASE
#define PLAT_BM_CRASH_UART_CLK_IN_HZ    UART1_CLK_IN_HZ

#define PLAT_BM_CONSOLE_BAUDRATE    115200

/*
 * GIC related constants
 */
#define GICD_BASE            0x08000000
#define GICC_BASE            0x08010000
#define GICR_BASE            0

#define QEMU_IRQ_SEC_SGI_0        8
#define QEMU_IRQ_SEC_SGI_1        9
#define QEMU_IRQ_SEC_SGI_2        10
#define QEMU_IRQ_SEC_SGI_3        11
#define QEMU_IRQ_SEC_SGI_4        12
#define QEMU_IRQ_SEC_SGI_5        13
#define QEMU_IRQ_SEC_SGI_6        14
#define QEMU_IRQ_SEC_SGI_7        15

/*
 * System counter
 */
#define SYS_COUNTER_FREQ_IN_TICKS    ((1000 * 1000 * 1000) / 16)

#endif /* __PLATFORM_DEF_H__ */
