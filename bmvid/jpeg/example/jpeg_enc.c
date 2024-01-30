#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <errno.h>
#include <stdarg.h>
#include "jpeg_common.h"
#include "bmjpuapi_jpeg.h"

#include "jpu_io.h" // IOGetPhyMem

//#define EXTERNAL_DMA_MEMORY

#define FRAMEBUFFER_ALIGNMENT 16

typedef struct {
    int width;  // actual width
    int height; // actual height
    int y_stride;
    int c_stride;
    int aligned_height;
    int pix_fmt;
    int rotate;
} EncParameter;

typedef struct {
    char *input_filename;
    char *output_filename;

    int loop_num;
    int device_index;

    EncParameter enc;
} InputParameter;


static void usage(char *progname);

static int parse_args(int argc, char **argv, InputParameter* par);

static int run(BmJpuJPEGEncoder* jpeg_encoder, EncParameter* enc_par, FILE *fin, FILE *fout, int device_index);

int main(int argc, char *argv[])
{
    BmJpuJPEGEncoder *jpeg_encoder = NULL;
    FILE *input_file, *output_file;
    InputParameter par;
    int i, ret = 0;

    ret = parse_args(argc, argv, &par);
    if (ret != RETVAL_OK)
        return -1;

    input_file = fopen(par.input_filename, "rb");
    if (input_file == NULL)
    {
        fprintf(stderr, "Opening %s for reading failed: %s\n",
                par.input_filename, strerror(errno));
        return -1;
    }

    output_file = fopen(par.output_filename, "wb");
    if (output_file == NULL)
    {
        fprintf(stderr, "Opening %s for writing failed: %s\n",
                par.output_filename, strerror(errno));
        fclose(input_file);
        return -1;
    }

    bm_jpu_set_logging_threshold(BM_JPU_LOG_LEVEL_ERROR);
    bm_jpu_set_logging_function(logging_fn);

    /* Load JPU */
    ret = bm_jpu_enc_load(par.device_index);

    for (i=0; i<par.loop_num; i++)
    {
        if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        {
            ret = -1;
            goto cleanup;
        }

        /* Open the JPEG encoder */
        ret = bm_jpu_jpeg_enc_open(&(jpeg_encoder), 0, par.device_index);
        if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        {
            ret = -1;
            goto cleanup;
        }

        fseek(input_file, 0, SEEK_SET);
        fseek(output_file, 0, SEEK_SET);
        ret = run(jpeg_encoder, &(par.enc), input_file, output_file, par.device_index);
        if (ret == RETVAL_ERROR)
        {
            ret = -1;
            goto cleanup;
        }

        /* Shut down the JPEG encoder */
        bm_jpu_jpeg_enc_close(jpeg_encoder);
        jpeg_encoder = NULL;
    }
cleanup:
    if (jpeg_encoder != NULL)
    {
        /* Shut down the JPEG encoder */
        bm_jpu_jpeg_enc_close(jpeg_encoder);
        jpeg_encoder = NULL;
    }

    /* Unload JPU */
    bm_jpu_dec_unload(par.device_index);

    if (input_file != NULL)
        fclose(input_file);
    if (output_file != NULL)
        fclose(output_file);

    return ret;
}

static void usage(char *progname)
{
    static char options[] =
#ifdef BM_PCIE_MODE
        "\t-d device index\n"
#endif
        "\t-f pixel format: 0.YUV420(default); 1.YUV422; 2.YUV444; 3.YUV400. (optional)\n"
        "\t-w actual width\n"
        "\t-h actual height\n"
        "\t-y luma stride (optional)\n"
        "\t-c chroma stride (optional)\n"
        "\t-v aligned height (optional)\n"
        "\t-i input file\n"
        "\t-o output file\n"
        "\t-g  rotate (default 0) [rotate mode[1:0]  0:No rotate  1:90  2:180  3:270] [rotator mode[2]:vertical flip] [rotator mode[3]:horizontal flip]\n"
        "For example,\n"
        "\tbmjpegenc -f 0 -w 100 -h 100 -y 128 -v 112 -i 100x100_yuv420.yuv -o 100x100_yuv420.jpg\n"
        "\tbmjpegenc -f 1 -w 100 -h 100 -y 128 -v 112 -i 100x100_yuv422.yuv -o 100x100_yuv422.jpg\n"
        "\tbmjpegenc -f 1 -w 100 -h 100 -y 128 -c 64 -v 112 -i 100x100_yuv422.yuv -o 100x100.jpg\n"
        ;

    fprintf(stderr, "usage:\t%s [option]\n\noption:\n%s\n", progname, options);
}

extern char *optarg;
static int parse_args(int argc, char **argv, InputParameter* par)
{
    int opt;

    memset(par, 0, sizeof(InputParameter));

    par->device_index = 0;

    while ((opt = getopt(argc, argv, "i:o:n:d:w:h:v:f:y:c:g:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            par->input_filename = optarg;
            break;
        case 'o':
            par->output_filename = optarg;
            break;
        case 'n':
            par->loop_num = atoi(optarg);
            break;
        case 'd':
#ifdef BM_PCIE_MODE
            par->device_index = atoi(optarg);
#endif
            break;
        case 'w':
            par->enc.width = atoi(optarg);
            break;
        case 'h':
            par->enc.height = atoi(optarg);
            break;
        case 'v':
            par->enc.aligned_height = atoi(optarg);
            break;
        case 'f':
            par->enc.pix_fmt = atoi(optarg);
            break;
        case 'y':
            par->enc.y_stride = atoi(optarg);
            break;
        case 'c':
            par->enc.c_stride = atoi(optarg);
            break;
        case 'g':
            par->enc.rotate = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            return 0;
        }
    }

    if (par->input_filename == NULL)
    {
        fprintf(stderr, "Missing input filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    if (par->output_filename == NULL)
    {
        fprintf(stderr, "Missing output filename\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->loop_num <= 0)
        par->loop_num = 1;

    if (par->enc.width <= 0)
    {
        fprintf(stderr, "width <= 0\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    if (par->enc.height <= 0)
    {
        fprintf(stderr, "height <= 0\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    if (par->enc.y_stride != 0 &&
        par->enc.y_stride < par->enc.width)
    {
        fprintf(stderr, "luma stride is less than width\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    if (par->enc.aligned_height != 0 &&
        par->enc.aligned_height < par->enc.height)
    {
        fprintf(stderr, "aligned height is less than actual height\n\n");
        usage(argv[0]);
        return RETVAL_ERROR;
    }
    if (par->enc.pix_fmt < 0 || par->enc.pix_fmt > 3)
    {
        fprintf(stderr, "Invalid pixel format(%d)! It's none of 0, 1, 2, 3\n",
                par->enc.pix_fmt);
        usage(argv[0]);
        return RETVAL_ERROR;
    }

    if (par->enc.y_stride == 0)
        par->enc.y_stride = par->enc.width;

    if (par->enc.c_stride == 0)
    {
        if (par->enc.pix_fmt==0 || par->enc.pix_fmt==1)
            par->enc.c_stride = (par->enc.y_stride+1)/2;
        else
            par->enc.c_stride = par->enc.y_stride;
    }

    if (par->enc.aligned_height == 0)
        par->enc.aligned_height = par->enc.height;

    if (par->enc.pix_fmt==0)
        par->enc.pix_fmt = BM_JPU_COLOR_FORMAT_YUV420;
    else if (par->enc.pix_fmt==1)
        par->enc.pix_fmt = BM_JPU_COLOR_FORMAT_YUV422_HORIZONTAL;
    else if (par->enc.pix_fmt==2)
        par->enc.pix_fmt = BM_JPU_COLOR_FORMAT_YUV444;
    else
        par->enc.pix_fmt = BM_JPU_COLOR_FORMAT_YUV400;

    return RETVAL_OK;
}

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

static int run(BmJpuJPEGEncoder* jpeg_encoder, EncParameter* enc_par, FILE *fin, FILE *fout, int device_index)
{
    bm_status_t ret = BM_SUCCESS;
    BmJpuEncReturnCodes enc_ret;
    BmJpuFramebuffer framebuffer;
    BmJpuJPEGEncParams enc_params;
    void *acquired_handle;
    size_t output_buffer_size;
#ifndef BM_PCIE_MODE
    uint8_t *virt_addr;
#else
    uint8_t *input_data = NULL;
#endif
    int frame_y_size = enc_par->y_stride * enc_par->aligned_height;
    int frame_c_size;
    int frame_total_size;

    if (enc_par->pix_fmt == BM_JPU_COLOR_FORMAT_YUV400)
        frame_c_size = 0;
    else if (enc_par->pix_fmt == BM_JPU_COLOR_FORMAT_YUV420)
        frame_c_size = enc_par->c_stride * ((enc_par->aligned_height+1)/2);
    else
        frame_c_size = enc_par->c_stride * enc_par->aligned_height;

    frame_total_size = frame_y_size + frame_c_size * 2;

    /* Initialize the input framebuffer */
    memset(&framebuffer, 0, sizeof(framebuffer));
    framebuffer.y_stride    = enc_par->y_stride;
    framebuffer.cbcr_stride = enc_par->c_stride;
    framebuffer.y_offset    = 0;
    framebuffer.cb_offset   = frame_y_size;
    framebuffer.cr_offset   = frame_y_size + frame_c_size;

    bm_handle_t handle = bm_jpu_get_handle(device_index);
    framebuffer.dma_buffer = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t));
    ret = bm_malloc_device_byte_heap_mask(handle, framebuffer.dma_buffer, 6, frame_total_size);
    if (ret != BM_SUCCESS) {
      printf("malloc device memory size = %d failed, ret = %d\n", frame_total_size, ret);
      return -1;
    }

#ifdef BM_PCIE_MODE
    input_data = (uint8_t *)calloc(frame_total_size, sizeof(uint8_t));
    if (!input_data)
    {
        fprintf(stderr, "can't alloc mem to save input data\n");
        return RETVAL_ERROR;
    }
    if (fread(input_data, sizeof(uint8_t), frame_total_size, fin) < frame_total_size){
        fprintf(stderr, "failed to read in %d byte of data\n", frame_total_size);
        if (input_data != NULL) free(input_data);

        return RETVAL_ERROR;
    }
    ret = bm_memcpy_s2d_partial(handle, *(framebuffer.dma_buffer), input_data, frame_total_size);
#else
    /* Load the input pixels into the DMA buffer */
    unsigned long long pmap_addr = 0;
    ret = bm_mem_mmap_device_mem(handle, framebuffer.dma_buffer, &pmap_addr);
    virt_addr = (uint8_t*)pmap_addr;
    if (fread(virt_addr, sizeof(uint8_t), frame_total_size, fin) < frame_total_size){
        fprintf(stderr, "failed to read in %d byte of data\n", frame_total_size);
        bm_mem_unmap_device_mem(handle, (void *)virt_addr, framebuffer.dma_buffer->size);
        return RETVAL_ERROR;
    }

    ret = bm_mem_flush_partial_device_mem(handle, framebuffer.dma_buffer, 0, frame_total_size);
    bm_mem_unmap_device_mem(handle, (void *)virt_addr, framebuffer.dma_buffer->size);
#endif

    /* Set up the encoding parameters */
    memset(&enc_params, 0, sizeof(enc_params));
    enc_params.frame_width    = enc_par->width;
    enc_params.frame_height   = enc_par->height;
    enc_params.quality_factor = 85;
    enc_params.color_format   = enc_par->pix_fmt;
    enc_params.acquire_output_buffer = acquire_output_buffer;
    enc_params.finish_output_buffer  = finish_output_buffer;
    enc_params.output_buffer_context = NULL;

    if ((enc_par->rotate & 0x3 ) != 0) {
        enc_params.rotationEnable = 1; //
        if ((enc_par->rotate & 0x3) == 1) {
            enc_params.rotationAngle = 90;
        } else if ((enc_par->rotate & 0x3) == 2) {
            enc_params.rotationAngle = 180;
        } else if ((enc_par->rotate & 0x3) == 3) {
            enc_params.rotationAngle = 270;
        }
    }
    if((enc_par->rotate & 0xc ) != 0) {
        enc_params.mirrorEnable = 1;  // --
        if ((enc_par->rotate & 0xc )>>2 == 1) {
            enc_params.mirrorDirection = 1; //
        } else if ((enc_par->rotate & 0xc )>>2 == 2) {
            enc_params.mirrorDirection = 2; //
        } else {
            enc_params.mirrorDirection = 3; //
        }
    }

    /* Do the actual encoding */
    enc_ret = bm_jpu_jpeg_enc_encode(jpeg_encoder, &framebuffer, &enc_params,
            &acquired_handle, &output_buffer_size);

    /* The framebuffer's DMA buffer isn't needed anymore, since we just
     * did the encoding, so deallocate it */
#if defined(EXTERNAL_DMA_MEMORY)
    IOFreePhyMem(pmd);
#else
    bm_free_device(handle, *(framebuffer.dma_buffer));
    if (framebuffer.dma_buffer != NULL) {
        free(framebuffer.dma_buffer);
        framebuffer.dma_buffer = NULL;
    }
#endif

    if (enc_ret != BM_JPU_ENC_RETURN_CODE_OK)
    {
        fprintf(stderr, "could not encode this image : %s\n", bm_jpu_enc_error_string(enc_ret));
        goto finish;
    }


    /* Write out the encoded frame to the output file. The encoder
     * will have called acquire_output_buffer(), which acquires a
     * buffer by malloc'ing it. The "handle" in this example is
     * just the pointer to the allocated memory. This means that
     * here, acquired_handle is the pointer to the encoded frame
     * data. Write it to file. In production, the acquire function
     * could retrieve an output memory block from a buffer pool for
     * example. */
    fwrite(acquired_handle, 1, output_buffer_size, fout);

finish:
    if (acquired_handle != NULL)
        free(acquired_handle);
#ifdef BM_PCIE_MODE
    if (input_data != NULL)
    {
        free(input_data);
        input_data = NULL;
    }
#endif
    return RETVAL_OK;
}

