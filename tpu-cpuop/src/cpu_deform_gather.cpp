#include <algorithm>
#include <functional>
#include "cpu_deform_gather.h"


namespace bmcpu {

/**
 * [INFO]
 * DEFORM_MXNET_MODE/DEFORM_TORCH_CAFFE2_MODE
 *  Modified from
 *   https://github.com/pytorch/pytorch/blob/c371542efc31b1abfe6f388042aa3ab0cef935f2/caffe2/operators/deform_conv_op.cu
 * DEFORM_TORCHVISION_MODE
 *  Modified from
 *   https://github.com/pytorch/vision/blob/release/0.9/torchvision/csrc/ops/cpu/deform_conv2d_kernel.cpp
 *
 **/

#define MIN(x, y) (((x)) < ((y)) ? (x) : (y))
#define MAX(x, y) (((x)) > ((y)) ? (x) : (y))
#define CLIP(x, xmin, xmax) MIN(MAX((x), (xmin)), (xmax))

template <typename T>
static inline T index_2d(const T* arr, int height, int width, int y, int x)
{
  if (y <= -1 || height <= y || x <= -1 || width <= x)
    return 0;
  return arr[y * width + x];
}

template <typename T>
T bilinear_interpolate(
    const T* in,
    int height,
    int width,
    T h,
    T w,
    const int mode)
{
  const int b = mode == DEFORM_TORCH_CAFFE2_MODE;
  if (b) {
    if (h < 0 || height <= h || w < 0 || width <= w) {
        return 0;
    }
  } else {
    if (h <= -1 || height <= h || w <= -1 || width <= w) {
        return 0;
    }
  }

  int h_low = floor(h);
  int w_low = floor(w);
  int h_high = h_low + 1;
  int w_high = w_low + 1;

  if (b) {
    h_high = MIN(h_high, height - 1);
    w_high = MIN(w_high, width - 1);
  }

  T lh = h - h_low;
  T lw = w - w_low;
  T hh = 1 - lh, hw = 1 - lw;

  T v1 = index_2d(in, height, width, h_low, w_low);
  T v2 = index_2d(in, height, width, h_low, w_high);
  T v3 = index_2d(in, height, width, h_high, w_low);
  T v4 = index_2d(in, height, width, h_high, w_high);

  T w1 = hh * hw, w2 = hh * lw, w3 = lh * hw, w4 = lh * lw;

  T val = (w1 * v1 + w2 * v2 + w3 * v3 + w4 * v4);
  return val;
}

template <typename T>
static inline void deform_process_per_kernel(
    const T* data_img,
    const T* data_offset,
    const T* data_mask,
    T* data_out,
    const int H,
    const int W,
    const int kh,
    const int kw,
    const int dilation_h,
    const int dilation_w,
    const int num_kernels_per_channel,
    const int in_h,
    const int in_w,
    const int mode) {
  for (int h = 0; h < kh; ++h) {
    for (int w = 0; w < kw; ++w) {
      const int out_idx = (h * kw + w) * num_kernels_per_channel;
      const int mask_idx = (h * kw + w) * num_kernels_per_channel;
      const int offset_h_idx = mask_idx * 2;
      const int offset_w_idx = offset_h_idx + num_kernels_per_channel;
      const T offset_h = data_offset[offset_h_idx];
      const T offset_w = data_offset[offset_w_idx];
      const T h_im = in_h + h * dilation_h + offset_h;
      const T w_im = in_w + w * dilation_w + offset_w;

      T val = bilinear_interpolate(data_img,
                                   H, W,
                                   h_im, w_im,
                                   mode);
      if (data_mask) {
        val *= data_mask[mask_idx];
      }
      data_out[out_idx] = val;
    }
  }
}


template <typename T>
static inline void deform_process_v1(
    const T* data_img,
    const T* data_offset,
    const T* data_mask,
    T* data_out,
    const int H,
    const int W,
    const int conved_H,
    const int conved_W,
    const int kh,
    const int kw,
    const int dilation_h,
    const int dilation_w,
    const int stride_h,
    const int stride_w,
    const int pad_t,
    const int pad_l,
    const int mode) {
  // process input features as the next layer is still conv layer
  // with new params, such as stride, dilation and pad.
  const int num_kernels_per_channel = conved_H * conved_W;
  for (int h = 0; h < conved_H; ++h) {
    for (int w = 0; w < conved_W; ++w) {
      const int in_h = stride_h * h - pad_t;
      const int in_w = stride_w * w - pad_l;
      const T* data_offset_ptr = data_offset + h * conved_W + w;
      const T* data_mask_ptr = (data_mask ? data_mask + h * conved_W + w :
                                nullptr);
      T* data_out_ptr = data_out + h * conved_W + w;
      deform_process_per_kernel(data_img, data_offset_ptr,
                                data_mask_ptr, data_out_ptr,
                                H, W, kh, kw, dilation_h, dilation_w,
                                num_kernels_per_channel, in_h, in_w, mode);

    }
  }
}

int cpu_deform_gatherlayer::process(void* param, int param_size) {
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_deform_gather_param_t,
                                 p, param, param_size);

  // data: [N, C, H, W]
  // offset： [N, num_deform_group*kh*kw*2, conved_H, conved_W]
  // mask： [N, num_deform_group*kh*kw, conved_H, conved_W]
  // out: [N, C, conved_H*kh, conved_W*kw]
  CPU_ASSERT_EQ(output_tensors_.size(), 1);
  const vector<int> data_shape = input_shapes_[0];
  const vector<int> offset_shape = input_shapes_[1];
  int offset_dim = offset_shape.size();
  CPU_ASSERT_EQ(offset_dim, 4);

  const int N = data_shape[0];
  const int C = data_shape[1];
  const int H = data_shape[2];
  const int W = data_shape[3];
  const int conved_H = ((H - (p->dilation_h * (p->kh - 1) + 1) +
      p->pad_t + p->pad_b) / p->stride_h + 1);
  const int conved_W = ((W - (p->dilation_w * (p->kw - 1) + 1) +
      p->pad_l + p->pad_r) / p->stride_w + 1);

  const int offset_offset = (offset_shape[1] * offset_shape[2] *
      offset_shape[3] / p->deform_groups);
  const int channel_per_deform_group = C / p->deform_groups;

  const float* data_img = (float*)input_tensors_[0];
  const float* data_offset = (float*)input_tensors_[1];
  float* data_out = (float*)output_tensors_[0];

  float* data_mask = nullptr;
  vector<int> mask_shape;
  int mask_offset = 0;
  if (p->modulated) {
    mask_shape = input_shapes_[2];
    data_mask = (float*)input_tensors_[2];
    mask_offset = (mask_shape[1] * mask_shape[2] *
        mask_shape[3] / p->deform_groups);
  }

  for (int n = 0; n < N; ++n) {
    for (int c = 0; c < C; ++c) {
      const int g = c / channel_per_deform_group;
      const int in_idx = (n * C + c) * H * W;
      const int out_idx = (n * C + c) * p->kh * conved_H * p->kw * conved_W;
      const int offset_idx = (n * p->deform_groups + g) * offset_offset;
      const int mask_idx = (n * p->deform_groups + g) * mask_offset;

      deform_process_v1<float>(data_img + in_idx,
                               data_offset + offset_idx,
                               (p->modulated ? data_mask + mask_idx : nullptr),
                               data_out + out_idx,
                               H, W, conved_H, conved_W,
                               p->kh, p->kw, p->dilation_h, p->dilation_w,
                               p->stride_h, p->stride_w,
                               p->pad_t, p->pad_l, p->mode);
    }
  }

  return 0;
}

REGISTER_CPULAYER_CLASS(CPU_DEFORM_GATHER, cpu_deform_gather)

} // namespace bmcpu
