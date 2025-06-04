#include "cpu_paddle_deformable_conv.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <queue>

namespace bmcpu {

typedef struct matmul_attr_s {
    int m,  n,  k;
    void* packed_weight{nullptr};
    matmul_attr_s() = default;
    explicit matmul_attr_s(int m_,  int n_,  int k_,  void* packed_weight_ = nullptr)
        : m(m_),  n(n_),  k(k_),  packed_weight(packed_weight_) {}
} matmul_attr_t;

template <typename T>
void MatMul(const T* A,  const T* B,  T* C,  const matmul_attr_t* attr) {
    int M = attr->m;
    int N = attr->n;
    int K = attr->k;
    for (int m = 0; m < M; ++m) {
        const T* pa = A + m * K;
        T* pc = C + m * N;
        for (int n = 0; n < N; ++n) {
            const T* pb = B + n;
            pc[n] = pa[0] * pb[0];
            for (int k = 1; k < K; ++k) {
                pc[n] += pa[k] * pb[k * N];
            }
        }
    }
}

template <typename T>
T DmcnIm2colBilinear(const T* bottom_data, const int data_width,
                     const int height, const int width, T h, T w) {
    int h_low = floor(h);
    int w_low = floor(w);
    int h_high = h_low + 1;
    int w_high = w_low + 1;

    T lh = h - h_low;
    T lw = w - w_low;
    T hh = 1 - lh;
    T hw = 1 - lw;

    T v1 =
        (h_low >= 0 && w_low >= 0) ? bottom_data[h_low * data_width + w_low] : 0;
    T v2 = (h_low >= 0 && w_high <= width - 1)
           ? bottom_data[h_low * data_width + w_high]
           : 0;
    T v3 = (h_high <= height - 1 && w_low >= 0)
           ? bottom_data[h_high * data_width + w_low]
           : 0;
    T v4 = (h_high <= height - 1 && w_high <= width - 1)
           ? bottom_data[h_high * data_width + w_high]
           : 0;

    T w1 = hh * hw;
    T w2 = hh * lw;
    T w3 = lh * hw;
    T w4 = lh * lw;

    return w1 * v1 + w2 * v2 + w3 * v3 + w4 * v4;
}

template <typename T>
void ModulatedDeformableIm2colCPUKernel(
    const int num_kernels, const T* data_im, const T* data_offset,
    const T* data_mask, const int height, const int width, const int kernel_h,
    const int kernel_w, const int pad_h, const int pad_w, const int stride_h,
    const int stride_w, const int dilation_h, const int dilation_w,
    const int channel_per_deformable_group, const int batch_size,
    const int num_channels, const int deformable_group, const int height_col,
    const int width_col, T* data_col) {
    for (int i = 0; i < num_kernels; i++) {
        const int w_col = i % width_col;
        const int h_col = (i / width_col) % height_col;
        const int b_col = (i / width_col) / height_col % batch_size;
        const int c_im = (i / width_col / height_col) / batch_size;
        const int c_col = c_im * kernel_h * kernel_w;

        const int deformable_group_index = c_im / channel_per_deformable_group;

        const int h_in = h_col * stride_h - pad_h;
        const int w_in = w_col * stride_w - pad_w;

        T* data_col_ptr =
            data_col +
            ((c_col * batch_size + b_col) * height_col + h_col) * width_col + w_col;
        const T* data_im_ptr =
            data_im + (b_col * num_channels + c_im) * height * width;
        const T* data_offset_ptr =
            data_offset +
            (b_col * deformable_group + deformable_group_index) * 2 * kernel_h *
            kernel_w * height_col * width_col;
        const T* data_mask_ptr =
            data_mask +
            (b_col * deformable_group + deformable_group_index) * kernel_h *
            kernel_w * height_col * width_col;

        for (int i = 0; i < kernel_h; ++i) {
            for (int j = 0; j < kernel_w; ++j) {
                const int data_offset_h_ptr =
                    ((2 * (i * kernel_w + j)) * height_col + h_col) * width_col + w_col;
                const int data_offset_w_ptr =
                    ((2 * (i * kernel_w + j) + 1) * height_col + h_col) * width_col +
                    w_col;
                const int data_mask_hw_ptr =
                    ((i * kernel_w + j) * height_col + h_col) * width_col + w_col;

                const T offset_h = data_offset_ptr[data_offset_h_ptr];
                const T offset_w = data_offset_ptr[data_offset_w_ptr];
                const T mask = data_mask_ptr[data_mask_hw_ptr];
                T val = static_cast<T>(0);
                const T h_im = h_in + i * dilation_h + offset_h;
                const T w_im = w_in + j * dilation_w + offset_w;
                if (h_im > -1 && w_im > -1 && h_im < height && w_im < width) {
                    val =
                        DmcnIm2colBilinear(data_im_ptr, width, height, width, h_im, w_im);
                }
                *data_col_ptr = val * mask;
                data_col_ptr += batch_size * height_col * width_col;
            }
        }
    }
}

template <typename T>
static inline void ModulatedDeformableIm2colCPU(
    const T* data_im,
    const T* data_offset, const T* data_mask,
    const std::vector<int64_t> im_shape, const std::vector<int64_t> col_shape,
    const int kh, const int kw, const std::vector<int> paddings,
    const std::vector<int> strides, const std::vector<int> dilations,
    const int deformable_groups, T* data_col) {

    int channel_per_deformable_group = im_shape[0] / deformable_groups;
    int num_kernels = im_shape[0] * col_shape[1] * col_shape[2] * col_shape[3];

    // get outputs of im2col with offset by bilinear interpolation
    ModulatedDeformableIm2colCPUKernel(
        num_kernels, data_im, data_offset, data_mask, im_shape[1], im_shape[2],
        kh, kw, paddings[0], paddings[1], strides[0],
        strides[1], dilations[0], dilations[1], channel_per_deformable_group,
        col_shape[1], im_shape[0], deformable_groups, col_shape[2], col_shape[3],
        data_col);

}

int cpu_paddle_deformable_convlayer::process(void *param, int psize) {

    setParam(param, psize);
    CPU_ASSERT_EQ(output_tensors_.size(), 1);
    const vector<int> data_shape = input_shapes_[0];
    const vector<int> offset_shape = input_shapes_[1];
    int offset_dim = offset_shape.size();
    CPU_ASSERT_EQ(offset_dim, 4);

    const int N = data_shape[0];
    const int C = data_shape[1];
    const int H = data_shape[2];
    const int W = data_shape[3];

    const float* data_img = (float*)input_tensors_[0];
    const float* data_offset = (float*)input_tensors_[1];
    float* data_out = (float*)output_tensors_[0];

    float* data_mask = nullptr;
    vector<int> mask_shape;

    mask_shape = input_shapes_[2];
    data_mask = (float*)input_tensors_[2];

    int im2col_step = im2col_step_;
    int input_dim = C*H*W;
    int input_offset_dim = offset_shape[1] * offset_shape[2] * offset_shape[3];
    int input_mask_dim = mask_shape[1] * mask_shape[2] * mask_shape[3];
    std::vector<int64_t> input_shape_vec = {C, H, W};
    std::vector<int> paddings = {pad_[0], pad_[1]};
    std::vector<int> strides = {stride_[0], stride_[1]};
    std::vector<int> dilations = {dilation_[0], dilation_[1]};

    std::vector<int> filter_shape_vec;
    filter_shape_vec.resize(4);
    std::vector<int64_t> col_buffer_shape_vec(filter_shape_vec.size());
    col_buffer_shape_vec[0] = C * kh_ * kw_;
    col_buffer_shape_vec[1] = im2col_step;
    for (size_t j = 0; j < filter_shape_vec.size() - 2; ++j) {
        col_buffer_shape_vec[j + 2] = (*output_shapes_)[0][j + 2];
    }

    int out_offset = im2col_step*(*output_shapes_)[0][1]*im2col_step*(*output_shapes_)[0][2]*(*output_shapes_)[0][3];

    for (int i = 0; i < N/im2col_step; ++i) {
        ModulatedDeformableIm2colCPU<float>(
            data_img + i * im2col_step * input_dim,
            data_offset + i * im2col_step * input_offset_dim,
            data_mask + i * im2col_step * input_mask_dim,  input_shape_vec,
            col_buffer_shape_vec,  kh_, kw_,  paddings,  strides,  dilations,
            deform_groups_,  data_out+i*out_offset);
    }

    return 0;

}


void cpu_paddle_deformable_convlayer::setParam(void *param, int psize) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_deformconv_param_t, deformable_conv_param, layer_param_, psize);
    modulated_ = deformable_conv_param->modulated;
    groups_ = deformable_conv_param->groups;
    deform_groups_ = deformable_conv_param->deform_groups;
    kh_ = deformable_conv_param->kh;
    kw_ = deformable_conv_param->kw;
    pad_[0] = deformable_conv_param->pad[0];
    pad_[1] = deformable_conv_param->pad[1];
    stride_[0] = deformable_conv_param->stride[0];
    stride_[1] = deformable_conv_param->stride[1];
    dilation_[0] = deformable_conv_param->dilation[0];
    dilation_[1] = deformable_conv_param->dilation[1];
    im2col_step_ = deformable_conv_param->im2col_step;
}

int cpu_paddle_deformable_convlayer::reshape(
    void* param, int psize,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_deformconv_param_t, p, param, psize);

    vector<int> data_shape = input_shapes[0];
    const int H = data_shape[2];
    const int W = data_shape[3];
    const int conved_H = ((H - (p->dilation[0]* (p->kh - 1) + 1) +
                           p->pad[0] + p->pad[0]) / p->stride[0] + 1);
    const int conved_W = ((W - (p->dilation[0] * (p->kw - 1) + 1) +
                           p->pad[1] + p->pad[1]) / p->stride[1] + 1);

    output_shapes[0].resize(4);
    output_shapes[0][0] = data_shape[0];
    output_shapes[0][1] = data_shape[1]*p->kh*p->kw/p->groups*p->im2col_step;
    output_shapes[0][2] = conved_H;
    output_shapes[0][3] = conved_W;

    return 0;
}

int cpu_paddle_deformable_convlayer::dtype(
    const void *param, size_t psize,
    const vector<int>& input_dtypes,
    vector<int>& output_dtypes) {
    CPU_ASSERT(input_dtypes.size() > 0);
    output_dtypes = {input_dtypes[0]};
    return 0;
}


REGISTER_CPULAYER_CLASS(CPU_PADDLE_DEFORM_CONV, cpu_paddle_deformable_conv);
}/* namespace bmcpu*/
