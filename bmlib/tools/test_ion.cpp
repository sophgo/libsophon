#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_memory.h"
#include "ion.h"
//#include "api.h"
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>


struct cv_ion_custom_data {
	unsigned int cmd;
	unsigned long arg;
};


int dev;

void ionVmemFlush(void* vddr, size_t size)
 {
   int ret;
   struct cv_ion_custom_data custom_data;
   struct cvitek_cache_range cache_range;

   cache_range.start = vddr;
   cache_range.size = size;

   custom_data.cmd = ION_IOC_CVITEK_FLUSH_RANGE;
   custom_data.arg = (ulong)&cache_range;

   ret = ioctl(dev, ION_IOC_CUSTOM, &custom_data);
   if (ret < 0) {
	 printf( "ion ionVmemFlush failed");
   }
 }
 void ionVmemInvalidate(void* vaddr, size_t size)
 {
   int ret;
   struct cv_ion_custom_data custom_data;
   struct cvitek_cache_range cache_range;

   cache_range.start = vaddr;
   cache_range.size = size;

   custom_data.cmd = ION_IOC_CVITEK_INVALIDATE_RANGE;
   custom_data.arg = (ulong)&cache_range;

   ret = ioctl(dev, ION_IOC_CUSTOM, &custom_data);
   if (ret < 0) {
	   printf("ioctl ionVmemInvalidate failed");
   }
 }

 
void ionPhyAddrFlush(unsigned long phy_addr, size_t size)
 {
   int ret;
   struct cv_ion_custom_data custom_data;
   struct cvitek_cache_range cache_range;

   cache_range.paddr = phy_addr;
   cache_range.size = size;

   custom_data.cmd = ION_IOC_CVITEK_FLUSH_PHY_RANGE;
   custom_data.arg = (ulong)&cache_range;

   ret = ioctl(dev, ION_IOC_CUSTOM, &custom_data);
   if (ret < 0) {
	 printf( "ion ionPhyAddrFlush failed");
   }
 }
 void ionPhyAddrInvalidate(unsigned long phy_addr, size_t size)
 {
   int ret;
   struct cv_ion_custom_data custom_data;
   struct cvitek_cache_range cache_range;

   cache_range.size = size;
   cache_range.paddr = (uint64_t)phy_addr;

   custom_data.cmd = ION_IOC_CVITEK_INVALIDATE_PHY_RANGE;
   custom_data.arg = (ulong)&cache_range;

   ret = ioctl(dev, ION_IOC_CUSTOM, &custom_data);
   if (ret < 0) {
	   printf("ioctl ionPhyAddrInvalidate failed");
   }
 }


int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;
	bm_device_mem_u64_t dev_buffer;
	u64 dev_vmem;
	u64 *mapped_ptr;
	u32 mem_size = 16*1024;

	ret = bm_dev_request(&handle, 0);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	dev = open("/dev/ion", O_RDWR | O_DSYNC);
	ret = bm_malloc_device_byte_u64(handle, &dev_buffer, mem_size);
	if (ret != BM_SUCCESS) {
		printf("bm_malloc_device_byte_u64 failed, ret = %d\n", ret);
		return -1;
	}
	
  	ret = bm_mem_mmap_device_mem_u64(handle, &dev_buffer, &dev_vmem);
	if (ret != BM_SUCCESS) {
		printf("bm_mem_mmap_device_mem_u64 failed, ret = %d\n", ret);
		return -1;
	}
	mapped_ptr = (u64 *)(uintptr_t)dev_vmem;

	printf("ionVmemFlush\n");
	ionVmemFlush((void *)mapped_ptr, mem_size);


	printf("ionPhyAddrInvalidate\n");
	ionPhyAddrInvalidate(dev_buffer.u.device.device_addr, mem_size);

	printf("ionPhyAddrFlush\n");
	ionPhyAddrFlush(dev_buffer.u.device.device_addr, mem_size);
	
	printf("ionVmemInvalidate\n");
	ionVmemInvalidate(mapped_ptr, mem_size);

	bm_free_device_u64(handle, dev_buffer);
    

    return 0;
}

