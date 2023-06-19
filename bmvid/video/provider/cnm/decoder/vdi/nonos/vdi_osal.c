//------------------------------------------------------------------------------
// File: vdi_osal.c
//
// Copyright (c) 2006, Chips & Media.  All rights reserved.
//------------------------------------------------------------------------------
//#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WIN32) || defined(__MINGW32__)
//#elif defined(linux) || defined(__linux) || defined(ANDROID)
//#else

#include <string.h>
#include <stdarg.h>
#include <malloc.h>

#include "vpuconfig.h"
#include "vputypes.h"
#include "vdi_osal.h"

#if defined(WAVE512_FVP) || defined(CODA960_FVP)
#define FILE_BUFFER_BASE 0xE0000000
#define LOG_MSG_BUF_BASE 0xD0000000
#elif defined(WAVE512_FPGA) || defined(CODA960_FPGA)
#if defined(CHIP_BM1684)
#define FILE_BUFFER_BASE 0x4bbe00000
#define LOG_MSG_BUF_BASE 0x4bbd00000
#else
#define FILE_BUFFER_BASE 0x150600000    //0x150600000
#define LOG_MSG_BUF_BASE 0x150500000    //0x150500000
#endif
#elif defined(WAVE512_PALLADIUM) || defined(CODA960_PALLADIUM)
#if defined(CHIP_BM1684)
#define FILE_BUFFER_BASE 0x4B8600000
#define LOG_MSG_BUF_BASE 0x4B8500000
#else
#define FILE_BUFFER_BASE 0x150600000
#define LOG_MSG_BUF_BASE 0x150500000
#endif
#endif

#define FILE_BUFFER_SIZE 1024*1024*16*MAX_FD_NUM
#define CMP_FILE_BUFFER_SIZE  0xED4E000 // for 4k 0x17bb000    // YUV file

#define STORE_FILE_BUFFER_BASE (FILE_BUFFER_BASE + CMP_FILE_BUFFER_SIZE + FILE_BUFFER_SIZE / MAX_FD_NUM * (MAX_FD_NUM - 2)) // 0x132DBB000
#define STORE_FILE_BUFFER_SIZE CMP_FILE_BUFFER_SIZE
//#define CMP_FILE_BUFFER_SIZE  0x640        // MD5 file

#define LOG_MSG_BUF_SIZE 0x100000
char *LOG_MSG_BUF = (char *)LOG_MSG_BUF_BASE;

#define MALLOC_BLOCK_SIZE_16K   (16*1024)
#define MALLOC_BLOCK_SIZE_1M    (1*1024*1024)
#define MALLOC_BLOCK_SIZE_4M    (4*1024*1024)
#if defined(CHIP_BM1684)
#define MALLOC_BLOCK_SIZE_16M   (16*1024*1024)
#else
#define MALLOC_BLOCK_SIZE_16M   (32*1024*1024)
#endif

#define MAX_MALLOC_BLOCK_NUM_16K   96
#define MAX_MALLOC_BLOCK_NUM_1M    16
#define MAX_MALLOC_BLOCK_NUM_4M    16
#define MAX_MALLOC_BLOCK_NUM_16M   16

//unsigned char osal_malloc_heap[MAX_MALLOC_BLOCK_NUM][MAX_MALLOC_BLOCK_SIZE];   // allocate 64 4M-block for malloc
//unsigned char osal_malloc_used[MAX_MALLOC_BLOCK_NUM] = {0};
unsigned char osal_malloc_heap_16k[MAX_MALLOC_BLOCK_NUM_16K * MALLOC_BLOCK_SIZE_16K];
unsigned char osal_malloc_16k_used[MAX_MALLOC_BLOCK_NUM_16K] = {0};
unsigned char osal_malloc_heap_1m[MAX_MALLOC_BLOCK_NUM_1M * MALLOC_BLOCK_SIZE_1M];
unsigned char osal_malloc_1m_used[MAX_MALLOC_BLOCK_NUM_1M] = {0};
unsigned char osal_malloc_heap_4m[MAX_MALLOC_BLOCK_NUM_4M * MALLOC_BLOCK_SIZE_4M];
unsigned char osal_malloc_4m_used[MAX_MALLOC_BLOCK_NUM_4M] = {0};
unsigned char osal_malloc_heap_16m[MAX_MALLOC_BLOCK_NUM_16M * MALLOC_BLOCK_SIZE_16M];
unsigned char osal_malloc_16m_used[MAX_MALLOC_BLOCK_NUM_16M] = {0};

#define MAX_MALLOC_BLOCK_SIZE   MALLOC_BLOCK_SIZE_16M
#define MAX_MALLOC_BLOCK_NUM    (MAX_MALLOC_BLOCK_NUM_16K + MAX_MALLOC_BLOCK_NUM_1M + MAX_MALLOC_BLOCK_NUM_4M + MAX_MALLOC_BLOCK_NUM_16M)


typedef struct fileio_buf_t {
    char *_ptr;
    int   _cnt;
    char *_base;
    int   _flag;
    int   _file;
    int   _charbuf;
    int   _bufsiz;
    char *_tmpfname;
} fileio_buf_t;

#define MAX_FD_NUM  4        // 0-bitstream file 1-yuv cmp file 2-
static fileio_buf_t s_file[MAX_FD_NUM] = {0};

int max_log_level = ERR;
int InitLog()
{
    return 1;
}

void DeInitLog()
{

}

void SetMaxLogLevel(int level)
{
    max_log_level = level;
}
int GetMaxLogLevel(void)
{
    return max_log_level;
}

void SetLogColor(int level, int color)
{
}
int GetLogColor(int level)
{
    return -1;
}

void SetLogDecor(int decor)
{
}
int GetLogDecor(void)
{
    return -1;
}

void myprintf(char *MsgBuf)
{
    int size = strlen(MsgBuf);

    if (size == 0) return;

    if (LOG_MSG_BUF + size  >= (char *)(LOG_MSG_BUF_BASE + LOG_MSG_BUF_SIZE))
        return;

    osal_memcpy(LOG_MSG_BUF, MsgBuf, size);

    LOG_MSG_BUF += size;

    return;
}


void LogMsg(int level, const char *format, ...)
{
    va_list ptr;
    va_start( ptr, format );
    char logBuf[MAX_PRINT_LENGTH] = {0};

    if (level > max_log_level) return;

    vsnprintf( logBuf, MAX_PRINT_LENGTH, format, ptr );

    va_end(ptr);

#if defined(CODA960_FPGA) || defined(WAVE512_FPGA) || defined(CODA960_PALLADIUM) || defined(WAVE512_PALLADIUM)
    printf(logBuf);
#elif defined(CODA960_FVP) || defined(WAVE512_FVP)
    myprintf(logBuf);
#endif


}
#if 0
static double timer_frequency_;

void timer_init()
{

}

void timer_start()
{
    timer_init();
}

void timer_stop()
{

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
#endif

void osal_init_keyboard()
{

}

void osal_close_keyboard()
{

}

void * osal_memcpy(void * dst, const void * src, int count)
{
    void *p = memcpy(dst, src, count);

    flush_dcache_range(dst, count);

    return p;
}

void * osal_memset(void *dst, int val, int count)
{
#if (defined(WAVE512_PALLADIUM) || defined(CODA960_PALLADIUM)) && !defined(USE_MEMSET)
    return dst;     // palladium specific
#else
    void *p = memset(dst, val, count);
    flush_dcache_range(dst, count);

    return p;
#endif
}

int osal_memcmp(const void* src, const void* dst, int size)
{
    return memcmp(src, dst, size);
}

void * osal_malloc(int size)
{
    int i;
    int num, max_size;
    unsigned char* pUsed;
    unsigned char* pHeap;

    if ((size > MAX_MALLOC_BLOCK_SIZE) || (size == 0))
        return NULL;

    if (size <= MALLOC_BLOCK_SIZE_16K ){
        num = MAX_MALLOC_BLOCK_NUM_16K;
        pUsed = osal_malloc_16k_used;
        pHeap = osal_malloc_heap_16k;
        max_size = MALLOC_BLOCK_SIZE_16K;
    }
    else if(size <= MALLOC_BLOCK_SIZE_1M ){
        num = MAX_MALLOC_BLOCK_NUM_1M;
        pUsed = osal_malloc_1m_used;
        pHeap = osal_malloc_heap_1m;
        max_size = MALLOC_BLOCK_SIZE_1M;
    }
    else if(size <= MALLOC_BLOCK_SIZE_4M ){
        num = MAX_MALLOC_BLOCK_NUM_4M;
        pUsed = osal_malloc_4m_used;
        pHeap = osal_malloc_heap_4m;
        max_size = MALLOC_BLOCK_SIZE_4M;
    }
    else {
        num = MAX_MALLOC_BLOCK_NUM_16M;
        pUsed = osal_malloc_16m_used;
        pHeap = osal_malloc_heap_16m;
        max_size = MALLOC_BLOCK_SIZE_16M;
    }

    for (i = 0; i < num; i++)
        if (pUsed[i] == 0)
            break;

    if (i < num)
    {
        pUsed[i] = 1;
        return (void *)(pHeap + max_size*i);
    }

    return NULL;
}

void * osal_realloc(void* ptr, int size)
{
    int max_size;

    if (ptr == NULL)
        return osal_malloc(size);

    if ((size == 0) || (size > MAX_MALLOC_BLOCK_SIZE))
    {
        osal_free(ptr);
        return NULL;
    }

    if ((ptr >= (void *)osal_malloc_heap_16k) && \
         (ptr < (void *)&osal_malloc_heap_16k[MAX_MALLOC_BLOCK_NUM_16K*MALLOC_BLOCK_SIZE_16K]))
        max_size = MALLOC_BLOCK_SIZE_16K;
    else if ((ptr >= (void *)osal_malloc_heap_1m) && \
          (ptr < (void *)&osal_malloc_heap_1m[MAX_MALLOC_BLOCK_NUM_1M*MALLOC_BLOCK_SIZE_1M]))
        max_size = MALLOC_BLOCK_SIZE_1M;
    else if ((ptr >= (void *)osal_malloc_heap_4m) && \
             (ptr < (void *)&osal_malloc_heap_4m[MAX_MALLOC_BLOCK_NUM_4M*MALLOC_BLOCK_SIZE_4M]))
        max_size = MALLOC_BLOCK_SIZE_4M;
    else if ((ptr >= (void *)osal_malloc_heap_16m) && \
             (ptr < (void *)&osal_malloc_heap_1m[MAX_MALLOC_BLOCK_NUM_16M*MALLOC_BLOCK_SIZE_16M]))
        max_size = MALLOC_BLOCK_SIZE_16M;

    if (size < max_size)
        return ptr;
    else if (size <= MAX_MALLOC_BLOCK_SIZE)
    {
        osal_free(ptr);
        return osal_malloc(size);
    }

    return NULL;
}

void osal_free(void *p)
{
    //free(p);

    int i;
    int num, max_size;
    unsigned char* pUsed;
    unsigned char* pHeap;

     if ((p >= (void *)osal_malloc_heap_16k) && \
         (p < (void *)&osal_malloc_heap_16k[MAX_MALLOC_BLOCK_NUM_16K*MALLOC_BLOCK_SIZE_16K])){
        num = MAX_MALLOC_BLOCK_NUM_16K;
        pUsed = osal_malloc_16k_used;
        pHeap = osal_malloc_heap_16k;
        max_size = MALLOC_BLOCK_SIZE_16K;
     }
    else if ((p >= (void *)osal_malloc_heap_1m) && \
             (p < (void *)&osal_malloc_heap_1m[MAX_MALLOC_BLOCK_NUM_1M*MALLOC_BLOCK_SIZE_1M])){
        num = MAX_MALLOC_BLOCK_NUM_1M;
        pUsed = osal_malloc_1m_used;
        pHeap = osal_malloc_heap_1m;
        max_size = MALLOC_BLOCK_SIZE_1M;
     }
    else if ((p >= (void *)osal_malloc_heap_4m) &&  \
             (p < (void *)&osal_malloc_heap_4m[MAX_MALLOC_BLOCK_NUM_4M*MALLOC_BLOCK_SIZE_4M])){
        num = MAX_MALLOC_BLOCK_NUM_4M;
        pUsed = osal_malloc_4m_used;
        pHeap = osal_malloc_heap_4m;
        max_size = MALLOC_BLOCK_SIZE_4M;
     }
    else if ((p >= (void *)osal_malloc_heap_16m) && \
             (p < (void *)&osal_malloc_heap_1m[MAX_MALLOC_BLOCK_NUM_16M*MALLOC_BLOCK_SIZE_16M])){
        num = MAX_MALLOC_BLOCK_NUM_16M;
        pUsed = osal_malloc_16m_used;
        pHeap = osal_malloc_heap_16m;
        max_size = MALLOC_BLOCK_SIZE_16M;
     }
     else
        return;

    for (i = 0; i < num; i++)
        if ((void *)(pHeap + i*max_size) == p)
            break;
    if (i < num)
        pUsed[i] = 0;

    return;
}

int osal_fflush(osal_file_t fp)
{
    return 1;
}

int osal_feof(osal_file_t fp)
{
    fileio_buf_t * p_fp = (fileio_buf_t *)fp;

    if ((uint64_t)p_fp->_ptr >= p_fp->_bufsiz)
        return 1;
    else
        return 0;
}

osal_file_t osal_fopen(const char * osal_file_name, const char * mode)
{
    int i;

    for (i = 0; i < MAX_FD_NUM; i++)
        if (s_file[i]._bufsiz == 0)            // not used
            break;

    if (!osal_memcmp(mode, "wb", 2))
    {
        s_file[MAX_FD_NUM - 1]._bufsiz = STORE_FILE_BUFFER_SIZE;
        s_file[MAX_FD_NUM - 1]._base = (char *)STORE_FILE_BUFFER_BASE;
        s_file[MAX_FD_NUM - 1]._ptr = (char *)0;

        return &s_file[MAX_FD_NUM - 1];
    }

    if (i == MAX_FD_NUM)
        return NULL;

    if (i != 1)        // 1 - cmp file
        s_file[i]._bufsiz = FILE_BUFFER_SIZE / MAX_FD_NUM;
    else
        s_file[i]._bufsiz = CMP_FILE_BUFFER_SIZE;        // 256M for YUV compare file
    if (i == 0) {
        s_file[i]._base = (char *)FILE_BUFFER_BASE;
    #ifdef CHIP_BM1684
        s_file[i]._bufsiz = *(int *)0x4F0000080UL; //get the real file size in 1684;
    #endif
    }
    else
        s_file[i]._base = s_file[i-1]._base + s_file[i-1]._bufsiz;
    s_file[i]._ptr = (char *)0;

    // to invalid cached data
    //inv_dcache_range(s_file[i]._base, s_file[i]._bufsiz);

    return &s_file[i];
}
size_t osal_fwrite(const void * p, int size, int count, osal_file_t fp)
{
    long addr;
    long real_size;
    fileio_buf_t * p_fp = (fileio_buf_t *)fp;

    if (p_fp == NULL) return 0;
    if ((unsigned long)(size * count + p_fp->_ptr) > p_fp->_bufsiz)
        real_size = p_fp->_bufsiz - (unsigned long)p_fp->_ptr;
    else
        real_size = size * count;

    addr = (long)((long)p_fp->_base+(long)p_fp->_ptr);
    osal_memcpy((void *)addr, (void *)p, real_size);
    p_fp->_ptr += real_size;

    return real_size;
}
size_t osal_fread(void *p, int size, int count, osal_file_t fp)
{
    long addr;
    long real_size;
    fileio_buf_t * p_fp = (fileio_buf_t *)fp;

    if (p_fp == NULL) return 0;
    if ((unsigned long)(size * count + p_fp->_ptr) > p_fp->_bufsiz)
    {
        real_size = p_fp->_bufsiz - (unsigned long)p_fp->_ptr;
    }
    else
        real_size = size * count;

    addr = (long)((long)p_fp->_base+(long)p_fp->_ptr);
#if defined(WAVE512_PALLADIUM) || defined(CODA960_PALLADIUM)
    UNREFERENCED_PARAMETER(addr);
#else
    osal_memcpy((void *)p, (void *)addr, real_size);
#endif
    p_fp->_ptr += real_size;
    if ((unsigned long)p_fp->_ptr == p_fp->_bufsiz)
        p_fp->_ptr = (char *)0;

    //printf("p_fp: _ptr = 0x%016llx _base = 0x%016llx _bufsiz = 0x%08x\n", (uint64_t)p_fp->_ptr, (uint64_t)p_fp->_base, p_fp->_bufsiz);

    return real_size;
}

char osal_fgetc(osal_file_t fp)
{
    char *ptr;
    fileio_buf_t * p_fp = (fileio_buf_t *)fp;

    if (p_fp == NULL) return -1;
    if ((unsigned long)p_fp->_ptr + sizeof(char) == p_fp->_bufsiz) return -1;

    ptr =  p_fp->_base+(unsigned long)p_fp->_ptr;
    p_fp->_ptr++;

    return *ptr;
}

size_t osal_fputs(const char *s, osal_file_t fp)
{
    return osal_fwrite(s, sizeof(char), strlen(s), fp);
}


long osal_ftell(osal_file_t fp)
{
    fileio_buf_t * p_fp = (fileio_buf_t *)fp;
    return p_fp->_bufsiz;
}

int osal_fseek(osal_file_t fp, long offset, int origin)
{
    uint8_t *curr_p;
    fileio_buf_t * p_fp = (fileio_buf_t *)fp;

    if (fp == NULL) return -1;

    switch(origin)
    {
    case SEEK_CUR:
        curr_p = (uint8_t *)p_fp->_ptr;
        break;
    case SEEK_END:
        curr_p = (uint8_t *)(uint64_t)p_fp->_bufsiz;
        break;
    case SEEK_SET:
        curr_p = (uint8_t *)0;
        break;
    default:
        break;
    }

    p_fp->_ptr = (char *)curr_p + offset;
    if (p_fp->_ptr > p_fp->_bufsiz)
        p_fp->_ptr = p_fp->_bufsiz;

    return 0;
}
int osal_fclose(osal_file_t fp)
{
    fileio_buf_t * p_fp = (fileio_buf_t *)fp;

    if (p_fp == NULL) return -1;

    p_fp->_base = (char *)0;
    p_fp->_bufsiz = 0;
    p_fp->_ptr = (char *)0;

    return 1;
}
int osal_fscanf(osal_file_t fp, const char * _Format, ...)
{
    return 1;
}

int osal_fprintf(osal_file_t fp, const char * _Format, ...)
{
    va_list ptr;
    char logBuf[MAX_PRINT_LENGTH] = {0};

    va_start( ptr, _Format);

    vsnprintf(logBuf, MAX_PRINT_LENGTH, _Format, ptr);

    va_end(ptr);

    myprintf(logBuf);

    return 1;

}

int osal_kbhit(void)
{
    return 0;
}
int osal_getch(void)
{
    return -1;
}

int osal_flush_ch(void)
{
    return -1;
}

size_t osal_strlen(const char * str)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strlen()\n", __FUNCTION__, __LINE__);
    return 0;
}

int osal_strcmp(const char * str1, const char * str2)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strcmp()\n", __FUNCTION__, __LINE__);
    return 0;
}

int osal_sprintf (char * str, const char * format, ...)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_sprintf()\n", __FUNCTION__, __LINE__);
    return 0;
}

int osal_atoi(const char * str)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_atoi()\n", __FUNCTION__, __LINE__);
    return 0;
}

char * osal_strtok(char * str, const char * delimiters)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strtok()\n", __FUNCTION__, __LINE__);
    return NULL;
}

char * osal_strcpy(char * destination, const char * source)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strcpy()\n", __FUNCTION__, __LINE__);
    return NULL;
}

double osal_atof(const char* str)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_atof()\n", __FUNCTION__, __LINE__);
    return 0;
}

char * osal_fgets(char * str, int num, osal_file_t stream)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_fgets()\n", __FUNCTION__, __LINE__);
    return NULL;
}

int osal_strcasecmp(const char *s1, const char *s2)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strcasecmp()\n", __FUNCTION__, __LINE__);
    return 0;
}

int osal_sscanf(const char * s, const char * format, ...)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_sscanf()\n", __FUNCTION__, __LINE__);
    return 0;
}

int osal_strncmp(const char * str1, const char * str2, size_t num)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strncmp()\n", __FUNCTION__, __LINE__);
    return 0;
}

char * osal_strstr(char * str1, const char * str2)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strstr()\n", __FUNCTION__, __LINE__);
    return NULL;
}

int osal_fputc(int c, osal_file_t fp)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_fputc()\n", __FUNCTION__, __LINE__);
    return 0;
}

char * osal_strdup(const char * str)
{
    VLOG(WARN, "<%s:%d> Please implemenet osal_strdup()\n", __FUNCTION__, __LINE__);
    return NULL;
}

void osal_msleep(Uint32 millisecond)
{
    Uint32 countDown = millisecond;
    while (countDown > 0) countDown--;
    //VLOG(WARN, "<%s:%d> Please implemenet osal_msleep()\n", __FUNCTION__, __LINE__);
}

osal_thread_t osal_thread_create(void(*start_routine)(void*), void*arg)
{
    return NULL;
}

Int32 osal_thread_join(osal_thread_t thread, void** retval)
{
    return -1;
}

Int32 osal_thread_timedjoin(osal_thread_t thread, void** retval, Uint32 second)
{
    return 2;   /* error */
}

osal_mutex_t osal_mutex_create(void)
{
    return NULL;
}

void osal_mutex_destroy(osal_mutex_t mutex)
{
}

BOOL osal_mutex_lock(osal_mutex_t mutex)
{
    return FALSE;
}

BOOL osal_mutex_unlock(osal_mutex_t mutex)
{
    return FALSE;
}

int _gettimeofday( struct timeval *tv, void *tzvp )
{
#if 0
    Uint64 t = 0;//__your_system_time_function_here__();  // get uptime in nanoseconds
    tv->tv_sec = t / 1000000000;  // convert to seconds
    tv->tv_usec = ( t % 1000000000 ) / 1000;  // get remaining microseconds

    VLOG(WARN, "<%s:%d> NEED TO IMPLEMENT %s\n", __FUNCTION__, __LINE__, __FUNCTION__);
#endif
    return 0;
}

Uint64 osal_gettime(void)
{
    //VLOG(WARN, "<%s:%d> NEED TO IMPLEMENT %s\n", __FUNCTION__, __LINE__, __FUNCTION__);
    Uint64 cur_clock = 0;
    Uint32 *p = (Uint32 *)(0x50010184);
    Uint32 high = p[1];
    Uint32 low = p[0];
    Uint32 new_high = p[1];

    while(new_high != high){
        high = new_high;
        low = p[0];
        new_high = p[1];
    }

    cur_clock = low | ((Uint64)high << 32);

    return cur_clock/25;
}

void osal_srand(int seed)
{

    return ;
}

/* to return a integer between 0~FEEDING_MAX_SIZE(4M) */
int osal_rand(void)
{

    return 0x10000;
}


/* to conver c to upper case */
int osal_toupper(int c)
{
    int ret = c;
    char *ptr = (char *)&ret;
    int i;

    for (i = 0; i < sizeof(int); i++)
        if (ptr[i] > 96 && ptr[i] < 123)
            ptr[i++] -= 32;

    return ret;
}


//------------------------------------------------------------------------------
// math related api
//------------------------------------------------------------------------------
#ifndef I64
typedef long long I64;
#endif

// 32 bit / 16 bit ==> 32-n bit remainder, n bit quotient
static int fixDivRq(int a, int b, int n)
{
    I64 c;
    I64 a_36bit;
    I64 mask, signBit, signExt;
    int  i;

    // DIVS emulation for BPU accumulator size
    // For SunOS build
    mask = 0x0F; mask <<= 32; mask |= 0x00FFFFFFFF; // mask = 0x0FFFFFFFFF;
    signBit = 0x08; signBit <<= 32;                 // signBit = 0x0800000000;
    signExt = 0xFFFFFFF0; signExt <<= 32;           // signExt = 0xFFFFFFF000000000;

    a_36bit = (I64) a;

    for (i=0; i<n; i++) {
        c =  a_36bit - (b << 15);
        if (c >= 0)
            a_36bit = (c << 1) + 1;
        else
            a_36bit = a_36bit << 1;

        a_36bit = a_36bit & mask;
        if (a_36bit & signBit)
            a_36bit |= signExt;
    }

    a = (int) a_36bit;
    return a;                           // R = [31:n], Q = [n-1:0]
}

int math_div(int number, int denom)
{
    int  c;
    c = fixDivRq(number, denom, 17);             // R = [31:17], Q = [16:0]
    c = c & 0xFFFF;
    c = (c + 1) >> 1;                   // round
    return (c & 0xFFFF);
}

#ifdef LIB_C_STUB
#if 0
/*
* newlib_stubs.c
* the bellow code is just to build ref-code. customers will removed the bellow code bacuase they need a library which is related to the system library such as newlibc
*/
#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/unistd.h>

#ifndef STDOUT_USART
#define STDOUT_USART 0
#endif

#ifndef STDERR_USART
#define STDERR_USART 0
#endif

#ifndef STDIN_USART
#define STDIN_USART 0
#endif

#undef errno
int errno;
extern int errno;

/*
environ
A pointer to a list of environment variables and their values.
For a minimal environment, this empty list is adequate:
*/
char *__env[1] = { 0 };
char **environ = __env;

int _write(int file, char *ptr, int len);

void _exit(int status) {
    _write(1, "exit", 4);
    while (1) {
        ;
    }
}

int _close(int file) {
    return -1;
}
/*
execve
Transfer control to a new process. Minimal implementation (for a system without processes):
*/
int _execve(char *name, char **argv, char **env) {
    errno = ENOMEM;
    return -1;
}
/*
fork
Create a new process. Minimal implementation (for a system without processes):
*/

int _fork() {
    errno = EAGAIN;
    return -1;
}
/*
fstat
Status of an open file. For consistency with other minimal implementations in these examples,
all files are regarded as character special devices.
The `sys/stat.h' header file required is distributed in the `include' subdirectory for this C library.
*/
int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

/*
getpid
Process-ID; this is sometimes used to generate strings unlikely to conflict with other processes. Minimal implementation, for a system without processes:
*/

int _getpid() {
    return 1;
}

/*
isatty
Query whether output stream is a terminal. For consistency with the other minimal implementations,
*/
int _isatty(int file) {
    switch (file){
    case STDOUT_FILENO:
    case STDERR_FILENO:
    case STDIN_FILENO:
        return 1;
    default:
        //errno = ENOTTY;
        errno = EBADF;
        return 0;
    }
}

/*
kill
Send a signal. Minimal implementation:
*/
int _kill(int pid, int sig) {
    errno = EINVAL;
    return (-1);
}

/*
link
Establish a new name for an existing file. Minimal implementation:
*/

int _link(char *old, char *new) {
    errno = EMLINK;
    return -1;
}

/*
lseek
Set position in a file. Minimal implementation:
*/
int _lseek(int file, int ptr, int dir) {
    return 0;
}

/*
sbrk
Increase program data space.
Malloc and related functions depend on this
*/
caddr_t _sbrk(int incr) {

    // extern char _ebss; // Defined by the linker
    char _ebss;
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &_ebss;
    }
    prev_heap_end = heap_end;

    //char * stack = (char*) __get_MSP();
#if 0
    char * stack;
    if (heap_end + incr >  stack)
    {
        _write (STDERR_FILENO, "Heap and stack collision\n", 25);
        errno = ENOMEM;
        return  (caddr_t) -1;
        //abort ();
    }
#endif
    heap_end += incr;
    return (caddr_t) prev_heap_end;

}

/*
read
Read a character to a file. `libc' subroutines will use this system routine for input from all files, including stdin
Returns -1 on error or blocks until the number of characters have been read.
*/

int _read(int file, char *ptr, int len) {
    int n;
    int num = 0;
    switch (file) {
    case STDIN_FILENO:
        for (n = 0; n < len; n++) {
            char c =0;
#if   STDIN_USART == 1
            while ((USART1->SR & USART_FLAG_RXNE) == (Uint16)RESET) {}
            c = (char)(USART1->DR & (Uint16)0x01FF);
#elif STDIN_USART == 2
            while ((USART2->SR & USART_FLAG_RXNE) == (Uint16) RESET) {}
            c = (char) (USART2->DR & (Uint16) 0x01FF);
#elif STDIN_USART == 3
            while ((USART3->SR & USART_FLAG_RXNE) == (Uint16)RESET) {}
            c = (char)(USART3->DR & (Uint16)0x01FF);
#endif
            *ptr++ = c;
            num++;
        }
        break;
    default:
        errno = EBADF;
        return -1;
    }
    return num;
}
#endif

#include <sys/stat.h>
/*
stat
Status of a file (by name). Minimal implementation:
int    _EXFUN(stat,( const char *__path, struct stat *__sbuf ));
*/
int stat(const char *filepath, struct stat *st) {
    return _stat(filepath, st);
}

int _stat(const char *filepath, struct stat *st) {
    st->st_mode = S_IFCHR;
    st->st_size = CMP_FILE_BUFFER_SIZE;
    return 0;
}
#if 0
/*
times
Timing information for current process. Minimal implementation:
*/

clock_t _times(struct tms *buf) {
    return -1;
}

/*
unlink
Remove a file's directory entry. Minimal implementation:
*/
int _unlink(char *name) {
    errno = ENOENT;
    return -1;
}

/*
wait
Wait for a child process. Minimal implementation:
*/
int _wait(int *status) {
    errno = ECHILD;
    return -1;
}

/*
write
Write a character to a file. `libc' subroutines will use this system routine for output to all files, including stdout
Returns -1 on error or number of bytes sent
*/
int _write(int file, char *ptr, int len) {
    int n;
    switch (file) {
    case STDOUT_FILENO: /*stdout*/
        for (n = 0; n < len; n++) {
#if STDOUT_USART == 1
            while ((USART1->SR & USART_FLAG_TC) == (Uint16)RESET) {}
            USART1->DR = (*ptr++ & (Uint16)0x01FF);
#elif  STDOUT_USART == 2
            while ((USART2->SR & USART_FLAG_TC) == (Uint16) RESET) {
            }
            USART2->DR = (*ptr++ & (Uint16) 0x01FF);
#elif  STDOUT_USART == 3
            while ((USART3->SR & USART_FLAG_TC) == (Uint16)RESET) {}
            USART3->DR = (*ptr++ & (Uint16)0x01FF);
#endif
        }
        break;
    case STDERR_FILENO: /* stderr */
        for (n = 0; n < len; n++) {
#if STDERR_USART == 1
            while ((USART1->SR & USART_FLAG_TC) == (Uint16)RESET) {}
            USART1->DR = (*ptr++ & (Uint16)0x01FF);
#elif  STDERR_USART == 2
            while ((USART2->SR & USART_FLAG_TC) == (Uint16) RESET) {
            }
            USART2->DR = (*ptr++ & (Uint16) 0x01FF);
#elif  STDERR_USART == 3
            while ((USART3->SR & USART_FLAG_TC) == (Uint16)RESET) {}
            USART3->DR = (*ptr++ & (Uint16)0x01FF);
#endif
        }
        break;
    default:
        errno = EBADF;
        return -1;
    }
    return len;
}

#endif
#endif

