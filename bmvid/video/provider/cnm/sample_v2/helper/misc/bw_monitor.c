//--=========================================================================--
//  This file is a part of QCTOOL project
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
#include "vpuapifunc.h"
#include "main_helper.h"
#include "bw_monitor.h"
#include "debug.h"

#define MBYTE                               (1024*1024)

static char picTypeString[10][4] = {"I", "P", "B", "", "", "IDR", "", "", "", ""};//picType enum in vpuapi.h

typedef void*   BWData;

typedef struct _tag_bw_data_struct {
    Uint32     vcpu_bw_rd;
    Uint32     vcpu_bw_wr;
    Uint32     sdma_bw_rd;
    Uint32     sdma_bw_wr;
    Uint32     vcpu_dma_bw_wr;
    Uint32     vcpu_dma_bw_rd;
    Uint32     vcpu_nbdma_bw_wr;
    Uint32     vcpu_nbdma_bw_rd;
    Uint32     prescan_wr;
    Uint32     prescan_rd;

    Uint32     arid_pri_total;
    Uint32     awid_pri_total;
    Uint32     arid_sec_total;
    Uint32     awid_sec_total;
    Uint32     arid_mclk_total;
    Uint32     awid_mclk_total;
    Uint32     total_write;
    Uint32     total_read;
    Uint32     total_bandwidth;
    Uint32     subblock_bw_pri_rd[16];
    Uint32     subblock_bw_pri_wr[16];
    Uint32     subblock_bw_sec_rd[16];
    Uint32     subblock_bw_sec_wr[16];
    Uint32     subblock_bw_mclk_rd[16];
    Uint32     subblock_bw_mclk_wr[16];
} BWGdiData;

typedef struct _tag_BWMonitorops {
    BWCtx   (*allocate)(Uint32);
    void    (*release)(BWCtx);
    void    (*reset)(BWCtx);
    BWData  (*get_data)(BWCtx, Uint32 numCores);
    void    (*print)(BWCtx, BWData, Uint32);
} BWMonitorOps_t;

#define BW_CONTEXT_COMMON_VARS            \
    BWMonitorOps_t* ops;                  \
    Uint32          coreIndex;            \
    Uint32          instanceIndex;        \
    Uint32          productId;            \
    CodecInst*      instance;             \
    Uint32          numFrames;            \
    Uint64          totalPriRead;         \
    Uint64          totalPriWrite;        \
    Uint64          totalSecRead;         \
    Uint64          totalSecWrite;        \
    Uint64          totalProcRead;        \
    Uint64          totalProcWrite;       \
    Uint64          total;                \
    BOOL            enableReportPerFrame; \
    osal_file_t     fpBWTotal;            \
    BWData*         data;                 \
    char strLogDir[256];

typedef struct _tag_bw_common_context_struct {
    BW_CONTEXT_COMMON_VARS
} BWCommonCtx;


/************************************************************************/
/* DUMMY                                                                */
/************************************************************************/
typedef struct _tag_dummy_bw_context_struct {
    BW_CONTEXT_COMMON_VARS
} dummy_bw_ctx_t;

static BWCtx
dummy_bw_monitor_allocate(
    Uint32     coreIndex
    )
{
    dummy_bw_ctx_t* context = (dummy_bw_ctx_t*)osal_malloc(sizeof(dummy_bw_ctx_t));

    UNREFERENCED_PARAMETER(coreIndex);

    return (BWCtx)context;
}

static void
dummy_bw_monitor_release(
    BWCtx    ctx
    )
{
    UNREFERENCED_PARAMETER(ctx);

    return;
}

static void
dummy_bw_monitor_reset(
    BWCtx    ctx
    )
{
    UNREFERENCED_PARAMETER(ctx);

    return;
}

static BWData
dummy_bw_monitor_get_data(
    BWCtx    ctx,
    Uint32   numCores
    )
{
    BWGdiData* data;

    data = (BWGdiData*)osal_malloc(sizeof(BWGdiData));

    osal_memset((void*)data, 0x00, sizeof(BWGdiData));

    return (BWData)data;
}

static void
    dummy_bw_monitor_print(
    BWCtx   context,
    BWData  data,
    Uint32  picType
    )
{
    return ;
}

static BWMonitorOps_t s_dummy_ops = {
    dummy_bw_monitor_allocate,
    dummy_bw_monitor_release,
    dummy_bw_monitor_reset,
    dummy_bw_monitor_get_data,
    dummy_bw_monitor_print
};


/************************************************************************/
/* WAVE5 BACKBONE INTERFACE                                             */
/************************************************************************/
typedef struct _tag_backbone_bw_context_struct {
    BW_CONTEXT_COMMON_VARS

    Uint32 arid_prp_total; 
    Uint32 awid_prp_total; 
    Uint32 arid_fbd_y_total;
    Uint32 awid_fbc_y_total;
    Uint32 arid_fbd_c_total;
    Uint32 awid_fbc_c_total;
    Uint32 arid_pri_total; 
    Uint32 awid_pri_total; 
    Uint32 arid_sec_total; 
    Uint32 awid_sec_total; 
    Uint32 arid_proc_total; 
    Uint32 awid_proc_total; 
} BackBoneBwCtx;

typedef struct _tag_backbone_bw_data_struct {
    Uint32 arid_prp; 
    Uint32 awid_prp; 
    Uint32 arid_fbd_y;
    Uint32 awid_fbc_y;
    Uint32 arid_fbd_c;
    Uint32 awid_fbc_c;
    Uint32 arid_pri; 
    Uint32 awid_pri; 
    Uint32 arid_sec; 
    Uint32 awid_sec; 
    Uint32 arid_proc; 
    Uint32 awid_proc; 
} BWBackboneData; 

static BWCtx 
backbone_bw_monitor_allocate(
    Uint32   coreIndex
    )
{
    BackBoneBwCtx* context;

    context = (BackBoneBwCtx*)osal_malloc(sizeof(BackBoneBwCtx));
    osal_memset((void*)context, 0x00, sizeof(BackBoneBwCtx));

    return context;
}

static void 
backbone_bw_monitor_release(
    BWCtx    context
    )
{
    BackBoneBwCtx*  ctx = (BackBoneBwCtx*)context;
    Uint64          avgPriRead, avgPriWrite;
    Uint64          avgSecRead, avgSecWrite;
    Uint32          avgFBCWrite;
    Uint32          avgFBDRead;
    Uint64          avgProcRead, avgProcWrite;
    Uint64          avgPRPRead, avgPRPWrite;
    Uint64          avgWrite;
    Uint64          avgRead;
    Uint64          avg;

    if (ctx == NULL)
        return;

    avgPriRead   = ctx->arid_pri_total / ctx->numFrames;
    avgPriWrite  = ctx->awid_pri_total / ctx->numFrames;
    avgSecRead   = ctx->arid_sec_total / ctx->numFrames;
    avgSecWrite  = ctx->awid_sec_total / ctx->numFrames;
    avgProcRead  = ctx->arid_proc_total / ctx->numFrames;
    avgProcWrite = ctx->awid_proc_total / ctx->numFrames;
    avgFBCWrite  = (ctx->awid_fbc_y_total+ctx->awid_fbc_c_total) / ctx->numFrames;
    avgFBDRead   = (ctx->arid_fbd_y_total+ctx->arid_fbd_c_total) / ctx->numFrames;
    avgPRPRead   = ctx->arid_prp_total / ctx->numFrames;
    avgPRPWrite  = ctx->awid_prp_total / ctx->numFrames;
    avgWrite     = (ctx->awid_pri_total+ctx->awid_sec_total+ctx->awid_proc_total+ctx->awid_fbc_y_total+ctx->awid_fbc_c_total+ctx->awid_prp_total) / ctx->numFrames;
    avgRead      = (ctx->arid_pri_total+ctx->arid_sec_total+ctx->arid_proc_total+ctx->arid_fbd_y_total+ctx->arid_fbd_c_total+ctx->arid_prp_total) / ctx->numFrames;
    avg          = ctx->total / ctx->numFrames;

    osal_fprintf(ctx->fpBWTotal, "=======================================================================================================================================================\n");

    osal_fprintf(ctx->fpBWTotal, "AVER.  %10d %10d %10d %10d %10d %10d  %10d %10d %10d %10d %10d %10d  %10d\n",
                 avgPriWrite, avgSecWrite, avgFBCWrite, avgPRPWrite, avgProcWrite, avgWrite, 
                 avgPriRead,  avgSecRead,  avgFBDRead,  avgPRPRead,  avgProcRead,  avgRead, 
                 avg);

    osal_free(context);
}

static void
backbone_bw_monitor_reset(
    BWCtx    context
    )
{
    BackBoneBwCtx* ctx = (BackBoneBwCtx*)context;

    if (ctx == NULL)
        return;
}

static void
backbone_bw_monitor_print(
    BWCtx   context,
    BWData  data,
    Uint32  picType
    )
{
    BWBackboneData* bdata = (BWBackboneData*)data;
    BWCommonCtx*    common = (BWCommonCtx*)context;
    Uint32          total_wr_bw; 
    Uint32          total_rd_bw;

    total_wr_bw = bdata->awid_pri + bdata->awid_sec + bdata->awid_fbc_y + bdata->awid_fbc_c + bdata->awid_prp + bdata->awid_proc;
    total_rd_bw = bdata->arid_pri + bdata->arid_sec + bdata->arid_fbd_y + bdata->arid_fbd_c + bdata->arid_prp + bdata->arid_proc;
    if (common->numFrames == 1) {
        osal_fprintf(common->fpBWTotal, "  No.                              WRITE(B)                                                     READ(B)                                        TOTAL\n");
        osal_fprintf(common->fpBWTotal, "        ----------------------------------------------------------------  -----------------------------------------------------------------  ----------\n");
        osal_fprintf(common->fpBWTotal, "              PRI       SEC       FBC       PRP       PROC      TOTAL        PRI        SEC        FBD         PRP        PROC       TOTAL\n");
        osal_fprintf(common->fpBWTotal, "+======================================================================================================================================================\n");
    }
    osal_fprintf(common->fpBWTotal, "%5d %s %10d %10d %10d %10d %10d %10d  %10d %10d %10d %10d %10d %10d  %10d\n", common->numFrames-1, picTypeString[picType], 
        bdata->awid_pri, bdata->awid_sec, (bdata->awid_fbc_y+bdata->awid_fbc_c), bdata->awid_prp, bdata->awid_proc, total_wr_bw, 
        bdata->arid_pri, bdata->arid_sec, (bdata->arid_fbd_y+bdata->arid_fbd_c), bdata->arid_prp, bdata->arid_proc, total_rd_bw,
        (total_wr_bw + total_rd_bw));
    osal_fflush(common->fpBWTotal);
}

static BWData
backbone_bw_monitor_get_data(
    BWCtx    context,
    Uint32   numCores
    )
{
    BackBoneBwCtx*  ctx   = (BackBoneBwCtx*)context;
    BWBackboneData* idata;
    VPUBWData       bwdata = {0, };
    RetCode         ret = RETCODE_FAILURE;
    Uint32          prevTotal;

    if (ctx->instance->isDecoder == TRUE) {
        ret = VPU_DecGiveCommand(ctx->instance, GET_BANDWIDTH_REPORT, (void*)&bwdata);
    }
    else {
        ret = VPU_EncGiveCommand(ctx->instance, GET_BANDWIDTH_REPORT, (void*)&bwdata);
    }
    if (ret != RETCODE_SUCCESS) {
        VLOG(ERR, "%s:%d Failed to VPU_EncGiveCommand(ENC_GET_BW_REPORT), ret(%d)\n", __FUNCTION__, __LINE__, ret);
        return NULL;
    }

    idata = (BWBackboneData*)osal_malloc(sizeof(BWBackboneData));

    prevTotal   = ctx->total;
    ctx->total += idata->arid_prp   = bwdata.prpBwRead;
    ctx->total += idata->awid_prp   = bwdata.prpBwWrite;
    ctx->total += idata->arid_fbd_y = bwdata.fbdYRead;
    ctx->total += idata->awid_fbc_y = bwdata.fbcYWrite;
    ctx->total += idata->arid_fbd_c = bwdata.fbdCRead;
    ctx->total += idata->awid_fbc_c = bwdata.fbcCWrite;
    ctx->total += idata->arid_pri   = bwdata.priBwRead;
    ctx->total += idata->awid_pri   = bwdata.priBwWrite;
    ctx->total += idata->arid_sec   = bwdata.secBwRead;
    ctx->total += idata->awid_sec   = bwdata.secBwWrite;
    ctx->total += idata->arid_proc  = bwdata.procBwRead;
    ctx->total += idata->awid_proc  = bwdata.procBwWrite;

    if (prevTotal == ctx->total) {
        // VPU didn't decode a frame.
        return NULL;
    }

    ctx->arid_prp_total   += idata->arid_prp  ;
    ctx->awid_prp_total   += idata->awid_prp  ;
    ctx->arid_fbd_y_total += idata->arid_fbd_y;
    ctx->arid_fbd_c_total += idata->arid_fbd_c;
    ctx->awid_fbc_y_total += idata->awid_fbc_y;
    ctx->awid_fbc_c_total += idata->awid_fbc_c;
    ctx->arid_pri_total   += idata->arid_pri  ;
    ctx->awid_pri_total   += idata->awid_pri  ;
    ctx->arid_sec_total   += idata->arid_sec  ;
    ctx->awid_sec_total   += idata->awid_sec  ;
    ctx->arid_proc_total  += idata->arid_proc ;
    ctx->awid_proc_total  += idata->awid_proc ;

    return (BWData)idata;
}

static BWMonitorOps_t s_wave_backbone_ops = {
    backbone_bw_monitor_allocate,
    backbone_bw_monitor_release,
    backbone_bw_monitor_reset,
    backbone_bw_monitor_get_data,
    backbone_bw_monitor_print
};


/************************************************************************/
/*                                                                      */
/************************************************************************/
BWCtx
BWMonitorSetup(
    CodecInst*  instance, 
    BOOL        perFrame,
    char*       strLogDir
    )
{
    Uint32          coreIndex;
    Uint32          productId;
    Uint32          instIndex;
    BWCommonCtx*    common;
    BWMonitorOps_t* bwOps;
    osal_file_t     fp = NULL;
    char            path[256];

    coreIndex = instance->coreIdx;
    productId = instance->productId;
    instIndex = instance->instIndex;

    switch (productId) {
    case PRODUCT_ID_520:
    case PRODUCT_ID_525:
    case PRODUCT_ID_521:
        bwOps = &s_wave_backbone_ops;
        break;
    default:
        bwOps = &s_dummy_ops;
        break;
    }

    if (strLogDir) {
        sprintf(path, "./report/bw/%s/", strLogDir);
        MkDir(path);
        sprintf(path, "./report/bw/%s/report_bandwidth_%d_%d.txt", strLogDir, coreIndex, instIndex);
    }
    else {
        sprintf(path, "./report/bw/report_bandwidth_%d_%d.txt", coreIndex, instIndex);
        MkDir("./report/bw/");
    }

    if ((fp=osal_fopen(path, "w")) == NULL) {
        VLOG(ERR, "Failed to open %s\n", path);
    }

    common = (BWCommonCtx*)bwOps->allocate(coreIndex);
    common->ops             = bwOps;
    common->coreIndex       = coreIndex;
    common->instanceIndex   = instIndex;
    common->productId       = instance->productId;
    common->instance        = instance;
    common->fpBWTotal       = fp;
    common->enableReportPerFrame = perFrame;
    if (strLogDir) {
        sprintf(common->strLogDir, "%s", strLogDir);
    }
    else {
        osal_memset(common->strLogDir, 0x00, sizeof(common->strLogDir)*sizeof(char));
    }

    return common;
}

void 
BWMonitorReset(
    BWCtx    context
    )
{
    BWCommonCtx* common = (BWCommonCtx*)context;

    if (common == NULL)
        return;

    common->ops->reset(context);
}

void 
BWMonitorRelease(
    BWCtx    context
    )
{
    BWCommonCtx* common = (BWCommonCtx*)context;

    if (common == NULL)
        return;

    common->ops->release(context);

    if (common->fpBWTotal) 
        osal_fclose(common->fpBWTotal);
}

void 
BWMonitorUpdate(
    BWCtx       context,
    Uint32      numCores
    )
{
    BWCommonCtx* common = (BWCommonCtx*)context;

    if (common == NULL || common->fpBWTotal == NULL) {
        return;
    }

    if ((common->data=common->ops->get_data(context, numCores)) == NULL) {
        return;
    }

    common->numFrames++;
}

void 
    BWMonitorUpdatePrint(
    BWCtx       context,
    Uint32      picType
    )
{
    BWCommonCtx* common = (BWCommonCtx*)context;

    if (common == NULL || common->fpBWTotal == NULL ) {
        return;
    }
    if (common->data)
        common->ops->print(context, common->data, picType);

    osal_free(common->data);
}

