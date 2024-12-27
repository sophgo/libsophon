#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm1688_clkrst.h"
#include "bm1688_reg.h"
#include "bm1688_card.h"
#include "bm1688_cdma.h"
#include "bm1688_smmu.h"
#include "bm_fw.h"
#include "bm_api.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#endif

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

	for(refdiv_index=BM1688_TPLL_REFDIV_MIN; refdiv_index<=BM1688_TPLL_REFDIV_MAX; refdiv_index++)
		for(fbdiv_index=BM1688_TPLL_FBDIV_MIN; fbdiv_index<=BM1688_TPLL_FBDIV_MAX; fbdiv_index++)
			for(postdiv1_index=BM1688_TPLL_POSTDIV1_MIN; postdiv1_index<=BM1688_TPLL_POSTDIV1_MAX;postdiv1_index++)
				for(postdiv2_index=BM1688_TPLL_POSTDIV2_MIN; postdiv2_index<=BM1688_TPLL_POSTDIV2_MAX;postdiv2_index++) {
					temp_target = BM1688_TPLL_FREF * fbdiv_index/refdiv_index/postdiv1_index/postdiv2_index;
					if(temp_target == target_freq) {
						if(bmdrv_1684_fvco_check(BM1688_TPLL_FREF, fbdiv_index, refdiv_index) &&
               bmdrv_1684_pfd_check(BM1688_TPLL_FREF, refdiv_index) &&
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

int bm1688_bmdev_clk_hwlock_lock(struct bm_device_info* bmdi)
{
	u32 timeout_count = bmdi->cinfo.delay_ms*1000;

	while (top_reg_read(bmdi, TOP_CLK_LOCK)) {
		udelay(1);
		if (--timeout_count == 0) {
			pr_err("wait clk hwlock timeout\n");
			return -1;
		}
	}

	return 0;
}

void bm1688_bmdev_clk_hwlock_unlock(struct bm_device_info* bmdi)
{
	top_reg_write(bmdi, TOP_CLK_LOCK, 0);
}

static int tpll_ctl_reg = 1 | 2<< 8 | 1 << 12 | 44<< 16;
int bm1688_bmdrv_clk_set_tpu_target_freq(struct bm_device_info *bmdi, int target)
{
	int fbdiv = 0;
	int refdiv = 0;
	int postdiv1 = 0;
	int postdiv2 = 0;
	int val = 0;

	if (bmdi->misc_info.pcie_soc_mode == 0) {
		if(target<bmdi->boot_info.tpu_min_clk ||
			target>bmdi->boot_info.tpu_max_clk) {
			pr_err("%s: freq %d is too small or large\n", __func__, target);
			return -1;
		}
	}

	if(!bmdrv_1684_cal_tpll_para(target, &fbdiv, &refdiv, &postdiv1, &postdiv2)) {
		pr_err("%s: not support freq %d\n", __func__, target);
		return -1;
	}

	if (bmdi->cinfo.chip_id == 0x1686) {
		val = top_reg_read(bmdi, TOP_CLK_SELECT);
		val &= ~(1 << 1);
		top_reg_write(bmdi, TOP_CLK_SELECT, val);
		udelay(1);
	} else {
		if (bm1688_bmdev_clk_hwlock_lock(bmdi) != 0)
		{
			pr_err("%s: get clk hwlock fail\n", __func__);
			return -1;
		}

		/* gate */
		val = top_reg_read(bmdi, TOP_PLL_ENABLE);
		val &= ~(1 << 1);
		top_reg_write(bmdi, TOP_PLL_ENABLE, val);
		udelay(1);
	}

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
	udelay(1);

	/* poll status until bit9=1 adn bit1=0 */
	if (bmdi->cinfo.platform == DEVICE) {
		val = top_reg_read(bmdi, TOP_PLL_STATUS);
		while((val&0x202) != 0x200) {
			udelay(1);
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

		bm1688_bmdev_clk_hwlock_unlock(bmdi);
	}

	return 0;
}

void bm1688_bmdrv_clk_set_tpu_divider_fpll(struct bm_device_info *bmdi, int divider_factor)
{
	int val = 0;

	if (bm1688_bmdev_clk_hwlock_lock(bmdi) != 0)
	{
		pr_err("%s: get clk hwlock fail\n", __func__);
		return;
	}

	/* gate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val &= ~(1 << BIT_CLK_TPU_ENABLE);
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);
	udelay(1);

	/* assert */
	val = top_reg_read(bmdi, TOP_CLK_DIV1);
	val &= ~(1 << BIT_CLK_TPU_ASSERT);
	top_reg_write(bmdi, TOP_CLK_DIV1, val);
	udelay(1);

	/* change divider */
	val &= ~(0x1f << 16);
	val |= (divider_factor & 0x1f) << 16;
	val |= 1 << 3;
	top_reg_write(bmdi, TOP_CLK_DIV1, val);
	udelay(1);

	/* deassert */
	val |= 1;
	top_reg_write(bmdi, TOP_CLK_DIV1, val);
	udelay(1);

	/* ungate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val |= 1 << BIT_CLK_TPU_ENABLE;
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);

	bm1688_bmdev_clk_hwlock_unlock(bmdi);
}

void bm1688_bmdrv_clk_set_close(struct bm_device_info *bmdi)
{
	u32 val = 0;

	if (bm1688_bmdev_clk_hwlock_lock(bmdi) != 0)
	{
		pr_err("%s: get clk hwlock fail\n", __func__);
		return;
	}

	val = top_reg_read(bmdi, TOP_CLK_ENABLE0);
	val &= ~0x0000fffc;
	top_reg_write(bmdi, TOP_CLK_ENABLE0, val);
	udelay(1);

	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val &= ~(1 << 18);
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);
	udelay(1);

	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 0x00003ff6;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
	udelay(1);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~0x003c0004;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(1);

	bm1688_bmdev_clk_hwlock_unlock(bmdi);
}

int bm1688_bmdrv_clk_get_tpu_freq(struct bm_device_info *bmdi)
{
	int prediv = 0;
	int postdiv = 0;
	int divsel = 0;
	int val = 0;
	int fout = 0;

	/* Modify TPLL Control Register */
	val = top_reg_read(bmdi, TOP_TPLL_CTL);

	prediv = (val >> 4) & 0x7f;
	postdiv = (val >> 24) & 0x7f;
	divsel = (val >> 16) & 0x7f;

	postdiv = postdiv ? postdiv : 1;
	prediv = prediv ? prediv : 1;

	fout = 1000 * BM1688_TPLL_FREF * divsel / prediv / postdiv;
	return fout/1000;
}

void bm1688_bmdrv_clk_set_tpu_divider(struct bm_device_info *bmdi, int divider_factor)
{
	int val = 0;

	if (bm1688_bmdev_clk_hwlock_lock(bmdi) != 0)
	{
		pr_err("%s: get clk hwlock fail\n", __func__);
		return;
	}

	/* gate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val &= ~(1 << BIT_CLK_TPU_ENABLE);
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);
	udelay(1);

	/* assert */
	val = top_reg_read(bmdi, TOP_CLK_DIV0);
	val &= ~(1 << BIT_CLK_TPU_ASSERT);
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	udelay(1);

	/* change divider */
	val &= ~(0x1f << 16);
	val |= (divider_factor & 0x1f) << 16;
	val |= 1 << 3;
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	udelay(1);

	/* deassert */
	val |= 1;
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	udelay(1);

	/* ungate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val |= 1 << BIT_CLK_TPU_ENABLE;
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);

	bm1688_bmdev_clk_hwlock_unlock(bmdi);
}

int bm1688_bmdrv_clk_get_tpu_divider(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_CLK_DIV0);
	return (val >> 16) & 0x1f;
}

int bm1688_bmdev_clk_ioctl_set_tpu_divider(struct bm_device_info* bmdi, unsigned long arg)
{
	int divider = 0;

	if(get_user(divider, (u64 __user *)arg)) {
		pr_err("bmdrv: bm1688_bmdev_clk_ioctl_set_tpu_divider get user failed!\n");
		return -1;
	} else {
		printk("bm1688_bmdrv_clk_set_tpu_divider: divider=%d\n", divider);
		bm1688_bmdrv_clk_set_tpu_divider(bmdi, divider);
		return 0;
	}
}

int bm1688_bmdev_clk_ioctl_set_tpu_freq(struct bm_device_info* bmdi, unsigned long arg)
{
	int target = 0;

	if(get_user(target, (u64 __user *)arg)) {
		pr_err("bmdrv: bm1688_bmdev_clk_ioctl_set_tpu_freq get user failed!\n");
		return -1;
	} else {
		return bm1688_bmdrv_clk_set_tpu_target_freq(bmdi, target);
	}
}

int bm1688_bmdev_clk_ioctl_get_tpu_freq(struct bm_device_info* bmdi, unsigned long arg)
{
	int freq = 0;
	int ret = 0;

	freq = bm1688_bmdrv_clk_get_tpu_freq(bmdi);
	ret = put_user(freq, (int __user *)arg);
	return ret;
}

void bm1688_bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bm1688_bmdrv_clk_disable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bm1688_bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bm1688_bmdrv_clk_disable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bm1688_bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bm1688_bmdrv_clk_disable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bm1688_bmdrv_sw_reset_tpu(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_TPU0);
	val &= ~(1 << BIT_SW_RESET_TPU1);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_TPU0;
	val |= 1 << BIT_SW_RESET_TPU1;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bm1688_bmdrv_sw_reset_gdma(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_GDMA0);
	val &= ~(1 << BIT_SW_RESET_GDMA1);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_GDMA0;
	val |= 1 << BIT_SW_RESET_GDMA1;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bm1688_bmdrv_sw_reset_tc906b(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_TC906B0);
	val &= ~(1 << BIT_SW_RESET_TC906B1);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_TC906B0;
	val |= 1 << BIT_SW_RESET_TC906B1;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bm1688_bmdrv_sw_reset_hau(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_HAU);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_HAU;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bm1688_bmdrv_clk_set_module_reset(struct bm_device_info* bmdi, BM1688_MODULE_ID module)
{
	switch (module) {
	case BM1688_MODULE_TPU:
		bm1688_bmdrv_sw_reset_tpu(bmdi);
		break;
	case BM1688_MODULE_GDMA:
		bm1688_bmdrv_sw_reset_gdma(bmdi);
		break;
	case BM1688_MODULE_TC906B:
		bm1688_bmdrv_sw_reset_tc906b(bmdi);
		break;
	case BM1688_MODULE_HAU:
		bm1688_bmdrv_sw_reset_hau(bmdi);
		break;
	default:
		break;
	}
}

int bm1688_bmdev_clk_ioctl_set_module_reset(struct bm_device_info* bmdi, unsigned long arg)
{
	BM1688_MODULE_ID module = (BM1688_MODULE_ID)0;

	if(get_user(module, (u64 __user *)arg)) {
		pr_err("bmdrv: bmdev_clk_ioctl_set_module get user failed!\n");
		return -1;
	} else {
		printk("bm1688_bmdrv_clk_set_module_reset: module=%d\n", module);
		bm1688_bmdrv_clk_set_module_reset(bmdi, module);
		return 0;
	}
}

#ifdef SOC_MODE
int bm1688_modules_reset_init(struct bm_device_info* bmdi)
{

	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo =  &bmdi->cinfo;
	int ret = 0;

	cinfo->tpusys = devm_reset_control_get(dev, "tpusys");
	if (IS_ERR(cinfo->tpusys)) {
		ret = PTR_ERR(cinfo->tpusys);
		dev_err(dev, "failed to retrieve tpusys reset");
		return ret;
	}
	cinfo->tpu0 = devm_reset_control_get(dev, "tpu0");
	if (IS_ERR(cinfo->tpu0)) {
		ret = PTR_ERR(cinfo->tpu0);
		dev_err(dev, "failed to retrieve tpu0 reset");
		return ret;
	}
	cinfo->tpu1 = devm_reset_control_get(dev, "tpu1");
	if (IS_ERR(cinfo->tpu1)) {
		ret = PTR_ERR(cinfo->tpu1);
		dev_err(dev, "failed to retrieve tpu1 reset");
		return ret;
	}
	cinfo->gdma0 = devm_reset_control_get(dev, "gdma0");
	if (IS_ERR(cinfo->gdma0)) {
		ret = PTR_ERR(cinfo->gdma0);
		dev_err(dev, "failed to retrieve gdma0 reset");
		return ret;
	}
	cinfo->gdma1 = devm_reset_control_get(dev, "gdma1");
	if (IS_ERR(cinfo->gdma1)) {
		ret = PTR_ERR(cinfo->gdma1);
		dev_err(dev, "failed to retrieve gdma1 reset");
		return ret;
	}
	cinfo->tc906b0 = devm_reset_control_get(dev, "tc906b0");
	if (IS_ERR(cinfo->tc906b0)) {
		ret = PTR_ERR(cinfo->tc906b0);
		dev_err(dev, "failed to retrieve tc906b0 reset");
		return ret;
	}
	cinfo->tc906b1 = devm_reset_control_get(dev, "tc906b1");
	if (IS_ERR(cinfo->tc906b1)) {
		ret = PTR_ERR(cinfo->tc906b1);
		dev_err(dev, "failed to retrieve tc906b1 reset");
		return ret;
	}
	return ret;
}

void bm1688_modules_reset(struct bm_device_info* bmdi)
{
	bm1688_tpu_reset(bmdi);
	bm1688_gdma_reset(bmdi);
	bm1688_hau_reset(bmdi);
}

void bm1688_modules_tpu_system_reset(struct bm_device_info *bmdi)
{
	int ret;

	bm1688_modules_reset(bmdi);

	ret = bmdrv_fw_load(bmdi, NULL, NULL);
	if (ret) {
		pr_err("load firmware fail\n");
	}

	bmdrv_clear_lib_list(bmdi);
	bmdrv_clear_func_list(bmdi);
}

int bm1688_modules_clk_init(struct bm_device_info* bmdi)
{
	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo =  &bmdi->cinfo;
	int ret = 0;

	cinfo->tpu_clk = devm_clk_get(dev, "tpu");
	if (IS_ERR(cinfo->tpu_clk)) {
		ret = PTR_ERR(cinfo->tpu_clk);
		dev_err(dev, "failed to retrieve tpu clk");
		return ret;
	}
	cinfo->top_fab0_clk = devm_clk_get(dev, "top_fab0");
	if (IS_ERR(cinfo->top_fab0_clk)) {
		ret = PTR_ERR(cinfo->top_fab0_clk);
		dev_err(dev, "failed to retrieve top_fab0 clk");
		return ret;
	}
	cinfo->tc906b_clk = devm_clk_get(dev, "tc906b");
	if (IS_ERR(cinfo->tc906b_clk)) {
		ret = PTR_ERR(cinfo->tc906b_clk);
		dev_err(dev, "failed to retrieve tc906b clk");
		return ret;
	}
	cinfo->timer_clk = devm_clk_get(dev, "timer");
	if (IS_ERR(cinfo->timer_clk)) {
		ret = PTR_ERR(cinfo->timer_clk);
		dev_err(dev, "failed to retrieve timer clk");
		return ret;
	}
	cinfo->gdma_clk = devm_clk_get(dev, "gdma");
	if (IS_ERR(cinfo->gdma_clk)) {
		ret = PTR_ERR(cinfo->gdma_clk);
		dev_err(dev, "failed to retrieve gdma clk");
		return ret;
	}
	return 0;
}

void bm1688_modules_clk_deinit(struct bm_device_info* bmdi)
{
	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo =  &bmdi->cinfo;

	devm_clk_put(dev, cinfo->tpu_clk);
	devm_clk_put(dev, cinfo->top_fab0_clk);
	devm_clk_put(dev, cinfo->tc906b_clk);
	devm_clk_put(dev, cinfo->timer_clk);
	devm_clk_put(dev, cinfo->gdma_clk);
}

void bm1688_modules_clk_enable(struct bm_device_info* bmdi)
{
	bm1688_tpu_clk_enable(bmdi);
	bm1688_top_fab0_clk_enable(bmdi);
	bm1688_tc906b_clk_enable(bmdi);
	bm1688_timer_clk_enable(bmdi);
	bm1688_gdma_clk_enable(bmdi);
}

void bm1688_modules_clk_disable(struct bm_device_info* bmdi)
{
	bm1688_tpu_clk_disable(bmdi);
	bm1688_top_fab0_clk_disable(bmdi);
	bm1688_tc906b_clk_disable(bmdi);
	bm1688_timer_clk_disable(bmdi);
	bm1688_gdma_clk_disable(bmdi);
}
#endif
