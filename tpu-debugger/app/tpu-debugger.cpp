#include "debugger.hpp"
#include "chip.hpp"
#include <iostream>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace tpu_debugger;

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "\nOptions:\n"
              << "  -h, --help              Show this help message\n"
              << "  -a, --all               Print all registers (not just anomalies)\n"
              << "  --chip <name>           Specify chip by name (required without -f) e.g., bm1684, bm1688, cv186x, cv184x \n"
              << "  -c, --core <id>         Check specific core (repeatable)\n"
            //   << "  -e, --engine <type>    Check specific engine: tiu_cmd, tiu_ctrl, dma_cmd, dma_ctrl (repeatable)\n"
              << "  -f, --file <path>       Read from JSON file (auto-detects chip and cores)\n"
              << "  -d, --dump <path>       Dump all engine data to JSON file\n"
              << "  --list-chips            List all registered chip names and exit\n"
              << "\nExamples:\n"
              << "  sudo " << prog << " -f test_data.json              # Read from JSON file\n"
              << "  sudo " << prog << " --chip bm1688 --dump           # Read from /dev/mem, save json file\n";
}

EngineType parseEngineType(const char* str) {
    if (strcmp(str, "tiu_cmd") == 0)  return EngineType::TIU_CMD;
    if (strcmp(str, "tiu_ctrl") == 0) return EngineType::TIU_CTRL;
    if (strcmp(str, "dma_cmd") == 0)  return EngineType::DMA_CMD;
    if (strcmp(str, "dma_ctrl") == 0) return EngineType::DMA_CTRL;
    return EngineType::UNKNOWN;
}

int main(int argc, char **argv) {
    DebuggerConfig config;
    bool has_json_file = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--list-chips") == 0) {
            std::cout << "Registered chips:";
            for (const auto& name : ChipRegistry::instance().registeredNames()) {
                std::cout << " " << name;
            }
            std::cout << std::endl;
            return 0;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            config.print_all_registers = true;
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--core") == 0) && i + 1 < argc) {
            config.target_cores.push_back(static_cast<uint32_t>(std::atoi(argv[++i])));
        } else if ((strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--engine") == 0) && i + 1 < argc) {
            EngineType type = parseEngineType(argv[++i]);
            if (type == EngineType::UNKNOWN) {
                std::cerr << "Unknown engine type: " << argv[i] << std::endl;
                return 1;
            }
            config.target_engines.push_back(type);
        } else if (strcmp(argv[i], "--chip") == 0 && i + 1 < argc) {
            config.chip_name = argv[++i];
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) && i + 1 < argc) {
            config.file_path = argv[++i];
            has_json_file = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dump") == 0) {
            // -d/--dump 后面可以跟文件名，也可以不跟（自动生成）
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                // 下一个参数不是以 - 开头，认为是文件名
                config.dump_file = argv[++i];
            } else {
                // 没有指定文件名，标记为需要自动生成
                config.dump_file = "";  // 空字符串表示需要自动生成
                config.auto_generate_dump_file = true;
            }
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // If JSON file is specified, ignore --chip and -c parameters
    if (has_json_file) {
        if (!config.chip_name.empty()) {
            std::cout << "Warning: --chip is ignored when using -f with JSON file" << std::endl;
        }
        if (!config.target_cores.empty()) {
            std::cout << "Warning: -c is ignored when using -f with JSON file" << std::endl;
            config.target_cores.clear();
        }
    } else {
        // Without JSON file, chip name is required
        if (config.chip_name.empty()) {
            std::cout << "You must specify chip name with --chip option (or use -f with JSON file)" << std::endl;
            return 1;
        }
    }

    Debugger debugger(config);

    if (!debugger.initialize()) {
        return 1;
    }

    // 如果需要自动生成 dump 文件名
    std::string dump_file;
    if (config.auto_generate_dump_file) {
        // 获取芯片名称（小写）
        std::string chip_name = debugger.getChipName();
        // 转换为小写
        for (auto& c : chip_name) {
            c = std::tolower(c);
        }

        // 获取当前时间
        std::time_t now = std::time(nullptr);
        std::tm* local_time = std::localtime(&now);

        // 格式化时间：年月日-时分秒
        std::ostringstream time_str;
        time_str << std::put_time(local_time, "%Y%m%d-%H%M%S");

        // 生成文件名：芯片名称_年月日-时分秒.json
        dump_file = chip_name + "_" + time_str.str() + ".json";
    } else {
        dump_file = config.dump_file;
    }

    if (!debugger.run(dump_file)) {
        return 1;
    }

    return static_cast<int>(debugger.getAnomalyCount());
}

