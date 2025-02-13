#include <linux/version.h>
#ifndef SOC_MODE
#include <linux/pci.h>
#include <linux/slab.h>
#include "bm1684_irq.h"
#include "bm1688_irq.h"
#else
#include <linux/irqflags.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "bm_msgfifo.h"
#include "bm_cdma.h"
#endif
#include <linux/types.h>
#include <linux/interrupt.h>
#include "bm_common.h"
#include "bm_fw.h"
#include "bm_irq.h"
#include <linux/delay.h>

#ifndef SOC_MODE
void bmdrv_enable_irq(struct bm_device_info *bmdi, int irq_num)
{
	if (bmdi->cinfo.bmdrv_enable_irq)
		bmdi->cinfo.bmdrv_enable_irq(bmdi, irq_num, true);
}

void bmdrv_disable_irq(struct bm_device_info *bmdi, int irq_num)
{
	if (bmdi->cinfo.bmdrv_enable_irq)
		bmdi->cinfo.bmdrv_enable_irq(bmdi, irq_num, false);
}

static void bmdrv_register_irq_handler(struct bm_device_info *bmdi,
		int irq_num, bmdrv_submodule_irq_handler handler)
{
	bmdi->cinfo.bmdrv_module_irq_handler[irq_num] = handler;
}

static void bmdrv_deregister_irq_handler(struct bm_device_info *bmdi, int irq_num)
{
	bmdi->cinfo.bmdrv_module_irq_handler[irq_num] = NULL;
}

void bmdrv_submodule_request_irq(struct bm_device_info *bmdi, int irq_num,
			bmdrv_submodule_irq_handler irq_handler)
{
	bmdrv_register_irq_handler(bmdi, irq_num, irq_handler);
	bmdrv_enable_irq(bmdi, irq_num);
}

void bmdrv_submodule_free_irq(struct bm_device_info *bmdi, int irq_num)
{
	bmdrv_deregister_irq_handler(bmdi, irq_num);
	bmdrv_disable_irq(bmdi, irq_num);
}

static void bmdrv_do_irq(struct bm_device_info *bmdi)
{
	int i = 0;
	int bitnum = 0;
	u32 status[6] = {0};
	enum arm9_fw_mode mode;

	if (bmdi->cinfo.chip_id == 0x1686a200)
		mode = gp_reg_read_enh(bmdi, GP_REG_C906_FW_MODE);
	else
		mode = gp_reg_read_enh(bmdi, GP_REG_ARM9_FW_MODE);
	bmdi->cinfo.bmdrv_get_irq_status(bmdi, status);
	if (mode == FW_MIX_MODE) {
		if ((status[1] >> (VETH_IRQ_ID - 32)) & 0x1) {
			bmdi->cinfo.irq_id = VETH_IRQ_ID;
			bmdi->cinfo.bmdrv_module_irq_handler[VETH_IRQ_ID](bmdi);
		}
	} else {
		for (i = 0; i < 6; i++) {
			bitnum = 0x0;
			while (status[i] != 0) {
				if (status[i] & 0x1) {
					if (bmdi->cinfo.bmdrv_module_irq_handler[i*32 + bitnum] != NULL) {
						bmdi->cinfo.irq_id = i*32 + bitnum;
						bmdi->cinfo.bmdrv_module_irq_handler[i*32 + bitnum](bmdi);
					}
				}
				bitnum++;
				status[i] = status[i] >> 1;
			}
		}
		if (bmdi->cinfo.bmdrv_unmaskall_intc_irq)
			bmdi->cinfo.bmdrv_unmaskall_intc_irq(bmdi);
	}
}

static irqreturn_t bmdrv_irq_handler(int irq, void *data)
{
	struct bm_device_info *bmdi = data;

	bmdrv_do_irq(bmdi);
	return IRQ_HANDLED;
}

int bmdrv_init_irq(struct pci_dev *pdev)
{
	int err = 0, flags = IRQF_ONESHOT;
	int count = 3;
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);

	bmdi->cinfo.has_msi = false;
	spin_lock_init(&bmdi->irq_lock);

	/* Request MSI IRQ */
	dev_info(bmdi->cinfo.device, "Hard IRQ line number: %d\n", pdev->irq);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	err = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
#else
	err = pci_enable_msi_range(pdev, 1, 1);
#endif
	if (err > 0) {
		flags &= ~IRQF_SHARED;
		bmdi->cinfo.has_msi = true;
	}

	sprintf(bmdi->cinfo.drv_irq_name, "bm-pcie%d", bmdi->dev_index);

retry:
	err = request_threaded_irq(pdev->irq, NULL, bmdrv_irq_handler, flags,
		bmdi->cinfo.drv_irq_name, bmdi);

	if (err < 0) {
		if (count > 0) {
			count--;
			msleep(20);
			goto retry;
		}
	}

#if SYNC_API_INT_MODE == 1
	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686)
		bm1684_pcie_msi_irq_enable(pdev, bmdi);
	else if (bmdi->cinfo.chip_id == 0x1686a200)
		bm1688_pcie_msi_irq_enable(pdev, bmdi);
#endif
	dev_info(bmdi->cinfo.device, "Requested IRQ NO:%d, MSI state:%s(%d, %d)\n",
			pdev->irq, bmdi->cinfo.has_msi ? "enabled" : "disabled",
			pci_msi_enabled(), err);
	return err;
}

void bmdrv_free_irq(struct pci_dev *pdev)
{
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);

	if (bmdi->cinfo.chip_id == 0x1684 || bmdi->cinfo.chip_id == 0x1686)
		bm1684_pcie_msi_irq_disable(bmdi);
	else if (bmdi->cinfo.chip_id == 0x1686a200)
		bm1688_pcie_msi_irq_disable(bmdi);
	free_irq(pdev->irq, bmdi);
	if (bmdi->cinfo.has_msi)
		pci_disable_msi(pdev);
}
#else // SOC_MODE
int bmdrv_init_irq(struct platform_device *pdev)
{
	int ret = 0;
	struct bm_device_info *bmdi = platform_get_drvdata(pdev);
	struct chip_info *cinfo = &bmdi->cinfo;

	if (cinfo->chip_id == 0x1686a200) {
		cinfo->irq_id_cdma0 = irq_of_parse_and_map(pdev->dev.of_node, 0);
		ret = devm_request_threaded_irq(&pdev->dev, cinfo->irq_id_cdma0, NULL, bmdrv_irq_handler_cdma0,
							IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "CDMA0", bmdi);
		if (ret)
			return -EINVAL;
		dev_info(&pdev->dev, "bmdrv: cdma0 irq is %d\n", cinfo->irq_id_cdma0);

		cinfo->irq_id_cdma1 = irq_of_parse_and_map(pdev->dev.of_node, 1);
		ret = devm_request_threaded_irq(&pdev->dev, cinfo->irq_id_cdma1, NULL, bmdrv_irq_handler_cdma1,
							IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "CDMA1", bmdi);
		if (ret)
			return -EINVAL;
		dev_info(&pdev->dev, "bmdrv: cdma1 irq is %d\n", cinfo->irq_id_cdma1);

		cinfo->irq_id_msg0 = irq_of_parse_and_map(pdev->dev.of_node, 2);
		ret = devm_request_threaded_irq(&pdev->dev, cinfo->irq_id_msg0, NULL, bmdrv_irq_handler_msg0,
							IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "MSG0", bmdi);
		if (ret)
			return -EINVAL;
		dev_info(&pdev->dev, "bmdrv: msg0 irq is %d\n", cinfo->irq_id_msg0);

		cinfo->irq_id_msg1 = irq_of_parse_and_map(pdev->dev.of_node, 3);
		ret = devm_request_threaded_irq(&pdev->dev, cinfo->irq_id_msg1, NULL, bmdrv_irq_handler_msg1,
							IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "MSG1", bmdi);
		if (ret)
			return -EINVAL;
		dev_info(&pdev->dev, "bmdrv: msg1 irq is %d\n", cinfo->irq_id_msg1);
	} else {
		cinfo->irq_id_cdma = irq_of_parse_and_map(pdev->dev.of_node, 0);
		ret = devm_request_threaded_irq(&pdev->dev, cinfo->irq_id_cdma, NULL, bmdrv_irq_handler_cdma,
							IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "CDMA", bmdi);
		if (ret)
			return -EINVAL;
		dev_info(&pdev->dev, "bmdrv: cdma irq is %d\n", cinfo->irq_id_cdma);

		cinfo->irq_id_msg = irq_of_parse_and_map(pdev->dev.of_node, 1);
		ret = devm_request_threaded_irq(&pdev->dev, cinfo->irq_id_msg, NULL, bmdrv_irq_handler_msg0,
							IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "MSG", bmdi);
		if (ret)
			return -EINVAL;
		dev_info(&pdev->dev, "bmdrv: msg irq is %d\n", cinfo->irq_id_msg);
	}

	return 0;
}

void bmdrv_free_irq(struct platform_device *pdev)
{
	struct bm_device_info *bmdi = platform_get_drvdata(pdev);

	if (bmdi->cinfo.chip_id == 0x1686a200) {
		devm_free_irq(&pdev->dev, bmdi->cinfo.irq_id_cdma0, bmdi);
		devm_free_irq(&pdev->dev, bmdi->cinfo.irq_id_cdma1, bmdi);
		devm_free_irq(&pdev->dev, bmdi->cinfo.irq_id_msg0, bmdi);
		devm_free_irq(&pdev->dev, bmdi->cinfo.irq_id_msg1, bmdi);
	} else {
		devm_free_irq(&pdev->dev, bmdi->cinfo.irq_id_cdma, bmdi);
		devm_free_irq(&pdev->dev, bmdi->cinfo.irq_id_msg, bmdi);
	}
}
#endif //SOC_MODE
