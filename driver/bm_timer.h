#ifndef _BM_TIMER_H_
#define _BM_TIMER_H_
#define BM_TIMER_PERIOD_NS (40)
struct bm_device_info;
struct timer_ctx {
	uint64_t timestamp;
	uint64_t timeout;
};
void bmdev_timer_init(struct bm_device_info *bmdi);
unsigned long bmdev_timer_get_cycle(struct bm_device_info *bmdi);
unsigned long bmdev_timer_get_time_ms(struct bm_device_info *bmdi);
unsigned long bmdev_timer_get_time_us(struct bm_device_info *bmdi);
int bmdrv_timer_start(struct bm_device_info *bmdi, struct timer_ctx *ctx, int timeout);
int bmdrv_timer_remain(struct bm_device_info *bmdi, struct timer_ctx *ctx);
void bmdrv_timer_stop(struct bm_device_info *bmdi, struct timer_ctx *ctx);
#endif
