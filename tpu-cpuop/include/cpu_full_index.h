#ifndef CPU_FULL_INDEX_H
#define CPU_FULL_INDEX_H

#include "cpu_layer.h"

namespace bmcpu {

class cpu_full_index_layer: public cpu_layer {
public:
    explicit cpu_full_index_layer() {}
    virtual ~cpu_full_index_layer() {}

    int process(void* param, int param_size) override;
    int reshape(void* param, int psize,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) override;
    virtual int dtype(const void* param, size_t param_size,
                      const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override;
};

} /* namespace bmcpu */

#endif // CPU_FULL_INDEX_H
