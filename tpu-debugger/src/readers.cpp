#include "readers.hpp"
#include "debugger.hpp"
#include <iostream>

namespace tpu_debugger {

// ============== DevMemReader ==============

DevMemReader::DevMemReader() : devmem_(std::make_unique<DevMem>()) {
    if (!devmem_->open()) {
        std::cerr << "Failed to open /dev/mem" << std::endl;
        devmem_.reset();
    }
}

DevMemReader::~DevMemReader() = default;

DevMemReader::DevMemReader(DevMemReader&&) noexcept = default;
DevMemReader& DevMemReader::operator=(DevMemReader&&) noexcept = default;

bool DevMemReader::ensureMapped(uint64_t addr, size_t size) {
    if (!devmem_) return false;

    // 检查当前映射是否覆盖所需范围
    if (addr >= current_base_addr_ &&
        addr + size <= current_base_addr_ + current_map_size_) {
        return true;
    }

    // 重新映射
    // 使用更大的映射范围以减少重新映射的次数
    size_t map_size = std::max(size, static_cast<size_t>(0x10000));  // 至少64KB
    uint64_t map_addr = addr & ~0xFFF;  // 4KB对齐

    if (!devmem_->mapMemory(map_addr, map_size)) {
        return false;
    }

    current_base_addr_ = map_addr;
    current_map_size_ = map_size;
    return true;
}

std::vector<uint32_t> DevMemReader::read(uint64_t addr, uint32_t num_words) {
    std::vector<uint32_t> result;

    if (!devmem_) {
        std::cerr << "DevMem not initialized" << std::endl;
        return result;
    }

    size_t total_size = num_words * 4;
    if (!ensureMapped(addr, total_size)) {
        std::cerr << "Failed to map memory at 0x" << std::hex << addr << std::dec << std::endl;
        return result;
    }

    result.reserve(num_words);
    for (uint32_t i = 0; i < num_words; ++i) {
        try {
            uint32_t value = devmem_->read<uint32_t>(addr + i * 4);
            result.push_back(value);
        } catch (const std::exception& e) {
            std::cerr << "Error reading at 0x" << std::hex << (addr + i * 4)
                     << ": " << e.what() << std::dec << std::endl;
            result.push_back(0);
        }
    }

    return result;
}

// ============== BmlibReader ==============

// BmlibReader::BmlibReader(bm_handle_t handle) : handle_(handle) {}

// std::vector<uint32_t> BmlibReader::read(uint64_t addr, uint32_t num_words) {
//     (void)addr;  // 暂时未使用，避免警告
//     std::vector<uint32_t> result;
//     result.reserve(num_words);

//     if (!handle_) {
//         std::cerr << "BM handle not initialized" << std::endl;
//         return result;
//     }

//     // 使用bmlib的接口读取寄存器
//     // 注意：这里需要根据实际的bmlib API进行调整
//     for (uint32_t i = 0; i < num_words; ++i) {
//         uint32_t value = 0;
//         // bm_read32(handle_, addr + i * 4, &value);  // 假设的API
//         // 目前先用0填充，实际使用时需要替换为真实的bmlib调用
//         result.push_back(value);
//     }

//     return result;
// }

// ============== ReaderFactory ==============

std::shared_ptr<IRegisterReader> ReaderFactory::createReader(
    ReaderType type,
    const std::string& filepath,
    DebuggerConfig* config) {

    switch (type) {
        case ReaderType::Json:
            return createJsonReader(filepath, config);
        case ReaderType::SoC:
            return std::make_shared<DevMemReader>();
        case ReaderType::PCIe:
            std::cerr << "PCIe reader not implemented" << std::endl;
            return nullptr;
        default:
            std::cerr << "Unknown reader type" << std::endl;
            return nullptr;
    }
}

std::shared_ptr<JsonReader> ReaderFactory::createJsonReader(
    const std::string& filepath,
    DebuggerConfig* config) {

    if (filepath.empty()) {
        std::cerr << "File path is required for Json reader" << std::endl;
        return nullptr;
    }

    auto reader = std::make_shared<JsonReader>(filepath);

    if (!reader->isValid()) {
        std::cerr << "Failed to create JsonReader" << std::endl;
        return nullptr;
    }

    // 更新 DebuggerConfig
    if (config != nullptr) {
        config->chip_name = reader->getChipName();
        config->target_cores.clear();  // 使用 JSON 中的所有 cores
    }

    return reader;
}

} // namespace tpu_debugger
