#include "cpu_tensor_scatter_op.h"
#include "bmcpu_utils.hpp"

namespace bmcpu {

template<typename T>
void update_core(void* data_, const void* updates_, int len, CPU_SCATTER_OP_T op){
    auto data = reinterpret_cast<T*>(data_);
    auto updates = reinterpret_cast<const T*>(updates_);
    if(op == CPU_SCATTER_ASSIGN){
        memcpy(data, updates, len*sizeof(T));
    } else if(op == CPU_SCATTER_ADD) {
        for(int i=0; i<len; i++){
            data[i] += updates[i];
        }
    } else if(op == CPU_SCATTER_SUB) {
        for(int i=0; i<len; i++){
            data[i] -= updates[i];
        }
    } else if(op == CPU_SCATTER_SUB_REVERSE) {
        for(int i=0; i<len; i++){
            data[i] = updates[i] - data[i];
        }
    } else if(op == CPU_SCATTER_MUL) {
        for(int i=0; i<len; i++){
            data[i] *= updates[i];
        }
    } else if(op == CPU_SCATTER_MAX) {
        for(int i=0; i<len; i++){
            data[i] += std::max(data[i], updates[i]);
        }
    } else if(op == CPU_SCATTER_MIN) {
        for(int i=0; i<len; i++){
            data[i] += std::min(data[i], updates[i]);
        }
    } else {
        CPU_ASSERT(0);
    }
}


void update_core_ex(void* data, const void* updates, int len, CPU_SCATTER_OP_T op, CPU_DATA_TYPE_T dtype){
    if(dtype == CPU_DTYPE_FP32){
        update_core<float>(data, updates, len, op);
    } else if(dtype == CPU_DTYPE_INT32){
        update_core<int>(data, updates, len, op);
    } else {
        CPU_ASSERT(0);
    }
}


int cpu_tensor_scatter_oplayer::process(void *param, int param_size)
{
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_tensor_scatter_op_param_t, detail_param, param, param_size);

    /*
      assert tf.rank(indices) >= 2
      index_depth = indices.shape[-1]
      batch_shape = indices.shape[:-1]
      assert index_depth <= tf.rank(tensor)
      outer_shape = tensor.shape[:index_depth]
      inner_shape = tensor.shape[index_depth:]
      assert updates.shape == batch_shape + inner_shape
    */

    CPU_ASSERT_EQ(input_tensors_.size(), 3);
    auto &tensor_shape = input_shapes_[0];
    auto &index_shape = input_shapes_[1];
    auto &data_shape = input_shapes_[2];
    auto index_depth = index_shape.back();

    std::vector<int> outer_shape(tensor_shape.begin(), tensor_shape.begin() + index_depth);

    auto batch = 1;
    auto length = 1;
    for(size_t i=0; i<index_shape.size()-1; i++){
        CPU_ASSERT_EQ(index_shape[i], data_shape[i]);
        batch *= index_shape[i];
    }
    for(size_t i=index_depth; i<tensor_shape.size(); i++){
        CPU_ASSERT_EQ(tensor_shape[i], data_shape[i]);
        length *= tensor_shape[i];
    }

    /**
     * validity check
     *  1. index rank - 1 less equal than output rank
     *  2. index rank large equal than 2
     *  3. length == 1 as long as index_shape[-1] == output rank
     *              otherwise index_shape[-1] < output rank
     *  4. output rank + index rank - index_shape[-1] - 1 == update rank
     */
    CPU_ASSERT_LE(index_depth, tensor_shape.size());
    CPU_ASSERT_GE(index_shape.size(),2);
    CPU_ASSERT(length == 1 && index_depth == tensor_shape.size());
    CPU_ASSERT_EQ(tensor_shape.size() + index_shape.size() - index_depth - 1, data_shape.size());

    size_t elem_num = 1;
    for(auto s: tensor_shape){
        elem_num *= s;
    }

    auto tensor_base = (char*)input_tensors_[0];
    auto index_base = (int*)input_tensors_[1];
    auto update_base = (char*)input_tensors_[2];
    auto type_len = cpu_get_type_len(detail_param->input_dtype);

    std::vector<int> outer_stride(outer_shape.size(), length*type_len);
    for(int i= outer_stride.size()-2; i>=0; i--) {
        outer_stride[i] = outer_stride[i+1] * outer_shape[i+1];
    }
    auto output_base = (char*)output_tensors_[0];
    if(output_base != tensor_base) {
        memcpy(output_base, tensor_base, elem_num * type_len);
    }


    // utilize openmp is recommended
    // May cause unstable in multi-threads since indexes might overlap
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    int nthreads = 1;
    if (nt != nullptr && *nt != '0') nthreads = atoi(nt);
    int worker_start = 0;
    std::vector<std::thread> threads;
    int thread_stride = batch / nthreads + 1;

    for (int t = 0; t < nthreads; t++) {
        threads.emplace_back([batch, &outer_stride, index_base, output_base, update_base, length, detail_param]
            (int batch_st, int batch_ed){
            for(int idx=batch_st; idx < batch_ed; idx++) {
                int index_depth = outer_stride.size();
                auto index_data = index_base + idx * index_depth;
                size_t tensor_offset = 0;
                for(size_t i = 0; i<outer_stride.size(); i++) {
                    tensor_offset += index_data[i] * outer_stride[i];
                }
                auto tensor_data = output_base + tensor_offset;
                auto update_data = update_base +  idx * outer_stride.back();
                update_core_ex(tensor_data, update_data, length, detail_param->scatter_op, detail_param->input_dtype);
            }
        },worker_start, std::min(worker_start + thread_stride, batch));
        worker_start += thread_stride;
    }
    for(auto& t: threads){
        t.join();
    }

    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_TENSOR_SCATTER_OP, cpu_tensor_scatter_op)

}
