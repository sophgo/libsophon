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
#include <string.h>
#include "component.h"
#include "cnm_app.h"
#include "misc/debug.h"
#include "wave5_regdefine.h"
#ifdef _WIN32
#include <time.h>
#elif __linux__
#include "sys/time.h"
#endif

#define EXTRA_FRAME_BUFFER_NUM 1

typedef enum {
    DEC_INT_STATUS_NONE,        // Interrupt not asserted yet
    DEC_INT_STATUS_EMPTY,       // Need more es
    DEC_INT_STATUS_DONE,        // Interrupt asserted
    DEC_INT_STATUS_TIMEOUT,     // Interrupt not asserted during given time.
} DEC_INT_STATUS;

typedef enum {
    DEC_STATE_NONE,
    DEC_STATE_OPEN_DECODER,
    DEC_STATE_INIT_SEQ,
    DEC_STATE_REGISTER_FB,
    DEC_STATE_DECODING,
    DEC_STATE_CLOSE,
} DecoderState;

typedef struct {
    TestDecConfig       testDecConfig;
    DecOpenParam        decOpenParam;
    DecParam            decParam;
    FrameBufferFormat   wtlFormat;
    DecHandle           handle;
    Uint64              startTimeout;
    vpu_buffer_t        vbUserData;
    BOOL                doFlush;
    BOOL                stateDoing;
    DecoderState        state;
    DecInitialInfo      initialInfo;
    Uint32              numDecoded;             /*!<< The number of decoded frames */
    Uint32              numOutput;
    PhysicalAddress     decodedAddr;
    Uint32              frameNumToStop;
    BOOL                doReset;
    BOOL                scenarioTest;
    Uint32              cyclePerTick;
    VpuAttr             attr;
#ifdef SUPPORT_RECOVER_AFTER_ERROR
    BOOL                doIframeSearchByError;
#endif
} DecoderContext;

static BOOL RegisterFrameBuffers(ComponentImpl* com)
{
    DecoderContext*         ctx               = (DecoderContext*)com->context;
    FrameBuffer*            pFrame            = NULL;
    Uint32                  framebufStride    = 0;
    ParamDecFrameBuffer     paramFb;
    RetCode                 result;
    DecInitialInfo*         codecInfo         = &ctx->initialInfo;
    BOOL                    success;
    CNMComponentParamRet    ret;
    CNMComListenerDecRegisterFb  lsnpRegisterFb;

    ctx->stateDoing = TRUE;
    ret = ComponentGetParameter(com, com->sinkPort.connectedComponent, GET_PARAM_RENDERER_FRAME_BUF, (void*)&paramFb);
    if (ComponentParamReturnTest(ret, &success) == FALSE) {
        return success;
    }

    ret = ComponentGetParameter(com, com->sinkPort.connectedComponent, GET_PARAM_DEC_FRAME_BUF_STRIDE, &framebufStride);
    if (ComponentParamReturnTest(ret, &success) == FALSE) {
        return success;
    }

    pFrame               = paramFb.fb;
    framebufStride       = paramFb.stride;
    VLOG(TRACE, "<%s> COMPRESSED: %d, LINEAR: %d\n", __FUNCTION__, paramFb.compressedNum, paramFb.linearNum);
    result = VPU_DecRegisterFrameBufferEx(ctx->handle, pFrame, paramFb.compressedNum, paramFb.linearNum, framebufStride, codecInfo->picHeight, COMPRESSED_FRAME_MAP);
#if (defined(WAVE512_PALLADIUM) || defined(CODA960_PALLADIUM))
    for(int i = 0; i< paramFb.compressedNum + paramFb.linearNum; i++)
        VLOG(INFO, "frame buffer %d: bufY 0x%lx  bufCb 0x%lx bufCr 0x%lx botY 0x%x botCb 0x%x botCr 0x%x\n", \
                i, pFrame[i].bufY, pFrame[i].bufCb, pFrame[i].bufCr, pFrame[i].bufYBot, pFrame[i].bufCbBot, pFrame[i].bufCrBot);
#endif
    lsnpRegisterFb.handle          = ctx->handle;
    lsnpRegisterFb.numCompressedFb = paramFb.compressedNum;
    lsnpRegisterFb.numLinearFb     = paramFb.linearNum;
    ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_REGISTER_FB, (void*)&lsnpRegisterFb);

    if (result != RETCODE_SUCCESS) {
        VLOG(ERR, "%s:%d Failed to VPU_DecRegisterFrameBufferEx(%d)\n", __FUNCTION__, __LINE__, result);
        return FALSE;
    }

    ctx->stateDoing = FALSE;

    return TRUE;
}

static BOOL SequenceChange(ComponentImpl* com, DecOutputInfo* outputInfo)
{
    DecoderContext* ctx               = (DecoderContext*)com->context;
    BOOL            dpbChanged, sizeChanged, bitDepthChanged;
    Uint32          sequenceChangeFlag = outputInfo->sequenceChanged;

    dpbChanged      = (sequenceChangeFlag&SEQ_CHANGE_ENABLE_DPB_COUNT) ? TRUE : FALSE;
    sizeChanged     = (sequenceChangeFlag&SEQ_CHANGE_ENABLE_SIZE)      ? TRUE : FALSE;
    bitDepthChanged = (sequenceChangeFlag&SEQ_CHANGE_ENABLE_BITDEPTH)  ? TRUE : FALSE;

    if (dpbChanged || sizeChanged || bitDepthChanged) {
        DecInitialInfo  initialInfo;

        VLOG(INFO, "----- SEQUENCE CHANGED -----\n");
        // Get current(changed) sequence information.
        VPU_DecGiveCommand(ctx->handle, DEC_GET_SEQ_INFO, &initialInfo);
        // Flush all remaining framebuffers of previous sequence.

        VLOG(INFO, "sequenceChanged : %x\n", sequenceChangeFlag);
        VLOG(INFO, "SEQUENCE NO     : %d\n", initialInfo.sequenceNo);
        VLOG(INFO, "DPB COUNT       : %d\n", initialInfo.minFrameBufferCount);
        VLOG(INFO, "BITDEPTH        : LUMA(%d), CHROMA(%d)\n", initialInfo.lumaBitdepth, initialInfo.chromaBitdepth);
        VLOG(INFO, "SIZE            : WIDTH(%d), HEIGHT(%d)\n", initialInfo.picWidth, initialInfo.picHeight);

        ComponentSetParameter(com, com->sinkPort.connectedComponent, SET_PARAM_RENDERER_FREE_FRAMEBUFFERS, (void*)&outputInfo->frameDisplayFlag);

        VPU_DecGiveCommand(ctx->handle, DEC_RESET_FRAMEBUF_INFO, NULL);

        if (ctx->testDecConfig.scaleDownWidth > 0 || ctx->testDecConfig.scaleDownHeight > 0) {
            ScalerInfo sclInfo = {0};

            sclInfo.scaleWidth  = CalcScaleDown(initialInfo.picWidth, ctx->testDecConfig.scaleDownWidth);
            sclInfo.scaleHeight = CalcScaleDown(initialInfo.picHeight, ctx->testDecConfig.scaleDownHeight);
            VLOG(INFO, "[SCALE INFO] %dx%d to %dx%d\n", initialInfo.picWidth, initialInfo.picHeight, sclInfo.scaleWidth, sclInfo.scaleHeight);
            sclInfo.enScaler    = TRUE;
            if (VPU_DecGiveCommand(ctx->handle, DEC_SET_SCALER_INFO, (void*)&sclInfo) != RETCODE_SUCCESS) {
                VLOG(ERR, "Failed to VPU_DecGiveCommand(DEC_SET_SCALER_INFO)\n");
                return FALSE;
            }
        }
        ctx->state = DEC_STATE_REGISTER_FB;
        osal_memcpy((void*)&ctx->initialInfo, (void*)&initialInfo, sizeof(DecInitialInfo));
        ComponentSetParameter(com, com->sinkPort.connectedComponent, SET_PARAM_RENDERER_ALLOC_FRAMEBUFFERS, NULL);

        VLOG(INFO, "----------------------------\n");
    }

    return TRUE;
}

static BOOL CheckAndDoSequenceChange(ComponentImpl* com, DecOutputInfo* outputInfo)
{
    if (outputInfo->sequenceChanged == 0) {
        return TRUE;
    }
    else {
        return SequenceChange(com, outputInfo);
    }
}

static BOOL PrepareSkip(ComponentImpl* com)
{
    DecoderContext*   ctx = (DecoderContext*)com->context;

    // Flush the decoder
    if (ctx->doFlush == TRUE) {
        Uint32          timeoutCount;
        Uint32          intReason;
        PhysicalAddress curRdPtr, curWrPtr;
        DecOutputInfo   outputInfo;

        VLOG(INFO, "========== FLUSH RENDERER           ========== \n");
        /* Send the renderer the signal to drop all frames. */
        ComponentSetParameter(com, com->sinkPort.connectedComponent, SET_PARAM_RENDERER_FLUSH, NULL);

        VLOG(INFO, "========== READY TO SEARCH KEYFRAME ========== \n");
        timeoutCount = 0;
        while (VPU_DecFrameBufferFlush(ctx->handle, NULL, NULL) == RETCODE_VPU_STILL_RUNNING) {
            osal_msleep(1);
            if (timeoutCount >= VPU_BUSY_CHECK_TIMEOUT) {
                VLOG(ERR, "NO RESPONSE FROM VPU_DecFrameBufferFlush()\n");
                return FALSE;
            }
            timeoutCount++;
        }

        VLOG(INFO, "========== CLEAR PENDING INTERRUPT ========== \n");
        intReason = VPU_WaitInterruptEx(ctx->handle, VPU_WAIT_TIME_OUT_CQ);
        if (intReason > 0) {
            VPU_ClearInterruptEx(ctx->handle, intReason);
            VPU_DecGetOutputInfo(ctx->handle, &outputInfo);  // ignore the return value and outputinfo
        }

        /* Clear CPB */
        VPU_DecGetBitstreamBuffer(ctx->handle, &curRdPtr, &curWrPtr, NULL);
        VPU_DecSetRdPtr(ctx->handle, curWrPtr, TRUE);
        ctx->doFlush = FALSE;
    }

    return TRUE;
}

static DEC_INT_STATUS HandlingInterruptFlag(ComponentImpl* com)
{
    DecoderContext*      ctx               = (DecoderContext*)com->context;
    DecHandle            handle            = ctx->handle;
    Int32                interruptFlag     = 0;
    Uint32               interruptWaitTime = VPU_WAIT_TIME_OUT_CQ;
    Uint32               interruptTimeout  = VPU_DEC_TIMEOUT;
    DEC_INT_STATUS       status            = DEC_INT_STATUS_NONE;
    CNMComListenerDecInt lsn;

    if (ctx->startTimeout == 0ULL) {
        ctx->startTimeout = osal_gettime();
    }
    do {
        interruptFlag = VPU_WaitInterruptEx(handle, interruptWaitTime);
        if (interruptFlag == -1) {
            Uint64   currentTimeout = osal_gettime();
            if ((currentTimeout - ctx->startTimeout) > interruptTimeout) {
                if (ctx->scenarioTest == FALSE) {
                    CNMErrorSet(CNM_ERROR_HANGUP);
                }
                VLOG(ERR, "\n INSNTANCE #%d INTERRUPT TIMEOUT.\n", handle->instIndex);
                status = DEC_INT_STATUS_TIMEOUT;
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
            status = DEC_INT_STATUS_DONE;
        }

        if (interruptFlag & (1<<INT_WAVE5_INIT_SEQ)) {
            break;
        }

        if (interruptFlag & (1<<INT_WAVE5_DEC_PIC)) {
            break;
        }

        if (interruptFlag & (1<<INT_WAVE5_BSBUF_EMPTY)) {
            status = DEC_INT_STATUS_EMPTY;
            break;
        }
    } while (FALSE);

    if (interruptFlag != 0) {
        lsn.handle = handle;
        lsn.flag   = interruptFlag;
        ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_INTERRUPT, (void*)&lsn);
    }

    return status;
}

static BOOL DoReset(ComponentImpl* com)
{
    DecoderContext* ctx    = (DecoderContext*)com->context;
    BitStreamMode   bsMode = ctx->decOpenParam.bitstreamMode;
    DecHandle       handle = ctx->handle;
    Uint32          timeoutCount;
    BOOL            pause  = TRUE;

    VLOG(INFO, "========== %s ==========\n", __FUNCTION__);

    ComponentSetParameter(com, com->srcPort.connectedComponent, SET_PARAM_COM_PAUSE, (void*)&pause);
    ComponentSetParameter(com, com->srcPort.connectedComponent, SET_PARAM_FEEDER_RESET, (void*)&pause);

    VLOG(INFO, "> FLUSH RENDERER\n");
    /* Send the renderer the signal to drop all frames. */
    ComponentSetParameter(com, com->sinkPort.connectedComponent, SET_PARAM_RENDERER_FLUSH, NULL);

    VLOG(INFO, "> EXPLICIT_END_SET_FLAG\n");
    // In order to stop processing bitstream.
    VPU_DecUpdateBitstreamBuffer(handle, EXPLICIT_END_SET_FLAG);

    // Clear DPB
    timeoutCount = 0;
    VLOG(INFO, "> Flush VPU internal buffer\n");
    while (VPU_DecFrameBufferFlush(handle, NULL, NULL) == RETCODE_VPU_STILL_RUNNING) {
        if (timeoutCount >= VPU_DEC_TIMEOUT) {
            VLOG(ERR, "NO RESPONSE FROM VPU_DecFrameBufferFlush()\n");
            return FALSE;
        }
        timeoutCount++;
    }

    VLOG(INFO, "> Reset VPU\n");
    if (VPU_SWReset(handle->coreIdx, SW_RESET_SAFETY, handle) != RETCODE_SUCCESS) {
        VLOG(ERR, "<%s:%d> Failed to VPU_SWReset()\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    // Clear CPB
    if (bsMode == BS_MODE_INTERRUPT) {
        PhysicalAddress newPtr;
        newPtr = ctx->decOpenParam.bitstreamBuffer;
        VPU_DecSetRdPtr(handle, newPtr, TRUE);
        VLOG(INFO, "> Clear CPB: %08x\n", newPtr);
    }

    VLOG(INFO, "> STREAM_END_CLEAR_FLAG\n");
    // Clear stream-end flag
    VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_CLEAR_FLAG);

    VLOG(INFO, "========== %s ==========\n", __FUNCTION__);

    ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_RESET_DONE, NULL);

    VLOG(INFO, "> FLUSH INPUT PORT\n");
    ComponentPortFlush(com);

    pause = FALSE;
    ComponentSetParameter(com, com->srcPort.connectedComponent, SET_PARAM_COM_PAUSE, (void*)&pause);

    ctx->doReset      = FALSE;
    ctx->startTimeout = 0ULL;
    return TRUE;
}

static BOOL Decode(ComponentImpl* com, PortContainerES* in, PortContainerDisplay* out)
{
    DecoderContext*                 ctx           = (DecoderContext*)com->context;
    DecOutputInfo                   decOutputInfo;
    DEC_INT_STATUS                  intStatus;
    CNMComListenerStartDec          lsnpPicStart;
    BitStreamMode                   bsMode        = ctx->decOpenParam.bitstreamMode;
    RetCode                         result;
    CNMComListenerDecDone           lsnpPicDone;
    CNMComListenerDecReadyOneFrame  lsnpReadyOneFrame = {0,};
    BOOL                            doDecode;

    lsnpReadyOneFrame.handle = ctx->handle;
    ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_READY_ONE_FRAME, (void*)&lsnpReadyOneFrame);

    ctx->stateDoing = TRUE;

    if (PrepareSkip(com) == FALSE) {
        return FALSE;
    }

    /* decode a frame except when the bitstream mode is PIC_END and no data */
    doDecode  = !(bsMode == BS_MODE_PIC_END && in == NULL);
    doDecode &= (BOOL)(com->pause == FALSE);
    if (doDecode == TRUE) {
        result = VPU_DecStartOneFrame(ctx->handle, &ctx->decParam);

        lsnpPicStart.result   = result;

#if (defined(WAVE512_PALLADIUM) || defined(CODA960_PALLADIUM))

        int *att_test = (int *)0x4F0000084;

        if(*att_test == 1)
        {
            unsigned int *rand_att = (unsigned int *)(0x50010184);
            unsigned int rand_use = *rand_att & 0xf;
            for(int idx_att = 0; idx_att < rand_use; idx_att++)
                printf("idx_att delay times = %d\n",idx_att);

           int *att_intr_mask  = (int *)0x50112014;
           int *att_guide_time = (int *)0x50112004;
           int *att_time_out   = (int *)0x50112008;
           int *att_ctrl       = (int *)0x50112000;
           int *att_intr       = (int *)0x50112010;
           int *att_clear      = (int *)0x50112018;

           *att_intr_mask  = 0xfc;
           *att_guide_time = 0x40;
           *att_time_out   = 0xfffffffc;
           printf("vpu dec att will tarnsform terminal mode\n");
           *att_ctrl       = 0x1;
           printf("vpu dec att tarnsform terminal mode success\n");

           while(1){
                if((*att_intr & 0x1) == 1){
                    VLOG(INFO, "<%s:%d> att reset success intr_val(%d)\n", __FUNCTION__, __LINE__, *att_intr);
                    *att_clear = 0x3;
                    printf("vpu will reset...\n");
                    int ret_reset = VPU_HWReset(0);
                    if(ret_reset == 0)
                        printf("vpu reset success\n");
                    while(1){
                        int *productId= (int *)0x50051044;
                        printf("vpu productId=%d\n",*productId);
                    }
                    break;
                }
                else if((*att_intr & 0x02) == 2){
                    VLOG(INFO, "<%s:%d> att reset timeout intr_val(%d)\n", __FUNCTION__, __LINE__, *att_intr);
                    *att_clear = 0x3;
                    break;
                }
                else{
                    VLOG(INFO, "<%s:%d> att reset check result(%d)\n", __FUNCTION__, __LINE__, *att_intr);
                }
           }
        }

#endif
        //lsnpPicStart.decParam = ctx->decParam;
        osal_memcpy(&(lsnpPicStart.decParam), &(ctx->decParam), sizeof(lsnpPicStart.decParam));
        ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_START_ONE_FRAME, (void*)&lsnpPicStart);

        if (result == RETCODE_SUCCESS) {
            ;   // Nothing to do
        }
        else if (result == RETCODE_QUEUEING_FAILURE) {
            QueueStatusInfo qStatus;
            // Just retry
            if (in) in->reuse = (bsMode == BS_MODE_PIC_END);
            VPU_DecGiveCommand(ctx->handle, DEC_GET_QUEUE_STATUS, (void*)&qStatus);
            if (qStatus.instanceQueueCount == 0) {
                return TRUE;
            }
        }
        else if (result == RETCODE_VPU_RESPONSE_TIMEOUT ) {
            VLOG(INFO, "<%s:%d> Failed to VPU_DecStartOneFrame() ret(%d)\n", __FUNCTION__, __LINE__, result);
            CNMErrorSet(CNM_ERROR_HANGUP);
            HandleDecoderError(ctx->handle, ctx->numDecoded, NULL);
            PrintDecVpuStatus(ctx->handle);
            return FALSE;
        }
        else {
            return FALSE;
        }
    }

    if ((intStatus=HandlingInterruptFlag(com)) == DEC_INT_STATUS_TIMEOUT) {
        if (ctx->scenarioTest == TRUE) {
            VLOG(ERR, "<%s:%d> VPU INTERRUTP TIMEOUT : GO TO RESET PROCESS\n", __FUNCTION__, __LINE__);
            ctx->doReset = TRUE;
            return TRUE;
        }
        else {
            HandleDecoderError(ctx->handle, ctx->numDecoded, NULL);
            PrintDecVpuStatus(ctx->handle);
            DoReset(com);
            return FALSE;
        }
    }
    else if (intStatus == DEC_INT_STATUS_NONE || intStatus == DEC_INT_STATUS_EMPTY) {
        return TRUE;    // Try again
    }


    // Get data from the sink component.
    if ((result=VPU_DecGetOutputInfo(ctx->handle, &decOutputInfo)) == RETCODE_SUCCESS) {
        DisplayDecodedInformation(ctx->handle, ctx->decOpenParam.bitstreamFormat, ctx->numDecoded, &decOutputInfo, ctx->testDecConfig.performance, ctx->cyclePerTick);
    }

    lsnpPicDone.handle     = ctx->handle;
    lsnpPicDone.ret        = result;
    lsnpPicDone.decParam   = &ctx->decParam;
    lsnpPicDone.output     = &decOutputInfo;
    lsnpPicDone.numDecoded = ctx->numDecoded;
    lsnpPicDone.vbUser     = ctx->vbUserData;
    ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_GET_OUTPUT_INFO, (void*)&lsnpPicDone);

    if (result == RETCODE_REPORT_NOT_READY) {
        return TRUE; // Not decoded yet. Try again
    }
    else if (result != RETCODE_SUCCESS) {
        /* ERROR */
        VLOG(ERR, "Failed to decode error\n");
        return FALSE;
    }


    if ((decOutputInfo.decodingSuccess & 0x01) == 0) {
        VLOG(ERR, "VPU_DecGetOutputInfo decode fail framdIdx %d error(0x%08x) reason(0x%08x), reasonExt(0x%08x)\n",
            ctx->numDecoded, decOutputInfo.decodingSuccess, decOutputInfo.errorReason, decOutputInfo.errorReasonExt);
        if (decOutputInfo.errorReason == WAVE5_SYSERR_WATCHDOG_TIMEOUT) {
            VLOG(ERR, "WAVE5_SYSERR_WATCHDOG_TIMEOUT\n");
            if (ctx->scenarioTest == FALSE) {
                HandleDecoderError(ctx->handle, ctx->numDecoded, &decOutputInfo);
                CNMErrorSet(CNM_ERROR_HANGUP);
                return FALSE;
            }
        }
        else if (decOutputInfo.errorReason == WAVE5_SPECERR_OVER_PICTURE_WIDTH_SIZE || decOutputInfo.errorReason == WAVE5_SPECERR_OVER_PICTURE_HEIGHT_SIZE) {
            VLOG(ERR, "Not supported Width or Height\n");
            return FALSE;
        }
    }

    if (CheckAndDoSequenceChange(com, &decOutputInfo) == FALSE) {
        return FALSE;
    }

    if (decOutputInfo.indexFrameDecoded >= 0 || decOutputInfo.indexFrameDecoded == DECODED_IDX_FLAG_SKIP) {
        // Return a used data to a source port.
        ctx->numDecoded++;
        ctx->decodedAddr = decOutputInfo.bytePosFrameStart;
        out->reuse = FALSE;
    }

    if ((decOutputInfo.indexFrameDisplay >= 0) || (decOutputInfo.indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END)) {
        ctx->numOutput++;
        out->last  = (BOOL)(decOutputInfo.indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END);
        out->reuse = FALSE;
    }
    if (decOutputInfo.indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END) {
        ctx->stateDoing = FALSE;
        com->terminate  = TRUE;
    }
    if (out->reuse == FALSE) {
        osal_memcpy((void*)&out->decInfo, (void*)&decOutputInfo, sizeof(DecOutputInfo));
    }

    if (ctx->frameNumToStop > 0) {
        if (ctx->frameNumToStop == ctx->numOutput) {
            com->terminate = TRUE;
        }
    }

    return TRUE;
}

static CNMComponentParamRet GetParameterDecoder(ComponentImpl* from, ComponentImpl* com, GetParameterCMD commandType, void* data)
{
    DecoderContext*             ctx     = (DecoderContext*)com->context;
    BOOL                        result  = TRUE;
    PhysicalAddress             rdPtr, wrPtr;
    Uint32                      room;
    ParamDecBitstreamBufPos*    bsPos   = NULL;
    ParamDecNeedFrameBufferNum* fbNum;
    ParamDecStatus*             status;
    QueueStatusInfo             cqInfo;
    PortContainerES*            container;
    vpu_buffer_t                vb;

    if (ctx->handle == NULL)  return CNM_COMPONENT_PARAM_NOT_READY;
    if (ctx->doReset == TRUE) return CNM_COMPONENT_PARAM_NOT_READY;

    switch(commandType) {
    case GET_PARAM_COM_IS_CONTAINER_CONUSUMED:
        // This query command is sent from the comonponent core.
        // If input data are consumed in sequence, it should return TRUE through PortContainer::consumed.
        container = (PortContainerES*)data;
        vb = container->buf;
        if (vb.phys_addr <= ctx->decodedAddr && ctx->decodedAddr < (vb.phys_addr+vb.size)) {
            container->consumed = TRUE;
            ctx->decodedAddr = 0;
        }
        break;
    case GET_PARAM_DEC_HANDLE:
        *(DecHandle*)data = ctx->handle;
        break;
    case GET_PARAM_DEC_FRAME_BUF_NUM:
        if (ctx->state <= DEC_STATE_INIT_SEQ) return CNM_COMPONENT_PARAM_NOT_READY;
        fbNum = (ParamDecNeedFrameBufferNum*)data;
        fbNum->compressedNum = ctx->initialInfo.minFrameBufferCount + EXTRA_FRAME_BUFFER_NUM;   // max_dec_pic_buffering
        if (ctx->decOpenParam.wtlEnable == TRUE) {
            fbNum->linearNum = (ctx->initialInfo.frameBufDelay+1) + EXTRA_FRAME_BUFFER_NUM;     // The frameBufDelay can be zero.
            if ((ctx->decOpenParam.bitstreamFormat == STD_VP9) || (ctx->decOpenParam.bitstreamFormat == STD_AVS2)) {
                fbNum->linearNum = fbNum->compressedNum;
            }
            if (ctx->testDecConfig.performance == TRUE) {
                if ((ctx->decOpenParam.bitstreamFormat == STD_VP9) || (ctx->decOpenParam.bitstreamFormat == STD_AVS2)) {
                    fbNum->linearNum++;
                    fbNum->compressedNum++;
                }
                else {
                    fbNum->linearNum += 3;
                }
            }

        }
        else {
            fbNum->linearNum = 0;
        }
        break;
    case GET_PARAM_DEC_BITSTREAM_BUF_POS:
        if (ctx->state < DEC_STATE_INIT_SEQ) return CNM_COMPONENT_PARAM_NOT_READY;
        VPU_DecGetBitstreamBuffer(ctx->handle, &rdPtr, &wrPtr, &room);
        bsPos = (ParamDecBitstreamBufPos*)data;
        bsPos->rdPtr = rdPtr;
        bsPos->wrPtr = wrPtr;
        bsPos->avail = room;
        break;
    case GET_PARAM_DEC_CODEC_INFO:
        if (ctx->state <= DEC_STATE_INIT_SEQ) return CNM_COMPONENT_PARAM_NOT_READY;
        VPU_DecGiveCommand(ctx->handle, DEC_GET_SEQ_INFO, data);
        break;
    case GET_PARAM_DEC_QUEUE_STATUS:
        if (ctx->state != DEC_STATE_DECODING) return CNM_COMPONENT_PARAM_NOT_READY;
        VPU_DecGiveCommand(ctx->handle, DEC_GET_QUEUE_STATUS, &cqInfo);
        status = (ParamDecStatus*)data;
        status->cq = cqInfo;
        break;
    default:
        result = FALSE;
        break;
    }

    return (result == TRUE) ? CNM_COMPONENT_PARAM_SUCCESS : CNM_COMPONENT_PARAM_FAILURE;
}

static CNMComponentParamRet SetParameterDecoder(ComponentImpl* from, ComponentImpl* com, SetParameterCMD commandType, void* data)
{
    BOOL            result = TRUE;
    DecoderContext* ctx    = (DecoderContext*)com->context;
    Int32           skipCmd;

    switch(commandType) {
    case SET_PARAM_COM_PAUSE:
        com->pause   = *(BOOL*)data;
        break;
    case SET_PARAM_DEC_SKIP_COMMAND:
        skipCmd = *(Int32*)data;
        ctx->decParam.skipframeMode = skipCmd;
        if (skipCmd == WAVE_SKIPMODE_NON_IRAP) {
            Uint32 userDataMask = (1<<H265_USERDATA_FLAG_RECOVERY_POINT);
            ctx->decParam.craAsBlaFlag = TRUE;
            /* For finding recovery point */
            VPU_DecGiveCommand(ctx->handle, ENABLE_REP_USERDATA, &userDataMask);
        }
        else {
            ctx->decParam.craAsBlaFlag = FALSE;
        }
        if (ctx->numDecoded > 0) {
            ctx->doFlush = (BOOL)(ctx->decParam.skipframeMode == WAVE_SKIPMODE_NON_IRAP);
        }
        break;
    case SET_PARAM_DEC_RESET:
        ctx->doReset = TRUE;
        break;
    default:
        result = FALSE;
        break;
    }

    return (result == TRUE) ? CNM_COMPONENT_PARAM_SUCCESS : CNM_COMPONENT_PARAM_FAILURE;
}

static BOOL UpdateBitstream(DecoderContext* ctx, PortContainerES* in)
{
    RetCode       ret    = RETCODE_SUCCESS;
    BitStreamMode bsMode = ctx->decOpenParam.bitstreamMode;
    BOOL          update = TRUE;
    Uint32        updateSize;

    if (in == NULL) return TRUE;

    if (bsMode == BS_MODE_PIC_END) {
        VPU_DecSetRdPtr(ctx->handle, in->buf.phys_addr, TRUE);
    }
    else {
        if (in->size > 0) {
            PhysicalAddress rdPtr, wrPtr;
            Uint32          room;
            VPU_DecGetBitstreamBuffer(ctx->handle, &rdPtr, &wrPtr, &room);
            if ((Int32)room < in->size) {
                in->reuse = TRUE;
                return TRUE;
            }
        }
    }

    if (in->last == TRUE) {
        updateSize = (in->size == 0) ? STREAM_END_SET_FLAG : in->size;
    }
    else {
        updateSize = in->size;
        update     = (in->size > 0 && in->last == FALSE);
    }

    if (update == TRUE) {
        if ((ret=VPU_DecUpdateBitstreamBuffer(ctx->handle, updateSize)) != RETCODE_SUCCESS) {
            VLOG(INFO, "<%s:%d> Failed to VPU_DecUpdateBitstreamBuffer() ret(%d)\n", __FUNCTION__, __LINE__, ret);
            return FALSE;
        }
        if (in->last == TRUE) {
            VPU_DecUpdateBitstreamBuffer(ctx->handle, STREAM_END_SET_FLAG);
        }
    }

    in->reuse = FALSE;

    return TRUE;
}

static BOOL OpenDecoder(ComponentImpl* com)
{
    DecoderContext*         ctx     = (DecoderContext*)com->context;
    ParamDecBitstreamBuffer bsBuf;
    CNMComponentParamRet    ret;
    CNMComListenerDecOpen   lspn    = {0};
    BOOL                    success = FALSE;
    BitStreamMode           bsMode  = ctx->decOpenParam.bitstreamMode;
    vpu_buffer_t            vbUserData;
    RetCode                 retCode;

    ctx->stateDoing = TRUE;
    ret = ComponentGetParameter(com, com->srcPort.connectedComponent, GET_PARAM_FEEDER_BITSTREAM_BUF, &bsBuf);
    if (ComponentParamReturnTest(ret, &success) == FALSE) {
        return success;
    }

    ctx->decOpenParam.bitstreamBuffer     = (bsMode == BS_MODE_PIC_END) ? 0 : bsBuf.bs->phys_addr;
    ctx->decOpenParam.bitstreamBufferSize = (bsMode == BS_MODE_PIC_END) ? 0 : bsBuf.bs->size;
    retCode = VPU_DecOpen(&ctx->handle, &ctx->decOpenParam);

    lspn.handle = ctx->handle;
    lspn.ret    = retCode;
    ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_OPEN, (void*)&lspn);

    if (retCode != RETCODE_SUCCESS) {
        VLOG(ERR, "<%s:%d> Failed to VPU_DecOpen(ret:%d)\n", __FUNCTION__, __LINE__, retCode);
        if ( retCode == RETCODE_VPU_RESPONSE_TIMEOUT)
            CNMErrorSet(CNM_ERROR_HANGUP);

        HandleDecoderError(NULL, 0, NULL);
        return FALSE;
    }
    // VPU_DecGiveCommand(ctx->handle, ENABLE_LOGGING, 0);

    if (ctx->vbUserData.size == 0) {
        vbUserData.size = (512*1024);
        vbUserData.enable_cache = 0;
        vdi_lock(ctx->testDecConfig.coreIdx);
        vdi_allocate_dma_memory(ctx->testDecConfig.coreIdx, &vbUserData);
        vdi_unlock(ctx->testDecConfig.coreIdx);
    }
    VPU_DecGiveCommand(ctx->handle, SET_ADDR_REP_USERDATA, (void*)&vbUserData.phys_addr);
    VPU_DecGiveCommand(ctx->handle, SET_SIZE_REP_USERDATA, (void*)&vbUserData.size);
    VPU_DecGiveCommand(ctx->handle, ENABLE_REP_USERDATA,   (void*)&ctx->testDecConfig.enableUserData);

    VPU_DecGiveCommand(ctx->handle, SET_CYCLE_PER_TICK,   (void*)&ctx->cyclePerTick);
    if (ctx->testDecConfig.thumbnailMode == TRUE) {
        VPU_DecGiveCommand(ctx->handle, ENABLE_DEC_THUMBNAIL_MODE, NULL);
    }

    ctx->vbUserData = vbUserData;
    ctx->stateDoing = FALSE;

    return TRUE;
}

static BOOL DecodeHeader(ComponentImpl* com)
{
    DecoderContext*                ctx     = (DecoderContext*)com->context;
    DecHandle                      handle  = ctx->handle;
    Uint32                         coreIdx = ctx->testDecConfig.coreIdx;
    RetCode                        ret     = RETCODE_SUCCESS;
    DEC_INT_STATUS                 status;
    DecInitialInfo*                initialInfo = &ctx->initialInfo;
    SecAxiUse                      secAxiUse;
    CNMComListenerDecCompleteSeq   lsnpCompleteSeq;

    if (ctx->stateDoing == FALSE) {
        /* previous state done */
        do {
            ret = VPU_DecIssueSeqInit(handle);
        } while (ret == RETCODE_QUEUEING_FAILURE);
        ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_ISSUE_SEQ, NULL);
        if (ret != RETCODE_SUCCESS) {
            VLOG(ERR, "%s:%d Failed to VPU_DecIssueSeqInit() ret(%d)\n", __FUNCTION__, __LINE__, ret);
            return FALSE;
        }
    }

    ctx->stateDoing = TRUE;

    while (com->terminate == FALSE) {
        if ((status=HandlingInterruptFlag(com)) == DEC_INT_STATUS_DONE) {
            break;
        }
        else if (status == DEC_INT_STATUS_TIMEOUT) {
            VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);    /* To finish bitstream empty status */
            VPU_SWReset(coreIdx, SW_RESET_SAFETY, handle);
            VPU_DecUpdateBitstreamBuffer(handle, STREAM_END_CLEAR_FLAG);    /* To finish bitstream empty status */
            return FALSE;
        }
        else if (status == DEC_INT_STATUS_EMPTY) {
            return TRUE;
        }
        else if (status == DEC_INT_STATUS_NONE) {
            return TRUE;
        }
        else {
            VLOG(INFO, "%s:%d Unknown interrupt status: %d\n", __FUNCTION__, __LINE__, status);
            return FALSE;
        }
    }

    ret = VPU_DecCompleteSeqInit(handle, initialInfo);

    strcpy(lsnpCompleteSeq.refYuvPath, ctx->testDecConfig.refYuvPath);
    lsnpCompleteSeq.ret             = ret;
    lsnpCompleteSeq.initialInfo     = initialInfo;
    lsnpCompleteSeq.wtlFormat       = ctx->wtlFormat;
    lsnpCompleteSeq.cbcrInterleave  = ctx->decOpenParam.cbcrInterleave;
    lsnpCompleteSeq.bitstreamFormat = ctx->decOpenParam.bitstreamFormat;
    ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_COMPLETE_SEQ, (void*)&lsnpCompleteSeq);

    if (ret != RETCODE_SUCCESS) {
        VLOG(ERR, "%s:%d FAILED TO DEC_PIC_HDR: ret(%d), SEQERR(%08x)\n", __FUNCTION__, __LINE__, ret, initialInfo->seqInitErrReason);
        return FALSE;
    }

    if (ctx->decOpenParam.wtlEnable == TRUE) {
        VPU_DecGiveCommand(ctx->handle, DEC_SET_WTL_FRAME_FORMAT, &ctx->wtlFormat);
    }

   /* Set up the secondary AXI is depending on H/W configuration.
    * Note that turn off all the secondary AXI configuration
    * if target ASIC has no memory only for IP, LF and BIT.
    */
    secAxiUse.u.wave.useIpEnable    = (ctx->testDecConfig.secondaryAXI&0x01) ? TRUE : FALSE;
    secAxiUse.u.wave.useLfRowEnable = (ctx->testDecConfig.secondaryAXI&0x02) ? TRUE : FALSE;
    secAxiUse.u.wave.useBitEnable   = (ctx->testDecConfig.secondaryAXI&0x04) ? TRUE : FALSE;
    secAxiUse.u.wave.useSclEnable   = (ctx->testDecConfig.secondaryAXI&0x08) ? TRUE : FALSE;
    VPU_DecGiveCommand(ctx->handle, SET_SEC_AXI, &secAxiUse);
    // Set up scale
    if (ctx->testDecConfig.scaleDownWidth > 0 || ctx->testDecConfig.scaleDownHeight > 0) {
        ScalerInfo sclInfo = {0};

        sclInfo.scaleWidth  = ctx->testDecConfig.scaleDownWidth;
        sclInfo.scaleHeight = ctx->testDecConfig.scaleDownHeight;
        VLOG(INFO, "[SCALE INFO] %dx%d\n", sclInfo.scaleWidth, sclInfo.scaleHeight);
        sclInfo.enScaler    = TRUE;
        if (VPU_DecGiveCommand(ctx->handle, DEC_SET_SCALER_INFO, (void*)&sclInfo) != RETCODE_SUCCESS) {
            VLOG(ERR, "Failed to VPU_DecGiveCommand(DEC_SET_SCALER_INFO)\n");
            return FALSE;
        }
    }

    ctx->stateDoing = FALSE;

    return TRUE;
}

static BOOL ExecuteDecoder(ComponentImpl* com, PortContainer* in , PortContainer* out)
{
    DecoderContext* ctx    = (DecoderContext*)com->context;
    BOOL            ret    = FALSE;;
    BitStreamMode   bsMode = ctx->decOpenParam.bitstreamMode;

    if (in)  in->reuse = TRUE;
    if (out) out->reuse = TRUE;
    if (ctx->state == DEC_STATE_INIT_SEQ || ctx->state == DEC_STATE_DECODING) {
        if (UpdateBitstream(ctx, (PortContainerES*)in) == FALSE) {
            return FALSE;
        }
        if (in) {
            // In ring-buffer mode, it has to return back a container immediately.
            if (bsMode == BS_MODE_PIC_END) {
                if (ctx->state == DEC_STATE_INIT_SEQ) {
                    in->reuse = TRUE;
                }
                in->consumed = FALSE;
            }
            else {
                in->consumed = (in->reuse == FALSE);
            }
        }
    }

    if (ctx->doReset == TRUE) {
        if (in) in->reuse = FALSE;
        return DoReset(com);
    }

    switch (ctx->state) {
    case DEC_STATE_OPEN_DECODER:
        ret = OpenDecoder(com);
        if (ctx->stateDoing == FALSE) ctx->state = DEC_STATE_INIT_SEQ;
        break;
    case DEC_STATE_INIT_SEQ:
        ret = DecodeHeader(com);
        if (ctx->stateDoing == FALSE) ctx->state = DEC_STATE_REGISTER_FB;
        break;
    case DEC_STATE_REGISTER_FB:
        ret = RegisterFrameBuffers(com);
        if (ctx->stateDoing == FALSE) {
            ctx->state = DEC_STATE_DECODING;
            DisplayDecodedInformation(ctx->handle, ctx->decOpenParam.bitstreamFormat, 0, NULL, ctx->testDecConfig.performance, 0);
        }
        break;
    case DEC_STATE_DECODING:
        ret = Decode(com, (PortContainerES*)in, (PortContainerDisplay*)out);
        break;
    default:
        ret = FALSE;
        break;
    }

    if (ret == FALSE || com->terminate == TRUE) {
        ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_DECODED_ALL, (void*)ctx->handle);
        if (out) {
            out->reuse = FALSE;
            out->last  = TRUE;
        }
    }

    return ret;
}

static BOOL PrepareDecoder(ComponentImpl* com, BOOL* done)
{
    *done = TRUE;

    return TRUE;
}

static void ReleaseDecoder(ComponentImpl* com)
{
    // Nothing to do
}

static BOOL DestroyDecoder(ComponentImpl* com)
{
    DecoderContext* ctx         = (DecoderContext*)com->context;
    DEC_INT_STATUS  intStatus;
    BOOL            success     = TRUE;
    Uint32          timeout     = 0;

    VPU_DecUpdateBitstreamBuffer(ctx->handle, STREAM_END_SET_FLAG);
    while (VPU_DecClose(ctx->handle) == RETCODE_VPU_STILL_RUNNING) {
        if ((intStatus=HandlingInterruptFlag(com)) == DEC_INT_STATUS_TIMEOUT) {
            HandleDecoderError(ctx->handle, ctx->numDecoded, NULL);
            VLOG(ERR, "<%s:%d> NO RESPONSE FROM VPU_DecClose()\n", __FUNCTION__, __LINE__);
            success = FALSE;
            break;
        }
        else if (intStatus == DEC_INT_STATUS_DONE) {
            DecOutputInfo outputInfo;
            VLOG(INFO, "VPU_DecClose() : CLEAR REMAIN INTERRUPT\n");
            VPU_DecGetOutputInfo(ctx->handle, &outputInfo);
            continue;
        }

        if (timeout > VPU_BUSY_CHECK_TIMEOUT) {
            VLOG(ERR, "<%s:%d> Failed to VPU_DecClose\n", __FUNCTION__, __LINE__);
        }
        timeout++;
        osal_msleep(1);
    }

    ComponentNotifyListeners(com, COMPONENT_EVENT_DEC_CLOSE, NULL);

    if (ctx->vbUserData.size) {
        vdi_lock(ctx->testDecConfig.coreIdx);
        vdi_free_dma_memory(ctx->testDecConfig.coreIdx, &ctx->vbUserData);
        vdi_unlock(ctx->testDecConfig.coreIdx);
    }

    VPU_DeInit(ctx->decOpenParam.coreIdx);

    osal_free(ctx);

    return success;
}

static Component CreateDecoder(ComponentImpl* com, CNMComponentConfig* componentParam)
{
    DecoderContext* ctx;
    Uint32          coreIdx      = componentParam->testDecConfig.coreIdx;
    Uint16*         firmware     = (Uint16*)componentParam->bitcode;
    Uint32          firmwareSize = componentParam->sizeOfBitcode;
    RetCode         retCode;
    VLOG(1, "+%s \n", __FUNCTION__);
    retCode = VPU_InitWithBitcode(coreIdx, firmware, firmwareSize);
    if (retCode != RETCODE_SUCCESS && retCode != RETCODE_CALLED_BEFORE) {
        VLOG(INFO, "%s:%d Failed to VPU_InitiWithBitcode, ret(%08x)\n", __FUNCTION__, __LINE__, retCode);
        return FALSE;
    }

    com->context = (DecoderContext*)osal_malloc(sizeof(DecoderContext));
    osal_memset(com->context, 0, sizeof(DecoderContext));
    ctx = (DecoderContext*)com->context;

    retCode = PrintVpuProductInfo(coreIdx, &ctx->attr);
    if (retCode == RETCODE_VPU_RESPONSE_TIMEOUT ) {
        CNMErrorSet(CNM_ERROR_HANGUP);
        VLOG(INFO, "<%s:%d> Failed to PrintVpuProductInfo()\n", __FUNCTION__, __LINE__);
        // HandleDecoderError(ctx->handle, ctx->numDecoded, NULL);
        return FALSE;
    }
    ctx->cyclePerTick = 32768;
    if (TRUE == ctx->attr.supportNewTimer)
        ctx->cyclePerTick = 256;

    ctx->decOpenParam.bitstreamFormat = componentParam->testDecConfig.bitFormat;
    ctx->decOpenParam.coreIdx         = componentParam->testDecConfig.coreIdx;
    ctx->decOpenParam.bitstreamMode   = componentParam->testDecConfig.bitstreamMode;
    ctx->decOpenParam.wtlEnable       = componentParam->testDecConfig.enableWTL;
    ctx->decOpenParam.wtlMode         = componentParam->testDecConfig.wtlMode;
    ctx->decOpenParam.cbcrInterleave  = componentParam->testDecConfig.cbcrInterleave;
    ctx->decOpenParam.nv21            = componentParam->testDecConfig.nv21;
    ctx->decOpenParam.streamEndian    = componentParam->testDecConfig.streamEndian;
    ctx->decOpenParam.frameEndian     = componentParam->testDecConfig.frameEndian;
    ctx->decOpenParam.bwOptimization  = componentParam->testDecConfig.wave.bwOptimization;
    ctx->wtlFormat                    = componentParam->testDecConfig.wtlFormat;
    ctx->frameNumToStop               = componentParam->testDecConfig.forceOutNum;
    ctx->scenarioTest                 = componentParam->testDecConfig.scenarioTest;
    ctx->testDecConfig                = componentParam->testDecConfig;
    ctx->state                        = DEC_STATE_OPEN_DECODER;
    ctx->stateDoing                   = FALSE;
    VLOG(INFO, "PRODUCT ID: %d\n", ctx->attr.productId);

    return (Component)com;
}

ComponentImpl decoderComponentImpl = {
    "decoder",
    NULL,
    {0,},
    {0,},
    sizeof(PortContainerDisplay),
    5,
    CreateDecoder,
    GetParameterDecoder,
    SetParameterDecoder,
    PrepareDecoder,
    ExecuteDecoder,
    ReleaseDecoder,
    DestroyDecoder
};

