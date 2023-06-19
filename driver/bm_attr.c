#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "i2c.h"
#include "spi.h"
#include "pwm.h"
#include "bm_attr.h"
#include "bm_ctl.h"
#include "bm_msgfifo.h"
#include "bm1684/bm1684_clkrst.h"
#include "bm1684/bm1684_card.h"
#ifndef SOC_MODE
#include "bm_pcie.h"
#include "console.h"
#endif

#define FREQ0DATA              0x024
#define FREQ1DATA              0x02c
/*
 * Cat value of the npu usage
 * Test: $cat /sys/class/bm-sophon/bm-sophon0/device/npu_usage
 */
static ssize_t npu_usage_show(struct device *d, struct device_attribute *attr, char *buf)
{
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
	struct bm_chip_attr *cattr = NULL;
	int usage = 0;
	int usage_all = 0;

	cattr = &bmdi->c_attr;

	if (atomic_read(&cattr->timer_on) == 0)
		return sprintf(buf, "Please, set [Usage enable] to 1\n");

	usage = (int)atomic_read(&cattr->npu_utilization);
	usage_all = cattr->npu_busy_time_sum_ms * 100/cattr->npu_start_probe_time;

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
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
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
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	/* Check the validity of the parameters */
	if (-1 == check_interval_store(buf)) {
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
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif

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

#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	/* Check the validity of the parameters */
	if (-1 == check_enable_store(buf)) {
		pr_info("Parameter error! Parameter: 0 or 1\n");
		return -1;
	}

	sscanf(buf, "%d", &enable);
	if ((enable == 1) && (atomic_read(&cattr->timer_on) == 0)) {
		atomic_set(&cattr->timer_on, 1);
	} else if (enable == 0) {
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

#ifndef SOC_MODE
int bmdrv_get_tpu_target_freq(struct bm_device_info *bmdi, enum bm_freq_scaling_caller caller, int *target)
{
	int vfs_freq_target = 0;
	struct bm_freq_scaling_db * p_data = NULL;
	int tmp_freq_target = 0;
	int tpu_max_freq = 0;
	int freq_min = 0;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
	    (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		p_data = bmdi->bmcd->vfs_db;
		if((p_data == NULL) || (p_data->start_flag == VFS_ORIGIN_MODE)) {
			return 0;
		}
		if(caller == FREQ_CALLER_TEMP) {
			p_data->thermal_freq[bmdi->cinfo.chip_index] = *target;
		}
		tmp_freq_target = p_data->thermal_freq[bmdi->cinfo.chip_index];
		vfs_freq_target = p_data->freq_volt_pair[p_data->vf_level].freq;
		tpu_max_freq = bmdi->boot_info.tpu_max_clk;
		freq_min = min(tmp_freq_target, tpu_max_freq);
		*target = min(freq_min, vfs_freq_target);
	}

	return 0;
}
#endif

void bmdrv_thermal_init(struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < BM_THERMAL_WINDOW_WIDTH; i++)
		bmdi->c_attr.thermal_info.elapsed_temp[i] = 0;

	bmdi->c_attr.thermal_info.tmp = 0;
	bmdi->c_attr.thermal_info.idx = 0;
	bmdi->c_attr.thermal_info.index = 0;
	bmdi->c_attr.thermal_info.is_off = 0;
#ifndef SOC_MODE
	bm1684_get_clk_temperature(bmdi);
#endif
}

static void  calculate_board_status(struct bm_device_info *bmdi)
{
	if (bmdi->status_over_temp) {
		bmdi->status |= CHIP_OVER_TEMP_MASK;
	} else {
		bmdi->status &= ~CHIP_OVER_TEMP_MASK;
	}

	if (bmdi->status_pcie) {
		bmdi->status |= PCIE_LINK_ERROR_MASK;
	} else {
		bmdi->status &= ~PCIE_LINK_ERROR_MASK;
	}

	if (bmdi->status_sync_api) {
		bmdi->status |= TPU_HANG_MASK;
	} else {
		bmdi->status &= ~TPU_HANG_MASK;
	}
}

void board_status_update(struct bm_device_info *bmdi, int cur_tmp, int cur_tpu_clk)
{
	int fusing_tmp = 95;
	int support_tmp = 90;
#ifndef SOC_MODE
	bm1684_get_fusing_temperature(bmdi, &fusing_tmp, &support_tmp);
#endif
	if ((bmdi->cinfo.chip_id == 0x1684) && (bmdi->misc_info.pcie_soc_mode == 0)) {
		if ((cur_tpu_clk < bmdi->boot_info.tpu_min_clk) || (cur_tpu_clk > bmdi->boot_info.tpu_max_clk)) {
			bmdi->status_pcie = 1;
		} else if (cur_tmp > fusing_tmp) {
			bmdi->status_over_temp = 1;
		} else if ((cur_tpu_clk >= bmdi->boot_info.tpu_min_clk)
			&& (cur_tpu_clk <= bmdi->boot_info.tpu_max_clk) && (cur_tmp < support_tmp)) {
			bmdi->status_pcie = 0;
			bmdi->status_over_temp = 0;
		}
	}

	calculate_board_status(bmdi);
}

#ifndef SOC_MODE
/* limit the frequency of SC7 Pro to avoid unpredictable problem
 * case 1 temperature >= 105, the board will shutdown and need to reset the whole environment(reboot)
 * case 2 temperature >= 95 && cur_freq != 25M, immediately decrease frequency to 25M
 * case 3 temperature >= 65 && current frequency == 1000M, decrease frequence to 750M
 * case 4 temperature >= 65 && (temperature < 85 && current frequency == 25M), increase the frequency to 750M
 * case 5 temperature < 60 && cur_freq != 1000M && index reach 50 times, increase frequency to 1000M
 * @param bmdi 		the info of current card
 * @param cur_tmp	the current average temperature of each chip
*/
static void bmdrv_thermal_update_status_sc7p(struct bm_device_info *bmdi, int cur_tmp)
{
#define BM_THERMAL_HALF_LEVEL_SC7P 10	// only used for sc7 pro
#define BM_THERMAL_MAX_LEVEL_SC7P 50
	int cur_tpu_clk = 0;
	int target = 0;
	int avg_tmp = 0;
	int ret = 0;
	struct console_ctx console;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int index = c_attr->thermal_info.index;

	//if the termperature is higher than 105, power down the chip immediately
	if (cur_tmp >= c_attr->thermal_info.extreme_tmp) {
		avg_tmp = cur_tmp;
		goto extreme;
	}

	// calculate average temperature and get current frequency
	// every 50 times, the index will be reset to 0
	cur_tpu_clk = c_attr->tpu_current_clock;
	avg_tmp = (c_attr->thermal_info.tmp*index)/(index+1) + cur_tmp/(index+1);
	bmdi->c_attr.thermal_info.tmp = avg_tmp;

	//if the termperature is higher than 95, decrease the frequency immediately
	if (cur_tmp >= c_attr->thermal_info.min_clk_tmp) {
		avg_tmp = cur_tmp;
	}

extreme:
	// set dynamic strartegy for sc7 pro
	if (avg_tmp >= c_attr->thermal_info.extreme_tmp) {
		if (c_attr->thermal_info.is_off == 0) {
			pr_info("bm-sophon %d, current temperature is over 105 celsius degree, \
				the chip is going to shut down now\n", bmdi->dev_index);
			bmdi->c_attr.thermal_info.is_off = 1;
			console.uart.bmdi = bmdi->bmcd->sc5p_mcu_bmdi;
			console.uart.uart_index = 0x2;
			ret = console_cmd_sc7p_poweroff(&console, bmdi->cinfo.chip_index);
		}
	}else if (avg_tmp >= c_attr->thermal_info.min_clk_tmp) {
		if (cur_tpu_clk != bmdi->boot_info.tpu_min_clk) {
			target = bmdi->boot_info.tpu_min_clk;
			bmdrv_clk_set_tpu_target_freq(bmdi,target);
		}
	} else if (avg_tmp >= c_attr->thermal_info.half_clk_tmp) {
		target = (bmdi->boot_info.tpu_max_clk * 75) / 100;
		if ((avg_tmp < (c_attr->thermal_info.min_clk_tmp-10)) &&
			(cur_tpu_clk == bmdi->boot_info.tpu_min_clk)) {
				if ((index % BM_THERMAL_HALF_LEVEL_SC7P) == 0) {
					bmdrv_clk_set_tpu_target_freq(bmdi,target);
				}
		} else if (cur_tpu_clk == bmdi->boot_info.tpu_max_clk) {
			bmdrv_clk_set_tpu_target_freq(bmdi,target);
		}
	} else if (avg_tmp < c_attr->thermal_info.max_clk_tmp) {
		if ((cur_tpu_clk != bmdi->boot_info.tpu_max_clk) &&
			((index % BM_THERMAL_MAX_LEVEL_SC7P) == 0)) {
			target = bmdi->boot_info.tpu_max_clk;
			bmdrv_clk_set_tpu_target_freq(bmdi,target);
		}
	}
	index = (index + 1) % BM_THERMAL_MAX_LEVEL_SC7P;
	bmdi->c_attr.thermal_info.index = index;
	board_status_update(bmdi, cur_tmp, cur_tpu_clk);
}
#endif

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

	avg_tmp = avg_tmp/BM_THERMAL_WINDOW_WIDTH;

	if (0 == bmdi->enable_dyn_freq) {
		if (cur_tpu_clk >= bmdi->boot_info.tpu_min_clk &&
			cur_tpu_clk < bmdi->boot_info.tpu_max_clk * 8 / 10) {
			new_led_status = LED_BLINK_THREE_TIMES_PER_S;
		} else if (cur_tpu_clk >= ((bmdi->boot_info.tpu_max_clk * 8) / 10) &&
			cur_tpu_clk < bmdi->boot_info.tpu_max_clk) {
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		} else if (cur_tpu_clk == bmdi->boot_info.tpu_max_clk) {
			new_led_status = LED_BLINK_ONE_TIMES_PER_2S;
		}
	} else {
		if (avg_tmp > c_attr->thermal_info.min_clk_tmp
				&& cur_tpu_clk != bmdi->boot_info.tpu_min_clk) {
			pr_info("bm-sophon %d, bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to min\n", bmdi->dev_index, cur_tpu_clk, cur_tmp, avg_tmp);
			target = bmdi->boot_info.tpu_min_clk;
#ifndef SOC_MODE
			bmdrv_get_tpu_target_freq(bmdi, FREQ_CALLER_TEMP, &target);
#endif
			bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_THREE_TIMES_PER_S;
		} else if (avg_tmp < c_attr->thermal_info.half_clk_tmp
				&& avg_tmp > (c_attr->thermal_info.half_clk_tmp - 5)
				&& cur_tpu_clk == (bmdi->boot_info.tpu_min_clk)) {
			pr_info("bm-sohpon %d, bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to mid\n", bmdi->dev_index, cur_tpu_clk, cur_tmp, avg_tmp);
			target = (bmdi->boot_info.tpu_max_clk * 8) / 10;
#ifndef SOC_MODE
			bmdrv_get_tpu_target_freq(bmdi, FREQ_CALLER_TEMP, &target);
#endif
			bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		} else if (avg_tmp > c_attr->thermal_info.half_clk_tmp
				&& avg_tmp < (c_attr->thermal_info.min_clk_tmp)
				&& cur_tpu_clk == (bmdi->boot_info.tpu_max_clk)) {
			pr_info("bm-sophon %d, bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to mid\n", bmdi->dev_index, cur_tpu_clk, cur_tmp, avg_tmp);
			target = (bmdi->boot_info.tpu_max_clk * 8) / 10;
#ifndef SOC_MODE
			bmdrv_get_tpu_target_freq(bmdi, FREQ_CALLER_TEMP, &target);
#endif
			bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		} else if (avg_tmp < (c_attr->thermal_info.half_clk_tmp - 5)
				&& cur_tpu_clk != bmdi->boot_info.tpu_max_clk) {
			// pr_info("bm-sophon %d, bmdrv_thermal_update_status cur_tmp = %d, avg tmp = %d,
			// 	change to max\n", bmdi->dev_index, cur_tmp, avg_tmp);
			target = bmdi->boot_info.tpu_max_clk;
#ifndef SOC_MODE
			bmdrv_get_tpu_target_freq(bmdi, FREQ_CALLER_TEMP, &target);
#endif
			bmdrv_clk_set_tpu_target_freq(bmdi, target);
			new_led_status = LED_BLINK_ONE_TIMES_PER_2S;
		}
	}

	board_status_update(bmdi, cur_tmp, cur_tpu_clk);
	mutex_lock(&c_attr->attr_mutex);
	if (c_attr->bm_set_led_status &&
		new_led_status != c_attr->led_status &&
		c_attr->led_status != LED_ON &&
		c_attr->led_status != LED_OFF) {
		c_attr->bm_set_led_status(bmdi, new_led_status);
		c_attr->led_status = new_led_status;
	}
	mutex_unlock(&c_attr->attr_mutex);
}

#ifndef SOC_MODE
static void bm_set_temp_position(struct bm_device_info *bmdi, u8 pos)
{
	if (bmdi->cinfo.chip_id == 0x1682)
		top_reg_write(bmdi, 0x01C, (0x1 << pos));
}

/* tmp451 range mode */
static int bm_set_tmp451_range_mode(struct bm_device_info *bmdi)
{
	int ret = 0;
	u8 cfg = 0;
	u32 i2c_index = 1;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS))
		i2c_index = 0;

	bm_set_temp_position(bmdi, 5);
	bm_i2c_set_target_addr(bmdi, i2c_index, 0x4c);
	ret = bm_i2c_read_byte(bmdi, i2c_index, 0x3, &cfg);
	ret = bm_i2c_write_byte(bmdi, i2c_index, 0x9, cfg | 4);
	return ret;
}
#endif

int bmdrv_card_attr_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	int i = 0;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
#ifndef SOC_MODE
	int value = 0;
#endif

	c_attr->fan_speed = 100;
	c_attr->fan_rev_read = 0;
	c_attr->npu_cnt = 0;
	c_attr->npu_busy_cnt = 0;
	atomic_set(&c_attr->npu_utilization, 0);
	c_attr->npu_timer_interval = 500;
	c_attr->npu_busy_time_sum_ms = 0ULL;
	c_attr->npu_start_probe_time = 0ULL;
	c_attr->npu_status_idx = 0;
	for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
		c_attr->npu_status[i] = 0;
	atomic_set(&c_attr->timer_on, 0);
	mutex_init(&c_attr->attr_mutex);

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
#ifndef SOC_MODE
		c_attr->fan_control = true;
#else
		c_attr->fan_control = false;
#endif
		break;
	case 0x1684:
#ifndef SOC_MODE
		c_attr->bm_get_tpu_power = bm_read_vdd_tpu_power;
		c_attr->bm_get_vddc_power = NULL;
		c_attr->bm_get_vddphy_power = NULL;
		c_attr->fan_control = bmdi->boot_info.fan_exist;
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS ||
			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H ||
			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) {
			/* fix this later with bootinfo */
			c_attr->bm_set_led_status = set_led_status;
		}
		if (bmdi->boot_info.board_power_sensor_exist) {
			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_EVB ||
				BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5)
				c_attr->bm_get_board_power = bm_read_sc5_power;
			else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
				c_attr->bm_get_board_power = bm_read_sc5_plus_power;
			else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
				c_attr->bm_get_board_power = bm_read_sc5_power;
			else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5M_P) {
				if (BM1684_HW_VERSION(bmdi) == 0x30 || BM1684_HW_VERSION(bmdi) == 0x31) {
					c_attr->bm_get_board_power = NULL;
					c_attr->board_power = ATTR_NOTSUPPORTED_VALUE;
				} else {
					c_attr->bm_get_board_power = bm_read_sc5_power;
				}
			} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO)
				c_attr->bm_get_board_power = bm_read_sc5_pro_power;
		} else {
			c_attr->bm_get_board_power = NULL;
		}
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
			value = top_reg_read(bmdi, 0x470);
			value &= ~(0x1 << 4);
			value |= (0x1 << 4);
			top_reg_write(bmdi, 0x470, value);     /* Selector for FAN1 */
		} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
			value = top_reg_read(bmdi, 0x46c);
			value &= ~(0x1 << 20);
			value |= (0x1 << 20);
			top_reg_write(bmdi, 0x46c, value);     /* Selector for FAN0 */
		}

		bmdrv_thermal_init(bmdi);
		c_attr->bm_get_fan_speed = bm_get_fan_speed;
		c_attr->bm_get_chip_temp = bm_read_tmp451_remote_temp;
		c_attr->bm_get_board_temp = bm_read_tmp451_local_temp;

		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_S ||
			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_P) {
			c_attr->bm_get_chip_temp = bm_read_mcu_chip_temp;
			c_attr->bm_get_board_temp = bm_read_mcu_board_temp;
		} else {
			ret = bm_set_tmp451_range_mode(bmdi);
		}
#else
		c_attr->bm_get_tpu_power = NULL;
		c_attr->bm_get_vddc_power = NULL;
		c_attr->bm_get_vddphy_power = NULL;
		c_attr->bm_get_board_power = NULL;
		c_attr->fan_control = false;
		c_attr->bm_get_chip_temp = NULL;
		c_attr->bm_get_board_temp = NULL;
#endif
		break;
	case 0x1686:
#ifndef  SOC_MODE
		c_attr->bm_get_tpu_power = bm_read_vdd_tpu_power;
		c_attr->bm_get_vddc_power = NULL;
		c_attr->bm_get_vddphy_power = NULL;
		if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS) ||
			(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO)) {
			/* fix this later with bootinfo */
			c_attr->bm_set_led_status = set_led_status;
		}
		if (bmdi->boot_info.board_power_sensor_exist) {
			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
				c_attr->bm_get_board_power = bm_read_1684x_evb_power;
			} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
					   (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
				c_attr->bm_get_board_power = bm_read_sc5_pro_power;
				c_attr->bm_get_vddc_power = bm_read_vddc_power;
				c_attr->bm_get_vddphy_power = bm_read_vddphy_power;
				if (bmdrv_sc5pro_uart_is_connect_mcu(bmdi) == 1) {
					char mode[8] = "verbose";
					struct console_ctx console;
					console.uart.bmdi = bmdi;
					console.uart.uart_index = 0x2;
					console_cmd_sc7p_set_mon_mode(&console, mode);
				}
			}
			else {
				c_attr->bm_get_board_power = NULL;
			}
		} else {
			c_attr->bm_get_board_power = NULL;
		}

		c_attr->fan_control = bmdi->boot_info.fan_exist;
		bmdrv_thermal_init(bmdi);

		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
			c_attr->bm_get_chip_temp = bm_read_tmp451_remote_temp_by_mcu;
			c_attr->bm_get_board_temp = bm_read_tmp451_local_temp_by_mcu;
		} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
				   (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
			c_attr->bm_get_chip_temp = bm_read_tmp451_remote_temp;
			c_attr->bm_get_board_temp = bm_read_tmp451_local_temp;
			ret = bm_set_tmp451_range_mode(bmdi);
		} else {
			c_attr->bm_get_chip_temp = NULL;
			c_attr->bm_get_board_temp = NULL;
		}

#else
		c_attr->bm_get_tpu_power = NULL;
		c_attr->bm_get_vddc_power = NULL;
		c_attr->bm_get_vddphy_power = NULL;
		c_attr->bm_get_board_power = NULL;
		c_attr->fan_control = false;
		c_attr->bm_get_chip_temp = NULL;
		c_attr->bm_get_board_temp = NULL;
#endif
		break;
	default:
		return -EINVAL;
	}

	c_attr->bm_get_npu_util = bm_read_npu_util;

	return ret;
}

void bm_npu_utilization_stat(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = & bmdi->c_attr;
	int i = 0;
	int npu_status_stat = 0;

	if (!bmdev_msgfifo_empty(bmdi, BM_MSGFIFO_CHANNEL_XPU)) {
			c_attr->npu_status[c_attr->npu_status_idx] = 1;
			c_attr->npu_busy_time_sum_ms += c_attr->npu_timer_interval/NPU_STAT_WINDOW_WIDTH;
	} else {
		c_attr->npu_status[c_attr->npu_status_idx] = 0;
	}

	c_attr->npu_start_probe_time += c_attr->npu_timer_interval/NPU_STAT_WINDOW_WIDTH;
	c_attr->npu_status_idx = (c_attr->npu_status_idx+1)%NPU_STAT_WINDOW_WIDTH;

	for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
		npu_status_stat += c_attr->npu_status[i];
	atomic_set(&c_attr->npu_utilization, npu_status_stat<<1);
}

int reset_fan_speed(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return set_pwm_high(bmdi, 0x1);
	else
		return set_pwm_high(bmdi, 0x0);
#else
	return 0;
#endif
}

int bmdrv_enable_attr(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	struct chip_info *cinfo = &bmdi->cinfo;
	int rc;

	if (c_attr->fan_control)
		reset_fan_speed(bmdi);

	rc = sysfs_create_group(&cinfo->device->kobj, &bm_npu_attribute_group);
	if (rc) {
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
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return set_pwm_level(bmdi, FAN_PWM_PERIOD, spd_level, 0x1);
	else
		return set_pwm_level(bmdi, FAN_PWM_PERIOD, spd_level, 0x0);
#else
	return 0;
#endif
}

static int set_led_on(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	u32 reg_val = 0x0;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		reg_val = gpio_reg_read(bmdi, 0x4 + 0x800); //gpio78 for led
		reg_val |= 1 << 13;
		gpio_reg_write(bmdi, 0x4 + 0x800, reg_val);
		reg_val = gpio_reg_read(bmdi, 0x0 + 0x800);
		reg_val |= 1 << 13;
		gpio_reg_write(bmdi, 0x0 + 0x800, reg_val);
		return 0;
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		   (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		return set_pwm_high(bmdi, 0);
	else
		return 0;

#else
	return 0;
#endif
}

static int set_led_off(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	u32 reg_val = 0x0;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		reg_val = gpio_reg_read(bmdi, 0x4 + 0x800);
		reg_val |= 1 << 13;
		gpio_reg_write(bmdi, 0x4 + 0x800, reg_val);
		reg_val = gpio_reg_read(bmdi, 0x0 + 0x800);
		reg_val &= ~(1 << 13);
		gpio_reg_write(bmdi, 0x0 + 0x800, reg_val);
		return 0;
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		   (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		return set_pwm_low(bmdi, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_1_per_2s(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
	    (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		return set_pwm_level(bmdi, LED_PWM_PERIOD*2, 50, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_1_per_s(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
	    (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		return set_pwm_level(bmdi, LED_PWM_PERIOD, 25, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_3_per_s(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
	    (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		return set_pwm_level(bmdi, LED_PWM_PERIOD / 3, 17, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_fast(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
	    (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS || BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO))
		return set_pwm_level(bmdi, LED_PWM_PERIOD / 2, 50, 0);
	else
		return 0;
#else
	return 0;
#endif
}

int set_led_status(struct bm_device_info *bmdi, int led_status)
{
	switch (led_status) {
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
#ifdef SOC_MODE
	return 0;
#else
	struct bm_boot_info boot_info;

	if (bm_spi_flash_get_boot_info(bmdi, &boot_info))
		return -1;

	if (ecc_enable == boot_info.ddr_ecc_enable)
		return 0;

	boot_info.ddr_ecc_enable = ecc_enable;
	if (bm_spi_flash_update_boot_info(bmdi, &boot_info))
		return -1;
	return 0;
#endif
}

#ifndef SOC_MODE
int board_type_sc5_rev_to_duty(u16 fan_rev)
{
	u32 fan_duty = 0;

	if ((fan_rev > 0) && (fan_rev < 2000))
		fan_duty = 3;
	else if ((fan_rev >= 2000) && (fan_rev < 2520))
		fan_duty = (u32)(20*fan_rev-36130)/1000;
	else if ((fan_rev >= 2520) && (fan_rev < 2970))
		fan_duty = (u32)(22*fan_rev-41000)/1000;
	else if ((fan_rev >= 2970) && (fan_rev < 3990))
		fan_duty = (u32)(245*fan_rev-477940)/10000;
	else if ((fan_rev >= 3990) && (fan_rev < 4320))
		fan_duty = (u32)(303*fan_rev-709090)/10000;
	else if ((fan_rev >= 4320) && (fan_rev < 4590))
		fan_duty = (u32)(37*fan_rev-100000)/1000;
	else if ((fan_rev >= 4590) && (fan_rev < 5400))
		fan_duty = (u32)(4*fan_rev-11360)/100;
	else if (fan_rev >= 5400)
		fan_duty = 100;
	else if (fan_rev == 0)
		fan_duty = 0;
	return fan_duty;
}

int board_type_sc5h_rev_to_duty(u16 fan_rev)
{
	u32 fan_duty = 0;

	if ((fan_rev > 0) && (fan_rev <= 2000))
		fan_duty = (u32)(83*fan_rev+33333)/10000;
	else if ((fan_rev > 2000) && (fan_rev <= 4000))
		fan_duty = (u32)fan_rev/100;
	else if ((fan_rev > 4000) && (fan_rev <= 6400))
		fan_duty = (u32)(125*fan_rev-100000)/10000;
	else if ((fan_rev > 6400) && (fan_rev <= 7800))
		fan_duty = (u32)(143*fan_rev-214290)/10000;
	else if ((fan_rev > 7800) && (fan_rev <= 8400))
		fan_duty = (u32)(167*fan_rev-400000)/10000;
	else if (fan_rev > 8400)
		fan_duty = 100;
	else if (fan_rev == 0)
		fan_duty = 0;
	return fan_duty;
}

int bm_get_fixed_fan_speed(struct bm_device_info *bmdi, u32 temp)
{
	u16 fan_spd = 100;

	if (0 == bmdi->fixed_fan_speed) {
		if (temp > 61)
			fan_spd = 100;
		else if (temp <= 61 && temp > 35)
			fan_spd = 20 + (temp - 35) * 8 / 3;
		else if (temp <= 35 && temp > 20)
			fan_spd = 20;
		else if (temp <= 20)
			fan_spd = 10;
		return fan_spd;
	} else
		return bmdi->fixed_fan_speed;
}
#endif

void bmdrv_adjust_fan_speed(struct bm_device_info *bmdi, u32 temp)
{
#ifndef SOC_MODE
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	u16 fan_rev = 0;
	u32 fan_duty = 0;
	u32 fan_speed_set = 0;

	mutex_lock(&c_attr->attr_mutex);
	fan_speed_set = bm_get_fixed_fan_speed(bmdi, temp);
	//PR_TRACE("temp = %d,fan_speed_set= %d,fixed_fan_speed = %d\n",temp,fan_speed_set,bmdi->fixed_fan_speed);
	if (c_attr->fan_speed != fan_speed_set) {
		if (set_fan_speed(bmdi, fan_speed_set) != 0)
			pr_err("bmdrv: set fan speed failed.\n");
	}
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
		fan_rev = pwm_reg_read(bmdi, FREQ0DATA) * 30;
		c_attr->fan_rev_read = fan_rev;
		fan_duty = board_type_sc5_rev_to_duty(fan_rev);
	}

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		fan_rev = pwm_reg_read(bmdi, FREQ1DATA) * 30;
		c_attr->fan_rev_read = fan_rev;
		fan_duty = board_type_sc5h_rev_to_duty(fan_rev);
	}

	if (fan_duty > 100)
		fan_duty = 100;
	c_attr->fan_speed = fan_duty;
	if (bmdi->bmcd != NULL)
		bmdi->bmcd->fan_speed = fan_duty;
	mutex_unlock(&c_attr->attr_mutex);
#endif
}

#ifndef SOC_MODE
int bm_get_fan_speed(struct bm_device_info *bmdi)
{
	if (bmdi->bmcd != NULL)
		return bmdi->bmcd->fan_speed;

	return 0;
}
#endif

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

#ifndef SOC_MODE
/* read local temperature (board) */
int bm_read_tmp451_local_temp(struct bm_device_info *bmdi, int *temp)
{
	int ret = 0;
	u8 local_high = 0;
	int temps = 0;
	u32 i2c_index = 1;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS))
		i2c_index = 0;

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x4c);
	ret = bm_i2c_read_byte(bmdi, i2c_index, 0, &local_high);
	temps = (local_high & 0xf) + ((local_high & 0xff) >> 4) * 16;

	temps -= 64;

	*temp = temps;

	return ret;
}

/* read remote temperature (chip) */
int bm_read_tmp451_remote_temp(struct bm_device_info *bmdi, int *temp)
{
	int ret = 0;
	u8 local_high = 0;
	int temps = 0;
	u32 i2c_index = 1;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS))
		i2c_index = 0;

	bm_set_temp_position(bmdi, 5);
	bm_i2c_set_target_addr(bmdi, i2c_index, 0x4c);
	ret = bm_i2c_read_byte(bmdi, i2c_index, 0x1, &local_high);
	temps = (local_high & 0xf) + ((local_high & 0xff) >> 4) * 16;

	temps -= 64;

	if (ret)
		return ret;

	/* remote temperature is that the sensor inside our IC needs
	 * to be calibrated, the approximate deviation is 5℃
	 */
	*temp = temps - 5;
	PR_TRACE("remote temp = 0x%x\n", temps);
	return ret;
}

int bm_read_tmp451_local_temp_by_mcu(struct bm_device_info *bmdi, int *temp)
{
	int ret = 0;
	u8 local_high = 0;
	int temps = 0;
	u32 i2c_index = 1;
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_BM1684X_EVB)
		return -1;
#endif
	bm_i2c_set_target_addr(bmdi, i2c_index, 0x6B);
	ret = bm_i2c_read_byte(bmdi, i2c_index, 0, &local_high);
	temps = (local_high & 0xf) + ((local_high & 0xff) >> 4) * 16;
	temps -= 64;
	*temp = temps;

	return ret;
}

/* read remote temperature (chip) */
int bm_read_tmp451_remote_temp_by_mcu(struct bm_device_info *bmdi, int *temp)
{
	int ret = 0;
	u8 local_high = 0;
	int temps = 0;
	u32 i2c_index = 1;
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_BM1684X_EVB)
		return -1;
#endif
	bm_i2c_set_target_addr(bmdi, i2c_index, 0x6B);
	ret = bm_i2c_read_byte(bmdi, i2c_index, 0x1, &local_high);
	temps = (local_high & 0xf) + ((local_high & 0xff) >> 4) * 16;
	temps -= 64;
	if (ret)
		return ret;

	/* remote temperature is that the sensor inside our IC needs
	 * to be calibrated, the approximate deviation is 5℃
	 */
	*temp = temps - 5;
	//PR_TRACE("remote temp = 0x%x\n", temps);
	return ret;
}
#endif

#ifndef SOC_MODE
/*IS6808 */
// static int bm_read_is6808_test(struct bm_device_info *bmdi, u32 *value)
// {
//         int ret = 0;
//         u32 i2c_index = 0;
// 	u8 val = 0;

//         ret = bm_i2c_write_byte(bmdi, i2c_index, 0x01, 0x84);
//         if (ret) {
//                 pr_err("bmdrv i2c set cmd failed!\n");
//                 return ret;
//         }
//         ret = bm_i2c_read_byte(bmdi, i2c_index, 0x01, &val);
//         if (ret) {
//                 pr_err("bmdrv i2c read pmbus revision value failed!\n");
//                 return ret;
//         }
// 	*value = val;
//         PR_TRACE("bmdrv i2c IS6608a reg 0x01 value is 0x%x\n", *value);

// 	return ret;
// }

/* chip power is a direct value;
 * it is fetched from isl68127
 * which is attached to i2c0 (smbus)
 */

// static int bm_set_68127_voltage_out(struct bm_device_info *bmdi, int id, u32 volt)
// {
//         int ret = 0;
//         u32 i2c_index = 0;

//         ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
//         if (ret) {
//                 pr_err("bmdrv i2c id %x set cmd failed!\n", id);
//                 return ret;
//         }
//         ret = bm_i2c_write_hword(bmdi, i2c_index, 0x21, (u16)volt);
//         if (ret) {
//                 pr_err("bmdrv i2c set voltage failed!\n");
//                 return ret;
//         }
//         PR_TRACE("bmdrv i2c id %x voltage %d mV\n", id, volt);
//         return ret;
// }

static int bm_read_68224_power(struct bm_device_info *bmdi, int id, u32 *power)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x96, &temp);
	*power = temp;
	if (ret) {
		pr_err("bmdrv i2c read power value failed!\n");
		return ret;
	}
	PR_TRACE("bmdrv i2c id %x power %d W\n", id, *power);
	return ret;
}

static int bm_read_68224_voltage_out(struct bm_device_info *bmdi, int id, u32 *volt)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x8B, &temp);
	*volt = temp;
	if (ret) {
		pr_err("bmdrv i2c read voltage failed!\n");
		return ret;
	}
	PR_TRACE("bmdrv i2c id %x voltage %d mV\n", id, *volt);
	return ret;
}

static int bm_set_68224_voltage_out(struct bm_device_info *bmdi, int id, u32 volt)
{
	int ret = 0;
	u32 i2c_index = 0;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c id %x set cmd failed!\n", id);
		return ret;
	}
	ret = bm_i2c_write_hword(bmdi, i2c_index, 0x21, (u16)volt);
	if (ret) {
		pr_err("bmdrv i2c set voltage failed!\n");
		return ret;
	}
	PR_TRACE("bmdrv i2c id %x voltage %d mV\n", id, volt);
	return ret;
}

static int bm_read_68224_12v_voltage_in(struct bm_device_info *bmdi, u32 *volt)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, 0);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x88, &temp);
	*volt = temp;
	if (ret) {
		pr_err("bmdrv i2c read voltage failed!\n");
		return ret;
	}
	*volt = (*volt) * 10;
	PR_TRACE("bmdrv i2c 12V vin voltage %d mV\n", *volt);
	return ret;
}

static int bm_read_68224_current_out(struct bm_device_info *bmdi, int id, u32 *cur)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x8c, &temp);
	*cur = temp;
	if (ret) {
		pr_err("bmdrv i2c read current failed!\n");
		return ret;
	}
	*cur = (*cur) * 100;
	PR_TRACE("bmdrv i2c id %x current %d mA\n", id, *cur);
	return ret;
}

static int bm_read_68127_power(struct bm_device_info *bmdi, int id, u32 *power)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x96, &temp);
	*power = temp;
	if (ret) {
		pr_err("bmdrv i2c read power value failed!\n");
		return ret;
	}
	PR_TRACE("bmdrv i2c id %x power %d W\n", id, *power);
	return ret;
}

static int bm_read_68127_voltage_out(struct bm_device_info *bmdi, int id, u32 *volt)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x8b, &temp);
	*volt = temp;
	if (ret) {
		pr_err("bmdrv i2c read voltage failed!\n");
		return ret;
	}
	PR_TRACE("bmdrv i2c id %x voltage %d mV\n", id, *volt);
	return ret;
}

static int bm_read_68127_current_out(struct bm_device_info *bmdi, int id, u32 *cur)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x8c, &temp);
	*cur = temp;
	if (ret) {
		pr_err("bmdrv i2c read current failed!\n");
		return ret;
	}
	*cur = (*cur) * 100;
	PR_TRACE("bmdrv i2c id %x current %d mA\n", id, *cur);
	return ret;
}

/* chip power is a direct value;
 * it is fetched from is pxc1331
 * which is attached to i2c0 (smbus)
 */
static int bm_read_1331_power(struct bm_device_info *bmdi, int id, u32 *power)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x2d, &temp);
	*power = temp;
	if (ret) {
		pr_err("bmdrv i2c read power value failed!\n");
		return ret;
	}
	*power = (*power) * 40 / 1000;
	PR_TRACE("bmdrv smbus id %x power %d W\n", id, *power);
	return ret;
}

static int bm_read_1331_voltage_out(struct bm_device_info *bmdi, int id, u32 *volt)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x1A, &temp);
	*volt = temp;
	if (ret) {
		pr_err("bmdrv i2c read voltage failed!\n");
		return ret;
	}
	*volt = (*volt) * 5 / 4;

	PR_TRACE("bmdrv i2c id %x voltage %d mV\n", id, *volt);
	return ret;
}

static int bm_read_1331_current_out(struct bm_device_info *bmdi, int id, u32 *cur)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x15, &temp);
	*cur = temp;
	if (ret) {
		pr_err("bmdrv i2c read current failed!\n");
		return ret;
	}
	*cur = (*cur) * 125 / 2;
	PR_TRACE("bmdrv i2c id %x current %d mA\n", id, *cur);
	return ret;
}

static int bm_read_1331_temp(struct bm_device_info *bmdi, int id, u32 *temp)
{
	int ret = 0;
	u32 i2c_index = 0;
	u16 temp_16;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0, id);
	if (ret) {
		pr_err("bmdrv i2c set cmd failed!\n");
		return ret;
	}
	ret = bm_i2c_read_hword(bmdi, i2c_index, 0x29, &temp_16);
	*temp = temp_16;
	if (ret) {
		pr_err("bmdrv i2c read current failed!\n");
		return ret;
	}
	*temp = (*temp) / 8;
	PR_TRACE("bmdrv i2c id %x temp %d C\n", id, *temp);
	return ret;
}

int bm_read_sc5_pro_tpu_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	*volt = mcu_info_reg_read(bmdi, 0xc);
	return 0;
}

int bm_read_sc5_pro_tpu_current(struct bm_device_info *bmdi, u32 *cur)
{
	*cur = mcu_info_reg_read(bmdi, 0x10);
	return 0;
}

int bm_read_sc5_pro_tpu_power(struct bm_device_info *bmdi, u32 *power)
{
	*power = mcu_info_reg_read(bmdi, 0x14);
	*power = *power / 1000;
	return 0;
}

int bm_read_sc7_pro_vddc_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	*volt = mcu_info_reg_read(bmdi, 0x18);
	return 0;
}
int bm_read_sc7_pro_vddc_power(struct bm_device_info *bmdi, u32 *power)
{
	*power = mcu_info_reg_read(bmdi, 0x20);
	*power = *power / 1000;
	return 0;
}

int bm_read_sc7_pro_vddphy_power(struct bm_device_info *bmdi, u32 *power)
{
	*power = mcu_info_reg_read(bmdi, 0x2c);
	*power = *power / 1000;
	return 0;
}

int bm_set_vdd_tpu_voltage(struct bm_device_info *bmdi, u32 volt)
{
	char tpu_volt[10];
	struct console_ctx console;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		return bm_set_68224_voltage_out(bmdi, 0x0, volt);
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		   (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if (bmdrv_sc5pro_uart_is_connect_mcu(bmdi) != 0x1)
			return 0;
		if ((volt <= 800) && (volt >= 550)) {
			sprintf(tpu_volt, "%d", volt);
			console.uart.bmdi = bmdi;
			console.uart.uart_index = 0x2;
			console_cmd_sc7p_set_tpu_vol(&console, tpu_volt);
		}
	}

	return 0;
}

int bm_read_vdd_tpu_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_voltage_out(bmdi, 0x60, volt);
	else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		return  bm_read_sc5_pro_tpu_voltage(bmdi, volt);
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5M_P) &&
		 (BM1684_HW_VERSION(bmdi) == 0x30 || BM1684_HW_VERSION(bmdi) == 0x31)) {
		*volt = ATTR_NOTSUPPORTED_VALUE;
		return 0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		return bm_read_68224_voltage_out(bmdi, 0x0, volt);
	} else {
		return bm_read_68127_voltage_out(bmdi, 0x0, volt);
	}
}

int bm_read_vdd_tpu_current(struct bm_device_info *bmdi, u32 *cur)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_current_out(bmdi, 0x60, cur);
	else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		return  bm_read_sc5_pro_tpu_current(bmdi, cur);
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5M_P) &&
		 (BM1684_HW_VERSION(bmdi) == 0x30 || BM1684_HW_VERSION(bmdi) == 0x31)) {
		*cur = ATTR_NOTSUPPORTED_VALUE;
		return 0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		return bm_read_68224_current_out(bmdi, 0x0, cur);
	} else {
		return bm_read_68127_current_out(bmdi, 0x0, cur);
	}
}

int bm_read_vdd_tpu_mem_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_voltage_out(bmdi, 0x62, volt);
	else
		return 0;
}

int bm_read_vdd_tpu_mem_current(struct bm_device_info *bmdi, u32 *cur)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_current_out(bmdi, 0x62, cur);
	else
		return 0;
}

int bm_set_vddc_voltage(struct bm_device_info *bmdi, u32 volt)
{
	char vddc_volt[10];
	struct console_ctx console;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		return bm_set_68224_voltage_out(bmdi, 0x1, volt);
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
	(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if(bmdrv_sc5pro_uart_is_connect_mcu(bmdi) != 0x1) {
			return 0;
		}
		if ((volt <= 980) && (volt >= 800)) {
			sprintf(vddc_volt, "%d", volt);
			console.uart.bmdi = bmdi;
			console.uart.uart_index = 0x2;
			console_cmd_sc7p_set_vddc_vol(&console, vddc_volt);
		}
	}

	return 0;
}

int bm_read_vddc_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_voltage_out(bmdi, 0x61, volt);
	else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		return bm_read_sc7_pro_vddc_voltage(bmdi, volt);
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5M_P) &&
		 (BM1684_HW_VERSION(bmdi) == 0x30 || BM1684_HW_VERSION(bmdi) == 0x31)) {
		*volt = ATTR_NOTSUPPORTED_VALUE;
		return 0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		return bm_read_68224_voltage_out(bmdi, 0x1, volt);
	} else {
		return bm_read_68127_voltage_out(bmdi, 0x1, volt);
	}
}

int bm_read_vddc_current(struct bm_device_info *bmdi, u32 *cur)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_current_out(bmdi, 0x61, cur);
	else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		return 0;
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5M_P) &&
		 (BM1684_HW_VERSION(bmdi) == 0x30 || BM1684_HW_VERSION(bmdi) == 0x31)) {
		*cur = ATTR_NOTSUPPORTED_VALUE;
		return 0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		return bm_read_68224_current_out(bmdi, 0x1, cur);
	} else {
		return bm_read_68127_current_out(bmdi, 0x1, cur);
	}
}

int bm_read_vdd_tpu_power(struct bm_device_info *bmdi, u32 *tpu_power)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_power(bmdi, 0x60, tpu_power);
	else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			 (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		return  bm_read_sc5_pro_tpu_power(bmdi, tpu_power);
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5M_P) &&
		 (BM1684_HW_VERSION(bmdi) == 0x30 || BM1684_HW_VERSION(bmdi) == 0x31)) {
		//bm_read_is6808_test(bmdi, tpu_power);
		//pr_err("%s : 1, tpu_power = 0x%x\n", __func__, *tpu_power);
		*tpu_power = ATTR_NOTSUPPORTED_VALUE;
		return 0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB) {
		return bm_read_68224_power(bmdi, 0x0, tpu_power);
	} else {
		return  bm_read_68127_power(bmdi, 0x0, tpu_power);
	}
}

int bm_read_vdd_tpu_mem_power(struct bm_device_info *bmdi, u32 *tpu_mem_power)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_power(bmdi, 0x62, tpu_mem_power);
	else
		return  0;
}

int bm_read_vddc_power(struct bm_device_info *bmdi, u32 *vddc_power)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_power(bmdi, 0x61, vddc_power);
	else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_BM1684X_EVB){
		return bm_read_68224_power(bmdi, 0x1, vddc_power);
	} else if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			   (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		return bm_read_sc7_pro_vddc_power(bmdi, vddc_power);
	} else
		return 0;
}

int bm_read_vddphy_power(struct bm_device_info *bmdi, u32 *vddphy_power)
{
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		return bm_read_sc7_pro_vddphy_power(bmdi, vddphy_power);
	} else
		return 0;
}

int bm_read_vdd_pmu_tpu_temp(struct bm_device_info *bmdi, u32 *pmu_tpu_temp)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_temp(bmdi, 0x60, pmu_tpu_temp);
	else
		return  0;
}

int bm_read_vdd_pmu_tpu_mem_temp(struct bm_device_info *bmdi, u32 *pmu_tpu_mem_temp)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_temp(bmdi, 0x62, pmu_tpu_mem_temp);
	else
		return  0;
}

int bm_read_vddc_pmu_vddc_temp(struct bm_device_info *bmdi, u32 *pmu_vddc_temp)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_temp(bmdi, 0x61, pmu_vddc_temp);
	else
		return 0;
}

int bm_read_mcu_current(struct bm_device_info *bmdi, u8 lo, u32 *cur)
{
	int ret = 0;
	u8 data = 0;
	u32 result = 0;

	ret = bm_mcu_read_reg(bmdi, lo, &data);
	if (ret)
		return ret;
	result = (u32)data;

	ret = bm_mcu_read_reg(bmdi, lo + 1, &data);
	if (ret)
		return ret;

	result |= (u32)data << 8;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_EVB ||
		BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
		if (BM1684_HW_VERSION(bmdi) < 4) {
			switch (lo) {
			case 0x28: // 12v atx
				result = (int)result * 2 * 1000 * 10 / 3 / 4096;
				break;
			case 0x2a: // vddio5
				result = (int)result * 2 * 1000 * 10 / 6 / 4096;
				break;
			case 0x2c: // vddio18
				result = (int)result * 2 * 1000 * 10 / 6 / 4096;
				break;
			case 0x2e: // vddio33
				result = (int)result * 2 * 1000 / 3 / 4096;
				break;
			case 0x30: // vdd_phy
				result = (int)result * 2 * 1000 * 10 / 8 / 4096;
				break;
			case 0x32: // vdd_pcie
				result = (int)result * 2 * 1000 * 10 / 6 / 4096;
				break;
			case 0x34: // vdd_tpu_mem
				result = (int)result * 2 * 1000 * 10 / 3 / 4096;
				break;
			case 0x36: // ddr_vddq
				result = (int)result * 2 * 1000 * 10 / 8 / 4096;
				break;
			case 0x38: // ddr_vddqlp
				result = (int)result * 2 * 1000 * 10 / 5 / 4096;
				break;
			default:
				break;
			}
		} else {
			switch (lo) {
			case 0x28:
				result = (int)result * 3 * 1000 / 4096;
				break;
			case 0x2a:
				result = (int)result * 6 * 1000 / 4096;
				break;
			case 0x2c:
				result = (int)result * 18 * 1000 / 15 / 4096;
				break;
			case 0x2e:
				result = (int)result * 18 * 1000 / 30 / 4096;
				break;
			case 0x30:
				result = (int)result * 18 * 1000 / 15 / 4096;
				break;
			case 0x32:
				result = (int)result * 18 * 1000 / 15 / 4096;
				break;
			case 0x34:
				result = (int)result * 6 * 1000 / 4096;
				break;
			case 0x36:
				result = (int)result * 18 * 1000 / 8 / 4096;
				break;
			case 0x38:
				result = (int)result * 18 * 1000 / 30 / 4096;
				break;
			default:
				break;
			}
		}
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
		switch (lo) {
		case 0x28: /* 12v atx */
			result = (int)result * 1000 * 12 / 4096;
			break;
		case 0x2e: /* vddio33 */
			result = (int)result * 72 * 100 / 4096;
			break;
		default:
			break;
		}
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		switch (lo) {
		case 0x28: /* 12v atx */
			result = (int)result * 1000 * 6 / 4096;
			break;
		default:
			break;
		}
	}

	//BM1684X_EVB using the result directly
	*cur = result;
	return ret;
}

int bm_read_mcu_voltage(struct bm_device_info *bmdi, u8 lo, u32 *volt)
{
	int ret = 0;
	u8 data = 0;
	u32 result = 0;

	switch (lo) {
	case 0x26: /* 12v atx */
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
			ret = bm_mcu_read_reg(bmdi, lo, &data);
			if (ret)
				return ret;
			result = (u32)data;

			ret = bm_mcu_read_reg(bmdi, lo + 1, &data);
			if (ret)
				return ret;
			result |= (u32)data << 8;

			result = (int)result * 18 * 11 * 1000 / (4096 * 10);
		} else {
			result = 12 * 1000;
		}
		break;
	case 0x2a: /* vddio5 */
		result = 5 * 1000;
		break;
	case 0x2c: /* vddio18 */
		result = 18 * 100;
		break;
	case 0x2e: /* vddio33 */
		result = 33 * 100;
		break;
	case 0x30: /* vdd_phy */
		result = 8 * 100;
		break;
	case 0x32: /* vdd_pcie */
		result = 8 * 100;
		break;
	case 0x34: /* vdd_tpu_mem */
		result = 7 * 100;
		break;
	case 0x36: /* ddr_vddq */
		result = 11 * 100;
		break;
	case 0x38: /* ddr_vddqlp */
		result = 6 * 100;
		break;
	default:
		result = 0;
		break;
	}
	*volt = result;
	return 0;
}

int bm_read_board_current(struct bm_device_info *bmdi, u32 *cur)
{

	int ret = 0;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		*cur = mcu_info_reg_read(bmdi, 0x8);
	} else {
		ret = bm_read_mcu_current(bmdi, 0x28, cur);
		if (ret) {
			pr_err("read mcu current failed!\n");
			return ret;
		}
	}
	return ret;
}

int bm_read_sc5_power(struct bm_device_info *bmdi, u32 *power)
{
	int ret;
	u32 volt, curr;

	/* read 12V atx value */
	ret = bm_read_mcu_voltage(bmdi, 0x26, &volt);
	if (ret) {
		pr_err("read mcu voltage failed!\n");
		return ret;
	}
	ret = bm_read_mcu_current(bmdi, 0x28, &curr);
	if (ret) {
		pr_err("read mcu current failed!\n");
		return ret;
	}
	*power = volt * curr / 1000 / 1000;
	return 0;
}

int bm_read_1684x_evb_power(struct bm_device_info *bmdi, u32 *power)
{
	int ret;
	u32 volt, curr;

	/* read 12V atx value */
	ret = bm_read_68224_12v_voltage_in(bmdi, &volt);
	if (ret) {
		pr_err("read mcu voltage failed!\n");
		return ret;
	}
	ret = bm_read_mcu_current(bmdi, 0x28, &curr);
	if (ret) {
		pr_err("read mcu current failed!\n");
		return ret;
	}
	*power = volt * curr / 1000 / 1000 - 4;
	return 0;
}

int bm_read_sc5_plus_power(struct bm_device_info *bmdi, u32 *power)
{
	int ret;
	u32 volt, curr;

	/* read 12V atx value */
	ret = bm_read_mcu_voltage(bmdi, 0x26, &volt);
	if (ret) {
		pr_err("read mcu voltage failed!\n");
		return ret;
	}
	ret = bm_read_mcu_current(bmdi, 0x28, &curr);
	if (ret) {
		pr_err("read mcu current failed!\n");
		return ret;
	}
	*power = volt * curr;

	volt = 0;
	curr = 0;
	/* read 3.3v value */
	ret = bm_read_mcu_voltage(bmdi, 0x2e, &volt);
	if (ret) {
		pr_err("read mcu voltage failed!\n");
		return ret;
	}
	ret = bm_read_mcu_current(bmdi, 0x2e, &curr);
	if (ret) {
		pr_err("read mcu current failed!\n");
		return ret;
	}
	*power += volt * curr;
	*power /= 1000 * 1000;
	return 0;
}

int bm_read_sc5_pro_power(struct bm_device_info *bmdi, u32 *power)
{
	int ret = 0;
	u32 cur = 0;

	cur = mcu_info_reg_read(bmdi, 0x8);

	*power = 12 * cur / 1000;

	return ret;
}

static int bm_read_mcu_temp(struct bm_device_info *bmdi, int id, int *temp)
{
	int ret;
	u8 data;

	ret = bm_mcu_read_reg(bmdi, id + 4, &data);
	if (ret)
		return ret;
	*temp = (int) (*(signed char *)(&data));
	return ret;
}

int bm_read_mcu_chip_temp(struct bm_device_info *bmdi, int *temp)
{
	return bm_read_mcu_temp(bmdi, 0, temp);
}

int bm_read_mcu_board_temp(struct bm_device_info *bmdi, int *temp)
{
	return bm_read_mcu_temp(bmdi, 1, temp);
}

static int bm_eeprom_write_unlock(struct bm_device_info *bmdi)
{
	int ret = 0x0;
	int i2c_index = 0x1;

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x17);

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0x60, 0x43);
	if (ret < 0)
		return -1;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0x60, 0x4b);
	if (ret < 0)
		return -1;

	return ret;
}

static int bm_eeprom_write_lock(struct bm_device_info *bmdi)
{
	int ret = 0x0;
	int i2c_index = 0x1;

	bm_i2c_set_target_addr(bmdi, i2c_index, 0x17);

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0x60, 0x4c);
	if (ret < 0)
		return -1;

	ret = bm_i2c_write_byte(bmdi, i2c_index, 0x60, 0x4f);
	if (ret < 0)
		return -1;

	return ret;
}

static int bm_set_eeprom(struct bm_device_info *bmdi, u8 offset, char *data, int size)
{
	int ret, i;

	for (i = 0; i < size; i++) {
		ret = bm_set_eeprom_reg(bmdi, offset + i, data[i]);
		if (ret)
			return ret;
		udelay(400); /* delay large enough is needed */
	}
	return 0;
}

static int bm_get_eeprom(struct bm_device_info *bmdi, u8 offset, char *data, int size)
{
	int ret, i;

	for (i = 0; i < size; i++) {
		ret = bm_get_eeprom_reg(bmdi, offset + i, data + i);
		if (ret)
			return ret;
	}
	return 0;
}

int bmdrv_sc5pro_uart_is_connect_mcu(struct bm_device_info *bmdi)
{
	int value = 0;

	if ((BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC5_PRO) &&
		(BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO) &&
		(BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PLUS))
		return 1;

	value = gpio_reg_read(bmdi, 0x50);
	value = value >> 0x5;
	value &= 0xf;

	if (value == 0x0)
		return 1;
	else
		return 0;
}

static int bm_set_sn(struct bm_device_info *bmdi, char *sn)
{
	int ret = 0x0;
	struct console_ctx console;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if (bmdrv_sc5pro_uart_is_connect_mcu(bmdi) != 0x1)
			return 0;
		console.uart.bmdi = bmdi;
		console.uart.uart_index = 0x2;
		console_cmd_set_sn(&console, sn);
		return 0;
	}

	ret = bm_eeprom_write_unlock(bmdi);
	if (ret < 0)
		return ret;

	ret = bm_set_eeprom(bmdi, 0, sn, 17);
	if (ret < 0)
		return ret;

	ret = bm_eeprom_write_lock(bmdi);
	if (ret < 0)
		return ret;

	return ret;
}

int bm_get_sn(struct bm_device_info *bmdi, char *sn)
{

	struct console_ctx console;
	unsigned char temp_sn[64] = {0};

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if (bmdrv_sc5pro_uart_is_connect_mcu(bmdi) != 0x1)
			return -1;

		console.uart.bmdi = bmdi;
		console.uart.uart_index = 0x2;
		console_cmd_get_sn(&console, temp_sn);
		memcpy(sn, temp_sn, 17);
		return 0;
	}


	return bm_get_eeprom(bmdi, 0, sn, 17);
}

int bm_get_device_sn(struct bm_device_info *bmdi, unsigned long arg)
{
	int ret = 0;

#ifndef SOC_MODE
	ret = copy_to_user((char __user *)arg, bmdi->bmcd->sn, sizeof(bmdi->bmcd->sn));
#else
	ret = copy_to_user((char __user *)arg, "N/A", sizeof("N/A"));
#endif
	return ret;
}

int bm_get_correct_num(struct bm_device_info *bmdi, unsigned long arg)
{
	int ret = 0;

#ifdef SOC_MODE
	ret = put_user(ATTR_NOTSUPPORTED_VALUE,(u32 __user *)arg);
#else
	struct chip_info *cinfo = &bmdi->cinfo;

	if (bmdi->misc_info.ddr_ecc_enable == 0) {
		ret = put_user(ATTR_NOTSUPPORTED_VALUE,(u32 __user *)arg);
	} else {
		ret = put_user(cinfo->ddr_ecc_correctN,(u32 __user *)arg);
	}
#endif
	return ret;
}

int bm_get_12v_atx(struct bm_device_info *bmdi, unsigned long arg)
{
	int ret = 0;

#ifndef SOC_MODE
	struct bm_chip_attr *c_attr=&bmdi->c_attr;
	struct bm_device_info *c_bmdi;

	if (bmdi->bmcd != NULL)
	{
		if (bmdi->bmcd->card_bmdi[0] != NULL)
		{
			c_bmdi = bmdi->bmcd->card_bmdi[0];
			c_attr = &c_bmdi->c_attr;
		}
	}
	ret = put_user(c_attr->atx12v_curr,(u32 __user *)arg);
#else
	ret = put_user(ATTR_NOTSUPPORTED_VALUE,(u32 __user *)arg);
#endif
	return ret;
}

int bm_burning_info_sn(struct bm_device_info *bmdi, unsigned long arg)
{
	char sn[18];
	char sn_zero[18] = {0};
	int ret;
	struct bm_chip_attr *c_attr;
	struct bm_device_info *tmp_bmdi = bmdi;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if ((bmdi->bmcd->sc5p_mcu_bmdi) != NULL && (bmdi->bmcd != NULL))
			tmp_bmdi = bmdi->bmcd->sc5p_mcu_bmdi;
		else {
			pr_err("set sn get sc5p_mcu_bmdi fail\n");
		}
	}

	c_attr = &tmp_bmdi->c_attr;

	ret = copy_from_user(sn, (char __user *)arg, sizeof(sn));
	if (ret) {
		pr_err("copy SN from user failed!\n");
		return ret;
	}

	mutex_lock(&c_attr->attr_mutex);
	if (strncmp(sn, sn_zero, 17)) {
		/* set SN */
		ret = bm_set_sn(tmp_bmdi, sn);
		if (ret) {
			pr_err("bmdrv: set SN failed\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
		bm_get_sn(tmp_bmdi, bmdi->bmcd->sn);
	} else {
		/* display SN */
		ret = bm_get_sn(tmp_bmdi, sn);
		if (ret) {
			pr_err("bmdrv: get SN failed\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
		ret = copy_to_user((char __user *)arg, sn, sizeof(sn));
	}
	mutex_unlock(&c_attr->attr_mutex);
	return ret;
}

static int bm_set_mac(struct bm_device_info *bmdi, int id, unsigned char *mac)
{
	int ret = 0x0;

	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		pr_err("bmsophon%d, sc5p sc7p not support set mac\n", bmdi->dev_index);
		return -ENOSYS;
	}
	ret = bm_eeprom_write_unlock(bmdi);
	if (ret < 0)
		return ret;

	bm_set_eeprom(bmdi, 0x20 + 0x20 * id, mac, 6);
	if (ret < 0)
		return ret;

	ret = bm_eeprom_write_lock(bmdi);
	if (ret < 0)
		return ret;

	return ret;
}

static int bm_get_mac(struct bm_device_info *bmdi, int id, unsigned char *mac)
{
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		pr_err("bmsophon%d, sc5p sc7p not support get mac\n", bmdi->dev_index);
		return -ENOSYS;
	}
	return bm_get_eeprom(bmdi, 0x20 + 0x20 * id, mac, 6);
}

int bm_burning_info_mac(struct bm_device_info *bmdi, int id, unsigned long arg)
{
	unsigned char mac_bytes[6];
	unsigned char mac_bytes_zero[6] = {0};
	int ret;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	ret = copy_from_user(mac_bytes, (unsigned char __user *)arg, sizeof(mac_bytes));
	if (ret) {
		pr_err("copy MAC from user failed!\n");
		return ret;
	}
	mutex_lock(&c_attr->attr_mutex);
	if (memcmp(mac_bytes, mac_bytes_zero, sizeof(mac_bytes))) {
		/* set MAC */
		ret = bm_set_mac(bmdi, id, mac_bytes);
		if (ret) {
			pr_err("bmdrv: set MAC %d failed!n", id);
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
	} else {
		/* get MAC */
		ret = bm_get_mac(bmdi, id, mac_bytes);
		if (ret) {
			pr_err("bmdrv: get MAC %d failed!n", id);
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
		ret = copy_to_user((unsigned char __user *)arg, mac_bytes, sizeof(mac_bytes));
	}
	mutex_unlock(&c_attr->attr_mutex);
	return ret;
}

static int bm_set_board_type(struct bm_device_info *bmdi, char b_type)
{
	int ret = 0x0;
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		pr_err("bmsophon%d, sc5p sc7p not support set board type\n", bmdi->dev_index);
		return -ENOSYS;
	}

	ret = bm_eeprom_write_unlock(bmdi);
	if (ret < 0)
		return ret;

	ret = bm_set_eeprom(bmdi, 0x60, &b_type, 1);
	if (ret < 0)
		return ret;

	ret = bm_eeprom_write_lock(bmdi);
	if (ret < 0)
		return ret;

	return ret;
}

int bm_get_board_type(struct bm_device_info *bmdi, char *b_type)
{
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		pr_err("bmsophon%d, sc5p sc7p not support get board type\n", bmdi->dev_index);
		return -ENOSYS;
	}
	return bm_get_eeprom(bmdi, 0x60, b_type, 1);
}

int bm_burning_info_board_type(struct bm_device_info *bmdi, unsigned long arg)
{
	char b_byte;
	int ret;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	ret = get_user(b_byte, (char __user *)arg);
	if (ret) {
		pr_err("copy board type from user failed!\n");
		return ret;
	}
	mutex_lock(&c_attr->attr_mutex);
	if (b_byte != -1) {
		/* set board byte */
		ret = bm_set_board_type(bmdi, b_byte);
		if (ret) {
			pr_err("bmdrv: set board type failed!\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
	} else {
		/* get board type */
		ret = bm_get_board_type(bmdi, &b_byte);
		if (ret) {
			pr_err("bmdrv: get board type failed!\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
		ret = put_user(b_byte, (char __user *)arg);
	}
	mutex_unlock(&c_attr->attr_mutex);
	return ret;
}
#endif

void bmdrv_fetch_attr(struct bm_device_info *bmdi, int count, int is_setspeed)
{

#ifndef SOC_MODE
	int rc = 0;
	struct chip_info *cinfo = &bmdi->cinfo;
#endif
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	long start=0;
	long end =0;
	int delt = 0;
	start = jiffies;
	bm_npu_utilization_stat(bmdi);
#ifndef SOC_MODE
	bm_vpu_check_usage_info(bmdi);
	bm_jpu_check_usage_info(bmdi);
	bm_vpp_check_usage_info(bmdi);
#endif

	if(count == 17){
		mutex_lock(&c_attr->attr_mutex);
		c_attr->tpu_current_clock = bmdrv_1684_clk_get_tpu_freq(bmdi);
		mutex_unlock(&c_attr->attr_mutex);
#ifndef SOC_MODE
		mutex_lock(&c_attr->attr_mutex);
		if(c_attr->bm_get_board_temp != NULL){
			c_attr->bm_get_board_temp(bmdi, &c_attr->board_temp);
			dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.board_temp_reg, c_attr->board_temp, sizeof(u8));//bm_smbus_update_dev_info
		}

		if(c_attr->bm_get_tpu_power != NULL){
			bm_read_vdd_tpu_voltage(bmdi, &c_attr->vdd_tpu_volt);
			if ((c_attr->vdd_tpu_volt > 0) && (c_attr->vdd_tpu_volt < 0xffff))
				c_attr->last_valid_tpu_volt = c_attr->vdd_tpu_volt;
			else if (c_attr->vdd_tpu_volt != ATTR_NOTSUPPORTED_VALUE) {
				c_attr->vdd_tpu_volt = c_attr->last_valid_tpu_volt;
			}

			bm_read_vdd_tpu_current(bmdi, &c_attr->vdd_tpu_curr);
			if ((c_attr->vdd_tpu_curr > 0) && (c_attr->vdd_tpu_curr < 0xffff))
				c_attr->last_valid_tpu_curr = c_attr->vdd_tpu_curr;
			else if (c_attr->vdd_tpu_curr != ATTR_NOTSUPPORTED_VALUE) {
				c_attr->vdd_tpu_curr = c_attr->last_valid_tpu_curr;
			}

			c_attr->bm_get_tpu_power(bmdi, &c_attr->tpu_power);
			if ((c_attr->tpu_power > 0) && (c_attr->tpu_power < 0xffff))
				c_attr->last_valid_tpu_power = c_attr->tpu_power;
			else if (c_attr->tpu_power != ATTR_NOTSUPPORTED_VALUE) {
				c_attr->tpu_power = c_attr->last_valid_tpu_power;
			}
		}

		if(c_attr->bm_get_vddc_power != NULL){
			bm_read_vddc_power(bmdi, &c_attr->vddc_power);
			if ((c_attr->vddc_power > 0) && (c_attr->vddc_power < 0xffff))
				c_attr->last_valid_vddc_power = c_attr->vddc_power;
			else if (c_attr->vddc_power != ATTR_NOTSUPPORTED_VALUE) {
				c_attr->vddc_power = c_attr->last_valid_vddc_power;
			}
		}

		if(c_attr->bm_get_vddphy_power != NULL){
			bm_read_vddphy_power(bmdi, &c_attr->vddphy_power);
			if ((c_attr->vddphy_power > 0) && (c_attr->vddphy_power < 0xffff))
				c_attr->last_valid_vddphy_power = c_attr->vddphy_power;
			else if (c_attr->vddphy_power != ATTR_NOTSUPPORTED_VALUE) {
				c_attr->vddphy_power = c_attr->last_valid_vddphy_power;
			}
		}

		if(bmdi->boot_info.fan_exist && c_attr->bm_get_fan_speed != NULL){
			dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.fan_speed_reg, c_attr->fan_speed, sizeof(u8));//bm_smbus_update_dev_info
		}

		if (!bmdi->boot_info.temp_sensor_exist){
			mutex_unlock(&c_attr->attr_mutex);
			goto err_fetch;
		}
		/* get chip temperature */
		if(c_attr->bm_get_chip_temp != NULL)
			rc = c_attr->bm_get_chip_temp(bmdi, &c_attr->chip_temp);
		if (rc) {
			dev_err(cinfo->device, "device chip temperature fetch failed %d\n", rc);
			mutex_unlock(&c_attr->attr_mutex);
			goto err_fetch;
		}
		else
			dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.chip_temp_reg, c_attr->chip_temp, sizeof(u8));//bm_smbus_update_dev_info
		mutex_unlock(&c_attr->attr_mutex);

		if (c_attr->fan_control && is_setspeed == 1)
			bmdrv_adjust_fan_speed(bmdi, c_attr->chip_temp);

		mutex_lock(&bmdi->clk_reset_mutex);
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) {
			bmdrv_thermal_update_status_sc7p(bmdi, c_attr->chip_temp);
		} else {
			bmdrv_thermal_update_status(bmdi, c_attr->chip_temp);
		}
		mutex_unlock(&bmdi->clk_reset_mutex);
err_fetch:
#else
		if (bmdi->status_sync_api != 0)
			calculate_board_status(bmdi); // update soc mode
#endif
		end  = jiffies;
		delt = jiffies_to_msecs(end-start);
		//PR_TRACE("dev_index=%d,bm_smbus_update_dev_info time is %d ms\n",bmdi->dev_index, delt);
		msleep_interruptible(((10-delt)>0)? 10-delt : 0);
	}
	else
		msleep_interruptible(10);

}

void bmdrv_fetch_attr_board_power(struct bm_device_info *bmdi, int count)
{
	long start=0;
	long end =0;
	int delt = 0;
	int threshold = 0;
#ifndef SOC_MODE
	struct bm_freq_scaling_db * p_data = NULL;
#endif

	start = jiffies;
	bm_npu_utilization_stat(bmdi);
#ifndef SOC_MODE
	if ((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
		(BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) {
		if (bmdi->bmcd == NULL)
			return;
		if (bmdi->dev_index == bmdi->bmcd->dev_start_index) {
			p_data = bmdi->bmcd->vfs_db;
			if ((p_data == NULL) || (p_data->start_flag == VFS_ORIGIN_MODE))
				return;
			threshold = 3;
			count = p_data->board_pwr_count;
		} else {
			threshold = 17;
		}
	} else
#endif
		threshold = 17;

	if(count == threshold){
#ifndef SOC_MODE
		struct bm_chip_attr *c_attr = &bmdi->c_attr;
		mutex_lock(&c_attr->attr_mutex);
		if(c_attr->bm_get_board_power!= NULL){
			c_attr->bm_get_board_power(bmdi, &c_attr->board_power);
			bm_read_board_current(bmdi, &c_attr->atx12v_curr);
			if (bmdi->bmcd != NULL) {
				bmdi->bmcd->board_power = c_attr->board_power;
				bmdi->bmcd->atx12v_curr = c_attr->atx12v_curr;
			}
			dev_info_reg_write(bmdi, bmdi->cinfo.dev_info.board_power_reg, c_attr->board_power, sizeof(u8));//bm_smbus_update_dev_info
		}
		mutex_unlock(&c_attr->attr_mutex);

		if (bmdi->bmcd != NULL) {
			if (((BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) ||
			     (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS)) &&
			     (bmdi->dev_index == bmdi->bmcd->dev_start_index)) {
				bmdrv_record_board_power(bmdi, c_attr->board_power);
			}
		}
#endif
		end  = jiffies;
		delt = jiffies_to_msecs(end-start);//delt is about 4ms
		//PR_TRACE("dev_index=%d,bm_get_board_power time is %d ms\n",bmdi->dev_index,delt);
		msleep_interruptible(((10-delt)>0)? 10-delt : 0);
	}
	else
		msleep_interruptible(10);
}

int bm_get_name(struct bm_device_info *bmdi, unsigned long arg) {

	int ret = 0;
#ifndef SOC_MODE
	char type[10];
	char name[20];
	int board_id = BM1684_BOARD_TYPE(bmdi);

	if(bmdi->cinfo.chip_id != 0x1682){
	  bm1684_get_board_type_by_id(bmdi, type, board_id);
	  snprintf(name,20,"%x-%s",bmdi->cinfo.chip_id ,type);
          ret = copy_to_user((unsigned char __user *)arg, name, sizeof(name));
        }
        else
          ret = put_user(ATTR_NOTSUPPORTED_VALUE, (u32 __user *)arg);
#else
	ret = copy_to_user((unsigned char __user *)arg, "soc", sizeof("soc"));
#endif
	return ret;
}

#ifndef SOC_MODE
int bmdrv_find_first_chip_logic_chip_id(struct bm_device_info *bmdi)
{
	u32 chip_num = 0;
	u32 value = 0;
	struct bm_device_info *chip_bmdi = NULL;

	if ((BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO) &&
		(BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PLUS))
		return bmdi->dev_index;

	for (chip_num = 0; chip_num < bmdi->bmcd->chip_num; chip_num++) {
			chip_bmdi = bmdi->bmcd->card_bmdi[chip_num];
			value = gpio_reg_read(chip_bmdi, 0x50);
			value = value >> 0x5;
			value &= 0xf;
			if (value == 0) {
				return chip_bmdi->dev_index;
			}
	}

	return -1;
}

struct bm_freq_scaling_db * bmdrv_alloc_vfs_database(struct bm_device_info *bmdi)
{
	struct bm_freq_scaling_db * p_vfs_db = NULL;

	if ((BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO) &&
		(BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PLUS))
		return NULL;

	p_vfs_db = kzalloc(sizeof(struct bm_freq_scaling_db), GFP_KERNEL);
	if (p_vfs_db == NULL) {
		pr_err("bm vfs alloc fail\n");
		return NULL;
	}

	memset((void *)p_vfs_db, 0x00, sizeof(struct bm_freq_scaling_db));
	return p_vfs_db;
}

void bmdrv_init_freq_scaling_status(struct bm_device_info *bmdi)
{
	int chip_num = 0;
	struct bm_freq_scaling_db * p_data = NULL;
	struct bm_vfs_pair freq_volt_pair_sc7_pro[VFS_MAX_LEVEL_SC7_PRO] = {
		{1000, 780},
		{950, 760},
		{900, 740},
		{850, 720},
		{800, 700},
		{750, 680},
		{700, 660},
		{650, 640},
		{600, 620},
		{550, 600},
		{500, 600},
		{450, 600},
		{400, 600},
		{350, 600},
		{300, 600},
		{250, 600},
		{200, 600},
		{150, 600},
		{100, 600},
		{25, 600}
	};
	struct bm_vfs_pair freq_volt_pair_sc7_plus[VFS_MAX_LEVEL_SC7_PLUS] = {
		{1000, 800},
		{950, 800},
		{900, 780},
		{850, 760},
		{800, 740},
		{750, 720},
		{700, 700},
		{650, 680},
		{600, 660},
		{550, 640},
		{500, 620},
		{450, 600},
		{400, 600},
		{350, 600},
		{300, 600},
		{250, 600},
		{200, 600},
		{150, 600},
		{100, 600},
		{25, 600}
	};

	p_data = bmdi->bmcd->vfs_db;
	if (p_data == NULL)
		return;
	p_data->chip0_index = bmdrv_find_first_chip_logic_chip_id(bmdi);
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) {
		p_data->vf_init_level = VFS_INIT_LEVEL_SC7_PRO;
		p_data->vf_relbl_level = VFS_RELBL_LEVEL_SC7_PRO;
		p_data->vfs_max_level = VFS_MAX_LEVEL_SC7_PRO;
	} else {
		p_data->vf_init_level = VFS_INIT_LEVEL_SC7_PLUS;
		p_data->vf_relbl_level = VFS_RELBL_LEVEL_SC7_PLUS;
		p_data->vfs_max_level = VFS_MAX_LEVEL_SC7_PLUS;
	}
	p_data->vf_level = p_data->vf_init_level;
	p_data->start_flag = VFS_ORIGIN_MODE;
	for (chip_num = 0; chip_num < BM_MAX_CHIP_NUM_PER_CARD; chip_num++) {
		if (p_data->thermal_freq[chip_num] == 0) {
			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) {
				p_data->thermal_freq[chip_num] = freq_volt_pair_sc7_pro[p_data->vf_level].freq;
			} else {
				p_data->thermal_freq[chip_num] = freq_volt_pair_sc7_plus[p_data->vf_level].freq;
			}
		}
	}
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) {
		memcpy((void *)&p_data->freq_volt_pair[0], (void *)&freq_volt_pair_sc7_pro[0], (sizeof(struct bm_vfs_pair) * VFS_MAX_LEVEL_SC7_PRO));
	} else {
		memcpy((void *)&p_data->freq_volt_pair[0], (void *)&freq_volt_pair_sc7_plus[0], (sizeof(struct bm_vfs_pair) * VFS_MAX_LEVEL_SC7_PLUS));
	}
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) {
		p_data->power_peak_threshold = 180;
		p_data->power_upper_threshold = 150;
		p_data->power_lower_threshold = 140;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS) {
		//p_data->power_peak_threshold = 100;
		p_data->power_peak_threshold = 85;
		p_data->power_upper_threshold = 80;
		p_data->power_lower_threshold = 72;
	}
}

int bmdrv_record_board_power(struct bm_device_info *bmdi, u32 power)
{
	struct bm_freq_scaling_db * p_data = NULL;

	p_data = bmdi->bmcd->vfs_db;
	if (p_data == NULL)
		return -1;

	p_data->power_total -= p_data->board_power[p_data->power_index % VFS_PWR_MEAN_SAMPLE_SIZE];
	p_data->board_power[p_data->power_index % VFS_PWR_MEAN_SAMPLE_SIZE] = power;
	p_data->power_total += p_data->board_power[p_data->power_index % VFS_PWR_MEAN_SAMPLE_SIZE];
	p_data->power_average = p_data->power_total / VFS_PWR_MEAN_SAMPLE_SIZE;
	if (p_data->power_highest < power)
		p_data->power_highest = power;
	p_data->power_index++;
	if (p_data->power_index == 10000)
		p_data->power_index = 0;

	return 0;
}

int bmdrv_check_tpu_utils(struct bm_device_info *bmdi)
{
	int util = 0;
	u32 chip_num = 0;
	struct chip_info *cinfo = NULL;
	struct bm_device_info *chip_bmdi = NULL;

	for (chip_num = 0; chip_num < bmdi->bmcd->chip_num; chip_num++) {
		chip_bmdi = bmdi->bmcd->card_bmdi[chip_num];
		util = bm_read_npu_util(chip_bmdi);
		if ((util >= 0) && (util <= 100)){
			return util;
		} else {
			cinfo = &chip_bmdi->cinfo;
			dev_err(cinfo->device, "get npu util fail, 0x%x\n", util);
		}
	}

	return -1;
}

int bmdrv_set_sc7_pro_tpu_volt_freq(struct bm_device_info *bmdi, u32 volt, u32 freq, enum bm_freq_scaling_mode mode)
{
	u32 chip_num = 0;
	int ret = 0;
	struct bm_device_info *chip_bmdi = NULL;
	struct chip_info *cinfo = NULL;
	struct bm_freq_scaling_db * p_data = NULL;

	if ((bmdi->dev_index != bmdi->bmcd->dev_start_index) || (mode >= FREQ_SCAL_BUTT)) {
		return -1;
	}

	//1. set tpu freq
	if ((mode == FREQ_INIT_MODE) || (mode == FREQ_DOWN_MODE)) {
		for (chip_num = 0; chip_num < bmdi->bmcd->chip_num; chip_num++) {
			chip_bmdi = bmdi->bmcd->card_bmdi[chip_num];
			bmdrv_get_tpu_target_freq(chip_bmdi, FREQ_CALLER_VFS, &freq);
			ret = bmdrv_clk_set_tpu_target_freq(chip_bmdi, freq);
			if (ret != 0) {
				cinfo = &chip_bmdi->cinfo;
				dev_err(cinfo->device, "device chip tpu freq cfg fail, %d\n", ret);
			}
		}
	}

	if (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO) {
		//2. set tpu voltage
		p_data = bmdi->bmcd->vfs_db;
		if ((p_data == NULL) || (p_data->chip0_index == -1))
			return -1;
		chip_bmdi = bmdi->bmcd->card_bmdi[p_data->chip0_index - bmdi->bmcd->dev_start_index];
		ret = bm_set_vdd_tpu_voltage(chip_bmdi, volt);
		if (ret != 0) {
			cinfo = &chip_bmdi->cinfo;
			dev_err(cinfo->device, "device chip tpu volt cfg fail, %d\n", ret);
		}
	}

	//3. set tpu freq
	if (mode == FREQ_UP_MODE) {
		msleep(10);
		for (chip_num = 0; chip_num < bmdi->bmcd->chip_num; chip_num++) {
			chip_bmdi = bmdi->bmcd->card_bmdi[chip_num];
			bmdrv_get_tpu_target_freq(chip_bmdi, FREQ_CALLER_VFS, &freq);
			ret = bmdrv_clk_set_tpu_target_freq(chip_bmdi, freq);
			if (ret != 0) {
				cinfo = &chip_bmdi->cinfo;
				dev_err(cinfo->device, "device chip tpu freq cfg fail, %d\n", ret);
			}
		}
	}

	return 0;
}

int bmdrv_volt_freq_scaling_controller(struct bm_device_info *bmdi)
{
	u32 volt = 0;
	int freq = 0;
	u32 ret = 0;
	int utils = 0;
	struct chip_info *cinfo = &bmdi->cinfo;
	enum bm_freq_scaling_mode mode = FREQ_SCAL_BUTT;
	struct bm_freq_scaling_db * p_data = NULL;

	if ((BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO) && (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PLUS))
		return 0;

	if  (bmdi->dev_index != bmdi->bmcd->dev_start_index)
		return 0;

	p_data = bmdi->bmcd->vfs_db;
	if (p_data == NULL)
		return -1;
	//1. get current board power

	//2. decision
	if (p_data->power_average >= p_data->power_upper_threshold) {
		if (p_data->vf_level < (p_data->vfs_max_level -1))
			p_data->vf_level++;
		else
			return 0;
		mode = FREQ_DOWN_MODE;
		p_data->freq_up_count = 0;
	} else if (p_data->power_average <= p_data->power_lower_threshold) {
		p_data->freq_up_count++;
		if (p_data->freq_up_count < 60)
			return 0;
		p_data->freq_up_count = 0;
		utils = bmdrv_check_tpu_utils(bmdi);
		if (p_data->vf_level > 0) {
			if (utils == 0) {
				if (p_data->vf_level > p_data->vf_init_level)
					p_data->vf_level--;
				else
					return 0;
			} else {
					p_data->vf_level--;
			}
		} else
			return 0;
		mode = FREQ_UP_MODE;
	} else {
		p_data->freq_up_count = 0;
		return 0;
	}

	//3. exection
	if (p_data->vf_level >= p_data->vfs_max_level) {
		dev_err(cinfo->device, "sc7p vfs level error, level = %d\n", p_data->vf_level);
		return -1;
	}
	volt = p_data->freq_volt_pair[p_data->vf_level].volt;
	//pr_info("%s: level = %d, volt = %d, freq = %d, mode = %d\n", __func__, p_data->vf_level, volt, freq, mode);
	ret = bmdrv_set_sc7_pro_tpu_volt_freq(bmdi, volt, (u32)freq, mode);
	if (ret != 0) {
		dev_err(cinfo->device, "sc7p freq scalling fail, %d\n", ret);
		return ret;
	}

	return 0;
}

int bmdrv_volt_freq_scaling(struct bm_device_info *bmdi)
{
	u32 volt = 0;
	int freq = 0;
	int utils = 0;
	struct bm_freq_scaling_db * p_data = NULL;

	if ((BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PRO) && (BM1684_BOARD_TYPE(bmdi) != BOARD_TYPE_SC7_PLUS))
		return -1;

	if (bmdi->bmcd == NULL)
		return -1;

	if (bmdi->dev_index != bmdi->bmcd->dev_start_index)
		return -1;

	p_data = bmdi->bmcd->vfs_db;
	if (p_data == NULL)
		return -1;

	if (p_data->start_flag == VFS_MISSION_MODE) {
		p_data->freq_scal_count++;
		p_data->board_pwr_count++;
		if ((p_data->power_highest >= p_data->power_peak_threshold) && (p_data->vf_level < p_data->vf_relbl_level)) {  //peak clipping
			p_data->vf_level = p_data->vf_relbl_level;
			volt = p_data->freq_volt_pair[p_data->vf_level].volt;
			bmdrv_set_sc7_pro_tpu_volt_freq(bmdi, volt, (u32)freq, FREQ_INIT_MODE);
			p_data->freq_scal_count = 0;
		} else if (p_data->freq_scal_count > 30) { //normal
			p_data->freq_scal_count = 0;
			bmdrv_volt_freq_scaling_controller(bmdi);
		} else {
			utils = bmdrv_check_tpu_utils(bmdi); //idle down freq
			if (utils == 0) {
				p_data->idle_count++;
				if ((p_data->vf_level < p_data->vf_init_level) && (p_data->idle_count > 49)) {
					p_data->vf_level = p_data->vf_init_level;
					volt = p_data->freq_volt_pair[p_data->vf_level].volt;
					bmdrv_set_sc7_pro_tpu_volt_freq(bmdi, volt, (u32)freq, FREQ_INIT_MODE);
					p_data->freq_scal_count = 0;
					p_data->idle_count = 0;
				}
			} else {
				p_data->idle_count = 0;
			}
		}

		p_data->power_highest = 0;
		if (p_data->board_pwr_count == 4) {
			p_data->board_pwr_count = 0;
		}
		return 0;
	}

	if ((bmdi->bmcd->card_bmdi[bmdi->bmcd->chip_num - 1] != NULL) && (p_data->start_flag == VFS_ORIGIN_MODE)) {
		pr_info("vfs init, dev_index = %d\n", bmdi->dev_index);
		bmdrv_init_freq_scaling_status(bmdi);
		p_data->start_flag = VFS_INIT_MODE;
		if ((int)BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS) {
		 	p_data->start_flag = VFS_MISSION_MODE;
		}
		volt = p_data->freq_volt_pair[p_data->vf_level].volt;
		bmdrv_set_sc7_pro_tpu_volt_freq(bmdi, volt, (u32)freq, FREQ_INIT_MODE);
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PRO) {
			bm_set_vddc_voltage(bmdi->bmcd->card_bmdi[p_data->chip0_index - bmdi->bmcd->dev_start_index], 980);
		}
		else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC7_PLUS) {
			bm_set_vddc_voltage(bmdi->bmcd->card_bmdi[p_data->chip0_index - bmdi->bmcd->dev_start_index], 960);
		}
	}

	return 0;
}

int bmdev_get_smi_attr(struct bm_device_info *bmdi, struct bm_smi_attr *pattr)
{
	struct chip_info *cinfo;
	int i;
#ifndef SOC_MODE
	vpu_statistic_info_t *vpu_usage_info;
	jpu_statistic_info_t *jpu_usage_info;
	vpp_statistic_info_t *vpp_usage_info;
#endif

	cinfo = &bmdi->cinfo;

#ifndef SOC_MODE
	vpu_usage_info = &bmdi->vpudrvctx.s_vpu_usage_info;
	jpu_usage_info = &bmdi->jpudrvctx.s_jpu_usage_info;
	vpp_usage_info = &bmdi->vppdrvctx.s_vpp_usage_info;

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
    } else if (bmdi->cinfo.chip_id == 0x1686) {
	    for (i = 0; i < MAX_NUM_VPU_CORE_BM1686; i++) {
#ifndef SOC_MODE
		    pattr->vpu_instant_usage[i] = vpu_usage_info->vpu_instant_usage[i];
#else
		    pattr->vpu_instant_usage[i] = ATTR_NOTSUPPORTED_VALUE;
#endif
	    }
	    for (i = 0; i < MAX_NUM_JPU_CORE_BM1686; i++) {
#ifndef SOC_MODE
	        pattr->jpu_core_usage[i] = jpu_usage_info->jpu_core_usage[i];
#else
	        pattr->jpu_core_usage[i] = ATTR_NOTSUPPORTED_VALUE;
#endif
	    }
	}
	return 0;
}

int bmdev_ioctl_get_attr(struct bm_device_info *bmdi, void *arg)
{
	int ret = 0;
	struct bm_smi_attr smi_attr;

	ret = copy_from_user(&smi_attr, (struct bm_smi_attr __user *)arg,
			sizeof(struct bm_smi_attr));
	if (ret) {
		pr_err("bmdev_ctl_ioctl: copy_from_user fail\n");
		return ret;
	}

	ret = bmdev_get_smi_attr(bmdi, &smi_attr);
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
#endif
