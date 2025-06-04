#ifndef CPU_GATHER_H
#define CPU_GATHER_H

#include "cpu_layer.h"

/*
 *  there are two tensor，tensor[0] is params, tensor[1] is indices.
 *  param is axis.
 */

namespace bmcpu {

class cpu_gatherlayer : public cpu_layer {
public:
    explicit cpu_gatherlayer() {}
    virtual ~cpu_gatherlayer() {}

    int process(void* param, int param_size);
    inline int get_shape_index(int*     shape,
                               int*     index,
                               int      size);
    int git_all_index(int                indices_val,
                      int                indices_idx,
                      int                axis_shape,       //axis's shape
                      std::vector<int>*  src_offset,     //source offest array
                      std::vector<int>*  dest_offset);   //dest offest array
    int gather_inner(float*         output_tensor,
                     const float*   input_tensor,
                     const int*     indices,
                     int            axis_shape);

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes) {
      std::vector<int> shape = input_shapes[0];
      std::vector<int> index_shape = input_shapes[1];
      int input_dim = shape.size();
      int index_dim = index_shape.size();
      output_dim = input_dim + index_dim - 1;
      output_shapes[0].resize(output_dim);
      BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_gather_t, params, param, param_size);
      axis = params->axis;
      if (axis < 0) {  // change negative axis to positive
        axis += input_dim;
      }
      CPU_ASSERT((axis >= 0) && (axis < input_dim));

      // the resule shape is tensor.shape[0:axis] + indices.shape + tensor.shape[axis+1:] =
      // input_dim + index_dim - 1
      for (int i = 0; i < axis; i++) {
        output_shapes[0][i] = shape[i];
      }
      for (int i = 0; i < index_dim; i++) {
        output_shapes[0][axis+i] = index_shape[i];
      }
      for (int i = axis + 1; i < input_dim; i++) {
        output_shapes[0][index_dim + i - 1] = shape[i];
      }
      return 0;
    }
    virtual int dtype(const void* param, size_t param_size, const vector<int>& input_dtypes,
              vector<int>& output_dtypes) override
    {
      output_dtypes.assign(1, input_dtypes[0]);
      return 0;
    }

protected:
    int batch_num;          //batch number
    int batch_size;         //batch size
    int index_size;
    int output_dim;
    int axis;
};

} /* namespace bmcpu */


#endif // CPU_GATHER_H
