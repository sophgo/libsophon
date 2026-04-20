#pragma once

// Include reg_value.h first for magic_enum customize specialization
#include "reg_value.h"
#include <nlohmann/json.hpp>
#include "engine.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace tpu_debugger {

// Forward declaration
class Engine;

class JsonDumperBase {
public:
    virtual ~JsonDumperBase() = default;

    // Add 'DmaCmdInfo' node for DMA_CMD engine
    virtual void addDmaCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) = 0;

    // Add 'TiuCmdInfo' node for TIU_CMD engine
    virtual void addTiuCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) = 0;

    // Add 'DmaCtrlInfo' node for DMA_CTRL engine
    virtual void addDmaCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) = 0;

    // Add 'TiuCtrlInfo' node for TIU_CTRL engine
    virtual void addTiuCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) = 0;

    // Add 'detailed information' node for all engines (default implementation provided)
    virtual nlohmann::ordered_json addDetailedInformationNode(const std::vector<std::unique_ptr<Engine>>& engines);

    // Factory function type
    using DumperFactory = std::function<std::unique_ptr<JsonDumperBase>()>;

    // Register a dumper factory for a chip name
    static void registerDumper(const std::string& name, DumperFactory factory);

    // Create a dumper by chip name
    static std::unique_ptr<JsonDumperBase> createDumper(const std::string& name);

protected:
    // Helper function: Extract field value from RegReadResult
    uint64_t getFieldValue(const RegReadResult& result, const std::string& field_name,
                          uint64_t default_val = 0);

    void buildTiuTensorsNode(nlohmann::ordered_json& tiu_obj, const RegReadResult& result);
    void buildDmaTensorsNode(nlohmann::ordered_json& Dma_obj, const RegReadResult& result);

private:
    // Helper function: Print warning
    void printWarning(const std::string& field_name, const uint64_t default_val);

    // Helper function: Format address
    std::string formatAddr(uint64_t addr, bool is_const);

    // Helper function: Build operand tensor
    nlohmann::ordered_json buildOperandTensor(const RegReadResult& result, int operand_index);

    // Helper function: Build result tensor
    nlohmann::ordered_json buildResultTensor(const RegReadResult& result);

    // Factory registry (singleton pattern)
    using FactoryMap = std::unordered_map<std::string, DumperFactory>;
    static FactoryMap& getFactoryMap();
};

// Helper macro to register dumper at static initialization time
#define REGISTER_JSON_DUMPER(chip_name, dumper_class) \
    namespace { \
    struct dumper_class##Registrar { \
        dumper_class##Registrar() { \
            ::tpu_debugger::JsonDumperBase::registerDumper(chip_name, []() { \
                return std::make_unique<dumper_class>(); \
            }); \
        } \
    }; \
    static dumper_class##Registrar g_##dumper_class##Registrar; \
    } // namespace

} // namespace tpu_debugger
