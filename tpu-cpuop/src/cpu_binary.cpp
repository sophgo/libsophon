#include "cpu_binary.h"

namespace bmcpu {

static int increase_indice(int* indice, const int* limit, int len){
    int carry_value = 1;
    for(int i=len-1; i>=0; i--){
        indice[i] += carry_value;
        if(indice[i]>=limit[i]){
            carry_value = 1;
            indice[i] = 0;
        } else {
            carry_value = 0;
            break;
        }
    }
    return carry_value == 0;
}

template<typename T>
T binary_core(T a, T b, BINARY_OP_CODE_T op){
    switch(op){
      case OP_MOD:
        return static_cast<int>(a) % static_cast<int>(b);
      case OP_DIV:
        return static_cast<int>(a) / static_cast<int>(b);
    default:
        CPU_ASSERT(0);
        exit(-1);
    }
    return 0;
}

template<typename T>
int broadcast_data(
        const T* in_data0, const int* in_shape0, const int in_dim0,
        const T* in_data1, const int* in_shape1, const int in_dim1,
        BINARY_OP_CODE_T binary_op, T* dst, int* out_shape)
{
    int out_dim = std::max(in_dim0, in_dim1);
    int* inner_shape0 = new int[out_dim];
    int* inner_shape1 = new int[out_dim];
    int* inner_stride0 = new int[out_dim];
    int* inner_stride1 = new int[out_dim];
    int* out_stride = new int[out_dim];
    int* out_indice = new int[out_dim];
    for(int i=out_dim-1; i>=0; i--){
        inner_shape0[i] = (i<out_dim-in_dim0)?1:in_shape0[i-(out_dim-in_dim0)];
        inner_shape1[i] = (i<out_dim-in_dim1)?1:in_shape1[i-(out_dim-in_dim1)];
        out_shape[i] = std::max(inner_shape0[i], inner_shape1[i]);
        inner_stride0[i] = (i==out_dim-1)?1: inner_stride0[i+1]*inner_shape0[i+1];
        inner_stride1[i] = (i==out_dim-1)?1: inner_stride1[i+1]*inner_shape1[i+1];
        out_stride[i] = (i==out_dim-1)?1: out_stride[i+1]*out_shape[i+1];
        out_indice[i] = 0;
    }
    if(dst){
        do {
            int in_offset0 = 0;
            int in_offset1 = 0;
            int out_offset = 0;
            for(int i=0; i<out_dim; i++){
                in_offset0 += inner_shape0[i]==1?0:out_indice[i]*inner_stride0[i];
                in_offset1 += inner_shape1[i]==1?0:out_indice[i]*inner_stride1[i];
                out_offset += out_indice[i]*out_stride[i];
            }
            dst[out_offset]=binary_core(in_data0[in_offset0], in_data1[in_offset1], binary_op);
        } while(increase_indice(out_indice, out_shape, out_dim));
    }
    delete [] inner_shape0;
    delete [] inner_shape1;
    delete [] inner_stride0;
    delete [] inner_stride1;
    delete [] out_stride;
    delete [] out_indice;
    return out_dim;
}

int cpu_binarylayer::process(void *param, int param_size)
{
    auto binary_param = (cpu_binary_param_t*)param;
    CPU_ASSERT_EQ(param_size, sizeof(cpu_binary_param_t));

    vector<int> out_shape(std::max(input_shapes_[0].size(), input_shapes_[1].size()));
    if (binary_param->dtype == CPU_DTYPE_INT32){
      broadcast_data<int>(
            (const int*)input_tensors_[0], input_shapes_[0].data(), input_shapes_[0].size(),
            (const int*)input_tensors_[1], input_shapes_[1].data(), input_shapes_[1].size(),
            binary_param->op, (int*)output_tensors_[0], out_shape.data());
    } else if(binary_param->dtype == CPU_DTYPE_FP32){
      broadcast_data<float>(
            input_tensors_[0], input_shapes_[0].data(), input_shapes_[0].size(),
            input_tensors_[1], input_shapes_[1].data(), input_shapes_[1].size(),
            binary_param->op, output_tensors_[0], out_shape.data());
    }
    output_shapes_->assign(1, out_shape);
    return 0;
}

int cpu_binarylayer::reshape(void *param, int param_size, const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes)
{
    (void)param;
    (void)param_size;
    vector<int> out_shape(std::max(input_shapes[0].size(), input_shapes[1].size()));
    broadcast_data<int>(
          nullptr, input_shapes[0].data(), input_shapes[0].size(),
          nullptr, input_shapes[1].data(), input_shapes[1].size(),
          OP_MOD, nullptr, out_shape.data());
    output_shapes.assign(1, out_shape);
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_BINARY, cpu_binary);
}
