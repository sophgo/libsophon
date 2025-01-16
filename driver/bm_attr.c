#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/math64.h>
#include "bm_common.h"
#include "bm_attr.h"
#include "bm_ctl.h"
// #include "bm_msgfifo.h"

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
static ssize_t store_usage_enable(struct device *d,
																	struct device_attribute *attr, const char *buf, size_t count)
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


void bmdrv_thermal_init(struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < BM_THERMAL_WINDOW_WIDTH; i++)
		bmdi->c_attr.thermal_info.elapsed_temp[i] = 0;

	bmdi->c_attr.thermal_info.tmp = 0;
	bmdi->c_attr.thermal_info.idx = 0;
	bmdi->c_attr.thermal_info.index = 0;
	bmdi->c_attr.thermal_info.is_off = 0;
}

static void calculate_board_status(struct bm_device_info *bmdi)
{
	if (bmdi->status_over_temp)
	{
		bmdi->status |= CHIP_OVER_TEMP_MASK;
	}
	else
	{
		bmdi->status &= ~CHIP_OVER_TEMP_MASK;
	}

	if (bmdi->status_pcie)
	{
		bmdi->status |= PCIE_LINK_ERROR_MASK;
	}
	else
	{
		bmdi->status &= ~PCIE_LINK_ERROR_MASK;
	}

	if (bmdi->status_sync_api)
	{
		bmdi->status |= TPU_HANG_MASK;
	}
	else
	{
		bmdi->status &= ~TPU_HANG_MASK;
	}
}

void board_status_update(struct bm_device_info *bmdi, int cur_tmp, int cur_tpu_clk)
{

	calculate_board_status(bmdi);
}


void bmdrv_thermal_update_status(struct bm_device_info *bmdi, int cur_tmp)
{
	int avg_tmp = 0;
	int cur_tpu_clk = 0;
	int i = 0;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int new_led_status = c_attr->led_status;
	int target = 0;

	cur_tpu_clk = c_attr->tpu_current_clock;
	c_attr->thermal_info.elapsed_temp[bmdi->c_attr.thermal_info.idx] = cur_tmp;
	if (c_attr->thermal_info.idx++ >= BM_THERMAL_WINDOW_WIDTH - 1)
		c_attr->thermal_info.idx = 0;

	for (i = 0; i < BM_THERMAL_WINDOW_WIDTH; i++)
		avg_tmp += c_attr->thermal_info.elapsed_temp[i];

	avg_tmp = div_u64(avg_tmp, BM_THERMAL_WINDOW_WIDTH);

	if (0 == bmdi->enable_dyn_freq)
	{
		if (cur_tpu_clk >= bmdi->boot_info.tpu_min_clk &&
				cur_tpu_clk < div_u64(bmdi->boot_info.tpu_max_clk * 8, 10))
		{
			new_led_status = LED_BLINK_THREE_TIMES_PER_S;
		}
		else if (cur_tpu_clk >= (div_u64((bmdi->boot_info.tpu_max_clk * 8), 10)) &&
						 cur_tpu_clk < bmdi->boot_info.tpu_max_clk)
		{
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		}
		else if (cur_tpu_clk == bmdi->boot_info.tpu_max_clk)
		{
			new_led_status = LED_BLINK_ONE_TIMES_PER_2S;
		}
	}
	else
	{
		if (avg_tmp > c_attr->thermal_info.min_clk_tmp && cur_tpu_clk != bmdi->boot_info.tpu_min_clk)
		{
			pr_info("bm-sophon %d, bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to min\n",
							bmdi->dev_index, cur_tpu_clk, cur_tmp, avg_tmp);
			target = bmdi->boot_info.tpu_min_clk;

			// bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_THREE_TIMES_PER_S;
		}
		else if (avg_tmp < c_attr->thermal_info.half_clk_tmp && avg_tmp > (c_attr->thermal_info.half_clk_tmp - 5) && cur_tpu_clk == (bmdi->boot_info.tpu_min_clk))
		{
			pr_info("bm-sohpon %d, bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to mid\n",
							bmdi->dev_index, cur_tpu_clk, cur_tmp, avg_tmp);
			target = div_u64((bmdi->boot_info.tpu_max_clk * 8), 10);

			// bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		}
		else if (avg_tmp > c_attr->thermal_info.half_clk_tmp && avg_tmp < (c_attr->thermal_info.min_clk_tmp) && cur_tpu_clk == (bmdi->boot_info.tpu_max_clk))
		{
			pr_info("bm-sophon %d, bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to mid\n",
							bmdi->dev_index, cur_tpu_clk, cur_tmp, avg_tmp);
			target = div_u64((bmdi->boot_info.tpu_max_clk * 8), 10);

			// bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		}
		else if (avg_tmp < (c_attr->thermal_info.half_clk_tmp - 5) && cur_tpu_clk != bmdi->boot_info.tpu_max_clk)
		{
			// pr_info("bm-sophon %d, bmdrv_thermal_update_status cur_tmp = %d, avg tmp = %d,
			// 	change to max\n", bmdi->dev_index, cur_tmp, avg_tmp);
			target = bmdi->boot_info.tpu_max_clk;

			// bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_ONE_TIMES_PER_2S;
		}
	}

	board_status_update(bmdi, cur_tmp, cur_tpu_clk);
	mutex_lock(&c_attr->attr_mutex);
	if (c_attr->bm_set_led_status &&
			new_led_status != c_attr->led_status &&
			c_attr->led_status != LED_ON &&
			c_attr->led_status != LED_OFF)
	{
		c_attr->bm_set_led_status(bmdi, new_led_status);
		c_attr->led_status = new_led_status;
	}
	mutex_unlock(&c_attr->attr_mutex);
}


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
	atomic_set(&c_attr->npu_utilization1, 0);
	c_attr->npu_timer_interval = 500;
	c_attr->npu_busy_time_sum_ms = 0ULL;
	c_attr->npu_busy_time_sum_ms1 = 0ULL;
	c_attr->npu_start_probe_time = 0ULL;
	c_attr->npu_start_probe_time1 = 0ULL;
	c_attr->npu_status_idx = 0;
	c_attr->npu_status_idx1 = 0;
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

	c_attr->bm_get_npu_util = bm_read_npu_util;
	c_attr->bm_get_npu_util1 = bm_read_npu_util1;

	return ret;
}

void bm_npu_utilization_stat(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int i = 0;
	int npu_status_stat = 0;


		c_attr->npu_status[c_attr->npu_status_idx] = 1;
		c_attr->npu_busy_time_sum_ms += div_u64(c_attr->npu_timer_interval, NPU_STAT_WINDOW_WIDTH);

	c_attr->npu_start_probe_time += div_u64(c_attr->npu_timer_interval, NPU_STAT_WINDOW_WIDTH);
	c_attr->npu_status_idx = (c_attr->npu_status_idx + 1) % NPU_STAT_WINDOW_WIDTH;

	for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
		npu_status_stat += c_attr->npu_status[i];
	atomic_set(&c_attr->npu_utilization, npu_status_stat << 1);
}

void bm_npu_utilization_stat1(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int i = 0;
	int npu_status_stat = 0;

	c_attr->npu_status1[c_attr->npu_status_idx1] = 0;

	c_attr->npu_start_probe_time1 += div_u64(c_attr->npu_timer_interval, NPU_STAT_WINDOW_WIDTH);
	c_attr->npu_status_idx1 = (c_attr->npu_status_idx1 + 1) % NPU_STAT_WINDOW_WIDTH;

	for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
		npu_status_stat += c_attr->npu_status1[i];
	atomic_set(&c_attr->npu_utilization1, npu_status_stat << 1);
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

static int set_led_on(struct bm_device_info *bmdi)
{
	return 0;
}

static int set_led_off(struct bm_device_info *bmdi)
{
	return 0;
}

static int set_led_blink_1_per_2s(struct bm_device_info *bmdi)
{
	return 0;
}

static int set_led_blink_1_per_s(struct bm_device_info *bmdi)
{
	return 0;
}

static int set_led_blink_3_per_s(struct bm_device_info *bmdi)
{
	return 0;
}

static int set_led_blink_fast(struct bm_device_info *bmdi)
{
	return 0;
}

int set_led_status(struct bm_device_info *bmdi, int led_status)
{
	switch (led_status)
	{
	case LED_ON:
		return set_led_on(bmdi);
	case LED_OFF:
		return set_led_off(bmdi);
	case LED_BLINK_FAST:
		return set_led_blink_fast(bmdi);
	case LED_BLINK_ONE_TIMES_PER_S:
		return set_led_blink_1_per_s(bmdi);
	case LED_BLINK_ONE_TIMES_PER_2S:
		return set_led_blink_1_per_2s(bmdi);
	case LED_BLINK_THREE_TIMES_PER_S:
		return set_led_blink_3_per_s(bmdi);
	default:
		return -1;
	}
}

int set_ecc(struct bm_device_info *bmdi, int ecc_enable)
{
	return 0;
}

void bmdrv_adjust_fan_speed(struct bm_device_info *bmdi, u32 temp)
{
}



int bm_read_npu_util(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int timer_on = 0;

	timer_on = atomic_read(&bmdi->c_attr.timer_on);
	if (timer_on)
		return atomic_read(&c_attr->npu_utilization);
	else
		return ATTR_NOTSUPPORTED_VALUE;
}

int bm_read_npu_util1(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int timer_on = 0;

	timer_on = atomic_read(&bmdi->c_attr.timer_on);
	if (timer_on)
		return atomic_read(&c_attr->npu_utilization1);
	else
		return ATTR_NOTSUPPORTED_VALUE;
}





void bmdrv_fetch_attr(struct bm_device_info *bmdi, int count, int is_setspeed)
{


	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	long start = 0;
	long end = 0;
	int delt = 0;

	start = jiffies;
	bm_npu_utilization_stat(bmdi);
	bm_npu_utilization_stat1(bmdi);


	if (count == 17) {
		mutex_lock(&c_attr->attr_mutex);
		// if (bmdi->cinfo.chip_id == CHIP_ID)
		// {
		// }

		mutex_unlock(&c_attr->attr_mutex);
		if (bmdi->status_sync_api != 0)
			calculate_board_status(bmdi); // update soc mode
		end = jiffies;
		delt = jiffies_to_msecs(end - start);
		// PR_TRACE("dev_index=%d,bm_smbus_update_dev_info time is %d ms\n",bmdi->dev_index, delt);
		msleep_interruptible(((10 - delt) > 0) ? 10 - delt : 0);
	} else
		msleep_interruptible(10);
}

void bmdrv_fetch_attr_board_power(struct bm_device_info *bmdi, int count)
{
	long start = 0;
	long end = 0;
	int delt = 0;
	int threshold = 0;


	start = jiffies;
	bm_npu_utilization_stat(bmdi);
	bm_npu_utilization_stat1(bmdi);

		threshold = 17;

	if (count == threshold)
	{

		end = jiffies;
		delt = jiffies_to_msecs(end - start); // delt is about 4ms
		// PR_TRACE("dev_index=%d,bm_get_board_power time is %d ms\n",bmdi->dev_index,delt);
		msleep_interruptible(((10 - delt) > 0) ? 10 - delt : 0);
	} else {
		msleep_interruptible(10);
	}
}

int bm_get_name(struct bm_device_info *bmdi, unsigned long arg)
{

	int ret = 0;

	ret = copy_to_user((unsigned char __user *)arg, "soc", sizeof("soc"));

	return ret;
}


