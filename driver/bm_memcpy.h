#ifndef _BM_MEMCPY_H_
#define _BM_MEMCPY_H_

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <asm/pgalloc.h>
#include "bm_cdma.h"

#ifdef SOC_MODE
#define CONFIG_HOST_REALMEM_SIZE 0x200000
#define STAGEMEM_SLOT_NUM 0x2
#define STAGEMEM_FULL ((0x1 << STAGEMEM_SLOT_NUM) - 0x1)
#define STAGEMEM_SLOT_SIZE (CONFIG_HOST_REALMEM_SIZE / STAGEMEM_SLOT_NUM)
#else
#define CONFIG_HOST_REALMEM_SIZE 0x400000
#define STAGEMEM_SLOT_NUM  0x4
#define STAGEMEM_FULL  ((0x1 << STAGEMEM_SLOT_NUM) - 0x1)
#define STAGEMEM_SLOT_SIZE (CONFIG_HOST_REALMEM_SIZE / STAGEMEM_SLOT_NUM)
#endif

struct bm_stagemem {
	void *v_addr;
	u64 p_addr;
	u32 size;
	u32 bitmap;
	struct mutex stage_mutex;
	struct completion stagemem_done;
};

struct mmap_info {
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
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
	u32 (*bm_cdma_transfer)(struct bm_device_info *, struct file *, pbm_cdma_arg, bool);
	u32 (*bm_dual_cdma_transfer)(struct bm_device_info *, struct file *, pbm_cdma_arg, pbm_cdma_arg, bool);
	int (*bm_disable_smmu_transfer)(struct bm_memcpy_info *, struct iommu_region *, struct iommu_region *, struct bm_buffer_object **);
	int (*bm_enable_smmu_transfer)(struct bm_memcpy_info *, struct iommu_region *, struct iommu_region *, struct bm_buffer_object **);
};

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

void bmdev_construct_cdma_arg(pbm_cdma_arg parg, u64 src, u64 dst, u64 size, MEMCPY_DIR dir,
		bool intr, bool use_iommu);
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
int bmdev_dual_cdma_memcpy(struct bm_device_info *bmdi, struct file *file, unsigned long arg);
int bmdev_memcpy_c2c(struct bm_device_info *bmdi, struct file *file, u64 src, u64 dst, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);
#endif
