#pragma once

#include "chip.hpp"
#include "readers.hpp"
#include <memory>
#include <string>
#include <vector>

namespace tpu_debugger {

struct DebuggerConfig {
    bool print_all_registers = false;       // print all fields, not only anomalies
    std::vector<uint32_t> target_cores;     // empty = all cores
    std::vector<EngineType> target_engines; // empty = all engine types
    std::string chip_name;                  // if non-empty, override auto-detection
    std::string file_path;                  // if non-empty, use file reader instead of /dev/mem
    std::string dump_file;                  // if non-empty, dump all engine data to JSON file
    bool auto_generate_dump_file = false;   // if true, auto-generate dump file name
};

class Debugger {
public:
    explicit Debugger(const DebuggerConfig& config = DebuggerConfig());
    ~Debugger();

    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;

    // Detect chip via bmlib (or use chip_name override), create reader and chip instance
    bool initialize();

    // Read registers and print results
    bool run();
    bool run(const std::string& dump_file);  // 重载，支持动态指定 dump 文件

    size_t getAnomalyCount() const;

    // Get chip name (for auto-generating dump file name)
    std::string getChipName() const;

private:
    DebuggerConfig config_;
    // bm_handle_t bm_handle_ = nullptr;
    std::shared_ptr<IRegisterReader> reader_;
    ChipPtr chip_;

    void performRead();
    void printResults() const;
    void printSummary() const;
    void printVersion() const;
};

} // namespace tpu_debugger
