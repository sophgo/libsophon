#ifndef __84X6_BASE64_H__
#define __84X6_BASE64_H__

char *bm84x6_base_get_chip_id(struct bm_device_info *bmdi);

#ifndef SOC_MODE
int bm84x6_base64_prepare(struct bm_device_info *base64_bmdi,
                          struct ce_base base);
int bm84x6_base64_start(struct bm_device_info *base64_bmdi);
#endif /* #ifndef SOC_MODE */

#endif /* __84X6_BASE64_H__ */
