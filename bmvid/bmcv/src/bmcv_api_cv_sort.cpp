#include <stdint.h>
#include <stdio.h>
#include "bmcv_api.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"

typedef float bm_sort_data_type_t;

bm_status_t bmcv_sort_test_bm1684(bm_handle_t     handle,
                           bm_device_mem_t src_index_addr,
                           bm_device_mem_t src_data_addr,
                           bm_device_mem_t dst_index_addr,
                           bm_device_mem_t dst_data_addr,
                           int             order,
                           int             location,
                           int             data_cnt,
                           int             sort_cnt) {
    bm_api_cv_sort_test_t arg;
    bm_device_mem_t       src_index_buf_device;
    bm_device_mem_t       src_data_buf_device;
    bm_device_mem_t       dst_index_buf_device;
    bm_device_mem_t       dst_data_buf_device;

    if (bm_mem_get_type(src_index_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &src_index_buf_device,
                                                sizeof(int) * data_cnt)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          src_index_buf_device,
                          bm_mem_get_system_addr(src_index_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        src_index_buf_device = src_index_addr;
    }
    if (bm_mem_get_type(src_data_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &src_data_buf_device,
                                  sizeof(bm_sort_data_type_t) * data_cnt)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          src_data_buf_device,
                          bm_mem_get_system_addr(src_data_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err2;
        }
    } else {
        src_data_buf_device = src_data_addr;
    }
    if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &dst_index_buf_device,
                                                sizeof(int) * sort_cnt)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err2;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          dst_index_buf_device,
                          bm_mem_get_system_addr(dst_index_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err3;
        }
    } else {
        dst_index_buf_device = dst_index_addr;
    }
    if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &dst_data_buf_device,
                                  sizeof(bm_sort_data_type_t) * sort_cnt)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err3;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          dst_data_buf_device,
                          bm_mem_get_system_addr(dst_data_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err4;
        }
    } else {
        dst_data_buf_device = dst_data_addr;
    }

    arg.src_index_addr = bm_mem_get_device_addr(src_index_buf_device);
    arg.src_data_addr  = bm_mem_get_device_addr(src_data_buf_device);
    arg.dst_index_addr = bm_mem_get_device_addr(dst_index_buf_device);
    arg.dst_data_addr  = bm_mem_get_device_addr(dst_data_buf_device);
    arg.order          = order;
    arg.location       = location;
    arg.data_cnt       = data_cnt;
    arg.sort_cnt       = sort_cnt;

    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_SROT_TEST, (uint8_t *)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("sort_test send api error\r\n");
        goto err4;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("sort_test sync api error\r\n");
        goto err4;
    }

    if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(dst_data_addr),
                                        dst_data_buf_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err4;
        }
        bm_free_device(handle, dst_data_buf_device);
    }
    if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(dst_index_addr),
                                        dst_index_buf_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err3;
        }
        bm_free_device(handle, dst_index_buf_device);
    }
    if (bm_mem_get_type(src_index_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, src_index_buf_device);
    }
    if (bm_mem_get_type(src_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, src_data_buf_device);
    }

    return BM_SUCCESS;

err4:
    if (bm_mem_get_type(dst_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, dst_data_buf_device);
    }
err3:
    if (bm_mem_get_type(dst_index_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, dst_index_buf_device);
    }
err2:
    if (bm_mem_get_type(src_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, src_data_buf_device);
    }
err1:
    if (bm_mem_get_type(src_index_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, src_index_buf_device);
    }
err0:
    return BM_ERR_FAILURE;
}

bm_status_t bmcv_sort_test(bm_handle_t     handle,
                           bm_device_mem_t src_index_addr,
                           bm_device_mem_t src_data_addr,
                           bm_device_mem_t dst_index_addr,
                           bm_device_mem_t dst_data_addr,
                           int             order,
                           int             location,
                           int             data_cnt,
                           int             sort_cnt) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_sort_test_bm1684(handle, src_index_addr,
                                    src_data_addr,
                                    dst_index_addr,
                                    dst_data_addr,
                                    order,
                                    location,
                                    data_cnt,
                                    sort_cnt);
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