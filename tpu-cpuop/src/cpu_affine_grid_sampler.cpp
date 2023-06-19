#include "cpu_affine_grid_sampler.h"
#include <cmath>
#include <thread>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))
namespace bmcpu {
static inline void linspace(float *dst, int L, bool alignCorners) {
    if (L <= 1)
        *dst = 0.f;
    else {
        float step = FLOAT(2. / (L - 1));
        dst[0] = -1.f;
        dst[L - 1] = 1.f;
        for (int i = 1; i < L - 1; ++i)
            dst[i] = dst[i - 1] + step;
        if (!alignCorners) {
            float scale = FLOAT((L - 1) * 1. / L);
            for (int i = 0; i < L; ++i)
                dst[i] *= scale;
        }
    }
}
static inline float computeIndex(
    float coord, int size, int paddingMode, bool alignCorners) {
    float res = 0.f;
    if (alignCorners)
        res = ((coord + 1.f) * .5f) * (size - 1);
    else
        res = ((coord + 1.f) * size - 1.f) * .5f;
    switch (paddingMode) {
    case 0:
        break;
    case 1:
    case 2:
    default:
        CPU_ASSERT(0);
    }
    return res;
}
int cpu_affine_grid_samplerlayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_affine_grid_sampler_param_t, rip, param, psize);
    const auto os = INT_PTR(input_tensors_[1]);
    const int N = input_shapes_[2][0];
    const int C = input_shapes_[2][1];
    const int IH = input_shapes_[2][2];
    const int IW = input_shapes_[2][3];
    const int OH = os[2];
    const int OW = os[3];
    float *hSteps = new float[OH > 0 ? OH : 0];
    float *wSteps = new float[OW > 0 ? OW : 0];
    linspace(hSteps, OH, rip->generator.align_corners);
    linspace(wSteps, OW, rip->generator.align_corners);
    int num_thread = 1;
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    if (nt != nullptr)
        num_thread = atoi(nt);
    typedef struct ThreadParam {
        int start;
        int end;
    } ThreadParam_t;
    int opt = OH / num_thread;
    int start = 0;
    std::vector<ThreadParam_t> ps(num_thread);
    for (int i = 0; i < num_thread; ++i) {
        ps[i].start = start;
        int len = opt + (i < OH - opt * num_thread ? 1 : 0);
        ps[i].end = start + len;
        start = ps[i].end;
    }
    auto func = [&](const ThreadParam_t *tp) {
        for (int n = 0; n < N; ++n) {
            const float *input = FLOAT_PTR(input_tensors_[2]) + n * C * IH * IW;
            const float *theta = FLOAT_PTR(input_tensors_[0]) + n * 6;
            float *output = FLOAT_PTR(output_tensors_[0]) + n * C * OH * OW + tp->start * OW;
            for (int h = tp->start; h < tp->end; ++h) {
                float xGridBase = hSteps[h] * theta[1] + theta[2];
                float yGridBase = hSteps[h] * theta[4] + theta[5];
                for (int w = 0; w < OW; ++w) {
                    float xGrid = xGridBase + wSteps[w] * theta[0];
                    auto fx = computeIndex(xGrid, IW, rip->sampler.padding_mode, rip->sampler.align_corners);
                    float yGrid = yGridBase + wSteps[w] * theta[3];
                    auto fy = computeIndex(yGrid, IH, rip->sampler.padding_mode, rip->sampler.align_corners);
                    switch (rip->sampler.mode) {
                    case 0: {
                        int x = INT(std::floor(fx));
                        int y = INT(std::floor(fy));
                        float dx = fx - x;
                        float dy = fy - y;
                        float tx = 1.f - dx;
                        float ty = 1.f - dy;
                        float txty = tx * ty, dxty = dx * ty, txdy = tx * dy, dxdy = dx * dy;
                        bool yBound_0 = y >= 0 && y < IH;
                        bool yBound_1 = y + 1 >= 0 && y + 1 < IH;
                        bool xBound_0 = x >= 0 && x < IW;
                        bool xBound_1 = x + 1 >= 0 && x + 1 < IW;
                        const float *iiter = input + y * IW + x;
                        float *oiter = output;
                        for (int c = 0; c < C; ++c) {
                            *oiter = 0.f;
                            if (yBound_0) {
                                if (xBound_0)
                                    *oiter += iiter[0] * txty;
                                if (xBound_1)
                                    *oiter += iiter[1] * dxty;
                            }
                            if (yBound_1) {
                                if (xBound_0)
                                    *oiter += iiter[IW] * txdy;
                                if (xBound_1)
                                    *oiter += iiter[IW + 1] * dxdy;
                            }
                            iiter += IH * IW;
                            oiter += OH * OW;
                        }
                    }
                    break;
                    case 1: {
                        int x = INT(std::round(fx));
                        int y = INT(std::round(fy));
                        const float *iiter = input + y * IW + x;
                        float *oiter = output;
                        for (int c = 0; c < C; ++c) {
                            *oiter = y >= 0 && y < IH && x >= 0 && x < IW ? *iiter : 0.f;
                            iiter += IH * IW;
                            oiter += OH * OW;
                        }
                    }
                    break;
                    default:
                        CPU_ASSERT(0);
                    }
                    ++output;
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
    *output_shapes_ = {{N, C, OH, OW}};
    delete [] hSteps;
    delete [] wSteps;
    return 0;
}
int cpu_affine_grid_samplerlayer::reshape(void *param, int psize,
        const std::vector<std::vector<int>> &input_shapes,
        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_affine_grid_sampler_param_t, rip, param, psize);
    output_shapes = {{input_shapes[2][0], input_shapes[2][1], rip->generator.H, rip->generator.W}};
    return 0;
}
int cpu_affine_grid_samplerlayer::dtype(const void *param, size_t psize,
                                        const std::vector<int> &input_dtypes,
                                        std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[2]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_AFFINE_GRID_SAMPLER, cpu_affine_grid_sampler);
} // namespace bmcpu
