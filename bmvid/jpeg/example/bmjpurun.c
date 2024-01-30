#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include "pthread.h"
#endif
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include "jpeg_common.h"
#include "bmjpurun.h"
#include "bmjpuapi.h"
#include "bmjpuapi_jpeg.h"
#include "jpu_lib.h"

extern int jpu_slt_en;
extern int jpu_stress_test;
extern int jpu_rand_sleep;
extern int use_npu_ion_en;
int compare_finish = 0;

#ifdef _WIN32
static int s_gettimeofday(struct timeval* tp, void* tzp)
{
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    tm.tm_year = wtm.wYear - 1900;
    tm.tm_mon = wtm.wMonth - 1;
    tm.tm_mday = wtm.wDay;
    tm.tm_hour = wtm.wHour;
    tm.tm_min = wtm.wMinute;
    tm.tm_sec = wtm.wSecond;
    tm.tm_isdst = -1;
    clock = mktime(&tm);
    tp->tv_sec = clock;
    tp->tv_usec = wtm.wMilliseconds * 1000;
    return (0);
}
#endif

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

static int copy_output_data(void *context, uint8_t const *data, uint32_t size,
                             BmJpuEncodedFrame *encoded_frame)
{
    return 0;
}

// Parameter parsing helper
static int GetValue(FILE *fp, char *para, char *value)
{
    static int LineNum = 1;
    char lineStr[256];
    char paraStr[256];
    fseek(fp, 0, SEEK_SET);

    while (1) {
        if (fgets(lineStr, 256, fp) == NULL)
            return 0;
        sscanf(lineStr, "%s %s", paraStr, value);
        if (paraStr[0] != ';') {
            if (strcmp(para, paraStr) == 0)
                return 1;
        }
        LineNum++;
    }
    return 1;
}

int parseJpgCfgFile(EncConfigParam *pEncConfig, char *FileName)
{
    FILE *Fp;
    char sLine[256] = {0,};

    Fp = fopen(FileName, "rt");
    if (Fp == NULL) {
        fprintf(stderr, "   > ERROR: File not exist <%s>\n", FileName);
        return 1;
    }

    // source file name
    if (GetValue(Fp, "YUV_SRC_IMG", sLine) == 0)
        return 1;
    sscanf(sLine, "%s", (char *)&pEncConfig->yuvFileName);

    // frame format
    //  0-planar, 1-NV12,NV16(CbCr interleave) 2-NV21,NV61(CbCr alternative),
    //  3-YUYV, 4-UYVY, 5-YVYU, 6-VYUY, 7-YUV packed (444 only)
    if (GetValue(Fp, "FRAME_FORMAT", sLine) == 0)
        return 1;
    sscanf(sLine, "%d", &pEncConfig->frameFormat);

    // width
    if (GetValue(Fp, "PICTURE_WIDTH", sLine) == 0)
        return 1;
    sscanf(sLine, "%d", &pEncConfig->picWidth);

    // height
    if (GetValue(Fp, "PICTURE_HEIGHT", sLine) == 0)
        return 1;
    sscanf(sLine, "%d", &pEncConfig->picHeight);

    if (GetValue(Fp, "IMG_FORMAT", sLine) == 0)
        return 1;
    sscanf(sLine, "%d", &pEncConfig->srcFormat);

    fclose(Fp);
    return 0;
}

static int getJpgEncOpenParam(EncConfigParam *pEncConfig, BmJpuEncOpenParams* pEncOP)
{
    int ret = 0;

    char cfgFileName[MAX_FILE_PATH];

    if(pEncConfig == NULL || pEncOP == NULL)
        return 1;

    if(strlen(pEncConfig->cfgFileName))
    {
        strcpy(cfgFileName, pEncConfig->cfgFileName);

        ret = parseJpgCfgFile(pEncConfig, cfgFileName);
        if(ret != 0)
        {
            printf(" Config params error\n");
            return ret;
        }
    }

    pEncOP->packed_format = (pEncConfig->frameFormat - 2);
    if (pEncOP->packed_format < 0)
        pEncOP->packed_format = 0;

    if (pEncConfig->frameFormat == 0)
        pEncOP->chroma_interleave= CBCR_SEPARATED;
    else if (pEncConfig->frameFormat == 1)
        pEncOP->chroma_interleave = CBCR_INTERLEAVE;
    else if (pEncConfig->frameFormat == 2)
        pEncOP->chroma_interleave = CRCB_INTERLEAVE;
    else
        pEncOP->chroma_interleave = CBCR_SEPARATED;

    pEncOP->frame_width = pEncConfig->picWidth;
    pEncOP->frame_height = pEncConfig->picHeight;
    pEncOP->color_format = pEncConfig->srcFormat;

    return ret;
}

void WritePlane(int width, int height, int stride, uint8_t* dst, uint8_t* src)
{
    uint8_t* src_addr = src;
    uint8_t* dst_addr = dst;
    for(int i = 0; i < height; i++)
    {
        memcpy(dst_addr, src_addr,width);
        src_addr += width;
        dst_addr += stride;
    }
}

int LoadYuvImage(int format, int interleave, int packed, int width, int height, BmJpuFramebuffer* framebuffer, BmJpuFramebufferSizes * calculated_sizes, uint8_t* dst, uint8_t* src)
{
    int ret = 0;
    int cbcr_w, cbcr_h, stride,cbcr_stride,chromaSize,lumaSize, w,h,packed_stride;
    if(dst == NULL || src == NULL || framebuffer == NULL || calculated_sizes == NULL)
        return -1;
    stride = calculated_sizes->y_stride;
    cbcr_stride = calculated_sizes->cbcr_stride;

    uint8_t* dst_y  = dst + framebuffer->y_offset;
    uint8_t* dst_cb = dst + framebuffer->cb_offset;
    uint8_t* dst_cr = dst + framebuffer->cr_offset;
    switch(format)
    {
    case FORMAT_420:
        cbcr_h = height >> 1;
        if(interleave == CBCR_SEPARATED)
            cbcr_w = width >> 1;
        else
            cbcr_w = width;
        break;
    case FORMAT_422:
        cbcr_h = height;
        if(interleave == CBCR_SEPARATED)
            cbcr_w = width >> 1;
        else
            cbcr_w = width;
        break;
    case FORMAT_444:
        cbcr_w = width;
        cbcr_h = height;
        break;
    case FORMAT_400:
        cbcr_w = 0;
        cbcr_h = 0;
        break;
    case FORMAT_224:
        cbcr_w = width;
        cbcr_h = height >> 1;
        break;
    default:
        printf("not support the format image, format=%d",format);
        return -1;
    }

    chromaSize = cbcr_w * cbcr_h;
    lumaSize = width * height;
    if(!packed)  //planner mode
    {
        WritePlane(width, height, stride, dst_y,src);           // write Y
        WritePlane(cbcr_w, cbcr_h, cbcr_stride, dst_cb, src+lumaSize);   // write U or UV(intereave)
        if(interleave == CBCR_SEPARATED)
            WritePlane(cbcr_w, cbcr_h, cbcr_stride, dst_cr, src+lumaSize + chromaSize);   // write V
    }
    else{   //packed mode
        if(packed == PACKED_FORMAT_444)
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
        lumaSize =w * h;
        if(width == stride)
            memcpy(dst_y, src,lumaSize);
        else
            WritePlane(w, h, packed_stride, dst_y,src);
    }
    return ret;
}
void *FnEncodeTest(void *param)
{
    long long ret;
    ret = EncodeTest(param);
    return (void *)ret;
}
void *FnDecodeTest(void *param)
{
    long long ret;
    ret = DecodeTest(param);
    return (void *)ret;
}

int DecodeTest(DecConfigParam *param)
{
    BmJpuDecOpenParams open_params;
    DecConfigParam decConfig;
    BmJpuJPEGDecInfo info;
    uint8_t *mapped_virtual_address;
    size_t num_out_byte;
    BmJpuJPEGDecoder *jpeg_decoder = NULL;
    int ret                        = 0;
    uint8_t *pBitStreamBuf         = NULL;
    uint8_t *pRefYuv               = NULL;
    FILE *input_file               = NULL;
    FILE *output_file              = NULL;
    FILE *ref_yuv_file             = NULL;
    int refYuvSize                 = 0;
    struct timeval end;
#ifdef BM_PCIE_MODE
    size_t yuv_size;
    uint8_t *yuv_data = NULL;
#else
    struct timeval start;
    double difftime = 0;
#endif
    memcpy(&decConfig, param, sizeof(DecConfigParam));

    input_file = fopen(decConfig.bitStreamFileName, "rb");
    if( !input_file )
    {
        ret = -1;
        fprintf(stderr,"Can't open %s \n", decConfig.bitStreamFileName );
        goto ERR_DEC_HEAD;
    }
        /* Determine size of the input file to be able to read all of its bytes in one go */
    fseek(input_file, 0, SEEK_END);
    int size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    /* Allocate buffer for the input data, and read the data into it */
    pBitStreamBuf = (uint8_t*)malloc(size);
    if (pBitStreamBuf==NULL)
    {
        fprintf(stderr, "malloc failed!\n");
        ret = -1;
        goto ERR_DEC_HEAD;
    }

    if ( size > fread(pBitStreamBuf, 1, size, input_file)){
        fprintf(stderr, "failed to read whole file\n");
        ret = -1;
        goto ERR_DEC_HEAD;
    }

    if (strlen(decConfig.refFileName))
    {
        ref_yuv_file = fopen(decConfig.refFileName, "rb");
        if (!ref_yuv_file)
        {
            ret = -1;
            fclose(input_file);
            fprintf(stderr,"Can't open %s \n", decConfig.refFileName );
            goto ERR_DEC_HEAD;
        }
        fseek(ref_yuv_file, 0, SEEK_END);
        refYuvSize = ftell(ref_yuv_file);
        fseek(ref_yuv_file, 0, SEEK_SET);

        pRefYuv = malloc(refYuvSize);
        if (!pRefYuv)
        {
            ret = -1;
            fclose(input_file);
            fclose(ref_yuv_file);
            goto ERR_DEC_HEAD;
        }
        if (refYuvSize > fread(pRefYuv, sizeof( uint8_t ), refYuvSize, ref_yuv_file)){
            printf("failed to read in refYuv file\n");
            ret = -1;
            goto ERR_DEC_HEAD;
        }
        fclose(ref_yuv_file);
        ref_yuv_file = NULL;
    }

    bm_jpu_set_logging_threshold(BM_JPU_LOG_LEVEL_ERROR);
    bm_jpu_set_logging_function(logging_fn);

    ret = bm_jpu_dec_load(decConfig.device_index);
    if (ret != BM_JPU_DEC_RETURN_CODE_OK)
    {
        ret = -1;
        goto cleanup;
    }

#ifndef BM_PCIE_MODE
#if _WIN32
    s_gettimeofday(&start, NULL);
#else
    gettimeofday(&start, NULL);
#endif
#endif
    bm_handle_t handle = bm_jpu_get_handle(decConfig.device_index);
    for (int i = 0; i < decConfig.loopNums; i++)
    {
        /* Open the JPEG decoder */
        memset(&open_params, 0, sizeof(BmJpuDecOpenParams));
        open_params.frame_width  = 0;
        open_params.frame_height = 0;
        open_params.chroma_interleave = 0;
        open_params.bs_buffer_size = size;
        open_params.device_index = decConfig.device_index;

        ret = bm_jpu_jpeg_dec_open(&jpeg_decoder, &open_params, 0);
        if (ret != BM_JPU_DEC_RETURN_CODE_OK)
        {
            ret = -1;
            goto cleanup;
        }

         /* Perform the actual JPEG decoding */
        BmJpuDecReturnCodes dec_ret = bm_jpu_jpeg_dec_decode(jpeg_decoder, pBitStreamBuf, size);
        if (dec_ret != BM_JPU_DEC_RETURN_CODE_OK)
        {
            fprintf(stderr, "could not decode this JPEG image : %s\n", bm_jpu_dec_error_string(dec_ret));
            ret = -1;
            goto cleanup;
        }
        /* Get some information about the frame
         * Note that the info is only available *after* calling bm_jpu_jpeg_dec_decode() */
        bm_jpu_jpeg_dec_get_info(jpeg_decoder, &info);

#ifdef PRINTOUT
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
#endif

        if (info.framebuffer == NULL)
        {
            fprintf(stderr, "could not decode this JPEG image : no framebuffer returned\n");
            ret = -1;
            goto cleanup;
        }

        /* Map the DMA buffer of the decoded picture, write out the decoded pixels, and unmap the buffer again */
        num_out_byte = info.y_size + info.cbcr_size * 2;
#ifdef PRINTOUT
        fprintf(stderr, "decoded output picture:  writing %zu byte\n", num_out_byte);
#endif

#ifndef BM_PCIE_MODE
        if((i % 100) == 0)
            printf("dec loopNums : %d\n",i);
        if((i == (decConfig.loopNums - 1)) || jpu_slt_en)
#endif
        {
#ifndef BM_PCIE_MODE
            if(i == (decConfig.loopNums - 1))
#endif
            {
#if _WIN32
                s_gettimeofday(&end, NULL);
#else
                gettimeofday(&end, NULL);
#endif

#ifndef BM_PCIE_MODE
                difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
                printf("Decoder thread = %d, time(second) : %f, real FPS:%d\n",param->instanceIndex, difftime, (int)(decConfig.loopNums/difftime));
#endif
            }
#ifdef BM_PCIE_MODE
            yuv_size = info.framebuffer->dma_buffer->size;
            yuv_data = (uint8_t *)malloc(yuv_size);
            if (!yuv_data)
            {
                printf("malloc yuv_data for pcie mode failed\n");
                ret = -1;
                goto cleanup;
            }

            mapped_virtual_address = yuv_data;
            bm_memcpy_d2s(handle, yuv_data, *(info.framebuffer->dma_buffer));
#else
            unsigned long long p_vaddr = 0;
            ret = bm_mem_mmap_device_mem(handle, info.framebuffer->dma_buffer, &p_vaddr);
            bm_mem_invalidate_device_mem(handle, info.framebuffer->dma_buffer);
            mapped_virtual_address = (uint8_t*)p_vaddr;
            if (ret != BM_SUCCESS)
            {
                fprintf(stderr, "bm_mem_mmap_device_mem failed\n");
                bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info.framebuffer);
                ret = -1;
                goto cleanup;
            }
#endif
            if (strlen(decConfig.yuvFileName))
            {
                output_file = fopen(decConfig.yuvFileName, "wb");
                if (!output_file)
                {
                    fprintf(stderr,"Can't open %s \n", decConfig.yuvFileName );
                    ret = -1;
                    goto cleanup;
                }
                if(output_file != 0)
                {
                    fwrite(mapped_virtual_address, 1, num_out_byte, output_file);
                    fclose(output_file);
                    output_file = NULL;
                }
            }
            if (pRefYuv && i % decConfig.Skip == 0)
            {
                if(memcmp(mapped_virtual_address,pRefYuv,num_out_byte)) {
                    fprintf(stderr,"dec thread = %d, count = %d, compare failed!\n",param->instanceIndex, i);
                    compare_finish = 1;
                    ret = 1;
                    break;
                }
                else
                {
                    printf("dec thread =%d, count = %d, compare passed!\n",param->instanceIndex, i);
                    ret = 0;
                }
            }
#ifdef BM_PCIE_MODE
            if (yuv_data)
            {
                free(yuv_data);
                yuv_data = NULL;
                mapped_virtual_address = NULL;
            }
#else
            bm_mem_unmap_device_mem(handle, mapped_virtual_address, info.framebuffer->dma_buffer->size);
#endif
            if(compare_finish)
            {
                ret = 0;
                break;
            }
        }

        /* Decoded frame is no longer needed, so inform the decoder that it can reclaim it */
        bm_jpu_jpeg_dec_frame_finished(jpeg_decoder, info.framebuffer);

        /* Shut down the JPEG decoder */
        bm_jpu_jpeg_dec_close(jpeg_decoder);
        jpeg_decoder = NULL;
        if(jpu_stress_test && jpu_rand_sleep)
        {

#ifdef _WIN32
            int rt = rand() % 5;
            Sleep(rt);
#else
            int rt = rand() % 5000;
            usleep(rt);
#endif
        }
    }

cleanup:
    if(input_file)      fclose(input_file);
    if(output_file)     fclose(output_file);
    if(ref_yuv_file)    fclose(ref_yuv_file);

    if (jpeg_decoder != NULL)
    {
        bm_jpu_jpeg_dec_close(jpeg_decoder);
        jpeg_decoder = NULL;
    }
    bm_jpu_dec_unload(decConfig.device_index);
ERR_DEC_HEAD:
    if (pBitStreamBuf != NULL)
        free(pBitStreamBuf);
    if (pRefYuv != NULL)
        free(pRefYuv);
#ifdef BM_PCIE_MODE
    if (yuv_data)
        free(yuv_data);
#endif
    return ret;
}

int EncodeTest(EncConfigParam *param)
{
    int ret = 0;
    FILE * fpSrcYuv                = NULL;
    FILE * fpBitstrm               = NULL;
    FILE * fpRefStream             = NULL;
    uint8_t * pRefStream           = NULL;
    uint8_t * pSrcYuv              = NULL;
    BmJpuJPEGEncoder *jpeg_encoder = NULL;
    EncConfigParam encConfig;
    BmJpuEncOpenParams open_params;
    void *acquired_handle;
    size_t output_buffer_size;
    struct timeval start;
#ifndef BM_PCIE_MODE
    struct timeval end;
    double difftime = 0;
#endif
    struct timeval start2;
    struct timeval end2;
    double difftime2 = 0;

    memset(&encConfig, 0x0, sizeof(encConfig));
    encConfig = *param;
    ret = getJpgEncOpenParam(&encConfig, &open_params);
    if(ret != 0)
        goto ERR_ENC_INIT;

    fpSrcYuv = fopen(encConfig.yuvFileName, "rb");
    if( !fpSrcYuv )
    {
        fprintf(stderr,  "Can't open input file %s \n", encConfig.yuvFileName );
        ret = -1;
        goto ERR_ENC_INIT;
    }

    fseek(fpSrcYuv, 0, SEEK_END);
    int srcYuvSize = ftell(fpSrcYuv);
    fseek(fpSrcYuv, 0, SEEK_SET);
    pSrcYuv = malloc(srcYuvSize);
    if (srcYuvSize > fread(pSrcYuv, 1, srcYuvSize, fpSrcYuv)){
        fprintf(stderr, "failed to read src YUV file\n");
        ret = -1;
        goto ERR_ENC_INIT;
    }

    if (strlen(encConfig.refFileName))
    {
        fpRefStream = fopen(encConfig.refFileName, "rb");
        if (!fpRefStream)
        {
            ret = -1;
            fprintf(stderr,"Can't open %s \n", encConfig.refFileName );
            goto ERR_ENC_INIT;
        }
        fseek(fpRefStream, 0, SEEK_END);
        int refBsSize = ftell(fpRefStream);
        fseek(fpRefStream, 0, SEEK_SET);

        pRefStream = malloc(refBsSize);
        if (!pRefStream)
        {
            ret = -1;
            goto ERR_ENC_INIT;
        }
        if ( refBsSize > fread(pRefStream, sizeof( uint8_t ), refBsSize, fpRefStream)){
            fprintf(stderr, "failed to read ref bitstream file\n");
            ret = -1;
            goto ERR_ENC_INIT;
        }
        fclose(fpRefStream);
        fpRefStream = NULL;
    }

    ret = bm_jpu_enc_load(encConfig.device_index);
    if (ret != BM_JPU_ENC_RETURN_CODE_OK)
    {
        goto cleanup;
    }
#if _WIN32
    s_gettimeofday(&start, NULL);
#else
    gettimeofday(&start, NULL);
#endif
    for (int i = 0; i < encConfig.loopNums; i++)
    {
        BmJpuFramebufferSizes calculated_sizes;
        BmJpuFramebuffer framebuffer;

        int raw_size = open_params.frame_width * open_params.frame_height*3; /* the size for YUV444 */
        /* assume the min compression ratio is 2 */
        int min_ratio = 2;
        int bs_buffer_size = raw_size/min_ratio;
        /* bitstream buffer size in unit of 16k */
        bs_buffer_size = (bs_buffer_size+BS_MASK)&(~BS_MASK);
        if (bs_buffer_size >= 16*1023*1024)
            bs_buffer_size = 16*1023*1024;

        /* Open BM JPEG encoder */
        ret = bm_jpu_jpeg_enc_open(&(jpeg_encoder), bs_buffer_size, encConfig.device_index);
        if (ret != BM_JPU_ENC_RETURN_CODE_OK)
        {
            fprintf(stderr, "Error! Failed to open bm_jpu_jpeg_enc_open() :  %s\n",
                    bm_jpu_enc_error_string(ret));
            goto  cleanup;
        }

        /* Initialize the input framebuffer */
        int framebuffer_alignment = 16;
        bm_jpu_calc_framebuffer_sizes(open_params.color_format,
                                    open_params.frame_width,
                                    open_params.frame_height,
                                    framebuffer_alignment,
                                    open_params.chroma_interleave,
                                    &calculated_sizes);


        memset(&framebuffer, 0, sizeof(framebuffer));
        if(open_params.packed_format >= PACKED_FORMAT_422_YUYV && open_params.packed_format <= PACKED_FORMAT_422_VYUY)
        {
            framebuffer.y_stride = 2 * open_params.frame_width;   //calculated_sizes.y_stride;
        }
        else if(open_params.packed_format == PACKED_FORMAT_444)
        {
            framebuffer.y_stride = 3 * open_params.frame_width;  //calculated_sizes.y_stride;
        }
        else
        {
            framebuffer.y_stride    = calculated_sizes.y_stride;
            framebuffer.cbcr_stride = calculated_sizes.cbcr_stride;
            framebuffer.y_offset    = 0;
            framebuffer.cb_offset   = calculated_sizes.y_size;
            framebuffer.cr_offset   = calculated_sizes.y_size + calculated_sizes.cbcr_size;
        }

        /* Allocate a DMA buffer for the input pixels. In production,
        * it is typically more efficient to make sure the input frames
        * already come in DMA / physically contiguous memory, so the
        * encoder can read from them directly. */
        bm_handle_t handle = bm_jpu_get_handle(encConfig.device_index);
        framebuffer.dma_buffer = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t));
        ret = bm_malloc_device_byte_heap_mask(handle, framebuffer.dma_buffer, 6, calculated_sizes.total_size);
        if (ret != BM_SUCCESS) {
            if (framebuffer.dma_buffer != NULL) {
                free(framebuffer.dma_buffer);
                framebuffer.dma_buffer = NULL;
            }
            printf("malloc device memory size = %d failed, ret = %d\n", calculated_sizes.total_size, ret);
            ret = -1;
            goto  cleanup;
        }

#ifdef BM_PCIE_MODE
        ret = bm_memcpy_s2d_partial(handle, *(framebuffer.dma_buffer), pSrcYuv, srcYuvSize);
#else
        /* Load the input pixels into the DMA buffer */
        unsigned long long p_virt_addr = 0;
        ret = bm_mem_mmap_device_mem(handle, framebuffer.dma_buffer, &p_virt_addr);
        if (ret != BM_SUCCESS)
        {
            if (framebuffer.dma_buffer != NULL) {
                free(framebuffer.dma_buffer);
                framebuffer.dma_buffer = NULL;
            }
            fprintf(stderr, "Error! bm_mem_mmap_device_mem failed.\n");
            ret = -1;
            goto  cleanup;
        }

        if(NULL != pRefStream)
        {
            memset((uint8_t*)p_virt_addr, 0x0, calculated_sizes.total_size);
        }
        LoadYuvImage(encConfig.srcFormat, open_params.chroma_interleave, open_params.packed_format, encConfig.picWidth,
             encConfig.picHeight, &framebuffer, &calculated_sizes, (uint8_t*)p_virt_addr, pSrcYuv);

        /* Flush cache to the DMA buffer */
        bm_mem_flush_device_mem(handle, framebuffer.dma_buffer);

        /* Unmap the DMA buffer */
        bm_mem_unmap_device_mem(handle, (uint8_t*)p_virt_addr, framebuffer.dma_buffer->size);
#endif
        /* Set up the encoding parameters */#
        BmJpuJPEGEncParams enc_params;
        memset(&enc_params, 0, sizeof(enc_params));
        enc_params.frame_width    = open_params.frame_width;
        enc_params.frame_height   = open_params.frame_height;
        enc_params.quality_factor = 85;
        enc_params.color_format   = open_params.color_format;
        enc_params.packed_format   = open_params.packed_format;
        enc_params.chroma_interleave = open_params.chroma_interleave;
        enc_params.output_buffer_context = NULL;
        if( strlen(encConfig.bitStreamFileName) || fpRefStream != NULL )
        {
            enc_params.acquire_output_buffer = acquire_output_buffer;
            enc_params.finish_output_buffer  = finish_output_buffer;
        }
        else{
            enc_params.write_output_data  = copy_output_data;
        }

        if ((encConfig.rotate & 0x3 ) != 0) {
            enc_params.rotationEnable = 1; //
            if ((encConfig.rotate & 0x3) == 1) {
                enc_params.rotationAngle = 90;
            } else if ((encConfig.rotate & 0x3) == 2) {
                enc_params.rotationAngle = 180;
            } else if ((encConfig.rotate & 0x3) == 3) {
                enc_params.rotationAngle = 270;
            }
        }
        if((encConfig.rotate & 0xc ) != 0) {
            enc_params.mirrorEnable = 1;  // --
            if ((encConfig.rotate & 0xc )>>2 == 1) {
                enc_params.mirrorDirection = 1; //
            } else if ((encConfig.rotate & 0xc )>>2 == 2) {
                enc_params.mirrorDirection = 2; //
            } else {
                enc_params.mirrorDirection = 3; //
            }
        }

#if _WIN32
        s_gettimeofday(&start2, NULL);
#else
        gettimeofday(&start2, NULL);
#endif
        /* Do the actual encoding */
        ret = bm_jpu_jpeg_enc_encode(jpeg_encoder, &framebuffer,
                                        &enc_params, &acquired_handle, &output_buffer_size);
#if _WIN32
        s_gettimeofday(&end2, NULL);
#else
        gettimeofday(&end2, NULL);
#endif

        difftime2 += (end2.tv_sec + end2.tv_usec / 1000.0 / 1000.0) - (start2.tv_sec + start2.tv_usec / 1000.0 / 1000.0);
        /* The framebuffer's DMA buffer isn't needed anymore, since we just
        * did the encoding, so deallocate it */
        bm_free_device(handle, *(framebuffer.dma_buffer));

#ifndef BM_PCIE_MODE
        if((i % 100) == 0)
            printf("enc loopNums : %d\n",i);

        if((i == (encConfig.loopNums - 1)) || jpu_slt_en)
#endif
        {

#ifndef BM_PCIE_MODE
            if(i == (encConfig.loopNums - 1))
#endif
            {

#ifndef BM_PCIE_MODE
#if _WIN32
                s_gettimeofday(&end, NULL);
#else
                gettimeofday(&end, NULL);
#endif
                difftime = (end.tv_sec + end.tv_usec / 1000.0 / 1000.0) - (start.tv_sec + start.tv_usec / 1000.0 / 1000.0);
                printf("Encoder thread = %d, time(second) : %f,   jpu theory FPS: %d (not include memory/flush times), real FPS:%d\n",param->instanceIndex, difftime,
                   (int)(encConfig.loopNums/difftime2), (int)(encConfig.loopNums/difftime));
#endif
            }
            if (!jpu_slt_en)
            {
                if(strlen(encConfig.bitStreamFileName))
                {
                    fpBitstrm = fopen(encConfig.bitStreamFileName, "wb");
                    if( !fpBitstrm )
                    {
                        fprintf(stderr,  "Can't open bitstream file %s \n", encConfig.bitStreamFileName );
                        ret = -1;
                        goto cleanup;
                    }
                    else
                    {
                        fwrite(acquired_handle, 1, output_buffer_size, fpBitstrm);
                        fclose(fpBitstrm);
                        fpBitstrm = NULL;
                    }
                }
            }
            if(pRefStream && i % encConfig.Skip == 0)
            {
                if(memcmp(acquired_handle,pRefStream,output_buffer_size)) {
                    fprintf(stderr,"enc thread = %d,count = %d, compare failed!\n",param->instanceIndex, i);
                    compare_finish = 1;
                    ret = 1;
                    break;
                }
                else
                {
                    printf("enc thread = %d, count = %d, compare passed!\n",param->instanceIndex, i);
                    ret = 0;
                }
            }
            if(compare_finish)
            {
                ret = 0;
                break;
            }
        }
        bm_jpu_jpeg_enc_close(jpeg_encoder);
        jpeg_encoder = NULL;

        if (framebuffer.dma_buffer != NULL) {
            free(framebuffer.dma_buffer);
            framebuffer.dma_buffer = NULL;
        }
        if (acquired_handle != NULL)
        {
            free(acquired_handle);
            acquired_handle = NULL;
        }
        if(jpu_stress_test && jpu_rand_sleep)
        {
#ifdef _WIN32
            int rt = rand() % 5;
            Sleep(rt);
#else
            int rt = rand() % 5000;
            usleep(rt);
#endif
        }
    }

cleanup:
    if (acquired_handle != NULL)
        free(acquired_handle);
    if (jpeg_encoder != NULL)
    {
        bm_jpu_jpeg_enc_close(jpeg_encoder);
        jpeg_encoder = NULL;
    }
    bm_jpu_enc_unload(encConfig.device_index);
ERR_ENC_INIT:
    if(fpSrcYuv) fclose(fpSrcYuv);
    if(fpBitstrm) fclose(fpBitstrm);
    if(fpRefStream) fclose(fpRefStream);
    if(pRefStream != NULL)
        free(pRefStream);
    if(pSrcYuv != NULL)
        free(pSrcYuv);
    return ret;
}
int MultiInstanceTest(MultiConfigParam *param)
{
    int i;
#ifdef _WIN32
    HANDLE thread_id[MAX_NUM_INSTANCE];
#else
    pthread_t thread_id[MAX_NUM_INSTANCE];
#endif
    void *ret[MAX_NUM_INSTANCE] = {0};
    DecConfigParam *pDecConfig;
    EncConfigParam *pEncConfig;

    for(i=0; i<param->numMulti; i++)
    {
        if (param->multiMode[i]) //decoder case
        {
            pDecConfig = &param->decConfig[i];
            pDecConfig->instanceIndex = i;
#ifdef _WIN32
            if ((thread_id[i] = CreateThread(NULL, 0, (void*)FnDecodeTest, (void*)pDecConfig, 0, NULL)) == NULL ) {
                printf("create thread error\n");
            }
#else
            pthread_create(&thread_id[i], NULL, FnDecodeTest, pDecConfig);
#endif
        }
        else
        {
            pEncConfig = &param->encConfig[i];
            pEncConfig->instanceIndex = i;
#ifdef _WIN32
            if ((thread_id[i] = CreateThread(NULL, 0, (void*)FnEncodeTest, (void*)pDecConfig, 0, NULL)) == NULL ) {
                printf("create thread error\n");
            }
#else
            pthread_create(&thread_id[i], NULL, FnEncodeTest, pEncConfig);
#endif
        }
#ifdef _WIN32
        Sleep(1000);
#else
        usleep(1000*1000);	  // it makes delay to adjust a gab among threads
#endif


    }

    for(i=0; i<param->numMulti; i++)
    {
#ifdef _WIN32
        if (WaitForSingleObject(thread_id[i], INFINITE) != WAIT_OBJECT_0) {
            printf("release thread error\n");
        }
#else
        pthread_join(thread_id[i], &ret[i]);
#endif

    }

    for(i=0; i<param->numMulti; i++)
    {
        if (ret[i] != 0)
        {
            printf("error: thread %d  faild, ret = %lld\n",i, (long long)ret[i]);
            return 1;
        }
    }
    return 0;
}
