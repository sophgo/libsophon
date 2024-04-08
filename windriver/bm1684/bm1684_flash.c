#include "../bm_common.h"
#include "bm1684_flash.tmh"

#define BOOT_LOADER_SPI_ADDR (0)
#define BOOT_LOADER_SIZE (1024*63*17)
#define SPI_BLOCK (1024*63)

#ifndef SOC_MODE
static int bm1684_cat_message(char *cat, u32 len, u32 num, char *buffer, char *res_buffer)
{
    UNREFERENCED_PARAMETER(res_buffer);
    UNREFERENCED_PARAMETER(buffer);
    UNREFERENCED_PARAMETER(num);
    UNREFERENCED_PARAMETER(len);
    UNREFERENCED_PARAMETER(cat);
	//char check_buf[len];
	//int i, j, k = 0;
	int ret = 0;

	//memset(check_buf, 0, sizeof(check_buf));
	//for (i = 0; i < BOOT_LOADER_SIZE - len; i++) {
	//	if (buffer[i] == cat[0]) {
	//		k = 0;
	//		for (j = 0; j < len; j++) {
	//			if (buffer[i + j] == cat[j] && buffer[i + j] != ' ')
	//				k++;
	//		}
	//		if (k >= num) {
	//			k = 0;
	//			strncpy(res_buffer, &buffer[i], len);
 //               TraceEvents(TRACE_LEVEL_INFORMATION,
 //                           TRACE_DEVICE,
 //                           "BL: %s\n",
 //                           res_buffer);
	//			memset(check_buf, 0, sizeof(check_buf));
	//			break;
	//		}
	//	}
	//}
	return ret;
}

int bm1684_get_bootload_version(struct bm_device_info *bmdi)
{
	u32 boot_load_spi_addr = BOOT_LOADER_SPI_ADDR;
	u32 size = BOOT_LOADER_SIZE;
	int ret = 0;
	unsigned int i = 0;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	//struct bm_stagemem *stagemem_s2d = &bmdi->memcpy_info.stagemem_s2d;
	char *BLv_cat = "v1.4                                  ";
	char *BL_cat = "Built :                        ";
	int len = (int)strlen(BLv_cat);

	if (bmdi->cinfo.platform == PALLADIUM)
		return 0;


	memset(bmdi->cinfo.boot_loader_version[0], 0, sizeof(bmdi->cinfo.boot_loader_version[0]));
	memset(bmdi->cinfo.boot_loader_version[1], 0, sizeof(bmdi->cinfo.boot_loader_version[0]));
	bm_spi_init(bmdi);
    ExAcquireFastMutex(&memcpy_info->s2dstag_Mutex);
	for (i = 0; i < size / SPI_BLOCK; i++) {
		ret = bm_spi_data_read(bmdi,
            (u8 *)memcpy_info->s2dstagging_mem_vaddr + i * SPI_BLOCK,
		boot_load_spi_addr + i * SPI_BLOCK, SPI_BLOCK);
	}
    bm1684_cat_message(BLv_cat,
                       len,
                       4,
                       (char *)memcpy_info->s2dstagging_mem_vaddr,
                       bmdi->cinfo.boot_loader_version[0]);
    bm1684_cat_message(BL_cat,
                       len,
                       6,
                       (char *)memcpy_info->s2dstagging_mem_vaddr,
                       bmdi->cinfo.boot_loader_version[1]);
    ExReleaseFastMutex(&memcpy_info->s2dstag_Mutex);
	return ret;
}
#endif
