#include <stdint.h>
#include <stdio.h>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmlib_runtime.h"

bm_status_t bmcv_sort(bm_handle_t     handle,
                      bm_device_mem_t src_index_addr,
                      bm_device_mem_t src_data_addr,
                      int             data_cnt,
                      bm_device_mem_t dst_index_addr,
                      bm_device_mem_t dst_data_addr,
                      int             sort_cnt,
                      int             order,
                      bool            index_enable,
                      bool            auto_index) {
    bm_api_cv_sort_t arg;
    bool use_index_i = index_enable && (!auto_index);
    bool use_index_o = index_enable;
    bm_device_mem_t       src_index_buf_device;
    bm_device_mem_t       src_data_buf_device;
    bm_device_mem_t       dst_index_buf_device;
    bm_device_mem_t       dst_data_buf_device;

    if (bm_mem_get_type(src_index_addr) == BM_MEM_TYPE_SYSTEM && use_index_i) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                         handle, &src_index_buf_device, src_index_addr,
                         true,  // needs copy
                         data_cnt));
    } else {
        src_index_buf_device = src_index_addr;
    }

    if (bm_mem_get_type(src_data_addr) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                         handle, &src_data_buf_device, src_data_addr,
                         true,  // needs copy
                         data_cnt));
    } else {
        src_data_buf_device = src_data_addr;
    }

    if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM && use_index_o) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                         handle, &dst_index_buf_device, dst_index_addr,
                         false,
                         data_cnt));
    } else {
        dst_index_buf_device = dst_index_addr;
    }

    if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
        BM_CHECK_RET(bm_mem_convert_system_to_device_coeff(
                         handle, &dst_data_buf_device, dst_data_addr,
                         false,
                         data_cnt));
    } else {
        dst_data_buf_device = dst_data_addr;
    }

    arg.src_index_addr = use_index_i ?
                         bm_mem_get_device_addr(src_index_buf_device) : 0;
    arg.src_data_addr  = bm_mem_get_device_addr(src_data_buf_device);
    arg.dst_index_addr = use_index_o ?
                         bm_mem_get_device_addr(dst_index_buf_device) : 0;
    arg.dst_data_addr  = bm_mem_get_device_addr(dst_data_buf_device);
    arg.order          = order;
    arg.index_enable   = index_enable ? 1 : 0;
    arg.auto_index     = auto_index ? 1 : 0;
    arg.data_cnt       = data_cnt;
    arg.sort_cnt       = sort_cnt;

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    switch(chipid)
    {
        case 0x1684:
            if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_SORT, (uint8_t *)&arg, sizeof(arg))) {
                BMCV_ERR_LOG("sort send api error\r\n");
                return BM_ERR_FAILURE;
            }
            if (BM_SUCCESS != bm_sync_api(handle)) {
                BMCV_ERR_LOG("sort sync api error\r\n");
                return BM_ERR_FAILURE;
            }
            break;

        case BM1684X:
            BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_sort", &arg, sizeof(arg)));
            break;

        default:
            printf("BM_NOT_SUPPORTED!\n");
            break;
    }

    /* copy and free */
    if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_mem_set_device_size(&dst_data_buf_device, sort_cnt * 4);
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(dst_data_addr),
                                        dst_data_buf_device)) {
            bm_mem_set_device_size(&dst_data_buf_device, data_cnt * 4);
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
            goto err2;
        }
        bm_mem_set_device_size(&dst_data_buf_device, data_cnt * 4);
        bm_free_device(handle, dst_data_buf_device);
    }
    if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM && use_index_o) {
        bm_mem_set_device_size(&dst_index_buf_device, sort_cnt * 4);
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(dst_index_addr),
                                        dst_index_buf_device)) {
            bm_mem_set_device_size(&dst_index_buf_device, data_cnt * 4);
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
            goto err1;
        }
        bm_mem_set_device_size(&dst_index_buf_device, data_cnt * 4);
        bm_free_device(handle, dst_index_buf_device);
    }
    if (bm_mem_get_type(src_index_addr) == BM_MEM_TYPE_SYSTEM && use_index_i) {
        bm_free_device(handle, src_index_buf_device);
    }
    if (bm_mem_get_type(src_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, src_data_buf_device);
    }

    return BM_SUCCESS;

err2:
    if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, dst_data_buf_device);
    }
err1:
    if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM && use_index_o) {
        bm_free_device(handle, dst_index_buf_device);
    }
    if (bm_mem_get_type(src_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, src_data_buf_device);
    }
    if (bm_mem_get_type(src_index_addr) == BM_MEM_TYPE_SYSTEM && use_index_i) {
        bm_free_device(handle, src_index_buf_device);
    }
    return BM_ERR_FAILURE;
}