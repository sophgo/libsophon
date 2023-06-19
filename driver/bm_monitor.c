#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "bm_ctl.h"
#include "bm_common.h"
#include "bm_attr.h"
#include "bm_pcie.h"
#include "bm_card.h"
#include "i2c.h"
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "bm1684/bm1684_card.h"
#include "bm1684/bm1684_jpu.h"
#include "vpu/vpu.h"
#include "bm_monitor.h"

#define SC7_PRO_HAS_VFS

int bm_monitor_thread(void *date)
{
	int ret = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)date;
	int count =0;
	int is_setspeed =0;
#ifdef PCIE_MODE_ENABLE_CPU
	int old_state = 0;
#endif

	set_current_state(TASK_INTERRUPTIBLE);
	ret = bm_arm9fw_log_init(bmdi);
	if (ret)
		return ret;

	while (!kthread_should_stop()) {//every loop is about (10 + 10 + 10)ms
	#ifdef PCIE_MODE_ENABLE_CPU
		if (bmdi->status_reset == A53_RESET_STATUS_TRUE)
		{
			msleep(1000);
			old_state = A53_RESET_STATUS_TRUE;
			continue;
		}

		if ((bmdi->status_reset == A53_RESET_STATUS_FALSE) && (old_state == A53_RESET_STATUS_TRUE))
		{
			bmdrv_power_and_temp_i2c_init(bmdi);
			old_state = A53_RESET_STATUS_FALSE;
		}
	#endif
		bm_dump_arm9fw_log(bmdi, count);
		bmdrv_fetch_attr(bmdi, count, is_setspeed);
		bmdrv_fetch_attr_board_power(bmdi, count);
#if ((!defined SOC_MODE) && (defined SC7_PRO_HAS_VFS))
		bmdrv_volt_freq_scaling(bmdi);
#endif
		count++;
		if(count == 18){//30*18ms
			count = 0;
			is_setspeed = (is_setspeed == 0 ? 1: 0);
		}
	}
	return ret;
}

int bm_monitor_thread_init(struct bm_device_info *bmdi)
{
	void *data = bmdi;
	char thread_name[20] = "";

	snprintf(thread_name, 20, "bm_monitor-%d", bmdi->dev_index);
	if (bmdi->monitor_thread_info.monitor_task == NULL) {
		bmdi->monitor_thread_info.monitor_task = kthread_run(bm_monitor_thread, data, thread_name);
		if (bmdi->monitor_thread_info.monitor_task == NULL) {
			pr_info("creat monitor thread %s fail\n", thread_name);
			return -1;
		}
		pr_info("creat monitor thread %s done\n", thread_name);
	}
	return 0;
}

int bm_monitor_thread_deinit(struct bm_device_info *bmdi)
{
	struct bm_arm9fw_log_mem *log_mem = &bmdi->monitor_thread_info.log_mem;

	if (bmdi->monitor_thread_info.monitor_task != NULL) {
		bmdrv_stagemem_free(bmdi, log_mem->host_paddr, log_mem->host_vaddr, log_mem->host_size);
		kthread_stop(bmdi->monitor_thread_info.monitor_task);
		pr_info("minitor thread bm_monitor-%d deinit done\n", bmdi->dev_index);
		bmdi->monitor_thread_info.monitor_task = NULL;
	}
	return 0;
}


