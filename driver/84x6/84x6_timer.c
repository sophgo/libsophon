#include "bm_common.h"
#include "bm_io.h"
#include "84x6_timer.h"

#ifdef SOC_MODE
#include <linux/time.h>
uint32_t bm84x6_timer_current_value(struct bm_device_info *bmdi)
{
#if 0                           //open it when needed
    struct timeval time_now = do_gettimeofday(&tstart);
    return (uint32_t) (time_now.tv_sec * 1000000 + time_now.tv_usec);
#endif
    return 0;
}

uint32_t bm84x6_timer_get_time_ms(struct bm_device_info *bmdi)
{
    return bm84x6_timer_current_value(bmdi) / 1000;
}

uint32_t bm84x6_timer_get_time_us(struct bm_device_info *bmdi)
{
    return bm84x6_timer_current_value(bmdi);
}

int bm84x6_timer_start(struct bm_device_info *bmdi)
{
    return 0;
}

void bm84x6_timer_stop(struct bm_device_info *bmdi)
{
}
#else
uint32_t bm84x6_timer_current_value(struct bm_device_info *bmdi)
{
    return nv_timer_reg_read(bmdi, 0x4);
}

uint32_t bm84x6_timer_get_time_ms(struct bm_device_info *bmdi)
{
    uint32_t cur_tick = 0xffffffff - bm84x6_timer_current_value(bmdi);
    return (cur_tick * BM84X6_TIMER_PERIOD_NS) / 1000000;
}

uint32_t bm84x6_timer_get_time_us(struct bm_device_info *bmdi)
{
    uint32_t cur_tick = 0xffffffff - bm84x6_timer_current_value(bmdi);
    return (cur_tick * BM84X6_TIMER_PERIOD_NS) / 1000;
}

int bm84x6_timer_start(struct bm_device_info *bmdi)
{
    uint32_t val = nv_timer_reg_read(bmdi, 0x8);
    nv_timer_reg_write(bmdi, 0x8, val | 0x5);
    return 0;
}

void bm84x6_timer_stop(struct bm_device_info *bmdi)
{
    uint32_t val = nv_timer_reg_read(bmdi, 0x8);
    nv_timer_reg_write(bmdi, 0x8, val & (~0x1));
}
#endif
