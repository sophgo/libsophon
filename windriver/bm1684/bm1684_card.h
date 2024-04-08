#ifndef _BM1684_CARD_H_
#define _BM1684_CARD_H_

void bm1684_stop_arm9(struct bm_device_info *bmdi);
void bm1684_start_arm9(struct bm_device_info *bmdi);
int bm1684_l2_sram_init(struct bm_device_info *bmdi);
void bm1684x_stop_a53lite(struct bm_device_info *bmdi);
void bm1684x_start_a53lite(struct bm_device_info *bmdi);


int bm1684_get_mcu_reg(struct bm_device_info *bmdi, u32 index, u8 *data);
int bm1684_get_board_version_from_mcu(struct bm_device_info *bmdi);
int bm1684_get_board_type_by_id(struct bm_device_info *bmdi, char *s_board_type, int id);
int bm1684_get_board_version_by_id(struct bm_device_info *bmdi, char *s_board_type,
		int board_id, int pcb_version, int bom_version);

int bm1684_card_get_chip_num(struct bm_device_info *bmdi);
#define BM1684_BOARD_TYPE(bmdi) ((u8)(((bmdi->cinfo.board_version) >> 8) & 0xff))
#define BM1684_HW_VERSION(bmdi) ((u8)((bmdi->cinfo.board_version) & 0xff))
#define BM1684_MCU_VERSION(bmdi) ((u8)(((bmdi->cinfo.board_version) >> 16) & 0xff))


#endif
