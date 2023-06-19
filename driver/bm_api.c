#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/version.h>
#include "bm_common.h"
#include "bm_api.h"
#include "bm_msgfifo.h"
#include "bm_thread.h"

DEFINE_SPINLOCK(msg_dump_lock);
void bmdev_dump_reg(struct bm_device_info *bmdi, u32 channel)
{
    int i=0;
    spin_lock(&msg_dump_lock);
    if (GP_REG_MESSAGE_WP_CHANNEL_XPU == channel) {
        for(i=0; i<32; i++)
            printk("DEV %d BDC_CMD_REG %d: addr= 0x%08x, value = 0x%08x\n", bmdi-> dev_index, i, bmdi->cinfo.bm_reg->tpu_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + i*4));
        for(i=0; i<64; i++)
            printk("DEV %d BDC_CTL_REG %d: addr= 0x%08x, value = 0x%08x\n", bmdi-> dev_index, i, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr + 0x100 + i*4));
        for(i=0; i<32; i++)
            printk("DEV %d GDMA_ALL_REG %d: addr= 0x%08x, value = 0x%08x\n", bmdi-> dev_index, i, bmdi->cinfo.bm_reg->gdma_base_addr + i*4, bm_read32(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr + i*4));
       }
    else {
    }
    spin_unlock(&msg_dump_lock);
}

int bmdrv_api_init(struct bm_device_info *bmdi, u32 channel)
{
	int ret = 0;
	struct bm_api_info *apinfo = &bmdi->api_info[channel];
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
		bdc_reg_write(bmdi, 0x100, val| 0x1);
	}
#endif

	if (BM_MSGFIFO_CHANNEL_XPU == channel) {
		ret = kfifo_alloc(&apinfo->api_fifo, bmdi->cinfo.share_mem_size * 4, GFP_KERNEL);

		bmdi->lib_dyn_info = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);
		INIT_LIST_HEAD(&(bmdi->lib_dyn_info->lib_list));
		mutex_init(&(bmdi->lib_dyn_info->bmcpu_lib_mutex));
		INIT_LIST_HEAD(&(bmdi->exec_func.func_list));
		mutex_init(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_WP_CHANNEL_XPU, 0);
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_RP_CHANNEL_XPU, 0);
	} else {
		INIT_LIST_HEAD(&apinfo->api_list);

		bmdi->lib_info = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);
		INIT_LIST_HEAD(&(bmdi->lib_info->lib_list));
		mutex_init(&(bmdi->lib_info->bmcpu_lib_mutex));
	}

	return ret;
}

void bmdrv_api_deinit(struct bm_device_info *bmdi, u32 channel)
{
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_info;
	struct bmcpu_lib *lib_dyn_info = bmdi->lib_dyn_info;

	if (BM_MSGFIFO_CHANNEL_XPU == channel) {
		mutex_lock(&lib_dyn_info->bmcpu_lib_mutex);
		list_for_each_entry_safe(lib_temp, lib_next, &lib_dyn_info->lib_list, lib_list) {
			list_del(&lib_temp->lib_list);
			kfree(lib_temp);
		}
		mutex_unlock(&lib_dyn_info->bmcpu_lib_mutex);
		kfree(bmdi->lib_dyn_info);
	} else {
		mutex_lock(&lib_info->bmcpu_lib_mutex);
		list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
			list_del(&lib_temp->lib_list);
			kfree(lib_temp);
		}
		mutex_unlock(&lib_info->bmcpu_lib_mutex);
		kfree(bmdi->lib_info);
	}

	kfifo_free(&bmdi->api_info[channel].api_fifo);
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
					for (i = 0; i < 50; i++) {
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

	list_for_each(pos, &(bmdi->exec_func.func_list))
    {
        if(!memcmp(((struct bmdrv_exec_func *)pos)->exec_func.md5, func_node->exec_func.md5, MD5SUM_LEN) &&
			!strncmp(((struct bmdrv_exec_func *)pos)->exec_func.func_name, func_node->exec_func.func_name, FUNC_MAX_NAME_LEN)) {
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

int bmdrv_api_dyn_load_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api)
{
	int ret;
	bm_api_dyn_cpu_load_library_internal_t api_cpu_load_library_internal;
	struct bmcpu_lib *lib_node;
	struct bmcpu_lib *lib_temp, *lib_next;
	struct bmcpu_lib *lib_info = bmdi->lib_dyn_info;

	ret = copy_from_user((u8 *)&api_cpu_load_library_internal, (bm_api_dyn_cpu_load_library_internal_t __user *)p_bm_api->api_addr, sizeof(bm_api_dyn_cpu_load_library_internal_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if(!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
			lib_node = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);
			strncpy(lib_node->lib_name, api_cpu_load_library_internal.lib_name, LIB_MAX_NAME_LEN);
			lib_node->cur_rec = lib_temp->cur_rec;
			lib_node->rec[lib_node->cur_rec] = 1;
			memcpy(lib_node->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN);
			lib_node->cur_pid = current->pid;
			list_add_tail(&(lib_node->lib_list), &(lib_info->lib_list));
			mutex_unlock(&lib_info->bmcpu_lib_mutex);
			return -1;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if(!strncmp(lib_temp->lib_name, api_cpu_load_library_internal.lib_name, LIB_MAX_NAME_LEN)) {
			(api_cpu_load_library_internal.cur_rec)++;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	lib_node = kzalloc(sizeof(struct bmcpu_lib), GFP_KERNEL);

	strncpy(lib_node->lib_name, api_cpu_load_library_internal.lib_name, LIB_MAX_NAME_LEN);
	lib_node->cur_rec = api_cpu_load_library_internal.cur_rec;
	lib_node->rec[lib_node->cur_rec] = 1;
	memcpy(lib_node->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN);
	lib_node->cur_pid = current->pid;
	list_add_tail(&(lib_node->lib_list), &(lib_info->lib_list));
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	ret = copy_to_user((u8 __user *)p_bm_api->api_addr, &api_cpu_load_library_internal, sizeof(bm_api_dyn_cpu_load_library_internal_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	return 0;
}

int bmdrv_api_dyn_unload_lib_process(struct bm_device_info *bmdi, bm_api_ext_t *p_bm_api)
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
		if(!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN) && current->pid == lib_temp->cur_pid) {
			api_cpu_load_library_internal.cur_rec = lib_temp->cur_rec;
			list_del(&(lib_temp->lib_list));
			kfree(lib_temp);
			break;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if(!memcmp(lib_temp->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
			del_lib = 0;
			break;
		}
	}
	mutex_unlock(&lib_info->bmcpu_lib_mutex);

	if(del_lib == 1) {
		mutex_lock(&(bmdi->exec_func.exec_func.bm_get_func_mutex));
		list_for_each_safe(pos, pos_next, &(bmdi->exec_func.func_list)) {
			if(!memcmp(((struct bmdrv_exec_func *)pos)->exec_func.md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
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

int ksend_api(struct bm_device_info *bmdi, struct file *file, unsigned char *msg)
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

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}

	bm_api.api_id = 0x90000004;
	bm_api.api_addr = (u8 *)msg;
	bm_api.api_size = sizeof(bm_api_dyn_cpu_load_library_internal_t);
	bm_api.api_handle = 0;

	channel = BM_MSGFIFO_CHANNEL_XPU;
	apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU];

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
	local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq);
	/* update handle api sequence number */
	mutex_lock(&h_info->h_api_seq_mutex);
	h_info->h_send_api_seq = local_send_api_seq;
	mutex_unlock(&h_info->h_api_seq_mutex);
	/* update last_api_seq of current thread */
	/* may overflow */
	thd_info->last_api_seq = local_send_api_seq;
	thd_info->profile.sent_api_counter++;
	bmdi->profile.sent_api_counter++;

	api_header.api_id = bm_api.api_id;
	api_header.api_size = bm_api.api_size / sizeof(u32);
	api_header.api_handle = (u64)h_info->file;
	api_header.api_seq = thd_info->last_api_seq;
	api_header.duration = 0; /* not get from this area now */
	api_header.result = 0;

	/* insert api info to api fifo */
	api_entry->thd_info = thd_info;
	api_entry->h_info = h_info;
	api_entry->thd_api_seq = thd_info->last_api_seq;
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

	PR_TRACE("bmdrv: %d last_api_seq is %d\n", api_pid, thd_info->last_api_seq);
	/*
	 *pr_info("bmdrv: %d sent_api_counter is %d --- completed_api_counger is %d", api_pid,
	 *		ti->profile.sent_api_counter, ti->profile.completed_api_counter);
	 *pr_info("bmdrv: %d send api id is %d\n", api_pid, bm_api.api_id);
	 */

	/* wait for available fifo space */
	if (bmdev_wait_msgfifo(bmdi, fifo_empty_number, bmdi->cinfo.delay_ms, channel)) {
		thd_info->last_api_seq--;
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
		thd_info->last_api_seq--;
		kfree(api_entry);
		mutex_unlock(&apinfo->api_fifo_mutex);
		mutex_unlock(&apinfo->api_mutex);
		return -EBUSY;
	}
	kfree(api_entry);
	/* copy api data to fifo */
	ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, NULL, channel, false);

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

	mutex_lock(&lib_info->bmcpu_lib_mutex);
	list_for_each_entry_safe(lib_temp, lib_next, &lib_info->lib_list, lib_list) {
		if (current->pid == lib_temp->cur_pid) {
			memcpy(api_cpu_load_library_internal.md5, lib_temp->md5, MD5SUM_LEN);
			memcpy(api_cpu_load_library_internal.lib_name, lib_temp->lib_name, LIB_MAX_NAME_LEN);
			api_cpu_load_library_internal.cur_rec = lib_temp->cur_rec;
			list_del(&(lib_temp->lib_list));
			kfree(lib_temp);

			list_for_each_entry(lib_temp2, &lib_info->lib_list, lib_list) {
				if (!memcmp(lib_temp2->md5, api_cpu_load_library_internal.md5, MD5SUM_LEN)) {
					del_lib = 0;
					break;
				}
			}

			if (del_lib == 1) {
				ksend_api(bmdi, file, (unsigned char *)&api_cpu_load_library_internal);
				ret = bmdrv_thread_sync_api(bmdi, file);
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

int bmdrv_send_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg, bool flag)
{
	int ret = 0;
	struct bm_thread_info *thd_info;
	struct api_fifo_entry *api_entry;
	struct api_list_entry *api_entry_list = NULL;
	struct bm_api_info *apinfo;
	pid_t api_pid;
	bm_api_ext_t bm_api;
	bm_kapi_header_t api_header;
	bm_kapi_opt_header_t api_opt_header;
	int fifo_avail;
	u32 fifo_empty_number;
	struct bm_handle_info *h_info;
	u64 local_send_api_seq;
	u32 channel;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}

	/*copy user data to bm_api */
	if (flag)
		ret = copy_from_user(&bm_api, (bm_api_ext_t __user *)arg, sizeof(bm_api_ext_t));
	else
		ret = copy_from_user(&bm_api, (bm_api_t __user *)arg, sizeof(bm_api_t));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}

	if (flag) {
#ifdef PCIE_MODE_ENABLE_CPU
		channel = BM_MSGFIFO_CHANNEL_CPU;
		apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU];
#else
		pr_err("bm-sophon%d bmdrv: cpu api not enable!\n", bmdi->dev_index);
		return -EINVAL;
#endif
	} else {
		channel = BM_MSGFIFO_CHANNEL_XPU;
		apinfo = &bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU];
	}

	if (bm_api.api_id == 0x90000001) {
		ret = bmdrv_api_dyn_load_lib_process(bmdi, &bm_api);
		if(ret == -1) {
			PR_TRACE("bm-sophon%d lib already exist\n", bmdi->dev_index);
			return 0;
		}
	}

	if (bm_api.api_id == 0x90000002) {
		ret = bmdrv_api_dyn_get_func_process(bmdi, &bm_api);
		if(ret == -1) {
			PR_TRACE("bm-sophon%d function id already exist\n", bmdi->dev_index);
			return 0;
		}
	}

	if (bm_api.api_id == 0x90000004) {
		ret = bmdrv_api_dyn_unload_lib_process(bmdi, &bm_api);
		if(ret == -1) {
			PR_TRACE("bm-sophon%d lib is using by other\n", bmdi->dev_index);
			return 0;
		}
	}

	if (bm_api.api_id == BM_API_ID_LOAD_LIBRARY) {
		ret = bmdrv_api_load_lib_process(bmdi, bm_api);
		if(ret != 0) {
			return ret;
		}
	}

	if (bm_api.api_id == BM_API_ID_UNLOAD_LIBRARY) {
		ret = bmdrv_api_unload_lib_process(bmdi, bm_api);
		if(ret != 0) {
			return ret;
		}
	}

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

	if (BM_MSGFIFO_CHANNEL_XPU == channel) {
		fifo_empty_number = bm_api.api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32);

		/* init api fifo entry */
		api_entry = kmalloc(API_ENTRY_SIZE, GFP_KERNEL);
		if (!api_entry) {
			mutex_unlock(&apinfo->api_mutex);
			return -ENOMEM;
		}
	} else {
		fifo_empty_number = bm_api.api_size / sizeof(u32) + sizeof(bm_kapi_header_t) / sizeof(u32) + sizeof(bm_kapi_opt_header_t) / sizeof(u32);

		api_entry_list = kmalloc(sizeof(struct api_list_entry), GFP_KERNEL);
		if (!api_entry_list) {
			mutex_unlock(&apinfo->api_mutex);
			pr_err("%s bm-sophon%d bmdrv: kmalloc api_list_entry failed!\n",
				__func__, bmdi->dev_index);
			return -ENOMEM;
		}
		api_entry = &api_entry_list->api_entry;
	}

	/* update global api sequence number */
	local_send_api_seq = atomic64_inc_return((atomic64_t *)&bmdi->bm_send_api_seq);
	/* update handle api sequence number */
	mutex_lock(&h_info->h_api_seq_mutex);
	h_info->h_send_api_seq = local_send_api_seq;
	mutex_unlock(&h_info->h_api_seq_mutex);
	/* update last_api_seq of current thread */
	/* may overflow */
	thd_info->last_api_seq = local_send_api_seq;
	thd_info->profile.sent_api_counter++;
	bmdi->profile.sent_api_counter++;

	api_header.api_id = bm_api.api_id;
	api_header.api_size = bm_api.api_size / sizeof(u32);
	api_header.api_handle = (u64)h_info->file;
	api_header.api_seq = thd_info->last_api_seq;
	api_header.duration = 0; /* not get from this area now */
	api_header.result = 0;

	/* insert api info to api fifo */
	api_entry->thd_info = thd_info;
	api_entry->h_info = h_info;
	api_entry->thd_api_seq = thd_info->last_api_seq;
	api_entry->dev_api_seq = 0;
	api_entry->api_id = bm_api.api_id;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0) || (LINUX_VERSION_CODE == KERNEL_VERSION(4, 18, 0) \
      && CENTOS_KERNEL_FIX >= 408))
	api_entry->sent_time_us = ktime_get_boottime_ns() / 1000;
#else
	api_entry->sent_time_us = ktime_get_boot_ns() / 1000;
#endif
	api_entry->global_api_seq = local_send_api_seq;
	api_entry->api_done_flag = 0;
	init_completion(&api_entry->api_done);

	PR_TRACE("bmdrv: %d last_api_seq is %d\n", api_pid, thd_info->last_api_seq);
	/*
	 *pr_info("bmdrv: %d sent_api_counter is %d --- completed_api_counger is %d", api_pid,
	 *		ti->profile.sent_api_counter, ti->profile.completed_api_counter);
	 *pr_info("bmdrv: %d send api id is %d\n", api_pid, bm_api.api_id);
	 */

	/* wait for available fifo space */
	if (bmdev_wait_msgfifo(bmdi, fifo_empty_number, bmdi->cinfo.delay_ms, channel)) {
		thd_info->last_api_seq--;
		kfree(api_entry);
		mutex_unlock(&apinfo->api_mutex);
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
		ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, &api_opt_header, channel, true);
	} else {
		mutex_lock(&apinfo->api_fifo_mutex);
		fifo_avail = kfifo_avail(&apinfo->api_fifo);
		if (fifo_avail >= API_ENTRY_SIZE) {
			kfifo_in(&apinfo->api_fifo, api_entry, API_ENTRY_SIZE);
			mutex_unlock(&apinfo->api_fifo_mutex);
		} else {
			dev_err(bmdi->dev, "api fifo full!%d\n", fifo_avail);
			pr_err("%s bm-sophon%d api fifo full!\n", __func__, bmdi->dev_index);
			thd_info->last_api_seq--;
			kfree(api_entry);
			mutex_unlock(&apinfo->api_fifo_mutex);
			mutex_unlock(&apinfo->api_mutex);
			return -EBUSY;
		}
		kfree(api_entry);
		/* copy api data to fifo */
		ret = bmdev_copy_to_msgfifo(bmdi, &api_header, (bm_api_t *)&bm_api, NULL, channel, true);
	}

	if (flag)
		put_user(api_entry->global_api_seq, (u64 __user *)&(((bm_api_ext_t __user *)arg)->api_handle));

	mutex_unlock(&apinfo->api_mutex);
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
int bmdrv_thread_sync_api(struct bm_device_info *bmdi, struct file *file)
{
	int ret = 1;
	pid_t api_pid;
	struct bm_thread_info *thd_info;
	int timeout_ms = bmdi->cinfo.delay_ms;
	struct bm_handle_info *h_info;

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

	while ((thd_info->cpl_api_seq != thd_info->last_api_seq) && (ret != 0)) {
		PR_TRACE("bm-sophon%d bmdrv: %d sync api, last is %llu -- cpl is %llu\n",
				bmdi->dev_index, api_pid, thd_info->last_api_seq, thd_info->cpl_api_seq);
		ret = wait_for_completion_timeout(&thd_info->msg_done, msecs_to_jiffies(timeout_ms));
	}

	if (ret)
		return 0;

	pr_err("bm-sophon%d %s, wait api timeout\n", bmdi->dev_index, __func__);
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_XPU);
	bmdev_dump_reg(bmdi, BM_MSGFIFO_CHANNEL_XPU);
#ifdef PCIE_MODE_ENABLE_CPU
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_CPU);
#endif
	return -EBUSY;
}

int bmdrv_handle_sync_api(struct bm_device_info *bmdi, struct file *file)
{
	int ret = 1;
	int timeout_ms = bmdi->cinfo.delay_ms;
	struct bm_handle_info *h_info;
	u64 handle_send_api_seq = 0;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bm-sophon%d bmdrv: file list is not found!\n", bmdi->dev_index);
		return -EINVAL;
	}
	mutex_lock(&h_info->h_api_seq_mutex);
	handle_send_api_seq = h_info->h_send_api_seq;
	mutex_unlock(&h_info->h_api_seq_mutex);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif

	ret = wait_event_timeout(h_info->h_msg_done, (h_info->h_cpl_api_seq >= handle_send_api_seq), msecs_to_jiffies(timeout_ms));

	if (ret)
		return 0;
	pr_err("bm-sophon%d %s, wait api timeout\n", bmdi->dev_index, __func__);
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_XPU);
	bmdev_dump_reg(bmdi, BM_MSGFIFO_CHANNEL_XPU);
#ifdef PCIE_MODE_ENABLE_CPU
	bmdev_dump_msgfifo(bmdi, BM_MSGFIFO_CHANNEL_CPU);
#endif
	return -EBUSY;
}
#else
int bmdrv_thread_sync_api(struct bm_device_info *bmdi, struct file *file)
{
	int polling_ms = bmdi->cinfo.polling_ms;
	int cnt = 10;

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		polling_ms *= PALLADIUM_CLK_RATIO;
#endif
	while (gp_reg_read_enh(bmdi, GP_REG_MESSAGE_WP) != gp_reg_read_enh(bmdi, GP_REG_MESSAGE_RP)) {
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

int bmdrv_handle_sync_api(struct bm_device_info *bmdi, struct file *file)
{
	return bmdrv_thread_sync_api(bmdi, file);
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
	u32 channel;
	struct api_list_entry *api_entry_list;

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		apinfo = &bmdi->api_info[channel];

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
			if (bmdev_msgfifo_empty(bmdi, BM_MSGFIFO_CHANNEL_CPU)) {
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
		api_entry->thd_api_seq = 0;
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

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		apinfo = &bmdi->api_info[channel];
		/* wait until device sync marker processed */
		while ((apinfo->device_sync_cpl != dev_sync_last) && (ret != 0))
			ret = wait_for_completion_timeout(&apinfo->dev_sync_done, msecs_to_jiffies(timeout_ms));

		if (ret)
			continue;
		pr_err("bm-sophon%d %s, wait api timeout\n", bmdi->dev_index, __func__);
		bmdev_dump_msgfifo(bmdi, channel);
		bmdev_dump_reg(bmdi, channel);
		return -EBUSY;
	}

	return 0;
}
