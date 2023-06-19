#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "vppion.h"

int queryHeapID(int devFd, enum ion_heap_type_n type)
{
    int ret;
    unsigned int i;
    struct ion_heap_query_n heap_query;
    struct ion_heap_data_n *heap_data;
    int heap_id = -1;

    memset(&heap_query, 0, sizeof(heap_query));
    ret = ioctl(devFd, ION_IOC_HEAP_QUERY_N, &heap_query);
    if (ret < 0 || heap_query.cnt == 0) {
        VppIonErr("ioctl ION_IOC_HEAP_QUERY_N failed");
        return -1;
    }

    heap_data = (struct ion_heap_data_n*)calloc(heap_query.cnt, sizeof(struct ion_heap_data_n));
    if (heap_data == NULL) {
        VppIonErr("calloc failed");
        return -1;
    }
    
    heap_query.heaps = (unsigned long)heap_data;
    ret = ioctl(devFd, ION_IOC_HEAP_QUERY_N, &heap_query);
    if (ret < 0) {
        VppIonErr("ioctl ION_IOC_HEAP_QUERY_N failed");
        return -1;
    }

        heap_id = heap_query.cnt;
        for(i = 0; i < heap_query.cnt; i++) {
            if ((heap_data[i].type == type) && (strcmp(heap_data[i].name, "vpp") == 0)) {
                heap_id = heap_data[i].heap_id;
                break;
            } else if ((heap_data[i].type == type) && (strcmp(heap_data[i].name, "carveout") == 0)) {
                heap_id = heap_data[i].heap_id;
                break;
            }
        }

        if (heap_id == heap_query.cnt)
          heap_id = -1;
        free(heap_data);

    return heap_id;
}

void* ionMalloc(int devFd, ion_para * para)
{
    int ret;
    int heap_id;
    struct ion_allocation_data_n allocData;

    VppIonAssert((devFd > 0) && (para->length > 0));

    allocData.len = para->length;

#ifdef ION_CACHE
    allocData.flags = ION_FLAG_CACHED;
#else
    allocData.flags = 0;
#endif

    heap_id = queryHeapID(devFd, ION_HEAP_TYPE_CARVEOUT_N);
    if (heap_id < 0){
        VppIonErr("ioctl ION_HEAP_TYPE_CARVEOUT_N failed");
        return NULL;
    }
    allocData.heap_id_mask = (1 << heap_id);

    ret = ioctl(devFd, ION_IOC_ALLOC_N, &allocData);
    if (ret < 0) {
        VppIonErr("ioctl ION_IOC_ALLOC_N failed");
        return NULL;
    }

    void *p = mmap(NULL, para->length, PROT_WRITE | PROT_READ, MAP_SHARED, allocData.fd, 0);
    if (p == MAP_FAILED) {
        VppIonErr("vppion mmap failed");
        return NULL;
    }

    para->pa = allocData.paddr;
    para->memFd = allocData.fd;
    para->va = p;
    para->ionDevFd.dev_fd = devFd;
//    printf("dev: %d, va: %p, mem: %d, paddr: %lx, len: %d\n", devFd, p, para->memFd, para->pa, para->length);
    return p;
}

void ionFree(ion_para * para)
{
    VppIonAssert(para->length > 0);
    munmap(para->va, para->length);
//   printf("xavier memfd %d\n", para->memFd );
    close(para->memFd);
}

void ion_flush(const ion_dev_fd_s *ion_dev_fd, void * va, int va_len)
{
    VppIonAssert((va != NULL) && (va_len > 0));

    int ret = 0, devFd = 0, ext_ion_devfd = 0;
    struct ion_custom_data custom_data;
    struct bitmain_cache_range cache_range;

    if ((NULL != ion_dev_fd) && ((ion_dev_fd->dev_fd > 0) && (strncmp(ion_dev_fd->name, "ion", 4) == 0))) {
        /*do nothing*/
        devFd = ion_dev_fd->dev_fd;
        ext_ion_devfd = 1;
    }
    else {
        devFd = open("/dev/ion", O_RDWR | O_DSYNC);
        if (devFd < 0) {
            VppIonErr("open /dev/ion failed\n");
            goto err0;
        }
        ext_ion_devfd = 0;
    }

    custom_data.cmd = ION_IOC_BITMAIN_FLUSH_RANGE;
    custom_data.arg = (unsigned long)&cache_range;
    cache_range.start = va;
    cache_range.size = va_len;
    ret = ioctl(devFd, ION_IOC_CUSTOM, &custom_data);
    if (ret < 0) {
        VppIonErr("ion flush err \n");
    }

    if (ext_ion_devfd == 0) {
        close(devFd);
    }

err0:
    return;
}

void ion_invalidate(const ion_dev_fd_s *ion_dev_fd, void * va, int va_len)
{
    VppIonAssert((va != NULL) && (va_len > 0));

    int ret = 0, devFd = 0, ext_ion_devfd = 0;
    struct ion_custom_data custom_data;
    struct bitmain_cache_range cache_range;

    if ((NULL != ion_dev_fd) && ((ion_dev_fd->dev_fd > 0) && (strncmp(ion_dev_fd->name, "ion", 4) == 0))) {
        /*do nothing*/
        devFd = ion_dev_fd->dev_fd;
        ext_ion_devfd = 1;
    }
    else {
        devFd = open("/dev/ion", O_RDWR | O_DSYNC);
        if (devFd < 0) {
            VppIonErr("open /dev/ion failed\n");
            goto err0;
        }
        ext_ion_devfd = 0;
    }

    custom_data.cmd = ION_IOC_BITMAIN_INVALIDATE_RANGE;
    custom_data.arg = (unsigned long)&cache_range;
    cache_range.start = va;
    cache_range.size = va_len;
    ret = ioctl(devFd, ION_IOC_CUSTOM, &custom_data);
    if (ret < 0) {
        printf("xavier ion invilate failed %d, errno %d\n", ret, errno );
        VppIonErr("ion flush err\n");
    }

    if (ext_ion_devfd == 0) {
        close(devFd);
    }

err0:
    return;
}

