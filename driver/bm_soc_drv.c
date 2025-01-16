#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/version.h>
#include "bm_common.h"
#include "bm_drv.h"
#include "bm_io.h"
// #include "bm_msgfifo.h"
#include "bm_irq.h"
#include "SGTPUV8_card.h"
#include "SGTPUV8_clkrst.h"

// TODO:
// extern uint32_t sophon_get_chip_id(void);
extern int dev_count;
extern struct bm_ctrl_info *bmci;
// static void __iomem *tpu_reg_base;

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
	writel(0xff, bar_info->bar_vaddr[1]);
	writel(readl(bar_info->bar_vaddr[2] + 0x100) | 0x1 , bar_info->bar_vaddr[2] + 0x100);
	PR_TRACE("read = %u\n",readl(bar_info->bar_vaddr[1]));
	printk("read smem = %u\n",readl(bar_info->bar_vaddr[3]));
	writel(0x1f, bar_info->bar_vaddr[3]);
	printk("read smem = %u after write\n",readl(bar_info->bar_vaddr[3]));
	PR_TRACE("bar_info->bar0_start=0x%llx,  bar_info->bar1_start=0x%llx\n",
					bar_info->bar_start[0], bar_info->bar_start[1]);
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
		cinfo->chip_type = "SGTPUV8";
#ifdef PLATFORM_PALLADIUM
		cinfo->platform = PALLADIUM;
#endif
#ifdef PLATFORM_ASIC
		cinfo->platform = DEVICE;
#endif
#ifdef PLATFORM_FPGA
		cinfo->platform = FPGA;
#endif
		// cinfo->bmdrv_clear_cdmairq = SGTPUV8_clear_cdmairq;
		// cinfo->bmdrv_clear_msgirq_by_core = SGTPUV8_clear_msgirq;
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
	misc_info->pcie_soc_mode = BM_DRV_SOC_MODE;
	misc_info->ddr_ecc_enable = 0;
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

	// io_init(bmdi);

	cinfo->tpu_core_num = 1;
	cinfo->device->dma_mask = &dummy_dma_mask;
	cinfo->device->coherent_dma_mask = dummy_dma_mask;
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
	cinfo->device->dma_coherent = false;
#else
	arch_setup_dma_ops(cinfo->device, 0, 0, NULL, false);
#endif
	// tpu_reg_base = ioremap(TPU_REG_BASE, TPU_REG_SIZE);
	// if (!tpu_reg_base) {
	// 		return -ENOMEM;
	// }
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
	// u32 val;

	switch (bmdi->cinfo.chip_id) {
	case CHIP_ID:
		SGTPUV8_modules_clk_init(bmdi);
		SGTPUV8_modules_clk_enable(bmdi);
		SGTPUV8_modules_reset_init(bmdi);
		SGTPUV8_modules_reset(bmdi);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void bmdrv_hardware_deinit(struct bm_device_info *bmdi)
{
	switch (bmdi->cinfo.chip_id) {
	case CHIP_ID:
		SGTPUV8_modules_clk_disable(bmdi);
		SGTPUV8_modules_clk_deinit(bmdi);
		break;
	default:
		break;
	}
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

static int bmdrv_get_boot_loader_version(struct bm_device_info *bmdi)
{
	int ret;

	bmdi->cinfo.version.bl1_version = kmalloc(BL1_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl1_version, BL1_VERSION_BASE, BL1_VERSION_SIZE);
	if (ret)
		goto err_bl1_version;

	bmdi->cinfo.version.bl2_version = kmalloc(BL2_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl2_version, BL2_VERSION_BASE, BL2_VERSION_SIZE);
	if (ret)
		goto err_bl2_version;

	bmdi->cinfo.version.bl31_version = kmalloc(BL31_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl31_version, BL31_VERSION_BASE, BL31_VERSION_SIZE);
	if (ret)
		goto err_bl31_version;

	bmdi->cinfo.version.uboot_version = kmalloc(UBOOT_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.uboot_version, UBOOT_VERSION_BASE, UBOOT_VERSION_SIZE);
	if (ret)
		goto err_uboot_version;

	bmdi->cinfo.version.chip_version = kmalloc(CHIP_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.chip_version, CHIP_VERSION_BASE, CHIP_VERSION_SIZE);
	if (ret)
		goto err_chip_version;

	return ret;

err_uboot_version:
	kfree(bmdi->cinfo.version.uboot_version);
err_bl31_version:
	kfree(bmdi->cinfo.version.bl31_version);
err_bl2_version:
	kfree(bmdi->cinfo.version.bl2_version);
err_bl1_version:
	kfree(bmdi->cinfo.version.bl1_version);
err_chip_version:
	kfree(bmdi->cinfo.version.chip_version);
	return -EBUSY;
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
	
	// rc = bmdrv_hardware_init(bmdi);
	// if (rc) {
	// 	dev_err(cinfo->device, "device hardware init fail %d\n", rc);
	// 	goto err_hardware_init;
	// }

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

	// bm_monitor_thread_init(bmdi);

	rc = bmdrv_ctrl_add_dev(bmci, bmdi);
	if (rc)
		goto err_ctrl_add_dev;

	bmdev_register_device(bmdi);

	// rc = bmdrv_get_boot_loader_version(bmdi);
	if (rc)
		goto err_get_boot_loader_version;

	// pwr_ctrl_set(bmdi, NULL);

	dev_info(cinfo->device, "Card %d(type:%s) probe done\n", bmdi->dev_index,
			cinfo->chip_type);
	return 0;

err_get_boot_loader_version:
	bmdrv_ctrl_del_dev(bmci, bmdi);
err_ctrl_add_dev:
	bmdrv_remove_bmci();
err_chip_specific:
	bmdrv_disable_attr(bmdi);
err_enable_attr:
	bmdrv_free_irq(pdev);
// err_irq:
// 	bmdrv_free_irq(pdev);
//	bmdrv_hardware_deinit(bmdi);
err_hardware_init:
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

	// kill_user_thread(bmdi->bmcpu_thread_id);
	if (bmdi == NULL)
		return 0;
	dev_info(bmdi->cinfo.device, "remove\n");

	bmdrv_free_boot_loader_version(bmdi);
	bmdev_unregister_device(bmdi);
	bm_monitor_thread_deinit(bmdi);
	bmdrv_ctrl_del_dev(bmci, bmdi);
	bmdrv_disable_attr(bmdi);

	// bmdrv_free_irq(pdev);
	// bmdrv_fw_unload(bmdi);
	// bmdrv_hardware_deinit(bmdi);
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

#ifdef CONFIG_PM_SLEEP
static int bmdrv_tpu_suspend(struct device *dev)
{
	struct bm_device_info *bmdi = dev_get_drvdata(dev);
	u32 pm_status = 0;
	u32 timeout_cnt = 0;

	gp_reg_write_enh(bmdi, GP_REG_PM_OFFSET, 1);
	while(!(pm_status & 0x2) && (timeout_cnt < 100)) {
		usleep_range(10000,20000);
		pm_status = gp_reg_read_enh(bmdi, GP_REG_PM_OFFSET);
		timeout_cnt++;
	}
	pr_err("bmdrv_tpu_suspend(%d)sus.\n", timeout_cnt);
	return 0;
}

static int bmdrv_tpu_resume(struct device *dev)
{
	struct bm_device_info *bmdi = dev_get_drvdata(dev);
	u32 pm_status = gp_reg_read_enh(bmdi, GP_REG_PM_OFFSET) & (~0x01);
	u32 timeout_cnt = 0;

	gp_reg_write_enh(bmdi, GP_REG_PM_OFFSET, pm_status); //set bit0 to 0
	while(!gp_reg_read_enh(bmdi, GP_REG_PM_OFFSET) && (timeout_cnt < 100)) {
		usleep_range(10000,20000);
		timeout_cnt++;
	}
	pr_err("bmdrv_tpu_resume(%d) sus.\n", timeout_cnt);
	return 0;
}
#endif

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
