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
#ifdef _WIN32
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include  < MMSystem.h >
#pragma comment(lib, "winmm.lib")
#elif __linux__
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h> // gettimeofday()
#include <fcntl.h>
#include <time.h>
#include <string.h>
#endif

#include "bm_vpudec_interface.h"
#include "bm_vpudec_internal.h"
#include "vpuapifunc.h"
#include "vdi.h"
#include "misc/debug.h"

#if defined(CHIP_BM1684)
#ifdef BM_PCIE_MODE
#define VPU_DEVICE_NAME "/dev/bm-sophon"
#else
#define VPU_DEVICE_NAME "/dev/vpu"
#endif
#endif

#define VID_PERFORMANCE_TEST
//#define STREAM_BUF_SIZE                 0x700000    // max bitstream size
#define PPU_FB_COUNT                    5
#define EXTRA_FRAME_BUFFER_NUM          5

#define MAX_SEQUENCE_MEM_COUNT          16

#define USLEEP_CLOCK  10000
#define INPUT_QUEUE_LEN 20
#define MAX_QUEUE_LEN 512
#define STREAM_ALIGNED_LEN 0
#define VIDEO_CAP_NONE 3

#define BUFFER_ALLOC_FROM_USER      1

#ifdef TRY_FLOCK_OPEN
#define VID_OPEN_FLOCK_NAME "/tmp/vid_open_global_flock"
#define VID_RESET_FLOCK_NAME "/tmp/vid_sw_reset_flock_%d_%d"
#ifdef __linux__
static __thread int s_vid_open_flock_fd[MAX_PCIE_BOARD_NUM] = {[0 ... (MAX_PCIE_BOARD_NUM-1)] = -1};
static __thread int s_vid_reset_flock_fd[MAX_PCIE_BOARD_NUM][MAX_NUM_VPU_CORE_CHIP] = {[0 ... (MAX_PCIE_BOARD_NUM-1)][0 ... (MAX_NUM_VPU_CORE_CHIP-1)] = -1};
#elif _WIN32
static  __declspec(thread) HANDLE s_vid_open_flock_fd[MAX_PCIE_BOARD_NUM] = {  NULL };
static  __declspec(thread) HANDLE s_vid_reset_flock_fd[MAX_PCIE_BOARD_NUM][MAX_NUM_VPU_CORE_CHIP] = { NULL };
#endif
#else
#define VID_OPEN_SEM_NAME "/vid_open_global_sem"
#endif
//for dump frame for debuging lasi..
#ifndef BM_PCIE_MODE
#ifdef __linux__
static __thread int dump_frame_num = 0;
#elif _WIN32
static  __declspec(thread) int dump_frame_num = 0;
#endif
#endif

/* This is a flag of init flag of each core.  */
static void* sVpuInstPool[MAX_VPU_CORE_NUM * MAX_NUM_INSTANCE] = {0};

extern RetCode VPU_DecUpdateBitstreamBufferParam(DecHandle handle, int size);
extern int VPU_DecGetBitstreamBufferRoom(DecHandle handle, PhysicalAddress wrPtr);

extern Uint32 Queue_Is_Full(Queue* queue);

extern void PrintVpuStatus(Uint32 coreIdx, Uint32 productId);

int BMVidDecSeqInitW5(BMVidCodHandle vidCodHandle);

static char * bmvpu_dec_error_string(RetCode errorcode){
    switch (errorcode)
    {
    case RETCODE_SUCCESS:
        return "ok";
    case RETCODE_FAILURE:
        return "unspecified error";
    case RETCODE_INVALID_HANDLE:
        return "invalid handle";
    case RETCODE_INVALID_PARAM:
        return "invalid params";
    case RETCODE_INVALID_COMMAND:
        return "invalid cmmoand";
    case RETCODE_ROTATOR_STRIDE_NOT_SET:
        return "rotator stride is not set";
    case RETCODE_FRAME_NOT_COMPLETE:
        return "decoding was not completed yet,";
    case RETCODE_INSUFFICIENT_FRAME_BUFFERS:
        return "frame buffers not enough";
    case RETCODE_INVALID_STRIDE:
        return "invalid stride";
    case RETCODE_WRONG_CALL_SEQUENCE:
        return "wrong call sequence";
    case RETCODE_CALLED_BEFORE:
        return "invalid multiple calls";
    case RETCODE_NOT_INITIALIZED:
        return "VPU not initialized yet";
    case RETCODE_USERDATA_BUF_NOT_SET:
        return "not alloc the userdata mem";
    case RETCODE_MEMORY_ACCESS_VIOLATION:
        return "access the protected memory";
    case RETCODE_VPU_RESPONSE_TIMEOUT:
        return "timeout";
    case RETCODE_INSUFFICIENT_RESOURCE:
        return "memory lack";
    case RETCODE_NOT_FOUND_BITCODE_PATH:
        return "not found firmware";
    case RETCODE_NOT_SUPPORTED_FEATURE:
        return "unsupport feature";
    case RETCODE_NOT_FOUND_VPU_DEVICE:
        return "not found VPU";
    case RETCODE_QUERY_FAILURE:
        return "query failed";
    case RETCODE_QUEUEING_FAILURE:
        return "queue buffer full";
    case RETCODE_VPU_STILL_RUNNING:
        return "VPU still running";
    case RETCODE_REPORT_NOT_READY:
        return "report not ready";
    case RETCODE_INVALID_SFS_INSTANCE:
        return "run sub-framesync failed";
    case RETCODE_ERROR_FW_FATAL:
        return "error FW";
    default:
        return "unkonwn error";
    }
}

int bmvpu_dec_get_core_idx(BMVidCodHandle handle)
{
    BMVidHandle vidHandle = (BMVidHandle)handle;
    DecHandle decHandle = vidHandle->codecInst;
    return decHandle->coreIdx;
}

void BMVidSetLogLevel()
{
    char *debug_env = getenv("BMVID_DEBUG");
//    char *profiling_env = getenv("BMIVA_PROFILING");
//    int profiling_on_ = 0;
    if (debug_env) {
        int log_level = 0;
        log_level = atoi(debug_env);
        if(log_level>=NONE && log_level<=MAX_LOG_LEVEL) {
            SetMaxLogLevel(log_level);
        }
    }
    /*
    profiling_on_ = 0;
    if (profiling_env) {
         profiling_on_ = atoi(profiling_env);
         init_plog(debug_on_);
    }
    */
}

void bmvpu_dec_set_logging_threshold(BmVpuDecLogLevel log_level)
{
   if(log_level>=BMVPU_DEC_LOG_LEVEL_NONE && log_level<=BMVPU_DEC_LOG_LEVEL_MAX_LOG_LEVEL) {
        SetMaxLogLevel(log_level);
   }
}

//dump compressed framebuffer for testing...
BOOL writeFilefromDram(
    int coreIdx,
    char* path,
    PhysicalAddress addr,
    int size,
    int endian,
    int print)
{
    osal_file_t   fp = NULL;
    unsigned char *buf;

    fp = osal_fopen(path, "wb");
    if( !fp ) {
        VLOG(ERR, "Can't open %s file\n", path);
        return FALSE;
    }

    if ( print )
        VLOG(INFO, "Save %s From 0x%x, size = 0x%08x(%d)\n", path, addr, size, size);

    buf = osal_malloc(size);
    if ( !buf ) {
        VLOG(ERR, "Fail malloc %d\n", size);
        return FALSE;
    }

    vdi_read_memory(coreIdx, addr, buf, size, endian);

    osal_fwrite((void *)buf, sizeof(Uint8), size, fp);
    osal_fflush(fp);

    osal_free(buf);
    osal_fclose(fp);

    return TRUE;
}

void dumpFbcOneFrame(
    DecHandle       handle,
    Uint32          frameIdx,
    DecOutputInfo*  outputInfo
    )
{
    DecInfo*    pDecInfo = NULL;
    Uint32      width=0;
    Uint32      height=0;
    Uint32      coreIdx = 0;
    PhysicalAddress addr;

    pDecInfo = &handle->CodecInfo->decInfo;
    coreIdx = VPU_HANDLE_CORE_INDEX(handle);

    if (outputInfo->indexFrameDisplayForTiled >= 0) {
        FrameBuffer     cfb;
        size_t          lumaTblSize, chromaTblSize;
        Uint32          frameIndex;
        char            srcPath[MAX_FILE_PATH];
        char            fileName[MAX_FILE_PATH];
        char            tmp[512];
        int             fbc_endian;
        size_t          fbcDataSizeY, fbcDataSizeC;
        VLOG(INFO, "---> DUMP COMPRESSED FRAMEBUFFER #%d TO below log\n", outputInfo->indexFrameDisplayForTiled);
        VPU_DecGetFrameBuffer(handle, outputInfo->indexFrameDisplayForTiled, &cfb);
        fbc_endian = 0; //cfb.endian;//vdi_convert_endian(handle->coreIdx, cfb.endian);

        /* Calculate FBC Data Size */
        //fbcDataSizeY  = CalcLumaSize(handle->productId, cfb.stride, outputInfo->dispPicHeight, FORMAT_420, 0, COMPRESSED_FRAME_MAP, NULL);
        //fbcDataSizeC  = CalcChromaSize(handle->productId, cfb.stride, outputInfo->dispPicHeight, FORMAT_420, 0, COMPRESSED_FRAME_MAP, NULL);
        fbcDataSizeY  = CalcLumaSize(handle->productId, cfb.stride, cfb.height, FORMAT_420, 0, COMPRESSED_FRAME_MAP, NULL);
        fbcDataSizeC  = CalcChromaSize(handle->productId, cfb.stride, cfb.height, FORMAT_420, 0, COMPRESSED_FRAME_MAP, NULL);
        fbcDataSizeC *= 2;
        VLOG(INFO, "displayPicHeight: %d,stride: %d, height: %d, width: %d, ytblSize: %d, ctblSize: %d\n",
        outputInfo->dispPicHeight, cfb.stride, cfb.height, cfb.width, pDecInfo->vbFbcYTbl[0].size, pDecInfo->vbFbcCTbl[0].size);
        /* Dump Y compressed data */
        sprintf(fileName, "./%dx%d_%04d_%d_fbc_data_y.bin", cfb.width, outputInfo->dispPicHeight, frameIdx, fbc_endian);
        writeFilefromDram(coreIdx, fileName,  cfb.bufY, fbcDataSizeY, fbc_endian, 1);//bigger than real Y size

        /* Dump C compressed data */
        VPU_GetFBCOffsetTableSize(STD_HEVC, (int)width, (int)height, (int*)&lumaTblSize, (int*)&chromaTblSize);
        frameIndex = outputInfo->indexFrameDisplayForTiled;
        sprintf(fileName, "./%dx%d_%04d_%d_fbc_table_y.bin", cfb.width, outputInfo->dispPicHeight, frameIdx, fbc_endian);
        addr = pDecInfo->vbFbcYTbl[frameIndex].phys_addr;
        writeFilefromDram(coreIdx, fileName,  addr, pDecInfo->vbFbcYTbl[0].size, fbc_endian, 1);

        /* Dump C Offset table */
        frameIndex = outputInfo->indexFrameDisplayForTiled;
        sprintf(fileName, "./%dx%d_%04d_%d_fbc_table_c.bin", cfb.width, outputInfo->dispPicHeight, frameIdx, fbc_endian);
        addr = pDecInfo->vbFbcCTbl[frameIndex].phys_addr;
        writeFilefromDram(coreIdx, fileName,  addr, pDecInfo->vbFbcCTbl[0].size, fbc_endian, 1);
        sprintf(srcPath,"%dx%d_%04d_%d", cfb.width, outputInfo->dispPicHeight, frameIdx, fbc_endian);

        sprintf(tmp,"./fbd -x %d -y %d -b %d -d %s_ -o %s_fbd.yuv",
            outputInfo->dispPicWidth, outputInfo->dispPicHeight, cfb.lumaBitDepth, srcPath, srcPath);
        VLOG(INFO, "%s\n", tmp);
    }
}

static void DisplayQueue_En(
    BMVidHandle vidHandle,
    DecOutputInfo *fbInfo, u64 pts, u64 dts)
{
    BMVidFrame displayInfo;
    int divX = 1;
    if (vidHandle == NULL)
    {
        VLOG(ERR, "%s:%d Invalid handle\n", __FUNCTION__, __LINE__);
        return;
    }
    osal_memset((void *)&displayInfo, 0x00, sizeof(BMVidFrame));

    displayInfo.picType = (BmVpuDecPicType)fbInfo->picType;
    displayInfo.width = fbInfo->rcDisplay.right - fbInfo->rcDisplay.left;
    displayInfo.height = fbInfo->rcDisplay.bottom - fbInfo->rcDisplay.top;
    displayInfo.coded_width = fbInfo->dispPicWidth;
    displayInfo.coded_height = fbInfo->dispPicHeight;
    displayInfo.stride[0] = fbInfo->dispFrame.stride;
    if (fbInfo->dispFrame.cbcrInterleave == 0)
    {
        divX = 2;
        displayInfo.stride[1] = fbInfo->dispFrame.stride / 2;
        displayInfo.stride[2] = fbInfo->dispFrame.stride / 2;
    }
    else
    {
        displayInfo.stride[1] = fbInfo->dispFrame.stride;
        displayInfo.stride[2] = 0;
    }

    if(fbInfo->decOutputExtData.userDataNum > 0 && (vidHandle->extraInfo.isFill == 0) ) {
        if(fbInfo->decOutputExtData.userDataHeader  & (1UL<<H264_USERDATA_FLAG_VUI)) {
            user_data_entry_t*  pEntry = NULL;
            Uint8*              pBase     = NULL;
            pBase  = (Uint8*)osal_malloc(vidHandle->vbUserData.size);
            osal_memset(pBase, 0, vidHandle->vbUserData.size);
            if (pBase == NULL) {
                VLOG(ERR, "%s failed to allocate memory\n", __FUNCTION__);
                return;
            }
            vdi_read_memory(vidHandle->codecInst->coreIdx, vidHandle->vbUserData.phys_addr, pBase, vidHandle->vbUserData.size, VPU_USER_DATA_ENDIAN);
            pEntry = (user_data_entry_t*)pBase;
            vidHandle->extraInfo.colorPrimaries = 2;
            vidHandle->extraInfo.colorTransferCharacteristic = 2;
            vidHandle->extraInfo.colorSpace = 2;
            if(vidHandle->decConfig.bitFormat == STD_HEVC) {
                h265_vui_param_t*  vui = (h265_vui_param_t*)(pBase + pEntry[H265_USERDATA_FLAG_VUI].offset);
                if (vui->colour_description_present_flag == 1) {
                    vidHandle->extraInfo.colorPrimaries = vui->colour_primaries;
                    vidHandle->extraInfo.colorSpace = vui->matrix_coefficients;
                    vidHandle->extraInfo.colorTransferCharacteristic = vui->transfer_characteristics;
                }
                if (vui->chroma_loc_info_present_flag == 1) {
                    vidHandle->extraInfo.chromaLocation = vui->chroma_sample_loc_type_top_field;
                }
                vidHandle->extraInfo.colorRange = vui->video_full_range_flag;
            }
            else if(vidHandle->decConfig.bitFormat == STD_AVC) {
                avc_vui_info_t*  vui = (avc_vui_info_t*)(pBase + pEntry[H265_USERDATA_FLAG_VUI].offset);
                if (vui->colour_description_present_flag == 1) {
                    vidHandle->extraInfo.colorPrimaries = vui->colour_primaries;
                    vidHandle->extraInfo.colorSpace = vui->matrix_coefficients;
                    vidHandle->extraInfo.colorTransferCharacteristic = vui->transfer_characteristics;
                }
                if (vui->chroma_loc_info_present_flag == 1) {
                    vidHandle->extraInfo.chromaLocation = vui->chroma_sample_loc_type_top_field;
                }
                vidHandle->extraInfo.colorRange = vui->video_full_range_flag;
            }
            vidHandle->extraInfo.isFill = 1;
            if(pBase!=NULL)
                osal_free(pBase);
        }
    }

    if(vidHandle->extraInfo.isFill != 0) {
        displayInfo.colorPrimaries = vidHandle->extraInfo.colorPrimaries;
        displayInfo.chromaLocation = vidHandle->extraInfo.chromaLocation;
        displayInfo.colorRange = vidHandle->extraInfo.colorRange + 1;//for compatible ffmpeg
        displayInfo.colorSpace = vidHandle->extraInfo.colorSpace;
        displayInfo.colorTransferCharacteristic = vidHandle->extraInfo.colorTransferCharacteristic;
        //VLOG(INFO, "corspace: %d, colorPrimaries: %d, chromaLocation: %d, colorRange: %d, colorTransferCharacteristic: %d\n",
        //            displayInfo.colorSpace, displayInfo.colorPrimaries, displayInfo.chromaLocation, displayInfo.colorRange, displayInfo.colorTransferCharacteristic);
    }

    if(vidHandle->codecInst->CodecInfo->decInfo.mapType && (vidHandle->codecInst->CodecInfo->decInfo.wtlEnable==FALSE))
    {
        //displayInfo.width = fbInfo->dispFrame.width;
        //displayInfo.height = fbInfo->dispFrame.height;
        if(vidHandle->codecInst->CodecInfo->decInfo.mapType == TILED_FRAME_V_MAP) {
            displayInfo.buf[4] = (unsigned char*)GetXY2AXIAddr(&(vidHandle->codecInst->CodecInfo->decInfo.mapCfg), 0, fbInfo->rcDisplay.top, fbInfo->rcDisplay.left, fbInfo->dispFrame.stride, &(fbInfo->dispFrame));
            if(fbInfo->dispFrame.cbcrInterleave) {
                displayInfo.buf[5] = displayInfo.buf[4] + CalcLumaSize(vidHandle->codecInst->productId, fbInfo->dispFrame.stride, fbInfo->dispFrame.height, fbInfo->dispFrame.format, fbInfo->dispFrame.cbcrInterleave,
                vidHandle->codecInst->CodecInfo->decInfo.mapType, &(vidHandle->codecInst->CodecInfo->decInfo.dramCfg));
                //displayInfo.buf[5] = (unsigned char *)GetXY2AXIAddr(&(vidHandle->codecInst->CodecInfo->decInfo.mapCfg), 2, fbInfo->rcDisplay.top/2, fbInfo->rcDisplay.left, fbInfo->dispFrame.stride, &(fbInfo->dispFrame));
            }
            else {
                VLOG(ERR, "please check the maptype of cbcrinterleave, the map type is : %d, cbcrinterleave: %d\n", vidHandle->codecInst->CodecInfo->decInfo.mapType, fbInfo->dispFrame.cbcrInterleave);
            }
        }
        else if(vidHandle->codecInst->CodecInfo->decInfo.mapType == COMPRESSED_FRAME_MAP) {
            int  lumaTblSize, chromaTblSize;
            VPU_GetFBCOffsetTableSize(vidHandle->decConfig.bitFormat, (int)displayInfo.width, (int)displayInfo.height, &lumaTblSize, &chromaTblSize);
            displayInfo.buf[4] = (unsigned char *)(fbInfo->dispFrame.bufY);
            displayInfo.buf[5] = (unsigned char *)(fbInfo->dispFrame.bufCb);
            displayInfo.buf[6] = (unsigned char *)(vidHandle->codecInst->CodecInfo->decInfo.vbFbcYTbl[fbInfo->dispFrame.myIndex].phys_addr);
            displayInfo.buf[7] = (unsigned char *)(vidHandle->codecInst->CodecInfo->decInfo.vbFbcCTbl[fbInfo->dispFrame.myIndex].phys_addr);
            displayInfo.stride[1] = displayInfo.stride[0];
            displayInfo.stride[2] = lumaTblSize;
            displayInfo.stride[6] = lumaTblSize;
            displayInfo.stride[3] = chromaTblSize;
            displayInfo.stride[7] = chromaTblSize;
            displayInfo.buf[3] = (Uint8 *)(vdi_get_virt_addr(vidHandle->codecInst->coreIdx, (u64)displayInfo.buf[7]));

        }
        else {
            VLOG(ERR, "please check the maptype of framebuffer, the map type is : %d\n", vidHandle->codecInst->CodecInfo->decInfo.mapType);
        }
        displayInfo.pixel_format = BM_VPU_DEC_PIX_FORMAT_COMPRESSED;
        //VLOG(INFO, "pY: 0x%lx, pCb: 0x%lx, pCr: 0x%lx, stride: %d\n", displayInfo.buf[4], displayInfo.buf[5], displayInfo.buf[6], fbInfo->dispFrame.stride);
    }
    else {
        //displayInfo.width = fbInfo->rcDisplay.right - fbInfo->rcDisplay.left;
        //displayInfo.height = fbInfo->rcDisplay.bottom - fbInfo->rcDisplay.top;
        displayInfo.buf[4] = (unsigned char *)(fbInfo->dispFrame.bufY  + fbInfo->rcDisplay.top * fbInfo->dispFrame.stride + fbInfo->rcDisplay.left);
        displayInfo.buf[5] = (unsigned char *)(fbInfo->dispFrame.bufCb + fbInfo->rcDisplay.top * displayInfo.stride[1]/2 + fbInfo->rcDisplay.left / divX);
        displayInfo.buf[6] = (unsigned char *)(fbInfo->dispFrame.bufCr + fbInfo->rcDisplay.top * displayInfo.stride[2]/2 + fbInfo->rcDisplay.left / divX);
        if(vidHandle->enable_cache == 1)
        {
            int fbIndex = fbInfo->dispFrame.myIndex;
            if(vidHandle->codecInst->CodecInfo->decInfo.wtlEnable == TRUE)
                fbIndex = VPU_CONVERT_WTL_INDEX(vidHandle->codecInst, fbIndex);
            //VLOG(INFO, "fbIndex: %d, frameIndex: %d\n, addr: 0x%lx, addr y: 0x%lx, flag: %d\n", fbIndex, fbInfo->dispFrame.myIndex, vidHandle->pFbMem[fbIndex].phys_addr, fbInfo->dispFrame.bufY, vidHandle->pFbMem[fbIndex].enable_cache);
#ifndef BM_PCIE_MODE
            vdi_invalidate_memory(vidHandle->codecInst->coreIdx, &vidHandle->pFbMem[fbIndex]);
#endif
        }
        if (fbInfo->dispFrame.cbcrInterleave) {
            if (fbInfo->dispFrame.nv21)
                displayInfo.pixel_format = BM_VPU_DEC_PIX_FORMAT_NV21;
            else
                displayInfo.pixel_format = BM_VPU_DEC_PIX_FORMAT_NV12;
        } else {
            displayInfo.pixel_format = BM_VPU_DEC_PIX_FORMAT_YUV420P;
        }
    }
#ifndef BM_PCIE_MODE
    displayInfo.buf[0] = (Uint8 *)(vdi_get_virt_addr(vidHandle->codecInst->coreIdx, (u64)displayInfo.buf[4]));
    displayInfo.buf[1] = (Uint8 *)(vdi_get_virt_addr(vidHandle->codecInst->coreIdx, (u64)displayInfo.buf[5]));
    displayInfo.buf[2] = (Uint8 *)(vdi_get_virt_addr(vidHandle->codecInst->coreIdx, (u64)displayInfo.buf[6]));
#else
    //use the fake virtual memory
    displayInfo.buf[0] = (Uint8 *)0xdeadbeef;
    displayInfo.buf[1] = (Uint8 *)0xdeadbeef;
    displayInfo.buf[2] = (Uint8 *)0xdeadbeef;
#endif

    displayInfo.lumaBitDepth = fbInfo->dispFrame.lumaBitDepth;
    displayInfo.chromaBitDepth = fbInfo->dispFrame.chromaBitDepth;
    //osal_memcpy((void*)&(displayInfo.outputInfo), fbInfo, sizeof(DecOutputInfo));
    displayInfo.frameIdx = fbInfo->dispFrame.myIndex;
    displayInfo.interlacedFrame = (BmVpuDecLaceFrame)fbInfo->interlacedFrame;

    displayInfo.stride[4] = displayInfo.stride[0];
    displayInfo.stride[5] = displayInfo.stride[1];
    if (fbInfo->dispFrame.cbcrInterleave == 0)
        displayInfo.stride[6] = displayInfo.stride[2];
    displayInfo.sequenceNo = fbInfo->sequenceNo;
    displayInfo.pts = pts;
    displayInfo.dts = dts;
    displayInfo.size = fbInfo->dispFrame.size;

    //char *debug_env = getenv("BMVID_DEBUG_DUMP_FRAME_NUM");
#ifndef BM_PCIE_MODE
    int dump_total_num = VPU_GetOutNum(vidHandle->codecInst->coreIdx, vidHandle->codecInst->instIndex);

    if (dump_total_num > 0) {
        if(dump_frame_num/10<dump_total_num) {
            if(dump_frame_num%10 == 0) {
                if(vidHandle->codecInst->CodecInfo->decInfo.mapType && (vidHandle->codecInst->CodecInfo->decInfo.wtlEnable==FALSE))
                {
                    //dump compress buffer
                    if(vidHandle->codecInst->CodecInfo->decInfo.mapType == COMPRESSED_FRAME_MAP) {
                       dumpFbcOneFrame(vidHandle->codecInst, dump_frame_num, fbInfo);
                    }
                }
                else {
                    char filename[255];
                    FILE *fp = NULL;
                    if(fbInfo->dispFrame.cbcrInterleave) {
                        sprintf(filename, "/data/frame%d_core%d_inst%d_%dx%d_nv12.yuv", dump_frame_num, vidHandle->codecInst->coreIdx, vidHandle->codecInst->instIndex, displayInfo.stride[0], displayInfo.height);
                    }
                    else {
                        sprintf(filename, "/data/frame%d_core%d_inst%d_%dx%d.yuv", dump_frame_num, vidHandle->codecInst->coreIdx, vidHandle->codecInst->instIndex, displayInfo.stride[0], displayInfo.height);
                    }
                    fp = fopen(filename, "wb+");
                    if(fp != NULL) {
                        fwrite(displayInfo.buf[0], 1, displayInfo.stride[0] * displayInfo.height, fp);
                        fwrite(displayInfo.buf[1], 1, displayInfo.stride[1] * displayInfo.height/2, fp);
                        if(fbInfo->dispFrame.cbcrInterleave==0)
                            fwrite(displayInfo.buf[2], 1, displayInfo.stride[2] * displayInfo.height/2, fp);
                        fclose(fp);
                    }
                }
            }
            dump_frame_num++;
        }
    }
#endif
    Queue_Enqueue(vidHandle->displayQ, (void *)&displayInfo);
}

#define MIN_WAITING_QUEUE_LEN 4

void reset_rd_ptr(BMVidHandle vidCodHandle, PhysicalAddress *p_last_rd)
{
    DecHandle handle = vidCodHandle->codecInst;
    PhysicalAddress rd_ptr = 0;
    int *pktsize = NULL;
    int end_flag = 0;
    int update_size = vidCodHandle->remainedSize;
    while((pktsize = Queue_Dequeue(vidCodHandle->inputQ2))!=NULL)
    {
        if(*pktsize == 0)
            end_flag = 1;
        update_size += *pktsize;
    }
    if(end_flag == 1) {
        if(update_size>0)
            VPU_DecUpdateBitstreamBuffer(handle, update_size);
        VPU_DecUpdateBitstreamBuffer(handle, 0);
        vidCodHandle->remainedSize = 0;
    }
    else {
        vidCodHandle->remainedSize = update_size & STREAM_ALIGNED_LEN;
        update_size -= vidCodHandle->remainedSize;
        if(update_size>0)
            VPU_DecUpdateBitstreamBuffer(handle, update_size);
    }
    if(Queue_Get_Cnt(vidCodHandle->inputQ)>1)
    {
        PkgInfo *pkginfo = Queue_Dequeue(vidCodHandle->inputQ);
        pkginfo = Queue_Peek(vidCodHandle->inputQ);
        rd_ptr = pkginfo->rd;

    }
    else
    {
        if(handle->CodecInfo->decInfo.streamBufStartAddr == handle->CodecInfo->decInfo.streamWrPtr)
            rd_ptr = handle->CodecInfo->decInfo.streamBufStartAddr;
        else
            rd_ptr = handle->CodecInfo->decInfo.streamWrPtr-1;
    }

    //VPU_DecSetRdPtr(handle, rd_ptr, FALSE);//set to next pkg.
    handle->CodecInfo->decInfo.streamRdPtr = rd_ptr;
    if(p_last_rd != NULL)
        *p_last_rd = rd_ptr;
}

void update_bs_buffer(BMVidHandle vidCodHandle, PhysicalAddress *p_last_rd)
{
    PhysicalAddress rdPtr, wrPtr;
    Uint32 size;
    DecHandle handle = vidCodHandle->codecInst;
    int *pktsize = NULL;
    PhysicalAddress lastRd = *p_last_rd;

    VPU_DecGetBitstreamBuffer(handle, &rdPtr, &wrPtr, &size);

    //processing input queue.
    PkgInfo *pkginfo;
    while((pkginfo = Queue_Peek(vidCodHandle->inputQ))!=NULL) {
        if(pkginfo->rd != lastRd) {
            VLOG(ERR, "this is a bug. didn't sync the input buffer!, lastRd:0x%lx, pkgrd:0x%lx\n",
                lastRd, pkginfo->rd);
            lastRd = pkginfo->rd;
        }

        if(pkginfo->flag != 0) {
            if(rdPtr < lastRd && handle->CodecInfo->decInfo.streamBufSize + rdPtr - lastRd >= pkginfo->len) {
                lastRd = pkginfo->rd + pkginfo->len - handle->CodecInfo->decInfo.streamBufSize;
                Queue_Dequeue(vidCodHandle->inputQ);
            }
            else {
                break;
            }
        }
        else {
            if(rdPtr - lastRd>=pkginfo->len) {
                lastRd = pkginfo->rd + pkginfo->len;
                Queue_Dequeue(vidCodHandle->inputQ);
            }
            else {
                break;
            }
        }
    }
    int end_flag = 0;
    int update_size = vidCodHandle->remainedSize;
    while((pktsize = Queue_Dequeue(vidCodHandle->inputQ2))!=NULL)
    {
        if(*pktsize == 0)
            end_flag = 1;
        update_size += *pktsize;
    }
    if(end_flag == 1) {
        if(update_size>0)
            VPU_DecUpdateBitstreamBuffer(handle, update_size);
        VPU_DecUpdateBitstreamBuffer(handle, 0);
        vidCodHandle->remainedSize = 0;
    }
    else {
        vidCodHandle->remainedSize = update_size & STREAM_ALIGNED_LEN;
        update_size -= vidCodHandle->remainedSize;
#ifndef BM_PCIE_MODE
        if(update_size>0)
#else
        if(update_size>512)
#endif
            VPU_DecUpdateBitstreamBuffer(handle, update_size);
    }
    *p_last_rd = lastRd;
}
#include <errno.h>
#define TRY_OPEN_SEM_TIMEOUT 20
#ifdef TRY_FLOCK_OPEN
void get_lock_timeout(int sec,int pcie_board_idx)
{
    char flock_name[255];
#ifdef __linux__
    if(s_vid_open_flock_fd[pcie_board_idx] == -1) {
        sprintf(flock_name, "%s%d", VID_OPEN_FLOCK_NAME, pcie_board_idx);
        VLOG(INFO, "flock_name : .. %s\n", flock_name);
        umask(0000);
        if (access(flock_name, F_OK) != 0) {
            s_vid_open_flock_fd[pcie_board_idx] = open(flock_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
        }else {
            s_vid_open_flock_fd[pcie_board_idx] = open(flock_name, O_WRONLY );
        }
    }
    flock(s_vid_open_flock_fd[pcie_board_idx], LOCK_EX);
#elif _WIN32
    if (s_vid_open_flock_fd[pcie_board_idx] == NULL) {
        sprintf(flock_name, "%s%d", VID_OPEN_FLOCK_NAME, pcie_board_idx);
        VLOG(INFO, "flock_name : .. %s\n", flock_name);
        umask(0000);
        s_vid_open_flock_fd[pcie_board_idx] = CreateMutex(NULL, FALSE, flock_name);
    }
    WaitForSingleObject(s_vid_open_flock_fd[pcie_board_idx], INFINITE);
#endif
}
void unlock_flock(int pcie_board_idx)
{
#ifdef __linux__
    if(s_vid_open_flock_fd[pcie_board_idx] == -1) {
#elif _WIN32
    if (s_vid_open_flock_fd[pcie_board_idx] == NULL) {
#endif
        printf("vid unlock flock failed....\n");
     }
     else {
#ifdef __linux__
        flock(s_vid_open_flock_fd[pcie_board_idx], LOCK_UN);
        close(s_vid_open_flock_fd[pcie_board_idx]);
        s_vid_open_flock_fd[pcie_board_idx] = -1;
#elif _WIN32
        ReleaseMutex(s_vid_open_flock_fd[pcie_board_idx]);
        CloseHandle(s_vid_open_flock_fd[pcie_board_idx]);
        s_vid_open_flock_fd[pcie_board_idx] = NULL;
#endif


     }
}
static void get_reset_flock(int pcie_board_idx, int core_idx)
{
    char flock_name[255];
#ifdef __linux__
    if(s_vid_reset_flock_fd[pcie_board_idx][core_idx] == -1) {
        sprintf(flock_name, VID_RESET_FLOCK_NAME, pcie_board_idx, core_idx);
        //VLOG(INFO, "reset flock_name : .. %s\n", flock_name);
        umask(0000);
        if (access(flock_name, F_OK) != 0) {
            s_vid_reset_flock_fd[pcie_board_idx][core_idx] = open(flock_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
        } else {
            s_vid_reset_flock_fd[pcie_board_idx][core_idx] = open(flock_name, O_WRONLY);
        }
    }
    flock(s_vid_reset_flock_fd[pcie_board_idx][core_idx], LOCK_EX);
#elif _WIN32
    if (s_vid_reset_flock_fd[pcie_board_idx][core_idx] == NULL) {

        sprintf(flock_name, VID_RESET_FLOCK_NAME, pcie_board_idx, core_idx);
        //VLOG(INFO, "reset flock_name : .. %s\n", flock_name);
        umask(0000);
        s_vid_reset_flock_fd[pcie_board_idx][core_idx] = CreateMutex(NULL, FALSE, flock_name);
	}
    WaitForSingleObject(s_vid_reset_flock_fd[pcie_board_idx][core_idx], INFINITE);
#endif
}
static void unlock_reset_flock(int pcie_board_idx, int core_idx)
{
#ifdef __linux__
    if(s_vid_reset_flock_fd[pcie_board_idx][core_idx] == -1) {
#elif _WIN32
    if (s_vid_reset_flock_fd[pcie_board_idx][core_idx] == NULL) {
#endif
         printf("vid reset unlock flock failed....\n");
     }
     else {
#ifdef __linux__
        flock(s_vid_reset_flock_fd[pcie_board_idx][core_idx], LOCK_UN);
        close(s_vid_reset_flock_fd[pcie_board_idx][core_idx]);
        s_vid_reset_flock_fd[pcie_board_idx][core_idx] = -1;
#elif _WIN32
        //flock(s_vid_reset_flock_fd[pcie_board_idx][core_idx], LOCK_UN); syj add
        ReleaseMutex(s_vid_reset_flock_fd[pcie_board_idx][core_idx]);
        CloseHandle(s_vid_reset_flock_fd[pcie_board_idx][core_idx]);
        s_vid_reset_flock_fd[pcie_board_idx][core_idx] = NULL;
#endif
     }
}
#else
static void get_lock_timeout(sem_t *sem, int sec)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        VLOG(INFO, "failed clock_gettime");
        sem_wait(sem);
    }
    else {
        int s = 0;
        ts.tv_sec += sec;
        while ((s = sem_timedwait(sem, &ts)) == -1 && errno == EINTR)
            continue;       /* Restart if interrupted by handler */

        if (s == -1) {
            if (errno == ETIMEDOUT)
                printf("sem_timedwait() timed out\n");
            else
                perror("sem_timedwait");
        }
    }
}
#endif

unsigned int bit_one_count(unsigned int n)
{
    unsigned int count = 0;

    while(0 < n)
    {
        unsigned int m = n - 1;
        n &= m;
        count++;
    }

    return count;
}

/**
 * @brief This function process message from VPU.
 * @param arg [Input]
 * @return non
 */

static void process_vpu_msg(void *arg)
{
    BMVidHandle vidCodHandle = (BMVidHandle)arg;
    RetCode ret = RETCODE_FAILURE;
    Int32 timeoutCount = 0;
    Int32 interruptFlag = 0;
    Int32 instIdx = 0, coreIdx;
    Int32 pcie_board_idx = 0;
    DecHandle handle;
    BOOL seqInited = FALSE;
    BOOL assertedFieldDoneInt = FALSE;
//    BOOL enablePPU = FALSE;
    BOOL repeat = TRUE;
    //    BOOL                needStream              = FALSE;
    BOOL seqChangeRequest = FALSE;
    PhysicalAddress seqChangedRdPtr = 0;
    PhysicalAddress seqChangedWrPtr = 0;
    Int32 seqChangedStreamEndFlag = 0;
    BMVidDecConfig *param;
    DecParam decParam;
    DecOutputInfo outputInfo;
//    FrameBuffer *pFrameBuffer;
    Int32 decodedIdx = 0;
    Int32 prevFbIndex = -1;
    BOOL waitPostProcessing = FALSE;
    FrameBuffer *ppuFb = NULL;
    Queue *ppuQ=NULL;
    Uint32 dispIdx = 0;
    PhysicalAddress lastRd = 0;
    int index = 0;
    VLOG(INFO, "Enter process message from VPU!\n");
    if (vidCodHandle == NULL || vidCodHandle->codecInst == NULL)
        return;
    handle = vidCodHandle->codecInst;
    lastRd = handle->CodecInfo->decInfo.streamRdPtr;
    param = &(vidCodHandle->decConfig);
    coreIdx = handle->coreIdx;

    pcie_board_idx = coreIdx/MAX_NUM_VPU_CORE_CHIP;

    osal_memset(&decParam, 0x00, sizeof(DecParam));
    osal_memset(&outputInfo, 0x00, sizeof(DecOutputInfo));
    VLOG(INFO, "Start process message from VPU, pid : %d!\n", getpid());
    Uint32 preDispIdx = 0xffffff;
    //Uint32 preDecIdx = 0xfffff;
    BOOL restart = FALSE;
    BOOL skipWaiting = FALSE;
    int bitstreamMode = handle->CodecInfo->decInfo.openParam.bitstreamMode;
    u64 *pts, *dts, *pts_org=NULL, *dts_org=NULL;
    u64 pts_tmp, dts_tmp;
    VpuRect rcPpu;
    BOOL updateStreamBufferFlag = TRUE;
    while (vidCodHandle->endof_flag < BMDEC_START_CLOSE)
    {
            int *pktsize = NULL;
            pts_tmp = -1L;
            dts_tmp = -1L;
            if(vidCodHandle->endof_flag == BMDEC_START_DECODING && vidCodHandle->decStatus == BMDEC_STOP) {
                vidCodHandle->decStatus = BMDEC_DECODING;
            }
            osal_cond_lock(vidCodHandle->inputCond);
            if(skipWaiting == FALSE && seqChangeRequest == FALSE && updateStreamBufferFlag == TRUE) {
                while(((Queue_Get_Cnt(vidCodHandle->inputQ)<MIN_WAITING_QUEUE_LEN && bitstreamMode != BS_MODE_PIC_END) || (Queue_Get_Cnt(vidCodHandle->inputQ)<1 && bitstreamMode == BS_MODE_PIC_END)) && vidCodHandle->endof_flag <= BMDEC_START_DECODING)
                {
    //                VLOG(INFO, "Waiting input, input queue len: %d\n", Queue_Get_Cnt(vidCodHandle->inputQ));
                    osal_cond_wait(vidCodHandle->inputCond);
                }
                if(vidCodHandle->endof_flag == BMDEC_START_CLOSE)
                    break;
                int update_size = vidCodHandle->remainedSize;
                while((pktsize = Queue_Peek(vidCodHandle->inputQ2))!=NULL && *pktsize>0)
                {
                    update_size += *pktsize;
                    Queue_Dequeue(vidCodHandle->inputQ2);
                    //if(bitstreamMode != BS_MODE_PIC_END)
                    //    VPU_DecUpdateBitstreamBufferParam(handle, *pktsize);
                }
                if(bitstreamMode != BS_MODE_PIC_END) {
                    vidCodHandle->remainedSize = update_size & STREAM_ALIGNED_LEN;
                    update_size -= vidCodHandle->remainedSize;
                    if(update_size>0)
                        VPU_DecUpdateBitstreamBufferParam(handle, update_size);
                }
                if(bitstreamMode == BS_MODE_PIC_END && vidCodHandle->decStatus <= BMDEC_DECODING) {
                    PkgInfo *pkginfo = Queue_Dequeue(vidCodHandle->inputQ);
                    //update rd pointer
                    if(pkginfo != NULL) {
                        pts_tmp = pkginfo->pts;
                        dts_tmp = pkginfo->dts;
                        handle->CodecInfo->decInfo.streamRdPtr = pkginfo->rd;
                        handle->CodecInfo->decInfo.streamWrPtr = pkginfo->rd + pkginfo->len;
                        if(handle->CodecInfo->decInfo.streamBufEndAddr < handle->CodecInfo->decInfo.streamWrPtr) {
                            handle->CodecInfo->decInfo.streamWrPtr = handle->CodecInfo->decInfo.streamBufEndAddr;
                            VLOG(ERR, "may be wr error: 0x%lx\n", handle->CodecInfo->decInfo.streamWrPtr);
                        }
                        if(pkginfo->len==0)
                            handle->CodecInfo->decInfo.streamEndflag |= (1<<2);
                        //VLOG(INFO, "dec pkginfo rd: 0x%lx, len: %d\n", pkginfo->rd, pkginfo->len);
                    }
                    else {
                        if(vidCodHandle->seqInitFlag == 0) {
                            vidCodHandle->decStatus = BMDEC_STOP;
                            break;
                        }
                        handle->CodecInfo->decInfo.streamEndflag |= (1<<2);
                        handle->CodecInfo->decInfo.streamRdPtr = handle->CodecInfo->decInfo.streamWrPtr;
                        VLOG(ERR, "status error, may be endof.. coreIdx: %d, instIdx: %d\n", coreIdx, instIdx);
                    }
                }
            }
            else {
                if(updateStreamBufferFlag == FALSE) {
                    updateStreamBufferFlag = TRUE;
                }
            }
            osal_cond_unlock(vidCodHandle->inputCond);

            osal_cond_lock(vidCodHandle->outputCond);
            if(vidCodHandle->enablePPU == TRUE) {
                if(vidCodHandle->frameInBuffer > 0 && (vidCodHandle->frameInBuffer>=vidCodHandle->extraFrameBufferNum || Queue_Get_Cnt(ppuQ)==0) && vidCodHandle->endof_flag <= BMDEC_START_CLOSE) {
    //                VLOG(INFO, "Waiting output buffer, frameInbuffer: %d, numFbsForDecoding: %d\n", vidCodHandle->frameInBuffer, handle->CodecInfo->decInfo.numFbsForDecoding);
                    osal_cond_wait(vidCodHandle->outputCond);
                }
            }
            else {
                if(vidCodHandle->frameInBuffer > 0 && vidCodHandle->frameInBuffer>=vidCodHandle->extraFrameBufferNum && vidCodHandle->endof_flag <= BMDEC_START_ENDOF) {
    //                VLOG(INFO, "Waiting output buffer, frameInbuffer: %d, numFbsForDecoding: %d\n", vidCodHandle->frameInBuffer, handle->CodecInfo->decInfo.numFbsForDecoding);
                    osal_cond_wait(vidCodHandle->outputCond);
                }
            }
            osal_cond_unlock(vidCodHandle->outputCond);
            if(vidCodHandle->endof_flag == BMDEC_START_CLOSE)
                break;
//            VLOG(INFO, "start dec\n");
            //            VLOG(INFO, "isStreamBufFilled\n");
            if (vidCodHandle->seqInitFlag == 0 && vidCodHandle->endof_flag < BMDEC_START_CLOSE)
            {
                VLOG(INFO, "seqInitFlag\n");
                timeoutCount = 0;
                while ((ret = VPU_DecIssueSeqInit(handle)) != RETCODE_SUCCESS)
                {
                    VLOG(ERR, "VPU_DecIssueSeqInit failed Error code is 0x%x \n", ret);
#ifdef __linux__
                    usleep(20*1000);
#elif _WIN32
                    Sleep(20);
#endif
                    timeoutCount += 1;
                    if(timeoutCount * 20 > VPU_DEC_TIMEOUT)
                        break;
                }
                VLOG(INFO, "seqInited\n");
                timeoutCount = 0;

                while (seqInited == FALSE/* && vidCodHandle->endof_flag < BMDEC_START_CLOSE*/)
                {
                    interruptFlag = VPU_WaitInterruptEx(handle, VPU_WAIT_TIME_OUT); //wait for 10ms to save stream filling time.
                    if (interruptFlag == -1)
                    {
                        if (timeoutCount * VPU_WAIT_TIME_OUT > VPU_DEC_TIMEOUT)
                        {
                            Uint32 i;
                            VLOG(ERR, "\n VPU interrupt wait timeout\n");
                            PrintDecVpuStatus(handle);
                            for (i=0; i<100; i++) VLOG(ERR, "Current BPU PC: %08x\n", VpuReadReg(coreIdx, 0x18/*PC*/));
                            //reset_rd_ptr(vidCodHandle, &lastRd);
                            VPU_SWReset(coreIdx, SW_RESET_SAFETY, handle);
                            restart = TRUE;
                            break;
                        }
                        timeoutCount++;
                        interruptFlag = 0;
                    }

                    if (interruptFlag > 0)
                    {
                        VPU_ClearInterrupt(coreIdx);
                        if (interruptFlag & (1 << INT_BIT_SEQ_INIT))
                        {
                            seqInited = TRUE;
                            break;
                        }
//note:sometimes the interruptFlag is the last instance's value!
#ifdef BM_PCIE_MODE
                        else
                        {
                            continue;
                        }
#endif
                        //reset_rd_ptr(vidCodHandle, &lastRd);
                        //break;
                        if(bitstreamMode != BS_MODE_PIC_END)
                        {
                            osal_cond_lock(vidCodHandle->inputCond);
                            update_bs_buffer(vidCodHandle, &lastRd);
                            while(Queue_Get_Cnt(vidCodHandle->inputQ)<MIN_WAITING_QUEUE_LEN && vidCodHandle->endof_flag<BMDEC_START_GET_ALLFRAME)
                            {
        //                        VLOG(INFO, "INT_BIT_BIT_BUF_EMPTY, waiting input...\n");
                                osal_cond_wait(vidCodHandle->inputCond);
                                //skipWaiting = TRUE;
                                //VPU_DecUpdateBitstreamBuffer(handle, 1);                   // let f/w to know stream end condition in bitstream buffer. force to know that bitstream buffer will be empty.
                                //VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG); // set to stream end condition to pump out a delayed framebuffer.
                            }
                            update_bs_buffer(vidCodHandle, &lastRd);
                            osal_cond_unlock(vidCodHandle->inputCond);
                        }
                        else
                        {
                            VLOG(INFO, "please check maybe exception in get seq info.....flag: %d\n", interruptFlag);
//                            seqInited = TRUE;
                        }
                        //if(vidCodHandle->endof_flag>=BMDEC_START_GET_ALLFRAME)
                        //    break;
                    }


                }
                if(restart)
                {
                    restart = FALSE;
                    continue;
                }
                if(seqInited==FALSE)
                {
//                    VPU_SWReset(coreIdx, SW_RESET_SAFETY, handle);
                    VLOG(ERR, "some error in find seq header, re try....\n");
                    continue;
                }

                if ((ret = bmvpu_dec_seq_init(vidCodHandle)) != RETCODE_SUCCESS)
                {
                    VLOG(ERR, "frame buffer allocation failed after sequence init, error = 0x%x\n", ret);
                    seqInited = FALSE;
                    if(ret < 0) {
                        vidCodHandle->decStatus = BMDEC_HUNG;
                        break;
                    }
                    continue;
                }

                vidCodHandle->seqInitFlag = 1;
                vidCodHandle->enablePPU = (BOOL)(param->coda9.rotate > 0 || param->coda9.mirror > 0 ||
                                   handle->CodecInfo->decInfo.openParam.tiled2LinearEnable == TRUE || param->coda9.enableDering == TRUE);
                waitPostProcessing = vidCodHandle->enablePPU;
                ppuQ = vidCodHandle->ppuQ;
                vidCodHandle->decStatus = BMDEC_DECODING;
                pts_org = osal_malloc(handle->CodecInfo->decInfo.numFrameBuffers*8 + 8);
                dts_org = osal_malloc(handle->CodecInfo->decInfo.numFrameBuffers*8 + 8);
                pts = (u64 *)((u64)pts_org & (~7L));
                dts = (u64 *)((u64)dts_org & (~7L));
                VLOG(INFO, "pts_org:%p, pts:%p, dts_org:%p, dts:%p\n", pts_org, pts, dts_org, dts);
                if(!pts || !dts) {
                    VLOG(ERR, "can alloc memory for pts and dts\n");
                    vidCodHandle->decStatus = BMDEC_HUNG;
                    break;
                }
                if(bitstreamMode==BS_MODE_PIC_END && handle->CodecInfo->decInfo.openParam.bitstreamFormat == STD_RV) {
                    osal_cond_lock(vidCodHandle->inputCond);
                    PkgInfo *pkginfo = Queue_Dequeue(vidCodHandle->inputQ);
                    osal_cond_unlock(vidCodHandle->inputCond);
                    if(pkginfo!=NULL) {
                        handle->CodecInfo->decInfo.streamRdPtr = pkginfo->rd;
                        handle->CodecInfo->decInfo.streamWrPtr = pkginfo->rd + pkginfo->len;
                    }
                    else {
                        continue;
                    }
                }
            }

            if(vidCodHandle->decStatus != BMDEC_DECODING)
            {
                osal_msleep(1);
                continue;
            }

            if (assertedFieldDoneInt == TRUE)
            {
                VPU_ClearInterrupt(coreIdx);
                assertedFieldDoneInt = FALSE;
            }
            else
            {
                // When the field done interrupt is asserted, just fill elementary stream into the bitstream buffer.
                if (vidCodHandle->enablePPU == TRUE)
                {
                    if ((ppuFb = (FrameBuffer *)Queue_Dequeue(ppuQ)) == NULL)
                    {
                        //                    MSleep(0);
                        //                        needStream = FALSE;
                        continue;
                    }
                    VPU_DecGiveCommand(handle, SET_ROTATOR_OUTPUT, (void *)ppuFb);
                    if (param->coda9.rotate > 0)
                    {
                        VPU_DecGiveCommand(handle, ENABLE_ROTATION, NULL);
                    }
                    if (param->coda9.mirror > 0)
                    {
                        VPU_DecGiveCommand(handle, ENABLE_MIRRORING, NULL);
                    }
                    if (param->coda9.enableDering == TRUE)
                    {
                        VPU_DecGiveCommand(handle, ENABLE_DERING, NULL);
                    }
                }
                timeoutCount = 0;
                // Start decoding a frame.
                while ((ret = VPU_DecStartOneFrame(handle, &decParam)) != RETCODE_SUCCESS)
                {
                    VLOG(ERR, "VPU_DecStartOneFrame failed Error code is 0x%x \n", ret);
#ifdef __linux__
                    usleep(20*1000);
#elif _WIN32
                    Sleep(20);
#endif
                    timeoutCount += 1;
                    if(timeoutCount * 20 > VPU_DEC_TIMEOUT)
                        break;
                }
            }

            timeoutCount = 0;
            repeat = TRUE;
            restart = FALSE;
            instIdx = handle->instIndex;
            while (repeat == TRUE)
            {
//                VLOG(INFO, "Waiting interrupt\n");
                if ((interruptFlag = VPU_WaitInterruptEx(handle, VPU_WAIT_TIME_OUT)) == -1)
                {
                    //wait for 10ms to save stream filling time.
                    if (timeoutCount * VPU_WAIT_TIME_OUT > VPU_DEC_TIMEOUT)
                    {
                        PrintVpuStatus(coreIdx, handle->productId);
                        VLOG(WARN, "\n VPU interrupt wait timeout coreIdx=%d, instIdx=%d\n", coreIdx, instIdx);

                        VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);    /* To finish bitstream empty status */
                        interruptFlag = VPU_WaitInterruptEx(handle, 1000);
                        if (interruptFlag)
                        {
                            VLOG(INFO, "get interrupt before reset!!\n");
                            if (interruptFlag > 0)
                            {
                                VPU_ClearInterrupt(coreIdx);
                            }
                        }
                        else
                        {
                            VLOG(ERR, "STREAM_END_SIZE called but VPU is still busy \n");
                        }
                        reset_rd_ptr(vidCodHandle, &lastRd);
                        VPU_SWReset(coreIdx, SW_RESET_SAFETY, handle);
                        VLOG(INFO, "sw reset!!!!!\n");
                        VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_CLEAR_FLAG);    /* To finish bitstream empty status */
                        restart = TRUE;
                        //LeaveLock(coreIdx);
                        dispIdx ++;
                        break;
                    }
                    timeoutCount++;
                    interruptFlag = 0;
                }

                CheckUserDataInterrupt(coreIdx, handle, outputInfo.indexFrameDecoded, handle->CodecInfo->decInfo.openParam.bitstreamFormat, interruptFlag);

                if (interruptFlag & (1 << INT_BIT_PIC_RUN))
                {
                    repeat = FALSE;
                }
                if (interruptFlag & (1 << INT_BIT_DEC_FIELD))
                {
                    if (bitstreamMode == BS_MODE_PIC_END)
                    {
                        // do not clear interrupt until feeding next field picture.
                        assertedFieldDoneInt = TRUE;
                        break;
                    }
                }
                if (interruptFlag & (1 << INT_BIT_DEC_MB_ROWS))
                {
                    VPU_ClearInterrupt(coreIdx);
                    VPU_DecGiveCommand(handle, GET_LOW_DELAY_OUTPUT, &outputInfo);
                    VLOG(INFO, "MB ROW interrupt is generated displayIdx=%d decodedIdx=%d picType=%d decodeSuccess=%d\n",
                         outputInfo.indexFrameDisplay, outputInfo.indexFrameDecoded, outputInfo.picType, outputInfo.decodingSuccess);
                }

                if (interruptFlag > 0)
                {
                    VPU_ClearInterrupt(coreIdx);
                }

                if (repeat == FALSE || restart == TRUE) break;

                if (vidCodHandle->endof_flag == BMDEC_START_CLOSE) //break;
                {
                    skipWaiting = TRUE;
                    VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG);
                }

                if(bitstreamMode == BS_MODE_PIC_END)
                {
                    VLOG(INFO, "interrupt flag: %d, in decoding BS_MODE_PIC_END, coreIdx: %d, instIdx: %d ......\n", interruptFlag, coreIdx, instIdx);
                    VPU_ClearInterrupt(coreIdx);
                    continue;
                }
                //                if ((interruptFlag & (1 << INT_BIT_BIT_BUF_EMPTY)))
                {
                    osal_cond_lock(vidCodHandle->inputCond);
                    update_bs_buffer(vidCodHandle, &lastRd);
                    if ((interruptFlag & (1 << INT_BIT_BIT_BUF_EMPTY)))
                    {
                        if(Queue_Get_Cnt(vidCodHandle->inputQ)<MIN_WAITING_QUEUE_LEN && vidCodHandle->endof_flag<BMDEC_START_GET_ALLFRAME)
                        {
    //                        VLOG(INFO, "INT_BIT_BIT_BUF_EMPTY, waiting input...\n");
                            //osal_cond_wait(vidCodHandle->inputCond);
                            //if(repeat == TRUE)
                            //    restart = TRUE;
                            skipWaiting = TRUE;
                            //VPU_DecUpdateBitstreamBuffer(handle, 1);                   // let f/w to know stream end condition in bitstream buffer. force to know that bitstream buffer will be empty.
                            VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG); // set to stream end condition to pump out a delayed framebuffer.
                            VLOG(WARN, "INT_BIT_BIT_BUF_EMPTY may be some wrong in decoder..., coreIdx: %d, instIdx: %d, flag: %d\n", coreIdx, instIdx, interruptFlag);
                        }
                    }
                    osal_cond_unlock(vidCodHandle->inputCond);
                }

                if (interruptFlag & (1 << INT_BIT_BIT_BUF_FULL))
                {
                    osal_cond_lock(vidCodHandle->outputCond);
                    if(vidCodHandle->frameInBuffer > 0 && vidCodHandle->frameInBuffer>=vidCodHandle->extraFrameBufferNum && vidCodHandle->endof_flag <= BMDEC_START_ENDOF)
                    {
                        //VLOG(INFO, "INT_BIT_BIT_BUF_FULL, waiting output...\n");
                        //osal_cond_wait(vidCodHandle->outputCond);
                        skipWaiting = TRUE;
//                        VPU_DecUpdateBitstreamBuffer(handle, 1);                   // let f/w to know stream end condition in bitstream buffer. force to know that bitstream buffer will be empty.
                        VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG); // set to stream end condition to pump out a delayed framebuffer.
                        VLOG(WARN, "may be some wrong in decoder..., coreIdx: %d, instIdx: %d\n", coreIdx, instIdx);
//                        restart = TRUE;
                    }
                    osal_cond_unlock(vidCodHandle->outputCond);
                }

            }

            if(restart)
            {
                restart = FALSE;
                if(repeat)
                {
//                    reset_rd_ptr(vidCodHandle, &lastRd);
                    //SetPendingInst(coreIdx, 0);
                    //LeaveLock(coreIdx);
                    VLOG(ERR, "Can't get interrupt retry to decoder 1 frame........, coreIdx: %d, instIdx: %d\n", coreIdx, instIdx);
                    continue;
                }
            }

            if (assertedFieldDoneInt == TRUE)
            {
                // needStream = TRUE;
                continue;
            }

            ret = VPU_DecGetOutputInfo(handle, &outputInfo);
            if (ret != RETCODE_SUCCESS)
            {
                 VLOG(ERR, "VPU_DecGetOutputInfo failed Error code is 0x%x , instIdx=%d \n", ret, instIdx);
                if (ret == RETCODE_MEMORY_ACCESS_VIOLATION)
                {
                    PrintVpuStatus(coreIdx, handle->productId);
                    PrintMemoryAccessViolationReason(coreIdx, &outputInfo);
                }
                continue;
            }

//            if (outputInfo.indexFrameDecoded >= 0)
//                DisplayDecodedInformation(handle, handle->CodecInfo->decInfo.openParam.bitstreamFormat, decodedIdx, &outputInfo);

            if (outputInfo.sequenceChanged == TRUE)
            {
                seqChangeRequest = TRUE;
                seqChangedRdPtr = outputInfo.rdPtr;
                seqChangedWrPtr = outputInfo.wrPtr;
                VLOG(INFO, "seqChangeRdPtr: 0x%08x, WrPtr: 0x%08x\n", seqChangedRdPtr, seqChangedWrPtr);
                if ((ret = VPU_DecSetRdPtr(handle, seqChangedRdPtr, TRUE)) != RETCODE_SUCCESS)
                {
                    VLOG(ERR, "%s:%d Failed to VPU_DecSetRdPtr(%d), ret(%d)\n", __FUNCTION__, __LINE__, seqChangedRdPtr, ret);
                    break;
                }
                seqChangedStreamEndFlag = outputInfo.streamEndFlag;
                //VPU_DecUpdateBitstreamBuffer(handle, 1);                   // let f/w to know stream end condition in bitstream buffer. force to know that bitstream buffer will be empty.
                VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG); // set to stream end condition to pump out a delayed framebuffer.

                VLOG(INFO, "---> SEQUENCE CHANGED <---\n");
            }

            if (bitstreamMode == BS_MODE_INTERRUPT && outputInfo.consumedByte > 0 && seqChangeRequest == FALSE) {
                //processing input queue.
                PkgInfo *pkginfo;
                osal_cond_lock(vidCodHandle->inputCond);
                while((pkginfo = Queue_Peek(vidCodHandle->inputQ))!=NULL) {
                    if(pkginfo->rd != lastRd) {
                        VLOG(ERR, "this is a bug. didn't sync the input buffer!, lastRd:0x%lx, pkgrd:0x%lx\n",
                            lastRd, pkginfo->rd);
                        lastRd = pkginfo->rd;
                    }

                    if(pkginfo->flag != 0) {
                        if(outputInfo.rdPtr < lastRd && handle->CodecInfo->decInfo.streamBufSize + outputInfo.rdPtr - lastRd >= pkginfo->len) {
                            lastRd = pkginfo->rd + pkginfo->len - handle->CodecInfo->decInfo.streamBufSize;
                            Queue_Dequeue(vidCodHandle->inputQ);
                            pts_tmp = pkginfo->pts;
                            dts_tmp = pkginfo->dts;
                        }
                        else {
                            break;
                        }
                    }
                    else {
                        if(outputInfo.rdPtr - lastRd>=pkginfo->len) {
                            lastRd = pkginfo->rd + pkginfo->len;
                            Queue_Dequeue(vidCodHandle->inputQ);
                            pts_tmp = pkginfo->pts;
                            dts_tmp = pkginfo->dts;
                        }
                        else {
                            break;
                        }
                    }
                }
                osal_cond_unlock(vidCodHandle->inputCond);
            }

            if (outputInfo.indexFrameDecoded >= 0/* && preDecIdx != outputInfo.indexFrameDecoded*/) {
                pts[outputInfo.indexFrameDecoded] = pts_tmp;
                dts[outputInfo.indexFrameDecoded] = dts_tmp;
                //VLOG(INFO, "decoderIdx: %d, lastRd:0x%lx, Rd:0x%lx, pts: %ld, dts: %ld\n",outputInfo.indexFrameDecoded, lastRd, outputInfo.rdPtr, pts_tmp, dts_tmp);

                //preDecIdx = outputInfo.indexFrameDecoded;
                decodedIdx++;
                if(vidCodHandle->no_reorder_flag == 1) {
                    osal_cond_lock(vidCodHandle->outputCond);
                    vidCodHandle->frameInBuffer += 1;
                    osal_cond_unlock(vidCodHandle->outputCond);
                    DisplayQueue_En(vidCodHandle, &outputInfo, pts_tmp, dts_tmp);
                }
            }

            if (vidCodHandle->enablePPU == TRUE)
            {
                if (prevFbIndex >= 0)
                {
                    VPU_DecClrDispFlag(handle, prevFbIndex);
                }
                prevFbIndex = outputInfo.indexFrameDisplay;

                if (waitPostProcessing == TRUE)
                {
                    if (outputInfo.indexFrameDisplay >= 0)
                    {
                        waitPostProcessing = FALSE;
                    }
                    rcPpu = outputInfo.rcDisplay;
                    /* Release framebuffer for PPU */
                    Queue_Enqueue(ppuQ, (void *)ppuFb);
                    osal_cond_lock(vidCodHandle->outputCond);
                    vidCodHandle->frameInBuffer += 1;
                    osal_cond_unlock(vidCodHandle->outputCond);
                    /*                    needStream = (handle->CodecInfo->decInfo.openParam.bitstreamMode == BS_MODE_PIC_END);
                    if (outputInfo.chunkReuseRequired == TRUE)
                    {
                        needStream = FALSE;
                    }
                    */
                    // Not ready for ppu buffer.
                    continue;
                }
                else
                {
                    if (outputInfo.indexFrameDisplay < 0)
                    {
                        waitPostProcessing = TRUE;
                    }
                }
            }

            if (/*preDispIdx != outputInfo.indexFrameDisplay && */(outputInfo.indexFrameDisplay >= 0 && vidCodHandle->no_reorder_flag == 0) || vidCodHandle->enablePPU == TRUE)
            {
                if(vidCodHandle->enablePPU == TRUE) {
                    preDispIdx = prevFbIndex;
                    outputInfo.rcDisplay = rcPpu;
                }
                else {
                    preDispIdx = outputInfo.indexFrameDisplay;
                }
                //VLOG(INFO, "preDispIdx: %d, myIndex: %d, dispIdx: %d\n", preDispIdx, outputInfo.dispFrame.myIndex, dispIdx);
                if(preDispIdx>=0 || vidCodHandle->enablePPU == TRUE)
                {
/*                   if(outputInfo.dispFrame.width!=0 && outputInfo.dispFrame.height!=0)
                     {
                        VPU_DecClrDispFlag(handle, outputInfo.dispFrame.myIndex);
                    }
                    else
                        */
                    {
                        osal_cond_lock(vidCodHandle->outputCond);
                        vidCodHandle->frameInBuffer += 1;
                        osal_cond_unlock(vidCodHandle->outputCond);
                        if(preDispIdx >= 0 && preDispIdx < handle->CodecInfo->decInfo.numFrameBuffers)
                            DisplayQueue_En(vidCodHandle, &outputInfo, pts[preDispIdx], dts[preDispIdx]);
                        else
                            DisplayQueue_En(vidCodHandle, &outputInfo, -1L, -1L);
                        dispIdx++;
                    }
                }
            }
            //else if(preDispIdx == outputInfo.indexFrameDisplay)
            //    VPU_DecClrDispFlag(handle, outputInfo.dispFrame.myIndex);

            if (outputInfo.indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END)
            {
                if (seqChangeRequest == TRUE)
                {
                    vpu_buffer_t *pFbMem = vidCodHandle->pFbMem;
                    vpu_buffer_t *pPPUFbMem = vidCodHandle->pPPUFbMem;
                    seqChangeRequest = FALSE;
                    VPU_DecSetRdPtr(handle, seqChangedRdPtr, TRUE);
                    if (seqChangedStreamEndFlag == 1)
                    {
                        VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG);
                    }
                    else
                    {
                        VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_CLEAR_FLAG);
                    }
                    if (seqChangedWrPtr >= seqChangedRdPtr)
                    {
                        VPU_DecUpdateBitstreamBufferParam(handle, seqChangedWrPtr - seqChangedRdPtr);
                    }
                    else
                    {
                        VPU_DecUpdateBitstreamBufferParam(handle, (vidCodHandle->codecInst->CodecInfo->decInfo.openParam.bitstreamBuffer + vidCodHandle->codecInst->CodecInfo->decInfo.openParam.bitstreamBufferSize) - seqChangedRdPtr +
                                                                 (seqChangedWrPtr - vidCodHandle->codecInst->CodecInfo->decInfo.openParam.bitstreamBuffer));
                    }

                    // Flush renderer: waiting all picture displayed.
                    //SimpleRenderer_Flush(renderer);

                    // Release all memory related to framebuffer.
                    VPU_DecGiveCommand(handle, DEC_FREE_FRAME_BUFFER, 0x00);
                    for (index = 0; index < MAX_REG_FRAME; index++)
                    {
                        if (pFbMem[index].size > 0)
                        {
                            vdi_free_dma_memory(coreIdx, &pFbMem[index]);
                            pFbMem[index].size=0;
                        }
                        if (pPPUFbMem[index].size > 0)
                        {
                            vdi_free_dma_memory(coreIdx, &pPPUFbMem[index]);
                            pPPUFbMem[index].size=0;
                        }
                    }
                    seqInited = FALSE;
                    vidCodHandle->seqInitFlag = 0;
                    updateStreamBufferFlag = FALSE;
                    continue;
                }

                if(skipWaiting==TRUE)
                {
                    skipWaiting = FALSE;
                    VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_CLEAR_FLAG);
                    VLOG(INFO, "clear the end of flag in vpu, coreIdx: %d, instIdx: %d.\n", coreIdx, instIdx);
                    continue;
                }
                //break; waiting for get all frame.
                VLOG(INFO, "get endof interrupt coreIdx: %d, instIdx: %d!!\n", coreIdx, instIdx);
                vidCodHandle->decStatus = BMDEC_STOP;
                while(Queue_Get_Cnt(vidCodHandle->displayQ)!=0 && vidCodHandle->endof_flag<BMDEC_START_STOP)
                {
                    osal_msleep(1);
                }
            }
            /*

            if (vidCodHandle->codecInst->CodecInfo->decInfo.openParam.bitstreamMode == BS_MODE_PIC_END) {
                if (outputInfo.indexFrameDecoded == DECODED_IDX_FLAG_NO_FB) {
                    needStream = FALSE;
                }
                else {
                    needStream = TRUE;
                }
            }

            if (outputInfo.chunkReuseRequired == TRUE) {
                needStream = FALSE;
            }

            SaveDecReport(coreIdx, handle, &outputInfo, decOP.bitstreamFormat,
                          ((sequenceInfo.picWidth+15)&~15)/16,
                          ((sequenceInfo.picHeight+15)&~15)/16);
            */

    }
    vidCodHandle->decStatus = BMDEC_STOP;
    while(vidCodHandle->endof_flag != BMDEC_START_CLOSE) {
        osal_msleep(USLEEP_CLOCK / 1000);
    }
    vidCodHandle->decStatus = BMDEC_CLOSE;
    //CloseDecReport(coreIdx);
    // Now that we are done with decoding, close the open instance.
    //VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);
#ifdef TRY_FLOCK_OPEN
    get_lock_timeout(TRY_OPEN_SEM_TIMEOUT,pcie_board_idx);
#else
    get_lock_timeout(vidCodHandle->vid_open_sem, TRY_OPEN_SEM_TIMEOUT);
#endif
    sVpuInstPool[coreIdx*MAX_NUM_INSTANCE+instIdx] = NULL;
    for (index=0; index<MAX_REG_FRAME; index++) {
        VPU_DecClrDispFlag(handle, index);
    }
    /********************************************************************************
     * DESTROY INSTANCE                                                             *
     ********************************************************************************/
    VPU_DecClose(handle);

    for (index = 0; index < MAX_REG_FRAME; index++)
    {
        if (vidCodHandle->pFbMem[index].size > 0)
        {
            vdi_free_dma_memory(coreIdx, &(vidCodHandle->pFbMem[index]));
            vidCodHandle->pFbMem[index].size=0;
        }

        if (vidCodHandle->pPPUFbMem[index].size > 0)
        {
            vdi_free_dma_memory(coreIdx, &(vidCodHandle->pPPUFbMem[index]));
            vidCodHandle->pPPUFbMem[index].size = 0;
        }
    }

    VLOG(INFO, "\nDec End. Tot Frame %d\n", decodedIdx);

    if (vidCodHandle->vbStream.size > 0)
    {
        VLOG(INFO, "free vbstream buffer !!!\n");
        vdi_free_dma_memory(coreIdx, &(vidCodHandle->vbStream));
        vidCodHandle->vbStream.size=0;
    }

    VPU_DeInit(coreIdx);
#ifdef TRY_FLOCK_OPEN
    unlock_flock(pcie_board_idx);
#else
    sem_post(vidCodHandle->vid_open_sem);
    sem_close(vidCodHandle->vid_open_sem);
#endif
    if (vidCodHandle->ppuQ != NULL)
        Queue_Destroy(vidCodHandle->ppuQ);
    if (vidCodHandle->freeQ != NULL)
        Queue_Destroy(vidCodHandle->freeQ);
    if (vidCodHandle->displayQ != NULL)
        Queue_Destroy(vidCodHandle->displayQ);
    if(vidCodHandle->inputQ != NULL)
        Queue_Destroy(vidCodHandle->inputQ);
    if(vidCodHandle->inputQ2 != NULL)
        Queue_Destroy(vidCodHandle->inputQ2);
    vidCodHandle->decStatus = BMDEC_CLOSED;
    if(pts_org != NULL)
        osal_free(pts_org);
    if(dts_org != NULL)
        osal_free(dts_org);
    //clean memory
}

SequenceMemInfo seqMemInfo[MAX_NUM_INSTANCE][MAX_SEQUENCE_MEM_COUNT];

static void releasePreviousSequenceResources(DecHandle handle, vpu_buffer_t* arrFbMem, DecGetFramebufInfo* prevSeqFbInfo, int mem_type)
{
    Uint32 i, coreIndex;

    if (handle == NULL)
    {
        return;
    }
    coreIndex = VPU_HANDLE_CORE_INDEX(handle);
    for (i = 0; i < MAX_REG_FRAME; i++)
    {
        if (arrFbMem[i].size > 0)
        {
            vdi_free_dma_memory(coreIndex, &arrFbMem[i]);
            arrFbMem[i].size =0;
        }
    }
    for (i = 0; i < MAX_REG_FRAME; i++)
    {
        if(mem_type != BUFFER_ALLOC_FROM_USER)
        {
            if (prevSeqFbInfo->vbFbcYTbl[i].size > 0)
            {
                vdi_free_dma_memory(coreIndex, &prevSeqFbInfo->vbFbcYTbl[i]);
                prevSeqFbInfo->vbFbcYTbl[i].size = 0;

            }
            if (prevSeqFbInfo->vbFbcCTbl[i].size > 0)
            {
                vdi_free_dma_memory(coreIndex, &prevSeqFbInfo->vbFbcCTbl[i]);
                prevSeqFbInfo->vbFbcCTbl[i].size = 0;
            }

        }
        else
        {
            vdi_dettach_dma_memory(coreIndex, &prevSeqFbInfo->vbFbcYTbl[i]);
            vdi_dettach_dma_memory(coreIndex, &prevSeqFbInfo->vbFbcCTbl[i]);
            vdi_unmap_memory(coreIndex, &prevSeqFbInfo->vbFbcYTbl[i]);
            vdi_unmap_memory(coreIndex, &prevSeqFbInfo->vbFbcCTbl[i]);
        }

        if (prevSeqFbInfo->vbMvCol[i].size > 0)
        {
            vdi_free_dma_memory(coreIndex, &prevSeqFbInfo->vbMvCol[i]);
            prevSeqFbInfo->vbMvCol[i].size = 0;
        }

    }
}

static int decSeqChange(BMVidCodHandle vidCodHandle, DecOutputInfo* outputInfo)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    RetCode ret = RETCODE_FAILURE;
    DecInitialInfo initialInfo;
    Int32 compressedFbCount, linearFbCount;
    Uint32 framebufStride;
    Uint32 index;
    FrameBuffer pFrame[MAX_REG_FRAME];
    vpu_buffer_t *pFbMem;
    DecHandle handle;
    BMVidDecConfig *param;
    TestDecConfig decParam;
    BOOL dpbChanged, sizeChanged, bitDepthChanged;
    Uint32 sequenceChangeFlag;
    Int32 coreIdx;

    if (vidHandle != NULL && vidHandle->codecInst != NULL)
    {
        handle = vidHandle->codecInst;
        param = &(vidHandle->decConfig);
        pFbMem = vidHandle->pFbMem;
        coreIdx = VPU_HANDLE_CORE_INDEX(handle);
    }
    else
    {
        return ret;
    }
    osal_memset(&initialInfo, 0x00, sizeof(DecInitialInfo));
    osal_memset(&decParam, 0x00, sizeof(TestDecConfig));

    decParam.enableCrop             = param->enableCrop;
    decParam.bitFormat              = (CodStd)param->bitFormat;
    decParam.coreIdx                = param->coreIdx;
    decParam.bitstreamMode          = param->bitstreamMode;
    decParam.enableWTL              = param->enableWTL;
    decParam.wtlMode                = param->wtlMode;
    decParam.wtlFormat              = param->wtlFormat;
    decParam.cbcrInterleave         = param->cbcrInterleave;
    decParam.nv21                   = param->nv21;
    decParam.streamEndian           = param->streamEndian;
    decParam.frameEndian            = param->frameEndian;
    decParam.mapType                = param->mapType;
//    decParam.wave.fbcMode           = param->wave.fbcMode;
    decParam.wave.bwOptimization    = param->wave.bwOptimization;
    decParam.secondaryAXI           = param->secondaryAXI;

    sequenceChangeFlag  = outputInfo->sequenceChanged;

    dpbChanged = (sequenceChangeFlag&SEQ_CHANGE_ENABLE_DPB_COUNT) ? TRUE : FALSE;
    sizeChanged = (sequenceChangeFlag&SEQ_CHANGE_ENABLE_SIZE) ? TRUE : FALSE;
    bitDepthChanged = (sequenceChangeFlag&SEQ_CHANGE_ENABLE_BITDEPTH) ? TRUE : FALSE;

    if (dpbChanged || sizeChanged || bitDepthChanged)
    {
        BOOL remainingFbs[MAX_REG_FRAME];
        Int32 fbIndex;
        Uint32 dispFlag;
        Uint32 curSeqNo    = outputInfo->sequenceNo;
        Uint32 seqMemIndex = curSeqNo % MAX_SEQUENCE_MEM_COUNT;
        SequenceMemInfo* pSeqMem = &seqMemInfo[handle->instIndex][seqMemIndex];
        DecGetFramebufInfo prevSeqFbInfo;
        VLOG(INFO, "----- SEQUENCE CHANGED -----\n");
        Queue_Enqueue(vidHandle->sequenceQ, (void*)&curSeqNo);
        osal_memset((void*)remainingFbs, 0x00, sizeof(remainingFbs));
        // Get previous memory related to framebuffer
        VPU_DecGiveCommand(handle, DEC_GET_FRAMEBUF_INFO, (void*)&prevSeqFbInfo);
        // Get current(changed) sequence information.
        VPU_DecGiveCommand(handle, DEC_GET_SEQ_INFO, &initialInfo);

        VLOG(INFO, "sequenceChanged : %x\n", sequenceChangeFlag);
        VLOG(INFO, "SEQUENCE NO : %d\n", initialInfo.sequenceNo);
        VLOG(INFO, "DPB COUNT: %d\n", initialInfo.minFrameBufferCount);
        VLOG(INFO, "BITDEPTH : LUMA(%d), CHROMA(%d)\n", initialInfo.lumaBitdepth, initialInfo.chromaBitdepth);
        VLOG(INFO, "SIZE     : WIDTH(%d), HEIGHT(%d)\n", initialInfo.picWidth, initialInfo.picHeight);

        /* Check Not displayed framebuffers */
        dispFlag = outputInfo->frameDisplayFlag;
        for (index=0; index<MAX_GDI_IDX; index++)
        {
            fbIndex = index;
            if ((dispFlag>>index) & 0x01)
            {
                if (param->enableWTL == TRUE)
                {
                    fbIndex = VPU_CONVERT_WTL_INDEX(handle, fbIndex);
                }
            remainingFbs[fbIndex] = TRUE;
            }
        }

        for (index=0; index<MAX_REG_FRAME; index++)
        {
            if (remainingFbs[index] == FALSE && vidHandle->framebuf_from_user != BUFFER_ALLOC_FROM_USER)
            {
                // free allocated framebuffer
                if (pFbMem[index].size > 0)
                {
                    vdi_free_dma_memory(coreIdx, &pFbMem[index]);
                    pFbMem[index].size = 0;
                }
            }
        }

        // Free all framebuffers
        if (param->bitFormat == STD_VP9)
        {
            for ( index=0 ; index<MAX_REG_FRAME; index++)
            {
                if(prevSeqFbInfo.vbMvCol[index].size > 0)
                {
                    vdi_free_dma_memory(coreIdx, &prevSeqFbInfo.vbMvCol[index]);
                    prevSeqFbInfo.vbMvCol[index].size = 0;
                }
                if(prevSeqFbInfo.vbFbcYTbl[index].size > 0)
                {
                    vdi_free_dma_memory(coreIdx, &prevSeqFbInfo.vbFbcYTbl[index]);
                    prevSeqFbInfo.vbFbcYTbl[index].size=0;
                }
                if(prevSeqFbInfo.vbFbcCTbl[index].size > 0)
                {
                    vdi_free_dma_memory(coreIdx, &prevSeqFbInfo.vbFbcCTbl[index]);
                    prevSeqFbInfo.vbFbcCTbl[index].size=0;
                }
            }
        }
        osal_memset(pSeqMem, 0x00, sizeof(SequenceMemInfo));
        osal_memcpy(&pSeqMem->fbInfo, &prevSeqFbInfo, sizeof(DecGetFramebufInfo));
        osal_memcpy(pSeqMem->allocFbMem, pFbMem, sizeof(vpu_buffer_t)*MAX_REG_FRAME);

        VPU_DecGiveCommand(handle, DEC_RESET_FRAMEBUF_INFO, NULL);

        /* clear all of display indexed for the previous sequence */;
        for (index=0; index<MAX_REG_FRAME; index++)
        {
            VPU_DecClrDispFlag(handle, index);
        }

        compressedFbCount = initialInfo.minFrameBufferCount + vidHandle->extraFrameBufferNum;       /* max_dec_pic_buffering + @, @ >= 1 */
        if (compressedFbCount > MAX_FRAMEBUFFER_COUNT) {
            compressedFbCount = MAX_FRAMEBUFFER_COUNT;
            vidHandle->extraFrameBufferNum = compressedFbCount - initialInfo.minFrameBufferCount;
            if (vidHandle->extraFrameBufferNum <= 0){
                VLOG(ERR, "bitstream error: frame buffering count %d is more than maxium DPB count!\n", initialInfo.minFrameBufferCount);
                goto ERR_DEC_OPEN;
            } else
                VLOG(WARN, "extra_frame_buffer_num exceeds internal maximum buffer, change to %d!\n", vidHandle->extraFrameBufferNum);
        }

        if (param->bitFormat == STD_VP9)
            linearFbCount      = compressedFbCount;
        else
            linearFbCount      = initialInfo.frameBufDelay + (1+vidHandle->extraFrameBufferNum);       /* max_num_reorder_pics + @,  @ >= 1,
                                                                                                In most case, # of linear fbs must be greater or equal than max_num_reorder,
                                                                                                but the expression of @ in the sample code is in order to make the situation
                                                                                                that # of linear is greater than # of fbc. */
        // TODO: sequence should use callback
        // Now, alloc framebuffer in sdk
        vidHandle->framebuf_from_user = 0;
        decParam.framebuf_from_user = 0;
        osal_memset((void*)pFbMem, 0x00, sizeof(vpu_buffer_t)*MAX_REG_FRAME);
        if (AllocateDecFrameBuffer(handle, &decParam, compressedFbCount, linearFbCount, pFrame, pFbMem, &framebufStride, vidHandle->enable_cache) == FALSE)
        {
            VLOG(ERR, "[SEQ_CHANGE] AllocateDecFrameBuffer failure\n");
            goto ERR_DEC_OPEN;
        }
        ret = VPU_DecRegisterFrameBufferEx(handle, pFrame, compressedFbCount, linearFbCount,
        framebufStride, initialInfo.picHeight, COMPRESSED_FRAME_MAP);
        if (ret != RETCODE_SUCCESS)
        {
            VLOG(ERR, "[SEQ_CHANGE] VPU_DecRegisterFrameBufferEx failed Error code is 0x%x\n", ret);
            goto ERR_DEC_OPEN;
        }
        VLOG(INFO, "----------------------------\n");
    }

    if (param->bitFormat == STD_VP9 && (sequenceChangeFlag&SEQ_CHANGE_INTER_RES_CHANGE))
    {
        Uint32          picWidth, picHeight, newStride;
        Int8            fbcIndex    = 0;
        Int8            linearIndex = 0;
        Int8            mvColIndex;

        FrameBuffer     newFbs[2];
        FrameBuffer*    pFbcFb      = NULL;
        FrameBuffer*    pLinearFb   = NULL;
        vpu_buffer_t    newMem[2];

        VLOG(INFO, "----- INTER RESOLUTION CHANGED -----\n");
        fbcIndex    = (Int8)(outputInfo->indexInterFrameDecoded & 0xff);
        linearIndex = (Int8)((outputInfo->indexInterFrameDecoded >> 8) & 0xff);
        if (linearIndex >= 0)
        {
            linearIndex = (Int8)VPU_CONVERT_WTL_INDEX(handle, linearIndex);
        }
        mvColIndex  = (Int8)((outputInfo->indexInterFrameDecoded >> 16) & 0xff);

        picWidth  = outputInfo->decPicWidth;
        picHeight = outputInfo->decPicHeight;

        VLOG(INFO, "FBC IDX  : %d\n", fbcIndex);
        VLOG(INFO, "LIN IDX  : %d\n", linearIndex);
        VLOG(INFO, "COL IDX  : %d\n", mvColIndex);
        VLOG(INFO, "SIZE     : WIDTH(%d), HEIGHT(%d)\n", picWidth, picHeight);

        if (fbcIndex >= 0)
        {
            /* Release the FBC framebuffer */
            vdi_free_dma_memory(coreIdx, &pFbMem[fbcIndex]);
        }

        if (linearIndex >= 0)
        {
            /* Release the linear framebuffer */
            vdi_free_dma_memory(coreIdx, &pFbMem[linearIndex]);
        }

        if (AllocateDecFrameBuffer(handle, &decParam, (fbcIndex>=0?1:0), (linearIndex>=0?1:0), newFbs, newMem, &newStride, vidHandle->enable_cache) == FALSE)
        {
            goto ERR_DEC_OPEN;
        }

        if (fbcIndex >= 0)
        {
            pFbcFb = &pFrame[fbcIndex];
            *pFbcFb = newFbs[0];
            pFbcFb->myIndex = fbcIndex;
            pFbcFb->width   = picWidth;
            pFbcFb->height  = picHeight;
            pFbMem[fbcIndex] = newMem[0];
        }
        if (linearIndex >= 0)
        {
            pLinearFb = &pFrame[linearIndex];
            *pLinearFb = newFbs[1];
            pLinearFb->myIndex = linearIndex;
            pLinearFb->width   = picWidth;
            pLinearFb->height  = picHeight;
            pFbMem[linearIndex] = newMem[1];
        }

        ret = VPU_DecUpdateFrameBuffer(handle, pFbcFb, pLinearFb, (Uint32)mvColIndex, picWidth, picHeight);
        if (ret != RETCODE_SUCCESS)
        {
            VLOG(ERR, ">>> Failed to INST(%02d) VPU_DecUpdateFrameBuffer(err: %08x)\n", param->instIdx, ret);
            goto ERR_DEC_OPEN;
        }
    }

    return RETCODE_SUCCESS;

ERR_DEC_OPEN:
    return RETCODE_FAILURE;
}
static void SetDoingSWResetInst(Uint32 coreIdx, Uint32 instIdx)
{
    vpu_instance_pool_t* vip;
    vip        = (vpu_instance_pool_t *)vdi_get_instance_pool(coreIdx);
    if (!vip) {
        return;
    }
    vip->doSwResetInstIdxPlus1 = ((int)instIdx + 1);
}
static int GetDoingSWResetInst(Uint32 coreIdx)
{
    vpu_instance_pool_t* vip;
    vip        = (vpu_instance_pool_t *)vdi_get_instance_pool(coreIdx);
    if (!vip) {
        return -1;
    }

    return (vip->doSwResetInstIdxPlus1 - 1);
}
static BOOL DoResetNoDecHandle(Uint32 coreIdx, Uint32 instIndex)
{
    RetCode result;
    BOOL ret = FALSE;

    VLOG(INFO, "> Reset Reqest to VPU inst=%d, DoingSWResetInst=%d\n", instIndex, GetDoingSWResetInst(coreIdx));

    if (GetDoingSWResetInst(coreIdx) == instIndex ||
        GetDoingSWResetInst(coreIdx) == -1) {
        result = VPU_SWReset(coreIdx, SW_RESET_SAFETY, NULL);
        if (result == RETCODE_VPU_STILL_RUNNING)
        {
            VLOG(INFO, "%s:%d inst=%d, > Reset Reqest Fail because VPU is still running\n", __FUNCTION__, __LINE__, instIndex);
            SetDoingSWResetInst(coreIdx, instIndex); // set flag that is doing SWReset
            ret = TRUE;
        }
        if (result == RETCODE_SUCCESS)
        {
            VLOG(WARN, "%s:%d inst=%d, > Reset VPU done\n", __FUNCTION__, __LINE__, instIndex);
            SetDoingSWResetInst(coreIdx, -1); // set flag that is doing SWReset
            ret = FALSE;
        }
    }
    else
    {
        VLOG(INFO, "> SWReset is doing at inst=%d, cur inst=%d\n", GetDoingSWResetInst(coreIdx), instIndex);
        ret = FALSE;
    }
    vdi_delay_ms(10);
    return ret;
}
static BOOL DoReset(DecHandle handle)
{
    RetCode result;
    BOOL ret = FALSE;

    VLOG(INFO, "> Reset Reqest to VPU inst=%d, DoingSWResetInst=%d\n", handle->instIndex, GetDoingSWResetInst(handle->coreIdx));

    if (GetDoingSWResetInst(handle->coreIdx) == handle->instIndex ||
        GetDoingSWResetInst(handle->coreIdx) == -1) {
        result = VPU_SWReset(handle->coreIdx, SW_RESET_SAFETY, handle);
        if (result == RETCODE_VPU_STILL_RUNNING)
        {
            VLOG(INFO, "%s:%d inst=%d, > Reset Reqest Fail because VPU is still running\n", __FUNCTION__, __LINE__, handle->instIndex);
            SetDoingSWResetInst(handle->coreIdx, handle->instIndex); // set flag that is doing SWReset
            ret = TRUE;
        }
        if (result == RETCODE_SUCCESS)
        {
            VLOG(WARN, "%s:%d inst=%d, > Reset VPU done\n", __FUNCTION__, __LINE__, handle->instIndex);
            SetDoingSWResetInst(handle->coreIdx, -1); // set flag that is doing SWReset
            ret = FALSE;
        }
    }
    else
    {
        VLOG(INFO, "> SWReset is doing at inst=%d, cur inst=%d\n", GetDoingSWResetInst(handle->coreIdx), handle->instIndex);
        ret = FALSE;
    }

    return ret;
}

void close_decoder(DecHandle handle, BOOL didSWReset)
{
    int timeoutCount = 0;
    int index;
    RetCode ret;
    Int32 interruptFlag;
    BOOL doSWReset = didSWReset; // didSWReset means that SWReset flow was on going before call close_decoder
    Uint32 coreIdxBak = handle->coreIdx;
    Uint32 instIndexBak = handle->instIndex;

    for (index=0; index<MAX_REG_FRAME; index++) {
        VPU_DecClrDispFlag(handle, index);
    }

    VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG);

    while ((ret=VPU_DecClose(handle)) == RETCODE_VPU_STILL_RUNNING)
    {
        if (timeoutCount >= VPU_BUSY_CHECK_TIMEOUT)
        {
            VLOG(ERR, "NO RESPONSE FROM VPU_DecClose()\n");
            break;
        }

        interruptFlag = VPU_WaitInterruptEx(handle, VPU_WAIT_TIME_OUT_CQ);
        if (interruptFlag > 0) {
            if (interruptFlag & (1<<INT_WAVE5_DEC_PIC)) {
                DecOutputInfo outputInfo;
                VPU_DecGetOutputInfo(handle, &outputInfo);

                if ((outputInfo.decodingSuccess & 0x01) == 0)
                {
                    VLOG(ERR, "%s decode fail instIdx=%d, error(0x%08x) reason(0x%08x), reasonExt(0x%08x)\n", __FUNCTION__,
                        handle->instIndex, outputInfo.decodingSuccess, outputInfo.errorReason, outputInfo.errorReasonExt);

                    if ((outputInfo.errorReason & 0xf0000000))
                    {
                        VLOG(INFO, "%s, > Reset VPU by ResetRestart\n", __FUNCTION__);
                        doSWReset = DoReset(handle);
                    }
                }
            }

            if (interruptFlag & (1<<INT_WAVE5_BSBUF_EMPTY)) {
                VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);    /* To finish bitstream empty status */
            }
            VPU_ClearInterruptEx(handle, interruptFlag);
        }

        for (index=0; index<MAX_REG_FRAME; index++) {
            VPU_DecClrDispFlag(handle, index);
        }

        if (doSWReset == TRUE)
        {
            VLOG(INFO, "%s, > 11. Reset VPU by ResetRequest \n", __FUNCTION__);
            doSWReset = DoReset(handle);
        }
        vdi_delay_ms(1);
        timeoutCount++;
    }
    timeoutCount = 0;
    while(doSWReset == TRUE)
    {
        VLOG(INFO, "+%s, > 22. Reset VPU by ResetRequest 22.\n", __FUNCTION__);
        // because this instance is responsible for done SWReset flow.
        doSWReset = DoResetNoDecHandle(coreIdxBak, instIndexBak);
        if (timeoutCount >= VPU_BUSY_CHECK_TIMEOUT)
        {
            VLOG(ERR, "<%s:%d> inst=%d, Reset flow VPU_BUSY_CHECK_TIMEOUT(%lld)\n",
                __FUNCTION__, __LINE__, instIndexBak, VPU_BUSY_CHECK_TIMEOUT);
            SetDoingSWResetInst(coreIdxBak, -1);  // give up to complete to reset at this instance
            break;
        }
        timeoutCount++;
    }
    if (ret != RETCODE_SUCCESS) {
        VLOG(ERR, "Failed to VPU_DecClose()\n");
    }
}

static void process_vpu_msg_w5(void *arg)
{
    BMVidHandle vidCodHandle = (BMVidHandle)arg;
    BMVidDecConfig *param;
    DecParam decParam;
    DecOutputInfo outputInfo;
    int ret = RETCODE_FAILURE;
    Int32 queueFailCount = 0;
    Int32 timeoutCount = 0;
    Int32 timeoutRetry = 0;
    Int32 interruptFlag = 0;
    Int32 instIdx = 0, coreIdx;
    Int32 pcie_board_idx = 0;
    DecHandle handle;
    BOOL seqInited = FALSE;
    BOOL doingSeqInit = FALSE; // gregory add, this flag is to prevent to call VPU_DecIssueSeqInit again after seqinit timeout
//    BOOL repeat = TRUE;
    Int32 decodedIdx = 0;
    Uint32 dispIdx = 0;
    PhysicalAddress lastRd = 0;
    Uint32 sequenceChangeFlag;
    BOOL supportCommandQueue = TRUE;
    BOOL isDecodingDone = FALSE;
    BOOL seqInitEscape = FALSE;
    BOOL doSWReset = FALSE;
    BOOL doSkipFrame = FALSE;
    Uint32 index;
    BOOL restart = FALSE, bufReuse = FALSE;
    int mem_type;
    int bitstreamMode;
    u64 pts_tmp, dts_tmp;
    u64 *pts, *dts, *pts_org=NULL, *dts_org=NULL;
    PkgInfo *pkginfo = NULL;
    PhysicalAddress prePkgRd = 0;
    //int dump_stream = 0;
    int crst_res=0;
#ifdef VID_PERFORMANCE_TEST
    int64_t total_time = 0;
    int64_t start_pts, end_pts, max_time = 0, min_time = 100000000;
    struct timeval tv;
#endif
    VLOG(INFO, "Enter process message from VPU!\n");
    if (vidCodHandle == NULL || vidCodHandle->codecInst == NULL)
        return;
    handle = vidCodHandle->codecInst;
    param = &(vidCodHandle->decConfig);
    lastRd = handle->CodecInfo->decInfo.streamRdPtr;
    coreIdx = handle->coreIdx;
    instIdx = handle->instIndex;
    pcie_board_idx = coreIdx/MAX_NUM_VPU_CORE_CHIP;

    bitstreamMode = handle->CodecInfo->decInfo.openParam.bitstreamMode;
    osal_memset(&decParam, 0x00, sizeof(DecParam));
    osal_memset(&outputInfo, 0x00, sizeof(DecOutputInfo));
    osal_memset(seqMemInfo[instIdx], 0x00, sizeof(seqMemInfo[instIdx]));
#ifdef __linux__
    VLOG(INFO, "<%x>InstIdx %d: Start process message from VPU!\n", pthread_self(), instIdx);
#elif _WIN32
    VLOG(INFO, "<%x>InstIdx %d: Start process message from VPU!\n", GetCurrentProcessId(), instIdx);
#endif
    while (vidCodHandle->endof_flag < BMDEC_START_CLOSE)
    {
        if(vidCodHandle->endof_flag == BMDEC_START_DECODING && vidCodHandle->decStatus == BMDEC_STOP)
        {
            vidCodHandle->decStatus = BMDEC_DECODING;
        }

        if (vidCodHandle->endof_flag == BMDEC_START_REWIND && vidCodHandle->decStatus == BMDEC_STOP)
        {
            int *pktsize = NULL;

            vidCodHandle->decStatus = BMDEC_DECODING;
            handle->CodecInfo->decInfo.streamEndflag = FALSE;
            crst_res = -1;  // reset status

            /* remove all packet.size == 0 package */
            osal_cond_lock(vidCodHandle->inputCond);
            while((Queue_Get_Cnt(vidCodHandle->inputQ) > 0) &&
                  ((pkginfo = Queue_Peek(vidCodHandle->inputQ)) != NULL && (pkginfo->len == 0)))
                Queue_Dequeue(vidCodHandle->inputQ);

            while(((pktsize = Queue_Peek(vidCodHandle->inputQ2)) != NULL) && (*pktsize == 0))
                Queue_Dequeue(vidCodHandle->inputQ2);
            osal_cond_unlock(vidCodHandle->inputCond);
        }

        //crst_res = bm_syscxt_status(coreIdx, instIdx, 0);
        if (crst_res < 0) {
            if (crst_res == -2) {
#ifdef __linux__
                usleep(100000);
#elif _WIN32
                Sleep(100);
#endif
            }
            supportCommandQueue = TRUE;
            restart = FALSE;
            bufReuse = FALSE;
            seqInited = FALSE;
            isDecodingDone = FALSE;
            seqInitEscape = FALSE;

            handle = vidCodHandle->codecInst;
            instIdx = handle->instIndex;
            lastRd = handle->CodecInfo->decInfo.streamRdPtr;

            crst_res = 0;

            VLOG(INFO, "core%d InstIdx %d restart....\n", coreIdx, instIdx);
            continue;
        }

        osal_cond_lock(vidCodHandle->inputCond);
        while(((Queue_Get_Cnt(vidCodHandle->inputQ)<MIN_WAITING_QUEUE_LEN && bitstreamMode != BS_MODE_PIC_END) || (Queue_Get_Cnt(vidCodHandle->inputQ)<1 && bitstreamMode == BS_MODE_PIC_END)) && vidCodHandle->endof_flag <= BMDEC_START_DECODING)
        {
            //VLOG(INFO, "InstIdx %d: Waiting input, input queue len: %d\n", instIdx, Queue_Get_Cnt(vidCodHandle->inputQ));
            //if (bm_syscxt_chkstatus(coreIdx)  < 0) break;
            osal_cond_wait(vidCodHandle->inputCond);
        }
        //Prevent the shutdown signal from being received while waiting,
        //resulting in subsequent errors
        if( vidCodHandle->endof_flag==BMDEC_START_CLOSE)
            break;
        int *pktsize = NULL;
        if(bufReuse == FALSE) {
            pts_tmp = -1L;
            dts_tmp = -1L;
        }
        while((pktsize = Queue_Peek(vidCodHandle->inputQ2))!=NULL && *pktsize>2)
        {
            if(bitstreamMode != BS_MODE_PIC_END)
                VPU_DecUpdateBitstreamBuffer(handle, *pktsize);
            //VLOG(INFO, "InstIdx %d: input q2, pktsize: %d\n", instIdx, *pktsize);
            Queue_Dequeue(vidCodHandle->inputQ2);
        }

        if(bitstreamMode == BS_MODE_PIC_END && vidCodHandle->decStatus <= BMDEC_DECODING)
        {
            if(bufReuse == TRUE)
            {
                if(prePkgRd != 0)
                    handle->CodecInfo->decInfo.streamRdPtr = prePkgRd;
            }
            else
            {
                pkginfo = Queue_Dequeue(vidCodHandle->inputQ);
                //update rd pointer
                if(pkginfo != NULL) {
                    prePkgRd = pkginfo->rd;
                    pts_tmp = pkginfo->pts;
                    dts_tmp = pkginfo->dts;
                    handle->CodecInfo->decInfo.streamRdPtr = pkginfo->rd;
                    handle->CodecInfo->decInfo.streamWrPtr = pkginfo->rd + pkginfo->len;
                    if(handle->CodecInfo->decInfo.streamBufEndAddr < handle->CodecInfo->decInfo.streamWrPtr) {
                        handle->CodecInfo->decInfo.streamWrPtr = handle->CodecInfo->decInfo.streamBufEndAddr;
                        VLOG(ERR, "may be wr error: 0x%lx\n", handle->CodecInfo->decInfo.streamWrPtr);
                    }
                    if(pkginfo->len==0)
                    {
                        handle->CodecInfo->decInfo.streamEndflag = TRUE;
                    }
                    //VLOG(INFO, "dec pkginfo rd: 0x%lx, len: %d\n", pkginfo->rd, pkginfo->len);
                }
                else {
                    if(vidCodHandle->seqInitFlag == 0) {
                        vidCodHandle->decStatus = BMDEC_STOP;
                        break;
                    }
                    handle->CodecInfo->decInfo.streamRdPtr = handle->CodecInfo->decInfo.streamWrPtr;
                    handle->CodecInfo->decInfo.streamEndflag = TRUE;
                    VLOG(ERR, "may be endof.. please check it.............\n");
                }
            }
        }
        osal_cond_unlock(vidCodHandle->inputCond);

        //if (bm_syscxt_chkstatus(coreIdx)  < 0) continue;
        osal_cond_lock(vidCodHandle->outputCond);
        if(vidCodHandle->frameInBuffer > 0 && (vidCodHandle->frameInBuffer>=vidCodHandle->extraFrameBufferNum )
        && vidCodHandle->endof_flag <= BMDEC_START_ENDOF)
        {
            //VLOG(INFO, "Waiting output buffer, frameInbuffer: %d, numFbsForDecoding: %d\n", vidCodHandle->frameInBuffer, handle->CodecInfo->decInfo.numFbsForWTL);
            osal_cond_wait(vidCodHandle->outputCond);
        }
        osal_cond_unlock(vidCodHandle->outputCond);

        get_reset_flock(pcie_board_idx, coreIdx);
        if (doSWReset == TRUE)
        {
            doSWReset = DoReset(handle);
            // vdi_delay_ms(10);
        }
        unlock_reset_flock(pcie_board_idx, coreIdx);

        if (vidCodHandle->seqInitFlag == 0)
        {
            VLOG(INFO, "InstIdx %d: seqInitFlag\n", instIdx);

            // Scan and retrieve sequence header information from the bitstream buffer.
            seqInitEscape = FALSE;

            ret = VPU_DecSetEscSeqInit(handle, seqInitEscape);
            if (ret != RETCODE_SUCCESS)
            {
                seqInitEscape = 0;
                VLOG(WARN, "InstIdx %d: Wanning! can not to set seqInitEscape in the current bitstream mode Option\n", instIdx);
            }

            if (seqInitEscape == TRUE)
            {
                VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);
            }

            if (doingSeqInit == FALSE)
            {
                do {
                    ret = VPU_DecIssueSeqInit(handle);
                    if (timeoutCount > VPU_DEC_TIMEOUT)
                    {
                        //VLOG(ERR, "InstIdx %d: VPU_DecIssueSeqInit failed Error code is 0x%x \n", instIdx, ret);
                        osal_msleep(20);
                        //break;
                    }
                    if(ret == RETCODE_QUEUEING_FAILURE){
                        QueueStatusInfo     QueueStatus;
                        VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, &QueueStatus);

                        osal_msleep(2);
                    }
                    timeoutCount++;
                } while (ret == RETCODE_QUEUEING_FAILURE);

                //doingSeqInit = TRUE;
            }
            timeoutCount = 0;
#ifdef _WIN32
            VLOG(INFO, "<%x>InstIdx %d: seqInited\n", GetCurrentProcessId(), instIdx);
#elif __linux__
			VLOG(INFO, "<%x>InstIdx %d: seqInited\n", pthread_self(), instIdx);
#endif
            while (seqInited == FALSE)
            {
                //if (bm_syscxt_chkstatus(coreIdx)  < 0) break;
                interruptFlag = VPU_WaitInterruptEx(handle, vidCodHandle->timeout); //wait for 10ms to save stream filling time.
                if (interruptFlag == -1)
                {
                    timeoutCount++;
                    if (timeoutCount > vidCodHandle->timeout_count)
                    {
                        VLOG(ERR, "\ncoreIdx %d InstIdx %d: VPU seqinit interrupt wait timeout\n", coreIdx, instIdx);

                        get_reset_flock(pcie_board_idx, coreIdx);
                        doSWReset = DoReset(handle);
                        timeoutCount = 0;
                        unlock_reset_flock(pcie_board_idx, coreIdx);
                        restart = TRUE;
                        break;
#if 0
                        vdi_print_vpu_status(coreIdx);
                        vidCodHandle->decStatus = BMDEC_HUNG;
                        restart = TRUE;
                        LeaveLock(coreIdx);
                        break;
#endif
                    }
                    interruptFlag = 0;
                }

                if (interruptFlag > 0)
                {
/*
                    EnterLock(coreIdx);
#ifndef BM_PCIE_MODE
                    VPU_ClearInterruptEx(handle, interruptFlag);
#endif
                    LeaveLock(coreIdx);
*/
                    if (interruptFlag & (1 << INT_WAVE5_INIT_SEQ))
                    {
                        seqInited = TRUE;
                        timeoutCount = 0;
                        break;
                    }
                    else if (interruptFlag & (1<<INT_WAVE5_BSBUF_EMPTY)) {
                        VLOG(ERR, "bs buffer empty in seq init...\n");
                    }
                    else {
                        VLOG(ERR, "interrupt flag 0x%x in seq init...\n", interruptFlag);
                    }
                    doingSeqInit = FALSE;
                }

                if(bitstreamMode == BS_MODE_INTERRUPT)
                {
                    osal_cond_lock(vidCodHandle->inputCond);
                    update_bs_buffer(vidCodHandle, &lastRd);
                    while(Queue_Get_Cnt(vidCodHandle->inputQ)<MIN_WAITING_QUEUE_LEN && vidCodHandle->endof_flag<BMDEC_START_GET_ALLFRAME)
                    {
                        osal_cond_wait(vidCodHandle->inputCond);
                    }
                    osal_cond_unlock(vidCodHandle->inputCond);
                }
                else
                {
                    VLOG(INFO, "please check maybe exception in get seq info.....flag: %d, inst_idx: %d\n", interruptFlag, instIdx);
//                    break;
                }
            }
            if(restart)
            {
                restart = FALSE;
                continue;
            }

            if(seqInited==FALSE)
            {
                SetPendingInst(coreIdx, NULL);
                VLOG(ERR, "some error in find seq header, re try....\n");
                continue;
            }

            if ((ret = BMVidDecSeqInitW5(vidCodHandle)) != RETCODE_SUCCESS)
            {
                VLOG(ERR, "InstIdx %d: BMVidDecSeqInitW5 failed Error code is %d \n", instIdx, ret);
                if(ret < 0) {
                    if(vidCodHandle->decStatus != BMDEC_WRONG_RESOLUTION && vidCodHandle->decStatus != BMDEC_FRAMEBUFFER_NOTENOUGH)
                        vidCodHandle->decStatus = BMDEC_HUNG;
                    break;
                }
                seqInited = FALSE;
                continue;
            }

            vidCodHandle->seqInitFlag = 1;
            vidCodHandle->decStatus = BMDEC_DECODING;
            if (!pts_org) pts_org = osal_malloc(handle->CodecInfo->decInfo.numFrameBuffers*8 + 8);
            if (!dts_org) dts_org = osal_malloc(handle->CodecInfo->decInfo.numFrameBuffers*8 + 8);
            pts = (u64 *)((u64)pts_org & (~7L));
            dts = (u64 *)((u64)dts_org & (~7L));
            VLOG(INFO, "pts_org:%p, pts:%p, dts_org:%p, dts:%p\n", pts_org, pts, dts_org, dts);
            if(!pts || !dts) {
                VLOG(ERR, "can alloc memory for pts and dts\n");
                vidCodHandle->decStatus = BMDEC_HUNG;
                break;
            }
        }

        if(vidCodHandle->decStatus != BMDEC_DECODING)
        {
            osal_msleep(1);
            continue;
        }

        // Start decoding a frame.
        // CRA_AS_BLA flag should be set to 1 when non-IRAP skip enabled.
        if(doSkipFrame == TRUE) {
             decParam.skipframeMode = WAVE_SKIPMODE_NON_IRAP;
             VLOG(INFO, "skip non I frame... inst: %d\n", instIdx);
        }
        else {
            decParam.skipframeMode = param->skipMode;
        }
        decParam.craAsBlaFlag = (decParam.skipframeMode == 1) ? TRUE : param->wave.craAsBla;
        isDecodingDone          = FALSE;
#ifdef VID_PERFORMANCE_TEST
#ifdef __linux__
        if (vidCodHandle->perf != 0 && bufReuse == FALSE && gettimeofday(&tv, NULL) == 0) {
        start_pts = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
        }
#elif _WIN32
        u64 sec = 0;
        u64 usec = 0;
        u64 msec = 0;
        if (vidCodHandle->perf != 0 && bufReuse == FALSE && s_gettimeofday(&sec,&usec,&msec, NULL) == 0) {
            start_pts = usec;
        }
#endif

#endif
        // Start decoding a frame.
        //if (bm_syscxt_chkstatus(coreIdx)  < 0) continue;
        ret = VPU_DecStartOneFrame(handle, &decParam);
        if (ret == RETCODE_QUEUEING_FAILURE)
        {
            QueueStatusInfo qStatus;
            //VLOG(INFO, "InstIdx %d: VPU_DecStartOneFrame PIC mode queueing, instanceQueueCount %d, totalQueueCount %d \n", instIdx, handle->CodecInfo->decInfo.instanceQueueCount, handle->CodecInfo->decInfo.totalQueueCount);
            bufReuse = TRUE;
            osal_msleep(2);

            VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&qStatus);
            if(qStatus.instanceQueueCount == 0) {
                queueFailCount++;
                if(queueFailCount > 500  && vidCodHandle->decStatus >= BMDEC_INT_TIMEOUT) {
                    VLOG(ERR, "dec send stream queueing fail.\n");
                    vidCodHandle->decStatus = BMDEC_HUNG;
                }
                continue;
            }
        }
        else if ((ret != RETCODE_QUEUEING_FAILURE) && (ret != RETCODE_SUCCESS))
        {
            QueueStatusInfo qStatus;
            VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&qStatus);
            vidCodHandle->decStatus = BMDEC_HUNG;
            VLOG(ERR, "instIdx %d: VPU_DecStartOneFrame PIC ret %d, instanceQueueCount %d, totalQueueCount %d\n", instIdx, ret, qStatus.instanceQueueCount, qStatus.totalQueueCount);
            PrintDecVpuStatus(handle);
            continue;
        }
        else
        {
            //QueueStatusInfo qStatus;
            //VPU_DecGiveCommand(handle, DEC_GET_QUEUE_STATUS, (void *)&qStatus);
            //VLOG(INFO, "instIdx %d: VPU_DecStartOneFrame PIC success, instanceQueueCount %d, totalQueueCount %d\n", instIdx, qStatus.instanceQueueCount, qStatus.totalQueueCount);
            bufReuse = FALSE;
        }
        do {
            //if (bm_syscxt_chkstatus(coreIdx)  < 0) break;
            if ((interruptFlag=VPU_WaitInterruptEx(handle, vidCodHandle->timeout)) == -1)
            {
                timeoutCount++;
                VLOG(WARN, "coreIdx %d InstIdx %d: interruptFlag %d\n", coreIdx, instIdx, interruptFlag);
                vidCodHandle->decStatus = BMDEC_INT_TIMEOUT;
                //wait for 10ms to save stream filling time.
                if (timeoutCount >= vidCodHandle->timeout_count)
                {
                    VLOG(ERR, "\ncoreIdx %d InstIdx %d: VPU interrupt wait timeout\n", coreIdx, instIdx);

                    if (timeoutRetry > 10){
                        vidCodHandle->decStatus = BMDEC_HUNG;
                        break;
                    }

                    get_reset_flock(pcie_board_idx, coreIdx);
                    doSWReset = DoReset(handle);
                    timeoutCount = 0;
                    timeoutRetry++;
                    unlock_reset_flock(pcie_board_idx, coreIdx);
#if 0
                    VLOG(WARN, "\nInstIdx %d: VPU interrupt timeout and 0xf0000000 reported for %d time, need to be destroy this intance\n", instIdx, VPU_DEC_TIMEOUT);
                    PrintDecVpuStatus(handle);
                    //goto ERR_OPEN;
		            // we will dump the stream with problem and run into dead loop, it is complicated to release all resources and inform releated working threads

                    timeoutCount=0;

                    if(dump_stream == 0) {
                        //dump stream
                        unsigned char *p_stream = malloc(0x700000);
                        if(p_stream != NULL) {
                            int len = bmvpu_dec_dump_stream(vidCodHandle, p_stream, 0x700000);
                            char timeout_dump_file_name[256] = {0};
                            FILE *fp = NULL;
                            sprintf(timeout_dump_file_name, "core%d_inst%d_timeoutdump.bin", coreIdx, instIdx);
                            fp = fopen(timeout_dump_file_name, "wb+");
                            if(fp != NULL) {
                                fwrite(p_stream, 1, len, fp);
                                fclose(fp);
                            }
                            free(p_stream);
                        }
                        dump_stream = 1;
                    }
                    VLOG(WARN, "\ncoreIdx %d InstIdx %d: VPU interrupt wait timeout, quite the program!\n", coreIdx, instIdx);

                    vidCodHandle->decStatus = BMDEC_HUNG;
                    break;
#endif
                }
                //timeoutCount++;
                interruptFlag = 0;
            }
            else if (vidCodHandle->decStatus == BMDEC_INT_TIMEOUT) {
                vidCodHandle->decStatus = BMDEC_DECODING;
            }

            if (interruptFlag > 0)
            {
/*
                EnterLock(coreIdx);
#ifndef	BM_PCIE_MODE
                VPU_ClearInterruptEx(handle, interruptFlag);
#endif
                LeaveLock(coreIdx);
*/
                timeoutRetry = 0;
                timeoutCount = 0;
            }

            if (interruptFlag & (1<<INT_WAVE5_DEC_PIC))
            {
                isDecodingDone = TRUE;
            }
//            if (vidCodHandle->endof_flag == BMDEC_START_CLOSE) isDecodingDone = TRUE;
            if (isDecodingDone == TRUE) break;

            if(bitstreamMode == BS_MODE_INTERRUPT)
            {
                osal_cond_lock(vidCodHandle->inputCond);
                update_bs_buffer(vidCodHandle, &lastRd);
                osal_cond_unlock(vidCodHandle->inputCond);
            }
		} while (supportCommandQueue == FALSE || interruptFlag == -2 );//interruptFlag -2, system signal make the interrupt failed.

        if (supportCommandQueue == TRUE)
        {
            if (isDecodingDone == FALSE)
            {
                if(vidCodHandle->decStatus != BMDEC_HUNG)
                    continue;
            }
            isDecodingDone = FALSE;
        }

        ret = VPU_DecGetOutputInfo(handle, &outputInfo);
        if (ret != RETCODE_SUCCESS)
        {
            VLOG(ERR, "VPU_DecGetOutputInfo failed Error code is 0x%x , instIdx=%d,in there %d\n", ret, instIdx);
            if (ret == RETCODE_MEMORY_ACCESS_VIOLATION)
            {
                PrintVpuStatus(coreIdx, handle->productId);
                PrintMemoryAccessViolationReason(coreIdx, &outputInfo);
            }
            continue;
        }
        if ((outputInfo.decodingSuccess & 0x01) == 0)
        {
            VLOG(ERR, "VPU_DecGetOutputInfo decode fail framdIdx %d error(0x%08x) reason(0x%08x), reasonExt(0x%08x)\n",
                decodedIdx, outputInfo.decodingSuccess, outputInfo.errorReason, outputInfo.errorReasonExt);
            //doSkipFrame = TRUE;
/*            if (outputInfo.errorReason == WAVE5_SPECERR_OVER_PICTURE_WIDTH_SIZE ||
                outputInfo.errorReason == WAVE5_SPECERR_OVER_PICTURE_HEIGHT_SIZE) {
                VLOG(ERR, "Not supported Width or Height\n");
                vidCodHandle->decStatus = BMDEC_HUNG;
                break;
            }*/
            if (outputInfo.errorReason & 0xf0000000)
            {
                VLOG(INFO, "FIX_WAVE_SW_RESET_V4 SWReset with SW_RESET_SAFETY\n");
                get_reset_flock(pcie_board_idx, coreIdx);
                doSWReset = DoReset(handle);
                unlock_reset_flock(pcie_board_idx, coreIdx);
            }
#if 0
            if (outputInfo.errorReason == WAVE5_SYSERR_WATCHDOG_TIMEOUT){
                vdi_log(coreIdx, 0x10000, 1);
	    }
#endif
        }

        sequenceChangeFlag    = outputInfo.sequenceChanged;
        if (sequenceChangeFlag)
        {
            VLOG(INFO, "InstIdx %d: sequenceChangeFlag: 0x%x\n", instIdx, sequenceChangeFlag);
            ret = decSeqChange(vidCodHandle, &outputInfo);
            if (ret != RETCODE_SUCCESS)
            {
                VLOG(ERR, "decSeqChange failed Error code is 0x%x , instIdx=%d\n", ret, instIdx);
                vidCodHandle->decStatus = BMDEC_HUNG;
                break;
            }

            if (pts_org) pts_org = osal_realloc(pts_org, handle->CodecInfo->decInfo.numFrameBuffers*8 + 8);
            if (dts_org) dts_org = osal_realloc(dts_org, handle->CodecInfo->decInfo.numFrameBuffers*8 + 8);
            pts = (u64 *)((u64)pts_org & (~7L));
            dts = (u64 *)((u64)dts_org & (~7L));
            VLOG(INFO, "seqChanged, pts_org:%p, pts:%p, dts_org:%p, dts:%p\n", pts_org, pts, dts_org, dts);
            if(!pts || !dts) {
                VLOG(ERR, "can't realloc memory for pts and dts\n");
                vidCodHandle->decStatus = BMDEC_HUNG;
                break;
            }

        }

        if (bitstreamMode == BS_MODE_INTERRUPT && outputInfo.consumedByte > 0)
        {
            //processing input queue.
            PkgInfo *pkginfo;
            osal_cond_lock(vidCodHandle->inputCond);
            while((pkginfo = Queue_Peek(vidCodHandle->inputQ))!=NULL)
            {
                if(pkginfo->rd != lastRd)
                {
                    VLOG(ERR, "this is a bug. didn't sync the input buffer!, lastRd:0x%lx, pkgrd:0x%lx\n",
                        lastRd, pkginfo->rd);
                    lastRd = pkginfo->rd;
                }

                if(pkginfo->flag != 0)
                {
                    if(outputInfo.rdPtr < lastRd && handle->CodecInfo->decInfo.streamBufSize + outputInfo.rdPtr - lastRd >= pkginfo->len)
                    {
                        lastRd = pkginfo->rd + pkginfo->len - handle->CodecInfo->decInfo.streamBufSize;
                        Queue_Dequeue(vidCodHandle->inputQ);
                        pts_tmp = pkginfo->pts;
                        dts_tmp = pkginfo->dts;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    if(outputInfo.rdPtr - lastRd>=pkginfo->len)
                    {
                        lastRd = pkginfo->rd + pkginfo->len;
                        Queue_Dequeue(vidCodHandle->inputQ);
                        pts_tmp = pkginfo->pts;
                        dts_tmp = pkginfo->dts;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            osal_cond_unlock(vidCodHandle->inputCond);
        }

        if (outputInfo.indexFrameDecoded >= 0)
        {
            pts[outputInfo.indexFrameDecoded] = pts_tmp;
            dts[outputInfo.indexFrameDecoded] = dts_tmp;
            decodedIdx++;
            if (doSkipFrame == TRUE)
            {
                if (decParam.skipframeMode == WAVE_SKIPMODE_NON_IRAP)
                    doSkipFrame = FALSE;
            }
#ifdef VID_PERFORMANCE_TEST
            if(vidCodHandle->perf != 0) {
#ifdef __linux__
            if(gettimeofday(&tv, NULL) == 0) {
                end_pts = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
            }
#elif _WIN32
            u64 sec = 0;
            u64 usec = 0;
            u64 msec = 0;
            if (s_gettimeofday(&sec, &usec , &msec,NULL) == 0) {
                end_pts = usec;
            }
#endif
            else {
                end_pts = 0;
            }
            if(max_time < end_pts - start_pts)
                max_time = end_pts - start_pts;
            if(min_time > end_pts - start_pts)
                min_time = end_pts - start_pts;
            total_time += end_pts - start_pts;

            if(decodedIdx > 0 && (decodedIdx % 1000 == 0))
                printf("core : %d, inst : %d, max_time : %ld us, min_time : %ld us, ave_time : %ld us.\n", coreIdx, instIdx, max_time, min_time, total_time/decodedIdx);
            }

#endif
            if(outputInfo.dispFrame.myIndex>=0 && vidCodHandle->no_reorder_flag == 1)
            {
                if(outputInfo.dispFrame.stride > 8192) {
                    VLOG(ERR, "The display stride wrong. %d\n", outputInfo.dispFrame.stride);
                    VPU_DecClrDispFlag(handle, outputInfo.dispFrame.myIndex);
                }
                else
                {
                    osal_cond_lock(vidCodHandle->outputCond);
                    vidCodHandle->frameInBuffer += 1;
                    osal_cond_unlock(vidCodHandle->outputCond);
                    DisplayQueue_En(vidCodHandle, &outputInfo, pts_tmp, pts_tmp);
                }
            }
        }

        if (vidCodHandle->enable_decode_order && outputInfo.indexFrameDecoded >= 0)
        {
            if(outputInfo.dispFrame.myIndex>=0 && outputInfo.dispFrame.stride <= 8192)
            {
                osal_cond_lock(vidCodHandle->outputCond);
                vidCodHandle->frameInBuffer += 1;
                osal_cond_unlock(vidCodHandle->outputCond);
                DisplayQueue_En(vidCodHandle, &outputInfo, pts[outputInfo.indexFrameDecoded], dts[outputInfo.indexFrameDecoded]);
                vidCodHandle->decode_index_map[outputInfo.dispFrame.myIndex] = outputInfo.indexFrameDisplay;
                dispIdx++;
            }
            else
            {
                VLOG(ERR, "The display stride wrong. decode index %d stride %d\n", outputInfo.indexFrameDecoded, outputInfo.dispFrame.stride);
                VPU_DecClrDispFlag(handle, outputInfo.indexFrameDecoded);
            }
        }
        else if (outputInfo.indexFrameDisplay >= 0 && vidCodHandle->no_reorder_flag == 0)
        {
            if(outputInfo.dispFrame.myIndex>=0 && outputInfo.dispFrame.stride <= 8192)
            {
                osal_cond_lock(vidCodHandle->outputCond);
                vidCodHandle->frameInBuffer += 1;
                osal_cond_unlock(vidCodHandle->outputCond);
                DisplayQueue_En(vidCodHandle, &outputInfo, pts[outputInfo.indexFrameDisplay], dts[outputInfo.indexFrameDisplay]);
                dispIdx++;
            }
            else {
                VLOG(ERR, "The display stride wrong. display index %d stride %d\n", outputInfo.indexFrameDisplay, outputInfo.dispFrame.stride);
                VPU_DecClrDispFlag(handle, outputInfo.indexFrameDisplay);
            }
        }

        if (outputInfo.indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END)
        {
            //break; waiting for get all frame.
            VLOG(INFO, "InstIdx %d: decodedIdx: %d, indexFrameDecoded: %d, indexFrameDisplay: %d, dispFrame.myIndex: %d, pts: %ld, dts: %ld\n",
                instIdx, decodedIdx, outputInfo.indexFrameDecoded, outputInfo.indexFrameDisplay, outputInfo.dispFrame.myIndex, pts_tmp, dts_tmp);
            vidCodHandle->decStatus = BMDEC_STOP;
            while(Queue_Get_Cnt(vidCodHandle->displayQ)!=0 && vidCodHandle->endof_flag<BMDEC_START_STOP)
            {
                osal_msleep(1);
            }
        }

        if(vidCodHandle->decStatus == BMDEC_HUNG)
            break;
    }

    if(vidCodHandle->decStatus != BMDEC_HUNG && vidCodHandle->decStatus != BMDEC_WRONG_RESOLUTION && vidCodHandle->decStatus != BMDEC_FRAMEBUFFER_NOTENOUGH)
        vidCodHandle->decStatus = BMDEC_STOP;
    while(vidCodHandle->endof_flag != BMDEC_START_CLOSE)
    {
        osal_msleep(USLEEP_CLOCK / 1000);
    }
    vidCodHandle->decStatus = BMDEC_CLOSE;
    CloseDecReport(coreIdx);
    // Now that we are done with decoding, close the open instance.
    VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);
#ifdef TRY_FLOCK_OPEN
    get_lock_timeout(TRY_OPEN_SEM_TIMEOUT,pcie_board_idx);
#else
    get_lock_timeout(vidCodHandle->vid_open_sem, TRY_OPEN_SEM_TIMEOUT);
#endif
    sVpuInstPool[coreIdx*MAX_NUM_INSTANCE+instIdx] = NULL;
    /********************************************************************************
     * DESTROY INSTANCE                                                             *
     ********************************************************************************/
    mem_type = handle->CodecInfo->decInfo.framebuf_from_user;
    close_decoder(handle, doSWReset);

    /* Release all previous sequence resources */
    if ( handle )
    {
        for (index = 0; index < MAX_SEQUENCE_MEM_COUNT; index++)
        {
            releasePreviousSequenceResources(handle, seqMemInfo[handle->instIndex][index].allocFbMem,&seqMemInfo[handle->instIndex][index].fbInfo, mem_type);
        }
    }

    for (index = 0; index < MAX_REG_FRAME; index++)
    {
        if(vidCodHandle->pFbMem[index].size > 0)
        {
            if(vidCodHandle->framebuf_from_user != BUFFER_ALLOC_FROM_USER)
            {
                vdi_free_dma_memory(coreIdx, &(vidCodHandle->pFbMem[index]));
                vidCodHandle->pFbMem[index].size=0;
            }
            else
            {
                vdi_dettach_dma_memory(coreIdx, &(vidCodHandle->pFbMem[index]));
                vdi_unmap_memory(coreIdx, &(vidCodHandle->pFbMem[index]));
            }
        }
    }

    VLOG(INFO, "\nDec End. Tot Frame %d. instIdx: %d\n", decodedIdx, instIdx);
#ifdef VID_PERFORMANCE_TEST
    if(vidCodHandle->perf != 0 && decodedIdx > 0)
        VLOG(INFO, "core : %d, inst : %d, max_time : %ld us, min_time : %ld us, ave_time : %ld us.\n", coreIdx, instIdx, max_time, min_time, total_time/decodedIdx);
#endif
    if (vidCodHandle->vbStream.size > 0 && vidCodHandle->bitstream_from_user != BUFFER_ALLOC_FROM_USER)
    {
        vdi_free_dma_memory(coreIdx, &vidCodHandle->vbStream);
        vidCodHandle->vbStream.size = 0;
    }

    if (vidCodHandle->vbUserData.size > 0)
    {
        vdi_free_dma_memory(coreIdx, &vidCodHandle->vbUserData);
        vidCodHandle->vbUserData.size = 0;
    }
    VPU_DeInit(coreIdx);
#ifdef TRY_FLOCK_OPEN
    unlock_flock(pcie_board_idx);
#else
    sem_post(vidCodHandle->vid_open_sem);
    sem_close(vidCodHandle->vid_open_sem);
#endif
    if (vidCodHandle->ppuQ != NULL)
        Queue_Destroy(vidCodHandle->ppuQ);
    if (vidCodHandle->freeQ != NULL)
        Queue_Destroy(vidCodHandle->freeQ);
    if (vidCodHandle->displayQ != NULL)
        Queue_Destroy(vidCodHandle->displayQ);
    if(vidCodHandle->inputQ != NULL)
        Queue_Destroy(vidCodHandle->inputQ);
    if(vidCodHandle->inputQ2 != NULL)
        Queue_Destroy(vidCodHandle->inputQ2);
    if(vidCodHandle->sequenceQ != NULL)
        Queue_Destroy(vidCodHandle->sequenceQ);

    if (vidCodHandle->pusBitCode != NULL)
        osal_free(vidCodHandle->pusBitCode);
    vidCodHandle->pusBitCode = NULL;
    if(pts_org)
        osal_free(pts_org);
    if(dts_org)
        osal_free(dts_org);
    vidCodHandle->decStatus = BMDEC_CLOSED;
}

/**
 * @brief This function returns index of VPU core that depending stretagy.
 * @param pop [Input] this is open decoder parameter
* @return
@verbatim
@* >=0 : VPU index.
@* <0 : Can't assign a VPU index.
@endverbatim
*/
#ifndef CHIP_BM1684
static int getVpuCoreIdx(BMDecStreamFormat format)
{
    if(format >= STD_AVC && format <= STD_VP8)
    {
        int core0InstNum = VPU_GetOpenInstanceNum(0);
        int core1InstNum = VPU_GetOpenInstanceNum(1);
        if(core0InstNum<MAX_NUM_INSTANCE || core1InstNum<MAX_NUM_INSTANCE)
        {
            return core0InstNum>core1InstNum?1:0;
        }
    }
    else if(format == STD_HEVC)
    {
        return 2;
    }
    return -1;
}
#endif

static int checkHandle(BMVidCodHandle handle)
{
    int i,j;
    for(i=0; i<MAX_NUM_VPU_CORE; i++)
        for(j=0; j<MAX_NUM_INSTANCE; j++)
            if(handle==sVpuInstPool[i*MAX_NUM_INSTANCE+j])
                return 1;
    VLOG(ERR, "err dec handle : 0x%p\n", handle);
    return 0;
}

static int bmvpu_dec_buffer_convert(int core_idx, vpu_buffer_t *vdb, BmVpuDecDMABuffer* vb)
{
    int ret;
    if(vdb == NULL || vb == NULL)
        return -1;

    vdb->base = 0xffffffff;
    vdb->size = vb->size;
    vdb->phys_addr = vb->phys_addr;
#ifndef BM_PCIE_MODE
    vdb->enable_cache = 1;
    ret = vdi_mmap_memory(core_idx, vdb);
    if(ret != BM_SUCCESS)
    {
        VLOG(ERR, "bmvpu_dec_buffer_convert: mmap failed. phys addr:0x%lx size:%d\n", vb->phys_addr, vb->size);
    }
    vb->virt_addr = vdb->virt_addr;
#else
    vdb->virt_addr = FAKE_PCIE_VIRT_ADDR;
    vb->virt_addr = 0;
#endif

    return 0;
}

/**
 * @brief This function create a decoder instance.
 * @param pop [Input] this is open decoder parameter
 * @param pHandle [Output] decoder instance.
 * @return error code.
 */
BMVidDecRetStatus bmvpu_dec_create(BMVidCodHandle *pVidCodHandle, BMVidDecParam decParam)
{
    vpu_buffer_t vbStream = {0};
    int ret = BM_ERR_VDEC_FAILURE;
    int coreIdx = 0;//getVpuCoreIdx(decParam.streamFormat);
    Int32 pcie_board_idx = 0;
    DecOpenParam decOP;
    //VpuReportConfig_t decReportCfg;
    DecHandle handle;
    BMVidHandle vidHandle;
    BMVidDecConfig testConfig, *param = &testConfig;

    if (coreIdx < 0 || coreIdx >= MAX_NUM_VPU_CORE)
        return BM_ERR_VDEC_ILLEGAL_PARAM;

    if ((decParam.streamFormat < BMDEC_AVC) && (decParam.streamFormat > BMDEC_HEVC))
        return ret;
    BMVidSetLogLevel();

#ifdef CHIP_BM1684
    return BMVidDecCreateW5(pVidCodHandle, &decParam);
#endif

    if (decParam.streamFormat == BMDEC_HEVC)
    {
        ret = BMVidDecCreateW5(pVidCodHandle, &decParam);
        if(ret != RETCODE_SUCCESS)
            return BM_ERR_FAILURE;
    }

    osal_memset(&testConfig, 0, sizeof(testConfig));

    pcie_board_idx = decParam.pcie_board_id;
    if ((pcie_board_idx <0) || (pcie_board_idx >= MAX_PCIE_BOARD_NUM))
    {
        VLOG(ERR, "pcie board id exceeds max value: %d\n",pcie_board_idx);
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }

    testConfig.bitstreamMode = decParam.bsMode; //BS_MODE_INTERRUPT;
    testConfig.streamEndian = VDI_LITTLE_ENDIAN;
    testConfig.frameEndian = VDI_LITTLE_ENDIAN;
    if (decParam.pixel_format == BM_VPU_DEC_PIX_FORMAT_NV12) {
        testConfig.cbcrInterleave = 1;
        testConfig.nv21 = 0;
    } else if (decParam.pixel_format == BM_VPU_DEC_PIX_FORMAT_NV21) {
        testConfig.cbcrInterleave = 1;
        testConfig.nv21 = 1;
    }
    testConfig.bitFormat = decParam.streamFormat;
    testConfig.coda9.mp4class = decParam.mp4class;
    testConfig.enableWTL = FALSE;
    if(decParam.wtlFormat == BMDEC_OUTPUT_TILED) {
        testConfig.mapType = TILED_FRAME_V_MAP;//decParam.wtlFormat; //LINEAR_FRAME_MAP;
    }
//    else
    testConfig.coda9.enableBWB = VPU_ENABLE_BWB;
    testConfig.coda9.frameCacheBypass = 0;
    testConfig.coda9.frameCacheBurst = 0;
    testConfig.coda9.frameCacheMerge = 3;
    testConfig.coda9.frameCacheWayShape = 15;
    //testConfig.coda9.rotate = 90;

    testConfig.secondaryAXI = decParam.secondaryAXI;

    VLOG(WARN, "------------------------ CHECK PARAMETER COMBINATION ------------------------\n");
    if (testConfig.mapType != LINEAR_FRAME_MAP)
    {
        if (testConfig.enableWTL == TRUE)
        {
            testConfig.wtlMode = FF_FRAME;
        }
    }
    switch (testConfig.mapType)
    {
    case LINEAR_FRAME_MAP:
    case LINEAR_FIELD_MAP:
        if (testConfig.coda9.enableTiled2Linear == TRUE || testConfig.enableWTL == TRUE)
        {
            VLOG(WARN, "[WARN] It can't enable Tiled2Linear OR WTL option where maptype is LINEAR_FRAME_MAP\n");
            VLOG(WARN, "       Disable WTL or Tiled2Linear\n");
            testConfig.coda9.enableTiled2Linear = FALSE;
            testConfig.coda9.tiled2LinearMode = FF_NONE;
            testConfig.enableWTL = FALSE;
            testConfig.wtlMode = FF_NONE;
        }
        break;
    case TILED_FRAME_MB_RASTER_MAP:
    case TILED_FIELD_MB_RASTER_MAP:
        if (testConfig.cbcrInterleave == FALSE)
        {
            VLOG(WARN, "[WARN] CBCR-interleave must be enable when maptype is TILED_FRAME/FIELD_MB_RASTER_MAP.\n");
            VLOG(WARN, "       Enable cbcr-interleave\n");
            testConfig.cbcrInterleave = TRUE;
        }
        break;
    default:
        break;
    }

    if (testConfig.coda9.enableTiled2Linear == TRUE)
    {
        VLOG(WARN, "[WARN] In case of Tiledmap, disabled BWB for better performance.\n");
        testConfig.coda9.enableBWB = FALSE;
    }

    if ((testConfig.bitFormat == STD_DIV3 || testConfig.bitFormat == STD_THO) && testConfig.bitstreamMode != BS_MODE_PIC_END) {
        //char* ext;
        VLOG(WARN, "[WARN] VPU can decode DIV3 or Theora codec on picend mode.\n");
        VLOG(WARN, "       BSMODE %d -> %d\n", BS_MODE_INTERRUPT, BS_MODE_PIC_END);

//        ext = GetFileExtension(inputPath);
//        if (ext != NULL && (strcmp(ext, "mp4") == 0 || strcmp(ext, "mkv") == 0 || strcmp(ext, "avi") == 0 || strcmp(ext, "MP4") == 0 || strcmp(ext, "MKV") == 0 || strcmp(ext, "AVI") == 0)) {
        testConfig.bitstreamMode = BS_MODE_PIC_END;
//        }
        if (testConfig.bitFormat == STD_THO) {
            testConfig.bitstreamMode = BS_MODE_PIC_END;
        }
    }

    int productId;
    Uint16 *pusBitCode = NULL;
    Uint32 sizeInWord = 0;
#ifdef TRY_FLOCK_OPEN
    get_lock_timeout(TRY_OPEN_SEM_TIMEOUT,pcie_board_idx);
#else
    sem_t* vid_open_sem = sem_open(VID_OPEN_SEM_NAME, O_CREAT, 0666, 1);
    get_lock_timeout(vid_open_sem, TRY_OPEN_SEM_TIMEOUT);
#endif
#if !defined(CHIP_BM1880)
    if(VPU_GetOpenInstanceNum(decParam.pcie_board_id*MAX_NUM_VPU_CORE_CHIP)>VPU_GetOpenInstanceNum(decParam.pcie_board_id*MAX_NUM_VPU_CORE_CHIP + 1))
    {
        coreIdx = decParam.pcie_board_id*MAX_NUM_VPU_CORE_CHIP + 1;
        if ((productId = VPU_GetProductId(decParam.pcie_board_id*MAX_NUM_VPU_CORE_CHIP + 1)) == -1)
        {
            VLOG(ERR, "Failed to get product ID\n");
            //success=FALSE;
            goto ERR_DEC_INIT;
        }
    }
    else
#endif
    {
        coreIdx = decParam.pcie_board_id*MAX_NUM_VPU_CORE_CHIP;

        if ((productId = VPU_GetProductId(decParam.pcie_board_id*MAX_NUM_VPU_CORE_CHIP)) == -1)
        {
            VLOG(ERR, "Failed to get product ID\n");
            //success=FALSE;
            goto ERR_DEC_INIT;
        }
    }
//    EnterLock(coreIdx);
//    if((coreIdx == 0 || coreIdx == 1) && VPU_IsInit(coreIdx) == 0) /* VPU don't run. */
    {
        //        LeaveLock(coreIdx);

        if (LoadFirmware(productId, (Uint8 **)&pusBitCode, &sizeInWord, CORE_0_BIT_CODE_FILE_PATH) < 0)
        {
            VLOG(ERR, "%s:%d Failed to load firmware: %s\n", __FUNCTION__, __LINE__, CORE_0_BIT_CODE_FILE_PATH);
            goto ERR_DEC_INIT;
        }

        ret = VPU_InitWithBitcode(coreIdx, (const Uint16 *)pusBitCode, sizeInWord);
        osal_free(pusBitCode);
        pusBitCode = NULL;

        PrintVpuVersionInfo(coreIdx);
    }

//    LeaveLock(coreIdx);
    if (ret != RETCODE_CALLED_BEFORE && ret != RETCODE_SUCCESS)
    {
        VLOG(ERR, "Failed to boot up VPU(coreIdx: %d, productId: %d)\n", coreIdx, productId);
        goto ERR_DEC_INIT;
    }

    VLOG(INFO, "load firmware success!\n");
    vbStream.size = STREAM_BUF_SIZE;
    if(vdi_allocate_dma_memory(coreIdx, &vbStream) < 0)
    {
        VLOG(ERR, "fail to allocate bitstream buffer\n");
        //success=FALSE;
        goto ERR_DEC_INIT;
    }

    osal_memset(&decOP, 0x00, sizeof(DecOpenParam));
    param->enableCrop = TRUE;

    /* set up decoder configurations */
    decOP.bitstreamFormat = (CodStd)param->bitFormat;
    decOP.avcExtension = param->coda9.enableMvc;
    decOP.coreIdx = coreIdx;
    decOP.bitstreamBuffer = vbStream.phys_addr;
    decOP.bitstreamBufferSize = vbStream.size;
    decOP.bitstreamMode = param->bitstreamMode;
    decOP.tiled2LinearEnable = param->coda9.enableTiled2Linear;
    decOP.tiled2LinearMode = param->coda9.tiled2LinearMode;
    decOP.wtlEnable = param->enableWTL;
    decOP.wtlMode = param->wtlMode;
    decOP.cbcrInterleave = param->cbcrInterleave;
    decOP.streamEndian = param->streamEndian;
    decOP.frameEndian = param->frameEndian;
    decOP.bwbEnable = param->coda9.enableBWB;
    decOP.mp4DeblkEnable = param->coda9.enableDeblock;
    decOP.mp4Class = param->coda9.mp4class;

    /********************************************************************************
     * CREATE INSTANCE                                                              *
     ********************************************************************************/
    if ((ret = VPU_DecOpen(&handle, &decOP)) != RETCODE_SUCCESS)
    {
        VLOG(ERR, "VPU_DecOpen failed Error reason is %s \n", bmvpu_dec_error_string(ret));
        ret = BM_ERR_VDEC_FAILURE;
        goto DECODE_OPEN_END;
    }
    VLOG(INFO, "boda create core_idx: %d, inst_idx: %d\n", (int)handle->coreIdx, (int)handle->instIndex);
    //osal_memset((void *)&decReportCfg, 0x00, sizeof(VpuReportConfig_t));
    //decReportCfg.userDataEnable = param->enableUserData;
    //decReportCfg.userDataReportMode = 1;
    //OpenDecReport(coreIdx, &decReportCfg);
    //ConfigDecReport(coreIdx, handle, decOP.bitstreamFormat);
    vidHandle = osal_malloc(sizeof(BMVidCodInst));
    if(vidHandle == NULL) {
        goto DECODE_OPEN_END;
    }
    osal_memset(vidHandle, 0, sizeof(BMVidCodInst));
    if(decParam.frameDelay >= 1)
        VPU_DecGiveCommand(handle, DEC_SET_FRAME_DELAY, &(decParam.frameDelay));
    vidHandle->codecInst = handle;

    osal_memcpy(&(vidHandle->vbStream), &vbStream, sizeof(vbStream));
    vidHandle->decConfig = *param;
    vidHandle->isStreamBufFilled = 0;
    vidHandle->seqInitFlag = 0;
    *pVidCodHandle = (BMVidCodHandle)vidHandle;
#if !defined(TRY_FLOCK_OPEN)
    vidHandle->vid_open_sem = vid_open_sem;
#endif
    vidHandle->enablePPU = FALSE;
    vidHandle->pusBitCode = pusBitCode;
    vidHandle->sizeInWord = sizeInWord;
    vidHandle->extraFrameBufferNum = decParam.extraFrameBufferNum;
    if(vidHandle->extraFrameBufferNum <= 0) {
        vidHandle->extraFrameBufferNum = EXTRA_FRAME_BUFFER_NUM;
    }

    if ((vidHandle->displayQ=Queue_Create_With_Lock(MAX_QUEUE_LEN, sizeof(BMVidFrame))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->freeQ = Queue_Create_With_Lock(MAX_QUEUE_LEN, sizeof(FrameBuffer))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->inputQ = Queue_Create(INPUT_QUEUE_LEN, sizeof(PkgInfo))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->inputQ2 = Queue_Create(MAX_QUEUE_LEN, sizeof(int))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->inputCond = osal_cond_create()) == NULL) {
        goto DECODE_OPEN_END;
    }
    if ((vidHandle->outputCond = osal_cond_create()) == NULL) {
        goto DECODE_OPEN_END;
    }

    osal_cond_lock(vidHandle->outputCond);
    vidHandle->frameInBuffer = 0;
    osal_cond_unlock(vidHandle->outputCond);
    vidHandle->isStreamBufFilled = 0;
    vidHandle->seqInitFlag = 0;
    vidHandle->endof_flag = 0;
    vidHandle->streamWrAddr = vbStream.phys_addr;
    vidHandle->remainedSize = 0;
    vidHandle->decStatus = BMDEC_UNINIT;
    vidHandle->enable_cache = decParam.enable_cache;
    sVpuInstPool[coreIdx*MAX_NUM_INSTANCE+handle->instIndex] = vidHandle;
    {
        vidHandle->processThread = osal_thread_create(process_vpu_msg, (void*)vidHandle);
    }

#ifdef TRY_FLOCK_OPEN
    unlock_flock(pcie_board_idx);
#else
    sem_post(vid_open_sem);
#endif
    return ret;
DECODE_OPEN_END:
    /********************************************************************************
     * DESTROY INSTANCE                                                             *
     ********************************************************************************/
    VPU_DecClose(handle);
    if (vidHandle->freeQ != NULL)
        Queue_Destroy(vidHandle->freeQ);
    if (vidHandle->displayQ != NULL)
        Queue_Destroy(vidHandle->displayQ);
    if(vidHandle->inputQ != NULL)
        Queue_Destroy(vidHandle->inputQ);
    if(vidHandle->inputQ2 != NULL)
        Queue_Destroy(vidHandle->inputQ2);
    if (vidHandle->inputCond)
        osal_cond_destroy(vidHandle->inputCond);
    if (vidHandle->outputCond)
        osal_cond_destroy(vidHandle->outputCond);
    if(vidHandle != NULL)
    {
        free(vidHandle);
        vidHandle=NULL;
    }

ERR_DEC_INIT:
    if (vbStream.size > 0)
    {
        vdi_free_dma_memory(coreIdx, &vidHandle->vbStream);
        vidHandle->vbStream.size = 0;
    }

    VPU_DeInit(coreIdx);
#ifdef TRY_FLOCK_OPEN
    unlock_flock(pcie_board_idx);
#else
    sem_post(vid_open_sem);
    //vidHandle->vid_open_sem = vid_open_sem;
    sem_close(vid_open_sem);
#endif
    return ret;
}

int BMVidDecCreateW5(BMVidCodHandle *pVidCodHandle, BMVidDecParam *decParam)
{
    vpu_buffer_t vbStream = {0};
    RetCode ret = RETCODE_FAILURE;
#if defined(CHIP_BM1684) || defined(CHIP_BM1686)
    int coreIdx = 0;
    int first_in_one_vpu = 0;
    int maxW5CoreNum = 0;
    int instanceNum[MAX_NUM_VPU_CORE] = {0}, minInstNum = 0;
#else
    int coreIdx = getVpuCoreIdx(decParam->streamFormat);
#endif
#if defined(TRY_FLOCK_OPEN)
    Int32 pcie_board_idx;
#endif
    if((decParam->pcie_board_id < 0) || (decParam->pcie_board_id >= MAX_PCIE_BOARD_NUM))
    {
        VLOG(ERR, "board index exceeds max support number %d\n",decParam->pcie_board_id);
        return ret;
    }
    coreIdx = MAX_NUM_VPU_CORE_CHIP*decParam->pcie_board_id+coreIdx;
    VLOG(ERR,"BMvidDecCreateW5 board id %d coreid %d\n",decParam->pcie_board_id, coreIdx);

    DecOpenParam decOP;
    //VpuReportConfig_t decReportCfg;
    DecHandle handle;
    BMVidHandle vidHandle = NULL;
    BMVidDecConfig testConfig, *param = &testConfig;
    char *fwPath = NULL;
    int productId[MAX_NUM_VPU_CORE] = {0};
    Uint16 *pusBitCode = NULL;
    Uint32 sizeInWord = 0;
    Int32 timeoutCount = 0;
    Int32 interruptFlag = 0;
    Uint32 index, ver, rev;
    Uint32 framebuffer_cnt;
    int bitstream_flag = 0;

    if (coreIdx < 0)
    {
        return ret;
    }
    VLOG(INFO, "MAX instance: %d, MAX queue: %d, MAX buffer: 0x%lx, EXTRA frame num: %d\n", MAX_NUM_INSTANCE, COMMAND_QUEUE_DEPTH, STREAM_BUF_SIZE, decParam->extraFrameBufferNum);
    VLOG(INFO, "PARAMETER: pixel_format %d, Wtlformat %d, bsmode %d\n", decParam->pixel_format, decParam->wtlFormat, decParam->bsMode);
    osal_memset(&testConfig, 0, sizeof(testConfig));
#if defined(TRY_FLOCK_OPEN)
    pcie_board_idx = decParam->pcie_board_id;
#endif
    testConfig.bitstreamMode    = decParam->bsMode;
    testConfig.streamEndian     = VPU_STREAM_ENDIAN;
    testConfig.frameEndian      = VPU_FRAME_ENDIAN;
    testConfig.bitFormat        = decParam->streamFormat;
    testConfig.mapType          = COMPRESSED_FRAME_MAP;
    testConfig.enableWTL        = TRUE;
    testConfig.wave.fbcMode     = 0x0c;           // best for bandwidth
    testConfig.wave.bwOptimization = FALSE;     // only valid for WTL enable case
    testConfig.wave.numVCores   = 1;
    if (decParam->pixel_format == BM_VPU_DEC_PIX_FORMAT_NV12) {
        testConfig.cbcrInterleave = 1;
        testConfig.nv21 = 0;
    } else if (decParam->pixel_format == BM_VPU_DEC_PIX_FORMAT_NV21) {
        testConfig.cbcrInterleave = 1;
        testConfig.nv21 = 1;
    }

    testConfig.wtlMode          = FF_FRAME;
    testConfig.extern_picWidth  = decParam->picWidth;
    testConfig.extern_picHeight = decParam->picHeight;
    if(decParam->wtlFormat != BMDEC_OUTPUT_COMPRESSED)
        testConfig.wtlFormat        = decParam->wtlFormat;
    else {
        testConfig.enableWTL        = FALSE;
    }
    testConfig.secondaryAXI     = 0;//decParam->secondaryAXI;                    //doen't use secondaryAXI for WAVE
    testConfig.skipMode = decParam->skip_mode;
#ifdef TRY_FLOCK_OPEN
    get_lock_timeout(TRY_OPEN_SEM_TIMEOUT,pcie_board_idx);
#else
    sem_t* vid_open_sem = sem_open(VID_OPEN_SEM_NAME, O_CREAT, 0666, 1);
    get_lock_timeout(vid_open_sem, TRY_OPEN_SEM_TIMEOUT);
#endif

#ifdef CHIP_BM1684
    coreIdx = -1;
    testConfig.secondaryAXI     = decParam->secondaryAXI;

    // In some chips, sys0 and sys1 may have problems
    // video_cap=0 sys0:ok sys1:ok ;video_cap=1 sys0:no sys1:ok ;video_cap=2 sys0:ok sys1:no ;video_cap=3 sys0:no sys1:no
    // bin_type=0 fast bin ; bin_type=1 one sys bad; bin_type=0 pcie bad
    int bin_type  = 0;
    int video_cap = 0;
    int chip_id   = 0;

    if (vdi_get_video_cap(MAX_NUM_VPU_CORE_CHIP * decParam->pcie_board_id, &video_cap, &bin_type, &maxW5CoreNum, &chip_id) < 0) {
        VLOG(ERR,"BMvidDecCreateW5 get chip info failed!!!\n");
        goto ERR_DEC_INIT;
    }

    if(video_cap == VIDEO_CAP_NONE)
    {
        VLOG(ERR,"No video core available!!!\n");
        goto ERR_DEC_INIT;
    }
    if(chip_id == 0x1686)
        testConfig.secondaryAXI = 0xf;

    maxW5CoreNum = maxW5CoreNum - 1;

    for(index = 0; index < maxW5CoreNum; index++)
    {
        instanceNum[index] = VPU_GetOpenInstanceNum(MAX_NUM_VPU_CORE_CHIP*decParam->pcie_board_id+index);
        if(first_in_one_vpu == 1 && instanceNum[index] < MAX_NUM_INSTANCE)
        {
            coreIdx = index;
            minInstNum = instanceNum[index];
            break;
        }
    }
    if(first_in_one_vpu != 1) {
        minInstNum = instanceNum[0];
        coreIdx = 0;
        for(index = 1; index < maxW5CoreNum; index++)
        {
            if(instanceNum[index] < minInstNum)
            {
                minInstNum = instanceNum[index];
                if(minInstNum < MAX_NUM_INSTANCE)
                    coreIdx = index;
            }
        }
    }

    if(decParam->core_idx != -1)
        coreIdx = decParam->core_idx;

    coreIdx = MAX_NUM_VPU_CORE_CHIP*decParam->pcie_board_id+coreIdx;

    if(coreIdx < 0) {
        VLOG(ERR, "Failed to get core index....\n");
        goto ERR_DEC_INIT;
    }

//    if(minInstNum == 0) {
//        VPU_HWReset(coreIdx);
//    }
#endif
#if defined(BM_PCIE_MODE) && defined(CHIP_BM1684)
    if ((productId[coreIdx%MAX_NUM_VPU_CORE_CHIP] = VPU_GetProductId(coreIdx)) == -1)
#else
    if ((productId[coreIdx] = VPU_GetProductId(coreIdx)) == -1)
#endif
    {
        VLOG(ERR, "Failed to get product ID\n");
        goto ERR_DEC_INIT;
    }

#if (defined(BM_PCIE_MODE) && defined(CHIP_BM1684))
    coreIdx %= MAX_NUM_VPU_CORE_CHIP;
#endif
    VLOG(INFO, "Use core %d\n", coreIdx);
    switch (productId[coreIdx]) {
        case PRODUCT_ID_512:  fwPath = CORE_2_BIT_CODE_FILE_PATH; break;
        case PRODUCT_ID_511:  fwPath = CORE_7_BIT_CODE_FILE_PATH; break;
        default:
            VLOG(ERR, "Unknown product id: %d\n", productId[coreIdx]);
            goto ERR_DEC_INIT;
    }
    if (LoadFirmware(productId[coreIdx], (Uint8 **)&pusBitCode, &sizeInWord, fwPath) < 0)
    {
        VLOG(ERR, "%s:%d Failed to load firmware: %s\n", __FUNCTION__, __LINE__, fwPath);
        goto ERR_DEC_INIT;
    }

#if (defined(BM_PCIE_MODE) && defined(CHIP_BM1684))
    coreIdx = MAX_NUM_VPU_CORE_CHIP*decParam->pcie_board_id + coreIdx;
#endif
    ret = VPU_InitWithBitcode(coreIdx, (const Uint16 *)pusBitCode, sizeInWord);

    if (ret != RETCODE_CALLED_BEFORE && ret != RETCODE_SUCCESS)
    {
        VLOG(ERR, "Failed to boot up VPU(coreIdx: %d, productId: %d)\n", coreIdx, productId[coreIdx]);
        goto ERR_DEC_INIT;
    }
    VLOG(INFO, "load firmware success!\n");

    VPU_GetVersionInfo(coreIdx, &ver, &rev, NULL);

    printf("VERSION=%d, REVISION=%d\n", ver, rev);
    osal_memset(&vbStream, 0, sizeof(vpu_buffer_t));

    if(decParam->bitstream_buffer.phys_addr && decParam->bitstream_buffer.size)
    {
        bitstream_flag = 1;
        bmvpu_dec_buffer_convert(coreIdx, &vbStream, &decParam->bitstream_buffer);
        if(vbStream.size == 0 || vbStream.phys_addr == 0)
        {
            VLOG(ERR, "bitstream buffer from user is NULL!!!\n");
            goto ERR_DEC_INIT;
        }
        vdi_attach_dma_memory(coreIdx, &vbStream);
    }
    else
    {
        vbStream.size = decParam->streamBufferSize == 0 ? STREAM_BUF_SIZE : decParam->streamBufferSize;
        if(vdi_allocate_dma_memory(coreIdx, &vbStream) < 0)
        {
            VLOG(ERR, "fail to allocate bitstream buffer\n");
            goto ERR_DEC_INIT;
        }
    }

    param->enableCrop = TRUE;

    osal_memset(&decOP, 0x00, sizeof(DecOpenParam));
    decOP.bitstreamFormat     = (CodStd)param->bitFormat;
    decOP.coreIdx             = coreIdx;
    decOP.bitstreamBuffer     = vbStream.phys_addr;
    decOP.bitstreamBufferSize = vbStream.size;
    decOP.bitstreamMode       = param->bitstreamMode;
    decOP.wtlEnable           = param->enableWTL;
    decOP.wtlMode             = param->wtlMode;
    decOP.cbcrInterleave      = param->cbcrInterleave;
    decOP.nv21                = param->nv21;
    decOP.streamEndian        = param->streamEndian;
    decOP.frameEndian         = param->frameEndian;
    //decOP.fbc_mode            = param->wave.fbcMode;
    decOP.bwOptimization      = param->wave.bwOptimization;
    decOP.decodeOrder         = decParam->decode_order;

    /********************************************************************************
     * CREATE INSTANCE                                                              *
     ********************************************************************************/

    if ((ret = VPU_DecOpen(&handle, &decOP)) != RETCODE_SUCCESS)
    {
        VLOG(ERR, "VPU_DecOpen failed Error code is 0x%x \n", ret);
        goto DECODE_OPEN_END;
    }

    vidHandle = osal_malloc(sizeof(BMVidCodInst));
    if(vidHandle == NULL) {
        goto DECODE_OPEN_END;
    }
    osal_memset(vidHandle,0, sizeof(BMVidCodInst));

    //VPU_DecGiveCommand(handle, ENABLE_LOGGING, 0);

    //osal_memset((void *)&decReportCfg, 0x00, sizeof(VpuReportConfig_t));
    //decReportCfg.userDataEnable = param->enableUserData;
    //decReportCfg.userDataReportMode = 1;
    //OpenDecReport(coreIdx, &decReportCfg);
    //ConfigDecReport(coreIdx, handle, decOP.bitstreamFormat);


    vidHandle->codecInst = handle;
    vidHandle->bitstream_from_user = bitstream_flag;
    osal_memset(vidHandle->pFbMem, 0, sizeof(vpu_buffer_t)*MAX_REG_FRAME);
    if(decParam->frame_buffer)
    {
        if(decParam->Ytable_buffer == NULL || decParam->Ctable_buffer == NULL )
        {
            VLOG(ERR, "Invalid parameter. Ytable_buffer=0x%lx Ctable_buffer=0x%lx\n", decParam->Ytable_buffer, decParam->Ctable_buffer);
            goto ERR_DEC_INIT;
        }

        if(decParam->min_framebuf_cnt < 0 || decParam->framebuf_delay < 0 || decParam->extraFrameBufferNum <= 0)
        {
            VLOG(ERR, "Invalid frame buffer count. frame_buffer:0x%x min_framebuf_cnt:%d frame_buffer:%d extra_frame_buffer:%d\n",
                decParam->frame_buffer, decParam->min_framebuf_cnt, decParam->framebuf_delay, decParam->extraFrameBufferNum);
            goto ERR_DEC_INIT;
        }
        vidHandle->framebuf_from_user = 1;
        vidHandle->min_framebuf_cnt = decParam->min_framebuf_cnt;
        vidHandle->framebuf_delay = decParam->framebuf_delay;

        framebuffer_cnt = decParam->min_framebuf_cnt + decParam->extraFrameBufferNum;
        for(index = 0; index < framebuffer_cnt; index++)
        {
            bmvpu_dec_buffer_convert(coreIdx, &vidHandle->pYtabMem[index], &decParam->Ytable_buffer[index]);
            bmvpu_dec_buffer_convert(coreIdx, &vidHandle->pCtabMem[index], &decParam->Ctable_buffer[index]);
            vdi_attach_dma_memory(coreIdx, &vidHandle->pYtabMem[index]);
            vdi_attach_dma_memory(coreIdx, &vidHandle->pCtabMem[index]);
        }

        if(testConfig.enableWTL == TRUE)
            framebuffer_cnt += decParam->framebuf_delay + decParam->extraFrameBufferNum + 1;
        for(index = 0; index < framebuffer_cnt; index++)
        {
            bmvpu_dec_buffer_convert(coreIdx, &vidHandle->pFbMem[index], &decParam->frame_buffer[index]);
            vdi_attach_dma_memory(coreIdx, &vidHandle->pFbMem[index]);
        }
    }

    osal_memcpy(&(vidHandle->vbStream), &vbStream, sizeof(vpu_buffer_t));
    param->enableUserData = 4;
    osal_memset(&vidHandle->vbUserData, 0, sizeof(vpu_buffer_t));
    vidHandle->vbUserData.size = 512 * 1024;
    if(vdi_allocate_dma_memory(coreIdx, &vidHandle->vbUserData) < 0)
    {
        VLOG(ERR, "fail to allocate user data buffer\n");
        goto ERR_DEC_INIT;
    }
    VPU_DecGiveCommand(handle, SET_ADDR_REP_USERDATA, (void*)&(vidHandle->vbUserData.phys_addr));
    VPU_DecGiveCommand(handle, SET_SIZE_REP_USERDATA, (void*)&(vidHandle->vbUserData.size));
    VPU_DecGiveCommand(handle, ENABLE_REP_USERDATA,   (void*)&param->enableUserData);
    if(testConfig.skipMode != 0) {
        VPU_DecGiveCommand(handle, ENABLE_DEC_THUMBNAIL_MODE, NULL);
    }
    vidHandle->decConfig = *param;
    vidHandle->isStreamBufFilled = 0;
    vidHandle->seqInitFlag = 0;
    *pVidCodHandle = (BMVidCodHandle)vidHandle;

    vidHandle->pusBitCode = pusBitCode;
    vidHandle->sizeInWord = sizeInWord;
    vidHandle->extraFrameBufferNum = decParam->extraFrameBufferNum;
    if(vidHandle->extraFrameBufferNum <= 0) {
        vidHandle->extraFrameBufferNum = EXTRA_FRAME_BUFFER_NUM;
    }

    if(decParam->frameDelay == 1)
        vidHandle->no_reorder_flag = 1;
    if ((vidHandle->displayQ=Queue_Create_With_Lock(32, sizeof(BMVidFrame))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->freeQ = Queue_Create_With_Lock(32, sizeof(FrameBuffer))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->inputQ = Queue_Create(INPUT_QUEUE_LEN, sizeof(PkgInfo))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->inputQ2 = Queue_Create(MAX_QUEUE_LEN, sizeof(int))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->sequenceQ=Queue_Create_With_Lock(32, sizeof(Uint32))) == NULL) {
        goto DECODE_OPEN_END;
    }

    if ((vidHandle->inputCond = osal_cond_create()) == NULL) {
        goto DECODE_OPEN_END;
    }
    if ((vidHandle->outputCond = osal_cond_create()) == NULL) {
        goto DECODE_OPEN_END;
    }
    osal_cond_lock(vidHandle->outputCond);
    vidHandle->frameInBuffer = 0;
    osal_cond_unlock(vidHandle->outputCond);
    vidHandle->isStreamBufFilled = 0;
    vidHandle->seqInitFlag = 0;
    vidHandle->endof_flag = 0;
    vidHandle->streamWrAddr = vbStream.phys_addr;
    vidHandle->decStatus = BMDEC_UNINIT;
    vidHandle->enable_cache = decParam->enable_cache;
    vidHandle->min_time = 100000000;
    vidHandle->perf = decParam->perf;
    vidHandle->enable_decode_order = decParam->decode_order;

    if(decParam->timeout > 0)
        vidHandle->timeout = decParam->timeout;
    else
        vidHandle->timeout = VPU_WAIT_TIME_OUT;

    if(decParam->timeout_count > 0)
        vidHandle->timeout_count = decParam->timeout_count;
    else
        vidHandle->timeout_count = 5;

    osal_memset(vidHandle->decode_index_map, -1, sizeof(vidHandle->decode_index_map));
#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    sVpuInstPool[coreIdx*MAX_NUM_INSTANCE+handle->instIndex] = vidHandle;
    {
        vidHandle->processThread = osal_thread_create(process_vpu_msg_w5, (void*)vidHandle);
        //bm_syscxt_init(&decOP, vidHandle);
    }

#ifdef TRY_FLOCK_OPEN
    unlock_flock(pcie_board_idx);
#else
    vidHandle->vid_open_sem = vid_open_sem;
    sem_post(vid_open_sem);
#endif

    return ret;
DECODE_OPEN_END:
    /********************************************************************************
     * DESTROY INSTANCE                                                             *
     ********************************************************************************/
    timeoutCount = 0;

    VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SET_FLAG);

    while ((ret=VPU_DecClose(handle)) == RETCODE_VPU_STILL_RUNNING)
    {
        if (timeoutCount >= VPU_BUSY_CHECK_TIMEOUT)
        {
            VLOG(ERR, "NO RESPONSE FROM VPU_DecClose()\n");
            break;
        }
        interruptFlag = VPU_WaitInterruptEx(handle, VPU_WAIT_TIME_OUT_CQ);
        if (interruptFlag > 0) {
            if (interruptFlag & (1<<INT_WAVE5_BSBUF_EMPTY)) {
                VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);    /* To finish bitstream empty status */
            }
            VPU_ClearInterruptEx(handle, interruptFlag);
        }

        for (index=0; index<MAX_REG_FRAME; index++) {
            VPU_DecClrDispFlag(handle, index);
        }

        vdi_delay_ms(1);
        timeoutCount++;
    }

    if (ret != RETCODE_SUCCESS) {
        VLOG(ERR, "Failed to VPU_DecClose()\n");
    }

    if (vidHandle->vbStream.size > 0)
    {
        if(vidHandle->bitstream_from_user != BUFFER_ALLOC_FROM_USER)
            vdi_free_dma_memory(coreIdx, &vidHandle->vbStream);
        else
        {
            vdi_dettach_dma_memory(coreIdx, &vidHandle->vbStream);
            vdi_unmap_memory(coreIdx, &vidHandle->vbStream);
        }
        vidHandle->vbStream.size = 0;
    }
    if (vidHandle) {
        if (vidHandle->vbUserData.size > 0)
        {
            vdi_free_dma_memory(coreIdx, &vidHandle->vbUserData);
            vidHandle->vbUserData.size = 0;
        }
        if (vidHandle->freeQ != NULL)
            Queue_Destroy(vidHandle->freeQ);
        if (vidHandle->displayQ != NULL)
            Queue_Destroy(vidHandle->displayQ);
        if(vidHandle->inputQ != NULL)
            Queue_Destroy(vidHandle->inputQ);
        if(vidHandle->inputQ2 != NULL)
            Queue_Destroy(vidHandle->inputQ2);
        if(vidHandle->sequenceQ != NULL)
            Queue_Destroy(vidHandle->sequenceQ);
        if (vidHandle->inputCond)
            osal_cond_destroy(vidHandle->inputCond);
        if (vidHandle->outputCond)
            osal_cond_destroy(vidHandle->outputCond);
        free(vidHandle);
    }
ERR_DEC_INIT:
    VPU_DeInit(coreIdx);
#ifdef TRY_FLOCK_OPEN
    unlock_flock(pcie_board_idx);
#else
    sem_post(vid_open_sem);
    sem_close(vid_open_sem);
#endif
    return ret;
}

BMVidDecRetStatus bmvpu_dec_seq_init(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    int ret = BM_ERR_VDEC_FAILURE;
    DecInitialInfo sequenceInfo;
    DRAMConfig dram_cfg = {0};
    Uint32 framebufStride, framebufSize;
    Int32 framebufHeight = 0;
    Int32 fbCount;
    FrameBufferAllocInfo fbAllocInfo;
    FrameBuffer pFrame[MAX_REG_FRAME];
    vpu_buffer_t *pFbMem;
    //FrameBuffer*        ppuFb;
    //FrameBuffer pPPUFrame[MAX_REG_FRAME];
    vpu_buffer_t *pPPUFbMem;
    vpu_buffer_t *pvb = NULL;
    Int32 coreIdx;
    Int32 index;
    DecOpenParam *pDecOP;
    SecAxiUse secAxiUse;
    MaverickCacheConfig cacheCfg;
    BOOL enablePPU = FALSE;
    Int32 ppuFbCount = PPU_FB_COUNT;
    Queue *ppuQ = NULL;
    DecHandle handle;
    BMVidDecConfig *param;
    VLOG(INFO, "INFO: enter seq init alloc memory\n");

    if (vidHandle != NULL && vidHandle->codecInst != NULL)
    {
        handle = vidHandle->codecInst;
        coreIdx = handle->coreIdx;
        param = &(vidHandle->decConfig);
        pFbMem = vidHandle->pFbMem;
        pPPUFbMem = vidHandle->pPPUFbMem;
    }
    else
    {
        return BM_ERR_VDEC_UNEXIST;
    }

    VLOG(INFO, "INFO: enter seq init memset\n");

    osal_memset(&sequenceInfo, 0x00, sizeof(DecInitialInfo));
    osal_memset(pFbMem, 0x00, sizeof(vpu_buffer_t) * MAX_REG_FRAME);
    osal_memset(pPPUFbMem, 0x00, sizeof(vpu_buffer_t) * MAX_REG_FRAME);
    VLOG(INFO, "INFO: enter seq init VPU_DecCompleteSeqInit\n");
    if ((ret = VPU_DecCompleteSeqInit(handle, &sequenceInfo)) != RETCODE_SUCCESS)
    {
        VLOG(ERR, "[ERROR] Failed to SEQ_INIT(ERROR REASON: %d)\n", sequenceInfo.seqInitErrReason);
        ret = BM_ERR_VDEC_FAILURE;
        goto DECODE_END;
    }
    VLOG(INFO, "INFO: enter seq init VPU_DecGiveCommand\n");
    ret = VPU_DecGiveCommand(handle, GET_DRAM_CONFIG, &dram_cfg);
    if (ret != RETCODE_SUCCESS)
    {
        VLOG(ERR, "VPU_DecGiveCommand[GET_DRAM_CONFIG] failed Error is reason:%s \n", bmvpu_dec_error_string(ret));
        ret = BM_ERR_VDEC_FAILURE;
        goto DECODE_END;
    }
    VLOG(INFO, "INFO: enter seq init decInfo.openParam\n");

    pDecOP = &(handle->CodecInfo->decInfo.openParam);
    /********************************************************************************
     * ALLOCATE RECON FRAMEBUFFERS                                                    *
     ********************************************************************************/
    framebufHeight = VPU_ALIGN32(sequenceInfo.picHeight);
    framebufStride = CalcStride(sequenceInfo.picWidth, sequenceInfo.picHeight, FORMAT_420, pDecOP->cbcrInterleave, param->mapType, FALSE);
    framebufSize = VPU_GetFrameBufSize(coreIdx, framebufStride, framebufHeight, param->mapType, FORMAT_420, pDecOP->cbcrInterleave, &dram_cfg);
    if(framebufHeight > 1088 || framebufStride > 1920) {
        VLOG(ERR, "height or width too big.....width: %d, height: %d\n", framebufStride, framebufHeight);
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }

    fbCount = sequenceInfo.minFrameBufferCount + vidHandle->extraFrameBufferNum;
    osal_memset((void *)&fbAllocInfo, 0x00, sizeof(fbAllocInfo));
    osal_memset((void *)pFrame, 0x00, sizeof(FrameBuffer) * fbCount);
    fbAllocInfo.format = FORMAT_420;
    fbAllocInfo.cbcrInterleave = pDecOP->cbcrInterleave;
    fbAllocInfo.mapType = param->mapType;
    fbAllocInfo.stride = framebufStride;
    fbAllocInfo.height = framebufHeight;
    fbAllocInfo.lumaBitDepth = sequenceInfo.lumaBitdepth;
    fbAllocInfo.chromaBitDepth = sequenceInfo.chromaBitdepth;
    fbAllocInfo.num = fbCount;
    fbAllocInfo.endian = pDecOP->frameEndian;
    fbAllocInfo.type = FB_TYPE_CODEC;
    for (index = 0; index < fbCount; index++)
    {
        pvb = &pFbMem[index];
        pvb->size = framebufSize;
        VLOG(INFO, "Start fb vdi_allocate_dma_memory, %d, size: %d.....\n", index, framebufSize);
        if(vdi_allocate_dma_memory(coreIdx, pvb) < 0)
        {
            VLOG(ERR, "%s:%d fail to allocate frame buffer\n", __FUNCTION__, __LINE__);
            ret = BM_ERR_VDEC_NOMEM;
            goto DECODE_END;
        }
        pFrame[index].bufY = pvb->phys_addr;
        pFrame[index].bufCb = -1;
        pFrame[index].bufCr = -1;
        pFrame[index].updateFbInfo = TRUE;
    }
    VLOG(INFO, "Start VPU_DecAllocateFrameBuffer....\n");
    if ((ret = VPU_DecAllocateFrameBuffer(handle, fbAllocInfo, pFrame)) != RETCODE_SUCCESS)
    {
        VLOG(ERR, "%s:%d failed to VPU_DecAllocateFrameBuffer(),reason:%s\n", __FUNCTION__, __LINE__, bmvpu_dec_error_string(ret));
        ret = BM_ERR_VDEC_NOMEM;
        goto DECODE_END;
    }
    VLOG(INFO, "Start ALLOCATE WTL FRAMEBUFFERS....\n");
    /********************************************************************************
     * ALLOCATE WTL FRAMEBUFFERS                                                    *
     ********************************************************************************/
    if (pDecOP->wtlEnable == TRUE)
    {
        TiledMapType linearMapType = pDecOP->wtlMode == FF_FRAME ? LINEAR_FRAME_MAP : LINEAR_FIELD_MAP;
        Uint32 strideWtl;

        strideWtl = CalcStride(sequenceInfo.picWidth, sequenceInfo.picHeight, FORMAT_420, pDecOP->cbcrInterleave, linearMapType, FALSE);
        framebufSize = VPU_GetFrameBufSize(coreIdx, strideWtl, framebufHeight, linearMapType, FORMAT_420, pDecOP->cbcrInterleave, &dram_cfg);

        for (index = fbCount; index < fbCount * 2; index++)
        {
            pvb = &pFbMem[index];
            pvb->size = framebufSize;
            if(vdi_allocate_dma_memory(coreIdx, pvb) < 0)
            {
                VLOG(ERR, "%s:%d fail to allocate frame buffer\n", __FUNCTION__, __LINE__);
                ret = BM_ERR_VDEC_NOMEM;
                goto DECODE_END;
            }
            pFrame[index].bufY = pvb->phys_addr;
            pFrame[index].bufCb = -1;
            pFrame[index].bufCr = -1;
            pFrame[index].updateFbInfo = TRUE;
        }
        fbAllocInfo.mapType = linearMapType;
        fbAllocInfo.cbcrInterleave = pDecOP->cbcrInterleave;
        fbAllocInfo.format = FORMAT_420;
        fbAllocInfo.stride = strideWtl;
        fbAllocInfo.height = framebufHeight;
        fbAllocInfo.endian = pDecOP->frameEndian;
        fbAllocInfo.lumaBitDepth = 8;
        fbAllocInfo.chromaBitDepth = 8;
        fbAllocInfo.num = fbCount;
        fbAllocInfo.type = FB_TYPE_CODEC;
        ret = VPU_DecAllocateFrameBuffer(handle, fbAllocInfo, &pFrame[fbCount]);
        if (ret != RETCODE_SUCCESS)
        {
            VLOG(ERR, "%s:%d failed to VPU_DecAllocateFrameBuffer() reason:%s\n", __FUNCTION__, __LINE__, bmvpu_dec_error_string(ret));
            ret = BM_ERR_VDEC_NOMEM;
            goto DECODE_END;
        }
    }

    VLOG(INFO, "Start SET_FRAMEBUF....\n");
    /********************************************************************************
     * SET_FRAMEBUF                                                                 *
     ********************************************************************************/
    secAxiUse.u.coda9.useBitEnable = (param->secondaryAXI >> 0) & 0x01;
    secAxiUse.u.coda9.useIpEnable = (param->secondaryAXI >> 1) & 0x01;
    secAxiUse.u.coda9.useDbkYEnable = (param->secondaryAXI >> 2) & 0x01;
    secAxiUse.u.coda9.useDbkCEnable = (param->secondaryAXI >> 3) & 0x01;
    secAxiUse.u.coda9.useOvlEnable = (param->secondaryAXI >> 4) & 0x01;
    secAxiUse.u.coda9.useBtpEnable = (param->secondaryAXI >> 5) & 0x01;
    VPU_DecGiveCommand(handle, SET_SEC_AXI, &secAxiUse);

    MaverickCache2Config(&cacheCfg, TRUE /*Decoder*/, param->cbcrInterleave,
                         param->coda9.frameCacheBypass,
                         param->coda9.frameCacheBurst,
                         param->coda9.frameCacheMerge,
                         param->mapType,
                         param->coda9.frameCacheWayShape);
    VPU_DecGiveCommand(handle, SET_CACHE_CONFIG, &cacheCfg);

    framebufStride = CalcStride(sequenceInfo.picWidth, sequenceInfo.picHeight, FORMAT_420, pDecOP->cbcrInterleave,
                                pDecOP->wtlEnable == TRUE ? LINEAR_FRAME_MAP : param->mapType, FALSE);
    ret = VPU_DecRegisterFrameBuffer(handle, pFrame, fbCount, framebufStride, framebufHeight, (int)param->mapType);
    if (ret != RETCODE_SUCCESS)
    {
        if (ret == RETCODE_MEMORY_ACCESS_VIOLATION)
            PrintMemoryAccessViolationReason(coreIdx, NULL);
        VLOG(ERR, "VPU_DecRegisterFrameBuffer failed Error reason is %s \n", bmvpu_dec_error_string(ret));
        ret = BM_ERR_FAILURE;
        goto DECODE_END;
    }
    VLOG(INFO, "Start SET_FRAMEBUF....\n");
    /********************************************************************************
     * ALLOCATE PPU FRAMEBUFFERS (When rotator, mirror or tiled2linear are enabled) *
     * NOTE: VPU_DecAllocateFrameBuffer() WITH PPU FRAMEBUFFER SHOULD BE CALLED     *
     *         AFTER VPU_DecRegisterFrameBuffer().                                    *
     ********************************************************************************/
    enablePPU = (BOOL)(param->coda9.rotate > 0 || param->coda9.mirror > 0 ||
                       pDecOP->tiled2LinearEnable == TRUE || param->coda9.enableDering == TRUE);
    if (enablePPU == TRUE)
    {
        Uint32 stridePpu;
        Uint32 sizePPUFb;
        Uint32 rotate = param->coda9.rotate;
        Uint32 rotateWidth = sequenceInfo.picWidth;
        Uint32 rotateHeight = sequenceInfo.picHeight;

        if (rotate == 90 || rotate == 270)
        {
            rotateWidth = sequenceInfo.picHeight;
            rotateHeight = sequenceInfo.picWidth;
        }
        rotateWidth = VPU_ALIGN32(rotateWidth);
        rotateHeight = VPU_ALIGN32(rotateHeight);

        stridePpu = CalcStride(rotateWidth, rotateHeight, FORMAT_420, pDecOP->cbcrInterleave, LINEAR_FRAME_MAP, FALSE);
        sizePPUFb = VPU_GetFrameBufSize(coreIdx, stridePpu, rotateHeight, LINEAR_FRAME_MAP, FORMAT_420, pDecOP->cbcrInterleave, &dram_cfg);

        for (index = 0; index < ppuFbCount; index++)
        {
            pvb = &pPPUFbMem[index];
            pvb->size = sizePPUFb;
            if(vdi_allocate_dma_memory(coreIdx, pvb) < 0)
            {
                VLOG(ERR, "%s:%d fail to allocate frame buffer\n", __FUNCTION__, __LINE__);
                ret = BM_ERR_VDEC_NOMEM;
                goto DECODE_END;
            }
            vidHandle->pPPUFrame[index].bufY = pvb->phys_addr;
            vidHandle->pPPUFrame[index].bufCb = -1;
            vidHandle->pPPUFrame[index].bufCr = -1;
            vidHandle->pPPUFrame[index].updateFbInfo = TRUE;
        }

        fbAllocInfo.mapType = LINEAR_FRAME_MAP;
        fbAllocInfo.cbcrInterleave = pDecOP->cbcrInterleave;
        fbAllocInfo.format = FORMAT_420;
        fbAllocInfo.stride = stridePpu;
        fbAllocInfo.height = rotateHeight;
        fbAllocInfo.endian = pDecOP->frameEndian;
        fbAllocInfo.lumaBitDepth = 8;
        fbAllocInfo.chromaBitDepth = 8;
        fbAllocInfo.num = ppuFbCount;
        fbAllocInfo.type = FB_TYPE_PPU;
        if ((ret = VPU_DecAllocateFrameBuffer(handle, fbAllocInfo, vidHandle->pPPUFrame)) != RETCODE_SUCCESS)
        {
            VLOG(ERR, "%s:%d failed to VPU_DecAllocateFrameBuffer(),Error reason is :%s\n", __FUNCTION__, __LINE__, bmvpu_dec_error_string(ret));
            ret = BM_ERR_VDEC_NOMEM;
            goto DECODE_END;
        }
        // Note: Please keep the below call sequence.
        VPU_DecGiveCommand(handle, SET_ROTATION_ANGLE, (void *)&param->coda9.rotate);
        VPU_DecGiveCommand(handle, SET_MIRROR_DIRECTION, (void *)&param->coda9.mirror);
        VPU_DecGiveCommand(handle, SET_ROTATOR_STRIDE, (void *)&stridePpu);

        if ((ppuQ = Queue_Create_With_Lock(MAX_REG_FRAME, sizeof(FrameBuffer))) == NULL)
        {
            ret = BM_ERR_VDEC_NOMEM;
            goto DECODE_END;
        }
        for (index = 0; index < ppuFbCount; index++)
        {
            Queue_Enqueue(ppuQ, (void *)&(vidHandle->pPPUFrame[index]));
        }
        vidHandle->ppuQ = ppuQ;
        vidHandle->PPUFrameNum = ppuFbCount;
    }
DECODE_END:
    return ret;
}

int BMVidDecSeqInitW5(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    RetCode ret = RETCODE_FAILURE;
    DecInitialInfo initialInfo;
    Int32 compressedFbCount, linearFbCount;
    Uint32 framebufStride;
    Uint32 val;
    FrameBuffer pFrame[MAX_REG_FRAME];
    vpu_buffer_t *pFbMem;
    Int32 coreIdx;
    SecAxiUse secAxiUse;
    DecHandle handle;
    BMVidDecConfig *param;
    TestDecConfig decParam;
    DecInfo *pDecInfo;
    int index;

    VLOG(INFO, "INFO: enter seq init alloc memory\n");

    if (vidHandle != NULL && vidHandle->codecInst != NULL)
    {
        handle = vidHandle->codecInst;
        coreIdx = handle->coreIdx;
        param = &(vidHandle->decConfig);
        pFbMem = vidHandle->pFbMem;
        pDecInfo = &(handle->CodecInfo->decInfo);
    }
    else
    {
        return -1;
    }

    osal_memset(&decParam, 0x00, sizeof(TestDecConfig));

    decParam.enableCrop             = param->enableCrop;
    decParam.bitFormat              = (CodStd)param->bitFormat;
    decParam.coreIdx                = param->coreIdx;
    decParam.bitstreamMode          = param->bitstreamMode;
    decParam.enableWTL              = param->enableWTL;
    decParam.wtlMode                = param->wtlMode;
    decParam.wtlFormat              = param->wtlFormat;
    decParam.cbcrInterleave         = param->cbcrInterleave;
    decParam.nv21                   = param->nv21;
    decParam.streamEndian           = param->streamEndian;
    decParam.frameEndian            = param->frameEndian;
    decParam.mapType                = param->mapType;
//    decParam.wave.fbcMode           = param->wave.fbcMode;
    decParam.wave.bwOptimization    = param->wave.bwOptimization;
    decParam.secondaryAXI           = param->secondaryAXI;
    osal_memset(&initialInfo, 0x00, sizeof(DecInitialInfo));
    if ((ret = VPU_DecCompleteSeqInit(handle, &initialInfo)) != RETCODE_SUCCESS)
    {
        VLOG(ERR, "[ERROR] Failed to DEC_PIC_HDR(ERROR REASON: %08x) error code is 0x%x\n", initialInfo.seqInitErrReason, ret);
        goto ERR_DEC_OPEN;
    }

    if(((vidHandle->decConfig.extern_picWidth > 0) && (vidHandle->decConfig.extern_picHeight) > 0) || vidHandle->framebuf_from_user)
    {
        if((vidHandle->decConfig.extern_picWidth != initialInfo.picWidth) || (vidHandle->decConfig.extern_picHeight != initialInfo.picHeight))
        {
            VLOG(ERR, "[ERROR] The size information does not match. input width:%d pic width:%d input height:%d pic height:%d\n",
                vidHandle->decConfig.extern_picWidth, initialInfo.picWidth, vidHandle->decConfig.extern_picHeight, initialInfo.picHeight);
            vidHandle->decConfig.extern_picHeight = initialInfo.picHeight;
            vidHandle->decConfig.extern_picWidth = initialInfo.picWidth;
            vidHandle->min_framebuf_cnt = initialInfo.minFrameBufferCount;
            vidHandle->framebuf_delay = initialInfo.frameBufDelay;
            vidHandle->decStatus = BMDEC_WRONG_RESOLUTION;
            ret = -1;
            goto ERR_DEC_OPEN;
        }
    }

    /********************************************************************************
    * ALLOCATE FRAME BUFFER                                                        *
    ********************************************************************************/
    /* Set up the secondary AXI is depending on H/W configuration.
    * Note that turn off all the secondary AXI configuration
    * if target ASIC has no memory only for IP, LF and BIT.
    */
    secAxiUse.u.wave.useIpEnable    = (param->secondaryAXI&0x01) ? TRUE : FALSE;
    secAxiUse.u.wave.useLfRowEnable = (param->secondaryAXI&0x02) ? TRUE : FALSE;
    secAxiUse.u.wave.useBitEnable   = (param->secondaryAXI&0x04) ? TRUE : FALSE;
    secAxiUse.u.wave.useSclEnable   = (param->secondaryAXI&0x08) ? TRUE : FALSE;
    VLOG(INFO, "INFO: enter seq init VPU_DecGiveCommand\n");
    ret = VPU_DecGiveCommand(handle, SET_SEC_AXI, &secAxiUse);
    if (ret != RETCODE_SUCCESS)
    {
        VLOG(ERR, "VPU_DecGiveCommand[SET_SEC_AXI] failed Error code is 0x%x \n", ret);
        goto ERR_DEC_OPEN;
    }

    decParam.framebuf_from_user = 0;
    pDecInfo->framebuf_from_user = 0;
    if(vidHandle->framebuf_from_user)
    {
        if(vidHandle->min_framebuf_cnt < initialInfo.minFrameBufferCount || vidHandle->framebuf_delay < initialInfo.frameBufDelay)
        {
            VLOG(ERR, "ERROR: The number of framebuffers is less than the minimum required by the VPU. minFrameBufferCount:%d  frameBufDelayCount:%d\n",
                    initialInfo.minFrameBufferCount, initialInfo.frameBufDelay);
            vidHandle->decConfig.extern_picHeight = initialInfo.picHeight;
            vidHandle->decConfig.extern_picWidth = initialInfo.picWidth;
            vidHandle->min_framebuf_cnt = initialInfo.minFrameBufferCount;
            vidHandle->framebuf_delay = initialInfo.frameBufDelay;
            vidHandle->decStatus = BMDEC_FRAMEBUFFER_NOTENOUGH;
            ret = -1;
            goto ERR_DEC_OPEN;
        }

        initialInfo.minFrameBufferCount = vidHandle->min_framebuf_cnt;
        initialInfo.frameBufDelay = vidHandle->framebuf_delay;
        decParam.framebuf_from_user = vidHandle->framebuf_from_user;
        pDecInfo->framebuf_from_user = 1;

        for(index = 0; index < initialInfo.minFrameBufferCount + vidHandle->extraFrameBufferNum; index++)
        {
            pDecInfo->vbFbcYTbl[index] = vidHandle->pYtabMem[index];
            pDecInfo->vbFbcCTbl[index] = vidHandle->pCtabMem[index];
        }
    }
    compressedFbCount = initialInfo.minFrameBufferCount + vidHandle->extraFrameBufferNum;   // max_dec_pic_buffering
    if (compressedFbCount > MAX_FRAMEBUFFER_COUNT) {
        compressedFbCount = MAX_FRAMEBUFFER_COUNT;
        vidHandle->extraFrameBufferNum = compressedFbCount - initialInfo.minFrameBufferCount;
        if (vidHandle->extraFrameBufferNum <= 0){
            VLOG(ERR, "bitstream error: frame buffering count %d is more than maxium DPB count!\n", initialInfo.minFrameBufferCount);
            ret = RETCODE_INVALID_PARAM;
            goto ERR_DEC_OPEN;
        } else
            VLOG(WARN, "extra_frame_buffer_num exceeds internal maximum buffer, change to %d!\n", vidHandle->extraFrameBufferNum);
    }

    if (param->enableWTL == TRUE) {
        linearFbCount = initialInfo.frameBufDelay + vidHandle->extraFrameBufferNum + 1;
        if (param->bitFormat == STD_VP9) {
            linearFbCount = compressedFbCount;
        }

        ret = VPU_DecGiveCommand(handle, DEC_SET_WTL_FRAME_FORMAT, &param->wtlFormat);
        if (ret != RETCODE_SUCCESS)
        {
            VLOG(ERR, "VPU_DecGiveCommand[DEC_SET_WTL_FRAME_FORMAT] failed Error code is 0x%x \n", ret);
            goto ERR_DEC_OPEN;
        }
    }
    else {
        linearFbCount = 0;
    }

    VLOG(INFO, "compressedFbCount=%d, linearFbCount=%d\n", compressedFbCount, linearFbCount);
    VLOG(INFO, "Start AllocateDecFrameBuffer....instIdx: %d\n", handle->instIndex);

    if (AllocateDecFrameBuffer(handle, &decParam, compressedFbCount, linearFbCount, pFrame, pFbMem, &framebufStride, vidHandle->enable_cache) == FALSE) {
        ret = -1;
        goto ERR_DEC_OPEN;
    }
//    LeaveLock(coreIdx);
    /********************************************************************************
     * REGISTER FRAME BUFFER                                                        *
     ********************************************************************************/
    VLOG(INFO, "Start VPU_DecRegisterFrameBufferEx....instIdx: %d\n", handle->instIndex);
    ret = VPU_DecRegisterFrameBufferEx(handle, pFrame, compressedFbCount, linearFbCount, framebufStride, initialInfo.picHeight, COMPRESSED_FRAME_MAP);
    if(ret != RETCODE_SUCCESS) {
        if (ret == RETCODE_MEMORY_ACCESS_VIOLATION) {
            EnterLock(coreIdx);
            PrintMemoryAccessViolationReason(coreIdx, NULL);
            LeaveLock(coreIdx);
        }

        VLOG(ERR, "VPU_DecRegisterFrameBuffer failed Error code is 0x%x \n", ret );
        goto ERR_DEC_OPEN;
    }

    val = (param->bitFormat == STD_HEVC) ? SEQ_CHANGE_ENABLE_ALL_HEVC : SEQ_CHANGE_ENABLE_ALL_AVC;
    ret = VPU_DecGiveCommand(handle, DEC_SET_SEQ_CHANGE_MASK, (void*)&val);
    if (ret != RETCODE_SUCCESS)
    {
        VLOG(ERR, "VPU_DecGiveCommand[DEC_SET_SEQ_CHANGE_MASK] failed Error code is 0x%x \n", ret);
        goto ERR_DEC_OPEN;
    }

	/*  set correct tid for hevc */
#if 1
    if (param->bitFormat == STD_HEVC){
	    val = -1;
		ret = VPU_DecGiveCommand(handle, DEC_SET_TARGET_TEMPORAL_ID, (void*)&val);
		if (ret != RETCODE_SUCCESS){
			VLOG(ERR, "VPU_DecGiveCOmmand[DEC_SET_TARGET_TEMPORAL_ID] failed error code is 0x%x \n", ret);
			goto ERR_DEC_OPEN;
		}
    }
#endif
    VLOG(INFO, "bmvpu_dec_seq_init END\n");
ERR_DEC_OPEN:
    return ret;
}

static int fill_ringbuffer(unsigned char *buf, int feedingSize, PhysicalAddress *pWrPtr, DecHandle decHandle, PkgInfo *pkginfo)
{
    PhysicalAddress wrPtr = *pWrPtr;
    Uint32 rightSize = 0, leftSize = feedingSize;
    int coreIdx = decHandle->coreIdx;

    if ((wrPtr + feedingSize) >= (decHandle->CodecInfo->decInfo.streamBufEndAddr))
    {
        PhysicalAddress endAddr = decHandle->CodecInfo->decInfo.streamBufEndAddr;
        rightSize = endAddr - wrPtr;
        leftSize = (wrPtr + feedingSize) - endAddr;
        if (rightSize > 0)
        {
            VpuWriteMem(coreIdx, wrPtr, buf, rightSize, (int)decHandle->CodecInfo->decInfo.openParam.streamEndian);
        }
        wrPtr = decHandle->CodecInfo->decInfo.streamBufStartAddr;
        pkginfo->flag = 1;

    }
        //VLOG(INFO, "VpuWriteMem: %llx\n", vidStream.buf + rightSize);
        //VLOG(INFO, "wrPtr: 0x%llx, leftSize: %d, coreIdx: %d, \n", wrPtr, leftSize, coreIdx);
    if(leftSize>0) {
        VpuWriteMem(decHandle->coreIdx, wrPtr, buf+rightSize, leftSize, (int)decHandle->CodecInfo->decInfo.openParam.streamEndian);
        wrPtr += leftSize;
    }
    *pWrPtr = wrPtr;
    return 0;
}
#ifdef _WIN32
// param u64* sec  1960-now sec
// param u64* usec 1960-now nsec
// param u64* msec 1960-now msec
static int s_gettimeofday(u64* sec, u64* usec, u64* msec, void* tzp)
{
    FILETIME s_time;
    GetSystemTimeAsFileTime(&s_time);
    u64 curr = ((u64)s_time.dwLowDateTime) + (((u64)s_time.dwHighDateTime) << 32LL);
    *sec = curr / 10 / 1000 / 1000;
    *usec = curr / 10;
    *msec = curr / 10 / 1000;
    return (0);
}
#endif
BMVidDecRetStatus bmvpu_dec_decode(BMVidCodHandle vidCodHandle, BMVidStream vidStream)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    Int32 feedingSize = vidStream.header_size + vidStream.length;
    PhysicalAddress wrPtr;
    Uint32 room;
    DecHandle decHandle;
#ifdef VID_PERFORMANCE_TEST
    int64_t start_pts = 0, end_pts = 0;
    struct timeval tv;
#ifdef __linux__
    if(vidHandle->perf != 0 && gettimeofday(&tv, NULL) == 0) {
        start_pts = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    }
#elif _WIN32
    u64 sec = 0;
    u64 usec = 0;
    u64 msec = 0;
    if (vidHandle->perf != 0 && s_gettimeofday(&sec,&usec,&msec, NULL) == 0) {
        start_pts = usec;
    }
#endif

#endif
    if(vidHandle->decStatus == BMDEC_HUNG) {
        VLOG(ERR, "vpu decode failed ....\n");
        return BM_ERR_VDEC_ERR_HUNG;
    }

    if(vidHandle->decStatus<BMDEC_UNINIT || vidHandle->endof_flag>=BMDEC_START_CLOSE) {
        VLOG(ERR, "decoder status error..\n");
        return BM_ERR_VDEC_SYS_NOTREADY;
    }

    if(vidHandle->decStatus == BMDEC_FRAMEBUFFER_NOTENOUGH) {
        VLOG(ERR, "the frame buffer count set is wrong\n");
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }

    if(vidHandle->decStatus == BMDEC_WRONG_RESOLUTION) {
        VLOG(ERR, "width or height which user set is wrong\n");
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }
/*
    if(vidHandle->endof_flag==BMDEC_START_GET_ALLFRAME && vidHandle->decStatus!=BMDEC_STOP)
        return RETCODE_FAILURE;
    */

    if (feedingSize <= 0 || vidStream.buf == NULL || vidHandle == NULL || vidHandle->codecInst == NULL) {

        VLOG(ERR, "coreIdx=%d,feeding size is negative value: %d\n", vidHandle->codecInst->coreIdx,feedingSize);
        return BM_ERR_VDEC_FAILURE;
    }

    //check input queue
    if(Queue_Is_Full(vidHandle->inputQ) || Queue_Is_Full(vidHandle->inputQ2))
       return BM_ERR_VDEC_BUF_FULL;

    //VLOG(INFO, "input queue count: %d\n", Queue_Get_Cnt(vidHandle->inputQ));

    decHandle = vidHandle->codecInst;

    if(vidHandle->endof_flag>=BMDEC_START_GET_ALLFRAME && vidHandle->decStatus != BMDEC_STOP)
        return BM_ERR_VDEC_FAILURE;

    if(vidHandle->decStatus == BMDEC_STOP && (vidHandle->endof_flag < BMDEC_START_REWIND || vidHandle->endof_flag > BMDEC_START_FLUSH)) {
        return BM_ERR_VDEC_SYS_NOTREADY;
    }

    if (feedingSize > 0) {
        PkgInfo pkginfo = {0};

        wrPtr = vidHandle->streamWrAddr;//pDecInfo->streamWrPtr;


        room = VPU_DecGetBitstreamBufferRoom(decHandle, wrPtr);

        if ((Int32)room < feedingSize)
        {
            if (decHandle->CodecInfo->decInfo.openParam.bitstreamMode == BS_MODE_PIC_END
                && wrPtr > decHandle->CodecInfo->decInfo.streamRdPtr && decHandle->CodecInfo->decInfo.streamRdPtr - decHandle->CodecInfo->decInfo.streamBufStartAddr > feedingSize)
            {
                wrPtr = decHandle->CodecInfo->decInfo.streamBufStartAddr;

                pkginfo.flag = 1;
                pkginfo.len = decHandle->CodecInfo->decInfo.streamBufEndAddr - wrPtr; //skip the empty size.
            }
            else
                return BM_ERR_VDEC_NOBUF;
        }

        if (vidHandle->endof_flag == BMDEC_START_GET_ALLFRAME || vidHandle->endof_flag == BMDEC_START_FLUSH)
            vidHandle->endof_flag = BMDEC_START_REWIND;
        else if (vidHandle->endof_flag == BMDEC_START_REWIND){
            if (vidHandle->decStatus == BMDEC_DECODING)
                vidHandle->endof_flag = BMDEC_START_DECODING;
            else if (vidHandle->decStatus == BMDEC_STOP){
                //osal_msleep(1);
            }
        } else
            vidHandle->endof_flag = BMDEC_START_DECODING;

        pkginfo.rd = wrPtr;
        pkginfo.len += feedingSize;

        if(vidStream.header_size > 0) {
            fill_ringbuffer(vidStream.header_buf, vidStream.header_size, &wrPtr, decHandle, &pkginfo);
        }

        fill_ringbuffer(vidStream.buf, vidStream.length, &wrPtr, decHandle, &pkginfo);
        vidHandle->streamWrAddr = wrPtr;

#ifdef VID_PERFORMANCE_TEST
        if(vidHandle->perf != 0) {
#ifdef __linux__
            if(gettimeofday(&tv, NULL) == 0) {
                end_pts = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
            }
#elif _WIN32
            u64 sec = 0;
            u64 usec = 0;
            u64 msec = 0;
            if (s_gettimeofday(&sec,&usec, &msec, NULL) == 0) {
                end_pts = usec;
            }
#endif

            if(vidHandle->max_time < end_pts - start_pts)
                vidHandle->max_time = end_pts - start_pts;
            if(vidHandle->min_time > end_pts - start_pts)
                vidHandle->min_time = end_pts - start_pts;
            vidHandle->total_time += end_pts - start_pts;
            vidHandle->dec_idx += 1;
            if(vidHandle->dec_idx > 0 && (vidHandle->dec_idx % 1000 == 0))
                printf("COPY STREAM, core : %d, inst : %d, max_time : %ld us, min_time : %ld us, ave_time : %ld us.\n",
                VPU_HANDLE_CORE_INDEX(decHandle), vidHandle->codecInst->instIndex, vidHandle->max_time, vidHandle->min_time, vidHandle->total_time/vidHandle->dec_idx);
        }

#endif
//        VLOG(INFO, "rdPtr: 0x%llx, wrPtr: 0x%llx, header: %d, feeding: %d\n", pkginfo.rd, wrPtr, vidStream.header_size, feedingSize);

        osal_cond_lock(vidHandle->inputCond);
        Queue_Enqueue(vidHandle->inputQ2, &(pkginfo.len));
        pkginfo.len = feedingSize;
        pkginfo.pts = vidStream.pts;
        pkginfo.dts = vidStream.dts;
        Queue_Enqueue(vidHandle->inputQ, &pkginfo);
        osal_cond_signal(vidHandle->inputCond);
        vidHandle->isStreamBufFilled = 1;
        osal_cond_unlock(vidHandle->inputCond);
#define BMVID_DEBUG_DUMP_STREAM_FRAME_NUM
#ifdef BMVID_DEBUG_DUMP_STREAM_FRAME_NUM
        //char *debug_env = getenv("BMVID_DEBUG_DUMP_STREAM_FRAME_NUM");
#ifndef BM_PCIE_MODE
        Uint32 coreIdx = VPU_HANDLE_CORE_INDEX(decHandle);
        int dump_stream_num = VPU_GetInNum(coreIdx, vidHandle->codecInst->instIndex);
#else
        char *debug_env = getenv("BMVID_PCIE_DUMP_STREAM_NUM");
        int dump_stream_num = 0;
        if(debug_env != NULL)
            dump_stream_num = atoi(debug_env);
#endif
        if(dump_stream_num > 0) {
            if((vidHandle->dump_frame_num>0) && ((vidHandle->dump_frame_num%dump_stream_num)==0) && (vidHandle->fp_stream != NULL)) {
                fclose(vidHandle->fp_stream);
                vidHandle->fp_stream = NULL;
                vidHandle->dump_frame_num = 0;
                if(vidHandle->file_flag == 0)
                    vidHandle->file_flag = 1;
                else
                    vidHandle->file_flag = 0;
            }
            if(vidHandle->fp_stream == NULL) {
                char filename[255]={0};
                sprintf(filename, "/data/core_%dinst%d_stream%d.bin", vidHandle->codecInst->coreIdx, vidHandle->codecInst->instIndex, vidHandle->file_flag);
//                printf("open file: %s, dump stream frame num: %d\n", filename, dump_stream_num);
                vidHandle->fp_stream = fopen(filename, "wb+");
            }
            if(vidHandle->fp_stream != NULL) {
                if(vidStream.header_size > 0) {
                    fwrite(vidStream.header_buf, 1, vidStream.header_size, vidHandle->fp_stream);
                }
                if(vidStream.length > 0)
                    fwrite(vidStream.buf, 1, vidStream.length, vidHandle->fp_stream);
                fflush(vidHandle->fp_stream);
            }
            vidHandle->dump_frame_num += 1;
        }
#endif
    }

    return BM_SUCCESS;
}

BMVidDecRetStatus bmvpu_dec_get_output(BMVidCodHandle vidCodHandle, BMVidFrame *frame)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    BMVidFrame *bmFrame = NULL;
    if(frame == NULL) {
        VLOG(ERR, "the frame buffer invalid.\n");
        return BM_ERR_VDEC_NOMEM;
    }

    if(vidHandle == NULL) {
        VLOG(ERR, "Vdec device fd error.\n");
        return BM_ERR_VDEC_UNEXIST;
    }

    if(vidHandle->decStatus > BMDEC_UNINIT && vidHandle->endof_flag < BMDEC_START_CLOSE)
    {
        bmFrame = (BMVidFrame*)Queue_Dequeue(vidHandle->displayQ);
        if(bmFrame && bmFrame->frameIdx < 32)
            vidHandle->cache_bmframe[bmFrame->frameIdx] = *bmFrame;
    }
    bmFrame = (bmFrame == NULL ?  bmFrame : &(vidHandle->cache_bmframe[bmFrame->frameIdx]));

    if(bmFrame == NULL)
        return BM_ERR_VDEC_FAILURE;
    else
        osal_memcpy(frame, bmFrame, sizeof(BMVidFrame));

    return BM_SUCCESS;
}

BMVidDecRetStatus bmvpu_dec_clear_output(BMVidCodHandle vidCodHandle, BMVidFrame *frame)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    FrameBuffer frameBuffer;
    Uint32 index;
    Uint32* ptr;
    int clrFrmIndex = -1;

    if(checkHandle(vidCodHandle) && vidHandle->decStatus>BMDEC_UNINIT && vidHandle->endof_flag < BMDEC_START_CLOSE)
    {
        if(vidHandle->enablePPU) {
            int i;
            for( i=0; i<vidHandle->PPUFrameNum; i++) {
                if(vidHandle->pPPUFrame[i].myIndex == frame->frameIdx) {
                    frameBuffer = vidHandle->pPPUFrame[i];
                    break;
                }
            }
            if(frame->frameIdx != frameBuffer.myIndex) {
                VLOG(ERR, "can't get frame idx!!!!\n");
                return BM_ERR_VDEC_ILLEGAL_PARAM;
            }
        }
        else {
            VPU_DecGetFrameBuffer(vidHandle->codecInst, frame->frameIdx, &frameBuffer);
        }
        //frameBuffer.sequenceNo = frame.sequenceNo;
//        Queue_Enqueue(vidHandle->freeQ, (void*)(&frameBuffer));
        osal_cond_lock(vidHandle->outputCond);
        if (vidHandle->enablePPU == TRUE)
        {
            Queue_Enqueue(vidHandle->ppuQ, (void *)(&frameBuffer));
        }
        else
        {
            if (vidHandle->enable_decode_order) {
                clrFrmIndex = vidHandle->decode_index_map[frameBuffer.myIndex];
            } else {
                clrFrmIndex = frameBuffer.myIndex;
            }
            if (clrFrmIndex >= 0)
                VPU_DecClrDispFlag(vidHandle->codecInst, clrFrmIndex);
        }

        ptr = (Uint32*)Queue_Peek(vidHandle->sequenceQ);


        if (ptr && *ptr != frame->sequenceNo) {
            /* Release all framebuffers of previous sequence */
            SequenceMemInfo* p;
            index = (*ptr) % MAX_SEQUENCE_MEM_COUNT;

            p = &seqMemInfo[vidHandle->codecInst->instIndex][index];
            releasePreviousSequenceResources(vidHandle->codecInst, p->allocFbMem, &p->fbInfo, vidHandle->codecInst->CodecInfo->decInfo.framebuf_from_user);
            osal_memset(p, 0x00, sizeof(SequenceMemInfo));
            Queue_Dequeue(vidHandle->sequenceQ);
        }

        vidHandle->frameInBuffer -= 1;
        osal_cond_signal(vidHandle->outputCond);
        osal_cond_unlock(vidHandle->outputCond);

    }
    else
    {
        VLOG(ERR, "can't clear output please check it!!!!, index: %d\n", frame->frameIdx);
    }
    return BM_SUCCESS;
}

BMVidDecRetStatus bmvpu_dec_flush(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    int ret = BM_SUCCESS;
    if (vidHandle->endof_flag < BMDEC_START_GET_ALLFRAME)
    {
        int size = STREAM_END_SIZE;
        PkgInfo pkginfo;

        if(Queue_Is_Full(vidHandle->inputQ) || Queue_Is_Full(vidHandle->inputQ2))
            return BM_ERR_VDEC_BUF_FULL;
        pkginfo.flag = 0;
        pkginfo.len = 0;
        pkginfo.rd = vidHandle->streamWrAddr;
//        VLOG(INFO, "flush decoder..., core index: %d, instance index: %d\n",
//                    vidHandle->codecInst->coreIdx, vidHandle->codecInst->instIndex);
        // Now that we are done with decoding, close the opening instance.
        osal_cond_lock(vidHandle->inputCond);

        if(vidHandle->codecInst->CodecInfo->decInfo.openParam.bitstreamMode == BS_MODE_PIC_END)
        {
            ret = Queue_Enqueue(vidHandle->inputQ, &pkginfo);
        }
        else
            ret = Queue_Enqueue(vidHandle->inputQ2, &size);
        vidHandle->endof_flag = BMDEC_START_FLUSH;
        osal_cond_signal(vidHandle->inputCond);
        osal_cond_unlock(vidHandle->inputCond);
//        osal_cond_signal(vidHandle->outputCond);
    }
    VLOG(INFO, "flush decoder Done......, core index: %d, instance index: %d\n",
                    vidHandle->codecInst->coreIdx, vidHandle->codecInst->instIndex);
    return ret;
}
#define TRY_CLOSE_COUNT 500000
BMVidDecRetStatus bmvpu_dec_delete(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    int coreIdx = -1;
    int try_count = 0;
    int instIdx = -1;
#ifdef _WIN32
        timeEndPeriod(1);
#endif
    if (vidHandle == NULL || vidHandle->codecInst == NULL)
    {
        VLOG(ERR, "handle null in dec delete!!!\n");
        return BM_ERR_VDEC_UNEXIST;
    }
    coreIdx = vidHandle->codecInst->coreIdx;
    instIdx = vidHandle->codecInst->instIndex;
    VLOG(INFO, "close decoder instance, core index: %d, instance index: %d\n",
                coreIdx, instIdx);

//    vidHandle->isStop = 1;
    if(vidHandle->endof_flag < BMDEC_START_FLUSH) {
        bmvpu_dec_flush(vidCodHandle);
    }

    osal_cond_lock(vidHandle->outputCond);
    vidHandle->endof_flag = BMDEC_START_CLOSE;
    osal_cond_signal(vidHandle->outputCond);
    osal_cond_unlock(vidHandle->outputCond);
    while(vidHandle->decStatus != BMDEC_CLOSED) {
        if(try_count%1000 == 0)
            VLOG(INFO, "waiting for stop decoder!!!!!!, coreIdx: %d, instIdx: %d\n", coreIdx, instIdx);

        osal_msleep(USLEEP_CLOCK / 1000);

        try_count++;
        if(try_count > TRY_CLOSE_COUNT) {
            osal_cond_lock(vidHandle->inputCond);
            osal_cond_lock(vidHandle->outputCond);
            vidHandle->endof_flag = BMDEC_START_CLOSE;
            osal_cond_signal(vidHandle->inputCond);
            osal_cond_signal(vidHandle->outputCond);
            osal_cond_unlock(vidHandle->outputCond);
            osal_cond_unlock(vidHandle->inputCond);
            printf("-----close timeout: coreIdx: %d, instIdx: %d\n", coreIdx, instIdx);
            osal_thread_cancel(vidHandle->processThread);
            return BM_ERR_VDEC_BUSY;
        }
    }

    if (vidHandle->processThread != NULL){
        osal_thread_join(vidHandle->processThread, NULL);
    }
#ifdef TRY_FLOCK
    if(s_flock_fd[coreIdx] != -1) {
        close(s_flock_fd[coreIdx]);
        s_flock_fd[coreIdx] = -1;
    }
    if(s_disp_flock_fd[coreIdx] != -1) {
        close(s_disp_flock_fd[coreIdx]);
        s_disp_flock_fd[coreIdx] = -1;
    }
#endif
    if(vidHandle->fp_stream != NULL) {
        fclose(vidHandle->fp_stream);
    }
    if (vidHandle->inputCond)
        osal_cond_destroy(vidHandle->inputCond);
    if (vidHandle->outputCond)
        osal_cond_destroy(vidHandle->outputCond);
    if(vidHandle)
        osal_free(vidHandle);
    VLOG(INFO, "core, %d, inst, %d closed!\n", coreIdx, instIdx);
    return BM_SUCCESS;
}

BMDecStatus bmvpu_dec_get_status(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    if(checkHandle(vidCodHandle)!=0)
        return vidHandle->decStatus;
    else
    {
        VLOG(ERR, "handle err, 0x%p\n", vidCodHandle);
        return BMDEC_CLOSED;
    }
}

int bmvpu_dec_get_stream_buffer_empty_size(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    DecHandle decHandle;

    if (vidHandle == NULL || vidHandle->codecInst == NULL)
    {
        VLOG(ERR, "point is null. in get stream buffer size.\n");
        return BM_ERR_VDEC_UNEXIST;
    }

    decHandle = vidHandle->codecInst;

    return VPU_DecGetBitstreamBufferRoom(decHandle, vidHandle->streamWrAddr);
}

BMVidDecRetStatus bmvpu_dec_get_caps(BMVidCodHandle vidCodHandle, BMVidStreamInfo *streamInfo)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    DecInitialInfo *pInitInfo = NULL;
    if (vidHandle == NULL || vidHandle->seqInitFlag < 1 || streamInfo == NULL)
        return BM_ERR_VDEC_UNEXIST;
    osal_memset(streamInfo, 0, sizeof(BMVidStreamInfo));
    pInitInfo = &(vidHandle->codecInst->CodecInfo->decInfo.initialInfo);
    streamInfo->picWidth = pInitInfo->picWidth;
    streamInfo->picHeight = pInitInfo->picHeight;
    streamInfo->aspectRateInfo = pInitInfo->aspectRateInfo;
    streamInfo->bitRate = pInitInfo->bitRate;
    streamInfo->frameBufDelay = pInitInfo->frameBufDelay;
    streamInfo->fRateNumerator = pInitInfo->fRateNumerator;
    streamInfo->fRateDenominator = pInitInfo->fRateDenominator;
    streamInfo->interlace = pInitInfo->interlace;
    streamInfo->chromaBitdepth = pInitInfo->chromaBitdepth;
    streamInfo->profile = pInitInfo->profile;
    streamInfo->level = pInitInfo->level;
    streamInfo->lumaBitdepth = pInitInfo->lumaBitdepth;
    streamInfo->maxNumRefFrmFlag = pInitInfo->maxNumRefFrmFlag;
    streamInfo->maxNumRefFrm = pInitInfo->maxNumRefFrm;
    streamInfo->minFrameBufferCount = pInitInfo->minFrameBufferCount;
    streamInfo->picCropRect.bottom = pInitInfo->picCropRect.bottom;
    streamInfo->picCropRect.top = pInitInfo->picCropRect.top;
    streamInfo->picCropRect.left = pInitInfo->picCropRect.left;
    streamInfo->picCropRect.right = pInitInfo->picCropRect.left;
    return BM_SUCCESS;
}

BMVidDecRetStatus bmvpu_dec_get_all_frame_in_buffer(BMVidCodHandle vidCodHandle)
{

    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;

//    VPU_DecUpdateBitstreamBuffer(vidHandle->codecInst, 1); // let f/w to know stream end condition in bitstream buffer. force to know that bitstream buffer will be empty.

    int size = STREAM_END_SIZE;
    PkgInfo pkginfo;
    pkginfo.flag = 0;
    pkginfo.len = 0;
    pkginfo.rd = vidHandle->streamWrAddr;
    pkginfo.pts = pkginfo.dts = 0; // should be ((int64_t)UINT64_C(0x8000000000000000)), but here is eof position, we use 0 in packet to flush out

    osal_cond_lock(vidHandle->inputCond);
    if(vidHandle->codecInst->CodecInfo->decInfo.openParam.bitstreamMode == BS_MODE_PIC_END)
    {
        Queue_Enqueue(vidHandle->inputQ, &pkginfo);
    }
    else
        Queue_Enqueue(vidHandle->inputQ2, &size);
    vidHandle->endof_flag = BMDEC_START_GET_ALLFRAME;
    osal_cond_signal(vidHandle->inputCond);
    osal_cond_unlock(vidHandle->inputCond);
//    osal_cond_signal(vidHandle->outputCond);
//    VLOG(INFO, "get all frame buffer from vpu!\n");
    return BM_SUCCESS;
}

int bmvpu_dec_get_all_empty_input_buf_cnt(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    if(!vidHandle || vidHandle->decStatus>=BMDEC_CLOSE)
        return BM_ERR_VDEC_UNEXIST;
    else
        return INPUT_QUEUE_LEN - Queue_Get_Cnt(vidHandle->inputQ);
}

int bmvpu_dec_get_pkt_in_buf_cnt(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    if(!vidHandle || vidHandle->decStatus>=BMDEC_CLOSE)
        return BM_ERR_VDEC_UNEXIST;
    else {
        if(vidHandle->codecInst->CodecInfo->decInfo.openParam.bitstreamMode == BS_MODE_PIC_END)
            return Queue_Get_Cnt(vidHandle->inputQ);
        else
            return Queue_Get_Cnt(vidHandle->inputQ) - MIN_WAITING_QUEUE_LEN - 1;
    }
}

BMVidDecRetStatus bmvpu_dec_reset(int devIdx, int coreIdx)
{
    int offset;
    int core;

    if (devIdx < 0 || devIdx > MAX_PCIE_BOARD_NUM-1) {
        fprintf(stderr, "Invalid device index %d. [0, %d]\n",
                devIdx, MAX_PCIE_BOARD_NUM-1);
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }

    if (coreIdx < -1 || coreIdx > MAX_NUM_VPU_CORE_CHIP-1) {
        fprintf(stderr, "Invalid core index %d\n", coreIdx);
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }

    offset = MAX_NUM_VPU_CORE_CHIP*devIdx;

    //sem_unlink(VID_OPEN_SEM_NAME);
    if (coreIdx == -1) {
#ifdef TRY_SEM_MUTEX
        for(core=offset; core<offset+MAX_NUM_VPU_CORE_CHIP; core++) {
            char sem_name[255];
            char disp_sem_name[255];
            sprintf(sem_name, "%s%d", CORE_SEM_NAME, core);
            sprintf(disp_sem_name, "%s%d", CORE_DISP_SEM_NEME, core);
            printf("sem_name unlink : %s\n", sem_name);
            sem_unlink(sem_name);
            sem_unlink(disp_sem_name);
        }
        sem_unlink(VID_OPEN_SEM_NAME);
#endif

        for(core=offset; core<offset+MAX_NUM_VPU_CORE_CHIP; core++) {
            VLOG(ERR,"HW reset device %d, core %d.\n",
                 devIdx, core-offset);

            VPU_HWReset(core);
        }
    } else {
        core = coreIdx + offset;

#ifdef TRY_SEM_MUTEX
        char sem_name[255];
        char disp_sem_name[255];
        sprintf(sem_name, "%s%d", CORE_SEM_NAME, core);
        sprintf(disp_sem_name, "%s%d", CORE_DISP_SEM_NEME, core);
        printf("sem_name unlink : %s\n", sem_name);
        sem_unlink(sem_name);
        sem_unlink(disp_sem_name);
        sem_unlink(VID_OPEN_SEM_NAME);
#endif

        VLOG(ERR,"HW reset device %d, core %d.\n",
             devIdx, core-offset);
        VPU_HWReset(core);
    }

    return RETCODE_SUCCESS;
}

int bmvpu_dec_dump_stream(BMVidCodHandle vidCodHandle, unsigned char *p_stream, int size)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    int len = 0;
    if(!vidHandle || vidHandle->decStatus>=BMDEC_CLOSE) {
        VLOG(ERR, "handle or status error...\n");
        return BM_ERR_VDEC_UNEXIST;
    }

    if(p_stream == NULL) {
        VLOG(ERR, "stream buffer is null...\n");
        return BM_ERR_VDEC_NULL_PTR;
    }
    len = (int)(vidHandle->streamWrAddr - vidHandle->vbStream.phys_addr);

    if(len < 0 || len > size)
    {
        VLOG(ERR, "stream buffer len : %d, p_stream size : %d. maybe too small...\n", len, size);
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }

    VpuReadMem(vidHandle->codecInst->coreIdx, vidHandle->vbStream.phys_addr, p_stream, len, vidHandle->decConfig.streamEndian);

    return len;
}

int bmvpu_dec_get_inst_idx(BMVidCodHandle vidCodHandle)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;

    if(!vidHandle || vidHandle->decStatus>=BMDEC_CLOSE) {
        VLOG(ERR, "handle or status error...\n");
        return BM_ERR_VDEC_UNEXIST;
    }

    return (int)(vidHandle->codecInst->instIndex);
}

BMVidDecRetStatus bmvpu_dec_get_stream_info(BMVidCodHandle vidCodHandle, int* width, int* height, int* mini_fb, int* frame_delay)
{
    BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
    if(!vidHandle || vidHandle->decStatus>=BMDEC_CLOSE) {
        VLOG(ERR, "handle or status error...\n");
        return BM_ERR_VDEC_INVALID_CHNID;
    }

    if(width == NULL || height == NULL || mini_fb == NULL || frame_delay == NULL){
        VLOG(ERR, "bmvpu_dec_get_stream_info param error...\n");
        return BM_ERR_VDEC_ILLEGAL_PARAM;
    }

    *width = vidHandle->decConfig.extern_picWidth;
    *height = vidHandle->decConfig.extern_picHeight;
    *mini_fb = vidHandle->min_framebuf_cnt;
    *frame_delay =  vidHandle->framebuf_delay;

    return RETCODE_SUCCESS;
}

int bmvpu_dec_read_memory(int coreIdx, unsigned long addr, unsigned char *data, int len, int endian)
{
    return vdi_read_memory(coreIdx, addr, data, len, endian);
}

u64 bmvpu_dec_calc_cbcr_addr(int codec_type, u64 y_addr, int y_stride, int frame_height)
{
    // codec_type for sync with BM1688 vp9 dec.
    return y_addr + y_stride * VPU_ALIGN32(frame_height);
}
