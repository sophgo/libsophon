#include <linux/device.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include "bm_common.h"
#include "bm1688_reg.h"
#include "bm_thread.h"
#include "bm_timer.h"
#include "bm_irq.h"
#include "bm1688_cdma.h"
#include "bm1688_irq.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#else
#include "bm1688_pcie.h"
#endif
#include "bm1688_timer.h"

#define CDMA_DUAL_MAJOR 408
#define CDMA_DUAL_NAME "dual_cdma"
#define CDMA0_XFER_DONE BIT(0)
#define CDMA1_XFER_DONE BIT(1)
#define CDMA0_WAIT_BIT BIT(2)
#define CDMA1_WAIT_BIT BIT(3)
#define CDMA0_XFER_EN BIT(16)
#define CDMA1_XFER_EN BIT(17)
#define CDMA_SEL_OFFSET 0x1000

struct xfer_info {
	u64 cdma_xfer_status;
	u64 cdma0_xfer_sz;
	u64 cdma1_xfer_sz;
	u64 cdma_send_us;
	u64 cdma_start_us;
	u64 cdma0_end_us;
	u64 cdma1_end_us;
};

#if 0
static struct class* dual_cdma_class;
static struct device* dual_cdma_dev;
#endif
static struct xfer_info cdma_xfer_info;
static DECLARE_WAIT_QUEUE_HEAD(req_waitq);

#define WAIT_IRQ_COMPLETE

static void cdma0_reg_write(struct bm_device_info* bmdi, u32 reg_offset, u32 val)
{
	cdma_reg_write(bmdi, reg_offset, val);
}

static void cdma1_reg_write(struct bm_device_info* bmdi, u32 reg_offset, u32 val)
{
	cdma_reg_write(bmdi, reg_offset + CDMA_SEL_OFFSET, val);
}

static u32 cdma0_reg_read(struct bm_device_info* bmdi, u32 reg_offset)
{
	return cdma_reg_read(bmdi, reg_offset);
}

static u32 cdma1_reg_read(struct bm_device_info* bmdi, u32 reg_offset)
{
	return cdma_reg_read(bmdi, reg_offset + CDMA_SEL_OFFSET);
}

void bm1688_clear_cdmairq(struct bm_device_info* bmdi)
{
	int reg_val;

	reg_val = cdma_reg_read(bmdi, CDMA_INT_STAT);
	reg_val |= 1;
	cdma_reg_write(bmdi, CDMA_INT_STAT, reg_val);
}

u32 bm1688_cdma_transfer(struct bm_device_info* bmdi, struct file* file, pbm_cdma_arg parg, bool lock_cdma)
{
	struct bm_thread_info* ti = NULL;
	struct bm_handle_info* h_info = NULL;
	struct bm_trace_item* ptitem = NULL;
	struct bm_memcpy_info* memcpy_info = &bmdi->memcpy_info;
	pid_t api_pid;
	u32 timeout_ms = bmdi->cinfo.delay_ms;
	u64 ret_wait = 0;
	u32 src_addr_hi = 0;
	u32 src_addr_lo = 0;
	u32 dst_addr_hi = 0;
	u32 dst_addr_lo = 0;
	u32 cdma_max_payload = 0;
	u32 reg_val = 0;
	u64 src = parg->src;
	u64 dst = parg->dst;
	u32 count = 3000;
	u32 nv_cdma_start_us = 0;
	u32 nv_cdma_end_us = 0;
	u32 nv_cdma_send_us = 0;
	u32 timeout = timeout_ms * 1000;
	// u32 int_mask_val;

	bm1688_timer_start(bmdi);
	udelay(1);
	// printk("cdma transfer enter\n");
	cdma_xfer_info.cdma_xfer_status = 0;
	nv_cdma_send_us = bm1688_timer_get_time_us(bmdi);

	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;

	if (file) {
		if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
			pr_err("bmdrv: file list is not found!\n");
			return -EINVAL;
		}
	}
	if (lock_cdma)
		mutex_lock(&memcpy_info->cdma_mutex);

	api_pid = current->pid;
	ti = bmdrv_find_thread_info(h_info, api_pid);

	if (ti && (parg->dir == HOST2CHIP)) {
		ti->profile.cdma_out_counter++;
		bmdi->profile.cdma_out_counter++;
	} else {
		if (ti) {
			ti->profile.cdma_in_counter++;
			bmdi->profile.cdma_in_counter++;
		}
	}

	if (ti && ti->trace_enable) {
		ptitem = (struct bm_trace_item*)mempool_alloc(bmdi->trace_info.trace_mempool, GFP_KERNEL);
		ptitem->payload.trace_type = 0;
		ptitem->payload.cdma_dir = parg->dir;
		ptitem->payload.sent_time = nv_cdma_send_us;
		ptitem->payload.start_time = 0;
	}

	/*Enable CDMA & interrupt*/
	reg_val = cdma_reg_read(bmdi, CDMA_CHL_CTRL);
	if (parg->intr)
		reg_val |= CDMA_CHL_CTRL_IRQ_EN;
	else
		reg_val &= ~CDMA_CHL_CTRL_IRQ_EN;
	reg_val |= CDMA_CHL_CTRL_DMA_EN;
	cdma_reg_write(bmdi, CDMA_CHL_CTRL, reg_val);

	/*set max payload which equal to PCIE bandwidth.*/
#ifdef SOC_MODE
	cdma_max_payload = 0x9;
#else
	cdma_max_payload = memcpy_info->cdma_max_payload;
#endif
	cdma_max_payload &= 0x3F; // [0:2]read payload width, [3:5]write payload width

	cdma_reg_write(bmdi, CDMA_MAX_PAYLOAD, cdma_max_payload);

	// Check cdma's FIFO status.
	timeout = timeout_ms * 1000;
	while (cdma_reg_read(bmdi, CDMA_CMD_FIFO_STAT) > 7) {
		udelay(1);
		if (--timeout == 0) {
			pr_err("cdma fifo_stat timeout\n");
			if (lock_cdma)
				mutex_unlock(&memcpy_info->cdma_mutex);
			return -EBUSY;
		}
	}
	PR_TRACE("CDMA fifo wait %dus\n", timeout_ms * 1000 - timeout);

	/* clear rf_des_int_mask */
	reg_val = cdma_reg_read(bmdi, CDMA_INT_MASK);
	reg_val &= ~(CDMA_INT_RF_DES_MASK);
	reg_val |= CDMA_INT_RF_CMD_EPT_MASK;
	cdma_reg_write(bmdi, CDMA_INT_MASK, reg_val);

#ifndef SOC_MODE
	if (parg->dir != CHIP2CHIP) {
		if (!parg->use_iommu) {
			if (parg->dir == CHIP2HOST)
				dst |= (u64)bmdi->cinfo.ob_base << 32;
			else
				src |= (u64)bmdi->cinfo.ob_base << 32;
		}
	}
#endif
	src_addr_hi = src >> 32;
	src_addr_lo = src & 0xffffffff;
	dst_addr_hi = dst >> 32;
	dst_addr_lo = dst & 0xffffffff;
	// pr_info("src:0x%llx dst:0x%llx size:%lld\n", src, dst, parg->size);
	PR_TRACE("src_addr_hi 0x%x src_addr_low 0x%x\n", src_addr_hi, src_addr_lo);
	PR_TRACE("dst_addr_hi 0x%x dst_addr_low 0x%x\n", dst_addr_hi, dst_addr_lo);

	cdma_reg_write(bmdi, CDMA_CMD_ACCP3, src_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_hi);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP5, dst_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP6, dst_addr_hi);
	if (parg->type == 0) {
		/* 1-D descriptor */
		cdma_reg_write(bmdi, CDMA_CMD_ACCP7, parg->size);
		reg_val = CDMA_CMD_NORMAL_CFG;
	} else if (parg->type == 1) {
		/* 2-D descriptor */
		cdma_reg_write(bmdi, CDMA_CMD_ACCP2, (parg->width << 16) | parg->height);
		cdma_reg_write(bmdi, CDMA_CMD_ACCP1, (parg->src_width << 16) | parg->dst_width);
		reg_val = CDMA_CMD_STRIDE_CFG;
		if (parg->flush) {
			parg->size = parg->dst_width * parg->height; // image_data + fixed_data
			reg_val |= CDMA_CMD_TRANS_WITH_FLUSH;
			reg_val |= parg->fixed_data << 16;
		} else {
			parg->size = parg->width * parg->height;
		}
		if (parg->format == 2) {
			parg->size *= 2;
			reg_val |= CDMA_CMD_2D_FORMAT_WORD;
		}
	}

	/* Using interrupt(10s timeout) or polling for detect cmda done */
	cdma_xfer_info.cdma_xfer_status |= CDMA0_XFER_EN;
	nv_cdma_start_us = bm1688_timer_get_time_us(bmdi);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP0, reg_val);
	if (parg->intr) {
		PR_TRACE("wait cdma\n");
		ret_wait = wait_for_completion_timeout(&memcpy_info->cdma_done,
			msecs_to_jiffies(timeout_ms));
		nv_cdma_end_us = bm1688_timer_get_time_us(bmdi);
		PR_TRACE("src:0x%llx dst:0x%llx size:%llx\n", src, dst, parg->size);
		PR_TRACE("time = %d\n", nv_cdma_end_us - nv_cdma_start_us);
		PR_TRACE("time = %d, function_num = %d, start = %d, end = %d, size = %llx, max_payload = %d\n",
			nv_cdma_end_us - nv_cdma_start_us, bmdi->cinfo.chip_index & 0x3,
			nv_cdma_start_us, nv_cdma_end_us, parg->size, cdma_max_payload);

		if (ret_wait) {
			PR_TRACE("End: wait cdma done\n");
		} else {
			pr_info("bmsophon%d, wait cdma timeout, src:0x%llx dst:0x%llx size:0x%llx dir:%d\n", bmdi->dev_index, src, dst, parg->size, parg->dir);
			if (lock_cdma)
				mutex_unlock(&memcpy_info->cdma_mutex);
			return -EBUSY;
		}
	} else {
		while (((cdma_reg_read(bmdi, CDMA_INT_STAT)) & 0x1) != 0x1) {
			udelay(1);
			if (--count == 0) {
				pr_err("[%s: %d] cdma polling wait timeout\n", __func__, __LINE__);
				if (lock_cdma)
					mutex_unlock(&memcpy_info->cdma_mutex);
				return -EBUSY;
			}
		}
		nv_cdma_end_us = bm1688_timer_get_time_us(bmdi);
		reg_val = cdma_reg_read(bmdi, CDMA_INT_STAT);
		reg_val |= (1 << 0x0);
		cdma_reg_write(bmdi, CDMA_INT_STAT, reg_val);

		PR_TRACE("cdma transfer using polling mode end\n");
	}

	if (ti && (parg->dir == HOST2CHIP)) {
		ti->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
		bmdi->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
	} else {
		if (ti) {
			ti->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
			bmdi->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
		}
	}

	if (ti && ti->trace_enable) {
		ptitem->payload.end_time = nv_cdma_end_us;
		ptitem->payload.start_time = nv_cdma_start_us;
		INIT_LIST_HEAD(&ptitem->node);
		mutex_lock(&ti->trace_mutex);
		list_add_tail(&ptitem->node, &ti->trace_list);
		ti->trace_item_num++;
		mutex_unlock(&ti->trace_mutex);
	}

	if (lock_cdma)
		mutex_unlock(&memcpy_info->cdma_mutex);
	bm1688_timer_stop(bmdi);
	// printk("cdma transfer exit\n");
	return 0;
}

static void dual_cdma_reg_write(struct bm_device_info* bmdi, u32 reg_offset, u32 val, u32 cdma)
{
	if (cdma == 1)
		reg_offset += CDMA_SEL_OFFSET;

	cdma_reg_write(bmdi, reg_offset, val);
}

static u32 dual_cdma_reg_read(struct bm_device_info* bmdi, u32 reg_offset, u32 cdma)
{
	if (cdma == 1)
		reg_offset += CDMA_SEL_OFFSET;

	return cdma_reg_read(bmdi, reg_offset);
}

void bm1688_cdma0_irq_handler(struct bm_device_info* bmdi)
{
	u32 cdma_status = cdma_xfer_info.cdma_xfer_status >> 16;
	u32 int_status = cdma0_reg_read(bmdi, CDMA_INT_STAT);
	if (int_status & 0x1) {
		cdma_xfer_info.cdma0_end_us = bm1688_timer_get_time_us(bmdi);
		cdma_xfer_info.cdma_xfer_status |= CDMA0_XFER_DONE;
		cdma_status ^= cdma_xfer_info.cdma_xfer_status;
		if (!(cdma_status & 0x3)) {
			wake_up_interruptible(&req_waitq);
#ifdef WAIT_IRQ_COMPLETE
			complete(&bmdi->memcpy_info.cdma_done);
#endif
		}
		cdma0_reg_write(bmdi, CDMA_INT_STAT, int_status);
	}
	printk("cdma0 interrupt 0x%x\n", int_status);
}

void bm1688_cdma1_irq_handler(struct bm_device_info* bmdi)
{
	u32 cdma_status = cdma_xfer_info.cdma_xfer_status >> 16;
	u32 int_status = cdma1_reg_read(bmdi, CDMA_INT_STAT);
	if (int_status & 0x1) {
		cdma_xfer_info.cdma1_end_us = bm1688_timer_get_time_us(bmdi);
		cdma_xfer_info.cdma_xfer_status |= CDMA1_XFER_DONE;
		cdma_status ^= cdma_xfer_info.cdma_xfer_status;
		if (!(cdma_status & 0x3)) {
			wake_up_interruptible(&req_waitq);
#ifdef WAIT_IRQ_COMPLETE
			complete(&bmdi->memcpy_info.cdma_done);
#endif
		}
		cdma1_reg_write(bmdi, CDMA_INT_STAT, int_status);
	}
	printk("cdma1 interrupt 0x%x\n", int_status);
}

static ssize_t bm1688_dual_cdma_read(struct file* filp, char __user* buff, size_t count, loff_t* offp)
{
	int ret = 0;
	if (count != 1)
		return -EINVAL;

	ret = copy_to_user(buff, &cdma_xfer_info, sizeof(struct xfer_info));
	if (ret)
		PR_TRACE("copy error\n");
	return count;
}

static int bm1688_dual_cdma_open(struct inode* inode, struct file* file)
{
	PR_TRACE("bm1688_dual_cdma_open\n");
	return 0;
}

static int bm1688_dual_cdma_close(struct inode* inode, struct file* file)
{
	PR_TRACE("bm1688_dual_cdma_close\n");
	return 0;
}

static unsigned int bm1688_dual_cdma_poll(struct file* file, poll_table* wait)
{
	unsigned int mask = 0;

	poll_wait(file, &req_waitq, wait);

	mask = cdma_xfer_info.cdma_xfer_status;

	return mask;
}

static const struct file_operations bm1688_dual_cdma_fops = {
	.owner = THIS_MODULE,
	.open = bm1688_dual_cdma_open,
	.read = bm1688_dual_cdma_read,
	//.write = bm1688_dual_cdma_write,
	//.fasync = bm1688_dual_cdma_fasync,
	.release = bm1688_dual_cdma_close,
	.poll = bm1688_dual_cdma_poll,
};

int bm1688_dual_cdma_init(struct bm_device_info* bmdi)
{
#if 0
	int ret;

	// mkdev
	ret = register_chrdev(CDMA_DUAL_MAJOR, CDMA_DUAL_NAME, &bm1688_dual_cdma_fops);
	if (ret < 0) {
		printk("can't register major number\n");
		return ret;
	}

	dual_cdma_class = class_create(THIS_MODULE, CDMA_DUAL_NAME);
	if (IS_ERR(dual_cdma_class)) {
		unregister_chrdev(CDMA_DUAL_MAJOR, CDMA_DUAL_NAME);
		return PTR_ERR(dual_cdma_class);
	}

	dual_cdma_dev = device_create(dual_cdma_class, NULL, MKDEV(CDMA_DUAL_MAJOR, 0), NULL, CDMA_DUAL_NAME);
	if (unlikely(IS_ERR(dual_cdma_dev))) {
		class_destroy(dual_cdma_class);
		unregister_chrdev(CDMA_DUAL_MAJOR, CDMA_DUAL_NAME);
		return PTR_ERR(dual_cdma_dev);
	}
#endif
	// request_irq
#ifndef SOC_MODE
	bmdrv_submodule_request_irq(bmdi, BM1688_CDMA0_IRQ_ID, bm1688_cdma0_irq_handler);
	bmdrv_submodule_request_irq(bmdi, BM1688_CDMA1_IRQ_ID, bm1688_cdma1_irq_handler);
#endif

	pr_info("dual cdma device initialized\n");
	return 0;
}

int bm1688_dual_cdma_remove(struct bm_device_info* bmdi)
{
#ifndef SOC_MODE
	bmdrv_submodule_free_irq(bmdi, BM1688_CDMA0_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, BM1688_CDMA1_IRQ_ID);
#endif
#if 0
	device_destroy(dual_cdma_class, MKDEV(CDMA_DUAL_MAJOR, 0));
	class_destroy(dual_cdma_class);

	unregister_chrdev(CDMA_DUAL_MAJOR, CDMA_DUAL_NAME);
#endif
	// request_irq
	pr_info("dual cdma device removed\n");
	return 0;
}

static void cdma_setting_apply(struct bm_device_info* bmdi, pbm_cdma_arg parg, u32* acc0, u32 cdma)
{
#ifndef SOC_MODE
	struct bm_memcpy_info* memcpy_info = &bmdi->memcpy_info;
#endif
	u64 src = parg->src;
	u64 dst = parg->dst;
	u32 src_addr_hi = 0;
	u32 src_addr_lo = 0;
	u32 dst_addr_hi = 0;
	u32 dst_addr_lo = 0;
	u32 reg_val = 0;
	u32 cdma_max_payload = 0;

	/*Enable CDMA & interrupt*/
	reg_val = dual_cdma_reg_read(bmdi, CDMA_CHL_CTRL, cdma);
	if (parg->intr)
		reg_val |= CDMA_CHL_CTRL_IRQ_EN;
	else
		reg_val &= ~CDMA_CHL_CTRL_IRQ_EN;
	reg_val |= CDMA_CHL_CTRL_DMA_EN;
	dual_cdma_reg_write(bmdi, CDMA_CHL_CTRL, reg_val, cdma);

	/*set max payload which equal to PCIE bandwidth.*/
#ifdef SOC_MODE
	cdma_max_payload = 0x9;
#else
	cdma_max_payload = memcpy_info->cdma_max_payload;
#endif
	cdma_max_payload &= 0x3F; // [0:2]read payload width, [3:5]write payload width

	dual_cdma_reg_write(bmdi, CDMA_MAX_PAYLOAD, cdma_max_payload, cdma);

	/* clear rf_des_int_mask */
	reg_val = dual_cdma_reg_read(bmdi, CDMA_INT_MASK, cdma);
	reg_val &= ~(CDMA_INT_RF_DES_MASK);
	reg_val |= CDMA_INT_RF_CMD_EPT_MASK;
	dual_cdma_reg_write(bmdi, CDMA_INT_MASK, reg_val, cdma);

#ifndef SOC_MODE
	if (parg->dir != CHIP2CHIP) {
		if (!parg->use_iommu) {
			if (parg->dir == CHIP2HOST)
				dst |= (u64)bmdi->cinfo.ob_base << 32;
			else
				src |= (u64)bmdi->cinfo.ob_base << 32;
		}
	}
#endif
	src_addr_hi = src >> 32;
	src_addr_lo = src & 0xffffffff;
	dst_addr_hi = dst >> 32;
	dst_addr_lo = dst & 0xffffffff;
	PR_TRACE("src:0x%llx dst:0x%llx size:%lld\n", src, dst, parg->size);
	PR_TRACE("src_addr_hi 0x%x src_addr_low 0x%x\n", src_addr_hi, src_addr_lo);
	PR_TRACE("dst_addr_hi 0x%x dst_addr_low 0x%x\n", dst_addr_hi, dst_addr_lo);

	dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP3, src_addr_lo, cdma);
	dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_hi, cdma);
	dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP5, dst_addr_lo, cdma);
	dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP6, dst_addr_hi, cdma);

	if (parg->type == 1) {
		/* 2-D descriptor */
		dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP2, (parg->width << 16) | parg->height, cdma);
		dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP1, (parg->src_width << 16) | parg->dst_width, cdma);
		reg_val = CDMA_CMD_STRIDE_CFG;
		if (parg->flush) {
			parg->size = parg->dst_width * parg->height; // image_data + fixed_data
			reg_val |= CDMA_CMD_TRANS_WITH_FLUSH;
			reg_val |= parg->fixed_data << 16;
		} else {
			parg->size = parg->width * parg->height;
		}
		if (parg->format == 2) {
			parg->size *= 2;
			reg_val |= CDMA_CMD_2D_FORMAT_WORD;
		}
	} else {
		/* 1-D descriptor */
		dual_cdma_reg_write(bmdi, CDMA_CMD_ACCP7, parg->size, cdma);
		reg_val = CDMA_CMD_NORMAL_CFG;
	}

	*acc0 = reg_val;
}

// only for bandwidth test
u32 bm1688_dual_cdma_transfer(struct bm_device_info* bmdi, struct file* file, pbm_cdma_arg parg0, pbm_cdma_arg parg1, bool lock_cdma)
{
	struct bm_handle_info* h_info = NULL;
	struct bm_memcpy_info* memcpy_info = &bmdi->memcpy_info;
	u32 timeout_ms = bmdi->cinfo.delay_ms;
	u32 reg_val = 0;
	u32 count = 3000;
	u32 wait_bit = 0;
	u32 reg_accp0_0 = 0;
	u32 reg_accp0_1 = 0;
#ifdef WAIT_IRQ_COMPLETE
	u64 ret_wait = 0;
#endif
	// u32 int_mask_val;

	bm1688_timer_start(bmdi);
	udelay(1); //for timer stable
	cdma_xfer_info.cdma_xfer_status = 0;
	printk("cdma dual transfer enter\n");
	cdma_xfer_info.cdma_send_us = bm1688_timer_get_time_us(bmdi);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
	timeout_ms *= PALLADIUM_CLK_RATIO;
	if (file) {
		if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
			pr_err("bmdrv: file list is not found!\n");
			return -EINVAL;
		}
	}
	if (lock_cdma)
		mutex_lock(&memcpy_info->cdma_mutex);

	if (parg0 != NULL) {
		cdma_setting_apply(bmdi, parg0, &reg_accp0_0, 0);
		cdma_xfer_info.cdma_xfer_status |= CDMA0_XFER_EN;
		if (!parg0->intr)
			wait_bit |= CDMA0_WAIT_BIT;
		cdma_xfer_info.cdma0_xfer_sz = parg0->size;
	}

	if (parg1 != NULL) {
		cdma_xfer_info.cdma_xfer_status |= CDMA1_XFER_EN;
		cdma_setting_apply(bmdi, parg1, &reg_accp0_1, 1);
		if (!parg1->intr)
			wait_bit |= CDMA1_WAIT_BIT;
		cdma_xfer_info.cdma1_xfer_sz = parg1->size;
	}

	cdma_xfer_info.cdma_start_us = bm1688_timer_get_time_us(bmdi);
	cdma0_reg_write(bmdi, CDMA_CMD_ACCP0, reg_accp0_0);
	cdma1_reg_write(bmdi, CDMA_CMD_ACCP0, reg_accp0_1);

#ifdef WAIT_IRQ_COMPLETE
	if (wait_bit == 0) {
		// shouldn't work in interrupt mode when another cdma polling
		ret_wait = wait_for_completion_timeout(&memcpy_info->cdma_done,
			msecs_to_jiffies(timeout_ms));
		if (ret_wait)
			PR_TRACE("End : wait cdma done\n");
	}
#endif

	while (wait_bit) {
		udelay(1);
		if (--count == 0) {
			pr_err("[%s: %d] cdma polling wait timeout\n", __func__, __LINE__);
			if (lock_cdma)
				mutex_unlock(&memcpy_info->cdma_mutex);
			return -EBUSY;
		}
		if ((wait_bit & CDMA0_WAIT_BIT) && (cdma0_reg_read(bmdi, CDMA_INT_STAT) & 0x01)) {
			cdma_xfer_info.cdma0_end_us = bm1688_timer_get_time_us(bmdi);
			cdma_xfer_info.cdma_xfer_status |= CDMA0_XFER_DONE;
			wake_up_interruptible(&req_waitq);
			reg_val = cdma0_reg_read(bmdi, CDMA_INT_STAT);
			reg_val |= 0x01;
			cdma0_reg_write(bmdi, CDMA_INT_STAT, reg_val);
			wait_bit &= ~CDMA0_WAIT_BIT;
		}
		if ((wait_bit & CDMA1_WAIT_BIT) && (cdma1_reg_read(bmdi, CDMA_INT_STAT) & 0x01)) {
			cdma_xfer_info.cdma1_end_us = bm1688_timer_get_time_us(bmdi);
			cdma_xfer_info.cdma_xfer_status |= CDMA1_XFER_DONE;
			wake_up_interruptible(&req_waitq);
			reg_val = cdma1_reg_read(bmdi, CDMA_INT_STAT);
			reg_val |= 0x01;
			cdma1_reg_write(bmdi, CDMA_INT_STAT, reg_val);
			wait_bit &= ~CDMA1_WAIT_BIT;
		}
		if (wait_bit == 0)
			bm1688_timer_stop(bmdi);
		PR_TRACE("cdma transfer using polling mode end\n");
	}

	if (lock_cdma)
		mutex_unlock(&memcpy_info->cdma_mutex);
	printk("cdma dual transfer exit\n");
	return 0;
}

#ifdef SOC_MODE
void bm1688_cdma_reset(struct bm_device_info* bmdi)
{
	PR_TRACE("bm1688 cdma reset\n");
	reset_control_assert(bmdi->cinfo.cdma);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.cdma);
}

void bm1688_cdma_clk_enable(struct bm_device_info* bmdi)
{
	PR_TRACE("bm1688 cdma clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.cdma_clk);
}

void bm1688_cdma_clk_disable(struct bm_device_info* bmdi)
{
	PR_TRACE("bm1688 cdma clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.cdma_clk);
}
#endif
