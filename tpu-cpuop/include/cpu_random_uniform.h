#ifndef CPU_RANDOM_UNIFORM_H
#define CPU_RANDOM_UNIFORM_H
#include "cpu_layer.h"
#include <mutex>
#include <random>

namespace bmcpu {
class cpu_random_uniformlayer : public cpu_layer {
private:
    std::once_flag seeded_;
    std::shared_ptr<std::default_random_engine> gen_;

public:
    explicit cpu_random_uniformlayer() {}
    virtual ~cpu_random_uniformlayer() {}
    int process(void *param, int param_size);
    int reshape(void *param, int param_size,
                const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes);
    int dtype(const void *param, size_t param_size,
              const vector<int> &input_dtypes, vector<int> &output_dtypes);
};
} /* namespace bmcpu */
#endif // CPU_RANDOM_UNIFORM_H
