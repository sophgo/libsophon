#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>

bm_status_t bmcv_calc_hist(bm_handle_t handle,
                           bm_device_mem_t input,
                           bm_device_mem_t output,
                           int C,
                           int H,
                           int W,
                           const int *channels,
                           int dims,
                           const int *histSizes,
                           const float *ranges,
                           int inputDtype) {
    if (dims <= 0 || dims > 3)
        return BM_ERR_PARAM;
    bm_status_t ret = BM_SUCCESS;
    int total = 1;
    for (int i = 0; i < dims; ++i)
        total *= histSizes[i];
    bm_api_cv_calc_hist_index_t api[3];
    int dsize = inputDtype == 0 ? 4 : 1;
    int imageSize = H * W;
    bm_device_mem_t idxDev[3];
    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    for (int i = 0; i < dims; ++i) {
        BM_CHECK_RET(bm_malloc_device_byte(handle, &idxDev[i], imageSize * 4));
        api[i].Xaddr = bm_mem_get_device_addr(input) + channels[i] * imageSize * dsize;
        api[i].Yaddr = bm_mem_get_device_addr(idxDev[i]);
        api[i].a = static_cast<float>(histSizes[i]) / (ranges[i * 2 + 1] - ranges[2 * i]);
        api[i].b = -ranges[2 * i] * api[i].a;
        api[i].len = imageSize;
        api[i].xdtype = inputDtype;
        api[i].upper = static_cast<float>(histSizes[i]);

        switch (chipid)
        {
            case 0x1684:
                BM_CHECK_RET(bm_send_api(handle,
                             BM_API_ID_CV_CALC_HIST_INDEX,
                             reinterpret_cast<uint8_t *>(&api[i]),
                             sizeof(api[i])));
                break;

            case BM1684X:
                BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_calc_hist", &api, sizeof(api)));
                // BM_CHECK_RET(tpu_kernel_launch_sync(handle, "cv_calc_hist", &api[i], sizeof(api[i])));
                break;

            default:
                printf("ChipID is NOT supported\n");
                break;
        }
    }

    switch (chipid)
    {
        case 0x1684:
            BM_CHECK_RET(bm_sync_api(handle));
            break;

        case BM1684X:
            break;
    }
    float *outputHost = new float[total];
    memset(outputHost, 0x0, total * 4);
    float *idxHost[3][2];
    int idx = 0;
    int len = 0;
    float **outPtrs_2 = nullptr;
    float ***outPtrs_3 = nullptr;
    switch (dims) {
        case 2: {
            float *outIter = outputHost;
            outPtrs_2 = new float *[histSizes[0]];
            for (int i = 0; i < histSizes[0]; ++i) {
                outPtrs_2[i] = outIter;
                outIter += histSizes[1];
            }
            break;
        }
        case 3: {
            float *outIter = outputHost;
            outPtrs_3 = new float **[histSizes[0]];
            for (int i = 0; i < histSizes[0]; ++i) {
                outPtrs_3[i] = new float *[histSizes[1]];
                for (int j = 0; j < histSizes[1]; ++j) {
                    outPtrs_3[i][j] = outIter;
                    outIter += histSizes[2];
                }
            }
            break;
        }
        default:
            break;
    }
    auto func = [&]() {
        float *tmp0 = idxHost[0][idx];
        float *tmp1 = idxHost[1][idx];
        float *tmp2 = idxHost[2][idx];
        switch (dims) {
            case 1:
                for (int i = 0; i < len; ++i)
                    if (tmp0[i] >= 0.f)
                        ++outputHost[static_cast<int>(tmp0[i])];
                break;
            case 2:
                for (int i = 0; i < len; ++i)
                    if (tmp0[i] >= 0.f && tmp1[i] >= 0.f)
                        ++outPtrs_2[static_cast<int>(tmp0[i])][static_cast<int>(tmp1[i])];
                break;
            case 3:
                for (int i = 0; i < len; ++i)
                    if (tmp0[i] >= 0.f && tmp1[i] >= 0.f && tmp2[i] >= 0.f)
                        ++outPtrs_3[static_cast<int>(tmp0[i])][static_cast<int>(tmp1[i])][static_cast<int>(tmp2[i])];
                break;
            default:
                throw;
        }
    };
    const int blockNum = 8;
    const int blockSize = (imageSize - 1) / blockNum + 1;
    for (int i = 0; i < dims; ++i) {
        idxHost[i][0] = new float [blockSize];
        idxHost[i][1] = new float [blockSize];
    }
    int remainedSize = imageSize;
    int doneSize = 0;
    while (remainedSize > 0) {
        int workSize = blockSize < remainedSize ? blockSize : remainedSize;
        std::thread thread;
        if (doneSize > 0)
            thread = std::thread(func);
        for (int i = 0; i < dims; ++i) {
            bm_device_mem_t partDev = idxDev[i];
            bm_mem_set_device_addr(&partDev, bm_mem_get_device_addr(idxDev[i]) + doneSize * 4);
            bm_mem_set_device_size(&partDev, workSize * 4);
            BM_CHECK_RET(bm_memcpy_d2s(handle, idxHost[i][1 - idx], partDev));
        }
        if (doneSize > 0)
            thread.join();
        len = workSize;
        idx = 1 - idx;
        remainedSize -= workSize;
        doneSize += workSize;
    }
    func();
    BM_CHECK_RET(bm_memcpy_s2d(handle, output, outputHost));
    for (int i = 0; i < dims; ++i) {
        bm_free_device(handle, idxDev[i]);
        delete [] idxHost[i][0];
        delete [] idxHost[i][1];
    }
    if (outPtrs_2 != nullptr)
        delete [] outPtrs_2;
    if (outPtrs_3 != nullptr) {
        for (int i = 0; i < histSizes[0]; ++i)
            delete [] outPtrs_3[i];
        delete [] outPtrs_3;
    }
    delete [] outputHost;
    (void)(C);
    return ret;
}

bm_status_t bmcv_calc_hist_with_weight(bm_handle_t handle,
                                       bm_device_mem_t input,
                                       bm_device_mem_t output,
                                       const float *weight,
                                       int C,
                                       int H,
                                       int W,
                                       const int *channels,
                                       int dims,
                                       const int *histSizes,
                                       const float *ranges,
                                       int inputDtype) {
    if (dims <= 0 || dims > 3)
        return BM_ERR_PARAM;
    bm_status_t ret = BM_SUCCESS;
    int total = 1;
    for (int i = 0; i < dims; ++i)
        total *= histSizes[i];
    bm_api_cv_calc_hist_index_t api[3];
    int dsize = inputDtype == 0 ? 4 : 1;
    int imageSize = H * W;
    bm_device_mem_t idxDev[3];
    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    for (int i = 0; i < dims; ++i) {
        BM_CHECK_RET(bm_malloc_device_byte(handle, &idxDev[i], imageSize * 4));
        api[i].Xaddr = bm_mem_get_device_addr(input) + channels[i] * imageSize * dsize;
        api[i].Yaddr = bm_mem_get_device_addr(idxDev[i]);
        api[i].a = static_cast<float>(histSizes[i]) / (ranges[i * 2 + 1] - ranges[2 * i]);
        api[i].b = -ranges[2 * i] * api[i].a;
        api[i].len = imageSize;
        api[i].xdtype = inputDtype;
        api[i].upper = static_cast<float>(histSizes[i]);

        switch (chipid)
        {
            case 0x1684:
                BM_CHECK_RET(bm_send_api(handle,
                             BM_API_ID_CV_CALC_HIST_INDEX,
                             reinterpret_cast<uint8_t *>(&api[i]),
                             sizeof(api[i])));
                break;

            case BM1684X:
                BM_CHECK_RET(tpu_kernel_launch_sync(handle, "cv_calc_hist", &api[i], sizeof(api[i])));
                break;

            default:
                printf("ChipID is NOT supported\n");
                break;
        }
    }

    switch (chipid)
    {
        case 0x1684:
            BM_CHECK_RET(bm_sync_api(handle));
            break;

        case BM1684X:
            break;
    }
    float *outputHost = new float[total];
    memset(outputHost, 0x0, total * 4);
    float *idxHost[3][2];
    int idx = 0;
    int len = 0;
    float **outPtrs_2 = nullptr;
    float ***outPtrs_3 = nullptr;
    switch (dims) {
    case 2: {
        float *outIter = outputHost;
        outPtrs_2 = new float *[histSizes[0]];
        for (int i = 0; i < histSizes[0]; ++i) {
            outPtrs_2[i] = outIter;
            outIter += histSizes[1];
        }
        break;
    }
    case 3: {
        float *outIter = outputHost;
        outPtrs_3 = new float **[histSizes[0]];
        for (int i = 0; i < histSizes[0]; ++i) {
            outPtrs_3[i] = new float *[histSizes[1]];
            for (int j = 0; j < histSizes[1]; ++j) {
                outPtrs_3[i][j] = outIter;
                outIter += histSizes[2];
            }
        }
        break;
    }
    default:
        break;
    }
    auto func = [&]() {
        float *tmp0 = idxHost[0][idx];
        float *tmp1 = idxHost[1][idx];
        float *tmp2 = idxHost[2][idx];
        switch (dims) {
        case 1:
            for (int i = 0; i < len; ++i)
                if (tmp0[i] >= 0.f)
                    outputHost[static_cast<int>(tmp0[i])] += weight[i];
            break;
        case 2:
            for (int i = 0; i < len; ++i)
                if (tmp0[i] >= 0.f && tmp1[i] >= 0.f)
                    outPtrs_2[static_cast<int>(tmp0[i])][static_cast<int>(tmp1[i])] += weight[i];
            break;
        case 3:
            for (int i = 0; i < len; ++i)
                if (tmp0[i] >= 0.f && tmp1[i] >= 0.f && tmp2[i] >= 0.f)
                    outPtrs_3[static_cast<int>(tmp0[i])][static_cast<int>(tmp1[i])][static_cast<int>(tmp2[i])] += weight[i];
            break;
        default:
            throw;
        }
    };
    const int blockNum = 8;
    const int blockSize = (imageSize - 1) / blockNum + 1;
    for (int i = 0; i < dims; ++i) {
        idxHost[i][0] = new float [blockSize];
        idxHost[i][1] = new float [blockSize];
    }
    int remainedSize = imageSize;
    int doneSize = 0;
    int prevWorkSize = 0;
    while (remainedSize > 0) {
        int workSize = blockSize < remainedSize ? blockSize : remainedSize;
        std::thread thread;
        if (doneSize > 0)
            thread = std::thread(func);
        for (int i = 0; i < dims; ++i) {
            bm_device_mem_t partDev = idxDev[i];
            bm_mem_set_device_addr(&partDev, bm_mem_get_device_addr(idxDev[i]) + doneSize * 4);
            bm_mem_set_device_size(&partDev, workSize * 4);
            BM_CHECK_RET(bm_memcpy_d2s(handle, idxHost[i][1 - idx], partDev));
        }
        if (doneSize > 0) {
            thread.join();
            weight += prevWorkSize;
        }
        len = workSize;
        idx = 1 - idx;
        remainedSize -= workSize;
        doneSize += workSize;
        prevWorkSize = workSize;
    }
    func();
    BM_CHECK_RET(bm_memcpy_s2d(handle, output, outputHost));
    for (int i = 0; i < dims; ++i) {
        bm_free_device(handle, idxDev[i]);
        delete [] idxHost[i][0];
        delete [] idxHost[i][1];
    }
    if (outPtrs_2 != nullptr)
        delete [] outPtrs_2;
    if (outPtrs_3 != nullptr) {
        for (int i = 0; i < histSizes[0]; ++i)
            delete [] outPtrs_3[i];
        delete [] outPtrs_3;
    }
    delete [] outputHost;
    (void)(C);
    return ret;
}
