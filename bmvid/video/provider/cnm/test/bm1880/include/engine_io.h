#ifndef _ENGINE_IO_H_
#define _ENGINE_IO_H_

#include "common.h"

#define fw_read32(addr)          RAW_READ32((FW_ADDR_T)(addr))
#define fw_write32(addr, val)    RAW_WRITE32((FW_ADDR_T)(addr), (val))
static inline void fw_read32_wait_ge(FW_ADDR_T addr,
    u32 wait_data, u32 bits, u32 shift)
{
  u32 mask_val = (0xffffffff >> (32 - bits));
  for (;;) {
    u32 rd_data = fw_read32(addr);
    if (((rd_data >> shift) & mask_val) >= wait_data)
      break;
  }
}

static inline u32 current_id(FW_ADDR_T reg_addr, u32 bits, u32 shift)
{
  u32 mask_val = (0xffffffff >> (32 - bits));
  u32 rd_data = fw_read32(reg_addr);
  return ((rd_data >> shift) & mask_val);
}

void resync_cmd_id(CMD_ID_NODE * p_cmd_id);
void resync_stress_cmd_id(CMD_ID_NODE * p_cmd_id, ENGINE_ID engine_id);
void set_task_descriptor(P_TASK_DESCRIPTOR pdesc, ENGINE_ID engine_id);
void emit_task_descriptor(P_TASK_DESCRIPTOR pdesc, ENGINE_ID eng_id);
void poll_all_engine_done(CMD_ID_NODE * id_node);

static inline void call_single_cmd(P_TASK_DESCRIPTOR desc, ENGINE_ID eng_id)
{
  set_task_descriptor(desc, eng_id);
  emit_task_descriptor(desc, eng_id);
}

#endif /* _ENGINE_IO_H_ */
