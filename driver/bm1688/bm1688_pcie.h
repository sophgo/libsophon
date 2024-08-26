#ifndef _BM1688_PCIE_H_
#define _BM1688_PCIE_H_
#include "bm_io.h"

// BAR0
#define BM1688_OFFSET_PCIE_CFG	0x0
#define BM1688_OFFSET_PCIE_iATU	0x300000 
#ifdef PCIE2_EP
#define BM1688_OFFSET_PCIE_TOP	0x7E0000
#define BM1688_OFFSET_PCIE_APB	0x7E2000
#else
#define BM1688_OFFSET_PCIE_TOP	0x3E0000
#define BM1688_OFFSET_PCIE_APB	0x3E3000
#endif

// BAR1
#define BAR1_PART0_OFFSET	0x0
#define BAR1_PART1_OFFSET	0x80000
#define BAR1_PART2_OFFSET	0x380000
#define BAR1_PART3_OFFSET	0x390000
#define BAR1_PART4_OFFSET	0x391000
#define BAR1_PART5_OFFSET	0x392000
#define BAR1_PART6_OFFSET	0x393000
#define BAR1_PART7_OFFSET	0x394000
#define BAR1_PART8_OFFSET	0x395000
#define BAR1_PART9_OFFSET   0x3a5000

void bm1688_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev);
void bm1688_unmap_bar(struct bm_bar_info *bari);
int bm1688_setup_bar_dev_layout(struct bm_device_info *bmdi, BAR_LAYOUT_TYPE type);
void bm1688_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi);
void bm1688_pci_slider_bar4_config_device_addr(struct bm_bar_info *bari, u32 addr);
int bm1688_bmdrv_pcie_get_mode(struct bm_device_info *bmdi);
int bm1688_get_chip_index(struct bm_device_info *bmdi);
int bm1688_bmdrv_pci_bus_scan(struct pci_dev *pdev, struct bm_device_info *bmdi, int max_fun_num);
int bm1688_config_iatu_for_function_x(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari);

#define BM1688_PCIE_DEVICE_ID 0x1686a200
#endif
