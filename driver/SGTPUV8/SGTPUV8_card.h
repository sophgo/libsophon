#ifndef _SGTPUV8_CARD_H_
#define _SGTPUV8_CARD_H_
#include "bm_common.h"

int SGTPUV8_reset_tpu(struct bm_device_info *bmdi);
int SGTPUV8_l2_sram_init(struct bm_device_info *bmdi);

void SGTPUV8_tpu_reset(struct bm_device_info *bmdi);
void SGTPUV8_gdma_reset(struct bm_device_info *bmdi);
void SGTPUV8_hau_reset(struct bm_device_info *bmdi);
void SGTPUV8_tpu_clk_enable(struct bm_device_info *bmdi);
void SGTPUV8_tpu_clk_disable(struct bm_device_info *bmdi);
void SGTPUV8_top_fab0_clk_enable(struct bm_device_info *bmdi);
void SGTPUV8_top_fab0_clk_disable(struct bm_device_info *bmdi);
void SGTPUV8_timer_clk_enable(struct bm_device_info *bmdi);
void SGTPUV8_timer_clk_disable(struct bm_device_info *bmdi);
void SGTPUV8_gdma_clk_enable(struct bm_device_info *bmdi);
void SGTPUV8_gdma_clk_disable(struct bm_device_info *bmdi);



#endif
