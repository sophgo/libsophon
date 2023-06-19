#ifndef CPU_DEFORM_GATHER_LAYER_H
#define CPU_DEFORM_GATHER_LAYER_H

#include "cpu_layer.h"

namespace bmcpu {

class cpu_deform_gatherlayer : public cpu_layer {
 public:
  explicit cpu_deform_gatherlayer() {}
  virtual ~cpu_deform_gatherlayer() {}

  virtual int process(void* param, int param_size);

  virtual string get_layer_name() const {
    return "DEFORM GATHER";
  }

  int reshape(void* param, int param_size,
              const vector<vector<int>> &input_shapes,
              vector<vector<int>> &output_shapes) {

    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_deform_gather_param_t,
                                   p, param, param_size);
    vector<int> data_shape = input_shapes[0];
    const int H = data_shape[2];
    const int W = data_shape[3];
    const int conved_H = ((H - (p->dilation_h * (p->kh - 1) + 1) +
        p->pad_t + p->pad_b) / p->stride_h + 1);
    const int conved_W = ((W - (p->dilation_w * (p->kw - 1) + 1) +
        p->pad_l + p->pad_r) / p->stride_w + 1);

    output_shapes[0][0] = data_shape[0];
    output_shapes[0][1] = data_shape[1] * p->kh * p->kw;
    output_shapes[0][2] = conved_H;
    output_shapes[0][3] = conved_W;

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