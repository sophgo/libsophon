#include "chip.hpp"
#include "json_reader.hpp"
#include <iostream>

namespace tpu_debugger {

// ============== Chip ==============

Chip::Chip(const ChipInfo& info, std::shared_ptr<IRegisterReader> reader)
    : info_(info), reader_(reader) {}

void Chip::initialize(const IChipConfig& config) {
    engines_.clear();

    for (uint32_t core_id = 0; core_id < info_.num_cores; ++core_id) {
        createEnginesForCore(core_id, config);
    }
}

void Chip::createEnginesForCore(uint32_t core_id, const IChipConfig& config) {
    // 计算该核心的基地址偏移
    uint64_t core_offset = core_id * info_.core_offset;

    // 创建 TIU_CMD Engine
    {
        auto engine = std::make_unique<Engine>(
            core_id, EngineType::TIU_CMD,
            info_.tiu_cmd_base + core_offset,
            reader_
        );
        auto regs = config.getTIUCmdRegisters(core_id);
        for (const auto& reg : regs) {
            engine->addRegister(reg);
        }
        engines_.push_back(std::move(engine));
    }

    // 创建 TIU_CTRL Engine
    {
        auto engine = std::make_unique<Engine>(
            core_id, EngineType::TIU_CTRL,
            info_.tiu_ctrl_base + core_offset,
            reader_
        );
        auto regs = config.getTIUCtrlRegisters(core_id);
        for (const auto& reg : regs) {
            engine->addRegister(reg);
        }
        engines_.push_back(std::move(engine));
    }

    // 创建 DMA_CMD Engine
    {
        auto engine = std::make_unique<Engine>(
            core_id, EngineType::DMA_CMD,
            info_.dma_cmd_base + core_offset,
            reader_
        );
        auto regs = config.getDMACmdRegisters(core_id);
        for (const auto& reg : regs) {
            engine->addRegister(reg);
        }
        engines_.push_back(std::move(engine));
    }

    // 创建 DMA_CTRL Engine
    {
        auto engine = std::make_unique<Engine>(
            core_id, EngineType::DMA_CTRL,
            info_.dma_ctrl_base + core_offset,
            reader_
        );
        auto regs = config.getDMACtrlRegisters(core_id);
        for (const auto& reg : regs) {
            engine->addRegister(reg);
        }
        engines_.push_back(std::move(engine));
    }
}

void Chip::readAll() {
    // 检查是否是 JsonReader
    auto json_reader = std::dynamic_pointer_cast<JsonReader>(reader_);

    for (auto& engine : engines_) {
        if (json_reader) {
            // JSON 模式：从 JsonReader 获取数据
            std::string engine_name = engineTypeToString(engine->getType());
            uint32_t core_id = engine->getCoreId();

            if (!json_reader->hasEngineData(core_id, engine_name)) {
                std::cerr << "Error: Missing data for engine " << engine_name
                         << " in core " << core_id << std::endl;
                // 报错退出
                exit(1);
            }

            auto data = json_reader->getEngineData(core_id, engine_name);
            if (data.has_value()) {
                engine->setData(data.value());
            }
        } else {
            // SoC 模式：通过 reader 读取
            engine->readAll();
        }
    }
}

void Chip::readByType(EngineType type) {
    // 检查是否是 JsonReader
    auto json_reader = std::dynamic_pointer_cast<JsonReader>(reader_);

    for (auto& engine : engines_) {
        if (engine->getType() == type) {
            if (json_reader) {
                // JSON 模式：从 JsonReader 获取数据
                std::string engine_name = engineTypeToString(engine->getType());
                uint32_t core_id = engine->getCoreId();

                if (!json_reader->hasEngineData(core_id, engine_name)) {
                    std::cerr << "Error: Missing data for engine " << engine_name
                             << " in core " << core_id << std::endl;
                    exit(1);
                }

                auto data = json_reader->getEngineData(core_id, engine_name);
                if (data.has_value()) {
                    engine->setData(data.value());
                }
            } else {
                // SoC 模式：通过 reader 读取
                engine->readAll();
            }
        }
    }
}

void Chip::readByCore(uint32_t core_id) {
    // 检查是否是 JsonReader
    auto json_reader = std::dynamic_pointer_cast<JsonReader>(reader_);

    for (auto& engine : engines_) {
        if (engine->getCoreId() == core_id) {
            if (json_reader) {
                // JSON 模式：从 JsonReader 获取数据
                std::string engine_name = engineTypeToString(engine->getType());

                if (!json_reader->hasEngineData(core_id, engine_name)) {
                    std::cerr << "Error: Missing data for engine " << engine_name
                             << " in core " << core_id << std::endl;
                    exit(1);
                }

                auto data = json_reader->getEngineData(core_id, engine_name);
                if (data.has_value()) {
                    engine->setData(data.value());
                }
            } else {
                // SoC 模式：通过 reader 读取
                engine->readAll();
            }
        }
    }
}

bool Chip::hasAnomalies() const {
    for (const auto& engine : engines_) {
        if (engine->hasAnomalies()) {
            return true;
        }
    }
    return false;
}

void Chip::printAnomalies() const {
    bool has_any = false;
    for (const auto& engine : engines_) {
        if (engine->hasAnomalies()) {
            engine->printAnomalies();
            has_any = true;
        }
    }
    if (!has_any) {
        std::cout << "No anomalies detected." << std::endl;
    }
}

void Chip::printAll() const {
    std::cout << "========================================" << std::endl;
    std::cout << "Chip: " << info_.name << std::endl;
    std::cout << "Number of cores: " << info_.num_cores << std::endl;
    std::cout << "========================================" << std::endl;

    for (const auto& engine : engines_) {
        engine->print();
        std::cout << std::endl;
    }
}

// ============== Factory functions ==============
ChipPtr createChipByName(const std::string& name, std::shared_ptr<IRegisterReader> reader) {
    ChipPtr chip = ChipRegistry::instance().createByName(name, reader);
    if (!chip) {
        std::cerr << "Unknown chip name: " << name << std::endl;
        std::cerr << "Supported chips:";
        for (const auto& n : ChipRegistry::instance().registeredNames()) {
            std::cerr << " " << n;
        }
        std::cerr << std::endl;
    }
    return chip;
}

} // namespace tpu_debugger
