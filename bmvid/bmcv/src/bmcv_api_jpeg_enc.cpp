//
// Created by nan.wu on 2019-08-22.
//
#ifndef USING_CMODEL

#include "bmcv_api_ext.h"
#include "bmjpuapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "bmjpuapi_jpeg.h"
#include "bmcv_internal.h"

typedef struct bmcv_jpeg_encoder_struct {
    BmJpuJPEGEncoder *encoder_;
} bmcv_jpeg_encoder_t;

typedef struct bmcv_jpeg_encoder_output_buffer_struct {
    void    *buffer;
    size_t  buffer_size;
    void    *opaque;   // reserved for extension
} bmcv_jpeg_buffer_t;

static void* acquire_output_buffer(void *context, size_t size, void **acquired_handle)
{
    bmcv_jpeg_buffer_t *p_encoded = (bmcv_jpeg_buffer_t *)context;

    if (context == NULL || p_encoded->buffer == NULL) {
/*
        bmlib_log("JPEG-ENC", BMLIB_LOG_WARNING,
            "User NOT malloc memory for output bitstream, it will malloc memory automatically. "\
            "But user need remember to free it.\n");
*/
        void *mem;
        mem = malloc(size);
        *acquired_handle = mem;
        return mem;
    } else {
        if (p_encoded->buffer_size < size){
            bmlib_log("JPEG-ENC", BMLIB_LOG_WARNING,
                    "Pre-allocated encoded output buffer is less than encoded JPEG size! Reallocate it.\n");
            p_encoded->buffer = realloc(p_encoded->buffer, size);
            p_encoded->buffer_size = size;
        }

        *acquired_handle = p_encoded->buffer;
        return *acquired_handle;
    }
}

static void finish_output_buffer(void *context, void *acquired_handle)
{
    ((void)(context));
    ((void)(acquired_handle));
}


static bm_status_t bmcv_jpeg_enc_check(bm_image *src,
                                       int image_num) {
    if (NULL == src) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                  "bm_image is nullptr %s: %s: %d\n",
                   filename(filename(__FILE__)), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    for (int i = 0; i < image_num; i++) {
        if (!bm_image_is_attached(src[i])) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "input image is not attached data %s: %s: %d\n",
                       filename(filename(__FILE__)), __func__, __LINE__);
            return BM_ERR_PARAM;
        }

        if (src[i].data_type != DATA_TYPE_EXT_1N_BYTE) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "data type only support 1N_BYTE %s: %s: %d\n",
                       filename(filename(__FILE__)), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }

        // only support yuv420p, nv12, nv21, yuv422p, nv16, nv61, yuv444p, gray.
        if (!((src[i].image_format == FORMAT_YUV420P) ||
              (src[i].image_format == FORMAT_NV12) ||
              (src[i].image_format == FORMAT_NV21) ||
              (src[i].image_format == FORMAT_YUV422P) ||
              (src[i].image_format == FORMAT_NV16) ||
              (src[i].image_format == FORMAT_NV61) ||
              (src[i].image_format == FORMAT_YUV444P) ||
              (src[i].image_format == FORMAT_GRAY))) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "not support this image format index=%d: image_format=%d\n",
                      i, src[i].image_format);

            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "not support this image format %s: %s: %d\n",
                      filename(filename(__FILE__)), __func__, __LINE__);

            switch (src[i].image_format) {
                case FORMAT_YUV420P: {
                    printf(" ---> FORMAT_YUV420P\n\n");
                    break;
                }
                case FORMAT_YUV422P: {
                    printf(" ---> FORMAT_YUV422P\n\n");
                    break;
                }
                case FORMAT_YUV444P: {
                    printf(" ---> FORMAT_YUV444P\n\n");
                    break;
                }
                case FORMAT_NV24: {
                    printf(" ---> FORMAT_NV24\n\n");
                    break;
                }
                case FORMAT_NV12: {
                    printf(" ---> FORMAT_NV12\n\n");
                    break;
                }
                case FORMAT_NV21: {
                    printf(" ---> FORMAT_NV21\n\n");
                    break;
                }
                case FORMAT_NV16: {
                    printf(" ---> FORMAT_NV16\n\n");
                    break;
                }
                case FORMAT_NV61: {
                    printf(" ---> FORMAT_NV61\n\n");
                    break;
                }
                case FORMAT_GRAY: {
                    printf(" ---> FORMAT_GRAY\n\n");
                    break;
                }
                case FORMAT_COMPRESSED: {
                    printf(" ---> FORMAT_COMPRESSED\n\n");
                    break;
                }
                case FORMAT_BGR_PACKED: {
                    printf(" ---> FORMAT_BGR_PACKED\n\n");
                    break;
                }
                case FORMAT_RGB_PACKED: {
                    printf(" ---> FORMAT_RGB_PACKED\n\n");
                    break;
                }
                case FORMAT_ABGR_PACKED: {
                    printf(" ---> FORMAT_ABGR_PACKED\n\n");
                    break;
                }
                case FORMAT_ARGB_PACKED: {
                    printf(" ---> FORMAT_ARGB_PACKED\n\n");
                    break;
                }
                case FORMAT_BGR_PLANAR: {
                    printf(" ---> FORMAT_BGR_PLANAR\n\n");
                    break;
                }
                case FORMAT_RGB_PLANAR: {
                    printf(" ---> FORMAT_RGB_PLANAR\n\n");
                    break;
                }
                case FORMAT_BGRP_SEPARATE: {
                    printf(" ---> FORMAT_BGRP_SEPARATE\n\n");
                    break;
                }
                case FORMAT_RGBP_SEPARATE: {
                    printf(" ---> FORMAT_RGBP_SEPARATE\n\n");
                    break;
                }
                case FORMAT_HSV_PLANAR: {
                    printf(" ---> FORMAT_HSV_PLANAR\n\n");
                    break;
                }
                case FORMAT_YUV444_PACKED: {
                    printf(" ---> FORMAT_YUV444_PACKED\n\n");
                    break;
                }
                case FORMAT_YVU444_PACKED: {
                    printf(" ---> FORMAT_YVU444_PACKED\n\n");
                    break;
                }
                case FORMAT_YUV422_YUYV: {
                    printf(" ---> FORMAT_YUV422_YUYV\n\n");
                    break;
                }
                case FORMAT_YUV422_YVYU: {
                    printf(" ---> FORMAT_YUV422_YVYU\n\n");
                    break;
                }
                case FORMAT_YUV422_UYVY: {
                    printf(" ---> FORMAT_YUV422_UYVY\n\n");
                    break;
                }
                case FORMAT_YUV422_VYUY: {
                    printf(" ---> FORMAT_YUV422_VYUY\n\n");
                    break;
                }
                case FORMAT_RGBYP_PLANAR: {
                    printf(" ---> FORMAT_RGBYP_PLANAR\n\n");
                    break;
                }
                case FORMAT_HSV180_PACKED: {
                    printf(" ---> FORMAT_HSV180_PACKED\n\n");
                    break;
                }
                case FORMAT_HSV256_PACKED: {
                    printf(" ---> FORMAT_HSV256_PACKED\n\n");
                    break;
                }
            }

            return BM_NOT_SUPPORTED;
        }
        // width and height should >= 16
        if ((src[i].width < 16) || src[i].height < 16) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "width and height should not less than 16\n",
                      filename(filename(__FILE__)), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
        }
    }
    return BM_SUCCESS;
}

static bool is_align(bm_image src) {
    // stride should be align to 16 or 8
    int bmcv_stride[3];
    int plane_num = bm_image_get_plane_num(src);
    bm_image_get_stride(src, bmcv_stride);
    int y_align = 0;
    int uv_align = 0;
    switch (src.image_format)
    {
        case FORMAT_YUV420P:
        case FORMAT_YUV422P:
            y_align = 16;
            uv_align = 8;
            break;
        case FORMAT_YUV444P:
        case FORMAT_GRAY:
            y_align = 8;
            uv_align = 8;
            break;
        case FORMAT_NV12:
        case FORMAT_NV21:
        case FORMAT_NV16:
        case FORMAT_NV61:
            y_align = 16;
            uv_align = 16;
            break;
        default:
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "not support this image format");
    }
    if (bmcv_stride[0] % y_align != 0) {
        return false;
    }
    for (int i = 1; i < plane_num; i++) {
        if (bmcv_stride[i] % uv_align != 0) {
            return false;
        }
    }
    return true;
}

static int bmcv_jpeg_encoder_create(bmcv_jpeg_encoder_t** p_jpeg_encoder,
                                    bm_image* src,
                                    int quality_factor,
                                    int devid) {
    BmJpuEncReturnCodes ret;
    bmcv_jpeg_encoder_t *enc = NULL;
    enc = (bmcv_jpeg_encoder_t*)malloc(sizeof(bmcv_jpeg_encoder_t));

    ((void)(quality_factor));

    memset(enc, 0, sizeof(*enc));
    float factor = 1;
    switch (src->image_format) {
        case FORMAT_NV12:
        case FORMAT_NV21:
        case FORMAT_YUV420P: {
            factor = 1.5;
            break;
        }
        case FORMAT_NV61:
        case FORMAT_NV16:
        case FORMAT_YUV422P: {
            factor = 2;
            break;
        }
        case FORMAT_YUV444P: {
            factor = 3;
            break;
        }
        case FORMAT_GRAY: {
            factor = 1;
            break;
        }
        default:
            break;
    }
    int buffer_size = src->height * src->width * factor;
    buffer_size = buffer_size > 16368 * 1024 ? 16368 * 1024 : buffer_size;

    ret = bm_jpu_enc_load(devid);
    if (BM_JPU_ENC_RETURN_CODE_OK != ret) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "load jpeg encoder failed!\r\n");
        goto End;
    }

    ret = bm_jpu_jpeg_enc_open(&enc->encoder_, buffer_size, devid);
    if (BM_JPU_ENC_RETURN_CODE_OK != ret) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "open jpeg encoder failed!\r\n");
       goto End;
    }

    *p_jpeg_encoder = enc;
    return BM_JPU_ENC_RETURN_CODE_OK;

End:
    bm_jpu_enc_unload(devid);
    free(enc);
    return BM_JPU_ENC_RETURN_CODE_ERROR;
}



static int bmcv_jpeg_encoder_destroy(bmcv_jpeg_encoder_t *jpeg_encoder)
{
    if (jpeg_encoder) {
        bm_jpu_jpeg_enc_close(jpeg_encoder->encoder_);
    }
    free(jpeg_encoder);
    return 0;
}


bm_status_t bmcv_jpeg_enc_one_image(bmcv_jpeg_encoder_t  *jpeg_enc,
                                    bm_image*            src,
                                    void**               p_jpeg_data,
                                    size_t*              out_size,
                                    int                  quality_factor){
    BmJpuEncReturnCodes ret;
    BmJpuJPEGEncParams enc_params;
    BmJpuFramebuffer framebuffer;
    bm_device_mem_t wrapped_mem;

    bm_image_format_info_t info;
    bm_image_get_format_info(src, &info);
    int width = src->width;
    int height = src->height;
    BmJpuColorFormat out_pixformat;

    int y_size = info.stride[0] * height;
    int c_size = 0;
    int total_size = 0;
    int interleave = 0;
    int packed_format = 0;

    int src_image_format = src->image_format;
    bmcv_jpeg_buffer_t encoded_buffer;

    encoded_buffer.buffer = *p_jpeg_data;
    encoded_buffer.buffer_size = *out_size;
    encoded_buffer.opaque = NULL;
    switch (src_image_format)
    {
        case FORMAT_YUV420P:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV420;
            c_size = info.stride[1] * (height + 1) / 2;
            total_size = y_size + c_size * 2;
            interleave = 0;
            break;
        case FORMAT_NV12:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV420;
            c_size = info.stride[1] * (height + 1) / 2;
            total_size = y_size + c_size;
            interleave = 1;
            break;
        case FORMAT_NV21:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV420;
            c_size = info.stride[1] * (height + 1) / 2;
            total_size = y_size + c_size;
            interleave = 2;
            break;
        case FORMAT_YUV422P:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size * 2;
            interleave = 0;
            break;
        case FORMAT_NV16:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size;
            interleave = 1;
            break;
        case FORMAT_NV61:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size;
            interleave = 2;
            break;
        case FORMAT_YUV444P:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV444;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size * 2;
            interleave = 0;
            break;
        case FORMAT_GRAY:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV400;
            c_size = 0;
            total_size = y_size;
            break;
        default:
            out_pixformat = BM_JPU_COLOR_FORMAT_YUV400;
            c_size = 0;
            total_size = y_size;
            break;
    }

    /* The EXTERNAL DMA buffer filled with frame data */
    #ifdef __linux__
    unsigned long dev_addr[3] = {0, 0, 0};
    #else
    unsigned long long dev_addr[3] = {0, 0, 0};
    #endif
    for (int i = 0; i < info.plane_nb; i++) {
        dev_addr[i] = info.plane_data[i].u.device.device_addr;
    }
    
    wrapped_mem.u.device.device_addr = dev_addr[0];
    wrapped_mem.u.device.dmabuf_fd = 1;
    wrapped_mem.size = total_size;

    framebuffer.y_stride    = info.stride[0];
    framebuffer.cbcr_stride = info.stride[1];
    framebuffer.y_offset    = 0;
    framebuffer.cb_offset   = dev_addr[1] - dev_addr[0];
    framebuffer.cr_offset   = dev_addr[2] - dev_addr[0];
    
    framebuffer.dma_buffer = &wrapped_mem;

    /* Set up the encoding parameters */
    memset(&enc_params, 0, sizeof(enc_params));
    enc_params.frame_width    = width;
    enc_params.frame_height   = height;
    enc_params.quality_factor = quality_factor;
    enc_params.color_format   = out_pixformat;
    enc_params.packed_format  = packed_format;
    enc_params.chroma_interleave     = interleave;
    enc_params.output_buffer_context = (void*)&encoded_buffer;
    enc_params.acquire_output_buffer = acquire_output_buffer;
    enc_params.finish_output_buffer  = finish_output_buffer;


    /* Do the actual encoding */
    ret = bm_jpu_jpeg_enc_encode(jpeg_enc->encoder_,
                                     &framebuffer,
                                     &enc_params,
                                     p_jpeg_data,
                                     out_size);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "jpeg encode failed!\r\n");
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_image_jpeg_enc(bm_handle_t  handle,
                                int          image_num,
                                bm_image*    src,
                                void**       p_jpeg_data,
                                size_t*      out_size,
                                int          quality_factor) {
    if(BM_SUCCESS != bmcv_jpeg_enc_check(src, image_num)) {
        BMCV_ERR_LOG("bmcv_jpeg_enc_check error\r\n");
        return BM_ERR_FAILURE;
    }
    if (handle == NULL) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    int devid = bm_get_devid(handle);
    bmcv_jpeg_encoder_t *jpeg_enc = NULL;
    if(bmcv_jpeg_encoder_create(&jpeg_enc, src, quality_factor, devid) != BM_JPU_ENC_RETURN_CODE_OK) {
        return BM_ERR_FAILURE;
    }
    if (out_size == NULL) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "pointer of out size is NULL,  \
                   it should be equal to image_num * sizeof(size_t)!\r\n");
    }
    for (int i = 0; i < image_num; i++) {
        bm_status_t ret;
        bm_image* src_align = NULL;
        bm_image tmp;
        bool src_is_align = is_align(src[i]);
        if (src_is_align) {
            src_align = &(src[i]);
        } else {
            int bmcv_stride[3];
            bm_image_get_stride(src[i], bmcv_stride);
            bmcv_stride[0] = (bmcv_stride[0] + 15) / 16 * 16;
            bmcv_stride[1] = (bmcv_stride[1] + 15) / 16 * 16;
            bmcv_stride[2] = (bmcv_stride[2] + 15) / 16 * 16;
            ret = bm_image_create(handle, src[i].height, src[i].width, src[i].image_format, DATA_TYPE_EXT_1N_BYTE, &tmp, bmcv_stride);
            if (ret != BM_SUCCESS) {
                bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "internal creat bm_image failed!\n");
                for (int j = 0; j < i; j++) {
                    free(p_jpeg_data[j]);
                    free(out_size);
                }
                bmcv_jpeg_encoder_destroy(jpeg_enc);
                return BM_ERR_FAILURE;
            }
            ret = bmcv_width_align(handle, src[i], tmp);
            if (ret != BM_SUCCESS) {
                bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "internal width align failed!\n");
                for (int j = 0; j < i; j++) {
                    free(p_jpeg_data[j]);
                    free(out_size);
                }
                bm_image_destroy(tmp);
                bmcv_jpeg_encoder_destroy(jpeg_enc);
                return BM_ERR_FAILURE;
            }
            src_align = &tmp;
        }
#if 0   // for debug
        int image_byte_size[3] = {0};
        char* output_ptr = (char*)malloc(src[0].width * src[0].height * 3);
        bm_image_get_byte_size(*src_align, image_byte_size);
        printf("plane0 size: %d\n", image_byte_size[0]);
        printf("plane1 size: %d\n", image_byte_size[1]);
        printf("plane2 size: %d\n", image_byte_size[2]);
        int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2];
        for (int i = 0;i < 1; i++) {
            void* out_ptr[3] = {(void*)((char*)output_ptr + i * byte_size),
                                (void*)((char*)output_ptr + i * byte_size + image_byte_size[0]),
                                (void*)((char*)output_ptr + i * byte_size + image_byte_size[0] + image_byte_size[1])};
            bm_image_copy_device_to_host(*src_align, out_ptr);
        }
        char* filename = (char *)"dbgenc";
        FILE *fp = fopen(filename, "wb+");
        fwrite(output_ptr, byte_size, 1, fp);
        printf("save to %s %d bytes\n", filename, byte_size);
        fclose(fp);
        free(output_ptr);
#endif
        ret = bmcv_jpeg_enc_one_image(jpeg_enc, src_align, &p_jpeg_data[i], &(out_size[i]), quality_factor);
        if (!src_is_align) {
            bm_image_destroy(*src_align);
        }
        if (ret != BM_SUCCESS) {
            for (int j = 0; j < i; j++) {
                free(p_jpeg_data[j]);
                free(out_size);
            }
            bmcv_jpeg_encoder_destroy(jpeg_enc);
            return BM_ERR_FAILURE;
        }
    }
    bmcv_jpeg_encoder_destroy(jpeg_enc);
    return BM_SUCCESS;
}

#endif
