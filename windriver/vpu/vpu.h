//=========================================================================
//  This file is linux device driver for VPU.
//-------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2015  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//=========================================================================

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x300, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_FREE_PHYSICALMEMORY            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x301, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WAIT_INTERRUPT                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x302, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SET_CLOCK_GATE                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x303, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_RESET                          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x304, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_INSTANCE_POOL              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x305, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_COMMON_MEMORY              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x306, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x308, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_OPEN_INSTANCE                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x309, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_CLOSE_INSTANCE                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_INSTANCE_NUM               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_REGISTER_INFO              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_FREE_MEM_SIZE              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_FIRMWARE_STATUS            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30e, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_VMEM                     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30f, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_READ_VMEM                      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x310, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_SET_STATUS              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x311, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_GET_STATUS              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x312, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_CHK_STATUS              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x313, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_SYSCXT_SET_EN                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x314, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_READ_HPI_REGISTER              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x315, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_HPI_FIRMWARE_REGISTER    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x316, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_HPI_REGRW_REGISTER       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x317, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_READ_REGISTER                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x318, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRIET_REGISTER                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x319, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_MMAP_INSTANCEPOOL              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_MUNMAP_INSTANCEPOOL            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_CHIP_ID                    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_GET_MAX_CORE_NUM               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x31d, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct vpudrv_syscxt_info_s {
	u32 core_idx;
	u32 inst_idx;
	u32 core_status;
	u32 pos;
	u32 fun_en;
	u32 is_all_block;
	int is_sleep;
} vpudrv_syscxt_info_t;


typedef struct vpudrv_buffer_t {
    u32 size;
    u64 phys_addr;
    u64 base;      /* kernel logical address in use kernel */
    u64 virt_addr; /* virtual user space address */
    u32 core_idx;
    u32 reserved;
} vpudrv_buffer_t;

typedef struct vpu_bit_firmware_info_t {
    u32 size;          /* size of this structure*/
    u32 core_idx;
    u64 reg_base_offset;
    u16 bit_code[512];
} vpu_bit_firmware_info_t;

typedef struct vpu_register_info_t {
    u32 core_idx;
    u32 addr_offset;
    u32 data;
} vpu_register_info_t;

typedef struct vpudrv_inst_info_t {
    u32 core_idx;
    u32 inst_idx;
    int inst_open_count;       /* for output only*/
} vpudrv_inst_info_t;

typedef struct vpudrv_intr_info_t {
    u32 timeout;
    int            intr_reason;
//#ifdef SUPPORT_MULTI_INST_INTR
    int            intr_inst_index;
//#endif
    int         core_idx;
} vpudrv_intr_info_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
    LIST_ENTRY             list;
    struct vpudrv_buffer_t vb;
    WDFFILEOBJECT filp;
} vpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
    LIST_ENTRY    list;
    u64 inst_idx;
    u64 core_idx;
    WDFFILEOBJECT filp;
} vpudrv_instanace_list_t;

typedef struct vpudrv_instance_pool_t {
    u8   codecInstPool[MAX_NUM_INSTANCE_VPU][MAX_INST_HANDLE_SIZE_VPU];
    vpudrv_buffer_t vpu_common_buffer;
    int             vpu_instance_num;
    int             instance_pool_inited;
    void            *pendingInst;
    int             pendingInstIdxPlus1;
} vpudrv_instance_pool_t;

/* end customer definition */

typedef struct vpudrv_regrw_info_t {
    u32 size;
    u64 reg_base;
    u32 value[4];
    u32 mask[4];
} vpudrv_regrw_info_t;


typedef struct vpu_video_mem_op_t {
    u32 size; /* size of this structure */
    u64 src;
    u64 dst;
} vpu_video_mem_op_t;

typedef struct _vpu_reset_ctrl_t {
    struct reset_control *axi2_rst;
    struct reset_control *apb_video_rst;
    struct reset_control *video_axi_rst;
} vpu_reset_ctrl;

enum {
    SYSCXT_STATUS_WORKDING      = 0,
    SYSCXT_STATUS_EXCEPTION,
};

typedef struct vpu_crst_context_s {
    u32 instcall[MAX_NUM_INSTANCE_VPU];
    u32 status;
    u32 disable;
    
    u32 reset;
    u32 count;
} vpu_crst_context_t;

typedef struct vpu_core_idx_context_t {
    LIST_ENTRY   list;
    int core_idx;
    WDFFILEOBJECT filp;
} vpu_core_idx_context;

typedef struct vpu_core_idx_mmap_t {
    LIST_ENTRY   list;
    int core_idx;
    WDFFILEOBJECT filp;
    vpudrv_buffer_t  *vdb;
    u64       phys_addr;
    u64       virt_addr;
} vpu_core_idx_mmap;

typedef struct vpu_drv_context {
    //struct fasync_struct *async_queue;
    FAST_MUTEX  s_vpu_mutex;
    KSEMAPHORE  s_vpu_sem;  //May not be needed, because the memory is now allocated elsewhere
    LIST_ENTRY s_vbp_head;
    LIST_ENTRY s_inst_list_head;
    //struct proc_dir_entry *entry[64];
    u32 open_count;                     /*!<< device reference count. Not instance count */
    u32 max_num_vpu_core;
    u32 max_num_instance;
    u32 *s_vpu_dump_flag;
    u32 *s_vpu_irq;
    u32 *s_vpu_reg_phy_base;
    
    /* move some control variables here to support multi board */
    //video_mm_t s_vmemboda;
    //video_mm_t s_vmemwave;
    //video_mm_t s_vmem;
    vpudrv_buffer_t s_video_memory;
    
    /* end customer definition */
    vpudrv_buffer_t instance_pool[MAX_NUM_VPU_CORE];
    WDFMEMORY       instance_pool_mem_obj[MAX_NUM_VPU_CORE];
    
    vpudrv_buffer_t s_common_memory[MAX_NUM_VPU_CORE];
    vpudrv_buffer_t s_vpu_register[MAX_NUM_VPU_CORE];
    vpu_reset_ctrl vpu_rst_ctrl;
    int s_vpu_open_ref_count[MAX_NUM_VPU_CORE];
    vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE];
    
    // must allocate them
    u32 s_init_flag[MAX_NUM_VPU_CORE];
    u32 s_vpu_reg_store[MAX_NUM_VPU_CORE][64];

    u64               interrupt_reason[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
    int               interrupt_flag[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
    KEVENT            interrupt_wait_q[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
    struct kfifo      interrupt_pending_q[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
    WDFSPINLOCK  s_kfifo_lock[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];

    vpu_crst_context_t crst_cxt[MAX_NUM_VPU_CORE];
    LIST_ENTRY         s_core_idx_head;
    LIST_ENTRY         s_mmap_list;
} vpu_drv_context_t;

//#define DUMP_FLAG_MEM_SIZE_VPU (MAX_NUM_VPU_CORE*MAX_NUM_INSTANCE_VPU*sizeof(unsigned int)*5 + MAX_NUM_VPU_CORE*2*sizeof(unsigned int)_)

//struct bm_device_info;
int bm_vpu_init(struct bm_device_info *bmdi);
void bm_vpu_exit(struct bm_device_info *bmdi);
//
void bm_vpu_request_irq(struct bm_device_info *bmdi);
void bm_vpu_free_irq(struct bm_device_info *bmdi);

//ssize_t bm_vpu_read(struct file *filp, char  *buf, size_t len, loff_t *ppos);
//ssize_t bm_vpu_write(struct file *filp, const char  *buf, size_t len, loff_t *ppos);
long bm_vpu_ioctl(struct bm_device_info *bmdi, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength, _In_ u32 IoControlCode);
//int bm_vpu_mmap(struct file *fp, struct vm_area_struct *vm);
//int bm_vpu_fasync(int fd, struct file *filp, int mode);
//int bm_vpu_release(struct inode *inode, struct file *filp);
int bm_vpu_release(struct bm_device_info *bmdi, _In_ WDFFILEOBJECT filp);
int bm_vpu_release_byfilp(struct bm_device_info* bmdi, _In_ WDFFILEOBJECT filp);
int bm_vpu_open(struct bm_device_info *bmdi);
//int bm_vpu_open(struct inode *inode, struct file *filp);
//extern const struct file_operations bmdrv_vpu_file_ops;


extern int bmdev_memcpy_d2s(struct bm_device_info *bmdi, WDFFILEOBJECT file,
		void  *dst, u64 src, u32 size,
		u32 intr, bm_cdma_iommu_mode cdma_iommu_mode);
extern int bmdev_memcpy_s2d(struct bm_device_info *bmdi,  WDFFILEOBJECT file,
		u64 dst, void  *src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);

extern void bm_get_bar_base(struct bm_bar_info *pbar_info, u32 address, u64 *base);

extern void bm_get_bar_offset(struct bm_bar_info *pbar_info, u32 address,
		void  **bar_vaddr, u32 *offset);
#endif
