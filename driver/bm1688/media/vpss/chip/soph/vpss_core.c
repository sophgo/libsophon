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
// #include <ldc_cb.h>
// #include <vi_cb.h>
// #include <rgn_cb.h>

#include "base_common.h"
#include "vpss_common.h"
#include "scaler.h"
#include "vpss.h"
#include "vpss_debug.h"
#include "vpss_core.h"
// #include "vo_cb.h"
#include "vpss_ioctl.h"
#include "vpss_proc.h"

#define VPSS_DEV_NAME "soph-vpss"

u32 vpss_log_lv = DBG_WARN;
int hw_mask = 0x3ff; //default vpss_v + vpss_t + vpss_d

static atomic_t open_count = ATOMIC_INIT(0);
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

void vpss_timer_core_update(void *data)
{
	u8 i;
	u32 duration;
	static u32 duration_long = 0;
	static u32 hw_duration_total_long[VPSS_MAX] = {0};
	struct vpss_device *dev = (struct vpss_device *)data;
	struct timespec64 cur_time;
	static struct timespec64 pre_time = {0};

	ktime_get_ts64(&cur_time);
	duration = vpss_get_diff_in_us(pre_time, cur_time);
	duration_long += duration;
	pre_time = cur_time;

	if (duration > 2000000)
		return;

	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		dev->vpss_cores[i].duty_ratio = (dev->vpss_cores[i].hw_duration_total * 100) / duration;
		hw_duration_total_long[i] += dev->vpss_cores[i].hw_duration_total;
		dev->vpss_cores[i].duty_ratio_long = (hw_duration_total_long[i] * 100) / duration_long;
		dev->vpss_cores[i].hw_duration_total = 0;
	}
}

irqreturn_t vpss_isr(int irq, void *data)
{
	union sclr_intr intr_status;
	struct vpss_core *core = (struct vpss_core *)data;
	u8 vpss_idx = core->vpss_type;

	if (core->irq_num != irq) {
		TRACE_VPSS(DBG_ERR, "irq(%d) Error.\n", irq);
		return IRQ_HANDLED;
	}
	intr_status = sclr_intr_status(vpss_idx);
	sclr_intr_clr(vpss_idx, intr_status);

	if(intr_status.b.img_in_frame_end == 1)
		core->intr_status |= INTR_IMG_IN;
	if(intr_status.b.scl_frame_end == 1)
		core->intr_status |= INTR_SC;

	TRACE_VPSS(DBG_DEBUG, "vpss(%d), irq(%d), status(0x%x)\n",
		vpss_idx, irq, intr_status.raw);

	if ((core->intr_status & INTR_IMG_IN) &&
		(core->intr_status & INTR_SC)) {
		//struct sclr_img_checksum_status img_status;
		struct sclr_core_checksum_status sc_status;

		//sclr_img_get_checksum_status(vpss_idx, &img_status);
		//TRACE_VPSS(DBG_DEBUG, "checksum en(%d)\n", img_status.checksum_base.b.enable);
		//TRACE_VPSS(DBG_DEBUG, "checksum axi_read_data(0x%x)\n", img_status.axi_read_data);
		//TRACE_VPSS(DBG_DEBUG, "checksum output_data(0x%x)\n", img_status.checksum_base.b.output_data);
		sclr_core_get_checksum_status(vpss_idx, &sc_status);
		//TRACE_VPSS(DBG_DEBUG, "checksum en(%d)\n", sc_status.checksum_base.b.enable);
		//TRACE_VPSS(DBG_DEBUG, "checksum axi_write_data(0x%x)\n", sc_status.axi_write_data);
		//TRACE_VPSS(DBG_DEBUG, "checksum data_in(0x%x)\n", sc_status.checksum_base.b.data_in_from_img_in);
		//TRACE_VPSS(DBG_DEBUG, "checksum data_out(0x%x)\n", sc_status.checksum_base.b.data_out);

		core->intr_status = 0;
		core->checksum = sc_status.axi_write_data;
		vpss_irq_handler(core);
	}

	return IRQ_HANDLED;
}

void vpss_hw_init(void)
{
	u8 i = 0;
	for (i = VPSS_V0; i < VPSS_MAX; ++i)
		if (hw_mask & BIT(i)) {
			sclr_ctrl_init(i, false);
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
	vpss_init();
	register_timer_fun(vpss_timer_core_update, (void *)dev);
}

void vpss_dev_deinit(struct vpss_device *dev)
{
	u8 i;
	struct vpss_core *core;

	vpss_deinit();
	vpss_hal_deinit();

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

static int vpss_init_resources(struct platform_device *pdev)
{
	int rc = 0;
	int irq_num;
	struct resource *res = NULL;
	void *reg_base[4];
	struct vpss_device *dev;
	int i;

	dev = dev_get_drvdata(&pdev->dev);
	if (!dev) {
		dev_err(&pdev->dev, "Can not get vip drvdata\n");
		return -EINVAL;
	}

	/* register ioremap */
	for (i = 0; i < ARRAY_SIZE(reg_base); ++i) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			TRACE_VPSS(DBG_ERR, "Failed to get res[%d]\n", i);
			return -EINVAL;
		}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
		reg_base[i] = devm_ioremap(&pdev->dev, res->start,
						    res->end - res->start);
#else
		reg_base[i] = devm_ioremap_nocache(&pdev->dev, res->start,
						    res->end - res->start);
#endif
		TRACE_VPSS(DBG_INFO, "(%d) res-reg: start: 0x%llx, end: 0x%llx, virt-addr(%px).\n",
			i, res->start, res->end, reg_base[i]);
		if (!reg_base[i]) {
			TRACE_VPSS(DBG_ERR, "Failed to get reg_base[%d]\n", i);
			return -EINVAL;
		}
	}
	sclr_set_base_addr(reg_base[0], reg_base[1], reg_base[2], reg_base[3]);

	sclr_init_sys_top_addr();

	/* Interrupt */
	for (i = 0; i < ARRAY_SIZE(vpss_name); ++i) {
		irq_num = platform_get_irq_byname(pdev, vpss_name[i]);
		if (irq_num < 0) {
			dev_err(&pdev->dev, "No IRQ resource for [%d]%s\n",
				i, vpss_name[i]);
			return -ENODEV;
		}
		TRACE_VPSS(DBG_INFO, "irq(%d) for %s get from platform driver.\n",
			irq_num, vpss_name[i]);
		dev->vpss_cores[i].irq_num = irq_num;
	}

	/* clk */
	for (i = 0; i < VPSS_MAX; ++i) {
		dev->vpss_cores[i].clk_src = devm_clk_get(&pdev->dev, vpss_clk_name[i][0]);
		if (IS_ERR(dev->vpss_cores[i].clk_src)) {
			pr_err("Cannot get source clk for vpss%d\n", i);
			dev->vpss_cores[i].clk_src = NULL;
		}

		if (vpss_clk_name[i][1]) {
			dev->vpss_cores[i].clk_apb = devm_clk_get(&pdev->dev, vpss_clk_name[i][1]);
			if (IS_ERR(dev->vpss_cores[i].clk_apb)) {
				pr_err("Cannot get apb clk for vpss%d\n", i);
				dev->vpss_cores[i].clk_apb = NULL;
			}
		} else {
			dev->vpss_cores[i].clk_apb = NULL;
		}

		dev->vpss_cores[i].clk_vpss = devm_clk_get(&pdev->dev, vpss_clk_name[i][2]);
		if (IS_ERR(dev->vpss_cores[i].clk_vpss)) {
			pr_err("Cannot get clk for vpss%d\n", i);
			dev->vpss_cores[i].clk_vpss = NULL;
		}
	}

	return rc;
}

static int vpss_open(struct inode *inode, struct file *filep)
{
	struct vpss_device *dev =
		container_of(filep->private_data, struct vpss_device, miscdev);
	int i;

	struct vpss_core *core;

	TRACE_VPSS(DBG_INFO, "vpss_open\n");

	if (!dev) {
		pr_err("Cannot find vpss private data\n");
		return -ENODEV;
	}

	i = atomic_inc_return(&open_count);
	if (i > 1) {
		TRACE_VPSS(DBG_INFO, "vpss_open: open %d times\n", i);
		return 0;
	}

	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		core = &dev->vpss_cores[i];
		if (core->clk_apb)
			clk_enable(core->clk_apb);
	}

	vpss_mode_init();

	return 0;
}

static int vpss_release(struct inode *inode, struct file *filep)
{
	struct vpss_device *dev =
		container_of(filep->private_data, struct vpss_device, miscdev);
	int i;
	struct vpss_core *core;

	TRACE_VPSS(DBG_INFO, "vpss_release\n");

	if (!dev) {
		pr_err("Cannot find vpss private data\n");
		return -ENODEV;
	}

	i = atomic_dec_return(&open_count);
	if (i) {
		TRACE_VPSS(DBG_INFO, "vpss_close: open %d times\n", i);
		return 0;
	}
	vpss_mode_deinit();

	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		core = &dev->vpss_cores[i];

		if (core->clk_apb)
			clk_disable(core->clk_apb);
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long vpss_compat_ptr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op->unlocked_ioctl)
		return -ENOIOCTLCMD;

	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations vpss_fops = {
	.owner = THIS_MODULE,
	.open = vpss_open,
	.release = vpss_release,
	.unlocked_ioctl = vpss_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpss_compat_ptr_ioctl,
#endif
};

static int register_vpss_dev(struct device *dev, struct vpss_device *vpss_dev)
{
	int rc;

	vpss_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	vpss_dev->miscdev.name =
		devm_kasprintf(dev, GFP_KERNEL, VPSS_DEV_NAME);
	vpss_dev->miscdev.fops = &vpss_fops;

	rc = misc_register(&vpss_dev->miscdev);
	if (rc)
		pr_err("vpss_dev: failed to register misc device.\n");

	return rc;
}

static int vpss_probe(struct platform_device *pdev)
{
	int rc = 0;
	int i;
	struct vpss_device *dev;

	/* allocate main structure */
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		TRACE_VPSS(DBG_ERR, "Failed to allocate resource\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, dev);

	// get hw-resources
	rc = vpss_init_resources(pdev);
	if (rc) {
		TRACE_VPSS(DBG_ERR, "Failed to init resource\n");
		goto err_dev;
	}

	rc = register_vpss_dev(&pdev->dev, dev);
	if (rc) {
		TRACE_VPSS(DBG_ERR, "Failed to register dev\n");
		goto err_dev;
	}

	rc = vpss_proc_init(dev);
	if (rc) {
		TRACE_VPSS(DBG_ERR, "Failed to init vpss proc\n");
		goto err_proc;
	}

	rc = vpp_proc_init(dev);
	if (rc) {
		TRACE_VPSS(DBG_ERR, "Failed to init vpss proc\n");
		goto err_proc;
	}

	for (i = 0; i < VPSS_MAX; ++i) {
		if (devm_request_irq(&pdev->dev, dev->vpss_cores[i].irq_num, vpss_isr, IRQF_SHARED,
			vpss_name[i], &dev->vpss_cores[i])) {
			dev_err(&pdev->dev, "Unable to request vpss IRQ(%d)\n",
					dev->vpss_cores[i].irq_num);
			goto err_irq;
		}
	}

	vpss_dev_init(dev);

	device = &pdev->dev;

	// Set DMA mask here
	device->coherent_dma_mask = DMA_BIT_MASK(64);
	device->dma_mask = &device->coherent_dma_mask;

	TRACE_VPSS(DBG_WARN, "vpss probe done\n");

	return rc;

err_irq:
	vpss_proc_remove(dev);
	vpp_proc_remove(dev);
err_proc:
	misc_deregister(&dev->miscdev);
err_dev:
	dev_set_drvdata(&pdev->dev, NULL);
	TRACE_VPSS(DBG_ERR, "failed with rc(%d).\n", rc);
	return rc;
}

/*
 * bmd_remove - device remove method.
 * @pdev: Pointer of platform device.
 */
static int vpss_remove(struct platform_device *pdev)
{
	struct vpss_device *dev;
	int i;

	if (!pdev) {
		dev_err(&pdev->dev, "invalid param");
		return -EINVAL;
	}

	dev = dev_get_drvdata(&pdev->dev);
	if (!dev) {
		dev_err(&pdev->dev, "Can not get vpss drvdata");
		return -EINVAL;
	}

	vpss_dev_deinit(dev);

	for (i = 0; i < VPSS_MAX; ++i)
		devm_free_irq(&pdev->dev, dev->vpss_cores[i].irq_num, &dev->vpss_cores[i]);

	vpss_proc_remove(dev);
	vpp_proc_remove(dev);
	misc_deregister(&dev->miscdev);
	dev_set_drvdata(&pdev->dev, NULL);
	sclr_deinit_sys_top_addr();

	TRACE_VPSS(DBG_WARN, "vpss remove done\n");

	return 0;
}

int vpss_suspend(struct device *dev)
{
	s32 ret = 0;

	/*step 1 stop hal*/
	vpss_hal_suspend();

	/*step 2 suspend envent_handlers*/
	ret = vpss_suspend_handler();
	if (ret) {
		TRACE_VPSS(DBG_ERR, "fail to suspend vpss thread, err=%d\n", ret);
		return -1;
	}

	/*step 3 turn off clock*/
	vpss_clk_disable();

	TRACE_VPSS(DBG_WARN, "vpss suspended\n");
	return 0;
}

int vpss_resume(struct device *dev)
{
	s32 ret = 0;
	u8 i = 0;

	/*step 1 turn on clock*/
	vpss_clk_enable();

	/*step 2 register reset*/
	for (i = VPSS_V0; i < VPSS_MAX; ++i) {
		if (hw_mask & BIT(i)) {
			sclr_ctrl_init(i, true);
			TRACE_VPSS(DBG_INFO, "vpss-%d init done\n", i);
		}
	}

	/*step 3 resume envent_handlers*/
	ret = vpss_resume_handler();
	if (ret) {
		TRACE_VPSS(DBG_ERR, "fail to resume vpss thread, err=%d\n", ret);
		return -1;
	}

	/*step 3 hal start*/
	vpss_hal_resume();

	TRACE_VPSS(DBG_WARN, "vpss resumed\n");
	return 0;
}

static const struct of_device_id vpss_dt_match[] = {
	{.compatible = "cvitek,vpss"},
	{}
};

static SIMPLE_DEV_PM_OPS(vpss_pm_ops, vpss_suspend,
				vpss_resume);

#if (!DEVICE_FROM_DTS)
static void vpss_pdev_release(struct device *dev)
{
}

static struct platform_device vpss_pdev = {
	.name		= "vpss",
	.dev.release	= vpss_pdev_release,
};
#endif

static struct platform_driver vpss_pdrv = {
	.probe      = vpss_probe,
	.remove     = vpss_remove,
	.driver     = {
		.name		= "vpss",
		.owner		= THIS_MODULE,
#if (DEVICE_FROM_DTS)
		.of_match_table	= vpss_dt_match,
#endif
		.pm		= &vpss_pm_ops,
	},
};

static int __init vpss_core_init(void)
{
	int rc;

	TRACE_VPSS(DBG_INFO, " +\n");
	#if (DEVICE_FROM_DTS)
	rc = platform_driver_register(&vpss_pdrv);
	#else
	rc = platform_device_register(&vpss_pdev);
	if (rc)
		return rc;

	rc = platform_driver_register(&vpss_pdrv);
	if (rc)
		platform_device_unregister(&vpss_pdev);
	#endif

	return rc;
}

static void __exit vpss_core_exit(void)
{
	TRACE_VPSS(DBG_INFO, " +\n");

	platform_driver_unregister(&vpss_pdrv);
	#if (!DEVICE_FROM_DTS)
	platform_device_unregister(&vpss_pdev);
	#endif
}

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
