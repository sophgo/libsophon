#ifndef _CPU_PYTORCH_ROI_ALIGN_LAYER_H
#define _CPU_PYTORCH_ROI_ALIGN_LAYER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_pytorch_roi_alignlayer : public cpu_layer {
public:
    cpu_pytorch_roi_alignlayer() {}
    ~cpu_pytorch_roi_alignlayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);
};
} // namespace bmcpu
#endif  /* _CPU_PYTORCH_ROI_ALIGN_LAYER_H */
