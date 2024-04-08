#include "bm_common.h"

#include "bm_drv.tmh"

static void bmdrv_print_cardinfo(struct chip_info *cinfo)
{

	u16 pcie_version = 0;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,
                "bar0 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar0_start, cinfo->bar_info.bar0_len,
			cinfo->bar_info.bar0_vaddr);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DRIVER,
                "bar1 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar1_start, cinfo->bar_info.bar1_len,
			cinfo->bar_info.bar1_vaddr);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DRIVER,
                "bar2 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar2_start, cinfo->bar_info.bar2_len,
			cinfo->bar_info.bar2_vaddr);

	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DRIVER,
                "bar1 part0 offset 0x%llx size 0x%llx, dev_addr = 0x%llx\n",
			cinfo->bar_info.bar1_part0_offset, cinfo->bar_info.bar1_part0_len,
			cinfo->bar_info.bar1_part0_dev_start);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DRIVER,
                "bar1 part1 offset 0x%llx size 0x%llx, dev_addr = 0x%llx\n",
			cinfo->bar_info.bar1_part1_offset, cinfo->bar_info.bar1_part1_len,
			cinfo->bar_info.bar1_part1_dev_start);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DRIVER,
                "bar1 part2 offset 0x%llx size 0x%llx, dev_addr = 0x%llx\n",
			cinfo->bar_info.bar1_part2_offset, cinfo->bar_info.bar1_part2_len,
			cinfo->bar_info.bar1_part2_dev_start);

	//pcie_version = pcie_caps_reg(cinfo->pcidev) & PCI_EXP_FLAGS_VERS;
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DRIVER,
                "PCIe version 0x%x\n",
                pcie_version);
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DRIVER,
                "board version 0x%x\n",
                cinfo->board_version);

}

void bmdrv_post_api_process(struct bm_device_info *bmdi,
                            struct api_fifo_entry  api_entry,
                            u32                    channel) {
    struct bm_thread_info *ti = api_entry.thd_info;
    struct bm_trace_item *ptitem       = NULL;
    u32                   next_rp      = 0;
    u32                   api_id       = 0;
    u32                   api_size     = 0;
    u32                   api_duration = 0;
    u32                   api_result   = 0;

    next_rp = bmdev_msgfifo_add_pointer(
        bmdi,
        bmdi->api_info[channel].sw_rp,
        offsetof(bm_kapi_header_t, api_id) / sizeof(u32));
    api_id  = shmem_reg_read_enh(bmdi, next_rp, channel);
    next_rp = bmdev_msgfifo_add_pointer(
        bmdi,
        bmdi->api_info[channel].sw_rp,
        offsetof(bm_kapi_header_t, api_size) / sizeof(u32));
    api_size = shmem_reg_read_enh(bmdi, next_rp, channel);
    next_rp  = bmdev_msgfifo_add_pointer(
        bmdi,
        bmdi->api_info[channel].sw_rp,
        offsetof(bm_kapi_header_t, duration) / sizeof(u32));
    api_duration = shmem_reg_read_enh(bmdi, next_rp, channel);
    next_rp      = bmdev_msgfifo_add_pointer(
        bmdi,
        bmdi->api_info[channel].sw_rp,
        offsetof(bm_kapi_header_t, result) / sizeof(u32));
    api_result = shmem_reg_read_enh(bmdi, next_rp, channel);
#ifdef PCIE_MODE_ENABLE_CPU
    if (channel == BM_MSGFIFO_CHANNEL_CPU)
        next_rp = bmdev_msgfifo_add_pointer(
            bmdi,
            bmdi->api_info[channel].sw_rp,
            ((sizeof(bm_kapi_header_t)) + sizeof(bm_kapi_opt_header_t)) /
                    sizeof(u32) +
                api_size);
    else
#endif
        next_rp = bmdev_msgfifo_add_pointer(
            bmdi,
            bmdi->api_info[channel].sw_rp,
            (sizeof(bm_kapi_header_t)) / sizeof(u32) + api_size);
    bmdi->api_info[channel].sw_rp = next_rp;
    // todo, print a log for tmp;
    if (api_result != 0) {
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_API, "bm-sophon%d api process fail, error id is %d", bmdi->dev_index, api_id);
     }

	if (ti) {
		ti->profile.tpu_process_time += api_duration;
		ti->profile.completed_api_counter++;
		bmdi->profile.tpu_process_time += api_duration;
		bmdi->profile.completed_api_counter++;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_API, "post_api:completed_api_counter is %lld, api_duration is %d, tpu_process_time is %lld",
                    ti->profile.completed_api_counter, api_duration, ti->profile.tpu_process_time);

		if (ti->trace_enable) {
            WdfSpinLockAcquire(ti->trace_spinlock);
            int size = sizeof(struct bm_trace_item);
			ptitem = (struct bm_trace_item *)WdfMemoryGetBuffer(bmdi->trace_info.trace_memHandle, (size_t *)&size );
			ptitem->payload.trace_type = 1;
			ptitem->payload.api_id = api_entry.api_id;
			ptitem->payload.sent_time = api_entry.sent_time_us;

            u64         tick_count;
            LARGE_INTEGER CurTime, Freq;
            CurTime    = KeQueryPerformanceCounter(&Freq);
            tick_count = (CurTime.QuadPart * 1000000) / Freq.QuadPart;

            ptitem->payload.end_time = tick_count;
			ptitem->payload.start_time = ptitem->payload.end_time - ((u64)api_duration) * 4166;
            InitializeListHead(&ptitem->node);
            InsertTailList(&ptitem->node, &ti->trace_list);
			ti->trace_item_num++;
            WdfSpinLockRelease(ti->trace_spinlock);
		}
	}
}

static void bmdrv_sw_register_init(struct bm_device_info *bmdi)
{
	int channel = 0;

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		bmdi->api_info[channel].bm_api_init = bmdrv_api_init;
		bmdi->api_info[channel].bm_api_deinit = bmdrv_api_deinit;
	}
	bmdi->c_attr.bm_card_attr_init = bmdrv_card_attr_init;
	bmdi->memcpy_info.bm_memcpy_init = bmdrv_memcpy_init;
	bmdi->memcpy_info.bm_memcpy_deinit = bmdrv_memcpy_deinit;
	bmdi->trace_info.bm_trace_init = bmdrv_init_trace_pool;
	bmdi->trace_info.bm_trace_deinit = bmdrv_destroy_trace_pool;
	bmdi->gmem_info.bm_gmem_init = bmdrv_gmem_init;
	bmdi->gmem_info.bm_gmem_deinit = bmdrv_gmem_deinit;
}

int bmdrv_software_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	u32 channel = 0;
	struct chip_info *cinfo = &bmdi->cinfo;

    WDF_OBJECT_ATTRIBUTES attributes;
    NTSTATUS              status;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! enter\n");
    InitializeListHead(&bmdi->handle_list);

	bmdrv_sw_register_init(bmdi);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = bmdi->WdfDevice;
    ExInitializeFastMutex(&bmdi->clk_reset_mutex);
    ExInitializeFastMutex(&bmdi->spacc_mutex);

    status = WdfSpinLockCreate(&attributes, &bmdi->device_spinlock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

	if (bmdi->gmem_info.bm_gmem_init &&
		bmdi->gmem_info.bm_gmem_init(bmdi))
        return -1;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Gmem init finished\n");

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		if (bmdi->api_info[channel].bm_api_init &&
			bmdi->api_info[channel].bm_api_init(bmdi, channel))
            return -1;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Bm_api init finished\n");

	if (bmdi->c_attr.bm_card_attr_init &&
		bmdi->c_attr.bm_card_attr_init(bmdi))
        return -1;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Bm_card_attr init finished\n");

	if (bmdi->memcpy_info.bm_memcpy_init &&
		bmdi->memcpy_info.bm_memcpy_init(bmdi))
        return -1;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Bm_memcpy init finished\n");


	if (bmdi->trace_info.bm_trace_init &&
		bmdi->trace_info.bm_trace_init(bmdi))
        return -1;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Bm_trace init finished\n");

	bmdrv_print_cardinfo(cinfo);

	bmdi->enable_dyn_freq = 1;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! exit\n");
	return ret;
}

void bmdrv_software_deinit(struct bm_device_info *bmdi)
{
	u32 channel = 0;

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		if (bmdi->api_info[channel].bm_api_deinit)
			bmdi->api_info[channel].bm_api_deinit(bmdi, channel);
	}
	if (bmdi->memcpy_info.bm_memcpy_deinit)
		bmdi->memcpy_info.bm_memcpy_deinit(bmdi);

	if (bmdi->gmem_info.bm_gmem_deinit)
		bmdi->gmem_info.bm_gmem_deinit(bmdi);

	if (bmdi->trace_info.bm_trace_deinit)
		bmdi->trace_info.bm_trace_deinit(bmdi);
}


int bmdrv_class_create(void)
{
	return 0;
}

struct class *bmdrv_class_get(void)
{
	return NULL;
}

int bmdrv_class_destroy(void)
{
	return 0;
}