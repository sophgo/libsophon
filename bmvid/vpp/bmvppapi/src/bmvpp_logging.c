//
// Created by yuan on 8/24/21.
//

#include "bmvppapi.h"
static int g_bmvpp_log_level = BMVPP_LOG_ERR;
void bmvpp_set_log_level(bmvpp_log_level_t level)
{
    g_bmvpp_log_level = level;
}

int bmvpp_log_level()
{
    return g_bmvpp_log_level;
}
