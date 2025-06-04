#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include "bm_common.h"
#include "bm_clkrst.h"


int SGTPUV8_modules_reset_init(struct bm_device_info *bmdi)
{

	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo = &bmdi->cinfo;
	int ret = 0;

	cinfo->tpusys = devm_reset_control_get(dev, "res_tpusys");
	if (IS_ERR(cinfo->tpusys))
	{
		ret = PTR_ERR(cinfo->tpusys);
		dev_err(dev, "failed to retrieve tpusys reset");
		return ret;
	}
	cinfo->tpu = devm_reset_control_get(dev, "res_tpu");
	if (IS_ERR(cinfo->tpu))
	{
		ret = PTR_ERR(cinfo->tpu);
		dev_err(dev, "failed to retrieve tpu0 reset");
		return ret;
	}
	cinfo->gdma = devm_reset_control_get(dev, "res_gdma");
	if (IS_ERR(cinfo->gdma))
	{
		ret = PTR_ERR(cinfo->gdma);
		dev_err(dev, "failed to retrieve gdma0 reset");
		return ret;
	}
	return ret;
}

void SGTPUV8_modules_reset(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.tpu && bmdi->cinfo.gdma) {
		PR_TRACE("SGTPUV8 reset\n");
		reset_control_assert(bmdi->cinfo.tpu);
		udelay(1000);
		reset_control_deassert(bmdi->cinfo.tpu);

		reset_control_assert(bmdi->cinfo.gdma);
		udelay(1000);
		reset_control_deassert(bmdi->cinfo.gdma);

		reset_control_assert(bmdi->cinfo.tpusys);
		udelay(1000);
		reset_control_deassert(bmdi->cinfo.tpusys);
	}
}

int SGTPUV8_modules_clk_init(struct bm_device_info *bmdi)
{
	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo = &bmdi->cinfo;
	int ret = 0;

	cinfo->tpu_clk = devm_clk_get(dev, "clk_tpu_axi");
	if (IS_ERR(cinfo->tpu_clk))
	{
		ret = PTR_ERR(cinfo->tpu_clk);
		dev_err(dev, "failed to retrieve tpu clk");
		return ret;
	}
	cinfo->top_fab0_clk = devm_clk_get(dev, "clk_tpu_sys");
	if (IS_ERR(cinfo->top_fab0_clk))
	{
		ret = PTR_ERR(cinfo->top_fab0_clk);
		dev_err(dev, "failed to retrieve top_fab0 clk");
		return ret;
	}
	cinfo->gdma_clk = devm_clk_get(dev, "clk_tpu_gdma");
	if (IS_ERR(cinfo->gdma_clk))
	{
		ret = PTR_ERR(cinfo->gdma_clk);
		dev_err(dev, "failed to retrieve gdma_clk clk");
		return ret;
	}
	return 0;
}

void SGTPUV8_modules_clk_deinit(struct bm_device_info *bmdi)
{
	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo = &bmdi->cinfo;

	devm_clk_put(dev, cinfo->tpu_clk);
	devm_clk_put(dev, cinfo->top_fab0_clk);
	// devm_clk_put(dev, cinfo->timer_clk);
	devm_clk_put(dev, cinfo->gdma_clk);
}

void SGTPUV8_modules_clk_enable(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.tpu_clk && bmdi->cinfo.top_fab0_clk) {
		PR_TRACE("enable tpu clk\n");
		clk_prepare_enable(bmdi->cinfo.tpu_clk);
		clk_prepare_enable(bmdi->cinfo.top_fab0_clk);
		clk_prepare_enable(bmdi->cinfo.gdma_clk);
	}
}

void SGTPUV8_modules_clk_disable(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.tpu_clk && bmdi->cinfo.top_fab0_clk) {
		PR_TRACE("SGTPUV8 clk is gating\n");
		clk_disable_unprepare(bmdi->cinfo.tpu_clk);
		clk_disable_unprepare(bmdi->cinfo.top_fab0_clk);
	}
}

