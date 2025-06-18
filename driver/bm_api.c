#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include "bm_common.h"
#include "bm_api.h"
#include "bm_msgfifo.h"
#include "bm_thread.h"
#include "bm1688_card.h"
#include "bm1688_base64.h"

#define TPU0_CFG_PWR_CTRL_ADDR (0x26000100 + 0xc0)
#define TPU1_CFG_PWR_CTRL_ADDR (0x26010100 + 0xc0)

cfg_pwr_ctrl_t g_cfg_pwr_param_default[2] = {
	{
		.cfg12 = {
			.cfg_pwr_ctrl_en     = 1,
			.cfg_pwr_bub_en      = 1,
			.cfg_pwr_limit_en    = 1,
			.cfg_pwr_timeout_en  = 1,
			.cfg_pwr_step_scale  = 1,
			.cfg_pwr_step_max    = 0xF,
			.cfg_pwr_step_min    = 0xC,
			.cfg_pwr_step_len    = 1,
			.cfg_pwr_timeout_len = 0xF,
			.cfg_pwr_lane_all_en = 1,
			.cfg_pwr_scale_en    = 1,
			.cfg_pwr_cur_step    = 0,
			.cfg12_rsvd0_part1   = 0,
			.cfg12_rsvd0_part2   = 0,
		},
		.cfg13 = {
			.cfg_pwr_max_grp0 = {0x25, 0x57, 0x3F, 0x5F, 0x6F, 0x77, 0x7B, 0x7D},
			.cfg_pwr_max_grp1 = {0x7E, 0x2F, 0x73, 0x0F, 0x37, 0x7F, 0x00, 0x00}
		},
	},
	{
		.cfg12 = {
			.cfg_pwr_ctrl_en     = 1,
			.cfg_pwr_bub_en      = 1,
			.cfg_pwr_limit_en    = 1,
			.cfg_pwr_timeout_en  = 1,
			.cfg_pwr_step_scale  = 1,
			.cfg_pwr_step_max    = 0xF,
			.cfg_pwr_step_min    = 0xC,
			.cfg_pwr_step_len    = 1,
			.cfg_pwr_timeout_len = 0xF,
			.cfg_pwr_lane_all_en = 1,
			.cfg_pwr_scale_en    = 1,
			.cfg_pwr_cur_step    = 0,
			.cfg12_rsvd0_part1   = 0,
			.cfg12_rsvd0_part2   = 0,
		},
		.cfg13 = {
			.cfg_pwr_max_grp0 = {0x25, 0x57, 0x3F, 0x5F, 0x6F, 0x77, 0x7B, 0x7D},
			.cfg_pwr_max_grp1 = {0x7E, 0x2F, 0x73, 0x0F, 0x37, 0x7F, 0x00, 0x00}
		},
	}
};

int pwr_ctrl_set(struct bm_device_info *bmdi, cfg_pwr_ctrl_t *cfg_pwr_ctrl_p)
{
	void __iomem *tpu0_cfg_pwr_ctrl_add_v;
	void __iomem *tpu1_cfg_pwr_ctrl_add_v;

	if (cfg_pwr_ctrl_p == NULL) {
		cfg_pwr_ctrl_p = &g_cfg_pwr_param_default[0];
	}

	tpu0_cfg_pwr_ctrl_add_v = bm_get_devmem_vaddr(bmdi, TPU0_CFG_PWR_CTRL_ADDR);
	tpu1_cfg_pwr_ctrl_add_v = bm_get_devmem_vaddr(bmdi, TPU1_CFG_PWR_CTRL_ADDR);

	memcpy(tpu0_cfg_pwr_ctrl_add_v, cfg_pwr_ctrl_p, sizeof(cfg_pwr_ctrl_t));
	memcpy(tpu1_cfg_pwr_ctrl_add_v, (cfg_pwr_ctrl_p+1), sizeof(cfg_pwr_ctrl_t));

	return 0;
}

int pwr_ctrl_get(struct bm_device_info *bmdi, cfg_pwr_ctrl_t *cfg_pwr_ctrl_p)
{
	cfg_pwr_ctrl_t cfg_pwr_ctrl[2];
	void __iomem *tpu0_cfg_pwr_ctrl_add_v;
	void __iomem *tpu1_cfg_pwr_ctrl_add_v;
	int core_id=0;

	tpu0_cfg_pwr_ctrl_add_v = bm_get_devmem_vaddr(bmdi, TPU0_CFG_PWR_CTRL_ADDR);
	tpu1_cfg_pwr_ctrl_add_v = bm_get_devmem_vaddr(bmdi, TPU1_CFG_PWR_CTRL_ADDR);
	memcpy(&cfg_pwr_ctrl[0], tpu0_cfg_pwr_ctrl_add_v, sizeof(cfg_pwr_ctrl_t));
	memcpy(&cfg_pwr_ctrl[1], tpu1_cfg_pwr_ctrl_add_v, sizeof(cfg_pwr_ctrl_t));
	if (cfg_pwr_ctrl_p != NULL) {
		memcpy(cfg_pwr_ctrl_p, &cfg_pwr_ctrl, sizeof(cfg_pwr_ctrl_t)*2);
	} else {
		for (core_id=0; core_id<2; core_id++) {
			pr_err("core_id=%d\n"
					".cfg12:\n"
					".cfg_pwr_ctrl_en     = 0x%x\n"
					".cfg_pwr_bub_en      = 0x%x\n"
					".cfg_pwr_limit_en    = 0x%x\n"
					".cfg_pwr_timeout_en  = 0x%x\n"
					".cfg_pwr_step_scale  = 0x%x\n"
					".cfg_pwr_step_max    = 0x%x\n"
					".cfg_pwr_step_min    = 0x%x\n"
					".cfg_pwr_step_len    = 0x%x\n"
					".cfg_pwr_timeout_len = 0x%x\n"
					".cfg_pwr_lane_all_en = 0x%x\n"
					".cfg_pwr_scale_en    = 0x%x\n"
					".cfg_pwr_cur_step    = 0x%x\n"
					".cfg13:\n"
					".cfg_pwr_max_grp0 = {0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}\n"
					".cfg_pwr_max_grp1 = {0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}\n",
					core_id,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_ctrl_en,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_bub_en,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_limit_en,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_timeout_en,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_scale,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_max,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_min,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_step_len,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_timeout_len,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_lane_all_en,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_scale_en,
					cfg_pwr_ctrl[core_id].cfg12.cfg_pwr_cur_step,
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[0], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[1],
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[2], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[3],
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[4], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[5],
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[6], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp0[7],
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[0], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[1],
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[2], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[3],
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[4], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[5],
					cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[6], cfg_pwr_ctrl[core_id].cfg13.cfg_pwr_max_grp1[7]);
		}
	}

	return 0;
}

int pwr_ctrl_ioctl(struct bm_device_info *bmdi, void *arg)
{
	bm_api_cfg_pwr_ctrl_t bm_api_cfg_pwr_ctrl;
	int ret;

	if (arg != NULL) {
		ret = copy_from_user(&bm_api_cfg_pwr_ctrl, (bm_api_cfg_pwr_ctrl_t __user *)arg, sizeof(bm_api_cfg_pwr_ctrl_t));
		if (ret) {
			pr_err("[%s: %d] bm-sophon%d copy_from_user fail\n", __func__, __LINE__, bmdi->dev_index);
			return ret;
		}
		if (bm_api_cfg_pwr_ctrl.op == 0) {
			pwr_ctrl_get(bmdi, &bm_api_cfg_pwr_ctrl.cfg_pwr_ctrl[0]);
			ret = copy_to_user((bm_api_cfg_pwr_ctrl_t __user *)arg, &bm_api_cfg_pwr_ctrl, sizeof(bm_api_cfg_pwr_ctrl_t));
			if (ret) {
				pr_err("[%s: %d] bm-sophon%d copy_to_user fail\n", __func__, __LINE__, bmdi->dev_index);
				return ret;
			}
		} else {
			pwr_ctrl_set(bmdi, &bm_api_cfg_pwr_ctrl.cfg_pwr_ctrl[0]);
		}
	}

	return 0;
}

static void reg_proc_show(struct bm_device_info *bmdi)
{
	int addr = 0;
	int value = 0;
	int core_num = 0;
	int core_offset = 0x10000;
	int tiu_reg_base_addr = 0x26000000;
	int gdma_reg_base_addr = 0x26020000;
	int read_count = 128;
	int i, j;

	core_num = base_get_core_num(bmdi);
	for (i = 0; i < core_num; i++) {
		for (j = 0; j < read_count; j++) {
			addr = (i * core_offset) + (j * 4) + tiu_reg_base_addr;
			value = bm_read32(bmdi, addr);
			pr_err("tiu core=%d addr=0x%x, value=0x%x\n", i, addr, value);
		}
	}

	for (i = 0; i < core_num; i++) {
		for (j = 0; j < read_count; j++) {
			addr = (i * core_offset) + (j * 4) + gdma_reg_base_addr;
			value = bm_read32(bmdi, addr);
			pr_err("gdma core=%d addr=0x%x, value=0x%x\n", i, addr, value);
		}
	}

}

int bmdev_debug_tpusys(struct bm_device_info *bmdi, int core_id)
{
	int threshold = 1000000;
	int i = 0;
	int tpu_hang = 1;
	int retire_pc_offset = 0x4c + core_id * 0xc;
	int previous_pc, current_pc;
	int previous_status, current_status;

	previous_pc = bdc_reg_read(bmdi, retire_pc_offset);
	for (i = 0; i < threshold; i++) {
		current_pc = bdc_reg_read(bmdi, retire_pc_offset);
		if (current_pc != previous_pc) {
			tpu_hang = 0;
			break;
		}
		previous_pc = current_pc;
		udelay(1);
	}

	// retire PC of TC906 stays still for a long time
	if (tpu_hang) {
		pr_err("bm-sophon%d: TPU%d C906 hang\n", bmdi->dev_index, core_id);
		return tpu_hang;
	}

	previous_status = gp_reg_read_idx(bmdi, GP_REG_FW_STATUS, core_id);
	for (i = 0; i < threshold; i++) {
		current_status = gp_reg_read_idx(bmdi, GP_REG_FW_STATUS, core_id);
		if (current_status != previous_status) {
			pr_err("bm-sophon%d: TPU%d SYS still working\n", bmdi->dev_index, core_id);
			return tpu_hang;
		}
		previous_status = current_status;
		udelay(1);
	}

	if (current_status == 0x103) {
		pr_err("bm-sophon%d: TPU%d SYS still working. TPU%d C906 current status: waiting for event\n", bmdi->dev_index, core_id, core_id);
		return 0;
	} else if (current_status == 0x102) {
		pr_err("bm-sophon%d: TPU%d SYS hang. TPU%d C906 current status: polling engine done\n", bmdi->dev_index, core_id, core_id);
		reg_proc_show(bmdi);
		return 1;
	} else {
		pr_err("bm-sophon%d: TPU%d C906 hang. TPU%d C906 current status: 0x%x\n",
				bmdi->dev_index, core_id, core_id, current_status);
		return 1;
	}
}

DEFINE_SPINLOCK(msg_dump_lock);
void bmdev_dump_reg(struct bm_device_info *bmdi, u32 channel, int core_id)
{
    int i=0;
    spin_lock(&msg_dump_lock);
    if (GP_REG_MESSAGE_WP_CHANNEL_XPU == channel) {
        for(i=0; i<32; i++)
            printk("DEV %d BDC_CMD_REG %d: addr= 0x%08lx, value = 0x%08x\n", bmdi-> dev_index, i,
	    	bmdi->cinfo.bm_reg->tpu_base_addr + i * 4 + core_id * BD_ENGINE_TPU1_OFFSET,
		bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + i * 4 + core_id * BD_ENGINE_TPU1_OFFSET));
        for(i=0; i<64; i++)
            printk("DEV %d BDC_CTL_REG %d: addr= 0x%08lx, value = 0x%08x\n", bmdi-> dev_index, i,
	    	bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i * 4 + core_id * BD_ENGINE_TPU1_OFFSET,
		bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i * 4 + core_id * BD_ENGINE_TPU1_OFFSET));
        for(i=0; i<32; i++)
            printk("DEV %d GDMA_ALL_REG %d: addr= 0x%08lx, value = 0x%08x\n",
	    	bmdi-> dev_index, i, bmdi->cinfo.bm_reg->gdma_base_addr + i * 4 + core_id * GDMA_ENGINE_TPU1_OFFSET,
		bm_read32(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr + i * 4 + core_id * GDMA_ENGINE_TPU1_OFFSET));
       }
    else {
    }
    spin_unlock(&msg_dump_lock);
}

int bmdrv_api_init(struct bm_device_info *bmdi, u32 core, u32 channel)
{
	int ret = 0;
	struct bm_api_info *apinfo = &bmdi->api_info[core][channel];
#ifndef SOC_MODE
	u32 val;
#endif

	apinfo->device_sync_last = 0;
	apinfo->device_sync_cpl = 0;
	apinfo->msgirq_num = 0UL;
	apinfo->sw_rp = 0;
	init_completion(&apinfo->msg_done);
	mutex_init(&apinfo->api_mutex);
	init_completion(&apinfo->dev_sync_done);
	mutex_init(&apinfo->api_fifo_mutex);

#ifndef SOC_MODE
	if(bmdi->cinfo.chip_id == 0x1686) {
		val = bdc_reg_read(bmdi, 0x100);
		bdc_reg_write(bmdi, 0x100, val | 0x1);
	}
#endif

	if (BM_MSGFIFO_CHANNEL_XPU == channel) {
		ret = kfifo_alloc(&apinfo->api_fifo, bmdi->cinfo.share_mem_size * 4, GFP_KERNEL);
		if (core == 0) {
			bmdi->lib_dyn_info = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);
			INIT_LIST_HEAD(&(bmdi->lib_dyn_info->lib_list));
			mutex_init(&(bmdi->lib_dyn_info->bmcpu_lib_mutex));
			INIT_LIST_HEAD(&(bmdi->exec_func.func_list));
			mutex_init(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
			gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU, 0);
			gp_reg_write_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_XPU, 0);
		}
	} else {
		INIT_LIST_HEAD(&apinfo->api_list);
		if (core == 0) {
			bmdi->lib_info = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);
			INIT_LIST_HEAD(&(bmdi->lib_info->lib_list));
			mutex_init(&(bmdi->lib_info->bmcpu_lib_mutex));
		}
	}

	return ret;
}

void bmdrv_api_deinit(struct bm_device_info *bmdi, u32 core, u32 channel)
{
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_info;
	struct bmcpu_lib *lib_dyn_info = bmdi->lib_dyn_info;

	if (core == 0 && BM_MSGFIFO_CHANNEL_XPU == channel) {
		mutex_lock(&lib_dyn_info->bmcpu_lib_mutex);
		list_for_each_entry_safe(lib_temp, lib_next, &lib_dyn_info->lib_list, lib_list) {
			list_del(&lib_temp->lib_list);
			kfree(lib_temp);
		}
		mutex_unlock(&lib_dyn_info->bmcpu_lib_mutex);
		kfree(bmdi->lib_dyn_info);
	} else if (core == 0 && BM_MSGFIFO_CHANNEL_CPU == channel) {
		mutex_lock(&lib_info->bmcpu_lib_mutex);
		list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
			list_del(&lib_temp->lib_list);
			kfree(lib_temp);
		}
		mutex_unlock(&lib_info->bmcpu_lib_mutex);
		kfree(bmdi->lib_info);
	}

	kfifo_free(&bmdi->api_info[core][channel].api_fifo);
}

#define MD5SUM_LEN 16
#define LIB_MAX_NAME_LEN 64
#define FUNC_MAX_NAME_LEN 64

int bmdrv_api_load_lib_process(struct bm_device_info *bmdi, bm_api_ext_t bm_api)
{
	int ret = 0;
	struct bmcpu_lib *lib_node;
	struct bmcpu_lib *lib_temp, *lib_next;
	bm_api_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_info = bmdi->lib_info;

	ret = copy_from_user((u8 *)&api_cpu_load_library_internal, bm_api.api_addr, sizeof(bm_api_cpu_load_library_internal_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}
	api_cpu_load_library_internal.process_handle = api_cpu_load_library_internal.process_handle % LIB_MAX_REC_CNT;
	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if (!strncmp(lib_temp->lib_name, api_cpu_load_library_internal.library_name, LIB_MAX_NAME_LEN) &&
			(lib_temp->rec[api_cpu_load_library_internal.process_handle] != 0)) {
			if (memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
				mutex_unlock(&lib_info->bmcpu_lib_mutex);
				return -EINVAL;
			} else {
				api_cpu_load_library_internal.obj_handle = lib_temp->cur_rec;
				ret = copy_to_user((u8 __user *)bm_api.api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
				if (ret) {
					pr_err("bm-sophon%d copy_to_user fail\n", bmdi->dev_index);
					mutex_unlock(&lib_info->bmcpu_lib_mutex);
					return ret;
				}
				mutex_unlock(&lib_info->bmcpu_lib_mutex);
				return 0;
			}
			break;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if(!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
			lib_temp->refcount++;
			lib_temp->rec[api_cpu_load_library_internal.process_handle] = lib_temp->cur_rec;
			api_cpu_load_library_internal.obj_handle = lib_temp->cur_rec;
			ret = copy_to_user((u8 __user *)bm_api.api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
			if (ret) {
				pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
				mutex_unlock(&lib_info->bmcpu_lib_mutex);
				return ret;
			}
			mutex_unlock(&lib_info->bmcpu_lib_mutex);
			return 0;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	lib_node = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);

	strncpy(lib_node->lib_name, api_cpu_load_library_internal.library_name, LIB_MAX_NAME_LEN);
	lib_node->refcount = 1;
	lib_node->cur_rec = api_cpu_load_library_internal.process_handle;
	lib_node->rec[lib_node->cur_rec] = lib_node->cur_rec;
	memcpy(lib_node->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN);

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_add_tail(&(lib_node->lib_list), &(lib_info->lib_list));
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	return 0;
}

int bmdrv_api_unload_lib_process(struct bm_device_info *bmdi, bm_api_ext_t bm_api)
{
	int ret = 0;
	int i;
	struct bmcpu_lib *lib_temp, *lib_next;
	bm_api_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_info = bmdi->lib_info;

	ret = copy_from_user((u8 *)&api_cpu_load_library_internal, bm_api.api_addr, sizeof(bm_api_cpu_load_library_internal_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	api_cpu_load_library_internal.process_handle = api_cpu_load_library_internal.process_handle % LIB_MAX_REC_CNT;

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if (!strncmp(lib_temp->lib_name, api_cpu_load_library_internal.library_name, LIB_MAX_NAME_LEN) &&
			lib_temp->rec[api_cpu_load_library_internal.process_handle] != 0) {
			lib_temp->refcount--;
			if (lib_temp->cur_rec != api_cpu_load_library_internal.process_handle) {
				api_cpu_load_library_internal.obj_handle = lib_temp->rec[api_cpu_load_library_internal.process_handle];
				ret = copy_to_user((u8 __user *)bm_api.api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
				if (ret) {
					pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
					mutex_unlock(&lib_info->bmcpu_lib_mutex);
					return ret;
				}
			} else if (lib_temp->cur_rec == api_cpu_load_library_internal.process_handle) {
				if (lib_temp->refcount == 0) {
					api_cpu_load_library_internal.obj_handle = lib_temp->rec[api_cpu_load_library_internal.process_handle];
					ret = copy_to_user((u8 __user *)bm_api.api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
					if (ret) {
						pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
						mutex_unlock(&lib_info->bmcpu_lib_mutex);
						return ret;
					}
					list_del(&lib_temp->lib_list);
					kfree(lib_temp);
				} else if (lib_temp->refcount != 0) {
					for (i = 0; i < LIB_MAX_REC_CNT; i++) {
						if(lib_temp->rec[i] != 0 && i != api_cpu_load_library_internal.process_handle)
							break;
					}
					api_cpu_load_library_internal.mv_handle = i;
					api_cpu_load_library_internal.obj_handle = lib_temp->rec[api_cpu_load_library_internal.process_handle];
					ret = copy_to_user((u8 __user *)bm_api.api_addr, &api_cpu_load_library_internal, sizeof(bm_api_cpu_load_library_internal_t));
					if (ret) {
						pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
						mutex_unlock(&lib_info->bmcpu_lib_mutex);
						return ret;
					}
					lib_temp->cur_rec = i;
				}
			}
			lib_temp->rec[api_cpu_load_library_internal.process_handle] = 0;
			break;
		}
	}

	mutex_unlock(&lib_info->bmcpu_lib_mutex);
	return 0;
}

int bmdrv_api_dyn_get_func_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api)
{
	int ret;
	static int f_id = 22;
	struct bmdrv_exec_func *func_node;
	struct list_head *pos;
	int fun_flag = 0;
	//TODO: P mutex
	mutex_lock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
	func_node = (struct bmdrv_exec_func *)kmalloc(sizeof(struct bmdrv_exec_func), GFP_KERNEL);
	if (!func_node) {
		pr_err("bm-sophon%d alloc func node error!\n", bmdi->dev_index);
		mutex_unlock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		return -1;
	}

	ret = copy_from_user(&func_node->exec_func, (bm_get_func_t __user *)p_bm_api->api_addr, sizeof(bm_get_func_t));
	if (ret) {
		pr_err("bm-sophon%d %s %d copy_from_user fail\n", bmdi->dev_index, __FILE__, __LINE__);
		mutex_unlock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		return ret;
	}

	if (func_node->exec_func.core_id != 0 && func_node->exec_func.core_id != 1) {
		pr_err("[%s: %d]error bm-sophon%d function core_id=0x%x\n", __FILE__, __LINE__, bmdi->dev_index,func_node->exec_func.core_id);
		return -1;
	}

	list_for_each(pos, &(bmdi->exec_func.func_list))
    {
        if(!memcmp(((struct bmdrv_exec_func *)pos)->exec_func.md5, func_node->exec_func.md5, MD5SUM_LEN) &&
			!strncmp(((struct bmdrv_exec_func *)pos)->exec_func.func_name, func_node->exec_func.func_name, FUNC_MAX_NAME_LEN) &&
			((struct bmdrv_exec_func *)pos)->exec_func.core_id == func_node->exec_func.core_id) {
			func_node->exec_func.f_id = ((struct bmdrv_exec_func *)pos)->exec_func.f_id;
			fun_flag = 1;
			break;
		}
    }
	mutex_unlock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));

	if (fun_flag == 0) {
		func_node->exec_func.f_id = f_id++;
		//TODO: V mutex
		mutex_lock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		list_add_tail(&(func_node->func_list), &(bmdi->exec_func.func_list));
		mutex_unlock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
	}

	PR_DEBUG("get func on core %d, func id = %d , func name = %s\n", func_node->exec_func.core_id,
				func_node->exec_func.f_id, func_node->exec_func.func_name);

	ret = copy_to_user((bm_get_func_t __user *)p_bm_api->api_addr, &func_node->exec_func, sizeof(bm_get_func_t));
	if (ret) {
		pr_err("bm-sophon%d %s %d copy_to_user fail\n", bmdi->dev_index, __FILE__, __LINE__);
		return ret;
	}

	if (fun_flag == 1) {
		kfree(func_node);
		return -1;
	}
	return ret;
}

int bmdrv_api_dyn_load_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api, struct file *file)
{
	int ret;
	bm_api_dyn_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_node;
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;
	int loaded_lib = 0;

	ret = copy_from_user((u8 *)&api_cpu_load_library_internal, (bm_api_dyn_cpu_load_library_internal_t __user *)p_bm_api->api_addr, sizeof(bm_api_dyn_cpu_load_library_internal_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if(!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN) && lib_temp->core_id == p_bm_api->core_id) {
			if (lib_temp->file == file) {
				lib_temp->refcount++;
				mutex_unlock(&lib_info->bmcpu_lib_mutex);
				return -1;
			} else {
				loaded_lib = 1;
			}
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	lib_node = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);
	strncpy(lib_node->lib_name, api_cpu_load_library_internal.lib_name, LIB_MAX_NAME_LEN);
	lib_node->core_id = p_bm_api->core_id;
	lib_node->refcount = 1;
	memcpy(lib_node->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN);
	lib_node->file = file;
	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_add_tail(&(lib_node->lib_list), &(lib_info->lib_list));
	mutex_unlock(&lib_info->bmcpu_lib_mutex);
	ret = copy_to_user((u8 __user *)p_bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_dyn_cpu_load_library_internal_t));
	if (ret) {
		pr_err("bm-sophon%d copy_to_user fail\n", bmdi->dev_index);
		return ret;
	}

	if (loaded_lib) {
		return -1;
	} else {
		return 0;
	}
}

int bmdrv_api_dyn_unload_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api, struct file *file)
{
	int ret;
	bm_api_dyn_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;
	struct list_head *pos, *pos_next;
	int del_lib = 1;

	ret = copy_from_user((u8 *)&api_cpu_load_library_internal, (bm_api_dyn_cpu_load_library_internal_t __user *)p_bm_api->api_addr, sizeof(bm_api_dyn_cpu_load_library_internal_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if (!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN) && lib_temp->file == file
		&& p_bm_api->core_id == lib_temp->core_id) {
			lib_temp->refcount--;
			if (!lib_temp->refcount) {
				api_cpu_load_library_internal.cur_rec = lib_temp->cur_rec;
				list_del(&(lib_temp->lib_list));
				kfree(lib_temp);
			}
			break;
		}
	}

	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if(!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)
		&& p_bm_api->core_id == lib_temp->core_id) {
			del_lib = 0;
			break;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	if(del_lib == 1) {
		mutex_lock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		list_for_each_safe(pos, pos_next, &(bmdi->exec_func.func_list)) {
			if(!memcmp(((struct bmdrv_exec_func *)pos)->exec_func.md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)
			&& ((struct bmdrv_exec_func *)pos)->exec_func.core_id == p_bm_api->core_id) {
				list_del(&((struct bmdrv_exec_func *)pos)->func_list);
				kfree((struct bmdrv_exec_func *)pos);
			}
		}
		mutex_unlock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		ret = copy_to_user((u8 __user *)p_bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_dyn_cpu_load_library_internal_t));
		if (ret) {
			pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
			return ret;
		}
		return 0;
	}

	return -1;
}

int ksend_api(struct bm_device_info *bmdi, struct file *file, unsigned char *msg, int core_id)
{
	int ret = 0;
	struct bm_thread_info *thd_info;
	struct api_fifo_entry *api_entry;
	struct bm_api_info *apinfo;
	pid_t api_pid;
	bm_api_ext_t bm_api;
	bm_kapi_header_t api_header;
	u32 fifo_empty_number;
	struct bm_handle_info *h_info;
	u64 local_send_api_seq;
	u32 channel;
	int fifo_avail;
	int is_pm_enable = 0;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}

	bm_api.api_id = 0x90000004;
	bm_api.api_addr = (u8 *)msg;
	bm_api.api_size = sizeof(bm_api_dyn_cpu_load_library_internal_t);
	bm_api.api_handle = 0;
	bm_api.core_id = core_id;

	channel = BM_MSGFIFO_CHANNEL_XPU;
	apinfo = &bmdi->api_info[core_id][BM_MSGFIFO_CHANNEL_XPU];

	mutex_lock(&apinfo->api_mutex);
	api_pid = current->pid;
	/* check if current pid already recorded */
	thd_info = bmdrv_find_thread_info(h_info, api_pid);
	if (!thd_info) {
		thd_info = bmdrv_create_thread_info(h_info, api_pid);
		if (!thd_info) {
			mutex_unlock(&apinfo->api_mutex);
			pr_err("%s bm-sophon%d bmdrv: bmdrv_create_thread_info failed!\n",
				__func__, bmdi->dev_index);
			return -ENOMEM;
		}
	}

	fifo_empty_number = bm_api.api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32);

	/* init api fifo entry */
	api_entry = kmalloc(API_ENTRY_SIZE, GFP_KERNEL);
	if (!api_entry) {
		mutex_unlock(&apinfo->api_mutex);
		return -ENOMEM;
	}

	/* update global api sequence number */
	if (core_id == 0)
		local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq);
	else
		local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq1);
	/* update handle api sequence number */
	mutex_lock(&h_info->h_api_seq_mutex);
	h_info->h_send_api_seq[core_id] = local_send_api_seq;
	mutex_unlock(&h_info->h_api_seq_mutex);
	/* update last_api_seq of current thread */
	/* may overflow */
	thd_info->last_api_seq[core_id] = local_send_api_seq;
	thd_info->profile.sent_api_counter++;
	bmdi->profile.sent_api_counter++;

	api_header.api_id = bm_api.api_id;
	api_header.api_size = bm_api.api_size / sizeof(u32);
	api_header.api_handle = (u64)h_info->file;
	api_header.api_seq = thd_info->last_api_seq[core_id];
	api_header.duration = 0; /* not get from this area now */
	api_header.result = 0;

	/* insert api info to api fifo */
	api_entry->thd_info = thd_info;
	api_entry->h_info = h_info;
	api_entry->thd_api_seq[core_id] = thd_info->last_api_seq[core_id];
	api_entry->dev_api_seq = 0;
	api_entry->api_id = bm_api.api_id;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0) || (LINUX_VERSION_CODE == KERNEL_VERSION(4, 18, 0) \
      && CENTOS_KERNEL_FIX == 408))
	api_entry->sent_time_us = ktime_get_boottime_ns() / 1000;
#else
	api_entry->sent_time_us = ktime_get_boot_ns() / 1000;
#endif
	api_entry->global_api_seq = local_send_api_seq;
	api_entry->api_done_flag = 0;
	init_completion(&api_entry->api_done);

	PR_TRACE("bmdrv: %d last_api_seq is %lld\n", api_pid, thd_info->last_api_seq[core_id]);
	/*
	 *pr_info("bmdrv: %d sent_api_counter is %d --- completed_api_counger is %d", api_pid,
	 *		ti->profile.sent_api_counter, ti->profile.completed_api_counter);
	 *pr_info("bmdrv: %d send api id is %d\n", api_pid, bm_api.api_id);
	 */

	/* wait for available fifo space */
	if (bmdev_wait_msgfifo(bmdi, fifo_empty_number, bmdi->cinfo.delay_ms, channel, core_id)) {
		thd_info->last_api_seq[core_id]--;
		kfree(api_entry);
		mutex_unlock(&apinfo->api_mutex);
		pr_err("%s bm-sophon%d bmdrv: bmdev_wait_msgfifo timeout!\n",
			__func__, bmdi->dev_index);
		return -EBUSY;
	}

	mutex_lock(&apinfo->api_fifo_mutex);
	fifo_avail = kfifo_avail(&apinfo->api_fifo);
	if (fifo_avail >= API_ENTRY_SIZE) {
		kfifo_in(&apinfo->api_fifo, api_entry, API_ENTRY_SIZE);
		mutex_unlock(&apinfo->api_fifo_mutex);
	} else {
		dev_err(bmdi->dev, "api fifo full!%d\n", fifo_avail);
		pr_err("%s bm-sophon%d api fifo full!\n", __func__, bmdi->dev_index);
		thd_info->last_api_seq[core_id]--;
		kfree(api_entry);
		mutex_unlock(&apinfo->api_fifo_mutex);
		mutex_unlock(&apinfo->api_mutex);
		return -EBUSY;
	}
	kfree(api_entry);
	is_pm_enable = bmdi->pm_thread_info.is_pm_enable;
	/* copy api data to fifo */
	mutex_lock(&bmdi->pm_thread_info.pm_mutex);
		if (is_pm_enable == 1) {
			bmdi->pm_thread_info.is_clk_enable = 1;
			bm1688_tpu_clk_enable(bmdi);
			bm1688_gdma_clk_enable(bmdi);
			ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, NULL, channel, false);
		} else {
			ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, NULL, channel, false);
		}
	mutex_unlock(&bmdi->pm_thread_info.pm_mutex);

	mutex_unlock(&apinfo->api_mutex);
	return ret;
}

void bmdrv_api_clear_lib(struct bm_device_info *bmdi, struct file *file)
{
	bm_api_dyn_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_temp, *lib_temp2, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;
	struct list_head *pos, *pos_next;
	int del_lib = 1;
	int ret;
	int core_id;

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if (file == lib_temp->file) {
			memcpy(api_cpu_load_library_internal.md5, lib_temp->md5, MD5SUM_LEN);
			memcpy(api_cpu_load_library_internal.lib_name, lib_temp->lib_name, LIB_MAX_NAME_LEN);
			api_cpu_load_library_internal.cur_rec = lib_temp->cur_rec;
			core_id = lib_temp->core_id;
			list_del(&(lib_temp->lib_list));
			kfree(lib_temp);

			list_for_each_entry(lib_temp2, &lib_info->lib_list, lib_list) {
				if (!memcmp(lib_temp2->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN) && (lib_temp2->core_id == core_id)) {
					del_lib = 0;
					break;
				}
			}

			if (del_lib == 1) {
				ksend_api(bmdi, file, (unsigned char *)&api_cpu_load_library_internal, core_id);
				ret = bmdrv_thread_sync_api(bmdi, file, (~0));
				if (ret)
					pr_err("%s: sync api error!\n", __func__);


				list_for_each_safe(pos, pos_next, &(bmdi->exec_func.func_list)) {
					if(!memcmp(((struct bmdrv_exec_func *)pos)->exec_func.md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
						list_del(&((struct bmdrv_exec_func *)pos)->func_list);
						kfree((struct bmdrv_exec_func *)pos);
					}
				}
			}
			del_lib = 1;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	return;
}

int bmdrv_send_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg, bool flag, bool api_from_userspace)
{
	int ret = 0;
	struct bm_thread_info *thd_info;
	struct api_fifo_entry *api_entry = NULL;
	struct api_list_entry *api_entry_list = NULL;
	struct bm_api_info *apinfo, *apinfo_core0, *apinfo_core1;
	pid_t api_pid;
	bm_api_ext_t bm_api, *bm_api_p;
	bm_kapi_header_t api_header;
	bm_kapi_opt_header_t api_opt_header;
	int fifo_avail;
	u32 fifo_empty_number;
	struct bm_handle_info *h_info;
	u64 local_send_api_seq;
	u32 channel;
	int core_id = 0;
	unsigned int param_num;
	bm_api_ext_t *bm_api_list = NULL;
	int i;
	int is_pm_enable = 0;
	int func_id;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}

	if (api_from_userspace) {
		/*copy user data to bm_api */
		if (flag)
			ret = copy_from_user(&bm_api, (bm_api_ext_t __user *)arg, sizeof(bm_api_ext_t));
		else
			ret = copy_from_user(&bm_api, (bm_api_t __user *)arg, sizeof(bm_api_t));
		if (ret) {
			pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
			return ret;
		}
	} else {
		memcpy(&bm_api, (bm_api_t *)arg, sizeof(bm_api_t));
	}

	if (!flag)
		core_id = bm_api.core_id;

	if (core_id >= BM_MAX_CORE_NUM) {
		pr_err("bm-sophon%d core id %d not valid\n", bmdi->dev_index, core_id);
		return -EINVAL;
	}

	if (flag) {
#ifdef PCIE_MODE_ENABLE_CPU
		channel = BM_MSGFIFO_CHANNEL_CPU;
		apinfo = &bmdi->api_info[core_id][BM_MSGFIFO_CHANNEL_CPU];
		apinfo_core0 = &bmdi->api_info[0][BM_MSGFIFO_CHANNEL_CPU];
		apinfo_core1 = &bmdi->api_info[1][BM_MSGFIFO_CHANNEL_CPU];
#else
		pr_err("bm-sophon%d bmdrv: cpu api not enable!\n", bmdi->dev_index);
		return -EINVAL;
#endif
	} else {
		channel = BM_MSGFIFO_CHANNEL_XPU;
		apinfo = &bmdi->api_info[core_id][BM_MSGFIFO_CHANNEL_XPU];
		apinfo_core0 = &bmdi->api_info[0][BM_MSGFIFO_CHANNEL_XPU];
		apinfo_core1 = &bmdi->api_info[1][BM_MSGFIFO_CHANNEL_XPU];
	}

	PR_TRACE("[%s: %d] api_id=0x%x, core_id=0x%x\n", __func__, __LINE__, bm_api.api_id, bm_api.core_id);
	if (bm_api.api_id == 0x90000001) {
		ret = bmdrv_api_dyn_load_lib_process(bmdi, &bm_api, file);
		if (ret == -1) {
			PR_TRACE("bm-sophon%d lib already exist\n", bmdi->dev_index);
			return 0;
		}
	}

	if (bm_api.api_id == 0x90000002) {
		ret = bmdrv_api_dyn_get_func_process(bmdi, &bm_api);
		if (ret == -1) {
			PR_TRACE("bm-sophon%d function id already exist\n", bmdi->dev_index);
			return 0;
		}
	}

	if (bm_api.api_id == 0x90000004) {
		ret = bmdrv_api_dyn_unload_lib_process(bmdi, &bm_api, file);
		if (ret == -1) {
			PR_TRACE("bm-sophon%d waring: lib is using by other\n", bmdi->dev_index);
			return 0;
		}
	}

	if (bm_api.api_id == BM_API_ID_LOAD_LIBRARY) {
		ret = bmdrv_api_load_lib_process(bmdi, bm_api);
		if (ret != 0)
			return ret;
	}

	if (bm_api.api_id == BM_API_ID_UNLOAD_LIBRARY) {
		ret = bmdrv_api_unload_lib_process(bmdi, bm_api);
		if (ret != 0)
			return ret;
	}

	if (bm_api.api_id == 0x90000003) {
		ret = copy_from_user(&func_id, (bm_get_func_t __user *)bm_api.api_addr, 4);
		PR_DEBUG("lanuch fun %d to core %d\n", func_id, core_id);
	}

	param_num = 1;
	if (bm_api.api_id == 0x90000013) {
		tpu_launch_param_t *param_list = kmalloc(bm_api.api_size, GFP_KERNEL);
		param_num = bm_api.api_size/sizeof(tpu_launch_param_t);
		bm_api_list = kmalloc((sizeof(bm_api_ext_t) * param_num), GFP_KERNEL);

		ret = copy_from_user(param_list, (tpu_launch_param_t __user *)bm_api.api_addr, bm_api.api_size);
		//pr_debug("%s bm-sophon%d param_num =0x%x, param_list[0].param_size=0x%x\n",
		//					__func__, bmdi->dev_index, param_num, param_list[0].param_size);
		for(i=0; i<param_num; i++) {
			u32 *api_addr_tmp = kmalloc(param_list[i].param_size + 8, GFP_KERNEL);
			*(api_addr_tmp + 0) = param_list[i].func_id;
			*(api_addr_tmp + 1) = param_list[i].param_size;
			ret = copy_from_user((api_addr_tmp + 2), (tpu_launch_param_t __user *)param_list[i].param_data, param_list[i].param_size);

			bm_api_list[i].core_id = param_list[i].core_id;
			bm_api_list[i].api_id = 0x90000003;
			bm_api_list[i].api_addr = (u8 *)api_addr_tmp;
			bm_api_list[i].api_size = (param_list[i].param_size + 8);
			api_from_userspace = 0;
			PR_DEBUG("lanuch func %d to core %d\n", param_list[i].func_id, param_list[i].core_id);
		}
		kfree(param_list);
		mutex_lock(&apinfo_core0->api_mutex);
		mutex_lock(&apinfo_core1->api_mutex);
	} else {
		mutex_lock(&apinfo->api_mutex);
	}

	is_pm_enable = bmdi->pm_thread_info.is_pm_enable;

	api_pid = current->pid;
	for (i = 0; i < param_num; i++){
		if (bm_api.api_id == 0x90000013) {
			bm_api_p = &bm_api_list[i];
		} else {
			bm_api_p = &bm_api;
		}
		if (bm_api_p->core_id == 0) {
			apinfo = apinfo_core0;
		} else {
			apinfo = apinfo_core1;
		}
		core_id = bm_api_p->core_id;
		/* check if current pid already recorded */
		thd_info = bmdrv_find_thread_info(h_info, api_pid);
		if (!thd_info) {
			thd_info = bmdrv_create_thread_info(h_info, api_pid);
			if (!thd_info) {
				if (bm_api.api_id == 0x90000013) {
					mutex_unlock(&apinfo_core1->api_mutex);
					mutex_unlock(&apinfo_core0->api_mutex);
				} else {
					mutex_unlock(&apinfo->api_mutex);
				}
				pr_err("%s bm-sophon%d bmdrv: bmdrv_create_thread_info failed!\n",
					__func__, bmdi->dev_index);
				return -ENOMEM;
			}
		}

		if (BM_MSGFIFO_CHANNEL_XPU == channel) {
			fifo_empty_number = bm_api_p->api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32);

			/* init api fifo entry */
			api_entry = kmalloc(API_ENTRY_SIZE, GFP_KERNEL);
			if (!api_entry) {
				if (bm_api.api_id == 0x90000013) {
					mutex_unlock(&apinfo_core1->api_mutex);
					mutex_unlock(&apinfo_core0->api_mutex);
				} else {
					mutex_unlock(&apinfo->api_mutex);
				}
				return -ENOMEM;
			}
		} else {
			fifo_empty_number = bm_api_p->api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32) + sizeof(bm_kapi_opt_header_t) / sizeof(u32);

			api_entry_list = kmalloc(sizeof(struct api_list_entry), GFP_KERNEL);
			if (!api_entry_list) {
				if (bm_api.api_id == 0x90000013) {
					mutex_unlock(&apinfo_core1->api_mutex);
					mutex_unlock(&apinfo_core0->api_mutex);
				} else {
					mutex_unlock(&apinfo->api_mutex);
				}
				pr_err("%s bm-sophon%d bmdrv: kmalloc api_list_entry failed!\n",
					__func__, bmdi->dev_index);
				return -ENOMEM;
			}
			api_entry = &api_entry_list->api_entry;
		}

		/* update global api sequence number */
		if (core_id == 0)
			local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq);
		else
			local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq1);
		/* update handle api sequence number */
		mutex_lock(&h_info->h_api_seq_mutex);
		h_info->h_send_api_seq[core_id] = local_send_api_seq;
		mutex_unlock(&h_info->h_api_seq_mutex);
		/* update last_api_seq of current thread */
		/* may overflow */
		thd_info->last_api_seq[core_id] = local_send_api_seq;
		thd_info->profile.sent_api_counter++;
		bmdi->profile.sent_api_counter++;
		api_header.api_id = bm_api_p->api_id;
		api_header.api_size = bm_api_p->api_size / sizeof(u32);
		api_header.api_handle = (u64)h_info->file;
		api_header.api_seq = thd_info->last_api_seq[core_id];
		api_header.duration = 0; /* not get from this area now */
		api_header.result = 0;

		/* insert api info to api fifo */
		api_entry->thd_info = thd_info;
		api_entry->h_info = h_info;
		api_entry->thd_api_seq[core_id] = thd_info->last_api_seq[core_id];
		api_entry->dev_api_seq = 0;
		api_entry->api_id = bm_api_p->api_id;
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0) || (LINUX_VERSION_CODE == KERNEL_VERSION(4, 18, 0) \
		&& CENTOS_KERNEL_FIX >= 408))
		api_entry->sent_time_us = ktime_get_boottime_ns() / 1000;
	#else
		api_entry->sent_time_us = ktime_get_boot_ns() / 1000;
	#endif
		api_entry->global_api_seq = local_send_api_seq;
		api_entry->api_done_flag = 0;
		init_completion(&api_entry->api_done);

		PR_TRACE("bmdrv: %d last_api_seq is %lld\n", api_pid, thd_info->last_api_seq[core_id]);
		/*
		*pr_info("bmdrv: %d sent_api_counter is %d --- completed_api_counger is %d", api_pid,
		*		ti->profile.sent_api_counter, ti->profile.completed_api_counter);
		*pr_info("bmdrv: %d send api id is %d\n", api_pid, bm_api_p->api_id);
		*/

		/* wait for available fifo space */
		if (bmdev_wait_msgfifo(bmdi, fifo_empty_number, bmdi->cinfo.delay_ms, channel, core_id)) {
			thd_info->last_api_seq[core_id]--;
			kfree(api_entry);
			if (bm_api.api_id == 0x90000013) {
				mutex_unlock(&apinfo_core1->api_mutex);
				mutex_unlock(&apinfo_core0->api_mutex);
			} else {
				mutex_unlock(&apinfo->api_mutex);
			}
			pr_err("%s bm-sophon%d bmdrv: bmdev_wait_msgfifo timeout!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",
				__func__, bmdi->dev_index);
			return -EBUSY;
		}

		if (BM_MSGFIFO_CHANNEL_CPU == channel) {
			mutex_lock(&apinfo->api_fifo_mutex);
			list_add_tail(&(api_entry_list->api_list_node), &apinfo->api_list);
			mutex_unlock(&apinfo->api_fifo_mutex);

			api_opt_header.global_api_seq = local_send_api_seq;
			api_opt_header.api_data = 0;
			/* copy api data to fifo */
			mutex_lock(&bmdi->pm_thread_info.pm_mutex);
			if (is_pm_enable == 1) {
				bmdi->pm_thread_info.is_clk_enable = 1;
				bm1688_tpu_clk_enable(bmdi);
				bm1688_gdma_clk_enable(bmdi);
				ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)bm_api_p, &api_opt_header, channel, api_from_userspace);
			} else {
				ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)bm_api_p, &api_opt_header, channel, api_from_userspace);
			}
			mutex_unlock(&bmdi->pm_thread_info.pm_mutex);
		} else {
			mutex_unlock(&apinfo->api_fifo_mutex);
			fifo_avail = kfifo_avail(&apinfo->api_fifo);
			if (fifo_avail >= API_ENTRY_SIZE) {
				kfifo_in(&apinfo->api_fifo, api_entry, API_ENTRY_SIZE);
				mutex_unlock(&apinfo->api_fifo_mutex);
			} else {
				thd_info->last_api_seq[core_id]--;
				mutex_unlock(&apinfo->api_fifo_mutex);
				if (bm_api.api_id == 0x90000013) {
					mutex_unlock(&apinfo_core1->api_mutex);
					mutex_unlock(&apinfo_core0->api_mutex);
				} else {
					mutex_unlock(&apinfo->api_mutex);
				}
				dev_err(bmdi->dev, "api fifo full!%d\n", fifo_avail);
				pr_err("%s bm-sophon%d api fifo full!\n", __func__, bmdi->dev_index);
				kfree(api_entry);
				return -EBUSY;
			}
			kfree(api_entry);
			/* copy api data to fifo */
			mutex_lock(&bmdi->pm_thread_info.pm_mutex);
			if (is_pm_enable == 1) {
				bmdi->pm_thread_info.is_clk_enable = 1;
				bm1688_tpu_clk_enable(bmdi);
				bm1688_gdma_clk_enable(bmdi);
				ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)bm_api_p, NULL, channel, api_from_userspace);
			} else {
				ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)bm_api_p, NULL, channel, api_from_userspace);
			}
			mutex_unlock(&bmdi->pm_thread_info.pm_mutex);
		}
		if (bm_api.api_id == 0x90000013) {
			kfree(bm_api_p->api_addr);
		}
	}

	if (bm_api.api_id == 0x90000013) {
		mutex_unlock(&apinfo_core1->api_mutex);
		mutex_unlock(&apinfo_core0->api_mutex);
		kfree(bm_api_list);
	} else {
		mutex_unlock(&apinfo->api_mutex);
	}
	if (flag)
		put_user(api_entry->global_api_seq, (u64 __user *)&(((bm_api_ext_t __user *)arg)->api_handle));
	return ret;
}

int bmdrv_query_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	int ret;
	bm_api_data_t bm_api_data;
	u32 channel;
	u64 data;

	ret = copy_from_user(&bm_api_data, (bm_api_data_t __user *)arg, sizeof(bm_api_data_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	if (0 == (bm_api_data.api_id & 0x80000000))
		channel = BM_MSGFIFO_CHANNEL_XPU;
	else {
#ifdef PCIE_MODE_ENABLE_CPU
		channel = BM_MSGFIFO_CHANNEL_CPU;
#else
		pr_err("bm-sophon%d bmdrv: cpu api not enable!\n", bmdi->dev_index);
		return -EINVAL;
#endif
	}

	ret = bmdev_msgfifo_get_api_data(bmdi, channel, bm_api_data.api_handle, &data, bm_api_data.timeout);
	if (0 == ret)
		put_user(data, (u64 __user *)&(((bm_api_data_t __user *)arg)->data));

	return ret;
}

#if SYNC_API_INT_MODE == 1
int bmdrv_thread_sync_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	int ret = 1;
	pid_t api_pid;
	struct bm_thread_info *thd_info;
	int timeout_ms = bmdi->cinfo.delay_ms;
	struct bm_handle_info *h_info;
	int core_id;

	if (arg == (~0) && bmdi->core_id >=0 && bmdi->core_id <=1) { // kernel space
		core_id = bmdi->core_id;
	} else { // user space
		ret = copy_from_user(&core_id, (int __user *)arg, sizeof(int));
		if (ret) {
			pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
			return ret;
		}
	}

	if (core_id >= BM_MAX_CORE_NUM) {
		pr_err("bm-sophon%d core id %d not valid\n", bmdi->dev_index, core_id);
		return -EINVAL;
	}

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
	api_pid = current->pid;
	PR_TRACE("bmdrv: %d sync api\n", api_pid);
	thd_info = bmdrv_find_thread_info(h_info, api_pid);
	if (!thd_info) {
		PR_TRACE("bm-sophon%d thread not recorded!\n",  bmdi->dev_index);
		return 0;
	}
	ret = 1;
	while ((thd_info->cpl_api_seq[core_id] != thd_info->last_api_seq[core_id]) && (ret != 0)) {
		PR_TRACE("bm-sophon%d bmdrv: %d core_id: %d sync api, last is %llu -- cpl is %llu\n",
				bmdi->dev_index, api_pid, core_id, thd_info->last_api_seq[core_id], thd_info->cpl_api_seq[core_id]);
		ret = wait_for_completion_timeout(&thd_info->msg_done, msecs_to_jiffies(timeout_ms));
	}

	if (ret)
		return 0;

	pr_err("bm-sophon%d %s, wait api timeout, wait %dms, core_id=%d\n",
		 bmdi->dev_index, __func__, timeout_ms, core_id);
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_XPU, core_id);
	if (bmdev_debug_tpusys(bmdi, core_id))
		return -EBUSY;
	else
		return 0;
}

int bmdrv_handle_sync_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	int ret = 1;
	int timeout_ms = bmdi->cinfo.delay_ms;
	struct bm_handle_info *h_info;
	u64 handle_send_api_seq = 0;
	int core_id;

	ret = copy_from_user(&core_id, (int __user *)arg, sizeof(int));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	if (core_id >= BM_MAX_CORE_NUM) {
		pr_err("bm-sophon%d core id %d not valid\n", bmdi->dev_index, core_id);
		return -EINVAL;
	}

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}
	mutex_lock(&h_info->h_api_seq_mutex);
	handle_send_api_seq = h_info->h_send_api_seq[core_id];
	mutex_unlock(&h_info->h_api_seq_mutex);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif

	ret = wait_event_timeout(h_info->h_msg_done, (h_info->h_cpl_api_seq[core_id] >= handle_send_api_seq), msecs_to_jiffies(timeout_ms));

	if (ret)
		return 0;
	pr_err("bm-sophon%d %s, wait api timeout\n", bmdi->dev_index, __func__);
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_XPU, core_id);
	bmdev_dump_reg(bmdi, BM_MSGFIFO_CHANNEL_XPU, core_id);
#ifdef PCIE_MODE_ENABLE_CPU
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_CPU, 0);
#endif
	return -EBUSY;
}
#else
int bmdrv_thread_sync_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	int polling_ms = bmdi->cinfo.polling_ms;
	int cnt = 10;
	int ret;
	int core_id;

	ret = copy_from_user(&core_id, (int __user *)arg, sizeof(int));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	if (core_id >= BM_MAX_CORE_NUM) {
		pr_err("bm-sophon%d core id %d not valid\n", bmdi->dev_index, core_id);
		return -EINVAL;
	}

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		polling_ms *= PALLADIUM_CLK_RATIO;
#endif
	while (gp_reg_read_idx(bmdi, GP_REG_MESSAGE_WP, core_id) != gp_reg_read_idx(bmdi, GP_REG_MESSAGE_RP, core_id)) {
		msleep(polling_ms);
		cnt--;
		pr_info("wait polling api done! %d \n", polling_ms);
		if (cnt < 0) {
			pr_info("sync api time out \n");
			break;
		}
	}
	return 0;
}

int bmdrv_handle_sync_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	return bmdrv_thread_sync_api(bmdi, file, arg);
}
#endif

int bmdrv_device_sync_api(struct bm_device_info *bmdi)
{
	struct api_fifo_entry *api_entry;
	int fifo_avail;
	struct bm_api_info *apinfo;
	int ret = 1;
	u64 dev_sync_last;
	int timeout_ms = bmdi->cinfo.delay_ms;
	u32 core_num = bmdi->cinfo.tpu_core_num;
	u32 core;
	u32 channel;
	struct api_list_entry *api_entry_list;

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
	for (core = 0; core < core_num; core++) {
		for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
			apinfo = &bmdi->api_info[core][channel];

			mutex_lock(&apinfo->api_mutex);

			if (channel == BM_MSGFIFO_CHANNEL_XPU) {
				api_entry = kmalloc(sizeof(struct api_fifo_entry), GFP_KERNEL);
				if (!api_entry) {
					mutex_unlock(&apinfo->api_mutex);
					return -ENOMEM;
				}
				mutex_lock(&apinfo->api_fifo_mutex);

				/* if fifo empty, return success */
				if (kfifo_is_empty(&apinfo->api_fifo)) {
					mutex_unlock(&apinfo->api_fifo_mutex);
					kfree(api_entry);
					mutex_unlock(&apinfo->api_mutex);
					return 0;
				}
			} else {
				mutex_lock(&apinfo->api_fifo_mutex);
				if (bmdev_msgfifo_empty(bmdi, BM_MSGFIFO_CHANNEL_CPU, core)) {
					mutex_unlock(&apinfo->api_fifo_mutex);
					mutex_unlock(&apinfo->api_mutex);
					return 0;
				}
				api_entry_list = kmalloc(sizeof(struct api_list_entry), GFP_KERNEL);
				if (!api_entry_list) {
					mutex_unlock(&apinfo->api_fifo_mutex);
					mutex_unlock(&apinfo->api_mutex);
					return -ENOMEM;
				}
				api_entry = &api_entry_list->api_entry;
			}

			/* insert device sync marker into fifo */
			apinfo->device_sync_last++;
			api_entry->thd_info = (struct bm_thread_info *)DEVICE_SYNC_MARKER;
			api_entry->thd_api_seq[0] = 0;
			api_entry->dev_api_seq = apinfo->device_sync_last;
			/* record device sync last; apinfo is global; its value may be changed */
			dev_sync_last = apinfo->device_sync_last;

			if (channel == BM_MSGFIFO_CHANNEL_XPU) {
				fifo_avail = kfifo_avail(&apinfo->api_fifo);
				if (fifo_avail >= API_ENTRY_SIZE) {
					kfifo_in(&apinfo->api_fifo, api_entry, API_ENTRY_SIZE);
					mutex_unlock(&apinfo->api_fifo_mutex);
					kfree(api_entry);
					mutex_unlock(&apinfo->api_mutex);
				} else {
					dev_err(bmdi->dev, "api fifo full!%d\n", fifo_avail);
					apinfo->device_sync_last--;
					mutex_unlock(&apinfo->api_fifo_mutex);
					kfree(api_entry);
					mutex_unlock(&apinfo->api_mutex);
					return -EBUSY;
				}
			} else {
				list_add_tail(&(api_entry_list->api_list_node), &apinfo->api_list);
				mutex_unlock(&apinfo->api_fifo_mutex);
				mutex_unlock(&apinfo->api_mutex);
			}
		}
	}

	for (core = 0; core < core_num; core++) {
		for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
			apinfo = &bmdi->api_info[core][channel];
			/* wait until device sync marker processed */
			while ((apinfo->device_sync_cpl != dev_sync_last) && (ret != 0))
				ret = wait_for_completion_timeout(&apinfo->dev_sync_done, msecs_to_jiffies(timeout_ms));

			if (ret)
				continue;
			pr_err("bm-sophon%d %s, wait api timeout\n", bmdi->dev_index, __func__);
			bmdev_dump_msgfifo(bmdi, channel, core);
			bmdev_dump_reg(bmdi, channel, core);
			return -EBUSY;
		}
	}

	return 0;
}

void bmdrv_clear_lib_list(struct bm_device_info *bmdi)
{
	struct bmcpu_lib *lib_temp;
	struct bmcpu_lib *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;

	if (!lib_info) {
		pr_info("The library list is empty\n");
		return;
	}

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		list_del(&(lib_temp->lib_list));
		kfree(lib_temp);
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);
}

void bmdrv_clear_func_list(struct bm_device_info *bmdi)
{
	struct list_head *pos, *pos_next;
	struct bmdrv_exec_func *func;

	if (list_empty(&bmdi->exec_func.func_list)) {
		pr_info("The function list is empty.\n");
		return;
	}

	// Assuming there is a mutex in bm_device_info or exec_func structure
	mutex_lock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));

	list_for_each_safe(pos, pos_next, &bmdi->exec_func.func_list) {
		func = list_entry(pos, struct bmdrv_exec_func, func_list);
		list_del(&func->func_list);
		kfree(func);
	}

	mutex_unlock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
}
void print_dny_lib_info(struct bm_device_info *bmdi)
{
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;

	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		pr_err("lib_name=%s,file=%p,refcount=%d\n", lib_temp->lib_name, lib_temp->file,
				lib_temp->refcount);
	}
}
