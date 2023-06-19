#ifndef CPU_MASKED_SELECT_H
#define CPU_MASKED_SELECT_H

#include "cpu_layer.h"

/*
 *  input tensor: float input[shape1, shape2, ...], float mask[shape1, shape2, ...]
 *  input param: bcast_from_begin, e.g. [2,1,3][2,3], if ture => [2,3,3], else => [2,2,3]
 *  output tensor: [masked_count]
 */

namespace bmcpu {

class cpu_masked_selectlayer : public cpu_layer {
 public:
  explicit cpu_masked_selectlayer() {}
  virtual ~cpu_masked_selectlayer() {}

  int process(void* param, int param_size);
  void setParam(void* param, int param_size);
  int reshape(void* param, int param_size,
              const vector<vector<int>>& input_shapes,
              vector<vector<int>>& output_shapes);
  virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                    vector<int>& output_dtypes) override
  {
    output_dtypes.assign(1, input_dtypes[0]);
    return 0;
  }

 private:
  bool bcast_from_begin;
};

} /* namespace bmcpu */

#endif  // CPU_MASKED_SELECT_H
