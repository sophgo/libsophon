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
// static spinlock_t tpu_int_got_spinlock;
// static uint8_t tpu_sync_backup;


irq_handler_t irq_handlers[MAX_IRQS] = {
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
	// int ret = 0;
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
	//*irq_array[0] = platform_get_irq(pdev, 0);
	//ret = devm_request_irq(&pdev->dev, *irq_array[0], bm_tpu_gdma_irq, 0,
	//		       "bm-tpu-gdma", bmdi);
	//if (ret) {
	//	dev_err(&pdev->dev, "Failed to request irq %d\n", *irq_array[0]);
	//	return -EINVAL;
	//}

	if (cinfo->chip_id == CHIP_ID) {
		for (; i < MAX_IRQS; ++i) {
			*irq_array[i] = platform_get_irq(pdev, i);
			dev_info(&pdev->dev, "bmdrv: %s irq is %d\n", irq_names[i], *irq_array[i]);
		}
	} else {
		pr_debug("cinfo->chip_id not error\n");
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

