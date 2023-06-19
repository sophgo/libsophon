#ifndef _BM_ATTR_H_
#define _BM_ATTR_H_
#include "bm_common.h"
#include "bm_card.h"

#define FAN_PWM_PERIOD 4000
#define LED_PWM_PERIOD 100000000UL  // p_clk 100MHz
#define BM_THERMAL_WINDOW_WIDTH 5
#define VFS_MAX_LEVEL_SC7_PRO    20
//#define VFS_MAX_LEVEL_SC7_PLUS    16
#define VFS_MAX_LEVEL_SC7_PLUS    20
//#define VFS_INIT_LEVEL_SC7_PLUS   1
#define VFS_INIT_LEVEL_SC7_PLUS   0
#define VFS_INIT_LEVEL_SC7_PRO   0
#define VFS_RELBL_LEVEL_SC7_PLUS   5
#define VFS_RELBL_LEVEL_SC7_PRO   5
#define VFS_PWR_MEAN_SAMPLE_SIZE  10

#define LED_OFF 0
#define LED_ON  1
#define LED_BLINK_FAST 2
#define LED_BLINK_ONE_TIMES_PER_S   3
#define LED_BLINK_ONE_TIMES_PER_2S   4
#define LED_BLINK_THREE_TIMES_PER_S   5

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

struct bm_freq_scaling_db {
	u32 start_flag;
	u32 freq_scal_count;
	u32 board_pwr_count;
	u32 freq_up_count;
	u32 idle_count;
	int chip0_index;
	u32 power_index;
	u32 board_power[VFS_PWR_MEAN_SAMPLE_SIZE];
	u32 power_average;
	u32 power_total;
	u32 power_highest;
	u32 power_peak_threshold;
	u32 power_upper_threshold;
	u32 power_lower_threshold;
	u32 vf_level;
	u32 vf_init_level;
	u32 vf_relbl_level;
	u32 vfs_max_level;
	u32 thermal_freq[BM_MAX_CHIP_NUM_PER_CARD];
	struct bm_vfs_pair freq_volt_pair[VFS_MAX_LEVEL_SC7_PRO];
};

int bmdrv_card_attr_init(struct bm_device_info *bmdi);
void bmdrv_card_attr_deinit(struct bm_device_info *bmdi);
int bmdrv_enable_attr(struct bm_device_info *bmdi);
int bmdrv_disable_attr(struct bm_device_info *bmdi);
int bm_get_correct_num(struct bm_device_info *bmdi, unsigned long arg);
int bm_get_12v_atx(struct bm_device_info *bmdi, unsigned long arg);
int bm_get_device_sn(struct bm_device_info *bmdi, unsigned long arg);
int bm_get_name(struct bm_device_info *bmdi, unsigned long arg);
#ifndef SOC_MODE
int bm_read_tmp451_local_temp(struct bm_device_info *bmdi, int *temp);
int bm_read_tmp451_remote_temp(struct bm_device_info *bmdi, int *temp);
int bm_read_vdd_tpu_power(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_mem_power(struct bm_device_info *, u32 *);
int bm_read_vddc_power(struct bm_device_info *, u32 *);
int bm_set_vddc_voltage(struct bm_device_info *, u32);
int bm_read_sc5_power(struct bm_device_info *, u32 *);
int bm_read_1684x_evb_power(struct bm_device_info *bmdi, u32 *power);
int bm_read_sc5_plus_power(struct bm_device_info *, u32 *);
int bm_read_sc5_pro_power(struct bm_device_info *, u32 *);
int bm_read_mcu_current(struct bm_device_info *, u8, u32 *);
int bm_read_mcu_voltage(struct bm_device_info *, u8, u32 *);
int bm_read_mcu_chip_temp(struct bm_device_info *, int *);
int bm_read_mcu_board_temp(struct bm_device_info *, int *);
int bm_read_vdd_tpu_current(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_voltage(struct bm_device_info *, u32 *);
int bm_set_vdd_tpu_voltage(struct bm_device_info *, u32 );
int bm_read_vdd_pmu_tpu_temp(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_mem_current(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_mem_voltage(struct bm_device_info *, u32 *);
int bm_read_vdd_pmu_tpu_mem_temp(struct bm_device_info *, u32 *);
int bm_read_vddc_current(struct bm_device_info *, u32 *);
int bm_read_vddc_voltage(struct bm_device_info *, u32 *);
int bm_read_vddphy_power(struct bm_device_info *bmdi, u32 *);
int bm_read_vddc_pmu_vddc_temp(struct bm_device_info *, u32 *);
int bm_burning_info_sn(struct bm_device_info *, unsigned long);
int bm_burning_info_mac(struct bm_device_info *, int, unsigned long);
int bm_burning_info_board_type(struct bm_device_info *, unsigned long);
int bm_get_sn(struct bm_device_info *, char *);
int bm_get_board_type(struct bm_device_info *, char *);
int bmdrv_sc5pro_uart_is_connect_mcu(struct bm_device_info *bmdi);
struct bm_freq_scaling_db * bmdrv_alloc_vfs_database(struct bm_device_info *bmdi);
void bmdrv_init_freq_scaling_status(struct bm_device_info *);
int bmdrv_record_board_power(struct bm_device_info *bmdi, u32 power);
int bmdrv_check_tpu_utils(struct bm_device_info *bmdi);
int bmdrv_set_sc7_pro_tpu_volt_freq(struct bm_device_info *, u32, u32, enum bm_freq_scaling_mode);
int bmdrv_volt_freq_scaling_controller(struct bm_device_info *);
int bmdrv_volt_freq_scaling(struct bm_device_info *);
int bmdrv_get_tpu_target_freq(struct bm_device_info *bmdi, enum bm_freq_scaling_caller caller, int *target);
#endif
int bm_read_tmp451_local_temp_by_mcu(struct bm_device_info *bmdi, int *temp);
int bm_read_tmp451_remote_temp_by_mcu(struct bm_device_info *bmdi, int *temp);
int bm_read_npu_util(struct bm_device_info *bmdi);
void bmdrv_adjust_fan_speed(struct bm_device_info *bmdi, u32 temp);
int bm_get_fan_speed(struct bm_device_info *bmdi);
int set_fan_speed(struct bm_device_info *bmdi, u16 spd);
int reset_fan_speed(struct bm_device_info *bmdi);
int set_led_status(struct bm_device_info *bmdi, int);
int set_ecc(struct bm_device_info *bmdi, int ecc_able);
void bm_npu_utilization_stat(struct bm_device_info *bmdi);
void bmdrv_fetch_attr(struct bm_device_info *bmdi, int count, int is_setspeed);
void bmdrv_fetch_attr_board_power(struct bm_device_info *bmdi, int count);
int bmdev_ioctl_get_attr(struct bm_device_info *bmdi, void *arg);

#endif
