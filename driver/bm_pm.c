#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/delay.h>
#include "bm_common.h"
#include "bm_msgfifo.h"
#include "bm1688_card.h"


int bm_pm_thread(void *date)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)date;
	bool msg0_empty, msg1_empty;

	msleep(2000);

	while (!kthread_should_stop()) {
		mutex_lock(&bmdi->pm_thread_info.pm_mutex);
		msg0_empty = bmdev_msgfifo_empty(bmdi, BM_MSGFIFO_CHANNEL_XPU, 0);
		msg1_empty = bmdev_msgfifo_empty(bmdi, BM_MSGFIFO_CHANNEL_XPU, 1);
		if (msg0_empty && msg1_empty) {
			if (bmdi->pm_thread_info.is_clk_enable == 1) {
				bm1688_tpu_clk_disable(bmdi);
				bm1688_gdma_clk_disable(bmdi);
				bmdi->pm_thread_info.is_clk_enable = 0;
			}
		}
		mutex_unlock(&bmdi->pm_thread_info.pm_mutex);

		msleep(5000);
	}
	return 0;
}

int bm_pm_thread_init(struct bm_device_info *bmdi)
{
	void *data = bmdi;
	char thread_name[20] = "";

	if (bmdi->pm_thread_info.is_pm_enable == 0) {
		bmdi->pm_thread_info.is_pm_enable = 1;
		bmdi->pm_thread_info.is_clk_enable = 1;
		mutex_init(&bmdi->pm_thread_info.pm_mutex);
	} else {
		pr_err("power manage already enable\n");
		return 0;
	}

	snprintf(thread_name, 20, "bm_pm-%d", bmdi->dev_index);
	if (bmdi->pm_thread_info.pm_task == NULL) {
		bmdi->pm_thread_info.pm_task = kthread_run(bm_pm_thread, data, thread_name);
		if (bmdi->pm_thread_info.pm_task == NULL) {
			pr_info("create pm thread %s fail\n", thread_name);
			return -1;
		}
		pr_info("create pm thread %s done\n", thread_name);
	}
	return 0;
}

int bm_pm_thread_deinit(struct bm_device_info *bmdi)
{
	bmdi->pm_thread_info.is_pm_enable = 0;
	kthread_stop(bmdi->pm_thread_info.pm_task);
	pr_info("pm thread bm_pm-%d deinit done\n", bmdi->dev_index);

	return 0;
}