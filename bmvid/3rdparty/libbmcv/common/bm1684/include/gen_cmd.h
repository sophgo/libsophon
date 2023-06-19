#ifndef GEN_CMD_H
#define GEN_CMD_H
#include "common.h"
#ifdef USING_CMODEL
#include <unistd.h>
#include "store_cmd.h"
#endif
#include "firmware_profile.h"
#ifdef __cplusplus
extern "C" {
#endif

#define __TRUE__     (1)
#define __FALSE__    (0)
#define BD_ID(id)    (id[0])
#define GDMA_ID(id)  (id[1])
#define WORD_SIZE    (32)
#define WORD_BITS    (5)
#define WORD_MASK    (0x1f)
#define LANE_SEC     ((NPU_NUM - 1) / WORD_SIZE + 1)
typedef struct REG_ID {
    unsigned short where;  /* low bit index. */
    unsigned short len;    /* bit length. */
} reg_id_t;
typedef struct REG_PACK {
    reg_id_t id;         /* register id. */
    unsigned int val;    /* value to be read or written. */
} reg_pack_t;
typedef struct CMD {
    P_COMMAND buf;
    unsigned int *cache;
    int len;
    void (*write)(P_COMMAND, unsigned int, unsigned int);
    unsigned int (*read)(P_COMMAND, unsigned int);
} cmd_t;
typedef struct GEN_CMD {
    cmd_t cmd;
    P_COMMAND (*buf)(void);
    void (*id_proc)(unsigned int *, CMD_ID_NODE *);
    void (*init_cache)(cmd_t *);
    void (*set_id)(cmd_t *, const unsigned int *);
    void (*common)(cmd_t *);
    void (*enable_cmd)(cmd_t *);
    void (*set_lane)(cmd_t *);
    void (*set_des_intr)(cmd_t *);
} gen_cmd_t;

gen_cmd_t *bd_gen_cmd();
gen_cmd_t *gdma_gen_cmd();
gen_cmd_t *gde_gen_cmd();
gen_cmd_t *cdma_gen_cmd();

#define BD_GEN_CMD     (bd_gen_cmd())
#define GDMA_GEN_CMD   (gdma_gen_cmd())
#define GDE_GEN_CMD   (gde_gen_cmd())
#define CDMA_GEN_CMD  (cdma_gen_cmd())

unsigned int get_bd_lane(int sec_idx);
void set_bd_lane(unsigned int lane, int sec_idx);
unsigned int get_gdma_lane(int sec_idx);
void set_gdma_lane(unsigned int lane, int sec_idx);
unsigned char get_bd_des_intr_enable();
void set_bd_des_intr_enable(unsigned char flag);
unsigned char get_gdma_des_intr_enable();
void set_gdma_des_intr_enable(unsigned char flag);
P_COMMAND gen(gen_cmd_t *g, const reg_pack_t *reg, int reg_len, CMD_ID_NODE *pid_node);
#define BEGIN_GEN_CMD(engine) \
  gen_cmd_t *pack_cmd_for_gen_cmd = engine##_GEN_CMD;\
  const reg_pack_t pack_configs_for_gen_cmd[] = {
#define BEGIN_GEN_CMD_BD   BEGIN_GEN_CMD(BD)
#define BEGIN_GEN_CMD_GDMA BEGIN_GEN_CMD(GDMA)
#define BEGIN_GEN_CMD_GDE  BEGIN_GEN_CMD(GDE)
#ifdef USING_CMODEL
#define CMD_GEN_INST(cmd, __id_node) \
  if (get_store_cmd_enable() || get_atomic_cmodel_enable()) { \
      cmd = gen(pack_cmd_for_gen_cmd, \
                pack_configs_for_gen_cmd, \
                sizeof(pack_configs_for_gen_cmd) / sizeof(reg_pack_t), \
                __id_node); \
  } else { \
      unsigned int id[2]; \
      pack_cmd_for_gen_cmd->id_proc(id, __id_node); \
  }
#else
#define CMD_GEN_INST(cmd, __id_node) \
  cmd = gen(pack_cmd_for_gen_cmd, \
            pack_configs_for_gen_cmd, \
            sizeof(pack_configs_for_gen_cmd) / sizeof(reg_pack_t), \
            __id_node);
#endif
#define END_GEN_CMD(__engine, __id_node) \
  }; \
  do { \
      P_COMMAND cmd = NULL; \
      CMD_GEN_INST(cmd, __id_node); \
      call_atomic(cmd, ENGINE_##__engine); \
  } while (0);\
  profile_time_begin_set_node(__id_node, ENGINE_##__engine);

#ifdef USING_CMODEL
#define WRITE_CMD_BD(offset, val) \
  fast_cmd_for_gen_cmd->cmd.write(fast_cmd_for_gen_cmd->cmd.buf, offset, val)
#define WRITE_CMD_GDMA(offset, val) \
  fast_cmd_for_gen_cmd->cmd.write(fast_cmd_for_gen_cmd->cmd.buf, offset, val)
#else
#define WRITE_CMD_BD(offset, val) \
    WRITE_BD_COMMAND_REG(BDC_CMD_BASE_ADDR, offset, val)
#define WRITE_CMD_GDMA(offset, val) \
    WRITE_GDMA_COMMAND_REG(GDMA_CMD_BASE_ADDR, offset, val)
#endif

#define BEGIN_FAST_GEN_CMD(engine) \
  u32 id[2]; \
  gen_cmd_t *fast_cmd_for_gen_cmd = engine##_GEN_CMD; \
  fast_cmd_for_gen_cmd->cmd.buf = fast_cmd_for_gen_cmd->buf(); \
  fast_cmd_for_gen_cmd->id_proc(id, pid_node);

#define END_FAST_GEN_CMD(__engine, __id_node) \
  do { \
      call_atomic(fast_cmd_for_gen_cmd->cmd.buf, ENGINE_##__engine); \
      fast_cmd_for_gen_cmd->cmd.buf = NULL; \
  } while (0);\
  profile_time_begin_set_node(__id_node, ENGINE_##__engine);

#define WRITE_BD_CMD0() \
  { \
    unsigned int val = (BD_ID(id) << 3) + (GDMA_ID(id) << 19) + 0x06; \
    WRITE_CMD_BD(0, val); \
    val += 0x01; \
    WRITE_CMD_BD(0, val); \
  }

#define WRITE_BD_CMD1(keep, type, op, op_num, shift_type, res_add) \
  { \
    unsigned int val = ((GDMA_ID(id) >> 13) & 0x07) + ((keep) << 3) + ((type) << 5); \
    val += ((op) << 9) + ((op_num) << 14) + ((shift_type) << 26); \
    val += ((res_add) << 27); \
    WRITE_CMD_BD(1, val); \
  }

#define WRITE_BD_CMD1_DES_OPT_RELU(keep, type, op, op_num, shift_type, res_add) \
  { \
    unsigned int val = ((GDMA_ID(id) >> 13) & 0x07) + ((keep) << 3) + ((type) << 5); \
    val += ((op) << 9) + ((op_num) << 14) + ((shift_type) << 26); \
    val += ((res_add) << 27); \
    val += (1 << 28); \
    WRITE_CMD_BD(1, val); \
  }

#define WRITE_BD_CMD2_ALL(opd0_sign, opd1_sign, opd2_sign, \
  res0_prec, opd0_prec, opd1_prec, opd2_prec, \
  opd0_const, opd1_const, opd2_const, \
  res0_align, opd0_align, opd1_align, opd2_align) \
  { \
    unsigned int val = (opd0_sign) + ((opd1_sign) << 1) + ((opd2_sign) << 2); \
    val += ((res0_prec) << 3) + ((opd0_prec) << 6) + ((opd1_prec) << 9) + ((opd2_prec) << 12); \
    val += ((opd0_const) << 15) + ((opd1_const) << 16) + ((opd2_const) << 17); \
    val += ((res0_align) << 18) + ((opd0_align) << 21) + ((opd1_align) << 24); \
    val += ((opd2_align) << 27); \
    WRITE_CMD_BD(2, val); \
  }

#define WRITE_BD_CMD2_TENSOR_FP32(opd0_const, opd1_const, opd2_const) \
  { \
    unsigned int val = 0x1B6C2490 + ((opd0_const) << 15) + ((opd1_const) << 16) + ((opd2_const) << 17); \
    WRITE_CMD_BD(2, val); \
  }

#define WRITE_BD_CMD2_TENSOR(res0_prec, opd0_prec, opd1_prec, opd2_prec, \
  opd0_const, opd1_const, opd2_const) \
  { \
    unsigned int val = ((res0_prec) << 3) + ((opd0_prec) << 6) + ((opd1_prec) << 9) + ((opd2_prec) << 12); \
    val += 0x1B6C0000 + ((opd0_const) << 15) + ((opd1_const) << 16) + ((opd2_const) << 17); \
    WRITE_CMD_BD(2, val); \
  }

#define WRITE_BD_CMD2_TENSOR_FP32_MD(opd0_const, opd1_const, opd2_const) \
  { \
    unsigned int val = 0x2490 + ((opd0_const) << 15) + ((opd1_const) << 16) + ((opd2_const) << 17); \
    WRITE_CMD_BD(2, val); \
  }

#define WRITE_BD_CMD4(res0_x_stride, res0_y_stride, lane_mask0) \
  { \
    unsigned int val = (((res0_x_stride) >> 2) & 0x03) + ((res0_y_stride) << 2) + ((lane_mask0) << 6); \
    WRITE_CMD_BD(4, val); \
  }

#define WRITE_BD_CMD5(lane_mask0, lane_mask1) \
  { \
    unsigned int val = (((lane_mask0) >> 26) & 0x3f) + ((lane_mask1) << 6); \
    WRITE_CMD_BD(5, val); \
  }

#define WRITE_BD_CMD6(lane_mask1, res0_n, res0_c) \
  { \
    unsigned int val = (((lane_mask1) >> 26) & 0x3f) + ((res0_n) << 6) + ((res0_c) << 22); \
    WRITE_CMD_BD(6, val); \
  }

#define WRITE_BD_CMD7(res0_c, res0_h, res0_w) \
  { \
    unsigned int val = (((res0_c) >> 10) & 0x03) + ((res0_h) << 2) + ((res0_w) << 18); \
    WRITE_CMD_BD(7, val); \
  }

#define WRITE_BD_CMD8(res0_w, opd0_n, opd0_c, opd0_h) \
  { \
    unsigned int val = (((res0_w) >> 14) & 0x03) + ((opd0_n) << 2) + ((opd0_c) << 18) + ((opd0_h) << 30); \
    WRITE_CMD_BD(8, val); \
  }

#define WRITE_BD_CMD9(opd0_h, opd0_w, opd1_n) \
  { \
    unsigned int val = (((opd0_h) >> 2) & 0x3fff) + ((opd0_w) << 14) + ((opd1_n) << 30); \
    WRITE_CMD_BD(9, val); \
  }

#define WRITE_BD_CMD10(opd1_n, opd1_c, opd1_h) \
  { \
    unsigned int val = (((opd1_n) >> 2) & 0x3ff) + ((opd1_c) << 10) + ((opd1_h) << 22); \
    WRITE_CMD_BD(10, val); \
  }

#define WRITE_BD_CMD11(opd1_h, opd1_w, res0_h_shift, res0_w_shift, opd0_h_shift) \
  { \
    unsigned int val = (((opd1_h) >> 10) & 0x3f) + ((opd1_w) << 6) + ((res0_h_shift) << 22); \
    val += ((res0_w_shift) << 26) + ((res0_h_shift) << 30); \
    WRITE_CMD_BD(11, val); \
  }

#define WRITE_BD_CMD12(opd0_h_shift, opd0_w_shift, opd1_h_shift, opd1_w_shift, res0_n_stride) \
  { \
    unsigned int val = (((opd0_h_shift) >> 2) & 0x03) + ((opd0_w_shift) << 2); \
    val += ((opd1_h_shift) << 6) + ((opd1_w_shift) << 10); \
    val += ((res0_n_stride) << 14); \
    WRITE_CMD_BD(12, val); \
  }

#define WRITE_BD_CMD13(res0_n_stride, res0_c_stride, opd0_n_stride) \
  { \
    unsigned int val = (((res0_n_stride) >> 18) & 0x01) + ((res0_c_stride) << 1) + ((opd0_n_stride) << 20); \
    WRITE_CMD_BD(13, val); \
  }

#define WRITE_BD_CMD14(opd0_n_stride, opd0_c_stride, opd1_n_stride) \
  { \
    unsigned int val = (((opd0_n_stride) >> 12) & 0x3f) + ((opd0_c_stride) << 7) + ((opd1_n_stride) << 26); \
    WRITE_CMD_BD(14, val); \
  }

#define WRITE_BD_CMD15(opd1_n_stride, opd1_c_stride) \
  { \
    unsigned int val = (((opd1_n_stride) >> 6) & 0x1fff) + ((opd1_c_stride) << 13); \
    WRITE_CMD_BD(15, val); \
  }

#define WRITE_BD_CMD16(opd2_n_stride, opd2_c_stride) \
  { \
    unsigned int val = (opd2_n_stride) + ((opd2_c_stride) << 19); \
    WRITE_CMD_BD(16, val); \
  }

#define WRITE_BD_CMD17(opd2_c_stride, res0_add_sign, opd0_neq1, opd1_neq1) \
  { \
    unsigned int val = (((opd2_c_stride) >> 13) & 0x3f) + ((res0_add_sign) << 6); \
    val += ((opd0_neq1) << 7) + ((opd1_neq1) << 8); \
    WRITE_CMD_BD(17, val); \
  }

#define WRITE_BD_CMD17_MD_CMP(opd2_c_stride, opd3_const) \
  { \
    unsigned int val = (((opd2_c_stride) >> 13) & 0x3f) + ((opd3_const) << 9); \
    WRITE_CMD_BD(17, val); \
  }

#define WRITE_BD_CMD_RES0_ADDR(res0_addr) \
  WRITE_CMD_BD(18, res0_addr);
#define WRITE_BD_CMD_OPD0_ADDR(opd0_addr) \
  WRITE_CMD_BD(19, opd0_addr);
#define WRITE_BD_CMD_OPD1_ADDR(opd1_addr) \
  WRITE_CMD_BD(20, opd1_addr);
#define WRITE_BD_CMD_OPD2_ADDR(opd2_addr) \
  WRITE_CMD_BD(21, opd2_addr);
#define WRITE_BD_CMD_RES0_H_STR(res0_h_stride) \
  WRITE_CMD_BD(22, res0_h_stride);
#define WRITE_BD_CMD_RES0_W_STR(res0_w_stride) \
  WRITE_CMD_BD(23, res0_w_stride);
#define WRITE_BD_CMD_OPD0_H_STR(opd0_h_stride) \
  WRITE_CMD_BD(24, opd0_h_stride);
#define WRITE_BD_CMD_OPD0_W_STR(opd0_w_stride) \
  WRITE_CMD_BD(25, opd0_w_stride);
#define WRITE_BD_CMD_OPD1_H_STR(opd1_h_stride) \
  WRITE_CMD_BD(26, opd1_h_stride);
#define WRITE_BD_CMD_OPD1_W_STR(opd1_w_stride) \
  WRITE_CMD_BD(27, opd1_w_stride);
#define WRITE_BD_CMD_RES1_ADDR(res1_addr) \
  WRITE_CMD_BD(30, res1_addr);
#define WRITE_BD_CMD_OPD3_ADDR(opd3_addr) \
  WRITE_CMD_BD(31, opd3_addr);
// ------------GDMA----------------
#define WRITE_GDMA_CMD0_COMMON(val) \
  { \
    val = (val) | (GDMA_ID(id) << 16) | 20 | ((get_gdma_des_intr_enable()) << 3); \
    WRITE_CMD_GDMA(0, val); \
    val += 0x01; \
    WRITE_CMD_GDMA(0, val); \
  }
#define WRITE_GDMA_CMD1_COMMON(val) \
  { \
    val = (val) | (BD_ID(id) << 16); \
    WRITE_CMD_GDMA(1, val); \
  }
#define WRITE_GDMA_CMD2_COMMON(val) \
  { \
    WRITE_CMD_GDMA(2, val); \
  }
#define WRITE_GDMA_CMD3_COMMON(val) \
  { \
    WRITE_CMD_GDMA(3, val); \
  }
#define WRITE_GDMA_CMD4_COMMON(val) \
  { \
    WRITE_CMD_GDMA(4, val); \
  }
#define WRITE_GDMA_CMD5_COMMON(val) \
  { \
    WRITE_CMD_GDMA(5, val); \
  }
#define WRITE_GDMA_CMD6_COMMON(val) \
  { \
    WRITE_CMD_GDMA(6, val); \
  }
#define WRITE_GDMA_CMD7_COMMON(val) \
  { \
    WRITE_CMD_GDMA(7, val); \
  }
#define WRITE_GDMA_CMD8_COMMON(val) \
  { \
    WRITE_CMD_GDMA(8, val); \
  }
#define WRITE_GDMA_CMD9_COMMON(val) \
  { \
    WRITE_CMD_GDMA(9, val); \
  }
#define WRITE_GDMA_CMD10_COMMON(val) \
  { \
    WRITE_CMD_GDMA(10, val); \
  }
#define WRITE_GDMA_CMD11_COMMON(val) \
  { \
    WRITE_CMD_GDMA(11, val); \
  }
#define WRITE_GDMA_CMD12_COMMON(val) \
  { \
    WRITE_CMD_GDMA(12, val); \
  }
#define WRITE_GDMA_CMD13_COMMON(val) \
  { \
    WRITE_CMD_GDMA(13, val); \
  }
#define WRITE_GDMA_CMD14_COMMON(val) \
  { \
    WRITE_CMD_GDMA(14, val); \
  }
#define WRITE_GDMA_CMD15_COMMON(val) \
  { \
    WRITE_CMD_GDMA(15, val); \
  }
#define WRITE_GDMA_CMD16_COMMON(val) \
  { \
    WRITE_CMD_GDMA(16, val); \
  }
#define WRITE_GDMA_CMD17_COMMON(val) \
  { \
    WRITE_CMD_GDMA(17, val); \
  }
#define WRITE_GDMA_CMD18_COMMON(val) \
  { \
    WRITE_CMD_GDMA(18, val); \
  }
#define WRITE_GDMA_CMD19_COMMON(val) \
  { \
    WRITE_CMD_GDMA(19, val); \
  }
#define WRITE_GDMA_CMD20_COMMON(val) \
  { \
    val = get_gdma_lane(0); \
    WRITE_CMD_GDMA(20, val); \
  }

#define WRITE_GDMA_CMD21_COMMON(val) \
  { \
    val = get_gdma_lane(1); \
    WRITE_CMD_GDMA(21, val); \
  }

#define WRITE_GDMA_CMD0_DEFAULT(val) \
  { \
    val = (val) | 1024; \
  }

#define WRITE_GDMA_CMD1_DEFAULT(val) \
  { \
    val = (val) | 2112; \
  }

#define WRITE_GDMA_CMD2_DEFAULT(val)
#define WRITE_GDMA_CMD3_DEFAULT(val)
#define WRITE_GDMA_CMD4_DEFAULT(val)
#define WRITE_GDMA_CMD5_DEFAULT(val)
#define WRITE_GDMA_CMD6_DEFAULT(val)
#define WRITE_GDMA_CMD7_DEFAULT(val) \
  { \
    val = (val) | 1; \
  }

#define WRITE_GDMA_CMD8_DEFAULT(val)
#define WRITE_GDMA_CMD9_DEFAULT(val)
#define WRITE_GDMA_CMD10_DEFAULT(val)
#define WRITE_GDMA_CMD11_DEFAULT(val)
#define WRITE_GDMA_CMD12_DEFAULT(val)
#define WRITE_GDMA_CMD13_DEFAULT(val)
#define WRITE_GDMA_CMD14_DEFAULT(val)
#define WRITE_GDMA_CMD15_DEFAULT(val)
#define WRITE_GDMA_CMD16_DEFAULT(val)
#define WRITE_GDMA_CMD17_DEFAULT(val)
#define WRITE_GDMA_CMD18_DEFAULT(val)
#define WRITE_GDMA_CMD19_DEFAULT(val)

#define WRITE_GDMA_CMD20_DEFAULT(val) \
  { \
    val = (val) | 0xffffffff; \
  }

#define WRITE_GDMA_CMD21_DEFAULT(val) \
  { \
    val = (val) | 0xffffffff; \
  }

#define WRITE_GDMA_CMD0(direction) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD0_DEFAULT(val); \
    val += (32) | \
           ((direction) << 6); \
    WRITE_GDMA_CMD0_COMMON(val); \
  }

#define WRITE_GDMA_CMD0_SET_ACC(direction, acc) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD0_DEFAULT(val); \
    val += (32) | \
           ((acc) << 8) | \
           ((direction) << 6); \
    WRITE_GDMA_CMD0_COMMON(val); \
  }

#define WRITE_GDMA_CMD0_DIS_STRIDE(direction) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD0_DEFAULT(val); \
    val += (0) | \
           ((direction) << 6); \
    WRITE_GDMA_CMD0_COMMON(val); \
  }

#define WRITE_GDMA_CMD0_SET_ACC_DIS_STRIDE(direction, acc) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD0_DEFAULT(val); \
    val += ((acc) << 8) | \
           ((direction) << 6); \
    WRITE_GDMA_CMD0_COMMON(val); \
  }

#define WRITE_GDMA_CMD0_COMMON_MODE_DIS_STRIDE(direction) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD0_DEFAULT(val); \
    val += (1 << 9) | \
           ((direction) << 6); \
    WRITE_GDMA_CMD0_COMMON(val); \
  }

#define WRITE_GDMA_CMD0_HOLD_DES_VALUE(direction, copy_to_next_instruction) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD0_DEFAULT(val); \
    val += (32) | \
           ((direction) << 6) | \
           ((copy_to_next_instruction) << 11); \
    WRITE_GDMA_CMD0_COMMON(val); \
  }

#define WRITE_GDMA_CMD0_HOLD_DES_VALUE_DIS_STRIDE(direction, copy_to_next_instruction) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD0_DEFAULT(val); \
    val += ((direction) << 6) | \
           ((copy_to_next_instruction) << 11); \
    WRITE_GDMA_CMD0_COMMON(val); \
  }

#define WRITE_GDMA_CMD1(special_func, src_format, dst_format, chw_copy) \
  { \
    unsigned int val = (special_func) | \
                        ((dst_format) << 3) | \
                        ((src_format) << 8); \
    WRITE_GDMA_CMD1_DEFAULT(val); \
    val = (val & 0xffffffbf) | ((chw_copy) << 6);  \
    WRITE_GDMA_CMD1_COMMON(val); \
  }

#define WRITE_GDMA_CMD1_MATRIX(special_func, src_format, dst_format, chw_copy) \
  { \
    unsigned int val = (special_func) | \
                        ((dst_format) << 3) | \
                        ((src_format) << 8); \
    WRITE_GDMA_CMD1_DEFAULT(val); \
    val = (val & 0xffffffbf) | ((chw_copy) << 6) | (1 << 7);  \
    WRITE_GDMA_CMD1_COMMON(val); \
  }

#define WRITE_GDMA_CMD1_LRN_SHIFT(special_func, src_format, dst_format, chw_copy, shift_num, shift_dir) \
  { \
    unsigned int val = (special_func) | \
                        ((dst_format) << 3) | \
                        ((src_format) << 8); \
    WRITE_GDMA_CMD1_DEFAULT(val); \
    val = (val & 0xfffff7bf) | ((chw_copy) << 6) | (shift_num << 11) | (shift_dir << 15);  \
    WRITE_GDMA_CMD1_COMMON(val); \
  }

#define WRITE_GDMA_CMD2() \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD2_DEFAULT(val); \
    WRITE_GDMA_CMD2_COMMON(val); \
  }

#define WRITE_GDMA_CMD3(constant_value) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD3_DEFAULT(val); \
    val = (val) | (constant_value); \
    WRITE_GDMA_CMD3_COMMON(val); \
  }

#define WRITE_GDMA_CMD4_SRC_N_STRIDE(src_n_stride) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD4_DEFAULT(val); \
    val = (val) | (src_n_stride); \
    WRITE_GDMA_CMD4_COMMON(val); \
  }

#define WRITE_GDMA_CMD5_SRC_C_STRIDE(src_c_stride) \
  { \
    unsigned int val = 0;  \
    WRITE_GDMA_CMD5_DEFAULT(val); \
    val = (src_c_stride); \
    WRITE_GDMA_CMD5_COMMON(val); \
  }

#define WRITE_GDMA_CMD6_SRC_H_STRIDE(src_h_stride) \
  { \
    unsigned int val = 0;  \
    WRITE_GDMA_CMD6_DEFAULT(val); \
    val = (src_h_stride); \
    WRITE_GDMA_CMD6_COMMON(val); \
  }

#define WRITE_GDMA_CMD7_SRC_W_STRIDE(src_w_stride) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD7_DEFAULT(val); \
    val = (val & 0xfffffffe) | (src_w_stride); \
    WRITE_GDMA_CMD7_COMMON(val); \
  }

#define WRITE_GDMA_CMD8_DST_N_STRIDE(dst_n_stride) \
  { \
    unsigned int val = (dst_n_stride); \
    WRITE_GDMA_CMD8_DEFAULT(val); \
    WRITE_GDMA_CMD8_COMMON(val); \
  }

#define WRITE_GDMA_CMD9_DST_C_STRIDE(dst_c_stride) \
  { \
    unsigned int val = (dst_c_stride); \
    WRITE_GDMA_CMD9_DEFAULT(val); \
    WRITE_GDMA_CMD9_COMMON(val); \
  }

#define WRITE_GDMA_CMD10_DST_H_STRIDE(dst_h_stride) \
  { \
    unsigned int val = (dst_h_stride); \
    WRITE_GDMA_CMD10_DEFAULT(val); \
    WRITE_GDMA_CMD10_COMMON(val); \
  }

#define WRITE_GDMA_CMD11_DST_W_STRIDE(dst_w_stride) \
  { \
    unsigned int val = (dst_w_stride); \
    WRITE_GDMA_CMD11_DEFAULT(val); \
    WRITE_GDMA_CMD11_COMMON(val); \
  }

#define WRITE_GDMA_CMD12_SRC_NC_SIZE(src_n_size, src_c_size) \
  { \
    unsigned int val = 0;  \
    WRITE_GDMA_CMD12_DEFAULT(val); \
    val += (src_n_size) | \
           ((src_c_size) << 16); \
    WRITE_GDMA_CMD12_COMMON(val); \
  }

#define WRITE_GDMA_CMD13_SRC_WH_SIZE(src_h_size, src_w_size) \
  { \
    unsigned int val = 0;  \
    WRITE_GDMA_CMD13_DEFAULT(val); \
    val += (src_h_size) | \
           ((src_w_size) << 16); \
    WRITE_GDMA_CMD13_COMMON(val); \
  }

#define WRITE_GDMA_CMD14_DST_NC_SIZE(dst_n_size, dst_c_size) \
  { \
    unsigned int val = 0;  \
    WRITE_GDMA_CMD14_DEFAULT(val); \
    val += (dst_n_size) | \
           ((dst_c_size) << 16); \
    WRITE_GDMA_CMD14_COMMON(val); \
  }

#define WRITE_GDMA_CMD15_DST_WH_SIZE(dst_h_size, dst_w_size) \
  { \
    unsigned int val = 0;  \
    WRITE_GDMA_CMD15_DEFAULT(val); \
    val += (dst_h_size) | \
                        ((dst_w_size) << 16); \
    WRITE_GDMA_CMD15_COMMON(val); \
  }
#define WRITE_GDMA_CMD16_SRC_ADDR_L32(src_addr) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD16_DEFAULT(val); \
    val = val + ((src_addr) & 0xFFFFFFFF); \
    WRITE_GDMA_CMD16_COMMON(val); \
  }

#define WRITE_GDMA_CMD17_DST_ADDR_L32(dst_addr) \
  { \
    unsigned int val = 0;  \
    WRITE_GDMA_CMD17_DEFAULT(val); \
    val = val + ((dst_addr) & 0xFFFFFFFF); \
    WRITE_GDMA_CMD17_COMMON(val); \
  }

#define WRITE_GDMA_CMD18_ADDR_H8(src_addr, dst_addr) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD18_DEFAULT(val); \
    val = val + (((src_addr)>>32) | (((dst_addr)>>32) << 8)); \
    WRITE_GDMA_CMD18_COMMON(val); \
  }

#define WRITE_GDMA_CMD19_SHIFT(src_h_shift, src_w_shift, dst_h_shift, dst_w_shift) \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD19_DEFAULT(val); \
    val += (src_h_shift) | \
           ((src_w_shift) <<8) | \
           ((dst_h_shift) <<16) | \
           ((dst_w_shift) <<24); \
    WRITE_GDMA_CMD19_COMMON(val); \
  }

#define WRITE_GDMA_CMD20_LOCAL_L32() \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD20_DEFAULT(val); \
    WRITE_GDMA_CMD20_COMMON(val); \
  }

#define WRITE_GDMA_CMD21_LOCAL_H32() \
  { \
    unsigned int val = 0; \
    WRITE_GDMA_CMD21_DEFAULT(val); \
    WRITE_GDMA_CMD21_COMMON(val); \
  }

#ifdef USING_CMODEL
#define READ_GDMA_FILTER_RES_NUM(val, pid_node)  \
do { \
    gen_cmd_t *fast_cmd_for_gen_cmd = GDMA_GEN_CMD; \
    fast_cmd_for_gen_cmd->cmd.buf = fast_cmd_for_gen_cmd->buf(); \
    int *res_buf = (int *)malloc(sizeof(int));\
    int *func_flag = (int *)fast_cmd_for_gen_cmd->cmd.buf + 20;\
    *func_flag = 0xffff;\
    *res_buf = 0xffff; \
    *(u32 *)fast_cmd_for_gen_cmd->cmd.buf = 0x1; \
    *(int **)((int *)fast_cmd_for_gen_cmd->cmd.buf + 21) = res_buf; \
    int gdma_id = ((pid_node->gdma_cmd_id) << 16); \
    int bd_id = ((pid_node->bd_cmd_id) << 16); \
    *((u32 *)fast_cmd_for_gen_cmd->cmd.buf + 1) += bd_id; \
    *(u32 *)fast_cmd_for_gen_cmd->cmd.buf += gdma_id; \
    call_atomic(fast_cmd_for_gen_cmd->cmd.buf, ENGINE_GDMA); \
    while (0xffff == *res_buf) {sleep(1);} \
    val = *res_buf;\
    free(res_buf);\
    fast_cmd_for_gen_cmd->cmd.buf = NULL; \
}while(0)
#else
#define READ_GDMA_FILTER_RES_NUM(val, pid_node) \
do { \
    val = READ_REG(GDMA_ENGINE_BASE_ADDR_PIO + ((832/32) * 4)); \
    val = val >> 2; \
} while (0)
#endif

#ifdef USING_CMODEL
unsigned int get_reg_id_val(P_COMMAND cmd, const reg_id_t id);
#define GET_REG_ID_VAL(cmd, id) get_reg_id_val(cmd, (reg_id_t)id)
void set_reg_id_val(P_COMMAND cmd, const reg_id_t id, unsigned int val);
#define SET_REG_ID_VAL(cmd, id, val) set_reg_id_val(cmd, (reg_id_t)id, val)
void lane_decomp(unsigned char array[NPU_NUM], const unsigned int lane[LANE_SEC]);
#endif /* USING_CMODEL */





#ifdef __cplusplus
}
#endif
#endif  // GEN_CMD_H
