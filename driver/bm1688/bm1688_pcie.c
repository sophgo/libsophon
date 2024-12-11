#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/delay.h>
#include "bm1688_reg.h"
#include "bm_io.h"
#include "bm_pcie.h"
#include "bm_common.h"
#include "bm_card.h"

static const struct bm_bar_info bm1688_pcie2_bar_layout[] = {
	{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x20400000,  //PCIE2 base addr

		.bar1_len = 0x800000,
		.bar1_dev_start = 0x00000000,
		.bar1_part_info[0]  = {0x000000, 0x010000, 0x28100000},
		.bar1_part_info[1]  = {0x010000, 0x0A0000, 0x27010000},
		.bar1_part_info[2]  = {0x0B0000, 0x030000, 0x36000000},
		.bar1_part_info[3]  = {0x0E0000, 0x021000, 0x20BC0000},
		.bar1_part_info[4]  = {0x101000, 0x001000, 0x20BE2000},
		.bar1_part_info[5]  = {0x102000, 0x001000, 0x20000000},
		.bar1_part_info[6]  = {0x103000, 0x001000, 0x20010000},
		.bar1_part_info[7]  = {0x104000, 0x001000, 0x20BE3000},
		.bar1_part_info[8]  = {0x105000, 0x001000, 0x20020000},
		.bar1_part_info[9]  = {0x106000, 0x400000, 0x20800000},
		.bar1_part_info[10] = {0x506000, 0x044000, 0x270D0000},

		.bar2_len = 0x400000,
		.bar2_dev_start = 0,
		.bar2_part_info[0] = {0x0, 0x0, 0x0},

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x80100000,
	},
	{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x20400000,  //PCIE2 base addr

		.bar1_len = 0x800000,
		.bar1_dev_start = 0x00000000,
		.bar1_part_info[0]  = {0x000000, 0x010000, 0x28100000},
		.bar1_part_info[1]  = {0x010000, 0x0A0000, 0x27010000},
		.bar1_part_info[2]  = {0x0B0000, 0x030000, 0x36000000},
		.bar1_part_info[3]  = {0x0E0000, 0x021000, 0x20BC0000},
		.bar1_part_info[4]  = {0x101000, 0x001000, 0x20BE2000},
		.bar1_part_info[5]  = {0x102000, 0x001000, 0x20000000},
		.bar1_part_info[6]  = {0x103000, 0x001000, 0x20010000},
		.bar1_part_info[7]  = {0x104000, 0x044000, 0x270D0000},
		.bar1_part_info[8]  = {0x148000, 0x0A0000, 0x29000000},
		.bar1_part_info[9]  = {0x1E8000, 0x190000, 0x05400000},
		.bar1_part_info[10] = {0x378000, 0x060000, 0x29300000},
		.bar1_part_info[11] = {0x3D8000, 0x060000, 0x21000000},
		.bar1_part_info[12] = {0x438000, 0x040000, 0x23000000},
		.bar1_part_info[13] = {0x478000, 0x040000, 0x24000000},
		.bar1_part_info[14] = {0x4B8000, 0x040000, 0x67000000},
		.bar1_part_info[15] = {0x4F8000, 0x111000, 0x68000000},
		.bar1_part_info[16] = {0x609000, 0x061000, 0x26000000},
		.bar1_part_info[17] = {0x66A000, 0x004000, 0x6FFF0000},
		.bar1_part_info[18] = {0x66E000, 0x080000, 0x70000000},
		.bar1_part_info[19] = {0x6EE000, 0x080000, 0x78000000},
		.bar1_part_info[20] = {0x76E000, 0x080000, 0x2580C000},
		.bar1_part_info[21] = {0x7EE000, 0x002000, 0x20BE8000},
		.bar1_part_info[22] = {0x7F0000, 0x001000, 0xFFFFE000},
		.bar1_part_info[23] = {0x7F1000, 0x001000, 0x05026000},

		.bar2_len = 0x400000,
		.bar2_dev_start = 0,
		.bar2_part_info[0] = {0x0, 0x100000, 0x20800000},

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x25000000,
	}
};

static const struct bm_bar_info bm1688_pcie4_bar_layout[] = {
	{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x20800000,  //PCIE4 base addr

		.bar1_len = 0x800000,
		.bar1_dev_start = 0x00000000,
		.bar1_part_info[0]  = {0x000000, 0x010000, 0x28100000},
		.bar1_part_info[1]  = {0x010000, 0x0A0000, 0x27010000},
		.bar1_part_info[2]  = {0x0B0000, 0x030000, 0x36000000},
		.bar1_part_info[3]  = {0x0E0000, 0x021000, 0x20BC0000},
		.bar1_part_info[4]  = {0x101000, 0x001000, 0x20BE3000},
		.bar1_part_info[5]  = {0x102000, 0x001000, 0x20000000},
		.bar1_part_info[6]  = {0x103000, 0x001000, 0x20020000},
		.bar1_part_info[7]  = {0x104000, 0x001000, 0x20BE2000},
		.bar1_part_info[8]  = {0x105000, 0x001000, 0x20010000},
		.bar1_part_info[9]  = {0x106000, 0x400000, 0x20400000},
		.bar1_part_info[10] = {0x506000, 0x044000, 0x270D0000},

		.bar2_len = 0x400000,
		.bar2_dev_start = 0,
		.bar2_part_info[0] = {0x0, 0x0, 0x0},

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x80100000,
	},
	{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x20800000,  //PCIE4 base addr

		.bar1_len = 0x800000,
		.bar1_dev_start = 0x00000000,
		.bar1_part_info[0]  = {0x000000, 0x010000, 0x28100000},
		.bar1_part_info[1]  = {0x010000, 0x0A0000, 0x27010000},
		.bar1_part_info[2]  = {0x0B0000, 0x030000, 0x36000000},
		.bar1_part_info[3]  = {0x0E0000, 0x021000, 0x20BC0000},
		.bar1_part_info[4]  = {0x101000, 0x001000, 0x20BE3000},
		.bar1_part_info[5]  = {0x102000, 0x001000, 0x20000000},
		.bar1_part_info[6]  = {0x103000, 0x001000, 0x20020000},
		.bar1_part_info[7]  = {0x104000, 0x044000, 0x270D0000},
		.bar1_part_info[8]  = {0x148000, 0x0A0000, 0x29000000},
		.bar1_part_info[9]  = {0x1E8000, 0x190000, 0x05400000},
		.bar1_part_info[10] = {0x378000, 0x060000, 0x29300000},
		.bar1_part_info[11] = {0x3D8000, 0x060000, 0x21000000},
		.bar1_part_info[12] = {0x438000, 0x040000, 0x23000000},
		.bar1_part_info[13] = {0x478000, 0x040000, 0x24000000},
		.bar1_part_info[14] = {0x4B8000, 0x040000, 0x67000000},
		.bar1_part_info[15] = {0x4F8000, 0x111000, 0x68000000},
		.bar1_part_info[16] = {0x609000, 0x061000, 0x26000000},
		.bar1_part_info[17] = {0x66A000, 0x004000, 0x6FFF0000},
		.bar1_part_info[18] = {0x66E000, 0x080000, 0x70000000},
		.bar1_part_info[19] = {0x6EE000, 0x080000, 0x78000000},
		.bar1_part_info[20] = {0x76E000, 0x080000, 0x2580C000},
		.bar1_part_info[21] = {0x7EE000, 0x002000, 0x20BE8000},
		.bar1_part_info[22] = {0x7F0000, 0x001000, 0xFFFFF000},
		.bar1_part_info[23] = {0x7F1000, 0x001000, 0x05026000},

		.bar2_len = 0x400000,
		.bar2_dev_start = 0,
		.bar2_part_info[0] = {0x0, 0x100000, 0x20400000},

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x80100000,
	},
};

void bm1688_pcie_get_outbound_base(struct bm_device_info *bmdi)
{
	struct bm_card *bmcd = NULL;
	u32 mode;
	if(bmdi->cinfo.pcie_func_index > 0) {
		bmcd = bmdrv_card_get_bm_card(bmdi);
		mode = bmcd->card_bmdi[0]->cinfo.mode;
	} else {
		mode = bmdi->cinfo.mode;
	}

	bmdi->cinfo.ob_base = 0x800;
}

void bm1688_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
	struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
	void __iomem *cfg_base_addr = bari->bar0_vaddr;
	void __iomem *atu_base_addr;
	u64 base_addr = 0;
	u64 bar1_start = 0;
	u64 bar2_start = 0;
	u64 bar4_start = 0;
	int function_num = 0;
	u64 size = 0x0;
	u32 pcie_timeout_config = 0x0;
	u32 mask = 0;
	u16 index;

	if (REG_READ32(cfg_base_addr, 0x14) == 0xffffffff) {
		pr_info("pcie link may error\n");
		return;
	}

	if (bmdi->cinfo.pcie_func_index > 0) {
		function_num = bmdi->cinfo.pcie_func_index;
		mask = 0x1 << 12;
	} else {
		function_num = (pdev->devfn & 0x7);
	}

	if (function_num == 0x0) {
		bar1_start = REG_READ32(cfg_base_addr, 0x14) & ~0xf;
		bar2_start = (REG_READ32(cfg_base_addr, 0x18) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x1c)) << 32);
		bar4_start = (REG_READ32(cfg_base_addr, 0x20) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x24)) << 32);
		size = 0xffffff;

	} else {
		bar1_start = 0x89000000;
		bar2_start = (u64)(0x0100000000000);
		bar4_start = 0x8b000000;
		size = 0x4ffffffff;
	}

	REG_WRITE32(bari->bar0_vaddr, 0x8bc, (REG_READ32(bari->bar0_vaddr, 0x8bc) | 0x1) );
	pcie_timeout_config = REG_READ32(bari->bar0_vaddr, 0x98);
	pcie_timeout_config &= ~0xf;
	pcie_timeout_config |= 0xa;
	REG_WRITE32(bari->bar0_vaddr, 0x98, pcie_timeout_config);
	REG_WRITE32(bari->bar0_vaddr, 0x8bc, (REG_READ32(bari->bar0_vaddr, 0x8bc) & (~0x1)));

	atu_base_addr = bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU;
	//ATU0 is configured at firmware, which configure as dbi_cfg_base //pcie_2:0x20400000 pcie_4:0x20800000

	//BAR1 ATU
	for (index=0; index<PCIE_BAR1_PART_MAX; index++) {
		base_addr = bar1_start + bari->bar1_part_info[index].offset;
		REG_WRITE32(atu_base_addr, 0x300 + index * 0x200, 0);
		REG_WRITE32(atu_base_addr, 0x304 + index * 0x200, 0x80000100);		//address match mode for bar1, BIT[10:8] are not checked in address match mode
		REG_WRITE32(atu_base_addr, 0x308 + index * 0x200, (u32)(base_addr & 0xffffffff));		//src addr
		REG_WRITE32(atu_base_addr, 0x30C + index * 0x200, base_addr >> 32);
		REG_WRITE32(atu_base_addr, 0x310 + index * 0x200, (u32)((base_addr & 0xffffffff) + bari->bar1_part_info[index].len -1));	//limit addr
		REG_WRITE32(atu_base_addr, 0x314 + index * 0x200, (u32)(bari->bar1_part_info[index].dev_start & 0xffffffff));		//dst addr
		REG_WRITE32(atu_base_addr, 0x318 + index * 0x200, bari->bar1_part_info[index].dev_start >> 32 | mask);
	}

	/*config bar2 for chip to chip transferr*/
	REG_WRITE32(atu_base_addr, 0x3500, 0x80000);
	REG_WRITE32(atu_base_addr, 0x3504, 0xc0080200);
	REG_WRITE32(atu_base_addr, 0x3508, 0);
	REG_WRITE32(atu_base_addr, 0x350C, 0);
	REG_WRITE32(atu_base_addr, 0x3510, 0);
	REG_WRITE32(atu_base_addr, 0x3514, 0x0);
	REG_WRITE32(atu_base_addr, 0x3518, mask);

	/* config bar4 to DDR phy*/
	//ATU4
	REG_WRITE32(atu_base_addr, 0x3700, 0);
	REG_WRITE32(atu_base_addr, 0x3704, 0x80000400);
	REG_WRITE32(atu_base_addr, 0x3708, (u32)(bar4_start & 0xffffffff));//src addr
	REG_WRITE32(atu_base_addr, 0x370C, bar4_start >> 32);
	REG_WRITE32(atu_base_addr, 0x3710, (u32)(bar4_start & 0xffffffff) + 0xFFFFF); //1M size
	REG_WRITE32(atu_base_addr, 0x3714, 0x25000000); 	//dst addr
	REG_WRITE32(atu_base_addr, 0x3718, mask);

	if (function_num == 0) {
		REG_WRITE32(atu_base_addr, 0x000, 0x2000);
		REG_WRITE32(atu_base_addr, 0x004, 0x80000000);
		REG_WRITE32(atu_base_addr, 0x008, 0); //src addr
		REG_WRITE32(atu_base_addr, 0x00C, bmdi->cinfo.ob_base);
		REG_WRITE32(atu_base_addr, 0x014, 0); //dst addr
		REG_WRITE32(atu_base_addr, 0x018, 0);
		REG_WRITE32(atu_base_addr, 0x010, 0xFFFFFFFF); //size
		REG_WRITE32(atu_base_addr, 0x020, bmdi->cinfo.ob_base + 0x3FF);
	}
}

void bm1688_unmap_bar(struct bm_bar_info *bari) {
	void __iomem *atu_base_addr;
	int i = 0;
	atu_base_addr = bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU;
	for (i = 0; i < 31; i++) {
		REG_WRITE32(atu_base_addr, 0x300 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x304 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x308 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x30C + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x310 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x314 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x318 + i*0x200, 0);
	}
}

void bm1688_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi)
{
	struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
	void __iomem *apb_base_addr;
	int max_payload = 0x0;
	int max_rd_req = 0x0;
	int mode = 0x0;
	int total_func_num = 0;
	int i = 0;
	int temp_value = 2;
	int temp_low = 2;
	struct bm_card *bmcd = NULL;

	if ((bmdi->cinfo.mode&0xc) == 0x8) {
		total_func_num = 0x2;
	} else {
		total_func_num = 0x1;
	}

	apb_base_addr = bari->bar1_vaddr + bari->bar1_part_info[4].offset;
	max_payload = REG_READ32(apb_base_addr, 0x44) & 0x3f;
	max_rd_req = (REG_READ32(apb_base_addr, 0x40) >> 16) & 0x3f;
	for (i = 0; i < total_func_num; i++) {
		temp_value = max_rd_req >> (i*0x3);
		temp_value &= 0x7;
		if (temp_value < temp_low)
			temp_low = temp_value;
		temp_value = max_payload >> (i*0x3);
		temp_value &= 0x7;
		if (temp_value < temp_low)
			temp_low = temp_value;
	}

	bmcd = bmdrv_card_get_bm_card(bmdi);
	if (bmcd != NULL) {
		temp_value = bmcd->cdma_max_payload & 0x7;
		if (temp_value < temp_low)
			temp_low =temp_value;
	}

	bmdi->memcpy_info.cdma_max_payload = temp_low | (temp_low << 3);
	/*this value will be updated to bmcd by calling the bmdrv_card_init in the probe sequence */

	pr_info("max_payload = 0x%x, max_rd_req = 0x%x, mode = 0x%x, total_func_num = 0x%x, max_paload = 0x%x \n",
		max_payload, max_rd_req, mode, total_func_num, bmdi->memcpy_info.cdma_max_payload);
}

/* Setup the right bar layout based on bar len,
 * return 0 if find, else not find.
 */
int bm1688_setup_bar_dev_layout(struct bm_device_info *bmdi, BAR_LAYOUT_TYPE type)
{
	struct bm_bar_info *bar_info = &bmdi->cinfo.bar_info;
	void __iomem *atu_base_addr = bar_info->bar0_vaddr + BM1688_OFFSET_PCIE_iATU;
	const struct bm_bar_info *bar_layout = NULL;
	int index;

	bar_info->bar0_dev_start = REG_READ32(atu_base_addr, 0x114);
	if (bar_info->bar0_dev_start == 0x20400000)
		bar_layout = &bm1688_pcie2_bar_layout[type];
	else
		bar_layout = &bm1688_pcie4_bar_layout[type];

	if (bar_layout->bar1_len == bar_info->bar1_len &&
			bar_layout->bar2_len == bar_info->bar2_len) {

		bar_info->bar1_dev_start = bar_layout->bar1_dev_start;
		for(index=0; index<PCIE_BAR1_PART_MAX; index++) {
			bar_info->bar1_part_info[index].dev_start = bar_layout->bar1_part_info[index].dev_start;
			bar_info->bar1_part_info[index].offset = bar_layout->bar1_part_info[index].offset;
			bar_info->bar1_part_info[index].len = bar_layout->bar1_part_info[index].len;
		}

		bar_info->bar2_dev_start = bar_layout->bar2_dev_start;
		for(index=0; index<PCIE_BAR2_PART_MAX; index++) {
			bar_info->bar2_part_info[index].dev_start = bar_layout->bar2_part_info[index].dev_start;
			bar_info->bar2_part_info[index].offset = bar_layout->bar2_part_info[index].offset;
			bar_info->bar2_part_info[index].len = bar_layout->bar2_part_info[index].len;
		}

		bar_info->bar4_dev_start = bar_layout->bar4_dev_start;
		bar_info->bar4_len = bar_layout->bar4_len;
		return 0;
	}
	/* Not find */
	return -1;
}

void bm1688_bmdrv_init_for_mode_chose(struct bm_device_info *bmdi, struct pci_dev *pdev, struct bm_bar_info *bari)
{
	struct bm_bar_info *bar_info = &bmdi->cinfo.bar_info;
	void __iomem *cfg_base_addr = bar_info->bar0_vaddr;
	void __iomem *atu_base_addr;
	int function_num = 0x0;
	u64 bar1_start = 0;
	u64 bar2_start = 0;
	u64 bar4_start = 0;
	u64 base_addr;
	u32 mask = 0;
	u16 index;

	if (REG_READ32(cfg_base_addr, 0x14) == 0xffffffff) {
		pr_info("pcie link may error for mode choose\n");
		return;
	}

	if (bmdi->cinfo.pcie_func_index > 0) {
		function_num = bmdi->cinfo.pcie_func_index;
		mask = 0x1 << 12;
	} else {
		function_num = (pdev->devfn & 0x7);
	}
	if (function_num == 0x0) {
		bar1_start = REG_READ32(cfg_base_addr, 0x14) & ~0xf;
		bar2_start = (REG_READ32(cfg_base_addr, 0x18) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x1c)) << 32);
		bar4_start = (REG_READ32(cfg_base_addr, 0x20) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x24)) << 32);
#ifdef __aarch64__
		bar1_start &= (u64)0xffffffff;
		bar2_start &= (u64)0xffffffff;
		bar4_start &= (u64)0xffffffff;
#endif
	} else {
		bar1_start = 0x89000000;
		bar2_start = (u64)(0x0100000000000) ;
		bar4_start = 0x8b000000;
	}

	bm1688_unmap_bar(bari);
	bmdi->cinfo.bmdrv_setup_bar_dev_layout(bmdi, MODE_CHOSE_LAYOUT);
	atu_base_addr = bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU;

	//BAR1 ATU
	for (index=0; index<11; index++) {
		base_addr = bar1_start + bari->bar1_part_info[index].offset;
		REG_WRITE32(atu_base_addr, 0x300 + index * 0x200, 0);
		REG_WRITE32(atu_base_addr, 0x304 + index * 0x200, 0x80000100);		//address match mode for bar1, BIT[10:8] are not checked in address match mode
		REG_WRITE32(atu_base_addr, 0x308 + index * 0x200, (u32)(base_addr & 0xffffffff));		//src addr
		REG_WRITE32(atu_base_addr, 0x30C + index * 0x200, base_addr >> 32);
		REG_WRITE32(atu_base_addr, 0x310 + index * 0x200, (u32)((base_addr & 0xffffffff) + bari->bar1_part_info[index].len -1));	//limit addr
		REG_WRITE32(atu_base_addr, 0x314 + index * 0x200, (u32)(bari->bar1_part_info[index].dev_start & 0xffffffff));		//dst addr
		REG_WRITE32(atu_base_addr, 0x318 + index * 0x200, bari->bar1_part_info[index].dev_start >> 32 | mask);
	}

	/* unused BAR2 ATU
	REG_WRITE32(atu_base_addr, 0x3500, 0);
	REG_WRITE32(atu_base_addr, 0x3504, 0x80000200);  //address match mode for bar2, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x3508, (u32)(bar2_start & 0xffffffff));//src addr
	REG_WRITE32(atu_base_addr, 0x350C, bar2_start >> 32);
	REG_WRITE32(atu_base_addr, 0x3510, (u32)(bar2_start & 0xffffffff) + 0xfffff); //size 1M
	REG_WRITE32(atu_base_addr, 0x3514, 0x20700000);      //dst addr - pcie2_iATU
	REG_WRITE32(atu_base_addr, 0x3518, mask);
	REG_WRITE32(atu_base_addr, 0x3520, bar2_start >> 32); */

	//BAR4 ATU
	REG_WRITE32(atu_base_addr, 0x3700, 0x0);
	REG_WRITE32(atu_base_addr, 0x3704, 0x80000400);  //address match mode for bar4, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x3708, (u32)(bar4_start & 0xffffffff));        //src addr
	REG_WRITE32(atu_base_addr, 0x370C, bar4_start >> 32);
	REG_WRITE32(atu_base_addr, 0x3710, (u32)(bar4_start & 0xffffffff) + 0xfffff);   //size 1M
	REG_WRITE32(atu_base_addr, 0x3714, 0x80100000);      //dst addr
	REG_WRITE32(atu_base_addr, 0x3718, mask);
	REG_WRITE32(atu_base_addr, 0x3720, bar4_start >> 32);
}

int bm1688_bmdrv_pcie_get_mode(struct bm_device_info *bmdi)
{
	int mode = 0x0;

	//get ssperi trap
	//mode = top_reg_read(bmdi, 0x04) >> 25;
	mode = bm_read32(bmdi, 0x28100004) >> 25;
	if (mode == 0xffffffff) {
		pr_err("pcie get mode fail\n");
		return -1;
	}
	bmdi->cinfo.mode = mode&0xf;
	pr_info("pcie mode 0x%x \n", bmdi->cinfo.mode);
	return 0;
}

void bm1688_bmdrv_pcie_set_function1_iatu_config(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	void __iomem *atu_base_addr;
	int value = 0x0;
	int function_num = 0x0;
	struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
	u32 pcie_timeout_config = 0x0;

	if (bmdi->cinfo.pcie_func_index > 0)
		function_num = bmdi->cinfo.pcie_func_index;
	else
		function_num = (pdev->devfn & 0x7);

	atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + BM1688_OFFSET_PCIE_iATU;

	REG_WRITE32(bari->bar0_vaddr, 0x8bc, (REG_READ32(bari->bar0_vaddr, 0x8bc) | 0x1) );
	pcie_timeout_config = REG_READ32(bari->bar0_vaddr, 0x98);
	pcie_timeout_config &= ~0xf;
	pcie_timeout_config |= 0xa;
	REG_WRITE32(bari->bar0_vaddr, 0x98, pcie_timeout_config);
	REG_WRITE32(bari->bar0_vaddr, 0x8bc, (REG_READ32(bari->bar0_vaddr, 0x8bc) & (~0x1)));

	REG_WRITE32(atu_base_addr, 0x3908, 0x0);
	REG_WRITE32(atu_base_addr, 0x390c, 0x0);
	REG_WRITE32(atu_base_addr, 0x3910, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x3914, 0x0);
	REG_WRITE32(atu_base_addr, 0x3918, 0x800);         //inbound tagrget address 0x800_0000_0000
	REG_WRITE32(atu_base_addr, 0x3900, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x3904, 0xc0080000); //bar match, match bar0

	REG_WRITE32(atu_base_addr, 0x3b08, 0x0);
	REG_WRITE32(atu_base_addr, 0x3b0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x3b10, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x3b14, 0x0);
	REG_WRITE32(atu_base_addr, 0x3b18, 0x900);         //inbound tagrget address 0x900_0000_0000
	REG_WRITE32(atu_base_addr, 0x3b00, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x3b04, 0xc0080100); //bar match, match bar1

	REG_WRITE32(atu_base_addr, 0x3d08, 0x0);
	REG_WRITE32(atu_base_addr, 0x3d0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x3d10, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x3d14, 0x0);
	REG_WRITE32(atu_base_addr, 0x3d18, 0x1 << 12);         //inbound tagrget address 0x1000_0000_0000
	REG_WRITE32(atu_base_addr, 0x3d00, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x3d04, 0xc0080200); //bar match, match bar2

	REG_WRITE32(atu_base_addr, 0x3f08, 0x0);
	REG_WRITE32(atu_base_addr, 0x3f0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x3f10, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x3f14, 0x0);
	REG_WRITE32(atu_base_addr, 0x3f18, 0xa00);       //inbound target address 0xa00_0000_0000
	REG_WRITE32(atu_base_addr, 0x3f00, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x3f04, 0xc0080400); //bar match, match bar4

	atu_base_addr = bari->bar1_vaddr + bari->bar1_part_info[9].offset + BM1688_OFFSET_PCIE_iATU;
	REG_WRITE32(atu_base_addr, 0x004, 0x80000000); //address match
	REG_WRITE32(atu_base_addr, 0x000, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x008, 0x0);
	REG_WRITE32(atu_base_addr, 0x00c, 0x800);       //outbound base address 800_0000_0000
	REG_WRITE32(atu_base_addr, 0x010, 0x400000 -1);  //size 4M
	REG_WRITE32(atu_base_addr, 0x020, 0x800);  //size 4M
	REG_WRITE32(atu_base_addr, 0x014, 0x88000000);
	REG_WRITE32(atu_base_addr, 0x018, 0x0);         //outbound target address 8800_0000

	REG_WRITE32(atu_base_addr, 0x200, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x204, 0x80000000); //address match
	REG_WRITE32(atu_base_addr, 0x208, 0x0);
	REG_WRITE32(atu_base_addr, 0x20c, 0x900);       //outbound base address 900_0000_0000
	REG_WRITE32(atu_base_addr, 0x210, 0x1000000 -1 );  //size 16M
	REG_WRITE32(atu_base_addr, 0x220, 0x900);  //size 4M
	REG_WRITE32(atu_base_addr, 0x214, 0x89000000);
	REG_WRITE32(atu_base_addr, 0x218, 0x0);         //outbound target address 8900_0000

	REG_WRITE32(atu_base_addr, 0x400, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x404, 0x80000000); //address match
	REG_WRITE32(atu_base_addr, 0x408, 0x0);
	REG_WRITE32(atu_base_addr, 0x40c, 0x1 << 12);       //outbound base address 1000_0000_0000
	REG_WRITE32(atu_base_addr, 0x410, 0xffffffff);
	REG_WRITE32(atu_base_addr, 0x420, 0x100f);  //size 1M
	REG_WRITE32(atu_base_addr, 0x414, 0x0);
	REG_WRITE32(atu_base_addr, 0x418, 0x1 << 12);         //outbound target address

	REG_WRITE32(atu_base_addr, 0x600, 0x2000);         //type 4'b0000, MRd/Mwr type
	REG_WRITE32(atu_base_addr, 0x604, 0x80000000); //address match
	REG_WRITE32(atu_base_addr, 0x608, 0x0);
	REG_WRITE32(atu_base_addr, 0x60c, 0xa00);       //outbound base address 9_0000_0000
	REG_WRITE32(atu_base_addr, 0x610, 0x100000 -1);  //size 1M
	REG_WRITE32(atu_base_addr, 0x620, 0xa00);  //size 1M
	REG_WRITE32(atu_base_addr, 0x614, 0x8b000000); //outbound target address 0x8b00_0000
	REG_WRITE32(atu_base_addr, 0x618, 0x0);

	REG_WRITE32(atu_base_addr, 0x808, 0x80000000); //out cfg
	REG_WRITE32(atu_base_addr, 0x80c, 0x0);
	REG_WRITE32(atu_base_addr, 0x810, 0x80300000 -1);
	REG_WRITE32(atu_base_addr, 0x814, 0x0);
	REG_WRITE32(atu_base_addr, 0x818, 0x0);
	REG_WRITE32(atu_base_addr, 0x800, 0x4);
	REG_WRITE32(atu_base_addr, 0x804, 0x90000000);

	//enable rc msi
	atu_base_addr = bari->bar1_vaddr + bari->bar1_part_info[9].offset;
	value = REG_READ32(atu_base_addr, 0x4);
	value |= 0x7;
	REG_WRITE32(atu_base_addr, 0x4, value);
	REG_READ32(atu_base_addr, 0x4);
	value = REG_READ32(atu_base_addr, 0x50);
	value |= (0x1 << 16);
	REG_WRITE32(atu_base_addr, 0x50, value);
	REG_READ32(atu_base_addr, 0x50);
}

uint32_t bm1688_bmdrv_get_chip_num(struct bm_device_info *bmdi)
{
	struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
	void __iomem *top_apb_addr = bari->bar1_vaddr + bari->bar1_part_info[3].offset + 0x20000;
	int value;

	value  = REG_READ32(top_apb_addr, 0x40);
	pr_info("chip_id 0x%x\n", value);
	return value & 0x1;
}

void bm1688_bmdrv_pcie_perst(struct bm_device_info *bmdi)
{
	int value = 0;

	value = bm_read32(bmdi, 0x27102004);
	value |= (0x1 << 21);
	bm_write32(bmdi, 0x27102004, value);

	value = bm_read32(bmdi, 0x27102000);
	pr_info("gpio 85 value = 0x%x\n", value);
	value |= (0x1 << 21);
	bm_write32(bmdi, 0x27102000, value);
	mdelay(300);
}

int bm1688_bmdrv_pcie_polling_rc_perst(struct pci_dev *pdev, struct bm_bar_info *bari)
{
	int loop = 200;
	int ret = 0;

	void __iomem *ssperi_base = bari->bar1_vaddr + bari->bar1_part_info[5].offset;
	// Wait for preset by other
	while (!((REG_READ32(ssperi_base, 0x54) & (1 << 0x1)))) { //bit0:pcie4 bit1:pcie2
		if (loop-- > 0) {
			msleep(1);
			pr_info("polling rc perst \n");
		}
		else {
			ret = -1;
			pr_info("polling rc perst time ount \n");
			break;
		}
	}

	return ret;
}

int bm1688_bmdrv_pcie_polling_rc_core_rst(struct pci_dev *pdev, struct bm_bar_info *bari)
{
	int loop = 200;
	int ret = 0;
	void __iomem *ssperi_base = bari->bar1_vaddr + bari->bar1_part_info[8].offset;
	//TODO Wait for preset by other
	while (!(REG_READ32(ssperi_base, 0x5c) & (1 << 8))) {
		if (loop-- > 0) {
			msleep(1000);
			pr_info("polling rc core rst \n");
		}
		else {
			ret = -1;
			pr_info("polling rc core time out\n");
			break;
		}
	}
	return ret;
}

void bm1688_bmdrv_pcie_set_rc_link_speed_gen_x(struct bm_bar_info *bari, int gen_speed)
{
	int value = 0;
	void __iomem *base = bari->bar1_vaddr + bari->bar1_part_info[9].offset;

	// enable DBI_RO_WR_EN
	REG_WRITE32(base, 0x8bc, REG_READ32(base, 0x8bc) | 0x1);

	// enable pcie link change speed
	REG_WRITE32(base, 0x80c, REG_READ32(base, 0x80c) | (0x1 << 17));
	//fast link config, for simulator
	//REG_WRITE32(base, 0x710, REG_READ32(base, 0x710) | (0x1 << 7));
	//value = REG_READ32(base, 0x718);
	//value &= ~(0x6<<28);
	//REG_WRITE32(base, 0x718, value);

	value = REG_READ32(base, 0x0a0);
	value &= ~0xf;
	value |= (0x3 & gen_speed);
	REG_WRITE32(base, 0x0a0, value); // cap_target_link_speed Gen3

	value = REG_READ32(base, 0x07c);
	value &= ~0xf;
	value |= (0x3 & gen_speed);
	REG_WRITE32(base, 0x07c, value); // cap_max_link_speed Gen3

	value = REG_READ32(base, 0x098);
	value &= ~(0xf);
	value |= (0xa);
	REG_WRITE32(base, 0x098, value);

	// disable DBI_RO_WR_EN
	REG_WRITE32(base, 0x8bc, REG_READ32(base, 0x8bc) & (~0x1));
}

void bm1688_bmdrv_pcie_set_rc_max_payload_setting(struct bm_bar_info *bari)
{
	int value = 0;
	void __iomem *base = bari->bar1_vaddr + bari->bar1_part_info[9].offset;

	// enable DBI_RO_WR_EN
	REG_WRITE32(base, 0x8bc, REG_READ32(base, 0x8bc) | 0x1);
	value = REG_READ32(base, 0x78);
	value &= ~(0x7 << 5);
	value |= (0x1 << 5);
	REG_WRITE32(base, 0x78, value);
	// disable DBI_RO_WR_EN
	REG_WRITE32(base, 0x8bc, REG_READ32(base, 0x8bc) & (~0x1));
}

void bm1688_bmdrv_pcie_enable_rc(struct bm_bar_info *bari)
{
	void __iomem *sii_base = bari->bar1_vaddr + bari->bar1_part_info[8].offset;
	REG_WRITE32(sii_base, 0x58, REG_READ32(sii_base, 0x58) | 0x1); // enable ltssm
}

int bm1688_bmdrv_pcie_polling_rc_link_state(struct bm_bar_info *bari)
{
	int value = 0x0;
	int count = 0x20;
	int ret = 0;
	void __iomem *sii_base = bari->bar1_vaddr + bari->bar1_part_info[8].offset;

	value = REG_READ32(sii_base, 0xb4);
	value = value >> 6;
	value &= 0x3;

	while(value != 0x3) {
		pr_info("rc link state: 0x%x\n", value);
		value = REG_READ32(sii_base, 0xb4);
		value = value >> 6;
		value &= 0x3;

		if (count-- > 0) {
			msleep(20);
			pr_info("wait link state count = %d\n", count);
		} else {
			ret = -1;
			pr_info("polling rc link time out\n");
			break;
		}
	}

	return ret;
}

int bm1688_try_to_link_as_gen1_speed(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari)
{
	int ret = 0;

	bm1688_bmdrv_pcie_perst(bmdi);
	bm1688_bmdrv_pcie_polling_rc_perst(pdev, bari);
	bm1688_bmdrv_pcie_polling_rc_core_rst(pdev, bari);
	bm1688_bmdrv_pcie_set_rc_link_speed_gen_x(bari, 0x1);
	bm1688_bmdrv_pcie_enable_rc(bari);

	if (bm1688_bmdrv_pcie_polling_rc_link_state(bari) < 0) {
		ret = -1;
		pr_info("bm1688_try_to_link_as_gen1_speed still fail \n");
	}

	return ret;
}

int bm1688_bmdrv_pcie_rc_init(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari)
{
	void __iomem *top_apb_base;
	void __iomem *ssperi_base;
	int count = 0x5;
	int ret = 0;
	int timeout = 100;
	ssperi_base = bari->bar1_vaddr + bari->bar1_part_info[8].offset;
	top_apb_base = bari->bar1_vaddr + bari->bar1_part_info[3].offset + 0x20000; //0x20be_0000 top_apb
	REG_WRITE32(top_apb_base, 0x4a4, REG_READ32(top_apb_base, 0x4a4) | 0x800);
	msleep(2);
	REG_WRITE32(top_apb_base, 0x4b8, REG_READ32(top_apb_base, 0x4b8) | 0xc00000);
	msleep(2);
	REG_WRITE32(top_apb_base, 0x2000, REG_READ32(top_apb_base, 0x2000) & 0xfffffffb);
	msleep(2);
	REG_WRITE32(top_apb_base, 0x2000, REG_READ32(top_apb_base, 0x2000) & 0xfffffffe);
	msleep(2);
	REG_WRITE32(top_apb_base, 0x2000, REG_READ32(top_apb_base, 0x2000) | 0x1);
	msleep(2);
	REG_WRITE32(ssperi_base, 0x50, (REG_READ32(ssperi_base, 0x50) & 0xffffe1ff) | (0x4 << 9));
	msleep(2);
	//remote config
	REG_WRITE32(top_apb_base, 0x0c, 0x80000000);
	msleep(2);
	REG_WRITE32(top_apb_base, 0x10, 0xbfffffff);
	msleep(2);
	REG_WRITE32(ssperi_base, 0x60, REG_READ32(ssperi_base, 0x60) | (0x1 << 20));
	// ???REG_WRITE32(top_apb_base, 0x4b4, REG_READ32(top_apb_base, 0x4b4) | 0x10);
	REG_WRITE32(top_apb_base, 0x2000, REG_READ32(top_apb_base, 0x2000) | 0x4);
	while (!(REG_READ32(top_apb_base, 0x4b8) & 0x800)) {
		if (timeout-- > 0) {
			mdelay(1);
		} else {
			pr_err("PCIE: sram init timeout\n");
			break;
		}
	}
	REG_WRITE32(top_apb_base, 0x4b4, REG_READ32(top_apb_base, 0x4b4) | (0x1 << 5));

retry:
	bm1688_bmdrv_pcie_perst(bmdi);
	bm1688_bmdrv_pcie_polling_rc_perst(pdev, bari);
	bm1688_bmdrv_pcie_polling_rc_core_rst(pdev, bari);
	bm1688_bmdrv_pcie_set_rc_link_speed_gen_x(bari, 0x3);
	bm1688_bmdrv_pcie_enable_rc(bari);

	if (bm1688_bmdrv_pcie_polling_rc_link_state(bari) < 0) {
		if (count-- > 0) {
			pr_info("rc link fail, retry %d\n", count);
			goto retry;
		} else {
			pr_info("rc link fail, retry still fail\n");
			ret = -1;
		}
	}

	if (ret < 0) {
		pr_info("try to link as gen1 speed \n");
		ret = bm1688_try_to_link_as_gen1_speed(pdev, bmdi, bari);
	}

	bm1688_bmdrv_pcie_set_rc_max_payload_setting(bari);

	return ret;
}

int bm1688_config_iatu_for_function_x(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari)
{
	int ret = 0;
	int max_function_num = 0x1;

	bmdi->cinfo.pcie_func_index = (pdev->devfn & 0x7);

	bm1688_bmdrv_init_for_mode_chose(bmdi, pdev, bari);

	ret = bm1688_bmdrv_pcie_get_mode(bmdi);
	if (ret < 0) {
		return -1;
	}

	bm1688_pcie_get_outbound_base(bmdi);
	if ((bmdi->cinfo.mode&0xc) == 0x8) {
		pr_info("dual mode chip_0\n");
	} else {
		return 0;
	}

	bm1688_bmdrv_pcie_rc_init(pdev, bmdi, bari);

	bm1688_bmdrv_pcie_set_function1_iatu_config(pdev, bmdi);
	return bm1688_bmdrv_pci_bus_scan(pdev, bmdi, max_function_num);
}

u32 bm1688_bmdrv_read_config(struct bm_device_info *bmdi, int cfg_base_addr, int offset)
{
	return bm_read32(bmdi, cfg_base_addr + offset);
}

void bm1688_bmdrv_write_config(struct bm_device_info *bmdi, int cfg_base_addr, int offset, u32 value, u32 mask)
{
	u32 val = 0;
	val = bm1688_bmdrv_read_config(bmdi, cfg_base_addr, offset);
	val = val | (value & mask);
	bm_write32(bmdi, cfg_base_addr + offset, val);
}

void bm1688_bmdrv_pci_busmaster_memory_enable(struct bm_device_info *bmdi, int cfg_base_addr, int offset)
{
	bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x4, 0x7, 0x7);
}

void bm1688_bmdrv_pci_msi_enable(struct bm_device_info *bmdi, int cfg_base_addr)
{
	bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x50, 0x1 << 16, 0x1 << 16);
}

void bm1688_bmdrv_pci_max_payload_setting(struct bm_device_info *bmdi, int cfg_base_addr)
{
	bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x78, 0x1 << 5, (0x7 << 5));
}

/*
*Cfg addrss translate
* Addr[27:20] = bus number
* Addr[19:15] = device number
* Addr[14:12] = function number
* Addr[11: 0] = register offset
*/
int bm1688_bmdrv_pci_bus_scan(struct pci_dev *pdev, struct bm_device_info *bmdi, int max_fun_num)
{
	int bus_num = 1;
	int cfg_base_addr = 0;
	int device_num = 0;
	int function_num = 0;
	int vendor_id   = 0;
	u32 count = 0;
	int root_function_num = 0x0;

	if (bmdi->cinfo.pcie_func_index > 0)
		root_function_num = bmdi->cinfo.pcie_func_index;
	else
		root_function_num = (pdev->devfn & 0x7);

	for (function_num = 0; function_num < max_fun_num; function_num++) {
		count = 600;
		cfg_base_addr = (bus_num << 20) | (device_num << 15) | (function_num << 12);
		cfg_base_addr += 0x80000000;
		while ((vendor_id != 0x1e30) && (vendor_id != 0x1f1c)) {
			vendor_id = bm1688_bmdrv_read_config(bmdi, cfg_base_addr, 0) & 0xffff;
			msleep(10);
			if (--count == 0) {
				pr_err("bus num = %d, device = %d, function = %d,   not find, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
				break;
			}
		}

		pr_info("bus num = %d, device = %d, function = %d, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
		msleep(2000);
		if((function_num == 0x0) && ((vendor_id == 0x1e30) || (vendor_id == 0x1f1c))) {
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x10, 0x88000000, 0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x14, 0x89000000, 0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x18, 0x0, 0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x1c, 0x1000, 0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x20, 0x8b000000, 0xfffffff0);
			bm1688_bmdrv_pci_busmaster_memory_enable(bmdi, cfg_base_addr, 0x4);
			bm1688_bmdrv_pci_msi_enable(bmdi, cfg_base_addr);
			bm1688_bmdrv_pci_max_payload_setting(bmdi, cfg_base_addr);
			pr_info("find 1 bus num = %d, device = %d, function = %d, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
			if (max_fun_num == 0x1)
				return 0;
		}
	}
	pr_err("not found: bus num = %d,device = %d, function = %d, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
	return -1;
}

void bm1688_pci_slider_bar4_config_device_addr(struct bm_bar_info *bari, u32 addr)
{
	void __iomem *atu_base_addr;
	u32 dst_addr = 0;
	u32 temp_addr = 0;
	atu_base_addr = bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU;
	dst_addr = REG_READ32(atu_base_addr, 0x3714); //dst addr

	if ((addr  > (dst_addr + 0xFFFFF)) || (addr < dst_addr) ) {
		temp_addr = addr & (~0xfffff);
		REG_WRITE32(atu_base_addr, 0x3714, temp_addr); //dst addr
		temp_addr = REG_READ32(atu_base_addr, 0x3714); //dst addr
	}
}
