#include <stdint.h>
#include <stdio.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmlib_interface.h"
#include "bm1684x/bmcv_1684x_vpp_ext.h"

#ifdef __linux__
#include <unistd.h>
#else
  #include <windows.h>
#endif
#ifndef USING_CMODEL
#include "vpplib.h"
#endif

typedef int32_t bm_resize_data_type_t;

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

int if_use_vpp(bm_image *        input,
               bm_image *        output,
               int               input_num,
               int               output_num,
               bmcv_resize_image resize_attr[MAX_INPUT_NUM])
{
    // int width_stride = 0;
    bm_image_format_ext input_format;
    bm_image_format_ext output_format;
    int idx = 0;

    if ((input_num > 16) || (output_num > 16)) {
        return 0;
    }

    // stretch fit
    for (int i = 0; i < input_num; i++) {
        if (resize_attr[i].stretch_fit == 0) {
            return 0;
        }
        idx += resize_attr[i].roi_num;
    }
    idx = 0;
    for (int i = 0; i < input_num; i++) {
        // only 1nit8->1n int8
        if (input[i].data_type != DATA_TYPE_EXT_1N_BYTE) {
            return 0;
        }

#if USING_DDR_LIMITATION
        // ddr limit
        bm_device_mem_t dev_mem[4];
        bm_image_get_device_mem(input[i], dev_mem);
        for (int plane = 0; plane < bm_image_get_plane_num(input[i]); plane++){
            u64 dev_addr = bm_mem_get_device_addr(dev_mem[plane]);
            if ((dev_addr >= 0x100000000) && (dev_addr < 0x300000000)) {
                return 0;
            }
            if (dev_addr % 32 != 0) {
                return 0;
            }
        }
#endif
        /* joint check for input and output */
        input_format = input[i].image_format;
        for (int roi = 0; roi < resize_attr[i].roi_num; roi++, idx++){
            vpp_limitation limit;
            bmcv_resize_t *attr = resize_attr[i].resize_img_attr + roi;
            output_format = output[idx].image_format;
            if (bm_vpp_query_limitation(input_format, output_format, limit) != BM_SUCCESS)
                return 0;

            if (!limit.support_scale)
                return 0;

            if ((attr->in_width % limit.src_width_align != 0)   ||
                (attr->in_height % limit.src_height_align != 0) ||
                (attr->start_x % limit.src_offset_x_align != 0) ||
                (attr->start_y % limit.src_offset_y_align != 0) ||
                (output[i].width % limit.dst_width_align != 0)  ||
                (output[i].height % limit.dst_height_align != 0))
                return 0;

            // xun: bmcv has alignmen before vpp operation, so we do not
            // check input and output stride here

            // only 1nit8->1n int8
            if (output[idx].data_type != DATA_TYPE_EXT_1N_BYTE) {
                return 0;
            }
        }
    }

    return 1;
}

static bm_status_t bmcv_resize_check(
        bm_handle_t       handle,
        int               image_num,
        bmcv_resize_image resize_attr[MAX_INPUT_NUM],
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
    for (int i = 0; i < image_num; i++) {
        out_num += resize_attr[i].roi_num;
    }
    bool out_is_4N = ((dst_type == DATA_TYPE_EXT_4N_BYTE) ||
                      (dst_type == DATA_TYPE_EXT_4N_BYTE_SIGNED));
    out_num = out_is_4N ? (out_num + 3) / 4 : out_num;
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
#if TPU_ONLY
    bool use_vpp = false;
#else
    bool use_vpp = if_use_vpp(input, output, image_num, out_num, resize_attr);
#endif
    if (use_vpp) return BM_SUCCESS;

    if (src_format != FORMAT_RGB_PLANAR && src_format != FORMAT_BGR_PLANAR &&
        src_format != FORMAT_RGB_PACKED && src_format != FORMAT_BGR_PACKED &&
        src_format != FORMAT_GRAY) {
        BMCV_ERR_LOG("[RESIZE]: src format not support:%d\n", src_format);
        return BM_NOT_SUPPORTED;
    }
    if (dst_format != FORMAT_RGB_PLANAR && dst_format != FORMAT_BGR_PLANAR &&
        dst_format != FORMAT_RGB_PACKED && dst_format != FORMAT_BGR_PACKED &&
        dst_format != FORMAT_GRAY) {
        BMCV_ERR_LOG("[RESIZE]: dst format not support:%d\n", dst_format);
        return BM_NOT_SUPPORTED;
    }
    if (((input[0].data_type == DATA_TYPE_EXT_1N_BYTE) &&
         (output[0].data_type == DATA_TYPE_EXT_FLOAT32)) ||
        ((output[0].data_type == DATA_TYPE_EXT_1N_BYTE) &&
         (input[0].data_type == DATA_TYPE_EXT_FLOAT32))) {
        BMCV_ERR_LOG("[RESIZE]: image data type not support\n");
        return BM_NOT_SUPPORTED;
    }

    if ((input[0].data_type == DATA_TYPE_EXT_FP16) ||
        (output[0].data_type == DATA_TYPE_EXT_FP16)||
        (input[0].data_type == DATA_TYPE_EXT_BF16) ||
        (output[0].data_type == DATA_TYPE_EXT_BF16)){
        BMCV_ERR_LOG("data type not support\n");

        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_resize_internal(bm_handle_t       handle,
                                 int               image_num,
                                 bmcv_resize_image resize_attr[MAX_INPUT_NUM],
                                 bm_image_tensor   input,
                                 bm_image_tensor   output) {
    bm_api_cv_resize_t arg;
    int                one_policy_in_diff_images = 1;
    int if_4N_to_1N = ((input.image.data_type == DATA_TYPE_EXT_4N_BYTE) &&
                       (output.image.data_type == DATA_TYPE_EXT_1N_BYTE))
                          ? (1)
                          : (0);
    #ifdef __linux__
    int roi_num_array[image_num];
    #else
    std::shared_ptr<int> roi_num_array_(new int[image_num], std::default_delete<int[]>());
    int*                 roi_num_array = roi_num_array_.get();
    #endif
    int if_multi_roi, total_roi_nums = 0;
    CHECK_MULTI_ROI_AND_ONE_POLICY(one_policy_in_diff_images, if_multi_roi);
    for (int i = 0; i < image_num; i++) {
        roi_num_array[i] = resize_attr[i].roi_num;
    }

    bm_resize_data_type_t data_type =
        store_mode_transfer(input.image.data_type);
    bm_device_mem_t img_attr_dev;
    int resize_img_attr_size = total_roi_nums * sizeof(bmcv_resize_t);
    if (BM_SUCCESS !=
        bm_malloc_device_byte(handle, &img_attr_dev, resize_img_attr_size)) {
        BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

        return BM_ERR_FAILURE;
    }
    u64          resize_attr_start_addr = bm_mem_get_device_addr(img_attr_dev);
    u64          temp_addr              = resize_attr_start_addr;
    unsigned int temp_size = resize_attr[0].roi_num * sizeof(bmcv_resize_t);
    bm_device_mem_t tmp_dev_mem;
    bool            output_mem_init_flag = false;
    for (int i = 0; i < image_num; i++) {
        // bm_mem_set_device_addr(tmp_dev_mem, temp_addr);
        // bm_mem_set_device_size(tmp_dev_mem, temp_size);
        tmp_dev_mem = bm_mem_from_device(temp_addr, temp_size);
        if (BM_SUCCESS != bm_memcpy_s2d(handle,
                                        tmp_dev_mem,
                                        resize_attr[i].resize_img_attr)) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
            bm_free_device(handle, img_attr_dev);

            return BM_ERR_FAILURE;
        }
        if (i != (image_num - 1)) {
            temp_addr = temp_addr + temp_size;
            temp_size = resize_attr[i + 1].roi_num * sizeof(bmcv_resize_t);
        }
    }
    if (!bm_image_tensor_is_attached(output)) {
        if (BM_SUCCESS !=
            bm_image_tensor_alloc_dev_mem(output, BMCV_HEAP0_ID)) {
            output_mem_init_flag = true;
            BMCV_ERR_LOG("bm_image_tensor_alloc_dev_mem error\r\n");
            bm_free_device(handle, img_attr_dev);

            return BM_ERR_FAILURE;
        }
    }
    bm_device_mem_t input_image_dev, output_image_dev;
    bm_image_get_device_mem(input.image, &input_image_dev);
    bm_image_get_device_mem(output.image, &output_image_dev);
    bm_device_mem_t roi_num_array_dev;
    if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                            &roi_num_array_dev,
                                            image_num * sizeof(int))) {
        BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
        bm_free_device(handle, img_attr_dev);
        if (output_mem_init_flag) {
            bm_device_mem_t dmem;
            bm_image_tensor_get_device_mem(output, &dmem);
            bm_free_device(handle, dmem);
        }

        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_memcpy_s2d(handle, roi_num_array_dev, roi_num_array)) {
        BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
        bm_free_device(handle, img_attr_dev);
        if (output_mem_init_flag) {
            bm_device_mem_t dmem;
            bm_image_tensor_get_device_mem(output, &dmem);
            bm_free_device(handle, dmem);
        }
        bm_free_device(handle, roi_num_array_dev);
    }

    image_num = ((STORAGE_MODE_4N_INT8 == data_type) && (!if_4N_to_1N))
                    ? ((image_num + 3) / 4)
                    : (image_num);
    arg.in_img_addr   = bm_mem_get_device_addr(input_image_dev);
    arg.out_img_addr  = bm_mem_get_device_addr(output_image_dev);
    arg.in_channel    = input.image_c;
    arg.img_para_addr = bm_mem_get_device_addr(img_attr_dev);
    arg.roi_buf       = bm_mem_get_device_addr(roi_num_array_dev);
    arg.in_data_type  = data_type;
    arg.out_data_type = store_mode_transfer(output.image.data_type);
    arg.width_stride  = input.image.width;
    arg.height_stride = input.image.height;
    arg.image_num     = image_num;
    arg.need_switch =
        (input.image.image_format != output.image.image_format) ? (1) : (0);
    arg.one_policy_in_diff_images = one_policy_in_diff_images;
    arg.if_4N_to_1N               = if_4N_to_1N;
    arg.multi_roi                 = if_multi_roi;
    arg.stretch_fit               = (int)resize_attr[0].stretch_fit;
    arg.padding_b                 = (int)resize_attr[0].padding_b;
    arg.padding_r                 = (int)resize_attr[0].padding_r;
    arg.padding_g                 = (int)resize_attr[0].padding_g;
    if (BM_SUCCESS != bm_send_api(handle,  BM_API_ID_CV_RESIZE, (uint8_t *)&arg, sizeof(arg))) {
        BMCV_ERR_LOG("resize send api error\r\n");
        bm_free_device(handle, img_attr_dev);
        bm_free_device(handle, roi_num_array_dev);
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_sync_api(handle)) {
        BMCV_ERR_LOG("resize sync api error\r\n");
        bm_free_device(handle, img_attr_dev);
        bm_free_device(handle, roi_num_array_dev);
        return BM_ERR_FAILURE;
    }
    bm_free_device(handle, img_attr_dev);
    bm_free_device(handle, roi_num_array_dev);

    return BM_SUCCESS;
}

#ifndef USING_CMODEL
static int bm_image_to_vpp_mat(bm_handle_t handle, bm_image image, vpp_mat_s* mat)
{
    memset(mat, 0, sizeof(vpp_mat));

    #ifdef __linux__
    bm_get_handle_fd(handle, VPP_FD, &mat->vpp_fd.dev_fd);
    memcpy(mat->vpp_fd.name, "bm-vpp", sizeof("bm-vpp"));

    #else

    #endif
    mat->vpp_fd.handle = handle;

    mat->cols = image.width;
    mat->rows = image.height;

    bm_device_mem_t mem[4];
    bm_image_get_device_mem(image, mem);
    uint64_t global_address[4] = {0};
    int plane_num = bm_image_get_plane_num(image);
    for(int i = 0; i < plane_num; i++)
    {
        global_address[i] = bm_mem_get_device_addr(mem[i]);
    }
    mat->is_pa = 1;

    layout::plane_layout memory_layout[4];
    {
        std::lock_guard<std::mutex> lock(image.image_private->memory_lock);
        for (int k = 0; k < image.image_private->plane_num; k++)
        {
            memory_layout[k] = image.image_private->memory_layout[k];
        }
    }
    int ret = 0;
    switch(image.image_format)
    {
        case FORMAT_YUV420P:
            mat->format = FMT_I420;
            mat->num_comp = 3;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->ion_len[1] = memory_layout[1].size;
            mat->ion_len[2] = memory_layout[2].size;
            mat->pa[0] = global_address[0];
            mat->pa[1] = global_address[1];
            mat->pa[2] = global_address[2];
            break;
        case FORMAT_GRAY:
            mat->format = FMT_Y;
            mat->num_comp = 1;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->pa[0] = global_address[0];
            break;
        case FORMAT_YUV444P:
            mat->format = FMT_YUV444P;
            mat->num_comp = 3;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].channel_stride;
            mat->ion_len[1] = memory_layout[0].channel_stride;
            mat->ion_len[2] = memory_layout[0].channel_stride;
            mat->pa[0] = global_address[0];
            mat->pa[1] = global_address[1];
            mat->pa[2] = global_address[2];
            break;
        case FORMAT_RGB_PLANAR:
            mat->format = FMT_RGBP;
            mat->num_comp = 3;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].channel_stride;
            mat->ion_len[1] = memory_layout[0].channel_stride;
            mat->ion_len[2] = memory_layout[0].channel_stride;
            mat->pa[0] = global_address[0];
            mat->pa[1] = mat->pa[0] + mat->ion_len[0];
            mat->pa[2] = mat->pa[1] + mat->ion_len[1];
            break;
        case FORMAT_BGR_PLANAR:
            mat->format = FMT_BGRP;
            mat->num_comp = 3;
            mat->stride     = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].channel_stride;
            mat->ion_len[1] = memory_layout[0].channel_stride;
            mat->ion_len[2] = memory_layout[0].channel_stride;
            mat->pa[0] = global_address[0];
            mat->pa[1] = mat->pa[0] + mat->ion_len[0];
            mat->pa[2] = mat->pa[1] + mat->ion_len[1];
            break;
        case FORMAT_RGBP_SEPARATE:
            mat->format = FMT_RGBP;
            mat->num_comp = 3;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->ion_len[1] = memory_layout[1].size;
            mat->ion_len[2] = memory_layout[2].size;
            mat->pa[0] = global_address[0];
            mat->pa[1] = global_address[1];
            mat->pa[2] = global_address[2];
            break;
        case FORMAT_BGRP_SEPARATE:
            mat->format = FMT_BGRP;
            mat->num_comp = 3;
            mat->stride     = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->ion_len[1] = memory_layout[1].size;
            mat->ion_len[2] = memory_layout[2].size;
            mat->pa[0] = global_address[0];
            mat->pa[1] = global_address[1];
            mat->pa[2] = global_address[2];
            break;
        case FORMAT_BGR_PACKED:
            mat->format = FMT_BGR;
            mat->num_comp = 1;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->pa[0] = global_address[0];
            break;
        case FORMAT_RGB_PACKED:
            mat->format = FMT_RGB;
            mat->num_comp = 1;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->pa[0] = global_address[0];
            break;
        case FORMAT_NV12:
            mat->format = FMT_NV12;
            mat->num_comp = 2;
            mat->stride     = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->ion_len[1] = memory_layout[1].size;
            mat->pa[0] = global_address[0];
            mat->pa[1] = global_address[1];
            break;
        case FORMAT_COMPRESSED:
            mat->format = FMT_NV12;
            mat->num_comp = 4;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->ion_len[1] = memory_layout[1].size;
            mat->ion_len[2] = memory_layout[2].size;
            mat->ion_len[3] = memory_layout[3].size;
            mat->pa[0] = global_address[0];
            mat->pa[1] = global_address[1];
            mat->pa[2] = global_address[2];
            mat->pa[3] = global_address[3];
            break;
        default:
            ret = -1;
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_INFO, "vpp not support this format %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            break;
    }
    return ret;
}

int vpp_format_mat(vpp_mat *mat, int format, int w, int h) {
    if (mat == NULL) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "mat is NULL  %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return VPP_ERR;
    }
    int ret = VPP_OK;

    memset(mat, 0, sizeof(vpp_mat));
    mat->format = (vpp_img_fmt)format;
    mat->cols   = w;
    mat->rows   = h;

    switch (mat->format) {
        case FMT_BGR:
        case FMT_RGB:
            mat->num_comp   = 1;
            mat->stride     = ALIGN(w * 3, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            break;
        case FMT_BGRP:
        case FMT_RGBP:
            mat->num_comp   = 3;
            mat->stride     = ALIGN(w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->ion_len[1] = mat->ion_len[2] =
                mat->stride * h;
            break;
        case FMT_I420:
            mat->num_comp   = 3;
            mat->stride     = ALIGN(w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = (h >> 1) * (mat->stride >> 1);
            mat->ion_len[2] = mat->ion_len[1];
            break;
        case FMT_NV12:
            mat->num_comp   = 2;
            mat->stride     = ALIGN(w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            mat->ion_len[1] = (h >> 1) * mat->stride;
            break;
        case FMT_Y:
            mat->num_comp   = 1;
            mat->stride     = ALIGN(w, STRIDE_ALIGN);
            mat->ion_len[0] = mat->stride * h;
            break;
        default:
            bmlib_log(
                BMCV_LOG_TAG, BMLIB_LOG_ERROR, "format = %d", mat->format);
            ret = VPP_ERR;
            break;
    }

    return ret;
}

int cv_vpp_format_convert(int cv_format, int *vpp_format) {
    switch (cv_format) {
        case (FORMAT_BGR_PACKED): {
            *vpp_format = FMT_BGR;
            break;
        }
        case (FORMAT_RGB_PACKED): {
            *vpp_format = FMT_RGB;
            break;
        }
        case (FORMAT_RGB_PLANAR): {
            *vpp_format = FMT_RGBP;
            break;
        }
        case (FORMAT_BGR_PLANAR): {
            *vpp_format = FMT_BGRP;
            break;
        }
        case (FORMAT_NV12): {
            *vpp_format = FMT_NV12;
            break;
        }
        case (FORMAT_GRAY): {
            *vpp_format = FMT_Y;
            break;
        }
        default: {
            BMCV_ERR_LOG("format not support\r\n");
            break;
        }
    }
    return 0;
}

int vpp_fill_mat_with_device_mem(vpp_mat *   mat,
                                 bm_image    image,
                                 bm_handle_t handle) {
    int format;
    cv_vpp_format_convert((int)image.image_format, &format);
    vpp_format_mat(mat, format, image.width, image.height);
    mat->is_pa         = 1;
    #ifdef __linux__
    bm_get_handle_fd(handle, VPP_FD, &mat->vpp_fd.dev_fd);
    memcpy(mat->vpp_fd.name, "bm-vpp", sizeof("bm-vpp"));

    #else
    /*mat->vpp_fd.dev_fd = handle->vpp_fd.dev_fd;
    memset(mat->vpp_fd.name, 0x0, sizeof(mat->vpp_fd.name));
    if (mat->vpp_fd.dev_fd > 0) {
        memcpy(
            mat->vpp_fd.name, handle->vpp_fd.name, strlen(handle->vpp_fd.name));
    } else {
        memcpy(mat->vpp_fd.name, "bm-vpp", sizeof("bm-vpp"));
    }*/
    #endif
    mat->vpp_fd.handle = handle;
    #ifdef __linux__
    bm_device_mem_t dev_mem[mat->num_comp];
    #else
    std::shared_ptr<bm_device_mem_t> dev_mem_(new bm_device_mem_t[mat->num_comp], std::default_delete<bm_device_mem_t[]>());
    bm_device_mem_t* dev_mem = dev_mem_.get();
    #endif
    bm_image_get_device_mem(image, dev_mem);
    u64 start_addr = bm_mem_get_device_addr(dev_mem[0]);
    for (int idx = 0; idx < mat->num_comp; idx++) {
        mat->pa[idx] = start_addr;
        mat->fd[idx] = 0;
        start_addr += mat->ion_len[idx];
    }

    return 0;
}

int vpp_fill_batch_mat_with_device_mem(vpp_mat *   mat,
                                       bm_image    image,
                                       int         image_c,
                                       int         image_num,
                                       bm_handle_t handle) {
    int format;
    int data_size = 1;
    for (int i = 0; i < image_num; i++) {
        cv_vpp_format_convert((int)image.image_format, &format);
        vpp_format_mat(&mat[i], format, image.width, image.height);
        mat[i].is_pa       = 1;
        #ifdef __linux__
        bm_get_handle_fd(handle, VPP_FD, &mat->vpp_fd.dev_fd);
        memcpy(mat->vpp_fd.name, "bm-vpp", sizeof("bm-vpp"));
        #else
        /* mat->vpp_fd.dev_fd = handle->vpp_fd.dev_fd;
        memset(mat->vpp_fd.name, 0x0, sizeof(mat->vpp_fd.name));
        if (mat->vpp_fd.dev_fd > 0) {
            memcpy(mat->vpp_fd.name,
                   handle->vpp_fd.name,
                   strlen(handle->vpp_fd.name));
        } else {
            memcpy(mat->vpp_fd.name, "bm-vpp", sizeof("bm-vpp"));
        }*/
        #endif
        mat->vpp_fd.handle = handle;
        #ifdef __linux__
        bm_device_mem_t dev_mem[mat[i].num_comp];
        #else
        std::shared_ptr<bm_device_mem_t> dev_mem_(new bm_device_mem_t[mat[i].num_comp], std::default_delete<bm_device_mem_t[]>());
        bm_device_mem_t* dev_mem = dev_mem_.get();
        #endif
        bm_image_get_device_mem(image, dev_mem);
        int single_len = image.width * image.height *
                         (image_c / mat[i].num_comp) * data_size;
        for (int idx = 0; idx < mat[i].num_comp; idx++) {
            mat[i].pa[idx] =
                bm_mem_get_device_addr(dev_mem[idx]) + i * single_len;
            mat[i].fd[idx] = 0;
        }
    }

    return 0;
}

int if_use_vpp_crop_mul(bm_image_tensor   input,
                        bm_image_tensor   output,
                        int               image_num,
                        bmcv_resize_image resize_attr[MAX_INPUT_NUM]) {
    // only 1nit8->1n int8
    if ((input.image.data_type != DATA_TYPE_EXT_1N_BYTE) ||
        (output.image.data_type != DATA_TYPE_EXT_1N_BYTE)) {
        return 0;
    }
    if (1 == image_num) {
        return 0;
    }
    for (int i = 0; i < image_num; i++) {
        if (1 != resize_attr[i].roi_num) {
            return 0;
        } else if (image_num > 16) {
            return 0;
        }
    }
    int one_policy_in_diff_images, if_multi_roi, total_roi_nums = 0;
    CHECK_MULTI_ROI_AND_ONE_POLICY(one_policy_in_diff_images, if_multi_roi);
    // multi roi
    if (1 == if_multi_roi) {
        return 0;
    }
    if (total_roi_nums != image_num) {
        return 0;
    }
    if ((input.image.image_format != FORMAT_BGR_PACKED) ||
        (output.image.image_format != FORMAT_BGR_PACKED)) {
        return 0;
    }
    int if_crop = 0;
    // stretch fit
    for (int i = 0; i < image_num; i++) {
        if (resize_attr[i].stretch_fit == 0) {
            return 0;
        }
        if_crop |= ONLY_DO_CROP(resize_attr[i].resize_img_attr[0]);
    }
    if (!if_crop) {
        return 0;
    }
    // width limit
    if ((input.image.width % 64 != 0) || (output.image.width % 64 != 0)) {
        return 0;
    }
#if USING_DDR_LIMITATION
    // ddr limit
    bm_device_mem_t dev_mem;
    bm_image_get_device_mem(input.image, &dev_mem);
    u64 dev_addr = bm_mem_get_device_addr(dev_mem);
    if ((dev_addr >= 0x100000000) && (dev_addr < 0x300000000)) {
        return 0;
    }
#endif

    return 1;
}

int if_use_vpp_crop_one(bm_image_tensor   input,
                        bm_image_tensor   output,
                        int               image_num,
                        bmcv_resize_image resize_attr[MAX_INPUT_NUM]) {
    // only 1nit8->1n int8
    if ((input.image.data_type != DATA_TYPE_EXT_1N_BYTE) ||
        (output.image.data_type != DATA_TYPE_EXT_1N_BYTE)) {
        return 0;
    }
    if (1 != image_num) {
        return 0;
    }
    for (int i = 0; i < image_num; i++) {
        if (1 == resize_attr[i].roi_num) {
            return 0;
        } else if (resize_attr[i].roi_num > 16) {
            return 0;
        }
    }
    int one_policy_in_diff_images, if_multi_roi, total_roi_nums = 0;
    CHECK_MULTI_ROI_AND_ONE_POLICY(one_policy_in_diff_images, if_multi_roi);
    // multi roi
    if (1 != if_multi_roi) {
        return 0;
    }
    if ((input.image.image_format != FORMAT_BGR_PACKED) ||
        (output.image.image_format != FORMAT_BGR_PACKED)) {
        return 0;
    }
    int if_crop = 0;
    // stretch fit
    for (int i = 0; i < image_num; i++) {
        if (resize_attr[i].stretch_fit == 0) {
            return 0;
        }
        for (int roi_idx = 0; roi_idx < resize_attr[i].roi_num; roi_idx++) {
            if_crop |= ONLY_DO_CROP(resize_attr[i].resize_img_attr[roi_idx]);
        }
    }
    if (!if_crop) {
        return 0;
    }
    // width limit
    if ((input.image.width % 64 != 0) || (output.image.width % 64 != 0)) {
        return 0;
    }
#if USING_DDR_LIMITATION
    // ddr limit
    bm_device_mem_t dev_mem;
    bm_image_get_device_mem(input.image, &dev_mem);
    u64 dev_addr = bm_mem_get_device_addr(dev_mem);
    if ((dev_addr >= 0x100000000) && (dev_addr < 0x300000000)) {
        return 0;
    }
#endif

    return 1;
}

int if_use_vpp_resize_crop_one(bm_image_tensor * input,
                               bm_image_tensor * output,
                               int               input_array_num,
                               int               output_array_num,
                               bmcv_resize_image resize_attr[MAX_INPUT_NUM]) {
    for (int i = 0; i < input_array_num; i++) {
        // only 1nit8->1n int8
        if (input[i].image.data_type != DATA_TYPE_EXT_1N_BYTE) {
            return 0;
        }
        // width limit
        if (input[i].image.width % 64 != 0) {
            return 0;
        }
        // if (input[i].image.image_format != FORMAT_BGR_PLANAR) {
        //    return 0;
        //}
#if USING_DDR_LIMITATION
        // ddr limit
        bm_device_mem_t dev_mem;
        bm_image_get_device_mem(input[i].image, &dev_mem);
        u64 dev_addr = bm_mem_get_device_addr(dev_mem);
        if ((dev_addr >= 0x100000000) || (dev_addr < 0x300000000)) {
            return 0;
        }
#endif
    }
    for (int i = 0; i < output_array_num; i++) {
        // only 1nit8->1n int8
        if (output[i].image.data_type != DATA_TYPE_EXT_1N_BYTE) {
            return 0;
        }
        // width limit
        if (output[i].image.width % 64 != 0) {
            return 0;
        }
        // if (output[i].image.image_format != FORMAT_BGR_PLANAR) {
        //    return 0;
        //}
    }
    int total_input_n  = 0;
    int total_output_n = 0;
    GET_TOTAL_BATCH_NUM(input, input_array_num, total_input_n);
    GET_TOTAL_BATCH_NUM(output, output_array_num, total_output_n);
    int one_policy_in_diff_images, if_multi_roi, total_roi_nums = 0;
    int image_num = total_input_n;
    CHECK_MULTI_ROI_AND_ONE_POLICY(one_policy_in_diff_images, if_multi_roi);
    // multi roi
    if (1 != if_multi_roi) {
        return 0;
    }
    if ((total_input_n > 16) || (total_output_n > 16)) {
        return 0;
    }
    // stretch fit
    for (int i = 0; i < total_input_n; i++) {
        if (resize_attr[i].stretch_fit == 0) {
            return 0;
        }
    }

    return 1;
}

int if_use_vpp_resize(bm_image_tensor   input,
                      bm_image_tensor   output,
                      int               image_num,
                      bmcv_resize_image resize_attr[MAX_INPUT_NUM]) {
    // only 1nit8->1n int8
    if ((input.image.data_type != DATA_TYPE_EXT_1N_BYTE) ||
        (output.image.data_type != DATA_TYPE_EXT_1N_BYTE)) {
        return 0;
    }
    int one_policy_in_diff_images, if_multi_roi, total_roi_nums = 0;
    CHECK_MULTI_ROI_AND_ONE_POLICY(one_policy_in_diff_images, if_multi_roi);
    // multi roi
    if (if_multi_roi || (!one_policy_in_diff_images)) {
        return 0;
    }
    // stretch fit
    for (int i = 0; i < input.image_n; i++) {
        if (resize_attr[i].stretch_fit == 0) {
            return 0;
        }
    }
    // width limit
    if ((input.image.width % 64 != 0) || (output.image.width % 64 != 0)) {
        return 0;
    }
    int if_crop = 0;
    // stretch fit
    for (int i = 0; i < image_num; i++) {
        for (int roi_idx = 0; roi_idx < resize_attr[i].roi_num; roi_idx++) {
            if_crop |= ONLY_DO_CROP(resize_attr[i].resize_img_attr[roi_idx]);
            if ((resize_attr[i].resize_img_attr[roi_idx].in_width !=
                 input.image.width) ||
                (resize_attr[i].resize_img_attr[roi_idx].in_height !=
                 input.image.height)) {
                return 0;
            }
        }
    }
    if (if_crop) {
        return 0;
    }
#if USING_DDR_LIMITATION
    // ddr limit
    bm_device_mem_t dev_mem;
    bm_image_get_device_mem(input.image, &dev_mem);
    u64 dev_addr = bm_mem_get_device_addr(dev_mem);
    if ((dev_addr >= 0x100000000) && (dev_addr < 0x300000000)) {
        return 0;
    }
#endif

    return 1;
}
#endif

bm_status_t bmcv_query_idx_in_array(int              query_idx,
                                    bm_image_tensor *array,
                                    int              arr_num,
                                    int *            arr_idx,
                                    int *            batch_idx) {
    int little_idx = 0;
    int big_idx    = array[0].image_n;
    for (int i = 0; i < arr_num; i++) {
        if ((query_idx < big_idx) && (query_idx >= little_idx)) {
            *arr_idx   = i;
            *batch_idx = query_idx - little_idx;

            return BM_SUCCESS;
        }
        if (i != arr_num - 1) {
            little_idx += array[i].image_n;
            big_idx += array[i + 1].image_n;
        }
    }
    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "find nothing \r\n");

    return BM_ERR_FAILURE;
}

bm_status_t try_image_align(bm_handle_t handle,
                            int         image_num,
                            bm_image *  src_images,
                            bm_image *  converted_images,
                            int *       image_align_flag) {
    #ifdef __linux__
    bool image_alloc_flag[image_num];
    #else
    std::shared_ptr<bool> image_alloc_flag_(new bool[image_num], std::default_delete<bool[]>());
    bool* image_alloc_flag = image_alloc_flag_.get();
    #endif
    for (int i = 0; i < image_num; i++) {
        int image_stride[3] = {0};
        image_align_flag[i] = 0;
        image_alloc_flag[i] = false;
        bm_image_get_stride(src_images[i], image_stride);

        int plane_num = bm_image_get_plane_num(src_images[i]);
        layout::plane_layout *plane;
        int data_size = 1;
        vpp_limitation limit;
        for (int idx = 0; idx < plane_num; idx++) {
            plane = bm_image_get_layout(src_images[i], idx);
            data_size = plane->data_size;

            /* use format_rgb_packed general foramt as output format */
            if (bm_vpp_query_limitation(src_images[i].image_format, \
                    FORMAT_RGB_PACKED, limit) != BM_SUCCESS){
                BMCV_ERR_LOG("format not support!\n");
                return BM_ERR_FAILURE;
            }

            if (image_stride[idx]%limit.src_stride_align[idx] != 0){
                image_stride[idx] =
                    ALIGN(plane->W, limit.src_stride_align[idx]) * data_size;
                image_align_flag[i] = 1;
            }
        }
        if (1 == image_align_flag[i]) {
            if (BM_SUCCESS != bm_image_create(handle,
                                              src_images[i].height,
                                              src_images[i].width,
                                              src_images[i].image_format,
                                              src_images[i].data_type,
                                              &converted_images[i],
                                              image_stride)) {
                BMCV_ERR_LOG("failed to create internal image\n");
                for (int free_idx = 0; free_idx < i; free_idx++) {
                    bm_image_destroy(converted_images[free_idx]);
                    if (image_alloc_flag[free_idx]) {
                        bm_device_mem_t dmem;
                        bm_image_get_device_mem(converted_images[free_idx],
                                                &dmem);
                        bm_free_device(handle, dmem);
                    }
                }

                return BM_ERR_FAILURE;
            }
            if (bm_image_is_attached(src_images[i])) {
                // keep same heap location with before
                int             heap_location = BMCV_HEAP1_ID;
                bm_device_mem_t dev_mem;
                bm_image_get_device_mem(src_images[i], &dev_mem);
                u64 dev_addr = bm_mem_get_device_addr(dev_mem);
                if ((dev_addr >= 0x100000000) && (dev_addr < 0x300000000)) {
                    heap_location = BMCV_HEAP_ANY;
                }
                if (BM_SUCCESS != bm_image_alloc_dev_mem(converted_images[i],
                                                         heap_location)) {
                    BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");
                    for (int free_idx = 0; free_idx <= i; free_idx++) {
                        bm_image_destroy(converted_images[free_idx]);
                    }
                    for (int free_idx = 0; free_idx < i; free_idx++) {
                        if (image_alloc_flag[free_idx]) {
                            bm_device_mem_t dmem;
                            bm_image_get_device_mem(converted_images[free_idx],
                                                    &dmem);
                            bm_free_device(handle, dmem);
                        }
                    }

                    return BM_ERR_FAILURE;
                }
                image_alloc_flag[i] = true;
            } else {
                BMCV_ERR_LOG("image should be attached memory firstly\n");
                for (int free_idx = 0; free_idx <= i; free_idx++) {
                    bm_image_destroy(converted_images[free_idx]);
                }
                return BM_ERR_FAILURE;
            }
            // bmcv_width_align(handle, src_images[i], converted_images[i]);
        } else {
            if (BM_SUCCESS != bm_image_create(handle,
                                              src_images[i].height,
                                              src_images[i].width,
                                              src_images[i].image_format,
                                              src_images[i].data_type,
                                              &converted_images[i],
                                              image_stride)) {
                BMCV_ERR_LOG("failed to create internal image\n");
                for (int free_idx = 0; free_idx < i; free_idx++) {
                    bm_image_destroy(converted_images[free_idx]);
                }

                return BM_ERR_FAILURE;
            }
            if (bm_image_is_attached(src_images[i])) {
                bm_device_mem_t dmem[3];
                bm_image_get_device_mem(src_images[i], dmem);
                bm_image_attach(converted_images[i], dmem);
            } else {
                BMCV_ERR_LOG("image should be attached memory firstly\n");
                for (int free_idx = 0; free_idx <= i; free_idx++) {
                    bm_image_destroy(converted_images[free_idx]);
                }

                return BM_ERR_FAILURE;
            }
        }
    }

    return BM_SUCCESS;
}

bm_status_t do_image_align(bm_handle_t handle,
                           int         image_num,
                           bm_image *  src_images,
                           bm_image *  converted_images,
                           int *       image_align_flag) {
    for (int i = 0; i < image_num; i++) {
        if (1 == image_align_flag[i]) {
            bmcv_width_align(handle, src_images[i], converted_images[i]);
        }
    }

    return BM_SUCCESS;
}

bm_status_t image_restore_align(bm_handle_t handle,
                                int         image_num,
                                bm_image *  aligned_images,
                                bm_image *  restored_images,
                                int *       image_align_flag) {
    if (NULL == restored_images) {
        for (int i = 0; i < image_num; i++) {
            bm_image_destroy(aligned_images[i]);
        }

        return BM_SUCCESS;
    }
    for (int i = 0; i < image_num; i++) {

        if ((!bm_image_is_attached(aligned_images[i])) ||
            (!bm_image_is_attached(restored_images[i]))) {
            BMCV_ERR_LOG("image should be attached memory firstly\n");

            return BM_ERR_FAILURE;
        }
        if (1 == image_align_flag[i]) {
            bmcv_width_align(handle, aligned_images[i], restored_images[i]);
            image_align_flag[i] = 0;
        }
        bm_image_destroy(aligned_images[i]);
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_image_resize_(bm_handle_t       handle,
                              int               input_num,
                              bmcv_resize_image resize_attr[MAX_INPUT_NUM],
                              bm_image *        input,
                              bm_image *        output) {
    bm_status_t ret = bmcv_resize_check(handle, input_num, resize_attr, input, output);
    if (BM_SUCCESS != ret) {
        BMCV_ERR_LOG("bmcv_resize_check error\r\n");
        return ret;
    }
    int output_num = 0;
    for (int input_idx = 0; input_idx < input_num; input_idx++) {
        output_num += resize_attr[input_idx].roi_num;
    }
    // this limit maybe too strict
    // for (int i = 0; i < input_num; i++) {
    //     if (0 != memcmp(handle,
    //                     bm_image_get_handle(&input[i]),
    //                     sizeof(struct bm_context))) {
    //         BMCV_ERR_LOG("input image handle error\n");

    //         return BM_ERR_FAILURE;
    //     }
    // }
    // for (int i = 0; i < output_num; i++) {
    //     if (0 != memcmp(handle,
    //                     bm_image_get_handle(&output[i]),
    //                     sizeof(struct bm_context))) {
    //         BMCV_ERR_LOG("output image handle error\n");

    //         return BM_ERR_FAILURE;
    //     }
    // }
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
                BMCV_ERR_LOG("bm_image_alloc_dev_mem error\n");
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
#if TPU_ONLY
    bool use_vpp = false;
#else
    bool use_vpp = if_use_vpp(input, output, input_num, output_num, resize_attr);
#endif

#if 0   // vpp cannot deal with odd
    if (input[0].data_type == DATA_TYPE_EXT_1N_BYTE) {
        int out_idx = 0;
        for (int i = 0; i < input_num; i++) {
            bmcv_rect_t* rect = new bmcv_rect_t [resize_attr[i].roi_num];
            for (int j = 0; j < resize_attr[i].roi_num; j++) {
                rect[j].start_x = resize_attr[i].resize_img_attr->start_x;
                rect[j].start_y = resize_attr[i].resize_img_attr->start_y;
                rect[j].crop_w = resize_attr[i].resize_img_attr->in_width;
                rect[j].crop_h = resize_attr[i].resize_img_attr->in_height;
            }
            bmcv_resize_algorithm algorithm = (bmcv_resize_algorithm)resize_attr[i].interpolation;
            bmcv_image_vpp_convert(handle, resize_attr[i].roi_num, input[i], output + out_idx, rect, algorithm);
            out_idx += resize_attr[i].roi_num;
            delete [] rect;
        }
        return BM_SUCCESS;
    }
#endif
    bm_image tmp_input[32], tmp_output[32];
    int      in_align_flag[32] = {0}, out_align_flag[32] = {0};
    if (BM_SUCCESS !=
        try_image_align(handle, input_num, input, tmp_input, in_align_flag)) {
        BMCV_ERR_LOG("try_image_align error\n");
        for (int free_idx = 0; free_idx < output_num; free_idx++) {
            if (output_mem_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }

        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS !=
        try_image_align(
            handle, output_num, output, tmp_output, out_align_flag)) {
        BMCV_ERR_LOG("try_image_align error\n");
        for (int free_idx = 0; free_idx < output_num; free_idx++) {
            if (output_mem_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }

        return BM_ERR_FAILURE;
    }

    if(!if_use_vpp(tmp_input, tmp_output, input_num, output_num, resize_attr)&&(BMCV_INTER_NEAREST != resize_attr[0].interpolation)){
        BMCV_ERR_LOG("[RESIZE]: vpp and tpu not support\r\n");
        return BM_NOT_SUPPORTED;
    }

    // if input is packed format, convert it to planar
    bm_image* input_planar;
    bool in_is_packed = ((input[0].image_format == FORMAT_BGR_PACKED) ||
                         (input[0].image_format == FORMAT_RGB_PACKED));
    if (in_is_packed && !use_vpp) {
        input_planar = new bm_image[input_num];
        bm_image_format_ext fmt = (input[0].image_format == FORMAT_BGR_PACKED) ?
                                   FORMAT_BGR_PLANAR : FORMAT_RGB_PLANAR;
        for (int i = 0; i < input_num; i++) {
            bm_image_create(handle, input[0].height, input[0].width, fmt,
                            input[0].data_type, input_planar + i);
            //bm_image_alloc_dev_mem(input_planar[i], BMCV_HEAP1_ID);
        }
        bmcv_image_storage_convert(handle, input_num, input, input_planar);
    } else {
        input_planar = input;
    }
    // if output is packed format, need get planar using tpu
    bm_image* output_planar;
    bool out_is_packed = ((output[0].image_format == FORMAT_BGR_PACKED) ||
                         (output[0].image_format == FORMAT_RGB_PACKED));
    if (out_is_packed && !use_vpp) {
        output_planar = new bm_image[output_num];
        bm_image_format_ext fmt = (output[0].image_format == FORMAT_BGR_PACKED) ?
                                   FORMAT_BGR_PLANAR : FORMAT_RGB_PLANAR;
        for (int i = 0; i < output_num; i++) {
            bm_image_create(handle, output[i].height, output[i].width, fmt,
                            output[i].data_type, output_planar + i);
            bm_image_alloc_dev_mem(output_planar[i]);
        }
    } else {
        output_planar = output;
    }
#if TPU_ONLY
    int             in_concat_status  = 0;
    int             out_concat_status = 0;
    bm_image_tensor in_tensor[MAX_INPUT_NUM];
    bm_image_tensor out_tensor[32];
    int if_4N_to_1N = ((input_planar[0].data_type == DATA_TYPE_EXT_4N_BYTE) &&
                       (output[0].data_type == DATA_TYPE_EXT_1N_BYTE))
                          ? (1)
                          : (0);
    if (BM_SUCCESS ==
        concat_images_to_tensor(handle, input_num, input_planar, in_tensor)) {
        in_concat_status = 1;
    }
    if (BM_SUCCESS ==
        concat_images_to_tensor(handle, output_num, output_planar, out_tensor)) {
        out_concat_status = 1;
    }
    if ((in_concat_status == 1) && (out_concat_status == 1)) {
        bmcv_resize_internal(handle,
                             (if_4N_to_1N == 1) ? (input_num * 4) : (input_num),
                             resize_attr,
                             in_tensor[0],
                             out_tensor[0]);
        bm_image_tensor_destroy(in_tensor[0]);
        bm_image_tensor_destroy(out_tensor[0]);
    } else {
        ASSERT_INFO(if_4N_to_1N == 0,
                    "mem must be continuous when in 4n to 1n mode\r\n");
        int tmp_idx = 0;
        for (int i = 0; i < input_num; i++) {
            concat_images_to_tensor(handle, 1, &input_planar[i], &in_tensor[i]);
            if (BM_SUCCESS != concat_images_to_tensor(handle,
                                                      resize_attr[i].roi_num,
                                                      &output[tmp_idx],
                                                      &out_tensor[i])) {
                ASSERT_INFO(0, "output images not continuous\r\n");
            }
            bmcv_resize_internal(
                handle, 1, &resize_attr[i], in_tensor[i], out_tensor[i]);
            tmp_idx += resize_attr[i].roi_num;
            bm_image_tensor_destroy(in_tensor[i]);
            bm_image_tensor_destroy(out_tensor[i]);
        }
    }
    if (out_is_packed) {
        bmcv_image_storage_convert(handle, output_num, output_planar, output);
        for (int i = 0; i < output_num; i++) {
            bm_image_destroy(output_planar[i]);
        }
        delete [] output_planar;
    }
    if (BM_SUCCESS != image_restore_align(
                          handle, input_num, tmp_input, NULL, in_align_flag)) {
        BMCV_ERR_LOG("image_restore_align error\n");
        for (int free_idx = 0; free_idx < output_num; free_idx++) {
            if (output_mem_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }
        if (in_is_packed) {
            for (int i = 0; i < input_num; i++) {
                bm_image_destroy(input_planar[i]);
            }
            delete[] input_planar;
        }
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS !=
        image_restore_align(
            handle, output_num, tmp_output, NULL, out_align_flag)) {
        BMCV_ERR_LOG("image_restore_align error\n");
        for (int free_idx = 0; free_idx < output_num; free_idx++) {
            if (output_mem_alloc_flag[free_idx]) {
                bm_device_mem_t dmem;
                bm_image_get_device_mem(output[free_idx], &dmem);
                bm_free_device(handle, dmem);
            }
        }
        if (in_is_packed) {
            for (int i = 0; i < input_num; i++) {
                bm_image_destroy(input_planar[i]);
            }
            delete[] input_planar;
        }

        return BM_ERR_FAILURE;
    }
#else
    int resize_alg = VPP_SCALE_BILINEAR;
    switch(resize_attr[0].interpolation) {
        case BMCV_INTER_LINEAR:
            resize_alg = VPP_SCALE_BILINEAR;
            break;
        case BMCV_INTER_NEAREST:
            resize_alg = VPP_SCALE_NEAREST;
            break;
        case BMCV_INTER_BICUBIC:
            resize_alg = VPP_SCALE_BICUBIC;
            break;
    }

    if (if_use_vpp(tmp_input, tmp_output, input_num, output_num, resize_attr)) {
        int      output_idx = 0;
        vpp_mat  src[16];
        vpp_mat  dst[16];
        vpp_rect rect[16];

        if (out_is_packed & !use_vpp) {
            for (int i = 0; i < output_num; i++) {
                bm_image_destroy(output_planar[i]);
            }
            delete[] output_planar;
        }
        output_idx = 0;
        if (BM_SUCCESS !=
            do_image_align(
                handle, input_num, input_planar, tmp_input, in_align_flag)) {
            BMCV_ERR_LOG("do_image_align error\n");
            for (int free_idx = 0; free_idx < output_num; free_idx++) {
                if (output_mem_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
            if (in_is_packed) {
                for (int i = 0; i < input_num; i++) {
                    bm_image_destroy(input_planar[i]);
                }
                delete[] input_planar;
            }

            return BM_ERR_FAILURE;
        }
        for (int input_idx = 0; input_idx < input_num; input_idx++) {
            for (int i = 0; i < resize_attr[input_idx].roi_num; i++) {
                /*vpp_fill_mat_with_device_mem(
                    &src[output_idx], tmp_input[input_idx], handle);
                vpp_fill_mat_with_device_mem(
                    &dst[output_idx], tmp_output[output_idx], handle);*/
                bm_image_to_vpp_mat(handle, tmp_input[input_idx], &src[output_idx]);
                bm_image_to_vpp_mat(handle, tmp_output[output_idx], &dst[output_idx]);
                rect[output_idx].x =
                    resize_attr[input_idx].resize_img_attr[i].start_x;
                rect[output_idx].y =
                    resize_attr[input_idx].resize_img_attr[i].start_y;
                rect[output_idx].width =
                    resize_attr[input_idx].resize_img_attr[i].in_width;
                rect[output_idx].height =
                    resize_attr[input_idx].resize_img_attr[i].in_height;
                output_idx++;
            }
        }
        if (BM_SUCCESS !=
            (bm_status_t)vpp_misc(
                src, rect, dst, output_idx, CSC_MAX, resize_alg)) {
            BMCV_ERR_LOG("vpp_misc error\n");
            for (int free_idx = 0; free_idx < output_num; free_idx++) {
                if (output_mem_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
            if (in_is_packed && !use_vpp) {
                for (int i = 0; i < input_num; i++) {
                    bm_image_destroy(input_planar[i]);
                }
                delete[] input_planar;
            }

            return BM_ERR_FAILURE;
        }
        if (BM_SUCCESS !=
            image_restore_align(
                handle, input_num, tmp_input, NULL, in_align_flag)) {
            BMCV_ERR_LOG("image_restore_align error\n");
            for (int free_idx = 0; free_idx < output_num; free_idx++) {
                if (output_mem_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
            if (in_is_packed && !use_vpp) {
                for (int i = 0; i < input_num; i++) {
                    bm_image_destroy(input_planar[i]);
                }
                delete[] input_planar;
            }

            return BM_ERR_FAILURE;
        }
        if (BM_SUCCESS !=
            image_restore_align(
                handle, output_num, tmp_output, output, out_align_flag)) {
            BMCV_ERR_LOG("image_restore_align error\n");
            for (int free_idx = 0; free_idx < output_num; free_idx++) {
                if (output_mem_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
            if (in_is_packed && !use_vpp) {
                for (int i = 0; i < input_num; i++) {
                    bm_image_destroy(input_planar[i]);
                }
                delete[] input_planar;
            }

            return BM_ERR_FAILURE;
        }
    } else {
        int             in_concat_status  = 0;
        int             out_concat_status = 0;
        bm_image_tensor in_tensor[MAX_INPUT_NUM];
        bm_image_tensor out_tensor[32];
        int if_4N_to_1N = ((input_planar[0].data_type == DATA_TYPE_EXT_4N_BYTE) &&
                           (output[0].data_type == DATA_TYPE_EXT_1N_BYTE))
                              ? (1)
                              : (0);
        if (BM_SUCCESS ==
            concat_images_to_tensor(handle, input_num, input_planar, in_tensor)) {
            in_concat_status = 1;
        }
        if (BM_SUCCESS ==
            concat_images_to_tensor(handle, output_num, output_planar, out_tensor)) {
            out_concat_status = 1;
        }
        if ((in_concat_status == 1) && (out_concat_status == 1)) {
            bmcv_resize_internal(
                handle,
                (if_4N_to_1N == 1) ? (input_num * 4) : (input_num),
                resize_attr,
                in_tensor[0],
                out_tensor[0]);
            bm_image_tensor_destroy(in_tensor[0]);
            bm_image_tensor_destroy(out_tensor[0]);
        } else {
            if (if_4N_to_1N != 0) {
                BMCV_ERR_LOG("[RESIZE]: mem must be continuous when in 4n to 1n mode\r\n");
                return BM_ERR_FAILURE;
            }
            int tmp_idx = 0;
            for (int i = 0; i < input_num; i++) {
                concat_images_to_tensor(handle, 1, &input_planar[i], &in_tensor[i]);
                if (BM_SUCCESS !=
                    concat_images_to_tensor(handle,
                                            resize_attr[i].roi_num,
                                            &output[tmp_idx],
                                            &out_tensor[i])) {
                    BMCV_ERR_LOG("[RESIZE]: output images not continuous\r\n");
                    return BM_ERR_FAILURE;
                }
                bmcv_resize_internal(
                    handle, 1, &resize_attr[i], in_tensor[i], out_tensor[i]);
                tmp_idx += resize_attr[i].roi_num;
                bm_image_tensor_destroy(in_tensor[i]);
                bm_image_tensor_destroy(out_tensor[i]);
            }
        }
        if (out_is_packed) {
            bmcv_image_storage_convert(handle, output_num, output_planar, output);
            for (int i = 0; i < output_num; i++) {
                bm_image_destroy(output_planar[i]);
            }
            delete [] output_planar;
        }
        if (BM_SUCCESS !=
            image_restore_align(
                handle, input_num, tmp_input, NULL, in_align_flag)) {
            BMCV_ERR_LOG("image_restore_align error\n");
            for (int free_idx = 0; free_idx < output_num; free_idx++) {
                if (output_mem_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
            if (in_is_packed) {
                for (int i = 0; i < input_num; i++) {
                    bm_image_destroy(input_planar[i]);
                }
                delete[] input_planar;
            }

            return BM_ERR_FAILURE;
        }
        if (BM_SUCCESS !=
            image_restore_align(
                handle, output_num, tmp_output, NULL, out_align_flag)) {
            BMCV_ERR_LOG("image_restore_align error\n");
            for (int free_idx = 0; free_idx < output_num; free_idx++) {
                if (output_mem_alloc_flag[free_idx]) {
                    bm_device_mem_t dmem;
                    bm_image_get_device_mem(output[free_idx], &dmem);
                    bm_free_device(handle, dmem);
                }
            }
            if (in_is_packed) {
                for (int i = 0; i < input_num; i++) {
                    bm_image_destroy(input_planar[i]);
                }
                delete[] input_planar;
            }

            return BM_ERR_FAILURE;
        }
    }

#endif
    if (in_is_packed && !use_vpp) {
        for (int i = 0; i < input_num; i++) {
            bm_image_destroy(input_planar[i]);
        }
        delete[] input_planar;
    }

    return BM_SUCCESS;

}

bm_status_t bmcv_image_resize(
    bm_handle_t       handle,
    int               input_num,
    bmcv_resize_image resize_attr[MAX_INPUT_NUM],
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
        ret = bmcv_image_resize_(handle, input_num, resize_attr, input, output);
        break;

      case BM1684X:
        ret = bm1684x_vpp_resize(handle, input_num, resize_attr, input, output);
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}


