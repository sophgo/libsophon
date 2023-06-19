/*
 * log functions
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_log.h"

#ifdef __linux__
#include <unistd.h>
#include "syslog.h"
#include "pthread.h"
#include "bmlib_utils.h"
#else
#endif

static int bmlib_log_level = BMLIB_LOG_ERROR;
#define BMLIB_LOG_LOG_TAG "bmlib_log"

#ifdef __linux__
    static pthread_mutex_t bmlog_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
    HANDLE ghMutex = CreateMutex(
            NULL,              // default security attributes
            FALSE,             // initially not owned
            NULL);             // unnamed mutex
 #endif



#ifdef __cplusplus
extern "C" {
#endif

static const char *get_level_str(int level) {
  switch (level) {
  case BMLIB_LOG_QUIET:
    return "quiet";
  case BMLIB_LOG_DEBUG:
    return "debug";
  case BMLIB_LOG_VERBOSE:
    return "verbose";
  case BMLIB_LOG_INFO:
    return "info";
  case BMLIB_LOG_WARNING:
    return "warning";
  case BMLIB_LOG_ERROR:
    return "error";
  case BMLIB_LOG_FATAL:
    return "fatal";
  case BMLIB_LOG_PANIC:
    return "panic";
  default:
    return "";
  }
}

static int cov_syslog_level(int level) {
  switch (level) {
  case BMLIB_LOG_QUIET:
    return 7;
  case BMLIB_LOG_DEBUG:
    return 7;
  case BMLIB_LOG_VERBOSE:
    return 5;
  case BMLIB_LOG_INFO:
    return 6;
  case BMLIB_LOG_WARNING:
    return 4;
  case BMLIB_LOG_ERROR:
    return 3;
  case BMLIB_LOG_FATAL:
    return 1;
  case BMLIB_LOG_PANIC:
    return 0;
  default:
    return 7;
  }
}
#define BMLIB_LOG_BUFFER_SIZE 256
void bmlib_log_default_callback(const char *tag, int level, const char *fmt, va_list args) {
#ifndef USING_CMODEL
  char log_buffer[BMLIB_LOG_BUFFER_SIZE] = "";

  if (level <= bmlib_log_level) {
    #ifdef _WIN32
        DWORD dwWaitResult = WaitForSingleObject(
                ghMutex,    // handle to mutex
                INFINITE);  // no time-out interval
        if(dwWaitResult == WAIT_OBJECT_0){
            vsnprintf(log_buffer, BMLIB_LOG_BUFFER_SIZE, fmt, args);
            printf("[%s][%s] %s", tag, get_level_str(level), log_buffer);
        }
        ReleaseMutex(ghMutex);
    #else
        pthread_mutex_lock(&bmlog_mutex);
        vsnprintf(log_buffer, BMLIB_LOG_BUFFER_SIZE, fmt, args);
        printf("[%s][%s] %s", tag, get_level_str(level), log_buffer);
        syslog(LOG_USER | cov_syslog_level(level), "[%s][%s] %s", tag, get_level_str(level), log_buffer);
        pthread_mutex_unlock(&bmlog_mutex);
    #endif
  }
#else
    #ifdef _WIN32
        DWORD dwWaitResult = WaitForSingleObject(
                ghMutex,    // handle to mutex
                INFINITE);  // no time-out interval
        if(dwWaitResult == WAIT_OBJECT_0){
            printf("[%s][%s]", tag, get_level_str(level));
            vprintf(fmt, args);
        }
        ReleaseMutex(ghMutex);
    #else
        pthread_mutex_lock(&bmlog_mutex);
        printf("[%s][%s]", tag, get_level_str(level));
        vprintf(fmt, args);
        pthread_mutex_unlock(&bmlog_mutex);
    #endif
#endif
}

static void (*bmlib_log_callback)(const char*, int, const char*, va_list) =
  bmlib_log_default_callback;

void bmlib_log(const char *tag, int level, const char *fmt, ...) {
  void (*log_callback)(const char*, int, const char*, va_list) = bmlib_log_callback;
  va_list args;

  if (log_callback) {
    va_start(args, fmt);
    log_callback(tag, level, fmt, args);
    va_end(args);
  }
}

int bmlib_log_get_level(void) {
  return bmlib_log_level;
}

void bmlib_log_set_level(int level) {
  bmlib_log_level = level;
}

void bmlib_log_set_callback(void (*callback)(const char*, int, const char*, va_list)) {
  bmlib_log_callback = callback;
}

void bm_set_debug_mode(bm_handle_t handle, int mode) {
  UNUSED(handle);
  UNUSED(mode);
  printf("fw log dump not support now \n");
}

bm_status_t bm_enable_perf_monitor(bm_handle_t handle, bm_perf_monitor_t *perf_monitor) {
#ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(perf_monitor);
  return BM_SUCCESS;
#else
  if (handle == nullptr) {
    bmlib_log(BMLIB_LOG_LOG_TAG, BMLIB_LOG_ERROR,
          "handle is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }
  if ((perf_monitor == nullptr) || (perf_monitor->buffer_size == 0)) {
    bmlib_log(BMLIB_LOG_LOG_TAG, BMLIB_LOG_ERROR,
          "bm_enable_perf_monitor param err\n");
    return BM_ERR_PARAM;
  }
  if (0 == platform_ioctl(handle, BMDEV_ENABLE_PERF_MONITOR, perf_monitor))
    return BM_SUCCESS;
  else
    return BM_ERR_FAILURE;
#endif
}

bm_status_t bm_disable_perf_monitor(bm_handle_t handle, bm_perf_monitor_t *perf_monitor) {
#ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(perf_monitor);
  return BM_SUCCESS;
#else
  if (handle == nullptr) {
    bmlib_log(BMLIB_LOG_LOG_TAG, BMLIB_LOG_ERROR,
          "handle is nullptr %s: %s: %d\n",
          __FILE__, __func__, __LINE__);
    return BM_ERR_DEVNOTREADY;
  }
  if ((perf_monitor == nullptr) || (perf_monitor->buffer_size == 0)) {
    bmlib_log(BMLIB_LOG_LOG_TAG, BMLIB_LOG_ERROR,
          "bm_disable_perf_monitor param err\n");
    return BM_ERR_PARAM;
  }
  if (0 == platform_ioctl(handle, BMDEV_DISABLE_PERF_MONITOR, perf_monitor))
    return BM_SUCCESS;
  else
    return BM_ERR_FAILURE;
#endif
}

bm_status_t bmlib_log_mutex_lock(void){
#ifdef __linux__
    int ret = pthread_mutex_lock(&bmlog_mutex);
    if(ret == 0)
        return BM_SUCCESS;
    else
        return BM_ERR_FAILURE;
#else
    DWORD dwWaitResult = WaitForSingleObject(
                ghMutex,    // handle to mutex
                INFINITE);  // no time-out interval
        if(dwWaitResult == WAIT_OBJECT_0)
            return BM_SUCCESS;
        else
            return BM_ERR_FAILURE;
#endif
}

bm_status_t bmlib_log_mutex_unlock(void){
#ifdef __linux__
    int ret = pthread_mutex_unlock(&bmlog_mutex);
    if(ret == 0)
        return BM_SUCCESS;
    else
        return BM_ERR_FAILURE;
#else
    if(ReleaseMutex(ghMutex))
        return BM_SUCCESS;
    else
        return BM_ERR_FAILURE;
#endif
}

#ifdef __cplusplus
}
#endif
