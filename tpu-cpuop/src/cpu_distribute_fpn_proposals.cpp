#include "cpu_distribute_fpn_proposals.h"
#include <algorithm>
#include <numeric>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
int cpu_distribute_fpn_proposalslayer::process(void *param, int psize) {
    CPU_ASSERT(false);
    return 0;
}
int cpu_distribute_fpn_proposalslayer::reshape(void *param, int psize,
        const std::vector<std::vector<int>> &input_shapes,
        std::vector<std::vector<int>> &output_shapes) {
    return 1;
}
int cpu_distribute_fpn_proposalslayer::dtype(const void *param, size_t psize,
        const std::vector<int> &input_dtypes,
        std::vector<int> &output_dtypes) {
    return 1;
}
REGISTER_CPULAYER_CLASS(CPU_DISTRIBUTE_FPN_PROPOSALS, cpu_distribute_fpn_proposals);
}/* namespace bmcpu*/

