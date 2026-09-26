#include <linux/device.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include "bm_common.h"
#include "84x6_reg.h"
#include "bm_thread.h"
#include "bm_timer.h"
#include "bm_irq.h"
#include "84x6_cdma.h"
#include "84x6_irq.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#else
#include "84x6_pcie.h"
#endif
#include "84x6_timer.h"

#define CDMA_SEL_OFFSET 0x100000

static DECLARE_WAIT_QUEUE_HEAD(req_waitq);

#define WAIT_IRQ_COMPLETE

static void cdma0_reg_write(struct bm_device_info *bmdi, u32 reg_offset,
                            u32 val)
{
    cdma_reg_write(bmdi, reg_offset, val);
}

static void cdma1_reg_write(struct bm_device_info *bmdi, u32 reg_offset,
                            u32 val)
{
    cdma_reg_write(bmdi, reg_offset + CDMA_SEL_OFFSET, val);
}

static u32 cdma0_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return cdma_reg_read(bmdi, reg_offset);
}

static u32 cdma1_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
    return cdma_reg_read(bmdi, reg_offset + CDMA_SEL_OFFSET);
}

void bm84x6_clear_cdmairq0(struct bm_device_info *bmdi)
{
    int reg_val;

    reg_val = cdma_reg_read(bmdi, CDMA_INT_STAT);
    reg_val |= 1;
    cdma_reg_write(bmdi, CDMA_INT_STAT, reg_val);
}

void bm84x6_clear_cdmairq1(struct bm_device_info *bmdi)
{
    int reg_val;

    reg_val = cdma_reg_read(bmdi, CDMA_INT_STAT + CDMA_SEL_OFFSET);
    reg_val |= 1;
    cdma_reg_write(bmdi, CDMA_INT_STAT + CDMA_SEL_OFFSET, reg_val);
}

u32 bm84x6_cdma_transfer(struct bm_device_info *bmdi, struct file *file,
                         pbm_cdma_arg parg, bool lock_cdma)
{
    struct bm_thread_info *ti = NULL;
    struct bm_handle_info *h_info = NULL;
    struct bm_trace_item *ptitem = NULL;
    struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
    pid_t api_pid;
    u32 timeout_ms = bmdi->cinfo.delay_ms;
    u64 ret_wait = 0;
    u32 src_addr_hi = 0;
    u32 src_addr_lo = 0;
    u32 dst_addr_hi = 0;
    u32 dst_addr_lo = 0;
    u32 cdma_max_payload = 0;
    u32 reg_val = 0;
    u64 src = parg->src;
    u64 dst = parg->dst;
    u32 count = 3000;
    u32 nv_cdma_start_us = 0;
    u32 nv_cdma_end_us = 0;
    u32 nv_cdma_send_us = 0;
    u32 timeout = timeout_ms * 1000;
    // u32 int_mask_val;

    //printk("cdma transfer enter\n");

    if (bmdi->cinfo.platform == PALLADIUM)
        timeout_ms *= PALLADIUM_CLK_RATIO;

    if (file) {
        if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
            pr_err("bmdrv: file list is not found!\n");
            return -EINVAL;
        }
    }
    if (lock_cdma)
        mutex_lock(&memcpy_info->cdma_mutex);

    bm84x6_timer_start(bmdi);
    udelay(1);
    nv_cdma_send_us = bm84x6_timer_get_time_us(bmdi);

    api_pid = current->pid;
    ti = bmdrv_find_thread_info(h_info, api_pid);

    if (ti && (parg->dir == HOST2CHIP)) {
        ti->profile.cdma_out_counter++;
        bmdi->profile.cdma_out_counter++;
    } else {
        if (ti) {
            ti->profile.cdma_in_counter++;
            bmdi->profile.cdma_in_counter++;
        }
    }

    if (ti && ti->trace_enable) {
        ptitem =
            (struct bm_trace_item *)mempool_alloc(bmdi->
                                                  trace_info.trace_mempool,
                                                  GFP_KERNEL);
        ptitem->payload.trace_type = 0;
        ptitem->payload.cdma_dir = parg->dir;
        ptitem->payload.sent_time = nv_cdma_send_us;
        ptitem->payload.start_time = 0;
    }
///////////  cdma_setting_apply atart///////
    /*Enable CDMA & interrupt */
    reg_val = cdma_reg_read(bmdi, CDMA_CHL_CTRL);
    if (parg->intr)
        reg_val |= CDMA_CHL_CTRL_IRQ_EN;
    else
        reg_val &= ~CDMA_CHL_CTRL_IRQ_EN;
    reg_val |= CDMA_CHL_CTRL_DMA_EN;
    cdma_reg_write(bmdi, CDMA_CHL_CTRL, reg_val);

    /*set max payload which equal to PCIE bandwidth. */
#ifdef SOC_MODE
    cdma_max_payload = 0x9;
#else
    cdma_max_payload = memcpy_info->cdma_max_payload;
#endif
    cdma_max_payload &= 0x3F;   // [0:2]read payload width, [3:5]write payload width

    cdma_reg_write(bmdi, CDMA_MAX_PAYLOAD, cdma_max_payload);

    // Check cdma's FIFO status.
    timeout = timeout_ms * 1000;
    while (cdma_reg_read(bmdi, CDMA_CMD_FIFO_STAT) > 7) {
        udelay(1);
        if (--timeout == 0) {
            pr_err("cdma fifo_stat timeout\n");
            if (lock_cdma)
                mutex_unlock(&memcpy_info->cdma_mutex);
            return -EBUSY;
        }
    }
    PR_TRACE("CDMA fifo wait %dus\n", timeout_ms * 1000 - timeout);

    /* clear rf_des_int_mask */
    reg_val = cdma_reg_read(bmdi, CDMA_INT_MASK);
    reg_val &= ~(CDMA_INT_RF_DES_MASK);
    reg_val |= CDMA_INT_RF_CMD_EPT_MASK;
    cdma_reg_write(bmdi, CDMA_INT_MASK, reg_val);

    src_addr_hi = src >> 32;
    src_addr_lo = src & 0xffffffff;
    dst_addr_hi = dst >> 32;
    dst_addr_lo = dst & 0xffffffff;

#ifdef SOC_MODE
    src_addr_hi |= (0x6 << 28);
    dst_addr_hi |= (0x6 << 28);
#else
    //TODO: use bmdi->cinfo.ob_base for multi-chip??
    if (parg->dir == HOST2CHIP) {
        //0x1: pcie x8 ctrl port, 0x7: dst id for PC
        src_addr_hi |= ((0x1 << 28) | (0x7 << 25)); //bmdi->cinfo.ob_base
        //0x6: local memory port
        dst_addr_hi |= (0x6 << 28);
    } else if (parg->dir == CHIP2HOST) {
        src_addr_hi |= (0x6 << 28);
        dst_addr_hi |= ((0x1 << 28) | (0x7 << 25)); //bmdi->cinfo.ob_base
    } else if (parg->dir == CHIP2CHIP) {
        src_addr_hi |= (0x6 << 28);
        dst_addr_hi |= (0x6 << 28);
    }
#endif

    //pr_info("src:0x%llx dst:0x%llx size:%lld type:%d\n", src, dst, parg->size, parg->type);
    PR_TRACE("src_addr_hi 0x%x src_addr_low 0x%x\n", src_addr_hi, src_addr_lo);
    PR_TRACE("dst_addr_hi 0x%x dst_addr_low 0x%x\n", dst_addr_hi, dst_addr_lo);

    cdma_reg_write(bmdi, CDMA_CMD_ACCP3, src_addr_lo);
    cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_hi);
    cdma_reg_write(bmdi, CDMA_CMD_ACCP5, dst_addr_lo);
    cdma_reg_write(bmdi, CDMA_CMD_ACCP6, dst_addr_hi);
    if (parg->type == 0) {
        /* 1-D descriptor */
        cdma_reg_write(bmdi, CDMA_CMD_ACCP7, parg->size);
        reg_val = CDMA_CMD_NORMAL_CFG;
    } else if (parg->type == 1) {
        /* 2-D descriptor */
        cdma_reg_write(bmdi, CDMA_CMD_ACCP2,
                       (parg->width << 16) | parg->height);
        cdma_reg_write(bmdi, CDMA_CMD_ACCP1,
                       (parg->src_width << 16) | parg->dst_width);
        reg_val = CDMA_CMD_STRIDE_CFG;
        if (parg->flush) {
            parg->size = parg->dst_width * parg->height;    // image_data + fixed_data
            reg_val |= CDMA_CMD_TRANS_WITH_FLUSH;
            reg_val |= parg->fixed_data << 16;
        } else {
            parg->size = parg->width * parg->height;
        }
        if (parg->format == 2) {
            parg->size *= 2;
            reg_val |= CDMA_CMD_2D_FORMAT_WORD;
        }
    }
//////////////////////cdma_setting_apply  end
    /* Using interrupt(10s timeout) or polling for detect cmda done */
    nv_cdma_start_us = bm84x6_timer_get_time_us(bmdi);
    cdma_reg_write(bmdi, CDMA_CMD_ACCP0, reg_val);
    if (parg->intr) {
        PR_TRACE("wait cdma\n");
        ret_wait = wait_for_completion_timeout(&memcpy_info->cdma_done0,
                                               msecs_to_jiffies(timeout_ms));
        nv_cdma_end_us = bm84x6_timer_get_time_us(bmdi);
        PR_TRACE("src:0x%llx dst:0x%llx size:%llx\n", src, dst, parg->size);
        PR_TRACE("time = %d\n", nv_cdma_end_us - nv_cdma_start_us);
        PR_TRACE
            ("time = %d, function_num = %d, start = %d, end = %d, size = %llx, max_payload = %d\n",
             nv_cdma_end_us - nv_cdma_start_us, bmdi->cinfo.chip_index & 0x3,
             nv_cdma_start_us, nv_cdma_end_us, parg->size, cdma_max_payload);

        if (ret_wait) {
            PR_TRACE("End: wait cdma done\n");
        } else {
            pr_info
                ("bmsophon%d, wait cdma timeout, src:0x%llx dst:0x%llx size:0x%llx dir:%d\n",
                 bmdi->dev_index, src, dst, parg->size, parg->dir);
            if (lock_cdma)
                mutex_unlock(&memcpy_info->cdma_mutex);
            return -EBUSY;
        }
    } else {
        while (((cdma_reg_read(bmdi, CDMA_INT_STAT)) & 0x1) != 0x1) {
            udelay(1);
            if (--count == 0) {
                pr_err("[%s: %d] cdma polling wait timeout\n", __func__,
                       __LINE__);
                if (lock_cdma)
                    mutex_unlock(&memcpy_info->cdma_mutex);
                return -EBUSY;
            }
        }
        nv_cdma_end_us = bm84x6_timer_get_time_us(bmdi);
        reg_val = cdma_reg_read(bmdi, CDMA_INT_STAT);
        reg_val |= (1 << 0x0);
        cdma_reg_write(bmdi, CDMA_INT_STAT, reg_val);

        PR_TRACE("cdma transfer using polling mode end\n");
    }

    if (ti && (parg->dir == HOST2CHIP)) {
        ti->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
        bmdi->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
    } else {
        if (ti) {
            ti->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
            bmdi->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
        }
    }

    if (ti && ti->trace_enable) {
        ptitem->payload.end_time = nv_cdma_end_us;
        ptitem->payload.start_time = nv_cdma_start_us;
        INIT_LIST_HEAD(&ptitem->node);
        mutex_lock(&ti->trace_mutex);
        list_add_tail(&ptitem->node, &ti->trace_list);
        ti->trace_item_num++;
        mutex_unlock(&ti->trace_mutex);
    }

    bm84x6_timer_stop(bmdi);

    if (lock_cdma)
        mutex_unlock(&memcpy_info->cdma_mutex);
    // printk("cdma transfer exit\n");
    return 0;
}

static void dual_cdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset,
                                u32 val, u32 cdma)
{
    if (cdma == 1)
        reg_offset += CDMA_SEL_OFFSET;

    cdma_reg_write(bmdi, reg_offset, val);
}

static u32 dual_cdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset,
                              u32 cdma)
{
    if (cdma == 1)
        reg_offset += CDMA_SEL_OFFSET;

    return cdma_reg_read(bmdi, reg_offset);
}

void bm84x6_cdma0_irq_handler(struct bm_device_info *bmdi)
{
    u32 int_status = cdma0_reg_read(bmdi, CDMA_INT_STAT);
    if (int_status & 0x1) {
        wake_up_interruptible(&req_waitq);
#ifdef WAIT_IRQ_COMPLETE
        complete(&bmdi->memcpy_info.cdma_done0);
#endif
        cdma0_reg_write(bmdi, CDMA_INT_STAT, int_status);
    }
    pr_debug("cdma0 interrupt 0x%x\n", int_status);
}

void bm84x6_cdma1_irq_handler(struct bm_device_info *bmdi)
{
    u32 int_status = cdma1_reg_read(bmdi, CDMA_INT_STAT);
    if (int_status & 0x1) {
        wake_up_interruptible(&req_waitq);
#ifdef WAIT_IRQ_COMPLETE
        complete(&bmdi->memcpy_info.cdma_done1);
#endif
        cdma1_reg_write(bmdi, CDMA_INT_STAT, int_status);
    }
    pr_debug("cdma1 interrupt 0x%x\n", int_status);
}

static void dual_cdma_lock(struct bm_device_info *bmdi, u32 cdma)
{
    struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

    switch (cdma) {
    case 0:
        mutex_lock(&memcpy_info->cdma_mutex0);
        break;
    case 1:
        mutex_lock(&memcpy_info->cdma_mutex1);
        break;
    default:
        break;
    }
}

static void dual_cdma_unlock(struct bm_device_info *bmdi, u32 cdma)
{
    struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

    switch (cdma) {
    case 0:
        mutex_unlock(&memcpy_info->cdma_mutex0);
        break;
    case 1:
        mutex_unlock(&memcpy_info->cdma_mutex1);
        break;
    default:
        break;
    }
}

int bm84x6_dual_cdma_init(struct bm_device_info *bmdi)
{
    // request_irq
#ifndef SOC_MODE
    bmdrv_submodule_request_irq(bmdi, BM84X6_CDMA0_IRQ_ID,
                                bm84x6_cdma0_irq_handler);
    bmdrv_submodule_request_irq(bmdi, BM84X6_CDMA1_IRQ_ID,
                                bm84x6_cdma1_irq_handler);
#endif

    pr_info("dual cdma device initialized\n");
    return 0;
}

int bm84x6_dual_cdma_remove(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
    bmdrv_submodule_free_irq(bmdi, BM84X6_CDMA0_IRQ_ID);
    bmdrv_submodule_free_irq(bmdi, BM84X6_CDMA1_IRQ_ID);
#endif
#if 0
    device_destroy(dual_cdma_class, MKDEV(CDMA_DUAL_MAJOR, 0));
    class_destroy(dual_cdma_class);

    unregister_chrdev(CDMA_DUAL_MAJOR, CDMA_DUAL_NAME);
#endif
    // request_irq
    pr_info("dual cdma device removed\n");
    return 0;
}

static void cdma_setting_apply_for_test(struct bm_device_info *bmdi,
                                        pbm_cdma_arg parg, u32 * acc0, u32 cdma)
{
#ifndef SOC_MODE
    struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
#endif
    u64 src = parg->src;
    u64 dst = parg->dst;
    u32 src_addr_hi = 0;
    u32 src_addr_lo = 0;
    u32 dst_addr_hi = 0;
    u32 dst_addr_lo = 0;
    u32 reg_val = 0;
    u32 cdma_max_payload = 0;

    /*Enable CDMA & interrupt */
    reg_val = dual_cdma_reg_read(bmdi, CDMA_CHL_CTRL, cdma);
    if (parg->intr)
        reg_val |= CDMA_CHL_CTRL_IRQ_EN;
    else
        reg_val &= ~CDMA_CHL_CTRL_IRQ_EN;
    reg_val |= CDMA_CHL_CTRL_DMA_EN;
    dual_cdma_reg_write(bmdi, CDMA_CHL_CTRL, reg_val, cdma);

    /*set max payload which equal to PCIE bandwidth. */
#ifdef SOC_MODE
    cdma_max_payload = 0x9;
#else
    cdma_max_payload = memcpy_info->cdma_max_payload;
#endif
    cdma_max_payload &= 0x3F;   // [0:2]read payload width, [3:5]write payload width

    dual_cdma_reg_write(bmdi, CDMA_MAX_PAYLOAD, cdma_max_payload, cdma);

    /* clear rf_des_int_mask */
    reg_val = dual_cdma_reg_read(bmdi, CDMA_INT_MASK, cdma);
    reg_val &= ~(CDMA_INT_RF_DES_MASK);
    reg_val |= CDMA_INT_RF_CMD_EPT_MASK;
    dual_cdma_reg_write(bmdi, CDMA_INT_MASK, reg_val, cdma);

#ifndef SOC_MODE
    if (parg->dir != CHIP2CHIP) {
        if (!parg->use_iommu) {
            if (parg->dir == CHIP2HOST)
                dst |= (u64) 0x800 << 32;
            else
                src |= (u64) 0x800 << 32;
        }
    }
#endif
    src_addr_hi = src >> 32;
    src_addr_lo = src & 0xffffffff;
    dst_addr_hi = dst >> 32;
    dst_addr_lo = dst & 0xffffffff;

    if (parg->dir == HOST2CHIP) {
        /* flush src data */
#ifndef SOC_MODE
        src_addr_hi |= (0x1 << 28); //0x1: pcie x8 ctrl port
#else
        src_addr_hi |= (0x6 << 28);
#endif
        dst_addr_hi |= (0x6 << 28);
    } else if (parg->dir == CHIP2HOST) {
        src_addr_hi |= (0x6 << 28);
#ifndef SOC_MODE
        dst_addr_hi |= (0x1 << 28); //0x1: pcie x8 ctrl port
#else
        dst_addr_hi |= (0x6 << 28);
#endif
        /* invalidate dst data */
    } else if (parg->dir == CHIP2CHIP) {
        src_addr_hi |= (0x6 << 28);
        dst_addr_hi |= (0x6 << 28);
    }
    PR_TRACE("src:0x%llx dst:0x%llx size:%lld\n", src, dst, parg->size);
    PR_TRACE("src_addr_hi 0x%x src_addr_low 0x%x\n", src_addr_hi, src_addr_lo);
    PR_TRACE("dst_addr_hi 0x%x dst_addr_low 0x%x\n", dst_addr_hi, dst_addr_lo);

    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP3, src_addr_lo, cdma);
    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_hi, cdma);
    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP5, dst_addr_lo, cdma);
    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP6, dst_addr_hi, cdma);

    if (parg->type == 1) {
        /* 2-D descriptor */
        dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP2,
                            (parg->width << 16) | parg->height, cdma);
        dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP1,
                            (parg->src_width << 16) | parg->dst_width, cdma);
        reg_val = CDMA_CMD_STRIDE_CFG;
        if (parg->flush) {
            parg->size = parg->dst_width * parg->height;    // image_data + fixed_data
            reg_val |= CDMA_CMD_TRANS_WITH_FLUSH;
            reg_val |= parg->fixed_data << 16;
        } else {
            parg->size = parg->width * parg->height;
        }
        if (parg->format == 2) {
            parg->size *= 2;
            reg_val |= CDMA_CMD_2D_FORMAT_WORD;
        }
    } else {
        /* 1-D descriptor */
        dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP7, parg->size, cdma);
        reg_val = CDMA_CMD_NORMAL_CFG;
    }

    *acc0 = reg_val;
}

static int cdma_setting_apply(struct bm_device_info *bmdi, pbm_cdma_arg parg,
                              u32 * acc0, u32 cdma, bool lock_cdma)
{
#ifndef SOC_MODE
    struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
#endif
    u64 src = parg->src;
    u64 dst = parg->dst;
    u32 src_addr_hi = 0;
    u32 src_addr_lo = 0;
    u32 dst_addr_hi = 0;
    u32 dst_addr_lo = 0;
    u32 reg_val = 0;
    u32 cdma_max_payload = 0;
    u32 timeout_ms = bmdi->cinfo.delay_ms;
    u32 timeout = timeout_ms * 1000;

    /*Enable CDMA & interrupt */
    reg_val = dual_cdma_reg_read(bmdi, CDMA_CHL_CTRL, cdma);
    if (parg->intr)
        reg_val |= CDMA_CHL_CTRL_IRQ_EN;
    else
        reg_val &= ~CDMA_CHL_CTRL_IRQ_EN;
    reg_val |= CDMA_CHL_CTRL_DMA_EN;
    dual_cdma_reg_write(bmdi, CDMA_CHL_CTRL, reg_val, cdma);

    /*set max payload which equal to PCIE bandwidth. */
#ifdef SOC_MODE
    cdma_max_payload = 0x9;
#else
    cdma_max_payload = memcpy_info->cdma_max_payload;
#endif
    cdma_max_payload &= 0x3F;   // [0:2]read payload width, [3:5]write payload width

    dual_cdma_reg_write(bmdi, CDMA_MAX_PAYLOAD, cdma_max_payload, cdma);

    // Check cdma's FIFO status.
    timeout = timeout_ms * 1000;
    while (cdma_reg_read(bmdi, CDMA_CMD_FIFO_STAT) > 7) {
        udelay(1);
        if (--timeout == 0) {
            pr_err("cdma fifo_stat timeout\n");
            if (lock_cdma)
                dual_cdma_unlock(bmdi, cdma);
            return -EBUSY;
        }
    }
    PR_TRACE("CDMA fifo wait %dus\n", timeout_ms * 1000 - timeout);

    /* clear rf_des_int_mask */
    reg_val = dual_cdma_reg_read(bmdi, CDMA_INT_MASK, cdma);
    reg_val &= ~(CDMA_INT_RF_DES_MASK);
    reg_val |= CDMA_INT_RF_CMD_EPT_MASK;
    dual_cdma_reg_write(bmdi, CDMA_INT_MASK, reg_val, cdma);

#ifndef SOC_MODE
    if (parg->dir != CHIP2CHIP) {
        if (!parg->use_iommu) {
            if (parg->dir == CHIP2HOST)
                dst |= (u64) bmdi->cinfo.ob_base << 32;
            else
                src |= (u64) bmdi->cinfo.ob_base << 32;
        }
    }
#endif
    src_addr_hi = src >> 32;
    src_addr_lo = src & 0xffffffff;
    dst_addr_hi = dst >> 32;
    dst_addr_lo = dst & 0xffffffff;
    if (parg->dir == HOST2CHIP) {
        /* flush src data */
#ifndef SOC_MODE
        src_addr_hi |= (0x1 << 28); //0x1: pcie x8 ctrl port
#else
        src_addr_hi |= (0x6 << 28);
#endif
        dst_addr_hi |= (0x6 << 28);
    } else if (parg->dir == CHIP2HOST) {
        src_addr_hi |= (0x6 << 28);
#ifndef SOC_MODE
        dst_addr_hi |= (0x1 << 28); //0x1: pcie x8 ctrl port
#else
        dst_addr_hi |= (0x6 << 28);
#endif
        /* invalidate dst data */
    } else if (parg->dir == CHIP2CHIP) {
        src_addr_hi |= (0x6 << 28);
        dst_addr_hi |= (0x6 << 28);
    }
    PR_TRACE("src:0x%llx dst:0x%llx size:%lld\n", src, dst, parg->size);
    PR_TRACE("src_addr_hi 0x%x src_addr_low 0x%x\n", src_addr_hi, src_addr_lo);
    PR_TRACE("dst_addr_hi 0x%x dst_addr_low 0x%x\n", dst_addr_hi, dst_addr_lo);

    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP3, src_addr_lo, cdma);
    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_hi, cdma);
    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP5, dst_addr_lo, cdma);
    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP6, dst_addr_hi, cdma);


    if (parg->type == 1) {
        /* 2-D descriptor */
        dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP2,
                            (parg->width << 16) | parg->height, cdma);
        dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP1,
                            (parg->src_width << 16) | parg->dst_width, cdma);
        reg_val = CDMA_CMD_STRIDE_CFG;
        if (parg->flush) {
            parg->size = parg->dst_width * parg->height;    // image_data + fixed_data
            reg_val |= CDMA_CMD_TRANS_WITH_FLUSH;
            reg_val |= parg->fixed_data << 16;
        } else {
            parg->size = parg->width * parg->height;
        }
        if (parg->format == 2) {
            parg->size *= 2;
            reg_val |= CDMA_CMD_2D_FORMAT_WORD;
        }
    } else {
        /* 1-D descriptor */
        dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP7, parg->size, cdma);
        reg_val = CDMA_CMD_NORMAL_CFG;
    }

    *acc0 = reg_val;

    return 0;
}

static int get_cdma_id(struct bm_device_info *bmdi)
{
    u32 cdma0_cmd_num = 0;
    u32 cdma1_cmd_num = 0;

    cdma0_cmd_num = cdma0_reg_read(bmdi, CDMA_CMD_FIFO_STAT);
    cdma1_cmd_num = cdma1_reg_read(bmdi, CDMA_CMD_FIFO_STAT);
    if (cdma0_cmd_num >= cdma1_cmd_num)
        return 1;
    else
        return 0;
}

static int bm84x6_single_cdma_transfer(struct bm_device_info *bmdi,
                                       struct file *file, pbm_cdma_arg parg,
                                       bool lock_cdma, int cdma_id)
{
    struct bm_thread_info *ti = NULL;
    struct bm_handle_info *h_info = NULL;
    struct bm_trace_item *ptitem = NULL;
    struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
    u32 reg_val = 0;
    u32 count = 3000;
    u32 reg_accp0 = 0;
    pid_t api_pid;
    u32 timeout_ms = bmdi->cinfo.delay_ms;
    u32 nv_cdma_start_us = 0;
    u32 nv_cdma_end_us = 0;
    u32 nv_cdma_send_us = 0;
    u32 ret = 0;

#ifdef WAIT_IRQ_COMPLETE
    u64 ret_wait = 0;
#endif
    // u32 int_mask_val;

    bm84x6_timer_start(bmdi);
    udelay(1);                  //for timer stable
    pr_debug("cdma %d transfer enter\n", cdma_id);

    if (bmdi->cinfo.platform == PALLADIUM)
        timeout_ms *= PALLADIUM_CLK_RATIO;

    if (file) {
        if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
            pr_err("[bm_driver %s]line %d: file list is not found!\n", __func__,
                   __LINE__);
            return -EINVAL;
        }
    }

    if (parg == NULL) {
        pr_err("[bm_driver %s]line %d: parg is null!\n", __func__, __LINE__);
    }

    if (lock_cdma)
        dual_cdma_lock(bmdi, cdma_id);

    api_pid = current->pid;
    ti = bmdrv_find_thread_info(h_info, api_pid);

    if (ti && (parg->dir == HOST2CHIP)) {
        ti->profile.cdma_out_counter++;
        bmdi->profile.cdma_out_counter++;
    } else {
        if (ti) {
            ti->profile.cdma_in_counter++;
            bmdi->profile.cdma_in_counter++;
        }
    }

    if (ti && ti->trace_enable) {
        ptitem =
            (struct bm_trace_item *)mempool_alloc(bmdi->
                                                  trace_info.trace_mempool,
                                                  GFP_KERNEL);
        ptitem->payload.trace_type = 0;
        ptitem->payload.cdma_dir = parg->dir;
        ptitem->payload.sent_time = nv_cdma_send_us;
        ptitem->payload.start_time = 0;
    }

    ret = cdma_setting_apply(bmdi, parg, &reg_accp0, cdma_id, lock_cdma);
    nv_cdma_start_us = bm84x6_timer_get_time_us(bmdi);
    dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP0, reg_accp0, cdma_id);
    if (parg->intr) {
        PR_TRACE("wait cdma\n");
        switch (cdma_id) {
        case 0:
            ret_wait = wait_for_completion_timeout(&memcpy_info->cdma_done0,
                                                   msecs_to_jiffies
                                                   (timeout_ms));
            break;

        case 1:
            ret_wait = wait_for_completion_timeout(&memcpy_info->cdma_done1,
                                                   msecs_to_jiffies
                                                   (timeout_ms));
            break;

        default:
            pr_err("invalid cdma id:%d\n", cdma_id);
            break;
        }
        nv_cdma_end_us = bm84x6_timer_get_time_us(bmdi);
        PR_TRACE("src:0x%llx dst:0x%llx size:%llx\n", parg->src, parg->dst,
                 parg->size);
        PR_TRACE("time = %d\n", nv_cdma_end_us - nv_cdma_start_us);
        PR_TRACE
            ("time = %d, function_num = %d, start = %d, end = %d, size = %llx\n",
             nv_cdma_end_us - nv_cdma_start_us, bmdi->cinfo.chip_index & 0x3,
             nv_cdma_start_us, nv_cdma_end_us, parg->size);

        if (ret_wait) {
            PR_TRACE("End : wait cdma done\n");
        } else {
            pr_info
                ("bmsophon%d, wait cdma timeout, src:0x%llx dst:0x%llx size:0x%llx dir:%d\n",
                 bmdi->dev_index, parg->src, parg->dst, parg->size, parg->dir);
            if (lock_cdma)
                dual_cdma_unlock(bmdi, cdma_id);

            return -EBUSY;
        }
    } else {
        while (((dual_cdma_reg_read(bmdi, CDMA_INT_STAT, cdma_id)) & 0x1) !=
               0x1) {
            udelay(1);
            if (--count == 0) {
                pr_err("[%s: %d] cdma polling wait timeout\n", __func__,
                       __LINE__);
                if (lock_cdma)
                    dual_cdma_unlock(bmdi, cdma_id);
                return -EBUSY;
            }
        }
        nv_cdma_end_us = bm84x6_timer_get_time_us(bmdi);
        reg_val = dual_cdma_reg_read(bmdi, CDMA_INT_STAT, cdma_id);
        reg_val |= (1 << 0x0);
        dual_cdma_reg_write(bmdi, CDMA_INT_STAT, reg_val, cdma_id);

        PR_TRACE("cdma transfer using polling mode end\n");
    }

    if (ti && (parg->dir == HOST2CHIP)) {
        ti->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
        bmdi->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
    } else {
        if (ti) {
            ti->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
            bmdi->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
        }
    }

    if (ti && ti->trace_enable) {
        ptitem->payload.end_time = nv_cdma_end_us;
        ptitem->payload.start_time = nv_cdma_start_us;
        INIT_LIST_HEAD(&ptitem->node);
        mutex_lock(&ti->trace_mutex);
        list_add_tail(&ptitem->node, &ti->trace_list);
        ti->trace_item_num++;
        mutex_unlock(&ti->trace_mutex);
    }

    if (lock_cdma)
        dual_cdma_unlock(bmdi, cdma_id);
    bm84x6_timer_stop(bmdi);
    // printk("cdma transfer exit\n");
    return 0;
}

u32 bm84x6_dual_cdma_transfer(struct bm_device_info *bmdi, struct file *file,
                              pbm_cdma_arg parg, bool lock_cdma)
{
    //int cdma_id;
    u32 ret;

    //cdma_id = get_cdma_id(bmdi);
    ret = bm84x6_single_cdma_transfer(bmdi, file, parg, lock_cdma, 0);

    return ret;
}

#ifdef SOC_MODE
void bm84x6_cdma_reset(struct bm_device_info *bmdi)
{
    PR_TRACE("84x6 cdma reset\n");
    reset_control_assert(bmdi->cinfo.cdma);
    udelay(1000);
    reset_control_deassert(bmdi->cinfo.cdma);
}
#endif
