#ifndef _SGTPUV8_PERF_H_
#define _SGTPUV8_PERF_H_
struct bm_device_info;
struct bm_perf_monitor;
void SGTPUV8_enable_tpu_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor, int core_id);
void SGTPUV8_disable_tpu_perf_monitor(struct bm_device_info *bmdi, int core_id);
void SGTPUV8_enable_gdma_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor, int core_id);
void SGTPUV8_disable_gdma_perf_monitor(struct bm_device_info *bmdi, int core_id);
#endif
