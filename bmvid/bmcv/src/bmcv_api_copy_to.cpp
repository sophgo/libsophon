#include <stdint.h>
#include <stdio.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

#ifdef __linux__
  #include <unistd.h>
#else
  #include <windows.h>
#endif
#include "bmcv_common_bm1684.h"

typedef enum { COPY_TO_GRAY = 0, COPY_TO_BGR, COPY_TO_RGB } padding_corlor_e;
typedef enum { PLANNER = 0, PACKED } padding_format_e;

static int store_mode_transfer(bm_image_data_format_ext data_type) {
    switch (data_type) {
        case DATA_TYPE_EXT_FLOAT32:
            return STORAGE_MODE_1N_FP32;
        case DATA_TYPE_EXT_4N_BYTE:
            return STORAGE_MODE_4N_INT8;
        default:
            return STORAGE_MODE_1N_INT8;
    }
}

bm_status_t bmcv_copy_to_check(bmcv_copy_to_atrr_t copy_to_attr,
                               bm_image            input,
                               bm_image            output,
                               int                 data_size) {
    if ((input.data_type != output.data_type) ||
        (input.image_format != output.image_format)) {
        BMCV_ERR_LOG(
            "[CopyTo] input data_type and image_format must be same to "
            "output!\r\n");

        return BM_NOT_SUPPORTED;
    }
    // image format check
    if ((input.image_format != FORMAT_RGB_PLANAR) &&
        (input.image_format != FORMAT_RGB_PACKED) &&
        (input.image_format != FORMAT_BGR_PLANAR) &&
        (input.image_format != FORMAT_BGR_PACKED) &&
        (input.image_format != FORMAT_GRAY)) {
        BMCV_ERR_LOG("[CopyTo] image format not support\r\n");

        return BM_NOT_SUPPORTED;
    }
    if (((input.image_format == FORMAT_RGB_PACKED) ||
         (input.image_format == FORMAT_BGR_PACKED)) &&
        ((input.data_type == DATA_TYPE_EXT_4N_BYTE) ||
         (input.data_type == DATA_TYPE_EXT_4N_BYTE_SIGNED))) {
        BMCV_ERR_LOG("[CopyTo] 4n image should match planner\r\n");

        return BM_NOT_SUPPORTED;
    }
    if ((input.data_type == DATA_TYPE_EXT_FP16) ||
        (output.data_type == DATA_TYPE_EXT_FP16)||
        (input.data_type == DATA_TYPE_EXT_BF16) ||
        (output.data_type == DATA_TYPE_EXT_BF16)){
        BMCV_ERR_LOG("data type not support\n");

        return BM_NOT_SUPPORTED;
    }

    // shape check
    if (input.width > output.width) {
        BMCV_ERR_LOG(
            "[CopyTo] input.with should be less than or equal to output's\r\n");

        return BM_NOT_SUPPORTED;
    }
    if (input.height > output.height) {
        BMCV_ERR_LOG(
            "[CopyTo] input.height should be less than or equal to "
            "output's\r\n");

        return BM_NOT_SUPPORTED;
    }
    int in_image_stride[3] = {0};
    bm_image_get_stride(input, in_image_stride);
    int out_image_stride[3] = {0};
    bm_image_get_stride(output, out_image_stride);
    // compare by bytes
    if (((copy_to_attr.start_x + input.width) * data_size) >
        out_image_stride[0]) {
        BMCV_ERR_LOG("[CopyTo] width exceeds range\r\n");

        return BM_NOT_SUPPORTED;
    }
    // compare by elems
    if ((copy_to_attr.start_y + input.height) > output.height) {
        BMCV_ERR_LOG("[CopyTo] height exceeds range\r\n");

        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_image_copy_to_(bm_handle_t         handle,
                               bmcv_copy_to_atrr_t copy_to_attr,
                               bm_image            input,
                               bm_image            output) {
    if (handle == NULL) {
        BMCV_ERR_LOG("[CopyTo] Can not get handle!\r\n");

        return BM_ERR_FAILURE;
    }
    int data_size         = 1;
    int data_type         = STORAGE_MODE_1N_INT8;
    int bgr_or_rgb        = COPY_TO_BGR;
    int channel           = 3;
    int planner_or_packed = PLANNER;
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "get chipid failed, %s: %s: %d\n",
          filename(__FILE__), __func__, __LINE__);
        return ret;
    }

    switch (input.image_format) {
        case FORMAT_RGB_PLANAR: {
            bgr_or_rgb        = COPY_TO_RGB;
            planner_or_packed = PLANNER;
            channel           = 3;
            break;
        }
        case FORMAT_RGB_PACKED: {
            bgr_or_rgb        = COPY_TO_RGB;
            planner_or_packed = PACKED;
            channel           = 3;
            break;
        }
        case FORMAT_BGR_PLANAR: {
            bgr_or_rgb        = COPY_TO_BGR;
            planner_or_packed = PLANNER;
            channel           = 3;
            break;
        }
        case FORMAT_BGR_PACKED: {
            bgr_or_rgb        = COPY_TO_BGR;
            planner_or_packed = PACKED;
            channel           = 3;
            break;
        }
        case FORMAT_GRAY: {
            bgr_or_rgb        = COPY_TO_GRAY;
            planner_or_packed = PLANNER;
            channel           = 1;
            break;
        }
        default: {
            bgr_or_rgb        = COPY_TO_BGR;
            planner_or_packed = PLANNER;
            channel           = 3;
            break;
        }
    }
    switch (input.data_type) {
        case DATA_TYPE_EXT_FLOAT32: {
            data_size = 4;
            data_type = STORAGE_MODE_1N_FP32;
            break;
        }
        case DATA_TYPE_EXT_4N_BYTE:
        case DATA_TYPE_EXT_4N_BYTE_SIGNED: {
            data_size = 4;
            data_type = STORAGE_MODE_4N_INT8;
            break;
        }
        default: {
            data_size = 1;
            data_type = STORAGE_MODE_1N_INT8;
            break;
        }
    }
    int elem_byte_stride =
        (planner_or_packed == PACKED) ? (data_size * 3) : (data_size);
    if (BM_SUCCESS !=
        bmcv_copy_to_check(copy_to_attr, input, output, elem_byte_stride)) {
        BMCV_ERR_LOG("[CopyTo] bmcv_copy_to_check error!\r\n");

        return BM_ERR_FAILURE;
    }
    if (!bm_image_is_attached(output)) {
        if (BM_SUCCESS != bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY)) {
            BMCV_ERR_LOG("[CopyTo] bm_image_alloc_dev_mem error!\r\n");

            return BM_ERR_FAILURE;
        }
    }
    bm_device_mem_t in_dev_mem, out_dev_mem;
    bm_image_get_device_mem(input, &in_dev_mem);
    u64 in_dev_addr        = bm_mem_get_device_addr(in_dev_mem);
    int in_image_stride[3] = {0};
    bm_image_get_stride(input, in_image_stride);
    int out_image_stride[3] = {0};
    bm_image_get_stride(output, out_image_stride);
    bm_image_get_device_mem(output, &out_dev_mem);
    u64 padding_dev_addr = bm_mem_get_device_addr(out_dev_mem);
    u64 out_dev_addr     = padding_dev_addr +
                       out_image_stride[0] * copy_to_attr.start_y +
                       copy_to_attr.start_x * elem_byte_stride;
    bm_api_cv_copy_to_t arg;
    arg.input_image_addr   = in_dev_addr;
    arg.padding_image_addr = padding_dev_addr;
    arg.output_image_addr  = out_dev_addr;
    arg.C                  = channel;
    arg.input_w_stride     = in_image_stride[0] / data_size;
    arg.input_w            = input.width;
    arg.input_h            = input.height;
    arg.padding_w_stride   = out_image_stride[0] / data_size;
    arg.padding_w          = output.width;
    arg.padding_h          = output.height;
    arg.data_type          = data_type;
    arg.bgr_or_rgb         = bgr_or_rgb;
    arg.planner_or_packed  = planner_or_packed;
    arg.padding_b          = (int)copy_to_attr.padding_b;
    arg.padding_r          = (int)copy_to_attr.padding_r;
    arg.padding_g          = (int)copy_to_attr.padding_g;
    arg.if_padding         = copy_to_attr.if_padding;

    switch(chipid)
    {
        case 0x1684:{
            if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_COPY_TO, (uint8_t *)&arg, sizeof(arg))) {
                BMCV_ERR_LOG("copy_to send api error\r\n");
                return BM_ERR_FAILURE;
            }
            if (BM_SUCCESS != bm_sync_api(handle)) {
                BMCV_ERR_LOG("copy_to sync api error\r\n");
                return BM_ERR_FAILURE;
            }
            break;
        }

        case BM1684X:
            // tpu_kernel_launch_sync(handle, "sg_cv_copy_to", &arg, sizeof(arg));
            if(BM_SUCCESS != bm_tpu_kernel_launch(handle, "sg_cv_copy_to", &arg, sizeof(arg))){
                BMCV_ERR_LOG("copy_to launch api error\r\n");
                return BM_ERR_FAILURE;
            }
            break;

        default:
            return BM_NOT_SUPPORTED;
            break;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_copy_to(
    bm_handle_t         handle,
    bmcv_copy_to_atrr_t copy_to_attr,
    bm_image            input,
    bm_image            output)
{
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    unsigned int data_type = input.data_type;

    switch(chipid)
    {
        case 0x1684:
            ret = bmcv_image_copy_to_(handle, copy_to_attr, input, output);
            break;

        case BM1684X:{
            if ((data_type == DATA_TYPE_EXT_1N_BYTE)) {
              ret = bm1684x_vpp_copy_to(handle, copy_to_attr, input, output);
            }else if((data_type == DATA_TYPE_EXT_FLOAT32) || (data_type == DATA_TYPE_EXT_1N_BYTE_SIGNED)){
                ret = bmcv_image_copy_to_(handle, copy_to_attr, input, output);
            }else{
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "not support, %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
                ret = BM_NOT_SUPPORTED;
            }
            break;
        }

        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    return ret;
}