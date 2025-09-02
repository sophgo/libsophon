#ifndef BM_BMCPU_H_
#define BM_BMCPU_H_
#include <mutex>
#include <thread>
#include <queue>
#include <list>
#include <semaphore.h>
#include <map>
#include <dlfcn.h>
#include <sys/sem.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdint.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <condition_variable>

#include "bmlib_runtime.h"


void* bmcpu_thread(void* arg);
extern std::queue<bm_api_to_bmcpu_t> uncomplete_msg_queue;
extern std::list<bm_ret_t> complete_msg_queue;
extern pthread_mutex_t uncomplete_msg_mtx;
extern pthread_mutex_t complete_msg_mtx;
extern pthread_cond_t uncomplete_msg_cv;
extern bm_profile_t bm_profile;
// extern uint64_t bm_profile_timestamp;
#endif