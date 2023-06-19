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

#include "vpuapifunc.h"
#include "product.h"
#include "wave5_regdefine.h"
#include "wave/wave5.h"
#ifdef __linux__
// #include <stdatomic.h>
#include <unistd.h>
#endif

//#include "misc/debug.h"


#ifdef BIT_CODE_FILE_PATH
#include BIT_CODE_FILE_PATH
#endif

#define W5_RET_DEC_DISPLAY_SIZE (W5_REG_BASE + 0x01D8)      // [FIX ME] delete ? or not?

#define INVALID_CORE_INDEX_RETURN_ERROR(_coreIdx)  \
    if (_coreIdx >= MAX_NUM_VPU_CORE) \
    return -1;

static const Uint16* s_pusBitCode[MAX_NUM_VPU_CORE] = {NULL,};
static int s_bitCodeSize[MAX_NUM_VPU_CORE] = {0,};

Uint32 __VPU_BUSY_TIMEOUT = VPU_BUSY_CHECK_TIMEOUT;

typedef struct _G_bm_handle{
    bm_handle_t bm_handle;
    unsigned int count;
} G_bm_handle;
static G_bm_handle g_bm_handle[MAX_NUM_VPU_CORE] = { {0, 0} };


#ifdef __linux__
static int bmhandle_atomic_lock = 0; /* atomic lock for bmlib_handle */
#elif _WIN32
static  volatile long bmhandle_atomic_lock = 0;
#endif

/* atomic lock for bmlib_handle operations*/
static void bm_handle_lock()
{
#ifdef __linux__
    while (__atomic_test_and_set(&bmhandle_atomic_lock, __ATOMIC_SEQ_CST))
    {
        usleep(100);
    }
#endif
#ifdef _WIN32
    while (InterlockedCompareExchange(&bmhandle_atomic_lock, 1, 0)) {
        Sleep(1);
    }
#endif
}

static void bm_handle_unlock()
{
#ifdef __linux__
    __atomic_clear(&bmhandle_atomic_lock, __ATOMIC_SEQ_CST);
#endif
#ifdef _WIN32
    InterlockedExchange(&bmhandle_atomic_lock, 0);
#endif
}




static RetCode CheckDecInstanceValidity(CodecInst* pCodecInst)
{
    RetCode ret;

    if (pCodecInst == NULL)
        return RETCODE_INVALID_HANDLE;

    ret = CheckInstanceValidity(pCodecInst);
    if (ret != RETCODE_SUCCESS) {
        return RETCODE_INVALID_HANDLE;
    }
    if (!pCodecInst->inUse) {
        return RETCODE_INVALID_HANDLE;
    }

    return ProductVpuDecCheckCapability(pCodecInst);
}

Int32 VPU_IsBusy(Uint32 coreIdx)
{
    Uint32 ret = 0;

    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

    ret = ProductVpuIsBusy(coreIdx);

    return ret != 0;
}

Int32 VPU_IsInit(Uint32 coreIdx)
{
    Int32 pc;

    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

    pc = ProductVpuIsInit(coreIdx);

    return pc;
}

Int32 VPU_WaitInterrupt(Uint32 coreIdx, int timeout)
{
    Int32 ret;
    CodecInst* instance;

    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

    if ((instance=GetPendingInst(coreIdx)) != NULL) {
        ret = ProductVpuWaitInterrupt(instance, timeout);
    }
    else {
        ret = -1;
    }

    return ret;
}

Int32 VPU_WaitInterruptEx(VpuHandle handle, int timeout)
{
    Int32 ret;
    CodecInst *pCodecInst;

    pCodecInst  = handle;

    INVALID_CORE_INDEX_RETURN_ERROR(pCodecInst->coreIdx);
    //VLOG(ERR,"In there %d coreId=%d\n",__LINE__,pCodecInst->coreIdx);
    ret = ProductVpuWaitInterrupt(pCodecInst, timeout);

    return ret;
}

void VPU_ClearInterrupt(Uint32 coreIdx)
{
    /* clear all interrupt flags */
    ProductVpuClearInterrupt(coreIdx, 0xffff);
}

void VPU_ClearInterruptEx(VpuHandle handle, Int32 intrFlag)
{
    CodecInst *pCodecInst;

    pCodecInst  = handle;

    ProductVpuClearInterrupt(pCodecInst->coreIdx, intrFlag);
}

int VPU_GetMvColBufSize(CodStd codStd, int width, int height, int num)
{
    int size_mvcolbuf = ProductCalculateAuxBufferSize(AUX_BUF_TYPE_MVCOL, codStd, width, height);

    if (codStd == STD_AVC || codStd == STD_HEVC || codStd == STD_VP9)
        size_mvcolbuf *= num;

    return size_mvcolbuf;
}
#if 0
RetCode VPU_GetFBCOffsetTableSize(CodStd codStd, int width, int height, int* ysize, int* csize)
{
    if (ysize == NULL || csize == NULL)
        return RETCODE_INVALID_PARAM;

    *ysize = ProductCalculateAuxBufferSize(AUX_BUF_TYPE_FBC_Y_OFFSET, codStd, width, height);
    *csize = ProductCalculateAuxBufferSize(AUX_BUF_TYPE_FBC_C_OFFSET, codStd, width, height);

    return RETCODE_SUCCESS;
}
#endif
int VPU_GetFrameBufSize(int coreIdx, int stride, int height, int mapType, int format, int interleave, DRAMConfig *pDramCfg)
{
    int productId;
    UNREFERENCED_PARAMETER(interleave);             /*!<< for backward compatiblity */

    if (coreIdx < 0 || coreIdx >= MAX_NUM_VPU_CORE)
        return -1;

    productId = ProductVpuGetId(coreIdx);

    return ProductCalculateFrameBufSize(productId, stride, height, (TiledMapType)mapType, (FrameBufferFormat)format, (BOOL)interleave, pDramCfg);
}


int VPU_GetProductId(int coreIdx)
{
    Int32 productId;

    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

    if ((productId=ProductVpuGetId(coreIdx)) != PRODUCT_ID_NONE) {
        return productId;
    }

    if (vdi_init(coreIdx) < 0)
        return -1;

    EnterLock(coreIdx);
    if (ProductVpuScan(coreIdx) == FALSE)
        productId = -1;
    else
        productId = ProductVpuGetId(coreIdx);
    LeaveLock((coreIdx));

    vdi_release(coreIdx);

    return productId;
}

int VPU_GetOpenInstanceNum(Uint32 coreIdx)
{
    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

    return vdi_get_instance_num(coreIdx);
}

static RetCode InitializeVPU(Uint32 coreIdx, const Uint16* code, Uint32 size)
{
    RetCode     ret;

    if (vdi_init(coreIdx) < 0)
        return RETCODE_FAILURE;

    EnterLock(coreIdx);

    if (vdi_get_instance_num(coreIdx) > 0) {
        if (ProductVpuScan(coreIdx) == 0) {
            LeaveLock(coreIdx);
            return RETCODE_NOT_FOUND_VPU_DEVICE;
        }
    }

    vdi_disable_kernel_reset(coreIdx);
#ifdef SUPPORT_SW_UART
    create_sw_uart_thread(coreIdx);
    vdi_delay_ms(500);
#endif
    if (VPU_IsInit(coreIdx) != 0 && ((vdi_get_instance_num(coreIdx) > 0) || ProductVpuGetId(coreIdx)==PRODUCT_ID_960)) {
        if(ProductVpuGetId(coreIdx)==PRODUCT_ID_960 && VPU_GetFirmwareStatus(coreIdx) == 0) {
            ret = ProductVpuReset(coreIdx, 0, SW_RESET_ON_BOOT);
            if (ret != RETCODE_SUCCESS) {
                LeaveLock(coreIdx);
                return ret;
            }
            vdi_set_bit_firmware_to_pm(coreIdx, code);
        }

        ProductVpuReInit(coreIdx, (void *)code, size);
        LeaveLock(coreIdx);
        return RETCODE_CALLED_BEFORE;
    }

    InitCodecInstancePool(coreIdx);

    VLOG(INFO, "reload firmware...\n");

    ret = ProductVpuReset(coreIdx, 0, SW_RESET_ON_BOOT);
    if (ret != RETCODE_SUCCESS) {
        LeaveLock(coreIdx);
        return ret;
    }

#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
	create_sw_uart_thread(coreIdx);
	osal_msleep(500);
#endif
    ret = ProductVpuInit(coreIdx, (void*)code, size);
    if (ret != RETCODE_SUCCESS) {
        LeaveLock(coreIdx);
        return ret;
    }

    LeaveLock(coreIdx);
    return RETCODE_SUCCESS;
}

RetCode VPU_Init(Uint32 coreIdx)
{
#ifdef BIT_CODE_FILE_PATH
    s_pusBitCode[coreIdx]  = bit_code;
    s_bitCodeSize[coreIdx] = sizeof(bit_code)/sizeof(bit_code[0]);
#endif

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return RETCODE_INVALID_PARAM;

    if (s_bitCodeSize[coreIdx] == 0)
        return RETCODE_NOT_FOUND_BITCODE_PATH;

    return InitializeVPU(coreIdx, s_pusBitCode[coreIdx], s_bitCodeSize[coreIdx]);
}

RetCode VPU_InitWithBitcode(Uint32 coreIdx, const Uint16* code, Uint32 size)
{
    if (coreIdx >= MAX_NUM_VPU_CORE)
        return RETCODE_INVALID_PARAM;
    if (code == NULL || size == 0)
        return RETCODE_INVALID_PARAM;
#if 0
    s_pusBitCode[coreIdx] = NULL;
    s_pusBitCode[coreIdx] = (Uint16 *)osal_malloc(size*sizeof(Uint16));
    if (!s_pusBitCode[coreIdx])
        return RETCODE_INSUFFICIENT_RESOURCE;
#if !(defined(WAVE512_PALLADIUM) || defined(CODA960_PALLADIUM))
    osal_memcpy((void *)s_pusBitCode[coreIdx], (const void *)code, size*sizeof(Uint16));
#endif
    s_bitCodeSize[coreIdx] = size;
#endif

    return InitializeVPU(coreIdx, code, size);
}

RetCode VPU_DeInit(Uint32 coreIdx)
{
    int ret;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return RETCODE_INVALID_PARAM;

    EnterLock(coreIdx);
    if (s_pusBitCode[coreIdx]) {
        osal_free((void *)s_pusBitCode[coreIdx]);
    }

    s_pusBitCode[coreIdx] = NULL;
    s_bitCodeSize[coreIdx] = 0;
    LeaveLock(coreIdx);

#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    destory_sw_uart_thread();
#endif
    ret = vdi_release(coreIdx);
    if (ret != 0)
        return RETCODE_FAILURE;

    return RETCODE_SUCCESS;
}

RetCode VPU_GetVersionInfo(Uint32 coreIdx, Uint32 *versionInfo, Uint32 *revision, Uint32 *productId)
{
    RetCode  ret;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return RETCODE_INVALID_PARAM;

    EnterLock(coreIdx);

    if (ProductVpuIsInit(coreIdx) == 0) {
        LeaveLock(coreIdx);
        return RETCODE_NOT_INITIALIZED;
    }

    if (GetPendingInst(coreIdx)) {
        LeaveLock(coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }

    if (productId != NULL) {
        *productId = ProductVpuGetId(coreIdx);
    }
    ret = ProductVpuGetVersion(coreIdx, versionInfo, revision);

    LeaveLock(coreIdx);

    return ret;
}

RetCode VPU_GetProductInfo(Uint32 coreIdx, VpuAttr *productInfo)
{
    RetCode  ret;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return RETCODE_INVALID_PARAM;

    EnterLock(coreIdx);

    if (ProductVpuIsInit(coreIdx) == 0) {
        LeaveLock(coreIdx);
        return RETCODE_NOT_INITIALIZED;
    }

    if (GetPendingInst(coreIdx)) {
        LeaveLock(coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }
    productInfo->productId = ProductVpuGetId(coreIdx);
    ret = ProductVpuGetProductInfo(coreIdx, productInfo);

    LeaveLock(coreIdx);

    return ret;
}

DecHandle VPU_DecGetHandleWithInstIdx(Uint32 coreIdx, Uint32 instIdx)
{
    vpu_instance_pool_t*    vip;

    if (MAX_NUM_VPU_CORE <= coreIdx || MAX_NUM_INSTANCE <= instIdx) {
        return NULL;
    }

    vip = (vpu_instance_pool_t *)vdi_get_instance_pool(coreIdx);
    if (!vip)
        return NULL;

    return (CodecInst *)vip->codecInstPool[instIdx];
}

RetCode VPU_DecOpen(DecHandle* pHandle, DecOpenParam* pop)
{
    CodecInst * pCodecInst;
    DecInfo*    pDecInfo;
    RetCode     ret;

    *pHandle = NULL;
    ret = ProductCheckDecOpenParam(pop);
    if (ret != RETCODE_SUCCESS)
        return ret;

    EnterLock(pop->coreIdx);

    if (VPU_IsInit(pop->coreIdx) == 0) {
        LeaveLock(pop->coreIdx);
        return RETCODE_NOT_INITIALIZED;
    }

    ret = GetCodecInstance(pop->coreIdx, &pCodecInst);
    if (ret != RETCODE_SUCCESS)
    {
        *pHandle = 0;
        LeaveLock(pop->coreIdx);
        return ret;
    }

    pCodecInst->isDecoder = TRUE;
    *pHandle = pCodecInst;
    pDecInfo = &pCodecInst->CodecInfo->decInfo;
    osal_memset(pDecInfo, 0x00, sizeof(DecInfo));
    osal_memcpy((void*)&pDecInfo->openParam, pop, sizeof(DecOpenParam));
    if (pop->bitstreamFormat == STD_MPEG4) {
        pCodecInst->codecMode    = MP4_DEC;
        pCodecInst->codecModeAux = MP4_AUX_MPEG4;
    }
    else if (pop->bitstreamFormat == STD_AVC) {
        if (pCodecInst->productId == PRODUCT_ID_521 || pCodecInst->productId == PRODUCT_ID_511) {
            pCodecInst->codecMode    = W_AVC_DEC;
        }
        else {
            pCodecInst->codecMode    = AVC_DEC;
            pCodecInst->codecModeAux = pop->avcExtension;
        }
    }
    else if (pop->bitstreamFormat == STD_VC1) {
        pCodecInst->codecMode = VC1_DEC;
    }
    else if (pop->bitstreamFormat == STD_MPEG2) {
        pCodecInst->codecMode = MP2_DEC;
    }
    else if (pop->bitstreamFormat == STD_H263) {
        pCodecInst->codecMode    = MP4_DEC;
        pCodecInst->codecModeAux = MP4_AUX_MPEG4;
    }
    else if (pop->bitstreamFormat == STD_DIV3) {
        pCodecInst->codecMode    = DV3_DEC;
        pCodecInst->codecModeAux = MP4_AUX_DIVX3;
    }
    else if (pop->bitstreamFormat == STD_RV) {
        pCodecInst->codecMode = RV_DEC;
    }
    else if (pop->bitstreamFormat == STD_AVS) {
        pCodecInst->codecMode = AVS_DEC;
    }
    else if (pop->bitstreamFormat == STD_THO) {
        pCodecInst->codecMode    = VPX_DEC;
        pCodecInst->codecModeAux = VPX_AUX_THO;
    }
    else if (pop->bitstreamFormat == STD_VP3) {
        pCodecInst->codecMode    = VPX_DEC;
        pCodecInst->codecModeAux = VPX_AUX_THO;
    }
    else if (pop->bitstreamFormat == STD_VP8) {
        pCodecInst->codecMode    = VPX_DEC;
        pCodecInst->codecModeAux = VPX_AUX_VP8;
    }
    else if (pop->bitstreamFormat == STD_HEVC) {
        pCodecInst->codecMode    = W_HEVC_DEC;
    }
    else if (pop->bitstreamFormat == STD_VP9) {
        pCodecInst->codecMode    = W_VP9_DEC;
    }
    else if (pop->bitstreamFormat == STD_AVS2) {
        pCodecInst->codecMode    = W_AVS2_DEC;
    }
    else if (pop->bitstreamFormat == STD_SVAC) {
        pCodecInst->codecMode    = W_SVAC_DEC;
    }
    else {
        LeaveLock(pop->coreIdx);
        return RETCODE_INVALID_PARAM;
    }

    pDecInfo->wtlEnable = pop->wtlEnable;
    pDecInfo->wtlMode   = pop->wtlMode;
    if (!pDecInfo->wtlEnable)
        pDecInfo->wtlMode = 0;

    pDecInfo->streamWrPtr        = pop->bitstreamBuffer;
    pDecInfo->streamRdPtr        = pop->bitstreamBuffer;
    pDecInfo->frameDelay         = -1;
    pDecInfo->streamBufStartAddr = pop->bitstreamBuffer;
    pDecInfo->streamBufSize      = pop->bitstreamBufferSize;
    pDecInfo->streamBufEndAddr   = pop->bitstreamBuffer + pop->bitstreamBufferSize;
    pDecInfo->reorderEnable      = VPU_REORDER_ENABLE;
    pDecInfo->mirrorDirection    = MIRDIR_NONE;
    pDecInfo->prevFrameEndPos    = pop->bitstreamBuffer;

    if ((ret=ProductVpuDecBuildUpOpenParam(pCodecInst, pop)) != RETCODE_SUCCESS) {
        *pHandle = 0;
        LeaveLock(pCodecInst->coreIdx);
        return ret;
    }

    pDecInfo->tiled2LinearEnable = pop->tiled2LinearEnable;
    pDecInfo->tiled2LinearMode   = pop->tiled2LinearMode;
    if (!pDecInfo->tiled2LinearEnable)
        pDecInfo->tiled2LinearMode = 0;     //coda980 only

    if (!pDecInfo->wtlEnable)               //coda980, wave320, wave410 only
        pDecInfo->wtlMode = 0;

    osal_memset((void*)&pDecInfo->cacheConfig, 0x00, sizeof(MaverickCacheConfig));

    vdi_resume_kernel_reset(pCodecInst->coreIdx);
    LeaveLock(pCodecInst->coreIdx);

    return RETCODE_SUCCESS;
}


DecHandle VPU_GetDecHanle(Uint32 coreIdx, Uint32 instIdx)
{
    vpu_instance_pool_t *vip = vdi_get_instance_pool(coreIdx);
    CodecInst*           pCodecInst;
    if (!vip) {
        return NULL;
    }

    pCodecInst = (CodecInst*)(vip->codecInstPool[instIdx]);
/*
    switch (coreIdx) {
    case 0: pCodecInst->productId = PRODUCT_ID_960;     break;
    case 1: pCodecInst->productId = PRODUCT_ID_960;     break;
    case 2: pCodecInst->productId = PRODUCT_ID_512;     break;
    default: pCodecInst->productId = PRODUCT_ID_NONE;   break;
    }
    */
    if(pCodecInst->productId == PRODUCT_ID_NONE)
        pCodecInst->productId = VPU_GetProductId(coreIdx);
    return (DecHandle)pCodecInst;
}

RetCode VPU_DecClose(DecHandle handle)
{
    CodecInst * pCodecInst;
    DecInfo * pDecInfo;
    RetCode ret;
    int i;
    bm_handle_t bm_handle;
    bm_handle =bmvpu_dec_get_bmlib_handle(handle->coreIdx);
    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    pCodecInst = handle;
    if (!pCodecInst) {
        return RETCODE_INVALID_HANDLE;
    }
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;

    EnterLock(pCodecInst->coreIdx);

    if ((ret=ProductVpuDecFiniSeq(pCodecInst)) != RETCODE_SUCCESS) {
        if (pCodecInst->loggingEnable)
            vdi_log(pCodecInst->coreIdx, DEC_SEQ_END, 2);

        if (ret == RETCODE_VPU_STILL_RUNNING) {
            LeaveLock(pCodecInst->coreIdx);
            return ret;
        }
    }
    if (pCodecInst->loggingEnable)
        vdi_log(pCodecInst->coreIdx, DEC_SEQ_END, 0);

    if (pDecInfo->vbDevSlice.size)
    {
        bm_free_mem(bm_handle,pDecInfo->vbDevSlice,pDecInfo->vbSliceVddr);
        pDecInfo->vbDevSlice.size=0;
    }

    if (pDecInfo->vbDevWork.size) {
        if (pDecInfo->workBufferAllocExt == 0) {
#ifndef BM_PCIE_MODE
        bm_mem_unmap_device_mem(bm_handle,(void *)pDecInfo->vbWorkVaddr,pDecInfo->vbDevWork.size);
#endif
        bm_free_device(bm_handle,pDecInfo->vbDevWork);
        pDecInfo->vbDevWork.size=0;
        }
        else {
#ifndef BM_PCIE_MODE
             bm_mem_unmap_device_mem(bm_handle,(void *)pDecInfo->vbWorkVaddr,pDecInfo->vbDevWork.size);
#endif
        }
    }

    if (pDecInfo->vbDevFrame.size) {
        if (pDecInfo->frameAllocExt == 0) {
            bm_free_mem(bm_handle,pDecInfo->vbDevFrame,pDecInfo->vbFrameVaddr);
            pDecInfo->vbDevFrame.size=0;
        }
    }
    for ( i=0 ; i<MAX_REG_FRAME; i++) {
        if (pDecInfo->vbDevMV[i].size)
        {
            bm_free_mem(bm_handle,pDecInfo->vbDevMV[i],pDecInfo->vbMVVaddr[i]);
            pDecInfo->vbDevMV[i].size=0;
        }

        if (pDecInfo->vbDevFbcYTbl[i].size)
        {

            bm_free_mem(bm_handle,pDecInfo->vbDevFbcYTbl[i],pDecInfo->vbFbcYTblVaddr[i]);
            pDecInfo->vbDevFbcYTbl[i].size=0;
        }

        if (pDecInfo->vbDevFbcCTbl[i].size)
        {
            bm_free_mem(bm_handle,pDecInfo->vbDevFbcCTbl[i],pDecInfo->vbFbcCTblVaddr[i]);
            pDecInfo->vbDevFbcCTbl[i].size=0;
        }
#ifndef BM_PCIE_MODE
        if(pDecInfo->vpu_frame_buffer_vaddr[i]!=0x00)
            {
                bm_mem_unmap_device_mem(bm_handle,(void *)pDecInfo->vpu_frame_buffer_vaddr[i],pDecInfo->vpu_frame_buffer_vaddr_size[i]);
            }
#endif
    }

    if (pDecInfo->vbDevTemp.size)
    {
        vdi_dettach_dma_memory(pCodecInst->coreIdx, &pDecInfo->vbTemp);
    }

    if (pDecInfo->vbDevPPU.size) {
        if (pDecInfo->ppuAllocExt == 0)
        {
            bm_free_mem(bm_handle,pDecInfo->vbDevPPU,0x00);
            pDecInfo->vbDevPPU.size=0;
        }
    }

    if (pDecInfo->vbDevWTL.size)
    {
          bm_free_mem(bm_handle,pDecInfo->vbDevWTL,0x00);
          pDecInfo->vbDevWTL.size=0;
    }
    // if (pDecInfo->vbUserData.size)
    // {
    //     vdi_dettach_dma_memory(pCodecInst->coreIdx, &pDecInfo->vbUserData);
    // }

    if (pDecInfo->vbDevReport.size)
    {
        bm_free_mem(bm_handle,pDecInfo->vbDevReport,pDecInfo->vbReportVddr);
        pDecInfo->vbDevReport.size=0;
    }
    if (GetPendingInst(pCodecInst->coreIdx) == pCodecInst)
        ClearPendingInst(pCodecInst->coreIdx);
    FreeCodecInstance(pCodecInst);
    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

RetCode VPU_DecSetEscSeqInit(DecHandle handle, int escape)
{
    CodecInst * pCodecInst;
    DecInfo * pDecInfo;
    RetCode ret;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;

    if (pDecInfo->openParam.bitstreamMode != BS_MODE_INTERRUPT)
        return RETCODE_INVALID_PARAM;

    pDecInfo->seqInitEscape = escape;

    return RETCODE_SUCCESS;
}

RetCode VPU_DecGetInitialInfo(DecHandle handle, DecInitialInfo* info)
{
    CodecInst*  pCodecInst;
    DecInfo*    pDecInfo;
    RetCode     ret;
    Int32       flags;
    Uint32      interruptBit;
    VpuAttr*    pAttr;

    if (PRODUCT_ID_W_SERIES(handle->productId)) {
        if (handle->productId == PRODUCT_ID_512 || handle->productId == PRODUCT_ID_515 || handle->productId == PRODUCT_ID_525 || handle->productId == PRODUCT_ID_521 || handle->productId == PRODUCT_ID_511) {
            interruptBit = INT_WAVE5_INIT_SEQ;
        }
        else
            return RETCODE_INVALID_PARAM;
    }
    else {
        /* CODA9xx */
        interruptBit = INT_BIT_SEQ_INIT;
    }

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    if (info == NULL)
        return RETCODE_INVALID_PARAM;

    pCodecInst = handle;
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;
    ret = ProductVpuDecCheckCapability(pCodecInst);
    if (ret != RETCODE_SUCCESS)
        return ret;

    EnterLock(pCodecInst->coreIdx);

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    if (GetPendingInst(pCodecInst->coreIdx)) {
        /* The other instance is running */
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }

    if (DecBitstreamBufEmpty(pDecInfo)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_WRONG_CALL_SEQUENCE;
    }

    ret = ProductVpuDecInitSeq(handle);
    if (ret != RETCODE_SUCCESS) {
        LeaveLock(pCodecInst->coreIdx);
        return ret;
    }

    if (pAttr->supportCommandQueue == TRUE) {
        LeaveLock(pCodecInst->coreIdx);
    }

    flags = ProductVpuWaitInterrupt(pCodecInst, __VPU_BUSY_TIMEOUT);

    if (pAttr->supportCommandQueue == TRUE) {
        EnterLock(pCodecInst->coreIdx);
    }

    if (flags == -1) {
        info->rdPtr = VpuReadReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr);
        info->wrPtr = VpuReadReg(pCodecInst->coreIdx, pDecInfo->streamWrPtrRegAddr);
        info->rdPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, info->rdPtr);
        info->wrPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, info->wrPtr);
        ret = RETCODE_VPU_RESPONSE_TIMEOUT;
    }
    else {
        if (flags&(1<<interruptBit))
            ProductVpuClearInterrupt(pCodecInst->coreIdx, (1<<interruptBit));

        if (flags != (1<<interruptBit))
            ret = RETCODE_FAILURE;
        else
            ret = ProductVpuDecGetSeqInfo(handle, info);
    }

    info->rdPtr = VpuReadReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr);
    info->wrPtr = VpuReadReg(pCodecInst->coreIdx, pDecInfo->streamWrPtrRegAddr);
    info->rdPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, info->rdPtr);
    info->wrPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, info->wrPtr);

    pDecInfo->initialInfo = *info;
    if (ret == RETCODE_SUCCESS) {
        pDecInfo->initialInfoObtained = 1;
    }

    SetPendingInst(pCodecInst->coreIdx, 0);

    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

RetCode VPU_DecIssueSeqInit(DecHandle handle)
{
    CodecInst*  pCodecInst;
    RetCode     ret;
    VpuAttr*    pAttr;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;

    EnterLock(pCodecInst->coreIdx);

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

#ifdef USE_PENDING_INSTANCE
    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }
#endif /* USE_PENDING_INSTANCE */

    ret = ProductVpuDecInitSeq(handle);
    if (ret == RETCODE_SUCCESS) {
        SetPendingInst(pCodecInst->coreIdx, pCodecInst);
    }

    if (pAttr->supportCommandQueue == TRUE) {
        SetPendingInst(pCodecInst->coreIdx, NULL);
        LeaveLock(pCodecInst->coreIdx);
    }

    return ret;
}

RetCode VPU_DecCompleteSeqInit(DecHandle handle, DecInitialInfo * info)
{
    CodecInst*  pCodecInst;
    DecInfo*    pDecInfo;
    RetCode     ret;
    VpuAttr*    pAttr;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    if (info == 0) {
        return RETCODE_INVALID_PARAM;
    }

    pCodecInst = handle;
    pDecInfo = &pCodecInst->CodecInfo->decInfo;

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    if (pAttr->supportCommandQueue == TRUE) {
        EnterLock(pCodecInst->coreIdx);
    }
#ifdef USE_PENDING_INSTANCE
    else {
        if (pCodecInst != GetPendingInst(pCodecInst->coreIdx)) {
            SetPendingInst(pCodecInst->coreIdx, 0);
            LeaveLock(pCodecInst->coreIdx);
            return RETCODE_WRONG_CALL_SEQUENCE;
        }
    }
#endif /* USE_PENDING_INSTANCE */

    ret = ProductVpuDecGetSeqInfo(handle, info);
    if (ret == RETCODE_SUCCESS) {
        pDecInfo->initialInfoObtained = 1;
    }
#ifdef CODA960_FVP
    info->picWidth = 1920;
    info->picHeight = 1080;
#endif


    info->rdPtr = ProductVpuDecGetRdPtr(pCodecInst);
    info->wrPtr = pDecInfo->streamWrPtr;
    //VLOG("wrptr=%p,rdptr=%p", info->wrPtr, info->rdPtr);
    pDecInfo->prevFrameEndPos = info->rdPtr;

    pDecInfo->initialInfo = *info;

    SetPendingInst(pCodecInst->coreIdx, NULL);

    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

static RetCode DecRegisterFrameBuffer(DecHandle handle, FrameBuffer *bufArray, int numFbsForDecoding, int numFbsForWTL, int stride, int height, int mapType)
{
    CodecInst*      pCodecInst;
    DecInfo*        pDecInfo;
    Int32           i;
    RetCode         ret;
    FrameBuffer*    fb, nullFb;
    bm_device_mem_t    *vb;
    FrameBufferFormat format = FORMAT_420;
    Int32          totalNumOfFbs;




    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    if (numFbsForDecoding > MAX_FRAMEBUFFER_COUNT || numFbsForWTL > MAX_FRAMEBUFFER_COUNT) {
        return RETCODE_INVALID_PARAM;
    }

    osal_memset(&nullFb, 0x00, sizeof(FrameBuffer));
    pCodecInst                  = handle;
    pDecInfo                    = &pCodecInst->CodecInfo->decInfo;
    pDecInfo->numFbsForDecoding = numFbsForDecoding;
    pDecInfo->numFbsForWTL      = numFbsForWTL;
    pDecInfo->numFrameBuffers   = numFbsForDecoding + numFbsForWTL;
    pDecInfo->stride            = stride;
    if (pCodecInst->codecMode == VPX_DEC)
        pDecInfo->frameBufferHeight = VPU_ALIGN64(height);
    else if (pCodecInst->codecMode == W_VP9_DEC)
        pDecInfo->frameBufferHeight = VPU_ALIGN64(height);
    else
        pDecInfo->frameBufferHeight = height;
    pDecInfo->mapType           = mapType;
    pDecInfo->mapCfg.productId  = pCodecInst->productId;

    ret = ProductVpuDecCheckCapability(pCodecInst);
    if (ret != RETCODE_SUCCESS)
        return ret;

    if (!pDecInfo->initialInfoObtained)
        return RETCODE_WRONG_CALL_SEQUENCE;

    if ( (stride < pDecInfo->initialInfo.picWidth) || (stride % 8 != 0) || (height<pDecInfo->initialInfo.picHeight) ) {
        return RETCODE_INVALID_STRIDE;
    }

    EnterLock(pCodecInst->coreIdx);
#ifdef USE_PENDING_INSTANCE
    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }
#endif /* USE_PENDING_INSTANCE */
    /* clear frameBufPool */
    for (i=0; i<(int)(sizeof(pDecInfo->frameBufPool)/sizeof(FrameBuffer)); i++) {
        pDecInfo->frameBufPool[i] = nullFb;
    }

    /* LinearMap or TiledMap, compressed framebuffer inclusive. */
    if (pDecInfo->initialInfo.lumaBitdepth > 8 || pDecInfo->initialInfo.chromaBitdepth > 8)
        format = FORMAT_420_P10_16BIT_LSB;

    totalNumOfFbs = numFbsForDecoding + numFbsForWTL;

    if (pCodecInst->codecMode == W_SVAC_DEC && pDecInfo->initialInfo.spatialSvcEnable == TRUE)
        totalNumOfFbs = numFbsForDecoding + numFbsForDecoding + numFbsForWTL; // (EL + BL + linear)

    if (bufArray) {
        for(i=0; i<totalNumOfFbs; i++)
            pDecInfo->frameBufPool[i] = bufArray[i];
    }
    else {
        vb = &pDecInfo->vbDevFrame;
        fb = &pDecInfo->frameBufPool[0];
        ret = ProductVpuAllocateFramebuffer(
            (CodecInst*)handle, fb, (TiledMapType)mapType, numFbsForDecoding, stride, height, format,
            pDecInfo->openParam.cbcrInterleave,
            pDecInfo->openParam.nv21,
            pDecInfo->openParam.frameEndian, vb, 0, FB_TYPE_CODEC);
        if (ret != RETCODE_SUCCESS) {
            LeaveLock(pCodecInst->coreIdx);
            return ret;
        }
    }
    if (pCodecInst->productId != PRODUCT_ID_960) {
        pDecInfo->mapCfg.tiledBaseAddr = pDecInfo->frameBufPool[0].bufY;
    }

    /* LinearMap */
    if (pDecInfo->wtlEnable == TRUE || numFbsForWTL != 0) {
        pDecInfo->stride = stride;
        if (!bufArray) {
            TiledMapType map;
            map = pDecInfo->wtlMode==FF_FRAME ? LINEAR_FRAME_MAP : LINEAR_FIELD_MAP;
            vb = &pDecInfo->vbDevWTL;
            fb = &pDecInfo->frameBufPool[numFbsForDecoding];

            ret = ProductVpuAllocateFramebuffer(
                (CodecInst*)handle, fb, map, numFbsForWTL, stride, height,
                pDecInfo->wtlFormat,
                pDecInfo->openParam.cbcrInterleave,
                pDecInfo->openParam.nv21,
                pDecInfo->openParam.frameEndian, vb, 0, FB_TYPE_PPU);

            if (ret != RETCODE_SUCCESS) {
                LeaveLock(pCodecInst->coreIdx);
                return ret;
            }
        }
    }

    ret = ProductVpuRegisterFramebuffer(pCodecInst);

    LeaveLock(pCodecInst->coreIdx);


    return ret;
}

RetCode VPU_DecRegisterFrameBuffer(DecHandle handle, FrameBuffer *bufArray, int num, int stride, int height, int mapType)
{
    DecInfo* pDecInfo  = &handle->CodecInfo->decInfo;
    Uint32   numWTL    = 0;

    if (pDecInfo->wtlEnable == TRUE) numWTL = num;
    return DecRegisterFrameBuffer(handle, bufArray, num, numWTL, stride, height, mapType);
}

RetCode VPU_DecRegisterFrameBufferEx(DecHandle handle, FrameBuffer *bufArray, int numOfDecFbs, int numOfDisplayFbs, int stride, int height, int mapType)
{
    return DecRegisterFrameBuffer(handle, bufArray, numOfDecFbs, numOfDisplayFbs, stride, height, mapType);
}

RetCode VPU_DecGetFrameBuffer(DecHandle handle, int frameIdx, FrameBuffer* frameBuf)
{
    CodecInst * pCodecInst;
    DecInfo * pDecInfo;
    RetCode ret;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    if (frameBuf == 0)
        return RETCODE_INVALID_PARAM;

    pCodecInst = handle;
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;

    if (frameIdx < 0 || frameIdx >= pDecInfo->numFrameBuffers)
        return RETCODE_INVALID_PARAM;

    *frameBuf = pDecInfo->frameBufPool[frameIdx];

    return RETCODE_SUCCESS;
}

RetCode VPU_DecUpdateFrameBuffer(
    DecHandle     handle,
    FrameBuffer*  fbcFb,
    FrameBuffer*  linearFb,
    Int32         mvColIndex,
    Int32         picWidth,
    Int32         picHeight
    )
{
    if (handle == NULL) {
        return RETCODE_INVALID_HANDLE;
    }

    return ProductVpuDecUpdateFrameBuffer((CodecInst*)handle, fbcFb, linearFb, mvColIndex, picWidth, picHeight);
}

int VPU_DecGetBitstreamBufferRoom(DecHandle handle, PhysicalAddress wrPtr)
{
    DecInfo*        pDecInfo = &handle->CodecInfo->decInfo;
    PhysicalAddress rdPtr;
    int             room;
    Int32           coreIdx = handle->coreIdx;
    VpuAttr*        pAttr = &g_VpuCoreAttributes[coreIdx];
    CodecInst*      pCodecInst;
    pCodecInst = handle;

    if (pAttr->supportCommandQueue == TRUE) {
        EnterLock(coreIdx);
        rdPtr = ProductVpuDecGetRdPtr(pCodecInst);
        LeaveLock(coreIdx);
    }
    else {
        rdPtr = pDecInfo->streamRdPtr;
    }

    if (pDecInfo->openParam.bitstreamMode != BS_MODE_PIC_END) {
        if (wrPtr < rdPtr) {
            room = rdPtr - wrPtr - pAttr->bitstreamBufferMargin*2;
        }
        else {
            room = (pDecInfo->streamBufEndAddr - wrPtr) + (rdPtr - pDecInfo->streamBufStartAddr) - pAttr->bitstreamBufferMargin*2;
        }
        room--;
    }
    else {
        if(wrPtr>=rdPtr)
            room = (pDecInfo->streamBufEndAddr - wrPtr);
        else
            room = rdPtr - wrPtr;
    }
    return room;
}

RetCode VPU_DecGetBitstreamBuffer(DecHandle handle, PhysicalAddress* prdPtr, PhysicalAddress* pwrPtr, Uint32* size)
{
    CodecInst*      pCodecInst;
    DecInfo*        pDecInfo;
    PhysicalAddress rdPtr;
    PhysicalAddress wrPtr;
    PhysicalAddress tempPtr;
    int             room;
    Int32           coreIdx;
    VpuAttr*        pAttr;

    coreIdx   = handle->coreIdx;
    pAttr = &g_VpuCoreAttributes[coreIdx];
    pCodecInst = handle;
    pDecInfo = &pCodecInst->CodecInfo->decInfo;

    if (pAttr->supportCommandQueue == TRUE) {
        if (pDecInfo->rdPtrValidFlag == TRUE) {         // when RdPtr has been updated by calling SetRdPtr.
			rdPtr = pDecInfo->streamRdPtr;
		}
		else {
			EnterLock(pCodecInst->coreIdx);
			rdPtr = ProductVpuDecGetRdPtr(pCodecInst);
			LeaveLock(pCodecInst->coreIdx);
		}
    }
    else {
        if (GetPendingInst(coreIdx) == pCodecInst) {
            if (pCodecInst->codecMode == AVC_DEC && pCodecInst->codecModeAux == AVC_AUX_MVC) {
                rdPtr = pDecInfo->streamRdPtr;
            }
            else {
                rdPtr = VpuReadReg(coreIdx, pDecInfo->streamRdPtrRegAddr);
                rdPtr = VPU_MapToAddr40Bit(coreIdx, rdPtr);
            }
        }
        else {
            rdPtr = pDecInfo->streamRdPtr;
        }
    }

    wrPtr = pDecInfo->streamWrPtr;

    pAttr = &g_VpuCoreAttributes[coreIdx];

    tempPtr = rdPtr;

    if (pDecInfo->openParam.bitstreamMode != BS_MODE_PIC_END) {
        if (wrPtr < tempPtr) {
            room = tempPtr - wrPtr - pAttr->bitstreamBufferMargin*2;
        }
        else {
            room = (pDecInfo->streamBufEndAddr - wrPtr) + (tempPtr - pDecInfo->streamBufStartAddr) - pAttr->bitstreamBufferMargin*2;
        }
        room--;
    }
    else {
        room = (pDecInfo->streamBufEndAddr - wrPtr);
    }

    if (prdPtr) *prdPtr = tempPtr;
    if (pwrPtr) *pwrPtr = wrPtr;
    if (size)   *size   = room;

    return RETCODE_SUCCESS;
}


RetCode VPU_DecUpdateBitstreamBuffer(DecHandle handle, int size)
{
    CodecInst*      pCodecInst;
    DecInfo*        pDecInfo;
    PhysicalAddress wrPtr;
    PhysicalAddress rdPtr;
    RetCode         ret;
    BOOL            running;
    VpuAttr*    pAttr;
    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;


    pCodecInst = handle;
    if (!pCodecInst)
        return RETCODE_INVALID_HANDLE;
    pAttr   = &g_VpuCoreAttributes[pCodecInst->coreIdx];
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;
    wrPtr      = pDecInfo->streamWrPtr;

    if (pAttr->supportCommandQueue == TRUE) {
        running = FALSE;
    }
    else {
        running = (BOOL)(GetPendingInst(pCodecInst->coreIdx) == pCodecInst);
    }

    if (size > 0) {
        Uint32      room    = 0;

        if (running == TRUE){
            rdPtr = VpuReadReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr);
            rdPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, rdPtr);
        }
        else
            rdPtr = pDecInfo->streamRdPtr;

        if (wrPtr < rdPtr) {
            if (rdPtr <= wrPtr + size) {
                return RETCODE_INVALID_PARAM;
            }
        }

        wrPtr += size;

        if (pDecInfo->openParam.bitstreamMode != BS_MODE_PIC_END) {
            if (wrPtr > pDecInfo->streamBufEndAddr) {
                room = wrPtr - pDecInfo->streamBufEndAddr;
                wrPtr = pDecInfo->streamBufStartAddr;
                wrPtr += room;
            }
            else if (wrPtr == pDecInfo->streamBufEndAddr) {
                wrPtr = pDecInfo->streamBufStartAddr;
            }
        }

        pDecInfo->streamWrPtr = wrPtr;
        pDecInfo->streamRdPtr = rdPtr;

        if (running == TRUE) {
            VpuWriteReg(pCodecInst->coreIdx, pDecInfo->streamWrPtrRegAddr, wrPtr);
        }
    }

    if (running == FALSE) {
        EnterLock(pCodecInst->coreIdx);
    }
    ret = ProductVpuDecSetBitstreamFlag(pCodecInst, running, size);
    if (running == FALSE) {
        LeaveLock(pCodecInst->coreIdx);
    }

    return ret;
}

RetCode VPU_DecUpdateBitstreamBufferParam(DecHandle handle, int size)
{
    CodecInst*      pCodecInst;
    DecInfo*        pDecInfo;
    PhysicalAddress wrPtr;
    PhysicalAddress rdPtr;
    RetCode         ret;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;


    pCodecInst = handle;
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;
    wrPtr      = pDecInfo->streamWrPtr;

    if (size > 0) {
        Uint32      room    = 0;

        rdPtr = pDecInfo->streamRdPtr;

        if (wrPtr < rdPtr) {
            if (rdPtr <= wrPtr + size) {
                return RETCODE_INVALID_PARAM;
            }
        }

        wrPtr += size;

        if (wrPtr >= pDecInfo->streamBufEndAddr) {
            room = wrPtr - pDecInfo->streamBufEndAddr;
            wrPtr = pDecInfo->streamBufStartAddr;
            wrPtr += room;
        }

        pDecInfo->streamWrPtr = wrPtr;
    }
    return ret;
}

RetCode VPU_HWReset(Uint32 coreIdx)
{
    if (vdi_hw_reset(coreIdx) < 0 )
        return RETCODE_FAILURE;

    if (GetPendingInst(coreIdx))
    {
        SetPendingInst(coreIdx, 0);
        LeaveLock(coreIdx);    //if vpu is in a lock state. release the state;
    }
    return RETCODE_SUCCESS;
}

/**
* VPU_SWReset
* IN
*    forcedReset : 1 if there is no need to waiting for BUS transaction,
*                  0 for otherwise
* OUT
*    RetCode : RETCODE_FAILURE if failed to reset,
*              RETCODE_SUCCESS for otherwise
*/
RetCode VPU_SWReset(Uint32 coreIdx, SWResetMode resetMode, void *pendingInst)
{
    CodecInst *pCodecInst = (CodecInst *)pendingInst;
    RetCode    ret = RETCODE_SUCCESS;
    VpuAttr*   attr = &g_VpuCoreAttributes[coreIdx];
    Uint32     instanceIndex = 0;

    if (attr->supportCommandQueue == TRUE) {
        if (pCodecInst && pCodecInst->loggingEnable) {
            vdi_log(pCodecInst->coreIdx, 0x10000, 1);
        }

        EnterLock(coreIdx);
        ret = ProductVpuReset(coreIdx, instanceIndex, resetMode);
        LeaveLock(coreIdx);

        if (pCodecInst && pCodecInst->loggingEnable) {
            vdi_log(pCodecInst->coreIdx, 0x10000, 0);
        }
    }
    else {
        if (pCodecInst) {
            if (pCodecInst->loggingEnable) {
                vdi_log(pCodecInst->coreIdx, (pCodecInst->productId == PRODUCT_ID_960 || pCodecInst->productId == PRODUCT_ID_980)?0x10:0x10000, 1);
            }
            instanceIndex = pCodecInst->instIndex;
        }
        else {
            EnterLock(coreIdx);
        }

        ret = ProductVpuReset(coreIdx, instanceIndex, resetMode);

        if (pCodecInst) {
            if (pCodecInst->loggingEnable) {
                vdi_log(pCodecInst->coreIdx, (pCodecInst->productId == PRODUCT_ID_960 || pCodecInst->productId == PRODUCT_ID_980)?0x10:0x10000, 0);
            }
            SetPendingInst(pCodecInst->coreIdx, 0);
            LeaveLock(coreIdx);
        }
        else {
            LeaveLock(coreIdx);
        }
    }

    return ret;
}

//---- VPU_SLEEP/WAKE
RetCode VPU_SleepWake(Uint32 coreIdx, int iSleepWake)
{
    RetCode ret;

    EnterLock(coreIdx);
    ret = ProductVpuSleepWake(coreIdx, iSleepWake, s_pusBitCode[coreIdx], s_bitCodeSize[coreIdx]);
    LeaveLock(coreIdx);

    return ret;
}

RetCode VPU_DecStartOneFrame(DecHandle handle, DecParam *param)
{
    CodecInst*  pCodecInst;
    DecInfo*    pDecInfo;
    Uint32      val     = 0;
    RetCode     ret     = RETCODE_SUCCESS;
    VpuAttr*    pAttr   = NULL;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    pCodecInst = (CodecInst*)handle;
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;


    if (pDecInfo->stride == 0) { // This means frame buffers have not been registered.
        return RETCODE_WRONG_CALL_SEQUENCE;
    }

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    EnterLock(pCodecInst->coreIdx);
#ifdef USE_PENDING_INSTANCE
    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }
#endif /* USE_PENDING_INSTANCE */

    if (pAttr->supportCommandQueue == FALSE) {
        EnterDispFlagLock(pCodecInst->coreIdx);
        val  = pDecInfo->frameDisplayFlag;
        val |= pDecInfo->setDisplayIndexes;
        val &= ~(Uint32)(pDecInfo->clearDisplayIndexes);
        VpuWriteReg(pCodecInst->coreIdx, pDecInfo->frameDisplayFlagRegAddr, val);
        pDecInfo->clearDisplayIndexes = 0;
        pDecInfo->setDisplayIndexes = 0;
        LeaveDispFlagLock(pCodecInst->coreIdx);
    }

    pDecInfo->frameStartPos = pDecInfo->streamRdPtr;

    ret = ProductVpuDecode(pCodecInst, param);

    if (pAttr->supportCommandQueue == TRUE) {
        SetPendingInst(pCodecInst->coreIdx, NULL);
        LeaveLock(pCodecInst->coreIdx);
    }
    else {
        SetPendingInst(pCodecInst->coreIdx, pCodecInst);
    }

    return ret;
}

RetCode VPU_DecGetOutputInfo(DecHandle handle, DecOutputInfo* info)
{
    CodecInst*  pCodecInst;
    DecInfo*    pDecInfo;
    RetCode     ret;
    VpuRect     rectInfo;
    Uint32      val;
    Int32       decodedIndex;
    Int32       displayIndex;
    Uint32      maxDecIndex;
    VpuAttr*    pAttr;
    PhysicalAddress streamRdPtr;
    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    if (info == 0) {
        return RETCODE_INVALID_PARAM;
    }

    pCodecInst  = handle;
    pDecInfo    = &pCodecInst->CodecInfo->decInfo;
    pAttr       = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    if (pAttr->supportCommandQueue == TRUE) {
        EnterLock(pCodecInst->coreIdx);
    }
#ifdef USE_PENDING_INSTANCE
    else {
        if (pCodecInst != GetPendingInst(pCodecInst->coreIdx)) {
            SetPendingInst(pCodecInst->coreIdx, 0);
            LeaveLock(pCodecInst->coreIdx);
            return RETCODE_WRONG_CALL_SEQUENCE;
        }
    }
#endif /* USE_PENDING_INSTANCE */

    osal_memset((void*)info, 0x00, sizeof(DecOutputInfo));

    ret = ProductVpuDecGetResult(pCodecInst, info);
    if (ret != RETCODE_SUCCESS) {
        info->rdPtr = pDecInfo->streamRdPtr;
        info->wrPtr = pDecInfo->streamWrPtr;
        SetPendingInst(pCodecInst->coreIdx, 0);
        LeaveLock(pCodecInst->coreIdx);
        return ret;
    }

    decodedIndex = info->indexFrameDecoded;


    // Calculate display frame region
    val = 0;
    if (decodedIndex >= 0 && decodedIndex < MAX_GDI_IDX) {
        //default value
        rectInfo.left   = 0;
        rectInfo.right  = info->decPicWidth;
        rectInfo.top    = 0;
        rectInfo.bottom = info->decPicHeight;

        if ((pCodecInst->productId != PRODUCT_ID_960 && pCodecInst->codecMode == W_AVC_DEC) || pCodecInst->codecMode == W_HEVC_DEC || pCodecInst->codecMode == AVC_DEC || pCodecInst->codecMode == AVS_DEC)
            rectInfo = pDecInfo->initialInfo.picCropRect;

        if (pCodecInst->codecMode == W_HEVC_DEC)
            pDecInfo->decOutInfo[decodedIndex].h265Info.decodedPOC = info->h265Info.decodedPOC;

        if (pCodecInst->codecMode == W_AVS2_DEC)
            pDecInfo->decOutInfo[decodedIndex].avs2Info.decodedPOI = info->avs2Info.decodedPOI;

        info->rcDecoded.left   =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.left   = rectInfo.left;
        info->rcDecoded.right  =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.right  = rectInfo.right;
        info->rcDecoded.top    =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.top    = rectInfo.top;
        info->rcDecoded.bottom =  pDecInfo->decOutInfo[decodedIndex].rcDecoded.bottom = rectInfo.bottom;
    }
    else
    {
        info->rcDecoded.left   = 0;
        info->rcDecoded.right  = info->decPicWidth;
        info->rcDecoded.top    = 0;
        info->rcDecoded.bottom = info->decPicHeight;
    }

    displayIndex = info->indexFrameDisplay;
    if (info->indexFrameDisplay >= 0 && info->indexFrameDisplay < MAX_GDI_IDX)
    {
        if (pCodecInst->codecMode == VC1_DEC) // vc1 rotates decoded frame buffer region. the other std rotated whole frame buffer region.
        {
            if (pDecInfo->rotationEnable && (pDecInfo->rotationAngle==90 || pDecInfo->rotationAngle==270))
            {
                info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
                info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
                info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
                info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
            }
            else
            {
                info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
                info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
                info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
                info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
            }
        }
        else
        {
            if (pDecInfo->rotationEnable)
            {
                switch(pDecInfo->rotationAngle)
                {
                case 90:
                    info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
                    info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
                    info->rcDisplay.top    = info->decPicWidth - pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
                    info->rcDisplay.bottom = info->decPicWidth - pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
                    break;
                case 270:
                    info->rcDisplay.left   = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
                    info->rcDisplay.right  = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
                    info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
                    info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
                    break;
                case 180:
                    info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
                    info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
                    info->rcDisplay.top    = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
                    info->rcDisplay.bottom = info->decPicHeight - pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
                    break;
                default:
                    info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
                    info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
                    info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
                    info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
                    break;
                }

            }
            else
            {
                info->rcDisplay.left   = pDecInfo->decOutInfo[displayIndex].rcDecoded.left;
                info->rcDisplay.right  = pDecInfo->decOutInfo[displayIndex].rcDecoded.right;
                info->rcDisplay.top    = pDecInfo->decOutInfo[displayIndex].rcDecoded.top;
                info->rcDisplay.bottom = pDecInfo->decOutInfo[displayIndex].rcDecoded.bottom;
            }

            if (pDecInfo->mirrorEnable) {
                Uint32 temp;
                Uint32 width  = (pDecInfo->rotationAngle == 90 || pDecInfo->rotationAngle == 270) ? info->decPicHeight: info->decPicWidth;
                Uint32 height = (pDecInfo->rotationAngle == 90 || pDecInfo->rotationAngle == 270) ? info->decPicWidth : info->decPicHeight;
                if (pDecInfo->mirrorDirection & MIRDIR_VER) {
                    temp = info->rcDisplay.top;
                    info->rcDisplay.top    = height - info->rcDisplay.bottom;
                    info->rcDisplay.bottom = height - temp;
                }
                if (pDecInfo->mirrorDirection & MIRDIR_HOR) {
                    temp = info->rcDisplay.left;
                    info->rcDisplay.left  = width - info->rcDisplay.right;
                    info->rcDisplay.right = width - temp;
                }
            }

            switch (pCodecInst->codecMode) {
            case W_HEVC_DEC:
                info->h265Info.displayPOC = pDecInfo->decOutInfo[displayIndex].h265Info.decodedPOC;
                break;
            case W_AVS2_DEC:
                info->avs2Info.displayPOI = pDecInfo->decOutInfo[displayIndex].avs2Info.decodedPOI;
                break;
            case W_AVC_DEC:
                // FIX ME.
                break;
            default:
                break;
            }
        }

        if (info->indexFrameDisplay == info->indexFrameDecoded)
        {
            info->dispPicWidth =  info->decPicWidth;
            info->dispPicHeight = info->decPicHeight;
        }
        else
        {
            /*
                When indexFrameDecoded < 0, and indexFrameDisplay >= 0
                info->decPicWidth and info->decPicHeight are still valid
                But those of pDecInfo->decOutInfo[displayIndex] are invalid in VP9
            */
            info->dispPicWidth  = pDecInfo->decOutInfo[displayIndex].decPicWidth;
            info->dispPicHeight = pDecInfo->decOutInfo[displayIndex].decPicHeight;
        }

        if (pDecInfo->scalerEnable == TRUE) { 
            if ((pDecInfo->scaleWidth != 0) && (pDecInfo->scaleHeight != 0)) {
                info->dispPicWidth     = pDecInfo->scaleWidth;
                info->dispPicHeight    = pDecInfo->scaleHeight;
                info->rcDisplay.right  = pDecInfo->scaleWidth;
                info->rcDisplay.bottom = pDecInfo->scaleHeight;
            }
        } 
    }
    else
    {
        info->rcDisplay.left   = 0;
        info->rcDisplay.right  = 0;
        info->rcDisplay.top    = 0;
        info->rcDisplay.bottom = 0;

        if (pDecInfo->rotationEnable || pDecInfo->mirrorEnable || pDecInfo->tiled2LinearEnable || pDecInfo->deringEnable) {
            info->dispPicWidth  = info->decPicWidth;
            info->dispPicHeight = info->decPicHeight;
        }
        else {
            info->dispPicWidth = 0;
            info->dispPicHeight = 0;
        }
    }

    if ( (pCodecInst->codecMode == VC1_DEC ) && info->indexFrameDisplay != -3) {
        if (pDecInfo->vc1BframeDisplayValid == 0) {
            if (info->picType == 2)
                info->indexFrameDisplay = -3;
            else
                pDecInfo->vc1BframeDisplayValid = 1;
        }
    }

    streamRdPtr =  ProductVpuDecGetRdPtr(pCodecInst);
    if (pDecInfo->openParam.bitstreamMode == BS_MODE_INTERRUPT) {
        pDecInfo->streamRdPtr = streamRdPtr;
    }

    pDecInfo->frameDisplayFlag = VpuReadReg(pCodecInst->coreIdx, pDecInfo->frameDisplayFlagRegAddr);
    if (pCodecInst->codecMode == W_VP9_DEC) {
        pDecInfo->frameDisplayFlag  &= 0xFFFF;
    }
    pDecInfo->frameEndPos = streamRdPtr;

    if (pDecInfo->frameEndPos < pDecInfo->frameStartPos)
        info->consumedByte = pDecInfo->frameEndPos + pDecInfo->streamBufSize - pDecInfo->frameStartPos;
    else
        info->consumedByte = pDecInfo->frameEndPos - pDecInfo->frameStartPos;

    if (pDecInfo->deringEnable || pDecInfo->mirrorEnable || pDecInfo->rotationEnable || pDecInfo->tiled2LinearEnable) {
        info->dispFrame        = pDecInfo->rotatorOutput;
        info->dispFrame.stride = pDecInfo->rotatorStride;
    }
    else {
        Int32 SvacSvcFlag = (pCodecInst->codecMode == W_SVAC_DEC && pDecInfo->initialInfo.spatialSvcEnable == TRUE);
            val = (pDecInfo->openParam.wtlEnable == TRUE ? pDecInfo->numFbsForDecoding: 0); //fbOffset
        if (SvacSvcFlag && pDecInfo->openParam.wtlEnable == TRUE)
            val = pDecInfo->numFbsForDecoding*2;

        maxDecIndex = (pDecInfo->numFbsForDecoding > pDecInfo->numFbsForWTL) ? (SvacSvcFlag ? pDecInfo->numFbsForDecoding*2 : pDecInfo->numFbsForDecoding) : pDecInfo->numFbsForWTL;

        if (0 <= info->indexFrameDisplay && info->indexFrameDisplay < (int)maxDecIndex)
            info->dispFrame = pDecInfo->frameBufPool[val+info->indexFrameDisplay];
    }
    info->rdPtr            = streamRdPtr;
    info->wrPtr            = pDecInfo->streamWrPtr;
    info->frameDisplayFlag = pDecInfo->frameDisplayFlag;

    info->sequenceNo = pDecInfo->initialInfo.sequenceNo;
    if (decodedIndex >= 0 && decodedIndex < MAX_GDI_IDX) {
        pDecInfo->decOutInfo[decodedIndex] = *info;
    }

    if (displayIndex >= 0 && displayIndex < MAX_GDI_IDX) {
        info->numOfTotMBs = info->numOfTotMBs;
        info->numOfErrMBs = info->numOfErrMBs;
        info->numOfTotMBsInDisplay = pDecInfo->decOutInfo[displayIndex].numOfTotMBs;
        info->numOfErrMBsInDisplay = pDecInfo->decOutInfo[displayIndex].numOfErrMBs;
        info->dispFrame.sequenceNo = info->sequenceNo;
    }
    else {
        info->numOfTotMBsInDisplay = 0;
        info->numOfErrMBsInDisplay = 0;
    }

    if (info->sequenceChanged != 0) {
        if (!(pCodecInst->productId == PRODUCT_ID_960 || pCodecInst->productId == PRODUCT_ID_980)) {
            /* Update new sequence information */
            osal_memcpy((void*)&pDecInfo->initialInfo, (void*)&pDecInfo->newSeqInfo, sizeof(DecInitialInfo));
        }
        if ((info->sequenceChanged & SEQ_CHANGE_INTER_RES_CHANGE) != SEQ_CHANGE_INTER_RES_CHANGE) {
            pDecInfo->initialInfo.sequenceNo++;
        }
    }

    SetPendingInst(pCodecInst->coreIdx, 0);

    LeaveLock(pCodecInst->coreIdx);

    return RETCODE_SUCCESS;
}

RetCode VPU_DecGetOutputFlush(DecHandle handle)
{
    CodecInst*  pCodecInst;
    RetCode     ret;
    Uint32      coreIdx, instIdx;

    pCodecInst  = handle;

    coreIdx = pCodecInst->coreIdx;
    instIdx = pCodecInst->instIndex;
    if(coreIdx > MAX_NUM_VPU_CORE || instIdx > MAX_NUM_INSTANCE)
        return RETCODE_FAILURE;

    EnterLock(pCodecInst->coreIdx);

    ret = ProductVpuDecGetResult(pCodecInst, NULL);
    if (ret != RETCODE_SUCCESS) {
        SetPendingInst(pCodecInst->coreIdx, 0);
        LeaveLock(pCodecInst->coreIdx);
        return ret;
    }

    SetPendingInst(pCodecInst->coreIdx, 0);

    LeaveLock(pCodecInst->coreIdx);

    return RETCODE_SUCCESS;
}

RetCode VPU_DecFrameBufferFlush(DecHandle handle, DecOutputInfo* pRemainings, Uint32* retNum)
{
    CodecInst*       pCodecInst;
    DecInfo*         pDecInfo;
    DecOutputInfo*   pOut;
    RetCode          ret;
    FramebufferIndex retIndex[MAX_GDI_IDX];
    Uint32           retRemainings = 0;
    Int32            i, index, val;
    VpuAttr*         pAttr      = NULL;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pDecInfo = &pCodecInst->CodecInfo->decInfo;

    EnterLock(pCodecInst->coreIdx);

#ifdef USE_PENDING_INSTANCE
    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }
#endif /* USE_PENDING_INSTANCE */

    osal_memset((void*)retIndex, 0xff, sizeof(retIndex));

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];
    if (pAttr->supportCommandQueue == FALSE) {
        EnterDispFlagLock(pCodecInst->coreIdx);
        val  = pDecInfo->frameDisplayFlag;
        val |= pDecInfo->setDisplayIndexes;
        val &= ~(Uint32)(pDecInfo->clearDisplayIndexes);
        VpuWriteReg(pCodecInst->coreIdx, pDecInfo->frameDisplayFlagRegAddr, val);
        pDecInfo->clearDisplayIndexes = 0;
        pDecInfo->setDisplayIndexes = 0;
        LeaveDispFlagLock(pCodecInst->coreIdx);
    }

    if ((ret=ProductVpuDecFlush(pCodecInst, retIndex, MAX_GDI_IDX)) != RETCODE_SUCCESS) {
        LeaveLock(pCodecInst->coreIdx);
        return ret;
    }

    if (pRemainings != NULL) {
        for (i=0; i<MAX_GDI_IDX; i++) {
            index = (pDecInfo->wtlEnable == TRUE) ? retIndex[i].tiledIndex : retIndex[i].linearIndex;
            if (index < 0)
                break;
            pRemainings[i] = pDecInfo->decOutInfo[index];
            pOut = &pRemainings[i];
            pOut->indexFrameDisplay         = pOut->indexFrameDecoded;
            pOut->indexFrameDisplayForTiled = pOut->indexFrameDecodedForTiled;
            if (pDecInfo->wtlEnable == TRUE)
                pOut->dispFrame             = pDecInfo->frameBufPool[pDecInfo->numFbsForDecoding+retIndex[i].linearIndex];
            else
                pOut->dispFrame             = pDecInfo->frameBufPool[index];

            pOut->dispFrame.sequenceNo = pOut->sequenceNo;

            if (pCodecInst->codecMode == W_VP9_DEC || pCodecInst->codecMode == W_AVS2_DEC) {
                Uint32   regVal;
                if ((pDecInfo->scaleWidth == 0) || (pDecInfo->scaleHeight == 0)) {
                    regVal = VpuReadReg(pCodecInst->coreIdx, W5_RET_DEC_DISPLAY_SIZE);
                    pOut->dispPicWidth   = regVal>>16;
                    pOut->dispPicHeight  = regVal&0xffff;
                }
                else {
                    pOut->dispPicWidth   = pDecInfo->scaleWidth;
                    pOut->dispPicHeight  = pDecInfo->scaleHeight;
                }
            } 
            else {
                pOut->dispPicWidth  = pOut->decPicWidth;
                pOut->dispPicHeight = pOut->decPicHeight;
            }


            if (pDecInfo->rotationEnable)
            {
                switch(pDecInfo->rotationAngle)
                {
                case 90:
                    pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.top;
                    pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.bottom;
                    pOut->rcDisplay.top    = pOut->decPicWidth - pDecInfo->decOutInfo[index].rcDecoded.right;
                    pOut->rcDisplay.bottom = pOut->decPicWidth - pDecInfo->decOutInfo[index].rcDecoded.left;
                    break;
                case 270:
                    pOut->rcDisplay.left   = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.bottom;
                    pOut->rcDisplay.right  = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.top;
                    pOut->rcDisplay.top    = pDecInfo->decOutInfo[index].rcDecoded.left;
                    pOut->rcDisplay.bottom = pDecInfo->decOutInfo[index].rcDecoded.right;
                    break;
                case 180:
                    pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.left;
                    pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.right;
                    pOut->rcDisplay.top    = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.bottom;
                    pOut->rcDisplay.bottom = pOut->decPicHeight - pDecInfo->decOutInfo[index].rcDecoded.top;
                    break;
                default:
                    pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.left;
                    pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.right;
                    pOut->rcDisplay.top    = pDecInfo->decOutInfo[index].rcDecoded.top;
                    pOut->rcDisplay.bottom = pDecInfo->decOutInfo[index].rcDecoded.bottom;
                    break;
                }
            }
            else
            {
                pOut->rcDisplay.left   = pDecInfo->decOutInfo[index].rcDecoded.left;
                pOut->rcDisplay.right  = pDecInfo->decOutInfo[index].rcDecoded.right;
                pOut->rcDisplay.top    = pDecInfo->decOutInfo[index].rcDecoded.top;
                pOut->rcDisplay.bottom = pDecInfo->decOutInfo[index].rcDecoded.bottom;
            }
            retRemainings++;
        }
    }

    if (retNum) *retNum = retRemainings;

    if (pCodecInst->loggingEnable)
        vdi_log(pCodecInst->coreIdx, DEC_BUF_FLUSH, 0);

    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

RetCode VPU_DecSetRdPtr(DecHandle handle, PhysicalAddress addr, int updateWrPtr)
{
    CodecInst* pCodecInst;
    CodecInst* pPendingInst;
    DecInfo*   pDecInfo;
    RetCode    ret;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }
    pCodecInst = (CodecInst*)handle;
    ret = ProductVpuDecCheckCapability(pCodecInst);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    pCodecInst   = handle;
    pDecInfo     = &pCodecInst->CodecInfo->decInfo;
    pPendingInst = GetPendingInst(pCodecInst->coreIdx);
    if (pCodecInst == pPendingInst) {
        VpuWriteReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr, addr);
    }
    else {
        EnterLock(pCodecInst->coreIdx);
        VpuWriteReg(pCodecInst->coreIdx, pDecInfo->streamRdPtrRegAddr, addr);
        LeaveLock(pCodecInst->coreIdx);
    }

    pDecInfo->streamRdPtr = addr;
    pDecInfo->prevFrameEndPos = addr;
    if (updateWrPtr == TRUE) {
        pDecInfo->streamWrPtr = addr;
    }
    pDecInfo->rdPtrValidFlag = 1;
    return RETCODE_SUCCESS;
}


RetCode VPU_EncSetWrPtr(EncHandle handle, PhysicalAddress addr, int updateRdPtr)
{
    CodecInst* pCodecInst;
    CodecInst* pPendingInst;
    EncInfo * pEncInfo;
    RetCode ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;
    pCodecInst = (CodecInst*)handle;

    if (pCodecInst->productId == PRODUCT_ID_960 || pCodecInst->productId == PRODUCT_ID_980) {
        return RETCODE_NOT_SUPPORTED_FEATURE;
    }

    pEncInfo = &handle->CodecInfo->encInfo;
    pPendingInst = GetPendingInst(pCodecInst->coreIdx);
    if (pCodecInst == pPendingInst) {
        VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr, addr);
    }
    else {
        EnterLock(pCodecInst->coreIdx);
        VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr, addr);
        LeaveLock(pCodecInst->coreIdx);
    }
    pEncInfo->streamWrPtr = addr;
    if (updateRdPtr)
        pEncInfo->streamRdPtr = addr;

    return RETCODE_SUCCESS;
}

RetCode VPU_DecClrDispFlag(DecHandle handle, int index)
{
    CodecInst*  pCodecInst;
    DecInfo*    pDecInfo;
    RetCode     ret         = RETCODE_SUCCESS;
    Int32       endIndex, SvacSvcFlag;
    VpuAttr*    pAttr       = NULL;
    BOOL        supportCommandQueue;

    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;
    pAttr      = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    SvacSvcFlag = (pCodecInst->codecMode == W_SVAC_DEC && pDecInfo->initialInfo.spatialSvcEnable == TRUE);

    endIndex = (pDecInfo->openParam.wtlEnable == TRUE) ? pDecInfo->numFbsForWTL : (SvacSvcFlag == TRUE ? pDecInfo->numFbsForDecoding*2 : pDecInfo->numFbsForDecoding);

    if ((index < 0) || (index > (endIndex - 1))) {
        return RETCODE_INVALID_PARAM;
    }

    supportCommandQueue = (pAttr->supportCommandQueue == TRUE);
    if (supportCommandQueue == TRUE) {
        EnterLock(pCodecInst->coreIdx);
        ret = ProductClrDispFlag(pCodecInst, index);
        LeaveLock(pCodecInst->coreIdx);
    }
    else {
        EnterDispFlagLock(pCodecInst->coreIdx);
        pDecInfo->clearDisplayIndexes |= (1<<index);
        LeaveDispFlagLock(pCodecInst->coreIdx);
    }

  return ret;
}

RetCode VPU_DecGiveCommand(DecHandle handle, CodecCommand cmd, void* param)
{
    CodecInst*          pCodecInst;
    DecInfo*            pDecInfo;
    RetCode             ret;
    bm_handle_t         bm_handle;
    bm_handle=bmvpu_dec_get_bmlib_handle(handle->coreIdx);
    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pDecInfo = &pCodecInst->CodecInfo->decInfo;
    switch (cmd)
    {
    case ENABLE_ROTATION :
        {
            if (pDecInfo->rotatorStride == 0) {
                return RETCODE_ROTATOR_STRIDE_NOT_SET;
            }
            pDecInfo->rotationEnable = 1;
            break;
        }

    case DISABLE_ROTATION :
        {
            pDecInfo->rotationEnable = 0;
            break;
        }

    case ENABLE_MIRRORING :
        {
            if (pDecInfo->rotatorStride == 0) {
                return RETCODE_ROTATOR_STRIDE_NOT_SET;
            }
            pDecInfo->mirrorEnable = 1;
            break;
        }
    case DISABLE_MIRRORING :
        {
            pDecInfo->mirrorEnable = 0;
            break;
        }
    case SET_MIRROR_DIRECTION :
        {

            MirrorDirection mirDir;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            mirDir = *(MirrorDirection *)param;
            if ( !(mirDir == MIRDIR_NONE) && !(mirDir==MIRDIR_HOR) && !(mirDir==MIRDIR_VER) && !(mirDir==MIRDIR_HOR_VER)) {
                return RETCODE_INVALID_PARAM;
            }
            pDecInfo->mirrorDirection = mirDir;

            break;
        }
    case SET_ROTATION_ANGLE :
        {
            int angle;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            angle = *(int *)param;
            if (angle != 0 && angle != 90 &&
                angle != 180 && angle != 270) {
                    return RETCODE_INVALID_PARAM;
            }
            if (pDecInfo->rotatorStride != 0) {
                if (angle == 90 || angle ==270) {
                    if (pDecInfo->initialInfo.picHeight > pDecInfo->rotatorStride) {
                        return RETCODE_INVALID_PARAM;
                    }
                } else {
                    if (pDecInfo->initialInfo.picWidth > pDecInfo->rotatorStride) {
                        return RETCODE_INVALID_PARAM;
                    }
                }
            }

            pDecInfo->rotationAngle = angle;
            break;
        }
    case SET_ROTATOR_OUTPUT :
        {
            FrameBuffer *frame;
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            frame = (FrameBuffer *)param;

            pDecInfo->rotatorOutput = *frame;
            pDecInfo->rotatorOutputValid = 1;
            break;
        }

    case SET_ROTATOR_STRIDE :
        {
            int stride;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            stride = *(int *)param;
            if (stride % 8 != 0 || stride == 0) {
                return RETCODE_INVALID_STRIDE;
            }

            if (pDecInfo->rotationAngle == 90 || pDecInfo->rotationAngle == 270) {
                if (pDecInfo->initialInfo.picHeight > stride) {
                    return RETCODE_INVALID_STRIDE;
                }
            } else {
                if (pDecInfo->initialInfo.picWidth > stride) {
                    return RETCODE_INVALID_STRIDE;
                }
            }

            pDecInfo->rotatorStride = stride;
            break;
        }
    case DEC_SET_SPS_RBSP:
        {
            if (pCodecInst->codecMode != AVC_DEC) {
                return RETCODE_INVALID_COMMAND;
            }
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }

            return SetParaSet(handle, 0, (DecParamSet *)param);

        }

    case DEC_SET_PPS_RBSP:
        {
            if (pCodecInst->codecMode != AVC_DEC) {
                return RETCODE_INVALID_COMMAND;
            }
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }

            return SetParaSet(handle, 1, (DecParamSet *)param);
        }
    case ENABLE_DERING :
        {
            if (pDecInfo->rotatorStride == 0) {
                return RETCODE_ROTATOR_STRIDE_NOT_SET;
            }
            pDecInfo->deringEnable = 1;
            break;
        }

    case DISABLE_DERING :
        {
            pDecInfo->deringEnable = 0;
            break;
        }
    case SET_SEC_AXI:
        {
            SecAxiUse secAxiUse;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            secAxiUse = *(SecAxiUse *)param;
            if (handle->productId == PRODUCT_ID_512 || handle->productId == PRODUCT_ID_520 || handle->productId == PRODUCT_ID_515 ||
                handle->productId == PRODUCT_ID_525 || handle->productId == PRODUCT_ID_521 || handle->productId == PRODUCT_ID_511) {
                pDecInfo->secAxiInfo.u.wave.useIpEnable    = secAxiUse.u.wave.useIpEnable;
                pDecInfo->secAxiInfo.u.wave.useLfRowEnable = secAxiUse.u.wave.useLfRowEnable;
                pDecInfo->secAxiInfo.u.wave.useBitEnable   = secAxiUse.u.wave.useBitEnable;
                pDecInfo->secAxiInfo.u.wave.useSclEnable   = secAxiUse.u.wave.useSclEnable;
            }
            else {
                pDecInfo->secAxiInfo.u.coda9.useBitEnable  = secAxiUse.u.coda9.useBitEnable;
                pDecInfo->secAxiInfo.u.coda9.useIpEnable   = secAxiUse.u.coda9.useIpEnable;
                pDecInfo->secAxiInfo.u.coda9.useDbkYEnable = secAxiUse.u.coda9.useDbkYEnable;
                pDecInfo->secAxiInfo.u.coda9.useDbkCEnable = secAxiUse.u.coda9.useDbkCEnable;
                pDecInfo->secAxiInfo.u.coda9.useOvlEnable  = secAxiUse.u.coda9.useOvlEnable;
                pDecInfo->secAxiInfo.u.coda9.useBtpEnable  = secAxiUse.u.coda9.useBtpEnable;
            }

            break;
        }
    case ENABLE_REP_USERDATA:
        {
            if (!pDecInfo->userDataBufAddr) {
                return RETCODE_USERDATA_BUF_NOT_SET;
            }
            if (pDecInfo->userDataBufSize == 0) {
                return RETCODE_USERDATA_BUF_NOT_SET;
            }
            switch (pCodecInst->productId) {
            case PRODUCT_ID_512:
            case PRODUCT_ID_515:
            case PRODUCT_ID_521:
            case PRODUCT_ID_511:
                if (!param)
                    return RETCODE_INVALID_PARAM;
                pDecInfo->userDataEnable = *(Uint32*)param;
                break;
            case PRODUCT_ID_960:
            case PRODUCT_ID_980:
                pDecInfo->userDataEnable = TRUE;
                break;
            default:
                VLOG(INFO, "%s(ENABLE_REP_DATA) invalid productId(%d)\n", __FUNCTION__, pCodecInst->productId);
                return RETCODE_INVALID_PARAM;
            }
            break;
        }
    case DISABLE_REP_USERDATA:
        {
            pDecInfo->userDataEnable = 0;
            break;
        }
    case SET_ADDR_REP_USERDATA:
        {
            PhysicalAddress userDataBufAddr;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            userDataBufAddr = *(PhysicalAddress *)param;
            if (userDataBufAddr % 8 != 0 || userDataBufAddr == 0) {
                return RETCODE_INVALID_PARAM;
            }

            pDecInfo->userDataBufAddr = userDataBufAddr;
            break;
        }
    case SET_VIRT_ADDR_REP_USERDATA:
        {
            u64 userDataVirtAddr;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }

            if (!pDecInfo->userDataBufAddr) {
                return RETCODE_USERDATA_BUF_NOT_SET;
            }
            if (pDecInfo->userDataBufSize == 0) {
                return RETCODE_USERDATA_BUF_NOT_SET;
            }

            userDataVirtAddr = *(u64 *)param;
            if (!userDataVirtAddr) {
                return RETCODE_INVALID_PARAM;
            }

            pDecInfo->vbUserData.phys_addr = pDecInfo->userDataBufAddr;
            pDecInfo->vbUserData.size = pDecInfo->userDataBufSize;
            pDecInfo->vbUserData.virt_addr = (u64)userDataVirtAddr;
            if (vdi_attach_dma_memory(pCodecInst->coreIdx, &pDecInfo->vbUserData) != 0) {
                return RETCODE_INSUFFICIENT_RESOURCE;
            }

            break;
        }
    case SET_SIZE_REP_USERDATA:
        {
            PhysicalAddress userDataBufSize;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            userDataBufSize = *(PhysicalAddress *)param;

            pDecInfo->userDataBufSize = userDataBufSize;
            break;
        }

    case SET_USERDATA_REPORT_MODE:
        {
            int userDataMode;

            userDataMode = *(int *)param;
            if (userDataMode != 1 && userDataMode != 0) {
                return RETCODE_INVALID_PARAM;
            }
            pDecInfo->userDataReportMode = userDataMode;
            break;
        }
     case SET_CACHE_CONFIG:
        {
            MaverickCacheConfig *mcCacheConfig;
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            mcCacheConfig = (MaverickCacheConfig *)param;
            pDecInfo->cacheConfig = *mcCacheConfig;
        }
        break;
    case SET_LOW_DELAY_CONFIG:
        {
            LowDelayInfo *lowDelayInfo;
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            if (pCodecInst->productId != PRODUCT_ID_980) {
                return RETCODE_NOT_SUPPORTED_FEATURE;
            }
            lowDelayInfo = (LowDelayInfo *)param;

            if (lowDelayInfo->lowDelayEn) {
                if ( (pCodecInst->codecMode != AVC_DEC) ||
                    pDecInfo->rotationEnable ||
                    pDecInfo->mirrorEnable ||
                    pDecInfo->tiled2LinearEnable ||
                    pDecInfo->deringEnable) {
                        return RETCODE_INVALID_PARAM;
                }
            }

            pDecInfo->lowDelayInfo.lowDelayEn = lowDelayInfo->lowDelayEn;
            pDecInfo->lowDelayInfo.numRows    = lowDelayInfo->numRows;
        }
        break;

    case DEC_SET_FRAME_DELAY:
        {
            pDecInfo->frameDelay = *(int *)param;
            break;
        }
    case DEC_ENABLE_REORDER:
        {
            if((handle->productId == PRODUCT_ID_980) || (handle->productId == PRODUCT_ID_960) || (handle->productId == PRODUCT_ID_950))
            {
                if (pDecInfo->initialInfoObtained) {
                    return RETCODE_WRONG_CALL_SEQUENCE;
                }
            }

            pDecInfo->reorderEnable = 1;
            break;
        }
    case DEC_DISABLE_REORDER:
        {
            if((handle->productId == PRODUCT_ID_980) || (handle->productId == PRODUCT_ID_960) || (handle->productId == PRODUCT_ID_950))
            {
                if (pDecInfo->initialInfoObtained) {
                    return RETCODE_WRONG_CALL_SEQUENCE;
                }

                if(pCodecInst->codecMode != AVC_DEC && pCodecInst->codecMode != VC1_DEC && pCodecInst->codecMode != AVS_DEC) {
                    return RETCODE_INVALID_COMMAND;
                }
            }

            pDecInfo->reorderEnable = 0;
            break;
        }
    case DEC_SET_AVC_ERROR_CONCEAL_MODE:
        {
            if(pCodecInst->codecMode != AVC_DEC) {
                return RETCODE_INVALID_COMMAND;
            }

            pDecInfo->avcErrorConcealMode = *(int *)param;
            break;
        }
    case DEC_FREE_FRAME_BUFFER:
        {
            int i;
            if (pDecInfo->vbDevSlice.size)
                {
                    bm_free_mem(bm_handle,pDecInfo->vbDevSlice,pDecInfo->vbSliceVddr);
                    pDecInfo->vbDevSlice.size=0;
                }

            if (pDecInfo->vbDevFrame.size){
            if (pDecInfo->frameAllocExt == 0) {
                bm_free_mem(bm_handle,pDecInfo->vbDevFrame,pDecInfo->vbFrameVaddr);
                pDecInfo->vbDevFrame.size=0;
                }
            }
            for (i=0  ; i<MAX_REG_FRAME; i++) {
                if (pDecInfo->vbDevFbcYTbl[i].size)
                    {
                        bm_free_mem(bm_handle,pDecInfo->vbDevFbcYTbl[i],pDecInfo->vbFbcYTblVaddr[i]);
                        pDecInfo->vbFbcYTblVaddr[i]=0x00;
                    }
                if (pDecInfo->vbDevFbcCTbl[i].size)
                {
                    bm_free_mem(bm_handle,pDecInfo->vbDevFbcCTbl[i],pDecInfo->vbFbcCTblVaddr[i]);
                    pDecInfo->vbFbcCTblVaddr[i]=0x00;
                }

                if (pDecInfo->vbDevMV[i].size)
                {
                    bm_free_mem(bm_handle,pDecInfo->vbDevMV[i],pDecInfo->vbMVVaddr[i]);
                    pDecInfo->vbMVVaddr[i]=0x00;
                }
            }

            if (pDecInfo->vbDevPPU.size) {
                if (pDecInfo->ppuAllocExt == 0)
                {
                 bm_free_mem(bm_handle,pDecInfo->vbDevPPU,0x00);
                pDecInfo->vbDevPPU.size=0;
                }
            }

            if (pDecInfo->wtlEnable) {
                if (pDecInfo->vbDevWTL.size)
                {
                    bm_free_mem(bm_handle,pDecInfo->vbDevWTL,0x00);
                    pDecInfo->vbDevWTL.size=0;
                }
            }
            break;
        }
    case DEC_GET_FRAMEBUF_INFO:
        {
            DecGetFramebufInfo* fbInfo = (DecGetFramebufInfo*)param;
            Uint32 i;
            fbInfo->devMemFrame   = pDecInfo->vbDevFrame;
            fbInfo->devMemWTL     = pDecInfo->vbDevWTL;
            for (i=0  ; i<MAX_REG_FRAME; i++)
            {
                fbInfo->devMemInfoVbFbcYTbl[i] = pDecInfo->vbDevFbcYTbl[i];
                fbInfo->devMemInfoVbFbcCTbl[i] = pDecInfo->vbDevFbcCTbl[i];
                fbInfo->devMvCol[i]   = pDecInfo->vbDevMV[i];
            }

            for (i=0; i<MAX_GDI_IDX*2; i++) {
                fbInfo->framebufPool[i] = pDecInfo->frameBufPool[i];
            }
        }
        break;
    case DEC_RESET_FRAMEBUF_INFO:
        {
            int i;

            pDecInfo->vbDevFrame.u.device.device_addr     = 0;
            pDecInfo->vbFrameVaddr                        = 0;
            pDecInfo->vbDevFrame.size                     = 0;
            //pDecInfo->vbWTLVaddr                          = 0;
            pDecInfo->vbDevWTL.u.device.device_addr       = 0;
            pDecInfo->vbDevWTL.size            = 0;
            for (i=0  ; i<MAX_REG_FRAME; i++)
            {
                pDecInfo->vbDevFbcYTbl[i].u.device.device_addr   = 0;
                pDecInfo->vbFbcCTblVaddr[i]                      = 0;
                pDecInfo->vbDevFbcYTbl[i].size                   = 0;
                pDecInfo->vbDevFbcCTbl[i].u.device.device_addr   = 0;
                pDecInfo->vbFbcCTblVaddr[i]                      = 0;
                pDecInfo->vbDevFbcCTbl[i].size                  = 0;
                pDecInfo->vbDevMV[i].u.device.device_addr       = 0;
                pDecInfo->vbMVVaddr[i]                          = 0;
                pDecInfo->vbDevMV[i].size                       = 0;
            }

            pDecInfo->frameDisplayFlag      = 0;
            pDecInfo->setDisplayIndexes     = 0;
            pDecInfo->clearDisplayIndexes   = 0;
            break;
        }
    case DEC_GET_QUEUE_STATUS:
        {
            QueueStatusInfo* queueInfo = (QueueStatusInfo*)param;
            queueInfo->instanceQueueCount = pDecInfo->instanceQueueCount;
            queueInfo->totalQueueCount    = pDecInfo->totalQueueCount;
            break;
        }


    case ENABLE_DEC_THUMBNAIL_MODE:
        {
            pDecInfo->thumbnailMode = 1;
            break;
        }
    case DEC_GET_SEQ_INFO:
        {
            DecInitialInfo* seqInfo = (DecInitialInfo*)param;
            *seqInfo = pDecInfo->initialInfo;
            break;
        }
    case DEC_GET_FIELD_PIC_TYPE:
        {
            return RETCODE_FAILURE;
        }
    case DEC_GET_DISPLAY_OUTPUT_INFO:
        {
            DecOutputInfo *pDecOutInfo = (DecOutputInfo *)param;
            *pDecOutInfo = pDecInfo->decOutInfo[pDecOutInfo->indexFrameDisplay];
            break;
        }
    case GET_TILEDMAP_CONFIG:
        {
            TiledMapConfig *pMapCfg = (TiledMapConfig *)param;
            if (!pMapCfg) {
                return RETCODE_INVALID_PARAM;
            }
            if (!pDecInfo->stride) {
                return RETCODE_WRONG_CALL_SEQUENCE;

            }
            *pMapCfg = pDecInfo->mapCfg;
            break;
        }
    case SET_DRAM_CONFIG:
        {
            DRAMConfig *cfg = (DRAMConfig *)param;

            if (!cfg) {
                return RETCODE_INVALID_PARAM;
            }

            pDecInfo->dramCfg = *cfg;
            break;
        }
    case GET_DRAM_CONFIG:
        {
            DRAMConfig *cfg = (DRAMConfig *)param;

            if (!cfg) {
                return RETCODE_INVALID_PARAM;
            }

            *cfg = pDecInfo->dramCfg;

            break;
        }
    case GET_LOW_DELAY_OUTPUT:
        {
            DecOutputInfo *lowDelayOutput;
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }

            if (!pDecInfo->lowDelayInfo.lowDelayEn || pCodecInst->codecMode != AVC_DEC) {
                return RETCODE_INVALID_COMMAND;
            }

            if (pCodecInst != GetPendingInst(pCodecInst->coreIdx)) {
                return RETCODE_WRONG_CALL_SEQUENCE;
            }

            lowDelayOutput = (DecOutputInfo *)param;

            GetLowDelayOutput(pCodecInst, lowDelayOutput);
        }
        break;
    case ENABLE_LOGGING:
        {
            pCodecInst->loggingEnable = 1;
        }
        break;
    case DISABLE_LOGGING:
        {
            pCodecInst->loggingEnable = 0;
        }
        break;
    case DEC_SET_SEQ_CHANGE_MASK:
        if (PRODUCT_ID_NOT_W_SERIES(pCodecInst->productId))
            return RETCODE_INVALID_PARAM;
        pDecInfo->seqChangeMask = *(int*)param;
        break;
    case DEC_SET_WTL_FRAME_FORMAT:
        pDecInfo->wtlFormat = *(FrameBufferFormat*)param;
        break;
    case DEC_SET_DISPLAY_FLAG:
        {
            Int32       index;
            VpuAttr*    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];
            BOOL        supportCommandQueue = FALSE;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }

            index = *(Int32 *)param;

            supportCommandQueue = (pAttr->supportCommandQueue == TRUE);
            if (supportCommandQueue == TRUE) {
                EnterLock(pCodecInst->coreIdx);
                ProductSetDispFlag(pCodecInst, index);
                LeaveLock(pCodecInst->coreIdx);
            }
            else {
                EnterDispFlagLock(pCodecInst->coreIdx);
                pDecInfo->setDisplayIndexes |= (1<<index);
                LeaveDispFlagLock(pCodecInst->coreIdx);
            }

        }
        break;
    case DEC_GET_SCALER_INFO:
        {
            ScalerInfo* scalerInfo = (ScalerInfo*)param;
            if (scalerInfo == NULL) {
                return RETCODE_INVALID_PARAM;
            }
            scalerInfo->enScaler    = pDecInfo->scalerEnable;
            scalerInfo->scaleWidth  = pDecInfo->scaleWidth;
            scalerInfo->scaleHeight = pDecInfo->scaleHeight;
        }
        break;
    case DEC_SET_SCALER_INFO:
        {
            ScalerInfo* scalerInfo = (ScalerInfo*)param;

            if (!pDecInfo->initialInfoObtained) {
                return RETCODE_WRONG_CALL_SEQUENCE;
            }

            if (scalerInfo == NULL) {
                return RETCODE_INVALID_PARAM;
            }

            pDecInfo->scalerEnable = scalerInfo->enScaler;
            if (scalerInfo->enScaler == TRUE) {
                VpuAttr*    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];
                Uint32      scalerStep = (pAttr->support2AlignScaler == TRUE) ? 2 : 8;
                // minW = Ceil8(picWidth/8), minH = Ceil8(picHeight/8)
                Uint32   minScaleWidth  = VPU_ALIGN8(pDecInfo->initialInfo.picWidth>>3);
                Uint32   minScaleHeight = VPU_ALIGN8(pDecInfo->initialInfo.picHeight>>3);

                if (minScaleWidth == 0)  minScaleWidth  = 8;
                if (minScaleHeight == 0) minScaleHeight = 8;

                if (scalerInfo->scaleWidth < minScaleWidth || scalerInfo->scaleHeight < minScaleHeight) {
                    return RETCODE_INVALID_PARAM;
                }

                if (scalerInfo->scaleWidth > 0 || scalerInfo->scaleHeight > 0) {
                    if ((scalerInfo->scaleWidth % scalerStep) || scalerInfo->scaleWidth > (Uint32)(VPU_ALIGN8(pDecInfo->initialInfo.picWidth))) {
                        return RETCODE_INVALID_PARAM;
                    }

                    if ((scalerInfo->scaleHeight % scalerStep) || scalerInfo->scaleHeight > (Uint32)(VPU_ALIGN8(pDecInfo->initialInfo.picHeight))) {
                        return RETCODE_INVALID_PARAM;
                    }
                    pDecInfo->scaleWidth   = scalerInfo->scaleWidth;
                    pDecInfo->scaleHeight  = scalerInfo->scaleHeight;
                    pDecInfo->scalerEnable = scalerInfo->enScaler;
                }
            }
            break;
        }
    case DEC_SET_TARGET_TEMPORAL_ID:
        if (param == NULL) {
            return RETCODE_INVALID_PARAM;
        }

        if (pCodecInst->codecMode == W_HEVC_DEC) {
            Int32 targetSublayerId = *(Int32*)param;
            if (targetSublayerId > HEVC_MAX_SUB_LAYER_ID)
                return RETCODE_INVALID_PARAM;

            pDecInfo->targetSubLayerId = targetSublayerId;
        }
        else if (pCodecInst->codecMode == W_SVAC_DEC) {
            Uint32 targetSublayerId = *(Uint32*)param;
            if (targetSublayerId > SVAC_MAX_SUB_LAYER_ID)
                return RETCODE_INVALID_PARAM;

            pDecInfo->targetSubLayerId = targetSublayerId;
        }
        else {
            return RETCODE_NOT_SUPPORTED_FEATURE;

        }
        break;
    case DEC_SET_BWB_CUR_FRAME_IDX:
        pDecInfo->chBwbFrameIdx = *(Uint32*)param;
        break;
    case DEC_SET_FBC_CUR_FRAME_IDX:
        pDecInfo->chFbcFrameIdx = *(Uint32*)param;
        break;
    case DEC_SET_INTER_RES_INFO_ON:
        pDecInfo->interResChange = 1;
        break;
    case DEC_SET_INTER_RES_INFO_OFF:
        pDecInfo->interResChange = 0;
        break;
    case DEC_FREE_FBC_TABLE_BUFFER:
        {
            Uint32 fbcCurFrameIdx = *(Uint32*)param;
            if(pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx].size > 0) {
                bm_free_mem(bm_handle,pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx],pDecInfo->vbFbcYTblVaddr[fbcCurFrameIdx]);
                pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx].size = 0;
            }
            if(pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx].size > 0) {
                bm_free_mem(bm_handle,pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx],pDecInfo->vbFbcCTblVaddr[fbcCurFrameIdx]);
                pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx].size = 0;
            }
        }
        break;
    case DEC_FREE_MV_BUFFER:
        {
            Uint32 fbcCurFrameIdx = *(Uint32*)param;
            if(pDecInfo->vbDevMV[fbcCurFrameIdx].size > 0) {
                bm_free_mem(bm_handle,pDecInfo->vbDevMV[fbcCurFrameIdx],pDecInfo->vbMVVaddr[fbcCurFrameIdx]);
                pDecInfo->vbDevMV[fbcCurFrameIdx].size = 0;
            }
        }
        break;
    case DEC_ALLOC_MV_BUFFER:
        {
            Uint32 fbcCurFrameIdx = *(Uint32*)param;
            Uint32 size;
            size          = WAVE5_DEC_VP9_MVCOL_BUF_SIZE(pDecInfo->initialInfo.picWidth, pDecInfo->initialInfo.picHeight);
            pDecInfo->vbDevMV[fbcCurFrameIdx].u.device.device_addr = 0;
            pDecInfo->vbDevMV[fbcCurFrameIdx].size      = ((size+4095)&~4095)+4096;   /* 4096 is a margin */
            bm_free_mem(bm_handle,  pDecInfo->vbDevMV[fbcCurFrameIdx],pDecInfo->vbMVVaddr[fbcCurFrameIdx]);
            if(bmvpu_malloc_device_byte_heap(bm_handle,&pDecInfo->vbDevMV[fbcCurFrameIdx],pDecInfo->vbDevMV[fbcCurFrameIdx].size,HEAP_MASK,1)!=BM_SUCCESS)
                return RETCODE_INSUFFICIENT_RESOURCE;
// #ifndef BM_PCIE_MODE
//             bm_mem_mmap_device_mem_no_cache(bm_handle,&pDecInfo->vbDevMV[fbcCurFrameIdx],&pDecInfo->vbMVVaddr[fbcCurFrameIdx]);
// #else
//             pDecInfo->vbMVVaddr[fbcCurFrameIdx]=0xDEADBEEFl;
// #endif
            bm_vdi_mmap(bm_handle,&pDecInfo->vbDevMV[fbcCurFrameIdx],&pDecInfo->vbMVVaddr[fbcCurFrameIdx]);
        }
        break;
    case DEC_ALLOC_FBC_Y_TABLE_BUFFER:
        {
            Uint32 fbcCurFrameIdx = *(Uint32*)param;
            Uint32 size;
            size        = WAVE5_FBC_LUMA_TABLE_SIZE(VPU_ALIGN64(pDecInfo->initialInfo.picWidth), VPU_ALIGN64(pDecInfo->initialInfo.picHeight));
            size        = VPU_ALIGN16(size);
            pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx].u.device.device_addr = 0;
            pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx].size      = ((size+4095)&~4095)+4096;
            bm_free_mem(bm_handle,  pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx],pDecInfo->vbFbcYTblVaddr[fbcCurFrameIdx]);
            if(bmvpu_malloc_device_byte_heap(bm_handle,&pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx],pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx].size,HEAP_MASK,1)!=BM_SUCCESS)
                return RETCODE_INSUFFICIENT_RESOURCE;
// #ifndef BM_PCIE_MODE
//             bm_mem_mmap_device_mem_no_cache(bm_handle,&pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx],&pDecInfo->vbFbcYTblVaddr[fbcCurFrameIdx]);
// #else
//             pDecInfo->vbFbcYTblVaddr[fbcCurFrameIdx]=0xDEADBEEFl;
// #endif
            bm_vdi_mmap(bm_handle,&pDecInfo->vbDevFbcYTbl[fbcCurFrameIdx],&pDecInfo->vbFbcYTblVaddr[fbcCurFrameIdx]);
        }
        break;
    case DEC_ALLOC_FBC_C_TABLE_BUFFER:
        {
            Uint32 fbcCurFrameIdx = *(Uint32*)param;
            Uint32 size;
            size        = WAVE5_FBC_CHROMA_TABLE_SIZE(VPU_ALIGN64(pDecInfo->initialInfo.picWidth), VPU_ALIGN64(pDecInfo->initialInfo.picHeight));
            size        = VPU_ALIGN16(size);
            pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx].u.device.device_addr = 0;
            pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx].size      = ((size+4095)&~4095)+4096;
            bm_free_mem(bm_handle,  pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx],pDecInfo->vbFbcCTblVaddr[fbcCurFrameIdx]);
            if(bmvpu_malloc_device_byte_heap(bm_handle,&pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx],pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx].size,HEAP_MASK,1)!=BM_SUCCESS)
                return RETCODE_INSUFFICIENT_RESOURCE;
// #ifndef BM_PCIE_MODE
//             bm_mem_mmap_device_mem_no_cache(bm_handle,&pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx],&pDecInfo->vbFbcCTblVaddr[fbcCurFrameIdx]);
// #else
//             pDecInfo->vbFbcYTblVaddr[fbcCurFrameIdx]=0xDEADBEEFl;
// #endif
            bm_vdi_mmap(bm_handle,&pDecInfo->vbDevFbcCTbl[fbcCurFrameIdx],&pDecInfo->vbFbcCTblVaddr[fbcCurFrameIdx]);
        }
        break;
    case GET_BANDWIDTH_REPORT:
        ret = ProductVpuGetBandwidth(pCodecInst, (VPUBWData*)param);
        break;
    case SET_CYCLE_PER_TICK:
        {
            pDecInfo->cyclePerTick = *(Uint32 *)param;
        }
        break;
    case GET_DEBUG_INFORM:
        ret = ProductVpuGetDebugInfo(pCodecInst, (VPUDebugInfo*)param);
        break;
    default:
        return RETCODE_INVALID_COMMAND;
    }

    return ret;
}

RetCode  VPU_DecAllocateFrameBuffer(DecHandle handle, FrameBufferAllocInfo info, FrameBuffer *frameBuffer)
{
    CodecInst*      pCodecInst;
    DecInfo*        pDecInfo;
    RetCode         ret;
    Uint32          gdiIndex;
    ret = CheckDecInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pDecInfo   = &pCodecInst->CodecInfo->decInfo;

    if (!frameBuffer) {
        return RETCODE_INVALID_PARAM;
    }
    //bm_handle= bmvpu_dec_get_bmlib_handle(handle->coreIdx);
    if (info.type == FB_TYPE_PPU) {
        if (pDecInfo->numFrameBuffers == 0)
            return RETCODE_WRONG_CALL_SEQUENCE;
        if (frameBuffer[0].updateFbInfo == TRUE) {
            pDecInfo->ppuAllocExt = TRUE;
        }
        pDecInfo->ppuAllocExt = frameBuffer[0].updateFbInfo;
        gdiIndex = pDecInfo->numFbsForDecoding;
        ret = ProductVpuAllocateFramebuffer(
            pCodecInst,  frameBuffer, (TiledMapType)info.mapType, (Int32)info.num,
            info.stride, info.height, info.format, info.cbcrInterleave, info.nv21, info.endian, &pDecInfo->vbDevPPU, gdiIndex, FB_TYPE_PPU);
    }
    else if (info.type == FB_TYPE_CODEC) {
        gdiIndex = 0;
        if (frameBuffer[0].updateFbInfo == TRUE) {
            pDecInfo->frameAllocExt = TRUE;
        }
        ret = ProductVpuAllocateFramebuffer(
            pCodecInst, frameBuffer, (TiledMapType)info.mapType, (Int32)info.num,
            info.stride, info.height, info.format, info.cbcrInterleave, info.nv21, info.endian, &pDecInfo->vbDevFrame, gdiIndex, (FramebufferAllocType)info.type);
        pDecInfo->mapCfg.tiledBaseAddr = pDecInfo->vbDevFrame.u.device.device_addr ;
    }

    return ret;
}

RetCode VPU_EncOpen(EncHandle* pHandle, EncOpenParam * pop)
{
    CodecInst*  pCodecInst;
    EncInfo*    pEncInfo;
    RetCode     ret;

    if ((ret=ProductCheckEncOpenParam(pop)) != RETCODE_SUCCESS)
        return ret;

    EnterLock(pop->coreIdx);

    if (VPU_IsInit(pop->coreIdx) == 0) {
        LeaveLock(pop->coreIdx);
        return RETCODE_NOT_INITIALIZED;
    }

    ret = GetCodecInstance(pop->coreIdx, &pCodecInst);
    if (ret == RETCODE_FAILURE) {
        *pHandle = 0;
        LeaveLock(pop->coreIdx);
        return RETCODE_FAILURE;
    }

    pCodecInst->isDecoder = FALSE;
    *pHandle = pCodecInst;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;

    osal_memset(pEncInfo, 0x00, sizeof(EncInfo));
    pEncInfo->openParam = *pop;

    if ((ret=ProductVpuEncBuildUpOpenParam(pCodecInst, pop)) != RETCODE_SUCCESS) {
        *pHandle = 0;
    }

    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

RetCode VPU_EncClose(EncHandle handle)
{
    CodecInst*  pCodecInst;
    EncInfo*    pEncInfo;
    RetCode     ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;

    EnterLock(pCodecInst->coreIdx);

    if (pEncInfo->initialInfoObtained) {
        VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);
        VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);

        if ((ret=ProductVpuEncFiniSeq(pCodecInst)) != RETCODE_SUCCESS) {
            if (pCodecInst->loggingEnable)
                vdi_log(pCodecInst->coreIdx, ENC_SEQ_END, 2);

            if (ret == RETCODE_VPU_STILL_RUNNING) {
                LeaveLock(pCodecInst->coreIdx);
                return ret;
            }
        }
        if (pCodecInst->loggingEnable)
            vdi_log(pCodecInst->coreIdx, ENC_SEQ_END, 0);
        pEncInfo->streamWrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);
        pEncInfo->streamWrPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, pEncInfo->streamWrPtr);
    }

    if (pEncInfo->vbScratch.size) {
        vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbScratch);
    }

    if (pEncInfo->vbWork.size) {
        vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbWork);
    }

    if (pEncInfo->vbFrame.size) {
        if (pEncInfo->frameAllocExt == 0)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbFrame);
    }

    if (pCodecInst->codecMode == W_HEVC_ENC || pCodecInst->codecMode == W_SVAC_ENC || pCodecInst->codecMode == W_AVC_ENC) {
        if (pEncInfo->vbSubSamBuf.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbSubSamBuf);

        if (pEncInfo->vbSubSamBufBL.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbSubSamBufBL);

        if (pEncInfo->vbMV.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbMV);

        if (pEncInfo->vbMVBL.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbMVBL);

        if (pEncInfo->vbFbcYTbl.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbFbcYTbl);

        if (pEncInfo->vbFbcYTblBL.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbFbcYTblBL);

        if (pEncInfo->vbFbcCTbl.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbFbcCTbl);

        if (pEncInfo->vbFbcCTblBL.size)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbFbcCTblBL);
    }

    if (pEncInfo->vbPPU.size) {
        if (pEncInfo->ppuAllocExt == 0)
            vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbPPU);
    }
    if (pEncInfo->vbSubSampFrame.size)
        vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbSubSampFrame);
    if (pEncInfo->vbMvcSubSampFrame.size)
        vdi_free_dma_memory(pCodecInst->coreIdx, &pEncInfo->vbMvcSubSampFrame);

    LeaveLock(pCodecInst->coreIdx);

    FreeCodecInstance(pCodecInst);

    return ret;
}

RetCode VPU_EncGetInitialInfo(EncHandle handle, EncInitialInfo * info)
{
    CodecInst*  pCodecInst;
    EncInfo*    pEncInfo;
    RetCode     ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    if (info == 0) {
        return RETCODE_INVALID_PARAM;
    }

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;

    EnterLock(pCodecInst->coreIdx);

    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }

    if ((ret=ProductVpuEncSetup(pCodecInst)) != RETCODE_SUCCESS) {
        LeaveLock(pCodecInst->coreIdx);
        return ret;
    }

    if (pCodecInst->codecMode == AVC_ENC && pCodecInst->codecModeAux == AVC_AUX_MVC)
        info->minFrameBufferCount = 3; // reconstructed frame + 2 reference frame
    else if(pCodecInst->codecMode == W_HEVC_ENC || pCodecInst->codecMode == W_SVAC_ENC || pCodecInst->codecMode == W_AVC_ENC) {
        info->minFrameBufferCount   = pEncInfo->initialInfo.minFrameBufferCount;
        info->minSrcFrameCount      = pEncInfo->initialInfo.minSrcFrameCount;
    }
    else
        info->minFrameBufferCount = 2; // reconstructed frame + reference frame

    pEncInfo->initialInfo         = *info;
    pEncInfo->initialInfoObtained = TRUE;

    LeaveLock(pCodecInst->coreIdx);

    return RETCODE_SUCCESS;
}

RetCode VPU_EncRegisterFrameBuffer(EncHandle handle, FrameBuffer* bufArray, int num, int stride, int height, TiledMapType mapType)
{
    CodecInst*      pCodecInst;
    EncInfo*        pEncInfo;
    Int32           i;
    RetCode         ret;
    EncOpenParam*   openParam;
    FrameBuffer*    fb;

    ret = CheckEncInstanceValidity(handle);
    // FIXME temp
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;
    openParam = &pEncInfo->openParam;

    if (pEncInfo->stride) 
        return RETCODE_CALLED_BEFORE;

    if (!pEncInfo->initialInfoObtained) 
        return RETCODE_WRONG_CALL_SEQUENCE;

    if (num < pEncInfo->initialInfo.minFrameBufferCount) 
        return RETCODE_INSUFFICIENT_FRAME_BUFFERS;

    if (stride == 0 || (stride % 8 != 0) || stride < 0) 
        return RETCODE_INVALID_STRIDE;

    if (height == 0 || height < 0)
        return RETCODE_INVALID_PARAM;

    if (openParam->bitstreamFormat == STD_HEVC) {
        if (stride % 32 != 0)
            return RETCODE_INVALID_STRIDE;
    }

    EnterLock(pCodecInst->coreIdx);

    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }

    pEncInfo->numFrameBuffers   = num;
    pEncInfo->stride            = stride;
    pEncInfo->frameBufferHeight = height;
    pEncInfo->mapType           = mapType;
    pEncInfo->mapCfg.productId  = pCodecInst->productId;

    if (bufArray) {
        for(i=0; i<num; i++) {
            pEncInfo->frameBufPool[i] = bufArray[i];
        }

        if (openParam->EncStdParam.waveParam.svcEnable == TRUE) {
            for(i=num; i<num*2; i++) {
                pEncInfo->frameBufPool[i] = bufArray[i];
            }
        }
    }

    if (pEncInfo->frameAllocExt == FALSE) {
        fb = pEncInfo->frameBufPool;
        if (bufArray) {
            if (bufArray[0].bufCb == (Uint32)-1 && bufArray[0].bufCr == (Uint32)-1) {
                Uint32 size;
                pEncInfo->frameAllocExt = TRUE;
                size = ProductCalculateFrameBufSize(pCodecInst->productId, stride, height,
                                                    (TiledMapType)mapType, (FrameBufferFormat)openParam->srcFormat,
                                                    (BOOL)openParam->cbcrInterleave, NULL);
                if (mapType == LINEAR_FRAME_MAP) {
                    pEncInfo->vbFrame.phys_addr = bufArray[0].bufY;
                    pEncInfo->vbFrame.size      = size * num;
                }
            }
        }
        ret = ProductVpuAllocateFramebuffer(
            pCodecInst, fb, (TiledMapType)mapType, num, stride, height, (FrameBufferFormat)openParam->srcFormat,
            openParam->cbcrInterleave, FALSE, openParam->frameEndian, (bm_device_mem_t*)&pEncInfo->vbFrame, 0, FB_TYPE_CODEC);
        if (ret != RETCODE_SUCCESS) {
            SetPendingInst(pCodecInst->coreIdx, 0);
            LeaveLock(pCodecInst->coreIdx);
            return ret;
        }
    }

    ret = ProductVpuRegisterFramebuffer(pCodecInst);

    SetPendingInst(pCodecInst->coreIdx, 0);

    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

RetCode VPU_EncGetBitstreamBuffer( EncHandle handle,
    PhysicalAddress * prdPrt,
    PhysicalAddress * pwrPtr,
    int * size)
{
    CodecInst * pCodecInst;
    EncInfo * pEncInfo;
    PhysicalAddress rdPtr;
    PhysicalAddress wrPtr;
    Uint32 room;
    RetCode ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    if ( prdPrt == 0 || pwrPtr == 0 || size == 0) {
        return RETCODE_INVALID_PARAM;
    }

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;

    rdPtr = pEncInfo->streamRdPtr;

    if (GetPendingInst(pCodecInst->coreIdx) == pCodecInst) {
        wrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);
        wrPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, wrPtr);
    }
    else {
        if (handle->productId == PRODUCT_ID_520 || handle->productId == PRODUCT_ID_525 || handle->productId == PRODUCT_ID_521) {
            EnterLock(pCodecInst->coreIdx);
            ProductVpuEncGetRdWrPtr(pCodecInst, &rdPtr, &wrPtr);
            LeaveLock(pCodecInst->coreIdx);
        }
        else
            wrPtr = pEncInfo->streamWrPtr;
    }

    if(pEncInfo->ringBufferEnable == 1 || pEncInfo->lineBufIntEn == 1) {
        if (wrPtr >= rdPtr) {
            room = wrPtr - rdPtr;
        }
        else {
            room = (pEncInfo->streamBufEndAddr - rdPtr) + (wrPtr - pEncInfo->streamBufStartAddr);
        }
    }
    else {
        if(wrPtr >= rdPtr)
            room = wrPtr - rdPtr;
        else
            return RETCODE_INVALID_PARAM;
    }

    *prdPrt = rdPtr;
    *pwrPtr = wrPtr;
    *size = room;

    return RETCODE_SUCCESS;
}

RetCode VPU_EncUpdateBitstreamBuffer(
    EncHandle handle,
    int size)
{
    CodecInst * pCodecInst;
    EncInfo * pEncInfo;
    PhysicalAddress wrPtr;
    PhysicalAddress rdPtr;
    RetCode ret;
    int        room = 0;
    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;

    rdPtr = pEncInfo->streamRdPtr;

    if (GetPendingInst(pCodecInst->coreIdx) == pCodecInst) {
        wrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);
        wrPtr = VPU_MapToAddr40Bit(pCodecInst->coreIdx, wrPtr);
    }
    else
        wrPtr = pEncInfo->streamWrPtr;

    if ( rdPtr < wrPtr ) {
        if ( rdPtr + size  > wrPtr ) {
            return RETCODE_INVALID_PARAM;
        }
    }

    if (pEncInfo->ringBufferEnable == TRUE || pEncInfo->lineBufIntEn == TRUE) {
        if (handle->productId == PRODUCT_ID_520 || handle->productId == PRODUCT_ID_525 || handle->productId == PRODUCT_ID_521) {
            if (VPU_ALIGN16(wrPtr) == pEncInfo->streamRdPtr + pEncInfo->streamBufSize) {  // linebuffer full detected.
                EnterLock(pCodecInst->coreIdx);
                ProductVpuEncUpdateBitstreamBuffer(pCodecInst);
                LeaveLock(pCodecInst->coreIdx);
            }
        }
        else {
            rdPtr += size;
            if (rdPtr > pEncInfo->streamBufEndAddr) {
                if (pEncInfo->lineBufIntEn == TRUE) {
                    return RETCODE_INVALID_PARAM;
                }
                room = rdPtr - pEncInfo->streamBufEndAddr;
                rdPtr = pEncInfo->streamBufStartAddr;
                rdPtr += room;
            }
            if (rdPtr == pEncInfo->streamBufEndAddr)
            rdPtr = pEncInfo->streamBufStartAddr;
        }
    }
    else {
        rdPtr = pEncInfo->streamBufStartAddr;
    }

    pEncInfo->streamRdPtr = rdPtr;
    pEncInfo->streamWrPtr = wrPtr;
    if (GetPendingInst(pCodecInst->coreIdx) == pCodecInst)
        VpuWriteReg(pCodecInst->coreIdx, pEncInfo->streamRdPtrRegAddr, rdPtr);

    if (pEncInfo->lineBufIntEn == TRUE) {
        pEncInfo->streamRdPtr = pEncInfo->streamBufStartAddr;
    }

    return RETCODE_SUCCESS;
}

RetCode VPU_EncStartOneFrame(
    EncHandle handle,
    EncParam * param 
    )
{
    CodecInst*          pCodecInst;
    EncInfo*            pEncInfo;
    RetCode             ret;
    VpuAttr*            pAttr   = NULL;
    vpu_instance_pool_t* vip;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pEncInfo   = &pCodecInst->CodecInfo->encInfo;
    vip        = (vpu_instance_pool_t *)vdi_get_instance_pool(pCodecInst->coreIdx);
    if (!vip) {
        return RETCODE_INVALID_HANDLE;
    }

    if (pEncInfo->stride == 0) { // This means frame buffers have not been registered.
        return RETCODE_WRONG_CALL_SEQUENCE;
    }

    ret = CheckEncParam(handle, param);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    EnterLock(pCodecInst->coreIdx);

    pEncInfo->ptsMap[param->srcIdx] = (pEncInfo->openParam.enablePTS == TRUE) ? GetTimestamp(handle) : param->pts;

    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }

    ret = ProductVpuEncode(pCodecInst, param);

    if (pAttr->supportCommandQueue == TRUE) {
        SetPendingInst(pCodecInst->coreIdx, NULL);
        LeaveLock(pCodecInst->coreIdx);
    }
    else {
        SetPendingInst(pCodecInst->coreIdx, pCodecInst);
    }

    return ret;
}

RetCode VPU_EncGetOutputInfo(
    EncHandle       handle,
    EncOutputInfo*  info
    )
{
    CodecInst*  pCodecInst;
    EncInfo*    pEncInfo;
    RetCode     ret;
    VpuAttr*    pAttr;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    if (info == 0) {
        return RETCODE_INVALID_PARAM;
    }

    pCodecInst = handle;
    pEncInfo   = &pCodecInst->CodecInfo->encInfo;
    pAttr      = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    if (pAttr->supportCommandQueue == TRUE) {
        EnterLock(pCodecInst->coreIdx);
    }
    else {
        if (pCodecInst != GetPendingInst(pCodecInst->coreIdx)) {
            SetPendingInst(pCodecInst->coreIdx, 0);
            LeaveLock(pCodecInst->coreIdx);
            return RETCODE_WRONG_CALL_SEQUENCE;
        }
    }

    ret = ProductVpuEncGetResult(pCodecInst, info);

    if (ret == RETCODE_SUCCESS) {
        if (info->encSrcIdx >= 0 && info->reconFrameIndex >= 0 )
            info->pts = pEncInfo->ptsMap[info->encSrcIdx];
    }
    else {
        info->pts = 0LL;
    }

    SetPendingInst(pCodecInst->coreIdx, 0);
    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

RetCode VPU_EncGiveCommand(
    EncHandle       handle,
    CodecCommand    cmd,
    void*           param
    )
{
    CodecInst*  pCodecInst;
    EncInfo*    pEncInfo;
    RetCode     ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;
    switch (cmd) 
    {
    case ENABLE_ROTATION :
        {
            pEncInfo->rotationEnable = 1;            
        }
        break;        
    case DISABLE_ROTATION :
        {
            pEncInfo->rotationEnable = 0;
        }
        break;
    case ENABLE_MIRRORING :
        {
            pEncInfo->mirrorEnable = 1;
        }
        break;
    case DISABLE_MIRRORING :
        {
            pEncInfo->mirrorEnable = 0;            
        }
        break;
    case SET_MIRROR_DIRECTION :
        {    
            MirrorDirection mirDir;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            mirDir = *(MirrorDirection *)param;
            if ( !(mirDir == MIRDIR_NONE) && !(mirDir==MIRDIR_HOR) && !(mirDir==MIRDIR_VER) && !(mirDir==MIRDIR_HOR_VER)) {
                return RETCODE_INVALID_PARAM;
            }
            pEncInfo->mirrorDirection = mirDir;
        }
        break;        
    case SET_ROTATION_ANGLE :
        {
            int angle;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            angle = *(int *)param;
            if (angle != 0 && angle != 90 &&
                angle != 180 && angle != 270) {
                    return RETCODE_INVALID_PARAM;
            }
            if (pEncInfo->initialInfoObtained && (angle == 90 || angle ==270)) {
                return RETCODE_INVALID_PARAM;
            }
            pEncInfo->rotationAngle = angle;            
        }
        break;
    case SET_CACHE_CONFIG:
        {
            MaverickCacheConfig *mcCacheConfig;
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            mcCacheConfig = (MaverickCacheConfig *)param;
            pEncInfo->cacheConfig = *mcCacheConfig;
        }
        break;
    case ENC_PUT_VIDEO_HEADER:
        {
            EncHeaderParam *encHeaderParam;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }                
            encHeaderParam = (EncHeaderParam *)param;
            if (pCodecInst->codecMode == MP4_ENC ) {
                if (!( VOL_HEADER<=encHeaderParam->headerType && encHeaderParam->headerType <= VIS_HEADER)) {
                    return RETCODE_INVALID_PARAM;
                }
            } 
            else if  (pCodecInst->codecMode == AVC_ENC) {
                if (!( SPS_RBSP<=encHeaderParam->headerType && encHeaderParam->headerType <= PPS_RBSP_MVC)) {
                    return RETCODE_INVALID_PARAM;
                }
            }
            else if (pCodecInst->codecMode == W_HEVC_ENC || pCodecInst->codecMode == W_SVAC_ENC || pCodecInst->codecMode == W_AVC_ENC) {
                if (!( CODEOPT_ENC_VPS<=encHeaderParam->headerType && encHeaderParam->headerType <= (CODEOPT_ENC_VPS|CODEOPT_ENC_SPS|CODEOPT_ENC_PPS))) {
                    return RETCODE_INVALID_PARAM;
                }
                if (pEncInfo->ringBufferEnable == 0 ) {
                    if (encHeaderParam->buf % 16 || encHeaderParam->size == 0) 
                        return RETCODE_INVALID_PARAM;
                }
                if (encHeaderParam->headerType & CODEOPT_ENC_VCL)   // ENC_PUT_VIDEO_HEADER encode only non-vcl header.
                    return RETCODE_INVALID_PARAM;

            }
            else
                return RETCODE_INVALID_PARAM;

            if (pEncInfo->ringBufferEnable == 0 ) {
                if (encHeaderParam->buf % 8 || encHeaderParam->size == 0) {
                    return RETCODE_INVALID_PARAM;
                }
            }
            if (pCodecInst->codecMode == W_HEVC_ENC || pCodecInst->codecMode == W_SVAC_ENC || pCodecInst->codecMode == W_AVC_ENC) {
                if (handle->productId == PRODUCT_ID_520 || handle->productId == PRODUCT_ID_525 || handle->productId == PRODUCT_ID_521)
                    return Wave5VpuEncGetHeader(handle, encHeaderParam);
                else
                    return RETCODE_INVALID_PARAM;
            }
            else {
                return GetEncHeader(handle, encHeaderParam);
            }
        }        
    case ENC_SET_PARAM:
        {
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            pEncInfo->openParam = *(EncOpenParam *)param;
 
            if (pCodecInst->codecMode != AVC_ENC)
                return RETCODE_INVALID_COMMAND;
 
            ret = EncParaSet(handle, SPS_RBSP);
            if (ret != RETCODE_SUCCESS)
                return ret;
 
            ret = EncParaSet(handle, PPS_RBSP);
			if (ret != RETCODE_SUCCESS)
                return ret;
            return ret;
        }
    case ENC_SET_GOP_NUMBER:
        {
            int *pGopNumber =(int *)param;
            if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC) {
                return RETCODE_INVALID_COMMAND;
            }
            if (*pGopNumber < 0)
                return RETCODE_INVALID_PARAM;
            pEncInfo->openParam.gopSize = *pGopNumber;
            SetGopNumber(handle, (Uint32 *)pGopNumber);        
        }
        break;
    case ENC_SET_INTRA_QP:
        {
            int *pIntraQp =(int *)param;
            if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC) {
                return RETCODE_INVALID_COMMAND;
            }
            if (pCodecInst->codecMode == MP4_ENC)
            {    
                if(*pIntraQp<1 || *pIntraQp>31)
                    return RETCODE_INVALID_PARAM;
            }
            if (pCodecInst->codecMode == AVC_ENC)
            {    
                if(*pIntraQp<0 || *pIntraQp>51)
                    return RETCODE_INVALID_PARAM;
            }
            SetIntraQp(handle, (Uint32 *)pIntraQp);        
        }
        break;
    case ENC_SET_BITRATE:
        {
            int *pBitrate = (int *)param;
            if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC) {
                return RETCODE_INVALID_COMMAND;
            }
            {
                if (*pBitrate < 0 || *pBitrate> 32767) {
                    return RETCODE_INVALID_PARAM;
                }
            }
            SetBitrate(handle, (Uint32 *)pBitrate);        
        }
        break;
    case ENC_SET_FRAME_RATE:
        {
            int *pFramerate = (int *)param;

            if (pCodecInst->codecMode != MP4_ENC && pCodecInst->codecMode != AVC_ENC) {
                return RETCODE_INVALID_COMMAND;
            }
            if (*pFramerate <= 0) {
                return RETCODE_INVALID_PARAM;
            }
            SetFramerate(handle, (Uint32 *)pFramerate);        
        }
        break;
    case ENC_SET_INTRA_MB_REFRESH_NUMBER:
        {
            int *pIntraRefreshNum =(int *)param;
            SetIntraRefreshNum(handle, (Uint32 *)pIntraRefreshNum); 
        }
        break;

    case ENC_SET_SLICE_INFO:
        {
            EncSliceMode *pSliceMode = (EncSliceMode *)param;
            if(pSliceMode->sliceMode<0 || pSliceMode->sliceMode>1)
            {
                return RETCODE_INVALID_PARAM;
            }
            if(pSliceMode->sliceSizeMode<0 || pSliceMode->sliceSizeMode>1)
            {
                return RETCODE_INVALID_PARAM;
            }

            SetSliceMode(handle, (EncSliceMode *)pSliceMode);
        }
        break;
    case ENC_ENABLE_HEC:
        {
            if (pCodecInst->codecMode != MP4_ENC) {
                return RETCODE_INVALID_COMMAND;
            }
            SetHecMode(handle, 1);
        }
        break;
    case ENC_DISABLE_HEC:
        {
            if (pCodecInst->codecMode != MP4_ENC) {
                return RETCODE_INVALID_COMMAND;
            }
            SetHecMode(handle, 0);
        }
        break;
    case SET_SEC_AXI:
        {
            SecAxiUse secAxiUse;

            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            secAxiUse = *(SecAxiUse *)param;
            if (handle->productId == PRODUCT_ID_520 || handle->productId == PRODUCT_ID_525 || handle->productId == PRODUCT_ID_521) {
                pEncInfo->secAxiInfo.u.wave.useEncRdoEnable = secAxiUse.u.wave.useEncRdoEnable;
                pEncInfo->secAxiInfo.u.wave.useEncLfEnable  = secAxiUse.u.wave.useEncLfEnable;
            }
            else { // coda9 or coda7q or ... 
                pEncInfo->secAxiInfo.u.coda9.useBitEnable  = secAxiUse.u.coda9.useBitEnable;
                pEncInfo->secAxiInfo.u.coda9.useIpEnable   = secAxiUse.u.coda9.useIpEnable;
                pEncInfo->secAxiInfo.u.coda9.useDbkYEnable = secAxiUse.u.coda9.useDbkYEnable;
                pEncInfo->secAxiInfo.u.coda9.useDbkCEnable = secAxiUse.u.coda9.useDbkCEnable;
                pEncInfo->secAxiInfo.u.coda9.useOvlEnable  = secAxiUse.u.coda9.useOvlEnable;
                pEncInfo->secAxiInfo.u.coda9.useBtpEnable  = secAxiUse.u.coda9.useBtpEnable;            
            }        
        }
        break;        
    case ENC_CONFIG_SUB_FRAME_SYNC: // for CODA9
        {
            EncSubFrameSyncConfig *subFrameSyncConfig;
            if (param == 0) {
                return RETCODE_INVALID_PARAM;
            }
            subFrameSyncConfig = (EncSubFrameSyncConfig *)param;
            pEncInfo->subFrameSyncConfig.subFrameSyncMode = subFrameSyncConfig->subFrameSyncMode;
            pEncInfo->subFrameSyncConfig.subFrameSyncOn = subFrameSyncConfig->subFrameSyncOn;
            pEncInfo->subFrameSyncConfig.sourceBufNumber = subFrameSyncConfig->sourceBufNumber;
            pEncInfo->subFrameSyncConfig.sourceBufIndexBase = subFrameSyncConfig->sourceBufIndexBase;
            pEncInfo->subFrameSyncConfig.curEncSourceIdx = subFrameSyncConfig->curEncSourceIdx;

        }
        break;
    case ENC_SET_SUB_FRAME_SYNC:
        EnterLock(pCodecInst->coreIdx);
        ProductVpuEncGiveCommand(pCodecInst, ENC_SET_SUB_FRAME_SYNC, param);
        LeaveLock(pCodecInst->coreIdx);
        break;
    case GET_TILEDMAP_CONFIG:
        {
            TiledMapConfig *pMapCfg = (TiledMapConfig *)param;
            if (!pMapCfg) {
                return RETCODE_INVALID_PARAM;
            }
            *pMapCfg = pEncInfo->mapCfg;
            break;
        }
    case SET_DRAM_CONFIG:
        {
            DRAMConfig *cfg = (DRAMConfig *)param;

            if (!cfg) {
                return RETCODE_INVALID_PARAM;
            }

            pEncInfo->dramCfg = *cfg;
            break;
        }
    case GET_DRAM_CONFIG:
        {
            DRAMConfig *cfg = (DRAMConfig *)param;

            if (!cfg) {
                return RETCODE_INVALID_PARAM;
            }

            *cfg = pEncInfo->dramCfg;

            break;
        }
    case ENABLE_LOGGING:
        {
            pCodecInst->loggingEnable = 1;            
        }
        break;
    case DISABLE_LOGGING:
        {
            pCodecInst->loggingEnable = 0;
        }
        break;
    case ENC_SET_PARA_CHANGE:
        {
            EncChangeParam* option = (EncChangeParam*)param;
            if (handle->productId == PRODUCT_ID_520 || handle->productId == PRODUCT_ID_525 || handle->productId == PRODUCT_ID_521)
                return Wave5VpuEncParaChange(handle, option);
            else
                return RETCODE_INVALID_PARAM;
        }
    case GET_BANDWIDTH_REPORT :
        return ProductVpuGetBandwidth(pCodecInst, (VPUBWData*)param);
    case ENC_GET_QUEUE_STATUS:
        {
            QueueStatusInfo* queueInfo = (QueueStatusInfo*)param;
            queueInfo->instanceQueueCount = pEncInfo->instanceQueueCount;
            queueInfo->totalQueueCount    = pEncInfo->totalQueueCount;
            break;
        }
    case ENC_WRPTR_SEL:
        {
            pEncInfo->encWrPtrSel = *(int *)param;
        }
        break;
    case SET_CYCLE_PER_TICK:
        {
            pEncInfo->cyclePerTick = *(Uint32 *)param;
        }
        break;
    default:
        return RETCODE_INVALID_COMMAND;
    }
    return RETCODE_SUCCESS;
}

RetCode VPU_EncAllocateFrameBuffer(EncHandle handle, FrameBufferAllocInfo info, FrameBuffer *frameBuffer)
{
    CodecInst*  pCodecInst;
    EncInfo*    pEncInfo;
    RetCode     ret;
    int         gdiIndex;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;
    pEncInfo   = &pCodecInst->CodecInfo->encInfo;

    if (!frameBuffer) {
        return RETCODE_INVALID_PARAM;
    }
    if (info.num == 0 || info.num < 0)  {
        return RETCODE_INVALID_PARAM;
    }
    if (info.stride == 0 || info.stride < 0) {
        return RETCODE_INVALID_PARAM;
    }
    if (info.height == 0 || info.height < 0) {
        return RETCODE_INVALID_PARAM;
    }

    if (info.type == FB_TYPE_PPU) {
        if (pEncInfo->numFrameBuffers == 0) {
            return RETCODE_WRONG_CALL_SEQUENCE;
        }
        pEncInfo->ppuAllocExt = frameBuffer[0].updateFbInfo;
        gdiIndex = pEncInfo->numFrameBuffers;
        ret = ProductVpuAllocateFramebuffer(pCodecInst, frameBuffer, (TiledMapType)info.mapType, (Int32)info.num,
                                            info.stride, info.height, info.format, info.cbcrInterleave, info.nv21,
                                            info.endian, (bm_device_mem_t*)&pEncInfo->vbPPU, gdiIndex, (FramebufferAllocType)info.type);
    }//this is for compile,can not guaranteed the operation
    else if (info.type == FB_TYPE_CODEC) {
        gdiIndex = 0;
        pEncInfo->frameAllocExt = frameBuffer[0].updateFbInfo;
        ret = ProductVpuAllocateFramebuffer(
            pCodecInst, frameBuffer, (TiledMapType)info.mapType, (Int32)info.num,
            info.stride, info.height, info.format, info.cbcrInterleave, FALSE, info.endian, (bm_device_mem_t*)&pEncInfo->vbFrame, gdiIndex, (FramebufferAllocType)info.type);
            //this is for compile,can not guaranteed the operation
    }
    else {
        ret = RETCODE_INVALID_PARAM;
    }

    return ret;
}

RetCode VPU_EncIssueSeqInit(EncHandle handle)
{
    CodecInst*  pCodecInst;
    RetCode     ret;
    VpuAttr*    pAttr;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS)
        return ret;

    pCodecInst = handle;

    EnterLock(pCodecInst->coreIdx);

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    if (GetPendingInst(pCodecInst->coreIdx)) {
        LeaveLock(pCodecInst->coreIdx);
        return RETCODE_FRAME_NOT_COMPLETE;
    }

    ret = ProductVpuEncInitSeq(handle);
    if (ret == RETCODE_SUCCESS) {
        SetPendingInst(pCodecInst->coreIdx, pCodecInst);
    }

    if (pAttr->supportCommandQueue == TRUE) {
        SetPendingInst(pCodecInst->coreIdx, NULL);
        LeaveLock(pCodecInst->coreIdx);
    }

    return ret;
}

RetCode VPU_EncCompleteSeqInit(EncHandle handle, EncInitialInfo * info)
{
    CodecInst*  pCodecInst;
    EncInfo*    pEncInfo;
    RetCode     ret;
    VpuAttr*    pAttr;

    ret = CheckEncInstanceValidity(handle);
    if (ret != RETCODE_SUCCESS) {
        return ret;
    }

    if (info == 0) {
        return RETCODE_INVALID_PARAM;
    }

    pCodecInst = handle;
    pEncInfo = &pCodecInst->CodecInfo->encInfo;

    pAttr = &g_VpuCoreAttributes[pCodecInst->coreIdx];

    if (pAttr->supportCommandQueue == TRUE) {
        EnterLock(pCodecInst->coreIdx);
    }
    else {
        if (pCodecInst != GetPendingInst(pCodecInst->coreIdx)) {
            SetPendingInst(pCodecInst->coreIdx, 0);
            LeaveLock(pCodecInst->coreIdx);    
            return RETCODE_WRONG_CALL_SEQUENCE;
        }
    }

    ret = ProductVpuEncGetSeqInfo(handle, info);
    if (ret == RETCODE_SUCCESS) {
        pEncInfo->initialInfoObtained = 1;
    }

    //info->rdPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamRdPtrRegAddr);
    //info->wrPtr = VpuReadReg(pCodecInst->coreIdx, pEncInfo->streamWrPtrRegAddr);

    //pEncInfo->prevFrameEndPos = info->rdPtr;

    pEncInfo->initialInfo = *info;

    SetPendingInst(pCodecInst->coreIdx, NULL);

    LeaveLock(pCodecInst->coreIdx);

    return ret;
}

PhysicalAddress VPU_MapToAddr40Bit(int coreIdx, unsigned int Addr)
{
    PhysicalAddress RealAddr = (PhysicalAddress)vdi_get_ddr_map(coreIdx);

    RealAddr = (RealAddr << 32) | (PhysicalAddress)Addr;

    return RealAddr;
}

int VPU_GetFirmwareStatus(Uint32 coreIdx)
{
    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);

    return vdi_get_firmware_status(coreIdx);
}

int VPU_GetInNum(Uint32 coreIdx, Uint32 instIdx)
{
    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);
    if(instIdx < 0 || instIdx >= MAX_NUM_INSTANCE)
        return -1;
    return vdi_get_in_num(coreIdx, instIdx);
}

int VPU_GetOutNum(Uint32 coreIdx, Uint32 instIdx)
{
    INVALID_CORE_INDEX_RETURN_ERROR(coreIdx);
    if(instIdx < 0 || instIdx >= MAX_NUM_INSTANCE)
        return -1;
    return vdi_get_out_num(coreIdx, instIdx);
}
void VPU_PrintW5AllReg(int coreIdx)
{
    int i, size;
    struct interface_reg{
        char* name;
        int rw;    //00 - invalid  01 - WO  10 - RO 11 - RW
    } addr_array[] =
    {
        {"power on config      ", 3},
        {"VPU current PC       ", 3},
        {"VPU current LR       ", 3},
        {"VPU debug  step mask ", 3},
        {"VPU debug  control   ", 3},
        {"VPU debug  index     ", 3},
        {"VPU debug  write data", 3},
        {"VPU debug  read  data", 3},
        {"VPU FIO control addr ", 3},
        {"VPU FIO control data ", 3},
        {"", 0},
        {"", 0},
        {"VPU intr reason      ", 3},
        {"VPU Intr reason clear", 1},    //1
        {"VPU host Intr request", 1},    // 1
        {"VPU Intr clear       ", 1},    // 1
        {"VPU host intr clear  ", 2},
        {"VPU VPU intr status  ", 2},
        {"VPU Intr enable      ", 3},
        {"VPU Intr reason      ", 3},
        {"VPU Reset Req        ", 3},
        {"VPU Reset status     ", 3},
        {"VPU cpu restart req  ", 3},
        {"VPU clock control    ", 3},
        {"VPU remap control    ", 3},
        {"VPU remap vir_addr   ", 3},
        {"VPU remap phy_addr   ", 3},
        {"VPU start request    ", 3},
        {"VPU busy status      ", 3},
        {"VPU halt status      ", 3},
        {"VPU vcpu status      ", 3},
        {"VPU_PRESCAN_STATUS   ", 3},
        {"RET_FIO_STATUS       ", 3},
        {"", 0},
        {"", 0},
        {"", 0},
        {"HW product name      ", 3},
        {"HW product version   ", 3},
        {"Config Information #0", 3},
        {"Config Information #1", 3},
        {"Standard Definition  ", 3},
        {"Configuration Date   ", 3},
        {"revision of H/W conf ", 3},
        {"define of H/W conf   ", 3},
        {"", 3},
        {"", 3},
        {"", 3},
        {"", 3},
        {"", 3},
        {"", 0},
        {"", 0},
        {"", 0},
        {"", 0},
        {"", 3},
        {"", 0},
        {"", 0},
        {"", 0},
        {"", 0},
        {"", 0},
        {"", 0},
        {"Debug register #0    ", 0},
        {"Debug register #1    ", 0},
        {"Debug register #2    ", 0},
        {"Debug register #3    ", 0},    // 0xFC
        {"Command              ", 3},
        {"Command option       ", 3},
        {"Run Command status   ", 3},
        {"command fail reason  ", 3},
        {"Command parameter 0  ", 3},
        {"Command parameter 1  ", 3},
        {"Command parameter 2  ", 3},
        {"Command parameter 3  ", 3},
        {"Command parameter 4  ", 3},
        {"Command parameter 5  ", 3},
        {"Command parameter 6  ", 3},
        {"Command parameter 7  ", 3},
        {"Command parameter 8  ", 3},
        {"Command parameter 9  ", 3},
        {"Command parameter 10 ", 3},
        {"Command parameter 11 ", 3},
        {"Command parameter 12 ", 3},
        {"Command parameter 13 ", 3},
        {"Command parameter 14 ", 3},
        {"Command parameter 15 ", 3},
        {"Command parameter 16 ", 3},
        {"Command parameter 17 ", 3},
        {"Command parameter 18 ", 3},
        {"Command parameter 19 ", 3},
        {"Command parameter 20 ", 3},
        {"Command parameter 21 ", 3},
        {"Command parameter 22 ", 3},
        {"Command parameter 23 ", 3},
        {"Command parameter 24 ", 3},
        {"Command parameter 25 ", 3},
        {"Command parameter 26 ", 3},
        {"Command parameter 27 ", 3},
        {"Command parameter 28 ", 3},
        {"Command parameter 29 ", 3},
        {"Command parameter 30 ", 3},
        {"Command parameter 31 ", 3},
        {"Command parameter 32 ", 3},
        {"Command parameter 33 ", 3},
        {"Command parameter 34 ", 3},
        {"Command parameter 35 ", 3},
        {"Command parameter 36 ", 3},
        {"Command parameter 37 ", 3},
        {"Command parameter 38 ", 3},
        {"Command parameter 39 ", 3},
        {"Config Info V-CORE0  ", 3},
        {"Config Info V-CORE1  ", 3},
        {"Config Info V-CORE2  ", 3},
        {"Config Info V-CORE3  ", 3},
        {"Command parameter 45 ", 3},
        {"Command parameter 46 ", 3},
        {"Command parameter 47 ", 3},
        {"Command parameter 48 ", 3},
        {"Number of VCOREs     ", 3},
        {"seek cycle           ", 3},
        {"parsing cycle        ", 3},
        {"decoding cycle       ", 3},
        {"frame cycle          ", 3},
        {"Bs buffer empty flag ", 3},
        {"Queued cmd done flag ", 3},
        {"Instan info in seek  ", 3},
        {"Instan info in parse ", 3},
        {"Instan info in decode", 3},
        {"", 3},
        {"Instan info dec done ", 3},    // 0x1FC
        {"", 3},
    };

    return;

    printf("print interface register\n");
    printf("===========================\n");

    size = sizeof(addr_array) / sizeof(addr_array[0]);

    for (i = 0; i < size; i++)
    {
        if (addr_array[i].rw & 0x2)
            printf("%s\t\t\t(0x%08x): 0x%08x\n", addr_array[i].name, i * 4, VpuReadReg(coreIdx, i*4));
    }


    printf("===========================\n");

    return;
}



 void bmvpu_dec_load_bmlib_handle(int coreIdx){
    if (coreIdx > MAX_NUM_VPU_CORE)
    {
         VLOG(INFO,"soc_idx excess MAX_SOC_NUM!\n");
        exit(0);
    }
    int soc_idx= coreIdx/MAX_NUM_VPU_CORE_CHIP;
    bm_handle_lock();
    if (g_bm_handle[soc_idx].bm_handle)
    {
        g_bm_handle[soc_idx].count += 1;
        bm_handle_unlock();
        return ;
    }

    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, soc_idx);
    if (ret != BM_SUCCESS) {
      VLOG(INFO,"Create Bm Handle Failed\n");
        bm_handle_unlock();
      exit(0);
    }
    g_bm_handle[soc_idx].count = 1;
    g_bm_handle[soc_idx].bm_handle = handle;
    bm_handle_unlock();
    return ;
}

/*
 * If a bm_handle_t on this soc already exists, then the bm_handle_t's reference count -1.
 * After that, if bm_handle_t's reference count is 0, free it(bm_dev_free),
 * otherwise do nothing.
 * This function is only be called by bmvpu_enc_unload().
 */
void bmvpu_dec_unload_bmlib_handle(int coreIdx){
    if (coreIdx > MAX_NUM_VPU_CORE)
    {
      VLOG(ERR,"soc_idx excess MAX_SOC_NUM!\n");
      exit(0);
    }
    int soc_idx= coreIdx/MAX_NUM_VPU_CORE_CHIP;
    if (g_bm_handle[soc_idx].bm_handle)
    {
        bm_handle_lock();
        if (g_bm_handle[soc_idx].count <= 1)
        {
            bm_dev_free(g_bm_handle[soc_idx].bm_handle);
            g_bm_handle[soc_idx].count = 0;
            g_bm_handle[soc_idx].bm_handle = 0;
            VLOG(INFO,"Free bm_handle for decode on soc %d \n", soc_idx);
        }
        else
        {
            g_bm_handle[soc_idx].count -= 1;
            VLOG(INFO,"The bm_handle for decode on soc is used by %d users \n", g_bm_handle[soc_idx].count);
        }
        bm_handle_unlock();
    }
    else
        VLOG(ERR,"Bm_handle for encode on soc %d not exist \n", soc_idx);
}



bm_handle_t bmvpu_dec_get_bmlib_handle(int coreIdx)
{
    bm_handle_t handle = 0;
    int soc_idx= coreIdx/MAX_NUM_VPU_CORE_CHIP;
    if (coreIdx > MAX_NUM_VPU_CORE)
    {
       VLOG(ERR,"soc_idx excess MAX_SOC_NUM!\n");
        exit(0);
    }
    bm_handle_lock();
    if (g_bm_handle[soc_idx].bm_handle)
    {
        handle = g_bm_handle[soc_idx].bm_handle;
        bm_handle_unlock();
        //VLOG(ERR,"core_idx=%d,bm_handle[soc_idx].bm_handle=%p\n",pcie_id,handle);
        return handle;
    }
    else
    {
        bm_handle_unlock();
        VLOG(ERR,"There is not bmlib_handle on soc %d, This function should be called after bmvpu_dec_load()! \n",soc_idx);
        return handle;
    }
}



int bmvpu_malloc_device_byte_heap(bm_handle_t bm_handle, bm_device_mem_t *pmem, unsigned int size, int heap_id_mask, int high_bit_first)
{
    int ret = 0;
    int i = 0;
    unsigned int heap_num = 0;
    ret = bm_get_gmem_total_heap_num(bm_handle, &heap_num);
    if (ret != 0)
    {
        VLOG(ERR,"bmvpu_malloc_device_byte_heap failed!\n");
        return -1;
    }

    int available_heap_mask = 0;
    for (i=0; i<heap_num; i++){
        available_heap_mask = available_heap_mask | (0x1 << i);
    }

    int enable_heap_mask = available_heap_mask & heap_id_mask;
    if (enable_heap_mask == 0x0)
    {
        VLOG(ERR,"bmvpu_malloc_device_byte_heap failed!\n");
        return -1;
    }
    if (high_bit_first)
    {
        for (i=(heap_num-1); i>=0; i--)
        {
            if ((enable_heap_mask & (0x1<<i)))
            {
                ret = bm_malloc_device_byte_heap(bm_handle, pmem, i, size);
                if (ret != 0)
                {
                    VLOG(ERR,"bmvpu_malloc_device_byte_heap failed!\n");
                }
                return ret;
            }
        }
    }
    else
    {
        for (i=0; i<heap_num; i++)
        {
            if ((enable_heap_mask & (0x1<<i)))
            {
                ret = bm_malloc_device_byte_heap(bm_handle, pmem, i, size);
                if (ret != 0)
                {
                    VLOG(ERR,"bmvpu_malloc_device_byte_heap failed!\n");
                }
                return ret;
            }
        }
    }
    return ret;
}

