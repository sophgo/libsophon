#include "bm_common.h"
#include "bm_io.h"
#include "bm1688_irq.h"

#ifndef SOC_MODE
int bm1688_clear_msgirq(struct bm_device_info *bmdi, int core_id)
{
	int irq_status = 0;

	if (bmdi->cinfo.irq_id == MSG_IRQ_ID_CHANNEL_XPU_A2_0) {
		irq_status = top_reg_read(bmdi, TOP_GP_REG_C906_IRQ_STATUS_OFFSET);

		if ((irq_status & (MSG_DONE_XPU_IRQ_MASK_0))) {
			top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, MSG_DONE_XPU_IRQ_MASK_0);
		} else {
			return -1;
		}
	} else if (bmdi->cinfo.irq_id == MSG_IRQ_ID_CHANNEL_XPU_A2_1) {
		irq_status = top_reg_read(bmdi, TOP_GP_REG_C906_IRQ_STATUS_OFFSET);

		if ((irq_status & (MSG_DONE_XPU_IRQ_MASK_1))) {
			top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, MSG_DONE_XPU_IRQ_MASK_1);
		} else {
			return -1;
		}
	} else {
		irq_status = top_reg_read(bmdi, TOP_GP_REG_C906_IRQ_STATUS_OFFSET);

		if ((irq_status & (MSG_DONE_CPU_IRQ_MASK))) {
			top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, MSG_DONE_CPU_IRQ_MASK);
		} else {
			return -1;
		}
	}

	return 0;
}
#else
int bm1688_clear_msgirq(struct bm_device_info *bmdi, int core_id)
{
	int irq_status = top_reg_read(bmdi, BM1688_GP_REG_A53_IRQ_STATUS_OFFSET);

	if ((core_id == 0) && (irq_status & (MSG_DONE_A53_IRQ_MASK_0))) {
		top_reg_write(bmdi, BM1688_GP_REG_A53_IRQ_CLEAR_OFFSET, MSG_DONE_A53_IRQ_MASK_0);
	} else if ((core_id == 1) && (irq_status & (MSG_DONE_A53_IRQ_MASK_1))) {
		top_reg_write(bmdi, BM1688_GP_REG_A53_IRQ_CLEAR_OFFSET, MSG_DONE_A53_IRQ_MASK_1);
	} else {
		return -1;
	}

	return 0;
}
#endif

u32 bm1688_pending_msgirq_cnt(struct bm_device_info *bmdi)
{
	return 1;
}
