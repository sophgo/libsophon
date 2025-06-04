//#include <string>
#include <vector>
#include "cpu_embedding.h"
#include "cpu_layer.h"

namespace bmcpu {


int cpu_embeddinglayer::process(void* param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_embedding_param_t, params, param, param_size);

    padding_idx_ = params->padding_idx;
    const int   *input = reinterpret_cast<int *>(input_tensors_[0]);
    const float *weight = input_tensors_[1];
    std::vector<int> shape = input_shapes_[0];
    std::vector<int> weight_shape = input_shapes_[1];
    CPU_ASSERT(weight_shape.size() == 2);

    std::vector<int> index_shape = input_shapes_[1];
    int input_dim = shape.size();
    int input_size = 1;
    for (int i = 0; i < input_dim; ++i) {
        input_size *= shape[i];
    }

    int embedding_dim = weight_shape[1];
    float * output = output_tensors_[0];
    for (int i=0; i<input_size; ++i) {
        const float *embedding_index = weight + input[i]*embedding_dim;
        for (int j=0; j<embedding_dim; ++j) {
            *output = *embedding_index;
            ++output;
            ++embedding_index;
        }
    }

    for (int i=0; i<input_dim; ++i) {
        (*output_shapes_)[0][i] = shape[i];
    }
    (*output_shapes_)[0][input_dim] = embedding_dim;
    return 0;
}

void cpu_embeddinglayer::setParam(void* param, int param_size)
{
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_embedding_param_t, params, param, param_size);

    padding_idx_ = params->padding_idx;
    if (padding_idx_ != nullptr) {
        if (*padding_idx_ > 0 ) {
            CPU_ASSERT(*padding_idx_ < input_shapes_[1][0]);
        } else if (*padding_idx_ < 0 ) {
            CPU_ASSERT(*padding_idx_ >= -input_shapes_[1][0]);
            // TODO
            // pytorch 1.13, torch/csrc/api/include/torch/nn/fuctional/embedding.h:34
            // padding_idx = weight.size(0) + *padding_idx;
            *padding_idx_ = *padding_idx_ + input_shapes_[1][0];
        }
    } else {
        // TODO embedding.h:37
//        *padding_idx_ = -1;
    }
}

int cpu_embeddinglayer::reshape(
    void* param, int param_size,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes)
{
  int input_dim = input_shapes[0].size();
  output_shapes[0].clear();
  for (int i=0; i<input_dim; ++i) {
      output_shapes[0].push_back(input_shapes[0][i]);
  }
  output_shapes[0].push_back(input_shapes[1][1]);
  return 0;
}

REGISTER_CPULAYER_CLASS(CPU_EMBEDDING, cpu_embedding);

} /* namespace bmcpu*/
