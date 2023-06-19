#include "cpu_bbox_transform.h"
#include <cmath>
#include <thread>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
int cpu_bbox_transformlayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_bbox_transform_param_t, rip, param, psize);
    const auto roi_in = FLOAT_PTR(input_tensors_[0]);
    const auto iminfo_in = FLOAT_PTR(input_tensors_[2]);
    const int box_dim = rip->rotated ? 5 : 4;
    CPU_ASSERT(box_dim == 4);
    const int num_classes = input_shapes_[1][1] / box_dim;
    const int batch_size = input_shapes_[2][0];
    std::vector<int> num_rois_per_batch(batch_size, 0);
    if (input_shapes_[0][1] == box_dim)
        num_rois_per_batch[0] = input_shapes_[0][0];
    else {
        for (int i = 0; i < input_shapes_[0][0]; ++i) {
            const int roi_batch_id = roi_in[i * input_shapes_[0][1]];
            ++num_rois_per_batch[roi_batch_id];
        }
    }
    int offset = 0;
    for (int i = 0; i < batch_size; ++i) {
        const int num_rois = num_rois_per_batch[i];
        const auto cur_iminfo = iminfo_in + i * input_shapes_[2][1];
        const float scale_before = cur_iminfo[2];
        const float scale_after = rip->apply_scale ? cur_iminfo[2] : 1.f;
        int img_h = INT(cur_iminfo[0] / scale_before + 0.5);
        int img_w = INT(cur_iminfo[1] / scale_before + 0.5);
        float max_w = FLOAT(img_w - INT(rip->legacy_plus_one));
        float max_h = FLOAT(img_h - INT(rip->legacy_plus_one));
        float *cur_boxes = new float[num_rois * box_dim];
        const float *cur_boxes_src = roi_in + input_shapes_[0][1] - box_dim +
                                     offset * input_shapes_[0][1];
        float *cur_boxes_dst = cur_boxes;
        for (int p = 0; p < num_rois; ++p) {
            for (int q = 0; q < box_dim; ++q) {
                *cur_boxes_dst = cur_boxes_src[q];
                ++cur_boxes_dst;
            }
            cur_boxes_src += input_shapes_[0][1];
        }
        const float scale_before_inv = 1.0 / scale_before;
        float *cur_boxes_iter = cur_boxes;
        if (box_dim == 4) {
            for (int i = 0; i < num_rois * box_dim; ++i) {
                *cur_boxes_iter *= scale_before_inv;
                ++cur_boxes_iter;
            }
        } else {
            for (int p = 0; p < num_rois; ++p) {
                for (int q = 0; q < 4; ++q) {
                    *cur_boxes_iter *= scale_before_inv;
                    ++cur_boxes_iter;
                }
                ++cur_boxes_iter;
            }
        }
        const float BBOX_XFORM_CLIP_DEFAULT = log(1000.0 / 16.0);
        const float weights[4] = {
            FLOAT(1. / rip->weights[0]),
            FLOAT(1. / rip->weights[1]),
            FLOAT(1. / rip->weights[2]),
            FLOAT(1. / rip->weights[3])
        };
        int num_thread = 1;
        char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
        if (nt != nullptr)
            num_thread = atoi(nt);
        typedef struct ThreadParam {
            int start;
            int end;
        } ThreadParam_t;
        int opt = num_classes / num_thread;
        int start = 0;
        std::vector<ThreadParam_t> ps(num_thread);
        for (int i = 0; i < num_thread; ++i) {
            ps[i].start = start;
            int len = opt + (i < num_classes - opt * num_thread ? 1 : 0);
            ps[i].end = start + len;
            start = ps[i].end;
        }
        auto func = [&](const ThreadParam_t *tp) {
            for (int k = tp->start; k < tp->end; ++k) {
                for (int i = 0; i < num_rois; ++i) {
                    float *pbiter = FLOAT_PTR(output_tensors_[0]) +
                                    (offset + i) * input_shapes_[1][1] + k * box_dim ;
                    const float *biter = cur_boxes + i * box_dim;
                    const float *diter = FLOAT_PTR(input_tensors_[1]) +
                                         (offset + i) * input_shapes_[1][1] + k * box_dim;
                    float W = biter[2] - biter[0] + FLOAT(rip->legacy_plus_one);
                    float H = biter[3] - biter[1] + FLOAT(rip->legacy_plus_one);
                    float W_half = W * .5f;
                    float H_half = H * .5f;
                    float pred_ctr_x = diter[0] * weights[0] * W + biter[0] + W_half;
                    float pred_ctr_y = diter[1] * weights[1] * H + biter[1] + H_half;
                    float pred_w_half = std::exp(std::min(diter[2] * weights[2], BBOX_XFORM_CLIP_DEFAULT)) * W_half;
                    float pred_h_half = std::exp(std::min(diter[3] * weights[3], BBOX_XFORM_CLIP_DEFAULT)) * H_half;
                    pbiter[0] = std::min(std::max(pred_ctr_x - pred_w_half, 0.f), max_w) * scale_after;
                    pbiter[1] = std::min(std::max(pred_ctr_y - pred_h_half, 0.f), max_h) * scale_after;
                    pbiter[2] = std::min(std::max(pred_ctr_x + pred_w_half - FLOAT(rip->legacy_plus_one), 0.f), max_w) * scale_after;
                    pbiter[3] = std::min(std::max(pred_ctr_y + pred_h_half - FLOAT(rip->legacy_plus_one), 0.f), max_h) * scale_after;
                }
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
        offset += num_rois;
        delete [] cur_boxes;
    }
    for (int i = 0; i < batch_size; ++i)
        FLOAT_PTR(output_tensors_[1])[i] = FLOAT(num_rois_per_batch[i]);
    *output_shapes_ = {{input_shapes_[0][0], input_shapes_[1][1]}, {batch_size}};
    return 0;
}
int cpu_bbox_transformlayer::reshape(void *param, int psize,
                                     const std::vector<std::vector<int>> &input_shapes,
                                     std::vector<std::vector<int>> &output_shapes) {
    output_shapes = {
        {input_shapes[0][0], input_shapes[1][1]},
        {input_shapes[2][0]}
    };
    return 0;
}
int cpu_bbox_transformlayer::dtype(const void *param, size_t psize,
                                   const std::vector<int> &input_dtypes,
                                   std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0], input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_BBOX_TRANSFORM, cpu_bbox_transform);
} // namespace bmcpu
