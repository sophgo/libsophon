#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include "jpuconfig.h"

typedef struct jpuvdrv_core_list_t {
    int id;
    bool is_used;
    struct list_head list;
} jpuvdrv_core_list_t;

static spinlock_t jpeg_spinlock[MAX_NUM_SOPHON_SOC];
static struct mutex jpuvdrv_core_list_lock[MAX_NUM_SOPHON_SOC];
static wait_queue_head_t jpuvdrv_core_wait_queue[MAX_NUM_SOPHON_SOC];
static struct list_head jpuvdrv_core_resource_list_head[MAX_NUM_SOPHON_SOC];
static bool isCoreIdle = false;

int jpu_core_request_resource(int soc_idx, int timeout) {
    jpuvdrv_core_list_t *res;
    unsigned long flags;
    int id = -1;
    unsigned long long elapse, cur;
    struct timespec64 ts;

    ktime_get_ts64(&ts);
    elapse = ts.tv_sec * 1000 + ts.tv_nsec/1000000;

    while (id == -1) {
        spin_lock_irqsave(&jpeg_spinlock[soc_idx], flags);
        list_for_each_entry(res, &jpuvdrv_core_resource_list_head[soc_idx], list) {
            if (!res->is_used) {
                res->is_used = true;
                id = res->id;
                break;
            }
        }
        isCoreIdle = false;
        spin_unlock_irqrestore(&jpeg_spinlock[soc_idx], flags);

        if (id == -1) {
            if (timeout > 0 )
                wait_event_idle_exclusive_timeout(jpuvdrv_core_wait_queue[soc_idx], isCoreIdle, msecs_to_jiffies(timeout));
            else
                wait_event(jpuvdrv_core_wait_queue[soc_idx], isCoreIdle);
        }

        ktime_get_ts64(&ts);
        cur = ts.tv_sec * 1000 + ts.tv_nsec/1000000;

        if (timeout > 0 && (cur - elapse) > timeout) {
            return -1;
        }
    }

    return id;
}

int jpu_core_release_resource(int soc_idx, int id) {
    jpuvdrv_core_list_t *res;
    unsigned long		flags;
    int ret = -1;

    spin_lock_irqsave(&jpeg_spinlock[soc_idx], flags);
    list_for_each_entry(res, &jpuvdrv_core_resource_list_head[soc_idx], list) {
        if (res->id == id) {
            res->is_used = false;
            ret = 0;
            isCoreIdle = true;
            wake_up(&jpuvdrv_core_wait_queue[soc_idx]);
            break;
        }
    }
    spin_unlock_irqrestore(&jpeg_spinlock[soc_idx], flags);

    return ret;
}

int jpu_core_init_resources(void) {
    jpuvdrv_core_list_t *res;
    int i = 0, j = 0;

    for (i=0; i<MAX_NUM_SOPHON_SOC; i++) {
        spin_lock_init(&jpeg_spinlock[i]);
        mutex_init(&jpuvdrv_core_list_lock[i]);
        INIT_LIST_HEAD(&jpuvdrv_core_resource_list_head[i]);
        init_waitqueue_head(&jpuvdrv_core_wait_queue[i]);

        for (; j < (i+1)*MAX_NUM_JPU_CORE_CHIP; j++) {
            res = kzalloc(sizeof(*res), GFP_KERNEL);
            if (!res) {
                printk(KERN_ERR "Failed to allocate memory for resource\n");
                return -ENOMEM;
            }

            res->id = j;
            res->is_used = false;
            INIT_LIST_HEAD(&res->list);

            list_add_tail(&res->list, &jpuvdrv_core_resource_list_head[i]);
        }
    }
    return 0;
}

void jpu_core_cleanup_resources(void) {
    jpuvdrv_core_list_t *res, *tmp;
    int i;

    for (i=0; i<MAX_NUM_SOPHON_SOC; i++) {
        list_for_each_entry_safe(res, tmp, &jpuvdrv_core_resource_list_head[i], list) {
            list_del(&res->list);
            kfree(res);
        }
    }
}

