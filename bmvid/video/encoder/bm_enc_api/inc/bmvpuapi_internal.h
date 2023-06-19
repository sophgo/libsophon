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

#include "bmvpuapi.h"


#define BMVPUAPI_UNUSED_PARAM(x) ((void)(x))

#define BM_VPU_ALIGN_VAL_TO(LENGTH, ALIGN_SIZE)   ( (((LENGTH) + (ALIGN_SIZE) - 1) / (ALIGN_SIZE)) * (ALIGN_SIZE) )
#define BM_VPU_ALIGN_PTR_TO(POINTER, ALIGN_SIZE)  ( ((uintptr_t)(((uint8_t*)(POINTER)) + (ALIGN_SIZE) - 1) / (ALIGN_SIZE)) * (ALIGN_SIZE) )

/***********************************************/
/* Log macros and functions                    */
/***********************************************/

#define BM_VPU_ERROR_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bmvpu_cur_log_level_threshold >= BM_VPU_LOG_LEVEL_ERROR)   { bmvpu_cur_logging_fn(BM_VPU_LOG_LEVEL_ERROR,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_WARNING_FULL(FILE_, LINE_, FUNCTION_, ...) do { if (bmvpu_cur_log_level_threshold >= BM_VPU_LOG_LEVEL_WARNING) { bmvpu_cur_logging_fn(BM_VPU_LOG_LEVEL_WARNING, FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_INFO_FULL(FILE_, LINE_, FUNCTION_, ...)    do { if (bmvpu_cur_log_level_threshold >= BM_VPU_LOG_LEVEL_INFO)    { bmvpu_cur_logging_fn(BM_VPU_LOG_LEVEL_INFO,    FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_DEBUG_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bmvpu_cur_log_level_threshold >= BM_VPU_LOG_LEVEL_DEBUG)   { bmvpu_cur_logging_fn(BM_VPU_LOG_LEVEL_DEBUG,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_LOG_FULL(FILE_, LINE_, FUNCTION_, ...)     do { if (bmvpu_cur_log_level_threshold >= BM_VPU_LOG_LEVEL_LOG)     { bmvpu_cur_logging_fn(BM_VPU_LOG_LEVEL_LOG,     FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_VPU_TRACE_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bmvpu_cur_log_level_threshold >= BM_VPU_LOG_LEVEL_TRACE)   { bmvpu_cur_logging_fn(BM_VPU_LOG_LEVEL_TRACE,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)


#define BM_VPU_ERROR(...)    BM_VPU_ERROR_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_WARNING(...)  BM_VPU_WARNING_FULL(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_INFO(...)     BM_VPU_INFO_FULL   (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_DEBUG(...)    BM_VPU_DEBUG_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_LOG(...)      BM_VPU_LOG_FULL    (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_VPU_TRACE(...)    BM_VPU_TRACE_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)


extern BmVpuLogLevel bmvpu_cur_log_level_threshold;
extern BmVpuLoggingFunc bmvpu_cur_logging_fn;



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

int bmvpu_load(void);
int bmvpu_unload(void);

#endif /* _BM_VPU_INTERNAL_H_ */

