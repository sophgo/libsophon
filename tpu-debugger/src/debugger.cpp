#include "debugger.hpp"
#include "json_dump.hpp"
#include <iostream>

namespace tpu_debugger {

Debugger::Debugger(const DebuggerConfig& config) : config_(config) {}

Debugger::~Debugger() = default;

// Helper function to get version string
// In a real build, this should be replaced by macros defined in CMake/Makefile:
// e.g., -DDEBUGGER_VERSION="1.0.0" -DDEBUGGER_COMMIT_HASH="abc1234" -DDEBUGGER_BUILD_DATE="2023-10-27"
void Debugger::printVersion() const {
    std::string version = "";

// #ifdef DEBUGGER_VERSION
//     version += DEBUGGER_VERSION;
// #else
//     version += "0.1.0-dev";
// #endif

#ifdef COMMIT_HASH
    version += " (commit: ";
    version += COMMIT_HASH;
    version += ")";
#endif

#ifdef BUILD_DATE
    version += " [built: ";
    version += BUILD_DATE;
    version += "]";
#endif

    std::cout << "TPU Debugger Version: " << version << std::endl;
}

bool Debugger::initialize() {
    // Print version info at startup
    printVersion();

    // Create register reader (JSON file mode or SoC mode)
    if (!config_.file_path.empty()) {
        // Check if it's a JSON file
        if (config_.file_path.size() >= 5 &&
            config_.file_path.substr(config_.file_path.size() - 5) == ".json") {
            std::cout << "Using JSON file mode: " << config_.file_path << std::endl;

            // Create JsonReader and update config
            auto json_reader = ReaderFactory::createJsonReader(config_.file_path, &config_);
            reader_ = json_reader;

            if (!reader_ || !reader_->isValid()) {
                std::cerr << "Failed to initialize JSON reader" << std::endl;
                return false;
            }
        } else {
            std::cerr << "Error: -f parameter only supports .json files" << std::endl;
            return false;
        }
    } else {
        reader_ = ReaderFactory::createReader(ReaderType::SoC);
    }

    if (!reader_ || !reader_->isValid()) {
        std::cerr << "Failed to initialize register reader" << std::endl;
        return false;
    }

    // User specified chip name
    chip_ = createChipByName(config_.chip_name, reader_);
    if (!chip_) {
        return false;
    }

    std::cout << "Chip: " << chip_->getInfo().name
              << ", cores: " << chip_->getInfo().num_cores << std::endl;
    return true;
}

bool Debugger::run() {
    return run(config_.dump_file);
}

bool Debugger::run(const std::string& dump_file) {
    if (!chip_) {
        std::cerr << "Debugger not initialized" << std::endl;
        return false;
    }

    performRead();

    // Dump to JSON if requested
    if (!dump_file.empty()) {
        std::string json = dumpToJson(*chip_);
        if (saveJsonToFile(json, dump_file)) {
            std::cout << "Dumped data to: " << dump_file << std::endl;
        } else {
            std::cerr << "Failed to dump data to: " << dump_file << std::endl;
            return false;
        }
    }

    printResults();
    return true;
}

void Debugger::performRead() {
    if (config_.target_cores.empty() && config_.target_engines.empty()) {
        chip_->readAll();
    } else if (!config_.target_cores.empty() && config_.target_engines.empty()) {
        for (uint32_t core_id : config_.target_cores) {
            chip_->readByCore(core_id);
        }
    } else if (config_.target_cores.empty() && !config_.target_engines.empty()) {
        for (EngineType type : config_.target_engines) {
            chip_->readByType(type);
        }
    } else {
        chip_->readAll();
    }
}

void Debugger::printResults() const {
    std::cout << "\n--- Anomaly Report ---" << std::endl;
    chip_->printAnomalies();

    if (config_.print_all_registers) {
        std::cout << "\n--- All Registers ---" << std::endl;
        chip_->printAll();
    }

    printSummary();
}

size_t Debugger::getAnomalyCount() const {
    if (!chip_) return 0;
    size_t count = 0;
    for (const auto& engine : chip_->getEngines()) {
        count += engine->getAllAnomalies().size();
    }
    return count;
}

void Debugger::printSummary() const {
    size_t n = getAnomalyCount();
    std::cout << "\nTotal anomalies: " << n << std::endl;
    std::cout << "Status: " << (n == 0 ? "OK" : "WARNING") << std::endl;
}

std::string Debugger::getChipName() const {
    if (chip_) {
        return chip_->getInfo().name;
    }
    return config_.chip_name;
}

} // namespace tpu_debugger
