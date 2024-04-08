#ifndef _BM_CDMA_H_
#define _BM_CDMA_H_

struct bm_device_info;
struct bm_memcpy_info;
typedef enum memcpy_dir
{
    HOST2CHIP,
    CHIP2HOST,
    CHIP2CHIP
} MEMCPY_DIR;

typedef enum {
    KERNEL_NOT_USE_IOMMU = 0,
    KERNEL_PRE_ALLOC_IOMMU,
    KERNEL_USER_SETUP_IOMMU
} bm_cdma_iommu_mode;

typedef struct bm_cdma_arg {
	u64 src;
	u64 dst;
	u64 size;
	MEMCPY_DIR dir;
	u32 intr;
    bm_cdma_iommu_mode use_iommu;
} bm_cdma_arg, *pbm_cdma_arg;

void bm_cdma_request_irq(struct bm_device_info *bmdi);
void bm_cdma_free_irq(struct bm_device_info *bmdi);

void bmdrv_cdma_irq_handler(struct bm_device_info *bmdi);

#include "./bm1682/bm1682_cdma.h"
#include "./bm1684/bm1684_cdma.h"

#endif