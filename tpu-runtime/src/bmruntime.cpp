#ifdef __linux__
#include <dlfcn.h>
#else
#include <windows.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux__
#include <unistd.h>
#include <atomic>
#endif
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <cmath>
#include <map>
#include "bmlib_runtime.h"

#ifdef _WIN32
#define BILLION (1E9)
static BOOL g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;

int bmrt_clock_gettime(int dummy, struct timespec* ct)
{
  LARGE_INTEGER count;

  if (g_first_time) {
    g_first_time = 0;

    if (0 == QueryPerformanceFrequency(&g_counts_per_sec)) {
      g_counts_per_sec.QuadPart = 0;
    }
  }

  if ((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) || (0 == QueryPerformanceCounter(&count))) {
    return -1;
  }

  ct->tv_sec = count.QuadPart / g_counts_per_sec.QuadPart;
  ct->tv_nsec =
      ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) / g_counts_per_sec.QuadPart;

  return 0;
}
#endif

int BMRT_LOG_LEVEL_THRESHOLD = 0;
namespace {
struct LogLevel {
    LogLevel() {
        const char *env_str = nullptr;
        if ((env_str = getenv("BMRT_LOG_VERSION")) != nullptr)
            BMRT_LOG_LEVEL_THRESHOLD = atoi(env_str);
        else
            BMRT_LOG_LEVEL_THRESHOLD = 0;
    }
};
static LogLevel log_level_init;
}

#ifndef __linux__
#include <stdarg.h>
#include "bmruntime_common.h"
static void bmrt_log_default_callback(int level, const char* fmt, va_list args)
{
  vprintf(fmt, args);
  printf("\n");
  if (level == FATAL)
    throw std::runtime_error("BMRuntime internal error.");
}

static void (*bmrt_log_callback)(int, const char*, va_list) = bmrt_log_default_callback;

extern void BMRT_LOG(int level, const char* fmt, ...)
{
  void (*log_callback)(int, const char*, va_list) = bmrt_log_callback;
  va_list args;

  if (log_callback) {
    va_start(args, fmt);
    log_callback(level, fmt, args);
    va_end(args);
  }
}
#endif

#include "bmruntime.h"

#define SWAP(a, b)   \
  do {               \
    (a) = (a) + (b); \
    (b) = (a) - (b); \
    (a) = (a) - (b); \
  } while (0)

std::map<std::string, void*> arch_map;
namespace bmruntime {

map<int, std::shared_ptr<BmCoeff>> Bmruntime::m_global_coeff_map;
std::mutex Bmruntime::m_global_coeff_mutex;

map<vector<u8>, std::unique_ptr<uint8_t[]>> Bmruntime::m_global_cpu_const_map;
std::mutex Bmruntime::m_global_cpu_const_mutex;

static void _reorder_bd_ins(uint8_t *cmdbuf)
{
  int total_bits = 112 * 8;

  for (int i = 0; i < total_bits; i += 128)
    cmdbuf[(i + 128 - 8) / 8] |= (i / 128) << 4;

  u8 tmp[128 / 8];
  u8 *last = &cmdbuf[(total_bits - 128) / 8];
  memcpy(tmp, last, sizeof(tmp));
  memcpy(last, cmdbuf, sizeof(tmp));
  memcpy(cmdbuf, tmp, sizeof(tmp));
}

typedef struct bm_device_mem_ext {
    bm_device_mem_t mem;
    Bmruntime *rt;
    bm_device_mem_ext(bm_device_mem_t mem, Bmruntime *rt) {
        this->mem = mem;
        this->rt = rt;
    }
    ~bm_device_mem_ext() {
        rt->must_free_device_mem(mem);
    }
} bm_device_mem_ext_t;

#ifdef _WIN32
static unsigned int bm_init_done = 0;
#else
static volatile std::atomic_flag bm_init_done = ATOMIC_FLAG_INIT;
#endif
static int bmruntime_lock(int sleep_us)
{
    struct timespec req = {0, sleep_us*1000};
#ifdef _WIN32
    while (InterlockedCompareExchange(&bm_init_done, 1, 0))
    {
        Sleep(1);
    }

#else
    while (std::atomic_flag_test_and_set((&bm_init_done)))
    {
        int ret = nanosleep(&req, NULL);
        if (ret < 0) /* interrupted by a signal handler */
        {
            std::atomic_flag_clear(&bm_init_done);
            return -1;
        }
    }
#endif
    return 0;
}
static void bmruntime_unlock()
{
#ifdef _WIN32
    InterlockedExchange(&bm_init_done, 0);
#else
    std::atomic_flag_clear(&bm_init_done);
#endif
}

void Bmruntime::init()
{
  m_device_mem_vec.clear();
  m_net_ctx_v.clear();
  m_net_ctx_v.reserve(MAX_NET_NUM);
  bmcpu_setup();
  bmtpu_setup();
  if (bmcpu_init_ != NULL) {
    bmcpu_handle_ = bmcpu_init_();
  }
  m_subnet_time_print = false;
  if (bmrt_arch_info::is_soc_mode()) {
    b_enable_mmap = true; /* soc default using mmap */
  } else {
    b_enable_mmap = false;
  }
  m_devid = bm_get_devid(m_handle);
  m_share_coeff = true;

#ifndef SOC_MODE
  // 1682 cmodel not support multi bm_handle
  if (bmrt_arch_info::get_bmtpu_arch() == BM1682) {
    m_share_coeff = false;
  }
#endif

  if (m_share_coeff) {
    std::lock_guard<std::mutex> guard(m_global_coeff_mutex);
    auto iter = m_global_coeff_map.find(m_devid);
    if (iter == m_global_coeff_map.end()) {
      m_local_coeff = std::make_shared<BmCoeff>(m_devid);
      m_global_coeff_map[m_devid] = m_local_coeff;
    } else if (iter->second == NULL) {
      m_local_coeff = std::make_shared<BmCoeff>(m_devid);
      iter->second = m_local_coeff;
    } else {
      m_local_coeff = iter->second;
    }
  } else {
    m_local_coeff = std::make_shared<BmCoeff>(m_handle);
  }
  // middle buffer
  bm_set_device_mem(&max_middle_buffer, 0, 0);
  max_middle_buffer_size = 0;
  middle_buffer_num = 0;

  auto neuron_heap_mask_env = std::getenv("BMRUNTIME_NEURON_HEAP_MASK");
  if (neuron_heap_mask_env)
  {
    try {
      m_neuron_heap_mask = std::stoi(neuron_heap_mask_env);
    } catch (std::invalid_argument &) {
      BMRT_LOG(FATAL, "invalid BMRUNTIME_NEURON_HEAP_MASK\"%s\"", neuron_heap_mask_env);
    }
  } else {
    m_neuron_heap_mask = 7;
  }
  m_profile = std::make_shared<BMProfile>(this);
}

void Bmruntime::init_bmfunc(const string& arch_name)
{
  /* modify for supporting difference arch.  */
  if(arch_map.find(arch_name) == arch_map.end()) {
    bmruntime_lock(200);
    if(arch_map.find(arch_name) == arch_map.end()) {
      p_bmfunc = new bmfunc(arch_name);
      arch_map[arch_name] = (void*)p_bmfunc;
    } else {
      p_bmfunc = (bmfunc*)arch_map[arch_name];
      bmrt_arch_info::set_current_arch_info(p_bmfunc->get_arch_info_ptr());
    }
    bmruntime_unlock();
  } else {
    p_bmfunc = (bmfunc*)arch_map[arch_name];
    bmrt_arch_info::set_current_arch_info(p_bmfunc->get_arch_info_ptr());
  }

#if 0   // origin code
  static bmfunc unify_bmfunc(arch_name); /* multi thread share 1 bmfunc */
  p_bmfunc = &unify_bmfunc;
#endif
}

/* using user parameter bm_handle */
Bmruntime::Bmruntime(bm_handle_t* bm_handle, bool user_initlized, const string& arch_name)
{
  init_bmfunc(arch_name);

  if (user_initlized == true)
    using_internal_bm_handle = false;
  else {
    bm_dev_request(bm_handle, 0);
    BMRT_ASSERT_INFO(bm_handle,"bm_handle shouldn't be NULL\n");
    using_internal_bm_handle = true;
  }

  m_handle = *bm_handle;
  init();
}

/* using internal initilized bm_handle ,with specific dev_id */
Bmruntime::Bmruntime(const string& arch_name, int devid)
{
  init_bmfunc(arch_name);

  bm_dev_request(&m_handle, devid);
  BMRT_ASSERT_INFO(m_handle,"m_handle shouldn't be NULL\n");
  using_internal_bm_handle = true;
  init();
}

Bmruntime::~Bmruntime()
{
  if (bmcpu_uninit_ != NULL) {
    bmcpu_uninit_(bmcpu_handle_);
  }

  for (auto& dev_mem : m_device_mem_vec) {
    BMRT_DEBUG("Free device memory, byte size %d\n", bm_mem_get_device_size(dev_mem));
    must_free_device_mem(dev_mem);
  }

  for (auto net_ctx : m_net_ctx_v) {
    subnet_clear(net_ctx);
    free_net_info(net_ctx);
    delete []net_ctx->stage_v[0];
    delete net_ctx;
  }

  // if this is last bmruntime, free coeff mem
  if (m_share_coeff) {
    std::lock_guard<std::mutex> guard(m_global_coeff_mutex);
    m_local_coeff = NULL;
    auto iter = m_global_coeff_map.find(m_devid);
    if (iter->second.unique()) {
      iter->second = NULL;
    }
  } else {
    m_local_coeff = NULL;
  }

  if (using_internal_bm_handle) {
    bm_dev_free(m_handle);
  }
}

static bool is_cmodel_mode(){
     #ifdef __linux__
     return dlsym(nullptr, "set_tpu_entry") != nullptr;
     #else
     return GetProcAddress(nullptr, "set_tpu_entry") != nullptr;
     #endif
}

typedef void (*set_tpu_entry_t)(void*, void*, void*);
typedef bm_status_t (*firmware_load_func_t)(bm_handle_t handle, const char *firmware_tcm, const char *firmware_ddr);

void Bmruntime::bmtpu_setup()
{
    auto firmware_path_c = getenv("BMRUNTIME_FIRMWARE_PATH");
    if(!firmware_path_c) return;
    BMRT_LOG(INFO, "Loading firmare from %s", firmware_path_c);
    string firmware_path = firmware_path_c;
    if(is_cmodel_mode()){
        // load *.so for cmodel weak symbol
        #ifdef __linux__
        set_tpu_entry_t set_entry = (set_tpu_entry_t)dlsym(nullptr, "set_tpu_entry");
        #else
        set_tpu_entry_t set_entry = (set_tpu_entry_t)GetProcAddress(nullptr, "set_tpu_entry");
        #endif
        string firmware_name = firmware_path + "/firmware.so";
        #ifdef __linux__
        auto fw_handle = dlopen(firmware_name.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if(!fw_handle){
            BMRT_LOG(WARNING, "fail to open %s: %s", firmware_name.c_str(), dlerror());
        }
        auto shape_ptr = dlsym(fw_handle, "tpu_shape_infer_entry");
        auto local_ptr = dlsym(fw_handle, "tpu_local_calculate_entry");
        auto global_ptr = dlsym(fw_handle, "tpu_global_calculate_entry");
        #else
        auto fw_handle = LoadLibrary(firmware_name.c_str());
        if (!fw_handle) {
          BMRT_LOG(WARNING, "fail to open %s: %s", firmware_name.c_str());
        }
        auto shape_ptr = GetProcAddress(fw_handle, "tpu_shape_infer_entry");
        auto local_ptr = GetProcAddress(fw_handle, "tpu_local_calculate_entry");
        auto global_ptr = GetProcAddress(fw_handle, "tpu_global_calculate_entry");
        #endif
        if(!shape_ptr || !local_ptr || !global_ptr){
            BMRT_LOG(WARNING, "fail to load entry function from", firmware_name.c_str());
            #ifdef __linux__
            dlclose(fw_handle);
            #else
            FreeLibrary(fw_handle);
            #endif
        }
        set_entry(shape_ptr, global_ptr, local_ptr);
    } else {
        string tcm_firmware = firmware_path + "/firmware_tcm.bin";
        string ddr_firmware = firmware_path + "/firmware_ddr.bin";
        #ifdef __linux__
        auto load_func = (firmware_load_func_t)dlsym(nullptr, "bmkernel_load_firmware");
        #else
        auto load_func = (firmware_load_func_t)GetProcAddress(nullptr, "bmkernel_load_firmware");
        #endif
        if(!load_func){
            BMRT_LOG(WARNING, "cannot find bmkernel_load_firmware function");
        } else if(load_func(m_handle, tcm_firmware.c_str(), ddr_firmware.c_str()) != BM_SUCCESS){
            BMRT_LOG(WARNING, "fail to load firmware: %s", tcm_firmware.c_str());
        }
    }
}


void Bmruntime::bmcpu_setup()
{
  #ifdef __linux__
  bmcpu_init_ = (t_bmcpu_init)dlsym(NULL, "bmcpu_init");
  bmcpu_uninit_ = (t_bmcpu_uninit)dlsym(NULL, "bmcpu_uninit");
  bmcpu_process_ = (t_bmcpu_process)dlsym(NULL, "bmcpu_process");
  #else
  bmcpu_init_ = (t_bmcpu_init)GetProcAddress(NULL, "bmcpu_init");
  bmcpu_uninit_ = (t_bmcpu_uninit)GetProcAddress(NULL, "bmcpu_uninit");
  bmcpu_process_ = (t_bmcpu_process)GetProcAddress(NULL, "bmcpu_process");
  #endif

  #ifdef __linux__
  if (bmcpu_process_ == NULL) {
    std::vector<const char*> cpu_libs = {
      "libbmcpu.so",
      "libcpuop.so"
    };
    void* cpu_so_handle_ = nullptr;
    for(auto cpu_lib: cpu_libs){
      cpu_so_handle_ = dlopen(cpu_lib, RTLD_LAZY);
      if(cpu_so_handle_) {
        BMRT_LOG(INFO, "cpu_lib '%s' is loaded.", cpu_lib);
        break;
      }
    }
    if (!cpu_so_handle_) {
      BMRT_LOG(WARNING, "Not able to open %s", "cpu.so");
      return;
    }
    bmcpu_init_ = (t_bmcpu_init)dlsym(cpu_so_handle_, "bmcpu_init");
    bmcpu_uninit_ = (t_bmcpu_uninit)dlsym(cpu_so_handle_, "bmcpu_uninit");
    bmcpu_process_ = (t_bmcpu_process)dlsym(cpu_so_handle_, "bmcpu_process");
  } else {
    BMRT_LOG(INFO, "cpu.so already exist, don't dlopen");
  }
  #else
  if (bmcpu_process_ == NULL) {
    std::vector<const char*> cpu_libs = {
      "libbmcpu.dll",
      "libcpuop.dll"
    };
    HMODULE cpu_so_handle_=NULL;
    for (auto cpu_lib: cpu_libs) {
      cpu_so_handle_ = LoadLibrary(cpu_lib);
      if(cpu_so_handle_) {
        BMRT_LOG(INFO, "cpu_lib '%s' is loaded.", cpu_lib);
        break;
      }
    }
    if (!cpu_so_handle_) {
      BMRT_LOG(WARNING, "Not able to open %s", "libbmcpu.dll");
      return;
    }
    bmcpu_init_ = (t_bmcpu_init)GetProcAddress(cpu_so_handle_, "bmcpu_init");
    bmcpu_uninit_ = (t_bmcpu_uninit)GetProcAddress(cpu_so_handle_, "bmcpu_uninit");
    bmcpu_process_ = (t_bmcpu_process)GetProcAddress(cpu_so_handle_, "bmcpu_process");
  } else {
    BMRT_LOG(INFO, "libbmcpu.dll already exist, don't dlopen");
  }
  #endif
}

u64 Bmruntime::fix_gdma_addr(const net_stage_t* stage, u64 origin_addr, bool is_src)
{
  if (origin_addr < CTX_START_ADDR) {
#ifdef SOC_MODE
    return origin_addr + bmrt_arch_info::get_gmem_offset_soc();
#else
    return origin_addr;
#endif
  }
  if (origin_addr < stage->ctx_start) {
    if (false == is_src) {
      BMRT_LOG(FATAL, "gdma dst shouldn't be coeff, origin[0x%llx], ctx[0x%llx]", origin_addr,
               stage->ctx_start);
    }
    return origin_addr + stage->coeff_offset;
  }
  return origin_addr + stage->ctx_offset[get_mem_index(stage->ctx_borders, stage->ctx_start, origin_addr)];
}

void Bmruntime::convert_cmd(u32* cmd, int engine_id, bool last_cmd, u64 start_address,
                            const net_stage_t* stage)
{
  ENGINE_ID id = (ENGINE_ID)(engine_id);
  BMRT_ASSERT_INFO(id == ENGINE_BD || id == ENGINE_GDMA,"id:%d should be ENGINE_BD:0 or ENGINE_GDMA:1\n",id);
  switch (bmrt_arch_info::get_bmtpu_arch()) {
    case BM1682:
      if (id == ENGINE_BD) {
        if (last_cmd)
          cmd[0] |= (1 << 7);  // cmd[0] |= (1 << BD_EOD_BIT);

        cmd[19] += (u32)(start_address >> BDC_ENGINE_CMD_ALIGNED_BIT);
        u32 cmd_idx;
        // shift the commands to the tail parts of buffer
        // Using ugly code to fix the strange order of bdc
        for (cmd_idx = 32 - 1; cmd_idx >= (32 - BD_ENGINE_COMMAND_NUM); cmd_idx--)
          cmd[cmd_idx] = cmd[cmd_idx - 32 + BD_ENGINE_COMMAND_NUM];

        for (cmd_idx = 0; cmd_idx < 32 - BD_ENGINE_COMMAND_NUM; cmd_idx++)
          cmd[cmd_idx] = 0;

        for (cmd_idx = 8; cmd_idx < 32; cmd_idx += 2)
          SWAP(cmd[cmd_idx], cmd[cmd_idx + 1]);

      } else { /* GDMA */
        if (last_cmd)
          cmd[0] |= (1 << 2);  // cmd[0] |= (1 << GDMA_ACCPI0_EOD_BIT);

        // if (append_mem_offset != 0 || bmrt_arch_info::is_soc_mode()) {
        u32 gdma_direction = (cmd[0] >> 6) & 0x3;
        u64 origin_addr, fix_addr;
        if (GDMA_DIR_S2L == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
          // if origin_addr is below ctx_start, needn't append
          origin_addr = (u64)cmd[12] + (((u64)(cmd[13] >> 24)) << 32);
          fix_addr = fix_gdma_addr(stage, origin_addr, true);
          if (fix_addr != origin_addr) {
            cmd[12] = fix_addr & 0xffffffff;
            cmd[13] = ((u32)(((fix_addr >> 32) & 0xff) << 24)) | (cmd[13] & 0x00ffffff);
          }
        }

        if (GDMA_DIR_L2S == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
          origin_addr = (u64)cmd[11] + (((u64)((cmd[13] >> 16) & 0xff)) << 32);
          fix_addr = fix_gdma_addr(stage, origin_addr, false);
          if (fix_addr != origin_addr) {
            cmd[11] = fix_addr & 0xffffffff;
            cmd[13] = ((u32)(((fix_addr >> 32) & 0xff) << 16)) | (cmd[13] & 0xff00ffff);
          }
        }
        cmd[GDMA_ENGINE_COMMAND_NUM] += (u32)(start_address >> GDMA_ENGINE_CMD_ALIGNED_BIT);
      }
      break;
    case BM1684:
      if (id == ENGINE_BD) {
        if (last_cmd)
          cmd[0] |= (1 << 1);  // BM1684
        /* special request: exchange cmd[i] and cmd[31-i] */
        for (u32 i = 0; i < BD_ENGINE_COMMAND_NUM_aligned / 2; ++i) {
          u32 tmp = cmd[i];
          cmd[i] = cmd[BD_ENGINE_COMMAND_NUM_aligned - 1 - i];
          cmd[BD_ENGINE_COMMAND_NUM_aligned - 1 - i] = tmp;
        }
      } else if (id == ENGINE_GDMA) {
        if (last_cmd)
          cmd[0] |= (1 << 2);  // BM1684

        u32 gdma_direction = (cmd[0] >> 6) & 0x3;
        u64 origin_addr = 0, fix_addr = 0;
        if (GDMA_DIR_S2L == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
          origin_addr = (u64)cmd[16] + (((u64)(cmd[18] & 0xff)) << 32);
          fix_addr = fix_gdma_addr(stage, origin_addr, true);
          if (fix_addr != origin_addr) {
            cmd[16] = fix_addr & 0xffffffff;
            cmd[18] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[18] & 0xffffff00);
          }
        }
        if (GDMA_DIR_L2S == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
          origin_addr = (u64)cmd[17] + (((u64)((cmd[18] >> 8) & 0xff)) << 32);
          fix_addr = fix_gdma_addr(stage, origin_addr, false);
          if (fix_addr != origin_addr) {
            cmd[17] = fix_addr & 0xffffffff;
            cmd[18] = ((u32)(((fix_addr >> 32) << 8) & 0xff00)) | (cmd[18] & 0xffff00ff);
          }
        }
      }
      break;
    case BM1880:
      if (id == ENGINE_BD) {
        if (last_cmd)
          cmd[0] |= (1 << 1);  // BM1880

        _reorder_bd_ins((uint8_t *)cmd);
      } else if (id == ENGINE_GDMA) {
        if (last_cmd)
          cmd[0] |= (1 << 2);  // BM1880

        u32 gdma_direction = (cmd[0] >> 6) & 0x3;
        u64 origin_addr, fix_addr;
        switch (gdma_direction) {
          case GDMA_DIR_S2L: /* TODO: here using little endian. */
            /* relocate src global address */
            origin_addr = (u64)cmd[0x30 / 4] + (((u64)((cmd[0x34 / 4] >> 24) && 0xff)) << 32);
            if(origin_addr == stage->ctx_start) {
              u32 idx = get_mem_index(stage->ctx_borders, stage->ctx_start, origin_addr);
              fix_addr = origin_addr + stage->ctx_offset[idx];
            } else {
              fix_addr = origin_addr + stage->coeff_offset;
            }

            cmd[0x30 / 4] = fix_addr & 0xffffffff;
            cmd[0x34 / 4] = ((u32)((fix_addr >> 32) & 0xff) << 24) | (cmd[0x34 / 4] & 0xffffff);
            break;
          case GDMA_DIR_L2S:
            /* relocate dst global address */
            origin_addr = (u64)cmd[0x2c / 4] + (((u64)((cmd[0x34 / 4] >> 16) && 0xff)) << 32);
            {
              u32 idx = get_mem_index(stage->ctx_borders, stage->ctx_start, origin_addr);
              fix_addr = origin_addr + stage->ctx_offset[idx];
            }

            cmd[0x2c / 4] = fix_addr & 0xffffffff;
            cmd[0x34 / 4] = ((u32)(((fix_addr >> 32) << 16) & 0xff0000)) | (cmd[0x34 / 4] & 0xff00ffff);
            break;
          case GDMA_DIR_S2S:
            /* relocate both src and dst global address */
            origin_addr = (u64)cmd[0x30 / 4] + (((u64)((cmd[0x34 / 4] >> 24) && 0xff)) << 32);
            fix_addr = fix_gdma_addr(stage, origin_addr, true);
            if (origin_addr != fix_addr) {
              cmd[0x30 / 4] = fix_addr & 0xffffffff;
              cmd[0x34 / 4] = ((u32)((fix_addr >> 32) & 0xff) << 24) | (cmd[0x34 / 4] & 0xffffff);
            }

            origin_addr = (u64)cmd[0x2c / 4] + (((u64)((cmd[0x34 / 4] >> 16) && 0xff)) << 32);
            if (origin_addr >= CTX_START_ADDR) {
              u32 idx = get_mem_index(stage->ctx_borders, stage->ctx_start, origin_addr);
              fix_addr = origin_addr + stage->ctx_offset[idx];
              cmd[0x2c / 4] = fix_addr & 0xffffffff;
              cmd[0x34 / 4] = ((u32)(((fix_addr >> 32) << 16) & 0xff0000)) | (cmd[0x34 / 4] & 0xff00ffff);
            }
            break;
          default:
            /* No optional process*/
            break;
        }
      }
      break;
    case BM1684X:
      if (id == ENGINE_GDMA && !last_cmd) {
        u64 src_addr = ((u64)(cmd[17] & 0xff) << 32) | ((u64)cmd[16]);
        u64 dst_addr = ((u64)(cmd[19] & 0xff) << 32) | ((u64)cmd[18]);
        bool src_in_global = src_addr >= GLOBAL_MEM_START_ADDR;
        bool dst_in_global = dst_addr >= GLOBAL_MEM_START_ADDR;
        u64 fix_addr;
        if (src_in_global) {
          fix_addr = fix_gdma_addr(stage, src_addr, true);
          if (fix_addr != src_addr) {
            cmd[16] = fix_addr & 0xffffffff;
            cmd[17] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[17] & 0xffffff00);
          }
        }
        if (dst_in_global) {
          fix_addr = fix_gdma_addr(stage, dst_addr, false);
          if (fix_addr != dst_addr) {
            cmd[18] = fix_addr & 0xffffffff;
            cmd[19] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[19] & 0xffffff00);
          }
        }
        // cmd type: 0:DMA_tensor, 1:DMA_matrix, 2:DMA_masked_select, 3:DMA_general
        // 4:DMA_cw_trans, 5:DMA_nonzero, 6:DMA_sys, 7:DMA_gather, 8:DMA_scatter
        // fix index_tensor or mask_tensor addr
        int cmd_type = (cmd[1] & 0x0f);
        if (cmd_type == 2 || cmd_type == 7 || cmd_type == 8) {
          u64 index_addr = ((u64)(cmd[21] & 0xff) << 32) | ((u64)cmd[20]);
          if (index_addr >= GLOBAL_MEM_START_ADDR) {
            fix_addr = fix_gdma_addr(stage, index_addr, true);
            if (fix_addr != index_addr) {
              cmd[20] = fix_addr & 0xffffffff;
              cmd[21] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[21] & 0xffffff00);
            }
          }
        }
      }
      break;
    case BM1686:
      if (id == ENGINE_GDMA && !last_cmd) {
        int cmd_type = (cmd[1] & 0x0f);
        if(cmd_type == 6) return; //cmd_type: DMA_sys
        u64 src_addr = ((u64)(cmd[17] & 0xff) << 32) | ((u64)cmd[16]);
        u64 dst_addr = ((u64)(cmd[19] & 0xff) << 32) | ((u64)cmd[18]);
        BMRT_ASSERT_INFO(((src_addr >> 36) & 0x7) == 0, "support tag != 0");
        BMRT_ASSERT_INFO(((dst_addr >> 36) & 0x7) == 0, "support tag != 0");
        bool src_in_global = (src_addr >> 39) & 0x1;
        bool dst_in_global = (dst_addr >> 39) & 0x1;
        u64 fix_addr;
        if (src_in_global) {
          fix_addr = fix_gdma_addr(stage, src_addr & ((1ull << 35) - 1), true);
          fix_addr |= (1ull << 39);
          if (fix_addr != src_addr) {
            cmd[16] = fix_addr & 0xffffffff;
            cmd[17] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[17] & 0xffffff00);
          }
        }
        if (dst_in_global) {
          fix_addr = fix_gdma_addr(stage, dst_addr & ((1ull << 35) - 1), false);
          fix_addr |= (1ull << 39);
          if (fix_addr != dst_addr) {
            cmd[18] = fix_addr & 0xffffffff;
            cmd[19] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[19] & 0xffffff00);
          }
        }
        // cmd type: 0:DMA_tensor, 1:DMA_matrix, 2:DMA_masked_select, 3:DMA_general
        // 4:DMA_cw_trans, 5:DMA_nonzero, 6:DMA_sys, 7:DMA_gather, 8:DMA_scatter
        // 9:DMA_reverse 10:DMA_compress 11: DMA_decompress
        // fix index_tensor or mask_tensor addr
        if (cmd_type == 2 || cmd_type == 7 || cmd_type == 8 || cmd_type == 0xa || cmd_type == 0xb) {
          u64 index_addr = ((u64)(cmd[21] & 0xff) << 32) | ((u64)cmd[20]);
          if ((index_addr >> 39) & 0x1) {
            fix_addr = fix_gdma_addr(stage, index_addr & ((1ull << 35) - 1), true);
            if (fix_addr != index_addr) {
              cmd[20] = fix_addr & 0xffffffff;
              cmd[21] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[21] & 0xffffff00);
            }
          }
        }
      }
      break;

    default:
      BMRT_LOG(FATAL, "Unkown BM TPU");
  }
}

bool Bmruntime::launch(int net_idx, const int input_num, const bm_device_mem_t* input_mems,
                       int* input_shapes, int* input_dims, int* in_stmode, int output_num,
                       const bm_device_mem_t* output_mems, int* out_stmode, bm_shape_t * output_shapes)
{
  // check parameters
    if (input_mems == NULL) {
        BMRT_LOG(WRONG, "input_mems is NULL");
        return false;
    }
    if (output_mems == NULL) {
        BMRT_LOG(WRONG, "output_mems is NULL");
        return false;
    }
    if (input_num != (int)m_net_ctx_v[net_idx]->input_name_v.size()) {
        BMRT_LOG(WRONG, "input_num is incorrect.");
        return false;
    }
    if (output_num == (int)m_net_ctx_v[net_idx]->output_name_v.size()) {
        BMRT_LOG(WRONG, "output_num is incorrect.");
        return false;
    }

  auto& net_ctx = m_net_ctx_v[net_idx];
  #ifdef __linux__
  bm_tensor_t input_tensors[input_num];
  #else
  std::shared_ptr<bm_tensor_t> input_tensors_(new bm_tensor_t[input_num],
                                              std::default_delete<bm_tensor_t[]>());
  bm_tensor_t* input_tensors = input_tensors_.get();
  #endif
  bool use_input_shape = (input_shapes != NULL && input_dims != NULL);
  for (int idx = 0, next_idx = 0; idx < input_num; idx++) {
    auto& user_input = input_tensors[idx];
    user_input.dtype = net_ctx->input_type_v[idx];
    user_input.st_mode = (in_stmode == NULL ? BM_STORE_1N : (bm_store_mode_t)in_stmode[idx]);
    user_input.device_mem = input_mems[idx];
    if (use_input_shape) {
      bmrt_shape(&user_input.shape, input_shapes + next_idx, input_dims[idx]);
      next_idx += input_dims[idx];
    } else {
      // only use stage 0
      user_input.shape = net_ctx->stage_v[0]->input_v[idx].shape;
    }
  }
  #ifdef __linux__
  bm_tensor_t output_tensors[output_num];
  #else
  std::shared_ptr<bm_tensor_t> output_tensors_(new bm_tensor_t[output_num],
                                               std::default_delete<bm_tensor_t[]>());
  bm_tensor_t* output_tensors = output_tensors_.get();
  #endif
  for (int idx = 0; idx < output_num; idx++) {
    output_tensors[idx].device_mem = output_mems[idx];
  }
  bool user_stmode = false;
  if (out_stmode != NULL) {
    user_stmode = true;
    for (int idx = 0; idx < output_num; idx++) {
      output_tensors[idx].st_mode = (bm_store_mode_t)(out_stmode[idx]);
    }
  }
  bool ret = launch(net_idx, input_tensors, input_num, output_tensors, output_num, true, user_stmode);
  // sync is needed for profile save data
  if (ret == true && m_profile->is_enabled()){
    ret = (BM_SUCCESS == bm_thread_sync(m_handle));
  }
  if (!ret) {
    BMRT_LOG(WRONG, "launch net[%d] failed", net_idx);
    return false;
  }
  if (output_shapes != NULL) {
    for (int idx = 0; idx < output_num; idx++) {
      output_shapes[idx] = output_tensors[idx].shape;
    }
  }
  return true;
}

bool Bmruntime::launch_ir(net_ctx_t* net_ctx, net_stage_t* stage,
                          const bm_tensor_t* input_tensors, int input_num,
                          bm_tensor_t* output_tensors, int output_num)
{
  bm_status_t status;
  auto arch = bmrt_arch_info::get_bmtpu_arch();

  #ifdef __linux__
  int *user_input_shapes[input_num];
  u64 user_input_global_addrs[input_num];
  u64 user_output_global_addrs[output_num];
  int input_dims[input_num];
  #else
  std::shared_ptr<int*> user_input_shapes_(new int*[input_num], std::default_delete<int*[]>());
  int** user_input_shapes = user_input_shapes_.get();
  std::shared_ptr<u64> user_input_global_addrs_(new u64[input_num], std::default_delete<u64[]>());
  u64* user_input_global_addrs = user_input_global_addrs_.get();
  std::shared_ptr<u64> user_output_global_addrs_(new u64[output_num], std::default_delete<u64[]>());
  u64* user_output_global_addrs = user_output_global_addrs_.get();
  std::shared_ptr<int> input_dims_(new int[input_num], std::default_delete<int[]>());
  int* input_dims = input_dims_.get();
  #endif
  // prepare inputs info
  for (int idx = 0; idx < input_num; idx++) {
    user_input_global_addrs[idx] = bm_mem_get_device_addr(input_tensors[idx].device_mem);
    user_input_shapes[idx] = (int*)input_tensors[idx].shape.dims;
    input_dims[idx] = input_tensors[idx].shape.num_dims;
    auto input_dtype = 0;
    if (arch == BM1684X || arch == BM1686) {
      input_dtype = input_tensors[idx].dtype;
    } else {
      if (input_tensors[idx].dtype == BM_FLOAT32) {
        input_dtype = 0; //DSIZE_FP32
      } else if (input_tensors[idx].dtype == BM_INT32) {
        input_dtype = 3; //DSIZE_INT32
      } else if(input_tensors[idx].dtype == BM_INT8 || input_tensors[idx].dtype == BM_UINT8){
          input_dtype = 2; //DSIZE_8
      } else if(input_tensors[idx].dtype == BM_INT16 || input_tensors[idx].dtype == BM_UINT16){
          input_dtype = 1; //DSIZE_16
      } else {
          BMRT_ASSERT_INFO(0 && "not supported input_dtype","not supported input_dtype,input_tensors[%d].dtype is %d",idx,input_tensors[idx].dtype);
      }
    }
    input_dims[idx] |= (input_dtype<<16);
  }

  for (int idx = 0; idx < output_num; idx++) {
    user_output_global_addrs[idx] = bm_mem_get_device_addr(output_tensors[idx].device_mem);
  }

  /* input and output store mode change need middle buffer in device memory */
  bool need_middle_buff_flag = false;
  #ifdef __linux__
  u64 user_input_global_addr_middle[input_num];
  u64 user_output_global_addr_middle[output_num];
  u32 output_need_middle_buff_flag[output_num];
  #else
  std::shared_ptr<u64> user_input_global_addr_middle_(new u64[input_num], std::default_delete<u64[]>());
  u64* user_input_global_addr_middle = user_input_global_addr_middle_.get();
  std::shared_ptr<u64> user_output_global_addr_middle_(new u64[output_num], std::default_delete<u64[]>());
  u64* user_output_global_addr_middle = user_output_global_addr_middle_.get();
  std::shared_ptr<u32> output_need_middle_buff_flag_(new u32[output_num], std::default_delete<u32[]>());
  u32* output_need_middle_buff_flag = output_need_middle_buff_flag_.get();
  #endif
  if (arch != BM1682) {
    // input, only 1N will switch to 4N
    int stmode_flag = 0;
    for (int idx = 0; idx < input_num; idx++) {
      bm_store_mode_t stmode = stage->input_v[idx].st_mode;
      bm_store_mode_t user_stmode = input_tensors[idx].st_mode;
      u64 middle_addr = bm_mem_get_device_addr(net_ctx->middlebuff_input[idx]);
      if (middle_addr == 0 || stmode == user_stmode) {
        user_input_global_addr_middle[idx] = 0;
        stmode_flag = ST_NO_CHANGE;
        input_dims[idx] |= (stmode_flag << 24);
        continue;
      }

      stmode_flag = get_stmode_flag(stmode, user_stmode, true);
      input_dims[idx] |= (stmode_flag << 24);

      user_input_global_addr_middle[idx] = middle_addr;
      need_middle_buff_flag = true;
      BMRT_ASSERT_INFO(stmode != user_stmode,"stmode:%d shouldn't equal to user_stmode:%d",stmode,user_stmode);
      BMRT_ASSERT_INFO(stage->input_v[idx].pad_h == 0, "stage->input_v[%d].pad_h:%d should be 0"\
        ,idx,stage->input_v[idx].pad_h);
    }
    // output
    for (int idx = 0; idx < output_num; idx++) {
      bm_store_mode_t stmode = stage->output_v[idx].st_mode;
      bm_store_mode_t user_stmode = output_tensors[idx].st_mode;
      u64 middle_addr = bm_mem_get_device_addr(net_ctx->middlebuff_output[idx]);
      if (stmode == user_stmode || middle_addr == 0) {
        user_output_global_addr_middle[idx] = 0;
        output_need_middle_buff_flag[idx] = ST_NO_CHANGE;
        continue;
      }

      user_output_global_addr_middle[idx] = middle_addr;
      output_need_middle_buff_flag[idx] = get_stmode_flag(stmode, user_stmode, false);
    }
  }

  // for multi-stage ir
  bm_device_mem_t output_shape_mem;
  u64 output_shape_global_addr =  must_alloc_device_mem(&output_shape_mem, output_num*sizeof(bm_shape_ex_t));
  bm_device_mem_ext_t output_shape_mem_ext(output_shape_mem, this);

  #ifdef __linux__
  int input_elem_num[input_num];
  #else
  std::shared_ptr<int> input_elem_num_(new int[input_num], std::default_delete<int[]>());
  int* input_elem_num = input_elem_num_.get();
  #endif
  memset(input_elem_num, 0, sizeof(int) * input_num); //setting to 0 means that does not need to count elem_num
  if (arch == BM1682) {
    status = bmfunc::bmdnn_1682()->_bmdnn_dynamic_fullnet_v2_(
        m_handle, stage->ir_mem.addr, stage->ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num, user_output_global_addrs,
        stage->ctx_start,
        // There is an assertion in bmruntime_bmodel.cpp to ensure ctx_offset
        // has one or none elements. So don't panic.
        stage->ctx_offset.empty() ? 0 : stage->ctx_offset[0],
        stage->coeff_offset, true, output_shape_global_addr,
        0  // no arm reserved buffer used
    );
  } else if (arch == BM1684) {
    status = bmfunc::bmdnn_1684()->_bmdnn_dynamic_fullnet_v2_(
        m_handle, stage->ir_mem.addr, stage->ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_global_addr_middle, user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, user_output_global_addr_middle, stage->ctx_start,
        stage->ctx_borders, stage->ctx_offset,
        stage->coeff_offset, need_middle_buff_flag, output_need_middle_buff_flag, true,
        output_shape_global_addr,
        0  // no arm reserved buffer used
    );
  } else if (arch == BM1684X) {
    int func_id = net_ctx->kernel_module_->get_dynamic_fullnet_func_id();
    status = bmfunc::bmdnn_1684x()->_bmdnn_dynamic_fullnet_(
        m_handle, func_id, stage->ir_mem.addr, stage->ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        stage->ctx_borders, stage->ctx_offset,
        stage->coeff_offset, true,
        output_shape_global_addr);
  } else if (arch == BM1686) {
    status = bmfunc::bmdnn_1686()->_bmdnn_dynamic_fullnet_(
        m_handle, stage->ir_mem.addr, stage->ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        stage->ctx_borders, stage->ctx_offset,
        stage->coeff_offset, true,
        output_shape_global_addr);
  } else {
    BMRT_LOG(FATAL, "Unknown BM TPU");
  }

  if (status == BM_SUCCESS) {
    status = bm_thread_sync(m_handle);
  }

  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "launch failed, status:%d", status);
    trace();
  }
  if (BM_SUCCESS == status) {
    // update output shape
    #ifdef __linux__
    bm_shape_ex_t output_shape_v[output_num];
    #else
    std::shared_ptr<bm_shape_ex_t> output_shape_v_(new bm_shape_ex_t[output_num], std::default_delete<bm_shape_ex_t[]>());
    bm_shape_ex_t* output_shape_v = output_shape_v_.get();
    #endif
    status = bm_memcpy_d2s(m_handle, output_shape_v, output_shape_mem);
    CHECK_status(status);
    for (int idx = 0; idx < output_num; idx++) {
      output_tensors[idx].shape = output_shape_v[idx].shape;
    }
  }

  return BM_SUCCESS == status;
}

bool Bmruntime::launch_static(net_ctx_t* net_ctx, net_stage_t* stage,
                              const bm_tensor_t* input_tensors, int input_num,
                              bm_tensor_t* output_tensors, int output_num)
{
  #ifdef __linux__
  u64 user_input_global_offset[input_num];
  u64 cmd_input_global_offset[input_num];

  int input_data_len[input_num];
  int input_n[input_num];
  int input_c[input_num];
  int input_h[input_num];
  int input_w[input_num];
  int input_length[input_num];
  unsigned short input_dtype[input_num];
  unsigned char input_stmode[input_num];
  unsigned char real_in_stmode[input_num];
  unsigned int input_pad_h[input_num];
  #else
  std::shared_ptr<u64> user_input_global_offset_(new u64[input_num], std::default_delete<u64[]>());
  u64* user_input_global_offset = user_input_global_offset_.get();
  std::shared_ptr<u64> cmd_input_global_offset_(new u64[input_num], std::default_delete<u64[]>());
  u64* cmd_input_global_offset = cmd_input_global_offset_.get();

  std::shared_ptr<int> input_data_len_(new int[input_num], std::default_delete<int[]>());
  int* input_data_len = input_data_len_.get();
  std::shared_ptr<int> input_n_(new int[input_num], std::default_delete<int[]>());
  int* input_n = input_n_.get();
  std::shared_ptr<int> input_c_(new int[input_num], std::default_delete<int[]>());
  int* input_c = input_c_.get();
  std::shared_ptr<int> input_h_(new int[input_num], std::default_delete<int[]>());
  int* input_h = input_h_.get();
  std::shared_ptr<int> input_w_(new int[input_num], std::default_delete<int[]>());
  int* input_w = input_w_.get();
  std::shared_ptr<int> input_length_(new int[input_num], std::default_delete<int[]>());
  int* input_length = input_length_.get();
  std::shared_ptr<unsigned short> input_dtype_(new unsigned short[input_num], std::default_delete<unsigned short[]>());
  unsigned short* input_dtype = input_dtype_.get();
  std::shared_ptr<unsigned char> input_stmode_(new unsigned char[input_num], std::default_delete<unsigned char[]>());
  unsigned char* input_stmode = input_stmode_.get();
  std::shared_ptr<unsigned char> real_in_stmode_(new unsigned char[input_num], std::default_delete<unsigned char[]>());
  unsigned char* real_in_stmode = real_in_stmode_.get();
  std::shared_ptr<unsigned int> input_pad_h_(new unsigned int[input_num], std::default_delete<unsigned int[]>());
  unsigned int* input_pad_h = input_pad_h_.get();
  #endif

  for (u32 idx = 0; idx < stage->input_v.size(); idx++) {
    auto& cmd_input = stage->input_v[idx];
    auto& user_input = input_tensors[idx];
    cmd_input_global_offset[idx] =
        bm_mem_get_device_addr(cmd_input.dev_mem) + GLOBAL_MEM_CMD_START_OFFSET;
    user_input_global_offset[idx] =
        bm_mem_get_device_addr(user_input.device_mem) + GLOBAL_MEM_CMD_START_OFFSET;

    input_data_len[idx] = bmrt_shape_count(&user_input.shape);
    input_n[idx] = user_input.shape.dims[0];
    input_c[idx] = user_input.shape.num_dims > 1 ? user_input.shape.dims[1] : 1;
    input_h[idx] = user_input.shape.num_dims > 2 ? user_input.shape.dims[2] : 1;
    input_w[idx] = 1;
    for (int s = 3; s < user_input.shape.num_dims; s++) {
      input_w[idx] *= user_input.shape.dims[s];
    }

    // input_length[idx] = max_c * max_h * max_w;
    input_length[idx] = 1;
    for (int s = 1; s < user_input.shape.num_dims; s++) {
      input_length[idx] *= user_input.shape.dims[s];
    }
    input_dtype[idx] = (unsigned short)user_input.dtype;
    input_stmode[idx] = (unsigned short)cmd_input.st_mode;
    BMRT_ASSERT_INFO(input_stmode[idx] == BM_STORE_1N || input_stmode[idx] == BM_STORE_4N,\
       "input_stmode[%d]:%d shouldn't be BM_STORE_2N\n", idx, input_stmode[idx]);
    real_in_stmode[idx] = user_input.st_mode;
    BMRT_ASSERT_INFO(real_in_stmode[idx] == BM_STORE_1N || real_in_stmode[idx] == BM_STORE_4N,\
     "real_in_stmode[%d] shouldn't be BM_STORE_2N\n", idx, real_in_stmode[idx]);

    // pad_h for conv 3ic(for BM1684)
    input_pad_h[idx] = stage->input_v[idx].pad_h;
  }

  #ifdef __linux__
  u64 user_output_global_offset[output_num];
  u64 cmd_output_global_offset[output_num];
  int output_n[output_num];
  int output_length[output_num];
  int output_data_len[output_num];
  unsigned short output_dtype[output_num];
  unsigned char output_stmode[output_num];
  unsigned char force_out_stmode[output_num];
  #else
  std::shared_ptr<u64> user_output_global_offset_(new u64[output_num], std::default_delete<u64[]>());
  u64* user_output_global_offset = user_output_global_offset_.get();
  std::shared_ptr<u64> cmd_output_global_offset_(new u64[output_num], std::default_delete<u64[]>());
  u64* cmd_output_global_offset = cmd_output_global_offset_.get();
  std::shared_ptr<int> output_n_(new int[output_num], std::default_delete<int[]>());
  int* output_n = output_n_.get();
  std::shared_ptr<int> output_length_(new int[output_num], std::default_delete<int[]>());
  int* output_length = output_length_.get();
  std::shared_ptr<int> output_data_len_(new int[output_num], std::default_delete<int[]>());
  int* output_data_len = output_data_len_.get();
  std::shared_ptr<unsigned short> output_dtype_(new unsigned short[output_num], std::default_delete<unsigned short[]>());
  unsigned short* output_dtype = output_dtype_.get();
  std::shared_ptr<unsigned char> output_stmode_(new unsigned char[output_num], std::default_delete<unsigned char[]>());
  unsigned char* output_stmode = output_stmode_.get();
  std::shared_ptr<unsigned char> force_out_stmode_(new unsigned char[output_num], std::default_delete<unsigned char[]>());
  unsigned char* force_out_stmode = force_out_stmode_.get();
  #endif
  for (u32 idx = 0; idx < stage->output_v.size(); idx++) {
    auto& cmd_output = stage->output_v[idx];
    auto& user_output = output_tensors[idx];
    cmd_output_global_offset[idx] =
        bm_mem_get_device_addr(cmd_output.dev_mem) + GLOBAL_MEM_CMD_START_OFFSET;

    user_output_global_offset[idx] =
        bm_mem_get_device_addr(user_output.device_mem) + GLOBAL_MEM_CMD_START_OFFSET;
    output_n[idx] = cmd_output.shape.num_dims == 0 ? 1 : cmd_output.shape.dims[0];
    // output_length[idx] = max_c * max_h * max_w;
    output_length[idx] = 1;
    for (int s = 1; s < cmd_output.shape.num_dims; s++) {
      output_length[idx] *= cmd_output.shape.dims[s];
    }
    output_data_len[idx] = output_n[idx] * output_length[idx];
    output_dtype[idx] = (unsigned short)user_output.dtype;
    output_stmode[idx] = (unsigned short)cmd_output.st_mode;
    BMRT_ASSERT_INFO(output_stmode[idx] == BM_STORE_1N || output_stmode[idx] == BM_STORE_4N,\
       "output_stmode[%d] shouldn't be BM_STORE_2N\n", idx, output_stmode[idx]);
    force_out_stmode[idx] = user_output.st_mode;
    BMRT_ASSERT_INFO(force_out_stmode[idx] == BM_STORE_1N || force_out_stmode[idx] == BM_STORE_4N, "force_out_stmode[%d] shouldn't be BM_STORE_2N\n", idx, force_out_stmode[idx]);
  }

  int group_num = stage->bdc_id.size();
  #ifdef __linux__
  int bdc_id_num[group_num];
  int gdma_id_num[group_num];
  int cdma_id_num[group_num]; /* useless, need been removed */
  u32 bdc_cmd_byte_size[group_num], gdma_cmd_byte_size[group_num];
  #else
  std::shared_ptr<int> bdc_id_num_(new int[group_num], std::default_delete<int[]>());
  int* bdc_id_num = bdc_id_num_.get();
  std::shared_ptr<int> gdma_id_num_(new int[group_num], std::default_delete<int[]>());
  int* gdma_id_num = gdma_id_num_.get();
  std::shared_ptr<int> cdma_id_num_(new int[group_num], std::default_delete<int[]>());
  int* cdma_id_num = cdma_id_num_.get();
  std::shared_ptr<u32> bdc_cmd_byte_size_(new u32[group_num], std::default_delete<u32[]>());
  u32* bdc_cmd_byte_size = bdc_cmd_byte_size_.get();
  std::shared_ptr<u32> gdma_cmd_byte_size_(new u32[group_num], std::default_delete<u32[]>());
  u32* gdma_cmd_byte_size = gdma_cmd_byte_size_.get();
  #endif
  for (int group_idx = 0; group_idx < group_num; group_idx++) {
    bdc_id_num[group_idx] = stage->bdc_id[group_idx];
    gdma_id_num[group_idx] = stage->gdma_id[group_idx];
    cdma_id_num[group_idx] = 0;
    bdc_cmd_byte_size[group_idx] = stage->bdc_cmd_byte[group_idx];
    gdma_cmd_byte_size[group_idx] = stage->gdma_cmd_byte[group_idx];
  }

  if(m_profile->is_enabled()){
      auto cmd_num = m_profile->record_subnet_cmd_info(stage->gdma_mem.addr, GLOBAL_MEM_CMD_START_OFFSET,
                                                       stage->bdc_mem.addr, GLOBAL_MEM_CMD_START_OFFSET,
                                                       group_num);
      for(int i=0; i<group_num; i++){
          cmd_num[i].bdc = bdc_id_num[i];
          cmd_num[i].gdma = gdma_id_num[i];
      }
  }
  bm_status_t status = BM_SUCCESS;
  switch (bmrt_arch_info::get_bmtpu_arch()) {
    case BM1684:
      status = bmfunc::bmdnn_1684()->_bmdnn_multi_fullnet_(
          m_handle, input_num, user_input_global_offset, cmd_input_global_offset, input_n,
          input_c, input_h, input_w, input_dtype, input_stmode, real_in_stmode, output_num,
          user_output_global_offset, cmd_output_global_offset, output_n, output_length,
          output_dtype, output_stmode, force_out_stmode,
          stage->bdc_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          stage->gdma_mem.addr + GLOBAL_MEM_CMD_START_OFFSET, bdc_id_num, gdma_id_num, group_num, input_pad_h);
      break;
    case BM1684X: {
      for (int s = 0; s < input_num; s++) {
        input_data_len[s] *= bmrt_data_type_size((bm_data_type_t)input_dtype[s]);
      }
      for (int s = 0; s < output_num; s++) {
        output_data_len[s] *= bmrt_data_type_size((bm_data_type_t)output_dtype[s]);
      }
      int func_id = net_ctx->kernel_module_->get_multi_fullnet_func_id();
      status = bmfunc::bmdnn_1684x()->_bmdnn_multi_fullnet_(
          m_handle, func_id, input_num, user_input_global_offset, cmd_input_global_offset,
          (u32*)input_data_len, output_num, user_output_global_offset,
          cmd_output_global_offset, (u32*)output_data_len,
          stage->bdc_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          stage->gdma_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          bdc_id_num, gdma_id_num, bdc_cmd_byte_size,
          gdma_cmd_byte_size, group_num);
      break;
    }
    case BM1686: {
      for (int s = 0; s < input_num; s++) {
        input_data_len[s] *= bmrt_data_type_size((bm_data_type_t)input_dtype[s]);
      }
      for (int s = 0; s < output_num; s++) {
        output_data_len[s] *= bmrt_data_type_size((bm_data_type_t)output_dtype[s]);
      }
      status = bmfunc::bmdnn_1686()->_bmdnn_multi_fullnet_(
          m_handle, input_num, user_input_global_offset, cmd_input_global_offset,
          (u32*)input_data_len, output_num, user_output_global_offset,
          cmd_output_global_offset, (u32*)output_data_len,
          stage->bdc_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          stage->gdma_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          bdc_id_num, gdma_id_num, bdc_cmd_byte_size,
          gdma_cmd_byte_size, group_num);
      break;
    }
    case BM1880:
      status = bmfunc::bmdnn_1880()->_bmdnn_multi_fullnet_(
          m_handle, input_num, user_input_global_offset, cmd_input_global_offset, input_n,
          input_length, input_dtype, input_stmode, real_in_stmode, output_num,
          user_output_global_offset, cmd_output_global_offset, output_n, output_length,
          output_dtype, output_stmode, force_out_stmode,
          // when engine fetch command, the commands can only locate at the previous 4G
          stage->bdc_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          stage->gdma_mem.addr + GLOBAL_MEM_CMD_START_OFFSET, bdc_id_num, gdma_id_num, group_num);
      break;
    case BM1682:
      status = bmfunc::bmdnn_1682()->_bmdnn_multi_fullnet_(
          m_handle, input_num, user_input_global_offset, cmd_input_global_offset, input_data_len, input_dtype,
          output_num, user_output_global_offset, cmd_output_global_offset, output_data_len, output_dtype,
          // when engine fetch command, the commands can only locate at the previous 4G
          stage->bdc_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          stage->gdma_mem.addr + GLOBAL_MEM_CMD_START_OFFSET,
          // m_cdma_cmd_start_address_v[net_idx] + GLOBAL_MEM_CMD_START_OFFSET,
          0, bdc_id_num, gdma_id_num, cdma_id_num, group_num);
      break;
    default:
      BMRT_LOG(FATAL, "Unknown BM TPU");
  }
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "launch failed, status:%d", status);
    trace();
  }

  return BM_SUCCESS == status;
}

static bool check_launch_params(net_ctx_t* net_ctx, const net_stage_t* stage, const bm_tensor_t* input_tensors, int input_num,
                                bm_tensor_t* output_tensors, int output_num, bool user_stmode)
{
  if (input_tensors == NULL) {
    BMRT_LOG(WRONG, "input_tensors is NULL");
    return false;
  }
  if (input_num != (int)net_ctx->input_name_v.size()) {
    BMRT_LOG(WRONG, "input num should be:%ld, not %d", static_cast<long>(net_ctx->input_name_v.size()), input_num);
    return false;
  }
  if (output_tensors == NULL) {
    BMRT_LOG(WRONG, "output_tensors is NULL");
    return false;
  }
  if (output_num != (int)net_ctx->output_name_v.size()) {
    BMRT_LOG(WRONG, "output num should be:%ld, not %d", static_cast<long>(net_ctx->output_name_v.size()),output_num);
    return false;
  }
  for (int idx = 0; idx < input_num; idx++) {
    auto& tensor = input_tensors[idx];
    size_t tensor_size = bmrt_tensor_bytesize(&tensor);
    size_t tensor_device_size = bmrt_tensor_device_size(&tensor);
    if (tensor_device_size < tensor_size ) {
      BMRT_LOG(WRONG, "input tensor[%d] may not initialized, tensor_size:%d is larger than tensor_device_size:%d", idx, tensor_size, tensor_device_size);
      return false;
    }
    else if(tensor_size == 0) {
      BMRT_LOG(WRONG, "input tensor[%d] may not initialized, tensor_size:%d is 0", idx, tensor_size);
      return false;
    }
    else if(tensor.shape.num_dims > BM_MAX_DIMS_NUM){
      BMRT_LOG(WRONG, "input tensor[%d] may not initialized, tensor.shape.num_dims:%d is larger than BM_MAX_DIMS_NUM:8", idx, tensor.shape.num_dims);
      return false;
    }
    if (tensor.dtype != net_ctx->input_type_v[idx]) {
      BMRT_LOG(WRONG, "input_tensors[%d] dtype:%d is not equal to net_ctx->input_type_v[idx]:%d", idx,tensor.dtype,net_ctx->input_type_v[idx]);
      return false;
    }
    // check input store mode
    bm_store_mode_t user_st_mode = tensor.st_mode;
    bm_store_mode_t in_st_mode = stage->input_v[idx].st_mode;
    if (in_st_mode == BM_STORE_1N && user_st_mode != BM_STORE_1N) {
      BMRT_LOG(WRONG, "input[%d] store mode must be 1N\n", idx);
      return false;
    }
  }
  if (user_stmode) {
    for (int idx = 0; idx < output_num; idx++) {
      bm_store_mode_t user_st_mode = output_tensors[idx].st_mode;
      bm_store_mode_t out_st_mode = stage->output_v[idx].st_mode;
      if (out_st_mode == BM_STORE_1N && user_st_mode != BM_STORE_1N) {
        BMRT_LOG(WRONG, "output[%d] store mode must be 1N\n", idx);
        return false;
      }
    }
  }
  return true;
}

static inline void saveData(bm_handle_t handle, const std::string &file,
                            const bm_tensor_t *tensors, int num) {
    std::ofstream f(file, std::ios::out | std::ios::binary);
    if (f.fail()) {
        BMRT_LOG(FATAL, "Failed to write %s", file.c_str());
    }
    for (int i = 0; i < num; ++i) {
        auto t = tensors[i];
        auto size = bmrt_tensor_bytesize(&t);
        auto m = malloc(size);
        bm_memcpy_d2s_partial(handle, m, t.device_mem, size);
        f.write(reinterpret_cast<char *>(m), static_cast<long>(size));
        free(m);
    }
    f.close();
}

void Bmruntime::init_output_tensors(net_ctx_t* net_ctx, net_stage_t* stage,
                                  bm_tensor_t* output_tensors, bool user_mem, bool user_stmode)
{
  for (u32 idx = 0; idx < stage->output_v.size(); idx++) {
    auto& output = output_tensors[idx];
    output.shape = stage->output_v[idx].shape;
    output.dtype = net_ctx->output_type_v[idx];

    if (user_stmode == false) {  // if not set, set to BM_STORE_1N as default
      output.st_mode = BM_STORE_1N;
    } else {
      if (output.st_mode == BM_STORE_4N && output.dtype != BM_UINT8 && output.dtype != BM_INT8) {
        BMRT_LOG(WRONG, "output store mode : only INT8/UINT8 support 4N");
      }
    }
    if (user_mem == false) {
      unsigned int mem_size = 0;
      if (output.st_mode == BM_STORE_4N) {
        bm_shape_t shape = output.shape;
        shape.dims[0] = ceiling_func(shape.dims[0], 4) * 4;
        mem_size = bmrt_shape_count(&shape);
      } else {
        mem_size = bmrt_tensor_bytesize(&output);
      }
      must_alloc_device_mem(&output.device_mem, mem_size, "output_mem");
    }
  }
}

bool Bmruntime::launch(int net_idx, const bm_tensor_t* input_tensors, int input_num,
                       bm_tensor_t* output_tensors, int output_num, bool user_mem, bool user_stmode)
{
  bool save_io = false;
  char *nt = getenv("BMRT_SAVE_IO_TENSORS");
  if (nt != nullptr)
      save_io = static_cast<bool>(atoi(nt));
  if (save_io)
      saveData(m_handle, "input_ref_data.dat.bmrt", input_tensors, input_num);
  // check parameters
  auto net_ctx = m_net_ctx_v[net_idx];
  int stage_idx = get_stage_idx(net_ctx, input_tensors);
  if (stage_idx == -1) {
      BMRT_LOG(WRONG, "Shapes of the input tensors are not supported");
    return false;
  }
  auto& stage = net_ctx->stage_v[stage_idx];

  if (false == check_launch_params(net_ctx, stage, input_tensors, input_num, output_tensors, output_num, user_stmode)) {
    return false;
  }

  // init output tensors
  init_output_tensors(net_ctx, stage, output_tensors, user_mem, user_stmode);

  bool ret = true;
  m_profile->init(net_ctx->net_name, stage->net_profile, stage->net_stat);
  if (stage->subnet_num > 1 ||
      (stage->subnet_num == 1 && stage->subnet_v[0]->subnet_mode == SUBNET_MODE_CPU)) {
        ret = launch_multi_subnet(net_ctx, stage, input_tensors, input_num, output_tensors,
                                     output_num);
  } else {
      m_profile->begin_subnet(net_ctx, 0, 0, SUBNET_MODE_TPU);
      m_profile->set_extra_data(net_ctx->is_dynamic);
      if(net_ctx->is_dynamic) {
          // launch_ir calls bm_thread_sync internally
          ret = launch_ir(net_ctx, stage, input_tensors, input_num, output_tensors, output_num);
      } else {
          // launch_static does not call bm_thread_sync internally
          ret = launch_static(net_ctx, stage, input_tensors, input_num, output_tensors, output_num);
          // so sync at some cases
          if(m_profile->is_enabled() || save_io){
            ret = (BM_SUCCESS == bm_thread_sync(m_handle));
          }
      }
      m_profile->end_subnet(net_ctx);
  }

  m_profile->deinit();
  if (save_io)
      saveData(m_handle, "output_ref_data.dat.bmrt", output_tensors, output_num);
  // free output mem if failed
  if (ret == false && user_mem == false) {
    for (int idx = 0; idx < output_num; idx++) {
      must_free_device_mem(output_tensors[idx].device_mem);
    }
  }
  return ret;
}

static bool check_launch_params(net_ctx_t* net_ctx, void* const input_datas[],
                                const bm_shape_t input_shapes[], int input_num,
                                void* output_datas[], bm_shape_t output_shapes[], int output_num,
                                bool user_mem)
{
  if (input_datas == NULL || input_shapes == NULL || input_num <= 0 || output_datas == NULL ||
      output_shapes == NULL || output_num <= 0) {
    BMRT_LOG(WRONG, "launch parameter is not correct");
    return false;
  }
  if (net_ctx->input_name_v.size() != (u32)input_num) {
    BMRT_LOG(WRONG, "input num [%d] is not supported", input_num);
    return false;
  }
  if (net_ctx->output_name_v.size() != (u32)output_num) {
    BMRT_LOG(WRONG, "output num [%d] is not supported", output_num);
    return false;
  }
  for (int i = 0; i < input_num; i++) {
    if (input_datas[i] == NULL) {
      BMRT_LOG(WRONG, "input[%d] is NULL", i);
      return false;
    }
  }

  if (user_mem) {
    for (int i = 0; i < output_num; i++) {
      if (output_datas[i] == NULL) {
        BMRT_LOG(WRONG, "output[%d] is NULL", i);
        return false;
      }
    }
  }
  return true;
}

bool Bmruntime::launch(int net_idx, void* const input_datas[], const bm_shape_t input_shapes[],
                       int input_num, void* output_datas[], bm_shape_t output_shapes[],
                       int output_num, bool user_mem)
{
  auto net_ctx = m_net_ctx_v[net_idx];
  // check parameters
  if (false == check_launch_params(net_ctx, input_datas, input_shapes, input_num, output_datas,
                                   output_shapes, output_num, user_mem)) {
    return false;
  }
  // prepare input and output tensors
  #ifdef __linux__
  bm_tensor_t input_tensors[input_num];
  bm_tensor_t output_tensors[output_num];
  #else
  std::shared_ptr<bm_tensor_t> input_tensors_(new bm_tensor_t[input_num], std::default_delete<bm_tensor_t[]>());
  bm_tensor_t* input_tensors = input_tensors_.get();
  std::shared_ptr<bm_tensor_t> output_tensors_(new bm_tensor_t[output_num], std::default_delete<bm_tensor_t[]>());
  bm_tensor_t* output_tensors = output_tensors_.get();
  #endif
  for (int i = 0; i < input_num; i++) {
    bmrt_tensor(&input_tensors[i], this, net_ctx->input_type_v[i], input_shapes[i]);
    bm_memcpy_s2d(m_handle, input_tensors[i].device_mem, (void*)input_datas[i]);
  }

  // launch may not call sync internally
  bool ret = launch(net_idx, input_tensors, input_num, output_tensors, output_num, false, false);

  // sync is needed for the following d2s to fetch output data
  if (ret){
    ret = (BM_SUCCESS == bm_thread_sync(m_handle));
  }
  if (!ret) {
    BMRT_LOG(WRONG, "launch net[%s] failed", net_ctx->net_name.c_str());
  } else {
    if (false == user_mem) {
      for (int i = 0; i < output_num; i++) {
        output_datas[i] = malloc(bmrt_tensor_bytesize(&output_tensors[i]));
      }
    }
    for (int i = 0; i < output_num; i++) {
      bm_memcpy_d2s_partial(m_handle, output_datas[i], output_tensors[i].device_mem,
                            bmrt_tensor_bytesize(&output_tensors[i]));
      must_free_device_mem(output_tensors[i].device_mem);
      output_shapes[i] = output_tensors[i].shape;
    }
  }
  for (int i = 0; i < input_num; i++) {
    must_free_device_mem(input_tensors[i].device_mem);
  }
  return ret;
}

const vector<string>* Bmruntime::get_input_tensor(int net_idx) const
{
  return &m_net_ctx_v[net_idx]->input_name_v;
}

const vector<string>* Bmruntime::get_output_tensor(int net_idx) const
{
  return &m_net_ctx_v[net_idx]->output_name_v;
}

const vector<u8>* Bmruntime::get_net_profile(int net_idx, int stage_idx)
{
  return &m_net_ctx_v[net_idx]->stage_v[stage_idx]->net_profile;
}

/* C-style Interface */
bool Bmruntime::get_input_tensor(int net_idx, int* input_num, const char*** input_names) const
{
  int i = 0;
  const vector<string>& tensor_names = m_net_ctx_v[net_idx]->input_name_v;
  if (input_num)
    *input_num = tensor_names.size();
  /* user need free input_names */
  *input_names = (const char**)malloc(tensor_names.size() * sizeof(char*));
  for (vector<string>::const_iterator iter = tensor_names.begin(); iter != tensor_names.end();
       ++iter, i++)
    (*input_names)[i] = (*iter).c_str();
  return true;
}

bool Bmruntime::get_output_tensor(int net_idx, int* output_num, const char*** output_names) const
{
  int i = 0;
  const vector<string>& tensor_names = m_net_ctx_v[net_idx]->output_name_v;
  if (output_num)
    *output_num = tensor_names.size();
  /* Caller need free output_names */
  *output_names = (const char**)malloc(tensor_names.size() * sizeof(char*));
  for (vector<string>::const_iterator iter = tensor_names.begin(); iter != tensor_names.end();
       ++iter, i++)
    (*output_names)[i] = (*iter).c_str();
  return true;
}

static std::string shape_to_str(const bm_shape_t& shape) {
  std::string str ="[ ";
  for(int i=0; i<shape.num_dims; i++){
    str += std::to_string(shape.dims[i]) + " ";
  }
  str += "]";
  return str;
}

static void show_net_info(const bm_net_info_t* netinfo, int index) {
  const char* dtypeMap[] = {
    "FLOAT32",
    "FLOAT16",
    "INT8",
    "UINT8",
    "INT16",
    "UINT16",
    "INT32",
    "UINT32",
  };
  BMRT_LOG(INFO, " ########################");
  BMRT_LOG(INFO, " NetName: %s, Index=%d", netinfo->name, index);
  for(int s=0; s<netinfo->stage_num; s++){
    BMRT_LOG(INFO, " ---- stage %d ----", s);
    for(int i=0; i<netinfo->input_num; i++){
      auto shapeStr = shape_to_str(netinfo->stages[s].input_shapes[i]);
      BMRT_LOG(INFO, "   Input %d) '%s' shape=%s dtype=%s scale=%g zero_point=%d",
          i,
          netinfo->input_names[i],
          shapeStr.c_str(),
          dtypeMap[netinfo->input_dtypes[i]],
          netinfo->input_scales[i],
          netinfo->input_zero_point[i]);
    }
    for(int i=0; i<netinfo->output_num; i++){
      auto shapeStr = shape_to_str(netinfo->stages[s].output_shapes[i]);
      BMRT_LOG(INFO, "   Output %d) '%s' shape=%s dtype=%s scale=%g zero_point=%d",
          i,
          netinfo->output_names[i],
          shapeStr.c_str(),
          dtypeMap[netinfo->output_dtypes[i]],
          netinfo->output_scales[i],
          netinfo->output_zero_point[i]);
    }
  }
  BMRT_LOG(INFO, " ########################");
}

void Bmruntime::show_neuron_network()
{
  int size = m_net_ctx_v.size();
  for (int idx = 0; idx < size; idx++) {
    auto net_info = get_net_info(idx);
    show_net_info(net_info, idx);
  }
}

int Bmruntime::get_net_idx(const string& net_name)
{
  int size = m_net_ctx_v.size();
  for (int idx = 0; idx < size; idx++) {
    if (m_net_ctx_v[idx]->net_name == net_name) {
      return idx;
    }
  }
  BMRT_LOG(WRONG, "Can't find network name:%s", net_name.c_str());
  return -1;
}

bool Bmruntime::can_batch_size_change(int net_idx) {
    return m_net_ctx_v[net_idx]->is_dynamic ? m_net_ctx_v[net_idx]->n_can_change == 1 : false;
}

bool Bmruntime::can_height_and_width_change(int net_idx) {
    return m_net_ctx_v[net_idx]->is_dynamic ? m_net_ctx_v[net_idx]->h_w_can_change == 1 : false;
}

// get input and output index by name
int Bmruntime::get_input_index(const string &tensor_name, int net_idx) {
    auto &v = m_net_ctx_v[net_idx]->input_name_v;
    auto it = std::find(v.begin(), v.end(), tensor_name);
    if (it == v.end()) {
        BMRT_LOG(WRONG, "Wrong input tensor name [%s]",  tensor_name.c_str());
        return -1;
    }
    return it - v.begin();
}

int Bmruntime::get_output_index(const string &tensor_name, int net_idx) {
    auto &v = m_net_ctx_v[net_idx]->output_name_v;
    auto it = std::find(v.begin(), v.end(), tensor_name);
    if (it == v.end()) {
        BMRT_LOG(WRONG, "Wrong output tensor name [%s]",  tensor_name.c_str());
        return -1;
    }
    return it - v.begin();
}

const bm_shape_t *Bmruntime::get_input_max_shape(int net_idx, int input_idx) {
    bm_shape_t *res = nullptr;
    u64 max_count = 0;
    for (auto &it : m_net_ctx_v[net_idx]->stage_v) {
        auto cur = &it->input_v[input_idx].shape;
        u64 count = bmrt_shape_count(cur);
        if (count > max_count) {
            max_count = count;
            res = cur;
        }
    }
    return res;
}

const bm_shape_t *Bmruntime::get_output_max_shape(int net_idx, int output_idx) {
    bm_shape_t *res = nullptr;
    u64 max_count = 0;
    for (auto &it : m_net_ctx_v[net_idx]->stage_v) {
        auto cur = &it->output_v[output_idx].shape;
        u64 count = bmrt_shape_count(cur);
        if (count > max_count) {
            max_count = count;
            res = cur;
        }
    }
    return res;
}

void Bmruntime::get_network_names(vector<const char *> *names) {
    if (names != nullptr) {
        names->clear();
        for (auto &net_ctx : m_net_ctx_v)
            names->push_back(net_ctx->net_name.c_str());
    }
}

int Bmruntime::get_input_blob_max_shape(const string &tensor_name, int net_idx, int *shape) {
    auto pos = get_input_index(tensor_name, net_idx);
    if (pos < 0)
        return -1;
    auto max_shape = get_input_max_shape(net_idx, pos);
    std::copy_n(max_shape->dims, max_shape->num_dims, shape);
    return max_shape->num_dims;
}

int Bmruntime::get_output_blob_max_shape(const string &tensor_name, int net_idx, int *shape) {
    auto pos = get_output_index(tensor_name, net_idx);
    if (pos < 0)
        return -1;
    auto max_shape = get_output_max_shape(net_idx, pos);
    std::copy_n(max_shape->dims, max_shape->num_dims, shape);
    return max_shape->num_dims;
}

int Bmruntime::get_input_data_type(const string &tensor_name, int net_idx) {
    auto &v = m_net_ctx_v[net_idx]->input_name_v;
    auto it = std::find(v.begin(), v.end(), tensor_name);
    if (it == v.end()) {
        BMRT_LOG(WRONG, "Wrong input tensor name [%s]",  tensor_name.c_str());
        return -1;
    }
    return m_net_ctx_v[net_idx]->input_type_v[it - v.begin()];
}

int Bmruntime::get_output_data_type(const string &tensor_name, int net_idx) {
    auto &v = m_net_ctx_v[net_idx]->output_name_v;
    auto it = std::find(v.begin(), v.end(), tensor_name);
    if (it == v.end()) {
        BMRT_LOG(WRONG, "Wrong output tensor name [%s]",  tensor_name.c_str());
        return -1;
    }
    return m_net_ctx_v[net_idx]->output_type_v[it - v.begin()];
}

int Bmruntime::get_input_gmem_stmode(const string &tensor_name, int net_idx) {
    auto &v = m_net_ctx_v[net_idx]->input_name_v;
    auto it = std::find(v.begin(), v.end(), tensor_name);
    if (it == v.end()) {
        BMRT_LOG(WARNING, "Wrong input tensor name [%s]",  tensor_name.c_str());
        return BM_STORE_1N;  // default 1N
    }
    return m_net_ctx_v[net_idx]->stage_v[0]->input_v[it - v.begin()].st_mode;
}

int Bmruntime::get_output_gmem_stmode(const string& tensor_name, int net_idx) {
    auto &v = m_net_ctx_v[net_idx]->output_name_v;
    auto it = std::find(v.begin(), v.end(), tensor_name);
    if (it == v.end()) {
        BMRT_LOG(WARNING, "Wrong output tensor name [%s]",  tensor_name.c_str());
        return BM_STORE_1N;  // default 1N
    }
    return m_net_ctx_v[net_idx]->stage_v[0]->output_v[it - v.begin()].st_mode;
}

int Bmruntime::get_stage_idx(const net_ctx_t* net_ctx, const bm_tensor_t* input_tensors)
{
  return net_ctx->is_dynamic ? get_dynamic_stage_idx(net_ctx, input_tensors)
                             : get_static_stage_idx(net_ctx, input_tensors);
}

/* for static net */
int Bmruntime::get_static_stage_idx(const net_ctx_t* net_ctx, const bm_tensor_t* input_tensors)
{
  for (u32 stage_idx = 0; stage_idx < net_ctx->stage_v.size(); stage_idx++) {
    auto& input_v = net_ctx->stage_v[stage_idx]->input_v;
    u32 input_idx;
    for (input_idx = 0; input_idx < input_v.size(); input_idx++) {
      if (false == bmrt_shape_is_same(&input_tensors[input_idx].shape, &input_v[input_idx].shape)) {
        break;
      }
    }
    if (input_idx == input_v.size()) {
      return stage_idx;
    }
  }

  return -1;
}

/* for dynamic net */
int Bmruntime::get_dynamic_stage_idx(const net_ctx_t* net_ctx, const bm_tensor_t* input_tensors)
{
  // TODO(pengchao.hu): only check input 0, need to check multi-input in future
  auto& user_shape = input_tensors[0].shape;
  uint64_t diff_sum = (uint64_t)(-1);
  int stage_idx = -1;
  for (u32 idx = 0; idx < net_ctx->stage_v.size(); idx++) {
    /* get stage idx, which (h, w) belongs to */
    uint64_t sum = 0;
    int num_dims = user_shape.num_dims;
    auto& stage = net_ctx->stage_v[idx];
    auto& stage_shape = stage->input_v[0].shape;
    if (stage->input_v[0].shape.num_dims != user_shape.num_dims) {
      continue;
    }
    // use is_dynamic instead of n_can_change to allow variable n outside
    if (0 == net_ctx->is_dynamic) {
      if (user_shape.dims[0] != stage_shape.dims[0]) {
        continue;
      }
    } else {
      if (user_shape.dims[0] > stage_shape.dims[0]) {
        continue;
      }
      if (stage_shape.dims[0] != user_shape.dims[0]) {
        if (sum == 0) {
          sum = 1;
        }
        sum *= (stage_shape.dims[0] - user_shape.dims[0]);
      }
    }
    int dim_idx = 1;
    for (; dim_idx < num_dims; dim_idx++) {
        // use is_dynamic instead of h_w_can_change to allow variable h/w outside
      if (0 == net_ctx->is_dynamic) {
        if (stage_shape.dims[dim_idx] != user_shape.dims[dim_idx]) {
          break;
        }
      } else {
        if (stage_shape.dims[dim_idx] < user_shape.dims[dim_idx]) {
          break;
        }
        if (stage_shape.dims[dim_idx] != user_shape.dims[dim_idx]) {
          if (sum == 0) {
            sum = 1;
          }
          sum *= (stage_shape.dims[dim_idx] - user_shape.dims[dim_idx]);
        }
      }
    }
    if (dim_idx < num_dims) {
      continue;
    }

    if (sum == 0) {
      return idx;
    } else if (sum < diff_sum) {
      stage_idx = idx;
      diff_sum = sum;
    }
  }
  return stage_idx;
}

const bm_net_info_t* Bmruntime::get_net_info(int net_idx)
{
  int size = m_net_ctx_v.size();
  if (net_idx < 0 || net_idx >= size) {
    return NULL;
  }
  return &m_net_ctx_v[net_idx]->net_info;
}

void Bmruntime::set_bmrt_mmap(bool enable)
{
  if (!bmrt_arch_info::is_soc_mode()) {
    BMRT_LOG(WARNING, "Only support mmap in soc mode");
    return;
  }
  b_enable_mmap = enable;
}

void Bmruntime::subnet_time_print(bool enable)
{
  m_subnet_time_print = enable;
}

const std::vector<bm_device_mem_t> &Bmruntime::get_neuron_mem(int net_idx)
{
  return m_net_ctx_v[net_idx]->neuron_mem;
}

void Bmruntime::set_debug_mode(int mode)
{
    bm_set_debug_mode(m_handle , mode);
}

u64 Bmruntime::must_alloc_device_mem(bm_device_mem_t *mem, u64 size, const std::string &desc, int type_len)
{
  bm_status_t status = bm_malloc_device_byte_heap_mask(m_handle, mem, m_neuron_heap_mask, size*type_len);
  if (BM_SUCCESS != status) {
    BMRT_LOG(FATAL, "device mem alloc failed: size=%llu[0x%x] type_len=%d status=%d desc=%s",
             size, size, type_len, status, desc.c_str());
  }
#if 0
  // Setting all neuron to nan can be useful when debugging net inference
  float nan = std::nanf("");
  if (bm_memset_device_ext(m_handle, &nan, 4, *mem) != BM_SUCCESS)
  {
    BMRT_LOG(FATAL, "bm_memset_device_ext failed");
  }
#endif
  m_profile->record_alloc_device_mem(*mem, desc);
  return bm_mem_get_device_addr(*mem);
}
bm_device_mem_t Bmruntime::must_alloc_device_mem(u64 size, const std::string &desc, int type_len){
  bm_device_mem_t mem;
  must_alloc_device_mem(&mem, size, desc, type_len);
  return mem;
}
void Bmruntime::must_free_device_mem(bm_device_mem_t& mem){
  bm_free_device(m_handle, mem);
  m_profile->record_free_device_mem(mem);
}

}  // namespace bmruntime
