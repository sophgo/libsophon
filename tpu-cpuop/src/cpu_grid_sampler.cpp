#include "cpu_grid_sampler.h"
#include <cmath>
#include <thread>
#define FLOAT_PTR(p) (reinterpret_cast<float *>(p))
#define INT_PTR(p) (reinterpret_cast<int *>(p))
#define FLOAT(val) (static_cast<float>(val))
#define INT(val) (static_cast<int>(val))

// Clips coordinates to between 0 and clip_limit - 1
template <typename scalar_t>
static inline scalar_t clip_coordinates(scalar_t in, int64_t clip_limit) {
  return std::min(static_cast<scalar_t>(clip_limit - 1),
                  std::max(in, static_cast<scalar_t>(0)));
}

// Reflects coordinates until they fall between low and high (inclusive).
// The bounds are passed as twice their value so that half-integer values
// can be represented as ints.
template <typename scalar_t>
static inline scalar_t reflect_coordinates(scalar_t in, int64_t twice_low,
                                           int64_t twice_high) {
  if (twice_low == twice_high) {
    return static_cast<scalar_t>(0);
  }
  scalar_t min = static_cast<scalar_t>(twice_low) / 2;
  scalar_t span = static_cast<scalar_t>(twice_high - twice_low) / 2;
  in = std::fabs(in - min);
  // `fmod` returns same sign as `in`, which is positive after the `fabs` above.
  scalar_t extra = std::fmod(in, span);
  int flips = static_cast<int>(std::floor(in / span));
  if (flips % 2 == 0) {
    return extra + min;
  } else {
    return span - extra + min;
  }
}

namespace bmcpu {
static inline float computeIndex(
    float coord, int size, int paddingMode, bool alignCorners) {
    float res = 0.f;

    // Unnormalize coordinate
    // From [-1, 1] to pixel index
    if (alignCorners)
        res = ((coord + 1.f) * .5f) * (size - 1);
    else
        res = ((coord + 1.f) * size - 1.f) * .5f;

    switch (paddingMode) {
    case GridSamplerZeros:
        break;
    case GridSamplerBorder:
        res = clip_coordinates(res, size);
        break;
    case GridSamplerReflection:
        if (alignCorners) {
            res = reflect_coordinates(res, 0, 2 * (size - 1));
        } else {
            res = reflect_coordinates(res, -1, 2 * size - 1);
        }
        res = clip_coordinates(res, size);
        break;
    default:
        CPU_ASSERT(0);
    }
    return res;
}
int cpu_grid_samplerlayer::process(void *param, int psize) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_grid_sampler_param_t, rip, param, psize);
    const int N = input_shapes_[0][0];
    const int C = input_shapes_[0][1];
    const int IH = input_shapes_[0][2];
    const int IW = input_shapes_[0][3];
    const int OH = input_shapes_[1][1];
    const int OW = input_shapes_[1][2];
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
            const float *input = FLOAT_PTR(input_tensors_[0]) + n * C * IH * IW;
            const float *grid = FLOAT_PTR(input_tensors_[1]) + n * OH * OW * 2 + tp->start * OW * 2;
            float *output = FLOAT_PTR(output_tensors_[0]) + n * C * OH * OW + tp->start * OW;
            for (int h = tp->start; h < tp->end; ++h) {
                for (int w = 0; w < OW; ++w) {
                    auto fx = computeIndex(*grid, IW, rip->padding_mode, rip->align_corners);
                    ++grid;
                    auto fy = computeIndex(*grid, IH, rip->padding_mode, rip->align_corners);
                    ++grid;
                    switch (rip->mode) {
                    case GridSamplerBilinear: {
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
                    case GridSamplerNearest: {
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
    return 0;
}
int cpu_grid_samplerlayer::reshape(void *param, int psize,
                                   const std::vector<std::vector<int>> &input_shapes,
                                   std::vector<std::vector<int>> &output_shapes) {
    output_shapes = {{input_shapes[0][0], input_shapes[0][1], input_shapes[1][1], input_shapes[1][2]}};
    return 0;
}
int cpu_grid_samplerlayer::dtype(const void *param, size_t psize,
                                 const std::vector<int> &input_dtypes,
                                 std::vector<int> &output_dtypes) {
    output_dtypes = {input_dtypes[0]};
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_GRID_SAMPLER, cpu_grid_sampler);
} // namespace bmcpu
