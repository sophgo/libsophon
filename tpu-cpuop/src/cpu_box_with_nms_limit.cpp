#include "cpu_box_with_nms_limit.h"
#include <algorithm>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
typedef struct BoxWithNMSLimitParam {
    const float *scores;
    int num_scores;
    int scores_stride;
    float score_thres;
    const float *boxes;
    int num_boxes;
    int box_dim;
    int boxes_stride;
    bool soft_nms_enabled;
    int detections_per_im;
    std::vector<int> *keep;
    float nms_thres;
    bool legacy_plus_one;
} BoxWithNMSLimitParam_t;
typedef struct WriteOutputsParam {
    const float *scores;
    int scores_stride;
    const float *boxes;
    int boxes_stride;
    int box_dim;
    int class_j;
    bool output_classes_include_bg_cls;
    const std::vector<int> *keep;
    float *out_scores;
    float *out_boxes;
    float *out_classes;
} WriteOutputsParam_t;
static inline void boxWithNMSLimit(const BoxWithNMSLimitParam_t *p) {
    const int legacy_plus_one = INT(p->legacy_plus_one);
    std::vector<int> inds;
    auto siter = p->scores;
    for (int i = 0; i < p->num_scores; ++i) {
        if (*siter > p->score_thres)
            inds.push_back(i);
        siter += p->scores_stride;
    }
    if (p->soft_nms_enabled)
        CPU_ASSERT(!p->soft_nms_enabled);
    else {
        std::sort(inds.data(), inds.data() + inds.size(),
        [&](int lhs, int rhs) {
            return p->scores[lhs * p->scores_stride] >
                   p->scores[rhs * p->scores_stride];
        });
        int keep_max = p->detections_per_im > 0 ? p->detections_per_im : -1;
        if (p->box_dim == 4) {
            std::vector<int> order = inds;
            float *areas = new float[p->num_boxes];
            const float *biter = p->boxes;
            for (int i = 0; i < p->num_boxes; ++i) {
                areas[i] = (biter[2] - biter[0] + legacy_plus_one) *
                           (biter[3] - biter[1] + legacy_plus_one);
                biter += p->boxes_stride;
            }
            while (order.size() > 0) {
                if (keep_max >= 0 && p->keep->size() >= keep_max)
                    break;
                int i = order[0];
                p->keep->push_back(i);
                std::vector<int> rinds(order.begin() + 1, order.end());
                const float *ib = p->boxes + i * p->boxes_stride;
                std::vector<int> new_order;
                for (int o = 0; o < rinds.size(); ++o) {
                    const float *biter = p->boxes + rinds[o] * p->boxes_stride;
                    float w = std::max(std::min(biter[2], ib[2]) -
                                       std::max(biter[0], ib[0]) + legacy_plus_one, 0.f);
                    if (w == 0.f) {
                        new_order.push_back(order[o + 1]);
                        continue;
                    }
                    float h = std::max(std::min(biter[3], ib[3]) -
                                       std::max(biter[1], ib[1]) + legacy_plus_one, 0.f);
                    if (h == 0.f) {
                        new_order.push_back(order[o + 1]);
                        continue;
                    }
                    float inter = w * h;
                    float ovr = inter / (areas[i] + areas[rinds[o]] - inter);
                    if (ovr <= p->nms_thres)
                        new_order.push_back(order[o + 1]);
                }
                order = new_order;
            }
            delete [] areas;
        } else
            CPU_ASSERT(p->box_dim == 4);
    }
}
static inline void writeOutputs(const WriteOutputsParam_t *p) {
    for (int i = 0; i < p->keep->size(); ++i) {
        p->out_scores[i] = p->scores[p->keep->at(i) * p->scores_stride];
        auto out_box = p->out_boxes + i * p->box_dim;
        auto box = p->boxes + p->keep->at(i) * p->boxes_stride;
        for (int d = 0; d < p->box_dim; ++d)
            out_box[d] = box[d];
        p->out_classes[i] = FLOAT(p->class_j - INT(!p->output_classes_include_bg_cls));
    }
}
int cpu_box_with_nms_limitlayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_box_with_nms_limit_param_t, rip, param, psize);
    this->cls_agnostic_bbox_reg_ = rip->cls_agnostic_bbox_reg;
    this->input_boxes_include_bg_cls_ = rip->input_boxes_include_bg_cls;
    this->input_scores_fg_cls_starting_id_ = INT(rip->input_boxes_include_bg_cls);
    const int box_dim = rip->rotated ? 5 : 4;
    int num_classes = input_shapes_[0][1];
    int batch_size = 1;
    std::vector<float> batch_splits_default(1, input_shapes_[0][0]);
    const float *batch_splits_data = batch_splits_default.data();
    if (input_tensors_.size() > 2) {
        batch_size = input_shapes_[2][0];
        batch_splits_data = FLOAT_PTR(input_tensors_[2]);
    }
    std::vector<int> total_keep_per_batch(batch_size);
    int offset = 0;
    int start_idx = 0;
    for (int b = 0; b < batch_size; ++b) {
        int num_boxes = batch_splits_data[b];
        const float *scores = FLOAT_PTR(input_tensors_[0]) + offset * input_shapes_[0][1];
        const float *boxes = FLOAT_PTR(input_tensors_[1]) + offset * input_shapes_[1][1];
        std::vector<std::vector<int>> keeps(num_classes);
        int total_keep_count = 0;
        for (int j = 1; j < num_classes; ++j) {
            BoxWithNMSLimitParam_t p;
            p.scores = scores + this->get_score_cls_index(j);
            p.num_scores = input_shapes_[0][0];
            p.scores_stride = input_shapes_[0][1];
            p.score_thres = rip->score_thresh;
            p.num_boxes = num_boxes;
            p.box_dim = box_dim;
            p.boxes_stride = input_shapes_[1][1];
            p.boxes = boxes + this->get_box_cls_index(j) * box_dim;
            p.soft_nms_enabled = rip->soft_nms_enabled;
            p.detections_per_im = rip->detections_per_im;
            p.nms_thres = rip->nms;
            p.legacy_plus_one = rip->legacy_plus_one;
            p.keep = &keeps[j];
            boxWithNMSLimit(&p);
            total_keep_count += p.keep->size();
        }
        if (rip->soft_nms_enabled)
            CPU_ASSERT(!rip->soft_nms_enabled);
        if (rip->detections_per_im > 0 && total_keep_count > rip->detections_per_im) {
            auto get_all_scores_sorted = [&]() {
                std::vector<std::pair<int, int>> ret(total_keep_count);
                int ret_idx = 0;
                for (int j = 1; j < num_classes; ++j) {
                    auto &keep = keeps[j];
                    for (auto &ckv : keep) {
                        ret[ret_idx] = {j, ckv};
                        ++ret_idx;
                    }
                }
                std::sort(ret.data(), ret.data() + ret.size(), [&](
                const std::pair<int, int> &lhs, const std::pair<int, int> &rhs) {
                    return scores[
                               lhs.second * input_shapes_[0][1] +
                               this->get_score_cls_index(lhs.first)] >
                           scores[
                               rhs.second * input_shapes_[0][1] +
                               this->get_score_cls_index(rhs.first)];
                });
                return ret;
            };
            auto all_scores_sorted = get_all_scores_sorted();
            for (auto &keep : keeps)
                keep.clear();
            for (int i = 0; i < rip->detections_per_im; ++i) {
                auto &cur = all_scores_sorted[i];
                keeps[cur.first].push_back(cur.second);
            }
            total_keep_count = rip->detections_per_im;
        }
        total_keep_per_batch[b] = total_keep_count;
        int out_idx = 0;
        for (int j = 1; j < num_classes; ++j) {
            WriteOutputsParam_t p;
            p.keep = &keeps[j];
            p.scores = scores + this->get_score_cls_index(j);
            p.boxes = boxes + this->get_box_cls_index(j)  * box_dim;
            p.scores_stride = input_shapes_[0][1];
            p.boxes_stride = input_shapes_[1][1];
            p.box_dim = box_dim;
            p.class_j = j;
            p.output_classes_include_bg_cls = rip->output_classes_include_bg_cls;
            int cur_idx = start_idx + out_idx;
            p.out_scores = FLOAT_PTR(output_tensors_[0]) + cur_idx;
            p.out_boxes = FLOAT_PTR(output_tensors_[1]) + cur_idx * box_dim;
            p.out_classes = FLOAT_PTR(output_tensors_[2]) + cur_idx;
            writeOutputs(&p);
            out_idx += p.keep->size();
        }
        auto out_keeps = INT_PTR(output_tensors_[4]) + start_idx;
        auto out_keeps_size = INT_PTR(output_tensors_[5]) + b * num_classes;
        for (int j = 0; j < num_classes; ++j) {
            for (int k = 0; k < keeps[j].size(); ++k)
                out_keeps[k] = keeps[j][k];
            out_keeps_size[j] = keeps[j].size();
            out_keeps += keeps[j].size();
        }
        offset += num_boxes;
        start_idx += total_keep_count;
    }
    for (int i = 0; i < total_keep_per_batch.size(); ++i)
        FLOAT_PTR(output_tensors_[3])[i] = FLOAT(total_keep_per_batch[i]);
    *output_shapes_ = {
        {start_idx},
        {start_idx, box_dim},
        {start_idx},
        {batch_size},
        {start_idx},
        {batch_size, num_classes}
    };
    return 0;
}
int cpu_box_with_nms_limitlayer::reshape(void *param, int psize,
        const std::vector<std::vector<int>> &input_shapes,
        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_box_with_nms_limit_param_t, rip, param, psize);
    int box_dim = rip->rotated ? 5 : 4;
    int batch_size = input_shapes[2][0];
    int total = rip->detections_per_im * batch_size;
    int num_classes = input_shapes[0][1];
    output_shapes = {
        {total},
        {total, box_dim},
        {total},
        {batch_size},
        {total},
        {batch_size, num_classes}
    };
    return 0;
}
int cpu_box_with_nms_limitlayer::dtype(const void *param, size_t psize,
                                       const std::vector<int> &input_dtypes,
                                       std::vector<int> &output_dtypes) {
    output_dtypes = {
        input_dtypes[0], input_dtypes[0], input_dtypes[0],
        input_dtypes[0], CPU_DTYPE_INT32, CPU_DTYPE_INT32
    };
    return 0;
}
int cpu_box_with_nms_limitlayer::get_box_cls_index(int bg_fg_cls_id) {
    if (this->cls_agnostic_bbox_reg_)
        return 0;
    else if (!this->input_boxes_include_bg_cls_)
        return bg_fg_cls_id - 1;
    else
        return bg_fg_cls_id;
}
int cpu_box_with_nms_limitlayer::get_score_cls_index(int bg_fg_cls_id) {
    return bg_fg_cls_id - 1 + this->input_scores_fg_cls_starting_id_;
}
REGISTER_CPULAYER_CLASS(CPU_BOX_WITH_NMS_LIMIT, cpu_box_with_nms_limit);
} // namespace bmcpu
