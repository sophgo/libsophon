#ifndef __BM1688_IRQ_H
#define __BM1688_IRQ_H

#define BM1688_INTC0_BASE_OFFSET  0x0
#define BM1688_INTC1_BASE_OFFSET  0x1000
#define BM1688_INTC2_BASE_OFFSET  0x2000
#define BM1688_INTC_INTEN_L_OFFSET      0x0
#define BM1688_INTC_INTEN_H_OFFSET      0x4
#define BM1688_INTC_MASK_L_OFFSET       0x8
#define BM1688_INTC_MASK_H_OFFSET       0xc
#define BM1688_INTC_STATUS_L_OFFSET     0x20
#define BM1688_INTC_STATUS_H_OFFSET     0x24

#define BM1688_GPIO_IRQ_ID     147
#define BM1688_VPP0_IRQ_ID     30
#define BM1688_VPP1_IRQ_ID     114
#define BM1688_PKA_IRQ_ID      17
#define BM1688_SPACC_IRQ_ID    18

#define BM1688_READ_ARM9_ID    51

#define BM1688_CDMA0_IRQ_ID    88
#define BM1688_CDMA1_IRQ_ID    89

#define TOP_GP_REG_C906_IRQ_STATUS_OFFSET   0xf8
#define TOP_GP_REG_C906_IRQ_SET_OFFSET      0xf8
#define TOP_GP_REG_C906_IRQ_CLEAR_OFFSET    0x78
#define BM1688_GP_REG_A53_IRQ_STATUS_OFFSET    0xfc
#define BM1688_GP_REG_A53_IRQ_SET_OFFSET       0xfc
#define BM1688_GP_REG_A53_IRQ_CLEAR_OFFSET     0x7c
#define MSG_DONE_A53_IRQ_MASK_0             (0x1)       // GP31[0]
#define MSG_DONE_A53_IRQ_MASK_1             (0x1 << 1)  // GP31[1]
#define MSG_DONE_XPU_IRQ_MASK_0             (0x1 << 12) // GP30[12]
#define MSG_DONE_XPU_IRQ_MASK_1             (0x1 << 13) // GP30[13]
#define MSG_DONE_CPU_IRQ_MASK               (0x1 << 14) // GP30[14]

#ifndef SOC_MODE
struct bm_device_info;
void bm1688_unmaskall_intc_irq(struct bm_device_info *bmdi);
void bm1688_maskall_intc_irq(struct bm_device_info *bmdi);

void bm1688_pcie_msi_irq_enable(struct pci_dev *pdev, struct bm_device_info *bmdi);
void bm1688_pcie_msi_irq_disable(struct bm_device_info *bmdi);
void bm1688_enable_intc_irq(struct bm_device_info *bmdi, int irq_num, bool irq_enable);
void bm1688_get_irq_status(struct bm_device_info *bmdi, unsigned int *status);
#endif

#endif
