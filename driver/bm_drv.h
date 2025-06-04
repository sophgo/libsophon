#ifndef _BM_DRV_H_
#define _BM_DRV_H_
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include "bm_ctl.h"
#include "bm_api.h"

struct bm_device_info;
int bmdrv_software_init(struct bm_device_info *bmdi);
void bmdrv_software_deinit(struct bm_device_info *bmdi);
int bmdrv_class_create(void);
int bmdrv_class_destroy(void);
struct class *bmdrv_class_get(void);
int bmdev_register_device(struct bm_device_info *bmdi);
int bmdev_unregister_device(struct bm_device_info *bmdi);
int bmdev_ctl_register_device(struct bm_ctrl_info *bmci);
int bmdev_ctl_unregister_device(struct bm_ctrl_info *bmci);
#endif
