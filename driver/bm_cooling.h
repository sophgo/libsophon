#ifndef _BM_COOLING_H_
#define _BM_COOLING_H_

#include "bm_common.h"

#define BM_COOLING_FREQ_NO_CHANGE (-1)

#ifndef SOC_MODE
void bmdrv_cooling_init(struct bm_device_info *bmdi);
void bmdrv_cooling_update_1688(struct bm_device_info *bmdi, int chip_temp_mc, int *target_freq_mhz);
void bmdrv_cooling_mark_freq_applied(struct bm_device_info *bmdi, int freq_mhz);
#endif

#endif
