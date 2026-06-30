#ifndef _BM_CTL_H_
#define _BM_CTL_H_
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/types.h>

#define ATTR_NOTSUPPORTED_VALUE		(int)(0xFFFFFC01)

struct bm_dev_list {
	struct list_head list;
	struct bm_device_info *bmdi;
};

struct bm_ctrl_info {
	struct list_head bm_dev_list;
	u32 dev_count;
	char dev_ctl_name[10];
	struct cdev cdev;
	struct device *dev;
	dev_t devno;
};

struct chip_info;
int bmctl_ioctl_get_attr(struct bm_ctrl_info *bmci, unsigned long arg);
int bmctl_ioctl_get_proc_gmem(struct bm_ctrl_info *bmci, unsigned long arg);
int bmdrv_init_bmci(struct chip_info *cinfo);
int bmdrv_remove_bmci(void);
int bmdrv_ctrl_add_dev(struct bm_ctrl_info *bmci, struct bm_device_info *bmdi);
int bmdrv_ctrl_del_dev(struct bm_ctrl_info *bmci, struct bm_device_info *bmdi);
struct bm_device_info *bmctl_get_bmdi(struct bm_ctrl_info *bmci, int dev_id);

#endif
