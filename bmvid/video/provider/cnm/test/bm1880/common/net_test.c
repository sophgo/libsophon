#include "boot_test.h"
#include "net_test.h"
#include "engine_internal.h"
#include "engine_io.h"
#include "debug.h"

#define TIMER_CTRL_REG 0x50022008
#define TIMER_COUNT_WR_REG 0x50022000
#define TIMER_COUNT_RD_REG 0x50022004
#define TIMER_PERIOD_NS 20

static void node_timer_init()
{
  fw_write32(TIMER_CTRL_REG, 0x0);
  fw_write32(TIMER_COUNT_WR_REG, 0xffffffff);
  fw_write32(TIMER_CTRL_REG, 0x3);
}

static u32 node_timer_get_cycle() {
  unsigned int cycle = fw_read32(TIMER_COUNT_RD_REG);
  return ((u32)0xffffffff - cycle);
}

static float elapse_ms(u32 start) {
  u32 stop = node_timer_get_cycle();
  return (float)((stop - start) * TIMER_PERIOD_NS)/1000000;
}

static void set_bdc_task_descriptor_dma(u64 desc_offset)
{
  uartlog("set_bdc_task_descriptor_dma desc_offset:0x%lx\n", desc_offset);
  fw_write32(BD_ENGINE_DESC_ADDR, (desc_offset >> 8));
  fw_write32(BD_ENGINE_MAIN_CTRL, (1 << 30));
}

static void set_gdma_task_descriptor_dma(u64 desc_offset)
{
  uartlog("set_gdma_task_descriptor_dma desc_offset:0x%lx\n", desc_offset);
  fw_write32(GDMA_ENGINE_BASE_ADDR + 0x4, (desc_offset >> 6));
  fw_write32(GDMA_ENGINE_MAIN_CTRL, 0xc3);
}

static void cmdbuf_exec(DESC_OFFSET *desc_off, DESC_NUM *desc_num)
{
  CMD_ID_NODE id_node;

  float ms = 0.0;
  u32 bdc_desc_offset = desc_off->bdc;
  u32 gdma_desc_offset = desc_off->gdma;

  uartlog("bdc_desc_offset %u gdma_desc_offset %u\n",
          bdc_desc_offset, gdma_desc_offset);

  int bdc_desc_num = desc_num->bdc;
  int gdma_desc_num = desc_num->gdma;

  uartlog("bdc_desc_num %d gdma_desc_num %d\n",
          bdc_desc_num, gdma_desc_num);

  resync_cmd_id(&id_node);

  id_node.bd_cmd_id = bdc_desc_num;
  id_node.gdma_cmd_id = gdma_desc_num;
  id_node.cdma_cmd_id = 0;

  uartlog("%s %d: polling cmd id <bd:%d gdma:%d\n",
            __func__, __LINE__,
            id_node.bd_cmd_id,
            id_node.gdma_cmd_id);

  node_timer_init();
  u32 start = node_timer_get_cycle();

  if (gdma_desc_num > 0) {
    set_gdma_task_descriptor_dma(gdma_desc_offset + GLOBAL_MEM_START_ADDR);
  }

  if(bdc_desc_num > 0) {
    set_bdc_task_descriptor_dma(bdc_desc_offset + GLOBAL_MEM_START_ADDR);
  }

  u64 idx = 0;

  while(1) {
    u32 bd_id = current_id((BD_ENGINE_BASE_ADDR + 0x8), 16, 16);
    u32 gdma_id = current_id((GDMA_ENGINE_BASE_ADDR + 0xC), 16, 16);
    if ( bd_id >= id_node.bd_cmd_id && gdma_id >= id_node.gdma_cmd_id) {
      ms = elapse_ms(start);
      break;
    }

    idx ++;
    if ((idx % 1000000) == 0)
      uartlog("current npu cmd id <bd:%u, gdma:%u>\n", bd_id, gdma_id);
  }

  poll_all_engine_done(&id_node);

  uartlog("poll all engine done. duration:%f ms\n", ms);
}

#define ARM_ENGINE_DESCRIPTOR_NUM     32
#define ARM_CMD_ACCPI3                3
#define ARM_CMD_ACCPI4                4
#define ARM_CMD_ACCPI5                5
#define ARM_CMD_ACCPI6                6

typedef struct {
  u32 regs[7];
  union {
    u32 regs[ARM_ENGINE_DESCRIPTOR_NUM - 7];
    char str[100];
  } u;
} ARM_DESC;

static void cmdbuf_exec_with_segments(u32 cmdbuf_addr, int segments)
{
  CMD_ID_NODE id_node;
  u32 ms = 0;

  ARM_DESC *desc = (ARM_DESC *)GA_TO_PTR(cmdbuf_addr);

  node_timer_init();
  for(int i = 0; i < segments; i++, desc++) {
    int bd_num = desc->regs[ARM_CMD_ACCPI3];
    int gdma_num = desc->regs[ARM_CMD_ACCPI4];
    u32 bd_offset = desc->regs[ARM_CMD_ACCPI5];
    u32 gdma_offset = desc->regs[ARM_CMD_ACCPI6];

    uartlog("num<bd:%d, gdma:%d>, addr<bd:0x%x, gdma:0x%x>\n",
             bd_num, gdma_num, bd_offset, gdma_offset);

    resync_cmd_id(&id_node);

    id_node.bd_cmd_id = bd_num;
    id_node.gdma_cmd_id = gdma_num;
    id_node.cdma_cmd_id = 0;


    u32 start = node_timer_get_cycle();

    if (gdma_num > 0) {
      set_gdma_task_descriptor_dma(cmdbuf_addr + gdma_offset + GLOBAL_MEM_START_ADDR);
    }

    if(bd_num > 0) {
      set_bdc_task_descriptor_dma(cmdbuf_addr + bd_offset + GLOBAL_MEM_START_ADDR);
    }

    poll_all_engine_done(&id_node);
    ms = elapse_ms(start);
    uartlog("time-consuming %d ms\n", ms);
  }
}

static void dump_output(u32 output_addr, u32 count)
{
  float *out = (float *)GA_TO_PTR(output_addr);

  count = 8 * ((count + 7) / 8) ;
  if (count > 64)
    count = 64;

  for(int i = 0; i < count; i += 8) {
    uartlog("%.7f %.7f %.7f %.7f %.7f %.7f %.7f %.7f\n",
      out[i], out[i+1], out[i+2], out[i+3],
      out[i+4], out[i+5], out[i+6], out[i+7]);
  }
}

#define EPSILON 0.00001
static int validate_result(u64 out_addr, u64 ref_addr, int count)
{
  //0.00001:epsilon, |exp - got| compare with a epsilon
  int err = globalmem_cmp_epsilon(ref_addr, out_addr, count, 0, EPSILON);
  if (err) {
    uartlog("bmnet compare failed:%d\n", err);
  } else {
    uartlog("bmnet compare OK\n");
  }

  return err;
}

int do_net_test(NET_INFO_T *net, int with_segments)
{
  u32 output_addr = net->output_addr;
  u32 output_size = net->output_size;
  u32 output_ref_addr = net->output_ref_addr;

  if(with_segments) {
    cmdbuf_exec_with_segments(net->cmdbuf_addr, net->segments);
  } else {
    cmdbuf_exec(&net->desc_off, &net->desc_num);
  }

  int err = validate_result(output_addr, output_ref_addr, output_size / sizeof(float));
  if (err) {
    dump_output(output_addr, output_size / sizeof(u32));
  }

  return err;
}
