#include "cpu_generate_proposals.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <thread>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
typedef struct ProposalsForOneImageParam {
    const float *im_info;
    const float *anchors;
    const float *bbox_deltas;
    const float *scores;
    int post_nms_topN;
    int pre_nms_topN;
    float nms_thresh;
    float min_size;
    float feat_stride;
    int box_dim;
    bool legacy_plus_one;
    float clip_angle_thresh;
    float bbox_xform_clip;
    int A;
    int H;
    int W;
    std::vector<float> *out_probs;
    std::vector<float> *out_boxes;
    float img_idx;
} ProposalsForOneImageParam_t;
static inline void proposalsForOneImage(const ProposalsForOneImageParam_t *p) {
    const int K = p->H * p->W;
    int num_boxes = p->A * K;
    const float legacy_plus_one = FLOAT(p->legacy_plus_one);
    const float max_w = p->im_info[1] - legacy_plus_one;
    const float max_h = p->im_info[0] - legacy_plus_one;
    const float min_size = p->min_size * p->im_info[2];
    std::vector<int> order(num_boxes);
    std::iota(order.begin(), order.end(), 0);
    if (p->pre_nms_topN <= 0 || p->pre_nms_topN >= num_boxes) {
        std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
            return p->scores[lhs] > p->scores[rhs];
        });
    } else {
        std::partial_sort(order.begin(), order.begin() + p->pre_nms_topN, order.end(),
        [&](int lhs, int rhs) {
            return p->scores[lhs] > p->scores[rhs];
        });
        order.resize(p->pre_nms_topN);
    }
    num_boxes = order.size();
    float *scores_sorted = new float[num_boxes];
    float *proposals = new float[num_boxes * p->box_dim];
    std::vector<int> inds;
    for (size_t i = 0; i < order.size(); ++i)
        scores_sorted[i] = p->scores[order[i]];
    int num_thread = 1;
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    if (nt != nullptr)
        num_thread = atoi(nt);
    typedef struct ThreadParam {
        int start;
        int end;
        std::vector<int> inds;
    } ThreadParam_t;
    int opt = INT(order.size()) / num_thread;
    int start = 0;
    std::vector<ThreadParam_t> ps(num_thread);
    for (int i = 0; i < num_thread; ++i) {
        ps[i].start = start;
        int len = opt + (i < INT(order.size()) - opt * num_thread ? 1 : 0);
        ps[i].end = start + len;
        start = ps[i].end;
    }
    if (p->box_dim == 4) {
        auto stride = 4 * K;
        auto func = [&](ThreadParam_t *tp) {
            for (int i = tp->start; i < tp->end; ++i) {
                int a = order[i] / K;
                int x = order[i] - a * K;
                int h = x / p->W;
                int w = x - h * p->W;
                auto bbox = p->bbox_deltas + a * stride + x;
                auto anchor = p->anchors + a * p->box_dim;
                auto piter = proposals + i * p->box_dim;
                float W = anchor[2] - anchor[0] + legacy_plus_one;
                float W_half = W * .5f;
                float pred_ctr_x = bbox[0] * W + anchor[0] + FLOAT(w) * p->feat_stride + W_half;
                float pred_w_half = std::exp(std::min(bbox[K * 2], p->bbox_xform_clip)) * W_half;
                piter[0] = std::min(std::max(pred_ctr_x - pred_w_half, 0.f), max_w);
                piter[2] = std::min(std::max(pred_ctr_x + pred_w_half - legacy_plus_one, 0.f), max_w);
                float ws = piter[2] - piter[0] + legacy_plus_one;
                if (ws < min_size || piter[0] + ws * .5f >= FLOAT(p->im_info[1]))
                    continue;
                float H = anchor[3] - anchor[1] + legacy_plus_one;
                float H_half = H * .5f;
                float pred_ctr_y = bbox[K] * H + anchor[1] + FLOAT(h) * p->feat_stride + H_half;
                float pred_h_half = std::exp(std::min(bbox[K * 3], p->bbox_xform_clip)) * H_half;
                piter[1] = std::min(std::max(pred_ctr_y - pred_h_half, 0.f), max_h);
                piter[3] = std::min(std::max(pred_ctr_y + pred_h_half - legacy_plus_one, 0.f), max_h);
                float hs = piter[3] - piter[1] + legacy_plus_one;
                if (hs < min_size || piter[1] + hs * .5f >= FLOAT(p->im_info[0]))
                    continue;
                tp->inds.push_back(i);
            }
        };
        if (num_thread == 1)
            func(ps.data());
        else {
            std::vector<std::thread> threads;
            for (auto &it : ps)
                threads.push_back(std::thread(func, &it));
            for (auto &it : threads)
                it.join();
        }
        for (auto &it : ps)
            inds.insert(inds.end(), it.inds.begin(), it.inds.end());
    } else
        CPU_ASSERT(0);
    std::vector<int> keep;
    int post_nms_topN = p->post_nms_topN > 0 && p->post_nms_topN < inds.size() ?
                        p->post_nms_topN : -1;
    if (p->box_dim == 4) {
        std::vector<int> order = inds;
        float *areas = new float[num_boxes];
        const float *biter = proposals;
        for (int i = 0; i < num_boxes; ++i) {
            areas[i] = (biter[2] - biter[0] + legacy_plus_one) *
                       (biter[3] - biter[1] + legacy_plus_one);
            biter += 4;
        }
        while (order.size() > 0) {
            if (post_nms_topN >= 0 && keep.size() >= post_nms_topN)
                break;
            int i = order[0];
            keep.push_back(i);
            std::vector<int> rinds(order.begin() + 1, order.end());
            const float *ib = proposals + i * 4;
            std::vector<int> new_order;
            for (int o = 0; o < rinds.size(); ++o) {
                const float *biter = proposals + rinds[o] * 4;
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
                if (ovr <= p->nms_thresh)
                    new_order.push_back(order[o + 1]);
            }
            order = new_order;
        }
        delete [] areas;
    }
    p->out_probs->resize(keep.size());
    p->out_boxes->resize(keep.size() * 5);
    for (size_t i = 0; i < keep.size(); ++i) {
        p->out_probs->at(i) = scores_sorted[keep[i]];
        auto biter = proposals + keep[i] * p->box_dim;
        auto piter = p->out_boxes->data() + i * 5;
        piter[0] = p->img_idx;
        piter[1] = biter[0];
        piter[2] = biter[1];
        piter[3] = biter[2];
        piter[4] = biter[3];
    }
    delete [] scores_sorted;
    delete [] proposals;
}
int cpu_generate_proposalslayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_generate_proposals_param_t, rip, param, psize);
    const auto num_images = input_shapes_[0][0];
    const auto box_dim = input_shapes_[3][1];
    std::vector<std::vector<float>> im_boxes(num_images);
    std::vector<std::vector<float>> im_probs(num_images);
    for (int i = 0; i < num_images; i++) {
        const float BBOX_XFORM_CLIP_DEFAULT = log(1000.0 / 16.0);
        ProposalsForOneImageParam_t p;
        p.im_info = FLOAT_PTR(input_tensors_[2]) + i * input_shapes_[2][1];
        p.anchors = FLOAT_PTR(input_tensors_[3]);
        p.bbox_deltas = FLOAT_PTR(input_tensors_[1]) + i *
                        input_shapes_[1][1] *
                        input_shapes_[1][2] *
                        input_shapes_[1][3];
        p.scores = FLOAT_PTR(input_tensors_[0]) + i *
                   input_shapes_[0][1] *
                   input_shapes_[0][2] *
                   input_shapes_[0][3];
        p.post_nms_topN = rip->rpn_post_nms_topN;
        p.pre_nms_topN = rip->rpn_pre_nms_topN;
        p.nms_thresh = rip->rpn_nms_thresh;
        p.min_size = rip->rpn_min_size;
        p.box_dim = box_dim;
        p.A = input_shapes_[1][1] / box_dim;
        p.H = input_shapes_[1][2];
        p.W = input_shapes_[1][3];
        p.feat_stride = FLOAT(1. / rip->spatial_scale);
        p.legacy_plus_one = rip->legacy_plus_one;
        p.clip_angle_thresh = rip->clip_angle_thresh;
        p.bbox_xform_clip = BBOX_XFORM_CLIP_DEFAULT;
        p.out_probs = im_probs.data() + i;
        p.out_boxes = im_boxes.data() + i;
        p.img_idx = FLOAT(i);
        proposalsForOneImage(&p);
    }
    int roi_counts = 0;
    for (int i = 0; i < num_images; i++) {
        memcpy(FLOAT_PTR(output_tensors_[0]) + roi_counts * 5,
               im_boxes[i].data(), 5 * im_probs[i].size() * sizeof(float));
        memcpy(FLOAT_PTR(output_tensors_[1]) + roi_counts,
               im_probs[i].data(), im_probs[i].size() * sizeof(float));
        roi_counts += im_probs[i].size();
    }
    *output_shapes_ = {{roi_counts, 5}, {roi_counts}};
    return 0;
}
int cpu_generate_proposalslayer::reshape(void *param, int psize,
        const std::vector<std::vector<int>> &input_shapes,
        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_generate_proposals_param_t, rip, param, psize);
    auto num_boxes = input_shapes[0][1] * input_shapes[0][2] * input_shapes[0][3];
    if (rip->rpn_pre_nms_topN > 0 && rip->rpn_pre_nms_topN < num_boxes)
        num_boxes = rip->rpn_pre_nms_topN;
    if (rip->rpn_post_nms_topN > 0)
        num_boxes = num_boxes < rip->rpn_post_nms_topN ?
                    num_boxes : rip->rpn_post_nms_topN;
    output_shapes = {{num_boxes, 5}, {num_boxes}};
    return 0;
}
int cpu_generate_proposalslayer::dtype(const void *param, size_t psize,
                                       const std::vector<int> &input_dtypes,
                                       std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0], input_dtypes[1]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_GENERATE_PROPOSALS, cpu_generate_proposals);
}/* namespace bmcpu*/
