#ifndef __JPEG_COMMON_H__
#define __JPEG_COMMON_H__

#include <stdio.h>
#include <stdarg.h>
#include "bmjpuapi.h"

#define RETVAL_OK     0
#define RETVAL_ERROR  1
#define RETVAL_EOS    2

void logging_fn(BmJpuLogLevel level, char const *file, int const line, char const *fn, const char *format, ...);

#endif /* __JPEG_COMMON_H__ */

