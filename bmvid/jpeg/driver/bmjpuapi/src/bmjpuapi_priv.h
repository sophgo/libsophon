/* bmjpuapi internals
 * Copyright (C) 2018 Solan Shang
 * Copyright (C) 2014 Carlos Rafael Giani
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */


#ifndef BMJPUAPI_PRIV_H
#define BMJPUAPI_PRIV_H

#include "bm_jpeg_internal.h"

#ifdef __cplusplus
extern "C" {
#endif


#define BMJPUAPI_UNUSED_PARAM(x) ((void)(x))

#define BM_JPU_ALIGN_VAL_TO(LENGTH, ALIGN_SIZE)   ( (((LENGTH) + (ALIGN_SIZE) - 1) / (ALIGN_SIZE)) * (ALIGN_SIZE) )
#define BM_JPU_ALIGN_PTR_TO(POINTER, ALIGN_SIZE)  ( ((uintptr_t)(((uint8_t*)(POINTER)) + (ALIGN_SIZE) - 1) / (ALIGN_SIZE)) * (ALIGN_SIZE) )


#define BM_JPU_ALLOC(SIZE) bm_jpu_cur_heap_alloc_fn((SIZE), bm_jpu_cur_heap_alloc_context, __FILE__, __LINE__, __func__)
#define BM_JPU_FREE(PTR, SIZE) bm_jpu_cur_heap_free_fn((PTR), (SIZE), bm_jpu_cur_heap_alloc_context, __FILE__, __LINE__, __func__)

extern void *bm_jpu_cur_heap_alloc_context;
extern BmJpuHeapAllocFunc bm_jpu_cur_heap_alloc_fn;
extern BmJpuHeapFreeFunc bm_jpu_cur_heap_free_fn;


#define BM_JPU_ERROR_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bm_jpu_cur_log_level_threshold >= BM_JPU_LOG_LEVEL_ERROR)   { bm_jpu_cur_logging_fn(BM_JPU_LOG_LEVEL_ERROR,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_JPU_WARNING_FULL(FILE_, LINE_, FUNCTION_, ...) do { if (bm_jpu_cur_log_level_threshold >= BM_JPU_LOG_LEVEL_WARNING) { bm_jpu_cur_logging_fn(BM_JPU_LOG_LEVEL_WARNING, FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_JPU_INFO_FULL(FILE_, LINE_, FUNCTION_, ...)    do { if (bm_jpu_cur_log_level_threshold >= BM_JPU_LOG_LEVEL_INFO)    { bm_jpu_cur_logging_fn(BM_JPU_LOG_LEVEL_INFO,    FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_JPU_DEBUG_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bm_jpu_cur_log_level_threshold >= BM_JPU_LOG_LEVEL_DEBUG)   { bm_jpu_cur_logging_fn(BM_JPU_LOG_LEVEL_DEBUG,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_JPU_LOG_FULL(FILE_, LINE_, FUNCTION_, ...)     do { if (bm_jpu_cur_log_level_threshold >= BM_JPU_LOG_LEVEL_LOG)     { bm_jpu_cur_logging_fn(BM_JPU_LOG_LEVEL_LOG,     FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)
#define BM_JPU_TRACE_FULL(FILE_, LINE_, FUNCTION_, ...)   do { if (bm_jpu_cur_log_level_threshold >= BM_JPU_LOG_LEVEL_TRACE)   { bm_jpu_cur_logging_fn(BM_JPU_LOG_LEVEL_TRACE,   FILE_, LINE_, FUNCTION_, __VA_ARGS__); } } while(0)


#define BM_JPU_ERROR(...)    BM_JPU_ERROR_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_JPU_WARNING(...)  BM_JPU_WARNING_FULL(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_JPU_INFO(...)     BM_JPU_INFO_FULL   (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_JPU_DEBUG(...)    BM_JPU_DEBUG_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_JPU_LOG(...)      BM_JPU_LOG_FULL    (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BM_JPU_TRACE(...)    BM_JPU_TRACE_FULL  (__FILE__, __LINE__, __func__, __VA_ARGS__)


extern BmJpuLogLevel bm_jpu_cur_log_level_threshold;
extern BmJpuLoggingFunc bm_jpu_cur_logging_fn;


#ifdef __cplusplus
}
#endif


#endif
