#ifndef BMCV_COMMON_H
#define BMCV_COMMON_H


#ifdef __x86_64__
void print_trace(void);
#else
#define print_trace()                                                                              \
  do {                                                                                             \
  } while (0)
#endif


#if defined (__cplusplus)
extern "C" {
#endif

#ifndef FW_MAX_SHAPE_DIMS
#define FW_MAX_SHAPE_DIMS 8
#endif  // FW_MAX_SHAPE_DIMS


#ifdef __linux__
#define EU_SHIFT                (CONFIG_EU_SHIFT)
#define NPU_SHIFT               (CONFIG_NPU_SHIFT)
#else
#define EU_SHIFT                5
#define NPU_SHIFT               6

#define CONFIG_LOCAL_MEM_ADDRWIDTH 19
#endif

#ifndef USING_CMODEL
#define EU_NUM                  (1<<EU_SHIFT)
#define NPU_NUM                 (1<<NPU_SHIFT)
#else
#define NPU_NUM 32
#define EU_NUM 64
#endif


#define LOCAL_MEM_SIZE          (1<<CONFIG_LOCAL_MEM_ADDRWIDTH)
#define LOCAL_MEM_BANKS 8
#define LOCAL_BANK_SIZE (LOCAL_MEM_SIZE / LOCAL_MEM_BANKS)



// For dynamic IR info
#define DYNAMIC_IR_OFFSET    0x22000
#define DYNAMIC_IR_MAX_SIZE  0x80000  // 512KB

#ifdef BM1686
#define DYNAMIC_IR_MAX_SIZE  0x40000 // 256KB
#endif

#define WARP_MAX_W (256)
#define WARP_MAX_H (256)

#define IS_NAN(x) ((((x >> 23) & 0xff) == 0xff) && ((x & 0x7fffff) != 0))
#define ABS(x) ((x) > 0 ? (x) : (-(x)))

#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

#ifdef __linux__
#define ALIGN(x, a) __ALIGN_MASK(x, (__typeof__(x))(a)-1)
#else
#define ALIGN(x, a) __ALIGN_MASK(x, (int)(a)-1)
#endif

typedef union {
  int ival;
  float fval;
} IF_VAL;



#define L2_SRAM_START_ADDR 0x10000000
// The following is allocation for l2 sram
// For lookup table
#define SFU_TABLE_SIZE              256
#define INT2FLOAT_TABLE_SIZE        256
#define UINT2FLOAT_TABLE_SIZE       INT2FLOAT_TABLE_SIZE
#define SFU_TAILOR_TABLE_SIZE       32
#define SFU_TAILOR_L_TABLE_SIZE     64
#define UNIFIED_LOOKUP_TABLE_SIZE   256
#define SERIAL_NUMBER_SIZE          2048

#define WARP_MAX_W                  (256)
#define WARP_MAX_H                  (256)
#define WARP_TABLE_SIZE             (WARP_MAX_W * WARP_MAX_H * 2)
#define EX_INT_TABLE_OFFSET         0x300000
#define EX_TABLE_OFFSET             (EX_INT_TABLE_OFFSET + SFU_TABLE_SIZE * sizeof(float))
#define LNX_TABLE_OFFSET            (EX_TABLE_OFFSET + SFU_TABLE_SIZE * sizeof(float))
#define EX_TAILOR_OFFSET            (LNX_TABLE_OFFSET + SFU_TABLE_SIZE * sizeof(float))
#define LNX_TAILOR_OFFSET           (EX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define ARCSINX_TAILOR_OFFSET       (LNX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define UINT2FLOAT_TABLE_OFFSET     (ARCSINX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define INT2FLOAT_TABLE_OFFSET      (UINT2FLOAT_TABLE_OFFSET + UINT2FLOAT_TABLE_SIZE * sizeof(float))
#define SINX_TAILOR_OFFSET          (INT2FLOAT_TABLE_OFFSET + INT2FLOAT_TABLE_SIZE * sizeof(float))
#define SERIAL_NUMBER_OFFSET        (SINX_TAILOR_OFFSET + SFU_TAILOR_L_TABLE_SIZE * sizeof(float))
#define WARP_TABLE_OFFSET           (SERIAL_NUMBER_OFFSET + SERIAL_NUMBER_SIZE * sizeof(float))
#define UNIFIED_LOOKUP_TABLE_OFFSET (WARP_TABLE_OFFSET + WARP_TABLE_SIZE)
#define L2_STATIC_END_OFFSET        (UNIFIED_LOOKUP_TABLE_OFFSET + UNIFIED_LOOKUP_TABLE_SIZE * sizeof(float))

// Start offset for L2 sram alloc
#define L2_ALLOC_START_OFFSET       (DYNAMIC_IR_OFFSET + DYNAMIC_IR_MAX_SIZE)
#define L2_ALLOC_START_OFFSET       (DYNAMIC_IR_OFFSET + DYNAMIC_IR_MAX_SIZE)
#define L2_SRAM_USER_SIZE           (EX_INT_TABLE_OFFSET - L2_ALLOC_START_OFFSET)


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char  s8;
typedef signed short s16;
typedef signed int   s32;
typedef signed long long int s64;

#define INLINE inline
#define UNUSED(x) (void)(x)

#define INT8_SIZE 1
#define FLOAT_SIZE 4

#define bm_min(x, y) (((x)) < ((y)) ? (x) : (y))
#define bm_max(x, y) (((x)) > ((y)) ? (x) : (y))

#ifdef SOC_MODE
struct ce_base {
    u32 alg;
    u32 enc;
    u64 src;
    u64 dst;
    u64 len;
    u64 dstlen;
};

#define CE_BASE_IOC_TYPE                ('C')
#define CE_BASE_IOC_IOR(nr, type)       _IOR(CE_BASE_IOC_TYPE, nr, type)
/* do one shot operation on physical address */
#define CE_BASE_IOC_OP_PHY      \
        CE_BASE_IOC_IOR(0, struct ce_base)

enum {
        CE_BASE_IOC_ALG_BASE64,
        CE_BASE_IOC_ALG_MAX,
};
#endif


#ifndef USING_CMODEL
#define hang(_ret) while (1)
#else
#define hang(_ret) exit(_ret)
#endif

#ifdef NO_PRINTF_IN_ASSERT
#define ASSERT_INFO(_cond, fmt, ...)                                                               \
  do {                                                                                             \
    if (!(_cond)) {                                                                                \
      hang(-1);                                                                                    \
    }                                                                                              \
  } while (0)
#else
#include <stdio.h>
#define ASSERT_INFO(_cond, fmt, ...)                                                               \
  do {                                                                                             \
    if (!(_cond)) {                                                                                \
      printf("ASSERT %s: %s: %d: %s\n", __FILE__, __func__, __LINE__, #_cond);                     \
      printf("ASSERT info: " fmt "\n", ##__VA_ARGS__);                                             \
      print_trace();                                                                               \
      hang(-1);                                                                                    \
    }                                                                                              \
  } while (0)
#endif

#define ASSERT(_cond) ASSERT_INFO(_cond, "none.")

typedef struct bm_api_gemm {
  bool is_A_trans;
  bool is_B_trans;
  int M;
  int N;
  int K;
  float alpha;
  u64 A_global_offset;
  int lda;
  u64 B_global_offset;
  int ldb;
  float beta;
  u64 C_global_offset;
  int ldc;
} bm_api_gemm_t;

#pragma pack(push, 1)
typedef struct bm_api_cv_calc_hist_index {
  u64 Xaddr;
  u64 Yaddr;
  float a;
  float b;
  int len;
  int xdtype;
  float upper;
} bm_api_cv_calc_hist_index_t;

typedef struct bm_api_cv_cmulp {
  u64 XR;
  u64 XI;
  u64 PR;
  u64 PI;
  u64 YR;
  u64 YI;
  int batch;
  int len;
} bm_api_cv_cmulp_t;

typedef struct bm_api_cv_yuv2hsv {
  u64 src_addr[3];
  u64 dst_addr[3];
  u32 width;
  u32 height;
  u32 stride_i;
  u32 ow;
  u32 oh;
  u32 src_format;
  u32 dst_format;
} bm_api_cv_yuv2hsv_t;

typedef struct bm_api_memcpy_tensor {
  u64 src_global_offset;
  u64 dst_global_offset;
  int n; int c; int h; int w;
  int src_nstride;
  int src_cstride;
  int src_hstride;
  int src_wstride;
  int dst_nstride;
  int dst_cstride;
  int dst_hstride;
  int dst_wstride;
  int format_bytes;
} bm_api_memcpy_tensor_t;

typedef struct bm_api_memcpy_tensors {
  int copy_num;
  u64 info_global_offset;
} bm_api_memcpy_tensors_t;

typedef struct bm_api_cv_split {
  u64 src_global_offset;
  u64 dst_global_offset;
  int src_w;
  int src_h;
  int dst_w;
  int dst_h;
  int type_size;
} bm_api_cv_split_t;


typedef enum {
  STORAGE_MODE_1N_FP32    = 0,
  STORAGE_MODE_1N_INT8    = 1,
  STORAGE_MODE_1N_INT16   = 2,
  STORAGE_MODE_2N_INT16   = 3,
  STORAGE_MODE_4N_INT8    = 4,
  STORAGE_MODE_2IC_FP32   = 5,  // special for 2IC weight
  STORAGE_MODE_4N_4IC_4OC = 6,
  STORAGE_MODE_4N_INT16   = 7,
  STORAGE_MODE_UNINITILIZED,
  STORAGE_MODE_END
} TENSOR_STORAGE_MODE;

#ifdef BM1686
typedef enum {
  STORAGE_MODE_1N_FP32 = 0,
  STORAGE_MODE_1N_INT8 = 1,
  STORAGE_MODE_1N_INT16 = 2,
  STORAGE_MODE_2N_INT16 = 3,
  STORAGE_MODE_4N_INT8 = 4,
  STORAGE_MODE_2IC_FP32 = 5, // special for 2IC weight
  STORAGE_MODE_4N_4IC_4OC = 6,
  STORAGE_MODE_UNINITILIZED,
  STORAGE_MODE_END
} TENSOR_STORAGE_MODE;
#endif

typedef struct bm_api_cv_copy_to_st {
  u64 input_image_addr;
  u64 output_image_addr;
  u64 padding_image_addr;
  int C;
  int input_w_stride;
  int input_w;
  int input_h;
  int padding_w_stride;
  int padding_w;
  int padding_h;
  int data_type;
  int bgr_or_rgb;
  int planner_or_packed;
  int padding_b;
  int padding_g;
  int padding_r;
  int if_padding;
} bm_api_cv_copy_to_t;


typedef struct bm_api_cv_feature_match {
  u64 input_data_global_addr;
  u64 db_data_global_addr;
  u64 db_feature_global_addr;
  u64 output_sorted_similarity_global_addr;
  u64 output_sorted_index_global_addr;
  int batch_size;
  int feature_size;
  int db_size;
} bm_api_cv_feature_match_t;

typedef struct bm_api_cv_feature_match_fix8b_st {
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
} bm_api_cv_feature_match_fix8b_t;

typedef struct bm_api_cv_filter {
  int channel;
  u64 input_addr[3];
  u64 kernel_addr;
  u64 output_addr[3];
  int width[3];
  int height[3];
  int kw;
  int kh;
  int stride_i[3];
  int stride_o[3];
  float delta;
  int is_packed;
  int out_type;
} bm_api_cv_filter_t;

typedef struct bm_api_cv_canny {
  u64 input_addr;
  u64 kernel_addr;
  u64 output_addr;
  int width;
  int height;
  int ks;
  int stride_i;
  int stride_o;
  float low_thresh;
  float high_thresh;
  int l2gradient;
} bm_api_cv_canny_t;

typedef struct bm_api_laplacian {
  int channel;
  u64 input_addr[3];
  u64 kernel_addr;
  u64 output_addr[3];
  u32 width[3];
  u32 height[3];
  int kw;
  int kh;
  u32 stride_i[3];
  u32 stride_o[3];
  float delta;
  int is_packed;
} bm_api_laplacian_t;

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
} bm_api_cv_nms_t;

typedef struct {
  u64   input_proposal_addr;
  int   proposal_size;
  int   weighting_method;
  float sigma;
  u64   overlap_output_addr;
  u64   weighting_res_output_addr;
} bm_api_cv_soft_nms_t;

typedef struct {
  u64 b0_global_addr;
  u64 b1_global_addr;
  u64 b2_global_addr;
  u64 top_global_addr;
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
  int yolov5_flag; //for yolov5 postprocess
  int len_per_batch;
} bm_api_yolov3_detect_out_t;

typedef struct {
  u64 input_global_mem_addr;
  u64 output_global_mem_addr;
  int channel;
  int height;
  int width;
  int type_len;
} bm_api_cv_transpose_t;

typedef struct {
  u64 input_global_mem_addr;
  u64 output_global_mem_addr;
  int input_shape[FW_MAX_SHAPE_DIMS];
  int order[FW_MAX_SHAPE_DIMS];
  int dims;
  int type_len;
  int store_mode;
  u64 buffer_global_mem_addr;
} bm_api_transpose_t;

typedef struct {
  u64 input_img_addr;
  int img_w;
  int img_w_stride;
  int img_h;
  float alpha_0;
  float beta_0;
  float alpha_1;
  float beta_1;
  float alpha_2;
  float beta_2;
  int convert_storage_mode;
  int image_num;
  int input_img_data_type;
  int output_img_data_type;
  u64 output_img_addr;
} bm_api_cv_convert_to_t;

typedef struct {
  u64 input_img_addr_0;
  u64 output_img_addr_0;
  u64 input_img_addr_1;
  u64 output_img_addr_1;
  u64 input_img_addr_2;
  u64 output_img_addr_2;
  u64 convert_to_attr_addr;
  int times;
} bm_api_cv_convert_to_inter_t;

typedef struct bm_api_cv_fft_1d {
  u64 XR;
  u64 XI;
  u64 YR;
  u64 YI;
  u64 ER;
  u64 EI;
  int batch;
  int len;
  int factors[16];
  int factorSize;
  int forward;
  int inputIsReal;
  int trans;
} bm_api_cv_fft_1d_t;

typedef struct bm_api_cv_min_max {
  u64 inputAddr;
  u64 minAddr;
  u64 maxAddr;
  int len;
  int mode;
} bm_api_cv_min_max_t;

typedef struct {
  u64 src_data_addr;
  u64 src_index_addr;
  int data_cnt;
  u64 dst_data_addr;
  u64 dst_index_addr;
  int sort_cnt;
  int order;
  int index_enable;
  int auto_index;
} bm_api_cv_sort_t;

#define BMCV_MAX_TOPK_BATCH 64
typedef struct {
  u64 bottom_global_offset;
  u64 bottom_index_global_offset;
  u64 top_value_global_offset;
  u64 top_index_global_offset;
  u64 buffer_global_offset;
  int bottom_index_valid;
  int k;
  int batch;
  int per_batch_size[BMCV_MAX_TOPK_BATCH];
  int per_batch_size_is_same;
  int batch_stride;
  int descending;
} bm_api_cv_batch_topk_t;

typedef struct bm_api_cv_lkpyramid {
  u64 prev_input_addr;
  u64 next_input_addr;
  u64 kernel_addr;
  u64 output_addr[18];
  int width;
  int height;
  int stride_i;
  int winW;
  int winH;
  int max_level;
} bm_api_cv_lkpyramid_t;

typedef struct bm_api_cv_bitwise {
  int channel;
  u64 input1_addr[3];
  u64 input2_addr[3];
  u64 output_addr[3];
  int width[3];
  int height[3];
  int input1_str[3];
  int input2_str[3];
  int output_str[3];
  int op;
} bm_api_cv_bitwise_t;

typedef struct bm_api_cv_absdiff {
  int channel;
  u64 input1_addr[3];
  u64 input2_addr[3];
  u64 output_addr[3];
  int width[3];
  int height[3];
  int input1_str[3];
  int input2_str[3];
  int output_str[3];
} bm_api_cv_absdiff_t;

typedef struct bm_api_memset_byte {
  u64 global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int stride_n;
  int stride_c;
  int stride_h;
  int stride_w;
  u32 const_val;
} bm_api_memset_byte_t;

typedef struct {
  u64 range_addr;
  u64 output_hcoeff_addr;
  u64 output_wcoeff_addr;
  u32 H;
  u32 W;
  u32 is_inversed;
} bm_api_dct_coeff_t;

typedef struct {
  u64 input_addr;
  u64 hcoeff_addr;
  u64 wcoeff_addr;
  u64 output_addr;
  u64 buffer_addr;
  u32 N;
  u32 C;
  u32 H;
  u32 W;
  u32 coeff_ready;
  u32 is_inversed;
} bm_api_dct_t;

typedef struct bm_api_cv_add_weighted {
  int channel;
  u64 input1_addr[3];
  u64 input2_addr[3];
  u64 output_addr[3];
  int width[3];
  int height[3];
  int input1_str[3];
  int input2_str[3];
  int output_str[3];
  float alpha;
  float beta;
  float gamma;
} bm_api_cv_add_weighted_t;

typedef struct bm_api_cv_threshold {
  int channel;
  u64 input_addr[3];
  u64 output_addr[3];
  int width[3];
  int height[3];
  int input_str[3];
  int output_str[3];
  int type;
  u32 thresh;
  u32 max_value;
} bm_api_cv_threshold_t;

typedef struct bm_api_cv_pyramid {
  u64 input_addr;
  u64 kernel_addr;
  u64 output_addr;
  int width;
  int height;
  int ow;
  int oh;
  int stride_i;
  int stride_o;
} bm_api_cv_pyramid_t;

typedef struct bm_api_cv_axpy {
  u64  A_global_offset;
  u64 X_global_offset;
  u64 Y_global_offset;
  u64 F_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
} bm_api_cv_axpy_t;

typedef struct bm_api_cv_warp {
  u64 S_global_offset[4];
  u64 P_global_offset;
  int matrix_num[4];
  int src_stride[4];
  int src_mode;
  int image_n;
  int image_c;
  int image_sh;
  int image_sw;
  int image_dh;
  int image_dw;
  int image_dh_real;
  int image_dw_real;
  int blockIdx_x;
  int blockIdx_y;
  int type;
} bm_api_cv_warp_t;

typedef struct bm_api_width_align {
  u64 S_global_offset;
  u64 D_global_offset;
  int N;
  int C;
  int H;
  int W;
  int src_n_stride;
  int src_c_stride;
  int src_h_stride;
  int dst_n_stride;
  int dst_c_stride;
  int dst_h_stride;
  int data_size;
} bm_api_cv_width_align_t;

typedef struct bm_api_cv_warp_bilinear {
  u64 S_global_offset;
  u64 P_global_offset;
  float matrix[6];
  int image_c;
  int image_sh;
  int image_sw;
  int image_dh;
  int image_dw;
  int image_dh_real;
  int image_dw_real;
  int blockIdx_x;
  int blockIdx_y;
  int src_stride;
} bm_api_cv_warp_bilinear_t;

typedef struct {
  u64 scores_addr_0;
  u64 bbox_deltas_addr_0;
  u64 anchor_scales_addr_0;
  float scale_factor_0;
  int feat_factor_0;
  int channel_0;
  int feat_w_0;
  int feat_h_0;
  int width_0;
  int height_0;
  int feat_stride_0;
  int anchor_scale_size_0;
  int base_size_0;
  float im_scale_0;
  int min_size_0;
  float base_threshold_0;
  int per_nms_topn_0;
  int bbox_n_offset_0;
  u64 scores_addr_1;
  u64 bbox_deltas_addr_1;
  u64 anchor_scales_addr_1;
  float scale_factor_1;
  int feat_factor_1;
  int channel_1;
  int feat_w_1;
  int feat_h_1;
  int width_1;
  int height_1;
  int feat_stride_1;
  int anchor_scale_size_1;
  int base_size_1;
  float im_scale_1;
  int min_size_1;
  float base_threshold_1;
  int per_nms_topn_1;
  int bbox_n_offset_1;
  u64 scores_addr_2;
  u64 bbox_deltas_addr_2;
  u64 anchor_scales_addr_2;
  float scale_factor_2;
  int feat_factor_2;
  int channel_2;
  int feat_w_2;
  int feat_h_2;
  int width_2;
  int height_2;
  int feat_stride_2;
  int anchor_scale_size_2;
  int base_size_2;
  float im_scale_2;
  int min_size_2;
  float base_threshold_2;
  int per_nms_topn_2;
  int bbox_n_offset_2;
  float nms_threshold;
  float filer_threshold;
  int batch_num;
  int anchor_num;
  u64 output_proposal_addr;
  u64 filter_output;
  u64 filter_output_shape_addr;
} bm_api_cv_gen_proposal_and_nms_t;

typedef struct bm_api_cv_morph {
  u64 src_addr;
  u64 dst_addr;
  int width;
  int height;
  int kh;
  int kw;
  int stride_i;
  int stride_o;
  int format;
  int op;
} bm_api_cv_morph_t;

typedef struct bm_api_cv_fusion {
  u64 input1_addr;
  u64 input2_addr;
  u64 output_addr;
  int width;
  int height;
  int input1_str;
  int input2_str;
  int output_str;
  // threshold para
  int type;
  u32 thresh;
  u32 max_value;
  // morph para
  int kh;
  int kw;
  int format;
  // int op;
} bm_api_cv_fusion_t;

typedef struct bm_api_cv_resize_st {
  u64 in_img_addr;
  u64 img_para_addr;
  int in_channel;
  u64 out_img_addr;
  u64 input_buf;
  u64 output_buf;
  u64 roi_buf;
  int in_data_type;
  int out_data_type;
  int width_stride;
  int height_stride;
  int image_num;
  int need_switch;
  int one_policy_in_diff_images;
  int if_4N_to_1N;
  int multi_roi;
  int stretch_fit;
  int padding_b;
  int padding_g;
  int padding_r;
} bm_api_cv_resize_t;

typedef struct bm_api_cv_distance {
  u64 Xaddr;
  u64 Yaddr;
  int dim;
  float pnt[8];
  int len;
  int dtype;
} bm_api_cv_distance_t;

typedef struct bm_api_cv_yuv2rgb {
  u64 input_offset_img0_plane0;
  u64 input_offset_img0_plane1;
  u64 input_offset_img0_plane2;
  u64 input_offset_img1_plane0;
  u64 input_offset_img1_plane1;
  u64 input_offset_img1_plane2;
  u64 input_offset_img2_plane0;
  u64 input_offset_img2_plane1;
  u64 input_offset_img2_plane2;
  u64 input_offset_img3_plane0;
  u64 input_offset_img3_plane1;
  u64 input_offset_img3_plane2;

  u64 output_offset_img0_plane0;
  u64 output_offset_img1_plane0;
  u64 output_offset_img2_plane0;
  u64 output_offset_img3_plane0;

  int src_store_mode;
  int src_image_format;
  int input_n;
  int input_h;
  int input_w;

  int dst_store_mode;
  int dst_image_format;
} bm_api_cv_yuv2rgb_t;

typedef struct bm_api_cv_storage_convert {
  u64 input0_addr;
  u64 input1_addr;
  u64 input2_addr;
  u64 input3_addr;
  u64 output0_addr;
  u64 output1_addr;
  u64 output2_addr;
  u64 output3_addr;

  u64 reserved0_addr0;
  u64 reserved0_addr1;
  u64 reserved0_addr2;
  u64 reserved0_addr3;

  u64 reserved1_addr0;
  u64 reserved1_addr1;
  u64 reserved1_addr2;
  u64 reserved1_addr3;

  u32 src_image_format;
  u32 dst_image_format;
  u32 src_data_type;
  u32 dst_data_type;

  int image_num;
  int height;
  int width;
  u32 op;
  u32 csc_type;
} bm_api_cv_storage_convert_t;

typedef struct bm_api_yuv_resize_st {
  u64 input_para_addr;
  u64 output_para_addr;
  int data_type;
  int image_num;
} bm_api_yuv_resize_t;

typedef struct {
  u64 src_data_addr;
  u64 src_index_addr;
  u64 dst_data_addr;
  u64 dst_index_addr;
  int order;
  int location;
  int data_cnt;
  int sort_cnt;
} bm_api_cv_sort_test_t;

typedef struct bm_api_fc_fix8b_forward_parallel {
  u64 bottom_global_addr;
  u64 weight_global_addr;
  u64 bias_global_addr;
  u64 top_global_addr;
  int batch_size;
  int input_neuron_num;
  int output_neuron_num;
  int transpose;  // if need to transpose weight matrix
  int have_bias;
  int bottom_sign;  // 1: signed, 0: unsigned
  int weight_sign;
  int bias_sign;
  int right_shift_bit;
  int res_16b;
  int if_relu;
  int if_global_in_4N;
  int if_global_out_4N;
  float alpha;
  float beta;
} bm_api_fc_fix8b_forward_parallel_t;

typedef struct bm_api_fc_fix8b_forward_parallel_opt {
  u64 A_global_addr;
  u64 B_global_addr;
  u64 C_global_addr;
  int batch_size;
  int input_neuron_num;
  int output_neuron_num;
  int A_sign;  // 1: signed, 0: unsigned
  int B_sign;
} bm_api_fc_fix8b_forward_parallel_opt_t;

typedef struct bm_api_memset {
  u64 global_offset;
  u32 height;
  u32 width;
  int mode;
  int val;
} bm_api_memset_t;

#pragma pack(pop)


INLINE static int ceiling_func(int numerator, int denominator) { return (numerator + denominator - 1) / denominator; }
INLINE static int ceiling_func_shift(int numerator, int shift) { return (numerator + (1 << shift) - 1) >> shift; }


#define  BM_API_ID_MEM_SET                   1
#define  BM_API_ID_FC_FWD_FIX8B_PARALLEL     85
#define  BM_API_ID_GEMM                      105
#define  BM_API_ID_CV_WARP                   112
#define  BM_API_ID_CV_RESIZE                 113
#define  BM_API_ID_CV_YUV2RGB                114
#define  BM_API_ID_TRANSPOSE                 117
#define  BM_API_ID_CV_NMS                    131
#define  BM_API_ID_MEMCPY_TENSOR             138
#define  BM_API_ID_MEMSET_BYTE               177
#define  BM_API_ID_YOLOV3_DETECT_OUT         178
#define  BM_API_ID_MEMCPY_TENSORS            183
#define  BM_API_ID_YUV_RESIZE                186
#define  BM_API_ID_FC_FWD_FIX8B_PARALLEL_OPT 187
#define  BM_API_ID_CV_CONVERT_TO             500
#define  BM_API_ID_CV_GEN_PROP_AND_NMS       501
#define  BM_API_ID_CV_CONVERT_TO_INTERGRATED 502
#define  BM_API_ID_CV_SROT_TEST              503
#define  BM_API_ID_CV_FEATURE_MATCH          504
#define  BM_API_ID_CV_STORAGE_CONVERT        505
#define  BM_API_ID_CV_CORRECT_LAYOUT         506
#define  BM_API_ID_CV_COPY_TO                507
#define  BM_API_ID_CV_SORT                   508
#define  BM_API_ID_CV_FEATURE_MATCH_FIX8B    509
#define  BM_API_ID_CV_WARP_BILINEAR          510
#define  BM_API_ID_CV_SOFT_NMS               511
#define  BM_API_ID_CV_SPLIT                  512
#define  BM_API_ID_CV_TRANSPOSE              513
#define  BM_API_ID_CV_FILTER                 514
#define  BM_API_ID_CV_ADD_WEIGHTED           515
#define  BM_API_ID_CV_BATCH_TOPK             516
#define  BM_API_ID_CV_YUV2HSV                517
#define  BM_API_ID_CV_DCT_COEFF              518
#define  BM_API_ID_CV_DCT                    519
#define  BM_API_ID_CV_CALC_HIST_INDEX        520
#define  BM_API_ID_CV_FFT_1D                 521
#define  BM_API_ID_CV_CMULP                  522
#define  BM_API_ID_CV_DISTANCE               523
#define  BM_API_ID_CV_MIN_MAX                524
#define  BM_API_ID_CV_ABSDIFF                525
#define  BM_API_ID_CV_THRESHOLD              526
#define  BM_API_ID_CV_CANNY                  527
#define  BM_API_ID_CV_MORPH                  528
#define  BM_API_ID_CV_PYRAMID                529
#define  BM_API_ID_CV_LKPYRAMID              530
#define  BM_API_ID_CV_LAPLACIAN              531
#define  BM_API_ID_CV_AXPY                   532
#define  BM_API_ID_CV_FUSION                 533
#define  BM_API_ID_CV_BITWISE                534

/******************bmlib*********************/

#ifdef WIN32
#define BMDEV_BASE64_CODEC                                                     \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x72, METHOD_BUFFERED, FILE_ANY_ACCESS)

#else
    #define BMDEV_BASE64_CODEC            _IOWR('p', 0x72, unsigned long)
#endif

DECL_EXPORT int platform_ioctl(bm_handle_t handle, u32 commandID, void* param);

DECL_EXPORT bm_status_t bm_send_api(
  bm_handle_t  handle,
  int api_id,
  const u8     *api,
  u32          size);

DECL_EXPORT bm_status_t bm_sync_api(bm_handle_t handle);
bm_status_t bm_get_device_time_us(bm_handle_t handle,
                                            unsigned long *time_us);
DECL_EXPORT int array_cmp(
    float *p_exp,
    float *p_got,
    int len,
    const char *info_label,
    float delta);

DECL_EXPORT int array_cmp_fix16b(
    void *p_exp,
    void *p_got,
    int sign,  // 0: unsigned, 1: signed
    int len,
    const char *info_label,
    int delta);

DECL_EXPORT int array_cmp_fix8b(
    void *p_exp,
    void *p_got,
    int sign,  // 0: unsigned, 1: signed
    int len,
    const char *info_label,
    int delta);

#ifdef _WIN32

DECL_EXPORT int clock_gettime(int dummy, struct timespec* ct);
DECL_EXPORT int gettimeofday(struct timeval* tp, void* tzp);

#endif

#if defined (__cplusplus)
}
#endif

/********************************************/
#endif  // BMCV_API_STRUCT_H
