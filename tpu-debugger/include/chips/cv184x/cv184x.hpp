#pragma once

#include "chip.hpp"

namespace tpu_debugger {

// cv184x configs(also known as mars3)
class CV184XConfig : public IChipConfig {
public:
    ChipInfo getChipInfo() const override {
        return {
            "CV184X",         // name
            1,                // num_cores
            0xc040000ull,     // tiu_cmd_base
            0xc040100ull,     // tiu_ctrl_base
            0xc000000ull,     // dma_cmd_base
            0xc001000ull,     // dma_ctrl_base
            0x0,              // core_offset
        };
    }

    std::vector<RegDescriptor> getTIUCmdRegisters(uint32_t core_id) const override {
        return createTIUCmdRegs(core_id);
    }

    std::vector<RegDescriptor> getTIUCtrlRegisters(uint32_t core_id) const override {
        return createTIUCtrlRegs(core_id);
    }

    std::vector<RegDescriptor> getDMACmdRegisters(uint32_t core_id) const override {
        return createDMACmdRegs(core_id);
    }

    std::vector<RegDescriptor> getDMACtrlRegisters(uint32_t core_id) const override {
        return createDMACtrlRegs(core_id);
    }

private:
    static uint64_t commonAddrProcess(uint64_t value) { return value << 7; }
    static uint64_t baseAddrProcess(uint64_t value) { return value << 8; }
    std::vector<RegDescriptor> createTIUCmdRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().tiu_cmd_base + core_id * getChipInfo().core_offset;
        RegDescriptor R("TIU_CMD", base);

        // Descriptor Fields
        R.addField({"des_cmd_short", 0, 1, "0: Long Des; 1: Short Des"});
        // R.addField({"des_op_code", 1, 16, "Operation Code"});
        R.addField({"des_cmd_id_dep", 17, 20, "Dependency CMD ID; Wait for the specified external engine ID (des_cmd_id_en) to complete before executing this descriptor"});
        R.addField({"des_dep_en", 37, 1, "Cmd id dependency enable; 0: Disable dependency; 1: Enable dependency"});
        R.addField({"des_dbg_mode", 40, 1, "Debug breakpoint descriptor mode"});
        R.addField({"des_tsk_typ", 41, 4, "Instruction task type; 0: convolution; 1: depthwise or pooling; 2: matrix multiply && matrix multiply2; 3: arithmetic && SEG; 4: RQ && DQ; 5: TRANS && BC; 6: scatter_gather && scatter_gather_line; 7: linear_arithmetic (not supported); 8: (not supported); 9: special_function; 10: fused_linear; 11: (not supported); 12: SYS_TR_WR; 13: fused_cmpare; 14: vector correlation; 15: system"});
        R.addField({"des_tsk_eu_typ", 45, 5, "Instruction operation type; Specified according to instruction type"});
        R.addField({"des_opt_rq", 50, 1, "Whether to quantize the result"});
        R.addField({"des_tsk_opd_num", 51, 2, "Number of input operands; 0: 4 operands; 1: 1 operand; 2: 2 operands; 3: 3 operands"});
        R.addField({"des_pad_mode", 53, 2, "If instruction type is CONV, this is padding mode; Otherwise, reserved0"});
        R.addField({"des_opt_res0_sign", 55, 1, "When result is INT type, indicates whether the result is signed or unsigned; 0: unsigned; 1: signed"});
        // R.addField({"des_pwr_step", 59, 4, "Power step"});
        R.addField({"des_intr_en", 63, 1, "0: This instruction execution end does not issue interrupt; 1: This instruction execution end issues interrupt"});
        R.addField({"des_opt_res_add", 64, 1, "Whether the result accumulates with the content at the original res0_addr address"});
        R.addField({"des_opt_relu", 65, 1, "reserved0"});
        R.addField({"des_opt_left_tran", 66, 1, "Whether to transpose, used only in matrix multiplication"});
        R.addField({"des_opt_opd4_const", 67, 1, "Whether opd4 is a constant"});
        R.addField({"des_opt_kernel_rotate", 68, 1, "Whether kernel is transposed"});
        R.addField({"des_opt_opd0_sign", 69, 1, "When opd0 data type is INT8, indicates whether opd0 is signed; 0: unsigned; 1: signed"});
        R.addField({"des_opt_opd1_sign", 70, 1, "When opd1 data type is INT8, indicates whether opd1 is signed; 0: unsigned; 1: signed"});
        R.addField({"des_opt_opd2_sign", 71, 1, "When opd2 data type is INT8, indicates whether opd2 is signed; 0: unsigned; 1: signed"});
        R.addField({"des_opt_res0_prec", 72, 3, "res0 data type; 0: INT8; 1: FP16; 2: FP32; 3: INT16; 4: INT32; 5: BFP16; 6: INT64; others: not support"});
        R.addField({"des_opt_opd0_prec", 75, 3, "opd0 data type; 0: INT8; 1: FP16; 2: FP32; 3: INT16; 4: INT32; 5: BFP16; others: not support"});
        R.addField({"des_opt_opd1_prec", 78, 3, "opd1 data type; 0: INT8; 1: FP16; 2: FP32; 3: INT16; 4: INT32; 5: BFP16; others: not support"});
        R.addField({"des_opt_opd2_prec", 81, 3, "opd2 data type; 0: INT8; 1: FP16; 2: FP32; 3: INT16; 4: INT32; 5: BFP16; others: not support"});
        R.addField({"des_opt_opd0_const", 84, 1, "Whether opd0 is a constant; If it is a constant, its value is in des_opd0_addr; 0: not constant; 1: constant"});
        R.addField({"des_opt_opd1_const", 85, 1, "Whether opd1 is a constant; If it is a constant, its value is in des_opd1_addr; 0: not constant; 1: constant"});
        R.addField({"des_opt_opd2_const", 86, 1, "Whether opd2 is a constant; If it is a constant, its value is in des_opd1_addr; 0: not constant; 1: constant"});
        R.addField({"des_short_res0_str", 87, 3, "res0 data storage format; 0: Aligned; 1: Compact; 2: Bias; 3: Tensor; others: not support"});
        R.addField({"des_short_opd0_str", 90, 3, "opd0 data storage format; 0: Aligned; 1: Compact; 2: Bias; 3: Tensor; others: not support"});
        R.addField({"des_short_opd1_str", 93, 3, "opd1 data storage format; 0: Aligned; 1: Compact; 2: Bias; 3: Tensor; others: not support"});
        R.addField({"des_short_opd2_str", 96, 3, "opd2 data storage format; 0: Aligned; 1: Compact; 2: Bias; 3: Tensor; others: not support"});
        R.addField({"des_opt_res_add_sign", 99, 1, "For int8 operation add_result, indicates whether the original result is signed or unsigned; 0: unsigned; 1: signed"});
        R.addField({"des_sym_range", 125, 1, "For conv/mm2 result quantization, whether to perform symmetric saturation: 0: No symmetric saturation; 1: Perform symmetric saturation"});
        R.addField({"des_opt_opd3_const", 126, 1, "Whether opd3 is a constant; If it is a constant, its value is in des_opd3_addr; 0: not constant; 1: constant"});
        R.addField({"des_opt_opd5_const", 127, 1, "For conv/mm2 result quantization, whether quantization parameters are constants; 0: not constant; 1: constant"});
        R.addField({"des_opd0_x_ins0", 128, 4, "Number of zeros inserted after each element in the row direction of opd0, valid for 0-14"});
        R.addField({"des_opd0_y_ins0", 132, 4, "Number of zeros inserted after each element in the column direction of opd0, valid for 0-14"});
        R.addField({"des_opd1_x_ins0", 136, 4, "Number of zeros inserted after each element in the row direction of opd1, valid for 0-14"});
        R.addField({"des_opd1_y_ins0", 140, 4, "Number of zeros inserted after each element in the column direction of opd1, valid for 0-14"});
        R.addField({"des_opd0_up_pad", 144, 4, "Number of padding rows above opd0, valid for 0-15"});
        R.addField({"des_opd0_dn_pad", 148, 4, "Number of padding rows below opd0, valid for 0-15"});
        R.addField({"des_opd0_lf_pad", 152, 4, "Number of padding columns to the left of opd0, valid for 0-15"});
        R.addField({"des_opd0_rt_pad", 156, 4, "Number of padding columns to the right of opd0, valid for 0-15"});
        R.addField({"des_res_op_x_str", 160, 4, "Convolution/pooling horizontal stride, valid for 1-15"});
        R.addField({"des_res_op_y_str", 164, 4, "Convolution/pooling vertical stride, valid for 1-15"});
        R.addField({"des_res0_h_shift", 168, 4, "Right shift value for res0's h, used only in tensor arithmetic"});
        R.addField({"des_res0_w_shift", 172, 4, "Right shift value for res0's w, used only in tensor arithmetic"});
        R.addField({"des_opd0_h_shift", 176, 4, "Right shift value for opd0's h, used only in tensor arithmetic"});
        R.addField({"des_opd0_w_shift", 180, 4, "Right shift value for opd0's w, used only in tensor arithmetic"});
        R.addField({"des_opd1_h_shift", 184, 4, "Right shift value for opd1's h, used only in tensor arithmetic"});
        R.addField({"des_opd1_w_shift", 188, 4, "Right shift value for opd1's w, used only in tensor arithmetic"});
        R.addField({"des_tsk_lane_num", 192, 64, "Lane mask; 0: Corresponding lane will not be written; 1: Corresponding lane can be written"});
        R.addField({"des_res0_n", 256, 16, "res0's n, 0 is invalid; For matrix multiply: number of rows in res0 matrix; For mdsum: opd0's n, res0's n is 1"});
        R.addField({"des_res0_c", 272, 16, "res0's c, 0 is invalid"});
        R.addField({"des_res0_h", 288, 16, "res0's h, 0 is invalid; In conv, depthwise conv and tensor arithmetic, the maximum value of res0's h is 2047(2048?)"});
        R.addField({"des_res0_w", 304, 16, "res0's w, 0 is invalid; In conv, depthwise conv and tensor arithmetic, the maximum value of res0's w is 2047(2048?)"});
        R.addField({"des_opd0_n", 320, 16, "opd0's n, 0 is invalid"});
        R.addField({"des_opd0_c", 336, 16, "opd0's c, 0 is invalid"});
        R.addField({"des_opd0_h", 352, 16, "opd0's h, 0 is invalid; In conv, depthwise conv and tensor arithmetic, the maximum value of opd0's h is 2047(2048?)"});
        R.addField({"des_opd0_w", 368, 16, "opd0's w, 0 is invalid; In conv, depthwise conv and tensor arithmetic, the maximum value of opd0's h is 2047(2048?)"});
        R.addField({"des_opd1_n", 384, 16, "opd1's n, 0 is invalid"});
        R.addField({"des_opd1_c", 400, 16, "opd1's c, 0 is invalid"});
        R.addField({"des_opd1_h", 416, 16, "opd1's h, 0 is invalid; In conv, depthwise conv and tensor arithmetic, the maximum value of opd1's h is 2047(2048?)"});
        R.addField({"des_opd1_w", 432, 16, "opd1's w, 0 is invalid; In conv, depthwise conv and tensor arithmetic, the maximum value of opd1's h is 2047(2048?)"});
        R.addField({"des_res0_n_str", 448, 16, "res0's n stride"});
        R.addField({"des_res0_c_str", 464, 16, "res0's c stride"});
        R.addField({"des_opd0_n_str", 480, 16, "opd0's n stride"});
        R.addField({"des_opd0_c_str", 496, 16, "opd0's c stride"});
        R.addField({"des_opd1_n_str", 512, 16, "opd1's n stride"});
        R.addField({"des_opd1_c_str", 528, 16, "opd1's n stride"});
        R.addField({"des_opd2_n_str", 544, 16, "opd2's n stride"});
        R.addField({"des_opd2_c_str", 560, 16, "opd2's c stride"});
        R.addField({"des_res0_addr", 576, 32, "res0 start address"});
        R.addField({"des_opd0_addr", 608, 32, "If des_opt_opd0_const is 0, opd0 start address; If des_opt_opd0_const is 1, opd0 constant value"});
        R.addField({"des_opd1_addr", 640, 32, "If des_opt_opd1_const is 0, opd1 start address; If des_opt_opd1_const is 1, opd1 constant value"});
        R.addField({"des_opd2_addr", 672, 32, "If des_opt_opd2_const is 0, opd2 start address; If des_opt_opd2_const is 1, opd2 constant value"});
        R.addField({"des_res0_h_str", 704, 32, "res0's h stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_res0_w_str", 736, 32, "res0's w stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_opd0_h_str", 768, 32, "opd0's h stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_opd0_w_str", 800, 32, "opd0's w stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_opd1_h_str", 832, 32, "opd1's h stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_opd1_w_str", 864, 32, "opd1's w stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_opd2_h_str", 896, 32, "opd2's h stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_opd2_w_str", 928, 32, "opd2's w stride, for instructions other than tensor arithmetic, only the lower 19 bits are used"});
        R.addField({"des_res1_addr", 960, 32, "res1 start address"});
        R.addField({"des_opd3_addr", 992, 32, "If des_opt_opd3_const is 0, opd3 start address; If des_opt_opd3_const is 1, opd3 constant value"});

        R.setFieldExpectedValue("des_tsk_lane_num", 0xffffffffffffffff);
        R.setFieldInvalidValues("des_tsk_typ", {7, 8, 11});
        regs.push_back(R);
        return regs;
    }

    std::vector<RegDescriptor> createTIUCtrlRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().tiu_ctrl_base + core_id * getChipInfo().core_offset;
        RegDescriptor R("TIU_CTRL", base);

        // Register: cfg0_L (Address: 0x400)
        R.addField({"cfg_enable", 0, 1, "TPU enable"});
        R.addField({"cfg_intr_stat", 1, 1, "R/W1C TIU interrupt Status"});
        R.addField({"cfg_instr_barrier", 26, 1, "Instruction barrier"});
        R.addField({"cfg_data_barrier", 27, 1, "Data barrier"});
        R.addField({"cfg_fb_pins_en", 28, 1, "pad insv read buffer"});
        R.addField({"cfg_fb_pxl_en", 29, 1, "pxl read buffer"});
        R.addField({"cfg_intr_en", 30, 1, "TIU interrupt global enable"});
        R.addField({"cfg_cmdbuf_intr_en", 31, 1, "des buf interrupt enable"});
        R.addField({"cfg_cmdbuf_avail", 32, 12, "[7:0] - cfgr_cmdbuf_avail; [11:8] - 4'b0; des buf remaining space, cfgr_cmdbuf_avail*128Byte, max 128*128Byte"});
        R.addField({"cfg_cmdbuf_intr_biten", 44, 4, "Enable cmdbuf interrupt"});
        R.addField({"cfg_cmdbuf_intr_stat", 48, 4, "cmdbuf interrupt status"});
        // R.addField({"cfg_dma_ban_overlap", 52, 1, "dma"});
        // R.addField({"cfg_dma_space_margin", 53, 1, "dma bus, read fifo space control, reduce outstanding count"});
        R.addField({"cfg_des_mode", 54, 1, "des mode indicator signal"});
        R.addField({"cfg_des_clr_busy", 55, 1, "Indicator signal when des_clr_state is not idle"});
        R.addField({"cfg_fb_only_cmm2", 56, 1, "Only open pxl/pins buffer during conv mm2 pord"});
        R.addField({"cfg_fb_conv_dis", 57, 1, "Disable pxl/pins buffer under conv instruction"});
        R.addField({"cfg_fb_mm2_dis", 58, 1, "Disable pxl/pins buffer under mm2 instruction"});
        R.addField({"cfg_fb_mm2_nt_dis", 59, 1, "Do not open pxl/pins buffer under mm2_nt instruction"});
        R.addField({"cfg_fb_pord_dis", 60, 1, "Do not open pxl/pins buffer under pord instruction"});
        // R.addField({"cfg_cmm2_i4_opd0_1p", 61, 1, "cmm2 opd0 w4 1step"});
        // R.addField({"cfg_rst_cmdid", 63, 1, "Auto clear to 0"});

        // Register: cfg0_H (Address: 0x80C)
        R.addField({"cfg_des_addr_vld", 64, 1, "Enable descriptor mode; Write 1'b1 to enable; Auto clear to 0"});
        // R.addField({"cfg_des_clr", 65, 1, "Write 1 to clear des buf"});
        R.addField({"cfg_des_addr", 66, 33, "Descriptor address, {cfg_des_addr[32:0], 7'b0}; Descriptor end triggers interrupt via sys end instruction.", commonAddrProcess});
        // R.addField({"cfg0_rsvd2", 99, 27, "reserved"});
        R.addField({"cfg_des_resp_err", 126, 1, "DES mode, AX interface slave err or decode err, set by hardware, cleared by software"});
        R.addField({"cfg_cmd_illegal", 127, 1, "Illegal instruction, set by hardware, cleared by software"});

        // Register: cfg1_L (Address: 0x1410)
        // R.addField({"cfg_des_arqos", 128, 4, "DES port ARQoS"});
        // R.addField({"cfg_des_awqos", 132, 4, "DES port AWQoS"});
        // R.addField({"cfg1_rsvd0", 136, 24, "reserved"});
        R.addField({"cfg_current_cmdid", 160, 20, "Last executed command id"});
        // R.addField({"cfg1_rsvd1", 128 + 52, 12, "reserved"});

        // Register: cfg1_H (Address: 0x1C18)
        // R.addField({"cfg1_rsvd2", 128 + 64, 64, "reserved"});

        // Register: cfg2_L (Address: 0x2420)
        // R.addField({"cfg2_rsvd0", 256 + 0, 64, "reserved"});

        // Register: cfg2_H (Address: 0x2C28)
        // R.addField({"cfg_arcache", 256 + 64, 4, "arcache to tpu cmd - tpu des arcache rs"});
        // R.addField({"cfg_awcache", 256 + 68, 4, "awcache to tpu perf monitor - awcache"});
        // R.addField({"cfg2_rsvd1", 256 + 72, 56, "reserved"});

        // Register: cfg3_L (Address: 0x3430)
        // R.addField({"cfg_load_mon_en", 384 + 0, 1, "Enable loading monitor function"});
        // R.addField({"cfg_load_mon_clr", 384 + 1, 1, "Synchronously clear load_mon_cnt, instr_vld_cnt, instr_iss_cnt; Write 1, hardware auto clears to 0"});
        // R.addField({"cfg3_rsvd0", 384 + 2, 62, "reserved"});

        // Register: cfg3_H (Address: 0x3C38)
        // R.addField({"cfg_load_mon_cnt", 384 + 64, 56, "load monitor counter, counts clock cycles when cfg_load_mon_en is enabled"});
        // R.addField({"cfg3_rsvd1", 384 + 120, 8, "reserved"});

        // Register: cfg4_L (Address: 0x4440)
        R.addField({"cfg_instr_vld_cnt", 512 + 0, 56, "Count when there are instructions waiting to be issued in the instruction queue"});
        // R.addField({"cfg4_rsvd0", 512 + 56, 8, "reserved"});

        // Register: cfg4_H (Address: 0x4C48)
        R.addField({"cfg4_instr_iss_cnt", 512 + 64, 56, "Count when there are instructions being executed in the instruction queue"});
        // R.addField({"cfg4_rsvd1", 512 + 120, 8, "reserved"});

        // Register: cfg5_L (Address: 0x5450)
        // R.addField({"cfg5_rsvd0", 640 + 0, 64, "reserved"});

        // Register: cfg5_H (Address: 0x5C58)
        R.addField({"perf_aw_addr", 640 + 64, 32, "performance monitor wr addr"});
        // R.addField({"cfg5_rsvd1", 640 + 96, 8, "reserved"});
        // R.addField({"cfg_stopper_pause_done", 640 + 104, 1, "All transfers have ended and no new transfers will be initiated"});
        // R.addField({"cfg_stopper_enable", 640 + 105, 1, "Enable stopper"});
        // R.addField({"cfg_stopper_bypass", 640 + 106, 1, "Stopper in bypass mode"});
        // R.addField({"cfg5_rsvd2", 640 + 107, 21, "reserved"});

        // Register: cfg6_L (Address: 0x6460)
        // R.addField({"cfg6_rsvd0", 768 + 0, 64, "reserved"});

        // Register: cfg6_H (Address: 0x6C68)
        // R.addField({"cfg6_rsvd1", 768 + 64, 64, "reserved"});

        // Register: cfg7_L (Address: 0x7470)
        // R.addField({"cfg7_rsvd0", 896 + 0, 7, "reserved"});
        R.addField({"cfg_perf_start_addr", 896 + 7, 32, "cfg_result_start_addr, saves the start address for monitor output results, must be configured before setting cfg_monitor_en"});
        // R.addField({"cfg7_rsvd1", 896 + 39, 8, "reserved"});
        R.addField({"cfg_perf_end_addr", 896 + 47, 32, "cfg_result_end_addr, saves the end address for monitor output results, must be configured before setting cfg_monitor_en"});

        // Register: cfg7_H (Address: 0x7C78)
        // R.addField({"cfg7_rsvd2", 896 + 79, 8, "reserved"});
        // R.addField({"cfg_cmpt_en", 896 + 87, 1, "cfg_cmpt_en, computation enable"});
        // R.addField({"cfg_cmpt_val", 896 + 88, 16, "cfg_cmpt_val, computation data, configured before or simultaneously with cfg_cmpt_en"});
        // R.addField({"cfg_rd_instr_en", 896 + 104, 1, "cfg_rd_instr_en, read instruction enable"});
        // R.addField({"cfg_rd_instr_stall_en", 896 + 105, 1, "cfg_rd_instr_stall_en, read instruction stall enable"});
        // R.addField({"cfg_wr_instr_en", 896 + 106, 1, "cfg_wr_instr_en, write instruction enable"});
        // R.addField({"cfg7_rsvd3", 896 + 107, 1, "reserved"});
        R.addField({"cfg_eu_clk_gate_en", 896 + 108, 1, "When high, enables eu clk_gate function"});
        R.addField({"cfg_cube_clk_gate_en", 896 + 109, 1, "When high, enables cube clk_gate function"});
        R.addField({"cfg_lmem_clk_gate_en", 896 + 110, 1, "When high, enables lmem clk_gate function"});
        // R.addField({"cfg_eu_clk_buf", 896 + 111, 4, "eu clk delayed turn-off adjustment time"});
        // R.addField({"cfg_cube_clk_buf", 896 + 115, 4, "cube clk delayed turn-off adjustment time"});
        // R.addField({"cfg_lmem_clk_buf", 896 + 119, 4, "lmem clk delayed turn-off adjustment time"});
        // R.addField({"cfg7_rsvd4", 896 + 123, 5, "reserved"});

        // Register: cfg14_L (Address: 0xE4E0)
        R.addField({"cfg_intr_dbg_single_step_en", 1792 + 0, 1, "cfg_intr_dbg_single_step_en"});
        R.addField({"cfg_intr_dbg_breakpoint_en", 1792 + 1, 1, "cfg_intr_dbg_breakpoint_en"});
        R.addField({"cfg_intr_tsk_type_err_en", 1792 + 2, 1, "task type and task eu type inst_chk err en"});
        R.addField({"cfg_intr_depend_id_err_en", 1792 + 3, 1, "Interrupt enable for sync_id_gdma and tpu depend id difference too large (0x80000)"});
        // R.addField({"cfg14_rsvd0", 1792 + 4, 12, "reserved"});
        R.addField({"cfg_intr_dbg_mode_clr", 1792 + 16, 1, "Clear debug mode (single_step/breakpoint) interrupt"});
        // R.addField({"cfg14_rsvd1", 1792 + 17, 1, "reserved"});
        // R.addField({"cfg_intr_tsk_type_err_clr", 1792 + 18, 1, "task type and task eu type inst_chk err clr"});
        // R.addField({"cfg_intr_depend_id_clr", 1792 + 19, 1, "instruction parity err - imem des_buf_out_err clr"});
        // R.addField({"cfg14_rsvd2", 1792 + 20, 12, "reserved"});
        R.addField({"intr_dbg_single_step_stat", 1792 + 32, 1, "debug single step stat"});
        R.addField({"intr_dbg_breakpoint_stat", 1792 + 33, 1, "des dbg breakpoint stat"});
        R.addField({"intr_tsk_type_err_stat", 1792 + 34, 1, "task type and task eu type inst_chk err state"});
        R.addField({"intr_depend_id_stat", 1792 + 35, 1, "Interrupt status for sync_id_gdma and tpu depend id difference too large (0x80000)"});
        // R.addField({"cfg14_rsvd3", 1792 + 36, 28, "reserved"});

        // set expected values
        R.setFieldExpectedValue("cfg_enable", 1);
        R.setFieldExpectedValue("cfg_cmd_illegal", 0);
        R.setFieldExpectedValue("intr_tsk_type_err_stat", 0);
        R.setFieldExpectedValue("intr_depend_id_stat", 0);
        R.setFieldExpectedValue("cfg_des_clr_busy", 0);

        regs.push_back(R);
        return regs;
    }

    std::vector<RegDescriptor> createDMACmdRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().dma_cmd_base + core_id * getChipInfo().core_offset;
        RegDescriptor R("DMA_CMD", base);

        R.addField({"intr_en", 0, 1, "Interrupt Enable; if this bit is set, CPU COULD get interrupt when the instruction is finished. 0: disable interrupt; 1: enable interrupt"});
        R.addField({"stride_enable", 1, 1, "Stride enable. 0: No stride for all blob definition; 1: Enable stride for all blob definition"});
        R.addField({"nchw_copy", 2, 1, "NCHW copy bit. 0: use separate src and dst NCHW value setting; 1: reuse src NCHW value for dst NCHW value"});
        R.addField({"cmd_short", 3, 1, "0: 768-bit full command; 1: 128/256/384/512-bit short command"});
        // R.addField({"cache_en", 4, 1, "Read cache enable. 0: Read not use cache; 1: Read use cache"});
        // R.addField({"cache_flush", 5, 1, "Flush cache. 0: Not Flush cache; 1: Flush cache"});
        R.addField({"cmd_type", 32, 5, "0x0: GDMA_tensor; 0x1: GDMA_matrix; 0x3: GDMA_general; 0x6: GDMA_sys; 0x7: GDMA_gather; 0x8: GDMA_scatter"});
        R.addField({"cmd_special_function", 37, 3, "This field is connected with cmd_type. When cmd_type == DMA_tensor: 000: No special function; 001: Enable transposition write (For Neuron transposition case, the dest_N = src_C, dest_C = src_N); 011: broadcast (only support C size <= 64 and local memory mask invalid); Others: reserved"});
        R.addField({"fill_constant_en", 40, 1, "0: no fill constant; 1: fill constant enable (only supports special_func = 000/011/100)"});
        R.addField({"src_data_format", 41, 4, "Source Data Format. 0: INT8; 1: FP16; 2: FP32; 3: INT16; 4: INT32; 5: BFP16; 6: FP20; 7: FP8 E4M3; 8: FP8 E5M2; Others: not supported"});
        R.addField({"cmd_id_dep", 64, 20, "Execution of this descriptor needs to wait for engine[i]'s cmd_id which depends on the cmd_id_en to be bigger than the ID specified in this field. [19:0] is depend_id, [20] is depend_id_enable (0: disable; 1: enable)"});
        R.addField({"cmd_id_dep_enable", 84, 1, "When this bit is 1, cmd_id_dep is valid."});
        R.addField({"break_point", 87, 1, "When dbg_mode equals to 2'b10 and this bit is 1, the GDMA will stop executing the next descriptor after this descriptor is executed."});
        R.addField({"constant_value", 96, 32, "When cmd_special_function == fill constant, this field means constant value."});
        R.addField({"src_nstride", 128, 32, "Unsigned number; Source blob N stride"});
        R.addField({"src_cstride", 160, 32, "Unsigned number; Source blob C stride"});
        R.addField({"src_hstride", 192, 32, "Unsigned number; Source blob H stride"});
        R.addField({"src_wstride", 224, 32, "Unsigned number; Source blob W stride"});
        R.addField({"dst_nstride", 256, 32, "Unsigned number; Destination blob N stride"});
        R.addField({"dst_cstride", 288, 32, "Unsigned number; Destination blob C stride"});
        R.addField({"dst_hstride", 320, 32, "Unsigned number; Destination blob H stride"});
        R.addField({"dst_wstride", 352, 32, "Unsigned number; Destination blob W stride"});
        R.addField({"src_nsize", 384, 16, "Source Blob Number"});
        R.addField({"src_csize", 400, 16, "Source blob C"});
        R.addField({"src_hsize", 416, 16, "Source blob H"});
        R.addField({"src_wsize", 432, 16, "Source blob W"});
        R.addField({"dst_nsize", 448, 16, "Destination Blob Number"});
        R.addField({"dst_csize", 464, 16, "Destination blob C"});
        R.addField({"dst_hsize", 480, 16, "Destination blob H"});
        R.addField({"dst_wsize", 496, 16, "Destination blob W"});
        R.addField({"src_start_addr", 512, 45, "Source blob start address[44:0]"});
        R.addField({"dst_start_addr", 576, 45, "Destination blob start address[44:0]"});
        R.addField({"all_reduce_code", 640, 16, "All_reduce_code. all_reduce_code[3:0]: dtype; all_reduce_code[7:4]: opcode; all_reduce_code[11:8]: psum_op; all_reduce_code[14:12]: reserved; all_reduce_code[15]: all_reduce_enable"});
        R.addField({"localmem_mask", 704, 64, "Used to announce the local memory mask or not, 1 means enable access, 0 means disable access (bit 0 corresponds to local memory index 0)"});

        R.setFieldValidValues("cmd_type", {0, 1, 3, 6, 7, 8});
        R.setFieldValidValues("src_data_format", {0, 1, 2, 3, 4, 5});
        R.setFieldExpectedValue("localmem_mask", 0xffffffffffffffff);
        R.setFieldExpectedValue("src_wstride", 1);
        R.setFieldExpectedValue("dst_wstride", 1);
        R.setFieldInvalidValues("src_nstride", {0});
        R.setFieldInvalidValues("src_cstride", {0});
        R.setFieldInvalidValues("src_hstride", {0});
        R.setFieldInvalidValues("dst_nstride", {0});
        R.setFieldInvalidValues("dst_cstride", {0});
        R.setFieldInvalidValues("dst_hstride", {0});
        R.setFieldInvalidValues("src_nsize", {0});
        R.setFieldInvalidValues("src_csize", {0});
        R.setFieldInvalidValues("src_hsize", {0});
        R.setFieldInvalidValues("src_wsize", {0});
        R.setFieldInvalidValues("dst_nsize", {0});
        R.setFieldInvalidValues("dst_csize", {0});
        R.setFieldInvalidValues("dst_hsize", {0});
        R.setFieldInvalidValues("dst_wsize", {0});
        regs.push_back(R);
        return regs;
    }

    std::vector<RegDescriptor> createDMACtrlRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().dma_ctrl_base + core_id * getChipInfo().core_offset;
        RegDescriptor R("DMA_CTRL", base);

        // gdma_csr_0 (Index: 0)
        R.addField({"des_mode_enable", 0, 1, "0: gdma in PIO mode; 1: gdma in DES mode"});
        // R.addField({"sync_id_reset", 1, 1, "0: Do not reset the SyncID output; 1: Reset the SyncID output to be all 0s. This bit will be cleared immediately after write."});
        R.addField({"low_power_enable", 3, 1, "0: disable clk gate; 1: enable clk gate"});
        // R.addField({"des_clr", 4, 1, "Clear instruction buffer. Write 1'b1 to enable. Automatically cleared to 0."});
        // R.addField({"gif_bpn", 5, 1, "When 0, gif can be backpressured; when 1, gif cannot be backpressured"});
        R.addField({"cache_cg_en", 6, 1, "When 1, clk_gate tied to 1; when 0, clk_gate controlled by instruction"});
        R.addField({"mst_ins_buf_status", 8, 7, "Master thread, Current entry number of the Ins Buf. It means how many descriptors that SW can write to Ins Buf. If the Ins Buf is full, this field will be 0."});
        // R.addField({"thrd_mode", 15, 1, "SW sets the dual thread mode. 0: sdma dual thread roundrobin; 1: gdma dual thread fork/join/exit"});
        R.addField({"message_base_id", 16, 9, "message_id used for sync_wait and sync_send"});

        // gdma_csr_1 (Index: 1)
        R.addField({"cfg_des_addr", 32, 33, "des_addr where the commands are stored", commonAddrProcess});

        // gdma_csr_5 (Index: 5)
        R.addField({"perf_monitor_res_start_addr", 160, 33, "The DDR start address [38:7] for performance monitor is updated by this register's bit[31:0]. Bit[0] must be 0. Bits [6:0] are always 0.", commonAddrProcess});

        // gdma_csr_7 (Index: 7)
        R.addField({"perf_monitor_res_end_addr", 224, 33, "The DDR end address [39:7] for performance monitor is updated by this register's bit[31:0]. Bit[0] must be 0. Bits [6:0] are always 0.", commonAddrProcess});

        // gdma_csr_9 (Index: 9)
        R.addField({"current_cmd_id", 288, 32, "Current SyncID output of this engine. This field will automatically update to be consistent with the SyncID output of this engine for master thread."});

        // gdma_csr_10 (Index: 10)
        R.addField({"word_nums_after_filter", 320, 32, "Used in DMA_masked_select/DMA_nonzero command, this means the number of the data after filtering. A2:WTC(address decoded clear) -> 2260:rwc(write 1 to clear)"});

        // gdma_csr_11 (Index: 11)
        R.addField({"mem_size_per_npu", 352, 1, "For NCHW data, based on MEM size of a NPU, it can know the start address of next NPU. 0: 128KB; 1: 256KB"});
        R.addField({"eu_num_per_npu", 353, 1, "EU_NUM in each NPU (operate on int8). 0: 128; 1: 16"});
        R.addField({"npu_num", 354, 1, "NPU NUMBER. 0: 64; 1: 32"});

        // gdma_csr_12 (Index: 12)
        R.addField({"intr_status_disable", 384, 1, "Interrupt Disable, highest priority in interrupt generation. 0: Enable all interrupt; 1: Disable all interrupt. When this bit is cleared, the interrupt generation is decided by each individual interrupt enable bits."});
        R.addField({"intr_des_mode_end_disable", 385, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mst_inst_buf_empty_disable", 388, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable RD_DATA_FROM_TPU0_ERR interrupt generation"});
        R.addField({"intr_mst_inst_buf_full_disable", 390, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable WR_DATA_TO_DDR0_ERR interrupt generation"});
        R.addField({"intr_mst_singlestep_disable", 392, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable WR_DATA_TO_DDR1_ERR interrupt generation"});
        R.addField({"intr_mst_breakpoint_disable", 394, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable WR_DATA_TO_DDR0_ERR interrupt generation"});

        // gdma_csr_13 (Index: 13) to gdma_csr_44 (Index: 44) - Base Address Regions (32-bit)
        // Note: These are 32-bit registers, each containing a single 32-bit field.
        for (uint32_t i = 0; i < 32; ++i) {
            R.addField({"base_addr_region" + std::to_string(i), 32 * i + 416, 32, "Base addr region" + std::to_string(i) + ", for gmem/l2", baseAddrProcess});
        }

        // gdma_csr_45 (Index: 45)
        // R.addField({"data_awqos", 1440, 4, "Data access axi master awqos"});
        // R.addField({"data_arqos", 1444, 4, "Data access axi master arqos"});
        // R.addField({"pmu_awqos", 1448, 4, "PMU awqos"});
        // R.addField({"des_arqos", 1452, 4, "Descriptor arqos"});
        // R.addField({"compress_wqos", 1456, 6, "Bit[1:0] -> wdma_ctrl0 qos; bit[3:2] -> wdma_ctrl1; bit[5:4] -> wdma_ctrl2"});
        // R.addField({"decompress_rqos", 1462, 6, "Bit[1:0] -> rdma_ctrl0 qos; bit[3:2] -> rdma_ctrl1; bit[5:4] -> rdma_ctrl2"});
        // R.addField({"data_awqos_hw_en", 1468, 1, "data_awqos hardware mode enable. 0: disable; 1: enable"});
        // R.addField({"data_arqos_hw_en", 1469, 1, "data_arqos hardware mode enable. 0: disable; 1: enable"});
        // R.addField({"pmu_awqos_hw_en", 1470, 1, "pmu_awqos hardware mode enable. 0: disable; 1: enable"});
        // R.addField({"des_arqos_hw_en", 1471, 1, "des_arqos hardware mode enable. 0: disable; 1: enable"});

        // gdma_csr_46 (Index: 46)
        // R.addField({"cfg_load_mon_en", 1472, 1, "Enable loading monitor function"});
        // R.addField({"cfg_load_mon_clr", 1473, 1, "Synchronously clear load_mon_cnt, instr_vld_cnt, instr_iss_cnt. A2:WSC -> 2260:rws"});

        // gdma_csr_47 (Index: 47)
        // R.addField({"cfg_load_mon_cnt", 1504, 56, "Load monitor counter. When cfg_load_mon_en is enabled, it counts clock cycles."});

        // gdma_csr_49 (Index: 49)
        R.addField({"cfg_instr_vld_cnt", 1568, 56, "Count when instruction queue has pending instructions to be issued."});

        // gdma_csr_51 (Index: 51)
        R.addField({"cfg_instr_iss_cnt", 1632, 56, "Count when instruction queue has instructions being executed."});

        // Register: gdma_csr_53 (n=53, Base Address: 0xhd4)
        R.addField({"sys_pulse", 0 + 32*53, 1, "sys_pulse, shut down sys and drain up all os"});
        R.addField({"pulse_bypass", 1 + 32*53, 1, "bypass sys_pulse engine"});
        R.addField({"pulse_done", 2 + 32*53, 1, "sys_pulse done, software can reset gdma"});

        // Register: gdma_csr_54 (n=54, Base Address: 0xhd8)
        R.addField({"perf_monitor_wr_addr", 0 + 32*54, 33, "the ddr nxt wr address [39:7] for performance monitor."});

        // Register: gdma_csr_56 (n=56, Base Address: 0xhe0)
        // R.addField({"masked_select_slot_div", 0 + 32*56, 18, "GDMA_masked_select instruction slot memory. bit[6:0] means rdma_ctrl0; bit[13:7] means rdma_ctrl1."});

        // Register: gdma_csr_57 (n=57, Base Address: 0xhe4)
        // R.addField({"gather_slot_div", 0 + 32*57, 27, "GDMA_gather instruction slot memory. bit[7:0] means src ostd; bit[15:8] means idx ostd."});

        // Register: gdma_csr_58 (n=58, Base Address: 0xhe8)
        // R.addField({"scatter_slot_div", 0 + 32*58, 27, "GDMA_scatter instruction slot memory. bit[7:0] means src ostd; bit[15:8] means idx ostd; bit[23:16] means dst read ostd."});

        // Register: gdma_csr_59 (n=59, Base Address: 0xhec)
        // R.addField({"decompress_slot_div", 0 + 32*59, 27, "GDMA_decompress instruction slot memory. bit[7:0] means vlc ostd; bit[15:8] means map ostd; bit[23:16] means header ostd."});

        // Register: gdma_csr_60 (n=60, Base Address: 0xhf0)
        // R.addField({"cdma_vde_bypass", 0 + 32*60, 1, "used for cdma_vbd"});

        // Register: gdma_csr_61 (n=61, Base Address: 0xhf4)
        // R.addField({"randmask_seed_low", 0 + 32*61, 64, "used for randmask instruction"});

        // Register: gdma_csr_63 (n=63, Base Address: 0xhfc)
        // R.addField({"randmask_seed_enable", 0 + 32*63, 1, "used for randmask instruction"});

        // Register: gdma_csr_64 (n=64, Base Address: 0xh100)
        // R.addField({"mst_des_addr", 0 + 32*64, 33, "sample master_des_addr for apb read"});

        // Register: gdma_csr_68 (n=68, Base Address: 0xh110)
        R.addField({"dual_thrd_nxt_state", 0 + 32*68, 4, "the next state of dual thread FSM"});
        R.addField({"dual_thrd_mst_period", 4 + 32*68, 1, "indicate that the master thread is not over when the dual thread is opened"});
        R.addField({"mst_sync_id_block", 6 + 32*68, 1, "depend_id doesn't pass the syncID in master thread"});
        R.addField({"mst_sys_wait_period", 8 + 32*68, 1, "sys_wait cmd in master thread"});
        R.addField({"mst_des_fetch_cur_state", 11 + 32*68, 3, "fetching descriptor FSM when des mode in master thread"});
        R.addField({"mst_des_clear_cur_state", 17 + 32*68, 3, "clear descriptor FSM in master thread, which is triggered by des_clr"});
        R.addField({"mst_cmd_exec_cnt", 23 + 32*68, 3, "the number of descriptors which are executing in master thread"});
        R.addField({"dma_mst_thrd_state", 29 + 32*68, 1, "gdma/sdma master thread state: 0: idle; 1: active"});

        // Register: gdma_csr_69 (n=69, Base Address: 0xh114)
        R.addField({"mst_intr_sync_id", 0 + 32*69, 32, "moved from gdma_csr_11 3-26(24bit), expanded to 32 bit. The registered SyncID when the latest interrupt triggered for master thread. This field won’t update until next interrupt event happens."});

        // Register: gdma_csr_72 (n=72, Base Address: 0xh120)
        // R.addField({"des_hang_timer", 0 + 32*72, 32, "timer for gdma_des descriptor outputting with sys_ctrl, hang cycle counter"});

        // Register: gdma_csr_73 (n=73, Base Address: 0xh124)
        R.addField({"intr_error_disable", 0 + 32*73, 1, "Interrupt Disable, highest priority in interrupt generation. 0: Enable all interrupt; 1: Disable all interrupt. When this bit is cleared, the interrupt generation is decided by each individual interrupt enable bits."});
        R.addField({"intr_mst_invld_des_disable", 1 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mst_des_rd_err_disable", 3 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_dtn_data_rd_err_disable", 7 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_gif_data_rd_err_disable", 8 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_pmu_data_wr_err_disable", 11 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_dtn_data_wr_err_disable", 12 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_gif_data_wr_err_disable", 13 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_dma_abort_disable", 14 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_ip_hang_disable", 15 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_des_hang_disable", 16 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mst_depend_id_err_disable", 17 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_des_mode_set_err_disable", 19 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_thrd_cmd_cfg_err_disable", 20 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mst_parity_err_intrp_disable", 21 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mst_mpu_fetch_addr_err_disable", 23 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mst_mpu_inst_araddr_err_disable", 25 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mst_mpu_inst_awaddr_err_disable", 28 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});
        R.addField({"intr_mpu_pmu_awaddr_err_disable", 30 + 32*73, 1, "0: interrupt generation is enabled; 1: interrupt is masked. Disable INVALID_CMD_ERR interrupt generation"});

        // Register: gdma_csr_74 (n=74, Base Address: 0xh128)
        R.addField({"des_mode_end", 0 + 32*74, 1, "Write 1 clear. 0: No interrupt; 1: Interrupt triggered"});
        R.addField({"mst_cmd_done_status", 1 + 32*74, 1, "Write 1 clear. 0: No interrupt; 2: Interrupt triggered"});
        R.addField({"mst_inst_buf_empty", 3 + 32*74, 1, "Write 1 clear. 0: No interrupt; 4: Interrupt triggered"});
        R.addField({"mst_inst_buf_full", 5 + 32*74, 1, "Write 1 clear. 0: No interrupt; 6: Interrupt triggered"});
        R.addField({"mst_irq_singlestep", 7 + 32*74, 1, "Write 1 clear. 0: No interrupt; 8: Interrupt triggered"});
        R.addField({"mst_irq_breakpoint", 9 + 32*74, 1, "Write 1 clear. 0: No interrupt; 9: Interrupt triggered"});

        // Register: gdma_csr_75 (n=75, Base Address: 0xh12c)
        R.addField({"mst_invld_des", 0 + 32*75, 1, "Write 1 clear. 0: No interrupt; 8: Interrupt triggered"});
        R.addField({"mst_des_rd_err", 2 + 32*75, 1, "Write 1 clear. 0: No interrupt; 10: Interrupt triggered"});
        R.addField({"dtn_data_rd_err", 6 + 32*75, 1, "Write 1 clear. 0: No interrupt; 14: Interrupt triggered"});
        R.addField({"gif_data_rd_err", 7 + 32*75, 1, "Write 1 clear. 0: No interrupt; 15: Interrupt triggered"});
        R.addField({"pmu_data_wr_err", 10 + 32*75, 1, "Write 1 clear. 0: No interrupt; 18: Interrupt triggered"});
        R.addField({"dtn_data_wr_err", 11 + 32*75, 1, "Write 1 clear. 0: No interrupt; 19: Interrupt triggered"});
        R.addField({"gif_data_wr_err", 12 + 32*75, 1, "Write 1 clear. 0: No interrupt; 20: Interrupt triggered"});
        R.addField({"dma_abort", 13 + 32*75, 1, "Write 1 clear. 0: No interrupt; 21: Interrupt triggered"});
        R.addField({"ip_hang", 14 + 32*75, 1, "Write 1 clear. 0: No interrupt; 22: Interrupt triggered"});
        R.addField({"des_hang", 15 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        R.addField({"mst_depend_id_err", 16 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        R.addField({"des_mode_set_err", 18 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        // R.addField({"thrd_cmd_cfg_err", 19 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        R.addField({"mst_inst_parity_err_intrp", 20 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        R.addField({"mst_mpu_fetch_addr_err", 22 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        R.addField({"mst_mpu_inst_araddr_err", 24 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        R.addField({"mst_mpu_inst_awaddr_err", 27 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        R.addField({"mpu_pmu_awaddr_err", 29 + 32*75, 1, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});

        // Register: gdma_csr_76 (n=76, Base Address: 0xh130)
        // R.addField({"tensor_rd_mode", 0 + 32*76, 3, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});
        // R.addField({"tensor_wr_mode", 3 + 32*76, 3, "Write 1 clear. 0: No interrupt; 23: Interrupt triggered"});

        // Register: gdma_csr_77 (n=77, Base Address: 0xh134)
        // R.addField({"mst_inst_err_cnt", 0 + 32*77, 16, "master thread cmd buffer xor err interrupt count, write 1 to clear"});

        // Register: gdma_csr_78 (n=78, Base Address: 0xh138)
        // R.addField({"mst_parity_cmd_id", 0 + 32*78, 32, "record the latest master thread cmd_id which has passed the xor ecc, attention that the descriptor which the latest cmd_id corresponds to may not execute finished"});

        // Register: gdma_csr_80 (n=80, Base Address: 0xh140)
        R.addField({"mst_dbg_mode", 0 + 32*80, 2, "sw set the master thread debug mode"});

        // Register: gdma_csr_81 (n=81, Base Address: 0xh144)
        R.addField({"mpu_entry_enable", 0 + 32*81, 16, "each bit indicate the enable of 40-bit ddr region"});

        // memory protection unit
        for (uint32_t i = 0; i < 16; ++i) {
            R.addField({"mpu_start_addr_region" + std::to_string(i), 82 * 32 + 64*i, 40, "memory protection unit start address"});
            R.addField({"mpu_end_addr_region" + std::to_string(i), 114 * 32 + 64*i, 40, "memory protection unit end address"});
        }

        // Register: gdma_csr_146 (Address: 0x248)
        // Field: mst_current_id_pre
        R.addField({"mst_current_id_pre", 4672, 32, "The last non-sys instruction cmd_id being executed by the master thread"});

        // Register: gdma_csr_151 (Address: 0x25c)
        // Field: des_hang_timer_lm
        // R.addField({"des_hang_timer_lm", 4832, 32, "If des hangs and reaches this value, an interrupt is reported"});

        // Register: gdma_csr_152 (Address: 0x260)
        // Field: ip_hang_timer_lm
        // R.addField({"ip_hang_timer_lm", 4864, 32, "If ip hangs and reaches this value, an interrupt is reported"});


        // 设置期望值
        R.setFieldExpectedValue("mst_invld_des", 0);
        R.setFieldExpectedValue("mst_des_rd_err", 0);
        R.setFieldExpectedValue("dtn_data_rd_err", 0);
        R.setFieldExpectedValue("gif_data_rd_err", 0);
        R.setFieldExpectedValue("pmu_data_wr_err", 0);
        R.setFieldExpectedValue("dtn_data_wr_err", 0);
        R.setFieldExpectedValue("gif_data_wr_err", 0);
        R.setFieldExpectedValue("dma_abort", 0);
        R.setFieldExpectedValue("ip_hang", 0);
        R.setFieldExpectedValue("des_hang", 0);
        R.setFieldExpectedValue("mst_depend_id_err", 0);
        R.setFieldExpectedValue("des_mode_set_err", 0);
        R.setFieldExpectedValue("mst_mpu_fetch_addr_err", 0);
        R.setFieldExpectedValue("mst_mpu_inst_araddr_err", 0);
        R.setFieldExpectedValue("mst_mpu_inst_awaddr_err", 0);
        R.setFieldExpectedValue("mpu_pmu_awaddr_err", 0);

        regs.push_back(R);
        return regs;
    }
};

// CV184X 芯片类
class CV184XChip : public Chip {
public:
    explicit CV184XChip(std::shared_ptr<IRegisterReader> reader)
        : Chip(CV184XConfig().getChipInfo(), reader) {
        CV184XConfig config;
        initialize(config);
    }
    static const char* chipName() { return "CV184X"; }
};

} // namespace tpu_debugger
