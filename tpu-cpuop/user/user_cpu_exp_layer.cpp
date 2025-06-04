#include "user_cpu_exp_layer.h"

namespace usercpu {

int cpu_exp_layer::process(void *param)
{
  setParam(param);
    /* Code From caffe */
  const float* bottom_data = (float*)input_tensors_[0];
  float* top_data = (float*)output_tensors_[0];
  const int count = input_shapes_[0][0] * input_shapes_[0][1] * input_shapes_[0][2] * input_shapes_[0][3];
  //Dtype negative_slope = this->layer_param_.exp_param().negative_slope();
  //float negative_slope = 0.0f;
  for (int i = 0; i < count; ++i) {
    top_data[i] = exp(bottom_data[i] * inner_scale_) * outer_scale_;
  }

  /* output reshape */
  (*output_shapes_)[0].clear();
  for (int i = 0; i < input_shapes_[0].size(); i++) {
      (*output_shapes_)[0].push_back(input_shapes_[0][i]);
  }

  printf("user define cpu exp layer, process success!\n");
  return 0;
}

void cpu_exp_layer::setParam(void *param)
{
    layer_param_ = param;
    user_cpu_exp_param_t *exp_param = (user_cpu_exp_param_t*)layer_param_;
    inner_scale_ = exp_param->inner_scale_;
    outer_scale_ = exp_param->outer_scale_;
}

int cpu_exp_layer::reshape(
          void* param,
          const vector<vector<int>>& input_shapes,
          vector<vector<int>>& output_shapes)
{
    USER_ASSERT(input_shapes.size() == 1);

    output_shapes[0].clear();
    for (auto& dim : input_shapes[0]) {
        output_shapes[0].push_back(dim);
    }
    cout << " cpu exp reshape "<< endl;
    return 0;
}

int cpu_exp_layer::dtype(
          void* param,
          const vector<int>& input_dtypes,
          vector<int>& output_dtypes)
{
    USER_ASSERT(input_dtypes.size() == 1);
    output_dtypes = {input_dtypes[0]};
    cout << " cpu exp dtype "<< endl;
    return 0;
}


/* must register user layer
 * in macro cpu_test##_layer  == class cpu_test_layer 
 * */

REGISTER_USER_CPULAYER_CLASS(USER_EXP, cpu_exp)

}
