#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"

bm_status_t bmcv_hamming_distance(bm_handle_t handle,
                                  bm_device_mem_t input1,
                                  bm_device_mem_t input2,
                                  bm_device_mem_t output,
                                  int bits_len,
                                  int input1_num,
                                  int input2_num){
    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid = BM1684X;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;
    sg_api_hamming_distance_t api;
    api.input_query_global_offset = bm_mem_get_device_addr(input1);
    api.input_database_global_offset = bm_mem_get_device_addr(input2);
    api.output_global_offset = bm_mem_get_device_addr(output);
    api.bits_len = bits_len;
    api.left_row = input1_num;
    api.right_row = input2_num;
    switch(chipid)
    {
        case 0x1684:{
          ret = BM_NOT_SUPPORTED;
          printf("bm1684 not support!\n");
          break;
        }

        case BM1684X:
            BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_hanming_distance_1684x", &api, sizeof(api)));
            break;

        default:{
            ret = BM_NOT_SUPPORTED;
            printf("BM_NOT_SUPPORTED!\n");
            break;
        }
    }
    return ret;
}
