#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/version.h>


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif

#include "bm1688_cdma.h"
#include "bm_common.h"
#include "bm_irq.h"
#include "bm_memcpy.h"
#include "bm1688_irq.h"
#include "bm_gmem.h"
#include "bm_memcpy.h"
#include "bm1688_vpp.h"

#define VPP_OK                             (0)
#define VPP_ERR                            (-1)
#define VPP_ERR_COPY_FROM_USER             (-2)
#define VPP_ERR_WRONG_CROPNUM              (-3)
#define VPP_ERR_INVALID_FD                 (-4)
#define VPP_ERR_INT_TIMEOUT                (-5)
#define VPP_ERR_INVALID_PA                 (-6)
#define VPP_ERR_INVALID_CMD                (-7)

#define VPP_ENOMEM                         (-12)
#define VPP_ERR_IDLE_BIT_MAP               (-256)
#define VPP_ERESTARTSYS                    (-512)

static struct bm_device_info *vpp_bmdi;

void vpss_reg_write(uintptr_t addr, u32 data)
{
	bm_write32(vpp_bmdi, addr, data);
}

u32 vpss_reg_read(uintptr_t addr)
{
	return bm_read32(vpp_bmdi, addr);
}

int bm1688_trigger_vpp(struct bm_device_info *bmdi, unsigned long arg)
{
	struct vpp_batch_n batch, batch_tmp;
	int ret = 0;

	ret = copy_from_user(&batch_tmp, (void *)arg, sizeof(batch_tmp));
	if (ret != 0) {
		pr_err("[1688VPPDRV]trigger_vpp copy_from_user wrong,\
			the number of bytes not copied %d, total need %lu, dev_index %d\n",
			ret, sizeof(batch_tmp), bmdi->dev_index);
		ret = VPP_ERR_COPY_FROM_USER;
		return ret;
	}

	batch = batch_tmp;

	batch.cmd = kzalloc(sizeof(bm_vpss_cfg), GFP_KERNEL);
	if (batch.cmd == NULL) {
		ret = VPP_ENOMEM;
		pr_info("[1686VPPDRV]batch_cmd is NULL, dev_index  %d\n", bmdi->dev_index);
		return ret;
	}

	ret = copy_from_user(batch.cmd, ((void *)batch_tmp.cmd), sizeof(bm_vpss_cfg));
	if (ret != 0) {
		pr_err("[1688VPPDRV]trigger_vpp copy_from_user wrong,\
			the number of bytes not copied %d, total need %lu\n",
			ret, sizeof(bm_vpss_cfg));
		kfree(batch.cmd);
		ret = VPP_ERR_COPY_FROM_USER;
		return ret;
	}

	ret = vpss_bm_send_frame(batch.cmd);
	kfree(batch.cmd);
	return ret;
}

int bm1688_vpp_init(struct bm_device_info *bmdi)
{
	struct vpss_device *dev = &bmdi->vppdrvctx.vpss_dev;
	void *reg_base[4] = {(void *)REG_VPSS_V_BASE, (void *)REG_VPSS_T1_BASE, (void *)REG_VPSS_T2_BASE, (void *)REG_VPSS_D_BASE};
	vpp_bmdi = bmdi;

	vpss_proc_init(dev);
	vpss_mode_init();

	dev->vpss_cores[0].irq_num = VPP0_IRQ_ID;
	dev->vpss_cores[1].irq_num = VPP1_IRQ_ID;
	dev->vpss_cores[2].irq_num = VPP2_IRQ_ID;
	dev->vpss_cores[3].irq_num = VPP3_IRQ_ID;
	dev->vpss_cores[4].irq_num = VPP4_IRQ_ID;
	dev->vpss_cores[5].irq_num = VPP5_IRQ_ID;
	dev->vpss_cores[6].irq_num = VPP6_IRQ_ID;
	dev->vpss_cores[7].irq_num = VPP7_IRQ_ID;
	dev->vpss_cores[8].irq_num = VPP8_IRQ_ID;
	dev->vpss_cores[9].irq_num = VPP9_IRQ_ID;

	sclr_set_base_addr(reg_base[0], reg_base[1], reg_base[2], reg_base[3]);
	sclr_init_sys_top_addr();
	vpss_dev_init(dev);

	return 0;
}

void bm1688_vpp_exit(struct bm_device_info *bmdi)
{
	struct vpss_device *dev = &bmdi->vppdrvctx.vpss_dev;
	vpss_mode_deinit();
	vpss_dev_deinit(dev);
	vpss_proc_remove(dev);
}

static void bmdrv_vpp_irq_handler(struct bm_device_info *bmdi, int irq, int idx)
{
	struct vpss_device *dev = &bmdi->vppdrvctx.vpss_dev;
	vpss_isr(irq, &dev->vpss_cores[idx]);
}

static void bmdrv_vpp0_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP0_IRQ_ID, 0);
}

static void bmdrv_vpp1_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP1_IRQ_ID, 1);
}

static void bmdrv_vpp2_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP2_IRQ_ID, 2);
}

static void bmdrv_vpp3_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP3_IRQ_ID, 3);
}

static void bmdrv_vpp4_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP4_IRQ_ID, 4);
}

static void bmdrv_vpp5_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP5_IRQ_ID, 5);
}

static void bmdrv_vpp6_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP6_IRQ_ID, 6);
}

static void bmdrv_vpp7_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP7_IRQ_ID, 7);
}

static void bmdrv_vpp8_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP8_IRQ_ID, 8);
}

static void bmdrv_vpp9_irq_handler(struct bm_device_info *bmdi)
{
	bmdrv_vpp_irq_handler(bmdi, VPP9_IRQ_ID, 9);
}

void bm1688_vpp_request_irq(struct bm_device_info *bmdi)
{
	struct vpss_device *dev = &bmdi->vppdrvctx.vpss_dev;
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[0].irq_num, bmdrv_vpp0_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[1].irq_num, bmdrv_vpp1_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[2].irq_num, bmdrv_vpp2_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[3].irq_num, bmdrv_vpp3_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[4].irq_num, bmdrv_vpp4_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[5].irq_num, bmdrv_vpp5_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[6].irq_num, bmdrv_vpp6_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[7].irq_num, bmdrv_vpp7_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[8].irq_num, bmdrv_vpp8_irq_handler);
	bmdrv_submodule_request_irq(bmdi, dev->vpss_cores[9].irq_num, bmdrv_vpp9_irq_handler);
}

void bm1688_vpp_free_irq(struct bm_device_info *bmdi)
{
	struct vpss_device *dev = &bmdi->vppdrvctx.vpss_dev;
	int i = 0;
	for (i = 0; i < VPP_CORE_MAX; ++i) {
		bmdrv_submodule_free_irq(bmdi, dev->vpss_cores[i].irq_num);
	}
}

