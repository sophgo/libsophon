#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "bm_common.h"
#include "bm_drv.h"
#include "bm_io.h"
#include "bm_irq.h"
#include "bm_clkrst.h"
#include "bm_gmem.h"

extern int dev_count;
extern struct bm_ctrl_info *bmci;

static const struct of_device_id bmdrv_match_table[] = {
	{.compatible = "cvitek,tpu"},
	{},
};

static struct kobj_type bmdrv_ktype = {
	NULL
};

static int platform_init_bar_address(struct platform_device *pdev, struct chip_info *cinfo)
{
	struct resource *res;
	struct bm_bar_info *bar_info = &cinfo->bar_info;
	u32 i = 0;

	for (i = 0; i < REG_COUNT; ++i) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL)
			return -EINVAL;
		bar_info->bar_len[i] = resource_size(res);
		bar_info->bar_start[i] = res->start;
		bar_info->bar_dev_start[i] = res->start;
		bar_info->bar_vaddr[i] = of_iomap(pdev->dev.of_node, i);
	}
	return 0;
}


static int bmdrv_cinfo_init(struct bm_device_info *bmdi, struct platform_device *pdev)
{
	struct chip_info *cinfo = &bmdi->cinfo;
	struct device_node *tpu_node;
	// u32 chip_id = 0;

	tpu_node = of_node_get(pdev->dev.of_node);
	if (of_device_is_compatible(tpu_node, "cvitek,tpu")) {
		cinfo->chip_id = CHIP_ID;
	} else {
		dev_err(&pdev->dev, "invalid device\n");
		return -1;
	}

	switch (cinfo->chip_id) {
	case CHIP_ID:
		cinfo->bm_reg = &bm_reg_SGTPUV8;
		cinfo->share_mem_size = 1 << 12;
		cinfo->chip_type = "MARS3";
		cinfo->platform = DEVICE;
		// cinfo->bmdrv_pending_msgirq_cnt = SGTPUV8_pending_msgirq_cnt;
		cinfo->tpu_core_num = 1;
		break;
	default:
		sprintf(cinfo->dev_name, "%s", "unknown device");
		return -EINVAL;
	}
	cinfo->delay_ms = DELAY_MS;
	cinfo->polling_ms = POLLING_MS;
	cinfo->pdev = pdev;
	cinfo->device = &pdev->dev;
	sprintf(cinfo->dev_name, "%s", BM_CDEV_NAME);
	return 0;
}

static int bmdrv_init_misc_info(struct platform_device *pdev, struct bm_device_info *bmdi)
{
	struct bm_misc_info *misc_info = &bmdi->misc_info;

	switch (bmdi->cinfo.chip_id) {
	case CHIP_ID:
		misc_info->chipid_bit_mask = SGTPUV8_CHIPID_BIT_MASK;
		break;
	default:
		sprintf(bmdi->cinfo.dev_name, "%s", "unknown device");
		return -EINVAL;
	}

	misc_info->chipid = bmdi->cinfo.chip_id;
	misc_info->tpu_core_num = bmdi->cinfo.tpu_core_num;
	misc_info->pcie_soc_mode = 1;
	misc_info->driver_version = BM_DRIVER_VERSION;
	return 0;
}

u64 dummy_dma_mask = DMA_BIT_MASK(42);

static int bmdrv_platform_init(struct bm_device_info *bmdi, struct platform_device *pdev)
{
	int rc = 0;
	struct chip_info *cinfo = &bmdi->cinfo;
	pr_info("42 bit mask\n");
	rc = platform_init_bar_address(pdev, cinfo);
	if (rc) {
		dev_err(&pdev->dev, "alloc bar address error\n");
		return rc;
	}

	io_init(bmdi);

	cinfo->tpu_core_num = 1;
	cinfo->device->dma_mask = &dummy_dma_mask;
	cinfo->device->coherent_dma_mask = dummy_dma_mask;

	bmdi->MMAP_TPYE=0;
	platform_set_drvdata(pdev, bmdi);
	return rc;
}

static void bmdrv_platform_deinit(struct bm_device_info *bmdi, struct platform_device *pdev)
{
	struct chip_info *cinfo = &bmdi->cinfo;

	platform_set_drvdata(pdev, NULL);
	cinfo->device->dma_mask = NULL;
	cinfo->device->coherent_dma_mask = 0;
}

static int bmdrv_hardware_init(struct bm_device_info *bmdi)
{
	//enable tiu and gdma
	writel(0xff, bmdi->cinfo.bar_info.bar_vaddr[MMAP_SYS]);
	//enable tpu
	writel(readl(bmdi->cinfo.bar_info.bar_vaddr[MMAP_REG] + 0x100) | 0x1,
							bmdi->cinfo.bar_info.bar_vaddr[MMAP_REG] + 0x100);

	SGTPUV8_modules_clk_init(bmdi);
	SGTPUV8_modules_clk_enable(bmdi);
	// printk("open clk read addr for tpu_sys:%hX\n", readl(bmdi->cinfo.bar_info.bar_vaddr[MMAP_SYS]));
	// printk("open clk read addr for tpu:%hX\n", readl(bmdi->cinfo.bar_info.bar_vaddr[MMAP_REG] + 0x100 + 0x100));
	// printk("open clk read addr for gdma:%hX\n", readl(bmdi->cinfo.bar_info.bar_vaddr[MMAP_GDMA] + 0x1000 + 0x4));
	// SGTPUV8_modules_reset_init(bmdi);
	// SGTPUV8_modules_reset(bmdi);
	return 0;
}

static void bmdrv_hardware_deinit(struct bm_device_info *bmdi)
{
	SGTPUV8_modules_clk_disable(bmdi);
	SGTPUV8_modules_clk_deinit(bmdi);
}

static int bmdrv_chip_specific_init(struct bm_device_info *bmdi)
{
	int rc = 0;

	switch (bmdi->cinfo.chip_id) {
	case CHIP_ID:
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}


static void bmdrv_free_boot_loader_version(struct bm_device_info *bmdi)
{
	kfree(bmdi->cinfo.version.bl1_version);
	kfree(bmdi->cinfo.version.bl2_version);
	kfree(bmdi->cinfo.version.bl31_version);
	kfree(bmdi->cinfo.version.uboot_version);
	kfree(bmdi->cinfo.version.chip_version);
}

static int bmdrv_probe(struct platform_device *pdev)
{
	int rc;
	struct chip_info *cinfo;
	struct bm_device_info *bmdi;

	PR_TRACE("bmdrv: probe start\n");

	bmdi = devm_kzalloc(&pdev->dev, sizeof(struct bm_device_info), GFP_KERNEL);
	if (!bmdi)
		return -ENOMEM;

	rc = bmdrv_class_create();
	if (rc) {
		dev_err(&pdev->dev, "bmdrv create class failed!\n");
		return -1;
	}

	cinfo = &bmdi->cinfo;
	bmdi->dev_index = dev_count;

	bmdrv_cinfo_init(bmdi, pdev);

	rc = bmdrv_platform_init(bmdi, pdev);
	if (rc) {
		goto err_platform_init;
	}

	/* Create sysfs node (/sys/kernel/.../debug) */
	rc = kobject_init_and_add(&bmdi->kobj, &bmdrv_ktype, kernel_kobj, "%s-%d",
			cinfo->dev_name, bmdi->dev_index);
	if (rc) {
		dev_err(cinfo->device, "kobject_init_and_add fail %d\n", rc);
		kobject_put(&bmdi->kobj);
		goto err_kobject_init;
	}

	rc = bmdrv_software_init(bmdi);
	if (rc) {
		dev_err(cinfo->device, "device software init fail %d\n", rc);
		goto err_software_init;
	}

	rc = bmdrv_init_misc_info(pdev, bmdi);
	if (rc) {
		dev_err(cinfo->device, " misc info init fail %d\n", rc);
		goto err_hardware_init;
	}
	
	rc = bmdrv_hardware_init(bmdi);
	if (rc) {
		dev_err(cinfo->device, "device hardware init fail %d\n", rc);
		goto err_hardware_init;
	}

	// rc = bmdrv_init_irq(pdev);
	// if (rc) {
	// 	dev_err(cinfo->device, "device irq init fail %d\n", rc);
	// 	goto err_irq;
	// }

	rc = bmdrv_enable_attr(bmdi);
	if (rc)
		goto err_enable_attr;

	rc = bmdrv_chip_specific_init(bmdi);
	if (rc)
		goto err_chip_specific;

	rc = bmdrv_init_bmci(cinfo);
	if (rc) {
		dev_err(&pdev->dev, "bmci init failed!\n");
		goto err_chip_specific;
	}

	rc = bmdrv_ctrl_add_dev(bmci, bmdi);
	if (rc)
		goto err_ctrl_add_dev;

	bmdev_register_device(bmdi);

	dev_info(cinfo->device, "Card %d(type:%s) probe done\n", bmdi->dev_index,
			cinfo->chip_type);
	return 0;

err_ctrl_add_dev:
	bmdrv_remove_bmci();
err_chip_specific:
	bmdrv_disable_attr(bmdi);
err_enable_attr:
	bmdrv_free_irq(pdev);
// err_irq:
//	bmdrv_free_irq(pdev);
//	bmdrv_hardware_deinit(bmdi);
err_hardware_init:
	bmdrv_hardware_deinit(bmdi);
	bmdrv_software_deinit(bmdi);
err_software_init:
	kobject_del(&bmdi->kobj);
err_kobject_init:
	bmdrv_platform_deinit(bmdi, pdev);
err_platform_init:
	bmdrv_class_destroy();
	return rc;
}

static int bmdrv_remove(struct platform_device *pdev)
{
	struct bm_device_info *bmdi = platform_get_drvdata(pdev);

	if (bmdi == NULL)
		return 0;
	dev_info(bmdi->cinfo.device, "remove\n");

	bmdrv_free_boot_loader_version(bmdi);
	bmdev_unregister_device(bmdi);
	bmdrv_ctrl_del_dev(bmci, bmdi);
	bmdrv_disable_attr(bmdi);

	// bmdrv_free_irq(pdev);
	bmdrv_hardware_deinit(bmdi);
	bmdrv_software_deinit(bmdi);
	bmdrv_platform_deinit(bmdi, pdev);

	kobject_del(&bmdi->kobj);

	if (dev_count == 0) {
		bmdrv_remove_bmci();
		bmdrv_class_destroy();
	}
	return 0;
}


MODULE_DEVICE_TABLE(of, bmdrv_match_table);


static int bmdrv_tpu_suspend(struct device *dev)
{
	return 0;
}

static int bmdrv_tpu_resume(struct device *dev)
{
	struct bm_device_info *bmdi = dev_get_drvdata(dev);
	bmdrv_hardware_init(bmdi);
	return 0;
}


static SIMPLE_DEV_PM_OPS(tpu_pm_ops, bmdrv_tpu_suspend, bmdrv_tpu_resume);

static struct platform_driver bm_driver = {
	.probe = bmdrv_probe,
	.remove = bmdrv_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = BM_CDEV_NAME,
		.pm	= &tpu_pm_ops,
		.of_match_table = bmdrv_match_table,
	},
};

module_platform_driver(bm_driver);
MODULE_DESCRIPTION("Sophon Series Deep Learning Accelerator Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiao.wang@sophgo.com");
MODULE_VERSION(PROJECT_VER);
