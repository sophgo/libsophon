#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <vector>
#include <math.h>
#include <float.h>
#include <thread>
#include <arm_neon.h>
#include "bmcv_cpu_func.h"
#include "bmcv_util.h"

using namespace std;

static void pad_const(
        const unsigned char* in,
        unsigned char* out,
        int c,
        int w,
        int h,
        int srcstep,
        int pad_w_l,
        int pad_w_r,
        int pad_h_t,
        int pad_h_b,
        unsigned char val
        ) {
    auto PAD = [&](int start, int lines) {
        for (int i = start; i < start + lines; i++) {
            for (int j = 0; j < pad_w_l * c; j++) {
                out[(i + pad_h_t) * (w + pad_w_l + pad_w_r) * c + j] = val;
            }
            for (int j = 0; j < pad_w_r * c; j++) {
                out[(i + pad_h_t + 1) * (w + pad_w_l + pad_w_r) * c - j - 1] = val;
            }
            memcpy(out + (i + pad_h_t) * (w + pad_w_l + pad_w_r) * c + pad_w_l * c,
                   in + i * srcstep,
                   w * c);
        }
    };
    int threadNum = 5;//thread::hardware_concurrency();
    vector<pair<int, int>> param(threadNum);
    int lines = (h + threadNum - 1) / threadNum;
    int last_lines = h - lines * (threadNum - 1);
    for (int i = 0; i < threadNum - 1; i++) {
        param.push_back(make_pair(i * lines, lines));
    }
    param.push_back(make_pair((threadNum - 1) * lines, last_lines));
    vector<thread> threads;
    for (auto it : param)
        threads.push_back(std::thread(PAD, it.first, it.second));
    // add top and bottom pad
    int count = (w + pad_w_l + pad_w_r) * c;
    uint8x16_t vval = vdupq_n_u8(val);
    for (int i = 0; i < pad_h_t; i++) {
        unsigned char* daddr = out + i * (w + pad_w_l + pad_w_r) * c;
        int j = 0;
        for (; j < (count & -16) ; j += 16, daddr += 16)
            vst1q_u8(daddr, vval);
        for (; j < count; j++, daddr++)
            daddr[0] = val;
    }
    for (int i = 0; i < pad_h_b; i++) {
        unsigned char* daddr = out + (h + pad_h_t + i) * (w + pad_w_l + pad_w_r) * c;
        int j = 0;
        for (; j < (count & -16) ; j += 16, daddr += 16)
            vst1q_u8(daddr, vval);
        for (; j < count; j++, daddr++)
            daddr[0] = val;
    }
    for (auto &it : threads)
        it.join();
}

static void get_kernel_coord(
        const unsigned char* kernel,
        int kw,
        int kh,
        vector<bmPoint>& coords) {
    for (int i = 0; i < kh; i++) {
        for (int j = 0; j < kw; j++) {
            if (kernel[i * kw +j]) {
                coords.push_back({j, i});
            }
        }
    }
}

template<typename T>
struct MaxOp {
    T operator ()(const T a, const T b) const { return std::max(a, b); }
    int val = 0;
};

template<typename T>
struct MinOp {
    T operator ()(const T a, const T b) const { return std::min(a, b); }
    int val = 255;
};

template<class Op>
static void morph_cpu(
        const unsigned char* src,
        const unsigned char* kernel,
        unsigned char* dst,
        int kw,
        int kh,
        int srcstep,
        int dststep,
        int width,
        int height,
        int cn) {
    Op op;
    int pad_w_l = kw / 2;
    int pad_w_r = kw - 1 - pad_w_l;
    int pad_h_t = kh / 2;
    int pad_h_b = kh - 1 - pad_h_t;
    int sw = width + pad_w_l + pad_w_r;
    int sh = height + pad_h_t + pad_h_t;
    unsigned char* src_padded = new unsigned char [sw * sh * cn];
    pad_const(
            src,
            src_padded,
            cn,
            width,
            height,
            srcstep,
            pad_w_l,
            pad_w_r,
            pad_h_t,
            pad_h_b,
            op.val);
    vector<bmPoint> coords;
    get_kernel_coord(kernel, kw, kh, coords);
    const bmPoint* pt = coords.data();
    int nz = (int)coords.size();
    width *= cn;
    auto MORPH = [&](int start, int lines) {
        vector<unsigned char*> ptrs(nz);
        const unsigned char** kp = (const unsigned char**)(ptrs.data());
        const unsigned char* S = static_cast<const unsigned char*>(src_padded) + start * sw * cn;
        unsigned char* D = dst + start * dststep;
        while (lines--) {
            for (int k = 0; k < nz; k++) {
                kp[k] = S + pt[k].y * sw * cn + pt[k].x * cn;
            }
            int i = 0;
            for (; i <= width - 4; i += 4) {
                const unsigned char* sptr = kp[0] + i;
                unsigned char s0 = sptr[0], s1 = sptr[1], s2 = sptr[2], s3 = sptr[3];
                for (int k = 1; k < nz; k++ ) {
                    sptr = kp[k] + i;
                    s0 = op(s0, sptr[0]); s1 = op(s1, sptr[1]);
                    s2 = op(s2, sptr[2]); s3 = op(s3, sptr[3]);
                }
                D[i] = s0; D[i+1] = s1;
                D[i+2] = s2; D[i+3] = s3;
            }
            for (; i < width; i++) {
                unsigned char s0 = kp[0][i];
                for (int k = 1; k < nz; k++)
                    s0 = op(s0, kp[k][i]);
                D[i] = s0;
            }
            S += sw * cn;
            D += dststep;
        }
    };
    int threadNum = thread::hardware_concurrency();
    vector<pair<int, int>> param(threadNum);
    int lines = (height + threadNum - 1) / threadNum;
    int last_lines = height - lines * (threadNum - 1);
    param.clear();
    for (int i = 0; i < threadNum - 1; i++) {
        param.push_back(make_pair(i * lines, lines));
    }
    param.push_back(make_pair((threadNum - 1) * lines, last_lines));
    vector<thread> threads;
    for (auto it : param)
        threads.push_back(std::thread(MORPH, it.first, it.second));
    for (auto &it : threads)
        it.join();
}

int bmcv_cpu_morph(
        void* addr,
        int size
        ) {
    auto param = reinterpret_cast<bmcv_morph_param_t*>(*UINT64_PTR(addr));
    bmcpu_dev_flush_and_invalidate_dcache(
            VOID_PTR(*UINT64_PTR(addr)),
            VOID_PTR(*UINT64_PTR(addr) + sizeof(bmcv_morph_param_t)));
    int loop = (param->format == FORMAT_BGR_PLANAR ||
                param->format == FORMAT_RGB_PLANAR) ? 3 : 1;
    int ch = (param->format == FORMAT_BGR_PACKED ||
              param->format == FORMAT_RGB_PACKED) ? 3 : 1;
    bmcpu_dev_flush_and_invalidate_dcache(
            VOID_PTR(param->src_addr),
            VOID_PTR(param->src_addr + param->width * param->height * loop * ch));
    bmcpu_dev_flush_and_invalidate_dcache(
            VOID_PTR(param->kernel_addr),
            VOID_PTR(param->kernel_addr + param->kh * param->kw));
    for (int i = 0; i < loop; i++) {
        if (param->op)
            morph_cpu<MaxOp<unsigned char>>(
                    UINT8_PTR(param->src_addr) + i * param->height * param->srcstep,
                    UINT8_PTR(param->kernel_addr),
                    UINT8_PTR(param->dst_addr + i * param->height * param->dststep),
                    param->kw,
                    param->kh,
                    param->srcstep,
                    param->dststep,
                    param->width,
                    param->height,
                    ch);
        else
            morph_cpu<MinOp<unsigned char>>(
                    UINT8_PTR(param->src_addr) + i * param->height * param->srcstep,
                    UINT8_PTR(param->kernel_addr),
                    UINT8_PTR(param->dst_addr + i * param->height * param->dststep),
                    param->kw,
                    param->kh,
                    param->srcstep,
                    param->dststep,
                    param->width,
                    param->height,
                    ch);
    }
    bmcpu_dev_flush_dcache(
            VOID_PTR(param->dst_addr),
            VOID_PTR(param->dst_addr + param->width * param->height * loop * ch));
    return 0;
}

