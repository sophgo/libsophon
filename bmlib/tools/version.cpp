#include <bmlib_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BL1_VERSION_BASE		0x101fb200
#define BL1_VERSION_SIZE		0x40
#define BL2_VERSION_BASE		(BL1_VERSION_BASE + BL1_VERSION_SIZE) // 0x101fb240
#define BL2_VERSION_SIZE		0x40
#define BL31_VERSION_BASE		(BL2_VERSION_BASE + BL2_VERSION_SIZE) // 0x101fb280
#define BL31_VERSION_SIZE		0x40
#define UBOOT_VERSION_BASE		(BL31_VERSION_BASE + BL31_VERSION_SIZE) // 0x101fb2c0
#define UBOOT_VERSION_SIZE		0x50

int main(int argc, char const *argv[])
{
    bm_handle_t handle = NULL;
    bm_status_t ret = BM_SUCCESS;
    int chip_num = 0;
    boot_loader_version version;
    int bl1_strlen, bl1_print = 1;
    int i;

    ret = bm_dev_request(&handle, chip_num);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    version.bl1_version = (char *)malloc(BL1_VERSION_SIZE);
    version.bl2_version = (char *)malloc(BL2_VERSION_SIZE);
    version.bl31_version = (char *)malloc(BL31_VERSION_SIZE);
    version.uboot_version = (char *)malloc(UBOOT_VERSION_SIZE);

    bm_get_boot_loader_version(handle, &version);

    bl1_strlen = strlen(version.bl1_version);
    for (i = 0; i < bl1_strlen; i++) {
        if (isprint(version.bl1_version[i]) == 0){
            bl1_print = 0;
            break;
        }
    }

    if (bl1_print == 1)
        printf("BL1 %s\n",  version.bl1_version);
    printf("BL2 %s\n",  version.bl2_version);
    printf("BL31 %s\n",  version.bl31_version);
    printf("%s\n",  version.uboot_version);

    free(version.bl1_version);
    free(version.bl2_version);
    free(version.bl31_version);
    free(version.uboot_version);
    return 0;
}