#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "bm_ion.h"
#ifdef BM_PCIE_MODE
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#include "internal_pcie_win32.h"
#else
#include "internal_pcie.h"
#endif
#include <stdlib.h>
#include "internal.h"
#endif

int bm_ion_allocator_open(int soc_idx)
{
#ifdef BM_PCIE_MODE
    return pcie_allocator_open(soc_idx);
#else
    return open_ion_allocator();
#endif
}

int bm_ion_allocator_close(int soc_idx)
{
#ifdef BM_PCIE_MODE
    return pcie_allocator_close(soc_idx);
#else
    return close_ion_allocator();
#endif
}

bm_ion_buffer_t* bm_ion_allocate_buffer(int soc_idx, size_t size, int flags)
{
    bm_ion_buffer_t* p = NULL;
#ifdef BM_PCIE_MODE
    p = pcie_allocate_buffer(soc_idx, size, flags);
#else
    p = ion_allocate_buffer(size, flags);
#endif
    if (p==NULL)
        return NULL;

    p->heap_type = (flags>>4) & 0xf;
    p->flags     = flags & 0xf;

    return p;
}

int bm_ion_free_buffer(bm_ion_buffer_t* p)
{
#ifdef BM_PCIE_MODE
    return pcie_free_buffer(p);
#else
    return ion_free_buffer(p);
#endif
}


int bm_ion_download_data(uint8_t *host_va, bm_ion_buffer_t* p, size_t size)
{
    if (p==NULL)
    {
        fprintf(stderr, "[%s,%d] invalid ion buffer pointer!\n", __func__, __LINE__);
        return -1;
    }
    if (host_va==NULL)
    {
        fprintf(stderr, "[%s,%d] invalid host buffer pointer!\n", __func__, __LINE__);
        return -1;
    }
    if (p->size < size)
    {
        fprintf(stderr, "ion buffer size(%d) is less than output data size(%ld)",
                p->size, size);
        return -1;
    }
#ifdef BM_PCIE_MODE
    return pcie_download_data(host_va, p, size);
#else
    int b_mapped = 0;
    int ret;
    if (p->vaddr==NULL) {
        ret = bm_ion_map_buffer(p, BM_ION_MAPPING_FLAG_READ);
        if (ret < 0) {
            fprintf(stderr, "retrieving virtual address for physical memory failed.\n");
            return -1;
        }

        ret = bm_ion_invalidate_buffer(p);
        if (ret < 0) {
            fprintf(stderr, "Invalidating physical memory failed.\n");
            return -1;
        }

        b_mapped = 1;
    }

    memcpy(host_va, p->vaddr, size);

    if (b_mapped) {
        ret = bm_ion_unmap_buffer(p);
        if (ret < 0) {
            fprintf(stderr, "shutting down virtual address for %d bytes of physical memory failed\n", p->size);
            return -1;
        }
    }
    return 0;
#endif
}

int bm_ion_upload_data(uint8_t *host_va, bm_ion_buffer_t* p, size_t size)
{
    if (p==NULL)
    {
        fprintf(stderr, "[%s,%d] invalid ion buffer pointer!\n", __func__, __LINE__);
        return -1;
    }
    if (host_va==NULL)
    {
        fprintf(stderr, "[%s,%d] invalid host buffer pointer!\n", __func__, __LINE__);
        return -1;
    }
    if (p->size < size)
    {
        fprintf(stderr, "ion buffer size(%d) is less than input data size(%ld)",
                p->size, size);
        return -1;
    }
#ifdef BM_PCIE_MODE
    return pcie_upload_data(host_va, p, size);
#else
    int b_mapped = 0;
    int ret;
    if (p->vaddr==NULL) {
        ret = bm_ion_map_buffer(p, BM_ION_MAPPING_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "retrieving virtual address for physical memory failed.\n");
            return -1;
        }

        ret = bm_ion_invalidate_buffer(p);
        if (ret < 0) {
            fprintf(stderr, "Invalidating physical memory failed.\n");
            return -1;
        }

        b_mapped = 1;
    }

    memcpy(p->vaddr, host_va, size);
    if (p->size > size)
        memset((p->vaddr+size), 0, (p->size-size)); // TODO

    /* Flush cache after copying data to DMA buffer */
    if (p->flags == BM_ION_FLAG_CACHED && (p->map_flags & BM_ION_MAPPING_FLAG_WRITE)) {
        ret = bm_ion_flush_buffer(p);
        if (ret < 0)
            return -1;
    }

    if (b_mapped) {
        ret = bm_ion_unmap_buffer(p);
        if (ret < 0) {
            fprintf(stderr, "shutting down virtual address for %d bytes of physical memory failed\n", p->size);
            return -1;
        }
    }
    return 0;
#endif
}

int bm_ion_download_data2(int soc_idx, uint64_t src_addr, uint8_t *dst_data, size_t size)
{
#ifdef BM_PCIE_MODE
    return pcie_download_data2(soc_idx, src_addr, dst_data, size);
#else
    return -1;
#endif
}

int bm_ion_upload_data2(int soc_idx, uint64_t dst_addr, uint8_t *src_data, size_t size)
{
#ifdef BM_PCIE_MODE
    return pcie_upload_data2(soc_idx, dst_addr, src_data, size);
#else
    return -1;
#endif
}

/* soc mode */
int bm_ion_map_buffer(bm_ion_buffer_t* p, int flags)
{
#ifdef BM_PCIE_MODE
    return 0;
#else
    return ion_map_buffer(p, flags);
#endif
}

int bm_ion_unmap_buffer(bm_ion_buffer_t* p)
{
#ifdef BM_PCIE_MODE
    return 0;
#else
    return ion_unmap_buffer(p);
#endif
}


int bm_ion_flush_buffer(bm_ion_buffer_t* p)
{
#ifdef BM_PCIE_MODE
    return 0;
#else
    return ion_flush_buffer(p);
#endif
}

int bm_ion_invalidate_buffer(bm_ion_buffer_t* p)
{
#ifdef BM_PCIE_MODE
    return 0;
#else
    return ion_invalidate_buffer(p);
#endif
}

