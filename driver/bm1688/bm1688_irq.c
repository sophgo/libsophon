#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include "bm_common.h"
#include "bm_card.h"
#include "bm1688_reg.h"
#include "bm1688_pcie.h"
#include "bm1688_irq.h"

void bm1688_pcie_msi_irq_disable(struct bm_device_info *bmdi)
{
#ifdef MSI_IRQ_FORWARD
	void __iomem *top_base_addr = NULL;
	top_base_addr = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part3_offset + 0x20000;
	if (bmdi->cinfo.pcie_func_index == 0) {
		REG_WRITE32(top_base_addr, 0x20, 0x0);
	} else {
		REG_WRITE32(top_base_addr, 0x14, 0x0);
	}
#else
	void __iomem *pcie_apb_base = NULL;
	u32 value = 0x0;
	pcie_apb_base = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part_info[4].offset;
	value = REG_READ32(pcie_apb_base, 0x70);
	value |= 0x04;
	REG_WRITE32(pcie_apb_base, 0x70, value); /* disable pcie msi */
#endif
}

//#define MSI_TIMEOUT_COUNTER
void bm1688_pcie_remap_msi_init(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	struct bm_card *bmcd = NULL;
	struct bm_bar_info *bari = &bmdi->cinfo.bar_info;
	void __iomem *pcie_cfg_base = bari->bar0_vaddr;
	void __iomem *pcie_apb_base = bari->bar1_vaddr + bari->bar1_part_info[4].offset;
	void __iomem *pcie_axi_base = bari->bar1_vaddr + bari->bar1_part_info[5].offset;
	u32 value = 0x0;
	u32 msi_low_addr =  0x0;
	u32 msi_hi_addr  =  0x0;
	u32 msi_data = 0x0;
	u32 function_num = 0x0;

	if (bmdi->cinfo.pcie_func_index != 0)
		function_num = 1;

	if (function_num == 0x0) {
		msi_low_addr =  REG_READ32(pcie_cfg_base, 0x54);
		msi_hi_addr =  REG_READ32(pcie_cfg_base, 0x58);
		msi_data =  REG_READ32(pcie_cfg_base, 0x5c);
	} else {
		bmcd = bmdrv_card_get_bm_card(bmdi);
		if ((bmcd != NULL) && (bmcd->card_bmdi[0] != NULL)) {
			pcie_cfg_base = bmcd->card_bmdi[0]->cinfo.bar_info.bar0_vaddr;
			pcie_cfg_base += 0x10000;
			msi_low_addr =  REG_READ32(pcie_cfg_base, 0x54);
			msi_hi_addr =  REG_READ32(pcie_cfg_base, 0x58);
			msi_data =  REG_READ32(pcie_cfg_base, 0x5c);
		} else {
			pci_read_config_dword(pdev, 0x54, &msi_low_addr);
			pci_read_config_dword(pdev, 0x58, &msi_hi_addr);
			pci_read_config_dword(pdev, 0x5c, &msi_data);
		}
		msi_hi_addr |= bmdi->cinfo.ob_base;
	}

#ifndef MSI_IRQ_FORWARD
	//enable top msi, 0x1 for pcie4, 0x0 for pcie2
	if (bmdi->cinfo.pcie_func_index == 0) {
		value = REG_READ32(pcie_axi_base, 0x30);
		value &= ~0x3;
		if (bmdi->cinfo.mode & 0x1)
			value |= 0x1;
		REG_WRITE32(pcie_axi_base, 0x30, value);
	}

	REG_WRITE32(pcie_apb_base, 0x60, msi_low_addr);
	REG_WRITE32(pcie_apb_base, 0x64, msi_hi_addr);
	REG_WRITE32(pcie_apb_base, 0x68, msi_data);
	value = REG_READ32(pcie_apb_base, 0x70);
	value &= ~(0xff05); //clear msi irq_mask, counter and divider
#ifdef MSI_TIMEOUT_COUNTER
	value |= 0x6401; //enble the counter and set divider
	REG_WRITE32(pcie_apb_base, 0x6C, 0x100);
#endif
	REG_WRITE32(pcie_apb_base, 0x70, value);
#else
    //enable top msi, 0x1 for pcie4, 0x0 for pcie2
	value = REG_READ32(pcie_axi_addr, 0x30);
	value |= 0x3;
	REG_WRITE32(pcie_axi_addr, 0x30, value);

	if (bmdi->cinfo.mode & 0x1) {
		REG_WRITE32(pcie_top_base_addr, 0x30, msi_hi_addr);
		REG_WRITE32(pcie_top_base_addr, 0x20, (msi_low_addr & 0xfffff000) | 0x1);
	} else {
		REG_WRITE32(pcie_top_base_addr, 0x28, msi_hi_addr);
		REG_WRITE32(pcie_top_base_addr, 0x14, (msi_low_addr & 0xfffff000) | 0x1);
		REG_WRITE32(bmdi->cinfo.bar_info.bar0_vaddr, 0x54, msi_low_addr);
		REG_WRITE32(bmdi->cinfo.bar_info.bar0_vaddr, 0x58, msi_hi_addr);
		REG_WRITE32(bmdi->cinfo.bar_info.bar0_vaddr, 0x5c, msi_data);
	}
#endif
	PR_TRACE("pcie msi low addr = 0x%x, hi addr = 0x%x, data = 0x%x\n",
			msi_low_addr, msi_hi_addr, msi_data);
	PR_TRACE("funtion num = 0x%x\n", function_num);
}

void bm1688_pcie_msi_irq_enable(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	bm1688_pcie_remap_msi_init(pdev, bmdi);
}

void bm1688_enable_intc_irq(struct bm_device_info *bmdi, int irq_num, bool irq_enable)
{
	int irq_enable_offset = 0x0;
	int irq_parent_mask = 0x0;
	int value = 0x0;
	unsigned long flag;

	if (irq_num < 0 || irq_num > 192) {
		pr_info("bmdrv_enbale_intc_irq irq_num = %d is wrong!\n", irq_num);
		return;
	}

	if (irq_num < 64) {
		if (irq_num < 32)
			irq_enable_offset = BM1688_INTC0_BASE_OFFSET + BM1688_INTC_INTEN_L_OFFSET;
		else
			irq_enable_offset = BM1688_INTC0_BASE_OFFSET + BM1688_INTC_INTEN_H_OFFSET;
	} else if (irq_num < 128) {
		irq_parent_mask = 30;
		if (irq_num < 96)
			irq_enable_offset = BM1688_INTC1_BASE_OFFSET + BM1688_INTC_INTEN_L_OFFSET;
		else
			irq_enable_offset = BM1688_INTC1_BASE_OFFSET + BM1688_INTC_INTEN_H_OFFSET;
	} else {
		irq_parent_mask = 31;
		if (irq_num < 160)
			irq_enable_offset = BM1688_INTC2_BASE_OFFSET + BM1688_INTC_INTEN_L_OFFSET;
		else
			irq_enable_offset = BM1688_INTC2_BASE_OFFSET + BM1688_INTC_INTEN_H_OFFSET;
	}


	if (irq_num > 63) {
		value = intc_reg_read(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_INTEN_H_OFFSET);
		if (irq_enable == true)
			value |= 0x1 << irq_parent_mask;
		else
			value &= (~(0x1 << irq_parent_mask));
		intc_reg_write(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_INTEN_H_OFFSET, value);
	}

	spin_lock_irqsave(&bmdi->irq_lock, flag);

	value = intc_reg_read(bmdi, irq_enable_offset);
	if (irq_enable == true)
		value |= (0x1 << (irq_num % 32));
	else
		value &= (~(0x1 << (irq_num % 32)));
	intc_reg_write(bmdi, irq_enable_offset, value);

	spin_unlock_irqrestore(&bmdi->irq_lock, flag);
}

void bm1688_unmaskall_intc_irq(struct bm_device_info *bmdi)
{
	int value = 0x0;
#ifndef MSI_IRQ_FORWARD
	void __iomem *pcie_apb_base = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part_info[4].offset;
#endif

	intc_reg_write(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC1_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC1_BASE_OFFSET + BM1688_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC2_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC2_BASE_OFFSET + BM1688_INTC_MASK_H_OFFSET, value);
#ifndef MSI_IRQ_FORWARD
	// clear msi mask bit
	REG_WRITE32(pcie_apb_base, 0x70, REG_READ32(pcie_apb_base, 0x70) | 0x2);
#endif
}

void bm1688_maskall_intc_irq(struct bm_device_info *bmdi)
{
	int value = 0xffffffff;

	intc_reg_write(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC1_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC1_BASE_OFFSET + BM1688_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC2_BASE_OFFSET + BM1688_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, BM1688_INTC2_BASE_OFFSET + BM1688_INTC_MASK_H_OFFSET, value);
}

void bm1688_get_irq_status(struct bm_device_info *bmdi, u32 *status)
{
	status[0] = intc_reg_read(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_STATUS_L_OFFSET);
	status[1] = intc_reg_read(bmdi, BM1688_INTC0_BASE_OFFSET + BM1688_INTC_STATUS_H_OFFSET);
	status[2] = intc_reg_read(bmdi, BM1688_INTC1_BASE_OFFSET + BM1688_INTC_STATUS_L_OFFSET);
	status[3] = intc_reg_read(bmdi, BM1688_INTC1_BASE_OFFSET + BM1688_INTC_STATUS_H_OFFSET);
	status[4] = intc_reg_read(bmdi, BM1688_INTC2_BASE_OFFSET + BM1688_INTC_STATUS_L_OFFSET);
	status[5] = intc_reg_read(bmdi, BM1688_INTC2_BASE_OFFSET + BM1688_INTC_STATUS_H_OFFSET);
}
