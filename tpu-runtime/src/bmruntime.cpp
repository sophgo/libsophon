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
#include <numeric>
#include "bmlib_runtime.h"
#include "bmruntime_common.h"

void* bmrt_load_lib(const char* libname, int flags) {
  void* handle=nullptr;
  #ifdef __linux__
  handle = dlopen(libname, flags);
  #else
  (void)flags;
  handle = (void*)LoadLibrary(libname);
  #endif
  return handle;
}

void* bmrt_find_sym(void* libhandle, const char* symname) {
  void* sym=nullptr;
#ifdef __linux__
  sym = (void*)dlsym(libhandle, symname);
#else
  sym = (void*)GetProcAddress((HMODULE)libhandle, symname);
#endif
  return sym;
}

void bmrt_unload_lib(void* libhandle) {
#ifdef __linux__
  (void) dlclose(libhandle);
#else
  (void) FreeLibrary((HMODULE)libhandle);
#endif
}

const char* bmrt_lib_error() {
#ifdef __linux__
  return dlerror();
#else
  static char buf[64];
  auto dw = GetLastError();
  snprintf(buf, sizeof(buf), "Error Code is %d", dw);
  return buf;
#endif
}

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

BMRT_LogLevel BMRT_LOG_LEVEL_THRESHOLD = BMRT_LogLevel::WRONG; //wrong level, default output wrong and fatal print

namespace {
struct LogLevel {
    LogLevel() {
        const char *env_str = nullptr;
        if ((env_str = getenv("BMRT_LOG_VERSION")) != nullptr)
            BMRT_LOG_LEVEL_THRESHOLD = (BMRT_LogLevel)atoi(env_str);
    }
};
static LogLevel log_level_init;
}

#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif

#include "bmrt_version.h"
static const char* version_string = "libbmrt_version:1.0.0, branch:" BRANCH_NAME ",commit:" COMMIT_HASH " ,compiled on " COMPILE_TIME ".";
extern "C" {
  const char* libbmrt_version() {
    return version_string;
  }

  BMRT_LogLevel bmrt_get_current_log_level() {
    return BMRT_LOG_LEVEL_THRESHOLD;
  }

  void bmrt_set_current_log_level(BMRT_LogLevel level) {
    BMRT_LOG_LEVEL_THRESHOLD = level;
  }
}

const char* _bmrt_version() {
  return version_string;
}
#ifdef _WIN32
const char *_libsophon_version()
{
    return "";
}
#else
#define DRIVER_PATH "/proc/bmsophon/driver_version"
const char* _libsophon_version() {
  static char data[64] = {0};
  if (data[0] == 0 && access(DRIVER_PATH, F_OK) == 0) {
    FILE *fp = NULL;
    fp = popen("cat " DRIVER_PATH, "r");
    if (fp == NULL) {
      BMRT_LOG(WARNING, "cannot open " DRIVER_PATH);
      return data;
    }
    auto result = fgets(data, sizeof(data), fp);
    if(result == NULL){
      BMRT_LOG(WARNING, "cannot read " DRIVER_PATH);
    }
    pclose(fp);
  }
  return data;
}
#endif

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

static void dump_tensor(bm_handle_t bm_handle, bm_tensor_t &tensor) {
  auto shape = tensor.shape;
  int size = 1;
  for (int i = 0; i < shape.num_dims; ++i){
    size *= shape.dims[i];
  }
  std::vector<float> data(size);
  bm_memcpy_d2s(bm_handle, data.data(), tensor.device_mem);
  // std::cout<< data[0] << "\t" << data[data.size()-1] << std::endl;
  auto ptr = data.data();
  ptr[0] = ptr[0];
}


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
    uint32_t devid;
    bm_device_mem_ext(bm_device_mem_t mem, Bmruntime *rt, uint32_t devid) {
        this->mem = mem;
        this->rt = rt;
        this->devid = devid;
    }
    ~bm_device_mem_ext() {
        rt->must_free_device_mem(devid, mem);
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
  // init core num
  m_core_num = 1;
  auto arch = bmrt_arch_info::get_bmtpu_arch();
  switch (arch) {
  case BM1688:
    bm_get_tpu_core_num(m_handles[0], &m_core_num);
    break;
  case SG2380:
    m_core_num = 4;
    break;
  case BM1690:
    m_core_num = 8;
    break;
  }

  // init mem
  alloc_mem = true;
  m_device_mem_vec.clear();
  m_net_ctx_v.clear();
  // pre alloc mem to avoid mem reallocation for multi-threads case
  m_net_ctx_v.reserve(1024);
  m_net_cascade_v.reserve(1024);
  bmcpu_setup();
  bmtpu_setup();
  if (bmcpu_init_ != NULL) {
    bmcpu_handle_ = bmcpu_init_();
  }
  if (customcpu_init_ != NULL) {
    customcpu_handle_ = customcpu_init_();
  }
  m_subnet_time_print = false;
  if (bmrt_arch_info::is_soc_mode()) {
    b_enable_mmap = true; /* soc default using mmap */
  } else {
    b_enable_mmap = false;
  }
  for (int i = 0; i < m_device_num;i++) {
    m_devids[i] = bm_get_devid(m_handles[i]);
  }

  if (true) {
    std::lock_guard<std::mutex> guard(m_global_coeff_mutex);
    for (int i = 0; i < m_device_num; i++) {
      auto iter = m_global_coeff_map.find(m_devids[i]);
      if (iter == m_global_coeff_map.end()) {
        m_local_coeffs[i] = std::make_shared<BmCoeff>(m_devids[i]);
        m_global_coeff_map[m_devids[i]] = m_local_coeffs[i];
      } else if (iter->second == NULL) {
        m_local_coeffs[i] = std::make_shared<BmCoeff>(m_devids[i]);
        iter->second = m_local_coeffs[i];
      } else {
        m_local_coeffs[i] = iter->second;
      }
    }
  }
  // middle buffer
  for (int i = 0; i < m_device_num; i++) {
    bm_set_device_mem(&max_middle_buffer[i], 0, 0);
    max_middle_buffer_size[i] = 0;
    middle_buffer_num[i] = 0;
    bm_set_device_mem(&max_hidden_buffer[i], 0, 0);
    max_hidden_buffer_size[i] = 0;
    hidden_buffer_num[i] = 0;
  }
  // set default flags
  m_flags = 0;
  if (m_core_num == 1) {
    m_flags |= BM_RUNTIME_SHARE_MEM;
  }
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
  if (m_device_num > 1) {
    for (int i = 0; i < m_device_num; i++) {
      m_cascade_thread_v.push_back(
          std::make_shared<CascadeThread>(this, m_handles[i]));
    }
  }
  temp_filename_ = "/tmp/lib_tmp_cpuop.so-XXXXXX";
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
Bmruntime::Bmruntime(bm_handle_t * bm_handle, bool user_initlized, const string& arch_name)
{
  init_bmfunc(arch_name);

  if (user_initlized == true)
    using_internal_bm_handle = false;
  else {
    bm_dev_request(bm_handle, 0);
    BMRT_ASSERT_INFO(bm_handle,"bm_handle shouldn't be NULL\n");
    using_internal_bm_handle = true;
  }
  m_handles[0] = *bm_handle;
  m_device_num = 1;
  init();
}

Bmruntime::Bmruntime(bm_handle_t *bm_handles, int num_handles,
                     bool using_internal_hiddens, const string &arch_name) {
  BMRT_ASSERT_INFO(num_handles > 0 && num_handles <= MAX_DEVICE_NUM,
                   "num_handles should > 0 and <= %d", MAX_DEVICE_NUM);
  init_bmfunc(arch_name);
  using_internal_bm_handle = false;
  using_internal_hidden_tensors = using_internal_hiddens;
  m_device_num = num_handles;
  std::copy(bm_handles, bm_handles + num_handles, m_handles);
  init();
  struct bm_misc_info misc_info;
  bm_get_misc_info(bm_handles[0], &misc_info);
  u8 board_type = (u8)((misc_info.board_version >> 8) & 0xff);
  if (board_type == 0x21) {
    card_chip_num = 8;
  } else if (board_type == 0x23) {
    card_chip_num = 6;
  } else {
  // BMRT_ASSERT_INFO(0, "CascadeNet can not run on this board.");
  }
}

/* using internal initilized bm_handle ,with specific dev_id */
Bmruntime::Bmruntime(const string& arch_name, int devid)
{
  init_bmfunc(arch_name);

  bm_dev_request(&m_handles[0], devid);
  BMRT_ASSERT_INFO(m_handles[0],"m_handle shouldn't be NULL\n");
  using_internal_bm_handle = true;
  m_device_num = 1;
  init();
}

/* free bmcpu_handle_ & customcpu_handle_ */
void Bmruntime::uninit_cpu_handles() {
  if (bmcpu_uninit_ != NULL) {
    bmcpu_uninit_(bmcpu_handle_);
  }
  if (customcpu_handle_ != NULL && customcpu_uninit_ != NULL) {
    customcpu_uninit_(customcpu_handle_);
  }
}

/* free device memory */
void Bmruntime::free_device_memory() {
  for (size_t i = 0; i < m_device_mem_vec.size(); i++) {
    auto &dev_mem = m_device_mem_vec[i];
    auto id = m_device_mem_ids[i];
    BMRT_DEBUG("Free device memory, byte size %d\n", bm_mem_get_device_size(dev_mem));
    must_free_device_mem(id, dev_mem);
  }

  for (size_t i = 0; i < m_sg_device_mem_vec.size(); i++) {
    auto &dev_mem = m_sg_device_mem_vec[i];
    auto id = m_sg_device_mem_ids[i];
    BMRT_DEBUG("Free device memory, byte size %d\n", bm_mem_get_device_size_u64(dev_mem));
    must_free_device_mem_u64(id, dev_mem);
  }
}

/* free subnet & dynamic neuron & net info & net_cascade */
void Bmruntime::free_network_contexts_and_cascades() {
  for (auto net_ctx : m_net_ctx_v) {
    subnet_clear(net_ctx);
    free_dyn_neuron(net_ctx);
    free_net_info(net_ctx);
    delete []net_ctx->stage_v[0];
    delete net_ctx;
  }
  for (auto net_cascade : m_net_cascade_v) {
    cascade_free_net_info(&net_cascade);
  }
}

/* free coeff mem */
void Bmruntime::free_coeff_mem() {
  std::lock_guard<std::mutex> guard(m_global_coeff_mutex);
  for (int i = 0; i < m_device_num; i++) {
    m_local_coeffs[i] = NULL;
    auto iter = m_global_coeff_map.find(m_devids[i]);
    if (iter->second.unique()) {
      iter->second = NULL;
    }
  }
}

/* free m_handles & cpu_handle */
void Bmruntime::free_bm_handle_and_custom_cpu_handles() {
  if (using_internal_bm_handle) {
    bm_dev_free(m_handles[0]);
  }
#ifdef __linux__
  if (customcpu_handle_ != NULL && tmpcpuso_handle_ != NULL) {
    unlink(&temp_filename_[0]);
    dlclose(tmpcpuso_handle_);
    remove(&temp_filename_[0]);
  }
#endif
}

void Bmruntime::destory_without_coeff()
{
  // step1: uninit cpu handles
  uninit_cpu_handles();

  // step2: free device memory
  free_device_memory();

  // step3: free subnet & dynamic neuron & net info & net_cascade
  free_network_contexts_and_cascades();

  // step4: free coeff mem
  // free_coeff_mem();

  // step5: free bm handle & cpu handle
  free_bm_handle_and_custom_cpu_handles();
}

Bmruntime::~Bmruntime()
{
  // step1: uninit cpu handles
  uninit_cpu_handles();

  // step2: free device memory
  free_device_memory();

  // step3: free subnet & dynamic neuron & net info & net_cascade
  free_network_contexts_and_cascades();

  // step4: free coeff mem
  free_coeff_mem();

  // step5: free bm handle & cpu handle
  free_bm_handle_and_custom_cpu_handles();
}

static bool is_cmodel_mode(){
     return bmrt_find_sym(nullptr, "set_tpu_entry") != nullptr;
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
        set_tpu_entry_t set_entry = (set_tpu_entry_t)bmrt_find_sym(nullptr, "set_tpu_entry");
        string firmware_name = firmware_path + "/firmware.so";
        auto fw_handle = bmrt_load_lib(firmware_name.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if(!fw_handle){
            BMRT_LOG(WARNING, "fail to open %s: %s", firmware_name.c_str(), bmrt_lib_error());
        }
        auto shape_ptr = bmrt_find_sym(fw_handle, "tpu_shape_infer_entry");
        auto local_ptr = bmrt_find_sym(fw_handle, "tpu_local_calculate_entry");
        auto global_ptr = bmrt_find_sym(fw_handle, "tpu_global_calculate_entry");
        if(!shape_ptr || !local_ptr || !global_ptr){
            BMRT_LOG(WARNING, "fail to load entry function from", firmware_name.c_str());
            bmrt_unload_lib(fw_handle);
        }
        set_entry(shape_ptr, global_ptr, local_ptr);
    } else {
        string tcm_firmware = firmware_path + "/firmware_tcm.bin";
        string ddr_firmware = firmware_path + "/firmware_ddr.bin";
        auto load_func = (firmware_load_func_t)bmrt_find_sym(nullptr, "bmkernel_load_firmware");
        if(!load_func){
            BMRT_LOG(WARNING, "cannot find bmkernel_load_firmware function");
        } else {
          for (int i = 0; i < m_device_num; i++) {
            if(load_func(m_handles[i], tcm_firmware.c_str(), ddr_firmware.c_str()) != BM_SUCCESS){
              BMRT_LOG(WARNING, "fail to load firmware: %s", tcm_firmware.c_str());
            }
          }
        }
    }
}


void Bmruntime::bmcpu_setup()
{
  bmcpu_init_ = (t_bmcpu_init)bmrt_find_sym(NULL, "bmcpu_init");
  bmcpu_uninit_ = (t_bmcpu_uninit)bmrt_find_sym(NULL, "bmcpu_uninit");
  bmcpu_process_ = (t_bmcpu_process)bmrt_find_sym(NULL, "bmcpu_process");

  if (bmcpu_process_ == NULL) {
  #ifdef __linux__
    std::vector<const char*> cpu_libs = {
      "libbmcpu.so",
      "libcpuop.so"
    };
  #else
    std::vector<const char*> cpu_libs = {
      "libbmcpu.dll",
      "libcpuop.dll"
    };
  #endif
    void* cpu_so_handle_ = nullptr;
    for(auto cpu_lib: cpu_libs){
      cpu_so_handle_ = bmrt_load_lib(cpu_lib, RTLD_LAZY);
      if(cpu_so_handle_) {
        BMRT_LOG(INFO, "cpu_lib '%s' is loaded.", cpu_lib);
        break;
      }
    }
    if (!cpu_so_handle_) {
      BMRT_LOG(WARNING, "Not able to open %s", "cpu.so");
      return;
    }
    bmcpu_init_ = (t_bmcpu_init)bmrt_find_sym(cpu_so_handle_, "bmcpu_init");
    bmcpu_uninit_ = (t_bmcpu_uninit)bmrt_find_sym(cpu_so_handle_, "bmcpu_uninit");
    bmcpu_process_ = (t_bmcpu_process)bmrt_find_sym(cpu_so_handle_, "bmcpu_process");
  } else {
    BMRT_LOG(INFO, "cpu lib already exist, don't load again");
  }

  #ifdef __linux__
    const char* cpu_lib = "libcustomcpuop.so";
  #else
    const char* cpu_lib = "libcustomcpuop.dll";
  #endif

  if (customcpu_process_ == NULL) {
    void* customcpu_so_handle_ = nullptr;
    customcpu_so_handle_ = bmrt_load_lib(cpu_lib, RTLD_LAZY);
    if(customcpu_so_handle_) {
      BMRT_LOG(INFO, "customcpu_lib '%s' is loaded.", cpu_lib);
    }
    if (!customcpu_so_handle_) {
      BMRT_LOG(INFO, "Not able to open %s", "libcustomcpuop.so");
    }
    customcpu_init_ = (t_bmcpu_init)bmrt_find_sym(customcpu_so_handle_, "bmcpu_init");
    customcpu_uninit_ = (t_bmcpu_uninit)bmrt_find_sym(customcpu_so_handle_, "bmcpu_uninit");
    customcpu_process_ = (t_bmcpu_process)bmrt_find_sym(customcpu_so_handle_, "customcpu_process");
  } else {
    BMRT_LOG(INFO, "%s already exist, don't load again", cpu_lib);
  }
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
  bool io_alone = stage->io_size > 0; // has io space
  auto coeff_limit = io_alone ? stage->io_start : stage->ctx_start;
  if (origin_addr < coeff_limit) {
    if (false == is_src) {
      BMRT_LOG(FATAL, "gdma dst shouldn't be coeff, origin[0x%llx], ctx[0x%llx]",
               origin_addr, coeff_limit);
    }
    return origin_addr + stage->coeff_offset;
  }
  if (io_alone && origin_addr < stage->ctx_start) {
    return origin_addr + stage->io_offset;
  }
  int mem_index  = get_mem_index(stage->ctx_borders, stage->ctx_start, origin_addr);
  BMRT_ASSERT_INFO(mem_index < stage->ctx_offset.size(), " addr 0x%llx is overflow, valid range is [0x%llx, 0x%llx)\n",
      origin_addr, stage->ctx_start, stage->ctx_start + stage->ctx_borders.back());
  return origin_addr + stage->ctx_offset[mem_index];
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
    case BM1688:
      if (id == ENGINE_GDMA && !last_cmd && stage->io_size > 0) {
        int cmd_type = (cmd[1] & 0x0f);
        if(cmd_type == 6) return; //cmd_type: DMA_sys
        u64 src_addr = ((u64)(cmd[17] & 0xff) << 32) | ((u64)cmd[16]);
        u64 dst_addr = ((u64)(cmd[19] & 0xff) << 32) | ((u64)cmd[18]);
        bool src_in_global = (src_addr >> 39) & 0x1;
        bool dst_in_global = (dst_addr >> 39) & 0x1;
        u64 fix_addr;
        if (src_in_global && ((src_addr >> 36) & 0x7) == 0) {
          fix_addr = fix_gdma_addr(stage, src_addr & ((1ull << 35) - 1), true);
          fix_addr |= (1ull << 39);
          if (fix_addr != src_addr) {
            cmd[16] = fix_addr & 0xffffffff;
            cmd[17] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[17] & 0xffffff00);
          }
        }
        if (dst_in_global && ((dst_addr >> 36) & 0x7) == 0) {
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
          if (((index_addr >> 39) & 0x1) && ((index_addr >> 36) & 0x7) == 0) {
            fix_addr = fix_gdma_addr(stage, index_addr & ((1ull << 35) - 1), true);
            fix_addr |= (1ull << 39);
            if (fix_addr != index_addr) {
              cmd[20] = fix_addr & 0xffffffff;
              cmd[21] = ((u32)((fix_addr >> 32) & 0xff)) | (cmd[21] & 0xffffff00);
            }
          }
        }
      }
      break;
    case SG2380:
      break;
    case BM1690:
      break;
    case MARS3:
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

  auto net_ctx = m_net_ctx_v[net_idx];
  auto devid = net_ctx->device_id;
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
    ret = (BM_SUCCESS == bm_thread_sync(m_handles[devid]));
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

void Bmruntime::sync_cores(bm_handle_t handle, const std::vector<int32_t>& core_list)
{
  for (int core_idx=0; core_idx<core_list.size(); core_idx++) {
      bm_status_t status = bm_thread_sync_from_core(handle, core_list[core_idx]);
      if (BM_SUCCESS != status) {
        BMRT_LOG(WRONG, "launch failed, status:%d", status);
        trace();
      }
    }
}

bool Bmruntime::launch_ir(net_ctx_t* net_ctx, net_stage_t* stage,
                          const bm_tensor_t* input_tensors, int input_num,
                          bm_tensor_t* output_tensors, int output_num,
                          const size_t dyn_core_mask)
{
  bm_status_t status;
  auto arch = bmrt_arch_info::get_bmtpu_arch();
  auto devid = net_ctx->device_id;
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
    if (arch == BM1684X || arch == BM1688 || arch == BM1690 || arch == SG2380) {
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
  if (arch == BM1684) {
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
  // u64 output_shape_global_addr =  must_alloc_device_mem(devid, &output_shape_mem, output_num*sizeof(bm_shape_ex_t));
  std::string suffix = (m_flags & BM_RUNTIME_SHARE_MEM) ? "" : "_" + std::to_string(dyn_core_mask);
  u64 output_shape_global_addr = alloc_device_mem(devid, output_shape_mem, output_num*sizeof(bm_shape_ex_t), "dynamic_out"+suffix, 1, false);
  if (alloc_mem) {
    bm_device_mem_ext_t output_shape_mem_ext(output_shape_mem, this, devid);
  }

  #ifdef __linux__
  int input_elem_num[input_num];
  #else
  std::shared_ptr<int> input_elem_num_(new int[input_num], std::default_delete<int[]>());
  int* input_elem_num = input_elem_num_.get();
  #endif
  memset(input_elem_num, 0, sizeof(int) * input_num); //setting to 0 means that does not need to count elem_num
  auto core_list = get_core_list_from_core_mask(dyn_core_mask);
  if (core_list.size() > 1) {
    core_list.resize(1);
  }
  if (arch == BM1682) {
    status = bmfunc::bmdnn_1682()->_bmdnn_dynamic_fullnet_v2_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr, stage->core_commands[0].ir_mem.dword_len, input_num, user_input_global_addrs,
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
        m_handles[devid], stage->core_commands[0].ir_mem.addr, stage->core_commands[0].ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_global_addr_middle, user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, user_output_global_addr_middle, stage->ctx_start,
        stage->ctx_borders, stage->ctx_offset,
        stage->coeff_offset, need_middle_buff_flag, output_need_middle_buff_flag, true,
        output_shape_global_addr,
        0  // no arm reserved buffer used
    );
  } else if (arch == BM1684X) {
    auto func_id = net_ctx->kernel_module_->get_dynamic_fullnet_func_id(core_list);
    status = bmfunc::bmdnn_1684x()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], func_id[0], stage->core_commands[0].ir_mem.addr, stage->core_commands[0].ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        stage->ctx_borders, stage->ctx_offset,
        stage->coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        net_ctx->do_allreduce == 1 ? &(net_ctx->allreduce_param) : NULL);
  } else if (arch == BM1688) {
    auto func_id = net_ctx->kernel_module_->get_dynamic_fullnet_func_id(core_list);
    status = bmfunc::bmdnn_1688()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], func_id, stage->core_commands[0].ir_mem.addr, stage->core_commands[0].ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        stage->ctx_borders, (m_flags & BM_RUNTIME_SHARE_MEM) ? stage->ctx_offset : net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->ctx_offset,
        stage->coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else if (arch == BM1690) {
    status = bmfunc::bmdnn_2260()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr, stage->core_commands[0].ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        stage->ctx_borders, (m_flags & BM_RUNTIME_SHARE_MEM) ? stage->ctx_offset : net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->ctx_offset,
        stage->coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else if (arch == MARS3) {
    status = bmfunc::bmdnn_mars3()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr, stage->core_commands[0].ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        stage->ctx_borders, stage->ctx_offset,
        stage->coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else if (arch == SG2380) {
    auto func_id = net_ctx->kernel_module_->get_dynamic_fullnet_func_id(core_list);
    status = bmfunc::bmdnn_2380()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], func_id, stage->core_commands[0].ir_mem.addr, stage->core_commands[0].ir_mem.dword_len, input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        stage->ctx_borders, (m_flags & BM_RUNTIME_SHARE_MEM) ? stage->ctx_offset : net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->ctx_offset,
        stage->coeff_offset, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else {
    BMRT_LOG(FATAL, "Unknown BM TPU");
  }

  if (status == BM_SUCCESS) {
    sync_cores(m_handles[devid], core_list);
  }

  if (BM_SUCCESS == status) {
    // update output shape
    #ifdef __linux__
    bm_shape_ex_t output_shape_v[output_num];
    #else
    std::shared_ptr<bm_shape_ex_t> output_shape_v_(new bm_shape_ex_t[output_num], std::default_delete<bm_shape_ex_t[]>());
    bm_shape_ex_t* output_shape_v = output_shape_v_.get();
    #endif
    status = bm_memcpy_d2s(m_handles[devid], output_shape_v, output_shape_mem);
    CHECK_status(status);
    for (int idx = 0; idx < output_num; idx++) {
      output_tensors[idx].shape = output_shape_v[idx].shape;
    }
  }

  return BM_SUCCESS == status;
}

api_info_t
Bmruntime::get_api_info(int net_idx, const bm_tensor_t *input_tensors,
                              int input_num, bm_tensor_t *output_tensors,
                              int output_num, bool user_mem, bool user_stmode,
                              uint32_t *core_ids) {
  api_info_t api_info;
  auto net_ctx = m_net_ctx_v[net_idx];
  int stage_idx = get_stage_idx(net_ctx, input_tensors);
  if (stage_idx == -1) {
    BMRT_LOG(WRONG, "Shapes of the input tensors are not supported");
    return api_info;
  }
  auto &stage = net_ctx->stage_v[stage_idx];

  // multi core info
  auto core_num = stage->core_commands.size();
  std::vector<int32_t> core_list;
  for (size_t idx = 0; idx < core_num; ++idx) {
    core_list.emplace_back(core_ids[idx]);
  }

  // init output tensors
  init_output_tensors(net_ctx, stage, output_tensors, user_mem, user_stmode);

  BMRT_ASSERT(!net_ctx->is_dynamic && stage->subnet_num == 1);

  uint32_t core_mask = get_dyn_core_mask(stage_idx, core_list);
  if (!(m_flags & BM_RUNTIME_SHARE_MEM)) {
    net_ctx_alloc_dyn_neuron(net_ctx, core_mask, stage, false);
  }

  tpu_net_info_t net_info;
  fill_tpu_net_info(net_ctx, stage, input_tensors, input_num, output_tensors,
                    output_num, core_list, net_info, core_mask);
  bmfunc::bmdnn_base()->fill_api_info(net_info, api_info);
  return api_info;
}

template <typename T_stage>
void Bmruntime::fill_tpu_tensor_info(
    std::vector<tpu_tensor_info_t> &tensor_info, const T_stage *stage,
    const bm_tensor_t *user_tensors, bool is_input) {
  auto ref_tensors = is_input ? stage->input_v : stage->output_v;
  return fill_tpu_tensor_info(tensor_info, ref_tensors, user_tensors, is_input);
}

void
Bmruntime::fill_tpu_net_info(net_ctx_t *net_ctx, net_stage_t *stage,
                             const bm_tensor_t *input_tensors, int input_num,
                             bm_tensor_t *output_tensors, int output_num,
                             const std::vector<int32_t> &core_list,
                             tpu_net_info_t &net_info,
                             const size_t dyn_core_mask) {
  std::vector<tpu_tensor_info_t> input_info;
  std::vector<tpu_tensor_info_t> output_info;
  if (m_flags & BM_RUNTIME_SHARE_MEM) {
    fill_tpu_tensor_info(input_info, stage, input_tensors, true);
    fill_tpu_tensor_info(output_info, stage, output_tensors, false);
  } else {
    fill_tpu_tensor_info(input_info, net_ctx->dyn_neuron_stage_dict[dyn_core_mask], input_tensors, true);
    fill_tpu_tensor_info(output_info, net_ctx->dyn_neuron_stage_dict[dyn_core_mask], output_tensors, false);
  }

  std::vector<tpu_single_core_cmd_t> core_command(core_list.size());
  for (size_t core_idx = 0; core_idx < core_list.size(); core_idx++) {
    std::vector<tpu_cmd_info_t> cmd_info;
    fill_tpu_cmd_info(cmd_info, stage->core_commands, core_idx);
    core_command[core_idx].cmd_info = std::move(cmd_info);
    core_command[core_idx].bdc_cmd_addr =
        stage->core_commands[core_idx].bdc_mem.addr +
        GLOBAL_MEM_CMD_START_OFFSET;
    core_command[core_idx].gdma_cmd_addr =
        stage->core_commands[core_idx].gdma_mem.addr +
        GLOBAL_MEM_CMD_START_OFFSET;
    core_command[core_idx].cdma_cmd_addr = 0;
    core_command[core_idx].hau_cmd_addr =
        stage->core_commands[core_idx].hau_mem.addr +
        GLOBAL_MEM_CMD_START_OFFSET;
    core_command[core_idx].sdma_cmd_addr =
        stage->core_commands[core_idx].sdma_mem.addr +
        GLOBAL_MEM_CMD_START_OFFSET;
  }

  memset(&net_info, 0x0, sizeof(tpu_net_info_t));
  net_info.input_info = std::move(input_info);
  net_info.output_info = std::move(output_info);
  net_info.core_commands = std::move(core_command);
  net_info.core_list = core_list;
  net_info.coeff_start_addr = stage->coeff_offset;
  if (m_flags & BM_RUNTIME_SHARE_MEM) {
    net_info.neuron_start_addr.assign(stage->ctx_offset.begin(),
                                    stage->ctx_offset.end());
  } else {
    net_info.neuron_start_addr.assign(net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->ctx_offset.begin(),
                                      net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->ctx_offset.end());
  }

  if (bmrt_arch_info::get_bmtpu_arch() == BM1684X ||
      bmrt_arch_info::get_bmtpu_arch() == BM1688 ) {
    net_info.kernel_func_ids = net_ctx->kernel_module_->get_multi_fullnet_func_id(core_list);
  }
  net_info.do_allreduce = net_ctx->do_allreduce;
  if (bmrt_arch_info::get_bmtpu_arch() == BM1684X && net_ctx->do_allreduce == 1) {
    net_info.allreduce_param = net_ctx->allreduce_param;
  }
  net_info.addr_mode = net_ctx->addr_mode;
}
bool Bmruntime::launch_static(net_ctx_t* net_ctx, net_stage_t* stage,
                              const bm_tensor_t* input_tensors, int input_num,
                              bm_tensor_t* output_tensors, int output_num,
                              const std::vector<int32_t> &core_list,
                              const size_t dyn_core_mask)
{
  auto devid = net_ctx->device_id;

  tpu_net_info_t net_info;
  fill_tpu_net_info(net_ctx, stage, input_tensors, input_num, output_tensors,
                    output_num, core_list, net_info, dyn_core_mask);
  if (m_profile->is_enabled()) {
    for(size_t core_idx=0; core_idx < core_list.size(); core_idx++){
      const size_t group_num = stage->core_commands[core_idx].bdc_id.size();
      auto cmd_num = m_profile->record_subnet_cmd_info(core_idx,
          stage->core_commands[core_idx].gdma_mem.addr, GLOBAL_MEM_CMD_START_OFFSET,
          stage->core_commands[core_idx].bdc_mem.addr, GLOBAL_MEM_CMD_START_OFFSET,
          group_num);
      for (size_t i = 0; i < group_num; i++) {
        cmd_num[i].bdc = net_info.core_commands[core_idx].cmd_info.at(i).bdc_cmd_num;
        cmd_num[i].gdma = net_info.core_commands[core_idx].cmd_info.at(i).gdma_cmd_num;
      }

    }
  }
  bm_status_t status = bmfunc::bmdnn_base()->_bmdnn_multi_fullnet_(m_handles[devid], net_info);
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

static inline void saveDataExt(bm_handle_t handle, const std::string &prefix,
                               const bm_tensor_t *tensors, int num) {
    auto bin_filename = prefix+".dat";
    saveData(handle, bin_filename, tensors, num);
    auto info_filename = prefix+".txt";
    std::ofstream f(info_filename, std::ios::out);
    if (f.fail()) {
        BMRT_LOG(FATAL, "Failed to write %s", info_filename.c_str());
    }
    for (int i = 0; i < num; ++i) {
        auto t = tensors[i];
        f<<"#"<<i<<":";
        f<<"addr=0x"<<std::hex<<bm_mem_get_device_addr(t.device_mem)<<",";
        f<<"size="<<std::dec<<bm_mem_get_device_size(t.device_mem)<<",";
        f<<"dtype="<<std::dec<<t.dtype<<",";

        f<<"shape=[";
        for(int s = 0; s<t.shape.num_dims-1; s++){
          f<<t.shape.dims[s]<<",";
        }
        if(t.shape.num_dims>0){
          f<<t.shape.dims[t.shape.num_dims-1];
        }
        f<<"]";

        f<<std::endl;
        auto size = bmrt_tensor_bytesize(&t);
    }
    f.close();
}

void Bmruntime::init_output_tensors(net_ctx_t* net_ctx, net_stage_t* stage,
                                    bm_tensor_t* output_tensors, bool user_mem, bool user_stmode)
{
  auto devid = net_ctx->device_id;
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
      must_alloc_device_mem(devid, &output.device_mem, mem_size, "output_mem");
    }
  }
}

bool Bmruntime::launch(int net_idx, const bm_tensor_t *input_tensors,
                       int input_num, bm_tensor_t *output_tensors,
                       int output_num, bool user_mem, bool user_stmode) {
  auto net_ctx = m_net_ctx_v[net_idx];
  int stage_idx = get_stage_idx(net_ctx, input_tensors);
  if (stage_idx == -1) {
    BMRT_LOG(WRONG, "Shapes of the input tensors are not supported");
    return false;
  }
  auto &stage = net_ctx->stage_v[stage_idx];
  // multi core info
  auto core_num = stage->core_commands.size();
  std::vector<int32_t> core_list(core_num);
  std::iota(core_list.begin(), core_list.end(), 0);

  return launch_multi_cores(net_idx, input_tensors, input_num, output_tensors,
                            output_num, 0, core_list, user_mem, user_stmode);
}

std::vector<int32_t>
Bmruntime::refine_core_list(const net_stage_t *stage,
                            const std::vector<int32_t> &core_list,
                            bm_handle_t handle) {
  // check valid core_list
  uint32_t arch_core_num;
  bm_get_tpu_scalar_num(handle, &arch_core_num);
  for (auto &core_idx : core_list) {
    if (core_idx >= arch_core_num || core_idx < 0) {
      BMRT_LOG(FATAL, "invalid core_id:%d, arch max core id:%d\n", core_idx, arch_core_num-1);
    }
  }

  // use  core_num to generate core_list
  std::vector<int32_t> full_core_list(m_core_num);
  std::iota(full_core_list.begin(), full_core_list.end(), 0);
  auto bmodel_core_num = stage->core_commands.size();
  if (bmodel_core_num > full_core_list.size()) {
    BMRT_LOG(FATAL,
            "(%d) the bmodel is not compatible with the current target.\n",
             full_core_list.size() * 14);
    return {};
  }
  std::vector<int32_t> final_core_list;
  for (auto core_id : core_list) {
    if (core_id < m_core_num &&
        std::find(final_core_list.begin(), final_core_list.end(), core_id) ==
            final_core_list.end())
      final_core_list.emplace_back(core_id);
  }
  if (final_core_list.size() > bmodel_core_num) {
    final_core_list.resize(bmodel_core_num);
  } else if (final_core_list.size() < bmodel_core_num) {
    for (auto core_id : full_core_list) {
      if (std::find(final_core_list.begin(), final_core_list.end(), core_id) ==
          final_core_list.end())
        final_core_list.emplace_back(core_id);
      if (final_core_list.size() == bmodel_core_num)
        break;
    }
  }

  BMRT_LOG_RUN(DEBUG, {
    std::string core_list_str = vector_to_string(final_core_list);
    BMRT_LOG(DEBUG, "Launch on cores: %s", core_list_str.c_str());
  });

  return std::move(final_core_list);
}

/*
  dyn_core_mask record the unique core-mession, combine with core_list
  for exp: max_arch_core_num = 8, core_list = 0,1       core_mask = 00000011
           max_arch_core_num = 8, core_list = 1         core_mask = 00000010
*/
uint32_t Bmruntime::get_dyn_core_mask(int stage_idx, const std::vector<int32_t> core_list) {
  uint32_t core_mask = 0;
  if (m_core_num > 1) { // use core_num to judge multi_core arch
    if (stage_idx > ((std::numeric_limits<uint32_t>::max)() >> m_core_num)) {
      BMRT_LOG(FATAL, "get dyn neuron code overlap");
    }
    // use core_num to get mask with size of core_num
    // core_mask = stage_idx << m_core_num;
    for (auto &core_idx : core_list) {
      core_mask |= (1 << core_idx);
    }
  }
  return core_mask;
}

std::vector<int> Bmruntime::get_core_list_from_core_mask(uint32_t dyn_core_mask) {
  std::vector<int> core_list;
  // use core_num to generate mask and get core_list
  for (size_t i = 0; i < m_core_num; ++i) {
    if (dyn_core_mask & 0x1) {
      core_list.emplace_back(i);
    }
    dyn_core_mask >>= 1;
  }
  if (core_list.empty()) core_list = {0};
  return core_list;
}

void Bmruntime::net_ctx_alloc_dyn_neuron(net_ctx_t* net_ctx, const uint64_t thread_id, const uint64_t dyn_core_mask) {
  if (net_ctx->dyn_neuron_stage_dict.find(dyn_core_mask) != net_ctx->dyn_neuron_stage_dict.end()) {
    return;
  }

  std::unique_lock<std::mutex> neuron_stage_lock(net_ctx->neuron_mutex);
  if (net_ctx->dyn_neuron_stage_dict.find(dyn_core_mask) != net_ctx->dyn_neuron_stage_dict.end()) {
    return;
  }
  auto devid = net_ctx->device_id;
  auto dyn_neuron_info = new dyn_neuron_stage_t();
  u64 neuron_num = (m_flags & BM_RUNTIME_SHARE_MEM) ? net_ctx->neuron_mem.size() : net_ctx->neuron_size.size();
  dyn_neuron_info->neuron_mem.resize(neuron_num);
  std::string suffix = "_" + std::to_string(thread_id);
  for (size_t i = 0; i < neuron_num; ++i) {
    auto &mem = dyn_neuron_info->neuron_mem[i];
    u64 neuron_size = (m_flags & BM_RUNTIME_SHARE_MEM) ? net_ctx->neuron_mem[i].size : net_ctx->neuron_size[i];
    alloc_device_mem_u64(devid, mem, neuron_size, "neuron_mem" + suffix, 1, false);
  }

  net_ctx->dyn_neuron_stage_dict.emplace(dyn_core_mask, dyn_neuron_info);

  neuron_stage_lock.unlock();
}

void Bmruntime::update_dyn_neuron(net_ctx_t* net_ctx, const size_t dyn_core_mask,
  const net_stage_t *common_stage_info) {
  if (net_ctx->dyn_neuron_stage_dict.find(dyn_core_mask) == net_ctx->dyn_neuron_stage_dict.end()) {
    net_ctx_alloc_dyn_neuron(net_ctx, dyn_core_mask, dyn_core_mask);
  }

  auto devid = net_ctx->device_id;
  auto dyn_neuron_info = net_ctx->dyn_neuron_stage_dict[dyn_core_mask];

  auto &ctx_sizes = common_stage_info->neuron_size;
  auto ctx_start = common_stage_info->ctx_start & bmrt_arch_info::addr_mask();
  if (!ctx_sizes.empty()) {
    dyn_neuron_info->ctx_offset.resize(ctx_sizes.size());
    for (size_t i = 0; i < ctx_sizes.size(); ++i)
    {
      u64 ctx_addr = bm_mem_get_device_addr_u64(dyn_neuron_info->neuron_mem[i]);
      dyn_neuron_info->ctx_offset[i] = ctx_addr - ctx_start;
      if (i > 0)
      {
        dyn_neuron_info->ctx_offset[i] -= common_stage_info->ctx_borders[i - 1];
      }
    }
  } else {
    dyn_neuron_info->ctx_offset.emplace_back(0);
  }

  dyn_neuron_info->input_v = common_stage_info->input_v;
  dyn_neuron_info->output_v = common_stage_info->output_v;

  if (common_stage_info->io_size == 0) {
    for (size_t i = 0; i < dyn_neuron_info->input_v.size(); ++i) {
      u64 io_addr = bm_mem_get_device_addr(common_stage_info->input_v[i].dev_mem);
      int tag = ((io_addr >> 36) & 0x7);
      if (tag < 3) {
        io_addr += dyn_neuron_info->ctx_offset[get_mem_index(
            common_stage_info->ctx_borders, common_stage_info->ctx_start,
            io_addr)];
        io_addr &= bmrt_arch_info::addr_mask();
        dyn_neuron_info->input_v[i].dev_mem = bm_mem_from_device(
            io_addr, common_stage_info->input_v[i].dev_mem.size);
      }
    }

    for (size_t i = 0; i < dyn_neuron_info->output_v.size(); ++i) {
      u64 io_addr = bm_mem_get_device_addr(common_stage_info->output_v[i].dev_mem);
      int tag = ((io_addr >> 36) & 0x7);
      if (tag < 3) {
        io_addr += dyn_neuron_info->ctx_offset[get_mem_index(
            common_stage_info->ctx_borders, common_stage_info->ctx_start,
            io_addr)];
        io_addr &= bmrt_arch_info::addr_mask();
        dyn_neuron_info->output_v[i].dev_mem = bm_mem_from_device(
            io_addr, common_stage_info->output_v[i].dev_mem.size);
      }
    }
  }

  fill_subnet_dyn_neuron_tensor(net_ctx, dyn_core_mask, common_stage_info);
}

void Bmruntime::net_ctx_alloc_dyn_neuron(net_ctx_t* net_ctx, const size_t dyn_core_mask,
    const net_stage_t *common_stage_info, bool use_multi_subnet) {
  if (net_ctx->dyn_neuron_stage_dict.find(dyn_core_mask) == net_ctx->dyn_neuron_stage_dict.end()) {
    net_ctx_alloc_dyn_neuron(net_ctx, dyn_core_mask, dyn_core_mask);
  }
  update_dyn_neuron(net_ctx, dyn_core_mask, common_stage_info);
}

void Bmruntime::fill_subnet_dyn_neuron_tensor(
  net_ctx_t* net_ctx, const size_t dyn_core_mask,
  const net_stage_t *common_stage_info) {
  for (auto &subnet_tensor : common_stage_info->subnet_tensor_v) {
    tensor_ext_t bm_tensor_ext = subnet_tensor.second;
    std::string tensor_name = subnet_tensor.first;

    if (bm_tensor_ext.mem_type & 0x1 == MEM_TYPE_TPU) {
      u64 tensor_addr = bm_tensor_ext.tensor_info.device_mem.u.device.device_addr; //fake device addr, record bmodel compile addr
      u32 tensor_size = bm_tensor_ext.tensor_info.device_mem.size;
      if (tensor_addr < common_stage_info->ctx_start) {
        bm_tensor_ext.tensor_info.device_mem = bm_mem_from_device(
                                                (tensor_addr & bmrt_arch_info::addr_mask()) + common_stage_info->coeff_offset, tensor_size);
      } else {
        u32 idx = get_mem_index(common_stage_info->ctx_borders, common_stage_info->ctx_start, tensor_addr);
        tensor_addr += net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->ctx_offset[idx];
        tensor_addr &= bmrt_arch_info::addr_mask();
        bm_tensor_ext.tensor_info.device_mem = bm_mem_from_device(tensor_addr, tensor_size);
      }
    }

    if (bm_tensor_ext.mem_type >= MEM_TYPE_CPU) {
      float* host_mem = NULL;
      bool need_mem_alloc = true;
      if (bm_tensor_ext.host_mem.type == HOST_MEM_MMAP) {
#ifndef SOC_MODE
        BMRT_LOG(FATAL, "Only soc mode run here");
#else
        bm_status_t ret = bm_mem_mmap_device_mem(m_handles[net_ctx->device_id], &bm_tensor_ext.tensor_info.device_mem, (u64 *)&host_mem);
        if (ret == BM_SUCCESS) {
          need_mem_alloc = false;
        } else {
          BMRT_LOG(WRONG, "mmap failed, malloc host memory");
        }
#endif
      }

      if (need_mem_alloc) {
        if (!net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->cpu_addr
            && common_stage_info->cpu_mem_size > 0) {
          net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->cpu_addr = new float[common_stage_info->cpu_mem_size];
        }
        if (net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->cpu_addr) {
          host_mem = net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->cpu_addr + bm_tensor_ext.host_mem.tensor_cpu_addr;
        } else {
          host_mem = new float[bm_tensor_ext.host_mem.size];
        }
        bm_tensor_ext.host_mem.type = HOST_MEM_ALLOC;
      }
      bm_tensor_ext.host_mem.addr = host_mem;
    }

    net_ctx->dyn_neuron_stage_dict[dyn_core_mask]->subnet_tensor_v.insert(make_pair(tensor_name, bm_tensor_ext));
  }
}

void Bmruntime::pre_alloc_neuron_multi_cores(int net_idx, int stage_idx, const std::vector<int> &core_list) {
  if (m_flags & BM_RUNTIME_SHARE_MEM) {
    return;
  }
  auto net_ctx = m_net_ctx_v[net_idx];
  auto& stage = net_ctx->stage_v[stage_idx];
  auto final_core_list = refine_core_list(stage, core_list, m_handles[net_ctx->device_id]);
  uint32_t core_mask = get_dyn_core_mask(stage_idx, final_core_list);
  bool use_multi_subnet = stage->subnet_num > 1 ||
                          (stage->subnet_num == 1 && stage->subnet_v[0]->subnet_mode == SUBNET_MODE_CPU);
  net_ctx_alloc_dyn_neuron(net_ctx, core_mask, stage, use_multi_subnet);
}

void Bmruntime::pre_alloc_neuron_multi_thread(uint64_t thread_idx, const mem_info_t* mem_info) {
  if (m_flags & BM_RUNTIME_SHARE_MEM) {
    return;
  }
  std::string suffix = "_" + std::to_string(thread_idx);
  for (int i = 0; i < m_net_ctx_v.size(); ++i) {
    clear_device_mem("neuron_mem" + suffix);
    clear_device_mem("middle_buffer" + suffix);
    clear_device_mem("dynamic_out" + suffix);
    clear_device_mem("io_mem" + suffix);
    auto iter = find(dmem_info.begin(), dmem_info.end(), "neuron_mem" + suffix);
    dmem_info.erase(iter);
    uint64_t offset = 0;
    int64_t base_addr = mem_info->neuron_mem.addr;
    auto dynamic_out_size = m_net_ctx_v[i]->mem_info_dict["dynamic_output_mem"];
    BMRT_ASSERT_INFO(offset + dynamic_out_size <= mem_info->neuron_mem.size, "");
    fill_dmem_info(base_addr + offset, dynamic_out_size, "dynamic_out" + suffix);
    offset += ALIGN(dynamic_out_size, 128);
    auto neuron_mem_size = m_net_ctx_v[i]->mem_info_dict["neuron_mem"];
    BMRT_ASSERT_INFO(offset + neuron_mem_size <= mem_info->neuron_mem.size, "");
    fill_dmem_info(base_addr, neuron_mem_size, "neuron_mem" + suffix);
    offset += ALIGN(neuron_mem_size, 128);
    auto middle_buffer_size = m_net_ctx_v[i]->mem_info_dict["middle_buffer"];
    BMRT_ASSERT_INFO(offset + middle_buffer_size <= mem_info->neuron_mem.size, "");
    fill_dmem_info(base_addr + offset, middle_buffer_size, "middle_buffer" + suffix);
    fill_dmem_info(mem_info->io_mem.addr, mem_info->io_mem.size, "io_mem" + suffix);

    net_ctx_alloc_dyn_neuron(m_net_ctx_v[i], thread_idx, thread_idx);
  }
}

bool Bmruntime::launch_multi_cores(int net_idx,
                                   const bm_tensor_t *input_tensors,
                                   int input_num, bm_tensor_t *output_tensors,
                                   int output_num, uint64_t thread_idx,
                                   const std::vector<int> &core_list,
                                   bool user_mem, bool user_stmode, bool using_thread) {
  auto net_ctx = m_net_ctx_v[net_idx];
  auto devid = net_ctx->device_id;
  bool save_io = false;
  char *nt = getenv("BMRT_SAVE_IO_TENSORS");
  if (nt != nullptr)
      save_io = static_cast<bool>(atoi(nt));
  if (save_io)
      saveData(m_handles[devid], "input_ref_data.dat.bmrt", input_tensors, input_num);

  static int save_count = 0;
  BMRT_LOG_RUN(DUMP, {
      std::string filename = std::string("tensor_dev")+std::to_string(devid) + "_" +std::to_string(save_count) + "_in";
      saveDataExt(m_handles[devid], filename, input_tensors, input_num);
  });

  // check parameters
  int stage_idx = get_stage_idx(net_ctx, input_tensors);
  if (stage_idx == -1) {
      BMRT_LOG(WRONG, "Shapes of the input tensors are not supported");
    return false;
  }
  auto& stage = net_ctx->stage_v[stage_idx];

  // process core list
  auto final_core_list = refine_core_list(stage, core_list, m_handles[devid]);

  if (false == check_launch_params(net_ctx, stage, input_tensors, input_num, output_tensors, output_num, user_stmode)) {
    return false;
  }

  bool use_multi_subnet = stage->subnet_num > 1 ||
                          (stage->subnet_num == 1 && stage->subnet_v[0]->subnet_mode == SUBNET_MODE_CPU);

  uint64_t core_mask = using_thread ? thread_idx : get_dyn_core_mask(stage_idx, final_core_list);;
  if (!(m_flags & BM_RUNTIME_SHARE_MEM)) {
    net_ctx_alloc_dyn_neuron(net_ctx, core_mask, stage, use_multi_subnet);
  }

  // init output tensors
  init_output_tensors(net_ctx, stage, output_tensors, user_mem, user_stmode);

  bool ret = true;
  m_profile->init(net_ctx->net_name, stage->net_profile, stage->net_stat, final_core_list);

  if (use_multi_subnet) {
        ret = launch_multi_subnet(net_ctx, stage, input_tensors, input_num, output_tensors,
                                     output_num, core_mask);
  } else {
      m_profile->begin_subnet(net_ctx, 0, 0, SUBNET_MODE_TPU);
      m_profile->set_extra_data(net_ctx->is_dynamic);
      if(net_ctx->is_dynamic) {
          // launch_ir calls bm_thread_sync internally
          ret = launch_ir(net_ctx, stage, input_tensors, input_num, output_tensors, output_num, core_mask);
      } else {
          // launch_static does not call bm_thread_sync internally
          ret = launch_static(net_ctx, stage, input_tensors, input_num, output_tensors, output_num, final_core_list, core_mask);
          // so sync at some cases
          if(m_profile->is_enabled() || save_io){
            sync_cores(m_handles[devid], final_core_list);
          }
      }
      m_profile->end_subnet(net_ctx);
  }

  m_profile->deinit();

  if (save_io)
      saveData(m_handles[devid], "output_ref_data.dat.bmrt", output_tensors, output_num);

  BMRT_LOG_RUN(DUMP, {
      sync_cores(m_handles[devid], final_core_list);
      std::string filename = std::string("tensor_dev")+std::to_string(devid) + "_" +std::to_string(save_count) + "_out";
      saveDataExt(m_handles[devid], filename, output_tensors, output_num);
      save_count++;
  });

  // free output mem if failed
  if (ret == false && user_mem == false) {
    for (int idx = 0; idx < output_num; idx++) {
      must_free_device_mem(devid, output_tensors[idx].device_mem);
    }
  }
  return ret;
}
static mem_cascade_t *
get_tensor(std::vector<mem_cascade_t> *tensors, const std::string &name) {
  for (auto &t : *tensors) {
    if (t.name == name) {
      return &t;
    }
  }
  return nullptr;
}

static mem_cascade_t *
get_tensor(std::vector<mem_cascade_t> *tensors, const std::string &name,
           int32_t devid) {
  for (auto &t : *tensors) {
    if (t.name == name && t.device_id == devid) {
      return &t;
    }
  }
  return nullptr;
}

// std::atomic<long> comm_time{0};
// std::atomic<int> comm_count{0};

bm_tensor_t *
Bmruntime::cascade_prepare_input(const std::string &name,
                                 int32_t devid,
                                 std::vector<mem_cascade_t> *src,
                                 std::vector<mem_cascade_t> *dst) {
  auto from = get_tensor(dst, name, devid);
  if (!from) {
    from = get_tensor(dst, name);
    if (!from) {
      return nullptr;
    }
  }
  if (from->device_id == devid) {
    return &from->tensor;
  }

  auto to = get_tensor(src, name, devid);
  if (!to) {
    return nullptr;
  }

//  #ifdef __linux__
//  struct timeval t1, t2;
//  gettimeofday(&t1, NULL);
//  #else
//  struct timespec t1, t2;
//  bmrt_clock_gettime(0, &t1);
//  #endif

  bm_tensor_t from_tensor, to_tensor;
  bmrt_tensor_with_device(&from_tensor, from->tensor.device_mem,
                          from->tensor.dtype, from->tensor.shape);
  bmrt_tensor_with_device(&to_tensor, to->tensor.device_mem,
                          to->tensor.dtype, from->tensor.shape);
  bm_memcpy_p2p(m_handles[from->device_id], from->tensor.device_mem,
                m_handles[devid], to->tensor.device_mem);
  // when net is dynamic, input_shape need to be changed to its real shape
  to->tensor.shape = from->tensor.shape;

//  #ifdef __linux__
//  gettimeofday(&t2, NULL);
//  long use1 = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
//  #else
//  bmrt_clock_gettime(0, &t2);
//  long use1 = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec)/1000;
//  #endif
//
//  comm_time += use1;
//  comm_count += 1;

  return &to->tensor;
}

bm_tensor_t *
Bmruntime::cascade_prepare_output(const std::string &name, uint32_t devid,
                                  std::vector<mem_cascade_t> *dst) {
  auto from = get_tensor(dst, name, devid);
  if (!from) {
    return nullptr;
  }
  return &from->tensor;
}

static bool update_tensor_shape(std::vector<mem_cascade_t> *tensors,
                                const std::string &name, int32_t devid,
                                const bm_tensor_t &ref) {
  for (auto &t : *tensors) {
    if (t.name == name && t.device_id == devid) {
      t.tensor.shape = ref.shape;
      return true;
    }
  }
  return false;
}

bool Bmruntime::cascade_update_output_shape(net_ctx_t *net_ctx,
                                            std::vector<mem_cascade_t> *dst,
                                            std::vector<bm_tensor_t> out_tensors) {
  int out_num = net_ctx->output_name_v.size();
  bool ret = false;
  for (int i = 0; i < out_num; i++) {
    ret = update_tensor_shape(dst, net_ctx->output_name_v[i],
                              net_ctx->device_id, out_tensors[i]);
    if (!ret) {
      return false;
    }
  }
  return true;
}

bool Bmruntime::cascade_thread_step(int net_idx,
                                    vector<mem_cascade_t> *src,
                                    vector<mem_cascade_t> *dst,
                                    bm_handle_t m_handle) {
  auto ctx = m_net_ctx_v[net_idx];
  int in_num = ctx->input_name_v.size();
  int out_num = ctx->output_name_v.size();
  std::vector<bm_tensor_t> in_tensors(in_num);
  std::vector<bm_tensor_t> out_tensors(out_num);
  for (int i = 0; i < in_num; i++) {
    auto in = cascade_prepare_input(ctx->input_name_v[i],
                                    ctx->device_id, src, dst);
    if (in == nullptr) {
      BMRT_LOG(WRONG, "input tensor[%s] are not in %d",
               ctx->input_name_v[i].c_str(), ctx->device_id);
      return false;
    }
    in_tensors[i] = *in;
  }
  for (int i = 0; i < out_num; i++) {
    auto out =
        cascade_prepare_output(ctx->output_name_v[i], ctx->device_id, dst);
    if (out == nullptr) {
      BMRT_LOG(WRONG, "output tensor[%s] are not in %d",
               ctx->output_name_v[i].c_str(), ctx->device_id);
      return false;
    }
    out_tensors[i] = *out;
  }

  auto ret = launch(net_idx, in_tensors.data(), in_num, out_tensors.data(),
                    out_num, true, false);

  if (ctx->is_dynamic) {
    ret = cascade_update_output_shape(ctx, dst, out_tensors);
  }

  if (!ret) {
    BMRT_LOG(WRONG, "launch %d is not correct", net_idx);
    return false;
  }
  return true;
}

bool Bmruntime::cascade_thread_global_move_data(
    int devid, bm_handle_t handle,
    std::vector<tpu_kernel_global_move_1684x_t> *param) {
  std::vector<int> core_list{0};
  auto func_id = kernel_modules[devid]->get_global_move_1684x_func_id(core_list)[0];
  int tensor_num = param->size();
  for (int i = 0; i < tensor_num; i++) {
    auto status = tpu_kernel_launch_async(handle, func_id, &(param->at(i)), sizeof(tpu_kernel_global_move_1684x_t));
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "global_move_1684x launch failed! dev_id=%d, func_id=%d, tensor_index=%d/%d",
          devid, func_id, i, tensor_num);
      return false;
    }
  }
  auto status = bm_thread_sync(handle);
  if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "global_move_1684x sync failed! dev_id=%d, func_id=%d", devid, func_id);
      return false;
  }
  return true;
}

bool Bmruntime::launch(const net_cascade_t *net_c,
                       const bm_tensor_t *input_tensors, int input_num,
                       bm_tensor_t *output_tensors, int output_num) {
  // run step by step
  if ((size_t)input_num != net_c->input_names.size() ||
      (size_t)output_num != net_c->output_names.size()) {
    BMRT_LOG(WRONG, "launch parameter is not correct");
    return false;
  }
  // prepare all inputs and outputs
  std::vector<mem_cascade_t> src(net_c->hidden_inputs);
  std::vector<mem_cascade_t> dst(net_c->hidden_outputs);
  for (size_t i = 0; i < input_num; i++) {
    int devid = net_c->net_info.input_loc_devices[i];
    dst.emplace_back(mem_cascade_t{net_c->input_names[i], devid, input_tensors[i]});
  }
  for (size_t i = 0; i < output_num; i++) {
    int devid = net_c->net_info.output_loc_devices[i];
    dst.emplace_back(mem_cascade_t{net_c->output_names[i], devid, output_tensors[i]});
  }

  for (size_t s = 0; s < net_c->step_ids.size(); s++) {
    // TODO: device_num = 2 fast_allreduce still have bug
    if (using_fast_allreduce &&
        net_c->step_ids[0].size() == m_device_num &&
        net_c->step_ids.size() > 1 &&
        (m_device_num == 4 || m_device_num == 6 || m_device_num == 8)) {
      bool skip = (m_device_num == 8 && (s == 1 || s == 2 || s == 3 || s == 5 || s == 6 || s == 7)) ||
                  (m_device_num == 6 && (s == 1 || s == 2 || s == 3 || s == 5 || s == 6 || s == 7)) ||
                  (m_device_num == 4 && (s == 1 || s == 2 || s == 4 || s == 5)) ||
                  (m_device_num == 2 && (s == 1 || s == 3));
      if (skip) {
        if (net_c->is_dynamic) {
          for (int devid = 0; devid < net_c->step_ids[s].size(); devid++) {
            auto &net_ctx = m_net_ctx_v[net_c->step_ids[s][devid]];
            for (int idx = 0; idx < net_ctx->output_name_v.size(); idx++) {
              auto out_name = net_ctx->output_name_v[idx];
              for (auto &t : dst) {
                if (t.name == out_name && t.device_id == devid) {
                  t.tensor.shape.dims[1] = input_tensors[0].shape.dims[1];
                }
              }
            }
          }
        }
        continue;
      }

      // set all-reduce param
      if ((m_device_num == 8 && (s == 0 || s == 4)) ||
          (m_device_num == 6 && (s == 0 || s == 4)) ||
          (m_device_num == 4 && (s == 0 || s == 3)) ||
          (m_device_num == 2 && (s == 0 || s == 2))) {
        // use step 0 output addr as all-reduce input addr
        // use step 1 input addr as all-reduce buffer addr
        // use step 3 output addr as all-reduce output addr
        // if do all-reduce twice, use second all-reduce input addr as first all-reduce output addr
        int cur_s = s;
        int next_s = s + 1;
        int last_s = s + 3;
        if (m_device_num == 4) last_s = s + 2;
        if (m_device_num == 2) last_s = s + 1;

        std::vector<tpu_kernel_allreduce_1684x_t> params(net_c->step_ids[next_s].size());

        for (int devid = 0; devid < net_c->step_ids[next_s].size(); devid++) {
          bm_tensor_t in_tensor, in1_tensor, out_tensor;
          auto cur_net_idx = net_c->step_ids[cur_s];
          auto cur_ctx = m_net_ctx_v[cur_net_idx[devid]];
          auto next_net_idx = net_c->step_ids[next_s];
          auto next_ctx = m_net_ctx_v[next_net_idx[devid]];
          auto next_in_name_v = next_ctx->input_name_v;

          // find input and buffer tensor by step 1 input names
          std::string in_name = "";
          std::string in1_name = next_in_name_v[1];
          for (int idx = 0; idx < cur_ctx->output_name_v.size(); idx++) {
            auto name = cur_ctx->output_name_v[idx];
            if (std::find(next_in_name_v.begin(), next_in_name_v.end(), name) != next_in_name_v.end()) {
              in_name = name;
              break;
            }
          }
          assert(in_name != "");
          in_tensor = get_tensor(&dst, in_name, devid)->tensor;
          in1_tensor = get_tensor(&src, in1_name, devid)->tensor;

          auto last_net_idx = net_c->step_ids[last_s];
          auto last_ctx = m_net_ctx_v[last_net_idx[devid]];
          auto last_out_name_v = last_ctx->output_name_v;
          std::string out_name = "";
          // if do second all-reduce, use second all-reduce step 0 input addr
          if ((m_device_num == 8 && net_c->step_ids.size() == 8 && cur_s == 0) ||
              (m_device_num == 6 && net_c->step_ids.size() == 8 && cur_s == 0) ||
              (m_device_num == 4 && net_c->step_ids.size() == 6 && cur_s == 0) ||
              (m_device_num == 2 && net_c->step_ids.size() == 4 && cur_s == 0)) {
            auto new_net_idx = net_c->step_ids[last_s+1];
            auto new_ctx = m_net_ctx_v[new_net_idx[devid]];
            for (int idx = 0; idx < new_ctx->input_name_v.size(); idx++) {
              auto name = new_ctx->input_name_v[idx];
              if (std::find(last_out_name_v.begin(), last_out_name_v.end(), name) != last_out_name_v.end()) {
                out_name = name;
                break;
              }
            }
          } else {
            auto out_name_v = net_c->output_names;
            for (int idx = 0; idx < last_out_name_v.size(); idx++) {
              auto name = last_out_name_v[idx];
              if (std::find(out_name_v.begin(), out_name_v.end(), name) != out_name_v.end()) {
                out_name = name;
                break;
              }
            }
          }
          assert(out_name != "");
          out_tensor = get_tensor(&dst, out_name, devid)->tensor;

          // size should be aligned to 4096 for allreduce
          bm_data_type_t bm_dtype = in_tensor.dtype;
          size_t type_size = bmrt_data_type_size(bm_dtype);
          uint64_t real_count = bmrt_shape_count(&in_tensor.shape);
          uint64_t aligned_count = ALIGN(type_size * real_count, 4096) / type_size;

          int dtype = 0;
          if (bm_dtype == BM_FLOAT32) {
            dtype = (2 << 1) | 1;
          } else if (bm_dtype == BM_FLOAT16) {
            dtype = (1 << 1) | 1;
          } else if (bm_dtype == BM_BFLOAT16) {
            dtype = (5 << 1) | 1;
          } else {
            BMRT_LOG(WRONG, "Allreduce only support float32/float16/bfloat16 now");
          }

          tpu_kernel_allreduce_1684x_t param;
          memset(&param, 0, sizeof(param));
          param.count = aligned_count;
          param.dtype = dtype;
          param.reduce_method = 1;
          for(int i=0; i<sizeof(param.group)/sizeof(param.group[0]); i++) {
              param.group[i] = i;
          }
          param.rank = devid;
          param.chip_num = 8; // card_chip_num,
          param.group_size = m_device_num;

          // clear addr high 6 bits because tpu-train use those bits as device id
          // TODO: move the logic to tpu-train
          param.i_global_addr[devid] = (in_tensor.device_mem.u.device.device_addr << 6) >> 6;
          param.i_global_addr_1[devid] = (in1_tensor.device_mem.u.device.device_addr << 6) >> 6;
          param.o_global_addr[devid] = (out_tensor.device_mem.u.device.device_addr << 6) >> 6;
          params[devid] = param;
        }

        for (int i = 0; i < net_c->step_ids[next_s].size(); i++) {
          for (int j = 0; j < net_c->step_ids[next_s].size(); j++) {
            params[i].i_global_addr[j] = params[j].i_global_addr[j];
            params[i].i_global_addr_1[j] = params[j].i_global_addr_1[j];
            params[i].o_global_addr[j] = params[j].o_global_addr[j];
          }
        }

        for (int devid = 0; devid < net_c->step_ids[cur_s].size(); devid++) {
          auto net_idx = net_c->step_ids[cur_s];
          auto net_ctx = m_net_ctx_v[net_idx[devid]];
          net_ctx->do_allreduce = 1;
          net_ctx->allreduce_param = params[devid];
        }

      }
    }

    std::set<int> devices;
    if (net_c->step_ids[s].size() > 1 &&
        net_c->num_device > 1 &&
        m_device_num > 1) {
      for (auto net_idx : net_c->step_ids[s]) {
        auto devid = m_net_ctx_v[net_idx]->device_id;
        if (devices.find(devid) != devices.end()) {
          m_cascade_thread_v[devid]->sync();
        }
        devices.insert(devid);
        m_cascade_thread_v[devid]->run(net_idx, &src, &dst);
      }
      for (auto d: devices) {
        if (false == m_cascade_thread_v[d]->sync()){
          return false;
        }
      }
    } else {
      // single device
      for (auto net_idx : net_c->step_ids[s]) {
        int devid = m_net_ctx_v[net_idx]->device_id;
        devices.insert(devid);
        if (false == cascade_thread_step(net_idx, &src, &dst, m_handles[devid])) {
          return false;
        }
      }
      for (auto d : devices) {
        bm_handle_sync(m_handles[d]);
      }
    }
  }

  if (net_c->is_dynamic) {
    for (int i = 0; i < output_num; i++) {
      int devid = net_c->net_info.output_loc_devices[i];
      std::string name = net_c->output_names[i];
      auto out = get_tensor(&dst, name, devid);
      output_tensors[i].shape = out->tensor.shape;
    }
  }
  return true;
}

bool Bmruntime::memcpy_s2d_parallel(bm_tensor_t tensors[],
                                    void * datas[],
                                    int tensor_num[],
                                    int device_num) {
  if (m_cascade_thread_v.size() == 0 && device_num == 1) {
    auto status = bm_memcpy_s2d(m_handles[0], tensors[0].device_mem, datas[0]);
    return BM_SUCCESS == status;
  }

  if (m_cascade_thread_v.size() < device_num) {
    BMRT_LOG(WRONG, "It doesn't support s2d parallel because device_num %d is larger than cascade_net thread_num %d.",
             device_num, m_cascade_thread_v.size());
  }
  int offset = 0;
  for (int d = 0; d < device_num; ++d) {
    m_cascade_thread_v[d]->s2d(tensors+offset, datas+offset, tensor_num[d]);
    offset += tensor_num[d];
  }

  for (int d = 0; d < device_num; ++d) {
    bool ret = m_cascade_thread_v[d]->sync();
    if (!ret) {
      return ret;
    }
  }
  return true;
}

bool Bmruntime::memcpy_d2s_parallel(void *datas[],
                                    bm_tensor_t tensors[],
                                    int tensor_num[],
                                    int device_num) {
  if (m_cascade_thread_v.size() == 0 && device_num == 1) {
    auto status = bm_memcpy_d2s(m_handles[0], datas[0], tensors[0].device_mem);
    return BM_SUCCESS == status;
  }

  if (m_cascade_thread_v.size() < device_num) {
    BMRT_LOG(WRONG, "It doesn't support d2s parallel because device_num %d is larger than cascade_net thread_num %d.",
             device_num, m_cascade_thread_v.size());
  }
  int offset = 0;
  for (int d = 0; d < device_num; ++d) {
    m_cascade_thread_v[d]->d2s(datas+offset, tensors+offset, tensor_num[d]);
    offset += tensor_num[d];
  }

  for (int d = 0; d < device_num; ++d) {
    bool ret = m_cascade_thread_v[d]->sync();
    if (!ret) {
      return ret;
    }
  }

  return true;
}

bool Bmruntime::memcpy_d2d_byte_parallel(bm_tensor_t dst_tensors[],
                                         size_t dst_offsets[],
                                         bm_tensor_t src_tensors[],
                                         size_t src_offsets[],
                                         size_t sizes[],
                                         int tensor_num[],
                                         int device_num) {
  if (m_cascade_thread_v.size() == 0 && device_num == 1) {
    auto status = bm_memcpy_d2d_byte(m_handles[0], dst_tensors[0].device_mem, dst_offsets[0],
                                    src_tensors[0].device_mem, src_offsets[0], sizes[0]);
    return BM_SUCCESS == status;
  }

  if (m_cascade_thread_v.size() < device_num) {
    BMRT_LOG(WRONG, "It doesn't support d2d parallel because device_num %d is larger than cascade_net thread_num %d.",
             device_num, m_cascade_thread_v.size());
  }

  int offset = 0;
  for (int d = 0; d < device_num; ++d) {
    m_cascade_thread_v[d]->d2d(dst_tensors + offset, dst_offsets + offset,
                              src_tensors + offset, src_offsets + offset,
                              sizes + offset, tensor_num[d]);
    offset += tensor_num[d];
  }

  for (int d = 0; d < device_num; ++d) {
    bool ret = m_cascade_thread_v[d]->sync();
    if (!ret) {
      return ret;
    }
  }

  return true;
}

bool Bmruntime::memcpy_d2d_stride_ex_parallel(bm_tensor_t dst_tensors[],
                                              size_t dst_offsets[],
                                              bm_shape_t dst_strides[],
                                              bm_tensor_t src_tensors[],
                                              size_t src_offsets[],
                                              bm_shape_t src_strides[],
                                              bm_shape_t shapes[],
                                              int tensor_num[],
                                              int device_num) {
  BMRT_ASSERT(bmrt_arch_info::get_bmtpu_arch() == BM1684X);
  std::vector<std::vector<tpu_kernel_global_move_1684x_t>> params(device_num);
  int processed_num = 0;
  for (int i = 0; i < device_num; ++i) {
    params[i].resize(tensor_num[i]);
    auto dst_tens = dst_tensors + processed_num;
    auto dst_offs = dst_offsets + processed_num;
    auto dst_strs = dst_strides + processed_num;
    auto src_tens = src_tensors + processed_num;
    auto src_offs = src_offsets + processed_num;
    auto src_strs = src_strides + processed_num;
    auto p_shape = shapes + processed_num;
    for (int j = 0; j < tensor_num[i]; ++j) {
      params[i][j].num_dims = p_shape[j].num_dims;
      if (params[i][j].num_dims > 4) {
        BMRT_LOG(WRONG, "Only support shape/stride num_dims <= 4, but num_dims passed is %d", params[i][j].num_dims);
      }
      for (int k = 0; k < p_shape[j].num_dims; ++k) {
        params[i][j].dst_stride[k] = dst_strs[j].dims[k];
        params[i][j].src_stride[k] = src_strs[j].dims[k];
        params[i][j].shape[k] = p_shape[j].dims[k];
      }
      params[i][j].dst_global_addr = bm_mem_get_device_addr(dst_tens[j].device_mem) + dst_offs[j];
      params[i][j].src_global_addr = bm_mem_get_device_addr(src_tens[j].device_mem) + src_offs[j];
      params[i][j].type_size = bmrt_data_type_size(src_tens[j].dtype);
      int dst_type_size = bmrt_data_type_size(dst_tens[j].dtype);
      if (params[i][j].type_size != dst_type_size) {
        BMRT_LOG(WRONG, "dst_type_size should be the same as src_type_size", dst_type_size, params[i][j].type_size);
      }
    }
    processed_num += tensor_num[i];
  }

  // 1 device
  if (m_cascade_thread_v.size() == 0 && device_num == 1) {
    auto ret = cascade_thread_global_move_data(0, m_handles[0], &params[0]);
    return ret;
  }

  if (m_cascade_thread_v.size() < device_num) {
    BMRT_LOG(WRONG, "It doesn't support d2d_stride_ex parallel because device_num %d is larger than cascade_net thread_num %d.",
             device_num, m_cascade_thread_v.size());
  }

  // multi devices
  for (int d = 0; d < device_num; ++d) {
    m_cascade_thread_v[d]->d2d_stride_ex(d, &params[d]);
  }

  for (int d = 0; d < device_num; ++d) {
    bool ret = m_cascade_thread_v[d]->sync();
    if (!ret) {
      return ret;
    }
  }
  return true;
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

bool Bmruntime::launch_multi_cores(int net_idx, void* const input_datas[], const bm_shape_t input_shapes[],
                       int input_num, void* output_datas[], bm_shape_t output_shapes[], int output_num,
                       uint64_t thread_idx, bool user_mem, const std::vector<int>& core_list, bool using_thread)
{
  auto net_ctx = m_net_ctx_v[net_idx];
  auto devid = net_ctx->device_id;
  // check parameters
  if (false == check_launch_params(net_ctx, input_datas, input_shapes, input_num, output_datas,
                                   output_shapes, output_num, user_mem)) {
    return false;
  }
  // prepare input and output tensors
  std::vector<bm_tensor_t> input_tensors(input_num);
  std::vector<bm_tensor_t> output_tensors(output_num);

  for (int i = 0; i < input_num; i++) {
    bmrt_tensor(&input_tensors[i], this, net_ctx->input_type_v[i], input_shapes[i]);
    bm_memcpy_s2d(m_handles[devid], input_tensors[i].device_mem, (void*)input_datas[i]);
  }

  // launch may not call sync internally
  bool ret = launch_multi_cores(net_idx, input_tensors.data(), input_num, output_tensors.data(), output_num, thread_idx, core_list, false, false, using_thread);

  // sync is needed for the following d2s to fetch output data
  if (ret){
    ret = (BM_SUCCESS == bm_thread_sync(m_handles[devid]));
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
      bm_memcpy_d2s_partial(m_handles[devid], output_datas[i], output_tensors[i].device_mem,
                            bmrt_tensor_bytesize(&output_tensors[i]));
      must_free_device_mem(devid, output_tensors[i].device_mem);
      output_shapes[i] = output_tensors[i].shape;
    }
  }
  for (int i = 0; i < input_num; i++) {
    must_free_device_mem(devid, input_tensors[i].device_mem);
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
    "BFLOAT16",
    "INT4",
    "UINT4"
  };
  BMRT_LOG(INFO, " ########################");
  // string version = netinfo;
  if (index < 0) {
    BMRT_LOG(INFO, " NetName: %s, Cascade", netinfo->name);
  } else {
    BMRT_LOG(INFO, " NetName: %s, Index=%d, CoreNum=%d", netinfo->name, index, netinfo->core_num);
  }
  for(int s=0; s<netinfo->stage_num; s++){
    BMRT_LOG(INFO, " ---- stage %d ----", s);
    for(int i=0; i<netinfo->input_num; i++){
      auto shapeStr = shape_to_str(netinfo->stages[s].input_shapes[i]);
      BMRT_LOG(INFO, "   Input %d) '%s' shape=%s dtype=%s scale=%g zero_point=%d device_id=%d",
          i,
          netinfo->input_names[i],
          shapeStr.c_str(),
          dtypeMap[netinfo->input_dtypes[i]],
          netinfo->input_scales[i],
          netinfo->input_zero_point[i],
          netinfo->input_loc_devices[i]);
    }
    for(int i=0; i<netinfo->output_num; i++){
      auto shapeStr = shape_to_str(netinfo->stages[s].output_shapes[i]);
      BMRT_LOG(INFO, "   Output %d) '%s' shape=%s dtype=%s scale=%g zero_point=%d device_id=%d",
          i,
          netinfo->output_names[i],
          shapeStr.c_str(),
          dtypeMap[netinfo->output_dtypes[i]],
          netinfo->output_scales[i],
          netinfo->output_zero_point[i],
          netinfo->output_loc_devices[i]);
    }
  }
  BMRT_LOG(INFO, " ########################");
}

void Bmruntime::show_neuron_network()
{
  int size = m_net_ctx_v.size();
  for (int idx = 0; idx < size; idx++) {
    if (!m_net_ctx_v[idx]->in_cascade) {
      auto net_info = get_net_info(idx);
      show_net_info(net_info, idx);
    }
  }
  for (auto &v : m_net_cascade_v) {
    show_net_info(&v.net_info, -1);
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

const net_cascade_t *Bmruntime::get_net_cascade(const string &net_name) {
  for (auto &v : m_net_cascade_v) {
    if (v.main_name == net_name) {
      return &v;
    }
  }
  return nullptr;
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
        for (auto net_ctx : m_net_ctx_v) {
          if (!net_ctx->in_cascade) {
            names->push_back(net_ctx->net_name.c_str());
          }
        }
        for (auto &net_cascade : m_net_cascade_v) {
          names->push_back(net_cascade.main_name.c_str());
        }
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

int Bmruntime::get_stage_idx(const char* net_name, const bm_tensor_t* input_tensors)
{
  int net_idx = get_net_idx(net_name);
  return get_stage_idx(m_net_ctx_v[net_idx], input_tensors);
}

int Bmruntime::get_stage_idx(const net_ctx_t* net_ctx, const bm_tensor_t* input_tensors)
{
  return net_ctx->is_dynamic ? get_dynamic_stage_idx(net_ctx, input_tensors)
                             :          get_static_stage_idx(net_ctx, input_tensors);
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
  /* if only one stage, count number is same also ok, these would make api
   * stronger adaptability*/
  if (net_ctx->stage_v.size() == 1) {
    auto &input_v = net_ctx->stage_v[0]->input_v;
    u32 input_idx;
    for (input_idx = 0; input_idx < input_v.size(); input_idx++) {
      if (bmrt_shape_count(&input_tensors[input_idx].shape) !=
          bmrt_shape_count(&input_v[input_idx].shape)) {
        break;
      }
    }
    if (input_idx == input_v.size()) {
      return 0;
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

const bm_net_info_t*  Bmruntime::get_net_info(const string& net_name)
{
  for (auto &v:m_net_cascade_v) {
    if (v.main_name == net_name) {
      return &v.net_info;
    }
  }
  for (auto v:m_net_ctx_v) {
    if (v->net_name == net_name) {
      return &v->net_info;
    }
  }
  return NULL;
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

const std::vector<bm_device_mem_u64_t> &Bmruntime::get_neuron_mem(int net_idx)
{
  return m_net_ctx_v[net_idx]->neuron_mem;
}

void Bmruntime::set_debug_mode(int mode)
{
  for (int i = 0; i < m_device_num; i++) {
    bm_set_debug_mode(m_handles[i] , mode);
  }
}

u64 Bmruntime::must_alloc_device_mem(uint32_t devid, bm_device_mem_t *mem, u64 size, const std::string &desc, int type_len)
{
  if (size == 0) {
    *mem = bm_mem_from_device(CTX_START_ADDR, 0);
  } else {
    bm_status_t status = bm_malloc_device_byte_heap_mask(m_handles[devid], mem, m_neuron_heap_mask, size*type_len);
    if (BM_SUCCESS != status) {
      BMRT_LOG(FATAL, "device mem alloc failed: size=%llu[0x%x] type_len=%d status=%d desc=%s",
              size, size, type_len, status, desc.c_str());
    }
  }
  BMRT_LOG_RUN(DEBUG, {
    // float nan = std::nanf("");
    // if (bm_memset_device_ext(m_handles[devid], &nan, 1, *mem) != BM_SUCCESS) {
    //   BMRT_LOG(FATAL, "bm_memset_device_ext failed");
    // }
    u64 mem_addr = bm_mem_get_device_addr(*mem);
    u64 mem_size = bm_mem_get_device_size(*mem);
    BMRT_LOG(DEBUG, "alloc mem devid=%d: %s [0x%llx, 0x%llx), size=%lld[0x%x]", devid, desc.c_str(), mem_addr, mem_addr+mem_size, mem_size, mem_size);
  });

  // Setting all neuron to nan can be useful when debugging net inference
  mem_pair_t mem_pair = { bm_mem_get_device_addr(*mem), bm_mem_get_device_size(*mem)};
  m_profile->record_alloc_device_mem(mem_pair, desc);
  return mem_pair.first;
}

u64 Bmruntime::must_alloc_device_mem_u64(uint32_t devid, bm_device_mem_u64_t *mem, u64 size, const std::string &desc, int type_len)
{
  if (size == 0) {
    *mem = bm_mem_from_device_u64(CTX_START_ADDR, 0);
  } else {
    bm_status_t status = bm_malloc_device_byte_heap_mask_u64(m_handles[devid], mem, m_neuron_heap_mask, size*type_len);
    if (BM_SUCCESS != status) {
      BMRT_LOG(FATAL, "device mem alloc failed: size=%llu[0x%x] type_len=%d status=%d desc=%s",
              size, size, type_len, status, desc.c_str());
    }
  }

  // Setting all neuron to nan can be useful when debugging net inference
  mem_pair_t mem_pair = { bm_mem_get_device_addr_u64(*mem), bm_mem_get_device_size_u64(*mem)};
  m_profile->record_alloc_device_mem(mem_pair, desc);
  BMRT_LOG_RUN(DEBUG, {
    u64 mem_addr = bm_mem_get_device_addr_u64(*mem);
    u64 mem_size = bm_mem_get_device_size_u64(*mem);
    BMRT_LOG(DEBUG, "alloc mem devid=%d: %s [0x%llx, 0x%llx), size=%lld[0x%x]", devid, desc.c_str(), mem_addr, mem_addr+mem_size, mem_size, mem_size);
  });

  return mem_pair.first;
}

bm_device_mem_t Bmruntime::must_alloc_device_mem(uint32_t devid, u64 size, const std::string &desc, int type_len){
  bm_device_mem_t mem;
  must_alloc_device_mem(devid, &mem, size, desc, type_len);
  return mem;
}

bm_device_mem_u64_t Bmruntime::must_alloc_device_mem_u64(uint32_t devid, u64 size, const string& desc, int type_len) {
  bm_device_mem_u64_t mem;
  must_alloc_device_mem_u64(devid, &mem, size, desc, type_len);
  return mem;
}

void Bmruntime::must_free_device_mem(uint32_t devid, bm_device_mem_t& mem){
  if (bm_mem_get_device_size(mem) == 0) {
      return;
  }
  BMRT_LOG_RUN(DEBUG, {
    u64 mem_addr = bm_mem_get_device_addr(mem);
    u64 mem_size = bm_mem_get_device_size(mem);
    BMRT_LOG(DEBUG, "free mem devid=%d: [0x%llx, 0x%llx), size=%lld[0x%x]", devid, mem_addr, mem_addr+mem_size, mem_size, mem_size);
  });
  bm_free_device(m_handles[devid], mem);
  m_profile->record_free_device_mem(bm_mem_get_device_addr(mem));
}

void Bmruntime::must_free_device_mem_u64(uint32_t devid, bm_device_mem_u64_t& mem){
  if (bm_mem_get_device_size_u64(mem) == 0) {
      return;
  }
  BMRT_LOG_RUN(DEBUG, {
    u64 mem_addr = bm_mem_get_device_addr_u64(mem);
    u64 mem_size = bm_mem_get_device_size_u64(mem);
    BMRT_LOG(DEBUG, "free mem devid=%d: [0x%llx, 0x%llx), size=%lld[0x%x]", devid, mem_addr, mem_addr+mem_size, mem_size, mem_size);
  });
  bm_free_device_u64(m_handles[devid], mem);
  m_profile->record_free_device_mem(bm_mem_get_device_addr_u64(mem));
}

u64 Bmruntime::alloc_device_mem(uint32_t devid, bm_device_mem_t &mem, u64 size, const std::string &desc, int type_len, bool auto_free_mem) {
  uint64_t device_addr;
  if (alloc_mem) {
    device_addr = must_alloc_device_mem(devid, &mem, size, desc, type_len);
    if (auto_free_mem) {
      m_device_mem_vec.push_back(mem);
      m_device_mem_ids.push_back(devid);
    }
  } else {
    size *= type_len;
    auto iter = find(dmem_info.begin(), dmem_info.end(), desc);
    if (iter != dmem_info.end()) {
      device_addr = iter->addr + iter->offset;
      BMRT_ASSERT_INFO((iter->offset + size <= iter->size), "Error: device memory: %s overflow, alloc failed. size=%llu[0x%x]", desc.c_str(), size, size);
      if (auto_free_mem)
        iter->offset += size;
      bm_set_device_mem(&mem, size, device_addr);
      BMRT_LOG(DEBUG, "alloc mem %s [0x%llx, 0x%llx), size=%lld[0x%x]", desc.c_str(), device_addr, device_addr+size, size, size);
    } else {
      BMRT_LOG(FATAL, "Error: device memory: %s don't alloc", desc.c_str());
    }
  }
  return device_addr;
}

void Bmruntime::reset_device_mem(const std::string &desc) {
  auto iter = find(dmem_info.begin(), dmem_info.end(), desc);
  if (iter != dmem_info.end()) {
    iter->offset = 0;
  }
}

void Bmruntime::clear_device_mem(const std::string &desc) {
  auto iter = find(dmem_info.begin(), dmem_info.end(), desc);
  if (iter != dmem_info.end()) {
    dmem_info.erase(iter);
  }
}

u64 Bmruntime::alloc_device_mem_u64(uint32_t devid, bm_device_mem_u64_t &mem, u64 size, const std::string &desc, int type_len, bool auto_free_mem) {
  uint64_t device_addr;
  if (alloc_mem) {
    device_addr = must_alloc_device_mem_u64(devid, &mem, size, desc, type_len);
    if (auto_free_mem) {
      m_sg_device_mem_vec.push_back(mem);
      m_sg_device_mem_ids.push_back(devid);
    }
  } else {
    size *= type_len;
    auto iter = find(dmem_info.begin(), dmem_info.end(), desc);
    if (iter != dmem_info.end()) {
      device_addr = iter->addr + iter->offset;
      BMRT_ASSERT_INFO((iter->offset + size <= iter->size), "Error: device memory: %s overflow, alloc failed. size=%llu[0x%x]", desc.c_str(), size, size);
      if (auto_free_mem)
        iter->offset += size;
      bm_set_device_mem_u64(&mem, size, device_addr);
      BMRT_LOG(DEBUG, "alloc mem %s [0x%llx, 0x%llx), size=%lld[0x%x]", desc.c_str(), device_addr, device_addr+size, size, size);
    } else {
      BMRT_LOG(FATAL, "Error: device memory: %s don't alloc", desc.c_str());
    }
  }
  return device_addr;
}

bm_device_mem_t Bmruntime::alloc_device_mem(uint32_t devid, u64 size, const std::string &desc, int type_len, bool auto_free_mem){
  bm_device_mem_t mem;
  alloc_device_mem(devid, mem, size, desc, type_len, auto_free_mem);
  return mem;
}

bm_device_mem_u64_t Bmruntime::alloc_device_mem_u64(uint32_t devid, u64 size, const std::string &desc, int type_len, bool auto_free_mem){
  bm_device_mem_u64_t mem;
  alloc_device_mem_u64(devid, mem, size, desc, type_len, auto_free_mem);
  return mem;
}

}  // namespace bmruntime
