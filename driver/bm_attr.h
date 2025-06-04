#ifndef _BM_ATTR_H_
#define _BM_ATTR_H_
#include "bm_common.h"

#define BM_THERMAL_WINDOW_WIDTH 5
#define BM_MAX_CHIP_NUM_PER_CARD	1
#define VFS_MAX_LEVEL_SC7_PRO    20
//#define VFS_MAX_LEVEL_SC7_PLUS    16
#define VFS_MAX_LEVEL_SC7_PLUS    20
//#define VFS_INIT_LEVEL_SC7_PLUS   1
#define VFS_INIT_LEVEL_SC7_PLUS   0
#define VFS_INIT_LEVEL_SC7_PRO   0
#define VFS_RELBL_LEVEL_SC7_PLUS   5
#define VFS_RELBL_LEVEL_SC7_PRO   5
#define VFS_PWR_MEAN_SAMPLE_SIZE  10

#define TPU_HANG_MASK              (0x1 << 0)
#define PCIE_LINK_ERROR_MASK       (0x1 << 0x1)
#define CHIP_OVER_TEMP_MASK        (0x1 << 0x2)

enum bm_freq_scaling_mode {
	FREQ_UP_MODE = 0,
	FREQ_DOWN_MODE,
	FREQ_INIT_MODE,
	FREQ_SCAL_BUTT
};

enum bm_vfs_status {
	VFS_ORIGIN_MODE = 0,
	VFS_INIT_MODE,
	VFS_MISSION_MODE,
	VFS_STATUS_BUTT
};

enum bm_freq_scaling_caller {
	FREQ_CALLER_TEMP = 0,
	FREQ_CALLER_VFS,
	FREQ_CALLER_BUTT
};

struct bm_thermal_info {
	int elapsed_temp[BM_THERMAL_WINDOW_WIDTH];
	int idx;
	int max_clk_tmp;
	int half_clk_tmp;
	int min_clk_tmp;
	// following variable only used for sc7 pro
	int tmp;
	int index;
	int is_off;
	int extreme_tmp;
};

struct bm_chip_attr {
	u16 fan_speed;
	u16 fan_rev_read;
	atomic_t npu_utilization;
	int npu_cnt;
	int npu_busy_cnt;
	int npu_timer_interval;
	u64 npu_busy_time_sum_ms;
	u64 npu_start_probe_time;
#define NPU_STAT_WINDOW_WIDTH 50
	int npu_status[NPU_STAT_WINDOW_WIDTH];
	int npu_status_idx;

	atomic_t npu_utilization1;
	u64 npu_busy_time_sum_ms1;
	u64 npu_start_probe_time1;
	int npu_status1[NPU_STAT_WINDOW_WIDTH];
	int npu_status_idx1;
	struct mutex attr_mutex;
	atomic_t timer_on;
	bool fan_control;
	int led_status;
	struct bm_thermal_info thermal_info;

	int (*bm_card_attr_init)(struct bm_device_info *);
	void (*bm_card_attr_deinit)(struct bm_device_info *);
	int (*bm_get_chip_temp)(struct bm_device_info *, int *);
	int (*bm_get_board_temp)(struct bm_device_info *, int *);
	int (*bm_get_tpu_power)(struct bm_device_info *, u32 *);
	int (*bm_get_vddc_power)(struct bm_device_info *, u32 *);
	int (*bm_get_vddphy_power)(struct bm_device_info *, u32 *);
	int (*bm_get_board_power)(struct bm_device_info *, u32 *);
	int (*bm_get_fan_speed)(struct bm_device_info *);
	int (*bm_get_npu_util)(struct bm_device_info *);
	int (*bm_get_npu_util1)(struct bm_device_info *);

	int (*bm_set_led_status)(struct bm_device_info *, int);
	int last_valid_tpu_power;
	int last_valid_vddc_power;
	int last_valid_vddphy_power;
	int last_valid_tpu_volt;
	int last_valid_tpu_curr;

	int board_temp;
	int chip_temp;
	int board_power;
	int tpu_power;
	int vddc_power;
	int vddphy_power;
	int vdd_tpu_volt;
	int vdd_tpu_curr;
	int atx12v_curr;
	int tpu_current_clock;
};

struct bm_vfs_pair {
	u32 freq;
	u32 volt;
};

int bmdrv_card_attr_init(struct bm_device_info *bmdi);
void bmdrv_card_attr_deinit(struct bm_device_info *bmdi);
int bmdrv_enable_attr(struct bm_device_info *bmdi);
int bmdrv_disable_attr(struct bm_device_info *bmdi);
int bm_get_name(struct bm_device_info *bmdi, unsigned long arg);

int bm_get_fan_speed(struct bm_device_info *bmdi);
int set_fan_speed(struct bm_device_info *bmdi, u16 spd);
int reset_fan_speed(struct bm_device_info *bmdi);
int bmdev_ioctl_get_attr(struct bm_device_info *bmdi, void *arg);

#endif
