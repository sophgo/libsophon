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
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
// #include <stdatomic.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#elif _WIN32
#include <windows.h>
#include  < MMSystem.h >
#pragma comment(lib, "winmm.lib")
#endif

#include <signal.h>     /* SIG_SETMASK */

#include "bmvpu.h"

#include "bmvpuapi.h"
#include "bmvpuapi_internal.h"

#define MAX_SOC_NUM 64
typedef struct _G_bm_handle{
    bm_handle_t bm_handle;
    unsigned int count;
} G_bm_handle;
static G_bm_handle g_bm_handle[MAX_SOC_NUM] = { {0, 0} };
#define HEAP_MASK 0x06

#define VPU_ENC_BITSTREAM_BUFFER_SIZE (1024*1024*1)

/* WAVE521ENC_WORKBUF_SIZE */
#define VPU_ENC_WORK_BUFFER_SIZE      (128*1024)

typedef struct _BmVpuEncWriteContext {
    uint8_t *start;
    uint8_t *end;
    uint8_t *p;
} BmVpuEncWriteContext;

#define BM_VPU_ENC_HANDLE_ERROR(MSG_START, RET_CODE) \
    bmvpu_enc_handle_error_full(__FILE__, __LINE__, __func__, (MSG_START), (RET_CODE))

#define BM_VPU_ENC_GET_BS_VA(BMVPUENC, BITSTREAM_PHYS_ADDR) \
    (((BMVPUENC)->bs_virt_addr) + ((bm_pa_t)(BITSTREAM_PHYS_ADDR) - (bm_pa_t)((BMVPUENC)->bs_phys_addr)))

/* For BM1684 */
#define MAX_NUM_VPU_CORE_CHIP     5
#define VPU_ENC_CORE_IDX          4

enum {
    BM_VPU_ENC_REC_IDX_END           = -1,
    BM_VPU_ENC_REC_IDX_DELAY         = -2,
    BM_VPU_ENC_REC_IDX_HEADER_ONLY   = -3,
    BM_VPU_ENC_REC_IDX_CHANGE_PARAM  = -4
};
#ifdef __linux__
// static atomic_flag bmve_atomic_lock = ATOMIC_FLAG_INIT;
// static atomic_flag bmhandle_atomic_lock = ATOMIC_FLAG_INIT; /* atomic lock for bmlib_handle */
static int bmve_atomic_lock = 0;
static int bmhandle_atomic_lock = 0; /* atomic lock for bmlib_handle */
#elif _WIN32
static  volatile long bmve_atomic_lock = 0;
static  volatile long bmhandle_atomic_lock = 0;
#endif

#ifdef __linux__
static __thread sigset_t oldmask[128];
static __thread sigset_t newmask[128];
#endif

/* atomic lock for bmlib_handle operations*/
static void bm_handle_lock()
{
#ifdef __linux__
    // while (atomic_flag_test_and_set(&bmhandle_atomic_lock))
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
    // atomic_flag_clear(&bmhandle_atomic_lock);
    __atomic_clear(&bmhandle_atomic_lock, __ATOMIC_SEQ_CST);
#endif
#ifdef _WIN32
    InterlockedExchange(&bmhandle_atomic_lock, 0);
#endif
}

static void bmvpu_enc_lock(int soc_idx)
{
#if __linux__
    pthread_sigmask(SIG_BLOCK, &newmask[soc_idx], &oldmask[soc_idx]);
#elif _WIN32
	//now don`t konw pthread_sigmask INT and TERM in windows working principle
#endif
    vpu_lock(soc_idx);
}
static void bmvpu_enc_unlock(int soc_idx)
{
    vpu_unlock(soc_idx);
#if __linux__
    pthread_sigmask(SIG_SETMASK, &oldmask[soc_idx], NULL);
#elif _WIN32
	//now don`t konw pthread_sigmask INT and TERM in windows working principle
#endif
}

static int bmvpu_enc_handle_error_full(char const *fn, int linenr, char const *funcn,
                                       char const *msg_start, int ret_code);
static BmVpuFrameType convert_frame_type(int vpu_pic_type);

/*
 * If a bm_handle_t on this soc already exists, return it directly,
 * otherwise return after creat one (using bm_dev_request).
 * This function is only be called by bmvpu_enc_load(), the bm_handle_t's reference count +1
 * if this function is called.
 */
static void bmvpu_enc_load_bmlib_handle(int soc_idx);

/*
 * If a bm_handle_t on this soc already exists, then the bm_handle_t's reference count -1.
 * After that, if bm_handle_t's reference count is 0, free it(bm_dev_free),
 * otherwise do nothing.
 * This function is only be called by bmvpu_enc_unload().
 */
static void bmvpu_enc_unload_bmlib_handle(int soc_idx);

int bmvpu_enc_get_core_idx(int soc_idx)
{
    return vpu_EncGetUniCoreIdx(soc_idx);
}

static int bmvpu_enc_bs_download(BmVpuEncoder* bve, uint8_t* host_va,
                                  uint64_t vpu_pa, int size)
{
#ifdef BM_PCIE_MODE
    bm_device_mem_t vpu_mem = bm_mem_from_device(vpu_pa, size);
    int ret = bm_memcpy_d2s_partial(bve->bm_handle, (void*)host_va, vpu_mem, size);
    return ret;
#else
    uint8_t* va = BM_VPU_ENC_GET_BS_VA(bve, vpu_pa);

    bm_device_mem_t vpu_mem = bm_mem_from_device(vpu_pa, size);
    memcpy(host_va, va, size);

    return 0;
#endif
}

static int
bmvpu_enc_generate_header_data(BmVpuEncoder *encoder)
{
    VpuEncHeaderParam par;

    int buf_size = encoder->bs_dmabuffer->size;
    int ret, enc_ret;

    BM_VPU_LOG("generate header data");

    memset(&par, 0, sizeof(par));
    par.buf = encoder->bs_phys_addr;
    par.size = buf_size;

    if (encoder->codec_format == BM_VPU_CODEC_FORMAT_H265)
        par.headerType |= (1<<2); /* vps rbsp */
    par.headerType |= (1<<3); /* sps rbsp */
    par.headerType |= (1<<4); /* pps rbsp */

    enc_ret = vpu_EncPutVideoHeader(encoder->handle, &par);
    ret = BM_VPU_ENC_HANDLE_ERROR("header generation command failed", enc_ret);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
        return ret;
    BM_VPU_TRACE("headers size: %d", par.size);

    encoder->headers_rbsp = malloc(par.size);
    if (encoder->headers_rbsp == NULL)
    {
        BM_VPU_ERROR("could not allocate %d byte for memory block", par.size);
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }

    ret = bmvpu_enc_bs_download(encoder, encoder->headers_rbsp, encoder->bs_phys_addr, par.size);
    if (ret != 0)
    {
        BM_VPU_ERROR("bmvpu_enc_bs_download failed");
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }
    encoder->headers_rbsp_size = par.size;

    return BM_VPU_ENC_RETURN_CODE_OK;
}

static int
bmvpu_enc_write_header_data(BmVpuEncoder *encoder,
                            BmVpuEncodedFrame *encoded_frame,
                            BmVpuEncParams *encoding_params,
                            BmVpuEncWriteContext *write_context,
                            uint32_t *output_code)
{
    uint8_t* rbsp = encoder->headers_rbsp;
    size_t   size = encoder->headers_rbsp_size;

    memcpy(write_context->p, rbsp, size);
    write_context->p += size;

    *output_code |= BM_VPU_ENC_OUTPUT_CODE_CONTAINS_HEADER;

    return BM_VPU_ENC_RETURN_CODE_OK;
}

static void
bmvpu_enc_free_header_data(BmVpuEncoder *encoder)
{
    if (encoder->headers_rbsp)
    {
        free(encoder->headers_rbsp);
        encoder->headers_rbsp = NULL;
    }
}

char const * bmvpu_enc_error_string(BmVpuEncReturnCodes code)
{
    switch (code)
    {
    case BM_VPU_ENC_RETURN_CODE_OK:                        return "ok";
    case BM_VPU_ENC_RETURN_CODE_ERROR:                     return "unspecified error";
    case BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS:            return "invalid params";
    case BM_VPU_ENC_RETURN_CODE_INVALID_HANDLE:            return "invalid handle";
    case BM_VPU_ENC_RETURN_CODE_INVALID_FRAMEBUFFER:       return "invalid framebuffer";
    case BM_VPU_ENC_RETURN_CODE_INSUFFICIENT_FRAMEBUFFERS: return "insufficient framebuffers";
    case BM_VPU_ENC_RETURN_CODE_INVALID_STRIDE:            return "invalid stride";
    case BM_VPU_ENC_RETURN_CODE_WRONG_CALL_SEQUENCE:       return "wrong call sequence";
    case BM_VPU_ENC_RETURN_CODE_TIMEOUT:                   return "timeout";
    default: return "<unknown>";
    }
}

int bmvpu_enc_load(int soc_idx)
{
    int init_ret;
    int ret = BM_VPU_ENC_RETURN_CODE_OK;

    /* Enter critical region */
#ifdef __linux__
//    while (atomic_flag_test_and_set(&bmve_atomic_lock))
    while (__atomic_test_and_set(&bmve_atomic_lock, __ATOMIC_SEQ_CST))
    {
        sleep(1);
    }
#endif
#ifdef _WIN32
    while (InterlockedCompareExchange(&bmve_atomic_lock, 1, 0)) {
        Sleep(1000);
    }
#endif

    BM_VPU_LOG("VPU at Sophon device [%d] init", soc_idx);

    BM_VPU_INFO("libbmvpuapi version %s", BMVPUAPI_VERSION);

    init_ret = vpu_EncInit(soc_idx, NULL);
    if (init_ret != VPU_RET_SUCCESS)
    {
        BM_VPU_ERROR("loading VPU failed");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    bmvpu_enc_load_bmlib_handle(soc_idx);

#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    BM_VPU_DEBUG("loaded VPU");

cleanup:
    /* Leave critical region */
#ifdef __linux__
//    atomic_flag_clear(&bmve_atomic_lock);
    __atomic_clear(&bmve_atomic_lock, __ATOMIC_SEQ_CST);
#endif
#ifdef _WIN32
    InterlockedExchange(&bmve_atomic_lock,0);
#endif

    return ret;
}

int bmvpu_enc_unload(int soc_idx)
{
    int ret = BM_VPU_ENC_RETURN_CODE_OK;

    /* Enter critical region */
#ifdef __linux__
//    while (atomic_flag_test_and_set(&bmve_atomic_lock))
    while (__atomic_test_and_set(&bmve_atomic_lock, __ATOMIC_SEQ_CST))
    {
        sleep(1);
    }
#endif
#ifdef _WIN32

    timeEndPeriod(1);
    while (InterlockedCompareExchange(&bmve_atomic_lock, 1, 0)) {// linux initial value false
        Sleep(1000);
    }
#endif

    BM_VPU_LOG("VPU at Sophon device [%d] deinit", soc_idx);

    vpu_EncUnInit(soc_idx);
    BM_VPU_DEBUG("unloaded VPU");

    /* free bm_handle_t */
    bmvpu_enc_unload_bmlib_handle(soc_idx);

    /* Leave critical region */
#ifdef __linux__
//    atomic_flag_clear(&bmve_atomic_lock);
    __atomic_clear(&bmve_atomic_lock, __ATOMIC_SEQ_CST);
#endif
#ifdef _WIN32
    InterlockedExchange(&bmve_atomic_lock,0);
#endif

    return ret;
}

void bmvpu_enc_get_bitstream_buffer_info(size_t *size, uint32_t *alignment)
{
    *size = VPU_ENC_BITSTREAM_BUFFER_SIZE;
    *alignment = VPU_MEMORY_ALIGNMENT;
}

static void bmvpu_enc_get_work_buffer_info(size_t *size, uint32_t *alignment)
{
    *size = VPU_ENC_WORK_BUFFER_SIZE;
    *alignment = VPU_MEMORY_ALIGNMENT;
}

void bmvpu_enc_set_default_open_params(BmVpuEncOpenParams *open_params,
                                       BmVpuCodecFormat codec_format)
{
    if (open_params == NULL) {
        BM_VPU_ERROR("bmvpu_enc_set_default_open_params failed: open_params is null.");
        return;
    }


    memset(open_params, 0, sizeof(BmVpuEncOpenParams));

    open_params->codec_format = codec_format;
    open_params->color_format = BM_VPU_COLOR_FORMAT_YUV420;
    open_params->frame_width = 0;
    open_params->frame_height = 0;
    open_params->fps_num = 1;
    open_params->fps_den = 1;

    open_params->cqp = -1;
    open_params->bitrate = 100000;
    open_params->vbv_buffer_size = 0;

    open_params->bg_detection    = 0;
    open_params->enable_nr       = 1;

    open_params->mb_rc           = 1;
    open_params->delta_qp        = 5;
    open_params->min_qp          = 8;
    open_params->max_qp          = 51;

    open_params->chroma_interleave = 0;

    open_params->soc_idx = 0;

    open_params->gop_preset = 5;
    open_params->intra_period = 60;

    open_params->enc_mode = 1;

    open_params->roi_enable = 0;

    open_params->max_num_merge = 2;

    open_params->enable_constrained_intra_prediction = 0;

    open_params->enable_wp = 1;

    open_params->disable_deblocking = 0;
    open_params->offset_tc          = 0;
    open_params->offset_beta        = 0;
    open_params->enable_cross_slice_boundary = 0;


    if (codec_format == BM_VPU_CODEC_FORMAT_H264)
    {
        BmVpuEncH264Params* par = &(open_params->h264_params);
        par->enable_transform8x8 = 1;
    }
    else if (codec_format == BM_VPU_CODEC_FORMAT_H265)
    {
        BmVpuEncH265Params* par = &(open_params->h265_params);

        par->enable_intra_trans_skip = 1;
        par->enable_strong_intra_smoothing = 1;
        par->enable_tmvp = 1;
        par->enable_wpp = 0;
        par->enable_sao = 1;

        par->enable_intraNxN = 1;
    }
}

static int bmvpu_enc_check_open_params(BmVpuEncOpenParams *opt)
{
    static int preset_gop_size[] = {
        1,  /* Custom GOP, Not used */
        1,  /* All Intra */
        1,  /* IPP Cyclic GOP size 1 */
        1,  /* IBB Cyclic GOP size 1 */
        2,  /* IBP Cyclic GOP size 2 */
        4,  /* IBBBP */
        4,  /* IPPPP */
        4,  /* IBBBB */
        8,  /* IBBBBBBBB */
    };
    int gop_step_size;
    bool low_delay;
    int unit, mask;

    if (opt->codec_format != BM_VPU_CODEC_FORMAT_H264 &&
        opt->codec_format != BM_VPU_CODEC_FORMAT_H265)
    {
        BM_VPU_ERROR("Invalid codec format %d. VPU encoder only supports H.264(%d) and H.265(%d)",
                     opt->codec_format, BM_VPU_CODEC_FORMAT_H264, BM_VPU_CODEC_FORMAT_H265);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (opt->codec_format == BM_VPU_CODEC_FORMAT_H264) {
        unit = 2;
    } else {
        unit = 8;
    }
    mask = unit - 1;
    if (opt->frame_width&mask)
    {
        BM_VPU_ERROR("Picture width should be multiple of %d", unit);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }
    if (opt->frame_height&mask)
    {
        BM_VPU_ERROR("Picture height should be multiple of %d", unit);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (opt->frame_width > 8192)
    {
        BM_VPU_ERROR("Max picture width is 8192");
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }
    if (opt->frame_height > 8192)
    {
        BM_VPU_ERROR("Max picture height is 8192");
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (opt->frame_width < 256)
    {
        BM_VPU_ERROR("Min picture width is 256");
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }
    if (opt->frame_height < 128)
    {
        BM_VPU_ERROR("Min picture height is 128");
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (opt->gop_preset < 1 || opt->gop_preset > 8)
    {
        BM_VPU_ERROR("GOP preset IDX is only one of 1,2,...,8");
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    if (opt->roi_enable < 0 || opt->roi_enable > 1)
    {
        BM_VPU_ERROR("roi enable value is 0 or 1.");
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    low_delay = false;
    if (opt->gop_preset == 1 ||
        opt->gop_preset == 2 || opt->gop_preset == 3 ||
        opt->gop_preset == 6 || opt->gop_preset == 7)
        low_delay = true;

    if (low_delay)
        gop_step_size = 1;
    else
        gop_step_size = preset_gop_size[opt->gop_preset];

    //if (opt->codec_format == BM_VPU_CODEC_FORMAT_H265) // TODO For H.264
    //{
    if (!low_delay && (opt->intra_period != 0) &&
        (opt->intra_period % gop_step_size) != 0)
    {
        BM_VPU_ERROR("Not support intra period[%d] for the gop structure",
                     opt->intra_period);
        BM_VPU_ERROR("recommending: Intra period = %d\n",
                     gop_step_size * (opt->intra_period / gop_step_size));
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }
    //}

    BM_VPU_TRACE("rc bit rate: %d", opt->bitrate);
    BM_VPU_TRACE("rc vbv buffer size: %d", opt->vbv_buffer_size);
    if (opt->bitrate > 0) {
        if (opt->vbv_buffer_size > 0) {
            int size_in_ms = opt->vbv_buffer_size*1000/opt->bitrate;
            if (size_in_ms < 10 || size_in_ms > 3000) {
                BM_VPU_ERROR("rc vbv size should be in range [%d, %d]\n",
                             (opt->bitrate+99)/100, opt->bitrate*3);
                return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
            }
        }
    }
    return BM_VPU_ENC_RETURN_CODE_OK;
}

static int bmvpu_enc_clear_work_buffer(bm_handle_t bm_handle, bm_device_mem_t *work_dmabuffer)
{
    int size = (*work_dmabuffer).size;

#ifdef BM_PCIE_MODE
    uint8_t* tmp_va = calloc(size, 1);
    if (tmp_va == NULL)
    {
        BM_VPU_ERROR("malloc failed");
        return -1;
    }

    int ret = bm_memcpy_s2d_partial(bm_handle, *work_dmabuffer, tmp_va, size);
    if (ret != 0)
    {
        BM_VPU_ERROR("PCIE mode vpu upload data failed");
        free(tmp_va);
        return -1;
    }

    free(tmp_va);
#else
    unsigned long long tmp_va;
    int ret = bm_mem_mmap_device_mem_no_cache(bm_handle, work_dmabuffer, &tmp_va);
    if (ret != 0)
    {
        BM_VPU_ERROR("SoC mode vpu upload data failed");
        return -1;
    }
    memset(tmp_va, 0, size);
    bm_mem_unmap_device_mem(bm_handle, tmp_va, size);

#endif

    return 0;
}

int bmvpu_enc_open(BmVpuEncoder **encoder,
                   BmVpuEncOpenParams *open_params,
                   bm_device_mem_t *bs_dmabuffer)
{
    VpuEncOpenParam eop;
    size_t work_buffer_size;
    uint32_t work_buffer_alignment;
    int soc_idx = open_params->soc_idx;
    BmVpuEncReturnCodes ret;
    int enc_ret;

    if((encoder == NULL) || (open_params == NULL) || (bs_dmabuffer == NULL)){
        BM_VPU_ERROR("bmvpu_enc_open params err: encoder(0X%x), open_params(0X%x), bs_dmabuffer(0X%x)", encoder, open_params, bs_dmabuffer);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    /* Check that the allocated bitstream buffer is big enough */
    int size = bs_dmabuffer->size;
    if (size < VPU_ENC_BITSTREAM_BUFFER_SIZE) {
        BM_VPU_ERROR("Get bs_dmabuffer buffer size failed: size=%d",size);
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }

#ifdef __linux__
    sigemptyset(&oldmask[soc_idx]);
    sigemptyset(&newmask[soc_idx]);
    sigaddset(&newmask[soc_idx], SIGINT);
    sigaddset(&newmask[soc_idx], SIGTERM);

#endif
    ret = bmvpu_enc_check_open_params(open_params);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
        return ret;

    /* Allocate encoder instance */
    *encoder = malloc(sizeof(BmVpuEncoder));
    if ((*encoder) == NULL)
    {
        BM_VPU_ERROR("allocating memory for encoder object failed");
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }

    /* Set default encoder values */
    memset(*encoder, 0, sizeof(BmVpuEncoder));

    (*encoder)->first_frame = TRUE;

    /* Map the bitstream buffer. This mapping will persist until the encoder is closed. */
#ifndef BM_PCIE_MODE
    bm_mem_mmap_device_mem_no_cache(bmvpu_enc_get_bmlib_handle(soc_idx), bs_dmabuffer, &((*encoder)->bs_virt_addr));
#endif
    (*encoder)->bs_phys_addr = bm_mem_get_device_addr(*bs_dmabuffer);
    (*encoder)->bs_dmabuffer = bs_dmabuffer;

    (*encoder)->cbcr_interleave = open_params->chroma_interleave;


#ifdef BM_PCIE_MODE
    (*encoder)->soc_idx = open_params->soc_idx;
    (*encoder)->bm_handle = bmvpu_enc_get_bmlib_handle(open_params->soc_idx);
#else
    (*encoder)->soc_idx = 0;
    (*encoder)->bm_handle = bmvpu_enc_get_bmlib_handle(0);
#endif

    (*encoder)->core_idx = bmvpu_enc_get_core_idx((*encoder)->soc_idx);

    memset(&eop, 0, sizeof(VpuEncOpenParam));

    vpu_SetEncOpenParam(&eop, open_params->frame_width, open_params->frame_height,
                        open_params->fps_num, open_params->fps_den,
                        open_params->bitrate, open_params->cqp);

    eop.socIdx  = (*encoder)->soc_idx;
    eop.coreIdx = (*encoder)->core_idx;

    if (open_params->codec_format == BM_VPU_CODEC_FORMAT_H264)
        eop.bitstreamFormat = VPU_CODEC_AVC;
    else /* if (open_params->codec_format == BM_VPU_CODEC_FORMAT_H265) */
        eop.bitstreamFormat = VPU_CODEC_HEVC;

    /* Fill in the bitstream buffer address and size. */
    eop.bitstreamBuffer     = bm_mem_get_device_addr(*bs_dmabuffer);
    eop.bitstreamBufferSize = bm_mem_get_device_size(*bs_dmabuffer);

    /* Miscellaneous codec format independent values */
    BM_VPU_TRACE("input bit rate: %d", open_params->bitrate);
    BM_VPU_TRACE("input vbv buffer size: %d", open_params->vbv_buffer_size);
    if (open_params->bitrate > 0) {
        if (open_params->vbv_buffer_size > 0) {
            eop.vbvBufferSize = open_params->vbv_buffer_size*1000/open_params->bitrate;
        } else {
            eop.vbvBufferSize = 3000;
        }
        BM_VPU_TRACE("hw opt: vbv buffer size in ms: %d", eop.vbvBufferSize);
    }

    eop.waveParam.bgDetectEnable  = open_params->bg_detection;
    eop.waveParam.nrYEnable       =
    eop.waveParam.nrCbEnable      =
    eop.waveParam.nrCrEnable      =
    eop.waveParam.nrNoiseEstEnable= open_params->enable_nr;


    if (open_params->codec_format == BM_VPU_CODEC_FORMAT_H264)
        eop.waveParam.mbLevelRcEnable = open_params->mb_rc;
    else
        eop.waveParam.cuLevelRCEnable = open_params->mb_rc;
    eop.waveParam.maxDeltaQp = open_params->delta_qp;

    eop.waveParam.minQpI     =
    eop.waveParam.minQpP     =
    eop.waveParam.minQpB     = open_params->min_qp;

    eop.waveParam.maxQpI     =
    eop.waveParam.maxQpP     =
    eop.waveParam.maxQpB     = open_params->max_qp;

    eop.cbcrInterleave = open_params->chroma_interleave;
    eop.cbcrOrder = 0;
    eop.nv21 = 0;

    eop.waveParam.gopPresetIdx = open_params->gop_preset;
    eop.waveParam.intraPeriod  = open_params->intra_period;

    eop.waveParam.useRecommendEncParam = open_params->enc_mode;

    eop.waveParam.maxNumMerge = open_params->max_num_merge;
    eop.waveParam.constIntraPredFlag = open_params->enable_constrained_intra_prediction;

    eop.waveParam.weightPredEnable = open_params->enable_wp;

    eop.waveParam.disableDeblk   = open_params->disable_deblocking;
    eop.waveParam.tcOffsetDiv2   = open_params->offset_tc;
    eop.waveParam.betaOffsetDiv2 = open_params->offset_beta;
    eop.waveParam.lfCrossSliceBoundaryEnable = open_params->enable_cross_slice_boundary;

    eop.waveParam.chromaCbQpOffset = 0;
    eop.waveParam.chromaCrQpOffset = 0;

    eop.waveParam.roiEnable = open_params->roi_enable;

    /* Fill in codec format specific values into the VPU's encoder open param structure */
    if (open_params->codec_format==BM_VPU_CODEC_FORMAT_H264)
    {
        BmVpuEncH264Params* par = &(open_params->h264_params);

        eop.waveParam.profile = H264_TAG_PROFILE_HIGH;
        eop.waveParam.level = H26X_LEVEL(4, 1);

        eop.waveParam.transform8x8Enable = par->enable_transform8x8;
        eop.waveParam.entropyCodingMode = 1; // cabac

        /* Check if the frame fits within the 16-pixel boundaries. */
        eop.waveParam.confWinRight = (16 - (open_params->frame_width  & 15)) & 15;
        eop.waveParam.confWinBot   = (16 - (open_params->frame_height & 15)) & 15;
    }
    else /* if (open_params->codec_format==BM_VPU_CODEC_FORMAT_H265) */
    {
        BmVpuEncH265Params* par = &(open_params->h265_params);

        eop.waveParam.profile = H265_TAG_PROFILE_MAIN;
        eop.waveParam.level = H26X_LEVEL(4, 1);

        eop.waveParam.skipIntraTrans          = par->enable_intra_trans_skip;
        eop.waveParam.strongIntraSmoothEnable = par->enable_strong_intra_smoothing;
        eop.waveParam.tmvpEnable              = par->enable_tmvp;
        eop.waveParam.wppEnable               = par->enable_wpp;
        eop.waveParam.saoEnable               = par->enable_sao;
        eop.waveParam.intraNxNEnable = par->enable_intraNxN;
#if 0
        /* Check if the frame fits within the 8-pixel boundaries. */
        eop.waveParam.confWinRight = (8 - (open_params->frame_width  & 7)) & 7;
        eop.waveParam.confWinBot   = (8 - (open_params->frame_height & 7)) & 7;
#endif
    }

    /* Retrieve information about the required working buffer */
    bmvpu_enc_get_work_buffer_info(&(work_buffer_size), &(work_buffer_alignment));

    /* in unit of 4k bytes */
    work_buffer_size = (work_buffer_size +(4*1024-1)) & (~(4*1024-1));

    /* Create work buffer */
    (*encoder)->work_dmabuffer = (bm_device_mem_t *)malloc(sizeof(bm_device_mem_t));
    ret = bmvpu_malloc_device_byte_heap(bmvpu_enc_get_bmlib_handle((*encoder)->soc_idx), (*encoder)->work_dmabuffer, work_buffer_size, HEAP_MASK, 1);  
    if (ret != 0)
    {
        BM_VPU_ERROR("bm_malloc_device_byte for work_dmabuffer failed!\n");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    // BM_VPU_LOG("work buffer: pa = 0x%lx, size = %zd",
    //            bmvpu_dma_buffer_get_physical_address(work_dmabuffer),
    //            work_buffer_size);

    /* Clear the working buffer */
    ret = bmvpu_enc_clear_work_buffer(bmvpu_enc_get_bmlib_handle((*encoder)->soc_idx), (*encoder)->work_dmabuffer);
    if (ret != 0)
    {
        BM_VPU_ERROR("bmvpu_enc_clear_work_buffer failed");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    /* Fill in the working buffer address and size. */
    eop.workBuffer     = bm_mem_get_device_addr(*((*encoder)->work_dmabuffer));
    eop.workBufferSize = bm_mem_get_device_size(*((*encoder)->work_dmabuffer));

    /* Now actually open the encoder instance */
    BM_VPU_LOG("opening encoder, frame size: %u x %u pixel",
               open_params->frame_width, open_params->frame_height);

    enc_ret = vpu_EncOpen((VpuEncoder**)(&((*encoder)->handle)), &eop);
    ret = BM_VPU_ENC_HANDLE_ERROR("could not open encoder", enc_ret);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
        goto cleanup;

    (*encoder)->codec_format = open_params->codec_format;
    (*encoder)->color_format = open_params->color_format;
    (*encoder)->frame_width  = open_params->frame_width;
    (*encoder)->frame_height = open_params->frame_height;
    (*encoder)->fps_n        = open_params->fps_num;
    (*encoder)->fps_d        = open_params->fps_den;
    (*encoder)->rc_enable    = (open_params->bitrate > 0);
    (*encoder)->cqp          = open_params->cqp;

finish:
    if (ret == BM_VPU_ENC_RETURN_CODE_OK)
        BM_VPU_DEBUG("successfully opened encoder");

    return ret;

cleanup:
#ifndef BM_PCIE_MODE
    bm_mem_unmap_device_mem(bmvpu_enc_get_bmlib_handle((*encoder)->soc_idx), (void *)((*encoder)->bs_virt_addr), bm_mem_get_device_size(*bs_dmabuffer)); 
#endif
    if ((*encoder)->work_dmabuffer!=NULL)
    {
        bm_free_device(bmvpu_enc_get_bmlib_handle((*encoder)->soc_idx), *((*encoder)->work_dmabuffer));
        free((*encoder)->work_dmabuffer);
    }

    free(*encoder);
    *encoder = NULL;

    goto finish;
}

int bmvpu_enc_close(BmVpuEncoder *encoder)
{
    BmVpuEncReturnCodes ret;
    int enc_ret;

    if (encoder == NULL)
        return BM_VPU_ENC_RETURN_CODE_OK;

    BM_VPU_DEBUG("closing encoder");

    /* Close the encoder handle */

    enc_ret = vpu_EncClose(encoder->handle);
    if (enc_ret == VPU_RET_FRAME_NOT_COMPLETE)
    {
        BM_VPU_ENC_HANDLE_ERROR("error while closing encoder", enc_ret);

        /* TODO VPU refused to close, since a frame is partially encoded.
         * Force it to close by first resetting the handle and retry. */
        vpu_EncSWReset(encoder->handle, 0);

        enc_ret = vpu_EncClose(encoder->handle);
    }
    ret = BM_VPU_ENC_HANDLE_ERROR("error while closing encoder", enc_ret);


    /* Remaining cleanup */

    bmvpu_enc_free_header_data(encoder);

#ifndef BM_PCIE_MODE
    if (encoder->bs_dmabuffer != NULL)
        bm_mem_unmap_device_mem(encoder->bm_handle, (void *)(encoder->bs_virt_addr), bm_mem_get_device_size(*encoder->bs_dmabuffer));
#endif

    if (encoder->internal_framebuffers != NULL)
    {
        // BM_VPU_FREE(encoder->internal_framebuffers, sizeof(VpuFrameBuffer) * encoder->num_framebuffers);
        free(encoder->internal_framebuffers);
        encoder->internal_framebuffers = NULL;
    }

    if (encoder->buffer_mv)
    {
        bm_free_device(encoder->bm_handle, *(encoder->buffer_mv));
        free(encoder->buffer_mv);
        encoder->buffer_mv = NULL;
    }
    if (encoder->buffer_fbc_y_tbl)
    {
        bm_free_device(encoder->bm_handle, *(encoder->buffer_fbc_y_tbl));
        free(encoder->buffer_fbc_y_tbl);
        encoder->buffer_fbc_y_tbl = NULL;
    }

    if (encoder->buffer_fbc_c_tbl)
    {
        bm_free_device(encoder->bm_handle, *(encoder->buffer_fbc_c_tbl));
        free(encoder->buffer_fbc_c_tbl);
        encoder->buffer_fbc_c_tbl = NULL;
    }

    if (encoder->buffer_sub_sam)
    {
        bm_free_device(encoder->bm_handle, *(encoder->buffer_sub_sam));
        free(encoder->buffer_sub_sam);
        encoder->buffer_sub_sam = NULL;
    }

    if (encoder->work_dmabuffer)
    {
        bm_free_device(encoder->bm_handle, *(encoder->work_dmabuffer));
        free(encoder->work_dmabuffer);
        encoder->work_dmabuffer = NULL;
    }

    free(encoder);
    encoder = NULL;



    if (ret == BM_VPU_ENC_RETURN_CODE_OK)
        BM_VPU_DEBUG("successfully closed encoder");

    return ret;
}


int bmvpu_enc_register_framebuffers(BmVpuEncoder *encoder,
                                    BmVpuFramebuffer *framebuffers,
                                    uint32_t num_framebuffers)
{
    VpuEncoder* handle = encoder->handle;
    bm_handle_t bm_handle = encoder->bm_handle;

    bm_device_mem_t* dmabuffer_mv        = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t));
    bm_device_mem_t* dmabuffer_fbc_y_tbl = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t));
    bm_device_mem_t* dmabuffer_fbc_c_tbl = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t));
    bm_device_mem_t* dmabuffer_sub_sam   = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t));

    uint32_t i;
    BmVpuEncReturnCodes ret;
    int enc_ret;

    if((encoder == NULL) || (framebuffers == NULL)){
        BM_VPU_ERROR("bmvpu_enc_register_framebuffers params err: encoder(0X%x), framebuffers(0X%x)", encoder, framebuffers);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    BM_VPU_DEBUG("attempting to register %u framebuffers", num_framebuffers);

    /* Allocate memory for framebuffer structures */
    encoder->internal_framebuffers = (VpuFrameBuffer*)malloc(sizeof(VpuFrameBuffer) * num_framebuffers);
    if (encoder->internal_framebuffers == NULL)
    {
        BM_VPU_ERROR("allocating memory for framebuffers failed");
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }
    memset(encoder->internal_framebuffers, 0, sizeof(VpuFrameBuffer) * num_framebuffers);

    /* Copy the values from the framebuffers array to the internal_framebuffers
     * one, which in turn will be used by the VPU */
    for (i = 0; i < num_framebuffers; ++i)
    {
        VpuFrameBuffer *internal_fb = NULL;
        BmVpuFramebuffer *fb = &framebuffers[i];
        bmvpu_phys_addr_t phys_addr = 0;

        phys_addr = bm_mem_get_device_addr(*(fb->dma_buffer));

        internal_fb = &(encoder->internal_framebuffers[i]);
        internal_fb->myIndex = i;
        internal_fb->mapType = BM_COMPRESSED_FRAME_MAP;
        internal_fb->format = FB_FMT_420;
        internal_fb->nv21 = 0;
        internal_fb->stride = fb->y_stride;
        internal_fb->width  = fb->width;
        internal_fb->height = fb->height;
        internal_fb->lumaBitDepth   = 8;
        internal_fb->chromaBitDepth = 8;
        /* if mapType is BM_COMPRESSED_FRAME_MAP, set 128BIT_LITTLE_ENDIAN */
        internal_fb->endian = 16; /* 128BIT_LITTLE_ENDIAN */

        internal_fb->bufY  = (bm_pa_t)(phys_addr + fb->y_offset);
        internal_fb->bufCb = (bm_pa_t)(phys_addr + fb->cb_offset);
        internal_fb->bufCr = (bm_pa_t)(phys_addr + fb->cr_offset);
    }

    ret = vpu_EncCalcCoreBufferSize(handle, num_framebuffers);
    if (ret != 0)
    {
        BM_VPU_ERROR("vpu_EncCalcCoreBufferSize failed");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    int mv_size        = handle->vbMV.size;
    int fbc_y_tbl_size = handle->vbFbcYTbl.size;
    int fbc_c_tbl_size = handle->vbFbcCTbl.size;
    int sub_sam_size   = handle->vbSubSamBuf.size;

    /* buffer_mv */
    ret = bmvpu_malloc_device_byte_heap(bm_handle, dmabuffer_mv, mv_size, HEAP_MASK, 1);
    if (ret != 0)
    {
        BM_VPU_ERROR("Get buffer_mv failed");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    /* buffer_fbc_y_tbl */
    ret = bmvpu_malloc_device_byte_heap(bm_handle, dmabuffer_fbc_y_tbl, fbc_y_tbl_size, HEAP_MASK, 1);
    if (ret != 0)
    {
        BM_VPU_ERROR("Get buffer_fbc_y_tbl failed");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    /* buffer_fbc_y_tbl */
    ret = bmvpu_malloc_device_byte_heap(bm_handle, dmabuffer_fbc_c_tbl, fbc_c_tbl_size, HEAP_MASK, 1);
    if (ret != 0)
    {
        BM_VPU_ERROR("Get buffer_fbc_c_tbl failed");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    /* buffer_fbc_y_tbl */
    ret = bmvpu_malloc_device_byte_heap(bm_handle, dmabuffer_sub_sam, sub_sam_size, HEAP_MASK, 1);
    if (ret != 0)
    {
        BM_VPU_ERROR("Get buffer_sub_sam failed");
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto cleanup;
    }

    BM_VPU_LOG("mv:        pa = 0x%lx, size = %d \n",
               bm_mem_get_device_addr(*(encoder->buffer_mv)),
               mv_size);
    BM_VPU_LOG("fbc_y_tbl: pa = 0x%lx, size = %d",
               bm_mem_get_device_addr(*(encoder->buffer_fbc_y_tbl)),
               fbc_y_tbl_size);
    BM_VPU_LOG("fbc_c_tbl: pa = 0x%lx, size = %d",
               bm_mem_get_device_addr(*(encoder->buffer_fbc_c_tbl)),
               fbc_c_tbl_size);
    BM_VPU_LOG("sub_sam:   pa = 0x%lx, size = %d",
               bm_mem_get_device_addr(*(encoder->buffer_sub_sam)),
               sub_sam_size);

    encoder->buffer_mv        = dmabuffer_mv;
    encoder->buffer_fbc_y_tbl = dmabuffer_fbc_y_tbl;
    encoder->buffer_fbc_c_tbl = dmabuffer_fbc_c_tbl;
    encoder->buffer_sub_sam   = dmabuffer_sub_sam;

    handle->vbMV.pa        = bm_mem_get_device_addr(*(encoder->buffer_mv));
    handle->vbFbcYTbl.pa   = bm_mem_get_device_addr(*(encoder->buffer_fbc_y_tbl));
    handle->vbFbcCTbl.pa   = bm_mem_get_device_addr(*(encoder->buffer_fbc_c_tbl));
    handle->vbSubSamBuf.pa = bm_mem_get_device_addr(*(encoder->buffer_sub_sam));

    enc_ret = vpu_EncRegisterFrameBuffer(encoder->handle,
                                         encoder->internal_framebuffers,
                                         num_framebuffers);
    ret = BM_VPU_ENC_HANDLE_ERROR("could not register framebuffers", enc_ret);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
        goto cleanup;

    /* Store the pointer to the caller-supplied framebuffer array */
    encoder->framebuffers = framebuffers;
    encoder->num_framebuffers = num_framebuffers;

    /*    OpenEncoder(CREATE_INST)
     * => SetSequenceInfo(INIT_SEQ)
     * => RegisterFrameBuffers(SET_FB)
     * => EncodeHeader
     * => Encode */

    /* The header data does not change during encoding, so
     * it only has to be generated once */
    // bmvpu_enc_lock(encoder->soc_idx);
    ret = bmvpu_enc_generate_header_data(encoder);

    bmvpu_enc_unlock(encoder->soc_idx);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
    {
        BM_VPU_ERROR("bmvpu_enc_generate_header_data() failed");
        return ret;
    }

    return BM_VPU_ENC_RETURN_CODE_OK;

cleanup:

    handle->vbMV.pa        = bm_mem_get_device_addr(*(encoder->buffer_mv));
    handle->vbFbcYTbl.pa   = bm_mem_get_device_addr(*(encoder->buffer_fbc_y_tbl));
    handle->vbFbcCTbl.pa   = bm_mem_get_device_addr(*(encoder->buffer_fbc_c_tbl));
    handle->vbSubSamBuf.pa = bm_mem_get_device_addr(*(encoder->buffer_sub_sam));

    if (encoder->buffer_mv != NULL)
    {
        bm_free_device(bm_handle, *(encoder->buffer_mv));
        free(encoder->buffer_mv);
    }

    if (encoder->buffer_fbc_y_tbl != NULL)
    {
        bm_free_device(bm_handle, *(encoder->buffer_fbc_y_tbl));
        free(encoder->buffer_fbc_y_tbl);
    }

    if (encoder->buffer_fbc_c_tbl != NULL)
    {
        bm_free_device(bm_handle, *(encoder->buffer_fbc_c_tbl));
        free(encoder->buffer_fbc_c_tbl);
    }

    if (encoder->buffer_sub_sam != NULL)
    {
        bm_free_device(bm_handle, *(encoder->buffer_sub_sam));
        free(encoder->buffer_sub_sam);
    }

    if (encoder->internal_framebuffers)
    {
        // BM_VPU_FREE(encoder->internal_framebuffers, sizeof(VpuFrameBuffer) * num_framebuffers);
        free(encoder->internal_framebuffers);
        encoder->internal_framebuffers = NULL;
    }

    return ret;
}

int bmvpu_enc_get_initial_info(BmVpuEncoder *encoder, BmVpuEncInitialInfo *info)
{
    VpuEncInitialInfo initial_info = {0};
    BmVpuEncReturnCodes ret;
    int enc_ret;

    if((encoder == NULL) || (info == NULL)){
        BM_VPU_ERROR("bmvpu_enc_get_initial_info params err: encoder(0X%x), info(0X%x)", encoder, info);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    memset(&initial_info, 0, sizeof(VpuEncInitialInfo));

    bmvpu_enc_lock(encoder->soc_idx);
    enc_ret = vpu_EncSetSeqInfo(encoder->handle, &initial_info);
    ret = BM_VPU_ENC_HANDLE_ERROR("could not get initial info", enc_ret);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK) {
        bmvpu_enc_unlock(encoder->soc_idx);
        return ret;
    }


    info->framebuffer_alignment = 1;
#if 0
    info->min_num_rec_fb = initial_info.minFrameBufferCount;
    if (info->min_num_rec_fb == 0)
        info->min_num_rec_fb = 1;
#else
    info->min_num_rec_fb = initial_info.reconFbNum;
    info->min_num_src_fb = initial_info.srcFbNum; // TODO
#endif

    BM_VPU_DEBUG("min_num_rec_fb=%d", info->min_num_rec_fb);

    enc_ret = bmvpu_calc_framebuffer_sizes(BM_LINEAR_FRAME_MAP, encoder->color_format,
                                 encoder->frame_width, encoder->frame_height,
                                 encoder->cbcr_interleave, &(info->src_fb));
    if (enc_ret != BM_VPU_ENC_RETURN_CODE_OK) {
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }

    enc_ret = bmvpu_calc_framebuffer_sizes(BM_COMPRESSED_FRAME_MAP, encoder->color_format,
                                 encoder->frame_width, encoder->frame_height,
                                 encoder->cbcr_interleave, &(info->rec_fb));
    if (enc_ret != BM_VPU_ENC_RETURN_CODE_OK) {
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }

    BM_VPU_DEBUG("src fb: width=%d, height=%d, stride=%d, luma size=%d, chroma size=%d, total size=%d",
                 info->src_fb.width, info->src_fb.height, info->src_fb.y_stride,
                 info->src_fb.y_size, info->src_fb.c_size, info->src_fb.size);
    BM_VPU_DEBUG("rec fb: width=%d, height=%d, stride=%d, luma size=%d, chroma size=%d, total size=%d",
                 info->rec_fb.width, info->rec_fb.height, info->rec_fb.y_stride,
                 info->rec_fb.y_size, info->rec_fb.c_size, info->rec_fb.size);

    memcpy(&(encoder->initial_info), info, sizeof(BmVpuEncInitialInfo));

    return BM_VPU_ENC_RETURN_CODE_OK;
}

int bmvpu_enc_encode(BmVpuEncoder *encoder,
                     BmVpuRawFrame const *raw_frame,
                     BmVpuEncodedFrame *encoded_frame,
                     BmVpuEncParams *encoding_params,
                     uint32_t *output_code)
{
    VpuEncOutputInfo enc_output_info;
    VpuEncParam enc_param;
    VpuFrameBuffer src_fb;
    bmvpu_phys_addr_t pa;
    BOOL timeout;
    BOOL add_header;
    size_t encoded_data_size;
    BmVpuEncWriteContext write_context;
    int i, enc_ret, ret = BM_VPU_ENC_RETURN_CODE_OK;
    if(!(encoder != NULL) ||
       !(raw_frame != NULL) ||
       !(encoded_frame != NULL) ||
       !(output_code != NULL) ||
       !(encoding_params != NULL)) {
        BM_VPU_ERROR("bmvpu_enc_encode params err: encoder(0X%x), raw_frame(0X%x), encoded_frame(0X%x), output_code(0X%x), encoding_params(0X%x).", \
                     encoder, raw_frame, encoded_frame, output_code, encoding_params);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }
    if(!(encoding_params->acquire_output_buffer != NULL) ||
       !(encoding_params->finish_output_buffer != NULL)) {
        BM_VPU_ERROR("bmvpu_enc_encode params err: encoding_params->acquire_output_buffer(0X%x), encoding_params->finish_output_buffer(0X%x).", \
                     encoding_params->acquire_output_buffer, encoding_params->finish_output_buffer);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;
    }

    /* Set this here to ensure that the handle is NULL if an error occurs
     * before acquire_output_buffer() is called */
    encoded_frame->acquired_handle = NULL;

    *output_code = 0;

    memset(&write_context, 0, sizeof(write_context));

    /* Fill encoding parameters structure */
    memset(&enc_param, 0, sizeof(enc_param));

    enc_param.skipPicture = encoding_params->skip_frame;

    enc_param.picStreamBufferAddr = bm_mem_get_device_addr(*(encoder->bs_dmabuffer));
    enc_param.picStreamBufferSize = bm_mem_get_device_size(*(encoder->bs_dmabuffer));

    /* FW will encode header data implicitly when changing the header syntaxes
     * If this value is 1, encodeVPS, encodeSPS, and encodePPS are ignored. */
    enc_param.codeOption.implicitHeaderEncode = 1;
    enc_param.codeOption.encodeVCL = 0;        /* encode VCL nal unit explicitly */
#if 0
    enc_param.codeOption.encodeVPS = 1;        /* encode VPS nal unit explicitly */
    enc_param.codeOption.encodeSPS = 1;        /* encode SPS nal unit explicitly */
    enc_param.codeOption.encodePPS = 1;        /* encode PPS nal unit explicitly */
#endif
    enc_param.codeOption.encodeAUD = 1;        /* encode AUD nal unit explicitly */
    enc_param.codeOption.encodeEOS = 0;        /* encode EOS nal unit explicitly. This should be set when to encode the last source picture of sequence. */
    enc_param.codeOption.encodeEOB = 0;        /* encode EOB nal unit explicitly. This should be set when to encode the last source picture of sequence. */
#if 0
    enc_param.codeOption.encodeFiller = 0;     /* encode filler data nal unit explicitly */
#endif
    if (encoding_params->customMapOpt != NULL) {
        enc_param.customMapOpt.roiAvgQp              = encoding_params->customMapOpt[encoding_params->customMapOptUsedIndex].roiAvgQp;
        enc_param.customMapOpt.customRoiMapEnable    = encoding_params->customMapOpt[encoding_params->customMapOptUsedIndex].customRoiMapEnable;
        enc_param.customMapOpt.customLambdaMapEnable = encoding_params->customMapOpt[encoding_params->customMapOptUsedIndex].customLambdaMapEnable;
        enc_param.customMapOpt.customModeMapEnable   = encoding_params->customMapOpt[encoding_params->customMapOptUsedIndex].customModeMapEnable;
        enc_param.customMapOpt.customCoefDropEnable  = encoding_params->customMapOpt[encoding_params->customMapOptUsedIndex].customCoefDropEnable;
        enc_param.customMapOpt.customCoefDropEnable  = encoding_params->customMapOpt[encoding_params->customMapOptUsedIndex].customCoefDropEnable;
        enc_param.customMapOpt.addrCustomMap         = encoding_params->customMapOpt[encoding_params->customMapOptUsedIndex].addrCustomMap;
    }

    /* Copy over information from the raw_frame's framebuffer into the
     * src_fb structure, which is what vpu_EncStartOneFrame() expects as input */
    memset(&src_fb, 0, sizeof(VpuFrameBuffer));

    src_fb.mapType = BM_LINEAR_FRAME_MAP;

    src_fb.format = FB_FMT_420;
    src_fb.cbcrInterleave = encoder->cbcr_interleave;
    src_fb.nv21 = 0;

    src_fb.lumaBitDepth   = 8;
    src_fb.chromaBitDepth = 8;

    src_fb.endian = 16; /* VPU_SOURCE_ENDIAN: 128BIT_LITTLE_ENDIAN */

    if (raw_frame->framebuffer != NULL)
    {
        BmVpuFramebuffer* framebuffer =  raw_frame->framebuffer;

        /* Get the physical address for the raw_frame that shall be encoded
         * and the virtual pointer to the output buffer */
        pa = bm_mem_get_device_addr(*(framebuffer->dma_buffer));

        BM_VPU_LOG("source framebuffer: myIndex: 0x%x, Y stride: %u,  CbCr stride: %u, pa: 0x%lx",
                   framebuffer->myIndex, framebuffer->y_stride, framebuffer->cbcr_stride, pa);

        src_fb.myIndex = framebuffer->myIndex;
        src_fb.stride = framebuffer->y_stride;
        src_fb.width  = framebuffer->width;
        src_fb.height = framebuffer->height;

        src_fb.bufY  = (bm_pa_t)(pa + framebuffer-> y_offset);
        src_fb.bufCb = (bm_pa_t)(pa + framebuffer->cb_offset);
        src_fb.bufCr = (bm_pa_t)(pa + framebuffer->cr_offset);

        src_fb.size = bm_mem_get_device_size(*(framebuffer->dma_buffer));

        enc_param.srcEndFlag = 0;
    }
    else
    {
        src_fb.myIndex = 0xFFFFFFFF;

        src_fb.stride = 0;
        src_fb.width  = 0;
        src_fb.height = 0;

        src_fb.bufY  = 0L;
        src_fb.bufCb = 0L;
        src_fb.bufCr = 0L;

        src_fb.size  = 0;

        /* mark no more source pictures */
        enc_param.srcEndFlag = 1;
    }
    enc_param.sourceFrame = &src_fb;
    enc_param.srcIdx  = src_fb.myIndex;

    enc_param.context = raw_frame->context;
    enc_param.pts     = raw_frame->pts;
    enc_param.dts     = raw_frame->pts;

    BM_VPU_LOG("enc_param: srcIdx=0x%x, context=%p, pts=0x%lx, dts=0x%lx",
               enc_param.srcIdx, enc_param.context, enc_param.pts, enc_param.dts);

    if (!encoder->rc_enable) {
        enc_param.forcePicQpEnable = 1;
        enc_param.forcePicQpI =
        enc_param.forcePicQpP =
        enc_param.forcePicQpB = encoder->cqp; // TODO
    } else {
        enc_param.forcePicQpEnable = 0;
    }
    /* Do the actual encoding */
    bmvpu_enc_lock(encoder->soc_idx);
    enc_ret = vpu_EncStartOneFrame(encoder->handle, &enc_param);
    ret = BM_VPU_ENC_HANDLE_ERROR("could not start frame encoding", enc_ret);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
    {
        bmvpu_enc_unlock(encoder->soc_idx);
        BM_VPU_ERROR("vpu_EncStartOneFrame() failed");
        goto finish;
    }

    /* Wait for frame completion */
    /* Wait a few times, since sometimes, it takes more than
     * one vpu_EncWaitForInt() call to cover the encoding interval */
    timeout = TRUE;
    BM_VPU_LOG("waiting for encoding completion");
    for (i = 0; i < VPU_MAX_TIMEOUT_COUNTS; i++)
    {
        ret = vpu_EncWaitForInt(encoder->handle, VPU_WAIT_TIMEOUT);
        if (ret == VPU_RET_SUCCESS)
        {
            timeout = FALSE;
            break;
        }
        BM_VPU_WARNING("timeout after waiting %d ms for frame completion", VPU_WAIT_TIMEOUT);
    }

    /* Retrieve information about the result of the encode process. Do so even if
     * a timeout occurred. This is intentional, since according to the VPU docs,
     * vpu_EncStartOneFrame() won't be usable again until vpu_EncGetOutputInfo()
     * is called. In other words, the vpu_EncStartOneFrame() locks down some
     * internals inside the VPU, and vpu_EncGetOutputInfo() releases them. */

    memset(&enc_output_info, 0, sizeof(VpuEncOutputInfo));
    enc_ret = vpu_EncGetOutputInfo(encoder->handle, &enc_output_info);
    bmvpu_enc_unlock(encoder->soc_idx);
    ret = BM_VPU_ENC_HANDLE_ERROR("could not get output information", enc_ret);
    if (ret != BM_VPU_ENC_RETURN_CODE_OK)
    {
        BM_VPU_ERROR("vpu_EncGetOutputInfo failed");
        goto finish;
    }


    /* If a timeout occurred earlier, this is the correct time to abort
     * encoding and return an error code, since vpu_EncGetOutputInfo()
     * has been called, unlocking the VPU encoder calls. */
    if (timeout)
    {
        BM_VPU_ERROR("Time out");
        ret = BM_VPU_ENC_RETURN_CODE_TIMEOUT;
        goto finish;
    }

    encoded_frame->data_size = 0;

    encoded_frame->frame_type = convert_frame_type(enc_output_info.picType);

    encoded_frame->src_idx = enc_output_info.encSrcIdx;
    encoded_frame->context = enc_output_info.context;
    encoded_frame->pts     = enc_output_info.pts;
    encoded_frame->dts     = enc_output_info.dts;
    encoded_frame->avg_ctu_qp = enc_output_info.avgCtuQp;

    BM_VPU_LOG("output info: picType %d (%s), skipEncoded %d, bitstream buffer %lx size %u",
               enc_output_info.picType, bmvpu_frame_type_string(encoded_frame->frame_type),
               enc_output_info.picSkipped,
               enc_output_info.bitstreamBuffer,
               enc_output_info.bitstreamSize);

    BM_VPU_LOG("SrcIdx 0x%x, GopPicIdx %d, PicPoc %d, PicCnt %d, reconFrameIndex %d",
               enc_output_info.encSrcIdx,
               enc_output_info.encGopPicIdx,
               enc_output_info.encPicPoc,
               enc_output_info.encPicCnt,
               enc_output_info.reconFrameIndex);

    if (enc_output_info.bitstreamSize <= 0)
    {
        BM_VPU_LOG("output bitstream size %d", enc_output_info.bitstreamSize);
        ret = BM_VPU_ENC_RETURN_CODE_OK;
        goto finish;
    }

    encoded_data_size = enc_output_info.bitstreamSize;

    add_header = (encoder->first_frame ||
                  (encoded_frame->frame_type == BM_VPU_FRAME_TYPE_IDR) ||
                  (encoded_frame->frame_type == BM_VPU_FRAME_TYPE_I));
    if (add_header)
        encoded_data_size += encoder->headers_rbsp_size;

    encoded_frame->data_size = encoded_data_size;

    write_context.start = encoding_params->acquire_output_buffer(encoding_params->output_buffer_context,
                                                                 encoded_data_size,
                                                                 &(encoded_frame->acquired_handle));
    if (write_context.start == NULL)
    {
        BM_VPU_ERROR("could not acquire buffer with %zu byte for encoded frame data", encoded_data_size);
        ret = BM_VPU_ENC_RETURN_CODE_ERROR;
        goto finish;
    }

    write_context.p   = write_context.start;
    write_context.end = write_context.p + encoded_data_size;

    /* Add header data if necessary.
     * (after the AUD, before the actual encoded frame data) */
    if (add_header)
    {
        ret = bmvpu_enc_write_header_data(encoder, encoded_frame, encoding_params, &write_context, output_code);
        if (ret != BM_VPU_ENC_RETURN_CODE_OK)
        {
            BM_VPU_ERROR("bmvpu_enc_write_header_data() failed");
            goto finish;
        }
    }

    /* Add this flag since the raw frame has been successfully consumed */
    *output_code |= BM_VPU_ENC_OUTPUT_CODE_INPUT_USED;

    /* Get the encoded data out of the bitstream buffer into the output buffer */
    if (enc_output_info.bitstreamBuffer != 0)
    {
        ptrdiff_t available_space = write_context.end - write_context.p;

        if (available_space < (ptrdiff_t)(enc_output_info.bitstreamSize))
        {
            BM_VPU_ERROR("insufficient space in output buffer for encoded data: need %u byte, got %td",
                         enc_output_info.bitstreamSize,
                         available_space);
            ret = BM_VPU_ENC_RETURN_CODE_ERROR;

            goto finish;
        }

        ret = bmvpu_enc_bs_download(encoder, write_context.p,
                                enc_output_info.bitstreamBuffer, enc_output_info.bitstreamSize);
        if (ret != 0)
        {
            BM_VPU_ERROR("bmvpu_enc_bs_download failed");
            ret = BM_VPU_ENC_RETURN_CODE_ERROR;
            goto finish;
        }
        write_context.p += enc_output_info.bitstreamSize;

        BM_VPU_LOG("added main encoded frame data with %u byte", enc_output_info.bitstreamSize);
        *output_code |= BM_VPU_ENC_OUTPUT_CODE_ENCODED_FRAME_AVAILABLE;
    }
    encoder->first_frame = FALSE;

finish:
    if (write_context.start != NULL)
        encoding_params->finish_output_buffer(encoding_params->output_buffer_context, encoded_frame->acquired_handle);

    if (ret == BM_VPU_ENC_RETURN_CODE_OK &&
        enc_output_info.reconFrameIndex == BM_VPU_ENC_REC_IDX_END) {
        BM_VPU_LOG("Encoding End");
        return BM_VPU_ENC_RETURN_CODE_END;
    }

    return ret;
}

static int
bmvpu_enc_handle_error_full(char const *fn, int linenr, char const *funcn, char const *msg_start, int ret_code)
{
    switch (ret_code)
    {
    case VPU_RET_SUCCESS:
        return BM_VPU_ENC_RETURN_CODE_OK;

    case VPU_RET_FAILURE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: failure", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_INVALID_HANDLE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid handle", msg_start);
        return BM_VPU_ENC_RETURN_CODE_INVALID_HANDLE;

    case VPU_RET_INVALID_PARAM:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid parameters", msg_start);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;

    case VPU_RET_INVALID_COMMAND:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid command", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_ROTATOR_OUTPUT_NOT_SET:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: rotation enabled but rotator output buffer not set", msg_start);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;

    case VPU_RET_ROTATOR_STRIDE_NOT_SET:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: rotation enabled but rotator stride not set", msg_start);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;

    case VPU_RET_FRAME_NOT_COMPLETE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: frame encoding operation not complete", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_INVALID_FRAME_BUFFER:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: frame buffers are invalid", msg_start);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;

    case VPU_RET_INSUFFICIENT_FRAME_BUFFERS:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: not enough frame buffers specified (must be equal to or larger than the minimum number reported by bmvpu_enc_get_initial_info)", msg_start);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;

    case VPU_RET_INVALID_STRIDE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: invalid stride - check Y stride values of framebuffers (must be a multiple of 8 and equal to or larger than the frame width)", msg_start);
        return BM_VPU_ENC_RETURN_CODE_INVALID_PARAMS;

    case VPU_RET_WRONG_CALL_SEQUENCE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: wrong call sequence", msg_start);
        return BM_VPU_ENC_RETURN_CODE_WRONG_CALL_SEQUENCE;

    case VPU_RET_CALLED_BEFORE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: already called before (may not be called more than once in a VPU instance)", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_NOT_INITIALIZED:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: VPU is not initialized", msg_start);
        return BM_VPU_ENC_RETURN_CODE_WRONG_CALL_SEQUENCE;

    case VPU_RET_USERDATA_BUF_NOT_SET:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: no memory allocation for reporting userdata", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_MEMORY_ACCESS_VIOLATION:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: memory access violation", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_VPU_RESPONSE_TIMEOUT:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: response timeout", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_INSUFFICIENT_RESOURCE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: lack of memory.", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_NOT_FOUND_BITCODE_PATH:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: BIT_CODE_FILE_PATH has a wrong firmware path or firmware size is 0", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_UNSUPPORTED_FEATURE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: feature not supported", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_NOT_FOUND_VPU_DEVICE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: undefined product ID", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_CP0_EXCEPTION:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: coprocessor exception", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_STREAM_BUF_FULL:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: stream buffer is full", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_ACCESS_VIOLATION_HW:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: GDI access error has occurred", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_QUERY_FAILURE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: query command failed", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_QUEUEING_FAILURE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: commands cannot be queued", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_VPU_STILL_RUNNING:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: VPU is still running, cannot be flushed/closed now", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_REPORT_NOT_READY:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: report is not ready for Query", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_VLC_BUF_FULL:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: VLC buffer is full", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    case VPU_RET_INVALID_SFS_INSTANCE:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: current instance can't run sub-framesync.", msg_start);
        return BM_VPU_ENC_RETURN_CODE_ERROR;

    default:
        BM_VPU_ERROR_FULL(fn, linenr, funcn, "%s: unknown error 0x%x", msg_start, ret_code);
        return BM_VPU_ENC_RETURN_CODE_ERROR;
    }
}


static BmVpuFrameType convert_frame_type(int vpu_pic_type)
{
    BmVpuFrameType type = BM_VPU_FRAME_TYPE_UNKNOWN;

    switch (vpu_pic_type)
    {
    case 0: type = BM_VPU_FRAME_TYPE_I;   break;
    case 1: type = BM_VPU_FRAME_TYPE_P;   break;
    case 2: type = BM_VPU_FRAME_TYPE_B;   break;
    case 5: type = BM_VPU_FRAME_TYPE_IDR; break;
    default: break;
    }

    return type;
}

/*
 * If a bm_handle_t on this soc already exists, return it directly,
 * otherwise return after creat one (using bm_dev_request).
 * This function is only be called by bmvpu_enc_load(), the bm_handle_t's reference count +1
 * if this function is called.
 */
static void bmvpu_enc_load_bmlib_handle(int soc_idx){
    if (soc_idx > MAX_SOC_NUM)
    {
        BM_VPU_ERROR("soc_idx excess MAX_SOC_NUM!\n");
        exit(0);
    }

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
      BM_VPU_ERROR("Create Bm Handle Failed\n");
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
static void bmvpu_enc_unload_bmlib_handle(int soc_idx){
    if (soc_idx > MAX_SOC_NUM)
    {
      BM_VPU_ERROR("soc_idx excess MAX_SOC_NUM!\n");
      exit(0);
    }

    if (g_bm_handle[soc_idx].bm_handle)
    {
        bm_handle_lock();
        if (g_bm_handle[soc_idx].count <= 1)
        {
            bm_dev_free(g_bm_handle[soc_idx].bm_handle);
            g_bm_handle[soc_idx].count = 0;
            g_bm_handle[soc_idx].bm_handle = 0;
            BM_VPU_DEBUG("Free bm_handle for encode on soc %d \n", soc_idx);
        }
        else
        {
            g_bm_handle[soc_idx].count -= 1;
            BM_VPU_DEBUG("The bm_handle for encode on soc is used by %d users \n", g_bm_handle[soc_idx].count);
        }
        bm_handle_unlock();
    }
    else
        BM_VPU_DEBUG("Bm_handle for encode on soc %d not exist \n", soc_idx);
}

bm_handle_t bmvpu_enc_get_bmlib_handle(int soc_idx)
{
    bm_handle_t handle = 0;
    if (soc_idx > MAX_SOC_NUM)
    {
        BM_VPU_ERROR("soc_idx excess MAX_SOC_NUM!\n");
        exit(0);
    }

    bm_handle_lock();
    if (g_bm_handle[soc_idx].bm_handle)
    {
        handle = g_bm_handle[soc_idx].bm_handle;
        bm_handle_unlock();
        return handle;
    }
    else
    {
        bm_handle_unlock();
        BM_VPU_ERROR("There is not bmlib_handle on soc %d, This function should be called after bmvpu_enc_load()! \n");
        return handle;
    }
}
