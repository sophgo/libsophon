#include <bm_io.h>
#include <ddr_sys.h>
#include <ddr_init.h>

uint32_t ddr_data_rate = 4266;


#ifdef DDR_2_RANK

void ddrc_init(struct bm_device_info *bmdi, uint32_t base_addr_ctrl, int ddr_type)
{
	bm_write32(bmdi,base_addr_ctrl + 0xc, 0x13784B10);
		// PATCH0.use_blk_extend:0:2:=0x0
		// PATCH0.dis_auto_ref_cnt_fix:2:1:=0x0
		// PATCH0.dis_auto_ref_algn_to_8:3:1:=0x0
		// PATCH0.starve_stall_at_dfi_ctrlupd:4:1:=0x1
		// PATCH0.starve_stall_at_abr:5:1:=0x0
		// PATCH0.dis_rdwr_switch_at_abr:6:1:=0x0
		// PATCH0.dfi_wdata_same_to_axi:7:1:=0x0
		// PATCH0.pagematch_limit_threshold:8:3=0x3
		// PATCH0.dis_ram_rd_reduction:11:1=0x1
		// PATCH0.qos_sel:12:2:=0x0
		// PATCH0.timeout_wait_wr_mode:14:1:=0x1
		// PATCH0.burst_rdwr_xpi:16:4:=0x8
		// PATCH0.always_critical_when_urgent_hpr:20:1:=0x1
		// PATCH0.always_critical_when_urgent_lpr:21:1:=0x1
		// PATCH0.always_critical_when_urgent_wr:22:1:=0x1
		// PATCH0.disable_hif_rcmd_stall_path:24:1:=0x1
		// PATCH0.disable_hif_wcmd_stall_path:25:1:=0x1
		// PATCH0.mr_in_rdwr:28:1:=0x1
		// PATCH0.derate_sys_en:29:1:=0x0
		// PATCH0.ref_4x_sys_high_temp:30:1:=0x0
		// PATCH0.in_order_accept:31:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x44, 0x08000000);
		// PATCH1.ref_adv_stop_threshold:0:7:=0x0
		// PATCH1.ref_adv_dec_threshold:8:7:=0x0
		// PATCH1.ref_adv_max:16:7:=0x0
		// PATCH1.burst_rdwr_wr_xpi:24:4:=0x8
		// PATCH1.use_blk_extend_wr:28:2:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x4c, 0x2A162925);
		// PATCH3.t_rd2mrw:0:8:=0x0
		// PATCH3.t_rda2mrw:8:8:=0x0
		// PATCH3.t_wr2mrw:16:8:=0x0
		// PATCH3.t_wra2mrw:24:8:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x148, 0xAEB01D04);
		// PATCH4.t_rd2mrr:0:4:=0x0
		// PATCH4.t_xp_mrri:8:6:=0x0
		// PATCH4.t_phyd_rden:16:6=0x0
		// PATCH4.phyd_rd_clk_stop:23:1=0x0
		// PATCH4.t_phyd_wren:24:6=0x0
		// PATCH4.phyd_wr_clk_stop:31:1=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x6c, 0x41000003);
		// PATCH5.vpr_fix:0:1=0x1
		// PATCH5.vpw_fix:1:1=0x1
		// PATCH5.rcam_entry_del_length:4:2=0x0
		// PATCH5.hif_rdata_rrb_en:7:1=0x0
		// PATCH5.auto_cam_size:24:1=0x1
		// PATCH5.burst_rdwr_xpi_min:28:4=0x4

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x0, 0x83081020);
	bm_write32(bmdi,base_addr_ctrl + 0x20, 0x00001303);
	bm_write32(bmdi,base_addr_ctrl + 0x24, 0x3F917FA0);
	bm_write32(bmdi,base_addr_ctrl + 0x2c, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x30, 0x00000100);
	bm_write32(bmdi,base_addr_ctrl + 0x34, 0x014F0005);
	bm_write32(bmdi,base_addr_ctrl + 0x38, 0x00d00002);
	bm_write32(bmdi,base_addr_ctrl + 0x50, 0x802103f6);
	bm_write32(bmdi,base_addr_ctrl + 0x60, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x64, 0x820800CB);
	bm_write32(bmdi,base_addr_ctrl + 0x68, 0x00600000);
	bm_write32(bmdi,base_addr_ctrl + 0xc0, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0xc4, 0x00000000);
#ifdef DDR_INIT_SPEED_UP
	bm_write32(bmdi,base_addr_ctrl + 0xd0, 0x00010003);
	bm_write32(bmdi,base_addr_ctrl + 0xd4, 0x000B0000);
	bm_write32(bmdi,base_addr_ctrl + 0xd8, 0x00000105);
#else
	bm_write32(bmdi,base_addr_ctrl + 0xd0, 0x00010043);
	bm_write32(bmdi,base_addr_ctrl + 0xd4, 0x00130000);
	bm_write32(bmdi,base_addr_ctrl + 0xd8, 0x00000305);
#endif
	bm_write32(bmdi,base_addr_ctrl + 0xdc, 0x007C003F);
	bm_write32(bmdi,base_addr_ctrl + 0xe0, 0x00E00000);
	bm_write32(bmdi,base_addr_ctrl + 0xe4, 0x00030007);
	bm_write32(bmdi,base_addr_ctrl + 0xe8, 0x0002004D);
	bm_write32(bmdi,base_addr_ctrl + 0xec, 0x00230019);
	bm_write32(bmdi,base_addr_ctrl + 0xf4, 0x0000FE4F);
	bm_write32(bmdi,base_addr_ctrl + 0x100, 0x2120242D);
	bm_write32(bmdi,base_addr_ctrl + 0x104, 0x00080840);
	bm_write32(bmdi,base_addr_ctrl + 0x108, 0x09141719);
	bm_write32(bmdi,base_addr_ctrl + 0x10c, 0x00F0F000);
	bm_write32(bmdi,base_addr_ctrl + 0x110, 0x14040814);
	bm_write32(bmdi,base_addr_ctrl + 0x114, 0x02061010);
	bm_write32(bmdi,base_addr_ctrl + 0x118, 0x0602000A);
	bm_write32(bmdi,base_addr_ctrl + 0x11c, 0x00000602);
	bm_write32(bmdi,base_addr_ctrl + 0x130, 0x00020000);
	bm_write32(bmdi,base_addr_ctrl + 0x134, 0x1E100002);
	bm_write32(bmdi,base_addr_ctrl + 0x138, 0x0000019E);
	bm_write32(bmdi,base_addr_ctrl + 0x180, 0xC42B0020);
	bm_write32(bmdi,base_addr_ctrl + 0x184, 0x03600001);

	// phyd related
	bm_write32(bmdi,base_addr_ctrl + 0x190, 0x04a08a08);
		// DFITMG0.dfi_t_ctrl_delay:24:5:=0x4
		// DFITMG0.dfi_rddata_use_dfi_phy_clk:23:1:=0x1
		// DFITMG0.dfi_t_rddata_en:16:7:=0x20
		// DFITMG0.dfi_wrdata_use_dfi_phy_clk:15:1:=0x1
		// DFITMG0.dfi_tphy_wrdata:8:6:=0xa
		// DFITMG0.dfi_tphy_wrlat:0:6:=0x8
	bm_write32(bmdi,base_addr_ctrl + 0x194, 0x000e0404);
		// DFITMG1.dfi_t_cmd_lat:28:4:=0x0
		// DFITMG1.dfi_t_parin_lat:24:2:=0x0
		// DFITMG1.dfi_t_wrdata_delay:16:5:=0xe
		// DFITMG1.dfi_t_dram_clk_disable:8:5:=0x2
		// DFITMG1.dfi_t_dram_clk_enable:0:5:=0x2
	bm_write32(bmdi,base_addr_ctrl + 0x198, 0x07c13121);
		// DFILPCFG0.dfi_tlp_resp:24:5:=0x7
		// DFILPCFG0.dfi_lp_wakeup_dpd:20:4:=0xc
		// DFILPCFG0.dfi_lp_en_dpd:16:1:=0x1
		// DFILPCFG0.dfi_lp_wakeup_sr:12:4:=0x3
		// DFILPCFG0.dfi_lp_en_sr:8:1:=0x1
		// DFILPCFG0.dfi_lp_wakeup_pd:4:4:=0x2
		// DFILPCFG0.dfi_lp_en_pd:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x19c, 0x00000021);
		// DFILPCFG1.dfi_lp_wakeup_mpsm:4:4:=0x2
		// DFILPCFG1.dfi_lp_en_mpsm:0:1:=0x1

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x1a0, 0xC0400018);
	bm_write32(bmdi,base_addr_ctrl + 0x1a4, 0x00FE00FF);
	bm_write32(bmdi,base_addr_ctrl + 0x1a8, 0x80000000);
	bm_write32(bmdi,base_addr_ctrl + 0x1b0, 0x000002C1);
	bm_write32(bmdi,base_addr_ctrl + 0x1b4, 0x00001C04);
		// DFITMG2.dfi_tphy_rdcslat:8:7:= DFITMG0.dfi_t_rddata_en - 0x4
		// DFITMG2.dfi_tphy_wrcslat:0:6:= DFITMG0.dfi_tphy_wrlat - 0x4
	bm_write32(bmdi,base_addr_ctrl + 0x1c0, 0x00000007);
	bm_write32(bmdi,base_addr_ctrl + 0x1c4, 0x80000001);

	// address map, auto gen.
	// CS0 = AXI[11]
	bm_write32(bmdi,base_addr_ctrl + 0x200, 0x00001F03);
	bm_write32(bmdi,base_addr_ctrl + 0x204, 0x00080808);
	bm_write32(bmdi,base_addr_ctrl + 0x208, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x20c, 0x1F000000);
	bm_write32(bmdi,base_addr_ctrl + 0x210, 0x00001F1F);
	bm_write32(bmdi,base_addr_ctrl + 0x214, 0x070F0707);
	bm_write32(bmdi,base_addr_ctrl + 0x218, 0x07070707);
	bm_write32(bmdi,base_addr_ctrl + 0x21c, 0x00000F07);
	bm_write32(bmdi,base_addr_ctrl + 0x220, 0x00003F3F);
	bm_write32(bmdi,base_addr_ctrl + 0x224, 0x07070707);
	bm_write32(bmdi,base_addr_ctrl + 0x228, 0x07070707);
	bm_write32(bmdi,base_addr_ctrl + 0x22c, 0x001F1F07);

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x250, 0x00003F85);
		// SCHED.opt_vprw_sch:31:1:=0x0
		// SCHED.rdwr_idle_gap:24:7:=0x0
		// SCHED.go2critical_hysteresis:16:8:=0x0
		// SCHED.lpddr4_opt_act_timing:15:1:=0x0
		// SCHED.lpr_num_entries:8:7:=0x1f
		// SCHED.autopre_rmw:7:1:=0x1
		// SCHED.dis_opt_ntt_by_pre:6:1:=0x0
		// SCHED.dis_opt_ntt_by_act:5:1:=0x0
		// SCHED.opt_wrcam_fill_level:4:1:=0x0
		// SCHED.rdwr_switch_policy_sel:3:1:=0x0
		// SCHED.pageclose:2:1:=0x1
		// SCHED.prefer_write:1:1:=0x0
		// SCHED.dis_opt_wrecc_collision_flush:0:1:=0x1

	bm_write32(bmdi,base_addr_ctrl + 0x254, 0x00000000);
		// SCHED1.page_hit_limit_rd:28:3:=0x0
		// SCHED1.page_hit_limit_wr:24:3:=0x0
		// SCHED1.visible_window_limit_rd:20:3:=0x0
		// SCHED1.visible_window_limit_wr:16:3:=0x0
		// SCHED1.delay_switch_write:12:4:=0x0
		// SCHED1.pageclose_timer:0:8:=0x0

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x25c, 0x10000218);
		// PERFHPR1.hpr_xact_run_length:24:8:=0x20
		// PERFHPR1.hpr_max_starve:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x264, 0x10000218);
		// PERFLPR1.lpr_xact_run_length:24:8:=0x20
		// PERFLPR1.lpr_max_starve:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x26c, 0x10000218);
		// PERFWR1.w_xact_run_length:24:8:=0x20
		// PERFWR1.w_max_starve:0:16:=0x1a8

	bm_write32(bmdi,base_addr_ctrl + 0x300, 0x00000000);
		// DBG0.dis_max_rank_wr_opt:7:1:=0x0
		// DBG0.dis_max_rank_rd_opt:6:1:=0x0
		// DBG0.dis_collision_page_opt:4:1:=0x0
		// DBG0.dis_act_bypass:2:1:=0x0
		// DBG0.dis_rd_bypass:1:1:=0x0
		// DBG0.dis_wc:0:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x304, 0x00000000);
		// DBG1.dis_hif:1:1:=0x0
		// DBG1.dis_dq:0:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x30c, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x320, 0x00000001);
		// SWCTL.sw_done:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x36c, 0x00000000);
		// POISONCFG.rd_poison_intr_clr:24:1:=0x0
		// POISONCFG.rd_poison_intr_en:20:1:=0x0
		// POISONCFG.rd_poison_slverr_en:16:1:=0x0
		// POISONCFG.wr_poison_intr_clr:8:1:=0x0
		// POISONCFG.wr_poison_intr_en:4:1:=0x0
		// POISONCFG.wr_poison_slverr_en:0:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x400, 0x00000011);
		// PCCFG.dch_density_ratio:12:2:=0x0
		// PCCFG.bl_exp_mode:8:1:=0x0
		// PCCFG.pagematch_limit:4:1:=0x1
		// PCCFG.go2critical_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x404, 0x00006000);
		// PCFGR_0.rdwr_ordered_en:16:1:=0x0
		// PCFGR_0.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_0.rd_port_urgent_en:13:1:=0x1
		// PCFGR_0.rd_port_aging_en:12:1:=0x0
		// PCFGR_0.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_0.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x408, 0x00006000);
		// PCFGW_0.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_0.wr_port_urgent_en:13:1:=0x1
		// PCFGW_0.wr_port_aging_en:12:1:=0x0
		// PCFGW_0.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x490, 0x00000001);
		// PCTRL_0.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x494, 0x00000007);
		// PCFGQOS0_0.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_0.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_0.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_0.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_0.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x498, 0x0000006a);
		// PCFGQOS1_0.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_0.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x49c, 0x00000e07);
		// PCFGWQOS0_0.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_0.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_0.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_0.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_0.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x4a0, 0x01a801a8);
		// PCFGWQOS1_0.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_0.wqos_map_timeout1:0:16:=0x1a8
	bm_write32(bmdi,base_addr_ctrl + 0x4b4, 0x00006000);
		// PCFGR_1.rdwr_ordered_en:16:1:=0x0
		// PCFGR_1.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_1.rd_port_urgent_en:13:1:=0x1
		// PCFGR_1.rd_port_aging_en:12:1:=0x0
		// PCFGR_1.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_1.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x4b8, 0x00006000);
		// PCFGW_1.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_1.wr_port_urgent_en:13:1:=0x1
		// PCFGW_1.wr_port_aging_en:12:1:=0x0
		// PCFGW_1.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x540, 0x00000001);
		// PCTRL_1.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x544, 0x00000007);
		// PCFGQOS0_1.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_1.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_1.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_1.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_1.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x548, 0x0000006a);
		// PCFGQOS1_1.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_1.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x54c, 0x00000e07);
		// PCFGWQOS0_1.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_1.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_1.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_1.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_1.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x550, 0x01a801a8);
		// PCFGWQOS1_1.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_1.wqos_map_timeout1:0:16:=0x1a8
	bm_write32(bmdi,base_addr_ctrl + 0x564, 0x00006000);
		// PCFGR_2.rdwr_ordered_en:16:1:=0x0
		// PCFGR_2.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_2.rd_port_urgent_en:13:1:=0x1
		// PCFGR_2.rd_port_aging_en:12:1:=0x0
		// PCFGR_2.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_2.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x568, 0x00006000);
		// PCFGW_2.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_2.wr_port_urgent_en:13:1:=0x1
		// PCFGW_2.wr_port_aging_en:12:1:=0x0
		// PCFGW_2.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x5f0, 0x00000001);
		// PCTRL_2.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x5f4, 0x00000007);
		// PCFGQOS0_2.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_2.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_2.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_2.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_2.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x5f8, 0x0000006a);
		// PCFGQOS1_2.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_2.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x5fc, 0x00000e07);
		// PCFGWQOS0_2.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_2.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_2.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_2.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_2.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x600, 0x01a801a8);
		// PCFGWQOS1_2.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_2.wqos_map_timeout1:0:16:=0x1a8
	bm_write32(bmdi,base_addr_ctrl + 0x614, 0x00006000);
		// PCFGR_3.rdwr_ordered_en:16:1:=0x0
		// PCFGR_3.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_3.rd_port_urgent_en:13:1:=0x1
		// PCFGR_3.rd_port_aging_en:12:1:=0x0
		// PCFGR_3.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_3.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x618, 0x00006000);
		// PCFGW_3.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_3.wr_port_urgent_en:13:1:=0x1
		// PCFGW_3.wr_port_aging_en:12:1:=0x0
		// PCFGW_3.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x6a0, 0x00000001);
		// PCTRL_3.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x6a4, 0x00000007);
		// PCFGQOS0_3.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_3.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_3.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_3.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_3.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x6a8, 0x0000006a);
		// PCFGQOS1_3.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_3.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x6ac, 0x00000e07);
		// PCFGWQOS0_3.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_3.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_3.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_3.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_3.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x6b0, 0x01a801a8);
		// PCFGWQOS1_3.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_3.wqos_map_timeout1:0:16:=0x1a8
}
#else
void ddrc_init(struct bm_device_info *bmdi, uint32_t base_addr_ctrl, int ddr_type)
{
	bm_write32(bmdi,base_addr_ctrl + 0xc, 0x13784B10);
		// PATCH0.use_blk_extend:0:2:=0x0
		// PATCH0.dis_auto_ref_cnt_fix:2:1:=0x0
		// PATCH0.dis_auto_ref_algn_to_8:3:1:=0x0
		// PATCH0.starve_stall_at_dfi_ctrlupd:4:1:=0x1
		// PATCH0.starve_stall_at_abr:5:1:=0x0
		// PATCH0.dis_rdwr_switch_at_abr:6:1:=0x0
		// PATCH0.dfi_wdata_same_to_axi:7:1:=0x0
		// PATCH0.pagematch_limit_threshold:8:3=0x3
		// PATCH0.dis_ram_rd_reduction:11:1=0x1
		// PATCH0.qos_sel:12:2:=0x0
		// PATCH0.timeout_wait_wr_mode:14:1:=0x1
		// PATCH0.burst_rdwr_xpi:16:4:=0x8
		// PATCH0.always_critical_when_urgent_hpr:20:1:=0x1
		// PATCH0.always_critical_when_urgent_lpr:21:1:=0x1
		// PATCH0.always_critical_when_urgent_wr:22:1:=0x1
		// PATCH0.disable_hif_rcmd_stall_path:24:1:=0x1
		// PATCH0.disable_hif_wcmd_stall_path:25:1:=0x1
		// PATCH0.mr_in_rdwr:28:1:=0x1
		// PATCH0.derate_sys_en:29:1:=0x0
		// PATCH0.ref_4x_sys_high_temp:30:1:=0x0
		// PATCH0.in_order_accept:31:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x44, 0x08000000);
		// PATCH1.ref_adv_stop_threshold:0:7:=0x0
		// PATCH1.ref_adv_dec_threshold:8:7:=0x0
		// PATCH1.ref_adv_max:16:7:=0x0
		// PATCH1.burst_rdwr_wr_xpi:24:4:=0x8
		// PATCH1.use_blk_extend_wr:28:2:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x4c, 0x2A162925);
		// PATCH3.t_rd2mrw:0:8:=0x0
		// PATCH3.t_rda2mrw:8:8:=0x0
		// PATCH3.t_wr2mrw:16:8:=0x0
		// PATCH3.t_wra2mrw:24:8:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x148, 0xAEB01D04);
		// PATCH4.t_rd2mrr:0:4:=0x0
		// PATCH4.t_xp_mrri:8:6:=0x0
		// PATCH4.t_phyd_rden:16:6=0x0
		// PATCH4.phyd_rd_clk_stop:23:1=0x0
		// PATCH4.t_phyd_wren:24:6=0x0
		// PATCH4.phyd_wr_clk_stop:31:1=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x6c, 0x41000003);
		// PATCH5.vpr_fix:0:1=0x1
		// PATCH5.vpw_fix:1:1=0x1
		// PATCH5.rcam_entry_del_length:4:2=0x0
		// PATCH5.hif_rdata_rrb_en:7:1=0x0
		// PATCH5.auto_cam_size:24:1=0x1
		// PATCH5.burst_rdwr_xpi_min:28:4=0x4

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x0, 0x81081020);
	bm_write32(bmdi,base_addr_ctrl + 0x20, 0x00001303);
	bm_write32(bmdi,base_addr_ctrl + 0x24, 0x3F917FA0);
	bm_write32(bmdi,base_addr_ctrl + 0x2c, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x30, 0x00000100);
	bm_write32(bmdi,base_addr_ctrl + 0x34, 0x014F0005);
	bm_write32(bmdi,base_addr_ctrl + 0x38, 0x00d00002);
	bm_write32(bmdi,base_addr_ctrl + 0x50, 0x802103f6);
	bm_write32(bmdi,base_addr_ctrl + 0x60, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x64, 0x820800CB);
	bm_write32(bmdi,base_addr_ctrl + 0x68, 0x00600000);
	bm_write32(bmdi,base_addr_ctrl + 0xc0, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0xc4, 0x00000000);
#ifdef DDR_INIT_SPEED_UP
	bm_write32(bmdi,base_addr_ctrl + 0xd0, 0x00010003);
	bm_write32(bmdi,base_addr_ctrl + 0xd4, 0x000B0000);
	bm_write32(bmdi,base_addr_ctrl + 0xd8, 0x00000105);
#else
	bm_write32(bmdi,base_addr_ctrl + 0xd0, 0x00010043);
	bm_write32(bmdi,base_addr_ctrl + 0xd4, 0x00130000);
	bm_write32(bmdi,base_addr_ctrl + 0xd8, 0x00000305);
#endif
	bm_write32(bmdi,base_addr_ctrl + 0xdc, 0x007C003F);
	bm_write32(bmdi,base_addr_ctrl + 0xe0, 0x00E00000);
	bm_write32(bmdi,base_addr_ctrl + 0xe4, 0x00030007);
	bm_write32(bmdi,base_addr_ctrl + 0xe8, 0x0002004D);
	bm_write32(bmdi,base_addr_ctrl + 0xec, 0x00230019);
	bm_write32(bmdi,base_addr_ctrl + 0xf4, 0x0000FE4F);
	bm_write32(bmdi,base_addr_ctrl + 0x100, 0x2120242D);
	bm_write32(bmdi,base_addr_ctrl + 0x104, 0x00080840);
	bm_write32(bmdi,base_addr_ctrl + 0x108, 0x09141719);
	bm_write32(bmdi,base_addr_ctrl + 0x10c, 0x00F0F000);
	bm_write32(bmdi,base_addr_ctrl + 0x110, 0x14040814);
	bm_write32(bmdi,base_addr_ctrl + 0x114, 0x02061010);
	bm_write32(bmdi,base_addr_ctrl + 0x118, 0x0602000A);
	bm_write32(bmdi,base_addr_ctrl + 0x11c, 0x00000602);
	bm_write32(bmdi,base_addr_ctrl + 0x130, 0x00020000);
	bm_write32(bmdi,base_addr_ctrl + 0x134, 0x1E100002);
	bm_write32(bmdi,base_addr_ctrl + 0x138, 0x0000019E);
	bm_write32(bmdi,base_addr_ctrl + 0x180, 0xC42B0020);
	bm_write32(bmdi,base_addr_ctrl + 0x184, 0x03600001);

	// phyd related
	bm_write32(bmdi,base_addr_ctrl + 0x190, 0x04a08a08);
		// DFITMG0.dfi_t_ctrl_delay:24:5:=0x4
		// DFITMG0.dfi_rddata_use_dfi_phy_clk:23:1:=0x1
		// DFITMG0.dfi_t_rddata_en:16:7:=0x20
		// DFITMG0.dfi_wrdata_use_dfi_phy_clk:15:1:=0x1
		// DFITMG0.dfi_tphy_wrdata:8:6:=0xa
		// DFITMG0.dfi_tphy_wrlat:0:6:=0x8
	bm_write32(bmdi,base_addr_ctrl + 0x194, 0x000e0404);
		// DFITMG1.dfi_t_cmd_lat:28:4:=0x0
		// DFITMG1.dfi_t_parin_lat:24:2:=0x0
		// DFITMG1.dfi_t_wrdata_delay:16:5:=0xe
		// DFITMG1.dfi_t_dram_clk_disable:8:5:=0x2
		// DFITMG1.dfi_t_dram_clk_enable:0:5:=0x2
	bm_write32(bmdi,base_addr_ctrl + 0x198, 0x07c13121);
		// DFILPCFG0.dfi_tlp_resp:24:5:=0x7
		// DFILPCFG0.dfi_lp_wakeup_dpd:20:4:=0xc
		// DFILPCFG0.dfi_lp_en_dpd:16:1:=0x1
		// DFILPCFG0.dfi_lp_wakeup_sr:12:4:=0x3
		// DFILPCFG0.dfi_lp_en_sr:8:1:=0x1
		// DFILPCFG0.dfi_lp_wakeup_pd:4:4:=0x2
		// DFILPCFG0.dfi_lp_en_pd:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x19c, 0x00000021);
		// DFILPCFG1.dfi_lp_wakeup_mpsm:4:4:=0x2
		// DFILPCFG1.dfi_lp_en_mpsm:0:1:=0x1

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x1a0, 0xC0400018);
	bm_write32(bmdi,base_addr_ctrl + 0x1a4, 0x00FE00FF);
	bm_write32(bmdi,base_addr_ctrl + 0x1a8, 0x80000000);
	bm_write32(bmdi,base_addr_ctrl + 0x1b0, 0x000002C1);
	bm_write32(bmdi,base_addr_ctrl + 0x1b4, 0x00001C04);
		// DFITMG2.dfi_tphy_rdcslat:8:7:= DFITMG0.dfi_t_rddata_en - 0x4
		// DFITMG2.dfi_tphy_wrcslat:0:6:= DFITMG0.dfi_tphy_wrlat - 0x4
	bm_write32(bmdi,base_addr_ctrl + 0x1c0, 0x00000007);
	bm_write32(bmdi,base_addr_ctrl + 0x1c4, 0x80000001);

	// address map, auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x200, 0x00001F1F);
	bm_write32(bmdi,base_addr_ctrl + 0x204, 0x00070707);
	bm_write32(bmdi,base_addr_ctrl + 0x208, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x20c, 0x1F000000);
	bm_write32(bmdi,base_addr_ctrl + 0x210, 0x00001F1F);
	bm_write32(bmdi,base_addr_ctrl + 0x214, 0x060F0606);
	bm_write32(bmdi,base_addr_ctrl + 0x218, 0x06060606);
	bm_write32(bmdi,base_addr_ctrl + 0x21c, 0x00000F06);
	bm_write32(bmdi,base_addr_ctrl + 0x220, 0x00003F3F);
	bm_write32(bmdi,base_addr_ctrl + 0x224, 0x06060606);
	bm_write32(bmdi,base_addr_ctrl + 0x228, 0x06060606);
	bm_write32(bmdi,base_addr_ctrl + 0x22c, 0x001F1F06);

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x250, 0x00003F85);
		// SCHED.opt_vprw_sch:31:1:=0x0
		// SCHED.rdwr_idle_gap:24:7:=0x0
		// SCHED.go2critical_hysteresis:16:8:=0x0
		// SCHED.lpddr4_opt_act_timing:15:1:=0x0
		// SCHED.lpr_num_entries:8:7:=0x1f
		// SCHED.autopre_rmw:7:1:=0x1
		// SCHED.dis_opt_ntt_by_pre:6:1:=0x0
		// SCHED.dis_opt_ntt_by_act:5:1:=0x0
		// SCHED.opt_wrcam_fill_level:4:1:=0x0
		// SCHED.rdwr_switch_policy_sel:3:1:=0x0
		// SCHED.pageclose:2:1:=0x1
		// SCHED.prefer_write:1:1:=0x0
		// SCHED.dis_opt_wrecc_collision_flush:0:1:=0x1

	bm_write32(bmdi,base_addr_ctrl + 0x254, 0x00000000);
		// SCHED1.page_hit_limit_rd:28:3:=0x0
		// SCHED1.page_hit_limit_wr:24:3:=0x0
		// SCHED1.visible_window_limit_rd:20:3:=0x0
		// SCHED1.visible_window_limit_wr:16:3:=0x0
		// SCHED1.delay_switch_write:12:4:=0x0
		// SCHED1.pageclose_timer:0:8:=0x0

	// auto gen.
	bm_write32(bmdi,base_addr_ctrl + 0x25c, 0x10000218);
		// PERFHPR1.hpr_xact_run_length:24:8:=0x20
		// PERFHPR1.hpr_max_starve:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x264, 0x10000218);
		// PERFLPR1.lpr_xact_run_length:24:8:=0x20
		// PERFLPR1.lpr_max_starve:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x26c, 0x10000218);
		// PERFWR1.w_xact_run_length:24:8:=0x20
		// PERFWR1.w_max_starve:0:16:=0x1a8

	bm_write32(bmdi,base_addr_ctrl + 0x300, 0x00000000);
		// DBG0.dis_max_rank_wr_opt:7:1:=0x0
		// DBG0.dis_max_rank_rd_opt:6:1:=0x0
		// DBG0.dis_collision_page_opt:4:1:=0x0
		// DBG0.dis_act_bypass:2:1:=0x0
		// DBG0.dis_rd_bypass:1:1:=0x0
		// DBG0.dis_wc:0:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x304, 0x00000000);
		// DBG1.dis_hif:1:1:=0x0
		// DBG1.dis_dq:0:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x30c, 0x00000000);
	bm_write32(bmdi,base_addr_ctrl + 0x320, 0x00000001);
		// SWCTL.sw_done:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x36c, 0x00000000);
		// POISONCFG.rd_poison_intr_clr:24:1:=0x0
		// POISONCFG.rd_poison_intr_en:20:1:=0x0
		// POISONCFG.rd_poison_slverr_en:16:1:=0x0
		// POISONCFG.wr_poison_intr_clr:8:1:=0x0
		// POISONCFG.wr_poison_intr_en:4:1:=0x0
		// POISONCFG.wr_poison_slverr_en:0:1:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x400, 0x00000011);
		// PCCFG.dch_density_ratio:12:2:=0x0
		// PCCFG.bl_exp_mode:8:1:=0x0
		// PCCFG.pagematch_limit:4:1:=0x1
		// PCCFG.go2critical_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x404, 0x00006000);
		// PCFGR_0.rdwr_ordered_en:16:1:=0x0
		// PCFGR_0.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_0.rd_port_urgent_en:13:1:=0x1
		// PCFGR_0.rd_port_aging_en:12:1:=0x0
		// PCFGR_0.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_0.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x408, 0x00006000);
		// PCFGW_0.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_0.wr_port_urgent_en:13:1:=0x1
		// PCFGW_0.wr_port_aging_en:12:1:=0x0
		// PCFGW_0.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x490, 0x00000001);
		// PCTRL_0.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x494, 0x00000007);
		// PCFGQOS0_0.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_0.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_0.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_0.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_0.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x498, 0x0000006a);
		// PCFGQOS1_0.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_0.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x49c, 0x00000e07);
		// PCFGWQOS0_0.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_0.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_0.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_0.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_0.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x4a0, 0x01a801a8);
		// PCFGWQOS1_0.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_0.wqos_map_timeout1:0:16:=0x1a8
	bm_write32(bmdi,base_addr_ctrl + 0x4b4, 0x00006000);
		// PCFGR_1.rdwr_ordered_en:16:1:=0x0
		// PCFGR_1.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_1.rd_port_urgent_en:13:1:=0x1
		// PCFGR_1.rd_port_aging_en:12:1:=0x0
		// PCFGR_1.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_1.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x4b8, 0x00006000);
		// PCFGW_1.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_1.wr_port_urgent_en:13:1:=0x1
		// PCFGW_1.wr_port_aging_en:12:1:=0x0
		// PCFGW_1.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x540, 0x00000001);
		// PCTRL_1.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x544, 0x00000007);
		// PCFGQOS0_1.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_1.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_1.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_1.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_1.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x548, 0x0000006a);
		// PCFGQOS1_1.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_1.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x54c, 0x00000e07);
		// PCFGWQOS0_1.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_1.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_1.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_1.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_1.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x550, 0x01a801a8);
		// PCFGWQOS1_1.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_1.wqos_map_timeout1:0:16:=0x1a8
	bm_write32(bmdi,base_addr_ctrl + 0x564, 0x00006000);
		// PCFGR_2.rdwr_ordered_en:16:1:=0x0
		// PCFGR_2.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_2.rd_port_urgent_en:13:1:=0x1
		// PCFGR_2.rd_port_aging_en:12:1:=0x0
		// PCFGR_2.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_2.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x568, 0x00006000);
		// PCFGW_2.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_2.wr_port_urgent_en:13:1:=0x1
		// PCFGW_2.wr_port_aging_en:12:1:=0x0
		// PCFGW_2.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x5f0, 0x00000001);
		// PCTRL_2.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x5f4, 0x00000007);
		// PCFGQOS0_2.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_2.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_2.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_2.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_2.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x5f8, 0x0000006a);
		// PCFGQOS1_2.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_2.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x5fc, 0x00000e07);
		// PCFGWQOS0_2.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_2.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_2.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_2.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_2.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x600, 0x01a801a8);
		// PCFGWQOS1_2.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_2.wqos_map_timeout1:0:16:=0x1a8
	bm_write32(bmdi,base_addr_ctrl + 0x614, 0x00006000);
		// PCFGR_3.rdwr_ordered_en:16:1:=0x0
		// PCFGR_3.rd_port_pagematch_en:14:1:=0x1
		// PCFGR_3.rd_port_urgent_en:13:1:=0x1
		// PCFGR_3.rd_port_aging_en:12:1:=0x0
		// PCFGR_3.read_reorder_bypass_en:11:1:=0x0
		// PCFGR_3.rd_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x618, 0x00006000);
		// PCFGW_3.wr_port_pagematch_en:14:1:=0x1
		// PCFGW_3.wr_port_urgent_en:13:1:=0x1
		// PCFGW_3.wr_port_aging_en:12:1:=0x0
		// PCFGW_3.wr_port_priority:0:10:=0x0
	bm_write32(bmdi,base_addr_ctrl + 0x6a0, 0x00000001);
		// PCTRL_3.port_en:0:1:=0x1
	bm_write32(bmdi,base_addr_ctrl + 0x6a4, 0x00000007);
		// PCFGQOS0_3.rqos_map_region2:24:8:=0x0
		// PCFGQOS0_3.rqos_map_region1:20:4:=0x0
		// PCFGQOS0_3.rqos_map_region0:16:4:=0x0
		// PCFGQOS0_3.rqos_map_level2:8:8:=0x0
		// PCFGQOS0_3.rqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x6a8, 0x0000006a);
		// PCFGQOS1_3.rqos_map_timeoutr:16:16:=0x0
		// PCFGQOS1_3.rqos_map_timeoutb:0:16:=0x6a
	bm_write32(bmdi,base_addr_ctrl + 0x6ac, 0x00000e07);
		// PCFGWQOS0_3.wqos_map_region2:24:8:=0x0
		// PCFGWQOS0_3.wqos_map_region1:20:4:=0x0
		// PCFGWQOS0_3.wqos_map_region0:16:4:=0x0
		// PCFGWQOS0_3.wqos_map_level2:8:8:=0xe
		// PCFGWQOS0_3.wqos_map_level1:0:8:=0x7
	bm_write32(bmdi,base_addr_ctrl + 0x6b0, 0x01a801a8);
		// PCFGWQOS1_3.wqos_map_timeout2:16:16:=0x1a8
		// PCFGWQOS1_3.wqos_map_timeout1:0:16:=0x1a8
}
#endif

#if 0
void ctrl_init_high_patch(void)
{
	// enable auto PD/SR
	bm_write32(bmdi,0x08004000 + 0x30, 0x00000002);
	// enable auto ctrl_upd
	bm_write32(bmdi,0x08004000 + 0x1a0, 0x00400018);
	// enable clock gating
	bm_write32(bmdi,0x0800a000 + 0x14, 0x00000000);
	// change xpi to multi DDR burst
	//bm_write32(bmdi,0x08004000 + 0xc, 0x63786370);
}

void ctrl_init_low_patch(void)
{
	// disable auto PD/SR
	bm_write32(bmdi,0x08004000 + 0x30, 0x00000000);
	// disable auto ctrl_upd
	bm_write32(bmdi,0x08004000 + 0x1a0, 0xC0400018);
	// disable clock gating
	bm_write32(bmdi,0x0800a000 + 0x14, 0x00000fff);
	// change xpi to single DDR burst
	//bm_write32(bmdi,0x08004000 + 0xc, 0x63746371);
}

void ctrl_init_update_by_dram_size(uint8_t dram_cap_in_mbyte)
{
	uint8_t dram_cap_in_mbyte_per_dev;

	rddata = mmio_rd32(0x08004000 + 0x0);
	dram_cap_in_mbyte_per_dev = dram_cap_in_mbyte;
	dram_cap_in_mbyte_per_dev >>= (1 - get_bits_from_value(rddata, 13, 12)); // change sys cap to x16 cap
	dram_cap_in_mbyte_per_dev >>= (2 - get_bits_from_value(rddata, 31, 30)); // change x16 cap to device cap
	uartlog("dram_cap_in_mbyte_per_dev=%d\n", dram_cap_in_mbyte_per_dev);
	switch (dram_cap_in_mbyte_per_dev) {
	case 6:
		bm_write32(bmdi,0x08004000 + 0x64, 0x00820030);
		bm_write32(bmdi,0x08004000 + 0x120, 0x00000903);
		break;
	case 7:
		bm_write32(bmdi,0x08004000 + 0x64, 0x0082003B);
		bm_write32(bmdi,0x08004000 + 0x120, 0x00000903);
		break;
	case 8:
		bm_write32(bmdi,0x08004000 + 0x64, 0x00820056);
		bm_write32(bmdi,0x08004000 + 0x120, 0x00000904);
		break;
	case 9:
		bm_write32(bmdi,0x08004000 + 0x64, 0x0082008B);
		bm_write32(bmdi,0x08004000 + 0x120, 0x00000906);
		break;
	case 10:
		bm_write32(bmdi,0x08004000 + 0x64, 0x008200BB);
		bm_write32(bmdi,0x08004000 + 0x120, 0x00000907);
		break;
	}
	// toggle refresh_update_level
	bm_write32(bmdi,0x08004000 + 0x60, 0x00000002);
	bm_write32(bmdi,0x08004000 + 0x60, 0x00000000);
}
#endif
