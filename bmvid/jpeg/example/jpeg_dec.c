#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <errno.h>

#include "jpeg_common.h"
#include "bmjpuapi.h"
#include "bmjpuapi_jpeg.h"

typedef struct {
    int x;
    int y;
    int w;
    int h;
}jpu_rect;

static void usage(char *progname);
static int parse_args(int argc, char **argv, char **input_filename,
                      char **output_filename, int* loop_num, int *device_index, int *dumpcrop, int *rotate, int *scale, jpu_rect *roi);
static int run(BmJpuJPEGDecoder* jpeg_decoder, uint8_t* buf, long long size, FILE *fout, int dumpcrop);

int main(int argc, char *argv[])
{
    BmJpuDecOpenParams open_params;
    BmJpuJPEGDecoder *jpeg_decoder = NULL;
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    char *input_filename, *output_filename;
    uint8_t* bs_buf = NULL;
    long long size;
    int loop_num = 1;
    int device_index = 0;
    int i, ret = 0;
    int dumpcrop = 0;
    int rotate = 0;
    int scale = 0;
    jpu_rect roi = {0,0,0,0};
    int roi_enable = 0;

    ret = parse_args(argc, argv, &input_filename, &output_filename, &loop_num, &device_index, &dumpcrop, &rotate, &scale, &roi);
    if (ret != RETVAL_OK)
        return -1;
    if (roi.w && roi.h) roi_enable = 1;

    input_file = fopen(input_filename, "rb");
    if (input_file == NULL)
    {
        fprintf(stderr, "Opening %s for reading failed: %s\n", input_filename, strerror(errno));
        ret = -1;
        goto cleanup;
    }

    output_file = fopen(output_filename, "wb");
    if (output_file == NULL)
    {
        fprintf(stderr, "Opening %s for writing failed: %s\n", output_filename, strerror(errno));
        ret = -1;
        goto cleanup;
    }

    /* Determine size of the input file to be able to read all of its bytes in one go */
    fseek(input_file, 0, SEEK_END);
    size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    printf("encoded input frame:  size: %lld byte\n", size);

    /* Allocate buffer for the input data, and read the data into it */
    bs_buf = (uint8_t*)malloc(size);
    if (bs_buf==NULL)
    {
        fprintf(stderr, "malloc failed!\n");
        ret = -1;
        goto cleanup;
    }

    if (fread(bs_buf, 1, size, input_file) < (size_t)size){
        printf("failed to read in whole file\n");
        ret = -1;
        goto cleanup;
    }

    bm_jpu_set_logging_threshold(BM_JPU_LOG_LEVEL_ERROR);
    bm_jpu_set_logging_function(logging_fn);

    for (i=0; i<loop_num; i++)
    {
        /* Load JPU */
        ret = bm_jpu_dec_load(device_index);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK)
        {
            ret = -1;
            goto cleanup;
        }

        /* Open the JPEG decoder */
        memset(&open_params, 0, sizeof(BmJpuDecOpenParams));
        open_params.frame_width  = 0;
        open_params.frame_height = 0;
        open_params.chroma_interleave = 0;
        open_params.bs_buffer_size = 0;
        open_params.device_index = device_index;
        if (roi_enable){
            open_params.roiEnable = roi_enable;
            open_params.roiWidth = roi.w;
            open_params.roiHeight = roi.h;
            open_params.roiOffsetX = roi.x;
            open_params.roiOffsetY = roi.y;
        }

        if ((rotate & 0x3 ) != 0) {
            open_params.rotationEnable = 1; //
            if ((rotate & 0x3) == 1) {
                open_params.rotationAngle = 90;
            } else if ((rotate & 0x3) == 2) {
                open_params.rotationAngle = 180;
            } else if ((rotate & 0x3) == 3) {
                open_params.rotationAngle = 270;
            }
        }
        if((rotate & 0xc ) != 0) {
            open_params.mirrorEnable = 1;  // --
            if ((rotate & 0xc )>>2 == 1) {
                open_params.mirrorDirection = 1; //
            } else if ((rotate & 0xc )>>2 == 2) {
                open_params.mirrorDirection = 2; //
            } else {
                open_params.mirrorDirection = 3; //
            }
        }
        open_params.scale_ratio = scale;

        ret = bm_jpu_jpeg_dec_open(&jpeg_decoder, &open_params, 0);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK)
        {
            ret = -1;
            goto cleanup;
        }

        fseek(output_file, 0, SEEK_SET);
        ret = run(jpeg_decoder, bs_buf, size, output_file, dumpcrop);
        if (ret == RETVAL_ERROR)
        {
            ret = -1;
            goto cleanup;
        }

        /* Shut down the JPEG decoder */
        bm_jpu_jpeg_dec_close(jpeg_decoder);
        jpeg_decoder = NULL;

        /* Unload JPU */
        bm_jpu_dec_unload(device_index);
    }

cleanup:
    /* Input data is not needed anymore, so free the input buffer */
    if (bs_buf != NULL)
        free(bs_buf);

    if (jpeg_decoder != NULL)
    {
        /* Shut down the JPEG decoder */
        bm_jpu_jpeg_dec_close(jpeg_decoder);
        jpeg_decoder = NULL;

        /* Unload JPU */
        bm_jpu_dec_unload(device_index);
    }

    if (input_file != NULL)
        fclose(input_file);
    if (output_file != NULL)
        fclose(output_file);

    return ret;
}

static void usage(char *progname)
{
    static char options[] =
        "\t-i input file\n"
        "\t-o output file\n"
        "\t-n loop number\n"
#ifdef BM_PCIE_MODE
        "\t-d device index\n"
#endif
        "\t-c crop function(0:disable   1:enable crop.)\n"
        "\t-g  rotate (default 0) [rotate mode[1:0]  0:No rotate  1:90  2:180  3:270] [rotator mode[2]:vertical flip] [rotator mode[3]:horizontal flip]\n"
        "\t-s scale (default 0) -> 0 to 3\n"
        "\t-r roi_x,roi_y,roi_w,roi_h\n"
        ;

    fprintf(stderr, "usage:\t%s [option]\n\noption:\n%s\n", progname, options);
}

extern char *optarg;
static int parse_args(int argc, char **argv, char **input_filename,
                      char **output_filename, int *loop_num,
                      int *device_index, int *dumpcrop, int *rotate, int *scale, jpu_rect *roi)
{
    int opt;

    *input_filename = NULL;
    *output_filename = NULL;

    while ((opt = getopt(argc, argv, "i:o:n:d:c:g:s:r:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            *input_filename = optarg;
            break;
        case 'o':
            *output_filename = optarg;
            break;
        case 'n':
            *loop_num = atoi(optarg);
            break;
        case 'd':
#ifdef BM_PCIE_MODE
            *device_index = atoi(optarg);
#endif
            break;
        case 'c':
            *dumpcrop = atoi(optarg);
            break;
        case 'g':
            *rotate = atoi(optarg);
            break;
        case 's':
            *scale = atoi(optarg);
            break;
        case 'r':
            sscanf(optarg, "%d,%d,%d,%d", &roi->x, &roi->y, &roi->w, &roi->h);
            break;
        default:
            usage(argv[0]);
            return 0;
        }
    }

    if (*input_filename == NULL)
    {
        fprintf(stderr, "Missing input filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (*output_filename == NULL)
    {
        fprintf(stderr, "Missing output filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (*loop_num <= 0)
        *loop_num = 1;

    return RETVAL_OK;
}

static int run(BmJpuJPEGDecoder* jpeg_decoder, uint8_t* buf, long long size, FILE *fout, int dumpcrop)
{
    bm_status_t ret = 0;
    BmJpuJPEGDecInfo info;
    uint8_t *mapped_virtual_address;
    size_t num_out_byte;
#ifdef BM_PCIE_MODE
    size_t yuv_size;
    uint8_t *yuv_data;
#endif

    /* Perform the actual JPEG decoding */
    BmJpuDecReturnCodes dec_ret = bm_jpu_jpeg_dec_decode(jpeg_decoder, buf, size);
    if (dec_ret != BM_JPU_DEC_RETURN_CODE_OK)
    {
        fprintf(stderr, "could not decode this JPEG image : %s\n", bm_jpu_dec_error_string(dec_ret));
        return RETVAL_ERROR;
    }

    /* Get some information about the frame
     * Note that the info is only available *after* calling bm_jpu_jpeg_dec_decode() */
    bm_jpu_jpeg_dec_get_info(jpeg_decoder, &info);

    printf("aligned frame size: %u x %u\n"
           "pixel actual frame size: %u x %u\n"
           "pixel Y/Cb/Cr stride: %u/%u/%u\n"
           "pixel Y/Cb/Cr size: %u/%u/%u\n"
           "pixel Y/Cb/Cr offset: %u/%u/%u\n"
           "color format: %s\n"
           "chroma interleave: %d\n",
           info.aligned_frame_width, info.aligned_frame_height,
           info.actual_frame_width, info.actual_frame_height,
           info.y_stride, info.cbcr_stride, info.cbcr_stride,
           info.y_size, info.cbcr_size, info.cbcr_size,
           info.y_offset, info.cb_offset, info.cr_offset,
           bm_jpu_color_format_string(info.color_format),
           info.chroma_interleave);

    if (info.framebuffer == NULL)
    {
        fprintf(stderr, "could not decode this JPEG image : no framebuffer returned\n");
        return RETVAL_ERROR;
    }

    /* Map the DMA buffer of the decoded picture, write out the decoded pixels, and unmap the buffer again */
    num_out_byte = info.y_size + info.cbcr_size * 2;
    fprintf(stderr, "decoded output picture:  writing %zu byte\n", num_out_byte);
    bm_handle_t handle = bm_jpu_get_handle(jpeg_decoder->device_index);
#ifdef BM_PCIE_MODE
    yuv_size = info.framebuffer->dma_buffer->size;
    yuv_data = (uint8_t *)malloc(yuv_size);
    if (!yuv_data)
    {
        printf("malloc yuv_data for pcie mode failed\n");
        return RETVAL_ERROR;
    }
    mapped_virtual_address = (uint8_t*)yuv_data;
    ret = bm_memcpy_d2s(handle, yuv_data, *(info.framebuffer->dma_buffer));
#else
    unsigned long long p_vaddr = 0;
    ret = bm_mem_mmap_device_mem(handle, info.framebuffer->dma_buffer, &p_vaddr);
    bm_mem_invalidate_device_mem(handle, info.framebuffer->dma_buffer);
    mapped_virtual_address = (uint8_t*)p_vaddr;
    if (mapped_virtual_address==NULL)
    {
        fprintf(stderr, "bm_jpu_dma_buffer_map failed\n");
        bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info.framebuffer);

        return RETVAL_ERROR;
    }
#endif
    if (dumpcrop == 1) {
        switch (info.color_format){
            case BM_JPU_COLOR_FORMAT_YUV420:
            {
                // write Y
                for (int i = 0; i < info.actual_frame_height ;i++) {
                    fwrite(mapped_virtual_address+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                // write UV
                for (int i = 0; i < info.actual_frame_height/2 ;i++) {
                    fwrite(mapped_virtual_address+(info.aligned_frame_height*info.aligned_frame_width)+i*info.aligned_frame_width/2, 1, info.actual_frame_width/2, fout);
                }
                for (int i = 0; i < info.actual_frame_height/2 ;i++) {
                    fwrite(mapped_virtual_address+(info.aligned_frame_height*info.aligned_frame_width)+(info.aligned_frame_height/2 * info.aligned_frame_width/2)+i*info.aligned_frame_width/2, 1, info.actual_frame_width/2, fout);
                }
                break;
            }
            case BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL:
            {
                // write Y
                for (int i = 0; i < info.actual_frame_height; i++) {
                    fwrite(mapped_virtual_address+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                // write UV
                for (int i = 0; i < info.actual_frame_height ;i++) {
                    fwrite(mapped_virtual_address+(info.aligned_frame_height*info.aligned_frame_width)+i*info.aligned_frame_width/2, 1, info.actual_frame_width/2, fout);
                }
                for (int i = 0; i < info.actual_frame_height ;i++) {
                    fwrite(mapped_virtual_address+(info.aligned_frame_height*info.aligned_frame_width)+(info.aligned_frame_height * info.aligned_frame_width/2)+i*info.aligned_frame_width/2, 1, info.actual_frame_width/2, fout);
                }
                break;
            }
            case BM_JPU_COLOR_FORMAT_YUV422_VERTICAL:
            {
                for (int i = 0; i < info.actual_frame_height; i++) {
                    fwrite(mapped_virtual_address+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                for (int i = 0; i < info.actual_frame_height;i++) {
                    fwrite(mapped_virtual_address+(info.aligned_frame_height*info.aligned_frame_width)+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                break;
            }
            case BM_JPU_COLOR_FORMAT_YUV444:
            {
                for (int i = 0; i < info.actual_frame_height; i++) {
                    fwrite(mapped_virtual_address+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                for (int i = 0; i < info.actual_frame_height; i++) {
                    fwrite(mapped_virtual_address+(info.aligned_frame_height*info.aligned_frame_width)+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                for (int i = 0; i < info.actual_frame_height; i++) {
                    fwrite(mapped_virtual_address+2*(info.aligned_frame_height*info.aligned_frame_width)+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                break;
            }
            case BM_JPU_COLOR_FORMAT_YUV400:
            {
                for (int i = 0; i < info.actual_frame_height;i++) {
                    fwrite(mapped_virtual_address+i*info.aligned_frame_width, 1, info.actual_frame_width, fout);
                }
                break;
            }
            default:
            {
                break;
            }
        }
    } else {
        fwrite(mapped_virtual_address, 1, num_out_byte, fout);
    }
#ifndef BM_PCIE_MODE
    bm_mem_unmap_device_mem(handle, mapped_virtual_address, info.framebuffer->dma_buffer->size);
#endif
    /* Decoded frame is no longer needed, so inform the decoder that it can reclaim it */
    bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info.framebuffer);

#ifdef BM_PCIE_MODE
    if (yuv_data) {
        free(yuv_data);
        yuv_data = NULL;
    }
#endif
    return RETVAL_OK;
}

