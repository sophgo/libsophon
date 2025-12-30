#pragma once

#include "bmlib_runtime.h"
#include "bmruntime_common.h"
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace bmruntime {

class MemoryContext {
public:
  MemoryContext() = delete;
  MemoryContext(const MemoryContext &) = delete;
  MemoryContext(bm_handle_t handle, uint64_t size) : m_handle(handle) {
    BMRT_ASSERT(bm_malloc_device_byte_u64(handle, &m_mem, size) == BM_SUCCESS);
    BMRT_LOG(DEBUG, "Context alloc mem addr: 0x%lx, size: 0x%lx\n",
             m_mem.u.device.device_addr, size);
  }
  ~MemoryContext() { Free(); }

  bm_device_mem_u64_t Mem() { return m_mem; }
  uint64_t Size() const { return m_mem.size; }
  uint64_t Addr() const { return m_mem.u.device.device_addr; }
  void Resize(bm_handle_t handle, uint64_t size) {
    BMRT_LOG(DEBUG, "Context resize from 0x%lx to 0x%lx\n", m_mem.size, size);
    bm_free_device_u64(handle, m_mem);
    BMRT_ASSERT(bm_malloc_device_byte_u64(handle, &m_mem, size) == BM_SUCCESS);
  }

  void Free() {
    BMRT_LOG(DEBUG, "Context free mem addr: 0x%lx, size: 0x%lx\n", Addr(),
             Size());
    bm_free_device_u64(m_handle, m_mem);
  }

private:
  bm_handle_t m_handle;
  bm_device_mem_u64_t m_mem;
};

class ContextManager {
public:
  ContextManager() = default;
  ~ContextManager() = default;

  static std::shared_ptr<ContextManager> Instance() {
    static std::once_flag flag;
    std::call_once(flag,
                   [&]() { m_instance = std::make_shared<ContextManager>(); });
    return m_instance;
  }

  std::shared_ptr<MemoryContext> CreateContext(bm_handle_t handle,
                                               const std::string &net_name,
                                               uint64_t mem_size,
                                               uint32_t core_mask);
  std::shared_ptr<MemoryContext>
  AddContext(std::shared_ptr<MemoryContext> context,
             const std::string &net_name, uint64_t mem_size,
             uint32_t core_mask);
  std::shared_ptr<MemoryContext> GetContext(bm_handle_t handle,
                                            const std::string &net_name,
                                            uint64_t mem_size,
                                            uint32_t core_mask);
  void DestroyContext(bm_handle_t handle, const std::string &net_name,
                      uint64_t mem_size);

private:
  static std::shared_ptr<ContextManager> m_instance;
  std::mutex m_mtx;

  // {net_name, context}
  using ContextMap =
      std::unordered_map<std::string, std::shared_ptr<MemoryContext>>;
  // {core_mask, {net_name, context}}
  // core_mask = core_mask | (1 << core_id) for core_id in range(core_num)
  std::unordered_map<uint32_t, ContextMap> m_context;
  // {size, net_names}
  using ContextUsers =
      std::map<uint64_t, std::list<std::string>, std::greater<uint64_t>>;
  std::unordered_map<uint32_t, ContextUsers> m_users;

  void Sync(bm_handle_t handle, uint32_t core_mask) {
    int core = 0;
    while (core_mask) {
      if (core_mask & 0x1) {
        bm_thread_sync_from_core(handle, core);
      }
      core++;
      core_mask >>= 1;
    }
  }
};
} // namespace bmruntime