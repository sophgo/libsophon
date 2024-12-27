#ifndef _VPSS_H_vpss_g
#define _VPSS_H_

#include <linux/common.h>
#include <linux/comm_video.h>
#include <linux/comm_sys.h>
#include <linux/comm_vpss.h>
// #include <linux/comm_region.h>
#include <linux/comm_errno.h>
#include <linux/vpss_uapi.h>
#include <vpss_ctx.h>
#include <base_ctx.h>
#include <base_cb.h>
// #include <ldc_cb.h>
#include "vpss_debug.h"

typedef void (*vpss_timer_cb)(void *data);

/* Configured from user, IOCTL */

signed int vpss_bm_send_frame(bm_vpss_cfg *vpss_cfg);

/* INTERNAL */
signed int vpss_set_vivpss_mode(const vi_vpss_mode_s *mode);
signed int vpss_set_grp_csc(struct vpss_grp_csc_cfg *cfg);
signed int vpss_set_chn_csc(struct vpss_chn_csc_cfg *cfg);
signed int vpss_get_proc_amp_ctrl(proc_amp_e type, proc_amp_ctrl_s *ctrl);
signed int vpss_get_proc_amp(vpss_grp grp_id, signed int *proc_amp);
signed int vpss_get_all_proc_amp(struct vpss_all_proc_amp_cfg *cfg);

void vpss_set_mlv_info(u8 snr_num, struct mlv_i_s *p_m_lv_i);
void vpss_get_mlv_info(u8 snr_num, struct mlv_i_s *p_m_lv_i);

int _vpss_call_cb(u32 m_id, u32 cmd_id, void *data);
void vpss_init(void);
void vpss_deinit(void);
s32 vpss_suspend_handler(void);
s32 vpss_resume_handler(void);

signed int check_vpss_id(vpss_grp grp_id, vpss_chn chn_id);

void vpss_mode_init(void);
void vpss_mode_deinit(void);

void register_timer_fun(vpss_timer_cb cb, void *data);

struct vpss_ctx **vpss_get_ctx(void);

void vpss_release_grp(void);

//Check GRP and CHN VALID, CREATED and FMT
#define vpss_grp_SUPPORT_FMT(fmt) \
	((fmt == PIXEL_FORMAT_RGB_888_PLANAR) || (fmt == PIXEL_FORMAT_BGR_888_PLANAR) ||	\
	 (fmt == PIXEL_FORMAT_RGB_888) || (fmt == PIXEL_FORMAT_BGR_888) ||			\
	 (fmt == PIXEL_FORMAT_YUV_PLANAR_420) || (fmt == PIXEL_FORMAT_YUV_PLANAR_422) ||	\
	 (fmt == PIXEL_FORMAT_YUV_PLANAR_444) || (fmt == PIXEL_FORMAT_YUV_400) ||		\
	 (fmt == PIXEL_FORMAT_NV12) || (fmt == PIXEL_FORMAT_NV21) ||				\
	 (fmt == PIXEL_FORMAT_NV16) || (fmt == PIXEL_FORMAT_NV61) ||				\
	 (fmt == PIXEL_FORMAT_YUYV) || (fmt == PIXEL_FORMAT_UYVY) ||				\
	 (fmt == PIXEL_FORMAT_YVYU) || (fmt == PIXEL_FORMAT_VYUY) ||				\
	 (fmt == PIXEL_FORMAT_YUV_444))

#define VPSS_CHN_SUPPORT_FMT(fmt) \
	((fmt == PIXEL_FORMAT_RGB_888_PLANAR) || (fmt == PIXEL_FORMAT_BGR_888_PLANAR) ||	\
	 (fmt == PIXEL_FORMAT_RGB_888) || (fmt == PIXEL_FORMAT_BGR_888) ||			\
	 (fmt == PIXEL_FORMAT_YUV_PLANAR_420) || (fmt == PIXEL_FORMAT_YUV_PLANAR_422) ||	\
	 (fmt == PIXEL_FORMAT_YUV_PLANAR_444) || (fmt == PIXEL_FORMAT_YUV_400) ||		\
	 (fmt == PIXEL_FORMAT_HSV_888) || (fmt == PIXEL_FORMAT_HSV_888_PLANAR) ||		\
	 (fmt == PIXEL_FORMAT_NV12) || (fmt == PIXEL_FORMAT_NV21) ||				\
	 (fmt == PIXEL_FORMAT_NV16) || (fmt == PIXEL_FORMAT_NV61) ||				\
	 (fmt == PIXEL_FORMAT_YUYV) || (fmt == PIXEL_FORMAT_UYVY) ||				\
	 (fmt == PIXEL_FORMAT_YVYU) || (fmt == PIXEL_FORMAT_VYUY) ||				\
	 (fmt == PIXEL_FORMAT_YUV_444) ||							\
	 (fmt == PIXEL_FORMAT_FP32_C3_PLANAR) || (fmt == PIXEL_FORMAT_FP16_C3_PLANAR) ||	\
	 (fmt == PIXEL_FORMAT_BF16_C3_PLANAR) || (fmt == PIXEL_FORMAT_INT8_C3_PLANAR) ||	\
	 (fmt == PIXEL_FORMAT_UINT8_C3_PLANAR))

#define VPSS_UPSAMPLE(fmt_grp, fmt_chn) \
	((IS_FMT_YUV420(fmt_grp) \
	&& (IS_FMT_YUV422(fmt_chn) || IS_FMT_RGB(fmt_chn) || (fmt_chn == PIXEL_FORMAT_YUV_PLANAR_444) || (fmt_chn == PIXEL_FORMAT_YUV_444))) \
	|| (IS_FMT_YUV422(fmt_grp) && (IS_FMT_RGB(fmt_chn) || (fmt_chn == PIXEL_FORMAT_YUV_PLANAR_444) || (fmt_chn == PIXEL_FORMAT_YUV_444))))

#define FRC_INVALID(frame_rate)	\
	(frame_rate.dst_frame_rate <= 0 || frame_rate.src_frame_rate <= 0 ||	\
		frame_rate.dst_frame_rate >= frame_rate.src_frame_rate)

#endif /* _VPSS_H_ */
