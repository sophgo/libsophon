/*
 * Copyright (c) 2018, Chips&Media
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif
#include "vpuapifunc.h"
#include "coda9_regdefine.h"
#include "wave5_regdefine.h"
#include "vpuerror.h"
#include "main_helper.h"
#include "misc/debug.h"
#include <stdarg.h>
#if !defined(_WIN32)
#include <getopt.h>
#endif
#if defined (PLATFORM_LINUX)
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define BIT_DUMMY_READ_GEN          0x06000000
#define BIT_READ_LATENCY            0x06000004
#define W5_SET_READ_DELAY           0x01000000
#define W5_SET_WRITE_DELAY          0x01000004
#define MAX_CODE_BUF_SIZE           (512*1024)

#ifdef _WIN32
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

#ifdef PLATFORM_WIN32
#pragma warning(disable : 4996)     //!<< disable waring C4996: The POSIX name for this item is deprecated.
#endif

char* EncPicTypeStringH264[] = {
    "IDR/I",
    "P",
};

char* EncPicTypeStringMPEG4[] = {
    "I",
    "P",
};

void SetDefaultDecTestConfig(TestDecConfig* testConfig)
{
    osal_memset(testConfig, 0, sizeof(TestDecConfig));

    testConfig->bitstreamMode   = BS_MODE_INTERRUPT;
    testConfig->feedingMode     = FEEDING_METHOD_FIXED_SIZE;
    testConfig->streamEndian    = VPU_STREAM_ENDIAN;
    testConfig->frameEndian     = VPU_FRAME_ENDIAN;
    testConfig->cbcrInterleave  = FALSE;
    testConfig->nv21            = FALSE;
    testConfig->bitFormat       = STD_HEVC;
    testConfig->renderType      = RENDER_DEVICE_NULL;
    testConfig->mapType         = COMPRESSED_FRAME_MAP;
    testConfig->enableWTL       = TRUE;
    testConfig->wtlMode         = FF_FRAME;
    testConfig->wtlFormat       = FORMAT_420;           //!<< 8 bit YUV
    testConfig->wave.bwOptimization = FALSE;            //!<< TRUE: non-ref pictures are not stored at reference picture buffer
                                                        //!<<       for saving bandwidth.
    testConfig->fps             = 30;
}

Uint32 randomSeed;
static BOOL initializedRandom;
Uint32 GetRandom(
    Uint32   start,
    Uint32   end
    )
{
    Uint32   range = end-start+1;

    if (randomSeed == 0) {
        randomSeed = (Uint32)time(NULL);
        VLOG(INFO, "======= RANDOM SEED: %08x ======\n", randomSeed);
    }
    if (initializedRandom == FALSE) {
        srand(randomSeed);
        initializedRandom = TRUE;
    }

    if (range == 0) {
        VLOG(ERR, "%s:%d RANGE IS 0\n", __FUNCTION__, __LINE__);
        return 0;
    }
    else {
        return ((rand()%range) + start);
    }
}
#if defined(PLATFORM_LINUX) || defined(PLATFORM_QNX)
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <link.h>
#endif
#include <stdlib.h>
Int32 LoadFirmware(
    Int32       productId,
    Uint8**     retFirmware,
    Uint32*     retSizeInWord,
    const char* path
    )
{
    Int32       nread;
    Uint32      totalRead, allocSize, readSize = 1024*1024;
    Uint8*      firmware = NULL;
    osal_file_t fp = NULL;

    if ((fp=osal_fopen(path, "rb")) == NULL)
    {
#if defined(PLATFORM_LINUX)
        //check firmware path
        char addr[255] = "/system/data/lib/vpu_firmware/";
        strcat(addr, path);
        if((fp=osal_fopen(addr, "rb")) == NULL) {
            void *handle = dlopen("libbmvideo.so", RTLD_NOW);
            struct link_map *map;
            if (handle == NULL) {
                fprintf(stderr, "dlopen() failed: %s\n", dlerror());
                return -1;
            }
            if(dlinfo(handle, RTLD_DI_LINKMAP, &map) != -1) {
                char *ptr = strrchr(map->l_name, '/');
                if(ptr) {
                    int name_len = ptr - map->l_name + 1;
                    printf("libbmvideo.so addr : %s, name_len: %d\n", map->l_name, name_len);

                    if(name_len > 0) {
                        memset(addr, 0, 255);
                        strncpy(addr, map->l_name, name_len);
                        strcat(addr, "vpu_firmware/");
                        strcat(addr, path);
                        printf("vpu firmware addr: %s\n", addr);
                        fp=osal_fopen(addr, "rb");
                    }
                }
                else {
                    VLOG(ERR, "can't get the absolute pathname of libbmvideo.so\n");
                    dlclose(handle);
                    return -1;
                }
            }
            else {
                printf("can't get addr of libbmvideo.so file.\n");
                dlclose(handle);
                return -1;
            }
            int ret = dlclose(handle);
            if (ret != 0)
                VLOG(WARN, "WARNING! dlclose failed.\n");
        }
        else {
            VLOG(INFO, "vpu firmware %s open.", addr);
        }
#endif
#ifdef _WIN32
	    char fw_path[512] = "";
	    char* ptr;
	    int dirname_len;
	    int ret;

	    /*test C:\\vpu_firmware\\chagall.bin */
	    memset(fw_path, 0, 512);
	    strcpy(fw_path, "C:\\vpu_firmware\\");// It can be replace path in the future
	    strcat(fw_path,path);
	    ret = _access(fw_path, 0);
	    if (ret == 0) {
	        fp = osal_fopen(fw_path, "rb");
	    }
	    else {
	        LPTSTR  strDLLPath1 = (void*)malloc(512);
	        GetModuleFileName((HINSTANCE)&__ImageBase, strDLLPath1, _MAX_PATH);
	        ptr = strrchr(strDLLPath1, '\\');

	        if (!ptr)
	        {
	            printf("can't get vpu_firmware path by libbmvideo.dll\n");
	        }

	        dirname_len = strlen(strDLLPath1) - strlen(ptr) + 1;
	        if (dirname_len <= 0)
	        {
	            printf("can't get vpu_firmware path by libbmvideo.dll\n");
	        }
	        memset(fw_path, 0, 512);
	        strncpy(fw_path, strDLLPath1, dirname_len);
	        strcat(fw_path, "vpu_firmware\\");
	        strcat(fw_path, path);
	        free(strDLLPath1);
	        fp = osal_fopen(fw_path, "rb");
	    }
#endif
    }

    if(fp==NULL) {
        printf("please put the vpu firmware to right way..\n");
        return -1;
    }

    totalRead = 0;
    if (PRODUCT_ID_W_SERIES(productId))
    {
        firmware = (Uint8*)osal_malloc(readSize);
        allocSize = readSize;
        nread = 0;
        while (TRUE)
        {
            if (allocSize < (totalRead+readSize))
            {
                allocSize += 2*nread;
                firmware = (Uint8*)osal_realloc(firmware, allocSize);
            }
            nread = osal_fread((void*)&firmware[totalRead], 1, readSize, fp);//lint !e613
            totalRead += nread;
            if (nread < (Int32)readSize)
                break;
        }
        *retSizeInWord = (totalRead+1)/2;
    }
    else
    {
        Uint16*     pusBitCode;
        pusBitCode = (Uint16 *)osal_malloc(MAX_CODE_BUF_SIZE);
        firmware   = (Uint8*)pusBitCode;
        if (pusBitCode)
        {
            int code;
            while (!osal_feof(fp) || totalRead < (MAX_CODE_BUF_SIZE/2)) {
                code = -1;
                if (osal_fscanf(fp, "%x", &code) <= 0) {
                    /* matching failure or EOF */
                    break;
                }
                pusBitCode[totalRead++] = (Uint16)code;
            }
        }
        *retSizeInWord = totalRead;
    }

    osal_fclose(fp);

    *retFirmware   = firmware;

    return 0;
}


void PrintVpuVersionInfo(
    Uint32   core_idx
    )
{
    Uint32 version;
    Uint32 revision;
    Uint32 productId;

    VPU_GetVersionInfo(core_idx, &version, &revision, &productId);

    VLOG(INFO, "VPU coreNum : [%d]\n", core_idx);
    VLOG(INFO, "Firmware : CustomerCode: %04x | version : %d.%d.%d rev.%d\n",
        (Uint32)(version>>16), (Uint32)((version>>(12))&0x0f), (Uint32)((version>>(8))&0x0f), (Uint32)((version)&0xff), revision);
    VLOG(INFO, "Hardware : %04x\n", productId);
    VLOG(INFO, "API      : %d.%d.%d\n\n", API_VERSION_MAJOR, API_VERSION_MINOR, API_VERSION_PATCH);
}

BOOL OpenDisplayBufferFile(CodStd codec, char *outputPath, VpuRect rcDisplay, TiledMapType mapType, FILE *fp[])
{
    char strFile[MAX_FILE_PATH];
    int width;
    int height;

    width = rcDisplay.right - rcDisplay.left;
    height = rcDisplay.bottom - rcDisplay.top;

    if (mapType == LINEAR_FRAME_MAP) {
        if ((fp[0]=osal_fopen(outputPath, "wb")) == NULL) {
            VLOG(ERR, "%s:%d failed to open %s\n", __FUNCTION__, __LINE__, outputPath);
            goto ERR_OPEN_DISP_BUFFER_FILE;
        }
    }
    else {
        width  = (codec == STD_HEVC || codec == STD_AVS2) ? VPU_ALIGN16(width)  : VPU_ALIGN64(width);
        height = (codec == STD_HEVC || codec == STD_AVS2) ? VPU_ALIGN16(height) : VPU_ALIGN64(height);
        sprintf(strFile, "%s_%dx%d_fbc_data_y.bin", outputPath, width, height);
        if ((fp[0]=osal_fopen(strFile, "wb")) == NULL) {
            VLOG(ERR, "%s:%d failed to open %s\n", __FUNCTION__, __LINE__, strFile);
            goto ERR_OPEN_DISP_BUFFER_FILE;
        }
        sprintf(strFile, "%s_%dx%d_fbc_data_c.bin", outputPath, width, height);
        if ((fp[1]=osal_fopen(strFile, "wb")) == NULL) {
            VLOG(ERR, "%s:%d failed to open %s\n", __FUNCTION__, __LINE__, strFile);
            goto ERR_OPEN_DISP_BUFFER_FILE;
        }
        sprintf(strFile, "%s_%dx%d_fbc_table_y.bin", outputPath, width, height);
        if ((fp[2]=osal_fopen(strFile, "wb")) == NULL) {
            VLOG(ERR, "%s:%d failed to open %s\n", __FUNCTION__, __LINE__, strFile);
            goto ERR_OPEN_DISP_BUFFER_FILE;
        }
        sprintf(strFile, "%s_%dx%d_fbc_table_c.bin", outputPath, width, height);
        if ((fp[3]=osal_fopen(strFile, "wb")) == NULL) {
            VLOG(ERR, "%s:%d failed to open %s\n", __FUNCTION__, __LINE__, strFile);
            goto ERR_OPEN_DISP_BUFFER_FILE;
        }
    }
    return TRUE;
ERR_OPEN_DISP_BUFFER_FILE:
    CloseDisplayBufferFile(fp);
    return FALSE;
}

void CloseDisplayBufferFile(FILE *fp[])
{
    int i;
    for (i=0; i < OUTPUT_FP_NUMBER; i++) {
        if (fp[i] != NULL) {
            osal_fclose(fp[i]);
            fp[i] = NULL;
        }
    }
}

RetCode VPU_GetFBCOffsetTableSize(CodStd codStd, int width, int height, int* ysize, int* csize)
{
    if (ysize == NULL || csize == NULL)
        return RETCODE_INVALID_PARAM;

    *ysize = ProductCalculateAuxBufferSize(AUX_BUF_TYPE_FBC_Y_OFFSET, codStd, width, height);
    *csize = ProductCalculateAuxBufferSize(AUX_BUF_TYPE_FBC_C_OFFSET, codStd, width, height);

    return RETCODE_SUCCESS;
}

void SaveDisplayBufferToFile(DecHandle handle, CodStd codStd, FrameBuffer dispFrame, VpuRect rcDisplay, FILE *fp[])
{
    Uint32   width;
    Uint32   height;
    Uint32   bpp;
    DecGetFramebufInfo  fbInfo;

    if (dispFrame.myIndex < 0)
        return;

    if (dispFrame.mapType != COMPRESSED_FRAME_MAP) {
        size_t sizeYuv;
        Uint8*   pYuv;
        pYuv = GetYUVFromFrameBuffer(handle, &dispFrame, rcDisplay, &width, &height, &bpp, &sizeYuv);
        osal_fwrite(pYuv, 1, sizeYuv, fp[0]);
        osal_free(pYuv);
    }
    else {
        Uint32  lumaTblSize;
        Uint32  chromaTblSize;
        Uint32  lSize;
        Uint32  cSize;
        PhysicalAddress  addr;
        Uint32  endian;
        Uint8*  buf;
        Uint32  coreIdx     = VPU_HANDLE_CORE_INDEX(handle);
        Uint32  productId   = VPU_HANDLE_PRODUCT_ID(handle);
        Uint32  cFrameStride;

        VPU_DecGiveCommand(handle, DEC_GET_FRAMEBUF_INFO, (void*)&fbInfo);

        width  = rcDisplay.right - rcDisplay.left;
        width  = (codStd == STD_HEVC || codStd == STD_AVS2) ? VPU_ALIGN16(width)  : VPU_ALIGN64(width);
        height = rcDisplay.bottom - rcDisplay.top;
        height = (codStd == STD_HEVC || codStd == STD_AVS2) ? VPU_ALIGN16(height) : VPU_ALIGN64(height);

        cFrameStride = CalcStride(width, height, dispFrame.format, dispFrame.cbcrInterleave, dispFrame.mapType, (codStd == STD_VP9));
        lSize        = CalcLumaSize(productId,   cFrameStride, height, dispFrame.format, dispFrame.cbcrInterleave, dispFrame.mapType, NULL);
        cSize        = CalcChromaSize(productId, cFrameStride, height, dispFrame.format, dispFrame.cbcrInterleave, dispFrame.mapType, NULL) * 2;

        // Invert the endian because the pixel order register doesn't affect the FBC data.
        endian = (~(Uint32)dispFrame.endian & 0xf) | 0x10;

        /* Dump Y compressed data */
        if ((buf=(Uint8*)osal_malloc(lSize)) != NULL) {
            vdi_read_memory(coreIdx, dispFrame.bufY, buf, lSize, endian);
            osal_fwrite((void *)buf, sizeof(Uint8), lSize, fp[0]);
            osal_fflush(fp[0]);
            osal_free(buf);
        }
        else {
            VLOG(ERR, "%s:%d Failed to save the luma compressed data!!\n", __FUNCTION__, __LINE__);
        }

        /* Dump C compressed data */
        if ((buf=(Uint8*)osal_malloc(cSize)) != NULL) {
            vdi_read_memory(coreIdx, dispFrame.bufCb, buf, cSize, endian);
            osal_fwrite((void *)buf, sizeof(Uint8), cSize, fp[1]);
            osal_fflush(fp[1]);
            osal_free(buf);
        }
        else {
            VLOG(ERR, "%s:%d Failed to save the chroma compressed data!!\n", __FUNCTION__, __LINE__);
        }

        /* Dump Y Offset table */
        VPU_GetFBCOffsetTableSize(codStd, (int)width, (int)height, (int*)&lumaTblSize, (int*)&chromaTblSize);

        addr = fbInfo.vbFbcYTbl[dispFrame.myIndex].phys_addr;

        if ((buf=(Uint8*)osal_malloc(lumaTblSize)) != NULL) {
            vdi_read_memory(coreIdx, addr, buf, lumaTblSize, endian);
            osal_fwrite((void *)buf, sizeof(Uint8), lumaTblSize, fp[2]);
            osal_fflush(fp[2]);
            osal_free(buf);
        }
        else {
            VLOG(ERR, "%s:%d Failed to save the offset table of luma!\n", __FUNCTION__, __LINE__);
        }

        /* Dump C Offset table */
        addr = fbInfo.vbFbcCTbl[dispFrame.myIndex].phys_addr;

        if ((buf=(Uint8*)osal_malloc(chromaTblSize)) != NULL) {
            vdi_read_memory(coreIdx, addr, buf, chromaTblSize, endian);
            osal_fwrite((void *)buf, sizeof(Uint8), chromaTblSize, fp[3]);
            osal_fflush(fp[3]);
            osal_free(buf);
        }
        else {
            VLOG(ERR, "%s:%d Failed to save the offset table of chroma!\n", __FUNCTION__, __LINE__);
        }
    }
}

void FreePreviousFramebuffer(
    Uint32              coreIdx,
    DecGetFramebufInfo* fb
    )
{
    int i;

    vdi_lock(coreIdx);
    if (fb->vbFrame.size > 0) {
        vdi_free_dma_memory(coreIdx, &fb->vbFrame);
        osal_memset((void*)&fb->vbFrame, 0x00, sizeof(vpu_buffer_t));
    }
    if (fb->vbWTL.size > 0) {
        vdi_free_dma_memory(coreIdx, &fb->vbWTL);
        osal_memset((void*)&fb->vbWTL, 0x00, sizeof(vpu_buffer_t));
    }
    for ( i=0 ; i<MAX_REG_FRAME; i++) {
        if (fb->vbFbcYTbl[i].size > 0) {
            vdi_free_dma_memory(coreIdx, &fb->vbFbcYTbl[i]);
            osal_memset((void*)fb->vbFbcYTbl, 0x00, sizeof(vpu_buffer_t));
        }
        if (fb->vbFbcCTbl[i].size > 0) {
            vdi_free_dma_memory(coreIdx, &fb->vbFbcCTbl[i]);
            osal_memset((void*)fb->vbFbcCTbl, 0x00, sizeof(vpu_buffer_t));
        }
    }
    vdi_unlock(coreIdx);
}

void PrintDecSeqWarningMessages(
    Uint32          productId,
    DecInitialInfo* seqInfo
    )
{
    if (PRODUCT_ID_W_SERIES(productId))
    {
        if (seqInfo->seqInitErrReason&0x00000001) VLOG(WARN, "sps_max_sub_layer_minus1 shall be 0 to 6\n");
        if (seqInfo->seqInitErrReason&0x00000002) VLOG(WARN, "general_reserved_zero_44bits shall be 0.\n");
        if (seqInfo->seqInitErrReason&0x00000004) VLOG(WARN, "reserved_zero_2bits shall be 0\n");
        if (seqInfo->seqInitErrReason&0x00000008) VLOG(WARN, "sub_layer_reserved_zero_44bits shall be 0");
        if (seqInfo->seqInitErrReason&0x00000010) VLOG(WARN, "general_level_idc shall have one of level of Table A.1\n");
        if (seqInfo->seqInitErrReason&0x00000020) VLOG(WARN, "sps_max_dec_pic_buffering[i] <= MaxDpbSize\n");
        if (seqInfo->seqInitErrReason&0x00000040) VLOG(WARN, "trailing bits shall be 1000... pattern, 7.3.2.1\n");
        if (seqInfo->seqInitErrReason&0x00100000) VLOG(WARN, "Not supported or undefined profile: %d\n", seqInfo->profile);
        if (seqInfo->seqInitErrReason&0x00200000) VLOG(WARN, "Spec over level(%d)\n", seqInfo->level);
    }
}

void DisplayDecodedInformationForHevc(
    DecHandle      handle,
    Uint32         frameNo,
    BOOL           performance,
    DecOutputInfo* decodedInfo
    )
{
    Int32 logLevel;
    QueueStatusInfo queueStatus;

    VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, &queueStatus);

    if (decodedInfo == NULL) {
        if ( performance == TRUE ) {
            VLOG(INFO, "                                                                                                                    | FRAME  |  HOST | SEEK_S SEEK_E    SEEK  | PARSE_S PARSE_E  PARSE  | DEC_S  DEC_E   DEC   |\n");
            VLOG(INFO, "I    NO  T     POC     NAL DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH      SEQ  TEMP | CYCLE  |  TICK |  TICK   TICK     CYCLE |  TICK    TICK    CYCLE  |  TICK   TICK   CYCLE | TQ IQ\n");
            VLOG(INFO, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        }
        else {
            VLOG(INFO, "I    NO  T     POC     NAL DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH      SEQ  TEMP  CYCLE  (   Seek,   Parse,    Dec)    TQ IQ\n");
            VLOG(INFO, "---------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        }
    }
    else {
        logLevel = (decodedInfo->decodingSuccess&0x01) == 0 ? WARN : INFO;
        // Print informations
        if ( performance == TRUE ) {
            VLOG(logLevel, "%02d %5d %d %4d(%4d) %3d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %4d  %4d  %8d %8d (%6d,%6d,%8d) (%6d,%6d,%8d) (%6d,%6d,%8d) %d %d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->h265Info.decodedPOC, decodedInfo->h265Info.displayPOC, decodedInfo->nalType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->h265Info.temporalId,
                decodedInfo->frameCycle, decodedInfo->decHostCmdTick,
                decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle,
                decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle,
                decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
        else {
            VLOG(logLevel, "%02d %5d %d %4d(%4d) %3d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %4d  %4d  %8d(%8d,%8d,%8d) %d %d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->h265Info.decodedPOC, decodedInfo->h265Info.displayPOC, decodedInfo->nalType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->h265Info.temporalId,
                decodedInfo->frameCycle, decodedInfo->seekCycle, decodedInfo->parseCycle, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
        if (logLevel == ERR) {
            VLOG(ERR, "\t>>ERROR REASON: 0x%08x(0x%08x)\n", decodedInfo->errorReason, decodedInfo->errorReasonExt);
        }
        if (decodedInfo->numOfErrMBs) {
            VLOG(WARN, "\t>> ErrorBlock: %d\n", decodedInfo->numOfErrMBs);
        }
    }
}

void DisplayDecodedInformationForAVS2(
    DecHandle      handle,
    Uint32         frameNo,
    BOOL           performance,
    DecOutputInfo* decodedInfo
    )
{
    Int32 logLevel;
    QueueStatusInfo queueStatus;

    VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, &queueStatus);

    if (decodedInfo == NULL) {
        if ( performance == TRUE ) {
            VLOG(INFO, "                                                                                                                    | FRAME  |  HOST | SEEK_S SEEK_E    SEEK  | PARSE_S PARSE_E  PARSE  | DEC_S  DEC_E   DEC   |\n");
            VLOG(INFO, "I    NO  T     POC     NAL DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH      SEQ  TEMP | CYCLE  |  TICK |  TICK   TICK     CYCLE |  TICK    TICK    CYCLE  |  TICK   TICK   CYCLE | TQ IQ\n");
        } else {
            VLOG(INFO, "I    NO  T     POC     NAL DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH      SEQ  TEMP  CYCLE (Seek, Parse, Dec) TQ IQ\n");
        }
        VLOG(INFO, "------------------------------------------------------------------------------------------------------------\n");
    }
    else {
        logLevel = (decodedInfo->decodingSuccess&0x01) == 0 ? ERR : TRACE;
        // Print informations
        if (performance == TRUE) {
            VLOG(logLevel, "%02d %5d %d %4d(%4d) %3d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %4d  %4d  %8d %8d (%6d,%6d,%8d) (%6d,%6d,%8d) (%6d,%6d,%8d) %d %d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->avs2Info.decodedPOI, decodedInfo->avs2Info.displayPOI, decodedInfo->nalType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->avs2Info.temporalId,
                decodedInfo->frameCycle, decodedInfo->decHostCmdTick,
                decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle,
                decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle,
                decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
        else {
            VLOG(logLevel, "%02d %5d %d %4d(%4d) %3d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %4d  %4d  %8d(%8d,%8d,%8d) %d %d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->avs2Info.decodedPOI, decodedInfo->avs2Info.displayPOI, decodedInfo->nalType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->avs2Info.temporalId,
                decodedInfo->frameCycle, decodedInfo->seekCycle, decodedInfo->parseCycle, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
        if (logLevel == ERR) {
            VLOG(ERR, "\t>>ERROR REASON: 0x%08x(0x%08x)\n", decodedInfo->errorReason, decodedInfo->errorReasonExt);
        }
        if (decodedInfo->numOfErrMBs) {
            VLOG(WARN, "\t>> ErrorBlock: %d\n", decodedInfo->numOfErrMBs);
        }
    }
}

void DisplayDecodedInformationForVP9(
    DecHandle handle,
    Uint32 frameNo,
    BOOL           performance,
    DecOutputInfo* decodedInfo)
{
    Int32 logLevel;
    QueueStatusInfo queueStatus;

    VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, &queueStatus);

    if (decodedInfo == NULL) {
        // Print header
        if ( performance == TRUE ) {
            VLOG(INFO, "                                                                                                | FRAME  |  HOST | SEEK_S SEEK_E    SEEK  | PARSE_S PARSE_E  PARSE  | DEC_S  DEC_E   DEC   |\n");
            VLOG(INFO, "I    NO  T  DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH      SEQ  | CYCLE  |  TICK |  TICK   TICK     CYCLE |  TICK    TICK    CYCLE  |  TICK   TICK   CYCLE | TQ IQ\n");
        }
        else {
            VLOG(INFO, "I    NO  T  DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH      SEQ  CYCLE (Seek, Parse, Dec)  TQ IQ\n");
        }
        VLOG(INFO, "--------------------------------------------------------------------------------------------\n");
    }
    else {
        logLevel = (decodedInfo->decodingSuccess&0x01) == 0 ? ERR : TRACE;
        // Print informations
        if (performance == TRUE) {
            VLOG(logLevel, "%02d %5d %d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %4d %8d %8d (%6d,%6d,%8d) (%6d,%6d,%8d) (%6d,%6d,%8d) %d %d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->frameCycle, decodedInfo->decHostCmdTick,
                decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle,
                decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle,
                decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
        else {
            VLOG(logLevel, "%02d %5d %d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %4d  %8d (%8d,%8d,%8d) %d %d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->frameCycle, decodedInfo->seekCycle, decodedInfo->parseCycle, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
        if (logLevel == ERR) {
            VLOG(ERR, "\t>>ERROR REASON: 0x%08x(0x%08x)\n", decodedInfo->errorReason, decodedInfo->errorReasonExt);
        }
        if (decodedInfo->numOfErrMBs) {
            VLOG(WARN, "\t>> ErrorBlock: %d\n", decodedInfo->numOfErrMBs);
        }
    }
}

void DisplayDecodedInformationForAVC(
    DecHandle      handle,
    Uint32         frameNo,
    BOOL           performance,
    DecOutputInfo* decodedInfo)
{
    Int32 logLevel;
    QueueStatusInfo queueStatus;

    VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, &queueStatus);

    if (decodedInfo == NULL) {
        // Print header
        if ( performance == TRUE ) {
            VLOG(INFO, "                                                                                              | FRAME  |  HOST | SEEK_S SEEK_E    SEEK  | PARSE_S PARSE_E  PARSE  | DEC_S  DEC_E   DEC   |\n");
            VLOG(INFO, "I    NO  T  DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH     SEQ | CYCLE  |  TICK |  TICK   TICK     CYCLE |  TICK    TICK    CYCLE  |  TICK   TICK   CYCLE | TQ IQ\n");
        }
        else {
            VLOG(INFO, "I    NO  T  DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE    WxH     SEQ   CYCLE  (  Seek,   Parse,   Dec)     TQ IQ\n");
        }
        VLOG(INFO, "-----------------------------------------------------------------------------------------------------------------------------------------\n");
    }
    else {
        logLevel = (decodedInfo->decodingSuccess&0x01) == 0 ? ERR : INFO;
        // Print informations
        if (performance == TRUE) {
            VLOG(logLevel, "%02d %5d %d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %3d  %8d %8d (%6d,%6d,%8d) (%6d,%6d,%8d) (%6d,%6d,%8d) %d %d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->frameCycle, decodedInfo->decHostCmdTick,
                decodedInfo->decSeekStartTick, decodedInfo->decSeekEndTick, decodedInfo->seekCycle,
                decodedInfo->decParseStartTick, decodedInfo->decParseEndTick, decodedInfo->parseCycle,
                decodedInfo->decDecodeStartTick, decodedInfo->decDecodeEndTick, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount
                );
        }
        else {
            VLOG(logLevel, "%02d %5d %d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %4dx%-4d %3d  %8d (%8d,%8d,%8d) %2d %2d\n",
                handle->instIndex, frameNo, decodedInfo->picType,
                decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
                decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
                decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
                decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
                decodedInfo->dispPicWidth, decodedInfo->dispPicHeight, decodedInfo->sequenceNo,
                decodedInfo->frameCycle, decodedInfo->seekCycle, decodedInfo->parseCycle, decodedInfo->DecodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount
                );
        }

        if (logLevel == ERR) {
            VLOG(ERR, "\t>>ERROR REASON: 0x%08x(0x%08x)\n", decodedInfo->errorReason, decodedInfo->errorReasonExt);
        }
        if (decodedInfo->numOfErrMBs) {
            VLOG(WARN, "\t>> ErrorBlock: %d\n", decodedInfo->numOfErrMBs);
        }
    }
}

void DisplayDecodedInformationCommon(
    DecHandle      handle,
    Uint32         frameNo,
    DecOutputInfo* decodedInfo)
{
    Int32 logLevel;

    if (decodedInfo == NULL) {
        // Print header
        VLOG(TRACE, "I    NO  T  DECO   DISP  DISPFLAG  RD_PTR   WR_PTR  FRM_START FRM_END FRM_SIZE WxH  \n");
        VLOG(TRACE, "---------------------------------------------------------------------------\n");
    }
    else {
        VpuRect rc    = decodedInfo->rcDisplay;
        Uint32 width  = rc.right - rc.left;
        Uint32 height = rc.bottom - rc.top;
        logLevel = (decodedInfo->decodingSuccess&0x01) == 0 ? WARN : INFO;
        // Print informations
        VLOG(logLevel, "%02d %5d %d %2d(%2d) %2d(%2d) %08x %08x %08x %08x %08x %8d %dx%d\n",
            handle->instIndex, frameNo, decodedInfo->picType,
            decodedInfo->indexFrameDecoded, decodedInfo->indexFrameDecodedForTiled,
            decodedInfo->indexFrameDisplay, decodedInfo->indexFrameDisplayForTiled,
            decodedInfo->frameDisplayFlag,decodedInfo->rdPtr, decodedInfo->wrPtr,
            decodedInfo->bytePosFrameStart, decodedInfo->bytePosFrameEnd, (decodedInfo->bytePosFrameEnd - decodedInfo->bytePosFrameStart),
            width, height);
        if (logLevel == ERR) {
            VLOG(ERR, "\t>>ERROR REASON: 0x%08x(0x%08x)\n", decodedInfo->errorReason, decodedInfo->errorReasonExt);
        }
        if (decodedInfo->numOfErrMBs) {
            VLOG(WARN, "\t>> ErrorBlock: %d\n", decodedInfo->numOfErrMBs);
        }
    }
}

/**
* \brief                   Print out decoded information such like RD_PTR, WR_PTR, PIC_TYPE, ..
* \param   decodedInfo     If this parameter is not NULL then print out decoded informations
*                          otherwise print out header.
*/
void
    DisplayDecodedInformation(
    DecHandle      handle,
    CodStd         codec,
    Uint32         frameNo,
    DecOutputInfo* decodedInfo,
    ...
    )
{
    int performance = FALSE;
    va_list         ap;

    va_start(ap, decodedInfo);
    performance = va_arg(ap, Uint32);
    va_end(ap);
    switch (codec)
    {
    case STD_HEVC:
        DisplayDecodedInformationForHevc(handle, frameNo, performance, decodedInfo);
        break;
    case STD_VP9:
        DisplayDecodedInformationForVP9(handle, frameNo, performance, decodedInfo);
        break;
    case STD_AVS2:
        DisplayDecodedInformationForAVS2(handle, frameNo, performance, decodedInfo);
        break;
    case STD_AVC:
        DisplayDecodedInformationForAVC(handle, frameNo, performance, decodedInfo);
        break;
    default:
        DisplayDecodedInformationCommon(handle, frameNo, decodedInfo);
        break;
    }

    return;
}

void SetDefaultEncTestConfig(TestEncConfig* testConfig) {
    osal_memset(testConfig, 0, sizeof(TestEncConfig));

    testConfig->stdMode        = STD_HEVC;
    testConfig->frame_endian   = VPU_FRAME_ENDIAN;
    testConfig->stream_endian  = VPU_STREAM_ENDIAN;
    testConfig->source_endian  = VPU_SOURCE_ENDIAN;
    testConfig->mapType        = COMPRESSED_FRAME_MAP;
    testConfig->lineBufIntEn   = TRUE;

}


static void Wave5DisplayEncodedInformation(
    EncHandle       handle,
    CodStd          codec,
    Uint32          frameNo,
    EncOutputInfo*  encodedInfo,
    Int32           srcEndFlag,
    Int32           srcFrameIdx,
    Int32           performance
    )
{
    QueueStatusInfo queueStatus;

    VPU_EncGiveCommand(handle, ENC_GET_QUEUE_STATUS, &queueStatus);

    if (encodedInfo == NULL) {
        if (performance == TRUE ) {
            VLOG(INFO, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
            VLOG(INFO, "                                                                        | FRAME  | HOST | PREP_S PREP_E    PREP  | PROCE_S PROCE_E  PROCE | ENC_S  ENC_E   ENC   |\n");
            VLOG(INFO, "I     NO     T   RECON   RD_PTR    WR_PTR     BYTES  SRCIDX  USEDSRCIDX | CYCLE  | TICK |  TICK   TICK     CYCLE |  TICK    TICK    CYCLE |  TICK   TICK   CYCLE | TQ IQ\n");
            VLOG(INFO, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        }
        else {
            VLOG(INFO, "---------------------------------------------------------------------------------------------------------------------\n");
            VLOG(INFO, "                                                                        |                CYCLE\n");
            VLOG(INFO, "I     NO     T   RECON   RD_PTR    WR_PTR     BYTES  SRCIDX  USEDSRCIDX | FRAME PREPARING PROCESSING ENCODING | TQ IQ\n");
            VLOG(INFO, "---------------------------------------------------------------------------------------------------------------------\n");
        }
    } else {
        if (performance == TRUE) {
            VLOG(INFO, "%02d %5d %5d %5d    %08x  %08x %8x     %2d        %2d    %8d %6d (%6d,%6d,%8d) (%6d,%6d,%8d) (%6d,%6d,%8d) %d %d\n",
                handle->instIndex, encodedInfo->encPicCnt, encodedInfo->picType, encodedInfo->reconFrameIndex, encodedInfo->rdPtr, encodedInfo->wrPtr,
                encodedInfo->bitstreamSize, (srcEndFlag == 1 ? -1 : srcFrameIdx), encodedInfo->encSrcIdx,
                encodedInfo->frameCycle, encodedInfo->encHostCmdTick,
                encodedInfo->encPrepareStartTick, encodedInfo->encPrepareEndTick, encodedInfo->prepareCycle,
                encodedInfo->encProcessingStartTick, encodedInfo->encProcessingEndTick, encodedInfo->processing,
                encodedInfo->encEncodeStartTick, encodedInfo->encEncodeEndTick, encodedInfo->EncodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
        else {
            VLOG(INFO, "%02d %5d %5d %5d    %08x  %08x %8x     %2d        %2d    %8d %8d %8d %8d      %d %d\n",
                handle->instIndex, encodedInfo->encPicCnt, encodedInfo->picType, encodedInfo->reconFrameIndex, encodedInfo->rdPtr, encodedInfo->wrPtr,
                encodedInfo->bitstreamSize, (srcEndFlag == 1 ? -1 : srcFrameIdx), encodedInfo->encSrcIdx,
                encodedInfo->frameCycle, encodedInfo->prepareCycle, encodedInfo->processing, encodedInfo->EncodedCycle,
                queueStatus.totalQueueCount, queueStatus.instanceQueueCount);
        }
    }
}

static void Coda9DisplayEncodedInformation(
    DecHandle       handle,
    CodStd          codec,
    Uint32          frameNo,
    EncOutputInfo*  encodedInfo
    )
{
    if (encodedInfo == NULL) {
        // Print header
        VLOG(INFO, "I    NO   T   RECON  RD_PTR   WR_PTR \n");
        VLOG(INFO, "-------------------------------------\n");
    }
    else {
        char** picTypeArray = (codec==STD_AVC ? EncPicTypeStringH264 : EncPicTypeStringMPEG4);
        char* strPicType;

        if (encodedInfo->picType > 2) strPicType = "?";
        else                          strPicType = picTypeArray[encodedInfo->picType];
        // Print informations
        VLOG(INFO, "%02d %5d %5s %5d  %08x %08x\n",
            handle->instIndex, frameNo, strPicType,
            encodedInfo->reconFrameIndex, encodedInfo->rdPtr, encodedInfo->wrPtr);
    }
}

/*lint -esym(438, ap) */
void
    DisplayEncodedInformation(
    EncHandle      handle,
    CodStd         codec,
    Uint32         frameNo,
    EncOutputInfo* encodedInfo,
    ...
    )
{
    int srcEndFlag;
    int srcFrameIdx;
    int performance = FALSE;
    va_list         ap;

    switch (codec) {
    case STD_HEVC:
        va_start(ap, encodedInfo);
        srcEndFlag = va_arg(ap, Uint32);
        srcFrameIdx = va_arg(ap, Uint32);
        performance = va_arg(ap, Uint32);
        va_end(ap);
        Wave5DisplayEncodedInformation(handle, codec, frameNo, encodedInfo, srcEndFlag , srcFrameIdx, performance);
        break;
    case STD_AVC:
        if(handle->productId == PRODUCT_ID_521) {
            va_start(ap, encodedInfo);
            srcEndFlag = va_arg(ap, Uint32);
            srcFrameIdx = va_arg(ap, Uint32);
            performance = va_arg(ap, Uint32);
            va_end(ap);
            Wave5DisplayEncodedInformation(handle, codec, frameNo, encodedInfo, srcEndFlag , srcFrameIdx, performance);
        }
        else
            Coda9DisplayEncodedInformation(handle, codec, frameNo, encodedInfo);
        break;
    default:
            Coda9DisplayEncodedInformation(handle, codec, frameNo, encodedInfo);
        break;
    }

    return;
}
/*lint +esym(438, ap) */



void replace_character(char* str,
    char  c,
    char  r)
{
    int i=0;
    int len;
    len = osal_strlen(str);

    for(i=0; i<len; i++)
    {
        if (str[i] == c)
            str[i] = r;
    }
}



void ChangePathStyle(
    char *str
    )
{
}

void ReleaseVideoMemory(
    bm_handle_t     handle,
    Uint32          coreIndex,
    bm_device_mem_t*memoryArr,
    Uint32          count
    )
{
    Uint32      idx;

    vdi_lock(coreIndex);
    for (idx=0; idx<count; idx++) {
        if (memoryArr[idx].size)
        {
            bm_free_device(handle,memoryArr[idx]);
        }
    }
    vdi_unlock(coreIndex);
}

BOOL AllocateDecFrameBuffer(
    DecHandle       decHandle,
    TestDecConfig*  config,
    Uint32          tiledFbCount,
    Uint32          linearFbCount,
    FrameBuffer*    retFbArray,
    bm_device_mem_t*retFbAddrs,
    Uint32*         retStride,
    int             enable_cache
    )
{
    Uint32                  framebufSize;
    Uint32                  totalFbCount, linearFbStartIdx = 0;
    Uint32                  coreIndex;
    Uint32                  idx;
    FrameBufferFormat       format = config->wtlFormat;
    DecInitialInfo          seqInfo;
    FrameBufferAllocInfo    fbAllocInfo;
    RetCode                 ret;
    bm_device_mem_t*        pvb;
    unsigned long long      pvbVaddr;
    size_t                  framebufStride;
    size_t                  framebufHeight;
    Uint32                  productId;
    DRAMConfig*             pDramCfg        = NULL;
    DRAMConfig              dramCfg         = {0};
    bm_handle_t             bm_handle;

    coreIndex = VPU_HANDLE_CORE_INDEX(decHandle);
    productId = VPU_HANDLE_PRODUCT_ID(decHandle);
    VPU_DecGiveCommand(decHandle, DEC_GET_SEQ_INFO, (void*)&seqInfo);
    bm_handle= bmvpu_dec_get_bmlib_handle(coreIndex);
    if (productId == PRODUCT_ID_960) {
        pDramCfg = &dramCfg;
        ret = VPU_DecGiveCommand(decHandle, GET_DRAM_CONFIG, pDramCfg);
    }
    totalFbCount    = tiledFbCount + linearFbCount;
    linearFbStartIdx= tiledFbCount;

    if (PRODUCT_ID_W_SERIES(productId)) {
        format = (seqInfo.lumaBitdepth > 8 || seqInfo.chromaBitdepth > 8) ? FORMAT_420_P10_16BIT_LSB : FORMAT_420;
    }

    if (config->bitFormat == STD_VP9) {
        framebufStride = CalcStride(VPU_ALIGN64(seqInfo.picWidth), seqInfo.picHeight, format, config->cbcrInterleave, config->mapType, TRUE);
        framebufHeight = VPU_ALIGN64(seqInfo.picHeight);
        framebufSize   = VPU_GetFrameBufSize(decHandle->coreIdx, framebufStride, framebufHeight,
            config->mapType, format, config->cbcrInterleave, NULL);
        *retStride     = framebufStride;
    }
    else if (config->bitFormat == STD_AVS2) {
        framebufStride = CalcStride(seqInfo.picWidth, seqInfo.picHeight, format, config->cbcrInterleave, config->mapType, FALSE);
        framebufHeight = VPU_ALIGN8(seqInfo.picHeight);
        framebufSize   = VPU_GetFrameBufSize(decHandle->coreIdx, framebufStride, framebufHeight,
            config->mapType, format, config->cbcrInterleave, pDramCfg);
        *retStride     = framebufStride;
    }
    else {
        *retStride     = VPU_ALIGN64(seqInfo.picWidth); //org is 32, just for compatible 1684 vpp
        framebufStride = CalcStride(seqInfo.picWidth, seqInfo.picHeight, format, config->cbcrInterleave, config->mapType, FALSE);
        framebufHeight = VPU_ALIGN32(seqInfo.picHeight);
        framebufSize   = VPU_GetFrameBufSize(decHandle->coreIdx, framebufStride, framebufHeight,
                                             config->mapType, format, config->cbcrInterleave, pDramCfg);
    }

    osal_memset((void*)&fbAllocInfo, 0x00, sizeof(fbAllocInfo));
    osal_memset((void*)retFbArray,   0x00, sizeof(FrameBuffer)*totalFbCount);
    fbAllocInfo.format          = format;
    fbAllocInfo.cbcrInterleave  = config->cbcrInterleave;
    fbAllocInfo.mapType         = config->mapType;
    fbAllocInfo.stride          = framebufStride;
    fbAllocInfo.height          = framebufHeight;
    fbAllocInfo.size            = framebufSize;
    fbAllocInfo.lumaBitDepth    = seqInfo.lumaBitdepth;
    fbAllocInfo.chromaBitDepth  = seqInfo.chromaBitdepth;
    fbAllocInfo.num             = tiledFbCount;
    fbAllocInfo.endian          = VDI_128BIT_LITTLE_ENDIAN;    // FBC endian is fixed.
    fbAllocInfo.type            = FB_TYPE_CODEC;
    osal_memset((void*)retFbAddrs, 0x00, sizeof(bm_device_mem_t)*totalFbCount);
    APIDPRINT("ALLOC MEM - FBC data\n");
    vdi_lock(coreIndex);
    for (idx=0; idx<tiledFbCount; idx++) {
        pvb = &retFbAddrs[idx];
        pvb->size = framebufSize;
        if(bmvpu_malloc_device_byte_heap(bm_handle,pvb,framebufSize,HEAP_MASK,1)!=BM_SUCCESS)
        {
            vdi_unlock(coreIndex);
            VLOG(ERR, "coreIdx:%d,%s:%d fail to allocate frame buffer\n",coreIndex, __FUNCTION__, __LINE__);
            ReleaseVideoMemory(bm_handle,coreIndex, retFbAddrs, totalFbCount);

            return FALSE;
        }
        bm_vdi_mmap(bm_handle,pvb,(unsigned long long *)&pvbVaddr);
#ifndef BM_PCIE_MODE
        retFbArray[idx].bufYVaddr=(Uint64)pvbVaddr;
#endif
        decHandle->CodecInfo->decInfo.vpu_frame_buffer_vaddr[idx]=pvbVaddr;
        decHandle->CodecInfo->decInfo.vpu_frame_buffer_vaddr_size[idx]=framebufSize;

        retFbArray[idx].bufY  = pvb->u.device.device_addr;
        retFbArray[idx].bufCb = (PhysicalAddress)-1;
        retFbArray[idx].bufCr = (PhysicalAddress)-1;
        retFbArray[idx].updateFbInfo = TRUE;
        retFbArray[idx].size  = framebufSize;
        retFbArray[idx].width = seqInfo.picWidth;
    }
    vdi_unlock(coreIndex);

    if (tiledFbCount != 0) {
        if ((ret=VPU_DecAllocateFrameBuffer(decHandle, fbAllocInfo, retFbArray)) != RETCODE_SUCCESS) {
            VLOG(ERR, "%s:%d failed to VPU_DecAllocateFrameBuffer(), ret(%d)\n",
                __FUNCTION__, __LINE__, ret);
            ReleaseVideoMemory(bm_handle,coreIndex,retFbAddrs,totalFbCount);
            return FALSE;
        }
    }

    if (config->enableWTL == TRUE || linearFbCount != 0) {
        size_t  linearStride;
        size_t  picWidth;
        size_t  picHeight;
        size_t  fbHeight;
        Uint32   mapType = LINEAR_FRAME_MAP;
        FrameBufferFormat outFormat = config->wtlFormat;
        picWidth  = seqInfo.picWidth;
        picHeight = seqInfo.picHeight;
        fbHeight  = picHeight;

        if (config->bitFormat == STD_VP9) {
            fbHeight = VPU_ALIGN64(picHeight);
        }
        else if (config->bitFormat == STD_AVS2) {
            fbHeight = VPU_ALIGN8(picHeight);
        }
        else if (productId == PRODUCT_ID_960 || productId == PRODUCT_ID_980)
        {
            fbHeight = VPU_ALIGN32(picHeight);
        }
        if (config->scaleDownWidth > 0 || config->scaleDownHeight > 0) {
            ScalerInfo sclInfo;
            VPU_DecGiveCommand(decHandle, DEC_GET_SCALER_INFO, (void*)&sclInfo);
            if (sclInfo.enScaler == TRUE) {
                picWidth  = sclInfo.scaleWidth;
                picHeight = sclInfo.scaleHeight;
                if (config->bitFormat == STD_VP9) {
                    fbHeight  = VPU_ALIGN64(picHeight);
                }
                else {
                    fbHeight  = VPU_CEIL(picHeight, 2);
                }
            }
        }
        if (config->bitFormat == STD_VP9) {
            linearStride = CalcStride(VPU_ALIGN64(picWidth), picHeight, outFormat, config->cbcrInterleave, (TiledMapType)mapType, TRUE);
        }
        else {
            linearStride = CalcStride(picWidth, picHeight, outFormat, config->cbcrInterleave, (TiledMapType)mapType, FALSE);
        }
        framebufSize = VPU_GetFrameBufSize(coreIndex, linearStride, fbHeight, (TiledMapType)mapType, outFormat, config->cbcrInterleave, pDramCfg);

        vdi_lock(coreIndex);
        for (idx=linearFbStartIdx; idx<totalFbCount; idx++) {
            pvb = &retFbAddrs[idx];
            pvb->size = framebufSize;
            if(bmvpu_malloc_device_byte_heap(bm_handle,pvb,pvb->size,HEAP_MASK,1)!=BM_SUCCESS)
            {
                vdi_unlock(coreIndex);
                VLOG(ERR, "%s:%d fail to allocate frame buffer\n", __FUNCTION__, __LINE__);
                ReleaseVideoMemory(bm_handle,coreIndex,retFbAddrs, totalFbCount-idx);
                return FALSE;
            }
            bm_vdi_mmap(bm_handle,pvb,(unsigned long long *)&pvbVaddr);
#ifndef BM_PCIE_MODE
            retFbArray[idx].bufYVaddr=(Uint64)pvbVaddr;
#endif
            decHandle->CodecInfo->decInfo.vpu_frame_buffer_vaddr[idx]=pvbVaddr;
            decHandle->CodecInfo->decInfo.vpu_frame_buffer_vaddr_size[idx]=framebufSize;

            retFbArray[idx].bufY  = pvb->u.device.device_addr;
            retFbArray[idx].bufCb = (PhysicalAddress)-1;
            retFbArray[idx].bufCr = (PhysicalAddress)-1;
            retFbArray[idx].updateFbInfo = TRUE;
            retFbArray[idx].size  = framebufSize;
            retFbArray[idx].width = picWidth;
        }
        vdi_unlock(coreIndex);

        fbAllocInfo.nv21    = config->nv21;
        fbAllocInfo.format  = outFormat;
        fbAllocInfo.num     = linearFbCount;
        fbAllocInfo.mapType = (TiledMapType)mapType;
        fbAllocInfo.stride  = linearStride;
        fbAllocInfo.height  = fbHeight;
        ret = VPU_DecAllocateFrameBuffer(decHandle, fbAllocInfo, &retFbArray[linearFbStartIdx]);
        if (ret != RETCODE_SUCCESS) {
            VLOG(ERR, "%s:%d failed to VPU_DecAllocateFrameBuffer() ret:%d\n",
                __FUNCTION__, __LINE__, ret);
            ReleaseVideoMemory(bm_handle,coreIndex, retFbAddrs, totalFbCount);
            return FALSE;
        }
    }

    return TRUE;
}

#if defined(_WIN32) || defined(__MSDOS__)
#define DOS_FILESYSTEM
#define IS_DIR_SEPARATOR(__c) ((__c == '/') || (__c == '\\'))
#else
/* UNIX style */
#define IS_DIR_SEPARATOR(__c) (__c == '/')
#endif

char* GetDirname(
    const char* path
    )
{
    int length;
    int i;
    char* upper_dir;

    if (path == NULL) return NULL;

    length = osal_strlen(path);
    for (i=length-1; i>=0; i--) {
        if (IS_DIR_SEPARATOR(path[i])) break;
    }

    if (i<0) {
        upper_dir = osal_strdup(".");
    } else {
        upper_dir = osal_strdup(path);
        upper_dir[i] = 0;
    }

    return upper_dir;
}

char* GetBasename(
    const char* pathname
    )
{
    const char* base = NULL;
    const char* p    = pathname;

    if (p == NULL) {
        return NULL;
    }

#if defined(DOS_FILESYSTEM)
    if (isalpha((int)p[0]) && p[1] == ':') {
        p += 2;
    }
#endif

    for (base=p; *p; p++) {//lint !e443
        if (IS_DIR_SEPARATOR(*p)) {
            base = p+1;
        }
    }

    return (char*)base;
}

#if !defined(PLATFORM_NON_OS)

char* GetFileExtension(
    const char* filename
    )
{
    Int32      len;
    Int32      i;

    len = osal_strlen(filename);
    for (i=len-1; i>=0; i--) {
        if (filename[i] == '.') {
            return (char*)&filename[i+1];
        }
    }

    return NULL;
}
#endif

void byte_swap(unsigned char* data, int len)
{
    Uint8 temp;
    Int32 i;

    for (i=0; i<len; i+=2) {
        temp      = data[i];
        data[i]   = data[i+1];
        data[i+1] = temp;
    }
}

void word_swap(unsigned char* data, int len)
{
    Uint16  temp;
    Uint16* ptr = (Uint16*)data;
    Int32   i, size = len/(int)sizeof(Uint16);

    for (i=0; i<size; i+=2) {
        temp      = ptr[i];
        ptr[i]   = ptr[i+1];
        ptr[i+1] = temp;
    }
}

void dword_swap(unsigned char* data, int len)
{
    Uint32  temp;
    Uint32* ptr = (Uint32*)data;
    Int32   i, size = len/(int)sizeof(Uint32);

    for (i=0; i<size; i+=2) {
        temp      = ptr[i];
        ptr[i]   = ptr[i+1];
        ptr[i+1] = temp;
    }
}

void lword_swap(unsigned char* data, int len)
{
    Uint64  temp;
    Uint64* ptr = (Uint64*)data;
    Int32   i, size = len/(int)sizeof(Uint64);

    for (i=0; i<size; i+=2) {
        temp      = ptr[i];
        ptr[i]   = ptr[i+1];
        ptr[i+1] = temp;
    }
}

BOOL IsEndOfFile(osal_file_t * fp)
{
    BOOL  result = FALSE;
    Int32 idx  = 0;
    char  cTemp;

    // Check current fp pos
    if (osal_feof(fp) != 0) {
        result = TRUE;
    }

    // Check next fp pos
    // Ignore newline character
    do {
        cTemp = osal_fgetc(fp);
        idx++;

        if (osal_feof(fp) != 0) {
            result = TRUE;
            break;
        }
    } while (cTemp == '\n' || cTemp == '\r');

    // Revert fp pos
    idx *= (-1);
    osal_fseek(fp, idx, SEEK_CUR);

    return result;
}

BOOL CalcYuvSize(
    Int32   format,
    Int32   picWidth,
    Int32   picHeight,
    Int32   cbcrInterleave,
    size_t  *lumaSize,
    size_t  *chromaSize,
    size_t  *frameSize,
    Int32   *bitDepth,
    Int32   *packedFormat,
    Int32   *yuv3p4b)
{
    Int32   temp_picWidth;
    Int32   chromaWidth;

    if ( bitDepth != 0)
        *bitDepth = 0;
    if ( packedFormat != 0)
        *packedFormat = 0;
    if ( yuv3p4b != 0)
        *yuv3p4b = 0;

    if (!lumaSize || !chromaSize || !frameSize )
        return FALSE;

    switch (format)
    {
    case FORMAT_420:
        *lumaSize = picWidth * picHeight;
        *chromaSize = picWidth * picHeight / 2;
        *frameSize = picWidth * picHeight * 3 /2;
        break;
    case FORMAT_YUYV:
    case FORMAT_YVYU:
    case FORMAT_UYVY:
    case FORMAT_VYUY:
        if ( packedFormat != 0)
            *packedFormat = 1;
        *lumaSize = picWidth * picHeight;
        *chromaSize = picWidth * picHeight;
        *frameSize = *lumaSize + *chromaSize;
        break;
    case FORMAT_224:
        *lumaSize = picWidth * picHeight;
        *chromaSize = picWidth * picHeight;
        *frameSize = picWidth * picHeight * 4 / 2;
        break;
    case FORMAT_422:
        *lumaSize = picWidth * picHeight;
        *chromaSize = picWidth * picHeight;
        *frameSize = picWidth * picHeight * 4 / 2;
        break;
    case FORMAT_444:
        *lumaSize  = picWidth * picHeight;
        *chromaSize = picWidth * picHeight * 2;
        *frameSize = picWidth * picHeight * 3;
        break;
    case FORMAT_400:
        *lumaSize  = picWidth * picHeight;
        *chromaSize = 0;
        *frameSize = picWidth * picHeight;
        break;
    case FORMAT_422_P10_16BIT_MSB:
    case FORMAT_422_P10_16BIT_LSB:
        if ( bitDepth != NULL) {
            *bitDepth = 10;
        }
        *lumaSize = picWidth * picHeight * 2;
        *chromaSize = *lumaSize;
        *frameSize = *lumaSize + *chromaSize;
        break;
    case FORMAT_420_P10_16BIT_MSB:
    case FORMAT_420_P10_16BIT_LSB:
        if ( bitDepth != 0)
            *bitDepth = 10;
        *lumaSize = picWidth * picHeight * 2;
        *chromaSize = picWidth * picHeight;
        *frameSize = *lumaSize + *chromaSize;
        break;
    case FORMAT_YUYV_P10_16BIT_MSB:   // 4:2:2 10bit packed
    case FORMAT_YUYV_P10_16BIT_LSB:
    case FORMAT_YVYU_P10_16BIT_MSB:
    case FORMAT_YVYU_P10_16BIT_LSB:
    case FORMAT_UYVY_P10_16BIT_MSB:
    case FORMAT_UYVY_P10_16BIT_LSB:
    case FORMAT_VYUY_P10_16BIT_MSB:
    case FORMAT_VYUY_P10_16BIT_LSB:
        if ( bitDepth != 0)
            *bitDepth = 10;
        if ( packedFormat != 0)
            *packedFormat = 1;
        *lumaSize = picWidth * picHeight * 2;
        *chromaSize = picWidth * picHeight * 2;
        *frameSize = *lumaSize + *chromaSize;
        break;
    case FORMAT_420_P10_32BIT_MSB:
    case FORMAT_420_P10_32BIT_LSB:
        if ( bitDepth != 0)
            *bitDepth = 10;
        if ( yuv3p4b != 0)
            *yuv3p4b = 1;
        temp_picWidth = VPU_ALIGN32(picWidth);
        chromaWidth = ((VPU_ALIGN16(temp_picWidth/2*(1<<cbcrInterleave))+2)/3*4);
        if ( cbcrInterleave == 1)
        {
            *lumaSize = (temp_picWidth+2)/3*4 * picHeight;
            *chromaSize = chromaWidth * picHeight/2;
        } else {
            *lumaSize = (temp_picWidth+2)/3*4 * picHeight;
            *chromaSize = chromaWidth * picHeight/2*2;
        }
        *frameSize = *lumaSize + *chromaSize;
        break;
    case FORMAT_YUYV_P10_32BIT_MSB:
    case FORMAT_YUYV_P10_32BIT_LSB:
    case FORMAT_YVYU_P10_32BIT_MSB:
    case FORMAT_YVYU_P10_32BIT_LSB:
    case FORMAT_UYVY_P10_32BIT_MSB:
    case FORMAT_UYVY_P10_32BIT_LSB:
    case FORMAT_VYUY_P10_32BIT_MSB:
    case FORMAT_VYUY_P10_32BIT_LSB:
        if ( bitDepth != 0)
            *bitDepth = 10;
        if ( packedFormat != 0)
            *packedFormat = 1;
        if ( yuv3p4b != 0)
            *yuv3p4b = 1;
        *frameSize = ((picWidth*2)+2)/3*4 * picHeight;
        *lumaSize = *frameSize/2;
        *chromaSize = *frameSize/2;
        break;
    default:
        *frameSize = picWidth * picHeight * 3 / 2;
        VLOG(ERR, "%s:%d Not supported format(%d)\n", __FILE__, __LINE__, format);
        return FALSE;
    }
    return TRUE;
}

FrameBufferFormat GetPackedFormat (
    int srcBitDepth,
    int packedType,
    int p10bits,
    int msb)
{
    FrameBufferFormat format = FORMAT_YUYV;

    // default pixel format = P10_16BIT_LSB (p10bits = 16, msb = 0)
    if (srcBitDepth == 8) {

        switch(packedType) {
        case PACKED_YUYV:
            format = FORMAT_YUYV;
            break;
        case PACKED_YVYU:
            format = FORMAT_YVYU;
            break;
        case PACKED_UYVY:
            format = FORMAT_UYVY;
            break;
        case PACKED_VYUY:
            format = FORMAT_VYUY;
            break;
        default:
            format = FORMAT_ERR;
        }
    }
    else if (srcBitDepth == 10) {
        switch(packedType) {
        case PACKED_YUYV:
            if (p10bits == 16) {
                format = (msb == 0) ? FORMAT_YUYV_P10_16BIT_LSB : FORMAT_YUYV_P10_16BIT_MSB;
            }
            else if (p10bits == 32) {
                format = (msb == 0) ? FORMAT_YUYV_P10_32BIT_LSB : FORMAT_YUYV_P10_32BIT_MSB;
            }
            else {
                format = FORMAT_ERR;
            }
            break;
        case PACKED_YVYU:
            if (p10bits == 16) {
                format = (msb == 0) ? FORMAT_YVYU_P10_16BIT_LSB : FORMAT_YVYU_P10_16BIT_MSB;
            }
            else if (p10bits == 32) {
                format = (msb == 0) ? FORMAT_YVYU_P10_32BIT_LSB : FORMAT_YVYU_P10_32BIT_MSB;
            }
            else {
                format = FORMAT_ERR;
            }
            break;
        case PACKED_UYVY:
            if (p10bits == 16) {
                format = (msb == 0) ? FORMAT_UYVY_P10_16BIT_LSB : FORMAT_UYVY_P10_16BIT_MSB;
            }
            else if (p10bits == 32) {
                format = (msb == 0) ? FORMAT_UYVY_P10_32BIT_LSB : FORMAT_UYVY_P10_32BIT_MSB;
            }
            else {
                format = FORMAT_ERR;
            }
            break;
        case PACKED_VYUY:
            if (p10bits == 16) {
                format = (msb == 0) ? FORMAT_VYUY_P10_16BIT_LSB : FORMAT_VYUY_P10_16BIT_MSB;
            }
            else if (p10bits == 32) {
                format = (msb == 0) ? FORMAT_VYUY_P10_32BIT_LSB : FORMAT_VYUY_P10_32BIT_MSB;
            }
            else {
                format = FORMAT_ERR;
            }
            break;
        default:
            format = FORMAT_ERR;
        }
    }
    else {
        format = FORMAT_ERR;
    }

    return format;
}


BOOL SetupEncoderOpenParam(
    EncOpenParam*   param,
    TestEncConfig*  config
    )
{
    param->bitstreamFormat = config->stdMode;
    if (osal_strlen(config->cfgFileName) != 0 && config->cfgFileName[0] != 0) {
        VLOG(INFO, "get param from cfg file.\n");
        if (GetEncOpenParam(param, config, NULL) == FALSE) {
            VLOG(ERR, "[ERROR] Failed to parse CFG file(GetEncOpenParam)\n");
            return FALSE;
        }
    }
    else {
        if (GetEncOpenParamDefault(param, config) == FALSE) {
            VLOG(ERR, "[ERROR] Failed to parse CFG file(GetEncOpenParamDefault)\n");
            return FALSE;
        }
    }
    param->streamBufCount = ENC_STREAM_BUF_COUNT;
    param->streamBufSize  = ENC_STREAM_BUF_SIZE;

    if ( param->streamBufCount < COMMAND_QUEUE_DEPTH )
        param->streamBufCount = COMMAND_QUEUE_DEPTH; //for encoder->numSinkPortQueue

    if (osal_strlen(config->cfgFileName) != 0) {
        if (param->srcBitDepth == 8) {
            config->srcFormat = FORMAT_420;
        }
        else if (param->srcBitDepth == 10) {
            config->srcFormat = FORMAT_420_P10_16BIT_MSB;
            if (config->srcFormat3p4b == 1) {
                config->srcFormat = FORMAT_420_P10_32BIT_MSB;
            }
            if (config->srcFormat3p4b == 2) {
                config->srcFormat = FORMAT_420_P10_32BIT_LSB;
            }
        }
    }

    if (config->optYuvPath[0] != 0)
        osal_strcpy(config->yuvFileName, config->optYuvPath);

    if (config->packedFormat >= 1)
        config->srcFormat = FORMAT_422;

    if (config->srcFormat == FORMAT_422 && config->packedFormat >= PACKED_YUYV) {
        int p10bits = config->srcFormat3p4b == 0 ? 16 : 32;
        FrameBufferFormat packedFormat = GetPackedFormat(param->srcBitDepth, config->packedFormat, p10bits, 1);

        if (packedFormat == -1) {
            VLOG(ERR, "[ERROR] Failed to GetPackedFormat\n");
            return FALSE;
        }
        param->srcFormat      = packedFormat;
        param->nv21           = 0;
        param->cbcrInterleave = 0;
    }
    else {
        param->srcFormat      = config->srcFormat;
        param->nv21           = config->nv21;
    }
    param->packedFormat   = config->packedFormat;
    param->cbcrInterleave = config->cbcrInterleave;
    param->frameEndian    = config->frame_endian;
    param->streamEndian   = config->stream_endian;
    param->sourceEndian   = config->source_endian;
    param->lineBufIntEn   = config->lineBufIntEn;
    param->coreIdx        = config->coreIdx;
    param->cbcrOrder      = CBCR_ORDER_NORMAL;
    param->lowLatencyMode = (config->lowLatencyMode&0x3);		// 2bits lowlatency mode setting. bit[1]: low latency interrupt enable, bit[0]: fast bitstream-packing enable.
    param->EncStdParam.waveParam.useLongTerm = (config->useAsLongtermPeriod > 0 && config->refLongtermPeriod > 0) ? 1 : 0;
    param->cframe50Enable         = config->cframe50Enable;
    param->cframe50LosslessEnable = config->cframe50Lossless;
    param->cframe50Tx16Y          = config->cframe50Tx16Y;
    param->cframe50Tx16C          = config->cframe50Tx16C;
    param->cframe50_422           = config->cframe50_422;
    param->subFrameSyncEnable     = config->subFrameSyncEn;
    param->subFrameSyncMode       = config->subFrameSyncMode;// default mode = register based for Ref.SW
    return TRUE;
}

void GenRegionToMap(
    VpuRect *region,        /**< The size of the ROI region for H.265 (start X/Y in CTU, end X/Y int CTU)  */
    int *roiQp,
    int num,
    Uint32 mapWidth,
    Uint32 mapHeight,
    Uint8 *roiCtuMap)
{
    Int32 roi_id, blk_addr;
    Uint32 roi_map_size      = mapWidth * mapHeight;

    //init roi map
    for (blk_addr=0; blk_addr<(Int32)roi_map_size; blk_addr++)
        roiCtuMap[blk_addr] = 0;

    //set roi map. roi_entry[i] has higher priority than roi_entry[i+1]
    for (roi_id=(Int32)num-1; roi_id>=0; roi_id--)
    {
        Uint32 x, y;
        VpuRect *roi = region + roi_id;

        for (y=roi->top; y<=roi->bottom; y++)
        {
            for (x=roi->left; x<=roi->right; x++)
            {
                roiCtuMap[y*mapWidth + x] = *(roiQp + roi_id);
            }
        }
    }
}


void GenRegionToQpMap(
    VpuRect *region,        /**< The size of the ROI region for H.265 (start X/Y in CTU, end X/Y int CTU)  */
    int *roiQp,
    int num,
    int initQp,
    Uint32 mapWidth,
    Uint32 mapHeight,
    Uint8 *roiCtuMap)
{
    Int32 roi_id, blk_addr;
    Uint32 roi_map_size      = mapWidth * mapHeight;

    //init roi map
    for (blk_addr=0; blk_addr<(Int32)roi_map_size; blk_addr++)
        roiCtuMap[blk_addr] = initQp;

    //set roi map. roi_entry[i] has higher priority than roi_entry[i+1]
    for (roi_id=(Int32)num-1; roi_id>=0; roi_id--)
    {
        Uint32 x, y;
        VpuRect *roi = region + roi_id;

        for (y=roi->top; y<=roi->bottom; y++)
        {
            for (x=roi->left; x<=roi->right; x++)
            {
                roiCtuMap[y*mapWidth + x] = *(roiQp + roi_id);
            }
        }
    }
}




int openRoiMapFile(TestEncConfig *encConfig)
{
    if (encConfig->roi_enable) {
        if (encConfig->roi_file_name) {
            ChangePathStyle(encConfig->roi_file_name);
            if ((encConfig->roi_file = osal_fopen(encConfig->roi_file_name, "r")) == NULL) {
                VLOG(ERR, "fail to open ROI file, %s\n", encConfig->roi_file_name);
                return FALSE;
            }
        }
    }
    return TRUE;
}

int allocateRoiMapBuf(int coreIdx, TestEncConfig encConfig, vpu_buffer_t *vbRoi, int srcFbNum, int ctuNum)
{
    int i;
    if (encConfig.roi_enable) {
        //number of roi buffer should be the same as source buffer num.
        vdi_lock(coreIdx);
        for (i = 0; i < srcFbNum ; i++) {
            vbRoi[i].size = ctuNum;
            if (vdi_allocate_dma_memory(coreIdx, &vbRoi[i]) < 0) {
                vdi_unlock(coreIdx);
                VLOG(ERR, "fail to allocate ROI buffer\n" );
                return FALSE;
            }
        }
        vdi_unlock(coreIdx);
    }
    return TRUE;
}

// Define tokens for parsing scaling list file
const char* MatrixType[SCALING_LIST_SIZE_NUM][SL_NUM_MATRIX] =
{
    {"INTRA4X4_LUMA", "INTRA4X4_CHROMAU", "INTRA4X4_CHROMAV", "INTER4X4_LUMA", "INTER4X4_CHROMAU", "INTER4X4_CHROMAV"},
    {"INTRA8X8_LUMA", "INTRA8X8_CHROMAU", "INTRA8X8_CHROMAV", "INTER8X8_LUMA", "INTER8X8_CHROMAU", "INTER8X8_CHROMAV"},
    {"INTRA16X16_LUMA", "INTRA16X16_CHROMAU", "INTRA16X16_CHROMAV", "INTER16X16_LUMA", "INTER16X16_CHROMAU", "INTER16X16_CHROMAV"},
    {"INTRA32X32_LUMA", "INTRA32X32_CHROMAU_FROM16x16_CHROMAU", "INTRA32X32_CHROMAV_FROM16x16_CHROMAV","INTER32X32_LUMA", "INTER32X32_CHROMAU_FROM16x16_CHROMAU", "INTER32X32_CHROMAV_FROM16x16_CHROMAV"}
};

const char* MatrixType_DC[SCALING_LIST_SIZE_NUM - 2][SL_NUM_MATRIX] =
{
    {"INTRA16X16_LUMA_DC", "INTRA16X16_CHROMAU_DC", "INTRA16X16_CHROMAV_DC", "INTER16X16_LUMA_DC", "INTER16X16_CHROMAU_DC", "INTER16X16_CHROMAV_DC"},
    {"INTRA32X32_LUMA_DC", "INTRA32X32_CHROMAU_DC_FROM16x16_CHROMAU", "INTRA32X32_CHROMAV_DC_FROM16x16_CHROMAV", "INTER32X32_LUMA_DC","INTER32X32_CHROMAU_DC_FROM16x16_CHROMAU","INTER32X32_CHROMAV_DC_FROM16x16_CHROMAV"},
};

static Uint8* get_sl_addr(UserScalingList* sl, Uint32 size_id, Uint32 mat_id)
{
    Uint8* addr = NULL;

    switch(size_id)
    {
    case SCALING_LIST_4x4:
        addr = sl->s4[mat_id];
        break;
    case SCALING_LIST_8x8:
        addr = sl->s8[mat_id];
        break;
    case SCALING_LIST_16x16:
        addr = sl->s16[mat_id];
        break;
    case SCALING_LIST_32x32:
        addr = sl->s32[mat_id];
        break;
    }
    return addr;
}

int parse_user_scaling_list(UserScalingList* sl, FILE* fp_sl, CodStd  stdMode)
{
#define LINE_SIZE (1024)
    const Uint32 scaling_list_size[SCALING_LIST_SIZE_NUM] = {16, 64, 64, 64};
    char line[LINE_SIZE];
    Uint32 i;
    Uint32 size_id, mat_id, data, num_coef = 0;
    Uint8* src = NULL;
    Uint8* ref = NULL;
    char* ret;
    const char* type_str;

    if ( fp_sl == NULL)
        return 0;

    for(size_id = 0; size_id < SCALING_LIST_SIZE_NUM; size_id++)  {// for 4, 8, 16, 32
        num_coef = scaling_list_size[size_id];//lint !e644

        for(mat_id = 0; mat_id < SL_NUM_MATRIX; mat_id++) { // for intra_y, intra_cb, intra_cr, inter_y, inter_cb, inter_cr
            src = get_sl_addr(sl, size_id, mat_id);

            if(size_id == SCALING_LIST_32x32 && (mat_id % 3)) { // derive scaling list of chroma32x32 from that of chrom16x16
                ref = get_sl_addr(sl, size_id - 1, mat_id);

                for(i = 0; i < num_coef; i++)
                    src[i] = ref[i];
            }
            else {
                osal_fseek(fp_sl,0,0);
                type_str = MatrixType[size_id][mat_id];

                do {
                    ret = osal_fgets(line, LINE_SIZE, fp_sl);
                    if((ret == NULL) || (osal_strstr(line, type_str) == NULL && osal_feof(fp_sl))) {
                        VLOG(ERR,"Error: can't read a scaling list matrix(%s)\n", type_str);
                        return 0;
                    }
                } while (osal_strstr(line, type_str) == NULL);

                // get all coeff
                for(i = 0; i < num_coef; i++) {
                    if(osal_fscanf(fp_sl, "%d,", &data) != 1) {
                        VLOG(ERR,"Error: can't read a scaling list matrix(%s)\n", type_str);
                        return 0;
                    }
                    if (stdMode == STD_AVC && data < 4) {
                        VLOG(ERR,"Error: invald value in scaling list matrix(%s %d)\n", type_str, data);
                        return 0;
                    }
                    src[i] = data;
                }

                // get DC coeff for 16, 32
                if(size_id > SCALING_LIST_8x8) {
                    osal_fseek(fp_sl,0,0);
                    type_str = MatrixType_DC[size_id - 2][mat_id];

                    do {
                        ret = osal_fgets(line, LINE_SIZE, fp_sl);
                        if((ret == NULL) || (osal_strstr(line, type_str) == NULL && osal_feof(fp_sl))) {
                            VLOG(ERR,"Error: can't read a scaling list matrix(%s)\n", type_str);
                            return 0;
                        }
                    } while (osal_strstr(line, type_str) == NULL);

                    if(osal_fscanf(fp_sl, "%d,", &data) != 1) {
                        VLOG(ERR,"Error: can't read a scaling list matrix(%s)\n", type_str);
                        return 0;
                    }
                    if (stdMode == STD_AVC && data < 4) {
                        VLOG(ERR,"Error: invald value in scaling list matrix(%s %d)\n", type_str, data);
                        return 0;
                    }
                    if(size_id == SCALING_LIST_16x16)
                        sl->s16dc[mat_id] = data;
                    else // SCALING_LIST_32x32
                        sl->s32dc[mat_id/3] = data;
                }
            }
        } // for matrix id
    } // for size id
    return 1;
}

int parse_custom_lambda(Uint32 buf[NUM_CUSTOM_LAMBDA], FILE* fp)
{
    int i, j = 0;
    char lineStr[256] = {0, };
    for(i = 0; i < 52; i++)
    {
        if( NULL == osal_fgets(lineStr, 256, fp) )
        {
            VLOG(ERR,"Error: can't read custom_lambda\n");
            return 0;
        }
        else
        {
            osal_sscanf(lineStr, "%d\n", &buf[j++]);
        }
    }
    for(i = 0; i < 52; i++)
    {
        if( NULL == osal_fgets(lineStr, 256, fp) )
        {
            VLOG(ERR,"Error: can't read custom_lambda\n");
            return 0;
        }
        else
            osal_sscanf(lineStr, "%d\n", &buf[j++]);
    }

    return 1;
}


#if defined(PLATFORM_NON_OS) || defined (PLATFORM_LINUX)
struct option* ConvertOptions(
    struct OptionExt*   cnmOpt,
    Uint32              nItems
    )
{
    struct option*  opt;
    Uint32          i;

    opt = (struct option*)osal_malloc(sizeof(struct option) * nItems);
    if (opt == NULL) {
        return NULL;
    }

    for (i=0; i<nItems; i++) {
        osal_memcpy((void*)&opt[i], (void*)&cnmOpt[i], sizeof(struct option));
    }

    return opt;
}
#endif

#ifdef PLATFORM_LINUX
int mkdir_recursive(
    char *path,
    mode_t omode
    )
{
    struct stat sb;
    mode_t numask, oumask;
    int first, last, retval;
    char *p;

    p = path;
    oumask = 0;
    retval = 0;
    if (p[0] == '/')        /* Skip leading '/'. */
        ++p;
    for (first = 1, last = 0; !last ; ++p) {//lint !e441 !e443
        if (p[0] == '\0')
            last = 1;
        else if (p[0] != '/')
            continue;
        *p = '\0';
        if (p[1] == '\0')
            last = 1;
        if (first) {
            /*
            * POSIX 1003.2:
            * For each dir operand that does not name an existing
            * directory, effects equivalent to those cased by the
            * following command shall occcur:
            *
            * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
            *    mkdir [-m mode] dir
            *
            * We change the user's umask and then restore it,
            * instead of doing chmod's.
            */
            oumask = umask(0);
            numask = oumask & ~(S_IWUSR | S_IXUSR);
            (void)umask(numask);
            first = 0;
        }
        if (last)
            (void)umask(oumask);
        if (mkdir(path, last ? omode : S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
            if (errno == EEXIST || errno == EISDIR) {
                if (stat(path, &sb) < 0) {
                    VLOG(INFO, "%s", path);
                    retval = 1;
                    break;
                } else if (!S_ISDIR(sb.st_mode)) {
                    if (last)
                        errno = EEXIST;
                    else
                        errno = ENOTDIR;
                    VLOG(INFO, "%s", path);
                    retval = 1;
                    break;
                }
            } else {
                VLOG(INFO, "%s", path);
                retval = 1;
                break;
            }
        } else {
            VLOG(INFO, "%s", path);
            chmod(path, omode);
        }
        if (!last)
            *p = '/';
    }
    if (!first && !last)
        (void)umask(oumask);
    return (retval);
}
#endif

int file_exist(
    char* path
    )
{
#if defined(PLATFORM_NON_OS) || defined(PLATFORM_QNX)
    /* need to implement */
    return TRUE;
#else
#ifdef _MSC_VER
    DWORD   attributes;
    char    temp[4096];
    LPCTSTR lp_path = (LPCTSTR)temp;

    if (path == NULL) {
        return FALSE;
    }

    strcpy(temp, path);
    replace_character(temp, '/', '\\');
    attributes = GetFileAttributes(lp_path);
    return (attributes != (DWORD)-1);
#else
    return !access(path, F_OK);
#endif
#endif
}

BOOL MkDir(
    char* path
    )
{
#if defined(PLATFORM_NON_OS) || defined(PLATFORM_QNX)
    /* need to implement */
    return FALSE;
#else
#ifdef _MSC_VER
    char cmd[4096];
#endif
    if (file_exist(path))
        return TRUE;

#ifdef _MSC_VER
    sprintf(cmd, "mkdir %s", path);
    replace_character(cmd, '/', '\\');
    if (system(cmd)) {
        return FALSE;
    }
    return TRUE;
#else
    return mkdir_recursive(path, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
#endif
}

void GetUserData(Int32 coreIdx, Uint8* pBase, vpu_buffer_t vbUserData, DecOutputInfo outputInfo)
{
    int idx;
    user_data_entry_t* pEntry = (user_data_entry_t*)pBase;

    VpuReadMem(coreIdx, vbUserData.phys_addr, pBase, vbUserData.size, VPU_USER_DATA_ENDIAN);
    VLOG(INFO, "===== USER DATA(SEI OR VUI) : NUM(%d) =====\n", outputInfo.decOutputExtData.userDataNum);

    for (idx=0; idx<32; idx++) {
        if (outputInfo.decOutputExtData.userDataHeader&(1<<idx)) {
            VLOG(INFO, "\nUSERDATA INDEX: %02d offset: %8d size: %d\n", idx, pEntry[idx].offset, pEntry[idx].size);

            if (idx == H265_USERDATA_FLAG_MASTERING_COLOR_VOL) {
                h265_mastering_display_colour_volume_t* mastering;
                int i;

                mastering = (h265_mastering_display_colour_volume_t*)(pBase + pEntry[H265_USERDATA_FLAG_MASTERING_COLOR_VOL].offset);
                VLOG(INFO, " MASTERING DISPLAY COLOR VOLUME\n");
                for (i=0; i<3; i++) {
                    VLOG(INFO, " PRIMARIES_X%d : %10d PRIMARIES_Y%d : %10d\n", i, mastering->display_primaries_x[i], i, mastering->display_primaries_y[i]);
                }
                VLOG(INFO, " WHITE_POINT_X: %10d WHITE_POINT_Y: %10d\n", mastering->white_point_x, mastering->white_point_y);
                VLOG(INFO, " MIN_LUMINANCE: %10d MAX_LUMINANCE: %10d\n", mastering->min_display_mastering_luminance, mastering->max_display_mastering_luminance);
            }

            if(idx == H265_USERDATA_FLAG_VUI)
            {
                h265_vui_param_t* vui;

                vui = (h265_vui_param_t*)(pBase + pEntry[H265_USERDATA_FLAG_VUI].offset);
                VLOG(INFO, " VUI SAR(%d, %d)\n", vui->sar_width, vui->sar_height);
                VLOG(INFO, "     VIDEO FORMAT(%d)\n", vui->video_format);
                VLOG(INFO, "     COLOUR PRIMARIES(%d)\n", vui->colour_primaries);
                VLOG(INFO, "log2_max_mv_length_horizontal: %d\n", vui->log2_max_mv_length_horizontal);
                VLOG(INFO, "log2_max_mv_length_vertical  : %d\n", vui->log2_max_mv_length_vertical);
                VLOG(INFO, "video_full_range_flag  : %d\n", vui->video_full_range_flag);
                VLOG(INFO, "transfer_characteristics  : %d\n", vui->transfer_characteristics);
                VLOG(INFO, "matrix_coeffs  : %d\n", vui->matrix_coefficients);
            }
            if (idx == H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT) {
                h265_chroma_resampling_filter_hint_t* c_resampleing_filter_hint;
                Uint32 i,j;

                c_resampleing_filter_hint = (h265_chroma_resampling_filter_hint_t*)(pBase + pEntry[H265_USERDATA_FLAG_CHROMA_RESAMPLING_FILTER_HINT].offset);
                VLOG(INFO, " CHROMA_RESAMPLING_FILTER_HINT\n");
                VLOG(INFO, " VER_CHROMA_FILTER_IDC: %10d HOR_CHROMA_FILTER_IDC: %10d\n", c_resampleing_filter_hint->ver_chroma_filter_idc, c_resampleing_filter_hint->hor_chroma_filter_idc);
                VLOG(INFO, " VER_FILTERING_FIELD_PROCESSING_FLAG: %d \n", c_resampleing_filter_hint->ver_filtering_field_processing_flag);
                if (c_resampleing_filter_hint->ver_chroma_filter_idc == 1 || c_resampleing_filter_hint->hor_chroma_filter_idc == 1) {
                    VLOG(INFO, " TARGET_FORMAT_IDC: %d \n", c_resampleing_filter_hint->target_format_idc);
                    if (c_resampleing_filter_hint->ver_chroma_filter_idc == 1) {
                        VLOG(INFO, " NUM_VERTICAL_FILTERS: %d \n", c_resampleing_filter_hint->num_vertical_filters);
                        for (i=0; i<c_resampleing_filter_hint->num_vertical_filters; i++) {
                            VLOG(INFO, " VER_TAP_LENGTH_M1[%d]: %d \n", i, c_resampleing_filter_hint->ver_tap_length_minus1[i]);
                            for (j=0; j<c_resampleing_filter_hint->ver_tap_length_minus1[i]; j++) {
                                VLOG(INFO, " VER_FILTER_COEFF[%d][%d]: %d \n", i, j, c_resampleing_filter_hint->ver_filter_coeff[i][j]);
                            }
                        }
                    }
                    if (c_resampleing_filter_hint->hor_chroma_filter_idc == 1) {
                        VLOG(INFO, " NUM_HORIZONTAL_FILTERS: %d \n", c_resampleing_filter_hint->num_horizontal_filters);
                        for (i=0; i<c_resampleing_filter_hint->num_horizontal_filters; i++) {
                            VLOG(INFO, " HOR_TAP_LENGTH_M1[%d]: %d \n", i, c_resampleing_filter_hint->hor_tap_length_minus1[i]);
                            for (j=0; j<c_resampleing_filter_hint->hor_tap_length_minus1[i]; j++) {
                                VLOG(INFO, " HOR_FILTER_COEFF[%d][%d]: %d \n", i, j, c_resampleing_filter_hint->hor_filter_coeff[i][j]);
                            }
                        }
                    }
                }
            }

            if (idx == H265_USERDATA_FLAG_KNEE_FUNCTION_INFO) {
                h265_knee_function_info_t* knee_function;

                knee_function = (h265_knee_function_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_KNEE_FUNCTION_INFO].offset);
                VLOG(INFO, " FLAG_KNEE_FUNCTION_INFO\n");
                VLOG(INFO, " KNEE_FUNCTION_ID: %10d\n", knee_function->knee_function_id);
                VLOG(INFO, " KNEE_FUNCTION_CANCEL_FLAG: %d\n", knee_function->knee_function_cancel_flag);
                if (!knee_function->knee_function_cancel_flag) {
                    int i;

                    VLOG(INFO, " KNEE_FUNCTION_PERSISTENCE_FLAG: %10d\n", knee_function->knee_function_persistence_flag);
                    VLOG(INFO, " INPUT_D_RANGE: %d\n", knee_function->input_d_range);
                    VLOG(INFO, " INPUT_DISP_LUMINANCE: %d\n", knee_function->input_disp_luminance);
                    VLOG(INFO, " OUTPUT_D_RANGE: %d\n", knee_function->output_d_range);
                    VLOG(INFO, " OUTPUT_DISP_LUMINANCE: %d\n", knee_function->output_disp_luminance);
                    VLOG(INFO, " NUM_KNEE_POINTS_M1: %d\n", knee_function->num_knee_points_minus1);
                    for (i=0; i<knee_function->num_knee_points_minus1; i++) {
                        VLOG(INFO, " INPUT_KNEE_POINT: %10d OUTPUT_KNEE_POINT: %10d\n", knee_function->input_knee_point[i], knee_function->output_knee_point[i]);
                    }
                }
            }

            if (idx == H265_USERDATA_FLAG_ITU_T_T35_PRE)
            {
                char* itu_t_t35 = (char*)(pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].offset);
                Uint32 i;

                VLOG(INFO, "ITU_T_T35_PRE = %d bytes, offset = %d\n",pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size, pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].offset);
                for(i=0; i<pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE].size; i++)
                {
                    VLOG(INFO, "%02X ", (unsigned char)(itu_t_t35[i]));
                }
                VLOG(INFO, "\n");

            }

            if (idx == H265_USERDATA_FLAG_ITU_T_T35_PRE_1)
            {
                char* itu_t_t35 = (char*)(pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].offset);
                Uint32 i;

                VLOG(INFO, "ITU_T_T35_PRE = %d bytes, offset = %d\n",pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size, pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].offset);
                for(i=0; i<pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_1].size; i++)
                {
                    VLOG(INFO, "%02X ", (unsigned char)(itu_t_t35[i]));
                }
                VLOG(INFO, "\n");
            }

            if (idx == H265_USERDATA_FLAG_ITU_T_T35_PRE_2)
            {
                char* itu_t_t35 = (char*)(pBase + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].offset);
                Uint32 i;
                //user_data_entry_t* pEntry_prev;

                VLOG(INFO, "ITU_T_T35_PRE = %d bytes, offset = %d\n",pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size, pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].offset);
                for(i=0; i<pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size; i++)
                {
                    VLOG(INFO, "%02X ", (unsigned char)(itu_t_t35[i]));
                }
                VLOG(INFO, "\n");

                //pEntry_prev = (user_data_entry_t*)(itu_t_t35 + pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size - sizeof(user_data_entry_t));
                //pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].size = pEntry_prev->size;
                //pEntry[H265_USERDATA_FLAG_ITU_T_T35_PRE_2].offset = pEntry_prev->offset;
            }

            if (idx == H265_USERDATA_FLAG_COLOUR_REMAPPING_INFO) {
                int c, i;
                h265_colour_remapping_info_t* colour_remapping;
                colour_remapping = (h265_colour_remapping_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_COLOUR_REMAPPING_INFO].offset);

                VLOG(INFO, " COLOUR_REMAPPING_INFO\n");
                VLOG(INFO, " COLOUR_REMAP_ID: %10d\n", colour_remapping->colour_remap_id);


                VLOG(INFO, " COLOUR_REMAP_CANCEL_FLAG: %d\n", colour_remapping->colour_remap_cancel_flag);
                if (!colour_remapping->colour_remap_cancel_flag) {
                    VLOG(INFO, " COLOUR_REMAP_PERSISTENCE_FLAG: %d\n", colour_remapping->colour_remap_persistence_flag);
                    VLOG(INFO, " COLOUR_REMAP_VIDEO_SIGNAL_INFO_PRESENT_FLAG: %d\n", colour_remapping->colour_remap_video_signal_info_present_flag);
                    if (colour_remapping->colour_remap_video_signal_info_present_flag) {
                        VLOG(INFO, " COLOUR_REMAP_FULL_RANGE_FLAG: %d\n", colour_remapping->colour_remap_full_range_flag);
                        VLOG(INFO, " COLOUR_REMAP_PRIMARIES: %d\n",	colour_remapping->colour_remap_primaries);
                        VLOG(INFO, " COLOUR_REMAP_TRANSFER_FUNCTION: %d\n", colour_remapping->colour_remap_transfer_function);
                        VLOG(INFO, " COLOUR_REMAP_MATRIX_COEFFICIENTS: %d\n", colour_remapping->colour_remap_matrix_coefficients);
                    }

                    VLOG(INFO, " COLOUR_REMAP_INPUT_BIT_DEPTH: %d\n", colour_remapping->colour_remap_input_bit_depth);
                    VLOG(INFO, " COLOUR_REMAP_BIT_DEPTH: %d\n", colour_remapping->colour_remap_bit_depth);

                    for( c = 0; c < H265_MAX_LUT_NUM_VAL; c++ )
                    {
                        VLOG(INFO, " PRE_LUT_NUM_VAL_MINUS1[%d]: %d\n", c, colour_remapping->pre_lut_num_val_minus1[c]);
                        if(colour_remapping->pre_lut_num_val_minus1[c] > 0)
                        {
                            for( i = 0; i <= colour_remapping->pre_lut_num_val_minus1[c]; i++ )
                            {
                                VLOG(INFO, " PRE_LUT_CODED_VALUE[%d][%d]: %d\n", c, i, colour_remapping->pre_lut_coded_value[c][i]);
                                VLOG(INFO, " PRE_LUT_TARGET_VALUE[%d][%d]: %d\n", c, i, colour_remapping->pre_lut_target_value[c][i]);
                            }
                        }
                    }
                    VLOG(INFO, " COLOUR_REMAP_MATRIX_PRESENT_FLAG: %d\n", colour_remapping->colour_remap_matrix_present_flag);
                    if(colour_remapping->colour_remap_matrix_present_flag) {
                        VLOG(INFO, " LOG2_MATRIX_DENOM: %d\n", colour_remapping->log2_matrix_denom);
                        for( c = 0; c < H265_MAX_COLOUR_REMAP_COEFFS; c++ )
                            for( i = 0; i < H265_MAX_COLOUR_REMAP_COEFFS; i++ )
                                VLOG(INFO, " LOG2_MATRIX_DENOM[%d][%d]: %d\n", c, i, colour_remapping->colour_remap_coeffs[c][i]);
                    }

                    for( c = 0; c < H265_MAX_LUT_NUM_VAL; c++ )
                    {
                        VLOG(INFO, " POST_LUT_NUM_VAL_MINUS1[%d]: %d\n", c, colour_remapping->post_lut_num_val_minus1[c]);
                        if(colour_remapping->post_lut_num_val_minus1[c] > 0)
                        {
                            for( i = 0; i <= colour_remapping->post_lut_num_val_minus1[c]; i++)
                            {
                                VLOG(INFO, " POST_LUT_CODED_VALUE[%d][%d]: %d\n", c, i, colour_remapping->post_lut_coded_value[c][i]);
                                VLOG(INFO, " POST_LUT_TARGET_VALUE[%d][%d]: %d\n", c, i, colour_remapping->post_lut_target_value[c][i]);
                            }
                        }
                    }
                }
            }

            if (idx == H265_USERDATA_FLAG_TONE_MAPPING_INFO) {
                h265_tone_mapping_info_t* tone_mapping;

                tone_mapping = (h265_tone_mapping_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_TONE_MAPPING_INFO].offset);
                VLOG(INFO, " FLAG_TONE_MAPPING_INFO\n");
                VLOG(INFO, " TONE_MAP_ID: %10d\n", tone_mapping->tone_map_id);
                VLOG(INFO, " TONE_MAP_CANCEL_FLAG: %d\n", tone_mapping->tone_map_cancel_flag);
                if (!tone_mapping->tone_map_cancel_flag) {
                    int i;

                    VLOG(INFO, " TONE_MAP_PERSISTENCE_FLAG: %10d\n", tone_mapping->tone_map_persistence_flag);
                    VLOG(INFO, " CODED_DATA_BIT_DEPTH : %d\n", tone_mapping->coded_data_bit_depth);
                    VLOG(INFO, " TARGET_BIT_DEPTH : %d\n", tone_mapping->target_bit_depth);
                    VLOG(INFO, " TONE_MAP_MODEL_ID : %d\n", tone_mapping->tone_map_model_id);
                    VLOG(INFO, " MIN_VALUE : %d\n", tone_mapping->min_value);
                    VLOG(INFO, " MAX_VALUE : %d\n", tone_mapping->max_value);
                    VLOG(INFO, " SIGMOID_MIDPOINT : %d\n", tone_mapping->sigmoid_midpoint);
                    VLOG(INFO, " SIGMOID_MIDPOINT : %d\n", tone_mapping->sigmoid_width);
                    for (i=0; i<(1<<tone_mapping->target_bit_depth); i++) {
                        VLOG(INFO, " START_OF_CODED_INTERVAL[%d] : %d\n", i, tone_mapping->start_of_coded_interval[i]); // [1 << target_bit_depth] // 10bits
                    }

                    VLOG(INFO, " NUM_PIVOTS : %d\n", tone_mapping->num_pivots); // [(1 << coded_data_bit_depth)?1][(1 << target_bit_depth)-1] // 10bits
                    for (i=0; i<tone_mapping->num_pivots; i++) {
                        VLOG(INFO, " CODED_PIVOT_VALUE[%d] : %d, TARGET_PIVOT_VALUE[%d] : %d\n", i, tone_mapping->coded_pivot_value[i]
                        , i, tone_mapping->target_pivot_value[i]);
                    }

                    VLOG(INFO, " CAMERA_ISO_SPEED_IDC : %d\n", tone_mapping->camera_iso_speed_idc);
                    VLOG(INFO, " CAMERA_ISO_SPEED_VALUE : %d\n", tone_mapping->camera_iso_speed_value);

                    VLOG(INFO, " EXPOSURE_INDEX_IDC : %d\n", tone_mapping->exposure_index_idc);
                    VLOG(INFO, " EXPOSURE_INDEX_VALUE : %d\n", tone_mapping->exposure_index_value);
                    VLOG(INFO, " EXPOSURE_INDEX_COMPESATION_VALUE_SIGN_FLAG : %d\n", tone_mapping->exposure_compensation_value_sign_flag);
                    VLOG(INFO, " EXPOSURE_INDEX_COMPESATION_VALUE_NUMERATOR : %d\n", tone_mapping->exposure_compensation_value_numerator);
                    VLOG(INFO, " EXPOSURE_INDEX_COMPESATION_VALUE_DENOM_IDC : %d\n", tone_mapping->exposure_compensation_value_denom_idc);

                    VLOG(INFO, " REF_SCREEN_LUMINANCE_WHITE : %d\n", tone_mapping->ref_screen_luminance_white);

                    VLOG(INFO, " EXTENDED_RANGE_WHITE_LEVEL : %d\n", tone_mapping->extended_range_white_level);
                    VLOG(INFO, " NOMINAL_BLACK_LEVEL_CODE_VALUE : %d\n", tone_mapping->nominal_black_level_code_value);
                    VLOG(INFO, " NOMINAL_WHITE_LEVEL_CODE_VALUE : %d\n", tone_mapping->nominal_white_level_code_value);
                    VLOG(INFO, " EXTENDED_WHITE_LEVEL_CODE_VALUE : %d\n", tone_mapping->extended_white_level_code_value);
                }
                VLOG(INFO, "\n");
            }

            if (idx == H265_USERDATA_FLAG_CONTENT_LIGHT_LEVEL_INFO) {
                h265_content_light_level_info_t* content_light_level;

                content_light_level = (h265_content_light_level_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_CONTENT_LIGHT_LEVEL_INFO].offset);
                VLOG(INFO, " CONTNET_LIGHT_INFO\n");
                VLOG(INFO, " MAX_CONTENT_LIGHT_LEVEL : %d\n", content_light_level->max_content_light_level);
                VLOG(INFO, " MAX_PIC_AVERAGE_LIGHT_LEVEL : %d\n", content_light_level->max_pic_average_light_level);
                VLOG(INFO, "\n");
            }

            if (idx == H265_USERDATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO) {
                h265_film_grain_characteristics_t* film_grain_characteristics;
                int i,j,c;

                film_grain_characteristics = (h265_film_grain_characteristics_t*)(pBase + pEntry[H265_USERDATA_FLAG_FILM_GRAIN_CHARACTERISTICS_INFO].offset);
                VLOG(INFO, " FILM_GRAIN_CHARACTERISTICS_INFO\n");
                VLOG(INFO, " FILM_GRAIN_CHARACTERISTICS_CANCEL_FLAG: %d\n", film_grain_characteristics->film_grain_characteristics_cancel_flag);
                if (!film_grain_characteristics->film_grain_characteristics_cancel_flag) {
                    VLOG(INFO, " FILM_GRAIN_MODEL_ID: %10d\n", film_grain_characteristics->film_grain_model_id);

                    VLOG(INFO, " SEPARATE_COLOUR_DESCRIPTION_PRESENT_FLAG: %d\n", film_grain_characteristics->separate_colour_description_present_flag);
                    if (film_grain_characteristics->separate_colour_description_present_flag)
                    {
                        VLOG(INFO, " FILM_GRAIN_BIT_DEPTH_LUMA_MINUS8: %d\n", film_grain_characteristics->film_grain_bit_depth_luma_minus8);
                        VLOG(INFO, " FILM_GRAIN_BIT_DEPTH_CHROMA_MINUS8: %d\n", film_grain_characteristics->film_grain_bit_depth_chroma_minus8);
                        VLOG(INFO, " FILM_GRAIN_FULL_RANGE_FLAG: %d\n", film_grain_characteristics->film_grain_full_range_flag);
                        VLOG(INFO, " FILM_GRAIN_COLOUR_PRIMARIES: %d\n", film_grain_characteristics->film_grain_colour_primaries);
                        VLOG(INFO, " FILM_GRAIN_TRANSFER_CHARACTERISTICS: %d\n", film_grain_characteristics->film_grain_transfer_characteristics);
                        VLOG(INFO, " FILM_GRAIN_MATRIX_COEFF: %d\n", film_grain_characteristics->film_grain_matrix_coeffs);
                    }
                }

                VLOG(INFO, " BLENDING_MODE_ID: %d\n", film_grain_characteristics->blending_mode_id);
                VLOG(INFO, " LOG2_SCALE_FACTOR: %d\n", film_grain_characteristics->log2_scale_factor);
                for(c=0; c < H265_MAX_NUM_FILM_GRAIN_COMPONENT; c++ )
                {
                    VLOG(INFO, " COMP_MODEL_PRESENT_FLAG[%d]: %d\n", c, film_grain_characteristics->comp_model_present_flag[c]);
                }

                for(c=0; c < H265_MAX_NUM_FILM_GRAIN_COMPONENT; c++ )
                {
                    if(film_grain_characteristics->comp_model_present_flag[c])
                    {

                        VLOG(INFO, " NUM_INTENSITY_INTERVALS_MINUS1[%d]: %d\n", c, film_grain_characteristics->num_intensity_intervals_minus1[c]);
                        VLOG(INFO, " NUM_MODEL_VALUES_MINUS1[%d]: %d\n", c, film_grain_characteristics->num_model_values_minus1[c]);
                        for(i=0; i <= film_grain_characteristics->num_intensity_intervals_minus1[c]; i++)
                        {
                            VLOG(INFO, " INTENSITY_INTERVAL_LOWER_BOUND[%d][%d]: %d\n", c, i, film_grain_characteristics->intensity_interval_lower_bound[c][i]);
                            VLOG(INFO, " INTENSITY_INTERVAL_UPPER_BOUND[%d][%d]: %d\n", c, i, film_grain_characteristics->intensity_interval_upper_bound[c][i]);
                            for(j=0; j <= film_grain_characteristics->num_model_values_minus1[c]; j++)
                            {
                                VLOG(INFO, " COMP_MODEL_VALUE[%d][%d][%d]: %d\n", c, i, j, film_grain_characteristics->comp_model_value[c][i][j]);
                            }
                        }
                    }
                }
                VLOG(INFO, " FILM_GRAIN_CHARACTERISTICS_PERSISTENCE_FLAG: %d\n", film_grain_characteristics->film_grain_characteristics_persistence_flag);
                VLOG(INFO, "\n");
            }
        }
    }
    VLOG(INFO, "===========================================\n");
}

void SetMapData(int coreIdx, TestEncConfig encConfig, EncOpenParam encOP, EncParam *encParam, int srcFrameWidth, int srcFrameHeight, u64 addrCustomMap)
{
    int productId = VPU_GetProductId(coreIdx);

    if (productId == PRODUCT_ID_521 && encOP.bitstreamFormat == STD_AVC) {
        Uint8               roiMapBuf[MAX_MB_NUM] = {0,};
        Uint8               modeMapBuf[MAX_MB_NUM] = {0,};
        AvcEncCustomMap     customMapBuf[MAX_MB_NUM];	// custom map 1 MB data = 8bits
        int MbWidth         = VPU_ALIGN16(encOP.picWidth) >> 4;
        int MbHeight        = VPU_ALIGN16(encOP.picHeight) >> 4;
        int h, w, mbAddr;
        osal_memset(&customMapBuf[0], 0x00, MAX_MB_NUM);
        if (encParam->customMapOpt.customRoiMapEnable == 1) {
            int sumQp = 0;
            int bufSize = MbWidth*MbHeight;

            if (osal_fread(&roiMapBuf[0], 1, bufSize, encConfig.roi_file) != bufSize) {
                osal_fseek(encConfig.roi_file, 0, SEEK_SET);
                osal_fread(&roiMapBuf[0], 1, bufSize, encConfig.roi_file);
            }

            for (h = 0; h < MbHeight; h++) {
                for (w = 0; w < MbWidth; w++) {
                    mbAddr = w + h*MbWidth;
                    customMapBuf[mbAddr].field.mb_qp = MAX(MIN(roiMapBuf[mbAddr]&0x3f, 51), 0);
                    sumQp += customMapBuf[mbAddr].field.mb_qp;
                }
            }
            encParam->customMapOpt.roiAvgQp = (sumQp + (bufSize>>1)) / bufSize; // round off.
        }

        if (encParam->customMapOpt.customModeMapEnable == 1) {
                int bufSize = MbWidth*MbHeight;

                if (osal_fread(&modeMapBuf[0], 1, bufSize, encConfig.mode_map_file) != bufSize) {
                    osal_fseek(encConfig.mode_map_file, 0, SEEK_SET);
                    osal_fread(&modeMapBuf[0], 1, bufSize, encConfig.mode_map_file);
                }

                for (h = 0; h < MbHeight; h++) {
                    for (w = 0; w < MbWidth; w++) {
                        mbAddr = w + h*MbWidth;
                        customMapBuf[mbAddr].field.mb_force_mode = modeMapBuf[mbAddr] & 0x3;
                    }
                }
        }

        encParam->customMapOpt.addrCustomMap = addrCustomMap;
        vdi_write_memory(coreIdx, encParam->customMapOpt.addrCustomMap, (unsigned char*)customMapBuf, MAX_MB_NUM, VDI_LITTLE_ENDIAN);
    }
    else {
        // for HEVC custom map.
        Uint8               roiMapBuf[MAX_SUB_CTU_NUM] = {0,};
        Uint8               lambdaMapBuf[MAX_SUB_CTU_NUM] = {0,};
        Uint8               modeMapBuf[MAX_CTU_NUM] = {0,};
        EncCustomMap        customMapBuf[MAX_CTU_NUM];	// custom map 1 CTU data = 64bits
        int ctuMapWidthCnt  = VPU_ALIGN64(encOP.picWidth) >> 6;
        int ctuMapHeightCnt = VPU_ALIGN64(encOP.picHeight) >> 6;
        int ctuMapStride = VPU_ALIGN64(encOP.picWidth) >> 6;
        int subCtuMapStride = VPU_ALIGN64(encOP.picWidth) >> 5;
        int i, sumQp = 0;;
        int h, w, ctuPos, initQp;
        Uint8* src;
        VpuRect region[MAX_ROI_NUMBER];        /**< The size of the ROI region for H.265 (start X/Y in CTU, end X/Y int CTU)  */
        int roiQp[MAX_ROI_NUMBER];       /**< An importance level for the given ROI region for H.265. The higher an ROI level is, the more important the region is with a lower QP.  */

        osal_memset(&customMapBuf[0], 0x00, MAX_CTU_NUM * 8);
        if (encParam->customMapOpt.customRoiMapEnable == 1) {
            int bufSize = (VPU_ALIGN64(encOP.picWidth) >> 5) * (VPU_ALIGN64(encOP.picHeight) >> 5);

            if (productId == PRODUCT_ID_521) {
                if (osal_fread(&roiMapBuf[0], 1, bufSize, encConfig.roi_file) != bufSize) {
                    osal_fseek(encConfig.roi_file, 0, SEEK_SET);
                    osal_fread(&roiMapBuf[0], 1, bufSize, encConfig.roi_file);
                }
            }
            else {
                int roiNum = 0;
                // sample code to convert ROI coordinate to ROI map.
                osal_memset(&region[0], 0, sizeof(VpuRect)*MAX_ROI_NUMBER);
                osal_memset(&roiQp[0], 0, sizeof(int)*MAX_ROI_NUMBER);
                if (encConfig.roi_file_name[0]) {
                    char lineStr[256] = {0,};
                    int  val;
                    osal_fgets(lineStr, 256, encConfig.roi_file);
                    osal_sscanf(lineStr, "%x\n", &val);
                    if (val != 0xFFFF)
                        VLOG(ERR, "can't find the picture delimiter \n");

                    osal_fgets(lineStr, 256, encConfig.roi_file);
                    osal_sscanf(lineStr, "%d\n", &val);  // picture index

                    osal_fgets(lineStr, 256, encConfig.roi_file);
                    osal_sscanf(lineStr, "%d\n", &roiNum);   // number of roi regions

                    for (i = 0; i < roiNum; i++) {
                        osal_fgets(lineStr, 256, encConfig.roi_file);
                        if (parseRoiCtuModeParam(lineStr, &region[i], &roiQp[i], srcFrameWidth, srcFrameHeight) == 0) {
                            VLOG(ERR, "CFG file error : %s value is not available. \n", encConfig.roi_file_name);
                        }
                    }
                }
                encParam->customMapOpt.customRoiMapEnable    = (roiNum != 0) ? encParam->customMapOpt.customRoiMapEnable : 0;

                if (encParam->customMapOpt.customRoiMapEnable) {
                    initQp = encConfig.roi_avg_qp;
                    GenRegionToQpMap(&region[0], &roiQp[0], roiNum, initQp, subCtuMapStride, VPU_ALIGN64(encOP.picHeight) >> 5, &roiMapBuf[0]);
                }
            }

            for (h = 0; h < ctuMapHeightCnt; h++) {
                src = &roiMapBuf[subCtuMapStride * h * 2];
                for (w = 0; w < ctuMapWidthCnt; w++, src += 2) {
                    ctuPos = (h * ctuMapStride + w);

                    // store in the order of sub-ctu.
                    customMapBuf[ctuPos].field.sub_ctu_qp_0 = MAX(MIN(*src, 51), 0);
                    customMapBuf[ctuPos].field.sub_ctu_qp_1 = MAX(MIN(*(src + 1), 51), 0);
                    customMapBuf[ctuPos].field.sub_ctu_qp_2 = MAX(MIN(*(src + subCtuMapStride), 51), 0);
                    customMapBuf[ctuPos].field.sub_ctu_qp_3 = MAX(MIN(*(src + subCtuMapStride + 1), 51), 0);
                    sumQp += (customMapBuf[ctuPos].field.sub_ctu_qp_0 + customMapBuf[ctuPos].field.sub_ctu_qp_1 + customMapBuf[ctuPos].field.sub_ctu_qp_2 + customMapBuf[ctuPos].field.sub_ctu_qp_3);
                }
            }
            if (productId == PRODUCT_ID_521)
                encParam->customMapOpt.roiAvgQp = (sumQp + (bufSize>>1)) / bufSize; // round off.
        }

        if (encParam->customMapOpt.customLambdaMapEnable == 1) {
            int bufSize = (VPU_ALIGN64(encOP.picWidth) >> 5) * (VPU_ALIGN64(encOP.picHeight) >> 5);

            if (osal_fread(&lambdaMapBuf[0], 1, bufSize, encConfig.lambda_map_file) != bufSize) {
                osal_fseek(encConfig.lambda_map_file, 0, SEEK_SET);
                osal_fread(&lambdaMapBuf[0], 1, bufSize, encConfig.lambda_map_file);
            }

            for (h = 0; h < ctuMapHeightCnt; h++) {
                src = &lambdaMapBuf[subCtuMapStride * 2 * h];
                for (w = 0; w < ctuMapWidthCnt; w++, src += 2) {
                    ctuPos = (h * ctuMapStride + w);

                    // store in the order of sub-ctu.
                    customMapBuf[ctuPos].field.lambda_sad_0 = *src;
                    customMapBuf[ctuPos].field.lambda_sad_1 = *(src + 1);
                    customMapBuf[ctuPos].field.lambda_sad_2 = *(src + subCtuMapStride);
                    customMapBuf[ctuPos].field.lambda_sad_3 = *(src + subCtuMapStride + 1);
                }
            }
        }

        if ((encParam->customMapOpt.customModeMapEnable == 1)
            || (encParam->customMapOpt.customCoefDropEnable == 1)) {
                int bufSize = (VPU_ALIGN64(encOP.picWidth) >> 6) * (VPU_ALIGN64(encOP.picHeight) >> 6);

                if (osal_fread(&modeMapBuf[0], 1, bufSize, encConfig.mode_map_file) != bufSize) {
                    osal_fseek(encConfig.mode_map_file, 0, SEEK_SET);
                    osal_fread(&modeMapBuf[0], 1, bufSize, encConfig.mode_map_file);
                }

                for (h = 0; h < ctuMapHeightCnt; h++) {
                    src = &modeMapBuf[ctuMapStride * h];
                    for (w = 0; w < ctuMapWidthCnt; w++, src++) {
                        ctuPos = (h * ctuMapStride + w);
                        customMapBuf[ctuPos].field.ctu_force_mode = (*src) & 0x3;
                        customMapBuf[ctuPos].field.ctu_coeff_drop  = ((*src) >> 2) & 0x1;
                    }
                }
        }

        encParam->customMapOpt.addrCustomMap = addrCustomMap;
        vdi_write_memory(coreIdx, encParam->customMapOpt.addrCustomMap, (unsigned char*)customMapBuf, MAX_CTU_NUM * 8, VDI_LITTLE_ENDIAN);
    }

}

RetCode SetChangeParam(EncHandle handle, TestEncConfig encConfig, EncOpenParam encOP, Int32 changedCount)
{
    int i;
    RetCode ret;
    ENC_CFG ParaChagCfg;
    EncChangeParam changeParam;

    osal_memset(&ParaChagCfg, 0x00, sizeof(ENC_CFG));
    osal_memset(&changeParam, 0x00, sizeof(EncChangeParam));

    parseWaveChangeParamCfgFile(&ParaChagCfg, encConfig.changeParam[changedCount].cfgName);

    changeParam.enable_option           = encConfig.changeParam[changedCount].enableOption;

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_PPS) {
        changeParam.constIntraPredFlag			= ParaChagCfg.waveCfg.constIntraPredFlag;
        changeParam.lfCrossSliceBoundaryEnable	= ParaChagCfg.waveCfg.lfCrossSliceBoundaryEnable;
        changeParam.weightPredEnable			= ParaChagCfg.waveCfg.weightPredEnable;
        changeParam.disableDeblk				= ParaChagCfg.waveCfg.disableDeblk;
        changeParam.betaOffsetDiv2				= ParaChagCfg.waveCfg.betaOffsetDiv2;
        changeParam.tcOffsetDiv2				= ParaChagCfg.waveCfg.tcOffsetDiv2;
        changeParam.chromaCbQpOffset			= ParaChagCfg.waveCfg.chromaCbQpOffset;
        changeParam.chromaCrQpOffset			= ParaChagCfg.waveCfg.chromaCrQpOffset;
        if (encConfig.stdMode == STD_AVC) {
            changeParam.transform8x8Enable      = ParaChagCfg.waveCfg.transform8x8;
            changeParam.entropyCodingMode       = ParaChagCfg.waveCfg.entropyCodingMode;
        }
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_INDEPEND_SLICE) {
        changeParam.independSliceMode			= ParaChagCfg.waveCfg.independSliceMode;
        changeParam.independSliceModeArg		= ParaChagCfg.waveCfg.independSliceModeArg;
        if (encConfig.stdMode == STD_AVC) {
            changeParam.avcSliceMode            = ParaChagCfg.waveCfg.avcSliceMode;
            changeParam.avcSliceArg             = ParaChagCfg.waveCfg.avcSliceArg;
        }
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_DEPEND_SLICE) {
        changeParam.dependSliceMode				= ParaChagCfg.waveCfg.dependSliceMode;
        changeParam.dependSliceModeArg			= ParaChagCfg.waveCfg.dependSliceModeArg;
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_RDO) {
        changeParam.coefClearDisable			= ParaChagCfg.waveCfg.coefClearDisable;
        changeParam.intraNxNEnable				= ParaChagCfg.waveCfg.intraNxNEnable;
        changeParam.maxNumMerge					= ParaChagCfg.waveCfg.maxNumMerge;
        changeParam.customLambdaEnable			= ParaChagCfg.waveCfg.customLambdaEnable;
        changeParam.customMDEnable				= ParaChagCfg.waveCfg.customMDEnable;
        if (encConfig.stdMode == STD_AVC) {
            changeParam.rdoSkip                 = ParaChagCfg.waveCfg.rdoSkip;
            changeParam.lambdaScalingEnable     = ParaChagCfg.waveCfg.lambdaScalingEnable;
        }
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_RC_TARGET_RATE) {
        changeParam.bitRate						= ParaChagCfg.RcBitRate;
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_RC) {
        changeParam.hvsQPEnable					= ParaChagCfg.waveCfg.hvsQPEnable;
        changeParam.hvsQpScale					= ParaChagCfg.waveCfg.hvsQpScale;
        changeParam.vbvBufferSize				= ParaChagCfg.VbvBufferSize;
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_RC_MIN_MAX_QP) {
        changeParam.minQpI						= ParaChagCfg.waveCfg.minQp;
        changeParam.minQpP						= ParaChagCfg.waveCfg.minQp;
        changeParam.minQpB						= ParaChagCfg.waveCfg.minQp;
        changeParam.maxQpI						= ParaChagCfg.waveCfg.maxQp;
        changeParam.maxQpP						= ParaChagCfg.waveCfg.maxQp;
        changeParam.maxQpB						= ParaChagCfg.waveCfg.maxQp;
        changeParam.maxDeltaQp					= ParaChagCfg.waveCfg.maxDeltaQp;
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_RC_BIT_RATIO_LAYER) {
        for (i=0 ; i<MAX_GOP_NUM; i++)
            changeParam.fixedBitRatio[i]	= ParaChagCfg.waveCfg.fixedBitRatio[i];
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_BG) {
        changeParam.s2fmeDisable                = ParaChagCfg.waveCfg.s2fmeDisable;
        changeParam.bgThrDiff					= ParaChagCfg.waveCfg.bgThrDiff;
        changeParam.bgThrMeanDiff				= ParaChagCfg.waveCfg.bgThrMeanDiff;
        changeParam.bgLambdaQp					= ParaChagCfg.waveCfg.bgLambdaQp;
        changeParam.bgDeltaQp					= ParaChagCfg.waveCfg.bgDeltaQp;
        changeParam.s2fmeDisable                = ParaChagCfg.waveCfg.s2fmeDisable;
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_NR) {
        changeParam.nrYEnable					= ParaChagCfg.waveCfg.nrYEnable;
        changeParam.nrCbEnable					= ParaChagCfg.waveCfg.nrCbEnable;
        changeParam.nrCrEnable					= ParaChagCfg.waveCfg.nrCrEnable;
        changeParam.nrNoiseEstEnable			= ParaChagCfg.waveCfg.nrNoiseEstEnable;
        changeParam.nrNoiseSigmaY				= ParaChagCfg.waveCfg.nrNoiseSigmaY;
        changeParam.nrNoiseSigmaCb				= ParaChagCfg.waveCfg.nrNoiseSigmaCb;
        changeParam.nrNoiseSigmaCr				= ParaChagCfg.waveCfg.nrNoiseSigmaCr;

        changeParam.nrIntraWeightY				= ParaChagCfg.waveCfg.nrIntraWeightY;
        changeParam.nrIntraWeightCb				= ParaChagCfg.waveCfg.nrIntraWeightCb;
        changeParam.nrIntraWeightCr				= ParaChagCfg.waveCfg.nrIntraWeightCr;
        changeParam.nrInterWeightY				= ParaChagCfg.waveCfg.nrInterWeightY;
        changeParam.nrInterWeightCb				= ParaChagCfg.waveCfg.nrInterWeightCb;
        changeParam.nrInterWeightCr				= ParaChagCfg.waveCfg.nrInterWeightCr;
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_CUSTOM_MD) {
        changeParam.pu04DeltaRate               = ParaChagCfg.waveCfg.pu04DeltaRate;
        changeParam.pu08DeltaRate               = ParaChagCfg.waveCfg.pu08DeltaRate;
        changeParam.pu16DeltaRate               = ParaChagCfg.waveCfg.pu16DeltaRate;
        changeParam.pu32DeltaRate               = ParaChagCfg.waveCfg.pu32DeltaRate;
        changeParam.pu04IntraPlanarDeltaRate    = ParaChagCfg.waveCfg.pu04IntraPlanarDeltaRate;
        changeParam.pu04IntraDcDeltaRate        = ParaChagCfg.waveCfg.pu04IntraDcDeltaRate;
        changeParam.pu04IntraAngleDeltaRate     = ParaChagCfg.waveCfg.pu04IntraAngleDeltaRate;
        changeParam.pu08IntraPlanarDeltaRate    = ParaChagCfg.waveCfg.pu08IntraPlanarDeltaRate;
        changeParam.pu08IntraDcDeltaRate        = ParaChagCfg.waveCfg.pu08IntraDcDeltaRate;
        changeParam.pu08IntraAngleDeltaRate     = ParaChagCfg.waveCfg.pu08IntraAngleDeltaRate;
        changeParam.pu16IntraPlanarDeltaRate    = ParaChagCfg.waveCfg.pu16IntraPlanarDeltaRate;
        changeParam.pu16IntraDcDeltaRate        = ParaChagCfg.waveCfg.pu16IntraDcDeltaRate;
        changeParam.pu16IntraAngleDeltaRate     = ParaChagCfg.waveCfg.pu16IntraAngleDeltaRate;
        changeParam.pu32IntraPlanarDeltaRate    = ParaChagCfg.waveCfg.pu32IntraPlanarDeltaRate;
        changeParam.pu32IntraDcDeltaRate        = ParaChagCfg.waveCfg.pu32IntraDcDeltaRate;
        changeParam.pu32IntraAngleDeltaRate     = ParaChagCfg.waveCfg.pu32IntraAngleDeltaRate;
        changeParam.cu08IntraDeltaRate          = ParaChagCfg.waveCfg.cu08IntraDeltaRate;
        changeParam.cu08InterDeltaRate          = ParaChagCfg.waveCfg.cu08InterDeltaRate;
        changeParam.cu08MergeDeltaRate          = ParaChagCfg.waveCfg.cu08MergeDeltaRate;
        changeParam.cu16IntraDeltaRate          = ParaChagCfg.waveCfg.cu16IntraDeltaRate;
        changeParam.cu16InterDeltaRate          = ParaChagCfg.waveCfg.cu16InterDeltaRate;
        changeParam.cu16MergeDeltaRate          = ParaChagCfg.waveCfg.cu16MergeDeltaRate;
        changeParam.cu32IntraDeltaRate          = ParaChagCfg.waveCfg.cu32IntraDeltaRate;
        changeParam.cu32InterDeltaRate          = ParaChagCfg.waveCfg.cu32InterDeltaRate;
        changeParam.cu32MergeDeltaRate          = ParaChagCfg.waveCfg.cu32MergeDeltaRate;
    }

    if (changeParam.enable_option & ENC_SET_CHANGE_PARAM_INTRA_PARAM) {
        changeParam.intraQP                     = ParaChagCfg.waveCfg.intraQP;
        changeParam.intraPeriod                 = ParaChagCfg.waveCfg.intraPeriod;
    }

    ret = VPU_EncGiveCommand(handle, ENC_SET_PARA_CHANGE, &changeParam);

    return ret;
}


