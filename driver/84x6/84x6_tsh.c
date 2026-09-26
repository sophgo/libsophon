#ifndef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": %s:%d: " fmt, __func__, __LINE__
#endif

#include "bm_common.h"
#include "84x6_tsh.h"
#include "84x6_task.h"


/* PCIE mode: addr is a full device address, resolved per access through the
 * bar1 part windows (bm_get_bar_offset). SOC mode keeps the linear
 * tpu_bar_vaddr mapping relative to TPUSYS_BASE_ADDR.
 */
#ifndef SOC_MODE
void tpusys_write_32(struct bm_device_info *bmdi, u32 addr, u32 val)
{
    bm_write32(bmdi, addr, val);
}

void tpusys_clrbits_32(struct bm_device_info *bmdi, u32 addr, u32 mask)
{
    u32 val = bm_read32(bmdi, addr);
    val &= ~mask;
    bm_write32(bmdi, addr, val);
}

void tpusys_setbits_32(struct bm_device_info *bmdi, u32 addr, u32 mask)
{
    u32 val = bm_read32(bmdi, addr);
    val |= mask;
    bm_write32(bmdi, addr, val);
}

u32 tpusys_read_32(struct bm_device_info *bmdi, u32 addr)
{
    return bm_read32(bmdi, addr);
}
#else
void tpusys_write_32(struct bm_device_info *bmdi, u32 addr, u32 val)
{
    tpu_reg_write(bmdi, addr - TPUSYS_BASE_ADDR, val);
}

void tpusys_clrbits_32(struct bm_device_info *bmdi, u32 addr, u32 mask)
{
    u32 val = tpu_reg_read(bmdi, addr - TPUSYS_BASE_ADDR);
    val &= ~mask;
    tpu_reg_write(bmdi, addr - TPUSYS_BASE_ADDR, val);
}

void tpusys_setbits_32(struct bm_device_info *bmdi, u32 addr, u32 mask)
{
    u32 val = tpu_reg_read(bmdi, addr - TPUSYS_BASE_ADDR);
    val |= mask;
    tpu_reg_write(bmdi, addr - TPUSYS_BASE_ADDR, val);
}

u32 tpusys_read_32(struct bm_device_info *bmdi, u32 addr)
{
    return tpu_reg_read(bmdi, addr - TPUSYS_BASE_ADDR);
}
#endif

static struct tsh_config tsh_cfg_default[TSH_MAX_NUM_MODE] = {

    {
     .mode = TSH_ASYNC_MODE,
     .active_core_num = 1,
     .active_core_mask = 0x1,
     .active_hwq_mask = 0xff,
      },
    {
     .mode = TSH_SYNC_MODE,
     .active_core_num = 1,
     .active_core_mask = 0x1,
     .active_hwq_mask = 0xff,
      },
    {
     .mode = TSH_BYPASS_MODE,
     .active_core_num = 1,
     .active_core_mask = 0x1,
     .active_outbox_mask = 0xff,
      }
};

int tsh_get_irq_hwq_index(void)
{
    return 0;
}

static void tshfifo_init(struct bm_device_info *bmdi, int coreID,
                         enum tsh_mode mode)
{
    // enable
    tpusys_write_32(bmdi, TXPSYS0_TPU_SCH_FIFO_REG + coreID * TXPSYS_OFFSET,
                    1 | ((mode == TSH_SYNC_MODE) << 1));
    tpusys_write_32(bmdi,
                    TXPSYS0_TSH_FIFO_TSH_BASE_ADDR_L + coreID * TXPSYS_OFFSET,
                    TSH_DONE & 0xffffffff);
    tpusys_write_32(bmdi,
                    TXPSYS0_TSH_FIFO_TSH_BASE_ADDR_H + coreID * TXPSYS_OFFSET,
                    TSH_DONE >> 32);
}


int tsh_init(struct bm_device_info *bmdi, struct tsh_config *cfg)
{
    uint32_t disable_core_mask = 0xFFFFFFFF;
    uint32_t depth;
    int i = 0;
    unsigned long timeout;

    if (cfg == NULL)
        cfg = &tsh_cfg_default[TSH_SYNC_MODE];

    cfg->active_core_num = bmdi->cinfo.tpu_core_num;
    if (cfg->active_core_num <= 0 || cfg->active_core_num > 4) {
        pr_err("invalid core num:%d\n", cfg->active_core_num);
        return -1;
    }

    for (i = 0; i < cfg->active_core_num; i++) {
        cfg->active_core_mask |= 1 << i;
    }

    // rst tsh
    tpusys_write_32(bmdi, TSH_RST_CLK_REG, 1 << 16);
    tpusys_setbits_32(bmdi, TSH_RST_CLK_REG, 1);
    // wait tsh rst done (with timeout)
    timeout = jiffies + msecs_to_jiffies(1000);
    while (!(tpusys_read_32(bmdi, TSH_RST_CLK_REG) & (0x1 << 8))) {
        if (time_after(jiffies, timeout)) {
            pr_err("TSH reset timed out\n");
            goto err;
        }
        cpu_relax();
    }
    tpusys_clrbits_32(bmdi, TSH_RST_CLK_REG, 1);

    // verify TSH block is accessible (0xffffffff indicates PCIe read failure)
    depth = tpusys_read_32(bmdi, TSH_RST_CLK_REG);
    if (depth == 0xffffffff) {
        pr_err("TSH cfg block not accessible (0x%x), device may not be ready\n",
               depth);
        goto err;
    }
    // programing tsh disable core
    for (i = 0; i < MAX_CORE_NUM; i++) {
        if (cfg->active_core_mask & BIT(i)) // TODO
            disable_core_mask &= ~BIT(i);
    }
    if (tpusys_read_32(bmdi, TSH_DISABLE_CORE) != disable_core_mask)
        tpusys_write_32(bmdi, TSH_DISABLE_CORE, disable_core_mask);
    // enable tshfifo
    for (i = 0; i < MAX_CORE_NUM; i++) {
        if (cfg->active_core_mask & BIT(i))
            tshfifo_init(bmdi, i, cfg->mode);
    }
    // timing sync, write 1 to enable
    tpusys_write_32(bmdi, TSH_TIMING_SYNC_REG, 1);
    // set tsh mode
    tpusys_write_32(bmdi, TSH_MODE_REG, cfg->mode);
    // set tsh tcore base
    for (i = 0; i < MAX_CORE_NUM; i++) {
        if (cfg->active_core_mask & BIT(i))
            tpusys_write_32(bmdi, TSH_TCORE0_BASE_ADDR + i * 0x8,
                            TXPSYS0_TSH_FIFO_INSTR + i * TXPSYS_OFFSET);
    }
    switch (cfg->mode) {
    case TSH_BYPASS_MODE:
        {
            depth = tpusys_read_32(bmdi, TSH_OUTBOX_DEPTH0123);
            if (depth != 0x40404040) {
                pr_err("TSH_OUTBOX_DEPTH0123 is not empty, depth: 0x%x\n",
                       depth);
                goto err;
            }

            depth = tpusys_read_32(bmdi, TSH_OUTBOX_DEPTH4567);
            if (depth != 0x40404040) {
                pr_err("TSH_OUTBOX_DEPTH4567 is not empty, depth: 0x%x\n",
                       depth);
                goto err;
            }
        }
        break;
    case TSH_SYNC_MODE:
        {
            // set sync map dir
            tpusys_write_32(bmdi, TSH_SYNC_MAP_DIR_REG, cfg->sync_map_dir);
            // enable timeout
            tpusys_write_32(bmdi, TSH_TIMEOUT_EN, 1);
            depth = tpusys_read_32(bmdi, TSH_HWQ_DEPTH0123);
            if (depth != 0x10101010) {
                pr_err("TSH_HWQ_DEPTH0123 is not empty, depth: 0x%x\n", depth);
                goto err;
            }
            depth = tpusys_read_32(bmdi, TSH_HWQ_DEPTH4567);
            if (depth != 0x10101010) {
                pr_err("TSH_HWQ_DEPTH4567 is not empty, depth: 0x%x\n", depth);
                goto err;
            }
        }
        break;
    case TSH_ASYNC_MODE:
        {
            // set async core config
            for (i = 0; i < HWQ_NUM; i += 2) {
                union tsh_async_hwq_config config = {
                    .first_hwq_tpu_core_num = cfg->async_hwq_core_num_conf[i],
                    .first_hwq_start_core_id =
                        cfg->async_hwq_start_core_conf[i],
                    .second_hwq_tpu_core_num =
                        cfg->async_hwq_core_num_conf[i + 1],
                    .second_hwq_start_core_id =
                        cfg->async_hwq_start_core_conf[i + 1],
                };
                tpusys_write_32(bmdi, TSH_ASYNC_CONFIG01 + i / 2 * 0x4,
                                config.config);
            }
            // set msgid threshold TODO: 32 is default value
            tpusys_write_32(bmdi, TSH_MSGID_THR, 32);
            // confirm hwq fifo is empty
            depth = tpusys_read_32(bmdi, TSH_HWQ_DEPTH0123);
            if (depth != 0x10101010)
                pr_err("TSH_HWQ_DEPTH0123 is not empty, depth: 0x%x\n", depth);
            depth = tpusys_read_32(bmdi, TSH_HWQ_DEPTH4567);
            if (depth != 0x10101010)
                pr_err("TSH_HWQ_DEPTH4567 is not empty, depth: 0x%x\n", depth);
        }
        break;
    default:
        {
            pr_err("tsh_init: unknown mode %d\n", cfg->mode);
            goto err;
        }
        break;
    }
    return 0;
  err:
    return -1;
}

int tsh_setmode(en_tsh_mode mode)
{
    return 0;
}

en_tsh_mode tsh_getmode(struct bm_device_info *bmdi)
{
    en_tsh_mode mode = (en_tsh_mode) (tpusys_read_32(bmdi, TSH_MODE_REG) & 0x3);
    return mode;
}

int tsh_get_active_queue_mask(struct bm_device_info *bmdi)
{
    /* all HWQs are active when TSH is in sync mode */
    return 0xff;
}

int tsh_get_hwq_free_slots(struct bm_device_info *bmdi, int idx)
{
    int free_slots = tpusys_read_32(bmdi,
                                    TSH_HWQ_DEPTH0123 +
                                    (idx / 4) * 0x4) & (0xff << ((idx % 4) *
                                                                 8));
    return free_slots;
}

int tsh_get_outbox_free_slots(struct bm_device_info *bmdi, int idx)
{
    int free_slots = tpusys_read_32(bmdi,
                                    TSH_OUTBOX_DEPTH0123 +
                                    (idx / 4) * 0x4) & (0xff << ((idx % 4) *
                                                                 8));
    return free_slots;
}

int tsh_write_hwq(struct bm_device_info *bmdi, int idx, u32 packet)
{
    tpusys_write_32(bmdi, TSH_KENEL_PACKET_HWQ0 + idx * 0x4, packet);
    return 0;
}

int tsh_write_outbox(struct bm_device_info *bmdi, int idx, u64 packet)
{
    // The lower 32bit must be written first
    tpusys_write_32(bmdi, TSH_OUTBOX0_PACKET + idx * 0x8, packet & 0xffffffff);
    tpusys_write_32(bmdi, TSH_OUTBOX0_PACKET + idx * 0x8 + 0x4, packet >> 32);
    return 0;
}

int tsh_clr_done_count(struct bm_device_info *bmdi, int idx)
{
    tpusys_write_32(bmdi, TSH_HWQ0_DONE_CNT, 1 << idx);
    return 0;
}

u64 tsh_get_task_done_count(struct bm_device_info *bmdi, int idx)
{
    return tpusys_read_32(bmdi, TSH_HWQ0_DONE_CNT + 0x4 * idx);
}

void tsh_clear_irq(struct bm_device_info *bmdi)
{
    tpusys_write_32(bmdi, TPU_SYS_REG_BASE + TPU_SYS_INT_CLR, 0xFFFFFFFF);
    return;
}

int tsh_active_core_num(struct bm_device_info *bmdi)
{
    return bmdi ? bmdi->cinfo.tpu_core_num : 0;
}
