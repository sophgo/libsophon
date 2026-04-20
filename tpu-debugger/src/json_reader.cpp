#include "json_reader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace tpu_debugger {

JsonReader::JsonReader(const std::string& filepath) {
    valid_ = loadFromFile(filepath);
}

bool JsonReader::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open JSON file: " << filepath << std::endl;
        return false;
    }

    try {
        // 使用 nlohmann::ordered_json 解析整个文件
        nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
        file.close();

        // 解析 "chip" 字段
        if (j.contains("chip")) {
            chip_name_ = j["chip"];
        }

        // 解析 "core" 字段（core 数量）
        if (j.contains("core")) {
            num_cores_ = j["core"].get<uint32_t>();
        }

        // 解析 "cores" 数组
        if (!j.contains("cores") || !j["cores"].is_array()) {
            std::cerr << "JSON file missing 'cores' array" << std::endl;
            return false;
        }

        uint32_t expected_core_id = 0;
        for (const auto& core : j["cores"]) {
            CoreData core_data;

            // 解析 core_id
            if (core.contains("core_id")) {
                core_data.core_id = core["core_id"].get<uint32_t>();

                // 验证 core_id 与数组索引的一致性
                if (core_data.core_id != expected_core_id) {
                    std::cerr << "Warning: Core ID mismatch. Expected " << expected_core_id
                              << " but found " << core_data.core_id << ". Using found ID." << std::endl;
                }
            } else {
                std::cerr << "Warning: Core missing 'core_id', using expected ID: " << expected_core_id << std::endl;
                core_data.core_id = expected_core_id;
            }

            // 解析 engines 数组
            if (core.contains("engines") && core["engines"].is_array()) {
                for (const auto& engine : core["engines"]) {
                    EngineData engine_data;

                    // 解析 name
                    if (engine.contains("name")) {
                        engine_data.name = engine["name"];
                    }

                    // 解析 base_addr
                    if (engine.contains("base_addr")) {
                        std::string addr_str = engine["base_addr"];
                        engine_data.base_addr = std::stoull(addr_str, nullptr, 16);
                    }

                    // 解析 data
                    if (engine.contains("data")) {
                        std::string hex_data = engine["data"];
                        parseHexData(hex_data, engine_data.data);
                    }

                    if (!engine_data.name.empty()) {
                        core_data.engines.push_back(engine_data);
                    }
                }
            }

            cores_data_.push_back(core_data);
            expected_core_id++;
        }

    } catch (const nlohmann::ordered_json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        file.close();
        return false;
    }

    return !cores_data_.empty();
}

void JsonReader::parseHexData(const std::string& hex_str, std::vector<uint32_t>& data) {
    std::istringstream iss(hex_str);
    std::string token;

    // 支持格式："0x10000000" 或 "10000000"
    while (iss >> token) {
        // 移除 "0x" 前缀（如果存在）
        if (token.length() >= 2 && token.substr(0, 2) == "0x") {
            token = token.substr(2);
        }

        if (!token.empty()) {
            try {
                uint32_t value = std::stoul(token, nullptr, 16);
                data.push_back(value);
            } catch (...) {
                // 解析失败，跳过
            }
        }
    }

    // 补零到 32 个 uint32_t
    while (data.size() < 32) {
        data.push_back(0);
    }
}

std::vector<uint32_t> JsonReader::read(uint64_t addr, uint32_t num_words) {
    (void)(addr);
    // JSON 模式下不应该调用此方法
    std::cerr << "Error: JsonReader::read() should not be called in JSON mode" << std::endl;
    std::vector<uint32_t> result;
    result.resize(num_words, 0);
    return result;
}

std::vector<std::string> JsonReader::getEngineNames(uint32_t core_id) const {
    std::vector<std::string> names;
    for (const auto& core : cores_data_) {
        if (core.core_id == core_id) {
            for (const auto& engine : core.engines) {
                names.push_back(engine.name);
            }
            break;
        }
    }
    return names;
}

std::optional<std::vector<uint32_t>> JsonReader::getEngineData(uint32_t core_id, const std::string& engine_name) const {
    for (const auto& core : cores_data_) {
        if (core.core_id == core_id) {
            for (const auto& engine : core.engines) {
                if (engine.name == engine_name) {
                    return engine.data;
                }
            }
        }
    }
    return std::nullopt;
}

bool JsonReader::hasEngineData(uint32_t core_id, const std::string& engine_name) const {
    for (const auto& core : cores_data_) {
        if (core.core_id == core_id) {
            for (const auto& engine : core.engines) {
                if (engine.name == engine_name) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace tpu_debugger
