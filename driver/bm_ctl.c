#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/sizes.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm_pcie.h"
#include "bm1684_clkrst.h"
#include "bm_ctl.h"
#include "bm_drv.h"
#include "bm_wdt.h"
#include "bm1684_card.h"

#ifndef SOC_MODE
#include "bm_card.h"
#endif

struct bm_ctrl_info *bmci;
int dev_count;

int bmdrv_init_bmci(struct chip_info *cinfo)
{
	int rc;

	bmci = kzalloc(sizeof(*bmci), GFP_KERNEL);
	if (!bmci)
		return -ENOMEM;
	sprintf(bmci->dev_ctl_name, "%s", BMDEV_CTL_NAME);
	INIT_LIST_HEAD(&bmci->bm_dev_list);
	bmci->dev_count = 0;
	rc = bmdev_ctl_register_device(bmci);
	return rc;
}

int bmdrv_remove_bmci(void)
{
	bmdev_ctl_unregister_device(bmci);
	kfree(bmci);
	return 0;
}

int bmdrv_ctrl_add_dev(struct bm_ctrl_info *bmci, struct bm_device_info *bmdi)
{
	struct bm_dev_list *bmdev_list;

	dev_count++;
	/* record a reference in the bmdev list in bm_ctl */
	bmci->dev_count = dev_count;
	bmdev_list = kmalloc(sizeof(struct bm_dev_list), GFP_KERNEL);
	if (!bmdev_list)
		return -ENOMEM;
	bmdev_list->bmdi = bmdi;
	list_add(&bmdev_list->list, &bmci->bm_dev_list);
	return 0;
}

int bmdrv_ctrl_del_dev(struct bm_ctrl_info *bmci, struct bm_device_info *bmdi)
{
	struct bm_dev_list *bmdev_list, *tmp;

	dev_count--;
	bmci->dev_count = dev_count;
	list_for_each_entry_safe(bmdev_list, tmp, &bmci->bm_dev_list, list) {
		if (bmdev_list->bmdi == bmdi) {
			list_del(&bmdev_list->list);
			kfree(bmdev_list);
		}
	}
	return 0;
}

static int bmctl_get_bus_id(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	return bmdi->misc_info.domain_bdf;
#else
	return 0;
#endif
}

struct bm_device_info *bmctl_get_bmdi(struct bm_ctrl_info *bmci, int dev_id)
{
	struct bm_dev_list *pos, *tmp;
	struct bm_device_info *bmdi;

	if (dev_id >= bmci->dev_count) {
		pr_err("bmdrv: invalid device id %d!\n", dev_id);
		return NULL;
	}
	list_for_each_entry_safe(pos, tmp, &bmci->bm_dev_list, list) {
		bmdi = pos->bmdi;
		if (bmdi->dev_index == dev_id)
			return bmdi;
	}
	pr_err("bmdrv: bmdi not found!\n");
	return NULL;
}

struct bm_device_info *bmctl_get_card_bmdi(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	int dev_id = 0;

	if ((bmdi->misc_info.domain_bdf & 0x7) == 0x0)
		return bmdi;

	if ((bmdi->misc_info.domain_bdf & 0x7) == 0x1
		|| (bmdi->misc_info.domain_bdf & 0x7) == 0x2) {
		if ((bmdi->misc_info.domain_bdf & 0x7) == 0x1)
			dev_id = bmdi->dev_index - 1;

		if ((bmdi->misc_info.domain_bdf & 0x7) == 0x2)
			dev_id = bmdi->dev_index - 2;

		if (dev_id < 0 || dev_id >= 0xff) {
			pr_err("bmctl_get_card_bmdi fail, dev_id = 0x%x\n", dev_id);
			return NULL;
		}

		return bmctl_get_bmdi(bmci, dev_id);
	}

	return NULL;
#else
	return bmdi;
#endif
}

static int bmctl_get_smi_attr(struct bm_ctrl_info *bmci, struct bm_smi_attr *pattr)
{
	struct bm_device_info *bmdi;
	struct chip_info *cinfo;
	struct bm_chip_attr *c_attr;
	int i;
#ifndef SOC_MODE
	struct bm_card *b_card;
	vpu_statistic_info_t *vpu_usage_info;
	jpu_statistic_info_t *jpu_usage_info;
	vpp_statistic_info_t *vpp_usage_info;
#endif
#define P_SHOW	0
	bmdi = bmctl_get_bmdi(bmci, pattr->dev_id);
	if (!bmdi)
		return -1;


#ifndef SOC_MODE
	b_card = bmdi->bmcd;
	if (!b_card)
		return -1;
#endif

	cinfo = &bmdi->cinfo;
	c_attr = &bmdi->c_attr;

	pattr->chip_mode = bmdi->misc_info.pcie_soc_mode;
	if (pattr->chip_mode == 0)
		pattr->domain_bdf = bmctl_get_bus_id(bmdi);
	else
		pattr->domain_bdf = ATTR_NOTSUPPORTED_VALUE;
	pattr->chip_id = bmdi->cinfo.chip_id;
	pattr->status = bmdi->status;

	pattr->mem_total = (int)(bmdrv_gmem_total_size(bmdi)/1024/1024);
	pattr->mem_used = pattr->mem_total - (int)(bmdrv_gmem_avail_size(bmdi)/1024/1024);
	bmdrv_heap_mem_used(bmdi, &pattr->stat);

	pattr->tpu_util = c_attr->bm_get_npu_util(bmdi);

	if (c_attr->bm_get_chip_temp != NULL)
		pattr->chip_temp =c_attr->chip_temp;
	else
		pattr->chip_temp = ATTR_NOTSUPPORTED_VALUE;
	if(P_SHOW)pr_err("pattr->chip_temp = 0x%x\n", pattr->chip_temp);
	if (c_attr->bm_get_board_temp != NULL)
		pattr->board_temp = c_attr->board_temp;
	else
		pattr->board_temp = ATTR_NOTSUPPORTED_VALUE;
	if(P_SHOW)pr_err("pattr->board_temp = 0x%x\n", pattr->board_temp);
	if (c_attr->bm_get_tpu_power != NULL) {
#ifndef SOC_MODE
		pattr->vdd_tpu_volt = c_attr->vdd_tpu_volt;
		pattr->vdd_tpu_curr = c_attr->vdd_tpu_curr;
		if(P_SHOW)pr_err("pattr->vdd_tpu_volt = %d\n", pattr->vdd_tpu_volt);//have
		if(P_SHOW)pr_err("pattr->vdd_tpu_curr = %d\n", pattr->vdd_tpu_curr);//have
#endif
		pattr->tpu_power = c_attr->tpu_power;
		if(P_SHOW)pr_err("pattr->tpu_power = %d\n", pattr->tpu_power);
	} else {
		pattr->tpu_power = ATTR_NOTSUPPORTED_VALUE;
		pattr->vdd_tpu_volt = ATTR_NOTSUPPORTED_VALUE;
		pattr->vdd_tpu_curr = ATTR_NOTSUPPORTED_VALUE;
	}
	if (c_attr->bm_get_board_power != NULL) {
#ifndef SOC_MODE
		pattr->board_power = b_card->board_power;
		pattr->atx12v_curr = b_card->atx12v_curr;
#endif
	} else {
		pattr->board_power = ATTR_NOTSUPPORTED_VALUE;
		pattr->atx12v_curr = ATTR_NOTSUPPORTED_VALUE;
	}
	if(P_SHOW)pr_err("pattr->board_power = 0x%x\n", pattr->board_power);
	if(P_SHOW)pr_err("pattr->atx12v_curr = 0x%x\n", pattr->atx12v_curr);
#ifndef SOC_MODE
	memcpy(pattr->sn, b_card->sn, 17);
#else
	memcpy(pattr->sn, "    N/A         ", 17);
#endif
	if(P_SHOW)pr_err("pattr->sn = %s\n", pattr->sn == NULL ? "no val": pattr->sn);
#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id != 0x1682) {
		bm1684_get_board_type_by_id(bmdi, pattr->board_type,
			BM1684_BOARD_TYPE(bmdi));
	}
#else
	strncpy(pattr->board_type, "SOC", 3);
#endif
#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id != 0x1682) {
		if (bmdi->bmcd != NULL) {
			pattr->card_index = bmdi->bmcd->card_index;
			pattr->chip_index_of_card = bmdi->dev_index - bmdi->bmcd->dev_start_index;
			if (pattr->chip_index_of_card < 0 || pattr->chip_index_of_card >  bmdi->bmcd->chip_num)
				pattr->chip_index_of_card = 0;
		}
	}
#else
	pattr->card_index = 0x0;
	pattr->chip_index_of_card = 0x0;
#endif

#ifndef SOC_MODE
	if (c_attr->fan_control)
		pattr->fan_speed = b_card->fan_speed;
	else
		pattr->fan_speed = ATTR_NOTSUPPORTED_VALUE;
#else
	pattr->fan_speed = ATTR_NOTSUPPORTED_VALUE;
#endif
	if(P_SHOW)pr_err("pattr->fan_speed = 0x%x\n", pattr->fan_speed);
	switch (pattr->chip_id) {
	case 0x1682:
		pattr->tpu_min_clock = 687;
		pattr->tpu_max_clock = 687;
		pattr->tpu_current_clock = 687;
		pattr->board_max_power = 75;
		break;
	case 0x1684:
		if (pattr->chip_mode == 0) {
			pattr->tpu_min_clock = bmdi->boot_info.tpu_min_clk;
			pattr->tpu_max_clock = bmdi->boot_info.tpu_max_clk;
		} else {
			pattr->tpu_min_clock = 75;
			pattr->tpu_max_clock = 550;
		}
		pattr->tpu_current_clock = c_attr->tpu_current_clock;
		if (pattr->tpu_current_clock < pattr->tpu_min_clock
                           || (pattr->tpu_current_clock > pattr->tpu_max_clock)) {
			pattr->tpu_current_clock = (int)0xFFFFFC00;
		}
		break;
	case 0x1686:
		if (pattr->chip_mode == 0) {
			pattr->tpu_min_clock = bmdi->boot_info.tpu_min_clk;
			pattr->tpu_max_clock = bmdi->boot_info.tpu_max_clk;
		} else {
			pattr->tpu_min_clock = 75;
			pattr->tpu_max_clock = 1000;
		}
		pattr->tpu_current_clock = c_attr->tpu_current_clock;
		if (pattr->tpu_current_clock < pattr->tpu_min_clock
                           || (pattr->tpu_current_clock > pattr->tpu_max_clock)) {
			pattr->tpu_current_clock = (int)0xFFFFFC00;
		}
		if(P_SHOW)pr_err("pattr->tpu_current_clock = 0x%x\n", pattr->tpu_current_clock);
		break;
	default:
		break;
	}
	if (pattr->board_power != ATTR_NOTSUPPORTED_VALUE)
		pattr->board_max_power = bmdi->boot_info.max_board_power;
	else
		pattr->board_max_power = ATTR_NOTSUPPORTED_VALUE;
	if(P_SHOW)pr_err("pattr->board_max_power = 0x%x\n", pattr->board_max_power);
#ifdef SOC_MODE
		pattr->ecc_enable = ATTR_NOTSUPPORTED_VALUE;
		pattr->ecc_correct_num = ATTR_NOTSUPPORTED_VALUE;
#else
	pattr->ecc_enable = bmdi->misc_info.ddr_ecc_enable;
	if(P_SHOW)pr_err("pattr->ecc_enable = 0x%x\n", pattr->ecc_enable);
	if (pattr->ecc_enable == 0)
		pattr->ecc_correct_num = ATTR_NOTSUPPORTED_VALUE;
	else
		pattr->ecc_correct_num = bmdi->cinfo.ddr_ecc_correctN;
#endif
	if(P_SHOW)pr_err("pattr->ecc_correct_num = 0x%x\n", pattr->ecc_correct_num);
#ifndef SOC_MODE
	vpu_usage_info = &bmdi->vpudrvctx.s_vpu_usage_info;
	jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;
	vpp_usage_info = &bmdi->vppdrvctx.s_vpp_usage_info;
	if(P_SHOW)pr_err("vpu_usage_info = 0x%x\n", pattr->ecc_correct_num);
	if(P_SHOW)pr_err("vpu_usage_info = 0x%x\n", pattr->ecc_correct_num);
	if (bmdi->cinfo.chip_id == 0x1682){
		//AVC decoder
		pattr->vpu_mem_total_size[0] = vpu_usage_info->vpu_mem_total_size[0];
		pattr->vpu_mem_used_size[0] = vpu_usage_info->vpu_mem_used_size[0];
		pattr->vpu_mem_free_size[0] = vpu_usage_info->vpu_mem_free_size[0];
		//HEVC decoder
		pattr->vpu_mem_total_size[1] = vpu_usage_info->vpu_mem_total_size[1];
    	pattr->vpu_mem_used_size[1] = vpu_usage_info->vpu_mem_used_size[1];
        pattr->vpu_mem_free_size[1] = vpu_usage_info->vpu_mem_free_size[1];
	} else if (bmdi->cinfo.chip_id == 0x1684) {
		pattr->vpu_mem_total_size[0] = vpu_usage_info->vpu_mem_total_size[0];
        pattr->vpu_mem_used_size[0] = vpu_usage_info->vpu_mem_used_size[0];
        pattr->vpu_mem_free_size[0] = vpu_usage_info->vpu_mem_free_size[0];
		pattr->mem_total += pattr->vpu_mem_total_size[0]/1000/1000;
		pattr->mem_used += pattr->vpu_mem_used_size[0]/1000/1000;
	} else if (bmdi->cinfo.chip_id == 0x1686) {
		pattr->vpu_mem_total_size[0] = vpu_usage_info->vpu_mem_total_size[0];
        pattr->vpu_mem_used_size[0] = vpu_usage_info->vpu_mem_used_size[0];
        pattr->vpu_mem_free_size[0] = vpu_usage_info->vpu_mem_free_size[0];
		pattr->mem_total += pattr->vpu_mem_total_size[0]/1000/1000;
		pattr->mem_used += pattr->vpu_mem_used_size[0]/1000/1000;
		if(P_SHOW)pr_err("pattr->vpu_mem_total_size[0] = 0x%llx\n", pattr->vpu_mem_total_size[0]);
		if(P_SHOW)pr_err("pattr->vpu_mem_used_size[0] = 0x%llx\n", pattr->vpu_mem_used_size[0]);
		if(P_SHOW)pr_err("pattr->vpu_mem_free_size[0] = 0x%llx\n", pattr->vpu_mem_free_size[0]);
		if(P_SHOW)pr_err("pattr->mem_total = %d\n", pattr->mem_total);
		if(P_SHOW)pr_err("pattr->mem_used = %d\n", pattr->mem_used);
	}

	for(i = 0; i < VPP_CORE_MAX; i++) {
		pattr->vpp_instant_usage[i] = vpp_usage_info->vpp_core_usage[i];
		if (vpp_usage_info->vpp_total_time_in_ms[i] == 0) {
			pattr->vpp_chronic_usage[i] = 0;
		} else {
			pattr->vpp_chronic_usage[i] = vpp_usage_info->vpp_working_time_in_ms[i] * 100 /
				vpp_usage_info->vpp_total_time_in_ms[i];
		}
	}
#endif

	if (bmdi->cinfo.chip_id == 0x1684){
	    for (i = 0; i < MAX_NUM_VPU_CORE; i++) {
#ifndef SOC_MODE
		    pattr->vpu_instant_usage[i] = vpu_usage_info->vpu_instant_usage[i];
#else
		    pattr->vpu_instant_usage[i] = ATTR_NOTSUPPORTED_VALUE;
#endif
	    }
	    for (i = 0; i < MAX_NUM_JPU_CORE; i++) {
#ifndef SOC_MODE
	        pattr->jpu_core_usage[i] = jpu_usage_info->jpu_core_usage[i];
#else
	        pattr->jpu_core_usage[i] = ATTR_NOTSUPPORTED_VALUE;
#endif
	    }
    }else if (bmdi->cinfo.chip_id == 0x1686) {
	    for (i = 0; i < MAX_NUM_VPU_CORE_BM1686; i++) {
#ifndef SOC_MODE
		    pattr->vpu_instant_usage[i] = vpu_usage_info->vpu_instant_usage[i];
#else
		    pattr->vpu_instant_usage[i] = ATTR_NOTSUPPORTED_VALUE;
#endif
			if(P_SHOW)pr_err("pattr->vpu_instant_usage[i] = 0x%x\n", pattr->vpu_instant_usage[i]);
	    }
	    for (i = 0; i < MAX_NUM_JPU_CORE_BM1686; i++) {
#ifndef SOC_MODE
	        pattr->jpu_core_usage[i] = jpu_usage_info->jpu_core_usage[i];
#else
	        pattr->jpu_core_usage[i] = ATTR_NOTSUPPORTED_VALUE;
#endif
			if(P_SHOW)pr_err("pattr->jpu_core_usage[i] = 0x%x\n", pattr->jpu_core_usage[i]);
	    }
	}
	return 0;
}

int bmctl_ioctl_get_attr(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int ret = 0;
	struct bm_smi_attr smi_attr;

	ret = copy_from_user(&smi_attr, (struct bm_smi_attr __user *)arg,
			sizeof(struct bm_smi_attr));
	if (ret) {
		pr_err("bmdev_ctl_ioctl: copy_from_user fail\n");
		return ret;
	}

	ret = bmctl_get_smi_attr(bmci, &smi_attr);
	if (ret)
		return ret;

	ret = copy_to_user((struct bm_smi_attr __user *)arg, &smi_attr,
			sizeof(struct bm_smi_attr));
	if (ret) {
		pr_err("BMCTL_GET_SMI_ATTR: copy_to_user fail\n");
		return ret;
	}
	return 0;
}

static int bmctl_get_smi_proc_gmem(struct bm_ctrl_info *bmci,
		struct bm_smi_proc_gmem *smi_proc_gmem)
{
	struct bm_device_info *bmdi;
	struct bm_handle_info *h_info;
	int proc_cnt = 0;

	bmdi = bmctl_get_bmdi(bmci, smi_proc_gmem->dev_id);
	if (!bmdi)
		return -1;

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_for_each_entry(h_info, &bmdi->handle_list, list) {
		smi_proc_gmem->pid[proc_cnt] = h_info->open_pid;
		smi_proc_gmem->gmem_used[proc_cnt] = h_info->gmem_used / 1024 / 1024;
		proc_cnt++;
		if (proc_cnt == 128)
			break;
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	smi_proc_gmem->proc_cnt = proc_cnt;
	return 0;
}

int bmctl_ioctl_get_proc_gmem(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int ret = 0;
	struct bm_smi_proc_gmem *smi_proc_gmem = kmalloc(sizeof(struct bm_smi_proc_gmem), GFP_KERNEL);

	if (!smi_proc_gmem)
		return -ENOMEM;

	ret = copy_from_user(smi_proc_gmem, (struct bm_smi_proc_gmem __user *)arg,
			sizeof(struct bm_smi_proc_gmem));
	if (ret) {
		pr_err("bmdev_ctl_ioctl: copy_from_user fail\n");
		kfree(smi_proc_gmem);
		return ret;
	}

	ret = bmctl_get_smi_proc_gmem(bmci, smi_proc_gmem);
	if (ret) {
		kfree(smi_proc_gmem);
		return ret;
	}

	ret = copy_to_user((struct bm_smi_proc_gmem__user *)arg, smi_proc_gmem,
			sizeof(struct bm_smi_proc_gmem));
	if (ret) {
		pr_err("BMCTL_GET_SMI_PROC_GMEM: copy_to_user fail\n");
		kfree(smi_proc_gmem);
		return ret;
	}
	kfree(smi_proc_gmem);
	return 0;
}

int bmctl_ioctl_set_led(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int dev_id = arg & 0xff;
	int led_op = (arg >> 8) & 0xff;
	struct bm_device_info *bmdi = bmctl_get_bmdi(bmci, dev_id);
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	if (!bmdi)
		return -1;
	if ((bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686) &&
			bmdi->cinfo.platform == DEVICE &&
			c_attr->bm_set_led_status) {
		mutex_lock(&c_attr->attr_mutex);
		switch (led_op) {
		case 0:
			c_attr->bm_set_led_status(bmdi, LED_ON);
			c_attr->led_status = LED_ON;
			break;
		case 1:
			c_attr->bm_set_led_status(bmdi, LED_OFF);
			c_attr->led_status = LED_OFF;
			break;
		case 2:
			c_attr->bm_set_led_status(bmdi, LED_BLINK_ONE_TIMES_PER_2S);
			c_attr->led_status = LED_BLINK_ONE_TIMES_PER_2S;
			break;
		default:
			break;
		}
		mutex_unlock(&c_attr->attr_mutex);
	}
	return 0;
}

int bmctl_ioctl_set_ecc(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int dev_id = arg & 0xff;
	int ecc_op = (arg >> 8) & 0xff;
	struct bm_device_info *bmdi = bmctl_get_bmdi(bmci, dev_id);

	if (!bmdi)
		return -1;

	if (bmdi->misc_info.chipid == 0x1684)
		set_ecc(bmdi, ecc_op);
	else if (bmdi->misc_info.chipid == 0x1686)
		set_ecc(bmdi, ecc_op);

	return 0;
}

#ifndef SOC_MODE
int bmctl_ioctl_recovery(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int dev_id = arg & 0xff;
	int func_num = 0x0;
	int i = 0x0;
	struct pci_bus *root_bus;
	struct bm_device_info *bmdi = bmctl_get_bmdi(bmci, dev_id);
	struct bm_device_info *bmdi_1 =  NULL;
	struct bm_device_info *bmdi_2 =  NULL;

	if (!bmdi)
		return -ENODEV;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
		func_num = bmdi->misc_info.domain_bdf&0x7;
		if (func_num == 0) {
			bmdi = bmctl_get_bmdi(bmci, dev_id);
			bmdi_1 =  bmctl_get_bmdi(bmci, dev_id + 1);
			bmdi_2 =  bmctl_get_bmdi(bmci, dev_id + 2);

			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -ENODEV;
		}

		if (func_num == 1) {
			bmdi = bmctl_get_bmdi(bmci, dev_id - 1);
			bmdi_1 =  bmctl_get_bmdi(bmci, dev_id);
			bmdi_2 =  bmctl_get_bmdi(bmci, dev_id + 1);

			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -ENODEV;
		}

		if (func_num == 2) {
			bmdi = bmctl_get_bmdi(bmci, dev_id - 2);
			bmdi_1 =  bmctl_get_bmdi(bmci, dev_id - 1);
			bmdi_2 =  bmctl_get_bmdi(bmci, dev_id);
			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -ENODEV;
		}

		if (BM1684_HW_VERSION(bmdi) == 0x0) {
			pr_info("not support for sc5+ v1_0\n");
			return  -EPERM;
		}

		if (bmdi->dev_refcount != 0 || bmdi_1->dev_refcount != 0 || bmdi_2->dev_refcount !=0) {
			pr_info("dev is busy!!, sophon%d_ref0:%lld, sophon%d_ref:%lld, sophon%d_ref:%lld\n",
			bmdi->dev_index, bmdi->dev_refcount, bmdi_1->dev_index, bmdi_1->dev_refcount,
			bmdi_2->dev_index, bmdi_2->dev_refcount);
			return -EBUSY;
		}

		if (atomic_read(&bmdi->dev_recovery) == 0x0) {
			atomic_set(&bmdi->dev_recovery, 1);
			atomic_set(&bmdi_1->dev_recovery, 1);
			atomic_set(&bmdi_2->dev_recovery, 1);
		} else {
			pr_info("dev%d is recoverying\n", bmdi->dev_index);
			return -EBUSY;
		}

		if (bmdi->misc_info.chipid == 0x1684) {
			pr_info("to reboot 1684, devid is %d\n", dev_id);
			bmdrv_wdt_start(bmdi);
		}

		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi_2->cinfo.device));
		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi_1->cinfo.device));
		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi->cinfo.device));

		msleep(6500);
	} else {
		if (bmdi->dev_refcount != 0) {
			pr_info("dev is busy!!, sophon%d_ref:%lld\n",
			bmdi->dev_index, bmdi->dev_refcount);
			return -EBUSY;
		}

		if (atomic_read(&bmdi->dev_recovery) == 0x0) {
			atomic_set(&bmdi->dev_recovery, 1);
		} else {
			pr_info("dev%d is recoverying\n", bmdi->dev_index);
			return -EBUSY;
		}

		if (bmdi->misc_info.chipid == 0x1684 || bmdi->misc_info.chipid == 0x1686) {
			if (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO) {
				pr_info("to reboot chip, devid is %d\n", dev_id);
				bmdrv_wdt_start(bmdi);
			}
		}
		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi->cinfo.device));

		msleep(6500);
	}
	for (i = 0; i < 2; i++) {
		pci_lock_rescan_remove();
		list_for_each_entry(root_bus, &pci_root_buses, node) {
			pci_rescan_bus(root_bus);
		}
		pci_unlock_rescan_remove();

		bmdi = bmctl_get_bmdi(bmci, dev_id);

		if (bmdi == NULL) {
			pr_info("retry recovery dev_id = %d\n", dev_id);
			msleep(3500);
		} else {
			return 0;
		}

	}

	pr_info("recovery fail, dev_id = %d\n", dev_id);
	return -ENODEV;
}
#endif
