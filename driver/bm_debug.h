#ifndef __BM_DEBUG_H__
#define __BM_DEBUG_H__
#include <linux/kthread.h>
#include <linux/version.h>

int bmdrv_proc_init(void);
void bmdrv_proc_deinit(void);
int bmdrv_proc_file_init(struct bm_device_info *bmdi);
void bmdrv_proc_file_deinit(struct bm_device_info *bmdi);
int bm_arm9fw_log_init(struct bm_device_info *bmdi);
void bm_dump_arm9fw_log(struct bm_device_info *bmdi, int count);
char *bmdrv_get_error_string(int error);
int i2c2_deinit(struct bm_device_info *bmdi);

struct bm_arm9fw_log_mem {
	void *host_vaddr;
	u64 host_paddr;
	u32 host_size;
	u64 device_paddr;
	u32 device_size;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define BM_PROC_OWNER    .proc_ioctl
#define BM_PROC_OPEN     .proc_open
#define BM_PROC_READ     .proc_read
#define BM_PROC_WRITE    .proc_write
#define BM_PROC_LLSEEK   .proc_lseek
#define BM_PROC_RELEASE  .proc_release
#define BM_PROC_FILE_OPS  proc_ops
#define BM_PROC_MODULE    NULL
#else
#define BM_PROC_OWNER    .owner
#define BM_PROC_OPEN     .open
#define BM_PROC_READ     .read
#define BM_PROC_WRITE    .write
#define BM_PROC_LLSEEK   .llseek
#define BM_PROC_RELEASE  .release
#define BM_PROC_FILE_OPS file_operations
#define BM_PROC_MODULE THIS_MODULE
#endif

#endif
