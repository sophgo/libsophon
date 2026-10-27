#ifndef _BM84X6_PCIE_H_
#define _BM84X6_PCIE_H_
#include "bm_io.h"


#define BM84X6_PCIE_MODE_SHIFT 21
#define BM84X6_PCIE_MODE_MASK 0x3FF
#define BM84X6_PCIE_EP_SEL_MASK 0x3
#define BM84X6_PCIE_EP_SEL_SHIFT 4
#define BM84X6_PCIE_FUNC_NUM_SHIFT 6

#define BM84X6_PCIE_DBI_X8_0 0x20000000
#define BM84X6_PCIE_DBI_X2_0 0x20400000
#define BM84X6_PCIE_DBI_X4_0 0x20800000
#define BM84X6_PCIE_DBI_X2_1 0x20C00000

#define BM84X6_PCIE_TRAP_REG 0x28100004

/* trap boot_sel field [20:18]; 0x5 == boot_from_pcie (load c2c/ddr fw) */
#define BM84X6_PCIE_BOOT_SEL_SHIFT 18
#define BM84X6_PCIE_BOOT_SEL_MASK  0x7
#define BM84X6_PCIE_BOOT_FROM_PCIE 0x5

#define BM84X6_CHAIN_REGMAP_CTRL 0x21790400
#define BM84X6_PORT_CODE_LIST_X8_0 0x21790400
#define BM84X6_PORT_CODE_LIST_X4_0 0x2179302c
#define BM84X6_PORT_CODE_LIST_X2_0 0x21793030
#define BM84X6_PORT_CODE_LIST_X2_1 0x21793034

#define BM84X6_PCIE_EP_SEL_x8_0 0
#define BM84X6_PCIE_EP_SEL_x2_0 1
#define BM84X6_PCIE_EP_SEL_x4_0 2
#define BM84X6_PCIE_EP_SEL_x2_1 3

// BAR0
#define BM84X6_OFFSET_PCIE_CFG	0x0
#define BM84X6_OFFSET_PCIE_iATU	0x300000

#define BM84X6_PCIE_STATUS_OFFSET	0xd0
#define BM84X6_PCIE_STATUS_REG		0x281000d0	/* SYS_CTRL_BASE + BM84X6_PCIE_STATUS_OFFSET */
#define BM84X6_PCIE_INIT_START		(0x1 << 0)	/* write 1 to kick off DDR/C2C init flow */
#define BM84X6_PCIE_DDR_INITIALIZED	(0x1 << 17)	/* read 1 => init flow completed */

// C2C top register base
#define C2C_TOP_REG_BASE                                    0x21790000

#define BM84X6_EP_BASE          0x4600000000000ULL
#define BM84X6_SLOT_STRIDE      0x2600000000000ULL
#define BM84X6_BAR1_OFFSET      0x400000ULL
#define BM84X6_BAR4_OFFSET      0x400000000000ULL
#define BM84X6_RC_WINDOW        0x400000ULL

// C2C outbound ATU
#define C2C_OB_ATU_ADDR_UP_0                                0x200
#define C2C_OB_ATU_ADDR_LOW_0                               0x204
#define C2C_OB_ATU_CTRL_0                                   0x208
#define C2C_OB_ATU_ADDR_UP(atu_id)  (C2C_OB_ATU_ADDR_UP_0 + ((atu_id) * 0x10))
#define C2C_OB_ATU_ADDR_LOW(atu_id) (C2C_OB_ATU_ADDR_LOW_0 + ((atu_id) * 0x10))
#define C2C_OB_ATU_CTRL(atu_id)     (C2C_OB_ATU_CTRL_0 + ((atu_id) * 0x10))

// C2C outbound ATU to PC
#define C2C_OB_ATU_PC_ADDR_UP_0                             0xA00
#define C2C_OB_ATU_PC_ADDR_LOW_0                            0xA04
#define C2C_OB_ATU_PC_CTRL_0                                0xA08
#define C2C_OB_ATU_PC_ADDR_UP(atu_id)  (C2C_OB_ATU_PC_ADDR_UP_0 + ((atu_id) * 0x10))
#define C2C_OB_ATU_PC_ADDR_LOW(atu_id) (C2C_OB_ATU_PC_ADDR_LOW_0 + ((atu_id) * 0x10))
#define C2C_OB_ATU_PC_CTRL(atu_id)     (C2C_OB_ATU_PC_CTRL_0 + ((atu_id) * 0x10))

// C2C inbound ATU
#define C2C_IB_ATU_ADDR_UP_0                                0xC00
#define C2C_IB_ATU_ADDR_LOW_0                               0xC04
#define C2C_IB_ATU_CTRL_0                                   0xC08
#define C2C_IB_ATU_DST_ADDR_0                               0xC0C
#define C2C_IB_ATU_ADDR_UP(atu_id)  (C2C_IB_ATU_ADDR_UP_0 + ((atu_id) * 0x10))
#define C2C_IB_ATU_ADDR_LOW(atu_id) (C2C_IB_ATU_ADDR_LOW_0 + ((atu_id) * 0x10))
#define C2C_IB_ATU_CTRL(atu_id)     (C2C_IB_ATU_CTRL_0 + ((atu_id) * 0x10))
#define C2C_IB_ATU_DST_ADDR(atu_id) (C2C_IB_ATU_DST_ADDR_0 + ((atu_id) * 0x10))
#define C2C_IB_ATU_SEL										0x2614

// snps outbound atu
#define PCIE_OB_IATU_REGION_CTRL_1_0        0x000
#define PCIE_OB_IATU_REGION_CTRL_2_0        0x004
#define PCIE_OB_IATU_LWR_BASE_ADDR_0        0x008
#define PCIE_OB_IATU_UPPER_BASE_ADDR_0      0x00C
#define PCIE_OB_IATU_LIMIT_ADDR_0           0x010
#define PCIE_OB_IATU_LWR_TARGET_ADDR_0      0x014
#define PCIE_OB_IATU_UPPER_TARGET_ADDR_0    0x018
#define PCIE_OB_IATU_UPPR_LIMIT_ADDR_0      0x020
#define PCIE_OB_IATU_REGION_CTRL_1(id)       (PCIE_OB_IATU_REGION_CTRL_1_0 + ((id) * 0x200))
#define PCIE_OB_IATU_REGION_CTRL_2(id)       (PCIE_OB_IATU_REGION_CTRL_2_0 + ((id) * 0x200))
#define PCIE_OB_IATU_LWR_BASE_ADDR(id)       (PCIE_OB_IATU_LWR_BASE_ADDR_0 + ((id) * 0x200))
#define PCIE_OB_IATU_UPPER_BASE_ADDR(id)     (PCIE_OB_IATU_UPPER_BASE_ADDR_0 + ((id) * 0x200))
#define PCIE_OB_IATU_LIMIT_ADDR(id)          (PCIE_OB_IATU_LIMIT_ADDR_0 + ((id) * 0x200))
#define PCIE_OB_IATU_LWR_TARGET_ADDR(id)     (PCIE_OB_IATU_LWR_TARGET_ADDR_0 + ((id) * 0x200))
#define PCIE_OB_IATU_UPPER_TARGET_ADDR(id)   (PCIE_OB_IATU_UPPER_TARGET_ADDR_0 + ((id) * 0x200))
#define PCIE_OB_IATU_UPPR_LIMIT_ADDR(id)     (PCIE_OB_IATU_UPPR_LIMIT_ADDR_0 + ((id) * 0x200))

// snps inbound atu
#define PCIE_IB_IATU_REGION_CTRL_1_1        0x300
#define PCIE_IB_IATU_REGION_CTRL_2_1        0x304
#define PCIE_IB_IATU_LWR_BASE_ADDR_1        0x308
#define PCIE_IB_IATU_UPPER_BASE_ADDR_1      0x30C
#define PCIE_IB_IATU_LIMIT_ADDR_1           0x310
#define PCIE_IB_IATU_LWR_TARGET_ADDR_1      0x314
#define PCIE_IB_IATU_UPPER_TARGET_ADDR_1    0x318
#define PCIE_IB_IATU_UPPR_LIMIT_ADDR_1      0x320
#define PCIE_IB_IATU_REGION_CTRL_1(id)       (PCIE_IB_IATU_REGION_CTRL_1_1 + ((id) * 0x200))
#define PCIE_IB_IATU_REGION_CTRL_2(id)       (PCIE_IB_IATU_REGION_CTRL_2_1 + ((id) * 0x200))
#define PCIE_IB_IATU_LWR_BASE_ADDR(id)       (PCIE_IB_IATU_LWR_BASE_ADDR_1 + ((id) * 0x200))
#define PCIE_IB_IATU_UPPER_BASE_ADDR(id)     (PCIE_IB_IATU_UPPER_BASE_ADDR_1 + ((id) * 0x200))
#define PCIE_IB_IATU_LIMIT_ADDR(id)          (PCIE_IB_IATU_LIMIT_ADDR_1 + ((id) * 0x200))
#define PCIE_IB_IATU_LWR_TARGET_ADDR(id)     (PCIE_IB_IATU_LWR_TARGET_ADDR_1 + ((id) * 0x200))
#define PCIE_IB_IATU_UPPER_TARGET_ADDR(id)   (PCIE_IB_IATU_UPPER_TARGET_ADDR_1 + ((id) * 0x200))
#define PCIE_IB_IATU_UPPR_LIMIT_ADDR(id)     (PCIE_IB_IATU_UPPR_LIMIT_ADDR_1 + ((id) * 0x200))

void bm84x6_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev);
void bm84x6_unmap_bar(struct bm_device_info *bmdi);
int bm84x6_setup_bar_dev_layout(struct bm_device_info *bmdi,
                                BAR_LAYOUT_TYPE type);
void bm84x6_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi);
void bm84x6_pci_slider_bar4_config_device_addr(struct bm_bar_info *bari,
                                               u32 addr);
int bm84x6_pcie_get_mode(struct bm_device_info *bmdi);
int bm84x6_config_iatu_for_function_x(struct pci_dev *pdev,
                                      struct bm_device_info *bmdi,
                                      struct bm_bar_info *bari);
int bm84x6_c2c_post_load(struct bm_device_info *bmdi);
#define BM84X6_PCIE_DEVICE_ID BM_CHIP_ID_84X6
#endif /* _BM84X6_PCIE_H_ */
