#include <string>
#include <queue>
#include <cmath>
#include "cpu_onnx_nms.h"
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

/*  input tensor: float box[batch_num, num_boxes, 4], float score[batch_num, num_calss, num_boxes]
 *                int max_output_boxes_per_class, iou_threshold, score_threshold
 *  output tensor: int [batch_index, class_index, box_index], len(indices) <= max_output_boxes_per_class*batch_num*num_class
 */
int cpu_onnx_nmslayer::process(void* param, int param_size) {
    setParam(param, param_size);
    const float* box = input_tensors_[0];
    const float* score = input_tensors_[1];
    const int num_boxes = input_shapes_[0][1];
    CPU_ASSERT(4 == input_shapes_[0][2]);
    CPU_ASSERT(num_boxes == input_shapes_[1][2]);
    const int batch_num = input_shapes_[1][0];
    const int num_class = input_shapes_[1][1];

    max_output_size = (input_tensors_.size() > 2) ? *(reinterpret_cast<int *>(input_tensors_[2])) : 0;
    max_output_size = ((max_output_size > num_boxes) || (max_output_size < 0)) ? num_boxes : max_output_size;
    iou_threshold = (input_tensors_.size() > 3) ? input_tensors_[3][0] : 0.f;
    score_threshold = (input_tensors_.size() > 4) ? input_tensors_[4][0] : 0.f;

    struct Candidate {
        int box_index;
        float score;
        int begin_index;
    };
    auto cmp = [](const Candidate i, const Candidate j) {
        return i.score < j.score;
    };

    int num_selected_indices = 0;
    for (int n = 0; n < batch_num; ++n) {
      for (int c = 0; c < num_class; ++c) {
        const int score_offset = (n * num_class + c) * num_boxes;
        const int box_offset = (n * num_boxes * 4);
        std::priority_queue<Candidate, std::deque<Candidate>, decltype(cmp)> candicate_prior_queue(cmp);
        for (int i = 0; i < num_boxes; ++i) {
            if (score[score_offset + i] > score_threshold) {
                candicate_prior_queue.emplace(Candidate({i, score[score_offset + i], 0}));
            }
        }

        std::vector<int> selected_index;
        Candidate next_cand;
        float iou;
        while (selected_index.size() < max_output_size && (!candicate_prior_queue.empty())) {
            next_cand = candicate_prior_queue.top();
            candicate_prior_queue.pop();

            bool selected = true;
            for (int i = static_cast<int>(selected_index.size()) -1;
                 i >= 0 ; --i) {
                iou = IOU((box + box_offset), next_cand.box_index, selected_index[i]);
                if ((iou > iou_threshold) && iou != 0.f) {
                    selected = false;
                    break;
                }
            }

            if (selected == true) {
                selected_index.push_back(next_cand.box_index);
            }
        }
        int *output = reinterpret_cast<int *> (output_tensors_[0]) + (num_selected_indices * 3);
        for (int i = 0; i < selected_index.size(); i++) {
            output[i * 3] = n;
            output[i * 3 + 1] = c;
            output[i * 3 + 2] = selected_index[i];
        }
        num_selected_indices += static_cast<int>(selected_index.size()) ;
      }
    }
    (*output_shapes_)[0][0] = num_selected_indices;
    (*output_shapes_)[0][1] = 3;

    return 0;

}

void cpu_onnx_nmslayer::setParam(void* param, int param_size)
{
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_onnx_nms_param_t, params, param, param_size);

    center_point_box       = params->center_point_box;
    max_output_size        = params->max_output_size;
//    center_point_box       = params->center_point_box;
//    center_point_box       = params->center_point_box;

    CPU_ASSERT(center_point_box == 0);
}

REGISTER_CPULAYER_CLASS(CPU_ONNX_NMS, cpu_onnx_nms);

} /* namespace bmcpu*/
