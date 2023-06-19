#include <stdint.h>
#include <stdio.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmlib_runtime.h"
#ifdef __linux__
  #include <unistd.h>
#else
  #include <windows.h>
#endif
#ifndef USING_CMODEL
#include "vpplib.h"
#endif
#include "bmcv_common_bm1684.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

typedef int32_t bm_resize_data_type_t;

typedef struct
{
    u64 y_addr;
    u64 u_addr;
    u64 v_addr;
    int width;
    int height;
    int y_stride;
    int u_stride;
    int v_stride;
} resize_yuv_attr_t;

#define MAX_INPUT_NUM (4)
#ifdef USING_CMODEL
#define TPU_ONLY (1)
#else
#define TPU_ONLY (0)
#endif
#define USING_DDR_LIMITATION (1)

#define FIND_MAX_VAR(input, image_num, output_w, output_h)                     \
    do {                                                                       \
        int temp_w = 0;                                                        \
        int temp_h = 0;                                                        \
        for (int find_idx = 0; find_idx < image_num; find_idx++) {             \
            temp_w = (temp_w >= input[find_idx].width)                         \
                         ? (temp_w)                                            \
                         : (input[find_idx].width);                            \
            temp_h = (temp_h >= input[find_idx].height)                        \
                         ? (temp_h)                                            \
                         : (input[find_idx].height);                           \
        }                                                                      \
        output_w = temp_w;                                                     \
        output_h = temp_h;                                                     \
    } while (0)

#define CHECK_MULTI_ROI_AND_ONE_POLICY(one_policy_in_diff_images,              \
                                       if_multi_roi)                           \
    do {                                                                       \
        one_policy_in_diff_images = 1;                                         \
        total_roi_nums            = 0;                                         \
        if_multi_roi              = 0;                                         \
        bmcv_resize_t tmp_resize_img_attr;                                     \
        memcpy(&tmp_resize_img_attr,                                           \
               &resize_attr[0].resize_img_attr[0],                             \
               sizeof(bmcv_resize_t));                                         \
        for (int i = 0; i < image_num; i++) {                                  \
            total_roi_nums += resize_attr[i].roi_num;                          \
            if_multi_roi = (1 != resize_attr[i].roi_num) | (if_multi_roi);     \
            if (one_policy_in_diff_images) {                                   \
                if ((if_multi_roi) ||                                          \
                    ((i > 0) && (resize_attr[i - 1].roi_num !=                 \
                                 resize_attr[i].roi_num)) ||                   \
                    ((i > 0) &&                                                \
                     (0 != memcmp(&resize_attr[i - 1].resize_img_attr[0],      \
                                  &resize_attr[i].resize_img_attr[0],          \
                                  sizeof(bmcv_resize_t))))) {                  \
                    one_policy_in_diff_images = 0;                             \
                    continue;                                                  \
                }                                                              \
                for (int roi_idx = 1; roi_idx < resize_attr[i].roi_num;        \
                     roi_idx++) {                                              \
                    if (0 != memcmp(&tmp_resize_img_attr,                      \
                                    &resize_attr[i].resize_img_attr[roi_idx],  \
                                    sizeof(bmcv_resize_t))) {                  \
                        one_policy_in_diff_images = 0;                         \
                    } else {                                                   \
                        memcpy(&tmp_resize_img_attr,                           \
                               &resize_attr[i].resize_img_attr[roi_idx],       \
                               sizeof(bmcv_resize_t));                         \
                    }                                                          \
                }                                                              \
            }                                                                  \
        }                                                                      \
    } while (0)

#define ONLY_DO_CROP(attr)                                                     \
    ((1 == ((float)(attr.in_width) / (float)attr.out_width)) &&                \
     (1 == ((float)(attr.in_height) / (float)attr.out_height)))                \
        ? (1)                                                                  \
        : (0);

#define GET_TOTAL_BATCH_NUM(array, array_num, total_num)                       \
    do {                                                                       \
        total_num = 0;                                                         \
        for (int arr_idx = 0; arr_idx < array_num; arr_idx++) {                \
            total_num += array[arr_idx].image_n;                               \
        }                                                                      \
    } while (0)

static int store_mode_transfer(bm_image_data_format_ext data_format) {
    switch (data_format) {
        case DATA_TYPE_EXT_FLOAT32:
            return STORAGE_MODE_1N_FP32;
        case DATA_TYPE_EXT_4N_BYTE:
            return STORAGE_MODE_4N_INT8;
        default:
            return STORAGE_MODE_1N_INT8;
    }
}


static bm_status_t bmcv_resize_check(
        bm_handle_t       handle,
        int               image_num,
        bm_image*         input,
        bm_image*         output) {

    if (handle == NULL) {
        BMCV_ERR_LOG("[RESIZE] Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    bm_image_format_ext src_format = input[0].image_format;
    bm_image_format_ext dst_format = output[0].image_format;
    bm_image_data_format_ext src_type = input[0].data_type;
    bm_image_data_format_ext dst_type = output[0].data_type;

    if (image_num < 0 || image_num > MAX_INPUT_NUM) {
        BMCV_ERR_LOG("[RESIZE]: image num not support:%d\n", image_num);
        return BM_NOT_SUPPORTED;
    }
    int out_num = 0;
    out_num = image_num;

    if (out_num <= 0) {
        BMCV_ERR_LOG("[RESIZE]: output image number must bigger than 0\n");
        return BM_NOT_SUPPORTED;
    }
    for (int i = 0; i < image_num; i++) {
        if (!bm_image_is_attached(input[i])) {
            BMCV_ERR_LOG("[RESIZE]: input[%d] is not attached device memory\n", i);
            return BM_NOT_SUPPORTED;
        }
        if ((input[i].data_type != src_type) || (input[i].image_format != src_format)) {
            BMCV_ERR_LOG("[RESIZE]: all src image_format and data_type should be same\n");
            return BM_NOT_SUPPORTED;
        }
    }
    for (int i = 1; i < out_num; i++) {
        if ((output[i].data_type != dst_type) || (output[i].image_format != dst_format)) {
            BMCV_ERR_LOG("[RESIZE]: all dst image_format and data_type should be same\n");
            return BM_NOT_SUPPORTED;
        }
    }

    if (src_format != FORMAT_YUV420P) {
        BMCV_ERR_LOG("[RESIZE]: src format not support:%d\n", src_format);
        return BM_NOT_SUPPORTED;
    }
    if (dst_format != FORMAT_YUV420P) {
        BMCV_ERR_LOG("[RESIZE]: dst format not support:%d\n", dst_format);
        return BM_NOT_SUPPORTED;
    }
    if (src_type != dst_type) {
        BMCV_ERR_LOG("[RESIZE]: image data type not support\n");
        return BM_NOT_SUPPORTED;
    }
    if ((src_type != DATA_TYPE_EXT_FLOAT32) && (src_type != DATA_TYPE_EXT_1N_BYTE)) {
        BMCV_ERR_LOG("[RESIZE]: input image data type not support\n");
        return BM_NOT_SUPPORTED;
    }
#if 0
    if (input->width <= 4096) {
        BMCV_ERR_LOG("[RESIZE]: image data size not support\n");
        return BM_NOT_SUPPORTED;
    }
#endif
    for (int input_idx = 0; input_idx < image_num; input_idx++) {
        if (!bm_image_is_attached(input[input_idx])) {
            BMCV_ERR_LOG("[RESIZE]: image data empty\n");
            return BM_NOT_SUPPORTED;
        }
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_resize_internal(bm_handle_t       handle,
                                 int               image_num,
                                 bm_image *        input,
                                 bm_image *        output) {
    bm_api_yuv_resize_t arg;
    bm_resize_data_type_t data_type = store_mode_transfer(input[0].data_type);

    resize_yuv_attr_t yuv_addr_in[MAX_INPUT_NUM];
    resize_yuv_attr_t yuv_addr_out[MAX_INPUT_NUM];
    bm_device_mem_t input_image_dev[3], output_image_dev[3];
    int in_stride[3] , out_stride[3];
    int input_para_size = 0;
    int output_para_size = 0;
    for (int i = 0; i < image_num;i++) {
        bm_image_get_device_mem (input[i] , input_image_dev);
        yuv_addr_in[i].y_addr = bm_mem_get_device_addr(input_image_dev[0]);
        yuv_addr_in[i].u_addr = bm_mem_get_device_addr(input_image_dev[1]);
        yuv_addr_in[i].v_addr = bm_mem_get_device_addr(input_image_dev[2]);
        yuv_addr_in[i].width = input[i].width;
        yuv_addr_in[i].height = input[i].height;
        bm_image_get_stride (input[i] , in_stride);
        yuv_addr_in[i].y_stride = in_stride[0];
        yuv_addr_in[i].u_stride = in_stride[1];
        yuv_addr_in[i].v_stride = in_stride[2];
        input_para_size += sizeof(resize_yuv_attr_t);

        bm_image_get_device_mem (output[i] , output_image_dev);
        yuv_addr_out[i].y_addr = bm_mem_get_device_addr(output_image_dev[0]);
        yuv_addr_out[i].u_addr = bm_mem_get_device_addr(output_image_dev[1]);
        yuv_addr_out[i].v_addr = bm_mem_get_device_addr(output_image_dev[2]);
        yuv_addr_out[i].width = output[i].width;
        yuv_addr_out[i].height = output[i].height;
        bm_image_get_stride (output[i] , out_stride);
        yuv_addr_out[i].y_stride = out_stride[0];
        yuv_addr_out[i].u_stride = out_stride[1];
        yuv_addr_out[i].v_stride = out_stride[2];
        output_para_size += sizeof(resize_yuv_attr_t);
    }

    bm_device_mem_t in_para_dev;
    if (BM_SUCCESS !=
        bm_malloc_device_byte(handle, &in_para_dev, input_para_size)) {
        BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_memcpy_s2d(handle, in_para_dev, yuv_addr_in)) {
        BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
        bm_free_device(handle, in_para_dev);
        return BM_ERR_FAILURE;
    }

    bm_device_mem_t out_para_dev;
    if (BM_SUCCESS !=
        bm_malloc_device_byte(handle, &out_para_dev, output_para_size)) {
        BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
        bm_free_device(handle, in_para_dev);
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_memcpy_s2d(handle, out_para_dev, yuv_addr_out)) {
        BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
        bm_free_device(handle, in_para_dev);
        bm_free_device(handle, out_para_dev);
        return BM_ERR_FAILURE;
    }

    // cfg args
    arg.input_para_addr = bm_mem_get_device_addr(in_para_dev);
    arg.output_para_addr = bm_mem_get_device_addr(out_para_dev);
    arg.data_type = data_type;
    arg.image_num = image_num;

    // send api
    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_YUV_RESIZE, (uint8_t *)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("resize send api error\r\n");
        bm_free_device(handle, in_para_dev);
        bm_free_device(handle, out_para_dev);
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("resize sync api error\r\n");
        bm_free_device(handle, in_para_dev);
        bm_free_device(handle, out_para_dev);
        return BM_ERR_FAILURE;
    }

    bm_free_device(handle, in_para_dev);
    bm_free_device(handle, out_para_dev);

    return BM_SUCCESS;
}


bm_status_t bmcv_image_yuv_resize_(bm_handle_t       handle,
                              int               input_num,
                              bm_image *        input,
                              bm_image *        output) {
    bm_status_t ret = bmcv_resize_check(handle, input_num, input, output);
    if (BM_SUCCESS != ret) {
        BMCV_ERR_LOG("bmcv_resize_check error\r\n");
        return ret;
    }
    int output_num = input_num;

    #ifdef __linux__
    bool output_mem_alloc_flag[output_num];
    #else
    std::shared_ptr<bool> output_mem_alloc_flag_(new bool[output_num], std::default_delete<bool[]>());
    bool* output_mem_alloc_flag = output_mem_alloc_flag_.get();
    #endif
    for (int output_idx = 0; output_idx < output_num; output_idx++) {
        output_mem_alloc_flag[output_idx] = false;
        if (!bm_image_is_attached(output[output_idx])) {
            /* here use BMCV_HEAP1_ID for following possible vpp process */
            if (BM_SUCCESS !=
                bm_image_alloc_dev_mem_heap_mask(output[output_idx], 6)) {
                BMCV_ERR_LOG("output bm_image_alloc_dev_mem error\n");
                for (int free_idx = 0; free_idx < output_idx; free_idx++) {
                    if (output_mem_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(output[free_idx], &dmem);
                        bm_free_device(handle, dmem);
                    }
                }

                return BM_ERR_FAILURE;
            }
            output_mem_alloc_flag[output_idx] = true;
        }
    }

    bmcv_resize_internal(handle,
                         input_num,
                         input,
                         output);

    return BM_SUCCESS;

}

bm_status_t bmcv_image_yuv_resize(
  bm_handle_t       handle,
  int               input_num,
  bm_image *        input,
  bm_image *        output)
{
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {
      case 0x1684:
        ret = bmcv_image_yuv_resize_(handle, input_num, input, output);
        break;

      case BM1684X:
        ret = bm1684x_vpp_yuv_resize(handle, input_num, input, output);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

