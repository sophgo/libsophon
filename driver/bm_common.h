#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/platform_device.h>

#include "bm_gmem.h"
#include "bm_uapi.h"
#include "version.h"

#define BM_THERMAL_WINDOW_WIDTH 5

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

#define REG_COUNT 5
typedef enum {
	MODE_CHOSE_LAYOUT = 0,
	SETUP_BAR_DEV_LAYOUT = 1
} BAR_LAYOUT_TYPE;


#define OTP_SHADOW_SYS_SIZE 0x100

struct bm_bar_info {
	u64 bar_start[REG_COUNT];                     // address in host I/O memory layout
	u64 bar_dev_start[REG_COUNT];                 // address in device memory layout
	u64 bar_len[REG_COUNT];
	void __iomem *bar_vaddr[REG_COUNT];
};

struct smbus_devinfo {
	u8 chip_temp_reg;
	u8 board_temp_reg;
	u8 board_power_reg;
	u8 fan_speed_reg;
	u8 vendor_id_reg;
	u8 hw_version_reg;
	u8 fw_version_reg;
	u8 board_name_reg;
	u8 sub_vendor_id_reg;
	u8 sn_reg;
	u8 mcu_version_reg;
};


typedef enum {
	DEVICE,
	PALLADIUM,
	FPGA
} PLATFORM;


struct irq_name_to_id {
	u32 irq_id_gdma_intr;
	u32 irq_id_gdma_err_intr;
	u32 irq_id_tpu_intr;
	u32 irq_id_tpu_intr_pio_empty;
	u32 irq_id_tpu_intr_pio_half_empty;
	u32 irq_id_tpu_intr_pio_quart_empty;
	u32 irq_id_tpu_intr_pio_one_empty;
};

struct chip_info {
	struct smbus_devinfo dev_info;
	struct device *device;
	const char *chip_type;
	char dev_name[20];
	struct bm_bar_info bar_info;
	int share_mem_size;
	PLATFORM platform;
	u32 delay_ms;
	u32 polling_ms;
	unsigned int chip_id;
	int chip_index;
	int tpu_core_num;
	struct irq_name_to_id irq_id;
	struct platform_device *pdev;
	struct reset_control *tpu;
	struct reset_control *gdma;
	struct reset_control *tpusys;
	struct clk *tpu_clk;
	struct clk *gdma_clk;
	struct clk *fixed_tpu_clk;
	struct clk *intc_clk;
	struct clk *tpu_sys_clk;
	struct clk *timer_clk;

	int (*bmdrv_setup_bar_dev_layout)(struct bm_device_info *bmdi, BAR_LAYOUT_TYPE type);
	u32 (*bmdrv_pending_msgirq_cnt)(struct bm_device_info *bmdi);
};


struct bm_device_info {
	int dev_index;
	u64 bm_send_api_seq;
	struct cdev cdev;
	struct device *dev;
	struct device *parent;
	dev_t devno;
	void *priv;
	struct kobject kobj;
	int core_id;
	int MMAP_TPYE;
	spinlock_t close_lock;
	int use_count;
	struct completion gdma_done;
	int tpu_intr_irq;
	int tpu_event_flag;
	spinlock_t tpu_event_lock;
	void __iomem *tpu_reg_base;
	wait_queue_head_t tpu_event_waitq;
	struct mutex device_mutex;
	struct chip_info cinfo;
	struct bm_chip_attr c_attr;
	struct bm_card *bmcd;
	int enable_dyn_freq;
	int dump_reg_type;
	int fixed_fan_speed;
	int status; /* active or fault */
	int status_over_temp;
	int status_sync_api;
	u64 dev_refcount;
	struct bm_gmem_info gmem_info;
	struct list_head handle_list;
	struct mutex clk_reset_mutex;
	struct bm_misc_info misc_info;
	struct bm_boot_info boot_info;
	struct bm_profile profile;
	struct proc_dir_entry *proc_dir;
};


char *base_get_chip_id(struct bm_device_info *bmdi);
char *bmdrv_get_error_string(int error);


#define BM_CHIP_VERSION PROJECT_VER_MAJOR
#define BM_MAJOR_VERSION PROJECT_VER_MINOR
#define BM_MINOR_VERSION PROJECT_VER_PATCH
#define BM_DRIVER_VERSION \
	((BM_CHIP_VERSION << 16) | (BM_MAJOR_VERSION << 8) | BM_MINOR_VERSION)

#define MAX_CARD_NUMBER 1

#define TPU_GDMA_BASE 0xC000000UL			// gdma base addr
#define TPU_GDMA_SIZE 0x10000					// gdma size
#define TPU_GDMA_CSR_BASE 0xC001000UL // gdma pio base addr
#define TPU_SYS_BASE 0xC010000UL			// sys base addr
#define TPU_SYS_SIZE 0x30000					// sys size
#define TPU_REG_BASE 0xC040000UL			// reg base addr
#define TPU_REG_SIZE 0x10000					// reg size
#define TPU_SMEM_BASE 0xC041000UL			// smem base addr
#define TPU_SMEM_SIZE 0x1000					// smem size
#define TPU_LMEM_BASE 0xC080000UL			// lmem base addr
#define TPU_LMEM_SIZE 0x80000					// lmem size

#define BM_CLASS_NAME "bm-tpu"
#define BM_CDEV_NAME "bm-tpu"
#define BMDEV_CTL_NAME "bmdev-ctl"
#define CHIP_ID 0x184
#define DELAY_MS 20000
#define POLLING_MS 1
#define BM_MAX_CARD_NUM	                1


struct bm_card {
	int card_index;
	int chip_num;
	int running_chip_num;
	int dev_start_index;
	int board_power;
	int fan_speed;
	int atx12v_curr;
	int board_max_power;
	char sn[18];
	void *vfs_db;
	struct bm_device_info *sc5p_mcu_bmdi;
	struct bm_device_info *card_bmdi[1];
	struct bm_device_info *first_probe_bmdi;
};

typedef enum {
	MMAP_GDMA = 0,
	MMAP_SYS,
	MMAP_REG,
	MMAP_SMEM,
	MMAP_LMEM
} TPU_SYS_NUM;


// #define PR_DEBUG

#ifdef PR_DEBUG
#define PR_TRACE(fmt, ...) pr_info(fmt, ##__VA_ARGS__) // to minimize print
#else
#define PR_TRACE(fmt, ...)
#endif

#define REG_COUNT 5

#endif /* _BM_COMMON_H_ */
