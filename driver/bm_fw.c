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

#define A53LITE_PARK         0x100000000
#define ARM_FW_ADDR(bmdi)    (bmdi->gmem_info.resmem_info.armfw_addr)

#ifndef SOC_MODE
#include "84x6/84x6_pcie.h"
#endif

/* ==================== unified firmware entry tables ==================== */

static struct bm_fw_entry bm1684_fw_entries[] = {
	{ "bm1684_tcm_firmware.bin", 0,                    NULL, 0 },
	{ "bm1684_ddr_firmware.bin", 0 /* runtime fill */, NULL, 0 },
};

static struct bm_fw_entry bm1686_fw_entries[] = {
	{ "bm1684x_firmware.bin", A53LITE_PARK, NULL, 0 },
};

static struct bm_fw_entry bm1688_fw_entries[] = {
	{ "bm1688_firmware0_os.bin", BM1688_C906_0_PARK, NULL, 0 },
	{ "bm1688_firmware1_os.bin", BM1688_C906_1_PARK, NULL, 0 },
};

static struct bm_fw_entry bm84x6_fw_entries[] = {
#ifndef SOC_MODE
	{ "cv84x6_firmware_ddr_c2c.bin", CV84X6_CA55_PCIE_PARK, bm84x6_c2c_post_load, FW_ONESHOT_C2C },
#endif
	{ "cv84x6_firmware0_os.bin",     CV84X6_C906_0_PARK,    NULL,                   0 },
	{ "cv84x6_firmware1_os.bin",     CV84X6_C906_1_PARK,    NULL,                   0 },
	{ "cv84x6_firmware2_os.bin",     CV84X6_C906_2_PARK,    NULL,                   0 },
	{ "cv84x6_firmware3_os.bin",     CV84X6_C906_3_PARK,    NULL,                   0 },
};

static struct chip_fw_map chip_table[] = {
	{ BM_CHIP_ID_1684, "BM1684",  bm1684_fw_entries, ARRAY_SIZE(bm1684_fw_entries) },
	{ BM_CHIP_ID_1686, "BM1686",  bm1686_fw_entries, ARRAY_SIZE(bm1686_fw_entries) },
	{ BM_CHIP_ID_1688, "BM1688",  bm1688_fw_entries, ARRAY_SIZE(bm1688_fw_entries) },
	{ BM_CHIP_ID_84X6, "CV84X6",  bm84x6_fw_entries, ARRAY_SIZE(bm84x6_fw_entries) },
};



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
				pr_info("compare fw fail, host = 0x%x, chip = 0x%x, index = %d\n", p[i], firmware[cur_addr_inc/sizeof(u32) + i], (int)(cur_addr_inc/sizeof(u32) + i));
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

	if (bmdev_memcpy_s2d_internal(bmdi, dst, firmware, word_num * sizeof(u32), false)) {
		pr_err("bmdrv: memcpy s2d firmware failed!\n");
		return -EFAULT;
	}
	pr_info("bmdrv: memcpy s2d firmware to physical address 0x%llx success!\n", dst);
	ret = bmdrv_compare_fw_stage(bmdi, dst, word_num * sizeof(u32), firmware);

	if (ret < 0) {
		pr_err("bmdrv: bmsophon%d fw compare fail!\n", bmdi->dev_index);
		return -EFAULT;
	}
	return ret;
}

int bmdrv_wait_fwinit_done(struct bm_device_info *bmdi)
{
	int cnt = 20000;
	int polling_ms = bmdi->cinfo.polling_ms;
	u32 fw_mask = ~(0xf << 28);
	u32 value0 = 0;
	int core_num = bmdi->cinfo.tpu_core_num;
    u32 status_values[4] = {0};
    bool init_done = false;
	u32 current_status;
	u32 gp_fw_status = 0;
    int i;
    int int_core_done[4]={0};

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM) {
		polling_ms *= PALLADIUM_CLK_RATIO;
	}
#endif
	if (bmdi->cinfo.chip_id == BM_CHIP_ID_1688)
		gp_fw_status = GP_REG_FW_STATUS;
	else
		gp_fw_status = GP_REG_84X6_FW_STATUS;
	
	pr_info("chipid:%x wait %d  sacler init...\n", bmdi->cinfo.chip_id, core_num);
	if (bmdi->cinfo.chip_id == BM_CHIP_ID_84X6
		||bmdi->cinfo.chip_id == BM_CHIP_ID_1688) {
	    while (cnt && !init_done) {
			init_done = true;

	        for (i = 0; i < core_num; i++) {
	            current_status = gp_reg_read_idx(bmdi, gp_fw_status, i) & fw_mask;
	            if (current_status != (LAST_INI_REG_VAL & fw_mask)) {
	                init_done = false;
	            } else {
					if(int_core_done[i] == 0) {
			 		    int_core_done[i] = 1;
					    pr_info("bmdrv: bmsophon%d firmware init done, core[%d] status = 0x%x\n",
			                      bmdi->dev_index, i, gp_reg_read_idx(bmdi, gp_fw_status, i));
					}
		    	}
	        }

	        if (!init_done) {
	            mdelay(polling_ms);
	            cnt--;
	        }
	    }

	    if (cnt) {
	        return 0;
	    } else {
		for (i = 0; i < core_num; i++) {
		    status_values[i] = gp_reg_read_idx(bmdi, gp_fw_status, i);
		}
		for (i = 0; i < core_num; i++) {
	            pr_info("bmdrv: bmsophon%d firmware init timeout! status[%d] = 0x%x, cnt: %d, polling_ms: %d\n",
	                    bmdi->dev_index, i, status_values[i], cnt, polling_ms);
	        }
	        return -EBUSY;
	    }
	} else {
		while ((gp_reg_read_enh(bmdi, gp_fw_status) & fw_mask) != (LAST_INI_REG_VAL & fw_mask)) {
			mdelay(polling_ms);
			if (--cnt == 0)
				break;
		}

		value0 = gp_reg_read_enh(bmdi, gp_fw_status);
		if (cnt) {
			pr_info("bmdrv: bmsophon%d firmware init done!, status = 0x%x\n", bmdi->dev_index, value0);
			return 0;
		}
		pr_err("bmdrv: bmsophon%d firmware init timeout!, status = 0x%x\n", bmdi->dev_index, value0);
		return -EBUSY;
	}
}

static int bmdrv_check_firmware_version(struct bm_device_info *bmdi, struct firmware_header *firmware_header){

#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id == BM_CHIP_ID_1688) {
		snprintf(bmdi->firmware_info, 50, "bm1688_firmware.bin_v%x.%x.%x-%x-%x",
				firmware_header->major,
				firmware_header->minor,
				firmware_header->patch,
				firmware_header->commit_hash,
				((firmware_header->date)>>8) & 0xffffff);
	} else if (bmdi->cinfo.chip_id == BM_CHIP_ID_84X6) {
		snprintf(bmdi->firmware_info, 50, "cv84x6_firmware.bin_v%x.%x.%x-%x-%x",
				firmware_header->major,
				firmware_header->minor,
				firmware_header->patch,
				firmware_header->commit_hash,
				((firmware_header->date)>>8) & 0xffffff);
	} else if (bmdi->cinfo.chip_id == 0x1686) {
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

#ifdef DEBUG_ON_PLD_FPGA
static int bmdrv_request_and_load_firmware(struct bm_device_info *bmdi, struct file *file, const char *fw_name, unsigned long load_addr)
{
    const struct firmware *fw;
    struct firmware_header *firmware_header = NULL;
    void __iomem *mapped_addr;
    int ret = 0;
    size_t fw_data_size;
    const void *fw_data_ptr;

    ret = request_firmware(&fw, fw_name, bmdi->cinfo.device);
    if (ret != 0) {
        pr_info("bm-sophon%d request_firmware fail, please check if there is %s in /lib/firmware!!\n", 
                bmdi->dev_index, fw_name);
        return -1;
    }

    if (bmdi->cinfo.chip_id == BM_CHIP_ID_1688 || bmdi->cinfo.chip_id == BM_CHIP_ID_84X6) {
        fw_data_ptr = fw->data;
        fw_data_size = fw->size;
    } else {
        firmware_header = (struct firmware_header *)fw->data;
        bmdrv_check_firmware_version(bmdi, firmware_header);
        fw_data_ptr = firmware_header->fw_data;
        fw_data_size = fw->size - ((u8*)firmware_header->fw_data - (u8*)fw->data);
    }

    mapped_addr = ioremap(load_addr, fw_data_size);
    if (!mapped_addr) {
        pr_err("bm-sophon%d: ioremap failed for physical address 0x%lx, size %zu\n", 
               bmdi->dev_index, load_addr, fw_data_size);
        release_firmware(fw);
        return -ENOMEM;
    }

    memcpy_toio(mapped_addr, fw_data_ptr, fw_data_size);
    
    iounmap(mapped_addr);

    pr_info("bm-sophon%d: firmware %s loaded via direct memcpy, size: %zu bytes\n", 
            bmdi->dev_index, fw_name, fw_data_size);

    release_firmware(fw);
    return 0;
}

#else
static int bmdrv_request_and_load_firmware(struct bm_device_info *bmdi, struct file *file, const char *fw_name, unsigned long load_addr) {
	const struct firmware *fw;
	struct firmware_header *firmware_header = NULL;
	int ret = 0;

	ret = request_firmware(&fw, fw_name, bmdi->cinfo.device);
	if (ret != 0) {
		pr_info("bm-sophon%d request_firmware fail, please check if these is %s in /lib/firmware !!\n", bmdi->dev_index, fw_name);
		return -1;
	}

	if (bmdi->cinfo.chip_id == BM_CHIP_ID_1688 || bmdi->cinfo.chip_id == BM_CHIP_ID_84X6) {
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

#endif

static int bmdrv_fw_download_kernel(struct bm_device_info *bmdi, struct file *file)
{
	int ret, i;
	int actual_fw_count;
	struct bm_fw_entry *entry;

#ifdef __linux__
	for (i = 0; i < ARRAY_SIZE(chip_table); i++) {
		if (chip_table[i].chip_id == bmdi->cinfo.chip_id) {
			bmdi->bm_fw_info = &chip_table[i];
			break;
		}
	}

	if (!bmdi->bm_fw_info) {
		pr_err("unsupported chip_id: 0x%x\n", bmdi->cinfo.chip_id);
		return -EINVAL;
	}

	/* 1684: ddr fw load address is runtime-determined */
	if (bmdi->bm_fw_info->chip_id == BM_CHIP_ID_1684)
		bmdi->bm_fw_info->fw_entries[1].park_addr = ARM_FW_ADDR(bmdi);

	actual_fw_count = bmdi->bm_fw_info->fw_count;

	if (bmdi->bm_fw_info->chip_id == BM_CHIP_ID_1688 && bmdi->cinfo.tpu_core_num == 1)
		actual_fw_count = 1;
	else if (bmdi->bm_fw_info->chip_id == BM_CHIP_ID_84X6) {
		int c2c_count = 0;
	#ifndef SOC_MODE
		c2c_count = 1;
	#endif
		actual_fw_count = c2c_count + min_t(int, bmdi->cinfo.tpu_core_num,
					      bmdi->bm_fw_info->fw_count - c2c_count);
	}

	pr_info("bmdrv: loading %d firmwares for %s, tpu_core_num: %d\n",
		actual_fw_count, bmdi->bm_fw_info->chip_name, bmdi->cinfo.tpu_core_num);

	/* ===== unified loop: load + post_load ===== */
	for (i = 0; i < actual_fw_count; i++) {
		entry = &bmdi->bm_fw_info->fw_entries[i];

		/* oneshot: skip if already completed in a previous invocation */
		if (bmdi->fw_oneshot_mask & entry->oneshot)
			continue;

		ret = bmdrv_request_and_load_firmware(bmdi, file,
						       entry->fw_name, entry->park_addr);
		if (ret) {
			pr_err("sophon %d load %s fail, ret = %d\n",
			       bmdi->dev_index, entry->fw_name, ret);
			return ret;
		}

		if (entry->post_load) {
			ret = entry->post_load(bmdi);
			if (ret) {
				pr_err("sophon %d post_load %s fail, ret = %d\n",
				       bmdi->dev_index, entry->fw_name, ret);
				return ret;
			}
		}

		bmdi->fw_oneshot_mask |= entry->oneshot;
	}

	pr_info("bmdrv: load %d firmwares for %s success\n",
		actual_fw_count, bmdi->bm_fw_info->chip_name);
#endif
	return 0;
}

static int bmdrv_fw_download_user(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw)
{
	int ret = 0;
	struct firmware_header *firmware_header_copy, *firmware_header_check;

	pr_info("bmdrv: firmware ddrfw_size is 0x%x, itcmfw_size is 0x%x\n",
			fw->ddrfw_size, fw->itcmfw_size);

	if(bmdi->cinfo.chip_id == BM_CHIP_ID_84X6) {
		pr_err("not support on 84x6\n");
		return -1;
	}

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
			      eu_cmd_warp, EU_CMD_LEN * sizeof(u32), false)) {
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
	int core = 0;
	int core_num = bmdi->cinfo.tpu_core_num;

	if ((bmdi->cinfo.chip_id == BM_CHIP_ID_1688) || (bmdi->cinfo.chip_id == BM_CHIP_ID_84X6)) {
		bmdi->cinfo.bmdrv_stop_arm9(bmdi);
	}

	if (bmdi->cinfo.chip_id == BM_CHIP_ID_1688) {
		gp_reg_write_idx(bmdi, GP_REG_FW_STATUS, FW_START, 0);
		if (core_num != 1)
			gp_reg_write_idx(bmdi, GP_REG_FW_STATUS, FW_START, 1);
	} else if (bmdi->cinfo.chip_id == BM_CHIP_ID_84X6) {
		for (core = 0; core < bmdi->cinfo.tpu_core_num; core++) {
			gp_reg_write_idx(bmdi, GP_REG_84X6_FW_STATUS, FW_START, core);
		}
	} else {
		gp_reg_write_enh(bmdi, GP_REG_FW_STATUS, FW_START);
	}

	ret = bmdrv_fw_download(bmdi, file, fw);
	if (ret) {
		pr_err("bmdrv: firmware download failed!\n");
		return ret;
	}
	pr_err("bmdrv_fw_load start c906\n");
	bmdi->cinfo.bmdrv_start_arm9(bmdi);
	pr_err("bmdrv_fw_load start c906 end\n");
	ret = bmdrv_wait_fwinit_done(bmdi);
	if (ret) {
		pr_err("bmdrv: firmware load timeout!\n");
		return ret;
	}
	if (fw) {
		for (core = 0; core < core_num; core++) {
			bmdi->api_info[core][BM_MSGFIFO_CHANNEL_XPU].msgirq_num = 0UL;
			bmdi->api_info[core][BM_MSGFIFO_CHANNEL_XPU].sw_rp = 0;
		}
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
