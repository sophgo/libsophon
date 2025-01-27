#include "bm_common.h"
void bmdrv_cdma_irq_handler0(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bmdrv_clear_cdmairq0(bmdi);
	complete(&bmdi->memcpy_info.cdma_done0);
}

void bmdrv_cdma_irq_handler1(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bmdrv_clear_cdmairq1(bmdi);
	complete(&bmdi->memcpy_info.cdma_done1);
}

#ifdef SOC_MODE
irqreturn_t bmdrv_irq_handler_cdma0(int irq, void *data)
{
	struct bm_device_info *bmdi = data;

	PR_TRACE("bmdrv: cdma irq received.\n");
	bmdrv_cdma_irq_handler0(bmdi);
	return IRQ_HANDLED;
}

irqreturn_t bmdrv_irq_handler_cdma1(int irq, void *data)
{
	struct bm_device_info *bmdi = data;

	PR_TRACE("bmdrv: cdma irq received.\n");
	bmdrv_cdma_irq_handler1(bmdi);
	return IRQ_HANDLED;
}

irqreturn_t bmdrv_irq_handler_cdma(int irq, void *data)
{
	struct bm_device_info *bmdi = data;

	PR_TRACE("bmdrv: cdma irq received.\n");
	bmdrv_cdma_irq_handler0(bmdi);
	return IRQ_HANDLED;
}
#endif

#ifndef SOC_MODE
void bm_cdma_request_irq(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1686a200)
		bm1688_dual_cdma_init(bmdi);
	else
		bmdrv_submodule_request_irq(bmdi, CDMA_IRQ_ID, bmdrv_cdma_irq_handler0);
}

void bm_cdma_free_irq(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1686a200)
		bm1688_dual_cdma_remove(bmdi);
	else
		bmdrv_submodule_free_irq(bmdi, CDMA_IRQ_ID);
}
#endif
