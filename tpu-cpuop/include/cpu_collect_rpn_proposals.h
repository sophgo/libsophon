#ifndef _CPU_COLLECT_RPN_PROPOSALS_LAYER_H
#define _CPU_COLLECT_RPN_PROPOSALS_LAYER_H
#include "cpu_layer.h"
namespace bmcpu {
class cpu_collect_rpn_proposalslayer : public cpu_layer {
public:
    cpu_collect_rpn_proposalslayer() {}
    ~cpu_collect_rpn_proposalslayer() {}
    int process(void *param, int psize);
    int reshape(void* param, int psize,
                const std::vector<std::vector<int>> &input_shapes,
                std::vector<std::vector<int>> &output_shapes);
    int dtype(const void *param, size_t psize,
              const std::vector<int> &input_dtypes,
              std::vector<int> &output_dtypes);
};
} // namespace bmcpu
#endif  /* _CPU_COLLECT_RPN_PROPOSALS_LAYER_H */
