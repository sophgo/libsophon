#pragma once

#include "engine.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <algorithm>

namespace tpu_debugger {

// Chip metadata
struct ChipInfo {
    std::string name;
    uint32_t num_cores;
    uint64_t tiu_cmd_base;
    uint64_t tiu_ctrl_base;
    uint64_t dma_cmd_base;
    uint64_t dma_ctrl_base;
    uint64_t core_offset;  // address stride between cores
};

// Interface for providing register definitions for a specific chip
class IChipConfig {
public:
    virtual ~IChipConfig() = default;
    virtual ChipInfo getChipInfo() const = 0;
    virtual std::vector<RegDescriptor> getTIUCmdRegisters(uint32_t core_id) const = 0;
    virtual std::vector<RegDescriptor> getTIUCtrlRegisters(uint32_t core_id) const = 0;
    virtual std::vector<RegDescriptor> getDMACmdRegisters(uint32_t core_id) const = 0;
    virtual std::vector<RegDescriptor> getDMACtrlRegisters(uint32_t core_id) const = 0;
};

// Base chip class
class Chip {
public:
    Chip(const ChipInfo& info, std::shared_ptr<IRegisterReader> reader);
    virtual ~Chip() = default;

    // Initialize all engines from config
    virtual void initialize(const IChipConfig& config);

    // Read all engines
    void readAll();

    // Read engines of a specific type
    void readByType(EngineType type);

    // Read all engines of a specific core
    void readByCore(uint32_t core_id);

    bool hasAnomalies() const;
    void printAnomalies() const;
    void printAll() const;

    const ChipInfo& getInfo() const { return info_; }
    const std::vector<EnginePtr>& getEngines() const { return engines_; }

protected:
    ChipInfo info_;
    std::shared_ptr<IRegisterReader> reader_;
    std::vector<EnginePtr> engines_;

    void createEnginesForCore(uint32_t core_id, const IChipConfig& config);
};

using ChipPtr = std::unique_ptr<Chip>;
using ChipFactory = std::function<ChipPtr(std::shared_ptr<IRegisterReader>)>;

// Chip registry: maps chip_id and name to factory functions.
class ChipRegistry {
public:
    static ChipRegistry& instance() {
        static ChipRegistry reg;
        return reg;
    }

    // Register a chip with its name.
    void registerChip(const std::string& name, ChipFactory factory) {
        // Store lowercase name for case-insensitive lookup
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        name_map_[lower] = factory;
        names_.push_back(name);
    }

    // Create by chip name (case-insensitive, e.g., "BM1688").
    ChipPtr createByName(const std::string& name, std::shared_ptr<IRegisterReader> reader) const {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        auto it = name_map_.find(lower);
        if (it == name_map_.end()) return nullptr;
        return it->second(reader);
    }

    // List all registered chip names.
    const std::vector<std::string>& registeredNames() const { return names_; }

private:
    ChipRegistry() = default;
    std::unordered_map<std::string, ChipFactory> name_map_;
    std::vector<std::string> names_;
};

// Helper used by REGISTER_CHIP macro to perform registration at static init time.
struct ChipRegistrar {
    ChipRegistrar(const std::string& name, ChipFactory factory) {
        ChipRegistry::instance().registerChip(name, std::move(factory));
    }
};

// Register a chip class. Place in chip_registry.cpp (or any translation unit).
// ChipClass must expose a static chip_id constant and a constructor taking
// std::shared_ptr<IRegisterReader>.
#define REGISTER_CHIP(ChipClass) \
    static ::tpu_debugger::ChipRegistrar _registrar_##ChipClass( \
        ChipClass::chipName(), \
        [](std::shared_ptr<::tpu_debugger::IRegisterReader> r) \
            -> ::tpu_debugger::ChipPtr { \
            return std::make_unique<ChipClass>(r); \
        });

// Create a chip instance by user-specified name (case-insensitive).
// Returns nullptr if the name is not registered.
ChipPtr createChipByName(const std::string& name, std::shared_ptr<IRegisterReader> reader);

} // namespace tpu_debugger
