#include "json_dumper_base.hpp"
#include "utils.hpp"
#include <iostream>
#include <sstream>

namespace tpu_debugger {

JsonDumperBase::FactoryMap& JsonDumperBase::getFactoryMap() {
    static FactoryMap instance;
    return instance;
}

void JsonDumperBase::registerDumper(const std::string& name, DumperFactory factory) {
    getFactoryMap()[name] = std::move(factory);
}

std::unique_ptr<JsonDumperBase> JsonDumperBase::createDumper(const std::string& name) {
    const auto& map = getFactoryMap();
    auto it = map.find(name);
    if (it != map.end()) {
        return it->second();
    }
    std::cerr << "Warning: No dumper registered for chip: " << name << std::endl;
    return nullptr;
}

std::string JsonDumperBase::formatAddr(uint64_t addr, bool is_const) {
    if (is_const) {
        return "const " + std::to_string(addr);
    }
    std::ostringstream ss;
    ss << "0x" << std::hex << addr;
    return ss.str();
}

nlohmann::ordered_json JsonDumperBase::buildOperandTensor(const RegReadResult& result,
                                                           int operand_index) {
    nlohmann::ordered_json tensor;

    std::string dtype_field = "des_opt_opd" + std::to_string(operand_index) + "_prec";
    uint64_t dtype_val = getFieldValue(result, dtype_field, 0);
    DATA_FORMAT dtype = static_cast<DATA_FORMAT>(dtype_val);
    tensor["dtype"] = magic_enum::enum_name(dtype);

    std::string const_field = "des_opt_opd" + std::to_string(operand_index) + "_const";
    uint64_t is_const = getFieldValue(result, const_field, 0);

    std::string addr_field = "des_opd" + std::to_string(operand_index) + "_addr";
    uint64_t addr = getFieldValue(result, addr_field, 0);

    if (is_const == 0) {
        std::string store_mode_field = "des_short_opd" + std::to_string(operand_index) + "_str";
        uint64_t store_mode_val = getFieldValue(result, store_mode_field, 0);
        STORE_MODE store_mode = static_cast<STORE_MODE>(store_mode_val);
        tensor["layout"] = magic_enum::enum_name(store_mode);

        std::string n_field = "des_opd" + std::to_string(operand_index) + "_n";
        std::string c_field = "des_opd" + std::to_string(operand_index) + "_c";
        std::string h_field = "des_opd" + std::to_string(operand_index) + "_h";
        std::string w_field = "des_opd" + std::to_string(operand_index) + "_w";

        uint64_t n = getFieldValue(result, n_field, 1);
        uint64_t c = getFieldValue(result, c_field, 1);
        uint64_t h = getFieldValue(result, h_field, 1);
        uint64_t w = getFieldValue(result, w_field, 1);
        tensor["shape"] = "<" + std::to_string(n) + "x" + std::to_string(c) + "x" +
                          std::to_string(h) + "x" + std::to_string(w) + ">";
    }

    tensor["address"] = formatAddr(addr, is_const != 0);
    return tensor;
}

nlohmann::ordered_json JsonDumperBase::buildResultTensor(const RegReadResult& result) {
    nlohmann::ordered_json tensor;

    uint64_t dtype_val = getFieldValue(result, "des_opt_res0_prec", 0);
    DATA_FORMAT dtype = static_cast<DATA_FORMAT>(dtype_val);
    tensor["dtype"] = magic_enum::enum_name(dtype);

    uint64_t store_mode_val = getFieldValue(result, "des_short_res0_str", 0);
    STORE_MODE store_mode = static_cast<STORE_MODE>(store_mode_val);
    tensor["layout"] = magic_enum::enum_name(store_mode);

    uint64_t n = getFieldValue(result, "des_res0_n", 1);
    uint64_t c = getFieldValue(result, "des_res0_c", 1);
    uint64_t h = getFieldValue(result, "des_res0_h", 1);
    uint64_t w = getFieldValue(result, "des_res0_w", 1);
    tensor["shape"] = "<" + std::to_string(n) + "x" + std::to_string(c) + "x" +
                      std::to_string(h) + "x" + std::to_string(w) + ">";

    uint64_t addr = getFieldValue(result, "des_res0_addr", 0);
    tensor["address"] = formatAddr(addr, false);

    return tensor;
}

uint64_t JsonDumperBase::getFieldValue(const RegReadResult& result, const std::string& field_name,
                                       uint64_t default_val) {
    for (const auto& fv : result.field_values) {
        if (fv.first == field_name) {
            return fv.second;
        }
    }
    printWarning(field_name, default_val);
    return default_val;
}

void JsonDumperBase::printWarning(const std::string& field_name, const uint64_t default_val) {
    std::cerr << "Warning: Field '" << field_name << "' not found in engine , using default value (" << default_val << ")." << std::endl;
}

void JsonDumperBase::buildTiuTensorsNode(nlohmann::ordered_json& tiu_obj, const RegReadResult& result) {
    uint64_t opd_num = getFieldValue(result, "des_tsk_opd_num", 2);
    tiu_obj["operands_num"] = opd_num;

    if (opd_num == 0) {
        return;
    }

    for (int i = 0; i < static_cast<int>(opd_num); ++i) {
        std::string key = "src" + std::to_string(i) + "_tensor";
        tiu_obj[key] = buildOperandTensor(result, i);
    }

    tiu_obj["res_tensor"] = buildResultTensor(result);
}

void JsonDumperBase::buildDmaTensorsNode(nlohmann::ordered_json& Dma_obj, const RegReadResult& result){
    (void)(Dma_obj);
    (void)(result);
}

nlohmann::ordered_json JsonDumperBase::addDetailedInformationNode(const std::vector<std::unique_ptr<Engine>>& engines) {
    nlohmann::ordered_json detailed_info_arr = nlohmann::ordered_json::array();

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

            nlohmann::ordered_json fields_obj;
            const auto& results = engine->getResults();

            // Collect all field values from all results
            for (const auto& result : results) {
                for (const auto& field_pair : result.field_values) {
                    std::ostringstream value_ss;
                    value_ss << "0x" << std::hex << field_pair.second;
                    fields_obj[field_pair.first] = value_ss.str();
                }
            }

            engine_obj["fields"] = fields_obj;

            // 对于TIU_CTRL和DMA_CTRL引擎，添加cmdbuf节点
            if (type_str == "TIU_CTRL" || type_str == "DMA_CTRL") {
                for (const auto& result : results) {
                    if (type_str == "TIU_CTRL" && !result.tiu_cmdbuff_data.empty()) {
                        engine_obj["cmdbuf"] = binaryToHexString(result.tiu_cmdbuff_data);
                    } else if (type_str == "DMA_CTRL" && !result.gdma_cmdbuff_data.empty()) {
                        engine_obj["cmdbuf"] = binaryToHexString(result.gdma_cmdbuff_data);
                    }
                }
            }

            engines_arr.push_back(engine_obj);
        }
        core_obj["engines"] = engines_arr;
        detailed_info_arr.push_back(core_obj);
    }

    return detailed_info_arr;
}
} // namespace tpu_debugger
