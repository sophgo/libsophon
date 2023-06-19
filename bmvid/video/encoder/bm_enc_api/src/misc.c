/*****************************************************************************
 *
 *    Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
 *
 *    bmvid is licensed under the 2-Clause BSD License except for the
 *    third-party components.
 *
 *****************************************************************************/
/* This library provides a high-level interface for controlling the BitMain
 * Sophon VPU en/decoder.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmvpu.h"

#include "bmvpuapi.h"
#include "bmvpuapi_internal.h"


int bmvpu_calc_framebuffer_sizes(int mapType, BmVpuColorFormat color_format,
                                  int frame_width, int frame_height,
                                  int chroma_interleave, BmVpuFbInfo *info)
{
    int fb_fmt = FB_FMT_420;
    if((info == NULL) || (frame_width <= 0) || (frame_height <= 0)){
        BM_VPU_ERROR("bmvpu_calc_framebuffer_sizes params err: info(0X%x), frame_width(%d), frame_height(%d).",info, frame_width, frame_height);
        return -1;
    }

    memset(info, 0, sizeof(BmVpuFbInfo));

    info->width  = BM_VPU_ALIGN_VAL_TO(frame_width, FRAME_ALIGN);
    info->height = BM_VPU_ALIGN_VAL_TO(frame_height, FRAME_ALIGN);

    switch (color_format)
    {
    case BM_VPU_COLOR_FORMAT_YUV420: fb_fmt = FB_FMT_420; break;
    case BM_VPU_COLOR_FORMAT_YUV422: fb_fmt = FB_FMT_422; break;
    case BM_VPU_COLOR_FORMAT_YUV444: fb_fmt = FB_FMT_444; break;
    case BM_VPU_COLOR_FORMAT_YUV400: fb_fmt = FB_FMT_400; break;
    default:
      {
        BM_VPU_ERROR("bmvpu_calc_framebuffer_sizes color_format(%d) err.", color_format);
        return -1;
      }
    }

    info->y_stride = vpu_CalcStride(mapType, info->width, info->height,
                                    fb_fmt, chroma_interleave);

    info->y_size = vpu_CalcLumaSize(mapType, info->y_stride, info->height,
                                    fb_fmt, chroma_interleave);

    info->c_size = vpu_CalcChromaSize(mapType, info->y_stride, info->height,
                                      fb_fmt, chroma_interleave);

    info->size = vpu_GetFrameBufSize(mapType, info->y_stride, info->height,
                                     fb_fmt, chroma_interleave);

    /* TODO */
    switch (color_format)
    {
    case BM_VPU_COLOR_FORMAT_YUV420: info->c_stride = info->y_stride / 2; break;
    case BM_VPU_COLOR_FORMAT_YUV422: info->c_stride = info->y_stride / 2; break;
    case BM_VPU_COLOR_FORMAT_YUV444: info->c_stride = info->y_stride;     break;
    case BM_VPU_COLOR_FORMAT_YUV400: info->c_stride = 0;                  break;
    default:
      {
        BM_VPU_ERROR("bmvpu_calc_framebuffer_sizes color_format(%d) err.", color_format);
        return -1;
      }
    }

    if (chroma_interleave)
        info->c_stride *= 2;
    return 0;
}


int bmvpu_fill_framebuffer_params(BmVpuFramebuffer *fb,
                                   BmVpuFbInfo *info,
                                   bm_device_mem_t *fb_dma_buffer,
                                   int fb_id, void* context)
{
    if((fb == NULL) || (info == NULL)){
        BM_VPU_ERROR("bmvpu_fill_framebuffer_params params err: fb(0X%x), info(0X%x).", fb, info);
        return -1;
    }

    fb->context = context;
    fb->myIndex = fb_id;

    fb->dma_buffer = fb_dma_buffer;

    fb->y_stride    = info->y_stride;
    fb->cbcr_stride = info->c_stride;

    fb->width  = info->width;
    fb->height = info->height;

    fb->y_offset  = 0;
    fb->cb_offset = info->y_size;
    fb->cr_offset = info->y_size + info->c_size;

    return 0;
}

char const *bmvpu_color_format_string(BmVpuColorFormat color_format)
{
    switch (color_format)
    {
    case BM_VPU_COLOR_FORMAT_YUV420: return "YUV 4:2:0";
    case BM_VPU_COLOR_FORMAT_YUV422: return "YUV 4:2:2";
    case BM_VPU_COLOR_FORMAT_YUV444: return "YUV 4:4:4";
    case BM_VPU_COLOR_FORMAT_YUV400: return "YUV 4:0:0 (8-bit grayscale)";
    default:
                                     return "<unknown>";
    }
}

char const *bmvpu_frame_type_string(BmVpuFrameType frame_type)
{
    switch (frame_type)
    {
    case BM_VPU_FRAME_TYPE_I: return "I";
    case BM_VPU_FRAME_TYPE_P: return "P";
    case BM_VPU_FRAME_TYPE_B: return "B";
    case BM_VPU_FRAME_TYPE_IDR: return "IDR";
    default: return "<unknown>";
    }
}

int bmvpu_enc_param_parse(BmVpuEncOpenParams *p, const char *name, const char *value)
{
    char *name_buf = NULL;
    int b_error = 0;
    int name_was_bool;
    int value_was_null = !value;

    if (!name)
        return -1;
    if (!value)
        value = "1";

    if (value[0] == '=')
        value++;

    if (strchr(name, '_')) // s/_/-/g
    {
        char *c;
        name_buf = strdup(name);
        if (!name_buf)
            return -1;
        while ((c = strchr(name_buf, '_')))
            *c = '-';
        name = name_buf;
    }

    if (!strncmp(name, "no", 2))
    {
        name += 2;
        if (name[0] == '-')
            name++;
        value = (!!atoi(value)) ? "0" : "1";
    }
    name_was_bool = 0;

#define OPT(STR) else if (!strcmp(name, STR))
    if (0);
    OPT("bg")
    {
        p->bg_detection    = !!atoi(value);
        BM_VPU_INFO("bg=%d", p->bg_detection);
    }
    OPT("nr")
    {
        p->enable_nr       = !!atoi(value);
        BM_VPU_INFO("nr=%d", p->enable_nr);
    }
    OPT("deblock")
    {
        int offset_tc   = 0;
        int offset_beta = 0;
        int ret = sscanf(value, "%d,%d", &offset_tc, &offset_beta);
        if (ret == 2) {
            p->disable_deblocking = 0;
            p->offset_tc   = offset_tc;
            p->offset_beta = offset_beta;
            BM_VPU_INFO("deblock=%d,%d", p->offset_tc, p->offset_beta);
        } else {
            p->disable_deblocking = !atoi(value);;
            BM_VPU_INFO("%s deblock", p->disable_deblocking?"disable":"enable");
        }
    }
    OPT("gop-preset")
    {
        p->gop_preset      = atoi(value);
        BM_VPU_INFO("gop_preset=%d", p->gop_preset);
    }
    OPT("preset")
    {
        BM_VPU_INFO("preset=%s", value);

        /* 0 : Custom mode, TODO
         * 1 : recommended encoder parameters (slow encoding speed, highest picture quality)
         * 2 : Boost mode (normal encoding speed, normal picture quality),
         * 3 : Fast mode (high encoding speed, low picture quality) */
        if (!strcmp(value, "fast") || !strcmp(value, "0"))
            p->enc_mode = 3;
        else if (!strcmp(value, "medium") || !strcmp(value, "1"))
            p->enc_mode = 2;
        else if (!strcmp(value, "slow") || !strcmp(value, "2"))
            p->enc_mode = 1;
        else {
            p->enc_mode = 2; /* TODO change to custom after lots of cfg parameters are added. */
            BM_VPU_WARNING("Invalid preset:%s. Use slow encoding preset instead.", value);
        }
    }
    OPT("mb-rc")
    {
        p->mb_rc = !!atoi(value);
        BM_VPU_INFO("mb_rc=%d", p->mb_rc);
    }
    OPT("delta-qp")
    {
        p->delta_qp = atoi(value);
        BM_VPU_INFO("delta_qp=%d", p->delta_qp);
    }
    OPT("min-qp")
    {
        p->min_qp = atoi(value);
        BM_VPU_INFO("min_qp=%d", p->min_qp);
    }
    OPT("max-qp")
    {
        p->max_qp = atoi(value);
        BM_VPU_INFO("max_qp=%d", p->max_qp);
    }
    OPT("qp")
    {
        int cqp = atoi(value);
        if (cqp < 0 || cqp > 51) {
            BM_VPU_ERROR("Invalid qp %d", cqp);
            b_error = 1;
            goto END;
        }
        p->bitrate = 0;
        p->cqp = cqp;
        BM_VPU_INFO("const quality mode. qp=%d", p->cqp);
    }
    OPT("bitrate")
    {
        int bitrate = atoi(value);
        if (bitrate < 0) {
            BM_VPU_ERROR("Invalid bitrate %dKbps", bitrate);
            b_error = 1;
            goto END;
        }
        p->bitrate = bitrate*1000;
        BM_VPU_INFO("bitrate=%dKbps", bitrate);
    }
    OPT("weightp")
    {
        p->enable_wp = !!atoi(value);
        BM_VPU_INFO("weightp=%d", p->enable_wp);
    }
    OPT("roi_enable")
    {
        p->roi_enable = atoi(value);
        BM_VPU_INFO("weightp=%d", p->roi_enable);
    }
    else
    {
        b_error = 1;
    }
#undef OPT

END:
    if (name_buf)
        free(name_buf);

    b_error |= value_was_null && !name_was_bool;
    return b_error ? -1 : 0;
}

int bmvpu_malloc_device_byte_heap(bm_handle_t bm_handle, bm_device_mem_t *pmem, unsigned int size, int heap_id_mask, int high_bit_first)
{
    int ret = 0;
    int i = 0;
    int heap_num = 0;
    ret = bm_get_gmem_total_heap_num(bm_handle, &heap_num);
    if (ret != 0)
    {
        BM_VPU_ERROR("bmvpu_malloc_device_byte_heap failed!\n");
        return -1;
    }

    int available_heap_mask = 0;
    for (i=0; i<heap_num; i++){
        available_heap_mask = available_heap_mask | (0x1 << i);
    }

    int enable_heap_mask = available_heap_mask & heap_id_mask;
    if (enable_heap_mask == 0x0)
    {
        BM_VPU_ERROR("bmvpu_malloc_device_byte_heap failed! \n");
        return -1;
    }
    if (high_bit_first)
    {
        for (i=(heap_num-1); i>=0; i--)
        {
            if ((enable_heap_mask & (0x1<<i)))
            {
                ret = bm_malloc_device_byte_heap(bm_handle, pmem, i, size);
                if (ret != 0)
                {
                    BM_VPU_ERROR("bm_malloc_device_byte_heap failed \n");
                }
                return ret;
            }
        }
    }
    else
    {
        for (i=0; i<heap_num; i++)
        {
            if ((enable_heap_mask & (0x1<<i)))
            {
                ret = bm_malloc_device_byte_heap(bm_handle, pmem, i, size);
                if (ret != 0)
                {
                    BM_VPU_ERROR("bm_malloc_device_byte_heap failed \n");
                }
                return ret;
            }
        }
    }
    return ret;
}

/**
 * Upload data from HOST to a VPU core
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_upload_data(int soc_idx,
                      const uint8_t* host_va, int host_stride,
                      uint64_t vpu_pa, int vpu_stride,
                      int width, int height)
{
#if !defined(BM_PCIE_MODE)
    BM_VPU_ERROR("Unsupported for now!");
    return -1;
#else
    int size = vpu_stride*height;
    int ret = 0;

    if (vpu_stride != host_stride)
    {
        const uint8_t *s0 = host_va;
        uint8_t *buffer, *s1;
        int i;

        buffer = calloc(size, sizeof(uint8_t));
        if (buffer == NULL)
        {
            BM_VPU_ERROR("calloc failed!");
            return -1;
        }

        s1 = buffer;
        for (i=0; i<height; i++)
        {
            memcpy(s1, s0, width);
            s0 += host_stride;
            s1 += vpu_stride;
        }

        bm_device_mem_t vpu_mem = bm_mem_from_device(vpu_pa, size);
        ret = bm_memcpy_s2d_partial(bmvpu_enc_get_bmlib_handle(soc_idx), vpu_mem, buffer, size);
        if (ret != 0)
        {
            BM_VPU_ERROR("vpu_write_memory failed!");
            free(buffer);
            return -1;
        }

        free(buffer);
    }
    else
    {
        bm_device_mem_t vpu_mem = bm_mem_from_device(vpu_pa, size);
        ret = bm_memcpy_s2d_partial(bmvpu_enc_get_bmlib_handle(soc_idx), vpu_mem, host_va, size);
        if (ret != 0)
        {
            BM_VPU_ERROR("vpu_write_memory failed!");
            return -1;
        }
    }

    return 0;
#endif
}

/**
 * Download data from a VPU core to HOST
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_download_data(int soc_idx,
                        uint8_t* host_va, int host_stride,
                        uint64_t vpu_pa, int vpu_stride,
                        int width, int height)
{
#if !defined(BM_PCIE_MODE)
    BM_VPU_ERROR("Unsupported for now!");
    return -1;
#else
    int size = vpu_stride*height;
    int ret = 0;

    if (host_stride != vpu_stride)
    {
        uint8_t *buffer, *d0, *d1;
        int i;

        buffer = calloc(size, sizeof(uint8_t));
        if (buffer == NULL)
        {
            BM_VPU_ERROR("calloc failed!");
            return -1;
        }

        bm_device_mem_t vpu_mem = bm_mem_from_device(vpu_pa, size);
        ret = bm_memcpy_d2s_partial(bmvpu_enc_get_bmlib_handle(soc_idx), buffer, vpu_mem, size);
        if (ret != 0)
        {
            BM_VPU_ERROR("vpu_write_memory failed!");
            free(buffer);
            return -1;
        }

        d0 = buffer;
        d1 = host_va;
        for (i=0; i<height; i++)
        {
            memcpy(d1, d0, width);
            d0 += vpu_stride;
            d1 += host_stride;
        }

        free(buffer);
    }
    else
    {
        bm_device_mem_t vpu_mem = bm_mem_from_device(vpu_pa, size);
        ret = bm_memcpy_d2s_partial(bmvpu_enc_get_bmlib_handle(soc_idx), host_va, vpu_mem, size);
        if (ret != 0)
        {
            BM_VPU_ERROR("vpu_write_memory failed!");
            return -1;
        }
    }

    return 0;
#endif
}


