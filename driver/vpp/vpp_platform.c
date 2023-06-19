#include "bm_common.h"
#include "vpp_platform.h"

int vpp_init(struct bm_device_info *bmdi)
{
  if (bmdi->cinfo.chip_id == 0x1684) {
    bm1684_vpp_init(bmdi);
    bmdi->vppdrvctx.vpp_init           = bm1684_vpp_init;
    bmdi->vppdrvctx.vpp_exit           = bm1684_vpp_exit;
    bmdi->vppdrvctx.trigger_vpp        = bm1684_trigger_vpp;
    bmdi->vppdrvctx.bm_vpp_request_irq = bm1684_vpp_request_irq;
    bmdi->vppdrvctx.bm_vpp_free_irq    = bm1684_vpp_free_irq;
  }

  if (bmdi->cinfo.chip_id == 0x1686) {
    bm1686_vpp_init(bmdi);
    bmdi->vppdrvctx.vpp_init           = bm1686_vpp_init;
    bmdi->vppdrvctx.vpp_exit           = bm1686_vpp_exit;
    bmdi->vppdrvctx.trigger_vpp        = bm1686_trigger_vpp;
    bmdi->vppdrvctx.bm_vpp_request_irq = bm1686_vpp_request_irq;
    bmdi->vppdrvctx.bm_vpp_free_irq    = bm1686_vpp_free_irq;
  }

  if(NULL == bmdi->vppdrvctx.vpp_init) {
    printk("vpp_init failed, bmdi->cinfo.chip_id %d\n",bmdi->cinfo.chip_id);
    return -1;
  }

  bmdi->vppdrvctx.vpp_init(bmdi);
  return 0;
}

void vpp_exit(struct bm_device_info *bmdi)
{
  if(NULL == bmdi->vppdrvctx.vpp_exit) {
    printk("vpp_exit failed, bmdi->cinfo.chip_id %d\n",bmdi->cinfo.chip_id);
    return;
  }

  bmdi->vppdrvctx.vpp_exit(bmdi);
  return;
}

int trigger_vpp(struct bm_device_info *bmdi, unsigned long arg)
{
  int ret = 0;

  if(NULL == bmdi->vppdrvctx.trigger_vpp) {
    printk("trigger_vpp failed, bmdi->cinfo.chip_id %d\n",bmdi->cinfo.chip_id);
    return -1;
  }

  ret = bmdi->vppdrvctx.trigger_vpp(bmdi,arg);
  return ret;
}

void bm_vpp_request_irq(struct bm_device_info *bmdi)
{
  if(NULL == bmdi->vppdrvctx.bm_vpp_request_irq) {
    printk("bm_vpp_request_irqfailed, bmdi->cinfo.chip_id %d\n",bmdi->cinfo.chip_id);
    return;
  }

  bmdi->vppdrvctx.bm_vpp_request_irq(bmdi);
  return;
}

void bm_vpp_free_irq(struct bm_device_info *bmdi)
{
  if(NULL == bmdi->vppdrvctx.bm_vpp_free_irq) {
    printk("bm_vpp_free_irq, bmdi->cinfo.chip_id %d\n",bmdi->cinfo.chip_id);
    return;
  }

  bmdi->vppdrvctx.bm_vpp_free_irq(bmdi);
  return;
}

static int check_vpp_core_busy(struct bm_device_info *bmdi, int coreIdx)
{
	int ret = 0;

	if (atomic_read(&bmdi->vppdrvctx.s_vpp_usage_info.vpp_busy_status[coreIdx]) > 0)
		ret = 1;

	return ret;
}

int bm_vpp_check_usage_info(struct bm_device_info *bmdi)
{
	int ret = 0, i;

	vpp_statistic_info_t *vpp_usage_info = &bmdi->vppdrvctx.s_vpp_usage_info;

	/* update usage */
	for (i = 0; i < VPP_CORE_MAX; i++){
		int busy = check_vpp_core_busy(bmdi, i);
		int vpp_core_usage = 0;
		int j;
		vpp_usage_info->vpp_status_array[i][vpp_usage_info->vpp_status_index[i]] = busy;
		vpp_usage_info->vpp_status_index[i]++;
		vpp_usage_info->vpp_status_index[i] %= MAX_VPP_STAT_WIN_SIZE;

		if (busy == 1)
			vpp_usage_info->vpp_working_time_in_ms[i] += vpp_usage_info->vpp_instant_interval / MAX_VPP_STAT_WIN_SIZE;

		vpp_usage_info->vpp_total_time_in_ms[i] += vpp_usage_info->vpp_instant_interval / MAX_VPP_STAT_WIN_SIZE;

		for (j = 0; j < MAX_VPP_STAT_WIN_SIZE; j++)
			vpp_core_usage += vpp_usage_info->vpp_status_array[i][j];

		vpp_usage_info->vpp_core_usage[i] = vpp_core_usage;
	}

	return ret;
}

void vpp_usage_info_init(struct bm_device_info *bmdi)
{
	vpp_statistic_info_t *vpp_usage_info = &bmdi->vppdrvctx.s_vpp_usage_info;

	memset(vpp_usage_info, 0, sizeof(vpp_statistic_info_t));

	vpp_usage_info->vpp_instant_interval = 500;

	bm_vpp_check_usage_info(bmdi);

	return;
}