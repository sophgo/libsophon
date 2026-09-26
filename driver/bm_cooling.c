#include <linux/kernel.h>
#include "bm_cooling.h"

#ifndef SOC_MODE

static int bmdrv_cooling_is_1688_pcie(const struct bm_device_info *bmdi)
{
	return bmdi->cinfo.chip_id == 0x1686a200;
}

static int bmdrv_cooling_state_to_freq(const struct bm_pcie_cooling_ctx *ctx, int state)
{
	switch (state) {
	case BM_PCIE_COOLING_S0:
		return ctx->freq_s0_mhz;
	case BM_PCIE_COOLING_S1:
		return ctx->freq_s1_mhz;
	case BM_PCIE_COOLING_S2:
		return ctx->freq_s2_mhz;
	default:
		return BM_COOLING_FREQ_NO_CHANGE;
	}
}

static int bmdrv_cooling_next_state(const struct bm_pcie_cooling_ctx *ctx, int cur_state, int chip_temp_mc)
{
	switch (cur_state) {
	case BM_PCIE_COOLING_S0:
		if (chip_temp_mc >= ctx->trip_s3_mc)
			return BM_PCIE_COOLING_S3_BLOCK;
		if (chip_temp_mc >= ctx->trip_s2_mc)
			return BM_PCIE_COOLING_S2;
		if (chip_temp_mc >= ctx->trip_s1_mc)
			return BM_PCIE_COOLING_S1;
		break;
	case BM_PCIE_COOLING_S1:
		if (chip_temp_mc >= ctx->trip_s3_mc)
			return BM_PCIE_COOLING_S3_BLOCK;
		if (chip_temp_mc >= ctx->trip_s2_mc)
			return BM_PCIE_COOLING_S2;
		if (chip_temp_mc <= ctx->clear_s1_mc)
			return BM_PCIE_COOLING_S0;
		break;
	case BM_PCIE_COOLING_S2:
		if (chip_temp_mc >= ctx->trip_s3_mc)
			return BM_PCIE_COOLING_S3_BLOCK;
		if (chip_temp_mc <= ctx->clear_s2_mc)
			return BM_PCIE_COOLING_S1;
		break;
	case BM_PCIE_COOLING_S3_BLOCK:
		if (chip_temp_mc <= ctx->clear_s3_mc)
			return BM_PCIE_COOLING_S2;
		break;
	default:
		break;
	}

	return cur_state;
}

void bmdrv_cooling_init(struct bm_device_info *bmdi)
{
	struct bm_pcie_cooling_ctx *ctx = &bmdi->cooling_ctx;

	if (!bmdrv_cooling_is_1688_pcie(bmdi)) {
		ctx->inited = 0;
		return;
	}

	ctx->inited = 1;
	ctx->state = BM_PCIE_COOLING_S0;
	ctx->last_applied_freq_mhz = BM_COOLING_FREQ_NO_CHANGE;
	ctx->trip_s1_mc = 100000;
	ctx->trip_s2_mc = 110000;
	ctx->trip_s3_mc = 115000;
	ctx->clear_s1_mc = 95000;
	ctx->clear_s2_mc = 105000;
	ctx->clear_s3_mc = 105000;
	ctx->freq_s0_mhz = 900;
	ctx->freq_s1_mhz = 450;
	ctx->freq_s2_mhz = 100;
}

void bmdrv_cooling_update_1688(struct bm_device_info *bmdi, int chip_temp_mc, int *target_freq_mhz)
{
	struct bm_pcie_cooling_ctx *ctx = &bmdi->cooling_ctx;
	int cur_state;
	int next_state;
	int target_freq;

	if (target_freq_mhz)
		*target_freq_mhz = BM_COOLING_FREQ_NO_CHANGE;

	if (!ctx->inited)
		return;

	cur_state = ctx->state;
	next_state = bmdrv_cooling_next_state(ctx, cur_state, chip_temp_mc);
	ctx->state = next_state;
	bmdi->status_over_temp = (next_state == BM_PCIE_COOLING_S3_BLOCK);

	if (cur_state == next_state)
		return;

	if (cur_state != BM_PCIE_COOLING_S3_BLOCK && next_state == BM_PCIE_COOLING_S3_BLOCK) {
		pr_warn_ratelimited(
			"bm-sophon%d over-temp asserted, temp=%d mC, threshold=%d mC\n",
			bmdi->dev_index, chip_temp_mc, ctx->trip_s3_mc);
	} else if (cur_state == BM_PCIE_COOLING_S3_BLOCK &&
		   next_state != BM_PCIE_COOLING_S3_BLOCK) {
		pr_info("bm-sophon%d over-temp cleared, temp=%d mC, clear=%d mC\n",
			bmdi->dev_index, chip_temp_mc, ctx->clear_s3_mc);
	}

	if (next_state == BM_PCIE_COOLING_S3_BLOCK)
		return;

	target_freq = bmdrv_cooling_state_to_freq(ctx, next_state);
	if (target_freq == BM_COOLING_FREQ_NO_CHANGE)
		return;

	if (target_freq == ctx->last_applied_freq_mhz)
		return;

	if (target_freq_mhz)
		*target_freq_mhz = target_freq;
}

void bmdrv_cooling_mark_freq_applied(struct bm_device_info *bmdi, int freq_mhz)
{
	struct bm_pcie_cooling_ctx *ctx = &bmdi->cooling_ctx;

	if (!ctx->inited)
		return;

	ctx->last_applied_freq_mhz = freq_mhz;
}

#endif
