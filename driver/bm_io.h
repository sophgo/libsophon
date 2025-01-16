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
#define REG_READ8(base, off)	ioread8((u8 *)(base + off))
#define REG_READ16(base, off)	ioread16((u16 *)(base + off))
#define REG_READ32(base, off)	ioread32((u32 *)(base + off))

/* General purpose register bit definition */
#define GP_REG_MESSAGE_WP          0
#define GP_REG_MESSAGE_RP          1
#define GP_REG_FW_STATUS             9
#define GP_REG_ARM9_FW_MODE              13
#define GP_REG_FW0_LOG_WP             11
#define GP_REG_FW1_LOG_WP             12


#define GP_REG_MESSAGE_WP_CHANNEL_XPU          GP_REG_MESSAGE_WP
#define GP_REG_MESSAGE_RP_CHANNEL_XPU          GP_REG_MESSAGE_RP
#define GP_REG_MESSAGE_WP_CHANNEL_CPU          6
#define GP_REG_MESSAGE_RP_CHANNEL_CPU          7
#define GP_REG_TEMPSMI                         4

// SGTPUV8 GP registers
// C906_0 -> GP[0,1,9,11,12]; C906_1 -> GP[14,15,23,25,26]
#define GP_REG_C906_FW_MODE              	10
#define GP_REG_TPU1_OFFSET			14
#define GP_REG_PM_OFFSET			27
#define BD_ENGINE_TPU1_OFFSET		       	0x10000UL
#define GDMA_ENGINE_TPU1_OFFSET		       	0x10000UL
#define SHMEM_TPU1_OFFSET		       	0x10000UL

struct bm_io_bar_vaddr {
	void __iomem *dev_info_bar_vaddr;
	void __iomem *smem_bar_vaddr;
	void __iomem *lmem_bar_vaddr;
	void __iomem *top_bar_vaddr;
	void __iomem *gp_bar_vaddr;
	void __iomem *cdma_bar_vaddr;
	void __iomem *pwm_bar_vaddr;
	void __iomem *ddr_bar_vaddr;
	void __iomem *bdc_bar_vaddr;
	void __iomem *smmu_bar_vaddr;
	void __iomem *intc_bar_vaddr;
	void __iomem *cfg_bar_vaddr;
	void __iomem *nv_timer_bar_vaddr;
	void __iomem *wdt_bar_vaddr;
	void __iomem *tpu_bar_vaddr;
	void __iomem *gdma_bar_vaddr;
	void __iomem *hau_bar_vaddr;
	void __iomem *spacc_bar_vaddr;
	void __iomem *pka_bar_vaddr;
	void __iomem *otp_bar_vaddr;
};

struct bm_card_reg {
	u32 dev_info_base_addr;
	u32 smem_base_addr;
	u32 ddr_base_addr;
	u32 lmem_base_addr;
	u32 cfg_base_addr;
	u32 bdc_base_addr;
	u32 cdma_base_addr;
	u32 smmu_base_addr;
	u32 nv_timer_base_addr;
	u32 tpu_base_addr;
	u32 gdma_base_addr;
	u32 gp_base_addr;
};



struct bm_bar_info {
	u64 bar_start[REG_COUNT];                     // address in host I/O memory layout
	u64 bar_dev_start[REG_COUNT];                 // address in device memory layout
	u64 bar_len[REG_COUNT];
	void __iomem *bar_vaddr[REG_COUNT];

	struct bm_io_bar_vaddr io_bar_vaddr;
};

static const struct bm_card_reg bm_reg_SGTPUV8 = {
	.smem_base_addr  = 0x0C041000,	// TPU_smem
	.lmem_base_addr = 0x0C080000,	//tpu_lmem
	.ddr_base_addr  = 0x80000000,
	.bdc_base_addr = 0x0C010000,	//TPU_sys
	.tpu_base_addr  = 0x0C040000,	//TPU
	.gdma_base_addr  = 0x0C000000,	//GDMA
	.gp_base_addr = 0x0C000000,
};

struct bm_device_info;
void __iomem *bm_get_devmem_vaddr(struct bm_device_info *bmdi, u32 address);
u32 bm_read32(struct bm_device_info *bmdi, u32 address);
u32 bm_write32(struct bm_device_info *bmdi, u32 address, u32 data);
u8 bm_read8(struct bm_device_info *bmdi, u32 address);
u32 bm_write8(struct bm_device_info *bmdi, u32 address, u8 data);
u16 bm_read16(struct bm_device_info *bmdi, u32 address);
u32 bm_write16(struct bm_device_info *bmdi, u32 address, u16 data);
u64 bm_read64(struct bm_device_info *bmdi, u32 address);
u64 bm_write64(struct bm_device_info *bmdi, u32 address, u64 data);

void mcu_info_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 mcu_info_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void dev_info_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int len);
u8 dev_info_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void dev_info_write_byte(struct bm_device_info *bmdi, u8 reg_offset, u8 val);
u8 dev_info_read_byte(struct bm_device_info *bmdi, u8 reg_offset);
void lmem_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, u32 channel);
u32 lmem_reg_read(struct bm_device_info *bmdi, u32 reg_offset, u32 channel);
void smem_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, u32 channel);
u32 smem_reg_read(struct bm_device_info *bmdi, u32 reg_offset, u32 channel);
void top_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 top_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void gp_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 gp_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void pwm_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 pwm_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void cdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 cdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void ddr_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 ddr_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void bdc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 bdc_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void smmu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 smmu_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void intc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 intc_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void cfg_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 cfg_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void nv_timer_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 nv_timer_reg_read(struct bm_device_info *bmdi, u32 reg_offset);

void tpu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 tpu_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void tpu_reg_write_idx(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int core_id);
u32 tpu_reg_read_idx(struct bm_device_info *bmdi, u32 reg_offset, int core_id);
void gdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 gdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void gdma_reg_write_idx(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int core_id);
u32 gdma_reg_read_idx(struct bm_device_info *bmdi, u32 reg_offset, int core_id);
void hau_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 hau_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void spacc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 spacc_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void pka_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 pka_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
u32 otp_reg_read(struct bm_device_info *bmdi, u32 reg_offset);

static inline void gp_reg_write_idx(struct bm_device_info *bmdi, u32 idx, u32 data, int core_id)
{
	gp_reg_write(bmdi, (idx + core_id * GP_REG_TPU1_OFFSET) * 4, data);
}

static inline u32 gp_reg_read_idx(struct bm_device_info *bmdi, u32 idx, int core_id)
{
	return gp_reg_read(bmdi, (idx + core_id * GP_REG_TPU1_OFFSET) * 4);
}

static inline void gp_reg_write_enh(struct bm_device_info *bmdi, u32 idx, u32 data)
{
	gp_reg_write(bmdi, idx * 4, data);
}

static inline u32 gp_reg_read_enh(struct bm_device_info *bmdi, u32 idx)
{
	return gp_reg_read(bmdi, idx * 4);
}

static inline void shmem_reg_write_enh(struct bm_device_info *bmdi, u32 idx, u32 data, u32 channel, int core_id)
{
	lmem_reg_write(bmdi, idx * 4 + core_id * SHMEM_TPU1_OFFSET, data, channel);
}

static inline u32 shmem_reg_read_enh(struct bm_device_info *bmdi, u32 idx, u32 channel, int core_id)
{
	return lmem_reg_read(bmdi, idx * 4 + core_id * SHMEM_TPU1_OFFSET, channel);
}

/* DDR read/write enhance: operate on specified ddr controller region */
#define DDR_REGION_SIZE 0x1000

static inline void ddr_reg_write_enh(struct bm_device_info *bmdi, u32 ddr_ctrl, u32 reg_offset, u32 val)
{
	ddr_reg_write(bmdi, DDR_REGION_SIZE * ddr_ctrl + reg_offset, val);
}

static inline u32 ddr_reg_read_enh(struct bm_device_info *bmdi, u32 ddr_ctrl, u32 reg_offset)
{
	return ddr_reg_read(bmdi, DDR_REGION_SIZE * ddr_ctrl + reg_offset);
}

void io_init(struct bm_device_info *bmdi);

void bm_reg_init_vaddr(struct bm_device_info *bmdi, u32 address, void __iomem **reg_base_vaddr);
int bm_set_reg(struct bm_device_info *bmdi, struct bm_reg *reg);
int bm_get_reg(struct bm_device_info *bmdi, struct bm_reg *reg);
#endif
