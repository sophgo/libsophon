#include "cpu_reverse_sequence.h"
#include <thread>

namespace bmcpu {

template <typename T>
void compute_reverse_sequence(
    const T* input,
    const int* seq_lens,
    T* output,
    const std::vector<int>& input_shape,
    const int seq_dim,
    const int batch_dim)
{
  int num_elems = 1;
  int seq_elems = 1;
  int batch_elems = 1;
  for (int i = 0; i < input_shape.size(); ++i) {
    num_elems *= input_shape[i];
    if (i > seq_dim) seq_elems *= input_shape[i];
    if (i > batch_dim) batch_elems *= input_shape[i];
  }
 
  int num_thread = 1;
  char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
  if (nt != nullptr) num_thread = atoi(nt);
 
  typedef struct ThreadParam {
    int start;
    int end;
  } ThreadParam_t;
  int opt = num_elems / num_thread;
  int start = 0;
  std::vector<ThreadParam_t> ps(num_thread);
  for (int i = 0; i < num_thread; ++i) {
    ps[i].start = start;
    int len = opt + (i < num_elems - opt * num_thread ? 1 : 0);
    ps[i].end = start + len;
    start = ps[i].end;
  }

  auto func = [&, output] (const ThreadParam_t *tp) {
    for (int i = tp->start; i < tp->end; ++i) {
      int new_idx = i;
      int seq_idx = (i / seq_elems) % input_shape[seq_dim];
      int batch_idx = (i / batch_elems) % input_shape[batch_dim];
      if (seq_idx < seq_lens[batch_idx]) {
        new_idx = i + seq_elems * (seq_lens[batch_idx] - 2 * seq_idx - 1); 
      }
      output[i] = input[new_idx];
    }
  };

  if (num_thread == 1) {
    func(ps.data());
  }
  else {
    std::vector<std::thread> threads;
    for (auto &it : ps) threads.push_back(std::thread(func, &it));
    for (auto &it : threads) it.join();
  }
 
}

int cpu_reverse_sequencelayer::process(void *param, int param_size)
{
  setParam(param, param_size);

  const int* seq_lens = reinterpret_cast<int*>(input_tensors_[1]);
  auto data_shape = input_shapes_[0];
  auto seq_lens_shape = input_shapes_[1];
  int data_dim = data_shape.size();
  CPU_ASSERT_NE(batch_dim_, seq_dim_);
  CPU_ASSERT_LT(seq_dim_, data_dim);
  CPU_ASSERT_LT(batch_dim_, data_dim);
  CPU_ASSERT_EQ(seq_lens_shape[0], data_shape[batch_dim_]);
  for (size_t d = 0; d < seq_lens_shape[0]; ++d) {
    CPU_ASSERT_GE(seq_lens[d], 0);
    CPU_ASSERT_LE(seq_lens[d], data_shape[seq_dim_]);
  }

  const float* data = input_tensors_[0];
  float* data_out = output_tensors_[0];
  compute_reverse_sequence(data, seq_lens, data_out, data_shape,
                           seq_dim_, batch_dim_);
  return 0;
}

void cpu_reverse_sequencelayer::setParam(void *param, int param_size)
{
  layer_param_ = param;
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_reverse_sequence_param_t, reverse_sequence_param,
                                 layer_param_, param_size);
  
  seq_dim_ = reverse_sequence_param->seq_dim;
  batch_dim_ = reverse_sequence_param->batch_dim;
}

REGISTER_CPULAYER_CLASS(CPU_REVERSE_SEQUENCE, cpu_reverse_sequence)

} /* namespace bmcpu */