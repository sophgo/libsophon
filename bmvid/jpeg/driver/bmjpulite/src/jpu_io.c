
#include <stdlib.h>
#include "jpu_types.h"
#include "jpu_lib.h"
#include "jpu_io.h"
#include "jpu_logging.h"
#include "../jdi/jdi.h"
#ifndef  _WIN32
int IOGetPhyMem(jpu_mem_desc* pmd, int flags)
{
    jpu_buffer_t* pb;
    int ret;

    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    pb = (jpu_buffer_t*)calloc(1, sizeof(jpu_buffer_t));
    if (pb == NULL) {
        JPU_ERROR("IO: calloc failed\n");
        return JPG_RET_FAILURE;
    }

    pb->flags = flags;
    pb->size  = pmd->size;
    pb->device_index = pmd->device_index;
    ret = jdi_allocate_dma_memory(pb);
    if (ret < 0) {
        JPU_ERROR("IO: jdi_allocate_dma_memory failed\n");
        free(pb);
        return JPG_RET_FAILURE;
    }

    pmd->base      = pb->base;
    pmd->phys_addr = pb->phys_addr;
    pmd->virt_addr = 0;

    pmd->pb        = (void*)pb;

    return JPG_RET_SUCCESS;
}

int IOFreePhyMem(jpu_mem_desc* pmd)
{
    jpu_buffer_t *pb = NULL;

    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    pb = (jpu_buffer_t*)(pmd->pb);
    if (pb) {
        jdi_free_dma_memory(pb);
        free(pb);
    }
    pmd->size         = 0;
    pmd->base         = 0;
    pmd->virt_addr    = 0;
    pmd->phys_addr    = 0;
    pmd->device_index = 0;
    pmd->pb           = NULL;

    return JPG_RET_SUCCESS;
}

int IOGetVirtMem(jpu_mem_desc* pmd, int map_flags)
{
    jpu_buffer_t *pb = NULL;
    int ret;

    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    pb = (jpu_buffer_t*)(pmd->pb);
    if (pb == NULL) {
        JPU_ERROR("IO: invalid pb\n");
        return JPG_RET_INVALID_PARAM;
    }

    if (pmd->size <= 0) {
        JPU_ERROR("IO: invalid size\n");
        return JPG_RET_INVALID_PARAM;
    }

    if (pmd->virt_addr != 0) {
        JPU_WARNING("IO: address was mapped\n");
        return JPG_RET_SUCCESS;
    }

    ret = jdi_map_dma_memory(pb, map_flags);
    if (ret < 0) {
        JPU_ERROR("IO: jdi_map_dma_memory failed!\n");
        pmd->virt_addr = 0;
        return JPG_RET_FAILURE;
    }

    pmd->virt_addr = pb->virt_addr;

    return JPG_RET_SUCCESS;
}

int IOFreeVirtMem(jpu_mem_desc* pmd)
{
    jpu_buffer_t *pb = NULL;
    int ret;

    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    pb = (jpu_buffer_t*)(pmd->pb);
    if (pb == NULL) {
        JPU_ERROR("IO: invalid pb\n");
        return JPG_RET_INVALID_PARAM;
    }

    if (pmd->size <= 0) {
        JPU_ERROR("IO: invalid size\n");
        return JPG_RET_INVALID_PARAM;
    }

    if (pmd->virt_addr == 0) {
        JPU_WARNING("IO: address was freed\n");
        return JPG_RET_SUCCESS;
    }

    ret = jdi_unmap_dma_memory(pb);
    if (ret < 0) {
        JPU_ERROR("IO: jdi_unmap_dma_memory failed!\n");
        return JPG_RET_FAILURE;
    }

    pmd->virt_addr = 0;

    return JPG_RET_SUCCESS;
}

int IOInvalidatePhyMem(jpu_mem_desc* pmd, unsigned long long phys_addr, unsigned int size)
{
    jpu_buffer_t *pb = NULL;
    int ret;

    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    pb = (jpu_buffer_t*)(pmd->pb);
    if (pb == NULL) {
        JPU_ERROR("IO: invalid pb\n");
        return JPG_RET_INVALID_PARAM;
    }

    /* invalidate only after map */
    if (pmd->virt_addr == 0) {
        JPU_WARNING("IO: address is not mapped\n");
        return JPG_RET_SUCCESS;
    }

    if (phys_addr < pmd->phys_addr ||
        (phys_addr+size) > (pmd->phys_addr+pmd->size)) {
        JPU_ERROR("IO: invalid phys_addr or size\n");
        return JPG_RET_INVALID_PARAM;
    }

    ret = jdi_invalidate_region(pb, phys_addr, size);

    return ret;
}

int IOFlushPhyMem(jpu_mem_desc* pmd, unsigned long long phys_addr, unsigned int size)
{
    jpu_buffer_t *pb = NULL;
    int ret;

    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    pb = (jpu_buffer_t*)(pmd->pb);
    if (pb == NULL) {
        JPU_ERROR("IO: invalid pb\n");
        return JPG_RET_INVALID_PARAM;
    }

    /* flush only after map */
    if (pmd->virt_addr == 0) {
        JPU_WARNING("IO: address is not mapped\n");
        return JPG_RET_SUCCESS;
    }

    if (phys_addr < pmd->phys_addr ||
        (phys_addr+size) > (pmd->phys_addr+pmd->size)) {
        JPU_ERROR("IO: invalid phys_addr or size\n");
        return JPG_RET_INVALID_PARAM;
    }

    ret = jdi_flush_region(pb, phys_addr, size);

    return ret;
}
#endif

#ifdef BM_PCIE_MODE
int IOPcieReadMem(jpu_mem_desc* pmd, unsigned long long src, uint8_t *dst, size_t size)
{
    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    if( size > pmd->size)
    {
        JPU_ERROR("pcie buffer size(%d) is less than output data size(%d)\n",
                     pmd->size, size);
        return -1;
    }
    return jdi_pcie_read_mem(pmd->device_index, src, dst, size);
}

int IOPcieWriteMem(jpu_mem_desc* pmd, uint8_t *src, unsigned long long dst, size_t size)
{
    if (pmd == NULL) {
        JPU_ERROR("IO: invalid handle\n");
        return JPG_RET_INVALID_PARAM;
    }

    if( size > pmd->size)
    {
        JPU_ERROR("pcie buffer size(%d) is less than output data size(%d)\n",
                     pmd->size, size);
        return -1;
    }

    return jdi_pcie_write_mem(pmd->device_index, src, dst, size);
}
#endif
