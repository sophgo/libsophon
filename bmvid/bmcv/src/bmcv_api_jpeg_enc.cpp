//
// Created by nan.wu on 2019-08-22.
//
#ifndef USING_CMODEL

#include "bmcv_api_ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "bmcv_internal.h"

#ifdef __linux__
#include <unistd.h>
#else
#include <windows.h>
#endif

#define JPU_ENCODE_MAX_RETRY_COUNT 3

typedef struct bmcv_jpeg_encoder_struct {
    BmJpuJPEGEncoder *encoder_;
} bmcv_jpeg_encoder_t;

typedef struct bmcv_jpeg_encoder_output_buffer_struct {
    void    *buffer;
    size_t  buffer_size;
    void    *opaque;   // reserved for extension
    int     bs_in_device;
    int     soc_idx;
} bmcv_jpeg_buffer_t;

static void* acquire_output_buffer(void *context, size_t size, void **acquired_handle)
{
    bmcv_jpeg_buffer_t *p_encoded = (bmcv_jpeg_buffer_t *)context;

    if (p_encoded == NULL) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_WARNING,
            "User NOT malloc memory for output bitstream, it will malloc memory automatically. "\
            "But user need remember to free it.\n");
        void *mem;
        mem = malloc(size);
        *acquired_handle = mem;
        return mem;
    } else if (p_encoded->bs_in_device == 1) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_INFO, "acquire_output_buffer: bs_in_device = 1\n");
        bm_handle_t handle;
        bm_dev_request(&handle, p_encoded->soc_idx);
        bm_device_mem_t *bs_device_mem = (bm_device_mem_t *)malloc(sizeof(bm_device_mem_t));
        //only vpp
        bm_malloc_device_byte_heap(handle, bs_device_mem, 1, size);
        *acquired_handle = bs_device_mem;
        bm_dev_free(handle);
        return (void *)bs_device_mem;
    } else {
        if (p_encoded->buffer == NULL) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_WARNING,
                    "encoded output buffer is null, allocate it.\n");
            p_encoded->buffer = malloc(size);
            p_encoded->buffer_size = size;
        }

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


static bm_status_t bmcv_jpeg_enc_check(bm_image *src, int image_num, int quality_factor) {
    if (NULL == src) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                  "bm_image is nullptr %s: %s: %d\n",
                   filename(filename(__FILE__)), __func__, __LINE__);
        return BM_ERR_DATA;
    }

    if(image_num < 1 || image_num > 4){
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                  "image_num(%d) should be between 1 and 4 %s: %s: %d\n",
                   image_num, filename(filename(__FILE__)), __func__, __LINE__);
        return BM_ERR_PARAM;
    }

    if (quality_factor < 0 || quality_factor > 100) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                  "quality_factor(%d) should be between 0 and 100 %s: %s: %d\n",
                   quality_factor, filename(filename(__FILE__)), __func__, __LINE__);
        return BM_ERR_PARAM;
    }

    for (int i = 0; i < image_num; i++) {
        if (!bm_image_is_attached(src[i])) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "input image is not attached data %s: %s: %d\n",
                       filename(filename(__FILE__)), __func__, __LINE__);
            return BM_ERR_DATA;
        }

        if (src[i].data_type != DATA_TYPE_EXT_1N_BYTE) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "data type only support 1N_BYTE %s: %s: %d\n",
                       filename(filename(__FILE__)), __func__, __LINE__);
            return BM_ERR_DATA;
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
                case FORMAT_BAYER: {
                    printf(" ---> FORMAT_BAYER\n\n");
                    break;
                }
                case FORMAT_BAYER_RG8: {
                    printf(" ---> FORMAT_BAYER_RG8\n\n");
                    break;
                }
                default:
                    printf(" ---> UNKNOWN FORMAT\n\n");
                    break;
            }

            return BM_ERR_DATA;
        }
        // width and height should >= 16
        if ((src[i].width < 16) || src[i].height < 16) {
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR,
                      "width and height should not less than 16\n",
                      filename(filename(__FILE__)), __func__, __LINE__);
            return BM_ERR_DATA;
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
    // buffer_size = buffer_size > 16368 * 1024 ? 16368 * 1024 : buffer_size;

    ret = bm_jpu_enc_load(devid);
    if (BM_JPU_ENC_RETURN_CODE_OK != ret) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "load jpeg encoder failed!\r\n");
        goto End;
    }

    ret = bm_jpu_jpeg_enc_open(&enc->encoder_, 0, buffer_size, devid);
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
                                    int                  quality_factor,
                                    int                  bs_in_device){
    BmJpuEncReturnCodes ret;
    BmJpuJPEGEncParams enc_params;
    BmJpuFramebuffer framebuffer;
    bm_device_mem_t wrapped_mem;

    bm_image_format_info_t info;
    bm_image_get_format_info(src, &info);
    int width = src->width;
    int height = src->height;
    BmJpuImageFormat image_format;

    int y_size = info.stride[0] * height;
    int c_size = 0;
    int total_size = 0;
    int count = 0;

    int src_image_format = src->image_format;
    bmcv_jpeg_buffer_t encoded_buffer;

    encoded_buffer.buffer = *p_jpeg_data;
    encoded_buffer.buffer_size = *out_size;
    encoded_buffer.opaque = NULL;
    encoded_buffer.soc_idx = 0;
    encoded_buffer.bs_in_device = bs_in_device;
    if(bs_in_device == 1){
        encoded_buffer.soc_idx = bm_get_devid(bm_image_get_handle(src));
    }

    switch (src_image_format)
    {
        case FORMAT_YUV420P:
            image_format = BM_JPU_IMAGE_FORMAT_YUV420P;
            c_size = info.stride[1] * ((height + 1) / 2);
            total_size = y_size + c_size * 2;
            break;
        case FORMAT_NV12:
            image_format = BM_JPU_IMAGE_FORMAT_NV12;
            c_size = info.stride[1] * ((height + 1) / 2);
            total_size = y_size + c_size;
            break;
        case FORMAT_NV21:
            image_format = BM_JPU_IMAGE_FORMAT_NV21;
            c_size = info.stride[1] * ((height + 1) / 2);
            total_size = y_size + c_size;
            break;
        case FORMAT_YUV422P:
            image_format = BM_JPU_IMAGE_FORMAT_YUV422P;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size * 2;
            break;
        case FORMAT_NV16:
            image_format = BM_JPU_IMAGE_FORMAT_NV16;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size;
            break;
        case FORMAT_NV61:
            image_format = BM_JPU_IMAGE_FORMAT_NV61;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size;
            break;
        case FORMAT_YUV444P:
            image_format = BM_JPU_IMAGE_FORMAT_YUV444P;
            c_size = info.stride[1] * height;
            total_size = y_size + c_size * 2;
            break;
        case FORMAT_GRAY:
            image_format = BM_JPU_IMAGE_FORMAT_GRAY;
            c_size = 0;
            total_size = y_size;
            break;
        default:
            image_format = BM_JPU_IMAGE_FORMAT_GRAY;
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

    wrapped_mem = bm_mem_from_device(dev_addr[0], total_size);

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
    enc_params.image_format   = image_format;
    enc_params.output_buffer_context = (void*)&encoded_buffer;

    enc_params.acquire_output_buffer = acquire_output_buffer;
    enc_params.finish_output_buffer  = finish_output_buffer;
    enc_params.bs_in_device          = bs_in_device;


    /* Do the actual encoding */
    for(count=0; count<JPU_ENCODE_MAX_RETRY_COUNT; count++)
    {
        ret = bm_jpu_jpeg_enc_encode(jpeg_enc->encoder_,
                                     &framebuffer,
                                     &enc_params,
                                     p_jpeg_data,
                                     out_size);
        uint8_t* jpeg_virt_addr = (uint8_t*)*p_jpeg_data;
        if(jpeg_virt_addr[0] != 0xff || jpeg_virt_addr[1] != 0xd8 || jpeg_virt_addr[2] != 0xff)
        {
            bm_jpu_hw_reset();
#ifdef __linux__
            usleep(20*1000);
#else
            Sleep(20);
#endif
            bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "wrong jpeg header, hw reset and retry count: %d, max count: %d\r\n", count+1, JPU_ENCODE_MAX_RETRY_COUNT);
            continue;
        } else {
            break;
        }
    }

    if(count == JPU_ENCODE_MAX_RETRY_COUNT)
    {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "jpeg encode header is wrong\r\n");
        return BM_ERR_FAILURE;
    }

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
                                int          quality_factor,
                                int          bs_in_device) {

    bm_status_t ret;
    ret = bmcv_jpeg_enc_check(src, image_num, quality_factor);
    if(BM_SUCCESS != ret) {
        return ret;
    }
    bm_handle_check_1(handle, src[0]);
    if (handle == NULL) {
        bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_DEVNOTREADY;
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
                    //if(bs_in_device == 0)
                    free(p_jpeg_data[j]);
                    free(out_size);
                }
                bmcv_jpeg_encoder_destroy(jpeg_enc);
                return ret;
            }
            ret = bmcv_width_align(handle, src[i], tmp);
            if (ret != BM_SUCCESS) {
                bmlib_log("JPEG-ENC", BMLIB_LOG_ERROR, "internal width align failed!\n");
                for (int j = 0; j < i; j++) {
                    //if(bs_in_device == 0)
                    free(p_jpeg_data[j]);
                    free(out_size);
                }
                bm_image_destroy(tmp);
                bmcv_jpeg_encoder_destroy(jpeg_enc);
                return ret;
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
        ret = bmcv_jpeg_enc_one_image(jpeg_enc, src_align, &p_jpeg_data[i], &(out_size[i]), quality_factor,bs_in_device);
        if (!src_is_align) {
            bm_image_destroy(*src_align);
        }
        if (ret != BM_SUCCESS) {
            for (int j = 0; j < i; j++) {
                //if(bs_in_device == 0)
                free(p_jpeg_data[j]);
                free(out_size);
            }
            bmcv_jpeg_encoder_destroy(jpeg_enc);
            return ret;
        }
    }
    bmcv_jpeg_encoder_destroy(jpeg_enc);
    return BM_SUCCESS;
}

#endif
