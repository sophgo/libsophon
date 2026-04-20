#ifndef UTILS_
#define UTILS_
#include "reg_value.h"
#include <string>
#include <vector>

const char* opToString(TSK_TYPE task, int op_value);

// Convert binary data to hex string (space separated, with 0x prefix)
std::string binaryToHexString(const std::vector<std::uint32_t>& data);

#endif