
#include <thread>
#include "cpu_repeat_interleave.h"
#include "cpu_layer.h"
namespace bmcpu {

template <typename T>
void repeat_interleave(const T *input,
                       const int *repeat,
                       T *output,
                       int inn_num,
                       int mid_num,
                       int out_num,
                       int acc_num) {
    for (int i = 0; i < inn_num; ++i) {
        const T* input_i = input + i * mid_num * out_num;
        T* output_i = output + i * acc_num * out_num;
        int run_num = 0;
        for (int j = 0; j < mid_num; run_num += repeat[j], ++j) {
            const T* input_ij = input_i + j * out_num;
            T* output_ij = output_i + run_num * out_num;
            for (int k = 0; k < repeat[j]; ++k) {
                memcpy(output_ij + k * out_num, input_ij, out_num * sizeof(T));
            }
        }
    }
}

int cpu_repeat_interleavelayer::process(void *param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_repeat_interleave_param_t, rip, param,
                                   param_size);
    const int dim = rip->dim;
    const auto& input_shape = input_shapes_[0];
    const auto& output_shape = (*output_shapes_)[0];

    uint64_t inn_num = 1, out_num = 1;
    for (int i = 0; i < dim; ++i) {
        assert(inn_num * input_shape[i] <= INT32_MAX);
        inn_num *= input_shape[i];
    }
    for (int i = dim + 1; i < (int)input_shape.size(); ++i) {
        assert(out_num * input_shape[i] <= INT32_MAX);
        out_num *= input_shape[i];
    }

    const float* input = input_tensors_[0];
    const int* repeat = (int*)input_tensors_[1];
    float* output = output_tensors_[0];
    repeat_interleave<float>(input,
                             repeat,
                             output,
                             (int)inn_num,
                             input_shape[dim],
                             (int)out_num,
                             output_shape[dim]);

    return 0;
}

int cpu_repeat_interleavelayer::reshape(
    void *param, int param_size,
    const vector<vector<int>> &input_shapes,
    vector<vector<int>> &output_shapes) {
    return 1;
}

int cpu_repeat_interleavelayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
        vector<int> &output_dtypes) {
    assert(input_dtypes[0] == CPU_DTYPE_FP32);
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_REPEAT_INTERLEAVE, cpu_repeat_interleave);
} /* namespace bmcpu*/
