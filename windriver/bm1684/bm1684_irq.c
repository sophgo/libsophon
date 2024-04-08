#include "../bm_common.h"
#include "bm1684_reg.h"
#include "bm1684_irq.tmh"

void bm1684_pcie_msi_irq_disable(struct bm_device_info *bmdi)
{
	u32  *pcie_top_base_addr = NULL;
	u32 value = 0x0;

	pcie_top_base_addr = (u32 *)(bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_TOP);
	value = REG_READ32(pcie_top_base_addr, 0x4);
	value &= ~(0x1 << 0x0);
	REG_WRITE32(pcie_top_base_addr, 0x4, value);
	gp_reg_write_enh(bmdi, GP_REG_MSI_DATA, 0x0);
}

void bm1684_pcie_remap_msi_init(struct bm_device_info *bmdi)
{
	u32  *pcie_top_base_addr = NULL;
	u32  *pcie_iatu_addr = NULL;
	u32  *pcie_apb_addr = NULL;
	u32  *pcie_cfg_addr = NULL;
	u32 value = 0x0;
	u32 msi_low_addr =  0x0;
	u32 msi_hi_addr  =  0x0;
	u32 msi_data = 0x0;
	u32 chip_id = 0x0;
	u32 function_num = 0x0;
    u32             vendor_deviceid    = 0;

	pcie_top_base_addr = (u32 *)(bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_TOP);
	pcie_iatu_addr = (u32 *)(bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU);
	pcie_apb_addr = (u32 *)(bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_APB);
	pcie_cfg_addr = (u32 *)(bmdi->cinfo.bar_info.bar0_vaddr);
    vendor_deviceid    = REG_READ32(pcie_cfg_addr, 0x0);

	chip_id = REG_READ32(pcie_top_base_addr, 0x0);
	chip_id = (chip_id >> 28);
	chip_id &= 0x7;
    if (chip_id > 0) {
        function_num = chip_id - 1;
    }

	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "funtion num = %d, chip_id=%d, vendor_deviceid=0x%x\n",
                function_num,
                chip_id,
                vendor_deviceid);

	if (function_num == 0x0) {
		msi_low_addr =  REG_READ32(pcie_cfg_addr, 0x54);
		msi_hi_addr =  REG_READ32(pcie_cfg_addr, 0x58);
		msi_data =  REG_READ32(pcie_cfg_addr, 0x5c);
	} else {
         	bmdrv_pci_read_confg(bmdi, &msi_low_addr, 0x54, sizeof(u32));
        	bmdrv_pci_read_confg(bmdi, &msi_hi_addr, 0x58, sizeof(u32));
        	bmdrv_pci_read_confg(bmdi, &msi_data, 0x5c, sizeof(u32));
	}

	pcie_cfg_addr = (u32 *)bmdi->cinfo.bar_info.bar0_vaddr;
	value = REG_READ32(pcie_top_base_addr, 0xf040);
	REG_WRITE32(pcie_top_base_addr, 0xf040,  (value | 0xf));
	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "pcie msi low addr = 0x%x, hi addr = 0x%x, data = 0x%x, "
                "chip_id = 0x%x\n",
			msi_low_addr, msi_hi_addr, msi_data, chip_id);
	REG_WRITE32(pcie_top_base_addr, 0xf000, msi_hi_addr);
	REG_WRITE32(pcie_top_base_addr, 0xf004, msi_low_addr);
	REG_WRITE32(pcie_apb_addr, 0x103c, msi_data);

	//PR_TRACE("funtion num = 0x%x\n", function_num);
}

void bm1684_pcie_msi_irq_enable(struct bm_device_info *bmdi)
{
	u32  *pcie_top_base_addr;
	u32 value = 0x0;
	int count = 300;

	pcie_top_base_addr = (u32 *)(bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_TOP); //0x5fb80000, offeset = 0x380000

	while (value == 0) {
		value = REG_READ32(pcie_top_base_addr, 0x8);
		value &= 0x1;
		bm_mdelay(1);
		if (count-- < 0)
			break;
	}

	value = REG_READ32(pcie_top_base_addr, 0x4);
	value |= ((0x1 << 0x0) | (0x1 << 8) | (0x1 << 16) | (0x1 << 24));
	REG_WRITE32(pcie_top_base_addr, 0x4, value); // enable pcie ep send func0-3 msi0

	value = REG_READ32(pcie_top_base_addr, 0x10);
	value |= ((0x7) | (0x7 << 6) | (0x7 << 12) | (0x7 << 18));
	REG_WRITE32(pcie_top_base_addr, 0x10, value); // enable arm write func0-3 msi0
	REG_READ32(pcie_top_base_addr, 0x10); // read for workaround for cfg read
	bm1684_pcie_remap_msi_init(bmdi);
}

void bm1684_enable_intc_irq(struct bm_device_info *bmdi, int irq_num, bool irq_enable)
{
	int irq_enable_offset = 0x0;
	int value = 0x0;

	if (irq_num < 0 || irq_num > 127) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bmdrv_enbale_intc_irq irq_num = %d is wrong!\n",
                    irq_num);
		return;
	}

	if (irq_num < 63) {
		if (irq_num < 32)
			irq_enable_offset = INTC0_BASE_ADDR_OFFSET + IRQ_INTEN_L_OFFSET;
		else
			irq_enable_offset = INTC0_BASE_ADDR_OFFSET + IRQ_INTEN_H_OFFSET;
	} else {
		if (irq_num < 96)
			irq_enable_offset = INTC1_BASE_ADDR_OFFSET + IRQ_INTEN_L_OFFSET;
		else
			irq_enable_offset = INTC1_BASE_ADDR_OFFSET + IRQ_INTEN_H_OFFSET;
	}

	if (irq_num > 63) {
		value = intc_reg_read(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_INTEN_H_OFFSET);
		if (irq_enable == TRUE)
			value |= 0x1 << 31;
		else
			value &= (~(0x1 << 31));
		intc_reg_write(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_INTEN_H_OFFSET, value);
	}

	value = intc_reg_read(bmdi, irq_enable_offset);
	if (irq_enable == TRUE)
		value |= (0x1 << (irq_num % 32));
	else
		value &= (~(0x1 << (irq_num % 32)));
	intc_reg_write(bmdi, irq_enable_offset, value);
}

void bm1684_unmaskall_intc_irq(struct bm_device_info *bmdi)
{
	int value = 0x0;

	intc_reg_write(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_MASK_H_OFFSET, value);
	intc_reg_write(bmdi, INTC1_BASE_ADDR_OFFSET + IRQ_MASK_L_OFFSET, value);
	intc_reg_write(bmdi, INTC1_BASE_ADDR_OFFSET + IRQ_MASK_H_OFFSET, value);
}

void bm1684_get_irq_status(struct bm_device_info *bmdi, u32 *status)
{
	status[0] = intc_reg_read(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_STATUS_L_OFFSET);
	status[1] = intc_reg_read(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_STATUS_H_OFFSET);
	status[2] = intc_reg_read(bmdi, INTC1_BASE_ADDR_OFFSET + IRQ_STATUS_L_OFFSET);
	status[3] = intc_reg_read(bmdi, INTC1_BASE_ADDR_OFFSET + IRQ_STATUS_H_OFFSET);
}
