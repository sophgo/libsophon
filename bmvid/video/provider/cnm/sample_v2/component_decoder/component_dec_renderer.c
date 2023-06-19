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

typedef struct {
    DecHandle                   handle;                 /*!<< A decoder handle */
    osal_thread_t               threadHandle;
    TestDecConfig               testDecConfig;
    Uint32                      framebufStride;
    Uint32                      displayPeriodTime;
    FrameBuffer                 pFrame[MAX_REG_FRAME];
    bm_device_mem_t             pFbMem[MAX_REG_FRAME];
    Uint64                      FbmemVaddr[MAX_REG_FRAME];
    BOOL                        fbAllocated;
    ParamDecNeedFrameBufferNum  fbCount;
    Queue*                      seqMemQ;
#if !defined(_WIN32)
    osal_mutex_t                lock;
#else
    osal_mutex_t* lock;
#endif
    char                        outputPath[256];
    FILE*                       fpOutput[OUTPUT_FP_NUMBER];
} RendererContext;

typedef struct SequenceMemInfo {
    Uint32              remainingCount;
    Uint32              sequenceNo;
    vpu_buffer_t        pFbMem[MAX_REG_FRAME];
    vpu_buffer_t        vbFbcYTbl[MAX_REG_FRAME];
    vpu_buffer_t        vbFbcCTbl[MAX_REG_FRAME];
} SequenceMemInfo;

static void FreeFrameBuffer(Uint32 coreIdx, Uint32 idx, SequenceMemInfo* info)
{
    vdi_lock(coreIdx);
    if (info->pFbMem[idx].size > 0) {
        vdi_free_dma_memory(coreIdx, &info->pFbMem[idx]);
        osal_memset((void*)&info->pFbMem[idx], 0x00, sizeof(vpu_buffer_t));
    }

    if (info->vbFbcYTbl[idx].size > 0) {
        vdi_free_dma_memory(coreIdx, &info->vbFbcYTbl[idx]);
        osal_memset((void*)&info->vbFbcYTbl[idx], 0x00, sizeof(vpu_buffer_t));
    }

    if (info->vbFbcCTbl[idx].size > 0) {
        vdi_free_dma_memory(coreIdx, &info->vbFbcCTbl[idx]);
        osal_memset((void*)&info->vbFbcCTbl[idx], 0x00, sizeof(vpu_buffer_t));
    }
    vdi_unlock(coreIdx);
}

static BOOL SetParamFreeFrameBuffers(ComponentImpl* com, Uint32 fbFlags)
{
    RendererContext*    ctx          = (RendererContext*)com->context;
    BOOL                remainingFbs[MAX_REG_FRAME] = {0};
    Uint32              idx;
    Uint32              fbIndex;
    BOOL                wtlEnable    = ctx->testDecConfig.enableWTL;
    DecGetFramebufInfo  curFbInfo;
    Uint32              coreIdx      = ctx->testDecConfig.coreIdx;
    SequenceMemInfo     seqMem       = {0};

    osal_mutex_lock(ctx->lock);
    VPU_DecGiveCommand(ctx->handle, DEC_GET_FRAMEBUF_INFO, (void*)&curFbInfo);

    for (idx=0; idx<MAX_GDI_IDX; idx++) {
        fbIndex = idx;
        if ((fbFlags>>idx) & 0x01) {
            if (wtlEnable == TRUE) {
                fbIndex = VPU_CONVERT_WTL_INDEX(ctx->handle, idx);
            }
            seqMem.remainingCount++;
            remainingFbs[fbIndex] = TRUE;
        }
    }

    osal_memcpy((void*)seqMem.pFbMem,    ctx->pFbMem,         sizeof(ctx->pFbMem));
    osal_memcpy((void*)seqMem.vbFbcYTbl, curFbInfo.vbFbcYTbl, sizeof(curFbInfo.vbFbcYTbl));
    osal_memcpy((void*)seqMem.vbFbcCTbl, curFbInfo.vbFbcCTbl, sizeof(curFbInfo.vbFbcCTbl));

    for (idx=0; idx<MAX_REG_FRAME; idx++) {
        if (remainingFbs[idx] == FALSE) {
            FreeFrameBuffer(coreIdx, idx, &seqMem);
        }
        // Free a mvcol buffer
        if(curFbInfo.vbMvCol[idx].size > 0) {
            vdi_lock(coreIdx);
            vdi_free_dma_memory(coreIdx, &curFbInfo.vbMvCol[idx]);
            vdi_unlock(coreIdx);
        }
    }

    if (seqMem.remainingCount > 0) {
        Queue_Enqueue(ctx->seqMemQ, (void*)&seqMem);
    }

    ctx->fbAllocated = FALSE;
    osal_mutex_unlock(ctx->lock);

    return TRUE;
}

static BOOL ReallocateFrameBuffers(ComponentImpl* com, ParamDecReallocFB* param)
{
    RendererContext* ctx        = (RendererContext*)com->context;
    Int32            fbcIndex    = param->compressedIdx;
    Int32            linearIndex = param->linearIdx;
    bm_device_mem_t* pFbMem      = ctx->pFbMem;
    FrameBuffer*     pFrame      = ctx->pFrame;
    FrameBuffer*     newFbs      = param->newFbs;
    bm_handle_t  bm_handle;
    vdi_lock(ctx->testDecConfig.coreIdx);
    if (fbcIndex >= 0) {
        /* Release the FBC framebuffer */

    bm_handle= bmvpu_dec_get_bmlib_handle(ctx->testDecConfig.coreIdx);
#ifndef BM_PCIE_MODE
        bm_mem_unmap_device_mem(bm_handle,(void *)ctx->FbmemVaddr[fbcIndex],ctx->pFbMem[fbcIndex].size);
#endif
        bm_free_device(bm_handle,pFbMem[fbcIndex]);

    }

    if (linearIndex >= 0) {
#ifndef BM_PCIE_MODE
        bm_mem_unmap_device_mem(bm_handle,(void *)ctx->FbmemVaddr[linearIndex],ctx->pFbMem[linearIndex].size);
#endif
        bm_free_device(bm_handle,pFbMem[linearIndex]);

    }
    vdi_unlock(ctx->testDecConfig.coreIdx);

    if (fbcIndex >= 0) {
        newFbs[0].myIndex = fbcIndex;
        newFbs[0].width   = param->width;
        newFbs[0].height  = param->height;
        pFrame[fbcIndex]  = newFbs[0];
    }

    if (linearIndex >= 0) {
        newFbs[1].myIndex = linearIndex;
        newFbs[1].width   = param->width;
        newFbs[1].height  = param->height;
        pFrame[linearIndex]  = newFbs[1];
    }

    return TRUE;
}

static void DisplayFrame(RendererContext* ctx, DecOutputInfo* result)
{
    osal_mutex_lock(ctx->lock);

    if (result->indexFrameDisplay >= 0) {
        DecInitialInfo  initialInfo;
        VPU_DecGiveCommand(ctx->handle, DEC_GET_SEQ_INFO, &initialInfo);

        // No renderer device
        if (initialInfo.sequenceNo != result->sequenceNo) {
            // Free a framebuffer of previous sequence
            SequenceMemInfo* memInfo = Queue_Peek(ctx->seqMemQ);
            if (memInfo != NULL) {
                FreeFrameBuffer(ctx->testDecConfig.coreIdx, result->indexFrameDisplay, memInfo);
                if (memInfo->remainingCount == 0) {
                    VLOG(ERR, "%s:%d remainingCout must be greater than zero\n", __FUNCTION__, __LINE__);
                }
                memInfo->remainingCount--;
                if (memInfo->remainingCount == 0) {
                    Queue_Dequeue(ctx->seqMemQ);
                }
            }
        }
        else {
            VPU_DecClrDispFlag(ctx->handle, result->indexFrameDisplay);
        }
    }
    osal_mutex_unlock(ctx->lock);
}

static BOOL FlushFrameBuffers(ComponentImpl* com)
{
    PortContainerDisplay*   srcData;
    RendererContext*    ctx = (RendererContext*)com->context;

    osal_mutex_lock(ctx->lock);
    while ((srcData=(PortContainerDisplay*)ComponentPortGetData(&com->srcPort)) != NULL) {
        if (srcData->decInfo.indexFrameDisplay >= 0) {
            VPU_DecClrDispFlag(ctx->handle, srcData->decInfo.indexFrameDisplay);
        }
        ComponentPortSetData(&com->srcPort, (PortContainer*)srcData);
    }
    osal_mutex_unlock(ctx->lock);

    return TRUE;
}
static BOOL AllocateFrameBuffer(ComponentImpl* com)
{
    RendererContext*     ctx            = (RendererContext*)com->context;
    BOOL                 success;
    Uint32               compressedNum;
    Uint32               linearNum;
    CNMComponentParamRet ret;

    ret = ComponentGetParameter(com, com->srcPort.connectedComponent, GET_PARAM_DEC_FRAME_BUF_NUM, &ctx->fbCount);
    if (ComponentParamReturnTest(ret, &success) == FALSE) {
        return success;
    }

    osal_memset((void*)ctx->pFbMem, 0x00, sizeof(ctx->pFbMem));
    osal_memset((void*)ctx->pFrame, 0x00, sizeof(ctx->pFrame));

    compressedNum  = ctx->fbCount.compressedNum;
    linearNum      = ctx->fbCount.linearNum;

    if (compressedNum == 0 && linearNum == 0) {
        VLOG(ERR, "%s:%d The number of framebuffers are zero. compressed %d, linear: %d\n",
            __FUNCTION__, __LINE__, compressedNum, linearNum);
        return FALSE;
    }

    if (AllocateDecFrameBuffer(ctx->handle, &ctx->testDecConfig, compressedNum, linearNum, ctx->pFrame, ctx->pFbMem, &ctx->framebufStride, 0) == FALSE) {
        VLOG(INFO, "%s:%d Failed to AllocateDecFrameBuffer()\n", __FUNCTION__, __LINE__);
        return FALSE;
    }
    ctx->fbAllocated = TRUE;

    return TRUE;
}

static CNMComponentParamRet GetParameterRenderer(ComponentImpl* from, ComponentImpl* com, GetParameterCMD commandType, void* data)
{
    RendererContext*        ctx     = (RendererContext*)com->context;
    ParamDecFrameBuffer*    allocFb = NULL;
    PortContainer*         container;

    if (ctx->fbAllocated == FALSE) return CNM_COMPONENT_PARAM_NOT_READY;

    switch(commandType) {
    case GET_PARAM_COM_IS_CONTAINER_CONUSUMED:
        // This query command is sent from the comonponent core.
        // If input data are consumed in sequence, it should return TRUE through PortContainer::consumed.
        container = (PortContainer*)data;
        container->consumed = TRUE;
        break;
    case GET_PARAM_RENDERER_FRAME_BUF:
        allocFb = (ParamDecFrameBuffer*)data;
        allocFb->stride        = ctx->framebufStride;
        allocFb->linearNum     = ctx->fbCount.linearNum;
        allocFb->compressedNum = ctx->fbCount.compressedNum;
        allocFb->fb            = ctx->pFrame;
        break;
    case GET_PARAM_DEC_FRAME_BUF_STRIDE:
        *(Uint32*)data = ctx->framebufStride;
        break;
    default:
        return CNM_COMPONENT_PARAM_NOT_FOUND;
    }

    return CNM_COMPONENT_PARAM_SUCCESS;
}

static CNMComponentParamRet SetParameterRenderer(ComponentImpl* from, ComponentImpl* com, SetParameterCMD commandType, void* data)
{
    RendererContext*    ctx    = (RendererContext*)com->context;
    BOOL                result = TRUE;

    UNREFERENCED_PARAMETER(ctx);
    switch(commandType) {
    case SET_PARAM_RENDERER_REALLOC_FRAMEBUFFER:
        result = ReallocateFrameBuffers(com, (ParamDecReallocFB*)data);
        break;
    case SET_PARAM_RENDERER_FREE_FRAMEBUFFERS:
        result = SetParamFreeFrameBuffers(com, *(Uint32*)data);
        break;
    case SET_PARAM_RENDERER_FLUSH:
        result = FlushFrameBuffers(com);
        break;
    case SET_PARAM_RENDERER_ALLOC_FRAMEBUFFERS:
        result = AllocateFrameBuffer(com);
        break;
    default:
        return CNM_COMPONENT_PARAM_NOT_FOUND;
    }

    if (result == TRUE) return CNM_COMPONENT_PARAM_SUCCESS;
    else                return CNM_COMPONENT_PARAM_FAILURE;
}



static BOOL ExecuteRenderer(ComponentImpl* com, PortContainer* in, PortContainer* out)
{
    RendererContext*        ctx               = (RendererContext*)com->context;
    PortContainerDisplay*   srcData           = (PortContainerDisplay*)in;
    Int32                   indexFrameDisplay;
    TestDecConfig*          decConfig         = &ctx->testDecConfig;

    in->reuse = TRUE;


    indexFrameDisplay = srcData->decInfo.indexFrameDisplay;

    if (indexFrameDisplay == DISPLAY_IDX_FLAG_SEQ_END) {
        com->terminate = TRUE;
    }
    else if (indexFrameDisplay >= 0) {
        if (strlen((const char*)decConfig->outputPath) > 0) {
            VpuRect rcDisplay = {0,};

            rcDisplay.right  = srcData->decInfo.dispPicWidth;
            rcDisplay.bottom = srcData->decInfo.dispPicHeight;
            if (decConfig->scaleDownWidth > 0 || decConfig->scaleDownHeight > 0) {
                rcDisplay.right  = VPU_CEIL(srcData->decInfo.dispPicWidth, 16);
                rcDisplay.bottom = VPU_CEIL(srcData->decInfo.dispPicHeight, 4);
            }

            if (ctx->fpOutput[0] == NULL) {
                if (OpenDisplayBufferFile(decConfig->bitFormat, decConfig->outputPath, rcDisplay, (TiledMapType)srcData->decInfo.dispFrame.mapType, ctx->fpOutput) == FALSE) {
                    return FALSE;
                }
            }
            SaveDisplayBufferToFile(ctx->handle, decConfig->bitFormat, srcData->decInfo.dispFrame, rcDisplay, ctx->fpOutput);
        }

        DisplayFrame(ctx, &srcData->decInfo);
        osal_msleep(ctx->displayPeriodTime);
    }

    srcData->consumed = TRUE;
    srcData->reuse    = FALSE;

    if (srcData->last == TRUE) {
        com->terminate = TRUE;
    }

    return TRUE;
}

static BOOL PrepareRenderer(ComponentImpl* com, BOOL* done)
{
    RendererContext* ctx = (RendererContext*)com->context;
    BOOL             ret;

    if (ctx->handle == NULL) {
        CNMComponentParamRet paramRet;
        BOOL                 success;
        paramRet = ComponentGetParameter(com, com->srcPort.connectedComponent, GET_PARAM_DEC_HANDLE, &ctx->handle);
        if (ComponentParamReturnTest(paramRet, &success) == FALSE) {
            return success;
        }
    }

    if ((ret=AllocateFrameBuffer(com)) == FALSE || ctx->fbAllocated == FALSE) {
        return ret;
    }
    // Nothing to do
    *done = TRUE;
    return TRUE;
}

static void ReleaseRenderer(ComponentImpl* com)
{
    RendererContext* ctx = (RendererContext*)com->context;
    Uint32           coreIdx = ctx->testDecConfig.coreIdx;
    Uint32           i;
    bm_handle_t      bm_handle;
    vdi_lock(coreIdx);
    bm_handle=bmvpu_dec_get_bmlib_handle(coreIdx);
    for (i=0; i<MAX_REG_FRAME; i++) {
        if (ctx->pFbMem[i].size)
        {
#ifndef BM_PCIE_MODE
            bm_mem_unmap_device_mem(bm_handle,(void *)ctx->FbmemVaddr[i],ctx->pFbMem[i].size);
#endif
            bm_free_device(bm_handle,ctx->pFbMem[i]);


        }
    }
    vdi_unlock(coreIdx);
}

static BOOL DestroyRenderer(ComponentImpl* com)
{
    RendererContext* ctx = (RendererContext*)com->context;

    CloseDisplayBufferFile(ctx->fpOutput);
    Queue_Destroy(ctx->seqMemQ);
    osal_mutex_destroy(ctx->lock);
    osal_free(ctx);

    return TRUE;
}

static Component CreateRenderer(ComponentImpl* com, CNMComponentConfig* componentParam)
{
    RendererContext*    ctx;

    com->context = (RendererContext*)osal_malloc(sizeof(RendererContext));
    osal_memset(com->context, 0, sizeof(RendererContext));
    ctx = com->context;

    osal_memcpy((void*)&ctx->testDecConfig, (void*)&componentParam->testDecConfig, sizeof(TestDecConfig));
    if (componentParam->testDecConfig.fps > 0)
        ctx->displayPeriodTime = (1000 / componentParam->testDecConfig.fps);
    else
        ctx->displayPeriodTime = 1000/30;
    ctx->fbAllocated       = FALSE;
    ctx->seqMemQ           = Queue_Create(10, sizeof(SequenceMemInfo));
    ctx->lock              = osal_mutex_create();

    return (Component)com;
}

ComponentImpl rendererComponentImpl = {
    "renderer",
    NULL,
    {0,},
    {0,},
    sizeof(PortContainer),     /* No output */
    5,
    CreateRenderer,
    GetParameterRenderer,
    SetParameterRenderer,
    PrepareRenderer,
    ExecuteRenderer,
    ReleaseRenderer,
    DestroyRenderer
};

