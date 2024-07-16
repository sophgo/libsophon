#ifndef _BMLIB_MEMORY_H_
#define _BMLIB_MEMORY_H_

#ifdef __linux__
    #include <pthread.h>
//    #include "common.h"
#else
    //#include "../../common/bm1684/include_win/common_win.h"
#endif
#include "bmlib_runtime.h"
#include "bmlib_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
    #define PAGE_SIZE ((u64)getpagesize())
#else
    #define PAGE_SIZE ((u64)bm_getpagesize())
#endif
#define PAGE_MASK (~(PAGE_SIZE - 1))
typedef enum host_cdma_dir { HOST2CHIP, CHIP2HOST, CHIP2CHIP } HOST_CDMA_DIR;
typedef enum host_xfer_type { TRANS_1D, TRANS_2D } HOST_XFER_TYPE;
typedef struct bm_memcpy_info {
#ifdef __linux__
    void               *host_addr;
#else
    u64                host_addr;
#endif
    u64                src_device_addr;
    u64                device_addr;
	union {
		u32 size;
		struct {
			u16 width;
			u16 height;
			u16 src_width;
			u16 dst_width;
			u16 format;   //2:2-byte format, others:1-byte format
			u16 fixed_data;
			bool flush;
		};
	};
    HOST_CDMA_DIR      dir;
    HOST_XFER_TYPE     type;
#ifdef __linux__
    bool  intr;
#else
    u32                intr;
#endif

    bm_cdma_iommu_mode cdma_iommu_mode;
} bm_memcpy_info_t;

typedef struct bm_memcpy_p2p_info {
    u64                src_device_addr;
    u64                dst_device_addr;
    u64                dst_num;
    u32                size;
    bool               intr;
    bm_cdma_iommu_mode cdma_iommu_mode;
} bm_memcpy_p2p_info_t;

struct bm_gmem_addr {
	u64 vir_addr;
	u64 phy_addr;
};

bm_status_t bm_init_basic_func_id(bm_handle_t handle);
bm_status_t bm_total_gmem(bm_handle_t ctx, u64* total);
bm_status_t bm_avail_gmem(bm_handle_t ctx, u64* avail);
bm_status_t bm_memcpy_d2s_poll(bm_handle_t     handle,
                               void *          dst,
                               bm_device_mem_t src,
                               unsigned int    size);
bm_status_t sg_memcpy_d2s_poll(bm_handle_t     handle,
                               void *          dst,
                               sg_device_mem_t src,
                               unsigned long long size);
bm_status_t bm_memcpy_d2s_poll_u64(bm_handle_t     handle,
                               void *          dst,
                               bm_device_mem_u64_t src,
                               unsigned long long size);
bm_status_t bm_memcpy_s2d_poll(bm_handle_t     handle,
                               bm_device_mem_t dst,
                               void *          src);
bm_status_t sg_memcpy_s2d_poll(bm_handle_t     handle,
                               sg_device_mem_t dst,
                               void *          src);
bm_status_t bm_memcpy_s2d_poll_u64(bm_handle_t     handle,
                               bm_device_mem_u64_t dst,
                               void *          src);
void *bm_mem_get_system_addr(struct bm_mem_desc mem);
u32 bm_mem_get_size(struct bm_mem_desc mem);
u64 sg_mem_get_size(struct sg_mem_desc mem);
u64 bm_mem_get_size_u64(struct bm_mem_desc_u64 mem);
bm_status_t bm_mem_mmap_device_mem(
    bm_handle_t      handle,
    bm_device_mem_t *dmem,
    u64 *            vmem);

bm_status_t bm_mem_mmap_device_mem_no_cache(
    bm_handle_t      handle,
    bm_device_mem_t *dmem,
    u64 *            vmem);

bm_status_t bm_mem_invalidate_partial_device_mem(
    bm_handle_t      handle,
    bm_device_mem_t *dmem,
    u32              offset,
    u32              len);

bm_status_t bm_mem_invalidate_device_mem(
    bm_handle_t      handle,
    bm_device_mem_t *dmem);

bm_status_t bm_mem_flush_partial_device_mem(
    bm_handle_t      handle,
    bm_device_mem_t *dmem,
    u32              offset,
    u32              len);

bm_status_t bm_mem_flush_device_mem(
    bm_handle_t      handle,
    bm_device_mem_t *dmem);

bm_status_t bm_mem_unmap_device_mem(
    bm_handle_t      handle,
    void *           vmem,
    int              size);
bm_status_t bm_get_carveout_heap_id(bm_handle_t ctx);
#ifdef __cplusplus
}
#endif
#endif

