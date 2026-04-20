#include "utils.hpp"
#include <sstream>
#include <iomanip>

// Convert binary data to hex string (space separated, with 0x prefix)
std::string binaryToHexString(const std::vector<std::uint32_t>& data) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << data[i];
    }
    return oss.str();
}

// 主函数：根据任务类型和操作值获取字符串名称
const char* opToString(TSK_TYPE task, int op_value) {
    thread_local char buffer[32];
    switch (task) {
        case TSK_TYPE::CONV:
            return magic_enum::enum_name(static_cast<CONV_OP>(op_value)).data();
        case TSK_TYPE::PD:
            return magic_enum::enum_name(static_cast<PD_OP>(op_value)).data();
        case TSK_TYPE::MM:
            return magic_enum::enum_name(static_cast<MM_OP>(op_value)).data();
        case TSK_TYPE::AR:
            return magic_enum::enum_name(static_cast<AR_OP>(op_value)).data();
        case TSK_TYPE::RQDQ:
            return magic_enum::enum_name(static_cast<RQDQ_OP>(op_value)).data();
        case TSK_TYPE::TRANS_BC:
            return magic_enum::enum_name(static_cast<TRANS_BC_OP>(op_value)).data();
        case TSK_TYPE::SG:
            return magic_enum::enum_name(static_cast<SG_OP>(op_value)).data();
        case TSK_TYPE::SFU:
            return magic_enum::enum_name(static_cast<SFU_OP>(op_value)).data();
        case TSK_TYPE::LIN:
            return magic_enum::enum_name(static_cast<LIN_OP>(op_value)).data();
        case TSK_TYPE::CMP:
            return magic_enum::enum_name(static_cast<CMP_OP>(op_value)).data();
        case TSK_TYPE::VC:
            return magic_enum::enum_name(static_cast<AR_OP>(op_value)).data();
        case TSK_TYPE::SYS:
            return magic_enum::enum_name(static_cast<SYS_TYPE>(op_value)).data();
        default:
            std::snprintf(buffer, sizeof(buffer), "Unknown tsk_type:%d", static_cast<int>(task));
            return buffer;
    }
}
