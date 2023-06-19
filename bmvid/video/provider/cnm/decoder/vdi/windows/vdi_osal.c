/*
 * Copyright (c) 2018, Chips&Media
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#if 0
#define __USE_GNU
#endif
#include <windows.h>
#include <time.h>

#include "vpuconfig.h"
#include "vdi_osal.h"

static int peek_character = -1;

static int log_colors[MAX_LOG_LEVEL] = {
    0,
    TERM_COLOR_R|TERM_COLOR_G|TERM_COLOR_B|TERM_COLOR_BRIGHT,     //INFO
    TERM_COLOR_R|TERM_COLOR_B|TERM_COLOR_BRIGHT,    //WARN
    TERM_COLOR_R|TERM_COLOR_BRIGHT,    // ERR
    TERM_COLOR_R|TERM_COLOR_G|TERM_COLOR_B    //TRACE
};

static unsigned log_decor = LOG_HAS_TIME | LOG_HAS_FILE | LOG_HAS_MICRO_SEC |
                LOG_HAS_NEWLINE |
                LOG_HAS_SPACE | LOG_HAS_COLOR;
static int max_log_level = ERR;
static FILE *fpLog  = NULL;

#define OSAL_MUTEX_TIMEOUT 10000 //ms

static void term_restore_color(void);
static void term_set_color(int color);
#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
static pthread_mutex_t s_log_mutex;
#endif

int InitLog()
{
    fpLog = osal_fopen("ErrorLog.txt", "w");
#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    pthread_mutex_init(&s_log_mutex, NULL);
#endif
    return 1;
}

void DeInitLog()
{
    if (fpLog)
    {
        osal_fclose(fpLog);
        fpLog = NULL;
    }
#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    pthread_mutex_destroy(&s_log_mutex);
#endif
}

void SetLogColor(int level, int color)
{
    log_colors[level] = color;
}

int GetLogColor(int level)
{
    return log_colors[level];
}

void SetLogDecor(int decor)
{
    log_decor = decor;
}

int GetLogDecor()
{
    return log_decor;
}

void SetMaxLogLevel(int level)
{
    max_log_level = level;
}
int GetMaxLogLevel()
{
    return max_log_level;
}

void LogMsg(int level, const char *format, ...)
{
    va_list ptr;
    char logBuf[MAX_PRINT_LENGTH] = {0};

    if (level > max_log_level)
        return;
#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    pthread_mutex_lock(&s_log_mutex);
#endif

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
        ALOGE("%s", logBuf);
    }
    else
    {
        ALOGI("%s", logBuf);
    }
    fputs(logBuf, stderr);
#else
    fputs(logBuf, stderr);
#endif

    if ((log_decor & LOG_HAS_FILE) && fpLog)
    {
        osal_fwrite(logBuf, strlen(logBuf), 1,fpLog);
        osal_fflush(fpLog);
    }

    if (log_decor & LOG_HAS_COLOR)
        term_restore_color();
#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    pthread_mutex_unlock(&s_log_mutex);
#endif
}

static void term_set_color(int color)
{
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
}

static void term_restore_color(void)
{
    term_set_color(log_colors[4]);
}

static double timer_frequency_;

static struct timeval tv_start;
static struct timeval tv_end;

void timer_init()
{
    u64 sec = 0;
    u64 usec = 0;
}

void timer_start()
{
    timer_init();
}

void timer_stop()
{
    u64 sec = 0;
    u64 usec = 0;
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
    double start_us = 0;
    double end_us = 0;
    end_us = tv_end.tv_sec*1000*1000 + tv_end.tv_usec;
    start_us = tv_start.tv_sec*1000*1000 + tv_start.tv_usec;
    elapsed =  end_us - start_us;
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

void osal_init_keyboard()
{
    //tcgetattr(0,&initial_settings);//
    //new_settings = initial_settings;
    //new_settings.c_lflag &= ~ICANON;
    //new_settings.c_lflag &= ~ECHO;
    ////new_settings.c_lflag &= ~ISIG;
    //new_settings.c_cc[VMIN] = 1;
    //new_settings.c_cc[VTIME] = 0;
    //tcsetattr(0, TCSANOW, &new_settings);
    //peek_character = -1;
}

void osal_close_keyboard()
{
    //tcsetattr(0, TCSANOW, &initial_settings);
}

int osal_kbhit()
{
#if 0
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
#endif
    return 0;
}

int osal_getch()
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

int osal_flush_ch(void)
{
    fflush(stdout);
    return 1;
}

void * osal_memcpy(void * dst, const void * src, int count)
{
#if 1 //defined(CHIP_BM1684)
    return memcpy(dst, src, count);
#else
    int i;
    char *dst_c = (char *)dst;
    char *src_c = (char *)src;
    for(i=0; i<count; i++)
        *dst_c++ = *src_c++;

    return dst;
#endif
}

int osal_memcmp(const void* src, const void* dst, int size)
{
    return memcmp(src, dst, size);
}

void * osal_memset(void *dst, int val, int count)
{
    return memset(dst, val, count);
}

void * osal_malloc(int size)
{
    return malloc(size);
}

void * osal_realloc(void* ptr, int size)
{
    return realloc(ptr, size);
}

void osal_free(void *p)
{
    free(p);
}

int osal_fflush(osal_file_t fp)
{
    return fflush(fp);
}

int osal_feof(osal_file_t fp)
{
    return feof((FILE *)fp);
}

osal_file_t osal_fopen(const char * osal_file_tname, const char * mode)
{
    return fopen(osal_file_tname, mode);
}
size_t osal_fwrite(const void * p, int size, int count, osal_file_t fp)
{
    return fwrite(p, size, count, fp);
}
size_t osal_fread(void *p, int size, int count, osal_file_t fp)
{
    return fread(p, size, count, fp);
}

long osal_ftell(osal_file_t fp)
{
    return ftell(fp);
}

int osal_fseek(osal_file_t fp, long offset, int origin)
{
    return fseek(fp, offset, origin);
}
int osal_fclose(osal_file_t fp)
{
    return fclose(fp);
}

char osal_fgetc(osal_file_t fp)
{
    return fgetc(fp);
}

void osal_srand(int seed)
{
    srand(seed);
}
int osal_rand(void)
{
    return rand();
}
int osal_toupper(int c)
{
    return toupper(c);
}

size_t osal_fputs(const char *s, osal_file_t fp)
{
    return fputs(s, fp);
}

int osal_fscanf(osal_file_t fp, const char * _Format, ...)
{
    int ret;
    extern int vfscanf(FILE*, const char*, va_list);

    va_list arglist;
    va_start(arglist, _Format);

    ret = vfscanf(fp, _Format, arglist);

    va_end(arglist);

    return ret;
}

int osal_fprintf(osal_file_t fp, const char * _Format, ...)
{
    int ret;
    va_list arglist;
    va_start(arglist, _Format);

    ret = vfprintf(fp, _Format, arglist);

    va_end(arglist);

    return ret;
}

size_t osal_strlen(const char * str)
{
    return strlen(str);
}

int osal_strcmp(const char * str1, const char * str2)
{
    return strcmp(str1, str2);
}

int osal_sprintf (char * str, const char * format, ...)
{
    int ret;
    va_list arglist;
    va_start(arglist, format);

    ret = vsprintf(str, format, arglist);

    va_end(arglist);

    return ret;
}

int osal_atoi(const char * str)
{
    return atoi(str);
}

char * osal_strtok(char * str, const char * delimiters)
{
    return strtok(str, delimiters);
}

char * osal_strcpy(char * destination, const char * source)
{
    return strcpy(destination, source);
}

double osal_atof(const char* str)
{
    return atof(str);
}

char * osal_fgets(char * str, int num, osal_file_t stream)
{
    return fgets(str, num, stream);
}

int osal_strcasecmp(const char *s1, const char *s2)
{
    return stricmp(s1, s2);
}

int osal_sscanf(const char * s, const char * format, ...)
{
    int ret;
    extern int vsscanf(const char * s, const char*, va_list);

    va_list arglist;
    va_start(arglist, format);

    ret = vsscanf(s, format, arglist);

    va_end(arglist);

    return ret;
}

int osal_strncmp(const char * str1, const char * str2, size_t num)
{
    return strncmp(str1, str2, num);
}

char * osal_strstr(char * str1, const char * str2)
{
    return strstr(str1, str2);
}

int osal_fputc(int c, osal_file_t fp)
{
    return fputc(c, fp);
}

char * osal_strdup(const char * str)
{
    return strdup(str);
}

void osal_msleep(Uint32 millisecond)
{
    Sleep(millisecond);
}

osal_thread_t osal_thread_create(void(*start_routine)(void*), void*arg)
{

    HANDLE *thread = (HANDLE *)osal_malloc(sizeof(HANDLE));
    HANDLE *handle = NULL;

    if ((*thread = CreateThread(NULL, 0, (void* (*)(void*))start_routine, arg, 0, NULL)) == NULL) {
        osal_free(thread);
        VLOG(ERR, "<%s:%d> Failed to pthread_create.\n", __FUNCTION__, __LINE__);
    }
    else {
        handle = (osal_thread_t)thread;
    }

    return handle;  //lint !e593
}

Int32 osal_thread_join(osal_thread_t **thread, void** retval)
{
    HANDLE   *pthreadHandle;

    if (thread == NULL) {
        VLOG(ERR, "%s:%d invalid thread handle\n", __FUNCTION__, __LINE__);
        return 2;
    }

   pthreadHandle = (HANDLE *)thread;

   if (WaitForSingleObject(*pthreadHandle, INFINITE) != WAIT_OBJECT_0) {
       osal_free(thread);
       VLOG(ERR, "%s:%d Failed to pthread_join\n", __FUNCTION__, __LINE__);
       return 2;
   }
    CloseHandle(*pthreadHandle);
    free(pthreadHandle);
    return 0;
}

Int32 osal_thread_timedjoin(osal_thread_t thread, void** retval, Uint32 second)
{
    Int32           ret;
    HANDLE       pthreadHandle;
    struct timespec absTime;

    if (thread == NULL) {
        VLOG(ERR, "%s:%d invalid thread handle\n", __FUNCTION__, __LINE__);
        return 2;
    }

    pthreadHandle = (HANDLE)thread;

    absTime.tv_sec  = second;
    absTime.tv_nsec = 0;
    if((ret = WaitForSingleObject(pthreadHandle, (absTime.tv_sec * 1000))) != WAIT_OBJECT_0){
        osal_free(thread);
        return 0;
    }else if (ret == WAIT_TIMEOUT) {
        return 1;
    }else {
        return 2;
    }
}

BOOL osal_thread_cancel(osal_thread_t thread)
{
    HANDLE   pthreadHandle;

    if (thread == NULL) {
        VLOG(ERR, "%s:%d invalid thread handle\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    pthreadHandle = *(HANDLE*)thread;

    if(TerminateThread(pthreadHandle, 0) == 0){
        VLOG(ERR, "%s:%d Failed to pthread_cancel.\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    return TRUE;
}

osal_mutex_t *osal_mutex_create(void)
{
    HANDLE* mutex = (HANDLE*)osal_malloc(sizeof(HANDLE));

    if (mutex == NULL) {
        VLOG(ERR, "<%s:%d> failed to allocate memory\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    if ((*mutex = CreateMutex(NULL, FALSE, NULL)) == NULL) {
        osal_free(mutex);
        VLOG(ERR, "<%s:%d> failed to pthread_mutex_init() errno(%d)\n", __FUNCTION__, __LINE__, errno);
        return NULL; //lint !e429
    }

    return (osal_mutex_t *)mutex;
}

void osal_mutex_destroy(osal_mutex_t *mutex)
{
    Int32   ret;

    if (mutex == NULL) {
        VLOG(ERR, "<%s:%d> Invalid mutex handle\n", __FUNCTION__, __LINE__);
        return;
    }

    if ((ret= CloseHandle((HANDLE)(*mutex))) == 0) {
        VLOG(ERR, "<%s:%d> Failed to pthread_mutex_destroy(). ret(%d)\n", __FUNCTION__, __LINE__, ret);
    }

    osal_free(mutex);
}

BOOL osal_mutex_lock(osal_mutex_t *mutex)
{


    if (mutex == NULL) {
        VLOG(ERR, "<%s:%d> Invalid mutex handle\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    if (WaitForSingleObject((HANDLE)(*mutex), INFINITE) != WAIT_OBJECT_0) {
        VLOG(ERR, "<%s:%d> Failed to pthread_mutex_lock().\n", __FUNCTION__, __LINE__);
        return FALSE;   //lint !e454
    }

    return TRUE;    //lint !e454
}

BOOL osal_mutex_unlock(osal_mutex_t *mutex)
{

    if (mutex == NULL) {
        VLOG(ERR, "<%s:%d> Invalid mutex handle\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    if (ReleaseMutex((HANDLE)(*mutex)) == FALSE) { //lint !e455
        VLOG(ERR, "<%s:%d> Failed to pthread_mutex_unlock().\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    return TRUE;
}

Uint64 osal_gettime(void)
{
    u64 sec;
    u64 usec;
    FILETIME s_time;
    GetSystemTimeAsFileTime(&s_time);
    u64 curr = ((u64)s_time.dwLowDateTime) + (((u64)s_time.dwHighDateTime) << 32LL);
    u64 sectoms = curr / 10 / 1000;
    return sectoms;
}


typedef struct {
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cond;
} VpuCondHandle;
osal_cond_t osal_cond_create(void)
{
    VpuCondHandle* handle = (VpuCondHandle*)osal_malloc(sizeof(VpuCondHandle));

    if (handle == NULL) {
        VLOG(ERR, "%s:%d failed to allocate memory\n", __FUNCTION__, __LINE__);
        return NULL;
    }
    InitializeCriticalSection(&(handle->cs));
    InitializeConditionVariable(&(handle->cond));

    return (osal_cond_t)handle;
}

void osal_cond_destroy(osal_cond_t handle)
{
    VpuCondHandle* cond = (VpuCondHandle*)handle;
    if (handle == NULL) {
        VLOG(ERR, "%s:%d Invalid mutex handle\n", __FUNCTION__, __LINE__);
        return;
    }
    DeleteCriticalSection(&(cond->cs));
    //The thread life cycle is valid handle->cond
    osal_free(handle);
    cond = NULL;
}
BOOL osal_cond_lock(osal_cond_t handle)
{
    VpuCondHandle* cond = (VpuCondHandle*)handle;
    if (cond == NULL) {
        VLOG(ERR, "<%s:%d> Invalid mutex handle\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    EnterCriticalSection(&(cond->cs));

    return TRUE;
}
BOOL osal_cond_unlock(osal_cond_t handle)
{
    VpuCondHandle* cond = (VpuCondHandle*)handle;

    if (cond == NULL) {
        VLOG(ERR, "<%s:%d> Invalid mutex handle\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    LeaveCriticalSection(&(cond->cs));
    return TRUE;
}

BOOL osal_cond_wait(osal_cond_t handle)
{
    VpuCondHandle* cond = (VpuCondHandle*)handle;

    if (cond == NULL) {
        VLOG(ERR, "%s:%d Invalid cond handle\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    SleepConditionVariableCS(&(cond->cond),&(cond->cs),INFINITE);

    return TRUE;
}

BOOL osal_cond_signal(osal_cond_t handle)
{
    VpuCondHandle* cond = (VpuCondHandle*)handle;

    if (cond == NULL) {
        VLOG(ERR, "%s:%d Invalid cond handle\n", __FUNCTION__, __LINE__);
        return FALSE;
    }

    WakeAllConditionVariable(&cond->cond);
    return TRUE;
}


#endif    //#ifdef _WIN32

