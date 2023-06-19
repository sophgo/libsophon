//#ifndef BMCV_COMMON_H
//#define BMCV_COMMON_H

#include "bmcv_api_ext.h"
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

typedef struct sg_api_yolo_detect_out{
    u64 b0_global_addr;
    u64 b1_global_addr;
    u64 b2_global_addr;
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

#pragma pack(pop)

}
// #endif