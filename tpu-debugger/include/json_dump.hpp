#pragma once

#include "chip.hpp"
#include <string>

namespace tpu_debugger {

// Dump all engine data to JSON format
std::string dumpToJson(const Chip& chip);

// Save JSON to file
bool saveJsonToFile(const std::string& json, const std::string& filepath);

} // namespace tpu_debugger
