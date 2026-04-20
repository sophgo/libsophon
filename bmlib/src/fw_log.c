#include <android/log.h>
#include <setjmp.h>
#include "bmlib_type.h"

jmp_buf error_stat;

void fw_log(int level, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    __android_log_vprint(ANDROID_LOG_INFO, "BMLIB", fmt, args);
    
    va_end(args);
}

void bm_set_error_handler(void) {
}

void bm_longjmp_error(void) {
    longjmp(error_stat, 1);
}
