#include "cpu_affine_grid_generator.h"
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
int cpu_affine_grid_generatorlayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_affine_grid_generator_param_t, rip, param, psize);
    const auto os = INT_PTR(input_tensors_[1]);
    const int N = os[0];
    const int H = os[2];
    const int W = os[3];
    float *hSteps = new float[H > 0 ? H : 0];
    float *wSteps = new float[W > 0 ? W : 0];
    linspace(hSteps, H, rip->align_corners);
    linspace(wSteps, W, rip->align_corners);
    int num_thread = 1;
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    if (nt != nullptr)
        num_thread = atoi(nt);
    typedef struct ThreadParam {
        int start;
        int end;
    } ThreadParam_t;
    int opt = H / num_thread;
    int start = 0;
    std::vector<ThreadParam_t> ps(num_thread);
    for (int i = 0; i < num_thread; ++i) {
        ps[i].start = start;
        int len = opt + (i < H - opt * num_thread ? 1 : 0);
        ps[i].end = start + len;
        start = ps[i].end;
    }
    auto func = [&](const ThreadParam_t *tp) {
        for (int n = 0; n < N; ++n) {
            auto oiter = FLOAT_PTR(output_tensors_[0]) + n * H * W * 2 + tp->start * W * 2;
            const auto titer = FLOAT_PTR(input_tensors_[0]) + n * 6;
            for (int h = tp->start; h < tp->end; ++h) {
                float wBase = hSteps[h] * titer[1] + titer[2];
                float yBase = hSteps[h] * titer[4] + titer[5];
                for (int w = 0; w < W; ++w) {
                    *oiter = wSteps[w] * titer[0] + wBase;
                    ++oiter;
                    *oiter = wSteps[w] * titer[3] + yBase;
                    ++oiter;
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
    *output_shapes_ = {{N, H, W, 2}};
    delete [] hSteps;
    delete [] wSteps;
    return 0;
}
int cpu_affine_grid_generatorlayer::reshape(void *param, int psize,
        const std::vector<std::vector<int>> &input_shapes,
        std::vector<std::vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_affine_grid_generator_param_t, rip, param, psize);
    output_shapes = {{rip->N, rip->H, rip->W, 2}};
    return 0;
}
int cpu_affine_grid_generatorlayer::dtype(const void *param, size_t psize,
        const std::vector<int> &input_dtypes,
        std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_AFFINE_GRID_GENERATOR, cpu_affine_grid_generator);
} // namespace bmcpu
