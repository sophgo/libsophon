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

typedef enum {
    READER_STATE_OPEN,
    READER_STATE_READING,
} EncReaderState;

typedef struct {
    EncHandle       handle;
    Uint32          streamBufCount;
    Uint32          streamBufSize;
    vpu_buffer_t*   bsBuffer;
    Uint32          numBuffers;
    Uint32          coreIdx;
    EndianMode      streamEndian;
    EncReaderState  state;
    char            bitstreamFileName[MAX_FILE_PATH];
    osal_file_t     fp;
} ReaderContext;

static CNMComponentParamRet GetParameterReader(ComponentImpl* from, ComponentImpl* com, GetParameterCMD commandType, void* data)
{
    ReaderContext*           ctx    = (ReaderContext*)com->context;
    BOOL                     result = TRUE;
    ParamEncBitstreamBuffer* bsBuf  = NULL;
    PortContainer*          container;

    switch(commandType) {
    case GET_PARAM_COM_IS_CONTAINER_CONUSUMED:
        container = (PortContainer*)data;
        container->consumed = TRUE;
        break;
    case GET_PARAM_READER_BITSTREAM_BUF:
        if (ctx->bsBuffer == NULL || ctx->numBuffers <= 0)
            return CNM_COMPONENT_PARAM_NOT_READY;
        bsBuf      = (ParamEncBitstreamBuffer*)data;
        bsBuf->bs  = ctx->bsBuffer;
        bsBuf->num = ctx->numBuffers;
        break;
    default:
        return CNM_COMPONENT_PARAM_NOT_FOUND;
    }

    return (result == TRUE) ? CNM_COMPONENT_PARAM_SUCCESS : CNM_COMPONENT_PARAM_FAILURE;
}

static CNMComponentParamRet SetParameterReader(ComponentImpl* from, ComponentImpl* com, SetParameterCMD commandType, void* data)
{
    BOOL result = TRUE;

    switch(commandType) {
    default:
        result = FALSE;
        break;
    }

    return (result == TRUE) ? CNM_COMPONENT_PARAM_SUCCESS : CNM_COMPONENT_PARAM_FAILURE;
}

static BOOL ExecuteReader(ComponentImpl* com, PortContainer* in, PortContainer* out)
{
    ReaderContext*          ctx     = (ReaderContext*)com->context;
    PortContainerES*        srcData = (PortContainerES*)in;
    BOOL                    success = TRUE;
    CNMComponentParamRet    ret;

    switch (ctx->state) {
    case READER_STATE_OPEN:
        ret = ComponentGetParameter(com, com->srcPort.connectedComponent, GET_PARAM_ENC_HANDLE, &ctx->handle);
        if (ComponentParamReturnTest(ret, &success) == FALSE) {
            return success;
        }

        ctx->state = READER_STATE_READING;
        srcData->reuse  = TRUE;
        break;
    case READER_STATE_READING:
        if (srcData->size > 0) {
            if (srcData->streamBufFull == TRUE) {
                EnterLock(ctx->coreIdx);
                VPU_EncUpdateBitstreamBuffer(ctx->handle, srcData->size);
                LeaveLock(ctx->coreIdx);
                srcData->buf.phys_addr = 0;
                srcData->streamBufFull = FALSE;
            }
            if (ctx->fp) {
                Uint8* buf = (Uint8*)osal_malloc(srcData->size);
                vdi_read_memory(ctx->coreIdx, srcData->buf.phys_addr, buf, srcData->size, ctx->streamEndian);
                osal_fwrite(buf, srcData->size, 1, ctx->fp);
                osal_fflush(ctx->fp);
                osal_free(buf);
            }
        }

        srcData->consumed = TRUE;
        com->terminate    = srcData->last;
        break;
    default:
        success = FALSE;
        break;
    }

    return success;
}

static BOOL PrepareReader(ComponentImpl* com, BOOL* done)
{
    ReaderContext*       ctx = (ReaderContext*)com->context;
    Uint32               i;
    vpu_buffer_t*        bsBuffer;
    Uint32               num = ComponentPortGetSize(&com->srcPort);

    *done = FALSE;
    bsBuffer = (vpu_buffer_t*)osal_malloc(num*sizeof(vpu_buffer_t));
    osal_memset(bsBuffer, 0, num*sizeof(vpu_buffer_t));
    vdi_lock(ctx->coreIdx);
    for (i = 0; i < num; i++) {
        bsBuffer[i].size = ctx->streamBufSize;
        if (vdi_allocate_dma_memory(ctx->coreIdx, &bsBuffer[i]) < 0) {
            vdi_unlock(ctx->coreIdx);
            VLOG(ERR, "%s:%d fail to allocate bitstream buffer\n", __FUNCTION__, __LINE__);
            osal_free(bsBuffer);
            return FALSE;
        }
        VLOG(ERR, "Allocate BS buffer %d: addr %lx size %d\n", i, bsBuffer[i].phys_addr, bsBuffer[i].size);
    }
    vdi_unlock(ctx->coreIdx);
    ctx->numBuffers = num;
    ctx->bsBuffer   = bsBuffer;
    ctx->state      = READER_STATE_OPEN;
    if ( ctx->bitstreamFileName[0] ) {
        if ((ctx->fp=osal_fopen(ctx->bitstreamFileName, "wb")) == NULL) {
            return FALSE;
        }
        VLOG(INFO, "output bin file: %s\n", ctx->bitstreamFileName);
    }

    *done = TRUE;

    return TRUE;
}

static void ReleaseReader(ComponentImpl* com)
{
    ReaderContext*  ctx = (ReaderContext*)com->context;
    Uint32          i   = 0;

    vdi_lock(ctx->coreIdx);
    if (ctx->bsBuffer != NULL) {
        for (i = 0; i < MAX_QUEUE_NUM; i++) {
            if (ctx->bsBuffer[i].size)
                vdi_free_dma_memory(ctx->coreIdx, &ctx->bsBuffer[i]);
        }
    }
    vdi_unlock(ctx->coreIdx);
}

static BOOL DestroyReader(ComponentImpl* com)
{
    ReaderContext*  ctx = (ReaderContext*)com->context;

    if (ctx->bsBuffer != NULL) osal_free(ctx->bsBuffer);
    if (ctx->fp) osal_fclose(ctx->fp);
    osal_free(ctx);

    return TRUE;
}

static Component CreateReader(ComponentImpl* com, CNMComponentConfig* componentParam)
{
    ReaderContext* ctx;

    com->context = osal_malloc(sizeof(ReaderContext));
    ctx = (ReaderContext*)com->context;
    osal_memset((void*)ctx, 0, sizeof(ReaderContext));

    osal_strcpy(ctx->bitstreamFileName, componentParam->testEncConfig.bitstreamFileName);
    ctx->handle       = NULL;
    ctx->coreIdx      = componentParam->testEncConfig.coreIdx;
    ctx->streamEndian = (EndianMode)componentParam->testEncConfig.stream_endian;

    ctx->streamBufCount = componentParam->encOpenParam.streamBufCount;
    ctx->streamBufSize  = componentParam->encOpenParam.streamBufSize;
    return (Component)com;
}

ComponentImpl readerComponentImpl = {
    "reader",
    NULL,
    {0,},
    {0,},
    sizeof(PortContainer),
    5,
    CreateReader,
    GetParameterReader,
    SetParameterReader,
    PrepareReader,
    ExecuteReader,
    ReleaseReader,
    DestroyReader,
};

