#ifndef _84X6_MSGFIFO_H_
#define _84X6_MSGFIFO_H_
struct bm_device_info;
u32 bm84x6_pending_msgirq_cnt(struct bm_device_info *bmdi);
int bm84x6_clear_msgirq(struct bm_device_info *bmdi, int core_id);
#endif
