#pragma once

#include "register.hpp"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tpu_debugger {

// JSON 文件读取器 - 从 JSON 文件读取引擎指令数据
class JsonReader : public IRegisterReader {
public:
    // 引擎数据结构
    struct EngineData {
        std::string name;
        uint64_t base_addr;
        std::vector<uint32_t> data;
    };

    // Core 数据结构
    struct CoreData {
        uint32_t core_id;
        std::vector<EngineData> engines;
    };

    explicit JsonReader(const std::string& filepath);
    ~JsonReader() = default;

    // 禁止拷贝
    JsonReader(const JsonReader&) = delete;
    JsonReader& operator=(const JsonReader&) = delete;

    // IRegisterReader 接口实现
    std::vector<uint32_t> read(uint64_t addr, uint32_t num_words) override;
    bool isValid() const override { return valid_; }

    // 获取配置信息
    std::string getChipName() const { return chip_name_; }
    uint32_t getNumCores() const { return num_cores_; }
    std::vector<std::string> getEngineNames(uint32_t core_id) const;

    // 获取所有 cores 数据
    const std::vector<CoreData>& getCoresData() const { return cores_data_; }

    // 获取指定 core 和 engine 的数据
    std::optional<std::vector<uint32_t>> getEngineData(uint32_t core_id, const std::string& engine_name) const;

    // 检查是否存在指定 core 和 engine 的数据
    bool hasEngineData(uint32_t core_id, const std::string& engine_name) const;

private:
    std::vector<CoreData> cores_data_;
    std::string chip_name_;
    uint32_t num_cores_ = 0;
    bool valid_ = false;

    // 内部方法
    bool loadFromFile(const std::string& filepath);
    void parseHexData(const std::string& hex_str, std::vector<uint32_t>& data);
};

} // namespace tpu_debugger
