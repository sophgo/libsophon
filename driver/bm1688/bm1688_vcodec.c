#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/irqreturn.h>
#include <linux/syscalls.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif

#include "bm_common.h"
#include "bm_memcpy.h"
#include "bm_irq.h"
#include "bm_gmem.h"

extern int drv_vpu_init(void);
extern int drv_vpu_deinit(void);
extern int drv_base_init(void);
extern int drv_base_deinit(void);
extern irqreturn_t jpu_irq_handler(int core, void *dev_id);
extern irqreturn_t vpu_irq_handler(int core, void *dev_id);
struct bm_device_info *g_bmdi = NULL;
static int s_jpu_irq[4] = {46, 47, 48, 49};
static int s_vpu_irq[3] = {45, 39, 42};

unsigned int vc_read_reg(unsigned int addr)
{
	return bm_read32(g_bmdi, addr);
}

unsigned int vc_write_reg(unsigned int addr, unsigned int data)
{
	return bm_write32(g_bmdi, addr, data);
}

uint64_t vc_ion_alloc(uint32_t len, void** ion_handle)
{
	struct ion_allocation_data alloc_data = {0};

	alloc_data.len = len;
	alloc_data.heap_id_mask = 0x1 << 1;

	*ion_handle = (void *)ion_alloc_nofd(g_bmdi, &alloc_data);
	if (*ion_handle == NULL)
		return 0;

	return alloc_data.paddr;
}

unsigned int vc_ion_free(void* ion_handle)
{
	ion_free_nofd((struct ion_buffer *)ion_handle);
	return 0;
}

int vc_memcpy_s2d(void *src, uint64_t dst, uint32_t size)
{
	return bmdev_memcpy_s2d_internal(g_bmdi, dst, src, size, true);
}

int vc_memcpy_d2s(void *dst, uint64_t src, uint32_t size)
{
	return bmdev_memcpy_d2s_internal(g_bmdi, dst, src, size, true);
}

int vc_memcpy_c2c(uint64_t dst, uint64_t src, uint32_t size)
{
	return bmdev_memcpy_c2c(g_bmdi, NULL, src, dst, size, true, KERNEL_NOT_USE_IOMMU);
}

static void vc_jpu0_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(0, NULL);
}

static void vc_jpu1_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(1, NULL);
}

static void vc_jpu2_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(2, NULL);
}

static void vc_jpu3_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(3, NULL);
}

static void vc_vpu0_irq_handler(struct bm_device_info *bmdi)
{
	vpu_irq_handler(0, NULL);
}

static void vc_vpu1_irq_handler(struct bm_device_info *bmdi)
{
	vpu_irq_handler(1, NULL);
}

static void vc_vpu2_irq_handler(struct bm_device_info *bmdi)
{
	vpu_irq_handler(2, NULL);
}

int drv_vc_request_irq(void)
{
	bmdrv_submodule_request_irq(g_bmdi, s_jpu_irq[0], vc_jpu0_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi, s_jpu_irq[1], vc_jpu1_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi, s_jpu_irq[2], vc_jpu2_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi, s_jpu_irq[3], vc_jpu3_irq_handler);

	bmdrv_submodule_request_irq(g_bmdi, s_vpu_irq[0], vc_vpu0_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi, s_vpu_irq[1], vc_vpu1_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi, s_vpu_irq[2], vc_vpu2_irq_handler);
	return 0;
}

void drv_vc_free_irq(void)
{
	bmdrv_submodule_free_irq(g_bmdi, s_jpu_irq[0]);
	bmdrv_submodule_free_irq(g_bmdi, s_jpu_irq[1]);
	bmdrv_submodule_free_irq(g_bmdi, s_jpu_irq[2]);
	bmdrv_submodule_free_irq(g_bmdi, s_jpu_irq[3]);

	bmdrv_submodule_free_irq(g_bmdi, s_vpu_irq[0]);
	bmdrv_submodule_free_irq(g_bmdi, s_vpu_irq[1]);
	bmdrv_submodule_free_irq(g_bmdi, s_vpu_irq[2]);
}
void drv_vc_enable_irq(int irq_num)
{
	bmdrv_enable_irq(g_bmdi, irq_num);
}

void drv_vc_disable_irq(int irq_num)
{
	bmdrv_disable_irq(g_bmdi, irq_num);
}

int vc_drv_init(struct bm_device_info *bmdi)
{
	g_bmdi = bmdi;

	drv_base_init();
	drv_vpu_init();
	drv_vc_request_irq();
	return 0;
}

int vc_drv_deinit(struct bm_device_info *bmdi)
{
	drv_vpu_deinit();
	drv_base_deinit();
	drv_vc_free_irq();

	g_bmdi = NULL;
	return 0;
}

