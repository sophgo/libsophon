#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/math64.h>
#include "bm_common.h"
#include "bm_memcpy.h"
// #include "bm_cdma.h"
#include "bm_gmem.h"

int bmdrv_memcpy_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	init_completion(&memcpy_info->cdma_done);
	mutex_init(&memcpy_info->cdma_mutex);
	mutex_init(&memcpy_info->p2p_mutex);

	ret = bmdrv_stagemem_init(bmdi, &memcpy_info->stagemem_s2d);
	if (ret < 0)
		return ret;

	ret = bmdrv_stagemem_init(bmdi, &memcpy_info->stagemem_d2s);
	if (ret < 0)
	{
		bmdrv_stagemem_release(bmdi, &bmdi->memcpy_info.stagemem_s2d);
		return ret;
	}

	switch (bmdi->cinfo.chip_id) {
	case CHIP_ID:
		// memcpy_info->bm_cdma_transfer = SGTPUV8_cdma_transfer;
		// memcpy_info->bm_dual_cdma_transfer = SGTPUV8_dual_cdma_transfer;
		// memcpy_info->bm_disable_smmu_transfer = SGTPUV8_disable_smmu_transfer;
		// memcpy_info->bm_enable_smmu_transfer = SGTPUV8_enable_smmu_transfer;
		break;
	default:
		return -EINVAL;
}
return ret;
}

void bmdrv_memcpy_deinit(struct bm_device_info *bmdi)
{
bmdrv_stagemem_release(bmdi, &bmdi->memcpy_info.stagemem_s2d);
bmdrv_stagemem_release(bmdi, &bmdi->memcpy_info.stagemem_d2s);
}

int bmdrv_stagemem_init(struct bm_device_info *bmdi, struct bm_stagemem *stagemem)
{
	int ret = 0;

	mutex_init(&stagemem->stage_mutex);
	init_completion(&stagemem->stagemem_done);
	ret = bmdrv_stagemem_alloc(bmdi, CONFIG_HOST_REALMEM_SIZE, &stagemem->p_addr,
															&stagemem->v_addr);
	if (ret)
		return ret;
	stagemem->size = CONFIG_HOST_REALMEM_SIZE;
	return ret;
}

int bmdrv_stagemem_release(struct bm_device_info *bmdi, struct bm_stagemem *stagemem)
{
	return bmdrv_stagemem_free(bmdi, stagemem->p_addr, stagemem->v_addr, stagemem->size);
}

int bmdrv_stagemem_free(struct bm_device_info *bmdi, u64 paddr, void *vaddr, u64 size)
{
	#ifdef USE_DMA_COHERENT
	dma_free_coherent(bmdi->cinfo.device, size, vaddr, paddr);
	#else
	set_memory_wb((unsigned long)vaddr, div_u64(size, PAGE_SIZE));
	free_pages((unsigned long)vaddr, get_order(size));
	#endif
	return 0;
}
	/*bit_n == 0, slot n is free, bit_n == 1, slot n is used*/
int caculate_stage_index(int *bitmap, int *index)
{
	int i = 0;

	for (i = 0; i < STAGEMEM_SLOT_NUM; i++)
	{
		if ((*bitmap & (0x1 << i)) == 0)
		{
			*index = i;
			*bitmap = *bitmap | (0x1 << i);
			return 0;
		}
	}
	return -1;
	}

int bmdrv_free_stagemem(struct bm_device_info *bmdi, MEMCPY_DIR dir, int index)
{
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	if (dir == HOST2CHIP)
	{
		mutex_lock(&memcpy_info->stagemem_s2d.stage_mutex);
		memcpy_info->stagemem_s2d.bitmap = memcpy_info->stagemem_s2d.bitmap & ~(0x1 << index);
		mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
		complete(&memcpy_info->stagemem_s2d.stagemem_done);
	}
	else if (dir == CHIP2HOST)
	{
		mutex_lock(&memcpy_info->stagemem_d2s.stage_mutex);
		memcpy_info->stagemem_d2s.bitmap = memcpy_info->stagemem_d2s.bitmap & ~(0x1 << index);
		mutex_unlock(&memcpy_info->stagemem_d2s.stage_mutex);
		complete(&memcpy_info->stagemem_d2s.stagemem_done);
	}
	return 0;
}

int bmdrv_get_stagemem(struct bm_device_info *bmdi, u64 *ppaddr,
												void **pvaddr, MEMCPY_DIR dir, int *index)
{

	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	retry:
	if (dir == HOST2CHIP)
	{
		mutex_lock(&memcpy_info->stagemem_s2d.stage_mutex);
		if (memcpy_info->stagemem_s2d.bitmap == STAGEMEM_FULL)
		{
			mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
			wait_for_completion(&memcpy_info->stagemem_s2d.stagemem_done);
			goto retry;
		}
		if (caculate_stage_index(&memcpy_info->stagemem_s2d.bitmap, index) < 0)
		{
			mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
			pr_err("bm-sophon%d caculate stage index fail\n", bmdi->dev_index);
			return -1;
		}

		*ppaddr = memcpy_info->stagemem_s2d.p_addr + (*index) * STAGEMEM_SLOT_SIZE;
		*pvaddr = memcpy_info->stagemem_s2d.v_addr + (*index) * STAGEMEM_SLOT_SIZE;

		mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
	}
	else if (dir == CHIP2HOST)
	{
		mutex_lock(&memcpy_info->stagemem_d2s.stage_mutex);
		if (memcpy_info->stagemem_d2s.bitmap == STAGEMEM_FULL)
		{
			mutex_unlock(&memcpy_info->stagemem_d2s.stage_mutex);
			wait_for_completion(&memcpy_info->stagemem_d2s.stagemem_done);
			goto retry;
		}

		if (caculate_stage_index(&memcpy_info->stagemem_d2s.bitmap, index) < 0)
		{
			mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
			pr_err("bm-sophon%d caculate stage index fail\n", bmdi->dev_index);
			return -1;
		}

		*ppaddr = memcpy_info->stagemem_d2s.p_addr + (*index) * STAGEMEM_SLOT_SIZE;
		*pvaddr = memcpy_info->stagemem_d2s.v_addr + (*index) * STAGEMEM_SLOT_SIZE;
		mutex_unlock(&memcpy_info->stagemem_d2s.stage_mutex);
	}
	return 0;
	}

int bmdrv_stagemem_alloc(struct bm_device_info *bmdi, u64 size, dma_addr_t *ppaddr,
													void **pvaddr)
	{
	u32 arg32l;
	void *vaddr;
	dma_addr_t paddr;

	arg32l = (u32)size;

	#ifdef USE_DMA_COHERENT
	vaddr = dma_alloc_coherent(bmdi->cinfo.device, arg32l, &paddr, GFP_USER);
	#else
	/*
		* use GFP_USER only would likely get paddr larger than 4GB,
		* GFP_DMA can ensure paddr within 32bit. dma_alloc_coherent
		* would likely do the same thing, check intel_alloc_coherent.
		*/
	vaddr = (void *)__get_free_pages(GFP_DMA | GFP_USER, get_order(size));
	memset(vaddr, 0, size);
	paddr = virt_to_phys(vaddr);
	/*
		* use uncached-minus type of memory explicitly.
		*/
	set_memory_uc((unsigned long)vaddr, div_u64(size, PAGE_SIZE));
	#endif
	if (!vaddr)
	{
		dev_err(bmdi->dev, "buffer allocation failed\n");
		return -ENOMEM;
	}
	*ppaddr = paddr;
	if (pvaddr != NULL)
		*pvaddr = vaddr;
	return 0;
	}

	// static void __iomem *mapped_io_gdma;
	// static void __iomem *mapped_io_sys;
	// static void __iomem *mapped_io_reg;
	// static void __iomem *mapped_io_smem;
	// static void __iomem *mapped_io_lmem;

static int bm_ioremapy(void __iomem **mapped_io, unsigned long phys_addr, unsigned long phys_size)
{
		//  Use ioremap to map a physical address to a virtual address
		if (*mapped_io == NULL) {
				*mapped_io = ioremap(phys_addr, phys_size);
				if (*mapped_io == NULL) {
						PR_TRACE("bm_ioremapy: ioremap failed for phys_addr=%lx, phys_size=%lx\n", phys_addr, phys_size);
						return -ENOMEM;
				}
		}
		PR_TRACE("bm_ioremapy: ioremap success for phys_addr=%lx, phys_size=%lx\n", phys_addr, phys_size);
		return 0;
	}

int bmdev_mmap(struct file *file, struct vm_area_struct *vma) {
	unsigned long start = vma->vm_start;  // Starting address of the mapping
	unsigned long end = vma->vm_end;      // Ending address of the mapping
	size_t length = end - start;          // Length of the mapping

	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;

	PR_TRACE("bmdev_mmap1: start=%lx, end=%lx, length=%lx, bmdi->MMAP_TPYE=%d\n", start, end, length, bmdi->MMAP_TPYE);
	vma->vm_flags |= VM_IO | VM_SHARED;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (bmdi->MMAP_TPYE == MMAP_GDMA) {
		if (length > TPU_GDMA_SIZE) {
				PR_TRACE("bmdev_mmap: size > GMDA size\n");
				return -EINVAL;
		}
		if (remap_pfn_range(vma, vma->vm_start, TPU_GDMA_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else if (bmdi->MMAP_TPYE == MMAP_SYS) {
		if (length > TPU_SYS_SIZE) {
			PR_TRACE("bmdev_mmap: size > SYS size\n");
			return -EINVAL;
		}
		// if (bm_ioremapy(&mapped_io_sys, TPU_SYS_BASE, TPU_SYS_SIZE) < 0) {
		//	return -ENOMEM;
		// }
		// PR_TRACE("bm_ioremapy: ioremap success for phys_addr=%lx, phys_size=%lu\n", TPU_SYS_BASE >> PAGE_SHIFT, length);
		if (remap_pfn_range(vma, vma->vm_start, TPU_SYS_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else if (bmdi->MMAP_TPYE == MMAP_REG) {
		if (length > TPU_REG_SIZE) {
			PR_TRACE("bmdev_mmap: size > REG size\n");
			return -EINVAL;
		}
		// if (bm_ioremapy(&mapped_io_reg, TPU_REG_BASE, TPU_REG_SIZE) < 0) {
		//	return -ENOMEM;
		// }
		if (remap_pfn_range(vma, vma->vm_start, TPU_REG_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else if (bmdi->MMAP_TPYE == MMAP_SMEM) {
		if (length > TPU_SMEM_SIZE) {
			PR_TRACE("bmdev_mmap: size > SMEM size\n");
			return -EINVAL;
		}
		// if (bm_ioremapy(&mapped_io_smem, TPU_SMEM_BASE, TPU_SMEM_SIZE) < 0) {
		//	return -ENOMEM;
		// }
		if (remap_pfn_range(vma, vma->vm_start, TPU_SMEM_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else if (bmdi->MMAP_TPYE == MMAP_LMEM) {
		if (length > TPU_LMEM_SIZE) {
			PR_TRACE("bmdev_mmap: size > LMEM size\n");
			return -EINVAL;
		}
		// if (bm_ioremapy(&mapped_io_lmem, TPU_LMEM_BASE, TPU_LMEM_SIZE) < 0) {
		//	return -ENOMEM;
		// }
		if (remap_pfn_range(vma, vma->vm_start, TPU_LMEM_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else {
		pr_err("bmdev_mmap: bmdi->MMAP_TPYE\n");
		return -EINVAL;
	}

	return 0;

}


