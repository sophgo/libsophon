#ifndef _CPU_GENERATE_PROPOSALS_LAYER_H
#define _CPU_GENERATE_PROPOSALS_LAYER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_generate_proposalslayer : public cpu_layer {
public:
    cpu_generate_proposalslayer() {}
    ~cpu_generate_proposalslayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);
};
} // namespace bmcpu
#endif  /* _CPU_GENERATE_PROPOSALS_LAYER_H */
