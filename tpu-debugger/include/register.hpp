#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <optional>
#include <functional>

namespace tpu_debugger {

using regPostProcessFunc = std::function<uint64_t(uint64_t)>;
// 寄存器字段定义，支持从Excel配置导入
struct RegField {
    std::string name;           // 字段名称
    uint32_t start_bit;         // 起始bit位（从0开始）
    uint32_t bit_width;         // bit宽度
    std::unordered_set<uint64_t> valid_values;  // 有效值集合（空表示接受任意值）
    std::unordered_set<uint64_t> invalid_values; // invalid values
    std::optional<uint64_t> expected_value;     // 期望值（用于异常检测）
    std::string description;    // 字段描述
    regPostProcessFunc post_process;    // post process functions

    // 构造函数
    RegField() = default;
    RegField(const std::string &n, uint32_t start, uint32_t width,
             const std::string &desc = "", regPostProcessFunc func = nullptr)
        : name(n), start_bit(start), bit_width(width), description(desc),
          post_process(func) {}

    // 添加有效值
    void addValidValue(uint64_t value) { valid_values.insert(value); }

    // Check valid
    bool isValid(uint64_t value) const {
        if (!invalid_values.empty() && invalid_values.count(value)) {
            return false;
        }
        if (valid_values.empty() || valid_values.count(value)) {
            return true;
        }
        return false;
    }

    // 检查是否与期望值匹配
    bool matchesExpected(uint64_t value) const {
        if (!expected_value.has_value()) return true;
        return value == expected_value.value();
    }
};

// 寄存器异常信息
struct RegAnomaly {
    std::string field_name;
    uint64_t actual_value;
    std::optional<uint64_t> expected_value;
    std::string reason;  // "invalid_value" 或 "mismatch_expected"

    std::string toString() const {
        std::string result = "Field '" + field_name + "': 0x" + std::to_string(actual_value);
        if (expected_value.has_value()) {
            result += " (expected: 0x" + std::to_string(expected_value.value()) + ")";
        }
        result += " - " + reason;
        return result;
    }
};

// 寄存器读取结果
struct RegReadResult {
    std::string register_name;
    std::vector<uint32_t> raw_data;  // RegDescriptor中的原始数据，每个元素32bit
    std::vector<std::pair<std::string, uint64_t>> field_values;
    std::vector<RegAnomaly> anomalies;
    std::vector<uint32_t>  tiu_cmdbuff_data;
    std::vector<uint32_t>  gdma_cmdbuff_data;
    bool hasAnomaly() const { return !anomalies.empty(); }
};

// 1024bit 寄存器定义（支持配置化）
class RegDescriptor {
public:
    static constexpr uint32_t REG_SIZE_BITS = 2048;
    static constexpr uint32_t REG_SIZE_BYTES = REG_SIZE_BITS / 8;
    static constexpr uint32_t REG_SIZE_WORDS = REG_SIZE_BITS / 32;

    // 引擎类型特定的最大字数常量
    static constexpr uint32_t MAX_TIU_CTRL_WORDS = 2048 / 32;   // 64 words
    static constexpr uint32_t MAX_DMA_CTRL_WORDS = 256 / 32;    // 8 words
    static constexpr uint32_t MAX_DMA_CMD_WORDS = 768 / 32;     // 24 words
    static constexpr uint32_t MAX_DEFAULT_WORDS = 1024 /32;    // 32 words

    RegDescriptor(const std::string& name, uint64_t base_addr)
        : name_(name), base_addr_(base_addr) {}

    // 添加字段定义（用于从Excel导入配置）
    void addField(const RegField& field) {
        fields_.push_back(field);
    }

    // 批量添加字段
    void addFields(const std::vector<RegField>& fields) {
        fields_.insert(fields_.end(), fields.begin(), fields.end());
    }

    // 从原始数据解析字段值
    RegReadResult parse(const std::vector<uint32_t>& raw_data) const;

    // 获取所有字段定义
    const std::vector<RegField>& getFields() const { return fields_; }

    // 获取寄存器名称和地址
    const std::string& getName() const { return name_; }
    uint64_t getBaseAddr() const { return base_addr_; }

    // 设置字段的期望值（用于代码中直接配置）
    void setFieldExpectedValue(const std::string& field_name, uint64_t expected) {
        for (auto& field : fields_) {
            if (field.name == field_name) {
                field.expected_value = expected;
                return;
            }
        }
    }

    // 设置字段的有效值集合
    void setFieldValidValues(const std::string& field_name, const std::vector<uint64_t>& values) {
        for (auto& field : fields_) {
            if (field.name == field_name) {
                field.valid_values.clear();
                for (auto v : values) {
                    field.valid_values.insert(v);
                }
                return;
            }
        }
    }

    // Set invalid values
    void setFieldInvalidValues(const std::string& field_name, const std::vector<uint64_t>& values) {
        for (auto& field : fields_) {
            if (field.name == field_name) {
                field.invalid_values.clear();
                for (auto v : values) {
                    field.invalid_values.insert(v);
                }
                return;
            }
        }
    }

private:
    std::string name_;
    uint64_t base_addr_;
    std::vector<RegField> fields_;
};

// 寄存器值读取器接口
class IRegisterReader {
public:
    virtual ~IRegisterReader() = default;
    virtual std::vector<uint32_t> read(uint64_t addr, uint32_t num_words) = 0;
    virtual bool isValid() const = 0;
};

// 字段值提取工具
class FieldExtractor {
public:
    // 从原始数据中提取指定bit范围的值
    static uint64_t extract(const std::vector<uint32_t>& raw_data,
                           uint32_t start_bit, uint32_t bit_width,
                           regPostProcessFunc post_process);
};

} // namespace tpu_debugger
