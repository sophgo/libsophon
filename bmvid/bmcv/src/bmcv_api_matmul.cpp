#include "bmcv_api_ext.h"
//#include "bmlib_internal.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"

#pragma pack(push, 1)
typedef struct {
  int   M;
  int   N;
  int   K;
  u64   A_addr;
  u64   B_addr;
  u64   C_addr;
  int   A_sign;
  int   B_sign;
  int   rshift_bit;
  int   result_type;
  int   is_B_trans;
  float alpha;
  float beta;
} sg_api_cv_matmul_t;
#pragma pack(pop)

bm_status_t bmcv_matmul(
        bm_handle_t      handle,
        int              M,
        int              N,
        int              K,
        bm_device_mem_t  A,
        bm_device_mem_t  B,
        bm_device_mem_t  C,
        int              A_sign,  // 1: signed 0: unsigned
        int              B_sign,
        int              rshift_bit,
        int              result_type,  // 0:8bit  1:int16  2:fp32
        bool             is_B_trans,
        float            alpha,
        float            beta
      ) {
    ASSERT(handle);

    bm_device_mem_t input_mem, weight_mem, output_mem;

    if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron_byte(
                     handle, &input_mem, A,
                     true,  // needs copy
                     M, 1, K, 1));
    } else {
        input_mem = A;
    }

    if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff_byte(
                     handle, &weight_mem, B,
                     true,  // needs copy
                     K * N));
    } else {
        weight_mem = B;
    }

    if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
        int byte_num = result_type ? 2 * result_type : 1;
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron_byte(
                     handle, &output_mem, C,
                     false,  // needs copy
                     M, N * byte_num, 1, 1));
    } else {
        output_mem = C;
    }

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid)
    {
        case 0x1684: {
            bm_api_fc_fix8b_forward_parallel_t api = {
                bm_mem_get_device_addr(input_mem),
                bm_mem_get_device_addr(weight_mem),
                BM_MEM_ADDR_NULL,
                bm_mem_get_device_addr(output_mem),
                M,
                K,
                N,
                is_B_trans,
                result_type == 2 ? 2 : 0,   // use_bias
                A_sign,
                B_sign,
                0,  // bias_sign
                rshift_bit,
                result_type,
                0,  // using_relu
                0,  // if_global_in_4N
                0,  //if_global_out_4N
                alpha,
                beta
            };

            if (BM_SUCCESS != bm_send_api(handle, BM_API_ID_FC_FWD_FIX8B_PARALLEL, (u8 *)&api, sizeof(api))) {
                BMCV_ERR_LOG("matmul send api error\r\n");
                if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, input_mem);
                }
                if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, weight_mem);
                }
                if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, output_mem);
                }
                return BM_ERR_FAILURE;
            }
            if (BM_SUCCESS != bm_sync_api(handle)) {
                BMCV_ERR_LOG("matmul sync api error\r\n");
                if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, input_mem);
                }
                if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, weight_mem);
                }
                if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, output_mem);
                }
                return BM_ERR_FAILURE;
            }
            break;
        }

        case BM1684X: {
            sg_api_cv_matmul_t api = {
                M,
                N,
                K,
                bm_mem_get_device_addr(input_mem),
                bm_mem_get_device_addr(weight_mem),
                bm_mem_get_device_addr(output_mem),
                A_sign,
                B_sign,
                rshift_bit,
                result_type,
                is_B_trans,
                alpha,
                beta
            };
           BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_matmul", &api, sizeof(api)));
            break;
        }

        default:
            printf("ChipID is NOT supported\n");
            break;
    }

    if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_mem);
    }
    if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, weight_mem);
    }
    if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(C),
                                        output_mem)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
            bm_free_device(handle, output_mem);
            return BM_ERR_FAILURE;
        }
        bm_free_device(handle, output_mem);
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_matmul_transpose_opt(
        bm_handle_t      handle,
        int              M,
        int              N,
        int              K,
        bm_device_mem_t  A,
        bm_device_mem_t  B,
        bm_device_mem_t  C,
        int              A_sign,
        int              B_sign) {
    ASSERT(handle);

    bm_device_mem_t input_mem, weight_mem, output_mem;

    if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron_byte(
                     handle, &input_mem, A,
                     true,  // needs copy
                     M, 1, K, 1));
    } else {
        input_mem = A;
    }

    if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff_byte(
                     handle, &weight_mem, B,
                     true,  // needs copy
                     K * N));
    } else {
        weight_mem = B;
    }

    int result_type = 2;
    if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
        int byte_num = result_type ? 2 * result_type : 1;
        BM_CHECK_RET(bm_mem_convert_system_to_device_neuron_byte(
                     handle, &output_mem, C,
                     false,  // needs copy
                     M, N * byte_num, 1, 1));
    } else {
        output_mem = C;
    }

    bm_api_fc_fix8b_forward_parallel_opt_t api = {
        bm_mem_get_device_addr(input_mem),
        bm_mem_get_device_addr(weight_mem),
        bm_mem_get_device_addr(output_mem),
        M,
        K,
        N,
        A_sign,
        B_sign,
    };

    if (BM_SUCCESS != bm_send_api(handle, BM_API_ID_FC_FWD_FIX8B_PARALLEL_OPT, (u8 *)&api, sizeof(api))) {
        BMCV_ERR_LOG("matmul send api error\r\n");
        if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, input_mem);
        }
        if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, weight_mem);
        }
        if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, output_mem);
        }
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("matmul sync api error\r\n");
        if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, input_mem);
        }
        if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, weight_mem);
        }
        if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, output_mem);
        }
        return BM_ERR_FAILURE;
    }

    if (bm_mem_get_type(A) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_mem);
    }
    if (bm_mem_get_type(B) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, weight_mem);
    }
    if (bm_mem_get_type(C) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(C),
                                        output_mem)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
            bm_free_device(handle, output_mem);
            return BM_ERR_FAILURE;
        }
        bm_free_device(handle, output_mem);
    }
    return BM_SUCCESS;
}