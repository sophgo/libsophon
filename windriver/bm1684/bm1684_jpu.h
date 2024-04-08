#ifndef __JPU_DRV_H__
#define __JPU_DRV_H__

#define JDI_IOCTL_MAGIC 'J'

#define JDI_IOCTL_WAIT_INTERRUPT               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x402, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_RESET                        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_GET_REGISTER_INFO            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x410, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_GET_INSTANCE_CORE_INDEX      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x412, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x413, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_WRITE_VMEM                    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x416, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_READ_VMEM                     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x417, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_WRITE_REGISTER                CTL_CODE(FILE_DEVICE_UNKNOWN, 0x418, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define JDI_IOCTL_READ_REGISTER                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x419, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_NUM_BOARD 128
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

#define MJPEG_PIC_START_REG         (0x000)  // [3] - start partial encoding  [2] - pic stop en/decoding[1] - pic init en/decoding [0] - pic start en/decoding
#define MJPEG_PIC_STATUS_REG        (0x004)  // [8] - stop [7:4] - partial buffer interrupt [3] - overflow, [2] - bbc interrupt, [1] - error, [0] - done
#define MJPEG_GBU_BBSR_REG          (0x140)
#define MJPEG_GBU_BBER_REG          (0x144)

typedef enum {
    JPG_START_PIC = 0,
    JPG_START_INIT,
    JPG_START_STOP,
    JPG_START_PARTIAL
}JpgStartCmd;

typedef enum {
    INT_JPU_DONE = 0,
    INT_JPU_ERROR = 1,
    INT_JPU_BIT_BUF_EMPTY = 2,
    INT_JPU_BIT_BUF_FULL = 2,
    INT_JPU_PARIAL_OVERFLOW = 3,
    INT_JPU_PARIAL_BUF0_EMPTY = 4,
    INT_JPU_PARIAL_BUF1_EMPTY,
    INT_JPU_PARIAL_BUF2_EMPTY,
    INT_JPU_PARIAL_BUF3_EMPTY,
    INT_JPU_BIT_BUF_STOP
} InterruptJpu;

typedef struct jpudrv_buffer_t {
	unsigned long long phys_addr;
	unsigned long long base;        /* kernel logical address in use kernel */
	unsigned long long virt_addr;   /* virtual user space address */
	unsigned int  size;
	unsigned int  flags;       /* page prot flag. 0: default; 1: writecombine; 2: noncache */
} jpudrv_buffer_t;

typedef struct jpu_register_info_t {
    unsigned int core_idx;
    unsigned int address_offset;
    unsigned int data;
} jpu_register_info_t;

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
	unsigned long long read_addr;
	unsigned long long write_addr;
	int core_idx;
} jpudrv_remap_info_t;

typedef struct dcache_range_t {
	unsigned long long start;
	unsigned long long size;
} dcache_range_t;

/* To track the instance index and buffer in instance pool */
typedef struct jpudrv_instance_list_t {
	LIST_ENTRY    list;
	int core_idx;
	int inuse;
	WDFFILEOBJECT filp;
} jpudrv_instance_list_t;

typedef struct jpudrv_instance_pool_t {
	unsigned char jpgInstPool[MAX_NUM_INSTANCE_JPU][MAX_INST_HANDLE_SIZE_JPU];
} jpudrv_instance_pool_t;

#define MAX_JPU_STAT_WIN_SIZE  100
typedef struct jpu_statistic_info {
	int jpu_open_status[MAX_NUM_JPU_CORE];
	u64 jpu_working_time_in_ms[MAX_NUM_JPU_CORE];
	u64 jpu_total_time_in_ms[MAX_NUM_JPU_CORE];
	LONG jpu_busy_status[MAX_NUM_JPU_CORE];
	char jpu_status_array[MAX_NUM_JPU_CORE][MAX_JPU_STAT_WIN_SIZE];
	int jpu_status_index[MAX_NUM_JPU_CORE];
	int jpu_core_usage[MAX_NUM_JPU_CORE];
	int jpu_instant_interval;
}jpu_statistic_info_t;

typedef struct jpu_drv_context {
	LIST_ENTRY    inst_head;
	KSEMAPHORE    jpu_sem;
	FAST_MUTEX    jpu_core_lock;
	FAST_MUTEX    jpu_proc_lock;
	int core[MAX_NUM_JPU_CORE];
	int jpu_irq[MAX_NUM_JPU_CORE];
	int pic_status[MAX_NUM_JPU_CORE];
	jpudrv_buffer_t jpu_control_register;
	jpudrv_buffer_t jpu_register[MAX_NUM_JPU_CORE];
	KEVENT        interrupt_wait_q[MAX_NUM_JPU_CORE];
	jpu_statistic_info_t s_jpu_usage_info;
} jpu_drv_context_t;

//extern const struct file_operations bmdrv_jpu_file_ops;
void bm_jpu_request_irq(struct bm_device_info *bmdi);
void bm_jpu_free_irq(struct bm_device_info *bmdi);
int bm_jpu_ioctl(struct bm_device_info *bmdi, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength, _In_ u32 IoControlCode);
//int bm_jpu_open(struct inode *inode);
int bm_jpu_release(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp);
//int bm_jpu_mmap(struct file *filp, struct vm_area_struct *vm);
int bm_jpu_addr_judge(unsigned long long addr, struct bm_device_info *bmdi);
int bmdrv_jpu_init(struct bm_device_info *bmdi);
int bmdrv_jpu_exit(struct bm_device_info *bmdi);
int bm_jpu_check_usage_info(struct bm_device_info *bmdi);
int bm_jpu_set_interval(struct bm_device_info *bmdi, int time_interval);

#endif
