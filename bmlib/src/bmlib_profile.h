#ifndef __BMLIB_PROFILE__
#define __BMLIB_PROFILE__

#include <string>
#include <map>
#include <vector>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#ifdef _WIN32
//#include "..\..\common\bm1684\include_win\common_win.h"
#else
//#include "common.h"
#endif

#pragma pack(1)
typedef struct {
    u32 api_id;
    u64 begin_usec;
} bm_profile_send_api_t;

typedef struct {
    u32 core_id;
    u32 api_id;
    u64 begin_usec;
} bm_profile_send_api_to_core_t;

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
    BLOCK_BMLIB_EXTRA  = 10,
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

typedef enum {
    SEND_EXTRA,
    SYNC_EXTRA,
    MARK_EXTRA,
    COPY_EXTRA,
    MEM_EXTRA,
} bmlib_extra_type_t;


typedef struct {
    bm_profile_interval_t interval;
    std::vector<bm_profile_send_api_t> send_info;
    std::vector<bm_profile_interval_t> sync_info;
    std::vector<bm_profile_mark_t> mark_info;
    std::vector<bm_profile_memcpy_t> copy_info;
    std::vector<bm_profile_mem_info_t> mem_info;
    std::vector<unsigned char> extra_info;
} bm_profile_bmlib_summary_t;

#pragma pack()

u64 get_usec();

typedef struct {
    size_t size;
    u8* ptr;
    bm_device_mem_t mem;
} buffer_pair_t;

typedef struct {
    bm_profile_bmlib_summary_t summary;
    buffer_pair_t              gdma_buffer;
    buffer_pair_t              tiu_buffer;
    buffer_pair_t              mcu_buffer;
    bm_perf_monitor            tpu_perf_monitor;
    bm_perf_monitor            gdma_perf_monitor;
    bm_device_mem_t            gdma_aligned_mem;
    tpu_kernel_module_t        current_module;
    tpu_kernel_function_t      set_engine_profile_param_func_id;
    tpu_kernel_function_t      enable_profile_func_id;
    tpu_kernel_function_t      get_profile_func_id;
    bool                       save_per_api_mode;
    bool                       profile_running;
    int                        save_id;
    int                        is_dumped;
    std::map<tpu_kernel_function_t, std::string> func_name_map;
} bmlib_profile_core_info_t;
typedef struct {
    u32 id;
    int arch_code;
    std::string dir;
    std::vector<bmlib_profile_core_info_t> cores;
} bmlib_profile_t;

#ifdef __cplusplus
extern "C" {
#endif
    bm_status_t bm_profile_init(bm_handle_t handle, bool force_enable);
    void bm_profile_deinit(bm_handle_t handle);
    void bm_profile_load_module(bm_handle_t handle, tpu_kernel_module_t p_module, int core_id);
    void bm_profile_unload_module(bm_handle_t handle, tpu_kernel_module_t p_module, int core_id);

    bm_profile_mark_t* bm_profile_mark_begin(bm_handle_t handle, u64 id, int core_id);
    void bm_profile_mark_end(bm_handle_t handle, bm_profile_mark_t* mark, int core_id);

    // used for internal
    void bm_profile_dump_core(bm_handle_t handle, int core_id);
    void bm_profile_record_send_api(bm_handle_t handle, u32 api_id, const void* param, int core_id);
    void bm_profile_record_sync_begin(bm_handle_t handle, int core_id);
    void bm_profile_record_sync_end(bm_handle_t handle, int core_id);

    void bm_profile_record_memcpy_begin(bm_handle_t handle);
    void bm_profile_record_memcpy_end(bm_handle_t handle, u64 src_addr, u64 dst_addr, u32 size, u32 dir);

    void bm_profile_record_mem_begin(bm_handle_t handle);
    void bm_profile_record_mem_end(bm_handle_t handle, bm_mem_op_type_t type, u64 device_addr, u32 size);
    void bm_profile_add_extra(bmlib_profile_t* profile, bmlib_extra_type_t type, const std::string& info, int core_id);

    void bm_profile_func_map(bm_handle_t handle, tpu_kernel_function_t func, const std::string& name, int core_id);


#ifdef __cplusplus
}
#endif

#endif
