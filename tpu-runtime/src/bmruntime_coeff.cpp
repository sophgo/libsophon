#include "bmruntime_coeff.hpp"
#include "bmruntime_context.hpp"

namespace bmruntime {
static void upload_coeff_data(ModelCtx *model_ctx,
                              const bmodel::CoeffMem *coeff_mem,
                              bm_handle_t handle, DevMemPtr dev_mem) {
  bm_status_t status = BM_SUCCESS;
  u64 size = coeff_mem->binary_coeff()->size();
#ifdef SOC_MODE
  void *vmem = NULL;
  status = bm_mem_mmap_device_mem_u64(
      handle, const_cast<bm_device_mem_u64_t *>(&dev_mem->Mem()), (u64 *)&vmem);
  CHECK_status(status);
  model_ctx->read_binary(coeff_mem->binary_coeff(), (uint8_t *)vmem);
  status = bm_mem_flush_device_mem_u64(
      handle, const_cast<bm_device_mem_u64_t *>(&dev_mem->Mem()));
  CHECK_status(status);
  bm_mem_unmap_device_mem_u64(handle, vmem, size);
#else
  if (coeff_mem->encrypt_mode() == 0) {
#define COEFF_BLK_SIZE 0x1000000
    std::vector<uint8_t> data(COEFF_BLK_SIZE, 0);
    u64 left_size = size;
    u64 offset = 0;
    u64 address = dev_mem->Addr();
    while (left_size > 0) {
      u64 data_size =
          (left_size >= COEFF_BLK_SIZE ? COEFF_BLK_SIZE : left_size);
      model_ctx->read_binary(coeff_mem->binary_coeff(), offset, data.data(),
                             data_size);
      bm_device_mem_t pmem = bm_mem_from_device(address + offset, data_size);
      status = bm_memcpy_s2d(handle, pmem, ((void *)data.data()));
      CHECK_status(status);
      offset += data_size;
      left_size -= data_size;
    }
  } else if (coeff_mem->encrypt_mode() == 1) {
    // encrypted data
    uint64_t out_size = 0;
    auto decrypt_data = model_ctx->read_binary_with_decrypt(
        coeff_mem->binary_coeff(), &out_size);
    auto dev_size = dev_mem->Size();
    BMRT_ASSERT_INFO((out_size == dev_size),
                     "Error: device memory vs coeff size overflow");
    status = bm_memcpy_s2d_u64(handle, dev_mem->Mem(), ((void *)decrypt_data));
    free(decrypt_data);
    CHECK_status(status);
  }
#endif
}

static void update_coeff_data(ModelCtx *model_ctx,
                              const bmodel::CoeffMem *coeff_mem,
                              const bm_device_mem_u64_t &dev_mem,
                              const std::vector<int> &weight_idx,
                              const std::vector<uint8_t> &file_data,
                              long long &start_position, bm_handle_t handle) {
  bm_status_t status = BM_SUCCESS;
  auto location = coeff_mem->location();
  u64 address = bm_mem_get_device_addr_u64(dev_mem);
  auto dev_size = bm_mem_get_device_size_u64(dev_mem);

  for (int k = 0; k < location->size(); k++) {
    auto info = location->Get(k);
    auto loc_size = info->size();
    auto loc_offset = info->offset();
    std::vector<uint8_t> buffer(loc_size, 0);

    if (std::find(weight_idx.begin(), weight_idx.end(), k) !=
            weight_idx.end() ||
        weight_idx.size() == 0) {
      // file_data.size() != 0 -> update
      // file_data.size() == 0 -> empty
      if (file_data.size() != 0) {
        if (start_position + loc_size > file_data.size()) {
          // out_of_range only for lora update
          // if you want to throw exception, please use runtime_error,
          // logic_error
          throw std::out_of_range("File data does not contain enough data for "
                                  "the requested operation.");
        }
        buffer.assign(file_data.begin() + start_position,
                      file_data.begin() + start_position + loc_size);
        start_position += loc_size;
      }
      bm_device_mem_t pmem = bm_mem_from_device(address + loc_offset, loc_size);
      status = bm_memcpy_s2d(handle, pmem, ((void *)buffer.data()));
      CHECK_status(status);
    }
  }
}

uint64_t CoeffMemory::Register(ModelCtx *model_ctx, const CoeffMem *coeff_mem,
                               const std::string &net_name,
                               addr_t addr_traits) {
  if (coeff_mem == NULL || model_ctx == NULL) {
    return 0;
  }
  uint64_t coeff_start = coeff_mem->address();
  coeff_start &= addr_traits.offset.mask;
  uint64_t coeff_size = coeff_mem->encrypt_mode() == 0
                            ? coeff_mem->binary_coeff()->size()
                            : coeff_mem->decrypt_size();
  uint8_t *coeff_size_ptr = (uint8_t *)&coeff_size;

  hash_t hash_value = {coeff_mem->check_code()->begin(),
                       coeff_mem->check_code()->end()};
  hash_value.insert(hash_value.end(), coeff_size_ptr,
                    coeff_size_ptr + sizeof(uint64_t));

  std::lock_guard<std::mutex> guard(m_mtx);
  BMRT_LOG(DEBUG, "Malloc coeff mem for net: %s", net_name.c_str());
  m_user[net_name].insert(hash_value);
  auto iter = m_hash_table.find(hash_value);

  if (iter != m_hash_table.end()) {
    iter->second->ref_count++;
    auto &mem_ptr = iter->second->mem;
    BMRT_LOG(DEBUG, "Reuse coeff mem: [0x%lx, 0x%lx), size=0x%lx]",
             mem_ptr->Addr(), mem_ptr->Addr() + mem_ptr->Size(),
             mem_ptr->Size());
    return mem_ptr->Addr() - coeff_start;
  }

  DevMemPtr pmem;
  if (m_customer.find(net_name) != m_customer.end()) {
    auto &mem_block = m_customer[net_name];
    pmem = DevMem::CreateUserManaged(mem_block->mem->Addr() + mem_block->offset,
                                     coeff_size);
    mem_block->offset += coeff_size;
    BMRT_LOG(DEBUG,
             "Coeff use registered memory: [0x%lx, 0x%lx), size=0x%lx]",
             pmem->Addr(), pmem->Addr() + pmem->Size(), pmem->Size());
  } else {
    pmem = DevMem::CreateAutoManaged(m_handle, coeff_size);
    BMRT_LOG(DEBUG, "Malloc coeff mem : [0x%lx, 0x%lx), size=0x%lx",
             pmem->Addr(), pmem->Addr() + pmem->Size(), pmem->Size());
    m_memory_pool.push_back(pmem->Mem());
  }

  m_latest_device_mem = pmem->Mem();
  upload_coeff_data(model_ctx, coeff_mem, m_handle, pmem);

  m_hash_table[hash_value] = std::make_unique<MemoryRef>(pmem);
  return pmem->Addr() - coeff_start;
}

void CoeffMemory::Destroy(const std::string &net_name) {
  std::lock_guard<std::mutex> guard(m_mtx);
  BMRT_LOG(DEBUG, "Destroy coeff mem: %s", net_name.c_str());
  for (auto &hash_value : m_user[net_name]) {
    auto iter = m_hash_table.find(hash_value);
    if (iter != m_hash_table.end()) {
      iter->second->ref_count--;
      if (iter->second->ref_count == 0) {
        auto &mem_ptr = iter->second->mem;
        BMRT_LOG(DEBUG, "Free coeff mem: [0x%lx, 0x%lx), size=0x%lx]",
                 mem_ptr->Addr(), mem_ptr->Addr() + mem_ptr->Size(),
                 mem_ptr->Size());
        m_hash_table.erase(iter);
      }
    }
  }
}

void CoeffMemory::RegisterCustomer(uint64_t addr, uint64_t size,
                                   const std::string &name) {
  std::lock_guard<std::mutex> guard(m_mtx);
  auto mem = DevMem::CreateUserManaged(addr, size);
  m_customer[name] = std::make_unique<MemoryBlock>(mem);
  BMRT_LOG(DEBUG, "Register memory addr: 0x%lx, size: 0x%lx", addr, size);
}
uint64_t CoeffMemory::Update(ModelCtx *model_ctx, const CoeffMem *coeff_mem,
                         int mem_idx, const std::vector<int> &weight_idx,
                         const std::vector<uint8_t> &file_data,
                         long long &start_position) {
  if (coeff_mem == NULL || model_ctx == NULL) {
    return 0;
  }

  // check whether the same
  std::lock_guard<std::mutex> guard(m_mtx);

  auto dev_mem = m_memory_pool[mem_idx];
  update_coeff_data(model_ctx, coeff_mem, dev_mem, weight_idx, file_data,
                    start_position, m_handle);
  return 0;
}

int CoeffMemory::Check() {
  int devid = bm_get_devid(m_handle);
  int err_count = 0;
  uint8_t crc32[bmodel::SHA256_LEN];
  std::lock_guard<std::mutex> guard(m_mtx);
  for (auto &coeff : m_hash_table) {
    auto &sha = coeff.first;
    auto &mem_ref = coeff.second;
    uint64_t size = mem_ref->mem->Size();
    if (size > 0x40000000) {
      fprintf(stderr,
              "Coeff size[0x%lx] is greater than 1GB, ignore the SHA check\n",
              size);
      continue;
    }
    std::vector<uint8_t> buffer(size, 0);
    bm_status_t status =
        bm_memcpy_d2s_u64(m_handle, buffer.data(), mem_ref->mem->Mem());
    CHECK_status(status);
    bmodel::CalcSha256(buffer.data(), size, crc32);
    uint64_t addr = bm_mem_get_device_addr_u64(mem_ref->mem->Mem());
    fprintf(stderr,
            "Coeff, chip[%d], SHA[%02X%02X%02X%02X], addr[0x%lx], size[0x%x]",
            devid, sha[0], sha[1], sha[2], sha[3], addr, (u32)size);
    if (0 != memcmp(crc32, coeff.first.data(), bmodel::SHA256_LEN)) {
      fprintf(stderr, ", Check:**FAILED**\n");
      err_count++;
    } else {
      fprintf(stderr, "\n");
    }
  }
  return err_count;
}
} // namespace bmruntime