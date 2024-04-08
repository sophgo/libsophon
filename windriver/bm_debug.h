#ifndef __BM_DEBUG_H__
#define __BM_DEBUG_H__


//int bmdrv_proc_init(void);
//void bmdrv_proc_deinit(void);
//int bmdrv_proc_file_init(struct bm_device_info *bmdi);
//void bmdrv_proc_file_deinit(struct bm_device_info *bmdi);
int bm_arm9fw_log_init(struct bm_device_info *bmdi);
void bm_dump_arm9fw_log(struct bm_device_info *bmdi, int count);
char *bmdrv_get_error_string(int error);

struct bm_arm9fw_log_mem {
	void *host_vaddr;
	u64 host_paddr;
	u32 host_size;
	u64 device_paddr;
	u32 device_size;
	WDFDMAENABLER log_mem_DmaEnabler;
	WDFCOMMONBUFFER  log_mem_CommonBuffer;
	PUCHAR           log_mem_vaddr;
	PHYSICAL_ADDRESS log_mem_paddr;  // Logical Address (PHY ADDR)
};

#endif
