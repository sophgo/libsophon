#include "bm_common.h"
#include "bm_gmem.tmh"

int bmdrv_get_gmem_mode(struct bm_device_info *bmdi)
{
	return (bmdi->boot_info.ddr_mode >> 16) & 0x3;
}

int bmdrv_gmem_init(struct bm_device_info *bmdi)
{
	s32 i;
    NTSTATUS             status;
    struct bm_gmem_info *gmem_info = &bmdi->gmem_info;
    gmem_info->heap_cnt            = 0;
    u64 heap_addr = 0;
    u64 heap_size = 0;
    WDF_OBJECT_ATTRIBUTES attributes;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = bmdi->WdfDevice;
    status = WdfSpinLockCreate(&attributes, &gmem_info->gmem_lock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		if (bmdrv_bm1682_parse_reserved_mem_info(bmdi))
			return -1;
		break;
	case 0x1684:
    case 0x1686:
		if (bmdrv_bm1684_parse_reserved_mem_info(bmdi))
			return -1;
		break;
	default:
		return -1;
	}

	for (i = 0; i < MAX_HEAP_CNT; i++) {
        heap_addr = gmem_info->resmem_info.npureserved_addr[i];
        heap_size = gmem_info->resmem_info.npureserved_size[i];
		if (heap_size > 0) {
            bmdrv_bgm_heap_cteate(&gmem_info->common_heap[i], heap_addr, heap_size);
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_BGM,
                        "bm-sophon%d gmem[%d]: mem base 0x%llx\n",
                        bmdi->dev_index,
                        i,
                        heap_addr);
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_BGM,
                        "bm-sophon%d gmem[%d]: total mem 0x%llx\n",
                        bmdi->dev_index,
                        i,
                        heap_size);
            gmem_info->common_heap[i].heap_id = gmem_info->heap_cnt;
            gmem_info->heap_cnt++;
		}
	}
	return 0;
}

void bmdrv_gmem_deinit(struct bm_device_info *bmdi)
{
    u32                  i;
    struct bm_gmem_info *gmem_info = &bmdi->gmem_info;

    for (i = 0; i < gmem_info->heap_cnt; i++) {
        bmdrv_bgm_heap_destroy(&bmdi->gmem_info.common_heap[i]);
    }
}

u64 bmdrv_gmem_total_size(struct bm_device_info *bmdi)
{
	u64 size = 0;
    u32 i    = 0;
    struct bm_gmem_info *gmem_info = &bmdi->gmem_info;

    for (i = 0; i < gmem_info->heap_cnt; i++) {
        size += gmem_info->common_heap[i].heap_size;
    }

	return size;
}

u64 bmdrv_gmem_vpu_total_size(struct bm_device_info *bmdi)
{
    u64 size = 0;
    u32 i    = 0;
    struct bm_gmem_info *gmem_info = &bmdi->gmem_info;

    for (i = 3; i < gmem_info->heap_cnt; i++) {
        size += gmem_info->common_heap[i].heap_size;
    }

    return size;
}

u64 bmdrv_gmem_avail_size(struct bm_device_info *bmdi)
{
	u64 avail_size = 0;
    u32                  i          = 0;
    struct bm_gmem_info *gmem_info  = &bmdi->gmem_info;

    for (i = 0; i < gmem_info->heap_cnt; i++) {
        avail_size +=
            gmem_info->common_heap[i].free_page_count * BM_GMEM_PAGE_SIZE;
    }

	return avail_size;
}

u64 bmdrv_gmem_vpu_avail_size(struct bm_device_info *bmdi)
{
    u64 avail_size = 0;
    u32 i          = 0;
    struct bm_gmem_info *gmem_info  = &bmdi->gmem_info;

    for (i = 3; i < gmem_info->heap_cnt; i++) {
        avail_size += gmem_info->common_heap[i].free_page_count * BM_GMEM_PAGE_SIZE;
    }

    return avail_size;
}

void bmdrv_print_heap_avail_size(struct bm_device_info *bmdi)
{
    u32                  i          = 0;
    struct bm_gmem_info *gmem_info  = &bmdi->gmem_info;

    for (i = 0; i < gmem_info->heap_cnt; i++) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_BGM,
                    "heap%d avail memory 0x%llx bytes\n",
                    i,
            gmem_info->common_heap[i].free_page_count * BM_GMEM_PAGE_SIZE);
    }
}

void bmdrv_heap_mem_used(struct bm_device_info *bmdi, struct bm_dev_stat *stat)
{
    u32                  i         = 0;
    struct bm_gmem_info *gmem_info = &bmdi->gmem_info;

    //WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
    for (i = 0; i < gmem_info->heap_cnt; i++) {
        stat->heap_stat[i].mem_avail =
            (u32)(gmem_info->common_heap[i].free_page_count * BM_GMEM_PAGE_SIZE/1024/1024);
        stat->heap_stat[i].mem_total =
            (u32)(gmem_info->common_heap[i].heap_size/1024/1024);
        stat->heap_stat[i].mem_used =
            (u32)(gmem_info->common_heap[i].alloc_page_count * BM_GMEM_PAGE_SIZE/1024/1024);
    }
    //WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
}

int bmdev_gmem_get_handle_info(struct bm_device_info * bmdi, WDFFILEOBJECT file,  struct bm_handle_info **f_list){
	struct bm_handle_info *h_info;
    PLIST_ENTRY            handle_list = NULL;

	WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);

    for (handle_list = bmdi->handle_list.Flink; handle_list != &bmdi->handle_list; handle_list = handle_list->Flink){//list is not empty or have finished serch
        h_info = CONTAINING_RECORD(handle_list, struct bm_handle_info, list);
        if (h_info->fileObject == file) {
            *f_list = h_info;
            WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
            return 0;
        }
    }
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
	return -1;
}

int bmdrv_gmem_ioctl_total_size(struct bm_device_info *bmdi,
                               _In_ WDFREQUEST        Request,
                               _In_ size_t            OutputBufferLength,
                               _In_ size_t            InputBufferLength) {
    u64                   total_size;
    size_t                bufSize;
    PVOID                 DataBuffer;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
        Request, sizeof(u64), &DataBuffer, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        return -1;
    }
    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
    total_size = bmdrv_gmem_total_size(bmdi);
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

    RtlCopyMemory(DataBuffer, &total_size, bufSize);
	WdfRequestCompleteWithInformation(Request, Status, sizeof(u64));

    return 0;
}

int bmdrv_gmem_ioctl_avail_size(struct bm_device_info *bmdi,
                               _In_ WDFREQUEST        Request,
                               _In_ size_t            OutputBufferLength,
                               _In_ size_t            InputBufferLength) {
    u64    avail_size;
    size_t bufSize;
    PVOID  DataBuffer;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
        Request, sizeof(u64), &DataBuffer, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        return -1;
    }
    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
    avail_size = bmdrv_gmem_avail_size(bmdi);
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

    RtlCopyMemory(DataBuffer, &avail_size, bufSize);
    WdfRequestCompleteWithInformation(Request, Status, sizeof(u64));

    return 0;
}

int bmdrv_gmem_alloc(struct bm_device_info *    bmdi,
                            struct ion_allocation_data *alloc_data) {
    int i            = 0;
    int heap_id_mask = alloc_data->heap_id_mask;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_BGM,
                "bm-sophon%d gmem alloc heap_id_mask is 0x%x\n",
                bmdi->dev_index,
                heap_id_mask);

    if ((alloc_data->len == 0) ||
        (alloc_data->len > bmdrv_gmem_avail_size(bmdi)) ||
        (heap_id_mask == 0x0) || (alloc_data->len > 0xffffffff)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM,
            "bm-sophon%d gmem alloc failed, request size is 0x%llx, heap_id_mask = 0x%x, avail memory size is 0x%llx\n",
            bmdi->dev_index,
            alloc_data->len,
            heap_id_mask,
            bmdrv_gmem_avail_size(bmdi));
        bmdrv_print_heap_avail_size(bmdi);
        return -1;
    }
    for (i = 0; i < MAX_HEAP_CNT; i++) {
        if (((0x1 << i) & heap_id_mask) == 0x0)
            continue;

        alloc_data->paddr = bmdrv_bgm_heap_alloc(&bmdi->gmem_info.common_heap[i], alloc_data->len);

        if (alloc_data->paddr != ((u64)-1)) {
            alloc_data->heap_id = i;
            TraceEvents(
                TRACE_LEVEL_INFORMATION,
                TRACE_BGM,
                "bm-sophon%d gmem alloc sucess, request size is 0x%llx, heap_id_mask is 0x%x, on heap%d, addr=0x%llx\n",
                bmdi->dev_index,
                alloc_data->len,
                heap_id_mask,
                i,
                alloc_data->paddr);
            return 0;
        }
    }

    alloc_data->paddr = BM_MEM_ADDR_NULL;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM,
        "bm-sophon%d gmem alloc failed, request size is 0x%llx, heap_id_mask is 0x%x, avail memory size is 0x%llx\n",
        bmdi->dev_index,
        alloc_data->len,
        heap_id_mask,
        bmdrv_gmem_avail_size(bmdi));

    bmdrv_print_heap_avail_size(bmdi);
    return -1;
}

int bmdrv_gmem_ioctl_alloc_mem(struct bm_device_info *bmdi,
                               _In_ WDFREQUEST        Request,
                               _In_ size_t            OutputBufferLength,
                               _In_ size_t            InputBufferLength) {
	int ret = 0;
    struct ion_allocation_data alloc_data;
	struct bm_handle_info *h_info;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
	WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);
	if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return -1;
	}
    size_t inbufSize;
    size_t outbufSize;
    PVOID  inDataBuffer;
    PVOID  outDataBuffer;

    NTSTATUS Status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct ion_allocation_data), &inDataBuffer, &inbufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        return -1;
    }

    Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(struct ion_allocation_data), &outDataBuffer, &outbufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        return -1;
    }

    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
    RtlCopyMemory(&alloc_data, inDataBuffer, inbufSize);

    gmem_buffer_pool_t *gbp = (gmem_buffer_pool_t *)BGM_P_ALLOC(sizeof(gmem_buffer_pool_t));
    if (gbp == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BGM, "bm-sophon%d bmdrv: ExAllocatePoolWithTag failed!\n", bmdi->dev_index);
        WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        return -1;
    }

    ret = bmdrv_gmem_alloc(bmdi, &alloc_data);
    if (ret) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_BGM, "bm-sophon%d bmdrv: bmdrv_gmem_ioctl_alloc_mem alloc failed!\n", bmdi->dev_index);
        WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        return -1;
    }

    gbp->gb.heap_id = alloc_data.heap_id;
    gbp->gb.size     = alloc_data.len;
    gbp->gb.phys_addr = alloc_data.paddr;
    gbp->file         = fileObject;
    InsertTailList(&h_info->list_gmem, &gbp->list);

    RtlCopyMemory(outDataBuffer, &alloc_data, outbufSize);

    h_info->gmem_used += BGM_4K_ALIGN(alloc_data.len);
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(struct ion_allocation_data));
    return 0;
}

int bmdrv_gmem_ioctl_free_mem(struct bm_device_info *bmdi,
                              _In_ WDFREQUEST        Request,
                              _In_ size_t            OutputBufferLength,
                              _In_ size_t            InputBufferLength) {
    int                        ret = 0;
    size_t                     bufSize;
    bm_device_mem_t            device_mem;
    PVOID                      inDataBuffer;
    gmem_buffer_pool_t *       pool, *n;
    struct bm_handle_info *h_info;
	WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
	if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return -1;
	}

    NTSTATUS Status = WdfRequestRetrieveInputBuffer(Request, sizeof(bm_device_mem_t), &inDataBuffer, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "%!FUNC! get input info failed\n");
        return -1;
    }

    RtlCopyMemory(&device_mem, inDataBuffer, bufSize);

    WdfSpinLockAcquire(bmdi->gmem_info.gmem_lock);
    ret = bmdrv_bgm_heap_free(&bmdi->gmem_info.common_heap[device_mem.flags.u.gmem_heapid], device_mem.u.device.device_addr);
    if (ret == 0) {
        h_info->gmem_used -= BGM_4K_ALIGN(device_mem.size);
        list_for_each_entry_safe(pool, n, &h_info->list_gmem, list, gmem_buffer_pool_t) {
            if ((fileObject == pool->file) && (device_mem.u.device.device_addr == pool->gb.phys_addr)) {
                RemoveEntryList(&pool->list);
                BGM_P_FREE(pool);
                break;
            }
        }
    }
    WdfSpinLockRelease(bmdi->gmem_info.gmem_lock);
    if (ret) {
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "%!FUNC! bmdrv_bgm_heap_free failed, addr = 0x%llx\n", device_mem.u.device.device_addr);
        return -1;
    }
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);

    return 0;
}

int bmdrv_ioctl_get_misc_info(struct bm_device_info *bmdi,
                                _In_ WDFREQUEST        Request,
                                _In_ size_t            OutputBufferLength,
                                _In_ size_t            InputBufferLength) {
    size_t bufSize;
    PVOID  DataBuffer;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
        Request, sizeof(struct bm_misc_info), &DataBuffer, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        return -1;
    }
    RtlCopyMemory(DataBuffer, &bmdi->misc_info, bufSize);
    WdfRequestCompleteWithInformation(
        Request, STATUS_SUCCESS, sizeof(struct bm_misc_info));
    return 0;
}

int bmdrv_gmem_ioctl_get_dev_stat(struct bm_device_info *bmdi,
                                _In_ WDFREQUEST        Request,
                                _In_ size_t            OutputBufferLength,
                                _In_ size_t            InputBufferLength) {
	bm_dev_stat_t stat;
	//struct bm_chip_attr *c_attr;
	PVOID  outDataBuffer;
	size_t bufSize;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
	NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
				Request,
				sizeof(bm_dev_stat_t),
				&outDataBuffer,
				&bufSize);
	if (!NT_SUCCESS(Status)) {
		WdfRequestCompleteWithInformation(Request, Status, 0);
		return -1;
	}

	//c_attr = &bmdi->c_attr;
	stat.mem_total = (u32)(bmdrv_gmem_total_size(bmdi)/1024/1024);
	stat.mem_used = stat.mem_total - (u32)(bmdrv_gmem_avail_size(bmdi)/1024/1024);
	//stat.tpu_util = c_attr->bm_get_npu_util(bmdi);
	stat.tpu_util = 0x1;
	stat.heap_num = bmdi->gmem_info.heap_cnt;
	bmdrv_heap_mem_used(bmdi, &stat);

	RtlCopyMemory(outDataBuffer, &stat, bufSize);

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(bm_dev_stat_t));
	return 0;
}

int bmdrv_gmem_ioctl_get_heap_num(struct bm_device_info *bmdi,
                                _In_ WDFREQUEST        Request,
                                _In_ size_t            OutputBufferLength,
                                _In_ size_t            InputBufferLength) {
	PVOID  outDataBuffer;
	size_t bufSize;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
	NTSTATUS Status = WdfRequestRetrieveOutputBuffer(
				Request,
				sizeof(u32),
				&outDataBuffer,
				&bufSize);
	if (!NT_SUCCESS(Status)) {
		WdfRequestCompleteWithInformation(Request, Status, 0);
		return -1;
	}

	RtlCopyMemory(outDataBuffer, &bmdi->gmem_info.heap_cnt, bufSize);

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(u32));

	return 0;
}

int bmdrv_gmem_ioctl_get_heap_stat_byte_by_id(struct bm_device_info *bmdi,
                                _In_ WDFREQUEST        Request,
                                _In_ size_t            OutputBufferLength,
                                _In_ size_t            InputBufferLength) {
	PVOID  outDataBuffer;
	PVOID  inDataBuffer;
	size_t bufSize;
	struct bm_heap_stat_byte heap_info;
	u32 id = 0x0;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
	NTSTATUS Status = WdfRequestRetrieveInputBuffer(
			Request, sizeof(bm_heap_stat_byte_t), &inDataBuffer, &bufSize);
	if (!NT_SUCCESS(Status)) {
		WdfRequestCompleteWithInformation(Request, Status, 0);
		TraceEvents(TRACE_LEVEL_INFORMATION,
				TRACE_BGM,
				"%!FUNC! request inDataBuffer failed\n");
		return -1;
	}

	RtlCopyMemory(&heap_info, inDataBuffer, bufSize);

	id = (u32)heap_info.heap_id;
	if (id >= bmdi->gmem_info.heap_cnt) {
		WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		TraceEvents(TRACE_LEVEL_INFORMATION,
				TRACE_BGM,
				"%!FUNC! heap id is illegal, heap id = %d\n",
				id);
		return -1;
	}

	heap_info.mem_avail = bmdi->gmem_info.common_heap[id].free_page_count * BM_GMEM_PAGE_SIZE;
	heap_info.mem_total = bmdi->gmem_info.common_heap[id].heap_size;
	heap_info.mem_used = heap_info.mem_total - heap_info.mem_avail;
	heap_info.mem_start_addr = bmdi->gmem_info.common_heap[id].base_addr;

	Status = WdfRequestRetrieveOutputBuffer(
				Request,
				sizeof(bm_heap_stat_byte_t),
				&outDataBuffer,
				&bufSize);
if (!NT_SUCCESS(Status)) {
	WdfRequestCompleteWithInformation(Request, Status, 0);
	return -1;
	}

	RtlCopyMemory(outDataBuffer, &heap_info, bufSize);

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(bm_heap_stat_byte_t));

	return 0;
}
