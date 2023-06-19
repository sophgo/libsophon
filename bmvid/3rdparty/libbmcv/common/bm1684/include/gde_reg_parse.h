#ifndef __GDE_REG_PARSE_H__
#define __GDE_REG_PARSE_H__
#include "gde_reg_def.h"
#include "gen_cmd.h"

typedef struct {
  int scr;
  int icr;
  int busy;
  int done;
  int index_addr_l32;
  int index_addr_h32;
  int out_addr_l32;
  int out_addr_h32;
  int in_addr_l32;
  int in_addr_h32;
  int len;
  int type;
} gde_des_t;

static inline void parse_gde_des_struct(const P_COMMAND cmd, gde_des_t *des) {
  des->scr = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_SCR);
  des->icr = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_ICR);
  des->busy = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_BUSY);
  des->done = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_DONE);
  des->index_addr_l32 = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_INDEX_ADDR_L32);
  des->index_addr_h32 = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_INDEX_ADDR_H32);
  des->out_addr_l32 = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_OUT_ADDR_L32);
  des->out_addr_h32 = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_OUT_ADDR_H32);
  des->in_addr_l32 = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_IN_ADDR_L32);
  des->in_addr_h32 = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_IN_ADDR_H32);
  des->len = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_LEN);
  des->type = (int)get_reg_id_val(cmd, (reg_id_t)GDE_ID_TYPE);
#ifdef DEBUG_GDE
  printf("scr=0x%x\n", des->scr);
  printf("icr=0x%x\n", des->icr);
  printf("ssr=0x%x\n", des->ssr);
  printf("index_addr_l32=0x%x\n", des->index_addr_l32);
  printf("index_addr_h32=0x%x\n", des->index_addr_h32);
  printf("out_addr_l32=0x%x\n", des->out_addr_l32);
  printf("out_addr_h32=0x%x\n", des->out_addr_h32);
  printf("in_addr_l32=0x%x\n", des->in_addr_l32);
  printf("in_addr_h32=0x%x\n", des->in_addr_h32);
  printf("len=%d\n", des->len);
  printf("type=%d\n", des->type);
#endif
}

#endif  // __GDE_REG_PARSE_H__
