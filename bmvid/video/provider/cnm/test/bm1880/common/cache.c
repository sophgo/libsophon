#include <stdint.h>
#include "asm/system.h"

void __asm_flush_dcache_all(void);
void __asm_invalidate_dcache_all(void);
void __asm_flush_dcache_range(uint64_t start, uint64_t end);
void __asm_invalidate_dcache_range(uint64_t start, uint64_t end);
void __asm_invalidate_tlb_all(void);
void __asm_invalidate_icache_all(void);
int __asm_invalidate_l3_dcache(void);
int __asm_flush_l3_dcache(void);
int __asm_invalidate_l3_icache(void);
void __asm_switch_ttbr(uint64_t new_ttbr);

static inline unsigned int current_el(void)
{
    unsigned int el;
    asm volatile("mrs %0, CurrentEL" : "=r" (el) : : "cc");
    return el >> 2;
}

static inline unsigned int get_sctlr(void)
{
    unsigned int el, val;

    el = current_el();
    if (el == 1)
        asm volatile("mrs %0, sctlr_el1" : "=r" (val) : : "cc");
    else if (el == 2)
        asm volatile("mrs %0, sctlr_el2" : "=r" (val) : : "cc");
    else
        asm volatile("mrs %0, sctlr_el3" : "=r" (val) : : "cc");

    return val;
}

static inline void set_sctlr(unsigned int val)
{
    unsigned int el;

    el = current_el();
    if (el == 1)
        asm volatile("msr sctlr_el1, %0" : : "r" (val) : "cc");
    else if (el == 2)
        asm volatile("msr sctlr_el2, %0" : : "r" (val) : "cc");
    else
        asm volatile("msr sctlr_el3, %0" : : "r" (val) : "cc");

    asm volatile("isb");
}

static inline unsigned long read_mpidr(void)
{
    unsigned long val;

    asm volatile("mrs %0, mpidr_el1" : "=r" (val));

    return val;
}

/*
 * Performs a invalidation of the entire data cache at all levels
 */
void invalidate_dcache_all(void)
{
    __asm_invalidate_dcache_all();
    __asm_invalidate_l3_dcache();
}

/*
 * Performs a clean & invalidation of the entire data cache at all levels.
 * This function needs to be inline to avoid using stack.
 * __asm_flush_l3_dcache return status of timeout
 */
inline void flush_dcache_all(void)
{
    __asm_flush_dcache_all();
    __asm_flush_l3_dcache();
}

/*
 * Invalidates range in all levels of D-cache/unified cache
 */
void invalidate_dcache_range(unsigned long start, unsigned long size)
{
    __asm_invalidate_dcache_range(start, start + size);
}

/*
 * Flush range(clean & invalidate) from all levels of D-cache/unified cache
 */
void flush_dcache_range(unsigned long start, unsigned long size)
{
    __asm_flush_dcache_range(start, start + size);
}

void dcache_enable(void)
{
    /* The data cache is not active unless the mmu is enabled */
    if (!(get_sctlr() & CR_M)) {
        return;
    }

    set_sctlr(get_sctlr() | CR_C);
}

void dcache_disable(void)
{
    uint32_t sctlr;

    sctlr = get_sctlr();

    /* if cache isn't enabled no need to disable */
    if (!(sctlr & CR_C))
        return;

    set_sctlr(sctlr & ~(CR_C|CR_M));

    flush_dcache_all();
    __asm_invalidate_tlb_all();
}

int dcache_status(void)
{
    return (get_sctlr() & CR_C) != 0;
}

void invalidate_icache_all(void)
{
    __asm_invalidate_icache_all();
    __asm_invalidate_l3_icache();
}

void icache_enable(void)
{
    invalidate_icache_all();
    set_sctlr(get_sctlr() | CR_I);
}

void icache_disable(void)
{
    set_sctlr(get_sctlr() & ~CR_I);
}

int icache_status(void)
{
    return (get_sctlr() & CR_I) != 0;
}

