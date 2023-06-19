#include <stdint.h>
#include <stdio.h>
#include "bmcv_api.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"

#define MAX_TOPK (30)
typedef float         bm_feature_match_data_type_t;
typedef unsigned char bm_feature_match_data_type_fix8b_t;

bm_status_t bmcv_feature_match_normalized_bm1684(
    bm_handle_t     handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t db_data_global_addr,
    bm_device_mem_t db_feature_global_addr,
    bm_device_mem_t output_sorted_similarity_global_addr,
    bm_device_mem_t output_sorted_index_global_addr,
    int             batch_size,
    int             feature_size,
    int             db_size) {
    bm_api_cv_feature_match_t arg;
    bm_device_mem_t input_data_global_addr_device, db_data_global_addr_device,
        db_feature_global_addr_device,
        output_sorted_similarity_global_addr_device,
        output_sorted_index_global_addr_device;

    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &input_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_t) *
                                      batch_size * feature_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          input_data_global_addr_device,
                          bm_mem_get_system_addr(input_data_global_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        input_data_global_addr_device = input_data_global_addr;
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &db_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_t) *
                                      db_size * feature_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          db_data_global_addr_device,
                          bm_mem_get_system_addr(db_data_global_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err2;
        }
    } else {
        db_data_global_addr_device = db_data_global_addr;
    }
    if (bm_mem_get_type(db_feature_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(
                              handle,
                              &db_feature_global_addr_device,
                              sizeof(bm_feature_match_data_type_t) * db_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err2;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          db_feature_global_addr_device,
                          bm_mem_get_system_addr(db_feature_global_addr))) {

            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err3;
        }
    } else {
        db_feature_global_addr_device = db_feature_global_addr;
    }
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(
                handle,
                &output_sorted_similarity_global_addr_device,
                sizeof(bm_feature_match_data_type_t) * batch_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err3;
        }
    } else {
        output_sorted_similarity_global_addr_device =
            output_sorted_similarity_global_addr;
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &output_sorted_index_global_addr_device,
                                  sizeof(int) * batch_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err4;
        }
    } else {
        output_sorted_index_global_addr_device =
            output_sorted_index_global_addr;
    }
    arg.input_data_global_addr =
        bm_mem_get_device_addr(input_data_global_addr_device);
    arg.db_data_global_addr =
        bm_mem_get_device_addr(db_data_global_addr_device);
    arg.db_feature_global_addr =
        bm_mem_get_device_addr(db_feature_global_addr_device);
    arg.output_sorted_similarity_global_addr =
        bm_mem_get_device_addr(output_sorted_similarity_global_addr_device);
    arg.output_sorted_index_global_addr =
        bm_mem_get_device_addr(output_sorted_index_global_addr_device);
    arg.feature_size = feature_size;
    arg.batch_size   = batch_size;
    arg.db_size      = db_size;
    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_FEATURE_MATCH, (uint8_t *)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("feature_match send api error\r\n");
        goto err5;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("feature_match sync api error\r\n");
        goto err5;
    }
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_similarity_global_addr),
                output_sorted_similarity_global_addr_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err5;
        }
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_index_global_addr),
                output_sorted_index_global_addr_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err5;
        }
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }
    if (bm_mem_get_type(db_feature_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_feature_global_addr_device);
    }

    return BM_SUCCESS;

err5:
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
err4:
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
err3:
    if (bm_mem_get_type(db_feature_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_feature_global_addr_device);
    }
err2:
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }
err1:
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
err0:
    return BM_ERR_FAILURE;
}


bm_status_t bmcv_feature_match_normalized_bm1684X(
    bm_handle_t     handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t db_data_global_addr,
    bm_device_mem_t db_feature_global_addr,
    bm_device_mem_t output_sorted_similarity_global_addr,
    bm_device_mem_t output_sorted_index_global_addr,
    int             batch_size,
    int             feature_size,
    int             db_size) {
    bm_api_cv_feature_match_1684x_t arg;
    bm_device_mem_t db_data_trans_addr;
    bm_device_mem_t output_temp_addr;
    bm_device_mem_t input_data_global_addr_device, db_data_global_addr_device,
        db_feature_global_addr_device,
        output_sorted_similarity_global_addr_device,
        output_sorted_index_global_addr_device;

    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &input_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_t) *
                                      batch_size * feature_size)) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_malloc_device_byte error!");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          input_data_global_addr_device,
                          bm_mem_get_system_addr(input_data_global_addr))) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_memcpy_s2d error!");

            goto err1;
        }
    } else {
        input_data_global_addr_device = input_data_global_addr;
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &db_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_t) *
                                      db_size * feature_size)) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_malloc_device_byte error!");

            goto err1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          db_data_global_addr_device,
                          bm_mem_get_system_addr(db_data_global_addr))) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_memcpy_s2d error!");

            goto err2;
        }
    } else {
        db_data_global_addr_device = db_data_global_addr;
    }
    if (bm_mem_get_type(db_feature_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(
                              handle,
                              &db_feature_global_addr_device,
                              sizeof(bm_feature_match_data_type_t) * db_size)) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_malloc_device_byte error!");

            goto err2;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          db_feature_global_addr_device,
                          bm_mem_get_system_addr(db_feature_global_addr))) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_memcpy_s2d error!");

            goto err3;
        }
    } else {
        db_feature_global_addr_device = db_feature_global_addr;
    }
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(
                handle,
                &output_sorted_similarity_global_addr_device,
                sizeof(bm_feature_match_data_type_t) * batch_size)) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_malloc_device_byte error!");

            goto err3;
        }
    } else {
        output_sorted_similarity_global_addr_device =
            output_sorted_similarity_global_addr;
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &output_sorted_index_global_addr_device,
                                  sizeof(int) * batch_size)) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_malloc_device_byte error!");

            goto err4;
        }
    } else {
        output_sorted_index_global_addr_device = output_sorted_index_global_addr;
    }

    if (BM_SUCCESS != bm_malloc_device_byte(handle, &db_data_trans_addr, feature_size * db_size * 4)) {
        bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_malloc_device_byte error!");
        goto err4;
    }
    if (BM_SUCCESS != bm_malloc_device_byte(handle, &output_temp_addr, batch_size * db_size * 4)) {
        bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_malloc_device_byte error!");
        goto err4;
    }
    arg.db_data_trans_addr = bm_mem_get_device_addr(db_data_trans_addr);
    arg.output_temp_addr = bm_mem_get_device_addr(output_temp_addr);
    arg.input_data_global_addr =
        bm_mem_get_device_addr(input_data_global_addr_device);
    arg.db_data_global_addr =
        bm_mem_get_device_addr(db_data_global_addr_device);
    arg.db_feature_global_addr =
        bm_mem_get_device_addr(db_feature_global_addr_device);
    arg.output_sorted_similarity_global_addr =
        bm_mem_get_device_addr(output_sorted_similarity_global_addr_device);
    arg.output_sorted_index_global_addr =
        bm_mem_get_device_addr(output_sorted_index_global_addr_device);
    arg.feature_size = feature_size;
    arg.batch_size   = batch_size;
    arg.db_size      = db_size;
    if(BM_SUCCESS != bm_tpu_kernel_launch(handle, "cv_feature_match_1684x", &arg, sizeof(arg))) {
        bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "tpu_kernel_launch_sync error!");
        goto err5;
    }

    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_similarity_global_addr),
                output_sorted_similarity_global_addr_device)) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_memcpy_d2s error!");

            goto err5;
        }
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_index_global_addr),
                output_sorted_index_global_addr_device)) {
            bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "bm_memcpy_d2s error!");

            goto err5;
        }
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }
    if (bm_mem_get_type(db_feature_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_feature_global_addr_device);
    }

    return BM_SUCCESS;

err5:
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
    bm_free_device(handle, db_data_trans_addr);
    bm_free_device(handle, output_temp_addr);
err4:
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
err3:
    if (bm_mem_get_type(db_feature_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_feature_global_addr_device);
    }
err2:
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }
err1:
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
err0:
    return BM_ERR_FAILURE;
}

bm_status_t bmcv_feature_match_bm1684(
    bm_handle_t     handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t db_data_global_addr,
    bm_device_mem_t output_sorted_similarity_global_addr,
    bm_device_mem_t output_sorted_index_global_addr,
    int             batch_size,
    int             feature_size,
    int             db_size,
    int             sort_cnt,
    int             rshiftbits) {
    if (sort_cnt > MAX_TOPK) {
        BMCV_ERR_LOG("sort_cnt is toot large\r\n");

        return BM_ERR_FAILURE;
    }
    if(rshiftbits < 0) {
        BMCV_ERR_LOG("rshiftbits is invalid\r\n");

        return BM_ERR_FAILURE;
    }
    if (batch_size > 10) {
        BMCV_ERR_LOG("batch_size should not be greater than 10, input is :%d\r\n", batch_size);

        return BM_ERR_FAILURE;
    }
    if (feature_size > 4096) {
        BMCV_ERR_LOG("feature_size should not be greater than 4096, input is :%d\r\n", feature_size);

        return BM_ERR_FAILURE;
    }
    if (db_size > 500000) {
        BMCV_ERR_LOG("db_size should not be greater than 500000, input is :%d\r\n", db_size);

        return BM_ERR_FAILURE;
    }
    bm_api_cv_feature_match_fix8b_t arg;
    bm_device_mem_t input_data_global_addr_device, db_data_global_addr_device,
        output_sorted_similarity_global_addr_device,
        output_sorted_index_global_addr_device;
    u64 arm_tmp_buffer;
    if (((sizeof(short) + sizeof(int)) * batch_size * sort_cnt) >= 0x4000000) {
        BMCV_ERR_LOG("tmp buffer is not enough\r\n");

        return BM_ERR_FAILURE;
    }
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &input_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_fix8b_t) *
                                      batch_size * feature_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          input_data_global_addr_device,
                          bm_mem_get_system_addr(input_data_global_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        input_data_global_addr_device = input_data_global_addr;
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &db_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_fix8b_t) *
                                      db_size * feature_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          db_data_global_addr_device,
                          bm_mem_get_system_addr(db_data_global_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err2;
        }
    } else {
        db_data_global_addr_device = db_data_global_addr;
    }
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &output_sorted_similarity_global_addr_device,
                                  sizeof(short) * batch_size * sort_cnt)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err2;
        }
    } else {
        output_sorted_similarity_global_addr_device =
            output_sorted_similarity_global_addr;
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &output_sorted_index_global_addr_device,
                                  sizeof(int) * batch_size * sort_cnt)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err3;
        }
    } else {
        output_sorted_index_global_addr_device =
            output_sorted_index_global_addr;
    }
    arm_tmp_buffer = bm_gmem_arm_reserved_request(handle);
    if (BM_MEM_ADDR_NULL == arm_tmp_buffer) {
        goto err4;
    }
    arg.input_data_global_addr =
        bm_mem_get_device_addr(input_data_global_addr_device);
    arg.db_data_global_addr =
        bm_mem_get_device_addr(db_data_global_addr_device);
    arg.output_similarity_global_addr =
        bm_mem_get_device_addr(output_sorted_similarity_global_addr_device);
    arg.output_sorted_index_global_addr =
        bm_mem_get_device_addr(output_sorted_index_global_addr_device);
    arg.feature_size = feature_size;
    arg.batch_size   = batch_size;
    arg.db_size      = db_size;
    arg.sort_cnt     = sort_cnt;
    arg.arm_tmp_buffer = arm_tmp_buffer;
    arg.rshiftbits   = rshiftbits;
    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_FEATURE_MATCH_FIX8B, (uint8_t *)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("feature_match_fix8b send api error\r\n");
        goto err5;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("feature_match_fix8b sync api error\r\n");
        goto err5;
    }
    bm_gmem_arm_reserved_release(handle);
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_similarity_global_addr),
                output_sorted_similarity_global_addr_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err4;
        }
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_index_global_addr),
                output_sorted_index_global_addr_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err5;
        }
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }

    return BM_SUCCESS;
err5:
    bm_gmem_arm_reserved_release(handle);
err4:
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
err3:
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
err2:
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }
err1:
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
err0:
    return BM_ERR_FAILURE;
}

bm_status_t bmcv_feature_match_fix8_bm1684X(
    bm_handle_t     handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t db_data_global_addr,
    bm_device_mem_t output_sorted_similarity_global_addr,
    bm_device_mem_t output_sorted_index_global_addr,
    int             batch_size,
    int             feature_size,
    int             db_size,
    int             sort_cnt,
    int             rshiftbits) {
    if (sort_cnt > MAX_TOPK) {
        printf("sort_cnt is toot large\r\n");

        return BM_ERR_FAILURE;
    }
    if(rshiftbits < 0) {
        printf("rshiftbits is invalid\r\n");

        return BM_ERR_FAILURE;
    }
    if (batch_size > 10) {
        printf("batch_size should not be greater than 10, input is :%d\r\n", batch_size);

        return BM_ERR_FAILURE;
    }
    if (feature_size > 4096) {
        printf("feature_size should not be greater than 4096, input is :%d\r\n", feature_size);

        return BM_ERR_FAILURE;
    }
    if (db_size > 500000) {
        printf("db_size should not be greater than 500000, input is :%d\r\n", db_size);

        return BM_ERR_FAILURE;
    }

    bm_api_cv_feature_match_fix8b_1684x_t arg;
    bm_device_mem_t input_data_global_addr_device, db_data_global_addr_device,
        output_sorted_similarity_global_addr_device,
        output_sorted_index_global_addr_device;
    u64 arm_tmp_buffer;
    if (((sizeof(short) + sizeof(int)) * batch_size * sort_cnt) >= 0x4000000) {
        printf("tmp buffer is not enough\r\n");

        return BM_ERR_FAILURE;
    }
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &input_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_fix8b_t) *
                                      batch_size * feature_size)) {
            printf("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          input_data_global_addr_device,
                          bm_mem_get_system_addr(input_data_global_addr))) {
            printf("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        input_data_global_addr_device = input_data_global_addr;
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &db_data_global_addr_device,
                                  sizeof(bm_feature_match_data_type_fix8b_t) *
                                      db_size * feature_size)) {
            printf("bm_malloc_device_byte error\r\n");

            goto err1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          db_data_global_addr_device,
                          bm_mem_get_system_addr(db_data_global_addr))) {
            printf("bm_memcpy_s2d error\r\n");

            goto err2;
        }
    } else {
        db_data_global_addr_device = db_data_global_addr;
    }
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &output_sorted_similarity_global_addr_device,
                                  sizeof(short) * batch_size * sort_cnt)) {
            printf("bm_malloc_device_byte error\r\n");

            goto err2;
        }
    } else {
        output_sorted_similarity_global_addr_device =
            output_sorted_similarity_global_addr;
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &output_sorted_index_global_addr_device,
                                  sizeof(int) * batch_size * sort_cnt)) {
            printf("bm_malloc_device_byte error\r\n");

            goto err3;
        }
    } else {
        output_sorted_index_global_addr_device =
            output_sorted_index_global_addr;
    }
    arm_tmp_buffer = bm_gmem_arm_reserved_request(handle);
    if (BM_MEM_ADDR_NULL == arm_tmp_buffer) {
        goto err4;
    }
    arg.input_data_global_addr =
        bm_mem_get_device_addr(input_data_global_addr_device);
    arg.db_data_global_addr =
        bm_mem_get_device_addr(db_data_global_addr_device);
    arg.output_similarity_global_addr =
        bm_mem_get_device_addr(output_sorted_similarity_global_addr_device);
    arg.output_sorted_index_global_addr =
        bm_mem_get_device_addr(output_sorted_index_global_addr_device);
    arg.feature_size = feature_size;
    arg.batch_size   = batch_size;
    arg.db_size      = db_size;
    arg.sort_cnt     = sort_cnt;
    arg.arm_tmp_buffer = arm_tmp_buffer;
    arg.rshiftbits   = rshiftbits;

    if(BM_SUCCESS != bm_tpu_kernel_launch(handle, "cv_feature_match_fix8b_1684x", &arg, sizeof(arg))) {
        bmlib_log("FEATURE MATCH", BMLIB_LOG_ERROR, "tpu_kernel_launch_sync error!");
        goto err5;
    }
    bm_gmem_arm_reserved_release(handle);
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_similarity_global_addr),
                output_sorted_similarity_global_addr_device)) {
            printf("bm_memcpy_d2s error\r\n");

            goto err4;
        }
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_memcpy_d2s(
                handle,
                bm_mem_get_system_addr(output_sorted_index_global_addr),
                output_sorted_index_global_addr_device)) {
            printf("bm_memcpy_d2s error\r\n");

            goto err5;
        }
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }

    return BM_SUCCESS;
err5:
    bm_gmem_arm_reserved_release(handle);
err4:
    if (bm_mem_get_type(output_sorted_index_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_index_global_addr_device);
    }
err3:
    if (bm_mem_get_type(output_sorted_similarity_global_addr) ==
        BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
    }
err2:
    if (bm_mem_get_type(db_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, db_data_global_addr_device);
    }
err1:
    if (bm_mem_get_type(input_data_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_data_global_addr_device);
    }
err0:
    return BM_ERR_FAILURE;
}


bm_status_t bmcv_feature_match_normalized(
    bm_handle_t     handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t db_data_global_addr,
    bm_device_mem_t db_feature_global_addr,
    bm_device_mem_t output_sorted_similarity_global_addr,
    bm_device_mem_t output_sorted_index_global_addr,
    int             batch_size,
    int             feature_size,
    int             db_size) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {
      case 0x1684:
        ret = bmcv_feature_match_normalized_bm1684(handle,
                                         input_data_global_addr,
                                         db_data_global_addr,
                                         db_feature_global_addr,
                                         output_sorted_similarity_global_addr,
                                         output_sorted_index_global_addr,
                                         batch_size,feature_size, db_size);
        break;

      case BM1684X:
        ret = bmcv_feature_match_normalized_bm1684X(handle,
                                        input_data_global_addr,
                                        db_data_global_addr,
                                        db_feature_global_addr,
                                        output_sorted_similarity_global_addr,
                                        output_sorted_index_global_addr,
                                        batch_size,feature_size, db_size);
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

bm_status_t bmcv_feature_match(
    bm_handle_t     handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t db_data_global_addr,
    bm_device_mem_t output_sorted_similarity_global_addr,
    bm_device_mem_t output_sorted_index_global_addr,
    int             batch_size,
    int             feature_size,
    int             db_size,
    int             sort_cnt,
    int             rshiftbits) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {
      case 0x1684:
        ret = bmcv_feature_match_bm1684(handle,
                                        input_data_global_addr,
                                        db_data_global_addr,
                                        output_sorted_similarity_global_addr,
                                        output_sorted_index_global_addr,
                                        batch_size, feature_size, db_size,
                                        sort_cnt, rshiftbits);
        break;

      case BM1684X:
        ret = bmcv_feature_match_fix8_bm1684X(handle,
                                    input_data_global_addr,
                                    db_data_global_addr,
                                    output_sorted_similarity_global_addr,
                                    output_sorted_index_global_addr,
                                    batch_size, feature_size, db_size,
                                    sort_cnt, rshiftbits);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }
    return ret;
}