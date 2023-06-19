#include "cpu_paddle_density_prior_box.h"
#include <algorithm>
#include <cmath>
namespace bmcpu {
int cpu_paddle_density_prior_boxlayer::process(void *param, int psize) {
    setParam(param, psize);
    auto* boxes = output_tensors_[0];
    auto* vars = output_tensors_[1];

    float* variances = input_tensors_[4];
    float* fixed_sizes = input_tensors_[3];
    float* fixed_ratios = input_tensors_[2];

    auto clip = clip_;
    auto step_w = step_w_;
    auto step_h = step_h_;
    auto offset = offset_;

    auto img_width = input_shapes_[1][3];
    auto img_height = input_shapes_[1][2];

    auto feature_width = input_shapes_[0][3];
    auto feature_height = input_shapes_[0][2];
    fixed_ratios_size_ = input_shapes_[2][0];
    fixed_sizes_size_ = input_shapes_[3][0];
    variances_size_ = input_shapes_[4][0];

    float step_width, step_height;
    if (step_w == 0 || step_h == 0) {
        step_width = static_cast<int>(img_width) / feature_width;
        step_height = static_cast<int>(img_height) / feature_height;
    } else {
        step_width = step_w;
        step_height = step_h;
    }

    num_priors_ = 0;
    for (size_t i = 0; i < densities_size_; ++i) {
        num_priors_ += fixed_ratios_size_ * (pow(densities_[i], 2));
    }
    std::vector<float> sqrt_fixed_ratios;

    for (size_t i = 0; i < fixed_ratios_size_; i++) {
        sqrt_fixed_ratios.push_back(sqrt(fixed_ratios[i]));
    }
    int step_average = static_cast<int>((step_width + step_height) * 0.5);
    int idx = 0;
    for (int h = 0; h < feature_height; ++h) {
        for (int w = 0; w < feature_width; ++w) {
            int center_x = (w + offset) * step_width;
            int center_y = (h + offset) * step_height;
            // Generate density prior boxes with fixed sizes.
            for (size_t s = 0; s < fixed_sizes_size_; ++s) {
                auto fixed_size = fixed_sizes[s];
                int density = densities_[s];
                int shift = step_average / density;
                // Generate density prior boxes with fixed ratios.
                for (size_t r = 0; r < fixed_ratios_size_; ++r) {
                    float box_width_ratio = fixed_size * sqrt_fixed_ratios[r];
                    float box_height_ratio = fixed_size / sqrt_fixed_ratios[r];
                    float density_center_x = center_x - step_average / 2. + shift / 2.;
                    float density_center_y = center_y - step_average / 2. + shift / 2.;
                    for (int di = 0; di < density; ++di) {
                        for (int dj = 0; dj < density; ++dj) {
                            float center_x_temp = density_center_x + dj * shift;
                            float center_y_temp = density_center_y + di * shift;
                            boxes[idx++] = std::max(
                                               (center_x_temp - box_width_ratio / 2.) / img_width, 0.);
                            boxes[idx++] = std::max(
                                               (center_y_temp - box_height_ratio / 2.) / img_height, 0.);
                            boxes[idx++] = std::min(
                                               (center_x_temp + box_width_ratio / 2.) / img_width, 1.);
                            boxes[idx++] = std::min(
                                               (center_y_temp + box_height_ratio / 2.) / img_height, 1.);
                        }
                    }
                }
            }
        }
    }

    if (clip) {
        float* dt = boxes;
        std::transform(dt, dt + (feature_width * feature_height * num_priors_ * 4), dt, [](float v) -> float {
            return std::min<float>(std::max<float>(v, 0.), 1.);
        });
    }
    int box_num = feature_width * feature_height * num_priors_;
    idx = 0;
    for (int i = 0; i < box_num; ++i) {
        for (size_t j = 0; j < variances_size_; ++j) {
            vars[idx++] = variances[j];
        }
    }

    (*output_shapes_)[0][0] = input_shapes_[0][2];
    (*output_shapes_)[0][1] = input_shapes_[0][3];
    (*output_shapes_)[0][2] = num_priors_;
    (*output_shapes_)[0][3] = 4;

    (*output_shapes_)[1][0] = input_shapes_[0][2];
    (*output_shapes_)[1][1] = input_shapes_[0][3];
    (*output_shapes_)[1][2] = num_priors_;
    (*output_shapes_)[1][3] = 4;

    return 0;
}

void cpu_paddle_density_prior_boxlayer::setParam(void *param, int psize) {
    layer_param_ = param;
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_density_prior_box_param_t, density_prior_box_param, layer_param_, psize);
    clip_ = density_prior_box_param->clip;
    flatten_to_2d_ = density_prior_box_param->flatten_to_2d;
    offset_ = density_prior_box_param->offset;
    step_h_ = density_prior_box_param->step_h;
    step_w_ = density_prior_box_param->step_w;

    densities_size_ = density_prior_box_param->densities_size;
    for (int i = 0; i < densities_size_; i++) {
        densities_[i] = density_prior_box_param->densities[i];
    }
}

int cpu_paddle_density_prior_boxlayer::reshape(
    void* param, int psize,
    const vector<vector<int>>& input_shapes,
    vector<vector<int>>& output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_paddle_density_prior_box_param_t, p, param, psize);

    fixed_ratios_size_ = input_shapes[2][0];
    fixed_sizes_size_ = input_shapes[3][0];
    variances_size_ = input_shapes[4][0];
    densities_size_ = p->densities_size;
    for (int i = 0; i < densities_size_; i++) {
        densities_[i] = p->densities[i];
    }
    //output_shape = [input.h input.w, num_priors, 4]
    //num_priors = += (fixed_ratios.size()) * (pow(densities[i], 2));
    num_priors_ = 0;
    for (size_t i = 0; i < densities_size_; ++i) {
        num_priors_ += fixed_ratios_size_ * (pow(densities_[i], 2));
    }
    output_shapes[0][0] = input_shapes[0][2];
    output_shapes[0][1] = input_shapes[0][3];
    output_shapes[0][2] = num_priors_;
    output_shapes[0][3] = 4;

    output_shapes[1][0] = input_shapes[0][2];
    output_shapes[1][1] = input_shapes[0][3];
    output_shapes[1][2] = num_priors_;
    output_shapes[1][3] = 4;

    return 0;
}

int cpu_paddle_density_prior_boxlayer::dtype(
    const void *param, size_t psize,
    const vector<int>& input_dtypes,
    vector<int>& output_dtypes) {
    CPU_ASSERT(input_dtypes.size() > 0);
    output_dtypes = {input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_PADDLE_DENSITY_PRIOR_BOX, cpu_paddle_density_prior_box);
}/* namespace bmcpu*/
