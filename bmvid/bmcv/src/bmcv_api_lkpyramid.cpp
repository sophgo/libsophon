#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "device_mem_allocator.h"
#include "pcie_cpu/bmcv_api_struct.h"
#include <float.h>
#include <memory>
#include <vector>
#include <cmath>

#ifdef __linux__
#include <sys/time.h>
#endif
#include <iostream>

using namespace std;
#define  BMCV_DESCALE(x,n)     (((x) + (1 << ((n)-1))) >> (n))

static bm_status_t bmcv_lkpyramid_check(
        bm_handle_t handle,
        bm_image prev,
        bm_image next) {
    if (handle == NULL) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (prev.width != next.width || prev.height != next.height) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "previous image and next image size should be same!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (prev.width + 5 - 1 >= 2048) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "image width is too large!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (prev.image_format != FORMAT_GRAY || next.image_format != FORMAT_GRAY) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "Image format only support GRAY\n");
        return BM_NOT_SUPPORTED;
    }
    if (prev.data_type != DATA_TYPE_EXT_1N_BYTE ||
        next.data_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

void lkpyramid_postprocess(
        int cn,
        int width,
        int height,
        int prevStep,
        int nextStep,
        int derivStep,
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
        bmcv_term_criteria_t criteria
        ) {
    float minEigThreshold = 1e-4;
    bmcv_point2f_t halfWin = {(kw - 1) * 0.5f, (kh - 1) * 0.5f};

    int j;
    short* _buf = new short [kh * kw * cn * 3];
    short* IWinBuf = _buf;
    short* derivIWinBuf = _buf + kh * kw * cn;

    for (int ptidx = 0; ptidx < ptsNum; ptidx++) {
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

        float iA11 = 0, iA12 = 0, iA22 = 0;
        float A11, A12, A22;
        // extract the patch from the first image, compute covariation matrix of derivatives
        int x, y;
        for (y = 0; y < kh; y++) {
            const unsigned char* src = prevPtr + (y + iprevPt.y) * prevStep + iprevPt.x * cn;
            const short* xdsrc = derivPtr + (y + iprevPt.y) * derivStep + iprevPt.x *cn;
            const short* ydsrc = derivPtr + (height + 2 * kh) * derivStep
                                          + (y + iprevPt.y) * derivStep + iprevPt.x;

            short* Iptr = IWinBuf + y * kw * cn;
            short* dIptr = derivIWinBuf + y * kw * cn * 2;
            x = 0;
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

                iA11 += (float)(ixval * ixval);
                iA12 += (float)(ixval * iyval);
                iA22 += (float)(iyval * iyval);
            }
        }
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

        for (j = 0; j < criteria.max_count; j++) {
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
            for (y = 0; y < kh; y++) {
                const unsigned char* Jptr = nextPtr + (y + inextPt.y) * nextStep + inextPt.x * cn;
                const short* Iptr = IWinBuf + y * kw * cn;
                const short* dIptr = derivIWinBuf + y * kw * cn * 2;
                x = 0;
                for (; x < kw*cn; x++, dIptr += 2) {
                    int diff = BMCV_DESCALE(Jptr[x]*iw00 + Jptr[x+cn]*iw01 +
                                            Jptr[x+nextStep]*iw10 + Jptr[x+nextStep+cn]*iw11,
                                             W_BITS1-5) - Iptr[x];
                    ib1 += diff * dIptr[0];
                    ib2 += diff * dIptr[1];
                }
            }
            b1 = ib1 * FLT_SCALE;
            b2 = ib2 * FLT_SCALE;
            bmcv_point2f_t delta = {(float)((A12*b2 - A22*b1) * D),
                                    (float)((A12*b1 - A11*b2) * D)};
            nextPt.x += delta.x;
            nextPt.y += delta.y;
            nextPts[ptidx].x = nextPt.x + halfWin.x;
            nextPts[ptidx].y = nextPt.y + halfWin.y;

            if ((delta.x * delta.x + delta.y * delta.y) <= criteria.epsilon)
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
}

struct LKPlan {
    int maxLevel;
    int winW;
    int winH;
    bm_device_mem_t kernelMem;
    vector<int> wPyr;
    vector<int> hPyr;
    vector<bm_device_mem_t> gradMem;
    vector<bm_device_mem_t> prevPyrMem;
    vector<bm_device_mem_t> nextPyrMem;
    bool kFlag = false;
    vector<bool> gradFlag;
    vector<bool> prevFlag;
    vector<bool> nextFlag;
    LKPlan(int maxLevel):
            maxLevel(maxLevel),
            wPyr(maxLevel + 1),
            hPyr(maxLevel + 1),
            gradMem(maxLevel + 1),
            prevPyrMem(maxLevel + 1),
            nextPyrMem(maxLevel + 1),
            gradFlag(maxLevel + 1, false),
            prevFlag(maxLevel + 1, false),
            nextFlag(maxLevel + 1, false) {}
    void release(bm_handle_t handle) {
        if (kFlag) bm_free_device(handle, kernelMem);
        for (int i = 0; i < maxLevel + 1; i++) {
            if (gradFlag[i]) bm_free_device(handle, gradMem[i]);
            if (prevFlag[i]) bm_free_device(handle, prevPyrMem[i]);
            if (nextFlag[i]) bm_free_device(handle, nextPyrMem[i]);
        }
    }
};

bm_status_t bmcv_image_lkpyramid_create_plan(
        bm_handle_t handle,
        void*& plan,
        int width,
        int height,
        int winW,
        int winH,
        int maxLevel) {
    auto P = new LKPlan(maxLevel);
    P->winW = winW;
    P->winH = winH;
    P->maxLevel = maxLevel;
    if (maxLevel > 5) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "Not supported maxLevel greater 5\n");
        return BM_NOT_SUPPORTED;
    }
    // in order to use same size kernel, gradient kernel pad 0
    int ksize = 5 * 5 * 3;  // pyramid-down + x-grad + y-grad
    #ifdef __linux__
//    float kernel[ksize] = {
    float kernel[75] = {
    #else
    float kernel[75] = {
    #endif
            // x-grad kernel
            0,  0,  0,  0, 0,
            0, -3,  0,  3, 0,
            0,-10,  0, 10, 0,
            0, -3,  0,  3, 0,
            0,  0,  0,  0, 0,
            // y-grad kernel
            0,  0,  0,  0, 0,
            0, -3,-10, -3, 0,
            0,  0,  0,  0, 0,
            0,  3, 10,  3, 0,
            0,  0,  0,  0, 0,
            // pyramid-down kernel
            1,  4,  6,  4, 1,
            4, 16, 24, 16, 4,
            6, 24, 36, 24, 6,
            4, 16, 24, 16, 4,
            1,  4,  6,  4, 1};
    bm_status_t ret = bm_malloc_device_byte(handle, &(P->kernelMem), ksize * sizeof(float));
    if (BM_SUCCESS != ret) {
        return ret;
    }
    P->kFlag = true;
    ret = bm_memcpy_s2d(handle, P->kernelMem, kernel);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    // malloc output device mem
    P->wPyr[0] = width;
    P->hPyr[0] = height;
    ret = bm_malloc_device_byte(handle, &(P->gradMem[0]), (width + winW * 2) * (height + winH * 2) * 2 * sizeof(short));
    if (BM_SUCCESS != ret) {
        return ret;
    }
    P->gradFlag[0] = true;
    for (int i = 1; i <= maxLevel; i++) {
        P->wPyr[i] = (P->wPyr[i - 1] + 1) / 2;
        P->hPyr[i] = (P->hPyr[i - 1] + 1) / 2;
        ret = bm_malloc_device_byte(handle, &(P->gradMem[i]), (P->wPyr[i] + winW * 2) * (P->hPyr[i] + winH * 2) * 2 * sizeof(short));
        if (BM_SUCCESS != ret) {
            return ret;
        }
        P->gradFlag[i] = true;
        ret = bm_malloc_device_byte(handle, &(P->prevPyrMem[i]), P->wPyr[i] * P->hPyr[i] * sizeof(char));
        if (BM_SUCCESS != ret) {
            return ret;
        }
        P->prevFlag[i] = true;
        ret = bm_malloc_device_byte(handle, &(P->nextPyrMem[i]), P->wPyr[i] * P->hPyr[i] * sizeof(char));
        if (BM_SUCCESS != ret) {
            return ret;
        }
        P->nextFlag[i] = true;
    }
    plan = P;
    return BM_SUCCESS;
}

bm_status_t bmcv_image_lkpyramid_execute_bm1684(
        bm_handle_t handle,
        void* plan,
        bm_image prevImg,
        bm_image nextImg,
        int ptsNum,
        bmcv_point2f_t* prevPts,
        bmcv_point2f_t* nextPts,
        bool* status,
        bmcv_term_criteria_t criteria) {
    if ((criteria.type & 1) == 0)
        criteria.max_count = 30;
    else
        #ifdef __linux__
        criteria.max_count = std::min(std::max(criteria.max_count, 0), 100);
        #else
        criteria.max_count = (std::min)((std::max)(criteria.max_count, 0), 100);
        #endif
    if ((criteria.type & 2) == 0)
        criteria.epsilon = 0.01;
    else
        #ifdef __linux__
        criteria.epsilon = std::min(std::max(criteria.epsilon, 0.), 10.);
        #else
        criteria.epsilon = (std::min)((std::max)(criteria.epsilon, 0.), 10.);
        #endif
    criteria.epsilon *= criteria.epsilon;
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_lkpyramid_check(handle, prevImg, nextImg);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    auto P = reinterpret_cast<LKPlan*>(plan);
    int width = prevImg.width;
    int height = prevImg.height;
    bm_image_get_device_mem(prevImg, &(P->prevPyrMem[0]));
    bm_image_get_device_mem(nextImg, &(P->nextPyrMem[0]));
    // construct and send api for previous image
    // struct timeval t1, t2;
    // gettimeofday(&t1, NULL);
    int stride_i;
    bm_image_get_stride(prevImg, &stride_i);
    bm_api_cv_lkpyramid_t api;
    for (int i = 0; i <= P->maxLevel; i++) {
        api.output_addr[i * 3 + 0] = bm_mem_get_device_addr(P->gradMem[i]);
        api.output_addr[i * 3 + 1] = i < P->maxLevel ? bm_mem_get_device_addr(P->prevPyrMem[i + 1]) : 0;
        api.output_addr[i * 3 + 2] = i < P->maxLevel ? bm_mem_get_device_addr(P->nextPyrMem[i + 1]) : 0;
    }
    api.kernel_addr = bm_mem_get_device_addr(P->kernelMem);
    api.prev_input_addr = bm_mem_get_device_addr(P->prevPyrMem[0]);
    api.next_input_addr = bm_mem_get_device_addr(P->nextPyrMem[0]);
    api.width = width;
    api.height = height;
    api.stride_i = stride_i;
    api.winW = P->winW;
    api.winH = P->winH;
    api.max_level = P->maxLevel;
    ret = bm_send_api(handle,  BM_API_ID_CV_LKPYRAMID, (uint8_t*)&api, sizeof(api));
    if (BM_SUCCESS != ret) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "previous-image level send api error\n");
        return ret;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
        bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "previous-image level sync api error\n");
        return ret;
    }
    // gettimeofday(&t2, NULL);
    // cout << "pyr and grad TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "um" << endl;
    // post-process
    int process_id = get_cpu_process_id(handle);
    if (process_id != -1) {
        int timeout = -1;
        int param_len = sizeof(bmcv_lkpyramid_param_t) + ptsNum * sizeof(bmcv_point2f_t) * 2 + ptsNum;
        char* param = new char [param_len];
        bmcv_lkpyramid_param_t* lkpyr_p = reinterpret_cast<bmcv_lkpyramid_param_t*>(param);
        lkpyr_p->width = width;
        lkpyr_p->height = height;
        lkpyr_p->winW = P->winW;
        lkpyr_p->winH = P->winH;
        lkpyr_p->ptsNum = ptsNum;
        lkpyr_p->maxLevel = P->maxLevel;
        lkpyr_p->maxCount = criteria.max_count;
        lkpyr_p->eps = criteria.epsilon;
        for (int i = 0; i <= P->maxLevel; i++) {
            lkpyr_p->prevPyrAddr[i] = get_mapped_addr(handle, &(P->prevPyrMem[i]));
            lkpyr_p->nextPyrAddr[i] = get_mapped_addr(handle, &(P->nextPyrMem[i]));
            lkpyr_p->gradAddr[i] = get_mapped_addr(handle, &(P->gradMem[i]));
        }
        memcpy(param + sizeof(bmcv_lkpyramid_param_t),
               prevPts,
               ptsNum * sizeof(bmcv_point2f_t));
        // copy param to device memory
        DeviceMemAllocator allocator(handle);
        bm_device_mem_t pmem;
        try {
            pmem = allocator.map_input_to_device<char>(
                              bm_mem_from_system(param),
                              param_len);
        } catch (bm_status_t ret) {
            delete [] param;
            return ret;
        }
        u64 param_addr_mapped = get_mapped_addr(handle, &pmem);
        int ret = bmcpu_exec_function_ext(
                handle,
                process_id,
                (char*)"bmcv_cpu_lkpyramid",
                (void*)&param_addr_mapped,
                sizeof(void*),
                1,
                timeout);
        if (ret != 0) {
            delete [] param;
            bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "exec function failed! return %d\r\n", ret);
            return BM_ERR_FAILURE;
        }
        // get next pts and status
        char* outPtr = new char [sizeof(bmcv_point2f_t) * ptsNum + ptsNum];
        u64 nextPtsAddr = bm_mem_get_device_addr(pmem) + sizeof(bmcv_lkpyramid_param_t) +
                                                         sizeof(bmcv_point2f_t) * ptsNum;
        bm_device_mem_t outMem = bm_mem_from_device(nextPtsAddr, sizeof(bmcv_point2f_t) * ptsNum + ptsNum);
        bm_memcpy_d2s(handle, outPtr, outMem);
        memcpy(nextPts, outPtr, sizeof(bmcv_point2f_t) * ptsNum);
        memcpy(status, outPtr + sizeof(bmcv_point2f_t) * ptsNum, ptsNum);
        delete [] outPtr;
        delete [] param;
    } else {
        int cn = 1;
        vector<shared_ptr<short*>> derivPtr(P->maxLevel + 1);
        vector<shared_ptr<unsigned char*>> prevPyr(P->maxLevel + 1);
        vector<shared_ptr<unsigned char*>> nextPyr(P->maxLevel + 1);
        memset(status, 1, ptsNum);
        for (int i = 0; i <= P->maxLevel; i++) {
            derivPtr[i] = make_shared<short*>(new short [(P->wPyr[i] + 2 * P->winW) * (P->hPyr[i] + 2 * P->winH) * cn * 2]);
            prevPyr[i] = make_shared<unsigned char*>(new unsigned char [P->wPyr[i] * P->hPyr[i]]);
            nextPyr[i] = make_shared<unsigned char*>(new unsigned char [P->wPyr[i] * P->hPyr[i]]);
            ret = bm_memcpy_d2s(handle, *derivPtr[i].get(), P->gradMem[i]);
            if (BM_SUCCESS != ret) {
                bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "lkpyramid grad d2s error\n");
                return ret;
            }
            ret = bm_memcpy_d2s(handle, *prevPyr[i].get(), P->prevPyrMem[i]);
            if (BM_SUCCESS != ret) {
                bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "lkpyramid prevPyrMem d2s error\n");
                return ret;
            }
            ret = bm_memcpy_d2s(handle, *nextPyr[i].get(), P->nextPyrMem[i]);
            if (BM_SUCCESS != ret) {
                bmlib_log("LKPYRAMID", BMLIB_LOG_ERROR, "lkpyramid nextPyrMem d2s error\n");
                return ret;
            }
        }
        for (int level = P->maxLevel; level >= 0; level--) {
            lkpyramid_postprocess(
                    cn,
                    P->wPyr[level],
                    P->hPyr[level],
                    P->wPyr[level],  // prevStep
                    P->wPyr[level],  // nextStep
                    P->wPyr[level] + 2 * P->winW,
                    *prevPyr[level].get(),
                    *nextPyr[level].get(),
                    *derivPtr[level].get() + P->winH * (P->wPyr[level] + 2 * P->winW) + P->winW,
                    prevPts,
                    nextPts,
                    ptsNum,
                    P->winW,
                    P->winH,
                    level,
                    P->maxLevel,
                    status,
                    criteria);
        }
    }
    return BM_SUCCESS;
}

void bmcv_image_lkpyramid_destroy_plan(bm_handle_t handle, void* plan) {
    auto P = reinterpret_cast<LKPlan*>(plan);
    P->release(handle);
    delete P;
}

bm_status_t bmcv_image_lkpyramid_execute(
        bm_handle_t handle,
        void* plan,
        bm_image prevImg,
        bm_image nextImg,
        int ptsNum,
        bmcv_point2f_t* prevPts,
        bmcv_point2f_t* nextPts,
        bool* status,
        bmcv_term_criteria_t criteria) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_image_lkpyramid_execute_bm1684(handle, plan, prevImg, nextImg, ptsNum, prevPts, nextPts, status, criteria);
        break;

      case BM1684X:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

