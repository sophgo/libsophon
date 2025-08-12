#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>

#include "base_ctx.h"

#include <base_cb.h>
#include <base_ctx.h>

#include "base_common.h"
#include "vpss_common.h"
#include "scaler.h"
#include "vpss.h"
#include "vpss_debug.h"
#include "vpss_core.h"

u32 vpss_log_lv = DBG_WARN;
int hw_mask = 0x3ff; //default vpss_v + vpss_t + vpss_d

struct device *device;
static const char *const vpss_name[] = {"vpss_v0", "vpss_v1", "vpss_v2",
										"vpss_v3", "vpss_t0", "vpss_t1",
										"vpss_t2", "vpss_t3", "vpss_d0",
										"vpss_d1"};
static const char *const vpss_clk_name[VPSS_MAX][3] = {
					{"clk_vi_sys3", NULL, "clk_vpss_v0"},
					{"clk_vi_sys3", NULL, "clk_vpss_v1"},
					{"clk_vi_sys3", NULL, "clk_vpss_v2"},
					{"clk_vi_sys3", NULL, "clk_vpss_v3"},
					{"clk_vd0_sys2", "clk_vd0_apb_vpss0", "clk_vpss_t0"},
					{"clk_vd0_sys2", "clk_vd0_apb_vpss1", "clk_vpss_t1"},
					{"clk_vd1_sys2", "clk_vd1_apb_vpss0", "clk_vpss_t2"},
					{"clk_vd1_sys2", "clk_vd1_apb_vpss1", "clk_vpss_t3"},
					{"clk_vo_sys1", NULL, "clk_vpss_d0"},
					{"clk_vo_sys1", NULL, "clk_vpss_d1"}};

module_param(vpss_log_lv, int, 0644);

module_param(hw_mask, int, 0644);

long vpss_get_diff_in_us(struct timespec64 start, struct timespec64 end) {
    return (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
}

irqreturn_t vpss_isr(int irq, void *data)
{
	union sclr_intr intr_status;
	struct vpss_core *core = (struct vpss_core *)data;
	u8 vpss_idx = core->vpss_type;
	struct vpss_device *dev = container_of(data, struct vpss_device, vpss_cores[vpss_idx]);

	if (core->irq_num != irq) {
		TRACE_VPSS(DBG_ERR, "irq(%d) Error.\n", irq);
		return IRQ_HANDLED;
	}
	intr_status = sclr_intr_status(dev->scaler, vpss_idx);
	sclr_intr_clr(dev->scaler, vpss_idx, intr_status);

	if(intr_status.b.img_in_frame_end == 1)
		core->intr_status |= INTR_IMG_IN;
	if(intr_status.b.scl_frame_end == 1)
		core->intr_status |= INTR_SC;

	TRACE_VPSS(DBG_DEBUG, "vpss(%d), irq(%d), status(0x%x)\n",
		vpss_idx, irq, intr_status.raw);

	if ((core->intr_status & INTR_IMG_IN) &&
		(core->intr_status & INTR_SC)) {
		struct sclr_core_checksum_status sc_status;
		sclr_core_get_checksum_status(dev->scaler, vpss_idx, &sc_status);
		core->intr_status = 0;
		core->checksum = sc_status.axi_write_data;
		vpss_irq_handler(core);
	}

	return IRQ_HANDLED;
}

void vpss_hw_init(struct scaler *scaler)
{
	u8 i = 0;
	for (i = VPSS_V0; i < VPSS_MAX; ++i)
		if (hw_mask & BIT(i)) {
			sclr_ctrl_init(scaler, i, false);
			TRACE_VPSS(DBG_INFO, "vpss-%d init done\n", i);
		}
}

void vpss_dev_init(struct vpss_device *dev)
{
	u8 i;
	struct vpss_core *core;

	/* initialize locks */
	spin_lock_init(&dev->lock);

	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		core = &dev->vpss_cores[i];
		core->vpss_type = i;
		spin_lock_init(&core->vpss_lock);
		core->job = NULL;
		core->intr_status = 0;

		if (core->clk_src)
			clk_prepare_enable(core->clk_src);
		if (core->clk_apb)
			clk_prepare_enable(core->clk_apb);
		if (core->clk_vpss)
			clk_prepare_enable(core->clk_vpss);

		if (hw_mask & BIT(i))
			atomic_set(&core->state, VIP_IDLE);
		else
			atomic_set(&core->state, VIP_RUNNING);

		if (core->clk_apb)
			clk_disable(core->clk_apb);
		if (core->clk_vpss)
			clk_disable(core->clk_vpss);
	}

	vpss_hal_init(dev);
	vpss_init(dev);
}

void vpss_dev_deinit(struct vpss_device *dev)
{
	u8 i;
	struct vpss_core *core;

	vpss_hal_deinit(dev);

	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		core = &dev->vpss_cores[i];

		if (core->clk_src)
			clk_disable_unprepare(core->clk_src);
		if (core->clk_apb)
			clk_disable_unprepare(core->clk_apb);
		if (core->clk_vpss)
			clk_unprepare(core->clk_vpss);
	}
}

#ifdef CONFIG_COMPAT
static long vpss_compat_ptr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op->unlocked_ioctl)
		return -ENOIOCTLCMD;

	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct of_device_id vpss_dt_match[] = {
	{.compatible = "cvitek,vpss"},
	{}
};

#if (!DEVICE_FROM_DTS)
static void vpss_pdev_release(struct device *dev)
{
}

static struct platform_device vpss_pdev = {
	.name		= "vpss",
	.dev.release	= vpss_pdev_release,
};
#endif

unsigned long vpss_dmabuf_fd_to_paddr(int dmabuf_fd){
	struct dma_buf *dma_buf = NULL;
	struct dma_buf_attachment *dma_attach = NULL;
	struct sg_table *sgt = NULL;
	unsigned long paddr = 0;
	dma_buf = dma_buf_get(dmabuf_fd);
	if (IS_ERR(dma_buf)) {
		TRACE_VPSS(DBG_ERR, "get dmabuf failed\n");
		dma_buf_put(dma_buf);
		return paddr;
	}
	dma_attach = dma_buf_attach(dma_buf, device);
	if (IS_ERR(dma_attach)) {
		TRACE_VPSS(DBG_ERR, "get dma_buf_attach failed\n");
		goto fail_detach;
	}
	sgt = dma_buf_map_attachment(dma_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		TRACE_VPSS(DBG_ERR, "get dma_buf_map_attachment failed\n");
		goto fail_unmap;
	}
	if (IS_ERR(sgt->sgl)) {
		TRACE_VPSS(DBG_ERR, "get sgl failed\n");
		goto fail_unmap;
	}
	paddr = sg_dma_address(sgt->sgl);
fail_unmap:
	dma_buf_unmap_attachment(dma_attach, sgt, DMA_BIDIRECTIONAL);
fail_detach:
	dma_buf_detach(dma_buf, dma_attach);
	dma_buf_put(dma_buf);
	return paddr;
}

MODULE_DESCRIPTION("Cvitek Video Driver");
MODULE_AUTHOR("zunshi.liu");
MODULE_LICENSE("GPL");
