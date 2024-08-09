#ifndef __GDMA_REG_PARSE_H__
#define __GDMA_REG_PARSE_H__
#include "gdma_reg_def.h"
#include "gen_cmd.h"

typedef struct {
  int pio_gdma_enable;
  int des_type;
  int chain_end;
  int intr_en;
  int barrier_enable;
  int stride_enable;
  int direction;
  int acc_write_enable;
  int common_mode;
  int prefetch_disable;
  int hold_des_value;
  int cmd_id;
  int special_func;
  int dst_data_format;
  int chw_copy;
  int sys_mem_type;
  int src_data_format;
  int lrn_shift_num;
  int lrn_shift_dir;
  int eng0_sync_id;
  int eng1_sync_id;
  int eng3_sync_id;
  int constant_value;
  int src_nstride;
  int src_cstride;
  int src_hstride;
  int src_wstride;
  int dst_nstride;
  int dst_cstride;
  int dst_hstride;
  int dst_wstride;
  int src_nsize;
  int src_csize;
  int src_hsize;
  int src_wsize;
  int dst_nsize;
  int dst_csize;
  int dst_hsize;
  int dst_wsize;
  int src_start_addr_l32;
  int dst_start_addr_l32;
  int src_start_addr_h8;
  int dst_start_addr_h8;
  int src_hshift;
  int src_wshift;
  int dst_hshift;
  int dst_wshift;
  int localmem_mask_l32;
  int localmem_mask_h32;
  int single_step;
  int debug_mode;
} gdma_des_t;

static inline void parse_gdma_des_struct(const P_COMMAND cmd, gdma_des_t *des) {
  des->pio_gdma_enable = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_PIO_GDMA_ENABLE);
  des->des_type = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DES_TYPE);
  des->chain_end = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_CHAIN_END);
  des->intr_en = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_INTR_EN);
  des->barrier_enable = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_BARRIER_ENABLE);
  des->stride_enable = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_STRIDE_ENABLE);
  des->direction = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DIRECTION);
  des->acc_write_enable = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_ACC_WRITE_ENABLE);
  des->common_mode = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_COMMON_MODE);
  des->prefetch_disable = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_PREFETCH_DISABLE);
  des->hold_des_value = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_HOLD_DES_VALUE);
  des->cmd_id = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_CMD_ID);
  des->special_func = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SPECIAL_FUNC);
  des->dst_data_format = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_DATA_FORMAT);
  des->chw_copy = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_CHW_COPY);
  des->sys_mem_type = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SYS_MEM_TYPE);
  des->src_data_format = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_DATA_FORMAT);
  des->lrn_shift_num = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_LRN_SHIFT_NUM);
  des->lrn_shift_dir = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_LRN_SHIFT_DIR);
  des->eng0_sync_id = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_ENG0_SYNC_ID);
  des->eng1_sync_id = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_ENG1_SYNC_ID);
  des->eng3_sync_id = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_ENG3_SYNC_ID);
  des->constant_value = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_CONSTANT_VALUE);
  des->src_nstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_NSTRIDE);
  des->src_cstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_CSTRIDE);
  des->src_hstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_HSTRIDE);
  des->src_wstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_WSTRIDE);
  des->dst_nstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_NSTRIDE);
  des->dst_cstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_CSTRIDE);
  des->dst_hstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_HSTRIDE);
  des->dst_wstride = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_WSTRIDE);
  des->src_nsize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_NSIZE);
  des->src_csize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_CSIZE);
  des->src_hsize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_HSIZE);
  des->src_wsize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_WSIZE);
  des->dst_nsize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_NSIZE);
  des->dst_csize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_CSIZE);
  des->dst_hsize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_HSIZE);
  des->dst_wsize = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_WSIZE);
  des->src_start_addr_l32 = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_START_ADDR_L32);
  des->dst_start_addr_l32 = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_START_ADDR_L32);
  des->src_start_addr_h8 = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_START_ADDR_H8);
  des->dst_start_addr_h8 = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_START_ADDR_H8);
  des->src_hshift = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_HSHIFT);
  des->src_wshift = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SRC_WSHIFT);
  des->dst_hshift = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_HSHIFT);
  des->dst_wshift = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DST_WSHIFT);
  des->localmem_mask_l32 = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_LOCALMEM_MASK_L32);
  des->localmem_mask_h32 = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_LOCALMEM_MASK_H32);
  des->single_step = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_SINGLE_STEP);
  des->debug_mode = (int)get_reg_id_val(cmd, (reg_id_t)GDMA_ID_DEBUG_MODE);
#ifdef DEBUG_GDMA
  printf("pio_gdma_enable=0x%x\n", des->pio_gdma_enable);
  printf("des_type=0x%x\n", des->des_type);
  printf("chain_end=0x%x\n", des->chain_end);
  printf("intr_en=0x%x\n", des->intr_en);
  printf("barrier_enable=0x%x\n", des->barrier_enable);
  printf("stride_enable=0x%x\n", des->stride_enable);
  printf("direction=0x%x\n", des->direction);
  printf("acc_write_enable=0x%x\n", des->acc_write_enable);
  printf("common_mode=0x%x\n", des->common_mode);
  printf("prefetch_disable=0x%x\n", des->prefetch_disable);
  printf("hold_des_value=0x%x\n", des->hold_des_value);
  printf("cmd_id=0x%x\n", des->cmd_id);
  printf("special_func=0x%x\n", des->special_func);
  printf("dst_data_format=0x%x\n", des->dst_data_format);
  printf("chw_copy=0x%x\n", des->chw_copy);
  printf("sys_mem_type=0x%x\n", des->sys_mem_type);
  printf("src_data_format=0x%x\n", des->src_data_format);
  printf("lrn_shift_num=0x%x\n", des->lrn_shift_num);
  printf("lrn_shift_dir=0x%x\n", des->lrn_shift_dir);
  printf("eng0_sync_id=0x%x\n", des->eng0_sync_id);
  printf("eng1_sync_id=0x%x\n", des->eng1_sync_id);
  printf("eng3_sync_id=0x%x\n", des->eng3_sync_id);
  printf("constant_value=0x%x\n", des->constant_value);
  printf("src_nstride=0x%x\n", des->src_nstride);
  printf("src_cstride=0x%x\n", des->src_cstride);
  printf("src_hstride=0x%x\n", des->src_hstride);
  printf("src_wstride=0x%x\n", des->src_wstride);
  printf("dst_nstride=0x%x\n", des->dst_nstride);
  printf("dst_cstride=0x%x\n", des->dst_cstride);
  printf("dst_hstride=0x%x\n", des->dst_hstride);
  printf("dst_wstride=0x%x\n", des->dst_wstride);
  printf("src_nsize=0x%x\n", des->src_nsize);
  printf("src_csize=0x%x\n", des->src_csize);
  printf("src_hsize=0x%x\n", des->src_hsize);
  printf("src_wsize=0x%x\n", des->src_wsize);
  printf("dst_nsize=0x%x\n", des->dst_nsize);
  printf("dst_csize=0x%x\n", des->dst_csize);
  printf("dst_hsize=0x%x\n", des->dst_hsize);
  printf("dst_wsize=0x%x\n", des->dst_wsize);
  printf("src_start_addr_l32=0x%x\n", des->src_start_addr_l32);
  printf("dst_start_addr_l32=0x%x\n", des->dst_start_addr_l32);
  printf("src_start_addr_h8=0x%x\n", des->src_start_addr_h8);
  printf("dst_start_addr_h8=0x%x\n", des->dst_start_addr_h8);
  printf("src_hshift=0x%x\n", des->src_hshift);
  printf("src_wshift=0x%x\n", des->src_wshift);
  printf("dst_hshift=0x%x\n", des->dst_hshift);
  printf("dst_wshift=0x%x\n", des->dst_wshift);
  printf("localmem_mask_l32=0x%x\n", des->localmem_mask_l32);
  printf("localmem_mask_h32=0x%x\n", des->localmem_mask_h32);
  printf("single_step=0x%x\n", des->single_step);
  printf("debug_mode=0x%x\n", des->debug_mode);
#endif
}

#endif  // __GDMA_REG_PARSE_H__
