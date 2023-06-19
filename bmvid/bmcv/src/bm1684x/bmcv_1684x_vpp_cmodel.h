/*
*    Copyright (c) 2019-2022 by Bitmain Technologies Inc. All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Bitmain Technologies Inc. This is proprietary information owned by
*    Bitmain Technologies Inc. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Bitmain Technologies Inc.
*
*/
#ifdef __linux__
#ifndef __BMCV_1686_VPP_CMODEL_H__
#define __BMCV_1686_VPP_CMODEL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
  extern "C" {
#endif

//------------------------------------------------------------------------------
typedef unsigned char    uint8;
typedef unsigned char    uint8_t;
typedef signed char      int8;
typedef unsigned short   uint16;
typedef short            int16;
typedef unsigned int     uint32;
typedef int              int32;
typedef unsigned int     uint32_t;
typedef unsigned long long uint64;

//------------------------------------------------------------------------------
#define sign(in) ((in >= 0) ? 1 : -1)
#define iabs(in) ((in >= 0) ? in : -in)
#define clip(in, low, upp) (((in) < (low)) ? (low) : ((in) > (upp)) ? (upp) : (in))
#define VPP_MIN(A, B) (A) = ((A) > (B) ? (B) : (A))
#define VPP_MAX(A, B) (A) = ((A) > (B) ? (A) : (B))

//------------------------------------------------------------------------------
#define STRING_LEN (100)

typedef struct
{
	unsigned short  manti : 10;
	unsigned short  exp : 5;
	unsigned short  sign : 1;
} fp16;
typedef struct
{
  unsigned short  manti: 7;
  unsigned short  exp  : 8;
  unsigned short  sign : 1;
} bf16;
union fp16_data
{
	unsigned short idata;
	fp16 ndata;
};
union bf16_data
{
	unsigned short idata;
	bf16 ndata;
};

typedef struct
{
	unsigned int manti : 23;
	unsigned int exp : 8;
	unsigned int sign : 1;
} fp32;

union fp32_data
{
	unsigned int idata;
    int idataSign;
	float fdata;
	fp32 ndata;
};
union float2char {
  float f;
  char c[4];
} float2char;


struct CSC_MATRIX_S {
  uint32 CSC_COE00;
  uint32 CSC_COE01;
  uint32 CSC_COE02;
  uint32 CSC_ADD0;
  uint32 CSC_COE10;
  uint32 CSC_COE11;
  uint32 CSC_COE12;
  uint32 CSC_ADD1;
  uint32 CSC_COE20;
  uint32 CSC_COE21;
  uint32 CSC_COE22;
  uint32 CSC_ADD2;
};
  struct DES_HEAD_S {
    uint32 crop_id : 16;
    uint32 crop_flag : 1;
    uint32 rsv : 7;
    uint32 next_cmd_base_ext : 8;
  };

//  struct NOR_01_S {
//    uint32 nor_alpha_0 : 8;
//    uint32 nor_beta_0 : 8;
//    uint32 nor_alpha_1 : 8;
//    uint32 nor_beta_1 : 8;
//  };
//  struct NOR_2_S {
//    uint32 nor_alpha_2 : 8;
//    uint32 nor_beta_2 : 8;
//    uint32 rsv : 16;
//  };

  struct SRC_CTRL_S {
    uint32 input_format : 5;
    uint32 csc_mode : 4;
    uint32 post_padding_enable : 1;
    uint32 padding_enable : 1;
    uint32 rect_border_enable : 1;
    uint32 font_enable : 1;
    uint32 con_bri_enable : 1;
    uint32 csc_scale_order:1;
    uint32 wdma_form:3;
    uint32 fbc_multi_map : 1;
    uint32 rsv : 13;

  };

  struct SRC_CROP_SIZE_S {
    uint32 src_crop_height : 14;
    uint32 rsv1 : 2;
    uint32 src_crop_width : 14;
    uint32 rsv2 : 2;
  };

  struct SRC_CROP_ST_S {
    uint32 src_crop_st_y : 14;
    uint32 rsv1 : 2;
    uint32 src_crop_st_x : 14;
    uint32 rsv2 : 2;
  };

  struct SRC_BASE_EXT_S {
    uint32 src_base_ext_ch0 : 8;
    uint32 src_base_ext_ch1 : 8;
    uint32 src_base_ext_ch2 : 8;
    uint32 rsv : 8;
  };

  struct PADDING_VALUE_S {
    uint32 padding_value_ch0 : 8;
    uint32 padding_value_ch1 : 8;
    uint32 padding_value_ch2 : 8;
    uint32 rsv : 8;
  };

  struct SRC_STRIDE_CH0_S {
    uint32 src_stride_ch0 : 16;
    uint32 rsv : 16;
  };
  struct SRC_STRIDE_CH1_S {
    uint32 src_stride_ch1 : 16;
    uint32 rsv : 16;
  };
  struct SRC_STRIDE_CH2_S {
    uint32 src_stride_ch2 : 16;
    uint32 rsv : 16;
  };

  struct PADDING_EXT_S {
    uint32 padding_ext_up : 8;
    uint32 padding_ext_bot : 8;
    uint32 padding_ext_left : 8;
    uint32 padding_ext_right : 8;
  };

  struct MAP_CONV_EXT_S {
    uint32 map_conv_comp_ext_c : 8;
    uint32 map_conv_comp_ext_y : 8;
    uint32 map_conv_comp_base_ext_c : 8;
    uint32 map_conv_comp_base_ext_y : 8;
  };

  struct MAP_CONV_OFF_BASE_Y_S {
    uint32 map_conv_off_base_y : 32;
  };

  struct MAP_CONV_OFF_BASE_C_S {
    uint32 map_conv_off_base_c : 32;
  };

  struct MAP_CONV_OFF_STRIDE_S {
    uint32 map_conv_pic_height_y : 16;
    uint32 map_conv_pic_height_c : 16;
  };

  struct MAP_CONV_COMP_BASE_Y_S {
    uint32 map_conv_comp_base_y : 32;

  };

  struct MAP_CONV_COMP_BASE_C_S {
    uint32 map_conv_comp_base_c : 32;
  };

  struct MAP_CONV_COMP_STRIDE_S {
    uint32 rsv1 : 2;
    uint32 map_conv_comp_stride_y : 14;
    uint32 rsv2 : 2;
    uint32 map_conv_comp_stride_c : 14;
  };

  struct MAP_CONV_CROP_POS_S {
    uint32 rsv1 : 2;
    uint32 map_conv_crop_st_x : 14;
    uint32 rsv2 : 2;
    uint32 map_conv_crop_st_y : 14;
  };

  struct MAP_CONV_CROP_SIZE_S {
    uint32 rsv1 : 2;
    uint32 map_conv_crop_width : 14;
    uint32 rsv2 : 2;
    uint32 map_conv_crop_height : 14;
  };

  struct MAP_CONV_OUT_CTRL_S {
    uint32 rsv1 : 9;
    uint32 map_conv_out_mode_y : 3;
    uint32 rsv2 : 1;
    uint32 map_conv_out_mode_c : 3;
    uint32 rsv3 : 2;
    uint32 map_conv_hsync_period : 14;
  };

  struct MAP_CONV_TIME_DEP_END_S {
    uint32 rsv : 1;
    uint32 map_conv_off_ext_y : 3;
    uint32 map_conv_bit_depth_y : 2;
    uint32 map_conv_bit_depth_c : 2;
    uint32 map_conv_offset_endian : 4;
    uint32 map_conv_comp_endian : 4;
    uint32 map_conv_time_out_cnt : 16;
    };

  struct SRC_BORDER_VALUE_S {
    uint32 src_border_value_r : 8;
    uint32 src_border_value_g : 8;
    uint32 src_border_value_b : 8;
    uint32 src_border_thickness : 8;
  };

  struct SRC_BORDER_SIZE_S {
    uint32 src_border_height : 14;
    uint32 rsv1 : 2;
    uint32 src_border_width : 14;
    uint32 rsv2 : 2;
  };

  struct SRC_BORDER_ST_S {
    uint32 src_border_st_y : 14;
    uint32 rsv1 : 2;
    uint32 src_border_st_x : 14;
    uint32 rsv2 : 2;
  };

  struct SRC_FONT_PITCH_S {
    uint32 src_font_pitch : 14;
    uint32 src_font_type : 1;
    uint32 src_font_nf_range : 1;
    uint32 src_font_dot_en : 1;
    uint32 rsv1 : 15;
  };

  struct SRC_FONT_VALUE_S {
    uint32 src_font_value_r : 8;
    uint32 src_font_value_g : 8;
    uint32 src_font_value_b : 8;
    uint32 src_font_ext : 8;
  };

  struct SRC_FONT_SIZE_S {
    uint32 src_font_height : 14;
    uint32 rsv1 : 2;
    uint32 src_font_width : 14;
    uint32 rsv2 : 2;
  };

  struct SRC_FONT_ST_S {
    uint32 src_font_st_y : 14;
    uint32 rsv1 : 2;
    uint32 src_font_st_x : 14;
    uint32 rsv2 : 2;
  };

  struct DST_CTRL_S {
    uint32 output_format : 5;
    uint32 rsv : 11;
    uint32 cb_coeff_sel_tl : 2;
    uint32 cb_coeff_sel_tr : 2;
    uint32 cb_coeff_sel_bl : 2;
    uint32 cb_coeff_sel_br : 2;
    uint32 cr_coeff_sel_tl : 2;
    uint32 cr_coeff_sel_tr : 2;
    uint32 cr_coeff_sel_bl : 2;
    uint32 cr_coeff_sel_br : 2;
  };

  struct DST_CROP_SIZE_S {
    uint32 dst_crop_height : 14;
    uint32 rsv1 : 2;
    uint32 dst_crop_width : 14;
    uint32 rsv2 : 2;
  };

  struct DST_CROP_ST_S {
    uint32 dst_crop_st_y : 14;
    uint32 rsv1 : 2;
    uint32 dst_crop_st_x : 14;
    uint32 rsv2 : 2;
  };

  struct DST_BASE_EXT_S {
    uint32 dst_base_ext_ch0 : 8;
    uint32 dst_base_ext_ch1 : 8;
    uint32 dst_base_ext_ch2 : 8;
    uint32 dst_base_ext_ch3 : 8;
  };

  struct DST_STRIDE_CH0_S {
    uint32 dst_stride_ch0 : 16;
    uint32 rsv : 16;
  };

  struct DST_STRIDE_CH1_S {
    uint32 dst_stride_ch1 : 16;
    uint32 rsv : 16;
  };

  struct DST_STRIDE_CH2_S {
    uint32 dst_stride_ch2 : 16;
    uint32 rsv : 16;
  };

  struct DST_STRIDE_CH3_S {
    uint32 dst_stride_ch3 : 16;
    uint32 rsv : 16;
  };

  struct SCL_CTRL_S {
    uint32 scl_ctrl : 2;
    uint32 rsv : 30;
  };

  struct SCL_INIT_S {
    uint32 scl_init_y : 1;
    uint32 rsv1 : 15;
    uint32 scl_init_x : 1;
    uint32 rsv2 : 15;
  };

  struct SCL_X_S {
    uint32 scl_x;
  };

  struct SCL_Y_S {
    uint32 scl_y;
  };

  struct CON_BRI_A_0_S{
    float con_bri_a_0;
  };
  struct CON_BRI_A_1_S{
    float con_bri_a_1;
  };
  struct CON_BRI_A_2_S {
    float con_bri_a_2;
  };

  struct CON_BRI_CTRL_S{
    uint32 con_bri_round : 3;
    uint32 con_bri_full : 1;
    uint32 hr_csc_f2i_round : 3;
    uint32 rsv1 : 1;
    uint32 hr_csc_i2f_round : 3;
    uint32 rsv : 21;
  };

  struct CON_BRI_B_0_S {
    float con_bri_b_0;
  };
  struct CON_BRI_B_1_S {
    float con_bri_b_1;
  };
  struct CON_BRI_B_2_S {
    float con_bri_b_2;
  };

  struct POST_PADDING_VALUE_S{
    uint32 post_padding_value_ch0;
    uint32 post_padding_value_ch1;
    uint32 post_padding_value_ch2;
    uint32 post_padding_value_ch3;
  };

  struct POST_PADDING_EXT_S{
    uint32 post_padding_ext_up    : 8;
    uint32 post_padding_ext_bot   : 8;
    uint32 post_padding_ext_left  : 8;
    uint32 post_padding_ext_right : 8;
  };



typedef struct vpp1686_processor_param {
  uint32        CROP_NUM;
  uint32        FONT_NUM;
  uint32        RECT_NUM;
  uint32        PAD_NUM;
  uint32        SRC_SIZE;

  struct DES_HEAD_S *DES_HEAD;
  uint32        *NEXT_CMD_BASE;

  struct SRC_CTRL_S        *SRC_CTRL;

  struct SRC_CROP_SIZE_S        *SRC_CROP_SIZE;

  struct SRC_CROP_ST_S *SRC_CROP_ST;

  struct SRC_BASE_EXT_S        SRC_BASE_EXT;
  uint64        SRC_BASE_CH0;
  uint64        SRC_BASE_CH1;
  uint64        SRC_BASE_CH2;

  struct PADDING_VALUE_S        *PADDING_VALUE;

  struct SRC_STRIDE_CH0_S        SRC_STRIDE_CH0;
  struct SRC_STRIDE_CH1_S        SRC_STRIDE_CH1;
  struct SRC_STRIDE_CH2_S         SRC_STRIDE_CH2;

  struct PADDING_EXT_S        *PADDING_EXT;

  struct MAP_CONV_EXT_S        *MAP_CONV_EXT;
  struct MAP_CONV_OFF_BASE_Y_S        *MAP_CONV_OFF_BASE_Y;
  struct MAP_CONV_OFF_BASE_C_S        *MAP_CONV_OFF_BASE_C;
  struct MAP_CONV_OFF_STRIDE_S        *MAP_CONV_OFF_STRIDE;
  struct MAP_CONV_COMP_BASE_Y_S        *MAP_CONV_COMP_BASE_Y;
  struct MAP_CONV_COMP_BASE_C_S        *MAP_CONV_COMP_BASE_C;
  struct MAP_CONV_COMP_STRIDE_S        *MAP_CONV_COMP_STRIDE;
  struct MAP_CONV_CROP_POS_S        *MAP_CONV_CROP_POS;
  struct MAP_CONV_CROP_SIZE_S        *MAP_CONV_CROP_SIZE;
  struct MAP_CONV_OUT_CTRL_S        *MAP_CONV_OUT_CTRL;
  struct MAP_CONV_TIME_DEP_END_S        *MAP_CONV_TIME_DEP_END;

  struct SRC_BORDER_VALUE_S        *SRC_BORDER_VALUE;
  struct SRC_BORDER_SIZE_S        *SRC_BORDER_SIZE;
  struct SRC_BORDER_ST_S        *SRC_BORDER_ST;

  struct SRC_FONT_PITCH_S        *SRC_FONT_PITCH;
  struct SRC_FONT_VALUE_S        *SRC_FONT_VALUE;
  struct SRC_FONT_SIZE_S        *SRC_FONT_SIZE;
  struct SRC_FONT_ST_S        *SRC_FONT_ST;
  uint64                       SRC_FONT_ADDR;

  struct DST_CTRL_S        *DST_CTRL;
  struct DST_CROP_SIZE_S        *DST_CROP_SIZE;
  struct DST_CROP_ST_S        *DST_CROP_ST;
  struct DST_BASE_EXT_S        DST_BASE_EXT;

  uint64        DST_BASE_CH0;
  uint64        DST_BASE_CH1;
  uint64        DST_BASE_CH2;
  uint64        DST_BASE_CH3;

  struct DST_STRIDE_CH0_S        *DST_STRIDE_CH0;
  struct DST_STRIDE_CH1_S        *DST_STRIDE_CH1;
  struct DST_STRIDE_CH2_S        *DST_STRIDE_CH2;
  struct DST_STRIDE_CH3_S        *DST_STRIDE_CH3;

  struct SCL_CTRL_S        *SCL_CTRL;

  struct SCL_INIT_S        *SCL_INIT;

  struct SCL_X_S        *SCL_X;

  struct SCL_Y_S        *SCL_Y;

  //DEF_CSC_MATRIX *CSC_MATRIX;
  struct CSC_MATRIX_S CSC_MATRIX;

  struct CON_BRI_A_0_S  *CON_BRI_A_0;
  struct CON_BRI_A_1_S  *CON_BRI_A_1;
  struct CON_BRI_A_2_S  *CON_BRI_A_2;

  struct CON_BRI_CTRL_S *CON_BRI_CTRL;

  struct CON_BRI_B_0_S  *CON_BRI_B_0;
  struct CON_BRI_B_1_S  *CON_BRI_B_1;
  struct CON_BRI_B_2_S  *CON_BRI_B_2;

  struct POST_PADDING_VALUE_S *POST_PADDING_VALUE;
  struct POST_PADDING_EXT_S   *POST_PADDING_EXT;

}VPP1686_PRO_PARAM;

  /*random_src :
    0: use the original source;
    1: generate and use fixed pattern;
    2: generate and use random source;
    */
  /*csc_path:
    0: yuv to yuv, or rgb to rgb, no need todo color space conversion;
    1: yuv to rgb;
    2: rgb to yuv;
    3: rgb to hsv
    */
  struct USER_DEF_S {
    uint32 random_src : 2;
    uint32 csc_path : 4;
    uint32 rsv : 28;
  };

typedef struct vpp1686_param {
  struct USER_DEF_S        USER_DEF;
  uint32        PROJECT;
  uint32        process_mode;  // 0:4P mode, 1:4N mode

  VPP1686_PRO_PARAM processor_param[4];
} VPP1686_PARAM;

typedef struct crop_param_s {
  uint32 font_en;
  uint32 rect_en;
  uint32 src_crop_st_x;
  uint32 src_crop_st_y;
  uint32 src_crop_sz_w;
  uint32 src_crop_sz_h;
  uint32 dst_crop_st_x;
  uint32 dst_crop_st_y;
  uint32 dst_crop_sz_w;
  uint32 dst_crop_sz_h;
  float scale_w;
  float scale_h;
  uint32 top;
  uint32 bottom;
  uint32 left;
  uint32 right;
  struct SRC_FONT_PITCH_S        SRC_FONT_PITCH;
  struct SRC_FONT_VALUE_S        SRC_FONT_VALUE;
  struct SRC_FONT_SIZE_S        SRC_FONT_SIZE;
  struct SRC_FONT_ST_S        SRC_FONT_ST;
  uint64                      SRC_FONT_ADDR;
  struct SRC_BORDER_VALUE_S        SRC_BORDER_VALUE;
  struct SRC_BORDER_SIZE_S        SRC_BORDER_SIZE;
  struct SRC_BORDER_ST_S        SRC_BORDER_ST;
} crop_param;


/*tensorflow::ops::CropAndResize random*/
  struct TF_DST_CROP_SIZE_S {
    uint32 tf_dst_crop_width : 14;
    uint32 rsv1 : 2;
    uint32 tf_dst_crop_height : 14;
    uint32 rsv2 : 2;
  };

  struct TF_BASE_ADDR_EXT_S {
    uint32 tf_src_base_addr_ext : 8;
    uint32 tf_dst_base_addr_ext : 8;
    uint32 tf_box_base_addr_ext : 8;
    uint32 tf_box_indice_base_addr_ext : 8;
  };

  struct TF_EXT_VALUE_S {
    union{
      char ext_value_int8;
      unsigned char ext_value_uint8;
    }tf_ext_value;
    uint32 tf_next_base_addr_ext : 8;
    uint32 rsv1 : 24;
  };

  struct TF_METHOD_S {
    uint32 tf_resize_method : 1;
    uint32 tf_uint8 : 1;
    uint32 rsv1 : 30;
  };

  struct TF_SRC_STRIDE_H_S {
    uint32 tf_src_stride_h : 14;
    uint32 rsv1 : 18;
  };

  struct TF_SRC_STRIDE_N_EXT_S {
    uint32 tf_src_stride_n_ext : 8;
    uint32 rsv1 : 24;
  };

  struct TF_DST_STRIDE_H_S {
    uint32 tf_dst_stride_h : 14;
    uint32 rsv1 : 18;
  };

  struct TF_DST_STRIDE_N_EXT_S {
    uint32 tf_dst_stride_n_ext : 8;
    uint32 rsv1 : 24;
  };

typedef struct tf_vpp1686_param {
  uint32 TF_TENSOR_NUM;
  uint32 TF_SRC_BASE_ADDR;
  uint32 TF_DST_BASE_ADDR;
  uint32 TF_BOX_BASE_ADDR;
  uint32 TF_BOX_INDICE_BASE_ADDR;
  uint32 TF_SRC_N;
  uint32 TF_SRC_C;
  uint32 TF_SRC_H;
  uint32 TF_SRC_W;
  struct TF_DST_CROP_SIZE_S TF_DST_CROP_SIZE;
  struct TF_BASE_ADDR_EXT_S TF_BASE_ADDR_EXT;
  struct TF_EXT_VALUE_S TF_EXT_VALUE;
  uint32 TF_NEXT_BASE_ADDR;
  uint32 TF_NUM_BOXES;
  struct TF_METHOD_S TF_METHOD;
  struct TF_SRC_STRIDE_H_S TF_SRC_STRIDE_H;
  uint32 TF_SRC_STRIDE_C;
  uint32 TF_SRC_STRIDE_N;
  struct TF_SRC_STRIDE_N_EXT_S TF_SRC_STRIDE_N_EXT;
  struct TF_DST_STRIDE_H_S TF_DST_STRIDE_H;
  uint32 TF_DST_STRIDE_C;
  uint32 TF_DST_STRIDE_N;
  struct TF_DST_STRIDE_N_EXT_S TF_DST_STRIDE_N_EXT;
} TF_VPP1686_PARAM;

//------------------------------------------------------------------------------
#define VPP_OK (0)
#define VPP_ERR (-1)

#define VppAssert(cond) do { if (!(cond)) {printf("[vppassert]:%s<%d> : %s\n", __FILE__, __LINE__, #cond); abort();}} while (0)
#define VppAssertDbg(cond) do { if (!(cond)) {printf("[vppassertdbg]:%s<%d> : %s\n", __FILE__, __LINE__, #cond);}} while (0)



#define VPP_MASK_ERR    (0x1)
#define VPP_MASK_WARN    (0x2)
#define VPP_MASK_INFO    (0x4)
#define VPP_MASK_DBG    (0x8)
#define VPP_MASK_TRACE    (0x100)

int vpp_level_cmodel = VPP_MASK_ERR | VPP_MASK_WARN | VPP_MASK_INFO | VPP_MASK_DBG;


//#define VPP_MSG
#ifdef VPP_MSG
#define VppErr(msg, ... )    if (vpp_level_cmodel & VPP_MASK_ERR)    { printf("[ERR] %s : %s = %d, ", __FILE__, __FUNCTION__, __LINE__); printf(msg, ## __VA_ARGS__); abort();}
#define VppWarn(msg, ... )    if (vpp_level_cmodel & VPP_MASK_WARN)    { printf("[WARN] %s :%s = %d, ", __FILE__, __FUNCTION__, __LINE__); printf(msg, ## __VA_ARGS__);}
#define VppInfo(msg, ...)    if (vpp_level_cmodel & VPP_MASK_INFO)    { printf("[INFO] %s :%s = %d, ", __FILE__, __FUNCTION__, __LINE__); printf(msg, ## __VA_ARGS__);}
#define VppDbg(msg, ...)    if (vpp_level_cmodel & VPP_MASK_DBG)    { printf("[DBG] %s :%s = %d, ", __FILE__, __FUNCTION__, __LINE__); printf(msg, ## __VA_ARGS__);}
#define VppTrace(msg, ...)    if (vpp_level_cmodel & VPP_MASK_TRACE)    { printf("[TRACE] %s :%s = %d, ", __FILE__, __FUNCTION__, __LINE__, printf(msg, ## __VA_ARGS__);}
#else
#define VppErr(msg, ... )    if (vpp_level_cmodel & VPP_MASK_ERR)    { printf("[ERR] %s : %s = %d, ", __FILE__, __FUNCTION__, __LINE__); printf(msg, ## __VA_ARGS__); abort();}
#define VppWarn(msg, ... )
#define VppInfo(msg, ...)
#define VppDbg(msg, ...)
#define VppTrace(msg, ...)
#endif


//#define PRINT_C_MSG

#define WRITE_DEBUG_FILES
#ifdef WRITE_DEBUG_FILES
#if 0
  #define WRITE_DMA_IN
  #define WRITE_DMA_OUT
  #define WRITE_DEBUG_LIST
  #define WRITE_INPUT_CH_FILE


  #define WRITE_SRC_CROP_FILE
  #define WRITE_UP_SAMPLING_FILE
  #define WRITE_PADDING_FILE
  #define WRITE_FONT_FILE
  #define WRITE_BORDER_FILE
  #define WRITE_SCALING_FILE
  #define WRITE_CSC_FILE
  #define WRITE_NOR_FILE
  #define WRITE_CON_BRI_FILE
  #define WRITE_DS_FILE
  #define WRITE_DDR_OUT
  #define WRITE_DDR_IN
  #define WRITE_OUTPUT_BMP
#endif
#endif

//------------------------------------------------------------------------------
#define INPUT_PARAM_LIST         "input_param_list"
#define OUTPUT_IMAGE             "output_image_"
#define VPP_IN        "vpp_in"
#define SRC_CROP             "src_crop_"
#define UP_SAMPLING             "up_sampling_"
#define PADDING             "padding_"
#define SCALING             "scaling_"
#define CSC             "csc_"
#define NOR             "nor_"
#define CON_BRI         "con_bri_"
#define DOWN_SAMPLE             "ds_"
#define OUT             "out_"
#define IMG             "img_"
#define CROP             "crop"
#define DDR_IN                   "ddr_i.dat"
#define DDR_OUT                  "ddr_o_"
#define VIDEO_REG                "video_reg.dat"
#define LMEM_ARRANGE_INDEX_TABLE "mem_arrange_index_table.dat"

#define VPP1686_PARAM_LIST       "input_param_list"
#define VPP1684_XCOE             "x_coe_list"
#define VPP1684_YCOE             "y_coe_list"
#define VPP1686_REG              "video_reg.dat"

#define RANDOM_INPUT_LINEAR  "./random_input_linear.bin"

#define LINEAR_INPUT1        "./random_input_linear.bin"
#define LINEAR_INPUT2        "./random_input_linear.bin"

#define I_INPUT_IMAGE       "/bm/AItoIC/video/tv_gen/random-1080p.bin"
#define I_INPUT_IMAGE_FHD   "/bm/AItoIC/video/tv_gen/random-1080p.bin"

#define Out_Lists_DIR       "./"

#define IN_FILE_CH_0        "cm_ch0_in.dat"
#define IN_FILE_CH_1        "cm_ch1_in.dat"
#define IN_FILE_CH_2        "cm_ch2_in.dat"

#define OUT_FILE_CH_0       "cm_ch0_"
#define OUT_FILE_CH_1       "cm_ch1_"
#define OUT_FILE_CH_2       "cm_ch2_"
#define OUT_FILE_CH_3       "cm_ch3_"

#define OUT_DMA_FILE_CH_0   "dma_write.0.c.dat"
#define OUT_DMA_FILE_CH_1   "dma_write.1.c.dat"
#define OUT_DMA_FILE_CH_2   "dma_write.2.c.dat"

#define OUT_DMA_FILE        "dma_write_crop."
#define FONT_BITMAP_ALPHA_BLEND        "./long_blendAlpha.bin"
#define FONT_BITMAP_BINARY        "./long_binary.bin"
//#define FONT_BITMAP_ALPHA_BLEND        "he_blendAlpha.bin"
//#define FONT_BITMAP_BINARY        "he.bin"
//------------------------------------------------------------------------------
#define CSC_WL    (20)
#define SHIFT    (0)
#define FRAC_WL    (14)  // fraction bits word length
#define FP_WL    (14)  // fixed point word length
#define Ratio    (4)

//------------------------------------------------------------------------------
// BM1686 src format and dst format
//------------------------------------------------------------------------------
#if 0
enum VPP_INPUT_FORMAT {
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
  IN_FBC = 31,  // FBC enable, when FBC enale, input format only support NV12
  IN_FORMAT_MAX = 32
};

enum VPP_OUTPUT_FORMAT {
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
  OUT_NCHW = 24,
  OUT_FORMAT_MAX = 25
};
#endif
enum VPP_CSC_MODE {
  YUV2RGB_709_F = 0,
  YUV2RGB_709_NF = 1,
  YUV2RGB_601_F = 2,
  YUV2RGB_601_NF = 3,
  RGB2YUV_709_F = 4,
  RGB2YUV_709_NF = 5,
  RGB2YUV_601_F = 6,
  RGB2YUV_601_NF = 7,
  CSC_MODE_PROGRAMMABLE = 8,
  CSC_MODE_MAX = 11
};

/*
*color_space_in : 0: rgb, 1: yuv
*
*color_space_out : 0: rgb, 1: yuv, 2: hsv
*csc_path: 0: yuv to yuv, or rgb to rgb; 1: yuv to rgb; 2: rgb to yuv; 3: rgb to hsv;
*/
#define COLOR_SPACE_IN_RGB        (0)
#define COLOR_SPACE_IN_YUV        (1)
#define COLOR_SPACE_OUT_RGB        (0)
#define COLOR_SPACE_OUT_YUV        (1)
#define COLOR_SPACE_OUT_HSV        (2)
#define CSC_PATH_RGB_RGB        (0)
#define CSC_PATH_YUV_YUV        (0)
#define CSC_PATH_YUV_RGB        (1)
#define CSC_PATH_RGB_YUV        (2)
#define CSC_PATH_RGB_HSV        (3)
#define CSC_PATH_YUV_HSV        (4)
//------------------------------------------------------------------------------
// BM1686 Scaling algorithm
//------------------------------------------------------------------------------
enum SCALING_ALGORITHM {
  BILINEAR_CENTER = 0,
  NEAREST_FLOOR = 1,
  NEAREST_ROUND = 2,
  SCALING_ALGORITHM_MAX = 3
};

//------------------------------------------------------------------------------
// BMP image related definition
//------------------------------------------------------------------------------
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef int                 LONG;
typedef unsigned char       BYTE;

typedef struct tagBITMAPFILEHEADER {
    WORD          bfType;
    DWORD         bfSize;
    WORD          bfReserved1;
    WORD          bfReserved2;
    DWORD         bfOffBits;
}__attribute__((packed, aligned(1))) BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    DWORD         biSize;
    LONG          biWidth;
    LONG          biHeight;
    WORD          biPlanes;
    WORD          biBitCount;
    DWORD         biCompression;
    DWORD         biSizeImage;
    LONG          biXPelsPerMeter;
    LONG          biYPelsPerMeter;
    DWORD         biClrUsed;
    DWORD         biClrImportant;
}__attribute__((packed, aligned(1))) BITMAPINFOHEADER;

typedef struct tagRGBQUAD {
    BYTE rgbRed;
    BYTE rgbGreen;
    BYTE rgbBlue;
    BYTE rgbReserved;
}__attribute__((packed, aligned(1))) RGBQUAD;

//------------------------------------------------------------------------------
// BM1686 Image resolution
//------------------------------------------------------------------------------
#define MAX_WIDTH        (8192)
#define MAX_HEIGHT       (8192)

#define MIN_WIDTH        (8)
#define MIN_HEIGHT       (8)
#define STRIDE_EXT        (128)
#define MAX_CROP_NUM        (256)
#define PADDING_PIX_MAX        (256)
#define SCALING_RATIO_FRACTION_BITS        (14)
#define SCALING_RATIO_FRACTION_MASK        (0x3FFF)
#define SCALING_RATION_INTEGER_MASK        (0x3FC000)
#define DATA_RANGE_FULL_RANGE        (1)
#define DATA_RANGE_NOT_FULL_RANGE        (0)
#if 0
#define NOR_R_ALPHA        (102.9801)
#define NOR_G_ALPHA        (115.9465)
#define NOR_B_ALPHA        (122.7717)
#define NOR_BETA        (128 / 152.094)
#endif
#define NOR_BETA        (128 / 152.094)
#define NOR_R_ALPHA        (102.9801 * NOR_BETA)
#define NOR_G_ALPHA        (115.9465 * NOR_BETA)
#define NOR_B_ALPHA        (122.7717 * NOR_BETA)
#define NOR_BETA_WL        (8)  // nor beta word length

#define FULL_RANGE_MAX        (255)
#define FULL_RANGE_MIN        (0)
#define NOT_FULL_RANGE_MAX_Y        (235)
#define NOT_FULL_RANGE_MAX_UV        (240)
#define NOT_FULL_RANGE_MIN_YUV        (16)

#define INT8_MIN        (-128)
#define INT8_MAX        (127)

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------
#endif  // __COMMON_H__

#endif