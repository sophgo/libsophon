#include "bm_common.h"
#include "bm_api.tmh"
#include "bm_uapi.h"

//DEFINE_SPINLOCK(msg_dump_lock);
void bmdev_dump_reg(struct bm_device_info *bmdi, u32 channel)
{
    int i=0;
    //spin_lock(&msg_dump_lock);
    if (GP_REG_MESSAGE_WP_CHANNEL_XPU == channel) {
        for(i=0; i<32; i++)
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"DEV %d BDC_CMD_REG %d: addr= 0x%08x, value = 0x%08x\n", bmdi-> dev_index, i, bmdi->cinfo.bm_reg->tpu_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + i*4));
        for(i=0; i<64; i++)
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"DEV %d BDC_CTL_REG %d: addr= 0x%08x, value = 0x%08x\n", bmdi-> dev_index, i, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4));
        for(i=0; i<32; i++)
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"DEV %d GDMA_ALL_REG %d: addr= 0x%08x, value = 0x%08x\n", bmdi-> dev_index, i, bmdi->cinfo.bm_reg->gdma_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr + i*4));
       }
    else {
    }
    //spin_unlock(&msg_dump_lock);
}

int bmdrv_api_init(struct bm_device_info *bmdi, u32 channel)
{
	int ret = 0;
	struct bm_api_info *apinfo = &bmdi->api_info[channel];
    WDF_OBJECT_ATTRIBUTES attributes;
    NTSTATUS              status;

#ifndef SOC_MODE
	u32 val;
#endif

	apinfo->device_sync_last = 0;
	apinfo->device_sync_cpl = 0;
	apinfo->msgirq_num = 0UL;
	apinfo->sw_rp = 0;

    KeInitializeEvent(&apinfo->msg_doneEvent, SynchronizationEvent, FALSE);
    KeInitializeEvent(&apinfo->dev_sync_doneEvent, SynchronizationEvent, FALSE);
    ExInitializeFastMutex(&apinfo->api_mutex);

 #ifndef SOC_MODE
	if(bmdi->cinfo.chip_id == 0x1686) {
		val = bdc_reg_read(bmdi, 0x100);
		bdc_reg_write(bmdi, 0x100, val| 0x1);
	}
#endif

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = bmdi->WdfDevice;
    status = WdfSpinLockCreate(&attributes, &apinfo->api_fifo_spinlock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

	if (BM_MSGFIFO_CHANNEL_XPU == channel){
		ret = kfifo_alloc(&apinfo->api_fifo, bmdi->cinfo.share_mem_size * 4);

		bmdi->lib_dyn_info = API_P_ALLOC(sizeof(struct bmcpu_lib));
		InitializeListHead(&(bmdi->lib_dyn_info->lib_list));
		ExInitializeFastMutex(&(bmdi->lib_dyn_info->bmcpu_lib_mutex));
		InitializeListHead(&(bmdi->exec_func.func_list));
		ExInitializeFastMutex(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU, 0);
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_XPU, 0);
	}
	else{
		InitializeListHead(&apinfo->api_list);

		bmdi->lib_info = API_P_ALLOC(sizeof(struct bmcpu_lib));
		InitializeListHead(&(bmdi->lib_info->lib_list));
		ExInitializeFastMutex(&(bmdi->lib_info->bmcpu_lib_mutex));
	}

	return ret;
}

void bmdrv_api_deinit(struct bm_device_info *bmdi, u32 channel)
{
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_info;
	struct bmcpu_lib *lib_dyn_info = bmdi->lib_dyn_info;

	if (BM_MSGFIFO_CHANNEL_XPU == channel){
		ExAcquireFastMutex(&lib_dyn_info->bmcpu_lib_mutex);
		list_for_each_entry_safe(lib_temp, lib_next, &lib_dyn_info->lib_list, lib_list, struct bmcpu_lib) {
			RemoveEntryList(&lib_temp->lib_list);
			MmFreeNonCachedMemory((struct bmcpu_lib *) lib_temp,
									sizeof(struct bmcpu_lib));
		}
		ExReleaseFastMutex(&lib_dyn_info->bmcpu_lib_mutex);
		API_P_FREE(bmdi->lib_dyn_info);
	} else {
		ExAcquireFastMutex(&lib_dyn_info->bmcpu_lib_mutex);
		list_for_each_entry_safe(lib_temp, lib_next, &lib_dyn_info->lib_list, lib_list,  struct bmcpu_lib) {
			RemoveEntryList(&lib_temp->lib_list);
			MmFreeNonCachedMemory((struct bmcpu_lib *) lib_temp,
									sizeof(struct bmcpu_lib));
		}
		ExReleaseFastMutex(&lib_dyn_info->bmcpu_lib_mutex);
		API_P_FREE(bmdi->lib_info);
	}

	kfifo_free(&bmdi->api_info[channel].api_fifo);
}


#define MD5SUM_LEN 16
#define LIB_MAX_NAME_LEN 64
#define FUNC_MAX_NAME_LEN 64
/*
int bmdrv_api_load_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *bm_api)
{
	struct bmcpu_lib *lib_node;
	struct bmcpu_lib *lib_temp, *lib_next;
	bm_api_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_info = bmdi->lib_info;
	

    RtlCopyMemory((u8 *)&api_cpu_load_library_internal, bm_api->api_addr, sizeof(bm_api_cpu_load_library_internal_t));

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib) {
    	if (!strncmp(lib_temp->lib_name, api_cpu_load_library_internal.library_name, LIB_MAX_NAME_LEN) &&
    		(lib_temp->rec[api_cpu_load_library_internal.process_handle] != 0)) {
    		if (memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
    			ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);
    			return -1;
    		} else {
    			api_cpu_load_library_internal.obj_handle = lib_temp->cur_rec;
    			RtlCopyMemory((u8 *)bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
    			ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);
    			return 0;
    		}
    		break;
    	}
    }

    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib) {
    	if(!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
    		lib_temp->refcount++;
    		lib_temp->rec[api_cpu_load_library_internal.process_handle] = lib_temp->cur_rec;
    		api_cpu_load_library_internal.obj_handle = lib_temp->cur_rec;
    		RtlCopyMemory((u8 *)bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
    		ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);
    		return 0;
    	}
    }
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    lib_node = API_P_ALLOC(sizeof(struct bmcpu_lib));
    strncpy(lib_node->lib_name, api_cpu_load_library_internal.library_name, LIB_MAX_NAME_LEN);
    lib_node->refcount = 1;
    lib_node->cur_rec = api_cpu_load_library_internal.process_handle;
    lib_node->rec[lib_node->cur_rec] = lib_node->cur_rec;
    memcpy(lib_node->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN);

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    InsertTailList(&(lib_node->lib_list), &(lib_info->lib_list));
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    return 0;
}

int bmdrv_api_unload_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *bm_api)
{
	int i;
	struct bmcpu_lib *lib_temp, *lib_next;
	bm_api_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_info = bmdi->lib_info;

    RtlCopyMemory((u8 *)&api_cpu_load_library_internal, bm_api->api_addr, sizeof(bm_api_cpu_load_library_internal_t));

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib) {
    	if (!strncmp(lib_temp->lib_name, api_cpu_load_library_internal.library_name, LIB_MAX_NAME_LEN) &&
			lib_temp->rec[api_cpu_load_library_internal.process_handle] != 0) {
			lib_temp->refcount--;
			if (lib_temp->cur_rec != api_cpu_load_library_internal.process_handle) {
				api_cpu_load_library_internal.obj_handle = lib_temp->rec[api_cpu_load_library_internal.process_handle];
				RtlCopyMemory((u8 *)bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
			} else if (lib_temp->cur_rec == api_cpu_load_library_internal.process_handle) {
				if (lib_temp->refcount == 0) {
					api_cpu_load_library_internal.obj_handle = lib_temp->rec[api_cpu_load_library_internal.process_handle];
					RtlCopyMemory((u8 *)bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
					RemoveEntryList(&lib_temp->lib_list);
					MmFreeNonCachedMemory((struct bmcpu_lib *) lib_temp,
											sizeof(struct bmcpu_lib));
				} else if (lib_temp->refcount != 0) {
					for (i = 0; i < 50; i++) {
						if (lib_temp->rec[i] != 0 && i != api_cpu_load_library_internal.process_handle) {
							break;
						}
					}
					api_cpu_load_library_internal.mv_handle = i;
					api_cpu_load_library_internal.obj_handle = lib_temp->rec[api_cpu_load_library_internal.process_handle];
					RtlCopyMemory((u8 *)bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
					lib_temp->cur_rec = i;
				}

			}
			lib_temp->rec[api_cpu_load_library_internal.process_handle] = 0;
			break;
		}
    }
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);
    return 0;
}
*/

int bmdrv_api_dyn_get_func_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api)
{
	static int f_id = 22;
	struct bmdrv_exec_func *func_node;
	LIST_ENTRY *pos;
	int fun_flag = 0;

	func_node = (struct bmdrv_exec_func *)API_P_ALLOC(sizeof(struct bmdrv_exec_func));
	if (!func_node){
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d alloc func node error!\n", bmdi->dev_index);
		return -1;
	}
    InitializeListHead(&(func_node->func_list));

    RtlCopyMemory(&func_node->exec_func, (bm_get_func_t *)p_bm_api->api_addr, sizeof(bm_get_func_t));

    if (!&(func_node->exec_func)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d copy from user fail!\n", bmdi->dev_index);
        return -1;
    }

    ExAcquireFastMutex(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
    list_for_each(pos, &(bmdi->exec_func.func_list))
    {
    	if (!memcmp(((struct bmdrv_exec_func *)pos)->exec_func.md5, func_node->exec_func.md5, MD5SUM_LEN) &&
    		!strncmp(((struct bmdrv_exec_func *)pos)->exec_func.func_name, func_node->exec_func.func_name, FUNC_MAX_NAME_LEN)) {
    		func_node->exec_func.f_id = ((struct bmdrv_exec_func *)pos)->exec_func.f_id;
    		fun_flag = 1;
    		break;
    	}
    }
    ExReleaseFastMutex(&(bmdi->exec_func.exec_func.bm_get_func_mutex));

    if (!&(func_node->func_list)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d func_node list entry corrupted\n", bmdi->dev_index);
        return -1;
    }

    if(fun_flag == 0) {
    	func_node->exec_func.f_id = f_id++;
    	//TODO : V mutex
    	//ExAcquireFastMutex(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
    	//InsertTailList(&(func_node->func_list), &(bmdi->exec_func.func_list));
    	//ExReleaseFastMutex(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
    }

    RtlCopyMemory((bm_get_func_t *)p_bm_api->api_addr, &func_node->exec_func, sizeof(bm_get_func_t));

    if(fun_flag == 1) {
    	API_P_FREE(func_node);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d get func process failed\n", bmdi->dev_index);
    	return -1;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d get func process successful\n", bmdi->dev_index);
    return 0;
}

/*
int bmdrv_api_dyn_load_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api)
{
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "[bmdrv_api_dyn_load_lib_process] Have gone to load library\n");
	bm_api_dyn_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_node;
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;

    RtlCopyMemory((u8 *)&api_cpu_load_library_internal, (bm_api_dyn_cpu_load_library_internal_t *)p_bm_api->api_addr, sizeof(bm_api_dyn_cpu_load_library_internal_t));

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib){
    	if (!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "Have gone to stage1\n");
    		lib_node = (struct bmcpu_lib *)MmAllocateNonCachedMemory(sizeof(struct bmcpu_lib));
    		strncpy(lib_node->lib_name, api_cpu_load_library_internal.lib_name, LIB_MAX_NAME_LEN);
    		lib_node->cur_rec = lib_temp->cur_rec;
    		lib_node->rec[lib_node->cur_rec] = 1;
    		memcpy(lib_node->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN);
    		lib_node->cur_pid = (u64)PsGetCurrentThreadId();
    		AppendTailList(&(lib_node->lib_list), &(lib_info->lib_list));
    		ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);
    		MmFreeNonCachedMemory((struct bmcpu_lib*)lib_node, sizeof(struct bmcpu_lib));
    		return -1;
    	}
    }
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib) {
    	if (!strncmp(lib_temp->lib_name, api_cpu_load_library_internal.lib_name, LIB_MAX_NAME_LEN)){
    		(api_cpu_load_library_internal.cur_rec)++;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "Have gone to stage3\n");
    	}
    }
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    lib_node = (struct bmcpu_lib *)MmAllocateNonCachedMemory(sizeof(struct bmcpu_lib));
    InitializeListHead(&(lib_node->lib_list));

    strncpy(lib_node->lib_name, api_cpu_load_library_internal.lib_name, LIB_MAX_NAME_LEN);
    lib_node->cur_rec = api_cpu_load_library_internal.cur_rec;
    lib_node->rec[lib_node->cur_rec] = 1;
    memcpy(lib_node->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN);
    lib_node->cur_pid = (u64)PsGetCurrentThreadId();
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "Have gone to here\n");
    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "Have gone to here2\n");
    AppendTailList(&(lib_node->lib_list), &(lib_info->lib_list));
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "Have gone to here3\n");

    RtlCopyMemory((u8 *)p_bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_dyn_cpu_load_library_internal_t));

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "Have gone to stage2\n");
    MmFreeNonCachedMemory((struct bmcpu_lib *)lib_node, sizeof(struct bmcpu_lib));
    return 0;
}

int bmdrv_api_dyn_unload_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api)
{
	bm_api_dyn_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;
	LIST_ENTRY *pos, *pos_next;
	int del_lib = 1;

    RtlCopyMemory((u8 *)&api_cpu_load_library_internal, (bm_api_dyn_cpu_load_library_internal_t *)p_bm_api->api_addr, sizeof(bm_api_dyn_cpu_load_library_internal_t));

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib) {
    	if (!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN) && (u64)(PsGetCurrentThreadId()) == lib_temp->cur_pid) {
    		api_cpu_load_library_internal.cur_rec = lib_temp->cur_rec;
    		RemoveEntryList(&(lib_temp->lib_list));
    		MmFreeNonCachedMemory((struct bmcpu_lib *)lib_temp,
                                   sizeof(struct bmcpu_lib));
    		break;
    	}
    }
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib) {
    	if (!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
    		del_lib = 0;
    		break;
    	}
    }
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);

    if (del_lib == 1) {
    	list_for_each_safe(pos, pos_next, &(bmdi->exec_func.func_list)) {
    		if (!memcmp(((struct bmdrv_exec_func *)pos)->exec_func.md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
    			RemoveEntryList(&((struct bmdrv_exec_func *)pos)->func_list);
    			MmFreeNonCachedMemory((struct bmdrv_exec_func *)pos,
    									sizeof(struct bmdrv_exec_func));
    		}
    	}

    	RtlCopyMemory((u8 *)p_bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_dyn_cpu_load_library_internal_t));
    	return 0;
    }

    return -1;

}
*/

int bmdrv_trigger_bmcpu(struct bm_device_info *bmdi,
                        _In_ WDFREQUEST        Request) {
    int           ret;
    unsigned char buffer[4] = {0xea, 0xea, 0xea, 0xea};
    unsigned char zeros[4]  = {0};
    NTSTATUS      Status;
    PVOID         DataBuffer;
    size_t        bufSize;
    int           delay = 0;
    u32 val;

    Status = WdfRequestRetrieveInputBuffer(Request, sizeof(int), &DataBuffer, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_API,
                    "bm-sophon%d %!FILE!-%!LINE!: %!FUNC!  fail\n",
                    bmdi->dev_index);
        return -1;
    }

    if (bmdi->cinfo.chip_id == 0x1686) {
		val = top_reg_read(bmdi, 0x214);
		val |= 1;
		top_reg_write(bmdi, 0x214, val);
		return 0;
	}

	RtlCopyMemory(&delay, (int *)DataBuffer, bufSize);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d %!FILE!-%!LINE!: %!FUNC!  delay:%d\n", bmdi->dev_index, delay);

    ExAcquireFastMutex(&bmdi->c_attr.attr_mutex);
    ret = bmdev_memcpy_s2d_internal(
        bmdi, 0x102ffffc, (const void *)buffer, sizeof(buffer));
    bm_mdelay(delay);
    ExReleaseFastMutex(&bmdi->c_attr.attr_mutex);
    bmdev_memcpy_s2d_internal(
        bmdi, 0x102ffffc, (const void *)zeros, sizeof(zeros));
	WdfRequestCompleteWithInformation(Request, Status, sizeof(int));
    return 0;
}

int bmdrv_set_bmcpu_status(struct bm_device_info *bmdi,
                           _In_ WDFREQUEST        Request) {
    int           ret = 0;
    NTSTATUS      Status;
    PVOID         DataBuffer;
    size_t        bufSize;
    int           bmcpuStatus = 0;

    Status = WdfRequestRetrieveInputBuffer(
        Request, sizeof(int), &DataBuffer, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_API,
                    "bm-sophon%d %!FILE!-%!LINE!: %!FUNC!  fail\n",
                    bmdi->dev_index);
        return -1;
    }
	RtlCopyMemory(&bmcpuStatus, (int *)DataBuffer, bufSize);
    ExAcquireFastMutex(&bmdi->c_attr.attr_mutex);
    bmdi->status_bmcpu = bmcpuStatus;
    ExReleaseFastMutex(&bmdi->c_attr.attr_mutex);
	WdfRequestCompleteWithInformation(Request, Status, sizeof(int));
    return ret;
}

int bmdrv_send_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request, U8 flag) {
	int ret = 0;
	struct bm_thread_info *thd_info;
	struct api_fifo_entry *api_entry;
	struct api_list_entry *api_entry_list = NULL;
	struct bm_api_info *apinfo;
	HANDLE api_pid;
	bm_api_ext_t bm_api;
	bm_kapi_header_t api_header;
	bm_kapi_opt_header_t api_opt_header;
	int fifo_avail;
	u32 fifo_empty_number;
	struct bm_handle_info *h_info;
	u64 local_send_api_seq;
	u32 channel;
	size_t bufSize;
	PVOID DataBuffer;
	PVOID OutDataBuffer;
    NTSTATUS Status;
    PHYSICAL_ADDRESS pa;
    pa.QuadPart = 0xFFFFFFFFFFFFFFFF;
    WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);

	if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d send_api: file list is not found!\n", bmdi->dev_index);
        ret = -1;
        goto Error;
	}
	/*copy user data to bm_api */
    if (flag) {
		Status = WdfRequestRetrieveInputBuffer(Request, sizeof(bm_api_ext_t), &DataBuffer, &bufSize);
        RtlCopyMemory(&bm_api, (bm_api_ext_t *)DataBuffer, bufSize);
    } else {
        Status = WdfRequestRetrieveInputBuffer(Request, sizeof(bm_api_t), &DataBuffer, &bufSize);
        RtlCopyMemory(&bm_api, (bm_api_t *)DataBuffer, bufSize);
    }
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
        return -1;
    }


// 	if (0 == (bm_api.api_id & 0x80000000)) {
// 		channel = BM_MSGFIFO_CHANNEL_XPU;
// 		apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU];
// 	} else {
// #ifdef PCIE_MODE_ENABLE_CPU
// 		channel = BM_MSGFIFO_CHANNEL_CPU;
// 		apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU];
// #else
//         TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d bmdrv: cpu api not enable!\n", bmdi->dev_index);
// 		return -1;
// #endif
// 	}

    if (flag){
#ifdef PCIE_MODE_ENABLE_CPU
    	channel = BM_MSGFIFO_CHANNEL_CPU;
    	apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU];
#else
    	TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d bmdrv: cpu api not enable!\n", bmdi->dev_index);
    	return -1;
#endif
    } else {
    	channel = BM_MSGFIFO_CHANNEL_XPU;
    	apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU];
    }

	ExAcquireFastMutex(&apinfo->api_mutex);
    api_pid = PsGetCurrentThreadId();

    /*
    if (bm_api.api_id == 0x90000001) {
    	ret = bmdrv_api_dyn_load_lib_process(bmdi, &bm_api);
    	if (ret == -1) {
    		ExReleaseFastMutex(&apinfo->api_mutex);
    		return 0;
    	}
    }
	*/

    if (bm_api.api_id == 0x90000002) {
    	ret = bmdrv_api_dyn_get_func_process(bmdi, &bm_api);
    	if (ret == -1) {
    		ExReleaseFastMutex(&apinfo->api_mutex);
    		return 0;
    	}
    }


    if (bm_api.api_id == 0x90000004) {
        ExReleaseFastMutex(&apinfo->api_mutex);
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d step to 0x90000004\n", bmdi->dev_index);
        return 0;
    }


    /*
    if (bm_api.api_id == 0x90000004) {
    	ret = bmdrv_api_dyn_unload_lib_process(bmdi, &bm_api);
    	if (ret != 0) {
    		ExReleaseFastMutex(&apinfo->api_mutex);
    		return ret;
    	}
    }
	*/
    /*
    if (bm_api.api_id == BM_API_ID_LOAD_LIBRARY) {
    	ret = bmdrv_api_load_lib_process(bmdi, &bm_api);
    	if (ret != 0) {
    		ExReleaseFastMutex(&apinfo->api_mutex);
    		return ret;
    	}
    }

    if (bm_api.api_id == BM_API_ID_UNLOAD_LIBRARY) {
    	ret = bmdrv_api_unload_lib_process(bmdi, &bm_api);
    	if (ret != 0) {
    		ExReleaseFastMutex(&apinfo->api_mutex);
    		return ret;
    	}
    }
    */
	/* check if current pid already recorded */
	thd_info = bmdrv_find_thread_info(h_info, (u64)api_pid);
	if (!thd_info) {
		thd_info = bmdrv_create_thread_info(h_info, (u64)api_pid);
		if (!thd_info) {
            ExReleaseFastMutex(&apinfo->api_mutex);
            TraceEvents( TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d bmdrv: bmdrv_create_thread_info failed!\n", bmdi->dev_index);
            ret = -1;
            goto Error;
		}
	}
	if (BM_MSGFIFO_CHANNEL_XPU == channel) {
		fifo_empty_number = bm_api.api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32);
		///* init api fifo entry */
        api_entry = (struct api_fifo_entry *)MmAllocateContiguousMemory(API_ENTRY_SIZE, pa);
		if (!api_entry) {
			ExReleaseFastMutex(&apinfo->api_mutex);
            ret = -1;
            goto Error;
		}
	} else {
		fifo_empty_number = bm_api.api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32) + sizeof(bm_kapi_opt_header_t) / sizeof(u32);
		api_entry_list = (struct api_list_entry * )MmAllocateNonCachedMemory(sizeof(struct api_list_entry));
		if (!api_entry_list) {
			ExReleaseFastMutex(&apinfo->api_mutex);
            TraceEvents(
               TRACE_LEVEL_ERROR,
               TRACE_API,
               "bm-sophon%d bmdrv: kmalloc api_list_entry failed!\n", bmdi->dev_index);
            ret = -1;
            goto Error;
		}
		InitializeListHead(&(api_entry_list->api_list_node));
		api_entry = &api_entry_list->api_entry;
	}
    /* update handle api sequence number */
	WdfSpinLockAcquire(h_info->h_api_seq_spinlock);
	///* update global api sequence number */
	bmdi->bm_send_api_seq++;
	local_send_api_seq = bmdi->bm_send_api_seq;
	h_info->h_send_api_seq = local_send_api_seq;
    WdfSpinLockRelease(h_info->h_api_seq_spinlock);
	/* update last_api_seq of current thread */
	/* may overflow */
	thd_info->last_api_seq = local_send_api_seq;
	thd_info->profile.sent_api_counter++;
	bmdi->profile.sent_api_counter++;

	api_header.api_id = bm_api.api_id;
	api_header.api_size = bm_api.api_size / sizeof(u32);
	api_header.api_handle = (u64)h_info->fileObject;
	api_header.api_seq = (u32)thd_info->last_api_seq;
	api_header.duration = 0; /* not get from this area now */
	api_header.result = 0;

	/* insert api info to api fifo */
	api_entry->thd_info = thd_info;
	api_entry->h_info = h_info;
	api_entry->thd_api_seq = (u32)thd_info->last_api_seq;
	api_entry->dev_api_seq = 0;
	api_entry->api_id = bm_api.api_id;

	u64 tick_count;
    LARGE_INTEGER CurTime, Freq;
    CurTime = KeQueryPerformanceCounter(&Freq);
    tick_count = (CurTime.QuadPart * 1000000) / Freq.QuadPart;

    api_entry->sent_time_us = tick_count;
    api_entry->global_api_seq = local_send_api_seq;
    api_entry->api_done_flag  = 0;

    KeInitializeEvent(&api_entry->api_doneEvent, SynchronizationEvent, FALSE);//used when cpu enabled, not used now
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"%!FUNC!:%!LINE! api_id:%lld send last_api_seq is %lld\n", (u64)api_pid, thd_info->last_api_seq);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"%!FUNC!:%!LINE! api_id: %lld sent_api_counter is %lld, completed_api_counter is %lld", (u64)api_pid,
				thd_info->profile.sent_api_counter, thd_info->profile.completed_api_counter);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"%!FUNC!:%!LINE! api_id: %lld send api id is %d\n", (u64)api_pid, bm_api.api_id);

	/* wait for available fifo space */
	if (bmdev_wait_msgfifo(bmdi, fifo_empty_number, bmdi->cinfo.delay_ms, channel)) {
		thd_info->last_api_seq--;
        MmFreeContiguousMemory(api_entry);
        ExReleaseFastMutex(&apinfo->api_mutex);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d bmdrv: bmdev_wait_msgfifo timeout!\n", bmdi->dev_index);
        ret = -1;
        goto Error;
	}
	if (BM_MSGFIFO_CHANNEL_CPU == channel) {
        WdfSpinLockAcquire(apinfo->api_fifo_spinlock);
		AppendTailList(&(api_entry_list->api_list_node), &(apinfo->api_list));
		WdfSpinLockRelease(apinfo->api_fifo_spinlock);

		api_opt_header.global_api_seq = local_send_api_seq;
		api_opt_header.api_data = 0;
		/* copy api data to fifo */
		ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, &api_opt_header, channel);
	} else {
		fifo_avail = kfifo_avail(&apinfo->api_fifo);
		if (fifo_avail >= API_ENTRY_SIZE) {
            WdfSpinLockAcquire(apinfo->api_fifo_spinlock);
			kfifo_in(&apinfo->api_fifo, (unsigned char* )api_entry, API_ENTRY_SIZE);
            WdfSpinLockRelease(apinfo->api_fifo_spinlock);
		} else {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d api fifo full!\n", bmdi->dev_index);
			thd_info->last_api_seq--;
            MmFreeContiguousMemory(api_entry);
            ExReleaseFastMutex(&apinfo->api_mutex);
            ret = -1;
            goto Error;
		}
        MmFreeContiguousMemory(api_entry);
		/* copy api data to fifo */
		ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, NULL, channel);
	}
	if (flag){
		Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(bm_api_ext_t), &OutDataBuffer, &bufSize);
		bm_api.api_handle = api_entry->global_api_seq;
        RtlCopyMemory(OutDataBuffer, &bm_api, bufSize);
		ExReleaseFastMutex(&apinfo->api_mutex);
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(bm_api_ext_t));
		return ret;
	}
    ExReleaseFastMutex(&apinfo->api_mutex);
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
	return ret;
Error:
    WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
    return ret;
}

int bmdrv_query_api(struct bm_device_info *bmdi,
                    _In_ WDFREQUEST        Request)
{
	bm_api_data_t bm_api_data;
	u32 channel;
	size_t bufSize;
	PVOID InDataBuffer;
	PVOID OutDataBuffer;
    NTSTATUS Status;
	u64 data;

	Status = WdfRequestRetrieveInputBuffer(Request, sizeof(bm_api_data_t), &InDataBuffer, &bufSize);
	if (!NT_SUCCESS(Status)) {
       TraceEvents(TRACE_LEVEL_ERROR,
                   TRACE_API,
                   "bm-sophon%d %!FUNC! fail, status is %d\n",
                   bmdi->dev_index, Status);
		WdfRequestCompleteWithInformation(Request, Status, 0);
		return Status;
	}
    RtlCopyMemory(&bm_api_data, (bm_api_data_t *)InDataBuffer, bufSize);
	if (0 == (bm_api_data.api_id & 0x80000000))
		channel = BM_MSGFIFO_CHANNEL_XPU;
	else {
#ifdef PCIE_MODE_ENABLE_CPU
		channel = BM_MSGFIFO_CHANNEL_CPU;
#else
       TraceEvents(TRACE_LEVEL_ERROR,
                   TRACE_API,
                   "bm-sophon%d bmdrv: cpu api not enable!\n",
                   bmdi->dev_index);
		return -1;
#endif
	}
	Status = bmdev_msgfifo_get_api_data(bmdi, channel, bm_api_data.api_handle, &data, bm_api_data.timeout);
	if (0 == Status){
		WdfRequestRetrieveOutputBuffer(Request, sizeof(bm_api_data_t), &OutDataBuffer, &bufSize);
		bm_api_data.data = data;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "%!FUNC!:%!LINE! data:%lld\n", bm_api_data.data);
		RtlCopyMemory(OutDataBuffer, &bm_api_data, bufSize);
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(bm_api_data_t));
	}else{
		WdfRequestCompleteWithInformation(Request, Status, 0);
	}
	return Status;
}

//#if SYNC_API_INIT_MODE == 1
int bmdrv_thread_sync_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request) {
	int ret = 1;
	HANDLE api_pid = 0;
	struct bm_thread_info *thd_info;
	int timeout_ms = bmdi->cinfo.delay_ms;
	struct bm_handle_info *h_info;
    LARGE_INTEGER          li;
    NTSTATUS               status = STATUS_SUCCESS;

	li.QuadPart = WDF_REL_TIMEOUT_IN_MS(timeout_ms);
    WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);
    if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d sync_api: file list is not found!\n", bmdi->dev_index);
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return -1;
	}
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
    api_pid = PsGetCurrentThreadId();
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"entry bmdrv: %lld thread sync api, wait_time=%d\n", (u64)api_pid, timeout_ms);
	thd_info = bmdrv_find_thread_info(h_info, (u64)api_pid);
	if (!thd_info) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d thread not recorded!\n", bmdi->dev_index);
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
		return 0;
	}

	while ((thd_info->cpl_api_seq != thd_info->last_api_seq) && (ret != 0)) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"bm-sophon%d bmdrv: %lld sync api, last is %lld, cpl is %lld, h_info is 0x%p,thd_info is 0x%p\n",
					bmdi->dev_index, (u64)api_pid, thd_info->last_api_seq, thd_info->cpl_api_seq, h_info,thd_info);
		status = KeWaitForSingleObject(& thd_info->msg_doneEvent, Executive, KernelMode, FALSE, &li);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon sync status: %d\n", status);
        if (status == STATUS_SUCCESS)
            ret = 1;
        else
            ret = 0;
	}

    if (STATUS_SUCCESS == status) {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
        return 0;
    } else {
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
    }
	TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d %s, wait api timeout\n", bmdi->dev_index, __func__);
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_XPU);
	bmdev_dump_reg(bmdi, BM_MSGFIFO_CHANNEL_XPU);
#ifdef PCIE_MODE_ENABLE_CPU
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_CPU);
#endif
	return -1;
}

int bmdrv_handle_sync_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request)
{
	int ret = 1;
	int timeout_ms = bmdi->cinfo.delay_ms;
	struct bm_handle_info *h_info;
	u64 handle_send_api_seq = 0;
    LARGE_INTEGER          li;
    NTSTATUS               status = STATUS_SUCCESS;

    li.QuadPart   = WDF_REL_TIMEOUT_IN_MS(timeout_ms);
    HANDLE api_pid  = PsGetCurrentThreadId();
    WDFFILEOBJECT fileObject = WdfRequestGetFileObject(Request);
    if (bmdev_gmem_get_handle_info(bmdi, fileObject, &h_info)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d handle_sync: file list is not found!\n", bmdi->dev_index);
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
		return -1;
	}
    WdfSpinLockAcquire(h_info->h_api_seq_spinlock);
	handle_send_api_seq = h_info->h_send_api_seq;
    WdfSpinLockRelease(h_info->h_api_seq_spinlock);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif

	while ((h_info->h_cpl_api_seq < handle_send_api_seq) && (ret != 0)) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"bmdrv: %lld sync api, last is %lld, cpl is %lld\n", (u64)api_pid, handle_send_api_seq,
				 h_info->h_cpl_api_seq);
			status = KeWaitForSingleObject(&h_info->h_msg_doneEvent, Executive, KernelMode, FALSE, &li);

        if (status == STATUS_SUCCESS)
            ret = 1;
        else
            ret = 0;
	}

    if (STATUS_SUCCESS == status) {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
        return 0;
    } else {
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
    }
	TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d %s, thread %lld wait api timeout, return %x\n", bmdi->dev_index, __func__, (u64)api_pid, status);
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_XPU);
	bmdev_dump_reg(bmdi, BM_MSGFIFO_CHANNEL_XPU);
#ifdef PCIE_MODE_ENABLE_CPU
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_CPU);
#endif
	return -1;
}

/*
#else
int bmdrv_thread_sync_api(struct bm_device_info *bmdi,  _In_ WDFREQUEST Request)
{
	int polling_ms = bmdi->cinfo.polling_ms;
	int cnt = 10;

	while (gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP) != gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP)) {
		bm_mdelay(polling_ms);
		cnt--;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "wait polling api done! %d \n", polling_ms);
		if (cnt < 0) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "sync api time out \n");
			break;
		}
	}

	return 0;
}

int bmdrv_handle_sync_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request)
{
	return bmdrv_thread_sync_api(bmdi, Request);
}
#endif
*/


int bmdrv_device_sync_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request)
{
	struct api_fifo_entry *api_entry;
	int fifo_avail;
	struct bm_api_info *apinfo;
	int ret = 1;
	u32 dev_sync_last;
	int timeout_ms = bmdi->cinfo.delay_ms;
	u32 channel;
	struct api_list_entry *api_entry_list;
    UNREFERENCED_PARAMETER(api_entry_list);
    PHYSICAL_ADDRESS       pa;
    pa.QuadPart = 0xFFFFFFFFFFFFFFFF;
    LARGE_INTEGER li;
    NTSTATUS      status = STATUS_SUCCESS;
    li.QuadPart = WDF_REL_TIMEOUT_IN_MS(timeout_ms);

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
	api_entry = (struct api_fifo_entry *)MmAllocateContiguousMemory(API_ENTRY_SIZE, pa);
	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		apinfo = &bmdi->api_info[channel];

        ExAcquireFastMutex(&apinfo->api_mutex);
		if (channel == BM_MSGFIFO_CHANNEL_XPU) {
			//api_entry = (struct api_fifo_entry *)MmAllocateContiguousMemory(API_ENTRY_SIZE, pa); 
			if (!api_entry) {
                ExReleaseFastMutex(&apinfo->api_mutex);
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
				return -1;
			}
            WdfSpinLockAcquire(apinfo->api_fifo_spinlock);
			/* if fifo empty, return success */
			if (kfifo_is_empty(&apinfo->api_fifo)) {
                WdfSpinLockRelease(apinfo->api_fifo_spinlock);
                MmFreeContiguousMemory(api_entry);
                ExReleaseFastMutex(&apinfo->api_mutex);
                WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "bm-sophon%d, thread %lld fifo is empty\n", bmdi->dev_index, (u64)PsGetCurrentThreadId());
				return 0;
			}
		} else {
           /* WdfSpinLockAcquire(apinfo->api_fifo_spinlock);
			if (bmdev_msgfifo_empty(bmdi, BM_MSGFIFO_CHANNEL_CPU)) {
                WdfSpinLockRelease(&apinfo->api_fifo_spinlock);
                ExReleaseFastMutex(&apinfo->api_mutex);
				return 0;
			}
			api_entry_list = kmalloc(sizeof(struct api_list_entry), GFP_KERNEL);
			if (!api_entry_list) {
                WdfSpinLockRelease(&apinfo->api_fifo_spinlock);
                ExReleaseFastMutex(&apinfo->api_mutex);
				return -1;
			}
			api_entry = &api_entry_list->api_entry;*/
		}

		/* insert device sync marker into fifo */
		apinfo->device_sync_last++;
		api_entry->thd_info = (struct bm_thread_info *)DEVICE_SYNC_MARKER;
		api_entry->thd_api_seq = 0;
		api_entry->dev_api_seq = apinfo->device_sync_last;
		/* record device sync last; apinfo is global; its value may be changed */
		dev_sync_last = apinfo->device_sync_last;

		if (channel == BM_MSGFIFO_CHANNEL_XPU) {
			fifo_avail = kfifo_avail(&apinfo->api_fifo);
			if (fifo_avail >= API_ENTRY_SIZE) {
				kfifo_in(&apinfo->api_fifo, (unsigned char *)api_entry, API_ENTRY_SIZE);
                WdfSpinLockRelease(apinfo->api_fifo_spinlock);
                MmFreeContiguousMemory(api_entry);
			} else {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"api fifo full!%d\n",fifo_avail);
				apinfo->device_sync_last--;
                WdfSpinLockRelease(apinfo->api_fifo_spinlock);
                MmFreeContiguousMemory(api_entry);
                ExReleaseFastMutex(&apinfo->api_mutex);
                WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
				return -1;
			}
		} else {
			//InsertTailList(&(api_entry_list->api_list_node), &apinfo->api_list);
            WdfSpinLockRelease(apinfo->api_fifo_spinlock);
            //ExReleaseFastMutex(&apinfo->api_mutex);
		}

		while((apinfo->device_sync_cpl != dev_sync_last) && (ret != 0)){
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"device_sync waiting entry:pid %lld  send api %d and complete api %d\n",(u64)PsGetCurrentThreadId(),
				apinfo->device_sync_last, apinfo->device_sync_cpl);
			status = KeWaitForSingleObject(&apinfo->dev_sync_doneEvent, Executive, KernelMode, FALSE, &li);
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API,"device_sync waiting exit:pid %lld  send api %d and complete api %d,status=%x\n",(u64)PsGetCurrentThreadId(),
				apinfo->device_sync_last, apinfo->device_sync_cpl,status);

            if (status == STATUS_SUCCESS)
                ret = 1;
            else
                ret = 0;
		}

		if (status == STATUS_SUCCESS) {
            ExReleaseFastMutex(&apinfo->api_mutex);
            continue;
        }

        TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d, wait api timeout\n", bmdi->dev_index);
        WdfRequestCompleteWithInformation(Request, status, 0);
        ExReleaseFastMutex(&apinfo->api_mutex);
		bmdev_dump_msgfifo(bmdi, channel);
		bmdev_dump_reg(bmdi, channel);
		return -1;
	}

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
	return 0;
}

int bmdrv_loaded_lib(struct bm_device_info *bmdi,
                     _In_ WDFREQUEST        Request,
                     _In_ size_t            OutputBufferLength,
                     _In_ size_t            InputBufferLength)
{
	loaded_lib_t loaded_lib;
	struct bmcpu_lib *lib_temp, *lib_next, *lib_info = bmdi->lib_dyn_info;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

	size_t inbufSize;
    PVOID  InDataBuffer;
    NTSTATUS Status = WdfRequestRetrieveInputBuffer(Request, sizeof(loaded_lib_t), &InDataBuffer, &inbufSize);
    if (!NT_SUCCESS(Status)) {
    	WdfRequestCompleteWithInformation(Request, Status, 0);
    	return -1;
    }

    size_t outbufSize;
    PVOID  OutDataBuffer;
    Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(loaded_lib_t), &OutDataBuffer, &outbufSize);
    if (!NT_SUCCESS(Status)) {
    	WdfRequestCompleteWithInformation(Request, Status, 0);
    	return -1;
    }

    RtlCopyMemory(&loaded_lib, InDataBuffer, sizeof(loaded_lib_t));
    loaded_lib.loaded = 0;

    ExAcquireFastMutex(&lib_info->bmcpu_lib_mutex);
    list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list, struct bmcpu_lib){
    	if (!memcmp(lib_temp->md5, loaded_lib.md5, 16)){
    		loaded_lib.loaded = 1;
    		break;
    	}
    }
    ExReleaseFastMutex(&lib_info->bmcpu_lib_mutex);
    RtlCopyMemory(OutDataBuffer, &loaded_lib, sizeof(loaded_lib_t));
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
    return 0;
}