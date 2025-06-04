#include <string>
#include <cmath>
#include <thread>
#include "cpu_crop_and_resize.h"
#include "cpu_layer.h"
namespace  bmcpu {
struct threadProcParam {
    int src_h, src_w, dst_h, dst_w;
    int channels;
    const float *src;
    float *dst;
    const float *box;
    const int *idx;
    int num;
    float extrapolation;
    int method;
};
static void threadProc(threadProcParam *p) {
    float sh = p->dst_h > 1 ?
               static_cast<float>(p->src_h - 1) / (p->dst_h - 1) : 0.f;
    float sw = p->dst_w > 1 ?
               static_cast<float>(p->src_w - 1) / (p->dst_w - 1) : 0.f;
    int fsize_src = p->src_h * p->src_w;
    int isize_src = p->channels * fsize_src;
    int isize_dst = p->channels * p->dst_h * p->dst_w;
    bool *vy = new bool[p->dst_h];
    bool *vx = new bool[p->dst_w];
    float *gy = p->method == METHOD_BILINEAR ? new float[p->dst_h] : nullptr;
    float *gx = p->method == METHOD_BILINEAR ? new float[p->dst_w] : nullptr;
    int *py_round = p->method == METHOD_BILINEAR ? nullptr : new int[p->dst_h];
    int *px_round = p->method == METHOD_BILINEAR ? nullptr : new int[p->dst_w];
    int *py_floor = p->method == METHOD_BILINEAR ? new int[p->dst_h] : nullptr;
    int *px_floor = p->method == METHOD_BILINEAR ? new int[p->dst_w] : nullptr;
    int *py_ceil = p->method == METHOD_BILINEAR ? new int[p->dst_h] : nullptr;
    int *px_ceil = p->method == METHOD_BILINEAR ? new int[p->dst_w] : nullptr;
    for (int b = 0; b < p->num; ++b) {
        float y1 = *p->box;
        ++p->box;
        float x1 = *p->box;
        ++p->box;
        float y2 = *p->box;
        ++p->box;
        float x2 = *p->box;
        ++p->box;
        float scale_h = (y2 - y1) * sh;
        float scale_w = (x2 - x1) * sw;
        const float *src = p->src + p->idx[b] * isize_src;
        float *dst = p->dst + b * isize_dst;
        if (p->method == METHOD_BILINEAR) {
            for (int h = 0; h < p->dst_h; ++h) {
                float py = p->dst_h > 1
                           ? y1 * (p->src_h - 1) + h * scale_h
                           : 0.5f * (y1 + y2) * (p->src_h - 1);
                vy[h] = py < 0.f || py > static_cast<float>(p->src_h - 1);
                gy[h] = py - floor(py);
                py_ceil[h] = static_cast<int>(ceil(py)) * p->src_w;
                py_floor[h] = static_cast<int>(floor(py)) * p->src_w;
            }
            for (int w = 0; w < p->dst_w; ++w) {
                float px = p->dst_w > 1
                           ? x1 * (p->src_w - 1) + w * scale_w
                           : 0.5f * (x1 + x2) * (p->src_w - 1);
                vx[w] = px < 0.f || px > static_cast<float>(p->src_w - 1);
                gx[w] = px - floor(px);
                px_ceil[w] = static_cast<int>(ceil(px));
                px_floor[w] = static_cast<int>(floor(px));
            }
            for (int c = 0; c < p->channels; ++c) {
                for (int h = 0; h < p->dst_h; ++h) {
                    if (vy[h]) {
                        for (int w = 0; w < p->dst_w; ++w) {
                            *dst = p->extrapolation;
                            ++dst;
                        }
                    } else {
                        for (int w = 0; w < p->dst_w; ++w) {
                            if (vx[w])
                                *dst = p->extrapolation;
                            else {
                                float tr = *(src + py_floor[h] + px_ceil[w]);
                                float tl = *(src + py_floor[h] + px_floor[w]);
                                float br = *(src + py_ceil[h] + px_ceil[w]);
                                float bl = *(src + py_ceil[h] + px_floor[w]);
                                float top = tl + (tr - tl) * gx[w];
                                float bottom = bl + (br - bl) * gx[w];
                                *dst = top + (bottom - top) * gy[h];
                            }
                            ++dst;
                        }
                    }
                }
                src += fsize_src;
            }
        } else {
            for (int h = 0; h < p->dst_h; ++h) {
                float py = p->dst_h > 1
                           ? y1 * (p->src_h - 1) + h * scale_h
                           : 0.5f * (y1 + y2) * (p->src_h - 1);
                vy[h] = py < 0.f || py > static_cast<float>(p->src_h - 1);
                py_round[h] = static_cast<int>(round(py)) * p->src_w;
            }
            for (int w = 0; w < p->dst_w; ++w) {
                float px = p->dst_w > 1
                           ? x1 * (p->src_w - 1) + w * scale_w
                           : 0.5f * (x1 + x2) * (p->src_w - 1);
                vx[w] = px < 0.f || px > static_cast<float>(p->src_w - 1);
                px_round[w] = static_cast<int>(round(px));
            }
            for (int c = 0; c < p->channels; ++c) {
                for (int h = 0; h < p->dst_h; ++h) {
                    if (vy[h]) {
                        for (int w = 0; w < p->dst_w; ++w) {
                            *dst = p->extrapolation;
                            ++dst;
                        }
                    } else {
                        for (int w = 0; w < p->dst_w; ++w) {
                            if (vx[w])
                                *dst = p->extrapolation;
                            else
                                *dst = *(src + py_round[h] + px_round[w]);
                            ++dst;
                        }
                    }
                }
                src += fsize_src;
            }
        }
    }
    if (vy != nullptr)
        delete [] vy;
    if (vx != nullptr)
        delete [] vx;
    if (gy != nullptr)
        delete [] gy;
    if (gx != nullptr)
        delete [] gx;
    if (py_round != nullptr)
        delete [] py_round;
    if (px_round != nullptr)
        delete [] px_round;
    if (py_floor != nullptr)
        delete [] py_floor;
    if (px_floor != nullptr)
        delete [] px_floor;
    if (py_ceil != nullptr)
        delete [] py_ceil;
    if (px_ceil != nullptr)
        delete [] px_ceil;
}
int cpu_crop_and_resizelayer::process(void *raw_param, int param_size) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_crop_and_resize_t, param, raw_param, param_size);
    RESIZE_METHOD_T method = param->method;
    CPU_ASSERT(input_shapes_[1][1] == 4);
    threadProcParam p;
    p.src = input_tensors_[0];
    p.method = method;
    p.channels = input_shapes_[0][1];
    p.src_h = input_shapes_[0][2];
    p.src_w = input_shapes_[0][3];
    p.dst_h = param->crop_h;
    p.dst_w = param->crop_w;
    p.extrapolation = param->extrapolation_value;
    p.box = reinterpret_cast<float *>(input_tensors_[1]);
    p.dst = output_tensors_[0];
    p.idx = reinterpret_cast<int *>(input_tensors_[2]);
    p.num = input_shapes_[1][0];
    int num_thread = 1;
    char *nt = getenv("BM_CPU_LAYER_NUM_THREAD");
    if (nt != nullptr)
        num_thread = atoi(nt);
    CPU_ASSERT(num_thread > 0);
    if (num_thread > input_shapes_[1][0] && input_shapes_[1][0] != 0) {
        num_thread = input_shapes_[1][0];
    }
    if (input_shapes_[1][0] != 0) {
        if (num_thread == 1) {
            threadProc(&p);
        }
        else {
            int bpt = (input_shapes_[1][0] - 1) / num_thread + 1;
            std::vector<threadProcParam> ps;
            for (int i = 0; i < num_thread; ++i) {
                threadProcParam c = p;
                c.box = p.box + i * bpt * 4;
                c.dst = p.dst + i * bpt * p.channels * p.dst_h * p.dst_w;
                c.idx = p.idx + i * bpt;
                c.num = i < num_thread - 1 ? bpt : (input_shapes_[1][0] - bpt * i);
                ps.push_back(c);
            }
            std::vector<std::thread> threads;
            for (auto &it : ps)
                threads.push_back(std::thread(threadProc, &it));
            for (auto &it : threads)
                it.join();
        }
    }
    (*output_shapes_)[0][0] = input_shapes_[1][0];
    (*output_shapes_)[0][1] = input_shapes_[0][1];
    (*output_shapes_)[0][2] = param->crop_h;
    (*output_shapes_)[0][3] = param->crop_w;
    return 0;
}
int cpu_crop_and_resizelayer::reshape(
    void *param, int param_size,
    const vector<vector<int>> &input_shapes,
    vector<vector<int>> &output_shapes) {
    BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_crop_and_resize_t, layer_param, param, param_size);
    output_shapes[0].clear();
    output_shapes[0].push_back(input_shapes[1][0]);
    output_shapes[0].push_back(input_shapes[0][1]);
    output_shapes[0].push_back(layer_param->crop_h);
    output_shapes[0].push_back(layer_param->crop_w);
    return 0;
}
REGISTER_CPULAYER_CLASS(CPU_CROP_AND_RESIZE, cpu_crop_and_resize)
} /* namespace bmcpu */
