/*****************************************************************************
 *
 *    Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
 *
 *    bmvid is licensed under the 2-Clause BSD License except for the
 *    third-party components.
 *
 *****************************************************************************/
/* This library provides a high-level interface for controlling the BitMain
 * Sophon VPU en/decoder.
 */
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#ifdef __linux__
#include <semaphore.h>
#elif _WIN32
#include <windows.h>
#endif
#include "bm_video_interface.h"
#include "bm_video_internal.h"
#include "vpuapi.h"
#include "config.h"
#include "vpuconfig.h"
#include "vdi_osal.h"

enum {
  SYSCXT_STATUS_WORKDING      = 0,
  SYSCXT_STATUS_EXCEPTION     ,
};

typedef struct{
  uint8_t instcall[MAX_NUM_INSTANCE];
  uint8_t instantNum;
  uint8_t coreid;

  DecOpenParam dec_param[MAX_NUM_INSTANCE];
  BMVidHandle vidHandle[MAX_NUM_INSTANCE];

  osal_cond_t cond_sleep;
  osal_cond_t cond_status;
  osal_cond_t cond_ex;

  int sys_isinit;
  osal_thread_t thread_handle;

  int crst_res;
} bm_syscxt_t;


#if defined(CHIP_BM1684)
static bm_syscxt_t g_syscxt[MAX_NUM_VPU_CORE] = {0};
#endif

int BMVidDecSeqInitW5(BMVidCodHandle vidCodHandle);
void get_lock_timeout(int sec, int pcie_board_idx);
void unlock_flock(int pcie_board_idx);

#if defined(CHIP_BM1684)
static int _syscxt_chkinst(bm_syscxt_t *p_syscxt) {
  int i;
  for (i = 0; i < MAX_NUM_INSTANCE; i++) {

    int instid = -1;
    if (p_syscxt->vidHandle[i])
      instid = p_syscxt->vidHandle[i]->codecInst->instIndex;

    if ((instid >= 0) && (p_syscxt->instcall[instid] != 3))
      return -1;
  }

  return 1;
}

static void _syscxt_release_mem(DecHandle handle) {

    CodecInst * pCodecInst;
    DecInfo * pDecInfo;
    int i;
    bm_handle_t bm_handle;
    if (!handle) {
      VLOG(ERR, "[%s:%d]handle point NULL, memory have not found\n",
                  __func__, __LINE__);
      return;
    }

    pCodecInst = handle;
    pDecInfo   = &(pCodecInst->CodecInfo->decInfo);

    if (!pDecInfo) {
      VLOG(ERR, "[%s:%d][core%d inst%d]pDecInfo point NULL, memory have been free\n",
                  __func__, __LINE__, pCodecInst->coreIdx, pCodecInst->instIndex);
      return;
    }
    bm_handle= bmvpu_dec_get_bmlib_handle(handle->coreIdx);
    EnterLock(pCodecInst->coreIdx);
    if (pDecInfo->vbDevSlice.size)
        {
          bm_free_mem(bm_handle,pDecInfo->vbDevSlice,pDecInfo->vbSliceVddr);
          pDecInfo->vbDevSlice.size=0;
        }

    if (pDecInfo->vbDevWork.size) {
        if (pDecInfo->workBufferAllocExt == 0) {
          bm_free_mem(bm_handle,pDecInfo->vbDevWork,pDecInfo->vbWorkVaddr);
          pDecInfo->vbDevWork.size=0;
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


    if (pDecInfo->vbDevReport.size)
       {
        bm_free_mem(bm_handle,pDecInfo->vbDevReport,pDecInfo->vbReportVddr);
        pDecInfo->vbDevReport.size=0;
       }

    if (GetPendingInst(pCodecInst->coreIdx) == pCodecInst)
        ClearPendingInst(pCodecInst->coreIdx);
    FreeCodecInstance(pCodecInst);
    LeaveLock(pCodecInst->coreIdx);
}

static int _syscxt_restart(bm_syscxt_t *p_syscxt) {

  int coreIdx = p_syscxt->coreid;
  int productId = 0;
  char *fwPath = NULL;
  uint16_t *pusBitCode = NULL;
  uint32_t sizeInWord = 0;
  RetCode ret = RETCODE_FAILURE;
  int res = 0;
  int i = 0;
  bm_handle_t bm_handle;
  if ((productId = VPU_GetProductId(coreIdx)) == -1) {
    VLOG(ERR, "[%s:%d]Failed to get product ID, PC=0x%x\n", __func__, __LINE__, vdi_read_register(coreIdx, 4));
    res = -1;
    goto restart_exit;
  }

  switch (productId) {
    case PRODUCT_ID_512:  fwPath = CORE_2_BIT_CODE_FILE_PATH; break;
    case PRODUCT_ID_511:  fwPath = CORE_7_BIT_CODE_FILE_PATH; break;
    default:
      VLOG(ERR, "[%s:%d]Unknown product id: %d, PC=0x%x\n", __func__, __LINE__, productId, vdi_read_register(coreIdx, 4));
      res = -1;
      goto restart_exit;
  }
  bm_handle= bmvpu_dec_get_bmlib_handle(coreIdx);
  VLOG(INFO, "[%s:%d]PC=0x%x\n", __func__, __LINE__, vdi_read_register(coreIdx, 4));
  if (LoadFirmware(productId, (Uint8 **)&pusBitCode, &sizeInWord, fwPath) < 0) {
    VLOG(ERR, "[%s:%d]Failed to load firmware: %s, PC=0x%x\n", __func__, __LINE__, fwPath, vdi_read_register(coreIdx, 4));
    res = -1;
    goto restart_exit;
  }

  for ( i = 0; i < MAX_NUM_INSTANCE; i++) {

    if (p_syscxt->vidHandle[i] == 0) {
      continue;
    }

    DecOpenParam *decOp =  &p_syscxt->dec_param[i];
    bm_device_mem_t vbStream = {0};

    ret = VPU_InitWithBitcode(coreIdx, (const Uint16 *)pusBitCode, sizeInWord);
    if (ret != RETCODE_CALLED_BEFORE && ret != RETCODE_SUCCESS) {
      printf("[%s:%d]Failed to boot up VPU(coreIdx: %d, productId: %d), PC=0x%x\n",
                                        __func__, __LINE__, coreIdx, productId, vdi_read_register(coreIdx, 4));
      res = -1;
      goto restart_exit;
    }

    vbStream.size = STREAM_BUF_SIZE;
    if (bmvpu_malloc_device_byte_heap(bm_handle,&vbStream,STREAM_BUF_SIZE,HEAP_MASK,1) !=BM_SUCCESS) {
      printf("[%s:%d]alloc mem fail, PC=0x%x\n", __func__, __LINE__, vdi_read_register(coreIdx, 4));
      res = -1;
      goto restart_exit;
    }


    decOp->bitstreamBuffer     = vbStream.u.device.device_addr;
    decOp->bitstreamBufferSize = vbStream.size;

    DecHandle handle;

    if ((ret = VPU_DecOpen(&handle, decOp) )!= RETCODE_SUCCESS) {
      printf("VPU_DecOpen failed Error code is 0x%x \n", ret);
      res = -1;
      goto restart_exit;
    }

    p_syscxt->vidHandle[i]->codecInst = handle;
    osal_memcpy(&(p_syscxt->vidHandle[i]->vbStream), &vbStream, sizeof(vbStream));

    p_syscxt->vidHandle[i]->frameInBuffer = 0;
    p_syscxt->vidHandle[i]->isStreamBufFilled = 0;
    p_syscxt->vidHandle[i]->seqInitFlag = 0;
    p_syscxt->vidHandle[i]->endof_flag = 0;
    p_syscxt->vidHandle[i]->streamWrAddr = vbStream.u.device.device_addr;
    p_syscxt->vidHandle[i]->decStatus = BMDEC_UNINIT;

    p_syscxt->vidHandle[i]->isStreamBufFilled = 0;
    p_syscxt->vidHandle[i]->seqInitFlag = 0;
  }

restart_exit:
  return res;
}

static void _syscxt_thread(void *arg) {

  bm_syscxt_t *p_syscxt = (bm_syscxt_t *)arg;
  int coreIdx = p_syscxt->coreid;
  int pcie_board_idx = 0;
  int i = 0;
  int index = 0;
  bm_handle_t  bm_handle;
  while(1) {
    osal_cond_lock(p_syscxt->cond_status);
    osal_cond_wait(p_syscxt->cond_status);
    osal_cond_unlock(p_syscxt->cond_status);

#ifdef TRY_FLOCK_OPEN
    get_lock_timeout(20,pcie_board_idx);
#else
    sem_t* vid_open_sem = sem_open(VID_OPEN_SEM_NAME, O_CREAT, 0666, 1);
    get_lock_timeout(vid_open_sem, TRY_OPEN_SEM_TIMEOUT);
#endif
    bm_handle= bmvpu_dec_get_bmlib_handle(coreIdx);
    printf("[* + *][core %d] start to reset vpu\n", coreIdx);

    for ( i = 0; i < MAX_NUM_INSTANCE; i++) {

      if (p_syscxt->vidHandle[i] == 0) {
        continue;
      }

      DecHandle handle = p_syscxt->vidHandle[i]->codecInst;
      _syscxt_release_mem(handle);

      for (index = 0; index < MAX_REG_FRAME; index++) {
        if (p_syscxt->vidHandle[i]->pFbMem[index].size > 0)
          {
            bm_free_mem(bm_handle,p_syscxt->vidHandle[i]->pFbMem[i],p_syscxt->vidHandle[i]->fbMemVaddr[i]);
          }
      }

      if (p_syscxt->vidHandle[i]->vbStream.size > 0)
      {
        bm_free_mem(bm_handle,p_syscxt->vidHandle[i]->vbStream,0x00);
      }

      VPU_DeInit(coreIdx);
    }

    p_syscxt->crst_res = 0;
    if(_syscxt_restart(p_syscxt) < 0) {
      printf("[* - *][core %d] vpu reset error!\n", coreIdx);
      p_syscxt->crst_res = -1;
    }

    printf("[* - *][core %d] vpu continue to work\n", coreIdx);
#ifdef TRY_FLOCK_OPEN
    unlock_flock(pcie_board_idx);
#else
    vidHandle->vid_open_sem = vid_open_sem;
    sem_post(vid_open_sem);
#endif

    vdi_crst_set_status(coreIdx, SYSCXT_STATUS_WORKDING);
    osal_cond_lock(p_syscxt->cond_sleep);
    for( i = 0; i < p_syscxt->instantNum * 2; i++) {
      osal_cond_signal(p_syscxt->cond_sleep);
    }
    osal_cond_unlock(p_syscxt->cond_sleep);
  }
}
#endif

void bm_syscxt_init(void *p_dec_param, BMVidCodHandle vidCodHandle) {

  #if defined(CHIP_BM1684)
  BMVidHandle vidHandle = (BMVidHandle)vidCodHandle;
  if ((!p_dec_param) || (!vidHandle)) {
    VLOG(ERR, "[%s:%d]para error\n", __func__, __LINE__);
    return ;
  }

  int instid = vidHandle->codecInst->instIndex;
  int coreid = vidHandle->codecInst->coreIdx;
  osal_memcpy(&g_syscxt[coreid].dec_param[instid], p_dec_param, sizeof(DecOpenParam));
  g_syscxt[coreid].vidHandle[instid] = vidHandle;
  g_syscxt[coreid].instantNum += 1;

  if (g_syscxt[coreid].sys_isinit)
    return;

  g_syscxt[coreid].sys_isinit = 1;
  g_syscxt[coreid].coreid = coreid;

  g_syscxt[coreid].cond_sleep = osal_cond_create();
  if (!g_syscxt[coreid].cond_sleep) {
    g_syscxt[coreid].sys_isinit = 0;
    return ;
  }

  g_syscxt[coreid].cond_status = osal_cond_create();
  if (!g_syscxt[coreid].cond_status) {
    g_syscxt[coreid].sys_isinit = 0;
    return ;
  }

  g_syscxt[coreid].cond_ex = osal_cond_create();
  if (!g_syscxt[coreid].cond_ex) {
    g_syscxt[coreid].sys_isinit = 0;
    return ;
  }

  g_syscxt[coreid].crst_res = 0;
  g_syscxt[coreid].thread_handle = osal_thread_create(_syscxt_thread, (void*)&g_syscxt[coreid]);
  #endif
}

void bm_syscxt_excepted(int coreid) {
  #if defined(CHIP_BM1684)
  osal_cond_lock(g_syscxt[coreid].cond_ex);
  vdi_crst_set_status(coreid, SYSCXT_STATUS_EXCEPTION);
  osal_cond_unlock(g_syscxt[coreid].cond_ex);
  #endif
}

void bm_syscxt_set(int coreid, int enable) {

  #if defined(CHIP_BM1684)
  enable = !!enable;
  vdi_crst_set_enable(coreid, enable);
  #endif
}

int bm_syscxt_status(int coreid, int instid, int pos) {

  #if defined(CHIP_BM1684)
  int is_sleep = -1, is_wakeup;
  int ret = vdi_crst_chk_status(coreid, instid, pos, &is_sleep, &is_wakeup);
  if (ret < 0) {
    VLOG(ERR, "[%s:%d]error happened!\n", __func__, __LINE__);
    return 0;
  }

  if (is_sleep) {
    g_syscxt[coreid].instcall[instid] |= is_sleep << pos;
    printf("[%s:%d]core %d inst %d, pos: %d, status: sleep:%d, wakeup:%d!\n",
              __func__, __LINE__, coreid, instid, pos, is_sleep, is_wakeup);
  }

  g_syscxt[coreid].instcall[instid] |= is_sleep << pos;
  while((_syscxt_chkinst(&g_syscxt[coreid]) == 1) && (is_wakeup == 0)) {

    ret = vdi_crst_chk_status(coreid, instid, pos, &is_sleep, &is_wakeup);
    if (ret < 0) {
      VLOG(ERR, "[%s:%d]error happened!\n", __func__, __LINE__);
      return 0;
    }
  }

  if(is_wakeup) {
    osal_cond_lock(g_syscxt[coreid].cond_status);
    osal_cond_signal(g_syscxt[coreid].cond_status);
    osal_cond_unlock(g_syscxt[coreid].cond_status);
  }

  if (is_sleep) {
    if (pos == 1) osal_cond_unlock(g_syscxt[coreid].cond_ex);
    osal_cond_lock(g_syscxt[coreid].cond_sleep);
    osal_cond_wait(g_syscxt[coreid].cond_sleep);
    osal_cond_unlock(g_syscxt[coreid].cond_sleep);

    if (pos == 1) osal_cond_lock(g_syscxt[coreid].cond_ex);
    g_syscxt[coreid].instcall[instid] = 0;
    return g_syscxt[coreid].crst_res < 0 ? -2 : -1;
  }
  #endif

  return 0;
}

int bm_syscxt_chkstatus(int coreid) {

  #if defined(CHIP_BM1684)
  int ret = vdi_crst_get_status(coreid);
  if (ret < 0) {
    VLOG(ERR, "[%s:%d]error happened!\n", __func__, __LINE__);
    return 0;
  }

  return ret == SYSCXT_STATUS_WORKDING ? 0 : -1;
  #else
  return 1;
  #endif
}

void bm_syscxt_statusLock(int coreid) {

  #if defined(CHIP_BM1684)
  osal_cond_lock(g_syscxt[coreid].cond_ex);
  #endif
}

void bm_syscxt_statusUnlock(int coreid) {

  #if defined(CHIP_BM1684)
  osal_cond_unlock(g_syscxt[coreid].cond_ex);
  #endif
}

void bm_syscxt_status_wakeup(int coreid) {

  #if defined(CHIP_BM1684)
  osal_cond_lock(g_syscxt[coreid].cond_sleep);
  osal_cond_signal(g_syscxt[coreid].cond_sleep);
  osal_cond_unlock(g_syscxt[coreid].cond_sleep);
  #endif
}
