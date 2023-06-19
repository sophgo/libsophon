#include <stdint.h>
#include <stdio.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

#define MAX_INPUT_NUM (4)
#define SET_DATA_FORMAT(img_color_type, data_size, channel)                    \
    do {                                                                       \
        switch (img_color_type) {                                              \
            case (UINT8_C1): {                                                 \
                channel   = 1;                                                 \
                data_size = 1;                                                 \
                break;                                                         \
            }                                                                  \
            case (UINT8_C3): {                                                 \
                channel   = 3;                                                 \
                data_size = 1;                                                 \
                break;                                                         \
            }                                                                  \
            case (INT8_C1): {                                                  \
                channel   = 1;                                                 \
                data_size = 1;                                                 \
                break;                                                         \
            }                                                                  \
            case (INT8_C3): {                                                  \
                channel   = 3;                                                 \
                data_size = 1;                                                 \
                break;                                                         \
            }                                                                  \
            case (FLOAT32_C1): {                                               \
                channel   = 1;                                                 \
                data_size = 4;                                                 \
                break;                                                         \
            }                                                                  \
            case (FLOAT32_C3): {                                               \
                channel   = 3;                                                 \
                data_size = 4;                                                 \
                break;                                                         \
            }                                                                  \
            default: {                                                         \
                channel   = 3;                                                 \
                data_size = 1;                                                 \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    } while (0)

static convert_storage_mode_e convert_to_type_translation(
    bm_image_data_format_ext in_format, bm_image_data_format_ext out_format) {
    convert_storage_mode_e convert_storage_mode;
    if (((DATA_TYPE_EXT_4N_BYTE == in_format) &&
         (DATA_TYPE_EXT_1N_BYTE == out_format)) ||
        ((DATA_TYPE_EXT_4N_BYTE == in_format) &&
         (DATA_TYPE_EXT_FLOAT32 == out_format))) {
        convert_storage_mode = CONVERT_4N_TO_1N;
    } else if (((DATA_TYPE_EXT_4N_BYTE_SIGNED == in_format) &&
                (DATA_TYPE_EXT_1N_BYTE_SIGNED == out_format)) ||
               ((DATA_TYPE_EXT_4N_BYTE_SIGNED == in_format) &&
                (DATA_TYPE_EXT_FLOAT32 == out_format))) {
        convert_storage_mode = CONVERT_4N_TO_1N;
    } else if (((DATA_TYPE_EXT_4N_BYTE_SIGNED == in_format) &&
                (DATA_TYPE_EXT_1N_BYTE == out_format)) ||
               ((DATA_TYPE_EXT_4N_BYTE == in_format) &&
                (DATA_TYPE_EXT_1N_BYTE_SIGNED == out_format))) {
        convert_storage_mode = CONVERT_4N_TO_1N;
    } else if ((DATA_TYPE_EXT_4N_BYTE == in_format) &&
               (DATA_TYPE_EXT_4N_BYTE == out_format)) {
        convert_storage_mode = CONVERT_4N_TO_4N;
    }

    else if ((DATA_TYPE_EXT_4N_BYTE_SIGNED == in_format) &&
             (DATA_TYPE_EXT_4N_BYTE_SIGNED == out_format)) {
        convert_storage_mode = CONVERT_4N_TO_4N;
    } else if ((DATA_TYPE_EXT_4N_BYTE == in_format) &&
               (DATA_TYPE_EXT_4N_BYTE_SIGNED == out_format)) {
        convert_storage_mode = CONVERT_4N_TO_4N;
    } else if ((DATA_TYPE_EXT_4N_BYTE_SIGNED == in_format) &&
               (DATA_TYPE_EXT_4N_BYTE == out_format)) {
        convert_storage_mode = CONVERT_4N_TO_4N;
    } else {
        convert_storage_mode = CONVERT_1N_TO_1N;
    }

    return convert_storage_mode;
}

static cv_color_e convert_to_color_translation(
    bm_image_data_format_ext data_format, int channel) {
    cv_color_e cv_color = UINT8_C3;
    if (((DATA_TYPE_EXT_4N_BYTE == data_format) ||
         (DATA_TYPE_EXT_1N_BYTE == data_format)) &&
        (3 == channel)) {
        cv_color = UINT8_C3;
    } else if (((DATA_TYPE_EXT_4N_BYTE == data_format) ||
                (DATA_TYPE_EXT_1N_BYTE == data_format)) &&
               (1 == channel)) {
        cv_color = UINT8_C1;
    } else if (((DATA_TYPE_EXT_4N_BYTE_SIGNED == data_format) ||
                (DATA_TYPE_EXT_1N_BYTE_SIGNED == data_format)) &&
               (3 == channel)) {
        cv_color = INT8_C3;
    } else if (((DATA_TYPE_EXT_4N_BYTE_SIGNED == data_format) ||
                (DATA_TYPE_EXT_1N_BYTE_SIGNED == data_format)) &&
               (1 == channel)) {
        cv_color = INT8_C1;
    } else if ((DATA_TYPE_EXT_FLOAT32 == data_format) && (3 == channel)) {
        cv_color = FLOAT32_C3;
    } else if ((DATA_TYPE_EXT_FLOAT32 == data_format) && (1 == channel)) {
        cv_color = FLOAT32_C1;
    }

    return cv_color;
}

bm_status_t bmcv_convert_to_internal(bm_handle_t          handle,
                                     bmcv_convert_to_attr convert_to_attr,
                                     int                  w_stride,
                                     bm_image_tensor      input,
                                     bm_image_tensor      output) {
    bm_device_mem_t input_img_addr;
    bm_device_mem_t output_img_addr;
    int             img_w                = input.image.width;
    int             img_h                = input.image.height;
    int             convert_storage_mode = convert_to_type_translation(
        input.image.data_type, output.image.data_type);
    int image_num = input.image_n;
    int input_img_data_type =
        convert_to_color_translation(input.image.data_type, input.image_c);
    int output_img_data_type =
        convert_to_color_translation(output.image.data_type, output.image_c);
    bm_api_cv_convert_to_t arg;

    bm_image_tensor_get_device_mem(input, &input_img_addr);
    if (!bm_image_tensor_is_attached(output)) {
        if (BM_SUCCESS !=
            bm_image_tensor_alloc_dev_mem(output, BMCV_HEAP0_ID)) {

            return BM_ERR_FAILURE;
        }
    }
    bm_image_tensor_get_device_mem(output, &output_img_addr);
    int in_channel = 0, in_data_size = 0;
    int out_channel = 0, out_data_size = 0;
    int is_4N_mode = ((CONVERT_4N_TO_1N == convert_storage_mode) ||
                      (CONVERT_4N_TO_4N == convert_storage_mode))
                         ? (1)
                         : (0);

    SET_DATA_FORMAT(input_img_data_type, in_data_size, in_channel);
    SET_DATA_FORMAT(output_img_data_type, out_data_size, out_channel);
    image_num        = (1 == is_4N_mode) ? ((image_num + 3) / 4) : (image_num);
    int in_img_size  = in_channel * in_data_size * img_w * img_h * image_num;
    int out_img_size = out_channel * out_data_size * img_w * img_h * image_num;
    if (is_4N_mode) {
        in_img_size  = in_img_size * 4;
        out_img_size = out_img_size * 4;
    }
    arg.input_img_addr       = bm_mem_get_device_addr(input_img_addr);
    arg.img_w                = img_w;
    arg.img_w_stride         = w_stride;
    arg.img_h                = img_h;
    arg.input_img_data_type  = input_img_data_type;
    arg.output_img_data_type = output_img_data_type;
    arg.alpha_0              = convert_to_attr.alpha_0;
    arg.beta_0               = convert_to_attr.beta_0;
    arg.alpha_1              = convert_to_attr.alpha_1;
    arg.beta_1               = convert_to_attr.beta_1;
    arg.alpha_2              = convert_to_attr.alpha_2;
    arg.beta_2               = convert_to_attr.beta_2;
    arg.convert_storage_mode = convert_storage_mode;
    arg.image_num            = image_num;
    arg.output_img_addr      = bm_mem_get_device_addr(output_img_addr);
    unsigned int chipid = BM1684X;
    bm_get_chipid(handle, &chipid);
    switch (chipid)
    {
    case 0x1684:{
        if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_CONVERT_TO, (uint8_t *)&arg, sizeof(arg))) {
            BMCV_ERR_LOG("convert_to send api error\r\n");
            return BM_ERR_FAILURE;
        }
       if(BM_SUCCESS != bm_sync_api(handle)){
        BMCV_ERR_LOG("convert_to sync api error\r\n");
        return BM_ERR_FAILURE;
       }
       break;
    }
    case BM1684X:{
        if(BM_SUCCESS != bm_tpu_kernel_launch(handle, "cv_convert_to", &arg, sizeof(arg))){
            BMCV_ERR_LOG("convert_to sync api error\r\n");
            return BM_ERR_FAILURE;
        }
        // tpu_kernel_launch_sync(handle, "cv_convert_to", &arg, sizeof(arg));
        break;
    }
    default:
        printf("ChipID is NOT supported\n");
        break;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_convert_to_intergrated(bm_handle_t     handle,
                                        bm_device_mem_t input_img_addr_0,
                                        bm_device_mem_t output_img_addr_0,
                                        bm_device_mem_t input_img_addr_1,
                                        bm_device_mem_t output_img_addr_1,
                                        bm_device_mem_t input_img_addr_2,
                                        bm_device_mem_t output_img_addr_2,
                                        bm_device_mem_t convert_to_attr_addr,
                                        int             times) {
    bm_api_cv_convert_to_inter_t arg;
    bm_device_mem_t              input_img_buf_device[MAX_INTERGRATED_NUM];
    bm_device_mem_t              output_img_buf_device[MAX_INTERGRATED_NUM];
    bm_device_mem_t              convert_to_attr_buf_device;
    int                          in_channel = 0, in_data_size = 0;
    int                          out_channel = 0, out_data_size = 0;
    int             img_w = 0, img_h = 0, image_num = 0, is_4N_mode = 0;
    bm_device_mem_t input_img_addr[MAX_INTERGRATED_NUM],
        output_img_addr[MAX_INTERGRATED_NUM];
    bmcv_convert_to_attr_t *convert_to_attr;

    input_img_addr[0]  = input_img_addr_0;
    output_img_addr[0] = output_img_addr_0;
    input_img_addr[1]  = input_img_addr_1;
    output_img_addr[1] = output_img_addr_1;
    input_img_addr[2]  = input_img_addr_2;
    output_img_addr[2] = output_img_addr_2;
    if (bm_mem_get_type(convert_to_attr_addr) == BM_MEM_TYPE_SYSTEM) {
        convert_to_attr = (bmcv_convert_to_attr_t *)bm_mem_get_system_addr(
            convert_to_attr_addr);
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle,
                                  &convert_to_attr_buf_device,
                                  sizeof(bmcv_convert_to_attr_t) * times)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            goto err0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          convert_to_attr_buf_device,
                          bm_mem_get_system_addr(convert_to_attr_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

            goto err1;
        }
    } else {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "convert_to_attr must be sys memory:%s:%d\n",
                  filename(__FILE__),
                  __LINE__);
        goto err0;
    }
    for (int idx = 0; idx < times; idx++) {
        img_w     = convert_to_attr[idx].img_w;
        img_h     = convert_to_attr[idx].img_h;
        image_num = convert_to_attr[idx].image_num;
        is_4N_mode =
            ((CONVERT_4N_TO_1N == convert_to_attr[idx].convert_storage_mode) ||
             (CONVERT_4N_TO_4N == convert_to_attr[idx].convert_storage_mode))
                ? (1)
                : (0);
        SET_DATA_FORMAT(
            convert_to_attr[idx].input_img_data_type, in_data_size, in_channel);
        SET_DATA_FORMAT(convert_to_attr[idx].output_img_data_type,
                        out_data_size,
                        out_channel);
        image_num = (1 == is_4N_mode) ? ((image_num + 3) / 4) : (image_num);
        int in_img_size = in_channel * in_data_size * img_w * img_h * image_num;
        int out_img_size =
            out_channel * out_data_size * img_w * img_h * image_num;
        if (is_4N_mode) {
            in_img_size  = in_img_size * 4;
            out_img_size = out_img_size * 4;
        }
        if (bm_mem_get_type(input_img_addr[idx]) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                    &input_img_buf_device[idx],
                                                    in_img_size)) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
                for (int free_idx = 0; free_idx < idx; free_idx++) {
                    if (bm_mem_get_type(input_img_buf_device[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, input_img_buf_device[free_idx]);
                    }
                    if (bm_mem_get_type(output_img_addr[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, output_img_addr[free_idx]);
                    }
                }

                goto err1;
            }
            if (BM_SUCCESS !=
                bm_memcpy_s2d(handle,
                              input_img_buf_device[idx],
                              bm_mem_get_system_addr(input_img_addr[idx]))) {
                BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
                for (int free_idx = 0; free_idx <= idx; free_idx++) {
                    if (bm_mem_get_type(input_img_buf_device[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, input_img_buf_device[free_idx]);
                    }
                }
                for (int free_idx = 0; free_idx < idx; free_idx++) {
                    if (bm_mem_get_type(output_img_addr[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, output_img_addr[free_idx]);
                    }
                }

                goto err1;
            }
        } else {
            input_img_buf_device[idx] = input_img_addr[idx];
        }
        if (bm_mem_get_type(output_img_addr[idx]) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                    &output_img_buf_device[idx],
                                                    out_img_size)) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
                for (int free_idx = 0; free_idx <= idx; free_idx++) {
                    if (bm_mem_get_type(input_img_buf_device[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, input_img_buf_device[free_idx]);
                    }
                }
                for (int free_idx = 0; free_idx < idx; free_idx++) {
                    if (bm_mem_get_type(output_img_addr[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, output_img_addr[free_idx]);
                    }
                }

                goto err1;
            }
            if (BM_SUCCESS !=
                bm_memcpy_s2d(handle,
                              output_img_buf_device[idx],
                              bm_mem_get_system_addr(output_img_addr[idx]))) {
                BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
                for (int free_idx = 0; free_idx <= idx; free_idx++) {
                    if (bm_mem_get_type(input_img_buf_device[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, input_img_buf_device[free_idx]);
                    }
                    if (bm_mem_get_type(output_img_addr[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, output_img_addr[free_idx]);
                    }
                }

                goto err1;
            }
        } else {
            output_img_buf_device[idx] = output_img_addr[idx];
        }
    }
    arg.input_img_addr_0  = bm_mem_get_device_addr(input_img_buf_device[0]);
    arg.output_img_addr_0 = bm_mem_get_device_addr(output_img_buf_device[0]);
    arg.input_img_addr_1  = bm_mem_get_device_addr(input_img_buf_device[1]);
    arg.output_img_addr_1 = bm_mem_get_device_addr(output_img_buf_device[1]);
    arg.input_img_addr_2  = bm_mem_get_device_addr(input_img_buf_device[2]);
    arg.output_img_addr_2 = bm_mem_get_device_addr(output_img_buf_device[2]);
    arg.convert_to_attr_addr =
        bm_mem_get_device_addr(convert_to_attr_buf_device);
    arg.times = times;
    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_CONVERT_TO_INTERGRATED, (uint8_t *)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("convert_to_intergrated send api error\r\n");
        goto err1;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("convert_to_intergrated sync api error\r\n");
        goto err1;
    }
    for (int idx = 0; idx < times; idx++) {
        if (bm_mem_get_type(output_img_addr[idx]) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS !=
                bm_memcpy_d2s(handle,
                              bm_mem_get_system_addr(output_img_addr[idx]),
                              output_img_buf_device[idx])) {
                BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");
                for (int free_idx = idx; free_idx < times; free_idx++) {
                    if (bm_mem_get_type(output_img_buf_device[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, output_img_buf_device[free_idx]);
                    }
                    if (bm_mem_get_type(input_img_buf_device[free_idx]) ==
                        BM_MEM_TYPE_SYSTEM) {
                        bm_free_device(handle, input_img_buf_device[free_idx]);
                    }
                }
                goto err1;
            }
            bm_free_device(handle, output_img_buf_device[idx]);
        }
        if (bm_mem_get_type(input_img_addr[idx]) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, input_img_buf_device[idx]);
        }
    }
    if (bm_mem_get_type(convert_to_attr_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, convert_to_attr_buf_device);
    }

    return BM_SUCCESS;

err1:
    if (bm_mem_get_type(convert_to_attr_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, convert_to_attr_buf_device);
    }
err0:
    return BM_ERR_FAILURE;
}

static bm_status_t bm_convert_to_get_stride(bm_image input, int &w_stride) {
    int data_size = 1;
    switch (input.data_type) {
        case DATA_TYPE_EXT_FLOAT32:
            data_size = 4;
            break;
        case DATA_TYPE_EXT_4N_BYTE:
        case DATA_TYPE_EXT_4N_BYTE_SIGNED:
            data_size = 4;
            break;
        default:
            data_size = 1;
            break;
    }
    bm_image_get_stride(input, &w_stride);
    w_stride = w_stride / data_size;

    return BM_SUCCESS;
}

static bm_status_t bmcv_convert_to_check(bm_handle_t          handle,
                                         int                  image_num,
                                         bmcv_convert_to_attr convert_to_attr,
                                         bm_image *           input,
                                         bm_image *           output) {
    UNUSED(convert_to_attr);
    if (handle == NULL) {
        bmlib_log("CONVERT TO", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (image_num == 0) {
        // bmlib_log(BMCV_LOG_TAG,
        //           BMLIB_LOG_ERROR,
        //           "input image num not support:%d\n",
        //           image_num);
        BMCV_ERR_LOG("input image num not support:%d\n", image_num);

        return BM_NOT_SUPPORTED;
    }
    for (int i = 0; i < image_num; i++) {
        if ((input[i].width != output[i].width) ||
            (input[i].height != output[i].height)) {
            BMCV_ERR_LOG("input size must be same to output\n");

            return BM_NOT_SUPPORTED;
        }
        int width_stride = 0;
        bm_convert_to_get_stride(output[i], width_stride);
        if (output[i].width != width_stride) {
            BMCV_ERR_LOG("output width must be equal to stride\n");

            return BM_NOT_SUPPORTED;
        }
        // this limit maybe too strict
        // if (0 != memcmp(handle,
        //                 bm_image_get_handle(&input[i]),
        //                 sizeof(struct bm_context))) {
        //     BMCV_ERR_LOG("input image handle error\n");

        //     return BM_ERR_FAILURE;
        // }
        // if (0 != memcmp(handle,
        //                 bm_image_get_handle(&output[i]),
        //                 sizeof(struct bm_context))) {
        //     BMCV_ERR_LOG("output image handle error\n");

        //     return BM_ERR_FAILURE;
        // }
    }
    for (int i = 0; i < image_num - 1; i++) {
        if ((input[i].width != input[i + 1].width) ||
            (output[i].height != output[i + 1].height) ||
            (input[i].image_format != input[i + 1].image_format) ||
            (output[i].image_format != output[i + 1].image_format) ||
            (input[i].data_type != input[i + 1].data_type) ||
            (output[i].data_type != output[i + 1].data_type)) {
            BMCV_ERR_LOG("input attr must be same to output\n");

            return BM_NOT_SUPPORTED;
        }
    }
    if (!(((input[0].data_type == DATA_TYPE_EXT_1N_BYTE) &&
           (output[0].data_type == DATA_TYPE_EXT_FLOAT32)) ||
          ((input[0].data_type == DATA_TYPE_EXT_1N_BYTE) &&
           (output[0].data_type == DATA_TYPE_EXT_1N_BYTE)) ||
          ((input[0].data_type == DATA_TYPE_EXT_1N_BYTE_SIGNED) &&
           (output[0].data_type == DATA_TYPE_EXT_1N_BYTE_SIGNED)) ||
          ((input[0].data_type == DATA_TYPE_EXT_1N_BYTE) &&
           (output[0].data_type == DATA_TYPE_EXT_1N_BYTE_SIGNED)) ||
          ((input[0].data_type == DATA_TYPE_EXT_FLOAT32) &&
           (output[0].data_type == DATA_TYPE_EXT_FLOAT32)) ||
          ((input[0].data_type == DATA_TYPE_EXT_4N_BYTE) &&
           (output[0].data_type == DATA_TYPE_EXT_FLOAT32)) ||
          ((input[0].data_type == DATA_TYPE_EXT_4N_BYTE) &&
           (output[0].data_type == DATA_TYPE_EXT_4N_BYTE_SIGNED)) ||
          ((input[0].data_type == DATA_TYPE_EXT_4N_BYTE) &&
           (output[0].data_type == DATA_TYPE_EXT_4N_BYTE)))) {
        BMCV_ERR_LOG("data type not support\n");

        return BM_NOT_SUPPORTED;
    }
    if ((input[0].data_type == DATA_TYPE_EXT_FP16) ||
        (output[0].data_type == DATA_TYPE_EXT_FP16)||
        (input[0].data_type == DATA_TYPE_EXT_BF16) ||
        (output[0].data_type == DATA_TYPE_EXT_BF16)){
        BMCV_ERR_LOG("data type not support\n");

        return BM_NOT_SUPPORTED;
    }

    if (((input[0].image_format != FORMAT_BGR_PLANAR) &&
         (input[0].image_format != FORMAT_RGB_PLANAR) &&
         (input[0].image_format != FORMAT_GRAY)) ||
        (input[0].image_format != output[0].image_format)) {
        BMCV_ERR_LOG("image format not support\n");

        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_image_convert_to_(bm_handle_t          handle,
                                   int                  input_num,
                                   bmcv_convert_to_attr convert_to_attr,
                                   bm_image *           input,
                                   bm_image *           output) {
    if (BM_SUCCESS != bmcv_convert_to_check(
                          handle, input_num, convert_to_attr, input, output)) {
        BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

        return BM_ERR_FAILURE;
    }
    int             in_concat_status  = 0;
    int             out_concat_status = 0;
    bm_image_tensor in_tensor[MAX_INPUT_NUM];
    bm_image_tensor out_tensor[32];
    int             w_stride = 0;
    bm_convert_to_get_stride(input[0], w_stride);
    int if_4N_to_1N = ((input[0].data_type == DATA_TYPE_EXT_4N_BYTE) &&
                       (output[0].data_type == DATA_TYPE_EXT_FLOAT32))
                          ? (1)
                          : (0);
    for (int output_idx = 0;
         output_idx < ((if_4N_to_1N) ? (input_num * 4) : (input_num));
         output_idx++) {
        if (!bm_image_is_attached(output[output_idx])) {
            if (BM_SUCCESS !=
                bm_image_alloc_dev_mem(output[output_idx], BMCV_HEAP_ANY)) {
                for (int free_idx = 0; free_idx < output_idx; free_idx++) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }

                return BM_ERR_FAILURE;
            }
        }
    }
    if (BM_SUCCESS ==
        concat_images_to_tensor(handle, input_num, input, in_tensor)) {
        in_concat_status = 1;
    }
    if (BM_SUCCESS ==
        concat_images_to_tensor(handle, input_num, output, out_tensor)) {
        out_concat_status = 1;
    }
    if ((in_concat_status == 1) && (out_concat_status == 1)) {
        bmcv_convert_to_internal(
            handle, convert_to_attr, w_stride, in_tensor[0], out_tensor[0]);
        bm_image_tensor_destroy(in_tensor[0]);
        bm_image_tensor_destroy(out_tensor[0]);
    } else {
        ASSERT_INFO(if_4N_to_1N == 0,
                    "mem must be continuous when in 4n to 1n mode\r\n");
        if (1 == in_concat_status) {
            bm_image_tensor_destroy(in_tensor[0]);
        }
        if (1 == out_concat_status) {
            bm_image_tensor_destroy(out_tensor[0]);
        }
        for (int i = 0; i < input_num; i++) {
            concat_images_to_tensor(handle, 1, &input[i], &in_tensor[i]);
            concat_images_to_tensor(handle, 1, &output[i], &out_tensor[i]);
            bmcv_convert_to_internal(
                handle, convert_to_attr, w_stride, in_tensor[i], out_tensor[i]);
            bm_image_tensor_destroy(in_tensor[i]);
            bm_image_tensor_destroy(out_tensor[i]);
        }
    }

    return BM_SUCCESS;
}

// in order to support input_num > 4
bm_status_t bmcv_image_convert_to(
    bm_handle_t          handle,
    int                  input_num,
    bmcv_convert_to_attr convert_to_attr,
    bm_image *           input,
    bm_image *           output)
{
    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid = BM1684X;
    int loop = (input_num + 3) / 4;
    int i = 0;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {
      case 0x1684:
        for (i = 0; i < loop; i++) {
          int num = (i == loop - 1) ? (input_num - (loop - 1) * 4) : 4;
          ret = bmcv_image_convert_to_(handle, num, convert_to_attr, input + i * 4, output + i * 4);
          if (ret != BM_SUCCESS) return ret;
        }
        break;

      case BM1684X:{
        if(input->data_type == DATA_TYPE_EXT_4N_BYTE || input->data_type == DATA_TYPE_EXT_4N_BYTE_SIGNED){
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
            "not support, %s: %s: %d\n",
            filename(__FILE__), __func__, __LINE__);
            ret = BM_NOT_SUPPORTED;
        } else if(input->data_type == DATA_TYPE_EXT_FLOAT32 || input->data_type == DATA_TYPE_EXT_1N_BYTE_SIGNED){
            for (i = 0; i < loop; i++) {
                int num = (i == loop - 1) ? (input_num - (loop - 1) * 4) : 4;
                ret = bmcv_image_convert_to_(handle, num, convert_to_attr, input + i * 4, output + i * 4);
                if (ret != BM_SUCCESS) return ret;
            }
        }else{
            ret = bm1684x_vpp_convert_to(handle, input_num, convert_to_attr, input, output);
        }
        break;
      }
      default:
        ret = BM_NOT_SUPPORTED;
        break;
      }

    return ret;
}

