#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include "bm_common.h"
#include "bm_attr.h"
#include "bm_ctl.h"

#define FREQ0DATA 0x024
#define FREQ1DATA 0x02c
/*
 * Cat value of the npu usage
 * Test: $cat /sys/class/bm-sophon/bm-sophon0/device/npu_usage
 */
static ssize_t npu_usage_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
	struct bm_chip_attr *cattr = NULL;
	int usage, usage1 = 0;
	int usage_all, usage_all1 = 0;

	cattr = &bmdi->c_attr;

	if (atomic_read(&cattr->timer_on) == 0)
		return sprintf(buf, "Please, set [Usage enable] to 1\n");

	usage = (int)atomic_read(&cattr->npu_utilization);
	usage_all = div_u64(cattr->npu_busy_time_sum_ms * 100, cattr->npu_start_probe_time);


	if (bmdi->cinfo.chip_id == CHIP_ID)
	{
		char *name;

		name = base_get_chip_id(bmdi);
		usage1 = (int)atomic_read(&cattr->npu_utilization1);
		usage_all1 = div_u64(cattr->npu_busy_time_sum_ms1 * 100, cattr->npu_start_probe_time1);

		return sprintf(buf, "usage:%d avusage:%d\n", usage, usage_all);
	}

	return sprintf(buf, "usage:%d avusage:%d\n", usage, usage_all);
}
static DEVICE_ATTR_RO(npu_usage);

/*
 * Check the validity of the parameters(Only for method of store***)
 */
static int check_interval_store(const char *buf)
{
	int ret = -1;

	int tmp = simple_strtoul(buf, NULL, 0);

	if ((tmp >= 200) && (tmp <= 2000))
		ret = 0;

	return ret;
}

/*
 * Cat value of the npu usage interval
 * Test: $cat /sys/class/bm-sophon/bm-sophon0/device/npu_usage_interval
 */
static ssize_t show_usage_interval(struct device *d, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	return sprintf(buf, "\"interval\": %d\n", cattr->npu_timer_interval);
}

/*
 * Echo value of the usage interval
 * Test: $sudo bash -c "echo 2000 > /sys/class/bm-sophon/bm-sophon0/device/npu_usage_interval"
 */
static ssize_t store_usage_interval(struct device *d,
																		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	/* Check the validity of the parameters */
	if (-1 == check_interval_store(buf))
	{
		pr_info("Parameter error! Parameter: 200 ~ 2000\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &cattr->npu_timer_interval);
	pr_info("usage interval: %d\n", cattr->npu_timer_interval);

	return strnlen(buf, count);
}

static DEVICE_ATTR(npu_usage_interval, 0664, show_usage_interval, store_usage_interval);

static ssize_t show_usage_enable(struct device *d, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);

	return sprintf(buf, "\"enable\": %d\n", atomic_read(&bmdi->c_attr.timer_on));
}

static int check_enable_store(const char *buf)
{
	int ret = -1;

	int tmp = simple_strtoul(buf, NULL, 0);

	if ((0 == tmp) || (1 == tmp))
		ret = 0;

	return ret;
}
static ssize_t store_usage_enable(struct device *d, struct device_attribute *attr, const char *buf, size_t count)
{
	int enable = 0;

	struct platform_device *pdev = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	/* Check the validity of the parameters */
	if (-1 == check_enable_store(buf))
	{
		pr_info("Parameter error! Parameter: 0 or 1\n");
		return -1;
	}

	sscanf(buf, "%d", &enable);
	if ((enable == 1) && (atomic_read(&cattr->timer_on) == 0))
	{
		atomic_set(&cattr->timer_on, 1);
	}
	else if (enable == 0)
	{
		atomic_set(&cattr->timer_on, 0);
	}
	pr_info("Usage enable: %d\n", enable);

	return strnlen(buf, count);
}

static DEVICE_ATTR(npu_usage_enable, 0664, show_usage_enable, store_usage_enable);
static struct attribute *bm_npu_sysfs_entries[] = {
		&dev_attr_npu_usage.attr,
		&dev_attr_npu_usage_interval.attr,
		&dev_attr_npu_usage_enable.attr,
		NULL,
};

static struct attribute_group bm_npu_attribute_group = {
		.name = NULL,
		.attrs = bm_npu_sysfs_entries,
};


int bmdrv_card_attr_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	int i = 0;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;


	c_attr->fan_speed = 100;
	c_attr->fan_rev_read = 0;
	c_attr->npu_cnt = 0;
	c_attr->npu_busy_cnt = 0;
	atomic_set(&c_attr->npu_utilization, 0);
	c_attr->npu_timer_interval = 500;
	c_attr->npu_busy_time_sum_ms = 0ULL;
	c_attr->npu_start_probe_time = 0ULL;
	c_attr->npu_status_idx = 0;
	c_attr->tpu_current_clock = 500;
	for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
	{
		c_attr->npu_status[i] = 0;
		c_attr->npu_status1[i] = 0;
	}
	atomic_set(&c_attr->timer_on, 0);
	mutex_init(&c_attr->attr_mutex);

	switch (bmdi->cinfo.chip_id) {
	case CHIP_ID:

		c_attr->bm_get_tpu_power = NULL;
		c_attr->bm_get_vddc_power = NULL;
		c_attr->bm_get_vddphy_power = NULL;
		c_attr->bm_get_board_power = NULL;
		c_attr->fan_control = false;
		c_attr->bm_get_chip_temp = NULL;
		c_attr->bm_get_board_temp = NULL;
		break;
	default:
		return -EINVAL;
	}


	return ret;
}

int reset_fan_speed(struct bm_device_info *bmdi)
{
	return 0;
}

int bmdrv_enable_attr(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	struct chip_info *cinfo = &bmdi->cinfo;
	int rc;

	if (c_attr->fan_control)
		reset_fan_speed(bmdi);

	rc = sysfs_create_group(&cinfo->device->kobj, &bm_npu_attribute_group);
	if (rc)
	{
		pr_err("create sysfs node failed\n");
		rc = -EINVAL;
		return rc;
	}
	atomic_set(&c_attr->timer_on, 1);
	return 0;
}

int bmdrv_disable_attr(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	struct chip_info *cinfo = &bmdi->cinfo;

	atomic_set(&c_attr->timer_on, 0);
	if (c_attr->fan_control)
		reset_fan_speed(bmdi);
	sysfs_remove_group(&cinfo->device->kobj, &bm_npu_attribute_group);
	return 0;
}

/* the function set_fan_speed sets the fan running speed
 *parameter: u8 spd_level is an unsigned integer ranging from
 *	0 - 100; 0 means min speed and 100 means full speed
 */
int set_fan_speed(struct bm_device_info *bmdi, u16 spd_level)
{
	return 0;
}


int bm_get_name(struct bm_device_info *bmdi, unsigned long arg)
{

	int ret = 0;

	ret = copy_to_user((unsigned char __user *)arg, "soc", sizeof("soc"));

	return ret;
}


