#include "bm_common.h"
#include "bm1684/bm1684_reg.h"

#include "bm_pcie_drv.tmh"


#define IOMMU_ADDR_BIT_NUM (40)

static int module_init;
static int module_exit;
extern int dev_count;
extern struct bm_ctrl_info *bmci;

static struct bm_pcie_record bm_record[BM_PCIE_MAX_CHIP_NUM];
static int bm_devid_inited;
extern FAST_MUTEX global_mutex;

void bmdrv_clean_record(void)
{
    bm_devid_inited = 0x0;
}

static int bmdrv_cinfo_init(struct bm_device_info *bmdi)
{
	struct chip_info *cinfo = &bmdi->cinfo;
	u32 device_id = bmdi->device_id;

	switch (device_id) {
	// case 0x1682:
	// 	cinfo->bmdrv_map_bar = bm1682_map_bar;
	// 	cinfo->bmdrv_unmap_bar = bm1682_unmap_bar;
	// 	cinfo->bmdrv_setup_bar_dev_layout = bm1682_setup_bar_dev_layout;
	// 	cinfo->bmdrv_pcie_calculate_cdma_max_payload = bm1682_pcie_calculate_cdma_max_payload;

	// 	cinfo->bmdrv_start_arm9 = bm1682_start_arm9;
	// 	cinfo->bmdrv_stop_arm9 = bm1682_stop_arm9;

	// 	cinfo->bm_reg = &bm_reg_1682;
	// 	cinfo->share_mem_size = 1 << 10;
	// 	cinfo->chip_type = "bm1682";
	// 	cinfo->platform = DEVICE;

	// 	cinfo->bmdrv_get_irq_status = bm1682_get_irq_status;
	// 	cinfo->bmdrv_unmaskall_intc_irq = NULL;
	// 	cinfo->bmdrv_enable_irq = NULL;
	// 	cinfo->bmdrv_clear_cdmairq = bm1682_clear_cdmairq;
	// 	cinfo->bmdrv_clear_msgirq = bm1682_clear_msgirq;
	// 	cinfo->bmdrv_pending_msgirq_cnt = bm1682_pending_msgirq_cnt;
	// 	break;
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
        cinfo->platform = DEVICE;
#ifdef PLATFORM_PALLADIUM
		cinfo->platform = PALLADIUM;
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
		break;
	case 0x1686:
		cinfo->bmdrv_map_bar = bm1684_map_bar;
		cinfo->bmdrv_unmap_bar = bm1684_unmap_bar;
		cinfo->bmdrv_setup_bar_dev_layout = bm1684_setup_bar_dev_layout;
		cinfo->bmdrv_pcie_calculate_cdma_max_payload = bm1684_pcie_calculate_cdma_max_payload;

		cinfo->bmdrv_start_arm9 = bm1684x_start_a53lite;
		cinfo->bmdrv_stop_arm9 = bm1684x_stop_a53lite;

		cinfo->bm_reg = &bm_reg_1684x;

		//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "[bmdrv_cinfo_init] current bm_mcu_info_reg_addr = %x \n",cinfo->bar_info.io_bar_vaddr.mcu_info_bar_vaddr);
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "%s", "unknown device");
		return -1;
	}
	cinfo->chip_id = device_id;
	cinfo->delay_ms = DELAY_MS;
	cinfo->polling_ms = POLLING_MS;
	sprintf(cinfo->dev_name, "%s", BM_CDEV_NAME);
	return 0;
}

void bmdrv_pci_write_confg(struct bm_device_info *bmdi, void* buffer, u32 offset, u32 length) {
    bmdi->BusInterface.SetBusData(bmdi->BusInterface.Context,
                                  PCI_WHICHSPACE_CONFIG,
                                  buffer,
                                  offset,
                                  length);
}

void bmdrv_pci_read_confg(struct bm_device_info *bmdi,
                           void *                 buffer,
                           u32                  offset,
                           u32                  length) {
    bmdi->BusInterface.GetBusData(bmdi->BusInterface.Context,
                                  PCI_WHICHSPACE_CONFIG,
                                  buffer,
                                  offset,
                                  length);
}

void bmdrv_pci_enable_device(struct bm_device_info *bmdi) {
    USHORT command = 0;
    bmdrv_pci_read_confg(bmdi,
                         &command,
                         FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                         sizeof(USHORT));

    command |= PCI_ENABLE_MEMORY_SPACE;
    command &= ~PCI_ENABLE_IO_SPACE;

    bmdrv_pci_write_confg(bmdi,
                          &command,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                          sizeof(USHORT));
}

void bmdrv_pci_disable_device(struct bm_device_info *bmdi) {
    USHORT command = 0;
    bmdrv_pci_read_confg(bmdi,
                         &command,
                         FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                         sizeof(USHORT));

    command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);

    bmdrv_pci_write_confg(bmdi,
                          &command,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                          sizeof(USHORT));
}

void bmdrv_pci_set_master(struct bm_device_info *bmdi) {
    USHORT command = 0;
    bmdrv_pci_read_confg(bmdi,
                         &command,
                         FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                         sizeof(USHORT));

    command |= PCI_ENABLE_BUS_MASTER;

    bmdrv_pci_write_confg(bmdi,
                          &command,
                          FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
                          sizeof(USHORT));
}

static int bmdrv_pci_init(struct bm_device_info *bmdi)
{
	int rc = 0;

	struct chip_info *cinfo = &bmdi->cinfo;

	/* Prepare PCI device */
	bmdrv_pci_enable_device(bmdi);

	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) {
		if (config_iatu_for_function_x(bmdi, &cinfo->bar_info)) {
			rc = -1;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "scan bus fail\n");
			goto err_bar_layout;
		}
	}

	/* setup bar device layout based on bar size */
	//if (cinfo->bmdrv_setup_bar_dev_layout(&cinfo->bar_info, SETUP_BAR_DEV_LAYOUT)) {
	if (cinfo->bmdrv_setup_bar_dev_layout(bmdi, SETUP_BAR_DEV_LAYOUT)) {
		rc = -1;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "invalid bar layout\n");
		goto err_bar_layout;
	}

	cinfo->bmdrv_map_bar(bmdi);

	TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Map bar layout successful\n");

	io_init(bmdi);

	TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "IO init successful\n");

	/* init pci DMA attributes */
	/* set pci card as DMA master */
	bmdrv_pci_set_master(bmdi);

	TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMA init successful\n");


	cinfo->bmdrv_pcie_calculate_cdma_max_payload(bmdi);

	//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "[bmdrv_pci_init] current bm_mcu_info_reg_addr = %x \n",bmdi->cinfo.bar_info.io_bar_vaddr.mcu_info_bar_vaddr);

	//cinfo->bmdrv_unmap_bar(&cinfo->bar_info);

    return rc;
err_bar_layout:
	return rc;
}

static void bmdrv_pci_deinit(struct bm_device_info *bmdi)
{
	bmdrv_pci_disable_device(bmdi);
}

static int bmdrv_hardware_init(struct bm_device_info *bmdi)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! enter\n");
	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		if (bm1682_ddr_top_init(bmdi)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bm-sophon%d bmdrv: ddr init failed!\n",
                        bmdi->dev_index);
			return -1;
		}
		//bm1682_init_iommu(&bmdi->memcpy_info.iommuctl, bmdi->parent);
		//bm_vpu_init(bmdi);
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
            TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bm-sophon%d bmdrv: ddr init failed!\n",
                        bmdi->dev_index);
			return -1;
		}
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bm1684_ddr_top_init done\n");
#ifndef FW_SIMPLE
		//bm1684_init_iommu(&bmdi->memcpy_info.iommuctl, bmdi->parent);
#endif
		if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
			vpp_init(bmdi);
			bm_vpu_init(bmdi);
			bmdrv_jpu_init(bmdi);
		}
		//top_reg_write(bmdi, TOP_CDMA_LOCK, 0);   //must be a 1684 only function

		bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(bmdi);
		bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(bmdi);
		bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(bmdi);
		break;
	case 0x1686:
		if (bm1684_ddr_top_init(bmdi)) {
			TraceEvents(TRACE_LEVEL_ERROR,
                        TRACE_DEVICE,
                        "bm-sophon%d bmdrv: ddr init failed!\n",
                        bmdi->dev_index);
		}
		TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bm1684_ddr_top_init done\n");
		if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
			vpp_init(bmdi);
			bm_vpu_init(bmdi);
			bmdrv_jpu_init(bmdi);
		}
		//gp_reg_write_enh(bmdi,GP_REG_ARM9_FW_MODE);
		//bmdrv_clk_set_tpu_divider_fpll(bmdi,4);
		break;
	default:
		return -1;
	}
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! exit\n");
	return 0;
}

static int bmdrv_hardware_early_init(struct bm_device_info *bmdi)
{
	u32 i2c0_addr = I2C_68127_ADDR;
	int count = 0x5;

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		bmdrv_i2c_init(bmdi);
		break;
	case 0x1684:
		bmdrv_pinmux_init(bmdi);
		bmdrv_gpio_init(bmdi);
		bmdrv_i2c_init(bmdi);
retry:
		if (bm1684_get_board_version_from_mcu(bmdi) != 0) {
			if (count-- > 0) {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d get board version fail count = %d\n", bmdi->dev_index, count);
				goto retry;
			} else {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d get board version still fail\n", bmdi->dev_index);
				return -1;
			}
		}

		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
			i2c0_addr = I2C_1331_ADDR;

		bm_smbus_init(bmdi, i2c0_addr);
		bm_get_sn(bmdi, bmdi->cinfo.sn); // v1.1 board may fail!
		break;
	case 0x1686:
		bmdrv_pinmux_init(bmdi);
		bmdrv_gpio_init(bmdi);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d is going to get board version from mcu!!!", bmdi->dev_index);
		//TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "[bmdrv_hardware_early_init] current bm_mcu_info_reg_addr = %x \n",bmdi->cinfo.bar_info.io_bar_vaddr.mcu_info_bar_vaddr);
retry1:
		if (bm1684_get_board_version_from_mcu(bmdi) != 0) {
			if (count-- > 0) {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d get board version fail count = %d\n", bmdi->dev_index, count);
				goto retry1;
			} else {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d get board version still fail\n", bmdi->dev_index);
				return -1;
			}
		}
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB){
			i2c0_addr = I2C_68224_ADDR;
		}

		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d is going to init i2c !!!", bmdi->dev_index);
		bmdrv_i2c_init(bmdi);
		bm_smbus_init(bmdi, i2c0_addr);
		bm_get_sn(bmdi, bmdi->cinfo.sn);
		break;
	default:
		return -1;
	}
	return 0;
}

static void bmdrv_hardware_deinit(struct bm_device_info *bmdi)
{
	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		//bm_vpu_exit(bmdi);
		//bm1682_deinit_iommu(&bmdi->memcpy_info.iommuctl);
		break;
	case 0x1684:
		if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
			//bm_vpu_exit(bmdi);
			vpp_exit(bmdi);
			bm_vpu_exit(bmdi);
			bmdrv_jpu_exit(bmdi);
		}
		//bm1684_deinit_iommu(&bmdi->memcpy_info.iommuctl);
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
			//bm_vpu_exit(bmdi);
			vpp_exit(bmdi);
			bm_vpu_exit(bmdi);
			bmdrv_jpu_exit(bmdi);
		}
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon%d 1684x bmdrv_hardware_deinit \n", bmdi->dev_index);
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
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon%d 1684x bmdrv_chip_specific_init \n", bmdi->dev_index);
		break;
	default:
		rc = -1;
	}
	return rc;
}

void bmdrv_modules_request_irq(struct bm_device_info *bmdi)
{
	bm_cdma_request_irq(bmdi);

	if (bmdi->cinfo.chip_id == 0x1684) {
		if (bmdi->boot_info.ddr_ecc_enable == 0x1)
			bm1684_ddr_ecc_request_irq(bmdi);
	}
	bm_msg_request_irq(bmdi);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon%d 1684x finished msg request irq \n", bmdi->dev_index);
	bm_gpio_request_irq(bmdi);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon%d 1684x finished gpio request irq  \n", bmdi->dev_index);
	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon start to request irq for vpp \n");
		bm_vpp_request_irq(bmdi);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon start to request irq for vpu \n");
		bm_vpu_request_irq(bmdi);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon start to request irq for jpu \n");
		bm_jpu_request_irq(bmdi);
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bm-sophon out of [bmdrv_modules_request_irq] \n");
}

void bmdrv_modules_free_irq(struct bm_device_info *bmdi)
{
	bm_cdma_free_irq(bmdi);
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

// #ifdef PCIE_MODE_ENABLE_CPU
// static void bmdrv_set_a53_boot_args(struct bm_device_info *bmdi)
// {
// 	u32 flag;

// 	if (bmdi->cinfo.chip_id == BM1684_DEVICE_ID) {
// 		flag = SKIP_PCIEI;
// 		(void)bmdev_memcpy_s2d_internal(bmdi, BOOT_ARGS_REG_1684, (const void *)&flag, sizeof(flag));
// 		flag = FIP_SRC_SPIF;
// 		(void)bmdev_memcpy_s2d_internal(bmdi, FIP_SOURCE_REG_1684, (const void *)&flag, sizeof(flag));
// 	} else if (bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
// 		flag = top_reg_read(bmdi, TOP_BOOT_ARGS_REG_1684X);
// 		flag &= ~FIP_LOADED;
// 		flag |= SKIP_PCIEI;
// 		top_reg_write(bmdi, TOP_BOOT_ARGS_REG_1684X, flag);
// 		flag = FIP_SRC_SPIF;
// 		top_reg_write(bmdi, TOP_FIP_SOURCE_REG_1684X, flag);
// 	} else {
// 		TraceEvents(
// 			TRACE_LEVEL_INFORMATION,
//             TRACE_DEVICE,
// 			"%s: %d set chip id 0x%x a53 boot args error\n", __func__, __LINE__, bmdi->cinfo.chip_id);
// 	}
// }


// static u32 bmdrv_get_a53_boot_args(struct bm_device_info *bmdi)
// {
// 	u32 flag;

// 	if (bmdi->cinfo.chip_id == BM1684_DEVICE_ID) {
// 		if (0 != bmdev_memcpy_d2s_internal(bmdi, &flag, BOOT_ARGS_REG_1684, sizeof(flag))) {
// 			TraceEvents(
// 				TRACE_LEVEL_INFORMATION,
//             	TRACE_DEVICE,
//             	"bmdrv: reset bcmcpu read flag 0x%x fail!\n", 
//             	flag);
// 		}
// 	} else if (bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
// 		flag = top_reg_read(bmdi, TOP_BOOT_ARGS_REG_1684X);
// 	} else {
// 		TraceEvents(
// 			TRACE_LEVEL_INFORMATION,
//             TRACE_DEVICE,
//             "%s: %d get chip id 0x%x error\n", 
//             __func__, 
//             __LINE__, 
//             bmdi->cinfo.chip_id);
// 		flag = FIP_SRC_INVALID;
// 	}

// 	return flag;
// }

// int bmdrv_force_reset_bmcpu(struct bm_device_info *bmdi) {
// 	int                  ret = 0;
// 	u32                  flag  = 0xabcdabcd;
// 	int                  retry = 3;
// 	u32 value = 0;

// 	bmdi->status_reset = A53_RESET_STATUS_TRUE;
// 	bmdrv_set_a53_boot_args(bmdi);

// 	value = bm_read32(bmdi, 0x50010c00);
// 	value &= ~(1<<0);
// 	bm_write32(bmdi, 0x50010c00, value);

// 	bm_udelay(500);
// 	value = bm_read32(bmdi, 0x50010c00);
// 	value |= (1<<0);
// 	bm_write32(bmdi, 0x50010c00, value);
// 	if (ret == 0) {
// 		bm_mdelay(1000);
// 		while (retry > 0) {
// 			flag = bmdrv_get_a53_boot_args(bmdi);
// 			if (flag == FIP_SRC_INVALID) {
// 				ret = -1;
// 				break;
// 			}
// 			if ((flag & FIP_LOADED) == 0) {
// 				break;
// 			}

// 			retry--;
// 			bm_mdelay(1000);
// 		}
// 		if (retry == 0) {
// 			TraceEvents(
// 				TRACE_LEVEL_INFORMATION,
//             	TRACE_DEVICE,
//             	"bmdrv: force reset bmcpu timeout!\n");
// 			ret = -1;
// 		}
// 	}

// 	if (ret == 0) {
// 		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU, 0);
// 		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU, 0);
// 		bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU].sw_rp = 0;

// 		bm_mdelay(1000);
// 		ExAcquireFastMutex(&bmdi->c_attr.attr_mutex);
// 		bmdi->status_bmcpu = 0;
// 		ExReleaseFastMutex(&bmdi->c_attr.attr_mutex);
// 	}
// 	return ret;
// }
// int bmdrv_reset_bmcpu(struct bm_device_info *bmdi)
// {
// 	int ret = 0;
// 	struct bm_api_info *apinfo;
// 	bm_api_ext_t bm_api;
// 	bm_kapi_header_t api_header;
// 	bm_kapi_opt_header_t api_opt_header;
// 	u32 fifo_empty_number;
// 	u32 channel;
// 	u64 local_send_api_seq;
// 	bm_api_reset_cpu_t api_reset_cpu;
// 	u32 flag;
// 	int retry = 3;

// 	typedef enum {
// 			FW_PCIE_MODE,
// 			FW_SOC_MODE,
// 			FW_MIX_MODE
// 		} bm_arm9_fw_mode;
// 	bm_arm9_fw_mode mode;

// 	mode = gp_reg_read_enh(bmdi, GP_REG_ARM9_FW_MODE);
// 	if (mode == FW_MIX_MODE && bmdi->cinfo.chip_id == BM1684X_DEVICE_ID) {
// 		pr_info("mix mode force reset bmcpu!\n");
// 		if (bmdi->eth_state == true)
// 		{
// 			bmdi->eth_state = false;
// 			bmdrv_veth_deinit(bmdi, bmdi->cinfo.pcidev);
// 		}
// 		bmdrv_fw_unload(bmdi);
// 		return bmdrv_force_reset_bmcpu(bmdi);
// 	}
// 	bmdrv_set_a53_boot_args(bmdi);

// 	channel = BM_MSGFIFO_CHANNEL_CPU;
// 	apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU];

// 	api_reset_cpu.flag = 1;
// 	bm_api.api_id = 0x8000000a;
// 	bm_api.api_addr = (u8 *)&api_reset_cpu;
// 	bm_api.api_size = sizeof(api_reset_cpu);
// 	bm_api.api_handle = 0;

// 	mutex_lock(&apinfo->api_mutex);
// 	fifo_empty_number = bm_api.api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32) + sizeof(bm_kapi_opt_header_t) / sizeof(u32);

// 	/* update global api sequence number */
// 	local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq);

// 	api_header.api_id = bm_api.api_id;
// 	api_header.api_size = bm_api.api_size / sizeof(u32);
// 	api_header.api_handle = 0;
// 	api_header.api_seq = 0;
// 	api_header.duration = 0;
// 	api_header.result = 0;

// 	/* wait for available fifo space */
// 	if (bmdev_wait_msgfifo(bmdi, fifo_empty_number, bmdi->cinfo.delay_ms, channel)) {
// 		mutex_unlock(&apinfo->api_mutex);
// 		pr_err("%s bm-sophon%d bmdrv: bmdev_wait_msgfifo timeout!\n",
// 			__func__, bmdi->dev_index);
// 		return -EBUSY;
// 	}

// 	api_opt_header.global_api_seq = local_send_api_seq;
// 	api_opt_header.api_data = 0;
// 	/* copy api data to fifo */
// 	ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, &api_opt_header, channel, false);

// 	mutex_unlock(&apinfo->api_mutex);

// 	bmdi->status_reset = A53_RESET_STATUS_TRUE;
// 	if (ret == 0) {
// 		msleep(1000);
// 		while (retry > 0) {
// 			flag = bmdrv_get_a53_boot_args(bmdi);
// 			if (flag == FIP_SRC_INVALID) {
// 				ret = -EFAULT;
// 				break;
// 			}
// 			if ((flag & FIP_LOADED) == 0) {
// 				break;
// 			}

// 			retry--;
// 			msleep(1000);
// 		}
// 		if (retry == 0) {
// 			pr_err("bmdrv: reset bmcpu timeout!\n");
// 			ret = -EFAULT;
// 		}
// 	}

// 	if (ret == 0) {
// 		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_CPU, 0);
// 		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_CPU, 0);
// 		bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU].sw_rp = 0;

// 		mutex_lock(&bmdi->c_attr.attr_mutex);
// 		bmdi->status_bmcpu = 0;
// 		mutex_unlock(&bmdi->c_attr.attr_mutex);
// 	}

// 	return ret;
// }

// #endif

void bmdrv_init_devid_array(void)
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

int bmdrv_check_domain_bdf(int domain_bdf)
{
	int i = 0;
	struct bm_pcie_record *p = bm_record;

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		if (p->inited == 0x1) {
			if (p->domain_bdf == domain_bdf)
				return p->dev_index;
		}
		p++;
	}

	return -1;
}

int bmdrv_check_probe_status(int domain_bdf) {
	int i = 0;
	struct bm_pcie_record *p = bm_record;

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		if (p->inited == 0x1) {
			if (p->domain_bdf == domain_bdf) {
				if (p->bmdi != NULL) {
					if (p->bmdi->probed == 1)
						return 1;
				}
			}
		}
		p++;
	}

	return -1;
}


int bmdrv_deinit_domain_bdf(struct bm_device_info *bmdi)
{
	int i = 0;
	struct bm_pcie_record *p = bm_record;
	int domain_bdf;
	int domain_nr = 0;
	u8 bus, dev, fn;

	bus = (u8)bmdi->bus_number;
	dev = (u8)bmdi->device_number;
	fn  = (u8)bmdi->function_number;
	domain_bdf = (domain_nr << 16) | ((bus&0xff) << 8) | ((dev&0x1f) << 5) | (fn&0x0);

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		if (p->domain_bdf == domain_bdf) {
			p->inited = 0x0;
			return 0;
		}
		p++;
	}

	return -1;
}

void bmdrv_dump_pcie_record(void)
{
	int i = 0;
	struct bm_pcie_record *p = bm_record;

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
        TraceEvents(
            TRACE_LEVEL_INFORMATION,
            TRACE_DEVICE,
            "i = 0x%x, domain_bdf = 0x%x, card_index = 0x%x, inted = 0x%x\n",
			i, p->domain_bdf, p->dev_index,
			p->inited);
		p++;
	}
}

struct bm_device_info *bmdrv_get_bmdi_by_domain_bdf(int domain_bdf)
{
	int i = 0;
	struct bm_pcie_record *p = bm_record;

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		if (p->inited == 0x1) {

			if (p->domain_bdf == domain_bdf)
				return p->bmdi;
		}
		p++;
	}
	bmdrv_dump_pcie_record();
	return NULL;
}

int bmdrv_get_card_start_index(struct bm_device_info *bmdi) {
	u8 bus, dev, fn;
	int domain_nr = 0;
	int domain_bdf = 0;
	int card_start_index = bmdi->dev_index;
	struct bm_device_info *bmdi1 = NULL;
	int i = 0x0;

	bus = (u8)bmdi->bus_number;
	dev = (u8)bmdi->device_number;
	fn  = (u8)bmdi->function_number;
	domain_bdf = (domain_nr << 16) | ((bus&0xff) << 8) | ((dev&0x1f) << 5) | (fn&0x0);

	for (i = 0x0; i < 0x3; i++) {
		bmdi1 = bmdrv_get_bmdi_by_domain_bdf(domain_bdf + i);
	if (bmdi1 != NULL) {
			if (bmdi1->dev_index < card_start_index)
				card_start_index = bmdi1->dev_index;
		}
	}

	return card_start_index;
}

int bmdrv_need_delay_probe(struct bm_device_info *bmdi) {
	u8 bus, dev, fn;
	int domain_nr = 0;
	int domain_bdf = 0;

	bus = (u8)bmdi->bus_number;
	dev = (u8)bmdi->device_number;
	fn  = (u8)bmdi->function_number;
	domain_bdf = (domain_nr << 16) | ((bus&0xff) << 8) | ((dev&0x1f) << 5) | (fn&0x7);

	if (fn != 0) {
		if (fn == 0x1) {
			if (bmdrv_check_probe_status(domain_bdf - 0x1) < 0) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "pcie root node is not ready %x\n",
                            domain_bdf);
				return 1;
			}
		}
		if (fn == 0x2) {
            if (bmdrv_check_probe_status(domain_bdf - 0x1) < 0) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "pcie pre node is not ready %x\n",
                            domain_bdf);
				return 1;
			}
            if (bmdrv_check_probe_status(domain_bdf - 0x2) < 0) {
                TraceEvents(TRACE_LEVEL_ERROR,
                            TRACE_DEVICE,
                            "pcie root node is not ready %x\n",
                            domain_bdf);
				return 1;
			}
		}
		return 0;
	} else
		return 0;
}

int bm1684_get_chip_num(struct bm_device_info *bmdi) {
    u8 *atu_base_addr;
    int mode  = 0x0;
    int num = 0x0;

    if (bmdi->function_number != 0)
        num = 3;
    else {
        atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr;
        mode          = REG_READ32(atu_base_addr, 0x718) & 0x3;

        if (mode == 0x2)
            num = 0x3;
        else if (mode == 0x3)
            num = 0x1;
        else
            num = -1;
    }
    return num;

}

int bmdrv_alloc_dev_index(struct bm_device_info *bmdi) {
	int dev_index = 0;
	int i = 0;
	struct bm_pcie_record *p = bm_record;
	int domain_nr = 0;
	int domain_bdf = 0;
	u8 bus, dev, fn;

	domain_nr = 0;
    bus = (u8)bmdi->bus_number;
	dev = (u8)bmdi->device_number;
	fn  = (u8)bmdi->function_number;
	domain_bdf = (domain_nr << 16) | ((bus&0xff) << 8) | ((dev&0x1f) << 5) | (fn&0x7);

	int chip_num = bm1684_get_chip_num(bmdi);
    if (chip_num == -1) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "chipnum no find\n");
        return -1;
    }
	dev_index = bmdrv_check_domain_bdf(domain_bdf);
	p = bm_record;
	if (dev_index >= 0) {
        for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
            if (p->dev_index == dev_index) {
                p->bmdi = bmdi;
                return dev_index;
            }
            p++;
        }
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "index find fail\n");
        return -1;
    } else {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "index start inital\n");
    }

	for (i = 0; i < (BM_PCIE_MAX_CHIP_NUM); i++) {
		if (p->inited == 0x0) {

            if (fn == 0x0) {
                p->domain_bdf = domain_bdf;
                p->dev_index  = i;
                p->inited     = 0x1;
                p->bmdi       = bmdi;

                if (chip_num == 3) {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "fun=0,chip_num=3 entry\n");
                    p++;
                    i++;
                    p->domain_bdf = domain_bdf + 1;
                    p->dev_index  = i;
                    p->inited     = 0x1;

                    p++;
                    i++;
                    p->domain_bdf = domain_bdf + 2;
                    p->dev_index  = i;
                    p->inited     = 0x1;
                    p             = p - 2;
                }
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            TRACE_DEVICE,
                            "index = %d\n",
                            p->dev_index);
                return p->dev_index;
            }

			if (fn == 0x1) {
                p->domain_bdf = domain_bdf - 1;
                p->dev_index  = i;
                p->inited     = 0x1;

                if (chip_num == 3) {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "fn=1,chip_num=3 entry\n");
                    p++;
                    i++;
                    p->domain_bdf = domain_bdf;
                    p->dev_index  = i;
                    p->inited     = 0x1;
                    p->bmdi       = bmdi;

                    p++;
                    i++;
                    p->domain_bdf = domain_bdf + 1;
                    p->dev_index  = i;
                    p->inited     = 0x1;
                    p             = p - 1;
                }
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            TRACE_DEVICE,
                            "index = %d\n",
                            p->dev_index);
                return p->dev_index;
            }

			if (fn == 0x2) {
                p->domain_bdf = domain_bdf - 2;
                p->dev_index  = i;
                p->inited     = 0x1;

                if (chip_num == 3) {
                    TraceEvents(TRACE_LEVEL_INFORMATION,
                                TRACE_DEVICE,
                                "fun=2,chip_num=3 entry\n");
                    p++;
                    i++;
                    p->domain_bdf = domain_bdf - 1;
                    p->dev_index  = i;
                    p->inited     = 0x1;

                    p++;
                    i++;
                    p->domain_bdf = domain_bdf;
                    p->dev_index  = i;
                    p->inited     = 0x1;
                    p->bmdi       = bmdi;
                }
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            TRACE_DEVICE,
                            "index = %d\n",
                            p->dev_index);
                return p->dev_index;
            }
		}
		p++;
	}
    TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "bmdrv_alloc_card_index fail, domain_bdf = 0x%x\n",
                domain_bdf);
	bmdrv_dump_pcie_record();
	return -1;
}

int bmdrv_pci_probe_delayed_function(struct bm_device_info *bmdi) {
	int ret = 0x0;
	int domain_nr = 0;
	int domain_bdf = 0;
	u8 bus, dev, fn;
	// int count = 1;
	// int i = 0x0;
	// struct bm_device_info *bmdi1 = NULL;

	bus = (u8)bmdi->bus_number;
    dev = (u8)bmdi->device_number;
    fn  = (u8)bmdi->function_number;
	domain_bdf = (domain_nr << 16) | ((bus&0xff) << 8) | ((dev&0x1f) << 5) | (fn&0x7);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "entry bmdrv_pci_probe_delayed_function, domain_bdf = %d-%d-%d  %d\n", bus,dev ,fn, domain_bdf);

	// default is for sc5 plus which has three chips, now we need a more excellent method to meet dynamic chip numbers.
	// for (i = fn; i < 2; i++) {
	// 	bmdi1 = bmdrv_get_bmdi_by_domain_bdf(domain_bdf + count);

	// 	TraceEvents(
 //            TRACE_LEVEL_INFORMATION,
 //            TRACE_DEVICE,
 //            "bmdi = 0x%p,count=%d\n",bmdi1,count);

	// 	if (bmdi1 == NULL) {
	// 		    TraceEvents(TRACE_LEVEL_ERROR,
 //                TRACE_DEVICE,
 //                "bmdrv_get_bmdi_by_domain_bdf fail, domain_bdf = 0x%x\n",
 //                domain_bdf + count);
	// 		return 0;
	// 	}

	// 	if (bmdi1->probed == 0x0) {
	// 		ret = bmdrv_pci_probe(bmdi1);
	// 		if (ret < 0) {
	// 			TraceEvents(TRACE_LEVEL_ERROR,
 //                TRACE_DEVICE,
 //                "bmdrv_pci_probe fail, ret = 0x%x\n",
 //                ret);
	// 			return ret;
	// 		}
	// 	}

	// 	count++;

	// }
	 TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_DEVICE,
                "bmdrv_pci_probe_delayed_function, ret = 0x%x\n",
                ret);

	return ret;
}

int bmdrv_pci_probe(struct bm_device_info *bmdi) {
	int rc = 0x0;
	int dev_index = 0x0;
	struct chip_info *cinfo;

	int domain_nr  = 0;
	u8  bus        = (u8)bmdi->bus_number;
	u8 dev        = (u8)bmdi->device_number;
	u8 fn         = (u8)bmdi->function_number;
	int domain_bdf = (domain_nr << 16) | ((bus & 0xff) << 8) | ((dev & 0x1f) << 5) | (fn & 0x7);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bmdrv: probe start 0x%x\n", domain_bdf);
	ExAcquireFastMutex(&global_mutex);
	bmdrv_init_devid_array();
	if (!bmdi->bmcd) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_CARD, "bmcd is null!\n");
	} else {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CARD, "bmcd has been init\n");
	}
	dev_index = bmdrv_alloc_dev_index(bmdi);
	if (dev_index < 0) {
		ExReleaseFastMutex(&global_mutex);
		return -1;
	}

	bmdi->dev_index = dev_index;

	if (bmdrv_need_delay_probe(bmdi) == 0x1) {
		ExReleaseFastMutex(&global_mutex);
		return 0;
	}
	cinfo = &bmdi->cinfo;
	bmdrv_cinfo_init(bmdi);
	rc = bmdrv_pci_init(bmdi);
	if (rc) {
		ExReleaseFastMutex(&global_mutex);
		return rc;
	}
	rc = bmdrv_card_init(bmdi);
	if (rc) {
		ExReleaseFastMutex(&global_mutex);
		goto err_card_init;
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CARD, "bdf 0x%x in card finished inited, card number= %d, chip start index= %d, chip number=%d\n", domain_bdf, bmdi->bmcd->card_index, bmdi->bmcd->dev_start_index, bmdi->bmcd->chip_num);
	ExReleaseFastMutex(&global_mutex);

	bmdrv_modules_reset(bmdi);

	rc = bmdrv_hardware_early_init(bmdi);
	if (rc) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "device hardware early init fail %d\n",
                    rc);
		goto err_software_init;
	}

	rc = bmdrv_boot_info_init(bmdi);
	if (rc)
		goto err_software_init;

	rc = bmdrv_misc_info_init(bmdi);
	if (rc) {
        TraceEvents(
            TRACE_LEVEL_ERROR, TRACE_DEVICE, " misc info init fail %d\n", rc);
		goto err_software_init;
	}

	rc = bmdrv_software_init(bmdi);
	if (rc) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "device software init fail %d\n",
                    rc);
		goto err_software_init;
	}

	rc = bmdrv_hardware_init(bmdi);
	if (rc) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "device hardware init fail %d\n",
                    rc);
		goto err_hardware_init;
	}

	if (bmdrv_init_irq(bmdi)) {
        TraceEvents(
            TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmdrv: init irq failed!\n");
        goto err_irq;
	}

	if (bmdrv_fw_load(bmdi, NULL, NULL)) {
        TraceEvents(
            TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmdrv: firmware load failed!\n");
		goto err_fw;
	}

	bmdrv_smbus_set_default_value(bmdi);

	bmdrv_modules_request_irq(bmdi);

	rc = bmdrv_enable_attr(bmdi);
	if (rc)
		goto err_enable_attr;

	rc = bmdrv_chip_specific_init(bmdi);
	if (rc)
		goto err_chip_specific;


	rc = bm_monitor_thread_init(bmdi);
	if (rc)
		goto err_monitor_thread_init;

	bmdi->probed = 0x1;
    bmdi->is_surpriseremoved = 0x0;
	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "Card %d(type:%s) probe done\n",
                bmdi->dev_index,
			cinfo->chip_type);
	return 0;

err_monitor_thread_init:
err_chip_specific:
	bmdrv_disable_attr(bmdi);
err_enable_attr:
err_fw:
	bmdrv_fw_unload(bmdi);
err_irq:
	bmdrv_hardware_deinit(bmdi);
err_hardware_init:
	bmdrv_software_deinit(bmdi);
err_software_init:
	bmdrv_card_deinit(bmdi);
err_card_init:
	bmdrv_pci_deinit(bmdi);
	return rc;
}

void bmdrv_pci_remove(struct bm_device_info *bmdi) {
	struct chip_info *cinfo;
	if (bmdi == NULL)
		return;

	cinfo = &bmdi->cinfo;
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! remove sophon dev --->%d\n", bmdi->dev_index);

	bmdrv_card_deinit(bmdi);
//	bm_monitor_thread_deinit(bmdi);

// #ifdef PCIE_MODE_ENABLE_CPU
// 	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) {
// 		if ((bmdi->misc_info.a53_enable == 1)
// 			&& (bmdi->status_bmcpu == BMCPU_RUNNING)){
// 				if(0 == bmdrv_reset_bmcpu(bmdi)){
// 					dev_info(cinfo->device, "reset bmcpu a53 success!\n");
// 				}else{
// 					dev_info(cinfo->device, "reset bmcpu a53 exec failed!\n");
// 				}
// 			}else{
// 				dev_info(cinfo->device,
// 						"by pass reset a53!\na53_enable: %d\nbmcpu status: %d\nboot_loader version: 0x%x\n",
// 						bmdi->misc_info.a53_enable, bmdi->status_bmcpu, bmdi->cinfo.boot_loader_num);
// 			}
// 		if (bmdi->eth_state == true)
// 		{
// 			bmdi->eth_state = false;
// 			bmdrv_veth_deinit(bmdi, bmdi->cinfo.pcidev);
// 		}
// 		if (bmdi->comm.ring_buffer != NULL)
// 			bmdrv_force_reset_bmcpu(bmdi);
// 	}
// #endif


	bmdrv_disable_attr(bmdi);
#if SYNC_API_INT_MODE == 1
	bmdrv_modules_free_irq(bmdi);
#endif

	bmdrv_fw_unload(bmdi);

	bmdrv_hardware_deinit(bmdi);

	bmdrv_software_deinit(bmdi);
	bmdrv_deinit_domain_bdf(bmdi);

	bmdrv_pci_deinit(bmdi);

	bmdi->probed = 0x0;
	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "%!FUNC! remove sophon dev <---%d\n",
                bmdi->dev_index);
}

int find_real_dev_index(int dev_id) {
    int i     = 0;
    int count = 0;
    for (i = 0; i < BM_PCIE_MAX_CHIP_NUM; i++) {
        if (bm_record[i].inited != 0) {
            if (count == dev_id)
                return i;
            else
                count++;
		}
    }
	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "not find real_dev_index,dev_id = %d, count = %d\n",
                dev_id,count);
    return -1;

}
