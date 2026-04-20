#pragma once

#include "chip.hpp"

namespace tpu_debugger {

// BM1688 chip configuration - register definitions are inlined directly in code
class BM1688Config : public IChipConfig {
public:
    ChipInfo getChipInfo() const override {
        return {
            "BM1688",         // name
            2,                // num_cores
            0x26000000ull,    // tiu_cmd_base
            0x26000100ull,    // tiu_ctrl_base
            0x26020000ull,    // dma_cmd_base
            0x26020100ull,    // dma_ctrl_base
            0x10000ull        // core_offset (64KB per core)
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
    static uint64_t baseAddrProcess(uint64_t value) { return value << 5; }
    // TIU command register definitions - configured directly in code
    std::vector<RegDescriptor> createTIUCmdRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().tiu_cmd_base + core_id * getChipInfo().core_offset;
        RegDescriptor tiu_cmd("TIU_CMD", base);

        // Field definition: name, start bit, width, description
        // Command Descriptor Fields
        tiu_cmd.addField({"des_cmd_short", 0, 1, "0: Long Des, 1: Short Des"});
        tiu_cmd.addField({"des_cmd_id_dep", 17, 24, "Dependency CMD ID, wait for des_cmd_id_en specified external eng ID completion before executing this descriptor"});
        tiu_cmd.addField({"des_tsk_typ", 41, 4, "Instruction task type: 0: convolution, 1: depthwise or pooling, 2: matrix multiply && matrix multiply2, 3: arithmetic && SEG, 4: RQ && DQ, 5: TRANS && BC, 6: scatter_gather && scatter_gather_line, 7: linear_arithmetic, 8: not support, 9: special_function, 10: fused_linear, 11: SYS_TR_WR, 12: not support, 13: fused_cmpare, 14: vector correlation, 15: system"});
        tiu_cmd.addField({"des_tsk_eu_typ", 45, 5, "Instruction operation type, specified according to instruction type"});
        tiu_cmd.addField({"des_opt_rq", 50, 1, "Whether to quantize the result"});
        tiu_cmd.addField({"des_tsk_opd_num", 51, 2, "Number of input operands: 0: 4 operands, 1: 1 operand, 2: 2 operands, 3: 3 operands"});
        tiu_cmd.addField({"des_pad_mode", 53, 2, "If instruction type is CONV: padding mode; otherwise reserved0"});
        tiu_cmd.addField({"des_opt_res0_sign", 55, 1, "When result is INT type, indicates whether result is signed or unsigned: 0: unsigned, 1: signed"});
        // tiu_cmd.addField({"des_pwr_step", 59, 4, "power step"});
        tiu_cmd.addField({"des_intr_en", 63, 1, "0: instruction completion does not generate intr, 1: instruction completion generates intr"});
        tiu_cmd.addField({"des_opt_res_add", 64, 1, "Whether result accumulates with original content at res0_addr"});
        tiu_cmd.addField({"des_opt_relu", 65, 1, "reserved0"});
        tiu_cmd.addField({"des_opt_left_tran", 66, 1, "Whether to transpose, only used in matrix multiply"});
        tiu_cmd.addField({"des_opt_opd4_const", 67, 1, "Whether opd4 is constant"});
        tiu_cmd.addField({"des_opt_kernel_rotate", 68, 1, "Whether kernel is transposed"});
        tiu_cmd.addField({"des_opt_opd0_sign", 69, 1, "When opd0 data type is INT8, whether opd0 is signed: 0: unsigned, 1: signed"});
        tiu_cmd.addField({"des_opt_opd1_sign", 70, 1, "When opd1 data type is INT8, whether opd1 is signed: 0: unsigned, 1: signed"});
        tiu_cmd.addField({"des_opt_opd2_sign", 71, 1, "When opd2 data type is INT8, whether opd2 is signed: 0: unsigned, 1: signed"});
        tiu_cmd.addField({"des_opt_res0_prec", 72, 3, "res0 data type: 0: INT8, 1: FP16, 2: FP32, 3: INT16, 4: INT32, 5: BFP16, 6: INT64, others: not support"});
        tiu_cmd.addField({"des_opt_opd0_prec", 75, 3, "opd0 data type: 0: INT8, 1: FP16, 2: FP32, 3: INT16, 4: INT32, 5: BFP16, others: not support"});
        tiu_cmd.addField({"des_opt_opd1_prec", 78, 3, "opd1 data type: 0: INT8, 1: FP16, 2: FP32, 3: INT16, 4: INT32, 5: BFP16, others: not support"});
        tiu_cmd.addField({"des_opt_opd2_prec", 81, 3, "opd2 data type: 0: INT8, 1: FP16, 2: FP32, 3: INT16, 4: INT32, 5: BFP16, others: not support"});
        tiu_cmd.addField({"des_opt_opd0_const", 84, 1, "Whether opd0 is constant, if constant, its value is in des_opd0_addr: 0: not constant, 1: constant"});
        tiu_cmd.addField({"des_opt_opd1_const", 85, 1, "Whether opd1 is constant, if constant, its value is in des_opd1_addr: 0: not constant, 1: constant"});
        tiu_cmd.addField({"des_opt_opd2_const", 86, 1, "Whether opd2 is constant, if constant, its value is in des_opd1_addr: 0: not constant, 1: constant"});
        tiu_cmd.addField({"des_short_res0_str", 87, 3, "res0 data storage format: 0: aligned (W_STRIDE=1, H_STRIDE=W, C_STRIDE=EU_NUM*roundup(H*W/EU_NUM), N_STRIDE=C_STRIDE*roundup(C+C_START/LANE_NUM)), 1: compact (W_STRIDE=1, H_STRIDE=W, C_STRIDE=W*H, N_STRIDE=roundup(C+C_START/LANE_NUM)*C_STRIDE), 2: bias (N_STRIDE=H_STRIDE=W_STRIDE=0, C_STRIDE=1), 3: tensor, see specific configuration, others: not support"});
        tiu_cmd.addField({"des_short_opd0_str", 90, 3, "opd0 data storage format: 0: aligned, 1: compact, 2: bias, 3: tensor, others: not support"});
        tiu_cmd.addField({"des_short_opd1_str", 93, 3, "opd1 data storage format: 0: aligned, 1: compact, 2: bias, 3: tensor, others: not support"});
        tiu_cmd.addField({"des_short_opd2_str", 96, 3, "opd2 data storage format: 0: aligned, 1: compact, 2: bias, 3: tensor, others: not support"});
        tiu_cmd.addField({"des_opt_res_add_sign", 99, 1, "int8 operation add_result, whether original result is signed or unsigned: 0: unsigned, 1: signed"});
        tiu_cmd.addField({"des_sym_range", 125, 1, "conv/mm2 result quantization, whether to perform symmetric saturation: 0: no symmetric saturation, 1: symmetric saturation"});
        tiu_cmd.addField({"des_opt_opd3_const", 126, 1, "Whether opd3 is constant, if constant, its value is in des_opd3_addr: 0: not constant, 1: constant"});
        tiu_cmd.addField({"des_opt_opd5_const", 127, 1, "conv/mm2 result quantization, whether quantization parameters are constant: 0: not constant, 1: constant"});
        tiu_cmd.addField({"des_opd0_x_ins0", 128, 4, "Number of zeros inserted after each element in opd0 row direction, valid 0-14"});
        tiu_cmd.addField({"des_opd0_y_ins0", 132, 4, "Number of zeros inserted after each element in opd0 column direction, valid 0-14"});
        tiu_cmd.addField({"des_opd1_x_ins0", 136, 4, "Number of zeros inserted after each element in opd1 row direction, valid 0-14"});
        tiu_cmd.addField({"des_opd1_y_ins0", 140, 4, "Number of zeros inserted after each element in opd1 column direction, valid 0-14"});
        tiu_cmd.addField({"des_opd0_up_pad", 144, 4, "opd0 top padding rows, valid 0-15"});
        tiu_cmd.addField({"des_opd0_dn_pad", 148, 4, "opd0 bottom padding rows, valid 0-15"});
        tiu_cmd.addField({"des_opd0_lf_pad", 152, 4, "opd0 left padding columns, valid 0-15"});
        tiu_cmd.addField({"des_opd0_rt_pad", 156, 4, "opd0 right padding columns, valid 0-15"});
        tiu_cmd.addField({"des_res_op_x_str", 160, 4, "convolution/pooling horizontal stride, valid 1-15"});
        tiu_cmd.addField({"des_res_op_y_str", 164, 4, "convolution/pooling vertical stride, valid 1-15"});
        tiu_cmd.addField({"des_res0_h_shift", 168, 4, "res0 h right shift value, only used in tensor arithmetic"});
        tiu_cmd.addField({"des_res0_w_shift", 172, 4, "res0 w right shift value, only used in tensor arithmetic"});
        tiu_cmd.addField({"des_opd0_h_shift", 176, 4, "opd0 h right shift value, only used in tensor arithmetic"});
        tiu_cmd.addField({"des_opd0_w_shift", 180, 4, "opd0 w right shift value, only used in tensor arithmetic"});
        tiu_cmd.addField({"des_opd1_h_shift", 184, 4, "opd1 h right shift value, only used in tensor arithmetic"});
        tiu_cmd.addField({"des_opd1_w_shift", 188, 4, "opd1 w right shift value, only used in tensor arithmetic"});
        tiu_cmd.addField({"des_tsk_lane_num", 192, 64, "lane mask: 0: corresponding lane will not be written, 1: corresponding lane can be written"});
        tiu_cmd.addField({"des_res0_n", 256, 16, "res0 n, 0 invalid. matrix multiply: res0 matrix row count, mdsum: opd0 n, res0 n is 1"});
        tiu_cmd.addField({"des_res0_c", 272, 16, "res0 c, 0 invalid"});
        tiu_cmd.addField({"des_res0_h", 288, 16, "res0 h, 0 invalid. In conv, depthwise conv and tensor arithmetic, res0 h max value is 2047(2048?)"});
        tiu_cmd.addField({"des_res0_w", 304, 16, "res0 w, 0 invalid. In conv, depthwise conv and tensor arithmetic, res0 w max value is 2047(2048?)"});
        tiu_cmd.addField({"des_opd0_n", 320, 16, "opd0 n, 0 invalid"});
        tiu_cmd.addField({"des_opd0_c", 336, 16, "opd0 c, 0 invalid"});
        tiu_cmd.addField({"des_opd0_h", 352, 16, "opd0 h, 0 invalid. In conv, depthwise conv and tensor arithmetic, opd0 h max value is 2047(2048?)"});
        tiu_cmd.addField({"des_opd0_w", 368, 16, "opd0 w, 0 invalid. In conv, depthwise conv and tensor arithmetic, opd0 w max value is 2047(2048?)"});
        tiu_cmd.addField({"des_opd1_n", 384, 16, "opd1 n, 0 invalid"});
        tiu_cmd.addField({"des_opd1_c", 400, 16, "opd1 c, 0 invalid"});
        tiu_cmd.addField({"des_opd1_h", 416, 16, "opd1 h, 0 invalid. In conv, depthwise conv and tensor arithmetic, opd1 h max value is 2047(2048?)"});
        tiu_cmd.addField({"des_opd1_w", 432, 16, "opd1 w, 0 invalid. In conv, depthwise conv and tensor arithmetic, opd1 w max value is 2047(2048?)"});
        tiu_cmd.addField({"des_res0_n_str", 448, 16, "res0 n stride"});
        tiu_cmd.addField({"des_res0_c_str", 464, 16, "res0 c stride"});
        tiu_cmd.addField({"des_opd0_n_str", 480, 16, "opd0 n stride"});
        tiu_cmd.addField({"des_opd0_c_str", 496, 16, "opd0 c stride"});
        tiu_cmd.addField({"des_opd1_n_str", 512, 16, "opd1 n stride"});
        tiu_cmd.addField({"des_opd1_c_str", 528, 16, "opd1 c stride"});
        tiu_cmd.addField({"des_opd2_n_str", 544, 16, "opd2 n stride"});
        tiu_cmd.addField({"des_opd2_c_str", 560, 16, "opd2 c stride"});
        tiu_cmd.addField({"des_res0_addr", 576, 32, "res0 start address"});
        tiu_cmd.addField({"des_opd0_addr", 608, 32, "If des_opt_opd0_const is 0: opd0 start address, if des_opt_opd0_const is 1: opd0 constant"});
        tiu_cmd.addField({"des_opd1_addr", 640, 32, "If des_opt_opd1_const is 0: opd1 start address, if des_opt_opd1_const is 1: opd1 constant"});
        tiu_cmd.addField({"des_opd2_addr", 672, 32, "If des_opt_opd2_const is 0: opd2 start address, if des_opt_opd2_const is 1: opd2 constant"});
        tiu_cmd.addField({"des_res0_h_str", 704, 32, "res0 h stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_res0_w_str", 736, 32, "res0 w stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_opd0_h_str", 768, 32, "opd0 h stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_opd0_w_str", 800, 32, "opd0 w stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_opd1_h_str", 832, 32, "opd1 h stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_opd1_w_str", 864, 32, "opd1 w stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_opd2_h_str", 896, 32, "opd2 h stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_opd2_w_str", 928, 32, "opd2 w stride, instructions other than tensor arithmetic only use lower 19 bits"});
        tiu_cmd.addField({"des_res1_addr", 960, 32, "res1 start address"});
        tiu_cmd.addField({"des_opd3_addr", 992, 32, "If des_opt_opd3_const is 0: opd3 start address, if des_opt_opd3_const is 1: opd3 constant"});

        // Set valid values (optional)
        tiu_cmd.setFieldInvalidValues("des_tsk_typ", {8, 12});
        tiu_cmd.setFieldValidValues("des_opt_res0_prec", {0, 1, 2, 3, 4, 5, 6});
        tiu_cmd.setFieldValidValues("des_opt_opd0_prec", {0, 1, 2, 3, 4, 5});
        tiu_cmd.setFieldValidValues("des_short_opd0_str", {0, 3});
        regs.push_back(tiu_cmd);
        return regs;
    }

    // TIU control register definitions
    std::vector<RegDescriptor> createTIUCtrlRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().tiu_ctrl_base + core_id * getChipInfo().core_offset;
        RegDescriptor tiu_ctrl("TIU_CTRL", base);

        // cfg0 register (width: 128 bits)
        tiu_ctrl.addField({"cfg_enable", 0, 1, "TPU enable"});
        tiu_ctrl.addField({"cfg_intr_stat", 1, 1, "TIU interrupt Status"});
        tiu_ctrl.addField({"cfg_curr_cmdid", 2, 24, "Last executed command id"});
        tiu_ctrl.addField({"cfg_instr_barrier", 26, 1, "Instruction barrier"});
        tiu_ctrl.addField({"cfg_data_barrier", 27, 1, "Data barrier"});
        tiu_ctrl.addField({"cfg_fb_pins_en", 28, 1, "pad insv read buffer"});
        tiu_ctrl.addField({"cfg_fb_pxl_en", 29, 1, "pxl read buffer"});
        tiu_ctrl.addField({"cfg_intr_en", 30, 1, "TIU interrupt global enable"});
        tiu_ctrl.addField({"cfg_cmdbuf_intr_en", 31, 1, "des buf interrupt enable"});
        tiu_ctrl.addField({"cfg_cmdbuf_avail", 32, 12, "des buf remaining space, cfg_cmdbuf_avail*128Byte, max 128*128Byte"});
        tiu_ctrl.addField({"cfg_cmdbuf_intr_biten", 44, 4, "Enable cmdbuf interrupt"});
        tiu_ctrl.addField({"cfg_cmdbuf_intr_stat", 48, 4, "cmdbuf interrupt status"});
        tiu_ctrl.addField({"cfg_dma_ban_overlap", 52, 1, "dma"});
        tiu_ctrl.addField({"cfg_dma_space_margin", 53, 1, "dma bus, read fifo space control to reduce outstanding count"});
        tiu_ctrl.addField({"cfg_des_mode", 54, 1, "des mode"});
        tiu_ctrl.addField({"cfg_des_clr_busy", 55, 1, "Clear instructions in des buf"});
        tiu_ctrl.addField({"cfg_fb_only_cmm2", 56, 1, "Enable pxl/pins buffer only during conv mm2 pord"});
        tiu_ctrl.addField({"cfg_fb_conv_dis", 57, 1, "Disable pxl/pins buffer under conv instruction"});
        tiu_ctrl.addField({"cfg_fb_mm2_dis", 58, 1, "Disable pxl/pins buffer under mm2 instruction"});
        tiu_ctrl.addField({"cfg_fb_mm2_nt_dis", 59, 1, "Do not open pxl/pins buffer under mm2_nt instruction"});
        tiu_ctrl.addField({"cfg_fb_pord_dis", 60, 1, "Do not open pxl/pins buffer under pord instruction"});
        // tiu_ctrl.addField({"cfg_rst_cmdid", 63, 1, "Auto clear to 0"});
        tiu_ctrl.addField({"cfg_des_addr_vld", 64, 1, "Enable descriptor mode, write 1'b1 to enable, auto clear to 0"});
        tiu_ctrl.addField({"cfg_des_clr", 65, 1, "Write 1 to clear des buf"});
        tiu_ctrl.addField({"cfg_des_addr", 66, 30, "Descriptor address. Descriptor ends by setting interrupt via sys end instruction.", commonAddrProcess});
        tiu_ctrl.addField({"cfg_des_resp_err", 126, 1, "DES mode, AX interface slave err or decode err, set by hardware, cleared by software"});
        tiu_ctrl.addField({"cfg_cmd_illegal", 127, 1, "Illegal instruction, set by hardware, cleared by software"});

        // cfg1 register (width: 128 bits)
        // tiu_ctrl.addField({"cfg_des_arqos", 128, 4, "DES port ARQoS"});
        // tiu_ctrl.addField({"cfg_des_awqos", 132, 4, "DES port AWQoS"});
        // tiu_ctrl.addField({"cfg1_rsvd0", 136, 120, "reserved"});

        // cfg2 register (width: 128 bits)
        // tiu_ctrl.addField({"cfg2_rsvd0", 256, 128, "reserved"});

        // cfg3 register (width: 128 bits)
        // tiu_ctrl.addField({"cfg_load_mon_en", 384, 1, "Enable loading monitor function"});
        // tiu_ctrl.addField({"cfg_load_mon_clr", 385, 1, "Synchronously clear load_mon_cnt, instr_vld_cnt, instr_iss_cnt, write 1 then hardware auto clears to 0"});
        // tiu_ctrl.addField({"cfg_load_mon_cnt", 448, 56, "load mon counter, counts clocks when cfg_load_mon_en is enabled"});

        // cfg4 register (width: 128 bits)
        tiu_ctrl.addField({"cfg_instr_vld_cnt", 512, 56, "Count when there are instructions waiting to be issued in instruction queue"});
        tiu_ctrl.addField({"cfg4_instr_iss_cnt", 576, 56, "Count when there are instructions being executed in instruction queue"});

        // cfg5 register (width: 128 bits)
        tiu_ctrl.addField({"cfg_tpu_slave", 640, 1, "In combined mode, 0: slave tpu; 1: master tpu"});
        tiu_ctrl.addField({"cfg_base_msgid", 672, 7, "base message ID"});
        tiu_ctrl.addField({"perf_aw_addr", 704, 40, "performance monitor write address"});
        tiu_ctrl.addField({"cfg_stopper_pause_done", 744, 1, "All transfers have ended and no new transfers will be initiated"});
        tiu_ctrl.addField({"cfg_stopper_enable", 745, 1, "Enable stopper"});
        tiu_ctrl.addField({"cfg_stopper_bypass", 746, 1, "Stopper in bypass mode"});

        // cfg7 register (width: 128 bits)
        tiu_ctrl.addField({"cfg_monitor_en", 912, 1, "tpu performance monitor enable signal"});
        tiu_ctrl.addField({"cfg_perf_start_addr", 913, 35, "start address for saving monitor output results, must be configured before setting cfg_monitor_en"});
        tiu_ctrl.addField({"cfg_perf_end_addr", 948, 35, "end address for saving monitor output results, must be configured before setting cfg_monitor_en"});
        // tiu_ctrl.addField({"cfg_cmpt_en", 983, 1, "computation enable"});
        // tiu_ctrl.addField({"cfg_cmpt_val", 984, 16, "computation data, configure before or simultaneously with cfg_cmpt_en"});
        tiu_ctrl.addField({"cfg_rd_instr_en", 1000, 1, "read instruction enable"});
        tiu_ctrl.addField({"cfg_rd_instr_stall_en", 1001, 1, "read instruction stall enable"});
        tiu_ctrl.addField({"cfg_wr_instr_en", 1002, 1, "write instruction enable"});
        tiu_ctrl.addField({"cfg_clk_gate_en", 1003, 1, "When high, enables clock gating during idle time between instructions and DMA clock gating"});
        tiu_ctrl.addField({"cfg_eu_clk_gate_en", 1004, 1, "When high, enables clk_gate function for eu"});
        tiu_ctrl.addField({"cfg_cube_clk_gate_en", 1005, 1, "When high, enables clk_gate function for cube"});
        tiu_ctrl.addField({"cfg_lmem_clk_gate_en", 1006, 1, "When high, enables clk_gate function for lmem"});
        // tiu_ctrl.addField({"cfg_eu_clk_buf", 1007, 4, "eu clk delayed shutdown adjustment time"});
        // tiu_ctrl.addField({"cfg_cube_clk_buf", 1011, 4, "cube clk delayed shutdown adjustment time"});
        // tiu_ctrl.addField({"cfg_lmem_clk_buf", 1015, 4, "lmem clk delayed shutdown adjustment time"});


        // Set expected values (for anomaly detection)
        tiu_ctrl.setFieldExpectedValue("cfg_enable", 1);           // tpu must be enabled
        tiu_ctrl.setFieldExpectedValue("cfg_cmd_illegal", 0);  // no illegal cmd
        tiu_ctrl.setFieldExpectedValue("cfg_des_clr_busy", 0);  // des buf clear should not be busy

        regs.push_back(tiu_ctrl);
        return regs;
    }

    // DMA command register definitions
    std::vector<RegDescriptor> createDMACmdRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().dma_cmd_base + core_id * getChipInfo().core_offset;
        RegDescriptor dma_cmd("DMA_CMD", base);

        dma_cmd.addField({"intr_en", 0, 1, "Interrupt Enable;if this bit is set, CPU COULD get interrupt when the instruction is finished ; 0: disable interrupt; 1: enable interrupt"});
        dma_cmd.addField({"stride_enable", 1, 1, "Stride enable; 0：No stride for all blob definition; 1：Enable stride for all blob definition"});
        dma_cmd.addField({"nchw_copy", 2, 1, "NCHW copy bit; 0: use separate src and dst NCHW value setting; 1: reuse src NCHW value for dst NCHW value"});
        dma_cmd.addField({"cmd_short", 3, 1, "0:768bit full cmd;; 1:128/256/384/512 short cmd"});
        dma_cmd.addField({"cmd_type", 32, 4, "0x0:DMA_tensor; 0x1:DMA_matrix; 0x2:DMA_masked_select; 0x3:DMA_general; 0x4:DMA_cw_transpose; 0x5:DMA_nonzero; 0x6:DMA_sys; 0x7:DMA_gather; ; 0x8:DMA_scatter; 0x9:DMA_reverse; 0xa:DMA_compress; 0xb:DMA_decompress"});
        dma_cmd.addField({"cmd_special_function", 36, 3, "this field is connected with cmd_type. when cmd_type == DMA_tensor; 0000: No special function; 0001: Enable transposition write (For Neuron transposition case, the dest_N= src_C, dest_C=src_N); 0010：collect; 0011: broadcast：only support C size<=64 and lmem mask invalid; 0100: distribute : lmem mask invalid; 0101: lmem 4 bank copy（only support lmem）; 0110: lmem 4 bank broadcast : only support C size <=64 and lmem mask invalid; others: reserved"});
        dma_cmd.addField({"fill_constant_en", 39, 1, "0: no fill constant; 1: fill constant enable; only support special_func=000/011/001"});
        dma_cmd.addField({"src_data_format", 40, 3, "Source Data Format; 0:INT8; 1:FP16; 2:FP32; 3:INT16; 4:INT32; 5:BFP16; 6:INT4; others：not support"});
        dma_cmd.addField({"cmd_id_dep", 64, 24, "Execution of this descriptor need to wait for engine[i]'s cmd_id  which depends on the cmd_id_en to be bigger than the ID specified in this field"});
        dma_cmd.addField({"constant_value", 96, 32, "when cmd_special_function==fill constant, this field means constant value"});
        dma_cmd.addField({"src_nstride", 128, 32, "unsigned number; Source blob N stride"});
        dma_cmd.addField({"src_cstride", 160, 32, "unsigned number. Source blob C stride"});
        dma_cmd.addField({"src_hstride", 192, 32, "unsigned number; Source blob H stride"});
        dma_cmd.addField({"src_wstride", 224, 32, "unsigned number; Source blob W stride"});
        dma_cmd.addField({"dst_nstride", 256, 32, "unsigned number; desitination blob N stride"});
        dma_cmd.addField({"dst_cstride", 288, 32, "unsigned number.desitination blob C stride"});
        dma_cmd.addField({"dst_hstride", 320, 32, "unsigned number; desitination blob H stride"});
        dma_cmd.addField({"dst_wstride", 352, 32, "unsigned number; desitination blob W stride"});
        dma_cmd.addField({"src_nsize", 384, 16, "Source Blob Number"});
        dma_cmd.addField({"src_csize", 400, 16, "Source blob C"});
        dma_cmd.addField({"src_hsize", 416, 16, "Source blob H"});
        dma_cmd.addField({"src_wsize", 432, 16, "Source blob W"});
        dma_cmd.addField({"dst_nsize", 448, 16, "Destination Blob Number"});
        dma_cmd.addField({"dst_csize", 464, 16, "Destination blob C"});
        dma_cmd.addField({"dst_hsize", 480, 16, "Destination blob H"});
        dma_cmd.addField({"dst_wsize", 496, 16, "Destination blob W"});
        dma_cmd.addField({"src_start_addr", 512, 40, "source blob start address[39:0]"});
        dma_cmd.addField({"dst_start_addr", 576, 40, "destination blob start address[39:0]"});
        dma_cmd.addField({"localmem_mask", 704, 64, "used to announce is the local memory mask or not, 1 means enable access, 0 means disable access (bit 0 corresponds local memory index 0)"});

        dma_cmd.setFieldExpectedValue("localmem_mask", 0xFFFFFFFFFFFFFFFF);
        dma_cmd.setFieldInvalidValues("cmd_type", {12, 13, 14, 15});
        dma_cmd.setFieldInvalidValues("src_nstride", {0});
        dma_cmd.setFieldInvalidValues("src_cstride", {0});
        dma_cmd.setFieldInvalidValues("src_hstride", {0});
        dma_cmd.setFieldInvalidValues("src_wstride", {0});
        dma_cmd.setFieldInvalidValues("dst_nstride", {0});
        dma_cmd.setFieldInvalidValues("dst_cstride", {0});
        dma_cmd.setFieldInvalidValues("dst_hstride", {0});
        dma_cmd.setFieldInvalidValues("dst_wstride", {0});
        dma_cmd.setFieldInvalidValues("src_nsize", {0});
        dma_cmd.setFieldInvalidValues("src_csize", {0});
        dma_cmd.setFieldInvalidValues("src_hsize", {0});
        dma_cmd.setFieldInvalidValues("src_wsize", {0});
        dma_cmd.setFieldInvalidValues("dst_nsize", {0});
        dma_cmd.setFieldInvalidValues("dst_csize", {0});
        dma_cmd.setFieldInvalidValues("dst_hsize", {0});
        dma_cmd.setFieldInvalidValues("dst_wsize", {0});
        regs.push_back(dma_cmd);
        return regs;
    }

    // DMA control register definitions
    std::vector<RegDescriptor> createDMACtrlRegs(uint32_t core_id) const {
        std::vector<RegDescriptor> regs;

        uint64_t base = getChipInfo().dma_ctrl_base + core_id * getChipInfo().core_offset;
        RegDescriptor dma_ctrl("DMA_CTRL", base);

        dma_ctrl.addField({"des_mode_enable", 0, 1, "0: gdma in PIO mode; 1: gdma in DES mode"});
        // dma_ctrl.addField({"sync_id_reset", 1, 1, "0: Do not reset the SyncID output; 1: Reset the SyncID output to be all 0s; This bit will be cleared immediately after write"});
        dma_ctrl.addField({"perf_monitor_enable", 2, 1, "0: disable performance monitor; 1: enable performance monitor"});
        dma_ctrl.addField({"low_power_enable", 3, 1, "0:disable clk gate; 1:enable clk gate"});
        // dma_ctrl.addField({"des_clr", 4, 1, "Clear instruction buffer; Write 1'b1 to enable; Auto clear to 0"});
        // dma_ctrl.addField({"gif_bpn", 5, 1, "0: gif can be back-pressured; 1: gif cannot be back-pressured"});
        // dma_ctrl.addField({"cache_cg_en", 6, 1, "1: clk_gate tied to 1; 0: clk_gate controlled by instruction"});
        dma_ctrl.addField({"ins_buf_status", 8, 7, "Current entry number of the Ins Buf. It means how many descriptors that SW can write to Ins Buf. If the Ins Buf is full, this field will be 0."});
        dma_ctrl.addField({"message_base_id", 16, 8, "message_id used for sync_wait and sync_send"});
        dma_ctrl.addField({"cfg_des_addr", 32, 28, "Descriptor Address Pointer [34:7] is updated by this register's bit. Descriptor address's bit [6:0] are always 0.", commonAddrProcess});
        dma_ctrl.addField({"perf_monitor_res_start_addr", 64, 28, "The DDR start address [34:7] for performance monitor is updated by this register's bit[27:0]. bit[0] must be 0; the bit [6:0] is always 0.", commonAddrProcess});
        dma_ctrl.addField({"perf_monitor_res_end_addr", 96, 28, "The DDR end address [34:7] for performance monitor is updated by this register's bit[27:0]. bit[0] must be 0; the bit [6:0] is always 0.", commonAddrProcess});
        dma_ctrl.addField({"current_cmd_id", 128, 24, "Current SyncID output of this engine. This field will automatically update to be consistent with the SyncID output of this engine."});
        dma_ctrl.addField({"word_nums_after_filter", 160, 32, "Used in DMA_masked_select/DMA_nonzero command, this means the number of the data after filtering."});
        dma_ctrl.addField({"mem_size_per_npu", 192, 1, "For NCHW data, based on MEM size of a NPU, it can know the start address of next NPU; 0: 128KB; 1: 256KB"});
        dma_ctrl.addField({"eu_num_per_npu", 193, 1, "EU_NUM in each NPU (operate on int8); 0: 64;"});
        dma_ctrl.addField({"npu_num", 194, 1, "NPU NUMBER; 0: 64; 1: 32"});
        dma_ctrl.addField({"intr_sync_id", 195, 24, "The registered SyncID when the latest interrupt triggered. This field won't update until next interrupt event happens."});
        dma_ctrl.addField({"intr_disable", 224, 1, "Interrupt Disable, highest priority in interrupt generation; 0: Enable all interrupt; 1: Disable all interrupt; When this bit is cleared, the interrupt generation is decided by each individual interrupt enable bits."});
        dma_ctrl.addField({"intr_invalid_cmd_disable", 225, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable INVALID_CMD_ERR interrupt generation"});
        dma_ctrl.addField({"intr_ddr1_data_rd_err_disable", 226, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable RD_DATA_FROM_DDR1_ERR interrupt generation"});
        dma_ctrl.addField({"intr_ddr0_data_rd_err_disable", 227, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable RD_DATA_FROM_DDR0_ERR interrupt generation"});
        dma_ctrl.addField({"intr_tpu0_rd_err_disable", 228, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable RD_DATA_FROM_TPU0_ERR interrupt generation"});
        dma_ctrl.addField({"intr_ddr1_data_wr_err_disable", 230, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable WR_DATA_TO_DDR1_ERR interrupt generation"});
        dma_ctrl.addField({"intr_ddr0_data_wr_err_disable", 231, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable WR_DATA_TO_DDR0_ERR interrupt generation"});
        dma_ctrl.addField({"intr_tpu0_wr_err_disable", 232, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable WR_DATA_TO_TPU0_ERR interrupt generation"});
        dma_ctrl.addField({"intr_des_mode_end_disable", 234, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable DES_MODE_END interrupt generation"});
        dma_ctrl.addField({"intr_ins_buf_empty_disable", 235, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable INS_BUF_EMPTY interrupt generation"});
        dma_ctrl.addField({"intr_ins_buf_one_forth_full_disable", 236, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable INS_BUF_ONE_FORTH_FULL interrupt generation"});
        dma_ctrl.addField({"des_data_rd_err_disable", 237, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable INS_BUF_TWO_FORTH_FULL interrupt generation"});
        dma_ctrl.addField({"intr_ins_buf_three_forth_full_disable", 238, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable INS_BUF_THREE_FORTH_FULL interrupt generation"});
        dma_ctrl.addField({"intr_ins_buf_full_disable", 239, 1, "0: interrupt generation is enabled; 1: interrupt is masked; Disable INS_BUF_FULL interrupt generation"});
        dma_ctrl.addField({"cmd_done_status", 240, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"invalid_cmd_err", 241, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"ddr1_data_rd_err", 242, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"ddr0_data_rd_err", 243, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"tpu0_rd_err", 244, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"ddr1_data_wr_err", 246, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"ddr0_data_wr_err", 247, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"tpu0_wr_err", 248, 1, "Write 1 clear; 0: No interrupt; 1: Interrupt triggered"});
        dma_ctrl.addField({"des_mode_end", 250, 1, "Write 1 clear; 0: No interrupt; 2: Interrupt triggered"});
        dma_ctrl.addField({"ins_buf_empty", 251, 1, "Write 1 clear; 0: No interrupt; 3: Interrupt triggered"});
        dma_ctrl.addField({"ins_buf_one_forth_full", 252, 1, "Write 1 clear; 0: No interrupt; 4: Interrupt triggered"});
        dma_ctrl.addField({"des_data_rd_err", 253, 1, "Write 1 clear; 0: No interrupt; 5: Interrupt triggered"});
        dma_ctrl.addField({"ins_buf_three_forth_full", 254, 1, "Write 1 clear; 0: No interrupt; 6: Interrupt triggered"});
        dma_ctrl.addField({"ins_buf_full", 255, 1, "Write 1 clear; 0: No interrupt; 7: Interrupt triggered"});
        for (int i = 0; i < 8; i++) {
            dma_ctrl.addField({"base_addr_regine" + std::to_string(i), 256u + 32 * i, 32, "base addr regine" + std::to_string(i), baseAddrProcess});
        }
        // dma_ctrl.addField({"des_awqos", 512, 4, "interface des_awqos"});
        // dma_ctrl.addField({"des_arqos", 516, 4, "interface des_arqos"});
        // dma_ctrl.addField({"ddr0_awqos", 520, 4, "interface ddr0_awqos"});
        // dma_ctrl.addField({"ddr0_arqos", 524, 4, "interface ddr0_arqos"});
        // dma_ctrl.addField({"ddr1_awqos", 528, 4, "interface ddr1_awqos"});
        // dma_ctrl.addField({"ddr1_arqos", 532, 4, "interface ddr1_arqos"});
        // dma_ctrl.addField({"cfg_load_mon_en", 544, 1, "Enable loading monitor function"});
        // dma_ctrl.addField({"cfg_load_mon_clr", 545, 1, "Synchronously clear load_mon_cnt, instr_vld_cnt, instr_iss_cnt to 0"});
        // dma_ctrl.addField({"cfg_load_mon_cnt", 576, 54, "Load monitor counter, counts clock cycles when cfg_load_mon_en is enabled"});
        // dma_ctrl.addField({"cfg_instr_vld_cnt", 640, 54, "Count when there are instructions pending to be issued in the instruction queue"});
        // dma_ctrl.addField({"cfg_instr_iss_cnt", 704, 54, "Count when there are instructions being executed in the instruction queue"});
        // dma_ctrl.addField({"sys_pulse", 768, 1, "sys_pulse, shut down sys and drain up all os"});
        // dma_ctrl.addField({"pulse_bypass", 776, 4, "bypass sys_pulse engine"});
        // dma_ctrl.addField({"pulse_done", 780, 4, "sys_pulse done, software can reset gdma; [0]:m_des_axi; [1]:m_ddr_axi_0; [2]:m_ddr_axi_1; [3]:m_lmem_gif"});
        // dma_ctrl.addField({"axi_filter_bypass", 784, 2, "axi read interface filter bypass; [0]:m_ddr_axi_0; [1]:m_ddr_axi_1"});
        // dma_ctrl.addField({"perf_monitor_wr_addr", 800, 28, "The DDR next write address [34:7] for performance monitor."});

        // Set expected values (for anomaly detection)
        dma_ctrl.setFieldExpectedValue("dma_err", 0);

        // Set valid values
        //dma_ctrl.setFieldValidValues("",{});
        //dma_ctrl.setFieldValidValues("",{});
        regs.push_back(dma_ctrl);
        return regs;
    }
};

// CV186X
class CV186XConfig : public BM1688Config {
public:
    ChipInfo getChipInfo() const override {
        ChipInfo info = BM1688Config::getChipInfo();
        info.name = "CV186X";
        info.num_cores = 1;
        info.core_offset = 0x0ull;
        return info;
    }
};

// BM1688 chip
class BM1688Chip : public Chip {
public:
    explicit BM1688Chip(std::shared_ptr<IRegisterReader> reader)
        : Chip(BM1688Config().getChipInfo(), reader) {
        BM1688Config config;
        initialize(config);
    }
    static const char* chipName() { return "BM1688"; }
};

// CV186X
class CV186XChip : public Chip {
public:
    explicit CV186XChip(std::shared_ptr<IRegisterReader> reader)
        : Chip(CV186XConfig().getChipInfo(), reader) {
        CV186XConfig config;
        initialize(config);
    }
    static const char* chipName() { return "CV186X"; }
};

} // namespace tpu_debugger
