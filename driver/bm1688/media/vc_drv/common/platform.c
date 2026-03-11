#include "platform.h"

static int inserted_soc_cnt = 1;

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

void platform_write_register(int soc_idx, unsigned int phys_addr, unsigned int *virt_addr, unsigned int data)
{
    *(volatile unsigned int *)(virt_addr) = data;
}

unsigned int platform_read_register(int soc_idx, unsigned int phys_addr, unsigned int *virt_addr)
{
    return *(volatile unsigned int *)(virt_addr);
}

void platform_enable_irq(int soc_idx, int irq_num)
{
    enable_irq(irq_num);
}

void platform_disable_irq(int soc_idx, int irq_num)
{
    disable_irq_nosync(irq_num);
}

vb_blk platform_vb_get_block_with_id(int soc_idx, vb_pool pool_id, uint32_t blk_size, mod_id_e mod_id)
{
    return vb_get_block_with_id(pool_id, blk_size, mod_id);
}

vb_blk platform_vb_create_block(int soc_idx, uint64_t phy_addr, void *vir_addr, vb_pool pool_id, bool is_external)
{
    return vb_create_block(phy_addr, vir_addr, pool_id, is_external);
}

vb_blk platform_vb_phys_addr2handle(int soc_idx, uint64_t phy_addr)
{
    return vb_phys_addr2handle(phy_addr);
}

int32_t platform_base_ion_alloc(int soc_idx, uint64_t *p_paddr, void **pp_vaddr, uint8_t *buf_name, uint32_t buf_len, bool is_cached)
{
    return base_ion_alloc(p_paddr, pp_vaddr, buf_name, buf_len, is_cached);
}

int32_t platform_base_ion_free(int soc_idx, uint64_t phy_addr)
{
    return base_ion_free(phy_addr);
}
#else //PLATFORM_PCIE

unsigned int *platform_ioremap(unsigned int addr, unsigned int size)
{
    return NULL;
}

void platform_iounmap(void *addr)
{

}

void platform_write_register(int soc_idx, unsigned int phys_addr, unsigned int *virt_addr, unsigned int data)
{
    pcie_write_reg(soc_idx, phys_addr, data);
}

unsigned int platform_read_register(int soc_idx, unsigned int phys_addr, unsigned int *virt_addr)
{
    return pcie_read_reg(soc_idx, phys_addr);
}

void platform_enable_irq(int soc_idx, int irq_num)
{
    pcie_enable_irq(soc_idx, irq_num);
}

void platform_disable_irq(int soc_idx, int irq_num)
{
    pcie_disable_irq(soc_idx, irq_num);
}

vb_blk platform_vb_get_block_with_id(int soc_idx, vb_pool pool_id, uint32_t blk_size, mod_id_e mod_id)
{
    return vb_get_block_with_id(soc_idx, pool_id, blk_size, mod_id);
}

vb_blk platform_vb_create_block(int soc_idx, uint64_t phy_addr, void *vir_addr, vb_pool pool_id, bool is_external)
{
    return vb_create_block(soc_idx, phy_addr, vir_addr, pool_id, is_external);
}

vb_blk platform_vb_phys_addr2handle(int soc_idx, uint64_t phy_addr)
{
    return vb_phys_addr2handle(soc_idx, phy_addr);
}

int32_t platform_base_ion_alloc(int soc_idx, uint64_t *p_paddr, void **pp_vaddr, uint8_t *buf_name, uint32_t buf_len, bool is_cached)
{
    return base_ion_alloc(soc_idx, p_paddr, pp_vaddr, buf_name, buf_len, is_cached);
}

int32_t platform_base_ion_free(int soc_idx, uint64_t phy_addr)
{
    return base_ion_free(soc_idx, phy_addr);
}
#endif

int platform_get_soc_cnt(void)
{
    return inserted_soc_cnt;
}

void platform_set_soc_cnt(int soc_cnt)
{
    inserted_soc_cnt = soc_cnt;
}