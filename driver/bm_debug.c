#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "bm_ctl.h"
#include "bm_common.h"
#include "bm_attr.h"
#include "i2c.h"
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "bm1684/bm1684_card.h"
#include "bm1684/bm1684_jpu.h"
#include "vpu/vpu.h"
#include "bm1684_clkrst.h"
#include "version.h"
#ifndef SOC_MODE
#include <linux/pci.h>
#include <bm_pcie.h>
#include <bm_card.h>
#include "bm1684/bm1684_flash.h"
#endif

//static struct proc_dir_entry *bmsophon_total_node;
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
#ifndef SOC_MODE
	card_num = bm_get_card_num_from_system();
#endif
	seq_printf(m, "%d\n", card_num);
	return 0;
}

static int bmdrv_card_nums_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_card_nums_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_card_nums_file_ops = {
	BM_PROC_OWNER          = BM_PROC_MODULE,
	BM_PROC_OPEN           = bmdrv_card_nums_proc_open,
	BM_PROC_READ           = seq_read,
	BM_PROC_LLSEEK         = seq_lseek,
	BM_PROC_RELEASE        = single_release,
};

static int bmdrv_chip_nums_proc_show(struct seq_file *m, void *v)
{
	int chip_num = 1;
#ifndef SOC_MODE
	chip_num = bm_get_chip_num_from_system();
#endif
	seq_printf(m, "%d\n", chip_num);
	return 0;
}

static int bmdrv_chip_nums_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chip_nums_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chip_nums_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_chip_nums_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

int bmdrv_proc_init(void)
{
	bmsophon_proc_dir = proc_mkdir(debug_node_name, NULL);
	if (!bmsophon_proc_dir)
		return -ENOMEM;

	bmsophon_card_num = proc_create("card_num", 0444, bmsophon_proc_dir, &bmdrv_card_nums_file_ops);
	if (!bmsophon_card_num) {
		proc_remove(bmsophon_card_num);
		return -ENOMEM;
	}
	bmsophon_chip_num = proc_create("chip_num", 0444, bmsophon_proc_dir, &bmdrv_chip_nums_file_ops);
	if (!bmsophon_chip_num) {
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

	if (bmdi->cinfo.chip_id== 0x1686)
		seq_printf(m, "0x1684x\n");
	else
		seq_printf(m, "0x%x\n", bmdi->cinfo.chip_id);

	return 0;
}

static int bmdrv_chipid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chipid_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chipid_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_chipid_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpuid_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_mode_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

#ifndef SOC_MODE
static int bmdrv_dbdf_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%03x:%02x:%02x.%1x\n", bmdi->misc_info.domain_bdf>>16,
		(bmdi->misc_info.domain_bdf&0xffff)>>8,
		(bmdi->misc_info.domain_bdf&0xff)>>3,
		(bmdi->misc_info.domain_bdf&0x7));
	return 0;
}

static int bmdrv_dbdf_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_dbdf_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_dbdf_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_dbdf_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_status_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int stat = bmdi->status;

	if (stat == 0) {
		seq_printf(m, "%s\n", "Active");
	} else {
		seq_printf(m, "%s\n", "Fault");
	}
	return 0;
}

static int bmdrv_status_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_status_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_status_file_ops = {
        BM_PROC_OWNER          = BM_PROC_MODULE,
        BM_PROC_OPEN           = bmdrv_status_proc_open,
        BM_PROC_READ           = seq_read,
        BM_PROC_LLSEEK         = seq_lseek,
        BM_PROC_RELEASE        = single_release,
};

static int bmdrv_tpu_minclk_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%dMHz\n", bmdi->boot_info.tpu_min_clk);
	return 0;
}

static int bmdrv_tpu_minclk_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_minclk_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_minclk_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_minclk_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_tpu_maxclk_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%dMHz\n", bmdi->boot_info.tpu_max_clk);
	return 0;
}

static int bmdrv_tpu_maxclk_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_maxclk_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_maxclk_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_maxclk_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_tpu_maxboardp_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_board_power != NULL)
		seq_printf(m, "%d W\n", bmdi->boot_info.max_board_power);
	else
		seq_printf(m, "N/A\n");
	return 0;
}

static int bmdrv_tpu_maxboardp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_maxboardp_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_maxboardp_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_maxboardp_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_ecc_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%s\n", bmdi->misc_info.ddr_ecc_enable ? "on" : "off");
	return 0;
}

static int bmdrv_ecc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_ecc_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_ecc_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_ecc_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_dynfreq_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%d\n", bmdi->enable_dyn_freq);
	return 0;
}

static int bmdrv_dynfreq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_dynfreq_proc_show, PDE_DATA(inode));
}

static ssize_t bmdrv_dynfreq_proc_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[2];
	struct bm_device_info *bmdi = NULL;
	struct seq_file *s = NULL;

	s = file->private_data;
	bmdi = s->private;

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "0", 1))
		bmdi->enable_dyn_freq = 0;
	else if (!strncmp(buf, "1", 1))
		bmdi->enable_dyn_freq = 1;
	else
		pr_err("invalid val for dyn freq\n");
	return count;
}

static const struct BM_PROC_FILE_OPS bmdrv_dynfreq_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_dynfreq_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_WRITE		= bmdrv_dynfreq_proc_write,
};

static int bmdrv_dumpreg_proc_show(struct seq_file *m, void *v)
{
    struct bm_device_info *bmdi = m->private;
    int i=0;

    if(bmdi->dump_reg_type == 0)
        seq_printf(m, " echo parameter is 0 ,echo 0 don't dump register\n help tips: echo 1 dump tpu register; echo 2 dump gdma register; echo others not supported\n");
    else if(bmdi->dump_reg_type == 1){
        seq_printf(m, "DEV %d tpu command reg:\n", bmdi-> dev_index);
        for(i=0; i<32; i++)
          seq_printf(m, "BDC_CMD_REG %d: addr= 0x%08x, value = 0x%08x\n",i, bmdi->cinfo.bm_reg->tpu_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + i*4));

        seq_printf(m, "DEV %d tpu control reg:\n", bmdi-> dev_index);
        for(i=0; i<64; i++)
          seq_printf(m, "BDC_CTL_REG %d: addr= 0x%08x, value = 0x%08x\n",i, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4));
    }
    else if(bmdi->dump_reg_type == 2){
        seq_printf(m, "DEV %d gdma all reg:\n", bmdi-> dev_index);
        for(i=0; i<32; i++)
          seq_printf(m, "GDMA_ALL_REG %d: addr= 0x%08x, value = 0x%08x\n",i, bmdi->cinfo.bm_reg->gdma_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr + i*4));
        }
    else{
        seq_printf(m, "invalid echo val for dumpreg,shoud be 0,1,2\n");
        }

    return 0;
}

static int bmdrv_dumpreg_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_dumpreg_proc_show, PDE_DATA(inode));
}

static ssize_t bmdrv_dumpreg_proc_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[2];
	struct bm_device_info *bmdi = NULL;
	struct seq_file *s = NULL;

	s = file->private_data;
	bmdi = s->private;

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "0", 1))
		bmdi->dump_reg_type = 0;
	else if (!strncmp(buf, "1", 1))
		bmdi->dump_reg_type = 1;
	else if (!strncmp(buf, "2", 1))
		bmdi->dump_reg_type = 2;
	else
		bmdi->dump_reg_type = 0xff;
	return count;
}


static const struct BM_PROC_FILE_OPS bmdrv_dumpreg_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_dumpreg_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_WRITE		= bmdrv_dumpreg_proc_write,
};

static int bmdrv_fan_speed_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	u32 fan;

	if (bmdi->boot_info.fan_exist) {
		c_attr = &bmdi->c_attr;
		mutex_lock(&c_attr->attr_mutex);
		fan = bm_get_fan_speed(bmdi);
		mutex_unlock(&c_attr->attr_mutex);

		seq_printf(m, "duty  %d\n", fan);
		seq_printf(m, "fan_speed  %d\n", c_attr->fan_rev_read);
	} else {
		seq_printf(m, "N/A\n");
	}

	return 0;
}

static int bmdrv_fan_speed_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_fan_speed_proc_show, PDE_DATA(inode));
}

static ssize_t bmdrv_fan_speed_proc_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct bm_device_info *bmdi = NULL;
	struct seq_file *s = NULL;
	int res;

	s = file->private_data;
	bmdi = s->private;
	if (bmdi->boot_info.fan_exist) {
		if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf), count)))
			return -EFAULT;

		if ((kstrtoint(buf, 10, &res)) != 0) {
			return -EFAULT;
		} /* String to integer data */

		if ((res < 0) || (res > 100)) {
			pr_err("Error, valid value range is 0 ~ 100\n");
			return -1;
		} else {
			bmdi->fixed_fan_speed = res;
			return count;
		}
	} else {
		pr_err("not supprot\n");
		return -1;
	}
}

static const struct BM_PROC_FILE_OPS bmdrv_fan_speed_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN = bmdrv_fan_speed_proc_open,
	BM_PROC_READ = seq_read,
	BM_PROC_WRITE = bmdrv_fan_speed_proc_write,
};

static int bmdrv_pcie_link_speed_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct pci_dev *pdev = bmdi->cinfo.pcidev;
	u32 link_status = 0;

	pci_read_config_dword(pdev, 0x80, &link_status);
	link_status = (link_status >> 16) & 0xf;
	seq_printf(m, "gen%d\n", link_status);
	return 0;
}

static int bmdrv_pcie_link_speed_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_pcie_link_speed_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_pcie_link_speed_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_pcie_link_speed_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_pcie_link_width_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct pci_dev *pdev = bmdi->cinfo.pcidev;
	u32 link_status = 0;

	pci_read_config_dword(pdev, 0x80, &link_status);
	link_status = (link_status >> 20) & 0x3f;
	seq_printf(m, "x%d\n", link_status);
	return 0;
}

static int bmdrv_pcie_link_width_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_pcie_link_width_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_pcie_link_width_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_pcie_link_width_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_pcie_cap_speed_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct pci_dev *pdev = bmdi->cinfo.pcidev;
	u32 link_status = 0;

	pci_read_config_dword(pdev, 0x7c, &link_status);
	link_status = (link_status >> 0) & 0xf;
	seq_printf(m, "gen%d\n", link_status);
	return 0;
}

static int bmdrv_pcie_cap_speed_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_pcie_cap_speed_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_pcie_cap_speed_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_pcie_cap_speed_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_pcie_cap_width_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct pci_dev *pdev = bmdi->cinfo.pcidev;
	u32 link_status = 0;

	pci_read_config_dword(pdev, 0x7c, &link_status);
	link_status = (link_status >> 4) & 0x3f;
	seq_printf(m, "x%d\n", link_status);
	return 0;
}

static int bmdrv_pcie_cap_width_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_pcie_cap_width_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_pcie_cap_width_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_pcie_cap_width_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_pcie_region_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_bar_info bar_info = bmdi->cinfo.bar_info;
	int size = 1024 * 1024;

	seq_printf(m, "%lldM, %lldM, %lldM, %lldM\n",
		bar_info.bar0_len / size, bar_info.bar1_len / size,
		bar_info.bar2_len / size, bar_info.bar4_len / size);
	return 0;
}

static int bmdrv_pcie_region_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_pcie_region_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_pcie_region_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_pcie_region_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_tpu_freq_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int vdd_tpu_freq = 0x0;

	c_attr = &bmdi->c_attr;
	mutex_lock(&c_attr->attr_mutex);
	vdd_tpu_freq = bmdrv_1684_clk_get_tpu_freq(bmdi);
	mutex_unlock(&c_attr->attr_mutex);
	seq_printf(m, "%d MHz\n",vdd_tpu_freq);
	return 0;
}

static ssize_t bmdrv_tpu_freq_proc_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *f_pos)
{
	char *buf = kzalloc((count+1), GFP_KERNEL);
	struct bm_device_info *bmdi = NULL;
	struct seq_file *s = NULL;
	int res;

	s = file->private_data;
	bmdi = s->private;
	if(copy_from_user(buf, buffer, min_t(size_t, sizeof(buf), count)))
	{
		kfree(buf);
        return EFAULT;
	}
	if ((kstrtoint(buf, 10, &res)) != 0) {
		kfree(buf);
		return -EFAULT;
	}
	if ((res < 750) || (res > 1000)) {
		pr_err("Error, valid value range is 750MHz ~ 1GHz\n");
		kfree(buf);
		return -1;
	} else {
		bmdrv_clk_set_tpu_target_freq(bmdi,res);
		kfree(buf);
		return count;
	}
}

static int bmdrv_tpu_freq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_freq_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_freq_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_freq_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
	BM_PROC_WRITE = bmdrv_tpu_freq_proc_write,
};

static int bmdrv_tpu_volt_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int vdd_tpu_volt = 0x0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_tpu_power != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		bm_read_vdd_tpu_voltage(bmdi, &vdd_tpu_volt);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d mV\n", vdd_tpu_volt);
	} else {
		seq_printf(m, "N/A\n");
	}

	return 0;
}

static ssize_t bmdrv_tpu_volt_proc_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *f_pos)
{
	char *buf = kzalloc((count+1), GFP_KERNEL);
	struct bm_device_info *bmdi = NULL;
	struct seq_file *s = NULL;
	u32 res;

	s = file->private_data;
	bmdi = s->private;
	if(copy_from_user(buf, buffer, min_t(size_t, sizeof(buf), count)))
	{
		kfree(buf);
        return EFAULT;
	}
	if ((kstrtouint(buf, 10, &res)) != 0) {
		kfree(buf);
		return -EFAULT;
	}
	if ((res < 550) || (res > 820)) {
		pr_err("Error, valid value range is 550mv ~ 820mv\n");
		kfree(buf);
		return -1;
	} else {
		bm_set_vdd_tpu_voltage(bmdi,res);
		kfree(buf);
		return count;
	}
}

static int bmdrv_tpu_volt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_volt_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_volt_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_volt_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
	BM_PROC_WRITE = bmdrv_tpu_volt_proc_write,
};

static int bmdrv_vddc_volt_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int vddc_volt = 0x0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_vddc_power != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		bm_read_vddc_voltage(bmdi, &vddc_volt);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d mV\n", vddc_volt);
	} else {
		seq_printf(m, "N/A\n");
	}

	return 0;
}

static ssize_t bmdrv_vddc_volt_proc_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *f_pos)
{
	char *buf = kzalloc((count+1), GFP_KERNEL);
	struct bm_device_info *bmdi = NULL;
	struct seq_file *s = NULL;
	u32 res;

	s = file->private_data;
	bmdi = s->private;
	if(copy_from_user(buf, buffer, min_t(size_t, sizeof(buf), count)))
	{
		kfree(buf);
		return EFAULT;
	}
	if ((kstrtouint(buf, 10, &res)) != 0) {
		kfree(buf);
		return -EFAULT;
	}
	if ((res < 750) || (res > 950)) {
		pr_err("Error, valid value range is 750mv ~ 950mv\n");
		kfree(buf);
		return -1;
	} else {
		bm_set_vddc_voltage(bmdi,res);
		kfree(buf);
		return count;
	}
}

static int bmdrv_vddc_volt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_vddc_volt_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_vddc_volt_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_vddc_volt_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
	BM_PROC_WRITE = bmdrv_vddc_volt_proc_write,
};

static int bmdrv_tpu_cur_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int vdd_tpu_cur = 0x0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_tpu_power != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		bm_read_vdd_tpu_current(bmdi, &vdd_tpu_cur);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d mA\n", vdd_tpu_cur);
	} else {
		seq_printf(m, "N/A\n");
	}

	return 0;
}

static int bmdrv_tpu_cur_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_cur_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_cur_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_cur_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_tpu_power_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int tpu_power = 0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_tpu_power != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		c_attr->bm_get_tpu_power(bmdi, &tpu_power);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d W\n", tpu_power);
	} else {
		seq_printf(m, "N/A\n");
	}
	return 0;
}

static int bmdrv_tpu_power_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_power_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_power_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_power_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_vddc_power_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int vddc_power = 0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_vddc_power != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		c_attr->bm_get_vddc_power(bmdi, &vddc_power);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d W\n", vddc_power);
	} else {
		seq_printf(m, "N/A\n");
	}

	return 0;
}

static int bmdrv_vddc_power_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_vddc_power_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_vddc_power_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_vddc_power_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE		= single_release,
};

static int bmdrv_vddphy_power_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int vddphy_power = 0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_vddphy_power != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		c_attr->bm_get_vddphy_power(bmdi, &vddphy_power);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d W\n", vddphy_power);
	} else {
		seq_printf(m, "N/A\n");
	}
	return 0;
}

static int bmdrv_vddphy_power_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_vddphy_power_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_vddphy_power_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_vddphy_power_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE		= single_release,
};

static int bmdrv_chip_power_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int chip_power = 0;

	c_attr = &bmdi->c_attr;
	if ((c_attr->bm_get_vddphy_power != NULL) &&
		(c_attr->bm_get_tpu_power != NULL) &&
		(c_attr->bm_get_vddc_power != NULL)) {
		mutex_lock(&c_attr->attr_mutex);
		chip_power = c_attr->vddc_power + c_attr->tpu_power + c_attr->vddphy_power;
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d W\n", chip_power);
	} else {
		seq_printf(m, "N/A\n");
	}
	return 0;
}

static int bmdrv_chip_power_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chip_power_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chip_power_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_chip_power_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE		= single_release,
};

static int bmdrv_firmware_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%s\n", bmdi->firmware_info);

	return 0;
}

static int bmdrv_firmware_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_firmware_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_firmware_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_firmware_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_board_power_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int board_power = 0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_board_power != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		c_attr->bm_get_board_power(bmdi, &board_power);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d W\n", board_power);
	} else {
		seq_printf(m, "N/A\n");
	}
	return 0;
}

static int bmdrv_board_power_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_board_power_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_board_power_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_board_power_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_chip_temp_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int chip_temp = 0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_chip_temp != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		c_attr->bm_get_chip_temp(bmdi, &chip_temp);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d C\n", chip_temp);
	} else {
		seq_printf(m, "N/A\n");
	}
	return 0;
}

static int bmdrv_chip_temp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chip_temp_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chip_temp_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_chip_temp_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_board_temp_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	int board_temp = 0;

	c_attr = &bmdi->c_attr;
	if (c_attr->bm_get_board_temp != NULL) {
		mutex_lock(&c_attr->attr_mutex);
		c_attr->bm_get_board_temp(bmdi, &board_temp);
		mutex_unlock(&c_attr->attr_mutex);
		seq_printf(m, "%d C\n", board_temp);
	} else {
		seq_printf(m, "N/A\n");
	}
	return 0;
}

static int bmdrv_board_temp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_board_temp_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_board_temp_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_board_temp_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_board_sn_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_chip_attr *c_attr;
	char sn[18] = "";
	struct bm_device_info *tmp_bmdi = bmdi;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if ((bmdi->bmcd->sc5p_mcu_bmdi) != NULL && (bmdi->bmcd != NULL))
			tmp_bmdi = bmdi->bmcd->sc5p_mcu_bmdi;
		else {
			pr_err("bmsopho %d, debug board get sc5p_mcu_bmdi fail\n", bmdi->dev_index);
			return -1;
		}
	}

	c_attr = &tmp_bmdi->c_attr;
	mutex_lock(&c_attr->attr_mutex);
	bm_get_sn(tmp_bmdi, sn);
	mutex_unlock(&c_attr->attr_mutex);

	seq_printf(m, "%s\n", sn);
	return 0;
}

static int bmdrv_board_sn_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_board_sn_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_board_sn_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_board_sn_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_mcu_version_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	seq_printf(m, "V%d\n", BM1684_MCU_VERSION(bmdi));
	return 0;
}

static int bmdrv_mcu_versions_proc_show(struct bm_device_info *bmdi, struct seq_file *m, void *v)
{
	seq_printf(m, "V%d\n", BM1684_MCU_VERSION(bmdi));
	return 0;
}

static int bmdrv_mcu_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_mcu_version_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_mcu_version_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_mcu_version_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_board_type_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	char type[10] = "";
	int board_id = BM1684_BOARD_TYPE(bmdi);

	if (bmdi->cinfo.chip_id != 0x1682) {
		bm1684_get_board_type_by_id(bmdi, type, board_id);
		seq_printf(m, "%s\n", type);
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_board_type_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_board_type_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_board_type_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_board_type_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_board_version_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	char version[10] = "";
	struct bm_device_info *card_bmdi = bmctl_get_card_bmdi(bmdi);
	int board_version = 0x0;
	int board_id = 0x0;
	int pcb_version = 0x0;
	int bom_version = 0x0;

	if (card_bmdi != NULL)
		bmdi = card_bmdi;

	board_version = bmdi->misc_info.board_version;
	board_id = BM1684_BOARD_TYPE(bmdi);
	pcb_version = (board_version >> 4) & 0xf;
	bom_version = board_version & 0xf;

	if (bmdi->cinfo.chip_id != 0x1682) {
		bm1684_get_board_version_by_id(bmdi, version, board_id,
			pcb_version, bom_version);
		seq_printf(m, "%s\n", version);
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_board_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_board_version_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_board_version_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_board_version_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_bom_version_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_device_info *card_bmdi = bmctl_get_card_bmdi(bmdi);
	int board_version = 0x0;
	int bom_version = 0x0;

	if (card_bmdi != NULL)
		bmdi = card_bmdi;

	board_version = bmdi->misc_info.board_version;
	bom_version = board_version & 0xf;

	if (bmdi->cinfo.chip_id != 0x1682) {
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H ||
			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_P) {
			seq_printf(m, "V%d\n", bom_version);
		} else {
			seq_printf(m, "N/A\n");
		}

	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_bom_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_bom_version_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_bom_version_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_bom_version_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_pcb_version_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bm_device_info *card_bmdi = bmctl_get_card_bmdi(bmdi);
	int board_version = 0x0;
	int pcb_version = 0x0;

	if (card_bmdi != NULL)
		bmdi = card_bmdi;

	board_version = bmdi->misc_info.board_version;
	pcb_version = (board_version >> 4) & 0xf;

	if (bmdi->cinfo.chip_id != 0x1682) {
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H ||
			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_P) {
			seq_printf(m, "V%d\n", pcb_version);
		} else {
			seq_printf(m, "N/A\n");
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_pcb_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_pcb_version_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_pcb_version_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_pcb_version_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_boot_loader_version_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int value = 0x0;
	int boot_mode_mask = 0x1 << 6;

	value = top_reg_read(bmdi, 0x4);
	value = (value & boot_mode_mask) >> 0x6;

	if (bmdi->cinfo.chip_id != 0x1682) {
		if(bmdi->cinfo.version.bl2_version[0] == 'v') {
			if (value != 0)
				seq_printf(m, "BL1: %s\n", bmdi->cinfo.version.bl1_version);
			seq_printf(m, "BL2: %s\n", bmdi->cinfo.version.bl2_version);
		} else {
			if (bmdi->cinfo.boot_loader_version[0][0] == 0 ||
				bmdi->cinfo.boot_loader_version[1][0] == 0)
				bm1684_get_bootload_version(bmdi);
			seq_printf(m, "%s   %s\n", bmdi->cinfo.boot_loader_version[0],
					bmdi->cinfo.boot_loader_version[1]);
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_boot_loader_versions_proc_show(struct bm_device_info *bmdi, struct seq_file *m, void *v)
{
	if (bmdi->cinfo.chip_id != 0x1682) {
		if(bmdi->cinfo.version.bl2_version[0] == 'v') {
			seq_printf(m, "%s\n", bmdi->cinfo.version.bl2_version);
		} else {
			if (bmdi->cinfo.boot_loader_version[0][0] == 0 ||
				bmdi->cinfo.boot_loader_version[1][0] == 0)
				bm1684_get_bootload_version(bmdi);
			seq_printf(m, "%s   %s\n", bmdi->cinfo.boot_loader_version[0],
					bmdi->cinfo.boot_loader_version[1]);
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
        return 0;

}

static int bmdrv_boot_loader_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_boot_loader_version_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_boot_loader_version_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_boot_loader_version_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_calc_clk(int value)
{
	int refdiv = 0;
	int postdiv = 0;
	int postdiv2 = 0;
	int fbdiv = 0;

	refdiv = value & 0x1f;
	postdiv = (value >> 8) & 0x7;
	postdiv2 = (value >> 12) & 0x7;
	fbdiv = (value >> 16) & 0xfff;
	if (refdiv == 0x0 || postdiv == 0x0
		|| postdiv2 == 0x0 || fbdiv == 0x0)
		return -1;
	return 25*fbdiv/refdiv/(postdiv*postdiv2);

}

static int bmdrv_clk_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int mpll = 0;
	int tpll = 0;
	int vpll = 0;
	int fpll = 0;
	int dpll0 = 0;
	int dpll1 = 0;

	if (bmdi->cinfo.chip_id != 0x1682) {
		mpll = top_reg_read(bmdi, 0xe8);
		tpll = top_reg_read(bmdi, 0xec);
		fpll = top_reg_read(bmdi, 0xf0);
		vpll = top_reg_read(bmdi, 0xf4);
		dpll0 = top_reg_read(bmdi, 0xf8);
		dpll1 = top_reg_read(bmdi, 0xfc);
		seq_printf(m, "mpll: %d MHz\n", bmdrv_calc_clk(mpll));
		seq_printf(m, "tpll: %d MHz\n", bmdrv_calc_clk(tpll));
		seq_printf(m, "fpll: %d MHz\n", bmdrv_calc_clk(fpll));
		seq_printf(m, "vpll: %d MHz\n", bmdrv_calc_clk(vpll));
		seq_printf(m, "dpll0: %d MHz\n", bmdrv_calc_clk(dpll0));
		seq_printf(m, "dpll1: %d MHz\n", bmdrv_calc_clk(dpll1));
		seq_printf(m, "vpu: %d MHz\n", bmdrv_calc_clk(vpll));
		seq_printf(m, "jpu: %d MHz\n", bmdrv_calc_clk(vpll));
		seq_printf(m, "vpp: %d MHz\n", bmdrv_calc_clk(vpll));
		seq_printf(m, "tpu: %d MHz\n", bmdrv_calc_clk(tpll));
		seq_printf(m, "ddr: %d MHz\n", 4*bmdrv_calc_clk(dpll0));
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_clk_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_clk_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_clk_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_clk_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_driver_version_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	if (bmdi->cinfo.chip_id == BM1684_DEVICE_ID) {
		seq_printf(m, "release version:%d.%d.%d", BM_CHIP_VERSION,
			BM_MAJOR_VERSION, BM_MINOR_VERSION);
		seq_printf(m,"   release date: %s\n", BUILD_DATE);
	} else if (bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
		seq_printf(m, "release version:%d.%d.%d", BM_CHIP_VERSION,
			BM_MAJOR_VERSION, BM_MINOR_VERSION);
		seq_printf(m,"   release date: %s\n", BUILD_DATE);
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_driver_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_driver_version_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_driver_version_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_driver_version_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_versions_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int i, j;

	if (bmdi->cinfo.chip_id != 0x1682) {
		seq_printf(m, "driver_version: ");
		bmdrv_driver_version_proc_show(m, v);
		seq_printf(m, "board_version: ");
		bmdrv_board_version_proc_show(m, v);
		seq_printf(m, "board_type: ");
		bmdrv_board_type_proc_show(m, v);
		j = 0;
		for (i = bmdi->bmcd->dev_start_index; i < bmdi->bmcd->chip_num + bmdi->bmcd->dev_start_index; i++) {
			seq_printf(m, "card%d bmsophon%d boot_loader_version:", bmdi->bmcd->card_index, i);
			bmdrv_boot_loader_versions_proc_show(bmdi->bmcd->card_bmdi[j], m, v);
			seq_printf(m, "card%d bmsophon%d mcu_version:", bmdi->bmcd->card_index, i);
			bmdrv_mcu_versions_proc_show(bmdi->bmcd->card_bmdi[j], m, v);
			j++;
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_versions_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_versions_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_versions_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_versions_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_a53_enable_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int value = 0x0;
	int enable_a53_mask = 0x1;

	if (bmdi->cinfo.chip_id != 0x1682) {
		value = gpio_reg_read(bmdi, 0x50);
		if ((value & enable_a53_mask) == 0x1) {
			seq_printf(m, "disable\n");
		} else {
			seq_printf(m, "enable\n");
		}
		if (bmdi->cinfo.a53_enable == 0x0) {
			seq_printf(m, "boot_info a53_disable\n");
		} else {
			seq_printf(m, "boot_info a53_enable\n");
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_a53_enable_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_a53_enable_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_a53_enable_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_a53_enable_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_bmcpu_status_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) {
		if (bmdi->status_bmcpu == BMCPU_IDLE)
			seq_printf(m, "idle\n");
		else if (bmdi->status_bmcpu == BMCPU_RUNNING)
			seq_printf(m, "running\n");
		else
			seq_printf(m, "fault\n");
	} else {
		seq_printf(m, "unsupport\n");
	}

	return 0;
}

static int bmdrv_bmcpu_status_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_bmcpu_status_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_bmcpu_status_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_bmcpu_status_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_heap_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
 	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;
	int i = 0;

	if (bmdi->cinfo.chip_id != 0x1682) {
		for (i = 0; i < 3; i++) {
			seq_printf(m, "heap%d, size = 0x%llx, start_addr =0x%llx\n", i,
				resmem_info->npureserved_size[i], resmem_info->npureserved_addr[i]);
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_heap_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_heap_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_heap_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_heap_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_boot_mode_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int value = 0x0;
	int boot_mode_mask = 0x1 << 6;

	if (bmdi->cinfo.chip_id != 0x1682) {
		value = top_reg_read(bmdi, 0x4);
		value = (value & boot_mode_mask) >> 0x6;
		if (value == 0x0) {
			seq_printf(m, "rom_boot\n");
		} else {
			seq_printf(m, "spi_boot\n");
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_boot_mode_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_boot_mode_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_boot_mode_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_boot_mode_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_pmu_infos_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int vdd_tpu_voltage = 0, vdd_tpu_current = 0;
	int tpu_power = 0, pmu_tpu_temp = 0;
	int vdd_tpu_mem_voltage = 0, vdd_tpu_mem_current = 0;
	int tpu_mem_power = 0, pmu_tpu_mem_temp = 0;
	int vddc_voltage = 0, vddc_current = 0;
	int vddc_power = 0, pmu_vddc_temp = 0;
	struct bm_chip_attr *c_attr = NULL;

	c_attr = &bmdi->c_attr;
	if (bmdi->cinfo.chip_id != 0x1682) {

		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {

			mutex_lock(&c_attr->attr_mutex);
			bm_read_vdd_tpu_voltage(bmdi, &vdd_tpu_voltage);
			bm_read_vdd_tpu_current(bmdi, &vdd_tpu_current);
			bm_read_vdd_tpu_power(bmdi, &tpu_power);
			bm_read_vdd_pmu_tpu_temp(bmdi, &pmu_tpu_temp);
			mutex_unlock(&c_attr->attr_mutex);

			mutex_lock(&c_attr->attr_mutex);
			bm_read_vdd_tpu_mem_voltage(bmdi, &vdd_tpu_mem_voltage);
			bm_read_vdd_tpu_mem_current(bmdi, &vdd_tpu_mem_current);
			bm_read_vdd_tpu_mem_power(bmdi, &tpu_mem_power);
			bm_read_vdd_pmu_tpu_mem_temp(bmdi, &pmu_tpu_mem_temp);
			mutex_unlock(&c_attr->attr_mutex);

			mutex_lock(&c_attr->attr_mutex);
			bm_read_vddc_voltage(bmdi, &vddc_voltage);
			bm_read_vddc_current(bmdi, &vddc_current);
			bm_read_vddc_power(bmdi, &vddc_power);
			bm_read_vddc_pmu_vddc_temp(bmdi, &pmu_vddc_temp);
			mutex_unlock(&c_attr->attr_mutex);

			seq_printf(m, "-------------pmu pxc1331 infos ---------------\n");

			seq_printf(m, "vdd_tpu_voltage     %6dmV\tvdd_tpu_current      %6dmA\n",
					vdd_tpu_voltage, vdd_tpu_current);
			seq_printf(m, "vdd_tpu_power        %6dW\tvdd_pmu_tpu_temp      %6dC\n",
					tpu_power, pmu_tpu_temp);
			seq_printf(m, "vdd_tpu_mem_voltage %6dmV\tvdd_tpu_mem_current  %6dmA\n",
					vdd_tpu_mem_voltage, vdd_tpu_mem_current);
			seq_printf(m, "vdd_tpu_mem_power    %6dW\tvdd_pmu_tpu_mem_temp  %6dC\n",
					tpu_mem_power, pmu_tpu_mem_temp);
			seq_printf(m, "vddc_voltage        %6dmV\tvddc_current         %6dmA\n",
					vddc_voltage, vddc_current);
			seq_printf(m, "vddc_power           %6dW\tvddc_pmu_vddc_temp    %6dC\n",
					vddc_power, pmu_vddc_temp);
		} else {
			mutex_lock(&c_attr->attr_mutex);
			bm_read_vdd_tpu_voltage(bmdi, &vdd_tpu_voltage);
			bm_read_vdd_tpu_current(bmdi, &vdd_tpu_current);
			mutex_unlock(&c_attr->attr_mutex);

			mutex_lock(&c_attr->attr_mutex);
			bm_read_vddc_voltage(bmdi, &vddc_voltage);
			bm_read_vddc_current(bmdi, &vddc_current);
			mutex_unlock(&c_attr->attr_mutex);

			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB)
				seq_printf(m, "-------------68224 infos ---------------\n");
			else
				seq_printf(m, "-------------68127 infos ---------------\n");
			seq_printf(m, "vdd_tpu_voltage %6dmV\tvdd_tpu_current %6dmA\n",
					vdd_tpu_voltage, vdd_tpu_current);
			seq_printf(m, "vddc_voltage    %6dmV\tvddc_current    %6dmA\n",
					vddc_voltage, vddc_current);
		}
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_pmu_infos_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_pmu_infos_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_pmu_infos_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_pmu_infos_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_location_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int value = 0x0;

	if (bmdi->cinfo.chip_id != 0x1682) {
		if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
			value = gpio_reg_read(bmdi, 0x50);
			value = value >> 0x5;
			value &= 0xf;
		} else
			value = bmdi->dev_index;
		seq_printf(m, "location_%d, bmsophon_%d, card_index_%d\n", value, bmdi->dev_index, bmdi->bmcd->card_index);
	} else {
		seq_printf(m, "1682 not support\n");
	}
	return 0;
}

static int bmdrv_location_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_location_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_location_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_location_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};
#endif

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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_cdma_in_counter_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_cdma_out_counter_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_cdma_in_time_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_cdma_out_time_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_tpu_process_time_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "%lld us\n", bmdi->profile.tpu_process_time);
	return 0;
}

static int bmdrv_tpu_process_time_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_tpu_process_time_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_tpu_process_time_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_tpu_process_time_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_sent_api_counter_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_completed_api_counter_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
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
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_arm9_cache_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_ddr_config_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	u64 gbyte = 1024*1024*1024;

	seq_printf(m, "%lldg, %lldg, %lldg, %lldg, %lldg\n",  bmdi->boot_info.ddr_0a_size/gbyte,
			 bmdi->boot_info.ddr_0b_size/gbyte,  bmdi->boot_info.ddr_1_size/gbyte,
			 bmdi->boot_info.ddr_2_size/gbyte,
			 (bmdi->boot_info.ddr_0a_size + bmdi->boot_info.ddr_0b_size +
			 bmdi->boot_info.ddr_1_size +  bmdi->boot_info.ddr_2_size)/gbyte);

	return 0;
}

static int bmdrv_ddr_config_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_ddr_config_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_ddr_config_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_ddr_config_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

static int bmdrv_chip_num_on_card_proc_show(struct seq_file *m, void *v)
{
#ifndef SOC_MODE
	struct bm_device_info *bmdi = m->private;
	int chip_num = bmdi->bmcd->chip_num;
	seq_printf(m, "%d\n", chip_num);
#endif

	return 0;
}

static int bmdrv_chip_num_on_card_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdrv_chip_num_on_card_proc_show, PDE_DATA(inode));
}

static const struct BM_PROC_FILE_OPS bmdrv_chip_num_on_card_file_ops = {
	BM_PROC_OWNER		= BM_PROC_MODULE,
	BM_PROC_OPEN		= bmdrv_chip_num_on_card_proc_open,
	BM_PROC_READ		= seq_read,
	BM_PROC_LLSEEK		= seq_lseek,
	BM_PROC_RELEASE	= single_release,
};

int bmdrv_proc_file_init(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	char card_name[MAX_NAMELEN];
	char name[MAX_NAMELEN];
	if (bmdi->dev_index == 0) {
		proc_create_data("driver_version", 0444, bmsophon_proc_dir, &bmdrv_driver_version_file_ops,
											(void *)bmdi);
	}
	if (bmdi->bmcd->dev_start_index == bmdi->dev_index) {
		sprintf(card_name, "card%d", bmdi->bmcd->card_index);
		bmdi->card_proc_dir = proc_mkdir(card_name, bmsophon_proc_dir);
		if (!bmdi->card_proc_dir)
			return -ENOMEM;
		proc_create_data("chipid", 0444, bmdi->card_proc_dir, &bmdrv_chipid_file_ops,
			(void *)bmdi);
		proc_create_data("mode", 0444, bmdi->card_proc_dir, &bmdrv_mode_file_ops,
			(void *)bmdi);
		proc_create_data("chip_num_on_card", 0444, bmdi->card_proc_dir, &bmdrv_chip_num_on_card_file_ops,
			(void *)bmdi);
		proc_create_data("tpu_minclk", 0444, bmdi->card_proc_dir, &bmdrv_tpu_minclk_file_ops,
			(void *)bmdi);
		proc_create_data("tpu_maxclk", 0444, bmdi->card_proc_dir, &bmdrv_tpu_maxclk_file_ops,
			(void *)bmdi);
		proc_create_data("maxboardp", 0444, bmdi->card_proc_dir, &bmdrv_tpu_maxboardp_file_ops,
			(void *)bmdi);
		proc_create_data("board_power", 0444, bmdi->card_proc_dir, &bmdrv_board_power_file_ops,
			(void *)bmdi);
		proc_create_data("fan_speed", 0644, bmdi->card_proc_dir, &bmdrv_fan_speed_file_ops,
			(void *)bmdi);
		proc_create_data("board_temp", 0444, bmdi->card_proc_dir, &bmdrv_board_temp_file_ops,
			(void *)bmdi);
		proc_create_data("sn", 0444, bmdi->card_proc_dir, &bmdrv_board_sn_file_ops,
			(void *)bmdi);
		proc_create_data("board_type", 0444, bmdi->card_proc_dir, &bmdrv_board_type_file_ops,
			(void *)bmdi);
		proc_create_data("board_version", 0444, bmdi->card_proc_dir, &bmdrv_board_version_file_ops,
			(void *)bmdi);
		proc_create_data("versions", 0444, bmdi->card_proc_dir, &bmdrv_versions_file_ops,
			(void *)bmdi);
		proc_create_data("pcb_version", 0444, bmdi->card_proc_dir, &bmdrv_pcb_version_file_ops,
			(void *)bmdi);
		proc_create_data("bom_version", 0444, bmdi->card_proc_dir, &bmdrv_bom_version_file_ops,
			(void *)bmdi);
		if (bmdi->bmcd->chip_num == 8) {
			proc_create_data("mcu_version", 0444, bmdi->card_proc_dir, &bmdrv_mcu_version_file_ops,
			(void *)bmdi);
		}
	}
	sprintf(name, "bmsophon%d", bmdi->dev_index);
	bmdi->proc_dir = proc_mkdir(name, bmdi->bmcd->first_probe_bmdi->card_proc_dir);
	if (!bmdi->proc_dir)
		return -ENOMEM;
	proc_create_data("tpuid", 0444, bmdi->proc_dir, &bmdrv_tpuid_file_ops,
		(void *)bmdi);
	proc_create_data("dbdf", 0444, bmdi->proc_dir, &bmdrv_dbdf_file_ops,
		(void *)bmdi);
	proc_create_data("status", 0444, bmdi->proc_dir, &bmdrv_status_file_ops,
		(void *)bmdi);
	proc_create_data("ecc", 0444, bmdi->proc_dir, &bmdrv_ecc_file_ops,
		(void *)bmdi);
	proc_create_data("jpu", 0644, bmdi->proc_dir, &bmdrv_jpu_file_ops,
		(void *)bmdi);
	proc_create_data("media", 0644, bmdi->proc_dir, &bmdrv_vpu_file_ops,
		(void *)bmdi);
	proc_create_data("dynfreq", 0644, bmdi->proc_dir, &bmdrv_dynfreq_file_ops,
		(void *)bmdi);
	proc_create_data("dumpreg", 0644, bmdi->proc_dir, &bmdrv_dumpreg_file_ops,
		(void *)bmdi);
	proc_create_data("pcie_link_speed", 0444, bmdi->proc_dir, &bmdrv_pcie_link_speed_file_ops,
		(void *)bmdi);
	proc_create_data("pcie_link_width", 0444, bmdi->proc_dir, &bmdrv_pcie_link_width_file_ops,
		(void *)bmdi);
	proc_create_data("pcie_cap_speed", 0444, bmdi->proc_dir, &bmdrv_pcie_cap_speed_file_ops,
		(void *)bmdi);
	proc_create_data("pcie_cap_width", 0444, bmdi->proc_dir, &bmdrv_pcie_cap_width_file_ops,
		(void *)bmdi);
	proc_create_data("pcie_region", 0444, bmdi->proc_dir, &bmdrv_pcie_region_file_ops,
		(void *)bmdi);
	proc_create_data("tpu_power", 0444, bmdi->proc_dir, &bmdrv_tpu_power_file_ops,
		(void *)bmdi);
	proc_create_data("firmware_info", 0444, bmdi->proc_dir, &bmdrv_firmware_file_ops,
			(void *)bmdi);
	proc_create_data("tpu_cur", 0444, bmdi->proc_dir, &bmdrv_tpu_cur_file_ops,
		(void *)bmdi);
	proc_create_data("tpu_volt", 0444, bmdi->proc_dir, &bmdrv_tpu_volt_file_ops,
		(void *)bmdi);
	proc_create_data("tpu_freq", 0444, bmdi->proc_dir, &bmdrv_tpu_freq_file_ops,
		(void *)bmdi);
	proc_create_data("chip_temp", 0444, bmdi->proc_dir, &bmdrv_chip_temp_file_ops,
		(void *)bmdi);
	proc_create_data("vddc_volt", 0444, bmdi->proc_dir, &bmdrv_vddc_volt_file_ops,
		(void *)bmdi);
	proc_create_data("vddc_power", 0444, bmdi->proc_dir, &bmdrv_vddc_power_file_ops,
		(void *)bmdi);
	proc_create_data("vddphy_power", 0444, bmdi->proc_dir, &bmdrv_vddphy_power_file_ops,
		(void *)bmdi);
	proc_create_data("chip_power", 0444, bmdi->proc_dir, &bmdrv_chip_power_file_ops,
		(void *)bmdi);
	if (bmdi->bmcd->chip_num != 8) {
		proc_create_data("mcu_version", 0444, bmdi->proc_dir, &bmdrv_mcu_version_file_ops,
		(void *)bmdi);
	}
	proc_create_data("boot_loader_version", 0444, bmdi->proc_dir, &bmdrv_boot_loader_version_file_ops,
		(void *)bmdi);
	proc_create_data("clk", 0444, bmdi->proc_dir, &bmdrv_clk_file_ops,
		(void *)bmdi);
	proc_create_data("pmu_infos", 0444, bmdi->proc_dir, &bmdrv_pmu_infos_file_ops,
		(void *)bmdi);
	proc_create_data("boot_mode", 0444, bmdi->proc_dir, &bmdrv_boot_mode_file_ops,
		(void *)bmdi);
	proc_create_data("a53_enable", 0444, bmdi->proc_dir, &bmdrv_a53_enable_file_ops,
		(void *)bmdi);
	proc_create_data("bmcpu_status", 0444, bmdi->proc_dir, &bmdrv_bmcpu_status_file_ops,
		(void *)bmdi);
	proc_create_data("heap", 0444, bmdi->proc_dir, &bmdrv_heap_file_ops,
		(void *)bmdi);
	proc_create_data("location", 0444, bmdi->proc_dir, &bmdrv_location_file_ops,
		(void *)bmdi);

	proc_create_data("cdma_in_time", 0444, bmdi->proc_dir, &bmdrv_cdma_in_time_file_ops,
		(void *)bmdi);
	proc_create_data("cdma_in_counter", 0444, bmdi->proc_dir, &bmdrv_cdma_in_counter_file_ops,
		(void *)bmdi);
	proc_create_data("cdma_out_time", 0444, bmdi->proc_dir,	&bmdrv_cdma_out_time_file_ops,
		(void *)bmdi);
	proc_create_data("cdma_out_counter", 0444, bmdi->proc_dir, &bmdrv_cdma_out_counter_file_ops,
		(void *)bmdi);
	proc_create_data("tpu_process_time", 0444, bmdi->proc_dir, &bmdrv_tpu_process_time_file_ops,
		(void *)bmdi);
	proc_create_data("sent_api_counter", 0444, bmdi->proc_dir, &bmdrv_sent_api_counter_file_ops,
		(void *)bmdi);
	proc_create_data("completed_api_counter", 0444, bmdi->proc_dir,	&bmdrv_completed_api_counter_file_ops,
		(void *)bmdi);
	proc_create_data("arm9_cache", 0444, bmdi->proc_dir, &bmdrv_arm9_cache_file_ops,
		(void *)bmdi);
	proc_create_data("ddr_capacity", 0444, bmdi->proc_dir, &bmdrv_ddr_config_file_ops,
		(void *)bmdi);
#endif
	return 0;
}

void bmdrv_proc_file_deinit(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	static struct proc_dir_entry *bmsophon_card_proc_dir;

	remove_proc_entry("tpuid", bmdi->proc_dir);

	remove_proc_entry("dbdf", bmdi->proc_dir);
	remove_proc_entry("status", bmdi->proc_dir);
	remove_proc_entry("ecc", bmdi->proc_dir);
	remove_proc_entry("jpu", bmdi->proc_dir);
	remove_proc_entry("media", bmdi->proc_dir);
	remove_proc_entry("dynfreq", bmdi->proc_dir);
	remove_proc_entry("dumpreg", bmdi->proc_dir);
	remove_proc_entry("pcie_link_speed", bmdi->proc_dir);
	remove_proc_entry("pcie_link_width", bmdi->proc_dir);
	remove_proc_entry("pcie_cap_speed", bmdi->proc_dir);
	remove_proc_entry("pcie_cap_width", bmdi->proc_dir);
	remove_proc_entry("pcie_region", bmdi->proc_dir);
	remove_proc_entry("tpu_power", bmdi->proc_dir);
	remove_proc_entry("firmware_info", bmdi->proc_dir);
	remove_proc_entry("tpu_cur", bmdi->proc_dir);
	remove_proc_entry("tpu_volt", bmdi->proc_dir);
	remove_proc_entry("tpu_freq", bmdi->proc_dir);
	remove_proc_entry("chip_temp", bmdi->proc_dir);
	remove_proc_entry("vddc_power", bmdi->proc_dir);
	remove_proc_entry("vddphy_power", bmdi->proc_dir);
	remove_proc_entry("chip_power", bmdi->proc_dir);
	remove_proc_entry("boot_loader_version", bmdi->proc_dir);
	remove_proc_entry("clk", bmdi->proc_dir);
	remove_proc_entry("pmu_infos", bmdi->proc_dir);
	remove_proc_entry("boot_mode", bmdi->proc_dir);
	remove_proc_entry("a53_enable", bmdi->proc_dir);
	remove_proc_entry("bmcpu_status", bmdi->proc_dir);
	remove_proc_entry("heap", bmdi->proc_dir);
	remove_proc_entry("location", bmdi->proc_dir);

	remove_proc_entry("cdma_in_time", bmdi->proc_dir);
	remove_proc_entry("cdma_in_counter", bmdi->proc_dir);
	remove_proc_entry("cdma_out_time", bmdi->proc_dir);
	remove_proc_entry("cdma_out_counter", bmdi->proc_dir);
	remove_proc_entry("tpu_process_time", bmdi->proc_dir);
	remove_proc_entry("sent_api_counter", bmdi->proc_dir);
	remove_proc_entry("completed_api_counter", bmdi->proc_dir);
	remove_proc_entry("arm9_cache", bmdi->proc_dir);
	remove_proc_entry("ddr_capacity", bmdi->proc_dir);
	proc_remove(bmdi->proc_dir);
	if (bmdi->dev_index == bmdi->bmcd->dev_start_index) {
		bmsophon_card_proc_dir = bmdi->card_proc_dir;
		remove_proc_entry("chipid", bmsophon_card_proc_dir);
		remove_proc_entry("mode", bmsophon_card_proc_dir);
		remove_proc_entry("chip_num_on_card", bmsophon_card_proc_dir);

		remove_proc_entry("tpu_minclk", bmsophon_card_proc_dir);
		remove_proc_entry("tpu_maxclk", bmsophon_card_proc_dir);
		remove_proc_entry("maxboardp", bmsophon_card_proc_dir);
		remove_proc_entry("fan_speed", bmsophon_card_proc_dir);
		remove_proc_entry("board_power", bmsophon_card_proc_dir);
		remove_proc_entry("board_temp", bmsophon_card_proc_dir);
		remove_proc_entry("sn", bmsophon_card_proc_dir);
		remove_proc_entry("board_type", bmsophon_card_proc_dir);
		remove_proc_entry("board_version", bmsophon_card_proc_dir);
		remove_proc_entry("versions", bmsophon_card_proc_dir);
		remove_proc_entry("pcb_version", bmsophon_card_proc_dir);
		remove_proc_entry("bom_version", bmsophon_card_proc_dir);
		if (bmdi->bmcd->chip_num == 8) {
			remove_proc_entry("mcu_version", bmsophon_card_proc_dir);
		}

		proc_remove(bmsophon_card_proc_dir);
	}
	if (bmdi->dev_index == 0)
		remove_proc_entry("driver_version", bmsophon_proc_dir);
#endif
}

#undef MAX_NAMELEN

bool bm_arm9fw_log_buffer_empty(struct bm_device_info *bmdi)
{
	int read_index = 0;
	int write_index = 0;

	read_index = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_RP);
	write_index = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_WP);
//	PR_TRACE("read_index = 0x%x, write_index = 0x%x\n", read_index, write_index);
	return read_index == write_index;

}

int bm_get_arm9fw_log_from_device(struct bm_device_info *bmdi)
{
	int read_p = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_RP);
	int write_p = gp_reg_read_enh(bmdi, GP_REG_ARM9FW_LOG_WP);
	int arm9fw_buffer_size = bmdi->monitor_thread_info.log_mem.device_size;
	int host_size = bmdi->monitor_thread_info.log_mem.host_size;
	int size = 0;
	u64 device_paddr = bmdi->monitor_thread_info.log_mem.device_paddr;
	u64 host_paddr = bmdi->monitor_thread_info.log_mem.host_paddr;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	bm_cdma_arg cdma_arg;

	PR_TRACE("wp = %d, rp = %d, device_paddr = %llx\n", write_p, read_p, device_paddr);
	if (write_p < read_p) {
		size = arm9fw_buffer_size - read_p;

		if (size > host_size) {
			size =  host_size;
			bmdev_construct_cdma_arg(&cdma_arg, device_paddr + read_p,
				 host_paddr & 0xffffffffff, size, CHIP2HOST, false, false);
			if (memcpy_info->bm_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
				pr_err("bm-sophon%d get arm9 log failed\n", bmdi->dev_index);
				return 0;
			}
			read_p = read_p + size;

		} else {
			size = arm9fw_buffer_size - read_p;
			bmdev_construct_cdma_arg(&cdma_arg, device_paddr + read_p,
				host_paddr & 0xffffffffff, size, CHIP2HOST, false, false);
			if (memcpy_info->bm_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
				pr_err("bm-sophon%d get arm9 log failed\n", bmdi->dev_index);
				return 0;
			}
			read_p = 0;
		}
	} else {
		size = write_p - read_p;
		if (size >= host_size)
			size = host_size;

		bmdev_construct_cdma_arg(&cdma_arg, device_paddr + read_p,
			host_paddr & 0xffffffffff, size, CHIP2HOST, false, false);
		if (memcpy_info->bm_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
				pr_err("bm-sophon%d get arm9 log failed\n", bmdi->dev_index);
			return 0;
		}
		read_p = read_p + size;

	}
	gp_reg_write_enh(bmdi, GP_REG_ARM9FW_LOG_RP, read_p);
	PR_TRACE("size = 0x%x\n", size);
	return size;
}

#define ARM9FW_LOG_HOST_BUFFER_SIZE (1024 * 512)
#define ARM9FW_LOG_DEVICE_BUFFER_SIZE (1024 * 1024 * 4)
#define ARM9FW_LOG_LINE_SIZE 512

void bm_print_arm9fw_log(struct bm_device_info *bmdi, int size)
{
	char str[ARM9FW_LOG_LINE_SIZE] = "";
	int i = 0;
	char *p = bmdi->monitor_thread_info.log_mem.host_vaddr;

	for (i = 0; i < size/ARM9FW_LOG_LINE_SIZE; i++) {
		strncpy(str, p, ARM9FW_LOG_LINE_SIZE - 1);
		pr_info("bm-sophon%d ARM9_LOG: %s", bmdi->dev_index, str);
		p += ARM9FW_LOG_LINE_SIZE;
	}
	memset(bmdi->monitor_thread_info.log_mem.host_vaddr, 0, size);
}

int bm_arm9fw_log_init(struct bm_device_info *bmdi)
{
	int ret = 0;

	bmdi->monitor_thread_info.log_mem.host_size = ARM9FW_LOG_HOST_BUFFER_SIZE;
	if (bmdi->cinfo.chip_id == 0x1686)
		bmdi->monitor_thread_info.log_mem.device_paddr = bmdi->gmem_info.resmem_info.armreserved_addr + (bmdi->gmem_info.resmem_info.armreserved_size - ARM9FW_LOG_DEVICE_BUFFER_SIZE);
	else
		bmdi->monitor_thread_info.log_mem.device_paddr = bmdi->gmem_info.resmem_info.armfw_addr + (bmdi->gmem_info.resmem_info.armfw_size - ARM9FW_LOG_DEVICE_BUFFER_SIZE);
	bmdi->monitor_thread_info.log_mem.device_size = ARM9FW_LOG_DEVICE_BUFFER_SIZE;
	ret = bmdrv_stagemem_alloc(bmdi, bmdi->monitor_thread_info.log_mem.host_size,
			&bmdi->monitor_thread_info.log_mem.host_paddr,
			&bmdi->monitor_thread_info.log_mem.host_vaddr);
	if (ret) {
		pr_err("bm-sophon%d alloc arm9fw log buffer failed\n", bmdi->dev_index);
		return ret;
	}

	memset(bmdi->monitor_thread_info.log_mem.host_vaddr, 0,
			bmdi->monitor_thread_info.log_mem.host_size);

	gp_reg_write_enh(bmdi, GP_REG_ARM9FW_LOG_RP, 0);
	PR_TRACE("host size = 0x%x, device_addr = 0x%llx, device size = 0x%x\n",
		bmdi->monitor_thread_info.log_mem.host_size, bmdi->monitor_thread_info.log_mem.device_paddr,
		bmdi->monitor_thread_info.log_mem.device_size);

	return ret;
}

#ifndef SOC_MODE
void bm_i2c2_reset(struct bm_device_info *bmdi)
{
	u32 value = 0;
        struct bm_chip_attr *c_attr = &bmdi->c_attr;

	mutex_lock(&c_attr->attr_mutex);
	/* reset i2c2 by software */
	value = top_reg_read(bmdi, 0xc00);
	value &= ~(1 << 28);
	top_reg_write(bmdi, 0xc00 , value);
	mdelay(1);
	value = top_reg_read(bmdi, 0xc00);
	value |= 1 << 28;
	top_reg_write(bmdi, 0xc00 , value);
	mutex_unlock(&c_attr->attr_mutex);
}

void bm_i2c2_recovey(struct bm_device_info *bmdi) {
	u32 i2c_index = 0x2;
	u32 addr = i2c_reg_read(bmdi, i2c_index, 0x8);
	u32 intc_mask = i2c_reg_read(bmdi, i2c_index, 0x30);
	u32 speed = i2c_reg_read(bmdi, i2c_index, 0x0);
	u32 rx_tl = i2c_reg_read(bmdi, i2c_index, 0x38);
	u32 tx_tl = i2c_reg_read(bmdi, i2c_index, 0x3c);
	u32 fs_spklen = i2c_reg_read(bmdi, i2c_index, 0xa0);
	u32 hs_spklen =  i2c_reg_read(bmdi, i2c_index, 0xa4);

	bm_i2c2_reset(bmdi);

	i2c_reg_write(bmdi, i2c_index, 0x6c, 0);
	i2c_reg_write(bmdi, i2c_index, 0x8, addr);
	i2c_reg_write(bmdi, i2c_index, 0x30, intc_mask);
	i2c_reg_write(bmdi, i2c_index, 0x0, speed);
	i2c_reg_write(bmdi, i2c_index, 0x38, rx_tl);
	i2c_reg_write(bmdi, i2c_index, 0x3c, tx_tl);
	i2c_reg_write(bmdi, i2c_index, 0xa0, fs_spklen);
	i2c_reg_write(bmdi, i2c_index, 0xa4, hs_spklen);
	i2c_reg_write(bmdi, i2c_index, 0x6c, 1);
}

int is_i2c2_idle(struct bm_device_info *bmdi) {
	int i = 0x0;
	u32 i2c2_status = 0;
	u32 i2c_index = 0x2;
	for (i = 0; i < 300; i++) {
                i2c2_status = i2c_reg_read(bmdi, i2c_index, 0x70);
                i2c2_status = (i2c2_status >> 0x6) & 0x1;
                if (i2c2_status == 0x0)
                        return 1;
                udelay(100);
        }

	return 0;
}

int i2c2_disable(struct bm_device_info *bmdi) {
	int i2c_index = 0x2;
	bm_i2c2_reset(bmdi);
	i2c_reg_write(bmdi, i2c_index, 0x6c, 0);
	return 0;
}

int i2c2_deinit(struct bm_device_info *bmdi) {
	if (is_i2c2_idle(bmdi)) {
		i2c2_disable(bmdi);
	} else {
		i2c2_disable(bmdi); //force disbale
	}
	return 0;
}

int bm_i2c2_need_recovery(struct bm_device_info *bmdi) {
	u32 i2c2_status = 0;
	u32 i2c2_count_old = 0;
	u32 i2c2_count_new = 0;
	int i = 0;
	u32 i2c_index = 0x2;

	i2c2_count_old = gp_reg_read_enh(bmdi, GP_REG_I2C2_IRQ_COUNT);

	for (i = 0; i < 300; i++) {
		i2c2_status = i2c_reg_read(bmdi, i2c_index, 0x70);
		i2c2_status = (i2c2_status >> 0x6) & 0x1;
		if (i2c2_status == 0x0)
			return 0;
		udelay(100);
	}

	i2c2_count_new = gp_reg_read_enh(bmdi, GP_REG_I2C2_IRQ_COUNT);

	if (i2c2_count_old == i2c2_count_new)
		return 1;
	else
		return 0;

}
#endif

void bm_dump_arm9fw_log(struct bm_device_info *bmdi, int count)
{
	int size = 0;
	long start=0;
	long end =0;
	int delt = 0;

	start = jiffies;
	bm_npu_utilization_stat(bmdi);
	if(count % 3 == 0) {
		if (!bm_arm9fw_log_buffer_empty(bmdi)) {
			size = bm_get_arm9fw_log_from_device(bmdi);
			bm_print_arm9fw_log(bmdi, size);
			end  = jiffies;
			delt = jiffies_to_msecs(end-start);
			//PR_TRACE("dev_index=%d,bm_dump_arm9fw_log time is %d ms\n",bmdi->dev_index, delt);
			msleep_interruptible(((10-delt)>0)? 10-delt : 0);
		}
	} else if (count % 10 == 0x0) {
#ifndef SOC_MODE
		if (bm_i2c2_need_recovery(bmdi)) {
			bm_i2c2_recovey(bmdi);
			pr_info("sophon%d recovery i2c2\n", bmdi->dev_index);
		}
#endif
		end  = jiffies;
		delt = jiffies_to_msecs(end-start);
		//PR_TRACE("dev_index=%d,bm_dump_arm9fw_log time is %d ms\n",bmdi->dev_index, delt);
		msleep_interruptible(((10-delt)>0)? 10-delt : 0);
	} else {
		msleep_interruptible(10);
		//PR_TRACE("buffer is empty\n");
	}
}

char *bmdrv_get_error_string(int error){
   int er = abs(error);
   switch(er){
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
