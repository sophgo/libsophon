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
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm_drv.h"
#include "bm_io.h"
#include "bm_fw.h"
#include "bm1688_msgfifo.h"
#include "bm_msgfifo.h"
#include "bm_irq.h"
#include "i2c.h"
#include "bm1688_card.h"
#include "bm1684_card.h"
#include "bm1682_card.h"
#include "bm1684_smmu.h"
#include "bm1682_smmu.h"
#include "bm1688_clkrst.h"
#include "bm1684_clkrst.h"
#include "bm1688_cdma.h"
#include "bm1684_cdma.h"
#include "bm1688_base64.h"

struct proc_dir_entry *bmdi_folder;

// TODO:
// extern uint32_t sophon_get_chip_id(void);
extern int dev_count;
extern struct bm_ctrl_info *bmci;
u32 c906_park_0_l, c906_park_0_h, c906_park_1_l, c906_park_1_h;

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
	if (bmdi->cinfo.chip_id == 0x1686a200)
		of_property_read_u32(tpu_node, "sophgo_c906_fw_mode", &val);
	else
		of_property_read_u32(tpu_node, "sophgon_arm9_fw_mode", &val);
	pr_info("C906 firmware mode from dtb is 0x%x\n", val);
	//TODO:mix mode dts not modify, make soc mode setup

	if (bmdi->cinfo.chip_id == 0x1686a200)
		gp_reg_write_enh(bmdi, GP_REG_C906_FW_MODE, val);
	else
		gp_reg_write_enh(bmdi, GP_REG_ARM9_FW_MODE, val);
	pr_info("set C906 firmware mode is %d\n", val);
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
		// TODO:
                // chip_id = sophon_get_chip_id();
		cinfo->chip_id = 0x1686;
		cinfo->chip_id = chip_id >> 16;
	} else if (of_device_is_compatible(tpu_node, "cvitek,tpu")) {
		cinfo->chip_id = 0x1686a200;
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
		cinfo->tpu_core_num = 1;
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
		cinfo->tpu_core_num = 1;
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
		cinfo->tpu_core_num = 1;
		break;
	case 0x1686a200:
		cinfo->bmdrv_start_arm9 = bm1688_start_c906;
		cinfo->bmdrv_stop_arm9 = bm1688_stop_c906;

		cinfo->bm_reg = &bm_reg_bm1688;
		cinfo->share_mem_size = 1 << 12;
		cinfo->chip_type = "bm1688";
#ifdef PLATFORM_PALLADIUM
		cinfo->platform = PALLADIUM;
#endif
#ifdef PLATFORM_ASIC
		cinfo->platform = DEVICE;
#endif
#ifdef PLATFORM_FPGA
		cinfo->platform = FPGA;
#endif
		cinfo->bmdrv_clear_cdmairq0 = bm1688_clear_cdmairq0;
		cinfo->bmdrv_clear_cdmairq1 = bm1688_clear_cdmairq1;
		cinfo->bmdrv_clear_msgirq_by_core = bm1688_clear_msgirq;
		cinfo->bmdrv_pending_msgirq_cnt = bm1688_pending_msgirq_cnt;
		cinfo->tpu_core_num = 2;
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
	case 0x1686a200:
		misc_info->chipid_bit_mask = BM1688_CHIPID_BIT_MASK;
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

	io_init(bmdi);

	if (cinfo->chip_id == 0x1686a200)
		if (((otp_reg_read(bmdi, 0x2014) & 0x1c0) >> 6) == 0b011)
			cinfo->tpu_core_num = 1;
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
	struct chip_info *cinfo = &bmdi->cinfo;

	platform_set_drvdata(pdev, NULL);
	cinfo->device->dma_mask = NULL;
	cinfo->device->coherent_dma_mask = 0;
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
	case 0x1686a200:
		bm1688_modules_clk_init(bmdi);
		bm1688_modules_clk_enable(bmdi);
		bm1688_modules_reset_init(bmdi);
		bm1688_modules_reset(bmdi);
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
	case 0x1686a200:
		bm1688_modules_clk_disable(bmdi);
		bm1688_modules_clk_deinit(bmdi);
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
	case 0x1686a200:
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
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl1_version, BL1_VERSION_BASE, BL1_VERSION_SIZE, false);
	if (ret)
		goto err_bl1_version;

	bmdi->cinfo.version.bl2_version = kmalloc(BL2_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl2_version, BL2_VERSION_BASE, BL2_VERSION_SIZE, false);
	if (ret)
		goto err_bl2_version;

	bmdi->cinfo.version.bl31_version = kmalloc(BL31_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.bl31_version, BL31_VERSION_BASE, BL31_VERSION_SIZE, false);
	if (ret)
		goto err_bl31_version;

	bmdi->cinfo.version.uboot_version = kmalloc(UBOOT_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.uboot_version, UBOOT_VERSION_BASE, UBOOT_VERSION_SIZE, false);
	if (ret)
		goto err_uboot_version;

	bmdi->cinfo.version.chip_version = kmalloc(CHIP_VERSION_SIZE, GFP_KERNEL);
	ret = bmdev_memcpy_d2s_internal(bmdi, bmdi->cinfo.version.chip_version, CHIP_VERSION_BASE, CHIP_VERSION_SIZE, false);
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

static int bmdi_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;

	seq_printf(m, "libsophon git version:%s\n", GIT_VER_STRING);
	seq_printf(m, "status:%d\n", bmdi->status);

	return 0;
}

static int seq_bmdi_open(struct inode *inode, struct file *file)
{
	return single_open(file, bmdi_proc_show, PDE_DATA(inode));
}

static const struct proc_ops bmdi_proc_ops = {
	.proc_open = seq_bmdi_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static int lib_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct bmcpu_lib *lib_node;
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;
	char hex_str[33];
	int i = 0;

	seq_puts(m, "lib_name                      md5                                refcount    core_id\n");

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		lib_node = lib_temp;
		for (i = 0; i < 16; i++) {
			sprintf(hex_str + (i * 2), "%02x", lib_node->md5[i]);
		}
		hex_str[32] = '\0';
		seq_printf(m, "%-30s%-35s%-12d%-7d\n", lib_node->lib_name, hex_str,
					lib_node->refcount, lib_node->core_id);
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	return 0;
}

static int seq_lib_open(struct inode *inode, struct file *file)
{
	return single_open(file, lib_proc_show, PDE_DATA(inode));
}

static const struct proc_ops lib_proc_ops = {
	.proc_open = seq_lib_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static int api_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	struct api_fifo_entry api_entry;
	struct bm_api_info *api_info;
	int count = 0;
	int core_num = 0;
	int api_num = 0;
	int i = 0;
	u64 glob_api_num = 0;
	u64 sync_api_num = 0;

	core_num = base_get_core_num(bmdi);
	for (i = 0; i < core_num; i++) {
		if (i == 0) {
			sync_api_num = bmdi->bm_sync_api_seq;
			glob_api_num = bmdi->bm_send_api_seq;
		} else if (i == 1) {
			sync_api_num = bmdi->bm_sync_api_seq1;
			glob_api_num = bmdi->bm_send_api_seq1;
		}
		api_info = &bmdi->api_info[i][GP_REG_MESSAGE_WP_CHANNEL_XPU];
		api_num = kfifo_len(&api_info->api_fifo) / API_ENTRY_SIZE;
		seq_printf(m, "there is %d apis in core_%d fifo\n", api_num, i);
		seq_printf(m, "%lld apis has send, %lld has sync\n", glob_api_num, sync_api_num);
		if (api_num > 0) {
			count = kfifo_out(&api_info->api_fifo, &api_entry, API_ENTRY_SIZE);
			if (count < API_ENTRY_SIZE) {
				pr_err("core_id %d: The dequeue entry size %d is not correct!\n", i, count);
				break;
			}
			seq_printf(m, "first api info:\n    api_id:%d\n", api_entry.api_id);
			seq_printf(m, "    global_api_seq:%lld\n", api_entry.global_api_seq);
		}
		seq_puts(m, "\n");
	}

	return 0;
}

static int seq_api_open(struct inode *inode, struct file *file)
{
	return single_open(file, api_proc_show, PDE_DATA(inode));
}

static const struct proc_ops api_proc_ops = {
	.proc_open = seq_api_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static int reg_proc_show(struct seq_file *m, void *v)
{
	struct bm_device_info *bmdi = m->private;
	int addr = 0;
	int value = 0;
	int core_num = 0;
	int core_offset = BD_ENGINE_TPU1_OFFSET;
	void *tiu_reg_base_addr = bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr;
	void *gdma_reg_base_addr = bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr;
	int read_count = 128;
	int i, j;

	core_num = base_get_core_num(bmdi);
	for (i = 0; i < core_num; i++) {
		for (j = 0; j < read_count; j++) {
			addr = (i * core_offset) + (j * 4) + *(u32 *)tiu_reg_base_addr;
			value = bm_read32(bmdi, addr);
			seq_printf(m, "tiu core=%d addr=0x%x, value=0x%x\n", i, addr, value);
		}
	}

	for (i = 0; i < core_num; i++) {
		for (j = 0; j < read_count; j++) {
			addr = (i * core_offset) + (j * 4) + *(u32 *)gdma_reg_base_addr;
			value = bm_read32(bmdi, addr);
			seq_printf(m, "gdma core=%d addr=0x%x, value=0x%x\n", i, addr, value);
		}
	}

	return 0;
}

static int seq_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, reg_proc_show, PDE_DATA(inode));
}

static const struct proc_ops reg_proc_ops = {
	.proc_open = seq_reg_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static int bmdrv_probe(struct platform_device *pdev)
{
	int rc;
	struct chip_info *cinfo;
	struct bm_device_info *bmdi;
	struct proc_dir_entry *proc_bmdi;
	struct proc_dir_entry *proc_lib;
	struct proc_dir_entry *proc_api;
	struct proc_dir_entry *proc_reg;
	const char *name;

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

	bm_monitor_thread_init(bmdi);

	//bm_pm_thread_init(bmdi);

	rc = bmdrv_ctrl_add_dev(bmci, bmdi);
	if (rc)
		goto err_ctrl_add_dev;

	bmdev_register_device(bmdi);

	rc = bmdrv_get_boot_loader_version(bmdi);
	if (rc)
		goto err_get_boot_loader_version;

	// pwr_ctrl_set(bmdi, NULL);

	name = pdev->dev.of_node->full_name;
	bmdi_folder = proc_mkdir(name, NULL);
	if (!bmdi_folder)
		dev_err(&pdev->dev, "Error creating bmdi proc folder entry\n");

	proc_bmdi = proc_create_data("bmdi_base_info", 0664, bmdi_folder, &bmdi_proc_ops, bmdi);
	if (!proc_bmdi)
		dev_err(&pdev->dev, "Create bmdi base info proc failed!\n");

	proc_lib = proc_create_data("bmdi_lib_info", 0664, bmdi_folder, &lib_proc_ops, bmdi);
	if (!proc_lib)
		dev_err(&pdev->dev, "Create bmdi lib info proc failed!\n");

	proc_api = proc_create_data("bmdi_api_info", 0664, bmdi_folder, &api_proc_ops, bmdi);
	if (!proc_api)
		dev_err(&pdev->dev, "Create bmdi api info proc failed!\n");

	proc_reg = proc_create_data("tpu_reg_info", 0664, bmdi_folder, &reg_proc_ops, bmdi);
	if (!proc_reg)
		dev_err(&pdev->dev, "Create bmdi reg info proc failed!\n");

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
err_irq:
err_fw:
	bmdrv_fw_unload(bmdi);
	bmdrv_hardware_deinit(bmdi);
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

	if (bmdi == NULL)
		return 0;
	dev_info(bmdi->cinfo.device, "remove\n");

	bmdrv_free_boot_loader_version(bmdi);
	bmdev_unregister_device(bmdi);
	//bm_pm_thread_deinit(bmdi);
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
	remove_proc_entry("bmdi_base_info", bmdi_folder);
	remove_proc_entry("bmdi_lib_info", bmdi_folder);
	remove_proc_entry("bmdi_api_info", bmdi_folder);
	remove_proc_entry("tpu_reg_info", bmdi_folder);
	remove_proc_entry("bmtpu", NULL);

	return 0;
}

static const struct of_device_id bmdrv_match_table[] = {
	{.compatible = "bitmain,tpu-1682"},
	{.compatible = "bitmain,tpu-1684"},
	{.compatible = "cvitek,tpu"},
	{},
};

MODULE_DEVICE_TABLE(of, bmdrv_match_table);

#ifdef CONFIG_PM_SLEEP
static int bmdrv_tpu_suspend(struct device *dev)
{
	struct bm_device_info *bmdi = dev_get_drvdata(dev);
	u32 pm_status0 = 0x1;
	u32 pm_status1 = 0x1;
	u32 timeout_cnt = 0;
	char *name;

	name = base_get_chip_id(bmdi);
	if (!strcmp(name, "BM1688-SOC")) {
		gp_reg_write_enh(bmdi, GP_REG_PM_0_OFFSET, 0x1);
		gp_reg_write_enh(bmdi, GP_REG_PM_1_OFFSET, 0x1);
	} else if (!strcmp(name, "CV186AH-SOC")) {
		gp_reg_write_enh(bmdi, GP_REG_PM_0_OFFSET, 0x1);
	} else {
		pr_err("illeage chip name: %s\n", name);
	}

	while ((pm_status0 != 0 || pm_status1 != 0) && (timeout_cnt < 100)) {
		usleep_range(10000,20000);
		pm_status0 = gp_reg_read_enh(bmdi, GP_REG_PM_0_OFFSET);
		pm_status1 = gp_reg_read_enh(bmdi, GP_REG_PM_1_OFFSET);
		timeout_cnt += 1;
	}

	if (timeout_cnt >= 100) {
		pr_err("bmdrv_tpu_suspend timeout  %d.\n", timeout_cnt);
		return -1;
	}

	c906_park_0_l = gp_reg_read_enh(bmdi, GP_REG_C906_0_ADDR_L);
	c906_park_0_h = gp_reg_read_enh(bmdi, GP_REG_C906_0_ADDR_H);
	c906_park_1_l = gp_reg_read_enh(bmdi, GP_REG_C906_1_ADDR_L);
	c906_park_1_h = gp_reg_read_enh(bmdi, GP_REG_C906_1_ADDR_H);

	//bm1688_tpu_clk_disable(bmdi);
	bm1688_tc906b_clk_disable(bmdi);
	//bm1688_gdma_clk_disable(bmdi);
	pr_err("bmdrv_tpu_suspend(%d)sus.\n", timeout_cnt);

	return 0;
}

static int bmdrv_tpu_resume(struct device *dev)
{
	struct bm_device_info *bmdi = dev_get_drvdata(dev);

	//bm1688_tpu_clk_enable(bmdi);
	bm1688_tc906b_clk_enable(bmdi);
	//bm1688_gdma_clk_enable(bmdi);

	bm1688_resume_tpu(bmdi, c906_park_0_l, c906_park_0_h, c906_park_1_l, c906_park_1_h);

	pr_err("bmdrv_tpu_resume done\n");
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
