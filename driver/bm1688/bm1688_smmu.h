#ifndef BM1688_SMMU_H
#define BM1688_SMMU_H

#include "bm_cdma.h"

struct bm1688_iommu_entry {
    u32 paddr : 30; // right shifted 12bits from original physical address
    u32 dma_task_end : 1;
    u32 valid : 1;
} __packed;

int bm1688_enable_smmu_transfer(struct bm_memcpy_info *memcpy_info, struct iommu_region *iommu_rgn_src, struct iommu_region *iommu_rgn_dst, struct bm_buffer_object **bo_buffer);
int bm1688_disable_smmu_transfer(struct bm_memcpy_info *memcpy_info, struct iommu_region *iommu_rgn_src, struct iommu_region *iommu_rgn_dst, struct bm_buffer_object **bo_buffer);
int bm1688_init_iommu(struct iommu_ctrl *ctrl, struct device *dev);
int bm1688_deinit_iommu(struct iommu_ctrl *ctrl);
#ifdef SOC_MODE
void bm1688_smmu_reset(struct bm_device_info *bmdi);
void bm1688_smmu_clk_enable(struct bm_device_info *bmdi);
void bm1688_smmu_clk_disable(struct bm_device_info *bmdi);
void bm1688_pcie_clk_enable(struct bm_device_info *bmdi);
void bm1688_pcie_clk_disable(struct bm_device_info *bmdi);
#endif
#endif /* _BM_IOMMU_H_ */
