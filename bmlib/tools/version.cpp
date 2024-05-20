#include <bmlib_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>

#define BL1_VERSION_BASE		0x25050100
#define BL1_VERSION_SIZE		0x40
#define BL2_VERSION_BASE		(BL1_VERSION_BASE + BL1_VERSION_SIZE) // 0x25050140
#define BL2_VERSION_SIZE		0x40
#define BL31_VERSION_BASE		(BL2_VERSION_BASE + BL2_VERSION_SIZE) // 0x25050180
#define BL31_VERSION_SIZE		0x40
#define UBOOT_VERSION_BASE		(BL31_VERSION_BASE + BL31_VERSION_SIZE) // 0x250501c0
#define UBOOT_VERSION_SIZE		0x50
#define CHIP_VERSION_BASE		0x27102014
#define CHIP_VERSION_SIZE		0x4

int main(int argc, char const *argv[])
{
    bm_handle_t handle = NULL;
    bm_status_t ret = BM_SUCCESS;
    int chip_num = 0;
    boot_loader_version version;
    int bl1_strlen, bl1_print = 1;
	int chip_info, i;
	char *str, *chip;
	char bl2_str[100] = "";
	char bl3_str[100] = "";

    ret = bm_dev_request(&handle, chip_num);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    version.bl1_version = (char *)malloc(BL1_VERSION_SIZE);
    version.bl2_version = (char *)malloc(BL2_VERSION_SIZE);
    version.bl31_version = (char *)malloc(BL31_VERSION_SIZE);
    version.uboot_version = (char *)malloc(UBOOT_VERSION_SIZE);
	version.chip_version = (char *)malloc(CHIP_VERSION_SIZE);

    bm_get_boot_loader_version(handle, &version);
	if (strcmp(version.chip_version, "" ))
		chip = "cv186ah";
	else
		chip = "bm1688";

	str = strstr(version.bl2_version, ":");
	strcat(bl2_str, chip);
	strcat(bl2_str, str);

	str = strstr(version.bl31_version, ":");
	strcat(bl3_str, chip);
	strcat(bl3_str, str);

    bl1_strlen = strlen(version.bl1_version);
    for (i = 0; i < bl1_strlen; i++) {
        if (isprint(version.bl1_version[i]) == 0){
            bl1_print = 0;
            break;
        }
    }

    if (bl1_print == 1)
        printf("BL1 %s\n",  version.bl1_version);
    printf("BL2 %s\n",  bl2_str);
    printf("BL31 %s\n",  bl3_str);
    printf("%s\n",  version.uboot_version);

    free(version.bl1_version);
    free(version.bl2_version);
    free(version.bl31_version);
    free(version.uboot_version);
	free(version.chip_version);
    return 0;
}