#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/platform_device.h>

#include "bm_api.h"
#include "bm_attr.h"
#include "bm_gmem.h"
#include "bm_io.h"

#include "bm_uapi.h"
#include "version.h"


char *base_get_chip_id(struct bm_device_info *bmdi);
static long get_phys_addr(unsigned long arg);
static inline bool is_user_address(unsigned long addr);
char *bmdrv_get_error_string(int error);

#ifndef __maybe_unused
#define __maybe_unused __attribute__((unused))
#endif

#define TASK_LIST_MAX 100
#define DONE_LIST_MAX 1000

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

#define CHIP_ID 0x184
#define BMDEV_CTL_NAME "bmdev-ctl"

#define A53_RESET_STATUS_TRUE 1
#define A53_RESET_STATUS_FALSE 0

#define TOP_REG_CTRL_BASE_ADDR         (0x50010000)


#define NV_TIMER_BASE_ADDR        0x50010180

/*top register*/
#define TOP_CLK_LOCK           0x104
#define TOP_PLL_STATUS         0x0c0
#define TOP_PLL_ENABLE         0x0c4
#define TOP_TPLL_CTL           0x29a0
#define TOP_SW_RESET0          0x3000
/*
 * memory policy
 * define it to use dma_xxx series APIs, which provide write-back
 * memory type, otherwise use kmalloc+set_memory_uc to get uncached-
 * minus memory type.
 */
#define USE_DMA_COHERENT

/* specify if platform is palladium */
#define PALLADIUM_CLK_RATIO 4000
#define DELAY_MS 20000
#define POLLING_MS 1

#define BL1_VERSION_BASE 0x25050100
#define BL1_VERSION_SIZE 0x40
#define BL2_VERSION_BASE (BL1_VERSION_BASE + BL1_VERSION_SIZE) // 0x101fb240
#define BL2_VERSION_SIZE 0x40
#define BL31_VERSION_BASE (BL2_VERSION_BASE + BL2_VERSION_SIZE) // 0x101fb280
#define BL31_VERSION_SIZE 0x40
#define UBOOT_VERSION_BASE (BL31_VERSION_BASE + BL31_VERSION_SIZE) // 0x101fb2c0
#define UBOOT_VERSION_SIZE 0x50
#define CHIP_VERSION_BASE 0x27102014
#define CHIP_VERSION_SIZE 0x4


#define BM_MAX_CARD_NUM	                1
#define BM_MAX_CHIP_NUM                 1


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
	struct bm_device_info *card_bmdi[BM_MAX_CHIP_NUM_PER_CARD];
	struct bm_device_info *first_probe_bmdi;
};


int bm_get_card_info(struct bm_card *bmcd);



typedef enum {
	DEVICE,
	PALLADIUM,
	FPGA
} PLATFORM;


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

struct bootloader_version {
	char *bl1_version;
	char *bl2_version;
	char *bl31_version;
	char *uboot_version;
	char *chip_version;
};

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
	const struct bm_card_reg *bm_reg;
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
	struct bootloader_version version;
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
	struct clk *top_fab0_clk;
	struct clk *timer_clk;

	int (*bmdrv_setup_bar_dev_layout)(struct bm_device_info *bmdi, BAR_LAYOUT_TYPE type);

	u32 (*bmdrv_pending_msgirq_cnt)(struct bm_device_info *bmdi);
};

typedef int tpu_kernel_function_t;





typedef enum {
	MMAP_GDMA = 0,
	MMAP_SYS,
	MMAP_REG,
	MMAP_SMEM,
	MMAP_LMEM
} TPU_SYS_NUM;
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

	struct bm_api_info api_info[BM_MAX_CORE_NUM][2];

	struct bm_gmem_info gmem_info;

	struct list_head handle_list;

	struct mutex clk_reset_mutex;

	struct bm_misc_info misc_info;

	struct bm_boot_info boot_info;

	struct bm_profile profile;

	struct proc_dir_entry *proc_dir;

};

// #define PR_DEBUG

#ifdef PR_DEBUG
#define PR_TRACE(fmt, ...) pr_info(fmt, ##__VA_ARGS__) // to minimize print
#else
#define PR_TRACE(fmt, ...)
#endif

#define REG_COUNT 5

#endif /* _BM_COMMON_H_ */
