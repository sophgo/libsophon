#include "cpu_pytorch_roi_align.h"
#include <algorithm>
#include <numeric>
#include <vector>
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
int cpu_pytorch_roi_alignlayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_pytorch_roi_align_param_t, rip, param, psize);
    const int pooled_h = rip->pooled_height;
    const int pooled_w = rip->pooled_width;
    const float pooled_h_inv = FLOAT(1. / pooled_h);
    const float pooled_w_inv = FLOAT(1. / pooled_w);
    const int channels = input_shapes_[0][1];
    const int ih = input_shapes_[0][2];
    const int iw = input_shapes_[0][3];
    const float roi_offset = rip->align ? .5f : 0.f;
    const int num_rois = input_shapes_[1][0];
    int num_thread = 1;
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    if (nt != nullptr)
        num_thread = atoi(nt);
    typedef struct ThreadParam {
        int start;
        int end;
    } ThreadParam_t;
    int opt = num_rois / num_thread;
    int start = 0;
    std::vector<ThreadParam_t> ps(num_thread);
    for (int i = 0; i < num_thread; ++i) {
        ps[i].start = start;
        int len = opt + (i < num_rois - opt * num_thread ? 1 : 0);
        ps[i].end = start + len;
        start = ps[i].end;
    }
    auto func = [&](const ThreadParam_t *tp) {
        for (int i = tp->start; i < tp->end; ++i) {
            auto riter = FLOAT_PTR(input_tensors_[1]) + i * 5;
            int batch_idx = INT(riter[0]);
            float start_w = riter[1] * rip->spatial_scale - roi_offset;
            float start_h = riter[2] * rip->spatial_scale - roi_offset;
            float end_w = riter[3] * rip->spatial_scale - roi_offset;
            float end_h = riter[4] * rip->spatial_scale - roi_offset;
            float roi_w = end_w - start_w;
            float roi_h = end_h - start_h;
            if (!rip->align) {
                roi_w = std::max(roi_w, 1.f);
                roi_h = std::max(roi_h, 1.f);
            }

            int grid_w = rip->sampling_ratio > 0.f ? rip->sampling_ratio
                         : ceil(roi_w * pooled_w_inv);
            int grid_h = rip->sampling_ratio > 0.f ? rip->sampling_ratio
                         : ceil(roi_h * pooled_h_inv);
            float avg_scaling = FLOAT(1. / FLOAT(grid_h * grid_w));
            std::vector<PreCalc> pre_calc(grid_h * grid_w * pooled_h * pooled_w);
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
            float *out = FLOAT_PTR(output_tensors_[0]) + i * channels * pooled_h * pooled_w;
            for (int c = 0; c < channels; ++c) {
                const float *idata = FLOAT_PTR(input_tensors_[0]) +
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
    *output_shapes_ = {{num_rois, channels, pooled_h, pooled_w}};
    return 0;
}
int cpu_pytorch_roi_alignlayer::reshape(void *param, int psize,
                                        const std::vector<std::vector<int>> &input_shapes,
                                        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_pytorch_roi_align_param_t, rip, param, psize);
    output_shapes = {{
            input_shapes[1][0], input_shapes[0][1],
            rip->pooled_height, rip->pooled_width
        }
    };
    return 0;
}
int cpu_pytorch_roi_alignlayer::dtype(const void *param, size_t psize,
                                      const std::vector<int> &input_dtypes,
                                      std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_PYTORCH_ROI_ALIGN, cpu_pytorch_roi_align);
}/* namespace bmcpu*/

