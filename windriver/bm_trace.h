#ifndef _BM_TRACE_H_
#define _BM_TRACE_H_

 struct bm_trace_item_data {
	int trace_type;
	u64 sent_time;
	u64 start_time;
	u64 end_time;
	int api_id;
	MEMCPY_DIR cdma_dir;
};

struct bm_trace_item {
	LIST_ENTRY node;
	struct bm_trace_item_data payload;
};

struct bm_trace_info {
    WDFMEMORY    trace_memHandle;
	int (*bm_trace_init)(struct bm_device_info *);
	void (*bm_trace_deinit)(struct bm_device_info *);
};

int bmdrv_init_trace_pool(struct bm_device_info *bmdi);
void bmdrv_destroy_trace_pool(struct bm_device_info *bmdi);
int  bmdev_trace_enable(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);
int  bmdev_trace_disable(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);
int bmdev_traceitem_number(struct bm_device_info *bmdi, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength);
int bmdev_trace_dump_one(struct bm_device_info *bmdi, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength);
int  bmdev_trace_dump_all(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);
int bmdev_enable_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor);
int bmdev_disable_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor);
#endif
