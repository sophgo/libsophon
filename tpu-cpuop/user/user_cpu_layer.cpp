#include "user_cpu_layer.h"

namespace usercpu {

int user_cpu_layer::set_common_param(
           const vector<float *>& input_tensors,
           const vector<vector<int>>& input_shapes,
           const vector<float *>& output_tensors,
           vector<vector<int>>& output_shapes)
{
    USER_ASSERT(input_tensors.size() == input_shapes.size());
    USER_ASSERT(output_tensors.size() == output_shapes.size());

    input_tensors_    = input_tensors;
    input_shapes_     = input_shapes;

    output_tensors_   = output_tensors;
    output_shapes_    = &output_shapes;

    return 0;
}

}
