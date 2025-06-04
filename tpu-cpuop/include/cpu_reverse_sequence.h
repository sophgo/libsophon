#ifndef _CPU_REVERSE_SEQUENCE_LAYER_H
#define _CPU_REVERSE_SEQUENCE_LAYER_H

#include "cpu_layer.h"
#include <algorithm>
#include <vector>

namespace bmcpu {

class cpu_reverse_sequencelayer : public cpu_layer {
public:
  explicit cpu_reverse_sequencelayer() {}
  virtual ~cpu_reverse_sequencelayer() {}

  int process(void *param, int param_size);
  void setParam(void *param, int param_size);

  virtual string get_layer_name() const {
    return "REVERSE_SEQUENCE";
  }

  int reshape(
      void* param, int param_size,
      const vector<vector<int>>& input_shapes,
      vector<vector<int>>& output_shapes) {
    // TODO
    output_shapes[0] = input_shapes[0];
    return 0;
  }

  int dtype(const void* param, size_t param_size,
            const vector<int>& input_dtypes,
            vector<int>& output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
  }

private:
  int seq_dim_;
  int batch_dim_;
};

} /* namespace bmcpu */

#endif /* _CPU_REVERSE_SEQUENCE */