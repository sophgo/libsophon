#include "cpu_where_squeeze_gather.h"
namespace bmcpu {
int bmcpu::cpu_where_squeeze_gatherlayer::process(void *param, int param_size) {
    const float *where = input_tensors_.back();
    std::vector<int> wshape = input_shapes_.back();
    std::vector<int> indice(wshape[0]);
    int cnt = 0;
    for (int i = 0; i < wshape[0]; ++i) {
        if (where[i] == 0.f)
            continue;
        indice[cnt] = i;
        ++cnt;
    }
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_where_squeeze_gather_t, l_param, param, param_size);
    for (int b = 0; b < input_tensors_.size() - 1; ++b) {
        int axis = l_param->axes[b];
        const float *table = input_tensors_[b];
        float *output = output_tensors_[b];
        std::vector<int> tshape = input_shapes_[b];
        if (axis < 0)
            axis += static_cast<int>(tshape.size());
        int outer = 1;
        int inner = 1;
        for (int i = 0; i < axis; ++i)
            outer *= tshape[i];
        for (int i = axis + 1; i < tshape.size(); ++i)
            inner *= tshape[i];
        for (int i = 0; i < outer; ++i) {
            for (int j = 0; j < cnt; ++j) {
                const float *src = table + indice[j] * inner;
                for (int k = 0; k < inner; ++k) {
                    *output = *src;
                    ++output;
                    ++src;
                }
            }
            table += tshape[axis] * inner;
        }
        for (int i = 0; i < tshape.size(); ++i)
            (*output_shapes_)[b][i] = tshape[i];
        (*output_shapes_)[b][axis] = cnt;
    }
    return 0;
}
int bmcpu::cpu_where_squeeze_gatherlayer::reshape(
    void *param, int param_size,
    const vector<vector<int> > &input_shapes,
    vector<vector<int> > &output_shapes) {
    output_shapes.clear();
    for (int i = 0; i < input_shapes.size() - 1; ++i)
        output_shapes.push_back(input_shapes[i]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_WHERE_SQUEEZE_GATHER, cpu_where_squeeze_gather)
} // namespace bmcpu
