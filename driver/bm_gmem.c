#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm_gmem.h"
#include "bm_genalloc.h"
#include "bm1682_gmem.h"
#include "bm1684_gmem.h"
#include "bm_debug.h"

int heap_id;

extern void bm_flush_dcache_area(void *addr, size_t size);
extern void bm_inval_dcache_area(void *addr, size_t size);

int bmdrv_get_gmem_mode(struct bm_device_info *bmdi)
{
#ifdef SOC_MODE
	return GMEM_NORMAL;
#else
	return (bmdi->boot_info.ddr_mode >> 16) & 0x3;
#endif
}

int bmdrv_gmem_init(struct bm_device_info *bmdi)
{
	int i;
	struct ion_platform_heap npu_heap;
	struct ion_heap *pion_heap;
	struct bm_gmem_info *gmem_info = &bmdi->gmem_info;

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		if (bmdrv_bm1682_parse_reserved_mem_info(bmdi))
			return -EINVAL;
		break;
	case 0x1684:
	case 0x1686:
		if (bmdrv_bm1684_parse_reserved_mem_info(bmdi))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	heap_id = 0;

	mutex_init(&gmem_info->gmem_mutex);

	npu_heap.type = ION_HEAP_TYPE_CARVEOUT;
	npu_heap.name = "npu-heap";
	npu_heap.align = 4096;
	npu_heap.priv = bmdi;

	ion_device_create(bmdi);

	for (i = 0; i < MAX_HEAP_CNT; i++) {
		npu_heap.base = gmem_info->resmem_info.npureserved_addr[i];
		npu_heap.size = gmem_info->resmem_info.npureserved_size[i];
		if (npu_heap.size > 0) {
			pion_heap = ion_carveout_heap_create(&npu_heap);
			ion_device_add_heap(bmdi, pion_heap);
			pr_info("bm-sophon%d gmem[%d]: mem base 0x%llx\n", bmdi->dev_index, i, (u64)npu_heap.base);
			pr_info("bm-sophon%d gmem[%d]: total mem 0x%llx\n", bmdi->dev_index, i, (u64)npu_heap.size);
		}
	}
	return 0;
}

void bmdrv_gmem_deinit(struct bm_device_info *bmdi)
{
	struct ion_carveout_heap *carveout_heap = NULL;
	struct ion_heap *heap = NULL;
	struct ion_heap *tmp = NULL;

	list_for_each_entry_safe(heap, tmp, &bmdi->gmem_info.idev.heaps, node) {
		carveout_heap =	container_of(heap, struct ion_carveout_heap, heap);
		list_del(&heap->node);
		ion_carveout_heap_destroy(carveout_heap);
	}
}

u64 bmdrv_gmem_total_size(struct bm_device_info *bmdi)
{
	struct ion_device *dev = &bmdi->gmem_info.idev;
	struct ion_heap *heap;
	struct ion_carveout_heap *carveout_heap = NULL;
	u64 size = 0;

	down_read(&dev->lock);
	list_for_each_entry(heap, &dev->heaps, node) {
		if (heap->type != ION_HEAP_TYPE_CARVEOUT)
			continue;
		carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
		size += bm_gen_pool_size(carveout_heap->pool);
	}
	up_read(&dev->lock);
	return size;
}

u64 bmdrv_gmem_avail_size(struct bm_device_info *bmdi)
{
	struct ion_device *dev = &bmdi->gmem_info.idev;
	struct ion_heap *heap;
	struct ion_carveout_heap *carveout_heap = NULL;
	u64 avail_size = 0;

	down_read(&dev->lock);
	list_for_each_entry(heap, &dev->heaps, node) {
		if (heap->type != ION_HEAP_TYPE_CARVEOUT)
			continue;
		carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
		avail_size += bm_gen_pool_avail(carveout_heap->pool);
	}
	up_read(&dev->lock);

	return avail_size;
}

void bmdrv_heap_avail_size(struct bm_device_info *bmdi)
{
	struct ion_device *dev = &bmdi->gmem_info.idev;
	struct ion_heap *heap;
	struct ion_carveout_heap *carveout_heap = NULL;
	u64 avail_size = 0;
	int i = 0;

	down_read(&dev->lock);
	list_for_each_entry(heap, &dev->heaps, node) {
		if (heap->type != ION_HEAP_TYPE_CARVEOUT)
			continue;
		carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
		avail_size = bm_gen_pool_avail(carveout_heap->pool);
		printk("heap%d avail memory 0x%llx bytes\n", heap->id, avail_size);
		i++;
	}
	up_read(&dev->lock);
}

void bmdrv_heap_mem_used(struct bm_device_info *bmdi, struct bm_dev_stat *stat)
{
	struct ion_device *dev = &bmdi->gmem_info.idev;
	struct ion_heap *heap;
	struct ion_carveout_heap *carveout_heap = NULL;

	stat->heap_num = dev->heap_cnt;
	down_read(&dev->lock);
	list_for_each_entry(heap, &dev->heaps, node) {
		if (heap->type != ION_HEAP_TYPE_CARVEOUT)
			continue;
		carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
		stat->heap_stat[heap->id].mem_avail = bm_gen_pool_avail(carveout_heap->pool) >> 20;
		stat->heap_stat[heap->id].mem_total = bm_gen_pool_size(carveout_heap->pool) >> 20;
		stat->heap_stat[heap->id].mem_used = stat->heap_stat[heap->id].mem_total - stat->heap_stat[heap->id].mem_avail;
	}
	up_read(&dev->lock);
}

#ifdef SOC_MODE
int bmdrv_gmem_map(struct bm_device_info *bmdi, u64 addr, u64 size, struct vm_area_struct *vma)
{
	bmdrv_gmem_invalidate(bmdi, addr, size);
	if (remap_pfn_range(vma, vma->vm_start, addr >> PAGE_SHIFT, size, vma->vm_page_prot)) {
		pr_err("bmdrv_gmem_map error\n");
		return -EAGAIN;
	}

	return 0;
}

int bmdrv_gmem_map_no_cache(struct bm_device_info *bmdi, u64 addr, u64 size, struct vm_area_struct *vma)
{
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, addr >> PAGE_SHIFT, size, vma->vm_page_prot)) {
		pr_err("bmdrv_gmem_map error\n");
		return -EAGAIN;
	}

	return 0;
}

int bmdrv_gmem_flush(struct bm_device_info *bmdi, u64 addr, u64 size)
{
	void *va;

	va = phys_to_virt(addr);
	bm_flush_dcache_area(va, size);

	return 0;
}


int bmdrv_gmem_invalidate(struct bm_device_info *bmdi, u64 addr, u64 size)
{
	void *va;

	va = phys_to_virt(addr);
	bm_inval_dcache_area(va, size);

	return 0;
}

int bmdrv_gmem_vir_to_phy(struct bm_device_info *bmdi, struct bm_gmem_addr *addr)
{
	unsigned long pa_pfn = 0x0;
        struct mm_struct *mm = current->mm;
        struct vm_area_struct *vma;
	unsigned int offset;
	int ret = 0x0;

	offset = addr->vir_addr & ~PAGE_MASK;
	down_read(&mm->mmap_sem);

	vma = find_vma(mm, addr->vir_addr);

	if (!vma) {
		pr_info("find vma fail, vir addr = 0x%lx\n", addr->vir_addr);
		up_read(&current->mm->mmap_sem);
		return -1;
	}

	ret = follow_pfn(vma, addr->vir_addr, &pa_pfn);

	if (ret) {
		pr_info("fllow_pfn fail, vir addr = 0x%lx, ret = %d\n", addr->vir_addr, ret);
		up_read(&current->mm->mmap_sem);
		return -1;
	}

	up_read(&current->mm->mmap_sem);

	addr->phy_addr = (pa_pfn << PAGE_SHIFT) + offset;

	//pr_info("bmdrv_gmem_vir_to_phy vir_addr = 0x%llx, pa_addr = 0x%llx\n", addr->vir_addr, addr->phy_addr);
	return 0;
}
#endif

int bmdev_gmem_get_handle_info(struct bm_device_info *bmdi, struct file *file,
		struct bm_handle_info **f_list)
{
	struct bm_handle_info *h_info;

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_for_each_entry(h_info, &bmdi->handle_list, list) {
		if (h_info->file == file) {
			*f_list = h_info;
			mutex_unlock(&bmdi->gmem_info.gmem_mutex);
			return 0;
		}
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	return -EINVAL;
}

static int bmdrv_gmem_alloc(struct bm_device_info *bmdi,
		struct bm_handle_info *h_info,
		struct ion_allocation_data *alloc_data)
{
	int ret  = 0;
	int i = 0;
	int heap_id_mask = alloc_data->heap_id_mask;

	if ((alloc_data->len == 0) ||
			(alloc_data->len > bmdrv_gmem_avail_size(bmdi)) ||
			(heap_id_mask == 0x0)) {
		pr_info("bm-sophon%d gmem alloc failed, request size is 0x%llx, heap_id_mask = 0x%x, avail memory size is 0x%llx\n",
						bmdi->dev_index, alloc_data->len, heap_id_mask, bmdrv_gmem_avail_size(bmdi));
		bmdrv_heap_avail_size(bmdi);
		return -1;
	}
	for (i = 0; i < MAX_HEAP_CNT; i++) {
		if (((0x1 << i) & heap_id_mask) == 0x0)
			continue;
		alloc_data->heap_id_mask = 0x1 << i;
		ret = ion_alloc(bmdi, alloc_data);
		alloc_data->heap_id_mask = heap_id_mask;
		if (ret >= 0) {
			PR_TRACE("%s 0x%llx, size 0x%llx\n", __func__,
				alloc_data->paddr,
				alloc_data->len);
			return 0;
		}
	}
	alloc_data->paddr = BM_MEM_ADDR_NULL;
	pr_info("bm-sophon%d gmem alloc failed, request size is 0x%llx, heap_id_mask is 0x%x, avail memory size is 0x%llx error_code=%d(%s)\n",
					bmdi->dev_index, alloc_data->len, heap_id_mask, bmdrv_gmem_avail_size(bmdi),ret,bmdrv_get_error_string(ret));
	bmdrv_heap_avail_size(bmdi);
	return -1;
}

int bmdrv_gmem_ioctl_alloc_mem(struct bm_device_info *bmdi, struct file *file,
	unsigned long arg)
{
	int ret = 0;
	struct ion_allocation_data alloc_data;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: bm-sophon%d file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}
	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	if (copy_from_user(&alloc_data, (struct ion_allocation_data __user *)arg, sizeof(alloc_data))) {
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -EFAULT;
	}
	if (!bmdrv_gmem_alloc(bmdi, h_info, &alloc_data)) {
		ret = copy_to_user((void __user *)arg, &alloc_data, sizeof(alloc_data));
		if (!ret)
			h_info->gmem_used += BGM_4K_ALIGN(alloc_data.len);
		else
			pr_err("bm-sophon%d %s: copy_to_user failed\n", bmdi->dev_index, __func__);
	} else {
		ret = -ENOMEM;
		pr_err("bm-sophon%d bmdrv_gmem_ioctl_alloc_mem alloc failed!\n", bmdi->dev_index);
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	return ret;
}

int bmdrv_gmem_ioctl_alloc_mem_ion(struct bm_device_info *bmdi, struct file *file,
	unsigned long arg)
{
	int ret = 0;
	bm_device_mem_t device_mem;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}
	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	if (copy_from_user(&device_mem, (bm_device_mem_t __user *)arg, sizeof(device_mem))) {
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -EFAULT;
	}
	h_info->gmem_used += BGM_4K_ALIGN(device_mem.size);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	PR_TRACE("bmdrv: gmem ion alloc %x\n", device_mem.size);

	return ret;
}

int bmdrv_gmem_ioctl_free_mem(struct bm_device_info *bmdi, struct file *file,
	unsigned long arg)
{
	int ret = 0;
	bm_device_mem_t device_mem;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	if (copy_from_user(&device_mem, (bm_device_mem_t __user *)arg, sizeof(device_mem))) {
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -EFAULT;
	}

	h_info->gmem_used -= BGM_4K_ALIGN(device_mem.size);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	PR_TRACE("%s 0x%lx, size 0x%x\n", __func__, device_mem.u.device.device_addr, device_mem.size);
	return ret;
}

int bmdrv_gmem_ioctl_get_heap_num(struct bm_device_info *bmdi, unsigned long arg)
{
	struct ion_device *dev = &bmdi->gmem_info.idev;
	unsigned int total_num = dev->heap_cnt;

	PR_TRACE("bm-sophon%d, total num = %d", total_num, bmdi->dev_index);
	return put_user(total_num, (int __user*)arg);
}

int bmdrv_gmem_ioctl_get_heap_stat_byte_by_id(struct bm_device_info *bmdi, struct file *file,
	unsigned long arg)
{
	struct bm_heap_stat_byte heap_info;
	struct ion_device *dev = &bmdi->gmem_info.idev;
	struct ion_heap *heap;
	struct ion_carveout_heap *carveout_heap = NULL;

	if (copy_from_user(&heap_info, (struct bm_heap_stat_byte __user *)arg, sizeof(struct bm_heap_stat_byte)))
		return -EFAULT;

	if (heap_info.heap_id < 0 || heap_info.heap_id > dev->heap_cnt)
		return -EFAULT;

	down_read(&dev->lock);
	list_for_each_entry(heap, &dev->heaps, node) {
		if (heap->type != ION_HEAP_TYPE_CARVEOUT)
			continue;
		if (heap->id != heap_info.heap_id)
			continue;
		carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
		heap_info.mem_avail = bm_gen_pool_avail(carveout_heap->pool);
		heap_info.mem_total = bm_gen_pool_size(carveout_heap->pool);
		heap_info.mem_used = heap_info.mem_total - heap_info.mem_avail;
		heap_info.mem_start_addr = carveout_heap->base;
		break;
	}
	PR_TRACE("bm-sophon%d, heap id = %d, mem_total = 0x%llx, mem_avail = 0x%llx, mem_used = 0x%llx, mem_start_addr = 0x%llx\n", bmdi->dev_index, heap_info.heap_id, heap_info.mem_total, heap_info.mem_avail, heap_info.mem_used, heap_info.mem_start_addr);

	up_read(&dev->lock);
	if (copy_to_user((struct bm_heap_stat_byte __user *)arg, &heap_info,
		sizeof(struct bm_heap_stat_byte)))
	return -EFAULT;

	return 0;
}
