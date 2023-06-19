#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/mempool.h>
#include <linux/proc_fs.h>
#ifndef SOC_MODE
#include <linux/pci.h>
#include "bm_irq.h"
#include "vpu/vpu.h"
#include "bm1684/bm1684_jpu.h"
#include "vpp/vpp_platform.h"
#include "bm1684/bm1684_ce.h"
#include "bm_napi.h"
#include "sg_comm.h"
#else
#include <linux/platform_device.h>
#endif
#include "bm_api.h"
#include "bm_io.h"
#include "bm_trace.h"
#include "bm_memcpy.h"
#include "bm_gmem.h"
#include "bm_attr.h"
#include "bm_uapi.h"
#include "bm_msgfifo.h"
#include "bm_debug.h"
#include "bm_monitor.h"
#include "version.h"

#ifndef __maybe_unused
#define __maybe_unused      __attribute__((unused))
#endif

//#define PR_DEBUG

#define BM_CHIP_VERSION 	PROJECT_VER_MAJOR
#define BM_MAJOR_VERSION	PROJECT_VER_MINOR
#define BM_MINOR_VERSION	PROJECT_VER_PATCH
#define BM_DRIVER_VERSION	((BM_CHIP_VERSION<<16) | (BM_MAJOR_VERSION<<8) | BM_MINOR_VERSION)

#ifdef SOC_MODE
#define MAX_CARD_NUMBER		1
#else
#define MAX_CARD_NUMBER		64
#endif

#ifdef SOC_MODE
#define BM_CLASS_NAME   "bm-tpu"
#define BM_CDEV_NAME	"bm-tpu"
#else
#define BM_CLASS_NAME   "bm-sophon"
#define BM_CDEV_NAME	"bm-sophon"
#endif

#define BMDEV_CTL_NAME		"bmdev-ctl"

#define A53_RESET_STATUS_TRUE    1
#define A53_RESET_STATUS_FALSE   0
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

#define SRAM_RESERVE_BASE		0x101FB000
#define BL1_VERSION_OFFSET		0x200
#define BL1_VERSION_BASE		(SRAM_RESERVE_BASE + BL1_VERSION_OFFSET)
#define BL1_VERSION_SIZE		0x40
#define BL2_VERSION_BASE		(BL1_VERSION_BASE + BL1_VERSION_SIZE) // 0x101fb240
#define BL2_VERSION_SIZE		0x40
#define BL31_VERSION_BASE		(BL2_VERSION_BASE + BL2_VERSION_SIZE) // 0x101fb280
#define BL31_VERSION_SIZE		0x40
#define UBOOT_VERSION_BASE		(BL31_VERSION_BASE + BL31_VERSION_SIZE) // 0x101fb2c0
#define UBOOT_VERSION_SIZE		0x50


typedef enum {
	DEVICE,
	PALLADIUM,
	FPGA
} PLATFORM;

enum {
	FIP_SRC_SPIF = 0X0, // SPI flash
	FIP_SRC_SRAM = 0X1, // SRAM upper half
	FIP_SRC_SDFT = 0X2, // SD card FAT32 partition
	FIP_SRC_INVALID = 0xFF,
};

enum {
	FIP_LOADED = (1 << 0), // fip.bin being loaded into RAM, set by PCIe host or JTAG
	SKIP_PCIEI = (1 << 1), // skip PCIe init during boot, set by warm reset routine
	PCIE_EP_LINKED = (1 << 2), // board has valid PCIe link to some PCIe host
	SOC_EP = (1 << 3), // Soc mode with PCIe EP linked
};

#ifdef PCIE_MODE_ENABLE_CPU
typedef enum {
	BMCPU_IDLE,
	BMCPU_RUNNING,
	BMCPU_FAULT
} BMCPU_STATUS;
#endif

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

struct bootloader_version{
	char *bl1_version;
	char *bl2_version;
	char *bl31_version;
	char *uboot_version;
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
#ifdef SOC_MODE
	u32 irq_id_cdma;
	u32 irq_id_msg;
	struct platform_device *pdev;
	struct reset_control *arm9;
	struct reset_control *tpu;
	struct reset_control *gdma;
	struct reset_control *smmu;
	struct reset_control *cdma;
	struct clk *tpu_clk;
	struct clk *gdma_clk;
	struct clk *pcie_clk;
	struct clk *smmu_clk;
	struct clk *cdma_clk;
	struct clk *fixed_tpu_clk;
	struct clk *intc_clk;
	struct clk *sram_clk;
	struct clk *arm9_clk;
	struct clk *hau_ngs_clk;
	struct clk *tpu_only_clk;
#else
	int a53_enable;
	unsigned long long heap2_size;
	int irq_id;
	char drv_irq_name[16];
	char boot_loader_version[2][64];
	char sn[18];
	u32 ddr_failmap;
	u32 ddr_ecc_correctN;
	struct pci_dev *pcidev;
	bool has_msi;
	int board_version; /*bit [8:15] board type, bit [0:7] hardware version*/
	int pcie_func_index;
	int boot_loader_num;
	bmdrv_submodule_irq_handler bmdrv_module_irq_handler[128];
	void (*bmdrv_map_bar)(struct bm_device_info *, struct pci_dev *);
	void (*bmdrv_unmap_bar)(struct bm_bar_info *);
	void (*bmdrv_pcie_calculate_cdma_max_payload)(struct bm_device_info *);
	void (*bmdrv_enable_irq)(struct bm_device_info *bmdi,
			int irq_num, bool irq_enable);
	void (*bmdrv_get_irq_status)(struct bm_device_info *bmdi,
			unsigned int *status);
	void (*bmdrv_unmaskall_intc_irq)(struct bm_device_info *bmdi);
#endif
	int (*bmdrv_setup_bar_dev_layout)(struct bm_device_info *bmdi, BAR_LAYOUT_TYPE type);

	void (*bmdrv_start_arm9)(struct bm_device_info *);
	void (*bmdrv_stop_arm9)(struct bm_device_info *);

	void (*bmdrv_clear_cdmairq)(struct bm_device_info *bmdi);
	int (*bmdrv_clear_msgirq)(struct bm_device_info *bmdi);
	int (*bmdrv_clear_cpu_msgirq)(struct bm_device_info *bmdi);

	u32 (*bmdrv_pending_msgirq_cnt)(struct bm_device_info *bmdi);
};

typedef int tpu_kernel_function_t;
typedef struct
{
	tpu_kernel_function_t f_id;
    unsigned char md5[16];
    char func_name[64];

	struct mutex bm_get_func_mutex;
}bm_get_func_t;

struct bmdrv_exec_func
{
	struct list_head func_list;
	bm_get_func_t exec_func;
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

	struct mutex device_mutex;
	struct chip_info cinfo;
	struct bm_chip_attr c_attr;
	struct bm_card *bmcd;
	int enable_dyn_freq;
	int dump_reg_type;
	int fixed_fan_speed;
	int status; /* active or fault */
	int status_pcie;
	int status_over_temp;
	int status_sync_api;
	u64 dev_refcount;

	struct bmcpu_lib *lib_info;

	struct bmcpu_lib *lib_dyn_info;

	struct bmdrv_exec_func exec_func;

	struct bm_api_info api_info[BM_MSGFIFO_CHANNEL_NUM];

	struct bm_gmem_info gmem_info;

	struct list_head handle_list;

	struct bm_memcpy_info memcpy_info;

	struct mutex clk_reset_mutex;

	struct bm_trace_info trace_info;

	struct bm_misc_info misc_info;

	struct bm_boot_info boot_info;

	struct bm_profile profile;

	struct bm_monitor_thread_info monitor_thread_info;

	struct proc_dir_entry *card_proc_dir;

	struct proc_dir_entry *proc_dir;

#ifndef SOC_MODE
	vpp_drv_context_t vppdrvctx;
	vpu_drv_context_t vpudrvctx;
	jpu_drv_context_t jpudrvctx;
	atomic_t dev_recovery;
	spacc_drv_context_t spaccdrvctx;
	struct mutex efuse_mutex;
	bool eth_state;
	struct eth_dev_info vir_eth;
	struct sg_comm_info comm;
	char firmware_info[50];
#endif
#ifdef PCIE_MODE_ENABLE_CPU
	int status_bmcpu;
	int status_reset;
#endif
};

struct bin_buffer {
	unsigned char *buf;
	unsigned int size;
	unsigned int target_addr;
};

#ifdef PR_DEBUG
#define PR_TRACE(fmt, ...) pr_info(fmt, ##__VA_ARGS__) // to minimize print
#else
#define PR_TRACE(fmt, ...)
#endif

#endif /* _BM_COMMON_H_ */
