#ifndef _TIMER_H_
#define _TIMER_H_

#define TIMER_CNT_US    (CONFIG_TIMER_FREQ / (1000 * 1000))
#define TIMER_CNT_MS    (CONFIG_TIMER_FREQ / (1000))

void timer_init(void);

/* tick */
uint64_t timer_get_tick(void);

/* delay */
void timer_mdelay(uint32_t ms);
void timer_udelay(uint32_t us);

/* meter */
void timer_meter_start(void);
uint32_t timer_meter_get_ms(void);
uint64_t timer_meter_get_tick(void);

#define mdelay(__ms)    timer_mdelay(__ms)
#define udelay(__us)    timer_udelay(__us)

#endif
