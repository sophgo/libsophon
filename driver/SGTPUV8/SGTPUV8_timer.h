#ifndef _SGTPUV8_TIMER_H_
#define _SGTPUV8_TIMER_H_
#define SGTPUV8_TIMER_PERIOD_NS (40)
uint32_t SGTPUV8_timer_current_value(struct bm_device_info *bmdi);
uint32_t SGTPUV8_timer_get_time_ms(struct bm_device_info *bmdi);
uint32_t SGTPUV8_timer_get_time_us(struct bm_device_info *bmdi);
int SGTPUV8_timer_start(struct bm_device_info *bmdi);
void SGTPUV8_timer_stop(struct bm_device_info *bmdi);
#endif
