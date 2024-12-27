#ifndef __BM_PM_H__
#define __BM_PM_H__

#include "bm_msgfifo.h"

struct bm_pm_thread_info {
	u8 is_pm_enable;	//0 disable , 1 enable
	u8 is_clk_enable;	//0 disable , 1 enable
	struct task_struct *pm_task;
	struct mutex pm_mutex;
};

int bm_pm_thread_init(struct bm_device_info *bmdi);
int bm_pm_thread_deinit(struct bm_device_info *bmdi);


#endif