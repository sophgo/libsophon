
#include <stdio.h>
#include <stdarg.h>
#include "jpu_logging.h"
#include "jpulog.h"

jpu_log_level_t jpu_log_level_threshold = JPU_LOG_ERROR;

void jpu_set_logging_threshold(jpu_log_level_t threshold)
{
    jpu_log_level_threshold = threshold;
    int level = ERR;

    switch (threshold) 
    {
    case JPU_LOG_ERROR:
        level = ERR;
        break;
    case JPU_LOG_WARNING:
        level = WARN;
        break;
    case JPU_LOG_INFO:
    case JPU_LOG_DEBUG:
        level = INFO;
        break;
    case JPU_LOG_LOG:
    case JPU_LOG_TRACE:
        level = TRACE;
        break;
    default:
        level = ERR;
        break;
    }

    jpu_SetMaxLogLevel(level);
}

void jpu_logging(char const *lvlstr, char const *file, int const line, char const *fn, const char *format, ...)
{
    va_list args;

    printf("%s:%d (%s)   %s: ", file, line, fn, lvlstr);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

