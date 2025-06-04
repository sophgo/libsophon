#ifndef CPU_WHERE_H
#define CPU_WHERE_H

#include "cpu_layer.h"

/*
 *  input tensor: float input[shape1, shape2, ...]
 *  input param: int input dimension
 *  output tensor: int coordinates[M, dim], (M = shape1*shape2*...*shapedim)
 *                 ([-1,...,-1] is invalid coordinate)
 */

namespace bmcpu {

class cpu_wherelayer : public cpu_layer {
public:
    explicit cpu_wherelayer() {}
    virtual ~cpu_wherelayer() {}

    int process(void* param, int param_size);
    void setParam(void* param, int param_size);
    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      output_dtypes.assign(1, CPU_DTYPE_INT32);
      return 0;
    }

    inline int where1d(const float *input,
                       const std::vector<int>& shape,
                       int* output);
    inline int where2d(const float *input,
                       const std::vector<int>& shape,
                       int* output);
    inline int where3d(const float *input,
                       const std::vector<int>& shape,
                       int* output);
    inline int where4d(const float *input,
                       const std::vector<int>& shape,
                       int* output);
    inline int where5d(const float *input,
                       const std::vector<int>& shape,
                       int* output);
private:
    int dim;
};

} /* namespace bmcpu */

#endif // CPU_WHERE_H
