#ifndef CPU_BOX_NMS_H
#define CPU_BOX_NMS_H
#include "cpu_layer.h"

namespace bmcpu {

class cpu_boxnmslayer: public cpu_layer {
public:
    explicit cpu_boxnmslayer() {}
    virtual ~cpu_boxnmslayer() {}

    int process(void *param, int param_size);

    virtual string get_layer_name () const {
        return "BOX_NMS";
    }

    // cpu_layer interface
    int reshape(void *param, int param_size,
                const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes);

    int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes) {
        output_dtypes = {CPU_DTYPE_FP32, CPU_DTYPE_FP32};
        return 0;
    }
};

} /* namespace bmcpu */

#endif // CPU_BOX_NMS_H

