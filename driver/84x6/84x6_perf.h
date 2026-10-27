#ifndef _84X6_PERF_H_
#define _84X6_PERF_H_
struct bm_device_info;
struct bm_perf_monitor;
void bm84x6_enable_tpu_perf_monitor(struct bm_device_info *bmdi,
                                    struct bm_perf_monitor *perf_monitor,
                                    int core_id);
void bm84x6_disable_tpu_perf_monitor(struct bm_device_info *bmdi, int core_id);
void bm84x6_enable_gdma_perf_monitor(struct bm_device_info *bmdi,
                                     struct bm_perf_monitor *perf_monitor,
                                     int core_id);
void bm84x6_disable_gdma_perf_monitor(struct bm_device_info *bmdi, int core_id);
#endif
