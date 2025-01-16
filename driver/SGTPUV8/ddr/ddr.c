#include "bm_common.h"
#include <ddr_sys_bring_up_pld.h>
//#include <ddr_init.h>
//#include <bm_io.h>
#include <bm_api.h>
#include "ddr.h"
//#include <bitwise_ops.h>

int a2_ddr_init(struct bm_device_info *bmdi)
{
	pr_info("SGTPUV8 DDR init.\n");
	pld_ddr_sys_bring_up(bmdi);

	return 0;
}
