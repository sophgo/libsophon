#include <stdlib.h>
#include "bm_kernel.h"
#include "system_common.h"
#include "engine_io.h"
#include "common.h"
#include "debug.h"
#include "timer.h"

//#define DEBUG_BM_FW_CMD

static CMD_ID_NODE cmd_id_node;

static unsigned int get_value(u32 data, int start, int width)
{
  return (data >> start) & ((1 << width) - 1);
}

static void fw_sync()
{
  //sync all operations
  poll_all_engine_done(&cmd_id_node);

  //reset all engines
  resync_cmd_id(&cmd_id_node);
}

void handle_cmd_buf(void *cmd_buf, u32 len)
{
  u32 *cmd = (u32*)cmd_buf;
  int i = 0;
  cmd_hdr_t *hdr;

  while(i < len / 4) {
    hdr = (cmd_hdr_t *)(&cmd[i]);
    ASSERT(hdr->magic == 0xa5);
    i += sizeof(cmd_hdr_t) / 4;
    int cmd_len = hdr->len / 4;
    switch (hdr->engine_id) {
    case ENGINE_BD:
#ifdef DEBUG_BM_FW_CMD
      printf("CMD BD, len %d, node %d\n", cmd_len, hdr->node_id);
#endif
      //dump_task_descriptor("bd", ENGINE_BD, (P_TASK_DESCRIPTOR *)&cmd[i]);
      cmd_id_node.bd_cmd_id = get_value(((u32 *)(&cmd[i]))[0], 16, 16);
      call_single_cmd((P_TASK_DESCRIPTOR *)&cmd[i], ENGINE_BD); //CAUTION(wwcai): hdr->node_id is always 0 in firmware bk
      break;
    case ENGINE_GDMA:
#ifdef DEBUG_BM_FW_CMD
      printf("CMD GDMA, len %d, node %d\n", cmd_len, hdr->node_id);
#endif
      //dump_task_descriptor("gdma", ENGINE_GDMA, (P_TASK_DESCRIPTOR *)&cmd[i]);
      cmd_id_node.gdma_cmd_id = get_value(((u32 *)(&cmd[i]))[0], 16, 16);
      call_single_cmd((P_TASK_DESCRIPTOR *)&cmd[i], ENGINE_GDMA);
      break;
    case ENGINE_ARM:
      break;
    /*
     * workaround for bmkernel workaround for mdsfu.
    default:
      ASSERT(0);
    */
    }
    i += cmd_len;
  }
}

static int wait_engine(void *addr,
    u32 wait_data, u32 bits, u32 shift, u64 timeout)
{
  u32 mask_val = (0xffffffff >> (32 - bits));
  timer_meter_start();
  for (;;) {
    u32 rd_data = readl(addr);
    if (((rd_data >> shift) & mask_val) >= wait_data) {
      uartlog("engine free, time-consuming:%lx ticks\n", timer_meter_get_tick());
      return 0;
    }
    u64 ticks = timer_meter_get_tick();
    if (ticks > timeout) {
      uartlog("poll engine timeout, time-step:%lx ticks\n", ticks);
      return ENGINE_END;
    }
  }
}

static int poll_stress_engine_done(CMD_ID_NODE *id_node, ENGINE_ID engine_id)
{
  int ret = ENGINE_END;
  switch(engine_id) {
  case ENGINE_BD:
    uartlog("[BD]\t");
    ret = wait_engine((void *)(BD_ENGINE_BASE_ADDR + 0x8), id_node->bd_cmd_id, 16, 16, 180);
    break;
  case ENGINE_GDMA:
    uartlog("[GDMA]\t");
    ret = wait_engine((void *)(GDMA_ENGINE_BASE_ADDR + 0xC), id_node->gdma_cmd_id, 16, 16, 4000);
    break;
  default:
    ASSERT(0);
  }

  return (ret != ENGINE_END ? engine_id : ENGINE_END);
}

void sync_engines_status(u8 *free_engines)
{
  u8 engines[2] = {ENGINE_BD, ENGINE_GDMA};

  for (int i = 0; i < 2; i++) {
    int ret = poll_stress_engine_done(&cmd_id_node, engines[i]);
    if (ENGINE_END != ret)
      resync_stress_cmd_id(&cmd_id_node, engines[i]);
    free_engines[i] = ret;
  }
}

static void* fw_alloc_cmd_buf(void *user_data, u32 size)
{
  void* cmd_buf = malloc(size);
#ifdef DEBUG_BM_FW_CMD
  printf("test alloc cmd_buf, len %d bytes, addr = %p\n", size, cmd_buf);
#endif
  return cmd_buf;
}

static void fw_emit_cmd_buf(void *user_data, void *cmd_buf, u32 len)
{
#ifdef DEBUG_BM_FW_CMD
  u32 *buf = (u32*)cmd_buf;
  printf("test emit cmd_buf, len %d bytes\n", len);
  for (int i = 0; i < len / sizeof(u32); i++) {
    printf("BUF %02d: 0x%08x\n", i, buf[i]);
  }
#endif

  handle_cmd_buf(cmd_buf, len);

  /* free implicitly */
#ifdef DEBUG_BM_FW_CMD
  printf("free cmd_buf, addr = %p\n", cmd_buf);
#endif
  free(cmd_buf);
}

static void fw_free_cmd_buf(void *user_data, void *cmd_buf)
{
  /* free without emit */
  fw_emit_cmd_buf(user_data, cmd_buf, 0);
}

static bmkernel_handle_t bmkernel_handle = NULL;

void test_bmkernel_init(void)
{
  bmkernel_info_t bmkernel_info;
  bmkernel_info.chip_version = BM_CHIP_BM1682;
  bmkernel_info.node_shift = 0;
  bmkernel_info.npu_shift = 6;
  bmkernel_info.eu_shift = 5;
  bmkernel_info.local_mem_shift = 18;
  bmkernel_info.local_mem_banks = 4;
  bmkernel_info.cmd_buf_size = 1000;
  bmkernel_info.emit_on_update = false;
  bmkernel_info.alloc_cmd_buf = fw_alloc_cmd_buf;
  bmkernel_info.emit_cmd_buf = fw_emit_cmd_buf;
  bmkernel_info.free_cmd_buf = fw_free_cmd_buf;
  bmkernel_info.sync = fw_sync;
  bmkernel_info.debug = NULL;

  bmkernel_register(&bmkernel_info, 0,
      (void *)NULL, &bmkernel_handle);

  resync_cmd_id(&cmd_id_node);

  kernel_enter(bmkernel_handle);
}

void test_bmkernel_exit(void)
{
  bmkernel_cleanup(bmkernel_handle);
}
