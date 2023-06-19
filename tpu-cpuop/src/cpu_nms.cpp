#include <string>
#include <queue>
#include <cmath>
#include "cpu_nms.h"
#include "cpu_layer.h"

namespace bmcpu {

inline float IOU(const float* box, const int i, const int j)
{
    //box:[y1, x1, y2, x2]
    const float* box_i = box + i * 4;
    const float* box_j = box + j * 4;
    const float ymax_i = (box_i[0] > box_i[2]) ? box_i[0] : box_i[2];
    const float ymin_i = (box_i[0] < box_i[2]) ? box_i[0] : box_i[2];
    const float xmax_i = (box_i[1] > box_i[3]) ? box_i[1] : box_i[3];
    const float xmin_i = (box_i[1] < box_i[3]) ? box_i[1] : box_i[3];
    const float ymax_j = (box_j[0] > box_j[2]) ? box_j[0] : box_j[2];
    const float ymin_j = (box_j[0] < box_j[2]) ? box_j[0] : box_j[2];
    const float xmax_j = (box_j[1] > box_j[3]) ? box_j[1] : box_j[3];
    const float xmin_j = (box_j[1] < box_j[3]) ? box_j[1] : box_j[3];
    const float area_i = (ymax_i - ymin_i) * (xmax_i - xmin_i);
    if (area_i <= 0.f)
        return 0.f;
    const float area_j = (ymax_j - ymin_j) * (xmax_j - xmin_j);
    if (area_j <= 0.f)
        return 0.f;
    const float ymax_inter = (ymax_i < ymax_j) ? ymax_i : ymax_j;
    const float ymin_inter = (ymin_i > ymin_j) ? ymin_i : ymin_j;
    const float y_inter = (ymax_inter > ymin_inter) ? (ymax_inter - ymin_inter) : 0;
    if (y_inter == 0.f)
        return 0.f;
    const float xmax_inter = (xmax_i < xmax_j) ? xmax_i : xmax_j;
    const float xmin_inter = (xmin_i > xmin_j) ? xmin_i : xmin_j;
    const float x_inter = (xmax_inter > xmin_inter) ? (xmax_inter - xmin_inter) : 0;
    if (x_inter == 0.f)
        return 0.f;
    const float area_inter = y_inter * x_inter;
    const float iou = area_inter / (area_i + area_j - area_inter);
    return iou;
}

/*  input tensor: float box[num_boxes, 4], float score[num_boxes]
 *  output tensor: int indices[M], M <= max_output_size
 */
int cpu_nmslayer::process(void* param, int param_size) {
    setParam(param, param_size);
    const float* box = input_tensors_[0];
    const float* score = input_tensors_[1];
    const int num_boxes = input_shapes_[0][0];
    CPU_ASSERT(4 == input_shapes_[0][1]);
    CPU_ASSERT(num_boxes == input_shapes_[1][0]);

    if (input_tensors_.size() > 2)
        max_output_size = *(reinterpret_cast<int *>(input_tensors_[2]));
    else
        max_output_size = num_boxes; /* workaround for bmlang nms */

    struct Candidate {
        int box_index;
        float score;
        int begin_index;
    };
    auto cmp = [](const Candidate i, const Candidate j) {
        return i.score < j.score;
    };
    std::priority_queue<Candidate, std::deque<Candidate>, decltype(cmp)> candicate_prior_queue(cmp);
    for (int i = 0; i < num_boxes; ++i) {
        if (score[i] > score_threshold) {
            candicate_prior_queue.emplace(Candidate({i, score[i], 0}));
        }
    }

    float scale = 0.f;
//    float soft_nms_sigma = 0.f;     //for NonMaxSuppressionV5
//    if (soft_nms_sigma > 0.f) {
//        scale = (-0.5f) / soft_nms_sigma;
//    }
    auto suppress_weight = [scale](const float iou, const float iou_threshold) {
        if (iou <= iou_threshold)
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
        for (int i = static_cast<int>(selected_index.size()) -1;
             i >= next_cand.begin_index ; --i) {
            iou = IOU(box, next_cand.box_index, selected_index[i]);

            next_cand.score *= suppress_weight(iou, iou_threshold);

            if (iou >= iou_threshold) {
                should_hard_suppress = true;
                break;
            }
            if (next_cand.score <= score_threshold) break;
        }
        next_cand.begin_index = selected_index.size();

        if (should_hard_suppress == false) {
            if (next_cand.score == original_score) {
                selected_index.push_back(next_cand.box_index);
                selected_score.push_back(next_cand.score);
            }
            if (next_cand.score > score_threshold) {
                candicate_prior_queue.push(next_cand);
            }
        }
    }

    (*output_shapes_)[0] = {static_cast<int>(selected_index.size())};
    int *output = reinterpret_cast<int *> (output_tensors_[0]);
    for (int i = 0; i < selected_index.size(); i++) {
        output[i] = selected_index[i];
    }

    return 0;

}

void cpu_nmslayer::setParam(void* param, int param_size)
{
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_nms_t, params, param, param_size);

    iou_threshold       = params->iou_threshold;
    score_threshold     = params->score_threshold;

    CPU_ASSERT((iou_threshold >= 0) && (iou_threshold <= 1));
}

REGISTER_CPULAYER_CLASS(CPU_NON_MAX_SUPPRESSION, cpu_nms);

} /* namespace bmcpu*/
