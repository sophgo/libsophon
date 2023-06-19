#ifndef BMCV_1684X_VPP_INTERANL_H
#define BMCV_1684X_VPP_INTERANL_H

#include "bmlib_runtime.h"

#if !defined DECL_EXPORT
#ifdef _WIN32
#define DECL_EXPORT __declspec(dllexport)
#define DECL_IMPORT __declspec(dllimport)
#else
#define DECL_EXPORT
#define DECL_IMPORT
#endif
#endif

/*vpp descriptor for DMA CMD LIST*/
#define VPP1684X_MIN_W           (8)
#define VPP1684X_MIN_H           (8)
#define VPP1684X_MAX_W           (8192)
#define VPP1684X_MAX_H           (8192)
#define VPP1684X_MAX_SCALE_RATIO (128)
#define VPP1684X_MAX_CROP_NUM    (512)
#define VPP1684X_MAX_CORE        (2)
#define Transaction_Mode         (2)

#define ENABLE             1
#define LEGACY             0
#define CENTRAL            1
#define BOOL               uint8
#define VPPALIGN(x, mask)  (((x) + ((mask)-1)) & ~((mask)-1))
#define UNUSED(x) (void)(x)

#define COLOR_SPACE_YUV             0
#define COLOR_SPACE_RGB             1
#define COLOR_SPACE_HSV             2
#define COLOR_SPACE_RGBY            3
#define COLOR_NOT_DEF               4


#define USE_CMODEL                1
#define NOT_USE_CMODEL            0

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef signed short       int16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

enum bm1686_vpp_color_space {
  COLOR_IN_RGB  = 0,
  COLOR_IN_YUV  = 1,
  COLOR_OUT_RGB = 2,
  COLOR_OUT_YUV = 3,
  COLOR_OUT_HSV = 4,

};
enum bm1686_vpp_data_type {
  DATA_TYPE_1N_BYTE_SIGNED  = 0,
  DATA_TYPE_1N_BYTE         = 1,
  DATA_TYPE_FP16            = 2,
  DATA_TYPE_BF16            = 3,
  DATA_TYPE_FLOAT32         = 4,
};

enum bm1686_vpp_input_format {
  IN_YUV400P = 0,
  IN_YUV420P = 1,
  IN_YUV422P = 2,
  IN_YUV444P = 3,
  IN_NV12 = 4,
  IN_NV21 = 5,
  IN_NV16 = 6,
  IN_NV61 = 7,
  IN_YUV444_PACKET = 8,
  IN_YVU444_PACKET = 9,
  IN_YUV422_YUYV = 10,
  IN_YUV422_YVYU = 11,
  IN_YUV422_UYVY = 12,
  IN_YUV422_VYUY = 13,
  IN_RSV5 = 14,
  IN_RSV6 = 15,
  IN_RSV7 = 16,
  IN_RGBP = 17,
  IN_RGB24 = 18,
  IN_BGR24 = 19,
  IN_RSV8 = 20,
  IN_RSV9 = 21,
  IN_RSV10 = 22,
  IN_RSV11 = 23,
  IN_RSV12 = 24,
  IN_RSV13 = 25,
  IN_RSV14 = 26,
  IN_RSV15 = 27,
  IN_RSV16 = 28,
  IN_RSV17 = 29,
  IN_RSV18 = 30,
  IN_FBC = 31,
  IN_FORMAT_MAX = 32
};

enum bm1686_vpp_output_format {
  OUT_YUV400P = 0,
  OUT_YUV420P = 1,
  OUT_YUV444P = 3,
  OUT_NV12 = 4,
  OUT_NV21 = 5,
  OUT_RGBYP = 16,
  OUT_RGBP = 17,
  OUT_RGB24 = 18,
  OUT_BGR24 = 19,
  OUT_HSV180 = 20,
  OUT_HSV256 = 21,
  OUT_FORMAT_MAX = 22
};


enum scaling_algorithm {
  bilinear_center       = 0,
  nearest_floor         = 1,
  nearest_round         = 2,
  scaling_algorithm_max = 3
};

typedef enum csc_type_1684x {
  YCbCr2RGB_BT601 = 0,
  YPbPr2RGB_BT601,
  RGB2YCbCr_BT601,
  YCbCr2RGB_BT709,
  RGB2YCbCr_BT709,
  RGB2YPbPr_BT601,
  YPbPr2RGB_BT709,
  RGB2YPbPr_BT709,
  USER_DEFINED_MATRIX = 1000,
  CSC_MAX
} csc_type_1684x_t;

typedef struct {
  float csc_coe00;
  float csc_coe01;
  float csc_coe02;
  float csc_add0;
  float csc_coe10;
  float csc_coe11;
  float csc_coe12;
  float csc_add1;
  float csc_coe20;
  float csc_coe21;
  float csc_coe22;
  float csc_add2;
#ifndef WIN32
} __attribute__((packed)) csc_matrix_1684x_t;
#else
} csc_matrix_1684x_t;
#endif

typedef struct padding {
  BOOL  padding_enable;
  uint8 top;
  uint8 bottom;
  uint8 left;
  uint8 right;
  uint8 padding_ch0;
  uint8 padding_ch1;
  uint8 padding_ch2;
}padding_t;

typedef struct post_padding {
  BOOL  post_padding_enable;
  uint8 top;
  uint8 bottom;
  uint8 left;
  uint8 right;
  float padding_ch0;
  float padding_ch1;
  float padding_ch2;
  float padding_ch3;
}post_padding_t;

typedef struct border {
  BOOL   rect_border_enable;
  uint16 st_x;
  uint16 st_y;
  uint16 width;
  uint16 height;
  uint8  value_r;
  uint8  value_g;
  uint8  value_b;
  uint8  thickness;
}border_t;

typedef struct font {
  BOOL   font_enable;
  BOOL   font_type;
  BOOL   font_nf_range;
  BOOL   font_dot_en;
  uint16 font_pitch;
  uint16 font_st_x;
  uint16 font_st_y;
  uint16 font_width;
  uint16 font_height;
  uint64 font_addr;
  uint8  font_value_r;
  uint8  font_value_g;
  uint8  font_value_b;
}font_t;

typedef struct downsample {
  uint8 cb_coeff_sel_tl;
  uint8 cb_coeff_sel_tr;
  uint8 cb_coeff_sel_bl;
  uint8 cb_coeff_sel_br;
  uint8 cr_coeff_sel_tl;
  uint8 cr_coeff_sel_tr;
  uint8 cr_coeff_sel_bl;
  uint8 cr_coeff_sel_br;
}downsample_t;

typedef struct resize {
  uint8 filter_sel;
  BOOL  scl_init_x ;
  BOOL  scl_init_y;
  float ratio_h_f;
  float ratio_w_f;
}resize_t;

typedef struct conbri_shifting {
  BOOL  con_bri_enable;
  uint8 con_bri_round;
  BOOL  con_bri_full;
  uint8 hr_csc_f2i_round;
  uint8 hr_csc_i2f_round;
  float con_bri_a_0;
  float con_bri_a_1;
  float con_bri_a_2;
  float con_bri_b_0;
  float con_bri_b_1;
  float con_bri_b_2;
}conbri_shifting_t;

typedef struct bm1684x_vpp_param_s {
  csc_type_1684x_t   csc_mode;
  csc_matrix_1684x_t csc_matrix;
  BOOL               fbc_multi_map;
  BOOL               csc_scale_order;
  padding_t          padding_param;
  post_padding_t     post_padding_param;
  border_t           border_param;
  font_t             font_param;
  downsample_t       downsample_param;
  resize_t           resize_param;
  conbri_shifting_t  conbri_shifting_param;
}bm1684x_vpp_param;

typedef struct bm1684x_vpp_mat_s {
  uint8  format;
  uint16 frm_w;
  uint16 frm_h;
  int16  axisX;
  int16  axisY;
  uint16 cropW;
  uint16 cropH;
  uint64 addr0;
  uint64 addr1;
  uint64 addr2;
  uint64 addr3;
  uint16 stride_ch0;
  uint16 stride_ch1;
  uint16 stride_ch2;
  uint16 stride_ch3;
  uint8  wdma_form;
}bm1684x_vpp_mat;


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

#define VPP_UPDATE_BATCH _IOWR('v', 0x01, unsigned long)
#define VPP_UPDATE_BATCH_VIDEO _IOWR('v', 0x02, unsigned long)
#define VPP_UPDATE_BATCH_SPLIT _IOWR('v', 0x03, unsigned long)
#define VPP_UPDATE_BATCH_NON_CACHE _IOWR('v', 0x04, unsigned long)
#define VPP_UPDATE_BATCH_CROP_TEST _IOWR('v', 0x05, unsigned long)
#define VPP_GET_STATUS _IOWR('v', 0x06, unsigned long)
#define VPP_TOP_RST _IOWR('v', 0x07, unsigned long)
#define VPP_UPDATE_BATCH_VIDEO_FD_PA _IOWR('v', 0x08, unsigned long)
#define VPP_UPDATE_BATCH_FD_PA _IOWR('v', 0x09, unsigned long)

DECL_EXPORT int bm1684x_vpp_cmodel(struct vpp_batch_n *batch,
  bm1684x_vpp_mat   *vpp_input,
  bm1684x_vpp_mat   *vpp_output,
  bm1684x_vpp_param *vpp_param);
#if defined(__cplusplus)
extern "C" {
#endif
DECL_EXPORT bm_status_t bm_trigger_vpp(bm_handle_t handle, struct vpp_batch_n* batch);
#if defined(__cplusplus)
}
#endif

#endif
