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
#include <stdlib.h>

#include "bmvpuapi.h"
#include "bmvpuapi_internal.h"
#include "bmvpu_logging.h"


static void default_logging_fn(BmVpuLogLevel level, char const *file,
                               int const line, char const *fn,
                               const char *format, ...)
{
    BMVPUAPI_UNUSED_PARAM(level);
    BMVPUAPI_UNUSED_PARAM(file);
    BMVPUAPI_UNUSED_PARAM(line);
    BMVPUAPI_UNUSED_PARAM(fn);
    BMVPUAPI_UNUSED_PARAM(format);
}

BmVpuLogLevel bmvpu_cur_log_level_threshold = BM_VPU_LOG_LEVEL_ERROR;
BmVpuLoggingFunc bmvpu_cur_logging_fn = default_logging_fn;

void bmvpu_set_logging_function(BmVpuLoggingFunc logging_fn)
{
    bmvpu_cur_logging_fn = (logging_fn != NULL) ? logging_fn : default_logging_fn;
}

void bmvpu_set_logging_threshold(BmVpuLogLevel threshold)
{
    bmvpu_cur_log_level_threshold = threshold;
    vpu_set_logging_threshold(threshold);
}

BmVpuLogLevel bmvpu_get_logging_threshold()
{
    return bmvpu_cur_log_level_threshold;
}

