#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"

#pragma pack(push, 1)
typedef struct sg_api_cv_gemm
{
    u64 A_global_offset;
    u64 B_global_offset;
    u64 C_global_offset;
    u64 Y_global_offset;
    int M;
    int N;
    int K;
    int is_A_trans;
    int is_B_trans;
    float alpha;
    float beta;
    bm_image_data_format_ext in_dtype;
    bm_image_data_format_ext out_dtype;
} sg_api_cv_gemm_t;
#pragma pack(pop)

static bm_status_t bmcv_gemm_runtime_check()
{
    return BM_SUCCESS;
}

bm_status_t bmcv_gemm(bm_handle_t handle,
                      bool is_A_trans,
                      bool is_B_trans,
                      int M,
                      int N,
                      int K,
                      float alpha,
                      bm_device_mem_t A,
                      int lda,
                      bm_device_mem_t B,
                      int ldb,
                      float beta,
                      bm_device_mem_t C,
                      int ldc)
{
    if (handle == NULL)
    {
        bmlib_log("GEMM", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }

    bm_status_t status = bmcv_gemm_runtime_check(); // TODO
    if (status == BM_NOT_SUPPORTED)
    {
        return status;
    }
    bm_device_mem_t A_mem, B_mem, C_mem;
    if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM)
    {
        if (BM_SUCCESS !=
            bm_mem_convert_system_to_device_neuron(handle,
                                                   &A_mem,
                                                   A,
                                                   true, // need copy
                                                   M,
                                                   K,
                                                   1,
                                                   1))
        {
            BMCV_ERR_LOG("A_mem convert error\r\n");

            goto err0;
        }
    }
    else
    {
        A_mem = A;
    }
    if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM)
    {
        if (BM_SUCCESS !=
            bm_mem_convert_system_to_device_coeff(handle,
                                                  &B_mem,
                                                  B,
                                                  true, // need copy
                                                  K * N))
        {
            BMCV_ERR_LOG("B_mem convert error\r\n");

            goto err1;
        }
    }
    else
    {
        B_mem = B;
    }
    if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM)
    {
        if (BM_SUCCESS !=
            bm_mem_convert_system_to_device_neuron(handle,
                                                   &C_mem,
                                                   C,
                                                   true, // need copy
                                                   M,
                                                   N,
                                                   1,
                                                   1))
        {
            BMCV_ERR_LOG("C_mem convert error\r\n");

            goto err2;
        }
    }
    else
    {
        C_mem = C;
    }

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid)
    {
    case 0x1684:
    {
        bm_api_gemm_t api;
        api = {is_A_trans,
               is_B_trans,
               M,
               N,
               K,
               alpha,
               bm_mem_get_device_addr(A_mem),
               lda,
               bm_mem_get_device_addr(B_mem),
               ldb,
               beta,
               bm_mem_get_device_addr(C_mem),
               ldc};
        if (BM_SUCCESS != bm_send_api(handle, BM_API_ID_GEMM, (u8 *)&api, sizeof(api)))
        {
            BMCV_ERR_LOG("gemm send api error\r\n");
            goto err3;
        }
        if (BM_SUCCESS != bm_sync_api(handle))
        {
            BMCV_ERR_LOG("gemm sync api error\r\n");
            goto err3;
        }
        break;
    }

    case BM1684X:
    {
        sg_api_cv_gemm_t api;
        api = {bm_mem_get_device_addr(A_mem),
               bm_mem_get_device_addr(B_mem),
               bm_mem_get_device_addr(C_mem),
               bm_mem_get_device_addr(C_mem),
               M,
               N,
               K,
               is_A_trans == true ? 1 : 0,
               is_B_trans == true ? 1 : 0,
               alpha,
               beta,
               DATA_TYPE_EXT_FLOAT32,
               DATA_TYPE_EXT_FLOAT32};
        if (BM_SUCCESS != bm_tpu_kernel_launch(handle, "cv_gemm", &api, sizeof(api)))
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "cv_gemm launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            goto err3;
        }
        break;
    }

    default:
        printf("ChipID is NOT supported\n");
        break;
    }

    if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM)
    {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(handle, bm_mem_get_system_addr(C), C_mem))
        {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err3;
        }
        bm_free_device(handle, C_mem);
    }
    if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM)
    {
        bm_free_device(handle, A_mem);
    }
    if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM)
    {
        bm_free_device(handle, B_mem);
    }

    return BM_SUCCESS;

err3:
    if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM)
    {
        bm_free_device(handle, C_mem);
    }
err2:
    if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM)
    {
        bm_free_device(handle, B_mem);
    }
err1:
    if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM)
    {
        bm_free_device(handle, A_mem);
    }
err0:
    return BM_ERR_FAILURE;
}

bm_status_t bmcv_gemm_ext(bm_handle_t handle,
                          bool is_A_trans,
                          bool is_B_trans,
                          int M,
                          int N,
                          int K,
                          float alpha,
                          bm_device_mem_t A,
                          bm_device_mem_t B,
                          float beta,
                          bm_device_mem_t C,
                          bm_device_mem_t Y,
                          bm_image_data_format_ext input_dtype,
                          bm_image_data_format_ext output_dtype)
{
    if (handle == NULL)
    {
        bmlib_log("GEMM", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }

    if (is_A_trans&&!is_B_trans){
        bmlib_log("GEMM", BMLIB_LOG_ERROR, "Can support L_trans and R_not_trans!\r\n");
        return BM_ERR_FAILURE;
    }

    if( input_dtype == DATA_TYPE_EXT_FP16 && is_A_trans && M > 64){
        bmlib_log("GEMM", BMLIB_LOG_ERROR, "It only support M <= 64 when A is trans and input_dtype is FP16\n");
        return BM_NOT_SUPPORTED;
    }

    bm_status_t status = bmcv_gemm_runtime_check(); // TODO
    if (status == BM_NOT_SUPPORTED)
    {
        return status;
    }
    bm_device_mem_t A_mem, B_mem, C_mem, Y_mem;

    //support malloc here may be better?
    A_mem = A;
    B_mem = B;
    C_mem = C;
    Y_mem = Y;
    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid)
    {
    case 0x1684:
    {
        bmlib_log("GEMM_EXT", BMLIB_LOG_ERROR, "1684 not support!\r\n");
        return BM_NOT_SUPPORTED;
    }
    case BM1684X:
    {
        sg_api_cv_gemm_t api;
        api = {bm_mem_get_device_addr(A_mem),
               bm_mem_get_device_addr(B_mem),
               bm_mem_get_device_addr(C_mem),
               bm_mem_get_device_addr(Y_mem),
               M,
               N,
               K,
               is_A_trans == true ? 1 : 0,
               is_B_trans == true ? 1 : 0,
               alpha,
               beta,
               input_dtype,
               output_dtype};
        if (BM_SUCCESS != bm_tpu_kernel_launch(handle, "cv_gemm", &api, sizeof(api)))
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "cv_gemm launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            goto err0;
        }
        break;
    }
    default:
        printf("ChipID is NOT supported\n");
        break;
    }

    return BM_SUCCESS;

err0:
    return BM_ERR_FAILURE;
}