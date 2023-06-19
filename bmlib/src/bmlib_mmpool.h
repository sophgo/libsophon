#ifndef BMLIB_MMPOOL_H_
#define BMLIB_MMPOOL_H_

#define MEM_POOL_ADDR_INVALID   (BM_MEM_ADDR_NULL)

#ifdef USING_CMODEL
#include <utility>
#include <vector>
#include <iostream>
#include <map>
#include <algorithm>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

#define MEM_POOL_SLOT_NUM       (256 * 256)
#define MIN_SLOT_SIZE (4 * 1024)
#define MAX_POOL_COUNT (2)

#define CHECK_FREED_MEM(offset, size, P)                                      \
  do {                                                                        \
    pool_map_t::iterator it;                                                  \
    for (it = P->slot_in_use.begin(); it != P->slot_in_use.end(); it++) {     \
      if ( ((*it).first <= offset && (*it).first + (*it).second > offset) ||   \
          ((*it).first < offset + size && (*it).first + (*it).second >= offset + size) ){ \
        printf("CHECK_FREED_MEM ERROR: attempting to free memory in use\n");  \
        ASSERT(0);                                                            \
      }                                                                       \
    }                                                                         \
  } while(0);

typedef u64 pool_addr_t;
typedef u64 pool_size_t;
typedef pair < pool_addr_t, pool_size_t> pool_pair_t;
typedef map < pool_addr_t, pool_size_t> pool_map_t;

class offset_prev_finder {
public:
  offset_prev_finder(pool_addr_t offset_prev) : offset_prev_(offset_prev) {}
  bool operator () (const pool_pair_t pair_) {
    return pair_.first + pair_.second == offset_prev_;
  }
private:
  pool_addr_t offset_prev_;
};

class offset_next_finder {
public:
  offset_next_finder(pool_addr_t offset_next) : offset_next_(offset_next) {}
  bool operator () (const pool_pair_t pair_) {
    return pair_.first == offset_next_;
  }
private:
  pool_addr_t offset_next_;
};

struct pool_struct {
  pthread_mutex_t mem_pool_lock;
  int num_slots_in_use;
  pool_size_t mem_in_use;
  vector < pool_pair_t> slot_avail;  // (offset, size)
  pool_map_t slot_in_use; // offset -> size
};

class bm_mem_pool {
 public:
  bm_mem_pool(u64 total_size);
  ~bm_mem_pool();
  pool_addr_t bm_mem_pool_alloc(pool_addr_t size);
  void bm_mem_pool_free(pool_addr_t addr);

 private:
  u64                  _total_size;
  struct pool_struct   _mem_pool_list[MAX_POOL_COUNT];
  int                  _mem_pool_count;
  /* managing allocated chunk */
  // u64                  slot_addr[MEM_POOL_SLOT_NUM];
  // int                  slot_size[MEM_POOL_SLOT_NUM];
};
#ifdef __cplusplus
}
#endif
#endif
#endif /* BMLIB_MMPOOL_H_ */
