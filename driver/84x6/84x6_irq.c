#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include "bm_common.h"
#include "bm_card.h"
#include "84x6_reg.h"
#include "84x6_pcie.h"
#include "84x6_irq.h"

static const int intc_bases[] = {
    BM84X6_INTC0_BASE_OFFSET,
    BM84X6_INTC1_BASE_OFFSET,
    BM84X6_INTC2_BASE_OFFSET
};

static const int intc_offsets[] = {
    BM84X6_INTC_INTEN_L_OFFSET,
    BM84X6_INTC_INTEN_H_OFFSET
};

void bm84x6_pcie_msi_irq_disable(struct bm_device_info *bmdi)
{
    void __iomem *msi_ctrl_base =
        bmdi->cinfo.bar_info.bar1_vaddr +
        bmdi->cinfo.bar_info.bar1_part_info[4].offset;

    REG_WRITE32(msi_ctrl_base, 0, REG_READ32(msi_ctrl_base, 0) & ~0x1);
}

//#define MSI_TIMEOUT_COUNTER

void bm84x6_pcie_msi_irq_enable(struct pci_dev *pdev,
                                struct bm_device_info *bmdi)
{
    struct bm_card *bmcd = bmdrv_card_get_bm_card(bmdi);
    void __iomem *pcie_cfg_base = bmdi->cinfo.bar_info.bar0_vaddr;
    void __iomem *msi_ctrl_base =
        bmdi->cinfo.bar_info.bar1_vaddr +
        bmdi->cinfo.bar_info.bar1_part_info[4].offset;
    void __iomem *pcie_atu_base;
    u32 msi_addr_l, msi_addr_h, msi_data, value;
    u32 function_num = bmdi->cinfo.pcie_func_index;
    u32 offset;

    // Write to REF control and status register to enable memory and IO accesses
	// Config write TLPs are triggered via AXI writes to region 0 in DUT core axi wrapper
    value = REG_READ32(pcie_cfg_base, 0x4);
    value |= 0x7;
    REG_WRITE32(pcie_cfg_base, 0x4, value);

	// config PCI_MSI_ENABLE
	value = REG_READ32(pcie_cfg_base, 0x50);
	value |= 0x10000;
	REG_WRITE32(pcie_cfg_base, 0x50, value);

    if ((bmcd != NULL) && (bmcd->card_bmdi[0] != NULL)) {
        pcie_cfg_base = bmcd->card_bmdi[0]->cinfo.bar_info.bar0_vaddr;
        pcie_cfg_base += (function_num << 16);
        msi_addr_l = REG_READ32(pcie_cfg_base, 0x54);
        msi_addr_h = REG_READ32(pcie_cfg_base, 0x58);
        msi_data = REG_READ32(pcie_cfg_base, 0x5c);
    } else {
        pci_read_config_dword(pdev, 0x54, &msi_addr_l);
        pci_read_config_dword(pdev, 0x58, &msi_addr_h);
        pci_read_config_dword(pdev, 0x5c, &msi_data);
    }

    if ((bmcd != NULL) && (bmcd->card_bmdi[0] != NULL))
        pcie_atu_base = bmcd->card_bmdi[0]->cinfo.bar_info.bar0_vaddr + BM84X6_OFFSET_PCIE_iATU;
    else
        pcie_atu_base = pcie_cfg_base + BM84X6_OFFSET_PCIE_iATU;

    offset = function_num * 0x200;
    REG_WRITE32(pcie_atu_base, offset + 0x08, 0x0);    //src addr
    REG_WRITE32(pcie_atu_base, offset + 0x0C, (0x50UL | function_num) | (function_num << 14) | (0x1UL << 13) | (0x7UL << 17));
    REG_WRITE32(pcie_atu_base, offset + 0x10, 0xffffffff);
    REG_WRITE32(pcie_atu_base, offset + 0x14, msi_addr_l & 0xfffff000); //dst addr
    REG_WRITE32(pcie_atu_base, offset + 0x18, msi_addr_h);
    REG_WRITE32(pcie_atu_base, offset + 0x20, 0);
    REG_WRITE32(pcie_atu_base, offset + 0x00, function_num << 20);
    REG_WRITE32(pcie_atu_base, offset + 0x04, 0x80000000 | (function_num << 8));

    //clear all msi mask bit
    REG_WRITE32(msi_ctrl_base, 0x18, 0xffffffff);
    //keep msi address
    msi_addr_h = 0x50 | function_num;
    msi_addr_h |= (0x1U << 21);        // msi: bit53
    msi_addr_h |= (function_num << 22);    // func_num: bit[56:54]
    msi_addr_h |= (0x7U << 25);        // dst chip_id: bit[59:57]
    msi_addr_h |= (0x1 << 28);         // cascade route: bit[63:60] x8_ctrl 0x1, x4_ctrl 0x3.
    REG_WRITE32(msi_ctrl_base, 0xc, msi_addr_l & 0xfff);
    REG_WRITE32(msi_ctrl_base, 0x10, msi_addr_h);
    //keep msi user data
    REG_WRITE32(msi_ctrl_base, 0x14, msi_data);

    REG_WRITE32(msi_ctrl_base, 0, 0x2645);
}

void bm84x6_enable_intc_irq(struct bm_device_info *bmdi, int irq_num,
                            bool irq_enable)
{
    int intc_ctrl;
    int value = 0x0;
    unsigned long flag;

    if (irq_num < 0 || irq_num > 192) {
        pr_info("bmdrv_enbale_intc_irq irq_num = %d is wrong!\n", irq_num);
        return;
    }

    intc_ctrl = intc_bases[irq_num / 64] + intc_offsets[(irq_num / 32) % 2];

    spin_lock_irqsave(&bmdi->irq_lock, flag);

    value = intc_reg_read(bmdi, intc_ctrl);

    if (irq_enable == true)
        value |= (0x1 << (irq_num % 32));
    else
        value &= (~(0x1 << (irq_num % 32)));
    intc_reg_write(bmdi, intc_ctrl, value);

    spin_unlock_irqrestore(&bmdi->irq_lock, flag);
}

void bm84x6_unmaskall_intc_irq(struct bm_device_info *bmdi)
{
    int value = 0x0;
    void __iomem *msi_ctrl_base =
        bmdi->cinfo.bar_info.bar1_vaddr +
        bmdi->cinfo.bar_info.bar1_part_info[4].offset;

    intc_reg_write(bmdi, BM84X6_INTC0_BASE_OFFSET + BM84X6_INTC_MASK_L_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC0_BASE_OFFSET + BM84X6_INTC_MASK_H_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC1_BASE_OFFSET + BM84X6_INTC_MASK_L_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC1_BASE_OFFSET + BM84X6_INTC_MASK_H_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC2_BASE_OFFSET + BM84X6_INTC_MASK_L_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC2_BASE_OFFSET + BM84X6_INTC_MASK_H_OFFSET,
                   value);

    // clear msi mask bit
    REG_WRITE32(msi_ctrl_base, 0x18, 0x1);
}

void bm84x6_maskall_intc_irq(struct bm_device_info *bmdi)
{
    int value = 0xffffffff;

    intc_reg_write(bmdi, BM84X6_INTC0_BASE_OFFSET + BM84X6_INTC_MASK_L_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC0_BASE_OFFSET + BM84X6_INTC_MASK_H_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC1_BASE_OFFSET + BM84X6_INTC_MASK_L_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC1_BASE_OFFSET + BM84X6_INTC_MASK_H_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC2_BASE_OFFSET + BM84X6_INTC_MASK_L_OFFSET,
                   value);
    intc_reg_write(bmdi, BM84X6_INTC2_BASE_OFFSET + BM84X6_INTC_MASK_H_OFFSET,
                   value);
}

void bm84x6_get_irq_status(struct bm_device_info *bmdi, u32 * status)
{
    status[0] =
        intc_reg_read(bmdi,
                      BM84X6_INTC0_BASE_OFFSET + BM84X6_INTC_STATUS_L_OFFSET);
    status[1] =
        intc_reg_read(bmdi,
                      BM84X6_INTC0_BASE_OFFSET + BM84X6_INTC_STATUS_H_OFFSET);
    status[2] =
        intc_reg_read(bmdi,
                      BM84X6_INTC1_BASE_OFFSET + BM84X6_INTC_STATUS_L_OFFSET);
    status[3] =
        intc_reg_read(bmdi,
                      BM84X6_INTC1_BASE_OFFSET + BM84X6_INTC_STATUS_H_OFFSET);
    status[4] =
        intc_reg_read(bmdi,
                      BM84X6_INTC2_BASE_OFFSET + BM84X6_INTC_STATUS_L_OFFSET);
    status[5] =
        intc_reg_read(bmdi,
                      BM84X6_INTC2_BASE_OFFSET + BM84X6_INTC_STATUS_H_OFFSET);
}
