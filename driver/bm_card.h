#ifndef _BM_CARD_H_
#define _BM_CARD_H_
#ifndef SOC_MODE
#define BM_MAX_CARD_NUM	                32
#define BM_MAX_CHIP_NUM_PER_CARD	8
#define BM_MAX_CHIP_NUM                 128
#else
#define BM_MAX_CARD_NUM	                1
#define BM_MAX_CHIP_NUM_PER_CARD	1
#define BM_MAX_CHIP_NUM                 1
#endif

struct bm_card {
	int card_index;
	int chip_num;
	int running_chip_num;
	int dev_start_index;
	int board_power;
	int fan_speed;
	int atx12v_curr;
	int board_max_power;
	int cdma_max_payload;
	char sn[18];
	void *vfs_db;
	struct bm_device_info *sc5p_mcu_bmdi;
	struct bm_device_info *card_bmdi[BM_MAX_CHIP_NUM_PER_CARD];
	struct bm_device_info *first_probe_bmdi;
};

int bmdrv_card_init(struct bm_device_info *bmdi);
int bmdrv_card_deinit(struct bm_device_info *bmdi);
int bm_get_card_num_from_system(void);
int bm_get_chip_num_from_system(void);
int bm_get_card_info(struct bm_card *bmcd);
struct bm_card *bmdrv_card_get_bm_card(struct bm_device_info *bmdi);
#endif
