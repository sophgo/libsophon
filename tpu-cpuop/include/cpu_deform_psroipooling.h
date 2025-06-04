#ifndef CPU_DEFORM_PSROIPOOLING_LAYER_H
#define CPU_DEFORM_PSROIPOOLING_LAYER_H

#include "cpu_layer.h"

namespace bmcpu {

class cpu_deform_psroipoolinglayer : public cpu_layer {
 public:
  explicit cpu_deform_psroipoolinglayer() {}
  virtual ~cpu_deform_psroipoolinglayer() {}

  virtual int process(void* param, int param_size);

  virtual string get_layer_name() const {
    return "DEFORM PSROIPOOLING";
  }

  int reshape(void* param, int param_size,
              const vector<vector<int>> &input_shapes,
              vector<vector<int>> &output_shapes) {
    
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_deform_psroipooling_param_t,
                                   p, param, param_size);
    const vector<int> data_shape = input_shapes[0];
    const vector<int> rois_shape = input_shapes[1];

    output_shapes[0] = data_shape;
    output_shapes[0][0] = rois_shape[0];
    output_shapes[0][1] = p->output_dim;
    output_shapes[0][2] = p->pooled_size;
    output_shapes[0][3] = p->pooled_size;

//    vector<int> top_count_shape({rois_shape[0], data_shape[1],
//                                 p->pooled_size, p->pooled_size});
//    output_shapes[1] = top_count_shape;

    return 0;
  }

  virtual int dtype(const void* param, size_t param_size,
                    const vector<int> &input_dtypes,
                    vector<int> &output_dtypes) override {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
  }

};
} // namespace bmcpu

#endif // CPU_DEFORM_GATHER_LAYER_H