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
//#include <time.h>
#ifdef __linux__
#include <sys/time.h>
#include <pthread.h>
#elif _WIN32
#include <time.h>
#include <windows.h>
#include <winsock.h>
#endif
#include "cnm_app.h"
#include "component.h"
#include "misc/debug.h"
#include "wave5_regdefine.h"


#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

typedef enum {
    ENC_INT_STATUS_NONE,        // Interrupt not asserted yet
    ENC_INT_STATUS_FULL,        // Need more buffer
    ENC_INT_STATUS_DONE,        // Interrupt asserted
    ENC_INT_STATUS_LOW_LATENCY,
    ENC_INT_STATUS_TIMEOUT,     // Interrupt not asserted during given time.
} ENC_INT_STATUS;

typedef enum {
    ENCODER_STATE_OPEN,
    ENCODER_STATE_INIT_SEQ,
    ENCODER_STATE_REGISTER_FB,
    ENCODER_STATE_ENCODE_HEADER,
    ENCODER_STATE_ENCODING,
} EncoderState;
#define VID_PERFORMANCE_TEST
typedef struct {
    EncHandle                   handle;
    TestEncConfig               testEncConfig;
    EncOpenParam                encOpenParam;
    ParamEncNeedFrameBufferNum  fbCount;
    Uint32                      frameIdx;
    vpu_buffer_t                vbCustomLambda;
    vpu_buffer_t                vbScalingList;
    Uint32                      customLambda[NUM_CUSTOM_LAMBDA];
    UserScalingList             scalingList;
    vpu_buffer_t                vbCustomMap[MAX_REG_FRAME];
    EncoderState                state;
    BOOL                        stateDoing;
    EncInitialInfo              initialInfo;
    Queue*                      encOutQ;
    EncParam                    encParam;
    Int32                       encodedSrcFrmIdxArr[ENC_SRC_BUF_NUM];
    ParamEncBitstreamBuffer     bsBuf;
    BOOL                        fullInterrupt;
    Uint32                      changedCount;
    Uint64                      startTimeout;
    EncSubFrameSyncConfig       subFrameSyncCfg;
    Int32                       encodedSrcFrmIdxArr2[ENC_SRC_BUF_NUM];
    Uint32                      cyclePerTick;
#ifdef __linux__
    pthread_barrier_t           data_sync;
#elif _WIN32
    SYNCHRONIZATION_BARRIER	    data_sync;
#elif  defined(PLATFORM_NON_OS)
    Int64                       data_sync;
    Int64                       data_sync1;
#endif
#ifdef VID_PERFORMANCE_TEST
    int64_t total_time;
    int64_t max_time;
    int64_t min_time;
#endif
} EncoderContext;

static BOOL FindEsBuffer(EncoderContext* ctx, PhysicalAddress addr, vpu_buffer_t* bs)
{
    Uint32 i;

    for (i=0; i<ctx->bsBuf.num; i++) {
        if (addr == ctx->bsBuf.bs[i].phys_addr) {
            *bs = ctx->bsBuf.bs[i];
            return TRUE;
        }
    }

    return FALSE;
}

static void SetEncPicParam(ComponentImpl* com, PortContainerYuv* in, EncParam* encParam)
{
    EncoderContext* ctx           = (EncoderContext*)com->context;
    TestEncConfig   testEncConfig = ctx->testEncConfig;
    Uint32          frameIdx      = ctx->frameIdx;
    Int32           srcFbWidth    = VPU_ALIGN8(ctx->encOpenParam.picWidth);
    Int32           srcFbHeight   = VPU_ALIGN8(ctx->encOpenParam.picHeight);
    Uint32          productId;
    vpu_buffer_t*   buf           = (vpu_buffer_t*)Queue_Peek(ctx->encOutQ);

    productId = VPU_GetProductId(ctx->encOpenParam.coreIdx);

    encParam->picStreamBufferAddr                = buf->phys_addr;
    encParam->picStreamBufferSize                = buf->size;
    encParam->srcIdx                             = in->srcFbIndex;
    encParam->srcEndFlag                         = in->last;
    encParam->sourceFrame                        = &in->fb;
    encParam->sourceFrame->sourceLBurstEn        = 0;
    if (ctx->encOpenParam.cframe50Enable)
        encParam->OffsetTblBuffer                = &in->fbOffsetTbl;
    if (testEncConfig.useAsLongtermPeriod > 0 && testEncConfig.refLongtermPeriod > 0) {
        encParam->useCurSrcAsLongtermPic         = (frameIdx % testEncConfig.useAsLongtermPeriod) == 0 ? 1 : 0;
        encParam->useLongtermRef                 = (frameIdx % testEncConfig.refLongtermPeriod)   == 0 ? 1 : 0;
    }
    encParam->skipPicture                        = 0;
    encParam->forceAllCtuCoefDropEnable	         = 0;

    encParam->forcePicQpEnable		             = 0;
    encParam->forcePicQpI			             = 0;
    encParam->forcePicQpP			             = 0;
    encParam->forcePicQpB			             = 0;
    encParam->forcePicTypeEnable	             = 0;
    encParam->forcePicType			             = 0;

    // FW will encode header data implicitly when changing the header syntaxes
    encParam->codeOption.implicitHeaderEncode    = 1;
    encParam->codeOption.encodeAUD               = testEncConfig.encAUD;
    encParam->codeOption.encodeEOS               = 0;
    encParam->codeOption.encodeEOB               = 0;

    // set custom map param
    if (productId != PRODUCT_ID_521)
        encParam->customMapOpt.roiAvgQp		     = testEncConfig.roi_avg_qp;
    encParam->customMapOpt.customLambdaMapEnable = testEncConfig.lambda_map_enable;
    encParam->customMapOpt.customModeMapEnable	 = testEncConfig.mode_map_flag & 0x1;
    encParam->customMapOpt.customCoefDropEnable  = (testEncConfig.mode_map_flag & 0x2) >> 1;
    encParam->customMapOpt.customRoiMapEnable    = testEncConfig.roi_enable;

    if (in->prevMapReuse == FALSE) {
        // packaging roi/lambda/mode data to custom map buffer.
        if (encParam->customMapOpt.customRoiMapEnable  || encParam->customMapOpt.customLambdaMapEnable ||
            encParam->customMapOpt.customModeMapEnable || encParam->customMapOpt.customCoefDropEnable) {
                SetMapData(testEncConfig.coreIdx, testEncConfig, ctx->encOpenParam, encParam, srcFbWidth, srcFbHeight, ctx->vbCustomMap[encParam->srcIdx].phys_addr);
        }

        // host should set proper value.
        // set weighted prediction param.
        if ((testEncConfig.wp_param_flag & 0x1)) {
            char lineStr[256] = {0,};
            Uint32 meanY, meanCb, meanCr, sigmaY, sigmaCb, sigmaCr;
            Uint32 maxMean  = (ctx->encOpenParam.bitstreamFormat == STD_AVC) ? 0xffff : ((1 << ctx->encOpenParam.EncStdParam.waveParam.internalBitDepth) - 1);
            Uint32 maxSigma = (ctx->encOpenParam.bitstreamFormat == STD_AVC) ? 0xffff : ((1 << (ctx->encOpenParam.EncStdParam.waveParam.internalBitDepth + 6)) - 1);
            osal_fgets(lineStr, 256, testEncConfig.wp_param_file);
            osal_sscanf(lineStr, "%d %d %d %d %d %d\n", &meanY, &meanCb, &meanCr, &sigmaY, &sigmaCb, &sigmaCr);

            meanY	= max(min(maxMean, meanY), 0);
            meanCb	= max(min(maxMean, meanCb), 0);
            meanCr	= max(min(maxMean, meanCr), 0);
            sigmaY	= max(min(maxSigma, sigmaY), 0);
            sigmaCb = max(min(maxSigma, sigmaCb), 0);
            sigmaCr = max(min(maxSigma, sigmaCr), 0);

            // set weighted prediction param.
            encParam->wpPixSigmaY	= sigmaY;
            encParam->wpPixSigmaCb	= sigmaCb;
            encParam->wpPixSigmaCr	= sigmaCr;
            encParam->wpPixMeanY    = meanY;
            encParam->wpPixMeanCb	= meanCb;
            encParam->wpPixMeanCr	= meanCr;
        }
    }
}

static BOOL RegisterFrameBuffers(ComponentImpl* com)
{
    EncoderContext*         ctx= (EncoderContext*)com->context;
    FrameBuffer*            pReconFb      = NULL;
    FrameBuffer*            pSrcFb        = NULL;
    FrameBufferAllocInfo    srcFbAllocInfo;
    Uint32                  reconFbStride = 0;
    Uint32                  reconFbHeight = 0;
    ParamEncFrameBuffer     paramFb;
    RetCode                 result;
    CNMComponentParamRet    ret;
    BOOL                    success;
    Uint32                  idx;
#if defined(PLATFORM_NON_OS)
    if (ctx->data_sync1 == 1){
	    ctx->stateDoing = TRUE;
	    return TRUE;
	}
    if (ctx->data_sync1 == 0){
	    ctx->stateDoing = FALSE;
		return TRUE;
	}
#endif
    ctx->stateDoing = TRUE;
    ret = ComponentGetParameter(com, com->srcPort.connectedComponent, GET_PARAM_YUVFEEDER_FRAME_BUF, (void*)&paramFb);
    if (ComponentParamReturnTest(ret, &success) == FALSE) return success;

    pReconFb      = paramFb.reconFb;
    reconFbStride = paramFb.reconFbStride;
    reconFbHeight = paramFb.reconFbHeight;
    result = VPU_EncRegisterFrameBuffer(ctx->handle, pReconFb, ctx->fbCount.reconFbNum, reconFbStride, reconFbHeight, COMPRESSED_FRAME_MAP);
    if (result != RETCODE_SUCCESS) {
        VLOG(ERR, "%s:%d Failed to VPU_EncRegisterFrameBuffer(%d)\n", __FUNCTION__, __LINE__, result);
        return FALSE;
    }
    ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_REGISTER_FB, NULL);

    pSrcFb         = paramFb.srcFb;
    //srcFbAllocInfo = paramFb.srcFbAllocInfo;
    osal_memcpy(&srcFbAllocInfo, &paramFb.srcFbAllocInfo, sizeof(srcFbAllocInfo));
    if (VPU_EncAllocateFrameBuffer(ctx->handle, srcFbAllocInfo, pSrcFb) != RETCODE_SUCCESS) {
        VLOG(ERR, "VPU_EncAllocateFrameBuffer fail to allocate source frame buffer\n");
        return FALSE;
    }

    if (ctx->testEncConfig.roi_enable || ctx->testEncConfig.lambda_map_enable || ctx->testEncConfig.mode_map_flag) {
        vdi_lock(ctx->testEncConfig.coreIdx);
        for (idx = 0; idx < ctx->fbCount.srcFbNum; idx++) {
            ctx->vbCustomMap[idx].size = (ctx->encOpenParam.bitstreamFormat == STD_AVC) ? MAX_MB_NUM : MAX_CTU_NUM * 8;
            if (vdi_allocate_dma_memory(ctx->testEncConfig.coreIdx, &ctx->vbCustomMap[idx]) < 0) {
                vdi_unlock(ctx->testEncConfig.coreIdx);
                VLOG(ERR, "fail to allocate ROI buffer\n");
                return FALSE;
            }
        }
        vdi_unlock(ctx->testEncConfig.coreIdx);
    }

    ctx->stateDoing = FALSE;


#ifdef __linux__
        pthread_barrier_wait(&ctx->data_sync);
#elif _WIN32
        EnterSynchronizationBarrier(&ctx->data_sync, 0);
#elif defined(PLATFORM_NON_OS)
        if (ctx->data_sync1 == 2)
	        ctx->data_sync1--;
        if (ctx->data_sync1 != 0){
			ctx->stateDoing = TRUE;
	        return TRUE;
		}
#endif

    return TRUE;
}

static CNMComponentParamRet GetParameterEncoder(ComponentImpl* from, ComponentImpl* com, GetParameterCMD commandType, void* data)
{
    EncoderContext*             ctx = (EncoderContext*)com->context;
    BOOL                        result  = TRUE;
    ParamEncNeedFrameBufferNum* fbCount;
    PortContainerYuv*           container;

    switch(commandType) {
    case GET_PARAM_COM_IS_CONTAINER_CONUSUMED:
        container = (PortContainerYuv*)data;
        if ( ctx->subFrameSyncCfg.subFrameSyncOn == TRUE) {
            if (ctx->encodedSrcFrmIdxArr[container->srcFbIndex]) {
                ctx->encodedSrcFrmIdxArr[container->srcFbIndex] = 0;
                container->consumed = TRUE;
            }
            else if (ctx->encodedSrcFrmIdxArr2[container->srcFbIndex]) {
                ctx->encodedSrcFrmIdxArr2[container->srcFbIndex] = 0;
                container->consumed = TRUE;
            }
        }
        else
        {
            if (ctx->encodedSrcFrmIdxArr[container->srcFbIndex]) {
                ctx->encodedSrcFrmIdxArr[container->srcFbIndex] = 0;
                container->consumed = TRUE;
            }
        }
        break;
    case GET_PARAM_ENC_HANDLE:
        if (ctx->handle == NULL) return CNM_COMPONENT_PARAM_NOT_READY;
        *(EncHandle*)data = ctx->handle;
        break;
    case GET_PARAM_ENC_FRAME_BUF_NUM:
#ifdef __linux__
        pthread_barrier_wait(&ctx->data_sync); // sync point 1 at setting seq
#elif _WIN32
        EnterSynchronizationBarrier(&ctx->data_sync, 0);
#elif defined(PLATFORM_NON_OS)
        if (ctx->data_sync == 1)
	        ctx->data_sync--;
        if (ctx->data_sync == 2){
	        return CNM_COMPONENT_PARAM_NOT_READY;
		}
#endif
        fbCount = (ParamEncNeedFrameBufferNum*)data;
        fbCount->reconFbNum = ctx->fbCount.reconFbNum;
        fbCount->srcFbNum   = ctx->fbCount.srcFbNum;
        break;
    case GET_PARAM_ENC_FRAME_BUF_REGISTERED:
#ifdef __linux__
        pthread_barrier_wait(&ctx->data_sync); // sync point 2 at registering fb
#elif _WIN32
        EnterSynchronizationBarrier(&ctx->data_sync, 0);
#elif defined(PLATFORM_NON_OS)
        if (ctx->data_sync1 == 1)
	        ctx->data_sync1--;
        if (ctx->data_sync1 == 2)
	        return CNM_COMPONENT_PARAM_NOT_READY;
#endif
        *(BOOL*)data = TRUE;
        break;
    default:
        result = FALSE;
        break;
    }

    return (result == TRUE) ? CNM_COMPONENT_PARAM_SUCCESS : CNM_COMPONENT_PARAM_FAILURE;
}

static CNMComponentParamRet SetParameterEncoder(ComponentImpl* from, ComponentImpl* com, SetParameterCMD commandType, void* data)
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

static ENC_INT_STATUS HandlingInterruptFlag(ComponentImpl* com)
{
    EncoderContext*   ctx               = (EncoderContext*)com->context;
    EncHandle       handle                = ctx->handle;
    Int32           interruptFlag         = 0;
    Uint32          interruptWaitTime     = VPU_WAIT_TIME_OUT_CQ;
    Uint32          interruptTimeout      = VPU_ENC_TIMEOUT;
    ENC_INT_STATUS  status                = ENC_INT_STATUS_NONE;

    if (ctx->startTimeout == 0ULL) {
        ctx->startTimeout = osal_gettime();
    }
    do {
        interruptFlag = VPU_WaitInterruptEx(handle, interruptWaitTime);
        if (interruptFlag == -1) {
            Uint64   currentTimeout = osal_gettime();

            if ((currentTimeout - ctx->startTimeout) > interruptTimeout) {
                VLOG(ERR, "<%s:%d> startTimeout(%lld) currentTime(%lld) diff(%d)\n",
                    __FUNCTION__, __LINE__, ctx->startTimeout, currentTimeout, (Uint32)(currentTimeout - ctx->startTimeout));
                CNMErrorSet(CNM_ERROR_HANGUP);
                status = ENC_INT_STATUS_TIMEOUT;
                break;
            }
            interruptFlag = 0;
        }

        if (interruptFlag < 0) {
            VLOG(ERR, "<%s:%d> interruptFlag is negative value! %08x\n", __FUNCTION__, __LINE__, interruptFlag);
        }

        if (interruptFlag > 0) {
            VPU_ClearInterruptEx(handle, interruptFlag);
            ctx->startTimeout = 0ULL;
        }

        if (interruptFlag & (1<<INT_WAVE5_ENC_SET_PARAM)) {
            status = ENC_INT_STATUS_DONE;
            break;
        }

        if (interruptFlag & (1<<INT_WAVE5_ENC_PIC)) {
            status = ENC_INT_STATUS_DONE;
            break;
        }

        if (interruptFlag & (1<<INT_WAVE5_BSBUF_FULL)) {
            status = ENC_INT_STATUS_FULL;
            break;
        }

        if (interruptFlag & (1<<INT_WAVE5_ENC_LOW_LATENCY)) {
            status = ENC_INT_STATUS_LOW_LATENCY;
        }
    } while (FALSE);

    return status;
}

static BOOL SetSequenceInfo(ComponentImpl* com)
{
    EncoderContext* ctx = (EncoderContext*)com->context;
    EncHandle       handle  = ctx->handle;
    RetCode         ret     = RETCODE_SUCCESS;
    ENC_INT_STATUS  status;
    EncInitialInfo* initialInfo = &ctx->initialInfo;
    CNMComListenerEncCompleteSeq lsnpCompleteSeq   = {0};

#if defined(PLATFORM_NON_OS)
    if (ctx->data_sync == 1){
	    ctx->stateDoing = TRUE;
		return TRUE;
	}
    if (ctx->data_sync == 0){
	    ctx->stateDoing = FALSE;
		return TRUE;
	}
#endif
    if (ctx->stateDoing == FALSE) {
        do {
            ret = VPU_EncIssueSeqInit(handle);
        } while (ret == RETCODE_QUEUEING_FAILURE && com->terminate == FALSE);

        if (ret != RETCODE_SUCCESS) {
            VLOG(ERR, "%s:%d Failed to VPU_EncIssueSeqInit() ret(%d)\n", __FUNCTION__, __LINE__, ret);
            return FALSE;
        }
        ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_ISSUE_SEQ, NULL);
    }
    ctx->stateDoing = TRUE;

    while (com->terminate == FALSE) {
        if ((status=HandlingInterruptFlag(com)) == ENC_INT_STATUS_DONE) {
            break;
        }
        else if (status == ENC_INT_STATUS_NONE) {
            return TRUE;
        }
        else if (status == ENC_INT_STATUS_TIMEOUT) {
            VLOG(INFO, "%s:%d INSTANCE #%d INTERRUPT TIMEOUT\n", __FUNCTION__, __LINE__, handle->instIndex);
            HandleEncoderError(ctx->handle, ctx->frameIdx, NULL);
            return FALSE;
        }
        else {
            VLOG(INFO, "%s:%d Unknown interrupt status: %d\n", __FUNCTION__, __LINE__, status);
            return FALSE;
        }
    }

    if ((ret=VPU_EncCompleteSeqInit(handle, initialInfo)) != RETCODE_SUCCESS) {
        VLOG(ERR, "%s:%d FAILED TO ENC_PIC_HDR: ret(%d), SEQERR(%08x)\n",
            __FUNCTION__, __LINE__, ret, initialInfo->seqInitErrReason);
        return FALSE;
    }

    lsnpCompleteSeq.handle = handle;
    ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_COMPLETE_SEQ, (void*)&lsnpCompleteSeq);

    ctx->fbCount.reconFbNum = initialInfo->minFrameBufferCount;
    ctx->fbCount.srcFbNum   = initialInfo->minSrcFrameCount + COMMAND_QUEUE_DEPTH + EXTRA_SRC_BUFFER_NUM;

    if ( ctx->subFrameSyncCfg.subFrameSyncOn == TRUE) {
        if ( ctx->fbCount.srcFbNum > 5 ) {
            ctx->fbCount.srcFbNum = 5;
            VLOG(TRACE, "Set src frame buffer number to 5 if subFrameSync is enabled(constraint)\n");
        }
    }

    if ( ctx->encOpenParam.sourceBufCount > ctx->fbCount.srcFbNum)
        ctx->fbCount.srcFbNum = ctx->encOpenParam.sourceBufCount;

    VLOG(INFO, "[ENCODER] Required  reconFbCount=%d, srcFbCount=%d, outNum=%d, %dx%d\n",
        ctx->fbCount.reconFbNum, ctx->fbCount.srcFbNum, ctx->testEncConfig.outNum, ctx->encOpenParam.picWidth, ctx->encOpenParam.picHeight);
    ctx->stateDoing = FALSE;

#ifdef __linux__
        pthread_barrier_wait(&ctx->data_sync); // sync point 1 at setting seq
#elif _WIN32
        EnterSynchronizationBarrier(&ctx->data_sync, 0);
#elif defined(PLATFORM_NON_OS)
        if (ctx->data_sync == 2)
	        ctx->data_sync--;
        if (ctx->data_sync != 0){
			ctx->stateDoing = TRUE;
	        return TRUE;
		}
#endif
    return TRUE;
}

static BOOL EncodeHeader(ComponentImpl* com)
{
    EncoderContext*         ctx = (EncoderContext*)com->context;
    EncHandle               handle  = ctx->handle;
    RetCode                 ret     = RETCODE_SUCCESS;
    EncHeaderParam          encHeaderParam;
    vpu_buffer_t*           buf;

    ctx->stateDoing = TRUE;
    buf = Queue_Dequeue(ctx->encOutQ);
    osal_memset(&encHeaderParam, 0x00, sizeof(EncHeaderParam));

    encHeaderParam.buf  = buf->phys_addr;
    encHeaderParam.size = buf->size;

    if (ctx->encOpenParam.bitstreamFormat == STD_HEVC) {
        encHeaderParam.headerType = CODEOPT_ENC_VPS | CODEOPT_ENC_SPS | CODEOPT_ENC_PPS;
    }
    else {
        // H.264
        encHeaderParam.headerType = CODEOPT_ENC_SPS | CODEOPT_ENC_PPS;
    }

    while(1) {
        ret = VPU_EncGiveCommand(handle, ENC_PUT_VIDEO_HEADER, &encHeaderParam);
        if ( ret != RETCODE_QUEUEING_FAILURE )
            break;
#if defined(HAPS_SIM) || defined(CNM_SIM_DPI_INTERFACE)
        osal_msleep(1);
#endif
    }

    DisplayEncodedInformation(ctx->handle, ctx->encOpenParam.bitstreamFormat, 0, NULL, 0, 0, ctx->testEncConfig.performance);

    ctx->stateDoing = FALSE;

    return TRUE;
}

static BOOL Encode(ComponentImpl* com, PortContainerYuv* in, PortContainerES* out)
{
    EncoderContext*         ctx             = (EncoderContext*)com->context;
    BOOL                    doEncode        = FALSE;
    BOOL                    doChangeParam   = FALSE;
    EncParam*               encParam        = &ctx->encParam;
    EncOutputInfo           encOutputInfo;
    ENC_INT_STATUS          intStatus;
    RetCode                 result;
    CNMComListenerHandlingInt lsnpHandlingInt   = {0};
    CNMComListenerEncDone   lsnpPicDone     = {0};
    ENC_QUERY_WRPTR_SEL     encWrPtrSel     = GET_ENC_PIC_DONE_WRPTR;

    ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_READY_ONE_FRAME, NULL);

    ctx->stateDoing = TRUE;
    if (in && in->type == SUB_FRAME_SYNC_DATA_FEED) {
        in->reuse = FALSE;
        return TRUE;
    }
    if (out) {
        if (out->buf.phys_addr != 0) {
            Queue_Enqueue(ctx->encOutQ, (void*)&out->buf);
            out->buf.phys_addr = 0;
            out->buf.size      = 0;
        }
        out->reuse = TRUE;  // For reusing it when no output
        if (encParam->srcEndFlag == TRUE) {
            doEncode = (BOOL)(Queue_Get_Cnt(ctx->encOutQ) > 0);
        }
    }
    if (in) {
        if (Queue_Get_Cnt(ctx->encOutQ) > 0) {
            SetEncPicParam(com, in, encParam);
            doEncode = TRUE;
            in->prevMapReuse = TRUE;
        }
        else {
            in->reuse = TRUE;
        }
    }


    if ((ctx->testEncConfig.numChangeParam > ctx->changedCount) &&
        (ctx->testEncConfig.changeParam[ctx->changedCount].setParaChgFrmNum == ctx->frameIdx)) {
        doChangeParam = TRUE;
    }

    if (doChangeParam == TRUE) {
        result = SetChangeParam(ctx->handle, ctx->testEncConfig, ctx->encOpenParam, ctx->changedCount);
        if (result == RETCODE_SUCCESS) {
            VLOG(TRACE, "ENC_SET_PARA_CHANGE queue success\n");
            ctx->changedCount++;
        }
        else if (result == RETCODE_QUEUEING_FAILURE) { // Just retry
            VLOG(INFO, "ENC_SET_PARA_CHANGE Queue Full\n");
            if (in) in->reuse = TRUE;
            doEncode  = FALSE;
        }
        else { // Error
            VLOG(ERR, "VPU_EncGiveCommand[ENC_SET_PARA_CHANGE] failed Error code is 0x%x \n", result);
            return FALSE;
        }
    }
#ifdef VID_PERFORMANCE_TEST
        int64_t start_pts, end_pts;
#ifdef __linux__
        struct timeval tv;
        if(gettimeofday(&tv, NULL) == 0) {
            start_pts = (int64_t)tv.tv_sec*1000000 + tv.tv_usec;
        }
        else {
            start_pts = 0;
        }
#elif _WIN32
        SYSTEMTIME wtm;
        GetLocalTime(&wtm);
        start_pts = ((int64_t)wtm.wSecond *1000000) + (wtm.wMilliseconds * 1000);
#else
        start_pts = osal_gettime();
#endif
#endif
    if (doEncode == TRUE) {
        CNMComListenerEncStartOneFrame lsn;
        result = VPU_EncStartOneFrame(ctx->handle, encParam);
        if (result == RETCODE_SUCCESS) {
            if (in)  in->prevMapReuse = FALSE;
            Queue_Dequeue(ctx->encOutQ);
            ctx->frameIdx++;
        }
        else if (result == RETCODE_QUEUEING_FAILURE) { // Just retry
            QueueStatusInfo qStatus;
            if (in)  in->reuse  = TRUE;
            if (out) out->reuse = TRUE;
            // Just retry
            VPU_EncGiveCommand(ctx->handle, ENC_GET_QUEUE_STATUS, (void*)&qStatus);
            if (qStatus.instanceQueueCount == 0) {
                return TRUE;
            }
        }
        else { // Error
            VLOG(ERR, "VPU_EncStartOneFrame failed Error code is 0x%x \n", result);
            CNMErrorSet(CNM_ERROR_HANGUP);
            HandleEncoderError(ctx->handle, ctx->frameIdx, NULL);
            return FALSE;
        }
        lsn.handle = ctx->handle;
        lsn.result = result;
        ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_START_ONE_FRAME, (void*)&lsn);
    }

    if ((intStatus=HandlingInterruptFlag(com)) == ENC_INT_STATUS_TIMEOUT) {
        HandleEncoderError(ctx->handle, ctx->frameIdx, NULL);
        VPU_SWReset(ctx->testEncConfig.coreIdx, SW_RESET_SAFETY, ctx->handle);
        return FALSE;
    }
    else if (intStatus == ENC_INT_STATUS_FULL || intStatus == ENC_INT_STATUS_LOW_LATENCY) {
        CNMComListenerEncFull   lsnpFull;
        PhysicalAddress         paRdPtr;
        PhysicalAddress         paWrPtr;
        int                     size;

        encWrPtrSel = (intStatus==ENC_INT_STATUS_FULL) ? GET_ENC_BSBUF_FULL_WRPTR : GET_ENC_LOW_LATENCY_WRPTR;
        VPU_EncGiveCommand(ctx->handle, ENC_WRPTR_SEL, &encWrPtrSel);
        VPU_EncGetBitstreamBuffer(ctx->handle, &paRdPtr, &paWrPtr, &size);
        VLOG(TRACE, "<%s:%d> INT_BSBUF_FULL %p, %p\n", __FUNCTION__, __LINE__, paRdPtr, paWrPtr);

        lsnpFull.handle = ctx->handle;
        lsnpFull.rdPtr  = paRdPtr;
        lsnpFull.wrPtr  = paWrPtr;
        lsnpFull.size   = size;
        ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_FULL_INTERRUPT, (void*)&lsnpFull);

        if ( out ) {
            if (FindEsBuffer(ctx, paRdPtr, &out->buf) == FALSE) {
                VLOG(ERR, "%s:%d Failed to find buffer(%p)\n", __FUNCTION__, __LINE__, paRdPtr);
                return FALSE;
            }
            out->size  = size;
            out->reuse = FALSE;
            out->streamBufFull = TRUE;
        }
        ctx->fullInterrupt = TRUE;
        return TRUE;
    }
    else if (intStatus == ENC_INT_STATUS_NONE) {
        if (out) {
            out->size  = 0;
            out->reuse = FALSE;
        }
        return TRUE; /* Try again */
    }
    ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_HANDLING_INT, (void*)&lsnpHandlingInt);

    VPU_EncGiveCommand(ctx->handle, ENC_WRPTR_SEL, &encWrPtrSel);
    osal_memset(&encOutputInfo, 0x00, sizeof(EncOutputInfo));
    encOutputInfo.result = VPU_EncGetOutputInfo(ctx->handle, &encOutputInfo);
    if (encOutputInfo.result == RETCODE_REPORT_NOT_READY) {
        return TRUE; /* Not encoded yet */
    }
    else if (encOutputInfo.result == RETCODE_VLC_BUF_FULL) {
        VLOG(ERR, "VLC BUFFER FULL!!! ALLOCATE MORE TASK BUFFER(%d)!!!\n", ONE_TASKBUF_SIZE_FOR_CQ);
    }
    else if (encOutputInfo.result != RETCODE_SUCCESS) {
        /* ERROR */
        VLOG(ERR, "Failed to encode error = %d\n", encOutputInfo.result);
        HandleEncoderError(ctx->handle, encOutputInfo.encPicCnt, &encOutputInfo);
        VPU_SWReset(ctx->testEncConfig.coreIdx, SW_RESET_SAFETY, ctx->handle);
        return FALSE;
    }
    else {
        ;/* SUCCESS */
    }
#ifdef VID_PERFORMANCE_TEST
    if(ctx->frameIdx > 0/* && (ctx->frameIdx % 100 == 0)*/)
    {
#ifdef __linux__
        if(gettimeofday(&tv, NULL) == 0) {
            end_pts = (int64_t)tv.tv_sec*1000000 + tv.tv_usec;
        }
        else {
            end_pts = 0;
        }
#elif _WIN32
        SYSTEMTIME wtm;
        GetLocalTime(&wtm);
        end_pts = 0;
        end_pts = ((int64_t)wtm.wSecond *1000000) + (wtm.wMilliseconds * 1000);
#else
        end_pts = osal_gettime();
#endif


        if(ctx->max_time < end_pts - start_pts)
            ctx->max_time = end_pts - start_pts;
        if(ctx->min_time > end_pts - start_pts)
            ctx->min_time = end_pts - start_pts;
        ctx->total_time += end_pts - start_pts;

        VLOG(INFO, "max_time : %ld us, min_time : %ld us, ave_time : %ld us, curr : %ld us.\n", \
            ctx->max_time, ctx->min_time, ctx->total_time/ctx->frameIdx, end_pts - start_pts);
    }

#endif
    if (encOutputInfo.reconFrameIndex == RECON_IDX_FLAG_CHANGE_PARAM) {
        VLOG(TRACE, "CHANGE PARAMETER!\n");
        return TRUE; /* Try again */
    }
    else {
        DisplayEncodedInformation(ctx->handle, ctx->encOpenParam.bitstreamFormat, ctx->frameIdx, &encOutputInfo, encParam->srcEndFlag, encParam->srcIdx, ctx->testEncConfig.performance);
    }
    lsnpPicDone.handle = ctx->handle;
    lsnpPicDone.output = &encOutputInfo;
    lsnpPicDone.fullInterrupted = ctx->fullInterrupt;
    ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_GET_OUTPUT_INFO, (void*)&lsnpPicDone);
    if ( encOutputInfo.result != RETCODE_SUCCESS )
        return FALSE;

    if ( encOutputInfo.encSrcIdx >= 0) {
        ctx->encodedSrcFrmIdxArr[encOutputInfo.encSrcIdx] = 1;
        if ( ctx->subFrameSyncCfg.subFrameSyncOn)
            ctx->encodedSrcFrmIdxArr2[encOutputInfo.encSrcIdx] = 1;
    }

    ctx->fullInterrupt      = FALSE;

    if ( out ) {
        if (FindEsBuffer(ctx, encOutputInfo.bitstreamBuffer, &out->buf) == FALSE) {
            VLOG(ERR, "%s:%d Failed to find buffer(%p)\n", __FUNCTION__, __LINE__, encOutputInfo.bitstreamBuffer);
            return FALSE;
        }
        out->size  = encOutputInfo.bitstreamSize;
        out->reuse = (BOOL)(out->size == 0);
    }

    // Finished encoding a frame
    if (encOutputInfo.reconFrameIndex == RECON_IDX_FLAG_ENC_END) {
        if (ctx->testEncConfig.outNum != encOutputInfo.encPicCnt && ctx->testEncConfig.outNum != -1) {
            VLOG(ERR, "outnum(%d) != encoded cnt(%d)\n", ctx->testEncConfig.outNum, encOutputInfo.encPicCnt);
            return FALSE;
        }
        if(out) out->last  = TRUE;  // Send finish signal
        if(out) out->reuse = FALSE;
        ctx->stateDoing    = FALSE;
        com->terminate     = TRUE;
    }

    return TRUE;
}

static BOOL AllocateCustomBuffer(ComponentImpl* com)
{
    EncoderContext* ctx       = (EncoderContext*)com->context;
    TestEncConfig testEncConfig = ctx->testEncConfig;

    /* Allocate Buffer and Set Data */
    if (ctx->encOpenParam.EncStdParam.waveParam.scalingListEnable) {
        ctx->vbScalingList.size = 0x1000;
        vdi_lock(testEncConfig.coreIdx);
        if (vdi_allocate_dma_memory(testEncConfig.coreIdx, &ctx->vbScalingList) < 0) {
            vdi_unlock(testEncConfig.coreIdx);
            VLOG(ERR, "fail to allocate scaling list buffer\n");
            return FALSE;
        }
        vdi_unlock(testEncConfig.coreIdx);
        ctx->encOpenParam.EncStdParam.waveParam.userScalingListAddr = ctx->vbScalingList.phys_addr;

        parse_user_scaling_list(&ctx->scalingList, testEncConfig.scaling_list_file, testEncConfig.stdMode);
        vdi_write_memory(testEncConfig.coreIdx, ctx->vbScalingList.phys_addr, (unsigned char*)&ctx->scalingList, ctx->vbScalingList.size, VDI_LITTLE_ENDIAN);
    }

    if (ctx->encOpenParam.EncStdParam.waveParam.customLambdaEnable) {
        ctx->vbCustomLambda.size = 0x200;
        vdi_lock(testEncConfig.coreIdx);
        if (vdi_allocate_dma_memory(testEncConfig.coreIdx, &ctx->vbCustomLambda) < 0) {
            vdi_unlock(testEncConfig.coreIdx);
            VLOG(ERR, "fail to allocate Lambda map buffer\n");
            return FALSE;
        }
        vdi_unlock(testEncConfig.coreIdx);
        ctx->encOpenParam.EncStdParam.waveParam.customLambdaAddr = ctx->vbCustomLambda.phys_addr;

        parse_custom_lambda(ctx->customLambda, testEncConfig.custom_lambda_file);
        vdi_write_memory(testEncConfig.coreIdx, ctx->vbCustomLambda.phys_addr, (unsigned char*)&ctx->customLambda[0], ctx->vbCustomLambda.size, VDI_LITTLE_ENDIAN);
    }

    return TRUE;
}

static BOOL OpenEncoder(ComponentImpl* com)
{
    EncoderContext*         ctx = (EncoderContext*)com->context;
    SecAxiUse               secAxiUse;
    MirrorDirection         mirrorDirection;
    RetCode                 result;
    CNMComListenerEncOpen   lspn    = {0};

    ctx->stateDoing = TRUE;

    ctx->encOpenParam.bitstreamBuffer     = ctx->bsBuf.bs[0].phys_addr;
    ctx->encOpenParam.bitstreamBufferSize = ctx->bsBuf.bs[0].size;

    if (AllocateCustomBuffer(com) == FALSE) return FALSE;

    if ((result = VPU_EncOpen(&ctx->handle, &ctx->encOpenParam)) != RETCODE_SUCCESS) {
        VLOG(ERR, "VPU_EncOpen failed Error code is 0x%x \n", result);
        if ( result == RETCODE_VPU_RESPONSE_TIMEOUT ) {
            CNMErrorSet(CNM_ERROR_HANGUP);
        }
        CNMAppStop();
        return FALSE;
    }
    //VPU_EncGiveCommand(ctx->handle, ENABLE_LOGGING, 0);
    lspn.handle = ctx->handle;
    ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_OPEN, (void*)&lspn);

    if (ctx->testEncConfig.rotAngle != 0 || ctx->testEncConfig.mirDir != 0) {
        VPU_EncGiveCommand(ctx->handle, ENABLE_ROTATION, 0);
        VPU_EncGiveCommand(ctx->handle, ENABLE_MIRRORING, 0);
        VPU_EncGiveCommand(ctx->handle, SET_ROTATION_ANGLE, &ctx->testEncConfig.rotAngle);
        mirrorDirection = (MirrorDirection)ctx->testEncConfig.mirDir;
        VPU_EncGiveCommand(ctx->handle, SET_MIRROR_DIRECTION, &mirrorDirection);
    }

    osal_memset(&secAxiUse,   0x00, sizeof(SecAxiUse));
    secAxiUse.u.wave.useEncRdoEnable = (ctx->testEncConfig.secondaryAXI & 0x1) ? TRUE : FALSE;  //USE_RDO_INTERNAL_BUF
    secAxiUse.u.wave.useEncLfEnable  = (ctx->testEncConfig.secondaryAXI & 0x2) ? TRUE : FALSE;  //USE_LF_INTERNAL_BUF
    VPU_EncGiveCommand(ctx->handle, SET_SEC_AXI, &secAxiUse);
    VPU_EncGiveCommand(ctx->handle, SET_CYCLE_PER_TICK,   (void*)&ctx->cyclePerTick);
    ctx->subFrameSyncCfg.subFrameSyncOn = ctx->testEncConfig.subFrameSyncEn;
    ctx->stateDoing = FALSE;

    return TRUE;
}

static BOOL ExecuteEncoder(ComponentImpl* com, PortContainer* in, PortContainer* out)
{
    EncoderContext* ctx             = (EncoderContext*)com->context;
    BOOL            ret;

    if (ctx->state != ENCODER_STATE_ENCODING) {
        if (in)  in->reuse  = TRUE;
        if (out) out->reuse = TRUE;
    }

    switch (ctx->state) {
    case ENCODER_STATE_OPEN:
        ret = OpenEncoder(com);
        if (ctx->stateDoing == FALSE) ctx->state = ENCODER_STATE_INIT_SEQ;
        break;
    case ENCODER_STATE_INIT_SEQ:
        ret = SetSequenceInfo(com);
        if (ctx->stateDoing == FALSE) ctx->state = ENCODER_STATE_REGISTER_FB;
        break;
    case ENCODER_STATE_REGISTER_FB:
        ret = RegisterFrameBuffers(com);
        if (ctx->stateDoing == FALSE) ctx->state = ENCODER_STATE_ENCODE_HEADER;
        break;
    case ENCODER_STATE_ENCODE_HEADER:
        ret = EncodeHeader(com);
        if (ctx->stateDoing == FALSE) ctx->state = ENCODER_STATE_ENCODING;
        break;
    case ENCODER_STATE_ENCODING:
        ret = Encode(com, (PortContainerYuv*)in, (PortContainerES*)out);
        break;
    default:
        ret = FALSE;
        break;
    }

    if (ret == FALSE || com->terminate == TRUE) {
        ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_ENCODED_ALL, (void*)ctx->handle);
        if (out != NULL) {
            out->reuse = FALSE;
            out->last  = TRUE;
        }
    }
    return ret;
}

static BOOL PrepareEncoder(ComponentImpl* com, BOOL* done)
{
    EncoderContext*      ctx       = (EncoderContext*)com->context;
    TestEncConfig*       testEncConfig = &ctx->testEncConfig;
    CNMComponentParamRet ret;
    BOOL                 success;
    Uint32               i;

    *done = FALSE;

    ret = ComponentGetParameter(com, com->sinkPort.connectedComponent, GET_PARAM_READER_BITSTREAM_BUF, &ctx->bsBuf);
    if (ComponentParamReturnTest(ret, &success) == FALSE) return success;

    ctx->encOutQ = Queue_Create(ctx->bsBuf.num, sizeof(vpu_buffer_t));
    for (i=0; i<ctx->bsBuf.num; i++) {
        Queue_Enqueue(ctx->encOutQ, (void*)&ctx->bsBuf.bs[i]);
    }

    /* Open Data File*/
    if (ctx->encOpenParam.EncStdParam.waveParam.scalingListEnable) {
        if (testEncConfig->scaling_list_fileName) {
            ChangePathStyle(testEncConfig->scaling_list_fileName);
            if ((testEncConfig->scaling_list_file = osal_fopen(testEncConfig->scaling_list_fileName, "r")) == NULL) {
                VLOG(ERR, "fail to open scaling list file, %s\n", testEncConfig->scaling_list_fileName);
                return FALSE;
            }
        }
    }

    if (ctx->encOpenParam.EncStdParam.waveParam.customLambdaEnable) {
        if (testEncConfig->custom_lambda_fileName) {
            ChangePathStyle(testEncConfig->custom_lambda_fileName);
            if ((testEncConfig->custom_lambda_file = osal_fopen(testEncConfig->custom_lambda_fileName, "r")) == NULL) {
                VLOG(ERR, "fail to open custom lambda file, %s\n", testEncConfig->custom_lambda_fileName);
                return FALSE;
            }
        }
    }

    if (testEncConfig->roi_enable) {
        if (testEncConfig->roi_file_name) {
            ChangePathStyle(testEncConfig->roi_file_name);
            if ((testEncConfig->roi_file = osal_fopen(testEncConfig->roi_file_name, "r")) == NULL) {
                VLOG(ERR, "fail to open ROI file, %s\n", testEncConfig->roi_file_name);
                return FALSE;
            }
        }
    }

    if (testEncConfig->lambda_map_enable) {
        if (testEncConfig->lambda_map_fileName) {
            ChangePathStyle(testEncConfig->lambda_map_fileName);
            if ((testEncConfig->lambda_map_file = osal_fopen(testEncConfig->lambda_map_fileName, "r")) == NULL) {
                VLOG(ERR, "fail to open lambda map file, %s\n", testEncConfig->lambda_map_fileName);
                return FALSE;
            }
        }
    }

    if (testEncConfig->mode_map_flag) {
        if (testEncConfig->mode_map_fileName) {
            ChangePathStyle(testEncConfig->mode_map_fileName);
            if ((testEncConfig->mode_map_file = osal_fopen(testEncConfig->mode_map_fileName, "r")) == NULL) {
                VLOG(ERR, "fail to open custom mode map file, %s\n", testEncConfig->mode_map_fileName);
                return FALSE;
            }
        }
    }

    if (testEncConfig->wp_param_flag & 0x1) {
        if (testEncConfig->wp_param_fileName) {
            ChangePathStyle(testEncConfig->wp_param_fileName);
            if ((testEncConfig->wp_param_file = osal_fopen(testEncConfig->wp_param_fileName, "r")) == NULL) {
                VLOG(ERR, "fail to open Weight Param file, %s\n", testEncConfig->wp_param_fileName);
                return FALSE;
            }
        }
    }


    *done = TRUE;

    return TRUE;
}

static void ReleaseEncoder(ComponentImpl* com)
{
    // Nothing to do
}

static BOOL DestroyEncoder(ComponentImpl* com)
{
    EncoderContext* ctx = (EncoderContext*)com->context;
    Uint32          i   = 0;
    BOOL            success = TRUE;
    ENC_INT_STATUS  intStatus;

    while (VPU_EncClose(ctx->handle) == RETCODE_VPU_STILL_RUNNING) {
        if ((intStatus = HandlingInterruptFlag(com)) == ENC_INT_STATUS_TIMEOUT) {
            HandleEncoderError(ctx->handle, ctx->frameIdx, NULL);
            VLOG(ERR, "NO RESPONSE FROM VPU_EncClose2()\n");
            success = FALSE;
            break;
        }
        else if (intStatus == ENC_INT_STATUS_DONE) {
            EncOutputInfo   outputInfo;
            VLOG(INFO, "VPU_EncClose() : CLEAR REMAIN INTERRUPT\n");
            VPU_EncGetOutputInfo(ctx->handle, &outputInfo);
            continue;
        }

        osal_msleep(10);
    }

    ComponentNotifyListeners(com, COMPONENT_EVENT_ENC_CLOSE, NULL);

    vdi_lock(ctx->testEncConfig.coreIdx);
    for (i = 0; i < ctx->fbCount.srcFbNum; i++) {
        if (ctx->vbCustomMap[i].size)
            vdi_free_dma_memory(ctx->testEncConfig.coreIdx, &ctx->vbCustomMap[i]);
    }
    if (ctx->vbCustomLambda.size)
        vdi_free_dma_memory(ctx->testEncConfig.coreIdx, &ctx->vbCustomLambda);
    if (ctx->vbScalingList.size)
        vdi_free_dma_memory(ctx->testEncConfig.coreIdx, &ctx->vbScalingList);
    vdi_unlock(ctx->testEncConfig.coreIdx);

    if (ctx->testEncConfig.roi_file)
        osal_fclose(ctx->testEncConfig.roi_file);
    if (ctx->testEncConfig.lambda_map_file)
        osal_fclose(ctx->testEncConfig.lambda_map_file);
    if (ctx->testEncConfig.mode_map_file)
        osal_fclose(ctx->testEncConfig.mode_map_file);
    if (ctx->testEncConfig.scaling_list_file)
        osal_fclose(ctx->testEncConfig.scaling_list_file);
    if (ctx->testEncConfig.custom_lambda_file)
        osal_fclose(ctx->testEncConfig.custom_lambda_file);
    if (ctx->testEncConfig.wp_param_file)
        osal_fclose(ctx->testEncConfig.wp_param_file);

    if (ctx->encOutQ) Queue_Destroy(ctx->encOutQ);

    VPU_DeInit(ctx->testEncConfig.coreIdx);

#ifdef __linux__
    pthread_barrier_destroy(&ctx->data_sync);
#elif _WIN32
    DeleteSynchronizationBarrier(&ctx->data_sync);
#endif

    osal_free(ctx);

    return success;
}

static Component CreateEncoder(ComponentImpl* com, CNMComponentConfig* componentParam)
{
    EncoderContext* ctx;
    RetCode         retCode;
    Uint32          coreIdx      = componentParam->testEncConfig.coreIdx;
    Uint16*         firmware     = (Uint16*)componentParam->bitcode;
    Uint32          firmwareSize = componentParam->sizeOfBitcode;
    Uint32          i;
    VpuAttr         productInfo;

    retCode = VPU_InitWithBitcode(coreIdx, firmware, firmwareSize);
    if (retCode != RETCODE_SUCCESS && retCode != RETCODE_CALLED_BEFORE) {
        VLOG(INFO, "%s:%d Failed to VPU_InitWidthBitCode, ret(%08x)\n", __FUNCTION__, __LINE__, retCode);
        return FALSE;
    }

    com->context = osal_malloc(sizeof(EncoderContext));
    ctx     = (EncoderContext*)com->context;
    osal_memset((void*)ctx, 0, sizeof(EncoderContext));

    retCode = PrintVpuProductInfo(coreIdx, &productInfo);
    if (retCode == RETCODE_VPU_RESPONSE_TIMEOUT ) {
        CNMErrorSet(CNM_ERROR_HANGUP);
        VLOG(INFO, "<%s:%d> Failed to PrintVpuProductInfo()\n", __FUNCTION__, __LINE__);
        HandleEncoderError(ctx->handle, 0, NULL);
        return FALSE;
    }
    ctx->cyclePerTick = 32768;
    if (TRUE == productInfo.supportNewTimer)
        ctx->cyclePerTick = 256;

    ctx->handle                      = NULL;
    ctx->frameIdx                    = 0;
    ctx->fbCount.reconFbNum          = 0;
    ctx->fbCount.srcFbNum            = 0;
    ctx->testEncConfig               = componentParam->testEncConfig;
    ctx->encOpenParam                = componentParam->encOpenParam;
    for (i=0; i<ENC_SRC_BUF_NUM ; i++ ) {
        ctx->encodedSrcFrmIdxArr[i] = 0;
        ctx->encodedSrcFrmIdxArr2[i] = 0;
    }
    osal_memset(&ctx->vbCustomLambda,  0x00, sizeof(vpu_buffer_t));
    osal_memset(&ctx->vbScalingList,   0x00, sizeof(vpu_buffer_t));
    osal_memset(&ctx->scalingList,     0x00, sizeof(UserScalingList));
    osal_memset(&ctx->customLambda[0], 0x00, sizeof(ctx->customLambda));
    osal_memset(ctx->vbCustomMap,      0x00, sizeof(ctx->vbCustomMap));
    com->numSinkPortQueue = componentParam->encOpenParam.streamBufCount;

    //pthread_barrier_init(&ctx->data_sync, NULL, 2);
#ifdef __linux__
    pthread_barrier_init(&ctx->data_sync, NULL, 2);
#elif _WIN32
    InitializeSynchronizationBarrier(&ctx->data_sync, 2, -1);
#elif defined(PLATFORM_NON_OS)
    ctx->data_sync = 2;
    ctx->data_sync1 = 2;
#endif
#ifdef VID_PERFORMANCE_TEST
    ctx->min_time = 10000000;
#endif

    return (Component)com;
}

ComponentImpl encoderComponentImpl = {
    "encoder",
    NULL,
    {0,},
    {0,},
    sizeof(PortContainerES),
    5,                       /* encoder's numSinkPortQueue(relates to streambufcount) */
    CreateEncoder,
    GetParameterEncoder,
    SetParameterEncoder,
    PrepareEncoder,
    ExecuteEncoder,
    ReleaseEncoder,
    DestroyEncoder
};

