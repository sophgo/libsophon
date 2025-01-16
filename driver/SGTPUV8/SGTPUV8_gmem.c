#include "bm_common.h"
#include "SGTPUV8_reg.h"
#include "bm_gmem.h"


#include <linux/of_address.h>
int bmdrv_SGTPUV8_parse_reserved_mem_info(struct bm_device_info *bmdi)
{
	struct platform_device *pdev = bmdi->cinfo.pdev;
	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;

	struct device_node *node;
	struct resource r[2];
	struct resource res;
	int ret = 0;
	int i = 0;

	for (i = 0; i < 2; i++)
	{
		node = of_parse_phandle(pdev->dev.of_node, "memory-region", i);
		if (!node)
		{
			dev_err(&pdev->dev, "no memory-region specified index %d\n", i);
			return -EINVAL;
		}

		ret = of_address_to_resource(node, 0, &r[i]);
		if (ret)
			return ret;
	}

	node = of_find_node_by_name(NULL, "scalar_mem");
	if (!node)
	{
		pr_err("scalar_mem node not found\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret)
	{
		pr_err("Failed to get resource from scalar_mem node\n");
		of_node_put(node);
		return ret;
	}

	resmem_info->armfw_addr = res.start;
	resmem_info->armfw_size = resource_size(&res);
	resmem_info->npureserved_addr[0] = r[0].start;
	resmem_info->npureserved_size[0] = resource_size(&r[0]);
	resmem_info->npureserved_addr[1] = r[1].start;
	resmem_info->npureserved_size[1] = resource_size(&r[1]);
	pr_info("soc mode: armfw_addr = 0x%llx, armfw_size=0x%llx", resmem_info->armfw_addr, resmem_info->armfw_size);
	pr_info("soc mode: npureserved_addr = 0x%llx, npureserved_size=0x%llx", resmem_info->npureserved_addr[0], resmem_info->npureserved_size[0]);

	of_node_put(node);
	return 0;
}

