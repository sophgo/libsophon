#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/irqreturn.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif

#include "bm_common.h"
#include "bm_memcpy.h"
#include "bm_irq.h"
#include "bm_gmem.h"
#include "bm1684_jpu.h"

static DEFINE_MUTEX(s_jpu_proc_lock);
static int dbg_enable;

#define dprintk(args...) \
	do { \
		if (dbg_enable) \
			pr_info(args); \
	} while (0)

#ifndef VM_RESERVED /* for kernel up to 3.7.0 version */
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static int jpu_reg_base[MAX_NUM_JPU_CORE] = {
	JPU_CORE0_REG,
	JPU_CORE1_REG,
	JPU_CORE2_REG,
	JPU_CORE3_REG,
};

static int jpu_reset[MAX_NUM_JPU_CORE] = {
	JPU_CORE0_RST,
	JPU_CORE1_RST,
	JPU_CORE2_RST,
	JPU_CORE3_RST,
};

static int jpu_irq[MAX_NUM_JPU_CORE] = {
	JPU_CORE0_IRQ,
	JPU_CORE1_IRQ,
	JPU_CORE2_IRQ,
	JPU_CORE3_IRQ,
};

static jpudrv_register_info_t bm_jpu_en_apb_clk[MAX_NUM_JPU_CORE] = {
	{JPU_TOP_CLK_EN_REG1, CLK_EN_APB_VD0_JPEG0_GEN_REG1},
	{JPU_TOP_CLK_EN_REG1, CLK_EN_APB_VD0_JPEG1_GEN_REG1},
	{JPU_TOP_CLK_EN_REG2, CLK_EN_APB_VD1_JPEG0_GEN_REG2},
	{JPU_TOP_CLK_EN_REG2, CLK_EN_APB_VD1_JPEG1_GEN_REG2},
};

static int jpu_reg_base_1684x[] = {
	JPU_CORE0_REG,
	JPU_CORE2_REG,
};

static int jpu_reset_1684x[] = {
	JPU_CORE0_RST,
	JPU_CORE2_RST,
};

static int jpu_irq_1684x[] = {
	JPU_CORE0_IRQ,
	JPU_CORE2_IRQ,
};

static jpudrv_register_info_t bm_jpu_en_apb_clk_1684x[] = {
	{JPU_TOP_CLK_EN_REG1, CLK_EN_APB_VD0_JPEG0_GEN_REG1},
	{JPU_TOP_CLK_EN_REG2, CLK_EN_APB_VD1_JPEG0_GEN_REG2},
};




static unsigned int get_max_num_jpu_core(struct bm_device_info *bmdi) {
	if(bmdi->cinfo.chip_id == 0x1686) {
            return MAX_NUM_JPU_CORE_BM1686;
	}
	return MAX_NUM_JPU_CORE;
}

static int jpu_core_reset(struct bm_device_info *bmdi, int core)
{
	int val = 0;
    int offset = bmdi->cinfo.chip_id == 0x1686 ? jpu_reset_1684x[core] : jpu_reset[core];

	val = bm_read32(bmdi, JPU_RST_REG);
    val &= ~(1 << offset);

	bm_write32(bmdi, JPU_RST_REG, val);
	udelay(10);

    val |= (1 << offset);
	bm_write32(bmdi, JPU_RST_REG, val);

	return 0;
}

static int jpu_cores_reset(struct bm_device_info *bmdi)
{
	int core_idx;

	mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
	for (core_idx = 0; core_idx < get_max_num_jpu_core(bmdi); core_idx++)
		jpu_core_reset(bmdi, core_idx);
	mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);

	return 0;
}

static int jpu_top_remap(struct bm_device_info *bmdi,
		unsigned int core_idx,
		unsigned long read_addr,
		unsigned long write_addr)
{
	int val, read_high, write_high, shift_w, shift_r;

	read_high = read_addr >> 32;
	write_high = write_addr >> 32;

    if (core_idx >= get_max_num_jpu_core(bmdi)) {
		pr_err("jpu top remap core_idx :%d error\n", core_idx);
		return -1;
	}

    if (bmdi->cinfo.chip_id == 0x1686) {
        shift_w = core_idx << 4;
	    shift_r = (core_idx << 4) + 4;
	} else {
	    shift_w = core_idx << 3;
	    shift_r = (core_idx << 3) + 4;
	}

	val = bm_read32(bmdi, JPU_CONTROL_REG);
	val &= ~(7 << shift_w);
	val &= ~(7 << shift_r);
	val |= read_high << shift_r;
	val |= write_high << shift_w;
	bm_write32(bmdi, JPU_CONTROL_REG, val);

	return 0;
}

int bm_jpu_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int bm_jpu_release(struct inode *inode, struct file *filp)
{
	jpudrv_instance_list_t *jil, *n;
	struct bm_device_info *bmdi;

	bmdi = (struct bm_device_info *)filp->private_data;

	mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
	list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list) {
		if (jil->filp == filp) {
			if (jil->inuse == 1) {
				jil->inuse = 0;
				mdelay(100);
				jpu_core_reset(bmdi, jil->core_idx);
				up(&bmdi->jpudrvctx.jpu_sem);
			}
			jil->filp = NULL;
		}
	}
	mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);

	return 0;
}

long bm_jpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	switch (cmd) {
	case JDI_IOCTL_GET_INSTANCE_CORE_INDEX: {
		jpudrv_instance_list_t *jil, *n;
		jpudrv_remap_info_t  info;
		int inuse = -1, core_idx = -1;

		ret = copy_from_user(&info, (jpudrv_remap_info_t *)arg, sizeof(jpudrv_remap_info_t));
		if (ret) {
			pr_err("copy_from_user failed\n");
			ret = -EFAULT;
			break;
		}
		ret = down_interruptible(&bmdi->jpudrvctx.jpu_sem);
		if (!ret) {
			mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
			list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list) {
				if (!jil->inuse) {
					jil->inuse = 1;
					jil->filp = filp;
					inuse = 1;
					core_idx = jil->core_idx;
					jpu_top_remap(bmdi, core_idx, info.read_addr, info.write_addr);
					bmdi->jpudrvctx.core[core_idx]++;
					bmdi->jpudrvctx.s_jpu_usage_info.jpu_open_status[core_idx] = 1;
					list_del(&jil->list);
					list_add_tail(&jil->list, &bmdi->jpudrvctx.inst_head);
					dprintk("[jpudrv]:core_idx=%d, filp=%p\n", (int)jil->core_idx, filp);
					break;
				}
				dprintk("[jpudrv]:jil->inuse == 1,filp=%p\n", filp);
			}
			mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);

			if (inuse == 1) {
				info.core_idx = core_idx;
				ret = copy_to_user((void __user *)arg, &info, sizeof(jpudrv_remap_info_t));
				if (ret)
					ret = -EFAULT;
			}
		}

		if (signal_pending(current)) {
			pr_err("down_interruptible ret=%d\n", ret);
			ret = -ERESTARTSYS;
			break;
		}
		break;
	}

	case JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX: {
		u32 core_idx;
		jpudrv_instance_list_t *jil, *n;

		if (get_user(core_idx, (u32 __user *)arg))
			return -EFAULT;

		ret = mutex_lock_interruptible(&bmdi->jpudrvctx.jpu_core_lock);
		if (!ret) {
			list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list) {
				if (jil->core_idx == core_idx && jil->filp == filp) {
					jil->inuse = 0;
					bmdi->jpudrvctx.s_jpu_usage_info.jpu_open_status[core_idx] = 0;
					dprintk("[jpudrv]:core_idx=%d,filp=%p\n", core_idx, filp);
					break;
				}
			}
			up(&bmdi->jpudrvctx.jpu_sem);
			mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);
		}

		if (signal_pending(current)) {
			pr_err("mutex_lock interruptible ret=%d\n", ret);
			ret = -ERESTARTSYS;
			break;
		}
		break;
	}

	case JDI_IOCTL_WAIT_INTERRUPT: {
		jpudrv_intr_info_t  info;

		ret = copy_from_user(&info, (jpudrv_intr_info_t *)arg, sizeof(jpudrv_intr_info_t));
		if (ret != 0)
			return -EFAULT;

		atomic_inc(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx]);
		if (!wait_event_interruptible_timeout(bmdi->jpudrvctx.interrupt_wait_q[info.core_idx],
					bmdi->jpudrvctx.interrupt_flag[info.core_idx] != 0,
					msecs_to_jiffies(info.timeout))) {
			pr_err("[jpudrv]:jpu wait_event_interruptible timeout,current->pid %d,\
				current->tgid %d, dev_index  %d\n",current->pid, current->tgid,bmdi->dev_index);
			ret = -ETIME;
			atomic_dec(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx]);
			break;
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			atomic_dec(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx]);
			break;
		}

		atomic_dec(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[info.core_idx]);
		bmdi->jpudrvctx.interrupt_flag[info.core_idx] = 0;
		break;
	}

	case JDI_IOCTL_RESET: {
		u32 core_idx;

		if (get_user(core_idx, (u32 __user *)arg))
			return -EFAULT;

		mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
		jpu_core_reset(bmdi, core_idx);
		mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);
		break;
	}

	case JDI_IOCTL_GET_REGISTER_INFO: {
		ret = copy_to_user((void __user *)arg, &bmdi->jpudrvctx.jpu_register,
				sizeof(jpudrv_buffer_t)*get_max_num_jpu_core(bmdi));
		if (ret != 0)
			ret = -EFAULT;
		dprintk("[jpudrv]:JDI_IOCTL_GET_REGISTER_INFO: phys_addr==0x%lx, virt_addr=0x%lx, size=%d\n",
				bmdi->jpudrvctx.jpu_register[0].phys_addr,
				bmdi->jpudrvctx.jpu_register[0].virt_addr,
				bmdi->jpudrvctx.jpu_register[0].size);
		break;
	}

	case JDI_IOCTL_WRITE_VMEM: {
		struct dma_params {
			u64 src;
			u64 dst;
			size_t size;
		} dma_param;

		if (!bmdi)
			return -EFAULT;

		ret = copy_from_user(&dma_param, (void __user *)arg, sizeof(dma_param));
		if (ret != 0)
			return -EFAULT;
		ret = bmdev_memcpy_s2d(bmdi, NULL, dma_param.dst, (void __user *)dma_param.src,
				dma_param.size, true, KERNEL_NOT_USE_IOMMU);
		if (ret) {
			pr_err("[jpudrv]:JDI_IOCTL_WRITE_MEM failed\n");
			return -EFAULT;
		}
		break;
	}

	case JDI_IOCTL_READ_VMEM: {
		struct dma_params {
			u64 src;
			u64 dst;
			size_t size;
		} dma_param;

		if (!bmdi)
			return -EFAULT;
		ret = copy_from_user(&dma_param, (void __user *)arg, sizeof(dma_param));
		if (ret != 0)
			return -EFAULT;

		ret = bmdev_memcpy_d2s(bmdi, NULL, (void __user *)dma_param.dst, dma_param.src,
				dma_param.size, true, KERNEL_NOT_USE_IOMMU);
		if (ret != 0) {
			pr_err("[jpudrv]:JDI_IOCTL_READ_MEM failed\n");
			return -EFAULT;
		}
		break;
	}
	case  JDI_IOCTL_GET_MAX_NUM_JPU_CORE: {

		int max_num_jpu_core = get_max_num_jpu_core(bmdi);
		ret = copy_to_user((void __user *)arg, &max_num_jpu_core, sizeof(int));
		if (ret != 0)
			ret = -EFAULT;
		dprintk("[jpudrv]:JDI_IOCTL_GET_MAX_NUM_JPU_CORE: max_num_jpu_core=%d\n", max_num_jpu_core);
		break;
	}
	default:
		pr_err("[jpudrv]:No such ioctl, cmd is %d\n", cmd);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int jpu_map_to_register(struct file *filp, struct vm_area_struct *vm, int core_idx)
{
	unsigned long pfn;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	vm->vm_flags |= VM_IO | VM_RESERVED;
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn = bmdi->jpudrvctx.jpu_register[core_idx].phys_addr >> PAGE_SHIFT;

	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

int bm_jpu_mmap(struct file *filp, struct vm_area_struct *vm)
{
	int i = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		if (vm->vm_pgoff == (bmdi->jpudrvctx.jpu_register[i].phys_addr >> PAGE_SHIFT)) {
			dprintk("jpu core %d,vm->vm_pgoff = 0x%lx\n", i, bmdi->jpudrvctx.jpu_register[i].phys_addr);
			return jpu_map_to_register(filp, vm, i);
		}
	}

	return -1;
}

static irqreturn_t bm_jpu_irq_handler(struct bm_device_info *bmdi)
{
	int core = 0;
	int irq =  bmdi->cinfo.irq_id;

	dprintk("[jpudrv]:irq handler card :%d irq :%d\n", bmdi->dev_index, irq);
	for (core = 0; core < get_max_num_jpu_core(bmdi); core++) {
		if (bmdi->jpudrvctx.jpu_irq[core] == irq)
			break;
	}

	bmdi->jpudrvctx.interrupt_flag[core] = 1;
	wake_up_interruptible(&bmdi->jpudrvctx.interrupt_wait_q[core]);

	return IRQ_HANDLED;
}

static void bmdrv_jpu_irq_handler(struct bm_device_info *bmdi)
{
	bm_jpu_irq_handler(bmdi);
}

void bm_jpu_request_irq(struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		bmdrv_submodule_request_irq(bmdi, bmdi->jpudrvctx.jpu_irq[i], bmdrv_jpu_irq_handler);
}

void bm_jpu_free_irq(struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		bmdrv_submodule_free_irq(bmdi, bmdi->jpudrvctx.jpu_irq[i]);
}

int bm_jpu_addr_judge(unsigned long addr, struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		if ((bmdi->jpudrvctx.jpu_register[i].phys_addr >> PAGE_SHIFT) == addr)
			return 0;
	}

	return -1;
}

static int jpu_reg_base_address_init(struct bm_device_info *bmdi)
{
	int i = 0, ret = 0;
	u32 offset = 0;
	u64 bar_paddr = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
        int base_addr = bmdi->cinfo.chip_id == 0x1686 ? jpu_reg_base_1684x[i] : jpu_reg_base[i];

        bm_get_bar_offset(pbar_info, base_addr, &bar_vaddr, &offset);
        bm_get_bar_base(pbar_info, base_addr, &bar_paddr);

		bmdi->jpudrvctx.jpu_register[i].phys_addr = (u64)(bar_paddr + offset);
		bmdi->jpudrvctx.jpu_register[i].virt_addr = (u64)(bar_vaddr + offset);
		bmdi->jpudrvctx.jpu_register[i].size = JPU_REG_SIZE;
		dprintk("[jpudrv]:jpu reg base addr=0x%lx, virtu base addr=0x%lx, size=%u\n",
				bmdi->jpudrvctx.jpu_register[i].phys_addr,
				bmdi->jpudrvctx.jpu_register[i].virt_addr,
				bmdi->jpudrvctx.jpu_register[i].size);
	}

	bm_get_bar_offset(pbar_info, JPU_CONTROL_REG, &bar_vaddr, &offset);
	bm_get_bar_base(pbar_info, JPU_CONTROL_REG, &bar_paddr);

	bmdi->jpudrvctx.jpu_control_register.phys_addr = (unsigned long)(bar_paddr + offset);
	bmdi->jpudrvctx.jpu_control_register.virt_addr = (unsigned long)(bar_vaddr + offset);
	dprintk("[jpudrv]:jpu control reg base addr=0x%lx,virtual base=0x%lx\n",
			bmdi->jpudrvctx.jpu_control_register.phys_addr,
			bmdi->jpudrvctx.jpu_control_register.virt_addr);

	return ret;
}
static int check_jpu_core_busy(struct bm_device_info *bmdi, int coreIdx)
{
	int ret = 0;
    unsigned int enable = 0;
    unsigned int clk_status = 0;
    jpudrv_register_info_t reg_info = bmdi->cinfo.chip_id == 0x1686 ? bm_jpu_en_apb_clk_1684x[coreIdx] : bm_jpu_en_apb_clk[coreIdx];

    enable = bm_read32(bmdi, reg_info.reg);

	if (bmdi->cinfo.chip_id == 0x1682)
		return ret;

    clk_status = 1 << reg_info.bit_n;
	if (enable & clk_status){
	    if (bmdi->jpudrvctx.s_jpu_usage_info.jpu_open_status[coreIdx] == 1){
	        if (atomic_read(&bmdi->jpudrvctx.s_jpu_usage_info.jpu_busy_status[coreIdx]) > 0)
	            ret = 1;
	    }
	}
	return ret;
}


int bm_jpu_check_usage_info(struct bm_device_info *bmdi)
{
	int ret = 0, i;

	jpu_statistic_info_t *jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;

	mutex_lock(&s_jpu_proc_lock);
	/* update usage */
	for (i = 0; i < get_max_num_jpu_core(bmdi); i++){
		int busy = check_jpu_core_busy(bmdi, i);
		int jpu_core_usage = 0;
		int j;
		jpu_usage_info->jpu_status_array[i][jpu_usage_info->jpu_status_index[i]] = busy;
		jpu_usage_info->jpu_status_index[i]++;
		jpu_usage_info->jpu_status_index[i] %= MAX_JPU_STAT_WIN_SIZE;

		if (busy == 1)
			jpu_usage_info->jpu_working_time_in_ms[i] += jpu_usage_info->jpu_instant_interval / MAX_JPU_STAT_WIN_SIZE;
		jpu_usage_info->jpu_total_time_in_ms[i] += jpu_usage_info->jpu_instant_interval / MAX_JPU_STAT_WIN_SIZE;

		for (j = 0; j < MAX_JPU_STAT_WIN_SIZE; j++)
			jpu_core_usage += jpu_usage_info->jpu_status_array[i][j];

		jpu_usage_info->jpu_core_usage[i] = jpu_core_usage;
	}
	mutex_unlock(&s_jpu_proc_lock);

	return ret;
}

int bm_jpu_set_interval(struct bm_device_info *bmdi, int time_interval)
{
	jpu_statistic_info_t *jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;

	mutex_lock(&s_jpu_proc_lock);
	jpu_usage_info->jpu_instant_interval = time_interval;
	mutex_unlock(&s_jpu_proc_lock);

	return 0;
}

static void bm_jpu_usage_info_init(struct bm_device_info *bmdi)
{
	jpu_statistic_info_t *jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;

	memset(jpu_usage_info, 0, sizeof(jpu_statistic_info_t));

	jpu_usage_info->jpu_instant_interval = 500;

	bm_jpu_check_usage_info(bmdi);

	return;
}


int bmdrv_jpu_init(struct bm_device_info *bmdi)
{
	int i;
	jpudrv_instance_list_t *jil;

	memset(&bmdi->jpudrvctx, 0, sizeof(jpu_drv_context_t));
    if(bmdi->cinfo.chip_id == 0x1686)
    {
        memcpy(bmdi->jpudrvctx.jpu_irq, jpu_irq_1684x, sizeof(jpu_irq_1684x));
    }
    else if(bmdi->cinfo.chip_id == 0x1684)
    {
        memcpy(bmdi->jpudrvctx.jpu_irq, jpu_irq, sizeof(jpu_irq));
    }

	INIT_LIST_HEAD(&bmdi->jpudrvctx.jbp_head);
	INIT_LIST_HEAD(&bmdi->jpudrvctx.inst_head);
	mutex_init(&bmdi->jpudrvctx.jpu_core_lock);
	sema_init(&bmdi->jpudrvctx.jpu_sem, get_max_num_jpu_core(bmdi));

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		init_waitqueue_head(&bmdi->jpudrvctx.interrupt_wait_q[i]);

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		jil = kzalloc(sizeof(*jil), GFP_KERNEL);
		if (!jil)
			return -ENOMEM;

		jil->core_idx = i;
		jil->inuse = 0;
		jil->filp = NULL;

		mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
		list_add_tail(&jil->list, &bmdi->jpudrvctx.inst_head);
		mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);
	}

	jpu_cores_reset(bmdi);
	jpu_reg_base_address_init(bmdi);
	/* init jpu_usage_info structure */
	bm_jpu_usage_info_init(bmdi);

	return 0;
}

int bmdrv_jpu_exit(struct bm_device_info *bmdi)
{
	if (bmdi->jpudrvctx.jpu_register[0].virt_addr)
		bmdi->jpudrvctx.jpu_register[0].virt_addr = 0;

	if (bmdi->jpudrvctx.jpu_control_register.virt_addr)
		bmdi->jpudrvctx.jpu_control_register.virt_addr = 0;

	jpu_cores_reset(bmdi);

	return 0;
}

static ssize_t info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int len = 0, err = 0, i = 0;
	char data[512] = { 0 };
	struct bm_device_info *bmdi;
	jpu_statistic_info_t *jpu_usage_info = NULL;

	bmdi = PDE_DATA(file_inode(file));
	jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;

	len = strlen(data);
	sprintf(data + len, "\njpu ctl reg base addr:0x%x\n", JPU_CONTROL_REG);
	len = strlen(data);
	for (i = 0; i < get_max_num_jpu_core(bmdi); i++) {
		sprintf(data + len, "\njpu core[%d] base addr:0x%x, size: 0x%x\n",
				i, jpu_reg_base[i], JPU_REG_SIZE);
		len = strlen(data);
	}

	len = strlen(data);
	sprintf(data + len, "\"core\" : [\n");
	for (i = 0; i < get_max_num_jpu_core(bmdi) - 1; i++) {
		len = strlen(data);
		sprintf(data + len, "{\"id\":%d, \"open_status\":%d, \"usage(short|long)\":%d%%|%llu%%}, ", i, jpu_usage_info->jpu_open_status[i], \
                jpu_usage_info->jpu_core_usage[i], jpu_usage_info->jpu_working_time_in_ms[i]*100/jpu_usage_info->jpu_total_time_in_ms[i]);
	}
	len = strlen(data);
	sprintf(data + len, "{\"id\":%d, \"open_status\":%d, \"usage(short|long)\":%d%%|%llu%%}]\n", i, jpu_usage_info->jpu_open_status[i], \
			jpu_usage_info->jpu_core_usage[i], jpu_usage_info->jpu_working_time_in_ms[i]*100/jpu_usage_info->jpu_total_time_in_ms[i]);

	len = strlen(data);
	if (*ppos >= len)
		return 0;
	err = copy_to_user(buf, data, len);
	if (err)
		return 0;
	*ppos = len;

	return len;
}

static ssize_t info_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	int err = 0, i = 0;
	char data[256] = { 0 };
	unsigned long val = 0, addr = 0;
	struct bm_device_info *bmdi;

	bmdi = PDE_DATA(file_inode(file));

	err = copy_from_user(data, buf, size);
	if (err)
		return -EFAULT;

	err = kstrtoul(data, 16, &val);
	if (err < 0)
		return -EFAULT;
	if (val == 1)
		dbg_enable = 1;
	else if (val == 0)
		dbg_enable = 0;
	else
		addr = val;

	if (val == 0 || val == 1)
		return size;

	if (addr == JPU_CONTROL_REG)
		goto valid_address;

	for (i = 0; i < get_max_num_jpu_core(bmdi); i++)
		if (addr >= jpu_reg_base[i] &&
				(addr <= jpu_reg_base[i] + JPU_REG_SIZE))
			goto valid_address;

	pr_err("jpu proc get addres: 0x%lx invalid\n", addr);
	return -EFAULT;

valid_address:
	val = bm_read32(bmdi, addr);
	pr_info("jpu get address :0x%lx value: 0x%lx\n", addr, val);
	return size;
}

const struct BM_PROC_FILE_OPS  bmdrv_jpu_file_ops = {
	BM_PROC_READ = info_read,
	BM_PROC_WRITE = info_write,
};
