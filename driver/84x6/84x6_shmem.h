#ifndef __84X6_SHMEM_H__
#define __84X6_SHMEM_H__

#include <linux/types.h>
#include "bm_common.h"

#define SHARED_MEM_PHYS_BASE TPU_84X6_SHMEM_BASE_ADDR
#define BM_SHARED_BLOCK_SIZE 4096
#define BM_SHARED_MAX_HANDLES 256
#define BM_SHARED_TOTAL_MEM_SIZE (BM_SHARED_MAX_HANDLES * BM_SHARED_BLOCK_SIZE)


int bmdev_shared_mem_init(struct bm_device_info *bmdi);
void bmdev_shared_mem_exit(struct bm_device_info *bmdi);
//alloc mem first,get phyaddr by bmdev_shared_mem_get_phys_addr
int bmdev_shared_mem_alloc(struct bm_device_info *bmdi, size_t size);
void bmdev_shared_mem_free(struct bm_device_info *bmdi, int handle);
unsigned long bmdev_shared_mem_get_phys_addr(struct bm_device_info *bmdi,
                                             int handle);
unsigned long bm84x6_shmem_get_offset(struct bm_device_info *bmdi,
                                          int handle);
size_t bmdev_shared_mem_get_size(struct bm_device_info *bmdi, int handle);
void bmdev_shared_mem_get_info(struct bm_device_info *bmdi, size_t *total,
                               size_t *used, size_t *free_size);
void bm84x6_shmem_reg_write(struct bm_device_info *bmdi, u32 reg_offset,
                                u32 val);
int bmdev_copy_to_shmem(struct bm_device_info *bmdi, unsigned int phys_addr,
                        bm_kapi_header_t * api_header_p, bm_api_t * bm_api_p,
                        bm_kapi_opt_header_t * api_opt_header_p,
                        bool api_from_userspace);
u32 bm84x6_shmem_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
int bm84x6_shmem_read_packet(struct bm_device_info *bmdi, u32 packet_addr,
                                 phys_addr_t * phys_out, void *buf,
                                 size_t buf_len);
int bm84x6_shmem_read_packet_at(struct bm_device_info *bmdi,
                                    u32 packet_addr, size_t byte_offset,
                                    void *buf, size_t buf_len);

#endif /* __84X6_SHMEM_H__ */
