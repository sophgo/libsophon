#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/uaccess.h>
#include <linux/irqflags.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/version.h>
#include "bm_common.h"
#include "bm_boot_info.h"
#include "bm_pcie.h"
#include "bm_fw.h"
#include "bm_drv.h"
#include "bm_ddr.h"
#include "bm_irq.h"
#include "bm_ctl.h"
#include "gpio.h"
#include "bm_cdma.h"
#include "uart.h"
#include "i2c.h"
#include "spi.h"
#include "vpu/vpu.h"
#include "bm_msgfifo.h"
#include "bm_memcpy.h"
#include "vpp/vpp_platform.h"
#include "bm1684/bm1684_jpu.h"
#include "bm1684_clkrst.h"
#include "bm1684/bm1684_irq.h"
#include "bm1682/bm1682_irq.h"
#include "bm1684/bm1684_card.h"
#include "bm1684/bm1684_reg.h"
#include "bm1684/bm1684_flash.h"
#include "bm1684/bm1684_smbus.h"
#include "bm1682/bm1682_card.h"
#include "bm1682/bm1682_smmu.h"
#include "bm1684/bm1684_ce.h"
#include "bm_card.h"
#include "bm_napi.h"
#include "sg_comm.h"

#define IOMMU_ADDR_BIT_NUM (40)

static int module_init;
static int module_exit;
extern int dev_count;
extern struct bm_ctrl_info *bmci;

static struct bm_pcie_record bm_record[BM_PCIE_MAX_CHIP_NUM];
static int bm_devid_inited;

#ifdef PCIE_MODE_ENABLE_CPU
typedef struct bm_api_reset_cpu {
	u32 flag;
} __attribute__((packed)) bm_api_reset_cpu_t;
#endif

void bmdrv_modules_request_irq(struct bm_device_info *bmdi);
int bmdrv_reset_bmcpu(struct bm_device_info *bmdi);

static int bmdrv_pci_init_bar_address(struct pci_dev *pdev, struct chip_info *cinfo)
{
	int rc;

	rc = pci_request_regions(pdev, cinfo->dev_name);
	if (rc) {
		dev_err(&pdev->dev, "cannot reserve memory region\n");
		goto err_request_regions;
	}
	cinfo->bar_info.bar0_start = pci_resource_start(pdev, 0);
	if (cinfo->bar_info.bar0_start != 0) {
		cinfo->bar_info.bar0_len = pci_resource_len(pdev, 0);
		cinfo->bar_info.bar0_vaddr = pci_iomap(pdev, 0, 0);
		if (!cinfo->bar_info.bar0_vaddr) {
			rc = -ENOMEM;
			dev_err(&pdev->dev, "iomap for bar0 failed\n");
			goto err_iomap;
		}
	}
	cinfo->bar_info.bar1_start = pci_resource_start(pdev, 1);
	if (cinfo->bar_info.bar1_start != 0) {
		cinfo->bar_info.bar1_len = pci_resource_len(pdev, 1);
		cinfo->bar_info.bar1_vaddr = pci_iomap(pdev, 1, 0);
		if (!cinfo->bar_info.bar1_vaddr) {
			rc = -ENOMEM;
			dev_err(&pdev->dev, "iomap for bar1 failed\n");
			goto err_iomap1;
		}
	}
	cinfo->bar_info.bar2_start = pci_resource_start(pdev, 2);
	if (cinfo->bar_info.bar2_start != 0) {
		cinfo->bar_info.bar2_len = pci_resource_len(pdev, 2);
		cinfo->bar_info.bar2_vaddr = pci_iomap(pdev, 2, 0);
		if (!cinfo->bar_info.bar2_vaddr) {
			rc = -ENOMEM;
			dev_err(&pdev->dev, "iomap for bar2 failed\n");
			goto err_iomap2;
		}
	}
	cinfo->bar_info.bar4_start = pci_resource_start(pdev, 4);
	if (cinfo->bar_info.bar4_start != 0) {
		cinfo->bar_info.bar4_len = pci_resource_len(pdev, 4);
		cinfo->bar_info.bar4_vaddr = pci_iomap(pdev, 4, 0);
		if (!cinfo->bar_info.bar4_vaddr) {
			rc = -ENOMEM;
			dev_err(&pdev->dev, "iomap for bar4 failed\n");
			goto err_iomap4;
		}
	}
	return 0;
err_iomap4:
	pci_iounmap(pdev, cinfo->bar_info.bar2_vaddr);
err_iomap2:
	pci_iounmap(pdev, cinfo->bar_info.bar1_vaddr);
err_iomap1:
	pci_iounmap(pdev, cinfo->bar_info.bar0_vaddr);
err_iomap:
	pci_release_regions(pdev);
err_request_regions:
	return rc;
}

static int bmdrv_pci_release_bar_addr(struct pci_dev *pdev, struct chip_info *cinfo)
{
	if (cinfo->bar_info.bar0_vaddr)
		pci_iounmap(pdev, cinfo->bar_info.bar0_vaddr);
	if (cinfo->bar_info.bar1_vaddr)
		pci_iounmap(pdev, cinfo->bar_info.bar1_vaddr);
	if (cinfo->bar_info.bar2_vaddr)
		pci_iounmap(pdev, cinfo->bar_info.bar2_vaddr);
	if (cinfo->bar_info.bar4_vaddr)
		pci_iounmap(pdev, cinfo->bar_info.bar4_vaddr);

	pci_release_regions(pdev);
	return 0;
}

static int bmdrv_cinfo_init(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
	struct chip_info *cinfo = &bmdi->cinfo;
	u16 device_id = 0;

	pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);
	switch (device_id) {
	case 0x1682:
		cinfo->bmdrv_map_bar = bm1682_map_bar;
		cinfo->bmdrv_unmap_bar = bm1682_unmap_bar;
		cinfo->bmdrv_setup_bar_dev_layout = bm1682_setup_bar_dev_layout;
		cinfo->bmdrv_pcie_calculate_cdma_max_payload = bm1682_pcie_calculate_cdma_max_payload;

		cinfo->bmdrv_start_arm9 = bm1682_start_arm9;
		cinfo->bmdrv_stop_arm9 = bm1682_stop_arm9;

		cinfo->bm_reg = &bm_reg_1682;
		cinfo->share_mem_size = 1 << 10;
		cinfo->chip_type = "bm1682";
		cinfo->platform = DEVICE;

		cinfo->bmdrv_get_irq_status = bm1682_get_irq_status;
		cinfo->bmdrv_unmaskall_intc_irq = NULL;
		cinfo->bmdrv_enable_irq = NULL;
		cinfo->bmdrv_clear_cdmairq = bm1682_clear_cdmairq;
		cinfo->bmdrv_clear_msgirq = bm1682_clear_msgirq;
		cinfo->bmdrv_pending_msgirq_cnt = bm1682_pending_msgirq_cnt;
		break;
	case 0x1684:
		cinfo->bmdrv_map_bar = bm1684_map_bar;
		cinfo->bmdrv_unmap_bar = bm1684_unmap_bar;
		cinfo->bmdrv_setup_bar_dev_layout = bm1684_setup_bar_dev_layout;
		cinfo->bmdrv_pcie_calculate_cdma_max_payload = bm1684_pcie_calculate_cdma_max_payload;

		cinfo->bmdrv_start_arm9 = bm1684_start_arm9;
		cinfo->bmdrv_stop_arm9 = bm1684_stop_arm9;

		cinfo->bm_reg = &bm_reg_1684;
		cinfo->share_mem_size = 1 << 12;  /* 4k DWORD, 16kB */
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

		cinfo->bmdrv_enable_irq = bm1684_enable_intc_irq;
		cinfo->bmdrv_get_irq_status = bm1684_get_irq_status;
		cinfo->bmdrv_unmaskall_intc_irq = bm1684_unmaskall_intc_irq;
		cinfo->bmdrv_clear_cdmairq = bm1684_clear_cdmairq;
		cinfo->bmdrv_clear_msgirq = bm1684_clear_msgirq;
		cinfo->bmdrv_pending_msgirq_cnt = bm1684_pending_msgirq_cnt;

		cinfo->dev_info.chip_temp_reg = 0x00;
		cinfo->dev_info.board_temp_reg = 0x01;
		cinfo->dev_info.board_power_reg = 0x02;
		cinfo->dev_info.fan_speed_reg = 0x03;
		cinfo->dev_info.vendor_id_reg = 0x10;
		cinfo->dev_info.hw_version_reg = 0x14;
		cinfo->dev_info.fw_version_reg = 0x18;
		cinfo->dev_info.board_name_reg = 0x1c;
		cinfo->dev_info.sub_vendor_id_reg = 0x20;
		cinfo->dev_info.sn_reg = 0x24;
		cinfo->dev_info.mcu_version_reg = 0x36;
		break;
	case 0x1686:
		cinfo->bmdrv_map_bar = bm1684_map_bar;
		cinfo->bmdrv_unmap_bar = bm1684_unmap_bar;
		cinfo->bmdrv_setup_bar_dev_layout = bm1684_setup_bar_dev_layout;
		cinfo->bmdrv_pcie_calculate_cdma_max_payload = bm1684_pcie_calculate_cdma_max_payload;

		cinfo->bmdrv_start_arm9 = bm1684x_start_a53lite;
		cinfo->bmdrv_stop_arm9 = bm1684x_stop_a53lite;

		cinfo->bm_reg = &bm_reg_1684x;
		cinfo->share_mem_size = 1 << 12;  /* 4k DWORD, 16kB */
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

		cinfo->bmdrv_enable_irq = bm1684_enable_intc_irq;
		cinfo->bmdrv_get_irq_status = bm1684_get_irq_status;
		cinfo->bmdrv_unmaskall_intc_irq = bm1684_unmaskall_intc_irq;
		cinfo->bmdrv_clear_cdmairq = bm1684_clear_cdmairq;
		cinfo->bmdrv_clear_msgirq = bm1684_clear_msgirq;
		cinfo->bmdrv_pending_msgirq_cnt = bm1684_pending_msgirq_cnt;

		cinfo->dev_info.chip_temp_reg = 0x00;
		cinfo->dev_info.board_temp_reg = 0x01;
		cinfo->dev_info.board_power_reg = 0x02;
		cinfo->dev_info.fan_speed_reg = 0x03;
		cinfo->dev_info.vendor_id_reg = 0x10;
		cinfo->dev_info.hw_version_reg = 0x14;
		cinfo->dev_info.fw_version_reg = 0x18;
		cinfo->dev_info.board_name_reg = 0x1c;
		cinfo->dev_info.sub_vendor_id_reg = 0x20;
		cinfo->dev_info.sn_reg = 0x24;
		cinfo->dev_info.mcu_version_reg = 0x36;
		break;
	default:
		sprintf(cinfo->dev_name, "%s", "unknown device");
		return -EINVAL;
	}

	cinfo->chip_id = device_id;
	cinfo->delay_ms = DELAY_MS;
	cinfo->polling_ms = POLLING_MS;
	cinfo->pcidev = pdev;
	cinfo->device = &pdev->dev;
	sprintf(cinfo->dev_name, "%s", BM_CDEV_NAME);
	return 0;
}


static int bmdrv_pci_init(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
	int rc = 0;
	struct chip_info *cinfo = &bmdi->cinfo;
	u64 pci_dma_mask = DMA_BIT_MASK(IOMMU_ADDR_BIT_NUM);

	/* Prepare PCI device */

	rc = pci_enable_device(pdev);
	if (rc) {
		dev_err(&pdev->dev, "can't enable PCI device\n");
		return rc;
	}

	/* set bar address and layout */

	rc = bmdrv_pci_init_bar_address(pdev, cinfo);
	if (rc) {
		dev_err(&pdev->dev, "alloc bar address error\n");
		goto err_init_bar_addr;
	}

	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) {
		if (config_iatu_for_function_x(pdev, bmdi, &cinfo->bar_info)) {
			rc = -EINVAL;
			dev_err(&pdev->dev, "scan bus fail\n");
			goto err_bar_layout;
		}
	}

	/* setup bar device layout based on bar size */
	if (cinfo->bmdrv_setup_bar_dev_layout(bmdi, SETUP_BAR_DEV_LAYOUT)) {
		rc = -EINVAL;
		dev_err(&pdev->dev, "invalid bar layout\n");
		goto err_bar_layout;
	}

	cinfo->bmdrv_map_bar(bmdi, pdev);

	io_init(bmdi);

	/* init pci DMA attributes */
	/* set pci card as DMA master */
	pci_set_master(pdev);
	if (pci_try_set_mwi(pdev)) {
		dev_info(cinfo->device, "MemoryWrite-Invalidate not support\n");
		rc = -EFAULT;
		goto err_dma;
	}

	/* Set dma address mask */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	if (dma_set_mask(&pdev->dev, pci_dma_mask) ||
            dma_set_coherent_mask(&pdev->dev, pci_dma_mask)){
#else
	if (pci_set_dma_mask(pdev, pci_dma_mask) ||
			pci_set_consistent_dma_mask(pdev, pci_dma_mask)) {
#endif
        dev_err(cinfo->device, "Error: No usable DMA configuration\n");
		rc = -EFAULT;
		goto err_dma;
	}
	/*
	 * x86's dma_alloc_coherent provides memory with write-back type, so in
	 * theory we need PCI device to have ability of snooping to maintain
	 * cache consistency.
	 * and also in theory, if we are using kmalloc memory backed with uncached
	 * type, snooping is useless and cut it may save some cache maintenance
	 * efforts.
	 */
#ifdef USE_DMA_COHERENT
	pcie_capability_clear_word(pdev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_NOSNOOP_EN);
#else
	pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_NOSNOOP_EN);
#endif
	cinfo->bmdrv_pcie_calculate_cdma_max_payload(bmdi);

	pci_set_drvdata(pdev, bmdi);

	return rc;

err_dma:
	cinfo->bmdrv_unmap_bar(&cinfo->bar_info);
err_bar_layout:
	bmdrv_pci_release_bar_addr(pdev, cinfo);
err_init_bar_addr:
	pci_disable_device(pdev);
	return rc;
}

static void bmdrv_pci_deinit(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
	struct chip_info *cinfo = &bmdi->cinfo;

	pci_set_drvdata(pdev, NULL);
	cinfo->bmdrv_unmap_bar(&cinfo->bar_info);
	bmdrv_pci_release_bar_addr(pdev, cinfo);
	pci_disable_device(pdev);
}

static void bm1684_update_a53_enable(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.boot_loader_num < 0x090401 && bmdi->cinfo.boot_loader_num != 0) {
		pr_info("bm-sophgo-%d: boot loader version too old, disable a53!\n", bmdi->dev_index);
		bmdi->misc_info.a53_enable = 0;
	}
}

static int bmdrv_hardware_init(struct bm_device_info *bmdi)
{
	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		if (bm1682_ddr_top_init(bmdi)) {
			pr_err("bm-sophon%d bmdrv: ddr init failed!\n", bmdi->dev_index);
			return -1;
		}
		bm1682_init_iommu(&bmdi->memcpy_info.iommuctl, bmdi->parent);
		bm_vpu_init(bmdi);
		bm1682_top_init(bmdi);
		break;
	case 0x1684:
		if (bmdi->c_attr.bm_set_led_status) {
			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
				bmdi->c_attr.bm_set_led_status(bmdi, LED_BLINK_FAST);
				bmdi->c_attr.led_status = LED_BLINK_FAST;
			} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
				bmdi->c_attr.bm_set_led_status(bmdi, LED_ON);
				bmdi->c_attr.led_status = LED_ON;
			}
		}
		if (bm1684_ddr_top_init(bmdi)) {
			pr_err("bm-sophon%d bmdrv: ddr init failed!\n", bmdi->dev_index);
			return -1;
		}
#ifndef FW_SIMPLE
		bm1684_init_iommu(&bmdi->memcpy_info.iommuctl, bmdi->parent);
#endif
		if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
			vpp_init(bmdi);
			bm_vpu_init(bmdi);
			bmdrv_jpu_init(bmdi);
			spacc_init(bmdi);
			mutex_init(&bmdi->efuse_mutex);
		}

		if (bm1684_get_bootload_version(bmdi)) {
			pr_err("bm-sophon%d bm1684_get_bootload_version failed!\n", bmdi->dev_index);
			return -1;
		}
		bm1684_update_a53_enable(bmdi);
		top_reg_write(bmdi, TOP_CDMA_LOCK, 0);

		bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(bmdi);
		bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(bmdi);
		bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(bmdi);
#ifdef PCIE_MODE_ENABLE_CPU
		sg_comm_init(bmdi->cinfo.pcidev, bmdi);
#endif
		gp_reg_write_enh(bmdi, GP_REG_ARM9_FW_MODE, FW_PCIE_MODE);
		break;
	case 0x1686:
		if (bm1684_ddr_top_init(bmdi)) {
			pr_err("bm-sophon%d bmdrv: ddr init failed!\n", bmdi->dev_index);
			return -1;
		}
#ifndef FW_SIMPLE
		bm1684_init_iommu(&bmdi->memcpy_info.iommuctl, bmdi->parent);
#endif
		if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
			vpp_init(bmdi);
			bm_vpu_init(bmdi);
			bmdrv_jpu_init(bmdi);
			spacc_init(bmdi);
			mutex_init(&bmdi->efuse_mutex);
		}
		gp_reg_write_enh(bmdi, GP_REG_ARM9_FW_MODE, FW_PCIE_MODE);
		bmdrv_clk_set_tpu_divider_fpll(bmdi, 4);
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) {
			bmdrv_clk_set_close(bmdi);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bmdrv_hardware_early_init(struct bm_device_info *bmdi)
{
	int count = 0x5;
	u32 uart_index = 0x2;
	u32 baudrate = 115200;

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		bmdrv_power_and_temp_i2c_init(bmdi);
		break;
	case 0x1684:
		bmdrv_pinmux_init(bmdi);
		bmdrv_gpio_init(bmdi);
retry:
		if (bm1684_get_board_version_from_mcu(bmdi) != 0) {
			if (count-- > 0) {
				pr_info("bm-sophon%d get board version fail count = %d\n", bmdi->dev_index, count);
				goto retry;
			} else {
				pr_info("bm-sophon%d get board version still fail\n", bmdi->dev_index);
				return -1;
			}
		}
		bmdrv_power_and_temp_i2c_init(bmdi);
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) {
			bmdrv_uart_init(bmdi, uart_index, baudrate);
		}
		break;
	case 0x1686:
		bmdrv_pinmux_init(bmdi);
		bmdrv_gpio_init(bmdi);
retry1:
		if (bm1684_get_board_version_from_mcu(bmdi) != 0) {
			if (count-- > 0) {
				pr_info("bm-sophon%d get board version fail count = %d\n", bmdi->dev_index, count);
				goto retry1;
			} else {
				pr_info("bm-sophon%d get board version still fail\n", bmdi->dev_index);
				return -1;
			}
		}
		bmdrv_power_and_temp_i2c_init(bmdi);
		if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_FP150) ||
			(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_CP24) ||
			(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS) ||
			(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_AIV01X) ||
			(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_AIV02X) ||
			(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_AIV03X)) {
			bmdrv_uart_init(bmdi, uart_index, baudrate);
		}
		pr_info("bm-sophon%d 1684x bmdrv_hardware_early_init \n", bmdi->dev_index);
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
		bm_vpu_exit(bmdi);
		bm1682_deinit_iommu(&bmdi->memcpy_info.iommuctl);
		break;
	case 0x1684:
#ifdef PCIE_MODE_ENABLE_CPU
		sg_comm_deinit(bmdi);
#endif
		if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
			vpp_exit(bmdi);
			bm_vpu_exit(bmdi);
			bmdrv_jpu_exit(bmdi);
		}
		bm1684_deinit_iommu(&bmdi->memcpy_info.iommuctl);
		if (bmdi->c_attr.bm_set_led_status) {
			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
				bmdi->c_attr.bm_set_led_status(bmdi, LED_OFF);
				bmdi->c_attr.led_status = LED_OFF;
			} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
				bmdi->c_attr.bm_set_led_status(bmdi, LED_OFF);
				bmdi->c_attr.led_status = LED_OFF;
			}
		}
		break;
	case 0x1686:
		if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
			vpp_exit(bmdi);
			bm_vpu_exit(bmdi);
			bmdrv_jpu_exit(bmdi);
		}
		pr_info("bm-sophon%d 1684x bmdrv_hardware_deinit \n", bmdi->dev_index);
		break;
	default:
		break;
	}
}

static void bmdrv_modules_reset(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1684) {
		bmdrv_sw_reset_tpu(bmdi);
		bmdrv_sw_reset_gdma(bmdi);
		bmdrv_sw_reset_smmu(bmdi);
		bmdrv_sw_reset_cdma(bmdi);
	////    bmdrv_sw_reset_vpp(bmdi);
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
		if (bmdi->c_attr.bm_set_led_status) {
			bmdi->c_attr.bm_set_led_status(bmdi, LED_BLINK_ONE_TIMES_PER_2S);
			bmdi->c_attr.led_status = LED_BLINK_ONE_TIMES_PER_2S;
		}
		break;
	case 0x1686:
		pr_info("bm-sophon%d 1684x bmdrv_chip_specific_init \n", bmdi->dev_index);
		break;
	default:
		rc = -EINVAL;
	}
	return rc;
}

void bmdrv_modules_request_irq(struct bm_device_info *bmdi)
{
	bm_cdma_request_irq(bmdi);
	if (bmdi->cinfo.chip_id == 0x1682)
		bm1682_iommu_request_irq(bmdi);
	if (bmdi->cinfo.chip_id == 0x1684) {
		if (bmdi->boot_info.ddr_ecc_enable == 0x1)
			bm1684_ddr_ecc_request_irq(bmdi);
	}
	bm_msg_request_irq(bmdi);
	bm_gpio_request_irq(bmdi);
	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
		bm_vpp_request_irq(bmdi);
		bm_vpu_request_irq(bmdi);
		bm_jpu_request_irq(bmdi);
		bm_spacc_request_irq(bmdi);
	}
}

static void bmdrv_modules_free_irq(struct bm_device_info *bmdi)
{
	bm_cdma_free_irq(bmdi);
	if (bmdi->cinfo.chip_id == 0x1682)
		bm1682_iommu_free_irq(bmdi);
	if (bmdi->cinfo.chip_id == 0x1684) {
		if (bmdi->boot_info.ddr_ecc_enable == 0x1)
			bm1684_ddr_ecc_free_irq(bmdi);
	}
	bm_msg_free_irq(bmdi);
	bm_gpio_free_irq(bmdi);
	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
		bm_vpp_free_irq(bmdi);
		bm_vpu_free_irq(bmdi);
		bm_jpu_free_irq(bmdi);
	}
}

#ifdef PCIE_MODE_ENABLE_CPU
static void bmdrv_set_a53_boot_args(struct bm_device_info *bmdi)
{
	u32 flag;

	if (bmdi->cinfo.chip_id == BM1684_DEVICE_ID) {
		flag = SKIP_PCIEI;
		(void)bmdev_memcpy_s2d_internal(bmdi, BOOT_ARGS_REG_1684, (const void *)&flag, sizeof(flag));
		flag = FIP_SRC_SPIF;
		(void)bmdev_memcpy_s2d_internal(bmdi, FIP_SOURCE_REG_1684, (const void *)&flag, sizeof(flag));
	} else if (bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
		flag = top_reg_read(bmdi, TOP_BOOT_ARGS_REG_1684X);
		flag &= ~FIP_LOADED;
		flag |= SKIP_PCIEI;
		top_reg_write(bmdi, TOP_BOOT_ARGS_REG_1684X, flag);
		flag = FIP_SRC_SPIF;
		top_reg_write(bmdi, TOP_FIP_SOURCE_REG_1684X, flag);
	} else {
		pr_err("%s: %d set chip id 0x%x a53 boot args error\n", __func__, __LINE__, bmdi->cinfo.chip_id);
	}
}

static u32 bmdrv_get_a53_boot_args(struct bm_device_info *bmdi)
{
	u32 flag;

	if (bmdi->cinfo.chip_id == BM1684_DEVICE_ID) {
		if (0 != bmdev_memcpy_d2s_internal(bmdi, &flag, BOOT_ARGS_REG_1684, sizeof(flag))) {
			pr_err("bmdrv: reset bcmcpu read flag 0x%x fail!\n", flag);
		}
	} else if (bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
		flag = top_reg_read(bmdi, TOP_BOOT_ARGS_REG_1684X);
	} else {
		pr_err("%s: %d get chip id 0x%x error\n", __func__, __LINE__, bmdi->cinfo.chip_id);
		flag = FIP_SRC_INVALID;
	}

	return flag;
}

static int bmdrv_force_reset_bmcpu(struct bm_device_info *bmdi) {
	int                  ret = 0;
	u32                  flag  = 0xabcdabcd;
	int                  retry = 3;
	u32 value = 0;

	if (bmdi->eth_state == true) {
		bmdrv_veth_early_deinit(bmdi, bmdi->cinfo.pcidev);
	}

	bmdi->status_reset = A53_RESET_STATUS_TRUE;
	bmdrv_set_a53_boot_args(bmdi);

	bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_RESET_REG, 0Xdeadbeef);
	msleep(1000);

	bmdrv_set_a53_boot_args(bmdi);
	value = bm_read32(bmdi, 0x50010c00);
	value &= ~(1<<0);
	bm_write32(bmdi, 0x50010c00, value);

	// value = bm_read32(bmdi, 0x50010c00);
	// value &= ~(1<<17);
	// bm_write32(bmdi, 0x50010c00, value);

	bm_write32(bmdi, 0x50010c04, 0x3FFFF);
	bm_write32(bmdi, 0x50010c08, 0xFFFF83C0);

	udelay(500);
	value = bm_read32(bmdi, 0x50010c00);
	value |= (1<<0);
	bm_write32(bmdi, 0x50010c00, value);

	// value = bm_read32(bmdi, 0x50010c00);
	// value |= (1<<17);
	// bm_write32(bmdi, 0x50010c00, value);

	bm_write32(bmdi, 0x50010c04, 0xFFFFFFFF);
	bm_write32(bmdi, 0x50010c08, 0xFFFFFFFF);

	if (ret == 0) {
		msleep(1000);
		while (retry > 0) {
			flag = bmdrv_get_a53_boot_args(bmdi);
			if (flag == FIP_SRC_INVALID) {
				ret = -EFAULT;
				break;
			}
			if ((flag & FIP_LOADED) == 0) {
				break;
			}

			retry--;
			msleep(1000);
		}
		if (retry == 0) {
			pr_err("bmdrv: force reset bmcpu timeout!\n");
			ret = -EFAULT;
		}
	}

	if (ret == 0) {
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU, 0);
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU, 0);
		bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU].sw_rp = 0;

		msleep(1000);
		mutex_lock(&bmdi->c_attr.attr_mutex);
		bmdi->status_bmcpu = 0;
		mutex_unlock(&bmdi->c_attr.attr_mutex);
	}

	if (bmdi->eth_state == true) {
		bmdi->eth_state = false;
		bmdrv_veth_deinit(bmdi, bmdi->cinfo.pcidev);
	}

	bm_write32(bmdi, VETH_SHM_START_ADDR_1684X + VETH_RESET_REG, 0);
	bmdi->status_reset = A53_RESET_STATUS_FALSE;
	return ret;
}

static int bmdrv_force_reset_bmcpu_pcie(struct bm_device_info *bmdi) {
	int                  ret = 0;
	u32                  flag  = 0xabcdabcd;
	int                  retry = 3;
	u32 value = 0;

	bmdrv_set_a53_boot_args(bmdi);

	value = bm_read32(bmdi, 0x50010c00);
	value &= ~(1<<0);
	bm_write32(bmdi, 0x50010c00, value);

	udelay(500);
	value = bm_read32(bmdi, 0x50010c00);
	value |= (1<<0);
	bm_write32(bmdi, 0x50010c00, value);
	if (ret == 0) {
		msleep(1000);
		while (retry > 0) {
			flag = bmdrv_get_a53_boot_args(bmdi);
			if (flag == FIP_SRC_INVALID) {
				ret = -EFAULT;
				break;
			}
			if ((flag & FIP_LOADED) == 0) {
				break;
			}

			retry--;
			msleep(1000);
		}
		if (retry == 0) {
			pr_err("bmdrv: force reset bmcpu timeout!\n");
			ret = -EFAULT;
		}
	}

	return ret;
}

static void bmdrv_fw_unload_mix(struct bm_device_info *bmdi)
{
	// u32 ctrl_word;
	int value = 0x0;

	/*send  fiq 6 to arm9, let it get ready to die*/
	top_reg_write(bmdi, TOP_GP_REG_ARM9_IRQ_SET_OFFSET, 0x1 << 3);

	udelay(50);

	/*stop smbus*/

	value = intc_reg_read(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_MASK_L_OFFSET);
	value = value | (0x1 << 27);
	intc_reg_write(bmdi, INTC0_BASE_ADDR_OFFSET + IRQ_MASK_L_OFFSET, value);

	/* reset arm9 */
	// ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	// ctrl_word &= ~(1 << 1);
	// top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
}

int bmdrv_reset_bmcpu(struct bm_device_info *bmdi)
{
	int ret = 0;
	struct bm_api_info *apinfo;
	bm_api_ext_t bm_api;
	bm_kapi_header_t api_header;
	bm_kapi_opt_header_t api_opt_header;
	u32 fifo_empty_number;
	u32 channel;
	u64 local_send_api_seq;
	bm_api_reset_cpu_t api_reset_cpu;
	u32 flag;
	int retry = 3;
	int board_version = 0;
	u8 board_type = 0;

	typedef enum {
			FW_PCIE_MODE,
			FW_SOC_MODE,
			FW_MIX_MODE
		} bm_arm9_fw_mode;
	bm_arm9_fw_mode mode;

	mode = gp_reg_read_enh(bmdi, GP_REG_ARM9_FW_MODE);

	if (bmdi->status_bmcpu == BMCPU_IDLE && mode == FW_PCIE_MODE) {
		if (bmdi->eth_state == true) {
			bmdi->eth_state = false;
			bmdrv_veth_deinit(bmdi, bmdi->cinfo.pcidev);
		}
		return 0;
	}

	if (mode == FW_MIX_MODE && bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
		pr_info("bmsophon%d mix mode force reset bmcpu!\n", bmdi->dev_index);
		bmdrv_fw_unload_mix(bmdi);
		return bmdrv_force_reset_bmcpu(bmdi);
	}

	bmdrv_set_a53_boot_args(bmdi);

	channel = BM_MSGFIFO_CHANNEL_CPU;
	apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU];

	api_reset_cpu.flag = 1;
	bm_api.api_id = 0x8000000a;
	bm_api.api_addr = (u8 *)&api_reset_cpu;
	bm_api.api_size = sizeof(api_reset_cpu);
	bm_api.api_handle = 0;

	mutex_lock(&apinfo->api_mutex);
	fifo_empty_number = bm_api.api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32) + sizeof(bm_kapi_opt_header_t) / sizeof(u32);

	/* update global api sequence number */
	local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq);

	api_header.api_id = bm_api.api_id;
	api_header.api_size = bm_api.api_size / sizeof(u32);
	api_header.api_handle = 0;
	api_header.api_seq = 0;
	api_header.duration = 0;
	api_header.result = 0;

	/* wait for available fifo space */
	if (bmdev_wait_msgfifo(bmdi, fifo_empty_number, bmdi->cinfo.delay_ms, channel)) {
		mutex_unlock(&apinfo->api_mutex);
		pr_err("%s bm-sophon%d bmdrv: bmdev_wait_msgfifo timeout!\n",
			__func__, bmdi->dev_index);
		return -EBUSY;
	}

	api_opt_header.global_api_seq = local_send_api_seq;
	api_opt_header.api_data = 0;
	/* copy api data to fifo */
	ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, &api_opt_header, channel, false);

	mutex_unlock(&apinfo->api_mutex);

	bmdi->status_reset = A53_RESET_STATUS_TRUE;
	if (ret == 0) {
		msleep(1000);
		while (retry > 0) {
			flag = bmdrv_get_a53_boot_args(bmdi);
			if (flag == FIP_SRC_INVALID) {
				ret = -EFAULT;
				break;
			}
			if ((flag & FIP_LOADED) == 0) {
				break;
			}

			retry--;
			msleep(1000);
		}
		if (retry == 0) {
			pr_err("bmdrv: reset bmcpu timeout!\n");
			ret = -EFAULT;
		}
	}

	board_version = bmdi->cinfo.board_version;
	board_type = (u8)((board_version >> 8) & 0xff);
	if (bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
		pr_info("force reset bmcpu!\n");
		ret = bmdrv_force_reset_bmcpu_pcie(bmdi);
	}

	if (ret == 0) {
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU, 0);
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU, 0);
		bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU].sw_rp = 0;

		mutex_lock(&bmdi->c_attr.attr_mutex);
		bmdi->status_bmcpu = 0;
		mutex_unlock(&bmdi->c_attr.attr_mutex);
	}

	bmdi->status_reset = A53_RESET_STATUS_FALSE;
	return ret;
}
#endif

static void bmdrv_init_devid_array(void)
{
	int i = 0;
	struct bm_pcie_record *p = bm_record;

	if (0 == bm_devid_inited) {
		for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
			memset(p, 0x0, sizeof(struct bm_pcie_record));
			p++;
		}
		bm_devid_inited = 1;
	}
}

static int bmdrv_check_domain_bdf(int domain_bdf)
{
	int i = 0;
	struct bm_pcie_record *p = bm_record;

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		if (p->inited == 0x1)
			if (p->domain_bdf == domain_bdf)
				return p->dev_index;
		p++;
	}

	return -1;
}

static void bmdrv_dump_pcie_record(void)
{
	int i = 0;
	struct bm_pcie_record *p = bm_record;

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		pr_info("i = 0x%x, domain_bdf = 0x%x, card_index = 0x%x, inted = 0x%x\n",
			i, p->domain_bdf, p->dev_index,
			p->inited);
		p++;
	}
}

static int bmdrv_alloc_dev_index(struct pci_dev *pdev)
{
	int dev_index = 0;
	int i = 0;
	struct bm_pcie_record *p = bm_record;
	int domain_nr = 0;
	int domain_bdf = 0;
	u8 bus, dev, fn;

	domain_nr = pci_domain_nr(pdev->bus);
	bus = pdev->bus->number;
	dev = PCI_SLOT(pdev->devfn);
	fn = PCI_FUNC(pdev->devfn);
	domain_bdf = (domain_nr << 16) | ((bus&0xff) << 8) | ((dev&0x1f) << 5) | (fn&0x7);

	if (fn != 0) {
		if (fn == 0x1) {
			if (bmdrv_check_domain_bdf(domain_bdf - 0x1) < 0) {
				pr_err("pcie root node is not ready %x\n", domain_bdf);
				return -1;
			}
		}
		if (fn == 0x2) {
			if (bmdrv_check_domain_bdf(domain_bdf - 0x1) < 0) {
				pr_err("pcie pre node is not ready %x\n", domain_bdf);
				return -1;
			}
			if (bmdrv_check_domain_bdf(domain_bdf - 0x2) < 0) {
				pr_err("pcie root node is not ready %x\n", domain_bdf);
				return -1;
			}
		}
	}

	dev_index = bmdrv_check_domain_bdf(domain_bdf);
	if (dev_index >= 0)
		return dev_index;

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		if (p->inited == 0x0) {
			p->domain_bdf = domain_bdf;
			p->dev_index = i;
			p->inited = 0x1;
			return p->dev_index;
		}
		p++;
	}
	pr_err("bmdrv_alloc_card_index fail, domain_bdf = 0x%x\n", domain_bdf);
	bmdrv_dump_pcie_record();
	return -1;
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

	return ret;
}

static void bmdrv_record_boot_loader_version(struct bm_device_info *bmdi)
{
	if(bmdi->cinfo.version.bl1_version[0] == 'v')
		bmdev_memcpy_s2d_internal(bmdi, BL1_VERSION_BASE, (void *)bmdi->cinfo.version.bl1_version, BL1_VERSION_SIZE);
	if(bmdi->cinfo.version.bl2_version[0] == 'v')
		bmdev_memcpy_s2d_internal(bmdi, BL2_VERSION_BASE, (void *)bmdi->cinfo.version.bl2_version, BL2_VERSION_SIZE);

	kfree(bmdi->cinfo.version.bl1_version);
	kfree(bmdi->cinfo.version.bl2_version);
}

extern int sg_comm_init(struct pci_dev *pdev, struct bm_device_info *bmdi);
extern void sg_comm_deinit(struct bm_device_info *bmdi);

static int bmdrv_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0x0;
	int dev_index = 0x0;
	struct chip_info *cinfo;
	struct bm_device_info *bmdi;

#ifdef PLATFORM_PALLADIUM
	u8 fn;

	fn = PCI_FUNC(pdev->devfn);
	if (fn > 0)
		return 0;
#endif

	PR_TRACE("bmdrv: probe start\n");

	dev_index = bmdrv_alloc_dev_index(pdev);
	if (dev_index < 0)
		return -1;

	bmdi = devm_kzalloc(&pdev->dev, sizeof(struct bm_device_info), GFP_KERNEL);
	if (!bmdi)
		return -ENOMEM;

	cinfo = &bmdi->cinfo;
	bmdi->dev_index = dev_index;

	bmdrv_cinfo_init(bmdi, pdev);

	rc = bmdrv_pci_init(bmdi, pdev);
	if (rc)
		return rc;

	bmdrv_modules_reset(bmdi);

	rc = bmdrv_hardware_early_init(bmdi);
	if (rc) {
		dev_err(cinfo->device, "device hardware early init fail %d\n", rc);
		goto err_software_init;
	}

	rc = bmdrv_boot_info_init(bmdi);
	if (rc)
		goto err_software_init;

	rc = bmdrv_misc_info_init(pdev, bmdi);
	if (rc) {
		dev_err(cinfo->device, " misc info init fail %d\n", rc);
		goto err_software_init;
	}

	rc = bmdrv_software_init(bmdi);
	if (rc) {
		dev_err(cinfo->device, "device software init fail %d\n", rc);
		goto err_software_init;
	}

	rc = bmdrv_hardware_init(bmdi);
	if (rc) {
		dev_err(cinfo->device, "device hardware init fail %d\n", rc);
		goto err_hardware_init;
	}

	rc = bmdrv_init_irq(pdev);
	if (rc) {
		dev_err(cinfo->device, "device irq init fail %d\n", rc);
		goto err_irq;
	}

	rc = bmdrv_fw_load(bmdi, NULL, NULL);
	if (rc) {
		pr_err("bmdrv: firmware load failed!\n");
		goto err_enable_attr;
	}

	bmdrv_smbus_set_default_value(pdev, bmdi);

#if SYNC_API_INT_MODE == 1
	bmdrv_modules_request_irq(bmdi);
#endif

#ifdef USE_RUNTIME_PM
	pm_runtime_use_autosuspend(cinfo->device);
	pm_runtime_set_autosuspend_delay(cinfo->device, 5000);
	pm_runtime_put_autosuspend(cinfo->device); // FIXME
#endif

	rc = bmdrv_enable_attr(bmdi);
	if (rc)
		goto err_enable_attr;

	rc = bmdrv_chip_specific_init(bmdi);
	if (rc)
		goto err_chip_specific;

	if ((dev_count == 0) && (module_init == 0)) {
		rc = bmdrv_init_bmci(cinfo);
		if (rc) {
			dev_err(&pdev->dev, "bmci init failed!\n");
			goto err_chip_specific;
		}
		module_init = 1;
	}

	rc = bmdrv_ctrl_add_dev(bmci, bmdi);
	if (rc)
		goto err_ctrl_add_dev;

	rc = bm_monitor_thread_init(bmdi);
	if (rc)
		goto err_monitor_thread_init;

	rc = bmdrv_card_init(bmdi);
	if (rc)
		goto err_card_init;

	rc = bmdrv_proc_file_init(bmdi);
	if (rc)
		goto err_proc_file_init;

	bmdev_register_device(bmdi);

	rc = bmdrv_get_boot_loader_version(bmdi);
	if (rc)
		goto err_card_init;

	dev_info(cinfo->device, "Card %d(type:%s) probe done\n", bmdi->dev_index,
			cinfo->chip_type);

	return 0;

err_card_init:
	bmdrv_proc_file_deinit(bmdi);
err_proc_file_init:
	bm_monitor_thread_deinit(bmdi);
err_monitor_thread_init:
	bmdrv_ctrl_del_dev(bmci, bmdi);
err_ctrl_add_dev:
	if (dev_count == 0)
		bmdrv_remove_bmci();
err_chip_specific:
	bmdrv_disable_attr(bmdi);
err_enable_attr:
	bmdrv_free_irq(pdev);
	bmdrv_fw_unload(bmdi);
err_irq:
	bmdrv_hardware_deinit(bmdi);
err_hardware_init:
	bmdrv_software_deinit(bmdi);
err_software_init:
	bmdrv_pci_deinit(bmdi, pdev);
	return rc;
}

static void bmdrv_pci_remove(struct pci_dev *pdev)
{
	struct chip_info *cinfo;
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);

	if (bmdi == NULL)
		return;

	cinfo = &bmdi->cinfo;
	dev_info(cinfo->device, "remove\n");
	i2c2_deinit(bmdi);
	bm_monitor_thread_deinit(bmdi);
#ifdef PCIE_MODE_ENABLE_CPU
	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) {
		if ((bmdi->misc_info.a53_enable == 1)
			&& (bmdi->status_bmcpu == BMCPU_RUNNING)){
				if(0 == bmdrv_reset_bmcpu(bmdi)){
					dev_info(cinfo->device, "reset bmcpu a53 success!\n");
				}else{
					dev_info(cinfo->device, "reset bmcpu a53 exec failed!\n");
				}
			}else{
				dev_info(cinfo->device,
						"by pass reset a53!\na53_enable: %d\nbmcpu status: %d\nboot_loader version: 0x%x\n",
						bmdi->misc_info.a53_enable, bmdi->status_bmcpu, bmdi->cinfo.boot_loader_num);
			}
		if (bmdi->eth_state == true)
		{
			bmdi->eth_state = false;
			bmdrv_veth_deinit(bmdi, bmdi->cinfo.pcidev);
		}
		if (bmdi->comm.ring_buffer != NULL)
			bmdrv_force_reset_bmcpu(bmdi);
	}
#endif
	bmdrv_record_boot_loader_version(bmdi);
	bmdev_unregister_device(bmdi);
	bmdrv_card_deinit(bmdi);
	bmdrv_proc_file_deinit(bmdi);

#ifdef USE_RUNTIME_PM
	pm_runtime_get_sync(cinfo->device); // FIXME
	pm_runtime_dont_use_autosuspend(cinfo->device);
	pm_runtime_set_autosuspend_delay(cinfo->device, -1);
#endif
	bmdrv_ctrl_del_dev(bmci, bmdi);

	if ((dev_count == 0) && (module_exit == 1))
		bmdrv_remove_bmci();

	bmdrv_disable_attr(bmdi);
	bmdrv_fw_unload(bmdi);
	bm1684_maskall_intc_irq(bmdi);
#if SYNC_API_INT_MODE == 1
	bmdrv_modules_free_irq(bmdi);
#endif
	bmdrv_free_irq(pdev);

	bmdrv_hardware_deinit(bmdi);

	bmdrv_software_deinit(bmdi);

	bmdrv_pci_deinit(bmdi, pdev);

	kobject_del(&bmdi->kobj);

	devm_kfree(&pdev->dev, bmdi);
}

#ifdef CONFIG_PM
/* PCI suspend */
static int bmdrv_pci_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);

	dev_info(dev, "suspend\n");

	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	return 0;
}

/* PCI resume */
static int bmdrv_pci_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);

	dev_info(dev, "resume\n");

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	if (pci_enable_device(pdev))
		return -1;
	pci_set_master(pdev);

	return 0;
}

static int bmdrv_pci_runtime_suspend(struct device *dev)
{
	dev_info(dev, "runtime suspend\n");
	return 0;
}

static int bmdrv_pci_runtime_resume(struct device *dev)
{
	dev_info(dev, "runtime resume\n");
	return 0;
}

static const struct dev_pm_ops bmdrv_pci_pm_ops = {
	.suspend	= bmdrv_pci_suspend,
	.resume		= bmdrv_pci_resume,
	.runtime_suspend = bmdrv_pci_runtime_suspend,
	.runtime_resume	= bmdrv_pci_runtime_resume,
};
#endif /* CONFIG_PM */

static void bmdrv_pci_shutdown(struct pci_dev *pdev)
{
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);

	dev_info(bmdi->cinfo.device, "shutdown\n");
	///TODO:

}

static struct pci_device_id bmdrv_devices_tbl[] = {
	{PCI_DEVICE(BITMAIN_VENDOR_ID, BM1682_DEVICE_ID)},
	{PCI_DEVICE(SYNOPSYS_VENDOR_ID, BM1682_DEVICE_ID)},
	{PCI_DEVICE(BITMAIN_VENDOR_ID, BM1684_DEVICE_ID)},
	{PCI_DEVICE(SOPHGO_VENDOR_ID, BM1684X_DEVICE_ID)},
	{0, 0, 0, 0, 0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, bmdrv_devices_tbl);

static struct pci_driver bmdrv_pci_driver = {
	.name	= "bmdrv",
	.id_table = bmdrv_devices_tbl,
	.probe	= bmdrv_pci_probe,
	.remove	= bmdrv_pci_remove,
	.shutdown = bmdrv_pci_shutdown,
#ifdef CONFIG_PM
	.driver = {
		.pm = &bmdrv_pci_pm_ops,
	},
#endif
};

static int bmdrv_init(void)
{
	int ret;

	ret = bmdrv_proc_init();
	if (ret) {
		pr_err("can not create /proc/bmsophon\n");
		return ret;
	}

	ret = bmdrv_class_create();
	if (ret) {
		bmdrv_proc_deinit();
		pr_err("can not create bmsophon class\n");
	}

	return ret;
}

static void bmdrv_exit(void)
{
	bmdrv_proc_deinit();
	bmdrv_class_destroy();
}

static int __init bmdrv_module_init(void)
{
	int ret;

	PR_TRACE("bmdrv: init module\n");

	bmdrv_init_devid_array();

	ret = bmdrv_init();
	if (ret)
		return ret;

	ret = pci_register_driver(&bmdrv_pci_driver);
	if (ret)
		bmdrv_exit();
	return ret;
}

static void __exit bmdrv_module_exit(void)
{
	PR_TRACE("bmdrv: exit module\n");
	module_exit = 1;
	pci_unregister_driver(&bmdrv_pci_driver);
	bmdrv_exit();
}

module_init(bmdrv_module_init);
module_exit(bmdrv_module_exit);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
MODULE_IMPORT_NS(DMA_BUF);
#endif
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiao.wang@sophgo.com");
MODULE_DESCRIPTION("Sophon Series Deep Learning Accelerator Driver");
MODULE_VERSION(PROJECT_VER);
