#ifndef NATIVE_UTIL_H
#define NATIVE_UTIL_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int array_cmp(float *p_exp, float *p_got, int len,
              const char * const info_label, float delta);

int tri_array_cmp(float *p_exp, float *p_got, float *third_party,
                  int len, const char * const info_label,
                  float delta, int* err_idx);

void extract_1_bit_integer(unsigned int *dst_buf, unsigned int *src_buf,
                           int n, int c, int h, int w);
void extract_1_bit_float(float *dst_buf, unsigned int *src_buf,
                         int n, int c, int h, int w);
void set_1_bit(unsigned int *dst_buf, unsigned int *src_buf,
               int n, int c, int h, int w);
void set_index_bit(unsigned int *dst_buf, unsigned int *src_buf,
                   int n, int c, int h, int w, int index_bit_width);

void native_md_scalar(float *a, float *b, float *r,
                      int N, int C, int H, int W,
                      int op, bool result_add);
void native_md_cmp(float *a, float *b, float *c, float *d, float *y, float *r,
                   int N, int C, int H, int W,
                   bool a_is_const, float a_const,
                   bool b_is_const, float b_const,
                   bool c_is_const, float c_const,
                   bool d_is_const, float d_const);
void native_img_sum(float *A, float *Yi, float *Yo,
                    int N, int C, int H, int W,
                    int result_add);
void native_matrix_mac(float* A, float* B, float* C, int is_A_trans, int is_B_trans,
                       int row_A, int col_B, int col_A,
                       int add_result, int have_bias, float* Y);
void native_conv_ref(
    const void *ifmap, void *ofmap, const void *weight,
    int input_n,
    int input_c, int input_h, int input_w,
    int output_c, int output_h, int output_w,
    int groups,
    int kh, int kw,
    int dilation_h, int dilation_w,
    int pad_h, int pad_w,
    int stride_h, int stride_w,
    int flip,
    int using_bias,
    const void *bias,
    int result_add);

void native_pooling_forward_max(
    const float* bottom_data, float* top_data,
    int* mask_data,
    const int count,
    const int num, const int channels,
    const int height, const int width,
    const int pooled_height, const int pooled_width,
    const int kernel_h, const int kernel_w,
    const int stride_h, const int stride_w,
    const int pad_h, const int pad_w);

void native_pooling_forward_ave(
    const float* bottom_data, float* top_data,
    const int count,
    const int num, const int channels,
    const int height, const int width,
    const int pooled_height, const int pooled_width,
    const int kernel_h, const int kernel_w,
    const int stride_h, const int stride_w,
    const int pad_h, const int pad_w);

void native_pooling_backward_max(
    const float* top_diff, float* bottom_diff,
    const int* mask_data,
    const int count,
    const int num, const int channels,
    const int height, const int width,
    const int pooled_height, const int pooled_width,
    const int kernel_h, const int kernel_w,
    const int stride_h, const int stride_w,
    const int pad_h, const int pad_w);

void native_pooling_backward_ave(
    const float* top_diff, float* bottom_diff,
    const int count,
    const int num, const int channels, const int height,
    const int width, const int pooled_height, const int pooled_width,
    const int kernel_h, const int kernel_w, const int stride_h,
    const int stride_w, const int pad_h, const int pad_w);

void native_softmax_forward(float *bottom_data, float *top_data,
                            int Blob_N, int Blob_C, int Blob_H, int Blob_W);
void native_softmax_backward(float *top_data, float *top_diff, float *bottom_diff,
                             int Blob_N, int Blob_C, int Blob_H, int Blob_W);

void native_softmax_loss_bidirection(float *bottom_data, float *label,
                                     float normalizer, float *bottom_diff, float *loss,
                                     int Blob_N, int Blob_C, int Blob_H, int Blob_W);

void native_softmax_loss_forward(float *bottom_data, float *label, float normalizer,
                                 float *top_data, float *loss,
                                 int Blob_N, int Blob_C, int Blob_H, int Blob_W);

void native_softmax_loss_backward(float *top_data, float *label, float *bottom_diff,
                                  float loss, float normalizer,
                                  int Blob_N, int Blob_C, int Blob_H, int Blob_W);

static inline void get_relu_result(float* mat, float* relu_result, int size)
{
  for (int i = 0; i < size; i++)  {
    relu_result[i] = (mat[i] >= 0) ? mat[i] : 0.0f;
  }
}

static inline void assign_matrix_entries(float* mat, int size, int rand_seed)
{
  for (int i = 0; i < size; i++)
    mat[i] = (rand() % 1000 * 0.01f + rand_seed * 0.001f) * 1.0f;
}

static inline void assign_blob_entries(float* A, int size)
{
  for (int i = 0; i < size; i++)
    A[i] = (rand() % 1000 - 500) * 0.01f;
}

static inline void assign_label_entries(float* A, int size, int num_class)
{
  for (int i = 0; i < size; i++)
    A[i] = (float)(rand() % num_class);
}

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_UTIL_H */
