#include "cpu_distribute_fpn_proposals_roi_align_concat.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <thread>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
struct PreCalc {
    int pos1, pos2, pos3, pos4;
    float w1, w2, w3, w4;
};
static inline void preCalcForBilinearInterpolate(
    const int ih,
    const int iw,
    const int pooled_h,
    const int pooled_w,
    const int iy_upper,
    const int ix_upper,
    float start_h,
    float start_w,
    float bin_h,
    float bin_w,
    int grid_h,
    int grid_w,
    std::vector<PreCalc> &pre_calc) {
    PreCalc *p = pre_calc.data();
    float bin_h_div_grid_h = bin_h / FLOAT(grid_h);
    float bin_w_div_grid_w = bin_w / FLOAT(grid_w);
    for (int ph = 0; ph < pooled_h; ++ph) {
        for (int pw = 0; pw < pooled_w; ++pw) {
            float yiter = start_h + FLOAT(ph) * bin_h + .5f * bin_h_div_grid_h;
            for (int iy = 0; iy < iy_upper; ++iy) {
                float y = yiter;
                if (y < -1.f || y > FLOAT(ih)) {
                    memset(p, 0x0, sizeof(PreCalc) * ix_upper);
                    p += ix_upper;
                } else {
                    y = std::min(std::max(y, 0.f), FLOAT(ih - 1));
                    int yl = INT(std::floor(y));
                    int yh = std::min(yl + 1, ih - 1);
                    float ly = y - FLOAT(yl);
                    float hy = 1.f - ly;
                    int yl_x_iw = yl * iw;
                    int yh_x_iw = yh * iw;
                    float xiter = start_w + FLOAT(pw) * bin_w + .5f * bin_w_div_grid_w;
                    for (int ix = 0; ix < ix_upper; ++ix) {
                        float x = xiter;
                        if (x < -1.f || x > FLOAT(iw))
                            *p = {0, 0, 0, 0, 0.f, 0.f, 0.f, 0.f};
                        else {
                            x = std::min(std::max(x, 0.f), FLOAT(iw - 1));
                            int xl = INT(std::floor(x));
                            int xh = std::min(xl + 1, iw - 1);
                            float lx = x - FLOAT(xl);
                            float hx = 1.f - lx;
                            *p = {
                                yl_x_iw + xl,
                                yl_x_iw + xh,
                                yh_x_iw + xl,
                                yh_x_iw + xh,
                                hy * hx,
                                hy * lx,
                                ly * hx,
                                ly * lx
                            };
                        }
                        ++p;
                        xiter += bin_w_div_grid_w;
                    }
                }
                yiter += bin_h_div_grid_h;
            }
        }
    }
}
int cpu_distribute_fpn_proposals_roi_align_concatlayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_distribute_fpn_proposals_roi_align_concat_param_t, rip, param, psize);
    auto dfp = rip->dfp;
    for (int i = 1; i < 4; ++i) {
        CPU_ASSERT(rip->ra[0].pooled_height = rip->ra[i].pooled_height);
        CPU_ASSERT(rip->ra[0].pooled_width = rip->ra[i].pooled_width);
        CPU_ASSERT(input_shapes_[1][1] == input_shapes_[i + 1][1]);
    }
    const int channels = input_shapes_[1][1];
    const int num_rois = input_shapes_[0][0];
    const int dim_rois = input_shapes_[0][1];
    const auto s0_inv = FLOAT(1. / FLOAT(dfp.roi_canonical_scale));
    const auto log2_inv = FLOAT(1. / std::log(2.));
    const int pooled_h = rip->ra[0].pooled_height;
    const int pooled_w = rip->ra[0].pooled_width;
    const float pooled_h_inv = FLOAT(1. / pooled_h);
    const float pooled_w_inv = FLOAT(1. / pooled_w);
    int num_thread = 1;
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    if (nt != nullptr)
        num_thread = atoi(nt);
    typedef struct ThreadParam {
        int start;
        int end;
        float *outs[4];
        int *inds[4];
        int cnts[4];
    } ThreadParam_t;
    int opt = num_rois / num_thread;
    int start = 0;
    std::vector<ThreadParam_t> ps(num_thread);
    for (int i = 0; i < num_thread; ++i) {
        ps[i].start = start;
        int len = opt + (i < num_rois - opt * num_thread ? 1 : 0);
        ps[i].end = start + len;
        start = ps[i].end;
        for (int j = 0; j < 4; ++j) {
            ps[i].outs[j] = new float[len * channels * pooled_h * pooled_w];
            ps[i].inds[j] = new int[len];
            ps[i].cnts[j] = 0;
        }
    }
    auto func = [&](ThreadParam_t *tp) {
        for (int i = tp->start; i < tp->end; ++i) {
            auto riter = FLOAT_PTR(input_tensors_[0]) + i * dim_rois;
            int batch_idx = 0;
            if (dim_rois == 5) {
                batch_idx = INT(riter[0]);
                ++riter;
            }
            const auto w = riter[2] - riter[0] + FLOAT(dfp.legacy_plus_one);
            const auto h = riter[3] - riter[1] + FLOAT(dfp.legacy_plus_one);
            const auto s = sqrtf(w * h);
            int tl = INT(std::floor(FLOAT(dfp.roi_canonical_level) + std::log(s * s0_inv + 1e-6f) * log2_inv));
            tl = std::max(dfp.roi_min_level, std::min(dfp.roi_max_level, tl));
            const int idx = tl - dfp.roi_min_level;
            const auto ra = rip->ra[idx];
            const float roi_offset = ra.align ? .5f : 0.f;
            float start_w = riter[0] * ra.spatial_scale - roi_offset;
            float start_h = riter[1] * ra.spatial_scale - roi_offset;
            float end_w = riter[2] * ra.spatial_scale - roi_offset;
            float end_h = riter[3] * ra.spatial_scale - roi_offset;
            float roi_w = ra.align ? std::max(end_w - start_w, 1.f) : end_w - start_w;
            float roi_h = ra.align ? std::max(end_h - start_h, 1.f) : end_h - start_h;
            int grid_w = ra.sampling_ratio > 0.f ? ra.sampling_ratio
                         : ceil(roi_w * pooled_w_inv);
            int grid_h = ra.sampling_ratio > 0.f ? ra.sampling_ratio
                         : ceil(roi_h * pooled_h_inv);
            const float avg_scaling = FLOAT(1. / FLOAT(grid_h * grid_w));
            std::vector<PreCalc> pre_calc(grid_h * grid_w * pooled_h * pooled_w);
            int ih = input_shapes_[idx + 1][2];
            int iw = input_shapes_[idx + 1][3];
            preCalcForBilinearInterpolate(ih,
                                          iw,
                                          pooled_h,
                                          pooled_w,
                                          grid_h,
                                          grid_w,
                                          start_h,
                                          start_w,
                                          roi_h * pooled_h_inv,
                                          roi_w * pooled_w_inv,
                                          grid_h,
                                          grid_w,
                                          pre_calc);
            float *out = tp->outs[idx] + tp->cnts[idx] * channels * pooled_h * pooled_w;
            for (int c = 0; c < channels; ++c) {
                const float *idata = FLOAT_PTR(input_tensors_[idx + 1]) +
                                     (batch_idx * channels + c) * ih * iw;
                int pre_calc_index = 0;
                for (int ph = 0; ph < pooled_h; ++ph) {
                    for (int pw = 0; pw < pooled_w; ++pw) {
                        float val = 0.f;
                        for (int iy = 0; iy < grid_h; ++iy) {
                            for (int ix = 0; ix < grid_w; ++ix) {
                                PreCalc pc = pre_calc[pre_calc_index];
                                val += pc.w1 * idata[pc.pos1] +
                                       pc.w2 * idata[pc.pos2] +
                                       pc.w3 * idata[pc.pos3] +
                                       pc.w4 * idata[pc.pos4];
                                ++pre_calc_index;
                            }
                        }
                        *out = val * avg_scaling;
                        ++out;
                    }
                }
            }
            tp->inds[idx][tp->cnts[idx]] = i;
            ++tp->cnts[idx];
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
    int total = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < num_thread; ++j) {
            if (ps[j].cnts[i] == 0)
                continue;
            memcpy(FLOAT_PTR(output_tensors_[0]) + total * channels * pooled_h * pooled_w,
                   ps[j].outs[i], sizeof(float) * ps[j].cnts[i] * channels * pooled_h * pooled_w);
            memcpy(INT_PTR(output_tensors_[1]) + total, ps[j].inds[i], sizeof(int) * ps[j].cnts[i]);
            total += ps[j].cnts[i];
        }
    }
    CPU_ASSERT(num_rois == total);
    std::vector<int> idxs(num_rois);
    std::iota(std::begin(idxs), std::end(idxs), 0);
    std::sort(idxs.begin(), idxs.end(), [&](int lhs, int rhs) {
        return INT_PTR(output_tensors_[1])[lhs] < INT_PTR(output_tensors_[1])[rhs];
    });
    memcpy(INT_PTR(output_tensors_[1]), idxs.data(), sizeof(int) * num_rois);
    *output_shapes_ = {{total, channels, pooled_h, pooled_w}, {total}};
    for (int i = 0; i < num_thread; ++i) {
        for (int j = 0; j < 4; ++j) {
            delete [] ps[i].outs[j];
            delete [] ps[i].inds[j];
        }
    }
    return 0;
}
int cpu_distribute_fpn_proposals_roi_align_concatlayer::reshape(void *param, int psize,
        const std::vector<std::vector<int>> &input_shapes,
        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_distribute_fpn_proposals_roi_align_concat_param_t, rip, param, psize);
    output_shapes = {{
            input_shapes[0][0], input_shapes[1][1],
            rip->ra[0].pooled_height, rip->ra[0].pooled_width
        }, {
            input_shapes[0][0]
        }
    };
    return 0;
}
int cpu_distribute_fpn_proposals_roi_align_concatlayer::dtype(const void *param, size_t psize,
        const std::vector<int> &input_dtypes,
        std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0], CPU_DTYPE_INT32};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_DISTRIBUTE_FPN_PROPOSALS_ROI_ALIGN_CONCAT, cpu_distribute_fpn_proposals_roi_align_concat);
}/* namespace bmcpu*/
