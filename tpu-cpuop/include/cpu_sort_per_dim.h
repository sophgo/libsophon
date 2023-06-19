#ifndef _CPU_SORT_PER_DIM_LAYER_H
#define _CPU_SORT_PER_DIM_LAYER_H
#include "cpu_layer.h"

namespace bmcpu {

class cpu_sort_per_dimlayer : public cpu_layer {
public:
    explicit cpu_sort_per_dimlayer() {}
    virtual ~cpu_sort_per_dimlayer() {}

    int process(void *parm, int param_size);
    void setParam(void *param, int param_size);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) { return 0; }

    virtual string get_layer_name () const {
        return "SORT PER DIM";
    }
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      output_dtypes={CPU_DTYPE_INT32, input_dtypes[0]};
      return 0;
    }

protected:
    int dim;
    bool is_argsort;
    bool stable;
    bool descending;
};

} /* namespace bmcpu */

#endif /* _CPU_SORT_PER_DIM_LAYER_H */

