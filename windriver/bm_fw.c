#include "bm_common.h"


#include "./bm1682/bm1682_eu_cmd.h"

#include "./bm1682/bm1682_firmware_ddr.h"
#include "./bm1682/bm1682_firmware_tcm.h"

#include "./bm1684/bm1684_firmware_ddr.h"
#include "./bm1684/bm1684_firmware_tcm.h"
#include "./bm1684/bm1686_firmware_ddr.h"

#include "bm_fw.tmh"

static int bmdrv_load_firmware(struct bm_device_info *bmdi, WDFFILEOBJECT file, const unsigned int *firmware,
		int word_num, u64 dst)
{
	//bm_cdma_arg cdma_arg;
	//int i = 0;
    //struct bm_memcpy_info *memcpy_info  = &bmdi->memcpy_info;
    //unsigned int *p = (unsigned int *)(memcpy_info->d2sstagging_mem_vaddr);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! enter\n");

	if (bmdev_memcpy_s2d_internal(bmdi, dst, firmware, word_num * sizeof(u32))) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bmdrv: memcpy s2d firmware failed!\n");
		return -1;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! memcpy s2d firmware successful\n");

	/*
    ExAcquireFastMutex(&memcpy_info->d2sstag_Mutex);
    memset(memcpy_info->d2sstagging_mem_vaddr, 0, word_num * sizeof(u32));

	for (i = 0; i < word_num; i++) {
		if (p[i] != 0)
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_DEVICE,
                        "after clean index = %d, value = 0x%x\n",
                        i,
                        p[i]);
	}

	bmdev_construct_cdma_arg(&cdma_arg, dst,
                             memcpy_info->d2sstagging_mem_paddr.QuadPart,
			word_num * sizeof(u32),
			CHIP2HOST,
			FALSE,
			FALSE);

	if (memcpy_info->bm_cdma_transfer(bmdi, &cdma_arg, TRUE, file)) {
        ExReleaseFastMutex(&memcpy_info->d2sstag_Mutex);
		return -1;
	}

	for (i = 0; i < word_num; i++) {
		if (p[i] != firmware[i]) {
            TraceEvents(
                TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "compare fw fail, host = 0x%x, chip = 0x%x, index = %d\n",
                p[i],
                firmware[i],
                i);
            ExReleaseFastMutex(&memcpy_info->d2sstag_Mutex);
			return -1;
		}
	}
    ExReleaseFastMutex(&memcpy_info->d2sstag_Mutex);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! exit\n");
	*/
	return 0;
}

static int bmdrv_wait_fwinit_done(struct bm_device_info *bmdi)
{
	u32 cnt = 1000;
	int polling_ms = bmdi->cinfo.polling_ms;
	u32 fw_mask = ~(0xf << 28);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		polling_ms *= PALLADIUM_CLK_RATIO;
#endif
	while ((gp_reg_read_enh(bmdi, GP_REG_FW_STATUS) & fw_mask) != (LAST_INI_REG_VAL & fw_mask)) {
		bm_mdelay(polling_ms);
		if (--cnt == 0)
			break;
	}

	fw_mask = gp_reg_read_enh(bmdi, GP_REG_FW_STATUS);
	if (cnt) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bmdrv: bmsophon%d firmware init done!, status = 0x%x\n", bmdi->dev_index, fw_mask);
		return 0;
	}
    TraceEvents(
        TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmdrv: bmsophon%d firmware init timeout!, satus = 0x%x\n", bmdi->dev_index, fw_mask);
	return -1;
}

static int bmdrv_fw_download_kernel(struct bm_device_info *bmdi, WDFFILEOBJECT file)
{
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! enter\n");
	int ret = 0;
	const unsigned int *fw_ddr_array, *fw_tcm_array;
	int fw_ddr_size, fw_tcm_size;

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		fw_ddr_array = bm1682_firmware_ddr_array;
		fw_tcm_array = bm1682_firmware_tcm_array;
		fw_ddr_size = sizeof(bm1682_firmware_ddr_array);
		fw_tcm_size = sizeof(bm1682_firmware_tcm_array);
		break;
	case 0x1684:
		fw_ddr_array = bm1684_firmware_ddr_array;
		fw_tcm_array = bm1684_firmware_tcm_array;
		fw_ddr_size = sizeof(bm1684_firmware_ddr_array);
		fw_tcm_size = sizeof(bm1684_firmware_tcm_array);
		break;
	case 0x1686:
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Start to add firmware of 0x1686 to array\n");
		fw_ddr_array = bm1686_firmware_ddr_array;
		fw_ddr_size = sizeof(bm1686_firmware_ddr_array);
		break;
	default:
		return -1;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bmdrv: 0x1686 firmware loaded to the big array\n");

	if (fw_ddr_size != 0) {
		ret = bmdrv_load_firmware(bmdi, file, fw_ddr_array,
			fw_ddr_size / sizeof(u32),
			bmdi->gmem_info.resmem_info.armfw_addr);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bmdrv: 0x1686 firmware loaded into card, the size is %lu\n", fw_ddr_size);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "bmdrv: 0x1686 firmware loaded into card, the ret is %d\n", ret);

		if (ret)
			return ret;
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bmdrv: firmware loaded to ddr\n");
	}
	// ret = bmdrv_load_firmware(bmdi, file, fw_tcm_array,
	// 		fw_tcm_size / sizeof(u32),
	// 		0);
	// if (ret)
	// 	return ret;
 //    TraceEvents(TRACE_LEVEL_INFORMATION,
 //                TRACE_DEVICE,
 //                "bmdrv: firmware loaded to tcm\n");
 	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! exit\n");
	return ret;
}

static int bmdrv_fw_download_user(struct bm_device_info *bmdi, WDFFILEOBJECT file, pbm_fw_desc fw)
{
	int ret = 0;

	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "bmdrv: firmware ddrfw_size is 0x%x, itcmfw_size is 0x%x\n",
			fw->ddrfw_size, fw->itcmfw_size);
	if (fw->ddrfw_size != 0) {
		ret = bmdev_memcpy_s2d(bmdi, file, bmdi->gmem_info.resmem_info.armfw_addr,
			(int  *)fw->ddr_fw, fw->ddrfw_size, FALSE, 0);
		if (ret)
			return ret;

		TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bmdrv: firmware loaded to ddr\n");
	}

	ret = bmdev_memcpy_s2d(bmdi, file, 0, (int  *)fw->itcm_fw,
			fw->itcmfw_size, FALSE, 0);
	if (ret)
		return ret;

	TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "bmdrv: firmware loaded to itcm\n");
	return ret;
}

static int bmdrv_fw_download(struct bm_device_info *bmdi, WDFFILEOBJECT file, pbm_fw_desc fw)
{
	if (fw){
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! get into download user\n");
		return bmdrv_fw_download_user(bmdi, file, fw);
	}
	else{
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! get into download kernel\n");
		return bmdrv_fw_download_kernel(bmdi, file);
	}
}

void bmdrv_fw_unload(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bmdrv_stop_arm9(bmdi);
}

static int bmdrv_eu_table_load(struct bm_device_info *bmdi)
{
	int i, cnt;
	u32 address_shift;
    u32 *eu_cmd_warp = NULL;  // kmalloc_array(EU_CMD_LEN, sizeof(u32), GFP_KERNEL);

	if (!eu_cmd_warp)
		return -1;
	for (i = 0; i < EU_CMD_LEN / 4; i++) {
		eu_cmd_warp[i * 4 + 0] = eu_cmd[i * 4 + 3];
		eu_cmd_warp[i * 4 + 1] = eu_cmd[i * 4 + 2];
		eu_cmd_warp[i * 4 + 2] = eu_cmd[i * 4 + 1];
		eu_cmd_warp[i * 4 + 3] = eu_cmd[i * 4 + 0];
	}

	if (bmdev_memcpy_s2d_internal(bmdi, bmdi->gmem_info.resmem_info.eutable_addr,
			      eu_cmd_warp, EU_CMD_LEN * sizeof(u32))) {
        TraceEvents(
            TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmdrv: load eu table failed!\n");
		//kfree(eu_cmd_warp);
		return -1;
	}

	//kfree(eu_cmd_warp);

	address_shift = (u32)(bmdi->gmem_info.resmem_info.eutable_addr >> 8);

	bdc_reg_write(bmdi, 0x18, address_shift);

	cnt = 1000000;
	while (((bdc_reg_read(bmdi, 0x4) & 0x1) != 0) &&
			--cnt != 0)
		;
	if (cnt) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_DEVICE,
                    "bmdrv: load eu table done!\n");
		return 0;
	}
    TraceEvents(
        TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmdrv: load eu table timeout!\n");
	return -1;
}

#ifndef SOC_MODE
extern void bmdrv_modules_request_irq(struct bm_device_info *bmdi);
extern void bm1684_pcie_msi_irq_enable(struct bm_device_info *bmdi);
#endif
int bmdrv_fw_load(struct bm_device_info *bmdi, WDFFILEOBJECT file, pbm_fw_desc fw)
{
	int ret = 0;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! enter\n");
	bmdi->cinfo.bmdrv_stop_arm9(bmdi);
	gp_reg_write_enh(bmdi, GP_REG_FW_STATUS, FW_START);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Starting to get into [bmdrv_fw_download]\n");
	ret = bmdrv_fw_download(bmdi, file, fw);
	if (ret) {
        TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_DEVICE,
                    "bmdrv: firmware download failed!\n");
		return ret;
	}
	bmdi->cinfo.bmdrv_start_arm9(bmdi);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! start to wait_fwinit_done\n");
	ret = bmdrv_wait_fwinit_done(bmdi);
	if (ret) {
        TraceEvents(
            TRACE_LEVEL_ERROR, TRACE_DEVICE, "bmdrv: firmware load timeout!\n");
		return ret;
	}
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "%!FUNC! pass wait_fwinit_done\n");
	if (fw) {
		bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].msgirq_num = 0UL;
		bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].sw_rp = 0;
#ifndef SOC_MODE
		/* arm9 reset may cause irq related registers reset*/
#if SYNC_API_INT_MODE == 1
	bmdrv_modules_request_irq(bmdi);
#endif
#endif
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_DEVICE,
                "bmdrv: firmware load success!\n");
	} else {
		/* load eu table for 1682 during probe */
		if (bmdi->cinfo.chip_id == 0x1682)
			ret = bmdrv_eu_table_load(bmdi);
	}
	return ret;
}
