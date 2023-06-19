#ifndef _VPP_K_H_
#define _VPP_K_H_
//#include "../bm_common.h"

typedef unsigned char u8;
typedef unsigned long long u64;
typedef unsigned int uint32;


#define SW_RESET_REG0 0x50010c00
#define SW_RESET_REG1 0x50010c04
#define VPP_CORE0_RST 22

#define SW_RESET_REG2 0x50010c08
#define VPP_CORE1_RST 0

/*vpp global control*/
#define VPP_VERSION           (0x100)
#define VPP_CONTROL0          (0x110)
#define VPP_CMD_BASE          (0x114)
#define VPP_CMD_BASE_EXT      (0x118)
#define VPP_STATUS            (0x11C)
#define VPP_INT_EN            (0x120)
#define VPP_INT_CLEAR         (0x124)
#define VPP_INT_STATUS        (0x128)
#define VPP_INT_RAW_STATUS    (0x12c)


/*vpp descriptor for DMA CMD LIST*/


#define VPP_CROP_NUM_MAX (512)
#define VPP1686_CORE_MAX (2)
#define Transaction_Mode (2)


struct des_head_s {
  uint32 crop_id           : 16;
  uint32 crop_flag         : 1;
  uint32 rsv               : 7;
  uint32 next_cmd_base_ext : 8;
};

struct src_size_s {
  uint32 src_height : 16;
  uint32 src_width  : 16;
};

struct src_ctrl_s {
  uint32 input_format        : 5;
  uint32 rsv0                : 4;
  uint32 post_padding_enable : 1;
  uint32 padding_enable      : 1;
  uint32 rect_border_enable  : 1;
  uint32 font_enable         : 1;
  uint32 con_bri_enable      : 1;
  uint32 csc_scale_order     : 1;
  uint32 wdma_form           : 3;
  uint32 fbc_multi_map       : 1;
  uint32 rsv1                : 13;
};

struct src_crop_size_s {
  uint32 src_crop_height : 14;
  uint32 rsv1            : 2;
  uint32 src_crop_width  : 14;
  uint32 rsv2            : 2;
};

struct src_crop_st_s {
  uint32 src_crop_st_y : 14;
  uint32 rsv1          : 2;
  uint32 src_crop_st_x : 14;
  uint32 rsv2          : 2;
};

struct src_base_ext_s {
  uint32 src_base_ext_ch0 : 8;
  uint32 src_base_ext_ch1 : 8;
  uint32 src_base_ext_ch2 : 8;
  uint32 rsv              : 8;
};
struct padding_value_s {
  uint32 padding_value_ch0 : 8;
  uint32 padding_value_ch1 : 8;
  uint32 padding_value_ch2 : 8;
  uint32 rsv               : 8;
};
struct src_stride_ch0_s {
  uint32 src_stride_ch0 : 16;
  uint32 rsv            : 16;
};
struct src_stride_ch1_s {
  uint32 src_stride_ch1 : 16;
  uint32 rsv            : 16;
};
struct src_stride_ch2_s {
  uint32 src_stride_ch2 : 16;
  uint32 rsv            : 16;
};
struct padding_ext_s {
  uint32 padding_ext_up    : 8;
  uint32 padding_ext_bot   : 8;
  uint32 padding_ext_left  : 8;
  uint32 padding_ext_right : 8;
};
struct src_border_value_s {
  uint32 src_border_value_r   : 8;
  uint32 src_border_value_g   : 8;
  uint32 src_border_value_b   : 8;
  uint32 src_border_thickness : 8;
};

struct src_border_size_s {
  uint32 src_border_height : 14;
  uint32 rsv1              : 2;
  uint32 src_border_width  : 14;
  uint32 rsv2              : 2;
};

struct src_border_st_s {
  uint32 src_border_st_y : 14;
  uint32 rsv1            : 2;
  uint32 src_border_st_x : 14;
  uint32 rsv2            : 2;
};
struct src_font_pitch_s {
  uint32 src_font_pitch    : 14;
  uint32 src_font_type     : 1;
  uint32 src_font_nf_range : 1;
  uint32 src_font_dot_en   : 1;
  uint32 rsv1              : 15;
};

struct src_font_value_s {
  uint32 src_font_value_r : 8;
  uint32 src_font_value_g : 8;
  uint32 src_font_value_b : 8;
  uint32 src_font_ext     : 8;
};

struct src_font_size_s {
  uint32 src_font_height : 14;
  uint32 rsv1            : 2;
  uint32 src_font_width  : 14;
  uint32 rsv2            : 2;
};

struct src_font_st_s {
  uint32 src_font_st_y : 14;
  uint32 rsv1          : 2;
  uint32 src_font_st_x : 14;
  uint32 rsv2          : 2;
};
struct dst_ctrl_s {
  uint32 output_format   : 5;
  uint32 rsv             : 11;
  uint32 cb_coeff_sel_tl : 2;
  uint32 cb_coeff_sel_tr : 2;
  uint32 cb_coeff_sel_bl : 2;
  uint32 cb_coeff_sel_br : 2;
  uint32 cr_coeff_sel_tl : 2;
  uint32 cr_coeff_sel_tr : 2;
  uint32 cr_coeff_sel_bl : 2;
  uint32 cr_coeff_sel_br : 2;
};

struct dst_crop_size_s {
  uint32 dst_crop_height : 14;
  uint32 rsv1            : 2;
  uint32 dst_crop_width  : 14;
  uint32 rsv2            : 2;
};

struct dst_crop_st_s {
  uint32 dst_crop_st_y : 14;
  uint32 rsv1          : 2;
  uint32 dst_crop_st_x : 14;
  uint32 rsv2          : 2;
};

struct dst_base_ext_s {
  uint32 dst_base_ext_ch0 : 8;
  uint32 dst_base_ext_ch1 : 8;
  uint32 dst_base_ext_ch2 : 8;
  uint32 dst_base_ext_ch3 : 8;
};
struct dst_stride_ch0_s {
  uint32 dst_stride_ch0 : 16;
  uint32 rsv            : 16;
};

struct dst_stride_ch1_s {
  uint32 dst_stride_ch1 : 16;
  uint32 rsv            : 16;
};

struct dst_stride_ch2_s {
  uint32 dst_stride_ch2 : 16;
  uint32 rsv            : 16;
};

struct dst_stride_ch3_s {
  uint32 dst_stride_ch3 : 16;
  uint32 rsv            : 16;
};
struct map_conv_ext_s {
  uint32 map_conv_off_base_ext_y : 8;
  uint32 map_conv_off_base_ext_c : 8;
  uint32 map_conv_comp_ext_y     : 8;
  uint32 map_conv_comp_ext_c     : 8;
};

struct map_conv_off_stride_s {
  uint32 map_conv_pic_height_c : 14;
  uint32 rsv1                  : 2;
  uint32 map_conv_pic_height_y : 14;
  uint32 rsv2                  : 2;
};

struct map_conv_comp_stride_s {
  uint32 map_conv_comp_stride_c : 16;
  uint32 map_conv_comp_stride_y : 16;
};

struct map_conv_crop_pos_s {
  uint32 map_conv_crop_st_y : 14;
  uint32 rsv1               : 2;
  uint32 map_conv_crop_st_x : 14;
  uint32 rsv2               : 2;
};

struct map_conv_crop_size_s {
  uint32 map_conv_crop_height : 14;
  uint32 rsv1                 : 2;
  uint32 map_conv_crop_width  : 14;
  uint32 rsv2                 : 2;
};

struct map_conv_out_ctrl_s {
  uint32 map_conv_hsync_period : 14;
  uint32 rsv1                  : 2;
  uint32 map_conv_out_mode_c   : 3;
  uint32 rsv2                  : 1;
  uint32 map_conv_out_mode_y   : 3;
  uint32 rsv3                  : 9;
};

struct map_conv_time_dep_end_s {
  uint32 map_conv_time_out_cnt  : 16;
  uint32 map_conv_comp_endian   : 4;
  uint32 map_conv_offset_endian : 4;
  uint32 map_conv_bit_depth_c   : 2;
  uint32 map_conv_bit_depth_y   : 2;
  uint32 rsv                    : 4;
};
struct scl_ctrl_s {
  uint32 scl_ctrl : 2;
  uint32 rsv      : 30;
};

struct scl_init_s {
  uint32 scl_init_y : 1;
  uint32 rsv1       : 15;
  uint32 scl_init_x : 1;
  uint32 rsv2       : 15;
};
struct con_bri_ctrl_s{
  uint32 con_bri_round    : 3;
  uint32 con_bri_full     : 1;
  uint32 hr_csc_f2i_round : 3;
  uint32 rsv1             : 1;
  uint32 hr_csc_i2f_round : 3;
  uint32 rsv              : 21;
};
struct post_padding_ext_s{
  uint32 post_padding_ext_up    : 8;
  uint32 post_padding_ext_bot   : 8;
  uint32 post_padding_ext_left  : 8;
  uint32 post_padding_ext_right : 8;
};

typedef struct vpp1686_descriptor_s {
  struct des_head_s              des_head;
  uint32                         next_cmd_base;
  struct src_size_s              src_size;
  uint32                         rev2;
  struct src_ctrl_s              src_ctrl;
  struct src_crop_size_s         src_crop_size;
  struct src_crop_st_s           src_crop_st;
  struct src_base_ext_s          src_base_ext;
  uint32                         src_base_ch0;
  uint32                         src_base_ch1;
  uint32                         src_base_ch2;
  struct padding_value_s         padding_value;
  struct src_stride_ch0_s        src_stride_ch0;
  struct src_stride_ch1_s        src_stride_ch1;
  struct src_stride_ch2_s        src_stride_ch2;
  struct padding_ext_s           padding_ext;
  struct src_border_value_s      src_border_value;
  struct src_border_size_s       src_border_size;
  struct src_border_st_s         src_border_st;
  struct src_font_pitch_s        src_font_pitch;
  struct src_font_value_s        src_font_value;
  struct src_font_size_s         src_font_size;
  struct src_font_st_s           src_font_st;
  uint32                         src_font_addr;
  struct dst_ctrl_s              dst_ctrl;
  struct dst_crop_size_s         dst_crop_size;
  struct dst_crop_st_s           dst_crop_st;
  struct dst_base_ext_s          dst_base_ext;
  uint32                         dst_base_ch0;
  uint32                         dst_base_ch1;
  uint32                         dst_base_ch2;
  uint32                         dst_base_ch3;
  struct dst_stride_ch0_s        dst_stride_ch0;
  struct dst_stride_ch1_s        dst_stride_ch1;
  struct dst_stride_ch2_s        dst_stride_ch2;
  struct dst_stride_ch3_s        dst_stride_ch3;
  uint32                         rev3;
  struct map_conv_ext_s          map_conv_ext;
  uint32                         map_conv_off_base_y;
  uint32                         map_conv_off_base_c;
  struct map_conv_off_stride_s   map_conv_off_stride;
  uint32                         map_conv_comp_base_y;
  uint32                         map_conv_comp_base_c;
  struct map_conv_comp_stride_s  map_conv_comp_stride;
  struct map_conv_crop_pos_s     map_conv_crop_pos;
  struct map_conv_crop_size_s    map_conv_crop_size;
  struct map_conv_out_ctrl_s     map_conv_out_ctrl;
  struct map_conv_time_dep_end_s map_conv_time_dep_end;
  struct scl_ctrl_s              scl_ctrl;
  struct scl_init_s              scl_init;
  float                          scl_x;
  float                          scl_y;
  float                          csc_coe00;
  float                          csc_coe01;
  float                          csc_coe02;
  float                          csc_add0;
  float                          csc_coe10;
  float                          csc_coe11;
  float                          csc_coe12;
  float                          csc_add1;
  float                          csc_coe20;
  float                          csc_coe21;
  float                          csc_coe22;
  float                          csc_add2;
  float                          con_bri_a_0;
  float                          con_bri_a_1;
  float                          con_bri_a_2;
  struct con_bri_ctrl_s          con_bri_ctrl;
  float                          con_bri_b_0;
  float                          con_bri_b_1;
  float                          con_bri_b_2;
  float                          post_padding_value_ch0;
  float                          post_padding_value_ch1;
  float                          post_padding_value_ch2;
  float                          post_padding_value_ch3;
  struct post_padding_ext_s      post_padding_ext;

  int                            src_fd0;
  int                            src_fd1;
  int                            src_fd2;
  int                            src_fd3;

  int                            dst_fd0;
  int                            dst_fd1;
  int                            dst_fd2;
  int                            dst_fd3;

}descriptor;

struct vpp_batch_n {
  int num;
  descriptor *cmd;
};


struct vpp_reset_info {
	u32 reg;
	u32 bit_n;
};

extern int bmdev_memcpy_d2s(struct bm_device_info *bmdi,struct file *file,
		void __user *dst, u64 src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);
#endif

