#ifndef CPU_SCATTER_ND_H
#define CPU_SCATTER_ND_H
#include "cpu_layer.h"
/*
 *  input tensor: float image[n,c,h,w], int output_shape=[h, w]
 *  input param: float paltform_sp, float intepolation_method
 *  output tensor: float resized_image
 */
namespace bmcpu {
class cpu_scatter_ndlayer : public cpu_layer {
public:
    explicit cpu_scatter_ndlayer() {}
    virtual ~cpu_scatter_ndlayer() {}
    int process(void *param, int param_size);
    int reshape(void *param, int param_size,
                const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes);
    int dtype(const void *param, size_t param_size,
              const vector<int> &input_dtypes, vector<int> &output_dtypes);
};
} /* namespace bmcpu */
#endif // CPU_SCATTER_ND_H
