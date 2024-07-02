#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "jpeg_common.h"

static void usage(char *progname);

static void* acquire_output_buffer(void *context, size_t size, void **acquired_handle)
{
    ((void)(context));
    void *mem;

    /* In this example, "acquire" a buffer by simply allocating it with malloc() */
    mem = malloc(size);
    *acquired_handle = mem;
    return mem;
}

static void finish_output_buffer(void *context, void *acquired_handle)
{
    ((void)(context));
}

int main(int argc, char *argv[])
{
    BmJpuJPEGEncoder *jpeg_encoder = NULL;
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    int device_index = 0;
    int bs_size = 0;
    int external_mem = 0;
    int pic_num = 0;
    char *input_filename = NULL;
    char output_filename[256] = {0};
    int format = 0;
    int cbcr_interleave = 0;
    int width = 0;
    int height = 0;
    int ret = 0;
    bm_handle_t bm_handle = NULL;
    bm_device_mem_t *bitstream_buffer = NULL;
    int i = 0;
    int y_stride = 0;
    int c_stride = 0;
    bm_handle_t handle;
    BmJpuFramebuffer framebuffer;
    BmJpuJPEGEncParams enc_params;
    void *acquired_handle = NULL;
    size_t output_buffer_size;
    int frame_y_size;
    int frame_c_size;
    int frame_total_size;

    if (argc < 10) {
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

    bs_size = atoi(argv[2]);
    if (bs_size <= 0) {
        fprintf(stderr, "Invalid bitstream size: %d, should be larger than 0\n\n", bs_size);
        usage(argv[0]);
        return -1;
    }
    bs_size = (bs_size + BS_SIZE_MASK - 1) & ~(BS_SIZE_MASK - 1);

    external_mem = atoi(argv[3]);
    if (external_mem < 0 || external_mem > 1) {
        fprintf(stderr, "Invalid external memory: %d, should be 0 or 1\n\n", external_mem);
        usage(argv[0]);
        return -1;
    }

    pic_num = atoi(argv[4]);
    if (pic_num <= 0) {
        fprintf(stderr, "Invalid picture num: %d, should be larger than 0\n\n", pic_num);
        usage(argv[0]);
        return -1;
    }

    if (external_mem) {
        ret = bm_dev_request(&bm_handle, device_index);
        if (ret != BM_SUCCESS) {
            fprintf(stderr, "failed to request device %d\n", device_index);
            ret = -1;
            goto cleanup;
        }

        bitstream_buffer = (bm_device_mem_t *)malloc(sizeof(bm_device_mem_t));
        if (bitstream_buffer == NULL) {
            fprintf(stderr, "malloc failed!\n");
            ret = -1;
            goto cleanup;
        }

        ret = bm_malloc_device_byte_heap_mask(bm_handle, bitstream_buffer, HEAP_MASK, bs_size);
        if (ret != BM_SUCCESS) {
            fprintf(stderr, "failed to malloc bitstream buffer, size=%d\n", bs_size);
            ret = -1;
            goto cleanup;
        }
    }

    bm_jpu_set_logging_threshold(BM_JPU_LOG_LEVEL_ERROR);
    bm_jpu_set_logging_function(logging_fn);

    /* Load JPU */
    ret = bm_jpu_enc_load(device_index);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK) {
        ret = -1;
        goto cleanup;
    }

    /* Open the JPEG encoder */
    ret = bm_jpu_jpeg_enc_open(&(jpeg_encoder), external_mem ? bitstream_buffer->u.device.device_addr : 0, bs_size, device_index);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK) {
        ret = -1;
        goto cleanup;
    }

    handle = bm_jpu_enc_get_bm_handle(device_index);
    for (i = 0; i < pic_num; i++) {
        input_filename = argv[5 + 5 * i];
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

        format = atoi(argv[5 + 5 * i + 1]);
        switch (format) {
            case 0:
                format = BM_JPU_COLOR_FORMAT_YUV420;
                break;
            case 1:
                format = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
                break;
            case 2:
                format = BM_JPU_COLOR_FORMAT_YUV444;
                break;
            case 3:
                format = BM_JPU_COLOR_FORMAT_YUV400;
                break;
            default:
                fprintf(stderr, "Invalid params: unknown pixel format %d\n\n", format);
                usage(argv[0]);
                ret = -1;
                goto cleanup;
        }

        cbcr_interleave = atoi(argv[5 + 5 * i + 2]);

        width = atoi(argv[5 + 5 * i + 3]);
        if (width <= 0 || width % 16 != 0) {
            fprintf(stderr, "Invalid params: width should be larger than 0 and aligned with 16\n\n");
            usage(argv[0]);
            ret = -1;
            goto cleanup;
        }

        height = atoi(argv[5 + 5 * i + 4]);
        if (height <= 0 || height % 16 != 0) {
            fprintf(stderr, "Invalid params: height should be larger than 0 and aligned with 16\n\n");
            usage(argv[0]);
            ret = -1;
            goto cleanup;
        }

        y_stride = width;
        c_stride = (width + 1) / 2;
        frame_y_size = y_stride * height;
        if (format == BM_JPU_COLOR_FORMAT_YUV400) {
            frame_c_size = 0;
        } else if (format == BM_JPU_COLOR_FORMAT_YUV420) {
            frame_c_size = c_stride * ((height + 1) / 2);
        } else {
            frame_c_size = c_stride * height;
        }
        frame_total_size = frame_y_size + frame_c_size * 2;

        /* Initialize the input framebuffer */
        memset(&framebuffer, 0, sizeof(framebuffer));
        framebuffer.y_stride = y_stride;
        framebuffer.y_offset = 0;
        if (cbcr_interleave == 0) {
            framebuffer.cbcr_stride = c_stride;
            framebuffer.cb_offset   = frame_y_size;
            framebuffer.cr_offset   = frame_y_size + frame_c_size;
        } else if (cbcr_interleave == 1) {
            framebuffer.cbcr_stride = c_stride * 2;
            framebuffer.cb_offset   = frame_y_size;
            framebuffer.cr_offset   = frame_y_size + 1;
        } else {
            framebuffer.cbcr_stride = c_stride * 2;
            framebuffer.cb_offset   = frame_y_size + 1;
            framebuffer.cr_offset   = frame_y_size;
        }

        framebuffer.dma_buffer = (bm_device_mem_t *)malloc(sizeof(bm_device_mem_t));
        ret = bm_malloc_device_byte_heap_mask(handle, framebuffer.dma_buffer, HEAP_MASK, frame_total_size);
        if (ret != BM_SUCCESS) {
            fprintf(stderr, "malloc device memory size = %d failed, ret = %d\n", frame_total_size, ret);
            ret = -1;
            goto cleanup;
        }

        /* Load the input pixels into the DMA buffer */
    #ifdef BM_PCIE_MODE
        uint8_t *input_data = (uint8_t *)calloc(frame_total_size, sizeof(uint8_t));
        if (!input_data) {
            fprintf(stderr, "can't alloc memory to save input data\n");
            ret = -1;
            goto cleanup;
        }

        if (fread(input_data, sizeof(uint8_t), frame_total_size, input_file) < frame_total_size) {
            fprintf(stderr, "failed to read in %d byte of data\n", frame_total_size);
            free(input_data);
            ret = -1;
            goto cleanup;
        }

        bm_memcpy_s2d_partial(handle, *(framebuffer.dma_buffer), input_data, frame_total_size);

        if (input_data != NULL) {
            free(input_data);
            input_data = NULL;
        }
    #else
        unsigned long long pmap_addr = 0;
        uint8_t *virt_addr = NULL;
        bm_mem_mmap_device_mem(handle, framebuffer.dma_buffer, &pmap_addr);
        virt_addr = (uint8_t*)pmap_addr;
        if (fread(virt_addr, sizeof(uint8_t), frame_total_size, input_file) < frame_total_size) {
            fprintf(stderr, "failed to read in %d byte of data\n", frame_total_size);
            bm_mem_unmap_device_mem(handle, (void *)virt_addr, framebuffer.dma_buffer->size);
            ret = -1;
            goto cleanup;
        }

        bm_mem_flush_partial_device_mem(handle, framebuffer.dma_buffer, 0, frame_total_size);
        bm_mem_unmap_device_mem(handle, (void *)virt_addr, framebuffer.dma_buffer->size);
    #endif
        fclose(input_file);
        input_file = NULL;

        /* Set up the encoding parameters */
        memset(&enc_params, 0, sizeof(enc_params));
        enc_params.frame_width              = width;
        enc_params.frame_height             = height;
        enc_params.quality_factor           = 85;
        enc_params.acquire_output_buffer    = acquire_output_buffer;
        enc_params.finish_output_buffer     = finish_output_buffer;
        enc_params.output_buffer_context    = NULL;

        ConvertToImageFormat(&enc_params.image_format, (BmJpuColorFormat)format, (BmJpuChromaFormat)cbcr_interleave, BM_JPU_PACKED_FORMAT_NONE);

        /* Do the actual encoding */
        ret = bm_jpu_jpeg_enc_encode(jpeg_encoder, &framebuffer, &enc_params, &acquired_handle, &output_buffer_size);
        if (ret != BM_JPU_ENC_RETURN_CODE_OK) {
            fprintf(stderr, "could not encode this image: %s\n", bm_jpu_enc_error_string(ret));
            ret = -1;
            goto cleanup;
        }

        sprintf(output_filename, "enc_out_%d.jpg", i);
        output_file = fopen(output_filename, "wb");
        if (output_file == NULL) {
            fprintf(stderr, "Opening %s for writing failed: %s\n", output_filename, strerror(errno));
            ret = -1;
            goto cleanup;
        }

        /* Write out the encoded frame to the output file. The encoder
        * will have called acquire_output_buffer(), which acquires a
        * buffer by malloc'ing it. The "handle" in this example is
        * just the pointer to the allocated memory. This means that
        * here, acquired_handle is the pointer to the encoded frame
        * data. Write it to file. In production, the acquire function
        * could retrieve an output memory block from a buffer pool for
        * example. */
        fwrite(acquired_handle, 1, output_buffer_size, output_file);
        fclose(output_file);
        output_file = NULL;

        free(acquired_handle);
        acquired_handle = NULL;
    }

cleanup:
    if (acquired_handle != NULL) {
        free(acquired_handle);
        acquired_handle = NULL;
    }

    if (framebuffer.dma_buffer != NULL) {
        bm_free_device(handle, *framebuffer.dma_buffer);
        framebuffer.dma_buffer = NULL;
    }

    if (jpeg_encoder != NULL) {
        /* Shut down the JPEG encoder */
        bm_jpu_jpeg_enc_close(jpeg_encoder);
        jpeg_encoder = NULL;
    }

    /* Unload JPU */
    bm_jpu_enc_unload(device_index);

    if (bm_handle != NULL) {
        if (bitstream_buffer != NULL) {
            bm_free_device(bm_handle, *bitstream_buffer);
            free(bitstream_buffer);
            bitstream_buffer = NULL;
        }

        bm_dev_free(bm_handle);
        bm_handle = NULL;
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
        "\t%s [device_index] [bs_size] [external_mem] [pic_num] [[input_filename] [format] [cbcr_interleave] [width] [height] ...]\n"
        "Params:\n"
        "\t[device_index]:      choose a device (valid in pice mode, will be ignore in soc mode)\n"
        "\t[bs_size]:           bitstream buffer size\n"
        "\t[external_mem]:      the bitstream buffer is allocated by user [0: no, 1: yes]\n"
        "\t[pic_num]:           input picture number\n"
        "\t[input_filename]:    input file\n"
        "\t[format]:            input picture format [0: YUV420, 1: YUV422, 2: YUV444, 3: YUV400]\n"
        "\t[cbcr_interleave]:   Cb/Cr component is interleaved or not (default 0) [0: planared, 1: Cb/Cr interleaved, 2: Cr/Cb interleaved]\n"
        "\t[width]:             input picture width\n"
        "\t[height]:            input picture height\n"
        ;

    fprintf(stderr, options, progname);
}
