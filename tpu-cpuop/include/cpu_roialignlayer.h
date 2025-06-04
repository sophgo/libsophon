#ifndef _CPU_ROI_ALIGN_LAYER_H
#define _CPU_ROI_ALIGN_LAYER_H
#include "cpu_layer.h"

namespace bmcpu {

class cpu_roialignlayer : public cpu_layer {
public:
    explicit cpu_roialignlayer() {}
    virtual ~cpu_roialignlayer() {}

    int process(void *param, int param_size);

    virtual string get_layer_name () const {
        return "ROI_ALIGN";
    }

    int reshape(void* param, int param_size,
                const vector<vector<int>>& input_shapes,
                vector<vector<int>>& output_shapes);

    int dtype(const void *param, size_t param_size, const vector<int> &input_dtypes, vector<int> &output_dtypes);

private:
    struct BilinearCoeff {
        int pos[4];
        float w[4];
    };
    void calc_bilinear_coeff(
            const int input_height,
            const int input_width,
            const int pooled_height,
            const int pooled_width,
            const float roi_start_h,
            const float roi_start_w,
            const float bin_height,
            const float bin_width,
            const int sample_h,
            const int sample_w,
            std::vector<BilinearCoeff>& coeffs
            );
};

} /* namespace bmcpu */
#endif /* _CPU_ROI_ALIGN_LAYER_H */

