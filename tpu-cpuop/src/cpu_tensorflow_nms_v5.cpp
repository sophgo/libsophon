#include "cpu_tensorflow_nms_v5.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <queue>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
inline float IOU(const float* box, const int i, const int j) {
    const float *box_i = box + i * 4;
    const float *box_j = box + j * 4;
    const float ymax_i = box_i[0] > box_i[2] ? box_i[0] : box_i[2];
    const float ymin_i = box_i[0] < box_i[2] ? box_i[0] : box_i[2];
    const float xmax_i = box_i[1] > box_i[3] ? box_i[1] : box_i[3];
    const float xmin_i = box_i[1] < box_i[3] ? box_i[1] : box_i[3];
    const float ymax_j = box_j[0] > box_j[2] ? box_j[0] : box_j[2];
    const float ymin_j = box_j[0] < box_j[2] ? box_j[0] : box_j[2];
    const float xmax_j = box_j[1] > box_j[3] ? box_j[1] : box_j[3];
    const float xmin_j = box_j[1] < box_j[3] ? box_j[1] : box_j[3];
    const float area_i = (ymax_i - ymin_i) * (xmax_i - xmin_i);
    if (area_i <= 0.f)
        return 0.f;
    const float area_j = (ymax_j - ymin_j) * (xmax_j - xmin_j);
    if (area_j <= 0.f)
        return 0.f;
    const float ymax_inter = ymax_i < ymax_j ? ymax_i : ymax_j;
    const float ymin_inter = ymin_i > ymin_j ? ymin_i : ymin_j;
    const float y_inter = ymax_inter > ymin_inter ? ymax_inter - ymin_inter : 0.f;
    if (y_inter == 0.f)
        return 0.f;
    const float xmax_inter = xmax_i < xmax_j ? xmax_i : xmax_j;
    const float xmin_inter = xmin_i > xmin_j ? xmin_i : xmin_j;
    const float x_inter = xmax_inter > xmin_inter ? xmax_inter - xmin_inter : 0.f;
    if (x_inter == 0.f)
        return 0.f;
    const float area_inter = y_inter * x_inter;
    const float iou = area_inter / (area_i + area_j - area_inter);
    return iou;
}
int cpu_tensorflow_nms_v5layer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_tensorflow_nms_v5_param_t, rip, param, psize);
    auto boxes = FLOAT_PTR(input_tensors_[0]);
    auto scores = FLOAT_PTR(input_tensors_[1]);
    int max_output_size = *INT_PTR(input_tensors_[2]);
    struct Candidate {
        int box_index;
        float score;
        int begin_index;
    };
    auto cmp = [](const Candidate i, const Candidate j) {
        return i.score < j.score;
    };
    std::priority_queue<Candidate, std::deque<Candidate>, decltype(cmp)>
    candicate_prior_queue(cmp);
    for (int i = 0; i < input_shapes_[0][0]; ++i)
        if (scores[i] > rip->score_threshold)
            candicate_prior_queue.emplace(Candidate({i, scores[i], 0}));
    float scale = 0.f;
    if (rip->soft_nms_sigma > 0.f)
        scale = -.5f / rip->soft_nms_sigma;
    auto suppress_weight = [&](const float iou) {
        if (iou <= rip->iou_threshold)
            return std::exp((scale * iou * iou));
        else
            return 0.f;
    };
    std::vector<int> selected_index;
    std::vector<float> selected_score;
    Candidate next_cand;
    float iou, original_score;
    while (selected_index.size() < max_output_size && (!candicate_prior_queue.empty())) {
        next_cand = candicate_prior_queue.top();
        original_score = next_cand.score;
        candicate_prior_queue.pop();
        bool should_hard_suppress = false;
        for (int i = INT(selected_index.size()) -1; i >= next_cand.begin_index ; --i) {
            iou = IOU(boxes, next_cand.box_index, selected_index[i]);
            next_cand.score *= suppress_weight(iou);
            if (iou >= rip->iou_threshold) {
                should_hard_suppress = true;
                break;
            }
            if (next_cand.score <= rip->score_threshold)
                break;
        }
        next_cand.begin_index = selected_index.size();
        if (should_hard_suppress == false) {
            if (next_cand.score == original_score) {
                selected_index.push_back(next_cand.box_index);
                selected_score.push_back(next_cand.score);
            }
            if (next_cand.score > rip->score_threshold)
                candicate_prior_queue.push(next_cand);
        }
    }
    int select_size = selected_index.size();
    memcpy(INT_PTR(output_tensors_[0]), selected_index.data(), select_size * sizeof(float));
    memcpy(INT_PTR(output_tensors_[1]), selected_score.data(), select_size * sizeof(float));
    if(rip->pad_to_max_output_size) {
        if(max_output_size > select_size){
            auto pad_byte_size = (max_output_size - select_size) * sizeof(float);
            memset(INT_PTR(output_tensors_[0]) + select_size, 0, pad_byte_size);
            memset(INT_PTR(output_tensors_[1]) + select_size, 0, pad_byte_size);
        }
        *output_shapes_ = {
            {INT(max_output_size)},
            {INT(max_output_size)}
        };
    } else {
        *output_shapes_ = {
            {INT(selected_index.size())},
            {INT(selected_index.size())}
        };
    }
    return 0;
}
int cpu_tensorflow_nms_v5layer::reshape(void *param, int psize,
                                        const std::vector<std::vector<int>> &input_shapes,
                                        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_tensorflow_nms_v5_param_t, rip, param, psize);
    output_shapes = {{rip->max_output_size}, {rip->max_output_size}};
    return 0;
}
int cpu_tensorflow_nms_v5layer::dtype(const void *param, size_t psize,
                                      const std::vector<int> &input_dtypes,
                                      std::vector<int> &output_dtypes) {
    output_dtypes = {CPU_DTYPE_INT32, input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_TENSORFLOW_NMS_V5, cpu_tensorflow_nms_v5);
}/* namespace bmcpu*/
