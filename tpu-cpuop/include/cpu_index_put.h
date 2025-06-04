#ifndef CPU_INDEX_PUT_H
#define CPU_INDEX_PUT_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_index_putlayer : public cpu_layer {
public:
    explicit cpu_index_putlayer() {}
    virtual ~cpu_index_putlayer() {}
    int process(void *param, int param_size);
    int reshape(void *param, int param_size,
                const vector<vector<int>> &input_shapes,
                vector<vector<int>> &output_shapes);
    int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes);
};
} // namespace bmcpu
#endif // CPU_INDEX_PUT_H
