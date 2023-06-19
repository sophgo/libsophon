#ifndef __CDMA_REG_PARSE_H__
#define __CDMA_REG_PARSE_H__
#include "cdma_reg_def.h"
#include "gen_cmd.h"

typedef struct {
  u32 pio_mode;
  u32 intr_en;
  u32 dst_addr_hi;
  u32 src_addr_hi;
  u32 dst_addr_lo;
  u32 src_addr_lo;
  u32 dma_amount;
  u32 cmd_id;
  u32 eng0_id;
  u32 eng1_id;
  u32 eng2_id;
  u32 misc_enable;
  u32 misc_select;
  u32 misc_min_max;
  u32 misc_index_enable;
  u32 misc_auto_index;
  u32 misc_input_addr_lo;
  u32 misc_input_addr_hi;
  u32 misc_output_addr_lo;
  u32 misc_output_addr_hi;
  u32 misc_cnt;
} cdma_des_t;

static inline void parse_cdma_des_struct(const P_COMMAND cmd, cdma_des_t *des) {
  des->pio_mode = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MODE_SELECT);
  des->intr_en = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_INT_DISABLE);
  des->dst_addr_hi = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_DST_ADDR_H);
  des->dst_addr_lo = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_DST_ADDR_L);
  des->src_addr_hi = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_SRC_ADDR_H);
  des->src_addr_lo = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_SRC_ADDR_L);
  des->dma_amount = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_DMA_AMNT);
  des->cmd_id = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_CMD_ID);
  des->eng0_id = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_ENG0_ID);
  des->eng1_id = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_ENG1_ID);
  des->eng2_id = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_ENG2_ID);
  des->misc_enable = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MISC_EN);
  des->misc_select = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MISC_SELECT);
  des->misc_min_max =(int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MIN_MAX);
  des->misc_index_enable =(int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_INDEX_ENABLE);
  des->misc_auto_index = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_AUTO_INDEX);
  des->misc_input_addr_lo = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MISC_INPUT_L);
  des->misc_input_addr_hi = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MISC_INPUT_H);
  des->misc_output_addr_lo = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MISC_OUTPUT_L);
  des->misc_output_addr_hi = (int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MISC_OUTPUT_H);
  des->misc_cnt =(int)get_reg_id_val(cmd, (reg_id_t)CDMA_ID_MISC_CNT);
}

#endif
