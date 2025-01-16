#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include "bm_common.h"
#include "bm_card.h"
#include "SGTPUV8_reg.h"
#include "SGTPUV8_pcie.h"
#include "SGTPUV8_irq.h"
#include <asm/div64.h>

void SGTPUV8_pcie_msi_irq_disable(struct bm_device_info *bmdi)
{
	void __iomem *apb_base_addr = NULL;
	u32 value = 0x0;
	apb_base_addr = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part3_offset + 0x20000 + 0x2000;
	value = REG_READ32(apb_base_addr, 0x70);
	value |= 0x04;
	REG_WRITE32(apb_base_addr, 0x70, value); /* disable pcie msi */
}

// #define MSI_TIMEOUT_COUNTER
void SGTPUV8_pcie_remap_msi_init(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	void __iomem *pcie_top_base_addr = NULL;
	void __iomem *pcie_iatu_addr = NULL;
	void __iomem *pcie_apb_addr = NULL;
	void __iomem *pcie_cfg_addr = NULL;
	struct bm_card *bmcd = NULL;
	u32 value = 0x0;
	u32 msi_low_addr = 0x0;
	u32 msi_hi_addr = 0x0;
	u32 msi_data = 0x0;
	u32 chip_id = 0x0;
	u32 function_num = 0x0;

	pcie_top_base_addr = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part3_offset + 0x20000;
	pcie_apb_addr = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part3_offset + 0x20000 + 0x2000;
	pcie_iatu_addr = bmdi->cinfo.bar_info.bar0_vaddr + SGTPUV8_OFFSET_PCIE_iATU;
	pcie_cfg_addr = bmdi->cinfo.bar_info.bar0_vaddr;
	chip_id = REG_READ32(pcie_top_base_addr, 0x40);
	chip_id &= 0x1;
	if (chip_id > 0)
		function_num = chip_id - 1;
	if (function_num == 0x0)
	{
		msi_low_addr = REG_READ32(pcie_cfg_addr, 0x54);
		msi_hi_addr = REG_READ32(pcie_cfg_addr, 0x58);
		msi_data = REG_READ32(pcie_cfg_addr, 0x5c);
	}
	else
	{
		bmcd = bmdrv_card_get_bm_card(bmdi);
		if ((bmcd != NULL) && (bmcd->card_bmdi[0] != NULL))
		{
			pcie_cfg_addr = bmcd->card_bmdi[0]->cinfo.bar_info.bar0_vaddr;
			pcie_cfg_addr += function_num * 0x10000;
			msi_low_addr = REG_READ32(pcie_cfg_addr, 0x54);
			msi_hi_addr = REG_READ32(pcie_cfg_addr, 0x58);
			msi_data = REG_READ32(pcie_cfg_addr, 0x5c);
		}
		else
		{
			pci_read_config_dword(pdev, 0x54, &msi_low_addr);
			pci_read_config_dword(pdev, 0x58, &msi_hi_addr);
			pci_read_config_dword(pdev, 0x5c, &msi_data);
		}
	}

#ifndef MSI_IRQ_FORWARD
	REG_WRITE32(pcie_apb_addr, 0x60, msi_low_addr);
	REG_WRITE32(pcie_apb_addr, 0x64, msi_hi_addr);
	REG_WRITE32(pcie_apb_addr, 0x68, msi_data);
	value = REG_READ32(pcie_apb_addr, 0x70);
	value &= ~(0xff05); // clear msi irq_mask, counter and divider
#ifdef MSI_TIMEOUT_COUNTER
	value |= 0x6401; // enble the counter and set divider
	REG_WRITE32(pcie_apb_addr, 0x6C, 0x100);
#endif
	REG_WRITE32(pcie_apb_addr, 0x70, value);
#else
	REG_WRITE32(pcie_top_base_addr, 0x30, msi_hi_addr);
	REG_WRITE32(pcie_top_base_addr, 0x20, (msi_low_addr & 0xfffff000) | 0x1);
#endif
	PR_TRACE("pcie msi low addr = 0x%x, hi addr = 0x%x, data = 0x%x, chip_id = 0x%x\n",
					 msi_low_addr, msi_hi_addr, msi_data, chip_id);
	PR_TRACE("funtion num = 0x%x\n", function_num);
}

void SGTPUV8_pcie_msi_irq_enable(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	// void __iomem *pcie_top_base_addr;
	// u32 value = 0x0;
	// int count = 300;

	// pcie_top_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + SGTPUV8_OFFSET_PCIE_TOP; //0x5fb80000, offeset = 0x380000

	// while (value == 0) {
	//	value = REG_READ32(pcie_top_base_addr, 0x8);
	//	value &= 0x1;
	//	msleep(1);
	//	if (count-- < 0)
	//		break;
	// }

	// value = REG_READ32(pcie_top_base_addr, 0x4);
	// value |= ((0x1 << 0x0) | (0x1 << 8) | (0x1 << 16) | (0x1 << 24));
	// REG_WRITE32(pcie_top_base_addr, 0x4, value); // enable pcie ep send func0-3 msi0

	// value = REG_READ32(pcie_top_base_addr, 0x10);
	// value |= ((0x7) | (0x7 << 6) | (0x7 << 12) | (0x7 << 18));
	// REG_WRITE32(pcie_top_base_addr, 0x10, value); // enable arm write func0-3 msi0
	// REG_READ32(pcie_top_base_addr, 0x10); // read for workaround for cfg read
	SGTPUV8_pcie_remap_msi_init(pdev, bmdi);
}

void SGTPUV8_enable_intc_irq(struct bm_device_info *bmdi, int irq_num, bool irq_enable)
{
	int irq_enable_offset = 0x0;
	int irq_parent_mask = 0x0;
	int value = 0x0;

	if (irq_num < 0 || irq_num > 192)
	{
		pr_info("bmdrv_enbale_intc_irq irq_num = %d is wrong!\n", irq_num);
		return;
	}

	if (irq_num < 63)
	{
		if (irq_num < 32)
			irq_enable_offset = SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_INTEN_L_OFFSET;
		else
			irq_enable_offset = SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_INTEN_H_OFFSET;
	}
	else if (irq_num < 127)
	{
		irq_parent_mask = 30;
		if (irq_num < 96)
			irq_enable_offset = SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_INTEN_L_OFFSET;
		else
			irq_enable_offset = SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_INTEN_H_OFFSET;
	}
	else
	{
		irq_parent_mask = 31;
		if (irq_num < 160)
			irq_enable_offset = SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_INTEN_L_OFFSET;
		else
			irq_enable_offset = SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_INTEN_H_OFFSET;
	}

	if (irq_num > 63)
	{
		value = intc_reg_read(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_INTEN_H_OFFSET);
		if (irq_enable == true)
			value |= 0x1 << irq_parent_mask;
		else
			value &= (~(0x1 << irq_parent_mask));
		intc_reg_write(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_INTEN_H_OFFSET, value);
	}

	value = intc_reg_read(bmdi, irq_enable_offset);
	if (irq_enable == true)
		value |= (0x1 << (do_div(irq_num, 32)));
	else
		value &= (~(0x1 << (do_div(irq_num , 32)));
	intc_reg_write(bmdi, irq_enable_offset, value);
}

void SGTPUV8_unmaskall_intc_irq(struct bm_device_info *bmdi)
{
	int value = 0x0;
#ifndef MSI_IRQ_FORWARD
	void __iomem *pcie_apb_addr = bmdi->cinfo.bar_info.bar1_vaddr + bmdi->cinfo.bar_info.bar1_part3_offset + 0x20000 + 0x2000; // pcie2 apb
#endif

	intc_reg_write(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_MASK_H_OFFSET, value);
#ifndef MSI_IRQ_FORWARD
	// clear msi mask bit
	REG_WRITE32(pcie_apb_addr, 0x70, REG_READ32(pcie_apb_addr, 0x70) | 0x2);
#endif
}

void SGTPUV8_maskall_intc_irq(struct bm_device_info *bmdi)
{
	int value = 0xffffffff;

	intc_reg_write(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_MASK_H_OFFSET, value);
}

void SGTPUV8_get_irq_status(struct bm_device_info *bmdi, u32 *status)
{
	status[0] = intc_reg_read(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_STATUS_L_OFFSET);
	status[1] = intc_reg_read(bmdi, SGTPUV8_INTC0_BASE_OFFSET + SGTPUV8_INTC_STATUS_H_OFFSET);
	status[2] = intc_reg_read(bmdi, SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_STATUS_L_OFFSET);
	status[3] = intc_reg_read(bmdi, SGTPUV8_INTC1_BASE_OFFSET + SGTPUV8_INTC_STATUS_H_OFFSET);
	status[4] = intc_reg_read(bmdi, SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_STATUS_L_OFFSET);
	status[5] = intc_reg_read(bmdi, SGTPUV8_INTC2_BASE_OFFSET + SGTPUV8_INTC_STATUS_H_OFFSET);
}
