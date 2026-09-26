#include <bmlib_runtime.h>
#include "bmlib_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BL1_VERSION_SIZE		0x40
#define BL2_VERSION_SIZE		0x40
#define BL31_VERSION_SIZE		0x40
#define UBOOT_VERSION_SIZE		0x50
#define CHIP_VERSION_SIZE		0x4

static int is_printable_str(const char *s, int max_len)
{
	int i;

	if (!s || !s[0])
		return 0;
	for (i = 0; i < max_len && s[i]; i++) {
		if (!isprint((unsigned char)s[i]))
			return 0;
	}
	return 1;
}

static void free_version(boot_loader_version *version)
{
	free(version->bl1_version);
	free(version->bl2_version);
	free(version->bl31_version);
	free(version->uboot_version);
	free(version->chip_version);
}

static void append_ver_str(char *dst, const char *chip, const char *src, int max_len)
{
	const char *str;

	strcat(dst, chip);
	if (!is_printable_str(src, max_len))
		return;
	str = strstr(src, ":");
	if (str)
		strcat(dst, str);
	else {
		strcat(dst, " ");
		strcat(dst, src);
	}
}

int main(int argc, char const *argv[])
{
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;
	int chip_num = 0;
	boot_loader_version version;
	unsigned int chipid = 0;
	int val;
	const char *chip;
	char bl2_str[128] = "";
	char bl3_str[128] = "";

	(void)argc;
	(void)argv;

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	version.bl1_version = (char *)malloc(BL1_VERSION_SIZE);
	version.bl2_version = (char *)malloc(BL2_VERSION_SIZE);
	version.bl31_version = (char *)malloc(BL31_VERSION_SIZE);
	version.uboot_version = (char *)malloc(UBOOT_VERSION_SIZE);
	version.chip_version = (int *)malloc(CHIP_VERSION_SIZE);
	if (!version.bl1_version || !version.bl2_version ||
			!version.bl31_version || !version.uboot_version ||
			!version.chip_version) {
		printf("malloc version buffer failed\n");
		free_version(&version);
		bm_dev_free(handle);
		return -1;
	}
	memset(version.bl1_version, 0, BL1_VERSION_SIZE);
	memset(version.bl2_version, 0, BL2_VERSION_SIZE);
	memset(version.bl31_version, 0, BL31_VERSION_SIZE);
	memset(version.uboot_version, 0, UBOOT_VERSION_SIZE);
	memset(version.chip_version, 0, CHIP_VERSION_SIZE);

	ret = bm_get_boot_loader_version(handle, &version);
	if (ret != BM_SUCCESS) {
		printf("bm_get_boot_loader_version failed, ret = %d\n", ret);
		free_version(&version);
		bm_dev_free(handle);
		return -1;
	}

	ret = bm_get_chipid(handle, &chipid);
	if (ret != BM_SUCCESS) {
		printf("bm_get_chipid failed, ret = %d\n", ret);
		free_version(&version);
		bm_dev_free(handle);
		return -1;
	}

	if (chipid == BM_CHIP_ID_84X6) {
		chip = "cv84x6";
	} else if (chipid == BM_CHIP_ID_1688) {
		val = *version.chip_version & 0x7;
		if (val == 0 || val == 7)
			chip = "bm1688";
		else
			chip = "cv186ah";
	} else {
		printf("unsupported chipid=0x%x\n", chipid);
		free_version(&version);
		bm_dev_free(handle);
		return -1;
	}

	append_ver_str(bl2_str, chip, version.bl2_version, BL2_VERSION_SIZE);
	append_ver_str(bl3_str, chip, version.bl31_version, BL31_VERSION_SIZE);

	printf("BL2 %s\n", bl2_str);
	printf("BL31 %s\n", bl3_str);
	if (is_printable_str(version.uboot_version, UBOOT_VERSION_SIZE))
		printf("%s\n", version.uboot_version);
	else
		printf("U-Boot version unavailable\n");

	free_version(&version);
	bm_dev_free(handle);
	return 0;
}
