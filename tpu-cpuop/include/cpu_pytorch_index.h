#ifndef CPU_PYTORCH_INDEX_H
#define CPU_PYTORCH_INDEX_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_pytorch_indexlayer : public cpu_layer {
public:
    explicit cpu_pytorch_indexlayer() {}
    virtual ~cpu_pytorch_indexlayer() {}
    int process(void *param, int param_size) override;
    void setParam(void *param, int param_size);
    int reshape(void *param, int param_size,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes) override;
    int dtype(const void *param, size_t param_size,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes) override;
};
} /* namespace bmcpu */
#endif // CPU_PYTORCH_INDEX_H
