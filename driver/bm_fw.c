#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>

#include "bm_common.h"
#include "bm_cdma.h"
#include "bm_fw.h"
#include "bm_memcpy.h"
#include "bm1682_eu_cmd.h"
#ifdef SOC_MODE
#include "bm1682_soc_firmware_ddr.h"
#include "bm1682_soc_firmware_tcm.h"
#else
#include "bm1682_firmware_ddr.h"
#include "bm1682_firmware_tcm.h"
#endif
#include "bm1684_firmware_ddr.h"
#include "bm1684_firmware_tcm.h"

static int bmdrv_compare_fw(struct bm_device_info *bmdi, struct file *file, const unsigned int *firmware,
		int word_num, u64 dst) {
	bm_cdma_arg cdma_arg;
	int i = 0;
	struct bm_stagemem *stagemem_d2s = &bmdi->memcpy_info.stagemem_d2s;
	unsigned int *p = stagemem_d2s->v_addr;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	int size = word_num * sizeof(u32);
	u32 realmem_size = memcpy_info->stagemem_s2d.size;
	u32 pass_idx = 0;
	u32 cur_addr_inc = 0;
	unsigned long size_step;

	mutex_lock(&stagemem_d2s->stage_mutex);

	for (pass_idx = 0, cur_addr_inc = 0; pass_idx < (size + realmem_size - 1) / realmem_size; pass_idx++) {
		if ((pass_idx + 1) * realmem_size < size)
			size_step = realmem_size;
		else
		size_step = size - pass_idx * realmem_size;

		memset(stagemem_d2s->v_addr, 0, size_step);

		for (i = 0; i < size_step/sizeof(u32); i++) {
			if (p[i] != 0)
			pr_info("after clean index = %d, value = 0x%x\n", i, p[i]);
		}

		bmdev_construct_cdma_arg(&cdma_arg, dst + cur_addr_inc,
			stagemem_d2s->p_addr,
			size_step,
			CHIP2HOST,
			false,
			false);

		if (memcpy_info->bm_cdma_transfer(bmdi, file, &cdma_arg, true)) {
			mutex_unlock(&stagemem_d2s->stage_mutex);
			return -EBUSY;
		}

		for (i = 0; i < size_step/sizeof(u32); i++) {
			if (p[i] != firmware[cur_addr_inc/sizeof(u32) + i]) {
				pr_info("compare fw fail, host = 0x%x, chip = 0x%x, index = %d\n", p[i], firmware[cur_addr_inc + i], cur_addr_inc + i);
				mutex_unlock(&stagemem_d2s->stage_mutex);
				return -EFAULT;
			}
		}
		cur_addr_inc += size_step;
	}
	mutex_unlock(&stagemem_d2s->stage_mutex);

	return 0;
}

static int bmdrv_load_firmware(struct bm_device_info *bmdi, struct file *file, unsigned int *firmware,
		int word_num, u64 dst)
{
	int ret = 0x0;

	if (bmdev_memcpy_s2d_internal(bmdi, dst, firmware, word_num * sizeof(u32))) {
		pr_err("bmdrv: memcpy s2d firmware failed!\n");
		return -EFAULT;
	}

	ret = bmdrv_compare_fw(bmdi, file, firmware, word_num, dst);

	if (ret < 0) {
		pr_err("bmdrv: bmsophon%d fw compare fail!\n", bmdi->dev_index);
		return -EFAULT;
	}
	return ret;
}

int bmdrv_wait_fwinit_done(struct bm_device_info *bmdi)
{
	int cnt = 2500;
	int polling_ms = bmdi->cinfo.polling_ms;
	u32 fw_mask = ~(0xf << 28);
	u32 value = 0;

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM) {
		polling_ms *= PALLADIUM_CLK_RATIO;
	}
#endif
	while ((gp_reg_read_enh(bmdi, GP_REG_FW_STATUS) & fw_mask) != (LAST_INI_REG_VAL & fw_mask)) {
		mdelay(polling_ms);
		if (--cnt == 0)
			break;
	}

	value = gp_reg_read_enh(bmdi, GP_REG_FW_STATUS);
	if (cnt) {
		pr_info("bmdrv: bmsophon%d firmware init done!, status = 0x%x\n", bmdi->dev_index, value);
		return 0;
	}
	pr_err("bmdrv: bmsophon%d firmware init timeout!, status = 0x%x\n", bmdi->dev_index, value);
	return -EBUSY;
}

static int bmdrv_check_firmware_version(struct bm_device_info *bmdi, struct firmware_header *firmware_header){

#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id == 0x1686) {
		snprintf(bmdi->firmware_info, 50, "bm1684x_firmware.bin_v%x.%x.%x-%x-%x",
				firmware_header->major,
				firmware_header->minor,
				firmware_header->patch,
				firmware_header->commit_hash,
				((firmware_header->date)>>8) & 0xffffff);
	} else {
		snprintf(bmdi->firmware_info, 50, "bm1684_firmware.bin_v%x.%x.%x-%x-%x",
				firmware_header->major,
				firmware_header->minor,
				firmware_header->patch,
				firmware_header->commit_hash,
				((firmware_header->date)>>8) & 0xffffff);
	}
	pr_info("%s\n", bmdi->firmware_info);
#endif
	return 0;
}

static int bmdrv_request_and_load_firmware(struct bm_device_info *bmdi, struct file *file, const char *fw_name, unsigned long load_addr) {
	const struct firmware *fw;
	struct firmware_header *firmware_header = NULL;
	int ret = 0;

	ret = request_firmware(&fw, fw_name, bmdi->cinfo.device);
	if (ret != 0) {
		pr_info("bm-sophon%d request_firmware fail, please check if these is bm1684x_firmware.bin in /lib/firmware !!\n", bmdi->dev_index);
		return -1;
	}

	if(bmdi->cinfo.chip_id == 0x1686) {
		ret = bmdrv_load_firmware(bmdi, file, (unsigned int *)(fw->data),
					fw->size / sizeof(u32), load_addr);
	} else {
		firmware_header = (struct firmware_header *)fw->data;
		bmdrv_check_firmware_version(bmdi, firmware_header);
		ret = bmdrv_load_firmware(bmdi, file, (unsigned int *)(firmware_header->fw_data),
						fw->size / sizeof(u32), load_addr);
	}
	release_firmware(fw);

	return ret;
}

static int bmdrv_fw_download_kernel(struct bm_device_info *bmdi, struct file *file)
{
	int ret;
	u64 a53lite_park = 0x100000000;
	const char *bm1684x_dyn_fw = "bm1684x_firmware.bin";
	const char *bm1684_itcm_fw = "bm1684_tcm_firmware.bin";
	const char *bm1684x_ddr_fw = "bm1684_ddr_firmware.bin";

	switch (bmdi->cinfo.chip_id) {
	case 0x1684:
		ret = bmdrv_request_and_load_firmware(bmdi, file, bm1684_itcm_fw, 0);
		if (ret != 0) {
			pr_info("sophon %d load bm1684_itcm_fw fail, ret = %d\n", bmdi->dev_index, ret);
			return -1;
		}
		ret = bmdrv_request_and_load_firmware(bmdi, file, bm1684x_ddr_fw, bmdi->gmem_info.resmem_info.armfw_addr);
		if (ret != 0) {
			pr_info("sophon %d load bm1684_ddr_fw fail, ret = %d\n", bmdi->dev_index, ret);
			return -1;
		}
		break;
	case 0x1686:
		ret = bmdrv_request_and_load_firmware(bmdi, file, bm1684x_dyn_fw, a53lite_park);
		if (ret != 0) {
			pr_info("sophon %d load bm1684_fw fail, ret = %d\n", bmdi->dev_index, ret);
			return -1;
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int bmdrv_fw_download_user(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw)
{
	int ret = 0;
	struct firmware_header *firmware_header_copy, *firmware_header_check;

	pr_info("bmdrv: firmware ddrfw_size is 0x%x, itcmfw_size is 0x%x\n",
			fw->ddrfw_size, fw->itcmfw_size);

	if (fw->ddrfw_size != 0) {
		firmware_header_copy = (struct firmware_header *)fw->ddr_fw;
		firmware_header_check = kmalloc(sizeof(struct firmware_header), GFP_KERNEL);
		ret = copy_from_user(firmware_header_check, fw->ddr_fw, sizeof(struct firmware_header));
		if(ret) pr_info("%s copy from user fail!\n",__func__);

		if(firmware_header_check->magic[0] == 's' && firmware_header_check->magic[1] == 'g' &&
		   firmware_header_check->magic[2] == 'f' && firmware_header_check->magic[3] == 'w') {
			ret = bmdev_memcpy_s2d(bmdi, file, bmdi->gmem_info.resmem_info.armfw_addr,
				(int __user *)firmware_header_copy->fw_data, fw->ddrfw_size - sizeof(struct firmware_header)/sizeof(int), false, 0);
			if (ret){
				kfree(firmware_header_check);
				return ret;
			}
			pr_info("bmdrv: firmware loaded to ddr\n");
		} else {
			ret = bmdev_memcpy_s2d(bmdi, file, bmdi->gmem_info.resmem_info.armfw_addr,
				(int __user *)fw->ddr_fw, fw->ddrfw_size, false, 0);
			if (ret){
				kfree(firmware_header_check);
				return ret;
			}
			pr_info("bmdrv: firmware loaded to ddr\n");
		}
		kfree(firmware_header_check);
	}
	if (fw->itcmfw_size != 0) {
		firmware_header_copy = (struct firmware_header *)fw->itcm_fw;
		firmware_header_check = kmalloc(sizeof(struct firmware_header), GFP_KERNEL);
		ret = copy_from_user(firmware_header_check, fw->itcm_fw, sizeof(struct firmware_header));
		if(ret) pr_info("%s copy from user fail!\n",__func__);

		if(firmware_header_check->magic[0] == 's' && firmware_header_check->magic[1] == 'g' &&
		   firmware_header_check->magic[2] == 'f' && firmware_header_check->magic[3] == 'w') {
			ret = bmdev_memcpy_s2d(bmdi, file, 0,
				(int __user *)firmware_header_copy->fw_data, fw->itcmfw_size - sizeof(struct firmware_header)/sizeof(int), false, 0);
			if (ret){
				kfree(firmware_header_check);
				return ret;
			}
			pr_info("bmdrv: firmware loaded to itcm\n");
		} else {
			ret = bmdev_memcpy_s2d(bmdi, file, 0, (int __user *)fw->itcm_fw,
						fw->itcmfw_size, false, 0);
			if (ret)
				return ret;

			pr_info("bmdrv: firmware loaded to itcm\n");
		}
		kfree(firmware_header_check);
	}

	return ret;
}

static int bmdrv_fw_download(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw)
{
	if (fw)
		return bmdrv_fw_download_user(bmdi, file, fw);
	else
		return bmdrv_fw_download_kernel(bmdi, file);
}

void bmdrv_fw_unload(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bmdrv_stop_arm9(bmdi);
}

static int bmdrv_eu_table_load(struct bm_device_info *bmdi)
{
	int i, cnt;
	u32 address_shift;
	u32 *eu_cmd_warp = kmalloc_array(EU_CMD_LEN, sizeof(u32), GFP_KERNEL);

	if (!eu_cmd_warp)
		return -ENOMEM;
	for (i = 0; i < EU_CMD_LEN / 4; i++) {
		eu_cmd_warp[i * 4 + 0] = eu_cmd[i * 4 + 3];
		eu_cmd_warp[i * 4 + 1] = eu_cmd[i * 4 + 2];
		eu_cmd_warp[i * 4 + 2] = eu_cmd[i * 4 + 1];
		eu_cmd_warp[i * 4 + 3] = eu_cmd[i * 4 + 0];
	}

	if (bmdev_memcpy_s2d_internal(bmdi, bmdi->gmem_info.resmem_info.eutable_addr,
			      eu_cmd_warp, EU_CMD_LEN * sizeof(u32))) {
		pr_err("bmdrv: load eu table failed!\n");
		kfree(eu_cmd_warp);
		return -EFAULT;
	}

	kfree(eu_cmd_warp);

	address_shift = bmdi->gmem_info.resmem_info.eutable_addr >> 8;

	bdc_reg_write(bmdi, 0x18, address_shift);

	cnt = 1000000;
	while (((bdc_reg_read(bmdi, 0x4) & 0x1) != 0) &&
			--cnt != 0)
		;
	if (cnt) {
		pr_info("bmdrv: load eu table done!\n");
		return 0;
	}
	pr_err("bmdrv: load eu table timeout!\n");
	return -EBUSY;
}

#ifndef SOC_MODE
extern void bmdrv_modules_request_irq(struct bm_device_info *bmdi);
extern void bm1684_pcie_msi_irq_enable(struct pci_dev *pdev,
		struct bm_device_info *bmdi);
#endif
int bmdrv_fw_load(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw)
{
	int ret = 0;

	if ((bmdi->cinfo.chip_id == 0x1684) || (bmdi->cinfo.chip_id == 0x1686)) {
		bmdi->cinfo.bmdrv_stop_arm9(bmdi);
	}

	gp_reg_write_enh(bmdi, GP_REG_FW_STATUS, FW_START);

	ret = bmdrv_fw_download(bmdi, file, fw);
	if (ret) {
		pr_err("bmdrv: firmware download failed!\n");
		return ret;
	}
	bmdi->cinfo.bmdrv_start_arm9(bmdi);

	ret = bmdrv_wait_fwinit_done(bmdi);
	if (ret) {
		pr_err("bmdrv: firmware load timeout!\n");
		return ret;
	}
	if (fw) {
		bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].msgirq_num = 0UL;
		bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].sw_rp = 0;
#ifndef SOC_MODE
		/* arm9 reset may cause irq related registers reset*/
#if SYNC_API_INT_MODE == 1
	bmdrv_modules_request_irq(bmdi);
#endif
#endif
		pr_info("bmdrv: firmware load success!\n");
	} else {
		/* load eu table for 1682 during probe */
		if (bmdi->cinfo.chip_id == 0x1682)
			ret = bmdrv_eu_table_load(bmdi);
	}
	return ret;
}
