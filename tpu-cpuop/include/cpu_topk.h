#ifndef CPU_TOPK_LAYER_H
#define CPU_TOPK_LAYER_H

#include "cpu_layer.h"

namespace bmcpu {

class cpu_topklayer : public cpu_layer {
public:
    explicit cpu_topklayer() {}
    explicit cpu_topklayer(bool descending) : descending_(descending) {}
    virtual ~cpu_topklayer() {}

    virtual int process(void* param, int param_size);
    void setParam(void *param, int param_size);

    virtual string get_layer_name () const {
        return "TOPK";
    }

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      output_dtypes = {input_dtypes[0], CPU_DTYPE_INT32};
      return 0;
    }

protected:
    bool descending_ = true;
    int  k_;
    int  axis_;
    bool sorted_;
};
} /* namespace bmcpu */


#endif // CPU_TOPK_H
