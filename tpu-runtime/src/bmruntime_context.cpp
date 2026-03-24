#include "bmruntime_context.hpp"

namespace bmruntime {

std::shared_ptr<ContextManager> ContextManager::m_instance = nullptr;
std::shared_ptr<MemoryContext>
ContextManager::CreateContext(bm_handle_t handle, const std::string &net_name,
                              uint64_t mem_size, uint32_t core_mask) {
  BMRT_ASSERT_INFO(core_mask != 0, "core_mask can't be zero");
  std::unique_lock<std::mutex> lock(m_mtx);

  // context is not exist with this core_mask
  if (m_context.count(core_mask) == 0) {
    m_context[core_mask][net_name] =
        std::make_shared<MemoryContext>(handle, mem_size);
    m_users[core_mask][mem_size] = {net_name};
    return m_context[core_mask][net_name];
  }

  // context is already created with this net_name
  if (m_context[core_mask].count(net_name)) {
    auto context = m_context[core_mask][net_name];
    BMRT_ASSERT_INFO(context->Size() == mem_size,
                     "context size is not equal to mem_size");
    return context;
  }

  // update context size
  auto context = m_context[core_mask].begin()->second;
  uint64_t max_size = m_users[core_mask].begin()->first;
  if (m_users[core_mask].count(mem_size) == 0) {
    m_users[core_mask][mem_size] = {net_name};
  } else {
    m_users[core_mask][mem_size].push_back(net_name);
  }

  if (mem_size > max_size) {
    // sync first and then resize, maybe some models are running with current mem.
    Sync(handle, core_mask);
    context->Resize(handle, mem_size);
  }
  m_context[core_mask][net_name] = context;
  return context;
}

std::shared_ptr<MemoryContext>
ContextManager::AddContext(std::shared_ptr<MemoryContext> context,
                           const std::string &net_name, uint64_t mem_size,
                           uint32_t core_mask) {
  std::unique_lock<std::mutex> lock(m_mtx);
  BMRT_ASSERT_INFO(m_context.count(core_mask), "core_mask is not exist");

  if (m_context[core_mask].count(net_name)) {
    auto context = m_context[core_mask][net_name];
    BMRT_ASSERT_INFO(context->Size() >= mem_size,
                     "context size should be bigger than mem_size");
    return context;
  }

  m_context[core_mask][net_name] = context;
  if (m_users[core_mask].count(mem_size) == 0) {
    m_users[core_mask][mem_size] = {net_name};
  } else {
    m_users[core_mask][mem_size].push_back(net_name);
  }
  return context;
}

std::shared_ptr<MemoryContext>
ContextManager::GetContext(bm_handle_t handle, const std::string &net_name,
                           uint64_t mem_size, uint32_t core_mask) {
  {
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_context.count(core_mask) && m_context[core_mask].count(net_name)) {
      return m_context[core_mask][net_name];
    }
  }

  return CreateContext(handle, net_name, mem_size, core_mask);
}

void ContextManager::DestroyContext(bm_handle_t handle,
                                    const std::string &net_name,
                                    uint64_t mem_size) {
  std::unique_lock<std::mutex> lock(m_mtx);

  std::list<uint32_t> core_masks;
  for (auto &iter : m_context) {
    if (iter.second.count(net_name)) {
      core_masks.push_back(iter.first);
    }
  }

  BMRT_LOG(DEBUG, "DestroyContext: %s, mem_size: %ld\n", net_name.c_str(),
           mem_size);
  for (auto &core_mask : core_masks) {
    // context is exist
    auto context = m_context[core_mask][net_name];
    m_context[core_mask].erase(net_name);
    m_users[core_mask][mem_size].remove(net_name);
    if (m_users[core_mask][mem_size].empty()) {
      m_users[core_mask].erase(mem_size);
    }
    if (m_context[core_mask].empty()) {
      m_context.erase(core_mask);
      m_users.erase(core_mask);
      continue;
    }

    // resize context
    if ((m_users[core_mask].begin())->first < mem_size) {
      Sync(handle, core_mask);
      context->Resize(handle, (m_users[core_mask].begin())->first);
    }
  }
}
} // namespace bmruntime