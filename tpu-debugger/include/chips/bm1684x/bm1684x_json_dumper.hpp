#pragma once

#include "json_dumper_base.hpp"
#include "../../reg_value.h"

namespace tpu_debugger {

class BM1684XJsonDumper : public JsonDumperBase {
public:
    ~BM1684XJsonDumper() override = default;

    void addDmaCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
    void addTiuCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
    void addDmaCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
    void addTiuCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
};

} // namespace tpu_debugger
