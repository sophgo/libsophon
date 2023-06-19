#ifndef CPU_TOPK_MX_H
#define CPU_TOPK_MX_H

#include "cpu_layer.h"

namespace bmcpu {

template<typename DType>
class cpu_topk_mx_template: public cpu_layer {
public:
    explicit cpu_topk_mx_template() {}
    virtual ~cpu_topk_mx_template() {}

    int process(void* param, int param_size);
    virtual string get_layer_name () const {
        return "TOPK_MX";
    }

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);
    int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes);
};

} /* namespace bmcpu */


#endif // CPU_TOPK_MX_H
