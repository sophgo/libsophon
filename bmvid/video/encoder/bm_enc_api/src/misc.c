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

#include "bm_vpuenc_interface.h"
#include "bm_vpuenc_internal.h"

int bmvpu_calc_framebuffer_sizes(int mapType, BmVpuEncPixFormat pix_format,
                                  int frame_width, int frame_height,
                                  BmVpuFbInfo *info)
{
    int fb_fmt = FB_FMT_420;
    BmVpuEncChromaFormat chroma_interleave = BM_VPU_ENC_CHROMA_NO_INTERLEAVE;
    if((info == NULL) || (frame_width <= 0) || (frame_height <= 0)){
        BM_VPU_ERROR("bmvpu_calc_framebuffer_sizes params err: info(0X%x), frame_width(%d), frame_height(%d).",info, frame_width, frame_height);
        return -1;
    }

    memset(info, 0, sizeof(BmVpuFbInfo));

    info->width  = BM_VPU_ALIGN_VAL_TO(frame_width, FRAME_ALIGN);
    info->height = BM_VPU_ALIGN_VAL_TO(frame_height, FRAME_ALIGN);

    switch (pix_format)
    {
    case BM_VPU_ENC_PIX_FORMAT_YUV420P:
        fb_fmt = FB_FMT_420;
        break;
    case BM_VPU_ENC_PIX_FORMAT_YUV422P:
        fb_fmt = FB_FMT_422;
        break;
    case BM_VPU_ENC_PIX_FORMAT_YUV444P:
        fb_fmt = FB_FMT_444;
        break;
    case BM_VPU_ENC_PIX_FORMAT_YUV400:
        fb_fmt = FB_FMT_400;
        break;
    case BM_VPU_ENC_PIX_FORMAT_NV12:
        fb_fmt = FB_FMT_420;
        chroma_interleave = BM_VPU_ENC_CHROMA_INTERLEAVE_CBCR;
        break;
    case BM_VPU_ENC_PIX_FORMAT_NV16:
        fb_fmt = FB_FMT_422;
        chroma_interleave = BM_VPU_ENC_CHROMA_INTERLEAVE_CBCR;
        break;
    case BM_VPU_ENC_PIX_FORMAT_NV24:
        fb_fmt = FB_FMT_444;
        chroma_interleave = BM_VPU_ENC_CHROMA_INTERLEAVE_CBCR;
        break;
    default:
      {
        BM_VPU_ERROR("bmvpu_calc_framebuffer_sizes pix_format(%s) err.", bmvpu_pix_format_string(pix_format));
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
    switch (pix_format)
    {
    case BM_VPU_ENC_PIX_FORMAT_YUV420P: info->c_stride = info->y_stride / 2; break;
    case BM_VPU_ENC_PIX_FORMAT_YUV422P: info->c_stride = info->y_stride / 2; break;
    case BM_VPU_ENC_PIX_FORMAT_YUV444P: info->c_stride = info->y_stride;     break;
    case BM_VPU_ENC_PIX_FORMAT_YUV400:  info->c_stride = 0;                  break;
    case BM_VPU_ENC_PIX_FORMAT_NV12:    info->c_stride = info->y_stride / 2; break;
    case BM_VPU_ENC_PIX_FORMAT_NV16:    info->c_stride = info->y_stride / 2; break;
    case BM_VPU_ENC_PIX_FORMAT_NV24:    info->c_stride = info->y_stride;     break;
    default:
      {
        BM_VPU_ERROR("bmvpu_calc_framebuffer_sizes pix_format(%s) err.", bmvpu_pix_format_string(pix_format));
        return -1;
      }
    }

    if ((pix_format == BM_VPU_ENC_PIX_FORMAT_NV12) || \
        (pix_format == BM_VPU_ENC_PIX_FORMAT_NV16) || \
        (pix_format == BM_VPU_ENC_PIX_FORMAT_NV24))
        info->c_stride *= 2;
    return 0;
}


int bmvpu_fill_framebuffer_params(BmVpuFramebuffer *fb,
                                   BmVpuFbInfo *info,
                                   BmVpuEncDMABuffer *fb_dma_buffer,
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

/**
 * Upload data from HOST to a VPU core
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_enc_upload_data(int vpu_core_idx,
                      const uint8_t* host_va, int host_stride,
                      uint64_t vpu_pa, int vpu_stride,
                      int width, int height)
{
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

        ret = vpu_write_memory(buffer, size, vpu_core_idx, vpu_pa);
        if (ret < 0)
        {
            BM_VPU_ERROR("vpu_write_memory failed!");
            free(buffer);
            return -1;
        }

        free(buffer);
    }
    else
    {
        ret = vpu_write_memory(host_va, size, vpu_core_idx, vpu_pa);
        if (ret < 0)
        {
            BM_VPU_ERROR("vpu_write_memory failed!");
            return -1;
        }
    }

    return 0;
}

/**
 * Download data from a VPU core to HOST
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_enc_download_data(int vpu_core_idx,
                        uint8_t* host_va, int host_stride,
                        uint64_t vpu_pa, int vpu_stride,
                        int width, int height)
{
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

        ret = vpu_read_memory(buffer, size, vpu_core_idx, vpu_pa);
        if (ret < 0)
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
        ret = vpu_read_memory(host_va, size, vpu_core_idx, vpu_pa);
        if (ret < 0)
        {
            BM_VPU_ERROR("vpu_write_memory failed!");
            return -1;
        }
    }

    return 0;
}

char const *bmvpu_pix_format_string(BmVpuEncPixFormat pix_format)
{
    switch (pix_format)
    {
    case BM_VPU_ENC_PIX_FORMAT_YUV420P: return "yuv420p YUV 4:2:0";
    case BM_VPU_ENC_PIX_FORMAT_YUV422P: return "yuv422p YUV 4:2:2";
    case BM_VPU_ENC_PIX_FORMAT_YUV444P: return "yuv444p YUV 4:4:4";
    case BM_VPU_ENC_PIX_FORMAT_YUV400:  return "yuv400 YUV 4:0:0 (8-bit grayscale)";
    case BM_VPU_ENC_PIX_FORMAT_NV12:    return "nv12 YUV 4:2:0";
    case BM_VPU_ENC_PIX_FORMAT_NV16:    return "nv16 YUV 4:2:2";
    case BM_VPU_ENC_PIX_FORMAT_NV24:    return "nv24 YUV 4:4:4";

    default:
                                     return "<unknown>";
    }
}

char const *bmvpu_frame_type_string(BmVpuEncFrameType frame_type)
{
    switch (frame_type)
    {
    case BM_VPU_ENC_FRAME_TYPE_I: return "I";
    case BM_VPU_ENC_FRAME_TYPE_P: return "P";
    case BM_VPU_ENC_FRAME_TYPE_B: return "B";
    case BM_VPU_ENC_FRAME_TYPE_IDR: return "IDR";
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
            p->enc_mode = BM_VPU_ENC_FAST_MODE;
        else if (!strcmp(value, "medium") || !strcmp(value, "1"))
            p->enc_mode = BM_VPU_ENC_BOOST_MODE;
        else if (!strcmp(value, "slow") || !strcmp(value, "2"))
            p->enc_mode = BM_VPU_ENC_RECOMMENDED_MODE;
        else {
            p->enc_mode = BM_VPU_ENC_BOOST_MODE; /* TODO change to custom after lots of cfg parameters are added. */
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

/************************************************************************************
 *                    encoder device memory management
 *************************************************************************************/
/**
 * Allocate device memory
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_enc_dma_buffer_allocate(int vpu_core_idx, BmVpuEncDMABuffer *buf, unsigned int size)
{
    int ret = 0;

    ret = vpu_EncAllocateDMABuffer(vpu_core_idx, (BmVpuDMABuffer *)buf, size);
    if (ret != 0) {
        BM_VPU_ERROR("vpu_EncAllocateDMABuffer failed!");
        return -1;
    }

    return 0;
}

/**
 * DeAllocate device memory
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_enc_dma_buffer_deallocate(int vpu_core_idx, BmVpuEncDMABuffer *buf)
{
    int ret;

    ret = vpu_EncDeAllocateDMABuffer(vpu_core_idx, (BmVpuDMABuffer *)buf);
    if (ret != 0){
        BM_VPU_ERROR("vpu_EncDeAllocateDMABuffer failed!");
        return -1;
    }

    return 0;
}

/**
 * Attach external DMA buffer to buffer_pool
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_enc_dma_buffer_attach(int vpu_core_idx, uint64_t paddr, unsigned int size)
{
    int ret = 0;

    BmVpuDMABuffer buf;
    /* The size of EXTERNAL DMA buffer */
    buf.size      =  size;
    /* The EXTERNAL DMA buffer physical address */
    buf.phys_addr = paddr;

    ret = vpu_EncAttachDMABuffer(vpu_core_idx, &buf);
    if (ret != 0) {
        BM_VPU_ERROR("vpu_EncAllocateDMABuffer failed!");
        return -1;
    }

    return 0;
}

/**
 * DeAttach external DMA buffer from buffer_pool
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_enc_dma_buffer_deattach(int vpu_core_idx,  uint64_t paddr, unsigned int size)
{
    int ret = 0;
    BmVpuDMABuffer buf;

    buf.size      = size;
    buf.phys_addr = paddr;

    ret = vpu_EncDeattachDMABuffer(vpu_core_idx, &buf);
    if (ret != 0) {
        BM_VPU_ERROR("vpu_EncAllocateDMABuffer failed!");
        return -1;
    }

    return 0;
}

/**
 * Mmap operation
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_dma_buffer_map(int vpu_core_idx, BmVpuEncDMABuffer* buf, int port_flag)
{
    int ret = 0;

    ret = vpu_EncMmap(vpu_core_idx, (BmVpuDMABuffer*)buf, port_flag);
    if (ret != 0) {
        BM_VPU_ERROR("vpu_EncMmap failed, core=%d", vpu_core_idx);
        return -1;
    }
    return 0;
}

/**
 * Munmap operation
 *
 * return value:
 *   -1, failed
 *    0, done
 */
int bmvpu_dma_buffer_unmap(int vpu_core_idx, BmVpuEncDMABuffer* buf)
{
    int ret = 0;
    ret = vpu_EncMunmap(vpu_core_idx, (BmVpuDMABuffer*)buf);
    if (ret != 0){
        BM_VPU_ERROR("vpu_EncMunmap failed");
        return -1;
    }
    return 0;
}

uint64_t bmvpu_enc_dma_buffer_get_physical_address(BmVpuEncDMABuffer* buf)
{
    return buf->phys_addr;
}

unsigned int bmvpu_enc_dma_buffer_get_size(BmVpuEncDMABuffer* buf)
{
    return buf->size;
}

int bmvpu_enc_dma_buffer_flush(int vpu_core_idx, BmVpuEncDMABuffer* buf)
{
    int ret;
    ret = vpu_EncFlushDecache(vpu_core_idx, (BmVpuDMABuffer*)buf);
    if (ret != 0) {
        BM_VPU_ERROR("vpu_EncFlushDecache failed");
        return -1;
    }

    return 0;
}

int bmvpu_enc_dma_buffer_invalidate(int vpu_core_idx, BmVpuEncDMABuffer* buf)
{
    int ret;
    ret = vpu_EncInvalidateDecache(vpu_core_idx, (BmVpuDMABuffer*)buf);
    if (ret != 0) {
        BM_VPU_ERROR("vpu_EncInvalidateDecache failed");
        return -1;
    }

    return 0;
}
