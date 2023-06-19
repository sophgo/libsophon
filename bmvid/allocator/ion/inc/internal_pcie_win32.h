
#ifndef _PCIE_WIN32_ION_H_
#define _PCIE_WIN32_ION_H_
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#define PCIE_IOCTL_MAGIC                          'V'

#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x300, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_FREE_PHYSICALMEMORY            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x301, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_WRITE_VMEM                     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x30f, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VDI_IOCTL_READ_VMEM                      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x310, METHOD_BUFFERED, FILE_ANY_ACCESS)

#if defined (__cplusplus)
extern "C" {
#endif



int pcie_allocator_open(int soc_idx);
int pcie_allocator_close(int soc_idx);

bm_ion_buffer_t* pcie_allocate_buffer(int soc_idx, size_t size, int flags);
int pcie_free_buffer(bm_ion_buffer_t* p);

int pcie_upload_data(uint8_t *host_va, bm_ion_buffer_t* p, size_t size);
int pcie_download_data(uint8_t *host_va, bm_ion_buffer_t* p, size_t size);

int pcie_upload_data2(int soc_idx, uint64_t dst_addr, uint8_t *src_data, size_t size);
int pcie_download_data2(int soc_idx, uint64_t src_addr, uint8_t *dst_data, size_t size);

#if defined (__cplusplus)
}
#endif

#endif
#endif /* _PCIE_WIN32_ION_H_ */

