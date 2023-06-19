#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_api.h"
#include "bmcv_common_bm1684.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define CHECK_RET(status)                                                      \
    if (status != BM_SUCCESS)                                                  \
    return status

static bm_status_t fill_image_private(bm_image *res, int *stride) {
    int data_size = 1;
    switch (res->data_type) {
        case DATA_TYPE_EXT_FLOAT32:
            data_size = 4;
            break;
        case DATA_TYPE_EXT_4N_BYTE:
        case DATA_TYPE_EXT_4N_BYTE_SIGNED:
            data_size = 4;
            break;
        case DATA_TYPE_EXT_FP16:
        case DATA_TYPE_EXT_BF16:
            data_size = 2;
            break;
        default:
            data_size = 1;
            break;
    }
    bool              use_default_stride = stride ? false : true;
    bm_image_private *image_private      = res->image_private;
    int               H                  = res->height;
    int               W                  = res->width;
    switch (res->image_format) {
        case FORMAT_YUV420P: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] = layout::plane_layout(
                1, 1, ALIGN(H, 2) >> 1, ALIGN(W, 2) >> 1, data_size);
            image_private->memory_layout[2] = layout::plane_layout(
                1, 1, ALIGN(H, 2) >> 1, ALIGN(W, 2) >> 1, data_size);
            break;
        }
        case FORMAT_YUV422P: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2) >> 1, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2) >> 1, data_size);
            break;
        }
        case FORMAT_YUV444P: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_NV24: {
            image_private->plane_num = 2;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2), data_size);
            break;
        }
        case FORMAT_NV12:
        case FORMAT_NV21: {
            image_private->plane_num = 2;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] = layout::plane_layout(
                1, 1, ALIGN(H, 2) >> 1, ALIGN(W, 2), data_size);
            break;
        }
        case FORMAT_NV16:
        case FORMAT_NV61: {
            image_private->plane_num = 2;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2), data_size);
            break;
        }
        case FORMAT_GRAY: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_COMPRESSED: {
            image_private->plane_num        = 4;
            image_private->memory_layout[0] = image_private->memory_layout[1] =
                image_private->memory_layout[2] =
                    image_private->memory_layout[3] =
                        layout::plane_layout(0, 0, 0, 0, 1);
            use_default_stride = true;
            break;
        }
        case FORMAT_YUV444_PACKED:
        case FORMAT_YVU444_PACKED:
        case FORMAT_HSV180_PACKED:
        case FORMAT_HSV256_PACKED:
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W * 3, data_size);
            break;
        }
        case FORMAT_ABGR_PACKED:
        case FORMAT_ARGB_PACKED: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W * 4, data_size);
            break;
        }
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 3, H, W, data_size);
            break;
        }
        case FORMAT_BGRP_SEPARATE:
        case FORMAT_RGBP_SEPARATE: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_HSV_PLANAR: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_RGBYP_PLANAR: {
            image_private->plane_num = 4;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[3] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_YUV422_YUYV:
        case FORMAT_YUV422_YVYU:
        case FORMAT_YUV422_UYVY:
        case FORMAT_YUV422_VYUY: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W * 2, data_size);
            break;
        }
    }

    if (!use_default_stride) {
        for (int p = 0; p < res->image_private->plane_num; p++) {
            image_private->memory_layout[p] = layout::stride_width(
                image_private->memory_layout[p], stride[p]);
        }
    }

    res->image_private->default_stride = use_default_stride;
    return BM_SUCCESS;
}

static void fill_image_private_tensor(bm_image_tensor res) {
    int data_size = 1;
    switch (res.image.data_type) {
        case DATA_TYPE_EXT_FLOAT32:
            data_size = 4 * res.image_n;
            break;
        case DATA_TYPE_EXT_4N_BYTE:
            data_size = 4 * (ALIGN(res.image_n, 4) / 4);
            break;
        case DATA_TYPE_EXT_FP16:
        case DATA_TYPE_EXT_BF16:
            data_size = 2 * res.image_n;
            break;
        default:
            data_size = 1 * res.image_n;
            break;
    }

    int  H             = res.image.height;
    int  W             = res.image.width;
    auto image_private = res.image.image_private;
    switch (res.image.image_format) {
        case FORMAT_YUV420P: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] = layout::plane_layout(
                1, 1, ALIGN(H, 2) >> 1, ALIGN(W, 2) >> 1, data_size);
            image_private->memory_layout[2] = layout::plane_layout(
                1, 1, ALIGN(H, 2) >> 1, ALIGN(W, 2) >> 1, data_size);
            break;
        }
        case FORMAT_YUV422P: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2) >> 1, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2) >> 1, data_size);
            break;
        }
        case FORMAT_YUV444P: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_NV24: {
            image_private->plane_num = 2;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2), data_size);
            break;
        }
        case FORMAT_NV12:
        case FORMAT_NV21: {
            image_private->plane_num = 2;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] = layout::plane_layout(
                1, 1, ALIGN(H, 2) >> 1, ALIGN(W, 2), data_size);
            break;
        }
        case FORMAT_NV16:
        case FORMAT_NV61: {
            image_private->plane_num = 2;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, ALIGN(W, 2), data_size);
            break;
        }
        case FORMAT_GRAY: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_YUV444_PACKED:
        case FORMAT_YVU444_PACKED:
        case FORMAT_HSV180_PACKED:
        case FORMAT_HSV256_PACKED:
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W * 3, data_size);
            break;
        }
        case FORMAT_ABGR_PACKED:
        case FORMAT_ARGB_PACKED: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W * 4, data_size);
            break;
        }
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 3, H, W, data_size);
            break;
        }
        case FORMAT_BGRP_SEPARATE:
        case FORMAT_RGBP_SEPARATE: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_HSV_PLANAR: {
            image_private->plane_num = 3;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_RGBYP_PLANAR: {
            image_private->plane_num = 4;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[1] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[2] =
                layout::plane_layout(1, 1, H, W, data_size);
            image_private->memory_layout[3] =
                layout::plane_layout(1, 1, H, W, data_size);
            break;
        }
        case FORMAT_YUV422_YUYV:
        case FORMAT_YUV422_YVYU:
        case FORMAT_YUV422_UYVY:
        case FORMAT_YUV422_VYUY: {
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W * 2, data_size);
            break;
        }
        default:
            image_private->plane_num = 1;
            image_private->memory_layout[0] =
                layout::plane_layout(1, 1, H, W, data_size);
    }
}

static bmcv_color_space color_space_convert(bm_image_format_ext image_format)
{
    switch (image_format)
    {
        case FORMAT_RGB_PLANAR:
        case FORMAT_BGR_PLANAR:
        case FORMAT_HSV_PLANAR:
        case FORMAT_RGB_PACKED:
        case FORMAT_BGR_PACKED:
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE:
        case FORMAT_GRAY:
        case FORMAT_COMPRESSED:
            return COLOR_RGB;
        case FORMAT_YUV420P:
        case FORMAT_YUV422P:
        case FORMAT_YUV444P:
        case FORMAT_NV12:
        case FORMAT_NV21:
        case FORMAT_NV16:
        case FORMAT_NV61:
        case FORMAT_NV24:
            return COLOR_YUV;
        default:
            //color_space = COLOR_YCbCr;
            printf("data format not supported!\n");
            return COLOR_RGB;
    }
}

static bmcv_data_format bmcv_data_format_convert(bm_image_data_format_ext data_format)
{
    switch (data_format)
    {
        case DATA_TYPE_EXT_FLOAT32:
            return DATA_TYPE_FLOAT;
            break;
        case DATA_TYPE_EXT_1N_BYTE:
            return DATA_TYPE_BYTE;
            break;
        default:
            printf("data_format not supported!\n");
            return DATA_TYPE_BYTE;
    }
}

static bm_image_format bmcv_image_format_convert(bm_image_format_ext image_format)
{
    switch (image_format)
    {
        case FORMAT_YUV420P:
            return YUV420P;
            break;
        case FORMAT_NV12:
            return NV12;
            break;
        case FORMAT_NV21:
            return NV21;
            break;
        case FORMAT_RGB_PLANAR:
            return RGB;
            break;
        case FORMAT_BGR_PLANAR:
            return BGR;
            break;
        case FORMAT_RGB_PACKED:
            return RGB_PACKED;
            break;
        case FORMAT_BGR_PACKED:
            return BGR_PACKED;
        default:
            printf("image format not supported!\n");
            return BGR_PACKED;
    }
}

bm_status_t bm_image_format_check(int                      img_h,
                                  int                      img_w,
                                  bm_image_format_ext      image_format,
                                  bm_image_data_format_ext data_type) {
    if (img_h <= 0 || img_w <= 0) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "invalid width or height %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (image_format <= FORMAT_NV24 &&
        (data_type == DATA_TYPE_EXT_4N_BYTE ||
         data_type == DATA_TYPE_EXT_4N_BYTE_SIGNED)) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "YUV related formats don't support data type 4N %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if (FORMAT_COMPRESSED == image_format) {
        if (!(data_type == DATA_TYPE_EXT_1N_BYTE)) {
            bmlib_log("BMCV",
                      BMLIB_LOG_ERROR,
                      "FORMAT_COMPRESSED only support  data  type "
                      "DATA_TYPE_EXT_1N_BYTE %s: %s: %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_NOT_SUPPORTED;
        }
    }
    return BM_SUCCESS;
}

bm_status_t bm_image_create(bm_handle_t              handle,
                            int                      img_h,
                            int                      img_w,
                            bm_image_format_ext      image_format,
                            bm_image_data_format_ext data_type,
                            bm_image *               res,
                            int *                    stride) {
    res->width         = img_w;
    res->height        = img_h;
    res->image_format  = image_format;
    res->data_type     = data_type;
    res->image_private = new bm_image_private;
    if (!res->image_private) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "host memory alloc failed %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_ERR_FAILURE;
    }
    memset(res->image_private->data,
           0,
           sizeof(bm_device_mem_t) * MAX_bm_image_CHANNEL);
    if (bm_image_format_check(img_h, img_w, image_format, data_type) !=
        BM_SUCCESS) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "illegal format or size %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        delete res->image_private;
        res->image_private = nullptr;
        return BM_NOT_SUPPORTED;
    }
    if (fill_image_private(res, stride) != BM_SUCCESS) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "illegal stride %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        delete res->image_private;
        res->image_private = nullptr;
        return BM_NOT_SUPPORTED;
    }
    res->image_private->handle = handle;
    return BM_SUCCESS;
}

bm_status_t bm_image_tensor_create(bm_handle_t              handle,
                                   int                      img_n,
                                   int                      img_c,
                                   int                      img_h,
                                   int                      img_w,
                                   bm_image_format_ext      image_format,
                                   bm_image_data_format_ext data_type,
                                   bm_image_tensor *        res) {
    res->image.width         = img_w;
    res->image.height        = img_h;
    res->image.image_format  = image_format;
    res->image.data_type     = data_type;
    res->image_n             = img_n;
    res->image_c             = img_c;
    res->image.image_private = new bm_image_private;
    if (!res->image.image_private)
        return BM_ERR_FAILURE;

    fill_image_private_tensor(*res);
    res->image.image_private->handle = handle;
    return BM_SUCCESS;
}
bm_status_t bm_image_destroy(bm_image image) {
    if (!image.image_private) {
        return BM_SUCCESS;
    }
    if (bm_image_detach(image) != BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "detach dev mem failed %s %s %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_ERR_FAILURE;
    }
#ifndef USING_CMODEL
    if (image.image_private->decoder != NULL) {
        BmJpuJPEGDecInfo info;
        bm_jpu_jpeg_dec_get_info(image.image_private->decoder, &info);
        bm_jpu_jpeg_dec_frame_finished(image.image_private->decoder, info.framebuffer);
        bm_jpu_jpeg_dec_close(image.image_private->decoder);
    }
#endif
    delete image.image_private;
    image.image_private = nullptr;

    return BM_SUCCESS;
}

bm_handle_t bm_image_get_handle(bm_image *image) {
    if (image->image_private) {
        return image->image_private->handle;
    } else {
        bmlib_log(
            "BMCV",
            BMLIB_LOG_ERROR,
            "Attempting to get handle from a non-existing image %s: %s: %d\n",
            filename(__FILE__),
            __func__,
            __LINE__);
        return 0;
    }
}

bm_status_t bm_image_tensor_destroy(bm_image_tensor image_tensor) {
    return bm_image_destroy(image_tensor.image);
}

bm_status_t bm_image_copy_host_to_device(bm_image image, void *buffers[]) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    if (image.image_format == FORMAT_COMPRESSED) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "compressed format only support attached device memory, not "
                  "host pointer\n");
        return BM_NOT_SUPPORTED;
    }
    // The image didn't attached to a buffer, malloc and own it.
    if (!image.image_private->attached) {
        if (bm_image_alloc_dev_mem(image, BMCV_HEAP_ANY) != BM_SUCCESS) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "alloc dst dev mem failed %s %s %d\n",
                      filename(__FILE__),
                      __func__,
                      __LINE__);
            return BM_ERR_FAILURE;
        }
    }

    unsigned char **host = (unsigned char **)buffers;
    for (int i = 0; i < image.image_private->plane_num; i++) {
        if (BM_SUCCESS != bm_memcpy_s2d(image.image_private->handle,
                                        image.image_private->data[i],
                                        host[i])) {
            BMCV_ERR_LOG("bm_memcpy_s2d error, src addr %p, dst addr 0x%llx\n",
              host[i], bm_mem_get_device_addr(image.image_private->data[i]));

            return BM_ERR_FAILURE;
        }
        // host = host + image.image_private->plane_byte_size[i];
    }
    return BM_SUCCESS;
}

bm_status_t bm_image_tensor_copy_to_device(bm_image_tensor image_tensor,
                                           void **         buffers) {
    return bm_image_copy_host_to_device(image_tensor.image, buffers);
}

bm_status_t bm_image_copy_device_to_host(bm_image image, void *buffers[]) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    if (!image.image_private->attached) {
        return BM_ERR_FAILURE;
    }
    unsigned char **host = (unsigned char **)buffers;
    for (int i = 0; i < image.image_private->plane_num; i++) {
        if (BM_SUCCESS != bm_memcpy_d2s(image.image_private->handle,
                                        host[i],
                                        image.image_private->data[i])) {
            BMCV_ERR_LOG("bm_memcpy_d2s error, src addr 0x%llx, dst addr %p\n",
              bm_mem_get_device_addr(image.image_private->data[i]), host[i]);

            return BM_ERR_FAILURE;
        }
        // host = host + image.image_private->plane_byte_size[i];
    }
    return BM_SUCCESS;
}

bm_status_t bm_image_tensor_copy_from_device(bm_image_tensor image_tensor,
                                             void **         buffers) {
    return bm_image_copy_device_to_host(image_tensor.image, buffers);
}

bm_status_t bm_image_tensor_attach(bm_image_tensor  image_tensor,
                                   bm_device_mem_t *device_memory) {
    return bm_image_attach(image_tensor.image, device_memory);
}

bm_status_t bm_image_attach(bm_image image, bm_device_mem_t *device_memory) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    if (image.image_private->data_owned) {
        std::lock_guard<std::mutex> lock(image.image_private->memory_lock);
        int                         total_size = 0;
        for (int i = 0; i < image.image_private->internal_alloc_plane; i++) {
            // bm_free_device(image.image_private->handle,
            // image.image_private->data[i]);
            total_size += image.image_private->data[i].size;
        }

        bm_device_mem_t dmem = image.image_private->data[0];
        dmem.size            = total_size;

        bm_free_device(image.image_private->handle, dmem);
        image.image_private->internal_alloc_plane = 0;
        image.image_private->data_owned           = false;
    }
    for (int i = 0; i < image.image_private->plane_num; i++) {
        image.image_private->data[i]               = device_memory[i];
        image.image_private->memory_layout[i].size = device_memory[i].size;
        if (image.image_format == FORMAT_COMPRESSED) {
            image.image_private->memory_layout[i] =
                layout::plane_layout(1, 1, 1, device_memory[i].size, 1);
        }
    }
    image.image_private->attached = true;
    return BM_SUCCESS;
}

bm_status_t bm_image_tensor_detach(bm_image image_tensor) {
    return bm_image_detach(image_tensor);
}

bm_status_t bm_image_detach(bm_image image) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    if (image.image_private->data_owned == true) {
        std::lock_guard<std::mutex> lock(image.image_private->memory_lock);
        int                         total_size = 0;
        for (int i = 0; i < image.image_private->internal_alloc_plane; i++) {
            // bm_free_device(image.image_private->handle,
            // image.image_private->data[i]);
            total_size += image.image_private->data[i].size;
        }

        bm_device_mem_t dmem = image.image_private->data[0];
        dmem.size            = total_size;

        bm_free_device(image.image_private->handle, dmem);
        image.image_private->internal_alloc_plane = 0;
        image.image_private->data_owned           = false;
        memset(image.image_private->data,
               0,
               MAX_bm_image_CHANNEL * sizeof(bm_device_mem_t));
    }
    image.image_private->attached = false;
    return BM_SUCCESS;
}

bm_status_t bm_image_get_byte_size(bm_image image, int *size) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    if (image.image_format == FORMAT_COMPRESSED &&
        !image.image_private->attached) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "compressed format plane size is variable, please attached "
                  "device memory first  %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }
    std::lock_guard<std::mutex> lock(image.image_private->memory_lock);
    for (int i = 0; i < image.image_private->plane_num; i++) {
        size[i] = image.image_private->memory_layout[i].size;
    }
    return BM_SUCCESS;
}

bm_status_t bm_image_alloc_dev_mem_heap_mask(bm_image image, int heap_mask) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    if (image.image_format == FORMAT_COMPRESSED) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "compressed format only support attached device memory  %s: "
                  "%s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (image.image_private->data_owned == true) {
        return BM_SUCCESS;
    }
    std::lock_guard<std::mutex> lock(image.image_private->memory_lock);
    // malloc continuious memory for acceleration
    int             total_size = 0;
    bm_device_mem_t dmem;
    for (int i = 0; i < image.image_private->plane_num; ++i) {
        total_size += image.image_private->memory_layout[i].size;
    }

    if (BM_SUCCESS !=
        bm_malloc_device_byte_heap_mask(
            image.image_private->handle, &dmem, heap_mask, total_size)) {
        BMCV_ERR_LOG("bm_malloc_device_byte_heap error\r\n");

        return BM_ERR_FAILURE;
    }

    #ifdef __linux__
    unsigned long base_addr = dmem.u.device.device_addr;
    #else
    unsigned long long base_addr = dmem.u.device.device_addr;
    #endif
    for (int i = 0; i < image.image_private->plane_num; i++) {
        image.image_private->data[i] = bm_mem_from_device(
            base_addr, image.image_private->memory_layout[i].size);
        if (i == 0) {
            image.image_private->data[0].flags.u.gmem_heapid =
                dmem.flags.u.gmem_heapid;
            image.image_private->data[0].u.device.dmabuf_fd =
                dmem.u.device.dmabuf_fd;
        }
        base_addr += image.image_private->memory_layout[i].size;

        image.image_private->internal_alloc_plane++;
    }

    image.image_private->data_owned = true;
    image.image_private->attached   = true;
    return BM_SUCCESS;
}

bm_status_t bm_image_alloc_dev_mem(bm_image image, int heap_id) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    if (image.image_format == FORMAT_COMPRESSED) {
        bmlib_log("BMCV",
                  BMLIB_LOG_ERROR,
                  "compressed format only support attached device memory  %s: "
                  "%s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (image.image_private->data_owned == true) {
        return BM_SUCCESS;
    }
    std::lock_guard<std::mutex> lock(image.image_private->memory_lock);
    // malloc continuious memory for acceleration
    int             total_size = 0;
    bm_device_mem_t dmem;
    for (int i = 0; i < image.image_private->plane_num; ++i) {
        total_size += image.image_private->memory_layout[i].size;
    }

    if (heap_id != BMCV_HEAP_ANY) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte_heap(
                image.image_private->handle, &dmem, heap_id, total_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte_heap error\r\n");

            return BM_ERR_FAILURE;
        }
    } else {
        if (BM_SUCCESS != bm_malloc_device_byte(
                              image.image_private->handle, &dmem, total_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            return BM_ERR_FAILURE;
        }
    }

    #ifdef __linux__
    unsigned long base_addr = dmem.u.device.device_addr;
    #else
    unsigned long long base_addr = dmem.u.device.device_addr;
    #endif
    for (int i = 0; i < image.image_private->plane_num; i++) {
        image.image_private->data[i] = bm_mem_from_device(
            base_addr, image.image_private->memory_layout[i].size);
        if (i == 0) {
            image.image_private->data[0].flags.u.gmem_heapid =
                dmem.flags.u.gmem_heapid;
            image.image_private->data[0].u.device.dmabuf_fd =
                dmem.u.device.dmabuf_fd;
        }
        base_addr += image.image_private->memory_layout[i].size;

        image.image_private->internal_alloc_plane++;
    }

    image.image_private->data_owned = true;
    image.image_private->attached   = true;
    return BM_SUCCESS;
}

// deprecated api
bm_status_t bm_image_dev_mem_alloc(bm_image image,
                                   int      heap_id) {
    return bm_image_alloc_dev_mem(image, heap_id);
}

bm_status_t bm_image_tensor_alloc_dev_mem(bm_image_tensor image_tensor,
                                          int             heap_id) {
    return bm_image_alloc_dev_mem(image_tensor.image, heap_id);
}

bool bm_image_is_attached(bm_image image) {
    if (!image.image_private)
        return false;
    return image.image_private->attached;
}

bool bm_image_tensor_is_attached(bm_image_tensor image_tensor) {
    return bm_image_is_attached(image_tensor.image);
}

bm_status_t bm_image_get_device_mem(bm_image image, bm_device_mem_t *mem) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    for (int i = 0; i < image.image_private->plane_num; i++) {
        mem[i] = image.image_private->data[i];
    }
    return BM_SUCCESS;
}

// Get Internal format information
bm_status_t bm_image_get_format_info(bm_image *              src,
                                     bm_image_format_info_t *info) {
    info->height         = src->height;
    info->width          = src->width;
    info->image_format   = src->image_format;
    info->data_type      = src->data_type;
    info->default_stride = src->image_private->default_stride;
    if (src->image_private == NULL) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "image is not created \n  %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_ERR_PARAM;
    }
    if (src->image_format == FORMAT_COMPRESSED &&
        !src->image_private->attached) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "for compressed format, if device memory is not attached, "
                  "the image info is not intact\n  %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }
    info->plane_nb = src->image_private->plane_num;
    for (int i = 0; i < info->plane_nb; ++i) {
        info->plane_data[i] = src->image_private->data[i];
        info->stride[i]     = src->image_private->memory_layout[i].pitch_stride;
    }

    return BM_SUCCESS;
}

//
// BM image tensor
//

bm_status_t bm_image_tensor_get_device_mem(bm_image_tensor  image_tensor,
                                           bm_device_mem_t *mem) {
    return bm_image_get_device_mem(image_tensor.image, mem);
}

int bm_image_get_plane_num(bm_image image) {
    if (!image.image_private)
        return BM_ERR_FAILURE;
    return image.image_private->plane_num;
}

bm_status_t bm_image_get_stride(bm_image image, int *stride) {
    if (!image.image_private)
        return BM_ERR_FAILURE;

    for (int i = 0; i < image.image_private->plane_num; i++) {
        stride[i] = image.image_private->memory_layout[i].pitch_stride;
    }
    return BM_SUCCESS;
}

bm_status_t bm_image_write_to_bmp(bm_image image, const char *filename) {
    if (!image.image_private || !image.image_private->attached)
        return BM_ERR_FAILURE;
    if (image.data_type == DATA_TYPE_EXT_4N_BYTE ||
        image.data_type == DATA_TYPE_EXT_4N_BYTE_SIGNED) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "not support 4N write  %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }
    int need_format_transform = (image.image_format != FORMAT_RGB_PACKED &&
                                 image.image_format != FORMAT_GRAY) ||
                                image.data_type != DATA_TYPE_EXT_1N_BYTE;
    bm_image image_temp = image;

    if (need_format_transform) {
        bm_status_t ret = bm_image_create(image.image_private->handle,
                                          image.height,
                                          image.width,
                                          FORMAT_RGB_PACKED,
                                          DATA_TYPE_EXT_1N_BYTE,
                                          &image_temp);
        if (ret != BM_SUCCESS) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "malloc failed\n");
            return ret;
        }
        ret = bmcv_image_storage_convert(
            image.image_private->handle, 1, &image, &image_temp);
        if (ret != BM_SUCCESS) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "failed to convert to suitable format\n");
            bm_image_destroy(image_temp);
            return ret;
        }
    }
    int component = image.image_format == FORMAT_GRAY ? 1 : 3;
    void *      buf = malloc(image_temp.width * image_temp.height * component);
    bm_status_t ret = bm_image_copy_device_to_host(image_temp, &buf);
    if (ret != BM_SUCCESS) {
        free(buf);
        if (need_format_transform) {
            bm_image_destroy(image_temp);
        }
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "failed to copy from device memory\n");
        return ret;
    }

    if (stbi_write_bmp(filename, image_temp.width, image_temp.height, component, buf) ==
        0) {
        free(buf);
        if (need_format_transform) {
            bm_image_destroy(image_temp);
        }
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "failed to write to file\n");
        return BM_ERR_FAILURE;
    }


    free(buf);
    if (need_format_transform) {
        bm_image_destroy(image_temp);
    }
    return BM_SUCCESS;
}

bm_status_t bm_image_tensor_init(bm_image_tensor *image_tensor) {
    image_tensor->image_n             = 0;
    image_tensor->image_c             = 0;
    image_tensor->image.width         = 0;
    image_tensor->image.height        = 0;
    image_tensor->image.image_private = nullptr;

    return BM_SUCCESS;
}

bm_status_t bmcv_width_align(bm_handle_t handle,
                             bm_image    input,
                             bm_image    output) {
    if (input.image_format == FORMAT_COMPRESSED) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_INFO,
                  "Compressed format not support stride align\n");
        return BM_SUCCESS;
    }

    if (input.width != output.width || input.height != output.height ||
        input.image_format != output.image_format ||
        input.data_type != output.data_type) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "input image and outuput image have different size or format "
                  " %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }

    if (!bm_image_is_attached(input)) {
        bmlib_log(BMCV_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "input image not attache device memory  %s: %s: %d\n",
                  filename(__FILE__),
                  __func__,
                  __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (!bm_image_is_attached(output)) {
        if (BM_SUCCESS != bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY)) {
            BMCV_ERR_LOG("bm_image_alloc_dev_mem error\r\n");

            return BM_ERR_FAILURE;
        }
    }

    int             plane_num = bm_image_get_plane_num(input);
    bm_device_mem_t src_mem[4];
    bm_device_mem_t dst_mem[4];
    bm_image_get_device_mem(input, src_mem);
    bm_image_get_device_mem(output, dst_mem);
    for (int p = 0; p < plane_num; p++) {
        layout::update_memory_layout(handle,
                                     src_mem[p],
                                     input.image_private->memory_layout[p],
                                     dst_mem[p],
                                     output.image_private->memory_layout[p]);
    }

    return BM_SUCCESS;
}

bm_status_t bm_image_alloc_contiguous_mem(int       image_num,
                                          bm_image *images,
                                          int       heap_id) {
    if (0 == image_num) {
        BMCV_ERR_LOG("image_num can not be set as 0\n");
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < image_num - 1; i++) {
        for (int idx = 0; idx < images[i].image_private->plane_num; idx++) {
            if (images[i].image_private->memory_layout[idx].size !=
                images[i + 1].image_private->memory_layout[idx].size) {
                BMCV_ERR_LOG("all image must have same size\n");
                return BM_ERR_FAILURE;
            }
        }
        if (images[i].image_private->attached) {
            BMCV_ERR_LOG("image has been attached memory\n");
            return BM_ERR_FAILURE;
        }
    }
    int single_image_sz = 0;
    for (int idx = 0; idx < images[0].image_private->plane_num; idx++) {
        single_image_sz += images[0].image_private->memory_layout[idx].size;
    }
    int             total_image_size = image_num * single_image_sz;
    bm_device_mem_t dmem;
    if (heap_id != BMCV_HEAP_ANY) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte_heap(images[0].image_private->handle,
                                       &dmem,
                                       heap_id,
                                       total_image_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte_heap error\r\n");

            return BM_ERR_FAILURE;
        }
    } else {
        if (BM_SUCCESS != bm_malloc_device_byte(images[0].image_private->handle,
                                                &dmem,
                                                total_image_size)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

            return BM_ERR_FAILURE;
        }
    }
    int           dmabuf_fd = dmem.u.device.dmabuf_fd;
    #ifdef __linux__
    unsigned long base_addr = dmem.u.device.device_addr;
    #else
    unsigned long long base_addr = dmem.u.device.device_addr;
    #endif
    for (int i = 0; i < image_num; i++) {
        std::lock_guard<std::mutex> lock(images[i].image_private->memory_lock);
        bm_device_mem_t             tmp_dmem[16];
        memset(tmp_dmem, 0x0, sizeof(tmp_dmem));
        for (int idx = 0; idx < images[i].image_private->plane_num; idx++) {
            tmp_dmem[idx] = bm_mem_from_device(
                base_addr, images[i].image_private->memory_layout[idx].size);
            tmp_dmem[idx].flags.u.gmem_heapid  = dmem.flags.u.gmem_heapid;
            tmp_dmem[idx].u.device.dmabuf_fd = dmabuf_fd;
            base_addr += images[i].image_private->memory_layout[idx].size;
        }
        bm_image_attach(images[i], tmp_dmem);
    }

    return BM_SUCCESS;
}

bm_status_t bm_image_alloc_contiguous_mem_heap_mask(int       image_num,
                                                    bm_image *images,
                                                    int       heap_mask) {
    if (0 == image_num) {
        BMCV_ERR_LOG("image_num can not be set as 0\n");
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < image_num - 1; i++) {
        for (int idx = 0; idx < images[i].image_private->plane_num; idx++) {
            if (images[i].image_private->memory_layout[idx].size !=
                images[i + 1].image_private->memory_layout[idx].size) {
                BMCV_ERR_LOG("all image must have same size\n");
                return BM_ERR_FAILURE;
            }
        }
        if (images[i].image_private->attached) {
            BMCV_ERR_LOG("image has been attached memory\n");
            return BM_ERR_FAILURE;
        }
    }
    int single_image_sz = 0;
    for (int idx = 0; idx < images[0].image_private->plane_num; idx++) {
        single_image_sz += images[0].image_private->memory_layout[idx].size;
    }
    int             total_image_size = image_num * single_image_sz;
    bm_device_mem_t dmem;
    if (BM_SUCCESS !=
        bm_malloc_device_byte_heap_mask(images[0].image_private->handle,
                                   &dmem,
                                   heap_mask,
                                   total_image_size)) {
        BMCV_ERR_LOG("bm_malloc_device_byte_heap error\r\n");

        return BM_ERR_FAILURE;
    }
    int           dmabuf_fd = dmem.u.device.dmabuf_fd;
    #ifdef __linux__
    unsigned long base_addr = dmem.u.device.device_addr;
    #else
    unsigned long long base_addr = dmem.u.device.device_addr;
    #endif
    for (int i = 0; i < image_num; i++) {
        std::lock_guard<std::mutex> lock(images[i].image_private->memory_lock);
        bm_device_mem_t             tmp_dmem[16];
        memset(tmp_dmem, 0x0, sizeof(tmp_dmem));
        for (int idx = 0; idx < images[i].image_private->plane_num; idx++) {
            tmp_dmem[idx] = bm_mem_from_device(
                base_addr, images[i].image_private->memory_layout[idx].size);
            tmp_dmem[idx].flags.u.gmem_heapid = dmem.flags.u.gmem_heapid;
            tmp_dmem[idx].u.device.dmabuf_fd = dmabuf_fd;
            base_addr += images[i].image_private->memory_layout[idx].size;
        }
        bm_image_attach(images[i], tmp_dmem);
    }

    return BM_SUCCESS;
}

bm_status_t bm_image_free_contiguous_mem(int image_num, bm_image *images) {
    if (0 == image_num) {
        BMCV_ERR_LOG("[FREE]image_num can not be set as 0\n");
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < image_num; i++) {
        if (images[i].image_private->data_owned) {
            return BM_ERR_FAILURE;
        }
        bm_image_detach(images[i]);
    }
    bm_device_mem_t dmem[16];
    bm_image_get_device_mem(images[0], dmem);
    int dmabuf_fd       = dmem->u.device.dmabuf_fd;
    u64 base_addr       = bm_mem_get_device_addr(dmem[0]);
    int single_image_sz = 0;
    for (int idx = 0; idx < images[0].image_private->plane_num; idx++) {
        single_image_sz += images[0].image_private->memory_layout[idx].size;
    }
    int             total_image_size = image_num * single_image_sz;
    bm_device_mem_t release_dmem;
    release_dmem = bm_mem_from_device(base_addr, total_image_size);
    release_dmem.u.device.dmabuf_fd = dmabuf_fd;
    release_dmem.flags.u.gmem_heapid = dmem->flags.u.gmem_heapid;
    bm_free_device(images[0].image_private->handle, release_dmem);

    return BM_SUCCESS;
}

bm_status_t bm_image_attach_contiguous_mem(int             image_num,
                                           bm_image *      images,
                                           bm_device_mem_t dmem) {
    if (0 == image_num) {
        BMCV_ERR_LOG("[ALLOC]image_num can not be set as 0\n");
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < image_num - 1; i++) {
        for (int idx = 0; idx < images[i].image_private->plane_num; idx++) {
            if (images[i].image_private->memory_layout[idx].size !=
                images[i + 1].image_private->memory_layout[idx].size) {
                BMCV_ERR_LOG("all image must have same size\n");
                return BM_ERR_FAILURE;
            }
        }
    }
    #ifdef __linux__
    unsigned long base_addr = dmem.u.device.device_addr;
    #else
    unsigned long long base_addr = dmem.u.device.device_addr;
    #endif
    int           dmabuf_fd = dmem.u.device.dmabuf_fd;
    for (int i = 0; i < image_num; i++) {
        std::lock_guard<std::mutex> lock(images[i].image_private->memory_lock);
        bm_device_mem_t             tmp_dmem[16];
        memset(tmp_dmem, 0x0, sizeof(tmp_dmem));
        for (int idx = 0; idx < images[i].image_private->plane_num; idx++) {
            tmp_dmem[idx] = bm_mem_from_device(
                base_addr, images[i].image_private->memory_layout[idx].size);
            tmp_dmem[idx].u.device.dmabuf_fd = dmabuf_fd;
            base_addr += images[i].image_private->memory_layout[idx].size;
        }
        if (images[i].image_private->attached) {
            bm_image_detach(images[i]);
        }
        bm_image_attach(images[i], tmp_dmem);
    }

    return BM_SUCCESS;
}

bm_status_t bm_image_dettach_contiguous_mem(int image_num, bm_image *images) {
    for (int i = 0; i < image_num; i++) {
        if (images[i].image_private->data_owned) {
            BMCV_ERR_LOG("image mem can not be free\n");
            return BM_ERR_FAILURE;
        }
        bm_image_detach(images[i]);
    }

    return BM_SUCCESS;
}

bm_status_t bm_image_get_contiguous_device_mem(int              image_num,
                                               bm_image *       images,
                                               bm_device_mem_t *mem) {
    if (0 == image_num) {
        BMCV_ERR_LOG("image_num can not be set as 0\n");
        return BM_ERR_FAILURE;
    }
    for (int i = 0; i < image_num - 1; i++) {
        for (int idx = 0; idx < images[i].image_private->plane_num; idx++) {
            if (images[i].image_private->memory_layout[idx].size !=
                images[i + 1].image_private->memory_layout[idx].size) {
                BMCV_ERR_LOG("all image must have same size\n");
                return BM_ERR_FAILURE;
            }
        }
        if (!(images[i].image_private->attached)) {
            BMCV_ERR_LOG("image has not been attached memory\n");
            return BM_ERR_FAILURE;
        }
    }
    bm_device_mem_t dmem;
    bm_image_get_device_mem(images[0], &dmem);
    int dmabuf_fd       = dmem.u.device.dmabuf_fd;
    u64 base_addr       = bm_mem_get_device_addr(dmem);
    int single_image_sz = 0;
    for (int idx = 0; idx < images[0].image_private->plane_num; idx++) {
        single_image_sz += images[0].image_private->memory_layout[idx].size;
    }
    int total_image_size = image_num * single_image_sz;
    for (int i = 1; i < image_num; i++) {
        bm_device_mem_t tmp_dmem;
        bm_image_get_device_mem(images[i], &tmp_dmem);
        u64 tmp_addr = bm_mem_get_device_addr(tmp_dmem);
        if ((base_addr + i * single_image_sz) != tmp_addr) {
            BMCV_ERR_LOG("images should have continuous mem\r\n");

            return BM_ERR_FAILURE;
        }
    }
    bm_device_mem_t out_dmem;
    out_dmem = bm_mem_from_device(base_addr, total_image_size);
    out_dmem.u.device.dmabuf_fd = dmabuf_fd;
    memcpy(mem, &out_dmem, sizeof(bm_device_mem_t));

    return BM_SUCCESS;
}

bm_status_t bm_image_to_bmcv_image(bm_image *src, bmcv_image *dst)
{
    if (!src->image_private)
        return BM_ERR_FAILURE;
    if (src->image_private->attached == false) {
        printf("please attach device memory first!\n");
        return BM_ERR_FAILURE;
    }

    dst->color_space = color_space_convert(src->image_format);
    dst->data_format = bmcv_data_format_convert(src->data_type);
    dst->image_format = bmcv_image_format_convert(src->image_format);
    dst->image_width = src->width;
    dst->image_height = src->height;
    for (int i = 0; i < src->image_private->plane_num; i++) {
        dst->stride[i] = src->image_private->memory_layout[i].pitch_stride;
        dst->data[i] = src->image_private->data[i];
    }
    return BM_SUCCESS;
}

#define DEV_NUM_MAX 64
using namespace std;
static vector<int> process_id(DEV_NUM_MAX, -1);
static vector<int> ref_cnt(DEV_NUM_MAX, 0);
static vector<vector<pair<u64, u64>>> addr_map(DEV_NUM_MAX);
mutex id_lock;
mutex cnt_lock;
mutex map_lock;

static inline void set_cpu_process_id(int dev_id, int id) {
    lock_guard<mutex> lock(id_lock);
    process_id[dev_id] = id;
}

static inline int get_ref_cnt(int dev_id) {
    lock_guard<mutex> lock(cnt_lock);
    return ref_cnt[dev_id];
}

static inline void increase_ref_cnt(int dev_id) {
    lock_guard<mutex> lock(cnt_lock);
    ++ref_cnt[dev_id];
}

static inline void decrease_ref_cnt(int dev_id) {
    lock_guard<mutex> lock(cnt_lock);
    --ref_cnt[dev_id];
}

static inline u64 get_map_paddr(int dev_id, int heap_id) {
    lock_guard<mutex> lock(map_lock);
    return addr_map[dev_id][heap_id].first;
}

static inline u64 get_map_vaddr(int dev_id, int heap_id) {
    lock_guard<mutex> lock(map_lock);
    return addr_map[dev_id][heap_id].second;
}

static inline void set_map_paddr(int dev_id, int heap_id, u64 addr) {
    lock_guard<mutex> lock(map_lock);
    addr_map[dev_id][heap_id].first = addr;
}

static inline void set_map_vaddr(int dev_id, int heap_id, u64 addr) {
    lock_guard<mutex> lock(map_lock);
    addr_map[dev_id][heap_id].second = addr;
}

static inline int get_map_heap_size(int dev_id) {
    lock_guard<mutex> lock(map_lock);
    return addr_map[dev_id].size();
}

static inline void set_map_heap_size(int dev_id, int size) {
    lock_guard<mutex> lock(map_lock);
    addr_map[dev_id].resize(size);
}

static bm_status_t map_addr(bm_handle_t handle, int process_id) {
    int timeout = -1;
    int dev_id = bm_get_devid(handle);
    unsigned int heap_num;
    if (BM_SUCCESS != bm_get_gmem_total_heap_num(handle, &heap_num)) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "get_heap num failed!\r\n");
        return BM_ERR_FAILURE;
    }
    set_map_heap_size(dev_id, heap_num);
    for (unsigned int i = 0; i < heap_num; i++) {
        bm_heap_stat_byte_t heap_info;
        if (BM_SUCCESS != bm_get_gmem_heap_stat_byte_by_id(handle, &heap_info, i)) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR, "get_heap info failed!\r\n");
            return BM_ERR_FAILURE;
        }
        u64 vaddr = (u64)bmcpu_map_phys_addr(
                handle,
                process_id,
                (void*)(heap_info.mem_start_addr),
                heap_info.mem_total,
                timeout);
        set_map_paddr(dev_id, i, heap_info.mem_start_addr);
        set_map_vaddr(dev_id, i, vaddr);
    }
    return BM_SUCCESS;
}

int get_cpu_process_id(bm_handle_t handle) {
    int dev_id = bm_get_devid(handle);
    lock_guard<mutex> lock(id_lock);
    return process_id[dev_id];
}

u64 get_mapped_addr(bm_handle_t handle, bm_device_mem_t* mem) {
    int dev_id = bm_get_devid(handle);
    unsigned int heap_id;
    bm_get_gmem_heap_id(handle, mem, &heap_id);
    u64 paddr = bm_mem_get_device_addr(*mem);
    u64 vaddr = paddr - get_map_paddr(dev_id, heap_id)
                      + get_map_vaddr(dev_id, heap_id);
    return vaddr;
}

bm_status_t bmcv_open_cpu_process(bm_handle_t handle) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)
    int dev_id = bm_get_devid(handle);
    if (get_cpu_process_id(handle) != -1) return BM_SUCCESS;
    int timeout = -1;
    char* lib_path = getenv("BMCV_CPU_LIB_PATH");
    char* kernel_path = getenv("BMCV_CPU_KERNEL_PATH");
    if (lib_path == NULL) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "Please set environment variable: BMCV_CPU_LIB_PATH!\r\n");
        return BM_ERR_FAILURE;
    }
    if (kernel_path == NULL) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "Please set environment variable: BMCV_CPU_KERNEL_PATH!\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bmcpu_start_cpu(
                handle,
            #ifdef __linux__
                (char*)(std::string(kernel_path) + std::string("/fip.bin")).c_str(),
                (char*)(std::string(kernel_path) + std::string("/ramboot_rootfs.itb")).c_str())) {
            #else
                (char *)(std::string(kernel_path) + std::string("\\fip.bin")).c_str(),
                (char *)(std::string(kernel_path) + std::string("\\ramboot_rootfs.itb")).c_str())) {
            #endif
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "start cpu failed!\r\n");
        return BM_ERR_FAILURE;
    }
    int cpu_id = bmcpu_open_process(handle, 0, timeout);
    set_cpu_process_id(dev_id, cpu_id);
    if (BM_SUCCESS != bmcpu_load_library(
                handle,
                cpu_id,
        #ifdef __linux__
                (char*)(std::string(lib_path) + std::string("/libbmcv_cpu_func.so")).c_str(),
        #else
                (char *)(std::string(lib_path) + std::string("\\libbmcv_cpu_func.so")).c_str(),
        #endif
                timeout)) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "load library failed!\r\n");
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != map_addr(handle, cpu_id)) {
        bmlib_log("BMCV", BMLIB_LOG_ERROR, "map addr failed!\r\n");
        return BM_ERR_FAILURE;
    }
    increase_ref_cnt(dev_id);
#else
    UNUSED(handle);
    bmlib_log("BMCV", BMLIB_LOG_WARNING, "NOT support CPU process!\r\n");
#endif
    return BM_SUCCESS;
}

bm_status_t bmcv_close_cpu_process(bm_handle_t handle) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)
    int dev_id = bm_get_devid(handle);
    if (get_ref_cnt(dev_id) > 1) {
        decrease_ref_cnt(dev_id);
        return BM_SUCCESS;
    }
    int timeout = -1;
    int cpu_id = get_cpu_process_id(handle);
    // unmap
    bm_status_t ret = BM_SUCCESS;
    for (int i = 0; i < get_map_heap_size(dev_id); i++) {
        ret = bmcpu_unmap_phys_addr(handle,
                                    cpu_id,
                                    (void*)(get_map_paddr(dev_id, i)),
                                    timeout);
        if (BM_SUCCESS != ret) {
            bmlib_log("BMCV", BMLIB_LOG_ERROR, "unmap cpu failed!\r\n");
            return BM_ERR_FAILURE;
        }
    }
    ret = bmcpu_close_process(handle, cpu_id, timeout);
    if (BM_SUCCESS != ret) {
        bmlib_log("BMCV", BMLIB_LOG_WARNING, "close process failed!\r\n");
        return BM_ERR_FAILURE;
    }
    set_cpu_process_id(dev_id, -1);
#else
    UNUSED(handle);
    bmlib_log("BMCV", BMLIB_LOG_WARNING, "NOT support CPU process!\r\n");
#endif
    return BM_SUCCESS;
}


