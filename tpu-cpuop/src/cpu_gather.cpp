#include <string>
#include <vector>
#include "cpu_gather.h"
#include "cpu_layer.h"

namespace bmcpu {

inline int cpu_gatherlayer::get_shape_index(int *shape, int *index, int size)
{
    int pointer = index[0];
    for (int i = 1; i < size; i++) {
        pointer *= shape[i];
        pointer += index[i];
    }
    return pointer;
}

int cpu_gatherlayer::git_all_index(int                indices_val,
                                   int                indices_idx,
                                   int                axis_shape,      //axis's shape
                                   std::vector<int>*  src_offset,     //source offest array
                                   std::vector<int>*  dest_offset)    //dest offset array
{
    int src_idx[3];
    int dest_idx[3];
    int src_shape[3];
    int dest_shape[3];

    dest_idx[0] = indices_idx;
    dest_idx[2] = 0;
    dest_shape[0] = index_size;
    dest_shape[1] = batch_num;
    dest_shape[2] = batch_size;

    src_idx[1] = indices_val;
    src_idx[2] = 0;
    src_shape[0] = batch_num;
    src_shape[1] = axis_shape;
    src_shape[2] = batch_size;

    //get each output pointer
    for (int i = 0; i < batch_num; i++) {
        dest_idx[1] = i;
        src_idx[0] = i;
        (*dest_offset)[i] = get_shape_index(dest_shape,dest_idx,3);
        (*src_offset)[i] = get_shape_index(src_shape,src_idx,3);
    }
    return 0;
}

int cpu_gatherlayer:: gather_inner(float*        output_tensor,
                                   const float*  input_tensor,
                                   const int*    indices,
                                   int           axis_shape)
{
    //1：
    std::vector<int> src_offset(batch_num);
    std::vector<int> dest_offset(batch_num);
    for (int i = 0; i < index_size; i++) {
        CPU_ASSERT(indices[i] < axis_shape);
        //get source pointer offest, and dest pointer offset
        git_all_index(indices[i], i, axis_shape, &src_offset, &dest_offset);
        //copy data to output
        for (int k = 0; k < batch_num; k++) {
            memcpy(output_tensor + dest_offset[k],
                   input_tensor + src_offset[k],
                   batch_size * sizeof(float));
        }
    }
    return 0;
}

int cpu_gatherlayer::process(void* param, int param_size) {

    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_gather_t, params, param, param_size);

    const float *tensor = input_tensors_[0];
    const int   *indices = reinterpret_cast<int *>(input_tensors_[1]);
    std::vector<int> shape = input_shapes_[0];
    std::vector<int> index_shape = input_shapes_[1];
    int input_dim = shape.size();
    int index_dim = index_shape.size();
//    for (int i = 0; i < params->input_dimension; i++) {
//        shape.push_back(input_shapes_[0][i]);
//    }
//    std::vector<int> index_shape;
//    for (int i = 0; i < params->index_dim; i++) {
//        index_shape.push_back(input_shapes_[1][i]);
//    }

    output_dim = input_dim + index_dim - 1;
    axis = params->axis;
    if (axis < 0) {     //change negative axis to positive
        axis += input_dim;
    }
    CPU_ASSERT((axis >= 0) && (axis < input_dim));

    //the resule shape is tensor.shape[0:axis] + indices.shape + tensor.shape[axis+1:] = input_dim + index_dim - 1
    batch_size = 1;
    batch_num = 1;
    index_size = 1;
    if (output_dim > 0) {
      for (int i = 0; i < index_dim; i++) {
        (*output_shapes_)[0][axis + i] = index_shape[i];
        index_size *= index_shape[i];
      }
      for (int i = 0; i < axis; i++) {
        (*output_shapes_)[0][i] = shape[i];
        batch_num *= shape[i];
      }
      for (int i = axis + 1; i < input_dim; i++) {
        (*output_shapes_)[0][index_dim + i - 1] = shape[i];
        batch_size *= shape[i];
      }
    } else {
      std::vector<int> out_shape(0);
      (*output_shapes_)[0] = out_shape;
    }

    float * output = output_tensors_[0];
    gather_inner(output, tensor, indices, shape[axis]);
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_GATHER, cpu_gather)

} /* namespace bmcpu*/
