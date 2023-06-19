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
#ifndef __BM_VPU_INTERNAL_H__
#define __BM_VPU_INTERNAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef __linux__
#include <pthread.h>
#elif _WIN32
#include <Windows.h>
#endif
#include "bmvpu_types.h"

enum {
    NONE   = 0,
    ERR    = 1,
    WARN   = 2,
    INFO   = 3,
    DEBUG  = 4,
    TRACE  = 5,
    MAX_LOG_LEVEL
};

enum {
    LOG_HAS_DAY_NAME   =    1, /* Include day name [default: no]           */
    LOG_HAS_YEAR       =    2, /* Include year digit [no]              */
    LOG_HAS_MONTH      =    4, /* Include month [no]              */
    LOG_HAS_DAY_OF_MON =    8, /* Include day of month [no]          */
    LOG_HAS_TIME       =   16, /* Include time [yes]              */
    LOG_HAS_MICRO_SEC  =   32, /* Include microseconds [yes]             */
    LOG_HAS_FILE       =   64, /* Include sender in the log [yes]           */
    LOG_HAS_NEWLINE    =  128, /* Terminate each call with newline [yes] */
    LOG_HAS_CR         =  256, /* Include carriage return [no]           */
    LOG_HAS_SPACE      =  512, /* Include two spaces before log [yes]    */
    LOG_HAS_COLOR      = 1024, /* Colorize logs [yes on win32]          */
    LOG_HAS_LEVEL_TEXT = 2048  /* Include level text string [no]          */
};
enum {
    TERM_COLOR_R      = 2,   /* Red          */
    TERM_COLOR_G      = 4,   /* Green        */
    TERM_COLOR_B      = 1,   /* Blue.        */
    TERM_COLOR_BRIGHT = 8    /* Bright mask. */
};

typedef struct {
    uint32_t productId;
    uint32_t fwVersion;
    uint32_t productName;     /* VPU hardware product name  */
    uint32_t productVersion;  /* VPU hardware product version */
    uint32_t customerId;      /* The customer id */
    uint32_t stdDef0;         /* The system configuration information  */
    uint32_t stdDef1;         /* The hardware configuration information  */
    uint32_t confFeature;     /* The supported codec standard */
    uint32_t configDate;      /* The date that the hardware has been configured in YYYYmmdd in digit */
    uint32_t configRevision;  /* The revision number when the hardware has been configured */
    uint32_t configType;      /* The define value used in hardware configuration */
} ProductInfo;

int  vpu_InitLog(void);
void vpu_DeInitLog(void);

void vpu_SetMaxLogLevel(int level);
int  vpu_GetMaxLogLevel(void);

void vpu_SetLogColor(int level, int color);
int  vpu_GetLogColor(int level);

void vpu_SetLogDecor(int decor);
int  vpu_GetLogDecor(void);

#ifdef __linux__
#define VLOG(level, msg, ...) vpu_LogMsg(level, "[%zx] %s:%d (%s)   "msg, pthread_self(), __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#else
#define VLOG(level, msg, ...) vpu_LogMsg(level, "[%zx] %s:%d (%s)   "msg, GetCurrentThreadId(), __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#endif
void vpu_LogMsg(int level, const char *format, ...);

/* The encoder index is 4 */
#define ENC_CORE_IDX 4

int VpuSetClockGate(uint32_t coreIdx, uint32_t on);
void bm_vdi_log(u64 core_idx, int cmd, int step);

/* system time in millisecond. */
uint64_t vpu_gettime(void);

int find_firmware_path(char fw_path[512], const char* path);
int load_firmware(uint8_t** firmware, uint32_t* fwSizeInWord, const char* path);

int vpu_ShowProductInfo(uint32_t coreIdx, ProductInfo *productInfo);

int vpu_InitWithBitcode(uint32_t coreIdx, const uint16_t *bitcode, uint32_t sizeInWord);
int vpu_GetProductInfo(uint32_t coreIdx, ProductInfo *productInfo);
int vpu_GetVersionInfo(int coreIdx, uint32_t *versionInfo, uint32_t *revision, uint32_t *productId);

int vpu_GetProductId(int coreIdx);

#endif /* __BM_VPU_INTERNAL_H__ */

