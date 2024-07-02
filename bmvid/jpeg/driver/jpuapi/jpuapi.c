
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "jpuapifunc.h"
#include "jpuapi.h"

#include "../include/jpulog.h"

#ifdef  _WIN32
#include  < MMSystem.h >
#pragma comment(lib, "winmm.lib")

HANDLE g_vpp_fd[64] = {0};
#else
int  g_vpp_fd[64] = {-1};
#endif

#ifdef BM_PCIE_MODE
#ifdef  _WIN32
HANDLE s_jpu_fd[64];
#else
extern int  s_jpu_fd[64];
#endif
#endif
void JpuDecFlush(JpgDecInfo *pJpgDecInfo);
void JpuEncFlush(JpgEncInfo *pJpgEncInfo);

int JPU_IsBusy()
{
    Uint32 val;
    val = JpuReadReg(MJPEG_PIC_STATUS_REG);

    if ((val & (1<<INT_JPU_DONE)) ||
        (val & (1<<INT_JPU_ERROR)))
        return 0;

    return 1;
}
void JPU_ClrStatus(Uint32 val)
{
    if (val != 0)
        JpuWriteReg(MJPEG_PIC_STATUS_REG, val);
}

Uint32 JPU_GetStatus()
{
    return JpuReadReg(MJPEG_PIC_STATUS_REG);
}


Uint32 JPU_IsInit()
{
#if 0
    jpu_instance_pool_t *pjip;

    pjip = (jpu_instance_pool_t *)jdi_get_instance_pool();

    if (!pjip)
        return 0;
#endif
    return 1;
}

Uint32 JPU_WaitInterrupt(int device_index, Uint32 coreIdx,int timeout)
{
    Uint32 reason = 0;

    reason = jdi_wait_interrupt(device_index, coreIdx, timeout);
    JpgSetClockGate(1);

#ifndef  _WIN32
    if (reason != (Uint32)-1)
        reason = JpuReadReg(MJPEG_PIC_STATUS_REG);
#endif
    JpgSetClockGate(0);

    return reason;
}

#if 0
int JPU_GetOpenInstanceNum()
{
    jpu_instance_pool_t *pjip;

    pjip = (jpu_instance_pool_t *)jdi_get_instance_pool();
    if (!pjip)
        return -1;

    return pjip->jpu_instance_num;
}
#endif

JpgRet JPU_Init(Uint32 device_index)
{
#if defined(FLOCK_PROCESS)
    void * r = NULL;
#endif
    int ret;

#if defined(FLOCK_PROCESS)
    if(0 != open_flock())
    {
        JLOG(ERR, "can't create flock file!");
        return JPG_RET_FAILURE;
    }

    r = JpuProcessEnterLock();
    if( r == NULL) {
        close_flock();
        JLOG(ERR, "can't acquire lock");
        return JPG_RET_FAILURE;
    }
#endif // FLOCK_PROCESS

    ret = jdi_init(device_index);
    if (ret < 0) {
#if defined(FLOCK_PROCESS)
        JpuProcessLeaveLock(r);
        close_flock();
#endif
        JLOG(ERR, "Failed to call jdi_init()!");
        return JPG_RET_FAILURE;
    }

#ifdef FLOCK_PROCESS
    JpuProcessLeaveLock(r);
    close_flock();
#endif

#ifdef DUMP_JPU_REGS
    JPU_Print_Reg_Status(1);
#endif
    return JPG_RET_SUCCESS;
}

JpgRet VPP_Init(Uint32 device_index)
{
#ifdef BM_PCIE_MODE

#ifdef  _WIN32
    g_vpp_fd[device_index] = s_jpu_fd[device_index];

    if(g_vpp_fd[device_index] == 0)
    {
        JLOG(ERR, "[JDI]g_vpp_fd open failed	ret=0x%x\n", (int)g_vpp_fd[device_index]);
        return JPG_RET_FAILURE;
    }
#else
    char dev_name[32] = {0};
    g_vpp_fd[device_index] = s_jpu_fd[device_index];

    if(g_vpp_fd[device_index] <= 0)
    {
        sprintf(dev_name, "/dev/bm-sophon%d", device_index);
        g_vpp_fd[device_index] = open(dev_name, O_RDWR);
        if (g_vpp_fd[device_index] < 0)
        {
            JLOG(ERR, "[JDI]open %s failed  ret=0x%x\n", dev_name, (int)g_vpp_fd[device_index]);
            return JPG_RET_FAILURE;
        }
    }
#endif

#else
    device_index = 0;
    g_vpp_fd[device_index] = open("/dev/bm-vpp", O_RDWR);
    if (g_vpp_fd[device_index] < 0)
    {
        JLOG(ERR, "[JDI] open /dev/bm-vpp failed ret=0x%x\n", (int)g_vpp_fd[device_index]);
        return JPG_RET_FAILURE;
    }
#endif
    printf("vpp fd = %d\n", g_vpp_fd[device_index]);
    return JPG_RET_SUCCESS;
}
void JPU_DeInit(Uint32 device_index)
{
#if defined(FLOCK_PROCESS)
    void * r;
    if(0 != open_flock())
    {
        JLOG(ERR, "can't create flock file!");
        jdi_release();
        return ;
    }
    r = JpuProcessEnterLock();
    if (r == NULL) {
        JLOG(ERR, "can't acquire lock");
        jdi_release();
        close_flock();
        return ;
    }
#endif

    jdi_release(device_index);

#ifdef FLOCK_PROCESS
    JpuProcessLeaveLock(r);
    close_flock();
#endif
}

JpgRet JPU_GetVersionInfo(Uint32 *versionInfo)
{
    if (JPU_IsInit() == 0) {
        return JPG_RET_NOT_INITIALIZED;
    }

    *versionInfo = API_VERSION;

    return JPG_RET_SUCCESS;
}

/**
 * Initialize JPU encoder/decoder status.
 * Initialize the internal state and control registers of JPU */
static JpgRet JPU_InitCodecStatus()
{
    Uint32 val;
    JpuWriteReg(MJPEG_PIC_START_REG, (1<<JPG_START_INIT));
    do {
        val = JpuReadReg(MJPEG_PIC_START_REG);
    } while ((val&(1<<JPG_START_INIT)) == (1<<JPG_START_INIT));

    return JPG_RET_SUCCESS;
}

JpgRet JPU_SWReset()
{
    PhysicalAddress streamBufStartAddr;
    PhysicalAddress streamBufEndAddr;
    PhysicalAddress streamWrPtr;
    PhysicalAddress streamRdPtr;

    JpgEnterLock(); // TODO this should be a process lock

    streamBufStartAddr = JpuReadReg(MJPEG_BBC_BAS_ADDR_REG);
    streamBufEndAddr   = JpuReadReg(MJPEG_BBC_END_ADDR_REG);
    streamWrPtr        = JpuReadReg(MJPEG_BBC_RD_PTR_REG);
    streamRdPtr        = JpuReadReg(MJPEG_BBC_WR_PTR_REG);

    JPU_InitCodecStatus();

    JpuWriteReg(MJPEG_BBC_BAS_ADDR_REG, streamBufStartAddr);
    JpuWriteReg(MJPEG_BBC_END_ADDR_REG, streamBufEndAddr);
    JpuWriteReg(MJPEG_BBC_RD_PTR_REG, streamRdPtr);
    JpuWriteReg(MJPEG_BBC_WR_PTR_REG, streamWrPtr);

    JpgLeaveLock();

    return JPG_RET_SUCCESS;
}

JpgRet JPU_HWReset()
{
    if (jdi_hw_reset() < 0 )
        return JPG_RET_FAILURE;

    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecOpen(JpgDecHandle * pHandle, JpgDecOpenParam * pop)
{
    JpgDecInfo * pDecInfo;
    JpgRet ret;

    ret = CheckJpgDecOpenParam(pop);
    if (ret != JPG_RET_SUCCESS) {
        return ret;
    }

    JpgEnterLock(); // TODO this should be a process lock
    ((JpgInst *)(*pHandle))->device_index = pop->deviceIndex;
    pDecInfo = &((JpgInst *)(*pHandle))->JpgInfo.decInfo;

    pDecInfo->streamWrPtr = pop->bitstreamBuffer;
    pDecInfo->streamRdPtr = pop->bitstreamBuffer;

    pDecInfo->streamBufStartAddr = pop->bitstreamBuffer;
    pDecInfo->streamBufSize = pop->bitstreamBufferSize;
    pDecInfo->streamBufEndAddr = pop->bitstreamBuffer + pop->bitstreamBufferSize;

    pDecInfo->streamEndian = pop->streamEndian;
    pDecInfo->frameEndian  = pop->frameEndian;
    pDecInfo->chromaInterleave = pop->chromaInterleave;
    pDecInfo->packedFormat = pop->packedFormat;
    pDecInfo->roiEnable  = pop->roiEnable;
    pDecInfo->roiWidth   = pop->roiWidth;
    pDecInfo->roiHeight  = pop->roiHeight;
    pDecInfo->roiOffsetX = pop->roiOffsetX;
    pDecInfo->roiOffsetY = pop->roiOffsetY;
    pDecInfo->rotationEnable  = pop->rotationEnable;
    pDecInfo->rotationAngle   = pop->rotationAngle;
    pDecInfo->mirrorEnable    = pop->mirrorEnable;
    pDecInfo->mirrorDirection = pop->mirrorDirection;

    JpgLeaveLock();

    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecClose(JpgDecHandle handle)
{
    JpgInst * pJpgInst = handle;
    JpgRet ret = JPG_RET_SUCCESS;

    JpgEnterLock(); // TODO this should be a process lock
    if(FreeJpgInstance(pJpgInst) < 0)
        ret = JPG_RET_FAILURE;
    JpgLeaveLock();

    return ret;
}

JpgRet JPU_DecGetInitialInfo(JpgDecHandle handle, JpgDecInitialInfo * info)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;

    if (info == 0) {
        return JPG_RET_INVALID_PARAM;
    }
    pDecInfo = &pJpgInst->JpgInfo.decInfo;
    if (JpegDecodeHeader(pDecInfo) <= 0) {
        JLOG(ERR, "Failed to call JpegDecodeHeader()!");
        return JPG_RET_FAILURE;
    }

    if(pDecInfo->picWidth < 16 || pDecInfo->picHeight < 16)
    {
        JLOG(ERR, "The picture is too small or damaged bitstream, width=%d,height=%d\n",pDecInfo->picWidth, pDecInfo->picHeight);
        return JPG_RET_INVALID_PARAM;
    }
    info->picWidth = pDecInfo->picWidth;
    info->picHeight = pDecInfo->picHeight;
    info->minFrameBufferCount = 1;
    info->sourceFormat = pDecInfo->format;
    info->ecsPtr = pDecInfo->ecsPtr;

    pDecInfo->initialInfoObtained = 1;
    pDecInfo->minFrameBufferNum = 1;

#ifdef MJPEG_ERROR_CONCEAL
    pDecInfo->curRstIdx = 0;
    pDecInfo->nextRstIdx = -1;
#endif

    if ((pDecInfo->packedFormat == PACKED_FORMAT_444) &&
        (pDecInfo->format != FORMAT_444)) {
        JLOG(ERR, "Invalid packed format 444!");
        return JPG_RET_INVALID_PARAM;
    }

    if (pDecInfo->roiEnable) {
        pDecInfo->roiMcuWidth = pDecInfo->roiWidth/pDecInfo->mcuWidth;
        pDecInfo->roiMcuHeight = pDecInfo->roiHeight/pDecInfo->mcuHeight;
        pDecInfo->roiMcuOffsetX = pDecInfo->roiOffsetX/pDecInfo->mcuWidth;
        pDecInfo->roiMcuOffsetY = pDecInfo->roiOffsetY/pDecInfo->mcuHeight;

        if ((pDecInfo->roiOffsetX > pDecInfo->alignedWidth) ||
            (pDecInfo->roiOffsetY > pDecInfo->alignedHeight) ||
            (pDecInfo->roiOffsetX + pDecInfo->roiWidth > pDecInfo->alignedWidth) ||
            (pDecInfo->roiOffsetY + pDecInfo->roiHeight > pDecInfo->alignedHeight))
            return JPG_RET_INVALID_PARAM;

        if (((pDecInfo->roiOffsetX + pDecInfo->roiWidth) < pDecInfo->mcuWidth) ||
            ((pDecInfo->roiOffsetY + pDecInfo->roiHeight) < pDecInfo->mcuHeight))
            return JPG_RET_INVALID_PARAM;

        info->roiFrameWidth = pDecInfo->roiMcuWidth*pDecInfo->mcuWidth;
        info->roiFrameHeight = pDecInfo->roiMcuHeight*pDecInfo->mcuHeight;
        info->roiFrameOffsetX = pDecInfo->roiMcuOffsetX*pDecInfo->mcuWidth;
        info->roiFrameOffsetY = pDecInfo->roiMcuOffsetY*pDecInfo->mcuHeight;
        info->roiMCUSize = pDecInfo->mcuWidth;
    }
    info->colorComponents = pDecInfo->compNum;

    return JPG_RET_SUCCESS;
}


JpgRet JPU_DecRegisterFrameBuffer(JpgDecHandle handle, FrameBuffer * bufArray, int num, int stride)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;

    pDecInfo = &pJpgInst->JpgInfo.decInfo;

    if (!pDecInfo->initialInfoObtained) {
        return JPG_RET_WRONG_CALL_SEQUENCE;
    }

    if (bufArray == 0) {
        return JPG_RET_INVALID_FRAME_BUFFER;
    }

    if (num < pDecInfo->minFrameBufferNum) {
        return JPG_RET_INSUFFICIENT_FRAME_BUFFERS;
    }
    if (pDecInfo->usePartial && pDecInfo->bufNum == 0) {
        return JPG_RET_INSUFFICIENT_FRAME_BUFFERS;
    }
    if (pDecInfo->usePartial && num < pDecInfo->bufNum) {
        return JPG_RET_INSUFFICIENT_FRAME_BUFFERS;
    }

    if(!pDecInfo->roiEnable)
    {
        if (stride < pDecInfo->picWidth>>3 || stride % 8 != 0)
            return JPG_RET_INVALID_STRIDE;
    }

    pDecInfo->frameBufPool = bufArray;
    pDecInfo->numFrameBuffers = num;
    pDecInfo->stride = stride;

    return JPG_RET_SUCCESS;
}

#if !defined(LIBBMJPULITE)
JpgRet JPU_DecGetBitstreamBuffer(JpgDecHandle handle,
                                 PhysicalAddress * prdPtr,
                                 PhysicalAddress * pwrPtr,
                                 int * size)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;
    JpgRet ret;
    PhysicalAddress rdPtr;
    PhysicalAddress wrPtr;
    int room;

    ret = CheckJpgInstValidity(handle);
    if (ret != JPG_RET_SUCCESS)
        return ret;

    if (prdPtr == 0 || pwrPtr == 0 || size == 0) {
        return JPG_RET_INVALID_PARAM;
    }

    pDecInfo = &pJpgInst->JpgInfo.decInfo;

    if (GetJpgPendingInst() == pJpgInst) {
        rdPtr = JpuReadReg(MJPEG_BBC_RD_PTR_REG);
        rdPtr = jdi_get_memory_addr_high(rdPtr);
    }
    else {
        rdPtr = pDecInfo->streamRdPtr;
    }

    wrPtr = pDecInfo->streamWrPtr;

    if (wrPtr == pDecInfo->streamBufStartAddr) {
        if (pDecInfo->frameOffset == 0)
            room = (pDecInfo->streamBufEndAddr-pDecInfo->streamBufStartAddr);
        else
            room = (pDecInfo->frameOffset);
    }
    else {
        room = (pDecInfo->streamBufEndAddr - wrPtr);
    }

    room = ((room>>9)<<9); // multiple of 512

    *prdPtr = rdPtr;
    *pwrPtr = wrPtr;

    *size = room;

    return JPG_RET_SUCCESS;
}
#endif

JpgRet JPU_DecUpdateBitstreamBuffer(JpgDecHandle handle, int size)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;
    PhysicalAddress wrPtr;
   // PhysicalAddress rdPtr;
    int     val = 0;

    pDecInfo = &pJpgInst->JpgInfo.decInfo;
    wrPtr = pDecInfo->streamWrPtr;

    if (size == 0)
    {
        val = (wrPtr-pDecInfo->streamBufStartAddr) / 256;
        if ((wrPtr-pDecInfo->streamBufStartAddr) % 256)
            val = val + 1;
       // if (GetJpgPendingInst() == pJpgInst)
            JpuWriteReg(MJPEG_BBC_STRM_CTRL_REG, (1 << 31 | val));
        pDecInfo->streamEndflag = 1;
        return JPG_RET_SUCCESS;
    }

#if 0
    JpgSetClockGate(1);

    wrPtr = pDecInfo->streamWrPtr;
    wrPtr += size;

    if (wrPtr == pDecInfo->streamBufEndAddr) {
        wrPtr = pDecInfo->streamBufStartAddr;
    }

    pDecInfo->streamWrPtr = wrPtr;

    if (GetJpgPendingInst() == pJpgInst) {
        rdPtr = JpuReadReg(MJPEG_BBC_RD_PTR_REG);
        rdPtr = jdi_get_memory_addr_high(rdPtr);
        if (rdPtr == pDecInfo->streamBufEndAddr) {
            JpuWriteReg(MJPEG_BBC_CUR_POS_REG, 0);
            JpuWriteReg(MJPEG_GBU_TT_CNT_REG, 0);
            JpuWriteReg(MJPEG_GBU_TT_CNT_REG+4, 0);
        }

        JpuWriteReg(MJPEG_BBC_WR_PTR_REG, wrPtr);
        if (wrPtr == pDecInfo->streamBufStartAddr)
            JpuWriteReg(MJPEG_BBC_END_ADDR_REG, pDecInfo->streamBufEndAddr);
        else
            JpuWriteReg(MJPEG_BBC_END_ADDR_REG, wrPtr);
    }
    else {
        rdPtr = pDecInfo->streamRdPtr;
    }

    pDecInfo->streamRdPtr = rdPtr;

    JpgSetClockGate(0);
#endif
    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecIssueStop(JpgDecHandle handle)
{
#if 0
    JpgInst * pJpgInst = handle;
    JpgRet ret;
    ret = CheckJpgInstValidity(handle);
    if (ret != JPG_RET_SUCCESS)
        return ret;

    if (pJpgInst != GetJpgPendingInst()) {
        return JPG_RET_WRONG_CALL_SEQUENCE;
    }
#endif
    JpgSetClockGate(1);
    JpuWriteReg(MJPEG_PIC_START_REG, 1<< JPG_START_STOP);
    JpgSetClockGate(0);
    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecCompleteStop(JpgDecHandle handle)
{
    Uint32 val;

    JpgSetClockGate(1);
    val = JpuReadReg(MJPEG_PIC_STATUS_REG);

    if (val & (1<<INT_JPU_BIT_BUF_STOP)) {
        SetJpgPendingInst(0);
        JpgSetClockGate(0);
    }
    else {
        JpgSetClockGate(0);
        return JPG_RET_WRONG_CALL_SEQUENCE;
    }
    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecSetResolutionInfo(DecHandle handle, int width, int height)
{
    JpgInst*    pJpgInst;
    JpgDecInfo* pDecInfo;
    JpgRet      ret;

    ret = CheckJpgInstValidity(handle);
    if (ret != JPG_RET_SUCCESS)
        return ret;

    pJpgInst = (JpgInst*)handle;
    pDecInfo = &pJpgInst->JpgInfo.decInfo;

    JpgEnterLock();

    if (GetJpgPendingInst()) {
        JpgLeaveLock();
        return JPG_RET_FRAME_NOT_COMPLETE;
    }

    pDecInfo->picWidth = width;
    pDecInfo->picHeight = height;

    JpgLeaveLock();

    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecSetRdPtrEx(JpgDecHandle handle, PhysicalAddress addr, BOOL updateWrPtr)
{
    JpgInst*    pJpgInst;
    JpgDecInfo* pDecInfo;
    JpgRet      ret;

    ret = CheckJpgInstValidity(handle);
    if (ret != JPG_RET_SUCCESS)
        return ret;

    pJpgInst = (JpgInst*)handle;
    pDecInfo = &pJpgInst->JpgInfo.decInfo;

    JpgEnterLock();

    if (GetJpgPendingInst()) {
        JpgLeaveLock();
        return JPG_RET_FRAME_NOT_COMPLETE;
    }
    pDecInfo->streamRdPtr = addr;
    pDecInfo->streamBufStartAddr = addr;
    if (updateWrPtr)
        pDecInfo->streamWrPtr = addr;

    pDecInfo->frameOffset = 0;
    pDecInfo->consumeByte = 0;

    JpuWriteReg(MJPEG_BBC_RD_PTR_REG, pDecInfo->streamRdPtr);

    JpgLeaveLock();

    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecSetRdPtr(JpgDecHandle handle, PhysicalAddress addr, int updateWrPtr)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;

    pDecInfo = &pJpgInst->JpgInfo.decInfo;

    JpgEnterLock(); // TODO this should be a process lock

    if (GetJpgPendingInst()) {
        JpgLeaveLock();
        JLOG(ERR, "Frame is not complete!");
        return JPG_RET_FRAME_NOT_COMPLETE;
    }

    pDecInfo->streamRdPtr = addr;
    if (updateWrPtr)
        pDecInfo->streamWrPtr = addr;

    pDecInfo->frameOffset = addr - pDecInfo->streamBufStartAddr;
    pDecInfo->consumeByte = 0;

    JpuWriteReg(MJPEG_BBC_RD_PTR_REG, pDecInfo->streamRdPtr);

    JpgLeaveLock();

    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecSetBsPtr(JpgDecHandle pHandle, uint8_t *data, int data_size)
{
    JpgDecInfo *pDecInfo;

    pDecInfo = &((JpgInst *)pHandle)->JpgInfo.decInfo;
    pDecInfo->pBitStream = data;
    pDecInfo->BitStreamByteSize = data_size;

    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecStartOneFrame(JpgDecHandle handle, JpgDecParam *param)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;
    Uint32 rotMir;
    Uint32 val;
    int i, ret = JPG_RET_SUCCESS;

    pDecInfo = &pJpgInst->JpgInfo.decInfo;

    if (pDecInfo->frameBufPool == 0) { // This means frame buffers have not been registered.
        JLOG(ERR, "Wrong call sequence!");
        return JPG_RET_WRONG_CALL_SEQUENCE;
    }

    rotMir = 0;
    if (pDecInfo->rotationEnable) {
        rotMir |= 0x10; // Enable rotator
        switch (pDecInfo->rotationAngle) {
        case 0:
            rotMir |= 0x0;
            break;
        case 90:
            rotMir |= 0x1;
            break;
        case 180:
            rotMir |= 0x2;
            break;
        case 270:
            rotMir |= 0x3;
            break;
        }
    }

    if (pDecInfo->mirrorEnable) {
        rotMir |= 0x10; // Enable rotator
        switch (pDecInfo->mirrorDirection) {
        case MIRDIR_NONE :
            rotMir |= 0x0;
            break;
        case MIRDIR_VER :
            rotMir |= 0x4;
            break;
        case MIRDIR_HOR :
            rotMir |= 0x8;
            break;
        case MIRDIR_HOR_VER :
            rotMir |= 0xc;
            break;
        }
    }

    JpgEnterLock(); // TODO this should be a process lock

    ret = GetJpgCoreIndex(pJpgInst, pDecInfo->streamBufStartAddr, pDecInfo->frameBufPool[0].bufY);
    if (ret < 0) {
        ret = JPG_RET_FAILURE;
        goto fail;
    }

    JpuDecFlush(pDecInfo);
    JpuWriteReg(MJPEG_BBC_BAS_ADDR_REG, pDecInfo->streamBufStartAddr);
    JpuWriteReg(MJPEG_BBC_END_ADDR_REG, pDecInfo->streamBufEndAddr);

#ifdef MJPEG_ERROR_CONCEAL
    // Error Concealment
    if(pDecInfo->errInfo.bError)
    {
        // error conceal main function
        val = JpegDecodeConcealError(pDecInfo);
        if(val == -1)
        {
            // stream buffer wrap around in error cases.
            pDecInfo->frameOffset = 0;
            pDecInfo->nextOffset = 0;

            // end of stream
            if (pDecInfo->streamEndflag == 1) {
                pDecInfo->frameOffset = -1;
                SetJpgPendingInst(pJpgInst);
                JLOG(ERR, "End of stream!");
                return JPG_RET_EOS;
            }

            // request data
            JLOG(ERR, "Bitstream is empty!");
            ret = JPG_RET_BIT_EMPTY;
            goto fail;
        }

        // init GBU
        JpuWriteReg(MJPEG_GBU_TT_CNT_REG, 0);
        JpuWriteReg(MJPEG_GBU_TT_CNT_REG+4, 0);

        JpuWriteReg(MJPEG_PIC_CTRL_REG, pDecInfo->huffAcIdx << 10 | pDecInfo->huffDcIdx << 7 | pDecInfo->userHuffTab << 6 | (JPU_CHECK_WRITE_RESPONSE_BVALID_SIGNAL << 2) | 0);
        JpuWriteReg(MJPEG_MCU_INFO_REG, pDecInfo->mcuBlockNum << 16 | pDecInfo->compNum << 12 | pDecInfo->compInfo[0] << 8 | pDecInfo->compInfo[1] << 4 | pDecInfo->compInfo[2]);
        JpuWriteReg(MJPEG_RST_INTVAL_REG, pDecInfo->rstIntval);

        JpgDecGramSetup(pDecInfo);

        JpuWriteReg(MJPEG_DPCM_DIFF_Y_REG, 0);
        JpuWriteReg(MJPEG_DPCM_DIFF_CB_REG, 0);
        JpuWriteReg(MJPEG_DPCM_DIFF_CR_REG, 0);

        JpuWriteReg(MJPEG_GBU_FF_RPTR_REG, pDecInfo->bitPtr);
        JpuWriteReg(MJPEG_GBU_CTRL_REG, 3);

        val = (pDecInfo->setPosX << 16) | (pDecInfo->setPosY);
        JpuWriteReg(MJPEG_PIC_SETMB_REG, val);
        JpuWriteReg(MJPEG_PIC_START_REG, (1<<JPG_START_PIC));

        SetJpgPendingInst(pJpgInst);
        return JPG_RET_SUCCESS;
    }
#endif

    if (pDecInfo->frameOffset < 0) {
        SetJpgPendingInst(pJpgInst);
        JLOG(ERR, "End of stream!");
        return JPG_RET_EOS;
    }

    val = JpegDecodeHeader(pDecInfo);
    if (val == 0) {
        JLOG(ERR, "Failed to call JpegDecodeHeader()! ret=%d", (int)val);
        ret = JPG_RET_FAILURE;
        goto fail;
    }

    if (val == -2) {    // wrap around case
        pDecInfo->frameOffset = 0;
        pDecInfo->ecsPtr = 0;
#ifdef MJPEG_ERROR_CONCEAL
        pDecInfo->nextOffset = 0;
#endif
        val = JpegDecodeHeader(pDecInfo);
        if (val == 0) {
            JLOG(ERR, "Failed to call JpegDecodeHeader()!");
            ret = JPG_RET_FAILURE;
            goto fail;
        }
    }

    if (val == -1) {    //stream empty case
        if (pDecInfo->streamEndflag == 1) {
            SetJpgPendingInst(pJpgInst);
            pDecInfo->frameOffset = -1;
            JLOG(WARN, "End of stream!");
            return JPG_RET_EOS;
        }

        JLOG(ERR, "Bitstream is empty!");
        ret = JPG_RET_BIT_EMPTY;
        goto fail;
    }

    if (pDecInfo->streamRdPtr == pDecInfo->streamBufEndAddr) {
        JpuWriteReg(MJPEG_BBC_CUR_POS_REG, 0);
        JpuWriteReg(MJPEG_GBU_TT_CNT_REG, 0);
        JpuWriteReg(MJPEG_GBU_TT_CNT_REG+4, 0);
    }

    JpuWriteReg(MJPEG_BBC_WR_PTR_REG, pDecInfo->streamWrPtr);
    if (pDecInfo->streamWrPtr == pDecInfo->streamBufStartAddr)
        JpuWriteReg(MJPEG_BBC_END_ADDR_REG, pDecInfo->streamBufEndAddr);
    else
        JpuWriteReg(MJPEG_BBC_END_ADDR_REG, pDecInfo->streamWrPtr);

    JpuWriteReg(MJPEG_BBC_BAS_ADDR_REG, pDecInfo->streamBufStartAddr);
    //JpuWriteReg(MJPEG_BBC_END_ADDR_REG, pDecInfo->streamWrPtr);

    if (pDecInfo->streamEndflag == 1) {
        val = JpuReadReg(MJPEG_BBC_STRM_CTRL_REG);
        if ((val & (1 << 31)) == 0 ) {
            val = (pDecInfo->streamWrPtr-pDecInfo->streamBufStartAddr) / 256;
            if ((pDecInfo->streamWrPtr-pDecInfo->streamBufStartAddr) % 256)
                val = val + 1;

            JpuWriteReg(MJPEG_BBC_STRM_CTRL_REG, (1 << 31 | val));
        }
    }
    else {
        JpuWriteReg(MJPEG_BBC_STRM_CTRL_REG, 0);
    }

    JpuWriteReg(MJPEG_GBU_TT_CNT_REG, 0);
    JpuWriteReg(MJPEG_GBU_TT_CNT_REG+4, 0);
    JpuWriteReg(MJPEG_PIC_ERRMB_REG, 0);
    JpuWriteReg(MJPEG_PIC_CTRL_REG, pDecInfo->huffAcIdx << 10 | pDecInfo->huffDcIdx << 7 | pDecInfo->userHuffTab << 6 | (JPU_CHECK_WRITE_RESPONSE_BVALID_SIGNAL << 2) | pDecInfo->usePartial);

    JpuWriteReg(MJPEG_PIC_SIZE_REG, (pDecInfo->alignedWidth << 16) | pDecInfo->alignedHeight);
    JpuWriteReg(MJPEG_ROT_INFO_REG, 0);
    JpuWriteReg(MJPEG_OP_INFO_REG,  pDecInfo->lineNum << 16| pDecInfo->bufNum << 3 |pDecInfo->busReqNum);
    JpuWriteReg(MJPEG_MCU_INFO_REG, pDecInfo->mcuBlockNum << 16 | pDecInfo->compNum << 12 | pDecInfo->compInfo[0] << 8 | pDecInfo->compInfo[1] << 4 | pDecInfo->compInfo[2]);

    if (pDecInfo->packedFormat == PACKED_FORMAT_NONE)
        JpuWriteReg(MJPEG_DPB_CONFIG_REG, (pDecInfo->frameEndian << 6) | (0<<5) | (0<<4) | ((pDecInfo->chromaInterleave==0)?0:(pDecInfo->chromaInterleave==1)?2:3));
    else if (pDecInfo->packedFormat == PACKED_FORMAT_444)
        JpuWriteReg(MJPEG_DPB_CONFIG_REG, (pDecInfo->frameEndian << 6) | (1<<5) | (0<<4) | (0<<2) | ((pDecInfo->chromaInterleave==0)?0:(pDecInfo->chromaInterleave==1)?2:3));
    else
        JpuWriteReg(MJPEG_DPB_CONFIG_REG, (pDecInfo->frameEndian << 6) | (0<<5) | (1<<4) | ((pDecInfo->packedFormat-1)<<2) | ((pDecInfo->chromaInterleave==0)?0:(pDecInfo->chromaInterleave==1)?2:3));

    JpuWriteReg(MJPEG_RST_INTVAL_REG, pDecInfo->rstIntval);

    if (param) {
        if (param->scaleDownRatioWidth > 0 )
            pDecInfo->iHorScaleMode = param->scaleDownRatioWidth;
        if (param->scaleDownRatioHeight > 0 )
            pDecInfo->iVerScaleMode = param->scaleDownRatioHeight;
    }
    if (pDecInfo->iHorScaleMode | pDecInfo->iVerScaleMode)
        val = ((pDecInfo->iHorScaleMode&0x3)<<2) | ((pDecInfo->iVerScaleMode&0x3)) | 0x10 ;
    else
        val = 0;
    JpuWriteReg(MJPEG_SCL_INFO_REG, val);

    if (pDecInfo->userHuffTab) {
        if (!JpgDecHuffTabSetUp(pDecInfo)) {
            JLOG(ERR, "Failed to setup HuffTab!");
            ret = JPG_RET_INVALID_PARAM;
            goto fail;
        }
    }

    if (!JpgDecQMatTabSetUp(pDecInfo)) {
        JLOG(ERR, "Failed to setup QMatTab!");
        ret = JPG_RET_INVALID_PARAM;
        goto fail;
    }

    JpgDecGramSetup(pDecInfo);

    JpuWriteReg(MJPEG_RST_INDEX_REG, 0); // RST index at the beginning.
    JpuWriteReg(MJPEG_RST_COUNT_REG, 0);

    JpuWriteReg(MJPEG_DPCM_DIFF_Y_REG, 0);
    JpuWriteReg(MJPEG_DPCM_DIFF_CB_REG, 0);
    JpuWriteReg(MJPEG_DPCM_DIFF_CR_REG, 0);

    JpuWriteReg(MJPEG_GBU_FF_RPTR_REG, pDecInfo->bitPtr);
    JpuWriteReg(MJPEG_GBU_CTRL_REG, 3);

    JpuWriteReg(MJPEG_ROT_INFO_REG, rotMir);

    if (rotMir & 1) {
        pDecInfo->format = (pDecInfo->format==FORMAT_422) ? FORMAT_224 : (pDecInfo->format==FORMAT_224) ? FORMAT_422 : pDecInfo->format;
    }

    if (rotMir & 0x10) {
        JpuWriteReg(MJPEG_DPB_BASE00_REG, pDecInfo->rotatorOutput.bufY);
        JpuWriteReg(MJPEG_DPB_BASE01_REG, pDecInfo->rotatorOutput.bufCb);
        JpuWriteReg(MJPEG_DPB_BASE02_REG, pDecInfo->rotatorOutput.bufCr);
    }
    else if(pDecInfo->usePartial){
        val = (pDecInfo->frameIdx % (pDecInfo->numFrameBuffers/pDecInfo->bufNum));
        for(i=0; i<pDecInfo->bufNum; i++)
        {
            JpuWriteReg(MJPEG_DPB_BASE00_REG + (i*12), pDecInfo->frameBufPool[(val*pDecInfo->bufNum)+i].bufY);
            JpuWriteReg(MJPEG_DPB_BASE01_REG + (i*12), pDecInfo->frameBufPool[(val*pDecInfo->bufNum)+i].bufCb);
            JpuWriteReg(MJPEG_DPB_BASE02_REG + (i*12), pDecInfo->frameBufPool[(val*pDecInfo->bufNum)+i].bufCr);
        }
    }
    else {
        JpuWriteReg(MJPEG_DPB_BASE00_REG, pDecInfo->rotatorOutput.bufY);
        JpuWriteReg(MJPEG_DPB_BASE01_REG, pDecInfo->rotatorOutput.bufCb);
        JpuWriteReg(MJPEG_DPB_BASE02_REG, pDecInfo->rotatorOutput.bufCr);
    }

    if(pDecInfo->rotationEnable) {
        JpuWriteReg(MJPEG_DPB_YSTRIDE_REG, pDecInfo->rotatorStride);

        val = (pDecInfo->format == FORMAT_420 || pDecInfo->format == FORMAT_422 || pDecInfo->format == FORMAT_400) ? 2 : 1;
        if (pDecInfo->chromaInterleave)
            JpuWriteReg(MJPEG_DPB_CSTRIDE_REG, (pDecInfo->rotatorStride/(int)val)*2);
        else
            JpuWriteReg(MJPEG_DPB_CSTRIDE_REG, pDecInfo->rotatorStride/(int)val);
    }
    else {
        JpuWriteReg(MJPEG_DPB_YSTRIDE_REG, pDecInfo->stride);

        val = (pDecInfo->format == FORMAT_420 || pDecInfo->format == FORMAT_422 || pDecInfo->format == FORMAT_400) ? 2 : 1;
        if (pDecInfo->chromaInterleave)
            JpuWriteReg(MJPEG_DPB_CSTRIDE_REG, (pDecInfo->stride/(int)val)*2);
        else
            JpuWriteReg(MJPEG_DPB_CSTRIDE_REG, pDecInfo->stride/(int)val);
    }
    if (pDecInfo->roiEnable) {
        JpuWriteReg(MJPEG_CLP_INFO_REG, 1);
        JpuWriteReg(MJPEG_CLP_BASE_REG, pDecInfo->roiOffsetX << 16 | pDecInfo->roiOffsetY); // pixel unit
        JpuWriteReg(MJPEG_CLP_SIZE_REG, (pDecInfo->roiMcuWidth*pDecInfo->mcuWidth) << 16 | (pDecInfo->roiMcuHeight*pDecInfo->mcuHeight)); // pixel Unit
    }
    else {
        JpuWriteReg(MJPEG_CLP_INFO_REG, 0);
    }

    if (pJpgInst->loggingEnable)
        jdi_log(JDI_LOG_CMD_PICRUN, 1);

    JpuWriteReg(MJPEG_PIC_STATUS_REG, JpuReadReg(MJPEG_PIC_STATUS_REG));
    JpuWriteReg(MJPEG_PIC_START_REG, (1<<JPG_START_PIC));

    SetJpgPendingInst(pJpgInst);

    return JPG_RET_SUCCESS;

fail:
    JpgLeaveLock();

    return ret;
}

JpgRet JPU_DecGetOutputInfo(JpgDecHandle handle, JpgDecOutputInfo * info)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo  *pDecInfo;
    JpgRet      ret =  JPG_RET_SUCCESS;
    Uint32      val = 0;

    if (info == 0) {
        JLOG(ERR, "Invalid parameter: info is NULL!");
        ret = JPG_RET_INVALID_PARAM;
        goto fail;
    }

    val = JpuReadReg(MJPEG_PIC_STATUS_REG);
    if (val != 0)
        JpuWriteReg(MJPEG_PIC_STATUS_REG, val);

    pDecInfo = &pJpgInst->JpgInfo.decInfo;
    if (pDecInfo->frameOffset < 0) {
        info->numOfErrMBs = 0;
        info->decodingSuccess = 1;
        ret = JPG_RET_SUCCESS; // TODO
        goto fail;
    }

    if (pDecInfo->roiEnable) {
        info->decPicWidth = pDecInfo->roiMcuWidth*pDecInfo->mcuWidth;
        info->decPicHeight = pDecInfo->roiMcuHeight*pDecInfo->mcuHeight;
    } else {
        info->decPicWidth = pDecInfo->alignedWidth;
        info->decPicHeight = pDecInfo->alignedHeight;
    }
    if ((pDecInfo->rotationEnable) && ((pDecInfo->rotationAngle == 90) || (pDecInfo->rotationAngle == 270))) {
        int temp = info->decPicWidth;
        info->decPicWidth = info->decPicHeight;
        info->decPicHeight = temp;
    }

    info->decPicWidth  >>= pDecInfo->iHorScaleMode;
    info->decPicHeight >>= pDecInfo->iVerScaleMode;

#ifdef MJPEG_ERROR_CONCEAL
    info->consumedByte = pDecInfo->gbuStartPtr + (JpuReadReg(MJPEG_GBU_TT_CNT_REG))/8;
#else
    info->consumedByte = (JpuReadReg(MJPEG_GBU_TT_CNT_REG))/8;
#endif
    pDecInfo->streamRdPtr = jdi_get_memory_addr_high(JpuReadReg(MJPEG_BBC_RD_PTR_REG));
    pDecInfo->consumeByte = info->consumedByte - 16 - pDecInfo->ecsPtr;
    info->bytePosFrameStart = pDecInfo->frameOffset;
    info->ecsPtr = pDecInfo->ecsPtr;

    pDecInfo->ecsPtr = 0;
    pDecInfo->frameIdx++;

    if (val & (1<<INT_JPU_DONE)) {
        info->decodingSuccess = 1;
        info->numOfErrMBs = 0;

#ifdef MJPEG_ERROR_CONCEAL
        pDecInfo->errInfo.bError = 0;
        pDecInfo->nextOffset = 0;
        pDecInfo->gbuStartPtr = 0;
#endif
    }
    else if (val & (1<<INT_JPU_ERROR)){
        info->numOfErrMBs = JpuReadReg(MJPEG_PIC_ERRMB_REG);
        info->decodingSuccess = 0;

#ifdef MJPEG_ERROR_CONCEAL
        // info->numOfErrMBs = JpuReadReg(MJPEG_PIC_ERRMB_REG);
        pDecInfo->errInfo.bError  = 1;
        pDecInfo->errInfo.errPosY = info->numOfErrMBs & 0xFFF;
        pDecInfo->errInfo.errPosX = (info->numOfErrMBs >> 12) & 0xFFF;

        // set search point to find next rstMarker from origin of frame buffer by host
        pDecInfo->nextOffset  = (info->consumedByte) & (~7);

        // prevent to find same position.
        if(pDecInfo->currOffset == pDecInfo->nextOffset)
            pDecInfo->nextOffset += JPU_GBU_SIZE;
#endif
        ret = JPG_RET_FAILURE;
        JLOG(ERR, "JPEG decode error!");
    }

    if (pJpgInst->loggingEnable)
        jdi_log(JDI_LOG_CMD_PICRUN, 0);

fail:
#if defined(LIBBMJPULITE)
    /* To fix the bug affecting the following encoding,
     * Initialize the internal status of JPU at the end of decoding. */
    //JPU_InitCodecStatus();
#endif

    SetJpgPendingInst(0);

    JpgLeaveLock();

    return ret;
}

JpgRet JPU_DecGetRotateInfo(JpgDecHandle handle, int *rotationEnable, int *rotationAngle, int *mirrorEnable, int *mirrorDirection)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;
    pDecInfo = &pJpgInst->JpgInfo.decInfo;

    *rotationEnable = pDecInfo->rotationEnable;
    *mirrorEnable   = pDecInfo->mirrorEnable;
    *mirrorDirection= pDecInfo->mirrorDirection;
    *rotationAngle  = pDecInfo->rotationAngle;
    return JPG_RET_SUCCESS;
}

JpgRet JPU_DecGiveCommand(JpgDecHandle handle, JpgCommand cmd, void * param)
{
    JpgInst * pJpgInst = handle;
    JpgDecInfo * pDecInfo;

    pDecInfo = &pJpgInst->JpgInfo.decInfo;
    switch (cmd)
    {
    case ENABLE_JPG_ROTATION :
        {
            if(pDecInfo->roiEnable) {
                return JPG_RET_INVALID_PARAM;
            }
           /*
            if (pDecInfo->rotatorStride == 0) {
                return JPG_RET_ROTATOR_STRIDE_NOT_SET;
            }  */
            pDecInfo->rotationEnable = 1;
            break;
        }
    case DISABLE_JPG_ROTATION :
        {
            pDecInfo->rotationEnable = 0;
            break;
        }
    case ENABLE_JPG_MIRRORING :
        {
        /*  if (pDecInfo->rotatorStride == 0) {
                return JPG_RET_ROTATOR_STRIDE_NOT_SET;
            } */
            pDecInfo->mirrorEnable = 1;
            break;
        }
    case DISABLE_JPG_MIRRORING :
        {
            pDecInfo->mirrorEnable = 0;
            break;
        }
    case SET_JPG_MIRROR_DIRECTION :
        {
            JpgMirrorDirection mirDir;

            if (param == 0) {
                return JPG_RET_INVALID_PARAM;
            }
            mirDir = *(JpgMirrorDirection *)param;
            if (!(MIRDIR_NONE <= mirDir && mirDir <= MIRDIR_HOR_VER)) {
                return JPG_RET_INVALID_PARAM;
            }
            pDecInfo->mirrorDirection = mirDir;

            break;
        }
    case SET_JPG_ROTATION_ANGLE :
        {
            int angle;

            if (param == 0) {
                return JPG_RET_INVALID_PARAM;
            }

            angle = *(int *)param;
            if (angle != 0 && angle != 90 &&
                angle != 180 && angle != 270) {
                return JPG_RET_INVALID_PARAM;
            }

            pDecInfo->rotationAngle = angle;
            break;
        }
    case SET_JPG_ROTATOR_OUTPUT :
        {
            FrameBuffer * frame;

            if (param == 0) {
                return JPG_RET_INVALID_PARAM;
            }
            frame = (FrameBuffer *)param;
            pDecInfo->rotatorOutput = *frame;
            pDecInfo->rotatorOutputValid = 1;
            break;
        }
    case SET_JPG_ROTATOR_STRIDE :
        {
            int stride;

            if (param == 0) {
                return JPG_RET_INVALID_PARAM;
            }
            stride = *(int *)param;
            if (stride % 8 != 0 || stride == 0) {
                return JPG_RET_INVALID_STRIDE;
            }

            if (pDecInfo->rotationAngle == 90 || pDecInfo->rotationAngle == 270) {
                if (pDecInfo->alignedHeight > stride) {
                    return JPG_RET_INVALID_STRIDE;
                }
            } else {
                if (pDecInfo->alignedWidth > stride) {
                    return JPG_RET_INVALID_STRIDE;
                }
            }
            pDecInfo->rotatorStride = stride;
            break;
        }
    case SET_JPG_SCALE_HOR:
        {
            int scale;
            scale = *(int *)param;
            if (pDecInfo->alignedWidth < 128 || pDecInfo->alignedHeight < 128) {
                if (scale) {
                    return JPG_RET_INVALID_PARAM;
                }
            }
            pDecInfo->iHorScaleMode = scale;
            break;
        }
    case SET_JPG_SCALE_VER:
        {
            int scale;
            scale = *(int *)param;
            if (pDecInfo->alignedWidth < 128 || pDecInfo->alignedHeight < 128) {
                if (scale) {
                    return JPG_RET_INVALID_PARAM;
                }
            }
            pDecInfo->iVerScaleMode = scale;
            break;
        }
    case SET_JPG_USE_PARTIAL_MODE:
        {
            int enable;
            enable = *(int *)param;
            pDecInfo->usePartial = enable;

            break;
        }
    case SET_JPG_PARTIAL_FRAME_NUM:
        {
            int frame;

            if (pDecInfo->stride != 0) {
                return JPG_RET_WRONG_CALL_SEQUENCE;
            }

            frame = *(int *)param;
            pDecInfo->bufNum = frame;

            break;
        }
    case SET_JPG_PARTIAL_LINE_NUM:
        {
            int line;

            if (pDecInfo->stride != 0) {
                return JPG_RET_WRONG_CALL_SEQUENCE;
            }
            line = *(int *)param;
            pDecInfo->lineNum = line;

            break;
        }
    case ENABLE_LOGGING:
        {
            pJpgInst->loggingEnable = 1;
        }
        break;
    case DISABLE_LOGGING:
        {
            pJpgInst->loggingEnable = 0;
        }
        break;
    default:
        return JPG_RET_INVALID_COMMAND;
    }
    return JPG_RET_SUCCESS;
}

JpgRet JPU_EncOpen(JpgEncHandle * pHandle, JpgEncOpenParam * pop)
{
    JpgEncInfo * pEncInfo;
    JpgRet ret;
    int i;
#ifdef DUMP_JPU_REGS
    JPU_Print_Reg_Status(2);
#endif
    ret = CheckJpgEncOpenParam(pop);
    if (ret != JPG_RET_SUCCESS) {
        return ret;
    }

    JpgEnterLock(); // TODO this should be a process lock
    ((JpgInst *)(*pHandle))->device_index = pop->deviceIndex;
    pEncInfo = &((JpgInst *)(*pHandle))->JpgInfo.encInfo;

    pEncInfo->openParam = *pop;
    pEncInfo->streamRdPtr = pop->bitstreamBuffer;
    pEncInfo->streamWrPtr = pop->bitstreamBuffer;

    pEncInfo->streamBufStartAddr = pop->bitstreamBuffer;
    pEncInfo->streamBufSize = pop->bitstreamBufferSize;

    /*
        When Wr pointer points to 512 bytes before the end of the bitstream buffer,
     INT_JPU_BIT_BUF_FULL interrupt is generated,prevent Wr pointer from crossing
     the boundary, so subtract 512 here.
    */
    pEncInfo->streamBufEndAddr = pop->bitstreamBuffer + pop->bitstreamBufferSize - 512;
    pEncInfo->streamEndian = pop->streamEndian;
    pEncInfo->frameEndian = pop->frameEndian;
    pEncInfo->chromaInterleave = pop->chromaInterleave;

    pEncInfo->format = pEncInfo->openParam.sourceFormat;
    pEncInfo->picWidth= pEncInfo->openParam.picWidth;
    pEncInfo->picHeight = pEncInfo->openParam.picHeight;

    pEncInfo->rotationEnable  = pop->rotationEnable;
    pEncInfo->mirrorEnable    = pop->mirrorEnable;
    pEncInfo->mirrorDirection = pop->mirrorDirection;
    pEncInfo->rotationAngle   = pop->rotationAngle;

    // Picture size alignment
    if (pEncInfo->format == FORMAT_420 || pEncInfo->format == FORMAT_422)
        pEncInfo->alignedWidth = ((pEncInfo->picWidth+15)/16)*16;
    else
        pEncInfo->alignedWidth = ((pEncInfo->picWidth+7)/8)*8;

    if (pEncInfo->format == FORMAT_420 || pEncInfo->format == FORMAT_224)
        pEncInfo->alignedHeight = ((pEncInfo->picHeight+15)/16)*16;
    else
        pEncInfo->alignedHeight = ((pEncInfo->picHeight+7)/8)*8;

    pEncInfo->rstIntval = pEncInfo->openParam.restartInterval;

    for (i=0; i<4; i++)
    {
        pEncInfo->pHuffVal[i]  = pEncInfo->openParam.huffVal[i];
        pEncInfo->pHuffBits[i] = pEncInfo->openParam.huffBits[i];
        pEncInfo->pQMatTab[i]  = pEncInfo->openParam.qMatTab[i];
        pEncInfo->pCInfoTab[i] = pEncInfo->openParam.cInfoTab[i];
    }

    if (pop->packedFormat == PACKED_FORMAT_444 && pEncInfo->format != FORMAT_444) {
        JpgLeaveLock();
        return JPG_RET_INVALID_PARAM;
    }
    pEncInfo->packedFormat = pop->packedFormat;
    JpgLeaveLock();

    return JPG_RET_SUCCESS;
}

JpgRet JPU_EncClose(JpgEncHandle handle)
{
    JpgInst * pJpgInst = handle;
    JpgRet ret = JPG_RET_SUCCESS;

    JpgEnterLock(); // TODO this should be a process lock

    JpuWriteReg(MJPEG_BBC_FLUSH_CMD_REG, 0);
    if(FreeJpgInstance(pJpgInst) < 0)
        ret = JPG_RET_FAILURE;
    JpgLeaveLock();

    return ret;
}


JpgRet JPU_EncGetInitialInfo(JpgEncHandle handle, JpgEncInitialInfo * info)
{
    JpgInst * pJpgInst = handle;
    JpgEncInfo * pEncInfo;

    if (info == 0) {
        return JPG_RET_INVALID_PARAM;
    }

    pEncInfo = &pJpgInst->JpgInfo.encInfo;

    if (pEncInfo->initialInfoObtained) {
        return JPG_RET_CALLED_BEFORE;
    }


    JpgEnterLock(); // TODO this should be a process lock

    if (pEncInfo->format == FORMAT_400) {
        pEncInfo->compInfo[1] = 0;
        pEncInfo->compInfo[2] = 0;
    }
    else {
        pEncInfo->compInfo[1] = 5;
        pEncInfo->compInfo[2] = 5;
    }

    if (pEncInfo->format == FORMAT_400)
        pEncInfo->compNum = 1;
    else
        pEncInfo->compNum = 3;

    if (pEncInfo->format == FORMAT_420) {
        pEncInfo->mcuBlockNum = 6;
        pEncInfo->compInfo[0] = 10;
        pEncInfo->busReqNum = 2;
    }
    else if (pEncInfo->format == FORMAT_422) {
        pEncInfo->mcuBlockNum = 4;
        pEncInfo->busReqNum = 3;
        pEncInfo->compInfo[0] = 9;
    } else if (pEncInfo->format == FORMAT_224) {
        pEncInfo->mcuBlockNum = 4;
        pEncInfo->busReqNum  = 3;
        pEncInfo->compInfo[0] = 6;
    } else if (pEncInfo->format == FORMAT_444) {
        pEncInfo->mcuBlockNum = 3;
        pEncInfo->compInfo[0] = 5;
        pEncInfo->busReqNum = 4;
    } else if (pEncInfo->format == FORMAT_400) {
        pEncInfo->mcuBlockNum = 1;
        pEncInfo->busReqNum = 4;
        pEncInfo->compInfo[0] = 5;
    }

    info->minFrameBufferCount = 0;
    info->colorComponents = pEncInfo->compNum;

    pEncInfo->initialInfo = *info;
    pEncInfo->initialInfoObtained = 1;

    JpgLeaveLock();
    return JPG_RET_SUCCESS;
}


#if !defined(LIBBMJPULITE)
JpgRet JPU_EncGetBitstreamBuffer(JpgEncHandle handle,
                                 PhysicalAddress * prdPtr,
                                 PhysicalAddress * pwrPtr,
                                 int * size)
{
    JpgInst * pJpgInst = handle;
    JpgEncInfo * pEncInfo;
    JpgRet ret;

    ret = CheckJpgInstValidity(handle);
    if (ret != JPG_RET_SUCCESS)
        return ret;

    if ( prdPtr == 0 || pwrPtr == 0 || size == 0) {
        return JPG_RET_INVALID_PARAM;
    }

    pEncInfo = &pJpgInst->JpgInfo.encInfo;

    JpgSetClockGate(1);
    *prdPtr = pEncInfo->streamRdPtr;

    if (GetJpgPendingInst() == pJpgInst)
        *pwrPtr = jdi_get_memory_addr_high(JpuReadReg(MJPEG_BBC_WR_PTR_REG));
    else
        *pwrPtr = pEncInfo->streamWrPtr;

    *size = *pwrPtr - *prdPtr;
    JpgSetClockGate(0);

    return JPG_RET_SUCCESS;

}

JpgRet JPU_EncUpdateBitstreamBuffer(JpgEncHandle handle, int size)
{
    JpgInst * pJpgInst = handle;
    JpgEncInfo * pEncInfo;
    PhysicalAddress rdPtr;
    JpgRet ret;

    ret = CheckJpgInstValidity(handle);
    if (ret != JPG_RET_SUCCESS)
        return ret;

    pEncInfo = &pJpgInst->JpgInfo.encInfo;

    JpgSetClockGate(1);
    rdPtr = pEncInfo->streamRdPtr;
    rdPtr += size;

    if (rdPtr == pEncInfo->streamBufEndAddr) {
        rdPtr = pEncInfo->streamBufStartAddr;
    }

    pEncInfo->streamRdPtr = pEncInfo->streamBufStartAddr;

    if (GetJpgPendingInst() == pJpgInst) {
        pEncInfo->streamWrPtr = jdi_get_memory_addr_high(JpuReadReg(MJPEG_BBC_WR_PTR_REG));
        JpuWriteReg(MJPEG_BBC_CUR_POS_REG, 0);
        JpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, pEncInfo->streamBufStartAddr);
        JpuWriteReg(MJPEG_BBC_RD_PTR_REG, pEncInfo->streamBufStartAddr);
        JpuWriteReg(MJPEG_BBC_WR_PTR_REG, pEncInfo->streamBufStartAddr);
    }
    JpgSetClockGate(0);
    return JPG_RET_SUCCESS;
}
#endif

JpgRet JPU_EncIssueStop(JpgEncHandle handle)
{
    JpgSetClockGate(1);
    JpuWriteReg(MJPEG_PIC_START_REG, 1<< JPG_START_STOP);
    JpgSetClockGate(0);
    return JPG_RET_SUCCESS;
}

JpgRet JPU_EncCompleteStop(JpgEncHandle handle)
{
    Uint32 val;
    JpgSetClockGate(1);
    val = JpuReadReg(MJPEG_PIC_STATUS_REG);

    if (val & (1<<INT_JPU_BIT_BUF_STOP)) {
        SetJpgPendingInst(0);
        JpgSetClockGate(0);
    }
    else {
        JpgSetClockGate(0);
        return JPG_RET_WRONG_CALL_SEQUENCE;
    }

    return JPG_RET_SUCCESS;
}

JpgRet JPU_EncStartOneFrame(JpgEncHandle handle, JpgEncParam * param )
{
    JpgInst * pJpgInst = handle;
    JpgEncInfo * pEncInfo;
    FrameBuffer * pBasFrame;
    Uint32 rotMirEnable;
    Uint32 rotMirMode;
    int ret = JPG_RET_SUCCESS;

    pEncInfo = &pJpgInst->JpgInfo.encInfo;

    ret = CheckJpgEncParam(handle, param);
    if (ret != JPG_RET_SUCCESS) {
        return ret;
    }

/*
#ifdef LIBBMJPULITE
    JpgEncDumpIn(handle, param);
#endif
*/
    pBasFrame = param->sourceFrame;
    rotMirEnable = 0;
    rotMirMode = 0;
    if (pEncInfo->rotationEnable) {
        rotMirEnable = 0x10; // Enable rotator
        switch (pEncInfo->rotationAngle) {
        case 0:
            rotMirMode |= 0x0;
            break;
        case 90:
            rotMirMode |= 0x1;
            break;
        case 180:
            rotMirMode |= 0x2;
            break;
        case 270:
            rotMirMode |= 0x3;
            break;
        }
    }

    if (pEncInfo->mirrorEnable) {
        rotMirEnable = 0x10; // Enable rotator
        switch (pEncInfo->mirrorDirection) {
        case MIRDIR_NONE :
            rotMirMode |= 0x0;
            break;
        case MIRDIR_VER :
            rotMirMode |= 0x4;
            break;
        case MIRDIR_HOR :
            rotMirMode |= 0x8;
            break;
        case MIRDIR_HOR_VER :
            rotMirMode |= 0xc;
            break;
        }
    }

    JpgEnterLock(); // TODO this should be a process lock

    ret = GetJpgCoreIndex(pJpgInst, pBasFrame->bufY, pEncInfo->streamBufStartAddr);
    if (ret < 0) {
        ret = JPG_RET_FAILURE;
        goto fail;
    }

    JpuEncFlush(pEncInfo);

    JpuWriteReg(MJPEG_CLP_INFO_REG, 0);    //off ROI enable due to not supported feature for encoder.

    JpuWriteReg(MJPEG_BBC_BAS_ADDR_REG, pEncInfo->streamBufStartAddr);
    JpuWriteReg(MJPEG_BBC_END_ADDR_REG, pEncInfo->streamBufEndAddr);
    JpuWriteReg(MJPEG_BBC_WR_PTR_REG, pEncInfo->streamBufStartAddr);
    JpuWriteReg(MJPEG_BBC_RD_PTR_REG, pEncInfo->streamBufStartAddr);
    JpuWriteReg(MJPEG_BBC_CUR_POS_REG, 0);
    JpuWriteReg(MJPEG_BBC_DATA_CNT_REG, 256 / 4);    // 64 * 4 byte == 32 * 8 byte
    JpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, pEncInfo->streamBufStartAddr);
    JpuWriteReg(MJPEG_BBC_INT_ADDR_REG, 0);

    JpuWriteReg(MJPEG_GBU_BT_PTR_REG, 0);
    JpuWriteReg(MJPEG_GBU_WD_PTR_REG, 0);
    JpuWriteReg(MJPEG_GBU_BBSR_REG, 0);
    JpuWriteReg(MJPEG_GBU_CTRL_REG, 0);

    JpuWriteReg(MJPEG_GBU_BBER_REG, ((256 / 4) * 2) - 1);
    JpuWriteReg(MJPEG_GBU_BBIR_REG, 256 / 4);    // 64 * 4 byte == 32 * 8 byte
    JpuWriteReg(MJPEG_GBU_BBHR_REG, 256 / 4);    // 64 * 4 byte == 32 * 8 byte
    JpuWriteReg(MJPEG_PIC_CTRL_REG, 0x18 | pEncInfo->usePartial | (JPU_CHECK_WRITE_RESPONSE_BVALID_SIGNAL<<2));
    JpuWriteReg(MJPEG_SCL_INFO_REG, 0);
    if (pEncInfo->packedFormat == PACKED_FORMAT_NONE)
        JpuWriteReg(MJPEG_DPB_CONFIG_REG, (pEncInfo->frameEndian << 6) | (0<<5) | (0<<4) | (0<<2) | ((pEncInfo->chromaInterleave==0)?0:(pEncInfo->chromaInterleave==1)?2:3));
    else if (pEncInfo->packedFormat == PACKED_FORMAT_444)
        JpuWriteReg(MJPEG_DPB_CONFIG_REG, (pEncInfo->frameEndian << 6) | (1<<5) | (0<<4) | (0<<2) | ((pEncInfo->chromaInterleave==0)?0:(pEncInfo->chromaInterleave==1)?2:3));
    else
        JpuWriteReg(MJPEG_DPB_CONFIG_REG, (pEncInfo->frameEndian << 6) | (0<<5) | (1<<4) | ((pEncInfo->packedFormat-1)<<2) | ((pEncInfo->chromaInterleave==0)?0:(pEncInfo->chromaInterleave==1)?2:3));

    JpuWriteReg(MJPEG_RST_INTVAL_REG, pEncInfo->rstIntval);
    JpuWriteReg(MJPEG_BBC_CTRL_REG, (pEncInfo->streamEndian << 1) | 1);
    JpuWriteReg(MJPEG_OP_INFO_REG, pEncInfo->partiallineNum << 16 | pEncInfo->partialBufNum << 3 | pEncInfo->busReqNum);

    // Load HUFFTab
    if (!JpgEncLoadHuffTab(pEncInfo)) {
        ret = JPG_RET_INVALID_PARAM;
        goto fail;
    }

    // Load QMATTab
    if (!JpgEncLoadQMatTab(pEncInfo)) {
        ret = JPG_RET_INVALID_PARAM;
        goto fail;
    }

    JpgEncEncodeHeader(handle, pEncInfo->paraSet);

    //although rotator is enable, this picture size must not be changed from widh to height.
    JpuWriteReg(MJPEG_PIC_SIZE_REG, pEncInfo->alignedWidth<<16 | pEncInfo->alignedHeight);
    JpuWriteReg(MJPEG_ROT_INFO_REG, (rotMirEnable|rotMirMode));

    JpuWriteReg(MJPEG_MCU_INFO_REG, pEncInfo->mcuBlockNum << 16 | pEncInfo->compNum << 12
        | pEncInfo->compInfo[0] << 8 | pEncInfo->compInfo[1] << 4 | pEncInfo->compInfo[2]);

    //JpgEncGbuResetReg
    JpuWriteReg(MJPEG_GBU_CTRL_REG, pEncInfo->stuffByteEnable<<3);     // stuffing "FF" data where frame end

    if(pEncInfo->usePartial){
        int i;
        for(i=0; i<pEncInfo->partialBufNum; i++) {
            JpuWriteReg(MJPEG_DPB_BASE00_REG + (i*12), pBasFrame[i].bufY);
            JpuWriteReg(MJPEG_DPB_BASE01_REG + (i*12), pBasFrame[i].bufCb);
            JpuWriteReg(MJPEG_DPB_BASE02_REG + (i*12), pBasFrame[i].bufCr);
        }
    } else {
        JpuWriteReg(MJPEG_DPB_BASE00_REG, pBasFrame->bufY);
        JpuWriteReg(MJPEG_DPB_BASE01_REG, pBasFrame->bufCb);
        JpuWriteReg(MJPEG_DPB_BASE02_REG, pBasFrame->bufCr);
    }

    JpuWriteReg(MJPEG_DPB_YSTRIDE_REG, pBasFrame->strideY);
    JpuWriteReg(MJPEG_DPB_CSTRIDE_REG, pBasFrame->strideC);

    if (pJpgInst->loggingEnable)
        jdi_log(JDI_LOG_CMD_PICRUN, 1);

    JpuWriteReg(MJPEG_PIC_STATUS_REG, JpuReadReg(MJPEG_PIC_STATUS_REG));
    if (pEncInfo->usePartial)
        JpuWriteReg(MJPEG_PIC_START_REG, (1<<JPG_START_PIC)|(1<<JPG_START_PARTIAL));
    else
        JpuWriteReg(MJPEG_PIC_START_REG, (1<<JPG_START_PIC));

    SetJpgPendingInst(pJpgInst);

    return JPG_RET_SUCCESS;

fail:
    JpgLeaveLock();
    return ret;
}

JpgRet JPU_EncGetOutputInfo(JpgEncHandle handle, JpgEncOutputInfo * info)
{
    JpgInst * pJpgInst = handle;
    JpgEncInfo * pEncInfo;
    Uint32 val;
    JpgRet ret = JPG_RET_SUCCESS;

    if (info == 0) {
        ret = JPG_RET_INVALID_PARAM;
        goto fail;
    }

    pEncInfo = &pJpgInst->JpgInfo.encInfo;

    val = JpuReadReg(MJPEG_PIC_STATUS_REG);
    if (val != 0)
        JpuWriteReg(MJPEG_PIC_STATUS_REG, val);

    if ((val & 0x4) >> 2) {
        ret = JPG_RET_WRONG_CALL_SEQUENCE;
        goto fail;
    }
    if (val & 0x2) {
        ret = JPG_RET_FAILURE;
        JLOG(ERR, "JPEG encoding error!");
        goto fail;
    }

    info->bitstreamBuffer = pEncInfo->streamBufStartAddr;
    info->bitstreamSize = jdi_get_memory_addr_high(JpuReadReg(MJPEG_BBC_WR_PTR_REG)) - pEncInfo->streamBufStartAddr;

    if(info->bitstreamSize >= pEncInfo->streamBufSize)
    {
        printf("%s(): output error, bitstreamSize = %d, streamBufSize = %d\n",__FUNCTION__, info->bitstreamSize, pEncInfo->streamBufSize);
        info->bitstreamSize = pEncInfo->streamBufSize;
        ret = JPG_RET_BS_BUFFER_FULL;
        goto fail;
    }
    JpuWriteReg(MJPEG_BBC_FLUSH_CMD_REG, 0);

    if (pJpgInst->loggingEnable)
        jdi_log(JDI_LOG_CMD_PICRUN, 0);

fail:
#if defined(LIBBMJPULITE)
    /* To fix the bug affecting the following encoding,
     * Initialize the internal status of JPU at the end of encoding. */
    //JPU_InitCodecStatus();
#endif

    SetJpgPendingInst(0);
    JpgLeaveLock();

    return ret;
}

JpgRet JPU_EncGiveCommand(JpgEncHandle handle, JpgCommand cmd, void * param)
{
    JpgInst * pJpgInst = handle;
    JpgEncInfo * pEncInfo;

    pEncInfo = &pJpgInst->JpgInfo.encInfo;
    switch (cmd)
    {
    case ENABLE_JPG_ROTATION :
        {
            pEncInfo->rotationEnable = 1;
        }
        break;
    case DISABLE_JPG_ROTATION :
        {
            pEncInfo->rotationEnable = 0;
        }
        break;
    case ENABLE_JPG_MIRRORING :
        {
            pEncInfo->mirrorEnable = 1;
        }
        break;
    case DISABLE_JPG_MIRRORING :
        {
            pEncInfo->mirrorEnable = 0;
        }
        break;
    case SET_JPG_MIRROR_DIRECTION :
        {
            JpgMirrorDirection mirDir;

            if (param == 0) {
                return JPG_RET_INVALID_PARAM;
            }
            mirDir = *(JpgMirrorDirection *)param;
            if (!(MIRDIR_NONE <= mirDir && mirDir <= MIRDIR_HOR_VER)) {
                return JPG_RET_INVALID_PARAM;
            }
            pEncInfo->mirrorDirection = mirDir;
        }
        break;
    case SET_JPG_ROTATION_ANGLE :
        {
            int angle;

            if (param == 0) {
                return JPG_RET_INVALID_PARAM;
            }
            angle = *(int *)param;
            if (angle != 0 && angle != 90 &&
                angle != 180 && angle != 270) {
                return JPG_RET_INVALID_PARAM;
            }
            if (pEncInfo->initialInfoObtained && (angle == 90 || angle ==270)) {
                return JPG_RET_INVALID_PARAM;
            }
            pEncInfo->rotationAngle = angle;
        }
        break;
    case ENC_JPG_GET_HEADER:
        {
            if (param == 0) {
                return JPG_RET_INVALID_PARAM;
            }

            pEncInfo->paraSet = (JpgEncParamSet *)param;
            break;
        }
    case SET_JPG_USE_PARTIAL_MODE:
        {
            int enable;
            enable = *(int *)param;
            pEncInfo->usePartial = enable;

            break;
        }
    case SET_JPG_PARTIAL_FRAME_NUM:
        {
            int frame;
            frame = *(int *)param;
            pEncInfo->partialBufNum = frame;

            break;
        }
    case SET_JPG_PARTIAL_LINE_NUM:
        {
            int line;
            line = *(int *)param;
            pEncInfo->partiallineNum = line;

            break;
        }
    case SET_JPG_ENCODE_NEXT_LINE:
        {
            JpuWriteReg(MJPEG_PIC_START_REG, (1<<JPG_START_PARTIAL));
            break;
        }
    case SET_JPG_USE_STUFFING_BYTE_FF:
        {
            int enable;
            enable = *(int *)param;
            pEncInfo->stuffByteEnable = enable;
            break;
        }
    case ENABLE_LOGGING:
        {
            pJpgInst->loggingEnable = 1;
        }
        break;
    case DISABLE_LOGGING:
        {
            pJpgInst->loggingEnable = 0;
        }
        break;

    default:
        return JPG_RET_INVALID_COMMAND;
    }
    return JPG_RET_SUCCESS;
}

void JpuDecFlush(JpgDecInfo *pJpgDecInfo)
{
    if(pJpgDecInfo == NULL)
    {
       printf("%s(): pJpgDecInfo = %p\n", __FUNCTION__, pJpgDecInfo) ;
    }
    pJpgDecInfo->streamWrPtr = pJpgDecInfo->streamBufStartAddr;
    pJpgDecInfo->streamRdPtr = pJpgDecInfo->streamBufStartAddr;

    JpuWriteReg(MJPEG_BBC_RD_PTR_REG, pJpgDecInfo->streamRdPtr);
    JpuWriteReg(MJPEG_BBC_WR_PTR_REG, pJpgDecInfo->streamWrPtr);
    JpuWriteReg(MJPEG_BBC_STRM_CTRL_REG, 0);

    pJpgDecInfo->frameOffset   = 0;
    pJpgDecInfo->consumeByte   = 0;
    pJpgDecInfo->nextOffset    = 0;
    pJpgDecInfo->currOffset    = 0;
    pJpgDecInfo->ecsPtr        = 0;
    pJpgDecInfo->streamEndflag = 0;
    // pDecInfo->headerSize    = 0;

    JPU_SWReset();
}

void JpuEncFlush(JpgEncInfo *pJpgEncInfo)
{
    if(pJpgEncInfo == NULL)
    {
       printf("%s(): pJpgDecInfo = %p\n", __FUNCTION__, pJpgEncInfo) ;
    }
    pJpgEncInfo->streamWrPtr = pJpgEncInfo->streamBufStartAddr;
    pJpgEncInfo->streamRdPtr = pJpgEncInfo->streamBufStartAddr;

    JpuWriteReg(MJPEG_BBC_RD_PTR_REG, pJpgEncInfo->streamRdPtr);
    JpuWriteReg(MJPEG_BBC_WR_PTR_REG, pJpgEncInfo->streamWrPtr);
    JpuWriteReg(MJPEG_BBC_STRM_CTRL_REG, 0);

    JPU_SWReset();
}
#if !defined(_WIN32)
int JPU_GetDump()
{
    int ret = JpgGetDump();
    return ret;
}
#endif
Uint32 JPU_GetCoreIndex(void * handle)
{
    JpgInst * pJpgInst = handle;
    if(pJpgInst != NULL)
        return pJpgInst->coreIndex;
    else{
        printf("%s(): pJpgInst = %p\n", __FUNCTION__, pJpgInst);
        return 0;
    }
}

int JPU_GetDeviceIndex(void * handle)
{
    JpgInst * pJpgInst = handle;
    if(pJpgInst != NULL)
        return pJpgInst->device_index;
    else{
        printf("%s(): pJpgInst = %p\n", __FUNCTION__, pJpgInst);
        return 0;
    }
}

#ifdef DUMP_JPU_REGS
void JPU_Print_Reg_Status(int type)
{
    if(type == 1)  //dec
    {
        printf(" Jpu dec regs:------------------------------------------------------------------\n");
        JPUSetReg();
        JPUPrintReg();
    }
    else if(type == 2)  //enc
    {
        printf(" Jpu enc regs:=================================================================\n");
        JPUSetReg();
        JPUPrintReg();
    }
}
void JPUSetReg()
{
    JpgSetClockGate(1);
    JpuWriteReg(MJPEG_PIC_START_REG, 0x0);
    JpuWriteReg(MJPEG_PIC_STATUS_REG, 0x0);
    JpuWriteReg(MJPEG_PIC_SETMB_REG, 0x0);

    JpuWriteReg(MJPEG_PIC_CTRL_REG, 0x0);
    JpuWriteReg(MJPEG_PIC_SIZE_REG, 0x0);
    JpuWriteReg(MJPEG_MCU_INFO_REG, 0x0);
    JpuWriteReg(MJPEG_ROT_INFO_REG, 0x0);

    JpuWriteReg(MJPEG_SCL_INFO_REG, 0x0);
    JpuWriteReg(MJPEG_IF_INFO_REG, 0x0);
    JpuWriteReg(MJPEG_CLP_INFO_REG, 0x0);
    JpuWriteReg(MJPEG_OP_INFO_REG, 0x00100001);

    JpuWriteReg(MJPEG_DPB_CONFIG_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE00_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE01_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE02_REG, 0x0);

    JpuWriteReg(MJPEG_DPB_BASE10_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE11_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE12_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE20_REG, 0x0);

    JpuWriteReg(MJPEG_DPB_BASE21_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE22_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE30_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_BASE31_REG, 0x0);

    JpuWriteReg(MJPEG_DPB_BASE32_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_YSTRIDE_REG, 0x0);
    JpuWriteReg(MJPEG_DPB_CSTRIDE_REG, 0x0);

    JpuWriteReg(MJPEG_CLP_BASE_REG, 0x0);
    JpuWriteReg(MJPEG_CLP_SIZE_REG, 0x0);

    JpuWriteReg(MJPEG_HUFF_CTRL_REG, 0x0);
    JpuWriteReg(MJPEG_HUFF_ADDR_REG, 0x0);
    JpuWriteReg(MJPEG_HUFF_DATA_REG, 0x0);

    JpuWriteReg(MJPEG_QMAT_CTRL_REG, 0x0);
    JpuWriteReg(MJPEG_QMAT_ADDR_REG, 0x0);
    JpuWriteReg(MJPEG_QMAT_DATA_REG, 0x0);

    JpuWriteReg(MJPEG_COEF_CTRL_REG, 0x0);
    JpuWriteReg(MJPEG_COEF_ADDR_REG, 0x0);
    JpuWriteReg(MJPEG_COEF_DATA_REG, 0x0);

    JpuWriteReg(MJPEG_RST_INTVAL_REG, 0x0);
    JpuWriteReg(MJPEG_RST_INDEX_REG, 0x0);
    JpuWriteReg(MJPEG_RST_COUNT_REG, 0x0);
    JpuWriteReg(MJPEG_PIC_STATUS_REG, 0x0);

    JpuWriteReg(MJPEG_INTR_MASK_REG, 0x0);

    JpuWriteReg(MJPEG_DPCM_DIFF_Y_REG, 0x0);
    JpuWriteReg(MJPEG_DPCM_DIFF_CB_REG, 0x0);
    JpuWriteReg(MJPEG_DPCM_DIFF_CR_REG, 0x0);

    JpuWriteReg(MJPEG_GBU_CTRL_REG, 0x0);

    JpuWriteReg(MJPEG_GBU_BT_PTR_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_WD_PTR_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_TT_CNT_REG, 0x0);

    JpuWriteReg(MJPEG_GBU_PBIT_08_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_PBIT_16_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_PBIT_24_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_PBIT_32_REG, 0x0);


    JpuWriteReg(MJPEG_GBU_BBSR_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_BBER_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_BBIR_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_BBHR_REG, 0x0);

    JpuWriteReg(MJPEG_GBU_BCNT_REG, 0x0);

    JpuWriteReg(MJPEG_GBU_FF_RPTR_REG, 0x0);
    JpuWriteReg(MJPEG_GBU_FF_WPTR_REG, 0x0);

    JpuWriteReg(MJPEG_BBC_END_ADDR_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_WR_PTR_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_RD_PTR_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_EXT_ADDR_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_INT_ADDR_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_DATA_CNT_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_COMMAND_REG, 0x0);

    JpuWriteReg(MJPEG_BBC_CTRL_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_CUR_POS_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_BAS_ADDR_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_STRM_CTRL_REG, 0x0);
    JpuWriteReg(MJPEG_BBC_FLUSH_CMD_REG, 0x0);
    JpgSetClockGate(0);
}
#endif

void JPUPrintReg(void)
{
    JpgSetClockGate(1);
    printf(" MJPEG_PIC_START_REG     = 0x%.8x \n", JpuReadReg(MJPEG_PIC_START_REG));
    printf(" MJPEG_PIC_STATUS_REG    = 0x%.8x \n", JpuReadReg(MJPEG_PIC_STATUS_REG));
    printf(" MJPEG_PIC_ERRMB_REG     = 0x%.8x \n", JpuReadReg(MJPEG_PIC_ERRMB_REG));
    printf(" MJPEG_PIC_SETMB_REG     = 0x%.8x \n", JpuReadReg(MJPEG_PIC_SETMB_REG));

    printf(" MJPEG_PIC_CTRL_REG      = 0x%.8x \n", JpuReadReg(MJPEG_PIC_CTRL_REG));
    printf(" MJPEG_PIC_SIZE_REG      = 0x%.8x \n", JpuReadReg(MJPEG_PIC_SIZE_REG));
    printf(" MJPEG_MCU_INFO_REG      = 0x%.8x \n", JpuReadReg(MJPEG_MCU_INFO_REG));
    printf(" MJPEG_ROT_INFO_REG      = 0x%.8x \n", JpuReadReg(MJPEG_ROT_INFO_REG));
    printf(" MJPEG_SCL_INFO_REG      = 0x%.8x \n", JpuReadReg(MJPEG_SCL_INFO_REG));
    printf(" MJPEG_IF_INFO_REG       = 0x%.8x \n", JpuReadReg(MJPEG_IF_INFO_REG));
    printf(" MJPEG_CLP_INFO_REG      = 0x%.8x \n", JpuReadReg(MJPEG_CLP_INFO_REG));
    printf(" MJPEG_OP_INFO_REG       = 0x%.8x \n", JpuReadReg(MJPEG_OP_INFO_REG));

    printf(" MJPEG_DPB_CONFIG_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_CONFIG_REG));
    printf(" MJPEG_DPB_BASE00_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE00_REG));
    printf(" MJPEG_DPB_BASE01_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE01_REG));
    printf(" MJPEG_DPB_BASE02_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE02_REG));

    printf(" MJPEG_DPB_BASE10_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE10_REG));
    printf(" MJPEG_DPB_BASE11_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE11_REG));
    printf(" MJPEG_DPB_BASE12_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE12_REG));
    printf(" MJPEG_DPB_BASE20_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE20_REG));

    printf(" MJPEG_DPB_BASE21_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE21_REG));
    printf(" MJPEG_DPB_BASE22_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE22_REG));
    printf(" MJPEG_DPB_BASE30_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE30_REG));
    printf(" MJPEG_DPB_BASE31_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE31_REG));

    printf(" MJPEG_DPB_BASE32_REG    = 0x%.8x \n", JpuReadReg(MJPEG_DPB_BASE32_REG));
    printf(" MJPEG_DPB_YSTRIDE_REG   = 0x%.8x \n", JpuReadReg(MJPEG_DPB_YSTRIDE_REG));
    printf(" MJPEG_DPB_CSTRIDE_REG   = 0x%.8x \n", JpuReadReg(MJPEG_DPB_CSTRIDE_REG));
    printf(" MJPEG_WRESP_CHECK_REG   = 0x%.8x \n", JpuReadReg(MJPEG_WRESP_CHECK_REG));

    printf(" MJPEG_CLP_BASE_REG      = 0x%.8x \n", JpuReadReg(MJPEG_CLP_BASE_REG));
    printf(" MJPEG_CLP_SIZE_REG      = 0x%.8x \n", JpuReadReg(MJPEG_CLP_SIZE_REG));

    printf(" MJPEG_HUFF_CTRL_REG     = 0x%.8x \n", JpuReadReg(MJPEG_HUFF_CTRL_REG));
    printf(" MJPEG_HUFF_ADDR_REG     = 0x%.8x \n", JpuReadReg(MJPEG_HUFF_ADDR_REG));
    printf(" MJPEG_HUFF_DATA_REG     = 0x%.8x \n", JpuReadReg(MJPEG_HUFF_DATA_REG));

    printf(" MJPEG_QMAT_CTRL_REG     = 0x%.8x \n", JpuReadReg(MJPEG_QMAT_CTRL_REG));
    printf(" MJPEG_QMAT_ADDR_REG     = 0x%.8x \n", JpuReadReg(MJPEG_QMAT_ADDR_REG));
    printf(" MJPEG_QMAT_DATA_REG     = 0x%.8x \n", JpuReadReg(MJPEG_QMAT_DATA_REG));

    printf(" MJPEG_COEF_CTRL_REG     = 0x%.8x \n", JpuReadReg(MJPEG_COEF_CTRL_REG));
    printf(" MJPEG_COEF_ADDR_REG     = 0x%.8x \n", JpuReadReg(MJPEG_COEF_ADDR_REG));
    printf(" MJPEG_COEF_DATA_REG     = 0x%.8x \n", JpuReadReg(MJPEG_COEF_DATA_REG));

    printf(" MJPEG_RST_INTVAL_REG    = 0x%.8x \n", JpuReadReg(MJPEG_RST_INTVAL_REG));
    printf(" MJPEG_RST_INDEX_REG     = 0x%.8x \n", JpuReadReg(MJPEG_RST_INDEX_REG));
    printf(" MJPEG_RST_COUNT_REG     = 0x%.8x \n", JpuReadReg(MJPEG_RST_COUNT_REG));

    printf(" MJPEG_INTR_MASK_REG     = 0x%.8x \n", JpuReadReg(MJPEG_INTR_MASK_REG));
    printf(" MJPEG_CYCLE_INFO_REG    = 0x%.8x \n", JpuReadReg(MJPEG_CYCLE_INFO_REG));

    printf(" MJPEG_DPCM_DIFF_Y_REG   = 0x%.8x \n", JpuReadReg(MJPEG_DPCM_DIFF_Y_REG));
    printf(" MJPEG_DPCM_DIFF_CB_REG  = 0x%.8x \n", JpuReadReg(MJPEG_DPCM_DIFF_CB_REG));
    printf(" MJPEG_DPCM_DIFF_CR_REG  = 0x%.8x \n", JpuReadReg(MJPEG_DPCM_DIFF_CR_REG));

    printf(" MJPEG_GBU_CTRL_REG      = 0x%.8x \n", JpuReadReg(MJPEG_GBU_CTRL_REG));
    printf(" MJPEG_GBU_PBIT_BUSY_REG = 0x%.8x \n", JpuReadReg(MJPEG_GBU_PBIT_BUSY_REG));

    printf(" MJPEG_GBU_BT_PTR_REG    = 0x%.8x \n", JpuReadReg(MJPEG_GBU_BT_PTR_REG));
    printf(" MJPEG_GBU_WD_PTR_REG    = 0x%.8x \n", JpuReadReg(MJPEG_GBU_WD_PTR_REG));
    printf(" MJPEG_GBU_TT_CNT_REG    = 0x%.8x \n", JpuReadReg(MJPEG_GBU_TT_CNT_REG));
     //printf(" MJPEG_GBU_TT_CNT_REG = 0x%.8x \n", JpuReadReg(MJPEG_GBU_TT_CNT_REG));

    printf(" MJPEG_GBU_BBSR_REG      = 0x%.8x \n", JpuReadReg(MJPEG_GBU_BBSR_REG));
    printf(" MJPEG_GBU_BBER_REG      = 0x%.8x \n", JpuReadReg(MJPEG_GBU_BBER_REG));
    printf(" MJPEG_GBU_BBIR_REG      = 0x%.8x \n", JpuReadReg(MJPEG_GBU_BBIR_REG));
    printf(" MJPEG_GBU_BBHR_REG      = 0x%.8x \n", JpuReadReg(MJPEG_GBU_BBHR_REG));

    printf(" MJPEG_GBU_BCNT_REG      = 0x%.8x \n", JpuReadReg(MJPEG_GBU_BCNT_REG));

    printf(" MJPEG_GBU_FF_RPTR_REG   = 0x%.8x \n", JpuReadReg(MJPEG_GBU_FF_RPTR_REG));
    printf(" MJPEG_GBU_FF_WPTR_REG   = 0x%.8x \n", JpuReadReg(MJPEG_GBU_FF_WPTR_REG));

    printf(" MJPEG_BBC_END_ADDR_REG  = 0x%.8x \n", JpuReadReg(MJPEG_BBC_END_ADDR_REG));
    printf(" MJPEG_BBC_WR_PTR_REG    = 0x%.8x \n", JpuReadReg(MJPEG_BBC_WR_PTR_REG));
    printf(" MJPEG_BBC_RD_PTR_REG    = 0x%.8x \n", JpuReadReg(MJPEG_BBC_RD_PTR_REG));

    printf(" MJPEG_BBC_EXT_ADDR_REG  = 0x%.8x \n", JpuReadReg(MJPEG_BBC_EXT_ADDR_REG));
    printf(" MJPEG_BBC_INT_ADDR_REG  = 0x%.8x \n", JpuReadReg(MJPEG_BBC_INT_ADDR_REG));
    printf(" MJPEG_BBC_DATA_CNT_REG  = 0x%.8x \n", JpuReadReg(MJPEG_BBC_DATA_CNT_REG));
    printf(" MJPEG_BBC_COMMAND_REG   = 0x%.8x \n", JpuReadReg(MJPEG_BBC_COMMAND_REG));
    printf(" MJPEG_BBC_BUSY_REG      = 0x%.8x \n", JpuReadReg(MJPEG_BBC_BUSY_REG));

    printf(" MJPEG_BBC_CTRL_REG      = 0x%.8x \n", JpuReadReg(MJPEG_BBC_CTRL_REG));
    printf(" MJPEG_BBC_CUR_POS_REG   = 0x%.8x \n", JpuReadReg(MJPEG_BBC_CUR_POS_REG));
    printf(" MJPEG_BBC_BAS_ADDR_REG  = 0x%.8x \n", JpuReadReg(MJPEG_BBC_BAS_ADDR_REG));
    printf(" MJPEG_BBC_STRM_CTRL_REG = 0x%.8x \n", JpuReadReg(MJPEG_BBC_STRM_CTRL_REG));
    printf(" MJPEG_BBC_FLUSH_CMD_REG = 0x%.8x \n", JpuReadReg(MJPEG_BBC_FLUSH_CMD_REG));
    JpgSetClockGate(1);
}

