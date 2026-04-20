#include "engine.hpp"
#include <iostream>
#include <iomanip>

namespace tpu_debugger {

Engine::Engine(uint32_t core_id, EngineType type, uint64_t base_addr,
               std::shared_ptr<IRegisterReader> reader)
    : core_id_(core_id), type_(type), base_addr_(base_addr), reader_(reader) {}

void Engine::addRegister(const RegDescriptor& reg_desc) {
    registers_.push_back(reg_desc);
}

void Engine::readAll() {
    results_.clear();
    for (const auto& reg_desc : registers_) {
        // 根据引擎类型确定最大读取字数
        uint32_t max_words = getMaxWords();

        // 读取原始数据（根据引擎类型确定字数）
        std::vector<uint32_t> raw_data = reader_->read(
            reg_desc.getBaseAddr(),
            max_words
        );

        // 解析寄存器
        RegReadResult result = reg_desc.parse(raw_data);

        // SOC模式下，对于TIU_CTRL和DMA_CTRL引擎，读取cmdbuff数据
        if (type_ == EngineType::TIU_CTRL || type_ == EngineType::DMA_CTRL) {
            // 查找cfg_des_addr字段
            uint64_t cfg_des_addr = 0;
            bool found_addr = false;
            for (const auto& field : result.field_values) {
                if (field.first == "cfg_des_addr") {
                    cfg_des_addr = field.second;
                    found_addr = true;
                    break;
                }
            }

            // 如果找到地址且不为0，读取cmdbuff数据
            if (found_addr && cfg_des_addr != 0) {
                // 读取1024字节 = 256个words
                if(cfg_des_addr > this->base_addr_) {
                    std::cout << result.register_name << " cfg_des_addr : 0x" << std::hex << std::setfill('0') <<
                        std::setw(8) << cfg_des_addr << std::endl;
                    std::vector<uint32_t> cmdbuff_data = reader_->read(cfg_des_addr, 256);

                    // 根据引擎类型保存到相应的变量
                    if (type_ == EngineType::TIU_CTRL) {
                        result.tiu_cmdbuff_data = std::move(cmdbuff_data);
                    } else if (type_ == EngineType::DMA_CTRL) {
                        result.gdma_cmdbuff_data = std::move(cmdbuff_data);
                    }
                } else {
                    std::cout << result.register_name << " cfg_des_addr : 0x" << std::hex << std::setfill('0') <<
                        std::setw(8) << cfg_des_addr << " cfg_des_addr address invalid!" << std::endl;
                }
            }
        }

        results_.push_back(result);
    }
}

void Engine::setData(const std::vector<uint32_t>& data) {
    results_.clear();
    for (const auto& reg_desc : registers_) {
        // 根据引擎类型确定最大字数
        uint32_t max_words = getMaxWords();

        // 使用提供的数据直接填充
        std::vector<uint32_t> raw_data = data;
        // 如果数据超过最大值，截断到最大值
        if (raw_data.size() > max_words) {
            raw_data.resize(max_words);
        }
        // 如果数据小于最大值，保持原大小（不补零）

        // 解析寄存器
        RegReadResult result = reg_desc.parse(raw_data);
        results_.push_back(result);
    }
}

bool Engine::hasAnomalies() const {
    for (const auto& result : results_) {
        if (result.hasAnomaly()) {
            return true;
        }
    }
    return false;
}

std::vector<RegAnomaly> Engine::getAllAnomalies() const {
    std::vector<RegAnomaly> all_anomalies;
    for (const auto& result : results_) {
        all_anomalies.insert(all_anomalies.end(),
                            result.anomalies.begin(),
                            result.anomalies.end());
    }
    return all_anomalies;
}

void Engine::print() const {
    std::cout << "=== " << getName() << " ===" << std::endl;

    for (const auto& result : results_) {
        std::cout << "Register: " << result.register_name << std::endl;

        for (const auto& field : result.field_values) {
            std::cout << "  " << field.first << ": 0x" << std::hex << field.second << std::dec;

            // 检查是否是异常字段
            bool is_anomaly = false;
            for (const auto& anomaly : result.anomalies) {
                if (anomaly.field_name == field.first) {
                    is_anomaly = true;
                    break;
                }
            }

            if (is_anomaly) {
                std::cout << " [ANOMALY]";
            }
            std::cout << std::endl;
        }
    }
}

void Engine::printAnomalies() const {
    bool has_any = false;
    for (const auto& result : results_) {
        if (result.hasAnomaly()) {
            if (!has_any) {
                // ANSI color codes: \033[31m = red, \033[0m = reset
                std::cout << "!!! \033[31mANOMALIES\033[0m in " << getName() << " !!!" << std::endl;
                has_any = true;
            }
            std::cout << "  Register: " << result.register_name << std::endl;
            for (const auto& anomaly : result.anomalies) {
                std::cout << "    - " << anomaly.field_name
                         << ": 0x" << std::hex << anomaly.actual_value << std::dec;
                if (anomaly.expected_value.has_value()) {
                    std::cout << " (expected: 0x" << std::hex
                             << anomaly.expected_value.value() << std::dec << ")";
                }
                std::cout << " [" << anomaly.reason << "]" << std::endl;
            }
        }
    }
}

std::string Engine::getName() const {
    return "Core" + std::to_string(core_id_) + "_" + engineTypeToString(type_);
}

uint32_t Engine::getMaxWords() const {
    switch (type_) {
        case EngineType::TIU_CTRL:
            return RegDescriptor::MAX_TIU_CTRL_WORDS;  // 64 words (2048/32)
        case EngineType::DMA_CTRL:
            return RegDescriptor::MAX_DMA_CTRL_WORDS;  // 8 words (256/32)
        case EngineType::DMA_CMD:
            return RegDescriptor::MAX_DMA_CMD_WORDS;   // 24 words (768/32)
        default:
            return RegDescriptor::MAX_DEFAULT_WORDS;   // 32 words (1024/32)
    }
}

} // namespace tpu_debugger
