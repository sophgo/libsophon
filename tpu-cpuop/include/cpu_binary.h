#ifndef CPU_BINARY_H
#define CPU_BINARY_H

#include "cpu_layer.h"

namespace bmcpu {

class cpu_binarylayer : public cpu_layer {
public:
    explicit cpu_binarylayer() {}
    virtual ~cpu_binarylayer() {}

    int process(void* param, int param_size) override;

    virtual string get_layer_name () const {
        return "binary";
    }

    virtual int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) override;
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      (void)param;
      output_dtypes.assign(1, input_dtypes[0]);
      return 0;
    }
};

} /* namespace bmcpu */


#endif // CPU_BINARY_H
