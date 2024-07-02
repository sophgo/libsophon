/**
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
/* This is a example of how to decode JPEGs with the bmjpuapi library.
 * It reads the given JPEG file and configures the jpu to decode MJPEG data.
 * Then, the decoded pixels are written to the output file, as raw pixels.
 *
 * Note that using the JPEG decoder is optional, and it is perfectly OK to use
 * the lower-level video decoder API for JPEGs as well. (In fact, this is what
 * the JPEG decoder does internally.) The JPEG decoder is considerably easier
 * to use and requires less boilerplate code, however. */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#include <dirent.h>
#else
#include <windows.h>
#endif
#include <errno.h>

#include "jpeg_common.h"
#include "getopt.h"

#define MAX_DATASET_PICS_NUM 1024
#define MAX_PICS_PATH_LENGTH 1024

typedef struct {
    int x;
    int y;
    int w;
    int h;
} RoiRect;

typedef struct {
    char *input_filename;
    char *output_filename;
    char *dataset_path;
    int loop_num;
    int device_index;
    int dump_crop;
    int rotate;
    int scale;
    RoiRect roi;
    int cbcr_interleave;
    int external_mem;
    int bs_size;
    int fb_num;
    int fb_size;
    int bs_heap;
    int fb_heap;
    int timeout_on_open;
    int timeout_count_on_open;
    int timeout_on_process;
    int timeout_count_on_process;
    int min_width;
    int min_height;
    int max_width;
    int max_height;
} JpegDecParams;

static void usage(char *progname);
static int parse_args(int argc, char **argv, JpegDecParams *params);
static int run(BmJpuJPEGDecoder* jpeg_decoder, uint8_t* buf, long long size, FILE *fout, int dump_crop, int timeout, int timeout_count);
int load_pics(char* dataset_path, int* pic_nums, char** input_names, char** output_names);

int load_pics(char* dataset_path, int* pic_nums, char** input_names, char** output_names)
{
    int ret = 0;
    int i = 0;
#ifdef WIN32
    HANDLE hFind;
    WIN32_FIND_DATA findData;
    LARGE_INTEGER size;
    hFind = FindFirstFile(dataset_path, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        printf("open %s fail\n", dataset_path);
        ret = -1;
    }
    do
    {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            continue;

        input_names[i] = (char*)malloc(MAX_PICS_PATH_LENGTH);
        output_names[i] = (char*)malloc(MAX_PICS_PATH_LENGTH);
        sprintf(input_names[i], "%s/%s", dataset_path, findData.cFileName);
        sprintf(output_names[i], "%s%s%s", "out_", findData.cFileName, ".yuv");

        i++;
    } while (FindNextFile(hFind, &findData));

#else
    DIR * dir = NULL;
    struct dirent * dirp;

    dir = opendir(dataset_path);
    if(dir == NULL)
    {
        printf("open %s fail\n", dataset_path);
        ret = -1;
    }

    while(1)
    {
        dirp = readdir(dir);
        if(dirp == NULL)
        {
            break;
        }

        if (0 == strcmp(dirp->d_name, ".") || 0 == strcmp(dirp->d_name, ".."))
            continue;

        input_names[i] = (char*)malloc(MAX_PICS_PATH_LENGTH);
        output_names[i] = (char*)malloc(MAX_PICS_PATH_LENGTH);
        sprintf(input_names[i], "%s/%s", dataset_path, dirp->d_name);
        sprintf(output_names[i], "%s%s%s", "out_", dirp->d_name, ".yuv");

        i++;
    }

#endif
    *pic_nums = i;
    return ret;
}

int main(int argc, char *argv[])
{
    BmJpuDecOpenParams open_params;
    BmJpuJPEGDecoder *jpeg_decoder = NULL;
    FILE **input_files = NULL;
    FILE **output_files = NULL;
    uint8_t **bs_buf = NULL;
    char **dataset_inputs = NULL;
    char **dataset_outputs = NULL;
    JpegDecParams dec_params;
    int ret = 0;
    int i = 0, j = 0;
    int *input_size = 0;
    int pics_num = 1;
    bm_handle_t bm_handle = NULL;
    bm_device_mem_t bitstream_buffer = {0};
    bm_device_mem_t frame_buffer[MAX_FRAME_COUNT] = {0};
    bm_jpu_phys_addr_t frame_buffer_phys_addr[MAX_FRAME_COUNT] = {0};

    ret = parse_args(argc, argv, &dec_params);
    if (ret != RETVAL_OK) {
        return -1;
    }

    if(dec_params.dataset_path){
        dataset_inputs = malloc(MAX_DATASET_PICS_NUM*sizeof(char*));
        dataset_outputs = malloc(MAX_DATASET_PICS_NUM*sizeof(char*));
        ret = load_pics(dec_params.dataset_path, &pics_num, dataset_inputs, dataset_outputs);
    } else {
        dataset_inputs = malloc(sizeof(char*));
        dataset_outputs = malloc(sizeof(char*));
        dataset_inputs[0] = dec_params.input_filename;
        dataset_outputs[0] = dec_params.output_filename;
    }

    input_files = malloc(pics_num * sizeof(FILE*));
    output_files = malloc(pics_num * sizeof(FILE*));
    input_size = malloc(pics_num * sizeof(int));
    bs_buf = malloc(pics_num * sizeof(uint8_t*));

    for(i=0; i<pics_num; i++)
    {
        input_files[i] = fopen(dataset_inputs[i], "rb");
        if (input_files[i] == NULL) {
            fprintf(stderr, "Opening %s for reading failed: %s\n", dec_params.input_filename, strerror(errno));
            ret = -1;
            goto cleanup;
        }

        output_files[i] = fopen(dataset_outputs[i], "wb");
        if (output_files[i] == NULL) {
            fprintf(stderr, "Opening %s for writing failed: %s\n", dec_params.output_filename, strerror(errno));
            ret = -1;
            goto cleanup;
        }

        /* Determine size of the input file to be able to read all of its bytes in one go */
        fseek(input_files[i], 0, SEEK_END);
        input_size[i] = ftell(input_files[i]);
        fseek(input_files[i], 0, SEEK_SET);

        printf("encoded input frame size: %d bytes\n", input_size[i]);

        /* Allocate buffer for the input data, and read the data into it */
        bs_buf[i] = (uint8_t*)malloc(input_size[i]);
        if (bs_buf[i] == NULL) {
            fprintf(stderr, "malloc failed!\n");
            ret = -1;
            goto cleanup;
        }

        if (fread(bs_buf[i], 1, input_size[i], input_files[i]) < (size_t)input_size[i]) {
            printf("failed to read in whole file!\n");
            ret = -1;
            goto cleanup;
        }
    }

    /* User memory config */
    if (dec_params.external_mem) {
        ret = bm_dev_request(&bm_handle, dec_params.device_index);
        if (ret != BM_SUCCESS) {
            fprintf(stderr, "failed to request device %d\n", dec_params.device_index);
            ret = -1;
            goto cleanup;
        }

        if (dec_params.bs_size) {
            ret = bm_malloc_device_byte_heap_mask(bm_handle, &bitstream_buffer, dec_params.bs_heap, dec_params.bs_size);
            if (ret != BM_SUCCESS) {
                fprintf(stderr, "failed to malloc bitstream buffer, size=%d\n", dec_params.bs_size);
                ret = -1;
                goto cleanup;
            }
        }

        if (dec_params.fb_size) {
            for (i = 0; i < dec_params.fb_num; i++) {
                ret = bm_malloc_device_byte_heap_mask(bm_handle, &frame_buffer[i], dec_params.fb_heap, dec_params.fb_size);
                if (ret != BM_SUCCESS) {
                    fprintf(stderr, "failed to malloc frame buffer, size=%d\n", dec_params.fb_size);
                    ret = -1;
                    goto cleanup;
                }
                frame_buffer_phys_addr[i] = frame_buffer[i].u.device.device_addr;
            }
        }
    }

    bm_jpu_set_logging_threshold(BM_JPU_LOG_LEVEL_ERROR);
    bm_jpu_set_logging_function(logging_fn);

    /* Load JPU */
    ret = bm_jpu_dec_load(dec_params.device_index);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        ret = -1;
        goto cleanup;
    }

    memset(&open_params, 0, sizeof(BmJpuDecOpenParams));
    open_params.min_frame_width = dec_params.min_width;
    open_params.min_frame_height = dec_params.min_height;
    open_params.max_frame_width = dec_params.max_width;
    open_params.max_frame_height = dec_params.max_height;
    open_params.chroma_interleave = (BmJpuChromaFormat)dec_params.cbcr_interleave;
    open_params.bs_buffer_size = dec_params.bs_size;
    open_params.device_index = dec_params.device_index;
    open_params.timeout = dec_params.timeout_on_open;
    open_params.timeout_count = dec_params.timeout_count_on_open;
    if (dec_params.roi.w && dec_params.roi.h) {
        open_params.roiEnable = 1;
        open_params.roiWidth = dec_params.roi.w;
        open_params.roiHeight = dec_params.roi.h;
        open_params.roiOffsetX = dec_params.roi.x;
        open_params.roiOffsetY = dec_params.roi.y;
    }

    /* Get rotate angle */
    if ((dec_params.rotate & 0x3) != 0) {
        open_params.rotationEnable = 1;
        if ((dec_params.rotate & 0x3) == 1) {
            open_params.rotationAngle = BM_JPU_ROTATE_90;
        } else if ((dec_params.rotate & 0x3) == 2) {
            open_params.rotationAngle = BM_JPU_ROTATE_180;
        } else if ((dec_params.rotate & 0x3) == 3) {
            open_params.rotationAngle = BM_JPU_ROTATE_270;
        }
    }
    /* Get mirror direction */
    if ((dec_params.rotate & 0xc) != 0) {
        open_params.mirrorEnable = 1;
        if ((dec_params.rotate & 0xc) >> 2 == 1) {
            open_params.mirrorDirection = BM_JPU_MIRROR_VER;
        } else if ((dec_params.rotate & 0xc) >> 2 == 2) {
            open_params.mirrorDirection = BM_JPU_MIRROR_HOR;
        } else if ((dec_params.rotate & 0xc) >> 2 == 3) {
            open_params.mirrorDirection = BM_JPU_MIRROR_HOR_VER;
        }
    }

    /* Set scale */
    open_params.scale_ratio = dec_params.scale;

    /* Set external memory */
    if (dec_params.external_mem) {
        if (dec_params.bs_size) {
            open_params.bitstream_from_user = 1;
            open_params.bs_buffer_phys_addr = bitstream_buffer.u.device.device_addr;
        }

        if (dec_params.fb_size) {
            open_params.framebuffer_from_user = 1;
            open_params.framebuffer_num = dec_params.fb_num;
            open_params.framebuffer_phys_addrs = frame_buffer_phys_addr;
            open_params.framebuffer_size = dec_params.fb_size;
        }
    }

    ret = bm_jpu_jpeg_dec_open(&jpeg_decoder, &open_params, 0);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK) {
        ret = -1;
        goto cleanup;
    }

    for (i = 0; i < dec_params.loop_num; i++) {
        /* Open the JPEG decoder */
        for(j=0; j<pics_num; j++){
            fseek(output_files[j], 0, SEEK_SET);
            ret = run(jpeg_decoder, bs_buf[j], input_size[j], output_files[j], dec_params.dump_crop, dec_params.timeout_on_process, dec_params.timeout_count_on_process);
            if (ret == RETVAL_ERROR) {
                ret = -1;
                goto cleanup;
            }
        }
    }
    /* Shut down the JPEG decoder */
    bm_jpu_jpeg_dec_close(jpeg_decoder);
    jpeg_decoder = NULL;

cleanup:
    if (jpeg_decoder != NULL) {
        /* Shut down the JPEG decoder */
        bm_jpu_jpeg_dec_close(jpeg_decoder);
        jpeg_decoder = NULL;
    }

    /* Unload JPU */
    bm_jpu_dec_unload(dec_params.device_index);

    /* Free external memory */
    if (bm_handle != NULL) {
        for (i = 0; i < MAX_FRAME_COUNT; i++) {
            if (frame_buffer[i].size != 0) {
                bm_free_device(bm_handle, frame_buffer[i]);
            }
        }

        if (bitstream_buffer.size != 0) {
            bm_free_device(bm_handle, bitstream_buffer);
        }

        bm_dev_free(bm_handle);
        bm_handle = NULL;
    }

    /* Input data is not needed anymore, so free the input buffer */

    for(i=0; i<pics_num; i++)
    {
        if (bs_buf[i] != NULL) {
            free(bs_buf[i]);
            bs_buf[i] = NULL;
        }

        if (input_files[i] != NULL) {
            fclose(input_files[i]);
            input_files[i] = NULL;
        }

        if (output_files[i] != NULL) {
            fclose(output_files[i]);
            output_files[i] = NULL;
        }
    }

    free(input_files);
    free(output_files);
    free(input_size);
    free(bs_buf);

    return ret;
}

static void usage(char *progname)
{
    static char options[] =
        "\t--help                       help info\n"
        "\t--dataset_path               dir of input jpeg files\n"
        "\t--input, -i                  input file\n"
        "\t--output, -i                 output file\n"
        "\t--loop_number, -n            loop number (default 1)\n"
    #ifdef BM_PCIE_MODE
        "\t--device_index, -d           device index (default 0)\n"
    #endif
        "\t--dump_crop, -c              crop output (default 0) [0: no crop, 1: crop to actual size]\n"
        "\t--rotate, -g                 rotate angle and mirror direction (default 0)\n"
        "\t                             b'[1:0] - 0: No rotate, 1: 90, 2: 180, 3: 270\n"
        "\t                             b'[2] - vertical flip\n"
        "\t                             b'[3] - horizontal flip\n"
        "\t--scale, -s                  scale ratio both width and height (default 0) [0: no scale, 1: 1/2, 2: 1/4, 3: 1/8]\n"
        "\t--roi_rect, -r               roi_x,roi_y,roi_w,roi_h\n"
        "\t--cbcr_interleave            Cb/Cr component is interleaved or not (default 0) [0: planared, 1: Cb/Cr interleaved, 2: Cr/Cb interleaved]\n"
        "\t--external_memory            bitstream and framebuffer memory is allocated by user\n"
        "\t--bitstream_size             bitstream buffer size set by user\n"
        "\t--framebuffer_num            framebuffer num set by user (default 1) (only valid if external_memory is enabled)\n"
        "\t--framebuffer_size           framebuffer size set by user (only valid if external_memory is enabled)\n"
        "\t--bs_heap                    bs buffer heap when user allocate bs buffer (default 7, 111 means all the 3 heaps can be used)\n"
        "\t--fb_heap                    frame buffer heap when user allocate frame buffer (default 7, 111 means all the 3 heaps can be used)\n"
        "\t--timeout_on_open            set timeout on decoder open\n"
        "\t--timeout_count_on_open      set timeout count on decoder open\n"
        "\t--timeout_on_process         set timeout on bm_jpu_jpeg_dec_decode\n"
        "\t--timeout_count_on_process   set timeout count on bm_jpu_jpeg_dec_decode\n"
        "\t--min_width                  set the min width that the decoder could resolve (default 0)\n"
        "\t--min_height                 set the min height that the decoder could resolve (default 0)\n"
        "\t--max_width, -w              set the max width that the decoder could resolve (default 0)\n"
        "\t--max_height, -h             set the max height that the decoder could resolve (default 0)\n"
        ;

    fprintf(stderr, "usage:\t%s [option]\n\noption:\n%s\n", progname, options);
}

static int parse_args(int argc, char **argv, JpegDecParams *params)
{
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"help", no_argument, NULL, '0'},
        {"input", required_argument, NULL, 'i'},
        {"output", required_argument, NULL, 'o'},
        {"dataset_path", required_argument, NULL, '0'},
        {"loop_number", required_argument, NULL, 'n'},
    #ifdef BM_PCIE_MODE
        {"device_index", required_argument, NULL, 'd'},
    #endif
        {"dump_crop", required_argument, NULL, 'c'},
        {"rotate", required_argument, NULL, 'g'},
        {"scale", required_argument, NULL, 's'},
        {"roi_rect", required_argument, NULL, 'r'},
        {"cbcr_interleave", required_argument, NULL, '0'},
        {"external_memory", no_argument, NULL, '0'},
        {"bitstream_size", required_argument, NULL, '0'},
        {"framebuffer_num", required_argument, NULL, '0'},
        {"framebuffer_size", required_argument, NULL, '0'},
        {"bs_heap", required_argument, NULL, '0'},
        {"fb_heap", required_argument, NULL, '0'},
        {"timeout_on_open", required_argument, NULL, '0'},
        {"timeout_count_on_open", required_argument, NULL, '0'},
        {"timeout_on_process", required_argument, NULL, '0'},
        {"timeout_count_on_process", required_argument, NULL, '0'},
        {"min_width", required_argument, NULL, '0'},
        {"min_height", required_argument, NULL, '0'},
        {"max_width", required_argument, NULL, 'w'},
        {"max_height", required_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    memset(params, 0, sizeof(JpegDecParams));
    params->bs_heap = HEAP_MASK;
    params->fb_heap = HEAP_MASK;
    while ((opt = getopt_long(argc, argv, "i:o:n:d:c:g:s:r:w:h:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                params->input_filename = optarg;
                break;
            case 'o':
                params->output_filename = optarg;
                break;
            case 'n':
                params->loop_num = atoi(optarg);
                break;
            case 'd':
            #ifdef BM_PCIE_MODE
                params->device_index = atoi(optarg);
            #endif
                break;
            case 'c':
                params->dump_crop = atoi(optarg);
                break;
            case 'g':
                params->rotate = atoi(optarg);
                break;
            case 's':
                params->scale = atoi(optarg);
                break;
            case 'r':
                sscanf(optarg, "%d,%d,%d,%d", &params->roi.x, &params->roi.y, &params->roi.w, &params->roi.h);
                break;
            case 'w':
                params->max_width = atoi(optarg);
                break;
            case 'h':
                params->max_height = atoi(optarg);
                break;
            case '0':
                if (!strcmp(long_options[option_index].name, "dataset_path")) {
                    params->dataset_path = optarg;
                } else if (!strcmp(long_options[option_index].name, "cbcr_interleave")) {
                    params->cbcr_interleave = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "external_memory")) {
                    params->external_mem = 1;
                } else if (!strcmp(long_options[option_index].name, "bitstream_size")) {
                    params->bs_size = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "framebuffer_num")) {
                    params->fb_num = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "framebuffer_size")) {
                    params->fb_size = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "bs_heap")) {
                    params->bs_heap = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "fb_heap")) {
                    params->fb_heap = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "timeout_on_open")) {
                    params->timeout_on_open = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "timeout_count_on_open")) {
                    params->timeout_count_on_open = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "timeout_on_process")) {
                    params->timeout_on_process = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "timeout_count_on_process")) {
                    params->timeout_count_on_process = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "min_width")) {
                    params->min_width = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "min_height")) {
                    params->min_height = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "max_width")) {
                    params->max_width = atoi(optarg);
                } else if (!strcmp(long_options[option_index].name, "max_height")) {
                    params->max_height = atoi(optarg);
                } else {
                    usage(argv[0]);
                    return RETVAL_ERROR;
                }
                break;
            default:
                usage(argv[0]);
                return RETVAL_ERROR;
        }
    }
/*
    if (params->input_filename == NULL) {
        fprintf(stderr, "Missing input filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (params->output_filename == NULL) {
        fprintf(stderr, "Missing output filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
*/
    if (params->loop_num <= 0) {
        params->loop_num = 1;
    }

    if (params->max_width < 0 || params->max_height < 0) {
        fprintf(stderr, "Invalid params: max_width and max_height could not be less than 0\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (params->bs_size < 0) {
        fprintf(stderr, "Invalid params: bitstream_size could not be less than 0\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    params->bs_size = (params->bs_size + BS_SIZE_MASK - 1) & ~(BS_SIZE_MASK - 1);

    if (params->external_mem) {
        if (params->fb_size < 0) {
            fprintf(stderr, "Invalid params: framebuffer_size could not be less than 0\n\n");
            usage(argv[0]);
            return RETVAL_ERROR;
        }

        if (params->fb_num <= 0) {
            params->fb_num = 1;
        }

        if (params->fb_num > MAX_FRAME_COUNT) {
            params->fb_num = MAX_FRAME_COUNT;
        }
    }

    return RETVAL_OK;
}

static int run(BmJpuJPEGDecoder* jpeg_decoder, uint8_t* buf, long long size, FILE *fout, int dump_crop, int timeout, int timeout_count)
{
    BmJpuJPEGDecInfo info;
    uint8_t *mapped_virtual_address;
    size_t num_out_byte;
#ifdef BM_PCIE_MODE
    size_t yuv_size;
    uint8_t *yuv_data;
#endif

    /* Perform the actual JPEG decoding */
    BmJpuDecReturnCodes dec_ret = bm_jpu_jpeg_dec_decode(jpeg_decoder, buf, size, timeout, timeout_count);
    if (dec_ret != BM_JPU_DEC_RETURN_CODE_OK) {
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
           "image format: %s\n",
           info.aligned_frame_width, info.aligned_frame_height,
           info.actual_frame_width, info.actual_frame_height,
           info.y_stride, info.cbcr_stride, info.cbcr_stride,
           info.y_size, info.cbcr_size, info.cbcr_size,
           info.y_offset, info.cb_offset, info.cr_offset,
           bm_jpu_image_format_string(info.image_format));

    if (info.framebuffer == NULL) {
        fprintf(stderr, "could not decode this JPEG image : no framebuffer returned\n");
        bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info.framebuffer);
        return RETVAL_ERROR;
    }

    /* Map the DMA buffer of the decoded picture, write out the decoded pixels, and unmap the buffer again */
    // num_out_byte = info.y_size + (info.chroma_interleave ? info.cbcr_size : info.cbcr_size * 2);
    num_out_byte = info.framebuffer_size;
    fprintf(stderr, "decoded output picture: writing %zu byte\n", num_out_byte);
    bm_handle_t handle = bm_jpu_dec_get_bm_handle(jpeg_decoder->device_index);
#ifdef BM_PCIE_MODE
    yuv_size = info.framebuffer->dma_buffer->size;
    yuv_data = (uint8_t *)malloc(yuv_size);
    if (!yuv_data) {
        fprintf(stderr, "malloc yuv_data for pcie mode failed\n");
        bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info.framebuffer);
        return RETVAL_ERROR;
    }
    mapped_virtual_address = (uint8_t*)yuv_data;
    bm_memcpy_d2s(handle, yuv_data, *(info.framebuffer->dma_buffer));
#else
    unsigned long long p_vaddr = 0;
    bm_mem_mmap_device_mem(handle, info.framebuffer->dma_buffer, &p_vaddr);
    bm_mem_invalidate_device_mem(handle, info.framebuffer->dma_buffer);
    mapped_virtual_address = (uint8_t*)p_vaddr;
    if (mapped_virtual_address == NULL) {
        fprintf(stderr, "bm_jpu_dma_buffer_map failed\n");
        bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info.framebuffer);
        return RETVAL_ERROR;
    }
#endif
    if (dump_crop == 1) {
        DumpCropFrame(&info, mapped_virtual_address, fout);
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

