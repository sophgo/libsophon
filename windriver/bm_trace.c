#include "bm_common.h"

#include "bm_trace.tmh"

int bmdrv_init_trace_pool(struct bm_device_info *bmdi)
{
	struct bm_trace_info *trace_info = &bmdi->trace_info;
    NTSTATUS              status;

	status = WdfMemoryCreateFromLookaside(bmdi->Lookaside, &trace_info->trace_memHandle);
    if(!NT_SUCCESS(status)){
         TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "Failed to get lookaside buffer\n");
         return -1;
     }

	return status;
}

void bmdrv_destroy_trace_pool(struct bm_device_info *bmdi)
{
	struct bm_trace_info *trace_info = &bmdi->trace_info;

	if (trace_info->trace_memHandle)
        WdfObjectDelete(trace_info->trace_memHandle);
}

int bmdev_trace_enable(struct bm_device_info *bmdi, _In_ WDFREQUEST Request) {
    struct bm_thread_info *ti = NULL;
    HANDLE                 api_pid;
	struct bm_handle_info *h_info;
    int                    ret      = 0;
    WDFFILEOBJECT          fileObject = WdfRequestGetFileObject(Request);

	if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "trace_enable: file list is not found!\n");
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
	}

	api_pid = PsGetCurrentThreadId();
    ti      = bmdrv_find_thread_info(h_info, (pid_t)api_pid);

	if (!ti) {
		ret = -1;
	} else {
		WdfSpinLockAcquire(ti->trace_spinlock);
		ti->trace_enable = 1;
		WdfSpinLockRelease(ti->trace_spinlock);
		ret = 0;
	}
    if (ret) {
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "%!FUNC! bmdev_trace_enable failed\n");
        return -1;
    } else {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
        return 0;
    }
}

int bmdev_trace_disable(struct bm_device_info *bmdi, _In_ WDFREQUEST Request) {
	struct bm_thread_info *ti = NULL;
    HANDLE                 api_pid;
	struct bm_handle_info *h_info;
    int                    ret      = 0;
    WDFFILEOBJECT          fileObject = WdfRequestGetFileObject(Request);

	if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "trace_disable: file list is not found!\n");
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return -1;
	}
    api_pid = PsGetCurrentThreadId();
    ti      = bmdrv_find_thread_info(h_info, (pid_t)api_pid);

	if (!ti) {
		ret = -1;
	} else {
		WdfSpinLockAcquire(ti->trace_spinlock);
		ti->trace_enable = 0;
		WdfSpinLockRelease(ti->trace_spinlock);
		ret = 0;
	}
    if (ret) {
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "%!FUNC! bmdev_trace_enable failed\n");
        return -1;
    } else {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
        return 0;
    }
}

int bmdev_traceitem_number(struct bm_device_info *bmdi,
                           _In_ WDFREQUEST        Request,
                           _In_ size_t            OutputBufferLength,
                           _In_ size_t            InputBufferLength) {
    struct bm_thread_info *ti = NULL;
    HANDLE                 api_pid;
    int                    ret = 0;
    struct bm_handle_info *h_info;
    WDFFILEOBJECT          fileObject = WdfRequestGetFileObject(Request);

	UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bmdrv: file list is not found!\n");
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        return -1;
    }
    api_pid = PsGetCurrentThreadId();
    ti      = bmdrv_find_thread_info(h_info, (pid_t)api_pid);

    if (!ti) {
        return -1;
    } else {
		size_t bufSize;
        PVOID  DataBuffer;
        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(u64), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            ret = -1;
        }
        WdfSpinLockAcquire(ti->trace_spinlock);
        RtlCopyMemory(DataBuffer, &ti->trace_item_num, bufSize);
        WdfSpinLockRelease(ti->trace_spinlock);
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u64));
        ret = 0;
        return ret;
    }
}

int bmdev_trace_dump_one(struct bm_device_info *bmdi, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength)
{
    struct bm_thread_info *ti = NULL;
    HANDLE                 api_pid;
    LIST_ENTRY *oldest = NULL;
	struct bm_trace_item *ptitem = NULL;
	struct bm_handle_info *h_info;

	UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);

	if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "dump one: file list is not found!\n");
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return -1;
	}

    api_pid = PsGetCurrentThreadId();
    ti      = bmdrv_find_thread_info(h_info, (pid_t)api_pid);

	if (!ti) {
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        return -1;
    }

	WdfSpinLockAcquire(ti->trace_spinlock);
    if (!IsListEmpty(&ti->trace_list)) {
        oldest = RemoveHeadList(&ti->trace_list);
        ptitem = CONTAINING_RECORD(oldest, struct bm_trace_item, node);
        size_t bufSize;
        PVOID  DataBuffer;

        NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
            Request, sizeof(struct bm_trace_item_data), &DataBuffer, &bufSize);
        if (!NT_SUCCESS(Status)) {
            WdfRequestCompleteWithInformation(Request, Status, 0);
            return -1;
        }
        RtlCopyMemory(DataBuffer, &ptitem->payload, bufSize);
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(struct bm_trace_item_data));
        MmFreeContiguousMemory(ptitem);
        ti->trace_item_num--;
    }
	WdfSpinLockRelease(ti->trace_spinlock);
	return 0;
}

int bmdev_trace_dump_all(struct bm_device_info *bmdi, _In_ WDFREQUEST Request) {
	//struct bm_thread_info *ti;
	//pid_t api_pid;
	int ret = -1;
	//int i = 0;
	//struct list_head *oldest = NULL;
	//struct bm_trace_item *ptitem = NULL;
	//struct bm_handle_info *h_info;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(bmdi);
	//if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
	//	TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "bmdrv: file list is not found!\n");
	//	return -1;
	//}

	//api_pid = current->pid;
	//ti = bmdrv_find_thread_info(h_info, api_pid);

	//if (!ti)
	//	return -1;

	////mutex_lock(&ti->trace_spinlock);
	//while (!list_empty(&ti->trace_list)) {
	//	oldest = ti->trace_list.next;
	//	list_del(oldest);
	//	ptitem = container_of(oldest, struct bm_trace_item, node);
	//	ret = copy_to_user((unsigned long  *)(arg + i*sizeof(struct bm_trace_item_data)),
	//			&ptitem->payload, sizeof(struct bm_trace_item_data));
	//	mempool_free(ptitem, bmdi->trace_info.trace_mempool);
	//	i++;
	//}
	////mutex_unlock(&ti->trace_spinlock);
	return ret;
}

int bmdev_enable_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor)
{
	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) {
		if (perf_monitor->monitor_id == PERF_MONITOR_TPU) {
			bm1684_enable_tpu_perf_monitor(bmdi, perf_monitor);
		} else if (perf_monitor->monitor_id == PERF_MONITOR_GDMA) {
			bm1684_enable_gdma_perf_monitor(bmdi, perf_monitor);
		} else {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"enable perf monitor bad perf monitor id 0x%x\n",
					perf_monitor->monitor_id);
			return -1;
		}
	} else {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bmdev_enable_perf_monitor chip id = 0x%x not support\n",
				bmdi->cinfo.chip_id);
		return -1;
	}
	return 0;
}

int bmdev_disable_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor)
{
	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) {
		if (perf_monitor->monitor_id == PERF_MONITOR_TPU) {
			bm1684_disable_tpu_perf_monitor(bmdi);
		} else if (perf_monitor->monitor_id == PERF_MONITOR_GDMA) {
			bm1684_disable_gdma_perf_monitor(bmdi);
		} else {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"disable perf monitor bad perf monitor id 0x%x\n",
					perf_monitor->monitor_id);
			return -1;
		}
	} else {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bmdev_disable_perf_monitor chip id = 0x%x not support\n",
				bmdi->cinfo.chip_id);
		return -1;
	}
	return 0;
}
