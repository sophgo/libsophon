#include <cmath>
#include <thread>
#include "cpu_resize_interpolation.h"
#include "cpu_layer.h"
namespace bmcpu {
struct BilinearInterp {
    int lower;
    int upper;
    float scale;
};
inline float bilinear_interp(float tl, float tr, float bl, float br,
                             float scale_x, float scale_y) {
    float t = tl + (tr - tl) * scale_x;
    float b = bl + (br - bl) * scale_x;
    return t + (b - t) * scale_y;
}
static inline void genInterp(int src, int dst, int align_corners,
                             int half_pixel_centers,
                             std::vector<BilinearInterp> &interp) {
    float dstf = static_cast<float>(dst);
    float scale = (align_corners && dst > 1) ? (src - 1) / (dstf - 1) : src / dstf;
    interp[dst].lower = 0;
    interp[dst].upper = 0;
    if (half_pixel_centers) {
        for (int i = dst - 1; i >= 0; --i) {
            float s = (i + 0.5f) * scale - 0.5f;
            float f = std::floor(s);
            interp[i].lower = std::max(static_cast<int>(f), 0);
            interp[i].upper = std::min(static_cast<int>(std::ceil(s)), src - 1);
            interp[i].scale = s - f;
        }
    } else {
        for (int i = dst - 1; i >= 0; --i) {
            float s = i * scale;
            float f = std::floor(s);
            interp[i].lower = std::max(static_cast<int>(f), 0);
            interp[i].upper = std::min(static_cast<int>(std::ceil(s)), src - 1);
            interp[i].scale = s - f;
        }
    }
}
static inline void genInterpPytorch(int src, int dst, int align_corners,
                                    std::vector<BilinearInterp> &interp) {
    float dstf = static_cast<float>(dst);
    float scale = dst > 1 ?
                  (align_corners ? (src - 1) / (dstf - 1) : src / dstf) : 0.f;
    interp[dst].lower = 0;
    interp[dst].upper = 0;
    if (!align_corners) {
        for (int i = dst - 1; i >= 0; --i) {
            float s = std::max((i + 0.5f) * scale - 0.5f, 0.f);
            float f = std::floor(s);
            interp[i].lower = static_cast<int>(f);
            interp[i].upper = interp[i].lower + (interp[i].lower < src - 1 ? 1 : 0);
            interp[i].scale = s - f;
        }
    } else {
        for (int i = dst - 1; i >= 0; --i) {
            float s = i * scale;
            float f = std::floor(s);
            interp[i].lower = static_cast<int>(f);
            interp[i].upper = interp[i].lower + (interp[i].lower < src - 1 ? 1 : 0);
            interp[i].scale = s - f;
        }
    }
}
static inline void genInterp(int src, int dst, int align_corners,
                             int half_pixel_centers,
                             std::vector<int> &interp) {
    float dstf = static_cast<float>(dst);
    float scale = (align_corners && dst > 1) ? (src - 1) / (dstf - 1) : src / dstf;
    if (half_pixel_centers) {
        for (int i = 0; i < dst; ++i) {
            float s = (i + 0.5f) * scale;
            interp[i] = std::min(static_cast<int>(std::floor(s)), src - 1);
            interp[i] = std::max(interp[i], 0);
        }
    } else {
        if (align_corners) {
            for (int i = 0; i < dst; ++i) {
                float s = i * scale;
                interp[i] = std::min(static_cast<int>(std::round(s)), src - 1);
            }
        } else {
            for (int i = 0; i < dst; ++i) {
                float s = i * scale;
                interp[i] = std::min(static_cast<int>(std::floor(s)), src - 1);
            }
        }
    }
}
static inline void genInterpPytorch(int src, int dst,
                                    std::vector<int> &interp) {
    float dstf = static_cast<float>(dst);
    float scale = src / dstf;
    for (int i = 0; i < dst; ++i) {
        float s = i * scale;
        interp[i] = std::min(static_cast<int>(std::floor(s)), src - 1);
    }
}
template <typename T>
struct threadProcParam {
    int N, C;
    int src_H, src_W;
    int dst_H, dst_W;
    const float *src;
    float *dst;
    int start;
    int len;
    const T *y_interp;
    const T *x_interp;
};
static inline void nearestNCHW2NCHW(const threadProcParam<int> *p) {
    float *dst = p->dst + p->start * p->dst_W;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        for (int c = 0; c < p->C; ++c) {
            float *d = dst;
            for (int h = 0; h < p->len; ++h) {
                const float *s = src + p->y_interp[p->start + h] * p->src_W;
                for (int w = 0; w < p->dst_W; ++w) {
                    *d = *(s + p->x_interp[w]);
                    ++d;
                }
            }
            dst += p->dst_H * p->dst_W;
            src += p->src_H * p->src_W;
        }
    }
}
static inline void nearestNHWC2NCHW(const threadProcParam<int> *p) {
    float *dst = p->dst + p->start * p->dst_W;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        for (int c = 0; c < p->C; ++c) {
            float *d = dst;
            const float *sc = src + c;
            for (int h = 0; h < p->len; ++h) {
                const float *sh = sc + p->y_interp[p->start + h] * p->src_W * p->C;
                for (int w = 0; w < p->dst_W; ++w) {
                    *d = *(sh + p->x_interp[w] * p->C);
                    ++d;
                }
            }
            dst += p->dst_H * p->dst_W;
        }
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void nearestNCHW2NHWC(const threadProcParam<int> *p) {
    int fsize = p->src_H * p->src_W;
    float *dst = p->dst + p->start * p->dst_W * p->C;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        float *d = dst;
        for (int h = 0; h < p->len; ++h) {
            const float *sh = src + p->y_interp[p->start + h] * p->src_W;
            for (int w = 0; w < p->dst_W; ++w) {
                const float *sw = sh + p->x_interp[w];
                for (int c = 0; c < p->C; ++c) {
                    *d = *sw;
                    ++d;
                    sw += fsize;
                }
            }
        }
        dst += p->dst_H * p->dst_W * p->C;
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void nearestNHWC2NHWC(const threadProcParam<int> *p) {
    float *dst = p->dst + p->start * p->dst_W * p->C;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        float *d = dst;
        for (int h = 0; h < p->len; ++h) {
            const float *sh = src + p->y_interp[p->start + h] * p->src_W * p->C;
            for (int w = 0; w < p->dst_W; ++w) {
                const float *sw = sh + p->x_interp[w] * p->C;
                for (int c = 0; c < p->C; ++c) {
                    *d = *sw;
                    ++d;
                    ++sw;
                }
            }
        }
        dst += p->dst_H * p->dst_W * p->C;
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void nearest(const std::vector<threadProcParam<int> > &ps,
                           DataFormat fmt_src, DataFormat fmt_dst) {
    void(*f)(const threadProcParam<int> *) = fmt_src == BMCPU_NCHW
            ? (fmt_dst == BMCPU_NCHW ? nearestNCHW2NCHW : nearestNCHW2NHWC)
            : (fmt_dst == BMCPU_NCHW ? nearestNHWC2NCHW : nearestNHWC2NHWC);
    if (ps.size() == 1)
        f(&ps[0]);
    else {
        std::vector<std::thread> threads;
        for (auto &it : ps)
            threads.push_back(std::thread(f, &it));
        for (auto &it : threads)
            it.join();
    }
}
static inline void bilinearNCHW2NCHW(const threadProcParam<BilinearInterp> *p) {
    float *dst = p->dst + p->start * p->dst_W;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        for (int c = 0; c < p->C; ++c) {
            float *d = dst;
            for (int h = 0; h < p->len; ++h) {
                BilinearInterp by = p->y_interp[p->start + h];
                const float *sl = src + by.lower * p->src_W;
                const float *su = src + by.upper * p->src_W;
                for (int w = 0; w < p->dst_W; ++w) {
                    BilinearInterp bx = p->x_interp[w];
                    float tl = *(sl + bx.lower);
                    float tr = *(sl + bx.upper);
                    float bl = *(su + bx.lower);
                    float br = *(su + bx.upper);
                    *d = bilinear_interp(tl, tr, bl, br, bx.scale, by.scale);
                    ++d;
                }
            }
            dst += p->dst_H * p->dst_W;
            src += p->src_H * p->src_W;
        }
    }
}
static inline void bilinearNHWC2NCHW(const threadProcParam<BilinearInterp> *p) {
    float *dst = p->dst + p->start * p->dst_W;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        for (int c = 0; c < p->C; ++c) {
            float *d = dst;
            const float *sc = src + c;
            for (int h = 0; h < p->len; ++h) {
                BilinearInterp by = p->y_interp[p->start + h];
                const float *shl = sc + by.lower * p->src_W * p->C;
                const float *shu = sc + by.upper * p->src_W * p->C;
                for (int w = 0; w < p->dst_W; ++w) {
                    BilinearInterp bx = p->x_interp[w];
                    float tl = *(shl + bx.lower * p->C);
                    float tr = *(shl + bx.upper * p->C);
                    float bl = *(shu + bx.lower * p->C);
                    float br = *(shu + bx.upper * p->C);
                    *d = bilinear_interp(tl, tr, bl, br, bx.scale, by.scale);
                    ++d;
                }
            }
            dst += p->dst_H * p->dst_W;
        }
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void bilinearNCHW2NHWC(const threadProcParam<BilinearInterp> *p) {
    int fsize = p->src_H * p->src_W;
    float *dst = p->dst + p->start * p->dst_W * p->C;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        float *d = dst;
        for (int h = 0; h < p->len; ++h) {
            BilinearInterp by = p->y_interp[p->start + h];
            const float *shl = src + by.lower * p->src_W;
            const float *shu = src + by.upper * p->src_W;
            for (int w = 0; w < p->dst_W; ++w) {
                BilinearInterp bx = p->x_interp[w];
                const float *tl = shl + bx.lower;
                const float *tr = shl + bx.upper;
                const float *bl = shu + bx.lower;
                const float *br = shu + bx.upper;
                for (int c = 0; c < p->C; ++c) {
                    *d = bilinear_interp(*tl, *tr, *bl, *br, bx.scale, by.scale);
                    ++d;
                    tl += fsize;
                    tr += fsize;
                    bl += fsize;
                    br += fsize;
                }
            }
        }
        dst += p->dst_H * p->dst_W * p->C;
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void bilinearNHWC2NHWC(const threadProcParam<BilinearInterp> *p) {
    float *dst = p->dst + p->start * p->dst_W * p->C;
    const float *src = p->src;
    for (int n = 0; n < p->N; ++n) {
        float *d = dst;
        for (int h = 0; h < p->len; ++h) {
            BilinearInterp by = p->y_interp[p->start + h];
            const float *shl = src + by.lower * p->src_W * p->C;
            const float *shu = src + by.upper * p->src_W * p->C;
            for (int w = 0; w < p->dst_W; ++w) {
                BilinearInterp bx = p->x_interp[w];
                const float *tl = shl + bx.lower * p->C;
                const float *tr = shl + bx.upper * p->C;
                const float *bl = shu + bx.lower * p->C;
                const float *br = shu + bx.upper * p->C;
                for (int c = 0; c < p->C; ++c) {
                    *d = bilinear_interp(*tl, *tr, *bl, *br, bx.scale, by.scale);
                    ++d;
                    ++tl;
                    ++tr;
                    ++bl;
                    ++br;
                }
            }
        }
        dst += p->dst_H * p->dst_W * p->C;
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void bilinear(
    const std::vector<threadProcParam<BilinearInterp> > &ps,
    DataFormat fmt_src, DataFormat fmt_dst) {
    void(*f)(const threadProcParam<BilinearInterp> *) =
        fmt_src == BMCPU_NCHW
        ? (fmt_dst == BMCPU_NCHW ? bilinearNCHW2NCHW : bilinearNCHW2NHWC)
        : (fmt_dst == BMCPU_NCHW ? bilinearNHWC2NCHW : bilinearNHWC2NHWC);
    if (ps.size() == 1)
        f(&ps[0]);
    else {
        std::vector<std::thread> threads;
        for (auto &it : ps)
            threads.push_back(std::thread(f, &it));
        for (auto &it : threads)
            it.join();
    }
}
static inline void copyNCHW2NCHW(const threadProcParam<void> *p) {
    float *dst = p->dst + p->start * p->dst_W;
    const float *src = p->src +p->start * p->src_W;
    for (int n = 0; n < p->N; ++n) {
        for (int c = 0; c < p->C; ++c) {
            memcpy(dst, src, p->len * p->src_W * sizeof(float));
            dst += p->dst_H * p->dst_W;
            src += p->src_H * p->src_W;
        }
    }
}
static inline void copyNHWC2NCHW(const threadProcParam<void> *p) {
    float *dst = p->dst + p->start * p->dst_W;
    const float *src = p->src + p->start * p->src_W * p->C;
    for (int n = 0; n < p->N; ++n) {
        for (int c = 0; c < p->C; ++c) {
            float *d = dst;
            const float *sc = src + c;
            for (int h = 0; h < p->len; ++h) {
                const float *sh = sc + h * p->src_W * p->C;
                for (int w = 0; w < p->dst_W; ++w) {
                    *d = *sh;
                    ++d;
                    sh += p->C;
                }
            }
            dst += p->dst_H * p->dst_W;
        }
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void copyNCHW2NHWC(const threadProcParam<void> *p) {
    int fsize = p->src_H * p->src_W;
    float *dst = p->dst + p->start * p->dst_W * p->C;
    const float *src = p->src +p->start * p->src_W;
    for (int n = 0; n < p->N; ++n) {
        float *d = dst;
        for (int h = 0; h < p->len; ++h) {
            const float *sh = src + h * p->src_W;
            for (int w = 0; w < p->dst_W; ++w) {
                const float *sw = sh + w;
                for (int c = 0; c < p->C; ++c) {
                    *d = *sw;
                    ++d;
                    sw += fsize;
                }
            }
        }
        dst += p->dst_H * p->dst_W * p->C;
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void copyNHWC2NHWC(const threadProcParam<void> *p) {
    float *dst = p->dst + p->start * p->dst_W * p->C;
    const float *src = p->src + p->start * p->src_W * p->C;
    for (int n = 0; n < p->N; ++n) {
        memcpy(dst, src, p->len * p->src_W * p->C * sizeof(float));
        dst += p->dst_H * p->dst_W * p->C;
        src += p->src_H * p->src_W * p->C;
    }
}
static inline void copy(const std::vector<threadProcParam<void> > &ps,
                        DataFormat fmt_src, DataFormat fmt_dst) {
    void(*f)(const threadProcParam<void> *) = fmt_src == BMCPU_NCHW
            ? (fmt_dst == BMCPU_NCHW ? copyNCHW2NCHW : copyNCHW2NHWC)
            : (fmt_dst == BMCPU_NCHW ? copyNHWC2NCHW : copyNHWC2NHWC);
    if (ps.size() == 1)
        f(&ps[0]);
    else {
        std::vector<std::thread> threads;
        for (auto &it : ps)
            threads.push_back(std::thread(f, &it));
        for (auto &it : threads)
            it.join();
    }
}
int cpu_resize_interpolationlayer::process(void *param, int param_size) {
    const int *out_shape = reinterpret_cast<int *>(input_tensors_[1]);
    CPU_ASSERT(4 == input_shapes_[0].size());
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_resize_interpolation_param_t, rip, param,
                                   param_size);
    int N = input_shapes_[0][0];
    int C = rip->ifmt == BMCPU_NCHW ? input_shapes_[0][1] : input_shapes_[0][3];
    int src_H = rip->ifmt == BMCPU_NCHW ? input_shapes_[0][2] : input_shapes_[0][1];
    int src_W = rip->ifmt == BMCPU_NCHW ? input_shapes_[0][3] : input_shapes_[0][2];
    int dst_H = out_shape[0];
    int dst_W = out_shape[1];
    int num_thread = 1;
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    if (nt != nullptr)
        num_thread = atoi(nt);
    CPU_ASSERT(num_thread > 0);
    if (num_thread > dst_H)
        num_thread = dst_H;
    if (src_H == dst_H && src_W == dst_W) {
        threadProcParam<void> p;
        p.N = N;
        p.C = C;
        p.src_H = src_H;
        p.src_W = src_W;
        p.dst_H = dst_H;
        p.dst_W = dst_W;
        p.src = input_tensors_[0];
        p.dst = output_tensors_[0];
        int hpt = (dst_H - 1) / num_thread + 1;
        std::vector<threadProcParam<void> > ps;
        for (int i = 0; i < num_thread; ++i) {
            threadProcParam<void> c = p;
            c.start = i * hpt;
            c.len = i < num_thread - 1 ? hpt : (dst_H - hpt * i);
            ps.push_back(c);
        }
        copy(ps, rip->ifmt, rip->ofmt);
        return 0;
    }
    switch (rip->intepolation_method) {
    case METHOD_BILINEAR:
    case METHOD_BILINEAR_PYTORCH: {
        std::vector<BilinearInterp> x_interp(dst_W + 1), y_interp(dst_H + 1);
        if (rip->intepolation_method == METHOD_BILINEAR) {
            genInterp(src_H, dst_H, rip->align_corners, rip->half_pixel_centers, y_interp);
            genInterp(src_W, dst_W, rip->align_corners, rip->half_pixel_centers, x_interp);
        } else {
            genInterpPytorch(src_H, dst_H, rip->align_corners, y_interp);
            genInterpPytorch(src_W, dst_W, rip->align_corners, x_interp);
        }
        threadProcParam<BilinearInterp> p;
        p.N = N;
        p.C = C;
        p.src_H = src_H;
        p.src_W = src_W;
        p.dst_H = dst_H;
        p.dst_W = dst_W;
        p.src = input_tensors_[0];
        p.dst = output_tensors_[0];
        p.y_interp = y_interp.data();
        p.x_interp = x_interp.data();
        int hpt = (dst_H - 1) / num_thread + 1;
        std::vector<threadProcParam<BilinearInterp> > ps;
        for (int i = 0; i < num_thread; ++i) {
            threadProcParam<BilinearInterp> c = p;
            c.start = i * hpt;
            c.len = i < num_thread - 1 ? hpt : (dst_H - hpt * i);
            ps.push_back(c);
        }
        bilinear(ps, rip->ifmt, rip->ofmt);
    }
    break;
    case METHOD_NEAREST:
    case METHOD_NEAREST_PYTORCH: {
        std::vector<int> x_interp(dst_W), y_interp(dst_H);
        if (rip->intepolation_method == METHOD_NEAREST) {
            genInterp(src_H, dst_H, rip->align_corners, rip->half_pixel_centers, y_interp);
            genInterp(src_W, dst_W, rip->align_corners, rip->half_pixel_centers, x_interp);
        } else {
            genInterpPytorch(src_H, dst_H, y_interp);
            genInterpPytorch(src_W, dst_W, x_interp);
        }
        threadProcParam<int> p;
        p.N = N;
        p.C = C;
        p.src_H = src_H;
        p.src_W = src_W;
        p.dst_H = dst_H;
        p.dst_W = dst_W;
        p.src = input_tensors_[0];
        p.dst = output_tensors_[0];
        p.y_interp = y_interp.data();
        p.x_interp = x_interp.data();
        int hpt = (dst_H - 1) / num_thread + 1;
        std::vector<threadProcParam<int> > ps;
        for (int i = 0; i < num_thread; ++i) {
            threadProcParam<int> c = p;
            c.start = i * hpt;
            c.len = i < num_thread - 1 ? hpt : (dst_H - hpt * i);
            ps.push_back(c);
        }
        nearest(ps, rip->ifmt, rip->ofmt);
    }
    break;
    default:
        printf("error: %s: %d: no match interpolation method: %d. \n",
               __FILE__, __LINE__, rip->intepolation_method);
        exit(-1);
    }
    (*output_shapes_)[0][0] = N;
    (*output_shapes_)[0][1] = rip->ofmt == BMCPU_NCHW ? C : dst_H;
    (*output_shapes_)[0][2] = rip->ofmt == BMCPU_NCHW ? dst_H : dst_W;
    (*output_shapes_)[0][3] = rip->ofmt == BMCPU_NCHW ? dst_W : C;
    return 0;
}
int cpu_resize_interpolationlayer::reshape(
    void *param, int param_size,
    const vector<vector<int>> &input_shapes,
    vector<vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_resize_interpolation_param_t, rip, param,
                                   param_size);
    int C = rip->ifmt == BMCPU_NCHW ? input_shapes[0][1] : input_shapes[0][3];
    output_shapes[0].clear();
    output_shapes[0].push_back(input_shapes[0][0]);
    output_shapes[0].push_back(rip->ofmt == BMCPU_NCHW ? C : rip->oh);
    output_shapes[0].push_back(rip->ofmt == BMCPU_NCHW ? rip->oh : rip->ow);
    output_shapes[0].push_back(rip->ofmt == BMCPU_NCHW ? rip->ow : C);
    return 0;
}
int cpu_resize_interpolationlayer::dtype(const void *param, size_t param_size, const vector<int> &input_dtypes,
        vector<int> &output_dtypes) {
    output_dtypes.clear();
    output_dtypes.push_back(input_dtypes[0]);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_RESIZE_INTERPOLATION, cpu_resize_interpolation);
} /* namespace bmcpu*/
