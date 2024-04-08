#ifndef _BM_GMEM_H_
#define _BM_GMEM_H_

#define BM_MEM_ADDR_NULL (0xfffffffff)

#define MAX_HEAP_CNT 4

#define GMEM_NORMAL 0
#define GMEM_TPU_ONLY 1

#include "bm_common.h"

struct bm_device_info;

typedef enum {
    BM_MEM_TYPE_DEVICE      = 0,
    BM_MEM_TYPE_HOST        = 1,
    BM_MEM_TYPE_SYSTEM      = 2,
    BM_MEM_TYPE_INT8_DEVICE = 3,
    BM_MEM_TYPE_INVALID     = 4
} bm_mem_type_t;

#pragma warning(push)
#pragma warning(disable : 4214)
typedef union {
    struct {
        bm_mem_type_t mem_type : 3;
        unsigned int  gmem_heapid : 3;
        unsigned int  reserved : 26;
    } u;
    unsigned int rawflags;
} bm_mem_flags_t;
#pragma warning(pop)

typedef struct bm_mem_desc {
    union {
        struct {
            u64 device_addr;
            u32 reserved0;
            s32 dmabuf_fd;
        } device;
        struct {
            void *       system_addr;
            unsigned int reserved;
            int          reserved1;
        } system;
    } u;

    bm_mem_flags_t flags;
    unsigned int   size;
} bm_mem_desc_t;

typedef struct bm_mem_desc bm_device_mem_t;

typedef struct _FILE_OBJECT_CONTEXT {
    WDFFILEOBJECT FileObject;
    WDFDEVICE     DeviceObject;
} FILE_OBJECT_CONTEXT, *PFILE_OBJECT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILE_OBJECT_CONTEXT, FileObjectGetContext)

typedef struct gmem_buffer_t {
    u64 size;
    u64 phys_addr;
    u64 base;      /* kernel logical address in use kernel */
    u32 heap_id;
} gmem_buffer_t;

/* To track the allocated memory buffer */
typedef struct gmem_buffer_pool_t {
    LIST_ENTRY             list;
    struct gmem_buffer_t   gb;
    WDFFILEOBJECT          file;
} gmem_buffer_pool_t;

#define TABLE_NUM 32
struct bm_handle_info {
    LIST_ENTRY api_htable[TABLE_NUM];
    LIST_ENTRY list;
    WDFFILEOBJECT fileObject;  // fileObjectContext or fileObject
	pid_t open_pid;
    pid_t process_id;
	u64 gmem_used;
	u64 h_send_api_seq;
	u64 h_cpl_api_seq;
    WDFSPINLOCK h_api_seq_spinlock;
    KEVENT h_msg_doneEvent;
	int f_owner;
    LIST_ENTRY list_gmem;
};

struct reserved_mem_info {
	u64 armfw_addr;
	u64 armfw_size;

	u64 eutable_addr;
	u64 eutable_size;

	u64 armreserved_addr;
	u64 armreserved_size;

	u64 smmu_addr;
	u64 smmu_size;

	u64 warpaffine_addr;
	u64 warpaffine_size;

	u64 npureserved_addr[MAX_HEAP_CNT];
	u64 npureserved_size[MAX_HEAP_CNT];

	u64 vpu_vmem_addr;
	u64 vpu_vmem_size;

	u64 vpp_addr;
	u64 vpp_size;

};

struct bm_gmem_info {
    WDFSPINLOCK gmem_lock;
    u32                      heap_cnt;
    bm_gmem_heap_t           common_heap[MAX_HEAP_CNT];
	struct reserved_mem_info resmem_info;
	int (*bm_gmem_init)(struct bm_device_info *);
	void (*bm_gmem_deinit)(struct bm_device_info *);
};

int bmdrv_get_gmem_mode(struct bm_device_info *bmdi);
int bmdrv_gmem_init(struct bm_device_info *bmdi);
void bmdrv_gmem_deinit(struct bm_device_info *bmdi);

u64 bmdrv_gmem_total_size(struct bm_device_info *bmdi);
u64 bmdrv_gmem_avail_size(struct bm_device_info *bmdi);
void bmdrv_heap_mem_used(struct bm_device_info *bmdi, struct bm_dev_stat *stat);
int  bmdrv_gmem_alloc(struct bm_device_info *bmdi, struct ion_allocation_data *alloc_data);
u64 bmdrv_gmem_vpu_avail_size(struct bm_device_info *bmdi);

int  bmdev_gmem_get_handle_info(struct bm_device_info * bmdi,
                                WDFFILEOBJECT          file,
                                struct bm_handle_info **h_info);

int bmdrv_gmem_ioctl_total_size(struct bm_device_info *bmdi,
                                _In_ WDFREQUEST        Request,
                                _In_ size_t            OutputBufferLength,
                                _In_ size_t            InputBufferLength);

int bmdrv_gmem_ioctl_avail_size(struct bm_device_info *bmdi,
                                _In_ WDFREQUEST        Request,
                                _In_ size_t            OutputBufferLength,
                                _In_ size_t            InputBufferLength);
int bmdrv_gmem_ioctl_alloc_mem(struct bm_device_info *bmdi,
                               _In_ WDFREQUEST        Request,
                               _In_ size_t            OutputBufferLength,
                               _In_ size_t            InputBufferLength);
int bmdrv_gmem_ioctl_free_mem(struct bm_device_info *bmdi,
                              _In_ WDFREQUEST        Request,
                              _In_ size_t            OutputBufferLength,
                              _In_ size_t            InputBufferLength);
int bmdrv_ioctl_get_misc_info(struct bm_device_info *bmdi,
                              _In_ WDFREQUEST        Request,
                              _In_ size_t            OutputBufferLength,
                              _In_ size_t            InputBufferLength);
int bmdrv_gmem_ioctl_get_dev_stat(struct bm_device_info *bmdi,
                              _In_ WDFREQUEST        Request,
                              _In_ size_t            OutputBufferLength,
                              _In_ size_t            InputBufferLength);
int bmdrv_gmem_ioctl_get_heap_num(struct bm_device_info *bmdi,
                              _In_ WDFREQUEST        Request,
                              _In_ size_t            OutputBufferLength,
                              _In_ size_t            InputBufferLength);
int bmdrv_gmem_ioctl_get_heap_stat_byte_by_id(struct bm_device_info *bmdi,
                              _In_ WDFREQUEST        Request,
                              _In_ size_t            OutputBufferLength,
                              _In_ size_t            InputBufferLength);
#endif
