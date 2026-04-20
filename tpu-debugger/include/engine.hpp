#pragma once

#include "register.hpp"
#include "mmap.hpp"
#include <memory>
#include <vector>
#include <string>

namespace tpu_debugger {

// Engine类型枚举
enum class EngineType {
    TIU_CMD = 0,    // TIU 命令寄存器
    TIU_CTRL,       // TIU 控制寄存器
    DMA_CMD,        // DMA 命令寄存器
    DMA_CTRL,       // DMA 控制寄存器
    UNKNOWN
};

// 将EngineType转换为字符串
inline std::string engineTypeToString(EngineType type) {
    switch (type) {
        case EngineType::TIU_CMD:  return "TIU_CMD";
        case EngineType::TIU_CTRL: return "TIU_CTRL";
        case EngineType::DMA_CMD:  return "DMA_CMD";
        case EngineType::DMA_CTRL: return "DMA_CTRL";
        default:                   return "UNKNOWN";
    }
}

// Engine 类：表示一个具体的引擎实例（如某个核的TIU_CMD）
class Engine {
public:
    Engine(uint32_t core_id, EngineType type, uint64_t base_addr,
           std::shared_ptr<IRegisterReader> reader);

    // 添加寄存器定义
    void addRegister(const RegDescriptor& reg_desc);

    // 读取所有寄存器
    void readAll();

    // 直接设置数据（用于 JSON 模式）
    void setData(const std::vector<uint32_t>& data);

    // 获取读取结果
    const std::vector<RegReadResult>& getResults() const { return results_; }

    // 检查是否有异常
    bool hasAnomalies() const;

    // 获取所有异常信息
    std::vector<RegAnomaly> getAllAnomalies() const;

    // 打印结果
    void print() const;

    // 打印异常信息
    void printAnomalies() const;

    // Getters
    uint32_t getCoreId() const { return core_id_; }
    EngineType getType() const { return type_; }
    uint64_t getBaseAddr() const { return base_addr_; }
    std::string getName() const;

    // 获取当前引擎类型的最大读取字数
    uint32_t getMaxWords() const;

private:
    uint32_t core_id_;
    EngineType type_;
    uint64_t base_addr_;
    std::shared_ptr<IRegisterReader> reader_;
    std::vector<RegDescriptor> registers_;
    std::vector<RegReadResult> results_;
};

using EnginePtr = std::unique_ptr<Engine>;

} // namespace tpu_debugger
