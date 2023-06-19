#include <string>
#include <vector>
#include "cpu_gathernd_tf.h"
#include "cpu_layer.h"

namespace bmcpu {

inline const float* get_input(const float* input,
                              const int* indices,
                              std::vector<int>& strides,
                              int in_flat,
                              int slice_dim)
{
    int pa = 0;
    for (int i=0; i<slice_dim; ++i) {
        pa += strides[i] * indices[i];
    }
    pa *= in_flat;
    return input + pa;
}

/*  input tensor: input, indices
 *  output tensor: output
 */
int cpu_gathernd_tflayer::process(void* param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_gathernd_t, params, param, param_size);
    auto batch_dims = params->batch_dims;
    const float* input = input_tensors_[0];
    const int   *indices = reinterpret_cast<int *>(input_tensors_[1]);
    std::vector<int> input_shape = input_shapes_[0];
    std::vector<int> index_shape = input_shapes_[1];
    float *output = (output_tensors_[0]);
    auto input_init = input;
    const int input_dim = input_shape.size();
    const int index_dim = index_shape.size();
    const int slice_dim = index_shape[index_dim-1];
    CPU_ASSERT(slice_dim + batch_dims <= input_dim);
    const int output_dim = index_dim - 1 + input_dim - slice_dim - batch_dims;
    CPU_ASSERT(output_dim == (*output_shapes_)[0].size());
    std::vector<int> out_shape(output_dim);

    int batch_volume = 1;
    int index_volume = 1;
    for (int i=0; i<index_dim-1; ++i) {
        out_shape[i] = index_shape[i];
        if(i<batch_dims){
            batch_volume *= index_shape[i];
        }else{
            index_volume *= index_shape[i];
        }  
    }

    int index_n_stride = 1;
    for(int i=batch_dims;i<input_dim;++i){
        index_n_stride *= input_shape[i];
    }

    int inner_flat = 1;
    const int offset_dim = index_dim - 1 - slice_dim - batch_dims;
    for (int i=slice_dim + batch_dims; i<input_dim ; ++i) {
        out_shape[i+offset_dim] = input_shape[i];
        inner_flat *= input_shape[i];
    }


    std::vector<int> strides(slice_dim);
    strides[slice_dim -1] = 1;
    int muldata = 1;
    for (int i=slice_dim -2; i>=0; --i) {
        muldata = muldata * input_shape[i+1];
        strides[i] = muldata;
    }

    //copy data to output
    for(int i=0; i<batch_volume; ++i){
        input = input_init + i * index_n_stride;
        for(int j=0; j<index_volume; ++j){
            const float *input_offset = get_input(input, indices, strides, inner_flat, slice_dim);
            memcpy(output, input_offset, inner_flat*sizeof(float));
            output += inner_flat;
            indices += slice_dim;
        }
    }

    for (int i=0; i<output_dim; ++i) {
        (*output_shapes_)[0][i] = out_shape[i];
    }
    return 0;

}

int cpu_gathernd_tflayer::reshape(void* param, int param_size,
            const vector<vector<int>>& input_shapes,
                                  vector<vector<int>>& output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_gathernd_t, params, param, param_size);
    auto batch_dims = params->batch_dims;
    std::vector<int> input_shape = input_shapes[0];
    std::vector<int> index_shape = input_shapes[1];
    const int input_dim = input_shape.size();
    const int index_dim = index_shape.size();
    const int slice_dim = index_shape[index_dim-1];
    const int output_dim = index_dim - 1 + input_dim - slice_dim - batch_dims;
    std::vector<int> out_shape(output_dim);

    for (int i=0; i<index_dim-1; ++i) {
        out_shape[i] = index_shape[i];
    }
    const int offset_dim = index_dim - 1 - slice_dim - batch_dims;
    for (int i=slice_dim + batch_dims; i<input_dim ; ++i) {
        out_shape[i+offset_dim] = input_shape[i];
    }

    output_shapes = {out_shape};
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_GATHERND_TF, cpu_gathernd_tf);

} /* namespace bmcpu*/
