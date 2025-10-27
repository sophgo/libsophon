#include "platform.h"

#ifdef PLATFORM_SOC
unsigned int *platform_ioremap(unsigned int addr, unsigned int size)
{
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
        return ioremap(addr, size);
    #else
        return ioremap_nocache(addr, size);
    #endif
}

void platform_iounmap(void *addr)
{
    iounmap(addr);
}

void platform_write_register(unsigned int phys_addr, unsigned int *virt_addr, unsigned int data)
{
    *(volatile unsigned int *)(virt_addr) = data;
}

unsigned int platform_read_register(unsigned int phys_addr, unsigned int *virt_addr)
{
    return *(volatile unsigned int *)(virt_addr);
}

void platform_enable_irq(int irq_num)
{
    enable_irq(irq_num);
}

void platform_disable_irq(int irq_num)
{
    disable_irq_nosync(irq_num);
}
#else //PLATFORM_PCIE

unsigned int *platform_ioremap(unsigned int addr, unsigned int size)
{
    return NULL;
}

void platform_iounmap(void *addr)
{

}

void platform_write_register(unsigned int phys_addr, unsigned int *virt_addr, unsigned int data)
{
    pcie_write_reg(phys_addr, data);
}

unsigned int platform_read_register(unsigned int phys_addr, unsigned int *virt_addr)
{
    return pcie_read_reg(phys_addr);
}

void platform_enable_irq(int irq_num)
{
    pcie_enable_irq(irq_num);
}

void platform_disable_irq(int irq_num)
{
    pcie_disable_irq(irq_num);
}
#endif