#include "bm_common.h"
#include "Device.h"

#include "bm_card.tmh"

static struct bm_card *g_bmcd[BM_MAX_CARD_NUM] = {NULL};

int bm_card_get_chip_num(struct bm_device_info *bmdi)
{
#ifdef SOC_MODE
	return 1;
#else
	if (bmdi->cinfo.chip_id == 0x1682)
		return 1;
	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686)
		return bm1684_card_get_chip_num(bmdi);
	else
		return 1;
#endif
}

static bool bm_chip_in_card(struct bm_card *bmcd, struct bm_device_info *bmdi)
{
	int dev_index = bmdi->dev_index;

	if (dev_index < 0)
		return FALSE;
	if (dev_index > BM_MAX_CHIP_NUM)
		return FALSE;

	if ((dev_index < bmcd->dev_start_index + bmcd->chip_num)
		&& (dev_index >= bmcd->dev_start_index))
		return TRUE;
	else
		return FALSE;
}

struct bm_card *bmdrv_card_get_bm_card(struct bm_device_info *bmdi)
{
	int i = 0;
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CARD, "bmdi->bmcd =%p\n",bmdi->bmcd);
	if (bmdi->bmcd != NULL)
		return bmdi->bmcd;

	for (i = 0; i < BM_MAX_CARD_NUM; i++) {
		if (g_bmcd[i] != NULL) {
			if (bm_chip_in_card(g_bmcd[i], bmdi) == TRUE)
				return g_bmcd[i];
		} else {
			return NULL;
		}
	}
	return NULL;
}

static int bm_add_chip_to_card(struct bm_device_info *bmdi)
{
	int i = 0;
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CARD, "dev%d  chip index=%d\n", bmdi->dev_index, bmdi->cinfo.chip_index);

// #ifndef SOC_MODE
// 	bm1684_card_get_chip_index(bmdi);
// #else
// 	bmdi->cinfo.chip_index = 0x0;
// #endif
	struct bm_card *bmcd = bmdrv_card_get_bm_card(bmdi);

	if (bmcd == NULL) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CARD, "bmcd == NULL\n");
		bmcd = MmAllocateNonCachedMemory(sizeof(struct bm_card));
		if (!bmcd) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_CARD, "bm card alloc fail\n");
			return -1;
		}
		RtlZeroMemory(bmcd, sizeof(struct bm_card));
		for (i = 0; i < BM_MAX_CARD_NUM; i++) {
			if (g_bmcd[i] == NULL) {
				g_bmcd[i] = bmcd;
				g_bmcd[i]->card_index = i;
				g_bmcd[i]->chip_num = bm_card_get_chip_num(bmdi);
				g_bmcd[i]->dev_start_index = bmdrv_get_card_start_index(bmdi);
				g_bmcd[i]->card_bmdi[bmdi->cinfo.chip_index] = bmdi;
				bmdi->bmcd = g_bmcd[i];
				g_bmcd[i]->running_chip_num = 0x1;
				return 0;
			}
		}
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_CARD, "card not find\n");
		return -1;
	} else {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CARD, "entry bmcd exist,chip_index=%d, bmcd =%p\n",bmdi->cinfo.chip_index,bmcd);
		bmdi->bmcd = bmcd;
		bmcd->card_bmdi[bmdi->cinfo.chip_index] = bmdi;
		bmcd->running_chip_num++;
	}
	return 0;
}

static int bm_remove_chip_from_card(struct bm_device_info *bmdi)
{
	int index = 0x0;

	if (bmdi->bmcd == NULL)
		return 0;

	index = bmdi->bmcd->card_index;
	if (g_bmcd[index] == NULL) {
		bmdi->bmcd = NULL;
		return 0;
	}

	g_bmcd[index]->card_bmdi[bmdi->cinfo.chip_index] = NULL;
	g_bmcd[index]->running_chip_num--;
	if (g_bmcd[index]->running_chip_num == 0x0) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CARD, "free card %d\n", index);
		MmFreeNonCachedMemory(g_bmcd[index], sizeof(struct bm_card));
		g_bmcd[index] = NULL;
		bmdi->bmcd    = NULL;
	}
	return 0;
}

int bmdrv_card_init(struct bm_device_info *bmdi)
{
	int ret = 0;

	ret = bm_add_chip_to_card(bmdi);
	if (ret < 0) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_CARD, "add chip to card fail\n");
		return ret;
	}

	return ret;
}

int bmdrv_card_deinit(struct bm_device_info *bmdi)
{
	int ret = 0;

	ret = bm_remove_chip_from_card(bmdi);
	if (ret < 0) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_CARD, "remove chip from card fail\n");
		return ret;
	}

	return ret;
}
