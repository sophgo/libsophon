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

typedef enum {
    MEMORY_MOVE,
    MEMORY_READ,
    MEMORY_WRITE,
} MEMORY_DIRECTION;

#ifdef PLATFORM_SOC
    #define platform_readl(phys_addr, virt_addr)            *(volatile unsigned int *)(virt_addr)
    #define platform_writel(phys_addr, virt_addr, data)     *(volatile unsigned int *)(virt_addr) = (unsigned int)data
#else //PLATFORM_PCIE
    extern void pcie_enable_irq(int irq_num);
    extern void pcie_disable_irq(int irq_num);
    extern unsigned int pcie_read_reg(unsigned int addr);
    extern unsigned int pcie_write_reg(unsigned int addr, unsigned int data);
    extern int pcie_memcpy_s2d(uint64_t dst, void *src, uint32_t size);
    extern int pcie_memcpy_d2s(void *dst, uint64_t src, uint32_t size);
    extern int pcie_memcpy_c2c(uint64_t dst, uint64_t src, uint32_t size);

    #define platform_readl(phys_addr, virt_addr)            pcie_read_reg(phys_addr)
    #define platform_writel(phys_addr, virt_addr, data)     pcie_write_reg(phys_addr, data)
#endif

unsigned int *platform_ioremap(unsigned int addr, unsigned int size);
void platform_iounmap(void *addr);

void platform_write_register(unsigned int phys_addr, unsigned int *virt_addr, unsigned int data);
unsigned int platform_read_register(unsigned int phys_addr, unsigned int *virt_addr);

void platform_enable_irq(int irq_num);
void platform_disable_irq(int irq_num);
#endif //__PLATFORM_H__
