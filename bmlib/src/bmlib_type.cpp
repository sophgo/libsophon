#include "bmlib_type.h"
#include <stdio.h>
#include <stdarg.h>

jmp_buf error_stat;  

void fw_log(int level, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
