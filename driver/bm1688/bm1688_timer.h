#ifndef _BM1688_TIMER_H_
#define _BM1688_TIMER_H_
#define BM1688_TIMER_PERIOD_NS (40)
uint32_t bm1688_timer_current_value(struct bm_device_info *bmdi);
uint32_t bm1688_timer_get_time_ms(struct bm_device_info *bmdi);
uint32_t bm1688_timer_get_time_us(struct bm_device_info *bmdi);
int bm1688_timer_start(struct bm_device_info *bmdi);
void bm1688_timer_stop(struct bm_device_info *bmdi);
#endif
