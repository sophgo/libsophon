
#ifndef BM_PCIE_MODE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>   /* mmap */
#include <sys/ioctl.h>  /* ioctl */
#include <sys/errno.h>  /* errno */
#include <pthread.h>
#include "internal.h"
#include "bm_ion.h"

//#define ION_PRINT_INFO

#define HEAP_MAX_COUNT   3
static int   ion_malloc(bm_ion_buffer_t* p, size_t size, int flags);
static void* ion_map(int memFd, size_t size, int flags);
static int   ion_unmap(void* vaddr, size_t size);
static void  ion_free(int memFd);
static int ion_queryHeapID(int devFd, ion_heap_t type, char* heap_name, uint32_t* heap_id);

/* ion fd shared by threads in one process */
static volatile int g_ion_fd = -1;
static long g_ion_fd_open_count = 0;
static uint32_t g_heap_id[HEAP_MAX_COUNT] = {0};
static pthread_mutex_t ion_mutex = PTHREAD_MUTEX_INITIALIZER;

int open_ion_allocator()
{
    int ret = 0;
    char* heap_name[HEAP_MAX_COUNT] = {"vpp","npu","vpu"};
    pthread_mutex_lock(&ion_mutex);

    g_ion_fd_open_count++;
    if (g_ion_fd >= 0)
        goto Exit;

    g_ion_fd = open("/dev/ion", O_RDWR | O_DSYNC,
                    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (g_ion_fd < 0)
    {
        fprintf(stderr, "[%s:%d] Failed to open ion device! [error=%s].\n",
                __func__, __LINE__, strerror(errno));
        ret = -1;
        goto Exit;
    }

#ifdef ION_PRINT_INFO
    printf("[%s:%d] ion Fd=%d\n", __func__, __LINE__, g_ion_fd);
#endif
    for(int i = 0; i < HEAP_MAX_COUNT; i++)
    {
        ret = ion_queryHeapID(g_ion_fd, ION_HEAP_TYPE_CARVEOUT, heap_name[i], &g_heap_id[i]);
        if (ret < 0)
        {
            /* compatible with the old ion heap. */
            if (i == BM_ION_FLAG_VPU)
            {
                g_heap_id[i] = -1;
                ret = 0;
            }
            else
            {
                fprintf(stderr, "[%s:%d] WARNING! ioctl ION_IOC_HEAP_QUERY failed, heap name = %s\n",
                        __func__, __LINE__, heap_name[i]);
            }
        }
    }
Exit:
    pthread_mutex_unlock(&ion_mutex);

    return ret;
}

int close_ion_allocator()
{
    int ret = 0;

    pthread_mutex_lock(&ion_mutex);

    g_ion_fd_open_count--;
    if (g_ion_fd_open_count == 0)
    {
        if (g_ion_fd < 0)
            goto Exit;

        ret = close(g_ion_fd);
        if (ret < 0)
        {
            fprintf(stderr, "[%s:%d] Failed to close ion device. [error=%s].\n",
                    __func__, __LINE__, strerror(errno));
            goto Exit;
        }
        g_ion_fd = -1;
    }
    else if (g_ion_fd_open_count < 0)
    {
        g_ion_fd_open_count = 0;
    }

Exit:
    pthread_mutex_unlock(&ion_mutex);
    return ret;
}

bm_ion_buffer_t* ion_allocate_buffer(size_t size, int flags)
{
    int ret;

    if (g_ion_fd < 0)
    {
        fprintf(stderr, "[%s:%d] Please open /dev/ion first!\n", __func__, __LINE__);
        return NULL;
    }

    bm_ion_buffer_t* p = (bm_ion_buffer_t*)calloc(1, sizeof(bm_ion_buffer_t));
    if (p == NULL)
    {
        fprintf(stderr, "[%s:%d] calloc failed!\n", __func__, __LINE__);
        return NULL;
    }

    //p->devFd = g_ion_fd;

    ret = ion_malloc(p, size, flags);
    if (ret < 0)
    {
        fprintf(stderr, "[%s:%d] ion_malloc failed!\n", __func__, __LINE__);
        free(p);
        return NULL;
    }

    return p;
}

int ion_free_buffer(bm_ion_buffer_t* p)
{
    if (g_ion_fd < 0)
    {
        fprintf(stderr, "[%s:%d] Please open /dev/ion first!\n", __func__, __LINE__);
        return -1;
    }

    if (!p)
        return 0;

    if (p->memFd < 0)
        return 0;

    ion_free(p->memFd);
    p->memFd = -1;

    free(p);

    return 0;
}

int ion_map_buffer(bm_ion_buffer_t* p, int flags)
{
    if (g_ion_fd < 0)
    {
        fprintf(stderr, "[%s:%d] Please open /dev/ion first!\n", __func__, __LINE__);
        return -1;
    }

    if (!p)
    {
        fprintf(stderr, "[%s:%d] invalid pointer!\n", __func__, __LINE__);
        return -1;
    }

    if (p->vaddr)
        return 0;

    p->vaddr = ion_map(p->memFd, p->size, flags);
    if (p->vaddr==NULL)
        return -1;

    p->map_flags = flags;

#ifdef ION_PRINT_INFO
    printf("[%s:%d] ion fd=%d, memFd=%d, vaddr=%p, size=%d,flags=%d\n",
           __func__, __LINE__,
           g_ion_fd, p->memFd, p->vaddr, p->size, flags);
#endif
    return 0;
}

int ion_unmap_buffer(bm_ion_buffer_t* p)
{
    if (g_ion_fd < 0)
    {
        fprintf(stderr, "[%s:%d] Please open /dev/ion first!\n", __func__, __LINE__);
        return -1;
    }

    if (!p)
    {
        fprintf(stderr, "[%s:%d] invalid pointer!\n", __func__, __LINE__);
        return -1;
    }

    if (p->vaddr)
    {
        ion_unmap(p->vaddr, p->size);
        p->vaddr = NULL;
    }

    return 0;
}

int ion_flush_buffer(bm_ion_buffer_t* p)
{
    int ret = 0 ;
    struct ion_custom_data custom_data = {0};
    struct bitmain_cache_range cache_range = {0};

    if (p == NULL)
    {
        fprintf(stderr, "[%s:%d] invalid pointer!\n", __func__, __LINE__);
        return -1;
    }

    if (g_ion_fd < 0)
    {
        fprintf(stderr, "[%s:%d] Please open /dev/ion first!\n", __func__, __LINE__);
        return -1;
    }
    custom_data.cmd = ION_IOC_BITMAIN_FLUSH_RANGE;
    custom_data.arg = (unsigned long)&cache_range;
    cache_range.start = p->vaddr;
    cache_range.size = p->size;
    ret = ioctl(g_ion_fd, ION_IOC_CUSTOM, &custom_data);
    if (ret < 0)
    {
        fprintf(stderr, "ion invilate failed %d. [error=%s].\n", ret, strerror(errno));
        return -1;
    }
    return ret;
}

int ion_invalidate_buffer(bm_ion_buffer_t* p)
{
    int ret = 0 ;
    struct ion_custom_data custom_data = {0};
    struct bitmain_cache_range cache_range = {0};

    if (p == NULL)
    {
        fprintf(stderr, "[%s:%d] invalid pointer!\n", __func__, __LINE__);
        return -1;
    }

    if (g_ion_fd < 0)
    {
        fprintf(stderr, "[%s:%d] Please open /dev/ion first!\n", __func__, __LINE__);
        return -1;
    }

    custom_data.cmd = ION_IOC_BITMAIN_INVALIDATE_RANGE;
    custom_data.arg = (unsigned long)&cache_range;
    cache_range.start = p->vaddr;
    cache_range.size = p->size;
    ret = ioctl(g_ion_fd, ION_IOC_CUSTOM, &custom_data);
    if (ret < 0)
    {
        fprintf(stderr, "ion invilate failed %d. [error=%s].\n", ret, strerror(errno));
        return -1;
    }
    return ret;
}

static int ion_queryHeapID(int devFd, ion_heap_t type, char* name, uint32_t* heap_id)
{
    ion_heap_query_t heap_query;
    ion_heap_data_t *heap_data;
    unsigned int i;
    int ret = 0;

    memset(&heap_query, 0, sizeof(heap_query));

    ret = ioctl(devFd, ION_IOC_HEAP_QUERY, &heap_query);
    if (ret < 0 || heap_query.cnt == 0)
    {
        fprintf(stderr, "[%s:%d] ioctl ION_IOC_HEAP_QUERY failed. [error=%s].\n",
                __func__, __LINE__, strerror(errno));
        return -1;
    }

    heap_data = (ion_heap_data_t*)calloc(heap_query.cnt, sizeof(ion_heap_data_t));
    if (heap_data == NULL)
    {
        fprintf(stderr, "[%s:%d] calloc failed",
                __func__, __LINE__);
        return -1;
    }

    heap_query.heaps = (uint64_t)heap_data;
    ret = ioctl(devFd, ION_IOC_HEAP_QUERY, &heap_query);
    if (ret < 0)
    {
        fprintf(stderr, "[%s:%d] ioctl ION_IOC_HEAP_QUERY failed. [error=%s].\n",
                __func__, __LINE__, strerror(errno));
        free(heap_data);
        return -1;
    }

    for (i=0; i<heap_query.cnt; i++)
    {
#ifdef ION_PRINT_INFO
        printf("[%s:%d] The %dth heap: type=%d, name=%s\n",
               __func__, __LINE__, i, heap_data[i].type, heap_data[i].name);
#endif
        if ((heap_data[i].type == type) && (strcmp(heap_data[i].name, name) == 0))
        {
            *heap_id = heap_data[i].heap_id;
            break;
        }
    }

    if (i >= heap_query.cnt)
    {
        if (strcmp(name, "vpu") != 0)
            fprintf(stderr, "[%s:%d] heap %s is not found!\n",
                    __func__, __LINE__, name);
        ret = -1;
    }

    free(heap_data);

    return ret;
}

static int ion_malloc(bm_ion_buffer_t* p, size_t size, int flags)
{
    ion_allocation_data_t allocData;
    int heap_type = (flags>>4) & 0xf;
    int flag = flags & 0xf;
    int ret;

    if (heap_type >= HEAP_MAX_COUNT || heap_type < 0)
    {
        fprintf(stderr, "[%s:%d] invalid heap type %d.\n",
                __func__, __LINE__, heap_type);
        return -1;
    }

    if (flag != BM_ION_FLAG_CACHED &&
        flag != BM_ION_FLAG_WRITECOMBINE)
    {
        fprintf(stderr, "[%s:%d] invalid values of map flags are %d, %d ,flag = 0x%x.\n",
                __func__, __LINE__,
                BM_ION_FLAG_CACHED, BM_ION_FLAG_WRITECOMBINE, flag);
        return -1;
    }

    if (heap_type == BM_ION_FLAG_VPU && (int)(g_heap_id[heap_type]) < 0)
    {
#ifdef ION_PRINT_INFO
        fprintf(stderr, "[%s:%d] can not use ion_vpu, maybe need to update the linux kernel. Now use the ion_vpp\n",
                __func__, __LINE__);
#endif
        heap_type = 0;
    }

    memset(&allocData, 0, sizeof(allocData));

    allocData.len = size;
    /* flag=1: mappings of this buffer should be cached,
     * ion will do cache maintenance when the buffer is mapped for dma. */
    if (flag == BM_ION_FLAG_CACHED)
        allocData.flags = 1;
    else
        allocData.flags = 0;

    allocData.heap_id_mask = (1u << g_heap_id[heap_type]);

    ret = ioctl(g_ion_fd, ION_IOC_ALLOC, &allocData);
    if (ret < 0)
    {
        fprintf(stderr, "[%s:%d] ioctl ION_IOC_ALLOC failed. [error=%s].\n",
                __func__, __LINE__, strerror(errno));
        return -1;
    }

    p->memFd = allocData.fd;
    p->paddr = allocData.paddr;
    p->vaddr = NULL;
    p->size  = size;

#ifdef ION_PRINT_INFO
    printf("[%s:%d] ion fd=%d, memFd=%d, paddr=0x%lx, size=%zd, flags=%d\n",
           __func__, __LINE__, g_ion_fd, p->memFd, p->paddr, size, flags);
#endif

    return 0;
}

static void* ion_map(int memFd, size_t size, int flags)
{
    void *va = NULL;
    int prot = 0;

    if (flags & BM_ION_MAPPING_FLAG_WRITE)
        prot |= PROT_WRITE;
    if (flags & BM_ION_MAPPING_FLAG_READ)
        prot |= PROT_READ;

    va = mmap(NULL, size, prot, MAP_SHARED, memFd, 0);
    if (va == MAP_FAILED)
    {
        fprintf(stderr, "[%s:%d] mmap failed. [error=%s]. ion fd: %d, mem fd: %d, size: %zd, prot: %d.\n",
                __func__, __LINE__, strerror(errno), g_ion_fd, memFd, size, prot);
        return NULL;
    }

#ifdef ION_PRINT_INFO
    printf("[%s:%d] ion fd: %d, memFd: %d, vaddr: %p, size: %zd\n",
           __func__, __LINE__, g_ion_fd, memFd, va, size);
#endif

    return va;
}

static int ion_unmap(void* vaddr, size_t size)
{
    int ret;
    ret = munmap(vaddr, size);
    if (ret != 0)
    {
        fprintf(stderr, "[%s:%d] munmap failed. [error=%s]. vaddr: %p, size: %zd.\n",
                __func__, __LINE__, strerror(errno), vaddr, size);
        return -1;
    }

    return 0;
}

static void ion_free(int memFd)
{
#ifdef ION_PRINT_INFO
    printf("[%s:%d] ion fd: %d, memFd: %d\n",
           __func__, __LINE__, g_ion_fd, memFd);
#endif
    close(memFd);
}
#endif

