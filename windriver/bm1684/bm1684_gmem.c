#include "../bm_common.h"
#include "bm1684_reg.h"
#include "bm1684_gmem.tmh"

int bmdrv_bm1684_parse_reserved_mem_info(struct bm_device_info *bmdi)
{
	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;
	struct chip_info *cinfo =  &bmdi->cinfo;
	int gmem_mode = 0x0;
	u64 ddr0_size;
    u64                       ddr1_size;
    u64                       ddr2_size;
	u64 a53_os_reserved_size = 0x40000000;
    u64                       heap2_size           = 0x2000000;

	gmem_mode = bmdrv_get_gmem_mode(bmdi);
	resmem_info->armfw_addr = GLOBAL_MEM_START_ADDR + FW_DDR_IMG_START;
	resmem_info->armfw_size = FW_DDR_IMG_SIZE;
	resmem_info->eutable_addr = 0;
	resmem_info->eutable_size = 0;
	resmem_info->armreserved_addr = GLOBAL_MEM_START_ADDR + ARM_RESERVED_START;
	resmem_info->armreserved_size = ARM_RESERVED_SIZE;
	resmem_info->smmu_addr = GLOBAL_MEM_START_ADDR + SMMU_RESERVED_START;
	resmem_info->smmu_size = SMMU_RESERVED_SIZE;
	resmem_info->warpaffine_addr = 0;
	resmem_info->warpaffine_size = 0;

	ddr0_size = bmdi->boot_info.ddr_0a_size + bmdi->boot_info.ddr_0b_size;
	ddr1_size = bmdi->boot_info.ddr_1_size;
	ddr2_size = bmdi->boot_info.ddr_2_size;

	if (bmdi->boot_info.ddr_ecc_enable) {
		ddr0_size = ddr0_size/8*7;
		ddr1_size = ddr1_size/8*7;
		ddr2_size = ddr2_size/8*7;
	}

	if (cinfo->a53_enable == 0x0)
		a53_os_reserved_size = 0;
	else
		a53_os_reserved_size = 0x40000000;

	// if (cinfo->heap2_size > 0x2000000 && cinfo->heap2_size < ddr2_size)
	// 	heap2_size = cinfo->heap2_size;
	// else
	// 	heap2_size = 0x2000000;
	if (cinfo->chip_id == 0x1684)
		heap2_size = ddr2_size - 0xAB00000;
	else if (cinfo->chip_id == 0x1686)
		heap2_size = ddr2_size - 0x6700000;

	resmem_info->npureserved_addr[0] = GLOBAL_MEM_START_ADDR + EFECTIVE_GMEM_START;
	resmem_info->npureserved_size[0] = ddr0_size - EFECTIVE_GMEM_START;

	if (gmem_mode == GMEM_NORMAL) {
		resmem_info->vpp_addr = 0x300000000 + a53_os_reserved_size;
		resmem_info->vpp_size = VPP_RESERVED_SIZE;

		resmem_info->npureserved_addr[1] = resmem_info->vpp_addr + resmem_info->vpp_size;
		resmem_info->npureserved_size[1] = ddr1_size - a53_os_reserved_size - resmem_info->vpp_size;

		resmem_info->vpu_vmem_addr = 0x400000000;
		resmem_info->vpu_vmem_size = ddr2_size - heap2_size;

		resmem_info->npureserved_addr[2] = resmem_info->vpu_vmem_addr + resmem_info->vpu_vmem_size;
		resmem_info->npureserved_size[2] = heap2_size;
		resmem_info->npureserved_addr[3] = resmem_info->vpu_vmem_addr;
        resmem_info->npureserved_size[3] = resmem_info->vpu_vmem_size;
	} else if (gmem_mode == GMEM_TPU_ONLY) {
		resmem_info->npureserved_addr[1] = 0x300000000;
		resmem_info->npureserved_addr[2] = 0x400000000;
		if (bmdi->boot_info.ddr_ecc_enable) {
			resmem_info->npureserved_size[1] = bmdi->boot_info.ddr_1_size/8*7;
			resmem_info->npureserved_size[2] = bmdi->boot_info.ddr_2_size/8*7;
		} else {
			resmem_info->npureserved_size[1] = bmdi->boot_info.ddr_1_size;
			resmem_info->npureserved_size[2] = bmdi->boot_info.ddr_2_size;
		}
		resmem_info->vpu_vmem_addr = 0;
		resmem_info->vpu_vmem_size = 0;
		resmem_info->vpp_addr = 0;
		resmem_info->vpp_size = 0;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: armfw_addr = 0x%llx, armfw_size=0x%llx", bmdi->dev_index, resmem_info->armfw_addr, resmem_info->armfw_size);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: armreserved_addr = 0x%llx, armreserved_size=0x%llx", bmdi->dev_index, resmem_info->armreserved_addr, resmem_info->armreserved_size);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: smmu_addr = 0x%llx, smmu_size=0x%llx", bmdi->dev_index, resmem_info->smmu_addr, resmem_info->smmu_size);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: warpaffine_addr = 0x%llx, warpaffine_size=0x%llx", bmdi->dev_index, resmem_info->warpaffine_addr, resmem_info->warpaffine_size);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: npureserved_addr0 = 0x%llx, npureserved0_size=0x%llx", bmdi->dev_index, resmem_info->npureserved_addr[0], resmem_info->npureserved_size[0]);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: npureserved_addr1 = 0x%llx, npureserved1_size=0x%llx", bmdi->dev_index, resmem_info->npureserved_addr[1], resmem_info->npureserved_size[1]);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: npureserved_addr2 = 0x%llx, npureserved2_size=0x%llx", bmdi->dev_index, resmem_info->npureserved_addr[2], resmem_info->npureserved_size[2]);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: npureserved_addr3 = 0x%llx, npureserveds_size=0x%llx", bmdi->dev_index, resmem_info->npureserved_addr[3], resmem_info->npureserved_size[3]);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: vpureserved_addr = 0x%llx, vpureserved_size=0x%llx", bmdi->dev_index, resmem_info->vpu_vmem_addr, resmem_info->vpu_vmem_size);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,"bm-sophon%d pcie mode: vpp_addr = 0x%llx, vpp_size=0x%llx", bmdi->dev_index, resmem_info->vpp_addr, resmem_info->vpp_size);
	return 0;
}