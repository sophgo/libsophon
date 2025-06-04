#include <algorithm>
#include <functional>
#include "cpu_deform_psroipooling.h"

namespace bmcpu {

typedef unsigned int index_t;
using std::max;
using std::min;

// Modified from
// https://github.com/apache/incubator-mxnet/blob/5722f8b38af58c5a296e46ca695bfaf7cff85040/src/operator/contrib/deformable_psroi_pooling.cc#L62
template <typename DType>
inline DType bilinear_interp_cpu(const DType* data,
                                 const DType x, const DType y,
                                 const index_t width, const index_t height) {
  index_t x1 = floor(x);
  index_t x2 = ceil(x);
  index_t y1 = floor(y);
  index_t y2 = ceil(y);
  DType dist_x = static_cast<DType>(x - x1);
  DType dist_y = static_cast<DType>(y - y1);
  DType value11 = data[y1 * width + x1];
  DType value12 = data[y2 * width + x1];
  DType value21 = data[y1 * width + x2];
  DType value22 = data[y2 * width + x2];
  DType value = (1 - dist_x) * (1 - dist_y) * value11 + (1 - dist_x) * dist_y * value12 +
    dist_x * (1 - dist_y) * value21 + dist_x * dist_y * value22;
  return value;
}

template <typename DType>
inline void DeformablePSROIPoolForwardCPU(const index_t count, const DType* bottom_data,
                                          const DType spatial_scale, const index_t channels,
                                          const index_t height, const index_t width,
                                          const index_t pooled_height, const index_t pooled_width,
                                          const DType* bottom_rois, const DType* bottom_trans,
                                          const bool no_trans, const DType trans_std,
                                          const index_t sample_per_part, const index_t output_dim,
                                          const index_t group_size, const index_t part_size,
                                          const index_t num_classes,
                                          const index_t channels_each_class,
                                          DType* top_data) { // , DType* top_count) {
  for (index_t index = 0; index < count; index++) {
    // The output is in order (n, ctop, ph, pw)
    index_t pw = index % pooled_width;
    index_t ph = (index / pooled_width) % pooled_height;
    index_t ctop = (index / pooled_width / pooled_height) % output_dim;
    index_t n = index / pooled_width / pooled_height / output_dim;

    // [start, end) interval for spatial sampling
    const DType* offset_bottom_rois = bottom_rois + n * 5;
    index_t roi_batch_ind = offset_bottom_rois[0];
    DType roi_start_w = static_cast<DType>(round(offset_bottom_rois[1])) * spatial_scale - 0.5;
    DType roi_start_h = static_cast<DType>(round(offset_bottom_rois[2])) * spatial_scale - 0.5;
    DType roi_end_w = static_cast<DType>(round(offset_bottom_rois[3]) + 1.) * spatial_scale - 0.5;
    DType roi_end_h = static_cast<DType>(round(offset_bottom_rois[4]) + 1.) * spatial_scale - 0.5;

    // Force too small ROIs to be 1x1
    DType roi_width = max(roi_end_w - roi_start_w, static_cast<DType>(0.1));  // avoid 0
    DType roi_height = max(roi_end_h - roi_start_h, static_cast<DType>(0.1));

    // Compute w and h at bottom
    DType bin_size_h = roi_height / static_cast<DType>(pooled_height);
    DType bin_size_w = roi_width / static_cast<DType>(pooled_width);

    DType sub_bin_size_h = bin_size_h / static_cast<DType>(sample_per_part);
    DType sub_bin_size_w = bin_size_w / static_cast<DType>(sample_per_part);

    index_t part_h = floor(static_cast<DType>(ph) / pooled_height * part_size);
    index_t part_w = floor(static_cast<DType>(pw) / pooled_width * part_size);
    index_t class_id = ctop / channels_each_class;
    DType trans_x = no_trans ? static_cast<DType>(0) :
      bottom_trans[(((n * num_classes + class_id) * 2)
                      * part_size + part_h)
                      * part_size + part_w] * trans_std;
    DType trans_y = no_trans ? static_cast<DType>(0) :
      bottom_trans[(((n * num_classes + class_id) * 2 + 1)
                      * part_size + part_h)
                      * part_size + part_w] * trans_std;

    DType wstart = static_cast<DType>(pw) * bin_size_w + roi_start_w;
    wstart += trans_x * roi_width;
    DType hstart = static_cast<DType>(ph) * bin_size_h + roi_start_h;
    hstart += trans_y * roi_height;

    DType sum = 0;
    index_t count = 0;
    index_t gw = floor(static_cast<DType>(pw) * group_size / pooled_width);
    index_t gh = floor(static_cast<DType>(ph) * group_size / pooled_height);
    gw = min(max(gw, static_cast<index_t>(0)), group_size - 1);
    gh = min(max(gh, static_cast<index_t>(0)), group_size - 1);

    const DType* offset_bottom_data = bottom_data + (roi_batch_ind * channels) * height * width;
    for (index_t ih = 0; ih < sample_per_part; ih++) {
      for (index_t iw = 0; iw < sample_per_part; iw++) {
        DType w = wstart + iw * sub_bin_size_w;
        DType h = hstart + ih * sub_bin_size_h;
        // bilinear interpolation
        if (w < -0.5 || w > width - 0.5 || h < -0.5 || h > height - 0.5) {
          continue;
        }
        w = min(max(w, static_cast<DType>(0)), static_cast<DType>(width - 1));
        h = min(max(h, static_cast<DType>(0)), static_cast<DType>(height - 1));
        index_t c = (ctop * group_size + gh) * group_size + gw;
        DType val = bilinear_interp_cpu(offset_bottom_data + c * height * width,
                                        w, h, width, height);
        sum += val;
        count++;
      }
    }
    top_data[index] = count == 0 ? static_cast<DType>(0) : sum / count;
    // top_count[index] = count;
  }
}

int cpu_deform_psroipoolinglayer::process(void* param, int param_size) {
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_deform_psroipooling_param_t,
                                 p, param, param_size);
  
  // data:  [batch_size, c, h, w]
  // rois(bbox): [num_rois, 5]
  // trans: [num_rois, 2, pooled_h, pooled_w]
  // out: [num_rois, c, pooled_h, pooled_w]
  // top_count: [num_rois, c, pooled_h, pooled_w]
  const float* bottom_data = (float*)input_tensors_[0];
  const float* bottom_rois = (float*)input_tensors_[1];
  const float* bottom_trans = p->no_trans ? nullptr : input_tensors_[2];
  float* top_data = (float*)output_tensors_[0];
  // float* top_count_data = (float*)output_tensors_[1];

  vector<int> data_shape = input_shapes_[0];
  vector<int> out_shape = (*output_shapes_)[0];
  int top_count = 1;
  for (int i = 0; i < out_shape.size(); ++i) {
    top_count *= out_shape[i];
  }
  const index_t channels = data_shape[1];
  const index_t height = data_shape[2];
  const index_t width = data_shape[3];
  const index_t pooled_height = p->pooled_size;
  const index_t pooled_width = p->pooled_size;
  const index_t num_classes = p->no_trans ? 1 : input_shapes_[2][1] / 2;
  const index_t channels_each_class = (p->no_trans ? p->output_dim :
      p->output_dim / num_classes);
  DeformablePSROIPoolForwardCPU<float>(
      top_count, bottom_data, p->spatial_scale,
      channels, height, width,
      pooled_height, pooled_width,
      bottom_rois, bottom_trans,
      p->no_trans, p->trans_std, p->sample_per_part,
      p->output_dim, p->group_size, p->part_size, num_classes,
      channels_each_class, top_data); //, top_count_data);
  return 0;
}

REGISTER_CPULAYER_CLASS(CPU_DEFORM_PSROIPOOLING, cpu_deform_psroipooling)
}