#ifndef _VDI_MMU_H_
#define _VDI_MMU_H_

/* include vpu mmu register */
#include "vdi_mmu_reg.h"

/* memory base address */
#define VDI_DRAM_PHYSICAL_BASE	    (0x438480000ULL)
/* system 0 and 1 DRAM size */
#define VDI_DRAM_DATA_SIZE	        (0x40000000ULL)                       // (1024*1024*1024L)

/* local ram address */
#define VDI_LRAM_PHYSICAL_BASE0     (0x12000000ULL)
#define VDI_LRAM_PHYSICAL_BASE1     (0x13000000ULL)
#define VDI_LRAM_PHYSICAL_SIZE      (0x40000ULL)        // local ram size 256K

/* system 0 and 1 mva address */
#define VDI_DRAM_MVA_BASE0          (0x000000000ULL)
#define VDI_DRAM_MVA_BASE1          (0x000000000ULL)

/* vpu page table size (default 1024K page size) */
#define VDI_DRAM_PT_SIZE            (0x100000ULL)                              // 1024K

/* system 0 and 1 page table address */
#define VDI_DRAM_PT_BASE0	        (VDI_DRAM_PHYSICAL_BASE)                   // 0x428480000
#define VDI_DRAM_PT_BASE1	        (VDI_DRAM_PHYSICAL_BASE)                   // 0x428480000

/* system 0 and 1 DRAM address */
#define VDI_DRAM_DATA_BASE0	        (VDI_DRAM_PT_BASE0+VDI_DRAM_PT_SIZE)     // 0x428580000
#define VDI_DRAM_DATA_BASE1	        (VDI_DRAM_PT_BASE1+VDI_DRAM_PT_SIZE) // 0x428580000

/* vpu system address width */
#define VDI_DRAM_DATA_WIDTH         (0xFFFFFFFFFFULL)   // 40BITS

/* vpu page size (default 128K) */
#define VDI_PAGE_SIZE_4K            (0x1000ULL)         // 4K
#define VDI_PAGE_SIZE_32K           (0x8000ULL)         // 32K
#define VDI_PAGE_SIZE_64K           (0x10000ULL)        // 64K
#define VDI_PAGE_SIZE_128K          (0x20000ULL)        // 128K

/* page address shift bits */
#define VDI_PAGE_SHIFT_4K           (12)                // 4K
#define VDI_PAGE_SHIFT_32K          (15)                // 32K
#define VDI_PAGE_SHIFT_64K          (16)                // 64K
#define VDI_PAGE_SHIFT_128K         (17)                // 128K

/* physic address size at pte */
#define VDI_PAGE_4K_MASK            (0xFFFFFFF)         // 28BITS
#define VDI_PAGE_32K_MASK           (0x1FFFFFF)         // 25BITS
#define VDI_PAGE_64K_MASK           (0xFFFFFF)          // 24BITS
#define VDI_PAGE_128K_MASK          (0x7FFFFF)          // 23BITS

/* pte reserve bit */
/*        | 31    | 30  | 29:28 | 27                0 | */
/* 4K  :  | Valid | End |  Res  | PA[39:12]           | */
/* 32K :  | Valid | End |  Res  | PA[39:15]   | 2:0   | */
/* 64K :  | Valid | End |  Res  | PA[39:16]  | 3:0    | */
/* 128K:  | Valid | End |  Res  | PA[39:17] | 4:0     | */
#define VDI_PTE_4K_RESERVE          (0)                 // 4K
#define VDI_PTE_32K_RESERVE         (3)                 // 32K
#define VDI_PTE_64K_RESERVE         (4)                 // 64K
#define VDI_PTE_128K_RESERVE        (5)                 // 128K

/* define video subsystem video mmu page size */
#if   (defined VIDEO_MMU_PAGE_4K)
    #define VDI_PAGE_SIZE       VDI_PAGE_SIZE_4K
    #define VDI_PAGE_SHIFT      VDI_PAGE_SHIFT_4K
    #define VDI_PTE_RESERVE     VDI_PTE_4K_RESERVE
    #define VDI_PAGE_MASK       VDI_PAGE_4K_MASK
#elif (defined VIDEO_MMU_PAGE_32K )
    #define VDI_PAGE_SIZE       VDI_PAGE_SIZE_32K
    #define VDI_PAGE_SHIFT      VDI_PAGE_SHIFT_32K
    #define VDI_PTE_RESERVE     VDI_PTE_32K_RESERVE
    #define VDI_PAGE_MASK       VDI_PAGE_32K_MASK
#elif (defined VIDEO_MMU_PAGE_64K )
    #define VDI_PAGE_SIZE       VDI_PAGE_SIZE_64K
    #define VDI_PAGE_SHIFT      VDI_PAGE_SHIFT_64K
    #define VDI_PTE_RESERVE     VDI_PTE_64K_RESERVE
    #define VDI_PAGE_MASK       VDI_PAGE_64K_MASK
#elif (defined VIDEO_MMU_PAGE_128K )
    #define VDI_PAGE_SIZE       VDI_PAGE_SIZE_128K
    #define VDI_PAGE_SHIFT      VDI_PAGE_SHIFT_128K
    #define VDI_PTE_RESERVE     VDI_PTE_128K_RESERVE
    #define VDI_PAGE_MASK       VDI_PAGE_128K_MASK
#else
    #error "!!YOU MUST DEFINE PAGE SIZE MACRO FIRST!!"
#endif

struct vpu_iommu_entry
{
#if   (defined VIDEO_MMU_PAGE_4K)
    unsigned int paddr        : 28;
#elif (defined VIDEO_MMU_PAGE_32K )    
    unsigned int reserve1     : 3;
    unsigned int paddr        : 25;
#elif (defined VIDEO_MMU_PAGE_64K )
    unsigned int reserve1     : 4;
    unsigned int paddr        : 24;
#elif (defined VIDEO_MMU_PAGE_128K )
    unsigned int reserve1     : 5;
    unsigned int paddr        : 23;
#else
    #error "!!YOU MUST DEFINE PAGE SIZE MACRO FIRST!!"
#endif        
	unsigned int reserve2     : 2;
	unsigned int dma_task_end : 1;
	unsigned int valid        : 1;
} __packed;

typedef struct vpu_iommu_pt
{
    /* page address at SDRAM: PTR */
	unsigned long alloc_ptr;

    /* page entry number */
	unsigned int  entry_num;

	unsigned int  enabled;

    struct vpu_iommu_entry *entry_base;

} vpu_iommu_pt;

void init_vpu_iommu(int system_id );
void uninit_vpu_iommu(int system_id);
//unsigned int wmmu_irq(unsigned int system_id, int channel);
//#if (defined VIDEO_MMU)
unsigned long get_mmu_phyaddr(int system_id, unsigned long addr);
//#endif /* (defined VIDEO_MMU) */

#endif /* _VDI_MMU_H_ */
