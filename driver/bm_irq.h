#ifndef _BM_IRQ_H_
#define _BM_IRQ_H_


#include <linux/platform_device.h>
#include <linux/interrupt.h>
int bmdrv_init_irq(struct platform_device *pdev);
void bmdrv_free_irq(struct platform_device *pdev);

#endif
