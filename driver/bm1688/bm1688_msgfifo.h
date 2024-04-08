#ifndef _BM1688_MSGFIFO_H_
#define _BM1688_MSGFIFO_H_
struct bm_device_info;
u32 bm1688_pending_msgirq_cnt(struct bm_device_info *bmdi);
int bm1688_clear_msgirq(struct bm_device_info *bmdi, int core_id);
#endif
