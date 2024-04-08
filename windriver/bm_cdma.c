#include "bm_common.h"

#include "bm_cdma.tmh"

void bmdrv_cdma_irq_handler(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bmdrv_clear_cdmairq(bmdi);
    KeSetEvent(&bmdi->memcpy_info.cdma_completionEvent, 0, FALSE);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CDMA, "%!FUNC! cdma irq done,pid = 0x%llx\n",(u64)PsGetCurrentThreadId());
}

void bm_cdma_request_irq(struct bm_device_info *bmdi)
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "%!FUNC! irqnum=%d\n",
                CDMA_IRQ_ID);
	bmdrv_submodule_request_irq(bmdi, CDMA_IRQ_ID, bmdrv_cdma_irq_handler);
}

void bm_cdma_free_irq(struct bm_device_info *bmdi)
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "%!FUNC! irqnum=%d\n",
                CDMA_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, CDMA_IRQ_ID);
}
