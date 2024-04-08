#ifndef _BM1682_CDMA_H_
#define _BM1682_CDMA_H_

u32 bm1682_cdma_transfer(struct bm_device_info *bmdi, pbm_cdma_arg parg, bool lock_cdma, WDFFILEOBJECT fileObject);
void bm1682_clear_cdmairq(struct bm_device_info *bmdi);

#endif
