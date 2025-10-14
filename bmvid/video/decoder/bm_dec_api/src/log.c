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
#include <stdarg.h>
#include <stdio.h>

#include "vdi_osal.h"
#include "bm_vpudec_interface.h"


void bmvpu_dec_set_logging_threshold(BmVpuDecLogLevel log_level)
{
    VpuDecLogLevel level = ERR;

    switch (log_level)
    {
    case BMVPU_DEC_LOG_LEVEL_ERROR:
        level = ERR;
        break;
    case BMVPU_DEC_LOG_LEVEL_WARNING:
        level = WARN;
        break;
    case BMVPU_DEC_LOG_LEVEL_INFO:
    case BMVPU_DEC_LOG_LEVEL_DEBUG:
    case BMVPU_DEC_LOG_LEVEL_LOG:
        level = INFO;
        break;
    case BMVPU_DEC_LOG_LEVEL_TRACE:
        level = TRACE;
        break;
    default:
        break;
    }
    if(level >= ERR && level <= MAX_LOG_LEVEL) {
        SetMaxLogLevel(level);
    }

}

void bmvpu_dec_set_logging_function(BmVpuDecLoggingFunc logging_fn)
{
    SetLoggingFunc((VpuDecLoggingFunc)logging_fn);
}

void BMVidSetLogLevel()
{
    char *debug_env = getenv("BMVID_DEBUG");
    int level = ERR;

    if (debug_env) {
        BmVpuDecLogLevel log_level = 0;
        log_level = atoi(debug_env);
        bmvpu_dec_set_logging_threshold(log_level);
    }
}

BmVpuDecLogLevel bmvpu_dec_get_logging_threshold()
{
    return GetLoggingFunc();
}