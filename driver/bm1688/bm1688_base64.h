#ifndef __BM1688_BASE64_H__
#define __BM1688_BASE64_H__

char *base_get_chip_id(struct bm_device_info *bmdi);
int base_get_core_num(struct bm_device_info *bmdi);
#ifndef SOC_MODE
int bm1688_base64_prepare(struct bm_device_info *base64_bmdi, struct ce_base base);
int bm1688_base64_start(struct bm_device_info *base64_bmdi);
#endif /* #ifndef SOC_MODE */

#endif  /* __BM1688_BASE64_H__ */