#ifndef _BM_MEMCPY_H_
#define _BM_MEMCPY_H_

#define CONFIG_HOST_REALMEM_SIZE 0x400000

struct bm_stagemem {
	void *v_addr;
	u64 p_addr;
	u32 size;
	////struct mutex stage_mutex;
};

struct mmap_info {
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
};

struct bm_memcpy_info {
	struct mmap_info mminfo;

    KEVENT cdma_completionEvent;
    FAST_MUTEX    cdma_Mutex;
    FAST_MUTEX    s2dstag_Mutex;
    FAST_MUTEX    d2sstag_Mutex;

    WDFDMAENABLER WdfDmaEnabler;

    u32            stagging_mem_size;

	WDFCOMMONBUFFER  s2dCommonBuffer;
    PUCHAR           s2dstagging_mem_vaddr;
    PHYSICAL_ADDRESS s2dstagging_mem_paddr;  // Logical Address (PHY ADDR)

	WDFCOMMONBUFFER  d2sCommonBuffer;
    PUCHAR           d2sstagging_mem_vaddr;
    PHYSICAL_ADDRESS d2sstagging_mem_paddr;  // Logical Address (PHY ADDR)

	WDFDMATRANSACTION s2dDmaTransaction;
    WDFDMATRANSACTION d2sDmaTransaction;

	int cdma_max_payload;

	int (*bm_memcpy_init)(struct bm_device_info *);
	void (*bm_memcpy_deinit)(struct bm_device_info *);
	u32 (*bm_cdma_transfer)(struct bm_device_info *, pbm_cdma_arg, bool, WDFFILEOBJECT);
};

struct bm_memcpy_param {
    void *  host_addr;
	u64 src_device_addr;
	u64 device_addr;
	u32 size;
	MEMCPY_DIR dir;
	u32 intr;
	bm_cdma_iommu_mode cdma_iommu_mode;
};

void bmdev_construct_cdma_arg(pbm_cdma_arg parg, u64 src, u64 dst, u64 size, MEMCPY_DIR dir,
		u32 intr, bool use_iommu);
int bmdrv_memcpy_init(struct bm_device_info *bmdi);
void bmdrv_memcpy_deinit(struct bm_device_info *bmdi);
//int bmdev_mmap(struct file *file, struct vm_area_struct *vma);
int bmdev_memcpy(struct bm_device_info *bmdi, _In_ WDFREQUEST Request);
int bmdev_memcpy_s2d_internal(struct bm_device_info *bmdi, u64 dst, const void *src, u32 size);
int bmdev_memcpy_s2d(struct bm_device_info *bmdi, WDFFILEOBJECT file, u64 dst, void *src, u32 size, bool intr, bm_cdma_iommu_mode cdma_iommu_mode);

#endif
