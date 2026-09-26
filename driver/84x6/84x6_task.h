#ifndef __84X6_TASK_H__
#define __84X6_TASK_H__
#include <linux/atomic.h>
#include <linux/timer.h>
#include "84x6_tsh.h"
#include "bm_common.h"
#include "../bm_thread.h"
#include "bm_api.h"

#define NUM_HWQS 8
#define DUMP_84X6_FIFO 0
#define BMDEV_84X6_DUMP_FILENAME_LEN 64

typedef struct bm_hw_q_info_t {
    int hwq_idx;
    int hwq_load_counters;
    int free_slots;
    u32 pushed_cnt;
    u64 last_done_count;
    spinlock_t hwq_spinlock;
    struct mutex api_fifo_mutex;
    struct completion msg_done;
    struct kfifo api_fifo;
    char filename[BMDEV_84X6_DUMP_FILENAME_LEN];
} bm_hw_q_info;


typedef struct bm_kernel_info_t {
    int max_depth;
    u32 irq_handle_cnt;
    u32 irq_fallback_cnt;
    struct tasklet_struct tsh_irq_tasklet;
    struct timer_list tsh_irq_fallback_timer;
    struct bm_device_info *bmdi;
    char filename[NUM_HWQS][BMDEV_84X6_DUMP_FILENAME_LEN];
    bm_hw_q_info hw_q_info[NUM_HWQS];
} bm_kernel_info;


int bmdev_thread_bind_hwq(struct bm_device_info *bmdi,
                          struct bm_thread_info *thread_info, int idx);
int bmdev_get_thread_bind_hwq_idx(struct bm_thread_info *thread_info);
int bmdev_thread_unbind_hwq(struct bm_device_info *bmdi,
                            struct bm_thread_info *thread_info);
int bmdev_wait_free_slots(struct bm_device_info *bmdi, int idx, int timeout);
int bmdev_wakeup_waiting_task(struct bm_device_info *bmdi, int idx);
int bmdev_push_kernel_kfifo(struct bm_device_info *bmdi, int idx,
                            struct api_fifo_entry *api_entry);
int bmdev_pop_kernel_kfifo(struct bm_device_info *bmdi, int idx,
                           struct api_fifo_entry *api_entry);
int bmdev_kernel_kfifo_free_count(struct bm_device_info *bmdi, int idx);
int bm84x6_task_init(struct bm_device_info *bmdi, en_tsh_mode mode);
int bm84x6_task_deinit(struct bm_device_info *bmdi);
irqreturn_t bmdev_tsh_irq_handler(int irq, void *data);



#endif //__84X6_TASK_H__
