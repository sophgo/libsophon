#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include "bm_common.h"
#include "bm_uapi.h"
#include "bm_thread.h"
// #include "bm_fw.h"
#include "bm_ctl.h"
#include "bm_drv.h"
#include <asm/div64.h>
#include "bm_card.h"
#include "SGTPUV8_card.h"
// #include "SGTPUV8_base64.h"
#include "SGTPUV8_clkrst.h"
#include "bm_timer.h"


extern dev_t bm_devno_base;
extern dev_t bm_ctl_devno_base;
extern int bmdrv_reset_bmcpu(struct bm_device_info *bmdi);

static int bmdev_open(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi;
	pid_t open_pid;
	struct bm_handle_info *h_info;
	struct bm_thread_info *thd_info = NULL;
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

	hash_init(h_info->api_htable);
	h_info->file = file;
	h_info->open_pid = open_pid;
	h_info->gmem_used = 0ULL;
	for (i = 0; i < BM_MAX_CORE_NUM; i++)
	{
		h_info->h_send_api_seq[i] = 0ULL;
		h_info->h_cpl_api_seq[i] = 0ULL;
	}
	init_waitqueue_head(&h_info->h_msg_done);
	mutex_init(&h_info->h_api_seq_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	thd_info = bmdrv_create_thread_info(h_info, open_pid);
	if (!thd_info)
	{
		kfree(h_info);
		bmdi->dev_refcount--;
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -ENOMEM;
	}
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

	pwr_ctrl_get(filp->private_data, NULL);
	return 0;

}

static ssize_t bmdev_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{

	return -1;

}

static int bmdev_fasync(int fd, struct file *filp, int mode)
{

	return -1;

}

static int bmdev_close(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi = file->private_data;
	struct bm_handle_info *h_info, *h_node;
	int handle_num = 0;
	// int core = 0;
	// int core_num = bmdi->cinfo.tpu_core_num;

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

	// if (handle_num == 1)
	// 	bmdrv_api_clear_lib(bmdi, file);


	/* invalidate pending APIs in msgfifo */
	// for (core = 0; core < core_num; core++)
	//	bmdev_invalidate_pending_apis(bmdi, h_info, core);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	bmdrv_delete_thread_info(h_info);
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
	void __iomem *reg_addr = NULL;

	if (bmdi->status_over_temp || bmdi->status_pcie)
	{
		pr_err("bmsophon %d the temperature is too high, bypass send ioctl cmd to chip\n", bmdi->dev_index);
		if (cmd != BMDEV_GET_STATUS)
		{
			return -1;
		}
	}

	switch (cmd)
	{
	
	case BMDEV_STORE_BMCPU_APP_THREAD:
		if(bmdi->bmcpu_thread_id!=0){
			pr_err("bm-sophon%d: bmcpu thread has been created\n", bmdi->dev_index);
			return -EBUSY;
		}else{
			bmdi->bmcpu_thread_id = (long)arg;
			PR_TRACE("bm-sophon%d: bmcpu thread id is %ld\n", bmdi->dev_index, bmdi->bmcpu_thread_id);
		}
		break;
	case BMDEV_GET_BMCPU_APP_THREAD:
		if(bmdi->bmcpu_thread_id==0){
			pr_err("bm-sophon%d: bmcpu thread has not been created\n", bmdi->dev_index);
			return -EINVAL;
		}
		ret = copy_to_user((void __user *)arg, &bmdi->bmcpu_thread_id, sizeof(long));
		break;
	
	case BMDEV_KILL_BMCPU_APP_THREAD:
		ret=kill_user_thread(bmdi->bmcpu_thread_id);
		break;
	
	case BMDEV_GET_PHYS_ADDR:
		ret = bmdev_get_phys_addr(bmdi, file, arg);
		break;

	case BMDEV_READL:
		printk("readl reg addr=%lx\n", (u_long)arg);
		reg_addr = ioremap(arg, 4);
		printk("reg addr=%lx, value=%x\n", (u_long)arg, readl(reg_addr));
		iounmap(reg_addr);
		break;

	case BMDEV_MEMCPY:
		// ret = bmdev_memcpy(bmdi, file, arg);
		break;

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
		ret = SGTPUV8_reset_tpu(bmdi);
		if (!ret)
			bmdi->status_sync_api = 0;
		break;

	case BMDEV_SEND_API:
		if (bmdi->status_sync_api == 0) {
			ret = bmdrv_send_api(bmdi, file, arg, false, true);
		} else {
			pr_err("bm-sophon%d: TPU SYS hang\n", bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_SEND_API_EXT:
		if (bmdi->status_sync_api == 0)
			ret = bmdrv_send_api(bmdi, file, arg, true, true);
		else
		{
			pr_err("bm-sophon%d: TPU SYS hang\n", bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_QUERY_API_RESULT:
		if (bmdi->status_sync_api == 0)
			ret = bmdrv_query_api(bmdi, file, arg);
		else
		{
			pr_err("bm-sophon%d: TPU SYS hang\n", bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_THREAD_SYNC_API:
		if (bmdi->status_sync_api == 0)
		{
			ret = bmdrv_thread_sync_api(bmdi, file, arg);
			bmdi->status_sync_api = ret;
		}
		else
		{
			pr_err("bm-sophon%d: TPU SYS hang\n", bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_HANDLE_SYNC_API:
		if (bmdi->status_sync_api == 0)
		{
			ret = bmdrv_handle_sync_api(bmdi, file, arg);
			bmdi->status_sync_api = ret;
		}
		else
		{
			pr_err("bm-sophon%d: TPU SYS hang\n", bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_DEVICE_SYNC_API:
		if (bmdi->status_sync_api == 0)
		{
			ret = bmdrv_device_sync_api(bmdi);
			bmdi->status_sync_api = ret;
		}
		else
		{
			pr_err("bm-sophon%d: TPU SYS hang\n", bmdi->dev_index);
			ret = -EBUSY;
		}
		break;

	case BMDEV_REQUEST_ARM_RESERVED:
		ret = put_user(bmdi->gmem_info.resmem_info.armreserved_addr, (unsigned long __user *)arg);
		break;

	case BMDEV_RELEASE_ARM_RESERVED:
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
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		if (c_attr->bm_get_tpu_power != NULL)
		{

		}
		else
			ret = put_user(ATTR_NOTSUPPORTED_VALUE, (u32 __user *)arg);
		;
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

	case BMDEV_ENABLE_PERF_MONITOR:
	{
		struct bm_perf_monitor perf_monitor;

		if (copy_from_user(&perf_monitor, (struct bm_perf_monitor __user *)arg,
											 sizeof(struct bm_perf_monitor)))
			return -EFAULT;

		if (bmdev_enable_perf_monitor(bmdi, &perf_monitor))
			return -EFAULT;

		break;
	}

	case BMDEV_DISABLE_PERF_MONITOR:
	{
		struct bm_perf_monitor perf_monitor;

		if (copy_from_user(&perf_monitor, (struct bm_perf_monitor __user *)arg,
											 sizeof(struct bm_perf_monitor)))
			return -EFAULT;

		if (bmdev_disable_perf_monitor(bmdi, &perf_monitor))
			return -EFAULT;

		break;
	}

	case BMDEV_GET_DEVICE_TIME:
	{
		unsigned long time_us = 0;

		time_us = bmdev_timer_get_time_us(bmdi);

		ret = copy_to_user((unsigned long __user *)arg,
											 &time_us, sizeof(unsigned long));
		break;
	}

	case BMDEV_GET_PROFILE:
	{
		struct bm_thread_info *thd_info;
		pid_t api_pid;
		struct bm_handle_info *h_info;

		if (bmdev_gmem_get_handle_info(bmdi, file, &h_info))
		{
			pr_err("bmdrv: file list is not found!\n");
			return -EINVAL;
		}

		mutex_lock(&bmdi->gmem_info.gmem_mutex);
		api_pid = current->pid;
		thd_info = bmdrv_find_thread_info(h_info, api_pid);

		if (!thd_info)
			ret = -EFAULT;
		else
			ret = copy_to_user((unsigned long __user *)arg,
												 &thd_info->profile, sizeof(bm_profile_t));
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		break;
	}
	case BMDEV_GET_VERSION:
	{
		ret = copy_to_user(((struct bootloader_version __user *)arg)->bl1_version, bmdi->cinfo.version.bl1_version, BL1_VERSION_SIZE);
		ret |= copy_to_user(((struct bootloader_version __user *)arg)->bl2_version, bmdi->cinfo.version.bl2_version, BL2_VERSION_SIZE);
		ret |= copy_to_user(((struct bootloader_version __user *)arg)->bl31_version, bmdi->cinfo.version.bl31_version, BL31_VERSION_SIZE);
		ret |= copy_to_user(((struct bootloader_version __user *)arg)->uboot_version, bmdi->cinfo.version.uboot_version, UBOOT_VERSION_SIZE);
		ret |= copy_to_user(((struct bootloader_version __user *)arg)->chip_version, bmdi->cinfo.version.chip_version, CHIP_VERSION_SIZE);
		break;
	}
	case BMDEV_LOADED_LIB:
	{
		loaded_lib_t loaded_lib;
		struct bmcpu_lib *lib_temp, *lib_next, *lib_info = bmdi->lib_dyn_info;
		;

		ret = copy_from_user(&loaded_lib, (loaded_lib_t __user *)arg, sizeof(loaded_lib_t));
		if (ret)
		{
			pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
			return ret;
		}
		loaded_lib.loaded = 0;

		mutex_lock(&lib_info->bmcpu_lib_mutex);
		list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list)
		{
			if (!memcmp(lib_temp->md5, loaded_lib.md5, 16) && loaded_lib.core_id == lib_temp->core_id)
			{
				loaded_lib.loaded = 1;
				break;
			}
		}
		mutex_unlock(&lib_info->bmcpu_lib_mutex);
		ret = copy_to_user((loaded_lib_t __user *)arg, &loaded_lib, sizeof(loaded_lib_t));
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

	case BMDEV_TRACE_ENABLE:
		ret = bmdev_trace_enable(bmdi, file);
		break;

	case BMDEV_TRACE_DISABLE:
		ret = bmdev_trace_disable(bmdi, file);
		break;

	case BMDEV_TRACEITEM_NUMBER:
		ret = bmdev_traceitem_number(bmdi, file, arg);
		break;

	case BMDEV_TRACE_DUMP:
		ret = bmdev_trace_dump_one(bmdi, file, arg);
		break;

	case BMDEV_TRACE_DUMP_ALL:
		ret = bmdev_trace_dump_all(bmdi, file, arg);
		break;

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

	case BMDEV_GET_MAXP:
	{
		struct bm_boot_info *boot_info = &bmdi->boot_info;

		ret = copy_to_user((unsigned int __user *)arg, &boot_info->max_board_power, sizeof(unsigned int));
		break;
	}

	case BMDEV_GET_BOARDP:
	{
		struct bm_device_info *c_bmdi;
		struct bm_chip_attr *c_attr;

		if (bmdi->bmcd != NULL)
		{
			if (bmdi->bmcd->card_bmdi[0] != NULL)
			{
				c_bmdi = bmdi->bmcd->card_bmdi[0];
				c_attr = &c_bmdi->c_attr;
			}
			else
			{
				return -EFAULT;
			}
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

	case BMDEV_GET_FAN:
	{
		struct bm_chip_attr *c_attr = &bmdi->c_attr;

		if (c_attr->bm_get_fan_speed != NULL)
		{
			ret = copy_to_user((u16 __user *)arg, &c_attr->fan_speed, sizeof(u16));
		}
		else
		{
			return -EFAULT;
		}
		break;
	}

	case BMDEV_SET_TPU_DIVIDER:
		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
			ret = -EPERM;
		else
		{
			mutex_lock(&bmdi->clk_reset_mutex);
			// ret = bmdev_clk_ioctl_set_tpu_divider(bmdi, arg);
			mutex_unlock(&bmdi->clk_reset_mutex);
		}
		break;

	case BMDEV_SET_TPU_FREQ:
		mutex_lock(&bmdi->clk_reset_mutex);
		// ret = bmdev_clk_ioctl_set_tpu_freq(bmdi, arg);
		mutex_unlock(&bmdi->clk_reset_mutex);
		break;

	case BMDEV_GET_TPU_FREQ:
		mutex_lock(&bmdi->clk_reset_mutex);
		// ret = bmdev_clk_ioctl_get_tpu_freq(bmdi, arg);
		mutex_unlock(&bmdi->clk_reset_mutex);
		break;

	case BMDEV_SET_MODULE_RESET:
		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
			ret = -EPERM;
		else
		{
			mutex_lock(&bmdi->clk_reset_mutex);
			// ret = bmdev_clk_ioctl_set_module_reset(bmdi, arg);
			mutex_unlock(&bmdi->clk_reset_mutex);
		}
		break;




	case BMDEV_INVALIDATE_GMEM:
	{
		u64 arg64;
		u32 arg32l, arg32h;

		if (get_user(arg64, (u64 __user *)arg))
		{
			dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
			return -EINVAL;
		}
		arg32l = (u32)arg64;
		arg32h = (u32)((arg64 >> 32) & 0xffffffff);
		bmdrv_gmem_invalidate(bmdi, ((unsigned long)arg32h) << 6, arg32l);
		break;
	}

	case BMDEV_FLUSH_GMEM:
	{
		u64 arg64;
		u32 arg32l, arg32h;

		if (get_user(arg64, (u64 __user *)arg))
		{
			dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
			return -EINVAL;
		}

		arg32l = (u32)arg64;
		arg32h = (u32)((arg64 >> 32) & 0xffffffff);
		bmdrv_gmem_flush(bmdi, ((unsigned long)arg32h) << 6, arg32l);
		break;
	}
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
	case BMDEV_PWR_CTRL:
	{
		pwr_ctrl_ioctl(bmdi, (void __user *)arg);
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

	case BMCTL_SET_LED:

		break;

	case BMCTL_SET_ECC:

		break;

	case BMCTL_DEV_RECOVERY:
		ret = -EPERM;

		break;

	case BMCTL_GET_DRIVER_VERSION:
		ret = put_user(BM_DRIVER_VERSION, (int __user *)arg);
		break;

	case BMCTL_GET_CARD_NUM:
	{
		int card_num = 0;
		card_num = bm_get_card_num_from_system();
		ret = put_user(card_num, (int __user *)arg);
		break;
	}

	case BMCTL_GET_CARD_INFO:
	{
		struct bm_card bmcd;
		if (copy_from_user(&bmcd, (struct bm_card __user *)arg, sizeof(struct bm_card)))
		{
			return -EINVAL;
		}
		ret = bm_get_card_info(&bmcd);
		if (ret != 0)
		{
			return -ENODEV;
		}
		ret = copy_to_user((struct bm_card __user *)arg, &bmcd, sizeof(struct bm_card));
		break;
	}

	default:
		pr_err("*************Invalid ioctl parameter************\n");
		return -EINVAL;
	}
	return ret;
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
