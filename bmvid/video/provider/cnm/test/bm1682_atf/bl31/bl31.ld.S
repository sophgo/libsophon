/*
 * Copyright (c) 2013-2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

OUTPUT_FORMAT(PLATFORM_LINKER_FORMAT)
OUTPUT_ARCH(PLATFORM_LINKER_ARCH)
ENTRY(bl31_entrypoint)


MEMORY {
    RAM (rwx): ORIGIN = BL31_BASE, LENGTH = BL31_LIMIT - BL31_BASE
}

#ifdef PLAT_EXTRA_LD_SCRIPT
#include <plat.ld.S>
#endif

SECTIONS
{
    . = BL31_BASE;
    ASSERT(. == ALIGN(4096),
           "BL31_BASE address is not aligned on a page boundary.")

#if SEPARATE_CODE_AND_RODATA
    .text . : {
        __TEXT_START__ = .;
        *bl31_entrypoint.o(.text*)
        *(.text*)
        *(.vectors)
        . = NEXT(4096);
        __TEXT_END__ = .;
    } >RAM

    .rodata . : {
        __RODATA_START__ = .;
        *(.rodata*)

        /* Ensure 8-byte alignment for descriptors and ensure inclusion */
        . = ALIGN(8);
        __RT_SVC_DESCS_START__ = .;
        KEEP(*(rt_svc_descs))
        __RT_SVC_DESCS_END__ = .;

#if ENABLE_PMF
        /* Ensure 8-byte alignment for descriptors and ensure inclusion */
        . = ALIGN(8);
        __PMF_SVC_DESCS_START__ = .;
        KEEP(*(pmf_svc_descs))
        __PMF_SVC_DESCS_END__ = .;
#endif /* ENABLE_PMF */

        /*
         * Ensure 8-byte alignment for cpu_ops so that its fields are also
         * aligned. Also ensure cpu_ops inclusion.
         */
        . = ALIGN(8);
        __CPU_OPS_START__ = .;
        KEEP(*(cpu_ops))
        __CPU_OPS_END__ = .;

        . = NEXT(4096);
        __RODATA_END__ = .;
    } >RAM
#else
    ro . : {
        __RO_START__ = .;
        *bl31_entrypoint.o(.text*)
        *(.text*)
        *(.rodata*)

        /* Ensure 8-byte alignment for descriptors and ensure inclusion */
        . = ALIGN(8);
        __RT_SVC_DESCS_START__ = .;
        KEEP(*(rt_svc_descs))
        __RT_SVC_DESCS_END__ = .;

#if ENABLE_PMF
        /* Ensure 8-byte alignment for descriptors and ensure inclusion */
        . = ALIGN(8);
        __PMF_SVC_DESCS_START__ = .;
        KEEP(*(pmf_svc_descs))
        __PMF_SVC_DESCS_END__ = .;
#endif /* ENABLE_PMF */

        /*
         * Ensure 8-byte alignment for cpu_ops so that its fields are also
         * aligned. Also ensure cpu_ops inclusion.
         */
        . = ALIGN(8);
        __CPU_OPS_START__ = .;
        KEEP(*(cpu_ops))
        __CPU_OPS_END__ = .;

        *(.vectors)
        __RO_END_UNALIGNED__ = .;
        /*
         * Memory page(s) mapped to this section will be marked as read-only,
         * executable.  No RW data from the next section must creep in.
         * Ensure the rest of the current memory page is unused.
         */
        . = NEXT(4096);
        __RO_END__ = .;
    } >RAM
#endif

    ASSERT(__CPU_OPS_END__ > __CPU_OPS_START__,
           "cpu_ops not defined for this platform.")

    /*
     * Define a linker symbol to mark start of the RW memory area for this
     * image.
     */
    __RW_START__ = . ;

    /*
     * .data must be placed at a lower address than the stacks if the stack
     * protector is enabled. Alternatively, the .data.stack_protector_canary
     * section can be placed independently of the main .data section.
     */
   .data . : {
        __DATA_START__ = .;
        *(.data*)
        __DATA_END__ = .;
    } >RAM

#ifdef BL31_PROGBITS_LIMIT
    ASSERT(. <= BL31_PROGBITS_LIMIT, "BL31 progbits has exceeded its limit.")
#endif

    stacks (NOLOAD) : {
        __STACKS_START__ = .;
        *(tzfw_normal_stacks)
        __STACKS_END__ = .;
    } >RAM

    /*
     * The .bss section gets initialised to 0 at runtime.
     * Its base address should be 16-byte aligned for better performance of the
     * zero-initialization code.
     */
    .bss (NOLOAD) : ALIGN(16) {
        __BSS_START__ = .;
        *(.bss*)
        *(COMMON)
#if !USE_COHERENT_MEM
        /*
         * Bakery locks are stored in normal .bss memory
         *
         * Each lock's data is spread across multiple cache lines, one per CPU,
         * but multiple locks can share the same cache line.
         * The compiler will allocate enough memory for one CPU's bakery locks,
         * the remaining cache lines are allocated by the linker script
         */
        . = ALIGN(CACHE_WRITEBACK_GRANULE);
        __BAKERY_LOCK_START__ = .;
        *(bakery_lock)
        . = ALIGN(CACHE_WRITEBACK_GRANULE);
        __PERCPU_BAKERY_LOCK_SIZE__ = ABSOLUTE(. - __BAKERY_LOCK_START__);
        . = . + (__PERCPU_BAKERY_LOCK_SIZE__ * (PLATFORM_CORE_COUNT - 1));
        __BAKERY_LOCK_END__ = .;
#ifdef PLAT_PERCPU_BAKERY_LOCK_SIZE
    ASSERT(__PERCPU_BAKERY_LOCK_SIZE__ == PLAT_PERCPU_BAKERY_LOCK_SIZE,
        "PLAT_PERCPU_BAKERY_LOCK_SIZE does not match bakery lock requirements");
#endif
#endif

#if ENABLE_PMF
        /*
         * Time-stamps are stored in normal .bss memory
         *
         * The compiler will allocate enough memory for one CPU's time-stamps,
         * the remaining memory for other CPU's is allocated by the
         * linker script
         */
        . = ALIGN(CACHE_WRITEBACK_GRANULE);
        __PMF_TIMESTAMP_START__ = .;
        KEEP(*(pmf_timestamp_array))
        . = ALIGN(CACHE_WRITEBACK_GRANULE);
        __PMF_PERCPU_TIMESTAMP_END__ = .;
        __PERCPU_TIMESTAMP_SIZE__ = ABSOLUTE(. - __PMF_TIMESTAMP_START__);
        . = . + (__PERCPU_TIMESTAMP_SIZE__ * (PLATFORM_CORE_COUNT - 1));
        __PMF_TIMESTAMP_END__ = .;
#endif /* ENABLE_PMF */
        __BSS_END__ = .;
    } >RAM

    /*
     * The xlat_table section is for full, aligned page tables (4K).
     * Removing them from .bss avoids forcing 4K alignment on
     * the .bss section and eliminates the unecessary zero init
     */
    xlat_table (NOLOAD) : {
        *(xlat_table)
    } >RAM

#if USE_COHERENT_MEM
    /*
     * The base address of the coherent memory section must be page-aligned (4K)
     * to guarantee that the coherent data are stored on their own pages and
     * are not mixed with normal data.  This is required to set up the correct
     * memory attributes for the coherent data page tables.
     */
    coherent_ram (NOLOAD) : ALIGN(4096) {
        __COHERENT_RAM_START__ = .;
        /*
         * Bakery locks are stored in coherent memory
         *
         * Each lock's data is contiguous and fully allocated by the compiler
         */
        *(bakery_lock)
        *(tzfw_coherent_mem)
        __COHERENT_RAM_END_UNALIGNED__ = .;
        /*
         * Memory page(s) mapped to this section will be marked
         * as device memory.  No other unexpected data must creep in.
         * Ensure the rest of the current memory page is unused.
         */
        . = NEXT(4096);
        __COHERENT_RAM_END__ = .;
    } >RAM
#endif

    /*
     * Define a linker symbol to mark end of the RW memory area for this
     * image.
     */
    __RW_END__ = .;
    __BL31_END__ = .;

    __BSS_SIZE__ = SIZEOF(.bss);
#if USE_COHERENT_MEM
    __COHERENT_RAM_UNALIGNED_SIZE__ =
        __COHERENT_RAM_END_UNALIGNED__ - __COHERENT_RAM_START__;
#endif

#if RESET_TO_BL31
    __DATA_RAM_START__ = __DATA_START__;
    __DATA_SIZE__ = SIZEOF(.data);
    __DATA_ROM_START__ = LOADADDR(.data);
#endif

    ASSERT(. <= BL31_LIMIT, "BL31 image has exceeded its limit.")
}
