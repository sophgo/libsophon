/*
 * log functions
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_log.h"
#include "syslog.h"
#include "pthread.h"
#include "bmlib_utils.h"

// Get the value set by the environment variable bmlib_log_level, use default BMLIB_LOG_WARNING if not set
static int bmlib_log_level = []() {
  const char* env = getenv("BMLIB_LOG_LEVEL");
  if (env) {
    if (strcmp(env, "quiet") == 0) {
      return BMLIB_LOG_QUIET;
    } else if (strcmp(env, "debug") == 0) {
      return BMLIB_LOG_DEBUG;
    } else if (strcmp(env, "verbose") == 0) {
      return BMLIB_LOG_VERBOSE;
    } else if (strcmp(env, "info") == 0) {
      return BMLIB_LOG_INFO;
    } else if (strcmp(env, "warning") == 0) {
      return BMLIB_LOG_WARNING;
    } else if (strcmp(env, "error") == 0) {
      return BMLIB_LOG_ERROR;
    } else if (strcmp(env, "fatal") == 0) {
      return BMLIB_LOG_FATAL;
    } else if (strcmp(env, "panic") == 0) {
      return BMLIB_LOG_PANIC;
    }
    int level = atoi(env);
    return level;
  }
  return BMLIB_LOG_WARNING;
}();
#define BMLIB_LOG_LOG_TAG "bmlib_log"

static pthread_mutex_t bmlog_mutex = PTHREAD_MUTEX_INITIALIZER;



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
      pthread_mutex_lock(&bmlog_mutex);
      vsnprintf(log_buffer, BMLIB_LOG_BUFFER_SIZE, fmt, args);
      printf("[%s][%s] %s", tag, get_level_str(level), log_buffer);
      syslog(LOG_USER | cov_syslog_level(level), "[%s][%s] %s", tag, get_level_str(level), log_buffer);
      pthread_mutex_unlock(&bmlog_mutex);
  }
#else
      pthread_mutex_lock(&bmlog_mutex);
      printf("[%s][%s]", tag, get_level_str(level));
      vprintf(fmt, args);
      pthread_mutex_unlock(&bmlog_mutex);
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
  int ret;
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
  ret = platform_ioctl(handle, BMDEV_ENABLE_PERF_MONITOR, perf_monitor);
  if (0 == ret)
    return BM_SUCCESS;
  else
    bmlib_log(BMLIB_LOG_LOG_TAG, BMLIB_LOG_ERROR,
          "bm_enable_perf_monitor ioclt err, ret = %d:%d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
#endif
}

bm_status_t bm_disable_perf_monitor(bm_handle_t handle, bm_perf_monitor_t *perf_monitor) {
#ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(perf_monitor);
  return BM_SUCCESS;
#else
  int ret;
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
  ret = platform_ioctl(handle, BMDEV_DISABLE_PERF_MONITOR, perf_monitor);
  if (0 == ret)
    return BM_SUCCESS;
  else
    bmlib_log(BMLIB_LOG_LOG_TAG, BMLIB_LOG_ERROR,
          "bm_disable_perf_monitor ioclt err, ret = %d:%d\n", ret, __LINE__);
  return BM_ERR_FAILURE;
#endif
}

bm_status_t bmlib_log_mutex_lock(void) {
    int ret = pthread_mutex_lock(&bmlog_mutex);
    if (ret == 0)
        return BM_SUCCESS;
    else
        return BM_ERR_FAILURE;
}

bm_status_t bmlib_log_mutex_unlock(void) {
    int ret = pthread_mutex_unlock(&bmlog_mutex);
    if (ret == 0)
        return BM_SUCCESS;
    else
        return BM_ERR_FAILURE;
}

#ifdef __cplusplus
}
#endif
