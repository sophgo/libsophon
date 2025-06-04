#include "cpu_index_put.h"
namespace bmcpu {
int cpu_index_putlayer::process(void *param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_index_put_param_t, p, param, param_size);
    const float *input = reinterpret_cast<float *>(input_tensors_[0]);
    const float *other = reinterpret_cast<float *>(input_tensors_[2]);
    float *output = reinterpret_cast<float *>(output_tensors_[0]);
    int inner = 1, outer = 1;
    for (size_t i = 0; i < input_shapes_[1].size(); ++i)
        outer *= input_shapes_[0][i];
    for (size_t i = input_shapes_[1].size(); i < input_shapes_[0].size(); ++i)
        inner *= input_shapes_[0][i];
    switch (p->mode) {
    case 1: {
        CPU_ASSERT(input_shapes_[1].size() == 1);
        memcpy(output, input, inner * outer * 4);
        const int *dt = reinterpret_cast<int *>(input_tensors_[1]);
        if (p->accumulate) {
            for (int i = 0; i < input_shapes_[1][0]; ++i) {
                float *ot = output + dt[i] * inner;
                const float *et = other + i * inner;
                for (int j = 0; j < inner; ++j)
                    ot[j] += et[j];
            }
        } else {
            for (int i = 0; i < input_shapes_[1][0]; ++i) {
                float *ot = output + dt[i] * inner;
                const float *et = other + i * inner;
                for (int j = 0; j < inner; ++j)
                    ot[j] = et[j];
            }
        }
    }
    break;
    case 0:
    default:
        CPU_ASSERT(0);
    }
    (*output_shapes_)[0] = input_shapes_[0];
    return 0;
}
int cpu_index_putlayer::reshape(void *param, int param_size,
                                const vector<vector<int>> &input_shapes,
                                vector<vector<int>> &output_shapes) {
    output_shapes[0] = input_shapes[0];
    return 0;
}
int cpu_index_putlayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
                              vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_INDEX_PUT, cpu_index_put);
} // namespace bmcpu
