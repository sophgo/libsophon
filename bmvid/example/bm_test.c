//--=========================================================================--
//  This file is a part of VPUAPI
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2004 - 2014   CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//-----------------------------------------------------------------------------
#include <stdio.h>
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

#include "bm_video_interface.h"
#include "bm_video_internal.h"
#include "vpuapifunc.h"
#include "main_helper.h"

#define defaultReadBlockLen 0x80000
int readBlockLen = defaultReadBlockLen;
int injectionPercent = 0;
int injectLost = 1; // default as lost data, or scramble the data
int injectWholeBlock = 0;

typedef struct BMTestConfig_struct {
    int streamFormat;
    int compareType;
    int compareNum;
    int instanceNum;

	int wirteYuv;

#ifdef	BM_PCIE_MODE
    int pcie_board_id;
#endif
    BOOL result;
    int log_level;
    int compare_skip_num;
    int first_comp_idx;

    FrameBufferFormat wtlFormat;
    BMVidCodHandle vidCodHandle;
    char outputPath[MAX_FILE_PATH];
    char inputPath[MAX_FILE_PATH];
    char refYuvPath[MAX_FILE_PATH];
} BMTestConfig;

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

static void process_output(void* arg)
{
    BMTestConfig *testConfigPara = (BMTestConfig *)arg;
    BMVidCodHandle vidCodHandle = (BMVidCodHandle)testConfigPara->vidCodHandle;
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
#ifdef BM_PCIE_MODE
    DecHandle decHandle = vidHandle->codecInst;
    int coreIdx = decHandle->coreIdx;
#endif
    BMVidFrame *pFrame=NULL;
    BOOL match, alloced = FALSE, result = TRUE;
    osal_file_t* fpRef;
    Uint8* pRefMem = NULL;
    Int32 readLen = -1, compare = 0, compareNum = 1;
    Uint32 frameSize = 0, ySize = 0, allocedSize = 0;
    int total_frame = 0, cur_frame_idx = 0;
    Uint64 start_time, dec_time;
    VLOG(INFO, "Enter process_output!\n");

#ifdef BM_PCIE_MODE
    int frame_write_num = testConfigPara->wirteYuv;
    int writeYuv = 0;
    writeYuv = testConfigPara->wirteYuv;
#endif
    if (testConfigPara->refYuvPath)
    {
        fpRef = osal_fopen(testConfigPara->refYuvPath, "rb");
        if(fpRef == NULL)
        {
            VLOG(ERR, "Can't open reference yuv file: %s\n", testConfigPara->refYuvPath);
        }
        else
        {
            compare = testConfigPara->compareType;
        }
    }
    start_time = osal_gettime();
    while(BMVidGetStatus(vidHandle)<=BMDEC_CLOSE)
    {
        if((pFrame = BMVidDecGetOutput(vidHandle)) != NULL)
        {
            //VLOG(INFO, "get a frame, display index: %d\n", pFrame->frameIdx);

            /* compare yuv if there is reference yum file */
            if(compare)
            {
                ySize = pFrame->stride[0]*pFrame->height;
                frameSize = ySize*3/2;

                if((alloced == TRUE) && (frameSize != allocedSize)) //if sequence change, free memory then malloc
                {
                    osal_free(pRefMem);
                    alloced = FALSE;
                }

                if(alloced == FALSE)
                {
                    pRefMem = osal_malloc(frameSize);
                    if(pRefMem == NULL)
                    {
                        VLOG(ERR, "Can't alloc reference yuv memory\n");
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
                    if(dec_time - start_time != 0)
                        printf("Inst %d: fps %.2f, passed!\n", testConfigPara->instanceNum, (float)total_frame*1000/(dec_time-start_time));
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
                        BMVidDecClearOutput(vidHandle, pFrame);
                        continue;
                    }

                    osal_fseek(fpRef, frameSize*cur_frame_idx, SEEK_SET);

                    readLen = osal_fread(pRefMem, 1, frameSize, fpRef);

                    if(readLen  == frameSize)
                    {
                        Uint8 *buf0;
                        Uint8 *buf1;
                        Uint8 *buf2;
#ifdef BM_PCIE_MODE
                        if(pFrame->size > 0 && pFrame->size <= 8192*4096*3) {
                            //optimize the framebuffer cdma copy.
                            buf0 = malloc(pFrame->size);
                            buf1 = buf0 + (unsigned int)(pFrame->buf[5] - pFrame->buf[4]);
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
                        vdi_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf0, pFrame->size,   VDI_LITTLE_ENDIAN);
#else
                        buf0 = pFrame->buf[0];
                        buf1 = pFrame->buf[1];
                        buf2 = pFrame->buf[2];
#endif

                        match = (memcmp(pRefMem, (void*)buf0, ySize) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            result = FALSE;
                            VLOG(ERR, "MISMATCH WITH GOLDEN Y in frame %d\n", cur_frame_idx);
                        }

                        match = (memcmp(pRefMem+ySize, (void*)buf1, ySize/4) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            result = FALSE;
                            VLOG(ERR, "MISMATCH WITH GOLDEN U in frame %d\n", cur_frame_idx);
                        }

                        match = (memcmp(pRefMem+ySize*5/4, (void*)buf2, ySize/4) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            result = FALSE;
                            VLOG(ERR, "MISMATCH WITH GOLDEN V in frame %d\n", cur_frame_idx);
                        }
#ifdef BM_PCIE_MODE
                        free(buf0);
#endif
                    }
                    else
                    {
                        result = FALSE;
                        VLOG(ERR, "NOT ENOUGH DATA\n");
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
                        BMVidDecClearOutput(vidHandle, pFrame);
                        continue;
                    }

                    osal_fseek(fpRef, 99*cur_frame_idx, SEEK_SET);
#ifdef BM_PCIE_MODE
                    Uint8 *buf0 = NULL;
                    Uint8 *buf1;
                    Uint8 *buf2;
                    if(pFrame->size > 0 && pFrame->size <= 8192*4096*3) {
                        //optimize the framebuffer cdma copy.
                        buf0 = malloc(pFrame->size);
                        buf1 = buf0 + (unsigned int)(pFrame->buf[5] - pFrame->buf[4]);
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
                    vdi_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf0, pFrame->size,   VDI_LITTLE_ENDIAN);

                    MD5(buf0, ySize, yMd);
                    MD5(buf1, ySize/4, uMd);
                    MD5(buf2, ySize/4, vMd);

                    free(buf0);
#else
                    MD5((Uint8*)pFrame->buf[0], ySize, yMd);
                    MD5((Uint8*)pFrame->buf[1], ySize/4, uMd);
                    MD5((Uint8*)pFrame->buf[2], ySize/4, vMd);
#endif

                    for(i=0; i<16; i++)
                    {
                        snprintf(yMd5Str + i*2, 2+1, "%02x", yMd[i]);
                        snprintf(uMd5Str + i*2, 2+1, "%02x", uMd[i]);
                        snprintf(vMd5Str + i*2, 2+1, "%02x", vMd[i]);
                    }

                    readLen = osal_fread(pRefMem, 1, 99, fpRef);

                    if(readLen == 99)
                    {
                        match = (osal_memcmp(pRefMem, yMd5Str, 32) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            snprintf(yRefMd5Str, 33, "%s", (char *)pRefMem);
                            result = FALSE;
                            VLOG(ERR, "MISMATCH WITH GOLDEN Y in frame %d, %s, %s\n", cur_frame_idx, yMd5Str, yRefMd5Str);
                        }

                        match = (osal_memcmp(pRefMem+33, uMd5Str, 32) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            snprintf(uRefMd5Str, 33, "%s", (char *)(pRefMem+33));
                            result = FALSE;
                            VLOG(ERR, "MISMATCH WITH GOLDEN U in frame %d, %s, %s\n", cur_frame_idx, uMd5Str, uRefMd5Str);
                        }

                        match = (osal_memcmp(pRefMem+66, vMd5Str, 32) == 0 ? TRUE : FALSE);
                        if (match == FALSE)
                        {
                            snprintf(vRefMd5Str, 33, "%s", (char *)(pRefMem+66));
                            result = FALSE;
                            VLOG(ERR, "MISMATCH WITH GOLDEN V in frame %d, %s, %s\n", cur_frame_idx, vMd5Str, vRefMd5Str);
                        }
                    }
                    else
                    {
                        result = FALSE;
                        VLOG(ERR, "NOT ENOUGH DATA\n");
                    }
                }
                if(result == FALSE && testConfigPara->result == TRUE) {
                    int stream_size = 0x700000;
                    int len = 0;
                    int core_idx = getcoreidx(vidHandle);
                    int inst_idx = BMVidVpuGetInstIdx(vidHandle);
                    unsigned char *p_stream = malloc(stream_size);
                    if(ySize != 0) {
                        char yuv_filename[256]={0};
                        FILE *fp = NULL;

                        sprintf(yuv_filename, "core%d_inst%d_frame%d_dump_%dx%d.yuv", core_idx, inst_idx, cur_frame_idx, pFrame->width, pFrame->height);
                        VLOG(ERR, "get error and dump yuvfile: %s\n", yuv_filename);
                        fp = fopen(yuv_filename, "wb+");

                        if(fp != NULL) {
#ifndef BM_PCIE_MODE
                            fwrite(pFrame->buf[0], 1, ySize, fp);
                            fwrite(pFrame->buf[1], 1, ySize/4, fp);
                            fwrite(pFrame->buf[2], 1, ySize/4, fp);
#else
                            Uint8 *buf0 = malloc(ySize);
                            Uint8 *buf1 = malloc(ySize/4);
                            Uint8 *buf2 = malloc(ySize/4);

                            vdi_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf0, ySize,   VDI_LITTLE_ENDIAN);
                            vdi_read_memory(coreIdx, (u64)(pFrame->buf[5]), buf1, ySize/4, VDI_LITTLE_ENDIAN);
                            vdi_read_memory(coreIdx, (u64)(pFrame->buf[6]), buf2, ySize/4, VDI_LITTLE_ENDIAN);

                            fwrite(buf0, 1, ySize, fp);
                            fwrite(buf1, 1, ySize/4, fp);
                            fwrite(buf2, 1, ySize/4, fp);

                            free(buf0);
                            free(buf1);
                            free(buf2);
#endif
                            fclose(fp);
                        }

                        if(compare == 1 && pRefMem != NULL) {
                            memset(yuv_filename, 0, 256);
                            sprintf(yuv_filename, "core%d_inst%d_frame%d_gold_%dx%d.yuv", core_idx, inst_idx, cur_frame_idx, pFrame->width, pFrame->height);

                            fp = fopen(yuv_filename, "wb+");

                            if(fp != NULL) {
                                fwrite(pRefMem, 1, ySize*3/2, fp);
                                fclose(fp);
                            }
                        }
                    }
                    else {
                        VLOG(ERR, "get error and ySize: %d\n", ySize);
                    }

                    if(p_stream != NULL) {
                        len = BMVidVpuDumpStream(vidHandle, p_stream, stream_size);

                        if(len > 0) {
                            char stream_filename[256]={0};
                            FILE *fp = NULL;

                            sprintf(stream_filename, "core%d_inst%d_len%d_stream_dump.bin", core_idx, inst_idx, len);
                            VLOG(ERR, "get error and dump streamfile: %s\n", stream_filename);
                            fp = fopen(stream_filename, "wb+");

                            if(fp != NULL) {
                                fwrite(p_stream, 1, len, fp);
                                fclose(fp);
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
                    if(dec_time - start_time != 0)
                        printf("Inst %d: fps %.2f, passed!\n", testConfigPara->instanceNum, (float)(cur_frame_idx+1)*1000/(dec_time-start_time));
                }
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
                    BMVidDecClearOutput(vidHandle, pFrame);
                    continue;
                }

                Uint8* buf0 = NULL;
                Uint8* buf1 = NULL;
                Uint8* buf2 = NULL;

                if (pFrame->size > 0 && pFrame->size <= 8192 * 4096 * 3) {
                    //optimize the framebuffer cdma copy.
                    buf0 = malloc(pFrame->size);
                    buf1 = buf0 + (unsigned int)(pFrame->buf[5] - pFrame->buf[4]);
                    buf2 = buf0 + (unsigned int)(pFrame->buf[6] - pFrame->buf[4]);
                }
                else {
                    buf0 = NULL;
                }
                if (buf0 == NULL) {
                    printf("the frame buffer size maybe error..\n");
                    break;
                }
                vdi_read_memory(coreIdx, (u64)(pFrame->buf[4]), buf0, pFrame->size, VDI_LITTLE_ENDIAN);
                //printf("pFrame->size = %d\n", pFrame->size);

                int core_idx = getcoreidx(vidHandle);
                int inst_idx = BMVidVpuGetInstIdx(vidHandle);
                sprintf(yuv_filename, "core%d_inst%d_frame%d_dump_%dx%d.yuv", core_idx, inst_idx, cur_frame_idx, pFrame->width, pFrame->height);
                FILE* fpWriteyuv = NULL;
                fpWriteyuv = fopen(yuv_filename, "wb+");
                if (ySize != 0) {
                    if (fpWriteyuv != NULL) {
                        fwrite(buf0, 1, ySize, fpWriteyuv);
                        fwrite(buf1, 1, ySize / 4, fpWriteyuv);
                        fwrite(buf2, 1, ySize / 4, fpWriteyuv);
                    }
                }
                if (fpWriteyuv) {
                    fflush(fpWriteyuv);
                    fclose(fpWriteyuv);
                    fpWriteyuv = NULL;
                }
                if (buf0)
                    free(buf0);

                if (frame_write_num > 0)
                    frame_write_num--;
            }
#endif
            cur_frame_idx += 1;
            BMVidDecClearOutput(vidHandle, pFrame);
        }
        else
        {
            if(BMVidGetStatus(vidHandle)==BMDEC_STOP)
                break;
#ifdef __linux__
            usleep(1);
#elif _WIN32
            Sleep(1);
#endif
        }
    }

    VLOG(INFO, "Exit process_output!\n");
    if(compare)
    {
        osal_fflush(fpRef);
        osal_fclose(fpRef);
        VLOG(INFO, "Inst %d: verification %d %s!\n", testConfigPara->instanceNum, compareNum, result == TRUE ? "passed" :"failed");
        if(result == FALSE) ret = -1;
    }
    if(pRefMem)
        osal_free(pRefMem);
}

static void dec_test(void* arg)
{
    osal_file_t* fpIn;
    Uint8* pInMem;
    Int32 readLen = -1;
    BMVidStream vidStream;
    BMVidDecParam param = {0};
    BMTestConfig *testConfigPara = (BMTestConfig *)arg;
    BMVidCodHandle vidHandle;
    char *inputPath = (char *)testConfigPara->inputPath;
    osal_thread_t vpu_thread;
    int compareNum = 1, i = 0, j = 0;
    struct timeval tv;
    int wrptr = 0;

    fpIn = osal_fopen(inputPath, "rb");
    if(fpIn==NULL)
    {
        VLOG(ERR, "Can't open input file: %s\n", inputPath);
        ret = -1;
        return;
    }

    param.streamFormat = testConfigPara->streamFormat;
    param.wtlFormat = testConfigPara->wtlFormat;
    param.extraFrameBufferNum = 1;
    param.streamBufferSize = 0x700000;
    param.enable_cache = 1;
    param.core_idx=-1;
//    param.wtlFormat = 101;
#ifdef	BM_PCIE_MODE
    param.pcie_board_id = testConfigPara->pcie_board_id;
#endif
    if (BMVidDecCreate(&vidHandle, param)!=RETCODE_SUCCESS)
    {
        VLOG(ERR, "Can't create decoder.\n");
        ret = -1;
        return;
    }

    VLOG(INFO, "Decoder Create success, inputpath %s!\n", inputPath);

    pInMem = osal_malloc(defaultReadBlockLen);

    vidStream.buf = pInMem;
    vidStream.header_size = 0;

    if(pInMem==NULL)
    {
        VLOG(ERR, "Can't get input memory\n");
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
#ifdef BM_PCIE_MODE
            osal_memset(pInMem,0,defaultReadBlockLen);
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
            int result = 0;
            while((result = BMVidDecDecode(vidHandle, vidStream))!=RETCODE_SUCCESS){
#ifdef __linux__
                usleep(10);
#elif _WIN32
                Sleep(1);
#endif
            }
            if (wrptr < defaultReadBlockLen){
                break;
            }
        }
        osal_fseek(fpIn, 0, SEEK_SET);
    }
OUT1:
    BMVidDecFlush(vidHandle);

    while (BMVidGetStatus(vidHandle)!=BMDEC_STOP)
    {
#ifdef __linux__
        usleep(2);
#elif _WIN32
        Sleep(1);
#endif
    }

    osal_thread_join(vpu_thread, NULL);
    VLOG(INFO, "EXIT\n");
    BMVidDecDelete(vidHandle);
    osal_free(pInMem);
}

static void
Help(const char *programName)
{
    fprintf(stderr, "------------------------------------------------------------------------------\n");
    fprintf(stderr, "%s(API v%d.%d.%d)\n", programName, API_VERSION_MAJOR, API_VERSION_MINOR, API_VERSION_PATCH);
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
    fprintf(stderr, "--input            bitstream path\n");
    fprintf(stderr, "--output           YUV path\n");
    fprintf(stderr, "--stream-type      0,12, default 0 (H.264:0, HEVC:12)\n");
    fprintf(stderr, "--ref-yuv          golden yuv path\n");
    fprintf(stderr, "--instance         instance number\n");
	fprintf(stderr, "--write_yuv        0 no writing , num write frame numbers\n");
    fprintf(stderr, "--wtl-format       yuv format. default 0.\n");
    fprintf(stderr, "--read-block-len      block length of read from file, default is 0x80000\n");
    fprintf(stderr, "--inject-percent      percent of blocks to introduce lost/scramble data, will introduce random length of data at %% of blocks, or the whole block \n");
    fprintf(stderr, "--inject-lost         type of injection, default is 1 for data lost, set to 0 for scramble the data\n");
    fprintf(stderr, "--inject-whole-block  lost the whole block, default is lost part of the block\n");

#ifdef	BM_PCIE_MODE
    fprintf(stderr, "--pcie_board_id    select pcie card by pci_board_id\n");
#endif
}

int main(int argc, char **argv)
{
    int i = 0;
    osal_thread_t vpu_thread[MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE];
    BMTestConfig *testConfigPara = malloc(MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE * sizeof(BMTestConfig));//[MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE];
    BMTestConfig testConfigOption;
    if(testConfigPara == NULL) {
        printf("can not allocate memory. please check it.\n");
        return -1;
    }
    parse_args(argc, argv, &testConfigOption);
    SetMaxLogLevel(testConfigOption.log_level);

    printf("compareNum: %d\n", testConfigOption.compareNum);
    printf("instanceNum: %d\n", testConfigOption.instanceNum);

    for(i = 0; i < testConfigOption.instanceNum; i++)
        osal_memset(&testConfigPara[i], 0, sizeof(BMTestConfig));

    for(i = 0; i < testConfigOption.instanceNum; i++)
    {
        memcpy(&(testConfigPara[i]), &(testConfigOption), sizeof(BMTestConfig));
        testConfigPara[i].instanceNum = i;
        testConfigPara[i].result = TRUE;
        printf("inputpath: %s\n", testConfigPara[i].inputPath);
        printf("refYuvPath: %s\n", testConfigPara[i].refYuvPath);
    }

    for(i = 0; i < testConfigOption.instanceNum; i++)
    {
        vpu_thread[i] = osal_thread_create(dec_test, (void*)&testConfigPara[i]);
    }

    for(i = 0; i < testConfigOption.instanceNum; i++)
    {
        osal_thread_join(vpu_thread[i], NULL);
    }
    free(testConfigPara);
    return ret;
}

static struct option   options[] = {
    {"output",                1, NULL, 0},
    {"input",                 1, NULL, 0},
    {"codec",                 1, NULL, 0},
    {"stream-type",           1, NULL, 0},
    {"ref-yuv",               1, NULL, 0},
    {"instance",              1, NULL, 0},
	{"write_yuv",             1, NULL, 0},
    {"wtl-format",            1, NULL, 0},
    {"comp-skip",             1, NULL, 0},
    {"read-block-len",        1, NULL, 0},
    {"inject-percent",        1, NULL, 0},
    {"inject-lost",           1, NULL, 0},
    {"inject-whole-block",    1, NULL, 0},
#ifdef	BM_PCIE_MODE
    {"pcie_board_id",         1, NULL, 0},
#endif
    {NULL,                    0, NULL, 0},
};

static int parse_args(int argc, char **argv, BMTestConfig* par)
{
    int index;
    Int32 opt;

    /* default setting. */
    osal_memset(par, 0, sizeof(BMTestConfig));

    par->instanceNum = 1;
    par->log_level = 4;

    while ((opt=getopt_long(argc, argv, "v:c:h:n:", options, &index)) != -1)
    {
        switch (opt)
        {
        case 'c':
            par->compareType = atoi(optarg);
            break;
        case 'n':
            par->compareNum = atoi(optarg);
            break;
        case 0:
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
                osal_memcpy(par->refYuvPath, optarg, strlen(optarg));
                ChangePathStyle(par->refYuvPath);
            }
            else if (!strcmp(options[index].name, "stream-type"))
            {
                par->streamFormat = (int)atoi(optarg);
            }
            else if (!strcmp(options[index].name, "instance"))
            {
                par->instanceNum = atoi(optarg);
            }
			else if (!strcmp(options[index].name, "write_yuv"))
            {
                par->wirteYuv = atoi(optarg);
            }
            else if (!strcmp(options[index].name, "wtl-format"))
            {
                par->wtlFormat = (FrameBufferFormat)atoi(optarg);
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

    if (par->instanceNum <= 0 || par->instanceNum > MAX_NUM_INSTANCE * MAX_NUM_VPU_CORE)
    {
        fprintf(stderr, "Invalid instanceNum(%d)\n", par->instanceNum);
        Help(argv[0]);
        exit(1);
    }

    if (par->log_level < NONE || par->log_level > TRACE)
    {
        fprintf(stderr, "Wrong log level: %d\n", par->log_level);
        Help(argv[0]);
        exit(1);
    }

    return 0;
}
