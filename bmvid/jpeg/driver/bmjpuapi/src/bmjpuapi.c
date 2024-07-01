/* bmjpuapi API library for the BitMain SoC
 * Copyright (C) 2018 Solan Shang
 * Copyright (C) 2014 Carlos Rafael Giani
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
#include <stdlib.h>
#include "bm_jpeg_internal.h"
#include "bmjpuapi_priv.h"

#include "jpu_lib.h"
#include "jpu_io.h"
#include "jpu_logging.h"

static void* default_heap_alloc_fn(size_t const size, void *context, char const *file, int const line, char const *fn)
{
    BMJPUAPI_UNUSED_PARAM(context);
    BMJPUAPI_UNUSED_PARAM(file);
    BMJPUAPI_UNUSED_PARAM(line);
    BMJPUAPI_UNUSED_PARAM(fn);
    return malloc(size);
}
static void default_heap_free_fn(void *memblock, size_t const size, void *context, char const *file, int const line, char const *fn)
{
    BMJPUAPI_UNUSED_PARAM(context);
    BMJPUAPI_UNUSED_PARAM(size);
    BMJPUAPI_UNUSED_PARAM(file);
    BMJPUAPI_UNUSED_PARAM(line);
    BMJPUAPI_UNUSED_PARAM(fn);
    free(memblock);
}

void *bm_jpu_cur_heap_alloc_context;
BmJpuHeapAllocFunc bm_jpu_cur_heap_alloc_fn = default_heap_alloc_fn;
BmJpuHeapFreeFunc bm_jpu_cur_heap_free_fn = default_heap_free_fn;

void bm_jpu_set_heap_allocator_functions(BmJpuHeapAllocFunc heap_alloc_fn, BmJpuHeapFreeFunc heap_free_fn, void *context)
{
    bm_jpu_cur_heap_alloc_context = context;
    if ((heap_alloc_fn == NULL) || (heap_free_fn == NULL))
    {
        bm_jpu_cur_heap_alloc_fn = default_heap_alloc_fn;
        bm_jpu_cur_heap_free_fn = default_heap_free_fn;
    }
    else
    {
        bm_jpu_cur_heap_alloc_fn = heap_alloc_fn;
        bm_jpu_cur_heap_free_fn = heap_free_fn;
    }
}




/***********************/
/******* LOGGING *******/
/***********************/


static void default_logging_fn(BmJpuLogLevel level, char const *file, int const line, char const *fn, const char *format, ...)
{
    BMJPUAPI_UNUSED_PARAM(level);
    BMJPUAPI_UNUSED_PARAM(file);
    BMJPUAPI_UNUSED_PARAM(line);
    BMJPUAPI_UNUSED_PARAM(fn);
    BMJPUAPI_UNUSED_PARAM(format);
}

BmJpuLogLevel bm_jpu_cur_log_level_threshold = BM_JPU_LOG_LEVEL_ERROR;
BmJpuLoggingFunc bm_jpu_cur_logging_fn = default_logging_fn;

void bm_jpu_set_logging_function(BmJpuLoggingFunc logging_fn)
{
    bm_jpu_cur_logging_fn = (logging_fn != NULL) ? logging_fn : default_logging_fn;
}

void bm_jpu_set_logging_threshold(BmJpuLogLevel threshold)
{
    bm_jpu_cur_log_level_threshold = threshold;
    int level;

    /* Pass the log level threshold to libbmjpu */
    switch (threshold)
    {
    case BM_JPU_LOG_LEVEL_ERROR:   level = JPU_LOG_ERROR;   break;
    case BM_JPU_LOG_LEVEL_WARNING: level = JPU_LOG_WARNING; break;
    case BM_JPU_LOG_LEVEL_INFO:    level = JPU_LOG_INFO;    break;
    case BM_JPU_LOG_LEVEL_DEBUG:   level = JPU_LOG_DEBUG;   break;
    case BM_JPU_LOG_LEVEL_LOG:     level = JPU_LOG_LOG;     break;
    case BM_JPU_LOG_LEVEL_TRACE:   level = JPU_LOG_TRACE;   break;
    default: level = JPU_LOG_TRACE; break;
    }

    jpu_SetLoggingThreshold(level);
}




/******************************************************/
/******* MISCELLANEOUS STRUCTURES AND FUNCTIONS *******/
/******************************************************/


char const *bm_jpu_color_format_string(BmJpuColorFormat color_format)
{
    switch (color_format)
    {
        case BM_JPU_COLOR_FORMAT_YUV420: return "YUV 4:2:0";
        case BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL: return "YUV 4:2:2 horizontal";
        case BM_JPU_COLOR_FORMAT_YUV422_VERTICAL: return "YUV 2:2:4 vertical";
        case BM_JPU_COLOR_FORMAT_YUV444: return "YUV 4:4:4";
        case BM_JPU_COLOR_FORMAT_YUV400: return "YUV 4:0:0 (8-bit grayscale)";
        default:
            return "<unknown>";
    }
}

char const *bm_jpu_image_format_string(BmJpuImageFormat image_format)
{
    switch (image_format)
    {
        case BM_JPU_IMAGE_FORMAT_YUV420P: return "YUV420P";
        case BM_JPU_IMAGE_FORMAT_YUV422P: return "YUV422P";
        case BM_JPU_IMAGE_FORMAT_YUV444P: return "YUV444P";
        case BM_JPU_IMAGE_FORMAT_NV12: return "NV12";
        case BM_JPU_IMAGE_FORMAT_NV21: return "NV21";
        case BM_JPU_IMAGE_FORMAT_NV16: return "NV16";
        case BM_JPU_IMAGE_FORMAT_NV61: return "NV61";
        case BM_JPU_IMAGE_FORMAT_GRAY: return "GRAY";
        default:
            return "<unknown>";
    }
}

void bm_jpu_convert_from_image_format(BmJpuImageFormat image_format, BmJpuColorFormat *color_format, BmJpuChromaFormat *chroma_interleave, BmJpuPackedFormat *packed_format)
{
    if (color_format == NULL || chroma_interleave == NULL || packed_format == NULL) {
        BM_JPU_ERROR("invalid params: color_format(%p), chroma_interleave(%p), packed_format(%p)", color_format, chroma_interleave, packed_format);
        return;
    }

    *color_format = BM_JPU_COLOR_FORMAT_YUV420;
    *chroma_interleave = BM_JPU_CHROMA_FORMAT_CBCR_SEPARATED;
    // TODO: support packed format?
    *packed_format = BM_JPU_PACKED_FORMAT_NONE;

    switch (image_format) {
        case BM_JPU_IMAGE_FORMAT_YUV420P:
            *color_format = BM_JPU_COLOR_FORMAT_YUV420;
            break;
        case BM_JPU_IMAGE_FORMAT_YUV422P:
            *color_format = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
            break;
        case BM_JPU_IMAGE_FORMAT_YUV444P:
            *color_format = BM_JPU_COLOR_FORMAT_YUV444;
            break;
        case BM_JPU_IMAGE_FORMAT_NV12:
            *color_format = BM_JPU_COLOR_FORMAT_YUV420;
            *chroma_interleave = BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE;
            break;
        case BM_JPU_IMAGE_FORMAT_NV21:
            *color_format = BM_JPU_COLOR_FORMAT_YUV420;
            *chroma_interleave = BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE;
            break;
        case BM_JPU_IMAGE_FORMAT_NV16:
            *color_format = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
            *chroma_interleave = BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE;
            break;
        case BM_JPU_IMAGE_FORMAT_NV61:
            *color_format = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
            *chroma_interleave = BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE;
            break;
        case BM_JPU_IMAGE_FORMAT_GRAY:
            *color_format = BM_JPU_COLOR_FORMAT_YUV400;
            break;
    }
}

void bm_jpu_convert_to_image_format(BmJpuColorFormat color_format, BmJpuChromaFormat chroma_interleave, BmJpuPackedFormat packed_format, BmJpuImageFormat *image_format)
{
    if (image_format == NULL) {
        BM_JPU_ERROR("invalid params: image_format(%p)", image_format);
        return;
    }

    *image_format = BM_JPU_IMAGE_FORMAT_YUV420P;
    // TODO: support packed format?
    if (color_format == BM_JPU_COLOR_FORMAT_YUV420) {
        if (chroma_interleave == BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV12;
        } else if (chroma_interleave == BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV21;
        } else {
            *image_format = BM_JPU_IMAGE_FORMAT_YUV420P;
        }
    } else if (color_format == BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL) {
        if (chroma_interleave == BM_JPU_CHROMA_FORMAT_CBCR_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV16;
        } else if (chroma_interleave == BM_JPU_CHROMA_FORMAT_CRCB_INTERLEAVE) {
            *image_format = BM_JPU_IMAGE_FORMAT_NV61;
        } else {
            *image_format = BM_JPU_IMAGE_FORMAT_YUV422P;
        }
    } else if (color_format == BM_JPU_COLOR_FORMAT_YUV444) {
        // only support planar type
        *image_format = BM_JPU_IMAGE_FORMAT_YUV444P;
    } else if (color_format == BM_JPU_COLOR_FORMAT_YUV400) {
        *image_format = BM_JPU_IMAGE_FORMAT_GRAY;
    }
}