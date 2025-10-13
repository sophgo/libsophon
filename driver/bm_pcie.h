#ifndef _BM_PCIE_H_
#define _BM_PCIE_H_
#include "bm1682_pcie.h"
#include "bm1684_pcie.h"
/*BM1684 PCIe end device info*/

/**
 * The vendor id of PCIe EP device
 */
#define BITMAIN_VENDOR_ID	0x1e30
#define SOPHGO_VENDOR_ID	0x1f1c

/**
 * The vendor id of PCIe EP device of sysnopsys
 */
#define SYNOPSYS_VENDOR_ID	0x16c3

/**
 * The device id of PCIe EP device
 */
#define BM1684_DEVICE_ID	0x1684
#define BM1682_DEVICE_ID	0x1682
#define BM1684X_DEVICE_ID	0x1686

#define BM_PCIE_MAX_CHIP_NUM 128

#define BOARD_TYPE_EVB       0x0
#define BOARD_TYPE_SA5       0x1
#define BOARD_TYPE_SC5       0x2
#define BOARD_TYPE_SE5       0x3
#define BOARD_TYPE_SM5_P     0x4
#define BOARD_TYPE_SM5_S     0x5
#define BOARD_TYPE_SA6       0x6
#define BOARD_TYPE_SC5_PLUS  0x7
#define BOARD_TYPE_SC5_H     0x8
#define BOARD_TYPE_SC5_PRO   0x9
#define BOARD_TYPE_AIV01T    0x10
#define BOARD_TYPE_AIV02T    0x11
#define BOARD_TYPE_AIV03T    0x12
#define BOARD_TYPE_AIV03T_24G    0x13

#define BOARD_TYPE_SM5M_P    0xb
#define BOARD_TYPE_BM1684X_EVB    0x20
#define BOARD_TYPE_SC7_PRO   0x21
#define BOARD_TYPE_SC7_PLUS  0x22
#define BOARD_TYPE_SC7_FP150  0x23
#define BOARD_TYPE_SC7_HP75_1  0x24
#define BOARD_TYPE_SM7_V0_0	 0x30
#define BOARD_TYPE_SM7_MP1_1	 0x36
#define BOARD_TYPE_CP24		 0x40
#define BOARD_TYPE_SM7_CUST_V2	 0x3b
#define BOARD_TYPE_AIV01X	 0x50
#define BOARD_TYPE_AIV02X	 0x51
#define BOARD_TYPE_AIV03X	 0x52

#define DUMMY_PCIDEV_NAME	"dummy-bmcard-pci"

int config_iatu_for_function_x(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari);
int bmdrv_pci_bus_scan(struct pci_dev *pdev, struct bm_device_info *bmdi, int cfg_num);
int bmdrv_pcie_get_EP_RC(struct bm_device_info *bmdi);
int bmdrv_get_chip_num(struct bm_device_info *bmdi);
int bmdrv_pcie_get_mode(struct bm_device_info *bmdi);

struct bm_pcie_record {
	int domain_bdf;
	int dev_index;
	int inited;
};

#endif
