#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "bm_ctl.h"
#include "bm_common.h"
#include "bm_attr.h"
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "SGTPUV8_clkrst.h"
#include "version.h"


// static struct proc_dir_entry *bmsophon_total_node;
static struct proc_dir_entry *bmsophon_proc_dir;
static struct proc_dir_entry *bmsophon_card_num;
static struct proc_dir_entry *bmsophon_chip_num;
static char debug_node_name[] = "bmsophon";
extern struct bm_ctrl_info *bmci;
extern int dev_count;
extern char release_date[];

static int bmdrv_card_nums_proc_show(struct seq_file *m, void *v)
{
	int card_num = 1;

	seq_printf(m, "%d\n", card_num);
	return 0;
}

static int bmdrv_card_nums_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_card_nums_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_card_nums_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_card_nums_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_chip_nums_proc_show(struct seq_file *m, void *v)
{
	int chip_num = 1;

	seq_printf(m, "%d\n", chip_num);
	return 0;
}

static int bmdrv_chip_nums_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chip_nums_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chip_nums_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_chip_nums_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

int bmdrv_proc_init(void)
{
	bmsophon_proc_dir = proc_mkdir(debug_node_name, NULL);
	if (!bmsophon_proc_dir)
		return -ENOMEM;

	bmsophon_card_num = proc_create("card_num", 0444, bmsophon_proc_dir, &bmdrv_card_nums_file_ops);
	if (!bmsophon_card_num)
	{
		proc_remove(bmsophon_card_num);
		return -ENOMEM;
	}
	bmsophon_chip_num = proc_create("chip_num", 0444, bmsophon_proc_dir, &bmdrv_chip_nums_file_ops);
	if (!bmsophon_chip_num)
	{
		proc_remove(bmsophon_chip_num);
		return -ENOMEM;
	}

	return 0;
}

void bmdrv_proc_deinit(void)
{
	proc_remove(bmsophon_card_num);
	proc_remove(bmsophon_chip_num);
	proc_remove(bmsophon_proc_dir);
}

#define MAX_NAMELEN 128

static int bmdrv_chipid_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;


	seq_printf(m, "0x%x\n", bmdi->cinfo.chip_id);

	return 0;
}

static int bmdrv_chipid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chipid_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chipid_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_chipid_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_tpuid_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%d\n", bmdi->dev_index);
	return 0;
}

static int bmdrv_tpuid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpuid_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpuid_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_tpuid_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_mode_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%s\n", bmdi->misc_info.pcie_soc_mode ? "soc" : "pcie");
	return 0;
}

static int bmdrv_mode_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_mode_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_mode_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_mode_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};



static int bmdrv_cdma_in_counter_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld\n", bmdi->profile.cdma_in_counter);
	return 0;
}

static int bmdrv_cdma_in_counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_cdma_in_counter_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_cdma_in_counter_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_cdma_in_counter_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_cdma_out_counter_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld\n", bmdi->profile.cdma_out_counter);
	return 0;
}

static int bmdrv_cdma_out_counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_cdma_out_counter_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_cdma_out_counter_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_cdma_out_counter_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_cdma_in_time_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld us\n", bmdi->profile.cdma_in_time);
	return 0;
}

static int bmdrv_cdma_in_time_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_cdma_in_time_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_cdma_in_time_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_cdma_in_time_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_cdma_out_time_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld us\n", bmdi->profile.cdma_out_time);
	return 0;
}

static int bmdrv_cdma_out_time_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_cdma_out_time_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_cdma_out_time_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_cdma_out_time_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_tpu_process_time_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld us\n", bmdi->profile.tpu_process_time);
	return 0;
}

static int bmdrv_tpu1_process_time_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld us\n", bmdi->profile.tpu1_process_time);
	return 0;
}

static int bmdrv_tpu_process_time_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_process_time_proc_show, PDE_DATA(inode));
}

static int bmdrv_tpu1_process_time_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu1_process_time_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_process_time_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_tpu_process_time_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static const struct BM_PROC_FILE_OPS bmdrv_tpu1_process_time_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_tpu1_process_time_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_sent_api_counter_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld\n", bmdi->profile.sent_api_counter);
	return 0;
}

static int bmdrv_sent_api_counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_sent_api_counter_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_sent_api_counter_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_sent_api_counter_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_completed_api_counter_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld\n", bmdi->profile.completed_api_counter);
	return 0;
}

static int bmdrv_completed_api_counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_completed_api_counter_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_completed_api_counter_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_completed_api_counter_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_arm9_cache_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int status = gp_reg_read_enh(bmdi, GP_REG_FW_STATUS);

	status = (status >> 28) & 0xf;
	if (status == 0x0)
		seq_printf(m, "disable\n");
	else
		seq_printf(m, "enable\n");

	return 0;
}

static int bmdrv_arm9_cache_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_arm9_cache_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_arm9_cache_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_arm9_cache_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_ddr_config_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	u64 gbyte = 1024 * 1024 * 1024;

	seq_printf(m, "%lldg, %lldg, %lldg, %lldg, %lldg\n", div_u64(bmdi->boot_info.ddr_0a_size, gbyte),
						 div_u64(bmdi->boot_info.ddr_0b_size, gbyte), div_u64(bmdi->boot_info.ddr_1_size, gbyte),
						 div_u64(bmdi->boot_info.ddr_2_size, gbyte),
						 div_u64((bmdi->boot_info.ddr_0a_size + bmdi->boot_info.ddr_0b_size +
											bmdi->boot_info.ddr_1_size + bmdi->boot_info.ddr_2_size),
										 gbyte));

	return 0;
}

static int bmdrv_ddr_config_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_ddr_config_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_ddr_config_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_ddr_config_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

static int bmdrv_chip_num_on_card_proc_show(struct seq_file *m, void *v)
{


	return 0;
}

static int bmdrv_chip_num_on_card_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chip_num_on_card_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chip_num_on_card_file_ops = {
		BM_PROC_OWNER = BM_PROC_MODULE,
		BM_PROC_OPEN = bmdrv_chip_num_on_card_proc_open,
		BM_PROC_READ = seq_read,
		BM_PROC_LLSEEK = seq_lseek,
		BM_PROC_RELEASE = single_release,
};

int bmdrv_proc_file_init(struct bm_device_info *bmdi)
{

	return 0;
}

void bmdrv_proc_file_deinit(struct bm_device_info *bmdi)
{

}

#undef MAX_NAMELEN

bool bm_arm9fw_log_buffer_empty(struct bm_device_info *bmdi, int core_id)
{
	int read_index = 0;
	int write_index = 0;

	read_index = bmdi->monitor_thread_info.log_mem[core_id].read_pos;
	if (core_id == 1)
	{
		write_index = gp_reg_read_enh(bmdi, GP_REG_FW1_LOG_WP);
	}
	else
	{
		write_index = gp_reg_read_enh(bmdi, GP_REG_FW0_LOG_WP);
	}
	//	PR_TRACE("read_index = 0x%x, write_index = 0x%x\n", read_index, write_index);
	return read_index == write_index;
}


#define ARM9FW_LOG_HOST_BUFFER_SIZE (1024 * 512)
#define ARM9FW_LOG_DEVICE_BUFFER_SIZE (1024 * 1024 * 4)
#define ARM9FW_LOG_LINE_SIZE 512

void bm_print_arm9fw_log(struct bm_device_info *bmdi, int core_id)
{
	char str[ARM9FW_LOG_LINE_SIZE] = "";
	int i = 0;
	int size = bmdi->monitor_thread_info.log_mem[core_id].read_size;
	char *p = bmdi->monitor_thread_info.log_mem[core_id].host_vaddr;

	for (i = 0; i < div_u64(size, ARM9FW_LOG_LINE_SIZE); i++)
	{
		strncpy(str, p, ARM9FW_LOG_LINE_SIZE - 1);
		pr_info("bm-sophon%d core_%d: %s", bmdi->dev_index, core_id, str);
		p += ARM9FW_LOG_LINE_SIZE;
	}
	memset(bmdi->monitor_thread_info.log_mem[core_id].host_vaddr, 0, size);
}

int bm_arm9fw_log_init(struct bm_device_info *bmdi, int core_id)
{
	int ret = 0;

	if (core_id == 0) {
		bmdi->monitor_thread_info.log_mem[core_id].device_paddr = bmdi->gmem_info.resmem_info.armfw_addr + (bmdi->gmem_info.resmem_info.armfw_size >> 1) - ARM9FW_LOG_DEVICE_BUFFER_SIZE;
	}
	bmdi->monitor_thread_info.log_mem[core_id].device_size = ARM9FW_LOG_DEVICE_BUFFER_SIZE;
	bmdi->monitor_thread_info.log_mem[core_id].host_size = ARM9FW_LOG_HOST_BUFFER_SIZE;
	ret = bmdrv_stagemem_alloc(bmdi, bmdi->monitor_thread_info.log_mem[core_id].host_size,
														 &bmdi->monitor_thread_info.log_mem[core_id].host_paddr,
														 &bmdi->monitor_thread_info.log_mem[core_id].host_vaddr);
	if (ret)
	{
		pr_err("bm-sophon%d alloc arm9fw log buffer failed\n", bmdi->dev_index);
		return ret;
	}

	memset(bmdi->monitor_thread_info.log_mem[core_id].host_vaddr, 0,
				 bmdi->monitor_thread_info.log_mem[core_id].host_size);

	// gp_reg_write_enh(bmdi, GP_REG_ARM9FW_LOG_RP, 0);
	bmdi->monitor_thread_info.log_mem[core_id].read_pos = 0;
	PR_TRACE("host size = 0x%x, device_addr = 0x%llx, device size = 0x%x\n",
					 bmdi->monitor_thread_info.log_mem[core_id].host_size, bmdi->monitor_thread_info.log_mem[core_id].device_paddr,
					 bmdi->monitor_thread_info.log_mem[core_id].device_size);

	return ret;
}



void bm_dump_arm9fw_log(struct bm_device_info *bmdi, int count)
{
	// int size = 0;
	long start = 0;
	long end = 0;
	int delt = 0;
	int core_id = 0;

	start = jiffies;
	bm_npu_utilization_stat(bmdi);
	if (count % 3 == 0)
	{
		for (core_id = 0; core_id < 2; core_id++)
		{
			if (!bm_arm9fw_log_buffer_empty(bmdi, core_id))
			{
				// size = bm_get_arm9fw_log_from_device(bmdi, core_id);
				bm_print_arm9fw_log(bmdi, core_id);
				end = jiffies;
				delt = jiffies_to_msecs(end - start);
				// PR_TRACE("dev_index=%d,bm_dump_arm9fw_log time is %d ms\n",bmdi->dev_index, delt);
				msleep_interruptible(((10 - delt) > 0) ? 10 - delt : 0);
			}
		}
	}
	else if (count % 10 == 0x0)
	{

		end = jiffies;
		delt = jiffies_to_msecs(end - start);
		// PR_TRACE("dev_index=%d,bm_dump_arm9fw_log time is %d ms\n",bmdi->dev_index, delt);
		msleep_interruptible(((10 - delt) > 0) ? 10 - delt : 0);
	}
	else
	{
		msleep_interruptible(10);
		// PR_TRACE("buffer is empty\n");
	}
}

char *bmdrv_get_error_string(int error)
{
	int er = abs(error);
	switch (er)
	{
	case EPERM:
		return "Operation not permitted";
	case ENOENT:
		return "No such file or directory";
	case ESRCH:
		return "No such process";
	case EINTR:
		return "Interrupted system call";
	case EIO:
		return "I/O error";
	case ENXIO:
		return "No such device or address";
	case E2BIG:
		return "Argument list too long";
	case ENOEXEC:
		return "Exec format error";
	case EBADF:
		return "Bad file number";
	case ECHILD:
		return "No child processes";
	case EAGAIN:
		return "Try again Or Operation would block";
	case ENOMEM:
		return "Out of memory";
	case EACCES:
		return "Permission denied";
	case EFAULT:
		return "Bad address";
	case ENOTBLK:
		return "Block device required";
	case EBUSY:
		return "Device or resource busy";
	case EEXIST:
		return "File exists";
	case EXDEV:
		return "Cross-device link";
	case ENODEV:
		return "No such device";
	case ENOTDIR:
		return "Not a directory";
	case EISDIR:
		return "Is a directory";
	case EINVAL:
		return "Invalid argument";
	case ENFILE:
		return "File table overflow";
	case EMFILE:
		return "Too many open files";
	case ENOTTY:
		return "Not a typewriter";
	case ETXTBSY:
		return "Text file busy";
	case EFBIG:
		return "File too large";
	case ENOSPC:
		return "No space left on device";
	case ESPIPE:
		return "Illegal seek";
	case EROFS:
		return "Read-only file system";
	case EMLINK:
		return "Too many links";
	case EPIPE:
		return "Broken pipe";
	case EDOM:
		return "Math argument out of domain of func";
	case ERANGE:
		return "Math result not representable";

	case EDEADLK:
		return "Resource deadlock would occur";
	case ENAMETOOLONG:
		return "File name too long";
	case ENOLCK:
		return "No record locks available";
	case ENOSYS:
		return "Invalid system call number";
	case ENOTEMPTY:
		return "Directory not empty";
	case ELOOP:
		return "Too many symbolic links encountered";
	case ENOMSG:
		return "No message of desired type";
	case EIDRM:
		return "Identifier removed";
	case ECHRNG:
		return "Channel number out of range";
	case EL2NSYNC:
		return "Level 2 not synchronized";
	case EL3HLT:
		return "Level 3 halted";
	case EL3RST:
		return "Level 3 reset";
	case ELNRNG:
		return "Link number out of range";
	case EUNATCH:
		return "Protocol driver not attached";
	case ENOCSI:
		return "No CSI structure available";
	case EL2HLT:
		return "Level 2 halted";
	case EBADE:
		return "Invalid exchange";
	case EBADR:
		return "Invalid request descriptor";
	case EXFULL:
		return "Exchange full";
	case ENOANO:
		return "No anode";
	case EBADRQC:
		return "Invalid request code";
	case EBADSLT:
		return "Invalid slot";
	case EBFONT:
		return "Bad font file format";
	case ENOSTR:
		return "Device not a stream";
	case ENODATA:
		return "No data available";
	case ETIME:
		return "Timer expired";
	case ENOSR:
		return "Out of streams resources";
	case ENONET:
		return "Machine is not on the network";
	case ENOPKG:
		return "Package not installed";
	case EREMOTE:
		return "Object is remote";
	case ENOLINK:
		return "Link has been severed";
	case EADV:
		return "Advertise error";
	case ESRMNT:
		return "Srmount error";
	case ECOMM:
		return "Communication error on send";
	case EPROTO:
		return "Protocol error";
	case EMULTIHOP:
		return "Multihop attempted";
	case EDOTDOT:
		return "RFS specific error";
	case EBADMSG:
		return "Not a data message";
	case EOVERFLOW:
		return "Value too large for defined data type";
	case ENOTUNIQ:
		return "Name not unique on network";
	case EBADFD:
		return "File descriptor in bad state";
	case EREMCHG:
		return "Remote address changed";
	case ELIBACC:
		return "Can not access a needed shared library";
	case ELIBBAD:
		return "Accessing a corrupted shared library";
	case ELIBSCN:
		return ".lib section in a.out corrupted";
	case ELIBMAX:
		return "Attempting to link in too many shared libraries";
	case ELIBEXEC:
		return "Cannot exec a shared library directly";
	case EILSEQ:
		return "Illegal byte sequence";
	case ERESTART:
		return "Interrupted system call should be restarted";
	case ESTRPIPE:
		return "Streams pipe error";
	case EUSERS:
		return "Too many users";
	case ENOTSOCK:
		return "Socket operation on non-socket";
	case EDESTADDRREQ:
		return "Destination address required";
	case EMSGSIZE:
		return "Message too long";
	case EPROTOTYPE:
		return "Protocol wrong type for socket";
	case ENOPROTOOPT:
		return "Protocol not available";
	case EPROTONOSUPPORT:
		return "Protocol not supported";
	case ESOCKTNOSUPPORT:
		return "Socket type not supported";
	case EOPNOTSUPP:
		return "Operation not supported on transport endpoint";
	case EPFNOSUPPORT:
		return "Protocol family not supported";
	case EAFNOSUPPORT:
		return "Address family not supported by protocol";
	case EADDRINUSE:
		return "Address already in use";
	case EADDRNOTAVAIL:
		return "Cannot assign requested address";
	case ENETDOWN:
		return "Network is down";
	case ENETUNREACH:
		return "Network is unreachable";
	case ENETRESET:
		return "Network dropped connection because of reset";
	case ECONNABORTED:
		return "Software caused connection abort";
	case ECONNRESET:
		return "Connection reset by peer";
	case ENOBUFS:
		return "No buffer space available";
	case EISCONN:
		return "Transport endpoint is already connected";
	case ENOTCONN:
		return "Transport endpoint is not connected";
	case ESHUTDOWN:
		return "Cannot send after transport endpoint shutdown";
	case ETOOMANYREFS:
		return "Too many references: cannot splice";
	case ETIMEDOUT:
		return "Connection timed out";
	case ECONNREFUSED:
		return "Connection refused";
	case EHOSTDOWN:
		return "Host is down";
	case EHOSTUNREACH:
		return "No route to host";
	case EALREADY:
		return "Operation already in progress";
	case EINPROGRESS:
		return "Operation now in progress";
	case ESTALE:
		return "Stale file handle";
	case EUCLEAN:
		return "Structure needs cleaning";
	case ENOTNAM:
		return "Not a XENIX named type file";
	case ENAVAIL:
		return "No XENIX semaphores available";
	case EISNAM:
		return "Is a named type file";
	case EREMOTEIO:
		return "Remote I/O error";
	case EDQUOT:
		return "Quota exceeded";
	case ENOMEDIUM:
		return "No medium found";
	case EMEDIUMTYPE:
		return "Wrong medium type";
	case ECANCELED:
		return "Operation Canceled";
	case ENOKEY:
		return "Required key not available";
	case EKEYREVOKED:
		return "Key has been revoked";
	case EKEYREJECTED:
		return "Key was rejected by service";
	case EOWNERDEAD:
		return "Owner died";
	case ENOTRECOVERABLE:
		return "State not recoverable";
	case ERFKILL:
		return "Operation not possible due to RF-kill";
	case EHWPOISON:
		return "Memory page has hardware error";
	}
	return "Unknown Error Dash!";
}
