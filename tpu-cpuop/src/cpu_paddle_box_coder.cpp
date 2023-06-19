#include "cpu_paddle_box_coder.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <queue>
#include <cstring>

namespace bmcpu {

enum class BoxCodeType { kEncodeCenterSize = 0, kDecodeCenterSize = 1 };

inline BoxCodeType GetBoxCodeType(const std::string &type) {
    if (type == "encode_center_size") {
        return BoxCodeType::kEncodeCenterSize;
    } else {
        return BoxCodeType::kDecodeCenterSize;
    }
}

template<typename T>
void EncodeCenterSize(T *target_box_data,std::vector<int> target_box_shape,
                      T *prior_box_data, std::vector<int> prior_box_shape,
                      T *prior_box_var_data, std::vector<int> prior_box_var_shape,
                      const bool normalized,
                      float* variance, T *output) {
    int64_t row = target_box_shape[0];
    int64_t col = prior_box_shape[0];
    int64_t len = prior_box_shape[1];

    for (int64_t i = 0; i < row; ++i) {
        for (int64_t j = 0; j < col; ++j) {
            size_t offset = i * col * len + j * len;
            T prior_box_width = prior_box_data[j * len + 2] -
                                prior_box_data[j * len] + (normalized == false);
            T prior_box_height = prior_box_data[j * len + 3] -
                                 prior_box_data[j * len + 1] +
                                 (normalized == false);
            T prior_box_center_x = prior_box_data[j * len] + prior_box_width / 2;
            T prior_box_center_y =
                prior_box_data[j * len + 1] + prior_box_height / 2;

            T target_box_center_x =
                (target_box_data[i * len + 2] + target_box_data[i * len]) / 2;
            T target_box_center_y =
                (target_box_data[i * len + 3] + target_box_data[i * len + 1]) / 2;
            T target_box_width = target_box_data[i * len + 2] -
                                 target_box_data[i * len] + (normalized == false);
            T target_box_height = target_box_data[i * len + 3] -
                                  target_box_data[i * len + 1] +
                                  (normalized == false);

            output[offset] =
                (target_box_center_x - prior_box_center_x) / prior_box_width;
            output[offset + 1] =
                (target_box_center_y - prior_box_center_y) / prior_box_height;
            output[offset + 2] =
                std::log(std::fabs(target_box_width / prior_box_width));
            output[offset + 3] =
                std::log(std::fabs(target_box_height / prior_box_height));
        }
    }

    if (prior_box_var_data) {
        for (int64_t i = 0; i < row; ++i) {
            for (int64_t j = 0; j < col; ++j) {
                for (int k = 0; k < 4; ++k) {
                    size_t offset = i * col * len + j * len;
                    int prior_var_offset = j * len;
                    output[offset + k] /= prior_box_var_data[prior_var_offset + k];
                }
            }
        }
    } else if (variance!=nullptr) {
        for (int64_t i = 0; i < row; ++i) {
            for (int64_t j = 0; j < col; ++j) {
                for (int k = 0; k < 4; ++k) {
                    size_t offset = i * col * len + j * len;
                    output[offset + k] /= static_cast<T>(variance[k]);
                }
            }
        }
    }
}

template <typename T, int axis, int var_size>
void DecodeCenterSize(T *target_box_data,std::vector<int> target_box_shape,
                      T *prior_box_data, std::vector<int> prior_box_shape,
                      T *prior_box_var_data, std::vector<int> prior_box_var_shape,
                      const bool normalized, float* variance,
                      T *output)  {
    int64_t row = target_box_shape[0];
    int64_t col = target_box_shape[1];
    int64_t len = target_box_shape[2];

    for (int64_t i = 0; i < row; ++i) {
        for (int64_t j = 0; j < col; ++j) {
            T var_data[4] = {1., 1., 1., 1.};
            T *var_ptr = var_data;
            size_t offset = i * col * len + j * len;
            int prior_box_offset = axis == 0 ? j * len : i * len;

            T prior_box_width = prior_box_data[prior_box_offset + 2] -
                                prior_box_data[prior_box_offset] +
                                (normalized == false);
            T prior_box_height = prior_box_data[prior_box_offset + 3] -
                                 prior_box_data[prior_box_offset + 1] +
                                 (normalized == false);
            T prior_box_center_x =
                prior_box_data[prior_box_offset] + prior_box_width / 2;
            T prior_box_center_y =
                prior_box_data[prior_box_offset + 1] + prior_box_height / 2;

            T target_box_center_x = 0, target_box_center_y = 0;
            T target_box_width = 0, target_box_height = 0;
            int prior_var_offset = axis == 0 ? j * len : i * len;
            if (var_size == 2) {
                std::memcpy(var_ptr, prior_box_var_data + prior_var_offset,
                            4 * sizeof(T));
            } else if (var_size == 1) {
                var_ptr = reinterpret_cast<T *>(variance);
            }
            T box_var_x = *var_ptr;
            T box_var_y = *(var_ptr + 1);
            T box_var_w = *(var_ptr + 2);
            T box_var_h = *(var_ptr + 3);

            target_box_center_x =
                box_var_x * target_box_data[offset] * prior_box_width +
                prior_box_center_x;
            target_box_center_y =
                box_var_y * target_box_data[offset + 1] * prior_box_height +
                prior_box_center_y;
            target_box_width =
                std::exp(box_var_w * target_box_data[offset + 2]) * prior_box_width;
            target_box_height = std::exp(box_var_h * target_box_data[offset + 3]) *
                                prior_box_height;

            output[offset] = target_box_center_x - target_box_width / 2;
            output[offset + 1] = target_box_center_y - target_box_height / 2;
            output[offset + 2] =
                target_box_center_x + target_box_width / 2 - (normalized == false);
            output[offset + 3] =
                target_box_center_y + target_box_height / 2 - (normalized == false);
        }
    }
}

int cpu_paddle_box_coderlayer::process(void *param, int psize) {
#ifdef CALC_TIME
    struct timeval start,  stop;
    gettimeofday(&start, 0);
#endif
    setParam(param, psize);
    float* prior_boxes_data = input_tensors_[0];
    float* prior_boxes_var_data = input_tensors_[1];
    float* target_box_data = input_tensors_[2];

    float* variance = nullptr;

    const int axis = axis_;
    bool normalized = box_normalized_;
    auto code_type = GetBoxCodeType(code_type_);

    auto prior_boxes_shape = input_shapes_[0];
    auto prior_boxes_var_shape = input_shapes_[1];
    auto target_box_shape = input_shapes_[2];

    float* output = output_tensors_[0];
    if (code_type == BoxCodeType::kEncodeCenterSize) {
        EncodeCenterSize<float>(target_box_data, target_box_shape, prior_boxes_data,prior_boxes_shape,prior_boxes_var_data,prior_boxes_var_shape,normalized,
                                variance, output);
    } else if (code_type == BoxCodeType::kDecodeCenterSize) {
        if (prior_boxes_var_data) {
            if (axis == 0) {
                DecodeCenterSize<float, 0, 2>(target_box_data, target_box_shape, prior_boxes_data,prior_boxes_shape,prior_boxes_var_data,prior_boxes_var_shape,
                                              normalized, variance, output);
            } else {
                DecodeCenterSize<float, 1, 2>(target_box_data, target_box_shape, prior_boxes_data,prior_boxes_shape,prior_boxes_var_data,prior_boxes_var_shape,
                                              normalized, variance, output);
            }
        } else if (variance!=nullptr) {
            if (axis == 0) {
                DecodeCenterSize<float, 0, 1>(target_box_data, target_box_shape, prior_boxes_data,prior_boxes_shape,prior_boxes_var_data,prior_boxes_var_shape,
                                              normalized, variance, output);
            } else {
                DecodeCenterSize<float, 1, 1>(target_box_data, target_box_shape, prior_boxes_data,prior_boxes_shape,prior_boxes_var_data,prior_boxes_var_shape,
                                              normalized, variance, output);
            }
        } else {
            if (axis == 0) {
                DecodeCenterSize<float, 0, 0>(target_box_data, target_box_shape, prior_boxes_data,prior_boxes_shape,prior_boxes_var_data,prior_boxes_var_shape,
                                              normalized, variance, output);
            } else {
                DecodeCenterSize<float, 1, 0>(target_box_data, target_box_shape, prior_boxes_data,prior_boxes_shape,prior_boxes_var_data,prior_boxes_var_shape,
                                              normalized, variance, output);
            }
        }
    }

    (*output_shapes_)[0][0] = target_box_shape[0];
    auto col = prior_boxes_shape[0];
    if (code_type == BoxCodeType::kDecodeCenterSize) {
        col = target_box_shape[1];
    }
    (*output_shapes_)[0][1] = col;
    (*output_shapes_)[0][2] = prior_boxes_shape[1];

    return 0;
}

void cpu_paddle_box_coderlayer::setParam(void *param, int psize) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_box_coder_param_t, box_coder_param, layer_param_, psize);
    axis_ = box_coder_param->axis;
    box_normalized_ = box_coder_param->box_normalized;
    code_type_.assign(box_coder_param->code_type, box_coder_param->code_type_len);
}

int cpu_paddle_box_coderlayer::reshape(
    void* param, int psize,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes) {
    auto prior_boxes_shape = input_shapes[0];
    auto prior_boxes_var_shape = input_shapes[1];
    auto target_box_shape = input_shapes[2];

    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_box_coder_param_t, box_coder_param, param, psize);
    std::string ct(box_coder_param->code_type, box_coder_param->code_type_len);
    auto code_type = GetBoxCodeType(ct);

    auto row = target_box_shape[0];
    auto col = prior_boxes_shape[0];
    if (code_type == BoxCodeType::kDecodeCenterSize) {
        col = target_box_shape[1];
    }
    auto len = prior_boxes_shape[1];
    output_shapes[0] = {row, col, len};

    return 0;
}

int cpu_paddle_box_coderlayer::dtype(
    const void *param, size_t psize,
    const vector<int>& input_dtypes,
    vector<int>& output_dtypes) {
    CPU_ASSERT(input_dtypes.size() > 0);
    output_dtypes = {input_dtypes[0]};
    return 0;
}

REGISTER_CPULAYER_CLASS(CPU_PADDLE_BOX_CODER, cpu_paddle_box_coder);

}/* namespace bmcpu*/
