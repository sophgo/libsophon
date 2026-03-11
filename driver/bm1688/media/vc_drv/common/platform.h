#ifndef __PLATFORM_H__
#define __PLATFORM_H__
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/version.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>

#include "vb.h"
#include "ion.h"

typedef enum {
    MEMORY_MOVE,
    MEMORY_READ,
    MEMORY_WRITE,
} MEMORY_DIRECTION;

#ifdef PLATFORM_SOC
    #define MAX_NUM_SOPHON_SOC                                      1
    #define platform_readl(soc_idx, phys_addr, virt_addr)           *(volatile unsigned int *)(virt_addr)
    #define platform_writel(soc_idx, phys_addr, virt_addr, data)    *(volatile unsigned int *)(virt_addr) = (unsigned int)data
#else //PLATFORM_PCIE
    extern void pcie_enable_irq(int soc_idx, int irq_num);
    extern void pcie_disable_irq(int soc_idx, int irq_num);
    extern unsigned int pcie_read_reg(int soc_idx, unsigned int addr);
    extern unsigned int pcie_write_reg(int soc_idx, unsigned int addr, unsigned int data);
    extern int pcie_memcpy_s2d(int soc_idx, uint64_t dst, void *src, uint32_t size);
    extern int pcie_memcpy_d2s(int soc_idx, void *dst, uint64_t src, uint32_t size);
    extern int pcie_memcpy_c2c(int soc_idx, uint64_t dst, uint64_t src, uint32_t size);

    #define MAX_NUM_SOPHON_SOC                                      8
    #define platform_readl(soc_idx, phys_addr, virt_addr)           pcie_read_reg(soc_idx, phys_addr)
    #define platform_writel(soc_idx, phys_addr, virt_addr, data)    pcie_write_reg(soc_idx, phys_addr, data)
#endif

unsigned int *platform_ioremap(unsigned int addr, unsigned int size);
void platform_iounmap(void *addr);

void platform_write_register(int core_idx, unsigned int phys_addr, unsigned int *virt_addr, unsigned int data);
unsigned int platform_read_register(int core_idx, unsigned int phys_addr, unsigned int *virt_addr);

void platform_enable_irq(int soc_idx, int irq_num);
void platform_disable_irq(int soc_idx, int irq_num);

vb_blk platform_vb_get_block_with_id(int soc_idx, vb_pool pool_id, uint32_t blk_size, mod_id_e mod_id);
vb_blk platform_vb_create_block(int soc_idx, uint64_t phy_addr, void *vir_addr, vb_pool pool_id, bool is_external);
vb_blk platform_vb_phys_addr2handle(int soc_idx, uint64_t phy_addr);
int32_t platform_base_ion_alloc(int soc_idx, uint64_t *p_paddr, void **pp_vaddr, uint8_t *buf_name, uint32_t buf_len, bool is_cached);
int32_t platform_base_ion_free(int soc_idx, uint64_t phy_addr);
int platform_get_soc_cnt(void);
void platform_set_soc_cnt(int soc_cnt);
#endif //__PLATFORM_H__


