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


std::vector<std::pair<void *, size_t>> mapped_memory_list;

struct SGTPUV8_lib_info {
    void *lib_handle;
    char lib_name[LIB_MAX_NAME_LEN];
    unsigned char md5[MD5SUM_LEN];
    std::map<int, struct exec_func> func_table;
};

std::map<std::array<unsigned char, MD5SUM_LEN>, SGTPUV8_lib_info> lib_table;

#define BMDEV_SET_IOMAP_TPYE             _IOWR('p', 0x99, u_int)
#define DEVICE_FILE "/dev/bm-tpu0"
#define TPU_GDMA_SIZE     0x10000  // gdma size
#define TPU_SYS_SIZE      0x30000  // sys size
#define TPU_REG_SIZE      0x10000  // reg size
#define TPU_SMEM_SIZE      0x1000     // smem size aligned to 4096 
#define TPU_LMEM_SIZE      0x80000  // lmem size


static void *mmap_gdma;
static void *mmap_sys;
static void *mmap_reg;
static void *mmap_smem;
static void *mmap_lmem;

static void * mmap_paddr_to_vaddr(int type, size_t size, int fd) {
  int ret=ioctl(fd, BMDEV_SET_IOMAP_TPYE, type);
  if (ret!=0){
    printf("ioctl failed\n");
  }

  void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_memory == MAP_FAILED) {
      perror("mmap failed");
      close(fd);
      return NULL;
  }
  printf("mapped_memory: %p\n", mapped_memory);

  // printf("The value of the register is: %x\n", *((unsigned char *)mapped_memory));


  // *((int *)mapped_memory) = 0xff;
  // printf("-------------------\n");
  // printf("The value of the register is: %x\n", *((unsigned char *)mapped_memory));


  // if (munmap(mapped_memory, size) < 0) {
  // 		perror("munmap failed");
  // }

  mapped_memory_list.emplace_back(mapped_memory, size);
  return mapped_memory;

}

static int mmap_tpu_sys(){
  // void *mmap;
  int result=0;

  int fd = open(DEVICE_FILE, O_RDWR);
  if (fd < 0) {
      perror("Failed to open device");
      return -1;
  }

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
  if(mmap==NULL){
    printf("LMEM mmap failed\n");
    return -1;
  }

  close(fd);
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
      return -1;
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
  printf("load_lookup_tables success\n");
  return 0;
}

// static int load_table_to_firmwarecore(void* handle){
// 	dlerror();
// 	void (*CallFunc)(void *) = (void (*)(void *))dlsym(handle, "firmware_main");
// 	const char *dlsym_error = dlerror();
// 	if (dlsym_error) {
// 			fprintf(stderr, "Cannot load symbol 'CallPhysicalToVirtual': %s\n", dlsym_error);
// 			dlclose(handle);
// 			return -1;
// 	}

// 	void *param = NULL;
// 	try {
// 		CallFunc(param);
// 	} catch (const std::exception &e) {
// 		std::cout << "load_lookup_tables failed: " << e.what() << std::endl;
// 		return -1;
// 	}
// 	printf("load_lookup_tables success\n");
// 	return 0;
// }



static int load_lib_process(bm_api_cpu_load_library_internal_t* api) {
    std::cout << "Library Name: " << api->library_name << std::endl;
    std::cout << "Library Path: " << api->library_path << std::endl;
    std::cout << "Library Size: " << api->size << std::endl;
    std::cout << "MD5: ";
    for (int i = 0; i < MD5SUM_LEN; i++) {
        std::cout << std::hex << (int)api->md5[i];
    }
    std::cout << std::endl;
    std::cout << std::dec;

    std::array<unsigned char, MD5SUM_LEN> api_md5;
    std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());

    if (lib_table.find(api_md5) != lib_table.end()) {
      std::cout << "Library " << api->library_path << " already loaded" << std::endl;
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
      std::cout << "Library " << api->library_name << " loaded" << std::endl;
    }

    if(mmap_addr_to_firmwarecore(lib_table[api_md5].lib_handle)){
      std::cout << "mmap_addr_to_firmwarecore failed" << std::endl;
    }else{
      std::cout << "mmap_addr_to_firmwarecore success" << std::endl;
    }

    if(load_table_to_firmwarecore(lib_table[api_md5].lib_handle)){
      std::cout << "load_table_to_firmwarecore failed" << std::endl;
    }else{
      std::cout << "load_table_to_firmwarecore success" << std::endl;
    }



    // int fd = open(DEVICE_FILE, O_RDWR);
    // if (fd < 0) {
    //     perror("Failed to open device");
    //     return -1;
    // }

    // void * smem=mmap_paddr_to_vaddr(3, TPU_SMEM_SIZE,fd);
    // if(mmap==NULL){
    //   printf("GDMA mmap failed\n");
    //   return -1;
    // }

    // for(int i = 0; i < 32; i+=2){
    //   printf("smem:%02x\n", *((unsigned short*)(smem + i)));
    // }
    // printf("***************\n");
    // for(int i = 736; i < 736+256; i+=2){
    //   printf("smem:%02x\n", *((unsigned short*)(smem + i)));
    // }
    // close(fd);

    return 0;
  }

static int unload_lib_process(bm_api_cpu_load_library_internal_t* api) {
    std::cout << "Library Name: " << api->library_name << std::endl;
    // std::cout << "Library Path: " << api->library_path << std::endl;
    // std::cout << "Library Size: " << api->size << std::endl;
    std::cout << "MD5: ";
    for (int i = 0; i < MD5SUM_LEN; i++) {
        std::cout << std::hex << (int)api->md5[i];
    }
    std::cout << std::endl;
		std::cout << std::dec;

    std::array<unsigned char, MD5SUM_LEN> api_md5;
    std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());
    if (lib_table.find(api_md5) == lib_table.end()) {
        std::cout << "Library " << api->library_name << " not loaded" << std::endl;
        return -1;
    }
    else {
        int ret = dlclose(lib_table[api_md5].lib_handle);
        if (ret != 0) {
            perror("dlclose");
            return -1;
        }
        lib_table.erase(api_md5);
        std::cout << "Library " << api->library_name << " unloaded" << std::endl;
    }
    return 0;
}

static int get_func_process(bm1688_get_func_internal_t *api) {
    std::cout << "Core ID: " << api->core_id << std::endl;
    std::cout << "Function Name: " << api->func_name << std::endl;
    std::cout << "MD5: ";
    for (int i = 0; i < MD5SUM_LEN; i++) {
      std::cout << std::hex << (int)api->md5[i];
    }
    std::cout << std::endl;
    std::cout << std::dec;

    std::array<unsigned char, MD5SUM_LEN> api_md5;
    std::copy(std::begin(api->md5), std::end(api->md5), api_md5.begin());
    if (lib_table.find(api_md5) == lib_table.end()) {
      std::cout << "Library not loaded" << std::endl;
      return -1;
    }
    else {
      SGTPUV8_lib_info lib_info = lib_table[api_md5];
      struct exec_func func;
      if (lib_info.func_table.find(api->f_id) != lib_info.func_table.end()) {
          std::cout << "Function already loaded" << std::endl;
          return 0;
      }
      dlerror();
      func.f_ptr = (int (*)(void *, unsigned int))dlsym(lib_info.lib_handle, (char*)api->func_name);

      printf("func.f_ptr: %p\n", func.f_ptr);
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
    std::cout << "Function ID: " << api_addr->f_id << std::endl;
    std::cout << "Function Size: " << api_addr->size << std::endl;
    auto it = lib_table.begin();
    for (; it != lib_table.end(); it++) {
        SGTPUV8_lib_info lib_info = it->second;
        if (lib_info.func_table.find(api_addr->f_id) != lib_info.func_table.end()) {
            struct exec_func func = lib_info.func_table[api_addr->f_id];
            std::cout << "find func name-----------\n" << func.func_name << std::endl;

            if(api_addr->param==NULL){
              std::cout<<"param is null"<<std::endl;
            }
            if(func.f_ptr==NULL){
              std::cout<<"f_ptr is null"<<std::endl;
            }else{
              printf("func.f_ptr not null: %p\n", func.f_ptr);
            }
            std::cout << "launch func**************111" << std::endl;
            ret = func.f_ptr((void *)api_addr->param, api_addr->size);
            std::cout << "launch func over**************" << std::endl;
            if (ret != 0) {
                perror("call_func");
                return -1;
            }
            break;
        }
    }

    if (it == lib_table.end()) {
        std::cout << "Function not found" << std::endl;
        return -1;
    }
    return 0;
}


void bmcpu_thread() {
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
        return;
    }

    std::cout << "bm_api_to_bmcpu_t size: " << sizeof(bm_api_to_bmcpu_t) << std::endl;
    std::cout << "bm_ret size: " << sizeof(bm_ret) << std::endl;
    std::cout << "bm_api_cpu_load_library_internal_t size: " << sizeof(bm_api_cpu_load_library_internal_t) << std::endl;
    std::cout << "bm1688_launch_func_internal_t size: " << sizeof(bm1688_launch_func_internal_t) << std::endl;
    std::cout << "bm1688_get_func_internal_t size: " << sizeof(bm1688_get_func_internal_t) << std::endl;
    bm_api_to_bmcpu_t *bm_api;
    bm_api_cpu_load_library_internal_t *api;
    bm1688_get_func_internal_t *bm_api_func;
    bm1688_launch_func_internal *bm_api_launch;
    u32 ret = 0;

    ret=mmap_tpu_sys();
    if(ret!=0){
      std::cout<<"mmap_tpu_sys fail"<<std::endl;
    }

    char *buffer;
    buffer = (char *)malloc(attr.mq_msgsize);
    while (1) {
        bm_ret ret_struct;

        std::cout << "waiting for API" << std::endl;
        ssize_t mq_size = mq_receive(uncomplete_mq, buffer, attr.mq_msgsize, NULL);
        if (uncomplete_mq == (mqd_t)-1) {
            perror("mq_receive");
            return;
        }
        // if (mq_size == -1) {
        // 	continue;
        // }
        bm_api=(bm_api_to_bmcpu_t *)buffer;
        std::cout << "recive mq_size1: " << mq_size << std::endl;

        std::cout << "API ID: 0x" << std::hex << bm_api->api_id << std::endl;
        std::cout << std::dec;

        auto start = std::chrono::high_resolution_clock::now();

        switch (static_cast<unsigned int>(bm_api->api_id)) {
        case API_ID_SGTPUV8_LOAD_LIB:
            std::cout << "load lib!" << std::endl;
            api=(bm_api_cpu_load_library_internal_t *)bm_api->api_data;
            ret = load_lib_process(api);
            break;
        case API_ID_SGTPUV8_UNLOAD_LIB:
            api=(bm_api_cpu_load_library_internal_t *)bm_api->api_data;
            std::cout << "unload lib!" << std::endl;
            ret = unload_lib_process(api);
            break;
        case API_ID_SGTPUV8_LAUNCH_FUNC:
            bm_api_launch=(bm1688_launch_func_internal *)bm_api->api_data;
            std::cout << "launch func!" << std::endl;
            ret = call_func_process(bm_api_launch);
            break;
        case API_ID_SGTPUV8_GET_FUNC:
            bm_api_func=(bm1688_get_func_internal_t *)bm_api->api_data;
            std::cout << "get func!" << std::endl;
            ret = get_func_process(bm_api_func);
            break;
        default:
            std::cout << "unknown API" << std::endl;
            continue;
            break;
        }


        // std::cout << "API ID: 0x" << std::hex << bm_api.api_id << "prosessing" << std::endl;

        auto end = std::chrono::high_resolution_clock::now();

        u32 duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        ret_struct.result = ret;
        ret_struct.sem_key = bm_api->sem_key;
        ret_struct.duration = duration;
        ret_struct.tid = bm_api->tid;
        if(bm_api->api_id==API_ID_SGTPUV8_GET_FUNC){
          snprintf(ret_struct.msg, sizeof(ret_struct.msg), "f_id value: %d", f_id);
          printf("f_id value: %d\n", f_id);
        }


        mq_send(complete_mq, (char*)&ret_struct, sizeof(bm_ret), 0);
        if (complete_mq == (mqd_t)-1) {
            perror("mq_send");
            return;
        }

        std::cout << "*************API ID: 0x " << std::hex << bm_api->api_id << " done **************" << std::endl;
        std::cout << std::dec;
        // sem_t *sem = sem_open(bm_api.sem_key, O_CREAT, 0644, 0);
        // if (sem == SEM_FAILED) {
        //     perror("sem_open");
        //     return;
        // }
        // sem_post(sem);
        // sem_close(sem);

        printf("ret_struct.sem_key:%ld\n",ret_struct.sem_key);
        int semid = semget(ret_struct.sem_key, 1, IPC_CREAT | 0666);
        if (semid == -1) {
            perror("semget");
            return;
        }
        struct sembuf sb = {0, 1, 0};
        semop(semid, &sb, 1);
         
    }

    mq_close(uncomplete_mq);
    mq_close(complete_mq);

    if (uncomplete_mq == (mqd_t)-1 || complete_mq == (mqd_t)-1) {
        perror("mq_close");
        return;
    }

    for (auto &mapping : mapped_memory_list) {
      munmap(mapping.first, mapping.second);
    }
}

int main(int argc, char *argv[]) {
    pthread_t mythread;
    if(pthread_create(&mythread, NULL, (void*(*)(void*))bmcpu_thread, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }

    if (pthread_join(mythread, NULL) != 0) {
        perror("pthread_join");
        return -1;
    }
    return 0;
}