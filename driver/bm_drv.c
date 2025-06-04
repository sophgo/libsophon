#include "bm_common.h"
#include "bm_drv.h"



/* be carefull with global variables, keep multi-card support in mind */
dev_t bm_devno_base;
dev_t bm_ctl_devno_base;

static void bmdrv_print_cardinfo(struct chip_info *cinfo)
{
	int i = 0;

	for ( ; i < REG_COUNT; ++i) {
		PR_TRACE("bar0 0x%llx size 0x%llx, vaddr = 0x%p\n",
					 cinfo->bar_info.bar_start[i], cinfo->bar_info.bar_len[i],
					 cinfo->bar_info.bar_vaddr[i]);
	}
}

static char *bmdrv_class_devnode(struct device *dev, umode_t *mode)
{
	if (!mode || !dev)
		return NULL;
	*mode = 0777;
	return NULL;
}

static void bmdrv_sw_register_init(struct bm_device_info *bmdi)
{
	bmdi->c_attr.bm_card_attr_init = bmdrv_card_attr_init;

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

	for (core = 0; core < core_num; core++)
	{
		for (channel = 0; channel < 2; channel++)
		{
			if (bmdi->api_info[core][channel].bm_api_init &&
					bmdi->api_info[core][channel].bm_api_init(bmdi, core, channel))
				return -EFAULT;
		}
	}

	if (bmdi->c_attr.bm_card_attr_init &&
			bmdi->c_attr.bm_card_attr_init(bmdi))
		return -EFAULT;


	bmdi->parent = cinfo->device;

	bmdrv_print_cardinfo(cinfo);

	bmdi->enable_dyn_freq = 1;

	return ret;
}

void bmdrv_software_deinit(struct bm_device_info *bmdi)
{
	u32 channel = 0;
	u32 core = 0;
	u32 core_num = bmdi->cinfo.tpu_core_num;

	for (core = 0; core < core_num; core++)
	{
		for (channel = 0; channel < 2; channel++)
		{
			if (bmdi->api_info[core][channel].bm_api_deinit)
				bmdi->api_info[core][channel].bm_api_deinit(bmdi, core, channel);
		}
	}

	if (bmdi->gmem_info.bm_gmem_deinit)
		bmdi->gmem_info.bm_gmem_deinit(bmdi);
}

struct class bmdev_class = {
		.name = BM_CLASS_NAME,
		.owner = THIS_MODULE,
		.devnode = bmdrv_class_devnode,
};

int bmdrv_class_create(void)
{
	int ret;

	ret = class_register(&bmdev_class);
	if (ret < 0)
	{
		pr_err("bmdrv: create class error\n");
		class_unregister(&bmdev_class);
		return ret;
	}

	ret = alloc_chrdev_region(&bm_devno_base, 0, MAX_CARD_NUMBER, BM_CDEV_NAME);
	if (ret < 0)
	{
		unregister_chrdev_region(bm_devno_base, MAX_CARD_NUMBER);
		pr_err("bmdrv: register chrdev error\n");
		return ret;
	}
	ret = alloc_chrdev_region(&bm_ctl_devno_base, 0, 1, BMDEV_CTL_NAME);
	if (ret < 0)
	{
		unregister_chrdev_region(bm_devno_base, MAX_CARD_NUMBER);
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
