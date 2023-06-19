#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include <memory>
#ifdef __linux__
#include <sys/time.h>
#else
#include "time.h"
#endif

#define PERF_MEASURE_EN 0
#define AVE_TIMES 10

bm_status_t bmcv_image_transpose(bm_handle_t handle,
                                 bm_image input,
                                 bm_image output) {
    if (handle == NULL) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (input.image_format != FORMAT_RGB_PLANAR &&
        input.image_format != FORMAT_BGR_PLANAR &&
        input.image_format != FORMAT_GRAY) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "color_space of input only support RGB_PLANAR/BGR_PLANAR/GRAY!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (output.image_format != FORMAT_RGB_PLANAR &&
        output.image_format != FORMAT_BGR_PLANAR &&
        output.image_format != FORMAT_GRAY) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "color_space of output only support RGB_PLANAR/BGR_PLANAR/GRAY!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.image_format != output.image_format) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "input and output should be same image_format!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.data_type != output.data_type) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "input and output should be same data_type!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.width != output.height || input.height != output.width) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "input width should equal to output height and input height should equal to output width!\r\n");
        return BM_NOT_SUPPORTED;
    }

    bool out_mem_alloc_flag = 0;
    if (!bm_image_is_attached(output)) {
        if (BM_SUCCESS != bm_image_alloc_dev_mem(output)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
            return BM_ERR_FAILURE;
        }
        out_mem_alloc_flag = 1;
    }

    bm_device_mem_t input_mem, output_mem;
    bm_image_get_device_mem(output, &output_mem);

    auto input_ = std::make_shared<image_warpper>(handle, 1, input.height, \
                  input.width, input.image_format, input.data_type);

    int plane_num = bm_image_get_plane_num(input);
    bool in_need_convert = false;
    for (int p = 0; p < plane_num; p++) {
        if ((input.image_private->memory_layout[p].pitch_stride !=
             input_->inner[0].image_private->memory_layout[p].pitch_stride) ||
            (input.image_private->memory_layout[p].channel_stride !=
             input_->inner[0].image_private->memory_layout[p].channel_stride) ||
            (input.image_private->memory_layout[p].batch_stride !=
             input_->inner[0].image_private->memory_layout[p].batch_stride)) {
            in_need_convert = true;
            break;
        }
    }
    if (in_need_convert) {
        if (bm_image_alloc_dev_mem(input_->inner[0], BMCV_HEAP_ANY) != BM_SUCCESS) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "alloc intermediate memory failed\n",
                filename(__FILE__), __func__, __LINE__);
            if (out_mem_alloc_flag) {
                bm_free_device(handle, output_mem);
            }
            return BM_ERR_FAILURE;
        }
        for (int p = 0; p < plane_num; p++) {
            layout::update_memory_layout(
                handle,
                input.image_private->data[p],
                input.image_private->memory_layout[p],
                input_->inner[0].image_private->data[p],
                input_->inner[0].image_private->memory_layout[p]);
        }
    } else {
        bm_image_attach(input_->inner[0], input.image_private->data);
    }

    int input_c = (input.image_format == FORMAT_GRAY) ? 1 : 3;
    int type_len = (input.data_type == DATA_TYPE_EXT_1N_BYTE ||
                    input.data_type == DATA_TYPE_EXT_1N_BYTE_SIGNED) ? INT8_SIZE : FLOAT_SIZE;
    bm_image_get_device_mem(input_->inner[0], &input_mem);

    bm_api_cv_transpose_t api;
    api.input_global_mem_addr  = bm_mem_get_device_addr(input_mem);
    api.output_global_mem_addr = bm_mem_get_device_addr(output_mem);
    api.channel                = input_c;
    api.height                 = input.height;
    api.width                  = input.width;
    api.type_len               = type_len;

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);

    switch (chipid)
    {
        case 0x1684:
            ret = bm_send_api(handle,  BM_API_ID_CV_TRANSPOSE, (u8 *)&api, sizeof(api));
            if (BM_SUCCESS != ret) {
                if (out_mem_alloc_flag) {
                    bm_free_device(handle, output_mem);
                }
                BMCV_ERR_LOG("error occured at bmcv_send_api!\n");
                return ret;
            }
            ret = bm_sync_api(handle);
            if (BM_SUCCESS != ret) {
                if (out_mem_alloc_flag) {
                    bm_free_device(handle, output_mem);
                }
                BMCV_ERR_LOG("error occured at bm_sync_api!\n");
                return ret;
            }
            break;

        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_transpose", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                BMCV_ERR_LOG("error occured at tpu_kernel_launch_sync!\n");
                return ret;
            }
            break;

        default:
            printf("ChipID is NOT supported\n");
            break;
    }

    return BM_SUCCESS;
}

bm_status_t
bmcv_img_transpose(bm_handle_t handle, bmcv_image input, bmcv_image output) {
#if PERF_MEASURE_EN
    static long    tot_time = 0;
    static int     times    = 0;
    #ifdef __linux__
        struct timeval tv_start;
        struct timeval tv_end;
        struct timeval timediff;
    #else
        struct timespec tp_start;
        struct timespec tp_end;
        struct timespec timediffs;
    #endif
#endif

    //double t0;
    //double t1;

    if (handle == NULL) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (input.color_space != COLOR_RGB || output.color_space != COLOR_RGB) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "color_space of input and output bmcv_image should be COLOR_RGB!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.image_format != BGR && input.image_format != RGB) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "image_format of input bmcv_image should be RGB or BGR!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (output.image_format != RGB && output.image_format != BGR) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "image_format of output bmcv_image should be RGB or BGR!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.image_width * input.image_height != output.image_width * output.image_height) {
        bmlib_log("TRANSPOSE", BMLIB_LOG_ERROR, "size of input and output should be same!\r\n");
        return BM_NOT_SUPPORTED;
    }


    ASSERT(handle);
    ASSERT(input.color_space == COLOR_RGB);
    ASSERT(output.color_space == COLOR_RGB);
    ASSERT(input.image_format == BGR || input.image_format == RGB);
    ASSERT(output.image_format == BGR || output.image_format == RGB);
    ASSERT(input.image_width * input.image_height == output.image_width * output.image_height);
    int input_c = 3;
    int in_type_len =
        (input.data_format == DATA_TYPE_FLOAT ? FLOAT_SIZE : INT8_SIZE);
    int out_type_len =
        (output.data_format == DATA_TYPE_FLOAT ? FLOAT_SIZE : INT8_SIZE);

    bm_device_mem_t input_mem, output_mem, tmp_mem;
    if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &input_mem,
                                  input_c * input.image_height *
                                      input.image_width * in_type_len)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(
                handle, input_mem, bm_mem_get_system_addr(input.data[0]))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        input_mem = input.data[0];
    }

    if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &output_mem,
                                  input_c * output.image_height *
                                      output.image_width * out_type_len)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err1;
        }
    } else {
        output_mem = output.data[0];
    }

    if (out_type_len != in_type_len) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &tmp_mem,
                                  input_c * output.image_height *
                                      output.image_width * in_type_len)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err2;
        }
    }

#if PERF_MEASURE_EN
    gettimeofday(&tv_start, NULL);
    clock_gettime(0, &tp_start);
#endif

    bm_api_transpose_t api;
    api.input_global_mem_addr  = bm_mem_get_device_addr(input_mem);
    api.output_global_mem_addr = bm_mem_get_device_addr(
        out_type_len != in_type_len ? tmp_mem : output_mem);
    api.input_shape[0]         = input_c;
    api.input_shape[1]         = input.image_height;
    api.input_shape[2]         = input.image_width;
    api.order[2]               = 1;
    api.order[1]               = 2;
    api.order[0]               = 0;
    api.dims                   = 3;
    api.type_len               = in_type_len;
    api.store_mode             = 0;      // 1N mode
    api.buffer_global_mem_addr = 0x0ll;  // no used.


    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_TRANSPOSE, (u8 *)&api, sizeof(api))) {
        BMCV_ERR_LOG("error occured at bmcv_send_api!\n");
        goto err2;
    }
    //t0 = (double)cv::getTickCount();
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("error occured at bm_sync_api!\n");
        goto err2;
    }
    //t1 = (double)cv::getTickCount() - t0;
    //printf("spi excution time is %gms\n", t1 * 1000. / cv::getTickFrequency());


    if (out_type_len != in_type_len) {
        bmcv_image img_in, img_out;
        memcpy(&img_in, &output, sizeof(bmcv_image));
        img_in.data[0]     = tmp_mem;
        img_in.data_format = input.data_format;
        memcpy(&img_out, &output, sizeof(bmcv_image));
        img_out.data[0]   = output_mem;
        img_out.stride[0] = output.image_width;
        bmcv_img_crop(handle, img_in, input_c, 0, 0, img_out);
    }

#if PERF_MEASURE_EN
    #ifdef __linux__
        gettimeofday(&tv_end, NULL);
        timersub(&tv_end, &tv_start, &timediff);
        long consume1 = timediff.tv_sec * 1000000 + timediff.tv_usec;
        tot_time += consume1;
        times++;
    #else
        clock_gettime(0, &tp_end);
        timediffs.tv_sec = tp_end.tv_sec - tp_start.tv_sec;
        timediffs.tv_nsec = (tp_end.tv_nsec - tp_start.tv_nsec)/1000;
        long consume2 = timediffs.tv_sec * 1000000 + timediffs.tv_nsec;
        tot_time += consume2;
        times++;
    #endif
        if (times % AVE_TIMES == 0) {
            printf("bmcv_img_transpose(): %d times average performance: %ldus\n",
               times,
               tot_time / times);
            times    = 0;
            tot_time = 0;
        }
#endif

    if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                        bm_mem_get_system_addr(output.data[0]),
                                        output_mem)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err3;
        }
        bm_free_device(handle, output_mem);
    }
    if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_mem);
    }
    if (out_type_len != in_type_len) {
        bm_free_device(handle, tmp_mem);
    }

    return BM_SUCCESS;

err3:
    if (out_type_len != in_type_len) {
        bm_free_device(handle, tmp_mem);
    }
err2:
    if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_mem);
    }
err1:
    if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_mem);
    }
err0:
    return BM_ERR_FAILURE;
}
