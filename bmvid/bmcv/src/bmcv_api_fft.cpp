#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"

#include <cmath>
#include <vector>
#include <thread>
#include <iostream>
#define EU_ALIGN(a) ALIGN((a), EU_NUM)
enum class FFTType {FFTType_0D, FFTType_1D, FFTType_2D};

#ifndef __linux__
#define M_PI 3.14159265358979323846
#endif

struct FFTPlan {
    FFTPlan(FFTType type) {
        this->type = type;
    }
    virtual ~FFTPlan() {}
    virtual void release(bm_handle_t handle) = 0;
    FFTType type;
};
struct FFT0DPlan : FFTPlan {
    FFT0DPlan() : FFTPlan(FFTType::FFTType_0D) {}
    void release(bm_handle_t) {}
    int batch;
};
struct FFT1DPlan : FFTPlan {
    FFT1DPlan() : FFTPlan(FFTType::FFTType_1D) {}
    void release(bm_handle_t handle) {
        if (factors.size() > 1) {
            if (ER_flag) bm_free_device(handle, ER);
            if (EI_flag) bm_free_device(handle, EI);
        }
    }
    bm_device_mem_t ER;
    bm_device_mem_t EI;
    bool ER_flag = false;
    bool EI_flag = false;
    int batch;
    int L;
    bool forward;
    std::vector<int> factors;
};
struct FFT2DPlan : FFTPlan {
    FFT2DPlan() : FFTPlan(FFTType::FFTType_2D) {}
    void release(bm_handle_t handle) {
        if (TR_flag) bm_free_device(handle, TR);
        if (TI_flag) bm_free_device(handle, TI);
        if (MP != nullptr) {
            auto P = reinterpret_cast<FFT1DPlan *>(MP);
            P->release(handle);
            delete P;
        }
        if (NP != nullptr) {
            auto P = reinterpret_cast<FFT1DPlan *>(NP);
            P->release(handle);
            delete P;
        }
    }
    bm_device_mem_t TR;
    bm_device_mem_t TI;
    bool TR_flag = false;
    bool TI_flag = false;
    void *MP = nullptr;
    void *NP = nullptr;
    int trans;
};
bm_status_t bmcv_fft_1d_create_plan_by_factors(
    bm_handle_t handle,
    int batch,
    int len,
    const int *factors,
    int factorSize,
    bool forward,
    void *&plan) {
    bm_status_t ret = BM_SUCCESS;
    auto P = new FFT1DPlan;
    P->batch = batch;
    P->L = len;
    P->forward = forward;
    int L = len;
    for (int i = 0; i < factorSize; ++i) {
        if (factors[i] == 1 || L % factors[i] != 0 ||
                (factors[i] != 2 && factors[i] != 3 &&
                 factors[i] != 4 && factors[i] != 5)) {
            std::cout << "INVALID FACTORS." << std::endl;
            return BM_ERR_PARAM;
        }
        P->factors.push_back(factors[i]);
        L /= factors[i];
    }
    if (L != 1) {
        std::cout << "INVALID FACTORS." << std::endl;
        return BM_ERR_PARAM;
    }
    std::vector<float> ER, EI;
    for (int i = factorSize - 1; i > 0; --i) {
        L *= factors[i];
        double s = 2.0 * M_PI / (L * factors[i - 1]);
        std::vector<float> cos(L * factors[i - 1]);
        std::vector<float> sin(L * factors[i - 1]);
        for (int k = 0; k < L * factors[i - 1]; ++k) {
            double ang = forward ? - s * k : s * k;
            cos[k] = static_cast<float>(std::cos(ang));
            sin[k] = static_cast<float>(std::sin(ang));
        }
        for (int j = 1; j < factors[i - 1]; ++j) {
            for (int k = 0; k < L; ++k) {
                ER.push_back(cos[(k * j) % (L * factors[i - 1])]);
                EI.push_back(sin[(k * j) % (L * factors[i - 1])]);
            }
        }
    }
    if (P->factors.size() > 1) {
        if (ER.empty()) return BM_ERR_FAILURE;
        if (EI.empty()) return BM_ERR_FAILURE;
        if (bm_malloc_device_byte(handle, &P->ER, ER.size() * 4) != BM_SUCCESS) {
            bmlib_log("FFT", BMLIB_LOG_ERROR, "alloc ER failed!\r\n");
            return BM_ERR_FAILURE;
        }
        P->ER_flag = true;
        if (bm_malloc_device_byte(handle, &P->EI, EI.size() * 4) != BM_SUCCESS) {
            bmlib_log("FFT", BMLIB_LOG_ERROR, "alloc EI failed!\r\n");
            bm_free_device(handle, P->ER);
            P->ER_flag = false;
            return BM_ERR_FAILURE;
        }
        P->EI_flag = true;
        if (bm_memcpy_s2d(handle, P->ER, ER.data()) != BM_SUCCESS) {
            bmlib_log("FFT", BMLIB_LOG_ERROR, "ER s2d failed!\r\n");
            bm_free_device(handle, P->ER);
            P->ER_flag = false;
            bm_free_device(handle, P->EI);
            P->EI_flag = false;
            return BM_ERR_FAILURE;
        }
        if (bm_memcpy_s2d(handle, P->EI, EI.data()) != BM_SUCCESS) {
            bmlib_log("FFT", BMLIB_LOG_ERROR, "EI s2d failed!\r\n");
            bm_free_device(handle, P->ER);
            P->ER_flag = false;
            bm_free_device(handle, P->EI);
            P->EI_flag = false;
            return BM_ERR_FAILURE;
        }
    }
    plan = P;
    return ret;
}
bm_status_t bmcv_fft_0d_create_plan(bm_handle_t,
                                    int batch,
                                    void *&plan) {
    bm_status_t ret = BM_SUCCESS;
    auto P = new FFT0DPlan;
    P->batch = batch;
    plan = P;
    return ret;
}
bm_status_t bmcv_fft_1d_create_plan(bm_handle_t handle,
                                    int batch,
                                    int len,
                                    bool forward,
                                    void *&plan) {
    if (len == 1)
        return bmcv_fft_0d_create_plan(handle, batch, plan);
    std::vector<int> F;
    int L = len;
    while (L % 5 == 0) {
        F.push_back(5);
        L /= 5;
    }
    while (L % 3 == 0) {
        F.push_back(3);
        L /= 3;
    }
    while (L % 2 == 0) {
        F.push_back(2);
        L /= 2;
    }
    if (L != 1) {
        std::cout << "INVALID INPUT SIZE (ONLY PRODUCT OF 2^a, 3^b, 4^c, 5^d IS SUPPORTED)." << std::endl;
        return BM_ERR_PARAM;
    }
    if (F.size() > 1) {
        if (F[F.size() - 1] == 2 && F[F.size() - 2] == 2) {
            F.pop_back();
            F.back() = 4;
        }
    }
    if (EU_ALIGN(len / F[0]) * 2 + EU_ALIGN(len) * 3 > LOCAL_MEM_SIZE / 4) {
        std::cout << "THE LENGTH OF INPUT IS TOO LARGE." << std::endl;
        return BM_ERR_PARAM;
    }
    return bmcv_fft_1d_create_plan_by_factors(
               handle,
               batch,
               len,
               F.data(),
               static_cast<int>(F.size()),
               forward,
               plan);
}
bm_status_t bmcv_fft_2d_create_plan(bm_handle_t handle,
                                    int M,
                                    int N,
                                    bool forward,
                                    void *&plan) {
    if (M == 1)
        return bmcv_fft_1d_create_plan(handle, 1, N, forward, plan);
    else if (N == 1)
        return bmcv_fft_1d_create_plan(handle, 1, M, forward, plan);
    bm_status_t ret = BM_SUCCESS;
    auto P = new FFT2DPlan;
    if (bmcv_fft_1d_create_plan(handle, M, N, forward, P->MP) != BM_SUCCESS) {
        bmlib_log("FFT-2D", BMLIB_LOG_ERROR, "create MP failed!\r\n");
        return BM_ERR_FAILURE;
    }
    if (bmcv_fft_1d_create_plan(handle, N, M, forward, P->NP)) {
        bmlib_log("FFT-2D", BMLIB_LOG_ERROR, "create NP failed!\r\n");
        return BM_ERR_FAILURE;
    }
    if (bm_malloc_device_byte(handle, &P->TR, M * N * 4) != BM_SUCCESS) {
        bmlib_log("FFT", BMLIB_LOG_ERROR, "alloc TR failed!\r\n");
        return BM_ERR_FAILURE;
    }
    P->TR_flag = true;
    if (bm_malloc_device_byte(handle, &P->TI, M * N * 4) != BM_SUCCESS) {
        bmlib_log("FFT", BMLIB_LOG_ERROR, "alloc TI failed!\r\n");
        bm_free_device(handle, P->TR);
        P->TR_flag = false;
        return BM_ERR_FAILURE;
    }
    P->TI_flag = true;
    char *trans = getenv("BM_FFT2D_TRANS_MODE");
    if (trans != nullptr)
        P->trans = atoi(trans);
    if (P->trans != 1 && P->trans != 2)
        P->trans = 2;
    plan = P;
    return ret;
}
static bm_status_t bmcv_fft_0d_execute(bm_handle_t handle,
                                       bm_device_mem_t inputReal,
                                       bm_device_mem_t inputImag,
                                       bm_device_mem_t outputReal,
                                       bm_device_mem_t outputImag,
                                       const void *plan,
                                       bool realInput) {
    bm_status_t ret = BM_SUCCESS;
    auto P = reinterpret_cast<const FFT0DPlan *>(plan);
    if (bm_memcpy_d2d(handle,
                      outputReal,
                      0,
                      inputReal,
                      0,
                      P->batch) != BM_SUCCESS) {
        bmlib_log("FFT", BMLIB_LOG_ERROR, "memcpy d2d failed!\r\n");
        return BM_ERR_FAILURE;
    }
    if (realInput) {
        if (bm_memset_device(handle,
                             0x0,
                             outputImag) != BM_SUCCESS) {
            bmlib_log("FFT", BMLIB_LOG_ERROR, "memset device failed!\r\n");
            return BM_ERR_FAILURE;
        }
    } else {
        if (bm_memcpy_d2d(handle,
                          outputImag,
                          0,
                          inputImag,
                          0,
                          P->batch) != BM_SUCCESS) {
            bmlib_log("FFT", BMLIB_LOG_ERROR, "memcpy d2d failed!\r\n");
            return BM_ERR_FAILURE;
        }
    }
    return ret;
}
static bm_status_t bmcv_fft_1d_execute_bm1684(bm_handle_t handle,
                                       bm_device_mem_t inputReal,
                                       bm_device_mem_t inputImag,
                                       bm_device_mem_t outputReal,
                                       bm_device_mem_t outputImag,
                                       const void *plan,
                                       bool realInput,
                                       int trans) {
    bm_status_t ret = BM_SUCCESS;
    bm_api_cv_fft_1d_t api;
    auto P = reinterpret_cast<const FFT1DPlan *>(plan);
    api.XR = bm_mem_get_device_addr(inputReal);
    api.XI = realInput ? 0 : bm_mem_get_device_addr(inputImag);
    api.YR = bm_mem_get_device_addr(outputReal);
    api.YI = bm_mem_get_device_addr(outputImag);
    api.ER = bm_mem_get_device_addr(P->ER);
    api.EI = bm_mem_get_device_addr(P->EI);
    api.batch = P->batch;
    api.len = P->L;
    api.forward = P->forward ? 1 : 0;
    api.inputIsReal = realInput ? 1 : 0;
    api.trans = trans;
    for (size_t i = 0; i < P->factors.size(); ++i)
        api.factors[i] = P->factors[i];
    api.factorSize = static_cast<int>(P->factors.size());
    if (bm_send_api(handle,
                     BM_API_ID_CV_FFT_1D,
                    reinterpret_cast<uint8_t *>(&api),
                    sizeof(api)) != BM_SUCCESS) {
        bmlib_log("FFT", BMLIB_LOG_ERROR, "send api failed!\r\n");
        return BM_ERR_FAILURE;
    }
    if (bm_sync_api(handle) != BM_SUCCESS) {
        bmlib_log("FFT", BMLIB_LOG_ERROR, "sync api failed!\r\n");
        return BM_ERR_FAILURE;
    }
    return ret;
}

static bm_status_t bmcv_fft_1d_execute(bm_handle_t handle,
                                       bm_device_mem_t inputReal,
                                       bm_device_mem_t inputImag,
                                       bm_device_mem_t outputReal,
                                       bm_device_mem_t outputImag,
                                       const void *plan,
                                       bool realInput,
                                       int trans) {
   unsigned int chipid = 0x1686;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_fft_1d_execute_bm1684(handle, inputReal,
                                        inputImag,
                                        outputReal,
                                        outputImag,
                                        plan,

                                        realInput,
                                        trans);
        break;

      case 0x1686:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static bm_status_t bmcv_fft_2d_execute(bm_handle_t handle,
                                       bm_device_mem_t inputReal,
                                       bm_device_mem_t inputImag,
                                       bm_device_mem_t outputReal,
                                       bm_device_mem_t outputImag,
                                       const void *plan,
                                       bool realInput) {
    bm_status_t ret = BM_SUCCESS;
    auto P = reinterpret_cast<const FFT2DPlan *>(plan);
    if (BM_SUCCESS != bmcv_fft_1d_execute(handle,
                                          inputReal,
                                          inputImag,
                                          P->TR,
                                          P->TI,
                                          P->MP,
                                          realInput,
                                          P->trans)) {
        bmlib_log("FFT-2D", BMLIB_LOG_ERROR, "exec MP failed!\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bmcv_fft_1d_execute(handle,
                                          P->TR,
                                          P->TI,
                                          outputReal,
                                          outputImag,
                                          P->NP,
                                          false,
                                          P->trans)) {
        bmlib_log("FFT-2D", BMLIB_LOG_ERROR, "exec NP failed!\r\n");
        return BM_ERR_FAILURE;
    }
    return ret;
}
bm_status_t bmcv_fft_execute(bm_handle_t handle,
                             bm_device_mem_t inputReal,
                             bm_device_mem_t inputImag,
                             bm_device_mem_t outputReal,
                             bm_device_mem_t outputImag,
                             const void *plan) {
    bm_status_t ret = BM_SUCCESS;
    auto P = reinterpret_cast<const FFTPlan *>(plan);
    switch (P->type) {
    case FFTType::FFTType_0D:
        ret = bmcv_fft_0d_execute(handle, inputReal, inputImag, outputReal, outputImag, plan, false);
        break;
    case FFTType::FFTType_1D:
        ret = bmcv_fft_1d_execute(handle, inputReal, inputImag, outputReal, outputImag, plan, false, 0);
        break;
    case FFTType::FFTType_2D:
        ret = bmcv_fft_2d_execute(handle, inputReal, inputImag, outputReal, outputImag, plan, false);
        break;
    default:
        std::cout << "INVALID PLAN TYPE." << std::endl;
        return BM_ERR_PARAM;
    }
    return ret;
}
bm_status_t bmcv_fft_execute_real_input(bm_handle_t handle,
                                        bm_device_mem_t inputReal,
                                        bm_device_mem_t outputReal,
                                        bm_device_mem_t outputImag,
                                        const void *plan) {
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t dummy;
    auto P = reinterpret_cast<const FFTPlan *>(plan);
    switch (P->type) {
    case FFTType::FFTType_0D:
        ret = bmcv_fft_0d_execute(handle, inputReal, dummy, outputReal, outputImag, plan, true);
        break;
    case FFTType::FFTType_1D:
        ret = bmcv_fft_1d_execute(handle, inputReal, dummy, outputReal, outputImag, plan, true, 0);
        break;
    case FFTType::FFTType_2D:
        ret = bmcv_fft_2d_execute(handle, inputReal, dummy, outputReal, outputImag, plan, true);
        break;
    default:
        std::cout << "INVALID PLAN TYPE." << std::endl;
        return BM_ERR_PARAM;
    }
    return ret;
}
void bmcv_fft_destroy_plan(bm_handle_t handle, void *plan) {
    auto P = reinterpret_cast<FFTPlan *>(plan);
    P->release(handle);
    delete P;
}
