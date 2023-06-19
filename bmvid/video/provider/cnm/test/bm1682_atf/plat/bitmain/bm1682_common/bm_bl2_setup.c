/*
 * Copyright (c) 2015-2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <arch_helpers.h>
#include <bl_common.h>
#include <console.h>
#include <debug.h>
#include <libfdt.h>
#include <platform_def.h>
#include <bm_private.h>
#include <string.h>
#include <utils.h>

/*
 * The next 2 constants identify the extents of the code & RO data region.
 * These addresses are used by the MMU setup code and therefore they must be
 * page-aligned.  It is the responsibility of the linker script to ensure that
 * __RO_START__ and __RO_END__ linker symbols refer to page-aligned addresses.
 */
#define BL2_RO_BASE (unsigned long)(&__RO_START__)
#define BL2_RO_LIMIT (unsigned long)(&__RO_END__)

/*******************************************************************************
 * This structure represents the superset of information that is passed to
 * BL3-1, e.g. while passing control to it from BL2, bl31_params
 * and other platform specific params
 ******************************************************************************/
typedef struct bl2_to_bl31_params_mem {
    bl31_params_t bl31_params;
    image_info_t bl31_image_info;
    image_info_t bl32_image_info;
    image_info_t bl33_image_info;
    entry_point_info_t bl33_ep_info;
    entry_point_info_t bl32_ep_info;
    entry_point_info_t bl31_ep_info;
} bl2_to_bl31_params_mem_t;


static bl2_to_bl31_params_mem_t bl31_params_mem;



/* Data structure which holds the extents of the trusted SRAM for BL2 */
static meminfo_t bl2_tzram_layout __aligned(CACHE_WRITEBACK_GRANULE);

meminfo_t *bl2_plat_sec_mem_layout(void)
{
    return &bl2_tzram_layout;
}

/*******************************************************************************
 * This function assigns a pointer to the memory that the platform has kept
 * aside to pass platform specific and trusted firmware related information
 * to BL31. This memory is allocated by allocating memory to
 * bl2_to_bl31_params_mem_t structure which is a superset of all the
 * structure whose information is passed to BL31
 * NOTE: This function should be called only once and should be done
 * before generating params to BL31
 ******************************************************************************/
bl31_params_t *bl2_plat_get_bl31_params(void)
{
    bl31_params_t *bl2_to_bl31_params;

    /*
     * Initialise the memory for all the arguments that needs to
     * be passed to BL3-1
     */
    zeromem(&bl31_params_mem, sizeof(bl2_to_bl31_params_mem_t));

    /* Assign memory for TF related information */
    bl2_to_bl31_params = &bl31_params_mem.bl31_params;
    SET_PARAM_HEAD(bl2_to_bl31_params, PARAM_BL31, VERSION_1, 0);

    /* Fill BL3-1 related information */
    bl2_to_bl31_params->bl31_image_info = &bl31_params_mem.bl31_image_info;
    SET_PARAM_HEAD(bl2_to_bl31_params->bl31_image_info, PARAM_IMAGE_BINARY,
        VERSION_1, 0);

    /* Fill BL3-2 related information */
    bl2_to_bl31_params->bl32_ep_info = &bl31_params_mem.bl32_ep_info;
    SET_PARAM_HEAD(bl2_to_bl31_params->bl32_ep_info, PARAM_EP,
        VERSION_1, 0);
    bl2_to_bl31_params->bl32_image_info = &bl31_params_mem.bl32_image_info;
    SET_PARAM_HEAD(bl2_to_bl31_params->bl32_image_info, PARAM_IMAGE_BINARY,
        VERSION_1, 0);

    /* Fill BL3-3 related information */
    bl2_to_bl31_params->bl33_ep_info = &bl31_params_mem.bl33_ep_info;
    SET_PARAM_HEAD(bl2_to_bl31_params->bl33_ep_info,
        PARAM_EP, VERSION_1, 0);

    /* BL3-3 expects to receive the primary CPU MPID (through x0) */
    bl2_to_bl31_params->bl33_ep_info->args.arg0 = 0xffff & read_mpidr();

    bl2_to_bl31_params->bl33_image_info = &bl31_params_mem.bl33_image_info;
    SET_PARAM_HEAD(bl2_to_bl31_params->bl33_image_info, PARAM_IMAGE_BINARY,
        VERSION_1, 0);

    return bl2_to_bl31_params;
}

/* Flush the TF params and the TF plat params */
void bl2_plat_flush_bl31_params(void)
{
    flush_dcache_range((unsigned long)&bl31_params_mem,
            sizeof(bl2_to_bl31_params_mem_t));
}

/*******************************************************************************
 * This function returns a pointer to the shared memory that the platform
 * has kept to point to entry point information of BL31 to BL2
 ******************************************************************************/
struct entry_point_info *bl2_plat_get_bl31_ep_info(void)
{
#if DEBUG
    bl31_params_mem.bl31_ep_info.args.arg1 = BM_BL31_PLAT_PARAM_VAL;
#endif

    return &bl31_params_mem.bl31_ep_info;
}



void bl2_early_platform_setup(meminfo_t *mem_layout)
{
    /* Initialize the console to provide early debug support */
    console_init(PLAT_BM_BOOT_UART_BASE, PLAT_BM_BOOT_UART_CLK_IN_HZ,
            PLAT_BM_CONSOLE_BAUDRATE);

    /* Setup the BL2 memory layout */
    bl2_tzram_layout = *mem_layout;

    plat_bm_io_setup();
}

static void security_setup(void)
{
    /*
     * This is where a TrustZone address space controller and other
     * security related peripherals, would be configured.
     */
}

void bl2_platform_setup(void)
{
    security_setup();

    /* TODO Initialize timer */
}

void bl2_plat_arch_setup(void)
{
    bm_configure_mmu_el1(bl2_tzram_layout.total_base,
                  bl2_tzram_layout.total_size,
                  BL2_RO_BASE, BL2_RO_LIMIT,
                  BL_COHERENT_RAM_BASE, BL_COHERENT_RAM_END);
}

/*******************************************************************************
 * Gets SPSR for BL33 entry
 ******************************************************************************/
static uint32_t bm_get_spsr_for_bl33_entry(void)
{
    unsigned int mode;
    uint32_t spsr;

    /* Figure out what mode we enter the non-secure world in */
    mode = EL_IMPLEMENTED(2) ? MODE_EL2 : MODE_EL1;

    /*
     * TODO: Consider the possibility of specifying the SPSR in
     * the FIP ToC and allowing the platform to have a say as
     * well.
     */
    spsr = SPSR_64(mode, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
    return spsr;
}

/*******************************************************************************
 * Before calling this function BL3-1 is loaded in memory and its entrypoint
 * is set by load_image. This is a placeholder for the platform to change
 * the entrypoint of BL3-1 and set SPSR and security state.
 * On ARM standard platforms we only set the security state of the entrypoint
 ******************************************************************************/
void bl2_plat_set_bl31_ep_info(image_info_t *bl31_image_info,
                    entry_point_info_t *bl31_ep_info)
{
    SET_SECURITY_STATE(bl31_ep_info->h.attr, SECURE);
    bl31_ep_info->spsr = SPSR_64(MODE_EL3, MODE_SP_ELX,
                    DISABLE_ALL_EXCEPTIONS);
}

/*******************************************************************************
 * Before calling this function BL3-3 is loaded in memory and its entrypoint
 * is set by load_image. This is a placeholder for the platform to change
 * the entrypoint of BL3-3 and set SPSR and security state.
 * On ARM standard platforms we only set the security state of the entrypoint
 ******************************************************************************/
void bl2_plat_set_bl33_ep_info(image_info_t *image,
                    entry_point_info_t *bl33_ep_info)
{

    SET_SECURITY_STATE(bl33_ep_info->h.attr, NON_SECURE);
    bl33_ep_info->spsr = bm_get_spsr_for_bl33_entry();
}

/*******************************************************************************
 * Populate the extents of memory available for loading BL33
 ******************************************************************************/
void bl2_plat_get_bl33_meminfo(meminfo_t *bl33_meminfo)
{
    bl33_meminfo->total_base = NS_DRAM0_BASE;
    bl33_meminfo->total_size = NS_DRAM0_SIZE;
    bl33_meminfo->free_base = NS_DRAM0_BASE;
    bl33_meminfo->free_size = NS_DRAM0_SIZE;
}

unsigned long plat_get_ns_image_entrypoint(void)
{
    return NS_IMAGE_OFFSET;
}
