#pragma once

#include "register.hpp"
#include "mmap.hpp"
#include "json_reader.hpp"
#include <memory>
#include <string>
#include <vector>

namespace tpu_debugger {

// 前向声明
struct DebuggerConfig;

enum class ReaderType {
    Json,   // 从JSON文件读取
    SoC,    // 从/dev/mem读取
    PCIe    // 从bmlib读取（暂未实现）
};

class DevMemReader : public IRegisterReader {
public:
    DevMemReader();
    ~DevMemReader();

    // 禁止拷贝
    DevMemReader(const DevMemReader&) = delete;
    DevMemReader& operator=(const DevMemReader&) = delete;

    // 允许移动
    DevMemReader(DevMemReader&&) noexcept;
    DevMemReader& operator=(DevMemReader&&) noexcept;

    // IRegisterReader接口实现
    std::vector<uint32_t> read(uint64_t addr, uint32_t num_words) override;
    bool isValid() const override { return devmem_ != nullptr && devmem_->isValid(); }

    // 获取底层DevMem（用于高级操作）
    DevMem* getDevMem() const { return devmem_.get(); }

private:
    std::unique_ptr<DevMem> devmem_;
    uint64_t current_base_addr_ = 0;
    size_t current_map_size_ = 0;

    // 确保内存映射覆盖指定地址范围
    bool ensureMapped(uint64_t addr, size_t size);
};

// bmlib读取器（用于PCIe模式）
// class BmlibReader : public IRegisterReader {
// public:
//     explicit BmlibReader(bm_handle_t handle);
//     ~BmlibReader() = default;

//     // IRegisterReader接口实现
//     std::vector<uint32_t> read(uint64_t addr, uint32_t num_words) override;
//     bool isValid() const override { return handle_ != nullptr; }

// private:
//     bm_handle_t handle_;
// };

// 读取器工厂
class ReaderFactory {
public:
    // 创建读取器（通过type和可选的filepath参数）
    // type = ReaderType::Json 时，filepath必须提供，config会被更新
    // type = ReaderType::SoC 时，filepath被忽略
    static std::shared_ptr<IRegisterReader> createReader(
        ReaderType type,
        const std::string& filepath = "",
        DebuggerConfig* config = nullptr);

    // 创建JSON读取器
    static std::shared_ptr<JsonReader> createJsonReader(
        const std::string& filepath,
        DebuggerConfig* config = nullptr);

    static std::shared_ptr<IRegisterReader> createSoCReader() {
        return createReader(ReaderType::SoC);
    }

    // 创建PCIe模式的读取器（使用bmlib）
    // static std::shared_ptr<IRegisterReader> createPCIeReader(bm_handle_t handle);

    // 自动检测并创建合适的读取器
    static std::shared_ptr<IRegisterReader> createAuto();
};

} // namespace tpu_debugger
