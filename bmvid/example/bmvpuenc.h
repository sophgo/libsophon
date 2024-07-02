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
#ifndef __TEST_ENC_H__
#define __TEST_ENC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#ifdef __linux__
#include <pthread.h>
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#endif

#include "bm_vpuenc_interface.h"

enum
{
    RETVAL_OK = 0,
    RETVAL_ERROR = 1,
    RETVAL_EOS = 2
};

static void logging_fn(BmVpuEncLogLevel level,
                       char const *file,
                       int const line,
                       char const *fn,
                       const char *format, ...)
{
    va_list args;

    char const *lvlstr = "";
    switch (level)
    {
    case BMVPU_ENC_LOG_LEVEL_ERROR:   lvlstr = "ERROR";   break;
    case BMVPU_ENC_LOG_LEVEL_WARNING: lvlstr = "WARNING"; break;
    case BMVPU_ENC_LOG_LEVEL_INFO:    lvlstr = "INFO";    break;
    case BMVPU_ENC_LOG_LEVEL_DEBUG:   lvlstr = "DEBUG";   break;
    case BMVPU_ENC_LOG_LEVEL_TRACE:   lvlstr = "TRACE";   break;
    case BMVPU_ENC_LOG_LEVEL_LOG:     lvlstr = "LOG";     break;
    default: break;
    }
#ifdef __linux__
    fprintf(stderr, "[%zx] %s:%d (%s)   %s: ", pthread_self(), file, line, fn, lvlstr);
#endif
#ifdef _WIN32
    fprintf(stderr, "[%zx] %s:%d (%s)   %s: ", GetCurrentThreadId(), file, line, fn, lvlstr);
#endif
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

#endif /* __TEST_ENC_H__ */

