#include "json_dump.hpp"
#include "utils.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <iomanip>
#include "json_dumper_base.hpp"
#include "chips/bm1684x/bm1684x_json_dumper.hpp"
#include "chips/bm1688/bm1688_json_dumper.hpp"
#include "chips/cv184x/cv184x_json_dumper.hpp"
namespace tpu_debugger {

// Register dumpers at static initialization
REGISTER_JSON_DUMPER("BM1684X", BM1684XJsonDumper)
REGISTER_JSON_DUMPER("BM1688", BM1688JsonDumper)
REGISTER_JSON_DUMPER("CV186X", CV186XJsonDumper)
REGISTER_JSON_DUMPER("CV184X", CV184XJsonDumper)

std::string dumpToJson(const Chip& chip) {
    const auto& info = chip.getInfo();
    const auto& engines = chip.getEngines();

    // Create dumper using factory
    std::unique_ptr<JsonDumperBase> dumper = JsonDumperBase::createDumper(info.name);

    // Build JSON using ordered_json
    nlohmann::ordered_json j;
    j["chip"] = info.name;
    j["core"] = info.num_cores;

    nlohmann::ordered_json cores_arr = nlohmann::ordered_json::array();

    // Group engines by core
    std::map<uint32_t, std::vector<const Engine*>> core_engines;
    for (const auto& engine_ptr : engines) {
        core_engines[engine_ptr->getCoreId()].push_back(engine_ptr.get());
    }

    for (const auto& core_pair : core_engines) {
        nlohmann::ordered_json core_obj;
        core_obj["core_id"] = core_pair.first;

        nlohmann::ordered_json engines_arr = nlohmann::ordered_json::array();
        for (const auto* engine : core_pair.second) {
            nlohmann::ordered_json engine_obj;
            std::string type_str = engineTypeToString(engine->getType());
            engine_obj["name"] = type_str;

            // Format base_addr as hex string
            std::ostringstream addr_ss;
            addr_ss << "0x" << std::hex << std::setfill('0') << std::setw(8) << engine->getBaseAddr();
            engine_obj["base_addr"] = addr_ss.str();

            // Get raw data from engine results
            const auto& results = engine->getResults();
            std::vector<uint32_t> raw_data;
            for (const auto& result : results) {
                raw_data.insert(raw_data.end(), result.raw_data.begin(), result.raw_data.end());
            }
            engine_obj["data"] = binaryToHexString(raw_data);

            // 添加 dma 或 bdc 节点
            if (dumper) {
                if (type_str == "DMA_CMD") {
                    dumper->addDmaCmdInfoNode(engine_obj, *engine);
                } else if (type_str == "TIU_CMD") {
                    dumper->addTiuCmdInfoNode(engine_obj, *engine);
                } else if (type_str == "DMA_CTRL") {
                     dumper->addDmaCtrlInfoNode(engine_obj, *engine);
                } else if (type_str == "TIU_CTRL") {
                     dumper->addTiuCtrlInfoNode(engine_obj, *engine);
                } else {
                     std::cerr << "Warning: Invalid engine type : '"<< type_str <<"'"<< std::endl;
                }
            }

            engines_arr.push_back(engine_obj);
        }
        core_obj["engines"] = engines_arr;
        cores_arr.push_back(core_obj);
    }
    j["cores"] = cores_arr;

    // Add detailed information node if dumper exists
    if (dumper) {
        j["detailed information"] = dumper->addDetailedInformationNode(engines);
    }

    // Return formatted JSON with 4-space indentation
    return j.dump(4);
}

bool saveJsonToFile(const std::string& json, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    file << json;
    return file.good();
}

} // namespace tpu_debugger
