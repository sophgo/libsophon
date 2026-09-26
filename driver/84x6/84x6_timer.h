#ifndef _84X6_TIMER_H_
#define _84X6_TIMER_H_
#define BM84X6_TIMER_PERIOD_NS (40)
uint32_t bm84x6_timer_current_value(struct bm_device_info *bmdi);
uint32_t bm84x6_timer_get_time_ms(struct bm_device_info *bmdi);
uint32_t bm84x6_timer_get_time_us(struct bm_device_info *bmdi);
int bm84x6_timer_start(struct bm_device_info *bmdi);
void bm84x6_timer_stop(struct bm_device_info *bmdi);
#endif
