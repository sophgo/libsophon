#ifndef _CPU_PADDLE_DEFORMABLE_CONV_H
#define _CPU_PADDLE_DEFORMABLE_CONV_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_paddle_deformable_convlayer : public cpu_layer {
public:
    cpu_paddle_deformable_convlayer() {}
    ~cpu_paddle_deformable_convlayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);

    void setParam(void *param, int psize);

protected:

    bool modulated_;                    
    int groups_;
    int deform_groups_;
    int kh_;
    int kw_;
    int pad_[2];
    int stride_[2];
    int dilation_[2];
    int im2col_step_;

};
} // namespace bmcpu
#endif  /* _CPU_PADDLE_YOLO_BOX_LAYER_H */
