#include <chrono>
#include "cpu_random_uniform.h"

namespace bmcpu {
int cpu_random_uniformlayer::process(void *param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_random_uniform_param_t, p, param, param_size);
    const int *out_shape = reinterpret_cast<int *>(input_tensors_[0]);
    std::call_once(
        seeded_,
        [this, p]() {
            if (p->seed == 0)
                p->seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            gen_ = std::make_shared<std::default_random_engine>(p->seed);
        });
    std::uniform_real_distribution<float> distribution(p->lower, p->upper);
    #ifdef __linux__
    unsigned long num = 1;
    #else
    unsigned long long num = 1;
    #endif
    for (int i = 0; i < p->dim; ++i)
        num *= out_shape[i];
    float *iter = output_tensors_[0];
    #ifdef __linux__
    for (unsigned long i = 0; i < num; ++i) {
    #else
    for (unsigned long long i = 0; i < num; ++i) {
    #endif
        *iter = distribution(*gen_);
        ++iter;
    }
    (*output_shapes_)[0].clear();
    for (int i = 0; i < p->dim; ++i)
        (*output_shapes_)[0].push_back(out_shape[i]);
    return 0;
}
int cpu_random_uniformlayer::reshape(void *param, int param_size,
                                     const vector<vector<int>> &input_shapes,
                                     vector<vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_random_uniform_param_t, p, param, param_size);
    output_shapes[0].resize(static_cast<size_t>(p->dim));
    for (int i = 0; i < p->dim; ++i)
        output_shapes[0][i] = p->shape[i];
    return 0;
}
int cpu_random_uniformlayer::dtype(const void *param, size_t param_size,
                                   const vector<int> &input_dtypes,
                                   vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(0);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_RANDOM_UNIFORM, cpu_random_uniform);
} // namespace bmcpu
