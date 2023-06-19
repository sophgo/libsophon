#ifndef CPU_TENSOR_SCATTER_OP_H
#define CPU_TENSOR_SCATTER_OP_H

#include "cpu_layer.h"
#include "cpu_utils.h"

namespace bmcpu {

class cpu_tensor_scatter_oplayer: public cpu_layer {
public:
    explicit cpu_tensor_scatter_oplayer() {}
    virtual ~cpu_tensor_scatter_oplayer() {}

    int process(void *param, int param_size) override;

    string get_layer_name () const {
        return "CPU_TENSOR_SCATTER_UPDATE";
    }

    // cpu_layer interface
    int reshape(void *param, int param_size,
                const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes) override {
        (void)param;
        (void)param_size;
        output_shapes.assign(1, input_shapes[0]);
        return 0;
    }

    int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
                      vector<int> &output_dtypes) override {
        (void)param;
        (void)param_size;
        output_dtypes.assign(1, input_dtypes[0]);
        return 0;
    }
};

}
#endif // CPU_TENSOR_SCATTER_OP_H
