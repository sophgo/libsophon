#ifndef __84X6_DEBUG_H__
#define __84X6_DEBUG_H__
#include "bm_common.h"
#include <linux/types.h>

#define TPU_USING_FLAG 0xa5a5

void bmdev_test_proc_exit(struct bm_device_info *bmdi);

int bmdev_test_proc_init(struct bm_device_info *bmdi);

void bmdev_set_scaler_log_low(struct bm_device_info *bmdi);
int bmdev_scaler_log_init(struct bm_device_info *bmdi);

void bmdev_scaler_log_deinit(struct bm_device_info *bmdi);

int bmdev_mbox_ready(void);
int bmdev_mbox_submit(struct bm_device_info *bmdi, int hwq_idx, u32 phy_offset,
		      u32 block_num, u32 group_num, u64 *info_out);
void bmdev_mbox_start(struct bm_device_info *bmdi);
void bmdev_mbox_stop(void);

u32 bmdev_get_scaler_using_flag(int core_id);

void bmdev_kernel_fifo_record(struct bm_device_info *bmdi, u32 api_id, u32 api_seq,
                              int hwq_idx, u32 hwq_packet, u64 outbox_packet,
                              bool is_outbox);

#endif
