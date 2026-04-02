#include "chips/bm1684x/bm1684x_json_dumper.hpp"
#include "utils.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

namespace tpu_debugger {

void BM1684XJsonDumper::addDmaCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) {
    nlohmann::ordered_json dma_obj;

    // 获取引擎结果
    const auto& results = engine.getResults();
    if (results.empty()) {
        std::cerr << "Warning: No results found for DMA_CMD engine" << std::endl;
        return;
    }
    const auto& result = results[0];

    uint64_t cmd_id_val = getFieldValue(result, "cmd_id", -1);
    uint64_t cmd_id_dep_val = getFieldValue(result, "cmd_id_dep", -1);
    dma_obj["cmd_id"] = cmd_id_val;
    dma_obj["cmd_id_dep"] = cmd_id_dep_val;

    // cmd_type: 从 "cmd_type" 字段获取值，映射到 GDMA_TYPE 枚举
    uint64_t cmd_type_val = getFieldValue(result, "cmd_type", 0);

    GDMA_TYPE cmd_type = static_cast<GDMA_TYPE>(cmd_type_val);
    dma_obj["cmd_type"] = magic_enum::enum_name(cmd_type);

    // data_format: 从 "src_data_format" 字段获取值，映射到 DATA_FORMAT 枚举
    uint64_t format_val = getFieldValue(result, "src_data_format", 0);

    DATA_FORMAT format = static_cast<DATA_FORMAT>(format_val);
    dma_obj["data_format"] = magic_enum::enum_name(format);

    // src_tensor
    nlohmann::ordered_json src_tensor;
    uint64_t src_n = getFieldValue(result, "src_nsize", 1);
    uint64_t src_c = getFieldValue(result, "src_csize", 1);
    uint64_t src_h = getFieldValue(result, "src_hsize", 1);
    uint64_t src_w = getFieldValue(result, "src_wsize", 1);
    src_tensor["shape"] = "<" + std::to_string(src_n) + "x" + std::to_string(src_c) +
     "x" + std::to_string(src_h) + "x" + std::to_string(src_w) + ">";

    uint64_t src_addr = getFieldValue(result, "src_start_addr", 0);
    std::ostringstream src_addr_ss;
    src_addr_ss << "0x" << std::hex << src_addr;
    src_tensor["addr"] = src_addr_ss.str();
    dma_obj["src_tensor"] = src_tensor;

    // dst_tensor
    nlohmann::ordered_json dst_tensor;
    uint64_t dst_n = getFieldValue(result, "dst_nsize", 1);
    uint64_t dst_c = getFieldValue(result, "dst_csize", 1);
    uint64_t dst_h = getFieldValue(result, "dst_hsize", 1);
    uint64_t dst_w = getFieldValue(result, "dst_wsize", 1);
    dst_tensor["shape"] = std::to_string(dst_n) + "x" + std::to_string(dst_c) +
     "x" + std::to_string(dst_h) + "x" + std::to_string(dst_w);

    uint64_t dst_addr = getFieldValue(result, "dst_start_addr", 0);

    std::ostringstream dst_addr_ss;
    dst_addr_ss << "0x" << std::hex << dst_addr;
    dst_tensor["addr"] = dst_addr_ss.str();
    dma_obj["dst_tensor"] = dst_tensor;

    engine_obj["dma_cmd_info"] = dma_obj;
}

void BM1684XJsonDumper::addTiuCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) {
    nlohmann::ordered_json tiu_obj;

    const auto& results = engine.getResults();
    if (results.empty()) {
        std::cerr << "Warning: No results found for TIU_CMD engine" << std::endl;
        return;
    }
    const auto& result = results[0];
    std::string engine_name = engine.getName();
    uint64_t cmd_id_val = getFieldValue(result, "des_cmd_id", -1);
    uint64_t cmd_id_dep_val = getFieldValue(result, "des_cmd_id_dep", -1);
    tiu_obj["cmd_id"] = cmd_id_val;
    tiu_obj["cmd_id_dep"] = cmd_id_dep_val;

    // tesk_type: 从 "des_tsk_typ" 字段获取值，映射到 TSK_TYPE 枚举
    uint64_t tsk_type_val = getFieldValue(result, "des_tsk_typ", 0);

    TSK_TYPE tsk_type = static_cast<TSK_TYPE>(tsk_type_val);
    tiu_obj["tesk_type"] = magic_enum::enum_name(tsk_type);

    // tesk_eu_type: 使用 opToString 函数
    uint64_t eu_type_val = getFieldValue(result, "des_tsk_eu_typ", 0);

    tiu_obj["tesk_eu_type"] = opToString(tsk_type, eu_type_val);

    buildTiuTensorsNode(tiu_obj, result);

    engine_obj["tiu_cmd_info"] = tiu_obj;
}

void BM1684XJsonDumper::addDmaCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) {
    nlohmann::ordered_json dma_obj;
    // 获取引擎结果
    const auto& results = engine.getResults();
    if (results.empty()) {
        std::cerr << "Warning: No results found for DMA_Ctrl engine" << std::endl;
        return;
    }
    const auto& result = results[0];
    (void)(result);
    engine_obj["dma_ctrl_info"] = dma_obj;
}

void BM1684XJsonDumper::addTiuCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) {
    nlohmann::ordered_json tiu_obj;
    // 获取引擎结果
    const auto& results = engine.getResults();
    if (results.empty()) {
        std::cerr << "Warning: No results found for TIU_CTRL engine" << std::endl;
        return;
    }
    const auto& result = results[0];
    (void)(result);
    engine_obj["tiu_ctrl_info"] = tiu_obj;
}
} // namespace tpu_debugger
