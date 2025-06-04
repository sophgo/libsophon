#include "cpu_pytorch_index.h"
#define INT_PTR(p) reinterpret_cast<int *>(p)
namespace bmcpu {
int cpu_pytorch_indexlayer::process(void *param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_pytorch_index_param_t, rip, param,
                                   param_size);
    int dim = input_shapes_[0].size();
    int D = input_shapes_[1][0];
    auto shape = input_shapes_[0];
    std::vector<unsigned long long> strides(dim, 1);
    for (int i = dim - 1; i >= 0; --i)
        for (int j = 0; j < i; ++j)
            strides[j] *= shape[i];
    unsigned long long outer = 1, inner = 1;
    for (int i = 0; i < rip->start; ++i)
        outer *= shape[i];
    for (int i = rip->end; i < shape.size(); ++i)
        inner *= shape[i];
    unsigned long long ostr = strides[rip->start - 1];
    const float *input = reinterpret_cast<float *>(input_tensors_[0]);
    float *output = reinterpret_cast<float *>(output_tensors_[0]);
    for (unsigned long long i = 0; i < outer; ++i) {
        const float *src = input + i * ostr;
        for (int d = 0; d < D; ++d) {
            const float *sp = src;
            for (int s = rip->start; s < rip->end; ++s)
                sp += strides[s] * INT_PTR(input_tensors_[1 + (s - rip->start)])[d];
            for (unsigned long long j = 0; j < inner; ++j) {
                *output = *sp;
                ++output;
                ++sp;
            }
        }
    }
    std::vector<int> os;
    for (int i = 0; i < rip->start; ++i)
        os.push_back(input_shapes_[0][i]);
    if (rip->start < input_shapes_[0].size())
        os.push_back(input_shapes_[1][0]);
    for (int i = rip->end; i < input_shapes_[0].size(); ++i)
        os.push_back(input_shapes_[0][i]);
    *output_shapes_ = {os};
    return 0;
}
int cpu_pytorch_indexlayer::reshape(void *param, int param_size,
                                    const std::vector<std::vector<int>>
                                    &input_shapes,
                                    std::vector<std::vector<int>>
                                    &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_pytorch_index_param_t, rip, param,
                                   param_size);
    std::vector<int> os;
    for (int i = 0; i < rip->start; ++i)
        os.push_back(input_shapes[0][i]);
    if (rip->start < input_shapes[0].size())
        os.push_back(input_shapes[1][0]);
    for (int i = rip->end; i < input_shapes[0].size(); ++i)
        os.push_back(input_shapes[0][i]);
    output_shapes = {os};
    return 0;
}
int cpu_pytorch_indexlayer::dtype(const void *param, size_t param_size,
                                  const std::vector<int> &input_dtypes,
                                  std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_PYTORCH_INDEX, cpu_pytorch_index);
}
