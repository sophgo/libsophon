/*
 * Copyright (c) 2013-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <generic_delay_timer.h>
#include <mmio.h>
#include <platform.h>
#include <xlat_tables.h>
#include "../zynqmp_private.h"

/*
 * Table of regions to map using the MMU.
 * This doesn't include TZRAM as the 'mem_layout' argument passed to
 * configure_mmu_elx() will give the available subset of that,
 */
const mmap_region_t plat_arm_mmap[] = {
    { DEVICE0_BASE, DEVICE0_BASE, DEVICE0_SIZE, MT_DEVICE | MT_RW | MT_SECURE },
    { DEVICE1_BASE, DEVICE1_BASE, DEVICE1_SIZE, MT_DEVICE | MT_RW | MT_SECURE },
    { CRF_APB_BASE, CRF_APB_BASE, CRF_APB_SIZE, MT_DEVICE | MT_RW | MT_SECURE },
    {0}
};

static unsigned int zynqmp_get_silicon_ver(void)
{
    static unsigned int ver;

    if (!ver) {
        ver = mmio_read_32(ZYNQMP_CSU_BASEADDR +
                   ZYNQMP_CSU_VERSION_OFFSET);
        ver &= ZYNQMP_SILICON_VER_MASK;
        ver >>= ZYNQMP_SILICON_VER_SHIFT;
    }

    return ver;
}

unsigned int zynqmp_get_uart_clk(void)
{
    unsigned int ver = zynqmp_get_silicon_ver();

    switch (ver) {
    case ZYNQMP_CSU_VERSION_VELOCE:
        return 48000;
    case ZYNQMP_CSU_VERSION_EP108:
        return 25000000;
    case ZYNQMP_CSU_VERSION_QEMU:
        return 133000000;
    }

    return 100000000;
}

#if LOG_LEVEL >= LOG_LEVEL_NOTICE
static const struct {
    unsigned int id;
    char *name;
} zynqmp_devices[] = {
    {
        .id = 0x10,
        .name = "3EG",
    },
    {
        .id = 0x11,
        .name = "2EG",
    },
    {
        .id = 0x20,
        .name = "5EV",
    },
    {
        .id = 0x21,
        .name = "4EV",
    },
    {
        .id = 0x30,
        .name = "7EV",
    },
    {
        .id = 0x38,
        .name = "9EG",
    },
    {
        .id = 0x39,
        .name = "6EG",
    },
    {
        .id = 0x40,
        .name = "11EG",
    },
    {
        .id = 0x50,
        .name = "15EG",
    },
    {
        .id = 0x58,
        .name = "19EG",
    },
    {
        .id = 0x59,
        .name = "17EG",
    },
};

static unsigned int zynqmp_get_silicon_id(void)
{
    uint32_t id;

    id = mmio_read_32(ZYNQMP_CSU_BASEADDR + ZYNQMP_CSU_IDCODE_OFFSET);

    id &= ZYNQMP_CSU_IDCODE_DEVICE_CODE_MASK | ZYNQMP_CSU_IDCODE_SVD_MASK;
    id >>= ZYNQMP_CSU_IDCODE_SVD_SHIFT;

    return id;
}

static char *zynqmp_get_silicon_idcode_name(void)
{
    unsigned int id;

    id = zynqmp_get_silicon_id();
    for (size_t i = 0; i < ARRAY_SIZE(zynqmp_devices); i++) {
        if (zynqmp_devices[i].id == id)
            return zynqmp_devices[i].name;
    }
    return "UNKN";
}

static unsigned int zynqmp_get_rtl_ver(void)
{
    uint32_t ver;

    ver = mmio_read_32(ZYNQMP_CSU_BASEADDR + ZYNQMP_CSU_VERSION_OFFSET);
    ver &= ZYNQMP_RTL_VER_MASK;
    ver >>= ZYNQMP_RTL_VER_SHIFT;

    return ver;
}

static char *zynqmp_print_silicon_idcode(void)
{
    uint32_t id, maskid, tmp;

    id = mmio_read_32(ZYNQMP_CSU_BASEADDR + ZYNQMP_CSU_IDCODE_OFFSET);

    tmp = id;
    tmp &= ZYNQMP_CSU_IDCODE_XILINX_ID_MASK |
           ZYNQMP_CSU_IDCODE_FAMILY_MASK;
    maskid = ZYNQMP_CSU_IDCODE_XILINX_ID << ZYNQMP_CSU_IDCODE_XILINX_ID_SHIFT |
         ZYNQMP_CSU_IDCODE_FAMILY << ZYNQMP_CSU_IDCODE_FAMILY_SHIFT;
    if (tmp != maskid) {
        ERROR("Incorrect XILINX IDCODE 0x%x, maskid 0x%x\n", id, maskid);
        return "UNKN";
    }
    VERBOSE("Xilinx IDCODE 0x%x\n", id);
    return zynqmp_get_silicon_idcode_name();
}

static unsigned int zynqmp_get_ps_ver(void)
{
    uint32_t ver = mmio_read_32(ZYNQMP_CSU_BASEADDR + ZYNQMP_CSU_VERSION_OFFSET);

    ver &= ZYNQMP_PS_VER_MASK;
    ver >>= ZYNQMP_PS_VER_SHIFT;

    return ver + 1;
}

static void zynqmp_print_platform_name(void)
{
    unsigned int ver = zynqmp_get_silicon_ver();
    unsigned int rtl = zynqmp_get_rtl_ver();
    char *label = "Unknown";

    switch (ver) {
    case ZYNQMP_CSU_VERSION_VELOCE:
        label = "VELOCE";
        break;
    case ZYNQMP_CSU_VERSION_EP108:
        label = "EP108";
        break;
    case ZYNQMP_CSU_VERSION_QEMU:
        label = "QEMU";
        break;
    case ZYNQMP_CSU_VERSION_SILICON:
        label = "silicon";
        break;
    }

    NOTICE("ATF running on XCZU%s/%s v%d/RTL%d.%d at 0x%x%s\n",
           zynqmp_print_silicon_idcode(), label, zynqmp_get_ps_ver(),
           (rtl & 0xf0) >> 4, rtl & 0xf, BL31_BASE,
           zynqmp_is_pmu_up() ? ", with PMU firmware" : "");
}
#else
static inline void zynqmp_print_platform_name(void) { }
#endif

/*
 * Indicator for PMUFW discovery:
 *   0 = No FW found
 *   non-zero = FW is present
 */
static int zynqmp_pmufw_present;

/*
 * zynqmp_discover_pmufw - Discover presence of PMUFW
 *
 * Discover the presence of PMUFW and store it for later run-time queries
 * through zynqmp_is_pmu_up.
 * NOTE: This discovery method is fragile and will break if:
 *  - setting FW_PRESENT is done by PMUFW itself and could be left out in PMUFW
 *    (be it by error or intentionally)
 *  - XPPU/XMPU may restrict ATF's access to the PMU address space
 */
static int zynqmp_discover_pmufw(void)
{
    zynqmp_pmufw_present = mmio_read_32(PMU_GLOBAL_CNTRL);
    zynqmp_pmufw_present &= PMU_GLOBAL_CNTRL_FW_IS_PRESENT;

    return !!zynqmp_pmufw_present;
}

/*
 * zynqmp_is_pmu_up - Find if PMU firmware is up and running
 *
 * Return 0 if firmware is not available, non 0 otherwise
 */
int zynqmp_is_pmu_up(void)
{
    return zynqmp_pmufw_present;
}

unsigned int zynqmp_get_bootmode(void)
{
    uint32_t r = mmio_read_32(CRL_APB_BOOT_MODE_USER);

    return r & CRL_APB_BOOT_MODE_MASK;
}

void zynqmp_config_setup(void)
{
    zynqmp_discover_pmufw();
    zynqmp_print_platform_name();
    generic_delay_timer_init();
}

unsigned int plat_get_syscnt_freq2(void)
{
    unsigned int ver = zynqmp_get_silicon_ver();

    switch (ver) {
    case ZYNQMP_CSU_VERSION_VELOCE:
        return 10000;
    case ZYNQMP_CSU_VERSION_EP108:
        return 4000000;
    case ZYNQMP_CSU_VERSION_QEMU:
        return 50000000;
    }

    return mmio_read_32(IOU_SCNTRS_BASEFREQ);
}
