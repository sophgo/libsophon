#include "engine_internal.h"
#include "engine_io.h"
#include "debug.h"

#define BD_CMD_FIFO_LIMIT    1
#define GDMA_CMD_FIFO_LIMIT  1
#define CDMA_CMD_FIFO_LIMIT  1

#ifdef BOARD_QEMU
static inline void poll_bd_engine_fifo()
{
}

static inline void poll_gdma_engine_fifo()
{
}
#else
static inline void poll_bd_engine_fifo()
{
  fw_read32_wait_ge((BD_ENGINE_MAIN_CTRL), BD_CMD_FIFO_LIMIT, 4, 2);
}

static inline void poll_gdma_engine_fifo()
{
  fw_read32_wait_ge((GDMA_ENGINE_BASE_ADDR + 0x8), GDMA_CMD_FIFO_LIMIT, 8, 16);
}
#endif

P_TASK_DESCRIPTOR get_task_descriptor(ENGINE_ID eng_id)
{
  P_TASK_DESCRIPTOR p_desc = NULL;
  switch(eng_id) {
  case ENGINE_BD:
    p_desc = (P_TASK_DESCRIPTOR)BD_CMD_BASE_ADDR;
    poll_bd_engine_fifo();
    break;
  case ENGINE_GDMA:
    p_desc = (P_TASK_DESCRIPTOR)GDMA_CMD_BASE_ADDR;
    poll_gdma_engine_fifo();
    break;
  default:
    ASSERT(0);
    break;
  }
  return p_desc;
}

int get_task_descriptor_regnum(ENGINE_ID engine_id)
{
  switch(engine_id) {
  case ENGINE_BD:
    return BD_ENGINE_DESCRIPTOR_NUM;
  case ENGINE_GDMA:
    return GDMA_ENGINE_DESCRIPTOR_NUM;
  default:
    return 0;
  }
}

void resync_cmd_id(CMD_ID_NODE *p_cmd_id)
{
  p_cmd_id->bd_cmd_id = 0;
  p_cmd_id->gdma_cmd_id = 0;
  p_cmd_id->cdma_cmd_id = 0;

  //fushun TODO: register spec doesn't define cmd id clear bit now
  //fw_write32(BD_ENGINE_MAIN_CTRL, (1 << BD_CLEAR_CMD_ID_BIT));
  fw_write32(GDMA_ENGINE_MAIN_CTRL, (1 << GDMA_CLEAR_CMD_ID_BIT));
}

void resync_stress_cmd_id(CMD_ID_NODE *p_cmd_id, ENGINE_ID engine_id)
{
  switch(engine_id) {
  case ENGINE_BD:
    p_cmd_id->bd_cmd_id = 0;
    //fushun TODO
    //fw_write32(BD_ENGINE_MAIN_CTRL, (1 << BD_CLEAR_CMD_ID_BIT));
    break;
  case ENGINE_GDMA:
    p_cmd_id->gdma_cmd_id = 0;
    fw_write32(GDMA_ENGINE_MAIN_CTRL, (1 << GDMA_CLEAR_CMD_ID_BIT));
    break;
  default:
    FW_INFO("no engine\n");
    break;
  }
}

void set_task_descriptor(P_TASK_DESCRIPTOR pdesc, ENGINE_ID engine_id)
{
  ASSERT(NULL != pdesc);

  u32 *p_pdesc = (u32*)pdesc;
  FW_ADDR_T descriptor_regbase = (FW_ADDR_T)get_task_descriptor(engine_id);
  int descriptor_regnum = get_task_descriptor_regnum(engine_id);

  // CAUTION(wwcai): the order is important, because the descriptor
  //                 loading trigger bit is in the first reg
  for (int idx = descriptor_regnum - 1; idx >= 0; idx--) {
    WRITE_DESC_REG(descriptor_regbase, idx, p_pdesc[idx]);
  }
#if 0
  for (int idx = 0; idx <= descriptor_regnum - 1; idx++) {
    printf("desc %02d: 0x%08x\n",  idx, p_pdesc[idx]);
  }
#endif
}

static inline u32 get_gdma_ctrl_bit()
{
    u32 bit_98;
    if ( NPU_NUM == 32){
        bit_98 = 1<<8;
    }
    else {
        // TODO fushun: spec doesn't define other values now.
        ASSERT(0);
    }

    u32 bit_7;
    if (EU_NUM == 16){
        bit_7 = 0<<7;
    }
    else{
        // TODO
        ASSERT(0);
    }
    u32 bit_6;
    if (LOCAL_MEM_ADDRWIDTH == 16){
        bit_6 = 0<<6;
    }
    else{
        // TODO
        ASSERT(0);
    }
    u32 bit_9876 = bit_98|bit_7|bit_6;
    return bit_9876;
}

void emit_task_descriptor(P_TASK_DESCRIPTOR pdesc, ENGINE_ID eng_id)
{
  UNUSED(pdesc);
  switch(eng_id) {
  case ENGINE_BD:
    // TODO: need ASIC team confirm if it's necessary in 188x
    // fw_write32(BD_ENGINE_MAIN_CTRL, (1 << 1));
    break;
  case ENGINE_GDMA:
    fw_write32(GDMA_ENGINE_MAIN_CTRL, (1)|get_gdma_ctrl_bit());
    break;
  default:
    ASSERT(0);
    break;
  }
}

#ifdef BOARD_QEMU
void poll_all_engine_done(CMD_ID_NODE *id_node)
{
  printf("engine: error, no npu on qemu\n");
}
#else
void poll_all_engine_done(CMD_ID_NODE *id_node)
{
  FW_TRACE("poll_all_engine_done, id_node(0,2,3) = %d %d %d\n",
           id_node->bd_cmd_id, id_node->gdma_cmd_id, id_node->cdma_cmd_id);
  fw_read32_wait_ge((BD_CTRL_BASE_ADDR), id_node->bd_cmd_id, 16, 6);
  fw_read32_wait_ge((GDMA_ENGINE_BASE_ADDR + 0xC), id_node->gdma_cmd_id, 16, 16);
  FW_TRACE("poll_all_engine_done, done\n");
#ifdef ENABLE_TV_GEN
  extern void cmodel_tv_gen_sync();
  cmodel_tv_gen_sync();
#endif
}
#endif
