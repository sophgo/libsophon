#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "bm_ctl.h"
#include "bm_common.h"
#include "bm_attr.h"
#include "bm_card.h"
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "bm_monitor.h"

#define SC7_PRO_HAS_VFS
#define CORE_ID0 0
#define CORE_ID1 1

int bm_monitor_thread(void *date)
{
	int ret = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)date;
	int count = 0;
	int is_setspeed = 0;

	// set_current_state(TASK_INTERRUPTIBLE);
	ret = bm_arm9fw_log_init(bmdi, CORE_ID0);
	if (ret)
		return ret;

	while (!kthread_should_stop())
	{ // every loop is about (10 + 10 + 10)ms
		bm_dump_arm9fw_log(bmdi, count);
		bmdrv_fetch_attr(bmdi, count, is_setspeed);
		bmdrv_fetch_attr_board_power(bmdi, count);

		count++;
		if (count == 18)
		{ // 30*18ms
			count = 0;
			is_setspeed = (is_setspeed == 0 ? 1 : 0);
		}
	}
	return ret;
}

int bm_monitor_thread_init(struct bm_device_info *bmdi)
{
	void *data = bmdi;
	char thread_name[20] = "";

	snprintf(thread_name, 20, "bm_monitor-%d", bmdi->dev_index);
	if (bmdi->monitor_thread_info.monitor_task == NULL)
	{
		bmdi->monitor_thread_info.monitor_task = kthread_run(bm_monitor_thread, data, thread_name);
		if (bmdi->monitor_thread_info.monitor_task == NULL)
		{
			pr_info("create monitor thread %s fail\n", thread_name);
			return -1;
		}
		pr_info("create monitor thread %s done\n", thread_name);
	}
	return 0;
}

int bm_monitor_thread_deinit(struct bm_device_info *bmdi)
{
	struct bm_arm9fw_log_mem *log_mem0 = &bmdi->monitor_thread_info.log_mem[CORE_ID0];
	struct bm_arm9fw_log_mem *log_mem1 = &bmdi->monitor_thread_info.log_mem[CORE_ID0];

	if (bmdi->monitor_thread_info.monitor_task != NULL)
	{
		bmdrv_stagemem_free(bmdi, log_mem0->host_paddr, log_mem0->host_vaddr, log_mem0->host_size);
		bmdrv_stagemem_free(bmdi, log_mem1->host_paddr, log_mem1->host_vaddr, log_mem1->host_size);
		kthread_stop(bmdi->monitor_thread_info.monitor_task);
		pr_info("monitor thread bm_monitor-%d deinit done\n", bmdi->dev_index);
		bmdi->monitor_thread_info.monitor_task = NULL;
	}
	return 0;
}
