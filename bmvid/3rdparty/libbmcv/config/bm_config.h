#ifndef __BM_CONFIG_H__
#define __BM_CONFIG_H__

#define NPU_SHIFT               (CONFIG_NPU_SHIFT)
#define EU_SHIFT                (CONFIG_EU_SHIFT)
#define LOCAL_MEM_SIZE          (1<<CONFIG_LOCAL_MEM_ADDRWIDTH)
#define NPU_NUM                 (1<<NPU_SHIFT)
#define NPU_MASK                (NPU_NUM - 1)
#define EU_NUM                  (1<<EU_SHIFT)
#define EU_MASK                 (EU_NUM  - 1)
#define MAX_ROI_NUM             200
#define KERNEL_MEM_SIZE         CONFIG_KERNEL_MEM_SIZE
#define L2_SRAM_SIZE            CONFIG_L2_SRAM_SIZE
#define ADDR_ALIGN_BYTES        (EU_NUM * 4)
#endif /* __BM_CONFIG_H__ */
