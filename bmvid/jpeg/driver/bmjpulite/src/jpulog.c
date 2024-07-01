#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#endif

#include "../jpuapi/jpuconfig.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../include/jpulog.h"

#if defined(linux) || defined(__linux) || defined(ANDROID)
#include <time.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>   // for read()

static struct termios initial_settings, new_settings;
static int peek_character = -1;
#endif

static int log_colors[MAX_LOG_LEVEL] = {
    0,
    TERM_COLOR_R|TERM_COLOR_BRIGHT,    // ERR
    TERM_COLOR_R|TERM_COLOR_B|TERM_COLOR_BRIGHT,    //WARN
    TERM_COLOR_R|TERM_COLOR_G|TERM_COLOR_B|TERM_COLOR_BRIGHT,     //INFO
    TERM_COLOR_R|TERM_COLOR_G|TERM_COLOR_B    //TRACE
};

static unsigned log_decor = (LOG_HAS_TIME | LOG_HAS_FILE | LOG_HAS_MICRO_SEC |
                             LOG_HAS_NEWLINE |
                             LOG_HAS_SPACE | LOG_HAS_COLOR);
static int max_log_level = ERR;
static FILE *fpLog  = NULL;

static void term_restore_color();
static void term_set_color(int color);

int jpu_InitLog()
{
#if 0
    fpLog = fopen("c:\\out.log", "w");
    if (fpLog==NULL)
    {
        return -1;
    }
#endif

    return 1;
}

void jpu_DeInitLog()
{
    if (fpLog)
    {
        fclose(fpLog);
        fpLog = NULL;
    }
}

void jpu_SetLogColor(int level, int color)
{
    log_colors[level] = color;
}

int jpu_GetLogColor(int level)
{
    return log_colors[level];
}

void jpu_SetLogDecor(int decor)
{
    log_decor = decor;
}

int jpu_GetLogDecor()
{
    return log_decor;
}

void jpu_SetMaxLogLevel(int level)
{
    max_log_level = level;
}
int jpu_GetMaxLogLevel()
{
    return max_log_level;
}

void jpu_LogMsg(int level, const char *format, ...)
{
    va_list ptr;
    char logBuf[MAX_PRINT_LENGTH] = {0};

    if (level > max_log_level)
        return;

    va_start( ptr, format );
#if defined(WIN32) || defined(__MINGW32__)
    _vsnprintf( logBuf, MAX_PRINT_LENGTH, format, ptr );
#else
    vsnprintf( logBuf, MAX_PRINT_LENGTH, format, ptr );
#endif
    va_end(ptr);

    if (log_decor & LOG_HAS_COLOR)
        term_set_color(log_colors[level]);

#ifdef ANDROID
    if (level == ERR)
    {
#ifdef LOGE
        LOGE("%s", logBuf);
#endif
#ifdef ALOGE
        ALOGE("%s", logBuf);
#endif
    }
    else
    {
#ifdef LOGI
        LOGI("%s", logBuf);
#endif
#ifdef ALOGI
        ALOGI("%s", logBuf);
#endif
    }
#endif
    fputs(logBuf, stdout);

    if ((log_decor & LOG_HAS_FILE) && fpLog)
    {
        fwrite(logBuf, strlen(logBuf), 1,fpLog);
        fflush(fpLog);
    }

    if (log_decor & LOG_HAS_COLOR)
        term_restore_color();
}

static void term_set_color(int color)
{
#ifdef WIN32
    unsigned short attr = 0;
    if (color & TERM_COLOR_R)
        attr |= FOREGROUND_RED;
    if (color & TERM_COLOR_G)
        attr |= FOREGROUND_GREEN;
    if (color & TERM_COLOR_B)
        attr |= FOREGROUND_BLUE;
    if (color & TERM_COLOR_BRIGHT)
        attr |= FOREGROUND_INTENSITY;

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attr);
#else

    /* put bright prefix to ansi_color */
    char ansi_color[12] = "\033[01;3";

    return;

    if (color & TERM_COLOR_BRIGHT)
        color ^= TERM_COLOR_BRIGHT;
    else
        strcpy(ansi_color, "\033[00;3");

    switch (color) {
    case 0:
        /* black color */
        strcat(ansi_color, "0m");
        break;
    case TERM_COLOR_R:
        /* red color */
        strcat(ansi_color, "1m");
        break;
    case TERM_COLOR_G:
        /* green color */
        strcat(ansi_color, "2m");
        break;
    case TERM_COLOR_B:
        /* blue color */
        strcat(ansi_color, "4m");
        break;
    case TERM_COLOR_R | TERM_COLOR_G:
        /* yellow color */
        strcat(ansi_color, "3m");
        break;
    case TERM_COLOR_R | TERM_COLOR_B:
        /* magenta color */
        strcat(ansi_color, "5m");
        break;
    case TERM_COLOR_G | TERM_COLOR_B:
        /* cyan color */
        strcat(ansi_color, "6m");
        break;
    case TERM_COLOR_R | TERM_COLOR_G | TERM_COLOR_B:
        /* white color */
        strcat(ansi_color, "7m");
        break;
    default:
        /* default console color */
        strcpy(ansi_color, "\033[00m");
        break;
    }

    fputs(ansi_color, stdout);
#endif
}

static void term_restore_color(void)
{
    term_set_color(log_colors[4]);
}


static double timer_frequency_;

#if defined(_MSC_VER)
static LARGE_INTEGER initial_;
static LARGE_INTEGER frequency_;
static LARGE_INTEGER counter_;
#endif

#if defined(linux) || defined(__linux) || defined(ANDROID)
static struct timeval tv_start;
static struct timeval tv_end;
#endif


void timer_init()
{
#if defined(linux) || defined(__linux) || defined(ANDROID)

#else
#if defined(_MSC_VER)
    if (QueryPerformanceFrequency(&frequency_))
    {
        //printf("High:%d, Quad:%d, Low:%d\n", frequency_.HighPart, frequency_.QuadPart, frequency_.LowPart);
        timer_frequency_ = (double)((long double)((long)(frequency_.HighPart) >> 32) + frequency_.LowPart);
    }
    else
        printf("QueryPerformanceFrequency returned FAIL\n");
#endif
#endif
}

void timer_start()
{
    timer_init();
#if defined(linux) || defined(__linux) || defined(ANDROID)
    gettimeofday(&tv_start, NULL);
#else
#if defined(_MSC_VER)
    if (timer_frequency_ == 0)
        return;

    QueryPerformanceCounter(&initial_);
#endif
#endif
}

void timer_stop()
{
#if defined(linux) || defined(__linux) || defined(ANDROID)
    gettimeofday(&tv_end, NULL);
#else
#if defined(_MSC_VER)
    if (timer_frequency_ == 0)
        return;

    QueryPerformanceCounter(&counter_);
#endif
#endif
}

double timer_elapsed_ms()
{
    double ms;
    ms = timer_elapsed_us()/1000.0;
    return ms;
}

double timer_elapsed_us()
{
    double elapsed = 0;
#if defined(linux) || defined(__linux) || defined(ANDROID)
    double start_us = 0;
    double end_us = 0;
    end_us = tv_end.tv_sec*1000*1000 + tv_end.tv_usec;
    start_us = tv_start.tv_sec*1000*1000 + tv_start.tv_usec;
    elapsed =  end_us - start_us;
#else
#if defined(_MSC_VER)
    if (timer_frequency_ == 0)
        return 0;

    elapsed = (double)((long double)(counter_.QuadPart - initial_.QuadPart) / (long double)frequency_.QuadPart);
#endif
#endif
    return elapsed;
}

int timer_is_valid()
{
    return timer_frequency_ != 0;
}

double timer_frequency()
{
    return timer_frequency_;
}




#if defined(linux) || defined(__linux) || defined(ANDROID)

void init_keyboard()
{
    tcgetattr(0,&initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
    peek_character = -1;
}

void close_keyboard()
{
    tcsetattr(0, TCSANOW, &initial_settings);
    fflush(stdout);
}

int kbhit()
{
    unsigned char ch;
    int nread;

    if (peek_character != -1) return 1;
    new_settings.c_cc[VMIN]=0;
    tcsetattr(0, TCSANOW, &new_settings);
    nread = read(0,&ch,1);
    new_settings.c_cc[VMIN]=1;
    tcsetattr(0, TCSANOW, &new_settings);
    if(nread == 1)
    {
        peek_character = (int)ch;
        return 1;
    }
    return 0;
}

int getch()
{
    int val;
    char ch;
    if(peek_character != -1)
    {
        val = peek_character;
        peek_character = -1;
        return val;
    }
    read(0,&ch,1);
    return ch;
}
#endif    //#if defined(linux) || defined(__linux) || defined(ANDROID)

