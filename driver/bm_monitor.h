#ifndef __BM_MONITOR_H__
#define __BM_MONITOR_H__

#include "bm_debug.h"

struct bm_monitor_thread_info {
	struct bm_arm9fw_log_mem log_mem;
	struct task_struct *monitor_task;
};

int bm_monitor_thread_init(struct bm_device_info *bmdi);
int bm_monitor_thread_deinit(struct bm_device_info *bmdi);

#endif

