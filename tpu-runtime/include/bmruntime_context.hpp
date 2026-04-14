#pragma once

#include "bmlib_runtime.h"
#include "bmruntime_common.h"
#include "bmruntime_coeff.hpp"
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <array>

namespace bmruntime {

class DevMem;
using DevMemPtr = std::shared_ptr<DevMem>;
class DevMem {
public:
  static DevMemPtr CreateAutoManaged(bm_handle_t handle, uint64_t size,
                                     bool reuse = false) {
    return std::make_shared<DevMem>(handle, size, reuse, true);
  }
  static DevMemPtr CreateUserManaged(uint64_t addr, uint64_t size) {
    return std::make_shared<DevMem>(addr, size, false);
  }

  DevMem(bm_handle_t handle, uint64_t size, bool reuse, bool auto_release)
      : m_handle(handle), m_reuse(reuse), m_mem{}, m_auto_release(auto_release) {
    if (size > 0) {
      BMRT_ASSERT(bm_malloc_device_byte_u64(handle, &m_mem, size) == BM_SUCCESS);
      BMRT_LOG(DEBUG, "Alloc mem addr: 0x%lx, size: 0x%lx",
               m_mem.u.device.device_addr, size);
    } else {
      BMRT_LOG(DEBUG, "Alloc mem: size is 0, skipping allocation\n");
      m_mem = bm_mem_from_device_u64(0, 0);
    }
  }
  DevMem(uint64_t addr, uint64_t size, bool auto_release)
      : m_handle(nullptr), m_reuse(true), m_auto_release(auto_release) {
    m_mem = bm_mem_from_device_u64(addr, size);
  }
  DevMem() = delete;
  DevMem(const DevMem &) = delete;
  ~DevMem() {
    if (m_auto_release) {
      CleanUp();
    }
  }

  const bm_device_mem_u64_t &Mem() { return m_mem; }
  uint64_t Size() const { return m_mem.size; }
  uint64_t Addr() const { return m_mem.u.device.device_addr; }
  void Resize(uint64_t size) {
    if (m_mem.size > 0) {
      BMRT_LOG(DEBUG, "Resize from 0x%lx to 0x%lx", m_mem.size, size);
      bm_free_device_u64(m_handle, m_mem);
    }
    BMRT_ASSERT(bm_malloc_device_byte_u64(m_handle, &m_mem, size) == BM_SUCCESS);
  }
  bool Reuse() const { return m_reuse; }

private:
  void CleanUp() {
    if (Size() == 0) {
      BMRT_LOG(DEBUG, "skipping free for zero-size memory at addr=0x%llx",
               bm_mem_get_device_addr_u64(m_mem));
      return;
    }
    BMRT_LOG(DEBUG, "Free mem addr: 0x%lx, size: 0x%lx", Addr(), Size());
    bm_free_device_u64(m_handle, m_mem);
  }

  bm_handle_t m_handle{nullptr};
  bool m_reuse{false};
  bm_device_mem_u64_t m_mem{};
  bool m_auto_release{true};
};

class ComputeMemory {
public:
  ComputeMemory(bm_handle_t handle) : m_handle(handle) {}
  ~ComputeMemory() = default;
  DevMemPtr Create(const std::string &net_name,
                   uint64_t size, uint32_t core_mask, bool reuse);
  DevMemPtr Add(DevMemPtr context, const std::string &net_name, uint64_t size,
                uint32_t core_mask);
  DevMemPtr Get(const std::string &net_name, uint64_t size,
                uint32_t core_mask, bool reuse);
  void Destroy(const std::string &net_name, uint64_t size);

  void RegisterCustomer(uint64_t addr, uint64_t size, const std::string &name);

private:
  bm_handle_t m_handle;
  std::mutex m_mtx;

  // {net_name, context}
  using MemMap = std::unordered_map<std::string, DevMemPtr>;
  // {core_mask, {net_name, context}}
  // core_mask = core_mask | (1 << core_id) for core_id in range(core_num)
  std::unordered_map<uint32_t, MemMap> m_memory;
  // {size, net_names}
  using MemUsers =
      std::map<uint64_t, std::list<std::string>, std::greater<uint64_t>>;
  std::unordered_map<uint32_t, MemUsers> m_users;

  void Sync(uint32_t core_mask) {
    int core = 0;
    while (core_mask && core_mask != -1) {
      if (core_mask & 0x1) {
        bm_thread_sync_from_core(m_handle, core);
      }
      core++;
      core_mask >>= 1;
    }
  }

  // the memory malloced by customers
  std::unordered_map<std::string, DevMemPtr> m_customer;
};

class MemoryManager {
public:
  MemoryManager(int dev_id) {
    bm_dev_request(&m_handle, dev_id);
    m_compute_mem = std::make_shared<ComputeMemory>(m_handle);
    m_coeff_mem = std::make_shared<CoeffMemory>(m_handle);
  }
  ~MemoryManager() { bm_dev_free(m_handle); }

  static std::shared_ptr<MemoryManager> Instance(int dev_id) {
    static InstanceArray instances{};
    static std::array<std::once_flag, MAX_DEVICES> flags{};
    std::call_once(flags[dev_id], [&]() {
      instances[dev_id] = std::make_shared<MemoryManager>(dev_id);
    });
    return instances[dev_id];
  }

  std::shared_ptr<ComputeMemory> computeMemory() { return m_compute_mem; }

  const std::shared_ptr<CoeffMemory> coeffMemory() { return m_coeff_mem; }

private:
  bm_handle_t m_handle;
  // compute memory or neuron memory
  std::shared_ptr<ComputeMemory> m_compute_mem;

  // coeff memory
  std::shared_ptr<CoeffMemory> m_coeff_mem;

  static constexpr size_t MAX_DEVICES = MAX_DEVICE_NUM;
  using InstanceArray = std::array<std::shared_ptr<MemoryManager>, MAX_DEVICES>;
};
} // namespace bmruntime
