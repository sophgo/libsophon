#ifndef _BM_IO_H_
#define _BM_IO_H_
#define REG_COUNT 5
typedef enum {
	MODE_CHOSE_LAYOUT = 0,
	SETUP_BAR_DEV_LAYOUT = 1
} BAR_LAYOUT_TYPE;

#define REG_WRITE8(base, off, val)	iowrite8((val), (u8 *)(base + off))
#define REG_WRITE16(base, off, val) iowrite16((val), (u16 *)(base + off))
//#define REG_WRITE32(base, off, val) iowrite32((val), (u32 *)(base + off))
#define REG_WRITE32(base, off, val) \
{\
	iowrite32((val), (u32 *)(base + off)); \
	ioread32((u32 *)(base + off)); \
}
#include "bm_uapi.h"
#define OTP_SHADOW_SYS_SIZE 0x100
#define REG_READ8(base, off)	ioread8((u8 *)(base + off))
#define REG_READ16(base, off)	ioread16((u16 *)(base + off))
#define REG_READ32(base, off)	ioread32((u32 *)(base + off))

// SGTPUV8 GP registers
// C906_0 -> GP[0,1,9,11,12]; C906_1 -> GP[14,15,23,25,26]
#define GP_REG_TPU1_OFFSET			14
#define GP_REG_PM_OFFSET			27
#define BD_ENGINE_TPU1_OFFSET		       	0x10000UL
#define GDMA_ENGINE_TPU1_OFFSET		       	0x10000UL

struct bm_io_bar_vaddr {
	void __iomem *dev_info_bar_vaddr;
	void __iomem *top_bar_vaddr;
	void __iomem *gp_bar_vaddr;
	void __iomem *ddr_bar_vaddr;
	void __iomem *intc_bar_vaddr;
	void __iomem *tpu_bar_vaddr;
	void __iomem *otp_bar_vaddr;
};

struct bm_card_reg {
	u32 dev_info_base_addr;
	u32 ddr_base_addr;
	u32 tpu_base_addr;
	u32 gp_base_addr;
	u32 otp_bar_aaddr;
};



struct bm_bar_info {
	u64 bar_start[REG_COUNT];                     // address in host I/O memory layout
	u64 bar_dev_start[REG_COUNT];                 // address in device memory layout
	u64 bar_len[REG_COUNT];
	void __iomem *bar_vaddr[REG_COUNT];

	struct bm_io_bar_vaddr io_bar_vaddr;
};

static const struct bm_card_reg bm_reg_SGTPUV8 = {
	.ddr_base_addr  = 0x80000000,
	.gp_base_addr = 0x0C000000,
	.otp_bar_aaddr = 0x03000000,
};

struct bm_device_info;
void __iomem *bm_get_devmem_vaddr(struct bm_device_info *bmdi, u32 address);


u32 top_reg_read(struct bm_device_info *bmdi, u32 reg_offset);

u32 otp_reg_read(struct bm_device_info *bmdi, u32 reg_offset);


void io_init(struct bm_device_info *bmdi);

#endif
