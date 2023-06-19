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
//#include <string.h>
#include "component.h"
#ifdef __linux__
#include <pthread.h>
#elif _WIN32
#include <windows.h>
#endif

typedef enum {
    SRC_FB_TYPE_YUV,
    SRC_FB_TYPE_CFRAME
} SRC_FB_TYPE;

typedef enum {
    YUV_FEEDER_STATE_WAIT,
    YUV_FEEDER_STATE_FEEDING
} YuvFeederState;

typedef struct {
    EncHandle                   handle;
    char                        inputPath[MAX_FILE_PATH];
    EncOpenParam                encOpenParam;
    Int32                       rotAngle;
    Int32                       mirDir;
    Int32                       frameOutNum;
    Int32                       yuvMode;
    Uint32                      reconFbStride;
    Uint32                      reconFbHeight;
    Uint32                      srcFbStride;
    FrameBufferAllocInfo        srcFbAllocInfo;
#ifdef __linux__
    pthread_barrier_t           fbAllocated;
#elif _WIN32
    SYNCHRONIZATION_BARRIER	    fbAllocated;
#elif defined(PLATFORM_NON_OS)
    Int64                       fbAllocated;
#endif
    ParamEncNeedFrameBufferNum  fbCount;
    FrameBuffer                 pFbRecon[MAX_REG_FRAME];
    vpu_buffer_t                pFbReconMem[MAX_REG_FRAME];
    FrameBuffer                 pFbSrc[ENC_SRC_BUF_NUM];
    vpu_buffer_t                pFbSrcMem[ENC_SRC_BUF_NUM];
    FrameBuffer                 pFbOffsetTbl[ENC_SRC_BUF_NUM];
    vpu_buffer_t                pFbOffsetTblMem[ENC_SRC_BUF_NUM];
    YuvFeeder                   yuvFeeder;
    BOOL                        last;
    YuvFeederState              state;
    BOOL                        stateDoing;
    Int32                       feedingNum;
    BOOL                        subFrameSyncEn;
} YuvFeederContext;

static void InitYuvFeederContext(YuvFeederContext* ctx, CNMComponentConfig* componentParam)
{
    osal_memset((void*)ctx, 0, sizeof(YuvFeederContext));
    osal_strcpy(ctx->inputPath, componentParam->testEncConfig.yuvFileName);
    ctx->reconFbStride      = 0;
    ctx->reconFbHeight      = 0;
    ctx->srcFbStride        = 0;
    ctx->encOpenParam       = componentParam->encOpenParam;
    ctx->yuvMode            = componentParam->testEncConfig.yuv_mode;
    ctx->fbCount.reconFbNum = 0;
    ctx->fbCount.srcFbNum   = 0;
    ctx->frameOutNum        = componentParam->testEncConfig.outNum;
    ctx->rotAngle           = componentParam->testEncConfig.rotAngle;
    ctx->mirDir             = componentParam->testEncConfig.mirDir;
    ctx->subFrameSyncEn     = componentParam->testEncConfig.subFrameSyncEn;

    osal_memset(&ctx->srcFbAllocInfo,          0x00, sizeof(FrameBufferAllocInfo));
    osal_memset((void*)ctx->pFbRecon,          0x00, sizeof(ctx->pFbRecon));
    osal_memset((void*)ctx->pFbReconMem,       0x00, sizeof(ctx->pFbReconMem));
    osal_memset((void*)ctx->pFbSrc,            0x00, sizeof(ctx->pFbSrc));
    osal_memset((void*)ctx->pFbSrcMem,         0x00, sizeof(ctx->pFbSrcMem));
    osal_memset((void*)ctx->pFbOffsetTbl,      0x00, sizeof(ctx->pFbOffsetTbl));
    osal_memset((void*)ctx->pFbOffsetTblMem,   0x00, sizeof(ctx->pFbOffsetTblMem));
}

static CNMComponentParamRet GetParameterYuvFeeder(ComponentImpl* from, ComponentImpl* com, GetParameterCMD commandType, void* data)
{
    YuvFeederContext*    ctx = (YuvFeederContext*)com->context;
    BOOL                 result  = TRUE;
    ParamEncFrameBuffer* allocFb = NULL;

    switch(commandType) {
    case GET_PARAM_YUVFEEDER_FRAME_BUF:
#ifdef __linux__
        pthread_barrier_wait(&ctx->fbAllocated);
#elif _WIN32
        EnterSynchronizationBarrier(&ctx->fbAllocated,0);
#elif defined(PLATFORM_NON_OS)
        if (ctx->fbAllocated == 1)
			ctx->fbAllocated--;
        if (ctx->fbAllocated == 2)
	        return CNM_COMPONENT_PARAM_NOT_READY;
#endif
        allocFb = (ParamEncFrameBuffer*)data;
        allocFb->reconFb          = ctx->pFbRecon;
        allocFb->reconFbStride    = ctx->reconFbStride;
        allocFb->reconFbHeight    = ctx->reconFbHeight;
        allocFb->srcFb            = ctx->pFbSrc;
        //allocFb->srcFbAllocInfo   = ctx->srcFbAllocInfo;
        osal_memcpy(&allocFb->srcFbAllocInfo, &ctx->srcFbAllocInfo, sizeof(allocFb->srcFbAllocInfo));
        break;
    default:
        result = FALSE;
        break;
    }

    return (result == TRUE) ? CNM_COMPONENT_PARAM_SUCCESS : CNM_COMPONENT_PARAM_FAILURE;
}

static CNMComponentParamRet SetParameterYuvFeeder(ComponentImpl* from, ComponentImpl* com, SetParameterCMD commandType, void* data)
{
    BOOL result = TRUE;

    switch(commandType) {
    default:
        VLOG(ERR, "Unknown SetParameterCMD Type : %d\n", commandType);
        result = FALSE;
        break;
    }

    return (result == TRUE) ? CNM_COMPONENT_PARAM_SUCCESS : CNM_COMPONENT_PARAM_FAILURE;
}

static BOOL AllocateReconFrameBuffer(ComponentImpl* com) {
    YuvFeederContext*    ctx = (YuvFeederContext*)com->context;
    EncOpenParam         encOpenParam = ctx->encOpenParam;
    Uint32               i = 0;
    Uint32               reconFbSize   = 0;
    Uint32               reconFbStride = 0;
    Uint32               reconFbWidth;
    Uint32               reconFbHeight;

    if (ctx->encOpenParam.bitstreamFormat == STD_AVC) {
        reconFbWidth  = VPU_ALIGN16(encOpenParam.picWidth);
        reconFbHeight = VPU_ALIGN16(encOpenParam.picHeight);
    }
    else {
        reconFbWidth  = VPU_ALIGN8(encOpenParam.picWidth);
        reconFbHeight = VPU_ALIGN8(encOpenParam.picHeight);
    }

    if ((ctx->rotAngle != 0 || ctx->mirDir != 0) && !(ctx->rotAngle == 180 && ctx->mirDir == MIRDIR_HOR_VER)) {
        reconFbWidth  = VPU_ALIGN32(encOpenParam.picWidth);
        reconFbHeight = VPU_ALIGN32(encOpenParam.picHeight);
    }

    if (ctx->rotAngle == 90 || ctx->rotAngle == 270) {
        reconFbWidth  = VPU_ALIGN32(encOpenParam.picHeight);
        reconFbHeight = VPU_ALIGN32(encOpenParam.picWidth);
    }

    /* Allocate framebuffers for recon. */
    reconFbStride = CalcStride(reconFbWidth, reconFbHeight, (FrameBufferFormat)encOpenParam.outputFormat, encOpenParam.cbcrInterleave, COMPRESSED_FRAME_MAP, FALSE);
    reconFbSize   = VPU_GetFrameBufSize(encOpenParam.coreIdx, reconFbStride, reconFbHeight, COMPRESSED_FRAME_MAP, (FrameBufferFormat)encOpenParam.outputFormat, encOpenParam.cbcrInterleave, NULL);
    vdi_lock(encOpenParam.coreIdx);
    for (i = 0; i < ctx->fbCount.reconFbNum; i++) {
        ctx->pFbReconMem[i].size = reconFbSize;
        if (vdi_allocate_dma_memory(encOpenParam.coreIdx, &ctx->pFbReconMem[i]) < 0) {
            vdi_unlock(encOpenParam.coreIdx);
            VLOG(ERR, "fail to allocate recon buffer\n");
            return FALSE;
        }
        ctx->pFbRecon[i].bufY  = ctx->pFbReconMem[i].phys_addr;
        ctx->pFbRecon[i].bufCb = (PhysicalAddress) - 1;
        ctx->pFbRecon[i].bufCr = (PhysicalAddress) - 1;
        ctx->pFbRecon[i].size  = reconFbSize;
        ctx->pFbRecon[i].updateFbInfo = TRUE;
    }
    vdi_unlock(encOpenParam.coreIdx);

    ctx->reconFbStride = reconFbStride;
    ctx->reconFbHeight = reconFbHeight;

    return TRUE;
}

static BOOL AllocateYuvBuffer(ComponentImpl* com, Uint32 srcFbWidth, Uint32 srcFbHeight)
{
    YuvFeederContext*    ctx = (YuvFeederContext*)com->context;
    EncOpenParam         encOpenParam = ctx->encOpenParam;
    Uint32               i = 0;
    FrameBufferAllocInfo srcFbAllocInfo;
    Uint32               srcFbSize    = 0;
    Uint32               srcFbStride  = 0;

    osal_memset(&srcFbAllocInfo,   0x00, sizeof(FrameBufferAllocInfo));

    /* Allocate framebuffers for source. */
    srcFbStride = CalcStride(srcFbWidth, srcFbHeight, (FrameBufferFormat)encOpenParam.srcFormat, encOpenParam.cbcrInterleave, LINEAR_FRAME_MAP, FALSE);
    srcFbSize   = VPU_GetFrameBufSize(encOpenParam.coreIdx, srcFbStride, srcFbHeight, LINEAR_FRAME_MAP, (FrameBufferFormat)encOpenParam.srcFormat, encOpenParam.cbcrInterleave, NULL);

    srcFbAllocInfo.mapType = LINEAR_FRAME_MAP;
    srcFbAllocInfo.format  = (FrameBufferFormat)encOpenParam.srcFormat;
    srcFbAllocInfo.cbcrInterleave = encOpenParam.cbcrInterleave;
    srcFbAllocInfo.stride  = srcFbStride;
    srcFbAllocInfo.height  = srcFbHeight;
    srcFbAllocInfo.endian  = encOpenParam.sourceEndian;
    srcFbAllocInfo.type    = FB_TYPE_PPU;
    srcFbAllocInfo.num     = ctx->fbCount.srcFbNum;
    srcFbAllocInfo.nv21    = encOpenParam.nv21;

    vdi_lock(encOpenParam.coreIdx);
    for (i = 0; i < ctx->fbCount.srcFbNum; i++) {
        ctx->pFbSrcMem[i].size = srcFbSize;
        if (vdi_allocate_dma_memory(encOpenParam.coreIdx, &ctx->pFbSrcMem[i]) < 0) {
            vdi_unlock(encOpenParam.coreIdx);
            VLOG(ERR, "fail to allocate src buffer\n");
            return FALSE;
        }
        ctx->pFbSrc[i].bufY  = ctx->pFbSrcMem[i].phys_addr;
        ctx->pFbSrc[i].bufCb = (PhysicalAddress) - 1;
        ctx->pFbSrc[i].bufCr = (PhysicalAddress) - 1;
        ctx->pFbSrc[i].updateFbInfo = TRUE;

        VLOG(ERR, "FbSrc %d: bufY 0x%lx size 0x%lx stride 0x%lx\n", i, ctx->pFbSrc[i].bufY, srcFbSize, srcFbStride);
    }
    vdi_unlock(encOpenParam.coreIdx);

    ctx->srcFbStride      = srcFbStride;
    //ctx->srcFbAllocInfo   = srcFbAllocInfo;
    osal_memcpy(&ctx->srcFbAllocInfo, &srcFbAllocInfo, sizeof(srcFbAllocInfo));
    return TRUE;
}

static BOOL AllocateCframeDataBuffer(ComponentImpl* com, Uint32 srcFbWidth, Uint32 srcFbHeight)
{
    YuvFeederContext*    ctx = (YuvFeederContext*)com->context;
    EncOpenParam         encOpenParam = ctx->encOpenParam;
    Uint32               i = 0;
    FrameBufferAllocInfo srcFbAllocInfo;
    DRAMConfig           compOption;
    TiledMapType         srcFbMapType = TILED_MAP_TYPE_MAX;
    Uint32               srcFbSize    = 0;
    Uint32               srcFbStride  = 0;

    osal_memset(&srcFbAllocInfo,   0x00, sizeof(FrameBufferAllocInfo));
    osal_memset(&compOption,       0x00, sizeof(DRAMConfig));

    if (encOpenParam.cframe50_422 == TRUE)
    {
        if (encOpenParam.cframe50LosslessEnable == TRUE) {
            if (encOpenParam.srcBitDepth == 8) {
                srcFbMapType = COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_8BIT;
            }
            else if (encOpenParam.srcBitDepth == 10) {
                srcFbMapType = COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_10BIT;
            }
        }
        else {
            srcFbMapType = COMPRESSED_FRAME_MAP_V50_LOSSY_422;
        }
    }
    else {
        if (encOpenParam.cframe50LosslessEnable == TRUE) {
            if (encOpenParam.srcBitDepth == 8) {
                srcFbMapType = COMPRESSED_FRAME_MAP_V50_LOSSLESS_8BIT;
            }
            else if (encOpenParam.srcBitDepth == 10) {
                srcFbMapType = COMPRESSED_FRAME_MAP_V50_LOSSLESS_10BIT;
            }
        }
        else {
            srcFbMapType = COMPRESSED_FRAME_MAP_V50_LOSSY;
        }
    }

    // tx16y, tx16c should be required for calculating Luma / Chroma buffer size of Cframe50 lossy mode.
    compOption.tx16y = encOpenParam.cframe50Tx16Y;
    compOption.tx16c = encOpenParam.cframe50Tx16C;

    /* Allocate framebuffers for source. */
    srcFbStride = CalcStride(srcFbWidth, srcFbHeight, (FrameBufferFormat)encOpenParam.srcFormat, encOpenParam.cbcrInterleave, srcFbMapType, FALSE);
    srcFbSize   = VPU_GetFrameBufSize(encOpenParam.coreIdx, srcFbStride, srcFbHeight, srcFbMapType, (FrameBufferFormat)encOpenParam.srcFormat, encOpenParam.cbcrInterleave, &compOption);

    srcFbAllocInfo.mapType = srcFbMapType;
    srcFbAllocInfo.format  = (FrameBufferFormat)encOpenParam.srcFormat;
    srcFbAllocInfo.cbcrInterleave = encOpenParam.cbcrInterleave;
    srcFbAllocInfo.stride  = srcFbStride;
    srcFbAllocInfo.height  = srcFbHeight;
    srcFbAllocInfo.endian  = encOpenParam.sourceEndian;
    srcFbAllocInfo.type    = FB_TYPE_PPU;
    srcFbAllocInfo.num     = ctx->fbCount.srcFbNum;
    srcFbAllocInfo.nv21    = encOpenParam.nv21;

    vdi_lock(encOpenParam.coreIdx);
    for (i = 0; i < ctx->fbCount.srcFbNum; i++) {
        ctx->pFbSrcMem[i].size = srcFbSize;
        if (vdi_allocate_dma_memory(encOpenParam.coreIdx, &ctx->pFbSrcMem[i]) < 0) {
            vdi_unlock(encOpenParam.coreIdx);
            VLOG(ERR, "fail to allocate src buffer\n");
            return FALSE;
        }
        ctx->pFbSrc[i].bufY  = ctx->pFbSrcMem[i].phys_addr;
        ctx->pFbSrc[i].bufCb = (PhysicalAddress) - 1;
        ctx->pFbSrc[i].bufCr = (PhysicalAddress) - 1;
        ctx->pFbSrc[i].updateFbInfo = TRUE;
    }
    vdi_unlock(encOpenParam.coreIdx);

    ctx->srcFbStride      = srcFbStride;
    //ctx->srcFbAllocInfo   = srcFbAllocInfo;
    osal_memcpy(&ctx->srcFbAllocInfo, &srcFbAllocInfo, sizeof(ctx->srcFbAllocInfo));

    return TRUE;
}

static BOOL AllocateCframeOffsetBuffer(ComponentImpl* com, Uint32 srcFbWidth, Uint32 srcFbHeight) {
    YuvFeederContext* ctx = (YuvFeederContext*)com->context;
    EncOpenParam    encOpenParam = ctx->encOpenParam;
    Uint32          i = 0;
    int             lumaTableSize     = WAVE5_ENC_FBC50_LUMA_TABLE_SIZE(srcFbWidth, srcFbHeight);
    int             chromaTableSize;

    if (encOpenParam.cframe50_422 == TRUE)
        chromaTableSize   = WAVE5_ENC_FBC50_422_CHROMA_TABLE_SIZE(srcFbWidth, srcFbHeight);
    else
        chromaTableSize   = WAVE5_ENC_FBC50_CHROMA_TABLE_SIZE(srcFbWidth, srcFbHeight);

    // allocate offset table buffer. the number of offset table buffer should be same as source frame buffer.
    vdi_lock(encOpenParam.coreIdx);
    for(i = 0; i < ctx->fbCount.srcFbNum; i++) {
        ctx->pFbOffsetTblMem[i].size = lumaTableSize + chromaTableSize*2;
        if (vdi_allocate_dma_memory(encOpenParam.coreIdx, &ctx->pFbOffsetTblMem[i]) < 0) {
            vdi_unlock(encOpenParam.coreIdx);
            VLOG(ERR, "fail to allocate fbc offset table buffer\n");
            return FALSE;
        }
        ctx->pFbOffsetTbl[i].bufY  = ctx->pFbOffsetTblMem[i].phys_addr;
        ctx->pFbOffsetTbl[i].bufCb = ctx->pFbOffsetTblMem[i].phys_addr + lumaTableSize;
        ctx->pFbOffsetTbl[i].bufCr = ctx->pFbOffsetTblMem[i].phys_addr + lumaTableSize + chromaTableSize;
        ctx->pFbOffsetTbl[i].updateFbInfo = FALSE;
    }
    vdi_unlock(encOpenParam.coreIdx);

    return TRUE;
}

static BOOL AllocateSourceFrameBuffer(ComponentImpl* com) {
    YuvFeederContext* ctx = (YuvFeederContext*)com->context;
    EncOpenParam    encOpenParam = ctx->encOpenParam;
    Uint32          srcFbWidth   = VPU_ALIGN8(encOpenParam.picWidth);
    Uint32          srcFbHeight  = VPU_ALIGN8(encOpenParam.picHeight);
    SRC_FB_TYPE     srcFbType = SRC_FB_TYPE_YUV;

    if (encOpenParam.cframe50Enable == TRUE)
        srcFbType = SRC_FB_TYPE_CFRAME;

    if (srcFbType == SRC_FB_TYPE_YUV) {
        if (AllocateYuvBuffer(com, srcFbWidth, srcFbHeight) == FALSE) {
            return FALSE;
        }
    }
    else { // srcFbType == SRC_FB_TYPE_CFRAME
        if (AllocateCframeDataBuffer(com, srcFbWidth, srcFbHeight) == FALSE) {
            return FALSE;
        }
        if (AllocateCframeOffsetBuffer(com, srcFbWidth, srcFbHeight) == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL PrepareYuvFeeder(ComponentImpl* com, BOOL* done)
{
    CNMComponentParamRet    ret;
    YuvFeederContext*       ctx = (YuvFeederContext*)com->context;
    EncOpenParam*           encOpenParam = &ctx->encOpenParam;
    BOOL                    success;
    Uint32                  i;
    YuvFeeder               yuvFeeder    = NULL;
    YuvInfo                 yuvFeederInfo;
#if defined(PLATFORM_NON_OS)
	if (ctx->fbAllocated == 1){
		*done = FALSE;
		return TRUE;
	}
	else if (ctx->fbAllocated == 0){
		*done = TRUE;
		return TRUE;
	}
#endif
    *done = FALSE;
    // wait signal GET_PARAM_ENC_FRAME_BUF_NUM
    ret = ComponentGetParameter(com, com->sinkPort.connectedComponent, GET_PARAM_ENC_FRAME_BUF_NUM, &ctx->fbCount);
    if (ComponentParamReturnTest(ret, &success) == FALSE) {
        return success;
    }

    ret = ComponentGetParameter(com, com->sinkPort.connectedComponent, GET_PARAM_ENC_HANDLE, &ctx->handle);
    if (ComponentParamReturnTest(ret, &success) == FALSE) {
        return success;
    }

    if (AllocateReconFrameBuffer(com) == FALSE) {
        VLOG(ERR, "AllocateReconFrameBuffer error\n");
        return FALSE;
    }

    if (AllocateSourceFrameBuffer(com) == FALSE) {
        VLOG(ERR, "AllocateSourceFrameBuffer error\n");
        return FALSE;
    }

    osal_memset(&yuvFeederInfo,   0x00, sizeof(YuvInfo));
    yuvFeederInfo.cframe50Enable = encOpenParam->cframe50Enable;
    yuvFeederInfo.tx16y          = encOpenParam->cframe50Tx16Y;
    yuvFeederInfo.tx16c          = encOpenParam->cframe50Tx16C;
    yuvFeederInfo.lossless       = encOpenParam->cframe50LosslessEnable;
    if (yuvFeederInfo.cframe50Enable)
        ctx->yuvMode = SOURCE_CFBC;

    yuvFeederInfo.cbcrInterleave = encOpenParam->cbcrInterleave;
    yuvFeederInfo.nv21           = encOpenParam->nv21;
    yuvFeederInfo.packedFormat   = encOpenParam->packedFormat;
    yuvFeederInfo.srcFormat      = encOpenParam->srcFormat;
    yuvFeederInfo.srcPlanar      = TRUE;
    yuvFeederInfo.srcStride      = ctx->srcFbStride;
    yuvFeederInfo.srcHeight      = VPU_CEIL(encOpenParam->picHeight, 8);
    yuvFeeder = YuvFeeder_Create(ctx->yuvMode, ctx->inputPath, yuvFeederInfo);
    if (yuvFeeder == NULL) {
        VLOG(ERR, "YuvFeeder_Create Error\n");
        return FALSE;
    }
    ((AbstractYuvFeeder*)yuvFeeder)->impl->handle = ctx->handle;
    ctx->yuvFeeder = yuvFeeder;

    // Fill data into the input queue of the sink port.
    // The empty input queue of the sink port occurs a hangup problem in this example.
    while (Queue_Dequeue(com->sinkPort.inputQ) != NULL);
    for (i = 0; i < com->sinkPort.inputQ->size; i++) {
        PortContainerYuv defaultData;

        osal_memset(&defaultData, 0x00, sizeof(PortContainerYuv));
        defaultData.srcFbIndex   = -1;
        if (i<ctx->fbCount.srcFbNum) {
            defaultData.srcFbIndex   = i;
            if (ctx->subFrameSyncEn == TRUE)
                defaultData.type = SUB_FRAME_SYNC_SRC_ADDR_SEND;
            Queue_Enqueue(com->sinkPort.inputQ, (void*)&defaultData);
            if (ctx->subFrameSyncEn == TRUE) {
                defaultData.type = SUB_FRAME_SYNC_DATA_FEED;
                Queue_Enqueue(com->sinkPort.inputQ, (void*)&defaultData);
            }
        }
    }

    *done = TRUE;

#ifdef __linux__
    pthread_barrier_wait(&ctx->fbAllocated);
#elif _WIN32
	EnterSynchronizationBarrier(&ctx->fbAllocated, 0);
#elif defined(PLATFORM_NON_OS)
	if (ctx->fbAllocated == 2)
		ctx->fbAllocated --;
	if (ctx->fbAllocated != 0){
		*done = FALSE;
		return TRUE;
	}
#endif

    return TRUE;
}

static BOOL ExecuteYuvFeeder(ComponentImpl* com, PortContainer* in, PortContainer* out)
{
    YuvFeederContext*   ctx          = (YuvFeederContext*)com->context;
    EncOpenParam*       encOpenParam = &ctx->encOpenParam;
    int                 ret          = 0;
    PortContainerYuv*   sinkData     = (PortContainerYuv*)out;
    void*               extraFeedingInfo = NULL;

    if (ctx->state == YUV_FEEDER_STATE_WAIT) {
        CNMComponentParamRet ParamRet;
        BOOL                 success, done = FALSE;

        out->reuse = TRUE;
        ParamRet = ComponentGetParameter(com, com->sinkPort.connectedComponent, GET_PARAM_ENC_FRAME_BUF_REGISTERED, &done);
        if (ComponentParamReturnTest(ParamRet, &success) == FALSE) {
            return success;
        }
        if (done == FALSE) return TRUE;
        ctx->state = YUV_FEEDER_STATE_FEEDING;
        out->reuse = FALSE;
    }

    if ( ctx->last ) {
        sinkData->last = TRUE;
        return TRUE;
    }

#if 0
    sinkData->fb              = ctx->pFbSrc[sinkData->srcFbIndex];
#else
    osal_memcpy(&(sinkData->fb), &(ctx->pFbSrc[sinkData->srcFbIndex]), sizeof(FrameBuffer));
#endif

    if (encOpenParam->cframe50Enable) {
        sinkData->fbOffsetTbl = ctx->pFbOffsetTbl[sinkData->srcFbIndex];
        extraFeedingInfo      = &sinkData->fbOffsetTbl;
    }

    if (ctx->subFrameSyncEn) {
        EncInfo*             pEncInfo = VPU_HANDLE_TO_ENCINFO(ctx->handle);
        if (sinkData->type == SUB_FRAME_SYNC_SRC_ADDR_SEND) {
            ctx->feedingNum++;
            if (ctx->feedingNum > ctx->frameOutNum && ctx->frameOutNum != -1) {
                ctx->last = sinkData->last = TRUE;
                return TRUE;
            }

            //write some YUV before VPU_EncStartOneFrame
            pEncInfo->subFrameSyncConfig.subFrameSyncSrcWriteMode = GetRandom(0, encOpenParam->picHeight/64+1) * 64;
            if (pEncInfo->subFrameSyncConfig.subFrameSyncSrcWriteMode > encOpenParam->picHeight)
                pEncInfo->subFrameSyncConfig.subFrameSyncSrcWriteMode = encOpenParam->picHeight;
            YuvFeeder_Feed(ctx->yuvFeeder, encOpenParam->coreIdx, &sinkData->fb, encOpenParam->picWidth, encOpenParam->picHeight, sinkData->srcFbIndex, extraFeedingInfo);
            return 1;
        }
        else { //type:SUB_FRAME_SYNC_DATA_FEED
            //write remain YUV after VPU_EncStartOneFrame
            pEncInfo->subFrameSyncConfig.subFrameSyncSrcWriteMode |= REMAIN_SRC_DATA_WRITE;
            ret = YuvFeeder_Feed(ctx->yuvFeeder, encOpenParam->coreIdx, &sinkData->fb, encOpenParam->picWidth, encOpenParam->picHeight, sinkData->srcFbIndex, extraFeedingInfo);
            if (ret == FALSE) {
                ctx->last = sinkData->last = TRUE;
            }
            return ret;
        }
    }
    else {
        ctx->feedingNum++;
        if (ctx->feedingNum > ctx->frameOutNum && ctx->frameOutNum != -1) {
            ctx->last = sinkData->last = TRUE;
            return TRUE;
        }
        ret = YuvFeeder_Feed(ctx->yuvFeeder, encOpenParam->coreIdx, &sinkData->fb, encOpenParam->picWidth, encOpenParam->picHeight, sinkData->srcFbIndex, extraFeedingInfo);
    }

    if (ret == FALSE) {
        ctx->last = sinkData->last = TRUE;
    }


    return TRUE;
}

static void ReleaseYuvFeeder(ComponentImpl* com)
{
    YuvFeederContext* ctx = (YuvFeederContext*)com->context;
    Uint32          i   = 0;

    vdi_lock(ctx->encOpenParam.coreIdx);
    for (i = 0; i < ctx->fbCount.reconFbNum*2; i++) {
        if (ctx->pFbReconMem[i].size)
            vdi_free_dma_memory(ctx->encOpenParam.coreIdx, &ctx->pFbReconMem[i]);
    }
    for (i = 0; i < ctx->fbCount.srcFbNum; i++) {
        if (ctx->pFbSrcMem[i].size)
            vdi_free_dma_memory(ctx->encOpenParam.coreIdx, &ctx->pFbSrcMem[i]);
        if (ctx->pFbOffsetTblMem[i].size)
            vdi_free_dma_memory(ctx->encOpenParam.coreIdx, &ctx->pFbOffsetTblMem[i]);
    }
    vdi_unlock(ctx->encOpenParam.coreIdx);
}

static BOOL DestroyYuvFeeder(ComponentImpl* com)
{
    YuvFeederContext* ctx = (YuvFeederContext*)com->context;

    if (ctx->yuvFeeder)   YuvFeeder_Destroy(ctx->yuvFeeder);
#ifdef __linux__
    pthread_barrier_destroy(&ctx->fbAllocated);
#elif _WIN32
    DeleteSynchronizationBarrier(&ctx->fbAllocated);
#endif

    osal_free(ctx);

    return TRUE;
}

static Component CreateYuvFeeder(ComponentImpl* com, CNMComponentConfig* componentParam)
{
    YuvFeederContext*   ctx;

    com->context = osal_malloc(sizeof(YuvFeederContext));
    ctx          = (YuvFeederContext*)com->context;

    InitYuvFeederContext(ctx, componentParam);

    if ( ctx->encOpenParam.sourceBufCount > com->numSinkPortQueue )
        com->numSinkPortQueue = ctx->encOpenParam.sourceBufCount;//set requested sourceBufCount

    if ( ctx->subFrameSyncEn == TRUE )
        com->numSinkPortQueue *= 2;
#ifdef __linux__
    pthread_barrier_init(&ctx->fbAllocated, NULL, 2);
#elif _WIN32
    InitializeSynchronizationBarrier(&ctx->fbAllocated, 2, -1);
#elif defined(PLATFORM_NON_OS)
    ctx->fbAllocated = 2;
#endif
    return (Component)com;
}

ComponentImpl yuvFeederComponentImpl = {
    "yuvFeeder",
    NULL,
    {0,},
    {0,},
    sizeof(PortContainerYuv),
    ENC_SRC_BUF_NUM,              /* minimum feeder's numSinkPortQueue(relates to source buffer count)*/
    CreateYuvFeeder,
    GetParameterYuvFeeder,
    SetParameterYuvFeeder,
    PrepareYuvFeeder,
    ExecuteYuvFeeder,
    ReleaseYuvFeeder,
    DestroyYuvFeeder
};

