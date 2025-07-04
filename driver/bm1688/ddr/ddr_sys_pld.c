#include <bm_io.h>
#include <ddr_sys_pld.h>
#include <ddr_init.h>
#include <bitwise_ops.h>
#include <linux/delay.h>
//#include <delay_timer.h>

//#define opdelay(_x) udelay((_x)/1000)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
// #pragma GCC diagnostic ignored "-Wunused-variable"


void cvx32_ctrl_init(struct bm_device_info *bmdi, uint32_t base_addr_ctrl, int ddr_type)
{
	ddrc_init(bmdi, base_addr_ctrl, ddr_type);
}

void ctrl_init_high_patch(struct bm_device_info *bmdi, uint32_t base_addr_ctrl, 
						  uint32_t ddr_top_base_addr)
{
	//{
	// enable auto PD/SR
	bm_write32(bmdi, base_addr_ctrl + 0x30, 0x0000010b);

	// enable auto ctrl_upd
	bm_write32(bmdi, base_addr_ctrl + 0x1a0, 0x00400018);

	// change xpi to multi DDR burst
	// in_order_accept = 0
	bm_write32(bmdi, base_addr_ctrl + 0xc, 0x13784310);
	//bm_write32(bmdi, base + 0x44, 0x08000000);
	//bm_write32(bmdi, base + 0x6c, 0x41000003);
	bm_write32(bmdi, base_addr_ctrl + 0x44, 0x08000000);  //from Harrison 0909
	bm_write32(bmdi, base_addr_ctrl + 0x6c, 0x41000000);  //from Harrson 0909

	// enable clock gating
	bm_write32(bmdi, ddr_top_base_addr + 0x14, 0x00000000);
	//}
}

void ctrl_init_low_patch(struct bm_device_info *bmdi, uint32_t base_addr_ctrl, uint32_t ddr_top_base_addr)
{
	//{
	// disable auto PD/SR
	bm_write32(bmdi, base_addr_ctrl + 0x30, 0x00000100);

	// disable auto ctrl_upd
	bm_write32(bmdi, base_addr_ctrl + 0x1a0, 0xC0400018);

	// change xpi to single DDR burst
	// in_order_accept = 1
	//bm_write32(bmdi, base + 0xc, 0x93784310);
	//bm_write32(bmdi, base + 0x44, 0x14000000);
	//bm_write32(bmdi, base + 0x6c, 0x40000003);
	bm_write32(bmdi, base_addr_ctrl + 0xc, 0x93744311);  //from Harrison 0909
	//bm_write32(bmdi, base_addr_ctrl + 0x44, 0x14000000);  //from Harrison 0909
	//bm_write32(bmdi, base_addr_ctrl + 0x6c, 0x40000000);  //from Harrison 0909

	// disable clock gating
	bm_write32(bmdi, ddr_top_base_addr + 0x14, 0x00000fff);
	//}
}
void  cvx32_ctrlupd_short (struct bm_device_info *bmdi, uint32_t base_addr_ctrl)
{
	uint32_t rddata;
	// ctrlupd short
	//`test_stream = "-2a- cvx32ctrlupd_short";
	// for gate track
	// dis_auto_ctrlupd [31] =0, dis_auto_ctrlupd_srx [30] =0, ctrlupd_pre_srx [29] =1 @ 0x1a0
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x000001a0);
	//original        rddata[31:29] = 3'b001;
	rddata = modified_bits_by_value(rddata,  1, 31, 29);//
	bm_write32(bmdi, base_addr_ctrl + 0x000001a0, rddata);
	// dfi_t_ctrlupd_interval_min_x1024 [23:16] = 4, dfi_t_ctrlupd_interval_max_x1024 [7:0] = 8 @ 0x1a4
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x000001a4);
	//original        rddata[23:16] = 80x04;
	rddata = modified_bits_by_value(rddata,  4, 23, 16);//
	//original        rddata[7:0] = 80x08;
	rddata = modified_bits_by_value(rddata,  8, 7, 0);//
	bm_write32(bmdi, base_addr_ctrl + 0x000001a4, rddata);
}

void ctrl_init_update_by_dram_size(struct bm_device_info *bmdi, uint32_t base_addr_ctrl, uint8_t dram_cap_in_mr8)
{
	//{
	switch (dram_cap_in_mr8) {
	case 0:
		bm_write32(bmdi, base_addr_ctrl + 0x64, 0x82080040);
		bm_write32(bmdi, base_addr_ctrl + 0x68, 0x00400000);
		bm_write32(bmdi, base_addr_ctrl + 0x138, 0x00000093);
	break;
	case 1:
	case 2:
		bm_write32(bmdi, base_addr_ctrl + 0x64, 0x82080060);
		bm_write32(bmdi, base_addr_ctrl + 0x68, 0x00600000);
		bm_write32(bmdi, base_addr_ctrl + 0x138, 0x000000C8);
	break;
	case 3:
	case 4:
		bm_write32(bmdi, base_addr_ctrl + 0x64, 0x82080096);
		bm_write32(bmdi, base_addr_ctrl + 0x68, 0x00600000);
		bm_write32(bmdi, base_addr_ctrl + 0x138, 0x00000133);
	break;
	case 5:
	case 6:
		bm_write32(bmdi, base_addr_ctrl + 0x64, 0x820800CB);
		bm_write32(bmdi, base_addr_ctrl + 0x68, 0x00600000);
		bm_write32(bmdi, base_addr_ctrl + 0x138, 0x0000019E);
	break;
	}
	//}case

	// toggle refresh_update_level
	bm_write32(bmdi, base_addr_ctrl + 0x60, 0x00000002);
	bm_write32(bmdi, base_addr_ctrl + 0x60, 0x00000000);
}

void cvx32_phy_init(struct bm_device_info *bmdi, uint32_t base_addr_phyd, int ddr_type)
{
	//if (ddr_type==LP4_2CH_1PHYD){
	//  `include "phy_setting/regconfig.CV_PHY_4266_cl40_bl16"
	//}
	//if ((ddr_type==DDR4_X16)||(ddr_type==DDR4_X8)){
	//  `include "phy_setting/regconfig.CV_PHY_1600_cl28_bl8"
	//}

	//`include "phy_setting/cv_phy_reg.sv"
	//KC_MSG(get_name(), "phy_init finish", UVM_NONE);
//	phy_init(base_addr_phyd, ddr_type);
}

void cvx32_setting_check(struct bm_device_info *bmdi, uint32_t base_addr_ctrl, uint32_t base_addr_phyd)
{
	uint32_t dfi_tphy_wrlat;
	uint32_t dfi_tphy_wrdata;
	uint32_t dfi_t_rddata_en;
	uint32_t dfi_t_ctrl_delay;
	uint32_t dfi_t_wrdata_delay;
#ifdef REAL_PHY
	uint32_t phy_reg_version;
#endif
	uint32_t rddata;

	//  `test_stream = "-0a- cvx32_setting_check";

#ifdef REAL_PHY
	phy_reg_version = bm_read32(bmdi, 0x3000 + base_addr_phyd);
#endif

	rddata = bm_read32(bmdi, base_addr_ctrl + 0x190);
	dfi_tphy_wrlat     = get_bits_from_value(rddata, 5, 0);//DFITMG0.dfi_tphy_wrlat
	dfi_tphy_wrdata    = get_bits_from_value(rddata, 13, 8);//DFITMG0.dfi_tphy_wrdata
	dfi_t_rddata_en    = get_bits_from_value(rddata, 22, 16);  //DFITMG0.dfi_t_rddata_en
	dfi_t_ctrl_delay   = get_bits_from_value(rddata, 29, 24);  //DFITMG0.dfi_t_ctrl_delay
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x194);
	dfi_t_wrdata_delay = get_bits_from_value(rddata, 20, 16);  //DFITMG1.dfi_t_wrdata_delay

	if (dfi_t_ctrl_delay != 0x4) {
		//$display("ERR !!! dfi_t_ctrl_delay not 0x4");
		pr_err("ERR !!! dfi_t_ctrl_delay not 0x4.\n");
	}
}

void cvx32_set_dfi_init_start(struct bm_device_info *bmdi, uint32_t base_addr_ctrl)
{
	uint32_t rddata;
	// synp setting
	// phy is ready for initial dfi_init_start request
	// set umctl2 to tigger dfi_init_start
	//`test_stream = "-0d- cvx32_set_dfi_init_start";
	bm_write32(bmdi, base_addr_ctrl + 0x00000320, 0x00000000);
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x000001b0); //dfi_init_start @ rddata[5];
	rddata = modified_bits_by_value(rddata, 1, 5, 5);//
	//rddata={rddata[31:6], 1'b1, rddata[4:0]};//
	bm_write32(bmdi, base_addr_ctrl + 0x000001b0, rddata);
	bm_write32(bmdi, base_addr_ctrl + 0x00000320, 1);
	//KC_MSG("set_dfi_init_start", " dfi_init_start finish", UVM_LOW);
}
void cvx32_ddr_phy_power_on_seq1(struct bm_device_info *bmdi, uint32_t cv_base_addr_phyd)
{
	uint32_t rddata;
	//power_seq_1
	//`test_stream = "-0e- cvx32_ddr_phy_power_on_seq1";
	//RESETZ/CKE PD=0
	rddata = bm_read32(bmdi, 0x64 + cv_base_addr_phyd);
	//TOP_REG_TX_CA_PD_CKE0/CKE1
	rddata = modified_bits_by_value(rddata, 0, 1, 0);
	rddata = modified_bits_by_value(rddata, 0, 1, 0);
	//TOP_REG_TX_CA_PD_RESETZ
	rddata = modified_bits_by_value(rddata, 0, 16, 16);
	//TOP_REG_CA_PD_ALL
	rddata = modified_bits_by_value(rddata, 0, 24, 24);
	//rddata = {rddata[31:25], 1'b0,rddata[23:17], 1'b0,rddata[15:2],2'b00};
	bm_write32(bmdi, 0x64 + cv_base_addr_phyd,  rddata);
	//KC_MSG("ddr_phy_power_on_seq1", " RESET PD", UVM_LOW);

	//CA PD=0
	//All PHYA CA PD=0
	rddata = bm_read32(bmdi, 0x60 + cv_base_addr_phyd);
	//rddata = modified_bits_by_value(rddata, 0, 31, 0);
	rddata = 0x0;
	bm_write32(bmdi, 0x60 + cv_base_addr_phyd,  rddata);
	//KC_MSG("ddr_phy_power_on_seq1", "All PHYA CA PD=0", UVM_LOW);

	//TOP_REG_TX_SEL_BYTE0123_GPIO = 1 (DQ)
	//TOP_REG_TX_BYTE0_SEL_GPIO    24    24
	//TOP_REG_TX_BYTE1_SEL_GPIO    25    25
	//TOP_REG_TX_BYTE2_SEL_GPIO    26    26
	//TOP_REG_TX_BYTE3_SEL_GPIO    27    27
	rddata = bm_read32(bmdi, 0x1C + cv_base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 15, 27, 24);
	//rddata = {rddata[31:28], 40xf,rddata[23:0]};
	bm_write32(bmdi, 0x1C + cv_base_addr_phyd,  rddata);
	//KC_MSG("ddr_phy_power_on_seq1", "TOP_REG_TX_BYTE0123_SEL_GPIO = 1", UVM_LOW);

	//DQ PD=0
	//TOP_REG_BYTE0_PD    0    0
	//TOP_REG_BYTE1_PD    1    1
	//TOP_REG_BYTE2_PD    2    2
	//TOP_REG_BYTE3_PD    3    3
	rddata = 0x00000000;
	bm_write32(bmdi, 0x00 + cv_base_addr_phyd,  rddata);
	//KC_MSG("ddr_phy_power_on_seq1", "TX_BYTE PD=0 ...", UVM_LOW);

	//TOP_REG_TX_SEL_BYTE0123_GPIO = 0 (DQ)
	rddata = bm_read32(bmdi, 0x1c + cv_base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 0, 27, 24);
	//rddata = {rddata[31:28], 40x0,rddata[23:0]};
	bm_write32(bmdi, 0x1c + cv_base_addr_phyd,  rddata);
	//KC_MSG("ddr_phy_power_on_seq1", "TOP_REG_TX_BYTE0123_SEL_GPIO = 0", UVM_LOW);
	// power_seq_1
}
void cvx32_polling_dfi_init_start(struct bm_device_info *bmdi, uint32_t base_addr_phyd)
{
	uint32_t rddata;
	//`test_stream = "-11- cvx32_polling_dfi_init_start";
	while (1) {
		// param_phyd_to_reg_phy_int
		rddata = bm_read32(bmdi, 0x3028 + base_addr_phyd);
		//(~param_phyd_phy_int_mask[08] & int_ps_dfi_init_start_phy_0to1)
		if ((get_bits_from_value(rddata, 8, 8) == 1)) {
			break;
		}
	}
}

void cvx32_INT_ISR_08(struct bm_device_info *bmdi, uint32_t base_addr_phyd, uint32_t cv_base_addr_phyd, uint32_t ddr_freq_mode)
{
	uint32_t EN_PLL_SPEED_CHG;
	uint32_t CUR_PLL_SPEED;
	uint32_t NEXT_PLL_SPEED;
	uint32_t lpddr4_calvl_done;
	uint32_t rddata;
	//`test_stream ="-1c- cvx32_INT_ISR_08";
	//KC_MSG("cvx32_INT_ISR_08", "Emulate PHY IRQ: init_start interrupt", UVM_LOW);
	//param_phyd_clkctrl_init_complete   <= int_regin[0];
	rddata = 0x00000000;
	bm_write32(bmdi, 0x0118 + base_addr_phyd,  rddata);
	//----------------------------------------------------
	//Set LPDDR4 delay value when CA training is done
	//param_phyd_reg_lpddr4_calvl_done    0     0
	rddata = bm_read32(bmdi, 0x00D0 + base_addr_phyd);
	lpddr4_calvl_done = get_bits_from_value(rddata, 0, 0);
	//----------------------------------------------------
	//TOP_REG_EN_PLL_SPEED_CHG    0    0
	//TOP_REG_CUR_PLL_SPEED        5    4
	//TOP_REG_NEXT_PLL_SPEED    9    8
	rddata = bm_read32(bmdi, 0x4C+cv_base_addr_phyd);
	EN_PLL_SPEED_CHG = get_bits_from_value(rddata, 0, 0);
	CUR_PLL_SPEED    = get_bits_from_value(rddata, 5, 4);
	NEXT_PLL_SPEED   = get_bits_from_value(rddata, 9, 8);
	//KC_MSG(get_name(), $sformatf("CUR_PLL_SPEED = %h, NEXT_PLL_SPEED = %h, EN_PLL_SPEED_CHG=%h ",
	//		CUR_PLL_SPEED, NEXT_PLL_SPEED, EN_PLL_SPEED_CHG), UVM_DEBUG);

	//----------------------------------------------------
	if ((lpddr4_calvl_done != 0) && (CUR_PLL_SPEED == 0)) {//
		//Do SW update
		//KC_MSG("cvx32_INT_ISR_08", "Do SW update ...", UVM_LOW);
		rddata = (0x00CC + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 1, 6, 6);   //param_phyd_calvl_sw_upd
		//rddata = {rddata[31:7], 10x1,rddata[5:0]};
		bm_write32(bmdi, 0x00CC + base_addr_phyd,  rddata);
		//Wait for SW update ACK
		rddata = 0x00000000;
		while (get_bits_from_value(rddata, 0, 0) == 0) {//param_phyd_to_reg_calvl_sw_upd_ack
			rddata = bm_read32(bmdi, 0x3078 + base_addr_phyd);
			//KC_MSG("cvx32_INT_ISR_08", "Wait for SW update ACK ...", UVM_LOW);
		}
		//Clear SW update
		//KC_MSG("cvx32_INT_ISR_08", "Clear SW update ...", UVM_LOW);
		rddata = bm_read32(bmdi, 0x00CC + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 0, 6, 6);    //param_phyd_calvl_sw_upd
		//rddata = {rddata[31:7], 10x0,rddata[5:0]};
		bm_write32(bmdi, 0x00CC + base_addr_phyd,  rddata);
	}
	//----------------------------------------------------

	cvx32_ddr_phy_power_on_seq2(bmdi, base_addr_phyd, cv_base_addr_phyd, ddr_freq_mode);
	cvx32_set_dfi_init_complete(bmdi, base_addr_phyd);
}

void cvx32_ddr_phy_power_on_seq2(struct bm_device_info *bmdi, uint32_t base_addr_phyd, uint32_t cv_base_addr_phyd, uint32_t ddr_freq_mode)
{
	uint32_t rddata;
	//`test_stream = "-0f- cvx32_ddr_phy_power_on_seq2 ";

	//Change PLL frequency
	cvx32_chg_pll_freq(bmdi, cv_base_addr_phyd);

	//OEN
	//param_phyd_sel_cke_oenz        <= `PI_SD int_regin[0];
	rddata = bm_read32(bmdi, 0x0154 + base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 0, 0, 0);
	//rddata = {rddata[31:1], 10x0};
	bm_write32(bmdi, 0x0154 + base_addr_phyd,  rddata);

	//param_phyd_tx_ca_oenz          <= `PI_SD int_regin[0];
	//param_phyd_tx_ca_clk0_oenz     <= `PI_SD int_regin[8];
	//param_phyd_tx_ca_clk1_oenz     <= `PI_SD int_regin[16];
	rddata = 0x00000000;
	bm_write32(bmdi, 0x0130 + base_addr_phyd,  rddata);

	//Do DLLCAL if necessary
	//KC_MSG(get_name(), "Do DLLCAL if necessary ...", UVM_NONE);
	cvx32_dll_cal(bmdi, cv_base_addr_phyd, base_addr_phyd, ddr_freq_mode);
	//KC_MSG(get_name(), "Do DLLCAL done", UVM_NONE);

	//Do ZQCAL, include wait 200ns/200us
	//cvx32_ddr_zqcal_hw_isr8(0x7);//zqcal hw mode, bit0: offset_cal, bit1:pl_en, bit2:step2_en
	//KC_MSG(get_name(), "Do ZQCAL done", UVM_NONE);

	//zq calculate variation
	// zq_cal_var();
	//KC_MSG(get_name(), "zq calculate variation not run", UVM_NONE);

	//CA PD =0
	//All PHYA CA PD=0
	rddata = 0x00000000;
	bm_write32(bmdi, 0x60 + cv_base_addr_phyd,  rddata);
	bm_write32(bmdi, 0x64 + cv_base_addr_phyd,  rddata);
	//KC_MSG(get_name(), "All PHYA CA PD=0", UVM_NONE);

	//BYTE PD =0
	rddata = 0x00000000;
	bm_write32(bmdi, 0x00 + cv_base_addr_phyd,  rddata);
	//KC_MSG(get_name(), "TX_BYTE PD=0 ...", UVM_NONE);
}

void cvx32_ddr_phy_power_on_seq3(struct bm_device_info *bmdi, uint32_t base_addr_phyd)
{
	uint32_t rddata;
	//power on 3
	//`test_stream = "-10- cvx32_ddr_phy_power_on_seq3";
	//RESETYZ/CKE OENZ
	//param_phyd_sel_cke_oenz        <= `PI_SD int_regin[0];
	rddata = bm_read32(bmdi, 0x0154 + base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 0, 0, 0);
	//rddata = {rddata[31:1], 10x0};
	bm_write32(bmdi, 0x0154 + base_addr_phyd,  rddata);

	//param_phyd_tx_ca_oenz          <= `PI_SD int_regin[0];
	//param_phyd_tx_ca_clk0_oenz     <= `PI_SD int_regin[8];
	//param_phyd_tx_ca_clk1_oenz     <= `PI_SD int_regin[16];
	rddata = 0x00000000;
	bm_write32(bmdi, 0x0130 + base_addr_phyd,  rddata);
	//KC_MSG("cvx32_ddr_phy_power_on_seq3", "--> ca_oenz  ca_clk_oenz", UVM_LOW);

	//clock gated for power save
	//param_phya_reg_tx_byte0_en_ext}_oenz_gated_dline <= `PI_SD int_regin[0];
	//param_phya_reg_tx_byte1_en_ext}_oenz_gated_dline <= `PI_SD int_regin[1];
	//param_phya_reg_tx_byte2_en_ext}_oenz_gated_dline <= `PI_SD int_regin[2];
	//param_phya_reg_tx_byte3_en_ext}_oenz_gated_dline <= `PI_SD int_regin[3];
	rddata = bm_read32(bmdi, 0x0204 + base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 1, 18, 18);
	//rddata = {rddata[31:19], 10x1,rddata[17:0] };
	bm_write32(bmdi, 0x0204 + base_addr_phyd,  rddata);
	rddata = bm_read32(bmdi, 0x0224 + base_addr_phyd);
	//rddata = {rddata[31:19], 10x1,rddata[17:0] };
	rddata = modified_bits_by_value(rddata, 1, 18, 18);
	bm_write32(bmdi, 0x0224 + base_addr_phyd,  rddata);
	rddata = bm_read32(bmdi, 0x0244 + base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 1, 18, 18);

	//rddata = {rddata[31:19], 10x1,rddata[17:0] };
	bm_write32(bmdi, 0x0244 + base_addr_phyd,  rddata);
	rddata = bm_read32(bmdi, 0x0264 + base_addr_phyd);
	//rddata = {rddata[31:19], 10x1,rddata[17:0] };
	rddata = modified_bits_by_value(rddata, 1, 18, 18);
	bm_write32(bmdi, 0x0264 + base_addr_phyd,  rddata);
	//KC_MSG("cvx32_ddr_phy_power_on_seq3", "en clock gated for power save !!!", UVM_LOW);
}

void cvx32_set_dfi_init_complete(struct bm_device_info *bmdi, uint32_t base_addr_phyd)
{
	uint32_t rddata;
	//`test_stream = "-48- cvx32_set_dfi_init_complete";
	#ifdef REAL_LOCK
		//#200000ns;
		udelay(200);
	#endif
	//rddata[8] = 1;
	rddata = 0x00000010;
	bm_write32(bmdi, 0x0120 + base_addr_phyd,  rddata);
	//KC_MSG(get_name(), "set init_complete = 1 ...", UVM_NONE);
	//param_phyd_clkctrl_init_complete   <= int_regin[0];
	rddata = 0x00000001;
	bm_write32(bmdi, 0x0118 + base_addr_phyd,  rddata);

}

void cvx32_wait_for_dfi_init_complete(struct bm_device_info *bmdi, uint32_t base_addr_ctrl)
{
	uint32_t rddata;
	//`test_stream = "-13- cvx32_wait_for_dfi_init_complete";
	// synp setting
	// wait for dfi_init_complete to be 1
	while (1) {
		rddata = bm_read32(bmdi, base_addr_ctrl + 0x000001bc);
		if (get_bits_from_value(rddata, 0, 0) == 1) {
			break;
		}
	}
	// synp setting
	// deassert dfi_init_start, and enable the act on dfi_init_complete
	bm_write32(bmdi, base_addr_ctrl + 0x00000320, 0x00000000);
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x000001b0);
	rddata = modified_bits_by_value(rddata, 5, 5, 0);//
	//rddata = {rddata[31:6], 60x5};
	bm_write32(bmdi, base_addr_ctrl + 0x000001b0, rddata);
	bm_write32(bmdi, base_addr_ctrl + 0x00000320, 0x00000001);
	//KC_MSG("dfi_init_complete", "dfi_init_complete finish", UVM_LOW);

}

void cvx32_polling_synp_normal_mode(struct bm_device_info *bmdi, uint32_t base_addr_ctrl)
{
	//`test_stream = "-14- cvx32_polling_synp_normal_mode";
	//synp ctrl operating_mode
	uint32_t rddata;

	while (1) {
		rddata = bm_read32(bmdi, base_addr_ctrl + 0x00000004);
		if (get_bits_from_value(rddata, 2, 0) == 1) {
			break;
		}
	}
	//KC_MSG("polling_synp_normal_mode", "polling init done ", UVM_NONE);
}

void cvx32_clk_gating_enable(struct bm_device_info *bmdi, uint32_t cv_base_addr_phyd, uint32_t base_addr_ctrl)
{
	uint32_t rddata;
	//{
	//`test_stream= "-4D- cvx32_clk_gating_enable";
	//TOP_REG_CG_EN_PHYD_TOP      0
	//TOP_REG_CG_EN_CALVL         1
	//TOP_REG_CG_EN_WRLVL         2
	//N/A                         3
	//TOP_REG_CG_EN_WRDQ          4
	//TOP_REG_CG_EN_RDDQ          5
	//TOP_REG_CG_EN_PIGTLVL       6
	//TOP_REG_CG_EN_RGTRACK       7
	//TOP_REG_CG_EN_DQSOSC        8
	//TOP_REG_CG_EN_LB            9
	//TOP_REG_CG_EN_DLL_SLAVE     10
	//TOP_REG_CG_EN_DLL_MST       11
	//TOP_REG_CG_EN_ZQ            12
	//TOP_REG_CG_EN_PHY_PARAM     13 //0:a-on
	//'b10110010000001
	rddata = 0x00002C81;
	bm_write32(bmdi, 0x44+cv_base_addr_phyd,  rddata);

	//DFITMG0.dfi_t_ctrl_delay
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x190);
	rddata = modified_bits_by_value(rddata, 6, 28, 24);
	//rddata = {rddata[31:29], 50x6, rddata[23:0]};
	bm_write32(bmdi, base_addr_ctrl + 0x190, rddata);

	rddata = bm_read32(bmdi, base_addr_ctrl + 0x30); //phyd_stop_clk
	rddata = modified_bits_by_value(rddata, 1, 9, 9);
	//rddata = {rddata[31:10], 10x1, rddata[8:0]};
	bm_write32(bmdi, base_addr_ctrl + 0x30, rddata);
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x148); //dfi read/write clock gatting
	rddata = modified_bits_by_value(rddata, 1, 23, 23);
	rddata = modified_bits_by_value(rddata, 1, 31, 31);
	//rddata = {10x1,rddata[30:24], 10x1, rddata[22:0]};
	bm_write32(bmdi, base_addr_ctrl + 0x148, rddata);
	//}
}

void cvx32_bist_wr_sram_init(struct bm_device_info *bmdi, uint32_t rank, uint32_t base_addr_bist, int ddr_type, uint32_t ddr_freq_mode)
{
	//const int byte_per_page = 256;
	//int axi_per_page = byte_per_page / 64;

	uint32_t rddata;
	uint32_t cmd[6];
	uint32_t sram_st;
	uint32_t sram_sp;
	uint32_t fmax;
	uint32_t fmin;
	uint32_t DDR_SSO_PERIOD;
	int i;
	uint32_t repeat_num;

	//`test_stream = "-23- cvx32_bist_wr_sram_init";
	//KC_MSG("bist_wr_sram_init", "bist_wr_sram_init", UVM_LOW);

	if (ddr_freq_mode == MEM_FREQ_4266) {
		DDR_SSO_PERIOD = 21;
	}
	if (ddr_freq_mode == MEM_FREQ_3733) {
		DDR_SSO_PERIOD = 18;
	}
	if (ddr_freq_mode == MEM_FREQ_3200) {
		DDR_SSO_PERIOD = 16;
	}
	// bist clock enable, axi_len 8
	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x0, 0x000E000E);//x16_en
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x0, 0x000E000E);//x16_en   //TODO need to confirm
	} else {
		//if (ddr_mode==X16_MODE) {
		//  bm_write32(bmdi, base_addr_bist + 0x0, 0x000E000E);//x16_en
		//} else {
			bm_write32(bmdi, base_addr_bist + 0x0, 0x000C000C);
		//}
	}

	if ((ddr_type == DDR4_X8) || (ddr_type == DDR4_X16)) {
		//DDR4-3200/200 = 16
		//period * 14 * 5 * 9 = UI
		//UI / 4 / 512  - 1 = repeat_num
	#ifdef DDR_TRAIN_SPEED_UP
		fmax = DDR_SSO_PERIOD;
		fmin = DDR_SSO_PERIOD;
		sram_st = 0;
		sram_sp = 63;
		repeat_num = 0;
		//KC_MSG("bist_wr_sram_init", $psprintf("DDR_TRAIN_SPEED_UP sram_sp = %h",sram_sp), UVM_LOW);
	#else
		fmax = DDR_SSO_PERIOD;
		fmin = DDR_SSO_PERIOD;
		sram_st = 0;
		sram_sp = 511;
		//repeat_num = $ceil( (fmax*14*5*9) / (4*512)) -1;
		repeat_num = ((fmax * 14 * 5 * 9) / (4 * 512)) + 1 - 1;//Question?
		//KC_MSG("bist_wr_sram_init", $psprintf("sram_sp = %h repeat_num = %h",sram_sp, repeat_num), UVM_LOW);
	#endif
		// bist DDR_SSO_PERIOD
		bm_write32(bmdi, base_addr_bist + 0x24, (fmax << 8) + fmin);
		// cmd queue
		// op_code   start  stop pattern dq_inv DBI_polarity   DM_as_DBI  addr_not_rst dq_rotate   repeat
		cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (1 << 8) | (1 << 7) |
				(1 << 6) | (0 << 5)   | (0 << 4) | (repeat_num << 0);  // W  1~17  sram  repeat0
		cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (1 << 8) | (1 << 7) |
				(1 << 6) | (0 << 5)   | (0 << 4) | (repeat_num << 0);  // R  1~17  sram  repeat0
		//       NOP idle
		cmd[2] = 1;
		//       GOTO      addr_not_reset loop_cnt
		cmd[3] = (3 << 30) | (0 << 20) | (1 << 0); //GOTO
		cmd[4] = 0;  // NOP
		cmd[5] = 0;  // NOP
	}
	if ((ddr_type == LP4) || (ddr_type == LP4_2CH_1PHYD)) {
		//LPDDR4-4266/200 = 21
		//period * 14 * 5 * 9 = UI
		//UI / 4 / 512  - 1 = repeat_num
	#ifdef DDR_TRAIN_SPEED_UP
		fmax = DDR_SSO_PERIOD;
		fmin = DDR_SSO_PERIOD;
		sram_st = 0;
		sram_sp = 63;
		repeat_num = 0;
		//$display("[KC_DBG] DDR_TRAIN_SPEED_UP sram_sp = %h", sram_sp);
		//KC_MSG("bist_wr_sram_init", $psprintf("DDR_TRAIN_SPEED_UP sram_sp = %h",sram_sp), UVM_LOW);
	#else
		fmax = DDR_SSO_PERIOD;
		fmin = DDR_SSO_PERIOD;
		sram_st = 0;
		sram_sp = 511;
		//repeat_num = $ceil( (fmax*14*5*9) / (4*512)) - 1;
		repeat_num = ((fmax * 14 * 5 * 9) / (4 * 512)) + 1 - 1;   //Question?
		//KC_MSG("bist_wr_sram_init", $psprintf("sram_sp = %h repeat_num = %h",sram_sp, repeat_num), UVM_LOW);
	#endif
		// bist DDR_SSO_PERIOD
		bm_write32(bmdi, base_addr_bist + 0x24, (fmax << 8) + fmin);
		if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x24, (fmax << 8) + fmin);
		}
		// cmd queue
		// op_code   start  stop pattern    dq_inv     DBI_polarity   DM_as_DBI  addr_not_rst dq_rotate   repeat
		cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (0 << 8) | (0 << 7) |
			(1 << 6) | (0 << 5)   | (0 << 4) | (repeat_num << 0);  // W  1~17  sram  repeat0
		cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (0 << 8) | (0 << 7) |
			(1 << 6) | (0 << 5)   | (0 << 4) | (repeat_num << 0);  // R  1~17  sram  repeat0
		//       NOP idle
		cmd[2] = 1;
		//       GOTO      addr_not_reset loop_cnt
		cmd[3] = (3 << 30) | (0 << 20) | (1 << 0); //GOTO
		cmd[4] = 0;  // NOP
		cmd[5] = 0;  // NOP
	}

	// write cmd queue
	for (i = 0; i < 6; i = i + 1) {
		bm_write32(bmdi, base_addr_bist + 0x40 + i * 4, cmd[i]);
		if (ddr_type == LP4_2CH_1PHYD) {
			bm_write32(bmdi, base_addr_bist + 0x20000 + 0x40 + i * 4, cmd[i]);
		}
	}
	// specified DDR space
	bm_write32(bmdi, base_addr_bist + 0x10, 0x00000000);
	bm_write32(bmdi, base_addr_bist + 0x14, 0xffffffff);
	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x10, 0x00000000);
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x14, 0xffffffff);
	}

	//`ifdef X16_MODE
	//if (ddr_mode==X16_MODE) {
		// specified AXI address step to 2KB
	//  bm_write32(bmdi, base_addr_bist + 0x18, 2048 / axi_per_page / 16);
	//`else
	//} else {
	//bm_write32(bmdi, base_addr_bist + 0x18, 0x00000008);

	if (ddr_type == LP4_2CH_1PHYD) {
		rddata = bm_read32(bmdi, base_addr_bist + 0x18);
		rddata = modified_bits_by_value(rddata, 4, 11, 0);
		//rddata = {rddata[31:12], 120x4};
		if (rank == 1) {//bist at rank0
			rddata = modified_bits_by_value(rddata, 11, 21, 16); //rank_at_axi
			//rddata=modified_bits_by_value(rddata, 0, 24, 24);  //rank 0
			//rddata = {rddata[31:22], 6'd11};                     //Question?
		} else {
			if (rank == 2) {//bist at rank1
				rddata = modified_bits_by_value(rddata, 11, 21, 16); //rank_at_axi
				rddata = modified_bits_by_value(rddata, 1, 24, 24);  //rank 1
			} else {// bist at rank0 and rank1
				rddata = modified_bits_by_value(rddata, 0, 21, 16); //rank_at_axi
			}
		}
		bm_write32(bmdi, base_addr_bist + 0x18, rddata);
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x18, rddata);
	} else {
		bm_write32(bmdi, base_addr_bist + 0x18, 0x00000008);
	}
		//`test_stream = "bist_wr_sram_init done";
	//KC_MSG("bist_wr_sram_init", "bist_wr_sram_init done", UVM_LOW);
}

#if 0
void cvx32_bist_start_check(uint32_t base_addr_bist, uint32_t bist_result,
			uint64_t err_data_odd, uint64_t err_data_even, int ddr_type)
{
	uint64_t err_data_even_l;
	uint64_t err_data_even_h;
	uint64_t err_data_odd_l;
	uint64_t err_data_odd_h;
	uint64_t err_data_even_l_ch1;
	uint64_t err_data_even_h_ch1;
	uint64_t err_data_odd_l_ch1;
	uint64_t err_data_odd_h_ch1;
	uint64_t bist_done;
	uint64_t bist_err;
	//`test_stream = "-25- cvx32_bist_start_check";

	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00030003);
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x0, 0x00030003);
	} else {
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00010001);
	}
	//`test_stream = "bist start X32_MODE";
	//KC_MSG("cvx32_bist_start_check", "bist start", UVM_LOW);

	// polling bist done
	while (1) {
		bist_done = 1;
		bist_err = 0;
		rddata = bm_read32(bmdi, base_addr_bist + 0x80);
		bist_done &= get_bits_from_value(rddata, 2, 2);
		bist_err |= get_bits_from_value(rddata, 3, 3);
		if (ddr_type == LP4_2CH_1PHYD) {
		rddata = bm_read32(bmdi, base_addr_bist + 0x20000 + 0x80);
		bist_done &= get_bits_from_value(rddata, 2, 2);
		bist_err |= get_bits_from_value(rddata, 3, 3);
		}

		if (bist_done == 1) {
			break;
		}
	}
	NOTICE("Bist Done.\n");
	//`test_stream = "bist done";
	//KC_MSG("cvx32_bist_start_check", "Read bist done", UVM_LOW);
	if (bist_err == 1) {
		//#10;
		udelay(1);
		bist_result = 0;
		//`test_stream = "bist fail";
		// read err_data
		err_data_odd_l = bm_read32(bmdi, base_addr_bist + 0x88);
		err_data_odd_h = bm_read32(bmdi, base_addr_bist + 0x8c);
		err_data_even_l = bm_read32(bmdi, base_addr_bist + 0x90);
		err_data_even_h = bm_read32(bmdi, base_addr_bist + 0x94);
		if (ddr_type == LP4_2CH_1PHYD) {
		err_data_odd_l_ch1 = bm_read32(bmdi, base_addr_bist + 0x20000 + 0x88);
		err_data_odd_h_ch1 = bm_read32(bmdi, base_addr_bist + 0x20000 + 0x8c);
		err_data_even_l_ch1 = bm_read32(bmdi, base_addr_bist + 0x20000 + 0x90);
		err_data_even_h_ch1 = bm_read32(bmdi, base_addr_bist + 0x20000 + 0x94);
		err_data_odd   = ((err_data_odd_h_ch1  | err_data_odd_l_ch1) << 32)
				| (err_data_odd_h  | err_data_odd_l);
		err_data_even  = ((err_data_even_h_ch1 | err_data_even_l_ch1) << 32)
				| (err_data_even_h | err_data_even_l);
		} else {
		err_data_odd   = err_data_odd_h<<32  | err_data_odd_l;
		err_data_even  = err_data_even_h<<32 | err_data_even_l;
		}
		//`test_stream = "err data";
	} else {
		//#10;
		NOTICE("Bist Pass.\n");
		//udelay(1);
		bist_result = 1;
		//`test_stream = "bist pass";
		err_data_odd = 0;
		err_data_even = 0;
	}
	// bist disable
	bm_write32(bmdi, base_addr_bist + 0x0, 0x00050000);
	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x0, 0x00050000);
	}
}
void cvx32_bist_wr_prbs_init(uint32_t base_addr_bist, uint32_t rank, int ddr_type)
{
	uint32_t cmd[6];
	uint32_t sram_st;
	uint32_t sram_sp;
	uint32_t rddata;
	int i;
	//`test_stream = "-23- cvx32_bist_wr_prbs_init";
	//KC_MSG(get_name(), "bist_wr_prbs_init", UVM_NONE);
	// bist clock enable
	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x0, 0x000E000E);//x16_en, axi_len8
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x0, 0x000E000E);
	} else {
		//if (ddr_mode==X16_MODE) {
		//  bm_write32(bmdi, base_addr_bist + 0x0, 0x000E000E);//x16_en, axi_len8
		//} else {
		bm_write32(bmdi, base_addr_bist + 0x0, 0x000C000C);//axi_len8
		//}
	}
	sram_st = 0;
	sram_sp = 511;
	// cmd queue
	//  op_ code  start  stop  pattern  dq_inv  DBI_polarity  DM_as_DBI  adr_no_rst  dq_rotate   repeat
	cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) |
				(0 << 6) | (0 << 5)  | (0 << 4) | (0 << 0);  // W  1~17  prbs  repeat0
	cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) |
				(0 << 6) | (0 << 5)  | (0 << 4) | (0 << 0);  // R  1~17  prbs  repeat0
	cmd[2] = 0;// NOP
	cmd[3] = 0;// NOP
	cmd[4] = 0;// NOP
	cmd[5] = 0;// NOP
	// write cmd queue
	for (i = 0; i < 6; i = i + 1) {
		bm_write32(bmdi, base_addr_bist + 0x40 + i * 4, cmd[i]);
		if (ddr_type == LP4_2CH_1PHYD) {
			bm_write32(bmdi, base_addr_bist + 0x20000 + 0x40 + i * 4, cmd[i]);
		}
	}
	// specified DDR space
	bm_write32(bmdi, base_addr_bist + 0x10, 0x00000000);
	bm_write32(bmdi, base_addr_bist + 0x14, 0xffffffff);
	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x10, 0x00000000);
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x14, 0xffffffff);
	}

	//if (ddr_mode==X16_MODE) {
	// specified AXI address step
	//  bm_write32(bmdi, base_addr_bist + 0x18, 0x00000004);
	//} else {
	//bm_write32(bmdi, base_addr_bist + 0x18, 0x00000008);
		if (ddr_type == LP4_2CH_1PHYD) {
		rddata = bm_read32(bmdi, base_addr_bist + 0x18);
		rddata = modified_bits_by_value(rddata, 4, 11, 0);
		if (rank == 1) {//bist at rank0
			rddata = modified_bits_by_value(rddata, 11, 21, 16);//rank_at_axi
			rddata = modified_bits_by_value(rddata, 0, 24, 24);//rank 0
		} else {
		if (rank == 2) {//bist at rank1
			rddata = modified_bits_by_value(rddata, 11, 21, 16);//rank_at_axi
			rddata = modified_bits_by_value(rddata, 1, 24, 24);//rank 1
		} else {// bist at rank0 and rank1
			rddata = modified_bits_by_value(rddata, 0, 21, 16);//rank_at_axi
		}
		}
		bm_write32(bmdi, base_addr_bist + 0x18, rddata);
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x18, rddata);
		} else {
		bm_write32(bmdi, base_addr_bist + 0x18, 0x00000008);
	}
	//}
	//KC_MSG(get_name(), "bist_wr_prbs_init done", UVM_NONE);
}

void ctrl_init_detect_dram_size(uint32_t base_addr_ctrl, uint32_t base_addr_bist, int ddr_type, int dram_cap_in_mbyte)
{
	uint32_t cmd[6];
	uint32_t rddata;

	if ((ddr_type == LP4) || (ddr_type == LP4_2CH_1PHYD)) {
		cvx32_synp_mrr_lp4(1, 8, base_addr_ctrl);
		//bm_read32(bmdi, base_addr_ctrl + 0xC10, rddata);
		dram_cap_in_mbyte = get_bits_from_value(rddata, 5, 2);
		//KC_MSG(get_name(), $sformatf("LP4 DRAM size by MR8 is 0x%x",dram_cap_in_mbyte), UVM_NONE);
	} else {
		dram_cap_in_mbyte = 4;
		for (int i = 0; i < 6; i++)
		cmd[i] = 0x0;

		// Axsize = 3, axlen = 4, cgen
		bm_write32(bmdi, base_addr_bist + 0x0, 0x000e0006);

		// DDR space
		bm_write32(bmdi, base_addr_bist + 0x10, 0x00000000);
		bm_write32(bmdi, base_addr_bist + 0x14, 0xffffffff);

		// specified AXI address step
		bm_write32(bmdi, base_addr_bist + 0x18, 0x00000004);

		// write PRBS to 0x0 as background {{{
		cmd[0] = (1 << 30) + (0 << 21) + (3 << 12) + (5 << 9) + (0 << 8) + (0 << 0);// write 16 UI prbs
		for (int i = 0; i < 6; i++) {
			bm_write32(bmdi, base_addr_bist + 0x40 + i * 4, cmd[i]);
		}
		// bist_enable
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00010001);
		// polling BIST done
		do {
			rddata = bm_read32(bmdi, base_addr_bist + 0x80);
		} while (get_bits_from_value(rddata, 2, 2) == 0);
		// bist disable
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00010000);
		// }}}

		do {
		dram_cap_in_mbyte += 1;
		// write ~PRBS to (0x1 << dram_cap_in_mbyte) {{{
		// DDR space
		bm_write32(bmdi, base_addr_bist + 0x10, 0x00000001 << (dram_cap_in_mbyte + 20 - 4));
		cmd[0] = (1 << 30) + (0 << 21) + (3 << 12) + (5 << 9) + (1 << 8) + (0 << 0);// write 16 UI ~prbs
		for (int i = 0; i < 6; i++) {
			bm_write32(bmdi, base_addr_bist + 0x40 + i * 4, cmd[i]);
		}
		// bist_enable
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00010001);
		// polling BIST done
		do {
			rddata = bm_read32(bmdi, base_addr_bist + 0x80);
		} while (get_bits_from_value(rddata, 2, 2) == 0);
		// bist disable
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00010000);
		// }}}

		// check PRBS at 0x0 {{{
		// DDR space
		bm_write32(bmdi, base_addr_bist + 0x10, 0x00000000);
		cmd[0] = (2 << 30) + (0 << 21) + (3 << 12) + (5 << 9) + (0 << 8) + (0 << 0);// read 16 UI prbs
		for (int i = 0; i < 6; i++) {
			bm_write32(bmdi, base_addr_bist + 0x40 + i * 4, cmd[i]);
		}
		// bist_enable
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00010001);
		// polling BIST done
		do {
			rddata = bm_read32(bmdi, base_addr_bist + 0x80);
		} while (get_bits_from_value(rddata, 2, 2) == 0);
		// bist disable
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00010000);
		// }}}
		} while ((get_bits_from_value(rddata, 3, 3) == 0) &&
				(dram_cap_in_mbyte < 15));// BIST fail stop the loop

		// cgen disable
		bm_write32(bmdi, base_addr_bist + 0x0, 0x00040000);

		//KC_MSG(get_name(), $sformatf("DDRn DRAM size by BIST is 0x%x",dram_cap_in_mbyte), UVM_NONE);
	}
}

void cvx32_synp_mrr_lp4(uint32_t rank, uint32_t addr, uint32_t base_addr_ctrl)
{
	uint32_t rddata;
	//`test_stream = "-20- cvx32_synp_mrr_lp4";
	// assert MRSTAT.gmr_req
	bm_write32(bmdi, base_addr_ctrl + 0x18, 0x80000000);
	//`test_stream = "assert gmr_req";
	// poll MRSTAT.gmr_ack until it is 1
	// Poll MRSTAT.mr_wr_busy until it is 0
	//`test_stream = "Poll MRSTAT.mr_wr_busy until it is 0";
	rddata = 0;
	while (1) {
		rddata = bm_read32(bmdi, base_addr_ctrl + 0x18);
		if (get_bits_from_value(rddata, 0, 0) == 0 && get_bits_from_value(rddata, 16, 16) == 1) {
			break;
		}
	}
	// Write the MRCTRL0.mr_type, MRCTRL0.mr_addr, MRCTRL0.mr_rank and (for MRWs) MRCTRL1.mr_data
	//rddata[31:0] = 0;
	//rddata[0] = 1;// mr_type  0:write   1:read
	//rddata[5:4] = rank;// mr_rank
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x10);
	rddata = modified_bits_by_value(rddata, 1, 0, 0);
	rddata = modified_bits_by_value(rddata, rank, 5, 4);
	bm_write32(bmdi, base_addr_ctrl + 0x10, rddata);
	//`test_stream = "lp4 Write the MRCTRL0";
	// rddata[31:0] = 0;
	// rddata[15:8] = addr;// mr_addr
	// rddata[ 7:0] = 0;// mr_data
	rddata = 0x00000000;
	rddata = modified_bits_by_value(rddata, addr, 15, 8);
	rddata = modified_bits_by_value(rddata, 0, 7, 0);
	bm_write32(bmdi, base_addr_ctrl + 0x14, rddata);
	//`test_stream = "lp4 Write the MRCTRL1";
	// Write MRCTRL0.mr_wr to 1
	rddata = bm_read32(bmdi, base_addr_ctrl + 0x10);
	//rddata[31] = 1;
	rddata = modified_bits_by_value(rddata, 1, 31, 31);
	bm_write32(bmdi, base_addr_ctrl + 0x10, rddata);
	//`test_stream = "lp4 Write MRCTRL0.mr_wr to 1";
	while (1) {
		rddata = bm_read32(bmdi, base_addr_ctrl + 0x18);
		//} while (rddata[0] != 0);
		if (get_bits_from_value(rddata, 0, 0) == 0) {
			break;
		}
	}
	// deassert gmr_req
	bm_write32(bmdi, base_addr_ctrl + 0x18, 0x00000000);
	//`test_stream = "de-assert gmr_req";
	rddata = bm_read32(bmdi, base_addr_ctrl + 0xC10);
	//$display("1st UI ch1/ch0 MRR data = %h ...", {80x0,rddata[16*1 +: 8],80x0,rddata[16*0 +: 8]});
	rddata = bm_read32(bmdi, base_addr_ctrl + 0xC14);
	//$display("2nd UI ch1/ch0 MRR data = %h ...", {80x0,rddata[16*1 +: 8],80x0,rddata[16*0 +: 8]});
	rddata = bm_read32(bmdi, base_addr_ctrl + 0xC18);
	//$display("3rd UI ch1/ch0 MRR data = %h ...", {80x0,rddata[16*1 +: 8],80x0,rddata[16*0 +: 8]});
	rddata = bm_read32(bmdi, base_addr_ctrl + 0xC1c);
	//$display("4th UI ch1/ch0 MRR data = %h ...", {80x0,rddata[16*1 +: 8],80x0,rddata[16*0 +: 8]});
}
#endif

void cvx32_chg_pll_freq(struct bm_device_info *bmdi, uint32_t cv_base_addr_phyd)
{
	uint32_t EN_PLL_SPEED_CHG;
	uint32_t CUR_PLL_SPEED;
	uint32_t NEXT_PLL_SPEED;
	uint32_t rddata;
	//`test_stream ="-04- cvx32_chg_pll_freq";
	//Change PLL frequency
	//KC_MSG("cvx32_chg_pll_freq", "Change PLL frequency ...", UVM_LOW);
	//TOP_REG_RESETZ_DIV =0
	rddata = 0x00000000;
	bm_write32(bmdi, 0x04 + cv_base_addr_phyd,  rddata);
	//TOP_REG_RESETZ_DQS =0
	bm_write32(bmdi, 0x08 + cv_base_addr_phyd,  rddata);

	//TOP_REG_DDRPLL_MAS_RSTZ_DIV  =0
	rddata = bm_read32(bmdi, 0xA8 + cv_base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 0, 6, 6);
	bm_write32(bmdi, 0xA8 + cv_base_addr_phyd,  rddata);

	//`test_stream = "RSTZ_DIV=0";
	rddata = bm_read32(bmdi, 0x4C + cv_base_addr_phyd);
	EN_PLL_SPEED_CHG  = get_bits_from_value(rddata, 0, 0);
	CUR_PLL_SPEED     = get_bits_from_value(rddata, 5, 4);
	NEXT_PLL_SPEED    = get_bits_from_value(rddata, 9, 8);
	//KC_MSG(get_name(), $sformatf("CUR_PLL_SPEED = %h, NEXT_PLL_SPEED = %h,
			//EN_PLL_SPEED_CHG=%h", CUR_PLL_SPEED, NEXT_PLL_SPEED, EN_PLL_SPEED_CHG), UVM_DEBUG);

	if (EN_PLL_SPEED_CHG) {
		if (NEXT_PLL_SPEED == 0) {// next clk_div40
		rddata = modified_bits_by_value(rddata, NEXT_PLL_SPEED, 5, 4);//
		rddata = modified_bits_by_value(rddata, CUR_PLL_SPEED, 9, 8);//
		bm_write32(bmdi, 0x4C + cv_base_addr_phyd,  rddata);
		cvx32_clk_div40(bmdi, cv_base_addr_phyd);
		//`test_stream ="clk_div40";
		//KC_MSG("cvx32_chg_pll_freq", "clk_div40 ...", UVM_LOW);
		} else {
			if (NEXT_PLL_SPEED == 0x2) {// next clk normal
				rddata = modified_bits_by_value(rddata, NEXT_PLL_SPEED, 5, 4);
				rddata = modified_bits_by_value(rddata, CUR_PLL_SPEED, 9, 8);
				bm_write32(bmdi, 0x4C + cv_base_addr_phyd,  rddata);
				cvx32_clk_normal(bmdi, cv_base_addr_phyd);
				//`test_stream ="clk_normal";
				//KC_MSG("cvx32_chg_pll_freq", "clk_normal ...", UVM_LOW);
			} else {
				if (NEXT_PLL_SPEED == 0x1) {// next clk normal div_2
				rddata = modified_bits_by_value(rddata, NEXT_PLL_SPEED, 5, 4);//
				rddata = modified_bits_by_value(rddata, CUR_PLL_SPEED, 9, 8);//
				bm_write32(bmdi, 0x4C + cv_base_addr_phyd,  rddata);
				cvx32_clk_div2(bmdi, cv_base_addr_phyd);
				//`test_stream ="clk_div2";
				//KC_MSG("cvx32_chg_pll_freq", "clk_div2 ...", UVM_LOW);
				}
			}
		}
	}
	//TOP_REG_RESETZ_DIV  =1
	rddata = 0x00000001;
	bm_write32(bmdi, 0x04 + cv_base_addr_phyd,  rddata);

	rddata = bm_read32(bmdi, 0xA8 + cv_base_addr_phyd);
	//TOP_REG_DDRPLL_MAS_RSTZ_DIV    6    6
	rddata = modified_bits_by_value(rddata, 1, 6, 6);
	bm_write32(bmdi, 0xA8 + cv_base_addr_phyd,  rddata);
	//`test_stream = "RSTZ_DIV=1";
	//TOP_REG_RESETZ_DQS
	rddata = 0x00000001;
	bm_write32(bmdi, 0x08 + cv_base_addr_phyd,  rddata);
	//`test_stream = "TOP_REG_RESETZ_DQS";
	//KC_MSG("cvx32_chg_pll_freq", "Wait for DRRPLL_SLV_LOCK=1 ...", UVM_LOW);
	if (EN_PLL_SPEED_CHG != 1) {
		rddata = 0;
		while (get_bits_from_value(rddata, 0, 0) == 0) {
			rddata = bm_read32(bmdi, 0xA4 + cv_base_addr_phyd);
			//KC_MSG("cvx32_chg_pll_freq", "REAL_LOCK ...", UVM_LOW);
			//#200;
			udelay(1);
		}
		//KC_MSG("cvx32_chg_pll_freq", "check PLL lock...", UVM_LOW);
		//#10ns;
		udelay(1);
	}
	//KC_MSG("cvx32_chg_pll_freq", "DRRPLL_SLV_LOCK=1...", UVM_LOW);
}

void cvx32_dll_cal(struct bm_device_info *bmdi, uint32_t cv_base_addr_phyd, uint32_t base_addr_phyd, uint32_t ddr_freq_mode)
{
	uint32_t EN_PLL_SPEED_CHG;
	uint32_t CUR_PLL_SPEED;
	uint32_t NEXT_PLL_SPEED;
	uint32_t rddata;

	//KC_MSG(get_name(), "Do DLLCAL ...", UVM_NONE);

	//`test_stream = "-2b- cvx32_dll_cal";
	//TOP_REG_EN_PLL_SPEED_CHG <= #RD (~pwstrb_mask[0] & TOP_REG_EN_PLL_SPEED_CHG) |  pwstrb_mask_pwdata[0];
	//TOP_REG_CUR_PLL_SPEED   [1:0] <= #RD (~pwstrb_mask[5:4] &
			//TOP_REG_CUR_PLL_SPEED[1:0]) |  pwstrb_mask_pwdata[5:4];
	//TOP_REG_NEXT_PLL_SPEED  [1:0] <= #RD (~pwstrb_mask[9:8] &
			//TOP_REG_NEXT_PLL_SPEED[1:0]) |  pwstrb_mask_pwdata[9:8];
	rddata = bm_read32(bmdi, 0x4C + cv_base_addr_phyd);
	EN_PLL_SPEED_CHG = get_bits_from_value(rddata, 0, 0);
	CUR_PLL_SPEED    = get_bits_from_value(rddata, 5, 4);
	NEXT_PLL_SPEED   = get_bits_from_value(rddata, 9, 8);
	//KC_MSG(get_name(), $sformatf("CUR_PLL_SPEED = %h, NEXT_PLL_SPEED = %h, EN_PLL_SPEED_CHG=%h ",
			//CUR_PLL_SPEED, NEXT_PLL_SPEED, EN_PLL_SPEED_CHG), UVM_DEBUG);

	if (CUR_PLL_SPEED != 0) {//only do calibration and update when high speed
		//param_phyd_dll_rx_start_cal <= int_regin[1];
		//param_phyd_dll_tx_start_cal <= int_regin[17];
		rddata = bm_read32(bmdi, 0x0040 + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 0, 1, 1);
		rddata = modified_bits_by_value(rddata, 0, 17, 17);
		bm_write32(bmdi, 0x0040 + base_addr_phyd,  rddata);
		//param_phyd_dll_rx_start_cal <= int_regin[1];
		//param_phyd_dll_tx_start_cal <= int_regin[17];
		rddata = bm_read32(bmdi, 0x0040 + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 1, 1, 1);
		rddata = modified_bits_by_value(rddata, 1, 17, 17);
		bm_write32(bmdi, 0x0040 + base_addr_phyd,  rddata);
		rddata = 0x00000000;
		while (get_bits_from_value(rddata, 16, 16) == 0) {
			//param_phyd_to_reg_tx_dll_lock            16     16
			rddata = bm_read32(bmdi, 0x3014 + base_addr_phyd);
		}
		//KC_MSG(get_name(), "DLL lock !", UVM_NONE);
		//`test_stream ="DLL lock";
		//#1000;
		udelay(1);
		//`test_stream ="Do DLLUPD";

		cvx32_dll_cal_status(bmdi, base_addr_phyd, ddr_freq_mode);
	} else {//stop calibration and update when low speed
		//param_phyd_dll_rx_start_cal <= int_regin[1];
		//param_phyd_dll_tx_start_cal <= int_regin[17];
		rddata = bm_read32(bmdi, 0x0040 + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 0, 1, 1);
		rddata = modified_bits_by_value(rddata, 0, 17, 17);
		bm_write32(bmdi, 0x0040 + base_addr_phyd,  rddata);
	}
	//KC_MSG(get_name(), "Do DLLCAL Finish", UVM_NONE);
	//`test_stream ="Do DLLCAL Finish";
}

void cvx32_dll_cal_status(struct bm_device_info *bmdi, uint32_t base_addr_phyd, uint32_t ddr_freq_mode)
{
	uint32_t err_cnt;
	uint32_t rx_dll_code;
	uint32_t tx_dll_code;
	uint32_t rddata;

	//`test_stream = "-37- cvx32_dll_cal_status";

	rddata = bm_read32(bmdi, 0x3018 + base_addr_phyd);
	//$display("param_phyd_to_reg_rx_dll_code0= %h ...", get_bits_from_value(rddata, 7, 0));
	//$display("param_phyd_to_reg_rx_dll_code1= %h ...", get_bits_from_value(rddata, 15, 8));
	//$display("param_phyd_to_reg_rx_dll_code2= %h ...", get_bits_from_value(rddata, 23, 16));
	//$display("param_phyd_to_reg_rx_dll_code3= %h ...", get_bits_from_value(rddata, 31, 24));

	rddata = bm_read32(bmdi, 0x301c + base_addr_phyd);
	//$display("param_phyd_to_reg_rx_dll_max= %h ...", get_bits_from_value(rddata, 7, 0));
	//$display("param_phyd_to_reg_rx_dll_min= %h ...", get_bits_from_value(rddata, 15, 8));

	rddata = bm_read32(bmdi, 0x3020 + base_addr_phyd);
	//$display("param_phyd_to_reg_tx_dll_code0= %h ...", get_bits_from_value(rddata, 7, 0));
	//$display("param_phyd_to_reg_tx_dll_code1= %h ...", get_bits_from_value(rddata, 15, 8));
	//$display("param_phyd_to_reg_tx_dll_code2= %h ...", get_bits_from_value(rddata, 23, 16));
	//$display("param_phyd_to_reg_tx_dll_code3= %h ...", get_bits_from_value(rddata, 31, 24));

	rddata = bm_read32(bmdi, 0x3024 + base_addr_phyd);
	//$display("param_phyd_to_reg_tx_dll_max= %h ...", get_bits_from_value(rddata, 7, 0));
	//$display("param_phyd_to_reg_tx_dll_min= %h ...", get_bits_from_value(rddata, 15, 8));


	err_cnt = 0;
	rddata = bm_read32(bmdi, 0x3014 + base_addr_phyd);
	rx_dll_code = get_bits_from_value(rddata, 15, 8);
	tx_dll_code = get_bits_from_value(rddata, 31, 24);

	if (ddr_freq_mode == MEM_FREQ_4266) {
		if (!((rx_dll_code > 0x37) && (rx_dll_code < 0x3D))) {
		//KC_MSG(get_name(),
		//$sformatf("Info!! rx_dll_code dly_sel result fail, not 0x37~0x3D --%h--",rx_dll_code), UVM_DEBUG);
		err_cnt = err_cnt + 1;
		}
		if (!((tx_dll_code > 0x37) && (tx_dll_code < 0x3D))) {
		//KC_MSG(get_name(),
		//$sformatf("Info!! tx_dll_code dly_sel result fail, not 0x37~0x3D --%h--",tx_dll_code), UVM_DEBUG);
		err_cnt = err_cnt + 1;
		}
	}
	if (ddr_freq_mode == MEM_FREQ_3733) {
		if (!((rx_dll_code > 0x15) && (rx_dll_code < 0x52))) {
		//KC_MSG(get_name(),
		//$sformatf("Info!! rx_dll_code dly_sel result fail, not 0x15~0x52 --%h--",rx_dll_code), UVM_DEBUG);
		err_cnt = err_cnt + 1;
		}
		if (!((tx_dll_code > 0x15) && (tx_dll_code < 0x52))) {
		//KC_MSG(get_name(),
		//$sformatf("Info!! tx_dll_code dly_sel result fail, not 0x15~0x52 --%h--",tx_dll_code), UVM_DEBUG);
		err_cnt = err_cnt + 1;
		}
	}
	if (ddr_freq_mode == MEM_FREQ_3200) {
		if (!((rx_dll_code > 0x4A) && (rx_dll_code < 0x52))) {
		//KC_MSG(get_name(),
		//$sformatf("Info!! rx_dll_code dly_sel result fail, not 0x4A~0x52 --%h--",rx_dll_code), UVM_DEBUG);
		err_cnt = err_cnt + 1;
		}
		if (!((tx_dll_code > 0x4A) && (tx_dll_code < 0x52))) {
		//KC_MSG(get_name(),
		//$sformatf("Info!! tx_dll_code dly_sel result fail, not 0x4A~0x52 --%h--",tx_dll_code), UVM_DEBUG);
		err_cnt = err_cnt + 1;
		}
	}

	//      FINISHED!

	if (err_cnt != 0x0) {
		//$display("*****************************************");
		//$display("DLL_CAL ERR!!! err_cnt = %d...", err_cnt);
		//$display("*****************************************");
	} else {
		//$display("*****************************************");
		//$display("DLL_CAL PASS!!!");
		//$display("*****************************************");
	}

}

void  cvx32_clk_div40(struct bm_device_info *bmdi, uint32_t cv_base_addr_phyd)
{
	uint32_t rddata;
	//`test_stream ="-02- cvx32_clk_div40";
	//KC_MSG(get_name(), "Enter low D40 frequency !!!", UVM_NONE);
	rddata = bm_read32(bmdi, 0xA8 + cv_base_addr_phyd);
	// TOP_REG_DDRPLL_MAS_SELLOWSPEED    7    7
	rddata = modified_bits_by_value(rddata, 1, 7, 7);//TOP_REG_DDRPLL_SEL_LOW_SPEED =1
	bm_write32(bmdi, 0xA8 + cv_base_addr_phyd,  rddata);
	//`test_stream =" Enter low D40 frequency";
}
void cvx32_clk_normal(struct bm_device_info *bmdi, uint32_t cv_base_addr_phyd)
{
	uint32_t rddata;
	//`test_stream ="-03- cvx32_clk_normal";
	//KC_MSG(get_name(), "back to original frequency !!!", UVM_NONE);
	rddata = bm_read32(bmdi, 0xA8 + cv_base_addr_phyd);
	//TOP_REG_DDRPLL_MAS_DIVOUTSEL        1    0
	//TOP_REG_DDRPLL_MAS_SELLOWSPEED    7    7
	rddata = modified_bits_by_value(rddata, 0, 1, 0);
	rddata = modified_bits_by_value(rddata, 0, 7, 7);
	bm_write32(bmdi, 0xA8 + cv_base_addr_phyd,  rddata);
}
void cvx32_clk_div2(struct bm_device_info *bmdi, uint32_t cv_base_addr_phyd)
{
	uint32_t rddata;
	//`test_stream ="-01- cvx32_clk_div2";
	//KC_MSG(get_name(), "div2 original frequency !!!", UVM_NONE);
	rddata = bm_read32(bmdi, 0xA8 + cv_base_addr_phyd);
	//TOP_REG_DDRPLL_MAS_DIVOUTSEL    1    0
	rddata = modified_bits_by_value(rddata, 1, 1, 0);
	bm_write32(bmdi, 0xA8 + cv_base_addr_phyd,  rddata);
	//`test_stream =" div2 original frequency";
}
#if 0
void cvx32_dll_sw_clr(uint32_t base_addr_ctrl, uint32_t base_addr_phyd)
{
	uint32_t phyd_stop_clk;
	uint32_t phyd_rd_wr_clk_stop;
	//`test_stream= "-56- cvx32_dll_sw_clr";

	phyd_stop_clk = bm_read32(bmdi, base_addr_ctrl + 0x30);//phyd_stop_clk
	rddata = modified_bits_by_value(phyd_stop_clk, 0, 9, 9);
	bm_write32(bmdi, base_addr_ctrl + 0x30, rddata);
	//    if (ddr_type==LP4_2CH_1PHYD) {
	//        bm_read32(bmdi, base_addr_ctrl_CH1 + 0x30, phyd_stop_clk);//phyd_stop_clk
	//        rddata = modified_bits_by_value(phyd_stop_clk, 0, 9, 9);
	//        bm_write32(bmdi, base_addr_ctrl_CH1 + 0x30, rddata);
	//    }

	phyd_rd_wr_clk_stop = bm_read32(bmdi, base_addr_ctrl + 0x148);
	rddata = phyd_rd_wr_clk_stop & 0x7F7FFFFF;// PATCH4.phyd_wr_clk_stop:31:1=0x0, PATCH4.phyd_rd_clk_stop:23:1=0x0
	bm_write32(bmdi, base_addr_ctrl + 0x148, rddata);

	//param_phyd_sw_dfi_phyupd_req
	rddata = bm_read32(bmdi, 0x0174  + base_addr_phyd);
	rddata = modified_bits_by_value(rddata, 1, 0, 0);
	rddata = modified_bits_by_value(rddata, 1, 8, 8);
	bm_write32(bmdi, 0x0174  + base_addr_phyd,  rddata);
	while (1) {
		// param_phyd_to_reg_sw_phyupd_dline_done
		rddata = bm_read32(bmdi, 0x3030 + base_addr_phyd);
		if (get_bits_from_value(rddata, 24, 24) == 0x1) {
			break;
		}
	}
	bm_write32(bmdi, base_addr_ctrl + 0x30, phyd_stop_clk);
	//    if (ddr_type==LP4_2CH_1PHYD) {
	//        bm_write32(bmdi, base_addr_ctrl_CH1 + 0x30, phyd_stop_clk);
	//    }
	rddata = phyd_rd_wr_clk_stop | 0x80800000;// PATCH4.phyd_wr_clk_stop:31:1=0x0, PATCH4.phyd_rd_clk_stop:23:1=0x0
	bm_write32(bmdi, base_addr_ctrl + 0x148, rddata);

}
void cvx32_bist_rdlvl_init(uint32_t mode, uint32_t ddr_freq_mode, uint32_t rank,
			uint32_t base_addr_bist, uint32_t base_addr_phyd, int ddr_type)
{
	uint32_t cmd[6];
	uint32_t sram_st;
	uint32_t sram_sp;
	uint32_t fmax;
	uint32_t fmin;
	uint32_t repeat_num;
	uint32_t rddata;
	uint32_t sso_period;
	int i;

	//mode = 0x0  : MPR mode, DDR3/4 only. MPC mode LP4 only
	//mode = 0x1  : sram write/read continuous goto
	//mode = 0x2  : multi- bist write/read
	//mode = 0x3  : sram write/read continuous goto with DM_AS_DBI
	//mode = 0x12 : with Error enject,  multi- bist write/read

	//`test_stream = "-23_2- cvx32_bist_rdlvl_init";
	//KC_MSG(get_name(), $psprintf("cvx32_bist_rdlvl_init mode = %h",mode), UVM_LOW);
	if (ddr_freq_mode == MEM_FREQ_4266) {
		sso_period = 21;
	}
	if (ddr_freq_mode == MEM_FREQ_3733) {
		sso_period = 18;
	}
	if (ddr_freq_mode == MEM_FREQ_3200) {
		sso_period = 16;
	}

	// bist clock enable
	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x0, 0x000E000E);//x16_en
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x0, 0x000E000E);
	} else {
		bm_write32(bmdi, base_addr_bist + 0x0, 0x000C000C);
	}
	if (mode == 0x0) {//MPR mode
		sram_st = 0;
		if (ddr_type == LP4_2CH_1PHYD) {
		sram_sp = 1;
		} else {
		sram_sp = 3;
		}
		// cmd queue
		//  op_code  start  stop  pattern  dq_inv  DBI_polarity  dq_rotate  repeat
		//cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) |
					//(0 << 7) | (0 << 4) | (0 << 0);// W  1~17  prbs  repeat0
		cmd[0] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) |
					(0 << 7) | (0 << 4) | (0 << 0);// R  1~17  prbs  repeat0
		cmd[1] = 0;//(2 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) |
					(0 << 7) | (0 << 4) | (0 << 0);// R  1~17  prbs  repeat0
		cmd[2] = 0;// NOP
		cmd[3] = 0;// NOP
		cmd[4] = 0;// NOP
		cmd[5] = 0;// NOP
	} else if (mode == 0x1) {//sram write/read continuous goto
		if ((ddr_type == DDR4_X8) || (ddr_type == DDR4_X16)) {
		//DDR4-3200/200 = 16
		//period * 14 * 5 * 9 = UI
		//UI / 4 / 512  - 1 = repeat_num
		if ($test$plusargs("DDR_TRAIN_SPEED_UP")) {
			fmax = sso_period;
			fmin = sso_period;
			sram_st = 0;
			sram_sp = 63;
			repeat_num = 0;
			//KC_MSG(get_name(), $psprintf("DDR_TRAIN_SPEED_UP sram_sp = %h",sram_sp), UVM_LOW);
		} else {
			fmax = sso_period;
			fmin = sso_period;
			sram_st = 0;
			sram_sp = 511;
			repeat_num = $ceil((fmax*14*5*9) / (4*512)) - 1;
			//KC_MSG(get_name(), $psprintf("sram_sp = %h repeat_num = %h",sram_sp, repeat_num), UVM_LOW);
		}
		// bist sso_period
		bm_write32(bmdi, base_addr_bist + 0x24, (fmax << 8) + fmin);
		// cmd queue
		//  op_code  start  stop  pattern  dq_inv  DBI_polarity   DM_as_DBI  addr_not_rst dq_rotate   repeat
		cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (1 << 8) | (1 << 7) |
			(1 << 6) | (0 << 5) | (0 << 4) | (repeat_num << 0);// W  1~17  sram  repeat0
		cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (1 << 8) | (1 << 7) |
			(1 << 6) | (0 << 5) | (0 << 4) | (repeat_num << 0);// R  1~17  sram  repeat0
		//       NOP idle
		cmd[2] = 1;
		//       GOTO      addr_not_reset loop_cnt
		cmd[3] = (3 << 30) | (0 << 20) | (1 << 0);//GOTO
		cmd[4] = 0;// NOP
		cmd[5] = 0;// NOP
		}
		if ((ddr_type == LP4) || (ddr_type == LP4_2CH_1PHYD)) {
		//LPDDR4-4266/200 = 21
		//period * 14 * 5 * 9 = UI
		//UI / 4 / 512  - 1 = repeat_num
		if ($test$plusargs("DDR_TRAIN_SPEED_UP")) {
			fmax = sso_period;
			fmin = sso_period;
			sram_st = 0;
			sram_sp = 63;
			repeat_num = 0;
			//KC_MSG(get_name(), $psprintf("DDR_TRAIN_SPEED_UP sram_sp = %h",sram_sp), UVM_LOW);
		} else {
			fmax = sso_period;
			fmin = sso_period;
			sram_st = 0;
			sram_sp = 511;
			repeat_num = $ceil((fmax*14*5*9) / (4*512)) - 1;
			//KC_MSG(get_name(), $psprintf("sram_sp = %h repeat_num = %h",sram_sp, repeat_num), UVM_LOW);
		}
		// bist sso_period
		bm_write32(bmdi, base_addr_bist + 0x24, (fmax << 8) + fmin);
		if (ddr_type == LP4_2CH_1PHYD) {
			bm_write32(bmdi, base_addr_bist + 0x20000 + 0x24, (fmax << 8) + fmin);
		}
		// cmd queue
		//  op_code  start  stop  pattern  dq_inv     DBI_polarity   DM_as_DBI  addr_not_rst dq_rotate   repeat
		cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (0 << 8) | (0 << 7) |
					(0 << 6) | (0 << 5) | (0 << 4) | (repeat_num << 0);// W  1~17  sram  repeat0
		cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (0 << 8) | (0 << 7) |
					(0 << 6) | (0 << 5) | (0 << 4) | (repeat_num << 0);// R  1~17  sram  repeat0
		//       NOP idle
		cmd[2] = 1;
		//       GOTO      addr_not_reset loop_cnt
		cmd[3] = (3 << 30) | (0 << 20) | (1 << 0);//GOTO
		cmd[4] = 0;// NOP
		cmd[5] = 0;// NOP
		}
	} else if (mode == 0x2) {//multi-prbs bist write/read
		sram_st = 0;
		//sram_sp = 32'd511;
		sram_sp = 7;
		// cmd queue
		//  op_code  start  stop  pattern  dq_inv  DBI_polarity   DM_as_DBI  addr_not_rst dq_rotate   repeat
		cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) | (0 << 6) |
					(0 << 5) | (0 << 4) | (0 << 0);// W  1~17  prbs  repeat0
		cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) | (0 << 6) |
					(0 << 5) | (0 << 4) | (0 << 0);// R  1~17  prbs  repeat0
		cmd[2] = 0;// NOP
		cmd[3] = 0;// NOP
		cmd[4] = 0;// NOP
		cmd[5] = 0;// NOP
	} else if (mode == 0x3) {//sram write/read continuous goto with DM_AS_DBI
		//LPDDR4-4266/200 = 21
		//period * 14 * 5 * 9 = UI
		//UI / 4 / 512  - 1 = repeat_num
		if ($test$plusargs("DDR_TRAIN_SPEED_UP")) {
		fmax = sso_period;
		fmin = sso_period;
		sram_st = 0;
		sram_sp = 63;
		repeat_num = 0;
		//KC_MSG(get_name(), $psprintf("DDR_TRAIN_SPEED_UP sram_sp = %h",sram_sp), UVM_LOW);
		} else {
		fmax = sso_period;
		fmin = sso_period;
		sram_st = 0;
		sram_sp = 511;
		//repeat_num = $ceil((fmax*14*5*9) / (4*512)) - 1;
		repeat_num = ((fmax*14*5*9) / (4*512)) + 1 - 1;//Question?
		//KC_MSG(get_name(), $psprintf("sram_sp = %h repeat_num = %h",sram_sp, repeat_num), UVM_LOW);
		}
		// bist sso_period
		bm_write32(bmdi, base_addr_bist + 0x24, (fmax << 8) + fmin);
		if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x24, (fmax << 8) + fmin);
		}
		// cmd queue
		//  op_code  start  stop  pattern  dq_inv  DBI_polarity  DM_as_DBI  addr_not_rst dq_rotate  repeat
		cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (0 << 8) | (0 << 7) | (1 << 6) |
					(0 << 5) | (0 << 4) | (repeat_num << 0);// W  1~17  sram  repeat0
		cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (4 << 9) | (0 << 8) | (0 << 7) | (1 << 6) |
					(0 << 5) | (0 << 4) | (repeat_num << 0);// R  1~17  sram  repeat0
		//       NOP idle
		cmd[2] = 1;
		//       GOTO      addr_not_reset loop_cnt
		cmd[3] = (3 << 30) | (0 << 20) | (1 << 0);//GOTO
		cmd[4] = 0;// NOP
		cmd[5] = 0;// NOP
	} else if (mode == 0x12) {// with Error enject,  multi- bist write/read
		//----for Error enject simulation only
		rddata = bm_read32(bmdi, 0x0084 + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 0x1d, 4, 0);//param_phyd_pirdlvl_trig_lvl_start
		rddata = modified_bits_by_value(rddata, 0x1f, 12, 8);//param_phyd_pirdlvl_trig_lvl_}
		bm_write32(bmdi, 0x0084 + base_addr_phyd,  rddata);
		//----for Error enject simulation only

		sram_st = 0;
		//sram_sp = 32'd511;
		sram_sp = 7;
		// cmd queue
		//  op_code  start  stop  pattern    dq_inv   DBI_polarity dq_rotate   repeat
		cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) |
					(0 << 4) | (0 << 0);  // W  1~17  prbs  repeat0
		cmd[1] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) |
					(0 << 4) | (0 << 0);  // R  1~17  prbs  repeat0
		cmd[2] = 0;  // NOP
		cmd[3] = 0;  // NOP
		cmd[4] = 0;  // NOP
		cmd[5] = 0;  // NOP
	} else if (mode == 0x10) {// with Error enject,  multi- bist write/read
		//----for Error enject simulation only
		rddata = bm_read32(bmdi, 0x0084 + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 1, 4, 0); //param_phyd_pirdlvl_trig_lvl_start
		rddata = modified_bits_by_value(rddata, 3, 12, 8);//param_phyd_pirdlvl_trig_lvl_}
		bm_write32(bmdi, 0x0084 + base_addr_phyd,  rddata);
		//----for Error enject simulation only

		sram_st = 0;
		//sram_sp = 32'd511;
		sram_sp = 3;
		// cmd queue
		//  op_code  start  stop  pattern    dq_inv     DBI_polarity    dq_rotate   repeat
		//cmd[0] = (1 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) |
					//(0 << 4) | (0 << 0);  // W  1~17  prbs  repeat0
		cmd[0] = (2 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) |
					(0 << 4) | (0 << 0);  // R  1~17  prbs  repeat0
		cmd[1] = 0;  //(2 << 30) | (sram_st << 21) | (sram_sp << 12) | (5 << 9) | (0 << 8) | (0 << 7) |
					(0 << 4) | (0 << 0);  // R  1~17  prbs  repeat0
		cmd[2] = 0;  // NOP
		cmd[3] = 0;  // NOP
		cmd[4] = 0;  // NOP
		cmd[5] = 0;  // NOP
	}
	// write cmd queue
	for (i = 0; i < 6; i = i+1) {
		bm_write32(bmdi, base_addr_bist + 0x40 + i * 4, cmd[i]);
		if (ddr_type == LP4_2CH_1PHYD) {
			bm_write32(bmdi, base_addr_bist + 0x20000 + 0x40 + i * 4, cmd[i]);
		}
	}
	// specified DDR space
	bm_write32(bmdi, base_addr_bist + 0x10, 0x00000000);
	bm_write32(bmdi, base_addr_bist + 0x14, 0xffffffff);
	if (ddr_type == LP4_2CH_1PHYD) {
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x10, 0x00000000);
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x14, 0xffffffff);
	}
	// specified AXI address step
	//bm_write32(bmdi, base_addr_bist + 0x18, 0x00000008);
	if (ddr_type == LP4_2CH_1PHYD) {
		rddata = bm_read32(bmdi, base_addr_bist + 0x18);
		rddata = modified_bits_by_value(rddata, 4, 11, 0);
		if (rank == 1) {//bist at rank0
		rddata = modified_bits_by_value(rddata, 11, 21, 16); //rank_at_axi
		rddata = modified_bits_by_value(rddata, 0, 24, 24);  //rank 0
		} else {
		if (rank == 2) {//bist at rank1
			rddata = modified_bits_by_value(rddata, 11, 21, 16); //rank_at_axi
			rddata = modified_bits_by_value(rddata, 1, 24, 24);  //rank 1
		} else {// bist at rank0 and rank1
			rddata = modified_bits_by_value(rddata, 0, 21, 16); //rank_at_axi
		}
		}
		bm_write32(bmdi, base_addr_bist + 0x18, rddata);
		bm_write32(bmdi, base_addr_bist + 0x20000 + 0x18, rddata);
	} else {
		bm_write32(bmdi, base_addr_bist + 0x18, 0x00000008);
	}
	//`test_stream = "cvx32_bist_rdlvl_init done";
	//KC_MSG(get_name(), "set init_complete = 1 ...", UVM_NONE);
}
#endif
void cvx32_pinmux(struct bm_device_info *bmdi, uint32_t base_addr_phyd)
{
	uint32_t rddata;
	int i = 0;
	//aspi_cvx32_pinmux() need update too
	//`test_stream= "-4E- cvx32_pinmux";
	#ifdef DDR_2_RANK
		//param_phyd_swap_ca12   [4:     0]
		//param_phyd_swap_ca13   [12:    8]
		//param_phyd_swap_ca14   [20:   16]
		//param_phyd_swap_ca15   [28:   24]
		rddata = 0x1F1E1B1A;
		bm_write32(bmdi, 0x000C  + base_addr_phyd,  rddata);
		rddata = bm_read32(bmdi, 0x0010  + base_addr_phyd);
		rddata = modified_bits_by_value(rddata, 29, 4, 0);
		//rddata = {rddata[31:5], 5'd29};
		bm_write32(bmdi, 0x0010  + base_addr_phyd,  rddata);
		//param_phya_reg_tx_byte0_en_multi_rank	0	0
		for (i = 0; i < 4; i = i + 1) {
			rddata = bm_read32(bmdi, 0x0210 + i * 0x20 + base_addr_phyd);
			rddata = modified_bits_by_value(rddata, 1, 0, 0);
			bm_write32(bmdi, 0x0210 + i * 0x20 + base_addr_phyd,  rddata);
		}

	#else
		//param_phyd_swap_cke0	1	0
		//param_phyd_swap_cke1	3	2
		//param_phyd_swap_cs0	5	4
		//param_phyd_swap_cs1	7	6
		rddata = 0x00000080;
		bm_write32(bmdi, 0x0018  + base_addr_phyd,  rddata);
		//`test_stream = "[KC_DBG] pin mux not DDR_2_RANK";
	#endif // RANK_2
}
#pragma GCC diagnostic pop
