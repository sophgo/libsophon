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
#define  BMCV_DESCALE(x,n)     (((x) + (1 << ((n)-1))) >> (n))
#define  BMCV_DECL_ALIGNED(x) __attribute__ ((aligned (x)))
#define BMCV_NEON 0

void lkpyramid_postprocess(
        int width,
        int height,
        unsigned char* prevPtr,
        unsigned char* nextPtr,
        short* derivPtr,
        bmcv_point2f_t* prevPts,
        bmcv_point2f_t* nextPts,
        int ptsNum,
        int kw,
        int kh,
        int level,
        int maxLevel,
        bool* status,
        int max_count,
        double eps
        ) {
    int cn = 1;
    int prevStep = width;
    int nextStep = width;
    int derivStep = width + 2 * kw;
    float minEigThreshold = 1e-4;
    bmcv_point2f_t halfWin = {(kw - 1) * 0.5f, (kh - 1) * 0.5f};
    int j;

    auto PROCESS = [&](int start_pt, int num) {
        short* _buf = new short [kh * kw * cn * 3];
        short* IWinBuf = _buf;
        short* derivIWinBuf = _buf + kh * kw * cn;
        for (int ptidx = start_pt; ptidx < start_pt + num; ptidx++) {
            bmcv_point2f_t prevPt = {prevPts[ptidx].x * (float)(1./(1 << level)),
                                     prevPts[ptidx].y * (float)(1./(1 << level))};
            bmcv_point2f_t nextPt;
            if ( level == maxLevel) {
                nextPt.x = prevPt.x;
                nextPt.y = prevPt.y;
            } else {
                nextPt.x = nextPts[ptidx].x * 2.f;
                nextPt.y = nextPts[ptidx].y * 2.f;
            }
            nextPts[ptidx].x = nextPt.x;
            nextPts[ptidx].y = nextPt.y;

            bmcv_point_t iprevPt, inextPt;
            prevPt.x -= halfWin.x;
            prevPt.y -= halfWin.y;
            iprevPt.x = floor(prevPt.x);
            iprevPt.y = floor(prevPt.y);

            if (iprevPt.x < -kw || iprevPt.x >= width ||
                iprevPt.y < -kh || iprevPt.y >= height ) {
                if (level == 0) {
                    if (status)
                        status[ptidx] = false;
                }
                continue;
            }
            float a = prevPt.x - iprevPt.x;
            float b = prevPt.y - iprevPt.y;
            const int W_BITS = 14, W_BITS1 = 14;
            const float FLT_SCALE = 1.f / (1 << 20);
            int iw00 = round((1.f - a) * (1.f - b) * (1 << W_BITS));
            int iw01 = round(a * (1.f - b) * (1 << W_BITS));
            int iw10 = round((1.f - a) * b * (1 << W_BITS));
            int iw11 = (1 << W_BITS) - iw00 - iw01 - iw10;
            int iA11 = 0, iA12 = 0, iA22 = 0;
            float A11, A12, A22;
#if BMCV_NEON
            float BMCV_DECL_ALIGNED(16) nA11[] = { 0, 0, 0, 0 }, nA12[] = { 0, 0, 0, 0 }, nA22[] = { 0, 0, 0, 0 };
            const int shifter1 = -(W_BITS - 5); //negative so it shifts right
            const int shifter2 = -(W_BITS);

            const int16x4_t d26 = vdup_n_s16((int16_t)iw00);
            const int16x4_t d27 = vdup_n_s16((int16_t)iw01);
            const int16x4_t d28 = vdup_n_s16((int16_t)iw10);
            const int16x4_t d29 = vdup_n_s16((int16_t)iw11);
            const int32x4_t q11 = vdupq_n_s32((int32_t)shifter1);
            const int32x4_t q12 = vdupq_n_s32((int32_t)shifter2);
#endif
            // extract the patch from the first image, compute covariation matrix of derivatives
            int x, y;
            for (y = 0; y < kh; y++) {
                const unsigned char* src = prevPtr + (y + iprevPt.y) * prevStep + iprevPt.x * cn;
                const short* xdsrc = derivPtr + (y + iprevPt.y) * derivStep + iprevPt.x;
                const short* ydsrc = derivPtr + (height + 2 * kh) * derivStep
                                              + (y + iprevPt.y) * derivStep + iprevPt.x;

                short* Iptr = IWinBuf + y * kw * cn;
                short* dIptr = derivIWinBuf + y * kw * cn * 2;
                x = 0;
#if BMCV_NEON
                for (; x <= kw * cn - 4; x += 4, xdsrc += 4, ydsrc += 4, dIptr += 4*2) {
                    uint8x8_t d0 = vld1_u8(&src[x]);
                    uint8x8_t d2 = vld1_u8(&src[x+cn]);
                    uint16x8_t q0 = vmovl_u8(d0);
                    uint16x8_t q1 = vmovl_u8(d2);

                    int32x4_t q5 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q0)), d26);
                    int32x4_t q6 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q1)), d27);

                    uint8x8_t d4 = vld1_u8(&src[x + prevStep]);
                    uint8x8_t d6 = vld1_u8(&src[x + prevStep + cn]);
                    uint16x8_t q2 = vmovl_u8(d4);
                    uint16x8_t q3 = vmovl_u8(d6);

                    int32x4_t q7 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q2)), d28);
                    int32x4_t q8 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q3)), d29);

                    q5 = vaddq_s32(q5, q6);
                    q7 = vaddq_s32(q7, q8);
                    q5 = vaddq_s32(q5, q7);

                    int16x4_t xd0d1 = vld1_s16(xdsrc);
                    int16x4_t yd0d1 = vld1_s16(ydsrc);
                    int16x4_t xd2d3 = vld1_s16(&xdsrc[cn]);
                    int16x4_t yd2d3 = vld1_s16(&ydsrc[cn]);

                    q5 = vqrshlq_s32(q5, q11);

                    int32x4_t q4 = vmull_s16(xd0d1, d26);
                    q6 = vmull_s16(yd0d1, d26);

                    int16x4_t nd0 = vmovn_s32(q5);

                    q7 = vmull_s16(xd2d3, d27);
                    q8 = vmull_s16(yd2d3, d27);

                    vst1_s16(&Iptr[x], nd0);

                    int16x4_t xd4d5 = vld1_s16(&xdsrc[derivStep]);
                    int16x4_t xd6d7 = vld1_s16(&xdsrc[derivStep + cn]);
                    int16x4_t yd4d5 = vld1_s16(&ydsrc[derivStep]);
                    int16x4_t yd6d7 = vld1_s16(&ydsrc[derivStep + cn]);

                    q4 = vaddq_s32(q4, q7);
                    q6 = vaddq_s32(q6, q8);

                    q7 = vmull_s16(xd4d5, d28);
                    int32x4_t q14 = vmull_s16(yd4d5, d28);
                    q8 = vmull_s16(xd6d7, d29);
                    int32x4_t q15 = vmull_s16(yd6d7, d29);

                    q7 = vaddq_s32(q7, q8);
                    q14 = vaddq_s32(q14, q15);

                    q4 = vaddq_s32(q4, q7);
                    q6 = vaddq_s32(q6, q14);

                    float32x4_t nq0 = vld1q_f32(nA11);
                    float32x4_t nq1 = vld1q_f32(nA12);
                    float32x4_t nq2 = vld1q_f32(nA22);

                    q4 = vqrshlq_s32(q4, q12);
                    q6 = vqrshlq_s32(q6, q12);

                    q7 = vmulq_s32(q4, q4);
                    q8 = vmulq_s32(q4, q6);
                    q15 = vmulq_s32(q6, q6);

                    nq0 = vaddq_f32(nq0, vcvtq_f32_s32(q7));
                    nq1 = vaddq_f32(nq1, vcvtq_f32_s32(q8));
                    nq2 = vaddq_f32(nq2, vcvtq_f32_s32(q15));

                    vst1q_f32(nA11, nq0);
                    vst1q_f32(nA12, nq1);
                    vst1q_f32(nA22, nq2);

                    int16x4_t d8 = vmovn_s32(q4);
                    int16x4_t d12 = vmovn_s32(q6);

                    int16x4x2_t d8d12;
                    d8d12.val[0] = d8; d8d12.val[1] = d12;
                    vst2_s16(dIptr, d8d12);
                }
#endif
                for (; x < kw*cn; x++, xdsrc++, ydsrc++, dIptr += 2) {
                    int ival = BMCV_DESCALE(src[x]*iw00 + src[x+cn]*iw01 +
                                          src[x+prevStep]*iw10 + src[x+prevStep+cn]*iw11, W_BITS1-5);
                    int ixval = BMCV_DESCALE(xdsrc[0]*iw00 + xdsrc[cn]*iw01 +
                                           xdsrc[derivStep]*iw10 + xdsrc[derivStep+cn]*iw11, W_BITS1);
                    int iyval = BMCV_DESCALE(ydsrc[0]*iw00 + ydsrc[cn]*iw01 + ydsrc[derivStep]*iw10 +
                                           ydsrc[derivStep+cn]*iw11, W_BITS1);
                    Iptr[x] = (short)ival;
                    dIptr[0] = (short)ixval;
                    dIptr[1] = (short)iyval;

                    iA11 += ixval * ixval;
                    iA12 += ixval * iyval;
                    iA22 += iyval * iyval;
                }
            }
#if BMCV_NEON
            iA11 += nA11[0] + nA11[1] + nA11[2] + nA11[3];
            iA12 += nA12[0] + nA12[1] + nA12[2] + nA12[3];
            iA22 += nA22[0] + nA22[1] + nA22[2] + nA22[3];
#endif
            A11 = iA11 * FLT_SCALE;
            A12 = iA12 * FLT_SCALE;
            A22 = iA22 * FLT_SCALE;

            float D = A11 * A22 - A12 * A12;
            float minEig = (A22 + A11 - std::sqrt((A11-A22)*(A11-A22) +
                            4.f*A12*A12))/(2*kw*kh);

            if (minEig < minEigThreshold || D < FLT_EPSILON) {
                if (level == 0 && status)
                    status[ptidx] = false;
                continue;
            }
            D = 1.f/D;
            nextPt.x -= halfWin.x;
            nextPt.y -= halfWin.y;
            bmcv_point2f_t prevDelta = {0, 0};

            for (j = 0; j < max_count; j++) {
                inextPt.x = floor(nextPt.x);
                inextPt.y = floor(nextPt.y);

                if (inextPt.x < -kw || inextPt.x >= width ||
                    inextPt.y < -kh || inextPt.y >= height) {
                    if (level == 0 && status)
                        status[ptidx] = false;
                    break;
                }
                a = nextPt.x - inextPt.x;
                b = nextPt.y - inextPt.y;
                iw00 = round((1.f - a) * (1.f - b) * (1 << W_BITS));
                iw01 = round(a * (1.f - b) * (1 << W_BITS));
                iw10 = round((1.f - a) * b * (1 << W_BITS));
                iw11 = (1 << W_BITS) - iw00 - iw01 - iw10;
                int ib1 = 0, ib2 = 0;
                float b1, b2;
#if BMCV_NEON
                float BMCV_DECL_ALIGNED(16) nB1[] = { 0,0,0,0 }, nB2[] = { 0,0,0,0 };
                const int16x4_t d26_2 = vdup_n_s16((int16_t)iw00);
                const int16x4_t d27_2 = vdup_n_s16((int16_t)iw01);
                const int16x4_t d28_2 = vdup_n_s16((int16_t)iw10);
                const int16x4_t d29_2 = vdup_n_s16((int16_t)iw11);
#endif
                for (y = 0; y < kh; y++) {
                    const unsigned char* Jptr = nextPtr + (y + inextPt.y) * nextStep + inextPt.x * cn;
                    const short* Iptr = IWinBuf + y * kw * cn;
                    const short* dIptr = derivIWinBuf + y * kw * cn * 2;
                    x = 0;
#if BMCV_NEON
                    for (; x <= kw * cn - 8; x += 8, dIptr += 8*2) {
                        uint8x8_t d0 = vld1_u8(&Jptr[x]);
                        uint8x8_t d2 = vld1_u8(&Jptr[x+cn]);
                        uint8x8_t d4 = vld1_u8(&Jptr[x+nextStep]);
                        uint8x8_t d6 = vld1_u8(&Jptr[x+nextStep+cn]);

                        uint16x8_t q0 = vmovl_u8(d0);
                        uint16x8_t q1 = vmovl_u8(d2);
                        uint16x8_t q2 = vmovl_u8(d4);
                        uint16x8_t q3 = vmovl_u8(d6);

                        int32x4_t nq4 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q0)), d26_2);
                        int32x4_t nq5 = vmull_s16(vget_high_s16(vreinterpretq_s16_u16(q0)), d26_2);

                        int32x4_t nq6 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q1)), d27_2);
                        int32x4_t nq7 = vmull_s16(vget_high_s16(vreinterpretq_s16_u16(q1)), d27_2);

                        int32x4_t nq8 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q2)), d28_2);
                        int32x4_t nq9 = vmull_s16(vget_high_s16(vreinterpretq_s16_u16(q2)), d28_2);

                        int32x4_t nq10 = vmull_s16(vget_low_s16(vreinterpretq_s16_u16(q3)), d29_2);
                        int32x4_t nq11 = vmull_s16(vget_high_s16(vreinterpretq_s16_u16(q3)), d29_2);

                        nq4 = vaddq_s32(nq4, nq6);
                        nq5 = vaddq_s32(nq5, nq7);
                        nq8 = vaddq_s32(nq8, nq10);
                        nq9 = vaddq_s32(nq9, nq11);

                        int16x8_t q6 = vld1q_s16(&Iptr[x]);

                        nq4 = vaddq_s32(nq4, nq8);
                        nq5 = vaddq_s32(nq5, nq9);

                        nq8 = vmovl_s16(vget_high_s16(q6));
                        nq6 = vmovl_s16(vget_low_s16(q6));

                        nq4 = vqrshlq_s32(nq4, q11);
                        nq5 = vqrshlq_s32(nq5, q11);

                        int16x8x2_t q0q1 = vld2q_s16(dIptr);
                        float32x4_t nB1v = vld1q_f32(nB1);
                        float32x4_t nB2v = vld1q_f32(nB2);

                        nq4 = vsubq_s32(nq4, nq6);
                        nq5 = vsubq_s32(nq5, nq8);

                        int32x4_t nq2 = vmovl_s16(vget_low_s16(q0q1.val[0]));
                        int32x4_t nq3 = vmovl_s16(vget_high_s16(q0q1.val[0]));

                        nq7 = vmovl_s16(vget_low_s16(q0q1.val[1]));
                        nq8 = vmovl_s16(vget_high_s16(q0q1.val[1]));

                        nq9 = vmulq_s32(nq4, nq2);
                        nq10 = vmulq_s32(nq5, nq3);

                        nq4 = vmulq_s32(nq4, nq7);
                        nq5 = vmulq_s32(nq5, nq8);

                        nq9 = vaddq_s32(nq9, nq10);
                        nq4 = vaddq_s32(nq4, nq5);

                        nB1v = vaddq_f32(nB1v, vcvtq_f32_s32(nq9));
                        nB2v = vaddq_f32(nB2v, vcvtq_f32_s32(nq4));

                        vst1q_f32(nB1, nB1v);
                        vst1q_f32(nB2, nB2v);
                    }
#endif
                    for (; x < kw*cn; x++, dIptr += 2) {
                        int diff = BMCV_DESCALE(Jptr[x]*iw00 + Jptr[x+cn]*iw01 +
                                                Jptr[x+nextStep]*iw10 + Jptr[x+nextStep+cn]*iw11,
                                                 W_BITS1-5) - Iptr[x];
                        ib1 += diff * dIptr[0];
                        ib2 += diff * dIptr[1];
                    }
                }
#if BMCV_NEON
                ib1 += (float)(nB1[0] + nB1[1] + nB1[2] + nB1[3]);
                ib2 += (float)(nB2[0] + nB2[1] + nB2[2] + nB2[3]);
#endif
                b1 = ib1 * FLT_SCALE;
                b2 = ib2 * FLT_SCALE;
                bmcv_point2f_t delta = {(float)((A12*b2 - A22*b1) * D),
                                        (float)((A12*b1 - A11*b2) * D)};
                nextPt.x += delta.x;
                nextPt.y += delta.y;
                nextPts[ptidx].x = nextPt.x + halfWin.x;
                nextPts[ptidx].y = nextPt.y + halfWin.y;
                if ((delta.x * delta.x + delta.y * delta.y) <= eps)
                    break;
                if (j > 0 && fabs(delta.x + prevDelta.x) < 0.01 &&
                    fabs(delta.y + prevDelta.y) < 0.01) {
                    nextPts[ptidx].x -= delta.x * 0.5f;
                    nextPts[ptidx].y -= delta.y * 0.5f;
                    break;
                }
                prevDelta.x = delta.x;
                prevDelta.y = delta.y;
            }
        }
        delete [] _buf;
    };
    int threadNumMax = thread::hardware_concurrency();
    int numPerThread = (ptsNum + threadNumMax - 1) / threadNumMax;
    int threadNum = (ptsNum + numPerThread - 1) / numPerThread;
    threadNum = threadNum > threadNumMax ? threadNumMax : threadNum;
    int lastNum = ptsNum - (threadNum - 1) * numPerThread;
    vector<pair<int, int>> param(threadNum);
    for (int i = 0; i < threadNum - 1; i++) {
        param.push_back(make_pair(i * numPerThread, numPerThread));
    }
    param.push_back(make_pair((threadNum - 1) * numPerThread, lastNum));
    vector<thread> threads;
    for (auto &it : param)
        threads.push_back(std::thread(PROCESS, it.first, it.second));
    for (auto &it : threads)
        it.join();
}

int bmcv_cpu_lkpyramid(
        void* addr,
        int size
        ) {
    auto param = reinterpret_cast<bmcv_lkpyramid_param_t*>(*UINT64_PTR(addr));
    // invalidate cache: lk-param and prev-pts
    bmcpu_dev_flush_and_invalidate_dcache(
            VOID_PTR(*UINT64_PTR(addr)),
            VOID_PTR(*UINT64_PTR(addr) + sizeof(bmcv_lkpyramid_param_t) +
                                         sizeof(bmcv_point2f_t) * param->ptsNum));
    vector<int> wPyr(param->maxLevel + 1);
    vector<int> hPyr(param->maxLevel + 1);
    wPyr[0] = param->width;
    hPyr[0] = param->height;
    for (int i = 1; i <= param->maxLevel; i++) {
        wPyr[i] = (wPyr[i - 1] + 1) / 2;
        hPyr[i] = (hPyr[i - 1] + 1) / 2;
    }
    // bmcpu_dev_log(0, "width = %d\n", param->width);
    // bmcpu_dev_log(0, "height = %d\n", param->height);
    // bmcpu_dev_log(0, "num = %d\n", param->ptsNum);
    bmcv_point2f_t* prevPtsPtr = reinterpret_cast<bmcv_point2f_t*>(*UINT64_PTR(addr) + sizeof(bmcv_lkpyramid_param_t));
    bmcv_point2f_t* nextPtsPtr = reinterpret_cast<bmcv_point2f_t*>(*UINT64_PTR(addr) + sizeof(bmcv_lkpyramid_param_t)
                                                                                     + sizeof(bmcv_point2f_t) * param->ptsNum);
    bool* statusPtr = reinterpret_cast<bool*>(*UINT64_PTR(addr) + sizeof(bmcv_lkpyramid_param_t)
                                                                + sizeof(bmcv_point2f_t) * param->ptsNum * 2);
    memset(statusPtr, 1, param->ptsNum);
    for (int i = param->maxLevel; i >= 0; i--) {
        bmcpu_dev_flush_and_invalidate_dcache(
                VOID_PTR(param->prevPyrAddr[i]),
                VOID_PTR(param->prevPyrAddr[i] + wPyr[i] * hPyr[i]));
        bmcpu_dev_flush_and_invalidate_dcache(
                VOID_PTR(param->nextPyrAddr[i]),
                VOID_PTR(param->nextPyrAddr[i] + wPyr[i] * hPyr[i]));
        bmcpu_dev_flush_and_invalidate_dcache(
                VOID_PTR(param->gradAddr[i]),
                VOID_PTR(param->gradAddr[i] + wPyr[i] * hPyr[i] * sizeof(short)));
        lkpyramid_postprocess(
                wPyr[i],
                hPyr[i],
                UINT8_PTR(param->prevPyrAddr[i]),
                UINT8_PTR(param->nextPyrAddr[i]),
                INT16_PTR(param->gradAddr[i]) + param->winH * (wPyr[i] + 2 * param->winW) + param->winW,
                prevPtsPtr,
                nextPtsPtr,
                param->ptsNum,
                param->winW,
                param->winH,
                i,
                param->maxLevel,
                statusPtr,
                param->maxCount,
                param->eps);
    }
    // flush next pts and status
    bmcpu_dev_flush_dcache(
            VOID_PTR(*UINT64_PTR(addr) + sizeof(bmcv_lkpyramid_param_t) +
                                         sizeof(bmcv_point2f_t) * param->ptsNum),
            VOID_PTR(*UINT64_PTR(addr) + sizeof(bmcv_lkpyramid_param_t) +
                                         sizeof(bmcv_point2f_t) * param->ptsNum * 2 + param->ptsNum));
    return 0;
}

