#ifndef _BM_MEMCPY_H_
#define _BM_MEMCPY_H_

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <asm/pgalloc.h>


#define CONFIG_HOST_REALMEM_SIZE 0x200000
#define STAGEMEM_SLOT_NUM 0x2
#define STAGEMEM_FULL ((0x1 << STAGEMEM_SLOT_NUM) - 0x1)
#define STAGEMEM_SLOT_SIZE (CONFIG_HOST_REALMEM_SIZE / STAGEMEM_SLOT_NUM)


struct bm_stagemem {
	void *v_addr;
	u64 p_addr;
	u32 size;
	u32 bitmap;
	struct mutex stage_mutex;
	struct completion stagemem_done;
};

typedef enum {
    KERNEL_NOT_USE_IOMMU = 0,
    KERNEL_PRE_ALLOC_IOMMU,
    KERNEL_USER_SETUP_IOMMU
} bm_cdma_iommu_mode;

struct mmap_info {
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
};

struct iommu_ctrl {
    atomic_t supported;                             // low level support in chip side.
    //struct iommu_entry __iomem *device_entry_base;  // for , it is SMMU_PAGE_TABLE_BASE in BAR0

    //struct iommu_entry *host_entry_base;            // init: alloc SMMU_PAGE_ENTRY_NUM page table entry in host
    u64 host_entry_base_paddr;
    void* host_entry_base_vaddr;
    unsigned int entry_num;
    /* Manage page entry table as ring buffer,
     * (1) alloc_ptr: set when user setup iommu, wait for DMA flash
     * (2) free_ptr : set when iommu DMA flash done , run after alloc_ptr
     */
    unsigned int alloc_ptr;
    unsigned int free_ptr;
    unsigned int full;                              // Do we need it?  macro will better to judge entry full/empty
    unsigned int enabled;                           // support allocate VA address space, configure device page table
    bool enable;                                    // Where enable iommu for address translation.
    /* user try to setup iommu, but all the entry are used,
     * wait for page entry free
     */
    wait_queue_head_t entry_waitq;
    struct device *device;                           // for DMA , and debug control
};
struct bm_memcpy_info {
	struct bm_stagemem stagemem_s2d;
	struct bm_stagemem stagemem_d2s;

	struct mmap_info mminfo;

	struct completion cdma_done;
	struct mutex cdma_mutex;
	struct mutex p2p_mutex;
	int cdma_max_payload;

	struct iommu_ctrl iommuctl;
	int (*bm_memcpy_init)(struct bm_device_info *);
	void (*bm_memcpy_deinit)(struct bm_device_info *);
};

typedef enum memcpy_type
{
    TRANS_1D,
    TRANS_2D
} MEMCPY_TYPE;

struct bm_memcpy_param {
	void __user *host_addr;
	u64 src_device_addr;
	u64 device_addr;
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
	MEMCPY_DIR dir;
	MEMCPY_TYPE type;
	bool intr;
	bm_cdma_iommu_mode cdma_iommu_mode;
};

struct bm_memcpy_p2p_param {
    u64 src_device_addr;
    u64 dst_device_addr;
    u64 dst_num;
    u32 size;
    bool intr;
    bm_cdma_iommu_mode cdma_iommu_mode;
};

// void bmdev_construct_cdma_arg(pbm_cdma_arg parg, u64 src, u64 dst, u64 size, MEMCPY_DIR dir,
// 		bool intr, bool use_iommu);
int bmdrv_memcpy_init(struct bm_device_info *bmdi);
void bmdrv_memcpy_deinit(struct bm_device_info *bmdi);
int bmdev_mmap(struct file *file, struct vm_area_struct *vma);
int bmdrv_stagemem_init(struct bm_device_info *bmdi, struct bm_stagemem *stagemem);
int bmdrv_stagemem_release(struct bm_device_info *bmdi, struct bm_stagemem *stagemem);
int bmdrv_stagemem_alloc(struct bm_device_info *bmdi, u64 size, dma_addr_t *ppaddr, void **pvaddr);
int bmdrv_stagemem_free(struct bm_device_info *bmdi, u64 paddr, void *vaddr, u64 size);
int bmdev_memcpy(struct bm_device_info *bmdi, struct file *file, unsigned long arg);
int bmdev_memcpy_p2p(struct bm_device_info *bmdi, struct file *file, unsigned long arg);
int bmdev_memcpy_s2d_internal(struct bm_device_info *bmdi, u64 dst, const void *src, u32 size);
int bmdev_memcpy_d2s_internal(struct bm_device_info *bmdi, void *dst, u64 src, u32 size);
int bmdev_memcpy_s2d(struct bm_device_info *bmdi,  struct file *file,
		uint64_t dst, void __user *src, u32 size, bool intr, bm_cdma_iommu_mode cdma_iommu_mode);

#endif
