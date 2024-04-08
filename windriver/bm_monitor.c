
#include "bm_common.h"
#include "bm_attr.h"
#include "i2c.h"
#include "bm_monitor.h"
#include "bm_monitor.tmh"

void bm_monitor_thread(PVOID date) {
	int ret = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)date;
	int count =0;
	int is_setspeed =0;

	ret = bm_arm9fw_log_init(bmdi);
    if (ret) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm_arm9fw_log_init fail\n");
        return;
    }

	while (bmdi->monitor_thread_info.isstop != 1) {//every loop is about (10 + 10 + 10)ms
		//bm_dump_arm9fw_log(bmdi, count);
		bmdrv_fetch_attr(bmdi, count, is_setspeed);
		bmdrv_fetch_attr_board_power(bmdi, count);
		count++;
		if(count == 30){//30*18ms
			count = 0;
			is_setspeed = (is_setspeed == 0 ? 1: 0);
		}
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "minitor thread bm_monitor-%d stop\n", bmdi->dev_index);
    PsTerminateSystemThread(STATUS_SUCCESS);
	return;
}

int bm_monitor_thread_init(struct bm_device_info *bmdi)
{
	void *data = bmdi;
	char thread_name[20] = "";
    NTSTATUS          status;
    HANDLE            handle;

	_snprintf(thread_name, 20, "bm_monitor-%d", bmdi->dev_index);
    bmdi->monitor_thread_info.isstop = 0;
    if (bmdi->monitor_thread_info.ThreadObj == NULL) {
		status = PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, NULL, NULL, NULL, bm_monitor_thread, data);
        if (NT_SUCCESS(status)) {
            status = ObReferenceObjectByHandle(handle,
                                          THREAD_ALL_ACCESS,
                                          NULL,
                                          KernelMode,
                                          &bmdi->monitor_thread_info.ThreadObj,
                                          NULL);
            if (!NT_SUCCESS(status)) {
                bmdi->monitor_thread_info.ThreadObj = NULL;
            }
        }

		if (bmdi->monitor_thread_info.ThreadObj == NULL) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "creat monitor thread %s fail\n", thread_name);
			return -1;
		}
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "creat monitor thread %s done\n", thread_name);
	}
	return 0;
}

int bm_monitor_thread_deinit(struct bm_device_info *bmdi)
{
	if (bmdi->monitor_thread_info.ThreadObj != NULL) {
        bmdi->monitor_thread_info.isstop = 1;
        KeWaitForSingleObject(bmdi->monitor_thread_info.ThreadObj, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(bmdi->monitor_thread_info.ThreadObj);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "minitor thread bm_monitor-%d deinit done\n", bmdi->dev_index);
        bmdi->monitor_thread_info.ThreadObj = NULL;
	}
	return 0;
}


