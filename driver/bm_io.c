#include "bm_io.h"
#include "bm_common.h"
#include <linux/math64.h>

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

u32 top_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr + reg_offset);
}

u32 otp_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.otp_bar_vaddr + reg_offset);
}


void io_init(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bar_info.io_bar_vaddr.otp_bar_vaddr =
			ioremap(bmdi->cinfo.bm_reg->otp_bar_aaddr, OTP_SHADOW_SYS_SIZE);
}

