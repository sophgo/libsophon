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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <unistd.h>
#include <getopt.h>
#include <semaphore.h>
#include <sys/time.h>
#elif _WIN32
#include <windows.h>
#include <time.h>
#include "windows/libusb-1.0.18/examples/getopt/getopt.h"
#endif

/*
 * vpuapifunc.h is for the compatibility with some operations on Windows and Linux, such as create thread, get time.
 * util.h is for md5 calculate.
 *
 * user can use their own tools rather than these headers.
 */
#include "vpuapifunc.h"
#include "util.h"

#include "bm_vpudec_interface.h"
#include "bmlib_runtime.h"

#define VPU_ALIGN16(_x)             (((_x)+0x0f)&~0x0f)
#define VPU_ALIGN32(_x)             (((_x)+0x1f)&~0x1f)
#define VPU_ALIGN256(_x)            (((_x)+0xff)&~0xff)
#define VPU_ALIGN4096(_x)           (((_x)+0xfff)&~0xfff)

#define HEAP_MASK 0x07
#define INTERVAL (10)
#define defaultReadBlockLen 0x80000
#define BM_VPU_DEC_LITTLE_ENDIAN 0
int readBlockLen = defaultReadBlockLen;
int injectionPercent = 0;
int injectLost = 1; // default as lost data, or scramble the data
int injectWholeBlock = 0;

typedef struct BMTestConfig_struct {
    int run_times;
    int compareType;
    int compareNum;
    int instanceNum;

    int cbcr_interleave;
    int nv21;
    int writeYuv;

    BmVpuDecBitStreamMode bsMode;
    BmVpuDecOutputMapType wtlFormat;
    BmVpuDecStreamFormat streamFormat;
#ifdef	BM_PCIE_MODE
    int pcie_board_id;
#endif
    BOOL result;
    int log_level;
    int compare_skip_num;
    int first_comp_idx;
    int decode_order;
    int width;
    int height;

    int mem_alloc_type;
    int extraFrame;
    int min_frame_cnt;
    int frame_delay;
    BMVidCodHandle vidCodHandle;
    char outputPath[MAX_FILE_PATH];
    char inputPath[MAX_FILE_PATH];
    char refYuvPath[MAX_FILE_PATH];
    osal_file_t* fpIn;
} BMTestConfig;

uint64_t count_enc[MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE];
double fps_enc[MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE];
int g_exit_flag = 0;

int my_memcmp(const void* src, const void* dst, int size)
{
    u64 *src_64, *dst_64;
    int size_64;
    int i;

    if(((u64)src & 7) != 0 || ((u64)dst & 7) != 0 || (size & 7) != 0) {
        printf("may addr error, src : %p, dst : %p, size : %d\n", src, dst, size);
        return -1;
    }
    src_64 = (u64 *)src;
    dst_64 = (u64 *)dst;
    size_64 = size / 8;

    for(i=0; i<size_64; i++) {
        u64 src_val_64 = *src_64, dst_val_64 = *dst_64;
        if(src_val_64 != dst_val_64) {
            int end_index = (i+16 < size_64) ? i+16 : size_64;
            int j;
            printf("src dump index: %d, src val : 0x%016lx, dst val : 0x%016lx, \n", i, src_val_64, dst_val_64);
            for(j=i; j<end_index; j++) {
                printf("0x%016lx, ", src_64[j-i]);
            }
            printf("\ndst dump:\n");
            for(j=i; j<end_index; j++) {
                printf("0x%016lx, ", dst_64[j-i]);
            }
            printf("\n");
            return i+1;
        }
        src_64++;
        dst_64++;
    }
    return 0;
}

int ret = 0;

static int parse_args(int argc, char **argv, BMTestConfig* par);

static void stat_pthread(void *arg)
{
    int thread_num = *(int*)arg;
    int i = 0;
    uint64_t last_count_enc[MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE] = {0};
    uint64_t last_count_sum = 0;
    int dis_mode = 0;
    char *display = getenv("BMVPUDEC_DISPLAY_FRAMERATE");
    if (display) dis_mode = atoi(display);
    printf("BMVPUDEC_DISPLAY_FRAMERATE=%d  thread_num=%d g_exit_flag=%d \n", dis_mode, thread_num, g_exit_flag);
    while(!g_exit_flag) {
#ifdef __linux__
        sleep(INTERVAL);
#elif _WIN32
        Sleep(INTERVAL*1000);
#endif
        if (dis_mode == 1) {
            for (i = 0; i < thread_num; i++) {
                if (i == 0) {
                    printf("ID[%d],   FRM[%10lld], FPS[%2.2lf]\n",
                        i, (long long)count_enc[i], ((double)(count_enc[i]-last_count_enc[i]))/INTERVAL);
                } else {
                    printf("ID[%d] ,  FRM[%10lld], FPS[%2.2lf]  \n",
                        i, (long long)count_enc[i], ((double)(count_enc[i]-last_count_enc[i]))/INTERVAL);
                }
                last_count_enc[i] = count_enc[i];
            }
        } else {
            uint64_t count_sum = 0;
            for (i = 0; i < thread_num; i++)
              count_sum += count_enc[i];
            printf("thread %d, frame %ld, fps %2.2f\n", thread_num, count_sum, ((double)(count_sum-last_count_sum))/INTERVAL);
            last_count_sum = count_sum;
        }
        osal_fflush(stdout);
    }
    printf("stat_pthread over.\n");
    return;
}

static void process_frame(uint8_t *buf0, uint8_t *buf1, uint8_t *buf2, int stride,
                          int width, int height, int is_interleave)
{
    int i;
    // Set strided part to 0 to make md5 stable
    if(stride != width) {
        for (i = 0; i < height; i ++) {
            memset(buf0 + stride * i + width , 0, stride - width);
        }
        if (0 == is_interleave) {
            for (i = 0; i < height / 2; i ++) {
                memset(buf1 + stride * i / 2 + width / 2 , 0, (stride - width) / 2);
            }
            for (i = 0; i < height / 2; i ++) {
                memset(buf2 + stride * i / 2 + width / 2 , 0, (stride - width) / 2);
            }
        } else {
            for (i = 0; i < height / 4; i ++) {
                memset(buf1 + stride * i + width , 0, (stride - width));
            }
        }
    }
}

static void process_output(void* arg)
{
    BMTestConfig *testConfigPara = (BMTestConfig *)arg;
    BMVidCodHandle vidHandle = (BMVidCodHandle)testConfigPara->vidCodHandle;
#ifdef BM_PCIE_MODE
    int coreIdx = bmvpu_dec_get_core_idx(vidHandle);
#endif
    BMVidFrame *pFrame = (BMVidFrame*)malloc(sizeof(BMVidFrame));
    BOOL match, alloced = FALSE, result = TRUE;
    osal_file_t *fpRef = NULL, fpOutput = NULL;
    uint8_t* pRefMem = NULL;
    int32_t readLen = -1, compare = 0, compareNum = 1;
    uint32_t frameSize = 0, ySize = 0, allocedSize = 0;
    int total_frame = 0, cur_frame_idx = 0;
    uint64_t start_time, dec_time;
    printf("Enter process_output!\n");

#ifdef BM_PCIE_MODE
    int frame_write_num = testConfigPara->writeYuv;
    int writeYuv = 0;
    writeYuv = testConfigPara->writeYuv;
#endif
    if (strcmp(testConfigPara->refYuvPath, ""))
    {
        fpRef = osal_fopen(testConfigPara->refYuvPath, "rb");
        if(fpRef == NULL)
        {
            fprintf(stderr, "Can't open reference yuv file: %s\n", testConfigPara->refYuvPath);
        }
        else
        {
            compare = testConfigPara->compareType;
        }
    }

    if (strcmp(testConfigPara->outputPath, ""))
    {
        fpOutput = osal_fopen(testConfigPara->outputPath, "wb");
        if(fpOutput == NULL)
        {
            fprintf(stderr, "Can't open output yuv file: %s\n", testConfigPara->outputPath);
        }
    } else {
        fpOutput = NULL;
    }

    start_time = osal_gettime();
    pFrame = malloc(sizeof(BMVidFrame));

    while(bmvpu_dec_get_status(vidHandle) <= BMDEC_CLOSE)
    {
        if(bmvpu_dec_get_output(vidHandle, pFrame) == 0)
        {
            //printf("get a frame, display index: %d\n", pFrame->frameIdx);

            /* compare yuv if there is reference yum file */
            if(compare)
            {
                ySize = pFrame->stride[0]*pFrame->height;
                frameSize = ySize*3/2;

                if((alloced == TRUE) && (frameSize != allocedSize)) //if sequence change, free memory then malloc
                {
                    free(pRefMem);
                    alloced = FALSE;
                }

                if(alloced == FALSE)
                {
                    pRefMem = malloc(frameSize);
                    if(pRefMem == NULL)
                    {
                        fprintf(stderr, "Can't alloc reference yuv memory\n");
                        break;
                    }
                    alloced = TRUE;
                    allocedSize = frameSize;
                }

                if(total_frame == 0) {
                    osal_fseek(fpRef, 0L, SEEK_END);
                    if(compare == 1)
                        total_frame = osal_ftell(fpRef)/frameSize;
                    else
                        total_frame = osal_ftell(fpRef)/99;
                    osal_fseek(fpRef, 0L, SEEK_SET);
                }

                if(cur_frame_idx==total_frame && result == TRUE) {
                    //srand((unsigned)time(NULL));
                    if(testConfigPara->compare_skip_num > 0) {
                        testConfigPara->first_comp_idx = rand()%(testConfigPara->compare_skip_num + 1);
                        //printf("first compare frame idx : %d\n", testConfigPara->first_comp_idx);
                    }
                    dec_time = osal_gettime();
                    if(dec_time - start_time != 0) {
                        fps_enc[testConfigPara->instanceNum] = (float)total_frame*1000/(dec_time-start_time);
                    }
                    start_time = dec_time;
                    compareNum++;
                    cur_frame_idx = 0;
                }

                if((compare == 1) && (result == TRUE)) //yuv compare
                {
                    if(cur_frame_idx<testConfigPara->first_comp_idx || (cur_frame_idx-testConfigPara->first_comp_idx)%(testConfigPara->compare_skip_num+1) != 0)
                    {
                        //result = TRUE;
                        //testConfigPara->result = TRUE;
                        cur_frame_idx += 1;
                        bmvpu_dec_clear_output(vidHandle, pFrame);
                        count_enc[testConfigPara->instanceNum]++;
                        continue;
                    }
                    osal_fseek(fpRef, (long)frameSize*cur_frame_idx, SEEK_SET);

                    readLen = osal_fread(pRefMem, 1, frameSize, fpRef);

                    if(readLen  == frameSize)
                    {
                        uint8_t *buf0 = NULL;
                        uint8_t *buf1 = NULL;
                        uint8_t *buf2 = NULL;
#ifdef BM_PCIE_MODE
                        if(pFrame->size > 0 && pFrame->size <= 8192*4096*3) {
                            //optimize the framebuffer cdma copy.
                            buf0 = malloc(pFrame->size);
                            buf1 = buf0 + (unsigned int)(pFrame->buf[5] - pFrame->buf[4]);
                            if(!testConfigPara->cbcr_interleave)
                                buf2 = buf0 + (unsigned int)(pFrame->buf[6] - pFrame->buf[4]);
                        }
                        else {
                            buf0 = NULL;
                        }
                        if(buf0 == NULL) {
                            printf("the frame buffer size maybe error..\n");
                            result = FALSE;
                            break;
                        }
                        bmvpu_dec_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf0, pFrame->size, BM_VPU_DEC_LITTLE_ENDIAN);
#else
                        buf0 = pFrame->buf[0];
                        buf1 = pFrame->buf[1];
                        if(!testConfigPara->cbcr_interleave)
                            buf2 = pFrame->buf[2];
#endif

                        match = (memcmp(pRefMem, (void*)buf0, ySize) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            result = FALSE;
                            fprintf(stderr, "MISMATCH WITH GOLDEN Y in frame %d\n", cur_frame_idx);
                        }

                        if(testConfigPara->cbcr_interleave)
                        {
                            match = (memcmp(pRefMem+ySize, (void*)buf1, ySize/2) == 0 ? TRUE : FALSE);
                            if (match == FALSE)
                            {
                                result = FALSE;
                                fprintf(stderr, "MISMATCH WITH GOLDEN chroma in frame %d\n", cur_frame_idx);
                            }
                        } else {
                            match = (memcmp(pRefMem+ySize, (void*)buf1, ySize/4) == 0 ? TRUE : FALSE);
                            if (match == FALSE)
                            {
                                result = FALSE;
                                fprintf(stderr, "MISMATCH WITH GOLDEN U in frame %d\n", cur_frame_idx);
                            }

                            match = (memcmp(pRefMem+ySize*5/4, (void*)buf2, ySize/4) == 0 ? TRUE : FALSE);
                            if (match == FALSE)
                            {
                                result = FALSE;
                                fprintf(stderr, "MISMATCH WITH GOLDEN V in frame %d\n", cur_frame_idx);
                            }
                        }
#ifdef BM_PCIE_MODE
                        free(buf0);
#endif
                    }
                    else
                    {
                        result = FALSE;
                        fprintf(stderr, "NOT ENOUGH DATA\n");
                    }
                }
                else if((compare == 2) && (result == TRUE)) //yuv md5 compare
                {
                    unsigned char yMd[16], uMd[16], vMd[16];
                    char yMd5Str[33], uMd5Str[33], vMd5Str[33];
                    char yRefMd5Str[33], uRefMd5Str[33], vRefMd5Str[33];
                    int i;

                    if(cur_frame_idx<testConfigPara->first_comp_idx || (cur_frame_idx-testConfigPara->first_comp_idx)%(testConfigPara->compare_skip_num+1) != 0)
                    {
                        //result = TRUE;
                        //testConfigPara->result = TRUE;
                        cur_frame_idx += 1;
                        bmvpu_dec_clear_output(vidHandle, pFrame);
                        count_enc[testConfigPara->instanceNum]++;
                        continue;
                    }

                    osal_fseek(fpRef, 99*cur_frame_idx, SEEK_SET);

                    uint8_t *buf0 = NULL;
                    uint8_t *buf1 = NULL;
                    uint8_t *buf2 = NULL;
#ifdef BM_PCIE_MODE
                    if(pFrame->size > 0 && pFrame->size <= 8192*4096*3) {
                        //optimize the framebuffer cdma copy.
                        buf0 = malloc(pFrame->size);
                        buf1 = buf0 + (unsigned int)(pFrame->buf[5] - pFrame->buf[4]);
                        if(!testConfigPara->cbcr_interleave)
                            buf2 = buf0 + (unsigned int)(pFrame->buf[6] - pFrame->buf[4]);
                    }
                    else {
                        buf0 = NULL;
                    }
                    if(buf0 == NULL) {
                        printf("the frame buffer size maybe error..\n");
                        result = FALSE;
                        break;
                    }
                    bmvpu_dec_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf0, pFrame->size,   BM_VPU_DEC_LITTLE_ENDIAN);
#else
                    buf0 = pFrame->buf[0];
                    buf1 = pFrame->buf[1];
                    if(!testConfigPara->cbcr_interleave)
                        buf2 = pFrame->buf[2];
#endif
                    process_frame(buf0, buf1, buf2, pFrame->stride[0],
                                  pFrame->width, pFrame->height, testConfigPara->cbcr_interleave);

                    MD5(buf0, ySize, yMd);
                    if(testConfigPara->cbcr_interleave)
                    {
                        MD5(buf1, ySize/2, uMd);
                    } else {
                        MD5(buf1, ySize/4, uMd);
                        MD5(buf2, ySize/4, vMd);
                    }
#ifdef BM_PCIE_MODE
                    free(buf0);
#endif
                    for(i=0; i<16; i++)
                    {
                        snprintf(yMd5Str + i*2, 2+1, "%02x", yMd[i]);
                        snprintf(uMd5Str + i*2, 2+1, "%02x", uMd[i]);
                        if(!testConfigPara->cbcr_interleave)
                            snprintf(vMd5Str + i*2, 2+1, "%02x", vMd[i]);
                    }

                    readLen = osal_fread(pRefMem, 1, 99, fpRef);

                    if(readLen == 99)
                    {
                        match = (memcmp(pRefMem, yMd5Str, 32) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            snprintf(yRefMd5Str, 33, "%s", (char *)pRefMem);
                            result = FALSE;
                            fprintf(stderr, "MISMATCH WITH GOLDEN Y in frame %d, %s, %s\n", cur_frame_idx, yMd5Str, yRefMd5Str);
                        }

                        match = (memcmp(pRefMem+33, uMd5Str, 32) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            snprintf(uRefMd5Str, 33, "%s", (char *)(pRefMem+33));
                            result = FALSE;
                            fprintf(stderr, "MISMATCH WITH GOLDEN U in frame %d, %s, %s\n", cur_frame_idx, uMd5Str, uRefMd5Str);
                        }

                        if(!testConfigPara->cbcr_interleave)
                        {
                            match = (memcmp(pRefMem+66, vMd5Str, 32) == 0 ? TRUE : FALSE);
                            if (match == FALSE)
                            {
                                snprintf(vRefMd5Str, 33, "%s", (char *)(pRefMem+66));
                                result = FALSE;
                                fprintf(stderr, "MISMATCH WITH GOLDEN V in frame %d, %s, %s\n", cur_frame_idx, vMd5Str, vRefMd5Str);
                            }
                        }
                    }
                    else
                    {
                        result = FALSE;
                        fprintf(stderr, "NOT ENOUGH DATA\n");
                    }
                }
                if(result == FALSE && testConfigPara->result == TRUE) {
                    int stream_size = 0x700000;
                    int len = 0;
                    int core_idx = bmvpu_dec_get_core_idx(vidHandle);
                    int inst_idx = bmvpu_dec_get_inst_idx(vidHandle);
                    unsigned char *p_stream = malloc(stream_size);
                    if(ySize != 0) {
                        char yuv_filename[256]={0};
                        FILE *fp = NULL;

                        sprintf(yuv_filename, "core%d_inst%d_frame%d_dump_%dx%d.yuv", core_idx, inst_idx, cur_frame_idx, pFrame->width, pFrame->height);
                        fprintf(stderr, "get error and dump yuvfile: %s\n", yuv_filename);
                        fp = fopen(yuv_filename, "wb+");

                        if(fp != NULL) {
#ifndef BM_PCIE_MODE
                            fwrite(pFrame->buf[0], 1, ySize, fp);
                            if(testConfigPara->cbcr_interleave)
                            {
                                fwrite(pFrame->buf[1], 1, ySize/2, fp);
                            } else {
                                fwrite(pFrame->buf[1], 1, ySize/4, fp);
                                fwrite(pFrame->buf[2], 1, ySize/4, fp);
                            }
#else
                            uint8_t *buf = malloc(frameSize);

                            bmvpu_dec_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf, ySize,   BM_VPU_DEC_LITTLE_ENDIAN);
                            if(testConfigPara->cbcr_interleave)
                            {
                                bmvpu_dec_read_memory(coreIdx, (u64)(pFrame->buf[5]), buf+ySize, ySize/2, BM_VPU_DEC_LITTLE_ENDIAN);
                            } else {
                                bmvpu_dec_read_memory(coreIdx, (u64)(pFrame->buf[5]), buf+ySize, ySize/4, BM_VPU_DEC_LITTLE_ENDIAN);
                                bmvpu_dec_read_memory(coreIdx, (u64)(pFrame->buf[6]), buf+(ySize+ySize/4), ySize/4, BM_VPU_DEC_LITTLE_ENDIAN);
                            }

                            fwrite(buf, 1, frameSize, fp);
                            free(buf);
#endif
                            osal_fclose(fp);
                        }

                        if(compare == 1 && pRefMem != NULL) {
                            memset(yuv_filename, 0, 256);
                            sprintf(yuv_filename, "core%d_inst%d_frame%d_gold_%dx%d.yuv", core_idx, inst_idx, cur_frame_idx, pFrame->width, pFrame->height);

                            fp = osal_fopen(yuv_filename, "wb+");

                            if(fp != NULL) {
                                fwrite(pRefMem, 1, ySize*3/2, fp);
                                osal_fclose(fp);
                            }
                        }
                    }
                    else {
                        fprintf(stderr, "get error and ySize: %d\n", ySize);
                    }

                    if(p_stream != NULL) {
                        len = bmvpu_dec_dump_stream(vidHandle, p_stream, stream_size);

                        if(len > 0) {
                            char stream_filename[256]={0};
                            FILE *fp = NULL;

                            sprintf(stream_filename, "core%d_inst%d_len%d_stream_dump.bin", core_idx, inst_idx, len);
                            fprintf(stderr, "get error and dump streamfile: %s\n", stream_filename);
                            fp = fopen(stream_filename, "wb+");

                            if(fp != NULL) {
                                fwrite(p_stream, 1, len, fp);
                                osal_fclose(fp);
                            }
                        }
                        free(p_stream);
                    }
                }
                testConfigPara->result = result;
            }
            else {
                if(cur_frame_idx != 0 && (cur_frame_idx % 1000) == 0) {
                    dec_time = osal_gettime();
                    if(dec_time - start_time != 0) {
                        fps_enc[testConfigPara->instanceNum] = (float)(cur_frame_idx+1)*1000/(dec_time-start_time);
                    }
                }
#ifndef BM_PCIE_MODE
                if (fpOutput)
                {
                    ySize = pFrame->stride[0]*pFrame->height;
                    osal_fwrite(pFrame->buf[0], 1, ySize, fpOutput);
                    if(testConfigPara->cbcr_interleave)
                    {
                        osal_fwrite(pFrame->buf[1], 1, ySize/2, fpOutput);
                    } else {
                        osal_fwrite(pFrame->buf[1], 1, ySize/4, fpOutput);
                        osal_fwrite(pFrame->buf[2], 1, ySize/4, fpOutput);
                    }
                }
#endif
            }

#ifdef BM_PCIE_MODE
            //add write file
            if (writeYuv && frame_write_num != 0) {
                char yuv_filename[256] = { 0 };
                memset(yuv_filename,0,256);

                ySize = pFrame->stride[0] * pFrame->height;
                frameSize = ySize * 3 / 2;
                if (cur_frame_idx < testConfigPara->first_comp_idx || (cur_frame_idx - testConfigPara->first_comp_idx) % (testConfigPara->compare_skip_num + 1) != 0)
                {
                    //result = TRUE;
                    //testConfigPara->result = TRUE;
                    cur_frame_idx += 1;
                    bmvpu_dec_clear_output(vidHandle, pFrame);
                    count_enc[testConfigPara->instanceNum]++;
                    continue;
                }

                uint8_t* buf0 = NULL;
                uint8_t* buf1 = NULL;
                uint8_t* buf2 = NULL;

                if (pFrame->size > 0 && pFrame->size <= 8192 * 4096 * 3) {
                    //optimize the framebuffer cdma copy.
                    buf0 = malloc(pFrame->size);
                    if(testConfigPara->cbcr_interleave)
                    {
                        buf1 = buf0 + (unsigned int)(pFrame->buf[5] - pFrame->buf[4]);
                    } else {
                        buf1 = buf0 + (unsigned int)(pFrame->buf[5] - pFrame->buf[4]);
                        buf2 = buf0 + (unsigned int)(pFrame->buf[6] - pFrame->buf[4]);
                    }
                }
                else {
                    buf0 = NULL;
                }
                if (buf0 == NULL) {
                    printf("the frame buffer size maybe error..\n");
                    break;
                }
                bmvpu_dec_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf0, pFrame->size, BM_VPU_DEC_LITTLE_ENDIAN);
                //printf("pFrame->size = %d\n", pFrame->size);

                int core_idx = bmvpu_dec_get_core_idx(vidHandle);
                int inst_idx = bmvpu_dec_get_inst_idx(vidHandle);
                sprintf(yuv_filename, "core%d_inst%d_frame%d_dump_%dx%d.yuv", core_idx, inst_idx, cur_frame_idx, pFrame->width, pFrame->height);
                FILE* fpWriteyuv = NULL;
                fpWriteyuv = fopen(yuv_filename, "wb+");
                if (ySize != 0) {
                    if (fpWriteyuv != NULL) {
                        fwrite(buf0, 1, ySize, fpWriteyuv);
                        if(testConfigPara->cbcr_interleave)
                        {
                            fwrite(buf1, 1, ySize / 2, fpWriteyuv);
                        } else {
                            fwrite(buf1, 1, ySize / 4, fpWriteyuv);
                            fwrite(buf2, 1, ySize / 4, fpWriteyuv);
                        }
                    }
                }
                if (fpWriteyuv) {
                    osal_fflush(fpWriteyuv);
                    osal_fclose(fpWriteyuv);
                    fpWriteyuv = NULL;
                }
                if (buf0)
                    free(buf0);

                if (frame_write_num > 0)
                    frame_write_num--;
            }
#endif
            cur_frame_idx += 1;
            bmvpu_dec_clear_output(vidHandle, pFrame);
            count_enc[testConfigPara->instanceNum]++;
        }
        else
        {
            ret = bmvpu_dec_get_status(vidHandle);
            if(ret == BMDEC_STOP || ret == BMDEC_WRONG_RESOLUTION || ret == BMDEC_FRAMEBUFFER_NOTENOUGH)
                break;
#ifdef __linux__
            usleep(1);
#elif _WIN32
            Sleep(1);
#endif
        }
    }

    printf("Exit process_output!\n");
    if(compare)
    {
        osal_fflush(fpRef);
        osal_fclose(fpRef);
        printf("Inst %d: verification %d %s!\n", testConfigPara->instanceNum, compareNum, result == TRUE ? "passed" :"failed");
        if(result == FALSE) ret = -1;
    }

    if (fpOutput)
    {
        osal_fflush(fpOutput);
        osal_fclose(fpOutput);
    }

    if(pRefMem)
        free(pRefMem);

    if(pFrame)
        free(pFrame);

    return;
}

static void free_frame_buffer(bm_handle_t bm_handle, int num, bm_device_mem_t *frame_buf, bm_device_mem_t *Ytab_buf, bm_device_mem_t *Ctab_buf, int compress_cnt, int linear_cnt)
{
    int i;

    if(num > (compress_cnt + linear_cnt))
    {
        printf("free_frame_buffer: invalid frame buffer count\n");
        return;
    }

    if(frame_buf == NULL || Ytab_buf == NULL || Ctab_buf == NULL)
    {
        printf("free_frame_buffer: invalid frame buffer\n");
        return;
    }

    for(i = 0; i < num; i++)
    {
        if(frame_buf[i].size > 0 && frame_buf[i].u.device.device_addr)
            bm_free_device(bm_handle, frame_buf[i]);
        if(i < compress_cnt)
        {
            if(Ytab_buf[i].size > 0 && Ytab_buf[i].u.device.device_addr)
                bm_free_device(bm_handle, Ytab_buf[i]);
            if(Ctab_buf[i].size > 0 && Ctab_buf[i].u.device.device_addr)
                bm_free_device(bm_handle, Ctab_buf[i]);
        }
    }

    free(frame_buf);
    free(Ytab_buf);
    free(Ctab_buf);
}

static void dec_test(void* arg)
{
    osal_file_t* fpIn;
    uint8_t* pInMem;
    int32_t readLen = -1;
    BMVidStream vidStream;
    BMVidDecParam param = {0};
    BMTestConfig *testConfigPara = (BMTestConfig *)arg;
    BMVidCodHandle vidHandle;
    char *inputPath = (char *)testConfigPara->inputPath;
    osal_thread_t vpu_thread;
    int compareNum = 1, i = 0, j = 0;
    struct timeval tv;
    int wrptr = 0;
    uint8_t bFindStart, bFindEnd;
    int UsedBytes = 0;
    int framebuffer_cnt = 0;
    int compress_count = 0;
    int linear_count = 0;
    int framebuf_size, Ytab_size, Ctab_size;
    int ret = 0;
    bm_handle_t bm_handle = NULL;
    bm_device_mem_t bitstream_buf;
    bm_device_mem_t *frame_buf = NULL;
    bm_device_mem_t *Ytab_buf = NULL;
    bm_device_mem_t *Ctab_buf = NULL;
    BmVpuDecDMABuffer *vpu_frame_buf = NULL;
    BmVpuDecDMABuffer *vpu_Ytab_buf = NULL;
    BmVpuDecDMABuffer *vpu_Ctab_buf = NULL;
    int height, width, stride;
    uint32_t heap_num;
    uint32_t heap_mask = 0;

    fpIn = testConfigPara->fpIn;

    if (testConfigPara->cbcr_interleave) {
        if (testConfigPara->nv21)
            param.pixel_format = BM_VPU_DEC_PIX_FORMAT_NV21;
        else
            param.pixel_format = BM_VPU_DEC_PIX_FORMAT_NV12;
    } else {
        param.pixel_format = BM_VPU_DEC_PIX_FORMAT_YUV420P;
    }
    param.streamFormat = testConfigPara->streamFormat;
    param.wtlFormat = testConfigPara->wtlFormat;
    param.extraFrameBufferNum = testConfigPara->extraFrame;
    param.streamBufferSize = 0x700000;
    param.enable_cache = 1;
    param.bsMode = testConfigPara->bsMode;   /* VIDEO_MODE_STREAM */
    param.core_idx=-1;
    param.decode_order = testConfigPara->decode_order;
    param.picWidth = testConfigPara->width;
    param.picHeight = testConfigPara->height;
//    param.wtlFormat = 101;
#ifdef	BM_PCIE_MODE
    param.pcie_board_id = testConfigPara->pcie_board_id;
#endif

    /* alloc frame buffer outside */
    if(testConfigPara->mem_alloc_type == 1)
    {
        if(testConfigPara->min_frame_cnt <= 0 || testConfigPara->extraFrame <= 0)
        {
            printf("invalid buffer count. min_frame_cnt:%d min_frame_cnt:%d\n", testConfigPara->min_frame_cnt, testConfigPara->extraFrame);
            return;
        }

        ret = bm_dev_request(&bm_handle, 0);
        if(ret != 0)
        {
            printf("failed to open vpu handle\n");
            return;
        }

        if(param.picWidth <= 0 || param.picHeight <= 0)
        {
            printf("invalid buffer size\n");
            return;
        }

        ret = bm_get_gmem_total_heap_num(bm_handle, &heap_num);
        if(ret != 0)
        {
            printf("fail to get heap num\n");
            return;
        }

        for(i = (heap_num - 1); i >= 0; i--)
        {
            if((1 << i) && HEAP_MASK != 0)
            {
                heap_mask = (1 << i);
                break;
            }
        }

        compress_count = testConfigPara->min_frame_cnt + testConfigPara->extraFrame;
        linear_count = 0;
        if(testConfigPara->wtlFormat != BMDEC_OUTPUT_COMPRESSED)
            linear_count = testConfigPara->frame_delay + testConfigPara->extraFrame + 1;
        framebuffer_cnt = compress_count + linear_count;
        frame_buf = (bm_device_mem_t *)malloc(framebuffer_cnt * sizeof(bm_device_mem_t));
        Ytab_buf = (bm_device_mem_t *)malloc(compress_count * sizeof(bm_device_mem_t));
        Ctab_buf = (bm_device_mem_t *)malloc(compress_count * sizeof(bm_device_mem_t));
        vpu_frame_buf = (BmVpuDecDMABuffer *)malloc(framebuffer_cnt * sizeof(BmVpuDecDMABuffer));
        vpu_Ytab_buf = (BmVpuDecDMABuffer *)malloc(compress_count * sizeof(BmVpuDecDMABuffer));
        vpu_Ctab_buf = (BmVpuDecDMABuffer *)malloc(compress_count * sizeof(BmVpuDecDMABuffer));

        height = param.picHeight;
        width = param.picWidth;
        stride = VPU_ALIGN32(width);

        /* allocate compress frame buffer */
        framebuf_size = stride * VPU_ALIGN32(height) + VPU_ALIGN32(stride / 2) * VPU_ALIGN32(height);
        Ytab_size = VPU_ALIGN16(height) * VPU_ALIGN256(width) / 32;
        Ytab_size = VPU_ALIGN4096(Ytab_size) + 4096;
        Ctab_size = VPU_ALIGN16(height) * VPU_ALIGN256(width / 2) / 32;
        Ctab_size = VPU_ALIGN4096(Ctab_size) + 4096;
        for(i=0; i<compress_count; i++)
        {
            frame_buf[i].size = framebuf_size;
            Ytab_buf[i].size = Ytab_size;
            Ctab_buf[i].size = Ctab_size;

            ret = bm_malloc_device_byte_heap_mask(bm_handle, &frame_buf[i], heap_mask, framebuf_size);
            if(ret != 0)
            {
                printf("allocate compress frame buffer failed.\n");

                free_frame_buffer(bm_handle, i+1, frame_buf, Ytab_buf, Ctab_buf, compress_count, 0);
                free(vpu_frame_buf);
                free(vpu_Ytab_buf);
                free(vpu_Ctab_buf);
                return;
            }
            vpu_frame_buf[i].size = frame_buf[i].size;
            vpu_frame_buf[i].phys_addr = frame_buf[i].u.device.device_addr;

            ret = bm_malloc_device_byte_heap_mask(bm_handle, &Ytab_buf[i], heap_mask, Ytab_size);
            if(ret != 0)
            {
                printf("allocate Y table buffer failed.\n");

                free_frame_buffer(bm_handle, i+1, frame_buf, Ytab_buf, Ctab_buf, compress_count, 0);
                free(vpu_frame_buf);
                free(vpu_Ytab_buf);
                free(vpu_Ctab_buf);
                return;
            }
            vpu_Ytab_buf[i].size = Ytab_buf[i].size;
            vpu_Ytab_buf[i].phys_addr = Ytab_buf[i].u.device.device_addr;

            ret = bm_malloc_device_byte_heap_mask(bm_handle, &Ctab_buf[i], heap_mask, Ctab_size);
            if(ret != 0)
            {
                printf("allocate C table buffer failed.\n");

                free_frame_buffer(bm_handle, i+1, frame_buf, Ytab_buf, Ctab_buf, compress_count, 0);
                free(vpu_frame_buf);
                free(vpu_Ytab_buf);
                free(vpu_Ctab_buf);
                return;
            }
            vpu_Ctab_buf[i].size = Ctab_buf[i].size;
            vpu_Ctab_buf[i].phys_addr = Ctab_buf[i].u.device.device_addr;
        }

        if(testConfigPara->wtlFormat != BMDEC_OUTPUT_COMPRESSED)
        {
            if(testConfigPara->frame_delay <= 0)
            {
                printf("invalid buffer count. min_frame_cnt:%d min_frame_cnt:%d\n", testConfigPara->min_frame_cnt, testConfigPara->extraFrame);
                free_frame_buffer(bm_handle, compress_count, frame_buf, Ytab_buf, Ctab_buf, compress_count, 0);
                free(vpu_frame_buf);
                free(vpu_Ytab_buf);
                free(vpu_Ctab_buf);
                return;
            }
            /* allocate linear frame buffer */
            framebuf_size = stride * height + stride *  height / 2;
            for(i = compress_count; i < framebuffer_cnt; i++)
            {
                frame_buf[i].size = framebuf_size;
                ret = bm_malloc_device_byte_heap_mask(bm_handle, &frame_buf[i], heap_mask, framebuf_size);
                if(ret != 0)
                {
                    printf("allocate linear frame buffer failed.\n");

                    free_frame_buffer(bm_handle, i+1, frame_buf, Ytab_buf, Ctab_buf, compress_count, linear_count);
                    free(vpu_frame_buf);
                    free(vpu_Ytab_buf);
                    free(vpu_Ctab_buf);
                    return;
                }
                vpu_frame_buf[i].size = frame_buf[i].size;
                vpu_frame_buf[i].phys_addr = frame_buf[i].u.device.device_addr;
            }
        }

        /* allocate bitstream buffer */
        bitstream_buf.size = param.streamBufferSize;
        ret = bm_malloc_device_byte_heap_mask(bm_handle, &bitstream_buf, heap_mask, param.streamBufferSize);
        if(ret != 0)
        {
            printf("allocate bitstream buffer failed.\n");
            free_frame_buffer(bm_handle, framebuffer_cnt, frame_buf, Ytab_buf, Ctab_buf, compress_count, linear_count);
            free(vpu_frame_buf);
            free(vpu_Ytab_buf);
            free(vpu_Ctab_buf);
            return;
        }

        param.min_framebuf_cnt = testConfigPara->min_frame_cnt;
        param.framebuf_delay = testConfigPara->frame_delay;
        param.bitstream_buffer.size = bitstream_buf.size;
        param.bitstream_buffer.phys_addr = bitstream_buf.u.device.device_addr;
        param.frame_buffer = vpu_frame_buf;
        param.Ytable_buffer = vpu_Ytab_buf;
        param.Ctable_buffer = vpu_Ctab_buf;
    }

    if (bmvpu_dec_create(&vidHandle, param)!= 0)
    {
        fprintf(stderr, "Can't create decoder.\n");
        ret = -1;
        return;
    }

    printf("Decoder Create success, inputpath %s!\n", inputPath);

    pInMem = malloc(defaultReadBlockLen);

    vidStream.buf = pInMem;
    vidStream.header_size = 0;

    if(pInMem==NULL)
    {
        fprintf(stderr, "Can't get input memory\n");
        ret = -1;
        return;
    }

    testConfigPara->vidCodHandle = vidHandle;
    vpu_thread = osal_thread_create(process_output, (void*)testConfigPara);

    if (testConfigPara->compareNum)
        compareNum = testConfigPara->compareNum;
    else
        compareNum = 1;
#ifdef __linux__
    gettimeofday(&tv, NULL);
    srand((unsigned int)tv.tv_usec);
#elif _WIN32
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    srand((unsigned int)wtm.wSecond);
#endif


    for (i = 0; i < compareNum; i++)
    {
        while(1){
            if(testConfigPara->bsMode == BMDEC_BS_MODE_INTERRUPT){ /* BS_MODE_INTERRUPT */
#ifdef BM_PCIE_MODE
                memset(pInMem,0,defaultReadBlockLen);
#endif
                wrptr = 0;
                do{
                    readLen = (defaultReadBlockLen-wrptr) >readBlockLen? readBlockLen:(defaultReadBlockLen-wrptr);
                    if( readLen == 0 )
                        break;
                    if((readLen = osal_fread(pInMem+wrptr, 1, readLen, fpIn))>0)
                    {
                        int toLost = 0;
                        if(injectionPercent == 0)
                        {
                            wrptr += readLen;
                            if(testConfigPara->result == FALSE)
                            {
                                goto OUT1;
                            }
                        }
                        else{
                            if((rand()%100) <= injectionPercent)
                            {
                                if(injectWholeBlock)
                                {
                                    toLost = readLen;
                                    if (injectLost)
                                        continue;
                                    else
                                    {
                                        for(j=0;j<toLost;j++)
                                            *((char*)(pInMem+wrptr+j)) = rand()%256;
                                        wrptr += toLost;
                                    }
                                }
                                else{
                                    toLost = rand()%readLen;
                                    while(toLost == readLen)
                                    {
                                        toLost = rand()%readLen;
                                    }
                                    if(injectLost)
                                    {
                                        readLen -= toLost;
                                        memset(pInMem+wrptr+readLen, 0, toLost);
                                    }
                                    else
                                    {
                                        for(j=0;j<toLost;j++)
                                            *((char *)(pInMem+wrptr+readLen-toLost+j)) = rand()%256;
                                    }
                                    wrptr += readLen;
                                }
                            }
                            else
                            {
                                wrptr += readLen;
                            }
                        }
                    }
                }while(readLen > 0);
    //1682 pcie card cdma need 4B aligned
#ifdef BM_PCIE_MODE
                readLen = (wrptr + 3)/4*4;
                vidStream.length = readLen;
#else
                vidStream.length = wrptr;
#endif
            } else{
                bFindStart = 0;
                bFindEnd = 0;
                int i;

                ret = osal_fseek(fpIn, UsedBytes, SEEK_SET);
                readLen = osal_fread(pInMem, 1, defaultReadBlockLen, fpIn);
                if(readLen == 0){
                    break;
                }

                if(testConfigPara->streamFormat == BMDEC_AVC) { /* H264 */
                    for (i = 0; i < readLen - 8; i++) {
                        int tmp = pInMem[i + 3] & 0x1F;

                        if (pInMem[i] == 0 && pInMem[i + 1] == 0 && pInMem[i + 2] == 1 &&
                            (((tmp == 0x5 || tmp == 0x1) && ((pInMem[i + 4] & 0x80) == 0x80)) ||
                            (tmp == 20 && (pInMem[i + 7] & 0x80) == 0x80))) {
                            bFindStart = 1;
                            i += 8;
                            break;
                        }
                    }

                    for (; i < readLen - 8; i++) {
                        int tmp = pInMem[i + 3] & 0x1F;

                        if (pInMem[i] == 0 && pInMem[i + 1] == 0 && pInMem[i + 2] == 1 &&
                            (tmp == 15 || tmp == 7 || tmp == 8 || tmp == 6 ||
                                ((tmp == 5 || tmp == 1) && ((pInMem[i + 4] & 0x80) == 0x80)) ||
                                (tmp == 20 && (pInMem[i + 7] & 0x80) == 0x80))) {
                            bFindEnd = 1;
                            break;
                        }
                    }

                    if (i > 0)
                        readLen = i;
                    if (bFindStart == 0) {
                        printf("chn %d can not find H264 start code!readLen %d, s32UsedBytes %d.!\n",
                                (int)*((int *)vidHandle), readLen, UsedBytes);
                    }
                    if (bFindEnd == 0) {
                        readLen = i + 8;
                    }
                }
                else if(testConfigPara->streamFormat == BMDEC_HEVC) { /* H265 */
                    int bNewPic = 0;

                    for(i=0; i<readLen-6; i++){
                        int tmp = (pInMem[i + 3] & 0x7E) >> 1;

                        bNewPic = (pInMem[i + 0] == 0 && pInMem[i + 1] == 0 && pInMem[i + 2] == 1 &&
                                   (tmp <= 21) && ((pInMem[i + 5] & 0x80) == 0x80));

                        if (bNewPic) {
                            bFindStart = 1;
                            i += 6;
                            break;
                        }
                    }

                    for (; i < readLen - 6; i++) {
                        int tmp = (pInMem[i + 3] & 0x7E) >> 1;

                        bNewPic = (pInMem[i + 0] == 0 && pInMem[i + 1] == 0 && pInMem[i + 2] == 1 &&
                                    (tmp == 32 || tmp == 33 || tmp == 34 || tmp == 39 || tmp == 40 ||
                                    ((tmp <= 21) && (pInMem[i + 5] & 0x80) == 0x80)));

                        if (bNewPic) {
                            bFindEnd = 1;
                            break;
                        }
                    }
                    if (i > 0)
                        readLen = i;

                    if (bFindEnd == 0) {
                        readLen = i + 6;
                    }
                }
                else{
                    fprintf(stderr, "Error: the stream type is invalid!\n");
                    ret = -1;
                    return;
                }

                vidStream.length = readLen;
            }

            int result = 0;
            while((result = bmvpu_dec_decode(vidHandle, vidStream))!= 0){
                if(result == BM_ERR_VDEC_ILLEGAL_PARAM)
                    goto OUT1;
#ifdef __linux__
                usleep(40*1000);
#elif _WIN32
                Sleep(1);
#endif
            }

            if(testConfigPara->bsMode == BMDEC_BS_MODE_INTERRUPT){
                if (wrptr < defaultReadBlockLen){
                    break;
                }
            }
            else{
                UsedBytes += readLen;
                vidStream.pts += 30;
            }
        }
        osal_fseek(fpIn, 0, SEEK_SET);
    }
OUT1:
    while((bmvpu_dec_flush(vidHandle)) != 0){
#ifdef __linux__
        usleep(2*1000);
#elif _WIN32
        Sleep(1);
#endif
    }

    while ((ret = bmvpu_dec_get_status(vidHandle)) != BMDEC_STOP)
    {
        if(ret == BMDEC_FRAMEBUFFER_NOTENOUGH || ret == BMDEC_WRONG_RESOLUTION)
            break;
#ifdef __linux__
        usleep(2*1000);
#elif _WIN32
        Sleep(1);
#endif
    }

    osal_thread_join(vpu_thread, NULL);
    printf("EXIT\n");
    if(testConfigPara->mem_alloc_type == 1)
    {
        if(bitstream_buf.size > 0)
            bm_free_device(bm_handle, bitstream_buf);
        free_frame_buffer(bm_handle, framebuffer_cnt, frame_buf, Ytab_buf, Ctab_buf, compress_count, linear_count);
        free(vpu_frame_buf);
        free(vpu_Ytab_buf);
        free(vpu_Ctab_buf);

        bm_dev_free(bm_handle);
        bm_handle = NULL;
    }
    bmvpu_dec_delete(vidHandle);
    free(pInMem);
}

static void run(void* arg)
{
    int i;
    BMTestConfig *testConfigPara = (BMTestConfig *)arg;
    testConfigPara->fpIn = osal_fopen((char *)testConfigPara->inputPath, "rb");
    if(testConfigPara->fpIn==NULL)
    {
        fprintf(stderr, "Can't open input file: %s\n", (char *)testConfigPara->inputPath);
        ret = -1;
        return;
    }
    for(i=0; i<testConfigPara->run_times; i++)
    {
        dec_test(arg);
    }
    osal_fclose(testConfigPara->fpIn);
    return;
}

static void
Help(const char *programName)
{
    fprintf(stderr, "------------------------------------------------------------------------------\n");
    fprintf(stderr, "\tAll rights reserved by Bitmain\n");
    fprintf(stderr, "------------------------------------------------------------------------------\n");
    fprintf(stderr, "%s [option] --input bistream\n", programName);
    fprintf(stderr, "-h                 help\n");
    fprintf(stderr, "-v                 set max log level\n");
    fprintf(stderr, "                   0: none, 1: error(default), 2: warning, 3: info, 4: trace\n");
    fprintf(stderr, "-c                 compare with golden\n");
    fprintf(stderr, "                   0 : no comparison\n");
    fprintf(stderr, "                   1 : compare with golden yuv that specified --ref-yuv option\n");
    fprintf(stderr, "                   2 : compare with golden yuv md5 that specified --ref-yuv option\n");
    fprintf(stderr, "-n                 compare count\n");
    fprintf(stderr, "-m                 bitstream mode. 0 for interrupt mode, 2 for pic end mode.\n");
    fprintf(stderr, "--run_times        open and close codec count\n");
    fprintf(stderr, "--input            bitstream path\n");
    fprintf(stderr, "--output           YUV path\n");
    fprintf(stderr, "--stream-type      0,12, default 0 (H.264:0, HEVC:12)\n");
    fprintf(stderr, "--ref-yuv          golden yuv path\n");
    fprintf(stderr, "--instance         instance number\n");
    fprintf(stderr, "--write_yuv        0 no writing , num write frame numbers\n");
    fprintf(stderr, "--wtl-format       yuv format. default 0.   101: fbc data.\n");
    fprintf(stderr, "--cbcr_interleave  chorma interleave. default 0.\n");
    fprintf(stderr, "--nv21             nv21 output. default 0.\n");
    fprintf(stderr, "--width            input width\n");
    fprintf(stderr, "--height           input height\n");
    fprintf(stderr, "--extraFrame       extra frame nums. default 2.\n");
    fprintf(stderr, "--mem_alloc_type   memory allocate type. default 0: allocate memory in sdk, 1: allocate memory by user.\n");
    fprintf(stderr, "--min_frame_cnt    minimum count of frame buffer use by VPU\n");
    fprintf(stderr, "--frame_delay      minimum count of linear buffer delay.\n");
    fprintf(stderr, "--read-block-len      block length of read from file, default is 0x80000\n");
    fprintf(stderr, "--inject-percent      percent of blocks to introduce lost/scramble data, will introduce random length of data at %% of blocks, or the whole block \n");
    fprintf(stderr, "--inject-lost         type of injection, default is 1 for data lost, set to 0 for scramble the data\n");
    fprintf(stderr, "--inject-whole-block  lost the whole block, default is lost part of the block\n");
    fprintf(stderr, "--decode_order        get yuv frame by decode order, default:0\n");

#ifdef	BM_PCIE_MODE
    fprintf(stderr, "--pcie_board_id    select pcie card by pci_board_id\n");
#endif
    fprintf(stderr, "BMVPUDEC_DISPLAY_FRAMERATE:\n");
    fprintf(stderr, "                   set BMVPUDEC_DISPLAY_FRAMERATE=0  defalut 0, print all stream rate.\n");
    fprintf(stderr, "                   set BMVPUDEC_DISPLAY_FRAMERATE=1  print the frame rate of each stream\n");
}

int main(int argc, char **argv)
{
    int i = 0 , ret = 0;
    osal_thread_t vpu_thread[MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE];
    osal_thread_t monitor_thread;
    BMTestConfig *testConfigPara = malloc(MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE * sizeof(BMTestConfig));//[MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE];
    BMTestConfig testConfigOption;
    if(testConfigPara == NULL) {
        printf("can not allocate memory. please check it.\n");
        return -1;
    }
    parse_args(argc, argv, &testConfigOption);
    bmvpu_dec_set_logging_threshold(testConfigOption.log_level);

    printf("compareNum: %d\n", testConfigOption.compareNum);
    printf("instanceNum: %d\n", testConfigOption.instanceNum);

    for(i = 0; i < testConfigOption.instanceNum; i++)
        memset(&testConfigPara[i], 0, sizeof(BMTestConfig));

    for(i = 0; i < testConfigOption.instanceNum; i++)
    {
        memcpy(&(testConfigPara[i]), &(testConfigOption), sizeof(BMTestConfig));
        testConfigPara[i].instanceNum = i;
        testConfigPara[i].result = TRUE;
        printf("inputpath: %s\n", testConfigPara[i].inputPath);
        printf("outputpath: %s\n", testConfigPara[i].outputPath);
        printf("refYuvPath: %s\n", testConfigPara[i].refYuvPath);
    }

    for(i = 0; i < testConfigOption.instanceNum; i++)
    {
        vpu_thread[i] = osal_thread_create(run, (void*)&testConfigPara[i]);
    }

    monitor_thread = osal_thread_create(stat_pthread, (void*)&testConfigOption.instanceNum);

    for(i = 0; i < testConfigOption.instanceNum; i++)
    {
        osal_thread_join(vpu_thread[i], NULL);
    }

    printf("set g_exit_flag=1\n");
    g_exit_flag = 1;
    osal_thread_join(monitor_thread, NULL);
    free(testConfigPara);
    return ret;
}

static struct option   options[] = {
    {"run_times",             1, NULL, 0},
    {"output",                1, NULL, 0},
    {"input",                 1, NULL, 0},
    {"codec",                 1, NULL, 0},
    {"stream-type",           1, NULL, 0},
    {"ref-yuv",               1, NULL, 0},
    {"instance",              1, NULL, 0},
    {"write_yuv",             1, NULL, 0},
    {"wtl-format",            1, NULL, 0},
    {"cbcr_interleave",       1, NULL, 0},
    {"nv21",                  1, NULL, 0},
    {"extraFrame",            1, NULL, 0},
    {"mem_alloc_type",        1, NULL, 0},
    {"min_frame_cnt",         1, NULL, 0},
    {"width",                 1, NULL, 0},
    {"height",                1, NULL, 0},
    {"frame_delay",           1, NULL, 0},
    {"comp-skip",             1, NULL, 0},
    {"read-block-len",        1, NULL, 0},
    {"inject-percent",        1, NULL, 0},
    {"inject-lost",           1, NULL, 0},
    {"inject-whole-block",    1, NULL, 0},
#ifdef	BM_PCIE_MODE
    {"pcie_board_id",         1, NULL, 0},
#endif
    {"decode_order",          1, NULL, 0},
    {NULL,                    0, NULL, 0},
};

static int parse_args(int argc, char **argv, BMTestConfig* par)
{
    int index;
    int32_t opt;

    /* default setting. */
    memset(par, 0, sizeof(BMTestConfig));

    par->run_times = 1;
    par->instanceNum = 1;
    par->log_level = 1;
    par->bsMode = BMDEC_BS_MODE_INTERRUPT;
    par->extraFrame = 2;

    while ((opt=getopt_long(argc, argv, "v:c:h:n:m:", options, &index)) != -1)
    {
        switch (opt)
        {
        case 'c':
            par->compareType = atoi(optarg);
            break;
        case 'n':
            par->compareNum = atoi(optarg);
            break;
        case 'm':
            par->bsMode = (BmVpuDecBitStreamMode)atoi(optarg);
            break;
        case 0:
            if (!strcmp(options[index].name, "run_times"))
            {
                par->run_times = (int)atoi(optarg);
            }
            if (!strcmp(options[index].name, "output"))
            {
                memcpy(par->outputPath, optarg, strlen(optarg));
                ChangePathStyle(par->outputPath);
            }
            else if (!strcmp(options[index].name, "input"))
            {
                memcpy(par->inputPath, optarg, strlen(optarg));
                ChangePathStyle(par->inputPath);
            }
            else if (!strcmp(options[index].name, "ref-yuv"))
            {
                memcpy(par->refYuvPath, optarg, strlen(optarg));
                ChangePathStyle(par->refYuvPath);
            }
            else if (!strcmp(options[index].name, "stream-type"))
            {
                par->streamFormat = (BmVpuDecStreamFormat)atoi(optarg);
            }
            else if (!strcmp(options[index].name, "instance"))
            {
                par->instanceNum = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "write_yuv"))
            {
                par->writeYuv = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "wtl-format"))
            {
                par->wtlFormat = (BmVpuDecOutputMapType)atoi(optarg);
            }
            else if (!strcmp(options[index].name, "cbcr_interleave"))
            {
                par->cbcr_interleave = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "nv21"))
            {
                par->nv21 = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "width"))
            {
                par->width = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "height"))
            {
                par->height = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "mem_alloc_type"))
            {
                par->mem_alloc_type = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "min_frame_cnt"))
            {
                par->min_frame_cnt = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "frame_delay"))
            {
                par->frame_delay = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "extraFrame"))
            {
                par->extraFrame = atoi(optarg);
            }
            else if(!strcmp(options[index].name, "comp-skip"))
            {
                par->compare_skip_num = atoi(optarg);
            }
            else if(!strcmp(options[index].name, "read-block-len"))
            {
                readBlockLen = atoi(optarg);
            }
            else if(!strcmp(options[index].name, "inject-percent"))
            {
                injectionPercent = atoi(optarg);
            }
            else if(!strcmp(options[index].name, "inject-lost"))
            {
                injectLost = atoi(optarg);
            }
            else if(!strcmp(options[index].name, "inject-whole-block"))
            {
                injectWholeBlock = atoi(optarg);
            }
#ifdef	BM_PCIE_MODE
            else if (!strcmp(options[index].name, "pcie_board_id")) {
                par->pcie_board_id = (int)atoi(optarg);
            }
#endif
            else if (!strcmp(options[index].name, "decode_order")) {
                par->decode_order = (int)atoi(optarg);
            }
            break;
        case 'v':
            par->log_level = atoi(optarg);
            break;
        case 'h':
        case '?':
        default:
            fprintf(stderr, "%s\n", optarg);
            Help(argv[0]);
            exit(1);
        }
    }

    if(par->nv21)
        par->cbcr_interleave = 1;

    if(par->bsMode != BMDEC_BS_MODE_INTERRUPT && par->bsMode != BMDEC_BS_MODE_PIC_END)
    {
        fprintf(stderr, "Invalid bsMode(%d), 0 for interrupt mode and 2 for pic end mode\n", par->instanceNum);
        Help(argv[0]);
        exit(1);
    }

    if (par->instanceNum <= 0 || par->instanceNum > MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE)
    {
        fprintf(stderr, "Invalid instanceNum(%d)\n", par->instanceNum);
        Help(argv[0]);
        exit(1);
    }

    if (par->log_level < BMVPU_DEC_LOG_LEVEL_NONE || par->log_level > BMVPU_DEC_LOG_LEVEL_TRACE)
    {
        fprintf(stderr, "Wrong log level: %d\n", par->log_level);
        Help(argv[0]);
        exit(1);
    }

    return 0;
}
