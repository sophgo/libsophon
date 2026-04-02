#include "register.hpp"
#include <algorithm>

namespace tpu_debugger {

// 从原始数据中提取指定bit范围的值
uint64_t FieldExtractor::extract(const std::vector<uint32_t>& raw_data,
                                 uint32_t start_bit, uint32_t bit_width,
                                 regPostProcessFunc post_process) {
    if (bit_width == 0 || bit_width > 64) {
        return 0;
    }

    uint64_t result = 0;
    uint32_t bits_collected = 0;
    uint32_t current_bit = start_bit;

    while (bits_collected < bit_width) {
        uint32_t word_idx = current_bit / 32;
        uint32_t bit_offset = current_bit % 32;

        if (word_idx >= raw_data.size()) {
            break;
        }

        // 计算当前word中可以读取的bit数
        uint32_t bits_available = 32 - bit_offset;
        uint32_t bits_to_read = std::min(bits_available, bit_width - bits_collected);

        // 提取值
        uint32_t mask = (bits_to_read == 32) ? 0xFFFFFFFF : ((1u << bits_to_read) - 1);
        uint64_t value = (raw_data[word_idx] >> bit_offset) & mask;

        // 合并到结果
        result |= (value << bits_collected);

        bits_collected += bits_to_read;
        current_bit += bits_to_read;
    }

    if (post_process) {
        result = post_process(result);
    }

    return result;
}

// 从原始数据解析字段值
RegReadResult RegDescriptor::parse(const std::vector<uint32_t>& raw_data) const {
    RegReadResult result;
    result.register_name = name_;
    result.raw_data = raw_data;

    for (const auto& field : fields_) {
        // 提取字段值
        uint64_t value = FieldExtractor::extract(raw_data, field.start_bit, field.bit_width, field.post_process);
        result.field_values.push_back({field.name, value});

        // 检查是否有效
        if (!field.isValid(value)) {
            RegAnomaly anomaly;
            anomaly.field_name = field.name;
            anomaly.actual_value = value;
            anomaly.reason = "invalid_value";
            result.anomalies.push_back(anomaly);
        }
        // 检查是否与期望值匹配
        else if (!field.matchesExpected(value)) {
            RegAnomaly anomaly;
            anomaly.field_name = field.name;
            anomaly.actual_value = value;
            anomaly.expected_value = field.expected_value;
            anomaly.reason = "mismatch_expected";
            result.anomalies.push_back(anomaly);
        }
    }

    return result;
}

} // namespace tpu_debugger
