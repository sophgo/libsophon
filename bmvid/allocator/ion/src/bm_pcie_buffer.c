
#ifdef BM_PCIE_MODE
#ifdef __linux__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "bm_ion.h"
#include "internal_pcie.h"

//#define ION_PRINT_INFO

typedef struct sophon_drv_buffer_s {
    uint32_t size;
    uint64_t base;  /* kernel logical address in use kernel */
    uint64_t pa;
    uint64_t va;    /* virtual user space address */
    uint32_t soc_idx;
} sophon_drv_buffer_t;

typedef struct transfer_opt_s {
    uint32_t size;
    uint64_t src;
    uint64_t dst;
} transfer_opt_t;

static int g_dev_fd[256] = { [0 ... 255] = -1};
static int g_count[256]  = { [0 ... 255] =  0};

static pthread_mutex_t ion_mutex = PTHREAD_MUTEX_INITIALIZER;

int pcie_allocator_open(int soc_idx)
{
    char sophon_device[512] = {0};
    int dev_fd = -1;
    int ret = 0;

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] enter\n", __func__, __LINE__);
#endif

    pthread_mutex_lock(&ion_mutex);

    if (g_dev_fd[soc_idx] >= 0) {
        g_count[soc_idx]++;
        goto Exit;
    }

    sprintf(sophon_device, "/dev/bm-sophon%d", soc_idx);
    dev_fd = open(sophon_device, O_RDWR | O_DSYNC,
                  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (dev_fd < 0) {
        fprintf(stderr, "[%s:%d] Failed to open %s device!\n",
                __func__, __LINE__, sophon_device);
        ret = -1;
        goto Exit;
    }

#ifdef ION_PRINT_INFO
    printf("[%s:%d] Fd=%d\n", __func__, __LINE__, dev_fd);
#endif
    g_dev_fd[soc_idx] = dev_fd;
    g_count[soc_idx] = 1;

Exit:
    pthread_mutex_unlock(&ion_mutex);

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] leave\n", __func__, __LINE__);
#endif

    return ret;
}

int pcie_allocator_close(int soc_idx)
{
    int dev_fd = -1;
    int ret = 0;

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] enter\n", __func__, __LINE__);
#endif

    pthread_mutex_lock(&ion_mutex);

    dev_fd = g_dev_fd[soc_idx];
    if (dev_fd < 0)
        goto Exit;

    if (g_count[soc_idx] > 0) {
        g_count[soc_idx]--;
        if (g_count[soc_idx] > 0)
            goto Exit;
    }

    ret = close(dev_fd);
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] Failed to close device. fd = %d\n",
                __func__, __LINE__, dev_fd);
        goto Exit;
    }

    g_dev_fd[soc_idx] = -1;

Exit:
    pthread_mutex_unlock(&ion_mutex);

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] leave\n", __func__, __LINE__);
#endif

    return ret;
}

bm_ion_buffer_t* pcie_allocate_buffer(int soc_idx, size_t size, int flags)
{
    sophon_drv_buffer_t sdb = {.size = size+32};
    int dev_fd = g_dev_fd[soc_idx];
    int ret;

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] enter\n", __func__, __LINE__);
#endif

    if (dev_fd < 0) {
        fprintf(stderr, "[%s:%d] Bad file descriptor! soc_idx=%d, fd=%d, size=%zd\n",
                __func__, __LINE__, soc_idx, dev_fd, size);
        return NULL;
    }

    bm_ion_buffer_t* p = (bm_ion_buffer_t*)calloc(1, sizeof(bm_ion_buffer_t));
    if (p == NULL) {
        fprintf(stderr, "[%s:%d] calloc failed!\n", __func__, __LINE__);
        return NULL;
    }
    p->context = (void*)calloc(1, sizeof(sophon_drv_buffer_t));
    if (p->context == NULL) {
        fprintf(stderr, "[%s:%d] calloc failed!\n", __func__, __LINE__);
        return NULL;
    }

    ret = ioctl(dev_fd, PCIE_IOCTL_PHYSICAL_MEMORY_ALLOCATE, &sdb);
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] fail to allocate physical memory (size=%d) with ioctl() "
             "request. [error=%s]\n", __func__, __LINE__, sdb.size, strerror(errno));
        return NULL;
    }

    p->devFd   = dev_fd;
    p->soc_idx = soc_idx;
    p->paddr   = sdb.pa &(~31);
    p->size    = size;

    memcpy(p->context, &sdb, sizeof(sophon_drv_buffer_t));

#ifdef ION_PRINT_INFO
    printf("[%s:%d] pcie_buffer soc_idx=%d, fd=%d, pa=0x%lx, size=%d, flags=%d\n",
           __func__, __LINE__, soc_idx, dev_fd, p->paddr, size, flags);

    fprintf(stderr, "[%s:%d] leave\n", __func__, __LINE__);
#endif

    return p;
}

int pcie_free_buffer(bm_ion_buffer_t* p)
{
    sophon_drv_buffer_t sdb = {0};
    int ret;

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] enter\n", __func__, __LINE__);
#endif

    if (!p)
        return 0;
    if (p->devFd < 0) {
        fprintf(stderr, "[%s:%d] Please open /dev/bm-sophonxx first!\n", __func__, __LINE__);
        return -1;
    }
    if (p->context == NULL) {
        fprintf(stderr, "[%s:%d] context is NULL!\n", __func__, __LINE__);
        return -1;
    }

    memcpy(&sdb, p->context, sizeof(sophon_drv_buffer_t));

    ret = ioctl(p->devFd, PCIE_IOCTL_PHYSICAL_MEMORY_FREE, &sdb);
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] fail to free physical memory with ioctl() request. "
             "[error=%s]\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    free(p->context);

    free(p);

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] leave\n", __func__, __LINE__);
#endif

    return 0;
}

int pcie_download_data(uint8_t* host_va, bm_ion_buffer_t* p, size_t size)
{
    transfer_opt_t opt = {0};
    int ret = 0;

    opt.src  = p->paddr;
    opt.dst  = (uint64_t)host_va;
    opt.size = size;

    ret = ioctl(p->devFd, PCIE_IOCTL_DOWNLOAD, &opt);
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] fail to download data with ioctl() request. "
                "[error=%s]\n", __func__, __LINE__, strerror(errno));
        ret = -1;
    }

    return ret;
}

int pcie_upload_data(uint8_t* host_va, bm_ion_buffer_t* p, size_t size)
{
    transfer_opt_t opt = {0};
    int ret = 0;

    opt.src  = (uint64_t)host_va;
    opt.dst  = p->paddr;
    opt.size = size;

    ret = ioctl(p->devFd, PCIE_IOCTL_UPLOAD, &opt);
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] fail to upload data with ioctl() request. "
                "[error=%s]\n", __func__, __LINE__, strerror(errno));
        ret = -1;
    }

    return ret;
}

int pcie_download_data2(int soc_idx, uint64_t src_addr, uint8_t *dst_data, size_t size)
{
    int devFd = g_dev_fd[soc_idx];
    transfer_opt_t opt = {0};

    opt.src  = src_addr;
    opt.dst  = (uint64_t)dst_data;
    opt.size = size;
    int ret = ioctl(devFd, PCIE_IOCTL_DOWNLOAD, &opt);
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] fail to download data with ioctl() request. "
                "[error=%s]\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    return size;
}

int pcie_upload_data2(int soc_idx, uint64_t dst_addr, uint8_t *src_data, size_t size)
{
    int devFd = g_dev_fd[soc_idx];
    transfer_opt_t opt = {0};

    if (!src_data)
        return -1;

    opt.src  = (uint64_t)src_data;
    opt.dst  = dst_addr;
    opt.size = size;
    int ret = ioctl(devFd, PCIE_IOCTL_UPLOAD, &opt);
    if (ret < 0) {
        fprintf(stderr, "[%s:%d] fail to upload data with ioctl() request. "
                "[error=%s]\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    return size;
}

#endif
#endif

