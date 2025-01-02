#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include "base_ctx.h"

#include "vpss_debug.h"
#include "vpss_proc.h"
#include "vpss_common.h"
#include "scaler.h"
#include "vpss_core.h"
#include "vpss.h"

#define VPSS_PROC_NAME          "bmsophon/vpss"
#define VPP_PROC_NAME          "vppinfo"

// for proc info
static int proc_vpss_mode;
static const char * const vb_source[] = {"CommonVB", "UserVB", "UserIon"};
static const char * const vpss_name[] = {"vpss_v0", "vpss_v1", "vpss_v2", "vpss_v3",
										"vpss_t0", "vpss_t1", "vpss_t2", "vpss_t3",
										"vpss_d0", "vpss_d1"};

/*************************************************************************
 *	VPSS proc functions
 *************************************************************************/
int vpss_ctx_proc_show(struct seq_file *m, void *v)
{
	int i;
	char c[32];
	struct vpss_device *dev = (struct vpss_device *)m->private;

	seq_puts(m, "\n-------------------------------VPSS HW STATUS-----------------------\n");
	seq_printf(m, "%10s%10s%10s%10s%10s%10s%10s\n",
		"ID", "Dev", "Online", "Status", "StartCnt", "IntCnt", "DutyRatio");
	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		int state = atomic_read(&dev->vpss_cores[i].state);

		memset(c, 0, sizeof(c));
		if (state == VIP_IDLE)
			strncpy(c, "Idle", sizeof(c));
		else if (state == VIP_RUNNING)
			strncpy(c, "Running", sizeof(c));
		else if (state == VIP_END)
			strncpy(c, "End", sizeof(c));
		else if (state == VIP_ONLINE)
			strncpy(c, "Online", sizeof(c));

		seq_printf(m, "%8s%2d%10s%10s%10s%10d%10d%10d\n",
			"#",
			i,
			vpss_name[i],
			dev->vpss_cores[i].is_online ? "y" : "N",
			c,
			dev->vpss_cores[i].start_cnt,
			dev->vpss_cores[i].int_cnt,
			dev->vpss_cores[i].duty_ratio);
	}

	return 0;
}

int vpp_ctx_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct vpss_device *dev = (struct vpss_device *)m->private;

	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		seq_printf(m, "{\"id\":%d, \"usage(instant|long)\":%10d%%|%10d%% \n", i, dev->vpss_cores[i].duty_ratio,\
                dev->vpss_cores[i].duty_ratio_long);
	}

	return 0;
}

static int vpss_proc_show(struct seq_file *m, void *v)
{
	// show driver status if vpss_mode == 1
	//if (proc_vpss_mode) {
	//}
	return vpss_ctx_proc_show(m, v);
}

static int vpp_proc_show(struct seq_file *m, void *v)
{

	return vpp_ctx_proc_show(m, v);
}

static ssize_t vpss_proc_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char proc_input_data[32] = {'\0'};

	if (user_buf == NULL || count >= sizeof(proc_input_data)) {
		pr_err("Invalid input value\n");
		return -EINVAL;
	}

	if (copy_from_user(proc_input_data, user_buf, count)) {
		pr_err("copy_from_user fail\n");
		return -EFAULT;
	}

	if (kstrtoint(proc_input_data, 10, &proc_vpss_mode))
		proc_vpss_mode = 0;

	return count;
}

static int vpss_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpss_proc_show, PDE_DATA(inode));
}

static ssize_t vpp_proc_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char proc_input_data[32] = {'\0'};

	if (user_buf == NULL || count >= sizeof(proc_input_data)) {
		pr_err("Invalid input value\n");
		return -EINVAL;
	}

	if (copy_from_user(proc_input_data, user_buf, count)) {
		pr_err("copy_from_user fail\n");
		return -EFAULT;
	}

	return count;
}

static int vpp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpp_proc_show, PDE_DATA(inode));
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static const struct proc_ops vpss_proc_fops = {
	.proc_open = vpss_proc_open,
	.proc_read = seq_read,
	.proc_write = vpss_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations vpss_proc_fops = {
	.owner = THIS_MODULE,
	.open = vpss_proc_open,
	.read = seq_read,
	.write = vpss_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static const struct proc_ops vpp_proc_fops = {
	.proc_open = vpp_proc_open,
	.proc_read = seq_read,
	.proc_write = vpp_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations vpp_proc_fops = {
	.owner = THIS_MODULE,
	.open = vpp_proc_open,
	.read = seq_read,
	.write = vpp_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int vpss_proc_init(struct vpss_device *dev)
{
	struct proc_dir_entry *entry;

	entry = proc_create_data(VPSS_PROC_NAME, 0644, NULL,
				 &vpss_proc_fops, dev);
	if (!entry) {
		TRACE_VPSS(DBG_ERR, "vpss proc creation failed\n");
		return -ENOMEM;
	}

	return 0;
}

int vpss_proc_remove(struct vpss_device *dev)
{
	remove_proc_entry(VPSS_PROC_NAME, NULL);
	return 0;
}

int vpp_proc_init(struct vpss_device *dev)
{
	struct proc_dir_entry *entry;

	entry = proc_create_data(VPP_PROC_NAME, 0644, NULL,
				 &vpp_proc_fops, dev);
	if (!entry) {
		TRACE_VPSS(DBG_ERR, "vpss proc creation failed\n");
		return -ENOMEM;
	}

	return 0;
}

int vpp_proc_remove(struct vpss_device *dev)
{
	remove_proc_entry(VPP_PROC_NAME, NULL);
	return 0;
}