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



#ifndef _BM_VPU_INTERNAL_H_
#define _BM_VPU_INTERNAL_H_

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "bmvpu.h"

#include "bm_vpuenc_interface.h"


#define BMVPUAPI_UNUSED_PARAM(x) ((void)(x))

#define BM_VPU_ALIGN_VAL_TO(LENGTH, ALIGN_SIZE)   ( (((LENGTH) + (ALIGN_SIZE) - 1) / (ALIGN_SIZE)) * (ALIGN_SIZE) )
#define BM_VPU_ALIGN_PTR_TO(POINTER, ALIGN_SIZE)  ( ((uintptr_t)(((uint8_t*)(POINTER)) + (ALIGN_SIZE) - 1) / (ALIGN_SIZE)) * (ALIGN_SIZE) )

/***********************************************/
/* Log macros and functions                    */
/***********************************************/

#define BM_VPU_ERROR_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bmvpu_cur_log_level_threshold >= BMVPU_ENC_LOG_LEVEL_ERROR)   { bmvpu_cur_logging_fn(BMVPU_ENC_LOG_LEVEL_ERROR,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_WARNING_FULL(FILE_, LINE_, FUNCTION_, ...) do { if (bmvpu_cur_log_level_threshold >= BMVPU_ENC_LOG_LEVEL_WARNING) { bmvpu_cur_logging_fn(BMVPU_ENC_LOG_LEVEL_WARNING, FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_INFO_FULL(FILE_, LINE_, FUNCTION_, ...)    do { if (bmvpu_cur_log_level_threshold >= BMVPU_ENC_LOG_LEVEL_INFO)    { bmvpu_cur_logging_fn(BMVPU_ENC_LOG_LEVEL_INFO,    FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_DEBUG_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bmvpu_cur_log_level_threshold >= BMVPU_ENC_LOG_LEVEL_DEBUG)   { bmvpu_cur_logging_fn(BMVPU_ENC_LOG_LEVEL_DEBUG,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_LOG_FULL(FILE_, LINE_, FUNCTION_, ...)     do { if (bmvpu_cur_log_level_threshold >= BMVPU_ENC_LOG_LEVEL_LOG)     { bmvpu_cur_logging_fn(BMVPU_ENC_LOG_LEVEL_LOG,     FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_TRACE_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bmvpu_cur_log_level_threshold >= BMVPU_ENC_LOG_LEVEL_TRACE)   { bmvpu_cur_logging_fn(BMVPU_ENC_LOG_LEVEL_TRACE,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)


#define BM_VPU_ERROR(...)    BM_VPU_ERROR_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_WARNING(...)  BM_VPU_WARNING_FULL(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_INFO(...)     BM_VPU_INFO_FULL   (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_DEBUG(...)    BM_VPU_DEBUG_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_LOG(...)      BM_VPU_LOG_FULL    (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_TRACE(...)    BM_VPU_TRACE_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)


extern BmVpuEncLogLevel bmvpu_cur_log_level_threshold;
extern BmVpuEncLoggingFunc bmvpu_cur_logging_fn;



/***********************************************/
/* Common structures and functions             */
/***********************************************/


#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef BOOL
#define BOOL int
#endif


/* This catches fringe cases where somebody passes a
 * non-null value as TRUE that is not the same as TRUE */
#define TO_BOOL(X) ((X) ? TRUE : FALSE)

#define FRAME_ALIGN 16

#define VPU_MEMORY_ALIGNMENT         0x8

/* milliseconds to wait for frame completion */
#if defined(PALLADIUM_SIM)
# define VPU_WAIT_TIMEOUT            (1000*1000)
#else
# define VPU_WAIT_TIMEOUT            1000
#endif

#define VPU_MAX_TIMEOUT_COUNTS       40   /* how many timeouts are allowed in series */
#define VPU_MAX_SRC_BUFFER_NUM       32

typedef struct
{
    bm_pa_t src_fb_addr;
    bm_pa_t custom_map_addr;
    int src_idx;
} BmVpuFrameAttr;

typedef struct
{
    BmVpuFramebuffer* rec_fb_list;
    BmVpuEncDMABuffer** rec_fb_dmabuffers;
    int num_rec_fb;

    BmVpuEncBufferAllocFunc buffer_alloc_func;
    BmVpuEncBufferFreeFunc buffer_free_func;
    void *buffer_context;
} BmVpuEncoderCtx;


typedef enum
{
    /* U and V in two separate planes */
    BM_VPU_ENC_CHROMA_NO_INTERLEAVE = 0,
    /* U and V in one plane, e.g. UVUV */
    BM_VPU_ENC_CHROMA_INTERLEAVE_CBCR = 1,
    /* U and V in one plane, e.g. VUVU(not support). */
    BM_VPU_ENC_CHROMA_INTERLEAVE_CRCB = 2,
} BmVpuEncChromaFormat;


int bmvpu_load(void);
int bmvpu_unload(void);

/**
 * Calculate various sizes out of the given width & height and color format.
 * The results are stored in "fb_info".
 * The given frame width and height will be aligned if they aren't already,
 * and the aligned value will be stored in fb_info.
 * Width & height must be nonzero.
 * The fb_info pointer must also be non-NULL.
 * framebuffer_alignment is an alignment value for the sizes of the Y/U/V planes.
 * 0 or 1 mean no alignment.
 * chroma_interleave is set to 1 if a shared CbCr chroma plane is to be used,
 * 0 if Cb and Cr shall use separate planes.
 */
DECL_EXPORT int bmvpu_calc_framebuffer_sizes(int mapType, BmVpuEncPixFormat color_format,
                                  int frame_width, int frame_height,
                                  BmVpuFbInfo *fb_info);
#endif /* _BM_VPU_INTERNAL_H_ */

