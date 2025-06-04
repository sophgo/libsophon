#ifndef CPU_UNARY_LAYER_H
#define CPU_UNARY_LAYER_H

#include "cpu_layer.h"

namespace bmcpu {

class cpu_unarylayer : public cpu_layer {
public:
    explicit cpu_unarylayer() {}
    virtual ~cpu_unarylayer() {}

    int process(void* param, int param_size);

    virtual string get_layer_name () const {
        return "UNARY";
    }

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) {
        output_shapes[0] = input_shapes[0];
        return 0;
    }
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      output_dtypes.assign(1, input_dtypes[0]);
      return 0;
    }
};

} /* namespace bmcpu */


#endif // CPU_UNARY_LAYER_H
