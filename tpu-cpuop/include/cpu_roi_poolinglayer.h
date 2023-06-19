#ifndef _CPU_ROI_POOLING_LAYER_H
#define _CPU_ROI_POOLING_LAYER_H
#include "cpu_layer.h"

namespace bmcpu {

class cpu_roi_poolinglayer : public cpu_layer {
public:
  explicit cpu_roi_poolinglayer() {}
  virtual ~cpu_roi_poolinglayer() {}

  /* dowork */
  int process(void *param, int param_size);
  void setParam(void *param, int param_size);

  virtual string get_layer_name () const {
    return "ROI_POOLING";
  }

  int reshape(void* param, int param_size,
              const vector<vector<int>>& input_shapes,
              vector<vector<int>>& output_shapes) {
    CPU_ASSERT(input_shapes.size() > 1);
    output_shapes[0] = input_shapes[0];
    output_shapes[0][0] = input_shapes[1][0];
    return 0;
  }
  virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
                    vector<int>& output_dtypes) override
  {
    output_dtypes.assign(1, input_dtypes[0]);
    return 0;
  }

protected:
  int pooled_height_;
  int pooled_width_;
  float spatial_scale_;
};

} /* namespace bmcpu */
#endif /* _CPU_ROI_POOLING_LAYER_H */

