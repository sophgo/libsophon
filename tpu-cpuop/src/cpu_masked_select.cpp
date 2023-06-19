#include "cpu_masked_select.h"
#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "cpu_layer.h"

namespace bmcpu {

static void adjust_shape(const vector<int> &src_shape, const vector<int> &mask_shape,
                         vector<int> &new_src_shape, vector<int> &new_mask_shape,
                         vector<int> &bcast_shape, bool bcast_from_begin)
{
  int src_dim = (int)src_shape.size();
  int mask_dim = (int)mask_shape.size();
  int bcast_dim = (int)bcast_shape.size();
  if (bcast_from_begin == false) {
    int index = bcast_dim;
    while (src_dim > 0 && mask_dim > 0) {
      index--;
      new_src_shape[index] = src_shape[--src_dim];
      new_mask_shape[index] = mask_shape[--mask_dim];
      bcast_shape[index] = std::max(new_src_shape[index], new_mask_shape[index]);
    }
    while (src_dim) {
      src_dim--;
      index--;
      bcast_shape[index] = src_shape[src_dim];
      new_src_shape[index] = src_shape[src_dim];
    }
    while (mask_dim) {
      mask_dim--;
      index--;
      bcast_shape[index] = mask_shape[mask_dim];
      new_mask_shape[index] = mask_shape[mask_dim];
    }
  } else {
    for (int i = 0; i < src_dim; i++) {
      new_src_shape[i] = src_shape[i];
    }
    for (int i = 0; i < mask_dim; i++) {
      new_mask_shape[i] = mask_shape[i];
    }
    for (int i = 0; i < bcast_dim; i++) {
      bcast_shape[i] = std::max(new_src_shape[i], new_mask_shape[i]);
    }
  }
}

static int masked_select(float *p_src, float *p_mask, float *p_dst, const vector<int> &src_shape,
                         const vector<int> &mask_shape, bool bcast_from_begin)
{
  int mask_count = 0;
  int src_dim = (int)src_shape.size();
  int mask_dim = (int)mask_shape.size();
  if (src_dim == 0 || mask_dim == 0) {
    return 0;
  }
  int new_dim_size = std::max(src_dim, mask_dim);
  vector<int> new_src_shape(new_dim_size, 1);
  vector<int> new_mask_shape(new_dim_size, 1);
  vector<int> bcast_shape(new_dim_size, 1);

  adjust_shape(src_shape, mask_shape, new_src_shape, new_mask_shape, bcast_shape, bcast_from_begin);
  int bcast_count = 1;
  for (int i = 0; i < new_dim_size; i++) {
    bcast_count *= bcast_shape[i];
  }

  vector<int> src_stride(new_dim_size, 1);
  vector<int> mask_stride(new_dim_size, 1);
  vector<int> bcast_stride(new_dim_size, 1);

  if (new_dim_size >= 2) {
    for (int idx = new_dim_size - 2; idx >= 0; idx--) {
      bcast_stride[idx] = bcast_stride[idx + 1] * bcast_shape[idx + 1];
      src_stride[idx] = src_stride[idx + 1] * new_src_shape[idx + 1];
      mask_stride[idx] = mask_stride[idx + 1] * new_mask_shape[idx + 1];
    }
  }
  for (int idx = 0; idx < bcast_count; idx++) {
    int total = idx, in_offset = 0, mask_offset = 0;
    for (size_t j = 0; j < new_dim_size; j++) {
      int shape_j = total / bcast_stride[j];
      total %= bcast_stride[j];
      in_offset += shape_j % new_src_shape[j] * src_stride[j];
      mask_offset += shape_j % new_mask_shape[j] * mask_stride[j];
    }
    if (p_mask[mask_offset] != 0.0f) {
      p_dst[mask_count++] = p_src[in_offset];
    }
  }
  return mask_count;
}

int cpu_masked_selectlayer::process(void *param, int param_size)
{
  setParam(param, param_size);
  (*output_shapes_)[0][0] = masked_select(input_tensors_[0], input_tensors_[1], output_tensors_[0],
                                          input_shapes_[0], input_shapes_[1], bcast_from_begin);
  return 0;
}

void cpu_masked_selectlayer::setParam(void *param, int param_size)
{
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_masked_select_param_t, cpu_param, param, param_size);
  bcast_from_begin = cpu_param->bcast_from_begin;
}

int cpu_masked_selectlayer::reshape(void *param, int param_size,
                               const vector<vector<int>> &input_shapes,
                               vector<vector<int>> &output_shapes)
{
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_masked_select_param_t, cpu_param, param, param_size);
  bool bcast_from_begin = cpu_param->bcast_from_begin;
  int src_dim = (int)input_shapes[0].size();
  int mask_dim = (int)input_shapes[1].size();
  int min_dim = std::min(src_dim, mask_dim);
  output_shapes[0].clear();
  if (src_dim == 0 || mask_dim == 0) {
    output_shapes[0].push_back(0);
    return 0;
  }
  int count = 1;
  if (bcast_from_begin) {
    while (src_dim > min_dim) {
      src_dim--;
      count *= input_shapes[0][src_dim];
    }
    while (mask_dim > min_dim) {
      mask_dim--;
      count *= input_shapes[1][mask_dim];
    }
    while (min_dim > 0) {
      min_dim--;
      count *= std::max(input_shapes[0][min_dim], input_shapes[1][min_dim]);
    }
  } else {
    while (src_dim > 0 && mask_dim > 0) {
      src_dim--;
      mask_dim--;
      count *= std::max(input_shapes[0][src_dim], input_shapes[1][mask_dim]);
    }
    while (src_dim > 0) {
      src_dim--;
      count *= input_shapes[0][src_dim];
    }
    while (mask_dim > 0) {
      mask_dim--;
      count *= input_shapes[1][mask_dim];
    }
  }
  output_shapes[0].push_back(count);
  return 0;
}

REGISTER_CPULAYER_CLASS(CPU_MASKED_SELECT, cpu_masked_select)

} /* namespace bmcpu */
