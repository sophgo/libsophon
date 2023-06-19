#include "cpu_scatter_nd.h"
namespace bmcpu {
void PrepareAndValidateInputs(const std::vector<int> &params_shape,
                              const std::vector<int> &indices_shape,
                              const std::vector<int> &updates_shape,
                              int *slice_dim, int *num_updates, int *slice_size) {
    int element_num = 1;
    for (size_t i = 0; i < indices_shape.size(); ++i)
        element_num *= indices_shape[i];
    *slice_dim = indices_shape.size() > 1 ? indices_shape.back() : 1;
    int total_nd = static_cast<int>(params_shape.size());
    int slice_size_big = 1;
    for (int i = *slice_dim; i < total_nd; ++i)
        slice_size_big *= params_shape[i];
    *slice_size = slice_size_big;
    const int safe_slice_dim = (*slice_dim < 1) ? 1 : *slice_dim;
    *num_updates = element_num / safe_slice_dim;
}
template <int IXDIM>
struct ScatterNdFunctor {
    void operator()(const int slice_size, const int *output_shape_prefix,
                    const int *indices, const std::vector<int> &indices_shape,
                    const float *updates, const std::vector<int> &updates_shape,
                    float *output, const std::vector<int> &output_shape) {
        int batch_size = indices_shape[0];
        int batch_strides[IXDIM];
        for (int dim = IXDIM - 1; dim >= 0; --dim) {
            if (dim == IXDIM - 1)
                batch_strides[dim] = 1;
            else
                batch_strides[dim] = batch_strides[dim + 1] * output_shape_prefix[dim + 1];
        }
        for (int loc = 0; loc < batch_size; ++loc) {
            int i = 0;
            for (int dim = 0; dim < IXDIM; ++dim) {
                const int ix_d = indices[loc * indices_shape[1] + dim];
                i += ix_d * batch_strides[dim];
            }
            const float *src = updates + loc * updates_shape[1];
            float *dst = output + i * output_shape[1];
            for (int j = 0; j < updates_shape[1]; ++j)
                dst[j] += src[j];
        }
    }
};
int cpu_scatter_ndlayer::process(void *param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_scatter_nd_param_t, p, param, param_size);
    int slice_dim, num_updates, slice_size;
    std::vector<int> params_shape(p->dim);
    int shape_num_elements = 1;
    if (input_shapes_[2].size() == 1) {
        for (int i = 0; i < p->dim; ++i) {
            params_shape[i] = reinterpret_cast<int *>(input_tensors_[2])[i];
            shape_num_elements *= params_shape[i];
        }
    } else {
        for (int i = 0; i < p->dim; ++i) {
            params_shape[i] = input_shapes_[2][i];
            shape_num_elements *= params_shape[i];
        }
    }
    PrepareAndValidateInputs(params_shape,
                             input_shapes_[0],
                             input_shapes_[1],
                             &slice_dim,
                             &num_updates,
                             &slice_size);
    std::vector<int> indices_flat_shape = {1, slice_dim};
    for (size_t i = 0; i < input_shapes_[0].size(); ++i)
        indices_flat_shape[0] *= input_shapes_[0][i];
    indices_flat_shape[0] /= slice_dim;
    auto indices_flat = reinterpret_cast<int *>(input_tensors_[0]);
    std::vector<int> updates_flat_shape = {num_updates, slice_size};
    auto updates_flat = reinterpret_cast<float *>(input_tensors_[1]);
    std::vector<int> output_matrix_shape = {shape_num_elements / slice_size, slice_size};
    auto output_matrix = reinterpret_cast<float *>(output_tensors_[0]);
    if (input_shapes_[2].size() == 1) {
        memset(output_matrix, 0x0, shape_num_elements * sizeof(int));
    } else {
        for (int i = 0; i < shape_num_elements; ++i) {
            output_matrix[i] = reinterpret_cast<float *>(input_tensors_[2])[i];
        }
    }
    if (shape_num_elements > 0) {
        switch (slice_dim) {
#define PARAMS_CASE(IXDIM)                                                      \
      case IXDIM: {                                                             \
        int output_shape_prefix[IXDIM];                                         \
        for (int i = 0; i < IXDIM; ++i)                                         \
            output_shape_prefix[i] = params_shape[i];                           \
        ScatterNdFunctor<IXDIM> functor;                                        \
            functor(slice_size, output_shape_prefix, indices_flat,              \
            indices_flat_shape, updates_flat, updates_flat_shape,               \
            output_matrix, output_matrix_shape);                                \
      } break
            PARAMS_CASE(1);
            PARAMS_CASE(2);
            PARAMS_CASE(3);
            PARAMS_CASE(4);
            PARAMS_CASE(5);
            PARAMS_CASE(6);
            PARAMS_CASE(7);
#undef PARAMS_CASE
        default:
            CPU_ASSERT(0);
        }
    }
    (*output_shapes_)[0].clear();
    for (int i = 0; i < p->dim; ++i)
        (*output_shapes_)[0].push_back(p->shape[i]);
    return 0;
}
int cpu_scatter_ndlayer::reshape(void *param, int param_size,
                                 const vector<vector<int>> &input_shapes,
                                 vector<vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_scatter_nd_param_t, p, param, param_size);
    output_shapes[0].resize(static_cast<size_t>(p->dim));
    for (int i = 0; i < p->dim; ++i)
        output_shapes[0][i] = p->shape[i];
    return 0;
}
int cpu_scatter_ndlayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
                               vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[1]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_SCATTER_ND, cpu_scatter_nd);
} // namespace bmcpu
