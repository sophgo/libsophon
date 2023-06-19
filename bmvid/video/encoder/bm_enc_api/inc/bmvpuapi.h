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
#ifndef __BMVPUAPI_H__
#define __BMVPUAPI_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMVPUAPI_VERSION "1.0.0"

#include "bmvpu_types.h"
#include "bmvpu.h"
#include "bmvpu_logging.h"

#include "bmvpuapi_common.h"
#include "bmvpuapi_enc.h"

#ifdef __cplusplus
}
#endif


#endif
