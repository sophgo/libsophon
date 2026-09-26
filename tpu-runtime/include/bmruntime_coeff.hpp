#pragma once

#include "backend/backend.hpp"
#include "bmodel.hpp"
#include <mutex>
#include <unordered_map>

using bmodel::CoeffMem;
using bmodel::ModelCtx;

namespace bmruntime {

class DevMem;
using DevMemPtr = std::shared_ptr<DevMem>;

class CoeffMemory {
public:
  explicit CoeffMemory(bm_handle_t handle) : m_handle(handle) {
    memset(&m_latest_device_mem, 0, sizeof(m_latest_device_mem));
  }
  ~CoeffMemory() = default;

  uint64_t Register(ModelCtx *model_ctx, const CoeffMem *coeff_mem,
                    const std::string &net_name, addr_t addr_traits);
  void Destroy(const std::string &net_name);
  void RegisterCustomer(uint64_t addr, uint64_t size, const std::string &name);

  uint64_t Update(ModelCtx *model_ctx, const CoeffMem *coeff_mem, int mem_idx,
                  const std::vector<int> &weight_idx,
                  const std::vector<uint8_t> &file_data,
                  long long &start_position);
  int Check();
  bm_device_mem_u64_t GetCoeffDeviceMem() { return m_latest_device_mem; }

private:
  bm_handle_t m_handle;
  bm_device_mem_u64_t m_latest_device_mem;
  std::mutex m_mtx;
  std::vector<bm_device_mem_u64_t> m_memory_pool;

  using hash_t = std::vector<uint8_t>;
  struct MemoryRef {
    DevMemPtr mem;
    size_t ref_count;
    MemoryRef(DevMemPtr &mem) : mem(mem), ref_count(1) {}
  };
  std::map<hash_t, std::unique_ptr<MemoryRef>> m_hash_table;
  std::unordered_map<std::string, std::set<hash_t>> m_user;

  // the memory malloced by customers
  struct MemoryBlock {
    DevMemPtr mem;
    uint64_t offset;
    MemoryBlock(DevMemPtr &mem) : mem(mem), offset(0) {}
  };
  std::unordered_map<std::string, std::unique_ptr<MemoryBlock>> m_customer;
};
} // namespace bmruntime
