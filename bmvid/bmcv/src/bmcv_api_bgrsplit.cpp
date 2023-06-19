#include "bmcv_api.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

#if PERF_MEASURE_EN
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#endif

#define FP32_PMT 0
#define INT8_PMT 4
#define PERF_MEASURE_EN  0
#define AVE_TIMES        10

bm_status_t  bmcv_img_bgrsplit_(
    bm_handle_t handle,
    bmcv_image input,
    bmcv_image output)
{
#if PERF_MEASURE_EN
        static long    tot_time=0;
        static int     times=0;
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

    int input_c = 3;
    int in_type_len = (input.data_format == DATA_TYPE_FLOAT ? FLOAT_SIZE : INT8_SIZE);
    int out_type_len = (output.data_format == DATA_TYPE_FLOAT ? FLOAT_SIZE : INT8_SIZE);

    bm_device_mem_t input_mem, output_mem, tmp_mem;
    if(bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM){
        if(BM_SUCCESS !=bm_malloc_device_byte(handle, &input_mem, input_c * input.image_height* input.image_width * in_type_len)){
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if(BM_SUCCESS !=bm_memcpy_s2d(handle, input_mem, bm_mem_get_system_addr(input.data[0]))){
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else{
        input_mem = input.data[0];
    }

    if(bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM){
        if(BM_SUCCESS !=bm_malloc_device_byte(handle, &output_mem, input_c * output.image_height* output.image_width * out_type_len)){
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err1;
        }
    } else{
        output_mem = output.data[0];
    }

    if (out_type_len != in_type_len) {
        if(BM_SUCCESS !=bm_malloc_device_byte(handle, &tmp_mem, input_c * output.image_height* output.image_width * in_type_len)){
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err2;
        }
    }

#if PERF_MEASURE_EN
    #ifdef __linux__
        gettimeofday(&tv_start, NULL);
    #else
        clock_gettime(0, &tp_start);
    #endif
#endif

    bm_api_transpose_t api;
    api.input_global_mem_addr = bm_mem_get_device_addr(input_mem);
    api.output_global_mem_addr = bm_mem_get_device_addr(out_type_len != in_type_len ? tmp_mem : output_mem);
    api.input_shape[0] = input.image_height;
    api.input_shape[1] = input.image_width;
    api.input_shape[2] = input_c;
    api.order[0] = 2;
    api.order[1] = 0;
    api.order[2] = 1;
    api.dims = 3;
    api.type_len = in_type_len;
    api.store_mode = 0;//1N mode
    api.buffer_global_mem_addr = 0x0ll;//no used.

    if(BM_SUCCESS !=bm_send_api(handle, BM_API_ID_TRANSPOSE, (u8 *)&api, sizeof(api))) {
        BMCV_ERR_LOG("send api error\r\n");

        goto err3;
    }
    if(BM_SUCCESS !=bm_sync_api(handle)) {
        BMCV_ERR_LOG("sync api error\r\n");

        goto err3;
    }

    if (out_type_len != in_type_len) {
        bmcv_image img_in, img_out;
        memcpy(&img_in, &output, sizeof(bmcv_image));
        img_in.data[0] = tmp_mem;
        img_in.data_format = input.data_format;
        memcpy(&img_out, &output, sizeof(bmcv_image));
        img_out.data[0] = output_mem;
        img_out.stride[0] = output.image_width;
        bmcv_img_crop(handle, img_in, input_c, 0, 0, img_out);
    }

#if PERF_MEASURE_EN
    #ifdef __linux__
        gettimeofday(&tv_end, NULL);
        timersub(&tv_end, &tv_start, &timediff);
        long consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
        tot_time += consume;
        times++;
    #else
        clock_gettime(0, &tp_end);
        timediffs.tv_sec = tp_end.tv_sec - tp_start.tv_sec;
        timediffs.tv_nsec = (tp_end.tv_nsec - tp_start.tv_nsec)/1000;
        long consumes = timediffs.tv_sec * 1000000 + timediffs.tv_nsec;
        tot_time += consume;
        times++;
    #endif
        if (times % AVE_TIMES == 0){
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_DEBUG,
                  "bmcv_img_bgrsplit()->permute+permute: %d times average performance: %ldus\n",
                  times,
                  tot_time/times);
            times = 0;
            tot_time = 0;
        }
#endif

    if(bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM){
        if(BM_SUCCESS != bm_memcpy_d2s(handle, bm_mem_get_system_addr(output.data[0]), output_mem)) {
            BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

            goto err3;
        }
        bm_free_device(handle, output_mem);
    }
    if(bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM){
        bm_free_device(handle, input_mem);
    }

    if (out_type_len != in_type_len) {
        bm_free_device(handle, tmp_mem);
    }

    return BM_SUCCESS;

err3:
    bm_free_device(handle, tmp_mem);
err2:
   if(bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM){
       bm_free_device(handle, output_mem);
   }
err1:
   if(bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM){
       bm_free_device(handle, input_mem);
   }
err0:
   return BM_ERR_FAILURE;
}

bm_status_t  bmcv_img_bgrsplit(
  bm_handle_t handle,
  bmcv_image input,
  bmcv_image output)
{
  unsigned int chipid = BM1684X;
  bm_status_t ret = BM_SUCCESS;
  bm_image_format_ext src_image_format = FORMAT_BGR_PACKED;
  bm_image_data_format_ext  src_data_format = DATA_TYPE_EXT_1N_BYTE;
  bm_image_format_ext dst_image_format = FORMAT_BGR_PLANAR;
  bm_image_data_format_ext  dst_data_format = DATA_TYPE_EXT_1N_BYTE;
  bm_image image_input, image_output;

  if (handle == NULL) {
      bmlib_log("BGRSPLIT", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
      return BM_ERR_FAILURE;
  }
  if (input.color_space != COLOR_RGB || output.color_space != COLOR_RGB) {
      bmlib_log("BGRSPLIT", BMLIB_LOG_ERROR, "color_space of input and output bmcv_image should be COLOR_RGB!\r\n");
      return BM_NOT_SUPPORTED;
  }
  if (input.image_format != RGB_PACKED && input.image_format != BGR_PACKED) {
      bmlib_log("BGRSPLIT", BMLIB_LOG_ERROR, "image_format of input bmcv_image should be RGB_PACKED or BGR_PACKED!\r\n");
      return BM_NOT_SUPPORTED;
  }
  if (output.image_format != RGB && output.image_format != BGR) {
      bmlib_log("BGRSPLIT", BMLIB_LOG_ERROR, "image_format of output bmcv_image should be RGB or BGR!\r\n");
      return BM_NOT_SUPPORTED;
  }
  if (input.image_width != output.image_width || input.image_height != output.image_height) {
      bmlib_log("BGRSPLIT", BMLIB_LOG_ERROR, "width/height of input and output should be same!\r\n");
      return BM_NOT_SUPPORTED;
  }

  ret = bm_get_chipid(handle, &chipid);
  if (BM_SUCCESS != ret)
    return ret;

  switch(chipid)
  {

    case 0x1684:
      ret = bmcv_img_bgrsplit_(handle, input, output);
      break;

    case BM1684X:
      bm_image_create(handle,
                        input.image_height,
                        input.image_width,
                        src_image_format,
                        src_data_format,
                        &image_input);
      bm_image_create(handle,
                        output.image_height,
                        output.image_width,
                        dst_image_format,
                        dst_data_format,
                        &image_output);

      bm_image_attach(image_input, input.data);
      bm_image_attach(image_output, output.data);

      ret = bm1684x_vpp_bgrsplit(handle, image_input, image_output);
      bm_image_destroy(image_input);
      bm_image_destroy(image_output);

      break;

    default:
      ret = BM_NOT_SUPPORTED;
      break;
  }

  return ret;
}

