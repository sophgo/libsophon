#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "bm_common.h"
#include "bm_uapi.h"
#include "bm_ctl.h"
#include "bm_drv.h"
#include "bm_clkrst.h"


extern dev_t bm_devno_base;
extern dev_t bm_ctl_devno_base;

static int bmdev_open(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi;
	pid_t open_pid;
	struct bm_handle_info *h_info;
	int i;

	PR_TRACE("bmdev_open\n");

	bmdi = container_of(inode->i_cdev, struct bm_device_info, cdev);

	if (bmdi == NULL)
		return -ENODEV;

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	bmdi->dev_refcount++;
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	open_pid = current->pid;

	h_info = kmalloc(sizeof(struct bm_handle_info), GFP_KERNEL);
	if (!h_info)
	{
		mutex_lock(&bmdi->gmem_info.gmem_mutex);
		bmdi->dev_refcount--;
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -ENOMEM;
	}

	// hash_init(h_info->api_htable);
	h_info->file = file;
	h_info->open_pid = open_pid;
	h_info->gmem_used = 0ULL;
	for (i = 0; i < 1; i++)
	{
		h_info->h_send_api_seq[i] = 0ULL;
		h_info->h_cpl_api_seq[i] = 0ULL;
	}
	mutex_init(&h_info->h_api_seq_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_add(&h_info->list, &bmdi->handle_list);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	file->private_data = bmdi;



#ifdef USE_RUNTIME_PM
	pm_runtime_get_sync(bmdi->cinfo.device);
#endif
	return 0;
}

static ssize_t bmdev_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t bmdev_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	return 0;
}

static int bmdev_fasync(int fd, struct file *filp, int mode)
{
	return 0;
}

static int bmdev_close(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi = file->private_data;
	struct bm_handle_info *h_info, *h_node;
	int handle_num = 0;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info))
	{
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_for_each_entry(h_node, &bmdi->handle_list, list)
	{
		if (h_node->open_pid == h_info->open_pid)
		{
			handle_num++;
		}
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_del(&h_info->list);
	kfree(h_info);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	file->private_data = NULL;

#ifdef USE_RUNTIME_PM
	pm_runtime_put_sync(bmdi->cinfo.device);
#endif
	PR_TRACE("bmdev_close\n");
	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	bmdi->dev_refcount--;
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	return 0;
}

static long bm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;
	int ret = 0;

	if (bmdi->status_over_temp)
	{
		pr_err("bmsophon %d the temperature is too high, bypass send ioctl cmd to chip\n", bmdi->dev_index);
		if (cmd != BMDEV_GET_STATUS)
		{
			return -1;
		}
	}

	switch (cmd)
	{
	case BMDEV_ALLOC_GMEM:
		ret = bmdrv_gmem_ioctl_alloc_mem(bmdi, file, arg);
		break;

	case BMDEV_ALLOC_GMEM_ION:
		ret = bmdrv_gmem_ioctl_alloc_mem_ion(bmdi, file, arg);
		break;

	case BMDEV_FREE_GMEM:
		ret = bmdrv_gmem_ioctl_free_mem(bmdi, file, arg);
		break;

	case BMDEV_TOTAL_GMEM:
		ret = put_user(bmdrv_gmem_total_size(bmdi), (u64 __user *)arg);
		break;

	case BMDEV_AVAIL_GMEM:
		ret = put_user(bmdrv_gmem_avail_size(bmdi), (u64 __user *)arg);
		break;

	case BMDEV_SET_IOMAP_TPYE:
		bmdi->MMAP_TPYE= (int)arg;
		break;

	case BMDEV_FORCE_RESET_TPU:
		break;

	case BMDEV_REQUEST_ARM_RESERVED:
		ret = put_user(bmdi->gmem_info.resmem_info.armreserved_addr, (unsigned long __user *)arg);
		break;

	case BMDEV_GET_STATUS:
		ret = put_user(bmdi->status, (int __user *)arg);
		break;

	case BMDEV_GET_DRIVER_VERSION:
	{
		ret = put_user(BM_DRIVER_VERSION, (int __user *)arg);
		break;
	}

	case BMDEV_GET_BOARD_TYPE:
	{
		char board_name[25];

		switch (bmdi->cinfo.chip_id)
		{
		case CHIP_ID:
			snprintf(board_name, 20, "%s", base_get_chip_id(bmdi));
			break;
		default:
			snprintf(board_name, 20, "unknown");
		}
		ret = copy_to_user((char __user *)arg, board_name, sizeof(board_name));

		break;
	}

	case BMDEV_GET_BOARDT:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_board_temp != NULL)
			ret = put_user(c_attr->board_temp, (u32 __user *)arg);
		else
			return -EFAULT;
		break;
	}

	case BMDEV_GET_CHIPT:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_chip_temp != NULL)
			ret = put_user(c_attr->chip_temp, (u32 __user *)arg);
		else
			return -EFAULT;
		break;
	}

	case BMDEV_GET_TPU_P:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_tpu_power != NULL)
		{
			long power = c_attr->vdd_tpu_volt * c_attr->vdd_tpu_curr;
			ret = put_user(power, (long __user *)arg);
		}
		else
			return -EFAULT;
		break;
	}

	case BMDEV_GET_TPU_V:
	{
		ret = put_user(ATTR_NOTSUPPORTED_VALUE, (u32 __user *)arg);
		break;
	}

	case BMDEV_GET_CARD_ID:
	{
		struct bm_card *bmcd = bmdi->bmcd;
		if (put_user(bmcd->card_index, (u32 __user *)arg))
		{
			return -EFAULT;
		}
		break;
	}

	case BMDEV_GET_DYNFREQ_STATUS:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		mutex_lock(&c_attr->attr_mutex);
		ret = copy_to_user((int __user *)arg, &bmdi->enable_dyn_freq, sizeof(int));
		mutex_unlock(&c_attr->attr_mutex);
		break;
	}

	case BMDEV_CHANGE_DYNFREQ_STATUS:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		mutex_lock(&c_attr->attr_mutex);
		if (copy_from_user(&bmdi->enable_dyn_freq, (unsigned int __user *)arg, sizeof(int)))
			;
		mutex_unlock(&c_attr->attr_mutex);
		break;
	}


	case BMDEV_GET_DEV_STAT:
	{
		bm_dev_stat_t stat;
		struct bm_chip_attr *c_attr;

		c_attr = &bmdi->c_attr;
		stat.mem_total = div_u64(div_u64(bmdrv_gmem_total_size(bmdi), 1024), 1024);
		stat.mem_used = stat.mem_total - div_u64(div_u64(bmdrv_gmem_avail_size(bmdi), 1024), 1024);
		stat.tpu_util = c_attr->bm_get_npu_util(bmdi);
		bmdrv_heap_mem_used(bmdi, &stat);
		ret = copy_to_user((unsigned long __user *)arg, &stat, sizeof(bm_dev_stat_t));
		break;
	}

	case BMDEV_GET_MISC_INFO:
		ret = copy_to_user((unsigned long __user *)arg, &bmdi->misc_info,
											 sizeof(struct bm_misc_info));
		break;

	case BMDEV_GET_TPUC:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;

		if (c_attr->bm_get_tpu_power != NULL)
		{
			ret = copy_to_user((u32 __user *)arg, &c_attr->vdd_tpu_curr, sizeof(u32));
		}
		else
		{
			return -EFAULT;
		}
		break;
	}

	case BMDEV_GET_BOARDP:
	{
		struct bm_device_info *c_bmdi;
		struct bm_chip_attr *c_attr;

		if (bmdi->bmcd->card_bmdi[0] != NULL)
		{
			c_bmdi = bmdi->bmcd->card_bmdi[0];
			c_attr = &c_bmdi->c_attr;
		}
		else
		{
			return -EFAULT;
		}

		if (c_attr->bm_get_board_power != NULL)
		{
			ret = copy_to_user((u32 __user *)arg, &c_attr->board_power, sizeof(u32));
		}
		else
		{
			return -EFAULT;
		}
		break;
	}

	case BMDEV_SET_TPU_DIVIDER:
		break;

	case BMDEV_SET_MODULE_RESET:
		break;

	case BMDEV_GMEM_ADDR:
	{
		struct bm_gmem_addr addr;

		if (copy_from_user(&addr, (struct bm_gmem_addr __user *)arg,
											 sizeof(struct bm_gmem_addr)))
			return -EFAULT;
		if (bmdrv_gmem_vir_to_phy(bmdi, &addr))
			return -EFAULT;

		if (copy_to_user((struct bm_gmem_addr __user *)arg, &addr,
										 sizeof(struct bm_gmem_addr)))
			return -EFAULT;

		break;
	}
	case BMDEV_GET_HEAP_INFO:
	{
		ret = bmdrv_get_heap_info(bmdi, arg);
		break;
	}

	default:
		dev_err(bmdi->dev, "*************Invalid ioctl parameter************\n");
		return -EINVAL;
	}

	return ret;
}

static long bmdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;
	int ret = 0;

	PR_TRACE("[%s: %d] _IOC_TYPE(cmd)=0x%x, cmd=0x%x\n", __func__, __LINE__, _IOC_TYPE(cmd), cmd);

	if ((_IOC_TYPE(cmd)) == BMDEV_IOCTL_MAGIC || (_IOC_TYPE(cmd))==0x71) {
		ret = bm_ioctl(file, cmd, arg);
	} else {
		dev_dbg(bmdi->dev, "Unknown cmd 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static int bmdev_ctl_open(struct inode *inode, struct file *file)
{
	struct bm_ctrl_info *bmci;

	bmci = container_of(inode->i_cdev, struct bm_ctrl_info, cdev);
	file->private_data = bmci;
	return 0;
}

static int bmdev_ctl_close(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long bmdev_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_ctrl_info *bmci = file->private_data;
	int ret = 0;

	switch (cmd)
	{
	case BMCTL_GET_DEV_CNT:
		ret = put_user(bmci->dev_count, (int __user *)arg);
		break;

	case BMCTL_GET_SMI_ATTR:
		ret = bmctl_ioctl_get_attr(bmci, arg);
		break;

	case BMCTL_GET_PROC_GMEM:
		ret = bmctl_ioctl_get_proc_gmem(bmci, arg);
		break;

	case BMCTL_GET_DRIVER_VERSION:
		ret = put_user(BM_DRIVER_VERSION, (int __user *)arg);
		break;

	case BMCTL_GET_CARD_NUM:
	{
		int card_num = 0;
		card_num = 1;
		ret = put_user(card_num, (int __user *)arg);
		break;
	}

	default:
		pr_err("*************Invalid ioctl parameter************\n");
		return -EINVAL;
	}
	return ret;
}

static int bmdev_mmap(struct file *file, struct vm_area_struct *vma) {
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
		if (remap_pfn_range(vma, vma->vm_start, TPU_SYS_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else if (bmdi->MMAP_TPYE == MMAP_REG) {
		if (length > TPU_REG_SIZE) {
			PR_TRACE("bmdev_mmap: size > REG size\n");
			return -EINVAL;
		}
		if (remap_pfn_range(vma, vma->vm_start, TPU_REG_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else if (bmdi->MMAP_TPYE == MMAP_SMEM) {
		if (length > TPU_SMEM_SIZE) {
			PR_TRACE("bmdev_mmap: size > SMEM size\n");
			return -EINVAL;
		}
		if (remap_pfn_range(vma, vma->vm_start, TPU_SMEM_BASE >> PAGE_SHIFT, length, vma->vm_page_prot)) {
			PR_TRACE("bmdev_mmap: remap_pfn_range failed\n");
			return -EAGAIN;
		}
	} else if (bmdi->MMAP_TPYE == MMAP_LMEM) {
		if (length > TPU_LMEM_SIZE) {
			PR_TRACE("bmdev_mmap: size > LMEM size\n");
			return -EINVAL;
		}
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

static const struct file_operations bmdev_fops = {
		.open = bmdev_open,
		.read = bmdev_read,
		.write = bmdev_write,
		.fasync = bmdev_fasync,
		.release = bmdev_close,
		.unlocked_ioctl = bmdev_ioctl,
		.mmap = bmdev_mmap,
		.owner = THIS_MODULE,
};

static const struct file_operations bmdev_ctl_fops = {
		.open = bmdev_ctl_open,
		.release = bmdev_ctl_close,
		.unlocked_ioctl = bmdev_ctl_ioctl,
		.owner = THIS_MODULE,
};

int bmdev_register_device(struct bm_device_info *bmdi)
{
	bmdi->devno = MKDEV(MAJOR(bm_devno_base), MINOR(bm_devno_base) + bmdi->dev_index);
	bmdi->dev = device_create(bmdrv_class_get(), bmdi->parent, bmdi->devno, NULL,
														"%s%d", BM_CDEV_NAME, bmdi->dev_index);
	if (IS_ERR(bmdi->dev)) {
		PR_TRACE("failed bmdi->dev create************************");
	}

	cdev_init(&bmdi->cdev, &bmdev_fops);

	bmdi->cdev.owner = THIS_MODULE;
	cdev_add(&bmdi->cdev, bmdi->devno, 1);

	dev_set_drvdata(bmdi->dev, bmdi);

	dev_dbg(bmdi->dev, "%s\n", __func__);
	return 0;
}

int bmdev_unregister_device(struct bm_device_info *bmdi)
{
	dev_dbg(bmdi->dev, "%s\n", __func__);
	cdev_del(&bmdi->cdev);
	device_destroy(bmdrv_class_get(), bmdi->devno);
	return 0;
}

int bmdev_ctl_register_device(struct bm_ctrl_info *bmci)
{
	bmci->devno = MKDEV(MAJOR(bm_ctl_devno_base), MINOR(bm_ctl_devno_base));
	bmci->dev = device_create(bmdrv_class_get(), NULL, bmci->devno, NULL,
														"%s", BMDEV_CTL_NAME);
	if (IS_ERR(bmci->dev)) {
		PR_TRACE("failed bmci->dev create************************");
	}
	cdev_init(&bmci->cdev, &bmdev_ctl_fops);
	bmci->cdev.owner = THIS_MODULE;
	cdev_add(&bmci->cdev, bmci->devno, 1);

	dev_set_drvdata(bmci->dev, bmci);

	return 0;
}

int bmdev_ctl_unregister_device(struct bm_ctrl_info *bmci)
{
	cdev_del(&bmci->cdev);
	device_destroy(bmdrv_class_get(), bmci->devno);
	return 0;
}
