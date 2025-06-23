#ifndef _BM_GMEM_H_
#define _BM_GMEM_H_
#include "bm_bgm.h"

#define BM_MEM_ADDR_NULL (0xfffffffff)

#define MAX_HEAP_CNT 3

#define GMEM_NORMAL 0
#define GMEM_TPU_ONLY 1

struct bm_device_info;

struct bm_handle_info {
	// DECLARE_HASHTABLE(api_htable, 5);
	struct list_head list;
	struct file *file;
	pid_t open_pid;
	u64 gmem_used;
	u64 h_send_api_seq[1];
	u64 h_cpl_api_seq[1];
	struct mutex h_api_seq_mutex;
	int f_owner;
};

struct reserved_mem_info {

	u64 eutable_addr;
	u64 eutable_size;

	u64 armreserved_addr;
	u64 armreserved_size;

	u64 warpaffine_addr;
	u64 warpaffine_size;

	u64 npureserved_addr[MAX_HEAP_CNT];
	u64 npureserved_size[MAX_HEAP_CNT];


};

struct bm_gmem_info {
	struct mutex gmem_mutex;
	struct ion_device idev;
	struct reserved_mem_info resmem_info;
	int (*bm_gmem_init)(struct bm_device_info *);
	void (*bm_gmem_deinit)(struct bm_device_info *);
};

int bmdrv_gmem_init(struct bm_device_info *bmdi);
void bmdrv_gmem_deinit(struct bm_device_info *bmdi);
u64 bmdrv_gmem_total_size(struct bm_device_info *bmdi);
u64 bmdrv_gmem_avail_size(struct bm_device_info *bmdi);
void bmdrv_heap_avail_size(struct bm_device_info *bmdi);
void bmdrv_heap_mem_used(struct bm_device_info *bmdi, struct bm_dev_stat *stat);

struct bm_gmem_addr {
	unsigned long vir_addr;
	unsigned long phy_addr;
};
int bmdrv_gmem_vir_to_phy(struct bm_device_info *bmdi, struct bm_gmem_addr *gmem_addr);


int bmdev_gmem_get_handle_info(struct bm_device_info *bmdi, struct file *file,
		struct bm_handle_info **h_info);
int bmdrv_gmem_ioctl_alloc_mem(struct bm_device_info *bmdi, struct file *file,
		unsigned long arg);
int bmdrv_gmem_ioctl_alloc_mem_ion(struct bm_device_info *bmdi, struct file *file,
		unsigned long arg);
int bmdrv_gmem_ioctl_free_mem(struct bm_device_info *bmdi, struct file *file,
		unsigned long arg);
#endif
