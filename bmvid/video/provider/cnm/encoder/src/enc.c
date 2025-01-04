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
#include <string.h>

#include "bmvpu_types.h"
#include "bmvpu.h"
#include "bmvpu_logging.h"

#include "internal.h"
#include "wave5_regdefine.h"
#include "vpuapi.h"

#ifdef __linux__
#include <unistd.h>
#ifdef BM_PCIE_MODE
#include <pthread.h>
#endif
#elif _WIN32
#include <Windows.h>
#include <stdint.h>
//#include "wave5_regdefine.h"
#endif

#ifdef  _WIN32
extern unsigned int* p_vpu_enc_create_inst_flag[MAX_NUM_VPU_CORE];
#elif __linux__
extern int* p_vpu_enc_create_inst_flag[MAX_NUM_ENC_CORE];
#endif

/* common memory = | codebuf(1M) | tempBuf(1M) | taskbuf0x0 ~ 0xF | */
#define WAVE5_CODEBUF_OFFSET                0
#define WAVE5_CODEBUF_SIZE                  (1024*1024) /* ALIGN TO 4KB */
#define WAVE5_TEMPBUF_OFFSET                (WAVE5_CODEBUF_OFFSET+WAVE5_CODEBUF_SIZE)
#define WAVE5_TEMPBUF_SIZE                  (1024*1024)
#define WAVE5_TASK_BUF_OFFSET               (WAVE5_TEMPBUF_OFFSET+WAVE5_TEMPBUF_SIZE)

#define WAVE5_SUBSAMPLED_ONE_SIZE(_w, _h)       ((((_w/4)+15)&~15)*(((_h/4)+7)&~7))
#define WAVE5_AVC_SUBSAMPLED_ONE_SIZE(_w, _h)   ((((_w/4)+31)&~31)*(((_h/4)+3)&~3))

#define WTL_RIGHT_JUSTIFIED          0
#define WTL_LEFT_JUSTIFIED           1
#define WTL_PIXEL_8BIT               0
#define WTL_PIXEL_16BIT              1
#define WTL_PIXEL_32BIT              2

/**
 * @brief This structure is used for reporting bandwidth (only for WAVE5).
 */
typedef struct {
    uint32_t prpBwRead;
    uint32_t prpBwWrite;
    uint32_t fbdYRead;
    uint32_t fbcYWrite;
    uint32_t fbdCRead;
    uint32_t fbcCWrite;
    uint32_t priBwRead;
    uint32_t priBwWrite;
    uint32_t secBwRead;
    uint32_t secBwWrite;
    uint32_t procBwRead;
    uint32_t procBwWrite;
} VPUBWData;


typedef struct _tag_FramebufferIndex {
    int16_t tiledIndex;
    int16_t linearIndex;
} FramebufferIndex;

typedef struct _tag_VpuAttrStruct {
    uint32_t  productId;
    uint32_t  productNumber;
    char      productName[8];
    uint32_t  supportDecoders;            /* bitmask: See CodStd in vpuapi.h */
    uint32_t  supportEncoders;            /* bitmask: See CodStd in vpuapi.h */
    uint32_t  numberOfMemProtectRgns;     /* always 10 */
    uint32_t  numberOfVCores;

    BOOL      supportWTL;
    BOOL      supportTiled2Linear;
    BOOL      supportTiledMap;
    BOOL      supportMapTypes;            /* Framebuffer map type */
    BOOL      supportLinear2Tiled;        /* Encoder */
    BOOL      support128bitBus;
    BOOL      supportThumbnailMode;
    BOOL      supportBitstreamMode;
    BOOL      supportFBCBWOptimization;   /* WAVExxx decoder feature */
    BOOL      supportGDIHW;
    BOOL      supportCommandQueue;
    BOOL      supportBackbone;            /* Enhanced version of GDI */
    BOOL      supportNewTimer;            /* 0 : timeeScale=32768, 1 : timerScale=256 (tick = cycle/timerScale) */
    BOOL      support2AlignScaler;

    uint32_t  supportEndianMask;
    uint32_t  bitstreamBufferMargin;
    uint32_t  interruptEnableFlag;
    uint32_t  hwConfigDef0;
    uint32_t  hwConfigDef1;
    uint32_t  hwConfigFeature;            /**<< supported codec standards */
    uint32_t  hwConfigDate;               /**<< Configuation Date Decimal, ex)20151130 */
    uint32_t  hwConfigRev;                /**<< SVN revision, ex) 83521 */
    uint32_t  hwConfigType;               /**<< Not defined yet */
} VpuAttr;

static int32_t WaveVpuGetProductId(uint32_t coreIdx);

static void Wave5BitIssueCommand(EncHandle handle, uint32_t cmd);

static int Wave5VpuGetVersion(uint32_t  coreIdx, uint32_t* versionInfo, uint32_t* revision);

static int Wave5VpuInit(uint32_t coreIdx, void* firmware, uint32_t sizeInShort);

static int Wave5VpuSleepWake(uint32_t coreIdx, int iSleepWake,
                             const uint16_t* code, uint32_t sizeInShort, BOOL reset);

static int Wave5VpuReset(uint32_t coreIdx, SWResetMode resetMode);

static int Wave5VpuReInit(uint32_t coreIdx, void* firmware, uint32_t sizeInShort);

static int Wave5VpuWaitInterrupt(EncHandle handle, int32_t timeout);

static void Wave5VpuClearInterrupt(uint32_t coreIdx, uint32_t flags);

static int Wave5VpuGetProductInfo(uint32_t coreIdx, ProductInfo* productInfo);

static int Wave5VpuGetBwReport(EncHandle handle, VPUBWData* bwMon);


static int Wave5VpuBuildUpEncParam(EncHandle handle, VpuEncOpenParam* param);

static int Wave5VpuEncInitSeq(EncHandle handle);

static int Wave5VpuEncGetSeqInfo(EncHandle handle, VpuEncInitialInfo* info);

static int Wave5VpuEncRegisterCFramebuffer(EncHandle inst, VpuFrameBuffer* fbArr, uint32_t count);

static int Wave5VpuEncode(EncHandle handle, VpuEncParam* option);

static int Wave5VpuEncGetResult(EncHandle handle, VpuEncOutputInfo* result);

static int Wave5VpuEncGetHeader(EncHandle handle, VpuEncHeaderParam* encHeaderParam);

static int Wave5VpuEncFiniSeq(EncHandle  handle);
static int Wave5VpuEncParaChange(EncHandle handle, EncChangeParam* param);

static int Wave5ConfigSecAXI(uint32_t coreIdx, int32_t codecMode, SecAxiInfo* sa,
                             uint32_t width, uint32_t height,
                             uint32_t profile, uint32_t level);

static int CheckEncCommonParamValid(VpuEncOpenParam* pop);
static int CheckEncRcParamValid(VpuEncOpenParam* pop);
static int CheckEncCustomGopParamValid(VpuEncOpenParam* pop);

static int InitCodecInstancePool(uint32_t coreIdx);
static int GetCodecInstance(uint32_t coreIdx, EncHandle* ppInst);
static int FreeCodecInstance(EncHandle pInst);

static int CheckEncInstanceValidity(EncHandle handle);
static int CheckEncParam(EncHandle handle, VpuEncParam* param);

static int EnterLock(uint32_t coreIdx);
static int LeaveLock(uint32_t coreIdx);

static int32_t CalcStride(uint32_t width, uint32_t height,
                          FrameBufferFormat format, BOOL cbcrInterleave,
                          TiledMapType mapType);
static int32_t CalcLumaSize(int32_t stride, int32_t height,
                            FrameBufferFormat format, BOOL cbcrIntl,
                            TiledMapType mapType, DRAMConfig* pDramCfg);
static int32_t CalcChromaSize(int32_t stride, int32_t height,
                              FrameBufferFormat format, BOOL cbcrIntl,
                              TiledMapType mapType, DRAMConfig* pDramCfg);

static int CalcCropInfo(EncHandle handle, VpuEncOpenParam* openParam, int rotMirMode);

#if 0
/* timescale: 1/90000 */
static uint64_t GetTimestamp(EncHandle handle);
#endif

static int enc_open(EncHandle* handle, VpuEncOpenParam* encOpParam);
static int enc_close(EncHandle handle);
static int enc_calc_core_buffer_size(EncHandle handle, int num, VpuEncCoreBuffer* info);
static int enc_issue_seq_init(EncHandle handle);
static int enc_complete_seq_init(EncHandle handle, VpuEncInitialInfo* info);
static int enc_register_framebuffer(EncHandle handle, VpuFrameBuffer* bufArray, int num,
                                    int stride, int height);
static int enc_start_one_frame(EncHandle handle, VpuEncParam* param);
static int enc_get_output_info(EncHandle handle, VpuEncOutputInfo* info);
static int enc_give_command(EncHandle handle, CodecCommand cmd, void* parameter);
static bm_pa_t MapToAddr40Bit(int coreIdx, unsigned int Addr);

static BOOL    VpuScanProductId(uint32_t coreIdx);
static int32_t VpuGetProductId(uint32_t coreIdx);

static int vpu_sw_reset(uint32_t coreIdx, SWResetMode resetMode, EncHandle pInst);
static uint32_t VpuIsInit(uint32_t coreIdx);

static int VpuUpdateCFramebuffer(EncHandle handle, VpuFrameBuffer* fbArr,
                                 int32_t num, int32_t stride, int32_t height,
                                 FrameBufferFormat format);

static int VpuCheckEncOpenParam(VpuEncOpenParam* param);

static int enc_wait_bus_busy(uint32_t core_idx, int timeout, unsigned int gdi_busy_flag);
static int enc_wait_vpu_busy(uint32_t core_idx, int timeout, unsigned int addr_bit_busy_flag);
static int enc_wait_vcpu_bus_busy(uint32_t core_idx, int timeout, unsigned int addr_bit_busy_flag);

static uint32_t __VPU_BUSY_TIMEOUT = VPU_BUSY_CHECK_TIMEOUT;

static int presetGopSize[] = {
    1,  /* Custom GOP, Not used */
    1,  /* All Intra */
    1,  /* IPP Cyclic GOP size 1 */
    1,  /* IBB Cyclic GOP size 1 */
    2,  /* IBP Cyclic GOP size 2 */
    4,  /* IBBBP */
    4,
    4,
    8,
};

static VpuAttr s_VpuCoreAttributes[MAX_NUM_VPU_CORE];

#ifdef __linux__
static int32_t s_encProductIds[MAX_NUM_VPU_CORE] = { [0 ... MAX_NUM_VPU_CORE-1] = PRODUCT_ID_NONE };
#elif _WIN32
static int32_t s_encProductIds[MAX_NUM_VPU_CORE] = {0};
static int vpuInitFlag = 0; //if 1 means that s_encProductIds[MAX_NUM_VPU_CORE] does not need to be written as PRODUCT_ID_NONE
#endif

static int check_enc_handle(VpuEncoder* h)
{
    if (h == NULL) {
        VLOG(ERR, "Invalid handle!\n");
        return VPU_RET_INVALID_PARAM;
    }

    if (h->priv == NULL) {
        VLOG(ERR, "encoder is NOT opened!\n");
        return VPU_RET_INVALID_PARAM;
    }

    return VPU_RET_SUCCESS;
}

int vpu_EncGetUniCoreIdx(int soc_idx)
{
    int chip_id = 0;
    int bin_type  = 0;
    int video_cap = 0;
    int max_core_num = 0;
    // In some chips, sys0 and sys1 may have problems so index may be change
    // 1684 video_cap=0 enc index 4; video_cap=1/2 enc index 2; video_cap=3 enc index 0
    // 1686 video_cap=0 enc index 2; video_cap=1/2 enc index 1; video_cap=3 enc index 0
    // bin_type=0 fast bin ; bin_type=1 one sys bad; bin_type=0 pcie bad
    if(vdienc_get_video_cap( MAX_NUM_VPU_CORE_CHIP * soc_idx, &video_cap, &bin_type, &max_core_num ,&chip_id) < 0){
        VLOG(ERR,"vpu_EncGetUniCoreIdx get video cap failed!!!\n");
        return -1;
    }
    return (MAX_NUM_VPU_CORE_CHIP*soc_idx + max_core_num -1);
}

int vpu_EncInit(int soc_idx, void *cb_func)
{
    int coreIdx = vpu_EncGetUniCoreIdx(soc_idx);
    ProductInfo productInfo = {0};
    int productId = 0;
    const char* fw_fname = CORE_6_BIT_CODE_FILE_PATH;
    char fw_path[512] = {0};
    uint16_t *pusBitCode = NULL;
    uint32_t sizeInWord = 0;
    uint32_t ver, rev;
    int ret;

    if (coreIdx < 0)
        return -1;
    else
        VLOG(ERR,"sophon_idx %d, VPU core index %d\n", soc_idx, ENC_CORE_IDX);

 #ifdef _WIN32
    while (!InterlockedCompareExchange(&vpuInitFlag,1,0)) {
            for (int idx = 0; idx < MAX_NUM_VPU_CORE; idx++) {
                s_encProductIds[idx] = PRODUCT_ID_NONE;
            }
    }
#endif
      productId = vpu_GetProductId(coreIdx);
    if (productId == -1)
    {
        VLOG(ERR, "Failed to get product ID for VPU core %d\n", coreIdx);
        return -1;
    }
    if (productId != PRODUCT_ID_521)
    {
        VLOG(ERR, "Unknown product id: %d\n", productId);
        return -1;
    }

    /* for wave521 */
    ret = find_firmware_path(fw_path, fw_fname);
    if (ret < 0)
    {
        VLOG(ERR, "Failed to find firmware: %s\n", fw_fname);
        return -1;
    }

    ret = load_firmware((uint8_t **)&pusBitCode, &sizeInWord, fw_path);
    if (ret < 0)
    {
        VLOG(ERR, "Failed to load firmware: %s\n", fw_path);
        return -1;
    }

    ret = vpu_InitWithBitcode(coreIdx, (const uint16_t *)pusBitCode, sizeInWord);
    if (ret != VPU_RET_CALLED_BEFORE && ret != VPU_RET_SUCCESS)
    {
        VLOG(ERR, "Failed to boot up VPU(coreIdx: %d, productId: %d)\n", coreIdx, productId);
        free((uint8_t*)pusBitCode);
        return -1;
    }
    free((uint8_t*)pusBitCode);

    VLOG(INFO, "VPU Firmware is successfully loaded!\n");

    vpu_GetVersionInfo(coreIdx, &ver, &rev, NULL);

    VLOG(INFO, "VPU FW VERSION=0x%x, REVISION=%d\n", ver, rev);

    ret = vpu_ShowProductInfo(coreIdx, &productInfo);
    if (ret != VPU_RET_SUCCESS)
    {
        VLOG(ERR, "vpu_ShowProductInfo failed\n");
    }

    return ret;
}

int vpu_EncUnInit(int soc_idx)
{
    int coreIdx = vpu_EncGetUniCoreIdx(soc_idx);
    int ret;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    ret = bm_vdi_release(coreIdx);
    if (ret != 0)
        return VPU_RET_FAILURE;

    return VPU_RET_SUCCESS;
}


uint32_t vpu_GetFirmwareStatus(int soc_idx)
{
    int coreIdx = vpu_EncGetUniCoreIdx(soc_idx);

    return bm_vdi_get_firmware_status(coreIdx);
}

int vpu_GetVersionInfo(int coreIdx, uint32_t* versionInfo, uint32_t* revision, uint32_t* productId)
{
    int  ret;

    EnterLock(coreIdx);

    if (VpuIsInit(coreIdx) == 0) {
        LeaveLock(coreIdx);
        return VPU_RET_NOT_INITIALIZED;
    }

    if (productId != NULL) {
        *productId = VpuGetProductId(coreIdx);
    }
    ret = Wave5VpuGetVersion(coreIdx, versionInfo, revision);

    LeaveLock(coreIdx);

    return ret;
}

int vpu_EncIsBusy(int soc_idx)
{
    int coreIdx = vpu_EncGetUniCoreIdx(soc_idx);
    uint32_t ret = 0;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return -1;

    ret = VpuReadReg(coreIdx, W5_VPU_BUSY_STATUS);

    return ret != 0;
}

int vpu_EncSWReset(VpuEncoder* h, int reset_mode)
{
    EncHandle handle = NULL;
    int ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);
    return vpu_sw_reset(h->coreIdx, reset_mode, handle);
}

/**
 * Init VpuEncOpenParam by runtime evaluation
 *
 * parameters:
 *   bit_rate: in unit of bps
 */
void vpu_SetEncOpenParam(VpuEncOpenParam* pEncOP, int width, int height,
                         int fps_n, int fps_d, int64_t bit_rate, int cqp)
{
    VpuEncWaveParam *wave_par = &(pEncOP->waveParam);
    int i;

    pEncOP->picWidth        = width;
    pEncOP->picHeight       = height;
    /* do not support 29.97 frame rate, use 30fps */
    pEncOP->frameRateInfo   = (fps_n + (fps_d >> 1)) / fps_d;
    //pEncOP->frameRateInfo   = (((fps_d-1) & 0xffffUL) << 16) | (fps_n & 0xffffUL);

    pEncOP->rcEnable        = (bit_rate > 0) ? 1 : 0;
    pEncOP->bitRate         = bit_rate;
    pEncOP->vbvBufferSize   = 3000; // TODO

    pEncOP->cbcrInterleave = FALSE;
    pEncOP->cbcrOrder      = 0;
    pEncOP->nv21           = 0;

    pEncOP->lineBufIntEn   = TRUE;

    /* pass through the pts of input source */
    pEncOP->enablePTS = 0;

    /* The input source frames is yuv format, not cframe50 */
    pEncOP->cframe50Enable          = 0;
    pEncOP->cframe50LosslessEnable  = 0;
    pEncOP->cframe50Tx16Y           = 0;
    pEncOP->cframe50Tx16C           = 0;
    pEncOP->cframe50_422            = 0;

#ifdef CHIP_BM1684
    /* No secondary AXI in BM1684 */
    pEncOP->secondaryAXI = 0;
#else
    /* use RDO and LF internal buffer */
    pEncOP->secondaryAXI = 3;
#endif

    memset(wave_par, 0, sizeof(VpuEncWaveParam));

    /* H.265 at default */
    wave_par->profile = H265_TAG_PROFILE_MAIN;
    wave_par->level   = H26X_LEVEL(4, 1); /* at most 1920x1088 */
    wave_par->tier    = H265_TIER_MAIN;

    wave_par->internalBitDepth   = 8;
    wave_par->losslessEnable     = 0;
    wave_par->constIntraPredFlag = 0;
    wave_par->useLongTerm        = 0;

    /* for W5_CMD_ENC_SEQ_GOP_PARAM */
    wave_par->gopPresetIdx = VPU_GOP_PRESET_IDX_IBBBP;    /* gopsize = 4 */

    int gopsize = presetGopSize[wave_par->gopPresetIdx];
    wave_par->intraPeriod         = ((28+gopsize-1)/gopsize)*gopsize; // TODO

    /* for W5_CMD_ENC_SEQ_INTRA_PARAM */
    wave_par->decodingRefreshType = 1;
    if (cqp > 0)
        wave_par->intraQP         = cqp;
    else
        wave_par->intraQP         = 28;

    /* for W5_CMD_ENC_SEQ_CONF_WIN_TOP_BOT/LEFT_RIGHT */
    wave_par->confWinTop   = 0;
    wave_par->confWinBot   = 0;
    wave_par->confWinLeft  = 0;
    wave_par->confWinRight = 0;

    /* for CMD_ENC_SEQ_INDEPENDENT_SLICE */
    wave_par->independSliceMode     = 0;
    wave_par->independSliceModeArg  = 0;

    /* for CMD_ENC_SEQ_DEPENDENT_SLICE */
    wave_par->dependSliceMode     = 0;
    wave_par->dependSliceModeArg  = 0;

    /* for CMD_ENC_SEQ_INTRA_REFRESH_PARAM */
    wave_par->intraRefreshMode     = 0;
    wave_par->intraRefreshArg      = 0;

    wave_par->cuSizeMode = 0x7;

    /* for CMD_ENC_PARAM
     * 0 : Custom,
     * 1 : recommended encoder parameters (slow encoding speed, highest picture quality)
     * 2 : Boost mode (normal encoding speed, normal picture quality),
     * 3 : Fast mode (high encoding speed, low picture quality) */
    wave_par->useRecommendEncParam = 1;
    if (wave_par->useRecommendEncParam != 1) {
        wave_par->scalingListEnable        = 0;
        wave_par->tmvpEnable               = 1;
        wave_par->wppEnable                = 0;
        wave_par->maxNumMerge              = 2;
        wave_par->disableDeblk             = 0;
        wave_par->lfCrossSliceBoundaryEnable = 1;
        wave_par->betaOffsetDiv2           = 0;
        wave_par->tcOffsetDiv2             = 0;
        wave_par->skipIntraTrans           = 1;
        wave_par->saoEnable                = 1;
        wave_par->intraNxNEnable           = 1;
    }

    /* for W5_CMD_ENC_SEQ_RC_PARAM */
    // wave_par->roiEnable    = 0;
    wave_par->bitAllocMode = 0;
    for (i = 0; i < MAX_GOP_NUM; i++)
        wave_par->fixedBitRatio[i] = 1;
    wave_par->cuLevelRCEnable       = 1;
    wave_par->hvsQPEnable           = 1;
    wave_par->hvsQpScale            = 2;

    /* for W5_CMD_ENC_SEQ_RC_MIN_MAX_QP */
    wave_par->minQpI            = 8;
    wave_par->minQpP            = 8;
    wave_par->minQpB            = 8;
    wave_par->maxQpI            = 51;
    wave_par->maxQpP            = 51;
    wave_par->maxQpB            = 51;

    wave_par->maxDeltaQp        = 5;

    /* for CMD_ENC_CUSTOM_GOP_PARAM */
    wave_par->gopParam.customGopSize     = 0;

    for (i= 0; i<wave_par->gopParam.customGopSize; i++) {
        wave_par->gopParam.picParam[i].picType      = 0;
        wave_par->gopParam.picParam[i].pocOffset    = 1;
        if (cqp > 0)
            wave_par->gopParam.picParam[i].picQp    = cqp;
        else
            wave_par->gopParam.picParam[i].picQp    = 30;
        wave_par->gopParam.picParam[i].refPocL0     = 0;
        wave_par->gopParam.picParam[i].refPocL1     = 0;
        wave_par->gopParam.picParam[i].temporalId   = 0;
    }

    /* for VUI / time information. */
    wave_par->numTicksPocDiffOne   = 0;
    if (pEncOP->bitstreamFormat == VPU_CODEC_AVC)
        wave_par->timeScale            = pEncOP->frameRateInfo * 1000 * 2;
    else
        wave_par->timeScale            = pEncOP->frameRateInfo * 1000;

    wave_par->numUnitsInTick       = 1000;

    wave_par->chromaCbQpOffset = 0;
    wave_par->chromaCrQpOffset = 0;

    wave_par->initialRcQp      = 63; // 63 is meaningless.

    wave_par->monochromeEnable            = 0;
    wave_par->strongIntraSmoothEnable     = 1;
    wave_par->weightPredEnable            = 1; // TODO

    /* Noise reduction */
    wave_par->nrYEnable        = 1; // TODO
    wave_par->nrCbEnable       = 1;
    wave_par->nrCrEnable       = 1;
    wave_par->nrNoiseEstEnable = 1;

    /* Background detection */
    wave_par->bgDetectEnable              = 0; // TODO
    wave_par->bgThrDiff                   = 8;
    wave_par->bgThrMeanDiff               = 1;
    wave_par->bgLambdaQp                  = 32;
    wave_par->bgDeltaQp                   = 3;

    wave_par->customLambdaEnable          = 0;
    wave_par->customMDEnable              = 0;
    wave_par->pu04DeltaRate               = 0;
    wave_par->pu08DeltaRate               = 0;
    wave_par->pu16DeltaRate               = 0;
    wave_par->pu32DeltaRate               = 0;
    wave_par->pu04IntraPlanarDeltaRate    = 0;
    wave_par->pu04IntraDcDeltaRate        = 0;
    wave_par->pu04IntraAngleDeltaRate     = 0;
    wave_par->pu08IntraPlanarDeltaRate    = 0;
    wave_par->pu08IntraDcDeltaRate        = 0;
    wave_par->pu08IntraAngleDeltaRate     = 0;
    wave_par->pu16IntraPlanarDeltaRate    = 0;
    wave_par->pu16IntraDcDeltaRate        = 0;
    wave_par->pu16IntraAngleDeltaRate     = 0;
    wave_par->pu32IntraPlanarDeltaRate    = 0;
    wave_par->pu32IntraDcDeltaRate        = 0;
    wave_par->pu32IntraAngleDeltaRate     = 0;
    wave_par->cu08IntraDeltaRate          = 0;
    wave_par->cu08InterDeltaRate          = 0;
    wave_par->cu08MergeDeltaRate          = 0;
    wave_par->cu16IntraDeltaRate          = 0;
    wave_par->cu16InterDeltaRate          = 0;
    wave_par->cu16MergeDeltaRate          = 0;
    wave_par->cu32IntraDeltaRate          = 0;
    wave_par->cu32InterDeltaRate          = 0;
    wave_par->cu32MergeDeltaRate          = 0;
    wave_par->coefClearDisable            = 0;

    /* For H.264 encoder */
    wave_par->rdoSkip                      = 0;
    wave_par->lambdaScalingEnable          = 0;
    wave_par->transform8x8Enable           = 1;
    wave_par->avcSliceMode                 = 0;
    wave_par->avcSliceArg                  = 0;
    wave_par->intraMbRefreshMode           = 0;
    wave_par->intraMbRefreshArg            = 1;
    wave_par->mbLevelRcEnable              = 1;
    wave_par->entropyCodingMode            = 1;
}

int vpu_EncOpen(VpuEncoder** h, VpuEncOpenParam* pEncOP)
{
    EncHandle* handle = NULL;
    SecAxiUse  secAxiUse = {0};
    int i, ret;

    ret = vpu_InitLog();
    if (ret < 0) {
        VLOG(ERR, "vpu_InitLog failed\n");
        ret = VPU_RET_FAILURE;
        goto failed;
    }

    (*h) = calloc(1, sizeof(VpuEncoder));
    if ((*h) == NULL) {
        VLOG(ERR, "calloc failed\n");
        ret = VPU_RET_FAILURE;
        goto failed;
    }

    (*h)->socIdx  = pEncOP->socIdx;
    (*h)->coreIdx = pEncOP->coreIdx;
    (*h)->findFirstIframe = 0;
    (*h)->fistIFramePTS = 0;
    for (i=0; i<32; i++)
        (*h)->inputMap[i].idx  = -2;

    handle = (EncHandle*)(&((*h)->priv));
    ret = enc_open(handle, pEncOP);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "enc_open failed\n");
        goto failed;
    }

    if(pEncOP->waveParam.gopPresetIdx == 4)  // 4: I-B-P-B-P,
        (* h)->bframe_delay = 1;
    else if(pEncOP->waveParam.gopPresetIdx == 5) // 5: I-B-B-B-P,
        (* h)->bframe_delay = 2;
    else if(pEncOP->waveParam.gopPresetIdx == 8) // Random Access, I-B-B-B-B-B-B-B-B, cyclic gopsize = 8 ?
        (* h)->bframe_delay = presetGopSize[pEncOP->waveParam.gopPresetIdx]-2; // TODO
    else
        (* h)->bframe_delay = 0;

    secAxiUse.useEncRdoEnable = (pEncOP->secondaryAXI & 0x1) ? TRUE : FALSE;
    secAxiUse.useEncLfEnable  = (pEncOP->secondaryAXI & 0x2) ? TRUE : FALSE;

    ret = enc_give_command(*handle, SET_SEC_AXI, (void*)(&secAxiUse));
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "enc_give_command - set sec axi - failed\n");
        goto failed;
    }

    return VPU_RET_SUCCESS;

failed:
    if ((*h) != NULL)
        free(*h);
    return ret;
}

int vpu_EncClose(VpuEncoder* h)
{
    EncHandle handle = NULL;
    int ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);

    //enc_give_command(handle, ENABLE_LOGGING, NULL);

    ret = enc_close(handle);

    //enc_give_command(handle, DISABLE_LOGGING, NULL);

    free(h);

    vpu_DeInitLog();

    return ret;
}

static int VpuHandlingInterruptFlag(EncHandle handle, int intrWaitTime)
{
    int32_t  reason = 0;
    uint64_t startTime;
#ifdef __linux__
    startTime = vpu_gettime();
#elif _WIN32
	startTime = GetTickCount();
#endif
    reason = Wave5VpuWaitInterrupt(handle, intrWaitTime);
    if (reason == -1) {
#ifdef __linux__
        uint64_t currentTime = vpu_gettime();
#elif _WIN32
		uint64_t currentTime = GetTickCount();
#endif
		if ((currentTime - startTime) > VPU_ENC_TIMEOUT) { // TODO
            VLOG(ERR, "Instance %d, startTime(%lld), currentTime(%lld), diff(%d)\n",
                 handle->instIndex,
                 startTime, currentTime, (uint32_t)(currentTime - startTime));
            return VPU_ENC_INTR_STATUS_TIMEOUT;
        }
        VLOG(ERR, "Instance %d, INTR: reason=-1\n", handle->instIndex);
        return VPU_ENC_INTR_STATUS_NONE;
    }
    if (reason == 0) {
        VLOG(ERR, "Instance %d, INTR: reason=0\n", handle->instIndex);
        return VPU_ENC_INTR_STATUS_NONE;
    }
    if (reason < 0) {
        VLOG(ERR, "Instance %d, INTR: reason(%d) is negative value!\n",
             handle->instIndex, reason);
        return VPU_ENC_INTR_STATUS_NONE;
    }

    Wave5VpuClearInterrupt(handle->coreIdx, reason);

    if (reason & (1<<INT_WAVE5_ENC_SET_PARAM)) {
        VLOG(DEBUG, "INTR Done: set param. reason=0x%0x\n", reason);
        return VPU_ENC_INTR_STATUS_DONE;
    }

    if (reason & (1<<INT_WAVE5_ENC_PIC)) {
        VLOG(DEBUG, "INTR Done: enc pic. reason=0x%0x\n", reason);
        return VPU_ENC_INTR_STATUS_DONE;
    }

    if (reason & (1<<INT_WAVE5_BSBUF_FULL)) {
        VLOG(ERR, "INTR: bs buffer is full. reason=0x%0x\n", reason);
        return VPU_ENC_INTR_STATUS_FULL;
    }

    if (reason & (1<<INT_WAVE5_ENC_LOW_LATENCY)) {
        VLOG(ERR, "INTR: low latency. reason=0x%0x\n", reason);
        return VPU_ENC_INTR_STATUS_LOW_LATENCY;
    }

    VLOG(ERR, "INTR: None. reason=0x%0x\n", reason);
    return VPU_ENC_INTR_STATUS_NONE;
}

int vpu_EncSetSeqInfo(VpuEncoder* h, VpuEncInitialInfo* pinfo)
{
    EncHandle handle = NULL;
#if defined(PALLADIUM_SIM)
    uint32_t intrWaitTime = 30000;
#else
    uint32_t intrWaitTime = 2999; // ms
#endif
    int ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);

    do {
        ret = enc_issue_seq_init(handle);
    } while (ret == VPU_RET_QUEUEING_FAILURE);

    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "enc_issue_seq_init() failed! ret(%d)\n", ret);
        return ret;
    }

    int status = VpuHandlingInterruptFlag(handle, intrWaitTime);
    if (status == VPU_ENC_INTR_STATUS_NONE) {
        VLOG(ERR, "Instance #%d interrupt none\n", handle->instIndex);
        return VPU_RET_FAILURE;
    }
    if (status == VPU_ENC_INTR_STATUS_TIMEOUT) {
        VLOG(ERR, "Instance #%d interrupt timeout\n", handle->instIndex);
        return VPU_RET_FAILURE;
    }
    if (status != VPU_ENC_INTR_STATUS_DONE) {
        VLOG(ERR, "Instance #%d Unknown interrupt status: %d\n", handle->instIndex, status);
        return VPU_RET_FAILURE;
    }

    ret = enc_complete_seq_init(handle, pinfo);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "enc_complete_seq_init() failed! ret(%d), SEQERR(%08x)\n",
             ret, pinfo->seqInitErrReason);
        return ret;
    }

    VLOG(DEBUG, "minFrameBufferCount = %d, minSrcFrameCount = %d, "
         "maxLatencyPictures = %d, seqInitErrReason = %d, warnInfo = %d\n",
         pinfo->minFrameBufferCount, pinfo->minSrcFrameCount,
         pinfo->maxLatencyPictures, pinfo->seqInitErrReason, pinfo->warnInfo);

    pinfo->reconFbNum = pinfo->minFrameBufferCount;
    pinfo->srcFbNum   = pinfo->minSrcFrameCount + COMMAND_QUEUE_DEPTH;

    VLOG(DEBUG, "reconFbCount=%d, srcFbCount=%d\n",
         pinfo->reconFbNum, pinfo->srcFbNum);

    return VPU_RET_SUCCESS;
}

#if 0
static void print_EncParam(VpuEncParam* opt)
{
    VLOG(ERR, "opt->skipPicture             = %d\n", opt->skipPicture             );

    VLOG(ERR, "opt->picStreamBufferAddr     = 0x%lx\n", opt->picStreamBufferAddr     );
    VLOG(ERR, "opt->picStreamBufferSize     = %d\n", opt->picStreamBufferSize     );

    VLOG(ERR, "opt->forcePicQpEnable        = %d\n", opt->forcePicQpEnable        );
    VLOG(ERR, "opt->forcePicQpI             = %d\n", opt->forcePicQpI             );
    VLOG(ERR, "opt->forcePicQpP             = %d\n", opt->forcePicQpP             );
    VLOG(ERR, "opt->forcePicQpB             = %d\n", opt->forcePicQpB             );
    VLOG(ERR, "opt->forcePicTypeEnable      = %d\n", opt->forcePicTypeEnable     );

    VLOG(ERR, "opt->forcePicType            = %d\n", opt->forcePicType            );
    VLOG(ERR, "opt->forceAllCtuCoefDropEnable %d\n", opt->forceAllCtuCoefDropEnable);

    VLOG(ERR, "opt->srcIdx                  = %d\n", opt->srcIdx                  );
    VLOG(ERR, "opt->srcEndFlag              = %d\n", opt->srcEndFlag              );

    VLOG(ERR, "opt->useCurSrcAsLongtermPic  = %d\n", opt->useCurSrcAsLongtermPic  );
    VLOG(ERR, "opt->useLongtermRef          = %d\n", opt->useLongtermRef          );
    VLOG(ERR, "opt->pts                     = %d\n" , opt->pts                    );
}
#endif

#if 0
static void print_frame_buffer(VpuFrameBuffer* fb)
{
    VLOG(ERR, "fb->bufY           = 0x%lx\n", fb->bufY);
    VLOG(ERR, "fb->bufCb          = 0x%lx\n", fb->bufCb);
    VLOG(ERR, "fb->bufCr          = 0x%lx\n", fb->bufCr);

    VLOG(ERR, "fb->cbcrInterleave = %d\n", fb->cbcrInterleave);
    VLOG(ERR, "fb->nv21           = %d\n", fb->nv21);

    VLOG(ERR, "fb->endian         = %d\n", fb->endian);

    VLOG(ERR, "fb->myIndex        = %d\n", fb->myIndex);
    VLOG(ERR, "fb->mapType        = %d\n", fb->mapType);

    VLOG(ERR, "fb->stride         = %d\n", fb->stride);
    VLOG(ERR, "fb->width          = %d\n", fb->width);
    VLOG(ERR, "fb->height         = %d\n", fb->height);
    VLOG(ERR, "fb->size           = %d\n", fb->size);

    VLOG(ERR, "fb->lumaBitDepth   = %d\n", fb->lumaBitDepth);
    VLOG(ERR, "fb->chromaBitDepth = %d\n", fb->chromaBitDepth);

    VLOG(ERR, "fb->format         = %d\n", fb->format);

    VLOG(ERR, "fb->sequenceNo     = %d\n", fb->sequenceNo);
    VLOG(ERR, "fb->updateFbInfo   = %d\n", fb->updateFbInfo);
}
#endif


int vpu_EncStartOneFrame(VpuEncoder* h, VpuEncParam* param)
{
    EncHandle handle = NULL;
    int i, ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    if (param->skipPicture == 0 && param->sourceFrame == NULL) {
        VLOG(ERR, "sourceFrame should not be NULL when current picture is not skipped\n");
        return VPU_RET_INVALID_PARAM;
    }

    for (i=0; i<32; i++) {
        if (h->inputMap[i].idx == -2) {
            h->inputMap[i].idx     = param->srcIdx;
            h->inputMap[i].context = param->context;
            h->inputMap[i].pts     = param->pts;
            h->inputMap[i].dts     = param->dts;
            h->inputMap[i].addrCustomMap = param->customMapOpt.addrCustomMap;
            break;
        }
    }

    handle = (EncHandle)(h->priv);
    // PrintEncVpuStatus(handle);

    //enc_give_command(handle, ENABLE_LOGGING, NULL);

    //print_EncParam(param);

    ret = enc_start_one_frame(handle, param);

    //enc_give_command(handle, DISABLE_LOGGING, NULL);

    return ret;
}

int vpu_EncGetOutputInfo(VpuEncoder* h, VpuEncOutputInfo* info)
{
    EncHandle handle = NULL;
    //VpuEncOpenParam* opt = NULL;
    int i, ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);
    //opt = &(handle->encInfo->openParam);

    ret = enc_get_output_info(handle, info);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    if (info->bitstreamSize > 0 && info->encSrcIdx >= 0) {
        for (i=0; i<32; i++) {
            if (h->inputMap[i].idx == info->encSrcIdx) {
                info->context = h->inputMap[i].context;
                info->pts     = h->inputMap[i].pts;
                if (h->findFirstIframe == 0 && info->picType == 0) {
                    h->findFirstIframe = 1;
                    h->fistIFramePTS = info->pts;
                }
                // TODO
                info->dts = info->encPicCnt-1-h->bframe_delay+h->fistIFramePTS;
                info->addrCustomMap = h->inputMap[i].addrCustomMap;

                /* release */
                h->inputMap[i].idx = -2;
                break;
            }
        }
    }

    //print_frame_buffer(&(info->reconFrame));

#if defined(PROFILE_PERFORMANCE)
    VLOG(TRACE, "inst %02d: %5d %5d %5d    %8d  %2d    %8d %6d (%6d,%6d,%8d) (%6d,%6d,%8d) (%6d,%6d,%8d)\n",
         handle->instIndex, info->encPicCnt, info->picType, info->reconFrameIndex,
         info->bitstreamSize, info->encSrcIdx,
         info->frameCycle, info->encHostCmdTick,
         info->encPrepareStartTick, info->encPrepareEndTick, info->prepareCycle,
         info->encProcessingStartTick, info->encProcessingEndTick, info->processing,
         info->encEncodeStartTick, info->encEncodeEndTick, info->EncodedCycle);
#else
    VLOG(TRACE, "inst %02d: %5d %5d %5d    %8d  %2d\n",
         handle->instIndex, info->encPicCnt, info->picType, info->reconFrameIndex,
         info->bitstreamSize, info->encSrcIdx);
#endif

    return ret;
}

#if 0
int vpu_EncGiveCommand(VpuEncoder* h, int cmd, void* p_par)
{
    EncHandle handle = NULL;
    int ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);
    return enc_give_command(handle, cmd, p_par);
}
#endif

int vpu_EncPutVideoHeader(VpuEncoder* h, VpuEncHeaderParam* par)
{
    EncHandle handle = NULL;
    VpuEncOutputInfo header_info = {0};
#if defined(PALLADIUM_SIM)
    uint32_t intrWaitTime = 10000;
#else
    uint32_t intrWaitTime = 2999; // ms
#endif
    int ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }
    handle = (EncHandle)(h->priv);

    if (par == NULL) {
        VLOG(ERR, "Invalid parameter!\n");
        return VPU_RET_INVALID_PARAM;
    }

    if (par->buf == 0) {
        VLOG(ERR, "Invalid physical address!\n");
        return VPU_RET_INVALID_PARAM;
    }

    if ((par->headerType>>2) & (~7)) {
        VLOG(ERR, "Invalid headerType(0x%x)!\n", par->headerType);
        return VPU_RET_INVALID_PARAM;
    }


    do {
        ret = enc_give_command(handle, ENC_PUT_VIDEO_HEADER, par);
    } while (ret == VPU_RET_QUEUEING_FAILURE);

    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "enc_give_command(ENC_PUT_VIDEO_HEADER) failed! ret(%d)\n", ret);
        return ret;
    }

    int status = VpuHandlingInterruptFlag(handle, intrWaitTime);
    if (status == VPU_ENC_INTR_STATUS_NONE) {
        VLOG(ERR, "Instance #%d interrupt none\n", handle->instIndex);
        return VPU_RET_FAILURE;
    }
    if (status == VPU_ENC_INTR_STATUS_TIMEOUT) {
        VLOG(ERR, "Instance #%d interrupt timeout\n", handle->instIndex);
        return VPU_RET_FAILURE;
    }
    if (status != VPU_ENC_INTR_STATUS_DONE) {
        VLOG(ERR, "Instance #%d Unknown interrupt status: %d\n", handle->instIndex, status);
        return VPU_RET_FAILURE;
    }

    ret = enc_get_output_info(handle, &header_info);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    if (par->buf != header_info.bitstreamBuffer) {
        VLOG(ERR, "par->buf=0x%lx\n", par->buf);
        VLOG(ERR, "header_info.bitstreamBuffer=0x%lx\n", header_info.bitstreamBuffer);
        VLOG(ERR, "the physical address between input and output does not match!\n");
        return VPU_RET_FAILURE;
    }

    par->size = header_info.bitstreamSize;

    return ret;
}

int vpu_EncCalcCoreBufferSize(VpuEncoder* h, int num_framebuffer)
{
    EncHandle handle = NULL;
    VpuEncCoreBuffer core_buff = {0};
    int ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);

    ret = enc_calc_core_buffer_size(handle, num_framebuffer, &core_buff);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "enc_calc_core_buffer_size failed\n");
        return ret;
    }

    h->vbMV.size        = core_buff.MV.size;
    h->vbFbcYTbl.size   = core_buff.FbcYTbl.size;
    h->vbFbcCTbl.size   = core_buff.FbcCTbl.size;
    h->vbSubSamBuf.size = core_buff.SubSamBuf.size;

    h->vbMV.pa        = 0;
    h->vbFbcYTbl.pa   = 0;
    h->vbFbcCTbl.pa   = 0;
    h->vbSubSamBuf.pa = 0;

    return VPU_RET_SUCCESS;
}

/* Register compressed frame buffer */
int vpu_EncRegisterFrameBuffer(VpuEncoder* h, VpuFrameBuffer* bufArray, int num_framebuffer)
{
    EncHandle handle = NULL;
    int stride, height;
    int i, ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    if (h->vbMV.pa == 0) {
        VLOG(ERR, "MV buffer is NULL!\n");
        return ret;
    }
    if (h->vbFbcYTbl.pa == 0) {
        VLOG(ERR, "FBC Y Tbl buffer is NULL!\n");
        return ret;
    }
    if (h->vbFbcCTbl.pa == 0) {
        VLOG(ERR, "FBC C Tbl buffer is NULL!\n");
        return ret;
    }
    if (h->vbSubSamBuf.pa == 0) {
        VLOG(ERR, "Subsample buffer is NULL!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS)
        return ret;

    EncInfo* pEncInfo = handle->encInfo;
    if (pEncInfo->stride)
        return VPU_RET_CALLED_BEFORE;

    if (!pEncInfo->initialInfoObtained)
        return VPU_RET_WRONG_CALL_SEQUENCE;

    if (num_framebuffer < pEncInfo->initialInfo.minFrameBufferCount)
        return VPU_RET_INSUFFICIENT_FRAME_BUFFERS;

    if (!bufArray)
        return VPU_RET_INVALID_PARAM;

    stride = bufArray[0].stride;
    if (stride <= 0 || (stride % 8 != 0))
        return VPU_RET_INVALID_STRIDE;

    height = bufArray[0].height;
    if (height <= 0)
        return VPU_RET_INVALID_PARAM;

    for (i=0; i<num_framebuffer; i++) {
        if (bufArray[i].bufY == (bm_pa_t)-1)
            return VPU_RET_INVALID_PARAM;
    }

    VpuEncOpenParam* openParam = &pEncInfo->openParam;

    if (openParam->bitstreamFormat == VPU_CODEC_HEVC &&
        stride % 32 != 0) {
        return VPU_RET_INVALID_STRIDE;
    }

    openParam->MV.pa          = h->vbMV.pa;
    openParam->MV.size        = h->vbMV.size;

    openParam->FbcYTbl.pa     = h->vbFbcYTbl.pa;
    openParam->FbcYTbl.size   = h->vbFbcYTbl.size;

    openParam->FbcCTbl.pa     = h->vbFbcCTbl.pa;
    openParam->FbcCTbl.size   = h->vbFbcCTbl.size;

    openParam->SubSamBuf.pa   = h->vbSubSamBuf.pa;
    openParam->SubSamBuf.size = h->vbSubSamBuf.size;

    ret = enc_register_framebuffer(handle, bufArray, num_framebuffer,
                                   stride, height);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "enc_register_framebuffer failed\n");
    }

    return ret;
}

/**
 * Wait for encoding interrupt
 */
int vpu_EncWaitForInt(VpuEncoder* h, int timeout_in_ms)
{
    EncHandle handle = NULL;
    int ret;

    ret = check_enc_handle(h);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "Invalid encoder handle!\n");
        return ret;
    }

    handle = (EncHandle)(h->priv);

    int status = VpuHandlingInterruptFlag(handle, timeout_in_ms);
    if (status == VPU_ENC_INTR_STATUS_NONE) {
        VLOG(ERR, "Instance #%d: interrupt none\n", handle->instIndex);
        return VPU_RET_FAILURE;
    }
    if (status == VPU_ENC_INTR_STATUS_TIMEOUT) {
        VLOG(ERR, "Instance #%d: interrupt timeout\n", handle->instIndex);
        return VPU_RET_FAILURE;
    }
    if (status != VPU_ENC_INTR_STATUS_DONE) {
        VLOG(ERR, "Instance #%d: Unknown interrupt status: %d\n", handle->instIndex, status);
        return VPU_RET_FAILURE;
    }

    return VPU_RET_SUCCESS;
}

int vpu_CalcStride(int mapType, uint32_t width, uint32_t height,
                   int yuv_format, int cbcrInterleave)
{
    return CalcStride(width, height, FORMAT_420, cbcrInterleave, mapType);
}

int vpu_CalcLumaSize(int mapType, uint32_t stride, uint32_t height, int yuv_format,
                     int cbcrInterleave)
{
    return CalcLumaSize(stride, height, yuv_format, cbcrInterleave, mapType, NULL);
}
int vpu_CalcChromaSize(int mapType, uint32_t stride, uint32_t height,
                       int yuv_format, int cbcrInterleave)
{
    return CalcChromaSize(stride, height, yuv_format, cbcrInterleave, mapType, NULL);
}
int vpu_GetFrameBufSize(int mapType, int stride, int height,
                        int yuv_format, int interleave)
{
    int32_t size_dpb_lum, size_dpb_chr, size_dpb_all;

    size_dpb_lum = CalcLumaSize(stride, height, yuv_format, interleave, mapType, NULL);
    size_dpb_chr = CalcChromaSize(stride, height, yuv_format, interleave, mapType, NULL);

    size_dpb_all = size_dpb_lum + size_dpb_chr*2;

    return size_dpb_all;
}

int vpu_write_memory(const uint8_t* host_va, int size, int vpu_core_idx, uint64_t vpu_pa)
{
    return bm_vdi_write_memory(vpu_core_idx, vpu_pa, (uint8_t*)host_va, size);
}

int vpu_read_memory(uint8_t* host_va, int size, int vpu_core_idx, uint64_t vpu_pa)
{
    return bm_vdi_read_memory(vpu_core_idx, vpu_pa, host_va, size);
}

int vpu_GetProductId(int coreIdx)
{
    int32_t productId,ret;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return -1;

    productId = VpuGetProductId(coreIdx);
    if (productId != PRODUCT_ID_NONE) {
        return productId;
    }

    ret = bm_vdi_init(coreIdx);
    if (ret < 0) {
        return -1;
    }

    EnterLock(coreIdx);
    if (VpuScanProductId(coreIdx) == FALSE)
        productId = -1;
    else
        productId = VpuGetProductId(coreIdx);
    LeaveLock((coreIdx));

    bm_vdi_release(coreIdx);

    return productId;
}

int vpu_InitWithBitcode(uint32_t coreIdx, const uint16_t* code, uint32_t size)
{
    int ret;
    int init_status;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    if (code == NULL || size == 0)
        return VPU_RET_INVALID_PARAM;

    ret = bm_vdi_init(coreIdx);
    if (ret < 0)
        return VPU_RET_FAILURE;

    EnterLock(coreIdx);

    ret = bm_vdi_get_instance_num(coreIdx);
    if (ret < 0) {
        return VPU_RET_FAILURE;
    } else if (ret > 0) {
        if (VpuScanProductId(coreIdx) == 0) {
            LeaveLock(coreIdx);
            return VPU_RET_NOT_FOUND_VPU_DEVICE;
        }
    }

    init_status = bm_vdi_get_firmware_status(coreIdx);
    if (VpuIsInit(coreIdx) != 0 && init_status == 1) {
        Wave5VpuReInit(coreIdx, (void*)code, size);
        LeaveLock(coreIdx);
        return VPU_RET_CALLED_BEFORE;
    }

    InitCodecInstancePool(coreIdx);

    VLOG(INFO, "reload firmware...\n");

    ret = Wave5VpuReset(coreIdx, SW_RESET_ON_BOOT);
    if (ret != VPU_RET_SUCCESS) {
        LeaveLock(coreIdx);
        return ret;
    }

    ret = Wave5VpuInit(coreIdx, (void*)code, size);
    if (ret != VPU_RET_SUCCESS) {
        LeaveLock(coreIdx);
        return ret;
    }

    LeaveLock(coreIdx);
    return VPU_RET_SUCCESS;
}

int vpu_GetProductInfo(uint32_t coreIdx, ProductInfo* productInfo)
{
    int  ret;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    EnterLock(coreIdx);

    if (VpuIsInit(coreIdx) == 0) {
        LeaveLock(coreIdx);
        return VPU_RET_NOT_INITIALIZED;
    }

    productInfo->productId = VpuGetProductId(coreIdx);
    ret = Wave5VpuGetProductInfo(coreIdx, productInfo);

    LeaveLock(coreIdx);

    return ret;
}

/*---------------------------------------
        device memory management
 ----------------------------------------*/
int vpu_EncAllocateDMABuffer(int coreIdx, BmVpuDMABuffer *buf, unsigned int size)
{
    int  ret;
    vpu_buffer_t vb;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    vb.size = size;
    vb.enable_cache = TRUE;

    ret = bm_vdi_allocate_dma_memory(coreIdx, &vb);
    if (ret < 0) {
        VLOG(INFO, "allocate device memory failed, size=%d byte\n", vb.size);
        return VPU_RET_FAILURE;
    }

    buf->phys_addr = vb.phys_addr;
    buf->size = size;
    buf->enable_cache = TRUE;
    if (vb.virt_addr)
        buf->virt_addr = vb.virt_addr;
    else
        buf->virt_addr = 0;

    return VPU_RET_SUCCESS;
}

int vpu_EncDeAllocateDMABuffer(int coreIdx, BmVpuDMABuffer *buf)
{
    int  ret;
    vpu_buffer_t vb;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    vb.phys_addr    = buf->phys_addr;
    vb.size         = buf->size;
    vb.virt_addr    = buf->virt_addr;
    vb.enable_cache = buf->enable_cache;

    ret = bm_vdi_free_dma_memory(coreIdx, &vb);
    if (ret < 0) {
        VLOG(INFO, "allocate device memory failed, size=%d byte\n", vb.size);
        return VPU_RET_FAILURE;
    }

    return VPU_RET_SUCCESS;
}


int vpu_EncAttachDMABuffer(int coreIdx, BmVpuDMABuffer *buf)
{
    int  ret;
    vpu_buffer_t vb;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    vb.phys_addr    = buf->phys_addr;
    vb.size         = buf->size;
    ret = bm_vdi_attach_dma_memory(coreIdx, &vb);
    if (ret < 0) {
        VLOG(INFO, "attach device memory failed, size=%d byte\n", vb.size);
        return VPU_RET_FAILURE;
    }
    return VPU_RET_SUCCESS;
}

int vpu_EncDeattachDMABuffer(int coreIdx, BmVpuDMABuffer *buf)
{
    int  ret;
    vpu_buffer_t vb;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    vb.phys_addr    = buf->phys_addr;
    vb.size         = buf->size;
    ret = bm_vdi_deattach_dma_memory(coreIdx, &vb);
    if (ret < 0) {
        VLOG(INFO, "deattach device memory failed, size=%d byte\n", vb.size);
        return VPU_RET_FAILURE;
    }
    return VPU_RET_SUCCESS;
}



int vpu_EncMmap(int coreIdx, BmVpuDMABuffer* buf, int port_flag)
{
    int  ret;
    int enable_read  = port_flag & BM_VPU_MAPPING_FLAG_READ;
    int enable_write = port_flag & BM_VPU_MAPPING_FLAG_WRITE;
    vpu_buffer_t tmp_buf;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    if (buf->virt_addr) {
        VLOG(WARN, "%s:%d dma_buffer already have vaddr 0x%lx\n", __func__, __LINE__, buf->virt_addr);
    }

    tmp_buf.phys_addr = buf->phys_addr;
    tmp_buf.size      = buf->size;
    ret = bm_vdi_mmap_dma_memory(coreIdx, &tmp_buf, enable_read, enable_write);
    if (ret < 0)
        return VPU_RET_FAILURE;

    buf->virt_addr = tmp_buf.virt_addr;
    return VPU_RET_SUCCESS;
};

int vpu_EncMunmap(int coreIdx, BmVpuDMABuffer* buf)
{
#ifdef BM_PCIE_MODE
    return 0;
#else
    int ret;
    vpu_buffer_t tmp_buf;

    if (buf->virt_addr == 0 || buf->size == 0) {
        return VPU_RET_SUCCESS;
    }

    tmp_buf.phys_addr = buf->phys_addr;
    tmp_buf.virt_addr = buf->virt_addr;
    tmp_buf.size      = buf->size;
    ret = bm_vdi_unmap_dma_memory(coreIdx, &tmp_buf);
    if (ret < 0)
        return VPU_RET_FAILURE;

    buf->virt_addr = 0;
    return ret;
#endif
}

int vpu_EncFlushDecache(int coreIdx, BmVpuDMABuffer* buf)
{
#ifdef BM_PCIE_MODE
#else
    int ret;
    vpu_buffer_t tmp_buf;

    if (buf->size == 0) {
        return VPU_RET_SUCCESS;
    }

    tmp_buf.size         = buf->size;
    tmp_buf.phys_addr    = buf->phys_addr;
    tmp_buf.virt_addr    = buf->virt_addr;
    tmp_buf.enable_cache = buf->enable_cache;
    ret = bm_vdi_flush_dma_memory(coreIdx, &tmp_buf);
    if (ret < 0)
        return VPU_RET_FAILURE;
#endif
    return VPU_RET_SUCCESS;
}

int vpu_EncInvalidateDecache(int coreIdx, BmVpuDMABuffer* buf)
{
    int ret;
    vpu_buffer_t tmp_buf;

    if (buf->size == 0) {
        return VPU_RET_SUCCESS;
    }

    tmp_buf.size         = buf->size;
    tmp_buf.phys_addr    = buf->phys_addr;
    tmp_buf.virt_addr    = buf->virt_addr;
    tmp_buf.enable_cache = buf->enable_cache;
    ret = bm_vdi_invalidate_dma_memory(coreIdx, &tmp_buf);
    if (ret < 0)
        return VPU_RET_FAILURE;
    return VPU_RET_SUCCESS;
}


/**
 * vpu_sw_reset
 * IN
 *    forcedReset : 1 if there is no need to waiting for BUS transaction,
 *                  0 for otherwise
 * OUT
 *    int : VPU_RET_FAILURE if failed to reset,
 *              VPU_RET_SUCCESS for otherwise
 */
static int vpu_sw_reset(uint32_t coreIdx, SWResetMode resetMode, EncHandle pInst)
{
    int ret;

    if (pInst && pInst->loggingEnable) {
        bm_vdi_log(pInst->coreIdx, 0x10000, 1);
    }

    EnterLock(coreIdx);
    ret = Wave5VpuReset(coreIdx, resetMode);
    LeaveLock(coreIdx);

    if (pInst && pInst->loggingEnable) {
        bm_vdi_log(pInst->coreIdx, 0x10000, 0);
    }

    return ret;
}

int vpu_lock(int soc_idx)
{
    int core_idx = vpu_EncGetUniCoreIdx(soc_idx);
    int ret = bm_vdi_lock2(core_idx);
    if (ret < 0)
        return VPU_RET_FAILURE;

    return VPU_RET_SUCCESS;
}
int vpu_unlock(int soc_idx)
{
    int core_idx = vpu_EncGetUniCoreIdx(soc_idx);
    int ret = bm_vdi_unlock2(core_idx);
    if (ret < 0)
        return VPU_RET_FAILURE;
    return VPU_RET_SUCCESS;
}

static int SendQuery(EncHandle handle, QUERY_OPT queryOpt)
{
    uint32_t regVal;
    int ret;

    /* Send QUERY cmd */
    VpuWriteReg(handle->coreIdx, W5_QUERY_OPTION, queryOpt);
    VpuWriteReg(handle->coreIdx, W5_VPU_BUSY_STATUS, 1);
    Wave5BitIssueCommand(handle, W5_QUERY);

    ret = enc_wait_vpu_busy(handle->coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "timeout\n");
        if (handle->loggingEnable)
            bm_vdi_log(handle->coreIdx, W5_QUERY, 2);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    regVal = VpuReadReg(handle->coreIdx, W5_RET_SUCCESS);
    if (regVal == FALSE)
        return VPU_RET_FAILURE;

    return VPU_RET_SUCCESS;
}


static int enc_open(EncHandle* pHandle, VpuEncOpenParam* pop)
{
    EncInfo* pEncInfo;
    int ret;

    ret = VpuCheckEncOpenParam(pop);
    if (ret != VPU_RET_SUCCESS) {
        VLOG(ERR, "VpuCheckEncOpenParam failed\n");
        return ret;
    }

    EnterLock(pop->coreIdx);

    if (VpuIsInit(pop->coreIdx) == 0) {
        LeaveLock(pop->coreIdx);
        VLOG(ERR, "VPU is NOT initialized\n");
        return VPU_RET_NOT_INITIALIZED;
    }

    ret = GetCodecInstance(pop->coreIdx, pHandle);
    if (ret != VPU_RET_SUCCESS) {
        *pHandle = 0;
        LeaveLock(pop->coreIdx);
        VLOG(ERR, "GetCodecInstance failed\n");
        return ret;
    }

    pEncInfo = (*pHandle)->encInfo;

    memset(pEncInfo, 0x00, sizeof(EncInfo));
    pEncInfo->openParam = *pop;

    ret = Wave5VpuBuildUpEncParam(*pHandle, pop);
    if (ret != VPU_RET_SUCCESS) {
        *pHandle = 0;
        VLOG(ERR, "Wave5VpuBuildUpEncParam failed\n");
    }

    LeaveLock(pop->coreIdx);

    return ret;
}

static int enc_close(EncHandle handle)
{
    EncInfo* pEncInfo;
    int ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    pEncInfo = handle->encInfo;

    EnterLock(handle->coreIdx);

    if (pEncInfo->initialInfoObtained) {
        VpuWriteReg(handle->coreIdx, pEncInfo->streamWrPtrRegAddr, pEncInfo->streamWrPtr);
        VpuWriteReg(handle->coreIdx, pEncInfo->streamRdPtrRegAddr, pEncInfo->streamRdPtr);
        while(Wave5VpuEncFiniSeq(handle) != VPU_RET_SUCCESS)
        {
            if (handle->loggingEnable)
                bm_vdi_log(handle->coreIdx, ENC_SEQ_END, 2);

            if (ret == VPU_RET_VPU_STILL_RUNNING) {
                SendQuery(handle, GET_RESULT);
                if (ret != VPU_RET_SUCCESS) {
                    if (VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON) == WAVE5_RESULT_NOT_READY)
                        return VPU_RET_REPORT_NOT_READY;
                    else
                        return VPU_RET_QUERY_FAILURE;
                }
            }
            else{
                LeaveLock(handle->coreIdx);
                return ret;
            }
        }

        if (handle->loggingEnable)
            bm_vdi_log(handle->coreIdx, ENC_SEQ_END, 0);
        pEncInfo->streamWrPtr = VpuReadReg(handle->coreIdx, pEncInfo->streamWrPtrRegAddr);
        pEncInfo->streamWrPtr = MapToAddr40Bit(handle->coreIdx, pEncInfo->streamWrPtr);
    }

    LeaveLock(handle->coreIdx);

    ret = FreeCodecInstance(handle);

    return ret;
}

static int enc_calc_core_buffer_size(EncHandle handle, int fb_num, VpuEncCoreBuffer* info)
{
    EncInfo*         pEncInfo;
    VpuEncOpenParam* openParam;
    int         ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS)
        return ret;

    pEncInfo = handle->encInfo;
    openParam = &pEncInfo->openParam;

    if (!pEncInfo->initialInfoObtained)
        return VPU_RET_WRONG_CALL_SEQUENCE;

    if (fb_num < pEncInfo->initialInfo.minFrameBufferCount)
        return VPU_RET_INSUFFICIENT_FRAME_BUFFERS;

    int codecMode = handle->codecMode;
    int bufHeight = 0, bufWidth = 0;
    int mvColSize, fbcYTblSize, fbcCTblSize, subSampledSize;

    if (codecMode == W_HEVC_ENC) {
        bufWidth  = VPU_ALIGN8(openParam->picWidth);
        bufHeight = VPU_ALIGN8(openParam->picHeight);

        if ((pEncInfo->rotationAngle != 0 || pEncInfo->mirrorDirection != 0) &&
            !(pEncInfo->rotationAngle == 180 && pEncInfo->mirrorDirection == MIRDIR_HOR_VER)) {
            bufWidth  = VPU_ALIGN32(openParam->picWidth);
            bufHeight = VPU_ALIGN32(openParam->picHeight);
        }

        if (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270) {
            bufWidth  = VPU_ALIGN32(openParam->picHeight);
            bufHeight = VPU_ALIGN32(openParam->picWidth);
        }
    } else {
        bufWidth  = VPU_ALIGN16(openParam->picWidth);
        bufHeight = VPU_ALIGN16(openParam->picHeight);

        if ((pEncInfo->rotationAngle != 0 || pEncInfo->mirrorDirection != 0) &&
            !(pEncInfo->rotationAngle == 180 && pEncInfo->mirrorDirection == MIRDIR_HOR_VER)) {
            bufWidth  = VPU_ALIGN16(openParam->picWidth);
            bufHeight = VPU_ALIGN16(openParam->picHeight);
        }

        if (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270) {
            bufWidth  = VPU_ALIGN16(openParam->picHeight);
            bufHeight = VPU_ALIGN16(openParam->picWidth);
        }
    }

    if (codecMode == W_HEVC_ENC) {
        mvColSize  = WAVE5_ENC_HEVC_MVCOL_BUF_SIZE(bufWidth, bufHeight);
        mvColSize  = VPU_ALIGN16(mvColSize);
    } else /* if (codecMode == W_AVC_ENC) */ {
        mvColSize  = WAVE5_ENC_AVC_MVCOL_BUF_SIZE(bufWidth, bufHeight);
    }

    fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(bufWidth, bufHeight);
    fbcYTblSize = VPU_ALIGN16(fbcYTblSize);

    fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(bufWidth, bufHeight);
    fbcCTblSize = VPU_ALIGN16(fbcCTblSize);

    if (openParam->bitstreamFormat == VPU_CODEC_AVC)
        subSampledSize = WAVE5_AVC_SUBSAMPLED_ONE_SIZE(bufWidth, bufHeight);
    else
        subSampledSize = WAVE5_SUBSAMPLED_ONE_SIZE(bufWidth, bufHeight);

    /* 4096 is a margin */
    info->MV.size        = ((mvColSize     *fb_num+4095)&(~4095))+4096;
    info->FbcYTbl.size   = ((fbcYTblSize   *fb_num+4095)&(~4095))+4096;
    info->FbcCTbl.size   = ((fbcCTblSize   *fb_num+4095)&(~4095))+4096;
    info->SubSamBuf.size = ((subSampledSize*fb_num+4095)&(~4095))+4096;

    return VPU_RET_SUCCESS;
}

static int enc_register_framebuffer(EncHandle handle, VpuFrameBuffer* bufArray, int fb_num,
                                    int stride, int height)
{
    EncInfo*         pEncInfo  = handle->encInfo;
    VpuEncOpenParam* openParam = &pEncInfo->openParam;
    VpuFrameBuffer*  fb;
    int i, ret;

    EnterLock(handle->coreIdx);

    pEncInfo->numFrameBuffers = fb_num;
    pEncInfo->stride          = stride;
    pEncInfo->mapType         = COMPRESSED_FRAME_MAP;

    for(i=0; i<fb_num; i++)
        pEncInfo->frameBufPool[i] = bufArray[i];

    fb = pEncInfo->frameBufPool;

    ret = VpuUpdateCFramebuffer(handle, fb, fb_num, stride, height,
                                (FrameBufferFormat)openParam->srcFormat);
    if (ret != VPU_RET_SUCCESS) {
        LeaveLock(handle->coreIdx);
        return ret;
    }

    /* Register framebuffers to VPU */
    ret = Wave5VpuEncRegisterCFramebuffer(handle, fb, fb_num);

    LeaveLock(handle->coreIdx);

    return ret;
}

static int enc_start_one_frame(EncHandle handle, VpuEncParam* param)
{
    EncInfo* pEncInfo;
    vpu_instance_pool_t* vip;
    int ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS)
        return ret;

    pEncInfo = handle->encInfo;
    vip      = (vpu_instance_pool_t*)bm_vdi_get_instance_pool(handle->coreIdx);
    if (!vip) {
        return VPU_RET_INVALID_HANDLE;
    }

    if (pEncInfo->stride == 0) { // This means frame buffers have not been registered.
        return VPU_RET_WRONG_CALL_SEQUENCE;
    }

    ret = CheckEncParam(handle, param);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    EnterLock(handle->coreIdx);

    // TODO
#if 0
    pEncInfo->ptsMap[param->srcIdx] = (pEncInfo->openParam.enablePTS == TRUE) ? GetTimestamp(handle) : param->pts;
#endif

    ret = Wave5VpuEncode(handle, param);

    LeaveLock(handle->coreIdx);

    VLOG(DEBUG, "instance Index: %d. instance Queue Count: %d, total Queue Count: %d\n",
         handle->instIndex, pEncInfo->instanceQueueCount, pEncInfo->totalQueueCount);

    return ret;
}

static int enc_get_output_info(EncHandle handle, VpuEncOutputInfo* info)
{
    int ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    if (info == 0) {
        return VPU_RET_INVALID_PARAM;
    }

    EnterLock(handle->coreIdx);

    ret = Wave5VpuEncGetResult(handle, info);
    if (ret == VPU_RET_SUCCESS) {
        // TODO
#if 0
        EncInfo* pEncInfo = handle->encInfo;
        if (info->encSrcIdx >= 0 && info->reconFrameIndex >= 0 )
            info->pts = pEncInfo->ptsMap[info->encSrcIdx];
#endif
    } else {
        info->pts = 0LL;
    }

    LeaveLock(handle->coreIdx);

    return ret;
}

static int enc_give_command(EncHandle handle, CodecCommand cmd, void* param)
{
    EncInfo*    pEncInfo;
    int     ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    pEncInfo = handle->encInfo;
    switch (cmd)
    {
    case ENABLE_ROTATION :
        pEncInfo->rotationEnable = 1;
        break;
    case DISABLE_ROTATION :
        pEncInfo->rotationEnable = 0;
        break;
    case ENABLE_MIRRORING :
        pEncInfo->mirrorEnable = 1;
        break;
    case DISABLE_MIRRORING :
        pEncInfo->mirrorEnable = 0;
        break;
    case SET_MIRROR_DIRECTION :
        {
            MirrorDirection mirDir;

            if (param == 0) {
                return VPU_RET_INVALID_PARAM;
            }
            mirDir = *(MirrorDirection*)param;
            if ( !(mirDir == MIRDIR_NONE) && !(mirDir==MIRDIR_HOR) && !(mirDir==MIRDIR_VER) && !(mirDir==MIRDIR_HOR_VER)) {
                return VPU_RET_INVALID_PARAM;
            }
            pEncInfo->mirrorDirection = mirDir;
        }
        break;
    case SET_ROTATION_ANGLE :
        {
            int angle;

            if (param == 0) {
                return VPU_RET_INVALID_PARAM;
            }
            angle = *(int*)param;
            if (angle != 0 && angle != 90 &&
                angle != 180 && angle != 270) {
                return VPU_RET_INVALID_PARAM;
            }
            if (pEncInfo->initialInfoObtained && (angle == 90 || angle ==270)) {
                return VPU_RET_INVALID_PARAM;
            }
            pEncInfo->rotationAngle = angle;
        }
        break;
    case ENC_PUT_VIDEO_HEADER:
        {
            VpuEncHeaderParam *encHeaderParam;

            if (param == 0) {
                return VPU_RET_INVALID_PARAM;
            }
            encHeaderParam = (VpuEncHeaderParam*)param;
            if (handle->codecMode == W_HEVC_ENC || handle->codecMode == W_AVC_ENC) {
                if (!( CODEOPT_ENC_VPS<=encHeaderParam->headerType && encHeaderParam->headerType <= (CODEOPT_ENC_VPS|CODEOPT_ENC_SPS|CODEOPT_ENC_PPS))) {
                    return VPU_RET_INVALID_PARAM;
                }
                if (encHeaderParam->buf % 16 || encHeaderParam->size == 0)
                    return VPU_RET_INVALID_PARAM;
                if (encHeaderParam->headerType & CODEOPT_ENC_VCL)   // ENC_PUT_VIDEO_HEADER encode only non-vcl header.
                    return VPU_RET_INVALID_PARAM;

            } else
                return VPU_RET_INVALID_PARAM;

            if (encHeaderParam->buf % 8 || encHeaderParam->size == 0) {
                return VPU_RET_INVALID_PARAM;
            }
            if (handle->codecMode == W_HEVC_ENC || handle->codecMode == W_AVC_ENC) {
                if (handle->productId == PRODUCT_ID_521)
                    return Wave5VpuEncGetHeader(handle, encHeaderParam);
                else
                    return VPU_RET_INVALID_PARAM;
            }
        }
        break;
    case SET_SEC_AXI:
        {
            SecAxiUse secAxiUse;

            if (param == 0) {
                return VPU_RET_INVALID_PARAM;
            }
            secAxiUse = *(SecAxiUse*)param;
            if (handle->productId == PRODUCT_ID_521) {
                pEncInfo->secAxiInfo.useEncRdoEnable = secAxiUse.useEncRdoEnable;
                pEncInfo->secAxiInfo.useEncLfEnable  = secAxiUse.useEncLfEnable;
            }
        }
        break;
    case ENABLE_LOGGING:
        {
            handle->loggingEnable = 1;
        }
        break;
    case DISABLE_LOGGING:
        {
            handle->loggingEnable = 0;
        }
        break;
    case ENC_SET_PARA_CHANGE:
        {
            EncChangeParam* option = (EncChangeParam*)param;
            if (handle->productId == PRODUCT_ID_521)
                return Wave5VpuEncParaChange(handle, option);
            else
                return VPU_RET_INVALID_PARAM;
        }
    case GET_BANDWIDTH_REPORT :
        if (param == 0) {
            return VPU_RET_INVALID_PARAM;
        }
        return Wave5VpuGetBwReport(handle, (VPUBWData*)param);
    case ENC_GET_QUEUE_STATUS:
        {
            QueueStatusInfo* queueInfo = (QueueStatusInfo*)param;
            queueInfo->instanceQueueCount = pEncInfo->instanceQueueCount;
            queueInfo->totalQueueCount    = pEncInfo->totalQueueCount;
            break;
        }
    case SET_CYCLE_PER_TICK:
        {
            pEncInfo->cyclePerTick = *(uint32_t*)param;
        }
        break;
    default:
        return VPU_RET_INVALID_COMMAND;
    }
    return VPU_RET_SUCCESS;
}

static int enc_issue_seq_init(EncHandle handle)
{
    int ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS)
        return ret;

    EnterLock(handle->coreIdx);

    ret = Wave5VpuEncInitSeq(handle);

    LeaveLock(handle->coreIdx);

    return ret;
}

static int enc_complete_seq_init(EncHandle handle, VpuEncInitialInfo* info)
{
    EncInfo* pEncInfo;
    int ret;

    ret = CheckEncInstanceValidity(handle);
    if (ret != VPU_RET_SUCCESS) {
        return ret;
    }

    if (info == 0) {
        return VPU_RET_INVALID_PARAM;
    }

    pEncInfo = handle->encInfo;

    EnterLock(handle->coreIdx);

    ret = Wave5VpuEncGetSeqInfo(handle, info);
    if (ret == VPU_RET_SUCCESS) {
        pEncInfo->initialInfoObtained = 1;
    }

    pEncInfo->initialInfo = *info;

    LeaveLock(handle->coreIdx);

    return ret;
}

static bm_pa_t MapToAddr40Bit(int coreIdx, unsigned int Addr)
{
    bm_pa_t RealAddr = (bm_pa_t)bm_vdi_get_ddr_map(coreIdx);

    RealAddr = (RealAddr << 32) | (bm_pa_t)Addr;

    return RealAddr;
}

/**
 * Returns:
 *   0 - not found product id
 *   1 - found product id
 */
static BOOL VpuScanProductId(uint32_t coreIdx)
{
    uint32_t productId;
    uint32_t foundProducts = 0;

    /* Already scanned */
    if (s_encProductIds[coreIdx] != PRODUCT_ID_NONE)
        return 1;

    productId = WaveVpuGetProductId(coreIdx);
    if (productId != PRODUCT_ID_NONE) {
        s_encProductIds[coreIdx] = productId;
        foundProducts++;
    }

    return (foundProducts >= 1);
}

static int32_t VpuGetProductId(uint32_t coreIdx)
{
    return s_encProductIds[coreIdx];
}

static uint32_t VpuIsInit(uint32_t coreIdx)
{
    int productId = s_encProductIds[coreIdx];
    uint32_t pc = 0;

    if (productId == PRODUCT_ID_NONE) {
        VpuScanProductId(coreIdx);
        productId = s_encProductIds[coreIdx];
    }

    if (productId == PRODUCT_ID_NONE) {
        return 0;
    }

    pc = (uint32_t)VpuReadReg(coreIdx, W5_VCPU_CUR_PC);

    return pc;
}

/* Update compressed frame buffer */
static int VpuUpdateCFramebuffer(EncHandle inst, VpuFrameBuffer* fbArr,
                                int32_t num, int32_t stride, int32_t height,
                                FrameBufferFormat format)
{
    EncInfo*  pEncInfo  = inst->encInfo;
    uint32_t  sizeLuma, sizeChroma;
    int       i;

    sizeLuma   = CalcLumaSize(stride, height, format, TRUE, COMPRESSED_FRAME_MAP, NULL);
    sizeChroma = CalcChromaSize(stride, height, format, TRUE, COMPRESSED_FRAME_MAP, NULL);

    /* Framebuffer common informations */
    for (i=0; i<num; i++) {
        if (fbArr[i].updateFbInfo == TRUE ) {
            fbArr[i].updateFbInfo = FALSE;
            fbArr[i].myIndex        = i;
            fbArr[i].stride         = stride;
            fbArr[i].height         = height;
            fbArr[i].mapType        = COMPRESSED_FRAME_MAP;
            fbArr[i].format         = format;
            fbArr[i].cbcrInterleave = TRUE;
            fbArr[i].nv21           = FALSE;
            fbArr[i].endian         = VDI_128BIT_LITTLE_ENDIAN;
            fbArr[i].size           = sizeLuma + sizeChroma*2;
            if (inst->codecMode == W_HEVC_ENC || inst->codecMode == W_AVC_ENC) {
                fbArr[i].lumaBitDepth   = pEncInfo->openParam.waveParam.internalBitDepth;
                fbArr[i].chromaBitDepth = pEncInfo->openParam.waveParam.internalBitDepth;
            }
        }
    }

    for (i=0; i<num; i++) {
        if (fbArr[i].bufCb == (bm_pa_t)-1 || fbArr[i].bufCr == (bm_pa_t)-1) {
            if (fbArr[i].bufCb == (bm_pa_t)-1)
                fbArr[i].bufCb = fbArr[i].bufY + sizeLuma;
            if (fbArr[i].bufCr == (bm_pa_t)-1)
                fbArr[i].bufCr = (bm_pa_t)-1;
        }
    }

    return VPU_RET_SUCCESS;
}

static int VpuCheckEncOpenParam(VpuEncOpenParam* pop)
{
    int32_t     coreIdx;
    int32_t     picWidth;
    int32_t     picHeight;
    int32_t     productId;
    VpuAttr*    pAttr;

    if (pop == 0)
        return VPU_RET_INVALID_PARAM;

    if (pop->coreIdx > MAX_NUM_VPU_CORE)
        return VPU_RET_INVALID_PARAM;

    coreIdx   = pop->coreIdx;
    picWidth  = pop->picWidth;
    picHeight = pop->picHeight;
    productId = s_encProductIds[coreIdx];
    pAttr     = &s_VpuCoreAttributes[coreIdx];

    if ((pAttr->supportEncoders&(1<<pop->bitstreamFormat)) == 0) {
        VLOG(ERR, "Unsupported feature!\n");
        return VPU_RET_UNSUPPORTED_FEATURE;
    }

    if (pop->frameRateInfo == 0)
        return VPU_RET_INVALID_PARAM;

    if (pop->bitstreamFormat == VPU_CODEC_AVC) {
    } else if (pop->bitstreamFormat == VPU_CODEC_HEVC) {
        if (productId == PRODUCT_ID_521) {
            if (pop->bitRate > 700000000 || pop->bitRate < 0)
                return VPU_RET_INVALID_PARAM;
        }
    } else {
        if (pop->bitRate > 32767 || pop->bitRate < 0)
            return VPU_RET_INVALID_PARAM;
    }

    if (pop->bitstreamFormat == VPU_CODEC_HEVC ||
        (pop->bitstreamFormat == VPU_CODEC_AVC && productId == PRODUCT_ID_521)) {
        VpuEncWaveParam* param = &pop->waveParam;

        if (picWidth < MIN_ENC_PIC_WIDTH || picWidth > MAX_ENC_PIC_WIDTH)
            return VPU_RET_INVALID_PARAM;

        if (picHeight < MIN_ENC_PIC_HEIGHT || picHeight > MAX_ENC_PIC_HEIGHT)
            return VPU_RET_INVALID_PARAM;

        if (param->profile != HEVC_PROFILE_MAIN && param->profile != HEVC_PROFILE_MAIN10 && param->profile != HEVC_PROFILE_STILLPICTURE)
            return VPU_RET_INVALID_PARAM;

        if (param->internalBitDepth != 8 && param->internalBitDepth != 10)
            return VPU_RET_INVALID_PARAM;

        if (param->internalBitDepth > 8 && param->profile == HEVC_PROFILE_MAIN)
            return VPU_RET_INVALID_PARAM;

        if (param->decodingRefreshType < 0 || param->decodingRefreshType > 2)
            return VPU_RET_INVALID_PARAM;

        if (param->gopPresetIdx == VPU_GOP_PRESET_IDX_CUSTOM_GOP) {
            if ( param->gopParam.customGopSize < 1 || param->gopParam.customGopSize > MAX_GOP_NUM)
                return VPU_RET_INVALID_PARAM;
        }

        if (pop->bitstreamFormat == VPU_CODEC_AVC) {
            if (param->customLambdaEnable == 1)
                return VPU_RET_INVALID_PARAM;
        }
        if (param->constIntraPredFlag != 1 && param->constIntraPredFlag != 0)
            return VPU_RET_INVALID_PARAM;

        if (param->intraRefreshMode < 0 || param->intraRefreshMode > 4)
            return VPU_RET_INVALID_PARAM;


        if (pop->bitstreamFormat == VPU_CODEC_HEVC) {
            if (param->independSliceMode < 0 || param->independSliceMode > 1)
                return VPU_RET_INVALID_PARAM;

            if (param->independSliceMode != 0) {
                if (param->dependSliceMode < 0 || param->dependSliceMode > 2)
                    return VPU_RET_INVALID_PARAM;
            }
        }

        if (param->useRecommendEncParam < 0 && param->useRecommendEncParam > 3)
            return VPU_RET_INVALID_PARAM;

        if (param->useRecommendEncParam == 0 || param->useRecommendEncParam == 2 || param->useRecommendEncParam == 3) {
            if (param->intraNxNEnable != 1 && param->intraNxNEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->skipIntraTrans != 1 && param->skipIntraTrans != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->scalingListEnable != 1 && param->scalingListEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->tmvpEnable != 1 && param->tmvpEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->wppEnable != 1 && param->wppEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->useRecommendEncParam != 3) {     // in FAST mode (recommendEncParam==3), maxNumMerge value will be decided in FW
                if (param->maxNumMerge < 0 || param->maxNumMerge > 3)
                    return VPU_RET_INVALID_PARAM;
            }

            if (param->disableDeblk != 1 && param->disableDeblk != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->disableDeblk == 0 || param->saoEnable != 0) {
                if (param->lfCrossSliceBoundaryEnable != 1 && param->lfCrossSliceBoundaryEnable != 0)
                    return VPU_RET_INVALID_PARAM;
            }

            if (param->disableDeblk == 0) {
                if (param->betaOffsetDiv2 < -6 || param->betaOffsetDiv2 > 6)
                    return VPU_RET_INVALID_PARAM;

                if (param->tcOffsetDiv2 < -6 || param->tcOffsetDiv2 > 6)
                    return VPU_RET_INVALID_PARAM;
            }
        }

        if (param->losslessEnable != 1 && param->losslessEnable != 0)
            return VPU_RET_INVALID_PARAM;

        if (param->intraQP < 0 || param->intraQP > 63)
            return VPU_RET_INVALID_PARAM;

        if (pop->rcEnable != 1 && pop->rcEnable != 0)
            return VPU_RET_INVALID_PARAM;

        if (pop->rcEnable == 1) {
            if (param->minQpI < 0 || param->minQpI > 63)
                return VPU_RET_INVALID_PARAM;
            if (param->maxQpI < 0 || param->maxQpI > 63)
                return VPU_RET_INVALID_PARAM;

            if (param->minQpP < 0 || param->minQpP > 63)
                return VPU_RET_INVALID_PARAM;
            if (param->maxQpP < 0 || param->maxQpP > 63)
                return VPU_RET_INVALID_PARAM;

            if (param->minQpB < 0 || param->minQpB > 63)
                return VPU_RET_INVALID_PARAM;
            if (param->maxQpB < 0 || param->maxQpB > 63)
                return VPU_RET_INVALID_PARAM;

            if (param->cuLevelRCEnable != 1 && param->cuLevelRCEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->hvsQPEnable != 1 && param->hvsQPEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->hvsQPEnable) {
                if (param->maxDeltaQp < 0 || param->maxDeltaQp > 51)
                    return VPU_RET_INVALID_PARAM;
            }

            if (param->bitAllocMode < 0 && param->bitAllocMode > 2)
                return VPU_RET_INVALID_PARAM;

            if (pop->vbvBufferSize < 10 || pop->vbvBufferSize > 3000 )
                return VPU_RET_INVALID_PARAM;
        }

        // packed format & cbcrInterleave & nv12 can't be set at the same time.
        if (pop->packedFormat == 1 && pop->cbcrInterleave == 1)
            return VPU_RET_INVALID_PARAM;

        if (pop->packedFormat == 1 && pop->nv21 == 1)
            return VPU_RET_INVALID_PARAM;

        // check valid for common param
        if (CheckEncCommonParamValid(pop) == VPU_RET_FAILURE)
            return VPU_RET_INVALID_PARAM;

        // check valid for RC param
        if (CheckEncRcParamValid(pop) == VPU_RET_FAILURE)
            return VPU_RET_INVALID_PARAM;

        if (param->gopPresetIdx == VPU_GOP_PRESET_IDX_CUSTOM_GOP) {
            if (CheckEncCustomGopParamValid(pop) == VPU_RET_FAILURE)
                return VPU_RET_INVALID_COMMAND;
        }

        if (param->chromaCbQpOffset < -12 || param->chromaCbQpOffset > 12)
            return VPU_RET_INVALID_PARAM;

        if (param->chromaCrQpOffset < -12 || param->chromaCrQpOffset > 12)
            return VPU_RET_INVALID_PARAM;

        if (param->intraRefreshMode == 3 && param-> intraRefreshArg == 0)
            return VPU_RET_INVALID_PARAM;

        if (pop->bitstreamFormat == VPU_CODEC_HEVC) {
            if (param->nrYEnable != 1 && param->nrYEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->nrCbEnable != 1 && param->nrCbEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->nrCrEnable != 1 && param->nrCrEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->nrNoiseEstEnable != 1 && param->nrNoiseEstEnable != 0)
                return VPU_RET_INVALID_PARAM;

            if (param->nrNoiseSigmaY > 255)
                return VPU_RET_INVALID_PARAM;

            if (param->nrNoiseSigmaCb > 255)
                return VPU_RET_INVALID_PARAM;

            if (param->nrNoiseSigmaCr > 255)
                return VPU_RET_INVALID_PARAM;

            if (param->nrIntraWeightY > 31)
                return VPU_RET_INVALID_PARAM;

            if (param->nrIntraWeightCb > 31)
                return VPU_RET_INVALID_PARAM;

            if (param->nrIntraWeightCr > 31)
                return VPU_RET_INVALID_PARAM;

            if (param->nrInterWeightY > 31)
                return VPU_RET_INVALID_PARAM;

            if (param->nrInterWeightCb > 31)
                return VPU_RET_INVALID_PARAM;

            if (param->nrInterWeightCr > 31)
                return VPU_RET_INVALID_PARAM;

            if((param->nrYEnable == 1 || param->nrCbEnable == 1 || param->nrCrEnable == 1) && (param->losslessEnable == 1))
                return VPU_RET_INVALID_PARAM;
        }
    }

    return VPU_RET_SUCCESS;
}

static int32_t WaveVpuGetProductId(uint32_t  coreIdx)
{
    uint32_t  productId = PRODUCT_ID_NONE;
    uint32_t  val;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return PRODUCT_ID_NONE;

    val = VpuReadReg(coreIdx, W5_PRODUCT_NUMBER);
    switch (val) {
    case WAVE521C_CODE:
        productId = PRODUCT_ID_521;
        break;
    default:
        VLOG(ERR, "Check productId(%d) %s, %d\n", val, __FILE__, __LINE__);
        break;
    }

    return productId;
}

static void Wave5BitIssueCommand(EncHandle handle, uint32_t cmd)
{
    uint32_t instanceIndex = 0;
    uint32_t codecMode     = 0;
    uint32_t coreIdx;

    if (handle == NULL) {
        return ;
    }

    instanceIndex = handle->instIndex;
    codecMode     = handle->codecMode;
    coreIdx = handle->coreIdx;

    VpuWriteReg(coreIdx, W5_CMD_INSTANCE_INFO,  (codecMode<<16)|(instanceIndex&0xffff));
    VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
    VpuWriteReg(coreIdx, W5_COMMAND, cmd);

    if ((handle != NULL && handle->loggingEnable))
        bm_vdi_log(coreIdx, cmd, 1);

    VLOG(DEBUG, "instanceIndex=%d: cmd=0x%x\n", instanceIndex, cmd);

    VpuWriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);
    return;
}

static int SetupWave5Properties(uint32_t coreIdx)
{
    VpuAttr*    pAttr = &s_VpuCoreAttributes[coreIdx];
    uint32_t    regVal;
    uint8_t*    str;
    int         ret;

    VpuWriteReg(coreIdx, W5_QUERY_OPTION, GET_VPU_INFO);
    VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
    VpuWriteReg(coreIdx, W5_COMMAND, W5_QUERY);
    VpuWriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);

    ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "timeout\n");
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    regVal = VpuReadReg(coreIdx, W5_RET_SUCCESS);
    if (regVal == FALSE) {
        return VPU_RET_QUERY_FAILURE;
    }

    regVal = VpuReadReg(coreIdx, W5_RET_PRODUCT_NAME);
    str    = (uint8_t*)&regVal;
    pAttr->productName[0] = str[3];
    pAttr->productName[1] = str[2];
    pAttr->productName[2] = str[1];
    pAttr->productName[3] = str[0];
    pAttr->productName[4] = 0;

    pAttr->productId       = WaveVpuGetProductId(coreIdx);

    pAttr->hwConfigDef0    = VpuReadReg(coreIdx, W5_RET_STD_DEF0);
    pAttr->hwConfigDef1    = VpuReadReg(coreIdx, W5_RET_STD_DEF1);
    pAttr->hwConfigFeature = VpuReadReg(coreIdx, W5_RET_CONF_FEATURE);
    pAttr->hwConfigDate    = VpuReadReg(coreIdx, W5_RET_CONF_DATE);
    pAttr->hwConfigRev     = VpuReadReg(coreIdx, W5_RET_CONF_REVISION);
    pAttr->hwConfigType    = VpuReadReg(coreIdx, W5_RET_CONF_TYPE);

    pAttr->supportGDIHW    = TRUE;

    /* wave521c only supports encoders */
#if 0
    pAttr->supportDecoders = (1<<VPU_CODEC_HEVC);
    pAttr->supportDecoders |= (1<<VPU_CODEC_AVC); // TODO
#else
    pAttr->supportDecoders = 0;
#endif

    /* pAttr->productId = PRODUCT_ID_521 */
    pAttr->supportEncoders  = (1<<VPU_CODEC_HEVC);
    pAttr->supportEncoders |= (1<<VPU_CODEC_AVC);
    pAttr->supportBackbone  = TRUE;

    pAttr->support2AlignScaler   = (BOOL)((pAttr->hwConfigDef0>>23)&1);

    /* No pending instance */
    pAttr->supportCommandQueue   = TRUE;

    pAttr->supportFBCBWOptimization = (BOOL)((pAttr->hwConfigDef1>>15)&0x01);
    pAttr->supportNewTimer          = (BOOL)((pAttr->hwConfigDef1>>27)&0x01);
    pAttr->supportWTL        = TRUE;

    pAttr->supportTiled2Linear   = FALSE;
    pAttr->supportMapTypes       = FALSE;
    pAttr->support128bitBus      = TRUE;
    pAttr->supportThumbnailMode  = TRUE;
    pAttr->supportEndianMask     = (uint32_t)((1<<VDI_LITTLE_ENDIAN) | (1<<VDI_BIG_ENDIAN) | (1<<VDI_32BIT_LITTLE_ENDIAN) | (1<<VDI_32BIT_BIG_ENDIAN) | (0xffffUL<<16));
    pAttr->supportBitstreamMode  = (1<<BS_MODE_INTERRUPT) | (1<<BS_MODE_PIC_END);
    pAttr->bitstreamBufferMargin = 0;
    pAttr->numberOfVCores        = 1;
    pAttr->numberOfMemProtectRgns = 10;

    return VPU_RET_SUCCESS;
}

static int Wave5VpuGetVersion(uint32_t coreIdx, uint32_t* versionInfo, uint32_t* revision)
{
    uint32_t regVal;
    int ret;

    VpuWriteReg(coreIdx, W5_QUERY_OPTION, GET_VPU_INFO);
    VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
    VpuWriteReg(coreIdx, W5_COMMAND, W5_QUERY);
    VpuWriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);

    ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "timeout\n");
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    if (VpuReadReg(coreIdx, W5_RET_SUCCESS) == FALSE) {
        VLOG(ERR, "Wave5VpuGetVersion FALSE\n");
        return VPU_RET_QUERY_FAILURE;
    }

    regVal = VpuReadReg(coreIdx, W5_RET_FW_VERSION);
    if (versionInfo != NULL) {
        *versionInfo = 0;
    }
    if (revision != NULL) {
        *revision    = regVal;
    }

    return VPU_RET_SUCCESS;
}

static int Wave5VpuGetProductInfo(uint32_t coreIdx, ProductInfo* productInfo)
{
    int ret;

    /* GET FIRMWARE&HARDWARE INFORMATION */
    VpuWriteReg(coreIdx, W5_QUERY_OPTION, GET_VPU_INFO);
    VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
    VpuWriteReg(coreIdx, W5_COMMAND, W5_QUERY);
    VpuWriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);

    ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "timeout\n");
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    if (VpuReadReg(coreIdx, W5_RET_SUCCESS) == FALSE) {
        VLOG(ERR, "Wave5VpuGetProductInfo FALSE\n");
        return VPU_RET_QUERY_FAILURE;
    }

    productInfo->fwVersion      = VpuReadReg(coreIdx, W5_RET_FW_VERSION);
    productInfo->productName    = VpuReadReg(coreIdx, W5_RET_PRODUCT_NAME);
    productInfo->productVersion = VpuReadReg(coreIdx, W5_RET_PRODUCT_VERSION);
    productInfo->customerId     = VpuReadReg(coreIdx, W5_RET_CUSTOMER_ID);
    productInfo->stdDef0        = VpuReadReg(coreIdx, W5_RET_STD_DEF0);
    productInfo->stdDef1        = VpuReadReg(coreIdx, W5_RET_STD_DEF1);
    productInfo->confFeature    = VpuReadReg(coreIdx, W5_RET_CONF_FEATURE);
    productInfo->configDate     = VpuReadReg(coreIdx, W5_RET_CONF_DATE);
    productInfo->configRevision = VpuReadReg(coreIdx, W5_RET_CONF_REVISION);
    productInfo->configType     = VpuReadReg(coreIdx, W5_RET_CONF_TYPE);

    return VPU_RET_SUCCESS;
}

static int Wave5VpuInit(uint32_t coreIdx, void* firmware, uint32_t sizeInShort)
{
    vpu_buffer_t    vb = {0};
    bm_pa_t codeBase, tempBase;
    bm_pa_t taskBufBase;
    uint32_t codeSize, tempSize;
    uint32_t regVal, remapSize;
    int sizeInByte = sizeInShort*2;
    int i, ret;

    // TODO
    bm_vdi_get_common_memory(coreIdx, &vb);

    codeBase = vb.phys_addr + WAVE5_CODEBUF_OFFSET;
    codeSize = WAVE5_CODEBUF_SIZE;
    if (codeSize < sizeInByte) {
        return VPU_RET_INSUFFICIENT_RESOURCE;
    }

    tempBase = vb.phys_addr + WAVE5_TEMPBUF_OFFSET;
    tempSize = WAVE5_TEMPBUF_SIZE;

    VLOG(INFO, "\nVPU INIT Start!!!\n");
    bm_vdi_write_memory(coreIdx, codeBase, (uint8_t*)firmware, sizeInByte);
    bm_vdi_set_bit_firmware_to_pm(coreIdx, (uint16_t*)firmware); // TODO

    regVal = 0;
    VpuWriteReg(coreIdx, W5_PO_CONF, regVal);

    /* Reset All blocks */
    regVal = 0x7ffffff;
    VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, regVal);

    /* Waiting reset done */
    ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_RESET_STATUS);
    if (ret == -1) {
        VLOG(ERR, "VPU init(W5_VPU_RESET_REQ) timeout\n");
        VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);

    /* clear registers */
    for (i=W5_CMD_REG_BASE; i<W5_CMD_REG_END; i+=4) {
        VpuWriteReg(coreIdx, i, 0x00);
    }

    /* remap page size */
    remapSize = (codeSize >> 12) &0x1ff;
    regVal = 0x80000000 | (WAVE5_UPPER_PROC_AXI_ID<<20) | (0 << 16) | (W5_REMAP_CODE_INDEX<<12) | (1<<11) | remapSize;
    VpuWriteReg(coreIdx, W5_VPU_REMAP_CTRL,     regVal);
    VpuWriteReg(coreIdx, W5_VPU_REMAP_VADDR,    0x00000000);    /* DO NOT CHANGE! */
    VpuWriteReg(coreIdx, W5_VPU_REMAP_PADDR,    codeBase);
    VpuWriteReg(coreIdx, W5_ADDR_CODE_BASE,     codeBase);
    VpuWriteReg(coreIdx, W5_CODE_SIZE,          codeSize);
    VpuWriteReg(coreIdx, W5_CODE_PARAM,         (WAVE5_UPPER_PROC_AXI_ID<<4) | 0);
    VpuWriteReg(coreIdx, W5_ADDR_TEMP_BASE,     tempBase);
    VpuWriteReg(coreIdx, W5_TEMP_SIZE,          tempSize);
    VpuWriteReg(coreIdx, W5_TIMEOUT_CNT, 0xffff);

    VpuWriteReg(coreIdx, W5_HW_OPTION, 0);

    /* Encoder interrupt */
    regVal  = (1<<INT_WAVE5_ENC_SET_PARAM);
    regVal |= (1<<INT_WAVE5_ENC_PIC);
    regVal |= (1<<INT_WAVE5_BSBUF_FULL);
    regVal |= (1<<INT_WAVE5_ENC_LOW_LATENCY);

    VpuWriteReg(coreIdx, W5_VPU_VINT_ENABLE,  regVal);

    regVal = VpuReadReg(coreIdx, W5_VPU_RET_VPU_CONFIG0);
    if (((regVal>>16)&1) == 1) {
        regVal = ((WAVE5_PROC_AXI_ID<<28)     |
                  (WAVE5_PRP_AXI_ID<<24)      |
                  (WAVE5_FBD_Y_AXI_ID<<20)    |
                  (WAVE5_FBC_Y_AXI_ID<<16)    |
                  (WAVE5_FBD_C_AXI_ID<<12)    |
                  (WAVE5_FBC_C_AXI_ID<<8)     |
                  (WAVE5_PRI_AXI_ID<<4)       |
                  (WAVE5_SEC_AXI_ID<<0));

        bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_PROG_AXI_ID, regVal);
    }

    VpuWriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, COMMAND_QUEUE_DEPTH);
    VpuWriteReg(coreIdx, W5_CMD_INIT_TASK_BUF_SIZE, ONE_TASKBUF_SIZE_FOR_CQ);

    for (i = 0; i < COMMAND_QUEUE_DEPTH; i++) {
        taskBufBase = vb.phys_addr + WAVE5_TASK_BUF_OFFSET + (i*ONE_TASKBUF_SIZE_FOR_CQ);
        VpuWriteReg(coreIdx, W5_CMD_INIT_ADDR_TASK_BUF0 + (i*4), taskBufBase);
    }

    /* TODO Get SRAM base/size */
    ret = bm_vdi_get_sram_memory(coreIdx, &vb);
    if (ret < 0)
        return VPU_RET_INSUFFICIENT_RESOURCE;

    VpuWriteReg(coreIdx, W5_ADDR_SEC_AXI, vb.phys_addr); // TODO
    VpuWriteReg(coreIdx, W5_SEC_AXI_SIZE, vb.size);

    VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
    VpuWriteReg(coreIdx, W5_COMMAND, W5_INIT_VPU);
    VpuWriteReg(coreIdx, W5_VPU_REMAP_CORE_START, 1);

    ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "VPU init(W5_VPU_REMAP_CORE_START) timeout\n");
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    regVal = VpuReadReg(coreIdx, W5_RET_SUCCESS);
    if (regVal == 0) {
        uint32_t reasonCode = VpuReadReg(coreIdx, W5_RET_FAIL_REASON);
        VLOG(ERR, "VPU init(W5_RET_SUCCESS) failed(%d) REASON CODE(%08x)\n", regVal, reasonCode);
        return VPU_RET_FAILURE;
    }

    ret = SetupWave5Properties(coreIdx);
    return ret;
}

static int Wave5VpuReInit(uint32_t coreIdx, void* firmware, uint32_t sizeInShort)
{
    vpu_buffer_t    vb = {0};
    bm_pa_t codeBase, tempBase, taskBufBase;
    bm_pa_t oldCodeBase, tempSize;
    uint32_t codeSize;
    uint32_t regVal, remapSize;
    int sizeInByte = sizeInShort*2;
    int i, ret;

    // TODO
    bm_vdi_get_common_memory(coreIdx, &vb);

    codeBase = vb.phys_addr + WAVE5_CODEBUF_OFFSET;
    codeSize = WAVE5_CODEBUF_SIZE;
    if (codeSize < sizeInByte) {
        return VPU_RET_INSUFFICIENT_RESOURCE;
    }

    tempBase = vb.phys_addr + WAVE5_TEMPBUF_OFFSET;
    tempSize = WAVE5_TEMPBUF_SIZE;

    oldCodeBase = VpuReadReg(coreIdx, W5_VPU_REMAP_PADDR);
    oldCodeBase = MapToAddr40Bit(coreIdx, oldCodeBase);
    if (oldCodeBase != codeBase) {
        bm_vdi_write_memory(coreIdx, codeBase, (uint8_t*)firmware, sizeInByte);
        bm_vdi_set_bit_firmware_to_pm(coreIdx, (uint16_t*)firmware); // TODO

        regVal = 0;
        VpuWriteReg(coreIdx, W5_PO_CONF, regVal);

        /* supportBackbone = TRUE */
        /* Step1 : disable request */
        bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_GDI_BUS_CTRL, 0x4);

        /* Step2 : Waiting for completion of bus transaction */
        ret = enc_wait_bus_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_BACKBONE_GDI_BUS_STATUS);
        if (ret == -1) {
            bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_GDI_BUS_CTRL, 0x00);
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

        /* Step3 : Waiting for completion of VCPU bus transaction */
        ret = enc_wait_vcpu_bus_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_VCPU_STATUS);
        if (ret == -1) {
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

        /* Reset All blocks */
        regVal = 0x7ffffff;
        VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, regVal);

        /* Waiting reset done */
        int ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_RESET_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

        VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);

        /* clear registers */
        for (i=W5_CMD_REG_BASE; i<W5_CMD_REG_END; i+=4) {
            VpuWriteReg(coreIdx, i, 0x00);
        }

        /* Step4 : must clear W5_BACKBONE_GDI_BUS_CTRL after done SW_RESET */
        /* pAttr->supportBackbone = TRUE */
        bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_GDI_BUS_CTRL, 0x00);

        /* remap page size */
        remapSize = (codeSize >> 12) &0x1ff;
        regVal = 0x80000000 | (WAVE5_UPPER_PROC_AXI_ID<<20) | (W5_REMAP_CODE_INDEX<<12) | (0 << 16) | (1<<11) | remapSize;
        VpuWriteReg(coreIdx, W5_VPU_REMAP_CTRL,     regVal);
        VpuWriteReg(coreIdx, W5_VPU_REMAP_VADDR,    0x00000000);    /* DO NOT CHANGE! */
        VpuWriteReg(coreIdx, W5_VPU_REMAP_PADDR,    codeBase);
        VpuWriteReg(coreIdx, W5_ADDR_CODE_BASE,     codeBase);
        VpuWriteReg(coreIdx, W5_CODE_SIZE,          codeSize);
        VpuWriteReg(coreIdx, W5_CODE_PARAM,         (WAVE5_UPPER_PROC_AXI_ID<<4) | 0);
        VpuWriteReg(coreIdx, W5_ADDR_TEMP_BASE,     tempBase);
        VpuWriteReg(coreIdx, W5_TEMP_SIZE,          tempSize);
        VpuWriteReg(coreIdx, W5_TIMEOUT_CNT,   0);

        VpuWriteReg(coreIdx, W5_HW_OPTION, 0);

        /* Encoder interrupt */
        regVal  = (1<<INT_WAVE5_ENC_SET_PARAM);
        regVal |= (1<<INT_WAVE5_ENC_PIC);
        regVal |= (1<<INT_WAVE5_BSBUF_FULL);
        regVal |= (1<<INT_WAVE5_ENC_LOW_LATENCY);

        VpuWriteReg(coreIdx, W5_VPU_VINT_ENABLE,  regVal);

        regVal = ((WAVE5_PROC_AXI_ID<<28)     |
                  (WAVE5_PRP_AXI_ID<<24)      |
                  (WAVE5_FBD_Y_AXI_ID<<20)    |
                  (WAVE5_FBC_Y_AXI_ID<<16)    |
                  (WAVE5_FBD_C_AXI_ID<<12)    |
                  (WAVE5_FBC_C_AXI_ID<<8)     |
                  (WAVE5_PRI_AXI_ID<<4)       |
                  (WAVE5_SEC_AXI_ID<<0));

        bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_PROG_AXI_ID, regVal);

        VpuWriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, COMMAND_QUEUE_DEPTH);
        VpuWriteReg(coreIdx, W5_CMD_INIT_TASK_BUF_SIZE, ONE_TASKBUF_SIZE_FOR_CQ);

        for (i = 0; i < COMMAND_QUEUE_DEPTH; i++) {
            taskBufBase = vb.phys_addr + WAVE5_TASK_BUF_OFFSET + (i*ONE_TASKBUF_SIZE_FOR_CQ);
            VpuWriteReg(coreIdx, W5_CMD_INIT_ADDR_TASK_BUF0 + (i*4), taskBufBase);
        }

        /* TODO Get SRAM base/size */
        ret = bm_vdi_get_sram_memory(coreIdx, &vb);
        if (ret < 0)
            return VPU_RET_INSUFFICIENT_RESOURCE;

        VpuWriteReg(coreIdx, W5_ADDR_SEC_AXI, vb.phys_addr); // TODO
        VpuWriteReg(coreIdx, W5_SEC_AXI_SIZE, vb.size);

        VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
        VpuWriteReg(coreIdx, W5_COMMAND, W5_INIT_VPU);
        VpuWriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);
        VpuWriteReg(coreIdx, W5_VPU_REMAP_CORE_START, 1);

        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

        regVal = VpuReadReg(coreIdx, W5_RET_SUCCESS);
        if (regVal == 0) {
            uint32_t reasonCode = VpuReadReg(coreIdx, W5_RET_FAIL_REASON);
            VLOG(ERR, "VPU init(W5_RET_SUCCESS) failed(%d) REASON CODE(%08x)\n", regVal, reasonCode);
            return VPU_RET_FAILURE;
        }
    }

    ret = SetupWave5Properties(coreIdx);
    return ret;
}

static int Wave5VpuSleepWake(uint32_t coreIdx, int iSleepWake, const uint16_t* code, uint32_t sizeInShort, BOOL reset)
{
    uint32_t regVal;
    int ret;

    if (iSleepWake==1) {  //saves
        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

        VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
        VpuWriteReg(coreIdx, W5_COMMAND, W5_SLEEP_VPU);
        VpuWriteReg(coreIdx, W5_VPU_HOST_INT_REQ, 1);

        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }
        regVal = VpuReadReg(coreIdx, W5_RET_SUCCESS);
        if (regVal == 0) {
            APIDPRINT("SLEEP_VPU failed [0x%x]", VpuReadReg(coreIdx, W5_RET_FAIL_REASON));
            return VPU_RET_FAILURE;
        }
    } else { //restore
        vpu_buffer_t vb = {0};
        bm_pa_t codeBase, tempBase;
        bm_pa_t taskBufBase;
        uint32_t codeSize, tempSize;
        uint32_t remapSize;
        int sizeInByte = sizeInShort*2;
        int i;

        // TODO
        bm_vdi_get_common_memory(coreIdx, &vb);

        codeBase = vb.phys_addr + WAVE5_TEMPBUF_OFFSET;
        codeSize = WAVE5_CODEBUF_SIZE;
        if (codeSize < sizeInByte) {
            return VPU_RET_INSUFFICIENT_RESOURCE;
        }

        tempBase = vb.phys_addr + WAVE5_TEMPBUF_OFFSET;
        tempSize = WAVE5_TEMPBUF_SIZE;

        regVal = 0;
        VpuWriteReg(coreIdx, W5_PO_CONF, regVal);

        /* SW_RESET_SAFETY */
        regVal = W5_RST_BLOCK_ALL;
        VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, regVal);    // Reset All blocks

        /* Waiting reset done */
        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_RESET_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

        VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);

        /* remap page size */
        remapSize = (codeSize >> 12) &0x1ff;
        regVal = 0x80000000 | (WAVE5_UPPER_PROC_AXI_ID<<20) | (W5_REMAP_CODE_INDEX<<12) | (0 << 16) | (1<<11) | remapSize;
        VpuWriteReg(coreIdx, W5_VPU_REMAP_CTRL,     regVal);
        VpuWriteReg(coreIdx, W5_VPU_REMAP_VADDR,    0x00000000);    /* DO NOT CHANGE! */
        VpuWriteReg(coreIdx, W5_VPU_REMAP_PADDR,    codeBase);
        VpuWriteReg(coreIdx, W5_ADDR_CODE_BASE,     codeBase);
        VpuWriteReg(coreIdx, W5_CODE_SIZE,          codeSize);
        VpuWriteReg(coreIdx, W5_CODE_PARAM,         (WAVE5_UPPER_PROC_AXI_ID<<4) | 0);
        VpuWriteReg(coreIdx, W5_ADDR_TEMP_BASE,     tempBase);
        VpuWriteReg(coreIdx, W5_TEMP_SIZE,          tempSize);
        VpuWriteReg(coreIdx, W5_TIMEOUT_CNT,   0);

        VpuWriteReg(coreIdx, W5_HW_OPTION, 0);

        /* encoder */
        regVal  = (1<<INT_WAVE5_ENC_SET_PARAM);
        regVal |= (1<<INT_WAVE5_ENC_PIC);
        regVal |= (1<<INT_WAVE5_BSBUF_FULL);
        regVal |= (1<<INT_WAVE5_ENC_LOW_LATENCY);

        VpuWriteReg(coreIdx, W5_VPU_VINT_ENABLE,  regVal);

        regVal  =  ((WAVE5_PROC_AXI_ID<<28)     |
                    (WAVE5_PRP_AXI_ID<<24)      |
                    (WAVE5_FBD_Y_AXI_ID<<20)    |
                    (WAVE5_FBC_Y_AXI_ID<<16)    |
                    (WAVE5_FBD_C_AXI_ID<<12)    |
                    (WAVE5_FBC_C_AXI_ID<<8)     |
                    (WAVE5_PRI_AXI_ID<<4)       |
                    (WAVE5_SEC_AXI_ID<<0));

        bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_PROG_AXI_ID, regVal);

        VpuWriteReg(coreIdx, W5_CMD_INIT_NUM_TASK_BUF, COMMAND_QUEUE_DEPTH);
        VpuWriteReg(coreIdx, W5_CMD_INIT_TASK_BUF_SIZE, ONE_TASKBUF_SIZE_FOR_CQ);

        for (i = 0; i < COMMAND_QUEUE_DEPTH; i++) {
            taskBufBase = vb.phys_addr + WAVE5_TASK_BUF_OFFSET + (i*ONE_TASKBUF_SIZE_FOR_CQ);
            VpuWriteReg(coreIdx, W5_CMD_INIT_ADDR_TASK_BUF0 + (i*4), taskBufBase);
        }

        /* TODO Get SRAM base/size */
        ret = bm_vdi_get_sram_memory(coreIdx, &vb);
        if (ret < 0)
            return VPU_RET_INSUFFICIENT_RESOURCE;

        VpuWriteReg(coreIdx, W5_ADDR_SEC_AXI, vb.phys_addr); // TODO
        VpuWriteReg(coreIdx, W5_SEC_AXI_SIZE, vb.size);

        VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 1);
        VpuWriteReg(coreIdx, W5_COMMAND, (reset==TRUE ? W5_INIT_VPU : W5_WAKEUP_VPU));
        VpuWriteReg(coreIdx, W5_VPU_REMAP_CORE_START, 1);

        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

        regVal = VpuReadReg(coreIdx, W5_RET_SUCCESS);
        if (regVal == 0) {
            return VPU_RET_FAILURE;
        }
        VpuWriteReg(coreIdx, W5_VPU_VINT_REASON_CLR, 0xffff);
        VpuWriteReg(coreIdx, W5_VPU_VINT_REASON_USR, 0);
        VpuWriteReg(coreIdx, W5_VPU_VINT_CLEAR, 0x1);
    }

    return VPU_RET_SUCCESS;
}

static int Wave5VpuReset(uint32_t coreIdx, SWResetMode resetMode)
{
    uint32_t  val = 0;
    int ret = VPU_RET_SUCCESS;
    VpuAttr*    pAttr = &s_VpuCoreAttributes[coreIdx];

    // VPU doesn't send response. Force to set BUSY flag to 0.
    VpuWriteReg(coreIdx, W5_VPU_BUSY_STATUS, 0);

    /* productId == PRODUCT_ID_521 */
    pAttr->supportBackbone = TRUE;

    // Waiting for completion of bus transaction
    /* pAttr->supportBackbone = TRUE */
    // Step1 : disable request
    bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_GDI_BUS_CTRL, 0x4);
    // Step2 : Waiting for completion of bus transaction
    if (enc_wait_bus_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_BACKBONE_GDI_BUS_STATUS) == -1) {
        bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_GDI_BUS_CTRL, 0x00);
        VLOG(ERR, "VpuReset Error = %d\n", pAttr->supportBackbone);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    // Step3 : Waiting for completion of VCPU bus transaction
    if (resetMode != SW_RESET_ON_BOOT ) {
        ret = enc_wait_vcpu_bus_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_VCPU_STATUS);
        if (ret == -1) {
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }
    }

    if (resetMode == SW_RESET_SAFETY) {
        ret = Wave5VpuSleepWake(coreIdx, TRUE, NULL, 0, TRUE);
        if (ret != VPU_RET_SUCCESS) {
            return ret;
        }
    }

    switch (resetMode) {
    case SW_RESET_ON_BOOT:
    case SW_RESET_FORCE:
    case SW_RESET_SAFETY:
        val = W5_RST_BLOCK_ALL;
        break;
    default:
        return VPU_RET_INVALID_PARAM;
    }

    if (val) {
        VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, val);

        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_RESET_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
            bm_vdi_log(coreIdx, W5_RESET_VPU, 2);
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }
        VpuWriteReg(coreIdx, W5_VPU_RESET_REQ, 0);
    }
    // Step3 : must clear GDI_BUS_CTRL after done SW_RESET
    /* pAttr->supportBackbone = TRUE */
    bm_vdi_fio_write_register(coreIdx, W5_BACKBONE_GDI_BUS_CTRL, 0x00);

    if (resetMode == SW_RESET_SAFETY || resetMode == SW_RESET_FORCE ) {
        ret = Wave5VpuSleepWake(coreIdx, FALSE, NULL, 0, TRUE);
    }

    return ret;
}

static int Wave5VpuWaitInterrupt(EncHandle handle, int32_t timeout)
{
    int reason = -1;

#ifdef SUPPORT_MULTI_INST_INTR
    /* check an interrupt for my instance during timeout */
    EnterLock(handle->coreIdx);
    reason = bm_vdi_wait_interrupt(handle->coreIdx, handle->instIndex, timeout);
    LeaveLock(handle->coreIdx);
#else
    int32_t  orgReason   = -1;
    int32_t  remain_intr = -1; // to set VPU_VINT_REASON for remain interrupt.
    int32_t  ownInt      = 0;
    uint32_t regVal;
    uint32_t IntrMask = ((1 << INT_WAVE5_BSBUF_FULL) | (1 << INT_WAVE5_ENC_PIC) |
                         (1 << INT_WAVE5_INIT_SEQ) | (1 << INT_WAVE5_ENC_SET_PARAM));

    EnterLock(handle->coreIdx);
    /* check one interrupt for current instance even if the number of interrupt triggered more than one. */
    reason = bm_vdi_wait_interrupt(handle->coreIdx, timeout);
    if (reason <= 0) {
        LeaveLock(handle->coreIdx);
        return reason;
    }

    remain_intr = reason;

    if (reason & (1 << INT_WAVE5_BSBUF_FULL)) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_BS_EMPTY_INST);
        regVal = (regVal & 0xffff);
        if (regVal & (1 << handle->instIndex)) {
            ownInt = 1;
            reason = (1 << INT_WAVE5_BSBUF_FULL);
            remain_intr &= ~(uint32_t)reason;

            regVal = regVal & ~(1UL << handle->instIndex);
            VpuWriteReg(handle->coreIdx, W5_RET_BS_EMPTY_INST, regVal);
        }
    }

    if (reason & (1 << INT_WAVE5_INIT_SEQ)) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST);
        regVal = (regVal & 0xffff);
        if (regVal & (1 << handle->instIndex)) {
            ownInt = 1;
            reason = (1 << INT_WAVE5_INIT_SEQ);
            remain_intr &= ~(uint32_t)reason;

            regVal = regVal & ~(1UL << handle->instIndex);
            VpuWriteReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST, regVal);
        }
    }

    if (reason & (1 << INT_WAVE5_ENC_PIC)) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST);
        regVal = (regVal & 0xffff);
        if (regVal & (1 << handle->instIndex)) {
            ownInt = 1;
            orgReason = reason;
            reason = (1 << INT_WAVE5_ENC_PIC);
            remain_intr &= ~(uint32_t)reason;
            /* Clear Low Latency Interrupt if two interrupts are occured */
            if (orgReason & (1 << INT_WAVE5_ENC_LOW_LATENCY)) {
                regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST);
                regVal = (regVal>>16);
                if (regVal & (1 << handle->instIndex)) {
                    remain_intr &= ~(1<<INT_WAVE5_ENC_LOW_LATENCY);
                    Wave5VpuClearInterrupt(handle->coreIdx, 1<<INT_WAVE5_ENC_LOW_LATENCY);
                }
            }
            regVal = regVal & ~(1UL << handle->instIndex);
            VpuWriteReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST, regVal);
        }
    }


    if (reason & (1 << INT_WAVE5_ENC_SET_PARAM)) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST);
        regVal = (regVal & 0xffff);
        if (regVal & (1 << handle->instIndex)) {
            ownInt = 1;
            reason = (1 << INT_WAVE5_ENC_SET_PARAM);
            remain_intr &= ~(uint32_t)reason;

            regVal = regVal & ~(1UL << handle->instIndex);
            VpuWriteReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST, regVal);
        }
    }

    if (reason & (1 << INT_WAVE5_ENC_LOW_LATENCY)) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST);
        regVal = (regVal>>16);
        if (regVal & (1 << handle->instIndex)) {
            ownInt = 1;
            reason = (1 << INT_WAVE5_ENC_LOW_LATENCY);
            remain_intr &= ~(uint32_t)reason;

            regVal = regVal & ~(1UL << handle->instIndex);
            regVal = (regVal << 16);
            VpuWriteReg(handle->coreIdx, W5_RET_QUEUE_CMD_DONE_INST, regVal);
        }
    }

    /* when interrupt is not for empty, dec_pic, init_seq. */
    if (reason & ~IntrMask) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_DONE_INSTANCE_INFO)&0xFF;
        if (regVal == handle->instIndex) {
            ownInt = 1;
            reason = (reason & ~IntrMask);
            remain_intr &= ~(uint32_t)reason;
        }
    }

    /* set remain interrupt flag to trigger interrupt next time. */
    VpuWriteReg(handle->coreIdx, W5_VPU_VINT_REASON, remain_intr);

    /* if there was no interrupt for current instance id, reason should be -1; */
    if (!ownInt)
        reason = -1;
    LeaveLock(handle->coreIdx);
#endif

    return reason;
}

static void Wave5VpuClearInterrupt(uint32_t coreIdx, uint32_t flags)
{
    uint32_t interruptReason;

    interruptReason = VpuReadReg(coreIdx, W5_VPU_VINT_REASON_USR);
    interruptReason &= ~flags;
    VpuWriteReg(coreIdx, W5_VPU_VINT_REASON_USR, interruptReason);
}

static int Wave5VpuGetBwReport(EncHandle handle, VPUBWData* bwMon)
{
    int32_t coreIdx;
    int ret = VPU_RET_SUCCESS;

    coreIdx = handle->coreIdx;

    ret = SendQuery(handle, GET_BW_REPORT);
    if (ret != VPU_RET_SUCCESS) {
        if (VpuReadReg(coreIdx, W5_RET_FAIL_REASON) == WAVE5_RESULT_NOT_READY)
            return VPU_RET_REPORT_NOT_READY;
        else
            return VPU_RET_QUERY_FAILURE;
    }

    bwMon->prpBwRead    = VpuReadReg(coreIdx, RET_QUERY_BW_PRP_AXI_READ)    * 16;
    bwMon->prpBwWrite   = VpuReadReg(coreIdx, RET_QUERY_BW_PRP_AXI_WRITE)   * 16;
    bwMon->fbdYRead     = VpuReadReg(coreIdx, RET_QUERY_BW_FBD_Y_AXI_READ)  * 16;
    bwMon->fbcYWrite    = VpuReadReg(coreIdx, RET_QUERY_BW_FBC_Y_AXI_WRITE) * 16;
    bwMon->fbdCRead     = VpuReadReg(coreIdx, RET_QUERY_BW_FBD_C_AXI_READ)  * 16;
    bwMon->fbcCWrite    = VpuReadReg(coreIdx, RET_QUERY_BW_FBC_C_AXI_WRITE) * 16;
    bwMon->priBwRead    = VpuReadReg(coreIdx, RET_QUERY_BW_PRI_AXI_READ)    * 16;
    bwMon->priBwWrite   = VpuReadReg(coreIdx, RET_QUERY_BW_PRI_AXI_WRITE)   * 16;
    bwMon->secBwRead    = VpuReadReg(coreIdx, RET_QUERY_BW_SEC_AXI_READ)    * 16;
    bwMon->secBwWrite   = VpuReadReg(coreIdx, RET_QUERY_BW_SEC_AXI_WRITE)   * 16;
    bwMon->procBwRead   = VpuReadReg(coreIdx, RET_QUERY_BW_PROC_AXI_READ)   * 16;
    bwMon->procBwWrite  = VpuReadReg(coreIdx, RET_QUERY_BW_PROC_AXI_WRITE)  * 16;

    return VPU_RET_SUCCESS;
}

/************************************************************************/
/*                       ENCODER functions                              */
/************************************************************************/
static int Wave5VpuBuildUpEncParam(EncHandle handle, VpuEncOpenParam* param)
{
    EncInfo* pEncInfo = handle->encInfo;
    VpuAttr* pAttr    = &s_VpuCoreAttributes[handle->coreIdx];
    int ret;

    int vpu_inst_flag = 0;
    int vpu_inst_flag_ori = 0;
    int enc_core_idx = 0;

    pEncInfo->streamRdPtrRegAddr      = W5_RET_ENC_RD_PTR;
    pEncInfo->streamWrPtrRegAddr      = W5_RET_ENC_WR_PTR;
    pEncInfo->currentPC               = W5_VCPU_CUR_PC;
    pEncInfo->busyFlagAddr            = W5_VPU_BUSY_STATUS;

    if ((pAttr->supportEncoders&(1<<param->bitstreamFormat)) == 0) {
        VLOG(ERR, "Unsupported feature!\n");
        return VPU_RET_UNSUPPORTED_FEATURE;
    }

    if (param->bitstreamFormat == VPU_CODEC_HEVC)
        handle->codecMode = W_HEVC_ENC;
    else /* if (param->bitstreamFormat == VPU_CODEC_AVC) */
        handle->codecMode = W_AVC_ENC;

    pEncInfo->vbWork.phys_addr = param->workBuffer;
    pEncInfo->vbWork.size      = param->workBufferSize;

    VLOG(DEBUG, "vbWork.phys_addr = 0x%lx\n", pEncInfo->vbWork.phys_addr);
    VLOG(DEBUG, "vbWork.size      = %d\n", pEncInfo->vbWork.size);

    VpuWriteReg(handle->coreIdx, W5_ADDR_WORK_BASE, pEncInfo->vbWork.phys_addr);
    VpuWriteReg(handle->coreIdx, W5_WORK_SIZE,      pEncInfo->vbWork.size);

    VpuWriteReg(handle->coreIdx, W5_VPU_BUSY_STATUS, 1);
    VpuWriteReg(handle->coreIdx, W5_RET_SUCCESS, 0); // for debug

    VpuWriteReg(handle->coreIdx, W5_CMD_ENC_VCORE_LIMIT, 1);

    if (handle->productId == PRODUCT_ID_521)
        VpuWriteReg(handle->coreIdx, W5_CMD_CREATE_INST_SUB_FRAME_SYNC, 0/*param->subFrameSyncEnable | param->subFrameSyncMode<<1*/ );

    Wave5BitIssueCommand(handle, W5_CREATE_INSTANCE);

    ret = enc_wait_vpu_busy(handle->coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {   // Check QUEUE_DONE
        VLOG(ERR, "timeout\n");
        if (handle->loggingEnable)
            bm_vdi_log(handle->coreIdx, W5_CREATE_INSTANCE, 2);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    if (VpuReadReg(handle->coreIdx, W5_RET_SUCCESS) == FALSE) {  // FAILED for adding into VCPU QUEUE
        uint32_t regVal = VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON);
        if (regVal == 2)
            return VPU_RET_INVALID_SFS_INSTANCE;
        else
            return VPU_RET_FAILURE;
    }

    pEncInfo->streamRdPtr         = param->bitstreamBuffer;
    pEncInfo->streamWrPtr         = param->bitstreamBuffer;
    pEncInfo->lineBufIntEn        = param->lineBufIntEn;
    pEncInfo->streamBufStartAddr  = param->bitstreamBuffer;
    pEncInfo->streamBufSize       = param->bitstreamBufferSize;
    pEncInfo->streamBufEndAddr    = param->bitstreamBuffer + param->bitstreamBufferSize;
    pEncInfo->stride              = 0;
    pEncInfo->initialInfoObtained = 0;

    vpu_inst_flag = 1 << handle->instIndex;
#if defined(BM_PCIE_MODE)
        enc_core_idx = handle->coreIdx/MAX_NUM_VPU_CORE_CHIP;
#else
        enc_core_idx = 0;
#endif
#ifdef __linux__
    vpu_inst_flag_ori = __atomic_load_n((int *)(p_vpu_enc_create_inst_flag[enc_core_idx]), __ATOMIC_SEQ_CST);
    __atomic_store_n((int *)p_vpu_enc_create_inst_flag[enc_core_idx], vpu_inst_flag|vpu_inst_flag_ori, __ATOMIC_SEQ_CST);
#elif _WIN32
    InterlockedExchange(&vpu_inst_flag_ori, *(p_vpu_enc_create_inst_flag[enc_core_idx]));
    InterlockedExchange((unsigned int*)p_vpu_enc_create_inst_flag[enc_core_idx], vpu_inst_flag | vpu_inst_flag_ori );
#endif
    return VPU_RET_SUCCESS;
}

static int Wave5VpuEncInitSeq(EncHandle handle)
{
    int32_t          coreIdx    = handle->coreIdx;
    EncInfo*         pEncInfo   = handle->encInfo;
    VpuEncOpenParam* pOpenParam = &pEncInfo->openParam;
    VpuEncWaveParam* pParam     = &pOpenParam->waveParam;
    uint32_t         regVal = 0, rotMirMode;
    int ret;

    /*==============================================*/
    /*  OPT_CUSTOM_GOP                              */
    /*==============================================*/
    /*
     * SET_PARAM + CUSTOM_GOP
     * only when gopPresetIdx == custom_gop, custom_gop related registers should be set
     */
    if (pParam->gopPresetIdx == VPU_GOP_PRESET_IDX_CUSTOM_GOP) {
        int i=0, j = 0;
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_SET_PARAM_OPTION, OPT_CUSTOM_GOP);
        VpuWriteReg(coreIdx, W5_CMD_ENC_CUSTOM_GOP_PARAM, pParam->gopParam.customGopSize);

        for (i=0 ; i<pParam->gopParam.customGopSize; i++) {
            regVal = ((pParam->gopParam.picParam[i].picType<<0)            |
                      (pParam->gopParam.picParam[i].pocOffset<<2)          |
                      (pParam->gopParam.picParam[i].picQp<<6)              |
                      (pParam->gopParam.picParam[i].numRefPicL0<<12)       |
                      ((pParam->gopParam.picParam[i].refPocL0&0x1F)<<14)   |
                      ((pParam->gopParam.picParam[i].refPocL1&0x1F)<<19)   |
                      (pParam->gopParam.picParam[i].temporalId<<24));
            VpuWriteReg(coreIdx, W5_CMD_ENC_CUSTOM_GOP_PIC_PARAM_0 + (i*4), regVal);
        }

        for (j = i; j < MAX_GOP_NUM; j++) {
            VpuWriteReg(coreIdx, W5_CMD_ENC_CUSTOM_GOP_PIC_PARAM_0 + (j*4), 0);
        }

        Wave5BitIssueCommand(handle, W5_ENC_SET_PARAM);

        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            if (handle->loggingEnable)
                bm_vdi_log(coreIdx, W5_ENC_SET_PARAM, 2);
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }

    }

    /*======================================================================*/
    /*  OPT_COMMON                                                          */
    /*      : the last SET_PARAM command should be called with OPT_COMMON   */
    /*======================================================================*/

    /* CMD_ENC_ROT_MODE :
     *          | hor_mir | ver_mir |   rot_angle     | rot_en |
     *              [4]       [3]         [2:1]           [0]
     */
    rotMirMode = 0;
    if (pEncInfo->rotationEnable == TRUE) {
        switch (pEncInfo->rotationAngle) {
        case   0: rotMirMode |= 0x0; break;
        case  90: rotMirMode |= 0x3; break;
        case 180: rotMirMode |= 0x5; break;
        case 270: rotMirMode |= 0x7; break;
        }
    }

    if (pEncInfo->mirrorEnable == TRUE) {
        switch (pEncInfo->mirrorDirection) {
        case MIRDIR_NONE :   rotMirMode |= 0x00; break;
        case MIRDIR_VER :    rotMirMode |= 0x09; break;
        case MIRDIR_HOR :    rotMirMode |= 0x11; break;
        case MIRDIR_HOR_VER: rotMirMode |= 0x19; break;
        }
    }

    CalcCropInfo(handle, pOpenParam, rotMirMode);

    /* SET_PARAM + COMMON */
    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_SET_PARAM_OPTION, OPT_COMMON);

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_SRC_SIZE,   pOpenParam->picHeight<<16 | pOpenParam->picWidth);

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MAP_ENDIAN, VDI_LITTLE_ENDIAN);

    if (handle->codecMode == W_AVC_ENC) {
        regVal = ((pParam->profile<<0)                    |
                  (pParam->level<<3)                      |
                  (pParam->internalBitDepth<<14)          |
                  (pParam->useLongTerm<<21)               |
                  (pParam->scalingListEnable<<22));
    } else {  // HEVC enc
        regVal = ((pParam->profile<<0)                    |
                  (pParam->level<<3)                      |
                  (pParam->tier<<12)                      |
                  (pParam->internalBitDepth<<14)          |
                  (pParam->useLongTerm<<21)               |
                  (pParam->scalingListEnable<<22)         |
                  (pParam->tmvpEnable<<23)                |
                  (pParam->saoEnable<<24)                 |
                  (pParam->skipIntraTrans<<25)            |
                  (pParam->strongIntraSmoothEnable<<27));
    }

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_SPS_PARAM,  regVal);

    regVal = ((pParam->losslessEnable)                |
              (pParam->constIntraPredFlag<<1)         |
              (pParam->lfCrossSliceBoundaryEnable<<2) |
              ((pParam->weightPredEnable&1)<<3)       |
              (pParam->wppEnable<<4)                  |
              (pParam->disableDeblk<<5)               |
              ((pParam->betaOffsetDiv2&0xF)<<6)       |
              ((pParam->tcOffsetDiv2&0xF)<<10)        |
              ((pParam->chromaCbQpOffset&0x1F)<<14)   |
              ((pParam->chromaCrQpOffset&0x1F)<<19)   |
              (pParam->transform8x8Enable<<29)        |
              (pParam->entropyCodingMode<<30));

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_PPS_PARAM,  regVal);

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_GOP_PARAM,  pParam->gopPresetIdx);

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INTRA_PARAM, ((pParam->decodingRefreshType<<0) |
                                                      (pParam->intraQP<<3) |
                                                      (pParam->intraPeriod<<16)));

    regVal  = ((pParam->useRecommendEncParam)     |
               (pParam->rdoSkip<<2)               |
               (pParam->lambdaScalingEnable<<3)   |
               (pParam->coefClearDisable<<4)      |
               (pParam->cuSizeMode<<5)            |
               (pParam->intraNxNEnable<<8)        |
               (pParam->maxNumMerge<<18)          |
               (pParam->customMDEnable<<20)       |
               (pParam->customLambdaEnable<<21)   |
               (pParam->monochromeEnable<<22));

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RDO_PARAM, regVal);
    if (handle->productId == PRODUCT_ID_521) {
        regVal = ((pOpenParam->cframe50Enable<<0)        |
                  (pOpenParam->cframe50LosslessEnable<<1)|
                  (pOpenParam->cframe50Tx16Y<<2)         |
                  (pOpenParam->cframe50Tx16C<<10)        |
                  (pOpenParam->cframe50_422<<18));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INPUT_SRC_PARAM, regVal);
    }

    if (handle->codecMode == W_AVC_ENC) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INTRA_REFRESH, pParam->intraMbRefreshArg<<16 | pParam->intraMbRefreshMode);
    } else {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INTRA_REFRESH, pParam->intraRefreshArg<<16 | pParam->intraRefreshMode);
    }


    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_FRAME_RATE, pOpenParam->frameRateInfo);
    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_TARGET_RATE, pOpenParam->bitRate);
    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_TARGET_RATE_BL, 0); // TODO

    if (handle->codecMode == W_AVC_ENC) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_PARAM, ((pOpenParam->rcEnable<<0)           |
                                                       (pParam->mbLevelRcEnable<<1)        |
                                                       (pParam->hvsQPEnable<<2)            |
                                                       (pParam->hvsQpScale<<4)             |
                                                       (pParam->bitAllocMode<<8)           |
                                                       (pParam->roiEnable<<13)             |
                                                       ((pParam->initialRcQp&0x3F)<<14)    |
                                                       (pOpenParam->vbvBufferSize<<20)));
    } else {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_PARAM, ((pOpenParam->rcEnable<<0)           |
                                                       (pParam->cuLevelRCEnable<<1)        |
                                                       (pParam->hvsQPEnable<<2)            |
                                                       (pParam->hvsQpScale<<4)             |
                                                       (pParam->bitAllocMode<<8)           |
                                                       (pParam->roiEnable<<13)             |
                                                       ((pParam->initialRcQp&0x3F)<<14)    |
                                                       (pOpenParam->vbvBufferSize<<20)));
    }

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_MIN_MAX_QP, ((pParam->minQpI<<0)          |
                                                        (pParam->maxQpI<<6)          |
                                                        (pParam->maxDeltaQp<<12)));

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_INTER_MIN_MAX_QP, ((pParam->minQpP << 0)   |
                                                              (pParam->maxQpP << 6)   |
                                                              (pParam->minQpB << 12)  |
                                                              (pParam->maxQpB << 18)));

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_BIT_RATIO_LAYER_0_3, ((pParam->fixedBitRatio[0]<<0)  |
                                                                 (pParam->fixedBitRatio[1]<<8)  |
                                                                 (pParam->fixedBitRatio[2]<<16) |
                                                                 (pParam->fixedBitRatio[3]<<24)));

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_BIT_RATIO_LAYER_4_7, ((pParam->fixedBitRatio[4]<<0)  |
                                                                 (pParam->fixedBitRatio[5]<<8)  |
                                                                 (pParam->fixedBitRatio[6]<<16) |
                                                                 (pParam->fixedBitRatio[7]<<24)));

    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_ROT_PARAM,  rotMirMode);


    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_BG_PARAM, ((pParam->bgDetectEnable)       |
                                                   (pParam->bgThrDiff<<1)         |
                                                   (pParam->bgThrMeanDiff<<10)    |
                                                   (pParam->bgLambdaQp<<18)       |
                                                   ((pParam->bgDeltaQp&0x1F)<<24) |
                                                   (handle->codecMode == W_AVC_ENC ? pParam->s2fmeDisable<<29 : 0)));


    if (handle->codecMode == W_HEVC_ENC || handle->codecMode == W_AVC_ENC) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_LAMBDA_ADDR, pParam->customLambdaAddr);

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CONF_WIN_TOP_BOT, pParam->confWinBot<<16 | pParam->confWinTop);
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CONF_WIN_LEFT_RIGHT, pParam->confWinRight<<16 | pParam->confWinLeft);

        if (handle->codecMode == W_AVC_ENC) {
            VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INDEPENDENT_SLICE, pParam->avcSliceArg<<16 | pParam->avcSliceMode);
        } else {
            VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INDEPENDENT_SLICE, pParam->independSliceModeArg<<16 | pParam->independSliceMode);
        }

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_USER_SCALING_LIST_ADDR, pParam->userScalingListAddr);
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_NUM_UNITS_IN_TICK, pParam->numUnitsInTick);
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_TIME_SCALE, pParam->timeScale);
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_NUM_TICKS_POC_DIFF_ONE, pParam->numTicksPocDiffOne);
    }

    if (handle->codecMode == W_HEVC_ENC) {
        regVal = ((pParam->pu04DeltaRate&0xFF)                 |
                  ((pParam->pu04IntraPlanarDeltaRate&0xFF)<<8) |
                  ((pParam->pu04IntraDcDeltaRate&0xFF)<<16)    |
                  ((pParam->pu04IntraAngleDeltaRate&0xFF)<<24));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU04, regVal);

        regVal = ((pParam->pu08DeltaRate&0xFF)                 |
                  ((pParam->pu08IntraPlanarDeltaRate&0xFF)<<8) |
                  ((pParam->pu08IntraDcDeltaRate&0xFF)<<16)    |
                  ((pParam->pu08IntraAngleDeltaRate&0xFF)<<24));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU08, regVal);

        regVal = ((pParam->pu16DeltaRate&0xFF)                 |
                  ((pParam->pu16IntraPlanarDeltaRate&0xFF)<<8) |
                  ((pParam->pu16IntraDcDeltaRate&0xFF)<<16)    |
                  ((pParam->pu16IntraAngleDeltaRate&0xFF)<<24));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU16, regVal);

        regVal = ((pParam->pu32DeltaRate&0xFF)                 |
                  ((pParam->pu32IntraPlanarDeltaRate&0xFF)<<8) |
                  ((pParam->pu32IntraDcDeltaRate&0xFF)<<16)    |
                  ((pParam->pu32IntraAngleDeltaRate&0xFF)<<24));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU32, regVal);

        regVal = ((pParam->cu08IntraDeltaRate&0xFF)        |
                  ((pParam->cu08InterDeltaRate&0xFF)<<8)   |
                  ((pParam->cu08MergeDeltaRate&0xFF)<<16));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_CU08, regVal);

        regVal = ((pParam->cu16IntraDeltaRate&0xFF)        |
                  ((pParam->cu16InterDeltaRate&0xFF)<<8)   |
                  ((pParam->cu16MergeDeltaRate&0xFF)<<16));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_CU16, regVal);

        regVal = ((pParam->cu32IntraDeltaRate&0xFF)        |
                  ((pParam->cu32InterDeltaRate&0xFF)<<8)   |
                  ((pParam->cu32MergeDeltaRate&0xFF)<<16));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_CU32, regVal);


        regVal = (pParam->dependSliceModeArg<<16 | pParam->dependSliceMode);
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_DEPENDENT_SLICE, regVal);

        regVal = ((pParam->nrYEnable<<0)       |
                  (pParam->nrCbEnable<<1)      |
                  (pParam->nrCrEnable<<2)      |
                  (pParam->nrNoiseEstEnable<<3)|
                  (pParam->nrNoiseSigmaY<<4)   |
                  (pParam->nrNoiseSigmaCb<<12) |
                  (pParam->nrNoiseSigmaCr<<20));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_NR_PARAM, regVal);

        regVal = ((pParam->nrIntraWeightY<<0)  |
                  (pParam->nrIntraWeightCb<<5) |
                  (pParam->nrIntraWeightCr<<10)|
                  (pParam->nrInterWeightY<<15) |
                  (pParam->nrInterWeightCb<<20)|
                  (pParam->nrInterWeightCr<<25));
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_NR_WEIGHT, regVal);

    }

    Wave5BitIssueCommand(handle, W5_ENC_SET_PARAM);

    ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "timeout\n");
        if (handle->loggingEnable)
            bm_vdi_log(coreIdx, W5_ENC_SET_PARAM, 2);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    if (VpuReadReg(coreIdx, W5_RET_SUCCESS) == 0) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON);
        if ( regVal == WAVE5_QUEUEING_FAIL)
            return VPU_RET_QUEUEING_FAILURE;
        else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
            return VPU_RET_MEMORY_ACCESS_VIOLATION;
        else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        else
            return VPU_RET_FAILURE;
    }

    return VPU_RET_SUCCESS;
}

static int Wave5VpuEncGetSeqInfo(EncHandle handle, VpuEncInitialInfo* info)
{
    EncInfo*    pEncInfo = handle->encInfo;
    uint32_t    regVal;
    int     ret = VPU_RET_SUCCESS;

    /* Send QUERY cmd */
    ret = SendQuery(handle, GET_RESULT);
    if (ret != VPU_RET_SUCCESS) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON);
        if (regVal == WAVE5_RESULT_NOT_READY)
            return VPU_RET_REPORT_NOT_READY;
        else if(regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
            return VPU_RET_MEMORY_ACCESS_VIOLATION;
        else
            return VPU_RET_QUERY_FAILURE;
    }

    if (handle->loggingEnable)
        bm_vdi_log(handle->coreIdx, W5_INIT_SEQ, 0);

    regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_STATUS);

    pEncInfo->instanceQueueCount = (regVal>>16)&0xff;
    pEncInfo->totalQueueCount    = (regVal & 0xffff);

    if (VpuReadReg(handle->coreIdx, W5_RET_ENC_ENCODING_SUCCESS) != 1) {
        info->seqInitErrReason = VpuReadReg(handle->coreIdx, W5_RET_ENC_ERR_INFO);
        ret = VPU_RET_FAILURE;
    } else {
        info->warnInfo = VpuReadReg(handle->coreIdx, W5_RET_ENC_WARN_INFO);
    }

    VpuReadReg(handle->coreIdx, W5_RET_DONE_INSTANCE_INFO);

    info->minFrameBufferCount   = VpuReadReg(handle->coreIdx, W5_RET_ENC_NUM_REQUIRED_FB);
    info->minSrcFrameCount      = VpuReadReg(handle->coreIdx, W5_RET_ENC_MIN_SRC_BUF_NUM);
    info->maxLatencyPictures    = VpuReadReg(handle->coreIdx, W5_RET_ENC_PIC_MAX_LATENCY_PICTURES);

    return ret;
}

/* Register compressed frame buffer */
static int Wave5VpuEncRegisterCFramebuffer(EncHandle inst, VpuFrameBuffer* fbArr,
                                           uint32_t count)
{
    int coreIdx   = inst->coreIdx;
    int codecMode = inst->codecMode;
    EncInfo* pEncInfo = inst->encInfo;
    VpuEncOpenParam* pOpenParam = &pEncInfo->openParam;
    int q, j, i, remain, idx;
    int bufHeight = 0, bufWidth = 0;
    int startNo, endNo, stride;
    uint32_t regVal  = 0;
    uint32_t picSize, mvColSize, fbcYTblSize, fbcCTblSize, subSampledSize;
    uint32_t endian, nv21=0, cbcrInterleave = 0;
    uint32_t lumaStride, chromaStride;
    uint32_t addrY, addrCb/*, addrCr*/;
    vpu_buffer_t vbMV = {0,};
    vpu_buffer_t vbFbcYTbl = {0,};
    vpu_buffer_t vbFbcCTbl = {0,};
    vpu_buffer_t vbSubSamBuf = {0,};
    int ret;

    stride  = pEncInfo->stride;

    if (codecMode == W_HEVC_ENC) {
        bufWidth = VPU_ALIGN8(pOpenParam->picWidth);
        bufHeight= VPU_ALIGN8(pOpenParam->picHeight);

        if ((pEncInfo->rotationAngle != 0 || pEncInfo->mirrorDirection != 0) &&
            !(pEncInfo->rotationAngle == 180 && pEncInfo->mirrorDirection == MIRDIR_HOR_VER)) {
            bufWidth  = VPU_ALIGN32(pOpenParam->picWidth);
            bufHeight = VPU_ALIGN32(pOpenParam->picHeight);
        }

        if (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270) {
            bufWidth  = VPU_ALIGN32(pOpenParam->picHeight);
            bufHeight = VPU_ALIGN32(pOpenParam->picWidth);
        }
    } else {
        bufWidth = VPU_ALIGN16(pOpenParam->picWidth);
        bufHeight= VPU_ALIGN16(pOpenParam->picHeight);

        if ((pEncInfo->rotationAngle != 0 || pEncInfo->mirrorDirection != 0) &&
            !(pEncInfo->rotationAngle == 180 && pEncInfo->mirrorDirection == MIRDIR_HOR_VER)) {
            bufWidth  = VPU_ALIGN16(pOpenParam->picWidth);
            bufHeight = VPU_ALIGN16(pOpenParam->picHeight);
        }

        if (pEncInfo->rotationAngle == 90 || pEncInfo->rotationAngle == 270) {
            bufWidth  = VPU_ALIGN16(pOpenParam->picHeight);
            bufHeight = VPU_ALIGN16(pOpenParam->picWidth);
        }
    }

    if (codecMode == W_HEVC_ENC) {
        mvColSize  = WAVE5_ENC_HEVC_MVCOL_BUF_SIZE(bufWidth, bufHeight);
        mvColSize  = VPU_ALIGN16(mvColSize);
    } else /* if (codecMode == W_AVC_ENC) */ {
        mvColSize  = WAVE5_ENC_AVC_MVCOL_BUF_SIZE(bufWidth, bufHeight);
    }

    fbcYTblSize = WAVE5_FBC_LUMA_TABLE_SIZE(bufWidth, bufHeight);
    fbcYTblSize = VPU_ALIGN16(fbcYTblSize);

    fbcCTblSize = WAVE5_FBC_CHROMA_TABLE_SIZE(bufWidth, bufHeight);
    fbcCTblSize = VPU_ALIGN16(fbcCTblSize);

    if (pOpenParam->bitstreamFormat == VPU_CODEC_AVC)
        subSampledSize = WAVE5_AVC_SUBSAMPLED_ONE_SIZE(bufWidth, bufHeight);
    else
        subSampledSize = WAVE5_SUBSAMPLED_ONE_SIZE(bufWidth, bufHeight);

    picSize = (bufWidth<<16) | bufHeight;

    nv21 = 0;
    cbcrInterleave = 0;

    vbMV.phys_addr = pOpenParam->MV.pa;
    vbMV.size      = pOpenParam->MV.size;

    vbFbcYTbl.phys_addr = pOpenParam->FbcYTbl.pa;
    vbFbcYTbl.size      = pOpenParam->FbcYTbl.size;

    vbFbcCTbl.phys_addr = pOpenParam->FbcCTbl.pa;
    vbFbcCTbl.size      = pOpenParam->FbcCTbl.size;

    vbSubSamBuf.phys_addr = pOpenParam->SubSamBuf.pa;
    vbSubSamBuf.size      = pOpenParam->SubSamBuf.size;

    VLOG(DEBUG, "vbMV.phys_addr        = 0x%lx, size = %d\n",
         vbMV.phys_addr, vbMV.size);
    VLOG(DEBUG, "vbFbcYTbl.phys_addr   = 0x%lx, size = %d\n",
         vbFbcYTbl.phys_addr, vbFbcYTbl.size);
    VLOG(DEBUG, "vbFbcCTbl.phys_addr   = 0x%lx, size = %d\n",
         vbFbcCTbl.phys_addr, vbFbcCTbl.size);
    VLOG(DEBUG, "vbSubSamBuf.phys_addr = 0x%lx, size = %d\n",
         vbSubSamBuf.phys_addr, vbSubSamBuf.size);

    /* Set sub-sampled buffer base addr */
    VpuWriteReg(coreIdx, W5_ADDR_SUB_SAMPLED_FB_BASE, vbSubSamBuf.phys_addr);
    /* Set sub-sampled buffer size for one frame */
    VpuWriteReg(coreIdx, W5_SUB_SAMPLED_ONE_FB_SIZE, subSampledSize);

    endian = bm_vdi_convert_endian(fbArr[0].endian);

    VpuWriteReg(coreIdx, W5_PIC_SIZE, picSize);

    /* Set stride of Luma/Chroma for compressed buffer */
    if ((pEncInfo->rotationAngle != 0 || pEncInfo->mirrorDirection != 0) &&
        !(pEncInfo->rotationAngle == 180 && pEncInfo->mirrorDirection == MIRDIR_HOR_VER)){
        lumaStride = VPU_ALIGN32(bufWidth)*(pOpenParam->waveParam.internalBitDepth >8 ? 5 : 4);
        lumaStride = VPU_ALIGN32(lumaStride);
        chromaStride = VPU_ALIGN16(bufWidth/2)*(pOpenParam->waveParam.internalBitDepth >8 ? 5 : 4);
        chromaStride = VPU_ALIGN32(chromaStride);
    } else {
        lumaStride = VPU_ALIGN16(pOpenParam->picWidth)*(pOpenParam->waveParam.internalBitDepth >8 ? 5 : 4);
        lumaStride = VPU_ALIGN32(lumaStride);

        chromaStride = VPU_ALIGN16(pOpenParam->picWidth/2)*(pOpenParam->waveParam.internalBitDepth >8 ? 5 : 4);
        chromaStride = VPU_ALIGN32(chromaStride);
    }

    VpuWriteReg(coreIdx, W5_FBC_STRIDE, (lumaStride<<16) | chromaStride);

    cbcrInterleave = pOpenParam->cbcrInterleave;
    stride = pEncInfo->stride;
    regVal =(nv21 << 29) | (cbcrInterleave << 16) | (stride);
    VpuWriteReg(coreIdx, W5_COMMON_PIC_INFO, regVal);

    remain = count;
    q      = (remain+7)/8;
    idx    = 0;
    for (j=0; j<q; j++) {
        regVal = (endian<<16) | (j==q-1)<<4 | ((j==0)<<3);//lint !e514
        regVal |= (pOpenParam->enableNonRefFbcWrite<< 26);
        VpuWriteReg(coreIdx, W5_SFB_OPTION, regVal);

        startNo = j*8;
        endNo   = startNo + (remain>=8 ? 8 : remain) - 1;

        VpuWriteReg(coreIdx, W5_SET_FB_NUM, (startNo<<8)|endNo);

        for (i=0; i<8 && i<remain; i++) {
            addrY  = fbArr[i+startNo].bufY;
            addrCb = fbArr[i+startNo].bufCb;
            //addrCr = fbArr[i+startNo].bufCr;

            VpuWriteReg(coreIdx, W5_ADDR_LUMA_BASE0 + (i<<4), addrY);
            VpuWriteReg(coreIdx, W5_ADDR_CB_BASE0   + (i<<4), addrCb);
            APIDPRINT("REGISTER FB[%02d] Y(0x%08x), Cb(0x%08x) ", i, addrY, addrCb);

            /* Luma FBC offset table */
            VpuWriteReg(coreIdx, W5_ADDR_FBC_Y_OFFSET0 + (i<<4), vbFbcYTbl.phys_addr+idx*fbcYTblSize);
            /* Chroma FBC offset table */
            VpuWriteReg(coreIdx, W5_ADDR_FBC_C_OFFSET0 + (i<<4), vbFbcCTbl.phys_addr+idx*fbcCTblSize);

            VpuWriteReg(coreIdx, W5_ADDR_MV_COL0  + (i<<2), vbMV.phys_addr+idx*mvColSize);

            APIDPRINT("Yo(0x%08x) Co(0x%08x), Mv(0x%08x)\n",
                      vbFbcYTbl.phys_addr+idx*fbcYTblSize,
                      vbFbcCTbl.phys_addr+idx*fbcCTblSize,
                      vbMV.phys_addr+idx*mvColSize);
            idx++;
        }
        remain -= i;

        Wave5BitIssueCommand(inst, W5_SET_FB);

        ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
        if (ret == -1) {
            VLOG(ERR, "timeout\n");
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        }
    }

    regVal = VpuReadReg(coreIdx, W5_RET_SUCCESS);
    if (regVal == 0) {
        return VPU_RET_FAILURE;
    }

    ret = Wave5ConfigSecAXI(coreIdx, inst->codecMode, &pEncInfo->secAxiInfo,
                            pOpenParam->picWidth, pOpenParam->picHeight,
                            pOpenParam->waveParam.profile, pOpenParam->waveParam.level);
    if (ret < 0) {
        return VPU_RET_INSUFFICIENT_RESOURCE;
    }

    return VPU_RET_SUCCESS;
}

static int Wave5VpuEncode(EncHandle handle, VpuEncParam* option)
{
    int32_t         coreIdx    = handle->coreIdx;
    EncInfo*        pEncInfo   = handle->encInfo;
    VpuEncOpenParam* pOpenParam = &pEncInfo->openParam;
    VpuFrameBuffer* pSrcFrame   = option->sourceFrame;
    int32_t         srcFrameFormat, srcPixelFormat, packedFormat;
    uint32_t        regVal = 0, bsEndian;
    uint32_t        srcStrideC = 0;
    BOOL            justified = WTL_RIGHT_JUSTIFIED;
    uint32_t        formatNo  = WTL_PIXEL_8BIT;


    switch (pOpenParam->srcFormat) {
    case FORMAT_420_P10_16BIT_MSB:
    case FORMAT_YUYV_P10_16BIT_MSB:
    case FORMAT_YVYU_P10_16BIT_MSB:
    case FORMAT_UYVY_P10_16BIT_MSB:
    case FORMAT_VYUY_P10_16BIT_MSB:
        justified = WTL_RIGHT_JUSTIFIED;
        formatNo  = WTL_PIXEL_16BIT;
        break;
    case FORMAT_420_P10_16BIT_LSB:
    case FORMAT_YUYV_P10_16BIT_LSB:
    case FORMAT_YVYU_P10_16BIT_LSB:
    case FORMAT_UYVY_P10_16BIT_LSB:
    case FORMAT_VYUY_P10_16BIT_LSB:
        justified = WTL_LEFT_JUSTIFIED;
        formatNo  = WTL_PIXEL_16BIT;
        break;
    case FORMAT_420_P10_32BIT_MSB:
    case FORMAT_YUYV_P10_32BIT_MSB:
    case FORMAT_YVYU_P10_32BIT_MSB:
    case FORMAT_UYVY_P10_32BIT_MSB:
    case FORMAT_VYUY_P10_32BIT_MSB:
        justified = WTL_RIGHT_JUSTIFIED;
        formatNo  = WTL_PIXEL_32BIT;
        break;
    case FORMAT_420_P10_32BIT_LSB:
    case FORMAT_YUYV_P10_32BIT_LSB:
    case FORMAT_YVYU_P10_32BIT_LSB:
    case FORMAT_UYVY_P10_32BIT_LSB:
    case FORMAT_VYUY_P10_32BIT_LSB:
        justified = WTL_LEFT_JUSTIFIED;
        formatNo  = WTL_PIXEL_32BIT;
        break;
    case FORMAT_420:
    case FORMAT_YUYV:
    case FORMAT_YVYU:
    case FORMAT_UYVY:
    case FORMAT_VYUY:
        justified = WTL_LEFT_JUSTIFIED;
        formatNo  = WTL_PIXEL_8BIT;
        break;
    default:
        return VPU_RET_FAILURE;
    }
    packedFormat = (pOpenParam->packedFormat >= 1) ? 1 : 0;

    srcFrameFormat = (packedFormat<<2   |
                      pOpenParam->cbcrInterleave<<1  |
                      pOpenParam->nv21);

    switch (pOpenParam->packedFormat) {     // additional packed format (interleave & nv21 bit are used to present these modes)
    case PACKED_YVYU:
        srcFrameFormat = 0x5;
        break;
    case PACKED_UYVY:
        srcFrameFormat = 0x6;
        break;
    case PACKED_VYUY:
        srcFrameFormat = 0x7;
        break;
    default:
        break;
    }

    srcPixelFormat = justified<<2 | formatNo;
    if (pOpenParam->cframe50Enable == TRUE) {
        srcFrameFormat = 0;
    }

    regVal = bm_vdi_convert_endian(VPU_STREAM_ENDIAN);
    /* NOTE: When endian mode is 0, SDMA reads MSB first */
    bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

    VpuWriteReg(coreIdx, W5_CMD_ENC_BS_START_ADDR, option->picStreamBufferAddr);
    VpuWriteReg(coreIdx, W5_CMD_ENC_BS_SIZE, option->picStreamBufferSize);
    pEncInfo->streamBufStartAddr = option->picStreamBufferAddr;
    pEncInfo->streamBufSize = option->picStreamBufferSize;
    pEncInfo->streamBufEndAddr = option->picStreamBufferAddr + option->picStreamBufferSize;

    VpuWriteReg(coreIdx, W5_BS_OPTION, (pOpenParam->lowLatencyMode<<7) | (pEncInfo->lineBufIntEn<<6) | bsEndian);

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_AXI_SEL, DEFAULT_SRC_AXI);
    /* Secondary AXI */
    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_USE_SEC_AXI,  (pEncInfo->secAxiInfo.useEncRdoEnable<<11) | (pEncInfo->secAxiInfo.useEncLfEnable<<15));

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_REPORT_ENDIAN, VDI_128BIT_LITTLE_ENDIAN);

    if (option->codeOption.implicitHeaderEncode == 1) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CODE_OPTION, (CODEOPT_ENC_HEADER_IMPLICIT | CODEOPT_ENC_VCL | // implicitly encode a header(headers) for generating bitstream. (to encode a header only, use ENC_PUT_VIDEO_HEADER for GiveCommand)
                                                          (option->codeOption.encodeAUD<<5)             |
                                                          (option->codeOption.encodeEOS<<6)             |
                                                          (option->codeOption.encodeEOB<<7)));
    } else {
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CODE_OPTION, ((option->codeOption.implicitHeaderEncode<<0)   |
                                                          (option->codeOption.encodeVCL<<1)              |
                                                          (option->codeOption.encodeVPS<<2)              |
                                                          (option->codeOption.encodeSPS<<3)              |
                                                          (option->codeOption.encodePPS<<4)              |
                                                          (option->codeOption.encodeAUD<<5)              |
                                                          (option->codeOption.encodeEOS<<6)              |
                                                          (option->codeOption.encodeEOB<<7)              |
                                                          (option->codeOption.encodeFiller<<8)));
    }

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_PIC_PARAM, ((option->skipPicture<<0)         |
                                                    (option->forcePicQpEnable<<1)    |
                                                    (option->forcePicQpI<<2)         |
                                                    (option->forcePicQpP<<8)         |
                                                    (option->forcePicQpB<<14)        |
                                                    (option->forcePicTypeEnable<<20) |
                                                    (option->forcePicType<<21)       |
                                                    (option->forceAllCtuCoefDropEnable<<24) |
                                                    (0<<25)));


    if (option->srcEndFlag == 1)
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_PIC_IDX, 0xFFFFFFFF);               // no more source image.
    else
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_PIC_IDX, option->srcIdx);

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_ADDR_Y, pSrcFrame->bufY);
    if (pOpenParam->cbcrOrder == CBCR_ORDER_NORMAL) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_ADDR_U, pSrcFrame->bufCb);
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_ADDR_V, pSrcFrame->bufCr);
    } else {
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_ADDR_U, pSrcFrame->bufCr);
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_ADDR_V, pSrcFrame->bufCb);
    }

    if (pOpenParam->cframe50Enable == TRUE) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CF50_Y_OFFSET_TABLE_ADDR,  option->OffsetTblBuffer->bufY);
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CF50_CB_OFFSET_TABLE_ADDR, option->OffsetTblBuffer->bufCb);
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CF50_CR_OFFSET_TABLE_ADDR, option->OffsetTblBuffer->bufCr);
        APIDPRINT("CFRAME Data addr=%x, %x, %x\n", pSrcFrame->bufY, pSrcFrame->bufCb, pSrcFrame->bufCr);
        APIDPRINT("CFRAME OFS  addr=%x, %x, %x\n", option->OffsetTblBuffer->bufY,
                  option->OffsetTblBuffer->bufCb, option->OffsetTblBuffer->bufCr);
    }

    if (formatNo == WTL_PIXEL_32BIT) {
        srcStrideC = VPU_ALIGN16(pSrcFrame->stride/2)*(1<<pSrcFrame->cbcrInterleave);
        if ( pSrcFrame->cbcrInterleave == 1)
            srcStrideC = pSrcFrame->stride;
    } else {
        srcStrideC = (pSrcFrame->cbcrInterleave == 1) ? pSrcFrame->stride : (pSrcFrame->stride>>1);
    }

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_STRIDE, (pSrcFrame->stride<<16) | srcStrideC );

    regVal = bm_vdi_convert_endian(VPU_SOURCE_ENDIAN);
    bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_FORMAT, ((srcFrameFormat<<0)  |
                                                     (srcPixelFormat<<3)  |
                                                     (bsEndian<<6)));

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CUSTOM_MAP_OPTION_ADDR, option->customMapOpt.addrCustomMap);

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CUSTOM_MAP_OPTION_PARAM, ((option->customMapOpt.customRoiMapEnable << 0)    |
                                                                  (option->customMapOpt.roiAvgQp << 1)              |
                                                                  (option->customMapOpt.customLambdaMapEnable<< 8)  |
                                                                  (option->customMapOpt.customModeMapEnable<< 9)    |
                                                                  (option->customMapOpt.customCoefDropEnable<< 10)));

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_LONGTERM_PIC, (option->useCurSrcAsLongtermPic<<0) | (option->useLongtermRef<<1));

    if (handle->codecMode == W_HEVC_ENC || handle->codecMode == W_AVC_ENC) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_WP_PIXEL_SIGMA_Y,  option->wpPix.SigmaY);
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_WP_PIXEL_SIGMA_C, (option->wpPix.SigmaCr<<16) | option->wpPix.SigmaCb);
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_WP_PIXEL_MEAN_Y, option->wpPix.MeanY);
        VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_WP_PIXEL_MEAN_C, (option->wpPix.MeanCr<<16) | (option->wpPix.MeanCb));
    }

    Wave5BitIssueCommand(handle, W5_ENC_PIC);

    int ret = enc_wait_vpu_busy(handle->coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {   // Check QUEUE_DONE
        VLOG(ERR, "timeout\n");
        if (handle->loggingEnable)
            bm_vdi_log(handle->coreIdx, W5_ENC_PIC, 2);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_STATUS);

    pEncInfo->instanceQueueCount = (regVal>>16)&0xff;
    pEncInfo->totalQueueCount    = (regVal & 0xffff);

    regVal = VpuReadReg(handle->coreIdx, W5_RET_SUCCESS);
    if (regVal == FALSE) {           // FAILED for adding a command into VCPU QUEUE
        regVal = VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON);
        if (regVal == 1) {
            VLOG(ERR, "queueing failure\n");
            return VPU_RET_QUEUEING_FAILURE;
        } else if (regVal == 16) {
            VLOG(ERR, "cp0 exception\n");
            return VPU_RET_CP0_EXCEPTION;
        } else {
            VLOG(ERR, "failure\n");
            return VPU_RET_FAILURE;
        }
    }

    return VPU_RET_SUCCESS;
}

static int Wave5VpuEncGetResult(EncHandle handle, VpuEncOutputInfo* result)
{
    int32_t   coreIdx  = handle->coreIdx;
    EncInfo*  pEncInfo = handle->encInfo;
    uint32_t  encodingSuccess;
    uint32_t  regVal;
    int   ret = VPU_RET_SUCCESS;

    ret = SendQuery(handle, GET_RESULT);
    if (ret != VPU_RET_SUCCESS) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON);
        if (regVal == WAVE5_RESULT_NOT_READY)
            return VPU_RET_REPORT_NOT_READY;
        else if(regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
            return VPU_RET_MEMORY_ACCESS_VIOLATION;
        else
            return VPU_RET_QUERY_FAILURE;
    }
    if (handle->loggingEnable)
        bm_vdi_log(coreIdx, W5_ENC_PIC, 0);

    regVal = VpuReadReg(coreIdx, W5_RET_QUEUE_STATUS);

    pEncInfo->instanceQueueCount = (regVal>>16)&0xff;
    pEncInfo->totalQueueCount    = (regVal & 0xffff);

    encodingSuccess = VpuReadReg(coreIdx, W5_RET_ENC_ENCODING_SUCCESS);
    if (encodingSuccess == FALSE) {
        result->errorReason = VpuReadReg(coreIdx, W5_RET_ENC_ERR_INFO);
        if (result->errorReason == WAVE5_SYSERR_ENC_VLC_BUF_FULL) {
            return VPU_RET_VLC_BUF_FULL;
        }
        return VPU_RET_FAILURE;
    } else {
        result->warnInfo = VpuReadReg(handle->coreIdx, W5_RET_ENC_WARN_INFO);
    }

    result->encPicCnt       = VpuReadReg(coreIdx, W5_RET_ENC_PIC_NUM);
    regVal= VpuReadReg(coreIdx, W5_RET_ENC_PIC_TYPE);
    result->picType         = regVal & 0xFFFF;

    result->encNuts         = VpuReadReg(coreIdx, W5_RET_ENC_NUT_0);
    result->encNuts1        = VpuReadReg(coreIdx, W5_RET_ENC_NUT_1);
    result->reconFrameIndex = VpuReadReg(coreIdx, W5_RET_ENC_PIC_IDX);

    if (result->reconFrameIndex >= 0)
        result->reconFrame  = pEncInfo->frameBufPool[result->reconFrameIndex];
    VpuReadReg(coreIdx, W5_RET_ENC_SVC_LAYER); // TODO

    result->numOfSlices     = VpuReadReg(coreIdx, W5_RET_ENC_PIC_SLICE_NUM);
    result->picSkipped      = VpuReadReg(coreIdx, W5_RET_ENC_PIC_SKIP);
    result->numOfIntra      = VpuReadReg(coreIdx, W5_RET_ENC_PIC_NUM_INTRA);
    result->numOfMerge      = VpuReadReg(coreIdx, W5_RET_ENC_PIC_NUM_MERGE);
    result->numOfSkipBlock  = VpuReadReg(coreIdx, W5_RET_ENC_PIC_NUM_SKIP);

    result->avgCtuQp        = VpuReadReg(coreIdx, W5_RET_ENC_PIC_AVG_CTU_QP);
    result->encPicByte      = VpuReadReg(coreIdx, W5_RET_ENC_PIC_BYTE);

    result->encGopPicIdx    = VpuReadReg(coreIdx, W5_RET_ENC_GOP_PIC_IDX);
    result->encPicPoc       = VpuReadReg(coreIdx, W5_RET_ENC_PIC_POC);
    result->encSrcIdx       = VpuReadReg(coreIdx, W5_RET_ENC_USED_SRC_IDX);
    pEncInfo->streamWrPtr   = VpuReadReg(coreIdx, pEncInfo->streamWrPtrRegAddr);
    pEncInfo->streamWrPtr   = MapToAddr40Bit(coreIdx, pEncInfo->streamWrPtr);
    pEncInfo->streamRdPtr   = VpuReadReg(coreIdx, pEncInfo->streamRdPtrRegAddr);
    pEncInfo->streamRdPtr   = MapToAddr40Bit(coreIdx, pEncInfo->streamRdPtr);

    result->picDistortionLow    = VpuReadReg(coreIdx, W5_RET_ENC_PIC_DIST_LOW);
    result->picDistortionHigh   = VpuReadReg(coreIdx, W5_RET_ENC_PIC_DIST_HIGH);

    result->bitstreamBuffer = VpuReadReg(coreIdx, pEncInfo->streamRdPtrRegAddr);
    result->bitstreamBuffer = MapToAddr40Bit(coreIdx, result->bitstreamBuffer);

    result->rdPtr = pEncInfo->streamRdPtr;
    result->wrPtr = pEncInfo->streamWrPtr;

    if (result->reconFrameIndex == RECON_IDX_FLAG_HEADER_ONLY) //result for header only(no vcl) encoding
        result->bitstreamSize   = result->encPicByte;
    else if (result->reconFrameIndex < 0)
        result->bitstreamSize   = 0;
    else
        result->bitstreamSize   = result->encPicByte;

    result->encHostCmdTick             = VpuReadReg(coreIdx, RET_ENC_HOST_CMD_TICK);
    result->encPrepareStartTick        = VpuReadReg(coreIdx, RET_ENC_PREPARE_START_TICK);
    result->encPrepareEndTick          = VpuReadReg(coreIdx, RET_ENC_PREPARE_END_TICK);
    result->encProcessingStartTick     = VpuReadReg(coreIdx, RET_ENC_PROCESSING_START_TICK);
    result->encProcessingEndTick       = VpuReadReg(coreIdx, RET_ENC_PROCESSING_END_TICK);
    result->encEncodeStartTick         = VpuReadReg(coreIdx, RET_ENC_ENCODING_START_TICK);
    result->encEncodeEndTick           = VpuReadReg(coreIdx, RET_ENC_ENCODING_END_TICK);

    if ( pEncInfo->firstCycleCheck == FALSE ) {
        result->frameCycle   = (result->encEncodeEndTick - result->encHostCmdTick) * pEncInfo->cyclePerTick;
        pEncInfo->firstCycleCheck = TRUE;
    } else {
        result->frameCycle   = (result->encEncodeEndTick - pEncInfo->PrevEncodeEndTick) * pEncInfo->cyclePerTick;
        if ( pEncInfo->PrevEncodeEndTick < result->encHostCmdTick)
            result->frameCycle   = (result->encEncodeEndTick - result->encHostCmdTick) * pEncInfo->cyclePerTick;
    }
    pEncInfo->PrevEncodeEndTick = result->encEncodeEndTick;
    result->prepareCycle = (result->encPrepareEndTick    - result->encPrepareStartTick) * pEncInfo->cyclePerTick;
    result->processing   = (result->encProcessingEndTick - result->encProcessingStartTick) * pEncInfo->cyclePerTick;
    result->EncodedCycle = (result->encEncodeEndTick     - result->encEncodeStartTick) * pEncInfo->cyclePerTick;

    return VPU_RET_SUCCESS;
}

static int Wave5VpuEncGetHeader(EncHandle handle, VpuEncHeaderParam* encHeaderParam)
{
    int32_t   coreIdx  = handle->coreIdx;
    EncInfo*  pEncInfo = handle->encInfo;
    uint32_t  regVal = 0, bsEndian;

    EnterLock(coreIdx);

    regVal = bm_vdi_convert_endian(VPU_STREAM_ENDIAN);

    /* NOTE: When endian mode is 0, SDMA reads MSB first */
    bsEndian = (~regVal&VDI_128BIT_ENDIAN_MASK);

    VpuWriteReg(coreIdx, W5_CMD_ENC_BS_START_ADDR, encHeaderParam->buf);
    VpuWriteReg(coreIdx, W5_CMD_ENC_BS_SIZE, encHeaderParam->size);

    pEncInfo->streamRdPtr = encHeaderParam->buf;
    pEncInfo->streamWrPtr = encHeaderParam->buf;
    pEncInfo->streamBufStartAddr = encHeaderParam->buf;
    pEncInfo->streamBufSize = encHeaderParam->size;
    pEncInfo->streamBufEndAddr = encHeaderParam->buf + encHeaderParam->size;

    VpuWriteReg(coreIdx, W5_BS_OPTION, (pEncInfo->lineBufIntEn<<6) | bsEndian);

    /* Secondary AXI */
    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_USE_SEC_AXI,  (pEncInfo->secAxiInfo.useEncImdEnable<<9)    |
                (pEncInfo->secAxiInfo.useEncRdoEnable<<11)   |
                (pEncInfo->secAxiInfo.useEncLfEnable<<15));

    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_CODE_OPTION, encHeaderParam->headerType);
    VpuWriteReg(coreIdx, W5_CMD_ENC_PIC_SRC_PIC_IDX, 0);

    Wave5BitIssueCommand(handle, W5_ENC_PIC);

    int ret = enc_wait_vpu_busy(handle->coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {   // Check QUEUE_DONE
        VLOG(ERR, "timeout\n");
        if (handle->loggingEnable)
            bm_vdi_log(handle->coreIdx, W5_ENC_PIC, 2);
        LeaveLock(coreIdx);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    regVal = VpuReadReg(handle->coreIdx, W5_RET_QUEUE_STATUS);

    pEncInfo->instanceQueueCount = (regVal>>16)&0xff;
    pEncInfo->totalQueueCount    = (regVal & 0xffff);

    if (VpuReadReg(handle->coreIdx, W5_RET_SUCCESS) == FALSE) {           // FAILED for adding a command into VCPU QUEUE
        regVal = VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON);
        LeaveLock(coreIdx);

        if ( regVal == WAVE5_QUEUEING_FAIL)
            return VPU_RET_QUEUEING_FAILURE;
        else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
            return VPU_RET_MEMORY_ACCESS_VIOLATION;
        else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        else
            return VPU_RET_FAILURE;
    }

    LeaveLock(coreIdx);
    return VPU_RET_SUCCESS;
}

static int Wave5VpuEncFiniSeq(EncHandle handle)
{
    int32_t  coreIdx = handle->coreIdx;
    uint32_t regVal;
    int ret;

    int vpu_inst_flag = 0;
    int vpu_inst_flag_ori = 0;
    int enc_core_idx = 0;
    int inst_idx = handle->instIndex;
#if defined(BM_PCIE_MODE)
    enc_core_idx = handle->coreIdx/MAX_NUM_VPU_CORE_CHIP;
#else
    enc_core_idx = 0;
#endif

    vpu_inst_flag = ~(1 << inst_idx);
#ifdef __linux__
    vpu_inst_flag_ori = __atomic_load_n((int*)(p_vpu_enc_create_inst_flag[enc_core_idx]), __ATOMIC_SEQ_CST);
    __atomic_store_n((int*)p_vpu_enc_create_inst_flag[enc_core_idx], vpu_inst_flag & vpu_inst_flag_ori, __ATOMIC_SEQ_CST);
#elif _WIN32
    InterlockedExchange(&vpu_inst_flag_ori, *(p_vpu_enc_create_inst_flag[enc_core_idx]));
    InterlockedExchange((unsigned int*)p_vpu_enc_create_inst_flag[enc_core_idx], vpu_inst_flag & vpu_inst_flag_ori);
#endif

    Wave5BitIssueCommand(handle, W5_DESTROY_INSTANCE);

    ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "timeout\n");
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    regVal = VpuReadReg(coreIdx, W5_RET_SUCCESS);
    if (regVal == FALSE) {
        regVal = VpuReadReg(coreIdx, W5_RET_FAIL_REASON);
        if (regVal == WAVE5_VPU_STILL_RUNNING) {
            VLOG(ERR, "Instance %d is still running. \n", handle->instIndex);
            return VPU_RET_VPU_STILL_RUNNING;
        } else {
            VLOG(ERR, "Failed to destroy instance: %d\n", handle->instIndex);
            return VPU_RET_FAILURE;
        }
    }

    return VPU_RET_SUCCESS;
}

static int Wave5VpuEncParaChange(EncHandle handle, EncChangeParam* param)
{
    int32_t  coreIdx  = handle->coreIdx;
    EncInfo* pEncInfo = handle->encInfo;
    uint32_t regVal = 0;

    EnterLock(coreIdx);

    /* SET_PARAM + COMMON */
    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_SET_PARAM_OPTION, OPT_CHANGE_PARAM);
    VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_SET_PARAM_ENABLE, param->enable_option);

    if (param->enable_option & ENC_SET_CHANGE_PARAM_PPS) {
        regVal = ((param->constIntraPredFlag<<1)         |
                  (param->lfCrossSliceBoundaryEnable<<2) |
                  ((param->weightPredEnable&1)<<3)       |
                  (param->disableDeblk<<5)               |
                  ((param->betaOffsetDiv2&0xF)<<6)       |
                  ((param->tcOffsetDiv2&0xF)<<10)        |
                  ((param->chromaCbQpOffset&0x1F)<<14)   |
                  ((param->chromaCrQpOffset&0x1F)<<19)   |
                  (param->transform8x8Enable<<29)        |
                  (param->entropyCodingMode<<30));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_PPS_PARAM,  regVal);
    }

    if (param->enable_option & ENC_SET_CHANGE_PARAM_INTRA_PARAM) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INTRA_PARAM, (param->intraQP<<3) | (param->intraPeriod<<16));
    }

    if (param->enable_option & ENC_SET_CHANGE_PARAM_RC_TARGET_RATE) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_TARGET_RATE, param->bitRate);
    }

    if (param->enable_option & ENC_SET_CHANGE_PARAM_RC) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_PARAM, ((param->hvsQPEnable<<2) |
                                                       (param->hvsQpScale<<4)  |
                                                       (param->vbvBufferSize<<20)));
    }

    if (param->enable_option & ENC_SET_CHANGE_PARAM_RC_MIN_MAX_QP) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_MIN_MAX_QP, ((param->minQpI<<0)   |
                                                            (param->maxQpI<<6)   |
                                                            (param->maxDeltaQp<<12)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_INTER_MIN_MAX_QP, ((param->minQpP<<0) |
                                                                  (param->maxQpP<<6) |
                                                                  (param->minQpB<<12) |
                                                                  (param->maxQpB<<18)));
    }

    if (param->enable_option & ENC_SET_CHANGE_PARAM_RC_BIT_RATIO_LAYER) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_BIT_RATIO_LAYER_0_3, ((param->fixedBitRatio[0]<<0)  |
                                                                     (param->fixedBitRatio[1]<<8)  |
                                                                     (param->fixedBitRatio[2]<<16) |
                                                                     (param->fixedBitRatio[3]<<24)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RC_BIT_RATIO_LAYER_4_7, ((param->fixedBitRatio[4]<<0)  |
                                                                     (param->fixedBitRatio[5]<<8)  |
                                                                     (param->fixedBitRatio[6]<<16) |
                                                                     (param->fixedBitRatio[7]<<24)));
    }

    if (param->enable_option & ENC_SET_CHANGE_PARAM_RDO) {
        regVal  = ((param->rdoSkip<<2)               |
                   (param->lambdaScalingEnable<<3)   |
                   (param->coefClearDisable<<4)      |
                   (param->intraNxNEnable<<8)        |
                   (param->maxNumMerge<<18)          |
                   (param->customMDEnable<<20)       |
                   (param->customLambdaEnable<<21));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_RDO_PARAM, regVal);

    }

    if (param->enable_option & ENC_SET_CHANGE_PARAM_INDEPEND_SLICE) {
        if (handle->codecMode == W_HEVC_ENC ) {
            VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INDEPENDENT_SLICE,
                        param->independSliceModeArg<<16 | param->independSliceMode);
        } else if (handle->codecMode == W_AVC_ENC) {
            VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_INDEPENDENT_SLICE,
                        param->avcSliceArg<<16 | param->avcSliceMode);
        }
    }

    if (handle->codecMode == W_HEVC_ENC && param->enable_option & ENC_SET_CHANGE_PARAM_DEPEND_SLICE) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_DEPENDENT_SLICE, param->dependSliceModeArg<<16 | param->dependSliceMode);
    }



    if (handle->codecMode == W_HEVC_ENC && param->enable_option & ENC_SET_CHANGE_PARAM_NR) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_NR_PARAM, (((param->nrYEnable<<0)       |
                                                        (param->nrCbEnable<<1)      |
                                                        (param->nrCrEnable<<2)      |
                                                        (param->nrNoiseEstEnable<<3)|
                                                        (param->nrNoiseSigmaY<<4)   |
                                                        (param->nrNoiseSigmaCb<<12) |
                                                        (param->nrNoiseSigmaCr<<20))));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_NR_WEIGHT, ((param->nrIntraWeightY<<0)  |
                                                        (param->nrIntraWeightCb<<5) |
                                                        (param->nrIntraWeightCr<<10)|
                                                        (param->nrInterWeightY<<15) |
                                                        (param->nrInterWeightCb<<20)|
                                                        (param->nrInterWeightCr<<25)));
    }


    if (param->enable_option & ENC_SET_CHANGE_PARAM_BG) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_BG_PARAM, ((param->bgThrDiff<<1)         |
                                                       (param->bgThrMeanDiff<<10)    |
                                                       (param->bgLambdaQp<<18)       |
                                                       ((param->bgDeltaQp&0x1F)<<24) |
                                                       (handle->codecMode == W_AVC_ENC ? param->s2fmeDisable<<29 : 0)));
    }
    if (handle->codecMode == W_HEVC_ENC && param->enable_option & ENC_SET_CHANGE_PARAM_CUSTOM_MD) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU04, ((param->pu04DeltaRate&0xFF)                 |
                                                             ((param->pu04IntraPlanarDeltaRate&0xFF)<<8) |
                                                             ((param->pu04IntraDcDeltaRate&0xFF)<<16)    |
                                                             ((param->pu04IntraAngleDeltaRate&0xFF)<<24)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU08, ((param->pu08DeltaRate&0xFF)                 |
                                                             ((param->pu08IntraPlanarDeltaRate&0xFF)<<8) |
                                                             ((param->pu08IntraDcDeltaRate&0xFF)<<16)    |
                                                             ((param->pu08IntraAngleDeltaRate&0xFF)<<24)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU16, ((param->pu16DeltaRate&0xFF)                 |
                                                             ((param->pu16IntraPlanarDeltaRate&0xFF)<<8) |
                                                             ((param->pu16IntraDcDeltaRate&0xFF)<<16)    |
                                                             ((param->pu16IntraAngleDeltaRate&0xFF)<<24)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_PU32, ((param->pu32DeltaRate&0xFF)                 |
                                                             ((param->pu32IntraPlanarDeltaRate&0xFF)<<8) |
                                                             ((param->pu32IntraDcDeltaRate&0xFF)<<16)    |
                                                             ((param->pu32IntraAngleDeltaRate&0xFF)<<24)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_CU08, ((param->cu08IntraDeltaRate&0xFF)        |
                                                             ((param->cu08InterDeltaRate&0xFF)<<8)   |
                                                             ((param->cu08MergeDeltaRate&0xFF)<<16)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_CU16, ((param->cu16IntraDeltaRate&0xFF)        |
                                                             ((param->cu16InterDeltaRate&0xFF)<<8)   |
                                                             ((param->cu16MergeDeltaRate&0xFF)<<16)));

        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_MD_CU32, ((param->cu32IntraDeltaRate&0xFF)        |
                                                             ((param->cu32InterDeltaRate&0xFF)<<8)   |
                                                             ((param->cu32MergeDeltaRate&0xFF)<<16)));
    }

    if (handle->codecMode == W_HEVC_ENC && param->enable_option & ENC_SET_CHANGE_PARAM_CUSTOM_LAMBDA) {
        VpuWriteReg(coreIdx, W5_CMD_ENC_SEQ_CUSTOM_LAMBDA_ADDR, param->customLambdaAddr);
    }

    Wave5BitIssueCommand(handle, W5_ENC_SET_PARAM);

    int ret = enc_wait_vpu_busy(coreIdx, __VPU_BUSY_TIMEOUT, W5_VPU_BUSY_STATUS);
    if (ret == -1) {
        VLOG(ERR, "timeout\n");
        if (handle->loggingEnable)
            bm_vdi_log(coreIdx, W5_ENC_SET_PARAM, 2);
        return VPU_RET_VPU_RESPONSE_TIMEOUT;
    }

    regVal = VpuReadReg(coreIdx, W5_RET_QUEUE_STATUS);

    pEncInfo->instanceQueueCount = (regVal>>16) & 0xFF;
    pEncInfo->totalQueueCount    = (regVal & 0xFFFF);

    if (VpuReadReg(coreIdx, W5_RET_SUCCESS) == 0) {
        regVal = VpuReadReg(handle->coreIdx, W5_RET_FAIL_REASON);
        LeaveLock(coreIdx);

        if ( regVal == WAVE5_QUEUEING_FAIL)
            return VPU_RET_QUEUEING_FAILURE;
        else if (regVal == WAVE5_SYSERR_ACCESS_VIOLATION_HW)
            return VPU_RET_MEMORY_ACCESS_VIOLATION;
        else if (regVal == WAVE5_SYSERR_WATCHDOG_TIMEOUT)
            return VPU_RET_VPU_RESPONSE_TIMEOUT;
        else
            return VPU_RET_FAILURE;
    }

    LeaveLock(coreIdx);
    return VPU_RET_SUCCESS;
}

static int CheckEncCommonParamValid(VpuEncOpenParam* pop)
{
    int ret = VPU_RET_SUCCESS;
    int32_t low_delay = 0;
    int32_t intra_period_gop_step_size;
    int32_t i;

    VpuEncWaveParam* param     = &pop->waveParam;

    // check low-delay gop structure
    if(param->gopPresetIdx == VPU_GOP_PRESET_IDX_CUSTOM_GOP) { // common gop
        int32_t minVal = 0;
        if(param->gopParam.customGopSize > 1) {
            minVal = param->gopParam.picParam[0].pocOffset;
            low_delay = 1;
            for(i = 1; i < param->gopParam.customGopSize; i++) {
                if(minVal > param->gopParam.picParam[i].pocOffset) {
                    low_delay = 0;
                    break;
                } else {
                    minVal = param->gopParam.picParam[i].pocOffset;
                }
            }
        }
    } else if(param->gopPresetIdx == VPU_GOP_PRESET_IDX_ALL_I ||
              param->gopPresetIdx == VPU_GOP_PRESET_IDX_IPP   ||
              param->gopPresetIdx == VPU_GOP_PRESET_IDX_IBBB  ||
              param->gopPresetIdx == VPU_GOP_PRESET_IDX_IPPPP ||
              param->gopPresetIdx == VPU_GOP_PRESET_IDX_IBBBB) // low-delay case (IPPP, IBBB)
        low_delay = 1;

    if(low_delay) {
        intra_period_gop_step_size = 1;
    } else {
        if (param->gopPresetIdx == VPU_GOP_PRESET_IDX_CUSTOM_GOP) {
            intra_period_gop_step_size = param->gopParam.customGopSize;
        } else {
            intra_period_gop_step_size = presetGopSize[param->gopPresetIdx];
        }
    }
    if (pop->bitstreamFormat == VPU_CODEC_HEVC) {
        if( !low_delay && (param->intraPeriod != 0) && (param->decodingRefreshType != 0) && (intra_period_gop_step_size != 0) &&
            (param->intraPeriod % intra_period_gop_step_size) != 0) {
            VLOG(ERR,"CFG FAIL : Not support intra period[%d] for the gop structure\n", param->intraPeriod);
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Intra period = %d\n",
                 intra_period_gop_step_size * (param->intraPeriod / intra_period_gop_step_size));
            ret = VPU_RET_FAILURE;
        }
    }

    if( !low_delay && (param->intraPeriod != 0) && (intra_period_gop_step_size != 0) &&
        ((param->intraPeriod % intra_period_gop_step_size) == 1) &&
        param->decodingRefreshType == 0) {
        VLOG(ERR,"CFG FAIL : Not support decoding refresh type[%d] for closed gop structure\n", param->decodingRefreshType );
        VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Decoding refresh type = IDR\n");
        ret = VPU_RET_FAILURE;
    }

    if (param->gopPresetIdx == VPU_GOP_PRESET_IDX_CUSTOM_GOP) {
        for(i = 0; i < param->gopParam.customGopSize; i++) {
            if (param->gopParam.picParam[i].temporalId >= MAX_NUM_TEMPORAL_LAYER ) {
                VLOG(ERR,"CFG FAIL : temporalId %d exceeds MAX_NUM_TEMPORAL_LAYER\n", param->gopParam.picParam[i].temporalId);
                VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Adjust temporal ID under MAX_NUM_TEMPORAL_LAYER(7) in GOP structure\n");
                ret = VPU_RET_FAILURE;
            }

            if (param->gopParam.picParam[i].temporalId < 0) {
                VLOG(ERR,"CFG FAIL : Must be %d-th temporal_id >= 0\n",param->gopParam.picParam[i].temporalId);
                VLOG(ERR,"RECOMMEND CONFIG PARAMETER : Adjust temporal layer above '0' in GOP structure\n");
                ret = VPU_RET_FAILURE;
            }
        }
    }

    if (param->useRecommendEncParam == 0) {
        // RDO
        int align_32_width_flag  = pop->picWidth % 32;
        int align_16_width_flag  = pop->picWidth % 16;
        int align_8_width_flag   = pop->picWidth % 8;
        int align_32_height_flag = pop->picHeight % 32;
        int align_16_height_flag = pop->picHeight % 16;
        int align_8_height_flag  = pop->picHeight % 8;

        if (((param->cuSizeMode&0x1) == 0) && ((align_8_width_flag != 0) || (align_8_height_flag != 0))) {
            VLOG(ERR,"CFG FAIL : Picture width and height must be aligned with 8 pixels when enable CU8x8 of cuSizeMode \n");
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : cuSizeMode |= 0x1 (CU8x8)\n");
            ret = VPU_RET_FAILURE;
        } else if (((param->cuSizeMode&0x1) == 0) && ((param->cuSizeMode&0x2) == 0) && ((align_16_width_flag != 0) || (align_16_height_flag != 0))) {
            VLOG(ERR,"CFG FAIL : Picture width and height must be aligned with 16 pixels when enable CU16x16 of cuSizeMode\n");
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : cuSizeMode |= 0x2 (CU16x16)\n");
            ret = VPU_RET_FAILURE;
        } else if(((param->cuSizeMode&0x1) == 0) &&
                  ((param->cuSizeMode&0x2) == 0) &&
                  ((param->cuSizeMode&0x4) == 0) &&
                  ((align_32_width_flag != 0) || (align_32_height_flag != 0)) ) {
            VLOG(ERR,"CFG FAIL : Picture width and height must be aligned with 32 pixels when enable CU32x32 of cuSizeMode\n");
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : cuSizeMode |= 0x4 (CU32x32)\n");
            ret = VPU_RET_FAILURE;
        }
    }

    // Slice
    {
        // multi-slice & wpp
        if(param->wppEnable == 1 && (param->independSliceMode != 0 || param->dependSliceMode != 0)) {
            VLOG(ERR,"CFG FAIL : If WaveFrontSynchro(WPP) '1', the option of multi-slice must be '0'\n");
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : independSliceMode = 0, dependSliceMode = 0\n");
            ret = VPU_RET_FAILURE;
        }

        if(param->independSliceMode == 0 && param->dependSliceMode != 0) {
            VLOG(ERR,"CFG FAIL : If independSliceMode is '0', dependSliceMode must be '0'\n");
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : independSliceMode = 1, independSliceModeArg = TotalCtuNum\n");
            ret = VPU_RET_FAILURE;
        } else if((param->independSliceMode == 1) && (param->dependSliceMode == 1) ) {
            if(param->independSliceModeArg < param->dependSliceModeArg) {
                VLOG(ERR,"CFG FAIL : If independSliceMode & dependSliceMode is both '1' (multi-slice with ctu count), must be independSliceModeArg >= dependSliceModeArg\n");
                VLOG(ERR,"RECOMMEND CONFIG PARAMETER : dependSliceMode = 0\n");
                ret = VPU_RET_FAILURE;
            }
        }

        if (param->independSliceMode != 0) {
            if (param->independSliceModeArg > 65535) {
                VLOG(ERR,"CFG FAIL : If independSliceMode is not 0, must be independSliceModeArg <= 0xFFFF\n");
                ret = VPU_RET_FAILURE;
            }
        }

        if (param->dependSliceMode != 0) {
            if (param->dependSliceModeArg > 65535) {
                VLOG(ERR,"CFG FAIL : If dependSliceMode is not 0, must be dependSliceModeArg <= 0xFFFF\n");
                ret = VPU_RET_FAILURE;
            }
        }
    }

    if (param->confWinTop % 2) {
        VLOG(ERR, "CFG FAIL : conf_win_top : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinTop);
        ret = VPU_RET_FAILURE;
    }

    if (param->confWinBot % 2) {
        VLOG(ERR, "CFG FAIL : conf_win_bot : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinBot);
        ret = VPU_RET_FAILURE;
    }

    if (param->confWinLeft % 2) {
        VLOG(ERR, "CFG FAIL : conf_win_left : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinLeft);
        ret = VPU_RET_FAILURE;
    }

    if (param->confWinRight % 2) {
        VLOG(ERR, "CFG FAIL : conf_win_right : %d value is not available. The value should be equal to multiple of 2.\n", param->confWinRight);
        ret = VPU_RET_FAILURE;
    }

    // RDO
    if (param->cuSizeMode == 0) {
        VLOG(ERR, "CFG FAIL :  EnCu8x8, EnCu16x16, and EnCu32x32 must be equal to 1 respectively.\n");
        ret = VPU_RET_FAILURE;
    }

    if (param->losslessEnable && (param->nrYEnable || param->nrCbEnable || param->nrCrEnable)) {
        VLOG(ERR, "CFG FAIL : LosslessCoding and NoiseReduction (EnNrY, EnNrCb, and EnNrCr) cannot be used simultaneously.\n");
        ret = VPU_RET_FAILURE;
    }

    if (param->losslessEnable && param->bgDetectEnable) {
        VLOG(ERR, "CFG FAIL : LosslessCoding and BgDetect cannot be used simultaneously.\n");
        ret = VPU_RET_FAILURE;
    }

    if (param->losslessEnable && pop->rcEnable) {
        VLOG(ERR, "CFG FAIL : osslessCoding and RateControl cannot be used simultaneously.\n");
        ret = VPU_RET_FAILURE;
    }

    if (param->losslessEnable && param->roiEnable) {
        VLOG(ERR, "CFG FAIL : LosslessCoding and Roi cannot be used simultaneously.\n");
        ret = VPU_RET_FAILURE;
    }

    if (param->losslessEnable && !param->skipIntraTrans) {
        VLOG(ERR, "CFG FAIL : IntraTransSkip must be enabled when LosslessCoding is enabled.\n");
        ret = VPU_RET_FAILURE;
    }

    // Intra refresh
    {
        int32_t num_ctu_row = (pop->picHeight + 64 - 1) / 64;
        int32_t num_ctu_col = (pop->picWidth + 64 - 1) / 64;

        if(param->intraRefreshMode && param->intraRefreshArg <= 0) {
            VLOG(ERR, "CFG FAIL : IntraCtuRefreshArg must be greater then 0 when IntraCtuRefreshMode is enabled.\n");
            ret = VPU_RET_FAILURE;
        }
        if(param->intraRefreshMode == 1 && param->intraRefreshArg > num_ctu_row) {
            VLOG(ERR, "CFG FAIL : IntraCtuRefreshArg must be less then the number of CTUs in a row when IntraCtuRefreshMode is equal to 1.\n");
            ret = VPU_RET_FAILURE;
        }
        if(param->intraRefreshMode == 2 && param->intraRefreshArg > num_ctu_col) {
            VLOG(ERR, "CFG FAIL : IntraCtuRefreshArg must be less then the number of CTUs in a column when IntraCtuRefreshMode is equal to 2.\n");
            ret = VPU_RET_FAILURE;
        }
        if(param->intraRefreshMode == 3 && param->intraRefreshArg > num_ctu_row*num_ctu_col) {
            VLOG(ERR, "CFG FAIL : IntraCtuRefreshArg must be less then the number of CTUs in a picture when IntraCtuRefreshMode is equal to 3.\n");
            ret = VPU_RET_FAILURE;
        }
        if(param->intraRefreshMode == 4 && param->intraRefreshArg > num_ctu_row*num_ctu_col) {
            VLOG(ERR, "CFG FAIL : IntraCtuRefreshArg must be less then the number of CTUs in a picture when IntraCtuRefreshMode is equal to 4.\n");
            ret = VPU_RET_FAILURE;
        }
        if(param->intraRefreshMode == 4 && param->losslessEnable) {
            VLOG(ERR, "CFG FAIL : LosslessCoding and IntraCtuRefreshMode (4) cannot be used simultaneously.\n");
            ret = VPU_RET_FAILURE;
        }
        if(param->intraRefreshMode == 4 && param->roiEnable) {
            VLOG(ERR, "CFG FAIL : Roi and IntraCtuRefreshMode (4) cannot be used simultaneously.\n");
            ret = VPU_RET_FAILURE;
        }
    }
    return ret;
}

static int CheckEncRcParamValid(VpuEncOpenParam* pop)
{
    VpuEncWaveParam* param = &pop->waveParam;
    int ret = VPU_RET_SUCCESS;

    if (pop->rcEnable == 1) {
        int fps_n =  pop->frameRateInfo&0xffff;
        int fps_d = (pop->frameRateInfo>>16) + 1;

        if ((param->minQpI > param->maxQpI) || (param->minQpP > param->maxQpP) || (param->minQpB > param->maxQpB)) {
            VLOG(ERR,"CFG FAIL : Not allowed MinQP > MaxQP\n");
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : MinQP = MaxQP\n");
            ret = VPU_RET_FAILURE;
        }

        if (pop->bitRate*fps_d <= fps_n) {
            VLOG(ERR,"CFG FAIL : Not allowed EncBitRate <= FrameRate\n");
            VLOG(ERR,"RECOMMEND CONFIG PARAMETER : EncBitRate = FrameRate * 10000\n");
            ret = VPU_RET_FAILURE;
        }
    }

    return ret;
}

static int CheckEncCustomGopParamValid(VpuEncOpenParam* pop)
{
    VpuCustomGopParam* gopParam = &(pop->waveParam.gopParam);
    int32_t gop_size = gopParam->customGopSize;
    VpuCustomGopPicParam* gopPicParam;
    VpuCustomGopPicParam new_gop[MAX_GOP_NUM*2 +1];
    int32_t curr_poc, i, ei = 0, gi;
    int32_t enc_tid[MAX_GOP_NUM*2 +1];
    int ret = VPU_RET_SUCCESS;

    new_gop[0].pocOffset    = 0;
    new_gop[0].temporalId   = 0;
    new_gop[0].picType      = VPU_PIC_TYPE_I;
    new_gop[0].numRefPicL0  = 0;
    enc_tid[0]              = 0;

    for(i = 0; i < gop_size * 2; i++) {
        ei = i % gop_size;
        gi = i / gop_size;
        gopPicParam = &gopParam->picParam[ei];

        curr_poc                    = gi * gop_size + gopPicParam->pocOffset;
        new_gop[i + 1].pocOffset    = curr_poc;
        new_gop[i + 1].temporalId   = gopPicParam->temporalId;
        new_gop[i + 1].picType      = gopPicParam->picType;
        new_gop[i + 1].refPocL0     = gopPicParam->refPocL0 + gi * gop_size;
        new_gop[i + 1].refPocL1     = gopPicParam->refPocL1 + gi * gop_size;
        new_gop[i + 1].numRefPicL0  = gopPicParam->numRefPicL0;
        enc_tid[i + 1] = -1;
    }

    for(i = 0; i < gop_size; i++) {
        gopPicParam = &gopParam->picParam[ei];

        if(gopPicParam->pocOffset <= 0) {
            VLOG(ERR, "CFG FAIL : the POC of the %d-th picture must be greater then -1\n", i+1);
            ret = VPU_RET_FAILURE;
        }
        if(gopPicParam->pocOffset > gop_size) {
            VLOG(ERR, "CFG FAIL : the POC of the %d-th picture must be less then GopSize + 1\n", i+1);
            ret = VPU_RET_FAILURE;
        }
        if(gopPicParam->temporalId < 0) {
            VLOG(ERR, "CFG FAIL : the temporal_id of the %d-th picture must be greater than -1\n", i+1);
            ret = VPU_RET_FAILURE;
        }
    }

    for(ei = 1; ei < gop_size * 2 + 1; ei++) {
        VpuCustomGopPicParam* cur_pic = &new_gop[ei];
        if(ei <= gop_size) {
            enc_tid[cur_pic->pocOffset] = cur_pic->temporalId;
            continue;
        }

        if(new_gop[ei].picType != VPU_PIC_TYPE_I) {
            int32_t ref_poc = cur_pic->refPocL0;
            if(enc_tid[ref_poc] < 0) { // reference picture is not encoded yet
                VLOG(ERR, "CFG FAIL : the 1st reference picture cannot be used as the reference of the picture (POC %d) in encoding order\n",
                     cur_pic->pocOffset - gop_size);
                ret = VPU_RET_FAILURE;
            }
            if(enc_tid[ref_poc] > cur_pic->temporalId) {
                VLOG(ERR, "CFG FAIL : the temporal_id of the picture (POC %d) is wrong\n", cur_pic->pocOffset - gop_size);
                ret = VPU_RET_FAILURE;
            }
            if(ref_poc >= cur_pic->pocOffset) {
                VLOG(ERR, "CFG FAIL : the POC of the 1st reference picture of %d-th picture is wrong\n", cur_pic->pocOffset - gop_size);
                ret = VPU_RET_FAILURE;
            }
        }

        if (new_gop[ei].picType != VPU_PIC_TYPE_P) {
            int32_t ref_poc = cur_pic->refPocL1;
            if(enc_tid[ref_poc] < 0) { // reference picture is not encoded yet
                VLOG(ERR, "CFG FAIL : the 2nd reference picture cannot be used as the reference of the picture (POC %d) in encoding order\n",
                     cur_pic->pocOffset - gop_size);
                ret = VPU_RET_FAILURE;
            }
            if(enc_tid[ref_poc] > cur_pic->temporalId) {
                VLOG(ERR, "CFG FAIL : the temporal_id of %d-th picture is wrong\n", cur_pic->pocOffset - gop_size);
                ret = VPU_RET_FAILURE;
            }
            if(new_gop[ei].picType == VPU_PIC_TYPE_P && new_gop[ei].numRefPicL0>1) {
                if(ref_poc >= cur_pic->pocOffset) {
                    VLOG(ERR, "CFG FAIL : the POC of the 2nd reference picture of %d-th picture is wrong\n", cur_pic->pocOffset - gop_size);
                    ret = VPU_RET_FAILURE;
                }
            } else { // HOST_PIC_TYPE_B
                if(ref_poc == cur_pic->pocOffset) {
                    VLOG(ERR, "CFG FAIL : the POC of the 2nd reference picture of %d-th picture is wrong\n", cur_pic->pocOffset - gop_size);
                    ret = VPU_RET_FAILURE;
                }
            }
        }
        curr_poc = cur_pic->pocOffset;
        enc_tid[curr_poc] = cur_pic->temporalId;
    }
    return ret;
}


/******************************************************************************
 *    Codec Instance Slot Management
 ******************************************************************************/
static int InitCodecInstancePool(uint32_t coreIdx)
{
    vpu_instance_pool_t *vip;
    int i;

    vip = (vpu_instance_pool_t *)bm_vdi_get_instance_pool(coreIdx);
    if (!vip)
        return VPU_RET_INSUFFICIENT_RESOURCE;

    if (vip->instance_pool_inited==0) {
        for (i = 0; i < LIMITED_INSTANCE_NUM; i++) {
            EncHandle pInst = (EncHandle)vip->codecInstPool[i];
            pInst->instIndex = i;
            pInst->inUse = 0;
        }
        vip->instance_pool_inited = 1;
    }
    return VPU_RET_SUCCESS;
}

/**
 * Obtains a instance and stores a pointer to the allocated instance in *pHandlel
 */
static int GetCodecInstance(uint32_t coreIdx, EncHandle* pHandle)
{
    EncHandle handle = NULL;
    vpu_instance_pool_t* vip;
    int i, ret;

    vip = (vpu_instance_pool_t *)bm_vdi_get_instance_pool(coreIdx);
    if (!vip) {
        VLOG(ERR, "bm_vdi_get_instance_pool failed\n");
        return VPU_RET_INSUFFICIENT_RESOURCE;
    }

    for (i = 0; i < LIMITED_INSTANCE_NUM; i++) {
        handle = (EncHandle)vip->codecInstPool[i];
        if (!handle) {
            VLOG(ERR, "codec instance %d is NULL\n", i);
            return VPU_RET_FAILURE;
        }
        if (!handle->inUse) {
            break;
        }
    }

    if (i >= LIMITED_INSTANCE_NUM) {
        *pHandle = 0;
        VLOG(ERR, "The instance pool is full: %d\n", LIMITED_INSTANCE_NUM);
        return VPU_RET_FAILURE;
    }

    handle->inUse         = 1;
    handle->coreIdx       = coreIdx;
    handle->codecMode     = -1;
    handle->codecModeAux  = -1;
    handle->loggingEnable = 0;
    handle->productId     = VpuGetProductId(coreIdx);
    handle->isDecoder     = 0;

    handle->encInfo = malloc(sizeof(EncInfo));
    if (handle->encInfo == NULL) {
        VLOG(ERR, "malloc failed\n");
        return VPU_RET_INSUFFICIENT_RESOURCE;
    }

    *pHandle = handle;

    ret = bm_vdi_open_instance(handle->coreIdx, handle->instIndex);
    if (ret < 0) {
        VLOG(ERR, "bm_vdi_open_instance failed\n");
        return VPU_RET_FAILURE;
    }

    return VPU_RET_SUCCESS;
}

static int FreeCodecInstance(EncHandle handle)
{
    int ret;

    handle->codecMode = -1;

    free(handle->encInfo);
    handle->encInfo = NULL;

    ret = bm_vdi_close_instance(handle->coreIdx, handle->instIndex);
    if (ret < 0)
    {
        handle->inUse = 0;
        VLOG(ERR, "bm_vdi_close_instance failed\n");
        return VPU_RET_FAILURE;
    }

    handle->inUse = 0;
    return VPU_RET_SUCCESS;
}

#if 0
static uint64_t GetTimestamp(EncHandle handle)
{
    EncInfo*  pEncInfo = NULL;
    uint64_t  pts;
    uint32_t  fps;

    if (handle == NULL) {
        return 0;
    }

    pEncInfo   = handle->encInfo;
    fps        = pEncInfo->openParam.frameRateInfo;
    if (fps == 0) {
        fps    = 30;        /* 30 fps */
    }

    pts        = pEncInfo->curPTS;
    pEncInfo->curPTS += 90000/fps; /* 90KHz/fps */

    return pts;
}
#endif

static int CalcCropInfo(EncHandle handle, VpuEncOpenParam* openParam, int rotMirMode)
{
    EncInfo* pEncInfo = handle->encInfo;
    VpuEncWaveParam* param = &openParam->waveParam;
    int srcWidth  = openParam->picWidth;
    int srcHeight = openParam->picHeight;
    int alignedWidth, alignedHeight;
    int pad_right, pad_bot;
    int crop_right, crop_left, crop_top, crop_bot;
    int prp_mode;

    if (handle->codecMode == W_AVC_ENC) {
        alignedWidth  = VPU_ALIGN16(srcWidth);
        alignedHeight = VPU_ALIGN16(srcHeight);
    } else {
        alignedWidth  = VPU_ALIGN32(srcWidth);
        alignedHeight = VPU_ALIGN32(srcHeight);
    }

    /* Disable crop conformance for HEVC, because bad quality when encoding 720p bbb */
    /* Disable crop conformance for AVC,
     * since 1. increased size when encoding 1080p,
     *     2. SOPHONSC5-659 */
    if (rotMirMode == 0)
        goto End;
    if (pEncInfo->rotationAngle == 180 && pEncInfo->mirrorDirection == MIRDIR_HOR_VER)
        goto End;
    if (openParam->picWidth == alignedWidth && openParam->picHeight == alignedHeight)
        goto End;

    pad_right = alignedWidth  - srcWidth;
    pad_bot   = alignedHeight - srcHeight;

    crop_left = param->confWinLeft;
    crop_top  = param->confWinTop;
    if (param->confWinRight > 0)
        crop_right = param->confWinRight + pad_right;
    else
        crop_right = pad_right;

    if (param->confWinBot > 0)
        crop_bot = param->confWinBot + pad_bot;
    else
        crop_bot = pad_bot;

    param->confWinTop   = crop_top;
    param->confWinLeft  = crop_left;
    param->confWinBot   = crop_bot;
    param->confWinRight = crop_right;


    prp_mode = rotMirMode>>1;  // remove prp_enable bit
    /* prp_mode :
     *          | hor_mir | ver_mir |   rot_angle
     *              [3]       [2]         [1:0] = {0= NONE, 1:90, 2:180, 3:270}
     */
    if (prp_mode == 1 || prp_mode ==15) {
        param->confWinTop   = crop_right;
        param->confWinLeft  = crop_top;
        param->confWinBot   = crop_left;
        param->confWinRight = crop_bot;
    } else if (prp_mode == 2 || prp_mode ==12) {
        param->confWinTop   = crop_bot;
        param->confWinLeft  = crop_right;
        param->confWinBot   = crop_top;
        param->confWinRight = crop_left;
    } else if (prp_mode == 3 || prp_mode ==13) {
        param->confWinTop   = crop_left;
        param->confWinLeft  = crop_bot;
        param->confWinBot   = crop_right;
        param->confWinRight = crop_top;
    } else if (prp_mode == 4 || prp_mode ==10) {
        param->confWinTop   = crop_bot;
        param->confWinBot   = crop_top;
    } else if (prp_mode == 8 || prp_mode ==6) {
        param->confWinLeft  = crop_right;
        param->confWinRight = crop_left;
    } else if (prp_mode == 5 || prp_mode ==11) {
        param->confWinTop   = crop_left;
        param->confWinLeft  = crop_top;
        param->confWinBot   = crop_right;
        param->confWinRight = crop_bot;
    } else if (prp_mode == 7 || prp_mode ==9) {
        param->confWinTop   = crop_right;
        param->confWinLeft  = crop_bot;
        param->confWinBot   = crop_left;
        param->confWinRight = crop_top;
    }

End:
    VLOG(DEBUG, "srcWidth =%d\n", srcWidth);
    VLOG(DEBUG, "srcHeight=%d\n", srcHeight);
    VLOG(DEBUG, "alignedWidth =%d\n", alignedWidth);
    VLOG(DEBUG, "alignedHeight=%d\n", alignedHeight);
    VLOG(DEBUG, "crop_top  =%d\n", param->confWinTop);
    VLOG(DEBUG, "crop_bot  =%d\n", param->confWinBot);
    VLOG(DEBUG, "crop_left =%d\n", param->confWinLeft);
    VLOG(DEBUG, "crop_right=%d\n", param->confWinRight);

    return VPU_RET_SUCCESS;
}

static int CheckEncInstanceValidity(EncHandle handle)
{
    vpu_instance_pool_t *vip;
    int i;

    if (handle == NULL)
        return VPU_RET_INVALID_HANDLE;

    vip = (vpu_instance_pool_t *)bm_vdi_get_instance_pool(handle->coreIdx);
    if (!vip) {
        return VPU_RET_INSUFFICIENT_RESOURCE;
    }

    for (i = 0; i < MAX_NUM_INSTANCE; i++) {
        if ((EncHandle)vip->codecInstPool[i] == handle)
            break;
    }

    if (i >= MAX_NUM_INSTANCE) {
        return VPU_RET_INVALID_HANDLE;
    }

    if (!handle->inUse) {
        return VPU_RET_INVALID_HANDLE;
    }

    if (handle->codecMode != W_HEVC_ENC &&
        handle->codecMode != W_AVC_ENC) {
        return VPU_RET_INVALID_HANDLE;
    }

    return VPU_RET_SUCCESS;
}

static int CheckEncParam(EncHandle handle, VpuEncParam * param)
{
    EncInfo* pEncInfo = handle->encInfo;

    if (param == 0) {
        return VPU_RET_INVALID_PARAM;
    }

    if (param->skipPicture != 0 && param->skipPicture != 1) {
        return VPU_RET_INVALID_PARAM;
    }
    if (param->skipPicture == 0 && param->sourceFrame == 0) {
        return VPU_RET_INVALID_FRAME_BUFFER;
    }

    if (param->picStreamBufferAddr % 8) {
        return VPU_RET_INVALID_PARAM;
    }
    if (param->picStreamBufferSize == 0) {
        return VPU_RET_INVALID_PARAM;
    }

    if (pEncInfo->openParam.bitRate == 0) { // no rate control
        if (handle->codecMode == W_HEVC_ENC) {
            if (param->forcePicQpEnable == 1) {
                if (param->forcePicQpI < 0 || param->forcePicQpI > 63)
                    return VPU_RET_INVALID_PARAM;

                if (param->forcePicQpP < 0 || param->forcePicQpP > 63)
                    return VPU_RET_INVALID_PARAM;

                if (param->forcePicQpB < 0 || param->forcePicQpB > 63)
                    return VPU_RET_INVALID_PARAM;
            }
            if (param->picStreamBufferAddr % 16)
                return VPU_RET_INVALID_PARAM;
        }
    }

    return VPU_RET_SUCCESS;
}

static int EnterLock(uint32_t coreIdx)
{
    if (bm_vdi_lock(coreIdx) != 0)
        return VPU_RET_FAILURE;

    VpuSetClockGate(coreIdx, 1);

    return VPU_RET_SUCCESS;
}
static int LeaveLock(uint32_t coreIdx)
{
    VpuSetClockGate(coreIdx, 0);
    bm_vdi_unlock(coreIdx);
    return VPU_RET_SUCCESS;
}

// TODO
int VpuSetClockGate(uint32_t coreIdx, uint32_t on)
{
    bm_vdi_set_clock_gate(coreIdx, on);

    return VPU_RET_SUCCESS;
}

static int Wave5ConfigSecAXI(uint32_t coreIdx, int32_t codecMode, SecAxiInfo *sa,
                             uint32_t width, uint32_t height,
                             uint32_t profile, uint32_t levelIdc)
{
    vpu_buffer_t vb;
    int offset;
    uint32_t lumaSize = 0;
    uint32_t chromaSize = 0;
    uint32_t productId;
    int ret;

    UNREFERENCED_PARAMETER(codecMode);
    UNREFERENCED_PARAMETER(height);

    // TODO
    ret = bm_vdi_get_sram_memory(coreIdx, &vb);
    if (ret < 0)
        return -1;

    productId = VpuGetProductId(coreIdx);

    if (!vb.size) {
        sa->bufSize           = 0;
        sa->useEncImdEnable   = 0;
        sa->useEncLfEnable    = 0;
        sa->useEncRdoEnable   = 0;
        return -1;
    }

    sa->bufBase = vb.phys_addr;
    offset      = 0;

    if (sa->useEncImdEnable == TRUE) {
        /* Main   profile(8bit) : Align32(picWidth)
         * Main10 profile(10bit): Align32(picWidth) */
        sa->bufImd = sa->bufBase + offset;
        offset    += VPU_ALIGN32(width);
        if (offset > vb.size) {
            sa->bufSize = 0;
            return -1;
        }
    }

    if (sa->useEncLfEnable == TRUE) {
        uint32_t luma, chroma;

        sa->bufLf = sa->bufBase + offset;

        if (codecMode == W_AVC_ENC) {
            luma   = (profile == HEVC_PROFILE_MAIN10 ? 5 : 4);
            chroma = 3;
            lumaSize   = VPU_ALIGN16(width) * luma;
            chromaSize = VPU_ALIGN16(width) * chroma;
        } else {
            luma   = (profile == HEVC_PROFILE_MAIN10 ? 7 : 5);
            if (productId == PRODUCT_ID_521)
                chroma = (profile == HEVC_PROFILE_MAIN10 ? 4 : 3);
            else
                chroma = (profile == HEVC_PROFILE_MAIN10 ? 5 : 3);

            lumaSize   = VPU_ALIGN64(width) * luma;
            chromaSize = VPU_ALIGN64(width) * chroma;
        }

        offset    += lumaSize + chromaSize;

        if (offset > vb.size) {
            sa->bufSize = 0;
            return -1;
        }
    }

    if (sa->useEncRdoEnable == TRUE) {
        switch (productId) {
        case PRODUCT_ID_521:
            sa->bufRdo = sa->bufBase + offset;
            if (codecMode == W_AVC_ENC)
                offset += (VPU_ALIGN64(width)>>6)*384;
            else // HEVC ENC
                offset += (VPU_ALIGN64(width)>>6)*288;
            break;
        default:
            /* Main   profile(8bit) : (Align64(picWidth)/64) * 336
             * Main10 profile(10bit): (Align64(picWidth)/64) * 336
             */
            sa->bufRdo = sa->bufBase + offset;
            offset    += (VPU_ALIGN64(width)/64) * 336;
            break;
        }

        if (offset > vb.size) {
            sa->bufSize = 0;
            return -1;
        }
    }

    sa->bufSize = offset;

    return 0;
}

static int32_t CalcStride(uint32_t width, uint32_t height,
                          FrameBufferFormat format,
                          BOOL cbcrInterleave,
                          TiledMapType mapType)
{
    uint32_t  lumaStride   = 0;
    uint32_t  chromaStride = 0;

    lumaStride = VPU_ALIGN32(width);

    if (mapType == LINEAR_FRAME_MAP) {
        uint32_t twice = 0;

        twice = cbcrInterleave == TRUE ? 2 : 1;
        switch (format) {
        case FORMAT_420:
            /* nothing to do */
            break;
        case FORMAT_420_P10_16BIT_LSB:
        case FORMAT_420_P10_16BIT_MSB:
        case FORMAT_422_P10_16BIT_MSB:
        case FORMAT_422_P10_16BIT_LSB:
            lumaStride = VPU_ALIGN32(width)*2;
            break;
        case FORMAT_420_P10_32BIT_LSB:
        case FORMAT_420_P10_32BIT_MSB:
        case FORMAT_422_P10_32BIT_MSB:
        case FORMAT_422_P10_32BIT_LSB:
            width = VPU_ALIGN32(width);
            lumaStride   = ((VPU_ALIGN16(width)+11)/12)*16;
            chromaStride = ((VPU_ALIGN16(width/2)+11)*twice/12)*16;
            if ( (chromaStride*2) > lumaStride)
            {
                lumaStride = chromaStride * 2;
                VLOG(ERR, "double chromaStride size is bigger than lumaStride\n");
            }
            if (cbcrInterleave == TRUE) {
                lumaStride = MAX(lumaStride, chromaStride);
            }
            break;
        case FORMAT_422:
            /* nothing to do */
            break;
        case FORMAT_YUYV:       // 4:2:2 8bit packed
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            lumaStride = VPU_ALIGN32(width) * 2;
            break;
        case FORMAT_YUYV_P10_16BIT_MSB:   // 4:2:2 10bit packed
        case FORMAT_YUYV_P10_16BIT_LSB:
        case FORMAT_YVYU_P10_16BIT_MSB:
        case FORMAT_YVYU_P10_16BIT_LSB:
        case FORMAT_UYVY_P10_16BIT_MSB:
        case FORMAT_UYVY_P10_16BIT_LSB:
        case FORMAT_VYUY_P10_16BIT_MSB:
        case FORMAT_VYUY_P10_16BIT_LSB:
            lumaStride = VPU_ALIGN32(width) * 4;
            break;
        case FORMAT_YUYV_P10_32BIT_MSB:
        case FORMAT_YUYV_P10_32BIT_LSB:
        case FORMAT_YVYU_P10_32BIT_MSB:
        case FORMAT_YVYU_P10_32BIT_LSB:
        case FORMAT_UYVY_P10_32BIT_MSB:
        case FORMAT_UYVY_P10_32BIT_LSB:
        case FORMAT_VYUY_P10_32BIT_MSB:
        case FORMAT_VYUY_P10_32BIT_LSB:
            lumaStride = VPU_ALIGN32(width*2)*2;
            break;
        default:
            break;
        }
    } else if (mapType == COMPRESSED_FRAME_MAP) {
        switch (format) {
        case FORMAT_420:
        case FORMAT_422:
        case FORMAT_YUYV:
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            break;
        case FORMAT_420_P10_16BIT_LSB:
        case FORMAT_420_P10_16BIT_MSB:
        case FORMAT_420_P10_32BIT_LSB:
        case FORMAT_420_P10_32BIT_MSB:
        case FORMAT_422_P10_16BIT_MSB:
        case FORMAT_422_P10_16BIT_LSB:
        case FORMAT_422_P10_32BIT_MSB:
        case FORMAT_422_P10_32BIT_LSB:
        case FORMAT_YUYV_P10_16BIT_MSB:
        case FORMAT_YUYV_P10_16BIT_LSB:
        case FORMAT_YVYU_P10_16BIT_MSB:
        case FORMAT_YVYU_P10_16BIT_LSB:
        case FORMAT_YVYU_P10_32BIT_MSB:
        case FORMAT_YVYU_P10_32BIT_LSB:
        case FORMAT_UYVY_P10_16BIT_MSB:
        case FORMAT_UYVY_P10_16BIT_LSB:
        case FORMAT_VYUY_P10_16BIT_MSB:
        case FORMAT_VYUY_P10_16BIT_LSB:
        case FORMAT_YUYV_P10_32BIT_MSB:
        case FORMAT_YUYV_P10_32BIT_LSB:
        case FORMAT_UYVY_P10_32BIT_MSB:
        case FORMAT_UYVY_P10_32BIT_LSB:
        case FORMAT_VYUY_P10_32BIT_MSB:
        case FORMAT_VYUY_P10_32BIT_LSB:
            lumaStride = VPU_ALIGN32(VPU_ALIGN16(width)*5)/4;
            lumaStride = VPU_ALIGN32(lumaStride);
            break;
        default:
            return -1;
        }
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_8BIT ||
               mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_10BIT ||
               mapType == COMPRESSED_FRAME_MAP_V50_LOSSY) {
        lumaStride = VPU_ALIGN32(width);
    } else {
        width = (width < height) ? height : width;

        lumaStride = ((width > 4096) ? 8192 :
                      (width > 2048) ? 4096 :
                      (width > 1024) ? 2048 :
                      (width >  512) ? 1024 : 512);
    }

    return lumaStride;
}

static int32_t CalcLumaSize(int32_t stride, int32_t height,
                            FrameBufferFormat format, BOOL cbcrIntl,
                            TiledMapType mapType, DRAMConfig *pDramCfg)
{
    int32_t unit_size_hor_lum, unit_size_ver_lum, size_dpb_lum;
    UNREFERENCED_PARAMETER(cbcrIntl);

    if (mapType == LINEAR_FRAME_MAP) {
        size_dpb_lum = stride * height;
    } else if (mapType == COMPRESSED_FRAME_MAP) {
        size_dpb_lum = stride * height;
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_10BIT || mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_10BIT) {
        size_dpb_lum = WAVE5_ENC_FBC50_LOSSLESS_LUMA_10BIT_FRAME_SIZE(stride, height);
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_8BIT || mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_8BIT) {
        size_dpb_lum = WAVE5_ENC_FBC50_LOSSLESS_LUMA_8BIT_FRAME_SIZE(stride, height);
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSY || mapType == COMPRESSED_FRAME_MAP_V50_LOSSY_422) {
        if (pDramCfg == NULL)
            return 0;
        size_dpb_lum = WAVE5_ENC_FBC50_LOSSY_LUMA_FRAME_SIZE(stride, height, pDramCfg->tx16y);
    } else {
        unit_size_hor_lum = stride;
        unit_size_ver_lum = ((height+63)/64) * 64; // unit vertical size is 64 pixel (4MB)
        size_dpb_lum      = unit_size_hor_lum * unit_size_ver_lum;
    }

    return size_dpb_lum;
}

static int32_t CalcChromaSize(int32_t stride, int32_t height,
                              FrameBufferFormat format, BOOL cbcrIntl,
                              TiledMapType mapType, DRAMConfig* pDramCfg)
{
    int32_t chr_size_y, chr_size_x;
    int32_t chr_vscale, chr_hscale;
    int32_t unit_size_hor_chr, unit_size_ver_chr;
    int32_t size_dpb_chr;

    unit_size_hor_chr = 0;
    unit_size_ver_chr = 0;

    chr_hscale = 1;
    chr_vscale = 1;

    switch (format) {
    case FORMAT_420_P10_16BIT_LSB:
    case FORMAT_420_P10_16BIT_MSB:
    case FORMAT_420_P10_32BIT_LSB:
    case FORMAT_420_P10_32BIT_MSB:
    case FORMAT_420:
        chr_hscale = 2;
        chr_vscale = 2;
        break;
    case FORMAT_224:
        chr_vscale = 2;
        break;
    case FORMAT_422:
    case FORMAT_422_P10_16BIT_LSB:
    case FORMAT_422_P10_16BIT_MSB:
    case FORMAT_422_P10_32BIT_LSB:
    case FORMAT_422_P10_32BIT_MSB:
        chr_hscale = 2;
        break;
    case FORMAT_444:
    case FORMAT_400:
    case FORMAT_YUYV:
    case FORMAT_YVYU:
    case FORMAT_UYVY:
    case FORMAT_VYUY:
    case FORMAT_YUYV_P10_16BIT_MSB:   // 4:2:2 10bit packed
    case FORMAT_YUYV_P10_16BIT_LSB:
    case FORMAT_YVYU_P10_16BIT_MSB:
    case FORMAT_YVYU_P10_16BIT_LSB:
    case FORMAT_UYVY_P10_16BIT_MSB:
    case FORMAT_UYVY_P10_16BIT_LSB:
    case FORMAT_VYUY_P10_16BIT_MSB:
    case FORMAT_VYUY_P10_16BIT_LSB:
    case FORMAT_YUYV_P10_32BIT_MSB:
    case FORMAT_YUYV_P10_32BIT_LSB:
    case FORMAT_YVYU_P10_32BIT_MSB:
    case FORMAT_YVYU_P10_32BIT_LSB:
    case FORMAT_UYVY_P10_32BIT_MSB:
    case FORMAT_UYVY_P10_32BIT_LSB:
    case FORMAT_VYUY_P10_32BIT_MSB:
    case FORMAT_VYUY_P10_32BIT_LSB:
        break;
    default:
        return 0;
    }

    if (mapType == LINEAR_FRAME_MAP) {
        switch (format) {
        case FORMAT_420:
            unit_size_hor_chr = stride/2;
            unit_size_ver_chr = height/2;
            break;
        case FORMAT_420_P10_16BIT_LSB:
        case FORMAT_420_P10_16BIT_MSB:
            unit_size_hor_chr = stride/2;
            unit_size_ver_chr = height/2;
            break;
        case FORMAT_420_P10_32BIT_LSB:
        case FORMAT_420_P10_32BIT_MSB:
            unit_size_hor_chr = VPU_ALIGN16(stride/2);
            unit_size_ver_chr = height/2;
            break;
        case FORMAT_422:
        case FORMAT_422_P10_16BIT_LSB:
        case FORMAT_422_P10_16BIT_MSB:
        case FORMAT_422_P10_32BIT_LSB:
        case FORMAT_422_P10_32BIT_MSB:
            unit_size_hor_chr = VPU_ALIGN32(stride/2);
            unit_size_ver_chr = height;
            break;
        case FORMAT_YUYV:
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
        case FORMAT_YUYV_P10_16BIT_MSB:   // 4:2:2 10bit packed
        case FORMAT_YUYV_P10_16BIT_LSB:
        case FORMAT_YVYU_P10_16BIT_MSB:
        case FORMAT_YVYU_P10_16BIT_LSB:
        case FORMAT_UYVY_P10_16BIT_MSB:
        case FORMAT_UYVY_P10_16BIT_LSB:
        case FORMAT_VYUY_P10_16BIT_MSB:
        case FORMAT_VYUY_P10_16BIT_LSB:
        case FORMAT_YUYV_P10_32BIT_MSB:
        case FORMAT_YUYV_P10_32BIT_LSB:
        case FORMAT_YVYU_P10_32BIT_MSB:
        case FORMAT_YVYU_P10_32BIT_LSB:
        case FORMAT_UYVY_P10_32BIT_MSB:
        case FORMAT_UYVY_P10_32BIT_LSB:
        case FORMAT_VYUY_P10_32BIT_MSB:
        case FORMAT_VYUY_P10_32BIT_LSB:
            unit_size_hor_chr = 0;
            unit_size_ver_chr = 0;
            break;
        default:
            break;
        }
        size_dpb_chr = (format == FORMAT_400) ? 0 : unit_size_ver_chr * unit_size_hor_chr;
    } else if (mapType == COMPRESSED_FRAME_MAP) {
        switch (format) {
        case FORMAT_420:
        case FORMAT_YUYV:       // 4:2:2 8bit packed
        case FORMAT_YVYU:
        case FORMAT_UYVY:
        case FORMAT_VYUY:
            size_dpb_chr = VPU_ALIGN16(stride/2)*height;
            break;
        default:
            /* 10bit */
            stride = VPU_ALIGN64(stride/2)+12; /* FIXME: need width information */
            size_dpb_chr = VPU_ALIGN32(stride)*VPU_ALIGN4(height);
            break;
        }
        size_dpb_chr = size_dpb_chr / 2;
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_10BIT) {
        size_dpb_chr = WAVE5_ENC_FBC50_LOSSLESS_CHROMA_10BIT_FRAME_SIZE(stride, height);
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_8BIT) {
        size_dpb_chr = WAVE5_ENC_FBC50_LOSSLESS_CHROMA_8BIT_FRAME_SIZE(stride, height);
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSY) {
        if (pDramCfg == NULL)
            return 0;
        size_dpb_chr = WAVE5_ENC_FBC50_LOSSY_CHROMA_FRAME_SIZE(stride, height, pDramCfg->tx16c);
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_10BIT) {
        size_dpb_chr = WAVE5_ENC_FBC50_LOSSLESS_422_CHROMA_10BIT_FRAME_SIZE(stride, height);
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSLESS_422_8BIT) {
        size_dpb_chr = WAVE5_ENC_FBC50_LOSSLESS_422_CHROMA_8BIT_FRAME_SIZE(stride, height);
    } else if (mapType == COMPRESSED_FRAME_MAP_V50_LOSSY_422) {
        if (pDramCfg == NULL)
            return 0;
        size_dpb_chr = WAVE5_ENC_FBC50_LOSSY_422_CHROMA_FRAME_SIZE(stride, height, pDramCfg->tx16c);
    } else {
        chr_size_y = height/chr_hscale;
        chr_size_x = cbcrIntl == TRUE ? stride : stride/chr_vscale;

        unit_size_hor_chr = ((chr_size_x> 4096) ? 8192:
                             (chr_size_x> 2048) ? 4096 :
                             (chr_size_x > 1024) ? 2048 :
                             (chr_size_x >  512) ? 1024 : 512);
        unit_size_ver_chr = ((chr_size_y+63)/64) * 64; // unit vertical size is 64 pixel (4MB)

        size_dpb_chr  = (format==FORMAT_400) ? 0 : unit_size_hor_chr * unit_size_ver_chr;
        size_dpb_chr /= (cbcrIntl == TRUE ? 2 : 1);
    }

    return size_dpb_chr;
}

static int enc_wait_bus_busy(uint32_t core_idx, int timeout, unsigned int gdi_busy_flag)
{
    uint64_t elapse, cur;
    uint32_t regVal;

    elapse = vpu_gettime();

    while(1) {
        regVal = bm_vdi_fio_read_register(core_idx, gdi_busy_flag);
        if (regVal == 0x3f)
            break;

        if (timeout > 0) {
            cur = vpu_gettime();
            if ((cur - elapse) > timeout) {
                VLOG(ERR, "[VDI] timeout, PC=0x%x\n", VpuReadReg(core_idx, 0x018));
                return -1;
            }
        }
    }
    return 0;
}
static int enc_wait_vpu_busy(uint32_t core_idx, int timeout, unsigned int addr_bit_busy_flag)
{
    uint32_t pc = W5_VCPU_CUR_PC;
    uint64_t elapse, cur;
    uint32_t regVal;
#ifdef __linux__
    elapse = vpu_gettime();
#elif _WIN32
	elapse = GetTickCount();
#endif
    while(1) {
        regVal = VpuReadReg(core_idx, addr_bit_busy_flag);
        if (regVal == 0)
            break;

        if(getenv("DECREASE_CPU_LOADING")!=NULL&&strcmp(getenv("DECREASE_CPU_LOADING"),"1")==0)
#ifdef __linux__
            usleep(10);
#elif _WIN32
            Sleep(1);
#endif


        if (timeout > 0) {
#ifdef __linux__
            cur = vpu_gettime();
#elif _WIN32
			cur = GetTickCount();
#endif

            if ((cur - elapse) > timeout) {
                uint32_t idx;
                for (idx=0; idx<5; idx++) {
                    VLOG(ERR, "[VDI] timeout, PC=0x%x\n", VpuReadReg(core_idx, pc));
                }
                return -1;
            }
        }
    }
    return 0;
}
static int enc_wait_vcpu_bus_busy(uint32_t core_idx, int timeout, unsigned int addr_bit_busy_flag)
{
    uint32_t pc = W5_VCPU_CUR_PC;
    uint64_t elapse, cur;
    uint32_t regVal;

#ifdef __linux__
    elapse = vpu_gettime();
#elif _WIN32
			elapse = GetTickCount();
#endif

    while (1) {
        regVal = VpuReadReg(core_idx, addr_bit_busy_flag);
        if (regVal == 0x40)
            break;

        if (timeout > 0) {
#ifdef __linux__
            cur = vpu_gettime();
#elif _WIN32
			cur = GetTickCount();
#endif
            if ((cur - elapse) > timeout) {
                uint32_t idx;
                for (idx=0; idx<5; idx++) {
                    VLOG(ERR, "[VDI] timeout, PC=0x%x\n", VpuReadReg(core_idx, pc));
                }
                return -1;
            }
        }
    }
    return 0;
}

