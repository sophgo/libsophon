#ifndef CPU_CROP_AND_RESIZE_H
#define CPU_CROP_AND_RESIZE_H

#include "cpu_layer.h"

/*
 *  输入为4个tensor，tensor[0]为输入为NCHW格式的image，tensor[1]为boxes，tensor[2]为box_index。
 *  param参数参考cpu_crop_and_resize_t，分别为method和extrapolation_value,crop_h, crop_w
 */

namespace bmcpu {

class cpu_crop_and_resizelayer : public cpu_layer {
public:
    explicit cpu_crop_and_resizelayer() {}
    virtual ~cpu_crop_and_resizelayer() {}

    int process(void* param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);

    int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
              vector<int>& output_dtypes)
    {
      output_dtypes.assign(1, input_dtypes[0]);
      return 0;
    }

};

} /* namespace bmcpu */

#endif // CPU_CROP_AND_RESIZE_H
