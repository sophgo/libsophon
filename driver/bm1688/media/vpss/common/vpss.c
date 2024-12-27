#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <asm/div64.h>

#include <base_ctx.h>
#include <linux/defines.h>
#include <linux/common.h>
#include <linux/comm_buffer.h>

#include "base_common.h"
#include "ion.h"
#include "sys.h"
#include "bind.h"
#include <vb.h>
#include "vbq.h"
#include <base_cb.h>
// #include <vpss_cb.h>
// #include <ldc_cb.h>
#include <vcodec_cb.h>
#include "vpss.h"
#include "vpss_common.h"
#include "vpss_hal.h"

/*******************************************************
 *  MACRO definition
 ******************************************************/
#ifndef FPGA_PORTING
#define IDLE_TIMEOUT_MS      10000
#define EOF_WAIT_TIMEOUT_MS  1000
#else
#define IDLE_TIMEOUT_MS      60000
#define EOF_WAIT_TIMEOUT_MS  60000
#endif

#define CTX_EVENT_WKUP       0x0001
#define CTX_EVENT_EOF        0x0002
#define CTX_EVENT_VI_ERR     0x0004

struct vpss_jobs_ctx {
	struct vb_jobs_t ins;
	struct vb_jobs_t outs[VPSS_MAX_CHN_NUM];
};
struct vpss_handler_ctx {
	wait_queue_head_t wait;
	struct task_struct *thread;
	spinlock_t hdl_lock;
	atomic_t active_cnt;
	unsigned char events;
};
struct vpss_ext_ctx {
	struct csc_cfg csc_cfg;
	struct csc_cfg chn_csc_cfg[VPSS_MAX_CHN_NUM];
	signed int proc_amp[PROC_AMP_MAX];
};

struct vpss_stitch_data {
	wait_queue_head_t wait;
	unsigned char flag;
};

static struct vpss_ctx *g_vpss_ctx[VPSS_MAX_GRP_NUM] = { [0 ... VPSS_MAX_GRP_NUM - 1] = NULL };

static unsigned char g_is_bm_scene = false;

static struct workqueue_struct *g_vpss_workqueue;

//update proc info
static void _update_vpss_chn_real_frame_rate(struct timer_list *timer);
DEFINE_TIMER(timer_proc, _update_vpss_chn_real_frame_rate);
static atomic_t g_timer_added = ATOMIC_INIT(0);

//global lock
static struct mutex g_vpss_lock;

//timer callback
static vpss_timer_cb g_core_cb;
static void *g_core_data;

static struct semaphore g_vpss_core_sem;

void vpss_wkup_frame_done_handle(void *pdata)
{
	struct vpss_job *job = container_of(pdata, struct vpss_job, data);

	if(!g_is_bm_scene)
		queue_work(g_vpss_workqueue, &job->job_work);
	else {
		struct vpss_stitch_data *data = (struct vpss_stitch_data *)job->data;
		data->flag = 1;
		wake_up(&data->wait);
	}
}

static void _update_vpss_chn_real_frame_rate(struct timer_list *timer)
{
	int i, j;
	unsigned long long duration, cur_time_us;
	struct timespec64 cur_time;

	UNUSED(timer);
	ktime_get_ts64(&cur_time);
	cur_time_us = (unsigned long long)cur_time.tv_sec * USEC_PER_SEC + cur_time.tv_nsec / NSEC_PER_USEC;

	for (i = 0; i < VPSS_MAX_GRP_NUM; ++i) {
		if (g_vpss_ctx[i] && g_vpss_ctx[i]->is_created) {
			for (j = 0; j < VPSS_MAX_CHN_NUM; ++j) {
				if (g_vpss_ctx[i]->chn_cfgs[j].is_enabled) {
					duration = cur_time_us - g_vpss_ctx[i]->chn_cfgs[j].chn_work_status.prev_time;
					if (duration >= 1000000) {
						g_vpss_ctx[i]->chn_cfgs[j].chn_work_status.real_frame_rate
							= g_vpss_ctx[i]->chn_cfgs[j].chn_work_status.frame_num;
						g_vpss_ctx[i]->chn_cfgs[j].chn_work_status.frame_num = 0;
						g_vpss_ctx[i]->chn_cfgs[j].chn_work_status.prev_time = cur_time_us;
					}
				}
			}
		}
	}
	g_core_cb(g_core_data);

	mod_timer(&timer_proc, jiffies + msecs_to_jiffies(1000));
}

signed int video_frame_dmabuf_fd_to_paddr(video_frame_s *video_frame){
	int dmabuf_fd = video_frame->phyaddr[0];
	int height = video_frame->height;
	pixel_format_e pixel_format = video_frame->pixel_format;
	int uv_height = (pixel_format == PIXEL_FORMAT_YUV_PLANAR_420 || pixel_format == PIXEL_FORMAT_NV21 ||
		pixel_format == PIXEL_FORMAT_NV12) ? (height / 2) : height;
	TRACE_VPSS(DBG_DEBUG, "dmabuf_fd%d\n", dmabuf_fd);
	video_frame->phyaddr[0] = vpss_dmabuf_fd_to_paddr(dmabuf_fd);
	video_frame->phyaddr[1] = video_frame->phyaddr[0]
		+ video_frame->stride[0] * height;
	video_frame->phyaddr[2] = video_frame->phyaddr[1]
		+ video_frame->stride[1] * uv_height;
	TRACE_VPSS(DBG_DEBUG, "phyaddr%lx, %lx, %lx\n", (long unsigned int)video_frame->phyaddr[0],
		(long unsigned int)video_frame->phyaddr[1], (long unsigned int)video_frame->phyaddr[2]);
	if(!video_frame->phyaddr[0]){
		TRACE_VPSS(DBG_ERR, "vpss_dmabuf_fd_to_paddr fail\n");
		return -1;
	}
	return 0;
}

signed int vpss_bm_send_frame(bm_vpss_cfg *vpss_cfg){
	signed int ret = 0, i = 0, j = 0;
	struct vpss_job *job = kzalloc(sizeof(struct vpss_job), GFP_ATOMIC);
	struct vpss_hal_grp_cfg *grp_hw_cfg = &job->cfg.grp_cfg;
	struct vpss_hal_chn_cfg *chn_hw_cfg = &job->cfg.chn_cfg[0];
	struct vpss_stitch_data data;

	if(!g_is_bm_scene) g_is_bm_scene = true;

	init_waitqueue_head(&data.wait);
	data.flag = 0;

	job->grp_id = 0;
	job->is_online = false;
	job->data = (void *)&data;
	job->job_cb = vpss_wkup_frame_done_handle;
	job->job_state = JOB_INVALID;
	spin_lock_init(&job->lock);

	job->cfg.chn_num = 1;
	job->cfg.chn_enable[0] = true;

	if(vpss_cfg->grp_csc_cfg.enable){
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++)
				grp_hw_cfg->csc_cfg.coef[i][j] = vpss_cfg->grp_csc_cfg.coef[i][j];
			grp_hw_cfg->csc_cfg.add[i] = vpss_cfg->grp_csc_cfg.add[i];
			grp_hw_cfg->csc_cfg.sub[i] = vpss_cfg->grp_csc_cfg.sub[i];
		}
		grp_hw_cfg->upsample = vpss_cfg->grp_csc_cfg.is_copy_upsample ? false : true;
	}

	if(vpss_cfg->chn_csc_cfg.enable){
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++)
				chn_hw_cfg->csc_cfg.coef[i][j] = vpss_cfg->chn_csc_cfg.coef[i][j];

			chn_hw_cfg->csc_cfg.add[i] = vpss_cfg->chn_csc_cfg.add[i];
			chn_hw_cfg->csc_cfg.sub[i] = vpss_cfg->chn_csc_cfg.sub[i];
		}
	}
	chn_hw_cfg->y_ratio = YRATIO_SCALE;

	if(vpss_cfg->grp_crop_cfg.crop_info.enable){
		grp_hw_cfg->crop.width = vpss_cfg->grp_crop_cfg.crop_info.crop_rect.width;
		grp_hw_cfg->crop.height = vpss_cfg->grp_crop_cfg.crop_info.crop_rect.height;
		grp_hw_cfg->crop.left = vpss_cfg->grp_crop_cfg.crop_info.crop_rect.x;
		grp_hw_cfg->crop.top = vpss_cfg->grp_crop_cfg.crop_info.crop_rect.y;
	} else {
		grp_hw_cfg->crop.left = 0;
		grp_hw_cfg->crop.top = 0;
		grp_hw_cfg->crop.width = vpss_cfg->snd_frm_cfg.video_frame.video_frame.width;
		grp_hw_cfg->crop.height = vpss_cfg->snd_frm_cfg.video_frame.video_frame.height;
	}

	if(vpss_cfg->chn_crop_cfg.crop_info.enable){
		chn_hw_cfg->crop.left = vpss_cfg->chn_crop_cfg.crop_info.crop_rect.x;
		chn_hw_cfg->crop.top = vpss_cfg->chn_crop_cfg.crop_info.crop_rect.y;
		chn_hw_cfg->crop.width = vpss_cfg->chn_crop_cfg.crop_info.crop_rect.width;
		chn_hw_cfg->crop.height = vpss_cfg->chn_crop_cfg.crop_info.crop_rect.height;
	} else {
		chn_hw_cfg->crop.left = 0;
		chn_hw_cfg->crop.top = 0;
		chn_hw_cfg->crop.width = grp_hw_cfg->crop.width;
		chn_hw_cfg->crop.height = grp_hw_cfg->crop.height;
	}

	if(vpss_cfg->chn_convert_cfg.convert.enable){
		chn_hw_cfg->convert_to_cfg.enable = vpss_cfg->chn_convert_cfg.convert.enable;
		chn_hw_cfg->convert_to_cfg.a_frac[0] = vpss_cfg->chn_convert_cfg.convert.a_factor[0];
		chn_hw_cfg->convert_to_cfg.a_frac[1] = vpss_cfg->chn_convert_cfg.convert.a_factor[1];
		chn_hw_cfg->convert_to_cfg.a_frac[2] = vpss_cfg->chn_convert_cfg.convert.a_factor[2];
		chn_hw_cfg->convert_to_cfg.b_frac[0] = vpss_cfg->chn_convert_cfg.convert.b_factor[0];
		chn_hw_cfg->convert_to_cfg.b_frac[1] = vpss_cfg->chn_convert_cfg.convert.b_factor[1];
		chn_hw_cfg->convert_to_cfg.b_frac[2] = vpss_cfg->chn_convert_cfg.convert.b_factor[2];
	}

	if(vpss_cfg->chn_draw_rect_cfg.draw_rect.rects[0].enable){
		for (i = 0; i < VPSS_RECT_NUM; i++) {
			rect_s rect;
			unsigned short thick;

			chn_hw_cfg->border_vpp_cfg[i].enable = vpss_cfg->chn_draw_rect_cfg.draw_rect.rects[i].enable;
			if (chn_hw_cfg->border_vpp_cfg[i].enable) {
				chn_hw_cfg->border_vpp_cfg[i].bg_color[0] = (vpss_cfg->chn_draw_rect_cfg.draw_rect.rects[i].bg_color >> 16) & 0xff;
				chn_hw_cfg->border_vpp_cfg[i].bg_color[1] = (vpss_cfg->chn_draw_rect_cfg.draw_rect.rects[i].bg_color >> 8) & 0xff;
				chn_hw_cfg->border_vpp_cfg[i].bg_color[2] = vpss_cfg->chn_draw_rect_cfg.draw_rect.rects[i].bg_color & 0xff;

				rect = vpss_cfg->chn_draw_rect_cfg.draw_rect.rects[i].rect;
				thick = vpss_cfg->chn_draw_rect_cfg.draw_rect.rects[i].thick;

				chn_hw_cfg->border_vpp_cfg[i].outside.start_x = rect.x;
				chn_hw_cfg->border_vpp_cfg[i].outside.start_y = rect.y;
				chn_hw_cfg->border_vpp_cfg[i].outside.end_x = rect.x + rect.width;
				chn_hw_cfg->border_vpp_cfg[i].outside.end_y = rect.y + rect.height;
				chn_hw_cfg->border_vpp_cfg[i].inside.start_x = rect.x + thick;
				chn_hw_cfg->border_vpp_cfg[i].inside.start_y = rect.y + thick;
				chn_hw_cfg->border_vpp_cfg[i].inside.end_x =
					chn_hw_cfg->border_vpp_cfg[i].outside.end_x - thick;
				chn_hw_cfg->border_vpp_cfg[i].inside.end_y =
					chn_hw_cfg->border_vpp_cfg[i].outside.end_y - thick;
			}
		}
	}

	if(vpss_cfg->coverex_cfg.rgn_coverex_cfg.rgn_coverex_param[0].enable){
		chn_hw_cfg->rgn_coverex_cfg = vpss_cfg->coverex_cfg.rgn_coverex_cfg;
	}

	for(i = 0; i < RGN_MAX_LAYER_VPSS; i++)
		if(vpss_cfg->rgn_cfg[i].num_of_rgn > 0)
			chn_hw_cfg->rgn_cfg[i] = vpss_cfg->rgn_cfg[i];

	grp_hw_cfg->src_size.width = vpss_cfg->snd_frm_cfg.video_frame.video_frame.width;
	grp_hw_cfg->src_size.height = vpss_cfg->snd_frm_cfg.video_frame.video_frame.height;
	grp_hw_cfg->pixelformat = vpss_cfg->snd_frm_cfg.video_frame.video_frame.pixel_format;
	for (i = 0; i < NUM_OF_PLANES; ++i)
		grp_hw_cfg->addr[i] = vpss_cfg->snd_frm_cfg.video_frame.video_frame.phyaddr[i];
	if (vpss_cfg->snd_frm_cfg.video_frame.video_frame.pixel_format == PIXEL_FORMAT_BGR_888_PLANAR) {
		grp_hw_cfg->addr[0] = vpss_cfg->snd_frm_cfg.video_frame.video_frame.phyaddr[2];
		grp_hw_cfg->addr[2] = vpss_cfg->snd_frm_cfg.video_frame.video_frame.phyaddr[0];
	}
	if (vpss_cfg->snd_frm_cfg.video_frame.video_frame.compress_mode == COMPRESS_MODE_FRAME){
		grp_hw_cfg->addr[3] = vpss_cfg->snd_frm_cfg.video_frame.video_frame.ext_phy_addr;
		grp_hw_cfg->fbd_enable = true;
	}
	grp_hw_cfg->bytesperline[0] = vpss_cfg->snd_frm_cfg.video_frame.video_frame.stride[0];
	grp_hw_cfg->bytesperline[1] = vpss_cfg->snd_frm_cfg.video_frame.video_frame.stride[1];
	grp_hw_cfg->bm_scene = true;

	chn_hw_cfg->pixelformat = vpss_cfg->chn_frm_cfg.video_frame.video_frame.pixel_format;
	chn_hw_cfg->bytesperline[0] = vpss_cfg->chn_frm_cfg.video_frame.video_frame.stride[0];
	chn_hw_cfg->bytesperline[1] = vpss_cfg->chn_frm_cfg.video_frame.video_frame.stride[1];
	if(vpss_cfg->chn_frm_cfg.video_frame.video_frame.phyaddr[0] < 0x1000){ // dmabuf_fd
		ret = video_frame_dmabuf_fd_to_paddr(&vpss_cfg->chn_frm_cfg.video_frame.video_frame);
		if(ret != 0){
			kfree(job);
			return ret;
		}
	}
	for (i = 0; i < NUM_OF_PLANES; ++i)
		chn_hw_cfg->addr[i] = vpss_cfg->chn_frm_cfg.video_frame.video_frame.phyaddr[i];
	if (vpss_cfg->chn_frm_cfg.video_frame.video_frame.pixel_format == PIXEL_FORMAT_BGR_888_PLANAR) {
		chn_hw_cfg->addr[0] = vpss_cfg->chn_frm_cfg.video_frame.video_frame.phyaddr[2];
		chn_hw_cfg->addr[2] = vpss_cfg->chn_frm_cfg.video_frame.video_frame.phyaddr[0];
	}
	chn_hw_cfg->src_size.width = grp_hw_cfg->crop.width;
	chn_hw_cfg->src_size.height = grp_hw_cfg->crop.height;
	chn_hw_cfg->dst_size.width = vpss_cfg->chn_frm_cfg.video_frame.video_frame.width;
	chn_hw_cfg->dst_size.height = vpss_cfg->chn_frm_cfg.video_frame.video_frame.height;

	if (vpss_cfg->chn_attr.chn_attr.flip && vpss_cfg->chn_attr.chn_attr.mirror)
		chn_hw_cfg->flip = SC_FLIP_HVFLIP;
	else if (vpss_cfg->chn_attr.chn_attr.flip)
		chn_hw_cfg->flip = SC_FLIP_VFLIP;
	else if (vpss_cfg->chn_attr.chn_attr.mirror)
		chn_hw_cfg->flip = SC_FLIP_HFLIP;
	else
		chn_hw_cfg->flip = SC_FLIP_NO;

	chn_hw_cfg->dst_rect.left = vpss_cfg->chn_attr.chn_attr.aspect_ratio.video_rect.x;
	chn_hw_cfg->dst_rect.top = vpss_cfg->chn_attr.chn_attr.aspect_ratio.video_rect.y;
	chn_hw_cfg->dst_rect.width = vpss_cfg->chn_attr.chn_attr.aspect_ratio.video_rect.width;
	chn_hw_cfg->dst_rect.height = vpss_cfg->chn_attr.chn_attr.aspect_ratio.video_rect.height;

	chn_hw_cfg->border_cfg.enable = vpss_cfg->chn_attr.chn_attr.aspect_ratio.enable_bgcolor
			&& ((chn_hw_cfg->dst_rect.width != chn_hw_cfg->dst_size.width)
			 || (chn_hw_cfg->dst_rect.height != chn_hw_cfg->dst_size.height));
	chn_hw_cfg->border_cfg.offset_x = chn_hw_cfg->dst_rect.left;
	chn_hw_cfg->border_cfg.offset_y = chn_hw_cfg->dst_rect.top;
	chn_hw_cfg->border_cfg.bg_color[2] = vpss_cfg->chn_attr.chn_attr.aspect_ratio.bgcolor & 0xff;
	chn_hw_cfg->border_cfg.bg_color[1] = (vpss_cfg->chn_attr.chn_attr.aspect_ratio.bgcolor >> 8) & 0xff;
	chn_hw_cfg->border_cfg.bg_color[0] = (vpss_cfg->chn_attr.chn_attr.aspect_ratio.bgcolor >> 16) & 0xff;

	switch (vpss_cfg->chn_coef_level_cfg.coef) {
	default:
	case VPSS_SCALE_COEF_BILINEAR:
		chn_hw_cfg->sc_coef = SC_SCALING_COEF_BILINEAR;
		break;
	case VPSS_SCALE_COEF_NEAREST:
		chn_hw_cfg->sc_coef = SC_SCALING_COEF_NEAREST;
		break;
	case VPSS_SCALE_COEF_BICUBIC_OPENCV:
		chn_hw_cfg->sc_coef = SC_SCALING_COEF_BICUBIC_OPENCV;
		break;
	}

	if (down_interruptible(&g_vpss_core_sem)){
		ret = -1;
		goto fail;
	}

	if (vpss_hal_direct_schedule(job))
		vpss_hal_push_job(job);

	wait_event_timeout(data.wait, data.flag, msecs_to_jiffies(vpss_cfg->chn_frm_cfg.milli_sec));
	if (data.flag){
		ret = 0;
		data.flag = 0;
	} else
		ret = -1;

	if (job->job_state == JOB_WAIT || job->job_state == JOB_WORKING){
		vpss_hal_remove_job(job);
		for (i = 0; i < VPSS_MAX; i++) {
			if (!(job->vpss_dev_mask & BIT(i)))
				continue;
			vpss_hal_down_reg(i);
		}
	}
fail:
	up(&g_vpss_core_sem);
	kfree(job);

	return ret;
}

void vpss_mode_init(void)
{
	mutex_lock(&g_vpss_lock);
	if (atomic_cmpxchg(&g_timer_added, 0, 1) == 0)
		add_timer(&timer_proc);
	mod_timer(&timer_proc, jiffies + msecs_to_jiffies(1000));
	mutex_unlock(&g_vpss_lock);
}

void vpss_mode_deinit(void)
{
	mutex_lock(&g_vpss_lock);
	if (atomic_cmpxchg(&g_timer_added,1, 0) == 1)
		del_timer_sync(&timer_proc);
	mutex_unlock(&g_vpss_lock);
}

void register_timer_fun(vpss_timer_cb cb, void *data)
{
	g_core_cb = cb;
	g_core_data = data;
}

void vpss_init(void)
{
	mutex_init(&g_vpss_lock);
	sema_init(&g_vpss_core_sem, VPSS_WORK_MAX);
}

void vpss_deinit(void)
{
	mutex_destroy(&g_vpss_lock);
}

signed int vpss_suspend_handler(void)
{
	int count;
	vpss_grp grp_id;
	struct vpss_ctx *ctx;

	mutex_lock(&g_vpss_lock);
	if (atomic_cmpxchg(&g_timer_added,1, 0) == 1)
		del_timer(&timer_proc);
	mutex_unlock(&g_vpss_lock);

	for (grp_id = 0; grp_id < VPSS_MAX_GRP_NUM; ++grp_id) {
		if (!g_vpss_ctx[grp_id])
			continue;

		ctx = g_vpss_ctx[grp_id];

		//wait frame done
		count = 20;
		while (!ctx->online_from_isp && --count > 0) {
			if (atomic_read(&ctx->hdl_state) == HANDLER_STATE_STOP)
				break;
			usleep_range(1000, 2000);
		}
		if (count == 0) {
			TRACE_VPSS(DBG_ERR, "Grp(%d) Wait timeout, HW hang.\n", grp_id);
			//TODO: reset dev and job
		}
	}

	return 0;
}

signed int vpss_resume_handler(void)
{
	mutex_lock(&g_vpss_lock);
	if (atomic_cmpxchg(&g_timer_added,0, 1) == 0)
		add_timer(&timer_proc);
	mod_timer(&timer_proc, jiffies + msecs_to_jiffies(1000));
	mutex_unlock(&g_vpss_lock);

	return 0;
}
