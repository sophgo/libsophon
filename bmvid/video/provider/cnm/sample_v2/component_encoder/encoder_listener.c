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
#include "cnm_app.h"
#include "encoder_listener.h"
#include "misc/debug.h"
#include "misc/bw_monitor.h"



static void HandleEncHandlingIntEvent(Component com, CNMComListenerHandlingInt* param, EncListenerContext* ctx)
{
    if (ctx->bwCtx != NULL) {
        BWMonitorUpdate(ctx->bwCtx, 1);
    }
}

static void HandleEncFullEvent(Component com, CNMComListenerEncFull* param, EncListenerContext* ctx)
{
    Uint8* buf = (Uint8*)osal_malloc(param->size);

    vdi_read_memory(ctx->coreIdx, param->rdPtr, buf, param->size, ctx->streamEndian);
    if (Comparator_Act(ctx->es, buf, param->size) == FALSE) {
        VLOG(ERR, "%s:%d Bitstream Mismatch\n", __FUNCTION__, __LINE__);
        CNMAppStop();
    }
    osal_free(buf);
}

static void HandleEncGetEncCloseEvent(Component com, CNMComListenerEncClose* param, EncListenerContext* ctx)
{
    if (ctx->pfCtx != NULL) {
        PFMonitorRelease(ctx->pfCtx);
    }
    if (ctx->bwCtx != NULL) {
        BWMonitorRelease(ctx->bwCtx);
    }
}

void HandleEncCompleteSeqEvent(Component com, CNMComListenerEncCompleteSeq* param, EncListenerContext* ctx)
{
    if (ctx->performance == TRUE) {
        Uint32 fps = (ctx->fps == 0) ? 30 : ctx->fps;
        ctx->pfCtx = PFMonitorSetup(ctx->coreIdx, 0, ctx->pfClock, fps, GetBasename((const char *)ctx->cfgFileName), 1);
    }
    if (ctx->bandwidth == TRUE) {
        ctx->bwCtx = BWMonitorSetup(param->handle, TRUE, GetBasename((const char *)ctx->cfgFileName));
    }
}

void HandleEncGetOutputEvent(Component com, CNMComListenerEncDone* param, EncListenerContext* ctx)
{
    EncOutputInfo* output = param->output;
    EncHandle      handle = param->handle;
    static Uint64 frame_start = 0;
    static Uint64 frame_end = 0;
    static Uint64 total_time = 0;
    static Int32  frame_no = 0;
    if (frame_start == 0) frame_start = osal_gettime();

    if (output->reconFrameIndex == RECON_IDX_FLAG_ENC_END) return;

    if (ctx->pfCtx != NULL) {
        PFMonitorUpdate(ctx->coreIdx, ctx->pfCtx, output->frameCycle, output->encPrepareEndTick - output->encPrepareStartTick,
            output->encProcessingEndTick - output->encProcessingStartTick, output->encEncodeEndTick- output->encEncodeStartTick);
    }
    if (ctx->bwCtx != NULL) {
        BWMonitorUpdatePrint(ctx->bwCtx, output->picType);
    }

    if (output->bitstreamSize > 0) {

        frame_end = osal_gettime();
        total_time += frame_end-frame_start;
#if (defined(WAVE512_PALLADIUM) || defined(CODA960_PALLADIUM))
        VLOG(ERR, "frame %d encoded: %dus total %ldus \n", frame_no++, (frame_end-frame_start), total_time);
#else
        VLOG(ERR, "frame %d encoded: %dus total %dms\n", frame_no++, (frame_end-frame_start), total_time);
#endif
        frame_start = frame_end;

        if (ctx->es != NULL) {
            Uint8*          buf      = NULL;
            Int32           compSize = output->bitstreamSize;
            PhysicalAddress addr;

            if (param->fullInterrupted == TRUE) {
                PhysicalAddress rd, wr;
                int             room;
                VPU_EncGetBitstreamBuffer(param->handle, &rd, &wr, &room);
                compSize = room;
                addr     = rd;
            }
            else {
                addr     = output->bitstreamBuffer;
                compSize = output->bitstreamSize;
            }

            buf = (Uint8*)osal_malloc(compSize);
            vdi_read_memory(ctx->coreIdx, addr, buf, compSize, ctx->streamEndian);

            if ( ctx->es != NULL ) {
                if ((ctx->match=Comparator_Act(ctx->es, buf, compSize)) == FALSE) {
                    VLOG(ERR, "<%s:%d> INSTANCE #%d Bitstream Mismatch\n", __FUNCTION__, __LINE__, handle->instIndex);
                }
            }
            osal_free(buf);
        }

    }

    if (ctx->headerEncDone[param->handle->instIndex] == FALSE) {
        ctx->headerEncDone[param->handle->instIndex] = TRUE;
    }

    if (ctx->match == FALSE) CNMAppStop();
}

void EncoderListener(Component com, Uint32 event, void* data, void* context)
{
    int key=0;
    if (osal_kbhit()) {
        key = osal_getch();
        osal_flush_ch();
        if (key) {
            if (key == 'q' || key == 'Q') {
                CNMAppStop();
                return;
            }
        }
    }
    switch (event) {
    case COMPONENT_EVENT_ENC_OPEN:
        break;
    case COMPONENT_EVENT_ENC_ISSUE_SEQ:
        break;
    case COMPONENT_EVENT_ENC_COMPLETE_SEQ:
        HandleEncCompleteSeqEvent(com, (CNMComListenerEncCompleteSeq*)data, (EncListenerContext*)context);
        break;
    case COMPONENT_EVENT_ENC_REGISTER_FB:
        break;
    case COMPONENT_EVENT_ENC_READY_ONE_FRAME:
        break;
    case COMPONENT_EVENT_ENC_START_ONE_FRAME:
        break;
    case COMPONENT_EVENT_ENC_HANDLING_INT:
        HandleEncHandlingIntEvent(com, (CNMComListenerHandlingInt*)data, (EncListenerContext*)context);
        break;
    case COMPONENT_EVENT_ENC_GET_OUTPUT_INFO:
        HandleEncGetOutputEvent(com, (CNMComListenerEncDone*)data, (EncListenerContext*)context);
        break;
    case COMPONENT_EVENT_ENC_ENCODED_ALL:
        break;
    case COMPONENT_EVENT_ENC_CLOSE:
        HandleEncGetEncCloseEvent(com, (CNMComListenerEncClose*)data, (EncListenerContext*)context);
        break;
    case COMPONENT_EVENT_ENC_FULL_INTERRUPT:
        HandleEncFullEvent(com, (CNMComListenerEncFull*)data, (EncListenerContext*)context);
        break;
    default:
        break;
    }
}

BOOL SetupEncListenerContext(EncListenerContext* ctx, CNMComponentConfig* config)
{
    TestEncConfig* encConfig = &config->testEncConfig;

    osal_memset((void*)ctx, 0x00, sizeof(EncListenerContext));

    if (encConfig->compareType & (1 << MODE_COMP_ENCODED)) {
        if ((ctx->es=Comparator_Create(STREAM_COMPARE, encConfig->ref_stream_path, encConfig->cfgFileName)) == NULL) {
            VLOG(ERR, "%s:%d Failed to Comparator_Create(%s)\n", __FUNCTION__, __LINE__, encConfig->ref_stream_path);
            return FALSE;
        }
    }
    ctx->coreIdx       = encConfig->coreIdx;
    ctx->streamEndian  = encConfig->stream_endian;
    ctx->match         = TRUE;
    ctx->matchOtherInfo= TRUE;
    ctx->performance   = encConfig->performance;
    ctx->bandwidth     = encConfig->bandwidth;
    ctx->pfClock       = encConfig->pfClock;
    osal_memcpy(ctx->cfgFileName, encConfig->cfgFileName, sizeof(ctx->cfgFileName));

    return TRUE;
}

void ClearEncListenerContext(EncListenerContext* ctx)
{
    if (ctx->es)    Comparator_Destroy(ctx->es);
}

