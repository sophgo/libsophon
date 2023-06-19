#ifndef CPU_REPEAT_INTERLEAVE_H
#define CPU_REPEAT_INTERLEAVE_H

#include "cpu_layer.h"

/*
 *  there are two tensor，tensor[0] is input, tensor[1] is repeat.
 */
namespace bmcpu {
class cpu_repeat_interleavelayer : public cpu_layer {
public:
    explicit cpu_repeat_interleavelayer() {}
    virtual ~cpu_repeat_interleavelayer() {}

    int process(void* param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);

    int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes);
};

}
#endif /* CPU_REPEAT_INTERLEAVE_H */
