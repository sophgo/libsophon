#ifndef CPU_GATHERND_TF_H
#define CPU_GATHERND_TF_H

//#include "bmcpu_common.h"
#include "cpu_layer.h"

/*  gather_nd for tensorflow 1.13.1
 *
 */

namespace bmcpu {

class cpu_gathernd_tflayer : public cpu_layer {
public:
    explicit cpu_gathernd_tflayer() {}
    virtual ~cpu_gathernd_tflayer() {}

    int process(void* param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);
    virtual int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
                      vector<int> &output_dtypes) override {
        output_dtypes.assign(1, input_dtypes[0]);
        return 0;
    }
};

} /* namespace bmcpu */


#endif // CPU_GATHERND_TF_H
