#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "jpeg_common.h"

static void usage(char *progname);

int main(int argc, char *argv[])
{
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    int device_index = 0;
    int bs_size = 0;
    int fb_recycle = 0;
    int pic_num = 0;
    char *input_filename = NULL;
    char output_filename[256] = {0};
    int ret = 0;
    int i = 0;
    BmJpuDecOpenParams open_params;
    BmJpuJPEGDecoder *jpeg_decoder = NULL;
    uint8_t* bs_buf = NULL;
    int input_size = 0;
    int fb_size = 0;
    bm_handle_t bm_handle = NULL;
    bm_device_mem_t bitstream_buffer = {0};
    bm_device_mem_t frame_buffer[MAX_FRAME_COUNT] = {0};
    unsigned long long frame_buffer_phys_addr[MAX_FRAME_COUNT] = {0};
    BmJpuJPEGDecInfo info[MAX_FRAME_COUNT] = {0};
    size_t num_out_byte = 0;
    uint8_t *mapped_virtual_address = NULL;
    int idx = 0;

    if (argc < 7) {
        fprintf(stderr, "Missing option argument\n\n");
        usage(argv[0]);
        return -1;
    }

    device_index = atoi(argv[1]);
    if (device_index < 0) {
        fprintf(stderr, "Invalid device index: %d, should not be less than 0\n\n", device_index);
        usage(0);
        return -1;
    }

    fb_recycle = atoi(argv[2]);

    bs_size = atoi(argv[3]);
    if (bs_size <= 0) {
        fprintf(stderr, "Invalid bitstream size: %d, should be larger than 0\n\n", bs_size);
        usage(argv[0]);
        return -1;
    }
    bs_size = (bs_size + BS_SIZE_MASK - 1) & ~(BS_SIZE_MASK - 1);

    fb_size = atoi(argv[4]);
    if (fb_size <= 0) {
        fprintf(stderr, "Invalid framebuffer size: %d, should be larger than 0\n\n", fb_size);
        usage(argv[0]);
        return -1;
    }

    pic_num = atoi(argv[5]);
    if (pic_num <= 0) {
        fprintf(stderr, "Invalid picture num: %d, should be larger than 0\n\n", pic_num);
        usage(argv[0]);
        return -1;
    }

    if (pic_num > MAX_FRAME_COUNT) {
        pic_num = MAX_FRAME_COUNT;
    }

    ret = bm_dev_request(&bm_handle, device_index);
    if (ret != BM_SUCCESS) {
        fprintf(stderr, "failed to request device %d\n", device_index);
        ret = -1;
        goto cleanup;
    }

    ret = bm_malloc_device_byte_heap_mask(bm_handle, &bitstream_buffer, HEAP_MASK, bs_size);
    if (ret != BM_SUCCESS) {
        fprintf(stderr, "failed to malloc bitstream buffer, size=%d\n", bs_size);
        ret = -1;
        goto cleanup;
    }

    for (i = 0; i < pic_num; i++) {
        ret = bm_malloc_device_byte_heap_mask(bm_handle, &frame_buffer[i], HEAP_MASK, fb_size);
        if (ret != BM_SUCCESS) {
            fprintf(stderr, "failed to malloc frame buffer, size=%d\n", fb_size);
            ret = -1;
            goto cleanup;
        }
        frame_buffer_phys_addr[i] = frame_buffer[i].u.device.device_addr;
    }

    bm_jpu_set_logging_threshold(BM_JPU_LOG_LEVEL_ERROR);
    bm_jpu_set_logging_function(logging_fn);

    /* Load JPU */
    ret = bm_jpu_dec_load(device_index);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        ret = -1;
        goto cleanup;
    }

    memset(&open_params, 0, sizeof(BmJpuDecOpenParams));
    open_params.min_frame_width = 0;
    open_params.min_frame_height = 0;
    open_params.max_frame_width = 0;
    open_params.max_frame_height = 0;
    open_params.chroma_interleave = BM_JPU_CHROMA_FORMAT_CBCR_SEPARATED;
    open_params.bs_buffer_size = bs_size;
    open_params.device_index = device_index;
    open_params.bitstream_from_user = 1;
    open_params.bs_buffer_phys_addr = bitstream_buffer.u.device.device_addr;
    open_params.framebuffer_from_user = 1;
    open_params.framebuffer_num = pic_num;
    open_params.framebuffer_phys_addrs = frame_buffer_phys_addr;
    open_params.framebuffer_recycle = fb_recycle;
    open_params.framebuffer_size = fb_size;

    /* Open the JPEG decoder */
    ret = bm_jpu_jpeg_dec_open(&jpeg_decoder, &open_params, 0);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        ret = -1;
        goto cleanup;
    }

    for (idx = 0; idx < pic_num; idx++) {
        input_filename = argv[6 + idx];
        if (input_filename == NULL) {
            fprintf(stderr, "Missing input filename\n\n");
            usage(argv[0]);
            ret = -1;
            goto cleanup;
        }

        input_file = fopen(input_filename, "rb");
        if (input_file == NULL) {
            fprintf(stderr, "Opening %s for reading failed: %s\n", input_filename, strerror(errno));
            ret = -1;
            goto cleanup;
        }

        if (bs_buf != NULL) {
            free(bs_buf);
            bs_buf = NULL;
        }

        fseek(input_file, 0, SEEK_END);
        input_size = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);

        bs_buf = (uint8_t *)malloc(input_size);
        if (bs_buf == NULL) {
            fprintf(stderr, "malloc failed\n");
            ret = -1;
            goto cleanup;
        }

        if (fread(bs_buf, 1, input_size, input_file) < (size_t)input_size) {
            fprintf(stderr, "failed to read in whole file!\n");
            ret = -1;
            goto cleanup;
        }

        fclose(input_file);
        input_file = NULL;

        /* Perform the actual JPEG decoding */
        ret = bm_jpu_jpeg_dec_decode(jpeg_decoder, bs_buf, input_size, 0, 0);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
            fprintf(stderr, "could not decode this JPEG image : %s\n", bm_jpu_dec_error_string(ret));
            ret = -1;
            goto cleanup;
        }

        /* Get some information about the frame
        * Note that the info is only available *after* calling bm_jpu_jpeg_dec_decode() */
        bm_jpu_jpeg_dec_get_info(jpeg_decoder, &info[idx]);

        printf("aligned frame size: %u x %u\n"
            "pixel actual frame size: %u x %u\n"
            "pixel Y/Cb/Cr stride: %u/%u/%u\n"
            "pixel Y/Cb/Cr size: %u/%u/%u\n"
            "pixel Y/Cb/Cr offset: %u/%u/%u\n"
            "image format: %s\n",
            info[idx].aligned_frame_width, info[idx].aligned_frame_height,
            info[idx].actual_frame_width, info[idx].actual_frame_height,
            info[idx].y_stride, info[idx].cbcr_stride, info[idx].cbcr_stride,
            info[idx].y_size, info[idx].cbcr_size, info[idx].cbcr_size,
            info[idx].y_offset, info[idx].cb_offset, info[idx].cr_offset,
            bm_jpu_color_format_string(info[idx].image_format));

        if (info[idx].framebuffer == NULL) {
            fprintf(stderr, "could not decode this JPEG image : no framebuffer returned\n");
            bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info[idx].framebuffer);
            ret = -1;
            goto cleanup;
        }

        sprintf(output_filename, "dec_out_%d.yuv", idx);
        output_file = fopen(output_filename, "wb");
        if (output_file == NULL) {
            fprintf(stderr, "Opening %s for writing failed: %s\n", output_filename, strerror(errno));
            bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info[idx].framebuffer);
            ret = -1;
            goto cleanup;
        }

        /* Map the DMA buffer of the decoded picture, write out the decoded pixels, and unmap the buffer again */
        // num_out_byte = info[idx].y_size + (info[idx].chroma_interleave ? info[idx].cbcr_size : info[idx].cbcr_size * 2);
        num_out_byte = info[idx].framebuffer_size;
        fprintf(stderr, "decoded output picture: writing %zu byte\n", num_out_byte);

    #ifdef BM_PCIE_MODE
        int yuv_size = info[idx].framebuffer->dma_buffer->size;
        uint8_t *yuv_data = (uint8_t *)malloc(yuv_size);
        if (!yuv_data) {
            fprintf(stderr, "malloc yuv_data for pcie mode failed\n");
            bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info[idx].framebuffer);
            ret = -1;
            goto cleanup;
        }
        mapped_virtual_address = (uint8_t*)yuv_data;
        ret = bm_memcpy_d2s(bm_handle, yuv_data, *(info[idx].framebuffer->dma_buffer));
    #else
        unsigned long long p_vaddr = 0;
        ret = bm_mem_mmap_device_mem(bm_handle, info[idx].framebuffer->dma_buffer, &p_vaddr);
        bm_mem_invalidate_device_mem(bm_handle, info[idx].framebuffer->dma_buffer);
        mapped_virtual_address = (uint8_t*)p_vaddr;
        if (mapped_virtual_address == NULL) {
            fprintf(stderr, "bm_jpu_dma_buffer_map failed\n");
            bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info[idx].framebuffer);
            ret = -1;
            goto cleanup;
        }
    #endif

        DumpCropFrame(&info[idx], mapped_virtual_address, output_file);

    #ifdef BM_PCIE_MODE
        if (yuv_data) {
            free(yuv_data);
            yuv_data = NULL;
        }
    #else
        bm_mem_unmap_device_mem(bm_handle, mapped_virtual_address, info[idx].framebuffer->dma_buffer->size);
    #endif

        bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info[idx].framebuffer);
    }

cleanup:
    if (jpeg_decoder != NULL) {
        /* Shut down the JPEG decoder */
        bm_jpu_jpeg_dec_close(jpeg_decoder);
        jpeg_decoder = NULL;
    }

    /* Unload JPU */
    bm_jpu_dec_unload(device_index);

    if (bm_handle != NULL) {
        bm_free_device(bm_handle, bitstream_buffer);

        for (i = 0; i < pic_num; i++) {
            bm_free_device(bm_handle, frame_buffer[i]);
        }

        bm_dev_free(bm_handle);
        bm_handle = NULL;
    }

    /* Input data is not needed anymore, so free the input buffer */
    if (bs_buf != NULL) {
        free(bs_buf);
        bs_buf = NULL;
    }

    if (input_file != NULL) {
        fclose(input_file);
        input_file = NULL;
    }

    if (output_file != NULL) {
        fclose(output_file);
        output_file = NULL;
    }

    return ret;
}

static void usage(char *progname)
{
    static char options[] =
        "Usage:\n"
        "\t%s [device_index] [fb_recycle] [bs_size] [fb_size] [pic_num] [input_filename] ...\n"
        "Params:\n"
        "\t[device_index]:      choose a device (valid in pice mode, will be ignore in soc mode)\n"
        "\t[fb_recycle]:        reuse the framebuffer when input resolution changed [0: no, 1: yes]\n"
        "\t[bs_size]:           bitstream buffer size\n"
        "\t[fb_size]:           framebuffer size\n"
        "\t[pic_num]:           input picture number\n"
        "\t[input_filename]:    input file\n"
        ;

    fprintf(stderr, options, progname);
}
