// SPDX-License-Identifier: GPL-2.0
/*
 * CVITEK cv186x thermal driver
 *
 * Copyright 2023 CVITEK Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include "bm_thermal.h"
#include "bm_io.h"
#include "bm_common.h"

static void clrsetbits(struct bm_device_info *bmdi, u32 reg, u32 clrval, u32 setval)
{
    u32 regval;

    regval = thermal_reg_read(bmdi, reg);
    regval &= ~(clrval);
    regval |= setval;
    thermal_reg_write(bmdi, reg, regval);
}

static void tempsen_set(struct bm_device_info *bmdi, u32 reg, u32 mask, u32 offset, u32 val)
{
    clrsetbits(bmdi, reg, mask, val << offset);
}

static int tempsen_get(struct bm_device_info *bmdi, u32 reg, u32 mask, u32 offset)
{
	return (thermal_reg_read(bmdi, reg) & mask) >> offset;
}


void bm_thermal_init(struct bm_device_info *bmdi)
{
	u32 regval;
	u32 hw_thm;
	/* u32 force_val = 0x30; */

	/* clear all interrupt status */
	regval = thermal_reg_read(bmdi, tempsen_top_tempsen_intr_raw);
	tempsen_set(bmdi, tempsen_top_sta_tempsen_intr_clr,
						tempsen_top_sta_tempsen_intr_clr_MASK,
						tempsen_top_sta_tempsen_intr_clr_OFFSET,
						regval);

	/* clear max result */
	tempsen_set(bmdi, tempsen_top_clr_tempsen_ch0_max_result,
						tempsen_top_clr_tempsen_ch0_max_result_MASK,
						tempsen_top_clr_tempsen_ch0_max_result_OFFSET,
						1);
	tempsen_set(bmdi, tempsen_top_clr_tempsen_ch1_max_result,
						tempsen_top_clr_tempsen_ch1_max_result_MASK,
						tempsen_top_clr_tempsen_ch1_max_result_OFFSET,
						1);

	/* set chop period to 3:1024T */
	tempsen_set(bmdi, tempsen_top_reg_tempsen_chopsel,
						tempsen_top_reg_tempsen_chopsel_MASK,
						tempsen_top_reg_tempsen_chopsel_OFFSET,
						0x3);

	/* set acc period to 2:2048T*/
	tempsen_set(bmdi, tempsen_top_reg_tempsen_accsel,
						tempsen_top_reg_tempsen_accsel_MASK,
						tempsen_top_reg_tempsen_accsel_OFFSET,
						0x2);

	/* set tempsen clock divider to 25M/(0x31+1)= 0.5M ,T=2us */
	tempsen_set(bmdi, tempsen_top_reg_tempsen_cyc_clkdiv,
						tempsen_top_reg_tempsen_cyc_clkdiv_MASK,
						tempsen_top_reg_tempsen_cyc_clkdiv_OFFSET,
						0x31);

	/* set reg_tempsen_auto_cycle */
	tempsen_set(bmdi, tempsen_top_reg_tempsen_auto_cycle,
						tempsen_top_reg_tempsen_auto_cycle_MASK,
						tempsen_top_reg_tempsen_auto_cycle_OFFSET,
						500000); // 1s

	/* set rtc hw poweroff, 127 C */
	tempsen_set(bmdi, tempsen_top_reg_tempsen_overheat_th,
						tempsen_top_reg_tempsen_overheat_th_MASK,
						tempsen_top_reg_tempsen_overheat_th_OFFSET,
						0x423);
	tempsen_set(bmdi, tempsen_top_reg_tempsen_overheat_cycle,
						tempsen_top_reg_tempsen_overheat_cycle_MASK,
						tempsen_top_reg_tempsen_overheat_cycle_OFFSET,
						3000000); // 6s
	tempsen_set(bmdi, tempsen_top_reg_overheat_reset_clr,
						tempsen_top_reg_overheat_reset_clr_MASK,
						tempsen_top_reg_overheat_reset_clr_OFFSET,
						0x1);
	tempsen_set(bmdi, tempsen_top_reg_overheat_reset_en,
						tempsen_top_reg_overheat_reset_en_MASK,
						tempsen_top_reg_overheat_reset_en_OFFSET,
						0x1);
	rtc_reg_write(bmdi, RTC_EN_THM_SHDN, 1);
	hw_thm = hwthermal_reg_read(bmdi, HW_THM_SHDN_EN);
	hwthermal_reg_write(bmdi, HW_THM_SHDN_EN, hw_thm | 0xffff0004);

	/* enable tempsen channel */
	tempsen_set(bmdi, tempsen_top_reg_tempsen_sel,
						tempsen_top_reg_tempsen_sel_MASK,
						tempsen_top_reg_tempsen_sel_OFFSET,
						0x7);
	tempsen_set(bmdi, tempsen_top_reg_tempsen_en,
						tempsen_top_reg_tempsen_en_MASK,
						tempsen_top_reg_tempsen_en_OFFSET,
						1);
}

void bm_thermal_uninit(struct bm_device_info *bmdi)
{
	u32 regval;

	/* disable all tempsen channel */
	tempsen_set(bmdi, tempsen_top_reg_tempsen_sel,
						tempsen_top_reg_tempsen_sel_MASK,
						tempsen_top_reg_tempsen_sel_OFFSET,
						0);
	tempsen_set(bmdi, tempsen_top_reg_tempsen_en,
						tempsen_top_reg_tempsen_en_MASK,
						tempsen_top_reg_tempsen_en_OFFSET,
						0);

	/* clear all interrupt status */
	regval = thermal_reg_read(bmdi, tempsen_top_tempsen_intr_raw);
	tempsen_set(bmdi, tempsen_top_sta_tempsen_intr_clr,
						tempsen_top_sta_tempsen_intr_clr_MASK,
						tempsen_top_sta_tempsen_intr_clr_OFFSET,
						regval);
}

static int calc_temp(int result)
{
	/* return (((uint64_t)result * 1000 * 654643) / 1801439 - 271280); */

	/* Original calculation formula */
	// y = 0.3634x - 271.28

	return (result * 4074 - 3046900) / 10000;
	// y = 0.4074x - 304.69
}

int bm_read_temp(struct bm_device_info *bmdi, int *temperature)
{
	int			     result, r1, r2, r3;

	/* read temperature */
	r1 = tempsen_get(bmdi, tempsen_top_sta_tempsen_ch0_result,
							tempsen_top_sta_tempsen_ch0_result_MASK,
							tempsen_top_sta_tempsen_ch0_result_OFFSET);
	r2 = tempsen_get(bmdi, tempsen_top_sta_tempsen_ch1_result,
							tempsen_top_sta_tempsen_ch1_result_MASK,
							tempsen_top_sta_tempsen_ch1_result_OFFSET);
	r3 = tempsen_get(bmdi, tempsen_top_sta_tempsen_ch2_result,
							tempsen_top_sta_tempsen_ch2_result_MASK,
							tempsen_top_sta_tempsen_ch2_result_OFFSET);
	result = MAX_OF_THREE(r1, r2, r3);
	*temperature = calc_temp(result);
	PR_DEBUG("temp = %d mC(0x%x)\n", calc_temp(result), result);

	return 0;
}

