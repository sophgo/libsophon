#include "cpu_collect_rpn_proposals.h"
#include <algorithm>
#include <numeric>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
int cpu_collect_rpn_proposalslayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_collect_rpn_proposals_param_t, rip, param, psize);
    int num_rpn_lvls = rip->rpn_max_level - rip->rpn_min_level + 1;
    int proposal_num = 0;
    for (int i = 0; i < num_rpn_lvls; ++i)
        proposal_num += input_shapes_[i][0];
    float *scores = new float[proposal_num];
    float *rois = new float[proposal_num * 5];
    int len = 0;
    for (int i = 0; i < num_rpn_lvls; ++i) {
        memcpy(scores + len, FLOAT_PTR(input_tensors_[num_rpn_lvls + i]),
               input_shapes_[i][0] * sizeof(float));
        memcpy(rois + len * 5, FLOAT_PTR(input_tensors_[i]),
               input_shapes_[i][0] * 5 * sizeof(float));
        len += input_shapes_[i][0];
    }
    std::vector<int> idxs(len);
    std::iota(idxs.begin(), idxs.end(), 0);
    auto comp = [&](int lhs, int rhs) {
        if (scores[lhs] > scores[rhs])
            return true;
        if (scores[lhs] < scores[rhs])
            return false;
        return lhs < rhs;
    };
    int n = rip->rpn_post_nms_topN;
    if (n > 0 && n < len)
        std::nth_element(idxs.begin(), idxs.begin() + n, idxs.end(), comp);
    else
        n = len;
    std::sort(idxs.begin(), idxs.begin() + n, comp);
    float *riter = FLOAT_PTR(output_tensors_[0]);
    for (int i = 0; i < n; ++i) {
        const float *iter = rois + idxs[i] * 5;
        riter[0] = iter[0];
        riter[1] = iter[1];
        riter[2] = iter[2];
        riter[3] = iter[3];
        riter[4] = iter[4];
        riter += 5;
    }
    delete [] scores;
    delete [] rois;
    *output_shapes_ = {{n, 5}};
    return 0;
}
int cpu_collect_rpn_proposalslayer::reshape(void *param, int psize,
        const std::vector<std::vector<int>> &input_shapes,
        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_collect_rpn_proposals_param_t, rip, param, psize);
    int num_rpn_lvls = rip->rpn_max_level - rip->rpn_min_level + 1;
    int proposal_num = 0;
    for (int i = 0; i < num_rpn_lvls; ++i)
        proposal_num += input_shapes[i][0];
    int n = rip->rpn_post_nms_topN;
    if (n <= 0 || n >= proposal_num)
        n = proposal_num;
    output_shapes = {{n, 5}};
    return 0;
}
int cpu_collect_rpn_proposalslayer::dtype(const void *param, size_t psize,
        const std::vector<int> &input_dtypes,
        std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_COLLECT_RPN_PROPOSALS, cpu_collect_rpn_proposals);
}/* namespace bmcpu*/
