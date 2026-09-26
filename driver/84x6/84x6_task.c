#ifndef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": %s:%d: " fmt, __func__, __LINE__
#endif
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/printk.h>
#include <linux/kfifo.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/delay.h>

#ifndef from_timer
#define from_timer(var, cb, field) timer_container_of(var, cb, field)
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
#ifndef del_timer_sync
#define del_timer_sync(t) timer_delete_sync(t)
#endif
#endif
#include "84x6_task.h"
#include "../bm_api.h"
#include "84x6_shmem.h"
#ifndef SOC_MODE
#include "../bm_irq.h"
#include "84x6_irq.h"
#endif

static DEFINE_MUTEX(hwq_global_lock);

extern void set_thread_running(int hwq_idx, bool status);
extern int bmdev_shmem_phys_to_file(struct bm_device_info *bmdi,
                                    int mem_handle, const char *filename);

static void bmdev_tsh_irq_fallback_timer_fn(struct timer_list *t)
{
    bm_kernel_info *pstBmKernelInfo =
        from_timer(pstBmKernelInfo, t, tsh_irq_fallback_timer);

    if (!pstBmKernelInfo || !pstBmKernelInfo->bmdi)
        return;

    pstBmKernelInfo->irq_fallback_cnt++;
    pr_debug("TSH fallback timeout(5s): schedule one-shot tasklet retry.\n");
    tasklet_schedule(&pstBmKernelInfo->tsh_irq_tasklet);
}


static void print_api_rusult_err_log(u32 api_id, u32 api_result)
{
    switch (api_id) {
    case 0x90000001:
        switch (api_result) {
        case -1:
            pr_err("\tMalloc memory for library node fail.\n");
            break;

        case -2:
            pr_err("\tDlopen file fail.\n");
            break;

        default:
            break;
        }

        break;

    case 0x90000002:
        switch (api_result) {
        case -1:
            pr_err("\tdlsym fail.\n");
            break;

        case -2:
            pr_err("\tFunction not found.\n");
            break;

        case -3:
            pr_err("\tThe library containing the function was not found.\n");
            break;

        default:
            break;
        }

        break;

    case 0x90000003:
        switch (api_result) {
        case 0x12345:
            pr_err("\tFunction did not execute.\n");
            break;

        default:
            break;
        }

        break;

    default:
        pr_err("unknown api id: 0x%x\n", api_id);
        break;
    }
}


void bmdev_post_api_process(struct bm_device_info *bmdi,
                            struct api_fifo_entry api_entry)
{
    struct bm_thread_info *ti = api_entry.thd_info;
    u32 next_rp = 0;
    u32 cur_rp = 0;
    u32 api_id = 0;
    u32 api_duration = 0;
    u32 api_result = 0;
    u32 func_id = 0;
    int mem_handle = api_entry.api_data;
    int offset = bm84x6_shmem_get_offset(bmdi, mem_handle);

    pr_debug("offset:%d, mem_handle:%d\n", offset, mem_handle);

    if (offset < 0 || offset > BM_SHARED_TOTAL_MEM_SIZE) {
        pr_err("ERROR invalid offset:0x%x\n", offset);
        return;
    }

    cur_rp = offset / sizeof(u32);

    next_rp = cur_rp + offsetof(bm_kapi_header_t, api_id) / sizeof(u32);
    api_id = bm84x6_shmem_reg_read(bmdi, next_rp);
    next_rp = cur_rp + offsetof(bm_kapi_header_t, duration) / sizeof(u32);
    api_duration = bm84x6_shmem_reg_read(bmdi, next_rp);
    next_rp = cur_rp + offsetof(bm_kapi_header_t, result) / sizeof(u32);
    api_result = bm84x6_shmem_reg_read(bmdi, next_rp);
    if (api_id == 0x90000003) {
        next_rp = cur_rp + sizeof(bm_kapi_header_t) / sizeof(u32);
        func_id = bm84x6_shmem_reg_read(bmdi, next_rp);
    }
    if (api_result != 0) {
        pr_err("[%s: %d] error: bm-sophon%d, api_id=0x%x, api_result=0x%x\n",
               __func__, __LINE__, bmdi->dev_index, api_id, api_result);
        print_api_rusult_err_log(api_id, api_result);
        if (api_id == 0x90000003)
            pr_err("func id: 0x%x\n", func_id);
    }
    if (ti) {
        ti->profile.tpu_process_time += api_duration;
        bmdi->profile.tpu_process_time += api_duration;
        ti->profile.completed_api_counter++;
        bmdi->profile.completed_api_counter++;
    }
}



static void bmdev_handle_hwq_tasklet_func(struct bm_device_info *bmdi,
                                          bm_hw_q_info * pst_hw_q_info)
{
    struct api_fifo_entry entry;
    struct list_head *handle_list = NULL;
    unsigned long flags;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;
    int free_count, used_count;
    static unsigned int processing_count = 0;
    int hwq_idx;
    int i;
    u64 current_done_count = 0;
    u32 process_done_count = 0;

    if (!pst_hw_q_info) {
        pr_err("Invalid pst_hw_q_info in bottom half\n");
        return;
    }

    hwq_idx = pst_hw_q_info->hwq_idx;
    processing_count++;
    pr_debug("tasklet[%d]started, count: %u\n", hwq_idx, processing_count);

    while (current_done_count != tsh_get_task_done_count(bmdi, hwq_idx)) {
        current_done_count = tsh_get_task_done_count(bmdi, hwq_idx);
        process_done_count =
            current_done_count - pst_hw_q_info->last_done_count;
        free_count = bmdev_kernel_kfifo_free_count(bmdi, hwq_idx);
        used_count = pstBmKernelInfo->max_depth - free_count;
        if (free_count < 0 || process_done_count > used_count) {
            pr_err("HWQ %d: Failed to get free count %d\n",
                   hwq_idx, free_count);
            return;
        }
        for (i = 0; i < process_done_count; i++) {
            if (bmdev_pop_kernel_kfifo(bmdi, hwq_idx, &entry) == 0) {
                if (DUMP_84X6_FIFO) {
                    bmdev_shmem_phys_to_file(pstBmKernelInfo->bmdi,
                                             entry.api_data,
                                             pst_hw_q_info->filename);
                }
                spin_lock_irqsave(&pst_hw_q_info->hwq_spinlock, flags);;
                list_for_each(handle_list, &bmdi->handle_list) {
                    if (container_of(handle_list, struct bm_handle_info, list)
                        == entry.h_info)
                        break;
                }
                if (handle_list == &bmdi->handle_list) {
                    entry.thd_info = NULL;
                    entry.h_info = NULL;
                    pr_err("tasklet process error happen.\n");
                }
                pr_debug("idx:%d api_id:%x thd_api_seq:%llu\n", hwq_idx,
                         entry.api_id, entry.thd_api_seq[0]);
                if (entry.thd_info && entry.h_info) {
                    entry.thd_info->cpl_api_seq[0] = entry.thd_api_seq[0];
                    entry.h_info->h_cpl_api_seq[0] =
                        entry.thd_info->cpl_api_seq[0];
                }
                bmdev_post_api_process(bmdi, entry);
                if (entry.h_info)
                    wake_up_all(&entry.h_info->h_msg_done);
                if (entry.thd_info) {
                    complete(&entry.thd_info->msg_done);
                    pr_debug("pid %d complete api %lld\n",
                             entry.thd_info->user_pid,
                             entry.thd_api_seq[hwq_idx]);
                }
                bmdev_shared_mem_free(bmdi, entry.api_data);

                spin_unlock_irqrestore(&pst_hw_q_info->hwq_spinlock, flags);;
            } else {
                pr_err("tasklet: Failed to pop data\n");
            }
            //always wakeup if irq comes.
            bmdev_wakeup_waiting_task(bmdi, hwq_idx);
        }
        pst_hw_q_info->last_done_count = current_done_count;
    }
    pr_debug("tasklet completed\n");
}

static void bmdev_tsh_irq_tasklet_handle(unsigned long data)
{

    struct bm_device_info *bmdi = (struct bm_device_info *)data;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;
    bm_hw_q_info *pst_hw_q_info;
    int i;
    u64 current_done_count[NUM_HWQS];
    static u64 previous_done_count[NUM_HWQS] = { 0 };
    bool irq_handled = false;

    if (!pstBmKernelInfo) {
        pr_err("Invalid bmdi in tasklet\n");
        return;
    }

    pr_debug("TSH tasklet started. Polling all %d HWQs.\n", NUM_HWQS);

    for (i = 0; i < NUM_HWQS; i++) {
        current_done_count[i] = tsh_get_task_done_count(bmdi, i);

        if (current_done_count[i] > previous_done_count[i]) {
            pr_debug("HWQ[%d] task completed. Count: %llu -> %llu\n",
                     i, previous_done_count[i], current_done_count[i]);

            previous_done_count[i] = current_done_count[i];
            pst_hw_q_info = &pstBmKernelInfo->hw_q_info[i];
            bmdev_handle_hwq_tasklet_func(bmdi, pst_hw_q_info);
            irq_handled = true;
        }
    }

    if (!irq_handled) {
        pr_debug("TSH tasklet: No new task completion found on any HWQ.\n");
    }

    pr_debug("TSH tasklet completed.\n");
}

irqreturn_t bmdev_tsh_irq_handler(int irq, void *data)
{
    struct bm_device_info *bmdi = (struct bm_device_info *)data;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;
    pr_debug("TSH IRQ %d received. Scheduling bottom half for all HWQs.\n",
             irq);
    pstBmKernelInfo->irq_handle_cnt++;
    tsh_clear_irq(bmdi);
    mod_timer(&pstBmKernelInfo->tsh_irq_fallback_timer, jiffies + 5 * HZ);
    tasklet_schedule(&(pstBmKernelInfo->tsh_irq_tasklet));
    return IRQ_HANDLED;
}

#ifndef SOC_MODE
static void bm84x6_tsh_irq_adapter(struct bm_device_info *bmdi)
{
    bmdev_tsh_irq_handler(0, bmdi);
}
#endif

int bm84x6_task_init(struct bm_device_info *bmdi, en_tsh_mode mode)
{
    int i;
    int j;
    int ret;
    bm_hw_q_info *pst_hw_q_info;
    bm_kernel_info *pstBmKernelInfo;

    pstBmKernelInfo = kzalloc(sizeof(bm_kernel_info), GFP_KERNEL);
    if (!pstBmKernelInfo) {
        pr_err("Failed to allocate memory for bm_kernel_info\n");
        return -ENOMEM;
    }

    if (TSH_BYPASS_MODE != mode)
        pstBmKernelInfo->max_depth = 16;
    else
        pstBmKernelInfo->max_depth = 64;

    pstBmKernelInfo->bmdi = bmdi;
    bmdi->pstBmKernelInfo = pstBmKernelInfo;

    for (i = 0; i < NUM_HWQS; i++) {
        pst_hw_q_info = &pstBmKernelInfo->hw_q_info[i];
        pst_hw_q_info->hwq_idx = i;

        ret = kfifo_alloc(&pst_hw_q_info->api_fifo,
                          sizeof(struct api_fifo_entry) *
                          pstBmKernelInfo->max_depth, GFP_KERNEL);
        if (ret) {
            pr_err("Failed to allocate kfifo for HWQ %d, error: %d\n", i, ret);
            goto err_cleanup_kfifo;
        }

        spin_lock_init(&pst_hw_q_info->hwq_spinlock);
        pst_hw_q_info->free_slots = pstBmKernelInfo->max_depth;
        snprintf(pst_hw_q_info->filename, BMDEV_84X6_DUMP_FILENAME_LEN,
                 "/data/irq_hwq%d_%ld.txt", i, jiffies);
        init_completion(&pst_hw_q_info->msg_done);
        mutex_init(&pst_hw_q_info->api_fifo_mutex);
    }

    tasklet_init(&pstBmKernelInfo->tsh_irq_tasklet,
                 bmdev_tsh_irq_tasklet_handle, (unsigned long)bmdi);
    timer_setup(&pstBmKernelInfo->tsh_irq_fallback_timer,
                bmdev_tsh_irq_fallback_timer_fn, 0);

#ifndef SOC_MODE
    /* PCIe: single MSI is dispatched by bmdrv_do_irq() on intc bit 83.
     * The intc handler signature is void(bmdi*), SoC's is irqreturn_t(irq,data). */
    bmdrv_submodule_request_irq(bmdi, BM84X6_TSH_IRQ_ID,
                                bm84x6_tsh_irq_adapter);
#endif

    return 0;

  err_cleanup_kfifo:
    for (j = 0; j < i; j++) {
        pst_hw_q_info = &pstBmKernelInfo->hw_q_info[j];
        if (kfifo_initialized(&pst_hw_q_info->api_fifo)) {
            kfifo_free(&pst_hw_q_info->api_fifo);
        }
        mutex_destroy(&pst_hw_q_info->api_fifo_mutex);
    }
    kfree(pstBmKernelInfo);
    bmdi->pstBmKernelInfo = NULL;

    return ret;
}

int bm84x6_task_deinit(struct bm_device_info *bmdi)
{
    int i;
    bm_hw_q_info *pst_hw_q_info;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;

    if (!pstBmKernelInfo) {
        pr_debug("Kernel info pointer is NULL, already deinitialized\n");
        return 0;
    }

    if (pstBmKernelInfo->tsh_irq_tasklet.func) {
        tasklet_kill(&pstBmKernelInfo->tsh_irq_tasklet);
        tasklet_disable(&pstBmKernelInfo->tsh_irq_tasklet);
    }

#ifndef SOC_MODE
    bmdrv_submodule_free_irq(bmdi, BM84X6_TSH_IRQ_ID);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    timer_delete_sync(&pstBmKernelInfo->tsh_irq_fallback_timer);
#else
    del_timer_sync(&pstBmKernelInfo->tsh_irq_fallback_timer);
#endif

    for (i = 0; i < NUM_HWQS; i++) {
        pst_hw_q_info = &pstBmKernelInfo->hw_q_info[i];
        pr_debug("Deinitializing HWQ %d\n", i);

        if (kfifo_initialized(&pst_hw_q_info->api_fifo)) {
            kfifo_free(&pst_hw_q_info->api_fifo);
        }

        mutex_destroy(&pst_hw_q_info->api_fifo_mutex);

        pst_hw_q_info->hwq_idx = -1;
        pst_hw_q_info->free_slots = 0;
        pst_hw_q_info->hwq_load_counters = 0;
        pst_hw_q_info->pushed_cnt = 0;
        memset(pst_hw_q_info->filename, 0, BMDEV_84X6_DUMP_FILENAME_LEN);
    }

    kfree(pstBmKernelInfo);
    bmdi->pstBmKernelInfo = NULL;

    pr_debug("bm84x6_task_deinit: All HWQs deinitialized successfully\n");
    return 0;
}


int bmdev_kernel_kfifo_free_count(struct bm_device_info *bmdi, int idx)
{
    int ret = 0;
    bm_hw_q_info *pst_hw_q_info;
    unsigned long flags;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;

    if (idx < 0 || idx >= NUM_HWQS) {
        pr_err("<%s,%d> invalid idx:%d\n", __FUNCTION__, __LINE__, idx);
        return -1;
    }
    if (!pstBmKernelInfo) {
        pr_err("<%s,%d> NULL pstBmKernelInfo (bm84x6_task_init not called?)\n",
               __FUNCTION__, __LINE__);
        return -3;
    }
    pst_hw_q_info = &pstBmKernelInfo->hw_q_info[idx];
    if (NULL == pst_hw_q_info)
        return -2;
    spin_lock_irqsave(&pst_hw_q_info->hwq_spinlock, flags);
    ret = pst_hw_q_info->free_slots;
    spin_unlock_irqrestore(&pst_hw_q_info->hwq_spinlock, flags);

    return ret;
}

int bmdev_push_kernel_kfifo(struct bm_device_info *bmdi, int idx,
                            struct api_fifo_entry *api_entry)
{
    int fifo_avail;
    unsigned long flags;
    int ret = 0;
    bm_hw_q_info *pst_hw_q_info;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;

    if (idx < 0 || idx >= NUM_HWQS) {
        pr_err("<%s,%d> invalid idx:%d\n", __FUNCTION__, __LINE__, idx);
        return -1;
    }
    pst_hw_q_info = &pstBmKernelInfo->hw_q_info[idx];
    if (NULL == pst_hw_q_info)
        return -2;
    spin_lock_irqsave(&pst_hw_q_info->hwq_spinlock, flags);
    fifo_avail = kfifo_avail(&pst_hw_q_info->api_fifo);
    if (fifo_avail >= API_ENTRY_SIZE) {
        kfifo_in(&pst_hw_q_info->api_fifo, api_entry, API_ENTRY_SIZE);
    } else {
        pr_err("<%s,%d> push fail.\n", __FUNCTION__, __LINE__);
        spin_unlock_irqrestore(&pst_hw_q_info->hwq_spinlock, flags);
        return -3;
    }
    pst_hw_q_info->pushed_cnt++;
    if (pst_hw_q_info->free_slots > 0)
        pst_hw_q_info->free_slots--;
    spin_unlock_irqrestore(&pst_hw_q_info->hwq_spinlock, flags);

    return ret;
}

int bmdev_pop_kernel_kfifo(struct bm_device_info *bmdi, int idx,
                           struct api_fifo_entry *api_entry)
{
    bm_hw_q_info *pst_hw_q_info;
    int count = 0;
    unsigned long flags;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;

    if (idx >= NUM_HWQS || idx < 0) {
        pr_err("<%s,%d> invalid idx:%d\n", __FUNCTION__, __LINE__, idx);
        return -1;
    }
    pst_hw_q_info = &pstBmKernelInfo->hw_q_info[idx];
    if (NULL == pst_hw_q_info)
        return -2;
    spin_lock_irqsave(&pst_hw_q_info->hwq_spinlock, flags);
    count = kfifo_out(&pst_hw_q_info->api_fifo, api_entry, API_ENTRY_SIZE);
    if (count < API_ENTRY_SIZE) {
        pr_err("<%s,%d> pop fail  size %d.\n", __FUNCTION__, __LINE__, count);
        spin_unlock_irqrestore(&pst_hw_q_info->hwq_spinlock, flags);
        return -3;
    }
    pst_hw_q_info->free_slots++;
    spin_unlock_irqrestore(&pst_hw_q_info->hwq_spinlock, flags);

    return 0;
}


int bmdev_wait_free_slots(struct bm_device_info *bmdi, int idx, int timeout)
{
    int ret_wait = 1;
    bm_hw_q_info *pst_hw_q_info;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;
    int free_queue_count = 0;
    //en_tsh_mode mode = tsh_getmode();

    if (idx < 0 || idx >= NUM_HWQS) {
        pr_err("<%s,%d> invalid idx:%d\n", __FUNCTION__, __LINE__, idx);
        return -1;
    }

    if (timeout <= 0) {
        timeout = 300 * 1000;
    }
    pst_hw_q_info = &pstBmKernelInfo->hw_q_info[idx];
    if (NULL == pst_hw_q_info)
        return -2;

    free_queue_count = bmdev_kernel_kfifo_free_count(bmdi, idx);
    while (free_queue_count == 0 && (ret_wait != 0)) {
        pr_debug("WAIT: Going to sleep on completion for HWQ %d\n", idx);
        ret_wait =
            wait_for_completion_timeout(&pst_hw_q_info->msg_done,
                                        msecs_to_jiffies(timeout));
        pr_debug("WAIT: Woken up or timeout, ret_wait=%d\n", ret_wait);
        free_queue_count = bmdev_kernel_kfifo_free_count(bmdi, idx);
        pr_debug("WAIT: After wakeup, free_slots=%d\n", free_queue_count);

    }
    if (free_queue_count > 0)
        return 0;
    pr_err("<%s,%d> busy timeout:%d\n", __FUNCTION__, __LINE__, timeout);
    return -EBUSY;
}

int bmdev_wakeup_waiting_task(struct bm_device_info *bmdi, int idx)
{
    bm_hw_q_info *pst_hw_q_info;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;

    if (idx < 0 || idx >= NUM_HWQS) {
        pr_err("<%s,%d> invalid idx:%d\n", __FUNCTION__, __LINE__, idx);
        return -1;
    }
    pst_hw_q_info = &pstBmKernelInfo->hw_q_info[idx];
    if (NULL == pst_hw_q_info)
        return -2;
    pr_debug("WAKE: About to call complete for HWQ %d\n", idx);
    complete(&pst_hw_q_info->msg_done);
    pr_debug("WAKE: complete called for HWQ %d\n", idx);

    return 0;
}

int bmdev_thread_bind_hwq(struct bm_device_info *bmdi,
                          struct bm_thread_info *thread_info, int idx)
{
    int target_idx = -1;
    int try_count = 0;
    static int s_last_bound_hwq_idx = -1;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;

    if (!thread_info) {
        pr_err("Error: NULL thread_info pointer\n");
        return -EINVAL;
    }
    if (!pstBmKernelInfo) {
        pr_err("Error: NULL pstBmKernelInfo pointer (bm84x6_task_init not called?)\n");
        return -EINVAL;
    }

    mutex_lock(&hwq_global_lock);
    if (idx < 0 || idx >= NUM_HWQS) {
        do {
            s_last_bound_hwq_idx = (s_last_bound_hwq_idx + 1) % NUM_HWQS;
            try_count++;
            target_idx = s_last_bound_hwq_idx;
        } while (try_count <= NUM_HWQS);
    } else
        target_idx = idx;

    if (target_idx != -1) {
        pstBmKernelInfo->hw_q_info[target_idx].hwq_load_counters++;
        thread_info->q_idx = target_idx;
        pr_debug("Thread bound to HWQ %d (Round Robin). Current load: %d\n",
                 target_idx,
                 pstBmKernelInfo->hw_q_info[target_idx].hwq_load_counters);
    } else {
        pr_err("Error: Failed to find a suitable HWQ\n");
        mutex_unlock(&hwq_global_lock);
        return -ENOSPC;
    }

    mutex_unlock(&hwq_global_lock);
    return 0;
}


int bmdev_get_thread_bind_hwq_idx(struct bm_thread_info *thread_info)
{
    if (!thread_info) {
        pr_err("Error: NULL thread_info pointer\n");
        return -EINVAL;
    }
    return thread_info->q_idx;
}

int bmdev_thread_unbind_hwq(struct bm_device_info *bmdi,
                            struct bm_thread_info *thread_info)
{
    int idx = 0;
    bm_kernel_info *pstBmKernelInfo = bmdi->pstBmKernelInfo;
    if (!thread_info) {
        pr_err("Error: NULL thread_info pointer\n");
        return -EINVAL;
    }

    if (thread_info->q_idx < 0 || thread_info->q_idx >= NUM_HWQS) {
        return 0;
    }
    idx = thread_info->q_idx;
    mutex_lock(&hwq_global_lock);
    if (pstBmKernelInfo->hw_q_info[idx].hwq_load_counters > 0) {
        pstBmKernelInfo->hw_q_info[idx].hwq_load_counters--;
        pr_debug("Thread unbound from HWQ %d. Current load: %d\n",
                 thread_info->q_idx,
                 pstBmKernelInfo->hw_q_info[idx].hwq_load_counters);
    }
    thread_info->q_idx = -1;
    mutex_unlock(&hwq_global_lock);

    return 0;
}


int test_wakeup_consumer_thread(void *data)
{
    struct bm_device_info *bmdi = (struct bm_device_info *)data;

    pr_info("Wakeup thread started for all HWQs, bmdi: %p\n", bmdi);

    while (!kthread_should_stop()) {
        if (!bmdi->pstBmKernelInfo) {
            pr_warn("Kernel info not initialized, skipping IRQ handling\n");
            msleep(1000);
            continue;
        }

        bmdev_tsh_irq_handler(0, bmdi);
        msleep(5000);
    }

    pr_info("Wakeup thread stopped for all HWQs\n");
    return 0;
}
