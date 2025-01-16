#include <linux/delay.h>
#include "bm_common.h"
#include "bm_card.h"
#include "SGTPUV8_reg.h"
#include "SGTPUV8_irq.h"


#include <linux/reset.h>
#include <linux/clk.h>







int SGTPUV8_reset_tpu(struct bm_device_info *bmdi)
{
	u32 timeout_ms = bmdi->cinfo.delay_ms;
	u32 timeout;

	/*send irq 183 to C906_0/1, let it get ready to die*/
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_SET_OFFSET, 0x1 << 6);
	udelay(100);

	// set hau_flush_enable to 1 then check hau_flush_done
	timeout = timeout_ms * 1000;

	// set tpu0/1_stopper_enable to 1 then check cfg_stopper_done
	timeout = timeout_ms * 1000;
	tpu_reg_write_idx(bmdi, 0x15c, tpu_reg_read_idx(bmdi, 0x15c, 0) | (0x1 << 9), 0);
	while ((tpu_reg_read_idx(bmdi, 0x15c, 0) & (0x1 << 8)) == 0) {
		udelay(1);
		if (--timeout == 0) {
			pr_err("tpu0_stopper_done timeout\n");
			return -EBUSY;
		}
	}

	// set gdma0/1_sys_pulse to 1 then check pulse done
	timeout = timeout_ms * 1000;
	gdma_reg_write_idx(bmdi, 0x160, gdma_reg_read_idx(bmdi, 0x160, 0) | 0x1, 0);
	while ((gdma_reg_read_idx(bmdi, 0x160, 0) & (0xf << 12)) != (0xf << 12)) {
		udelay(1);
		if (--timeout == 0) {
			pr_err("gdma0_stopper_done timeout\n");
			return -EBUSY;
		}
	}

	// clear irq 183 in C906_0/1
	top_reg_write(bmdi, TOP_GP_REG_C906_IRQ_CLEAR_OFFSET, 0x1 << 6);

	// reset tpu0/1 gdma0/1 hau tc9060/1
	top_reg_write(bmdi, TOP_SW_RESET0, top_reg_read(bmdi, TOP_SW_RESET0) & ~0x3F800000);

        mdelay(1);
	pr_info("tpu sys reset\n");
	return 0;
}

int SGTPUV8_l2_sram_init(struct bm_device_info *bmdi)
{
	//bmdev_memcpy_s2d_internal(bmdi, L2_SRAM_START_ADDR + L2_SRAM_TPU_TABLE_OFFSET,
	//		l2_sram_table, sizeof(l2_sram_table));
	return 0;
}


void SGTPUV8_tpu_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 tpu0 reset\n");
	reset_control_assert(bmdi->cinfo.tpu0);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.tpu0);
}

void SGTPUV8_gdma_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 gdma0 reset\n");
	reset_control_assert(bmdi->cinfo.gdma0);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.gdma0);
}

void SGTPUV8_hau_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 hau reset\n");
	reset_control_assert(bmdi->cinfo.hau);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.hau);
}

void SGTPUV8_tpu_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 tpu clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.tpu_clk);
}

void SGTPUV8_tpu_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 tpu clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.tpu_clk);
}

void SGTPUV8_top_fab0_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 top_fab0 clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.top_fab0_clk);
}

void SGTPUV8_top_fab0_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 top_fab0 clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.top_fab0_clk);
}


void SGTPUV8_timer_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 timer clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.timer_clk);
}

void SGTPUV8_timer_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 timer clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.timer_clk);
}

void SGTPUV8_gdma_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 gdma clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.gdma_clk);
}

void SGTPUV8_gdma_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("SGTPUV8 gdma clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.gdma_clk);
}


