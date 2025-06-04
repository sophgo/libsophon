#ifndef _BM_API_H_
#define _BM_API_H_
#include "bm_uapi.h"
#include <linux/completion.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include "bm_common.h"

#define DEVICE_SYNC_MARKER 0xffff

#define API_FLAG_FINISH 4
#define API_FLAG_WAIT 2
#define BM_MAX_CORE_NUM 2
#define API_FLAG_DONE 1
#define LIB_MAX_REC_CNT (50)

struct bm_device_info;
struct bm_thread_info;

struct bm_api_info {
	/* sw_rp is used to manage share mameory */
	u32 sw_rp;

	u64 device_sync_last;
	u64 device_sync_cpl;
	struct completion dev_sync_done;

	struct mutex api_mutex;
	struct kfifo api_fifo;
	struct list_head api_list;
	struct mutex api_fifo_mutex;

	int (*bm_api_init)(struct bm_device_info *, u32 core, u32 channel);
	void (*bm_api_deinit)(struct bm_device_info *, u32 core, u32 channel);
};

struct api_fifo_entry {
	struct bm_thread_info *thd_info;
	struct bm_handle_info *h_info;
	u64 thd_api_seq[BM_MAX_CORE_NUM];
	u64 dev_api_seq;
	u64 sent_time_us;
	u64 global_api_seq;
	u64 api_done_flag;
	struct completion api_done;
	u64 api_data;
};

struct api_list_entry {
	struct list_head api_list_node;
	struct api_fifo_entry api_entry;
};

typedef struct bm_kapi_header {
	u32 api_size; /* size of payload, not including header */
	u64 api_handle;
	u32 api_seq;
	u32 duration;
	u32 result;
} __packed bm_kapi_header_t;

typedef struct bm_kapi_opt_header {
	u64 global_api_seq;
	u64 api_data;
} bm_kapi_opt_header_t;

typedef struct {
	u32 cfg_pwr_ctrl_en : 1;
	u32 cfg_pwr_bub_en : 1;
	u32 cfg_pwr_limit_en : 1;
	u32 cfg_pwr_timeout_en : 1;
	u32 cfg_pwr_step_scale : 4;
	u32 cfg_pwr_step_max : 4;
	u32 cfg_pwr_step_min : 4;
	u32 cfg_pwr_step_len : 16;
	u32 cfg_pwr_timeout_len : 32;
	u32 cfg_pwr_lane_all_en : 1;
	u32 cfg_pwr_scale_en : 1;
	u32 cfg_pwr_cur_step : 4;
	u32 cfg12_rsvd0_part1 : 26;
	u32 cfg12_rsvd0_part2 : 32;
} cfg12_t;

typedef struct {
	u8 cfg_pwr_max_grp0[8];
	u8 cfg_pwr_max_grp1[8];
} cfg13_t;

typedef struct {
	cfg12_t cfg12;
	cfg13_t cfg13;
} cfg_pwr_ctrl_t;

typedef struct {
	u32 op; // 0:get	1:set
	cfg_pwr_ctrl_t cfg_pwr_ctrl[2];
} bm_api_cfg_pwr_ctrl_t;

#define API_ENTRY_SIZE sizeof(struct api_fifo_entry)

int pwr_ctrl_get(struct bm_device_info *bmdi, cfg_pwr_ctrl_t *cfg_pwr_ctrl_p);
int pwr_ctrl_set(struct bm_device_info *bmdi, cfg_pwr_ctrl_t *cfg_pwr_ctrl_p);
int pwr_ctrl_ioctl(struct bm_device_info *bmdi, void *arg);
#endif
