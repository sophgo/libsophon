#ifndef _CPU_PADDLE_DENSITY_PIROR_BOX_H
#define _CPU_PADDLE_DENSITY_PRIOR_BOX_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_paddle_density_prior_boxlayer : public cpu_layer {
public:
    cpu_paddle_density_prior_boxlayer() {}
    ~cpu_paddle_density_prior_boxlayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);

    void setParam(void *param, int psize);

protected:
    bool clip_;
    bool flatten_to_2d_;
    float offset_;
    float step_h_;
    float step_w_;
    int  densities_[10];
    int densities_size_;
    int fixed_ratios_size_;
    int fixed_sizes_size_;
    int variances_size_;
    int num_priors_;
};
} // namespace bmcpu
#endif  /* _CPU_PADDLE_DENSITY_PRIOR_BOX_H */
