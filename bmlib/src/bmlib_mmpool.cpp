#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_mmpool.h"

#ifdef USING_CMODEL
// #define MEM_POOL_DEBUG
/*
void mem_pool_init(mem_pool_t *pool) {
  struct pool_struct *P = &pool->_mem_pool_list[0];
  P->slot_avail.clear();
  P->slot_in_use.clear();
  P->slot_avail.push_back(make_pair(0, pool->total_size));
}
*/
#define GLOBAL_MEM_START_ADDR 0x100000000
/* look for the smallest yet sufficient memory slot for the demanding size */
static pool_addr_t find_slot(struct pool_struct *P, pool_size_t size) {
  ASSERT(size % MIN_SLOT_SIZE == 0);

  vector < pool_pair_t>::iterator it, it_min = P->slot_avail.end();
  pool_size_t min_size_among_sufficient = 0;
  for (it = P->slot_avail.begin(); it != P->slot_avail.end(); it++) {
    if ((*it).second >= size) {
      if (min_size_among_sufficient == 0 ||
              (*it).second < min_size_among_sufficient) {
        it_min = it;
        min_size_among_sufficient = (*it).second;
      }
    }
  }

  if (it_min == P->slot_avail.end() || (*it_min).second == 0) {
    printf("Memory exhausted: cannot find a slot.\n");
    return MEM_POOL_ADDR_INVALID;
  }
  pool_addr_t addr = (*it_min).first;

  if ((*it_min).second == size) {
    P->slot_avail.erase(it_min);
  } else {
    (*it_min).first = addr + size;
    (*it_min).second -= size;
  }

  P->slot_in_use.insert(make_pair(addr, size));

  return addr;
}

pool_addr_t bm_mem_pool::bm_mem_pool_alloc(pool_addr_t size) {
  struct pool_struct *P = &_mem_pool_list[0];
  pthread_mutex_lock(&P->mem_pool_lock);
  ASSERT(P->num_slots_in_use < MEM_POOL_SLOT_NUM);
  pool_size_t size_to_alloc = (size + MIN_SLOT_SIZE - 1) / MIN_SLOT_SIZE
                              * MIN_SLOT_SIZE;
  pool_addr_t addr_to_alloc = find_slot(P, size_to_alloc);

  if (addr_to_alloc == MEM_POOL_ADDR_INVALID) {
#ifdef MEM_POOL_DEBUG
    printf("mem_pool: mem alloc failed in searching stage\n");
#endif
    ASSERT(0);  // no error handling yet
    return MEM_POOL_ADDR_INVALID;
  } else if (addr_to_alloc + size_to_alloc > _total_size) {
#ifdef MEM_POOL_DEBUG
    printf("mem_pool: mem alloc insufficient size\n");
#endif
    ASSERT(0);  // no error handling yet
    return MEM_POOL_ADDR_INVALID;
  }

  P->num_slots_in_use++;
  P->mem_in_use += size_to_alloc;
  ASSERT(P->mem_in_use <= _total_size);

#ifdef MEM_POOL_DEBUG
  printf("mem_pool: alloc addr 0x%llx with size of 0x%llx; \n", addr_to_alloc, size_to_alloc);
  printf("mem_pool: actual size required = 0x%llx\n", size);
#endif
  pthread_mutex_unlock(&P->mem_pool_lock);
  return addr_to_alloc + GLOBAL_MEM_START_ADDR;
}

void bm_mem_pool::bm_mem_pool_free(pool_addr_t addr_to_free) {
  struct pool_struct *P = &_mem_pool_list[0];
  pthread_mutex_lock(&P->mem_pool_lock);
  addr_to_free -= GLOBAL_MEM_START_ADDR;
  pool_map_t::iterator it = P->slot_in_use.find(addr_to_free);
  ASSERT(it != P->slot_in_use.end());
  pool_size_t size_to_free = P->slot_in_use[addr_to_free];
  ASSERT(size_to_free % MIN_SLOT_SIZE == 0);
  P->slot_in_use.erase(it);
  P->num_slots_in_use--;
  ASSERT(P->mem_in_use >= size_to_free);
  P->mem_in_use -= size_to_free;

  pool_addr_t addr_next = addr_to_free + size_to_free;
  vector < pool_pair_t>::iterator it_prev_slot = find_if (P->slot_avail.begin(),
      P->slot_avail.end(), offset_prev_finder(addr_to_free));
  vector < pool_pair_t>::iterator it_next_slot = find_if (P->slot_avail.begin(),
      P->slot_avail.end(), offset_next_finder(addr_next));

  if (it_prev_slot == P->slot_avail.end() &&
          it_next_slot == P->slot_avail.end()) {
    P->slot_avail.push_back(make_pair(addr_to_free, size_to_free));
  } else if (it_prev_slot == P->slot_avail.end()) {
    (*it_next_slot).first = addr_to_free;
    (*it_next_slot).second += size_to_free;
#ifdef DEBUG
    CHECK_FREED_MEM((*it_next_slot).first, (*it_next_slot).second, P);
#endif
  } else if (it_next_slot == P->slot_avail.end()) {
    (*it_prev_slot).second += size_to_free;
#ifdef DEBUG
    CHECK_FREED_MEM((*it_prev_slot).first, (*it_prev_slot).second, P);
#endif
  } else {
    (*it_prev_slot).second += (size_to_free + (*it_next_slot).second);
    P->slot_avail.erase(it_next_slot);
  }

#ifdef MEM_POOL_DEBUG
  printf("mem_pool_free: addr_to_free = 0x%llx; size_to_free = 0x%llx\n",
          addr_to_free, size_to_free);
#endif
  pthread_mutex_unlock(&P->mem_pool_lock);
}

/*
void mem_pool_create(mem_pool_t **pool, u64 total_size) {
  mem_pool_t *tpool = new mem_pool_t;
  tpool->total_size = total_size;
  mem_pool_init(tpool);
  tpool->_mem_pool_count = 1;
  struct pool_struct *P = &tpool->_mem_pool_list[0];
  P->num_slots_in_use = 0;
  P->mem_in_use = 0;
  *pool = tpool;

#ifdef MEM_POOL_DEBUG
  printf("mem_pool: create\n");
#endif
}
*/

bm_mem_pool::bm_mem_pool(u64 total_size)
  : _total_size(total_size),
    _mem_pool_count(1) {
  struct pool_struct *P = &_mem_pool_list[0];
  pthread_mutex_init(&P->mem_pool_lock, NULL);
  P->slot_avail.clear();
  P->slot_in_use.clear();
  P->slot_avail.push_back(make_pair(0, _total_size));
  P->num_slots_in_use = 0;
  P->mem_in_use = 0;
}

bm_mem_pool::~bm_mem_pool() {
  /* sanity checking */
  struct pool_struct *P = &_mem_pool_list[0];
  ASSERT(P->slot_avail[0].first == 0);
  ASSERT(P->slot_avail[0].second == _total_size);
  ASSERT(P->slot_in_use.empty());
  ASSERT(P->num_slots_in_use == 0);
  ASSERT(P->mem_in_use == 0);
}
#endif

