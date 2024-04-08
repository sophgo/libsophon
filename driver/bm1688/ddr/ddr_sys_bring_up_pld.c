#include <ddr_sys_pld.h>
#include <ddr_init.h>
#include <bm_io.h>
#include <bm_api.h>
#include <bitwise_ops.h>

#define DO_BIST

void cfg_ddr_fab_reg(struct bm_device_info *bmdi)
{
#ifdef DDR4
	bm_write32(bmdi,DDR_TOP_CFG_BASE + 0x4, 0x00000002);// interleave size = 256
	bm_write32(bmdi,DDR_TOP_CFG_BASE + 0x8, 0x0fff07ff);
#else
	// top fabric
	bm_write32(bmdi,DDR_TOP_CFG_BASE + 0x4, 0x00000003);// interleave size = 512
	bm_write32(bmdi,DDR_TOP_CFG_BASE + 0x8, 0x0fff07ff);

	// sys1/2 fabric
	bm_write32(bmdi,DDR_TOP_SYS1_BASE + 0x30, 0x00000020);// interleave size = 256
	bm_write32(bmdi,DDR_TOP_SYS2_BASE + 0x30, 0x00000020);
	bm_write32(bmdi,DDR_TOP_SYS1_BASE + 0x34, 0x0fff07ff);
	bm_write32(bmdi,DDR_TOP_SYS2_BASE + 0x34, 0x0fff07ff);
#endif
}

void dram_init(struct bm_device_info *bmdi, 
				uint32_t ddr_top_base_addr, 
				uint32_t base_addr_ctrl, 
				uint32_t base_addr_phyd,
				uint32_t cv_base_addr_phyd, 
				uint32_t base_addr_bist, 
				uint32_t ddr_type, 
				uint32_t ddr_freq_mode)
{
	uint32_t  rddata;
	//uint8_t  dram_cap_in_mbyte;
	//uint32_t bist_result;
	//uint64_t err_data_even;
	//uint64_t err_data_odd;

	//pll_init
#ifdef REAL_LOCK
	cvx32_pll_init(cv_base_addr_phyd, ddr_freq_mode);
	//uvm_info(get_name(), "pll_init_h finish", UVM_NONE);
#endif

	//config interleave related registers
	cfg_ddr_fab_reg(bmdi);

	//ctrl_init
	cvx32_ctrl_init(bmdi, base_addr_ctrl, ddr_type);
	//uvm_info(get_name(), "ctrl_init_h finish", UVM_NONE);
	//NOTICE("ctrl_init_h finish.\n");

	ctrl_init_low_patch(bmdi, base_addr_ctrl, ddr_top_base_addr);

	//Release controler reset
	//uvm_info("release reset", "Release controler reset ", UVM_NONE);
	rddata = bm_read32(bmdi, ddr_top_base_addr + 0x20);
	if (ddr_type == LP4_2CH_1PHYD) {
		//rddata = {rddata[31:2], 2'b00};
		rddata = modified_bits_by_value(rddata, 0, 1, 0);
	} else {
		//rddata = {rddata[31:1], 1'b0};
		rddata = modified_bits_by_value(rddata, 0, 0, 0);
	}
	bm_write32(bmdi,ddr_top_base_addr + 0x020, rddata);


#ifdef REAL_PHY
	//phy_init
	cvx32_phy_init(base_addr_phyd, ddr_type);
#endif
	//uvm_info(get_name(), "svtb_phy_init_h finish", UVM_NONE);

	if ((ddr_type == DDR4_X8) || (ddr_type == DDR4_X16)) {
		//dfi_msb_sel =1
		rddata = bm_read32(bmdi, ddr_top_base_addr + 0x020);
		//rddata = {rddata[31:9], 1'b0,rddata[7:0] };
		rddata = modified_bits_by_value(rddata, 0, 8, 8);
		bm_write32(bmdi,ddr_top_base_addr + 0x020, rddata);
		//intlv_bypass  = 1
		rddata = bm_read32(bmdi, ddr_top_base_addr + 0x030);
		//rddata = {rddata[31:9], 1'b1,rddata[7:0] };
		rddata = modified_bits_by_value(rddata, 1, 8, 8);
		bm_write32(bmdi,ddr_top_base_addr + 0x030, rddata);
	}
	if (ddr_type == LP4_2CH_1PHYD) {
		//dfi_msb_sel =1
		rddata = bm_read32(bmdi, ddr_top_base_addr + 0x020);
		//rddata = {rddata[31:9], 1'b1,rddata[7:0] };
		rddata = modified_bits_by_value(rddata, 1, 8, 8);
		bm_write32(bmdi,ddr_top_base_addr + 0x020, rddata);
		//intlv_bypass  = 0
		rddata = bm_read32(bmdi, ddr_top_base_addr + 0x030);
		//rddata = {rddata[31:9], 1'b0,rddata[7:0] };
		rddata = modified_bits_by_value(rddata, 0, 8, 8);
		bm_write32(bmdi,ddr_top_base_addr + 0x030, rddata);
#ifdef REAL_PHY
		//param_phyd_lp4_2ch_en 8   8
		rddata = bm_read32(bmdi, 0x0050  + base_addr_phyd);
		//rddata = {rddata[31:9], 1'b1,rddata[7:0] };
		rddata = modified_bits_by_value(rddata, 1, 8, 8);
		bm_write32(bmdi,0x0050 + base_addr_phyd,  rddata);
#endif
	}

	//setting_check
	cvx32_setting_check(bmdi, base_addr_ctrl, base_addr_phyd);
	//uvm_info(get_name(), "cvx32_setting_check finish", UVM_NONE);

#ifdef REAL_PHY
	//pinmux
	cvx32_pinmux(bmdi, base_addr_phyd);
	//uvm_info(get_name(), "cvx32_pinmux finish", UVM_NONE);
#endif
	//set_dfi_init_start
	cvx32_set_dfi_init_start(bmdi, base_addr_ctrl);
	//uvm_info(get_name(), "cvx32_set_dfi_init_start finish", UVM_NONE);
	pr_info("cvx32_set_dfi_init_start finish.\n");

#ifdef REAL_PHY
	//ddr_phy_power_on_seq1
	cvx32_ddr_phy_power_on_seq1(cv_base_addr_phyd);
	//uvm_info(get_name(), "cvx32_ddr_phy_power_on_seq1 finish", UVM_NONE);

	//first dfi_init_start
	cvx32_polling_dfi_init_start(bmdi, base_addr_phyd);
	//uvm_info(get_name(), "first dfi_init_start finish", UVM_NONE);

	//INT_ISR_08
	cvx32_INT_ISR_08(base_addr_phyd, cv_base_addr_phyd, ddr_freq_mode);
	//uvm_info(get_name(), "isr_process_h finish", UVM_NONE);

	//ddr_phy_power_on_seq3
	cvx32_ddr_phy_power_on_seq3(base_addr_phyd);
	//uvm_info(get_name(), "cvx32_ddr_phy_power_on_seq3 finish", UVM_NONE);
#endif
	//uvm_info(get_name(), "cvx32_ddr_phy_power_on_seq3 finish", UVM_NONE);

	//wait_for_dfi_init_complete
	cvx32_wait_for_dfi_init_complete(bmdi, base_addr_ctrl);
	//uvm_info(get_name(), "cvx32_wait_for_dfi_init_complete finish", UVM_NONE);
	pr_info("cvx32_wait_for_dfi_init_complete finish.\n");

	//polling_synp_normal_mode
	cvx32_polling_synp_normal_mode(bmdi, base_addr_ctrl);
	//uvm_info(get_name(), "cvx32_polling_synp_normal_mode finish", UVM_NONE);
	pr_info("cvx32_polling_synp_normal_mode finish.\n");
#ifdef PLD_DDR_CFG
	ctrl_init_high_patch(bmdi, base_addr_ctrl, ddr_top_base_addr);
#endif
	
#if 0
	cvx32_bist_wr_sram_init(1, base_addr_bist, ddr_type, ddr_freq_mode);
	cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
	if (bist_result == 0) {
		//uvm_error(get_type_name(),
		//$psprintf("bist_wr_sram_1 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
		//bist_result, err_data_odd, err_data_even))
		NOTICE("Bist fail.\n");
	}
#endif
	//cvx32_clk_gating_disable();

	//ctrl_ch0_x32tox16
	//cvx32_ctrl_ch0_x32tox16();
	//$display("[KC_DBG] set ctrl ch0 to x16");
#if 0
	if ($test$plusargs("INT_DQS")) {
		for (int i = 0; i < 4; i++) {
			reg32_read(0x0204 + i*0x20 + base_addr_phyd,  rddata);
			rddata = modified_bits_by_value(rddata, 0, 10, 10);//param_phya_reg_rx_byte0_en_mask
			bm_write32(bmdi,0x0204 + i*0x20 + base_addr_phyd,  rddata);

			reg32_read(0x020c + i*0x20 + base_addr_phyd,  rddata);
			//param_phya_reg_rx_byte0_sel_dqs_wo_pream_mode
			rddata = modified_bits_by_value(rddata, 0, 15, 15);
			//param_phya_reg_rx_byte0_sel_internal_dqs
			rddata = modified_bits_by_value(rddata, 1, 16, 16);
			bm_write32(bmdi,0x020c + i*0x20 + base_addr_phyd,  rddata);

			reg32_read(0x0814 + i*0x60 + base_addr_phyd,  rddata);
			rddata = 0x003c0038;
			bm_write32(bmdi,0x0814 + i*0x60 + base_addr_phyd,  rddata);

			//f0_param_phyd_reg_tx_byte0_tx_dline_code_mask_ranka_sw
			//f0_param_phyd_rx_byte0_mask_shift
			//f0_param_phyd_reg_tx_byte0_tx_dline_code_int_dqs_sw
			//f0_param_phyd_tx_byte0_int_dqs_shift

			reg32_read(0x0818 + i*0x60 + base_addr_phyd,  rddata);
			rddata = 0x1c961b96;
			bm_write32(bmdi,0x0818 + i*0x60 + base_addr_phyd,  rddata);
		}
		cvx32_dll_sw_clr(base_addr_ctrl, base_addr_phyd);
		//uvm_info(get_name(), "INT_DQS", UVM_NONE);
		ctrl_init_detect_dram_size(base_addr_ctrl, base_addr_bist, ddr_type, dram_cap_in_mbyte);
		cvx32_bist_wr_prbs_init(base_addr_bist, 3, ddr_type);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
			$psprintf("bist_wr_prbs_3 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}
	}

	ctrl_init_high_patch(base_addr_ctrl, ddr_top_base_addr);

	cvx32_clk_gating_enable(cv_base_addr_phyd, base_addr_ctrl);
	//uvm_info(get_name(), "cvx32_clk_gating_enable finish", UVM_NONE);

	//`test_stream = "RANK 0 only";

	if ($test$plusargs("ENABLE_BIST")) {
		cvx32_bist_wr_sram_init(1, base_addr_bist, ddr_type, ddr_freq_mode);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
				$psprintf("bist_wr_sram_1 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}

		cvx32_bist_wr_prbs_init(base_addr_bist, 1, ddr_type);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
				$psprintf("bist_wr_prbs_1 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}

		//`test_stream = "RANK 1 only";

		cvx32_bist_wr_sram_init(2, base_addr_bist, ddr_type, ddr_freq_mode);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
				$psprintf("bist_wr_sram_2 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}

		cvx32_bist_wr_prbs_init(base_addr_bist, 2, ddr_type);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
				$psprintf("bist_wr_prbs_2 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}

		//`test_stream = "RANK 01 only";

		cvx32_bist_wr_sram_init(3, base_addr_bist, ddr_type, ddr_freq_mode);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
				$psprintf("bist_wr_sram_3 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}

		cvx32_bist_wr_prbs_init(base_addr_bist, 3, ddr_type);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
				$psprintf("bist_wr_prbs_3 bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}

		if ($test$plusargs("BIST_SSO")) {
		ctrl_init_low_patch(base_addr_ctrl, ddr_top_base_addr);
		//cvx32_bist_wdqlvl_init(1, DDR_SSO_PERIOD, 3);

		cvx32_bist_rdlvl_init(1, ddr_freq_mode, 3, base_addr_bist, base_addr_phyd, ddr_type);
		cvx32_bist_start_check(base_addr_bist, bist_result, err_data_odd, err_data_even, ddr_type);
		if (bist_result == 0) {
			`uvm_error(get_type_name(),
				$psprintf("bist_rdlvl bist_fail::bist_result = %8h,	err_data_odd = %8h, err_data_even = %8h",
				bist_result, err_data_odd, err_data_even))
		}
		ctrl_init_high_patch(base_addr_ctrl, ddr_top_base_addr);
		}
	}
	//     cvx32_dll_sw_clr();


	//    ctrl_init_h.ctrl_detect_dram_size=1;
	//    `uvm_s}(ctrl_init_h);
		ctrl_init_detect_dram_size(base_addr_ctrl, base_addr_bist, ddr_type, dram_cap_in_mbyte);
	//ctrl_init_h.dram_cap_in_mbyte = dram_cap_in_mbyte;
	//ctrl_init_h.ctrl_update_by_dram_size=1;
	//`uvm_s}(ctrl_init_h);
	//cvx32_dram_cap_check(dram_cap_in_mbyte);
	//uvm_info(get_name(), $psprintf("ctrl_init_detect_dram_size dram_cap_in_mbyte = %h",
				//dram_cap_in_mbyte), UVM_NONE);

#endif
}

void pld_ddr_sys_bring_up(struct bm_device_info *bmdi)
{
#if	defined(DDR4_X8)
	dram_init(bmdi, DDR_TOP_SYS1_BASE, DDR_CTRL_SYS1_BASE, DDR_PHYD2_CH0_SYS1_BASE,
		DDR_PHYD_CH0_SYS1_BASE, DDR_BIST_CH0_SYS1_BASE, DDR4_X8, MEM_FREQ_3200);
	dram_init(bmdi, DDR_TOP_SYS2_BASE, DDR_CTRL_SYS2_BASE, DDR_PHYD2_CH0_SYS2_BASE,
		DDR_PHYD_CH0_SYS2_BASE, DDR_BIST_CH0_SYS2_BASE, DDR4_X8, MEM_FREQ_3200);
#elif defined(DDR4_X16)
	dram_init(bmdi, DDR_TOP_SYS1_BASE, DDR_CTRL_SYS1_BASE, DDR_PHYD2_CH0_SYS1_BASE,
		DDR_PHYD_CH0_SYS1_BASE, DDR_BIST_CH0_SYS1_BASE, DDR4_X16, MEM_FREQ_3200);
	dram_init(bmdi, DDR_TOP_SYS2_BASE, DDR_CTRL_SYS2_BASE, DDR_PHYD2_CH0_SYS2_BASE,
		DDR_PHYD_CH0_SYS2_BASE, DDR_BIST_CH0_SYS2_BASE, DDR4_X16, MEM_FREQ_3200);
#else
	dram_init(bmdi, DDR_TOP_SYS1_BASE, DDR_CTRL_SYS1_BASE, DDR_PHYD2_CH0_SYS1_BASE,
		DDR_PHYD_CH0_SYS1_BASE, DDR_BIST_CH0_SYS1_BASE, LP4_2CH_1PHYD, MEM_FREQ_4266);
	dram_init(bmdi, DDR_TOP_SYS2_BASE, DDR_CTRL_SYS2_BASE, DDR_PHYD2_CH0_SYS2_BASE,
		DDR_PHYD_CH0_SYS2_BASE, DDR_BIST_CH0_SYS2_BASE, LP4_2CH_1PHYD, MEM_FREQ_4266);
#endif
	pr_info("******THE DDR SUBSYSTEM intialization has been completed!!******\n");
}
