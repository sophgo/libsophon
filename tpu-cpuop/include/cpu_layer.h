#ifndef _CPU_LAYER_H_
#define _CPU_LAYER_H_

#include <memory>
#include "bmcpu_macro.h"
#include "bmcpu_common.h"
#include "cpu_layer_factory.h"

namespace bmcpu {

inline int bmcpu_shape_count(vector<int> shape_v)
{
    int count = 1;
    for(auto& shape : shape_v)
        count *= shape;

    return count;
}

class cpu_layer {
public:
    cpu_layer() {}
    ~cpu_layer() {}

    /* dowork */
    virtual int process(void *parm, int param_size) = 0;

    int set_common_param(
          const vector<float *>& input_tensors,
          const vector<vector<int>>& input_shapes,
          const vector<float *>& output_tensors,
          vector<vector<int>>& output_shape);

    virtual int reshape(
          void* param, int param_size,
          const vector<vector<int>>& input_shapes,
          vector<vector<int>>& output_shapes) = 0;

    virtual int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
                      vector<int> &output_dtypes) {
        std::cout << "Output dtypes cannot be obtained." << std::endl;
        return -1;
    }

protected:

    vector<float *> input_tensors_;
    //vector<vector<int>> input_shape;
    vector<vector<int>> input_shapes_;

    vector<float *> output_tensors_;  /* TODO: int8 */
    //vector<vector<int>> output_shape;
    //vector<vector<int>> output_shapes_;
    vector<vector<int>>* output_shapes_;

    /* layer specific param */
    void* layer_param_;
};

} /* namespace bmcpu */

#endif /* _CPU_LAYER_H_ */
