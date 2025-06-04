#ifndef CPU_WHERE_SQUEEZE_GATHER_H
#define CPU_WHERE_SQUEEZE_GATHER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_where_squeeze_gatherlayer : public cpu_layer {
public:
    explicit cpu_where_squeeze_gatherlayer() {}
    virtual ~cpu_where_squeeze_gatherlayer() {}
    int process(void* param, int param_size);
    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                      vector<int>& output_dtypes) override
    {
      output_dtypes = input_dtypes;
      output_dtypes.pop_back();
      return 0;
    }
};
} // namespace bmcpu
#endif // CPU_WHERE_SQUEEZE_GATHER_H
