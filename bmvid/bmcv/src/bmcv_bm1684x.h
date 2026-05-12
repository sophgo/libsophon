//#ifndef BMCV_COMMON_H
//#define BMCV_COMMON_H

#include "bmcv_api_ext.h"
#include <stddef.h>
#include <stdint.h>

#ifdef _MSC_VER
    #define ALIGN_STRUCT __declspec(align(4))
#else
    #define ALIGN_STRUCT __attribute__((aligned(4)))
#endif

#if defined (__cplusplus)
extern "C" {
#endif

#define sg_min(x, y) (((x)) < ((y)) ? (x) : (y))
#define sg_max(x, y) (((x)) > ((y)) ? (x) : (y))

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char  s8;
typedef signed short s16;
typedef signed int   s32;
typedef signed long long int s64;

#pragma pack(push, 1)

typedef struct {
  u64 input_proposal_addr;
  u64 output_proposal_addr;
  u64 all_mask_addr;
  u64 iou_addr;
  int nms_type; //0:hard nms, 1:soft nms 2:ADAPTIVE_NMS 3:SSD_NMS
  int proposal_size;
  float nms_threshold;

  //below pararemter is used for soft-nms, ADAPTIVE_NMS, ssd_nms
  float score_threshold;
  float sigma;
  int   weighting_method;
  float eta;
  int hard_nms_version; //0: v1, 1: v2
  int keep_top_k; //just for hard_nms v2
} sg_api_nms_t;

typedef struct sgcv_affine_matrix_s{
    float m[6];
} sgcv_affine_matrix;

typedef struct sg_api_cv_warp {
  u64 input_image_addr;
  u64 output_image_addr;
  u64 index_image_addr;
  u64 input_image_addr_align;
  u64 out_image_addr_align;
  sgcv_affine_matrix m;
  int src_w_stride;
  int dst_w_stride;
  int image_n;
  int image_c;
  int image_sh;
  int image_sw;
  int image_dh;
  int image_dw;
  int type;
} sg_api_cv_warp_t;

typedef struct {
  u64 input_image_addr;
  u64 output_image_addr;
  u64 index_image_addr;
  u64 input_image_addr_align_r;
  u64 input_image_addr_align_g;
  u64 input_image_addr_align_b;
  u64 out_image_addr_align_a;
  u64 out_image_addr_align_b;
  u64 system_image_addr_lu;
  u64 system_image_addr_ld;
  u64 system_image_addr_ru;
  u64 system_image_addr_rd;
  bm_image_data_format_ext input_format;
  bm_image_data_format_ext output_format;
  sgcv_affine_matrix m;
  int image_c;
  int image_sh;
  int image_sw;
  int image_dh;
  int image_dw;
  int type;
} sg_api_cv_warp_bilinear_t;

typedef struct {
  u64 input_image_addr;
  u64 output_image_addr;
  u64 index_image_addr;
  u64 input_image_addr_align_r;
  u64 input_image_addr_align_g;
  u64 input_image_addr_align_b;
  u64 out_image_addr_align_a;
  u64 out_image_addr_align_b;
  u64 system_image_addr_lu;
  u64 system_image_addr_ld;
  u64 system_image_addr_ru;
  u64 system_image_addr_rd;
  bm_image_data_format_ext input_format;
  bm_image_data_format_ext output_format;
  sgcv_affine_matrix m;
  int image_c;
  int image_sh;
  int image_sw;
  int image_dh;
  int image_dw;
  int padding_r;
  int padding_g;
  int padding_b;
  int type;
} sg_api_cv_warp_bilinear_padding_rgb_t;

typedef struct sgcv_perspective_matrix_s{
    float m[9];
} sgcv_perspective_matrix;

typedef struct sg_api_cv_warp_perspective_1684x {
  u64 input_image_addr;
  u64 output_image_addr;
  u64 index_image_addr;
  u64 input_image_addr_align;
  u64 out_image_addr_align;
  sgcv_perspective_matrix m;
  int src_w_stride;
  int dst_w_stride;
  int image_n;
  int image_sh;
  int image_sw;
  int image_dh;
  int image_dw;
  int type;
} sg_api_cv_warp_perspective_1684x_t;

typedef struct sg_api_cv_remap {
    u64 input_addr[3];
    u64 output_addr[3];
    u64 mapx_addr;
    u64 mapy_addr;
    int channel;
    int input_width;
    int input_height;
    int output_width;
    int output_height;
    int interpolation_mode;
} sg_api_cv_remap_t;

typedef struct sg_api_yolo_detect_out{
    u64 b0_global_addr;
    u64 b1_global_addr;
    u64 b2_global_addr;
    u64 buffer_global_offset;
    u64 top_global_addr;
    u64 all_mask_ddr_addr; //for to support large box, need to alloc ddr at outside
    int input_num;
    int batch_num;
    int hw_shape[3][2];
    int num_classes;
    int num_boxes;
    int mask_group_size;
    int keep_top_k;
    float nms_threshold;
    float confidence_threshold;
    float bias[18];
    float anchor_scale[3];
    float mask[9];
    int yolo_flag; //0: yolov3 1:yolov5 2:yolov7
    int len_per_batch;
    int scale;//for yolov7 post hanle
    int orig_image_shape[100]; //reserved 800 bytes
    int model_h;
    int model_w;
} sg_api_yolo_detect_out_1684x_t;

typedef struct {
    u64 input_data_addr;
    u64 input_index_addr;
    u64 output_data_addr;
    u64 output_index_addr;
    u64 buffer_addr;
    int input_index_valid;
    int k;
    int descending;
    int batchs;
    int batch_num;
    int batch_stride;
    int dtype;
} sg_api_topk_1684x_t;

typedef struct {
    u64 input_query_global_offset;
    u64 input_database_global_offset;
    u64 output_global_offset;
    int bits_len;
    int left_row;
    int right_row;
} sg_api_hamming_distance_t;

typedef struct {
  u64 input_global_offset;
  u64 aux_input0_global_offset;
  u64 aux_input1_global_offset;
  u64 yuv444_global_offset;
  u64 cropped_global_offset[4];
  u64 resize_global_offset[4];
  u64 output_global_offset[4];
  u64 aux_output0_global_offset[4];
  u64 aux_output1_global_offset[4];
  int roi_num;
  int start_x[4];
  int start_y[4];
  int src_h;
  int src_w;
  int roi_h;
  int roi_w;
  int dst_h;
  int dst_w;
  u32 src_format;
  u32 dst_format;
  int src_data_type;
  int dst_data_type;
  unsigned char stretch_fit;
  unsigned int  interpolation;
  int padding_b;
  int padding_g;
  int padding_r;
} sg_api_crop_resize_t;

#pragma pack(push, 4)
struct sg_api_cv_fft_t {
    u64   XR;
    u64   XI;
    u64   YR;
    u64   YI;
    u64   ER;
    u64   EI;
    int   batch;
    int   len;
    bool  forward;
    bool  realInput;
    int  trans;
    int   factorSize;
    int   factors[15];
    bool normalize;
};
#pragma pack(pop)

typedef struct bm_api_cv_feature_match_1684x {
  u64 input_data_global_addr;
  u64 db_data_global_addr;
  u64 db_feature_global_addr;
  u64 db_data_trans_addr;
  u64 output_temp_addr;
  u64 output_sorted_similarity_global_addr;
  u64 output_sorted_index_global_addr;
  int batch_size;
  int feature_size;
  int db_size;
} bm_api_cv_feature_match_1684x_t;

typedef struct bm_api_cv_feature_match_fix8b_1684x_st {
  u64 input_data_global_addr;
  u64 db_data_global_addr;
  u64 output_similarity_global_addr;
  u64 output_sorted_index_global_addr;
  u64 arm_tmp_buffer;
  int batch_size;
  int feature_size;
  int db_size;
  int sort_cnt;
  int rshiftbits;
} bm_api_cv_feature_match_fix8b_1684x_t;

typedef struct bm_api_cv_bayer2rgb {
    int width;
    int height;
    int dst_fmt;
    int src_type;
    u64 input_addr;
    u64 output_addr;
    u64 sys_mem_addr_temp_ul;
    u64 sys_mem_addr_temp_br;
    u64 sys_mem_addr_temp_g;
    u64 sys_mem_addr_temp_b;
    u64 input_addr_padding_up_left;
    u64 input_addr_padding_bottom_right;
    u64 kernel_addr;
} bm_api_cv_bayer2rgb_t;

typedef struct bm_api_cv_distance_1684x {
  u64 Xaddr;
  u64 Yaddr;
  int dim;
  float pnt[8];
  int len;
  int dtype;
} bm_api_cv_distance_1684x_t;

typedef struct bm_api_cv_as_strided
{
    u64 input_addr;
    u64 output_addr;
    int input_row;
    int input_col;
    int output_row;
    int output_col;
    int row_stride;
    int col_stride;
} bm_api_cv_as_strided_t;

typedef struct bm_matrix_log{
  u64 s_addr;
  u64 d_addr;
  int row;
  int col;
  int s_dtype;
  int d_dtype;
}bm_matrix_log_t;

typedef struct bm_api_cv_hist_balance1 {
    u64 Xaddr;
    int len;
    u64 cdf_addr;
} bm_api_cv_hist_balance_t1;

typedef struct bm_api_cv_hist_balance2 {
    u64 Xaddr;
    int len;
    float cdf_min;
    u64 cdf_addr;
    u64 Yaddr;
} bm_api_cv_hist_balance_t2;

typedef struct bm_api_cv_lap_matrix {
    u64 input_device_addr;
    u64 output_device_addr;
    u64 const_val_addr;
    u64 res_addr;
    int row;
    int col;
} bm_api_cv_lap_matrix;

#define FW_MAX_SHAPE_DIMS 8
typedef struct bm_api_cv_knn {
    u64 centroids_global_addr;
    u64 labels_global_addr;
    u64 input_global_addr;
    u64 weight_global_addr;
    u64 buffer_global_addr;
    int  Shape_Input[FW_MAX_SHAPE_DIMS];
    int  Shape_Weight[FW_MAX_SHAPE_DIMS];
    int dims_Input;
    int dims_Weight;
    int k;
    int num_iter;
    int dtype;
} bm_api_cv_knn_t;

typedef struct bm_api_cv_knn2 {
    u64 ref_data_dev_mem;
    u64 test_data_dev_mem;
    u64 dist_to_sort_dev_mem;
    u64 distance_tpu_dev_mem;
    u64 indices_tpu_dev_mem;
    int n_test;
    int n_ref;
    int n_feat;
    int k;
} bm_api_cv_knn2_t;

typedef struct bm_api_cv_knn_match {
    u64 ref_data_dev_mem;
    u64 test_data_dev_mem;
    u64 dist_to_sort_dev_mem;
    u64 distance_tpu_dev_mem;
    u64 good_match_dev_mem;
    u64 index_sorted_dev_mem;
    int n_ref;
    int n_ref_feat;
    int n_test_feat;
    int n_descriptor;
    int k;
    float ratio_thresh;
} bm_api_cv_knn_match_t;

typedef struct sg_api_cv_rotate {
  int channel;
  int rotation_angle;
  u64 input_addr[3];
  u64 output_addr[3];
  int width;
  int height;
} sg_api_cv_rotate_t;

typedef struct sg_api_cv_cos_similarity {
    unsigned long long input_data_global_addr;
    unsigned long long output_data_global_addr;
    unsigned long long normalized_data_global_addr;
    int vec_num;
    int vec_dims;
} sg_api_cv_cos_similarity_t;

typedef struct sg_api_cv_matrix_prune {
    unsigned long long input_data_global_addr;
    unsigned long long output_data_global_addr;
    unsigned long long sort_data_global_addr;
    unsigned long long sort_index_global_addr;
    unsigned long long sort_trans_global_addr;
    int matrix_dims;
    float p;
} sg_api_cv_matrix_prune_t;


typedef struct sg_api_cv_overlay {
  int overlay_num;
  unsigned long long base_addr;
  unsigned long long overlay_addr[10];
  int base_width;
  int base_height;
  int overlay_width[10];
  int overlay_height[10];
  int pos_x[10];
  int pos_y[10];
  int format;
} sg_api_cv_overlay_t;

typedef struct sg_api_cv_sobel_t {
    int channel;
    unsigned long long input_addr[3];
    unsigned long long kernel_addr;
    unsigned long long output_addr[3];
    int width[3];
    int height[3];
    int kw;
    int kh;
    int stride_i[3];
    int stride_o[3];
    float delta;
    int is_packed;
    int out_type;
} sg_api_cv_sobel_t;

typedef struct sg_api_cv_gaussian_blur {
  int channel;
  u64 input_addr[3];
  u64 kernel_addr;
  u64 output_addr[3];
  int width;
  int height;
  int kw;
  int kh;
  int stride_i;
  int stride_o;
  float delta;
  int is_packed;
  int out_type;
} sg_api_cv_gaussian_blur_t;
#pragma pack(pop)

#pragma pack(push, 4)
typedef struct sg_api_cv_surf_response {
	int width;
	int height;
	unsigned long long surf_img_addr;
	int rl_width[12];
	int rl_height[12];
	int rl_step[12];
	int rl_filter[12];
	unsigned long long responses_addr[12];
	unsigned long long laplacian_addr[12];
	unsigned long long BoxFilterKernel_addr[12];
}sg_api_cv_surf_response;

#pragma pack(pop)
}

#pragma pack(push, 1)
typedef struct sg_api_cv_assigned_area_blur {
    int base_width;
    int base_height;
    int ksize;
    int channel;
    int assigned_area_num;
    int assigned_area_width[200];
    int assigned_area_height[200];
    int start_x[200];
    int start_y[200];
    unsigned long long base_addr[3];
    unsigned long long output_addr[3];
    unsigned long long kernel_sys_addr;
} sg_api_cv_assigned_area_blur_t;
#pragma pack(pop)

typedef struct sg_api_cv_canny_sobel_t {
    int channel;
    unsigned long long input_addr[3];
    unsigned long long kernel_addr;
    unsigned long long output_addr[3];
    int width[3];
    int height[3];
    int kw;
    int kh;
    int stride_i[3];
    int stride_o[3];
    float delta;
    int is_packed;
}ALIGN_STRUCT sg_api_cv_canny_sobel_t;
typedef struct sg_api_cv_canny_mag_t {
    u64 dx_addr;
    u64 dy_addr;
    u64 d_addr;
    bool l2gradient;
    int size;
}ALIGN_STRUCT sg_api_cv_canny_mag_t;

typedef struct sg_api_cv_canny_double_thred_t {
    u64 mag_addr;
    float low_threshold;
    float high_threshold;
    int size;
}ALIGN_STRUCT sg_api_cv_canny_double_thred_t;

typedef struct sg_api_cv_count_nonzero{
    int width;
    int height;
    int channel;
    u64 input_addr;
    u64 nonzero_count_addr;
    u64 nonzero_idx_addr;
}ALIGN_STRUCT sg_api_cv_count_nonzero_t;

typedef struct sg_api_add_mask_to_image
{
  unsigned long long input_addr;
  unsigned long long output_addr;
  int width;
  int height;
  int mask_num;
  mask_info_t mask_info[4];
} ALIGN_STRUCT sg_api_add_mask_to_image_t;

// #endif
