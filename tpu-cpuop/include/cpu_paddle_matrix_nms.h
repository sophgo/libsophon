#ifndef _CPU_PADDLE_MATRIX_NMS_LAYER_H
#define _CPU_PADDLE_MATRIX_NMS_LAYER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_paddle_matrix_nmslayer : public cpu_layer {
public:
    cpu_paddle_matrix_nmslayer() {}
    ~cpu_paddle_matrix_nmslayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);

    void setParam(void *param, int psize);

protected:

    bool has_output_nms_num_;
    int background_label_;
    int nms_top_k_;
    int keep_top_k_;
    bool normalized_;
    float score_threshold_;
    float post_threshold_;
    bool use_gaussian_;
    float gaussian_sigma_;

};
} // namespace bmcpu
#endif  /* _CPU_TENSORFLOW_NMS_V5_LAYER_H */
