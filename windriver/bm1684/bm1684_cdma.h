#ifndef _BM1684_CDMA_H_
#define _BM1684_CDMA_H_

u32 bm1684_cdma_transfer(struct bm_device_info *bmdi, pbm_cdma_arg parg, bool lock_cdma, WDFFILEOBJECT fileObject);
void bm1684_clear_cdmairq(struct bm_device_info *bmdi);

#endif
