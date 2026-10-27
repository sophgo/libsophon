#ifndef _BM84X6_CARD_H_
#define _BM84X6_CARD_H_
#include "bm_common.h"

void bm84x6_stop_c906(struct bm_device_info *bmdi);
void bm84x6_start_c906(struct bm_device_info *bmdi);
int bm84x6_reset_tpu(struct bm_device_info *bmdi);
int bm84x6_l2_sram_init(struct bm_device_info *bmdi);
#ifdef SOC_MODE
void bm84x6_tpu_reset(struct bm_device_info *bmdi);
void bm84x6_gdma_reset(struct bm_device_info *bmdi);
void bm84x6_tc906_reset(struct bm_device_info *bmdi);
void bm84x6_hau_reset(struct bm_device_info *bmdi);
void bm84x6_tpusys_reset(struct bm_device_info *bmdi);
void bm84x6_top_fab0_clk_enable(struct bm_device_info *bmdi);
void bm84x6_top_fab0_clk_disable(struct bm_device_info *bmdi);
void bm84x6_tc906b_clk_enable(struct bm_device_info *bmdi);
void bm84x6_tc906b_clk_disable(struct bm_device_info *bmdi);
void bm84x6_timer_clk_enable(struct bm_device_info *bmdi);
void bm84x6_timer_clk_disable(struct bm_device_info *bmdi);

void bm84x6_resume_tpu(struct bm_device_info *bmdi, u32 c906_park_0_l,
                       u32 c906_park_0_h, u32 c906_park_1_l, u32 c906_park_1_h);
#endif
void bm84x6_tpu_clk_enable(struct bm_device_info *bmdi);
void bm84x6_tpu_clk_disable(struct bm_device_info *bmdi);
void bm84x6_gdma_clk_enable(struct bm_device_info *bmdi);
void bm84x6_gdma_clk_disable(struct bm_device_info *bmdi);
void bm84x6_cdma_clk_enable(struct bm_device_info *bmdi);
void bm84x6_cdma_clk_disable(struct bm_device_info *bmdi);

#ifndef SOC_MODE
int bm84x6_get_mcu_reg(struct bm_device_info *bmdi, u32 index, u8 * data);

int bm84x6_get_board_type_by_id(struct bm_device_info *bmdi, char *s_board_type,
                                int id);
void bm84x6_get_clk_temperature(struct bm_device_info *bmdi);
void bm84x6_get_fusing_temperature(struct bm_device_info *bmdi, int *max_tmp,
                                   int *support_tmp);

int bm84x6_card_get_chip_num(struct bm_device_info *bmdi);
int bm84x6_card_get_chip_index(struct bm_device_info *bmdi);
#define BM84X6_BOARD_TYPE(bmdi) ((u8)(((bmdi->cinfo.board_version) >> 8) & 0xff))
#define BM84X6_HW_VERSION(bmdi) ((u8)((bmdi->cinfo.board_version) & 0xff))
#define BM84X6_MCU_VERSION(bmdi) ((u8)(((bmdi->cinfo.board_version) >> 16) & 0xff))

#endif
#endif
