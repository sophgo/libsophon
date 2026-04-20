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
#include <linux/platform_device.h>

#include "bm_common.h"
#include "bm_memcpy.h"
#include "bm_irq.h"
#include "bm_gmem.h"

#define MAX_NUM_VPU_CORE_SOC			3
#define MAX_NUM_JPU_CORE_SOC			4
#define MAX_NUM_SOC						8

extern int vc_drv_init(void);
extern void vc_drv_exit(void);
extern int vc_drv_plat_probe(struct platform_device *pdev);
extern int vc_drv_plat_remove(struct platform_device *pdev);
extern int drv_base_init(void);
extern int drv_base_deinit(void);
extern irqreturn_t jpu_irq_handler(int core, void *dev_id);
extern irqreturn_t vpu_irq_handler(int core, void *dev_id);
extern void vc_drv_set_socnum(int num);
extern void vc_drv_threadpool_init(int soc_idx);

struct bm_device_info *g_bmdi[2] = {0};
static int s_jpu_irq[MAX_NUM_JPU_CORE_SOC] = {46, 47, 48, 49};
static int s_vpu_irq[MAX_NUM_VPU_CORE_SOC] = {45, 39, 42};
static int soc_cnt = 0;

unsigned int pcie_read_reg(int soc_idx, unsigned int addr)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return 0;
	}

	return bm_read32(g_bmdi[soc_idx], addr);
}

unsigned int pcie_write_reg(int soc_idx, unsigned int addr, unsigned int data)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return 0;
	}

	return bm_write32(g_bmdi[soc_idx], addr, data);
}

uint64_t pcie_ion_alloc(int soc_idx, uint32_t len, void** ion_handle)
{
	struct ion_allocation_data alloc_data = {0};

	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return 0;
	}

	alloc_data.len = len;
	alloc_data.heap_id_mask = 0x1 << 1;

	*ion_handle = (void *)ion_alloc_nofd(g_bmdi[soc_idx], &alloc_data);
	if (*ion_handle == NULL)
		return 0;

	return alloc_data.paddr;
}

unsigned int pcie_ion_free(void* ion_handle)
{
	ion_free_nofd((struct ion_buffer *)ion_handle);
	return 0;
}

int pcie_memcpy_s2d(int soc_idx, uint64_t dst, void *src, uint32_t size)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return 0;
	}

	return bmdev_memcpy_s2d_internal(g_bmdi[soc_idx], dst, src, size, true);
}

int pcie_memcpy_d2s(int soc_idx, void *dst, uint64_t src, uint32_t size)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return 0;
	}

	return bmdev_memcpy_d2s_internal(g_bmdi[soc_idx], dst, src, size, true);
}

int pcie_memcpy_c2c(int soc_idx, uint64_t dst, uint64_t src, uint32_t size)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return 0;
	}

	return bmdev_memcpy_c2c(g_bmdi[soc_idx], NULL, src, dst, size, true, KERNEL_NOT_USE_IOMMU);
}

void pcie_enable_irq(int soc_idx, int irq_num)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return ;
	}

	bmdrv_enable_irq(g_bmdi[soc_idx], irq_num);
}

void pcie_disable_irq(int soc_idx, int irq_num)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return ;
	}

	bmdrv_disable_irq(g_bmdi[soc_idx], irq_num);
}

static void vc_jpu0_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(MAX_NUM_JPU_CORE_SOC * bmdi->dev_index + 0, NULL);
}

static void vc_jpu1_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(MAX_NUM_JPU_CORE_SOC * bmdi->dev_index + 1, NULL);
}

static void vc_jpu2_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(MAX_NUM_JPU_CORE_SOC * bmdi->dev_index + 2, NULL);
}

static void vc_jpu3_irq_handler(struct bm_device_info *bmdi)
{
	jpu_irq_handler(MAX_NUM_JPU_CORE_SOC * bmdi->dev_index + 3, NULL);
}

static void vc_vpu0_irq_handler(struct bm_device_info *bmdi)
{
	vpu_irq_handler(MAX_NUM_VPU_CORE_SOC * bmdi->dev_index + 0, NULL);
}

static void vc_vpu1_irq_handler(struct bm_device_info *bmdi)
{
	vpu_irq_handler(MAX_NUM_VPU_CORE_SOC * bmdi->dev_index + 1, NULL);
}

static void vc_vpu2_irq_handler(struct bm_device_info *bmdi)
{
	vpu_irq_handler(MAX_NUM_VPU_CORE_SOC * bmdi->dev_index + 2, NULL);
}

static int drv_vc_request_irq(int soc_idx)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return 0;
	}

	bmdrv_submodule_request_irq(g_bmdi[soc_idx], s_jpu_irq[0], vc_jpu0_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi[soc_idx], s_jpu_irq[1], vc_jpu1_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi[soc_idx], s_jpu_irq[2], vc_jpu2_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi[soc_idx], s_jpu_irq[3], vc_jpu3_irq_handler);

	bmdrv_submodule_request_irq(g_bmdi[soc_idx], s_vpu_irq[0], vc_vpu0_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi[soc_idx], s_vpu_irq[1], vc_vpu1_irq_handler);
	bmdrv_submodule_request_irq(g_bmdi[soc_idx], s_vpu_irq[2], vc_vpu2_irq_handler);
	return 0;
}

static void drv_vc_free_irq(int soc_idx)
{
	if (g_bmdi[soc_idx] == NULL) {
		pr_err("Only %d PCIe cards were inserted, but you operated the %dth PCIe card\n", soc_cnt, soc_idx+1);
		return ;
	}

	bmdrv_submodule_free_irq(g_bmdi[soc_idx], s_jpu_irq[0]);
	bmdrv_submodule_free_irq(g_bmdi[soc_idx], s_jpu_irq[1]);
	bmdrv_submodule_free_irq(g_bmdi[soc_idx], s_jpu_irq[2]);
	bmdrv_submodule_free_irq(g_bmdi[soc_idx], s_jpu_irq[3]);

	bmdrv_submodule_free_irq(g_bmdi[soc_idx], s_vpu_irq[0]);
	bmdrv_submodule_free_irq(g_bmdi[soc_idx], s_vpu_irq[1]);
	bmdrv_submodule_free_irq(g_bmdi[soc_idx], s_vpu_irq[2]);
}

int vc_init(struct bm_device_info *bmdi)
{
	if (bmdi == NULL)
		return -1;

	g_bmdi[bmdi->dev_index] = bmdi;

	if (bmdi->dev_index == 0) {
		drv_base_init();
		vc_drv_init();
		vc_drv_plat_probe(NULL);
	} else {
		vc_drv_threadpool_init(bmdi->dev_index);
	}
	drv_vc_request_irq(bmdi->dev_index);
	soc_cnt = bmdi->dev_index + 1;
	vc_drv_set_socnum(soc_cnt);

	return 0;
}

int vc_exit(struct bm_device_info *bmdi)
{
	if (bmdi == NULL)
		return -1;

	drv_vc_free_irq(bmdi->dev_index);
	if (bmdi->dev_index == 0) {
		vc_drv_plat_remove(NULL);
		vc_drv_exit();
		drv_base_deinit();
	}
	soc_cnt = 0;
	g_bmdi[bmdi->dev_index] = NULL;


	return 0;
}

