#ifndef _VPP_K_H_
#define _VPP_K_H_
//#include "../bm_common.h"
#include <linux/vpss_uapi.h>
#include "vpss.h"
#include "vpss_proc.h"

typedef unsigned char u8;
typedef unsigned long long u64;
typedef unsigned int uint32;


#define SW_RESET_REG0 0x50010c00
#define SW_RESET_REG1 0x50010c04
#define VPP_CORE0_RST 22

#define SW_RESET_REG2 0x50010c08
#define VPP_CORE1_RST 0

/*vpp global control*/
#define VPP_VERSION           (0x100)
#define VPP_CONTROL0          (0x110)
#define VPP_CMD_BASE          (0x114)
#define VPP_CMD_BASE_EXT      (0x118)
#define VPP_STATUS            (0x11C)
#define VPP_INT_EN            (0x120)
#define VPP_INT_CLEAR         (0x124)
#define VPP_INT_STATUS        (0x128)
#define VPP_INT_RAW_STATUS    (0x12c)


/*vpp descriptor for DMA CMD LIST*/


#define VPP_CROP_NUM_MAX (512)
#define VPP1688_CORE_MAX (10)
#define Transaction_Mode (2)

#define REG_VPSS_V_BASE 0x68080000
#define REG_VPSS_T1_BASE 0x23020000
#define REG_VPSS_T2_BASE 0x24020000
#define REG_VPSS_D_BASE 0x67010000

enum VPP1688_IRQ {
  VPP0_IRQ_ID = 23,
  VPP1_IRQ_ID,
  VPP2_IRQ_ID,
  VPP3_IRQ_ID,
  VPP8_IRQ_ID = 31,
  VPP9_IRQ_ID,
  VPP4_IRQ_ID = 40,
  VPP5_IRQ_ID,
  VPP6_IRQ_ID = 43,
  VPP7_IRQ_ID,
};

struct vpp_reset_info {
	u32 reg;
	u32 bit_n;
};

struct vpp_batch_n {
	bm_vpss_cfg *cmd;
};

extern int bmdev_memcpy_d2s(struct bm_device_info *bmdi,struct file *file,
		void __user *dst, u64 src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);
void vpss_reg_write(uintptr_t addr, u32 data);
u32 vpss_reg_read(uintptr_t addr);
#endif

