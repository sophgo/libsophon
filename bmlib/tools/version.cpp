#include <bmlib_runtime.h>
#include <stdio.h>
#include <stdlib.h>

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

    if(version.bl1_version[0] == 'v')
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