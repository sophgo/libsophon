#include "bm_common.h"
#include "bm_io.h"
#include "SGTPUV8_timer.h"
#include <linux/math64.h>

uint32_t SGTPUV8_timer_current_value(struct bm_device_info *bmdi)
{
	return nv_timer_reg_read(bmdi, 0x4);
}

uint32_t SGTPUV8_timer_get_time_ms(struct bm_device_info *bmdi)
{
	uint32_t cur_tick = 0xffffffff - SGTPUV8_timer_current_value(bmdi);
	return div_u64((cur_tick * SGTPUV8_TIMER_PERIOD_NS), 1000000);
}

uint32_t SGTPUV8_timer_get_time_us(struct bm_device_info *bmdi)
{
	uint32_t cur_tick = 0xffffffff - SGTPUV8_timer_current_value(bmdi);
	return div_u64((cur_tick * SGTPUV8_TIMER_PERIOD_NS), 1000);
}

int SGTPUV8_timer_start(struct bm_device_info *bmdi)
{
	uint32_t val = nv_timer_reg_read(bmdi, 0x8);
	nv_timer_reg_write(bmdi, 0x8, val | 0x5);
	return 0;
}

void SGTPUV8_timer_stop(struct bm_device_info *bmdi)
{
	uint32_t val = nv_timer_reg_read(bmdi, 0x8);
	nv_timer_reg_write(bmdi, 0x8, val & (~0x1));
}
