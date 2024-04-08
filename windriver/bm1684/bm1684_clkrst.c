#include "../bm_common.h"
#include "bm1684_reg.h"
#include "bm1684_clkrst.tmh"

static int bmdrv_1684_fvco_check(int fref, int fbdiv, int refdiv)
{
	int foutvco = (fref * fbdiv) / refdiv;
	if((foutvco >= 800) && (foutvco <= 3200))
		return 1;
	else
		return 0;
}

static int bmdrv_1684_target_check(int target)
{
	if((target > 16) && (target < 3200))
		return 1;
	else
		return 0;
}

static int bmdrv_1684_pfd_check(int fref, int refdiv)
{
	if(fref/refdiv < 10)
		return 0;
	else
		return 1;
}

static int bmdrv_1684_postdiv_check(int postdiv1, int postdiv2)
{
	if(postdiv1 < postdiv2)
		return 0;
	else
		return 1;
}

static int bmdrv_1684_cal_tpll_para(int target_freq, int* fbdiv, int* refdiv,
                             int* postdiv1, int* postdiv2)
{
	int refdiv_index = 0;
	int fbdiv_index = 0;
	int postdiv1_index = 0;
	int postdiv2_index = 0;
	int success = 0;
	int temp_target = 0;

	if(!bmdrv_1684_target_check(target_freq)) return 0;

	for(refdiv_index=BM1684_TPLL_REFDIV_MIN; refdiv_index<=BM1684_TPLL_REFDIV_MAX; refdiv_index++)
		for(fbdiv_index=BM1684_TPLL_FBDIV_MIN; fbdiv_index<=BM1684_TPLL_FBDIV_MAX; fbdiv_index++)
			for(postdiv1_index=BM1684_TPLL_POSTDIV1_MIN; postdiv1_index<=BM1684_TPLL_POSTDIV1_MAX;postdiv1_index++)
				for(postdiv2_index=BM1684_TPLL_POSTDIV2_MIN; postdiv2_index<=BM1684_TPLL_POSTDIV2_MAX;postdiv2_index++) {
					temp_target = BM1684_TPLL_FREF * fbdiv_index/refdiv_index/postdiv1_index/postdiv2_index;
					if(temp_target == target_freq) {
						if(bmdrv_1684_fvco_check(BM1684_TPLL_FREF, fbdiv_index, refdiv_index) &&
               bmdrv_1684_pfd_check(BM1684_TPLL_FREF, refdiv_index) &&
               bmdrv_1684_postdiv_check(postdiv1_index, postdiv2_index)) {
							*fbdiv = fbdiv_index;
							*refdiv = refdiv_index;
							*postdiv1 = postdiv1_index;
							*postdiv2 = postdiv2_index;
							success = 1;
							break;
						}
						else
							continue;
					}
				}

	return success;
}

int bmdev_clk_hwlock_lock(struct bm_device_info* bmdi)
{
	u32 timeout_count = bmdi->cinfo.delay_ms*1000;

	while (top_reg_read(bmdi, TOP_CLK_LOCK)) {
		bm_udelay(1);
		if (--timeout_count == 0) {
            //TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "wait clk hwlock timeout\n");
			return -1;
		}
	}

	return 0;
}

void bmdev_clk_hwlock_unlock(struct bm_device_info* bmdi)
{
	top_reg_write(bmdi, TOP_CLK_LOCK, 0);
}

static int tpll_ctl_reg = 1 | 2<< 8 | 1 << 12 | 44<< 16;
int bmdrv_clk_set_tpu_target_freq(struct bm_device_info *bmdi, int target)
{
	int fbdiv = 0;
	int refdiv = 0;
	int postdiv1 = 0;
	int postdiv2 = 0;
	int val = 0;

	if (bmdi->misc_info.pcie_soc_mode == 0) {
		if((u32)target<bmdi->boot_info.tpu_min_clk ||
			(u32)target>bmdi->boot_info.tpu_max_clk) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,"freq %d is too small or large\n", target);
			return -1;
		}
	}

	if(!bmdrv_1684_cal_tpll_para(target, &fbdiv, &refdiv, &postdiv1, &postdiv2)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "not support freq %d\n",target);
		return -1;
	}

	if (bmdi->cinfo.chip_id == 0x1686) {
		val = top_reg_read(bmdi, TOP_CLK_SELECT);
		val &= ~(1 << 1);
		top_reg_write(bmdi, TOP_CLK_SELECT, val);
		bm_udelay(1);
	} else {
		if (bmdev_clk_hwlock_lock(bmdi) != 0)
		{
	        TraceEvents(TRACE_LEVEL_ERROR,
	                    TRACE_DEVICE,
	                    "get clk hwlock fail\n");
			return -1;
		}
	}

	/* gate */
	val = top_reg_read(bmdi, TOP_PLL_ENABLE);
	val &= ~(1 << 1);
	top_reg_write(bmdi, TOP_PLL_ENABLE, val);
	bm_udelay(1);

	/* Modify TPLL Control Register */
	val = top_reg_read(bmdi, TOP_TPLL_CTL);
	if (bmdi->cinfo.platform == PALLADIUM) {
		val = tpll_ctl_reg;
	}
	val &= 0xf << 28;
	val |= refdiv & 0x1f;
	val |= (postdiv1 & 0x7) << 8;
	val |= (postdiv2 & 0x7) << 12;
	val |= (fbdiv & 0xfff) << 16;
	top_reg_write(bmdi, TOP_TPLL_CTL, val);
	if (bmdi->cinfo.platform == PALLADIUM) {
		tpll_ctl_reg = val;
	}
	bm_udelay(1);

	/* poll status until bit9=1 adn bit1=0 */
	if (bmdi->cinfo.platform == DEVICE) {
		val = top_reg_read(bmdi, TOP_PLL_STATUS);
		while((val&0x202) != 0x200) {
			bm_udelay(1);
			val = top_reg_read(bmdi, TOP_PLL_STATUS);
		}
	}

	if (bmdi->cinfo.chip_id == 0x1686) {
		val = top_reg_read(bmdi, TOP_CLK_SELECT);
		val |= (1 << 1);
		top_reg_write(bmdi, TOP_CLK_SELECT, val);
	} else {
		/* ungate */
		val = top_reg_read(bmdi, TOP_PLL_ENABLE);
		val |= 1 << 1;
		top_reg_write(bmdi, TOP_PLL_ENABLE, val);

		bmdev_clk_hwlock_unlock(bmdi);
	}

	return 0;
}

int bmdrv_1684_clk_get_tpu_freq(struct bm_device_info *bmdi)
{
	int fbdiv = 0;
	int refdiv = 0;
	int postdiv1 = 0;
	int postdiv2 = 0;
	int val = 0;
	int fout = 0;

	/* Modify TPLL Control Register */
	val = top_reg_read(bmdi, TOP_TPLL_CTL);
	if (bmdi->cinfo.platform == PALLADIUM) {
		val = tpll_ctl_reg;
	}
	refdiv = val & 0x1f;
	postdiv1 = (val >> 8) & 0x7;
	postdiv2 = (val >> 12) & 0x7;
	fbdiv = (val >> 16) & 0xfff;

	fout = 1000*BM1684_TPLL_FREF*fbdiv/refdiv/postdiv1/postdiv2;
	return fout/1000;
}

void bmdrv_clk_set_tpu_divider(struct bm_device_info *bmdi, int divider_factor)
{
	int val = 0;

	if (bmdev_clk_hwlock_lock(bmdi) != 0)
	{
        //TraceEvents(TRACE_LEVEL_ERROR,
        //            TRACE_DEVICE,
        //            "get clk hwlock fail\n");
		return;
	}

	/* gate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val &= ~(1 << BIT_CLK_TPU_ENABLE);
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);
	bm_udelay(1);

	/* assert */
	val = top_reg_read(bmdi, TOP_CLK_DIV0);
	val &= ~(1 << BIT_CLK_TPU_ASSERT);
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	bm_udelay(1);

	/* change divider */
	val &= ~(0x1f << 16);
	val |= (divider_factor & 0x1f) << 16;
	val |= 1 << 3;
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	bm_udelay(1);

	/* deassert */
	val |= 1;
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	bm_udelay(1);

	/* ungate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val |= 1 << BIT_CLK_TPU_ENABLE;
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);

	bmdev_clk_hwlock_unlock(bmdi);
}

int bmdrv_clk_get_tpu_divider(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_CLK_DIV0);
	return (val >> 16) & 0x1f;
}

int bmdev_clk_ioctl_set_tpu_divider(struct bm_device_info* bmdi, unsigned long arg)
{
    UNREFERENCED_PARAMETER(bmdi);
    UNREFERENCED_PARAMETER(arg);
	//int divider = 0;

	//if(get_user(divider, (u64  *)arg)) {
 //       TraceEvents(
 //           TRACE_LEVEL_ERROR,
 //           TRACE_DEVICE,
 //           "bmdrv: bmdev_clk_ioctl_set_tpu_divider get user failed!\n");
	//	return -1;
	//} else {
	//	//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bmdrv_clk_set_tpu_divider: divider=%d\n", divider);
	//	bmdrv_clk_set_tpu_divider(bmdi, divider);
	//	return 0;
	//}
    return 0;
}

int bmdev_clk_ioctl_get_tpu_freq(struct bm_device_info* bmdi)
{
	int freq = 0;

	freq = bmdrv_1684_clk_get_tpu_freq(bmdi);
	return freq;
}

void bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_disable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_disable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_disable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_sw_reset_tpu(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_TPU);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	bm_udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_TPU;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bmdrv_sw_reset_axi_sram(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_AXI_SRAM);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	bm_udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_AXI_SRAM;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bmdrv_sw_reset_gdma(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_GDMA);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	bm_udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_GDMA;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bmdrv_sw_reset_smmu(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val &= ~(1 << BIT_SW_RESET_SMMU);
	top_reg_write(bmdi, TOP_SW_RESET1, val);
	bm_udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val |= 1 << BIT_SW_RESET_SMMU;
	top_reg_write(bmdi, TOP_SW_RESET1, val);
}

void bmdrv_sw_reset_cdma(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val &= ~(1 << BIT_SW_RESET_CDMA);
	top_reg_write(bmdi, TOP_SW_RESET1, val);
	bm_udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val |= 1 << BIT_SW_RESET_CDMA;
	top_reg_write(bmdi, TOP_SW_RESET1, val);
}

void bmdrv_clk_set_module_reset(struct bm_device_info* bmdi, MODULE_ID module)
{
	switch (module) {
	case MODULE_CDMA:
		bmdrv_sw_reset_cdma(bmdi);
		break;
	case MODULE_GDMA:
		bmdrv_sw_reset_gdma(bmdi);
		break;
	case MODULE_TPU:
		bmdrv_sw_reset_tpu(bmdi);
		break;
	case MODULE_SMMU:
		bmdrv_sw_reset_smmu(bmdi);
		break;
	case MODULE_SRAM:
		bmdrv_sw_reset_axi_sram(bmdi);
		break;
	default:
		break;
	}
}

int bmdev_clk_ioctl_set_module_reset(struct bm_device_info *bmdi, _In_ WDFREQUEST        Request) {
    int             ret = 0;
    size_t          bufSize;
    PVOID           inDataBuffer;
	MODULE_ID module = (MODULE_ID)0;

    NTSTATUS Status = WdfRequestRetrieveInputBuffer(
        Request, sizeof(MODULE_ID), &inDataBuffer, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestCompleteWithInformation(Request, Status, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_CLK_RESET,
                    "%!FUNC! get input info failed\n");
        return -1;
    }

    RtlCopyMemory( &module, inDataBuffer, sizeof(MODULE_ID));
	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_CLK_RESET,
            "bmdrv_clk_set_module_reset: module=%d\n",
            module);

    bmdrv_clk_set_module_reset(bmdi, module);
    ret = 0;

	if (ret) {
        WdfRequestCompleteWithInformation(Request, STATUS_UNSUCCESSFUL, 0);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_CLK_RESET,
                    "%!FUNC! bmdrv_clk_set_module_reset failed\n");
        return -1;
    }

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0);
    return 0;
}