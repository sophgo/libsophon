/* Simplified API for JPEG en- and decoding with the BitMain SoC
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
#ifndef  _WIN32
#include <pthread.h>
#endif
#include "bm_jpeg_internal.h"
#include "bmjpuapi_priv.h"
#define BS_MASK (1024*16-1)

#define HEAP_0_1_2  (0x7)
#define HEAP_1_2    (0x6)

static unsigned long int get_cur_threadid(){
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

bm_status_t bm_jpu_malloc_device_mem_heap_mask(bm_handle_t handle, bm_jpu_phys_addr_t *paddr, int heap_mask, size_t size)
{
    int heap_id = 0;
    bm_status_t ret = BM_SUCCESS;

    while (heap_mask) {
        if (heap_mask & 0x1) {
            if ((ret = bm_malloc_device_mem(handle, paddr, heap_id, size)) == BM_SUCCESS)
                return BM_SUCCESS;
        }
        heap_mask >>= 1;
        heap_id += 1;
    }

    return BM_ERR_FAILURE;
}

/******************
 ** JPEG DECODER **
 ******************/

static BmJpuDecReturnCodes initial_info_callback(BmJpuDecoder *decoder, BmJpuDecInitialInfo *new_initial_info, unsigned int output_code, void *user_data);
static BmJpuDecReturnCodes bm_jpu_jpeg_dec_deallocate_framebuffers(BmJpuJPEGDecoder *jpeg_decoder);


static BmJpuDecReturnCodes initial_info_callback(BmJpuDecoder *decoder, BmJpuDecInitialInfo *new_initial_info, unsigned int output_code, void *user_data)
{
    unsigned int i;
    BmJpuDecReturnCodes ret;
    bm_status_t bmlib_ret = BM_SUCCESS;
    bm_handle_t handle = NULL;
    BmJpuJPEGDecoder *jpeg_decoder = (BmJpuJPEGDecoder *)user_data;
    int frame_width, frame_height;
    int framebuffer_size;

    BMJPUAPI_UNUSED_PARAM(decoder);
    BMJPUAPI_UNUSED_PARAM(output_code);

    jpeg_decoder->initial_info = *new_initial_info;
    frame_width = new_initial_info->frame_width;
    frame_height = new_initial_info->frame_height;
    if (new_initial_info->roiFrameWidth && new_initial_info->roiFrameHeight) {
        if (new_initial_info->roiFrameWidth < new_initial_info->frame_width)
            frame_width = new_initial_info->roiFrameWidth;
        if (new_initial_info->roiFrameHeight < new_initial_info->frame_height)
            frame_height = new_initial_info->roiFrameHeight;
    }

    BM_JPU_DEBUG(
        "initial info => size: %u x %u, min_num_required_framebuffers: %u, framebuffer_alignment: %u, image_format: %s",
        new_initial_info->frame_width,
        new_initial_info->frame_height,
        new_initial_info->min_num_required_framebuffers,
        new_initial_info->framebuffer_alignment,
        bm_jpu_image_format_string(new_initial_info->image_format)
    );

    ret = bm_jpu_calc_framebuffer_sizes(frame_width,
                                  frame_height,
                                  new_initial_info->framebuffer_alignment,
                                  new_initial_info->image_format,
                                  &(jpeg_decoder->calculated_sizes));
    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        BM_JPU_ERROR("failed to calculate framebuffer size: %s. id=%lu", bm_jpu_dec_error_string(ret), get_cur_threadid());
        return ret;
    }

    BM_JPU_DEBUG(
        "calculated sizes => frame width x height: %d x %d, Y stride: %u, CbCr stride: %u, Y size: %u, CbCr size: %u, total size: %u",
        jpeg_decoder->calculated_sizes.aligned_frame_width, jpeg_decoder->calculated_sizes.aligned_frame_height,
        jpeg_decoder->calculated_sizes.y_stride, jpeg_decoder->calculated_sizes.cbcr_stride,
        jpeg_decoder->calculated_sizes.y_size, jpeg_decoder->calculated_sizes.cbcr_size,
        jpeg_decoder->calculated_sizes.total_size
    );

    if ((jpeg_decoder->framebuffer_recycle || jpeg_decoder->framebuffer_from_user) && jpeg_decoder->framebuffer_addrs != NULL) {
        // assume all framebuffer have the same size
        if (jpeg_decoder->framebuffer_size < jpeg_decoder->calculated_sizes.total_size) {
            BM_JPU_ERROR("framebuffer size is less than calculated size, request: %d, has: %d\n", jpeg_decoder->calculated_sizes.total_size, jpeg_decoder->framebuffer_size);
            return BM_JPU_DEC_RETURN_CODE_INSUFFICIENT_FRAMEBUFFERS;
        }
    }

    if (!jpeg_decoder->framebuffer_recycle) {
        // need to release framebuffer, but reuse dma buffer
        BM_JPU_DEBUG("de-allocate framebuffer");
        ret = bm_jpu_jpeg_dec_deallocate_framebuffers(jpeg_decoder);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
            return ret;
        }
    }

    if (!jpeg_decoder->framebuffer_from_user) {
        // the num of framebuffer is set by decoder
        jpeg_decoder->num_framebuffers = new_initial_info->min_num_required_framebuffers + jpeg_decoder->num_extra_framebuffers;
    }

    framebuffer_size = jpeg_decoder->framebuffer_recycle ? jpeg_decoder->framebuffer_size : jpeg_decoder->calculated_sizes.total_size;
    if (jpeg_decoder->framebuffer_addrs == NULL) {
        /* Only enter here in following situations:
         * 1. the first time to decode or sequence change in normal mode.
         * 2. the first time to decode if framebuffer is not allocated by user in framebuffer_recycle mode.
         */
        BM_JPU_DEBUG("allocate dma buffer");
        jpeg_decoder->framebuffer_addrs = BM_JPU_ALLOC(sizeof(bm_jpu_phys_addr_t) * jpeg_decoder->num_framebuffers);

        // allocate framebuffer device memory
        handle = bm_jpu_dec_get_bm_handle(jpeg_decoder->device_index);
        for (i = 0; i < jpeg_decoder->num_framebuffers; ++i) {
            bmlib_ret = bm_jpu_malloc_device_mem_heap_mask(handle, &jpeg_decoder->framebuffer_addrs[i], HEAP_1_2, framebuffer_size);
            if (bmlib_ret != BM_SUCCESS) {
                BM_JPU_ERROR("could not allocate DMA buffer for framebuffer #%u. id=%lu", i, get_cur_threadid());
                goto error;
            }
        }
        jpeg_decoder->framebuffer_size = framebuffer_size;
    }

    if (jpeg_decoder->framebuffers == NULL) {
        BM_JPU_DEBUG("allocate framebuffer");
        jpeg_decoder->framebuffers = BM_JPU_ALLOC(sizeof(BmJpuFramebuffer) * jpeg_decoder->num_framebuffers);
        for (i = 0; i < jpeg_decoder->num_framebuffers; ++i) {
            jpeg_decoder->framebuffers[i].dma_buffer = BM_JPU_ALLOC(sizeof(bm_device_mem_t) * jpeg_decoder->num_framebuffers);
        }
    }

    // update framebuffer params (stride and offset)
    for (i = 0; i < jpeg_decoder->num_framebuffers; ++i) {
        ret = bm_jpu_fill_framebuffer_params(&(jpeg_decoder->framebuffers[i]), &(jpeg_decoder->calculated_sizes), jpeg_decoder->framebuffer_addrs[i], framebuffer_size, 0);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
            goto error;
        }
    }

    // the decoded frames will be flushed
    ret = bm_jpu_dec_register_framebuffers(jpeg_decoder->decoder, jpeg_decoder->framebuffers, jpeg_decoder->num_framebuffers);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        BM_JPU_ERROR("could not register framebuffers: %s. id=%lu", bm_jpu_dec_error_string(ret), get_cur_threadid());
        goto error;
    }

    return BM_JPU_DEC_RETURN_CODE_OK;

error:
    bm_jpu_jpeg_dec_deallocate_framebuffers(jpeg_decoder);

    return BM_JPU_DEC_RETURN_CODE_ERROR;
}


BmJpuDecReturnCodes bm_jpu_jpeg_dec_open(BmJpuJPEGDecoder **jpeg_decoder,
                                         BmJpuDecOpenParams *open_params,
                                         unsigned int num_extra_framebuffers)
{
    BmJpuDecReturnCodes ret = BM_JPU_DEC_RETURN_CODE_OK;
    BmJpuJPEGDecoder *jpegdec = NULL;
    bm_handle_t handle = NULL;
    bm_device_mem_t dev_mem = BM_MEM_NULL;
    bm_status_t bmlib_ret = BM_SUCCESS;
    int i = 0;

    if ((jpeg_decoder == NULL) || (open_params == NULL)) {
        BM_JPU_ERROR("bm_jpu_jpeg_dec_open params err: jpeg_decoder(0X%lx), open_params(0X%lx).", jpeg_decoder, open_params);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    handle = bm_jpu_dec_get_bm_handle(open_params->device_index);
    if (handle == 0) {
        BM_JPU_ERROR("bm_jpu_dec_get_bm_handle failed device_index:%d.\n", open_params->device_index);
        return BM_JPU_DEC_RETURN_CODE_ERROR;
    }

    jpegdec = BM_JPU_ALLOC(sizeof(BmJpuJPEGDecoder));
    if (jpegdec == NULL) {
        BM_JPU_ERROR("allocating memory for JPEG decoder object failed. id=%lu", get_cur_threadid());
        return BM_JPU_DEC_RETURN_ALLOC_MEM_ERROR;
    }

    memset(jpegdec, 0, sizeof(BmJpuJPEGDecoder));
    jpegdec->device_index = open_params->device_index;
    jpegdec->num_extra_framebuffers = num_extra_framebuffers;  // TODO: Motion JPEG

    BM_JPU_DEBUG("bitstream_from_user=%d", open_params->bitstream_from_user);
    jpegdec->bitstream_from_user = open_params->bitstream_from_user;
    if (jpegdec->bitstream_from_user) {
        if (open_params->bs_buffer_phys_addr == 0) {
            BM_JPU_ERROR("input bitstream buffer addr (%#lx) is invalid", open_params->bs_buffer_phys_addr);
            ret = BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
            goto error;
        }

        if (open_params->bs_buffer_size <= 0) {
            BM_JPU_ERROR("input bitstream buffer size (%d) is invalid", open_params->bs_buffer_size);
            ret = BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
            goto error;
        }

        open_params->bs_buffer_size = (open_params->bs_buffer_size+BS_MASK)&(~BS_MASK);

        jpegdec->bitstream_buffer_addr = open_params->bs_buffer_phys_addr;
        jpegdec->bitstream_buffer_size = open_params->bs_buffer_size;
        jpegdec->bitstream_buffer_alignment = JPU_MEMORY_ALIGNMENT;
    }
    else {
        if (open_params->bs_buffer_size <= 0) {
            bm_jpu_dec_get_bitstream_buffer_info(&open_params->bs_buffer_size, &(jpegdec->bitstream_buffer_alignment));
        }
        else {
            open_params->bs_buffer_size = (open_params->bs_buffer_size+BS_MASK)&(~BS_MASK);
            jpegdec->bitstream_buffer_alignment = JPU_MEMORY_ALIGNMENT;
        }

        bmlib_ret = bm_jpu_malloc_device_mem_heap_mask(handle, &jpegdec->bitstream_buffer_addr, HEAP_1_2, open_params->bs_buffer_size);
        if (bmlib_ret != BM_SUCCESS) {
            BM_JPU_ERROR("could not allocate DMA buffer for bitstream buffer with %u bytes and alignment %u. id=%lu",
                        open_params->bs_buffer_size, jpegdec->bitstream_buffer_alignment, get_cur_threadid());
            ret = BM_JPU_DEC_RETURN_ALLOC_MEM_ERROR;
            goto error;
        }

        jpegdec->bitstream_buffer_size = open_params->bs_buffer_size;
    }

    if (open_params->framebuffer_num <= 0) {
        open_params->framebuffer_num = MIN_NUM_FREE_FB_REQUIRED;
    }
    BM_JPU_DEBUG("framebuffer_num=%d", open_params->framebuffer_num);
    jpegdec->num_framebuffers = open_params->framebuffer_num;

    BM_JPU_DEBUG("framebuffer_recycle=%d", open_params->framebuffer_recycle);
    jpegdec->framebuffer_recycle = open_params->framebuffer_recycle;

    BM_JPU_DEBUG("framebuffer_from_user=%d", open_params->framebuffer_from_user);
    jpegdec->framebuffer_from_user = open_params->framebuffer_from_user;

    if (jpegdec->framebuffer_recycle || jpegdec->framebuffer_from_user) {
        BM_JPU_DEBUG("framebuffer_size=%u", open_params->framebuffer_size);
        if (open_params->framebuffer_size <= 0) {
            BM_JPU_ERROR("the size of framebuffer should be larger than 0");
            ret = BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
            goto error;
        }

        jpegdec->framebuffer_size = open_params->framebuffer_size;
    }

    if (jpegdec->framebuffer_from_user) {
        jpegdec->framebuffer_addrs = BM_JPU_ALLOC(sizeof(bm_jpu_phys_addr_t) * jpegdec->num_framebuffers);
        for (i = 0; i < jpegdec->num_framebuffers; i++) {
            BM_JPU_DEBUG("framebuffer[%d]: phys_addr=%#lx, size=%zu", i, open_params->framebuffer_phys_addrs[i], jpegdec->framebuffer_size);
            jpegdec->framebuffer_addrs[i] = open_params->framebuffer_phys_addrs[i];
        }
    }

    BM_JPU_DEBUG("dec input bitstream address = %#lx, size = %u", jpegdec->bitstream_buffer_addr, jpegdec->bitstream_buffer_size);
    dev_mem = bm_mem_from_device(jpegdec->bitstream_buffer_addr, jpegdec->bitstream_buffer_size);
    ret = bm_jpu_dec_open(&(jpegdec->decoder), open_params, &dev_mem, initial_info_callback, jpegdec);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        goto error;
    }

    *jpeg_decoder = jpegdec;

    return BM_JPU_DEC_RETURN_CODE_OK;

error:
    if (jpegdec->framebuffer_from_user && jpegdec->framebuffer_addrs != NULL) {
        BM_JPU_FREE(jpegdec->framebuffer_addrs, sizeof(bm_jpu_phys_addr_t) * jpegdec->num_framebuffers);
        jpegdec->framebuffer_addrs = NULL;
    }

    if (jpegdec != NULL) {
        BM_JPU_FREE(jpegdec, sizeof(BmJpuJPEGDecoder));
        jpegdec = NULL;
    }

    return ret;
}


BmJpuDecReturnCodes bm_jpu_jpeg_dec_close(BmJpuJPEGDecoder *jpeg_decoder)
{
    bm_handle_t handle = NULL;
    BmJpuDecReturnCodes ret = BM_JPU_DEC_RETURN_CODE_OK;
    int i = 0;

    if (jpeg_decoder == NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_dec_close params err: jpeg_decoder(0X%lx)", jpeg_decoder);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    bm_jpu_dec_close(jpeg_decoder->decoder);

    ret = bm_jpu_jpeg_dec_deallocate_framebuffers(jpeg_decoder);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK){
        return ret;
    }

    handle =  bm_jpu_dec_get_bm_handle(jpeg_decoder->device_index);
    if (handle == 0) {
        BM_JPU_ERROR("bm_jpu_dec_get_bm_handle failed device_index:%d.\n", jpeg_decoder->device_index);
        return BM_JPU_DEC_RETURN_CODE_ERROR;
    }

    if (jpeg_decoder->bitstream_buffer_addr != 0) {
        if (!jpeg_decoder->bitstream_from_user) {
            bm_free_device_mem(handle, jpeg_decoder->bitstream_buffer_addr);
        }
        jpeg_decoder->bitstream_buffer_addr = 0;
        BM_JPU_DEBUG("bitstream buffer has been released");
    }

    if (jpeg_decoder->framebuffer_addrs != NULL) {
        if (!jpeg_decoder->framebuffer_from_user) {
            for (i = 0; i < jpeg_decoder->num_framebuffers; i++) {
                bm_free_device_mem(handle, jpeg_decoder->framebuffer_addrs[i]);
            }
        }
        BM_JPU_FREE(jpeg_decoder->framebuffer_addrs, sizeof(bm_jpu_phys_addr_t) * jpeg_decoder->num_framebuffers);
        jpeg_decoder->framebuffer_addrs = NULL;
        BM_JPU_DEBUG("frame dma buffer has been released");
    }

    BM_JPU_FREE(jpeg_decoder, sizeof(BmJpuJPEGDecoder));

    return BM_JPU_DEC_RETURN_CODE_OK;
}


static BmJpuDecReturnCodes bm_jpu_jpeg_dec_deallocate_framebuffers(BmJpuJPEGDecoder *jpeg_decoder)
{
    unsigned int i;
    bm_handle_t handle = NULL;

    if (jpeg_decoder == NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_dec_deallocate_framebuffers params err: jpeg_decoder(0X%lx)", jpeg_decoder);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    if (jpeg_decoder->framebuffers != NULL) {
        for (i = 0; i < jpeg_decoder->num_framebuffers; ++i) {
            BM_JPU_FREE(jpeg_decoder->framebuffers[i].dma_buffer, sizeof(bm_device_mem_t));
            jpeg_decoder->framebuffers[i].dma_buffer = NULL;
        }
        BM_JPU_FREE(jpeg_decoder->framebuffers, sizeof(BmJpuFramebuffer) * jpeg_decoder->num_framebuffers);
        jpeg_decoder->framebuffers = NULL;
        BM_JPU_DEBUG("framebuffer has been released");
    }

    if (jpeg_decoder->framebuffer_addrs != NULL) {
        if (!(jpeg_decoder->framebuffer_recycle || jpeg_decoder->framebuffer_from_user)) {
            // only release dma buffer in normal mode
            handle = bm_jpu_dec_get_bm_handle(jpeg_decoder->device_index);
            for (i = 0; i < jpeg_decoder->num_framebuffers; ++i) {
                bm_free_device_mem(handle, jpeg_decoder->framebuffer_addrs[i]);
            }

            BM_JPU_FREE(jpeg_decoder->framebuffer_addrs, sizeof(bm_jpu_phys_addr_t) * jpeg_decoder->num_framebuffers);
            jpeg_decoder->framebuffer_addrs = NULL;
            jpeg_decoder->num_framebuffers = 0;
        }
    }

    return BM_JPU_DEC_RETURN_CODE_OK;
}


int bm_jpu_jpeg_dec_can_decode(BmJpuJPEGDecoder *jpeg_decoder)
{
    return bm_jpu_dec_check_if_can_decode(jpeg_decoder->decoder);
}

BmJpuDecReturnCodes bm_jpu_jpeg_dec_decode(BmJpuJPEGDecoder *jpeg_decoder, uint8_t const *jpeg_data, size_t const jpeg_data_size, int timeout, int timeout_count)
{
    unsigned int output_code;
    BmJpuDecReturnCodes ret;
    BmJpuEncodedFrame encoded_frame;
    int count = 0;
    bm_handle_t handle = NULL;
    bm_device_mem_t bs_mem = {0};
    int bs_size = 0;
    int bmlib_ret = 0;

    if ((jpeg_decoder == NULL) || (jpeg_data == NULL) || (jpeg_data_size <= 0)){
        BM_JPU_ERROR("bm_jpu_jpeg_dec_decode params err: jpeg_decoder(0X%lx), jpeg_data(0X%lx), jpeg_data_size(%d).", jpeg_decoder, jpeg_data, jpeg_data_size);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    memset(&encoded_frame, 0, sizeof(encoded_frame));
    encoded_frame.data = (uint8_t *)jpeg_data;
    encoded_frame.data_size = jpeg_data_size;

    // set interrupt timeout config
    bm_jpu_dec_set_interrupt_timeout(jpeg_decoder->decoder, timeout, timeout_count);

    do{
        ret = bm_jpu_dec_decode(jpeg_decoder->decoder, bs_mem, &encoded_frame, &output_code);
        if(ret == BM_JPU_DEC_RETURN_ILLEGAL_BS_SIZE)
        {
            if(jpeg_decoder->bitstream_from_user)
            {
                BM_JPU_ERROR("bs size: %d is less than %d\n", jpeg_decoder->bitstream_buffer_size, encoded_frame.data_size);
                return ret;
            } else {
                handle = bm_jpu_enc_get_bm_handle(jpeg_decoder->device_index);
                bm_free_device_mem(handle, jpeg_decoder->bitstream_buffer_addr);
                jpeg_decoder->bitstream_buffer_addr = 0;

                bs_size = (encoded_frame.data_size+BS_MASK)&(~BS_MASK);
                bmlib_ret = bm_jpu_malloc_device_mem_heap_mask(handle, &jpeg_decoder->bitstream_buffer_addr, HEAP_1_2, bs_size);
                if (bmlib_ret != 0) {
                    BM_JPU_ERROR("could not allocate DMA buffer for bitstream buffer with %u bytes. id=%lu",
                                encoded_frame.data_size, get_cur_threadid());
                    return BM_JPU_DEC_RETURN_ALLOC_MEM_ERROR;
                }

                BM_JPU_DEBUG("bs size: %d is less than %d, alloca new bs_buffer: 0x%lx\n", jpeg_decoder->bitstream_buffer_size, encoded_frame.data_size, jpeg_decoder->bitstream_buffer_addr);
                jpeg_decoder->bitstream_buffer_size = bs_size;
                bs_mem = bm_mem_from_device(jpeg_decoder->bitstream_buffer_addr, jpeg_decoder->bitstream_buffer_size);
            }
        }
    } while( (ret == BM_JPU_DEC_RETURN_CODE_TIMEOUT || ret == BM_JPU_DEC_RETURN_ILLEGAL_BS_SIZE) && (count++ < 3) );

    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        if (!jpeg_decoder->framebuffer_recycle) {
            bm_jpu_jpeg_dec_deallocate_framebuffers(jpeg_decoder);
        }
        return ret;
    }
    if (output_code & BM_JPU_DEC_OUTPUT_CODE_DECODED_FRAME_AVAILABLE) {
        ret = bm_jpu_dec_get_decoded_frame(jpeg_decoder->decoder, &(jpeg_decoder->raw_frame));
        if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
            if (!jpeg_decoder->framebuffer_recycle) {
                bm_jpu_jpeg_dec_deallocate_framebuffers(jpeg_decoder);
            }
            return ret;
        }
    }
    else {
        jpeg_decoder->raw_frame.framebuffer = NULL;
    }

    return BM_JPU_DEC_RETURN_CODE_OK;
}

BmJpuDecReturnCodes bm_jpu_jpeg_dec_get_info(BmJpuJPEGDecoder *jpeg_decoder, BmJpuJPEGDecInfo *info)
{
    if ((jpeg_decoder == NULL) || (info == NULL) ) {
        BM_JPU_ERROR("bm_jpu_jpeg_dec_get_info params err: jpeg_decoder(0X%lx), info(0X%lx)", jpeg_decoder, info);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    if (jpeg_decoder->framebuffers == NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_dec_get_info params err: jpeg_decoder->framebuffers(0X%lx)", jpeg_decoder->framebuffers);
        return BM_JPU_DEC_RETURN_CODE_INVALID_FRAMEBUFFER;
    }

    info->aligned_frame_width  = jpeg_decoder->calculated_sizes.aligned_frame_width;
    info->aligned_frame_height = jpeg_decoder->calculated_sizes.aligned_frame_height;

    info->actual_frame_width  = jpeg_decoder->initial_info.frame_width;
    info->actual_frame_height = jpeg_decoder->initial_info.frame_height;

    info->y_size    = jpeg_decoder->calculated_sizes.y_size;
    info->cbcr_size = jpeg_decoder->calculated_sizes.cbcr_size;

    // TODO: support to get framebuffer info by index?
    info->framebuffer = jpeg_decoder->raw_frame.framebuffer;

    info->y_stride    = info->framebuffer->y_stride;
    info->cbcr_stride = info->framebuffer->cbcr_stride;

    info->y_offset  = info->framebuffer->y_offset;
    info->cb_offset = info->framebuffer->cb_offset;
    info->cr_offset = info->framebuffer->cr_offset;

    info->image_format = jpeg_decoder->calculated_sizes.image_format;

    info->framebuffer_recycle = jpeg_decoder->framebuffer_recycle;
    info->framebuffer_size = jpeg_decoder->calculated_sizes.total_size;

    return BM_JPU_DEC_RETURN_CODE_OK;
}


BmJpuDecReturnCodes bm_jpu_jpeg_dec_frame_finished(BmJpuJPEGDecoder *jpeg_decoder, BmJpuFramebuffer *framebuffer)
{
    if ((jpeg_decoder == NULL) || (framebuffer == NULL)){
        BM_JPU_ERROR("bm_jpu_jpeg_dec_frame_finished params err: jpeg_decoder(0X%lx), framebuffer(0X%lx).", jpeg_decoder, framebuffer);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }
    if (jpeg_decoder->decoder == NULL){
        BM_JPU_ERROR("bm_jpu_jpeg_dec_frame_finished params err: jpeg_decoder->decoder(0X%lx)", jpeg_decoder->decoder);
        return BM_JPU_DEC_RETURN_CODE_INVALID_PARAMS;
    }

    return bm_jpu_dec_mark_framebuffer_as_displayed(jpeg_decoder->decoder, framebuffer);
}

BmJpuDecReturnCodes bm_jpu_jpeg_dec_flush(BmJpuJPEGDecoder *jpeg_decoder)
{
    if (jpeg_decoder && jpeg_decoder->decoder)
        return bm_jpu_dec_flush(jpeg_decoder->decoder);

    return BM_JPU_DEC_RETURN_CODE_OK; // TODO
}



/******************
 ** JPEG ENCODER **
 ******************/

static BmJpuEncReturnCodes bm_jpu_jpeg_enc_open_internal(BmJpuJPEGEncoder *jpeg_encoder, int timeout, int timeout_count);
static BmJpuEncReturnCodes bm_jpu_jpeg_enc_close_internal(BmJpuJPEGEncoder *jpeg_encoder);


static BmJpuEncReturnCodes bm_jpu_jpeg_enc_open_internal(BmJpuJPEGEncoder *jpeg_encoder, int timeout, int timeout_count)
{
    BmJpuEncOpenParams open_params;
    BmJpuEncReturnCodes ret = BM_JPU_ENC_RETURN_CODE_OK;
    BmJpuColorFormat color_format = BM_JPU_COLOR_FORMAT_YUV420;
    BmJpuPackedFormat packed_format = BM_JPU_PACKED_FORMAT_NONE;
    BmJpuChromaFormat chroma_interleave = BM_JPU_CHROMA_FORMAT_CBCR_SEPARATED;
    bm_device_mem_t dev_mem = BM_MEM_NULL;

    if (jpeg_encoder == NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_enc_open_internal params err: jpeg_encoder(0X%lx).", jpeg_encoder);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (jpeg_encoder->encoder != NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_enc_open_internal: encoder has been opened, do not call it again.");
        return BM_JPU_ENC_RETURN_CODE_ALREADY_CALLED;
    }

    if (!(jpeg_encoder->frame_width > 0) || !(jpeg_encoder->frame_height > 0)) {
        BM_JPU_ERROR("bm_jpu_jpeg_enc_open_internal params err: jpeg_encoder->frame_width(%d), jpeg_encoder->frame_height(%d)", jpeg_encoder->frame_width, jpeg_encoder->frame_height);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    ret = bm_jpu_enc_set_default_open_params(&open_params);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK) {
        return ret;
    }

    // convert image format to color format, packed format and cbcr interleave
    bm_jpu_convert_from_image_format(jpeg_encoder->image_format, &color_format, &chroma_interleave, &packed_format);

    open_params.frame_width  = jpeg_encoder->frame_width;
    open_params.frame_height = jpeg_encoder->frame_height;
    open_params.color_format = color_format;
    open_params.quality_factor = jpeg_encoder->quality_factor;
    open_params.packed_format  = packed_format;
    open_params.chroma_interleave = chroma_interleave;
    open_params.device_index = jpeg_encoder->device_index;

    open_params.rotationEnable  = jpeg_encoder->rotationEnable;
    open_params.rotationAngle   = jpeg_encoder->rotationAngle;
    open_params.mirrorEnable    = jpeg_encoder->mirrorEnable;
    open_params.mirrorDirection = jpeg_encoder->mirrorDirection;

    open_params.timeout = timeout;
    open_params.timeout_count = timeout_count;

    dev_mem = bm_mem_from_device(jpeg_encoder->bitstream_buffer_addr, jpeg_encoder->bitstream_buffer_size);

    ret = bm_jpu_enc_open(&(jpeg_encoder->encoder), &open_params, &dev_mem);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        goto error;

    ret = bm_jpu_enc_get_initial_info(jpeg_encoder->encoder, &(jpeg_encoder->initial_info));
    if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        goto error;

    ret = bm_jpu_calc_framebuffer_sizes(jpeg_encoder->frame_width,
                                  jpeg_encoder->frame_height,
                                  jpeg_encoder->initial_info.framebuffer_alignment,
                                  jpeg_encoder->image_format,
                                  &(jpeg_encoder->calculated_sizes));

    return ret;

error:
    bm_jpu_jpeg_enc_close_internal(jpeg_encoder);

    return ret;
}


static BmJpuEncReturnCodes bm_jpu_jpeg_enc_close_internal(BmJpuJPEGEncoder *jpeg_encoder)
{
    if (jpeg_encoder == NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_enc_close_internal params err: jpeg_encoder(0X%lx).", jpeg_encoder);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (jpeg_encoder->encoder != NULL) {
        bm_jpu_enc_close(jpeg_encoder->encoder);
        jpeg_encoder->encoder = NULL;
    }

    return BM_JPU_ENC_RETURN_CODE_OK;
}

#if !defined(_WIN32)
int bm_jpu_jpeg_get_dump()
{
    int ret = bm_jpu_get_dump();
    return ret;
}
#endif

BmJpuEncReturnCodes bm_jpu_jpeg_enc_open(BmJpuJPEGEncoder **jpeg_encoder,
                                         bm_jpu_phys_addr_t bs_buffer_phys_addr,
                                         int bs_buffer_size,
                                         int device_index)
{
    BmJpuJPEGEncoder *jpegenc = NULL;
    bm_status_t ret = BM_SUCCESS;
    bm_handle_t handle = NULL;

    if (jpeg_encoder == NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_enc_open params err: jpeg_encoder(0X%lx).", jpeg_encoder);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    jpegenc = BM_JPU_ALLOC(sizeof(BmJpuJPEGEncoder));
    if (jpegenc == NULL) {
        BM_JPU_ERROR("allocating memory for JPEG encoder object failed. id=%lu", get_cur_threadid());
        return BM_JPU_ENC_RETURN_ALLOC_MEM_ERROR;
    }

    memset(jpegenc, 0, sizeof(BmJpuJPEGEncoder));
    jpegenc->device_index = device_index;

    if (bs_buffer_phys_addr != 0) {
        if (bs_buffer_size <= 0 || (bs_buffer_size & BS_MASK) != 0) {
            BM_JPU_ERROR("input bitstream buffer size (%d) is invalid, should be larger than 0 and align with %d", bs_buffer_size, BS_MASK+1);
            BM_JPU_FREE(jpegenc, sizeof(BmJpuJPEGEncoder));
            return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
        }

        jpegenc->bitstream_buffer_addr = bs_buffer_phys_addr;
        jpegenc->bitstream_buffer_size = bs_buffer_size;
        jpegenc->bitstream_buffer_alignment = JPU_MEMORY_ALIGNMENT;
        jpegenc->bitstream_from_user = 1;
    }
    else {
        if (bs_buffer_size <= 0) {
            bm_jpu_enc_get_bitstream_buffer_info(&(jpegenc->bitstream_buffer_size), &(jpegenc->bitstream_buffer_alignment));
        }
        else {
            bs_buffer_size = (bs_buffer_size+BS_MASK)&(~BS_MASK);

            jpegenc->bitstream_buffer_size = bs_buffer_size;
            jpegenc->bitstream_buffer_alignment = JPU_MEMORY_ALIGNMENT;
        }

        handle = bm_jpu_enc_get_bm_handle(jpegenc->device_index);
        ret = bm_jpu_malloc_device_mem_heap_mask(handle, &jpegenc->bitstream_buffer_addr, HEAP_1_2, jpegenc->bitstream_buffer_size);
        if (ret != BM_SUCCESS) {
            BM_JPU_ERROR("could not allocate DMA buffer for bitstream buffer with %u bytes and alignment %u. id=%lu",
                        jpegenc->bitstream_buffer_size,
                        jpegenc->bitstream_buffer_alignment,
                        get_cur_threadid());
            BM_JPU_FREE(jpegenc, sizeof(BmJpuJPEGEncoder));
            return BM_JPU_ENC_RETURN_ALLOC_MEM_ERROR;
        }
    }

    BM_JPU_DEBUG("bitstream_from_user=%d, addr=%#lx, size=%u, alignment=%u", jpegenc->bitstream_from_user,
                jpegenc->bitstream_buffer_addr, jpegenc->bitstream_buffer_size, jpegenc->bitstream_buffer_alignment);
    /* bm_jpu_enc_open() is called later on demand during encoding, to accomodate
     * for potentially changing parameters like width, height, quality factor */

    *jpeg_encoder = jpegenc;

    return BM_JPU_ENC_RETURN_CODE_OK;
}


BmJpuEncReturnCodes bm_jpu_jpeg_enc_close(BmJpuJPEGEncoder *jpeg_encoder)
{
    bm_handle_t handle = NULL;

    if (jpeg_encoder == NULL) {
        BM_JPU_ERROR("bm_jpu_jpeg_enc_close params err: jpeg_encoder(0X%lx).", jpeg_encoder);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    bm_jpu_jpeg_enc_close_internal(jpeg_encoder);

    if (jpeg_encoder->bitstream_buffer_addr != 0) {
        if (!jpeg_encoder->bitstream_from_user) {
            handle = bm_jpu_enc_get_bm_handle(jpeg_encoder->device_index);
            bm_free_device_mem(handle, jpeg_encoder->bitstream_buffer_addr);
        }
        jpeg_encoder->bitstream_buffer_addr = 0;
    }

    BM_JPU_FREE(jpeg_encoder, sizeof(BmJpuJPEGEncoder));

    return BM_JPU_ENC_RETURN_CODE_OK;
}

BmJpuEncReturnCodes bm_jpu_jpeg_enc_encode(BmJpuJPEGEncoder *jpeg_encoder,
                                           BmJpuFramebuffer const *framebuffer,
                                           BmJpuJPEGEncParams const *params,
                                           void **acquired_handle,
                                           size_t *output_buffer_size)
{
    unsigned int output_code;
    BmJpuEncParams enc_params;
    BmJpuEncReturnCodes ret;
    BmJpuRawFrame raw_frame;
    BmJpuEncodedFrame encoded_frame;
    int count = 0;
    bm_handle_t handle = NULL;

    if ((jpeg_encoder == NULL) || (framebuffer == NULL) || (params == NULL)){
        BM_JPU_ERROR("bm_jpu_jpeg_enc_encode params err: jpeg_encoder(0X%lx), framebuffer(0X%lx), params(0X%lx).", jpeg_encoder, framebuffer, params);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (params->bs_buffer_size != 0 && (params->bs_buffer_size & BS_MASK) != 0) {
        BM_JPU_ERROR("input bitstream buffer size (%d) is invalid, should be larger than 0 and align with %d", params->bs_buffer_size, BS_MASK+1);
        return BM_JPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (acquired_handle != NULL)
        *acquired_handle = NULL;

    BM_JPU_DEBUG("jpeg_encoder->device_index      = %d", jpeg_encoder->device_index);
    BM_JPU_DEBUG("jpeg_encoder->encoder           = %p", (void*)(jpeg_encoder->encoder));
    BM_JPU_DEBUG("jpeg_encoder->frame_width       = %d", jpeg_encoder->frame_width);
    BM_JPU_DEBUG("jpeg_encoder->frame_height      = %d", jpeg_encoder->frame_height);
    BM_JPU_DEBUG("jpeg_encoder->quality_factor    = %d", jpeg_encoder->quality_factor);
    BM_JPU_DEBUG("jpeg_encoder->image_format      = %d", jpeg_encoder->image_format);

    BM_JPU_DEBUG("params->frame_width    = %d", params->frame_width);
    BM_JPU_DEBUG("params->frame_height   = %d", params->frame_height);
    BM_JPU_DEBUG("params->quality_factor = %d", params->quality_factor);
    BM_JPU_DEBUG("params->image_format   = %d", params->image_format);

    /* Need to reopen internal encoder in following situations:
     * 1. internal encoder is none.
     * 2. encode params changed.
     * 3. user set the device memory of bitstream, if the physical address is changed or the size is larger than current.
     */
    if ((jpeg_encoder->encoder == NULL) ||
        (jpeg_encoder->frame_width       != params->frame_width) ||
        (jpeg_encoder->frame_height      != params->frame_height) ||
        (jpeg_encoder->quality_factor    != params->quality_factor) ||
        (jpeg_encoder->image_format      != params->image_format) ||
        (params->bs_buffer_phys_addr != 0 && jpeg_encoder->bitstream_buffer_addr != params->bs_buffer_phys_addr) ||
        (params->bs_buffer_size != 0 && jpeg_encoder->bitstream_buffer_size < params->bs_buffer_size))
    {
        bm_jpu_jpeg_enc_close_internal(jpeg_encoder);

        jpeg_encoder->frame_width       = params->frame_width;
        jpeg_encoder->frame_height      = params->frame_height;
        jpeg_encoder->quality_factor    = params->quality_factor;
        jpeg_encoder->image_format      = params->image_format;
        jpeg_encoder->rotationEnable    = params->rotationEnable;
        jpeg_encoder->rotationAngle     = params->rotationAngle;
        jpeg_encoder->mirrorEnable      = params->mirrorEnable;
        jpeg_encoder->mirrorDirection   = params->mirrorDirection;

        if (params->bs_buffer_phys_addr) {
            if (!jpeg_encoder->bitstream_from_user && jpeg_encoder->bitstream_buffer_addr != 0) {
                handle = bm_jpu_enc_get_bm_handle(jpeg_encoder->device_index);
                bm_free_device_mem(handle, jpeg_encoder->bitstream_buffer_addr);
                BM_JPU_DEBUG("bitstream buffer has been released, addr=%#lx, size=%zu", jpeg_encoder->bitstream_buffer_addr, jpeg_encoder->bitstream_buffer_size);
            }

            jpeg_encoder->bitstream_buffer_addr = params->bs_buffer_phys_addr;
            jpeg_encoder->bitstream_buffer_size = params->bs_buffer_size;
            jpeg_encoder->bitstream_buffer_alignment = JPU_MEMORY_ALIGNMENT;
            jpeg_encoder->bitstream_from_user = 1;

            BM_JPU_DEBUG("bitstream buffer is from user, addr=%#lx, size=%zu", jpeg_encoder->bitstream_buffer_addr, jpeg_encoder->bitstream_buffer_size);
        }

        ret = bm_jpu_jpeg_enc_open_internal(jpeg_encoder, params->timeout, params->timeout_count);
        if (ret != BM_JPU_ENC_RETURN_CODE_OK)
            return ret;
    }

    bm_jpu_enc_set_interrupt_timeout(jpeg_encoder->encoder, params->timeout, params->timeout_count);

    memset(&enc_params, 0, sizeof(enc_params));
    enc_params.acquire_output_buffer = params->acquire_output_buffer;
    enc_params.finish_output_buffer  = params->finish_output_buffer;
    enc_params.write_output_data     = params->write_output_data;
    enc_params.output_buffer_context = params->output_buffer_context;
    enc_params.bs_in_device          = params->bs_in_device;

    memset(&raw_frame, 0, sizeof(raw_frame));
    raw_frame.framebuffer = (BmJpuFramebuffer *)framebuffer;

    memset(&encoded_frame, 0, sizeof(encoded_frame));

    do{
        ret = bm_jpu_enc_encode(jpeg_encoder->encoder, &raw_frame, &encoded_frame, &enc_params, &output_code);
    }while( ( ret == BM_JPU_ENC_BYTE_ERROR || ret == BM_JPU_ENC_RETURN_CODE_TIMEOUT ) && (count++ < 3) );

    if (acquired_handle != NULL)
        *acquired_handle = encoded_frame.acquired_handle;
    if (output_buffer_size != NULL)
        *output_buffer_size = encoded_frame.data_size;

    return ret;
}

int bm_jpu_hw_reset()
{
    return bm_jpu_hwreset();
}
