//=====================================================================
//  This file is linux device driver for VPU.
//---------------------------------------------------------------------
//  This confidential and proprietary software may be used only
//  as authorized by a licensing agreement from BITMAIN Inc.
//  In the event of publication, the following notice is applicable:
//
//  (C) COPYRIGHT 2015 - 2025  BITMAIN INC. ALL RIGHTS RESERVED
//
//  The entire notice above must be reproduced on all authorized copies.
//
//=====================================================================
#include "../bm_common.h"
#include "../bm1684/bm1684_reg.h"
#include "vpu.tmh"


//#ifdef ENABLE_DEBUG_MSG
//#define TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,args...)    TraceEvents(TRACE_LEVEL_INFORMATION,args)
//#else
////#define TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,args...)
//#endif

#define VPU_SUPPORT_ISR
#ifdef VPU_SUPPORT_ISR
//#define VPU_IRQ_CONTROL
#endif

#define VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#define VPU_SUPPORT_CLOSE_COMMAND

#define BIT_BASE                    (0x0000)
#define BIT_INT_CLEAR               (BIT_BASE + 0x00C)
#define BIT_INT_STS                 (BIT_BASE + 0x010)
#define BIT_BUSY_FLAG               (BIT_BASE + 0x160)
#define BIT_RUN_COMMAND             (BIT_BASE + 0x164)
#define BIT_RUN_INDEX               (BIT_BASE + 0x168)
#define BIT_RUN_COD_STD             (BIT_BASE + 0x16C)
#define BIT_INT_REASON              (BIT_BASE + 0x174)


/* WAVE5 registers */
#define W5_REG_BASE                 (0x0000)
#define W5_VPU_INT_REASON_CLEAR     (W5_REG_BASE + 0x0034)
#define W5_VPU_HOST_INT_REQ         (W5_REG_BASE + 0x0038)
#define W5_VPU_VINT_CLEAR           (W5_REG_BASE + 0x003C)
#define W5_VPU_VPU_INT_STS          (W5_REG_BASE + 0x0044)   //
#define W5_VPU_INT_REASON           (W5_REG_BASE + 0x004c)   //
#define W5_VPU_BUSY_STATUS          (W5_REG_BASE + 0x0070)
#define W5_VPU_HOST_INT_REQ         (W5_REG_BASE + 0x0038)
#define W5_RET_SUCCESS              (W5_REG_BASE + 0x0108)
#define W5_RET_FAIL_REASON          (W5_REG_BASE + 0x010C)

#ifdef VPU_SUPPORT_CLOSE_COMMAND
#define W5_RET_STAGE0_INSTANCE_INFO     (W5_REG_BASE + 0x01EC)
// for debug the current status
#define W5_COMMAND                      (W5_REG_BASE + 0x0100)
#define W5_RET_QUEUE_STATUS             (W5_REG_BASE + 0x01E0)
#define W5_RET_STAGE1_INSTANCE_INFO     (W5_REG_BASE + 0x01F0)
#define W5_RET_STAGE2_INSTANCE_INFO     (W5_REG_BASE + 0x01F4)
#define W5_CMD_INSTANCE_INFO            (W5_REG_BASE + 0x0110)
#endif

#define W5_RET_BS_EMPTY_INST        (W5_REG_BASE + 0x01E4)
#define W5_RET_QUEUE_CMD_DONE_INST  (W5_REG_BASE + 0x01E8)
#define W5_RET_DONE_INSTANCE_INFO   (W5_REG_BASE + 0x01FC)
#define VPU_PRODUCT_CODE_REGISTER   (BIT_BASE    + 0x1044)

#ifndef VM_RESERVED /*for kernel up to 3.7.0 version*/
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define VPU_REMAP_REG 0x50010064
#define VPU_REG_SIZE  0x10000
#define SW_RESET_REG1 0x50010c04
#define SW_RESET_REG2 0x50010c08

#define TOP_CLK_EN_REG1 0x50010804
#define CLK_EN_AXI_VD0_WAVE1_GEN_REG1 25
#define CLK_EN_APB_VD0_WAVE1_GEN_REG1 24
#define CLK_EN_AXI_VD0_WAVE0_GEN_REG1 23
#define CLK_EN_APB_VD0_WAVE0_GEN_REG1 22
#define CLK_EN_AXI_VDE_AXI_BRIDGE_GEN_REG1 21
#define CLK_EN_APB_VDE_WAVE_GEN_REG1 20
#define CLK_EN_AXI_VDE_WAVE_GEN_REG1 19
#define TOP_CLK_EN_REG2 0x50010808
#define CLK_EN_AXI_VD1_WAVE1_GEN_REG2 3
#define CLK_EN_APB_VD1_WAVE1_GEN_REG2 2
#define CLK_EN_AXI_VD1_WAVE0_GEN_REG2 1
#define CLK_EN_APB_VD1_WAVE0_GEN_REG2 0
#define NORMAL_MODE 0X0

#define vpu_read_register(core, addr) \
    bm_read32(bmdi, bmdi->vpudrvctx.s_vpu_reg_phy_base[core] + \
            (u32)bmdi->vpudrvctx.s_bit_firmware_info[core].reg_base_offset + addr)

#define vpu_write_register(core, addr, val) \
    bm_write32(bmdi, bmdi->vpudrvctx.s_vpu_reg_phy_base[core] + \
            (u32)bmdi->vpudrvctx.s_bit_firmware_info[core].reg_base_offset + addr, val)

#define BM1682_VPU_IRQ_NUM_0 (1000)
#define BM1682_VPU_IRQ_NUM_1 (1001)
#define BM1682_VPU_IRQ_NUM_2 (1002)

#define BM1684_VPU_IRQ_NUM_0 (31)
#define BM1684_VPU_IRQ_NUM_1 (32)
#define BM1684_VPU_IRQ_NUM_2 (64 + 51)
#define BM1684_VPU_IRQ_NUM_3 (64 + 52)
#define BM1684_VPU_IRQ_NUM_4 (64 + 0)

#define TOP_PLL_CTRL(fbdiv, p1, p2, refdiv) \
    (((fbdiv & 0xfff) << 16) | ((p2 & 0x7) << 12) | ((p1 & 0x7) << 8) | (refdiv & 0x1f))
#define PLL_STAT_LOCK_OFFSET    0x8

#define VPU_HEAP_ID_3  3    /* heap is 3 for vpu*/

struct bm_pll_ctrl {
    u32 fbdiv;
    u32 postdiv1;
    u32 postdiv2;
    u32 refdiv;
};

static u32 s_vpu_irq_1682[] = {
    BM1682_VPU_IRQ_NUM_0,
    BM1682_VPU_IRQ_NUM_1,
    BM1682_VPU_IRQ_NUM_2,
};

static u32 s_vpu_irq_1684[] = {
    BM1684_VPU_IRQ_NUM_0,
    BM1684_VPU_IRQ_NUM_1,
    BM1684_VPU_IRQ_NUM_2,
    BM1684_VPU_IRQ_NUM_3,
    BM1684_VPU_IRQ_NUM_4
};

static u32 s_vpu_irq_1686[] = {
    BM1684_VPU_IRQ_NUM_0,
    BM1684_VPU_IRQ_NUM_2,
    BM1684_VPU_IRQ_NUM_4
};

static u32 s_vpu_reg_phy_base_1682[] = {
    0x50440000,
    0x50450000,
    0x50460000,
};

static u32 s_vpu_reg_phy_base_1684[] = {
    0x50050000,
    0x50060000,
    0x500D0000,
    0x500E0000,
    0x50126000,
};

static u32 s_vpu_reg_phy_base_1686[] = {
    0x50050000,
    0x500D0000,
    0x50126000,
};

struct vpu_reset_info {
    u32 reg;
    u32 bit_n;
};

static struct vpu_reset_info bm_vpu_rst[MAX_NUM_VPU_CORE] = {
    {SW_RESET_REG1, 18},
    {SW_RESET_REG1, 19},
    {SW_RESET_REG1, 28},
    {SW_RESET_REG1, 29},
    {SW_RESET_REG2, 10},
};

static struct vpu_reset_info bm_vpu_rst_1686[MAX_NUM_VPU_CORE_BM1686] = {
    {SW_RESET_REG1, 18},
    {SW_RESET_REG1, 28},
    {SW_RESET_REG2, 10},
};

static struct vpu_reset_info bm_vpu_en_axi_clk[MAX_NUM_VPU_CORE] = {
    {TOP_CLK_EN_REG1, CLK_EN_AXI_VD0_WAVE0_GEN_REG1},
    {TOP_CLK_EN_REG1, CLK_EN_AXI_VD0_WAVE1_GEN_REG1},
    {TOP_CLK_EN_REG2, CLK_EN_AXI_VD1_WAVE0_GEN_REG2},
    {TOP_CLK_EN_REG2, CLK_EN_AXI_VD1_WAVE1_GEN_REG2},
    {TOP_CLK_EN_REG1, CLK_EN_AXI_VDE_WAVE_GEN_REG1},
};

static struct vpu_reset_info bm_vpu_en_axi_clk_1686[MAX_NUM_VPU_CORE_BM1686] = {
    {TOP_CLK_EN_REG1, CLK_EN_AXI_VD0_WAVE0_GEN_REG1},
    {TOP_CLK_EN_REG2, CLK_EN_AXI_VD1_WAVE0_GEN_REG2},
    {TOP_CLK_EN_REG1, CLK_EN_AXI_VDE_WAVE_GEN_REG1},
};

static struct vpu_reset_info bm_vpu_en_apb_clk[MAX_NUM_VPU_CORE] = {
    {TOP_CLK_EN_REG1, CLK_EN_APB_VD0_WAVE0_GEN_REG1},
    {TOP_CLK_EN_REG1, CLK_EN_APB_VD0_WAVE1_GEN_REG1},
    {TOP_CLK_EN_REG2, CLK_EN_APB_VD1_WAVE0_GEN_REG2},
    {TOP_CLK_EN_REG2, CLK_EN_APB_VD1_WAVE1_GEN_REG2},
    {TOP_CLK_EN_REG1, CLK_EN_APB_VDE_WAVE_GEN_REG1},
};

static struct vpu_reset_info bm_vpu_en_apb_clk_1686[MAX_NUM_VPU_CORE_BM1686] = {
    {TOP_CLK_EN_REG1, CLK_EN_APB_VD0_WAVE0_GEN_REG1},
    {TOP_CLK_EN_REG2, CLK_EN_APB_VD1_WAVE0_GEN_REG2},
    {TOP_CLK_EN_REG1, CLK_EN_APB_VDE_WAVE_GEN_REG1},
};

enum init_op {
    FREE,
    INIT,
};

typedef enum {
    INT_WAVE5_INIT_VPU          = 0,
    INT_WAVE5_WAKEUP_VPU        = 1,
    INT_WAVE5_SLEEP_VPU         = 2,

    INT_WAVE5_CREATE_INSTANCE   = 3,
    INT_WAVE5_FLUSH_INSTANCE    = 4,
    INT_WAVE5_DESTORY_INSTANCE  = 5,

    INT_WAVE5_INIT_SEQ          = 6,
    INT_WAVE5_SET_FRAMEBUF      = 7,
    INT_WAVE5_DEC_PIC           = 8,
    INT_WAVE5_ENC_PIC           = 8,
    INT_WAVE5_ENC_SET_PARAM     = 9,
    INT_WAVE5_ENC_LOW_LATENCY   = 13,
    INT_WAVE5_DEC_QUERY         = 14,
    INT_WAVE5_BSBUF_EMPTY       = 15,
    INT_WAVE5_BSBUF_FULL        = 15,
} Wave5InterruptBit;

struct bmdi_list {
    struct bm_device_info *bmdi;
    //struct list_head list;
};

static int vpu_polling_create;
static int vpu_open_inst_count;
static struct task_struct *irq_task;

static int bm_vpu_reset_core(struct bm_device_info *bmdi, int core_idx, int reset);
static int bm_vpu_hw_reset(struct bm_device_info *bmdi, int core_idx);

NTSTATUS MapMemory(_Inout_ vpudrv_buffer_t *vb, _In_ MEMORY_CACHING_TYPE cacheMode);

NTSTATUS UnMapMemory(_Inout_ vpudrv_buffer_t *vb);

static int get_max_num_vpu_core(struct bm_device_info *bmdi) {

    if(bmdi->cinfo.chip_id == 0x1686) {
            return MAX_NUM_VPU_CORE_BM1686;
    }
    return MAX_NUM_VPU_CORE;
}


static u64    bm_vpu_get_tick_count() {
    LARGE_INTEGER CurTime, Freq;
    CurTime   = KeQueryPerformanceCounter(&Freq);
    return (u64)((CurTime.QuadPart * 1000) / Freq.QuadPart);
}

static int bm_vpu_clk_unprepare(struct bm_device_info *bmdi, int core_idx)
{
    u32 value = 0;

        if (bmdi->cinfo.chip_id == 0x1684) {
                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);
                value &= ~(0x1 << bm_vpu_en_apb_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_apb_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);

                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);
                value &= ~(0x1 << bm_vpu_en_axi_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_axi_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);
        }


        if (bmdi->cinfo.chip_id == 0x1686) {
                value = bm_read32(bmdi, bm_vpu_en_apb_clk_1686[core_idx].reg);
                value &= ~(0x1 << bm_vpu_en_apb_clk_1686[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_apb_clk_1686[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_apb_clk_1686[core_idx].reg);

                value = bm_read32(bmdi, bm_vpu_en_axi_clk_1686[core_idx].reg);
                value &= ~(0x1 << bm_vpu_en_axi_clk_1686[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_axi_clk_1686[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_axi_clk_1686[core_idx].reg);
        }

        return value;
}

static int bm_vpu_clk_prepare(struct bm_device_info *bmdi, int core_idx)
{
    u32 value = 0;

        if (bmdi->cinfo.chip_id == 0x1684) {
                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);
                value |= (0x1 << bm_vpu_en_axi_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_axi_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);

                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);
                value |= (0x1 << bm_vpu_en_apb_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_apb_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);
        }

        if (bmdi->cinfo.chip_id == 0x1686) {
                value = bm_read32(bmdi, bm_vpu_en_axi_clk_1686[core_idx].reg);
                value |= (0x1 << bm_vpu_en_axi_clk_1686[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_axi_clk_1686[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_axi_clk_1686[core_idx].reg);

                value = bm_read32(bmdi, bm_vpu_en_apb_clk_1686[core_idx].reg);
                value |= (0x1 << bm_vpu_en_apb_clk_1686[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_apb_clk_1686[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_apb_clk_1686[core_idx].reg);
        }

        return value;
}

#ifdef VPU_SUPPORT_CLOCK_CONTROL
static void bm_vpu_clk_enable(int op)
{
}
#endif

#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
typedef enum {
    CORE_LOCK,
    CORE_DISPLAY_LOCK,
} LOCK_INDEX;

static int get_lock(struct bm_device_info *bmdi, int core_idx, LOCK_INDEX lock_index)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    volatile LONG* addr = (LONG*)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE * 4);
    LONG val = 0;

    LONG val1  = (u32)(u64)PsGetCurrentThreadId();
    LONG val2  = (u32)(u64)PsGetCurrentProcessId();
    int ret = 1;
    LONG      count = 0;
    addr += lock_index;

    while ((val = InterlockedCompareExchange(addr, val1, 0)) != 0) {
        if(val == val1 || val==val2) {
            ret = 1;
            break;
        } else {
            val = 0;
        }
        if(count >= 5000) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"can't get lock, org: %d, ker: %d", *addr, val1);
            ret = 0;
            break;
        }
        bm_mdelay(2);
        count += 1;
    }

    return ret;
}
static void release_lock(struct bm_device_info *bmdi, int core_idx, LOCK_INDEX lock_index)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    volatile LONG *addr = (LONG*)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    addr += lock_index;
    InterlockedExchange(addr, 0);
}

static int get_vpu_create_inst_flag(struct bm_device_info *bmdi, int core_idx)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    volatile LONG*addr = (LONG*)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    return InterlockedExchangeAdd((LONG*)(addr + 2), 0);
}
static void release_vpu_create_inst_flag(struct bm_device_info *bmdi, int core_idx, int inst_idx)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    volatile LONG *addr = (LONG*)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    volatile LONG vpu_create_inst_flag = get_vpu_create_inst_flag(bmdi, core_idx);
    InterlockedExchange((LONG*)(addr + 2), (vpu_create_inst_flag & (~(1 << inst_idx))));
}

#ifdef VPU_SUPPORT_CLOSE_COMMAND
int CodaCloseInstanceCommand(struct bm_device_info *bmdi, int core, u32 instanceIndex)
{
    int ret = 0;
    LONGLONG      timeout = 1000; /* vpu wait timeout to 1sec */
    u64         start = 0;
    start   = bm_vpu_get_tick_count();

    vpu_write_register(core, BIT_BUSY_FLAG, 1);
    vpu_write_register(core, BIT_RUN_INDEX, instanceIndex);
#define DEC_SEQ_END 2
    vpu_write_register(core, BIT_RUN_COMMAND, DEC_SEQ_END);

    while (vpu_read_register(core, BIT_BUSY_FLAG)) {

        if (bm_vpu_get_tick_count() > start + timeout) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"CodaCloseInstanceCommand after BUSY timeout");
            ret = 1;
            goto DONE_CMD_CODA;
        }
    }
DONE_CMD_CODA:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"sent CodaCloseInstanceCommand 55 ret=%d", ret);
    return ret;
}
static int WaitBusyTimeout(struct bm_device_info *bmdi, u32 core, u32 addr)
{
    unsigned long timeout = 1000; /* vpu wait timeout to 1sec */
    u64           start   = bm_vpu_get_tick_count();

    while (vpu_read_register(core, addr)) {
        if (bm_vpu_get_tick_count() > start + timeout) {
            return 1;
        }
    }
    return 0;
}
#define W5_CMD_DEC_SET_DISP_IDC             (W5_REG_BASE + 0x0118)
#define W5_CMD_DEC_CLR_DISP_IDC             (W5_REG_BASE + 0x011C)
#define W5_QUERY_OPTION             (W5_REG_BASE + 0x0104)
static void Wave5BitIssueCommandInst(struct bm_device_info *bmdi, u32 core, u32 instanceIndex, u32 cmd)
{
    vpu_write_register(core, W5_CMD_INSTANCE_INFO,  instanceIndex&0xffff);
    vpu_write_register(core, W5_VPU_BUSY_STATUS, 1);
    vpu_write_register(core, W5_COMMAND, cmd);
    vpu_write_register(core, W5_VPU_HOST_INT_REQ, 1);
}

static int SendQuery(struct bm_device_info *bmdi, u32 core, u32 instanceIndex, u32 queryOpt)
{
    // Send QUERY cmd
    vpu_write_register(core, W5_QUERY_OPTION, queryOpt);
    vpu_write_register(core, W5_VPU_BUSY_STATUS, 1);
#define W5_QUERY           0x4000
    Wave5BitIssueCommandInst(bmdi, core, instanceIndex, W5_QUERY);

    if(WaitBusyTimeout(bmdi, core, W5_VPU_BUSY_STATUS)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "SendQuery BUSY timeout");
        return 2;
    }

    if (vpu_read_register(core, W5_RET_SUCCESS) == 0)
        return 1;

    return 0;
}

static int Wave5DecClrDispFlag(struct bm_device_info *bmdi, u32 core, u32 instanceIndex, u32 index)
{
    int ret = 0;

    vpu_write_register(core, W5_CMD_DEC_CLR_DISP_IDC, (1<<index));
    vpu_write_register(core, W5_CMD_DEC_SET_DISP_IDC, 0);
#define UPDATE_DISP_FLAG     3
    ret = SendQuery(bmdi, core, instanceIndex, UPDATE_DISP_FLAG);

    if (ret != 0) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "Wave5DecClrDispFlag QUERY FAILURE\n");
        return 1;
    }

    return 0;
}
#define W5_BS_OPTION                            (W5_REG_BASE + 0x0120)
static int Wave5VpuDecSetBitstreamFlag(struct bm_device_info *bmdi, u32 core, u32 instanceIndex)
{
    vpu_write_register(core, W5_BS_OPTION, 2);
#define W5_UPDATE_BS       0x8000
    Wave5BitIssueCommandInst(bmdi, core, instanceIndex, W5_UPDATE_BS);
    if(WaitBusyTimeout(bmdi, core, W5_VPU_BUSY_STATUS)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "Wave5VpuDecSetBitstreamFlag BUSY timeout");
        return 2;
    }

    if (vpu_read_register(core, W5_RET_SUCCESS) == 0) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "Wave5VpuDecSetBitstreamFlag failed.\n");
        return 1;
    }

    return 0;
}
#define W5_CMD_DEC_ADDR_REPORT_BASE         (W5_REG_BASE + 0x0114)
#define W5_CMD_DEC_REPORT_SIZE              (W5_REG_BASE + 0x0118)
#define W5_CMD_DEC_REPORT_PARAM             (W5_REG_BASE + 0x011C)
#define W5_CMD_DEC_GET_RESULT_OPTION        (W5_REG_BASE + 0x0128)

static int FlushDecResult(struct bm_device_info *bmdi, u32 core, u32 instanceIndex)
{
    int     ret = 0;
    u32     regVal;

    vpu_write_register(core, W5_CMD_DEC_ADDR_REPORT_BASE, 0);
    vpu_write_register(core, W5_CMD_DEC_REPORT_SIZE,      0);
    vpu_write_register(core, W5_CMD_DEC_REPORT_PARAM,     31);
    //vpu_write_register(core, W5_CMD_DEC_GET_RESULT_OPTION, 0x1);

#define GET_RESULT 2
    // Send QUERY cmd
    ret = SendQuery(bmdi, core, instanceIndex, GET_RESULT);
    if (ret != 0) {
        regVal = vpu_read_register(core, W5_RET_FAIL_REASON);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"flush result reason: 0x%x", regVal);
        return 1;
    }

    return 0;
}
int Wave5CloseInstanceCommand(struct bm_device_info *bmdi, int core, u32 instanceIndex)
{
    unsigned long timeout = 1000; /* vpu wait timeout to 1sec */
    int ret = 0;
    u64           start   = bm_vpu_get_tick_count();

#define W5_DESTROY_INSTANCE 0x0020
    vpu_write_register(core, W5_CMD_INSTANCE_INFO,  (instanceIndex&0xffff));
    vpu_write_register(core, W5_VPU_BUSY_STATUS, 1);
    vpu_write_register(core, W5_COMMAND, W5_DESTROY_INSTANCE);

    vpu_write_register(core, W5_VPU_HOST_INT_REQ, 1);

    while (vpu_read_register(core, W5_VPU_BUSY_STATUS)) {
        if (bm_vpu_get_tick_count() > start + timeout) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"Wave5CloseInstanceCommand after BUSY timeout\n");
            ret = 1;
            goto DONE_CMD;
        }
    }

    if (vpu_read_register(core, W5_RET_SUCCESS) == 0) {
        // TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"Wave5CloseInstanceCommand failed REASON=[0x%x]\n", vpu_read_register(core, W5_RET_FAIL_REASON));
#define WAVE5_VPU_STILL_RUNNING 0x00001000
        if (vpu_read_register(core, W5_RET_FAIL_REASON) == WAVE5_VPU_STILL_RUNNING)
            ret = 2;
        else
            ret = 1;
        goto DONE_CMD;
    }
    ret = 0;
DONE_CMD:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"sent Wave5CloseInstanceCommandt 55 ret=%d", ret);
    return ret;
}

int CloseInstanceCommand(struct bm_device_info *bmdi, int core, u32 instanceIndex)
{
    int product_code;
    product_code = vpu_read_register(core, VPU_PRODUCT_CODE_REGISTER);
    if (PRODUCT_CODE_W_SERIES(product_code)) {
        if(WAVE521C_CODE != product_code) {
            u32 i =0;
            u32 interrupt_flag_in_q = 0;
            Wave5VpuDecSetBitstreamFlag(bmdi, core, instanceIndex);
            WdfSpinLockAcquire(bmdi->vpudrvctx.s_kfifo_lock[core][instanceIndex]);
            interrupt_flag_in_q = kfifo_out(&bmdi->vpudrvctx.interrupt_pending_q[core][instanceIndex], (unsigned char*)&i, sizeof(u32));
            WdfSpinLockRelease(bmdi->vpudrvctx.s_kfifo_lock[core][instanceIndex]);

            if (interrupt_flag_in_q > 0) {
                //FlushDecResult(bmdi, core, instanceIndex);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"interrupt flag : %d\n", interrupt_flag_in_q);
            }
            FlushDecResult(bmdi, core, instanceIndex);
            for(i=0; i<62; i++) {
            int ret = Wave5DecClrDispFlag(bmdi, core, instanceIndex, i);
                if(ret != 0)
                    break;
            }
        }
        return Wave5CloseInstanceCommand(bmdi, core, instanceIndex);
    }
    else {
        return CodaCloseInstanceCommand(bmdi, core, instanceIndex);
    }
}
#endif


static int bm_vpu_free_instances(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp)
{
    vpudrv_instanace_list_t *vil, *n;
    vpudrv_instance_pool_t *vip;
    void *vip_base;
    int ret = 0;


    list_for_each_entry_safe(vil, n, &bmdi->vpudrvctx.s_inst_list_head, list, vpudrv_instanace_list_t) {
        if (vil->filp == filp) {
            vip_base = (void *)(bmdi->vpudrvctx.instance_pool[vil->core_idx].base);
            vip = (vpudrv_instance_pool_t *)vip_base;
            if (vip) {
                if (vip->pendingInstIdxPlus1 - 1 == vil->inst_idx)
                    vip->pendingInstIdxPlus1 = 0;
                /* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the corresponding instance. */
                memset(&vip->codecInstPool[vil->inst_idx], 0x00, 4);
            }
            if (ret == 0) {
                ret = (int)vil->core_idx + 1;
            } else if (ret != vil->core_idx + 1) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "please check the release core index: %I64u ....\n", vil->core_idx);
            }
            vip->vpu_instance_num -= 1;
            bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx]--;
            RemoveEntryList(&vil->list);
            BGM_P_FREE(vil);

            if (bmdi->cinfo.chip_id == 0x1682)
                if (vpu_open_inst_count > 0)
                    vpu_open_inst_count--;
        }
    }

    return ret;
}

static int bm_vpu_free_buffers(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp)
{
    int ret = 0;
    vpudrv_buffer_pool_t *pool, *n;
    vpudrv_buffer_t vb;
    vpu_drv_context_t *dev = NULL;

    dev = &bmdi->vpudrvctx;
    list_for_each_entry_safe(pool, n, &bmdi->vpudrvctx.s_vbp_head, list, vpudrv_buffer_pool_t) {
        if (pool->filp == filp) {
            vb = pool->vb;
            if (vb.phys_addr) {
                if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
                    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
                    ret = bmdrv_bgm_heap_free(&bmdi->gmem_info.common_heap[VPU_HEAP_ID_3], vb.phys_addr);
                    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
                }
            }
            RemoveEntryList(&pool->list);
            BGM_P_FREE(pool);
        }
    }
    return 0;
}

int bm_vpu_open(struct bm_device_info *bmdi)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV][+] %s\n", __func__);

    //if (!bmdi->vpudrvctx.open_count)
    // bm_vpu_deassert(&bmdi->vpudrvctx.vpu_rst_ctrl);
    ExAcquireFastMutex(&bmdi->vpudrvctx.s_vpu_mutex);

#if 0
    if (bmdi->vpudrvctx.open_count == 0) {
        int i;
        for (i = 0; i < get_max_num_vpu_core(bmdi); i++)
            bm_vpu_hw_reset(bmdi, i);
    }
#endif
    bmdi->vpudrvctx.open_count++;
    ExReleaseFastMutex(&bmdi->vpudrvctx.s_vpu_mutex);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV][-] %s\n", __func__);

    return 0;
}

static inline int get_core_idx(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp)
{
    int core_idx = -1;
    vpu_core_idx_context *pool, *n;
    vpu_drv_context_t *dev = NULL;
    dev = &bmdi->vpudrvctx;
    list_for_each_entry_safe(pool, n, &bmdi->vpudrvctx.s_core_idx_head, list, vpu_core_idx_context) {
        if (pool->filp == filp) {
            if (core_idx != -1) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "maybe error in get core index, please check.\n");
            }
            core_idx = pool->core_idx;

            RemoveEntryList(&pool->list);
            BGM_P_FREE(pool);
            //break;
        }
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[get_core_idx][-] core_idx=%d\n", core_idx);
    return core_idx;
}

static inline u32 get_inst_idx(vpu_drv_context_t *dev, u32 reg_val)
{
    u32 inst_idx;
    u32 i;

    for (i = 0; i < dev->max_num_instance; i++) {
        if (((reg_val >> i) & 0x01) == 1)
            break;
    }
    inst_idx = i;
    return inst_idx;
}

static u32 bm_get_vpu_inst_idx(vpu_drv_context_t *dev, u32 *reason, u32 empty_inst, u32 done_inst, u32 other_inst)
{
    u32 inst_idx;
    u32 reg_val;
    u32 int_reason;
    int_reason = *reason;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV][+]%s, int_reason=0x%x, empty_inst=0x%x, done_inst=0x%x\n", __func__,  int_reason, empty_inst, done_inst);

    if (int_reason & (1 << INT_WAVE5_BSBUF_EMPTY)) {
#if defined(CHIP_BM1682)
        reg_val = (empty_inst & 0xffff); // at most 16 instances
#else
        reg_val = empty_inst;
#endif
        inst_idx = get_inst_idx(dev, reg_val);
        *reason = (1 << INT_WAVE5_BSBUF_EMPTY);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] %s, W5_RET_BS_EMPTY_INST reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }

    if (int_reason & (1 << INT_WAVE5_INIT_SEQ)) {
#if defined(CHIP_BM1682)
        reg_val = (done_inst & 0xffff); // at most 16 instances
#else
        reg_val = done_inst;
#endif
        inst_idx = get_inst_idx(dev, reg_val);
        *reason  = (1 << INT_WAVE5_INIT_SEQ);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST INIT_SEQ reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }

    if (int_reason & (1 << INT_WAVE5_DEC_PIC)) {
#if defined(CHIP_BM1682)
        reg_val = (done_inst & 0xffff); // at most 16 instances
#else
        reg_val = done_inst;
#endif
        inst_idx = get_inst_idx(dev, reg_val);
        *reason  = (1 << INT_WAVE5_DEC_PIC);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);

#if defined(CHIP_BM1682)
        if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY)) {
            u32 ll_inst_idx;
            reg_val = (done_inst >> 16);
            ll_inst_idx = get_inst_idx(dev, reg_val); // ll_inst_idx < 16
            if (ll_inst_idx == inst_idx)
                *reason = ((1 << INT_WAVE5_DEC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY));
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC and ENC_LOW_LATENCY reg_val=0x%x, inst_idx=%d, ll_inst_idx=%d\n", __func__, reg_val, inst_idx, ll_inst_idx);
        }
#endif
        goto GET_VPU_INST_IDX_HANDLED;
    }

    if (int_reason & (1 << INT_WAVE5_ENC_SET_PARAM)) {
#if defined(CHIP_BM1682)
        reg_val = (done_inst & 0xffff); // at most 16 instances
#else
        reg_val = done_inst;
#endif
        inst_idx = get_inst_idx(dev, reg_val);
        *reason  = (1 << INT_WAVE5_ENC_SET_PARAM);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST ENC_SET_PARAM reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }

#if defined(CHIP_BM1682)
    if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY)) {
        reg_val = (done_inst >> 16); // 16 instances at most
        inst_idx = get_inst_idx(dev, reg_val);
        *reason  = (1 << INT_WAVE5_ENC_LOW_LATENCY);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST ENC_LOW_LATENCY reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
        goto GET_VPU_INST_IDX_HANDLED;
    }
#endif

    //if interrupt is not for empty and dec_pic and init_seq.
    reg_val = (other_inst & 0xFF);
    inst_idx = reg_val; // at most 256 instances
    *reason  = int_reason;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] %s, W5_RET_DONE_INSTANCE_INFO reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);

GET_VPU_INST_IDX_HANDLED:

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV][-]%s, inst_idx=%d. *reason=0x%x\n", __func__, inst_idx, *reason);

    return inst_idx;
}

static void bm_vpu_irq_handler(struct bm_device_info *bmdi)
{
    vpu_drv_context_t *dev = &bmdi->vpudrvctx;

    /* this can be removed. it also work in VPU_WaitInterrupt of API function */
    u32 core;
    u32 product_code;
    u32 intr_reason;
    u32 intr_inst_index;

#ifdef VPU_IRQ_CONTROL
    disable_irq_nosync(bmdi->cinfo.irq_id);
#endif

    intr_inst_index = 0;
    intr_reason = 0;

#ifdef VPU_SUPPORT_ISR
    for (core = 0; core < (u32)get_max_num_vpu_core(bmdi); core++) {
        if (bmdi->vpudrvctx.s_vpu_irq[core] == (u32)bmdi->cinfo.irq_id)
            break;
    }
#endif

    if (core >= (u32)get_max_num_vpu_core(bmdi)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV] : can't find the core index in irq_process\n");
        return;
    }

    if (bmdi->vpudrvctx.s_bit_firmware_info[core].size == 0) {
        /* it means that we didn't get an information the current core from API layer. No core activated.*/
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV] : bmdi->vpudrvctx.s_bit_firmware_info[core].size is zero\n");
        return;
    }

    product_code = vpu_read_register(core, VPU_PRODUCT_CODE_REGISTER);

    if (PRODUCT_CODE_W_SERIES(product_code)) {
        if (vpu_read_register(core, W5_VPU_VPU_INT_STS)) {
            u32 empty_inst;
            u32 done_inst;
            u32 other_inst;

            intr_reason = vpu_read_register(core, W5_VPU_INT_REASON);
            empty_inst = vpu_read_register(core, W5_RET_BS_EMPTY_INST);
            done_inst = vpu_read_register(core, W5_RET_QUEUE_CMD_DONE_INST);
            other_inst = vpu_read_register(core, W5_RET_DONE_INSTANCE_INFO);

            intr_inst_index = bm_get_vpu_inst_idx(dev, &intr_reason, empty_inst, done_inst, other_inst);
            if ((intr_inst_index < bmdi->vpudrvctx.max_num_instance)) {
                if (intr_reason == (1 << INT_WAVE5_BSBUF_EMPTY)) {
                    empty_inst = empty_inst & ~(1 << intr_inst_index);
                    vpu_write_register(core, W5_RET_BS_EMPTY_INST, empty_inst);
                }

                if ((intr_reason == (1 << INT_WAVE5_DEC_PIC)) ||
                        (intr_reason == (1 << INT_WAVE5_INIT_SEQ)) ||
                        (intr_reason == (1 << INT_WAVE5_ENC_SET_PARAM))) {
                    done_inst = done_inst & ~(1 << intr_inst_index);
                    vpu_write_register(core, W5_RET_QUEUE_CMD_DONE_INST, done_inst);
                }

                if ((intr_reason == (1 << INT_WAVE5_ENC_LOW_LATENCY))) {
                    done_inst = (done_inst >> 16);
                    done_inst = done_inst & ~(1 << intr_inst_index);
                    done_inst = (done_inst << 16);
                    vpu_write_register(core, W5_RET_QUEUE_CMD_DONE_INST, done_inst);
                }

                if (!kfifo_is_full(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index])) {
                    if (intr_reason == ((1 << INT_WAVE5_DEC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY))) {
                        u32 ll_intr_reason = (1 << INT_WAVE5_DEC_PIC);
                        kfifo_in(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index], (unsigned char*)&ll_intr_reason, sizeof(u32));
                    } else {
                        kfifo_in(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index], (unsigned char*)&intr_reason, sizeof(u32));
                    }
                } else {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] : kfifo_is_full kfifo_count=%d \n",
                            kfifo_len(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index]));
                }
            } else {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] : intr_inst_index is wrong intr_inst_index=%d \n", intr_inst_index);
            }

            vpu_write_register(core, W5_VPU_INT_REASON_CLEAR, intr_reason);
            vpu_write_register(core, W5_VPU_VINT_CLEAR, 0x1);
        }
    } else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {
        if (vpu_read_register(core, BIT_INT_STS)) {
            intr_reason = vpu_read_register(core, BIT_INT_REASON);
            intr_inst_index = 0; /* in case of coda seriese. treats intr_inst_index is already 0 */
            kfifo_in(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index], (unsigned char*)&intr_reason, sizeof(u32));
            vpu_write_register(core, BIT_INT_CLEAR, 0x1);
        }
    } else {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] bm_vpu_irq_handler errendUnknown product id : %08x\n", product_code);
        return ;
    }

    if ((intr_inst_index < bmdi->vpudrvctx.max_num_instance)) {
        bmdi->vpudrvctx.interrupt_flag[core][intr_inst_index] = 1;
        KeSetEvent(&bmdi->vpudrvctx.interrupt_wait_q[core][intr_inst_index], 0, FALSE);
    }
    return;
}

static int _vpu_syscxt_chkinst(vpu_crst_context_t *p_crst_cxt)
{
    int i = 0;

    for (; i < MAX_NUM_INSTANCE_VPU; i++) {
        int pos = p_crst_cxt->count % 2;
        pos = (p_crst_cxt->reset ? pos : 1 - pos) * 8;
        if (((p_crst_cxt->instcall[i] >> pos) & 3) != 0)
            return -1;
    }

    return 1;
}

static int _vpu_syscxt_printinst(int core_idx, vpu_crst_context_t *p_crst_cxt)
{
    int i = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"+*********************+\n");
    for (; i < MAX_NUM_INSTANCE_VPU; i++) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"core %d, inst %d, map:0x%x.\n",
            core_idx, i, p_crst_cxt->instcall[i]);
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"-*********************-\n\n");
    return 1;
}


static int _vpu_syscxt_chkready(vpu_crst_context_t *p_crst_cxt)
{
    int num = 0, i = 0;
    for(; i < MAX_NUM_INSTANCE_VPU; i++) {
        int pos = (p_crst_cxt->count % 2) * 8;
        if (((p_crst_cxt->instcall[i] >> pos) & 3) != 0)
            num += 1;
    }

    return num;
}




NTSTATUS
MapMemory(_Inout_ vpudrv_buffer_t *vb, _In_ MEMORY_CACHING_TYPE cacheMode) {
    NTSTATUS        status = STATUS_SUCCESS;

    __try {

        try {

            MmBuildMdlForNonPagedPool((PMDL)(ULONG_PTR)vb->phys_addr);
            vb->virt_addr = (ULONG_PTR)MmMapLockedPagesSpecifyCache((PMDL)(ULONG_PTR)vb->phys_addr, UserMode, cacheMode, NULL, FALSE, NormalPagePriority);
            if (!vb->virt_addr) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "%!FUNC! Fail to MmMapLockedPagesSpecifyCache\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                __leave;
            };

            status = STATUS_SUCCESS;
        }
        except(EXCEPTION_EXECUTE_HANDLER) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        };
    } __finally {
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "%!FUNC! Fail to UnMapMemory status = 0x%x\n", (int)status);
            UnMapMemory(vb);
        };
    };
    return status;
};

NTSTATUS UnMapMemory(_Inout_ vpudrv_buffer_t *vb) {
    NTSTATUS        status = STATUS_SUCCESS;

    try {
        if (vb->virt_addr){
            MmUnmapLockedPages((PVOID)(ULONG_PTR)vb->virt_addr, (PMDL)(ULONG_PTR)vb->phys_addr);
            vb->virt_addr = 0;
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, " Fail to UnMapMemory status = 0x%x\n", status);
    };

    return status;
}



long bm_vpu_ioctl(struct bm_device_info *bmdi,
                  _In_ WDFREQUEST        Request,
                  _In_ size_t            OutputBufferLength,
                  _In_ size_t            InputBufferLength,
                  _In_ u32               IoControlCode) {
    int ret = 0;
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    vpu_drv_context_t *dev = &bmdi->vpudrvctx;
    WDFFILEOBJECT      fileObject = WdfRequestGetFileObject(Request);
    switch (IoControlCode) {
        case VDI_IOCTL_SYSCXT_SET_EN:
            {
                int i;
                vpu_crst_context_t *p_crst_cxt;
                vpudrv_syscxt_info_t syscxt_info;

                size_t bufSize;
                PVOID  inDataBuffer;

                NTSTATUS status = WdfRequestRetrieveInputBuffer( Request, sizeof(vpudrv_syscxt_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                RtlCopyMemory(&syscxt_info, inDataBuffer, sizeof(vpudrv_syscxt_info_t));

                ExAcquireFastMutex(&dev->s_vpu_mutex);
                for (i = 0; i < get_max_num_vpu_core(bmdi); i++) {
                    p_crst_cxt = &dev->crst_cxt[i];
                    p_crst_cxt->disable = !syscxt_info.fun_en;
                }
                ExReleaseFastMutex(&dev->s_vpu_mutex);
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }

        case VDI_IOCTL_SYSCXT_SET_STATUS:
            {
                int core_idx;
                vpu_crst_context_t *p_crst_cxt;
                vpudrv_syscxt_info_t syscxt_info;

                size_t bufSize;
                PVOID  inDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer( Request, sizeof(vpudrv_syscxt_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&syscxt_info, inDataBuffer, sizeof(vpudrv_syscxt_info_t));

                ExAcquireFastMutex(&dev->s_vpu_mutex);
                core_idx = syscxt_info.core_idx;
                p_crst_cxt = &dev->crst_cxt[core_idx];
                if ((_vpu_syscxt_chkready(p_crst_cxt) != dev->s_vpu_open_ref_count[core_idx]) && (syscxt_info.core_status)) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] core %d not ready! status: %d\n", core_idx, p_crst_cxt->status);
                    ExReleaseFastMutex(&dev->s_vpu_mutex);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                p_crst_cxt->status = p_crst_cxt->disable ? SYSCXT_STATUS_WORKDING : syscxt_info.core_status;
                ExReleaseFastMutex(&dev->s_vpu_mutex);

                if (p_crst_cxt->status) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] core %d set status:%d. |********************|\n",
                            core_idx, p_crst_cxt->status);
                } else {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] core %d set status:%d.\n",
                            core_idx, p_crst_cxt->status);
                }
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }
            break;
        case VDI_IOCTL_SYSCXT_GET_STATUS:
            {
                vpudrv_syscxt_info_t syscxt_info;
                int core_idx;
                vpu_crst_context_t *p_crst_cxt;

                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer( Request, sizeof(vpudrv_syscxt_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&syscxt_info, inDataBuffer, sizeof(vpudrv_syscxt_info_t));

                ExAcquireFastMutex(&dev->s_vpu_mutex);
                core_idx = syscxt_info.core_idx;
                p_crst_cxt = &dev->crst_cxt[core_idx];
                syscxt_info.core_status = p_crst_cxt->status;
                ExReleaseFastMutex(&dev->s_vpu_mutex);

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpu_video_mem_op_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &syscxt_info, sizeof(vpudrv_syscxt_info_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_syscxt_info_t));
            }
            break;
        case VDI_IOCTL_SYSCXT_CHK_STATUS:
            {
                vpudrv_syscxt_info_t syscxt_info;
                int core_idx;
                int instid, shift, mask;
                vpu_crst_context_t *p_crst_cxt;

                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer( Request, sizeof(vpudrv_syscxt_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&syscxt_info, inDataBuffer, sizeof(vpudrv_syscxt_info_t));

                ExAcquireFastMutex(&dev->s_vpu_mutex);
                core_idx = syscxt_info.core_idx;
                instid = syscxt_info.inst_idx;
                p_crst_cxt = &dev->crst_cxt[core_idx];
                shift = (p_crst_cxt->count % 2) * 8;
                mask = (1u << syscxt_info.pos) << shift;
                if ((SYSCXT_STATUS_WORKDING == p_crst_cxt->status) && (syscxt_info.is_sleep < 0)) {
                    int __num = _vpu_syscxt_chkready(p_crst_cxt);
                    if ((p_crst_cxt->instcall[instid] & mask) == 0) {
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] core %d instant %d mask:0x%x. pos %d, reset: %d, inst num: %d, chk num:%d\n",
                            core_idx, instid, mask, p_crst_cxt->count%2, p_crst_cxt->reset, dev->s_vpu_open_ref_count[core_idx], __num);
                    }

                    p_crst_cxt->instcall[instid] |= mask;
                    syscxt_info.is_all_block = 0;
                    syscxt_info.is_sleep = 0;
                    p_crst_cxt->reset = __num == dev->s_vpu_open_ref_count[core_idx] ? 1 : p_crst_cxt->reset;
                } else {
                    p_crst_cxt->instcall[instid] &= ~mask;
                    syscxt_info.is_sleep = 1;
                    syscxt_info.is_all_block = 0;
                    if (_vpu_syscxt_chkinst(p_crst_cxt) == 1) {
                        syscxt_info.is_all_block = 1;

                        if (p_crst_cxt->reset == 1) {
                            bm_vpu_hw_reset(bmdi, core_idx);
                            p_crst_cxt->reset = 0;
                            p_crst_cxt->count += 1;
                        }
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] core %d instant %d will re-start. pos %d\n", core_idx, instid, 1-(p_crst_cxt->count%2));
                        _vpu_syscxt_printinst(core_idx, p_crst_cxt);
                    }

                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] core: %d, inst: %d, pos:%d, instcall %d, status: %d, sleep:%d, wakeup:%d.\n",
                            core_idx, instid, syscxt_info.pos, p_crst_cxt->instcall[instid],
                            p_crst_cxt->status, syscxt_info.is_sleep, syscxt_info.is_all_block);
                }
                ExReleaseFastMutex(&dev->s_vpu_mutex);
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpudrv_syscxt_info_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &syscxt_info, sizeof(vpudrv_syscxt_info_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_syscxt_info_t));
            }
            break;
        case VDI_IOCTL_GET_COMMON_MEMORY:
            {
                vpudrv_buffer_t vdb = {0};

                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;

                NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_buffer_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&vdb, inDataBuffer, sizeof(vpudrv_buffer_t));
                if (vdb.core_idx >= (u32)get_max_num_vpu_core(bmdi)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                KeWaitForSingleObject(&bmdi->vpudrvctx.s_vpu_sem, Executive, KernelMode, FALSE, NULL);
                if (bmdi->vpudrvctx.s_common_memory[vdb.core_idx].phys_addr != 0) {
                    if (vdb.size != bmdi->vpudrvctx.s_common_memory[vdb.core_idx].size) {
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU, "maybe wrong in the common buffer size: %d and %d\n", vdb.size, bmdi->vpudrvctx.s_common_memory[vdb.core_idx].size);
                        KeReleaseSemaphore(&bmdi->vpudrvctx.s_vpu_sem, 0, 1, FALSE);
                        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                        return -1;
                    }
                    else {
                        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpudrv_buffer_t), &outDataBuffer, &bufSize);
                        if (!NT_SUCCESS(status)) {
                            KeReleaseSemaphore(&bmdi->vpudrvctx.s_vpu_sem, 0, 1, FALSE);
                            WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                            return -1;
                        }
                        RtlCopyMemory(outDataBuffer, &bmdi->vpudrvctx.s_common_memory[vdb.core_idx], sizeof(vpudrv_buffer_t));
                        KeReleaseSemaphore(&bmdi->vpudrvctx.s_vpu_sem, 0, 1, FALSE);
                        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_buffer_t));
                        return  0;
                    }
                }
                KeReleaseSemaphore(&bmdi->vpudrvctx.s_vpu_sem, 0, 1, FALSE);

                struct ion_allocation_data alloc_data;
                alloc_data.heap_id      = VPU_HEAP_ID_3;
                alloc_data.heap_id_mask = 1 << VPU_HEAP_ID_3;
                alloc_data.len          = SIZE_COMMON;
                WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
                ret = bmdrv_gmem_alloc(bmdi, &alloc_data);
                WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
                if (ret == -1) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[%s,%d] can not allocate the common buffer.\n", __func__, __LINE__);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                vdb.phys_addr = alloc_data.paddr;
                vdb.size      = (u32)alloc_data.len;
                KeWaitForSingleObject(&bmdi->vpudrvctx.s_vpu_sem, Executive, KernelMode, FALSE, NULL);
                memcpy(&bmdi->vpudrvctx.s_common_memory[vdb.core_idx], &vdb, sizeof(vpudrv_buffer_t));
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpudrv_buffer_t), &outDataBuffer, &bufSize);
                KeReleaseSemaphore(&bmdi->vpudrvctx.s_vpu_sem, 0, 1, FALSE);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &vdb, sizeof(vpudrv_buffer_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_buffer_t));
            }
            break;
        case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
            {
                struct ion_allocation_data alloc_data;
                struct bm_handle_info *h_info;
                vpudrv_buffer_pool_t *     vbp;
                if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;

                NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_buffer_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                vbp = (vpudrv_buffer_pool_t*)BGM_P_ALLOC(sizeof(vpudrv_buffer_pool_t));
                RtlCopyMemory(&vbp->vb, inDataBuffer, sizeof(vpudrv_buffer_t));

                alloc_data.heap_id = VPU_HEAP_ID_3;
                alloc_data.heap_id_mask = 1 << VPU_HEAP_ID_3;
                alloc_data.len          = vbp->vb.size;

                WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
                ret = bmdrv_gmem_alloc(bmdi, &alloc_data);
                WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
                if (ret) {
                    if (vbp != NULL) {
                        BGM_P_FREE(vbp);
                    }
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                vbp->vb.size      = (u32)alloc_data.len;
                vbp->vb.phys_addr = alloc_data.paddr;
                vbp->filp         = fileObject;

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpudrv_buffer_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    if (vbp != NULL) {
                        BGM_P_FREE(vbp);
                    }
                    if (vbp->vb.phys_addr != 0) {
                        WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
                        bmdrv_bgm_heap_free(&bmdi->gmem_info.common_heap[VPU_HEAP_ID_3], vbp->vb.phys_addr);
                        WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
                    }
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &vbp->vb, sizeof(vpudrv_buffer_t));

                KeWaitForSingleObject(&dev->s_vpu_sem, Executive, KernelMode, FALSE, NULL);
                InsertTailList(&bmdi->vpudrvctx.s_vbp_head, &vbp->list);
                KeReleaseSemaphore(&dev->s_vpu_sem, 0, 1, FALSE);

                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(struct vpudrv_buffer_t));
            }
            break;
        case VDI_IOCTL_FREE_PHYSICALMEMORY: 
            {
                size_t                     bufSize;
                PVOID                      inDataBuffer;
                vpudrv_buffer_t            vdb;
                vpudrv_buffer_pool_t *     pool, *n;

                NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_buffer_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                RtlCopyMemory(&vdb, inDataBuffer, sizeof(vpudrv_buffer_t));
                KeWaitForSingleObject(&dev->s_vpu_sem, Executive, KernelMode, FALSE, NULL);
                list_for_each_entry_safe(pool, n, &bmdi->vpudrvctx.s_vbp_head, list, vpudrv_buffer_pool_t) {
                    if ((fileObject == pool->filp) && (vdb.phys_addr == pool->vb.phys_addr)) {
                        RemoveEntryList(&pool->list);
                        BGM_P_FREE(pool);
                        break;
                    }
                }
                KeReleaseSemaphore(&dev->s_vpu_sem, 0, 1, FALSE);

                WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
                ret = bmdrv_bgm_heap_free(&bmdi->gmem_info.common_heap[VPU_HEAP_ID_3], vdb.phys_addr);
                WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
                if (ret) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }
            break;

        case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
            {
                size_t bufSize;
                PVOID  outDataBuffer;
                if (bmdi->vpudrvctx.s_video_memory.base == 0) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpudrv_buffer_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer,  &bmdi->vpudrvctx.s_video_memory, sizeof(vpudrv_buffer_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_buffer_t));
            }
            break;

        case VDI_IOCTL_GET_FREE_MEM_SIZE:
            {
                u64    avail_size;
                size_t bufSize;
                PVOID  DataBuffer;

                NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(u64), &DataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
                avail_size = bmdrv_gmem_vpu_avail_size(bmdi);
                WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

                RtlCopyMemory(DataBuffer, &avail_size, sizeof(u64));
                WdfRequestCompleteWithInformation(Request, status, sizeof(u64));
            }
            break;

        case VDI_IOCTL_WAIT_INTERRUPT:
            {
#ifdef SUPPORT_TIMEOUT_RESOLUTION
                ktime_t kt;
#endif
                vpudrv_intr_info_t info;
                u32 intr_inst_index;
                u32 intr_reason_in_q;
                u32 interrupt_flag_in_q;
                u32 core_idx;


                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer( Request, sizeof(vpudrv_intr_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&info, inDataBuffer, sizeof(vpudrv_intr_info_t));

                intr_inst_index = info.intr_inst_index;
                core_idx = info.core_idx;

                if (bmdi->cinfo.chip_id == 0x1682)
                    core_idx %= bmdi->vpudrvctx.max_num_vpu_core;

                intr_reason_in_q = 0;
                WdfSpinLockAcquire(bmdi->vpudrvctx.s_kfifo_lock[core_idx][intr_inst_index]);
                interrupt_flag_in_q = kfifo_out(&bmdi->vpudrvctx.interrupt_pending_q[core_idx][intr_inst_index], (unsigned char*)&intr_reason_in_q, sizeof(u32));
                if (interrupt_flag_in_q > 0)
                    KeResetEvent(&bmdi->vpudrvctx.interrupt_wait_q[core_idx][intr_inst_index]);
                WdfSpinLockRelease(bmdi->vpudrvctx.s_kfifo_lock[core_idx][intr_inst_index]);
                if (interrupt_flag_in_q > 0) {
                    dev->interrupt_reason[core_idx][intr_inst_index] = intr_reason_in_q;
                    goto INTERRUPT_REMAIN_IN_QUEUE;
                }

                LARGE_INTEGER li;
                li.QuadPart = WDF_REL_TIMEOUT_IN_MS(info.timeout);
                status = KeWaitForSingleObject(&bmdi->vpudrvctx.interrupt_wait_q[core_idx][intr_inst_index], Executive, KernelMode, FALSE, &li);
                if (status != STATUS_SUCCESS) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT timeout = %d\n", info.timeout);
                    return -1;
                }

                intr_reason_in_q = 0;
                WdfSpinLockAcquire(bmdi->vpudrvctx.s_kfifo_lock[core_idx][intr_inst_index]);
                interrupt_flag_in_q = kfifo_out(&bmdi->vpudrvctx.interrupt_pending_q[core_idx][intr_inst_index], (unsigned char*)&intr_reason_in_q, sizeof(u32));
                WdfSpinLockRelease(bmdi->vpudrvctx.s_kfifo_lock[core_idx][intr_inst_index]);
                if (interrupt_flag_in_q > 0) {
                    dev->interrupt_reason[core_idx][intr_inst_index] = intr_reason_in_q;
                } else {
                    dev->interrupt_reason[core_idx][intr_inst_index] = 0;
                }


INTERRUPT_REMAIN_IN_QUEUE:

                info.intr_reason = (int)dev->interrupt_reason[core_idx][intr_inst_index];
                bmdi->vpudrvctx.interrupt_flag[core_idx][intr_inst_index] = 0;
                dev->interrupt_reason[core_idx][intr_inst_index] = 0;

#ifdef VPU_IRQ_CONTROL
                enable_irq(bmdi->vpudrvctx.s_vpu_irq[core_idx]);
#endif

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpudrv_intr_info_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &info, sizeof(vpudrv_intr_info_t));

                if (ret != 0) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS,  sizeof(vpudrv_intr_info_t));
            }
            break;

        case VDI_IOCTL_SET_CLOCK_GATE:
            {
                u32 clkgate;
                size_t bufSize;
                PVOID  inDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer( Request, sizeof(u32), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&clkgate, inDataBuffer, sizeof(u32));
#ifdef VPU_SUPPORT_CLOCK_CONTROL
                bm_vpu_clk_enable(clkgate);
#endif
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }
            break;

        case VDI_IOCTL_GET_INSTANCE_POOL:
            {
                NTSTATUS        status = STATUS_SUCCESS;
                 
                int              core_idx = -1;
                int              size     = 0;
                vpudrv_buffer_t  vdb;
                size_t           bufSize;
                PVOID            inDataBuffer;
                PVOID            outDataBuffer;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_buffer_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&vdb, inDataBuffer, sizeof(vpudrv_buffer_t));

                ExAcquireFastMutex(&dev->s_vpu_mutex);
                vpu_core_idx_context *pool;
                int check_flag = 0;

                core_idx = vdb.core_idx;
                size     = vdb.size;
                if (core_idx >= get_max_num_vpu_core(bmdi)) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"wrong core_idx: %d\n", core_idx);
                    ExReleaseFastMutex(&dev->s_vpu_mutex);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                list_for_each_entry(pool, &bmdi->vpudrvctx.s_core_idx_head, list, vpu_core_idx_context) {
                    if (pool->filp == fileObject && pool->core_idx == core_idx) {
                        check_flag = 1;
                        break;
                    }
                }
                if (check_flag == 0) {
                    vpu_core_idx_context* p_core_idx_context = BGM_P_ALLOC(sizeof(vpu_core_idx_context));
                    RtlZeroMemory(p_core_idx_context, sizeof(vpu_core_idx_context));
                    if (p_core_idx_context == NULL) {
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"no mem in vzalloc.\n");
                    }
                    else {
                        p_core_idx_context->core_idx = core_idx;
                        p_core_idx_context->filp     = fileObject;
                        InsertTailList(&bmdi->vpudrvctx.s_core_idx_head, &p_core_idx_context->list);
                    }
                }

                if (bmdi->vpudrvctx.instance_pool[core_idx].base == 0) {
                    u64 p_base = 0;
                    status = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPoolNx, 0, size, (WDFMEMORY*)&bmdi->vpudrvctx.instance_pool_mem_obj[core_idx], (PVOID*)&p_base);

                    vdb.base = p_base;
                    if (!NT_SUCCESS(status) || !vdb.base) {
                        ExReleaseFastMutex(&dev->s_vpu_mutex);
                        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                        return -1;
                    }

                    RtlZeroMemory((PVOID)(ULONG_PTR)vdb.base, vdb.size);
                    vdb.phys_addr = (ULONG_PTR)(PMDL)IoAllocateMdl((PVOID)(ULONG_PTR)vdb.base, vdb.size, FALSE, FALSE, NULL);
                    if (!vdb.phys_addr) {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "%!FUNC! Fail to IoAllocateMdl vb->size = %d\n", (int)vdb.size);
                        ExReleaseFastMutex(&dev->s_vpu_mutex);
                        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                        return -1;
                    }

                    bmdi->vpudrvctx.instance_pool[core_idx].size      = vdb.size;
                    bmdi->vpudrvctx.instance_pool[core_idx].base      = vdb.base;
                    bmdi->vpudrvctx.instance_pool[core_idx].phys_addr = vdb.phys_addr;
                    bmdi->vpudrvctx.instance_pool[core_idx].virt_addr = vdb.virt_addr;
                    bmdi->vpudrvctx.instance_pool[core_idx].core_idx = core_idx;
                    bm_vpu_clk_prepare(bmdi, core_idx);
                    bm_vpu_reset_core(bmdi, core_idx, 1);
                }

                vdb = bmdi->vpudrvctx.instance_pool[core_idx];
                ExReleaseFastMutex(&dev->s_vpu_mutex);

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpudrv_buffer_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &vdb, sizeof(vpudrv_buffer_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_buffer_t));
            }
            break;

        case VDI_IOCTL_MMAP_INSTANCEPOOL:
            {
                NTSTATUS        status = STATUS_SUCCESS;
            
                vpudrv_buffer_t  vdb;
                size_t           bufSize;
                PVOID            inDataBuffer;
                PVOID            outDataBuffer;
            
                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_buffer_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&vdb, inDataBuffer, sizeof(vpudrv_buffer_t));

                vpu_core_idx_mmap* pool;
                int check_flag = 0;
                list_for_each_entry(pool, &bmdi->vpudrvctx.s_mmap_list, list, vpu_core_idx_mmap) {
                    if (pool->filp == fileObject && (u32)(pool->core_idx) == vdb.core_idx) {
                        check_flag = 1;
                        break;
                    }
                }
                if (check_flag == 0) {
                    status = MapMemory(&vdb, MmCached);
                    if (!NT_SUCCESS(status)) {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "%!FUNC! Fail to map instance-pool to user status = 0x%x\n", status);
                        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                        return -1;
                    }
                    vpu_core_idx_mmap* p_core_idx_mmap = BGM_P_ALLOC(sizeof(vpu_core_idx_mmap));
                    RtlZeroMemory(p_core_idx_mmap, sizeof(vpu_core_idx_mmap));
                    if (p_core_idx_mmap == NULL) {
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU, "no mem in vzalloc.\n");
                    }
                    else {
                        p_core_idx_mmap->core_idx = vdb.core_idx;
                        p_core_idx_mmap->filp = fileObject;
                        p_core_idx_mmap->vdb = &vdb;
                        p_core_idx_mmap->phys_addr = vdb.phys_addr;
                        p_core_idx_mmap->virt_addr = vdb.virt_addr;
                        InsertTailList(&bmdi->vpudrvctx.s_mmap_list, &p_core_idx_mmap->list);
                    }
                }
            
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpudrv_buffer_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &vdb, sizeof(vpudrv_buffer_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_buffer_t));
            }
            break;

        case VDI_IOCTL_MUNMAP_INSTANCEPOOL:
            {
                NTSTATUS        status = STATUS_SUCCESS;
            
                vpudrv_buffer_t  vdb;
                size_t           bufSize;
                PVOID            inDataBuffer;
                PVOID            outDataBuffer;
            
                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_buffer_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&vdb, inDataBuffer, sizeof(vpudrv_buffer_t));
            
                status = UnMapMemory(&vdb); 
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "%!FUNC! Fail to map instance-pool to user status = 0x%x\n", status);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                vpu_core_idx_mmap* pmmap, * n;
                list_for_each_entry_safe(pmmap, n, &bmdi->vpudrvctx.s_mmap_list, list, vpu_core_idx_mmap) {
                    if ((pmmap->phys_addr == vdb.phys_addr) && (pmmap->virt_addr == vdb.virt_addr)) {
                        RemoveEntryList(&pmmap->list);
                        BGM_P_FREE(pmmap);
                        break;
                    }
                }

            
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpudrv_buffer_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &vdb, sizeof(vpudrv_buffer_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_buffer_t));
            }
            break;
        case VDI_IOCTL_OPEN_INSTANCE:
            {
                vpudrv_inst_info_t inst_info;
                vpudrv_instanace_list_t *vil;
                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct vpudrv_inst_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&inst_info, inDataBuffer, sizeof(struct vpudrv_inst_info_t));

                vil = (vpudrv_instanace_list_t*)BGM_P_ALLOC(sizeof(vpudrv_instanace_list_t));
                if (!vil)
                    return -1;
                vil->inst_idx = inst_info.inst_idx;
                vil->core_idx = inst_info.core_idx;
                vil->filp     = fileObject;
                ExAcquireFastMutex(&dev->s_vpu_mutex);
                InsertTailList(&bmdi->vpudrvctx.s_inst_list_head, &vil->list);
                bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx]++; /* flag just for that vpu is in opened or closed */
                inst_info.inst_open_count = bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx];
                kfifo_reset(&bmdi->vpudrvctx.interrupt_pending_q[inst_info.core_idx][inst_info.inst_idx]);
                bmdi->vpudrvctx.interrupt_flag[inst_info.core_idx][inst_info.inst_idx] = 0;

                ExReleaseFastMutex(&dev->s_vpu_mutex);

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpudrv_inst_info_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &inst_info, sizeof(vpudrv_inst_info_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_inst_info_t));
            }
            break;

        case VDI_IOCTL_CLOSE_INSTANCE:
            {
                vpudrv_inst_info_t inst_info;
                vpudrv_instanace_list_t *vil, *n;

                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct vpudrv_inst_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&inst_info, inDataBuffer, sizeof(struct vpudrv_inst_info_t));

                ExAcquireFastMutex(&dev->s_vpu_mutex);
                list_for_each_entry_safe(vil, n, &bmdi->vpudrvctx.s_inst_list_head, list, vpudrv_instanace_list_t) {
                    if (vil->inst_idx == inst_info.inst_idx && vil->core_idx == inst_info.core_idx) {
                        RemoveEntryList(&vil->list);
                        vpu_open_inst_count--;
                        bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx]--;
                        inst_info.inst_open_count = bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx];
                        BGM_P_FREE(vil);
                        break;
                    }
                }
                bmdi->vpudrvctx.crst_cxt[inst_info.core_idx].instcall[inst_info.inst_idx] = 0;
                kfifo_reset(&bmdi->vpudrvctx.interrupt_pending_q[inst_info.core_idx][inst_info.inst_idx]);
                bmdi->vpudrvctx.interrupt_flag[inst_info.core_idx][inst_info.inst_idx] = 0;

                ExReleaseFastMutex(&dev->s_vpu_mutex);

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpudrv_inst_info_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &inst_info, sizeof(vpudrv_inst_info_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_inst_info_t));
            }
            break;

        case VDI_IOCTL_GET_INSTANCE_NUM:
            {
                vpudrv_inst_info_t inst_info;
                vpudrv_instanace_list_t *vil;

                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_inst_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&inst_info, inDataBuffer, sizeof(struct vpudrv_inst_info_t));

                ExAcquireFastMutex(&dev->s_vpu_mutex);
                inst_info.inst_open_count = 0;
                list_for_each_entry(vil, &bmdi->vpudrvctx.s_inst_list_head, list, vpudrv_instanace_list_t) {
                    if (vil->core_idx == inst_info.core_idx)
                        inst_info.inst_open_count++;
                }
                ExReleaseFastMutex(&dev->s_vpu_mutex);


                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpudrv_inst_info_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, status, 0);
                    return -1;
                }else{
                    RtlCopyMemory(outDataBuffer, &inst_info, sizeof(vpudrv_inst_info_t));
                    WdfRequestCompleteWithInformation(Request, status, sizeof(vpudrv_inst_info_t));
                }
            }
            break;

        case VDI_IOCTL_RESET:
            {
                int core_idx;
                size_t bufSize;
                PVOID  inDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_inst_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, status, 0);
                    return -1;
                }
                RtlCopyMemory(&core_idx, inDataBuffer, sizeof(int));
                bm_vpu_hw_reset(bmdi, core_idx);
                WdfRequestCompleteWithInformation(Request, status, sizeof(vpudrv_inst_info_t));
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV][-]VDI_IOCTL_RESET end\n");
            }
            break;

        case VDI_IOCTL_GET_REGISTER_INFO:
            {
                u32 core_idx;
                vpudrv_buffer_t info;
                size_t bufSize;
                PVOID  inDataBuffer;
                PVOID  outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct vpudrv_buffer_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&info, inDataBuffer, sizeof(struct vpudrv_buffer_t));
                core_idx = info.size;

                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct vpudrv_buffer_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &bmdi->vpudrvctx.s_vpu_register[core_idx], sizeof(vpudrv_buffer_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_buffer_t));

            }
            break;

        case VDI_IOCTL_GET_FIRMWARE_STATUS:
            {
                u32 core_idx;
                size_t bufSize;
                PVOID  inDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(u32), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&core_idx, inDataBuffer, sizeof(u32));

                if (core_idx >= (u32)get_max_num_vpu_core(bmdi))
                    return -1;
                if (bmdi->vpudrvctx.s_bit_firmware_info[core_idx].size == 0)
                    ret = 100;
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }
            break;

        case VDI_IOCTL_WRITE_VMEM:
            {
                vpu_video_mem_op_t mem_op_write;
                size_t             bufSize;
                PVOID              inDataBuffer;
                NTSTATUS status;
                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpu_video_mem_op_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                RtlCopyMemory(&mem_op_write, inDataBuffer, sizeof(vpu_video_mem_op_t));

                if ((mem_op_write.src == 0) || (mem_op_write.dst == 0)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                ret = bmdev_memcpy_s2d(bmdi, fileObject, mem_op_write.dst, (void *)mem_op_write.src, mem_op_write.size, TRUE, KERNEL_NOT_USE_IOMMU);
                if (ret != 0) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }
            break;

        case VDI_IOCTL_READ_VMEM:
            {
                vpu_video_mem_op_t mem_op_read;
                size_t             bufSize;
                PVOID              inDataBuffer;
                NTSTATUS           status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpu_video_mem_op_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&mem_op_read, inDataBuffer, sizeof(vpu_video_mem_op_t));
                if ((mem_op_read.src == 0) || (mem_op_read.dst == 0)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                ret = bmdev_memcpy_d2s(bmdi, fileObject, (void *)mem_op_read.dst, mem_op_read.src, mem_op_read.size, TRUE, KERNEL_NOT_USE_IOMMU);
                if (ret != 0) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }
            break;
        case VDI_IOCTL_READ_HPI_REGISTER:
            {
                vpudrv_regrw_info_t vpu_reg_info;

                size_t   bufSize;
                PVOID    inDataBuffer;
                PVOID    outDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_regrw_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&vpu_reg_info, inDataBuffer, sizeof(struct vpudrv_regrw_info_t));

                if (vpu_reg_info.size > 0 && vpu_reg_info.size <= 16 && !(vpu_reg_info.size & 0x3)) {
                    u32 i;
                    for (i = 0; i < vpu_reg_info.size; i += sizeof(int)) {
                        u32 value;
                        value = bm_read32(bmdi, (u32)vpu_reg_info.reg_base + i);
                        vpu_reg_info.value[i] = value & vpu_reg_info.mask[i];
                    }

                    status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpudrv_regrw_info_t), &outDataBuffer, &bufSize);
                    if (!NT_SUCCESS(status)) {
                        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                        return -1;
                    }
                    RtlCopyMemory(outDataBuffer, &vpu_reg_info, sizeof(vpudrv_regrw_info_t));
                    ret = sizeof(vpudrv_regrw_info_t);
                } else {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV] vpu_read invalid size %d\n", vpu_reg_info.size);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }

                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_regrw_info_t));
                ret = 0;
            }
            break;
        case VDI_IOCTL_WRITE_HPI_FIRMWARE_REGISTER:
            {
                vpu_bit_firmware_info_t bit_firmware_info;

                size_t   bufSize;
                PVOID    inDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpu_bit_firmware_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&bit_firmware_info, inDataBuffer, sizeof(vpu_bit_firmware_info_t));

                if (bit_firmware_info.size == sizeof(vpu_bit_firmware_info_t)) {
                    if (bit_firmware_info.core_idx > (u32)get_max_num_vpu_core(bmdi)) {
                        return -1;
                    } else{
                        ExAcquireFastMutex(&dev->s_vpu_mutex);
                        memcpy((void *)&bmdi->vpudrvctx.s_bit_firmware_info[bit_firmware_info.core_idx],
                                &bit_firmware_info, sizeof(vpu_bit_firmware_info_t));
                        ExReleaseFastMutex(&dev->s_vpu_mutex);
                        ret = sizeof(vpu_bit_firmware_info_t);
                    }
                }

                if (ret == -1) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpu_bit_firmware_info_t));
                ret = 0;
            }
            break;
        case VDI_IOCTL_WRITE_HPI_REGRW_REGISTER:
            {
                vpudrv_regrw_info_t vpu_reg_info;
                size_t   bufSize;
                PVOID    inDataBuffer;
                NTSTATUS status;

                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpudrv_regrw_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&vpu_reg_info, inDataBuffer, sizeof(vpudrv_regrw_info_t));

                if (vpu_reg_info.size > 0 && vpu_reg_info.size <= 16 && !(vpu_reg_info.size & 0x3)) {
                    u32 i;
                    u32 value;
                    for (i = 0; i < vpu_reg_info.size; i += sizeof(int)) {
                        value = bm_read32(bmdi, (u32)vpu_reg_info.reg_base + i);
                        value = value & ~vpu_reg_info.mask[i];
                        value = value | (vpu_reg_info.value[i] & vpu_reg_info.mask[i]);
                        bm_write32(bmdi, (u32)vpu_reg_info.reg_base + i, value);
                    }
                    ret = sizeof(vpudrv_regrw_info_t);
                } else {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV] vpu_write invalid size %d\n", vpu_reg_info.size);
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                if (ret == -1) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpudrv_regrw_info_t));
                ret = 0;
            }
            break;
        case VDI_IOCTL_READ_REGISTER:
            {
                vpu_register_info_t reg;
                size_t              bufSize;
                PVOID    inDataBuffer;
                PVOID    outDataBuffer;
                NTSTATUS        status;
                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpu_register_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&reg, inDataBuffer, sizeof(vpu_register_info_t));
                reg.data = bm_read32(bmdi, bmdi->vpudrvctx.s_vpu_reg_phy_base[reg.core_idx] + reg.addr_offset);
                status = WdfRequestRetrieveOutputBuffer(Request, sizeof(vpu_register_info_t), &outDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(outDataBuffer, &reg, sizeof(vpu_register_info_t));
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(vpu_register_info_t));
            }
            break;
        case VDI_IOCTL_WRIET_REGISTER:
            {
                vpu_register_info_t reg;
                size_t   bufSize;
                PVOID    inDataBuffer;
                NTSTATUS        status;
                status = WdfRequestRetrieveInputBuffer(Request, sizeof(vpu_register_info_t), &inDataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                RtlCopyMemory(&reg, inDataBuffer, sizeof(vpu_register_info_t));
                bm_write32(bmdi, bmdi->vpudrvctx.s_vpu_reg_phy_base[reg.core_idx] + reg.addr_offset, reg.data);
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
            }
            break;
        case VDI_IOCTL_GET_CHIP_ID:
            {
                u64    tmp_chip_id = 0;
                size_t bufSize;
                PVOID  DataBuffer;
                NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(u64), &DataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                tmp_chip_id = bmdi->cinfo.chip_id;
                RtlCopyMemory(DataBuffer, &tmp_chip_id, sizeof(u64));
                WdfRequestCompleteWithInformation(Request, status, sizeof(u64));
            }
            break;
        case VDI_IOCTL_GET_MAX_CORE_NUM:
            {
                u64    tmp_max_core_num = 0;
                size_t bufSize;
                PVOID  DataBuffer;
                NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(u64), &DataBuffer, &bufSize);
                if (!NT_SUCCESS(status)) {
                    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
                    return -1;
                }
                tmp_max_core_num = get_max_num_vpu_core(bmdi);
                RtlCopyMemory(DataBuffer, &tmp_max_core_num, sizeof(u64));
                WdfRequestCompleteWithInformation(Request, status, sizeof(u64));
            }
            break;
        default:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV] No such IOCTL, IoControlCode is %d\n", IoControlCode);
            WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
            break;
    }

    return ret;
}

static u64 get_exception_instance_info(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp)
{
    vpudrv_instanace_list_t *vil;
    u64 core_idx = (u64)-1;
    u64 ret = 0;
    list_for_each_entry(vil, &bmdi->vpudrvctx.s_inst_list_head, list, vpudrv_instanace_list_t)
    {
        if (vil->filp == filp) {
            if (core_idx == (u64)-1) {
                core_idx = vil->core_idx;
            } else if(core_idx != vil->core_idx) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "please check the release core index: %lld ....\n", vil->core_idx);
            }
            if (vil->inst_idx > 31)
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "please check the release core index too big. inst_idx : %lld\n", vil->inst_idx);
            ret |= ((u64)1UL<<vil->inst_idx);
        }
    }

    if (core_idx != (u64)-1) {
        ret |= (core_idx << 32);
    }

    return ret;
}

static int close_vpu_instance(u64 flags, struct bm_device_info *bmdi)
{
    int core_idx, i;
    int vpu_create_inst_flag = 0;
    //struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
    int release_lock_flag = 0;
    if (flags == 0)
        return release_lock_flag;

    core_idx = (flags >> 32) & 0xff;
    if (core_idx >= get_max_num_vpu_core(bmdi)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "[VPUDRV] please check the core_idx in close wave.(%d)\n", core_idx);
        return release_lock_flag;
    }

    if (bmdi->vpudrvctx.crst_cxt[core_idx].status != SYSCXT_STATUS_WORKDING) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "[VPUDRV] core %d is exception\n", core_idx);
        return release_lock_flag;
    }

    for (i = 0; i < MAX_NUM_INSTANCE_VPU; i++) {
        if ((flags & ((u64)1UL<<i)) != 0) {
            //close the vpu instance
            int count = 0;

            while (1) {
                int ret;
                get_lock(bmdi, core_idx, CORE_LOCK);
                release_lock_flag = 1;
                //whether the instance has been created
                vpu_create_inst_flag = get_vpu_create_inst_flag(bmdi, core_idx);

                if ((vpu_create_inst_flag & (1 << i)) != 0) {
                    ret = CloseInstanceCommand(bmdi, core_idx, i);
                    release_lock(bmdi, core_idx, CORE_LOCK);
                } else {
                    release_lock(bmdi, core_idx, CORE_LOCK);
                    break;
                }
                if (ret != 0) {
                    bm_mdelay(1);    // delay more to give idle time to OS;
                    count += 1;
                    if (count > 5) {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "can not stop instances core %d inst %d\n", (int)core_idx, i);
                        release_vpu_create_inst_flag(bmdi, core_idx, i);
                        return release_lock_flag;
                    }
                    continue; // means there is command which should be flush.
                } else {
                    release_vpu_create_inst_flag(bmdi, core_idx, i);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"stop instances core %d inst %d success\n", (int)core_idx, i);
                }
                break;
            }
        }
    }
    return release_lock_flag;
}

int bm_vpu_release(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp) {
    int ret = 0;
    u32 i, j;
    u32 open_count;
    u64 except_info = 0;
    vpu_drv_context_t *dev = &bmdi->vpudrvctx;
    int core_idx, release_lock_flag;

    ExAcquireFastMutex(&dev->s_vpu_mutex);
    core_idx = get_core_idx(bmdi,filp);
    except_info = get_exception_instance_info(bmdi, filp);
    ExReleaseFastMutex(&dev->s_vpu_mutex);
    release_lock_flag = close_vpu_instance(except_info, bmdi);
    if (core_idx >= 0 && core_idx < get_max_num_vpu_core(bmdi)) {
        get_lock(bmdi, core_idx, CORE_LOCK);
    }

    /* found and free the not handled buffer by user applications */
    KeWaitForSingleObject(&dev->s_vpu_sem, Executive, KernelMode, FALSE, NULL);

    //down(&dev->s_vpu_sem);
    bm_vpu_free_buffers(bmdi, filp);
    //up(&dev->s_vpu_sem);
    KeReleaseSemaphore(&dev->s_vpu_sem, 0, 1, FALSE);

    ExAcquireFastMutex(&dev->s_vpu_mutex);
    /* found and free the not closed instance by user applications */
    ret = bm_vpu_free_instances(bmdi, filp);
    ExReleaseFastMutex(&dev->s_vpu_mutex);
    if (core_idx >= 0 && core_idx < get_max_num_vpu_core(bmdi)) {
        release_lock(bmdi, core_idx, CORE_LOCK);
        if (get_lock(bmdi, core_idx, CORE_DISPLAY_LOCK) == 1)
            release_lock(bmdi, core_idx, CORE_DISPLAY_LOCK);
    }

    if (vpu_polling_create == 1 && vpu_open_inst_count == 0) {
        //destory_irq_poll_thread();
        vpu_polling_create = 0;
    }

    ExAcquireFastMutex(&dev->s_vpu_mutex);
    bmdi->vpudrvctx.open_count--;
    open_count = bmdi->vpudrvctx.open_count;

    if (open_count == 0) {
        for (j = 0; j < (u32)get_max_num_vpu_core(bmdi); j++) {
            for (i = 0; i < bmdi->vpudrvctx.max_num_instance; i++) {
                kfifo_reset(&bmdi->vpudrvctx.interrupt_pending_q[j][i]);
                bmdi->vpudrvctx.interrupt_flag[j][i] = 0;
            }
        }

        for (i = 0; i < (u32)get_max_num_vpu_core(bmdi); i++) {
            if (bmdi->vpudrvctx.instance_pool[i].base) {
                UnMapMemory(&bmdi->vpudrvctx.instance_pool[i]);
                if (bmdi->vpudrvctx.instance_pool[i].phys_addr) {
                    IoFreeMdl((PMDL)(ULONG_PTR)bmdi->vpudrvctx.instance_pool[i].phys_addr);
                }
                if (bmdi->vpudrvctx.instance_pool_mem_obj[i] != 0){
                    WdfObjectDelete(bmdi->vpudrvctx.instance_pool_mem_obj[i]);
                    bmdi->vpudrvctx.instance_pool_mem_obj[i] = 0;
                }
                bmdi->vpudrvctx.instance_pool[i].base = 0;
                bmdi->vpudrvctx.instance_pool[i].phys_addr = 0;
                bm_vpu_reset_core(bmdi, i, 0);
                bm_vpu_clk_unprepare(bmdi, i);
            }
            /*
            if (bmdi->vpudrvctx.s_common_memory[i].base) {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] free common memory\n");
                if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
                    bm_vpu_free_dma_buffer(&bmdi->vpudrvctx.s_common_memory[i], &bmdi->vpudrvctx.s_vmem);
                } else {
                    if (bmdi->vpudrvctx.s_common_memory[i].core_idx < 2)
                        bm_vpu_free_dma_buffer(&bmdi->vpudrvctx.s_common_memory[i], &bmdi->vpudrvctx.s_vmemboda);
                    else
                        bm_vpu_free_dma_buffer(&bmdi->vpudrvctx.s_common_memory[i], &bmdi->vpudrvctx.s_vmemboda);
                }
                bmdi->vpudrvctx.s_common_memory[i].base = 0;
            }
            */
        }

    }
    memset(&dev->crst_cxt[0], 0, sizeof(vpu_crst_context_t) * get_max_num_vpu_core(bmdi));

    if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
        if (ret > 0 && bmdi->vpudrvctx.s_vpu_open_ref_count[ret-1] == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_INFO "exception will reset the vpu core: %d\n", ret - 1);
            bmdi->vpudrvctx.s_bit_firmware_info[ret-1].size = 0;
        }
    }
    ExReleaseFastMutex(&dev->s_vpu_mutex);

    return 0;
}

int bm_vpu_release_byfilp(struct bm_device_info* bmdi, _In_ WDFFILEOBJECT filp)
{

    vpu_core_idx_mmap* pmmap, * n;
    list_for_each_entry_safe(pmmap, n, &bmdi->vpudrvctx.s_mmap_list, list, vpu_core_idx_mmap) {
        if (pmmap->filp == filp) {
            RemoveEntryList(&pmmap->list);
            BGM_P_FREE(pmmap);
            vpudrv_buffer_t  vdb;
            vdb.phys_addr = pmmap->phys_addr;
            vdb.virt_addr = pmmap->virt_addr;
            UnMapMemory(&vdb);
        }
    }

    return 0;
}

static int bm_vpu_address_init(struct bm_device_info *bmdi, int op)
{
    u32 offset = 0, i;
    u64 bar_paddr = 0;
    u32 bar_vaddr = 0;
    struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

    for (i = 0; i < (u32)get_max_num_vpu_core(bmdi); i++) {
        if (op) {
            bm_get_bar_offset(pbar_info, bmdi->vpudrvctx.s_vpu_reg_phy_base[i], (void*)&bar_vaddr, &offset);
            bm_get_bar_base(pbar_info, bmdi->vpudrvctx.s_vpu_reg_phy_base[i], &bar_paddr);

            bmdi->vpudrvctx.s_vpu_register[i].phys_addr = (u64)(bar_paddr + offset);
            bmdi->vpudrvctx.s_vpu_register[i].virt_addr = (u64)(bar_vaddr + offset);
            bmdi->vpudrvctx.s_vpu_register[i].size = VPU_REG_SIZE;
        } else {
            bmdi->vpudrvctx.s_vpu_register[i].virt_addr = 0;
            bmdi->vpudrvctx.s_vpu_register[i].phys_addr = 0;
            bmdi->vpudrvctx.s_vpu_register[i].size = 0;
        }
    }

    return 0;
}

static void bm_vpu_inst_pool_init(struct bm_device_info *bmdi, int op)
{
    u32 i;

    for (i = 0; i <(u32)get_max_num_vpu_core(bmdi); i++) {
        if (op) {
            bmdi->vpudrvctx.instance_pool[i].base = 0;
        } else {
            if (bmdi->vpudrvctx.instance_pool[i].base) {
                UnMapMemory(&bmdi->vpudrvctx.instance_pool[i]);
                if (bmdi->vpudrvctx.instance_pool[i].phys_addr) {
                    IoFreeMdl((PMDL)(ULONG_PTR)bmdi->vpudrvctx.instance_pool[i].phys_addr);
                }
                if (bmdi->vpudrvctx.instance_pool_mem_obj[i] != 0){
                    WdfObjectDelete(bmdi->vpudrvctx.instance_pool_mem_obj[i]);
					bmdi->vpudrvctx.instance_pool_mem_obj[i] = 0;
                }
                bmdi->vpudrvctx.instance_pool[i].base = 0;
            }
        }
    }
}

static void bm_vpu_int_wq_init(struct bm_device_info *bmdi)
{
    u32 i, j;

    for (j = 0; j < (u32)get_max_num_vpu_core(bmdi); j++)
        for (i = 0; i < bmdi->vpudrvctx.max_num_instance; i++) {
            KeInitializeEvent(&bmdi->vpudrvctx.interrupt_wait_q[j][i], SynchronizationEvent, FALSE);
        }
}

static int bm_vpu_int_fifo_init(struct bm_device_info *bmdi, int op)
{
    u32 i, j;
    int ret = 0;
    int max_interrupt_queue = 16 * bmdi->vpudrvctx.max_num_instance;

    for (j = 0; j < (u32)get_max_num_vpu_core(bmdi); j++) {
        for (i = 0; i < bmdi->vpudrvctx.max_num_instance; i++) {
            if (op) {
                ret = kfifo_alloc(&bmdi->vpudrvctx.interrupt_pending_q[j][i], max_interrupt_queue * sizeof(u32));
                if (ret) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV] kfifo_alloc failed 0x%x\n", ret);
                    return ret;
                }
                WDF_OBJECT_ATTRIBUTES attributes;
                WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
                attributes.ParentObject = bmdi->WdfDevice;
                NTSTATUS status = WdfSpinLockCreate(&attributes, &bmdi->vpudrvctx.s_kfifo_lock[j][i]);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "SpinLock Create fail\n");
                    return -1;
                }
            } else {
                kfifo_free(&bmdi->vpudrvctx.interrupt_pending_q[j][i]);
                bmdi->vpudrvctx.interrupt_flag[j][i] = 0;
            }
        }
    }

    return 0;
}

static void bm_vpu_topaddr_set(struct bm_device_info *bmdi)
{
    u32 remap_val;
    u64 vmem_addr;

    if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
        vpudrv_buffer_t s_vpu_remap_register = {0};
        s_vpu_remap_register.phys_addr = VPU_REMAP_REG;
        remap_val = bm_read32(bmdi, (u32)s_vpu_remap_register.phys_addr);
        remap_val = (remap_val & 0xF000FF00) | 0x4040004;
        bm_write32(bmdi, (u32)s_vpu_remap_register.phys_addr, remap_val);
    }

    if (bmdi->cinfo.chip_id == 0x1682) {
        vmem_addr = 0x270000000;
        vmem_addr = vmem_addr - vmem_addr % 0x200000000;
        remap_val = bm_read32(bmdi, 0x50008008);
        remap_val &= 0xFF00FFFF;
        remap_val |= ((vmem_addr >> 32) & 0xff) << 16;
        bm_write32(bmdi, 0x50008008, remap_val);
        remap_val = bm_read32(bmdi, 0x50008008);
    }
}

static int bm_vpu_reset_core(struct bm_device_info *bmdi, int core_idx, int reset)
{
    u32 value = 0;

    if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
        value = bm_read32(bmdi, bm_vpu_rst[core_idx].reg);
        if (reset == 0)
            value &= ~(0x1 << bm_vpu_rst[core_idx].bit_n);
        else if (reset == 1)
            value |= (0x1 << bm_vpu_rst[core_idx].bit_n);
        else
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "VPUDRV :vpu reset unsupported operation\n");
        bm_write32(bmdi, bm_vpu_rst[core_idx].reg, value);
        value = bm_read32(bmdi, bm_vpu_rst[core_idx].reg);
    }

    return value;
}

static void bm_vpu_free_common_mem(struct bm_device_info *bmdi)
{
    if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
        u32 i;
        WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
        for (i = 0; i < (u32)get_max_num_vpu_core(bmdi); i++) {
            if (bmdi->vpudrvctx.s_common_memory[i].base) {
                bmdrv_bgm_heap_free(&bmdi->gmem_info.common_heap[VPU_HEAP_ID_3], bmdi->vpudrvctx.s_common_memory[i].phys_addr);
                bmdi->vpudrvctx.s_common_memory[i].base = 0;
            }
        }
        WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
    }
}

static int bm_vpu_res_init(struct bm_device_info *bmdi)
{
    InitializeListHead(&bmdi->vpudrvctx.s_vbp_head);
    InitializeListHead(&bmdi->vpudrvctx.s_inst_list_head);
    InitializeListHead(&bmdi->vpudrvctx.s_core_idx_head);
    InitializeListHead(&bmdi->vpudrvctx.s_mmap_list);

    ExInitializeFastMutex(&bmdi->vpudrvctx.s_vpu_mutex);
    KeInitializeSemaphore(&bmdi->vpudrvctx.s_vpu_sem, 1, 1);

    return 0;
}

static void get_pll_ctl_setting(struct bm_pll_ctrl *best,struct bm_device_info *bmdi)
{
    // for vpu 640MHZ
    best->fbdiv = 128;
    best->refdiv = 1;
    best->postdiv1 = 5;
    best->postdiv2 = 1;

    if(bmdi->cinfo.chip_id == 0x1686){
	    // for 1686 vpu 800MHZ
        int chip_mode = top_reg_read(bmdi, 0x4) & 0x3;
        if(chip_mode != NORMAL_MODE){
	        best->fbdiv = 256;
	        best->refdiv = 2;
	        best->postdiv1 = 4;
	        best->postdiv2 = 1;
        }
    }
}

static void bm_vpu_pll_set_freq(struct bm_device_info *bmdi)
{
    unsigned int enable, status;
    unsigned int div, status_id, pll_id;
    struct bm_pll_ctrl pll_ctrl = { 0 };
    // for vpu
    status_id = 2;
    pll_id = 3;

    get_pll_ctl_setting(&pll_ctrl,bmdi);

    div = TOP_PLL_CTRL(pll_ctrl.fbdiv, pll_ctrl.postdiv1, pll_ctrl.postdiv2,
            pll_ctrl.refdiv);

    enable = top_reg_read(bmdi, TOP_PLL_ENABLE);
    top_reg_write(bmdi, TOP_PLL_ENABLE, enable & ~(0x1 << status_id));
    top_reg_write(bmdi, TOP_VPLL_CTL, div);

    status = top_reg_read(bmdi, TOP_PLL_STATUS);
    while (!((status >> (PLL_STAT_LOCK_OFFSET + status_id)) & 0x1))
        status = top_reg_read(bmdi, TOP_PLL_STATUS);

    status = top_reg_read(bmdi, TOP_PLL_STATUS);
    while ((status >> status_id) & 0x1)
        status = top_reg_read(bmdi, TOP_PLL_STATUS);

    top_reg_write(bmdi, TOP_PLL_ENABLE, enable | (0x1 << pll_id));
}

int bm_vpu_init_bm1686(struct bm_device_info *bmdi)
{
    if (bmdi->cinfo.chip_id == 0x1686) {

        s_vpu_irq_1684[0] = BM1684_VPU_IRQ_NUM_0;
        s_vpu_irq_1684[1] = BM1684_VPU_IRQ_NUM_2;
        s_vpu_irq_1684[2] = BM1684_VPU_IRQ_NUM_4;

        s_vpu_reg_phy_base_1684[0] = 0x50050000;
        s_vpu_reg_phy_base_1684[1] = 0x500D0000;
        s_vpu_reg_phy_base_1684[2] = 0x50126000;

        bm_vpu_rst[0].reg   = SW_RESET_REG1;
        bm_vpu_rst[0].bit_n = 18;
        bm_vpu_rst[1].reg   = SW_RESET_REG1;
        bm_vpu_rst[1].bit_n = 28;
        bm_vpu_rst[2].reg   = SW_RESET_REG2;
        bm_vpu_rst[2].bit_n = 10;

        bm_vpu_en_axi_clk[0].reg   = TOP_CLK_EN_REG1;
        bm_vpu_en_axi_clk[0].bit_n = CLK_EN_AXI_VD0_WAVE0_GEN_REG1;
        bm_vpu_en_axi_clk[1].reg   = TOP_CLK_EN_REG2;
        bm_vpu_en_axi_clk[1].bit_n = CLK_EN_APB_VD1_WAVE0_GEN_REG2;
        bm_vpu_en_axi_clk[2].reg   = TOP_CLK_EN_REG1;
        bm_vpu_en_axi_clk[2].bit_n = CLK_EN_APB_VDE_WAVE_GEN_REG1;
    }
    return 0;
}


int bm_vpu_init(struct bm_device_info *bmdi)
{
    int ret = 0;
    u32 i = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] *********************************************************************begin vpu_init\n");
    if (bmdi->cinfo.chip_id == 0x1684) {
        bmdi->vpudrvctx.s_vpu_irq = s_vpu_irq_1684;
        bmdi->vpudrvctx.s_vpu_reg_phy_base = s_vpu_reg_phy_base_1684;
        bmdi->vpudrvctx.max_num_vpu_core = 5;
        bmdi->vpudrvctx.max_num_instance = 32;
    }

    if (bmdi->cinfo.chip_id == 0x1686) {
        bmdi->vpudrvctx.s_vpu_irq = s_vpu_irq_1686;
        bmdi->vpudrvctx.s_vpu_reg_phy_base = s_vpu_reg_phy_base_1686;
        bmdi->vpudrvctx.max_num_vpu_core = 3;
        bmdi->vpudrvctx.max_num_instance = 32;
    }

    if (bmdi->cinfo.chip_id == 0x1682) {
        bmdi->vpudrvctx.s_vpu_irq = s_vpu_irq_1682;
        bmdi->vpudrvctx.s_vpu_reg_phy_base = s_vpu_reg_phy_base_1682;
        bmdi->vpudrvctx.max_num_vpu_core = 3;
        bmdi->vpudrvctx.max_num_instance = 8;
    }
    //bm_vpu_init_bm1686(bmdi);

#ifdef VPU_SUPPORT_CLOCK_CONTROL
    bm_vpu_clk_enable(INIT);
#endif
    if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
        bm_vpu_pll_set_freq(bmdi);
    }
    bm_vpu_res_init(bmdi);

    ret = bm_vpu_int_fifo_init(bmdi, INIT);
    if (ret) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "[VPUDRV] fifo init failed!\n");
        return ret;
    }

    bm_vpu_int_wq_init(bmdi);
    bm_vpu_inst_pool_init(bmdi, INIT);
    bm_vpu_address_init(bmdi, INIT);
    bm_vpu_topaddr_set(bmdi);

    for (i = 0; i < (u32)get_max_num_vpu_core(bmdi); i++) {
        struct ion_allocation_data alloc_data;
        alloc_data.heap_id                              = VPU_HEAP_ID_3;
        alloc_data.heap_id_mask                         = 1 << VPU_HEAP_ID_3;
        alloc_data.len                                  = SIZE_COMMON;
        WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
        ret = bmdrv_gmem_alloc(bmdi, &alloc_data);
        WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
        if (ret == -1) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "[%s,%d] can not allocate the common buffer.\n", __func__, __LINE__);
            ret = -1;
            break;
        }
        bmdi->vpudrvctx.s_common_memory[i].phys_addr = alloc_data.paddr;
        bmdi->vpudrvctx.s_common_memory[i].size      = (u32)alloc_data.len;
        bmdi->vpudrvctx.s_common_memory[i].core_idx  = i;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] end vpu_init result=0x%x\n", ret);
    return 0;
}

void bm_vpu_exit(struct bm_device_info *bmdi)
{

#ifdef VPU_SUPPORT_CLOCK_CONTROL
    bm_vpu_clk_enable(FREE);
#endif
    bm_vpu_inst_pool_init(bmdi, FREE);
    bm_vpu_free_common_mem(bmdi);
    bm_vpu_int_fifo_init(bmdi, FREE);
    bm_vpu_address_init(bmdi, FREE);
}

static void bmdrv_vpu_irq_handler(struct bm_device_info *bmdi)
{
    bm_vpu_irq_handler(bmdi);
}

void bm_vpu_request_irq(struct bm_device_info *bmdi)
{
    u32 i = 0;

    if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
        for (i = 0; i < (u32)get_max_num_vpu_core(bmdi); i++){
            bmdrv_submodule_request_irq(bmdi, bmdi->vpudrvctx.s_vpu_irq[i], bmdrv_vpu_irq_handler);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] vpudrvctx.s_vpu_irq[%d]: %d\n", i, bmdi->vpudrvctx.s_vpu_irq[i]);
        }
    }
}

void bm_vpu_free_irq(struct bm_device_info *bmdi)
{
    u32 i = 0;

    if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
        //TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[VPUDRV] vpudrvctx.s_vpu_irq[0]: %d\n", bmdi->vpudrvctx.s_vpu_irq[0]);
        for (i = 0; i < (u32)get_max_num_vpu_core(bmdi); i++)
            bmdrv_submodule_free_irq(bmdi, bmdi->vpudrvctx.s_vpu_irq[i]);
    }
}

//typedef struct vpu_inst_info {
//    unsigned long flag;
//    unsigned long time;
//    unsigned long status;
//    unsigned char *url;
//} vpu_inst_info_t;
//
//static vpu_inst_info_t s_vpu_inst_info[MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU] = {0};
//static DEFINE_MUTEX(s_vpu_proc_lock);
//
//static unsigned long get_ms_time(void)
//{
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
//    struct timespec64 tv;
//#else
//    struct timeval tv;
//#endif
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
//    ktime_get_real_ts64(&tv);
//#else
//    do_gettimeofday(&tv);
//#endif
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
//    return tv.tv_sec * 1000 + tv.tv_nsec / 1000000;
//#else
//    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
//#endif
//}
//
//static void set_stop_inst_info(unsigned long flag)
//{
//    int i;
//
//    //mutex_lock(&s_vpu_proc_lock);
//    if (flag == 0) { //set all to stop
//        for (i = 0; i < get_max_num_vpu_core(bmdi) * MAX_NUM_INSTANCE_VPU; i++) {
//            if (s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].status == 0) {
//                s_vpu_inst_info[i].time = get_ms_time();
//                s_vpu_inst_info[i].status = 1;
//            }
//        }
//    } else {
//        for (i = 0; i < get_max_num_vpu_core(bmdi) * MAX_NUM_INSTANCE_VPU; i++) {
//            if (s_vpu_inst_info[i].flag == flag && s_vpu_inst_info[i].status == 0) {
//                s_vpu_inst_info[i].time = get_ms_time();
//                s_vpu_inst_info[i].status = 1;
//                break;
//            }
//        }
//    }
//    //mutex_unlock(&s_vpu_proc_lock);
//}
//
//static void clean_timeout_inst_info(unsigned long timeout)
//{
//    unsigned long cur_time = get_ms_time();
//    int i;
//
//    //mutex_lock(&s_vpu_proc_lock);
//    for (i = 0; i < get_max_num_vpu_core(bmdi) * MAX_NUM_INSTANCE_VPU; i++) {
//        if (s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].status == 1 && cur_time - s_vpu_inst_info[i].time > timeout) {
//            s_vpu_inst_info[i].flag = 0;
//            s_vpu_inst_info[i].status = 0;
//            s_vpu_inst_info[i].time = 0;
//            vfree(s_vpu_inst_info[i].url);
//            s_vpu_inst_info[i].url = NULL;
//        }
//    }
//    //mutex_unlock(&s_vpu_proc_lock);
//}
//
//static int get_inst_info_idx(unsigned long flag)
//{
//    int i = -1;
//
//    for (i = 0; i < get_max_num_vpu_core(bmdi) * MAX_NUM_INSTANCE_VPU; i++) {
//        if (s_vpu_inst_info[i].flag == flag)
//            return i;
//    }
//
//    for (i = 0; i < get_max_num_vpu_core(bmdi) * MAX_NUM_INSTANCE_VPU; i++) {
//        if (s_vpu_inst_info[i].flag == 0)
//            break;
//    }
//
//    return i;
//}
//
//static void printf_inst_info(void *mem, int size)
//{
//    int total_len = 0;
//    int total_len_first = 0;
//    int i;
//
//    sprintf(mem, "\"link\":[\n");
//    total_len = strlen(mem);
//    total_len_first = total_len;
//
//    for (i = 0; i < get_max_num_vpu_core(bmdi) * MAX_NUM_INSTANCE_VPU; i++) {
//        if (s_vpu_inst_info[i].flag != 0) {
//            sprintf(mem + total_len, "{\"id\" : %lu, \"time\" : %lu, \"status\" : %lu, \"url\" : \"%s\"},\n",
//                    s_vpu_inst_info[i].flag, s_vpu_inst_info[i].time, s_vpu_inst_info[i].status, s_vpu_inst_info[i].url);
//            total_len = strlen(mem);
//            if (total_len >= size) {
//                TraceEvents(TRACE_LEVEL_ERROR, TRACE_VPU, "maybe overflow the memory, please check..\n");
//                break;
//            }
//        }
//    }
//
//    if (total_len > total_len_first)
//        sprintf(mem + total_len - 2, "]}\n");
//    else
//        sprintf(mem + total_len, "]}\n");
//}
//
//#define MAX_DATA_SIZE (256 * 5 * 8)
//static ssize_t info_read(struct file *file, char  *buf, size_t size, loff_t *ppos)
//{
//    struct bm_device_info *bmdi;
//    vpu_drv_context_t *dev;
//    char *dat = vmalloc(MAX_DATA_SIZE);
//    int len = 0;
//    int err = 0;
//    int i = 0;
//#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
//    vmem_info_t vinfo = {0};
//#endif
//
//    bmdi = PDE_DATA(file_inode(file));
//    dev = &bmdi->vpudrvctx;
//#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
//    vmem_get_info(&dev->s_vmem, &vinfo);
//    size = vinfo.free_pages * (unsigned long)vinfo.page_size;
//    sprintf(dat, "{\"total_mem_size\" : %lu, \"used_mem_size\" : %lu, \"free_mem_size\" : %lu,\n",
//            vinfo.total_pages * (unsigned long)vinfo.page_size, vinfo.alloc_pages * (unsigned long)vinfo.page_size, size);
//#else
//    sprintf(dat, "{");
//#endif
//    len = strlen(dat);
//    sprintf(dat + len, "\"core\" : [\n");
//    for (i = 0; i < dev->max_num_vpu_core - 1; i++) {
//        len = strlen(dat);
//        sprintf(dat + len, "{\"id\":%d, \"link_num\":%d}, ", i, dev->s_vpu_open_ref_count[i]);
//    }
//    len = strlen(dat);
//    sprintf(dat + len, "{\"id\":%d, \"link_num\":%d}],\n", i, dev->s_vpu_open_ref_count[i]);
//
//    len = strlen(dat);
//    printf_inst_info(dat+len, MAX_DATA_SIZE-len);
//    len = strlen(dat) + 1;
//    if (size < len) {
//        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,KERN_ERR "read buf too small\n");
//        err = -1;
//        goto info_read_error;
//    }
//
//    if (*ppos >= len) {
//        err = -1;
//        goto info_read_error;
//    }
//
//    err = copy_to_user(buf, dat, len);
//info_read_error:
//    if (err) {
//        vfree(dat);
//        return 0;
//    }
//
//    *ppos = len;
//    vfree(dat);
//    return len;
//}
//
//static ssize_t info_write(struct file *file, const char  *buf, size_t size, loff_t *ppos)
//{
//    //for debug the vpu.
//    char data[256] = {0};
//    int err = 0;
//    int core_idx = -1;
//    int inst_idx = -1;
//    int in_num = -1;
//    int out_num = -1;
//    int i, j;
//    unsigned long flag;
//
//    if (size > 256)
//        return 0;
//    err = copy_from_user(data, buf, size);
//    if (err)
//        return 0;
//
//    sscanf(data, "%ld ", &flag);
//
//    if (flag > get_max_num_vpu_core(bmdi)) {
//        unsigned long __time;
//        unsigned long __status;
//        unsigned char __url[256];
//
//        sscanf(data, "%ld %ld %ld, %s", &flag, &__time, &__status, __url);
//
//        if (__status == 1) {
//            set_stop_inst_info(flag);
//        } else {
//            clean_timeout_inst_info(60*1000);
//            //mutex_lock(&s_vpu_proc_lock);
//            i = get_inst_info_idx(flag);
//            if (i < get_max_num_vpu_core(bmdi) * MAX_NUM_INSTANCE_VPU) {
//                if (s_vpu_inst_info[i].url != NULL)
//                    vfree(s_vpu_inst_info[i].url);
//                j = strlen(__url);
//                s_vpu_inst_info[i].url = vmalloc(j+1);
//                memcpy(s_vpu_inst_info[i].url, __url, j);
//                s_vpu_inst_info[i].url[j] = 0;
//                s_vpu_inst_info[i].time = __time;
//                s_vpu_inst_info[i].status = __status;
//                s_vpu_inst_info[i].flag = flag;
//            }
//            //mutex_unlock(&s_vpu_proc_lock);
//        }
//        return size;
//    }
//
//    sscanf(data, "%d %d %d %d", &core_idx, &inst_idx, &in_num, &out_num);
//    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"%s", data);
//    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"core_idx: %d, inst: %d, in_num: %d, out_num: %d\n", core_idx, inst_idx, in_num, out_num);
//#if 0
//    if (s_vpu_dump_flag) {
//        if (core_idx >= 0 && core_idx < get_max_num_vpu_core(bmdi) && inst_idx >= 0 && inst_idx < MAX_NUM_INSTANCE_VPU) {
//            if (in_num >= 0)
//                s_vpu_dump_flag[core_idx * MAX_NUM_INSTANCE_VPU * 2 + inst_idx * 2] = in_num;
//            if (out_num >= 0)
//                s_vpu_dump_flag[core_idx * MAX_NUM_INSTANCE_VPU * 2 + inst_idx * 2 + 1] = out_num;
//        } else if(core_idx == -1) {
//            for (i = 0; i < get_max_num_vpu_core(bmdi); i++)
//            {
//                for (j = 0; j < 2*MAX_NUM_INSTANCE_VPU; j = j+2)
//                {
//                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"[%d, %d]; ", s_vpu_dump_flag[i*MAX_NUM_INSTANCE_VPU*2+j], s_vpu_dump_flag[i*MAX_NUM_INSTANCE_VPU*2+j+1]);
//                }
//                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_VPU,"\n");
//            }
//        } else if(core_idx == -2) {
//            memset(s_vpu_dump_flag, 0, sizeof(u32)*get_max_num_vpu_core(bmdi)*MAX_NUM_INSTANCE_VPU*2);
//        }
//    }
//#endif
//    return size;
//}
//
//const struct file_operations bmdrv_vpu_file_ops = {
//    .read   = info_read,
//    .write  = info_write,
//};
//
static int bm_vpu_hw_reset(struct bm_device_info *bmdi, int core_idx)
{
    bm_vpu_reset_core(bmdi, core_idx, 0);
    bm_mdelay(10);
    bm_vpu_reset_core(bmdi, core_idx, 1);

    return 0;
}
