#include "bmruntime_context.hpp"

namespace bmruntime {

DevMemPtr ComputeMemory::Create(const std::string &net_name, uint64_t size,
                                uint32_t core_mask, bool reuse) {
  if (m_customer.count(net_name)) {
    return m_customer[net_name];
  }
  // core_mask = core_mask | (1 << core_id) for core_id in range(core_num)
  // -1 means pre alloced memory and can be used by any core
  BMRT_ASSERT_INFO(core_mask != 0, "core_mask can't be zero");
  std::unique_lock<std::mutex> lock(m_mtx);

  if (core_mask != -1 && m_memory.count(core_mask) == 0 && m_memory.count(-1)) {
    // assign context with core_mask=-1 to current core_mask
    m_memory[core_mask] = m_memory[-1];
    m_users[core_mask] = m_users[-1];
    m_memory.erase(-1);
    m_users.erase(-1);
  }

  // context is not exist with this core_mask
  if (m_memory.count(core_mask) == 0) {
    m_memory[core_mask][net_name] =
        DevMem::CreateAutoManaged(m_handle, size, reuse);
    m_users[core_mask][size] = {net_name};
    return m_memory[core_mask][net_name];
  }

  // context is already created with this net_name
  auto context = m_memory[core_mask].begin()->second;
  if (m_memory[core_mask].count(net_name)) {
    context = m_memory[core_mask][net_name];
    if (context->Size() >= size) {
      BMRT_LOG(DEBUG, "Reuse memory, net_name: %s, size: %ld", net_name.c_str(), size);
      return context;
    } else if (context->Reuse() && reuse) {
      BMRT_LOG(
          WARNING,
          "Memory is exist, net_name: %s, size: 0x%lx and the incoming size "
          "is 0x%lx, need to resize",
          net_name.c_str(), context->Size(), size);
    }
  } else if (reuse && !context->Reuse()) {
    // to find a reusable context in current core_mask, if exist, assign to current net_name
    for(auto &iter : m_memory[core_mask]) {
      if (iter.second->Reuse()) {
        context = iter.second;
        break;
      }
    }
  }

  // update context size
  if (m_users[core_mask].count(size) == 0) {
    m_users[core_mask][size] = {net_name};
  } else {
    m_users[core_mask][size].push_back(net_name);
  }

  if (!context->Reuse() || !reuse) {
    // instructions of DMA has been changed with previous memory and malloc
    // again, like BM1684/BM1684X
    context = DevMem::CreateAutoManaged(m_handle, size, reuse);
  } else if (size > context->Size()) {
    // sync first and then resize, maybe some models are running with current
    // mem.
    Sync(core_mask);
    context->Resize(size);
  }
  m_memory[core_mask][net_name] = context;
  return context;
}

DevMemPtr ComputeMemory::Add(DevMemPtr context, const std::string &net_name,
                             uint64_t size, uint32_t core_mask) {
  std::unique_lock<std::mutex> lock(m_mtx);
  BMRT_ASSERT_INFO(m_memory.count(core_mask), "core_mask is not exist");

  if (m_memory[core_mask].count(net_name)) {
    auto context = m_memory[core_mask][net_name];
    BMRT_ASSERT_INFO(context->Size() >= size,
                     "context size should be bigger than mem_size");
    return context;
  }

  m_memory[core_mask][net_name] = context;
  if (m_users[core_mask].count(size) == 0) {
    m_users[core_mask][size] = {net_name};
  } else {
    m_users[core_mask][size].push_back(net_name);
  }
  return context;
}

DevMemPtr ComputeMemory::Get(const std::string &net_name, uint64_t size,
                             uint32_t core_mask, bool reuse) {
  {
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_memory.count(core_mask) && m_memory[core_mask].count(net_name)) {
      return m_memory[core_mask][net_name];
    }
  }

  return Create(net_name, size, core_mask, reuse);
}

void ComputeMemory::Destroy(const std::string &net_name, uint64_t size) {
  std::unique_lock<std::mutex> lock(m_mtx);

  std::list<uint32_t> core_masks;
  for (auto &iter : m_memory) {
    if (iter.second.count(net_name)) {
      core_masks.push_back(iter.first);
    }
  }

  BMRT_LOG(DEBUG, "DestroyContext: %s, mem_size: 0x%lx", net_name.c_str(),
           size);
  for (auto &core_mask : core_masks) {
    // context is exist
    auto context = m_memory[core_mask][net_name];
    m_memory[core_mask].erase(net_name);
    m_users[core_mask][size].remove(net_name);
    if (m_users[core_mask][size].empty()) {
      m_users[core_mask].erase(size);
    }
    if (m_memory[core_mask].empty()) {
      m_memory.erase(core_mask);
      m_users.erase(core_mask);
      continue;
    }

    // resize context
    if ((m_users[core_mask].begin())->first < size && context.use_count() > 1) {
      BMRT_ASSERT(context->Reuse());
      Sync(core_mask);
      context->Resize((m_users[core_mask].begin())->first);
    }
  }
}

void ComputeMemory::RegisterCustomer(uint64_t addr, uint64_t size,
                                     const std::string &name) {
  auto mem = DevMem::CreateUserManaged(addr, size);
  m_customer[name] = mem;
  BMRT_LOG(DEBUG, "Register memory addr: 0x%lx, size: 0x%lx", addr, size);
}

} // namespace bmruntime