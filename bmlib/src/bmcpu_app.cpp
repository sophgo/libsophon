/*
This is a user-mode program written in C++
It will create a thread
The thread will open two queues, implemented using mqueue.h, one queue for the unfinished queue and one queue for the completed queue
Take out an API from the queue, execute the API, put the result into the completed queue, and notify the API caller through the sem_name semaphore passed by the API
API types include:
--LOAD_LIB: read lib_name, store the handle obtained by ldopen in the global library table,
--UNLOAD_LIB: read lib_name, find the corresponding handle and delete it to ensure code robustness
--GET_FUNC: find the corresponding lib through the MD5 value of the passed structure, then find the corresponding function through func_name, and store it in the global function table through func_id
--CALL_FUNC: read lib_name and func_name, find the handle from the global library table, call dlsym to get the function pointer, and call the function
The program should strictly ensure robustness, including but not limited to: boundary checking, error handling, memory leak checking
The program should contain necessary debugging prints
The program ensures thread safety
*/
#include "bmcpu_app.h"
#include "bmlib_ioctl.h"
#include "bmlib_utils.h"
#include "api.h"

typedef uint8_t u8;
typedef uint32_t u32;
static int f_id = 22;
static int find_func_id = 0;
#define BMLIB_bmcpu_LOG_TAG "bmcpu_app"
std::queue<bm_api_to_bmcpu_t> uncomplete_msg_queue;
std::list<bm_ret_t> complete_msg_queue;
pthread_mutex_t uncomplete_msg_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t complete_msg_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t uncomplete_msg_cv = PTHREAD_COND_INITIALIZER;
static uint64_t start_bm_profile_timestamp=0;
static uint64_t end_bm_profile_timestamp=0;
bm_profile_t bm_profile = {0};
static int flag= 0;

typedef struct bm_api_cpu_load_library_internal {
  u8 library_path[256];
  void *library_addr;
  u32 size;
  u8 library_name[LIB_MAX_NAME_LEN];
  unsigned char md5[MD5SUM_LEN];
  int cur_rec;
} __attribute__((packed)) bm_api_cpu_load_library_internal_t;

typedef struct bm1688_get_func_internal {
  int core_id;
  int f_id;
  unsigned char md5[MD5SUM_LEN];
  unsigned char func_name[FUNC_MAX_NAME_LEN];
} __attribute__((packed)) bm1688_get_func_internal_t;

typedef struct bm1688_launch_func_internal {
  int f_id;
  unsigned int size;
  u8 param[4096];
} __attribute__((packed)) bm1688_launch_func_internal_t;
typedef enum {
  API_ID_SGTPUV8_LOAD_LIB = 0x90000001,
  API_ID_SGTPUV8_GET_FUNC = 0x90000002,
  API_ID_SGTPUV8_LAUNCH_FUNC = 0x90000003,
  API_ID_SGTPUV8_UNLOAD_LIB = 0x90000004,
} API_ID;

struct exec_func {
  int f_id;
  int (*f_ptr)(void *, unsigned int);
  unsigned char func_name[FUNC_MAX_NAME_LEN];
};

// records the mapped void *mapped_memory and the int size corresponding to mapped_memory
std::vector<std::pair<void *, size_t>> mapped_memory_list;

struct SGTPUV8_lib_info {
  void *lib_handle;
  char lib_name[LIB_MAX_NAME_LEN];
  unsigned char md5[MD5SUM_LEN];
  std::map<int, struct exec_func> func_table;
};
//A mapping table, md5 value corresponds to SGTPUV8_lib_info
std::map<std::array<unsigned char, MD5SUM_LEN>, SGTPUV8_lib_info> lib_table;

// #define BMDEV_SET_IOMAP_TPYE             _IOWR('p', 0x99, u_int)
#define TPU_GDMA_SIZE     0x10000  // gdma size
#define TPU_SYS_SIZE      0x30000  // sys size
#define TPU_REG_SIZE      0x10000  // reg size
#define TPU_SMEM_SIZE      0x1000     // smem size aligned to 4096
#define TPU_LMEM_SIZE      0x80000  // lmem size


/*Physical addresses are mapped to virtual addresses so that registers can be read and written directly*/
static void *mmap_gdma;
static void *mmap_sys;
static void *mmap_reg;
static void *mmap_smem;
static void *mmap_lmem;

static void * mmap_paddr_to_vaddr(int type, size_t size, int fd) {
  int ret = ioctl(fd, BMDEV_SET_IOMAP_TPYE, type);
  if (ret != 0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "ioctl failed\n");
  }
  // Request mmap, passing the starting address and size
  void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_memory == MAP_FAILED) {
      perror("mmap_paddr_to_vaddr, mmap failed");
      close(fd);
      return NULL;
  }
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "mapped_memory: %p\n", mapped_memory);

  // Read the value of this register
  // printf("The value of the register is: %x\n", *((unsigned char *)mapped_memory));

  // Using mapped memory
  // *((int *)mapped_memory) = 0xff;
  // printf("The value of the register is: %x\n", *((unsigned char *)mapped_memory));

  mapped_memory_list.emplace_back(mapped_memory, size);
  return mapped_memory;

}

static int mmap_tpu_sys(int fd) {
  // void *mmap;
  int result = 0;

  mmap_gdma = mmap_paddr_to_vaddr(0, TPU_GDMA_SIZE, fd);
  if (mmap_gdma == NULL) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "GDMA mmap failed\n");
    return -1;
  }

  mmap_sys = mmap_paddr_to_vaddr(1, TPU_SYS_SIZE, fd);
  if (mmap_sys == NULL) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "SYS mmap failed\n");
    return -1;
  }

  mmap_reg = mmap_paddr_to_vaddr(2, TPU_REG_SIZE, fd);
  if (mmap_reg == NULL) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "REG mmap failed\n");
    return -1;
  }

  mmap_smem = mmap_paddr_to_vaddr(3, TPU_SMEM_SIZE, fd);
  if (mmap_smem == NULL) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "SMEM mmap failed\n");
    return -1;
  }

  mmap_lmem = mmap_paddr_to_vaddr(4, TPU_LMEM_SIZE, fd);
  if (mmap_lmem == NULL) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "LMEM mmap failed\n");
    return -1;
  }

  return 0;
}

static int mmap_addr_to_firmwarecore(void* handle) {
  // void *mmap;
  int result = 0;
  dlerror();
  int (*CallPhysicalToVirtual)(void *, int) = (int (*)(void *, int))dlsym(handle, "PhysicalToVirtual");
  const char *dlsym_error = dlerror();
  if (dlsym_error) {
    fprintf(stderr, "Cannot load symbol 'CallPhysicalToVirtual': %s\n", dlsym_error);
    dlclose(handle);
    return 1;
  }

  result = CallPhysicalToVirtual(mmap_gdma, 0);
  if (result!=0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_sys, 1);
  if (result!=0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_reg, 2);
  if (result != 0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_smem, 3);
  if (result != 0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_lmem, 4);
  if (result != 0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "CallPhysicalToVirtual failed\n");
    return -1;
  }

  return 0;
}

static int load_table_to_firmwarecore(void* handle) {
  dlerror();
  void (*CallFunc)() = (void (*)())dlsym(handle, "load_lookup_tables");
  const char *dlsym_error = dlerror();
  if (dlsym_error) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "Cannot load symbol 'CallFunc': %s\n", dlsym_error);
      dlclose(handle);
      return -1;
  }

  try {
    CallFunc();
  } catch (const std::exception &e) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "load_lookup_tables failed: %s\n", e.what());
    return -1;
  }
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_DEBUG, "load_lookup_tables success\n");
  return 0;
}


//4 functions, corresponding to 4 APIs
static int load_lib_process(bm_api_cpu_load_library_internal_t* api) {
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Name: %s\n", api->library_name);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Path: %s\n", api->library_path);
  // bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Size: %d\n", api->size);
  // bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "MD5: ");
  // for (int i = 0; i < MD5SUM_LEN; i++) {
  //     printf("%x", (int)api->md5[i]);
  // }
  // std::cout << std::endl;
  // std::cout << std::dec;
  bm_status_t ret = BM_SUCCESS;

  std::array<unsigned char, MD5SUM_LEN> api_md5;
  std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());

  if (lib_table.find(api_md5) != lib_table.end()) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library %s already loaded\n", api->library_path);
    return 0;
  } else {
    // char* file_path = nullptr;

    // ret = find_lib_path((char *)api->library_name, &file_path);
    // if (ret != BM_SUCCESS) {
    //   bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "find_lib_path failed.\n");
    //   return -1;
    // }

    void* handle = dlopen((char*)api->library_path, RTLD_LAZY);
    if (handle == NULL) {
        bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "dlopen failed: %s, lib path is %s.\n", dlerror(), (char*)api->library_path);
        return -1;
    }
    SGTPUV8_lib_info lib_info;
    lib_info.lib_handle = handle;
    memcpy(lib_info.md5, api->md5, MD5SUM_LEN);
    memcpy(lib_info.lib_name, api->library_name, LIB_MAX_NAME_LEN);

    lib_table[api_md5] = lib_info;
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library %s loaded!\n", (char*)api->library_path);
  }
  if (mmap_addr_to_firmwarecore(lib_table[api_md5].lib_handle)) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "mmap_addr_to_firmwarecore failed\n");
  }

  if (load_table_to_firmwarecore(lib_table[api_md5].lib_handle)) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "load_table_to_firmwarecore failed\n");
  }

  return 0;
}

static int unload_lib_process(bm_api_cpu_load_library_internal_t* api) {
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Name: %s\n", api->library_name);
  // std::cout << "Library Path: " << api->library_path << std::endl;
  // std::cout << "Library Size: " << api->size << std::endl;
  // bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "MD5: ");
  // for (int i = 0; i < MD5SUM_LEN; i++) {
  //     printf("%x", (int)api->md5[i]);
  // }
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "\n");

  std::array<unsigned char, MD5SUM_LEN> api_md5;
  std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());
  if (lib_table.find(api_md5) == lib_table.end()) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library %s not loaded\n", api->library_name);
      return -1;
  } else {
      int ret = dlclose(lib_table[api_md5].lib_handle);
      if (ret != 0) {
          perror("dlclose");
          return -1;
      }
      lib_table.erase(api_md5);
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library %s\n", api->library_name);
  }
  return 0;
}

static int get_func_process(bm1688_get_func_internal_t *api) {
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Core ID: %d\n", api->core_id);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Function Name:%s \n", api->func_name);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "MD5: ");
  for (int i = 0; i < MD5SUM_LEN; i++) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO,"%d", (int)api->md5[i]);
  }
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "\n");

  std::array<unsigned char, MD5SUM_LEN> api_md5;
  std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());
  SGTPUV8_lib_info lib_info = lib_table[api_md5];
  struct exec_func func;

  if (lib_table.find(api_md5) == lib_table.end()) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "Library not exist\n");
    return -1;
  } else {
    if (lib_info.func_table.find(api->f_id) != lib_info.func_table.end()) {
        find_func_id = api->f_id;
        bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Function <%s> already loaded\n", api->func_name);
        return 0;
    }
    dlerror();
    func.f_ptr = (int (*)(void *, unsigned int))dlsym(lib_info.lib_handle, (char*)api->func_name);

    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "func.f_ptr: %p\n", func.f_ptr);
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        fprintf(stderr, "dlsym error: %s\n", dlsym_error);
        return -1;
    }
    f_id = (f_id+1) % 100000;
    find_func_id = f_id;
    func.f_id = f_id;
    memcpy(func.func_name, api->func_name, FUNC_MAX_NAME_LEN);
    lib_info.func_table[f_id] = func;
    lib_table[api_md5] = lib_info;
  }
  return BM_SUCCESS;
}

static int call_func_process(bm1688_launch_func_internal* api_addr) {
  int ret = 0;
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Function ID: %d\n", api_addr->f_id);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Function Size: %d\n", api_addr->size);
  int (*f_ptr)(void *, unsigned int);
  char func_name[32];

  auto it = lib_table.begin();
  if (api_addr->f_id == BM_API_ID_MEM_CPY ||
      api_addr->f_id == BM_API_ID_MEMCPY_BYTE ||
      api_addr->f_id == BM_API_ID_MEMCPY_WSTRIDE) {
    for (; it != lib_table.end(); it++) {
        SGTPUV8_lib_info lib_info = it->second;
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_DEBUG, "Function not record. try to find directly in lib\n");

      switch (api_addr->f_id)
      {
      case BM_API_ID_MEM_CPY:
        strcpy(func_name, "sg_api_memcpy");
        break;
      case BM_API_ID_MEMCPY_BYTE:
        strcpy(func_name, "sg_api_memcpy_byte");
        break;
      case BM_API_ID_MEMCPY_WSTRIDE:
        strcpy(func_name, "sg_api_memcpy_wstride");
        break;

      default:
        bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "Function not found, api_addr->f_id=%d\n", api_addr->f_id);
        return -1;
      }
      dlerror();
      f_ptr = (int (*)(void *, unsigned int))dlsym(lib_info.lib_handle, func_name);
      const char *dlsym_error = dlerror();
      if (dlsym_error) {
          fprintf(stderr, "dlsym error: %s\n", dlsym_error);
          continue;
      }
      if (f_ptr != NULL) {
        ret = f_ptr((void *)api_addr->param, api_addr->size);
        if (ret != 0) {
            perror("call_func_process call_func");
            return -1;
        }
        return 0;
      }
    }
  } else {
    for (; it != lib_table.end(); it++) {
        SGTPUV8_lib_info lib_info = it->second;
        if (lib_info.func_table.find(api_addr->f_id) != lib_info.func_table.end()) {
            struct exec_func func = lib_info.func_table[api_addr->f_id];
            bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "find func name----------- %s\n", func.func_name);
            if (api_addr->param == NULL) {
              bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO,"param is null\n");
            }
            if (func.f_ptr == NULL) {
              bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "func.f_ptr is null\n");
            }
            ret = func.f_ptr((void *)api_addr->param, api_addr->size);
            if (ret != 0) {
                perror("call_func_process call_func");
                return -1;
            }
            break;
        }
    }
  }

  if (it == lib_table.end()) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "Function not found\n");
      return -1;
  }
  return 0;
}

void usage_thread_cleanup_func(void *arg)
{
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "usage_thread_cleanup_func set usage as 0\n");
  int fd_file = open("/tmp/bmcpu_app_usage", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd_file < 0) {
      perror("File open failed");
      return;
  }
  dprintf(fd_file, "%d\n", 0);
  fsync(fd_file);
  close(fd_file);
}

void* timer_tpu_usage_thread(void* arg) {
    int fd_file = open("/tmp/bmcpu_app_usage", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_file < 0) {
        perror("File open failed");
        return NULL;
    }

    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0) {
        perror("Timer creation failed");
        close(fd_file);
        return NULL;
    }

    uint64_t interval_us = 5000;  // default 1000 us
    const char* interval_env = getenv("SET_TPU_WINDOWS");
    if (interval_env != nullptr) {
      uint64_t val = strtoull(interval_env, nullptr, 10);
      if (val > 0 && val <= 20000) {
        interval_us = val;
      } else {
        bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_WARNING, "Invalid value for SET_TPU_WINDOWS(0~20000), using default value 1000 us\n");
      }
    }
    int interval_write = 0; // every interval_write, write to file
    int average_tpu_usage = 0;
    // open /tmp/tpu_usage.log
    int fd_tmp = open("/tmp/tpu_usage.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_tmp < 0) {
        perror("File open failed");
        return NULL;
    }

    pthread_cleanup_push(usage_thread_cleanup_func, NULL);
    while (1) {
        uint64_t exp;
        usleep(interval_us);
        uint64_t tpu_start = start_bm_profile_timestamp;
        uint64_t tpu_end = 0;
        uint64_t window_end = get_timestamp_us();
        interval_write += interval_us / 1000;

        if (flag==1) {
            tpu_end = get_timestamp_us();
        } else {
            tpu_end = end_bm_profile_timestamp;
        }

        uint64_t current_timestamp = window_end - interval_us;
        int tpu_usage = 0;
        // printf("tpu_start: %llu, tpu_end: %llu, current_timestamp: %llu, window_end: %llu, caculate_time: %llu\n",
              //  tpu_start, tpu_end, current_timestamp, window_end, (tpu_end - tpu_start));

        // 1. The time window is completely before TPU execution
        if (tpu_start >= window_end) {
            tpu_usage = 0;
        }
        // 2. The time window is completely after TPU execution
        else if (tpu_end <= current_timestamp) {
            tpu_usage = 0;
        }
        // 3. The TPU execution completely covers the time window
        else if (tpu_start <= current_timestamp && tpu_end >= window_end) {
            tpu_usage = 100;
        }
        // 4. Partial overlap
        else {
            uint64_t overlap_start = (tpu_start > current_timestamp) 
                                    ? tpu_start : current_timestamp;
            uint64_t overlap_end = (tpu_end < window_end) 
                                  ? tpu_end : window_end;

            if (overlap_end > overlap_start) {
                uint64_t overlap_duration = overlap_end - overlap_start;
                tpu_usage = (overlap_duration * 100) / interval_us;
            } else {
                tpu_usage = 0;
            }
        }
        if (average_tpu_usage == 0 && tpu_usage != 0) {
            average_tpu_usage = tpu_usage;
            // “TPU usage average_tpu_usage”
            dprintf(fd_tmp, "tpu_usage: %d\n", average_tpu_usage);
            fsync(fd_tmp);
        } else {
            average_tpu_usage = (average_tpu_usage + tpu_usage) / 2;
            // “TPU usage average_tpu_usage”
            dprintf(fd_tmp, "tpu_usage: %d\n", average_tpu_usage);
            fsync(fd_tmp);
        }
        // printf("TPU usage in the last 0.005 seconds: %d%%\n", tpu_usage);
        if (interval_write >= 1000) {
          // caculate avrage tpu usage
          lseek(fd_file, 0, SEEK_SET);
          dprintf(fd_file, "%d\n", average_tpu_usage);
          fsync(fd_file);
          dprintf(fd_tmp, "=============interval:1s==========\n");
          fsync(fd_tmp);
          interval_write = 0;
          average_tpu_usage = 0;
        }
    }

    pthread_cleanup_pop(1);
    close(tfd);
    close(fd_file);
    return NULL;
}


void* bmcpu_thread(void* arg) {
  bm_api_to_bmcpu_t *bm_api;
  bm_api_cpu_load_library_internal_t *api;
  bm1688_get_func_internal_t *bm_api_func;
  bm1688_launch_func_internal *bm_api_launch;
  u32 ret = 0;


  bm_api = (bm_api_to_bmcpu_t *)malloc(sizeof(bm_api_to_bmcpu_t));
  start_bm_profile_timestamp = get_timestamp_us();
  end_bm_profile_timestamp = start_bm_profile_timestamp;

  // Check the environment variable SHOW_TPU_USAGE, if set, start the timer_tpu_usage_thread
  const char *env_var = getenv("SHOW_TPU_USAGE");
  if (env_var != NULL && strcmp(env_var, "1") == 0) {
      pthread_t tid;
      if (pthread_create(&tid, NULL, timer_tpu_usage_thread, NULL) != 0) {
          perror("Failed to create timer thread");
      }

      struct sched_param param;
      param.sched_priority = 80;
      int ret = pthread_setschedparam(tid, SCHED_FIFO, &param);
      if (ret != 0) {
          errno = ret;
          perror("pthread_setschedparam");
      }
      pthread_detach(tid);
  }

  int *fd = (int *)arg;

  ret = mmap_tpu_sys(*fd);
  if (ret != 0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "mmap_tpu_sys fail");
  }

  while (1) {
      bm_ret ret_struct;

      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "waiting for API\n");
      pthread_mutex_lock(&uncomplete_msg_mtx);
      while (uncomplete_msg_queue.empty()) {
        pthread_cond_wait(&uncomplete_msg_cv, &uncomplete_msg_mtx);
      }
      *bm_api = uncomplete_msg_queue.front();
      uncomplete_msg_queue.pop();
      pthread_mutex_unlock(&uncomplete_msg_mtx);

      // bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "recive mq_size1: %d \n", mq_size);
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "API ID: 0x%x \n" , bm_api->api_id);
      //A timer to record the API usage time
      auto start = std::chrono::high_resolution_clock::now();
      bm_profile.sent_api_counter++;

      switch (static_cast<unsigned int>(bm_api->api_id)) {
      case API_ID_SGTPUV8_LOAD_LIB:
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "===========load lib!===========\n");
          api = (bm_api_cpu_load_library_internal_t *)bm_api->api_data;
          ret = load_lib_process(api);
          break;
      case API_ID_SGTPUV8_UNLOAD_LIB:
          api = (bm_api_cpu_load_library_internal_t *)bm_api->api_data;
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "===========unload lib!==========\n");
          ret = unload_lib_process(api);
          break;
      case API_ID_SGTPUV8_LAUNCH_FUNC:
          start_bm_profile_timestamp= get_timestamp_us();
          flag= 1;
          bm_api_launch = (bm1688_launch_func_internal *)bm_api->api_data;
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "==========launch func!=========\n");
          ret = call_func_process(bm_api_launch);
          flag= 0;
          end_bm_profile_timestamp = get_timestamp_us();
          break;
      case API_ID_SGTPUV8_GET_FUNC:
          bm_api_func = (bm1688_get_func_internal_t *)bm_api->api_data;
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "===========get func!===========\n");
          ret = get_func_process(bm_api_func);
          break;
      default:
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "==========unknown API==========\n");
          continue;
          break;
      }

      bm_profile.completed_api_counter++;

      auto end = std::chrono::high_resolution_clock::now();
      /*
        duration should be the corresponding c language of u32 type：
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        duration = (end_time.tv_sec - start_time.tv_sec)*1000000 + (end_time.tv_nsec - start_time.tv_nsec)/1000;
      */
      u32 duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      //Return value structure
      ret_struct.result = ret;
      ret_struct.sem_key = bm_api->sem_key;
      ret_struct.duration = duration;
      ret_struct.tid = bm_api->tid;

      if (bm_api->api_id == API_ID_SGTPUV8_LAUNCH_FUNC)
          bm_profile.tpu_process_time += duration;

      if (bm_api->api_id == API_ID_SGTPUV8_GET_FUNC) {
          snprintf(ret_struct.msg, sizeof(ret_struct.msg), "find_func_id value: %d\n", find_func_id);
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "find_func_id value: %d\n", find_func_id);
      }
      pthread_mutex_lock(&complete_msg_mtx);
      complete_msg_queue.push_back(ret_struct);
      pthread_mutex_unlock(&complete_msg_mtx);
      //Wake up the API caller
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "*************API ID: 0x%x done **************\n", bm_api->api_id);
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "*************ret: %d **************\n", ret);
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "*************duration: %d **************\n", duration);

      // The semaphore is released here, and the API caller can continue to execute
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_DEBUG, "ret_struct.sem_key:%ld\n", ret_struct.sem_key);
      int semid = semget(ret_struct.sem_key, 1, IPC_CREAT | 0666);
      //bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_DEBUG, "===========sem value = %d\n", semctl(semid, 0, GETVAL));
      if (semid == -1) {
          perror("semget");
          return nullptr;
      }
      struct sembuf sb = {0, 1, 0};
      if (semop(semid, &sb, 1) == -1) {
          perror("bmcpu semop");
      }
  }

  for (auto &mapping : mapped_memory_list) {
    munmap(mapping.first, mapping.second);
  }
}
