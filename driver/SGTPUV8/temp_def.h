/*
 * Copyright (c) 2015-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TEMP_DEF_H__
#define __TEMP_DEF_H__


//#define CLI_NO_WAIT
#define CLI_HOT_KEY ('j')
#define NON_CACHEABLE_FLASH

// DDR options
//#define DDR_ECC_ENABLE
//#define DDR_PERF_ENABLE
#define DDR_INTERLEAVE_ENABLE
//#define DDR_PHY_ENABLE
#define DDR_TYPE_LPDDR


#define PLATFORM_STACK_SIZE 0x1000

#define PLATFORM_MAX_CPUS_PER_CLUSTER	4
#define PLATFORM_CLUSTER_COUNT		2
#define PLATFORM_CLUSTER0_CORE_COUNT	PLATFORM_MAX_CPUS_PER_CLUSTER
#define PLATFORM_CLUSTER1_CORE_COUNT	PLATFORM_MAX_CPUS_PER_CLUSTER
#define PLATFORM_CORE_COUNT		(PLATFORM_CLUSTER0_CORE_COUNT + \
					 PLATFORM_CLUSTER1_CORE_COUNT)

#define BM_PRIMARY_CPU	0

#define BM_NCORE

#define PLAT_NUM_PWR_DOMAINS		(PLATFORM_CLUSTER_COUNT + \
					PLATFORM_CORE_COUNT)
#define PLAT_MAX_PWR_LVL		MPIDR_AFFLVL1

#define PLAT_MAX_RET_STATE		1
#define PLAT_MAX_OFF_STATE		2

/* Local power state for power domains in Run state. */
#define PLAT_LOCAL_STATE_RUN		0
/* Local power state for retention. Valid only for CPU power domains */
#define PLAT_LOCAL_STATE_RET		1
/*
 * Local power state for OFF/power-down. Valid for CPU and cluster power
 * domains.
 */
#define PLAT_LOCAL_STATE_OFF		2

/*
 * Macros used to parse state information from State-ID if it is using the
 * recommended encoding for State-ID.
 */
#define PLAT_LOCAL_PSTATE_WIDTH		4
#define PLAT_LOCAL_PSTATE_MASK		((1 << PLAT_LOCAL_PSTATE_WIDTH) - 1)

/*
 * Some data must be aligned on the biggest cache line size in the platform.
 * This is known only to the platform as it might have a combination of
 * integrated and external caches.
 */
#define CACHE_WRITEBACK_SHIFT		6
#define CACHE_WRITEBACK_GRANULE		(1 << CACHE_WRITEBACK_SHIFT)

#define PLAT_PHY_ADDR_SPACE_SIZE	(1ull << 36)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ull << 36)
#ifdef BL1_INIT_DDR
#define MAX_MMAP_REGIONS		10
#else
#define MAX_MMAP_REGIONS		9 // mmap_add_region
#endif


/*
 * Partition memory into secure ROM, non-secure SRAM, secure SRAM.
 * the "SRAM" region is NPS SRAM, 256KB in total.
 * all sizes need to be page aligned for page table requirement.
 */
#define NS_DRAM0_BASE			0x100000000 // 8GB
#define NS_DRAM1_BASE			0x300000000 // 4GB
#define NS_DRAM2_BASE			0x400000000 // 4GB

#define NS_DRAM_BASE			NS_DRAM1_BASE
#define NS_DRAM_SIZE			0x10000000 // 256MB

#define SEC_SRAM_BASE			0x10000000
#define SEC_SRAM_SIZE			0x00040000 // 256KB

#define NS_IMAGE_OFFSET			(NS_DRAM_BASE + 0x8000000)



#define BM_FLASH_BASE			SEC_SRAM_BASE + 0x100000 // 1MB
#define BM_FLASH_SIZE			0x00200000 // 2MB

#define NS_SRAM_UNUSED_BASE		BM_FLASH_BASE + BM_FLASH_SIZE
#define NS_SRAM_UNUSED_SIZE		0x100000 // 1MB

/*
 * ARM-TF lives in SRAM, partition it here
 */
#define SHARED_RAM_BASE			SEC_SRAM_BASE
#define SHARED_RAM_SIZE			0x00001000 // 4KB

#define PLAT_BM_TRUSTED_MAILBOX_BASE	SHARED_RAM_BASE
#define PLAT_BM_TRUSTED_MAILBOX_SIZE	(8 + PLAT_BM_HOLD_SIZE)
#define PLAT_BM_HOLD_BASE		(PLAT_BM_TRUSTED_MAILBOX_BASE + 8)
#define PLAT_BM_HOLD_SIZE		(PLATFORM_CORE_COUNT * \
					  PLAT_BM_HOLD_ENTRY_SIZE)
#define PLAT_BM_HOLD_ENTRY_SIZE	8
#define PLAT_BM_HOLD_STATE_WAIT	0
#define PLAT_BM_HOLD_STATE_GO	1

#define BL_RAM_BASE			(SHARED_RAM_BASE + SHARED_RAM_SIZE)
#define BL_RAM_SIZE			(SEC_SRAM_SIZE - SHARED_RAM_SIZE)



// some non-cached variables, located in flash regoin for non-secure access by dbg_i2c or PCIe
#define FIP_LOADED_REG			(BM_FLASH_BASE + BM_FLASH_SIZE - 4) // 4bytes
#define FIP_SOURCE_REG			(BM_FLASH_BASE + BM_FLASH_SIZE - 8) // 4bytes
#define FIP_RETRY_REG			(BM_FLASH_BASE + BM_FLASH_SIZE - 12) // 4bytes
#define RISCV_HOLD_BASE			(BM_FLASH_BASE + BM_FLASH_SIZE - 16) // 4bytes, 16bytes align


/*
 * FIP binary defines.
 */
#define PLAT_BM_FIP_BASE	BM_FLASH_BASE
#define PLAT_BM_FIP_MAX_SIZE	BM_FLASH_SIZE


/*
 * device register defines.
 */
#define DEVICE0_BASE			0x00000000 // ITCM
#define DEVICE0_SIZE			0x00001000 // 4KB
#define DEVICE1_BASE			0x06000000 // SPI flash
#define DEVICE1_SIZE			0x00200000 // 2MB
#define DEVICE2_BASE			0x50000000 // peripheral registers
#define DEVICE2_SIZE			0x00300000 // 3MB
#define DEVICE3_BASE			0x5F600000 // PCIe config
#define DEVICE3_SIZE			0x00A00000 // 10MB

#define DDRC_BASE			0x68000000 // DDR controller
#define DDRC_SIZE			0x08000000 // 128MB

#define SDIO_BASE			0x50101000
#define DDR_CTRL2			0x68000000
#define DDR_CTRL0_A			0x6A000000
#define DDR_CTRL0_B			0x6C000000
#define DDR_CTRL1			0x6E000000
#define TOP_BASE			0x50010000
#define WATCHDOG_BASE			0x50026000
#define EFUSE_BASE			0x50028000
#define NCORE_BASE			0x50200000
#define DMAC_BASE			0x50110000

/*
 * TOP registers.
 */
#define REG_TOP_CHIP_VERSION		0x0
#define REG_TOP_CONF_INFO		0x4
#define REG_TOP_CTRL			0x8
#define REG_TOP_ARM_BOOT_ADDR		0x20
#define REG_TOP_RISCV_RST_VEC		0x3C
#define REG_TOP_PLL_EN_CTRL		0xC4
#define REG_TOP_MPLL_CTRL		0xE8
#define REG_TOP_TPLL_CTRL		0xEC
#define REG_TOP_FPLL_CTRL		0xF0
#define REG_TOP_VPLL_CTL		0xF4
#define REG_TOP_PINMUX_BASE		0x488 // starts from GPIO0
#define REG_TOP_SOFT_RST0		0xC00
#define REG_TOP_SOFT_RST1		0xC04

#define BIT_SHIFT_TOP_CONF_INFO_MODE_SEL 0
#define BIT_SHIFT_TOP_CONF_INFO_BOOT_SEL 3
#define BIT_SHIFT_TOP_CONF_INFO_EFUSE_CHK 15

#define BIT_MASK_TOP_CONF_INFO_MODE_SEL	(0x3 << BIT_SHIFT_TOP_CONF_INFO_MODE_SEL)
#define BIT_MASK_TOP_CONF_INFO_BOOT_SEL	(0x7 << BIT_SHIFT_TOP_CONF_INFO_BOOT_SEL)
#define BIT_MASK_TOP_CONF_INFO_EFUSE_CHK (0x1 << BIT_SHIFT_TOP_CONF_INFO_EFUSE_CHK)

#define BIT_MASK_TOP_CTRL_AHBROM_BOOT_FIN	(1 << 0) // 1 to put boot ROM in sleep mode
#define BIT_MASK_TOP_CTRL_ITCM_AXI_ENABLE	(1 << 1) // 0: ITCM for ARM9; 1: ITCM for A53
#define BIT_MASK_TOP_CTRL_SW_ROOT_RESET_EN	(1 << 2) // 1 to enable warm reboot

#define BIT_MASK_TOP_SOFT_RST0_ARM9		(1 << 1) // 0 to reset
#define BIT_MASK_TOP_SOFT_RST0_RISCV		(1 << 2)
#define BIT_MASK_TOP_SOFT_RST0_DDRC		(0xFF << 3)
#define BIT_MASK_TOP_SOFT_RST0_SDIO		(1 << 21)

/*
 * GIC definitions.
 */
#define PLAT_ARM_GICD_BASE		0x50001000
#define PLAT_ARM_GICC_BASE		0x50002000

#define BM_IRQ_SEC_SGI_0		8
#define BM_IRQ_SEC_SGI_1		9
#define BM_IRQ_SEC_SGI_2		10
#define BM_IRQ_SEC_SGI_3		11
#define BM_IRQ_SEC_SGI_4		12
#define BM_IRQ_SEC_SGI_5		13
#define BM_IRQ_SEC_SGI_6		14
#define BM_IRQ_SEC_SGI_7		15
#define BM_IRQ_SEC_PHY_TIMER		29

#define PLAT_ARM_G1S_IRQS		BM_IRQ_SEC_PHY_TIMER, \
					BM_IRQ_SEC_SGI_1, \
					BM_IRQ_SEC_SGI_2, \
					BM_IRQ_SEC_SGI_3, \
					BM_IRQ_SEC_SGI_4, \
					BM_IRQ_SEC_SGI_5, \
					BM_IRQ_SEC_SGI_7
#define PLAT_ARM_G0_IRQS		BM_IRQ_SEC_SGI_0, \
					BM_IRQ_SEC_SGI_6

/*
 * MODE_SEL definitions
 */
#define MODE_NORMAL		0x0
#define MODE_FAST		0x1
#define MODE_SAFE		0x2
#define MODE_BYPASS		0x3

/*
 * clock definitions.
 * system counter uses the external reference clock of MPLL,
 * which is fixed at 50MHz.
 */
#define SYS_COUNTER_FREQ_IN_TICKS		50000000
#define SYS_COUNTER_FREQ_IN_TICKS_BYPASS	25000000


#define PLAT_BM_BOOT_SD_CLK_IN_HZ_NORMAL	100000000
#define PLAT_BM_BOOT_SD_CLK_IN_HZ_FAST		100000000
#define PLAT_BM_BOOT_SD_CLK_IN_HZ_SAFE		100000000
#define PLAT_BM_BOOT_SD_CLK_IN_HZ_BYPASS	25000000





// cell index for block content and its duplicate, physical
#define EBLK_BASE_SECONF	0
#define EBLK_DUP_SECONF		0
#define EBLK_BASE_ROTPKD	18
#define EBLK_DUP_ROTPKD		26

// bit define in cell[0], logical, after merged double bits
#define EBIT_SHIFT_SEC_FIREWALL		0
#define EBIT_SHIFT_JTAG_DBG_DIS		1
#define EBIT_SHIFT_ONCHIP_BOOT		2
#define EBIT_SHIFT_SECURE_BOOT		3

/*
 * NCORE definitions
 */
#define NCORE_DIRUCASER0	0x80040
#define NCORE_DIRUMRHER		0x80070
#define NCORE_DIRUSFER		0x80010
#define NCORE_CSADSER		0xff040

#endif /* __PLATFORM_DEF_H__ */
