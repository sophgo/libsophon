#ifndef __BMLIB_PROFILE__
#define __BMLIB_PROFILE__

#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#ifdef _WIN32
//#include "..\..\common\bm1684\include_win\common_win.h"
#else
//#include "common.h"
#endif

#define PROFILE_MAX_SEND_API 1024
#define PROFILE_MAX_SYNC_API 1024
#define PROFILE_MAX_MARK 1024
#define PROFILE_MAX_MEMCPY 1024
#define PROFILE_MAX_MEM_INFO 1024

#pragma pack(1)
typedef struct {
    u32 api_id;
    u64 begin_usec;
} bm_profile_send_api_t;

typedef struct {
    u64 begin_usec;
    u64 end_usec;
} bm_profile_interval_t;
#pragma pack()

#pragma pack(1)
typedef enum {
    BLOCK_SUMMARY      = 1,
    BLOCK_COMPILER_LOG = 2,
    BLOCK_MONITOR_BDC  = 3,
    BLOCK_MONITOR_GDMA = 4,
    BLOCK_DYN_DATA     = 5,
    BLOCK_DYN_EXTRA    = 6,
    BLOCK_FIRMWARE_LOG = 7,
    BLOCK_CMD          = 8,
    BLOCK_BMLIB        = 9,
} profile_block_type_t;


typedef struct {
    u64 id;
    u64 begin_usec;
    u64 end_usec;
} bm_profile_mark_t;

typedef enum {
    ALLOC=0,
    FREE=1,
    INVALIDATE=2,
    FLUSH=3,
} bm_mem_op_type_t;

typedef struct {
    u64 device_addr;
    u32 size;
    u32 type;
    u64 begin_usec;
    u64 end_usec;
} bm_profile_mem_info_t;

typedef struct {
    u64 src_addr;
    u64 dst_addr;
    u32 dir;
    u32 size;
    u64 begin_usec;
    u64 end_usec;
} bm_profile_memcpy_t;

typedef struct {
    bm_profile_interval_t interval;
    bm_profile_send_api_t send_info[PROFILE_MAX_SEND_API];
    u32 num_send;
    bm_profile_interval_t sync_info[PROFILE_MAX_SYNC_API];
    u32 num_sync;
    bm_profile_mark_t mark_info[PROFILE_MAX_MARK];
    u32 num_mark;
    bm_profile_memcpy_t copy_info[PROFILE_MAX_MEMCPY];
    u32 num_copy;
    bm_profile_mem_info_t mem_info[PROFILE_MAX_MEM_INFO];
    u32 num_mem;
} bm_profile_bmlib_summary_t;

#pragma pack()

u64 get_usec();

typedef struct {
    size_t size;
    u8* ptr;
    bm_device_mem_t mem;
} buffer_pair_t;

typedef struct {
    u32 id;
    bm_profile_bmlib_summary_t summary;
    buffer_pair_t              gdma_buffer;
    buffer_pair_t              bdc_buffer;
    buffer_pair_t              arm_buffer;
    bm_perf_monitor            tpu_perf_monitor;
    bm_perf_monitor            gdma_perf_monitor;
    bm_device_mem_t            gdma_aligned_mem;

} bmlib_profile_t;

#ifdef __cplusplus
extern "C" {
#endif
    bm_status_t bm_profile_init(bm_handle_t handle, bool force_enable);
    void bm_profile_deinit(bm_handle_t handle);

    bm_profile_mark_t* bm_profile_mark_begin(bm_handle_t handle, u64 id);
    void bm_profile_mark_end(bm_handle_t handle, bm_profile_mark_t* mark);

    // used for internal
    void bm_profile_record_send_api(bm_handle_t handle, u32 api_id);
    void bm_profile_record_sync_begin(bm_handle_t handle);
    void bm_profile_record_sync_end(bm_handle_t handle);

    void bm_profile_record_memcpy_begin(bm_handle_t handle);
    void bm_profile_record_memcpy_end(bm_handle_t handle, u64 src_addr, u64 dst_addr, u32 size, u32 dir);

    void bm_profile_record_mem_begin(bm_handle_t handle);
    void bm_profile_record_mem_end(bm_handle_t handle, bm_mem_op_type_t type, u64 device_addr, u32 size);


#ifdef __cplusplus
}
#endif

#endif
