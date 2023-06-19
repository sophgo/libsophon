#include "bm_card.h"
#include "bm1684_card.h"
#include "bm_common.h"

#ifndef SOC_MODE
#include "bm_pcie.h"
#include "bm_attr.h"
#endif

static struct bm_card *g_bmcd[BM_MAX_CARD_NUM] = {NULL};

int bm_card_get_chip_num(struct bm_device_info *bmdi)
{
#ifdef SOC_MODE
	return 1;
#else
	if (bmdi->cinfo.chip_id == 0x1682)
		return 1;
	if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686))
		return bm1684_card_get_chip_num(bmdi);
	else
		return 1;
#endif
}

static bool bm_chip_in_card(struct bm_card *bmcd, struct bm_device_info *bmdi)
{
	int dev_index = bmdi->dev_index;

	if (dev_index < 0)
		return false;
	if (dev_index > BM_MAX_CHIP_NUM)
		return false;

	if ((dev_index < bmcd->dev_start_index + bmcd->chip_num)
		&& (dev_index >= bmcd->dev_start_index))
		return true;
	else
		return false;
}

struct bm_card *bmdrv_card_get_bm_card(struct bm_device_info *bmdi)
{
	int i = 0;

	if (bmdi->bmcd != NULL)
		return bmdi->bmcd;

	for (i = 0; i < BM_MAX_CARD_NUM; i++) {
		if (g_bmcd[i] != NULL) {
			if (bm_chip_in_card(g_bmcd[i], bmdi) == true)
				return g_bmcd[i];
		} else {
			return NULL;
		}
	}
	return NULL;
}

#ifndef SOC_MODE
static int bm_update_sc5p_mcu_bmdi_to_card(struct bm_device_info *bmdi)
{
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if (bmdrv_sc5pro_uart_is_connect_mcu(bmdi) != 0x1)
			return -1;
		else {
			bmdi->bmcd->sc5p_mcu_bmdi = bmdi;
			return 0;
		}
	}

	bmdi->bmcd->sc5p_mcu_bmdi = NULL;
	return -1;
}
#endif

#ifndef SOC_MODE
int bm_card_update_sn(struct bm_device_info *bmdi, char *sn)
{
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if (bmdi->cinfo.chip_index != 0)
			return -1;
	}
	bm_get_sn(bmdi, sn);
	return 0;
}
#endif

static int bm_add_chip_to_card(struct bm_device_info *bmdi)
{
	int i = 0;
	struct bm_card *bmcd = NULL;

#ifndef SOC_MODE
	bm1684_card_get_chip_index(bmdi);
#else
	bmdi->cinfo.chip_index = 0x0;
#endif
	bmcd = bmdrv_card_get_bm_card(bmdi);

	if (bmcd == NULL) {
		bmcd = kzalloc(sizeof(struct bm_card), GFP_KERNEL);
		if (!bmcd) {
			pr_err("bm card alloc fail\n");
			return -1;
		}
#ifndef SOC_MODE
		bm_card_update_sn(bmdi, bmcd->sn);
#endif
		for (i = 0; i < BM_MAX_CARD_NUM; i++) {
			if (g_bmcd[i] == NULL) {
				g_bmcd[i] = bmcd;
				g_bmcd[i]->card_index = i;
				g_bmcd[i]->chip_num = bm_card_get_chip_num(bmdi);
				g_bmcd[i]->dev_start_index = bmdi->dev_index;
				g_bmcd[i]->card_bmdi[bmdi->cinfo.chip_index] = bmdi;
				g_bmcd[i]->first_probe_bmdi = bmdi;
				bmdi->bmcd = g_bmcd[i];
				g_bmcd[i]->running_chip_num = 0x1;
				g_bmcd[i]->cdma_max_payload = bmdi->memcpy_info.cdma_max_payload;
#ifndef SOC_MODE
				g_bmcd[i]->vfs_db = bmdrv_alloc_vfs_database(bmdi);
				bm_update_sc5p_mcu_bmdi_to_card(bmdi);
#endif
				return 0;
			}
		}
		pr_err("card not find\n");
		return -1;
	} else {
		bmdi->bmcd = bmcd;
		bmcd->card_bmdi[bmdi->cinfo.chip_index] = bmdi;
		bmcd->running_chip_num++;
		bmcd->cdma_max_payload = bmdi->memcpy_info.cdma_max_payload;
#ifndef SOC_MODE
		bm_card_update_sn(bmdi, bmcd->sn);
		bm_update_sc5p_mcu_bmdi_to_card(bmdi);
#endif
	}
	return 0;
}

static int bm_remove_chip_from_card(struct bm_device_info *bmdi)
{
	int index = 0x0;

	if (bmdi->bmcd == NULL)
		return 0;

	index = bmdi->bmcd->card_index;
	if (g_bmcd[index] == NULL)
		return 0;

	g_bmcd[index]->card_bmdi[bmdi->cinfo.chip_index] = NULL;
	g_bmcd[index]->running_chip_num--;
	if (g_bmcd[index]->running_chip_num == 0x0) {
		pr_info("free card %d\n", index);
#ifndef SOC_MODE
		if (g_bmcd[index]->vfs_db != NULL) {
			kfree(g_bmcd[index]->vfs_db);
			g_bmcd[index]->vfs_db = NULL;
		}
#endif
		kfree(g_bmcd[index]);
		g_bmcd[index] = NULL;
	}
	return 0;
}

int bmdrv_card_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	int i = 0;

	if (bmdi->cinfo.platform == PALLADIUM)
		return 0;

	ret = bm_add_chip_to_card(bmdi);
	if (ret < 0) {
		pr_err("add chip to card fail\n");
		return ret;
	}

	for (i = 0; i < 17; i++)
		dev_info_write_byte(bmdi, bmdi->cinfo.dev_info.sn_reg + i, bmdi->bmcd->sn[i]);

	return ret;
}

int bmdrv_card_deinit(struct bm_device_info *bmdi)
{
	int ret = 0;

	if (bmdi->cinfo.platform == PALLADIUM)
		return 0;

	ret = bm_remove_chip_from_card(bmdi);
	if (ret < 0) {
		pr_err("remove chip from card fail\n");
		return ret;
	}

	return ret;
}

int bm_get_card_num_from_system(void) {
#ifdef SOC_MODE
	return 1;
#else
	int card_num = 0;
	int i = 0;
	for (i = 0; i < BM_MAX_CARD_NUM; i++) {
		if (g_bmcd[i] != NULL)
			card_num++;
	}
	return card_num;
#endif
}

int bm_get_chip_num_from_system(void) {
#ifdef SOC_MODE
	return 1;
#else
	int chip_num = 0;
	int i = 0;
	for (i = 0; i < BM_MAX_CARD_NUM; i++) {
		if (g_bmcd[i] != NULL)
			chip_num += g_bmcd[i]->running_chip_num;
	}
	return chip_num;
#endif
}

int bm_get_card_info(struct bm_card *bmcd) {
	int i = 0;
	int ret = 0;
	for (i = 0; i < BM_MAX_CARD_NUM; i++) {
		if (bmcd->card_index == g_bmcd[i]->card_index) {
			bmcd->chip_num = g_bmcd[i]->chip_num;
			bmcd->dev_start_index = g_bmcd[i]->dev_start_index;
			return ret;
		}
	}
	return 1;
}
