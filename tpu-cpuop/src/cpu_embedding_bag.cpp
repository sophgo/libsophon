#include "cpu_embedding_bag.h"
#include <vector>
#include "cpu_layer.h"
namespace bmcpu {

#if 1  // optimize version
int cpu_embedding_baglayer::process(void* param, int param_size)
{
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_embedding_bag_param_t, params, param, param_size);

  num_embeddings_ = params->num_embeddings;
  embedding_dim_ = params->embedding_dim;
  mode_ = params->mode;

  float* weight = reinterpret_cast<float*>(input_tensors_[0]);
  const int* indices = reinterpret_cast<int*>(input_tensors_[1]);
  const int* offsets = reinterpret_cast<int*>(input_tensors_[2]);
  std::vector<int> indices_shape = input_shapes_[1];
  std::vector<int> offsets_shape = input_shapes_[2];
  CPU_ASSERT(indices_shape.size() == 1 && offsets_shape.size() == 1);

  float* output = output_tensors_[0];
  if ((mode_ == EMB_MAX)) {
    for (int i = 0; i < offsets_shape[0]; i++) {
      int end = (i == (offsets_shape[0] - 1)) ? indices_shape[0] : offsets[i + 1];
      for (int j = offsets[i]; j < end; j++) {
        if (j == offsets[i]) {
          if (indices[j] < num_embeddings_) {
            memcpy(output + i * embedding_dim_, weight + indices[j] * embedding_dim_,
                   embedding_dim_ * sizeof(float));
          } else {
            memset(output + i * embedding_dim_, 0, embedding_dim_ * sizeof(float));
          }
          continue;
        }
        for (int k = 0; k < embedding_dim_; k++) {
          float weight_val = 0.0;
          if (indices[j] < num_embeddings_) {
            weight_val = weight[indices[j] * embedding_dim_ + k];
          }
          output[i * embedding_dim_ + k] = std::max(output[i * embedding_dim_ + k], weight_val);
        }
      }
    }
  } else {
    for (int i = 0; i < offsets_shape[0]; i++) {
      int end = (i == (offsets_shape[0] - 1)) ? indices_shape[0] : offsets[i + 1];
      for (int j = offsets[i]; j < end; j++) {
        if (j == offsets[i]) {
          if (indices[j] < num_embeddings_) {
            memcpy(output + i * embedding_dim_, weight + indices[j] * embedding_dim_,
                   embedding_dim_ * sizeof(float));
          } else {
            memset(output + i * embedding_dim_, 0, embedding_dim_ * sizeof(float));
          }
          continue;
        }

        int indict_tmp = indices[j];
        if (indict_tmp < num_embeddings_) {
          const float* weight_ptr = &weight[indict_tmp * embedding_dim_];
          float* output_ptr = &output[i * embedding_dim_];
          int remainder = embedding_dim_ & 0x7;
          for (int k = 0; k < (embedding_dim_ >> 3); k += 1, output_ptr += 8, weight_ptr += 8) {
            *output_ptr += *weight_ptr;
            *(output_ptr+1) += *(weight_ptr+1);
            *(output_ptr+2) += *(weight_ptr+2);
            *(output_ptr+3) += *(weight_ptr+3);
            *(output_ptr+4) += *(weight_ptr+4);
            *(output_ptr+5) += *(weight_ptr+5);
            *(output_ptr+6) += *(weight_ptr+6);
            *(output_ptr+7) += *(weight_ptr+7);
          }
          for (int k = 0; k < remainder; k++) {
            *output_ptr += *weight_ptr++;
            output_ptr++;
          }
        }

        if (mode_ == EMB_MEAN && j == end - 1) {
          for (int k = 0; k < embedding_dim_; k++) {
            output[i * embedding_dim_ + k] /= (end - offsets[i]);
          }
        }
      }
    }
  }
  return 0;
}
#else  // original version
int cpu_embedding_baglayer::process(void* param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_embedding_bag_param_t, params, param, param_size);

    num_embeddings_ = params->num_embeddings;
    embedding_dim_ = params->embedding_dim;
    mode_ = params->mode;

    float* weight = reinterpret_cast<float *>(input_tensors_[0]);
    const int *indices = reinterpret_cast<int *>(input_tensors_[1]);
    const int *offsets = reinterpret_cast<int *>(input_tensors_[2]);
    std::vector<int> indices_shape = input_shapes_[1];
    std::vector<int> offsets_shape = input_shapes_[2];
    CPU_ASSERT(indices_shape.size() == 1 && offsets_shape.size() == 1);

    float * output = output_tensors_[0];
    for (int i = 0; i < offsets_shape[0]; i++) {
        int end = (i == (offsets_shape[0] - 1)) ? indices_shape[0] : offsets[i + 1];
        for (int j = offsets[i]; j < end; j++) {
            if (j == offsets[i]) {
                if(indices[j]<num_embeddings_){
                    memcpy(output + i * embedding_dim_, weight + indices[j] * embedding_dim_, embedding_dim_ * sizeof(float));
                } else {
                    memset(output + i * embedding_dim_, 0, embedding_dim_ * sizeof(float));
                }
                continue;
            }
            for (int k = 0; k < embedding_dim_; k++) {
                float weight_val = 0.0;
                if(indices[j]<num_embeddings_){
                    weight_val = weight[indices[j] * embedding_dim_ + k];
                }
                output[i * embedding_dim_ + k] = (mode_ == EMB_MAX) ?
                    std::max(output[i * embedding_dim_ + k], weight_val) :
                    output[i * embedding_dim_ + k] + weight_val;
            }
            if (mode_ == EMB_MEAN && j == end - 1) {
                for (int k = 0; k < embedding_dim_; k++) {
                    output[i * embedding_dim_ + k] /= (end - offsets[i]);
                }
            }
        }
    }
    return 0;
}
#endif

int cpu_embedding_baglayer::reshape(void* param, int param_size,
                                    const vector<vector<int>>& input_shapes,
                                    vector<vector<int>>& output_shapes)
{
  output_shapes[0].clear();
  output_shapes[0].push_back(input_shapes[2][0]);
  output_shapes[0].push_back(reinterpret_cast<cpu_embedding_bag_param_t*>(param)->embedding_dim);
  return 0;
}

REGISTER_CPULAYER_CLASS(CPU_EMBEDDING_BAG, cpu_embedding_bag);

} /* namespace bmcpu*/
