#include "cpu_gather_pytorch.h"
#include "bmcpu_common.h"
namespace bmcpu {
static inline void gather_dim1_0(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    for (int i = 0; i < shape[0]; ++i) {
        *dst = src[*idx];
        ++dst;
        ++idx;
    }
}
static inline void gather_dim2_0(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    for (int i = 0; i < shape[0]; ++i) {
        for (int j = 0; j < shape[1]; ++j) {
            *dst = src[*idx * org_shape[1] + j];
            ++dst;
            ++idx;
        }
    }
}
static inline void gather_dim2_1(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    for (int i = 0; i < shape[0]; ++i) {
        int idx_i = i * org_shape[1];
        for (int j = 0; j < shape[1]; ++j) {
            *dst = src[idx_i + *idx];
            ++dst;
            ++idx;
        }
    }
}
static inline void gather_dim3_0(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    int shape_1_2 = org_shape[1] * org_shape[2];
    for (int i = 0; i < shape[0]; ++i) {
        for (int j = 0; j < shape[1]; ++j) {
            int idx_j = j * org_shape[2];
            for (int k = 0; k < shape[2]; ++k) {
                *dst = src[*idx * shape_1_2 + idx_j + k];
                ++dst;
                ++idx;
            }
        }
    }
}
static inline void gather_dim3_1(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    int shape_1_2 = org_shape[1] * org_shape[2];
    for (int i = 0; i < shape[0]; ++i) {
        int idx_i = i * shape_1_2;
        for (int j = 0; j < shape[1]; ++j) {
            for (int k = 0; k < shape[2]; ++k) {
                *dst = src[idx_i + *idx * org_shape[2] + k];
                ++dst;
                ++idx;
            }
        }
    }
}
static inline void gather_dim3_2(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    int shape_1_2 = org_shape[1] * org_shape[2];
    for (int i = 0; i < shape[0]; ++i) {
        int idx_i = i * shape_1_2;
        for (int j = 0; j < shape[1]; ++j) {
            int idx_j = idx_i + j * org_shape[2];
            for (int k = 0; k < shape[2]; ++k) {
                *dst = src[idx_j + *idx];
                ++dst;
                ++idx;
            }
        }
    }
}
static inline void gather_dim4_0(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    int shape_1_2_3 = org_shape[1] * org_shape[2] * org_shape[3];
    int shape_2_3 = org_shape[2] * org_shape[3];
    for (int i = 0; i < shape[0]; ++i) {
        for (int j = 0; j < shape[1]; ++j) {
            int idx_j = j * shape_2_3;
            for (int k = 0; k < shape[2]; ++k) {
                int idx_k = idx_j + k * org_shape[3];
                for (int t = 0; t < shape[3]; ++t) {
                    *dst = src[*idx * shape_1_2_3 + idx_k + t];
                    ++dst;
                    ++idx;
                }
            }
        }
    }
}
static inline void gather_dim4_1(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    int shape_1_2_3 = org_shape[1] * org_shape[2] * org_shape[3];
    int shape_2_3 = org_shape[2] * org_shape[3];
    for (int i = 0; i < shape[0]; ++i) {
        int idx_i = i * shape_1_2_3;
        for (int j = 0; j < shape[1]; ++j) {
            for (int k = 0; k < shape[2]; ++k) {
                int idx_k = k * org_shape[3];
                for (int t = 0; t < shape[3]; ++t) {
                    *dst = src[idx_i + *idx * shape_2_3 + idx_k + t];
                    ++dst;
                    ++idx;
                }
            }
        }
    }
}
static inline void gather_dim4_2(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    int shape_1_2_3 = org_shape[1] * org_shape[2] * org_shape[3];
    int shape_2_3 = org_shape[2] * org_shape[3];
    for (int i = 0; i < shape[0]; ++i) {
        int idx_i = i * shape_1_2_3;
        for (int j = 0; j < shape[1]; ++j) {
            int idx_j = idx_i + j * shape_2_3;
            for (int k = 0; k < shape[2]; ++k) {
                for (int t = 0; t < shape[3]; ++t) {
                    *dst = src[idx_j + *idx * org_shape[3] + t];
                    ++dst;
                    ++idx;
                }
            }
        }
    }
}
static inline void gather_dim4_3(
    float *dst, const float *src, const int *idx, const int *shape, int *org_shape) {
    int shape_1_2_3 = org_shape[1] * org_shape[2] * org_shape[3];
    int shape_2_3 = org_shape[2] * org_shape[3];
    for (int i = 0; i < shape[0]; ++i) {
        int idx_i = i * shape_1_2_3;
        for (int j = 0; j < shape[1]; ++j) {
            int idx_j = idx_i + j * shape_2_3;
            for (int k = 0; k < shape[2]; ++k) {
                int idx_k = idx_j + k * org_shape[3];
                for (int t = 0; t < shape[3]; ++t) {
                    *dst = src[idx_k + *idx];
                    ++dst;
                    ++idx;
                }
            }
        }
    }
}
int cpu_gather_ptlayer::process(void* param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_gather_t, p, param, param_size);
    std::vector<int> input_shape = input_shapes_[0];
    std::vector<int> index_shape = input_shapes_[1];
    const float *input = reinterpret_cast<float *>(input_tensors_[0]);
    const int *index = reinterpret_cast<int *>(input_tensors_[1]);
    float *output = reinterpret_cast<float *>(output_tensors_[0]);
    switch (input_shape.size()) {
    case 1:
        gather_dim1_0(output, input, index, index_shape.data(), input_shape.data());
        break;
    case 2:
        if (p->axis == 0)
            gather_dim2_0(output, input, index, index_shape.data(), input_shape.data());
        else if (p->axis == 1)
            gather_dim2_1(output, input, index, index_shape.data(), input_shape.data());
        break;
    case 3:
        if (p->axis == 0)
            gather_dim3_0(output, input, index, index_shape.data(), input_shape.data());
        else if (p->axis == 1)
            gather_dim3_1(output, input, index, index_shape.data(), input_shape.data());
        else if (p->axis == 2)
            gather_dim3_2(output, input, index, index_shape.data(), input_shape.data());
        break;
    case 4:
        if (p->axis == 0)
            gather_dim4_0(output, input, index, index_shape.data(), input_shape.data());
        else if (p->axis == 1)
            gather_dim4_1(output, input, index, index_shape.data(), input_shape.data());
        else if (p->axis == 2)
            gather_dim4_2(output, input, index, index_shape.data(), input_shape.data());
        else if (p->axis == 3)
            gather_dim4_3(output, input, index, index_shape.data(), input_shape.data());
        break;
    default:
        printf("error: %s: %d: invalid input dimension: %d. \n",
               __FILE__, __LINE__, static_cast<int>(input_shape.size()));
        exit(-1);
    }
    (*output_shapes_)[0] = index_shape;
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_GATHER_PT, cpu_gather_pt);
} /* namespace bmcpu*/
