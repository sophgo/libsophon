/**
 * Copyright (C) 2018 Solan Shang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <string.h>
#include "jpeg_common.h"
#include "jpu_lib.h"

void logging_fn(BmJpuLogLevel level, char const *file, int const line, char const *fn, const char *format, ...)
{
    va_list args;

    char const *lvlstr = "";
    switch (level)
    {
        case BM_JPU_LOG_LEVEL_ERROR:   lvlstr = "ERROR";   break;
        case BM_JPU_LOG_LEVEL_WARNING: lvlstr = "WARNING"; break;
        case BM_JPU_LOG_LEVEL_INFO:    lvlstr = "INFO";    break;
        case BM_JPU_LOG_LEVEL_DEBUG:   lvlstr = "DEBUG";   break;
        case BM_JPU_LOG_LEVEL_TRACE:   lvlstr = "TRACE";   break;
        case BM_JPU_LOG_LEVEL_LOG:     lvlstr = "LOG";     break;
        default: break;
    }

    fprintf(stderr, "%s:%d (%s)   %s: ", file, line, fn, lvlstr);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void WritePlane(int width, int height, int stride, uint8_t *dst, uint8_t *src)
{
    uint8_t *src_addr = src;
    uint8_t *dst_addr = dst;
    for(int i = 0; i < height; i++)
    {
        memcpy(dst_addr, src_addr,width);
        src_addr += width;
        dst_addr += stride;
    }
}

int LoadYuvImage(int format, int interleave, int packed, int width, int height, BmJpuFramebuffer *framebuffer, BmJpuFramebufferSizes *calculated_sizes, uint8_t *dst, uint8_t *src)
{
    int ret = 0;
    int cbcr_w, cbcr_h, stride, cbcr_stride, chromaSize, lumaSize, w, h, packed_stride;
    if(dst == NULL || src == NULL || framebuffer == NULL || calculated_sizes == NULL)
        return -1;
    stride = calculated_sizes->y_stride;
    cbcr_stride = calculated_sizes->cbcr_stride;

    uint8_t *dst_y  = dst + framebuffer->y_offset;
    uint8_t *dst_cb = dst + framebuffer->cb_offset;
    uint8_t *dst_cr = dst + framebuffer->cr_offset;
    switch(format)
    {
    case BM_JPU_COLOR_FORMAT_YUV420:
        cbcr_h = height >> 1;
        if(interleave == BM_JPU_CHROMA_FORMAT_CBCR_SEPARATED)
            cbcr_w = width >> 1;
        else
            cbcr_w = width;
        break;
    case BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL:
        cbcr_h = height;
        if(interleave == BM_JPU_CHROMA_FORMAT_CBCR_SEPARATED)
            cbcr_w = width >> 1;
        else
            cbcr_w = width;
        break;
    case BM_JPU_COLOR_FORMAT_YUV444:
        cbcr_w = width;
        cbcr_h = height;
        break;
    case BM_JPU_COLOR_FORMAT_YUV400:
        cbcr_w = 0;
        cbcr_h = 0;
        break;
    case BM_JPU_COLOR_FORMAT_YUV422_VERTICAL:
        cbcr_w = width;
        cbcr_h = height >> 1;
        break;
    default:
        printf("not support the format image, format=%d",format);
        return -1;
    }

    chromaSize = calculated_sizes->cbcr_size;
    lumaSize = calculated_sizes->y_size;
    if(!packed)  // planner mode
    {
        WritePlane(width, height, stride, dst_y,src);  // write Y
        if(interleave == BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE)
        {
            WritePlane(cbcr_w, cbcr_h, cbcr_stride, dst_cb, src+lumaSize);  // write UV(interleave)
        }
        else if(interleave == BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE)
        {
            WritePlane(cbcr_w, cbcr_h, cbcr_stride, dst_cr, src+lumaSize);  // write VU(interleave)
        }
        else
        {
            WritePlane(cbcr_w, cbcr_h, cbcr_stride, dst_cb, src+lumaSize);  // write U
            WritePlane(cbcr_w, cbcr_h, cbcr_stride, dst_cr, src+lumaSize+chromaSize);  // write V
        }
    }
    else{  // packed mode
        if(packed == BM_JPU_PACKED_FORMAT_444)
        {
            w = 3 * width;
            packed_stride = 3 * stride;
        }
        else
        {
            w = 2 * width;
            packed_stride = 2 * stride;
        }
        h = height;
        lumaSize = w * h;
        if(width == stride)
            memcpy(dst_y, src,lumaSize);
        else
            WritePlane(w, h, packed_stride, dst_y,src);
    }
    return ret;
}

int DumpCropFrame(BmJpuJPEGDecInfo *info, uint8_t *mapped_virtual_address, FILE *fout)
{
    switch (info->image_format){
        case BM_JPU_IMAGE_FORMAT_YUV420P:
        case BM_JPU_IMAGE_FORMAT_NV12:
        case BM_JPU_IMAGE_FORMAT_NV21:
        {
            // write Y
            for (int i = 0; i < info->actual_frame_height; i++) {
                fwrite(mapped_virtual_address+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
            }
            // write UV
            if (info->image_format != BM_JPU_IMAGE_FORMAT_YUV420P) {
                for (int i = 0; i < info->actual_frame_height/2; i++) {
                    fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
                }
            }
            else {
                for (int i = 0; i < info->actual_frame_height/2 ;i++) {
                    fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+i*info->aligned_frame_width/2, 1, info->actual_frame_width/2, fout);
                }
                for (int i = 0; i < info->actual_frame_height/2 ;i++) {
                    fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+(info->aligned_frame_height/2 * info->aligned_frame_width/2)+i*info->aligned_frame_width/2, 1, info->actual_frame_width/2, fout);
                }
            }
            break;
        }
        case BM_JPU_IMAGE_FORMAT_YUV422P:
        case BM_JPU_IMAGE_FORMAT_NV16:
        case BM_JPU_IMAGE_FORMAT_NV61:
        {
            // write Y
            for (int i = 0; i < info->actual_frame_height; i++) {
                fwrite(mapped_virtual_address+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
            }
            // write UV
            if (info->image_format != BM_JPU_IMAGE_FORMAT_YUV422P) {
                for (int i = 0; i < info->actual_frame_height; i++) {
                    fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
                }
            }
            else {
                for (int i = 0; i < info->actual_frame_height ;i++) {
                    fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+i*info->aligned_frame_width/2, 1, info->actual_frame_width/2, fout);
                }
                for (int i = 0; i < info->actual_frame_height ;i++) {
                    fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+(info->aligned_frame_height * info->aligned_frame_width/2)+i*info->aligned_frame_width/2, 1, info->actual_frame_width/2, fout);
                }
            }
            break;
        }
        // FIXME: which image format to use?
        // case BM_JPU_COLOR_FORMAT_YUV422_VERTICAL:
        // {
        //     for (int i = 0; i < info->actual_frame_height; i++) {
        //         fwrite(mapped_virtual_address+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
        //     }
        //     for (int i = 0; i < info->actual_frame_height;i++) {
        //         fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
        //     }
        //     break;
        // }
        case BM_JPU_IMAGE_FORMAT_YUV444P:
        {
            for (int i = 0; i < info->actual_frame_height; i++) {
                fwrite(mapped_virtual_address+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
            }
            for (int i = 0; i < info->actual_frame_height; i++) {
                fwrite(mapped_virtual_address+(info->aligned_frame_height*info->aligned_frame_width)+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
            }
            for (int i = 0; i < info->actual_frame_height; i++) {
                fwrite(mapped_virtual_address+2*(info->aligned_frame_height*info->aligned_frame_width)+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
            }
            break;
        }
        case BM_JPU_IMAGE_FORMAT_GRAY:
        {
            for (int i = 0; i < info->actual_frame_height;i++) {
                fwrite(mapped_virtual_address+i*info->aligned_frame_width, 1, info->actual_frame_width, fout);
            }
            break;
        }
        default:
        {
            printf("not support the format image, format=%d", info->image_format);
            return -1;
        }
    }

    return 0;
}

void ConvertToImageFormat(BmJpuImageFormat *image_format, BmJpuColorFormat color_format, BmJpuChromaFormat cbcr_interleave, BmJpuPackedFormat packed_format)
{
    // TODO: support packed format?
    if (color_format == BM_JPU_COLOR_FORMAT_YUV420) {
        if (cbcr_interleave == BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV12;
        } else if (cbcr_interleave == BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV21;
        } else {
            *image_format = BM_JPU_IMAGE_FORMAT_YUV420P;
        }
    } else if (color_format == BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL) {
        if (cbcr_interleave == BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV16;
        } else if (cbcr_interleave == BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV61;
        } else {
            *image_format = BM_JPU_IMAGE_FORMAT_YUV422P;
        }
    } else if (color_format == BM_JPU_COLOR_FORMAT_YUV444) {
        // only support planar type
        *image_format = BM_JPU_IMAGE_FORMAT_YUV444P;
    } else {
        *image_format = BM_JPU_IMAGE_FORMAT_GRAY;
    }
}
