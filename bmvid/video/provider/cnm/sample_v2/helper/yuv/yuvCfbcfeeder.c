//--=========================================================================--
//  This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2013  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================--

#include <string.h>
#include "main_helper.h"

BOOL cfbcYuvFeeder_Create(
    YuvFeederImpl *impl,
    const char*   path,
    Uint32        packed,
    Uint32        fbStride,
    Uint32        fbHeight)
{
    yuvContext*   ctx;
    osal_file_t*  fp;
    Uint8*        pYuv;

    if ((fp=osal_fopen(path, "rb")) == NULL) {
        VLOG(ERR, "%s:%d failed to open yuv file: %s\n", __FUNCTION__, __LINE__, path);
        return FALSE;
    }

    if ( packed == 1 )
        pYuv = osal_malloc(fbStride*fbHeight*3*2*2);//packed, unsigned short
    else
        pYuv = osal_malloc(fbStride*fbHeight*3*2);//unsigned short

    if ((ctx=(yuvContext*)osal_malloc(sizeof(yuvContext))) == NULL) {
        osal_free(pYuv);
        osal_fclose(fp);
        return FALSE;
    }

    osal_memset(ctx, 0, sizeof(yuvContext));

    ctx->fp       = fp;
    ctx->pYuv     = pYuv;
    impl->context = ctx;

    return TRUE;
}

BOOL cfbcYuvFeeder_Destory(
    YuvFeederImpl *impl
    )
{
    yuvContext*    ctx = (yuvContext*)impl->context;

    osal_fclose(ctx->fp);
    osal_free(ctx->pYuv);
    osal_free(ctx);
    return TRUE;
}

BOOL cfbcYuvFeeder_Feed(
    YuvFeederImpl*  impl,
    Int32           coreIdx,
    FrameBuffer*    fb,
    size_t          picWidth,
    size_t          picHeight,
    Uint32          srcFbIndex,
    void*           arg
    )
{
    FrameBuffer *tableFb = (FrameBuffer*)arg;
    yuvContext* ctx     = (yuvContext*)impl->context;
    Uint32 lossless     = ctx->lossless;
    Uint32 tx16y        = ctx->tx16y;
    Uint32 tx16c        = ctx->tx16c;
    Uint8*    pInCfbc          = NULL;
    Uint32    lumaCompSize     = 0, chromaCompSize   = 0;
    Uint32    lumaOffsetSize   = 0, chromaOffsetSize = 0;
    Uint32    frameSize        = 0;

    if ( lossless == 1 ) {
        if (fb->format == FORMAT_420 ) {
            lumaCompSize = WAVE5_ENC_FBC50_LOSSLESS_LUMA_8BIT_FRAME_SIZE(picWidth , picHeight);
            chromaCompSize = WAVE5_ENC_FBC50_LOSSLESS_CHROMA_8BIT_FRAME_SIZE(picWidth, picHeight);
            if ( fb->mapType  == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_8BIT)
                chromaCompSize = WAVE5_ENC_FBC50_LOSSLESS_422_CHROMA_8BIT_FRAME_SIZE(picWidth, picHeight);
        }
        else {
            lumaCompSize = WAVE5_ENC_FBC50_LOSSLESS_LUMA_10BIT_FRAME_SIZE(picWidth, picHeight);
            chromaCompSize = WAVE5_ENC_FBC50_LOSSLESS_CHROMA_10BIT_FRAME_SIZE(picWidth, picHeight);
            if ( fb->mapType  == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_10BIT)
                chromaCompSize = WAVE5_ENC_FBC50_LOSSLESS_422_CHROMA_10BIT_FRAME_SIZE(picWidth, picHeight);
        }
    }
    else {
        lumaCompSize = WAVE5_ENC_FBC50_LOSSY_LUMA_FRAME_SIZE(picWidth, picHeight, tx16y);
        chromaCompSize = WAVE5_ENC_FBC50_LOSSY_CHROMA_FRAME_SIZE(picWidth, picHeight, tx16c);
            if ( fb->mapType  == COMPRESSED_FRAME_MAP_V50_LOSSY_422)
                chromaCompSize = WAVE5_ENC_FBC50_LOSSY_422_CHROMA_FRAME_SIZE(picWidth, picHeight, tx16c);
    }
    lumaOffsetSize = WAVE5_ENC_FBC50_LUMA_TABLE_SIZE(picWidth, picHeight);
    chromaOffsetSize = WAVE5_ENC_FBC50_CHROMA_TABLE_SIZE(picWidth, picHeight);
    if ( fb->mapType  == COMPRESSED_FRAME_MAP_V50_LOSSY_422 
        || fb->mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_8BIT 
        || fb->mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_10BIT)
        chromaOffsetSize = WAVE5_ENC_FBC50_422_CHROMA_TABLE_SIZE(picWidth, picHeight);

    frameSize        = lumaCompSize + chromaCompSize*2 + lumaOffsetSize +  chromaOffsetSize*2;

    pInCfbc = osal_malloc(frameSize);

    if (!osal_fread(pInCfbc, 1, frameSize, ctx->fp)) {
        if (osal_feof(ctx->fp) == 0) 
            VLOG(ERR, "FBC Data osal_fread failed file handle is 0x%x\n", ctx->fp);
        return FALSE;
    }

    VLOG(INFO, "cframe50_frame_size = lc(%d) + cc(%d) + lo(%d) + co(%d) = %d\n", lumaCompSize, chromaCompSize, lumaOffsetSize, chromaOffsetSize, frameSize);
    //VLOG(INFO, "fbSrc    = %08x, %08x, %08x\n", fb->bufY, fb->bufCb, fb->bufCr);
    //VLOG(INFO, "tableFb  = %08x, %08x, %08x\n", tableFb->bufY, tableFb->bufCb, tableFb->bufCr);

    vdi_write_memory(coreIdx, fb->bufY,      (Uint8 *)(pInCfbc), lumaCompSize, fb->endian);
    vdi_write_memory(coreIdx, fb->bufCb,     (Uint8 *)(pInCfbc + lumaCompSize), chromaCompSize, fb->endian);
    vdi_write_memory(coreIdx, fb->bufCr,     (Uint8 *)(pInCfbc + lumaCompSize + chromaCompSize), chromaCompSize, fb->endian);
    if ( lossless == 1 ) {

        vdi_write_memory(coreIdx, tableFb->bufY, (Uint8 *)(pInCfbc + lumaCompSize + chromaCompSize*2), lumaOffsetSize, fb->endian);
        vdi_write_memory(coreIdx, tableFb->bufCb, (Uint8 *)(pInCfbc + lumaCompSize + chromaCompSize*2 + lumaOffsetSize), chromaOffsetSize, fb->endian);
        vdi_write_memory(coreIdx, tableFb->bufCr, (Uint8 *)(pInCfbc + lumaCompSize + chromaCompSize*2 + lumaOffsetSize + chromaOffsetSize), chromaOffsetSize, fb->endian);
    }

    osal_free(pInCfbc);

    return TRUE;
}

BOOL cfbcYuvFeeder_Configure(
    YuvFeederImpl* impl,
    Uint32         cmd,
    YuvInfo        yuv
    )
{
    yuvContext* ctx = (yuvContext*)impl->context;
    UNREFERENCED_PARAMETER(cmd);

    ctx->fbStride       = yuv.srcStride;
    ctx->cbcrInterleave = yuv.cbcrInterleave;
    ctx->srcPlanar      = yuv.srcPlanar;
    ctx->lossless       = yuv.lossless;
    ctx->tx16y          = yuv.tx16y;
    ctx->tx16c          = yuv.tx16c;

    return TRUE;
}

YuvFeederImpl cfbcYuvFeederImpl = {
    NULL,
    cfbcYuvFeeder_Create,
    cfbcYuvFeeder_Feed,
    cfbcYuvFeeder_Destory,
    cfbcYuvFeeder_Configure
};

