#ifndef CPU_RESIZE_INTERPOLATION_H
#define CPU_RESIZE_INTERPOLATION_H
#include "cpu_layer.h"
/*
 *  input tensor: float image[n,c,h,w], int output_shape=[h, w]
 *  input param: float paltform_sp, float intepolation_method
 *  output tensor: float resized_image
 */
namespace bmcpu {
class cpu_resize_interpolationlayer : public cpu_layer {
public:
    explicit cpu_resize_interpolationlayer() {}
    virtual ~cpu_resize_interpolationlayer() {}
    int process(void *param, int param_size);
    int reshape(void *param, int param_size,
                const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes);
    int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes);
};
} /* namespace bmcpu */
#endif // CPU_RESIZE_INTERPOLATION_H
