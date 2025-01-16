#include "bm_io.h"
#include <linux/math64.h>
#include "bm_common.h"

void bm_get_bar_offset(struct bm_bar_info *pbar_info, u32 address,
											 void __iomem **bar_vaddr, u32 *offset)
{
	/* Choose bar the address belongs to, and compute the offset on bar */
	int i = 0;

	for (; i < REG_COUNT; ++i) {
		if (address >= pbar_info->bar_dev_start[i] &&
				address < pbar_info->bar_dev_start[i] + pbar_info->bar_len[i]) {
			*bar_vaddr = pbar_info->bar_vaddr[i];
			*offset = address - pbar_info->bar_dev_start[i];
		} else {
			pr_err("%s invalid address = 0x%x\n", __func__, address);
		}
	}
}


void __iomem *bm_get_devmem_vaddr(struct bm_device_info *bmdi, u32 address)
{
	u32 offset = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, &bar_vaddr, &offset);
	return bar_vaddr + offset;
}

u32 bm_read32(struct bm_device_info *bmdi, u32 address)
{
	u32 offset = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, &bar_vaddr, &offset);
	return ioread32(bar_vaddr + offset);
}

u32 bm_write32(struct bm_device_info *bmdi, u32 address, u32 data)
{
	u32 offset = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, &bar_vaddr, &offset);
	iowrite32(data, bar_vaddr + offset);
	return 0;
}

u8 bm_read8(struct bm_device_info *bmdi, u32 address)
{
	return ioread8(bm_get_devmem_vaddr(bmdi, address));
}

u32 bm_write8(struct bm_device_info *bmdi, u32 address, u8 data)
{
	iowrite8(data, bm_get_devmem_vaddr(bmdi, address));
	return 0;
}

u16 bm_read16(struct bm_device_info *bmdi, u32 address)
{
	return ioread16(bm_get_devmem_vaddr(bmdi, address));
}

u32 bm_write16(struct bm_device_info *bmdi, u32 address, u16 data)
{
	iowrite16(data, bm_get_devmem_vaddr(bmdi, address));
	return 0;
}

u64 bm_read64(struct bm_device_info *bmdi, u32 address)
{
	u64 temp = 0;

	temp = bm_read32(bmdi, address + 4);
	temp = (u64)bm_read32(bmdi, address) | (temp << 32);
	return temp;
}

u64 bm_write64(struct bm_device_info *bmdi, u32 address, u64 data)
{
	bm_write32(bmdi, address, data & 0xFFFFFFFF);
	bm_write32(bmdi, address + 4, data >> 32);
	return 0;
}

void bm_reg_init_vaddr(struct bm_device_info *bmdi, u32 address, void __iomem **reg_base_vaddr)
{
	u32 offset = 0;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	if (address == 0)
	{
		*reg_base_vaddr = NULL;
		return;
	}
	bm_get_bar_offset(pbar_info, address, reg_base_vaddr, &offset);
	*reg_base_vaddr += offset;
	PR_TRACE("device address = 0x%x, vaddr = 0x%p\n", address, *reg_base_vaddr);
}

/* Define register operation as a generic marco. it provide
 * xxx_reg_read/write wrapper, currently support read/write u32,
 * using bm_read/write APIs directly for access u16/u8.
 */

void dev_info_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int len)
{
	int i, j;

	char *p = (u8 *)&val;

	for (i = len - 1, j = 0; i >= 0; i--)
		iowrite8(*(p + i), bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + reg_offset + j++);
}

u8 dev_info_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread8(bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + reg_offset);
}

void dev_info_write_byte(struct bm_device_info *bmdi, u8 reg_offset, u8 val)
{
	iowrite8(val, bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + reg_offset);
}

u8 dev_info_read_byte(struct bm_device_info *bmdi, u8 reg_offset)
{
	return ioread8(bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + reg_offset);
}



void top_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr + reg_offset);
}

u32 top_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr + reg_offset);
}

void gp_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr + reg_offset);
}

u32 gp_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr + reg_offset);
}


void pwm_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr + reg_offset);
}
u32 otp_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.otp_bar_vaddr + reg_offset);
}
u32 pwm_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr + reg_offset);
}


void ddr_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr + reg_offset);
}

u32 ddr_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr + reg_offset);
}

void bdc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr + reg_offset);
}

u32 bdc_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr + reg_offset);
}

void smmu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr + reg_offset);
}

u32 smmu_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr + reg_offset);
}

void cfg_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.cfg_bar_vaddr + reg_offset);
}

u32 cfg_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.cfg_bar_vaddr + reg_offset);
}
void nv_timer_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr + reg_offset);
}

u32 nv_timer_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr + reg_offset);
}

void tpu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	tpu_reg_write_idx(bmdi, reg_offset, val, 0);
}

u32 tpu_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return tpu_reg_read_idx(bmdi, reg_offset, 0);
}

void tpu_reg_write_idx(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int core_id)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr + reg_offset + core_id * BD_ENGINE_TPU1_OFFSET);
}

u32 tpu_reg_read_idx(struct bm_device_info *bmdi, u32 reg_offset, int core_id)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr + reg_offset + core_id * BD_ENGINE_TPU1_OFFSET);
}

void gdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	gdma_reg_write_idx(bmdi, reg_offset, val, 0);
}

u32 gdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return gdma_reg_read_idx(bmdi, reg_offset, 0);
}

void gdma_reg_write_idx(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int core_id)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr + reg_offset + core_id * GDMA_ENGINE_TPU1_OFFSET);
}

u32 gdma_reg_read_idx(struct bm_device_info *bmdi, u32 reg_offset, int core_id)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr + reg_offset + core_id * GDMA_ENGINE_TPU1_OFFSET);
}

void spacc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.spacc_bar_vaddr + reg_offset);
}

u32 spacc_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.spacc_bar_vaddr + reg_offset);
}

void pka_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.pka_bar_vaddr + reg_offset);
}

u32 pka_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.pka_bar_vaddr + reg_offset);
}


void io_init(struct bm_device_info *bmdi)
{
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->smem_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.smem_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->lmem_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.lmem_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->bdc_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr);
	// bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gp_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr);
}

int bm_get_reg(struct bm_device_info *bmdi, struct bm_reg *reg)
{
	reg->reg_value = bm_read32(bmdi, reg->reg_addr);
	return 0;
}

int bm_set_reg(struct bm_device_info *bmdi, struct bm_reg *reg)
{
#ifdef PR_DEBUG
	bm_write32(bmdi, reg->reg_addr, reg->reg_value);
#endif
	return 0;
}
