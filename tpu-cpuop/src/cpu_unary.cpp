#include <string>
#include <vector>
#include <cmath>
#include "cpu_unary.h"
#include "cpu_layer.h"
#include "bmcpu_common.h"

namespace bmcpu {

int cpu_unarylayer::process(void* param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_unary_param_t, params, param, param_size);
    UNARY_OP_CODE_T op = params->unary_op;
    const float* input = input_tensors_[0];
    std::vector<int> input_shape = input_shapes_[0];
    float *output = (output_tensors_[0]);
    int input_size = 1;
    for(auto s: input_shape){
        input_size *= s;
    }

    switch (op) {
    case OP_SIN:
        for (int i=0; i<input_size; ++i) {
            output[i] = std::sin(input[i]);
        }
        break;
    case OP_COS:
        for (int i=0; i<input_size; ++i) {
            output[i] = std::cos(input[i]);
        }
        break;
    case OP_ISFINITE:
        for (int i=0; i<input_size; ++i) {
            output[i] = std::isfinite(input[i]) ? 1.f : 0.f;
        }
        break;
    case OP_ROUND:
        for (int i=0; i<input_size; ++i) {
            output[i] = std::round(input[i]);
        }
        break;
    case OP_FLOOR:
        for (int i=0; i<input_size; ++i) {
            output[i] = std::floor(input[i]);
        }
        break;
    case OP_CEIL:
        for (int i=0; i<input_size; ++i) {
            output[i] = std::ceil(input[i]);
        }
        break;
    default:
        printf("error: %s: %d: no match operation. \n", __FILE__, __LINE__);
        exit(-1);
        break;
    }

    (*output_shapes_).assign(1, input_shapes_[0]);

    return 0;

}

REGISTER_CPULAYER_CLASS(CPU_UNARY, cpu_unary);

} /* namespace bmcpu*/
