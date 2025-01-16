#include "bm_common.h"
#include "SGTPUV8_perf.h"
#include "bm_io.h"

void SGTPUV8_enable_tpu_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor, int core_id)
{
	int size = perf_monitor->buffer_size;
	u64 start_addr = perf_monitor->buffer_start_addr;
	u64 end_addr = start_addr + size;
	u32 value = 0;

	PR_TRACE("SGTPUV8 enable  tpu perf monitor size = 0x%x, start addr = 0x%llx, end addr = 0x%llx\n",
					 size, start_addr, end_addr);
	// set start address low 15bit, 0x170 bit [17:31]
	value = tpu_reg_read_idx(bmdi, 0x170, core_id);
	value &= ~(0x7fff << 17);
	value |= (start_addr & 0x7fff) << 17;
	tpu_reg_write_idx(bmdi, 0x170, value, core_id);

	// set start address high 20bit, 0x174 bit [0:19]
	value = tpu_reg_read_idx(bmdi, 0x174, core_id);
	value &= ~(0xfffff << 0);
	value |= (start_addr >> 15 & 0xfffff) << 0;
	tpu_reg_write_idx(bmdi, 0x174, value, core_id);

	// set end address low 12bit, 0x174 bit [20:31]
	value = tpu_reg_read_idx(bmdi, 0x174, core_id);
	value &= ~(0xfff << 20);
	value |= (end_addr & 0xfff) << 20;
	tpu_reg_write_idx(bmdi, 0x174, value, core_id);

	// set end address high 23bit, 0x178 bit [0:22]
	value = tpu_reg_read_idx(bmdi, 0x178, core_id);
	value &= ~(0x7fffff << 0);
	value |= (end_addr >> 12 & 0x7fffff) << 0;
	tpu_reg_write_idx(bmdi, 0x178, value, core_id);

	// set config_cmpt_en 0x178 bit[23] = 1, set_config_cmt_val as 1 0x178 bit[24:31] = 0x1
	// bit[23:31] = 0x3;
	value = tpu_reg_read_idx(bmdi, 0x178, core_id);
	value &= ~(0x1ff << 23);
	value |= 0x3 << 23;
	tpu_reg_write_idx(bmdi, 0x178, value, core_id);

	// set_conifg_cmtval as 1 0x17c bit[0:7] = 0, //set cfg_rd_instr_en =1 0x17c bit[8] = 1;
	// set cfg_rd_instr_stall_en =1 0x17c bit[9] = 1;
	// set cfg_wr_instr_en =1 0x17c bit[10] = 1;
	// bit [0:10] = 0x7 << 8
	value = tpu_reg_read_idx(bmdi, 0x17c, core_id);
	value &= ~(0x7ff << 0);
	value |= 0x7 << 8;
	tpu_reg_write_idx(bmdi, 0x17c, value, core_id);

	// enable tpu monitor 0x170 bit[16] = 1;
	value = tpu_reg_read_idx(bmdi, 0x170, core_id);
	value &= ~(0x1 << 16);
	value |= 0x1 << 16;
	tpu_reg_write_idx(bmdi, 0x170, value, core_id);
}

void SGTPUV8_disable_tpu_perf_monitor(struct bm_device_info *bmdi, int core_id)
{
	u32 value = 0;

	value = tpu_reg_read_idx(bmdi, 0x170, core_id);
	value &= 0xffff;
	tpu_reg_write_idx(bmdi, 0x170, value, core_id);

	value = tpu_reg_read_idx(bmdi, 0x174, core_id);
	value &= 0x0;
	tpu_reg_write_idx(bmdi, 0x174, value, core_id);

	value = tpu_reg_read_idx(bmdi, 0x178, core_id);
	value &= 0x0;
	tpu_reg_write_idx(bmdi, 0x178, value, core_id);

	value = tpu_reg_read_idx(bmdi, 0x17c, core_id);
	value &= 0xfffff800;
	tpu_reg_write_idx(bmdi, 0x17c, value, core_id);
}

void SGTPUV8_enable_gdma_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor, int core_id)
{
	int size = perf_monitor->buffer_size;
	u64 start_addr = perf_monitor->buffer_start_addr;
	u64 end_addr = start_addr + size;

	PR_TRACE("SGTPUV8 enable  gdma perf monitor size = 0x%x, start addr = 0x%llx, end addr = 0x%llx\n",
					 size, start_addr, end_addr);

	gdma_reg_write_idx(bmdi, 0x108, (start_addr >> 7), core_id);
	gdma_reg_write_idx(bmdi, 0x10c, (end_addr >> 7), core_id);
	gdma_reg_write_idx(bmdi, 0x100, gdma_reg_read_idx(bmdi, 0x100, core_id) | (1 << 2), core_id);
}

void SGTPUV8_disable_gdma_perf_monitor(struct bm_device_info *bmdi, int core_id)
{
	gdma_reg_write_idx(bmdi, 0x100, gdma_reg_read_idx(bmdi, 0x100, core_id) & ~(1 << 2), core_id);
	gdma_reg_write_idx(bmdi, 0x108, 0x0, core_id);
	gdma_reg_write_idx(bmdi, 0x10c, 0x0, core_id);
}
