#ifndef _CPU_PADDLE_BOX_CODER_LAYER_H
#define _CPU_PADDLE_BOX_CODER_LAYER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_paddle_box_coderlayer : public cpu_layer {
public:
    cpu_paddle_box_coderlayer() {}
    ~cpu_paddle_box_coderlayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);

    void setParam(void *param, int psize);

protected:
private:
    int axis_;
    bool box_normalized_;
    std::string code_type_;
};
} // namespace bmcpu
#endif  /* _CPU_PADDLE_BOX_CODER_LAYER_H */
