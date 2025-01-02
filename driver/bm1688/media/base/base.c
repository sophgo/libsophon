#include <linux/types.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/compat.h>

#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/base_uapi.h>
#include "vb.h"
#include "bind.h"
#include "ion.h"
#include "base_debug.h"

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define BASE_CLASS_NAME "soph-base"
#define BASE_DEV_NAME "soph-base"

u32 base_log_lv = DBG_WARN;

static atomic_t open_count = ATOMIC_INIT(0);

struct base_device {
	//struct device *dev;
	struct miscdevice miscdev;
	void *shared_mem;
	//u16 mmap_count;
	//u16 use_count;
};

module_param(base_log_lv, int, 0644);

static int base_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int i;
	UNUSED(inode);

	i = atomic_inc_return(&open_count);
	if (i > 1) {
		TRACE_BASE(DBG_INFO, "base_open: open %d times\n", i);
		return 0;
	}

	return ret;
}

static int base_release(struct inode *inode, struct file *filp)
{
	int i;
	UNUSED(inode);

	i = atomic_dec_return(&open_count);
	if (i) {
		TRACE_BASE(DBG_INFO, "base_close: open %d times\n", i);
		return 0;
	}
	vb_release();
	bind_deinit();
	TRACE_BASE(DBG_DEBUG, "base release ok\n");

	return 0;
}

static int base_mmap(struct file *filp, struct vm_area_struct *vma)
{

	return 0;
}

static long base_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case BASE_VB_CMD:
	{
		CHECK_IOCTL_CMD(cmd, struct vb_ext_control);
		ret = vb_ctrl(arg);
		break;
	}

	case BASE_SET_BINDCFG:
	{
		CHECK_IOCTL_CMD(cmd, struct sys_bind_cfg);
		ret = bind_set_cfg_user(arg);
		break;
	}

	case BASE_GET_BINDCFG:
	{
		CHECK_IOCTL_CMD(cmd, struct sys_bind_cfg);
		ret = bind_get_cfg_user(arg);
		break;
	}

	case BASE_ION_ALLOC:
	{
		struct sys_ion_data stIonDate;
		void *addr_v = NULL;

		CHECK_IOCTL_CMD(cmd, struct sys_ion_data);
		if (copy_from_user(&stIonDate, (struct sys_ion_data __user *)arg,
					sizeof(struct sys_ion_data)))
			return -EINVAL;

		ret = base_ion_alloc(&stIonDate.addr_p, &addr_v, stIonDate.name,
				stIonDate.size, stIonDate.cached);

		if (copy_to_user((struct sys_ion_data __user *)arg, &stIonDate,
					sizeof(struct sys_ion_data)))
			return -EINVAL;

		break;
	}

	case BASE_ION_FREE:
	{
		struct sys_ion_data stIonDate;
		int32_t size;

		CHECK_IOCTL_CMD(cmd, struct sys_ion_data);
		if (copy_from_user(&stIonDate, (struct sys_ion_data __user *)arg,
					sizeof(struct sys_ion_data)))
			return -EINVAL;

		ret = base_ion_free2(stIonDate.addr_p, &size);
		if (ret < 0) {
			TRACE_BASE(DBG_ERR, "base_ion_free fail\n");
			return -EINVAL;
		}
		stIonDate.size = size;

		if (copy_to_user((struct sys_ion_data __user *)arg, &stIonDate,
					sizeof(struct sys_ion_data)))
			return -EINVAL;
		break;
	}

	case BASE_CACHE_INVLD:
	{
		struct sys_cache_op stCacheOp;

		CHECK_IOCTL_CMD(cmd, struct sys_cache_op);
		if (copy_from_user(&stCacheOp, (struct sys_cache_op __user *)arg,
			sizeof(struct sys_cache_op)))
			return -EINVAL;

		ret = base_ion_cache_invalidate(stCacheOp.addr_p, stCacheOp.addr_v, stCacheOp.size);
		break;
	}

	case BASE_CACHE_FLUSH:
	{
		struct sys_cache_op stCacheOp;

		CHECK_IOCTL_CMD(cmd, struct sys_cache_op);
		if (copy_from_user(&stCacheOp, (struct sys_cache_op __user *)arg,
			sizeof(struct sys_cache_op)))
			return -EINVAL;

		ret = base_ion_cache_flush(stCacheOp.addr_p, stCacheOp.addr_v, stCacheOp.size);
		break;
	}

	default:
		TRACE_BASE(DBG_ERR, "Not support functions");
		return -ENOTTY;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long base_compat_ptr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op->unlocked_ioctl)
		return -ENOIOCTLCMD;

	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int base_probe(struct platform_device *pdev)
{
	if (vb_create_instance()) {
		TRACE_BASE(DBG_ERR, "vb_create_instance failed\n");
		return -ENOMEM;
	}

	bind_init();

	TRACE_BASE(DBG_WARN, "base probe done\n");

	return 0;
}

static int base_remove(struct platform_device *pdev)
{
	bind_deinit();
	vb_destroy_instance();
	vb_cleanup();

	return 0;
}

int drv_base_init(void)
{
	return base_probe(NULL);
}

int drv_base_deinit(void)
{
	return base_remove(NULL);
}
