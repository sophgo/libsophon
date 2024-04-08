#include "bm_common.h"

#include "bm_memcpy.tmh"

int bmdrv_memcpy_init(struct bm_device_info *bmdi)
{
	int ret = 0;
    NTSTATUS               status;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
    WDF_DMA_ENABLER_CONFIG dmaConfig;

	memcpy_info->stagging_mem_size = CONFIG_HOST_REALMEM_SIZE;

	WDF_DMA_ENABLER_CONFIG_INIT(
        &dmaConfig, WdfDmaProfilePacket64, memcpy_info->stagging_mem_size);

	WdfDeviceSetAlignmentRequirement(bmdi->WdfDevice, 1);

    status = WdfDmaEnablerCreate(bmdi->WdfDevice,
                                 &dmaConfig,
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &memcpy_info->WdfDmaEnabler);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "WdfDmaEnblerCreate failed: %08X\n",
                    status);
        return status;
    }

	status = WdfCommonBufferCreate(memcpy_info->WdfDmaEnabler,
                                   memcpy_info->stagging_mem_size,
                                   WDF_NO_OBJECT_ATTRIBUTES,
                                   &memcpy_info->s2dCommonBuffer);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "s2dCommonBuffer failed: %!STATUS!",
                    status);
        return status;
    }

    memcpy_info->s2dstagging_mem_vaddr =
        WdfCommonBufferGetAlignedVirtualAddress(memcpy_info->s2dCommonBuffer);

    memcpy_info->s2dstagging_mem_paddr =
        WdfCommonBufferGetAlignedLogicalAddress(memcpy_info->s2dCommonBuffer);

    RtlZeroMemory(memcpy_info->s2dstagging_mem_vaddr,
                  memcpy_info->stagging_mem_size);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "s2dCommonBuffer 0x%p  (#0x%I64X), length %I64d",
                memcpy_info->s2dstagging_mem_vaddr,
                memcpy_info->s2dstagging_mem_paddr.QuadPart,
                WdfCommonBufferGetLength(memcpy_info->s2dCommonBuffer));

    status = WdfCommonBufferCreate(memcpy_info->WdfDmaEnabler,
                                   memcpy_info->stagging_mem_size,
                                   WDF_NO_OBJECT_ATTRIBUTES,
                                   &memcpy_info->d2sCommonBuffer);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "d2sCommonBuffer failed: %!STATUS!",
                    status);
        return status;
    }

    memcpy_info->d2sstagging_mem_vaddr =
        WdfCommonBufferGetAlignedVirtualAddress(memcpy_info->d2sCommonBuffer);

    memcpy_info->d2sstagging_mem_paddr =
        WdfCommonBufferGetAlignedLogicalAddress(memcpy_info->d2sCommonBuffer);

    RtlZeroMemory(memcpy_info->d2sstagging_mem_vaddr,
                  memcpy_info->stagging_mem_size);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "d2sCommonBuffer 0x%p  (#0x%I64X), length %I64d",
                memcpy_info->d2sstagging_mem_vaddr,
                memcpy_info->d2sstagging_mem_paddr.QuadPart,
                WdfCommonBufferGetLength(memcpy_info->d2sCommonBuffer));


	//WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);

    status = WdfDmaTransactionCreate(memcpy_info->WdfDmaEnabler,
                                     WDF_NO_OBJECT_ATTRIBUTES,
                                     &memcpy_info->s2dDmaTransaction);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "s2dDmaTransaction create failed: %!STATUS!",
                    status);
        return status;
    }

    //WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);
    //
    // Create a new DmaTransaction.
    //
    status = WdfDmaTransactionCreate(memcpy_info->WdfDmaEnabler,
                                     WDF_NO_OBJECT_ATTRIBUTES,
                                     &memcpy_info->d2sDmaTransaction);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "d2sDmaTransaction create failed: %!STATUS!",
                    status);
        return status;
    }

    KeInitializeEvent(&memcpy_info->cdma_completionEvent, SynchronizationEvent , FALSE);

    ExInitializeFastMutex(&memcpy_info->s2dstag_Mutex);
    ExInitializeFastMutex(&memcpy_info->d2sstag_Mutex);
    ExInitializeFastMutex(&memcpy_info->cdma_Mutex);

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		memcpy_info->bm_cdma_transfer = bm1682_cdma_transfer;
		break;
	case 0x1684:
    case 0x1686:
		memcpy_info->bm_cdma_transfer = bm1684_cdma_transfer;
		break;
	default:
		return -1;
	}
	return ret;
}

void bmdrv_memcpy_deinit(struct bm_device_info *bmdi)
{
    UNREFERENCED_PARAMETER(bmdi);
}

//int bmdev_mmap(struct file *file, struct vm_area_struct *vma)
//{
//	struct bm_device_info *bmdi = file->private_data;
//
//#ifndef SOC_MODE
//	u64 vpu_vmem_addr = 0, vpu_vmem_size = 0;
//
//	vpu_vmem_addr = bmdi->vpudrvctx.s_video_memory.base;
//	vpu_vmem_size = bmdi->vpudrvctx.s_video_memory.size;
//#endif
//
//	if (vma->vm_pgoff != 0) {
//#ifdef SOC_MODE
//		return bmdrv_gmem_map(bmdi, vma->vm_pgoff<<PAGE_SHIFT, vma->vm_end-vma->vm_start, vma);
//#else
//		if (((vma->vm_pgoff >= (vpu_vmem_addr >> PAGE_SHIFT)) &&
//		     (vma->vm_pgoff <= ((vpu_vmem_addr + vpu_vmem_size)) >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[0].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[1].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[2].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[3].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[4].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[0].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[1].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[2].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[3].phys_addr >> PAGE_SHIFT))
//		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[4].phys_addr >> PAGE_SHIFT))) {
//
//			return bm_vpu_mmap(file, vma);
//
//		} else if (!bm_jpu_addr_judge(vma->vm_pgoff, bmdi)) {
//			return bm_jpu_mmap(file, vma);
//		} else {
//			dev_err(bmdi->dev, "pcie mode unsupport mmap, mmap failed!\n");
//			return -1;
//		}
//#endif
//	} else {
//		dev_err(bmdi->dev, "vm_pgoff is 0, mmap failed!\n");
//		return -1;
//	}
//}

void bmdev_construct_cdma_arg(pbm_cdma_arg parg,
		u64 src,
		u64 dst,
		u64 size,
		MEMCPY_DIR dir,
		u32 intr,
		bool use_iommu)
{
	parg->src = src;
	parg->dst = dst;
	parg->size = size;
	parg->dir = dir;
	parg->intr = intr;
	parg->use_iommu = use_iommu;
}

int bmdev_memcpy_s2d(struct bm_device_info *bmdi, WDFFILEOBJECT file, u64 dst, void  *src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode)
{
	u32 pass_idx = 0;
	u32 cur_addr_inc = 0;
	unsigned long size_step;
	u32 realmem_size = CONFIG_HOST_REALMEM_SIZE;
	void  *src_cpy;
	bm_cdma_arg cdma_arg;
	int ret = 0;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	if (cdma_iommu_mode == KERNEL_USER_SETUP_IOMMU) {
		ret = -1;// do not support iommu in windows now
	} else {
		for (pass_idx = 0, cur_addr_inc = 0; pass_idx < (size + realmem_size - 1) / realmem_size; pass_idx++) {
			if ((pass_idx + 1) * realmem_size < size)
				size_step = realmem_size;
			else
				size_step = size - pass_idx * realmem_size;
			src_cpy = (u8  *)src + cur_addr_inc;

            ExAcquireFastMutex(&memcpy_info->s2dstag_Mutex);
            RtlCopyMemory(memcpy_info->s2dstagging_mem_vaddr, src_cpy, size_step);
            bmdev_construct_cdma_arg(
                &cdma_arg,
                memcpy_info->s2dstagging_mem_paddr.QuadPart & 0xffffffffff,
				dst + cur_addr_inc, size_step, HOST2CHIP, intr, FALSE);

            if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, file)) {
                ExReleaseFastMutex(&memcpy_info->s2dstag_Mutex);
				return -1;
			}
			cur_addr_inc += size_step;
            ExReleaseFastMutex(&memcpy_info->s2dstag_Mutex);
		}
	}
	return ret;
}

//for now, ignore mmu mode in memcpy_s2d_internal
int bmdev_memcpy_s2d_internal(struct bm_device_info *bmdi, u64 dst, const void *src, u32 size)
{
	u32 pass_idx = 0;
	u32 cur_addr_inc = 0;
	unsigned long size_step;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
    u32                    realmem_size = memcpy_info->stagging_mem_size;
	void *src_cpy;
	bm_cdma_arg cdma_arg;

	for (pass_idx = 0, cur_addr_inc = 0; pass_idx < (size + realmem_size - 1) / realmem_size; pass_idx++) {
		if ((pass_idx + 1) * realmem_size < size)
			size_step = realmem_size;
		else
			size_step = size - pass_idx * realmem_size;
		src_cpy = (u8 *)src + cur_addr_inc;
        ExAcquireFastMutex(&memcpy_info->s2dstag_Mutex);
        memcpy(memcpy_info->s2dstagging_mem_vaddr, src_cpy, size_step);
        bmdev_construct_cdma_arg(&cdma_arg, memcpy_info->s2dstagging_mem_paddr.QuadPart & 0xffffffffff,
				dst + cur_addr_inc, size_step, HOST2CHIP, FALSE, FALSE);
        if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, NULL)) {
            ExReleaseFastMutex(&memcpy_info->s2dstag_Mutex);
			return -1;
		}
 		ExReleaseFastMutex(&memcpy_info->s2dstag_Mutex);
		cur_addr_inc += size_step;
	}
	return 0;
}

int bmdev_memcpy_d2s(struct bm_device_info *bmdi, WDFFILEOBJECT file, void  *dst, u64 src, u32 size,
		u32 intr, bm_cdma_iommu_mode cdma_iommu_mode)
{
	u32 pass_idx = 0;
	u32 cur_addr_inc = 0;
	unsigned long size_step;
	u32 realmem_size = CONFIG_HOST_REALMEM_SIZE;
	void  *dst_cpy;
	bm_cdma_arg cdma_arg;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	int ret = 0;

	if (cdma_iommu_mode == KERNEL_USER_SETUP_IOMMU) {
        ret = -1;  // do not support iommu in windows now
	} else {
		for (pass_idx = 0, cur_addr_inc = 0; pass_idx < (size + realmem_size - 1) / realmem_size; pass_idx++) {
			if ((pass_idx + 1) * realmem_size < size)
				size_step = realmem_size;
			else
				size_step = size - pass_idx * realmem_size;
			dst_cpy = (u8  *)dst + cur_addr_inc;
            ExAcquireFastMutex(&memcpy_info->d2sstag_Mutex);

			bmdev_construct_cdma_arg(&cdma_arg, src + cur_addr_inc,
                memcpy_info->d2sstagging_mem_paddr.QuadPart & 0xffffffffff,
				size_step, CHIP2HOST, intr, FALSE);

            if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, file)) {
                ExReleaseFastMutex(&memcpy_info->d2sstag_Mutex);
				return -1;
			}
            RtlCopyMemory(dst_cpy, memcpy_info->d2sstagging_mem_vaddr, size_step);
			cur_addr_inc += size_step;
            ExReleaseFastMutex(&memcpy_info->d2sstag_Mutex);
		}
	}
	return ret;
}

int bmdev_memcpy_c2c(struct bm_device_info *bmdi, WDFFILEOBJECT file, u64 src, u64 dst, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode)
{
    UNREFERENCED_PARAMETER(cdma_iommu_mode);
	int ret = 0;
	bm_cdma_arg cdma_arg;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	bmdev_construct_cdma_arg(&cdma_arg, src,
			dst, size, CHIP2CHIP, intr, FALSE);
    if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, file))
		return -1;
	return ret;
}

int bmdev_memcpy(struct bm_device_info *bmdi, _In_ WDFREQUEST Request)
{
	int ret = 0;
    struct bm_memcpy_param* memcpy_param = NULL;
    struct bm_handle_info * h_info       = NULL;
    size_t                  bufSize;
    KIRQL                   irql = KeGetCurrentIrql();
    WDFFILEOBJECT           file = WdfRequestGetFileObject(Request);

    if (file) {
        if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "bm-sophon%d bmdev_memcpy: file not found!\n", bmdi->dev_index);
            WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
            return -1;
        }
    }
	NTSTATUS Status = WdfRequestRetrieveInputBuffer(Request, sizeof(struct bm_memcpy_param), &memcpy_param, &bufSize);
    if (!NT_SUCCESS(Status)) {
        WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
        return -1;
    }

	TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "bm-sophon%d memcpy_param: current irq level=%d\n",
                bmdi->dev_index,
                irql);

	TraceEvents(
        TRACE_LEVEL_ERROR,
        TRACE_DEVICE,
        "bm-sophon%d memcpy_param: host_addr= 0x%p, device_addr=0x%lld, size=0x%x, dir = %d, intr = %d, iommu_mode=%d\n",
        bmdi->dev_index,
        memcpy_param->host_addr,
        memcpy_param->device_addr,
        memcpy_param->size,
        memcpy_param->dir,
        memcpy_param->intr,
        memcpy_param->cdma_iommu_mode);

	if (memcpy_param->dir == HOST2CHIP) {
        ret = bmdev_memcpy_s2d(bmdi,
                               file,
                               memcpy_param->device_addr,
                               memcpy_param->host_addr,
                               memcpy_param->size,
                               memcpy_param->intr,
                               memcpy_param->cdma_iommu_mode);
    } else if (memcpy_param->dir == CHIP2HOST) {
        ret = bmdev_memcpy_d2s(bmdi,
                               file,
                               memcpy_param->host_addr,
                               memcpy_param->device_addr,
                               memcpy_param->size,
                               memcpy_param->intr,
                               memcpy_param->cdma_iommu_mode);
	} else if (memcpy_param->dir == CHIP2CHIP) {
        ret = bmdev_memcpy_c2c(bmdi,
                               file,
                               memcpy_param->src_device_addr,
                               memcpy_param->device_addr,
                               memcpy_param->size,
                               memcpy_param->intr,
                               memcpy_param->cdma_iommu_mode);
    }
	else {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bm-sophon%d memcpy_param dir err\n",
                    bmdi->dev_index);
		ret = -1;
	}

     if (ret) {
        WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
    } else {
        WdfRequestComplete(Request, STATUS_SUCCESS);
    }

	return ret;
}