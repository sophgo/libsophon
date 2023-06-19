#ifndef BM_API_STRUCT_H
#define BM_API_STRUCT_H

#define MAX_MULTIREGION_INPUT_NUM 5
#define MAX_CONCATLAYER_INPUT_NUM 10
#define MAX_ELTWISELAYER_INPUT_NUM 8
#define MAX_SPLIT_TF_OUTPUT_NUM 10
#ifndef FW_MAX_SHAPE_DIMS
#define FW_MAX_SHAPE_DIMS 8
#endif  // FW_MAX_SHAPE_DIMS
#ifndef FW_MAX_STRIDE_DIMS
#define FW_MAX_STRIDE_DIMS 4
#endif  // FW_MAX_STRIDE_DIMS

#pragma pack(push, 1)
typedef struct bm_api_absval_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
} bm_api_absval_forward_t;



typedef struct bm_api_absval_backward {
  u64 top_diff_global_offset;
  u64 bottom_data_global_offset;
  u64 bottom_diff_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
} bm_api_absval_backward_t;


typedef struct bm_api_accuracy_layer {
  u64 global_offset_bottom_data;
  u64 global_offset_top_data;
  u64 global_offset_label_idx;
  u64 global_offset_bottom_data_buffer;
  u64 arm_reserved_global_offset;
  int Tensor_Dim;
  int Tensor_N;
  int Top_K;
} bm_api_accuracy_layer_t;


typedef struct bm_api_batchnorm_forward_inference_parallel {
  u64 bottom_global_offset;
  u64 mean_ma_global_offset;
  u64 variance_ma_global_offset;
  float scale_ma;
  u64 variance_global_offset;
  u64 top_global_offset;
  float eps;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int need_var;
  int need_calc;
  int if_relu;

} bm_api_batchnorm_forward_inference_parallel_t;


typedef struct bm_api_batchnorm_forward_train_parallel {
  u64 bottom_global_offset;
  u64 mean_ma_global_offset;
  u64 variance_ma_global_offset;
  u64 mean_global_offset;
  u64 variance_global_offset;
  u64 top_global_offset;
  float eps;
  float ma_fraction;
  float m;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;

} bm_api_batchnorm_forward_train_parallel_t;


typedef struct bm_api_batchnorm_backward_parallel {
  u64 output_global_offset;
  u64 output_diff_global_offset;
  u64 variance_global_offset;
  u64 input_diff_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int using_global_stats;

} bm_api_batchnorm_backward_parallel_t;


#define BILINEAR_SEGMENT_NUM 8
typedef struct bm_api_bilinear_interpolation {
  u64 in_data_offset_global;
  u64 in_coeff_offset_global;
  u64 out_data_offset_global;
  u64 buffer_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int output_h;
  int output_w;
  int filter_size_h;
  int filter_size_w;

} bm_api_bilinear_interpolation_t;


typedef struct bm_api_bilinear_interpolation_parallel {
  u64 in_data_offset_global;
  u64 in_coeff_offset_global;
  u64 out_data_offset_global;
  u64 buffer_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int output_h;
  int output_w;
  int filter_size_h;
  int filter_size_w;

} bm_api_bilinear_interpolation_parallel_t;
#pragma pack(pop)

typedef struct bm_api_axpy {
  u64 x_global_offset;
  u64 y_global_offset;
  int len;
  int inc_x;
  int inc_y;
  float alpha;
} bm_api_axpy_t;

typedef struct bm_api_scale {
  u64 x_global_offset;
  int len;
  int inc;
  float scale;
} bm_api_scale_t;

typedef struct bm_api_dot {
  u64 X_global_offset;
  u64 Y_global_offset;
  u64 dot_global_offset;
  u64 arm_reserve_global_offset;
  int len;
  int inc_x;
  int inc_y;
} bm_api_dot_t;

typedef struct bm_api_asum {
  u64 X_global_offset;
  u64 arm_reserve_global_offset;
  u64 asum_global_offset;
  int len;
  int inc_x;
} bm_api_asum_t;

typedef struct bm_api_nrm2 {
  u64 x_global_offset;
  u64 nrm2_global_offset;
  u64 arm_reserve_global_offset;
  int len;
  int inc_x;
} bm_api_nrm2_t;

typedef struct bm_api_rot {
  u64 X_global_offset;
  u64 Y_global_offset;
  int len;
  int inc_x;
  int inc_y;
  float c;
  float s;
} bm_api_rot_t;

typedef struct bm_api_rotm {
  u64 X_global_offset;
  u64 Y_global_offset;
  int len;
  int inc_x;
  int inc_y;
  float sflag;
  float sh11;
  float sh21;
  float sh12;
  float sh22;
} bm_api_rotm_t;

typedef struct bm_api_tpmv {
  bool is_lower_tri;
  bool is_A_trans;
  bool is_unit_tri;
  int N;
  u64 A_global_offset;
  u64 A_full_global_offset;
  u64 X_global_offset;
  int incX;
} bm_api_tpmv_t;

typedef struct bm_api_tbmv {
  bool is_lower_tri;
  bool is_A_trans;
  bool is_unit_tri;
  int N;
  int K;
  u64 A_global_offset;
  u64 shift_band_global_offset;
  int lda;
  u64 X_global_offset;
  int incX;
} bm_api_tbmv_t;

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

#define USE_INDEX_TABLE
typedef struct bm_api_trmm {
  bool is_left_side;
  bool is_lower_tri;
  bool is_A_trans;
  bool is_unit_tri;
  int M;
  int N;
  float alpha;
  u64 A_global_offset;
#ifndef USE_INDEX_TABLE
  u64 A_full_global_offset;
#else
  u64 index_global_offset;
#endif
  int lda;
  u64 B_global_offset;
  int ldb;
} bm_api_trmm_t;

typedef struct bm_api_symm {
  bool is_left;
  bool is_up;
  int M;
  int N;
  float alpha;
  u64 A_global_offset;
  int lda;
  u64 B_global_offset;
  int ldb;
  float beta;
  u64 C_global_offset;
  int ldc;
  u64 index_global_offset;
} bm_api_symm_t;

#pragma pack(push, 1)
typedef struct bm_api_bnll_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;

} bm_api_bnll_forward_t;


typedef struct bm_api_bnll_backward {
  u64 top_diff_global_offset;
  u64 bottom_data_global_offset;
  u64 bottom_diff_global_offset;
  float threshold;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;

} bm_api_bnll_backward_t;


typedef struct bm_api_coeff_update_SGD_parallel {
  u64 weight_diff_mem;
  u64 weight_mem;
  u64 history_weight_mem;
  int weight_count;
  float base_lr;
  float momentum;
  float weight_decay;

} bm_api_coeff_update_SGD_parallel_t;


typedef struct bm_api_contrastive_loss_forward {
  u64 bottom_0_global_offset;
  u64 bottom_1_global_offset;
  u64 label_global_offset;
  u64 diff_global_offset;
  u64 dist_sq_global_offset;
  u64 loss_global_offset;
  u64 buffer_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;
  float margin;
  int legacy_version;

} bm_api_contrastive_loss_forward_t;


typedef struct bm_api_contrastive_loss_backward {
  u64 label_global_offset;
  u64 diff_global_offset;
  u64 dist_sq_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_diff_0_global_offset;
  u64 bottom_diff_1_global_offset;
  u64 buffer_global_offset;
  u64 arm_reserved_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;
  float margin;
  int legacy_version;
  int propagate_down_flag;

} bm_api_contrastive_loss_backward_t;


typedef struct bm_api_conv_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 weight_offset_global;
  u64 bias_offset_global;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h;
  int pad_w;
  int pad_h_after;
  int pad_w_after;
  int stride_h;
  int stride_w;
  int using_bias;
  int result_add;
  int use_winograd;
  int hsecs;
  int nsecs;
  int icsecs;
  int ocsecs;

} bm_api_conv_forward_parallel_t;

typedef struct bm_api_conv_forward_parallel_fix8b {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 weight_offset_global;
  u64 bias_offset_global;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h;
  int pad_h_after;
  int pad_w;
  int pad_w_after;
  int stride_h;
  int stride_w;
  int using_bias;
  int result_add;
  int use_winograd;
  int hsecs;
  int nsecs;
  int icsecs;
  int ocsecs;
  int if_relu;
  int upper_limit;
  int if_ic_inner;
  int ins_h;
  int ins_w;
  int rshiftbits;
  int opd0_sign;
  int opd1_sign;
  int opd2_sign;
  int opd0_short_str;
  int use_3ic_optimize;

} bm_api_conv_forward_parallel_fix8b_t;


typedef struct bm_api_conv_forward_parallel_fix16b {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 weight_offset_global;
  u64 bias_offset_global;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h;
  int pad_h_after;
  int pad_w;
  int pad_w_after;
  int stride_h;
  int stride_w;
  int using_bias;
  int result_add;
  int use_winograd;
  int hsecs;
  int nsecs;
  int icsecs;
  int ocsecs;
  int owsecs;
  int if_relu;
  int upper_limit;
  int if_ic_inner;
  int ins_h;
  int ins_w;
  int rshiftbits;
  int opd0_sign;
  int opd1_sign;
  int opd2_sign;
  int opd0_short_str;

} bm_api_conv_forward_parallel_fix16b_t;


typedef struct bm_api_bnscale_forward_parallel_fix8b {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 scale_offset_global;
  u64 bias_offset_global;
  u64 rshift_offset_global;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;

  int input_sign;
  int scale_sign;
  int bias_sign;
  //    int             hsecs;
  //    int             nsecs;
  int if_relu;

} bm_api_bnscale_forward_parallel_fix8b_t;


typedef struct bm_api_perchannel_shift_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 rshift_offset_global;
  int a_shape[4];
  int b_shape[4];
  int a_dim;
  int b_dim;
  int shift_type;
  int shift_num;
  int is_int16[4];
  int bottom_sign;

} bm_api_perchannel_shift_forward_parallel_t;
#pragma pack(pop)


typedef struct bm_api_deconv_forward_parallel {
  u64 ifmap_offset_global;
  u64 weight_offset_global;
  u64 bias_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int output_h;
  int output_w;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h_t;
  int pad_h_b;
  int pad_w_l;
  int pad_w_r;
  int stride_h;
  int stride_w;
  int with_bias;
  int result_add;
  bool use_winograd;
  int icsecs;
  int ocsecs;
  int nsecs;
  int hsecs;
  int coef_st_way;  // 0: ic oc h w 1: oc ic h w
} bm_api_deconv_forward_parallel_t;

typedef struct bm_api_deconv_fix8b_forward_parallel {
  u64 ifmap_global_addr;
  u64 ofmap_global_addr;
  u64 weight_global_addr;
  u64 bias_global_addr;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int output_c;
  int groups;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h;
  int pad_w;
  int stride_h;
  int stride_w;
  int output_padding_h;
  int output_padding_w;
  int using_bias;
  int if_relu;
  int rshift_num;
  int coef_st_way;
  int ifmap_sign;
  int weight_sign;
  int bias_sign;
} bm_api_deconv_fix8b_forward_parallel_t;

#pragma pack(push, 1)
typedef struct bm_api_conv_backward_bias_parallel {
  u64 ofmap_diff_offset_global;
  u64 bias_diff_offset_global;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kh;
  int kw;
  int pad_h;
  int pad_w;
  int stride_h;
  int stride_w;
  int result_add;

} bm_api_conv_backward_bias_parallel_t;


typedef struct nodechip_winograd_bottom_diff_parallel {
  u64 top_diff_offset_global_offset;
  u64 weight_global_offset;
  u64 convert_matrix_global_offset;
  u64 bottom_diff_offset_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int output_h;
  int output_w;
  int kh;
  int kw;
  int pad_h;
  int pad_w;

} nodechip_winograd_bottom_diff_parallel_t;


typedef struct bm_api_conv_parallel_bank_conflict {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 weight_offset_global;
  u64 bias_offset_global;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h;
  int pad_w;
  int stride_h;
  int stride_w;
  int using_bias;
  int result_add;

} bm_api_conv_parallel_bank_conflict_t;


typedef struct bm_api_conv_parallel_power_evaluation {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 weight_offset_global;
  u64 bias_offset_global;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h;
  int pad_w;
  int stride_h;
  int stride_w;
  int using_bias;
  int result_add;

} bm_api_conv_parallel_power_evaluation_t;


typedef struct bm_api_depthwise_forward {
  u64 input_global_offset;
  u64 output_global_offset;
  u64 weight_global_offset;
  u64 bias_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kernel_h;
  int kernel_w;
  int pad_t;
  int pad_b;
  int pad_l;
  int pad_r;
  int stride_h;
  int stride_w;
  int using_bias;

} bm_api_depthwise_forward_t;


typedef struct bm_api_depthwise_fix8b_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 weight_offset_global;
  u64 bias_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kh;
  int kw;
  int pad_h_top;
  int pad_h_bottom;
  int pad_w_left;
  int pad_w_right;
  int stride_h;
  int stride_w;
  int r_shift;
  int using_bias;
  int rshift_typ;
  int opd0_sign;
  int opd1_sign;
  int opd2_sign;
  int res0_sign;
  int if_relu;

} bm_api_depthwise_fix8b_forward_parallel_t;


typedef struct bm_api_memset {
  u64 global_offset;
  unsigned int height;
  unsigned int width;
  int mode;
  int val;
} bm_api_memset_t;

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
  unsigned int const_val;
}bm_api_memset_byte_t;

typedef struct bm_api_memcpy {
  u64 src_global_offset;
  u64 dst_global_offset;
  int input_n;
  int src_nstride;
  int dst_nstride;
  int count;
}bm_api_memcpy_t;

typedef struct bm_api_memcpy_wstride {
  u64 src_global_offset;
  u64 dst_global_offset;
  int src_wstride;
  int dst_wstride;
  int count;
  int format_bytes;
}bm_api_memcpy_wstride_t;

typedef struct bm_api_memcpy_byte {
  u64 src_global_offset;
  u64 dst_global_offset;
  u64 size;
}bm_api_memcpy_byte_t;

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
}bm_api_memcpy_tensor_t;

typedef struct bm_api_memcpy_tensors {
  int copy_num;
  u64 info_global_offset;
}bm_api_memcpy_tensors_t;

typedef struct bm_api_cv_split {
  u64 src_global_offset;
  u64 dst_global_offset;
  int src_w;
  int src_h;
  int dst_w;
  int dst_h;
  int type_size;
}bm_api_cv_split_t;

typedef struct bm_api_dropout_forward {
  u64 global_offset_bottom_data;
  u64 global_offset_top_data;
  u64 global_offset_mask_data;
  u64 global_offset_rand_table;
  int offset_relative_rand_table;
  float dropout_ratio;
  int input_n;
  int input_dim;
} bm_api_dropout_forward_t;


typedef struct bm_api_eltwise_forward {
  u64 bottom_global_offset[MAX_ELTWISELAYER_INPUT_NUM];
  u64 top_global_offset;
  u64 mask_global_offset;
  int input_num;
  int tensor_n;
  int tensor_c;
  int tensor_h;
  int tensor_w;
  int op_code;
  float coeff[MAX_ELTWISELAYER_INPUT_NUM];
  int need_mask;
  float mask_index[MAX_ELTWISELAYER_INPUT_NUM];
  int if_relu;
} bm_api_eltwise_forward_t;


typedef struct bm_api_eltwise_fix8b_forward {
  u64 bottom_A_global_addr;
  u64 bottom_B_global_addr;
  u64 top_global_addr;
  int tensor_n;
  int tensor_c;
  int tensor_h;
  int tensor_w;
  int op_code;
  int scale_A;
  int scale_B;
  int sign_A;
  int sign_B;
  int rshift_A;
  int rshift_B;
  int if_relu;

} bm_api_eltwise_fix8b_forward_t;


typedef struct bm_api_eltwise_backward {
  u64 top_data_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_data_global_offset;
  u64 mask_data_global_offset;
  u64 bottom_diff_global_offset;
  int op_;
  int flag_first;
  float coeffs_;
  int index;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_eltwise_backward_t;


typedef struct bm_api_elu_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  float alpha;
  float scale;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;

} bm_api_elu_forward_t;


typedef struct bm_api_elu_backward {
  u64 top_data_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_data_global_offset;
  u64 bottom_diff_global_offset;
  float alpha;
  float scale;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;

} bm_api_elu_backward_t;


typedef struct bm_api_euclidean_loss_forward {
  u64 bottom_data_global_offset;
  u64 label_global_offset;
  u64 temp_data_global_offset;
  u64 diff_global_offset;
  u64 loss_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_euclidean_loss_forward_t;


typedef struct bm_api_euclidean_loss_backward {
  u64 output_global_offset;
  u64 bottom_diff_global_offset;
  float alpha;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_euclidean_loss_backward_t;


typedef struct bm_api_exp_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  float base;
  float input_scale;
  float input_shift;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_exp_forward_t;


typedef struct bm_api_exp_backward {
  u64 top_diff_global_offset;
  u64 top_data_global_offset;
  u64 bottom_diff_global_offset;
  float base;
  float input_scale;
  float input_shift;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_exp_backward_t;


typedef struct bm_api_fc_forward_parallel {
  u64 global_offset_bottom_data;
  u64 global_offset_weight_data;
  u64 global_offset_bias_data;
  u64 global_offset_top_data;
  u64 global_offset_slope_data;
  int input_row_num;
  int input_col_num;
  int weight_col_num;
  int transpose;
  int have_bias;
  int using_relu;
  int if_activated;
  int active_type;
  int channel_shared;
  float shared_slope;
  int W_param;

} bm_api_fc_forward_parallel_t;


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

typedef struct bm_api_fc_weight_decompress {
  u64 bottom_global_addr;
  u64 top_global_addr;
  int start_local_addr;
  int index_size;
  int row;
  int col;

} bm_api_fc_weight_decompress_t;


typedef struct bm_api_fc_backward_parallel {
  u64 global_offset_bottom_data;
  u64 global_offset_top_diff;
  u64 global_offset_weight_data;
  u64 global_offset_weight_diff;
  u64 global_offset_bias_diff;
  u64 global_offset_bottom_diff;
  int input_row_num;
  int input_col_num;
  int weight_col_num;
  int propagate_down_bias_diff;
  int propagate_down_weight_diff;
  int propagate_down_bottom;
  int using_bias;
  int W_param;

} bm_api_fc_backward_parallel_t;


typedef struct bm_api_fc_forward_parallel_bank_conflict {
  u64 global_offset_bottom_data;
  u64 global_offset_weight_data;
  u64 global_offset_bias_data;
  u64 global_offset_top_data;
  int input_row_num;
  int input_col_num;
  int weight_col_num;
  int transpose;
  int have_bias;
  int using_relu;
  int W_param;

} bm_api_fc_forward_parallel_bank_conflict_t;


typedef struct bm_api_fc_backward_sgd_parallel {
  u64 global_offset_bottom_data;
  u64 global_offset_top_diff;
  u64 global_offset_weight_data;
  u64 global_offset_history_weight_data;
  u64 global_offset_bias_diff;
  u64 global_offset_bottom_diff;
  int input_row_num;
  int input_col_num;
  int weight_col_num;
  int propagate_down_bias_diff;
  int propagate_down_weight_diff;
  int propagate_down_bottom;
  int using_bias;
  int W_param;
  float base_lr;
  float momentum;
  float weight_decay;

} bm_api_fc_backward_sgd_parallel_t;


typedef struct bm_api_filter_forward {
  u64 bottom_global_offset;
  u64 filter_global_offset;
  u64 top_global_offset;
  u64 arm_reserved_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_filter_forward_t;


typedef struct bm_api_filter_backward {
  u64 top_diff_global_offset;
  u64 filter_global_offset;
  u64 bottom_diff_global_offset;
  u64 arm_reserved_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_filter_backward_t;


typedef struct bm_api_fullnet {
  u64 bdc_cmd_offset;
  u64 gdma_cmd_offset;
  u64 cmd_num_offset;

} bm_api_fullnet_t;


typedef struct bm_api_img_sum {
  u64 input_tensor_global_offset;
  u64 output_tensor_global_offset;
  int add_result;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_img_sum_t;


typedef struct bm_api_active_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;
  int input_dim;
  int active_type;
  void *param;

} bm_api_active_forward_t;


typedef struct bm_api_active_forward_fix8b {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;
  int input_dim;
  int active_type;
  float input_scale;
  float output_scale;
  int input_signed;
  int output_signed;
  void *param;

} bm_api_active_forward_fix8b_t;


typedef struct bm_api_log_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  float scale;
  float shift;
  float base;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_log_forward_t;


typedef struct bm_api_log_backward {
  u64 bottom_data_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  float scale;
  float shift;
  float base;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_log_backward_t;


typedef struct bm_api_lrn_forward_parallel {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  float alpha;
  int size;
  float beta;
  float k;

} bm_api_lrn_forward_parallel_t;


typedef struct bm_api_lrn_backward_parallel {
  u64 top_global_offset;
  u64 bottom_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int size;
  float alpha;
  float beta;
  float k;

} bm_api_lrn_backward_parallel_t;


typedef struct bm_api_lrn_fix8b_forward_parallel {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;
  int sign_unsign;
  float alpha;
  int size;
  float beta;
  float k;
  float scale_in;
  float scale_out;

} bm_api_lrn_fix8b_forward_parallel_t;


typedef struct bm_api_lstm_forward {
  u64 x_global_offset;
  u64 cont_global_offset;
  u64 x_static_global_offset;
  u64 w_hc_global_offset;
  u64 w_xc_global_offset;
  u64 w_xstatic_global_offset;
  u64 b_c_global_offset;
  u64 h_0_global_offset;
  u64 c_0_global_offset;
  u64 h_T_global_offset;
  u64 c_T_global_offset;
  u64 h_global_offset;
  u64 wxc_buf_global_offset;
  u64 wxc_sta_buf_global_offset;
  int input_n;
  int seq_len;
  int input_dim;
  int output_dim;
  int with_x_static;
  int expose_hidden;
  int method;
  int n_slice;

} bm_api_lstm_forward_t;


typedef struct bm_api_lstm_unit_forward {
  u64 X_i_global_offset;
  u64 X_f_global_offset;
  u64 X_o_global_offset;
  u64 X_g_global_offset;
  u64 C_prev_global_offset;
  u64 cont_expand_global_offset;
  u64 C_global_offset;
  u64 H_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_lstm_unit_forward_t;


typedef struct bm_api_lstm_unit_backward {
  u64 C_diff_global_offset;
  u64 H_diff_global_offset;
  u64 X_i_global_offset;
  u64 X_f_global_offset;
  u64 X_o_global_offset;
  u64 X_g_global_offset;
  u64 C_prev_global_offset;
  u64 C_global_offset;
  u64 cont_expand_global_offset;
  u64 C_prev_diff_global_offset;
  u64 X_i_diff_global_offset;
  u64 X_f_diff_global_offset;
  u64 X_o_diff_global_offset;
  u64 X_g_diff_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_lstm_unit_backward_t;


typedef struct bm_api_nodechip_md_cmp {
  u64 A_offset_global;
  u64 B_offset_global;
  u64 C_offset_global;
  u64 D_offset_global;
  u64 Y_offset_global;
  u64 R_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int A_is_constant;
  int B_is_constant;
  int C_is_constant;
  int D_is_constant;
  float A_constant;
  float B_constant;
  unsigned int C_constant;
  unsigned int D_constant;
  int result_skip;
  int is_top_1;
  int is_min;
  int is_md_cmp;

} bm_api_nodechip_md_cmp_t;


// ---------------------------
typedef struct bm_api_nodechip_float2int8 {
  u64 A_global_offset;
  u64 R_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int sign_unsign;

} bm_api_nodechip_float2int8_t;


typedef struct bm_api_md_linear {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 S_global_offset;
  u64 Y_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int result_add;
  int B_is_const;
  int S_is_const;
  float B_const_val;
  float S_const_val;
  LINEAR_OP linear_op;

} bm_api_md_linear_t;


// ---------------------------
typedef struct {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 R_global_offset;
  u64 arm_reserved_addr;
  u64 length;
  ALIGN_TENSOR_OP op;
  int add_result;
  int A_constant_flag;
  int B_constant_flag;
  float A_constant;
  float B_constant;

} bm_api_nodechip_1d_scalar_t;

typedef struct bm_api_nodechip_md_scalar {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 R_global_offset;
  // u64 arm_reserved_addr;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  ALIGN_TENSOR_OP op;
  int add_result;
  int A_constant_flag;
  int B_constant_flag;
  float A_constant;
  float B_constant;
  int B_N_is_1;
  int B_index_is_1;

} bm_api_nodechip_md_scalar_t;


typedef struct {
  u64 tensor_A_offset_global;
  u64 tensor_Y_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  float a;
  int n;
  SFU_OP sfu_op;
  u64 table_start_addr;

} bm_api_nodechip_md_sfu_t;


typedef struct {
  u64 tensor_A_offset_global;
  u64 tensor_Y_offset_global;
  u64 length;
  float a;
  int n;
  SFU_OP sfu_op;
  u64 table_start_addr;

} bm_api_nodechip_1d_sfu_t;


typedef struct bm_api_md_sum {
  u64 A_global_offset;
  u64 Y_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int result_add;

} bm_api_md_sum_t;


typedef struct bm_api_md_ops {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 C_global_offset;
  int in_a;  // tensor A: N, C, H, W
  int ic_a;
  int ih_a;
  int iw_a;
  int in_b;  // tensor B: N, C, H, W
  int ic_b;
  int ih_b;
  int iw_b;
  int op_code;
  int A_is_constant;
  int B_is_constant;
  int A_const_val;
  int B_const_val;

} bm_api_md_ops_t;


typedef struct bm_api_normalize_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  u64 scale_global_offset;
  int across_spatial;
  int channel_share;
  int n;
  int c;
  int h;
  int w;
  float eps;
  float scale;
  int if_relu;

} bm_api_normalize_forward_t;


typedef struct bm_api_normalize_fix8b_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  u64 scale_global_offset;
  int across_spatial;
  int channel_share;
  int n;
  int c;
  int h;
  int w;
  float eps;
  float scale;
  int if_relu;
  int in_sign;
  int out_sign;

} bm_api_normalize_fix8b_forward_t;


typedef struct bm_api_permute_param {
  u64 input_global_offset;
  u64 output_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;

} bm_api_permute_param_t;


typedef struct bm_api_permute_fix8b_param {
  u64 input_global_offset;
  u64 output_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;

} bm_api_permute_fix8b_param_t;


typedef struct bm_api_pooling_backward_parallel {
  u64 ofmap_offset_global;
  u64 oifmap_offset_global;
  u64 ifmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kh;
  int kw;
  int pad_h;
  int pad_w;
  int pad_h_after;
  int pad_w_after;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int c_step;
  int h_step;
  float Ratio;
  int result;  /* For split result*/

} bm_api_pooling_backward_parallel_t;


typedef struct bm_api_pooling_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int output_h;
  int output_w;
  int kh;
  int kw;
  int pad_h;
  int pad_w;
  int pad_h_after;
  int pad_w_after;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int avg_pooling_mode;
  int c_step;
  int h_step;
  float Ratio;

} bm_api_pooling_forward_parallel_t;


typedef struct bm_api_pooling_fix8b_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kh;
  int kw;
  int pad_h_top;
  int pad_h_bottom;
  int pad_w_left;
  int pad_w_right;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int avg_pooling_mode;
  int r_shift;
  int using_bias;
  int rshift_typ;
  int opd0_sign;
  int opd1_sign;
  int opd2_sign;
  int res0_sign;
  int if_relu;
  int c_step;
  int h_step;
  float Ratio;

} bm_api_pooling_fix8b_forward_parallel_t;


typedef struct bm_api_pooling3d_fix8b_forward_parallel {
  u64 ifmap_offset_global;
  u64 buffer_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_t;
  int input_h;
  int input_w;
  int kt;
  int kh;
  int kw;
  int pad_t_top;
  int pad_t_bottom;
  int pad_h_top;
  int pad_h_bottom;
  int pad_w_left;
  int pad_w_right;
  int stride_t;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int avg_pooling_mode;
  int r_shift;
  int using_bias;
  int rshift_typ;
  int opd0_sign;
  int opd1_sign;
  int opd2_sign;
  int res0_sign;
  int if_relu;
  int relu_upper_limit;

} bm_api_pooling3d_fix8b_forward_parallel_t;


typedef struct bm_api_depthwise_fix16b_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kh;
  int kw;
  int pad_h_top;
  int pad_h_bottom;
  int pad_w_left;
  int pad_w_right;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int avg_pooling_mode;
  int r_shift;
  int using_bias;
  int rshift_typ;
  int opd0_sign;
  int opd1_sign;
  int opd2_sign;
  int res0_sign;
  int if_relu;
  int c_step;
  int h_step;
  float Ratio;

} bm_api_depthwise_fix16b_forward_parallel_t;


typedef struct bm_api_pooling_train_index_forward_parallel {
  u64 ifmap_offset_global;
  u64 oifmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kh;
  int kw;
  int pad_h;
  int pad_w;
  int pad_h_after;
  int pad_w_after;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int c_step;
  int h_step;
  float Ratio;
  int result;

} bm_api_pooling_train_index_forward_parallel_t;


typedef struct bm_api_pooling_train_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 oifmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kh;
  int kw;
  int pad_h;
  int pad_w;
  int pad_h_after;
  int pad_w_after;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int c_step;
  int h_step;
  float Ratio;
  int result;

} bm_api_pooling_train_forward_parallel_t;


typedef struct bm_api_adaptive_pooling_forward {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int pooled_h;
  int pooled_w;
  int is_avg_pooling;

} bm_api_adaptive_pooling_forward_t;


typedef struct bm_api_adaptive_pooling_fix8b_forward {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int pooled_h;
  int pooled_w;
  int is_avg_pooling;
  int r_shift;
  int rshift_typ;
  int opd0_sign;
  int opd1_sign;
  int opd2_sign;
  int res0_sign;
  int if_relu;

} bm_api_adaptive_pooling_fix8b_forward_t;


typedef struct bm_api_upsample_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int size;
  int c_step;
  int h_step;
  float Ratio;

} bm_api_upsample_forward_parallel_t;


typedef struct bm_api_upsample_mask_forward_parallel {
  u64 bottom_global_offset;
  u64 bottom_mask_global_offset;
  u64 top_global_offset;
  int bottom_global_N;
  int bottom_c;
  int bottom_h;
  int bottom_w;
  int size;

} bm_api_upsample_mask_forward_parallel_t;


typedef struct bm_api_pooling_forward_parallel_bank_conflict {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int kh;
  int kw;
  int pad_h;
  int pad_w;
  int stride_h;
  int stride_w;
  int is_avg_pooling;
  int c_step;
  int h_step;
  float Ratio;

} bm_api_pooling_forward_parallel_bank_conflict_t;


typedef struct bm_api_power_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  float power_;
  float scale_;
  float shift_;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_power_forward_t;


typedef struct bm_api_power_backward {
  u64 top_diff_global_offset;
  u64 top_data_global_offset;
  u64 bottom_data_global_offset;
  u64 bottom_diff_global_offset;
  float power_;
  float scale_;
  float shift_;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_power_backward_t;


typedef struct bm_api_prelu_forward {
  u64 bottom_global_offset;
  u64 slope_global_offset;
  u64 top_global_offset;
  // u64             arm_reserved_global_offset;
  float slope0;
  int channel_shared;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_prelu_forward_t;


typedef struct bm_api_prelu_backward {
  u64 top_diff_global_offset;
  u64 bottom_data_global_offset;
  u64 slope_data_global_offset;
  u64 slope_diff_global_offset;
  u64 bottom_diff_global_offset;
  u64 arm_reserved_global_offset;
  int propagate_down_flag;
  int channel_shared;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_prelu_backward_t;


typedef struct bm_api_relu_forward_parallel {
  u64 global_offset_bottom_data;
  u64 global_offset_top_data;
  float negative_slope;
  float limit;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_relu_forward_parallel_t;


typedef struct bm_api_relu_backward_parallel {
  u64 global_offset_bottom_data;
  u64 global_offset_top_diff;
  u64 global_offset_bottom_diff;
  float negative_slope;
  float limit;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_relu_backward_parallel_t;


typedef struct bm_api_roi_pooling_forward {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 rois_offset_global;
  u64 arm_reserved_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int pooled_h;
  int pooled_w;
  int roi_num;
  float spatial_scale;

} bm_api_roi_pooling_forward_t;


typedef struct bm_api_psroipooling_forward {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  u64 rois_offset_global;
  u64 arm_reserved_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int output_dim;
  int group_size;
  int roi_num;
  float spatial_scale;
  int input_sign;
  int output_sign;

} bm_api_psroipooling_forward_t;


typedef struct bm_api_rpnproposal_forward_t {
  u64 bottom_global_offset0;
  u64 bottom_global_offset1;
  u64 bottom_global_offset2;
  u64 top_global_offset;
  int Tensor_N[2];
  int Tensor_C[2];
  int Tensor_H[2];
  int Tensor_W[2];
  int feat_stride_;
  int base_size_;
  int min_size_;
  int pre_nms_topN_;
  int post_nms_topN_;
  float nms_thresh_;
  float score_thresh_;
  int in_tensor_global_store_mode;
  u64 global_offset_1N_buf;
  u64 arm_reserved_global_offset;
  int bottom_prec;  // 0: FP32, 1: INT8, 2: UINT8
  float scale_val;  // only vaid if FIX8B

} bm_api_rpnproposal_forward_t;


typedef struct bm_api_scale_forward {
  u64 bottom_global_offset;
  u64 scale_global_offset;
  u64 bias_global_offset;
  u64 top_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;
  int shape_axis;
  int shape_axis_num;
  int add_bias;
  int if_relu;

} bm_api_scale_forward_t;


typedef struct bm_api_sigmoid_forward_parallel {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;  // note this is total input_n
  int input_c;

} bm_api_sigmoid_forward_parallel_t;


typedef struct bm_api_sigmoid_forward_parallel_fix8b {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;  // note this is total input_n
  int input_c;
  float input_scale;
  float output_scale;
  int input_signed;
  int output_signed;

} bm_api_sigmoid_forward_parallel_fix8b_t;


typedef struct bm_api_sigmoid_backward_parallel {
  u64 top_data_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  int input_n;  // note this is total input_n
  int input_c;

} bm_api_sigmoid_backward_parallel_t;


typedef struct bm_api_sigmoid_cross_entropy_loss_forward {
  u64 bottom_global_offset;
  u64 target_global_offset;
  u64 top_global_offset;
  u64 loss_global_offset;
  u64 buffer_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_sigmoid_cross_entropy_loss_forward_t;


typedef struct bm_api_sigmoid_cross_entropy_loss_backward {
  u64 sigmoid_output_global_offset;
  u64 target_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  u64 arm_reserved_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_sigmoid_cross_entropy_loss_backward_t;


typedef struct bm_api_silence_backward {
  u64 bottom_diff_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_silence_backward_t;


typedef struct bm_api_softmax_loss_forward_parallel {
  u64 bottom_data_global_offset;
  u64 label_global_offset;
  u64 top_data_global_offset;
  u64 loss_global_offset;
  u64 arm_reserved_global_offset;
  float normalizer;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_softmax_loss_forward_parallel_t;


typedef struct bm_api_softmax_loss_backward_parallel {
  u64 top_data_global_offset;
  u64 label_global_offset;
  u64 bottom_diff_global_offset;
  u64 loss_global_offset;
  u64 arm_reserved_global_offset;
  float normalizer;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_softmax_loss_backward_parallel_t;


typedef struct bm_api_softmax_loss_bidirection_parallel {
  u64 bottom_data_global_offset;
  u64 label_global_offset;
  u64 bottom_diff_global_offset;
  u64 loss_global_offset;
  u64 arm_reserved_global_offset;
  float normalizer;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_softmax_loss_bidirection_parallel_t;


typedef struct bm_api_softmax_forward_parallel {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;
  int bottom_prec;
  float scale_val;
  int in_tensor_global_store_mode;
  u64 global_offset_1N_buf;

} bm_api_softmax_forward_parallel_t;


typedef struct bm_api_softmax_backward_parallel {
  u64 bottom_data_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_softmax_backward_parallel_t;


typedef struct bm_api_global_memcpy {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int input_n;
  int src_nstride;
  int dst_nstride;
  int count;

} bm_api_global_memcpy_t;


typedef struct bm_api_crop {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int offset[4];
  int topshape[4];
  int bottomshape[4];

} bm_api_crop_t;


typedef struct bm_api_concat {
  u64 bottom_global_offset[MAX_CONCATLAYER_INPUT_NUM];
  u64 top_global_offset;
  int bottom_shape[MAX_CONCATLAYER_INPUT_NUM][4];
  int top_shape[4];
  int st_by_concatway[MAX_CONCATLAYER_INPUT_NUM];
  int bottom_size;
  int concat_axis;

} bm_api_concat_t;


typedef struct bm_api_unfold {
  u64 input_global_offset;
  int input_rank;
  int input_shape[FW_MAX_SHAPE_DIMS - 1];
  int unfold_axis;
  int unfold_size;
  int unfold_step;
  u64 buffer_global_offset;
  u64 output_global_offset;

} bm_api_unfold_t;


typedef struct bm_api_multiregion_forward_parallel {
  u64 bottom_global_offset[MAX_MULTIREGION_INPUT_NUM];
  u64 top_global_offset[MAX_MULTIREGION_INPUT_NUM];
  int Tensor_N[MAX_MULTIREGION_INPUT_NUM];
  int Tensor_C[MAX_MULTIREGION_INPUT_NUM];
  int Tensor_H[MAX_MULTIREGION_INPUT_NUM];
  int Tensor_W[MAX_MULTIREGION_INPUT_NUM];
  int input_num;
  int classes;
  int coords;
  int nums;
  int Activate_parm[4];

} bm_api_multiregion_forward_parallel_t;

typedef struct bm_api_split_backward {
  int is_first;
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_split_backward_t;


typedef struct bm_api_tanh_forward_parallel {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int Tensor_N;
  int Tensor_C;

} bm_api_tanh_forward_parallel_t;


typedef struct bm_api_tanh_forward_parallel_fix8b {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int Tensor_N;
  int Tensor_C;
  float input_scale;
  float output_scale;
  int input_signed;
  int output_signed;

} bm_api_tanh_forward_parallel_fix8b_t;


typedef struct {
  u64 bottom_global_offset;
  u64 top_global_offset;
  u64 length;

} bm_api_sign_t;


typedef struct bm_api_tanh_backward_parallel {
  u64 bottom_data_global_offset;
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  int Tensor_N;
  int Tensor_C;

} bm_api_tanh_backward_parallel_t;


typedef struct bm_api_threshold_forward {
  u64 bottom_global_offset;
  u64 top_global_offset;
  float threshold;
  int input_n;  // note this is total input_n
  int input_c;
  int input_h;
  int input_w;

} bm_api_threshold_forward_t;


typedef struct bm_api_bias_forward {
  u64 bottom_data_global_offset;
  u64 bias_data_global_offset;
  u64 top_data_global_offset;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_bias_forward_t;


typedef struct bm_api_bias_fix8b_forward {
  u64 bottom_data_global_offset;
  u32 bottom_shape[4];
  u64 bias_data_global_offset;
  u64 top_data_global_offset;
  int bottom_sign;
  int bias_sign;
  int scale;
  int rshift;
  int if_relu;
  int relu_limit;

} bm_api_bias_fix8b_forward_t;


typedef struct bm_api_bias_backward {
  u64 top_diff_global_offset;
  u64 bottom_diff_global_offset;
  u64 bias_diff_global_offset;
  int flag;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;

} bm_api_bias_backward_t;


typedef struct bm_api_scale_backward {
  u64 top_diff_global_offset;
  u64 bottom_data_global_offset;
  u64 scale_data_global_offset;
  u64 scale_diff_global_offset;
  u64 bottom_diff_global_offset;
  int propagate_down_flag;
  int Tensor_N;
  int Tensor_C;
  int Tensor_H;
  int Tensor_W;
  int scale_dim;
  int inner_dim;
  int scale_is_neuron;

} bm_api_scale_backward_t;


typedef struct bm_api_conv_correlation {
  u64 input_data_global_offset;
  u64 output_diff_global_offset;
  u64 weight_diff_global_offset; /* output. */
  int input_c;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kh;
  int kw;
  int dh;
  int dw;
  int pad_h_t;
  int pad_h_b;
  int pad_w_l;
  int pad_w_r;
  int ins_h;
  int ins_w;
  int stride_h;
  int stride_w;
  int add_result;

} bm_api_conv_correlation_t;


typedef struct bm_api_conv3d_parallel {
  u64 input_global_offset;
  u64 output_global_offset;
  u64 filter_global_offset;
  u64 bias_global_offset;
  int input_n;
  int input_c;
  int input_t;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kt;
  int kh;
  int kw;
  int dt;
  int dh;
  int dw;
  int pad_t;
  int pad_t_after;
  int pad_h;
  int pad_h_after;
  int pad_w;
  int pad_w_after;
  int stride_t;
  int stride_h;
  int stride_w;
  int using_bias;
  int method;

} bm_api_conv3d_parallel_t;


typedef struct bm_api_regularization_l1 {
  u64 data_global_offset;
  u64 diff_global_offset;
  int n;
  int c;
  int h;
  int w;
  float local_decay;

} bm_api_regularization_l1_t;


typedef struct bm_api_global_int2float {
  u64 input_global_offset;
  u64 output_global_offset;
  int n;
  int c;
  int h;
  int w;
  int sign_unsign;

} bm_api_global_int2float_t;


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


typedef struct bm_api_cv_calc_hist_index {
  u64 Xaddr;
  u64 Yaddr;
  float a;
  float b;
  int len;
  int xdtype;
  float upper;

} bm_api_cv_calc_hist_index_t;


typedef struct bm_api_cv_distance {
  u64 Xaddr;
  u64 Yaddr;
  int dim;
  float pnt[8];
  int len;

} bm_api_cv_distance_t;


typedef struct bm_api_cv_min_max {
  u64 inputAddr;
  u64 minAddr;
  u64 maxAddr;
  int len;
  int mode;

} bm_api_cv_min_max_t;


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

typedef struct bm_api_laplacian {
  int channel;
  unsigned long long input_addr[3];
  unsigned long long kernel_addr;
  unsigned long long output_addr[3];
  unsigned int width[3];
  unsigned int height[3];
  int kw;
  int kh;
  unsigned int stride_i[3];
  unsigned int stride_o[3];
  float delta;
  int is_packed;
}  bm_api_laplacian_t;

typedef struct bm_api_cv_axpy {
  unsigned long long  A_global_offset;
  unsigned long long X_global_offset;
  unsigned long long Y_global_offset;
  unsigned long long F_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
}  bm_api_cv_axpy_t;


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

typedef struct {
  u64 input_global_mem_addr;
  u64 buffer_global_mem_addr;
  u64 output_global_mem_addr;
  u32 input_shape[FW_MAX_SHAPE_DIMS];
  int tile_coeff[FW_MAX_SHAPE_DIMS];
  int input_dim;
  int input_format;
  int output_format;
  int type;

} bm_api_tile_forward_t;


typedef struct {
  u64 input_global_mem_addr;
  u64 buffer_global_mem_addr;
  u64 output_global_mem_addr;
  u32 input_shape[FW_MAX_SHAPE_DIMS];
  int tile_coeff[FW_MAX_SHAPE_DIMS];
  int input_dim;
  int in_store_mode;
  int out_store_mode;
  int type;

} bm_api_tile_fix8b_forward_t;


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
  u64 input_global_mem_addr;
  u64 output_global_mem_addr;
  int input_shape[FW_MAX_SHAPE_DIMS];
  int order[FW_MAX_SHAPE_DIMS];
  int dims;
  int in_store_mode;
  int out_store_mode;
  u64 buffer_global_mem_addr;

} bm_api_transpose_fix8b_t;


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
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int sum_dim;

} bm_api_sum_forward_t;


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

typedef struct bm_api_yuv_resize_st {
  u64 input_para_addr;
  u64 output_para_addr;
  int data_type;
  int image_num;

} bm_api_yuv_resize_t;

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


typedef struct bm_api_cv_yuv2hsv {
  u64 src_addr[3];
  u64 dst_addr[3];
  unsigned int width;
  unsigned int height;
  unsigned int stride_i;
  unsigned int ow;
  unsigned int oh;
  unsigned int src_format;
  unsigned int dst_format;

} bm_api_cv_yuv2hsv_t;



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


typedef struct bm_api_compare {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 C_global_offset; /* output. */
  u64 arm_reserved_addr;
  u64 length;
  int type;
  int A_is_constant;
  int B_is_constant;
  float A_constant;
  float B_constant;

} bm_api_compare_t;


typedef struct bm_api_eltwise_binary_fix8b_forward {
  u64 bottom_A_global_addr;
  u64 bottom_B_global_addr;
  u64 top_global_addr;
  int bottom_shape[FW_MAX_SHAPE_DIMS];
  int tensor_dim;
  int op_code;
  int scale_A;
  int scale_B;
  int rshift_A;
  int rshift_B;
  int sign_A;
  int sign_B;
  int if_relu;

} bm_api_eltwise_binary_fix8b_forward_t;


typedef struct bm_api_const_binary_fix8b_forward {
  u64 bottom_A_global_addr;
  u64 top_global_addr;
  int bottom1_val;
  int bottom_shape[FW_MAX_SHAPE_DIMS];
  int tensor_dim;
  int op_code;
  int scale_A;
  int scale_B;
  int rshift_A;
  int rshift_B;
  int sign_A;
  int sign_B;
  int inversed;
  int if_relu;

} bm_api_const_binary_fix8b_forward_t;


typedef struct bm_api_broadcast_binary_fix8b_forward {
  u64 bottom_A_global_addr;
  u64 bottom_B_global_addr;
  u64 top_global_addr;
  int b0_shape[FW_MAX_SHAPE_DIMS];
  int b1_shape[FW_MAX_SHAPE_DIMS];
  int tensor_dim;
  int op_code;
  int scale_A;
  int scale_B;
  int rshift_A;
  int rshift_B;
  int sign_A;
  int sign_B;
  int if_relu;

} bm_api_broadcast_binary_fix8b_forward_t;


typedef struct bm_api_binary {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 C_global_offset;
  u64 arm_reserved_addr;
  int n;
  int c;
  int h;
  int w;
  int A_bcast_n;
  int A_bcast_c;
  int A_bcast_h;
  int A_bcast_w;
  int B_bcast_n;
  int B_bcast_c;
  int B_bcast_h;
  int B_bcast_w;
  int A_is_constant;
  float A_value;
  int B_is_constant;
  float B_value;
  int type;

} bm_api_binary_t;


typedef struct bm_api_simple_binary {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 C_global_offset;
  u64 arm_reserved_addr;
  u64 len;
  int A_is_constant;
  float A_value;
  int B_is_constant;
  float B_value;
  int type;

} bm_api_simple_binary_t;


#define UNARY_PARAM_MAX_LEN 8
typedef struct {
  u64 A_global_offset;
  u64 C_global_offset; /* output. */
  u64 length;
  int type;
  unsigned char param[UNARY_PARAM_MAX_LEN];

} bm_api_unary_t;


typedef struct bm_api_logical {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 C_global_offset; /* output. */
  u64 arm_reserved_addr;
  u64 length;
  int type;
  int A_is_constant;
  int B_is_constant;
  unsigned int A_constant;
  unsigned int B_constant;

} bm_api_logical_t;


typedef struct bm_api_ternary {
  u64 A_global_offset;
  u64 B_global_offset;
  u64 C_global_offset; /* output. */
  u64 arm_reserved_addr;
  u64 length;
  int type;
  int A_is_constant;
  int B_is_constant;
  float A_constant;
  float B_constant;

} bm_api_ternary_t;


typedef struct bm_api_select {
  u64 cond_global_offset;
  u64 then_global_offset;
  u64 else_global_offset;
  u64 select_global_offset;
  u64 arm_reserved_addr;
  int n;
  int c;
  int h;
  int w;
  int cond_bcast_n;
  int cond_bcast_c;
  int cond_bcast_h;
  int cond_bcast_w;
  int then_bcast_n;
  int then_bcast_c;
  int then_bcast_h;
  int then_bcast_w;
  int else_bcast_n;
  int else_bcast_c;
  int else_bcast_h;
  int else_bcast_w;
  int cond_is_constant;
  float cond_value;
  int then_is_constant;
  float then_value;
  int else_is_constant;
  float else_value;

} bm_api_select_t;


typedef struct bm_api_simple_select {
  u64 cond_global_offset;
  u64 then_global_offset;
  u64 else_global_offset;
  u64 select_global_offset;
  u64 arm_reserved_addr;
  u64 len;
  int cond_is_constant;
  float cond_value;
  int then_is_constant;
  float then_value;
  int else_is_constant;
  float else_value;

} bm_api_simple_select_t;


typedef struct {
  u64 input_global_offset;
  u64 output_global_offset;
  u64 length;
  int N;  // sum(x^(2^N))
  float multiplier;

} bm_api_sum_x2n_t;


typedef struct {
  u64 scores_addr;
  u64 bbox_deltas_addr;
  u64 anchor_scales_addr;
  float scale_factor;
  int feat_factor;
  int feat_w;
  int feat_h;
  int width;
  int height;
  int feat_stride;
  int anchor_scale_size;
  int base_size;
  float im_scale;
  int min_size;
  float base_threshold;
  int per_nms_topn;
  u64 output_proposal_addr;

} bm_api_cv_gen_proposal_t;


typedef struct bm_api_word2vec {
  u64 wi_global_offset;
  u64 wo_global_offset;
  u64 samples_global_offset;
  u64 examples_global_offset;
  u64 labels_global_offset;
  u64 arm_reserved_global_offset;
  float lr;
  int neg_sample_num;
  int example_num;
  int dims;
  int vocab_size;

} bm_api_word2vec_t;


typedef struct bm_api_ctcloss {
  u64 l_prime_global_offset;
  u64 y_b_global_offset;
  u64 log_alpha_global_offset;
  u64 log_beta_global_offset;
  u64 arm_reserved_global_offset;
  int num_classes;
  int lprime_s;
  int seq_l;

} bm_api_ctcloss_t;


typedef struct {
  u64 input_proposal_addr;
  int proposal_size;
  float nms_threshold;
  u64 output_proposal_addr;

} bm_api_cv_nms_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  u64 length;
  int convert_flag;

} bm_api_f32_convert_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  u64 length;
  int cast_flag;

} bm_api_cast_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  u64 length;
  u32 op_code;

} bm_api_triangle_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  u64 length;
  int what;

} bm_api_f32_is_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  u64 buffer_global_addr;
  int input_shape[FW_MAX_SHAPE_DIMS];
  int input_dims;
  int axis_list[FW_MAX_SHAPE_DIMS];
  int axis_number;
  int method;

} bm_api_reduce_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  u64 buffer_global_addr;
  int input_shape[FW_MAX_SHAPE_DIMS];
  int input_dims;
  int axis_list[FW_MAX_SHAPE_DIMS];
  int axis_number;
  int method;
  int keep_dims;
  int bottom_sign;
  int store_mode;
  float bottom_scale_val;
  float top_scale_val;

} bm_api_reduce_fix8b_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  int input_rows;
  int output_rows;
  int row_size;
  int segment_reduce_flag;

} bm_api_segment_reduce_t;



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


typedef struct {
  u64  in_addr;
  u64  index_addr;
  u64  out_addr;
  u64  data_buffer_addr;
  u64  data_buffer2_addr;
  u64  index_buffer_addr;
  int  type_len;
  int  input_shape[FW_MAX_SHAPE_DIMS];
  int  input_dims;
  int  sort_dim;
  bool is_argsort;
  bool stable;
  bool descending;
  char reserved;

} bm_api_sort_per_dim_t;


typedef struct {
  u64  in_addr;
  u64  index_addr;
  int  index_num;
  u64  out_addr;
  u64  buffer1_addr;
  u64  buffer2_addr;
  int  type_len;
  int  input_shape[FW_MAX_SHAPE_DIMS];
  int  input_dims;
  int  dim;

} bm_api_index_select_t;


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


typedef struct bm_api_pad {
  u64 src_global_offset;
  u64 dst_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int pad[4][2];
  float const_val;
  int type;

} bm_api_pad_t;


typedef struct bm_api_pad_fix8b {
  u64 src_global_offset;
  u64 dst_global_offset;
  int src_is_4N;
  u64 src_1N_global_offset;
  int dst_is_4N;
  u64 dst_1N_global_offset;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int pad[4][2];
  int const_val;
  int type;

} bm_api_pad_fix8b_t;


typedef struct bm_api_arg {
  u64 input_global_addr;
  u64 output_global_addr;
  u64 index_global_addr;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int axis;
  int method;

} bm_api_arg_t;


typedef struct bm_api_arg_fix8b {
  u64 input_global_addr;
  u64 input_1N_global_addr;
  u64 output_global_addr;
  u64 output_1N_global_addr;
  u64 index_global_addr;
  u64 imm_index_global_addr;
  int input_sign;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int axis;
  int method;

} bm_api_arg_fix8b_t;


typedef struct bm_api_shuffle_channel_forward {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int group_num;

} bm_api_shuffle_channel_forward_t;


typedef struct bm_api_shuffle_channel_fix8b_forward {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int group_num;

} bm_api_shuffle_channel_fix8b_forward_t;


typedef struct bm_api_split_tf_fix8b {
  u64 bottom_global_offset;
  u64 buffer_global_addr;
  u64 imm_global_addr;
  int *input_shape;
  int shape_size;
  int in_store_mode;
  int out_store_mode;
  int axis;
  int split_size[MAX_SPLIT_TF_OUTPUT_NUM];
  int split_num;
  u64 top_global_offset[MAX_SPLIT_TF_OUTPUT_NUM];

} bm_api_split_tf_fix8b_t;


typedef struct bm_api_cv_feature_match_t {
  u64 input_data_global_addr;
  u64 db_data_global_addr;
  u64 db_feature_global_addr;
  u64 output_sorted_similarity_global_addr;
  u64 output_sorted_index_global_addr;
  int batch_size;
  int feature_size;
  int db_size;

} bm_api_cv_feature_match_t;


typedef struct {
  u64 bottom_global_offset;
  u64 top_value_global_offset;
  u64 top_index_global_offset;
  int k;
  int dim;
  int input_n;
  int input_c;
  int input_h;
  int input_w;

} bm_api_topk_t;


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


#define BMDNN_API_WHERE_MAX_DIM_SIZE 8
typedef struct {
    u64             bottom_data_global_offset;
    u64             top_data_global_offset;
    u64             top_data_num_global_offset;
    u64             temp_data_global_offset;
    u64             arm_reserved_global_offset;
    int             dim_size;
    int             dim_values[BMDNN_API_WHERE_MAX_DIM_SIZE];

} bm_api_where_t;


typedef struct {
    u64          src_global_offset;
    u64          mask_global_offset;
    int          shape[FW_MAX_SHAPE_DIMS];
    int          dim;
    u64          dst_global_offset; // to save result
    u64          mask_count_global_offset; // to save mask count
    int          data_format_bytes;  // data type size, such as sizeof(float), sizeof(char)
    int          mask_format_bytes;  // mask type size, such as sizeof(float), sizeof(char)
    u64          arm_reserved_global_offset;

} bm_api_masked_select_t;


typedef struct {
  u64 bottom_global_offset;
  u64 top_global_offset;
  int dim;
  int input_n;
  int input_c;
  int input_h;
  int input_w;

} bm_api_cumsum_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  int shape_size;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int begin_mask;
  int end_mask;
  int begin_index[FW_MAX_STRIDE_DIMS];
  int end_index[FW_MAX_STRIDE_DIMS];
  int stride[FW_MAX_STRIDE_DIMS];

} bm_api_stride_slice_t;


typedef struct {
  u64 input_global_addr;
  u64 buffer_global_addr;
  u64 imm_global_addr;
  u64 output_global_addr;
  int *input_shape;
  int shape_size;
  int in_store_mode;
  int out_store_mode;
  int begin_mask;
  int end_mask;
  int *begin_index;
  int *end_index;
  int *stride;

} bm_api_stride_slice_fix8b_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  int *input_shape;
  int shape_size;
  int begin_mask;
  int end_mask;
  int *begin_index;
  int *end_index;
  int *stride;

} bm_api_stride_slice_md_t;


typedef struct bm_api_interp_forward_parallel {
  u64 ifmap_offset_global;
  u64 ofmap_offset_global;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int pad_bag;
  int pad_end;
  int output_h;
  int output_w;
  int platform_sp;

} bm_api_interp_forward_parallel_t;


typedef struct {
  u64 input_global_mem_addr;
  u64 output_global_mem_addr;
  u64 buffer_global_mem_addr;
  u64 imm_global_mem_addr;
  int input_shape[FW_MAX_SHAPE_DIMS];
  int input_dim;
  int in_store_mode;
  int out_store_mode;
  int block_sizes[2];  // must have 2 elements
  int crop_sizes[4];   // must have 4 elements

} bm_api_batch2space_fix8b_forward_t;


typedef struct {
  u64 input_global_mem_addr;
  u64 output_global_mem_addr;
  u64 buffer_global_mem_addr;
  int input_shape[FW_MAX_SHAPE_DIMS];
  int input_dim;
  int in_store_mode;
  int out_store_mode;
  int block_sizes[2];  // must have 2 elements
  int pad_sizes[4];    // must have 4 elements

} bm_api_space2batch_fix8b_forward_t;


typedef struct {
  u64 input_global_addr;
  u64 output_global_addr;
  int input_n;
  int input_c;
  int input_h;
  int input_w;
  int n;
  int classes;
  int coords;
  int background;
  int softmax;

} bm_api_yolo_t;


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
  u64   loc_global_offset;
  u64   conf_global_offset;
  u64   prior_global_offset;
  u64   top_global_offset;
  int   batch_num;
  int   num_prior;
  int   num_classes;
  int   num_loc_classes;
  int   share_location;
  int   background_label_id;
  int   top_k;
  int   code_type;
  int   keep_top_k;
  int   variance_encoded_in_target;
  float nms_threshold;
  float conf_threshold;
  float eta;

} bm_api_ssd_detect_out_t;


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


typedef struct {
  u64   input_proposal_addr;
  int   proposal_size;
  int   weighting_method;
  float sigma;
  u64   overlap_output_addr;
  u64   weighting_res_output_addr;

} bm_api_cv_soft_nms_t;



typedef struct {
    int enable;  // 0: disable, 1: enable
} bm_api_set_profile_t;



typedef struct {
    u64 arm_reserved_addr;
    u64 output_global_addr;
    u32 output_size;
    u32 byte_offset;
    u32 data_category;  // 0: profile_data, 1: profile extra data
} bm_api_get_profile_data_t;


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


typedef struct bm_api_conv3d_fix8b_parallel {
  u64 input_global_offset;
  u64 output_global_offset;
  u64 filter_global_offset;
  u64 bias_global_offset;
  int input_n;
  int input_c;
  int input_t;
  int input_h;
  int input_w;
  int groups;
  int output_c;
  int kt;
  int kh;
  int kw;
  int dt;
  int dh;
  int dw;
  int pad_t;
  int pad_t_after;
  int pad_h;
  int pad_h_after;
  int pad_w;
  int pad_w_after;
  int stride_t;
  int stride_h;
  int stride_w;
  int using_bias;
  int in_sign;
  int out_sign;
  int weight_sign;
  int bias_sign;
  int rshift_num;
  int if_relu;
  int upper_limit;

} bm_api_conv3d_fix8b_parallel_t;
#pragma pack(pop)

#endif  // BM_API_STRUCT_H
