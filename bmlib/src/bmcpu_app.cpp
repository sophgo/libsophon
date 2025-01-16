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

#include <iostream>
#include <thread>
#include <mqueue.h>
#include <semaphore.h>
#include <map>
#include <dlfcn.h>
#include <sys/sem.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "bmlib_runtime.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include "bmcpu_app.h"

typedef uint8_t u8;
typedef uint32_t u32;
static int f_id = 22;
#define BMLIB_bmcpu_LOG_TAG "bmcpu_app"

// typedef struct bm_api_to_bmcpu {
//     bm_handle_t handle;
//     int api_id;
//     const uint8_t* api_addr;
//     uint32_t api_size;
//     char* sem_name;
//     std::thread::id tid;
// } bm_api_to_bmcpu_t;
// #define MD5SUM_LEN 16
// #define LIB_MAX_NAME_LEN 64
// #define FUNC_MAX_NAME_LEN 64

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

// #define QUEUE1_NAME "/msg_fifo_uncomplete_bmcpu"  // msg_fifo name
// #define QUEUE2_NAME "/msg_fifo_complete_bmcpu"  // msg_fifo name
struct exec_func {
  int f_id;
  int (*f_ptr)(void *, unsigned int);
  unsigned char func_name[FUNC_MAX_NAME_LEN];
};

//A linked list records the mapped void *mapped_memory and the int size corresponding to mapped_memory
std::vector<std::pair<void *, size_t>> mapped_memory_list;

struct SGTPUV8_lib_info {
  void *lib_handle;
  char lib_name[LIB_MAX_NAME_LEN];
  unsigned char md5[MD5SUM_LEN];
  std::map<int, struct exec_func> func_table;
};
//A mapping table, md5 value corresponds to SGTPUV8_lib_info
std::map<std::array<unsigned char, MD5SUM_LEN>, SGTPUV8_lib_info> lib_table;

#define BMDEV_SET_IOMAP_TPYE             _IOWR('p', 0x99, u_int)
#define DEVICE_FILE "/dev/bm-tpu0"
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
  int ret=ioctl(fd, BMDEV_SET_IOMAP_TPYE, type);
  if (ret!=0){
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "ioctl failed\n");
  }
  // Request mmap, passing the starting address and size
  void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_memory == MAP_FAILED) {
      perror("mmap failed");
      close(fd);
      return NULL;
  }
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO,"mapped_memory: %p\n", mapped_memory);

  // Read the value of this register
  // printf("The value of the register is: %x\n", *((unsigned char *)mapped_memory));

  // Using mapped memory
  // *((int *)mapped_memory) = 0xff;
  // printf("-------------------\n");
  // printf("The value of the register is: %x\n", *((unsigned char *)mapped_memory));


  // if (munmap(mapped_memory, size) < 0) {
  // 		perror("munmap failed");
  // }

  mapped_memory_list.emplace_back(mapped_memory, size);
  return mapped_memory;

}

static int mmap_tpu_sys(int fd){
  // void *mmap;
  int result=0;

  mmap_gdma=mmap_paddr_to_vaddr(0, TPU_GDMA_SIZE,fd);
  if(mmap==NULL){
    printf("GDMA mmap failed\n");
    return -1;
  }

  mmap_sys=mmap_paddr_to_vaddr(1, TPU_SYS_SIZE, fd);
  if(mmap==NULL){
    printf("SYS mmap failed\n");
    return -1;
  }

  mmap_reg=mmap_paddr_to_vaddr(2, TPU_REG_SIZE, fd);
  if(mmap==NULL){
    printf("REG mmap failed\n");
    return -1;
  }

  mmap_smem=mmap_paddr_to_vaddr(3, TPU_SMEM_SIZE, fd);
  if(mmap==NULL){
    printf("SMEM mmap failed\n");
    return -1;
  }

  mmap_lmem=mmap_paddr_to_vaddr(4, TPU_LMEM_SIZE, fd);
  if (mmap==NULL) {
    printf("LMEM mmap failed\n");
    return -1;
  }

  return 0;
}

static int mmap_addr_to_firmwarecore(void* handle){
  // void *mmap;
  int result=0;
  dlerror();
  int (*CallPhysicalToVirtual)(void *, int) = (int (*)(void *, int))dlsym(handle, "PhysicalToVirtual");
  const char *dlsym_error = dlerror();
  if (dlsym_error) {
    fprintf(stderr, "Cannot load symbol 'CallPhysicalToVirtual': %s\n", dlsym_error);
    dlclose(handle);
    return 1;
  }

  result = CallPhysicalToVirtual(mmap_gdma, 0);
  if(result!=0){
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_sys, 1);
  if(result!=0){
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_reg, 2);
  if(result!=0){
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_smem, 3);
  if(result!=0){
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_lmem, 4);
  if(result!=0){
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  return 0;
}

static int load_table_to_firmwarecore(void* handle){
  dlerror();
  void (*CallFunc)() = (void (*)())dlsym(handle, "load_lookup_tables");
  const char *dlsym_error = dlerror();
  if (dlsym_error) {
      fprintf(stderr, "Cannot load symbol 'CallPhysicalToVirtual': %s\n", dlsym_error);
      dlclose(handle);
      return -1;
  }

  try {
    CallFunc();
  } catch (const std::exception &e) {
    std::cout << "load_lookup_tables failed: " << e.what() << std::endl;
    return -1;
  }
  // bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO,"load_lookup_tables success\n");
  return 0;
}


//4 functions, corresponding to 4 APIs
static int load_lib_process(bm_api_cpu_load_library_internal_t* api) {
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Name: %s\n", api->library_name);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Path: %s\n", api->library_path);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Size: %d\n", api->size);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "MD5: ");
  for (int i = 0; i < MD5SUM_LEN; i++) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "%d", (int)api->md5[i]);
  }
  std::cout << std::endl;
  std::cout << std::dec;

  std::array<unsigned char, MD5SUM_LEN> api_md5;
  std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());

  if (lib_table.find(api_md5) != lib_table.end()) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library %s\n", api->library_path);
    return -1;
  } else {
    void* handle = dlopen((char*)api->library_path, RTLD_LAZY);
    if (handle == NULL) {
        perror("dlopen");
        return -1;
    }
    SGTPUV8_lib_info lib_info;
    lib_info.lib_handle = handle;
    memcpy(lib_info.md5, api->md5, MD5SUM_LEN);
    memcpy(lib_info.lib_name, api->library_name, LIB_MAX_NAME_LEN);

    lib_table[api_md5] = lib_info;
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library %s loaded!\n", api->library_name);
  }

  if(mmap_addr_to_firmwarecore(lib_table[api_md5].lib_handle)){
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "mmap_addr_to_firmwarecore failed\n");
  }

  if(load_table_to_firmwarecore(lib_table[api_md5].lib_handle)){
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "load_table_to_firmwarecore failed\n");
  }

  return 0;
}

static int unload_lib_process(bm_api_cpu_load_library_internal_t* api) {
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library Name: %s\n", api->library_name);
  // std::cout << "Library Path: " << api->library_path << std::endl;
  // std::cout << "Library Size: " << api->size << std::endl;
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "MD5: ");
  for (int i = 0; i < MD5SUM_LEN; i++) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO,"%d", (int)api->md5[i]);
  }
  std::cout << std::endl;
  std::cout << std::dec;

  std::array<unsigned char, MD5SUM_LEN> api_md5;
  std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());
  if (lib_table.find(api_md5) == lib_table.end()) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Library %s not loaded\n", api->library_name);
      return -1;
  }
  else {
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
  std::cout << std::endl;
  std::cout << std::dec;

  std::array<unsigned char, MD5SUM_LEN> api_md5;
  std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());
  if (lib_table.find(api_md5) == lib_table.end()) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "Library not loaded");
      return -1;
  }
  else {
      SGTPUV8_lib_info lib_info = lib_table[api_md5];
      struct exec_func func;

      if (lib_info.func_table.find(api->f_id) != lib_info.func_table.end()) {
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Function already loaded\n");
          return 0;
      }
      dlerror();
      func.f_ptr = (int (*)(void *, unsigned int))dlsym(lib_info.lib_handle, (char*)api->func_name);

      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO,"func.f_ptr: %p\n", func.f_ptr);
      const char *dlsym_error = dlerror();
      if (dlsym_error) {
          fprintf(stderr, "dlsym error: %s\n", dlsym_error);
          return -1;
      }
      f_id = (f_id+1) % 100000;
      func.f_id = f_id;
      memcpy(func.func_name, api->func_name, FUNC_MAX_NAME_LEN);
      lib_info.func_table[f_id] = func;
      lib_table[api_md5] = lib_info;
  }
  return BM_SUCCESS;
}

static int call_func_process(bm1688_launch_func_internal* api_addr) {
  int ret=0;
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Function ID: %d\n", api_addr->f_id);
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "Function Size: %d\n", api_addr->size);;

  auto it = lib_table.begin();
  for (; it != lib_table.end(); it++) {
      SGTPUV8_lib_info lib_info = it->second;
      if (lib_info.func_table.find(api_addr->f_id) != lib_info.func_table.end()) {
          struct exec_func func = lib_info.func_table[api_addr->f_id];
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "find func name----------- %s\n", func.func_name);
          if(api_addr->param==NULL){
            std::cout<<"param is null"<<std::endl;
          }
          if(func.f_ptr==NULL){
            bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "func.f_ptr is null\n");
          }
          // bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "launch func**************111\n");
          ret = func.f_ptr((void *)api_addr->param, api_addr->size);
          // std::cout << "launch func over**************" << std::endl;
          if (ret != 0) {
              perror("call_func");
              return -1;
          }
          break;
      }
  }

  if (it == lib_table.end()) {
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "Function not found\n");
      return -1;
  }
  return 0;
}


void* bmcpu_thread(void* arg) {
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 20;
  attr.mq_msgsize = sizeof(bm_api_to_bmcpu_t);
  attr.mq_curmsgs = 0;
  mqd_t uncomplete_mq = mq_open(QUEUE1_NAME, O_RDONLY | O_CREAT, 0644, &attr);
  struct mq_attr attr2;
  attr2.mq_flags = 0;
  attr2.mq_maxmsg = 20;
  attr2.mq_msgsize = sizeof(bm_ret);
  attr2.mq_curmsgs = 0;
  mqd_t complete_mq = mq_open(QUEUE2_NAME, O_WRONLY | O_CREAT, 0644, &attr2);

  if (uncomplete_mq == (mqd_t)-1 || complete_mq == (mqd_t)-1) {
      perror("mq_open");
      return nullptr;
  }

  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "bm_api_to_bmcpu_t size: %d\n", sizeof(bm_api_to_bmcpu_t));
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "bm_ret size: %d\n", sizeof(bm_ret));
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "bm_api_cpu_load_library_internal_t size: %d\n", sizeof(bm_api_cpu_load_library_internal_t));
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "_launch_func_internal_t size: %d\n", sizeof(bm1688_launch_func_internal_t));
  bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "_get_func_internal_t size: %d\n", sizeof(bm1688_get_func_internal_t));
  bm_api_to_bmcpu_t *bm_api;
  bm_api_cpu_load_library_internal_t *api;
  bm1688_get_func_internal_t *bm_api_func;
  bm1688_launch_func_internal *bm_api_launch;
  u32 ret = 0;

  int *fd = (int *)arg;
  ret = mmap_tpu_sys(*fd);
  if (ret != 0) {
    bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_ERROR, "mmap_tpu_sys fail");
  }

  char *buffer;
  buffer = (char *)malloc(attr.mq_msgsize);
  while (1) {
      bm_ret ret_struct;

      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "waiting for API\n" );
      ssize_t mq_size = mq_receive(uncomplete_mq, buffer, attr.mq_msgsize, NULL);
      if (uncomplete_mq == (mqd_t)-1) {
          perror("mq_receive");
          return nullptr;
      }
      // if (mq_size == -1) {
      // 	continue;
      // }
      bm_api=(bm_api_to_bmcpu_t *)buffer;

      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "recive mq_size1: %d \n", mq_size);
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "API ID: 0x%x \n" , bm_api->api_id);
      std::cout << std::dec;
      //A timer to record the API usage time
      auto start = std::chrono::high_resolution_clock::now();

      switch (bm_api->api_id) {
      case API_ID_SGTPUV8_LOAD_LIB:
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "===========load lib!===========\n");
          api=(bm_api_cpu_load_library_internal_t *)bm_api->api_data;
          ret = load_lib_process(api);
          break;
      case API_ID_SGTPUV8_UNLOAD_LIB:
          api=(bm_api_cpu_load_library_internal_t *)bm_api->api_data;
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "===========unload lib!==========\n");
          ret = unload_lib_process(api);
          break;
      case API_ID_SGTPUV8_LAUNCH_FUNC:
          bm_api_launch=(bm1688_launch_func_internal *)bm_api->api_data;
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "==========launch func!=========\n");
          ret = call_func_process(bm_api_launch);
          break;
      case API_ID_SGTPUV8_GET_FUNC:
          bm_api_func=(bm1688_get_func_internal_t *)bm_api->api_data;
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "===========get func!===========\n");
          ret = get_func_process(bm_api_func);
          break;
      default:
          bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "==========unknown API==========\n");
          continue;
          break;
      }


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
      if(bm_api->api_id==API_ID_SGTPUV8_GET_FUNC){
        snprintf(ret_struct.msg, sizeof(ret_struct.msg), "f_id value: %d\n", f_id);
        bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "f_id value: %d\n", f_id);
      }

      //Put the results into the completed queue
      mq_send(complete_mq, (char*)&ret_struct, sizeof(bm_ret), 0);
      if (complete_mq == (mqd_t)-1) {
          perror("mq_send");
          return nullptr;
      }
      //Wake up the API caller
      //Print debugging information
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "*************API ID: 0x%x done **************\n", bm_api->api_id);
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "*************ret: %d **************\n", ret);
      bmlib_log(BMLIB_bmcpu_LOG_TAG, BMLIB_LOG_INFO, "*************duration: %d **************\n", duration);
      std::cout << std::dec;
      // sem_t *sem = sem_open(bm_api.sem_key, O_CREAT, 0644, 0);
      // if (sem == SEM_FAILED) {
      //     perror("sem_open");
      //     return;
      // }
      // sem_post(sem);
      // sem_close(sem);

      // The semaphore is released here, and the API caller can continue to execute
      // printf("ret_struct.sem_key:%ld\n",ret_struct.sem_key);
      int semid = semget(ret_struct.sem_key, 1, IPC_CREAT | 0666);
      if (semid == -1) {
          perror("semget");
          return nullptr;
      }
      struct sembuf sb = {0, 1, 0};
      semop(semid, &sb, 1);

  }

  mq_close(uncomplete_mq);
  mq_close(complete_mq);

  if (uncomplete_mq == (mqd_t)-1 || complete_mq == (mqd_t)-1) {
      perror("mq_close");
      return nullptr;
  }

  for (auto &mapping : mapped_memory_list) {
    munmap(mapping.first, mapping.second);
  }
}
/*
int main(int argc, char *argv[]) {
  // //Create a thread using the C++ thread library
  // std::thread t1(thread2);
  // //Waiting for a thread to finish
  // t1.join();
  //Using the pthread library in C language
  pthread_t mythread;
  if(pthread_create(&mythread, NULL, (void*(*)(void*))thread1, NULL) != 0) {
      perror("pthread_create");
      return -1;
  }
  //Waiting for a thread to finish
  if (pthread_join(mythread, NULL) != 0) {
      perror("pthread_join");
      return -1;
  }
  return 0;
}
*/