#ifndef __BM84X6_IRQ_H__
#define __BM84X6_IRQ_H__

#define BM84X6_INTC0_BASE_OFFSET  0x0
#define BM84X6_INTC1_BASE_OFFSET  0x1000
#define BM84X6_INTC2_BASE_OFFSET  0x2000

#define BM84X6_INTC_OFFSET  0x1000

#define BM84X6_INTC_INTEN_L_OFFSET      0x0
#define BM84X6_INTC_INTEN_H_OFFSET      0x4
#define BM84X6_INTC_MASK_L_OFFSET       0x8
#define BM84X6_INTC_MASK_H_OFFSET       0xc
#define BM84X6_INTC_STATUS_L_OFFSET     0x20
#define BM84X6_INTC_STATUS_H_OFFSET     0x24

#define BM84X6_GPIO_IRQ_ID     68

#define BM84X6_VETH_IRQ_ID    182

#define BM84X6_CDMA0_IRQ_ID    114
#define BM84X6_CDMA1_IRQ_ID    115

#define BM84X6_TSH_IRQ_ID      83

#ifndef SOC_MODE
struct bm_device_info;

void bm84x6_unmaskall_intc_irq(struct bm_device_info *bmdi);
void bm84x6_maskall_intc_irq(struct bm_device_info *bmdi);

void bm84x6_pcie_msi_irq_enable(struct pci_dev *pdev,
                                struct bm_device_info *bmdi);
void bm84x6_pcie_msi_irq_disable(struct bm_device_info *bmdi);
void bm84x6_enable_intc_irq(struct bm_device_info *bmdi, int irq_num,
                            bool irq_enable);
void bm84x6_get_irq_status(struct bm_device_info *bmdi, unsigned int *status);
#endif

#endif /* __BM84X6_IRQ_H__ */
