#include <stdint.h>
#include <stdio.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"

#ifdef __linux__
#include <unistd.h>
#else
  #include <windows.h>
#endif

static bm_status_t
bmcv_img_scale_check(bm_handle_t handle, bmcv_image input, bmcv_image output) {
    int if_4n = ((BGR4N == input.image_format) || (RGB4N == input.image_format))
                    ? (1)
                    : (0);
    int if_fp32 = (DATA_TYPE_FLOAT == input.data_format) ? (1) : (0);
    if ((if_4n == 1) && (if_fp32 == 1)) {
        BMCV_ERR_LOG("Not support 4n and fp32\n");

        return BM_NOT_SUPPORTED;
    }
    if (!handle) {
        BMCV_ERR_LOG("Can not get handle!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if ((input.color_space != COLOR_RGB) || (output.color_space != COLOR_RGB)) {
        BMCV_ERR_LOG("color_space not support\r\n");

        return BM_NOT_SUPPORTED;
    }
    if ((input.image_format != BGR) && (input.image_format != RGB) &&
        (input.image_format != BGR4N) && (input.image_format != RGB4N)) {
        BMCV_ERR_LOG("input image_format not support:%d\n", input.image_format);

        return BM_NOT_SUPPORTED;
    }
    if ((output.image_format != BGR) && (output.image_format != RGB) &&
        (output.image_format != BGR4N) && (output.image_format != RGB4N)) {
        BMCV_ERR_LOG("output image_format not support:%d\n",
                     output.image_format);

        return BM_NOT_SUPPORTED;
    }
    if ((input.data_format != DATA_TYPE_FLOAT) &&
        (input.data_format != DATA_TYPE_BYTE)) {
        BMCV_ERR_LOG("input data format not support:%d\n", input.data_format);

        return BM_NOT_SUPPORTED;
    }
    if ((output.data_format != DATA_TYPE_FLOAT) &&
        (output.data_format != DATA_TYPE_BYTE)) {
        BMCV_ERR_LOG("output data format not support:%d\n", output.data_format);

        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_img_scale(bm_handle_t   handle,
                           bmcv_image    input,
                           int           n,
                           int           do_crop,
                           int           top,
                           int           left,
                           int           height,
                           int           width,
                           unsigned char stretch,
                           unsigned char padding_b,
                           unsigned char padding_g,
                           unsigned char padding_r,
                           int           pixel_weight_bias,
                           float         weight_b,
                           float         bias_b,
                           float         weight_g,
                           float         bias_g,
                           float         weight_r,
                           float         bias_r,
                           bmcv_image    output) {
    if (BM_SUCCESS != bmcv_img_scale_check(handle, input, output)) {
        BMCV_ERR_LOG("bmcv_img_scale_check error\r\n");

        return BM_ERR_FAILURE;
    }
    bmcv_resize_image resize_attr[4];
    bmcv_resize_t     resize_img_attr[4];
    int               in_w = 0;
    int               in_h = 0;
    int               in_x = 0;
    int               in_y = 0;
    bm_image          input_image[4], output_image[4];

    if (do_crop) {
        in_w = width;
        in_h = height;
        in_x = left;
        in_y = top;
    } else {
        in_w = input.image_width;
        in_h = input.image_height;
        in_x = 0;
        in_y = 0;
    }
    for (int img_idx = 0; img_idx < n; img_idx++) {
        resize_img_attr[img_idx].start_x    = in_x;
        resize_img_attr[img_idx].start_y    = in_y;
        resize_img_attr[img_idx].in_width   = in_w;
        resize_img_attr[img_idx].in_height  = in_h;
        resize_img_attr[img_idx].out_width  = output.image_width;
        resize_img_attr[img_idx].out_height = output.image_height;
    }
    for (int img_idx = 0; img_idx < n; img_idx++) {
        resize_attr[img_idx].resize_img_attr = &resize_img_attr[img_idx];
        resize_attr[img_idx].roi_num         = 1;
        resize_attr[img_idx].stretch_fit     = stretch;
        resize_attr[img_idx].interpolation   = BMCV_INTER_NEAREST;
        if ((RGB == input.image_format) || (RGB4N == input.image_format)) {
            resize_attr[img_idx].padding_b = padding_r;
            resize_attr[img_idx].padding_r = padding_g;
            resize_attr[img_idx].padding_g = padding_b;
        } else if ((BGR == input.image_format) ||
                   (BGR4N == input.image_format)) {
            resize_attr[img_idx].padding_b = padding_r;
            resize_attr[img_idx].padding_r = padding_b;
            resize_attr[img_idx].padding_g = padding_g;
        }
    }
    int data_type =
        (((DATA_TYPE_BYTE == input.data_format) &&
          (RGB == input.image_format)) ||
         ((DATA_TYPE_BYTE == input.data_format) && (BGR == input.image_format)))
            ? (DATA_TYPE_EXT_1N_BYTE)
            : ((DATA_TYPE_BYTE == input.data_format) ? (DATA_TYPE_EXT_4N_BYTE)
                                                     : (DATA_TYPE_EXT_FLOAT32));
    for (int i = 0; i < n; i++) {
        bm_image_create(handle,
                        input.image_height,
                        input.image_width,
                        FORMAT_BGR_PLANAR,
                        (bm_image_data_format_ext)data_type,
                        &input_image[i]);
        bm_image_create(handle,
                        output.image_height,
                        output.image_width,
                        FORMAT_BGR_PLANAR,
                        (bm_image_data_format_ext)data_type,
                        &output_image[i]);
    }
    int c = 3;
    if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS !=
            bm_image_alloc_contiguous_mem(n, input_image, BMCV_IMAGE_FOR_OUT)) {
            BMCV_ERR_LOG("bm_image_alloc_contiguous_mem error\r\n");
            for (int i = 0; i < n; i++) {
                bm_image_destroy(input_image[i]);
                bm_image_destroy(output_image[i]);
            }

            return BM_ERR_FAILURE;
        }
        char *input_addr = (char *)bm_mem_get_system_addr(input.data[0]);
        int   img_offset = input_image[0].width * input_image[0].height * c *
                         (data_type == DATA_TYPE_EXT_1N_BYTE ? 1 : 4);
        for (int i = 0; i < n; i++) {
            char *in_img_data = input_addr + img_offset * i;
            bm_image_copy_host_to_device(input_image[i],
                                         (void **)(&in_img_data));
        }
    } else {
        bm_image_attach_contiguous_mem(n, input_image, input.data[0]);
    }
    if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_image_alloc_contiguous_mem(
                              n, output_image, BMCV_IMAGE_FOR_OUT)) {
            BMCV_ERR_LOG("bm_image_alloc_contiguous_mem error\r\n");
            if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
                bm_image_free_contiguous_mem(n, input_image);
            }
            for (int i = 0; i < n; i++) {
                bm_image_destroy(input_image[i]);
                bm_image_destroy(output_image[i]);
            }

            return BM_ERR_FAILURE;
        }
    } else {
        bm_image_attach_contiguous_mem(n, output_image, output.data[0]);
    }
    bm_image temp_input_image[4];
    if (pixel_weight_bias) {
        bmcv_convert_to_attr convert_to_attr;
        if ((BGR == input.image_format) || (BGR4N == input.image_format)) {
            convert_to_attr.alpha_0 = weight_b;
            convert_to_attr.beta_0  = bias_b;
            convert_to_attr.alpha_1 = weight_g;
            convert_to_attr.beta_1  = bias_g;
            convert_to_attr.alpha_2 = weight_r;
            convert_to_attr.beta_2  = bias_r;
        } else {
            convert_to_attr.alpha_0 = weight_r;
            convert_to_attr.beta_0  = bias_r;
            convert_to_attr.alpha_1 = weight_g;
            convert_to_attr.beta_1  = bias_g;
            convert_to_attr.alpha_2 = weight_b;
            convert_to_attr.beta_2  = bias_b;
        }
        for (int i = 0; i < n; i++) {
            bm_image_create(handle,
                            input.image_height,
                            input.image_width,
                            FORMAT_BGR_PLANAR,
                            (bm_image_data_format_ext)data_type,
                            &temp_input_image[i]);
        }
        if (BM_SUCCESS != bm_image_alloc_contiguous_mem(
                              n, temp_input_image, BMCV_IMAGE_FOR_OUT)) {
            BMCV_ERR_LOG("bm_image_alloc_contiguous_mem error\r\n");
            if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
                bm_image_free_contiguous_mem(n, input_image);
            }
            if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM) {
                bm_image_free_contiguous_mem(n, output_image);
            }
            for (int i = 0; i < n; i++) {
                bm_image_destroy(input_image[i]);
                bm_image_destroy(output_image[i]);
                bm_image_destroy(temp_input_image[i]);
            }

            return BM_ERR_FAILURE;
        }
        bmcv_image_convert_to(
            handle, n, convert_to_attr, input_image, temp_input_image);
        bmcv_image_resize(handle, n, resize_attr, temp_input_image, output_image);
    } else {
        bmcv_image_resize(handle, n, resize_attr, input_image, output_image);
    }
    if (bm_mem_get_type(input.data[0]) == BM_MEM_TYPE_SYSTEM) {
        bm_image_free_contiguous_mem(n, input_image);
    }
    if (bm_mem_get_type(output.data[0]) == BM_MEM_TYPE_SYSTEM) {
        char *output_addr = (char *)bm_mem_get_system_addr(output.data[0]);
        int   img_offset  = output_image[0].width * output_image[0].height * c *
                         (data_type == DATA_TYPE_EXT_1N_BYTE ? 1 : 4);
        for (int i = 0; i < n; i++) {
            char *out_img_data = output_addr + img_offset * i;
            bm_image_copy_device_to_host(output_image[i],
                                         (void **)(&out_img_data));
        }
        bm_image_free_contiguous_mem(n, output_image);
    }
    for (int i = 0; i < n; i++) {
        bm_image_destroy(input_image[i]);
    }
    for (int i = 0; i < n; i++) {
        bm_image_destroy(output_image[i]);
    }
    if (pixel_weight_bias) {
        bm_image_free_contiguous_mem(n, temp_input_image);
        for (int i = 0; i < n; i++) {
            bm_image_destroy(temp_input_image[i]);
        }
    }

    return BM_SUCCESS;
}
