#include "cpu_gathernd.h"
#include <algorithm>
#include <fstream>
using namespace std;

namespace bmcpu {
int cpu_gatherndlayer::process(void *raw_param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_gathernd_t, gathernd_param, raw_param, param_size);
    bool is_float = gathernd_param == nullptr || !gathernd_param->indice_is_int;
    auto data_shape = input_shapes_[0];      //data shape
    auto indice_shape = input_shapes_[1];    //indice shape
    int data_dim = data_shape.size();
    int indice_dim = indice_shape.size();   //at present only support dim = 2
    CPU_ASSERT(indice_dim == 2);
    const float* data_ptr = input_tensors_[0];
    const int* indice = (int*)input_tensors_[1];
    if(is_float){
        int indice_len = 1;
        for(int i=0; i<indice_dim; i++){
            indice_len *= indice_shape[i];
        }
        auto int_buffer = new int[indice_len];
        for(int i=0; i<indice_len; i++){
            int_buffer[i] = (int)(input_tensors_[1][i]);
        }
        indice = int_buffer;
    }

    int dim_out = data_dim - indice_shape[0] + 1;
    
    auto& shape_out = (*output_shapes_)[0]; //output shape
    shape_out[0] = indice_shape[1];
    //calculate the output shape
    if (dim_out - 1 > 0) {
        std::copy(data_shape.end() - (dim_out - 1), data_shape.end(), shape_out.begin()+1);
    }
    
    //calculate the offset coefficient of the data sequence
    int* base_offset = new int[data_dim];
    base_offset[data_dim - 1] = 1;
    for (int i = 0; i < data_dim - 1; i++) {
        int temp_base = 1;
        for (int i_temp_base = i; i_temp_base < data_dim - 1; i_temp_base++) {
            temp_base *= data_shape[i_temp_base + 1];
        }
        base_offset[i] = temp_base;
    }
    
    //copy the list with different offset according to different output dim
    float* out_ptr = output_tensors_[0];
    int out_index = 0;
    for (int i = 0; i < shape_out[0]; i++) {
        int offset_move = 0;  //the offset of every move action
        for (int i_move = 0; i_move < indice_shape[0]; i_move++) {
            offset_move += extract_from_indice(indice, indice_shape,i_move, i) * base_offset[i_move];
        }
        memcpy(out_ptr + out_index, data_ptr + offset_move, base_offset[indice_shape[0] - 1] * sizeof(float));
        out_index += base_offset[indice_shape[0] - 1];
    }
    
    delete []base_offset;
    if(is_float){
        delete []indice;
    }
    
    return 0;

}

int cpu_gatherndlayer::reshape(void *param, int param_size,
    const vector<vector<int> > &input_shapes, vector<vector<int> > &output_shapes) {
    CPU_ASSERT_EQ(input_shapes.size(), 2);
    auto x_shape = input_shapes[0];
    auto y_shape = input_shapes[1];
    int diff_size = x_shape.size()-y_shape[0];
    CPU_ASSERT_GE(diff_size, 0);
    vector<int> out_shape(y_shape.begin()+1, y_shape.end());
    if(diff_size>0){
        out_shape.insert(out_shape.end(), x_shape.begin()+y_shape[0], x_shape.end());
    }
    output_shapes.assign(1, out_shape);
    return 0;
}

int cpu_gatherndlayer::extract_from_indice(const int* ptr, vector<int> shape, int i0, int i1) {
    return ptr[i0 * shape[1] + i1];
}

REGISTER_CPULAYER_CLASS(CPU_GATHERND, cpu_gathernd)
}/* namespace bmcpu*/
