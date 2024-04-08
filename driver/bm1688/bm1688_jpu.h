#ifndef __JPU_DRV_H__
#define __JPU_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>
#include "../vpu/vmm_type.h"

#include "bm_debug.h"
#ifndef SOC_MODE
#define JDI_IOCTL_MAGIC 'J'
#define JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY       _IO(JDI_IOCTL_MAGIC, 0)
#define JDI_IOCTL_FREE_PHYSICAL_MEMORY           _IO(JDI_IOCTL_MAGIC, 1)
#define JDI_IOCTL_WAIT_INTERRUPT                 _IO(JDI_IOCTL_MAGIC, 2)
#define JDI_IOCTL_SET_CLOCK_GATE                 _IO(JDI_IOCTL_MAGIC, 3)
#define JDI_IOCTL_RESET                          _IO(JDI_IOCTL_MAGIC, 4)
#define JDI_IOCTL_GET_INSTANCE_POOL              _IO(JDI_IOCTL_MAGIC, 5)
#define JDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(JDI_IOCTL_MAGIC, 6)
#define JDI_IOCTL_OPEN_INSTANCE                  _IO(JDI_IOCTL_MAGIC, 7)
#define JDI_IOCTL_CLOSE_INSTANCE                 _IO(JDI_IOCTL_MAGIC, 8)
#define JDI_IOCTL_GET_INSTANCE_NUM               _IO(JDI_IOCTL_MAGIC, 9)
#define JDI_IOCTL_GET_REGISTER_INFO              _IO(JDI_IOCTL_MAGIC, 10)
#define JDI_IOCTL_GET_CONTROL_REG                _IO(JDI_IOCTL_MAGIC, 11)
#define JDI_IOCTL_GET_INSTANCE_CORE_INDEX        _IO(JDI_IOCTL_MAGIC, 12)
#define JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX      _IO(JDI_IOCTL_MAGIC, 13)
#define JDI_IOCTL_WRITE_VMEM                     _IO(JDI_IOCTL_MAGIC, 16)
#define JDI_IOCTL_READ_VMEM                      _IO(JDI_IOCTL_MAGIC, 17)
#define JDI_IOCTL_GET_MAX_NUM_JPU_CORE           _IO(JDI_IOCTL_MAGIC, 19)

#define MAX_NUM_BOARD    128
#define MAX_NUM_JPU_CORE 4
#define MAX_NUM_JPU_CORE_BM1686 2
#define MAX_INST_HANDLE_SIZE_JPU (12 * 1024)
#define MAX_NUM_INSTANCE_JPU MAX_NUM_JPU_CORE

#define JPU_CONTROL_REG 0x50010060
#define JPU_RST_REG     0x50010c04
#define JPU_REG_SIZE    0x300

#define JPU_TOP_CLK_EN_REG1 0x50010804
#define CLK_EN_AXI_VD0_JPEG1_GEN_REG1 29
#define CLK_EN_APB_VD0_JPEG1_GEN_REG1 28
#define CLK_EN_AXI_VD0_JPEG0_GEN_REG1 27
#define CLK_EN_APB_VD0_JPEG0_GEN_REG1 26
#define JPU_TOP_CLK_EN_REG2 0x50010808
#define CLK_EN_AXI_VD1_JPEG1_GEN_REG2 7
#define CLK_EN_APB_VD1_JPEG1_GEN_REG2 6
#define CLK_EN_AXI_VD1_JPEG0_GEN_REG2 5
#define CLK_EN_APB_VD1_JPEG0_GEN_REG2 4

#define JPU_CORE0_REG 0x50030000
#define JPU_CORE1_REG 0x50040000
#define JPU_CORE2_REG 0x500b0000
#define JPU_CORE3_REG 0x500c0000

#define JPU_CORE0_IRQ 28
#define JPU_CORE1_IRQ 29
#define JPU_CORE2_IRQ 112
#define JPU_CORE3_IRQ 113

#define JPU_CORE0_RST 20
#define JPU_CORE1_RST 21
#define JPU_CORE2_RST 30
#define JPU_CORE3_RST 31

typedef struct jpudrv_buffer_t {
	unsigned long phys_addr;
	unsigned long base;        /* kernel logical address in use kernel */
	unsigned long virt_addr;   /* virtual user space address */
	unsigned int  size;
	unsigned int  flags;       /* page prot flag. 0: default; 1: writecombine; 2: noncache */
} jpudrv_buffer_t;

typedef struct jpudrv_register_info{
	u32 reg;
	u32 bit_n;
}jpudrv_register_info_t;

typedef struct jpudrv_clkgate_info_t {
	unsigned int clkgate;
	int core_idx;
} jpudrv_clkgate_info_t;

typedef struct jpudrv_intr_info_t {
	unsigned int timeout;
	unsigned int core_idx;
} jpudrv_intr_info_t;

typedef struct jpudrv_remap_info_t {
	unsigned long read_addr;
	unsigned long write_addr;
	int core_idx;
} jpudrv_remap_info_t;

typedef struct dcache_range_t {
	unsigned long start;
	unsigned long size;
} dcache_range_t;

/* To track the instance index and buffer in instance pool */
typedef struct jpudrv_instance_list_t {
	struct list_head list;
	unsigned long core_idx;
	int inuse;
	struct file *filp;
} jpudrv_instance_list_t;

typedef struct jpudrv_instance_pool_t {
	unsigned char jpgInstPool[MAX_NUM_INSTANCE_JPU][MAX_INST_HANDLE_SIZE_JPU];
} jpudrv_instance_pool_t;

#define MAX_JPU_STAT_WIN_SIZE  100
typedef struct jpu_statistic_info {
	int jpu_open_status[MAX_NUM_JPU_CORE];
	uint64_t jpu_working_time_in_ms[MAX_NUM_JPU_CORE];
	uint64_t jpu_total_time_in_ms[MAX_NUM_JPU_CORE];
	atomic_t jpu_busy_status[MAX_NUM_JPU_CORE];
	char jpu_status_array[MAX_NUM_JPU_CORE][MAX_JPU_STAT_WIN_SIZE];
	int jpu_status_index[MAX_NUM_JPU_CORE];
	int jpu_core_usage[MAX_NUM_JPU_CORE];
	int jpu_instant_interval;
}jpu_statistic_info_t;

typedef struct jpu_drv_context {
	struct semaphore jpu_sem;
	struct mutex jpu_mem_lock;
	struct mutex jpu_core_lock;
	struct mutex buffer_lock;
	struct list_head jbp_head;
	struct list_head inst_head;
	int core[MAX_NUM_JPU_CORE];
	int jpu_irq[MAX_NUM_JPU_CORE];
	int interrupt_flag[MAX_NUM_JPU_CORE];
	jpu_mm_t jmem;
	jpudrv_buffer_t video_memory;
	jpudrv_buffer_t jpu_control_register;
	jpudrv_buffer_t jpu_register[MAX_NUM_JPU_CORE];
	wait_queue_head_t interrupt_wait_q[MAX_NUM_JPU_CORE];
	jpu_statistic_info_t s_jpu_usage_info;
} jpu_drv_context_t;

extern const struct BM_PROC_FILE_OPS bmdrv_jpu_file_ops;
void bm_jpu_request_irq(struct bm_device_info *bmdi);
void bm_jpu_free_irq(struct bm_device_info *bmdi);
long bm_jpu_ioctl(struct file *filp, u_int cmd, u_long arg);
int bm_jpu_open(struct inode *inode, struct file *filp);
int bm_jpu_release(struct inode *inode, struct file *filp);
int bm_jpu_mmap(struct file *filp, struct vm_area_struct *vm);
int bm_jpu_addr_judge(unsigned long addr, struct bm_device_info *bmdi);
int bmdrv_jpu_init(struct bm_device_info *bmdi);
int bmdrv_jpu_exit(struct bm_device_info *bmdi);
int bm_jpu_check_usage_info(struct bm_device_info *bmdi);
int bm_jpu_set_interval(struct bm_device_info *bmdi, int time_interval);

#endif
#endif
