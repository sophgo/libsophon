#ifndef __BMLIB_LOG_H__
#define __BMLIB_LOG_H__

/*
 * log functions
 */
#ifdef __linux__
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "pthread.h"
#else
#include <windows.h>
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
void bmlib_log(const char *tag, int level, const char *fmt, ...);
void bmlib_log_default_callback(const char *tag, int level, const char *fmt, va_list args);
#ifdef __cplusplus
}
#endif

#endif

