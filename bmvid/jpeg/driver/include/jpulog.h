
#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

enum { NONE=0, ERR, WARN, INFO, TRACE, MAX_LOG_LEVEL };
enum {
    LOG_HAS_DAY_NAME   =    1, /**< Include day name [default: no]         */
    LOG_HAS_YEAR       =    2, /**< Include year digit [no]                */
    LOG_HAS_MONTH      =    4, /**< Include month [no]                     */
    LOG_HAS_DAY_OF_MON =    8, /**< Include day of month [no]              */
    LOG_HAS_TIME       =   16, /**< Include time [yes]                     */
    LOG_HAS_MICRO_SEC  =   32, /**< Include microseconds [yes]             */
    LOG_HAS_FILE       =   64, /**< Include sender in the log [yes]        */
    LOG_HAS_NEWLINE    =  128, /**< Terminate each call with newline [yes] */
    LOG_HAS_CR         =  256, /**< Include carriage return [no]           */
    LOG_HAS_SPACE      =  512, /**< Include two spaces before log [yes]    */
    LOG_HAS_COLOR      = 1024, /**< Colorize logs [yes on win32]           */
    LOG_HAS_LEVEL_TEXT = 2048  /**< Include level text string [no]         */
};
enum {
    TERM_COLOR_R    = 2,    /**< Red            */
    TERM_COLOR_G    = 4,    /**< Green          */
    TERM_COLOR_B    = 1,    /**< Blue.          */
    TERM_COLOR_BRIGHT = 8   /**< Bright mask.   */
};

#define MAX_PRINT_LENGTH 512

#ifdef ANDROID
#include <utils/Log.h>
#undef LOG_NDEBUG
#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "JPUAPI"
#endif

#define JLOG(level, msg, ...) jpu_LogMsg(level, "%s(), %d, "msg"\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define LOG_ENABLE_FILE    jpu_SetLogDecor(GetLogDecor()|LOG_HAS_FILE);

#if defined (__cplusplus)
extern "C" {
#endif

    int jpu_InitLog();
    void jpu_DeInitLog();

    void jpu_SetMaxLogLevel(int level);
    int jpu_GetMaxLogLevel();

    void jpu_SetLogColor(int level, int color);
    int jpu_GetLogColor(int level);

    void jpu_SetLogDecor(int decor);
    int jpu_GetLogDecor();

    void jpu_LogMsg(int level, const char *format, ...);

#if defined(linux) || defined(__linux) || defined(ANDROID)
    int kbhit();
    int getch();
    void init_keyboard();
    void close_keyboard();
#endif

    void timer_start();
    void timer_stop();
    double timer_elapsed_us();
    double timer_elapsed_ms();
    int timer_is_valid();
    double timer_frequency();

#if defined (__cplusplus)
}
#endif

#endif //#ifndef _LOG_H_
