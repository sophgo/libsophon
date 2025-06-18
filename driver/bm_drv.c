#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/mempool.h>
#include "bm_common.h"
#include "bm_msgfifo.h"
#include "bm_drv.h"
#include "bm_thread.h"
#include "bm_debug.h"
#include <linux/version.h>

/* be carefull with global variables, keep multi-card support in mind */
dev_t bm_devno_base;
dev_t bm_ctl_devno_base;

static void bmdrv_print_cardinfo(struct chip_info *cinfo)
{
#ifndef SOC_MODE
	u16 pcie_version;
	u16 index;
#endif
	dev_info(cinfo->device, "bar0 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar0_start, cinfo->bar_info.bar0_len,
			cinfo->bar_info.bar0_vaddr);
	dev_info(cinfo->device, "bar1 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar1_start, cinfo->bar_info.bar1_len,
			cinfo->bar_info.bar1_vaddr);
	dev_info(cinfo->device, "bar2 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar2_start, cinfo->bar_info.bar2_len,
			cinfo->bar_info.bar2_vaddr);
#ifndef SOC_MODE
	for(index=0; index<PCIE_BAR1_PART_MAX; index++) {
		dev_info(cinfo->device, "bar1 part offset 0x%llx size 0x%llx, dev_addr = 0x%llx\n",
			cinfo->bar_info.bar1_part_info[index].offset, cinfo->bar_info.bar1_part_info[index].len,
			cinfo->bar_info.bar1_part_info[index].dev_start);
	}

	pcie_version = pcie_caps_reg(cinfo->pcidev) & PCI_EXP_FLAGS_VERS;
	dev_info(cinfo->device, "PCIe version 0x%x\n", pcie_version);
	dev_info(cinfo->device, "board version 0x%x\n", cinfo->board_version);
#endif
}

void print_api_log(u32 api_id, u32 api_result)
{
	switch (api_id) {
	case 0x90000001:
		switch (api_result) {
		case -1:
			pr_err("\tMalloc memory for library node fail.\n");
			break;

		case -2:
			pr_err("\tDlopen file fail.\n");
			break;

		default:
			break;
		}

		break;

	case 0x90000002:
		switch (api_result) {
		case -1:
			pr_err("\tdlsym fail.\n");
			break;

		case -2:
			pr_err("\tFunction not found.\n");
			break;

		case -3:
			pr_err("\tThe library containing the function was not found.\n");
			break;

		default:
			break;
		}

		break;

	case 0x90000003:
		switch (api_result) {
		case 0x12345:
			pr_err("\tFunction did not execute.\n");
			break;

		default:
			break;
		}

		break;

	default:
		pr_err("unknown api id: 0x%x\n", api_id);
		break;
	}
}

void bmdrv_post_api_process(struct bm_device_info *bmdi,
		struct api_fifo_entry api_entry, u32 channel, int core_id)
{
	struct bm_thread_info *ti = api_entry.thd_info;
	struct bm_trace_item *ptitem = NULL;
	u32 next_rp = 0;
	u32 api_id = 0;
	u32 api_size = 0;
	u32 api_duration = 0;
	u32 api_result = 0;

	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[core_id][channel].sw_rp, offsetof(bm_kapi_header_t, api_id) / sizeof(u32));
	api_id = shmem_reg_read_enh(bmdi, next_rp, channel, core_id);
	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[core_id][channel].sw_rp, offsetof(bm_kapi_header_t, api_size) / sizeof(u32));
	api_size = shmem_reg_read_enh(bmdi, next_rp, channel, core_id);
	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[core_id][channel].sw_rp, offsetof(bm_kapi_header_t, duration) / sizeof(u32));
	api_duration = shmem_reg_read_enh(bmdi, next_rp, channel, core_id);
	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[core_id][channel].sw_rp, offsetof(bm_kapi_header_t, result) / sizeof(u32));
	api_result = shmem_reg_read_enh(bmdi, next_rp, channel, core_id);
#ifdef PCIE_MODE_ENABLE_CPU
	if (channel == BM_MSGFIFO_CHANNEL_CPU)
		next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[core_id][channel].sw_rp, (sizeof(bm_kapi_header_t) + sizeof(bm_kapi_opt_header_t)) / sizeof(u32) + api_size);
	else
#endif
		next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[core_id][channel].sw_rp, sizeof(bm_kapi_header_t) / sizeof(u32) + api_size);
	bmdi->api_info[core_id][channel].sw_rp = next_rp;
	//todo, print a log for tmp;
	if (api_result != 0) {
		pr_err("[%s: %d] error: bm-sophon%d, api_id=0x%x, core_id=0x%x, api_result=0x%x\n",
			 __func__, __LINE__, bmdi->dev_index, api_id, core_id, api_result);
		print_api_log(api_id, api_result);
	}
	if (ti) {
		if (core_id == 0) {
			ti->profile.tpu_process_time += api_duration;
			bmdi->profile.tpu_process_time += api_duration;
		}
		else if (core_id == 1) {
			ti->profile.tpu1_process_time += api_duration;
			bmdi->profile.tpu1_process_time += api_duration;
		}
		ti->profile.completed_api_counter++;
		bmdi->profile.completed_api_counter++;

		mutex_lock(&ti->trace_mutex);
		if (ti->trace_enable) {
			ptitem = (struct bm_trace_item *)mempool_alloc(bmdi->trace_info.trace_mempool, GFP_KERNEL);
			ptitem->payload.trace_type = 1;
			ptitem->payload.api_id = api_entry.api_id;
			ptitem->payload.sent_time = api_entry.sent_time_us;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0) || (LINUX_VERSION_CODE == KERNEL_VERSION(4, 18, 0) \
      && CENTOS_KERNEL_FIX >= 408))
			ptitem->payload.end_time = ktime_get_boottime_ns() / 1000;
#else
			ptitem->payload.end_time = ktime_get_boot_ns() / 1000;
#endif
			ptitem->payload.start_time = ptitem->payload.end_time - api_duration * 4166;
			INIT_LIST_HEAD(&ptitem->node);
			list_add_tail(&ptitem->node, &ti->trace_list);
			ti->trace_item_num++;
		}
		mutex_unlock(&ti->trace_mutex);
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static char *bmdrv_class_devnode(struct device *dev, umode_t *mode)
#else
static char *bmdrv_class_devnode(const struct device *dev, umode_t *mode)
#endif
{
	if (!mode || !dev)
		return NULL;
	*mode = 0777;
	return NULL;
}

static void bmdrv_sw_register_init(struct bm_device_info *bmdi)
{
	int channel = 0;
	int core = 0;
	int core_num = bmdi->cinfo.tpu_core_num;

	for (core = 0; core < core_num; core++) {
		for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
			bmdi->api_info[core][channel].bm_api_init = bmdrv_api_init;
			bmdi->api_info[core][channel].bm_api_deinit = bmdrv_api_deinit;
		}
	}
	bmdi->c_attr.bm_card_attr_init = bmdrv_card_attr_init;
	bmdi->memcpy_info.bm_memcpy_init = bmdrv_memcpy_init;
	bmdi->memcpy_info.bm_memcpy_deinit = bmdrv_memcpy_deinit;
	bmdi->trace_info.bm_trace_init = bmdrv_init_trace_pool;
	bmdi->trace_info.bm_trace_deinit = bmdrv_destroy_trace_pool;
	bmdi->gmem_info.bm_gmem_init = bmdrv_gmem_init;
	bmdi->gmem_info.bm_gmem_deinit = bmdrv_gmem_deinit;
}

int bmdrv_software_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	struct chip_info *cinfo = &bmdi->cinfo;
	u32 channel = 0;
	u32 core = 0;
	u32 core_num = cinfo->tpu_core_num;

	INIT_LIST_HEAD(&bmdi->handle_list);
	bmdrv_sw_register_init(bmdi);
	mutex_init(&bmdi->clk_reset_mutex);
	mutex_init(&bmdi->device_mutex);

	if (bmdi->gmem_info.bm_gmem_init &&
		bmdi->gmem_info.bm_gmem_init(bmdi))
		return -EFAULT;
	for (core = 0; core < core_num; core++) {
		for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
			if (bmdi->api_info[core][channel].bm_api_init &&
				bmdi->api_info[core][channel].bm_api_init(bmdi, core, channel))
				return -EFAULT;
		}
	}

	if (bmdi->c_attr.bm_card_attr_init &&
		bmdi->c_attr.bm_card_attr_init(bmdi)) {
		pr_err("bm_card_attr_init failed\n");
		return -EFAULT;
	}
	if (bmdi->memcpy_info.bm_memcpy_init &&
		bmdi->memcpy_info.bm_memcpy_init(bmdi)) {
		pr_err("bm_card_attr_init failed\n");
		return -EFAULT;
	}

	if (bmdi->trace_info.bm_trace_init &&
		bmdi->trace_info.bm_trace_init(bmdi)) {
		pr_err("bm_card_attr_init failed\n");
		return -EFAULT;
	}

	bmdi->parent = cinfo->device;

	bmdrv_print_cardinfo(cinfo);

	bmdi->enable_dyn_freq = 1;

	return ret;
}

void bmdrv_software_deinit(struct bm_device_info *bmdi)
{
#ifdef SOC_MODE
	u32 channel = 0;
	u32 core = 0;
	u32 core_num = bmdi->cinfo.tpu_core_num;

	for (core = 0; core < core_num; core++) {
		for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
			if (bmdi->api_info[core][channel].bm_api_deinit)
				bmdi->api_info[core][channel].bm_api_deinit(bmdi, core, channel);
		}
	}
#endif
	if (bmdi->memcpy_info.bm_memcpy_deinit)
		bmdi->memcpy_info.bm_memcpy_deinit(bmdi);

	if (bmdi->gmem_info.bm_gmem_deinit)
		bmdi->gmem_info.bm_gmem_deinit(bmdi);

	if (bmdi->trace_info.bm_trace_deinit)
		bmdi->trace_info.bm_trace_deinit(bmdi);
}

struct class bmdev_class = {
	.name		= BM_CLASS_NAME,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
	.owner		= THIS_MODULE,
#endif
	.devnode = bmdrv_class_devnode,
};

int bmdrv_class_create(void)
{
	int ret;

	ret = class_register(&bmdev_class);
	if (ret < 0) {
		pr_err("bmdrv: create class error\n");
		return ret;
	}

	ret = alloc_chrdev_region(&bm_devno_base, 0, MAX_CARD_NUMBER, BM_CDEV_NAME);
	if (ret < 0) {
		pr_err("bmdrv: register chrdev error\n");
		return ret;
	}
	ret = alloc_chrdev_region(&bm_ctl_devno_base, 0, 1, BMDEV_CTL_NAME);
	if (ret < 0) {
		pr_err("bmdrv: register ctl chrdev error\n");
		return ret;
	}
	return 0;
}

struct class *bmdrv_class_get(void)
{
	return &bmdev_class;
}

int bmdrv_class_destroy(void)
{
	unregister_chrdev_region(bm_devno_base, MAX_CARD_NUMBER);
	unregister_chrdev_region(bm_ctl_devno_base, 1);
	class_unregister(&bmdev_class);
	return 0;
}
