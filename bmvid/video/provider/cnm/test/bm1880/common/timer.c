#include "system_common.h"
#include "timer.h"

void timer_init(void)
{
}

u64 timer_get_tick(void)
{
  u64 cntpct;
  asm volatile("mrs %0, cntpct_el0" : "=r" (cntpct));
  return cntpct;
}

void timer_mdelay(u32 ms)
{
  u64 cnt = TIMER_CNT_MS * ms;
  u64 start_tick = timer_get_tick();
  u64 exp_tick = start_tick + cnt;
  u64 val;
  do {
    val = timer_get_tick();
  } while(val < exp_tick);
}

void timer_udelay(u32 us)
{
  u64 cnt = TIMER_CNT_US * us;
  u64 start_tick = timer_get_tick();
  u64 exp_cnt = start_tick + cnt;
  u64 val;
  do {
    val = timer_get_tick();
  } while(val < exp_cnt);
}

static u64 timer_meter_start_cnt = 0;

void timer_meter_start(void)
{
  timer_meter_start_cnt = timer_get_tick();
}

u64 timer_meter_get_tick(void)
{
  return (timer_get_tick() - timer_meter_start_cnt);
}

u32 timer_meter_get_ms(void)
{
  return (timer_get_tick() - timer_meter_start_cnt) / TIMER_CNT_MS;
}
