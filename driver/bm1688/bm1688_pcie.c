#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/delay.h>
#include "bm1688_reg.h"
#include "bm_io.h"
#include "bm_pcie.h"
#include "bm_common.h"
#include "bm_card.h"

int bm1688_get_pcie_func_index(struct bm_device_info *bmdi)
{
	void __iomem *cfg_base_addr;
	//int mode = 0x0;
	int index = 0x0;

	cfg_base_addr = bmdi->cinfo.bar_info.bar0_vaddr;
	//mode = REG_READ32(cfg_base_addr, 0x718) & 0x3;
	//if (mode == 0x1)
	//	index = 1;
	//else
	//	index = 0;
	if (REG_READ32(cfg_base_addr + 0x100000, 0x1c) == 0x0)
		index = 0x0;
	else
		index = 0x1;

	//index = PCI_FUNC(bmdi->cinfo.pcidev->devfn);
	if ((bmdi->cinfo.chip_id == BM1688_PCIE_DEVICE_ID)) {
		bmdi->cinfo.pcie_func_index = index;
		pr_info("bm-sophon%d, pcie_func_index = %d\n", bmdi->dev_index, index);
	}

	return index;
}

void bm1688_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
	void __iomem *atu_base_addr;
	void __iomem *cfg_base_addr;
	u64 base_addr = 0;
	u64 bar1_start = 0;
	u64 bar2_start = 0;
	u64 bar4_start = 0;
	int function_num = 0;
	u64 size = 0x0;
	u32 pcie_timeout_config = 0x0;
	struct bm_bar_info *bari = &bmdi->cinfo.bar_info;

	cfg_base_addr = bmdi->cinfo.bar_info.bar0_vaddr; //pcie_1:0x20000000 pcie_2:0x20400000 pcie_4:20800000
	if (bmdi->cinfo.pcie_func_index > 0)
		function_num = bmdi->cinfo.pcie_func_index;
	else
		function_num = (pdev->devfn & 0x7);

	if (REG_READ32(cfg_base_addr, 0x14) == 0xffffffff) {
		pr_info("pcie link may error\n");
		return;
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
		size = 0xffffff;

	} else {
		bar1_start = 0x89000000;
		bar2_start = (u64)(0x0100000000000);
		bar4_start = 0x8b000000;
		size = 0x4ffffffff;
	}

	atu_base_addr = bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU; //pcie_1:0x20300000 pcie_2:0x20700000 pcie_4:0x20b00000
	REG_WRITE32(bari->bar0_vaddr, 0x8bc, (REG_READ32(bari->bar0_vaddr, 0x8bc) | 0x1) );
	pcie_timeout_config = REG_READ32(bari->bar0_vaddr, 0x98);
	pcie_timeout_config &= ~0xf;
	pcie_timeout_config |= 0xa;
	REG_WRITE32(bari->bar0_vaddr, 0x98, pcie_timeout_config);
	REG_WRITE32(bari->bar0_vaddr, 0x8bc, (REG_READ32(bari->bar0_vaddr, 0x8bc) & (~0x1)));

	//ATU0 is configured at firmware, which configure as dbi_cfg_base //pcie_2:0x20400000 pcie_4:0x20800000

	//ATU1
	base_addr = bar1_start + bari->bar1_part0_offset;
	REG_WRITE32(atu_base_addr, 0x300, 0);
	REG_WRITE32(atu_base_addr, 0x304, 0x80000100);		//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x308, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x30C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x310, (u32)((base_addr & 0xffffffff) + bari->bar1_part0_len -1));	//limit addr
	REG_WRITE32(atu_base_addr, 0x314, (u32)(bari->bar1_part0_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x318, bari->bar1_part0_dev_start >> 32);

	//ATU2
	base_addr = bar1_start + bari->bar1_part1_offset;
	REG_WRITE32(atu_base_addr, 0x500, 0);
	REG_WRITE32(atu_base_addr, 0x504, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x508, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x50C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x510,  (u32)((base_addr & 0xffffffff) + bari->bar1_part1_len -1));	//size 3M
	REG_WRITE32(atu_base_addr, 0x514, (u32)(bari->bar1_part1_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x518, bari->bar1_part1_dev_start >> 32);

	//ATU3
	base_addr = bar1_start + bari->bar1_part2_offset;
	REG_WRITE32(atu_base_addr, 0x700, 0);
	REG_WRITE32(atu_base_addr, 0x704, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x708, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x70C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x710, (u32)((base_addr & 0xffffffff) + bari->bar1_part2_len -1));	//size 64K
	REG_WRITE32(atu_base_addr, 0x714, (u32)(bari->bar1_part2_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x718, bari->bar1_part2_dev_start >> 32);

	//ATU5
	base_addr = bar1_start + bari->bar1_part3_offset;
	REG_WRITE32(atu_base_addr, 0xb00, 0);
	REG_WRITE32(atu_base_addr, 0xb04, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xb08, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0xb0C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0xb10,  (u32)((base_addr & 0xffffffff) + bari->bar1_part3_len -1));	//size 4K
	REG_WRITE32(atu_base_addr, 0xb14, (u32)(bari->bar1_part3_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0xb18, bari->bar1_part3_dev_start >> 32);

	//ATU6
	base_addr = bar1_start + bari->bar1_part4_offset;
	REG_WRITE32(atu_base_addr, 0xd00, 0);
	REG_WRITE32(atu_base_addr, 0xd04, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xd08, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0xd0C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0xd10,  (u32)((base_addr & 0xffffffff) + bari->bar1_part4_len -1));	//size 4K
	REG_WRITE32(atu_base_addr, 0xd14, (u32)(bari->bar1_part4_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0xd18, bari->bar1_part4_dev_start >> 32);

	//ATU7
	base_addr = bar1_start + bari->bar1_part5_offset;
	REG_WRITE32(atu_base_addr, 0xf00, 0);
	REG_WRITE32(atu_base_addr, 0xf04, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xf08, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0xf0C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0xf10, (u32)((base_addr & 0xffffffff) + bari->bar1_part5_len -1));	//size 4K
	REG_WRITE32(atu_base_addr, 0xf14, (u32)(bari->bar1_part5_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0xf18, bari->bar1_part5_dev_start >> 32);

	//ATU8
	base_addr = bar1_start + bari->bar1_part6_offset;
	REG_WRITE32(atu_base_addr, 0x1100, 0);
	REG_WRITE32(atu_base_addr, 0x1104, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x1108, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x110C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x1110, (u32)((base_addr & 0xffffffff) + bari->bar1_part6_len -1));	//size 4K
	REG_WRITE32(atu_base_addr, 0x1114, (u32)(bari->bar1_part6_dev_start & 0xffffffff));	//dst addr
	REG_WRITE32(atu_base_addr, 0x1118, bari->bar1_part6_dev_start >> 32);

	//ATU9
	base_addr = bar1_start + bari->bar1_part7_offset;
	REG_WRITE32(atu_base_addr, 0x1300, 0);
	REG_WRITE32(atu_base_addr, 0x1304, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x1308, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x130C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x1310, (u32)((base_addr & 0xffffffff) + bari->bar1_part7_len -1));	//size 4K
	REG_WRITE32(atu_base_addr, 0x1314, (u32)(bari->bar1_part7_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x1318, bari->bar1_part7_dev_start >> 32);;

	base_addr = bar1_start + bari->bar1_part8_offset;
	REG_WRITE32(atu_base_addr, 0x2f00, 0);
	REG_WRITE32(atu_base_addr, 0x2f04, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x2f08, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x2f0C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x2f10, (u32)((base_addr & 0xffffffff) + bari->bar1_part8_len -1)); //size 64K
	REG_WRITE32(atu_base_addr, 0x2f14, (u32)(bari->bar1_part8_dev_start & 0xffffffff));            //dst addr
	REG_WRITE32(atu_base_addr, 0x2f18, bari->bar1_part8_dev_start >> 32);

	//ATU10
	base_addr = bar1_start + bari->bar1_part9_offset;
	REG_WRITE32(atu_base_addr, 0x3100, 0);
	REG_WRITE32(atu_base_addr, 0x3104, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x3108, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x310C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x3110, (u32)((base_addr & 0xffffffff) + bari->bar1_part9_len -1));;	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x3114, (u32)(bari->bar1_part9_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x3118, bari->bar1_part9_dev_start >> 32);

	//ATU11
	base_addr = bar1_start + bari->bar1_part10_offset;
	REG_WRITE32(atu_base_addr, 0x3300, 0);
	REG_WRITE32(atu_base_addr, 0x3304, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x3308, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x330C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x3310, (u32)((base_addr & 0xffffffff) + bari->bar1_part10_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x3314, (u32)(bari->bar1_part10_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x3318, bari->bar1_part10_dev_start >> 32);

	//ATU12
	base_addr = bar1_start + bari->bar1_part11_offset;
	REG_WRITE32(atu_base_addr, 0x3500, 0);
	REG_WRITE32(atu_base_addr, 0x3504, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x3508, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x350C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x3510, (u32)((base_addr & 0xffffffff) + bari->bar1_part11_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x3514, (u32)(bari->bar1_part11_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x3518, bari->bar1_part11_dev_start >> 32);

	//ATU13
	base_addr = bar1_start + bari->bar1_part12_offset;
	REG_WRITE32(atu_base_addr, 0x3700, 0);
	REG_WRITE32(atu_base_addr, 0x3704, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x3708, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x370C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x3710, (u32)((base_addr & 0xffffffff) + bari->bar1_part12_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x3714, (u32)(bari->bar1_part12_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x3718, bari->bar1_part12_dev_start >> 32);

	//ATU14
	base_addr = bar1_start + bari->bar1_part13_offset;
	REG_WRITE32(atu_base_addr, 0x3900, 0);
	REG_WRITE32(atu_base_addr, 0x3904, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x3908, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x390C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x3910, (u32)((base_addr & 0xffffffff) + bari->bar1_part13_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x3914, (u32)(bari->bar1_part13_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x3918, bari->bar1_part13_dev_start >> 32);

	base_addr = bar1_start + bari->bar1_part14_offset;
	REG_WRITE32(atu_base_addr, 0x2b00, 0);
	REG_WRITE32(atu_base_addr, 0x2b04, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x2b08, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x2b0C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x2b10, (u32)((base_addr & 0xffffffff) + bari->bar1_part14_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x2b14, (u32)(bari->bar1_part14_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x2b18, bari->bar1_part14_dev_start >> 32);

	base_addr = bar1_start + bari->bar1_part15_offset;
	REG_WRITE32(atu_base_addr, 0x2900, 0);
	REG_WRITE32(atu_base_addr, 0x2904, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x2908, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x290C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x2910, (u32)((base_addr & 0xffffffff) + bari->bar1_part15_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x2914, (u32)(bari->bar1_part15_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x2918, bari->bar1_part15_dev_start >> 32);

	base_addr = bar1_start + bari->bar1_part16_offset;
	REG_WRITE32(atu_base_addr, 0x2700, 0);
	REG_WRITE32(atu_base_addr, 0x2704, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x2708, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x270C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x2710, (u32)((base_addr & 0xffffffff) + bari->bar1_part16_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x2714, (u32)(bari->bar1_part16_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x2718, bari->bar1_part16_dev_start >> 32);

	base_addr = bar1_start + bari->bar1_part17_offset;
	REG_WRITE32(atu_base_addr, 0x2500, 0);
	REG_WRITE32(atu_base_addr, 0x2504, 0x80000100); //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x2508, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x250C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x2510, (u32)((base_addr & 0xffffffff) + bari->bar1_part17_len -1));	//size 512 Bytes
	REG_WRITE32(atu_base_addr, 0x2514, (u32)(bari->bar1_part17_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x2518, bari->bar1_part17_dev_start >> 32);

	/*config bar2 for chip to chip transferr*/
	REG_WRITE32(atu_base_addr, 0x2d00, 0x80000);
	REG_WRITE32(atu_base_addr, 0x2d04, 0xc0080200);
	REG_WRITE32(atu_base_addr, 0x2d08, 0);
	REG_WRITE32(atu_base_addr, 0x2d0C, 0);
	REG_WRITE32(atu_base_addr, 0x2d10, 0);
	REG_WRITE32(atu_base_addr, 0x2d14, 0x0);
	REG_WRITE32(atu_base_addr, 0x2d18, 0x0);

	/* config bar4 to DDR phy*/
	//ATU4
	REG_WRITE32(atu_base_addr, 0x900, 0);
	REG_WRITE32(atu_base_addr, 0x904, 0x80000400);
	REG_WRITE32(atu_base_addr, 0x908, (u32)(bar4_start & 0xffffffff));//src addr
	REG_WRITE32(atu_base_addr, 0x90C, bar4_start >> 32);
	REG_WRITE32(atu_base_addr, 0x910, (u32)(bar4_start & 0xffffffff) + 0xFFFFF); //1M size
	REG_WRITE32(atu_base_addr, 0x914, 0x70000000); 	//dst addr
	REG_WRITE32(atu_base_addr, 0x918, 0x0);


	REG_WRITE32(atu_base_addr, 0x000, 0x2000);
	REG_WRITE32(atu_base_addr, 0x004, 0x80000000);
	REG_WRITE32(atu_base_addr, 0x008, 0); //src addr
	REG_WRITE32(atu_base_addr, 0x00C, 0x800);
	REG_WRITE32(atu_base_addr, 0x014, 0); //dst addr
	REG_WRITE32(atu_base_addr, 0x018, 0);
	REG_WRITE32(atu_base_addr, 0x010, 0xFFFFFFFF); //size
	REG_WRITE32(atu_base_addr, 0x020, 0xBFF);
}

void bm1688_unmap_bar(struct bm_bar_info *bari) {
	void __iomem *atu_base_addr;
	int i = 0;
	atu_base_addr = bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU;
	for (i = 0; i < 30; i++) {
		REG_WRITE32(atu_base_addr, 0x300 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x304 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x308 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x30C + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x310 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x314 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x318 + i*0x200, 0);
	}
}

static const struct bm_bar_info bm1688_bar_layout[] = {
{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x20400000,  //PCIE2 base addr

		.bar1_len = 0x800000,
		.bar1_dev_start = 0x00000000,

		.bar1_part0_offset = 0,
		.bar1_part0_len = 0x10000,
		.bar1_part0_dev_start = 0x28100000,

		.bar1_part1_offset = 0x10000,
		.bar1_part1_len = 0xA0000,
		.bar1_part1_dev_start = 0x27010000,

		.bar1_part2_offset = 0xB0000,
		.bar1_part2_len = 0x30000,
		.bar1_part2_dev_start = 0x36000000,

		.bar1_part3_offset = 0xE0000,
		.bar1_part3_len = 0x2A000,
		.bar1_part3_dev_start = 0x20BC0000,

		.bar1_part4_offset = 0x10A000,
		.bar1_part4_len = 0x44000,
		.bar1_part4_dev_start = 0x270D0000,

		.bar1_part5_offset = 0x14E000,
		.bar1_part5_len = 0xA0000,
		.bar1_part5_dev_start = 0x29000000,

		.bar1_part6_offset = 0x1EE000,
		.bar1_part6_len = 0x190000,
		.bar1_part6_dev_start = 0x290D0000,

		.bar1_part7_offset = 0x37E000,
		.bar1_part7_len = 0x60000,
		.bar1_part7_dev_start = 0x29300000,

		.bar1_part8_offset = 0x3DE000,
		.bar1_part8_len = 0x60000,
		.bar1_part8_dev_start = 0x21000000,

		.bar1_part9_offset = 0x43E000,
		.bar1_part9_len = 0x40000,
		.bar1_part9_dev_start = 0x23000000,

		.bar1_part10_offset = 0x47E000,
		.bar1_part10_len = 0x40000,
		.bar1_part10_dev_start = 0x24000000,

		.bar1_part11_offset = 0x4BE000,
		.bar1_part11_len = 0x40000,
		.bar1_part11_dev_start = 0x67000000,

		.bar1_part12_offset = 0x4FE000,
		.bar1_part12_len = 0x111000,
		.bar1_part12_dev_start = 0x68000000,

		.bar1_part13_offset = 0x60F000,
		.bar1_part13_len = 0x61000,
		.bar1_part13_dev_start = 0x26000000,

		.bar1_part14_offset = 0x670000,
		.bar1_part14_len = 0x4000,
		.bar1_part14_dev_start = 0x6FFF0000,

		.bar1_part15_offset = 0x674000,
		.bar1_part15_len = 0x80000,
		.bar1_part15_dev_start = 0x70000000,

		.bar1_part16_offset = 0x6F4000,
		.bar1_part16_len = 0x80000,
		.bar1_part16_dev_start = 0x78000000,

		.bar1_part17_offset = 0x774000,
		.bar1_part17_len = 0x80000,
		.bar1_part17_dev_start = 0x2580c000,

		.bar2_len = 0x100000,
		.bar2_dev_start = 0x0,
		.bar2_part0_offset = 0x0,
		.bar2_part0_len = 0x100000,
		.bar2_part0_dev_start = 0x200000000,

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x0,
	},
};

void bm1688_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi)
{
	void __iomem *apb_base_addr;
	int max_payload = 0x0;
	int max_rd_req = 0x0;
	int mode = 0x0;
	int total_func_num = 0;
	int i = 0;
	int temp_value = 2;
	int temp_low = 2;
	struct bm_card *bmcd = NULL;

	//apb_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + BM1688_OFFSET_PCIE_APB; //TODO for a2 0x20be3000, pcie4_apb
	apb_base_addr = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part3_offset + 0x20000 + 0x2000; //0x20be_2000 pcie2_apb
	mode =  bm1688_bmdrv_pcie_get_mode(bmdi) & 0x7;
	if (mode == 0x3)
		total_func_num = 0x1;
	else
		total_func_num = (mode & 0x3) + 0x1;

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

static struct bm_bar_info bm_mode_chose_layout_bm1688[] = {
{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x20400000,  //PCIE2 base addr

		.bar1_len = 0x800000,
		.bar1_dev_start = 0x00000000,

		.bar1_part0_offset = 0,
		.bar1_part0_len = 0x10000,
		.bar1_part0_dev_start = 0x28100000,

		.bar1_part1_offset = 0x10000,
		.bar1_part1_len = 0xA0000,
		.bar1_part1_dev_start = 0x27010000,

		.bar1_part2_offset = 0xB0000,
		.bar1_part2_len = 0x30000,
		.bar1_part2_dev_start = 0x36000000,

		.bar1_part3_offset = 0xE0000,
		.bar1_part3_len = 0x2A000,
		.bar1_part3_dev_start = 0x20BC0000,

		.bar1_part4_offset = 0x10A000,
		.bar1_part4_len = 0x44000,
		.bar1_part4_dev_start = 0x270D0000,

		.bar1_part5_offset = 0x14E000,
		.bar1_part5_len = 0xA0000,
		.bar1_part5_dev_start = 0x29000000,

		.bar1_part6_offset = 0x1EE000,
		.bar1_part6_len = 0x190000,
		.bar1_part6_dev_start = 0x290D0000,

		.bar1_part7_offset = 0x37E000,
		.bar1_part7_len = 0x60000,
		.bar1_part7_dev_start = 0x29300000,

		.bar1_part8_offset = 0x3DE000,
		.bar1_part8_len = 0x60000,
		.bar1_part8_dev_start = 0x21000000,

		.bar1_part9_offset = 0x43E000,
		.bar1_part9_len = 0x40000,
		.bar1_part9_dev_start = 0x23000000,

		.bar1_part10_offset = 0x47E000,
		.bar1_part10_len = 0x40000,
		.bar1_part10_dev_start = 0x24000000,

		.bar1_part11_offset = 0x4BE000,
		.bar1_part11_len = 0x40000,
		.bar1_part11_dev_start = 0x67000000,

		.bar1_part12_offset = 0x4FE000,
		.bar1_part12_len = 0x111000,
		.bar1_part12_dev_start = 0x68000000,

		.bar1_part13_offset = 0x60F000,
		.bar1_part13_len = 0x61000,
		.bar1_part13_dev_start = 0x26000000,

		.bar2_len = 0x100000,
		.bar2_dev_start = 0x0,
		.bar2_part0_offset = 0x0,
		.bar2_part0_len = 0x100000,
		.bar2_part0_dev_start = 0x20700000,

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x60100000,
	},
};

/* Setup the right bar layout based on bar len,
 * return 0 if find, else not find.
 */
int bm1688_setup_bar_dev_layout(struct bm_device_info *bmdi, BAR_LAYOUT_TYPE type)
{
	struct bm_bar_info *bar_info = &bmdi->cinfo.bar_info;
	const struct bm_bar_info *bar_layout = NULL;

	if (type == SETUP_BAR_DEV_LAYOUT) {
		bar_layout = bm1688_bar_layout;
	} else if (type == MODE_CHOSE_LAYOUT) {
		bar_layout = bm_mode_chose_layout_bm1688;
	}

	if (bar_layout->bar1_len == bar_info->bar1_len &&
			bar_layout->bar2_len == bar_info->bar2_len) {
		bar_info->bar0_dev_start = bar_layout->bar0_dev_start;
		bar_info->bar1_dev_start = bar_layout->bar1_dev_start;
		bar_info->bar1_part0_dev_start = bar_layout->bar1_part0_dev_start;
		bar_info->bar1_part1_dev_start = bar_layout->bar1_part1_dev_start;
		bar_info->bar1_part2_dev_start = bar_layout->bar1_part2_dev_start;
		bar_info->bar1_part3_dev_start = bar_layout->bar1_part3_dev_start;
		bar_info->bar1_part4_dev_start = bar_layout->bar1_part4_dev_start;
		bar_info->bar1_part5_dev_start = bar_layout->bar1_part5_dev_start;
		bar_info->bar1_part6_dev_start = bar_layout->bar1_part6_dev_start;
		bar_info->bar1_part7_dev_start = bar_layout->bar1_part7_dev_start;
		bar_info->bar1_part8_dev_start = bar_layout->bar1_part8_dev_start;
		bar_info->bar1_part9_dev_start = bar_layout->bar1_part9_dev_start;
		bar_info->bar1_part10_dev_start = bar_layout->bar1_part10_dev_start;
		bar_info->bar1_part11_dev_start = bar_layout->bar1_part11_dev_start;
		bar_info->bar1_part12_dev_start = bar_layout->bar1_part12_dev_start;
		bar_info->bar1_part13_dev_start = bar_layout->bar1_part13_dev_start;
		bar_info->bar1_part14_dev_start = bar_layout->bar1_part14_dev_start;
		bar_info->bar1_part15_dev_start = bar_layout->bar1_part15_dev_start;
		bar_info->bar1_part16_dev_start = bar_layout->bar1_part16_dev_start;
		bar_info->bar1_part17_dev_start = bar_layout->bar1_part17_dev_start;
		bar_info->bar1_part0_offset = bar_layout->bar1_part0_offset;
		bar_info->bar1_part1_offset = bar_layout->bar1_part1_offset;
		bar_info->bar1_part2_offset = bar_layout->bar1_part2_offset;
		bar_info->bar1_part3_offset = bar_layout->bar1_part3_offset;
		bar_info->bar1_part4_offset = bar_layout->bar1_part4_offset;
		bar_info->bar1_part5_offset = bar_layout->bar1_part5_offset;
		bar_info->bar1_part6_offset = bar_layout->bar1_part6_offset;
		bar_info->bar1_part7_offset = bar_layout->bar1_part7_offset;
		bar_info->bar1_part8_offset = bar_layout->bar1_part8_offset;
		bar_info->bar1_part9_offset = bar_layout->bar1_part9_offset;
		bar_info->bar1_part10_offset = bar_layout->bar1_part10_offset;
		bar_info->bar1_part11_offset = bar_layout->bar1_part11_offset;
		bar_info->bar1_part12_offset = bar_layout->bar1_part12_offset;
		bar_info->bar1_part13_offset = bar_layout->bar1_part13_offset;
		bar_info->bar1_part14_offset = bar_layout->bar1_part14_offset;
		bar_info->bar1_part15_offset = bar_layout->bar1_part15_offset;
		bar_info->bar1_part16_offset = bar_layout->bar1_part16_offset;
		bar_info->bar1_part17_offset = bar_layout->bar1_part17_offset;
		bar_info->bar1_part0_len = bar_layout->bar1_part0_len;
		bar_info->bar1_part1_len = bar_layout->bar1_part1_len;
		bar_info->bar1_part2_len = bar_layout->bar1_part2_len;
		bar_info->bar1_part3_len = bar_layout->bar1_part3_len;
		bar_info->bar1_part4_len = bar_layout->bar1_part4_len;
		bar_info->bar1_part5_len = bar_layout->bar1_part5_len;
		bar_info->bar1_part6_len = bar_layout->bar1_part6_len;
		bar_info->bar1_part7_len = bar_layout->bar1_part7_len;
		bar_info->bar1_part8_len = bar_layout->bar1_part8_len;
		bar_info->bar1_part9_len = bar_layout->bar1_part9_len;
		bar_info->bar1_part10_len = bar_layout->bar1_part10_len;
		bar_info->bar1_part11_len = bar_layout->bar1_part11_len;
		bar_info->bar1_part12_len = bar_layout->bar1_part12_len;
		bar_info->bar1_part13_len = bar_layout->bar1_part13_len;
		bar_info->bar1_part14_len = bar_layout->bar1_part14_len;
		bar_info->bar1_part15_len = bar_layout->bar1_part15_len;
		bar_info->bar1_part16_len = bar_layout->bar1_part16_len;
		bar_info->bar1_part17_len = bar_layout->bar1_part17_len;

		bar_info->bar2_dev_start = bar_layout->bar2_dev_start;
		bar_info->bar2_part0_dev_start = bar_layout->bar2_part0_dev_start;
		bar_info->bar2_part0_offset = bar_layout->bar2_part0_offset;
		bar_info->bar2_part0_len = bar_layout->bar2_part0_len;

		bar_info->bar4_dev_start = bar_layout->bar4_dev_start;
		bar_info->bar4_len = bar_layout->bar4_len;
		return 0;
	}
	/* Not find */
	return -1;
}

void bm1688_bmdrv_init_for_mode_chose(struct bm_device_info *bmdi, struct pci_dev *pdev, struct bm_bar_info *bari)
{
	void __iomem *atu_base_addr;
	void __iomem *cfg_base_addr;
	struct bm_bar_info *bar_info = &bmdi->cinfo.bar_info;
	int function_num = 0x0;
	u64 bar1_start = 0;
	u64 bar2_start = 0;
	u64 bar4_start = 0;
	u64 base_addr;

	cfg_base_addr = bar_info->bar0_vaddr; //0x5f800000
	if (bmdi->cinfo.pcie_func_index > 0)
		function_num = bmdi->cinfo.pcie_func_index;
	else
		function_num = (pdev->devfn & 0x7);

	if (REG_READ32(cfg_base_addr, 0x14) == 0xffffffff) {
		pr_info("pcie link may error for mode choose\n");
		return;
	}
	bmdi->cinfo.bmdrv_setup_bar_dev_layout(bmdi, MODE_CHOSE_LAYOUT);
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
	atu_base_addr = bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU;

	//ATU1
	// DTCM  512k
	base_addr = bar1_start + bari->bar1_part0_offset;
	REG_WRITE32(atu_base_addr, 0x300, 0);
	REG_WRITE32(atu_base_addr, 0x304, 0x80000100);		//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x308, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x30C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x310, (u32)((base_addr & 0xffffffff) + bari->bar1_part0_len -1));	//limit addr
	REG_WRITE32(atu_base_addr, 0x314, (u32)(bari->bar1_part0_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x318, bari->bar1_part0_dev_start >> 32);

	//ATU2
	// Top regs  3M
	base_addr = bar1_start + bari->bar1_part1_offset;
	REG_WRITE32(atu_base_addr, 0x500, 0);
	REG_WRITE32(atu_base_addr, 0x504, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x508, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x50C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x510,  (u32)((base_addr & 0xffffffff) + bari->bar1_part1_len -1));	//size 3M
	REG_WRITE32(atu_base_addr, 0x514, (u32)(bari->bar1_part1_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x518, bari->bar1_part1_dev_start >> 32);

	//ATU3
	// DMA/MMU/TPU  64k
	base_addr = bar1_start + bari->bar1_part2_offset;
	REG_WRITE32(atu_base_addr, 0x700, 0);
	REG_WRITE32(atu_base_addr, 0x704, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x708, (u32)(base_addr & 0xffffffff));		//src addr
	REG_WRITE32(atu_base_addr, 0x70C, base_addr >> 32);
	REG_WRITE32(atu_base_addr, 0x710, (u32)((base_addr & 0xffffffff) + bari->bar1_part2_len -1));	//size 64K
	REG_WRITE32(atu_base_addr, 0x714, (u32)(bari->bar1_part2_dev_start & 0xffffffff));		//dst addr
	REG_WRITE32(atu_base_addr, 0x718, bari->bar1_part2_dev_start >> 32);

	//ATU5
	REG_WRITE32(atu_base_addr, 0xb00, 0);
	REG_WRITE32(atu_base_addr, 0xb04, 0x80000000);  //address match mode for bar2, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xb08, (u32)(bar2_start & 0xffffffff));//src addr
	REG_WRITE32(atu_base_addr, 0xb0C, bar2_start >> 32);
	REG_WRITE32(atu_base_addr, 0xb10, (u32)(bar2_start & 0xffffffff) + 0xfffff); //size 1M
	REG_WRITE32(atu_base_addr, 0xb14, 0x20700000);      //dst addr - pcie2_iATU
	REG_WRITE32(atu_base_addr, 0xb18, 0);
	REG_WRITE32(atu_base_addr, 0xb20, bar2_start >> 32);

	//ATU6
	// for down cfg
	REG_WRITE32(atu_base_addr, 0xd00, 0x0);
	REG_WRITE32(atu_base_addr, 0xd04, 0x80000000);  //address match mode for bar4, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xd08, (u32)(bar4_start & 0xffffffff));        //src addr
	REG_WRITE32(atu_base_addr, 0xd0C, bar4_start >> 32);
	REG_WRITE32(atu_base_addr, 0xd10, (u32)(bar4_start & 0xffffffff) + 0xfffff);   //size 1M
	REG_WRITE32(atu_base_addr, 0xd14, 0x80100000);      //dst addr
	REG_WRITE32(atu_base_addr, 0xd18, 0);
	REG_WRITE32(atu_base_addr, 0xd20, bar4_start >> 32);
}

int bm1688_bmdrv_pcie_get_mode(struct bm_device_info *bmdi)
{
	int mode = 0x0;

	//TODO get gpio 107~110
	//mode = gpio_reg_read(bmdi, 0x50);
	//if (mode == 0xffffffff) {
	//	pr_info("pcie get mode fail 0x%x \n", mode);
	//	return -1;
	//}
	//mode >>= 0x9;
	mode &= 0xf;
	pr_info("pcie get mode is 0x%x \n", mode);
	return mode;
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

	REG_WRITE32(atu_base_addr, 0x1508, 0x0);
	REG_WRITE32(atu_base_addr, 0x150c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1510, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1514, 0x0);
	REG_WRITE32(atu_base_addr, 0x1518, 0x800);         //inbound tagrget address 0x800_0000_0000
	REG_WRITE32(atu_base_addr, 0x1500, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1504, 0xc0080000); //bar match, match bar0

	REG_WRITE32(atu_base_addr, 0x1708, 0x0);
	REG_WRITE32(atu_base_addr, 0x170c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1710, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1714, 0x0);
	REG_WRITE32(atu_base_addr, 0x1718, 0x900);         //inbound tagrget address 0x900_0000_0000
	REG_WRITE32(atu_base_addr, 0x1700, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1704, 0xc0080100); //bar match, match bar1

	REG_WRITE32(atu_base_addr, 0x1908, 0x0);
	REG_WRITE32(atu_base_addr, 0x190c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1910, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x1914, 0x0);
	REG_WRITE32(atu_base_addr, 0x1918, 0x1 << 12);         //inbound tagrget address 0x1000_0000_0000
	REG_WRITE32(atu_base_addr, 0x1900, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1904, 0xc0080200); //bar match, match bar2

	REG_WRITE32(atu_base_addr, 0x1b08, 0x0);
	REG_WRITE32(atu_base_addr, 0x1b0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1b10, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x1b14, 0x0);
	REG_WRITE32(atu_base_addr, 0x1b18, 0xa00);       //inbound target address 0xa00_0000_0000
	REG_WRITE32(atu_base_addr, 0x1b00, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1b04, 0xc0080400); //bar match, match bar4

	atu_base_addr = bmdi->cinfo.bar_info.bar2_vaddr; //0x5ff00000
	REG_WRITE32(atu_base_addr, 0x008, 0x0);
	REG_WRITE32(atu_base_addr, 0x00c, 0x800);       //outbound base address 800_0000_0000
	REG_WRITE32(atu_base_addr, 0x010, 0x400000 -1);  //size 4M
	REG_WRITE32(atu_base_addr, 0x020, 0x800);  //size 4M
	REG_WRITE32(atu_base_addr, 0x014, 0x88000000);
	REG_WRITE32(atu_base_addr, 0x018, 0x0);         //outbound target address 8800_0000
	REG_WRITE32(atu_base_addr, 0x000, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x004, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x208, 0x0);
	REG_WRITE32(atu_base_addr, 0x20c, 0x900);       //outbound base address 900_0000_0000
	REG_WRITE32(atu_base_addr, 0x210, 0x1000000 -1 );  //size 16M
	REG_WRITE32(atu_base_addr, 0x220, 0x900);  //size 4M
	REG_WRITE32(atu_base_addr, 0x214, 0x89000000);
	REG_WRITE32(atu_base_addr, 0x218, 0x0);         //outbound target address 8900_0000
	REG_WRITE32(atu_base_addr, 0x200, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x204, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x408, 0x0);
	REG_WRITE32(atu_base_addr, 0x40c, 0x1 << 12);       //outbound base address 1000_0000_0000
	REG_WRITE32(atu_base_addr, 0x410, 0xffffffff);
	REG_WRITE32(atu_base_addr, 0x420, 0x1 << 12);  //size 1M
	REG_WRITE32(atu_base_addr, 0x414, 0x0);
	REG_WRITE32(atu_base_addr, 0x418, 0x1 << 12);         //outbound target address
	REG_WRITE32(atu_base_addr, 0x400, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x404, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x608, 0x0);
	REG_WRITE32(atu_base_addr, 0x60c, 0xa00);       //outbound base address 9_0000_0000
	REG_WRITE32(atu_base_addr, 0x610, 0x100000 -1);  //size 1M
	REG_WRITE32(atu_base_addr, 0x620, 0xa00);  //size 1M
	REG_WRITE32(atu_base_addr, 0x614, 0x8b000000); //outbound target address 0x8b00_0000
	REG_WRITE32(atu_base_addr, 0x618, 0x0);
	REG_WRITE32(atu_base_addr, 0x600, 0x2000);         //type 4'b0000, MRd/Mwr type
	REG_WRITE32(atu_base_addr, 0x604, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x808, 0x80000000); //out cfg
	REG_WRITE32(atu_base_addr, 0x80c, 0x0);
	REG_WRITE32(atu_base_addr, 0x810, 0x80300000 -1);
	REG_WRITE32(atu_base_addr, 0x814, 0x0);
	REG_WRITE32(atu_base_addr, 0x818, 0x0);
	REG_WRITE32(atu_base_addr, 0x800, 0x4);
	REG_WRITE32(atu_base_addr, 0x804, 0x90000000);

	//enable rc msi
	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20400000);      //dst addr
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr
	value = REG_READ32(bari->bar2_vaddr, 0x4);
	value |= 0x7;
	REG_WRITE32(bari->bar2_vaddr, 0x4, value);
	value = REG_READ32(bari->bar2_vaddr, 0x50);
	value |= (0x1 << 16);
	REG_WRITE32(bari->bar2_vaddr, 0x50, value);
	REG_READ32(bari->bar2_vaddr, 0x50);


	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20700000);      //dst addr
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr

}

int bm1688_bmdrv_calculate_chip_num(int mode, int function_num)
{
	int chip_seqnum = 0;

	switch (mode) {
	case 0x7:
	case 0x6:
		chip_seqnum = 0x1;
		break;
	default:
		chip_seqnum = 0x0;
		break;
	}
	return chip_seqnum;
}

void bm1688_bmdrv_set_chip_num(int chip_seqnum, struct bm_device_info *bmdi)
{
       void __iomem *top_apb_addr;
       int value = 0x0;

       //top_abp_addr = bmdi->cinfo.bar_info.bar0_vaddr + BM1688_OFFSET_PCIE_TOP; //0x20BE00000
       top_apb_addr = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part3_offset + 0x20000;
       if (chip_seqnum)
               value = 0x1;
       REG_WRITE32(top_apb_addr + 0x40, 0, value);
}

void bm1688_bmdrv_pcie_perst(struct bm_device_info *bmdi)
{
	//TODO for A2, gpio85
	int value = 0;

	//value = gpio_reg_read(bmdi, 0x4);
	//value |= (0x1 << 16);
	//gpio_reg_write(bmdi, 0x4, value);

	//value = gpio_reg_read(bmdi,0x0);
	pr_info("gpio 16 value = 0x%x\n", value);
	//value |= (0x1 << 16);
	//gpio_reg_write(bmdi, 0x0, value);
	//mdelay(300);
}

int bm1688_bmdrv_pcie_polling_rc_perst(struct pci_dev *pdev, struct bm_bar_info *bari)
{
	int loop = 200;
	int ret = 0;

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20000000);
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);

	// Wait for preset by other, dual_chip PCIE2 as RC, wait bit1
	while (!((REG_READ32(bari->bar2_vaddr, 0x54) & (1 << 0x1)))) { //bit0:pcie4 bit1:pcie2
		if (loop-- > 0) {
			msleep(1000);
			pr_info("polling rc perst \n");
		}
		else {
			ret = -1;
			pr_info("polling rc perst time ount \n");
			break;
		}
	}

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20700000);      //dst addr: RC iATU
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr
	return ret;
}

int bm1688_bmdrv_pcie_polling_rc_core_rst(struct pci_dev *pdev, struct bm_bar_info *bari)
{
	int loop = 200;
	int ret = 0;

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20010000);
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);
	//TODO Wait for preset by other
	while (!(REG_READ32(bari->bar2_vaddr, 0x5c) & (1 << 8))) { //polling core_rst_n
		if (loop-- > 0) {
			msleep(1);
			pr_info("polling rc core rst \n");
		}
		else {
			ret = -1;
			pr_info("polling rc core time out\n");
			break;
		}
	}
	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20700000);      //dst addr: RC iATU
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr
	return ret;
}

void bm1688_bmdrv_pcie_set_rc_link_speed_gen_x(struct bm_bar_info *bari, int gen_speed)
{
	int value = 0;

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20400000);      //dst addr: RC dbi
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr

	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) | 0x1); // enable DBI_RO_WR_EN

	REG_WRITE32(bari->bar2_vaddr, 0x80c, REG_READ32(bari->bar2_vaddr, 0x80c) | (0x1 << 17)); // enable pcie link change speed

	value = REG_READ32(bari->bar2_vaddr, 0xa0);
	value &= ~0xf;
	value |= (0x3 & gen_speed);
	REG_WRITE32(bari->bar2_vaddr, 0xa0, value); // cap_target_link_speed Gen3

	value = REG_READ32(bari->bar2_vaddr ,0x7c);
	value &= ~0xf;
	value |= (0x3 & gen_speed);
	REG_WRITE32(bari->bar2_vaddr , 0x7c, value); // cap_max_link_speed Gen3

	value = REG_READ32(bari->bar2_vaddr, 0x98);
	value &= ~(0xf);
	value |= (0xa);
	REG_WRITE32(bari->bar2_vaddr, 0x98, value);

	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) & (~0x1)); // disable DBI_RO_WR_EN

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20700000);      //dst addr: RC iATU
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr
}

void bm1688_bmdrv_pcie_set_rc_max_payload_setting(struct bm_bar_info *bari)
{
	int value = 0;

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20400000);      //dst addr
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr
	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) | 0x1); // enable DBI_RO_WR_EN
	value = REG_READ32(bari->bar2_vaddr, 0x78);
	value &= ~(0x7 << 5);
	value |= (0x1 << 5);
	REG_WRITE32(bari->bar2_vaddr, 0x78, value);
	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) & (~0x1)); // disable DBI_RO_WR_EN
	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20700000);      //dst addr
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr
}

void bm1688_bmdrv_pcie_enable_rc(struct bm_bar_info *bari)
{
	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20010000);
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);
	REG_WRITE32(bari->bar2_vaddr, 0x58, REG_READ32(bari->bar2_vaddr, 0x58) | 0x1); // enable ltssm
	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20700000);
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);
}

int bm1688_bmdrv_pcie_polling_rc_link_state(struct bm_bar_info *bari)
{
	int value = 0x0;
	int count = 0x20;
	int ret = 0;

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20010000);      //dst addr
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);      //dst addr

	value = REG_READ32(bari->bar2_vaddr, 0xb4);
	value = value >> 6;
	value &= 0x3;

	while(value != 0x3) {
		value = REG_READ32(bari->bar2_vaddr, 0xb4);
		value = value >> 6;
		value &= 0x3;

		if (count-- > 0) {
			msleep(2);  //TODO: use 2ms for palladium now, else 2000ms
			pr_info("wait link state count = %d, link state = 0x%x\n", count, value);
		} else {
			ret = -1;
			pr_info("polling rc link time out\n");
			break;
		}
	}

	REG_WRITE32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14, 0x20700000);
	REG_READ32(bari->bar0_vaddr + BM1688_OFFSET_PCIE_iATU, 0xb14);

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
	int count = 0x5;
	int ret = 0;

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
	int mode = 0x0;
	int debug_value =0x0;
	int function_num = 0x0;
	int chip_seqnum = 0x0;
	int max_function_num = 0x0;
	int ret = 0x0;

	bm1688_get_pcie_func_index(bmdi);
	if (bmdi->cinfo.pcie_func_index > 0)
		function_num = bmdi->cinfo.pcie_func_index;
	else
		function_num = (pdev->devfn & 0x7);

	bm1688_bmdrv_init_for_mode_chose(bmdi, pdev, bari);
	io_init(bmdi);

	mode = bm1688_bmdrv_pcie_get_mode(bmdi);
	pr_info("mode  = 0x%x\n", mode);
	if (mode < 0) {
		return -1;
	}

	//debug_value = top_reg_read(bmdi, 0);
	pr_info("top value = 0x%x\n", debug_value);
	if (mode == 0) {
		pr_info("single ep mode\n");
		return 0;
	}

	if (mode == 0x3) {
		chip_seqnum = 0x1;
		bm1688_bmdrv_set_chip_num(chip_seqnum, bmdi);
		return 0;
	}

	if (mode != 0) {
		bm1688_bmdrv_pcie_rc_init(pdev, bmdi, bari);
	}

	pr_info("function_num = 0x%d \n", function_num);
	chip_seqnum = bm1688_bmdrv_calculate_chip_num(mode, function_num);
	if ((chip_seqnum < 0x1) || (chip_seqnum > 0x4)) {
		pr_info("chip_seqnum is 0x%x illegal", chip_seqnum);
		return -1;
	}

	bm1688_bmdrv_set_chip_num(chip_seqnum, bmdi);

	max_function_num = (mode & 0x3);

	switch (max_function_num) {
	case 0x1:
		bm1688_bmdrv_pcie_set_function1_iatu_config(pdev, bmdi);
		ret = bm1688_bmdrv_pci_bus_scan(pdev, bmdi, max_function_num);
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
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
		cfg_base_addr += 0x60000000;  //????
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
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x10, 0x88000000,0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x14, 0x89000000,0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x18, 0x0,0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x1c, 0x1000, 0xfffffff0);
			bm1688_bmdrv_write_config(bmdi, cfg_base_addr, 0x20, 0x8b000000,0xfffffff0);
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
	dst_addr = REG_READ32(atu_base_addr, 0x914); //dst addr

	if ((addr  > (dst_addr + 0xFFFFF)) || (addr < dst_addr) ) {
		temp_addr = addr & (~0xfffff);
		REG_WRITE32(atu_base_addr, 0x914, temp_addr); //dst addr
		temp_addr = REG_READ32(atu_base_addr, 0x914); //dst addr
	}
}
