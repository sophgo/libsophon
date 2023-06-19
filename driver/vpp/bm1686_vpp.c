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

#include "bm1684_cdma.h"
#include "bm_common.h"
#include "bm_irq.h"
#include "bm_memcpy.h"
#include "bm1684_irq.h"
#include "bm_gmem.h"
#include "bm_memcpy.h"
#include "bm1686_vpp.h"

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

static struct vpp_reset_info bm_vpp_rst[VPP1686_CORE_MAX] = {
	{SW_RESET_REG1, VPP_CORE0_RST},
	{SW_RESET_REG2, VPP_CORE1_RST},
};


#if 0
/*Positive number * 1024*/
/*negative number * 1024, then Complement code*/
YPbPr2RGB, BT601
Y = 0.299 R + 0.587 G + 0.114 B
U = -0.1687 R - 0.3313 G + 0.5 B + 128
V = 0.5 R - 0.4187 G - 0.0813 B + 128
R = Y + 1.4018863751529200 (Cr-128)
G = Y - 0.345806672214672 (Cb-128) - 0.714902851111154 (Cr-128)
B = Y + 1.77098255404941 (Cb-128)

YCbCr2RGB, BT601
Y = 16  + 0.257 * R + 0.504 * g + 0.098 * b
Cb = 128 - 0.148 * R - 0.291 * g + 0.439 * b
Cr = 128 + 0.439 * R - 0.368 * g - 0.071 * b
R = 1.164 * (Y - 16) + 1.596 * (Cr - 128)
G = 1.164 * (Y - 16) - 0.392 * (Cb - 128) - 0.812 * (Cr - 128)
B = 1.164 * (Y - 16) + 2.016 * (Cb - 128)
#endif


static void vpp_reg_write(int core_id, struct bm_device_info *bmdi, unsigned int val, unsigned int offset)
{
	if (core_id == 0)
		vpp0_reg_write(bmdi, offset, val);
	else
		vpp1_reg_write(bmdi, offset, val);
}

static unsigned int vpp_reg_read(int core_id, struct bm_device_info *bmdi, unsigned int offset)
{
	unsigned int ret;

	if (core_id == 0)
		ret = vpp0_reg_read(bmdi, offset);
	else
		ret = vpp1_reg_read(bmdi, offset);
	return ret;
}
#if  1
__maybe_unused static int vpp_reg_dump(struct bm_device_info *bmdi,int core_id)
{
	u32 reg_value = 0;

	pr_info("core_id: %d\n",core_id);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_VERSION);
	pr_info("VPP_VERSION: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_CONTROL0);
	pr_info("VPP_CONTROL0: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE);
	pr_info("VPP_CMD_BASE: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE_EXT);
	pr_info("VPP_CMD_BASE_EXT: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_STATUS);
	pr_info("VPP_STATUS: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_EN);
	pr_info("VPP_INT_EN: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_CLEAR);
	pr_info("VPP_INT_CLEAR: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_STATUS);
	pr_info("VPP_INT_STATUS: 0x%x\n",reg_value);
	reg_value = vpp_reg_read(core_id, bmdi, VPP_INT_RAW_STATUS);
	pr_info("VPP_INT_RAW_STATUS: 0x%x\n",reg_value);

	return 0;
}

__maybe_unused static int top_reg_dump(struct bm_device_info *bmdi)
{

	u32 reg_value = 0;

	reg_value = bm_read32(bmdi,SW_RESET_REG0);
	pr_info("SW_RESET_REG0: 0x%x\n",reg_value);
	reg_value = bm_read32(bmdi,SW_RESET_REG1);
	pr_info("SW_RESET_REG1: 0x%x\n",reg_value);
	reg_value = bm_read32(bmdi,SW_RESET_REG2);
	pr_info("SW_RESET_REG2: 0x%x\n",reg_value);

	return 0;
}
__maybe_unused static int vpp_regs_dump(struct bm_device_info *bmdi)
{
	int core_id;

	top_reg_dump(bmdi);

	for (core_id = 0; core_id < VPP1686_CORE_MAX; core_id++)
		vpp_reg_dump(bmdi, core_id);

	return 0;
}

__maybe_unused static int vpp_core_reset(struct bm_device_info *bmdi, int core_id)
{
	u32 val = 0;

/*add mutex lock*/
	val = bm_read32(bmdi, bm_vpp_rst[core_id].reg);
	val &= ~(1 << bm_vpp_rst[core_id].bit_n);
	bm_write32(bmdi, bm_vpp_rst[core_id].reg, val);
	udelay(10);
	val |= (1 << bm_vpp_rst[core_id].bit_n);
	bm_write32(bmdi, bm_vpp_rst[core_id].reg, val);
/*add mutex unlock*/

	return 0;
}
#endif
static int vpp_soft_rst(struct bm_device_info *bmdi, int core_id)
{
#if 0
	u32 reg_read = 0;
	u32 active_check = 0;
	u32 count_value = 0;

	/*set dma_stop=1*/
	//vpp_reg_write(core_id, bmdi, 0x104, VPP_CONTROL0);

	/*check dma_stop_active==1*/
	for (count_value = 0; count_value < 1000; count_value++)
	{
		reg_read = vpp_reg_read(core_id, bmdi, VPP_STATUS);
		if ((reg_read >> 17) & 0x1)
		{
			active_check++;
			if (active_check == 5)
			{
				break;
			}
		}
		udelay(1);
	}
	if((1000 == count_value) && (5 != active_check))
	{
		//pr_err("[1686VPPDRV] dma_stop failed, continue vpp soft reset\n");
	}
#endif
	/*vpp soft reset*/
	vpp_reg_write(core_id, bmdi, 0x80, VPP_CONTROL0);

	udelay(10);

	vpp_reg_write(core_id, bmdi, 0x82, VPP_CONTROL0);


/*deassert dma_stop and check dma_stop_active*/
	//vpp_reg_write(core_id, bmdi, 0x100, VPP_CONTROL0);
	//pr_info("vpp_soft_rst done\n");

	return VPP_OK;
}

static void vpp_clear_int(struct bm_device_info *bmdi, int core_id)
{
	if (core_id == 0)
		vpp0_reg_write(bmdi, VPP_INT_CLEAR, 0xffffffff);
	else
		vpp1_reg_write(bmdi, VPP_INT_CLEAR, 0xffffffff);
}

static void vpp_irq_handler(struct bm_device_info *bmdi, int core_id)
{
	bmdi->vppdrvctx.got_event_vpp[core_id] = 1;
	wake_up(&bmdi->vppdrvctx.wq_vpp[core_id]);
}

static int vpp_check_hw_idle(int core_id, struct bm_device_info *bmdi)
{
	unsigned int status, val;
	unsigned int count = 0;

	while (1) {
		status = vpp_reg_read(core_id, bmdi, VPP_STATUS);
		val = (status >> 16) & 0x1;
		if (val) // idle
			break;

		count++;
		if (count > 20000) { // 2 Sec
			pr_err("[1686VPPDRV]vpp is busy!!! status 0x%08x, core %d, pid %d, tgid %d, device %d\n",
			       status, core_id, current->pid, current->tgid, bmdi->dev_index);
			vpp_soft_rst(bmdi,core_id);
			return VPP_ERR;
		}
		udelay(100);
	}

	count = 0;
	while (1) {
		status = vpp_reg_read(core_id, bmdi, VPP_INT_RAW_STATUS);
		if (status == 0x0)
			break;

		count++;
		if (count > 20000) { // 2 Sec
			pr_err("vpp raw status 0x%08x, core %d, pid %d,tgid %d, device %d\n",
			       status, core_id, current->pid, current->tgid, bmdi->dev_index);
			vpp_soft_rst(bmdi,core_id);
			return VPP_ERR;
		}
		udelay(100);

		vpp_reg_write(core_id, bmdi, 0xffffffff, VPP_INT_CLEAR);
	}

	return VPP_OK;
}

static int vpp_setup_desc(struct bm_device_info *bmdi, struct vpp_batch_n *batch, int core_id)
{
	int ret;
	uint32 reg_value = 0;
	u64 vpp_desc_pa = 0;
	unsigned char vpp_id_mode = Transaction_Mode;

	vpp_soft_rst(bmdi,core_id);

	/* check vpp hw idle */
	ret = vpp_check_hw_idle(core_id, bmdi);
	if (ret < 0) {
		pr_err("[1686VPPDRV]vpp_check_hw_idle failed! core_id %d, dev_index %d.\n", core_id, bmdi->dev_index);
		return ret;
	}

	/*set cmd list addr*/
	vpp_desc_pa = bmdi->gmem_info.resmem_info.vpp_addr + core_id * (512 << 10);
	vpp_reg_write(core_id, bmdi, (unsigned int)(vpp_desc_pa & 0xffffffff), VPP_CMD_BASE);
	reg_value = ((vpp_id_mode & 0x3) << 29 ) | ((vpp_desc_pa >> 32) & 0xff);// vpp_id_mode;

	vpp_reg_write(core_id, bmdi, reg_value, VPP_CMD_BASE_EXT);
	vpp_reg_write(core_id, bmdi, 0x01, VPP_INT_EN);//interruput mode : frame_done_int_en

	/*start vpp hw work*/
	vpp_reg_write(core_id, bmdi, 0x41, VPP_CONTROL0);

	return VPP_OK;
}

static int vpp_prepare_cmd_list(struct bm_device_info *bmdi,
				struct vpp_batch_n *batch,
				int core_id)
{
	int idx, ret=0;
	u64 vpp_desc_pa=0;

	for (idx = 0; idx < batch->num; idx++) {

		descriptor *pdes = (batch->cmd + idx);
		if (((pdes->src_base_ext.src_base_ext_ch0 & 0xff) != 0x1) &&
		  ((pdes->src_base_ext.src_base_ext_ch0 & 0xff) != 0x2) &&
		  ((pdes->src_base_ext.src_base_ext_ch0 & 0xff) != 0x3) &&
		  ((pdes->src_base_ext.src_base_ext_ch0 & 0xff) != 0x4))
		{
			pr_err("[1686VPPDRV]pdes->src_base_ext.src_base_ext_ch0  0x%x\n", pdes->src_base_ext.src_base_ext_ch0);
			ret = VPP_ERR_INVALID_PA;
			return ret;
		}

		vpp_desc_pa = bmdi->gmem_info.resmem_info.vpp_addr + core_id * (512 << 10) + sizeof(descriptor) * (idx + 1);

		if (idx == (batch->num - 1)) {
			pdes->des_head.crop_flag = 1;
			pdes->des_head.next_cmd_base_ext = 0;
			pdes->next_cmd_base= 0;
		}
		else {
			pdes->des_head.crop_flag = 0;
			pdes->des_head.next_cmd_base_ext = (uint32)((vpp_desc_pa >> 32) & 0xff);
			pdes->next_cmd_base= (uint32)(vpp_desc_pa & 0xffffffff);
		}

		pdes->des_head.crop_id = idx;
	}
	//dump_des(batch, pdes, vpp_desc_pa);

	vpp_desc_pa = bmdi->gmem_info.resmem_info.vpp_addr + core_id * (512 << 10);

	ret = bmdev_memcpy_s2d_internal(bmdi, vpp_desc_pa, (void *)(batch->cmd), batch->num * sizeof(descriptor));
	if (ret != VPP_OK) {
		pr_err("[1686VPPDRV]bmdev_memcpy_s2d_internal failed, dev_index %d\n", bmdi->dev_index);
		return ret;
	}

	return ret;
}

static int vpp_get_core_id(struct bm_device_info *bmdi, int *core)
{
	if (bmdi->vppdrvctx.vpp_idle_bit_map >= 3) {
		pr_err("[1686VPPDRV]take sem, but two vpp core are busy, vpp_idle_bit_map = %ld\n", bmdi->vppdrvctx.vpp_idle_bit_map);
		return VPP_ERR_IDLE_BIT_MAP;
	}

	if (test_and_set_bit(0, &bmdi->vppdrvctx.vpp_idle_bit_map) == 0) {
		*core = 0;
	} else if (test_and_set_bit(1, &bmdi->vppdrvctx.vpp_idle_bit_map) == 0) {
		*core = 1;
	} else {
		pr_err("[1686VPPDRV]Abnormal status, vpp_idle_bit_map = %ld\n", bmdi->vppdrvctx.vpp_idle_bit_map);
		return VPP_ERR_IDLE_BIT_MAP;
	}
	return VPP_OK;
}

static int vpp_free_core_id(struct bm_device_info *bmdi, int core_id)
{
	if ((core_id != 0) && (core_id != 1)) {
		pr_err("vpp abnormal status, vpp_idle_bit_map = %ld, core_id is %d\n", bmdi->vppdrvctx.vpp_idle_bit_map, core_id);
		return VPP_ERR_IDLE_BIT_MAP;
	}

	clear_bit(core_id, &bmdi->vppdrvctx.vpp_idle_bit_map);
	return VPP_OK;
}
#if 0
__maybe_unused static void dump_des(struct vpp_batch_n *batch, struct vpp_descriptor **pdes_vpp, dma_addr_t *des_paddr)
{
	int idx = 0;

	//pr_info("bmdi->dev_index   is      %d\n", bmdi->dev_index);
	pr_info("batch->num   is      %d\n", batch->num);
	for (idx = 0; idx < batch->num; idx++) {
		pr_info("des_paddr[%d]   0x%llx\n", idx, des_paddr[idx]);
		pr_info("pdes_vpp[%d]->des_head   0x%x\n", idx, pdes_vpp[idx]->des_head);
		pr_info("pdes_vpp[%d]->next_cmd_base   0x%x\n", idx, pdes_vpp[idx]->next_cmd_base);
	}
}

void vpp_dump(struct vpp_batch_n *batch)
{
	struct vpp_cmd *cmd ;
	int i;
	for (i = 0; i < batch->num; i++) {
		cmd = (batch->cmd + i);
		pr_info("batch->num    is  %d      \n", batch->num);
		pr_info("cmd id %d, cmd->src_format  0x%x\n", i, cmd->src_format);
		pr_info("cmd id %d, cmd->src_stride  0x%x\n", i, cmd->src_stride);
	}
	return;
}
#endif
static int vpp_handle_setup(struct bm_device_info *bmdi, struct vpp_batch_n *batch)
{
	int ret = VPP_OK;
	int core_id = -1;

	if (down_interruptible(&bmdi->vppdrvctx.vpp_core_sem)) {
		pr_err("[1686VPPDRV]down_interruptible id interrupted, dev_index %d\n", bmdi->dev_index);
		return VPP_ERESTARTSYS;
	}
	ret = vpp_get_core_id(bmdi, &core_id);
	if (ret != VPP_OK) {
		up(&bmdi->vppdrvctx.vpp_core_sem);
		return VPP_ERR;
	}

	ret = vpp_prepare_cmd_list(bmdi, batch, core_id);
	if (ret != VPP_OK) {
		pr_err("[1686VPPDRV]vpp_prepare_cmd_list failed, dev_index %d\n", bmdi->dev_index);
		vpp_free_core_id(bmdi, core_id);
		up(&bmdi->vppdrvctx.vpp_core_sem);
		return VPP_ERR;
	}

	ret = vpp_setup_desc(bmdi, batch, core_id);
	if (ret < 0){
		pr_err("[1686VPPDRV]vpp_setup_desc failed \n");
		vpp_free_core_id(bmdi, core_id);
		up(&bmdi->vppdrvctx.vpp_core_sem);
		return ret;
	}
	atomic_inc(&bmdi->vppdrvctx.s_vpp_usage_info.vpp_busy_status[core_id]);
	ret = wait_event_timeout(bmdi->vppdrvctx.wq_vpp[core_id],
					bmdi->vppdrvctx.got_event_vpp[core_id],
					100*HZ);
	atomic_dec(&bmdi->vppdrvctx.s_vpp_usage_info.vpp_busy_status[core_id]);

	if (ret == 0) {
		pr_err("vpp timeout,core_id %d,current->pid %d,current->tgid %d,vpp_idle_bit_map %ld, dev_index  %d\n",
				 core_id, current->pid, current->tgid,
				bmdi->vppdrvctx.vpp_idle_bit_map,
				bmdi->dev_index);
	//	vpp_dump(batch);
		vpp_regs_dump(bmdi);
		vpp_soft_rst(bmdi,core_id);
		ret = VPP_ERR_INT_TIMEOUT;
	}

	bmdi->vppdrvctx.got_event_vpp[core_id] = 0;

	ret = vpp_free_core_id(bmdi, core_id);
	up(&bmdi->vppdrvctx.vpp_core_sem);

	if (signal_pending(current)) {
		ret |= VPP_ERESTARTSYS;
		pr_err("signal_pending ret=%d,current->pid %d,current->tgid %d,vpp_idle_bit_map %ld, dev_index %d\n",
		       ret, current->pid, current->tgid,
		       bmdi->vppdrvctx.vpp_idle_bit_map, bmdi->dev_index);
	}

	return ret;
}


int bm1686_trigger_vpp(struct bm_device_info *bmdi, unsigned long arg)
{
	struct vpp_batch_n batch, batch_tmp;
	int ret = 0;

	//pr_info("[1686VPPDRV] start trigger_vpp\n");

	ret = copy_from_user(&batch_tmp, (void *)arg, sizeof(batch_tmp));
	if (ret != 0) {
		pr_err("[1686VPPDRV]trigger_vpp copy_from_user wrong,the number of bytes not copied %d, sizeof(batch_tmp) total need %lu, dev_index  %d\n",
			ret, sizeof(batch_tmp), bmdi->dev_index);
		ret = VPP_ERR_COPY_FROM_USER;
		return ret;
	}

	batch = batch_tmp;

	batch.cmd = kzalloc(batch.num * (sizeof(descriptor)), GFP_KERNEL);
	if (batch.cmd == NULL) {
		ret = VPP_ENOMEM;
		pr_info("[1686VPPDRV]batch_cmd is NULL, dev_index  %d\n", bmdi->dev_index);
		return ret;
	}

	ret = copy_from_user(batch.cmd, ((void *)batch_tmp.cmd), (batch.num * sizeof(descriptor)));
	if (ret != 0) {
		pr_err("[1686VPPDRV]trigger_vpp copy_from_user wrong,the number of bytes not copied %d,batch.num %d, single vpp_cmd %lu, total need %lu\n",
			ret, batch.num, sizeof(descriptor), (batch.num * sizeof(descriptor)));
		kfree(batch.cmd);
		ret = VPP_ERR_COPY_FROM_USER;
		return ret;
	}

	ret = vpp_handle_setup(bmdi, &batch);
	if ((ret != VPP_OK) && (ret != VPP_ERESTARTSYS))
		pr_err("[1686VPPDRV]trigger_vpp ,vpp_handle_setup wrong, ret %d, line  %d, dev_index  %d\n", ret, __LINE__, bmdi->dev_index);

	kfree(batch.cmd);
	return ret;
}

int bm1686_vpp_init(struct bm_device_info *bmdi)
{
	int i;
	sema_init(&bmdi->vppdrvctx.vpp_core_sem, VPP1686_CORE_MAX);
	bmdi->vppdrvctx.vpp_idle_bit_map = 0;
	vpp_soft_rst(bmdi,0);
	vpp_soft_rst(bmdi,1);
	vpp_usage_info_init(bmdi);
	for (i = 0; i < VPP1686_CORE_MAX; i++)
		init_waitqueue_head(&bmdi->vppdrvctx.wq_vpp[i]);
	return 0;
}

void bm1686_vpp_exit(struct bm_device_info *bmdi)
{
}

static void bmdrv_vpp0_irq_handler(struct bm_device_info *bmdi)
{
	vpp_clear_int(bmdi, 0);
	vpp_irq_handler(bmdi,0);
}

static void bmdrv_vpp1_irq_handler(struct bm_device_info *bmdi)
{
	vpp_clear_int(bmdi, 1);
	vpp_irq_handler(bmdi, 1);
}

void bm1686_vpp_request_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_request_irq(bmdi, VPP0_IRQ_ID, bmdrv_vpp0_irq_handler);
	bmdrv_submodule_request_irq(bmdi, VPP1_IRQ_ID, bmdrv_vpp1_irq_handler);
}

void bm1686_vpp_free_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_free_irq(bmdi, VPP0_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, VPP1_IRQ_ID);
}

