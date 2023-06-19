//
// Created by yuan on 8/24/21.
//

#ifndef MIDDLEWARE_SOC_BMVPP_INT_H
#define MIDDLEWARE_SOC_BMVPP_INT_H

#include <stdio.h>
#include <string.h>
#include "bmvppapi.h"
#include "vpplib.h"

#define BMVPP_MAX_NUM_SOPHONS 128

#define bmvpp_log_err(msg, ... )   if (bmvpp_log_level() >= BMVPP_LOG_ERR)   { fprintf(stderr, "[ERROR] [%s, %d] "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define bmvpp_log_warn(msg, ... )  if (bmvpp_log_level() >= BMVPP_LOG_WARN)  { fprintf(stderr, "[WARN]  [%s, %d] "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define bmvpp_log_info(msg, ...)   if (bmvpp_log_level() >= BMVPP_LOG_INFO)  { fprintf(stderr, "[INFO]  [%s, %d] "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define bmvpp_log_debug(msg, ...)  if (bmvpp_log_level() >= BMVPP_LOG_DEBUG) { fprintf(stderr, "[DEBUG] [%s, %d] "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }
#define bmvpp_log_trace(msg, ...)  if (bmvpp_log_level() >= BMVPP_LOG_TRACE) { fprintf(stderr, "[TRACE] [%s, %d] "msg, __FUNCTION__, __LINE__, ## __VA_ARGS__); }

#define BMVPP_UNUSED(x) (x)=(x);

int bmvpp_get_vppfd(int soc_idx, vpp_fd_s *vppfd);

#endif //MIDDLEWARE_SOC_BMVPP_INT_H
