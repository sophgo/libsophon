
#include "bm_common.h"
#include "bm_api.h"

#define TPU0_CFG_PWR_CTRL_ADDR (0x26000100 + 0xc0)

cfg_pwr_ctrl_t g_cfg_pwr_param_default[2] = {
	{
		.cfg12 = {
			.cfg_pwr_ctrl_en = 1,
			.cfg_pwr_bub_en = 1,
			.cfg_pwr_limit_en = 1,
			.cfg_pwr_timeout_en = 1,
			.cfg_pwr_step_scale = 1,
			.cfg_pwr_step_max = 0xF,
			.cfg_pwr_step_min = 0xC,
			.cfg_pwr_step_len = 1,
			.cfg_pwr_timeout_len = 0xF,
			.cfg_pwr_lane_all_en = 1,
			.cfg_pwr_scale_en = 1,
			.cfg_pwr_cur_step = 0,
			.cfg12_rsvd0_part1 = 0,
			.cfg12_rsvd0_part2 = 0,
		},
		.cfg13 = {.cfg_pwr_max_grp0 = {0x25, 0x57, 0x3F, 0x5F, 0x6F, 0x77, 0x7B, 0x7D}, .cfg_pwr_max_grp1 = {0x7E, 0x2F, 0x73, 0x0F, 0x37, 0x7F, 0x00, 0x00}},
	},
	{
		.cfg12 = {
			.cfg_pwr_ctrl_en = 1,
			.cfg_pwr_bub_en = 1,
			.cfg_pwr_limit_en = 1,
			.cfg_pwr_timeout_en = 1,
			.cfg_pwr_step_scale = 1,
			.cfg_pwr_step_max = 0xF,
			.cfg_pwr_step_min = 0xC,
			.cfg_pwr_step_len = 1,
			.cfg_pwr_timeout_len = 0xF,
			.cfg_pwr_lane_all_en = 1,
			.cfg_pwr_scale_en = 1,
			.cfg_pwr_cur_step = 0,
			.cfg12_rsvd0_part1 = 0,
			.cfg12_rsvd0_part2 = 0,
		},
		.cfg13 = {.cfg_pwr_max_grp0 = {0x25, 0x57, 0x3F, 0x5F, 0x6F, 0x77, 0x7B, 0x7D}, .cfg_pwr_max_grp1 = {0x7E, 0x2F, 0x73, 0x0F, 0x37, 0x7F, 0x00, 0x00}},
	}};

int pwr_ctrl_set(struct bm_device_info *bmdi, cfg_pwr_ctrl_t *cfg_pwr_ctrl_p)
{
	void __iomem *tpu0_cfg_pwr_ctrl_add_v;

	if (cfg_pwr_ctrl_p == NULL)
	{
		cfg_pwr_ctrl_p = &g_cfg_pwr_param_default[0];
	}

	tpu0_cfg_pwr_ctrl_add_v = bm_get_devmem_vaddr(bmdi, TPU0_CFG_PWR_CTRL_ADDR);

	memcpy(tpu0_cfg_pwr_ctrl_add_v, cfg_pwr_ctrl_p, sizeof(cfg_pwr_ctrl_t));
	return 0;
}

int pwr_ctrl_get(struct bm_device_info *bmdi, cfg_pwr_ctrl_t *cfg_pwr_ctrl_p)
{
	cfg_pwr_ctrl_t cfg_pwr_ctrl[2];
	void __iomem *tpu0_cfg_pwr_ctrl_add_v;
	int core_id = 0;

	tpu0_cfg_pwr_ctrl_add_v = bm_get_devmem_vaddr(bmdi, TPU0_CFG_PWR_CTRL_ADDR);
	memcpy(&cfg_pwr_ctrl[0], tpu0_cfg_pwr_ctrl_add_v, sizeof(cfg_pwr_ctrl_t));
	if (cfg_pwr_ctrl_p != NULL)
	{
		memcpy(cfg_pwr_ctrl_p, &cfg_pwr_ctrl, sizeof(cfg_pwr_ctrl_t) * 2);
	}
	else
	{
		for (core_id = 0; core_id < 2; core_id++){
			pr_err("core_id=%d\n"
						".cfg12:\n"
						".cfg_pwr_ctrl_en     = 0x%x\n"
						".cfg_pwr_bub_en      = 0x%x\n"
						".cfg_pwr_limit_en    = 0x%x\n"
						".cfg_pwr_timeout_en  = 0x%x\n"
						".cfg_pwr_step_scale  = 0x%x\n"
						".cfg_pwr_step_max    = 0x%x\n"
						".cfg_pwr_step_min    = 0x%x\n"
						".cfg_pwr_step_len    = 0x%x\n"
						".cfg_pwr_timeout_len = 0x%x\n"
						".cfg_pwr_lane_all_en = 0x%x\n"
						".cfg_pwr_scale_en    = 0x%x\n"
						".cfg_pwr_cur_step    = 0x%x\n"
						".cfg13:\n"
						".cfg_pwr_max_grp0 = {0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}\n"
						".cfg_pwr_max_grp1 = {0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}\n",
						core_id,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_ctrl_en,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_bub_en,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_limit_en,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_timeout_en,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_scale,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_max,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_min,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_len,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_timeout_len,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_lane_all_en,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_scale_en,
						cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_cur_step,
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[0], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[1],
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[2], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[3],
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[4], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[5],
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[6], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[7],
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[0], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[1],
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[2], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[3],
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[4], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[5],
						cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[6], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[7]);
		}
	}

	return 0;
}

int pwr_ctrl_ioctl(struct bm_device_info *bmdi, void *arg)
{
	bm_api_cfg_pwr_ctrl_t bm_api_cfg_pwr_ctrl;
	int ret;

	if (arg != NULL) {
		ret = copy_from_user(&bm_api_cfg_pwr_ctrl, (bm_api_cfg_pwr_ctrl_t __user *)arg, sizeof(bm_api_cfg_pwr_ctrl_t));
		if (ret) {
			pr_err("[%s: %d] bm-sophon%d copy_from_user fail\n", __func__, __LINE__, bmdi->dev_index);
			return ret;
		}
		if (bm_api_cfg_pwr_ctrl.op == 0) {
			pwr_ctrl_get(bmdi, &bm_api_cfg_pwr_ctrl.cfg_pwr_ctrl[0]);
			ret = copy_to_user((bm_api_cfg_pwr_ctrl_t __user *)arg, &bm_api_cfg_pwr_ctrl, sizeof(bm_api_cfg_pwr_ctrl_t));
			if (ret)
			{
				pr_err("[%s: %d] bm-sophon%d copy_to_user fail\n", __func__, __LINE__, bmdi->dev_index);
				return ret;
			}
		} else {
			pwr_ctrl_set(bmdi, &bm_api_cfg_pwr_ctrl.cfg_pwr_ctrl[0]);
		}
	}

	return 0;
}


