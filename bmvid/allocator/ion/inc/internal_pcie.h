
#ifndef _PCIE_LINUX_ION_H_
#define _PCIE_LINUX_ION_H_
#ifdef __linux__
#define PCIE_IOCTL_MAGIC                          'V'

#define PCIE_IOCTL_PHYSICAL_MEMORY_ALLOCATE       _IO(PCIE_IOCTL_MAGIC, 0)
#define PCIE_IOCTL_PHYSICAL_MEMORY_FREE           _IO(PCIE_IOCTL_MAGIC, 1)
#define PCIE_IOCTL_UPLOAD                         _IO(PCIE_IOCTL_MAGIC, 20)
#define PCIE_IOCTL_DOWNLOAD                       _IO(PCIE_IOCTL_MAGIC, 21)

int pcie_allocator_open(int soc_idx);
int pcie_allocator_close(int soc_idx);

bm_ion_buffer_t* pcie_allocate_buffer(int soc_idx, size_t size, int flags);
int pcie_free_buffer(bm_ion_buffer_t* p);

int pcie_upload_data(uint8_t *host_va, bm_ion_buffer_t* p, size_t size);
int pcie_download_data(uint8_t *host_va, bm_ion_buffer_t* p, size_t size);

int pcie_upload_data2(int soc_idx, uint64_t dst_addr, uint8_t *src_data, size_t size);
int pcie_download_data2(int soc_idx, uint64_t src_addr, uint8_t *dst_data, size_t size);
#endif
#endif /* _PCIE_LINUX_ION_H_ */

