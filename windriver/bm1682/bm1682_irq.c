#include "../bm_common.h"
#include "bm1682_reg.h"

void bm1682_get_irq_status(struct bm_device_info *bmdi, u32 *status)
{
	u32 irq_status;

	irq_status = gp_reg_read_enh(bmdi, GP_REG_CDMA_IRQSTATUS);
	if (irq_status == IRQ_STATUS_CDMA_INT) {
		status[1] |= 1 << 14;
		//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"The cdma irq received\n");
	}

	irq_status = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_IRQSTATUS);
	if (irq_status == IRQ_STATUS_MSG_DONE_INT) {
		status[1] |= 1 << 16;
		//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"The msg done irq received\n");
	}
}
