#ifndef _BM_API_H_
#define _BM_API_H_

#include "bm_common.h"
#include "bm_uapi.h"

#define API_P_ALLOC(_x) ExAllocatePoolWithTag(NonPagedPoolExecute, _x, 'API')
#define API_P_FREE(_x)  ExFreePoolWithTag(_x, 'API')

#define DEVICE_SYNC_MARKER 0xffff

#define API_FLAG_FINISH  4
#define API_FLAG_WAIT    2
#define API_FLAG_DONE    1

struct kfifo {
    unsigned char *buffer; /* the buffer holding the data */
    unsigned int   size;   /* the size of the allocated buffer */
    unsigned int   in;     /* data is added at offset (in % size) */
    unsigned int   out;    /* data is extracted from off. (out % size) */
    // lock;   /* protects concurrent modifications */
};

struct bm_device_info;
struct bm_thread_info;

struct bm_api_info {
	KEVENT msg_doneEvent;
	u64 msgirq_num;
/* sw_rp is used to manage share mameory */
	u32 sw_rp;

	u32 device_sync_last;
	u32 device_sync_cpl;
    KEVENT dev_sync_doneEvent;

    FAST_MUTEX   api_mutex;
	struct kfifo api_fifo;
    LIST_ENTRY   api_list;
    WDFSPINLOCK  api_fifo_spinlock;

	int (*bm_api_init)(struct bm_device_info *, u32 channel);
	void (*bm_api_deinit)(struct bm_device_info *, u32 channel);
};

struct bmcpu_lib{
    char lib_name[64];
    int refcount;
    int cur_rec;
    int rec[50];
    u8 md5[16];
	pid_t cur_pid;

    FAST_MUTEX bmcpu_lib_mutex;
    LIST_ENTRY lib_list;
};

typedef struct loaded_lib
{
  unsigned char md5[16];
  int loaded;
}loaded_lib_t;

typedef struct bm_api_cpu_load_library_internal {
    u64   process_handle;
    u8 *  library_path;
    void *library_addr;
    u32   size;
    u8    library_name[64];
    u8    md5[16];
    int   obj_handle;
    int   mv_handle;
} bm_api_cpu_load_library_internal_t;

typedef struct bm_api_dyn_cpu_load_library_internal {
    u8 *lib_path;
    void *lib_addr;
    u32 size;
    u8 lib_name[64];
	unsigned char md5[16];
	int cur_rec;
} bm_api_dyn_cpu_load_library_internal_t;


struct api_fifo_entry {
	struct bm_thread_info *thd_info;
	struct bm_handle_info *h_info;
	u32 thd_api_seq;
	u32 dev_api_seq;
	bm_api_id_t api_id;
	u64 sent_time_us;
	u64 global_api_seq;
	u32 api_done_flag;
	KEVENT api_doneEvent;
	u64 api_data;
};

struct api_list_entry {
	LIST_ENTRY api_list_node;
	struct api_fifo_entry api_entry;
};

#pragma pack(push, 1)
typedef struct bm_kapi_header {
	bm_api_id_t api_id;
	u32 api_size;/* size of payload, not including header */
	u64 api_handle;
	u32 api_seq;
	u32 duration;
	u32 result;
}bm_kapi_header_t ;
#pragma pack(pop)

typedef struct bm_kapi_opt_header {
	u64 global_api_seq;
	u64 api_data;
} bm_kapi_opt_header_t;

#define API_ENTRY_SIZE sizeof(struct api_fifo_entry)

int bmdrv_api_init(struct bm_device_info *bmdi, u32 channel);
void bmdrv_api_deinit(struct bm_device_info *bmdi, u32 channel);
int bmdrv_trigger_bmcpu(struct bm_device_info *bmdi,
                        _In_ WDFREQUEST        Request);
int bmdrv_set_bmcpu_status(struct bm_device_info *bmdi,
                           _In_ WDFREQUEST        Request);
int bmdrv_send_api(struct bm_device_info *bmdi,
                   _In_ WDFREQUEST        Request,
                    U8                    flag);
int bmdrv_query_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);
int bmdrv_thread_sync_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);
int bmdrv_handle_sync_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);
int bmdrv_device_sync_api(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);

int bm_read_1684x_evb_power(struct bm_device_info* bmdi, u32* power);

int bmdrv_loaded_lib(struct bm_device_info *bmdi,
                     _In_ WDFREQUEST        Request,
                     _In_ size_t            OutputBufferLength,
                     _In_ size_t            InputBufferLength);
#endif
