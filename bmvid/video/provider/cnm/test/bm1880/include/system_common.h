#ifndef _SYSTEM_COMMON_H_
#define _SYSTEM_COMMON_H_

#include <stdio.h>

#ifdef USE_NPU_LIB
#include "bm_types.h"
#else
#include "bmtest_type.h"
#endif
#include "fw_config.h"

static inline u32 float_to_u32(float x)
{
  union {
    int ival;
    float fval;
  } v = { .fval = x };
  return v.ival;
}

static inline float u32_to_float(u32 x)
{
  union {
    int ival;
    float fval;
  } v = { .ival = x };
  return v.fval;
}

#define array_len(a)              (sizeof(a) / sizeof(a[0]))

#define ALIGNMENT(x, a)              __ALIGNMENT_MASK((x), (typeof(x))(a)-1)
#define __ALIGNMENT_MASK(x, mask)    (((x)+(mask))&~(mask))
#define PTR_ALIGNMENT(p, a)         ((typeof(p))ALIGNMENT((unsigned long)(p), (a)))
#define IS_ALIGNMENT(x, a)        (((x) & ((typeof(x))(a) - 1)) == 0)

#define __raw_readb(a)          (*(volatile unsigned char *)(a))
#define __raw_readw(a)          (*(volatile unsigned short *)(a))
#define __raw_readl(a)          (*(volatile unsigned int *)(a))
#define __raw_readq(a)          (*(volatile unsigned long long *)(a))

#define __raw_writeb(a,v)       (*(volatile unsigned char *)(a) = (v))
#define __raw_writew(a,v)       (*(volatile unsigned short *)(a) = (v))
#define __raw_writel(a,v)       (*(volatile unsigned int *)(a) = (v))
#define __raw_writeq(a,v)       (*(volatile unsigned long long *)(a) = (v))

#define readb(a)        __raw_readb(a)
#define readw(a)        __raw_readw(a)
#define readl(a)        __raw_readl(a)
#define readq(a)        __raw_readq(a)

#define writeb(a, v)        __raw_writeb(a,v)
#define writew(a, v)        __raw_writew(a,v)
#define writel(a, v)        __raw_writel(a,v)
#define writeq(a, v)        __raw_writeq(a,v)

#define cpu_write8(a, v)    writeb(a, v)
#define cpu_write16(a, v)    writew(a, v)
#define cpu_write32(a, v)    writel(a, v)

#define cpu_read8(a)        readb(a)
#define cpu_read16(a)        readw(a)
#define cpu_read32(a)        readl(a)

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#ifdef ENABLE_DEBUG
#define debug(fmt, args...)    printf(fmt, ##args)
#else
#define debug(...)
#endif

#ifdef ENABLE_PRINT
#define uartlog(fmt, args...)    printf(fmt, ##args)
#else
#define uartlog(...)
#endif

#if 0
extern u32 debug_level;
#define debug_out(flag, fmt, args...)           \
  do {                                          \
    if (flag <= debug_level)                    \
      printf(fmt, ##args);                      \
  } while (0)
#endif

#ifdef USE_BMTAP
#define call_atomic(nodechip_idx, atomic_func, p_command, eng_id)       \
  emit_task_descriptor(p_command, eng_id)
#endif

/* irq */
#define IRQ_LEVEL   0
#define IRQ_EDGE    3

#define PERI_UART3_INTR     50
#define PERI_UART2_INTR     47
#define PERI_UART1_INTR     44
#define PERI_UART0_INTR     41
#define USB_OTG_INTR        114

#define IRQF_TRIGGER_NONE    0x00000000
#define IRQF_TRIGGER_RISING  0x00000001
#define IRQF_TRIGGER_FALLING 0x00000002
#define IRQF_TRIGGER_HIGH    0x00000004
#define IRQF_TRIGGER_LOW     0x00000008
#define IRQF_TRIGGER_MASK   (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW | \
                         IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)

typedef int (*irq_handler_t)(int irqn, void *priv);

extern int request_irq(unsigned int irqn, irq_handler_t handler, unsigned long flags,
        const char *name, void *priv);

void disable_irq(unsigned int irqn);
void enable_irq(unsigned int irqn);

void cpu_enable_irqs(void);
void cpu_disable_irqs(void);

extern void irq_trigger(int irqn);
extern void irq_clear(int irqn);
extern int irq_get_nums(void);

#endif
