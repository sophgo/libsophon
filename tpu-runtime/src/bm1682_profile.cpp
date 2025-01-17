#ifndef BM1682_PROFILE_H
#define BM1682_PROFILE_H
#include <vector>
#include <stdio.h>
#include <string.h>
#include "bm1682_profile.h"

namespace bm1682_profile {

std::vector<unsigned char> &get_log()
{
    static std::vector<unsigned char> u8_log;
    return u8_log;
}
void bm_log_profile_callback(int api, int result, int duration, const char * log_buffer)
{
    auto& v_log = get_log();
    if(log_buffer){
        // need to use printf, do not change to BMRT_LOG
        v_log.insert(v_log.end(), log_buffer, log_buffer + strlen(log_buffer));
        printf("%s\n", log_buffer);
    }
}
}

#endif // BM1682_PROFILE_H

