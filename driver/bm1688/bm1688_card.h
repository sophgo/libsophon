#ifndef _BM1688_CARD_H_
#define _BM1688_CARD_H_
#include "bm_common.h"

void bm1688_stop_c906(struct bm_device_info *bmdi);
void bm1688_start_c906(struct bm_device_info *bmdi);
int bm1688_reset_tpu(struct bm_device_info *bmdi);
int bm1688_l2_sram_init(struct bm_device_info *bmdi);
#ifdef SOC_MODE
void bm1688_tpu_reset(struct bm_device_info *bmdi);
void bm1688_gdma_reset(struct bm_device_info *bmdi);
void bm1688_hau_reset(struct bm_device_info *bmdi);
void bm1688_top_fab0_clk_enable(struct bm_device_info *bmdi);
void bm1688_top_fab0_clk_disable(struct bm_device_info *bmdi);
void bm1688_tc906b_clk_enable(struct bm_device_info *bmdi);
void bm1688_tc906b_clk_disable(struct bm_device_info *bmdi);
void bm1688_timer_clk_enable(struct bm_device_info *bmdi);
void bm1688_timer_clk_disable(struct bm_device_info *bmdi);
void bm1688_resume_tpu(struct bm_device_info *bmdi, u32 c906_park_0_l,
						u32 c906_park_0_h, u32 c906_park_1_l, u32 c906_park_1_h);
#endif
void bm1688_tpu_clk_enable(struct bm_device_info *bmdi);
void bm1688_tpu_clk_disable(struct bm_device_info *bmdi);
void bm1688_gdma_clk_enable(struct bm_device_info *bmdi);
void bm1688_gdma_clk_disable(struct bm_device_info *bmdi);

#ifndef SOC_MODE
int bm1688_get_mcu_reg(struct bm_device_info *bmdi, u32 index, u8 *data);
int bm1688_get_board_version_from_mcu(struct bm_device_info *bmdi);
int bm1688_get_board_type_by_id(struct bm_device_info *bmdi, char *s_board_type, int id);
int bm1688_get_board_version_by_id(struct bm_device_info *bmdi, char *s_board_type,
		int board_id, int pcb_version, int bom_version);
void bm1688_get_clk_temperature (struct bm_device_info *bmdi);
void bm1688_get_fusing_temperature(struct bm_device_info *bmdi, int *max_tmp, int *support_tmp);

int bm1688_card_get_chip_num(struct bm_device_info *bmdi);
int bm1688_card_get_chip_index(struct bm_device_info *bmdi);
#define BM1688_BOARD_TYPE(bmdi) ((u8)(((bmdi->cinfo.board_version) >> 8) & 0xff))
#define BM1688_HW_VERSION(bmdi) ((u8)((bmdi->cinfo.board_version) & 0xff))
#define BM1688_MCU_VERSION(bmdi) ((u8)(((bmdi->cinfo.board_version) >> 16) & 0xff))

#endif
#endif
