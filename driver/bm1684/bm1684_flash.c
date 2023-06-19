#include "bm_common.h"
#include "spi.h"
#include "bm1684_flash.h"
#include "bm_pcie.h"
#include "bm1684_card.h"

#define BOOT_LOADER_SPI_ADDR (0)
#define BOOT_LOADER_SIZE (1024*63*17)
#define SPI_BLOCK (1024*63)

#ifndef SOC_MODE
static int bm1684_cat_message(char *cat, u32 len, u32 num, char *buffer, char *res_buffer)
{
	char *check_buf = vmalloc(len);
	int i, j, k = 0;
	int ret = 0;

	memset(check_buf, 0, len);
	for (i = 0; i < BOOT_LOADER_SIZE - len; i++) {
		if (buffer[i] == cat[0]) {
			k = 0;
			for (j = 0; j < len; j++) {
				if (buffer[i + j] == cat[j] && buffer[i + j] != ' ')
					k++;
			}
			if (k >= num) {
				k = 0;
				strncpy(res_buffer, &buffer[i], len);
				pr_info("BL: %s\n", res_buffer);
				memset(check_buf, 0, len);
				break;
			}
		}
	}
	vfree(check_buf);
	return ret;
}


/*v1.4-bm1684-v4.3.0-(release):g2a38cf7*/
static int bm1684_get_bootload_version_number(struct bm_device_info *bmdi)
{
	char *l_ver = bmdi->cinfo.boot_loader_version[0];
	int index = 13;
	int num[3];
	int i = 0x0;
	int j = 0x0;

	if ((l_ver[0] != 'v') && (l_ver[12] != 'v'))
		return -1;

	for (i = 0; i < 3; i++) {
		num[i] = 0;
		for (j = 0; j < 4; j++) {
			if (('0' <= l_ver[index]) && (l_ver[index] <= '9')) {
				num[i] = (num[i] * 10 + (l_ver[index] - '0'));
				num[i] = num[i] & 0xff;
				index++;
			} else if ('.' == l_ver[index]) {
				index++;
				break;
			} else if ('-' == l_ver[index]) {
				index++;
				break;
			} else {
				return -1;
			}
		}
	}

	bmdi->cinfo.boot_loader_num = ((num[0] << 16) | (num[1] << 8) | num[2]);
	pr_info("boot_loader_num = 0x%x\n", bmdi->cinfo.boot_loader_num);

	return 0;
}


int bm1684_get_bootload_version(struct bm_device_info *bmdi)
{
	u32 boot_load_spi_addr = BOOT_LOADER_SPI_ADDR;
	u32 size = BOOT_LOADER_SIZE;
	int ret = 0;
	int i = 0;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	struct bm_stagemem *stagemem_s2d = &bmdi->memcpy_info.stagemem_s2d;
	char *BLv_cat = "v1.4                                  ";
	char *BLv_cat_bm1686 = "v2.7                                  ";
	char *BL_cat = "Built :                        ";
	int len = strlen(BLv_cat);

	if (bmdi->cinfo.platform == PALLADIUM)
		return 0;

	memset(bmdi->cinfo.boot_loader_version[0], 0, sizeof(bmdi->cinfo.boot_loader_version[0]));
	memset(bmdi->cinfo.boot_loader_version[1], 0, sizeof(bmdi->cinfo.boot_loader_version[0]));
	bm_spi_init(bmdi);
	mutex_lock(&memcpy_info->stagemem_s2d.stage_mutex);

	for (i = 0; i < size / SPI_BLOCK; i++) {
		ret = bm_spi_data_read(bmdi,
		(u8 *)stagemem_s2d->v_addr + i * SPI_BLOCK,
		boot_load_spi_addr + i * SPI_BLOCK, SPI_BLOCK);
	}
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) ||
	    (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		bm1684_cat_message(BLv_cat_bm1686, len, 4, (char *)stagemem_s2d->v_addr, bmdi->cinfo.boot_loader_version[0]);
	else
		bm1684_cat_message(BLv_cat, len, 4, (char *)stagemem_s2d->v_addr, bmdi->cinfo.boot_loader_version[0]);

	bm1684_cat_message(BL_cat, len, 6, (char *)stagemem_s2d->v_addr, bmdi->cinfo.boot_loader_version[1]);
	mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);

	if ((BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_BM1684X_EVB) &&
	    (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO)) {
		ret = bm1684_get_bootload_version_number(bmdi);
		if (ret < 0) {
			bmdi->cinfo.boot_loader_num = 0;
		}
	}
	bm_spi_enable_dmmr(bmdi);

	return 0;
}
#endif
