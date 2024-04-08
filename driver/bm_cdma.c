#include "bm_common.h"
void bmdrv_cdma_irq_handler(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bmdrv_clear_cdmairq(bmdi);
	complete(&bmdi->memcpy_info.cdma_done);
}
#ifdef SOC_MODE
irqreturn_t bmdrv_irq_handler_cdma(int irq, void *data)
{
	struct bm_device_info *bmdi = data;

	PR_TRACE("bmdrv: cdma irq received.\n");
	bmdrv_cdma_irq_handler(bmdi);
	return IRQ_HANDLED;
}
#endif

#ifndef SOC_MODE
void bm_cdma_request_irq(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1686a200)
		bm1688_dual_cdma_init(bmdi);
	else
		bmdrv_submodule_request_irq(bmdi, CDMA_IRQ_ID, bmdrv_cdma_irq_handler);
}

void bm_cdma_free_irq(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1686a200)
		bm1688_dual_cdma_remove(bmdi);
	else
		bmdrv_submodule_free_irq(bmdi, CDMA_IRQ_ID);
}
#endif
