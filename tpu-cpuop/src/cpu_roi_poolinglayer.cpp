#include "cpu_roi_poolinglayer.h"
#include <math.h>
#include <float.h>
namespace bmcpu {

#define roi_max(a,b) (((a) > (b)) ? (a) : (b))
#define roi_min(a,b) (((a) > (b)) ? (b) : (a))

int cpu_roi_poolinglayer::process(void *param, int param_size)
{
  setParam(param, param_size);

  const float* bottom_data = input_tensors_[0];
  const float* bottom_rois = input_tensors_[1];
  float* top_data = output_tensors_[0];
  int num_rois = input_shapes_[1][0];

  int channels = input_shapes_[0][1];
  int height = input_shapes_[0][2];
  int width = input_shapes_[0][3];

  (*output_shapes_)[0].resize(4);
  (*output_shapes_)[0][0] = num_rois ;
  (*output_shapes_)[0][1] = channels;
  (*output_shapes_)[0][2] = pooled_height_;
  (*output_shapes_)[0][3] = pooled_width_;

  for (int n = 0; n < num_rois; ++n) {
    int roi_batch_ind = bottom_rois[0];
    int roi_start_w = round(bottom_rois[1] * spatial_scale_);
    int roi_start_h = round(bottom_rois[2] * spatial_scale_);
    int roi_end_w = round(bottom_rois[3] * spatial_scale_);
    int roi_end_h = round(bottom_rois[4] * spatial_scale_);
    //CPU_ASSERT(roi_batch_ind >= 1);
    //CPU_ASSERT(roi_batch_ind < batch_size);

    int roi_height = roi_max(roi_end_h - roi_start_h + 1, 1);
    int roi_width = roi_max(roi_end_w - roi_start_w + 1, 1);
    const float bin_size_h = (float)roi_height / (float)pooled_height_;
    const float bin_size_w = (float)roi_width / (float)pooled_width_;

    const float* batch_data = bottom_data +
                              roi_batch_ind * channels * height * width;

    for (int c = 0; c < channels; ++c) {
      for (int ph = 0; ph < pooled_height_; ++ph) {
        for (int pw = 0; pw < pooled_width_; ++pw) {
          // Compute pooling region for this output unit:
          int hstart = (int)floor(ph * bin_size_h);
          int wstart = (int)floor(pw * bin_size_w);
          int hend = (int)ceil((ph + 1) * bin_size_h);
          int wend = (int)(ceil((pw + 1) * bin_size_w));

          hstart = roi_min(roi_max(hstart + roi_start_h, 0), height);
          hend = roi_min(roi_max(hend + roi_start_h, 0), height);
          wstart = roi_min(roi_max(wstart + roi_start_w, 0), width);
          wend = roi_min(roi_max(wend + roi_start_w, 0), width);

          const int pool_index = ph * pooled_width_ + pw;
          float pool_value = -FLT_MAX;
          for (int h = hstart; h < hend; ++h) {
            for (int w = wstart; w < wend; ++w) {
              const int index = h * width + w;
              if (batch_data[index] > pool_value) {
                pool_value = batch_data[index];
              }
            }
          }
          top_data[pool_index] = pool_value;
        }
      }
      // Increment all data pointers by one channel
      batch_data += height * width;
      top_data += (*output_shapes_)[0][2] * (*output_shapes_)[0][3];
    }
    // Increment ROI data pointer
    bottom_rois += input_shapes_[1][1] * input_shapes_[1][2] * input_shapes_[1][3];
  }

  return 0;
}

void cpu_roi_poolinglayer::setParam(void *param, int param_size)
{
  layer_param_ = param;
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_roi_pooling_param_t, roi_pooling_param, layer_param_, param_size);
  pooled_height_ = roi_pooling_param->pooled_height_;
  pooled_width_ = roi_pooling_param->pooled_width_;
  spatial_scale_ = roi_pooling_param->spatial_scale_;
}

REGISTER_CPULAYER_CLASS(CPU_ROI_POOLING, cpu_roi_pooling)

}
