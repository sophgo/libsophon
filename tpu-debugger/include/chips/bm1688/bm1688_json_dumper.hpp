#pragma once

#include "json_dumper_base.hpp"
#include "../../reg_value.h"

namespace tpu_debugger {

class BM1688JsonDumper : public JsonDumperBase {
public:
    ~BM1688JsonDumper() override = default;

    void addDmaCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
    void addTiuCmdInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
    void addDmaCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
    void addTiuCtrlInfoNode(nlohmann::ordered_json& engine_obj, const Engine& engine) override;
};

// CV186X uses the same dumper as BM1688 (single-core variant of BM1688)
using CV186XJsonDumper = BM1688JsonDumper;

} // namespace tpu_debugger
