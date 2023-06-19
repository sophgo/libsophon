#ifndef USING_CMODEL
#include "bmcv_api_ext.h"
#include "bmlib_interface.h"
#include "vpplib.h"
#ifdef __linux__
#include "vppion.h"
#endif
#include "bmcv_internal.h"
#include <vector>
#include <memory>
#include <functional>
#include "bmcv_common_bm1684.h"

extern void format_to_str(bm_image_format_ext format, char* res);
static int bm_image_to_vpp_mat(bm_handle_t handle, bm_image image,
    vpp_mat_s* mat, bmcv_rect_t* dst_crop_rect = NULL)
{
    memset(mat, 0, sizeof(vpp_mat));

    #ifdef __linux__
    bm_get_handle_fd(handle, VPP_FD, &mat->vpp_fd.dev_fd);
    memset(mat->vpp_fd.name, 0x0, sizeof(mat->vpp_fd.name));
    memcpy(mat->vpp_fd.name, "bm-vpp", sizeof("bm-vpp"));

    #endif
    mat->vpp_fd.handle = handle;

    mat->cols = image.width;
    mat->rows = image.height;

    /*for border*/
    if (dst_crop_rect != NULL) {
        mat->axisX = dst_crop_rect->start_x;
        mat->axisY = dst_crop_rect->start_y;
        mat->cols = dst_crop_rect->crop_w;
        mat->rows = dst_crop_rect->crop_h;
    }

    bm_device_mem_t mem[4];
    bm_image_get_device_mem(image, mem);
    uint64_t global_address[4] = {0};
    int plane_num = bm_image_get_plane_num(image);
    for(int i = 0; i < plane_num; i++)
    {
        global_address[i] = bm_mem_get_device_addr(mem[i]);
        if (global_address[i] == 0){
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "no device memory in bmimage %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            return -1;
        }
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
            mat->uv_stride = memory_layout[1].pitch_stride;
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
        case FORMAT_ABGR_PACKED:
            mat->format = FMT_ABGR;
            mat->num_comp = 1;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->pa[0] = global_address[0];
            break;
        case FORMAT_ARGB_PACKED:
            mat->format = FMT_ARGB;
            mat->num_comp = 1;
            mat->stride = memory_layout[0].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->pa[0] = global_address[0];
            break;
        case FORMAT_NV12:
            mat->format = FMT_NV12;
            mat->num_comp = 2;
            mat->stride     = memory_layout[0].pitch_stride;
            mat->uv_stride = memory_layout[1].pitch_stride;
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
        case FORMAT_YUV422P:
            mat->format = FMT_YUV422P;
            mat->num_comp = 3;
            mat->stride = memory_layout[0].pitch_stride;
            mat->uv_stride = memory_layout[1].pitch_stride;
            mat->ion_len[0] = memory_layout[0].size;
            mat->ion_len[1] = memory_layout[1].size;
            mat->ion_len[2] = memory_layout[2].size;
            mat->pa[0] = global_address[0];
            mat->pa[1] = global_address[1];
            mat->pa[2] = global_address[2];
            break;
        default:
            ret = -1;
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp not support this format %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            break;
    }
    return ret;
}


static bool inline check_stride(
           int* input_stride,
           int  plane_num,
           unsigned short *stride_align)
{
    for (int i = 0; i < plane_num; i++){
        if (input_stride[i] % stride_align[i] != 0){
            input_stride[i] = ALIGN(input_stride[i], stride_align[i]);
            return false;
        }
    }

    return true;
}

static bool check_address_align(bm_image image, int address_align)
{
    bm_device_mem_t src_mem[4];
    int             src_plane_num = bm_image_get_plane_num(image);
    bm_image_get_device_mem(image, src_mem);
    for (int k = 0; k < src_plane_num; k++) {
        if(bm_mem_get_device_addr(src_mem[k]) % address_align != 0)
        {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp src addr not aligned, src_addr:%lx, %s: %s: %d\n",
                      bm_mem_get_device_addr(src_mem[k]),
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return false;
        }
    }

    if(image.image_format == FORMAT_RGB_PLANAR || image.image_format == FORMAT_BGR_PLANAR)
    {
        int stride[4] = {0};
        bm_image_get_stride(image, stride);
        if(stride[0] * image.height % address_align != 0)
        {
            return false;
        }
    }
    return true;
}

static bm_status_t bm_image_vpp_check_format_single(
    bm_image      input,
    int           crop_num,
    bmcv_rect_t*  crop_rect,
    std::vector<bm_image*>&   output_ptr
)
{
    int input_stride[4] = {0};
    vpp_limitation limitation;

    if(bm_image_get_stride(input, input_stride) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "not correctly create bm_image %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    if(bm_vpp_query_limitation(input.image_format,
          output_ptr[0][0].image_format, limitation) != BM_SUCCESS)
    {
        char src_format[30];
        char dst_format[30];
        format_to_str(input.image_format, src_format);
        format_to_str(output_ptr[0][0].image_format, dst_format);
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                  "vpp not support conversion from %s to %s %s: %s: %d\n", src_format, dst_format,
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if(input.width > limitation.width_max || input.width < limitation.width_min)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp src width size not match %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if(input.height > limitation.height_max || input.height < limitation.height_min)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp src height size not match %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if(!check_stride(input_stride, input.image_private->plane_num, limitation.src_stride_align))
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp src stride not match %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);

        return BM_NOT_SUPPORTED;
    }

    if(check_address_align(input, limitation.src_address_align) != true)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp src image address not aligned %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);

        return BM_NOT_SUPPORTED;
    }

    for (int i = 0; i < crop_num; i++) {
        if (output_ptr[i]->width > limitation.width_max ||
            output_ptr[i]->width < limitation.width_min) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp dst width size not match %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (output_ptr[i]->height > limitation.height_max ||
            output_ptr[i]->height < limitation.height_min) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp dst height size not match %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (crop_rect[i].crop_w > limitation.width_max ||
            crop_rect[i].crop_w < limitation.width_min) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp crop_rect width size not match %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (crop_rect[i].crop_h > limitation.height_max ||
            crop_rect[i].crop_h < limitation.height_min) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp crop_rect height size not match %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (crop_rect[i].start_x < 0 || crop_rect[i].crop_w <= 0 ||
            (crop_rect[i].start_x + crop_rect[i].crop_w > input.width)) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp crop_rect width size not match %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (crop_rect[i].start_y < 0 || crop_rect[i].crop_h <= 0 ||
            (crop_rect[i].start_y + crop_rect[i].crop_h > input.height)) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp crop_rect height size not match %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (!limitation.support_crop) {
            if (crop_rect[i].start_x != 0 || crop_rect[i].start_y != 0 ||
                crop_rect[i].crop_w != input.width ||
                crop_rect[i].crop_h != input.height) {
                bmlib_log(BMCV_LOG_TAG,
                          BMLIB_LOG_WARNING,
                          "vpp not support crop on this format conversion "
                          "situation %s: %s: %d\n",
                          filename(__FILE__),
                          __func__,
                          __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }
        if (!limitation.support_scale) {
            if (crop_rect[i].crop_w != output_ptr[i]->width ||
                crop_rect[i].crop_h != output_ptr[i]->height) {
                bmlib_log(BMCV_LOG_TAG,
                          BMLIB_LOG_WARNING,
                          "vpp not support scale on this format conversion "
                          "situation %s: %s: %d\n",
                          filename(__FILE__),
                          __func__,
                          __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }

        if ((crop_rect[i].crop_w * limitation.zoom_limitation_max <
             output_ptr[i]->width) ||
            (crop_rect[i].crop_h * limitation.zoom_limitation_max <
             output_ptr[i]->height)) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp zoom up too much %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if ((output_ptr[i]->width * limitation.zoom_limitation_min <
             crop_rect[i].crop_w) ||
            (output_ptr[i]->height * limitation.zoom_limitation_min <
             crop_rect[i].crop_h)) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_WARNING,
                      "vpp zoom down too much %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (crop_rect[i].start_x % limitation.src_offset_x_align != 0 ||
            crop_rect[i].start_x % limitation.src_offset_x_align != 0 ||
            crop_rect[i].crop_w % limitation.src_width_align != 0 ||
            crop_rect[i].crop_h % limitation.src_height_align != 0 ||
            output_ptr[i]->width % limitation.dst_width_align != 0 ||
            output_ptr[i]->height % limitation.dst_height_align != 0) {
            bmlib_log(
                BMCV_LOG_TAG,
                BMLIB_LOG_WARNING,
                "vpp offset / width / height align not match %s: %s: %d\n",
                filename(__FILE__),
                __func__,
                __LINE__);
            return BM_NOT_SUPPORTED;
        }
    }
    return BM_SUCCESS;
}

bm_status_t pre_process_for_vpp_input(
    bm_handle_t handle,
    bm_image& input,
    bool& pre_processed,
    std::shared_ptr<image_warpper>& output,
    vpp_limitation limitation,
    int& expand_height,
    int& expand_width,
    bool mem_limit = true
)
{
    pre_processed    = false;
    int input_width  = input.width;
    int input_height = input.height;
    int stride[4];
    if(bm_image_get_stride(input, stride) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "not correctly create bm_image %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    if(!bm_image_is_attached(input))
    {
        return BM_ERR_PARAM;
    }

    if(input.image_format == FORMAT_COMPRESSED && \
         (input.width % limitation.src_width_align !=0 || input.height % limitation.src_height_align !=0))
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "Compressed format could not do any preprocess %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }

    if(input.width % limitation.src_width_align !=0 || input.height % limitation.src_height_align !=0)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
            "YUV format width / height is not aligned, start width / height align preprocess for input %s: %s: %d\n",
            filename(__FILE__), __func__, __LINE__);
        pre_processed = true;
        input_width   = ALIGN(input_width, limitation.src_width_align);
        input_height   = ALIGN(input_height, limitation.src_height_align);
        expand_height = input_height - input.height;
        expand_width = input_width - input.width;

        if (input_width <= stride[0]){   // after expanding, if input_width is less than stride[0], do not need to copy
            input.width = input_width;
            input.height = input_height;
            pre_processed = false;
        }
    }

    int plane_num = bm_image_get_plane_num(input);
    bm_device_mem_t mem[4];
    if(plane_num == 0 || bm_image_get_device_mem(input, mem) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "not correctly create bm_image %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < plane_num; i++) {
        if(bm_mem_get_device_addr(mem[i]) < 0x300000000 && mem_limit)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                "src device memory not on DDR1, preprocess and start copying to DDR1, src_addr:%lx, %s: %s: %d\n",
                bm_mem_get_device_addr(mem[i]), filename(__FILE__), __func__, __LINE__);
            pre_processed = true;
        }

        if(bm_mem_get_device_addr(mem[i]) % limitation.src_address_align != 0)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                "src device memory start address not aligned, start address aligned preprocess, src_addr:%lx, %s: %s: %d\n",
                bm_mem_get_device_addr(mem[i]), filename(__FILE__), __func__, __LINE__);
            pre_processed = true;
        }
    }

    if(!check_stride(stride, input.image_private->plane_num, limitation.src_stride_align))
    {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                "input stride not aligned, start input stride align preprocess %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            pre_processed = true;
    }

    // Start to preprocess
    if(pre_processed)
    {
        if(input.image_format == FORMAT_COMPRESSED)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "illegal compressed format %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_ERR_PARAM;
        }
        else
        {
            output = std::make_shared<image_warpper>(handle,
                                                    1,
                                                    input_height,
                                                    input_width,
                                                    input.image_format,
                                                    input.data_type);
            {
                //std::lock_guard<std::mutex> lock(input.image_private->memory_lock);
               for (int k = 0; k < output->inner[0].image_private->plane_num; k++)
                {
                    output->inner[0].image_private->memory_layout[k] =
                        layout::align_width(
                            output->inner[0].image_private->memory_layout[k],
                            limitation.src_stride_align[k]);
                }
            }

            if(bm_image_alloc_dev_mem_heap_mask(output->inner[0], 6) != BM_SUCCESS)
            {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "preprocess alloc fail %s: %s: %d\n",
                        filename(__FILE__), __func__, __LINE__);
                return BM_ERR_FAILURE;
            }


            bm_device_mem_t             dst[4];
            bm_device_mem_t             src[4];
            if(bm_image_get_device_mem(output->inner[0], dst) != BM_SUCCESS \
                                   || bm_image_get_device_mem(input, src) != BM_SUCCESS)
            {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "not correctly create bm_image %s: %s: %d\n",
                        filename(__FILE__), __func__, __LINE__);
                return BM_ERR_FAILURE;
            }

            for (int k = 0; k < input.image_private->plane_num; k++)
            {
                if(layout::update_memory_layout(
                    handle,
                    src[k],
                    input.image_private->memory_layout[k],
                    dst[k],
                    output->inner[0].image_private->memory_layout[k]) != BM_SUCCESS)
                {
                    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "FATAL ERROR!!! TPU CRASH %s: %s: %d\n",
                            filename(__FILE__), __func__, __LINE__);
                    return BM_ERR_FAILURE;
                }
            }
        }
    }
    return BM_SUCCESS;
}


bm_status_t check_vpp_input_param(
    bm_handle_t handle,
    bm_image& input,
    vpp_limitation limitation,
    int output_num,
    bmcv_rect_t* dst_crop_rect,
    bmcv_rect_t* src_crop_rect,
    bool mem_limit = true
)
{
    int plane_num;
    int out_idx;
    bm_device_mem_t device_mem[4];
    int stride[4];
    u64 device_addr = 0;
    int src_crop_w, src_crop_h;
    int dst_crop_w, dst_crop_h;

    (void)handle;
    if(!bm_image_is_attached(input))
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "not correctly create input bm_image , not attache mem %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }

    if(input.data_type != DATA_TYPE_EXT_1N_BYTE)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "vpp only support DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if(bm_image_get_stride(input, stride) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "not correctly create input bm_image , get stride err %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    if(input.width > limitation.width_max || input.width < limitation.width_min)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "vpp src width size not match %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if(input.height > limitation.height_max || input.height < limitation.height_min)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "vpp src height size not match %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if(input.width % limitation.src_width_align !=0 || input.height % limitation.src_height_align !=0)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "input bm_image width / height is not aligned %s: %s: %d\n",
            filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    plane_num = bm_image_get_plane_num(input);

    if(plane_num == 0 || bm_image_get_device_mem(input, device_mem) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "not correctly create input bm_image, get plane num or device mem err %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    for (int i = 0; i < plane_num; i++) {
        device_addr = bm_mem_get_device_addr(device_mem[i]);
        if(device_addr < 0x300000000 && mem_limit)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "src device memory not on DDR1, src_addr:%lx, %s: %s: %d\n",
                device_addr, filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        if(device_addr % limitation.src_address_align != 0)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "src device memory start address not aligned, src_addr:%lx, %s: %s: %d\n",
                device_addr, filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        if(stride[i] % limitation.src_stride_align[i] != 0)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "src stride %d not aligned %s: %s: %d\n", i,
                filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
    }

    std::vector<bmcv_rect_t> rects;
    if (!src_crop_rect)
    {
        rects.reserve(output_num);
        for (out_idx = 0; out_idx < output_num; out_idx++)
        {
            rects.push_back({0, 0, input.width, input.height});
        }
        src_crop_rect = &rects[0];
    }

    for (out_idx = 0; out_idx < output_num; out_idx++)
    {
        src_crop_rect[out_idx].start_x =
          src_crop_rect[out_idx].start_x / limitation.src_offset_x_align * limitation.src_offset_x_align;
        src_crop_rect[out_idx].start_y =
          src_crop_rect[out_idx].start_y / limitation.src_offset_y_align * limitation.src_offset_y_align;
        src_crop_rect[out_idx].crop_w = ALIGN(src_crop_rect[out_idx].crop_w, limitation.src_width_align);
        src_crop_rect[out_idx].crop_h = ALIGN(src_crop_rect[out_idx].crop_h, limitation.src_height_align);

        if ((src_crop_rect[out_idx].start_x < 0) ||
            (src_crop_rect[out_idx].start_y < 0) ||
            (src_crop_rect[out_idx].crop_w < limitation.width_min) ||
            (src_crop_rect[out_idx].crop_h < limitation.height_min) ||
            (src_crop_rect[out_idx].start_x + src_crop_rect[out_idx].crop_w > input.width) ||
            (src_crop_rect[out_idx].start_y + src_crop_rect[out_idx].crop_h > input.height)) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "src crop is out of range  %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (src_crop_rect[out_idx].start_x % limitation.src_offset_x_align != 0 ||
            src_crop_rect[out_idx].start_x % limitation.src_offset_x_align != 0 ||
            src_crop_rect[out_idx].crop_w % limitation.src_width_align != 0 ||
            src_crop_rect[out_idx].crop_h % limitation.src_height_align != 0) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "src crop param align not match %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }

        src_crop_w = src_crop_rect[out_idx].crop_w;
        src_crop_h = src_crop_rect[out_idx].crop_h;
        dst_crop_w = dst_crop_rect[out_idx].crop_w;
        dst_crop_h = dst_crop_rect[out_idx].crop_h;

        if (dst_crop_w > src_crop_w * VPP_MAX_SCALING_RATIO ||
          dst_crop_h > src_crop_h * VPP_MAX_SCALING_RATIO ||
            src_crop_w > dst_crop_w * VPP_MAX_SCALING_RATIO ||
            src_crop_h > dst_crop_h * VPP_MAX_SCALING_RATIO) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                  "vpp not support: scaling ratio greater than 32: %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if (!limitation.support_crop) {
            if (src_crop_rect[out_idx].start_x != 0 ||
                src_crop_rect[out_idx].start_y != 0 ||
                src_crop_rect[out_idx].crop_w != input.width ||
                src_crop_rect[out_idx].crop_h != input.height) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                          "vpp not support crop on this format conversion "
                          "situation %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }
    }

    return BM_SUCCESS;
}

bm_status_t pre_process_for_vpp_output_(
    bm_handle_t handle,
    bm_image output,
    bool& pre_processed,
    std::shared_ptr<image_warpper>& dst,
    vpp_limitation limitation
)
{
    pre_processed    = false;
    int output_width  = output.width;
    int output_height = output.height;

    if(output.width % limitation.dst_width_align !=0 || output.height % limitation.dst_height_align !=0)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
            "YUV format width / height is not aligned, start width / height align preprocess for output %s: %s: %d\n",
            filename(__FILE__), __func__, __LINE__);
        pre_processed = true;
        output_width   = ALIGN(output_width, limitation.dst_width_align);
        output_height   = ALIGN(output_height, limitation.dst_height_align);
    }

    int plane_num = bm_image_get_plane_num(output);
    bm_device_mem_t mem[4];
    if(plane_num == 0 || bm_image_get_device_mem(output, mem) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "not correctly create bm_image %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < plane_num; i++) {
        if(bm_mem_get_device_addr(mem[i]) % limitation.dst_address_align != 0)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                "dst device memory start address not aligned, start address aligned preprocess, dst_addr:%lx, %s: %s: %d\n",
                bm_mem_get_device_addr(mem[i]), filename(__FILE__), __func__, __LINE__);
            pre_processed = true;
        }
    }

    int stride[4] = {0};
    if(bm_image_get_stride(output, stride) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "not correctly create bm_image %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    if(!check_stride(stride, output.image_private->plane_num, limitation.dst_stride_align))
    {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                "output stride not aligned, start output stride align preprocess %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            pre_processed = true;
    }

    if(pre_processed)
    {
        dst = std::make_shared<image_warpper>(handle,
                                              1,
                                              output_height,
                                              output_width,
                                              output.image_format,
                                              output.data_type);

        {
            for (int k = 0; k < dst->inner[0].image_private->plane_num; k++)
            {
                dst->inner[0].image_private->memory_layout[k] =
                    layout::align_width(
                        dst->inner[0].image_private->memory_layout[k],
                        limitation.dst_stride_align[k]);
            }
        }

        if(bm_image_alloc_dev_mem(dst->inner[0], BMCV_HEAP_ANY) != BM_SUCCESS)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "preprocess alloc fail %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
    }
    return BM_SUCCESS;
}

bm_status_t check_vpp_output_param(
    bm_handle_t handle,
    bm_image& input,
    bmcv_rect_t* crop_rect,
    int output_num,
    bm_image* output_arr,
    bmcv_rect_t* dst_crop_rect,
    vpp_limitation limitation
)
{
    bm_device_mem_t device_mem[4];
    u64 device_addr;
    int stride[4] = {0};
    int plane_idx;
    int output_idx;
    int plane_num;
    bm_image output;
    bmcv_rect_t dst_crop;
    bm_image_format_ext output_image_format = output_arr[0].image_format;

    (void)handle;
    (void)input;

    for (output_idx = 0; output_idx < output_num; output_idx++)
    {
        output  = output_arr[output_idx];
        dst_crop = dst_crop_rect[output_idx];

        if(output.width > limitation.width_max ||
          output.width < limitation.width_min)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "dst width not in the right range %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        if(output.height > limitation.height_max ||
          output.height < limitation.height_min)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "dst height not in the right range %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        if(output.width % limitation.dst_width_align !=0 ||
          output.height % limitation.dst_height_align !=0)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "YUV format dst width / height is not aligned %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        if ((dst_crop.crop_w < limitation.width_min) ||
            (dst_crop.crop_h < limitation.height_min) ||
            (dst_crop.start_x + dst_crop.crop_w > output.width) ||
            (dst_crop.start_y + dst_crop.crop_h > output.height))
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "dst crop is out of range  %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        if(dst_crop.start_x % limitation.dst_offset_x_align != 0 ||
          dst_crop.start_y % limitation.dst_offset_y_align != 0 ||
          dst_crop.crop_w % limitation.dst_width_align !=0 ||
          dst_crop.crop_h % limitation.dst_height_align !=0)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "dst crop param not aligned %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        plane_num = bm_image_get_plane_num(output);

        if(plane_num == 0 || bm_image_get_device_mem(output, device_mem) != BM_SUCCESS)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
              "output get device mem err %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        if(output.data_type != DATA_TYPE_EXT_1N_BYTE)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
              "vpp only support DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if(output_image_format != output.image_format)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
              "expected consistant output image data type and format %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if(bm_image_get_stride(output, stride) != BM_SUCCESS)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
              "output get stride err %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        for (plane_idx = 0; plane_idx < plane_num; plane_idx++) {
            device_addr = bm_mem_get_device_addr(device_mem[plane_idx]);
            if(device_addr % limitation.dst_address_align != 0)
            {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                    "dst device memory start address not aligned, dst_addr:%lx, %s: %s: %d\n",
                    device_addr, filename(__FILE__), __func__, __LINE__);
                return BM_ERR_FAILURE;
            }

            if(stride[plane_idx] % limitation.dst_stride_align[plane_idx] != 0)
            {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                    "dst stride %d not aligned %s: %s: %d\n", plane_idx,
                    filename(__FILE__), __func__, __LINE__);
                return BM_ERR_FAILURE;
            }
        }

        if (!limitation.support_scale) {
            if (crop_rect[output_idx].crop_w != dst_crop.crop_w ||
                crop_rect[output_idx].crop_h != dst_crop.crop_h) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING,
                          "vpp not support scale on this format conversion "
                          "situation %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
        }
    }

    return BM_SUCCESS;
}

static bm_status_t bmcv_memset_device_bm1684(bm_handle_t handle,
                                      void* value,
                                      int mode,
                                      unsigned int height,
                                      unsigned int width,
                                      bm_device_mem_t mem) {
    bm_status_t ret = BM_SUCCESS;
    int tmp = 0;
    if (!value) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "input NULL pointer %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }
    if (mode <= 0 || mode > 4) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "input wrong memset mode = %d  %s: %s: %d\n",
                mode, filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }
    if (bm_mem_get_device_size(mem) != height * width) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "height %d width %d is mismatch with size %d  %s: %s: %d\n",
                height, width, bm_mem_get_device_size(mem), filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }
    memcpy(&tmp, value, mode);
    bm_api_memset_t api = {bm_mem_get_device_addr(mem),
                           height,
                           width,
                           mode,
                           tmp};
    ret = bm_send_api(handle, BM_API_ID_MEM_SET,
                      (u8 *)(&api), sizeof(api));
    ret = bm_sync_api(handle);
    return ret;
}

static bm_status_t bmcv_memset_device(bm_handle_t handle,
                                      void* value,
                                      int mode,
                                      unsigned int height,
                                      unsigned int width,
                                      bm_device_mem_t mem) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_memset_device_bm1684(handle, value, mode, height, width, mem);
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

bm_status_t bmcv_image_vpp_memset(
        bm_handle_t handle,
        bm_image* output,
        color_per_ch* color) {
    int stride[3];
    bm_image_get_stride(*output, stride);
    bm_device_mem_t device_mem[3];
    int mode = (color->ch0 == color->ch1 && color->ch1 == color->ch2) ? 1 : 3;
    if (output->image_format == FORMAT_BGR_PACKED) {
        unsigned int value = (color->ch0 << 16) | (color->ch1 << 8) | color->ch2;
        bm_image_get_device_mem(*output, device_mem);
        bmcv_memset_device(handle, &value, mode, output->height, stride[0], device_mem[0]);
    } else if (output->image_format == FORMAT_RGB_PACKED) {
        unsigned int value = (color->ch2 << 16) | (color->ch1 << 8) | color->ch0;
        bm_image_get_device_mem(*output, device_mem);
        bmcv_memset_device(handle, &value, mode, output->height, stride[0], device_mem[0]);
    } else if (output->image_format == FORMAT_BGR_PLANAR ||
               output->image_format == FORMAT_RGB_PLANAR) {
        bm_device_mem_t dmem;
        bm_image_get_device_mem(*output, &dmem);
        if (mode == 1) {
            unsigned int value = color->ch0;
            bmcv_memset_device(handle, &value, mode, output->height * 3, stride[0], dmem);
        } else {
            unsigned long long device_addr = bm_mem_get_device_addr(dmem);
            int size = output->height * stride[0];
            device_mem[0] = bm_mem_from_device(device_addr, size);
            device_mem[1] = bm_mem_from_device(device_addr + size, size);
            device_mem[2] = bm_mem_from_device(device_addr + size * 2, size);
            unsigned int value = (output->image_format == FORMAT_RGB_PLANAR) ? color->ch0 : color->ch2;
            bmcv_memset_device(handle, &value, 1, output->height, stride[0], device_mem[0]);
            value = color->ch1;
            bmcv_memset_device(handle, &value, 1, output->height, stride[0], device_mem[1]);
            value = (output->image_format == FORMAT_RGB_PLANAR) ? color->ch2 : color->ch0;
            bmcv_memset_device(handle, &value, 1, output->height, stride[0], device_mem[2]);
        }
    } else if (output->image_format == FORMAT_BGRP_SEPARATE ||
               output->image_format == FORMAT_RGBP_SEPARATE) {
        bm_image_get_device_mem(*output, device_mem);
        unsigned int value = (output->image_format == FORMAT_RGBP_SEPARATE) ? color->ch0 : color->ch2;
        bmcv_memset_device(handle, &value, 1, output->height, stride[0], device_mem[0]);
        value = color->ch1;
        bmcv_memset_device(handle, &value, 1, output->height, stride[0], device_mem[1]);
        value = (output->image_format == FORMAT_RGBP_SEPARATE) ? color->ch2 : color->ch0;
        bmcv_memset_device(handle, &value, 1, output->height, stride[0], device_mem[2]);
    } else if (output->image_format == FORMAT_GRAY) {
        unsigned int value = color->ch0;
        bm_image_get_device_mem(*output, device_mem);
        bmcv_memset_device(handle, &value, 1, output->height, stride[0], device_mem[0]);
    } else {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                "this format not support padding internal %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    return BM_SUCCESS;
}

bm_status_t pre_process_for_vpp_output(
    bm_handle_t handle,
    bm_image* output,
    int output_num,
    std::vector<bool>& pre_processed,
    std::vector<std::shared_ptr<image_warpper>>& dsts,
    std::vector<bm_image*>& output_ptr,
    vpp_limitation limitation
)
{
    for (int k = 0; k < output_num; k++)
    {
        bool preprocess = false;
        std::shared_ptr<image_warpper> dst;
        if(pre_process_for_vpp_output_(handle, output[k], preprocess, dst, limitation) != BM_SUCCESS)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "preprocess dst fail %s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
        pre_processed.push_back(preprocess);
        if(preprocess)
        {
            dsts.push_back(dst);
            output_ptr.push_back(dst->inner);
        } else {
            output_ptr.push_back(output + k);
        }
    }
    return BM_SUCCESS;
}

bm_status_t preprocess_for_rect(
    bm_image_format_ext input_format,
    bm_image_format_ext output_format,
    bmcv_rect_t *crop_rect,
    int rect_num,
    int input_height,
    int input_width,
    int src_expand_height,
    int src_expand_width,
    vpp_limitation limitation
)
{
    (void)input_format;
    (void)output_format;
    (void)input_height;
    (void)input_width;
    (void)src_expand_height;
    (void)src_expand_width;

    for (int k = 0; k < rect_num; k++)
    {
        crop_rect[k].start_x = crop_rect[k].start_x / limitation.src_offset_x_align * limitation.src_offset_x_align;
        crop_rect[k].start_y = crop_rect[k].start_y / limitation.src_offset_y_align * limitation.src_offset_y_align;
        crop_rect[k].crop_w = ALIGN(crop_rect[k].crop_w, limitation.src_width_align);
        crop_rect[k].crop_h = ALIGN(crop_rect[k].crop_h, limitation.src_height_align);
        // if(crop_rect[k].start_x + crop_rect[k].crop_w > input_width - src_expand_width && crop_rect[k].start_x + crop_rect[k].crop_w <= input_width)
        // {
        //     crop_rect[k].crop_w -=
        //         ALIGN(src_expand_width, limitation.src_width_align);
        // }
        // if(crop_rect[k].start_y + crop_rect[k].crop_h > input_height - src_expand_height && crop_rect[k].start_y + crop_rect[k].crop_h <= input_height)
        // {
        //     crop_rect[k].crop_h -=
        //         ALIGN(src_expand_height, limitation.src_height_align);
        // }

        crop_rect[k].crop_w = ALIGN(crop_rect[k].crop_w, limitation.dst_width_align);
        crop_rect[k].crop_h = ALIGN(crop_rect[k].crop_h, limitation.dst_height_align);
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_image_resize_cpu(bm_handle_t handle,
                                         bm_image& input,
                                         bm_image& output,
                                         bmcv_resize_algorithm algorithm,
                                         csc_matrix_t* matrix)
{
    if (input.image_format != output.image_format) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                  "input and output format is not same : %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (input.data_type != output.data_type) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                  "input and output data type is not same : %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    // seems the vpp_misc_cmodel do not have version for matrix parameter, ignore it here
    UNUSED(matrix);

    vpp_limitation limitation;
    bool pre_processed_input = false;
    std::shared_ptr<image_warpper> preprocessed_input;
    bm_image* input_ptr = &input;
    int expand_width = 0;
    int expand_height = 0;

    if (bm_vpp_query_limitation(input.image_format, output.image_format,
          limitation) != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                  "vpp not support format conversion: %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    // move it before pre_process_for_vpp_input, because it will change input.width/height
    bmcv_rect_t crop_rect = {0, 0, input.width, input.height};
    if (pre_process_for_vpp_input(
            handle, input, pre_processed_input, preprocessed_input,
            limitation, expand_height, expand_width, false) != BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp process for input failed %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    if(pre_processed_input)
    {
        input_ptr = preprocessed_input->inner;
    }

    // This function always return BM_SUCCESS
    preprocess_for_rect(input_ptr->image_format, output.image_format, &crop_rect, 1, \
                        input_ptr->height, input_ptr->width, expand_height, expand_width, limitation);

    std::vector<bool> output_process_mark;
    std::vector<std::shared_ptr<image_warpper>> dsts;
    std::vector<bm_image*> output_ptr;

    if(pre_process_for_vpp_output(
        handle, &output, 1, output_process_mark, dsts, output_ptr, limitation) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "output image preprocess fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    if(!bm_image_is_attached(output)) {
        if(bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY) != BM_SUCCESS) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "output dev alloc fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
    }
    int plane_num = bm_image_get_plane_num(input);
    int src_stride[4] = {0};
    int dst_stride[4] = {0};
    bm_image_get_stride(*input_ptr, src_stride);
    bm_image_get_stride(output_ptr[0][0], dst_stride);
    int src_img_size[4] = {0};
    int dst_img_size[4] = {0};
    bm_image_get_byte_size(*input_ptr, src_img_size);
    bm_image_get_byte_size(output_ptr[0][0], dst_img_size);
    void* src_data[4] = {NULL};
    void* dst_data[4] = {NULL};
    for (int i = 0; i < plane_num; i++) {
        src_data[i] = (void*)malloc(src_img_size[i] * sizeof(unsigned char));
        dst_data[i] = (void*)malloc(dst_img_size[i] * sizeof(unsigned char));
    }
    bm_image_copy_device_to_host(*input_ptr, src_data);

    struct vpp_mat_s src;
    struct vpp_mat_s dst;
    memset(&src, 0, sizeof(struct vpp_mat_s));
    memset(&dst, 0, sizeof(struct vpp_mat_s));
    src.cols = input_ptr->width;
    src.rows = input_ptr->height;
    src.num_comp = plane_num;
    src.stride = src_stride[0];
    src.is_pa = 0;
    dst.cols = output_ptr[0]->width;
    dst.rows = output_ptr[0]->height;
    dst.num_comp = plane_num;
    dst.stride = dst_stride[0];
    dst.is_pa = 0;

    for (int i = 0; i < plane_num; i++) {
        src.ion_len[i] = src_img_size[i];
        src.va[i] = src_data[i];
        dst.ion_len[i] = dst_img_size[i];
        dst.va[i] = dst_data[i];
    }

    switch(input.image_format)
    {
        case FORMAT_YUV420P:
            src.format = FMT_I420;
            dst.format = FMT_I420;
            break;
        case FORMAT_GRAY:
            src.format = FMT_Y;
            dst.format = FMT_Y;
            break;
        case FORMAT_YUV444P:
            src.format = FMT_YUV444P;
            dst.format = FMT_YUV444P;
            break;
        case FORMAT_RGB_PLANAR:
            src.format = FMT_RGBP;
            src.num_comp = 3;
            src.ion_len[0] = src_stride[0] * input_ptr->height;
            src.ion_len[1] = src_stride[0] * input_ptr->height;
            src.ion_len[2] = src_stride[0] * input_ptr->height;
            src.va[0] = (unsigned char*)src_data[0];
            src.va[1] = (unsigned char*)src_data[0] + src.ion_len[0];
            src.va[2] = (unsigned char*)src_data[0] + src.ion_len[1];
            dst.format = FMT_RGBP;
            dst.num_comp = 3;
            dst.ion_len[0] = dst_stride[0] * output_ptr[0]->height;
            dst.ion_len[1] = dst_stride[0] * output_ptr[0]->height;
            dst.ion_len[2] = dst_stride[0] * output_ptr[0]->height;
            dst.va[0] = (unsigned char*)dst_data[0];
            dst.va[1] = (unsigned char*)dst_data[0] + dst.ion_len[0];
            dst.va[2] = (unsigned char*)dst_data[0] + dst.ion_len[1];
            break;
        case FORMAT_BGR_PLANAR:
            src.format = FMT_BGRP;
            src.num_comp = 3;
            src.ion_len[0] = src_stride[0] * input_ptr->height;
            src.ion_len[1] = src_stride[0] * input_ptr->height;
            src.ion_len[2] = src_stride[0] * input_ptr->height;
            src.va[0] = (unsigned char*)src_data[0];
            src.va[1] = (unsigned char*)src_data[0] + src.ion_len[0];
            src.va[2] = (unsigned char*)src_data[0] + src.ion_len[1];
            dst.format = FMT_RGBP;
            dst.num_comp = 3;
            dst.ion_len[0] = dst_stride[0] * output_ptr[0]->height;
            dst.ion_len[1] = dst_stride[0] * output_ptr[0]->height;
            dst.ion_len[2] = dst_stride[0] * output_ptr[0]->height;
            dst.va[0] = (unsigned char*)dst_data[0];
            dst.va[1] = (unsigned char*)dst_data[0] + dst.ion_len[0];
            dst.va[2] = (unsigned char*)dst_data[0] + dst.ion_len[1];
            break;
        case FORMAT_RGBP_SEPARATE:
            src.format = FMT_RGBP;
            dst.format = FMT_RGBP;
            break;
        case FORMAT_BGRP_SEPARATE:
            src.format = FMT_BGRP;
            dst.format = FMT_BGRP;
            break;
        case FORMAT_BGR_PACKED:
            src.format = FMT_BGR;
            dst.format = FMT_BGR;
            break;
        case FORMAT_RGB_PACKED:
            src.format = FMT_RGB;
            dst.format = FMT_RGB;
            break;
        case FORMAT_NV12:
            src.format = FMT_NV12;
            dst.format = FMT_NV12;
            break;
        case FORMAT_COMPRESSED:
            src.format = FMT_NV12;
            dst.format = FMT_NV12;
            break;
        default:
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "resize not support this format %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            for (int i = 0; i < plane_num; i++) {
                free(src_data[i]);
                free(dst_data[i]);
            }
            return BM_ERR_FAILURE;
    }

    struct vpp_rect_s loca;
    loca.x = 0;
    loca.y = 0;
    loca.width = crop_rect.crop_w;
    loca.height = crop_rect.crop_h;
    int vpp_algorithm = VPP_SCALE_BILINEAR;
    switch(algorithm)
    {
        case BMCV_INTER_LINEAR:
            vpp_algorithm = VPP_SCALE_BILINEAR;
            break;
        case BMCV_INTER_NEAREST:
            vpp_algorithm = VPP_SCALE_NEAREST;
            break;
        default:
          bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp cmodel resize only support NEAREST and BILINEAR%s: %s: %d\n",
                    filename(__FILE__), __func__, __LINE__);
          for (int i = 0; i < plane_num; i++) {
              free(src_data[i]);
              free(dst_data[i]);
          }
          return BM_NOT_SUPPORTED;
    }

    int ret = vpp_misc_cmodel(&src, &loca, &dst, 1, CSC_MAX, vpp_algorithm);
    if(ret != 0) {
        for (int i = 0; i < plane_num; i++) {
            free(src_data[i]);
            free(dst_data[i]);
        }
        return BM_ERR_FAILURE;
    }

    bm_image_copy_host_to_device(output_ptr[0][0], dst_data);
    // post process
    if(output_process_mark[0]) {
        std::lock_guard<std::mutex> lock(output.image_private->memory_lock);
        for (int k = 0; k < output_ptr[0][0].image_private->plane_num; k++) {
            if(layout::update_memory_layout(
                handle,
                output_ptr[0][0].image_private->data[k],
                output_ptr[0][0].image_private->memory_layout[k],
                output.image_private->data[k],
                output.image_private->memory_layout[k]) != BM_SUCCESS)
            {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "FATAL ERROR!!! TPU CRASH %s: %s: %d\n",
                        filename(__FILE__), __func__, __LINE__);
                for (int i = 0; i < plane_num; i++) {
                    free(src_data[i]);
                    free(dst_data[i]);
                }
                return BM_ERR_FAILURE;
            }
        }
    }
    for (int i = 0; i < plane_num; i++) {
        free(src_data[i]);
        free(dst_data[i]);
    }

    return BM_SUCCESS;
}

static bm_status_t bmcv_image_vpp_convert_(bm_handle_t           handle,
                                           int                   output_num,
                                           bm_image              input,
                                           bm_image*             output,
                                           bmcv_rect_t*          crop_rect_,
                                           bmcv_resize_algorithm algorithm,
                                           csc_matrix_t*         matrix)
{
    if(input.data_type != DATA_TYPE_EXT_1N_BYTE)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp only support DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    bm_image_data_format_ext data_type = output[0].data_type;
    bm_image_format_ext image_format = output[0].image_format;
    for(int i = 0; i < output_num; i++)
    {
        if(output[i].data_type != DATA_TYPE_EXT_1N_BYTE)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp only support DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }

        if(data_type != output[i].data_type || image_format != output[i].image_format)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "expected consistant output image format %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
    }

    for(int i = 0; i < output_num; i++)
    {
        if(!bm_image_is_attached(output[i]))
        {
            if(bm_image_alloc_dev_mem(output[i], BMCV_HEAP_ANY) != BM_SUCCESS)
            {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                    "output dev alloc fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
                for (int j = 0; j < i; j++)
                {
                    if(bm_image_detach(output[j]) != BM_SUCCESS)
                    {
                        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                            "not correctly create bm_image %s: %s: %d\n", __FILE__, __func__, __LINE__);
                    }
                }
                return BM_ERR_FAILURE;
            }
        }
    }

    vpp_limitation limitation;
    if (bm_vpp_query_limitation(input.image_format,
            output[0].image_format, limitation) != BM_SUCCESS) {
        char src_format[30];
        char dst_format[30];
        format_to_str(input.image_format, src_format);
        format_to_str(output[0].image_format, dst_format);
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, \
                  "vpp not support conversion from %s to %s %s: %s: %d\n", src_format, dst_format,
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    bool        pre_processed_input = false;
    std::shared_ptr<image_warpper> preprocessed_input;
    bm_image*                      input_ptr = &input;
    int expand_width = 0;
    int expand_height = 0;
    std::vector<bmcv_rect_t> rects;
    bmcv_rect_t *crop_rect = crop_rect_;

    // move before pre_process_for_vpp_input because it may change input.width, input.height
    if (!crop_rect_)
    {
        rects.reserve(output_num);
        for (int i = 0; i < output_num; i++)
        {
            rects.push_back({0, 0, input.width, input.height});
        }
        crop_rect = &rects[0];
    }

    if (pre_process_for_vpp_input(
            handle, input, pre_processed_input, preprocessed_input, limitation, expand_height, expand_width) !=
        BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp process for input failed %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    if(pre_processed_input)
    {
        input_ptr = preprocessed_input->inner;
    }

    // This function always return BM_SUCCESS
    preprocess_for_rect(input_ptr->image_format, output[0].image_format, crop_rect, \
           output_num, input_ptr->height, input_ptr->width, expand_height,
           expand_width, limitation);


    std::vector<bool> output_process_mark;
    std::vector<std::shared_ptr<image_warpper>> dsts;

    std::vector<bm_image*> output_ptr;

    if(pre_process_for_vpp_output(
        handle, output, output_num, output_process_mark, dsts, output_ptr, limitation) != BM_SUCCESS)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "output image preprocess fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    if (bm_image_vpp_check_format_single(
            *input_ptr, output_num, crop_rect, output_ptr) != BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp not support this conversion %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    int vpp_algorithm = VPP_SCALE_BILINEAR;
    switch(algorithm)
    {
        case BMCV_INTER_LINEAR:
            vpp_algorithm = VPP_SCALE_BILINEAR;
            break;
        case BMCV_INTER_NEAREST:
            vpp_algorithm = VPP_SCALE_NEAREST;
            break;
        case BMCV_INTER_BICUBIC:
            vpp_algorithm = VPP_SCALE_BICUBIC;
            break;

    }

    struct vpp_mat_s src[16];
    struct vpp_mat_s dst[16];
    struct vpp_rect_s loca[16];
    memset(src, 0, 16 * sizeof(struct vpp_mat_s));
    memset(dst, 0, 16 * sizeof(struct vpp_mat_s));
    memset(loca, 0, 16 * sizeof(struct vpp_rect_s));


//struct vpp_mat_s* src, struct vpp_rect_s* loca, struct vpp_mat_s* dst, int crop_num, char csc_type, int scale_type, struct csc_matrix *matrix
    std::function<int(struct vpp_mat_s * src,
                      struct vpp_rect_s * loca,
                      struct vpp_mat_s * dst,
                      int crop_num,
                      int scale_type)>
               vpp_function;
    csc_matrix csc_matrix_from_vpp;

    if (input.image_format == FORMAT_COMPRESSED) {
        if(matrix == NULL)
        {
            vpp_function = std::bind(fbd_csc_crop_multi_resize_ctype_stype,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_MAX,
                                     std::placeholders::_5);
        }
        else
        {
            // we do not use csc_matrix in api parameter
            // as it would expose vpp header to the user
            memcpy(&csc_matrix_from_vpp, matrix, sizeof(csc_matrix_from_vpp));
            vpp_function                  = std::bind(fbd_matrix,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_USER_DEFINED,
                                     std::placeholders::_5,
                                     &csc_matrix_from_vpp);
        }
    }
    else
    {
        if(matrix == NULL)
        {
            vpp_function = std::bind(vpp_misc,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_MAX,
                                     std::placeholders::_5);
        }
        else
        {
            // we do not use csc_matrix in api parameter
            // as it would expose vpp header to the user
            memcpy(&csc_matrix_from_vpp, matrix, sizeof(csc_matrix_from_vpp));
            vpp_function                  = std::bind(vpp_misc_matrix,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_USER_DEFINED,
                                     std::placeholders::_5,
                                     &csc_matrix_from_vpp);
        }
    }
    //auto vpp_function_ = input.image_format == FORMAT_COMPRESSED ? fbd_csc_crop_multi_resize_ctype_stype : vpp_misc;
    int ret = bm_image_to_vpp_mat(handle, input_ptr[0], src);
    if(ret != 0)
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp not support this format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    for(int i = 1; i < 16; i++)
    {
        memcpy(src + i, src, sizeof(vpp_mat_s));
    }

    int loop_num = (output_num + 15) >> 4;
    for(int loop = 0; loop < loop_num; loop++)
    {
        int cur_crop_num = (loop == loop_num - 1) ? (output_num - 16 * loop) : 16;
        for(int k = 0; k < cur_crop_num; k++)
        {
            ret = bm_image_to_vpp_mat(handle, output_ptr[16 * loop + k][0], &dst[k]);
            if(ret != 0)
            {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp not support this format %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }

            loca[k].x = crop_rect[16 * loop + k].start_x;
            loca[k].y = crop_rect[16 * loop + k].start_y;
            loca[k].width = crop_rect[16 * loop + k].crop_w;
            loca[k].height = crop_rect[16 * loop + k].crop_h;
        }

        ret = vpp_function(src, loca, dst, cur_crop_num, vpp_algorithm);

        if(ret != 0)
        {
            //If vpp failed, we may hang there or met vppAssert
            return BM_ERR_FAILURE;
        }
    }

    // post process

    for (int i = 0; i < output_num; i++)
    {
        if(output_process_mark[i])
        {
            std::lock_guard<std::mutex> lock(output[i].image_private->memory_lock);
            for (int k = 0; k < output_ptr[i][0].image_private->plane_num; k++)
            {
                if(layout::update_memory_layout(
                    handle,
                    output_ptr[i][0].image_private->data[k],
                    output_ptr[i][0].image_private->memory_layout[k],
                    output[i].image_private->data[k],
                    output[i].image_private->memory_layout[k]) != BM_SUCCESS)
                {
                    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "FATAL ERROR!!! TPU CRASH %s: %s: %d\n",
                            filename(__FILE__), __func__, __LINE__);
                    return BM_ERR_FAILURE;
                }
            }
        }
    }

    return BM_SUCCESS;
}

bm_status_t bm1684_basic(bm_handle_t          handle,
                        int                   in_img_num,
                        bm_image*             input,
                        bm_image*             output,
                        int*                  crop_num_vec,
                        bmcv_rect_t*          crop_rect,
                        bmcv_padding_atrr_t*  padding_attr,
                        bmcv_resize_algorithm algorithm,
                        csc_type_t            csc_type,
                        csc_matrix_t*         matrix) {
    int out_img_num = 0;
    bmcv_rect_t* rect_inner;
    if (crop_rect == NULL) {
        if (crop_num_vec != NULL) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                      "crop_num_vec should be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
        out_img_num = in_img_num;
        rect_inner = new bmcv_rect_t [out_img_num];
        for (int i = 0; i < in_img_num; i++) {
            rect_inner[i].start_x = 0;
            rect_inner[i].start_y = 0;
            rect_inner[i].crop_w = input[i].width;
            rect_inner[i].crop_h = input[i].height;
        }
    } else {
        if (crop_num_vec == NULL) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                      "crop_num_vec should not be NULL err %s: %s: %d\n", __FILE__, __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
        for (int i = 0; i < in_img_num; i++) {
            for (int j = 0; j < crop_num_vec[i]; j++) {
                if(input[i].image_format==FORMAT_YUV422P && output[out_img_num].image_format==FORMAT_YUV420P){
                    if(crop_rect[out_img_num].start_x!=0 || crop_rect[out_img_num].start_y!=0 ||
                        crop_rect[out_img_num].crop_h!=input[i].height || crop_rect[out_img_num].crop_w!=input[i].width){
                            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                            "vpp not support yuv422p to yuv420p add crop\n", filename(__FILE__), __func__, __LINE__);
                            return BM_ERR_FAILURE;
                    }
                }
                out_img_num = out_img_num + 1;
            }
        }
        rect_inner = crop_rect;
    }
    bmcv_padding_atrr_t* padding_attr_inner;
    if (padding_attr == NULL) {
        padding_attr_inner = new bmcv_padding_atrr_t [out_img_num];
        for (int i = 0; i < out_img_num; i++) {
            padding_attr_inner[i].dst_crop_stx = 0;
            padding_attr_inner[i].dst_crop_sty = 0;
            padding_attr_inner[i].dst_crop_w = output[i].width;
            padding_attr_inner[i].dst_crop_h = output[i].height;
            padding_attr_inner[i].if_memset = 0;
        }
    } else {
        padding_attr_inner = padding_attr;
    }

    int* crop_num_vec_inner;
    if (NULL == crop_num_vec) {
        crop_num_vec_inner = new int[out_img_num];
        for (int i = 0; i < out_img_num; i++) {
            crop_num_vec_inner[i] = 1;
        }
    } else {
        crop_num_vec_inner = crop_num_vec;
    }

    vpp_mat* src = new vpp_mat [in_img_num];
    vpp_mat* dst = new vpp_mat [out_img_num];
    vpp_rect* loca = new vpp_rect [out_img_num];
    memset(src, 0, in_img_num * sizeof(vpp_mat));
    memset(dst, 0, out_img_num * sizeof(vpp_mat));
    memset(loca, 0, out_img_num * sizeof(vpp_rect));
    color_per_ch* color = new color_per_ch [out_img_num];
    int out_idx = 0;
    for (int i = 0; i < in_img_num; i++) {
        for (int j = 0; j < crop_num_vec_inner[i]; j++) {
            /*query limitation according the pair of src_fmt and dst_fmt*/
            vpp_limitation limitation;
            if(input[i].image_format==FORMAT_YUV422P && output[out_idx + j].image_format==FORMAT_YUV420P)
                bm_vpp_query_limitation(FORMAT_GRAY, FORMAT_GRAY, limitation);
            else if(bm_vpp_query_limitation(input[i].image_format,
                    output[out_idx + j].image_format, limitation) != BM_SUCCESS) {
                char src_fmt_str[30];
                char dst_fmt_str[30];
                format_to_str(input[i].image_format, src_fmt_str);
                format_to_str(output[out_idx + j].image_format, dst_fmt_str);
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                          "vpp not support conversion from %s to %s %s: %s: %d\n", src_fmt_str, dst_fmt_str,
                          filename(__FILE__), __func__, __LINE__);
                if (crop_rect == NULL) delete [] rect_inner;
                if (padding_attr == NULL) delete [] padding_attr_inner;
                delete [] src;
                delete [] dst;
                delete [] loca;
                delete [] color;
                return BM_NOT_SUPPORTED;
            }
            bmcv_rect_t dst_crop_rect;
            dst_crop_rect.start_x = padding_attr_inner[out_idx + j].dst_crop_stx;
            dst_crop_rect.start_y = padding_attr_inner[out_idx + j].dst_crop_sty;
            dst_crop_rect.crop_w = padding_attr_inner[out_idx + j].dst_crop_w;
            dst_crop_rect.crop_h = padding_attr_inner[out_idx + j].dst_crop_h;
            color[out_idx + j].ch0 = padding_attr_inner[out_idx + j].padding_r;
            color[out_idx + j].ch1 = padding_attr_inner[out_idx + j].padding_g;
            color[out_idx + j].ch2 = padding_attr_inner[out_idx + j].padding_b;

            /*check input param*/
            if (check_vpp_input_param(handle, input[i], limitation, 1, &dst_crop_rect,
                                      rect_inner + out_idx + j) !=BM_SUCCESS) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                  "vpp input image param err %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                if (crop_rect == NULL) delete [] rect_inner;
                if (padding_attr == NULL) delete [] padding_attr_inner;
                delete [] src;
                delete [] dst;
                delete [] loca;
                delete [] color;
                return BM_ERR_FAILURE;
            }
            /*check output param*/
            if (check_vpp_output_param(handle, input[i], rect_inner + out_idx + j, 1, output + out_idx + j,
                                       &dst_crop_rect, limitation) != BM_SUCCESS) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                    "vpp output image param err %s: %s: %d\n", __FILE__, __func__, __LINE__);
                if (crop_rect == NULL) delete [] rect_inner;
                if (padding_attr == NULL) delete [] padding_attr_inner;
                delete [] src;
                delete [] dst;
                delete [] loca;
                delete [] color;
                return BM_ERR_FAILURE;
            }

            loca[out_idx + j].x = rect_inner[out_idx + j].start_x;
            loca[out_idx + j].y = rect_inner[out_idx + j].start_y;
            loca[out_idx + j].width = rect_inner[out_idx + j].crop_w;
            loca[out_idx + j].height = rect_inner[out_idx + j].crop_h;
            /* output bm_image to vpp_mat*/
            if (bm_image_to_vpp_mat(handle, output[out_idx + j], dst + out_idx + j, &dst_crop_rect)) {
                if (crop_rect == NULL) delete [] rect_inner;
                if (padding_attr == NULL) delete [] padding_attr_inner;
                if (crop_num_vec == NULL) delete [] crop_num_vec_inner;
                delete [] src;
                delete [] dst;
                delete [] loca;
                delete [] color;
                return BM_NOT_SUPPORTED;
            }
            /*memset dst image device mem*/
            if (padding_attr_inner[out_idx + j].if_memset == 1) {
                if (bmcv_image_vpp_memset(handle, output + out_idx + j, color + out_idx + j) != BM_SUCCESS) {
                    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                              "bmcv_image_vpp_memset err %s: %s: %d\n", __FILE__, __func__, __LINE__);
                    if (crop_rect == NULL) delete [] rect_inner;
                    if (padding_attr == NULL) delete [] padding_attr_inner;
                    if (crop_num_vec == NULL) delete [] crop_num_vec_inner;
                    delete [] src;
                    delete [] dst;
                    delete [] loca;
                    delete [] color;
                    return BM_ERR_FAILURE;
                }
            }
        }
        /* input bm_image to vpp_mat*/
        if (bm_image_to_vpp_mat(handle, input[i], src + i)) {
            if (crop_rect == NULL) delete [] rect_inner;
            if (padding_attr == NULL) delete [] padding_attr_inner;
            if (crop_num_vec == NULL) delete [] crop_num_vec_inner;
            delete [] src;
            delete [] dst;
            delete [] loca;
            delete [] color;
            return BM_NOT_SUPPORTED;
        }
        out_idx += crop_num_vec_inner[i];
    }

    vpp_scale_type vpp_scale_algorithm = VPP_SCALE_BILINEAR;
    switch (algorithm) {
        case BMCV_INTER_LINEAR:
            vpp_scale_algorithm = VPP_SCALE_BILINEAR;
            break;
        case BMCV_INTER_NEAREST:
            vpp_scale_algorithm = VPP_SCALE_NEAREST;
            break;
        case BMCV_INTER_BICUBIC:
            vpp_scale_algorithm = VPP_SCALE_BICUBIC;
            break;

    }

    vpp_csc_type vpp_csc = CSC_MAX;
    switch (csc_type) {
        case CSC_YCbCr2RGB_BT601:
            vpp_csc = YCbCr2RGB_BT601;
            break;
        case CSC_YPbPr2RGB_BT601:
            vpp_csc = YPbPr2RGB_BT601;
            break;
        case CSC_RGB2YCbCr_BT601:
            vpp_csc = RGB2YCbCr_BT601;
            break;
        case CSC_YCbCr2RGB_BT709:
            vpp_csc = YCbCr2RGB_BT709;
            break;
        case CSC_RGB2YCbCr_BT709:
            vpp_csc = RGB2YCbCr_BT709;
            break;
        case CSC_RGB2YPbPr_BT601:
            vpp_csc = RGB2YPbPr_BT601;
            break;
        case CSC_YPbPr2RGB_BT709:
            vpp_csc = YPbPr2RGB_BT709;
            break;
        case CSC_RGB2YPbPr_BT709:
            vpp_csc = RGB2YPbPr_BT709;
            break;
        case CSC_USER_DEFINED_MATRIX:
            vpp_csc = CSC_USER_DEFINED;
            break;
        case CSC_MAX_ENUM:
            vpp_csc = CSC_MAX;
            break;
        default:
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "invalidate csc enum\n");
            if (crop_rect == NULL) delete [] rect_inner;
            if (padding_attr == NULL) delete [] padding_attr_inner;
            if (crop_num_vec == NULL) delete [] crop_num_vec_inner;
            delete [] src;
            delete [] dst;
            delete [] loca;
            delete [] color;
            return BM_ERR_FAILURE;
    }
    /*call native vpp func and start vpp hw to work*/
    int ret = VPP_OK;
    if(input->image_format!=FORMAT_COMPRESSED){
        ret = vpp_basic(src, loca, dst, in_img_num, crop_num_vec_inner, vpp_csc, vpp_scale_algorithm, (vpp_csc_matrix*)matrix);
    }
    else{
        int crop_num = 0;
        for (int i = 0; i < in_img_num; i++) {
            crop_num += crop_num_vec[i];
        }
        vpp_mat buf[VPP_CROP_NUM_MAX];
        int idx = 0;
        for (int i = 0; i < in_img_num; i++) {
            for (int j = 0; j < crop_num_vec[i]; j++) {
                buf[idx + j] = src[i];
            }
            idx += crop_num_vec[i];
        }
        ret = fbd_csc_crop_multi_resize_ctype_stype(buf, loca, dst, crop_num, vpp_csc, vpp_scale_algorithm);
    }
    if (ret != 0) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "vpp basic return err %s: %s: %d\n",
                 filename(__FILE__), __func__, __LINE__);
        if (crop_rect == NULL) delete [] rect_inner;
        if (padding_attr == NULL) delete [] padding_attr_inner;
        if (crop_num_vec == NULL) delete [] crop_num_vec_inner;
        delete [] src;
        delete [] dst;
        delete [] loca;
        delete [] color;
        return BM_ERR_FAILURE;
    }

    if (crop_rect == NULL) delete [] rect_inner;
    if (padding_attr == NULL) delete [] padding_attr_inner;
    if (crop_num_vec == NULL) delete [] crop_num_vec_inner;
    delete [] src;
    delete [] dst;
    delete [] loca;
    delete [] color;

    return BM_SUCCESS;
}

bm_status_t basic_two_steps_check(int         in_img_num,
                                bm_image*     input,
                                bm_image*     output,
                                int*          crop_num_vec){
    bm_status_t ret = BM_SUCCESS;
    if(input->image_format == FORMAT_YUV422P){
        if(output->image_format == FORMAT_YUV420P)
            ret = BM_NOT_SUPPORTED;
        else if(output->image_format == FORMAT_RGB_PACKED ||
                output->image_format == FORMAT_BGR_PACKED ||
                output->image_format == FORMAT_RGBP_SEPARATE ||
                output->image_format == FORMAT_BGRP_SEPARATE){
            if(in_img_num != 1 || crop_num_vec[0] != 1){
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "yuv422 input does not support multiple batches %s: %s: %d\n", __FILE__, __func__, __LINE__);
                ret = BM_ERR_FAILURE;
            }
        }
        else
            ret = BM_NOT_SUPPORTED;
        }
    else if(input->image_format == FORMAT_YUV420P ||
            input->image_format == FORMAT_NV12){
        if(output->image_format == FORMAT_YUV444P){
            if(in_img_num != 1 || crop_num_vec[0] != 1){
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "yuv420p to yuv444p does not support multiple batches %s: %s: %d\n", __FILE__, __func__, __LINE__);
                ret = BM_ERR_FAILURE;
            }
        }
        else
            ret = BM_NOT_SUPPORTED;
    }
    else{
        ret = BM_NOT_SUPPORTED;
    }
    return ret;
}

bm_status_t basic_two_steps(bm_handle_t           handle,
                            bm_image*             input,
                            bm_image*             output,
                            bmcv_rect_t*          crop_rect,
                            bmcv_padding_atrr_t*  padding_attr,
                            bmcv_resize_algorithm algorithm,
                            csc_type_t            csc_type,
                            csc_matrix_t*         matrix){
    bm_status_t ret = BM_SUCCESS;
    bm_image middle;
    int single_num = 1;
    int stride[3] = {0};
    bm_image_format_ext middle_fmt = input->image_format == FORMAT_YUV422P ? FORMAT_YUV420P: FORMAT_BGR_PLANAR;
    if(middle_fmt == FORMAT_YUV420P){
        stride[0] = ALIGN(input->width, 64);
        stride[1] = stride[0] >> 1;
        stride[2] = stride[1];
    }
    else if(middle_fmt == FORMAT_BGR_PLANAR){
        stride[0] = ALIGN(input->width, 64);
        csc_type = CSC_MAX_ENUM;
    }
    bm_image_create(handle, input->height, input->width, middle_fmt, input->data_type, &middle, stride);
    ret = bm_image_alloc_dev_mem(middle, BMCV_HEAP1_ID);
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
        "middle_dst dev alloc fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
        goto fail;
    }
    ret = bm1684_basic(handle, 1, input, &middle, NULL, NULL, NULL, algorithm, CSC_MAX_ENUM, matrix);
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "middle_dst convert fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
        goto fail;
    }
    ret = bm1684_basic(handle, 1, &middle, output, &single_num, crop_rect, padding_attr, algorithm, csc_type, matrix);
    if(ret != BM_SUCCESS){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "dst convert fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
    }
fail:
    bm_image_destroy(middle);
    return ret;
}

bm_status_t bm1684_vpp_basic(bm_handle_t              handle,
                                int                   in_img_num,
                                bm_image*             input,
                                bm_image*             output,
                                int*                  crop_num_vec,
                                bmcv_rect_t*          crop_rect,
                                bmcv_padding_atrr_t*  padding_attr,
                                bmcv_resize_algorithm algorithm,
                                csc_type_t            csc_type,
                                csc_matrix_t*         matrix){
    bm_status_t ret = BM_SUCCESS;
    bm_status_t is_two_steps = basic_two_steps_check(in_img_num, input, output, crop_num_vec);
    if(is_two_steps == BM_SUCCESS)
        ret = basic_two_steps(handle, input, output, crop_rect, padding_attr, algorithm, csc_type, matrix);
    else if(is_two_steps == BM_NOT_SUPPORTED)
        ret = bm1684_basic(handle, in_img_num, input, output, crop_num_vec, crop_rect, padding_attr, algorithm, csc_type, matrix);
    else
        return is_two_steps;
    return ret;
}

bm_status_t bm1684_vpp_cvt_padding(
    bm_handle_t           handle,
    int                   output_num,
    bm_image              input,
    bm_image*             output,
    bmcv_padding_atrr_t*  padding_attr,
    bmcv_rect_t*          crop_rect,
    bmcv_resize_algorithm algorithm,
    csc_matrix_t*         matrix = NULL) {
    if (padding_attr == NULL) {
        bmlib_log("VPP_PADDING", BMLIB_LOG_ERROR, "vpp padding info is nullptr");
        return BM_ERR_FAILURE;
    }
    std::vector<bool> alloc_out_dmem(false);
    auto free_dmem = [&]() {
        for (int i = 0; i < output_num; i++) {
            if (alloc_out_dmem[i]) {
                bm_image_detach(output[i]);
            }
        }
    };
    for (int i = 0; i < output_num; i++) {
        if (!bm_image_is_attached(output[i])) {
            if (bm_image_alloc_dev_mem(output[i], BMCV_HEAP_ANY) != BM_SUCCESS) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                    "output dev alloc fail %s: %s: %d\n", __FILE__, __func__, __LINE__);
                free_dmem();
                return BM_ERR_FAILURE;
            }
            alloc_out_dmem[i] = true;
        }
    }
    /*query limitation according the pair of src_fmt and dst_fmt*/
    vpp_limitation limitation;
    if (bm_vpp_query_limitation(input.image_format,
            output[0].image_format, limitation) != BM_SUCCESS) {
        char src_fmt_str[30];
        char dst_fmt_str[30];
        format_to_str(input.image_format, src_fmt_str);
        format_to_str(output[0].image_format, dst_fmt_str);
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                  "vpp not support conversion from %s to %s %s: %s: %d\n", src_fmt_str, dst_fmt_str,
                  filename(__FILE__), __func__, __LINE__);
        free_dmem();
        return BM_NOT_SUPPORTED;
    }

    #ifdef __linux__
    bmcv_rect_t dst_crop_rect[output_num];
    color_per_ch color[output_num];
    #else
    std::shared_ptr<bmcv_rect_t> dst_crop_rect_(new bmcv_rect_t[output_num], std::default_delete<bmcv_rect_t[]>());
    bmcv_rect_t* dst_crop_rect = dst_crop_rect_.get();
    std::shared_ptr<color_per_ch> color_(new color_per_ch[output_num], std::default_delete<color_per_ch[]>());
    color_per_ch* color = color_.get();
    #endif

    for (int idx = 0; idx < output_num; idx++) {
        dst_crop_rect[idx].start_x = padding_attr[idx].dst_crop_stx;
        dst_crop_rect[idx].start_y = padding_attr[idx].dst_crop_sty;
        dst_crop_rect[idx].crop_w = padding_attr[idx].dst_crop_w;
        dst_crop_rect[idx].crop_h = padding_attr[idx].dst_crop_h;
        color[idx].ch0 = padding_attr[idx].padding_r;
        color[idx].ch1 = padding_attr[idx].padding_g;
        color[idx].ch2 = padding_attr[idx].padding_b;
        /*memset dst image device mem*/
        if (padding_attr[idx].if_memset == 1) {
            if (bmcv_image_vpp_memset(handle, output + idx, color + idx) != BM_SUCCESS) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "bmcv_image_vpp_memset err %s: %s: %d\n", __FILE__, __func__, __LINE__);
                free_dmem();
                return BM_ERR_FAILURE;
            }
        }

    }

    /*check input param*/
    if (check_vpp_input_param(handle, input, limitation,
      output_num, dst_crop_rect, crop_rect) !=BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "vpp input image param err %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        free_dmem();
        return BM_ERR_FAILURE;
    }

    /*check output param*/
    if (check_vpp_output_param(
        handle, input, crop_rect, output_num, output, dst_crop_rect, limitation) != BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
            "vpp output image param err %s: %s: %d\n", __FILE__, __func__, __LINE__);
        free_dmem();
        return BM_ERR_FAILURE;
    }

    /*prepare for vpp hw func: bind native vpp func*/
    int vpp_algorithm = VPP_SCALE_BILINEAR;
    switch(algorithm) {
        case BMCV_INTER_LINEAR:
            vpp_algorithm = VPP_SCALE_BILINEAR;
            break;
        case BMCV_INTER_NEAREST:
            vpp_algorithm = VPP_SCALE_NEAREST;
            break;
        case BMCV_INTER_BICUBIC:
            vpp_algorithm = VPP_SCALE_BICUBIC;
            break;
    }

    struct vpp_mat_s src[16];
    struct vpp_mat_s dst[16];
    struct vpp_rect_s loca[16];
    memset(src, 0, 16 * sizeof(struct vpp_mat_s));
    memset(dst, 0, 16 * sizeof(struct vpp_mat_s));
    memset(loca, 0, 16 * sizeof(struct vpp_rect_s));

    std::function<int(struct vpp_mat_s * src,
                      struct vpp_rect_s * loca,
                      struct vpp_mat_s * dst,
                      int crop_num,
                      int scale_type)>
               vpp_function;
    csc_matrix csc_matrix_from_vpp;

    if (input.image_format == FORMAT_COMPRESSED) {
        if(matrix == NULL) {
            vpp_function = std::bind(fbd_csc_crop_multi_resize_ctype_stype,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_MAX,
                                     std::placeholders::_5);
        } else {
            // we do not use csc_matrix in api parameter
            // as it would expose vpp header to the user
            memcpy(&csc_matrix_from_vpp, matrix, sizeof(csc_matrix_from_vpp));
            vpp_function                  = std::bind(fbd_matrix,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_USER_DEFINED,
                                     std::placeholders::_5,
                                     &csc_matrix_from_vpp);
        }
    }
    else {
        if (matrix == NULL) {
            vpp_function = std::bind(vpp_misc,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_MAX,
                                     std::placeholders::_5);
        } else {
            // we do not use csc_matrix in api parameter
            // as it would expose vpp header to the user
            memcpy(&csc_matrix_from_vpp, matrix, sizeof(csc_matrix_from_vpp));
            vpp_function                  = std::bind(vpp_misc_matrix,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3,
                                     std::placeholders::_4,
                                     CSC_USER_DEFINED,
                                     std::placeholders::_5,
                                     &csc_matrix_from_vpp);
        }
    }

    /*bm_image to vpp_mat*/
    int ret = bm_image_to_vpp_mat(handle, input, src);
    if (ret != 0) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
          "vpp not support this format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        free_dmem();
        return BM_NOT_SUPPORTED;
    }
    for (int i = 1; i < 16; i++) {
        memcpy(src + i, src, sizeof(vpp_mat_s));
    }

    int loop_num = (output_num + 15) >> 4;
    for (int loop = 0; loop < loop_num; loop++) {
        int cur_crop_num = (loop == loop_num - 1) ? (output_num - 16 * loop) : 16;
        for (int k = 0; k < cur_crop_num; k++) {
            ret = bm_image_to_vpp_mat(handle, output[16 * loop + k], &dst[k],
              &dst_crop_rect[16 * loop + k]);
            if (ret != 0) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp not support this format %s: %s: %d\n",
                          filename(__FILE__), __func__, __LINE__);
                free_dmem();
                return BM_NOT_SUPPORTED;
            }

            loca[k].x = crop_rect[16 * loop + k].start_x;
            loca[k].y = crop_rect[16 * loop + k].start_y;
            loca[k].width = crop_rect[16 * loop + k].crop_w;
            loca[k].height = crop_rect[16 * loop + k].crop_h;
        }

        /*call native vpp func and start vpp hw to work*/
        ret = vpp_function(src, loca, dst, cur_crop_num, vpp_algorithm);

        if (ret != 0) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "native vpp return err %s: %s: %d\n",
                     filename(__FILE__), __func__, __LINE__);
            free_dmem();
            return BM_ERR_FAILURE;
        }
    }

    return BM_SUCCESS;
}

bm_status_t bm1684_vpp_convert_internal(
    bm_handle_t             handle,
    int                     output_num,
    bm_image                input,
    bm_image*               output,
    bmcv_rect_t*            crop_rect_,
    bmcv_resize_algorithm   algorithm,
    csc_matrix_t *        matrix
    )
{
    bm_status_t ret = BM_SUCCESS;
    std::vector<bool> use_cpu;
    use_cpu.resize(output_num);
    #ifdef __linux__
    bm_image img_before_resize[output_num];
    #else
    std::shared_ptr<bm_image> img_before_resize_(new bm_image[output_num], std::default_delete<bm_image[]>());
    bm_image* img_before_resize = img_before_resize_.get();
    #endif
    for (int i = 0; i < output_num; i++) {
        int h = (crop_rect_ == NULL) ? input.height : crop_rect_[i].crop_h;
        int w = (crop_rect_ == NULL) ? input.width : crop_rect_[i].crop_w;
        if (output[i].width > w * 32 || output[i].height > h * 32 ||
            w > output[i].width * 32 || h > output[i].height * 32) {
            bm_image_create(handle,
                            h,
                            w,
                            output[i].image_format,
                            output[i].data_type,
                            &img_before_resize[i]);
            for (int k = 0; k < img_before_resize[i].image_private->plane_num; k++) {
                img_before_resize[i].image_private->memory_layout[k] =
                    layout::align_width(
                        img_before_resize[i].image_private->memory_layout[k],
                        STRIDE_ALIGN);
                 if ((output[i].image_format == FORMAT_YUV420P) && k != 0) {
                    img_before_resize[i].image_private->memory_layout[k] = layout::stride_width(
                    img_before_resize[i].image_private->memory_layout[k],
                    img_before_resize[i].image_private->memory_layout[0].pitch_stride / 2);
                }
            }
            use_cpu[i] = true;
        } else {
            img_before_resize[i] = output[i];
            use_cpu[i] = false;
        }
    }
    ret = bmcv_image_vpp_convert_(handle, output_num, input, img_before_resize, crop_rect_, algorithm, matrix);
    if (ret != BM_SUCCESS) {
        for (int i = 0; i < output_num; i++) {
            if (use_cpu[i]) {
                bm_image_destroy(img_before_resize[i]);
            }
        }
        return ret;
    }
    for (int i = 0; i < output_num; i++) {
        if (use_cpu[i]) {
            ret = bmcv_image_resize_cpu(handle, img_before_resize[i], output[i], algorithm, matrix);
            if (ret != BM_SUCCESS) {
                bm_image_destroy(img_before_resize[i]);
                return ret;
            }
            bm_image_destroy(img_before_resize[i]);
        }
    }

    return BM_SUCCESS;
}

bm_status_t bm1684_vpp_stitch(
    bm_handle_t           handle,
    int                   input_num,
    bm_image*              input,
    bm_image            output,
    bmcv_rect_t*         dst_crop_rect,
    bmcv_rect_t*         src_crop_rect,
    bmcv_resize_algorithm algorithm) {
    if (handle == NULL) {
        bmlib_log("VPP-STITCH", BMLIB_LOG_ERROR, "handle is nullptr");
        return BM_ERR_FAILURE;
    }
    if (input == NULL) {
        bmlib_log("VPP-STITCH", BMLIB_LOG_ERROR, "input is nullptr");
        return BM_ERR_FAILURE;
    }
    if (input_num > 256) {
        bmlib_log("VPP-STITCH", BMLIB_LOG_ERROR, "input num should less than 256");
        return BM_NOT_SUPPORTED;
    }
    if (dst_crop_rect == NULL) {
        bmlib_log("VPP-STITCH", BMLIB_LOG_ERROR, "dst_crop_rect is nullptr");
        return BM_ERR_FAILURE;
    }
    int ret;
    int input_idx = 0;
    const int output_num = 1;
    bool output_alloc_flag = false;
    if (!bm_image_is_attached(output)) {
        bm_status_t ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            return ret;
        }
        output_alloc_flag = true;
    }

    /*query limitation according the pair of src_fmt and dst_fmt*/
    vpp_limitation limitation;
    if (bm_vpp_query_limitation(input[0].image_format,
            output.image_format, limitation) != BM_SUCCESS) {
        char src_fmt_str[30];
        char dst_fmt_str[30];
        format_to_str(input[0].image_format, src_fmt_str);
        format_to_str(output.image_format, dst_fmt_str);
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                  "vpp not support conversion from %s to %s %s: %s: %d\n", src_fmt_str, dst_fmt_str,
                  filename(__FILE__), __func__, __LINE__);
        if (output_alloc_flag)
            bm_image_detach(output);
        return BM_NOT_SUPPORTED;
    }

    std::vector<bmcv_rect_t> rects;
    if (!src_crop_rect)
    {
        rects.reserve(input_num);
        for (input_idx = 0; input_idx < input_num; input_idx++)
        {
            rects.push_back({0, 0, input[input_idx].width, input[input_idx].height});
        }
        src_crop_rect = &rects[0];
    }

    for (input_idx = 0; input_idx < input_num; input_idx++) {
        if (input[input_idx].image_format != input[0].image_format) {
            bmlib_log("VPP-STITCH", BMLIB_LOG_ERROR, "input format and output format is not same");
            if (output_alloc_flag)
                bm_image_detach(output);
            return BM_NOT_SUPPORTED;
        }
        /*check input param*/
        if (check_vpp_input_param(handle, input[input_idx], limitation,
            output_num, &dst_crop_rect[input_idx], &src_crop_rect[input_idx]) !=BM_SUCCESS) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
            "vpp input image param err %s: %s: %d\n",
                      filename(__FILE__), __func__, __LINE__);
            if (output_alloc_flag)
                bm_image_detach(output);
            return BM_ERR_FAILURE;
        }

        /*check output param*/
        if (check_vpp_output_param(
            handle, input[input_idx], &src_crop_rect[input_idx], output_num,
            &output, &dst_crop_rect[input_idx], limitation) != BM_SUCCESS) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, \
                "vpp output image param err %s: %s: %d\n", __FILE__, __func__, __LINE__);
            if (output_alloc_flag)
                bm_image_detach(output);
            return BM_ERR_FAILURE;
        }
    }


    #ifdef __linux__
    struct vpp_mat_s src_mat[input_num];
    struct vpp_mat_s dst_mat[input_num];
    struct vpp_rect_s loca[input_num];
    #else
    std::shared_ptr<struct vpp_mat_s> src_mat_(new struct vpp_mat_s[input_num], std::default_delete<struct vpp_mat_s[]>());
    struct vpp_mat_s*                 src_mat = src_mat_.get();
    std::shared_ptr<struct vpp_mat_s> dst_mat_(new struct vpp_mat_s[input_num], std::default_delete<struct vpp_mat_s[]>());
    struct vpp_mat_s*                 dst_mat = dst_mat_.get();
    std::shared_ptr<struct vpp_rect_s> loca_(new struct vpp_rect_s[input_num], std::default_delete<struct vpp_rect_s[]>());
    struct vpp_rect_s*                 loca = loca_.get();
    #endif
    memset(src_mat, 0, input_num * sizeof(struct vpp_mat_s));
    memset(dst_mat, 0, input_num * sizeof(struct vpp_mat_s));
    memset(loca, 0, input_num * sizeof(struct vpp_rect_s));

    /*bm_image to vpp_mat*/
    for (input_idx = 0; input_idx < input_num; input_idx++) {
        ret = bm_image_to_vpp_mat(handle, input[input_idx], &src_mat[input_idx]);
        if (ret != 0) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "vpp not support this format %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
            if (output_alloc_flag)
                bm_image_detach(output);
            return BM_NOT_SUPPORTED;
        }

        loca[input_idx].x = src_crop_rect[input_idx].start_x;
        loca[input_idx].y = src_crop_rect[input_idx].start_y;
        loca[input_idx].width = src_crop_rect[input_idx].crop_w;
        loca[input_idx].height = src_crop_rect[input_idx].crop_h;

        ret = bm_image_to_vpp_mat(handle, output, &dst_mat[input_idx], &dst_crop_rect[input_idx]);
        if (ret != 0) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "vpp not support this format %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
            if (output_alloc_flag)
                bm_image_detach(output);
            return BM_NOT_SUPPORTED;
        }
    }

    /*prepare for vpp hw func: bind native vpp func*/
    int vpp_algorithm = VPP_SCALE_BILINEAR;
    switch(algorithm) {
        case BMCV_INTER_LINEAR:
            vpp_algorithm = VPP_SCALE_BILINEAR;
            break;
        case BMCV_INTER_NEAREST:
            vpp_algorithm = VPP_SCALE_NEAREST;
            break;
        case BMCV_INTER_BICUBIC:
            vpp_algorithm = VPP_SCALE_BICUBIC;
            break;
    }

    ret = vpp_misc(src_mat, loca, dst_mat, input_num, CSC_MAX, vpp_algorithm);
    if (ret != VPP_OK) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "native vpp return err %s: %s: %d\n",
             filename(__FILE__), __func__, __LINE__);
        if (output_alloc_flag)
            bm_image_detach(output);
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

// Some useful conversion matrix parameter here
/*
const csc_matrix_t YCbCr2RGB_BT601 = {
    1192, 0, 1634, -228255, 1192, -403, -833, 139193, 1192, 2064, 0, -283328;
const csc_matrix_t YPbPr2RGB_BT601 = {
    1024, 0, 1436, -183748, 1024, -354, -732, 139029, 1024, 1813, 0, -232126};
const csc_matrix_t RGB2YCbCr_BT601 = {
    263, 516, 101, 16384, -152, -298, 450, 131072, 450, -376, -73, 131072};
const csc_matrix_t YCbCr2RGB_BT709 = {
    1192, 0, 1836, -254044, 1192, -218, -546, 78734, 1192, 2163, 0, -295956};
const csc_matrix_t RGB2YCbCr_BT709 = {
    187, 629, 63, 16384, -103, -347, 450, 131072, 450, -409, -41, 131072};
*/

namespace bmcv
{
    csc_matrix_t YCbCr2RGB_BT601 = { 0x000004a8, 0x00000000, 0x00000662, (int)0xfffc8450, \
                     0x000004a8, (int)0xfffffe6f, (int)0xfffffcc0, 0x00021e4d, \
                     0x000004a8, 0x00000812, 0x00000000, (int)0xfffbaca8 };
    csc_matrix_t YPbPr2RGB_BT601 = { 0x00000400, 0x00000000, 0x0000059b, (int)0xfffd322d, \
                     0x00000400, (int)0xfffffea0, (int)0xfffffd25, 0x00021dd6, \
                     0x00000400, 0x00000716, 0x00000000, (int)0xfffc74bc };
    csc_matrix_t RGB2YCbCr_BT601 = { 0x107, 0x204, 0x64, 0x4000, \
                     (int)0xffffff68, (int)0xfffffed6, 0x1c2, 0x20000, \
                     0x1c2, (int)0xfffffe87, (int)0xffffffb7, 0x20000 };
    csc_matrix_t YCbCr2RGB_BT709 = { 0x000004a8, 0x00000000, 0x0000072c, (int)0xfffc1fa4, \
                     0x000004a8, (int)0xffffff26, (int)0xfffffdde, 0x0001338e, \
                     0x000004a8, 0x00000873, 0x00000000, (int)0xfffb7bec };
    csc_matrix_t RGB2YCbCr_BT709 = { 0x000000bb, 0x00000275, 0x0000003f, 0x00004000, \
                     (int)0xffffff99, (int)0xfffffea5, 0x000001c2, 0x00020000, \
                     0x000001c2, (int)0xfffffe67, (int)0xffffffd7, 0x00020000 };
    csc_matrix_t RGB2YPbPr_BT601 = { 0x132, 0x259, 0x74, 0, \
                     (int)0xffffff54, (int)0xfffffead, 0x200, 0x20000, \
                     0x200, (int)0xfffffe54, (int)0xffffffad, 0x20000 };
    csc_matrix_t YPbPr2RGB_BT709 = { 0x00000400, 0x00000000, 0x0000064d, (int)0xfffcd9be, \
                                     0x00000400, (int)0xffffff40, (int)0xfffffe21, 0x00014fa1, \
                                     0x00000400, 0x0000076c, 0x00000000, (int)0xfffc49ed };
    csc_matrix_t RGB2YPbPr_BT709 = { 0x000000da, 0x000002dc, 0x0000004a, 0, \
                                     (int)0xffffff8b, (int)0xfffffe75, 0x00000200, 0x00020000, \
                                     0x00000200, (int)0xfffffe2f, (int)0xffffffd1, 0x00020000 };
    csc_matrix_t* useful_conversion_matrix[] = {
        &YCbCr2RGB_BT601,
        &YPbPr2RGB_BT601,
        &RGB2YCbCr_BT601,
        &YCbCr2RGB_BT709,
        &RGB2YCbCr_BT709,
        &RGB2YPbPr_BT601,
        &YPbPr2RGB_BT709,
        &RGB2YPbPr_BT709};
};  // namespace bmcv

bool inline is_csc_yuv(bm_image_format_ext format)
{
    return (format == FORMAT_NV12 || format == FORMAT_YUV420P ||
            format == FORMAT_YUV422P || format == FORMAT_YUV444P ||
            format == FORMAT_COMPRESSED || format == FORMAT_GRAY);
}

bool inline is_csc_rgb(bm_image_format_ext format)
{
    return (format == FORMAT_BGR_PACKED || format == FORMAT_BGR_PLANAR ||
            format == FORMAT_BGRP_SEPARATE || format == FORMAT_RGB_PACKED ||
            format == FORMAT_RGB_PLANAR || format == FORMAT_RGBP_SEPARATE ||
            format == FORMAT_ARGB_PACKED|| format == FORMAT_ABGR_PACKED);
}

bm_status_t bm1684_vpp_csc_matrix_convert(
    bm_handle_t           handle,
    int                   output_num,
    bm_image              input,
    bm_image*             output,
    csc_type_t            csc,
    csc_matrix_t*         matrix,
    bmcv_resize_algorithm algorithm,
    bmcv_rect_t *         crop_rect )
{
    if(csc == CSC_MAX_ENUM)
    {
        return bm1684_vpp_convert_internal(
            handle, output_num, input, output, crop_rect, algorithm, nullptr);
    }
    switch(csc)
    {
        case CSC_YPbPr2RGB_BT601:
        case CSC_YCbCr2RGB_BT709:
        case CSC_YCbCr2RGB_BT601:
        case CSC_YPbPr2RGB_BT709:
            if (!is_csc_yuv(input.image_format)) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                       "input image format should be yuv csc and vpp should support this format %s: %s: %d",
                        filename(__FILE__), __func__, __LINE__);
                return BM_ERR_PARAM;
            }
            for (int i = 0; i < output_num; i++)
            {
                if (!is_csc_rgb(output[i].image_format)) {
                    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                            "output image format should be rgb csc and vpp should support this format %s: %s: %d",
                            filename(__FILE__), __func__, __LINE__);
                    return BM_ERR_PARAM;
                }
            }
            matrix = bmcv::useful_conversion_matrix[(int)csc];
            break;
        case CSC_RGB2YCbCr_BT601:
        case CSC_RGB2YCbCr_BT709:
        case CSC_RGB2YPbPr_BT601:
        case CSC_RGB2YPbPr_BT709:
            if (!is_csc_rgb(input.image_format)) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                        "input image format should be yuv csc and vpp should support this format %s: %s: %d",
                        filename(__FILE__), __func__, __LINE__);
                return BM_ERR_PARAM;
            }
            for (int i = 0; i < output_num; i++)
            {
                if (!is_csc_yuv(output[i].image_format)) {
                    bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR,
                            "output image format should be rgb csc and vpp should support this format %s: %s: %d",
                            filename(__FILE__), __func__, __LINE__);
                    return BM_ERR_PARAM;
                }
            }
            matrix = bmcv::useful_conversion_matrix[(int)csc];
            break;
        case CSC_USER_DEFINED_MATRIX:
            if (matrix == nullptr) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "please input a valid matrix\n");
                return BM_ERR_PARAM;
            }
            break;
        default:
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "invalidate csc enum\n");
            return BM_ERR_PARAM;
    }

    return bm1684_vpp_convert_internal(
        handle, output_num, input, output, crop_rect, algorithm, matrix);
}

#endif

