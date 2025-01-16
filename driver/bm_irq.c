#include <linux/version.h>

#include <linux/irqflags.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
// #include "bm_msgfifo.h"

#include <linux/types.h>
#include <linux/interrupt.h>
#include "bm_common.h"
#include "bm_irq.h"
#include <linux/delay.h>


#define MAX_IRQS 7 // device-tree intr count


irq_handler_t irq_handlers[MAX_IRQS] = {
	// bmdrv_irq_handler_cdma,
	// bmdrv_irq_handler_msg0,
    // bmdrv_irq_handler_msg1,
    // bmdrv_irq_handler_msg2,
    // bmdrv_irq_handler_msg3,
    // bmdrv_irq_handler_msg4,
    // bmdrv_irq_handler_msg5,
    // bmdrv_irq_handler_msg6,
};


const char *irq_names[MAX_IRQS] = {
	"irq_id_gdma_intr",
	"irq_id_gdma_err_intr",
	"irq_id_tpu_intr",
	"irq_id_tpu_intr_pio_empty",
	"irq_id_tpu_intr_pio_half_empty",
	"irq_id_tpu_intr_pio_quart_empty",
	"irq_id_tpu_intr_pio_one_empty"
};

int bmdrv_init_irq(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct bm_device_info *bmdi = platform_get_drvdata(pdev);
	struct chip_info *cinfo = &bmdi->cinfo;
	u32 *irq_array[] = {
				&cinfo->irq_id.irq_id_gdma_intr,
				&cinfo->irq_id.irq_id_gdma_err_intr,
				&cinfo->irq_id.irq_id_tpu_intr,
				&cinfo->irq_id.irq_id_tpu_intr_pio_empty,
				&cinfo->irq_id.irq_id_tpu_intr_pio_half_empty,
				&cinfo->irq_id.irq_id_tpu_intr_pio_quart_empty,
				&cinfo->irq_id.irq_id_tpu_intr_pio_one_empty
			};

	if (cinfo->chip_id == CHIP_ID) {
		for (; i < MAX_IRQS; ++i) {
			*irq_array[i] = platform_get_irq(pdev, i);
			// ret = devm_request_threaded_irq(&pdev->dev, *irq_array[i], NULL, bmdrv_irq_handler_cdma,
			// 					IRQF_TRIGGER_HIGH | IRQF_ONESHOT, irq_names[i], bmdi);
			if (ret)
				return -EINVAL;
			dev_info(&pdev->dev, "bmdrv: %s irq is %d\n", irq_names[i], *irq_array[i]);
		}
	} else {
		// PR_DEBUG("have not accomplish\n");
		return -EINVAL;
	}

	return 0;
}

void bmdrv_free_irq(struct platform_device *pdev)
{
	int i = 0;
	struct bm_device_info *bmdi = platform_get_drvdata(pdev);
	struct chip_info *cinfo = &bmdi->cinfo;
	u32 *irq_array[] = {
				&cinfo->irq_id.irq_id_gdma_intr,
				&cinfo->irq_id.irq_id_gdma_err_intr,
				&cinfo->irq_id.irq_id_tpu_intr,
				&cinfo->irq_id.irq_id_tpu_intr_pio_empty,
				&cinfo->irq_id.irq_id_tpu_intr_pio_half_empty,
				&cinfo->irq_id.irq_id_tpu_intr_pio_quart_empty,
				&cinfo->irq_id.irq_id_tpu_intr_pio_one_empty
			};
	for ( ; i < MAX_IRQS; ++i) {
				// dev_info(&pdev->dev, "------------------- %d\n",  *irq_array[i]);
		devm_free_irq(&pdev->dev, *irq_array[i], bmdi);
	}
}

