#include "jpeg_common.h"

void logging_fn(BmJpuLogLevel level, char const *file, int const line, char const *fn, const char *format, ...)
{
    va_list args;

    char const *lvlstr = "";
    switch (level)
    {
        case BM_JPU_LOG_LEVEL_ERROR:   lvlstr = "ERROR";   break;
        case BM_JPU_LOG_LEVEL_WARNING: lvlstr = "WARNING"; break;
        case BM_JPU_LOG_LEVEL_INFO:    lvlstr = "INFO";    break;
        case BM_JPU_LOG_LEVEL_DEBUG:   lvlstr = "DEBUG";   break;
        case BM_JPU_LOG_LEVEL_TRACE:   lvlstr = "TRACE";   break;
        case BM_JPU_LOG_LEVEL_LOG:     lvlstr = "LOG";     break;
        default: break;
    }

    fprintf(stderr, "%s:%d (%s)   %s: ", file, line, fn, lvlstr);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

