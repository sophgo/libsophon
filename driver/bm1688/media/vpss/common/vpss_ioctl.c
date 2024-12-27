#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
//#include <linux/module.h>

// #include <vpss_cb.h>

#include "vpss_debug.h"
#include "vpss_core.h"
#include "vpss.h"
#include "vpss_ioctl.h"
#include "base_ctx.h"

long vpss_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	char stack_kdata[128];
	char *kdata = stack_kdata;
	int ret = 0;
	unsigned int in_size, out_size, drv_size, ksize;

	/* Figure out the delta between user cmd size and kernel cmd size */
	drv_size = _IOC_SIZE(cmd);
	out_size = _IOC_SIZE(cmd);
	in_size = out_size;
	if ((cmd & IOC_IN) == 0)
		in_size = 0;
	if ((cmd & IOC_OUT) == 0)
		out_size = 0;
	ksize = max(max(in_size, out_size), drv_size);

	/* If necessary, allocate buffer for ioctl argument */
	if (ksize > sizeof(stack_kdata)) {
		kdata = kmalloc(ksize, GFP_KERNEL);
		if (!kdata)
			return -ENOMEM;
	}

	if (!access_ok((void __user *)arg, in_size)) {
		TRACE_VPSS(DBG_ERR, "access_ok failed\n");
	}

	ret = copy_from_user(kdata, (void __user *)arg, in_size);
	if (ret != 0) {
		TRACE_VPSS(DBG_INFO, "copy_from_user failed: ret=%d\n", ret);
		goto err;
	}

	/* zero out any difference between the kernel/user structure size */
	if (ksize > in_size)
		memset(kdata + in_size, 0, ksize - in_size);

	switch (cmd) {
	case VPSS_BM_SEND_FRAME:
	{
		bm_vpss_cfg *cfg = (bm_vpss_cfg *)kdata;

		CHECK_IOCTL_CMD(cmd, bm_vpss_cfg);
		ret = vpss_bm_send_frame(cfg);
		break;
	}

	default:
		TRACE_VPSS(DBG_DEBUG, "unknown cmd(0x%x)\n", cmd);
		break;
	}

	if (copy_to_user((void __user *)arg, kdata, out_size) != 0)
		ret = -EFAULT;

err:
	if (kdata != stack_kdata)
		kfree(kdata);

	return ret;
}
