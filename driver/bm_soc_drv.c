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
#include "bm_fw.h"
#include "bm_msgfifo.h"
#include "bm_irq.h"
#include "i2c.h"
#include "bm1684_card.h"
#include "bm1682_card.h"
#include "bm1684_smmu.h"
#include "bm1682_smmu.h"
#include "bm1684_clkrst.h"
#include "bm1684_cdma.h"

extern uint32_t sophon_get_chip_id(void);
extern int dev_count;
extern struct bm_ctrl_info *bmci;

static struct kobj_type bmdrv_ktype = {
	NULL
};

static int platform_init_bar_address(struct platform_device *pdev, struct chip_info *cinfo)
{
	struct resource *res;
	struct bm_bar_info *bar_info = &cinfo->bar_info;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -EINVAL;
	bar_info->bar0_len = resource_size(res);
	bar_info->bar0_start = res->start;
	bar_info->bar0_dev_start = res->start;
	bar_info->bar0_vaddr = of_iomap(pdev->dev.of_node, 0);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res == NULL)
		return -EINVAL;
	bar_info->bar1_len = resource_size(res);
	bar_info->bar1_start = res->start;
	bar_info->bar1_dev_start = res->start;
	bar_info->bar1_vaddr = of_iomap(pdev->dev.of_node, 1);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res == NULL)
		return -EINVAL;
	bar_info->bar2_len = resource_size(res);
	bar_info->bar2_start = res->start;
	bar_info->bar2_dev_start = res->start;
	bar_info->bar2_vaddr = of_iomap(pdev->dev.of_node, 2);
	return 0;
}
static void bmdrv_set_fw_mode(struct bm_device_info *bmdi, struct platform_device *pdev)
{
	struct device_node *tpu_node;
	u32 val;

	tpu_node = of_node_get(pdev->dev.of_node);
	of_property_read_u32(tpu_node, "sophgon_arm9_fw_mode", &val);
	pr_info("arm9 firmware mode from dtb is 0x%x\n", val);
	//TODO:mix mode dts not modify, make soc mode setup

	gp_reg_write_enh(bmdi, GP_REG_ARM9_FW_MODE, val);
	pr_info("set arm9 firmware mode is %d\n", val);
}
static int bmdrv_cinfo_init(struct bm_device_info *bmdi, struct platform_device *pdev)
{
	struct chip_info *cinfo = &bmdi->cinfo;
	struct device_node *tpu_node;
        u32 chip_id = 0;

	tpu_node = of_node_get(pdev->dev.of_node);
	if (of_device_is_compatible(tpu_node, "bitmain,bmdnn")) {
		cinfo->chip_id = 0x1682;
	} else if (of_device_is_compatible(tpu_node, "bitmain,tpu-1684")) {
                chip_id = sophon_get_chip_id();
		cinfo->chip_id = chip_id >> 16;
	} else {
		dev_err(&pdev->dev, "invalid device\n");
		return -1;
	}

	switch (cinfo->chip_id) {
	case 0x1682:
		cinfo->bmdrv_start_arm9 = bm1682_start_arm9;
		cinfo->bmdrv_stop_arm9 = bm1682_stop_arm9;

		cinfo->bm_reg = &bm_reg_1682;
		cinfo->share_mem_size = 1 << 10;
		cinfo->chip_type = "bm1682";
		cinfo->platform = DEVICE;
		cinfo->bmdrv_clear_cdmairq = bm1682_clear_cdmairq;
		cinfo->bmdrv_clear_msgirq = bm1682_clear_msgirq;
		cinfo->bmdrv_pending_msgirq_cnt = bm1682_pending_msgirq_cnt;
		break;
	case 0x1684:
		cinfo->bmdrv_start_arm9 = bm1684_start_arm9;
		cinfo->bmdrv_stop_arm9 = bm1684_stop_arm9;

		cinfo->bm_reg = &bm_reg_1684;
		cinfo->share_mem_size = 1 << 12;
		cinfo->chip_type = "bm1684";
#ifdef PLATFORM_PALLADIUM
		cinfo->platform = PALLADIUM;
#endif
#ifdef PLATFORM_ASIC
		cinfo->platform = DEVICE;
#endif
#ifdef PLATFORM_FPGA
		cinfo->platform = FPGA;
#endif
		cinfo->bmdrv_clear_cdmairq = bm1684_clear_cdmairq;
		cinfo->bmdrv_clear_msgirq = bm1684_clear_msgirq;
		cinfo->bmdrv_pending_msgirq_cnt = bm1684_pending_msgirq_cnt;
		break;
	case 0x1686:
		cinfo->bmdrv_start_arm9 = bm1684x_start_a53lite;
		cinfo->bmdrv_stop_arm9 = bm1684x_stop_a53lite;

		cinfo->bm_reg = &bm_reg_1684x;
		cinfo->share_mem_size = 1 << 12;
		cinfo->chip_type = "bm1684x";
#ifdef PLATFORM_PALLADIUM
		cinfo->platform = PALLADIUM;
#endif
#ifdef PLATFORM_ASIC
		cinfo->platform = DEVICE;
#endif
#ifdef PLATFORM_FPGA
		cinfo->platform = FPGA;
#endif
		cinfo->bmdrv_clear_cdmairq = bm1684_clear_cdmairq;
		cinfo->bmdrv_clear_msgirq = bm1684_clear_msgirq;
		cinfo->bmdrv_pending_msgirq_cnt = bm1684_pending_msgirq_cnt;
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
	case 0x1682:
		misc_info->chipid_bit_mask = BM1682_CHIPID_BIT_MASK;
		break;
	case 0x1684:
		misc_info->chipid_bit_mask = BM1684_CHIPID_BIT_MASK;
		break;
	case 0x1686:
		misc_info->chipid_bit_mask = BM1684X_CHIPID_BIT_MASK;
		break;
	default:
		sprintf(bmdi->cinfo.dev_name, "%s", "unknown device");
		return -EINVAL;
	}

	misc_info->chipid = bmdi->cinfo.chip_id;
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

	io_init(bmdi);

	cinfo->device->dma_mask = &dummy_dma_mask;
	cinfo->device->coherent_dma_mask = dummy_dma_mask;
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,4,0)
	cinfo->device->dma_coherent = false;
#else
	arch_setup_dma_ops(cinfo->device, 0, 0, NULL, false);
#endif
	platform_set_drvdata(pdev, bmdi);

	return rc;
}

static void bmdrv_platform_deinit(struct bm_device_info *bmdi, struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
}

static int bmdrv_hardware_init(struct bm_device_info *bmdi)
{
	u32 val;

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		bm1682_init_iommu(&bmdi->memcpy_info.iommuctl, bmdi->parent);
		bm1682_top_init(bmdi);
		break;
	case 0x1684:
	case 0x1686:
		bm1684_modules_clk_init(bmdi);
		bm1684_modules_clk_enable(bmdi);
		bm1684_modules_reset_init(bmdi);
		bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(bmdi);
		bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(bmdi);
		bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(bmdi);
		bm1684_modules_reset(bmdi);
		bm1684_init_iommu(&bmdi->memcpy_info.iommuctl, bmdi->parent);
		if (bmdi->cinfo.chip_id == 0x1686 ) {
			bm1686_modules_clk_init(bmdi);
			bm1686_modules_clk_enable(bmdi);
			val = bdc_reg_read(bmdi, 0x100);
			bdc_reg_write(bmdi, 0x100, val| 0x1);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void bmdrv_hardware_deinit(struct bm_device_info *bmdi)
{
	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		bm1682_deinit_iommu(&bmdi->memcpy_info.iommuctl);
		break;
	case 0x1684:
	case 0x1686:
		bm1684_deinit_iommu(&bmdi->memcpy_info.iommuctl);
		bm1684_modules_clk_disable(bmdi);
		bm1684_modules_clk_deinit(bmdi);
		if (bmdi->cinfo.chip_id == 0x1686 ) {
			bm1686_modules_clk_disable(bmdi);
			bm1686_modules_clk_deinit(bmdi);
		}
		break;
	default:
		break;
	}
}

static int bmdrv_chip_specific_init(struct bm_device_info *bmdi)
{
	int rc = 0;

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		break;
	case 0x1684:
		bm1684_l2_sram_init(bmdi);
		break;
	case 0x1686:
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
	if(ret)
		return -EBUSY;

	bmdi->cinfo.version.bl2_version = kmalloc(BL2_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl2_version, BL2_VERSION_BASE, BL2_VERSION_SIZE);
	if(ret)
		return -EBUSY;

	bmdi->cinfo.version.bl31_version = kmalloc(BL31_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl31_version, BL31_VERSION_BASE, BL31_VERSION_SIZE);
	if(ret)
		return -EBUSY;

	bmdi->cinfo.version.uboot_version = kmalloc(UBOOT_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.uboot_version, UBOOT_VERSION_BASE, UBOOT_VERSION_SIZE);
	if(ret)
		return -EBUSY;

	return ret;
}

static void bmdrv_free_boot_loader_version(struct bm_device_info *bmdi)
{
	kfree(bmdi->cinfo.version.bl1_version);
	kfree(bmdi->cinfo.version.bl2_version);
	kfree(bmdi->cinfo.version.bl31_version);
	kfree(bmdi->cinfo.version.uboot_version);
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
	if (rc)
		goto err_platform_init;

	/* Create sysfs node (/sys/kernel/bm1684-0/debug) */
	rc = kobject_init_and_add(&bmdi->kobj, &bmdrv_ktype, kernel_kobj, "%s-%d",
			cinfo->dev_name, bmdi->dev_index);
	if (rc) {
		dev_err(cinfo->device, "kobject_init_and_add fail %d\n", rc);
		kobject_put(&bmdi->kobj);
		goto err_software_init;
	}

	rc = bmdrv_software_init(bmdi);
	if (rc) {
		dev_err(cinfo->device, "device software init fail %d\n", rc);
		goto err_software_init;
	}

	rc = bmdrv_init_misc_info(pdev, bmdi);
	if (rc) {
		dev_err(cinfo->device, " misc info init fail %d\n", rc);
		goto err_software_init;
	}

	rc = bmdrv_hardware_init(bmdi);
	if (rc) {
		dev_err(cinfo->device, "device hardware init fail %d\n", rc);
		goto err_hardware_init;
	}

	bmdrv_set_fw_mode(bmdi, pdev);
	if (bmdrv_fw_load(bmdi, NULL, NULL)) {
		pr_err("bmdrv: firmware load failed!\n");
		goto err_fw;
	}

	rc = bmdrv_init_irq(pdev);
	if (rc) {
		dev_err(cinfo->device, "device irq init fail %d\n", rc);
		goto err_irq;
	}
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

	bm_monitor_thread_init(bmdi);

	bmdev_register_device(bmdi);

	rc = bmdrv_get_boot_loader_version(bmdi);
	if (rc)
		goto err_ctrl_add_dev;

	dev_info(cinfo->device, "Card %d(type:%s) probe done\n", bmdi->dev_index,
			cinfo->chip_type);
	return 0;

err_ctrl_add_dev:
	bmdrv_remove_bmci();
err_chip_specific:
	bmdrv_disable_attr(bmdi);
err_enable_attr:
	bmdrv_free_irq(pdev);
err_irq:
err_fw:
	bmdrv_fw_unload(bmdi);
	bmdrv_hardware_deinit(bmdi);
err_hardware_init:
	bmdrv_software_deinit(bmdi);
err_software_init:
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
	bm_monitor_thread_deinit(bmdi);
	bmdrv_ctrl_del_dev(bmci, bmdi);
	bmdrv_disable_attr(bmdi);

	bmdrv_free_irq(pdev);
	bmdrv_fw_unload(bmdi);
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

static const struct of_device_id bmdrv_match_table[] = {
	{.compatible = "bitmain,tpu-1682"},
	{.compatible = "bitmain,tpu-1684"},
	{},
};

MODULE_DEVICE_TABLE(of, bmdrv_match_table);

static struct platform_driver bm_driver = {
	.probe = bmdrv_probe,
	.remove = bmdrv_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = BM_CDEV_NAME,
		.of_match_table = bmdrv_match_table,
	},
};

module_platform_driver(bm_driver);
MODULE_DESCRIPTION("Sophon Series Deep Learning Accelerator Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiao.wang@sophgo.com");
MODULE_VERSION(PROJECT_VER);
