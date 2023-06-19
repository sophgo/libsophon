#ifndef CPU_GATHERND_PYTORCH_H
#define CPU_GATHERND_PYTORCH_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_gather_ptlayer : public cpu_layer {
public:
    explicit cpu_gather_ptlayer() {}
    virtual ~cpu_gather_ptlayer() {}
    int process(void* param, int param_size);
    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) override {
        output_shapes = {input_shapes[1]};
        return 0;
    }
    virtual int dtype(const void *param, size_t param_size,
                      const vector<int> &input_dtypes,
                      vector<int> &output_dtypes) override {
        output_dtypes = {input_dtypes[0]};
        return 0;
    }
};
} /* namespace bmcpu */
#endif // CPU_GATHERND_PYTORCH_H
