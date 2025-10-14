#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include <stdio.h>
#include <vector>

#define M_PI 3.14159265358979323846
#define NPU_NUM 64
#define DIV_UP(a, b) ((a) == 0 ? 0 : ((a) - 1) / (b) + 1)

enum class FFTType {FFTType_0D, FFTType_1D, FFTType_2D};
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

enum Pad_mode {
    CONSTANT = 0,
    REFLECT = 1,
};

enum Win_mode {
    HANN = 0,
    HAMM = 1,
};

#pragma pack(push, 4)
typedef struct {
    int64_t XR;
    int64_t XI;
    int64_t window;
    int batch;
    int L;
    bool real_input;
} bm_api_win;
#pragma pack(pop)

static bm_status_t bmcv_win(bm_device_mem_t XR, bm_device_mem_t XI, bm_device_mem_t WinDev, int num_frm,
                            int n_fft, bool real_input, bm_handle_t handle)
{
    bm_api_win api;
    unsigned int chipid;
    bm_status_t ret = BM_SUCCESS;

    api.XR = bm_mem_get_device_addr(XR);
    api.XI = bm_mem_get_device_addr(XI);
    api.window = bm_mem_get_device_addr(WinDev);
    api.batch = num_frm;
    api.L = n_fft;
    api.real_input = real_input;

    ret = bm_get_chipid(handle, &chipid);
    if (ret) {
        printf("get chipid is error !\n");
        return BM_ERR_FAILURE;
    }

    switch(chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_fft_window", &api, sizeof(api));
            if(ret != BM_SUCCESS){
                bmlib_log("FFT_WINDOW", BMLIB_LOG_ERROR, "cv_fft_window sync api error\n");
                return ret;
            }
            break;
        default:
            printf("BMCV_WIN BM_NOT_SUPPORTED!\n");
            ret = BM_NOT_SUPPORTED;
            break;
    }

    return ret;
}

static bm_status_t bmcv_fft_apply(bm_handle_t handle, bm_device_mem_t inputReal,
                                bm_device_mem_t inputImag, bm_device_mem_t outputReal,
                                bm_device_mem_t outputImag, const void *plan, bool realInput,
                                int trans, bool normalize)
{
    bm_status_t ret = BM_SUCCESS;
    sg_api_cv_fft_t api;
    auto P = reinterpret_cast<const FFT1DPlan *>(plan);
    size_t i;
    unsigned int chipid;

    api.XR = bm_mem_get_device_addr(inputReal);
    api.XI = realInput ? 0 : bm_mem_get_device_addr(inputImag);
    api.YR = bm_mem_get_device_addr(outputReal);
    api.YI = bm_mem_get_device_addr(outputImag);
    api.ER = bm_mem_get_device_addr(P->ER);
    api.EI = bm_mem_get_device_addr(P->EI);
    api.batch = P->batch;
    api.len = P->L;
    api.forward = P->forward ? 1 : 0;
    api.realInput = realInput ? 1 : 0;
    api.trans = trans;
    api.factorSize = static_cast<int>(P->factors.size());
    api.normalize = normalize;

    for (i = 0; i < P->factors.size(); ++i) {
        api.factors[i] = P->factors[i];
    }

    ret = bm_get_chipid(handle, &chipid);
    if (ret) {
        printf("get chipid is error !\n");
        return BM_ERR_FAILURE;
    }

    switch(chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_fft", &api, sizeof(api));
            if(ret != BM_SUCCESS){
                bmlib_log("FFT_1D", BMLIB_LOG_ERROR, "fft_1d sync api error\n");
                return ret;
            }
            break;
        default:
            printf(" BMCV_FFT_APPLY BM_NOT_SUPPORTED!\n");
            ret = BM_NOT_SUPPORTED;
            break;
    }
    return ret;
}

static bm_status_t bmcv_fft_calculate(bm_handle_t handle, float* XRHost, float* XIHost, float* YR,
                                    float* YI, int batch, int L, bool realInput,
                                    int pad_mode, int n_fft, int win_mode, int hop_len,
                                    bool normalize)
{
    bm_status_t ret = BM_SUCCESS;
    void* plan = NULL;
    int hop, i, j;
    int pad_len = n_fft / 2;
    int pad_sig_len = L + 2 * pad_len;
    int num_frm = 1 + L / hop_len; /* YRHost col*/
    bm_device_mem_t XRDev, XIDev, YRDev, YIDev, WinDev;
    float* input_XR = (float*)malloc(pad_sig_len * sizeof(float));
    float* input_XI = (float*)malloc(pad_sig_len * sizeof(float));
    float* window = (float*)malloc(n_fft * sizeof(float));
    int batch_num = 0;

    /* pad the input signal */
    memset(input_XR, 0, pad_sig_len * sizeof(float));
    if (!realInput) {
        memset(input_XI, 0, pad_sig_len * sizeof(float));
    }

    for (j = 0; j < batch; ++j) {
        if (pad_mode == REFLECT) {
            memcpy(input_XR + pad_len, XRHost + j * L, L * sizeof(float));
            if (!realInput) {
                memcpy(input_XI + pad_len, XIHost + j * L, L * sizeof(float));
            }

            for (i = 0; i < pad_len; ++i) {
                input_XR[pad_len - i - 1] = XRHost[j * L + i];
                input_XR[pad_sig_len - pad_len + i] = XRHost[j * L + L - i - 1];
                if (!realInput) {
                    input_XI[pad_len - i - 1] = XIHost[j * L + i];
                    input_XI[pad_sig_len - pad_len + i] = XIHost[j * L + L - i - 1];
                }
            }
        } else if (pad_mode == CONSTANT) {
            memcpy(input_XR + pad_len, XRHost + j * L, L * sizeof(float));
            if (!realInput) {
                memcpy(input_XI + pad_len, XIHost + j * L, L * sizeof(float));
            }
        }

        for (hop = 0; hop < num_frm; ++hop) {
            memcpy(&YR[j * num_frm * n_fft + hop * n_fft], &input_XR[hop * hop_len], n_fft * sizeof(float));
            if (!realInput) {
            memcpy(&YI[j * num_frm * n_fft + hop * n_fft], &input_XI[hop * hop_len], n_fft * sizeof(float));
            }
        }
    }

    if (win_mode == HANN) {
        for (i = 0; i < n_fft; i++) {
            window[i] = 0.5 * (1 - cos(2 * M_PI * (float)i / n_fft));
        }
    } else if (win_mode == HAMM) {
        for (i = 0; i < n_fft; i++) {
            window[i] = 0.54 - 0.46 * cos(2 * M_PI * (float)i / n_fft);
        }
    }

    ret = bm_malloc_device_byte(handle, &WinDev, n_fft * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte failed!\n");
        goto exit0;
    }

    ret = bm_memcpy_s2d(handle, WinDev, window);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d failed!\n");
        goto exit1;
    }

    batch_num = batch * num_frm;

    ret = bm_malloc_device_byte(handle, &YRDev, n_fft * batch_num * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte failed!\n");
        goto exit1;
    }
    ret = bm_malloc_device_byte(handle, &YIDev, n_fft * batch_num * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte failed!\n");
        goto exit2;
    }
    ret = bmcv_fft_1d_create_plan(handle, batch_num, n_fft, true, plan);
    if (ret != BM_SUCCESS) {
        printf("bmcv_fft_1d_create_plan failed!\n");
        goto exit3;
    }
    ret = bm_malloc_device_byte(handle, &XRDev, n_fft * batch_num * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte failed!\n");
        goto exit4;
    }
    ret = bm_memcpy_s2d(handle, XRDev, YR);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d failed!\n");
        goto exit5;
    }
    if (!realInput) {
        ret = bm_malloc_device_byte(handle, &XIDev, n_fft * batch_num * sizeof(float));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto exit5;
        }
        ret = bm_memcpy_s2d(handle, XIDev, YI);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed!\n");
            goto exit6;
        }
    }
    ret = bmcv_win(XRDev, XIDev, WinDev, batch_num, n_fft, realInput, handle);
    if (ret != BM_SUCCESS) {
        printf("bmcv_win failed!\n");
        goto exit6;
    }
    ret = bmcv_fft_apply(handle, XRDev, XIDev, YRDev, YIDev, plan, realInput, 0, normalize);
    if (ret != BM_SUCCESS) {
        printf("bmcv_fft_execute failed!\n");
        goto exit6;
    }
    ret = bm_memcpy_d2s(handle, YR, YRDev);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed!\n");
        goto exit6;
    }
    ret = bm_memcpy_d2s(handle, YI, YIDev);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed!\n");
        goto exit6;
    }

exit6:
    if (!realInput) {
        bm_free_device(handle, XIDev);
    }
exit5:
    bm_free_device(handle, XRDev);
exit4:
    bmcv_fft_destroy_plan(handle, plan);
exit3:
    bm_free_device(handle, YIDev);
exit2:
    bm_free_device(handle, YRDev);
exit1:
    bm_free_device(handle, WinDev);
exit0:
    free(input_XI);
    free(input_XR);
    free(window);
    return ret;
}

static bm_status_t bmcv_tran_res(bm_handle_t handle, float* YR, float* YI, int batch, int num_frm, int n_fft)
{
    bm_status_t ret = BM_SUCCESS;
    int i, j;
    bm_image input_img, output_img;
    int stride_byte = n_fft * sizeof(float);
    unsigned char** host;
    float* Y_tmp;

    ret = bm_image_create(handle, num_frm, n_fft, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &input_img, &stride_byte);
    if (ret != BM_SUCCESS) {
        printf("ERROR: Cannot create input_bm_image\n");
        goto exit0;
    }

    ret = bm_image_alloc_dev_mem(input_img, BMCV_HEAP_ANY);
    if (ret != BM_SUCCESS) {
        printf("Alloc bm dev input mem failed. ret = %d\n", ret);
        goto exit0;
    }

    ret = bm_image_create(handle, n_fft, num_frm, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &output_img, NULL);
    if (ret != BM_SUCCESS) {
        printf("ERROR: Cannot create output_bm_image\n");
        goto exit1;
    }

    ret = bm_image_alloc_dev_mem(output_img, BMCV_HEAP_ANY);
    if (ret != BM_SUCCESS) {
        printf("Alloc bm dev output mem failed. ret = %d\n", ret);
        goto exit1;
    }

    for (j = 0; j < batch; ++j) {
        Y_tmp = YR + j * num_frm * n_fft;
        host = (unsigned char**)&Y_tmp;
        for (i = 0; i < input_img.image_private->plane_num; i++) {
            ret = bm_memcpy_s2d(input_img.image_private->handle, input_img.image_private->data[i], host[i]);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_s2d error, src addr %p, dst addr 0x%llx\n",
                    host[i], bm_mem_get_device_addr(input_img.image_private->data[i]));
                goto exit2;
            } /* host = host + image.image_private->plane_byte_size[i]; */
        }

        ret = bmcv_image_transpose(handle, input_img, output_img);
        if (ret != BM_SUCCESS) {
            printf("bmcv_image_transpose error!");
            goto exit2;
        }

        for (i = 0; i < output_img.image_private->plane_num; i++) {
            ret = bm_memcpy_d2s(output_img.image_private->handle, host[i], output_img.image_private->data[i]);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_d2s error, src addr 0x%llx, dst addr %p\n",
                    bm_mem_get_device_addr(output_img.image_private->data[i]), host[i]);
                goto exit2;
            } /* host = host + image.image_private->plane_byte_size[i]; */
        }

        Y_tmp = YI + j * num_frm * n_fft;
        host = (unsigned char**)&Y_tmp;
        for (i = 0; i < input_img.image_private->plane_num; i++) {
            ret = bm_memcpy_s2d(input_img.image_private->handle, input_img.image_private->data[i], host[i]);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_s2d error, src addr %p, dst addr 0x%llx\n",
                    host[i], bm_mem_get_device_addr(input_img.image_private->data[i]));
                goto exit2;
            } /* host = host + image.image_private->plane_byte_size[i]; */
        }

        ret = bmcv_image_transpose(handle, input_img, output_img);
        if (ret != BM_SUCCESS) {
            printf("bmcv_image_transpose error!");
            goto exit2;
        }

        for (i = 0; i < output_img.image_private->plane_num; i++) {
            ret = bm_memcpy_d2s(output_img.image_private->handle, host[i], output_img.image_private->data[i]);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_d2s error, src addr 0x%llx, dst addr %p\n",
                    bm_mem_get_device_addr(output_img.image_private->data[i]), host[i]);
                goto exit2;
            } /* host = host + image.image_private->plane_byte_size[i]; */
        }
    }

exit2:
    bm_image_destroy(output_img);
exit1:
    bm_image_destroy(input_img);
exit0:
    return ret;
}

bm_status_t bmcv_stft(bm_handle_t handle, float* XRHost, float* XIHost, float* YRHost,
                    float* YIHost, int batch, int L, bool realInput,
                    int pad_mode, int n_fft, int win_mode, int hop_len, bool normalize)
{
    bm_status_t ret = BM_SUCCESS;
    int num_frm = 1 + L / hop_len;
    int num_len = n_fft / 2 + 1;
    int i;

    float* YR = (float*)malloc(batch * n_fft * num_frm * sizeof(float));
    float* YI = (float*)malloc(batch * n_fft * num_frm * sizeof(float));

    ret = bmcv_fft_calculate(handle, XRHost, XIHost, YR, YI, batch, L, realInput,
                            pad_mode, n_fft, win_mode, hop_len, normalize);
    if (ret != BM_SUCCESS) {
        printf("bmcv_fft_calculate failed!\n");
        goto exit;
    }

    ret = bmcv_tran_res(handle, YR, YI, batch, num_frm, n_fft);
    if (ret != BM_SUCCESS) {
        printf("bmcv_tran_res failed!\n");
        goto exit;
    }

    for (i = 0; i <  batch; ++i) {
        memcpy(YRHost + i * num_len * num_frm, YR + i * num_frm * n_fft, num_len * num_frm * sizeof(float));
        memcpy(YIHost + i * num_len * num_frm, YI + i * num_frm * n_fft, num_len * num_frm * sizeof(float));
    }

exit:
    free(YI);
    free(YR);
    return ret;
}