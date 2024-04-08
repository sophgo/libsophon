#ifndef _BM1688_CDMA_H_
#define _BM1688_CDMA_H_
#include "bm_cdma.h"

/* CDMA bit definition */
/* CHL_CTRL REG */
#define CDMA_CHL_CTRL_WrQOS_VALUE (0xF<<24)
#define CDMA_CHL_CTRL_RdQOS_VALUE (0xF<<20)
#define CDMA_CHL_CTRL_RdQOS_VALUE_LOW (0xF<<16)
#define CDMA_CHL_CTRL_RdQOS_VALUE_NORMAL (0xF<<12)
#define CDMA_CHL_CTRL_RdQOS_VALUE_HIGH (0xF<<8)
#define CDMA_CHL_CTRL_RdQOS_MODE (0x1<<7)
#define CDMA_CHL_CTRL_IRQ_EN (0x1<<3)  // interrupt output
#define CDMA_CHL_CTRL_DMA_EN (0x1)

/* INT_MAST REG */
#define CDMA_INT_RF_CMD_EPT_MASK (0x1<<3)
#define CDMA_INT_RF_DES_MASK   (0x1)

/* INT_STATUS REG */
#define CDMA_INT_RF_CMD_EPT (0x1<<3)
#define CDMA_INT_RF_DES   (0x1)

/* CMD_FIFO_STATUS REG */
#define CDMA_CMD_FIFO_STATUS_MASK 0xFF

/* MAX_PAYLOAD REG */
#define CDMA_MAX_READ_PAYLOAD (0x7<<3)
#define CDMA_MAX_WRITE_PAYLOAD 0x7

/* CDMA Descriptors */
#define CDMA_CMD_2D_FORMAT_MASK (0x1<<7)
#define CDMA_CMD_2D_FORMAT_BYTE (0x0<<7)
#define CDMA_CMD_2D_FORMAT_WORD (0x1<<7)

#define CDMA_CMD_2D_TRANS_MODE_MASK (0x3<<5)
#define CDMA_CMD_TRANS_DATA_ONLY (0x0<<5)
#define CDMA_CMD_TRANS_WITH_FLUSH (0x1<<5)

#define CDMA_CMD_MODE_STRIDE (0x1<<4)
#define CDMA_CMD_MODE_NORMAL (0x0<<4)
#define CDMA_CMD_IRQ_EN (0x1<<3)
#define CDMA_CMD_DESC_EOD (0x1<<2)
#define CDMA_CMD_DESC_VALID 0x1


#define CDMA_CMD_NORMAL_CFG (CDMA_CMD_MODE_NORMAL | CDMA_CMD_IRQ_EN | \
         CDMA_CMD_DESC_EOD | CDMA_CMD_DESC_VALID)
#define CDMA_CMD_STRIDE_CFG (CDMA_CMD_MODE_STRIDE | CDMA_CMD_IRQ_EN | \
         CDMA_CMD_DESC_EOD | CDMA_CMD_DESC_VALID)

#define CDMA_ADDR_WORD_ALIGN 0xFFFFFFFE //for 2-bytes align

struct bm_device_info;
u32 bm1688_cdma_transfer(struct bm_device_info *bmdi, struct file *file,
		pbm_cdma_arg parg, bool lock_cdma);
void bm1688_clear_cdmairq(struct bm_device_info *bmdi);

int bm1688_dual_cdma_init(struct bm_device_info *bmdi);
int bm1688_dual_cdma_remove(struct bm_device_info *bmdi);
u32 bm1688_dual_cdma_transfer(struct bm_device_info *bmdi, struct file *file,
                        pbm_cdma_arg parg0, pbm_cdma_arg parg1, bool lock_cdma);

#ifdef SOC_MODE
void bm1688_cdma_reset(struct bm_device_info *bmdi);
void bm1688_cdma_clk_enable(struct bm_device_info *bmdi);
void bm1688_cdma_clk_disable(struct bm_device_info *bmdi);
#endif
#endif
