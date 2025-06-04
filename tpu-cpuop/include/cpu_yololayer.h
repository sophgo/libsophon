#ifndef _CPU_YOLO_LAYER_H
#define _CPU_YOLO_LAYER_H

#include "cpu_layer.h"
#include <cmath>

namespace bmcpu {

class cpu_yololayer : public cpu_layer {
public:
  explicit cpu_yololayer() {}
  virtual ~cpu_yololayer() {}

  /* dowork */
  int process(void *parm, int param_size);
  void setParam(void *param, int param_size);

  virtual string get_layer_name () const {
    return "YOLO";
  }

  int reshape(void* param, int param_size,
              const vector<vector<int>>& input_shapes,
              vector<vector<int>>& output_shapes) {
      output_shapes[0] = input_shapes[0];
      return 0; }
  virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                    vector<int>& output_dtypes) override
  {
    output_dtypes.assign(1, input_dtypes[0]);
    return 0;
  }

protected:
  inline int entry_index(int w, int h, int outputs, int classes,
      int batch, int location, int entry)
  {
    int n = location / (w * h);
    int loc = location % (w * h);
    return batch * outputs + n * w * h * (4 + classes + 1) +
           entry * w * h + loc;
  }

  void activate_array(float *x, const int n);

protected:
  int classes_;
  int num_;
};

} /* namespace bmcpu */
#endif /* _CPU_YOLO_LAYER_H */

