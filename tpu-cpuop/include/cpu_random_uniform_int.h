#ifndef CPU_RANDOM_UNIFORM_INT_H
#define CPU_RANDOM_UNIFORM_INT_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_random_uniform_intlayer : public cpu_layer {
public:
    explicit cpu_random_uniform_intlayer() {}
    virtual ~cpu_random_uniform_intlayer() {}
    int process(void *param, int param_size);
    int reshape(void *param, int param_size,
                const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes);
    int dtype(const void *param, size_t param_size,
              const vector<int> &input_dtypes, vector<int> &output_dtypes);
};
} /* namespace bmcpu */
#endif // CPU_RANDOM_UNIFORM_INT_H
