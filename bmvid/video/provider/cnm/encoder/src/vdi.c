//--=========================================================================--
//  This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2013  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================--


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef    _KERNEL_
#include <linux/delay.h>
#endif
#include <pthread.h>
#include <sys/mman.h>   /* mmap */
#include <sys/ioctl.h>  /* ioctl */
#include <sys/errno.h>  /* errno */
#include <sys/types.h>
#include <sys/file.h>   /* flock */
//#include <stdatomic.h>

#include "vpuopt.h"
#include "linux/vpu.h"
#include "vdi.h"
#include "wave5_regdefine.h"
#include "internal.h"

#if !defined(CHIP_BM1684)
#  error "Only BM1684 is supported for now!"
#endif

#ifdef BM_PCIE_MODE
#  define VPU_DEVICE_NAME "/dev/bm-sophon"
#else
#  define VPU_DEVICE_NAME "/dev/vpu"
#endif

#ifdef BM_PCIE_MODE
# define FAKE_PCIE_VIRT_ADDR 0xDEADBEEFl
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#endif

#define VPU_BIT_REG_SIZE    (0x10000*MAX_NUM_VPU_CORE)
/* If we can know the sram address in SOC directly for vdi layer.
 * it is possible to set in vdi layer without allocation from driver */
#define VDI_SRAM_BASE_ADDR           0x00000000
#define VDI_WAVE521C_SRAM_SIZE       0x2D000     /* 10bit profile : 8Kx8K -> 143360, 4Kx2K -> 71680
                                                  *  8bit profile : 8Kx8K -> 114688, 4Kx2K -> 57344
                                                  * NOTE: Decoder > Encoder */

#define VDI_128BIT_BUS_SYSTEM_ENDIAN     VDI_128BIT_LITTLE_ENDIAN
#define VDI_NUM_LOCK_HANDLES             2
static uint32_t videnc_chip_info[MAX_NUM_SOPHON_SOC] = { 0 };
typedef struct {
    vpudrv_buffer_t vdb;
    int inuse;
} vpudrv_buffer_pool_t;

typedef struct  {
    uint32_t             core_idx;
    uint32_t             product_code;

    int                  vpu_fd;
    int                  task_num;

    int                  clock_state;

    vpu_instance_pool_t* pvip;
    uint8_t*             inst_memory_va;
    int                  inst_memory_size;

    vpudrv_buffer_t      vdb_register;

    vpu_buffer_t         common_memory;

    vpudrv_buffer_pool_t buffer_pool[MAX_VPU_BUFFER_POOL];
    int                  buffer_pool_count;
    void*                buffer_pool_lock;

    pid_t                pid;
    int*                 mutex[VDI_NUM_LOCK_HANDLES];
    vpudrv_reset_flag    reset_core_flag;
} bm_vdi_info_t;

static bm_vdi_info_t* p_vdi_info[MAX_NUM_ENC_CORE] = { [0 ... (MAX_NUM_ENC_CORE-1)] = NULL };

int* p_vpu_enc_create_inst_flag[MAX_NUM_ENC_CORE];

typedef int MUTEX_HANDLE;

static int bm_vdi_allocate_common_memory(uint32_t core_idx);

static bm_vdi_info_t* bm_vdi_get_info_ptr(uint32_t core_idx);
static int            bm_vdi_free_info_ptr(uint32_t core_idx);
static bm_vdi_info_t* bm_vdi_check_info_ptr(uint32_t core_idx);

int bm_vdi_init(uint32_t core_idx)
{
    bm_vdi_info_t *vdi;
#if defined(BM_PCIE_MODE)
    int soc_idx       = core_idx/MAX_NUM_VPU_CORE_CHIP;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    int i, ret;

    vdi = bm_vdi_get_info_ptr(core_idx);
    if (vdi == NULL) {
        VLOG(ERR, "bm_vdi_get_info_ptr failed\n");
        return -1;
    }

    if (vdi->vpu_fd >= 0) {
        bm_vdi_lock(core_idx);
        vdi->task_num++;
        bm_vdi_unlock(core_idx);
        return 0;
    }

    /* if this API supports VPU parallel processing using multi VPU.
     * the driver should be made to open multiple times. */
    char vpu_dev_name[64] = {0};
#if defined(BM_PCIE_MODE)
    sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, soc_idx);
#else
    sprintf(vpu_dev_name,"%s", VPU_DEVICE_NAME);
#endif
    ret = access(vpu_dev_name, F_OK);
    if (ret == -1) {
        VLOG(ERR, "[VDI] access %s failed! [error=%s].\n",
             vpu_dev_name, strerror(errno));
        goto ERR_VDI_INIT0;
    }
    vdi->vpu_fd = open(vpu_dev_name, O_RDWR);

    if (vdi->vpu_fd < 0) {
#ifndef BM_PCIE_MODE
        VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to load ko again\n", strerror(errno));
#else
        VLOG(ERR, "[VDI] Can't open device driver. [error=%s]. try to load ko again\n", strerror(errno));
#endif
        goto ERR_VDI_INIT0;
    }

    VLOG(INFO,"[VDI] Open device %s, fd=%d\n", vpu_dev_name, vdi->vpu_fd);
    vdi->pid    = getpid();
    bm_vdi_disable_kernel_reset(core_idx);

    memset(&vdi->buffer_pool, 0, sizeof(vpudrv_buffer_pool_t)*MAX_VPU_BUFFER_POOL);

    if (!bm_vdi_get_instance_pool(core_idx)) {
        VLOG(ERR, "[VDI] fail to create shared info for saving context \n");
        goto ERR_VDI_INIT;
    }

    if (vdi->pvip->instance_pool_inited == FALSE) {
        for (i=0; i<VDI_NUM_LOCK_HANDLES; i++)
            *(vdi->mutex[i]) = 0;

        for( i = 0; i < MAX_NUM_INSTANCE; i++) {
            int* pCodecInst = (int *)vdi->pvip->codecInstPool[i];
            pCodecInst[1] = i;    // indicate instIndex of CodecInst
            pCodecInst[0] = 0;    // indicate inUse of CodecInst
        }

        vdi->pvip->instance_pool_inited = TRUE;
    }

    if (vdi->buffer_pool_lock == NULL) {
        vdi->buffer_pool_lock = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init((pthread_mutex_t *)vdi->buffer_pool_lock, NULL);
    }

#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
#ifndef BM_PCIE_MODE
    vdi->vdb_register.size = core_idx;
#else
    vdi->vdb_register.size = chip_core_idx;
#endif
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_GET_REGISTER_INFO, &vdi->vdb_register);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to get host interface register with ioctl() "
             "request. [error=%s]\n", strerror(errno));
        goto ERR_VDI_INIT;
    }
#endif

    VLOG(DEBUG, "[VDI] page size: %d\n", PAGE_SIZE);
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
    unsigned long offset       = vdi->vdb_register.phys_addr & (PAGE_SIZE-1); // TODO
    unsigned long aligned_pa   = vdi->vdb_register.phys_addr - offset;
    unsigned long aligned_size = vdi->vdb_register.size + (offset ? PAGE_SIZE : 0);
    VLOG(DEBUG, "[VDI] map vpu %d registers. fd=%d. pa=0x%lx, size=%d; aligned pa: 0x%lx, offset=%ld, aligned size=%ld\n",
         core_idx, vdi->vpu_fd, vdi->vdb_register.phys_addr, vdi->vdb_register.size, aligned_pa, offset, aligned_size);
    vdi->vdb_register.virt_addr = (unsigned long)mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                      vdi->vpu_fd, aligned_pa);
    if ((void *)vdi->vdb_register.virt_addr == MAP_FAILED) {
        VLOG(ERR, "[VDI] fail to map vpu registers. [error=%s].\n", strerror(errno));
        goto ERR_VDI_INIT;
    }
    vdi->vdb_register.virt_addr += offset;
#else
    vdi->vdb_register.size = VPU_BIT_REG_SIZE;
    vdi->vdb_register.virt_addr = (unsigned long)mmap(NULL, vdi->vdb_register.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                      vdi->vpu_fd, 0);
    if ((void *)vdi->vdb_register.virt_addr == MAP_FAILED) {
        VLOG(ERR, "[VDI] fail to map vpu registers. [error=%s].\n", strerror(errno));
        goto ERR_VDI_INIT;
    }
#endif
    VLOG(DEBUG, "[VDI] map vdb_register core_idx=%ld, pa=0x%lx, va=0x%lx, size=%d\n",
         core_idx, vdi->vdb_register.phys_addr, vdi->vdb_register.virt_addr, vdi->vdb_register.size);

    bm_vdi_set_clock_gate(core_idx, 1);

    vdi->product_code = VpuReadReg(core_idx, VPU_PRODUCT_CODE_REGISTER);

    if (VpuReadReg(core_idx, W5_VCPU_CUR_PC) == 0) { // if BIT processor is not running.
        for (i=0; i<64; i++)
            VpuWriteReg(core_idx, (i*4) + 0x100, 0x0);
    }

    ret = bm_vdi_lock(core_idx);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to handle lock function\n");
        goto ERR_VDI_INIT;
    }

    ret = bm_vdi_allocate_common_memory(core_idx);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to get vpu common buffer from driver\n");
        goto ERR_VDI_INIT;
    }

    vdi->core_idx = core_idx;
    vdi->task_num++;
    bm_vdi_unlock(core_idx);

    VLOG(INFO, "[VDI] success to init driver \n");

    return 0;

ERR_VDI_INIT:
    bm_vdi_unlock(core_idx);
    bm_vdi_release(core_idx);
ERR_VDI_INIT0:
    bm_vdi_free_info_ptr(core_idx);
    return -1;
}

// TODO
int bm_vdi_set_bit_firmware_to_pm(uint32_t core_idx, const uint16_t* code)
{
    vpu_bit_firmware_info_t bit_firmware_info;
    bm_vdi_info_t *vdi;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    int i, ret;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    bit_firmware_info.size = sizeof(vpu_bit_firmware_info_t);
#ifndef BM_PCIE_MODE
    bit_firmware_info.core_idx = core_idx;
#else
    bit_firmware_info.core_idx = chip_core_idx;
#endif
    bit_firmware_info.reg_base_offset = 0;

    for (i=0; i<512; i++)
        bit_firmware_info.bit_code[i] = code[i];

    ret = write(vdi->vpu_fd, &bit_firmware_info, bit_firmware_info.size);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to bm_vdi_set_bit_firmware core=%d\n", bit_firmware_info.core_idx);
        return -1;
    }

    return 0;
}

int bm_vdi_release(uint32_t core_idx)
{
    vpudrv_buffer_t vdb;
    bm_vdi_info_t *vdi;
    int i, ret;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    ret = bm_vdi_lock(core_idx);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to handle lock function\n");
        return -1;
    }

    if (vdi->task_num > 1) { // means that the opened instance remains
        vdi->task_num--;
        bm_vdi_unlock(core_idx);

        return 0;
    }

    if (vdi->vdb_register.virt_addr)
        munmap((void *)vdi->vdb_register.virt_addr, vdi->vdb_register.size);
    memset(&vdi->vdb_register, 0, sizeof(vpudrv_buffer_t));

    vdb.size = 0;

    /* get common memory information to free virtual address */
    /* TODO */
    pthread_mutex_lock((pthread_mutex_t *)vdi->buffer_pool_lock);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++) {
        if (vdi->common_memory.phys_addr >= vdi->buffer_pool[i].vdb.phys_addr &&
            vdi->common_memory.phys_addr < (vdi->buffer_pool[i].vdb.phys_addr +
                                            vdi->buffer_pool[i].vdb.size)) {
            vdi->buffer_pool[i].inuse = 0;
            vdi->buffer_pool_count--;
            vdb = vdi->buffer_pool[i].vdb;
            break;
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)vdi->buffer_pool_lock);

    vdi->task_num--;
    bm_vdi_get_kernel_reset(core_idx);
    if(vdi->reset_core_flag.reset == 1)
        bm_vdi_resume_kernel_reset(core_idx);
    bm_vdi_unlock(core_idx);

    if (vdb.size > 0) {
        munmap((void *)vdb.virt_addr, vdb.size);
        memset(&vdi->common_memory, 0, sizeof(vpu_buffer_t));
    }

    if (vdi->inst_memory_va) {
        munmap((void *)vdi->inst_memory_va, vdi->inst_memory_size);
    }


    if (vdi->vpu_fd >= 0)
        close(vdi->vpu_fd);

    if (vdi->buffer_pool_lock != NULL) {
        pthread_mutex_destroy((pthread_mutex_t *)vdi->buffer_pool_lock);
        free(vdi->buffer_pool_lock);
        vdi->buffer_pool_lock = NULL;
    }

    bm_vdi_free_info_ptr(core_idx);

    return 0;
}

int bm_vdi_get_common_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
    bm_vdi_info_t* vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if (vb == NULL)
        return -1;

    memcpy(vb, &vdi->common_memory, sizeof(vpu_buffer_t));

    return 0;
}

static int bm_vdi_allocate_common_memory(uint32_t core_idx)
{
    bm_vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
    int i, ret;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    memset(&vdb, 0, sizeof(vpudrv_buffer_t));

    vdb.size = SIZE_COMMON;
    vdb.core_idx = chip_core_idx;

    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_GET_COMMON_MEMORY, &vdb);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to get common memory (size=%d) with ioctl() "
             "request. [error=%s]\n", vdb.size, strerror(errno));
        return -1;
    }

#ifndef BM_PCIE_MODE
    vdb.virt_addr = (unsigned long)mmap(NULL, vdb.size, PROT_READ | PROT_WRITE, MAP_SHARED, vdi->vpu_fd, vdb.phys_addr);
    if ((void *)vdb.virt_addr == MAP_FAILED) {
        VLOG(ERR, "[VDI] fail to map common memory. phyaddr=0x%lx, size=%d. [error=%s].\n",
             vdb.phys_addr, vdb.size, strerror(errno));
        return -1;
    }
#else
    vdb.virt_addr = FAKE_PCIE_VIRT_ADDR;
#endif

    VLOG(DEBUG, "[VDI] physaddr=0x%lx, virtaddr=0x%lx\n", vdb.phys_addr, vdb.virt_addr);

    vpu_buffer_t common_buffer = {0};

    common_buffer.size      = SIZE_COMMON;
    common_buffer.phys_addr = (unsigned long)(vdb.phys_addr);
    common_buffer.base      = (unsigned long)(vdb.base);
    common_buffer.virt_addr = (unsigned long)(vdb.virt_addr);

    memcpy(&vdi->common_memory, &common_buffer, sizeof(vpu_buffer_t));

    pthread_mutex_lock((pthread_mutex_t *)vdi->buffer_pool_lock);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++) {
        if (vdi->buffer_pool[i].inuse == 0) {
            vdi->buffer_pool[i].vdb = vdb;
            vdi->buffer_pool_count++;
            vdi->buffer_pool[i].inuse = 1;
            break;
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)vdi->buffer_pool_lock);

    VLOG(DEBUG, "[VDI] physaddr=0x%lx, size=%d, virtaddr=0x%lx\n",
         vdi->common_memory.phys_addr, vdi->common_memory.size, vdi->common_memory.virt_addr);

    return 0;
}

int bm_vdi_allocate_dma_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
    bm_vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);

    if(vdi == NULL || vdi->vpu_fd < 0)
        return -1;

    memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

#if defined(BM_PCIE_MODE)
    vdb.core_idx = chip_core_idx;
#endif

    vdb.size = vb->size;
#if !defined(BM_PCIE_MODE)
    vdb.enable_cache = vb->enable_cache;
#endif
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY, &vdb) < 0) {
        VLOG(ERR, "[VDI] fail to vdi_allocate_dma_memory size=%d\n", vdb.size);
        return -1;
    }

#ifndef BM_PCIE_MODE
    /* map to virtual address */
    // vdb.virt_addr = (unsigned long)mmap(NULL, vdb.size, PROT_READ | PROT_WRITE,
    //                                     MAP_SHARED, vdi->vpu_fd, vdb.phys_addr);
    // if ((void *)vdb.virt_addr == MAP_FAILED) {
    //     memset(vb, 0x00, sizeof(vpu_buffer_t));
    //     return -1;
    // }

    // memset(vb->virt_addr, 0x00, sizeof(vb->size));
    /* unmap */
    // if (munmap((void *)vdb.virt_addr, vdb.size) < 0) {
    //     VLOG(ERR, "[VDI] munmap failed, vaddr=0x%lx\n", vdb.virt_addr);
    //     return -1;
    // }
    // vdb.virt_addr = 0;

    /* flush operation */
    // if (vdb.enable_cache) {
    //     if (ioctl(vdi->vpu_fd, VDI_IOCTL_FLUSH_DCACHE, &vdb) < 0) {
    //         VLOG(ERR, "[VDI] fail to flush ioctl\n");
    //         return -1;
    //     }
    // }
#else
    vdb.virt_addr = FAKE_PCIE_VIRT_ADDR;
    // vdi_clear_memory(core_idx, vdb.phys_addr, vdb.size, VDI_SYSTEM_ENDIAN); //if need clear, todo
#endif

    vb->base      = vdb.base;
    vb->phys_addr = vdb.phys_addr;
    vb->virt_addr = vdb.virt_addr;

    pthread_mutex_lock((pthread_mutex_t *)vdi->buffer_pool_lock);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++) {
        if (vdi->buffer_pool[i].inuse == 0) {
            vdi->buffer_pool[i].vdb = vdb;
            vdi->buffer_pool_count++;
            vdi->buffer_pool[i].inuse = 1;
            break;
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)vdi->buffer_pool_lock);

    VLOG(DEBUG, "[VDI] vdi_allocate_dma_memory, physaddr=0x%lx, virtaddr=0x%lx~0x%lx, size=%d\n",
         vb->phys_addr, vb->virt_addr, vb->virt_addr + vb->size, vb->size);

    return 0;
}

int bm_vdi_free_dma_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
    bm_vdi_info_t *vdi;
    int i, ret;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if (vb->size == 0)
        return 0;

    memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    pthread_mutex_lock((pthread_mutex_t *)vdi->buffer_pool_lock);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->buffer_pool[i].inuse = 0;
            vdi->buffer_pool_count--;
            vdb = vdi->buffer_pool[i].vdb;
            break;
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)vdi->buffer_pool_lock);

    if (!vdb.size) {
        VLOG(ERR, "[VDI] invalid buffer to free address = 0x%lx\n", vdb.virt_addr);
        return -1;
    }

    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_FREE_PHYSICALMEMORY, &vdb);
    if (ret < 0) {
        VLOG(ERR, "[VDI] ioctl free_physical_memory failed!\n", vdb.virt_addr);
        if (vdb.virt_addr) {
#ifndef BM_PCIE_MODE
            munmap((void *)vdb.virt_addr, vdb.size);
#endif
            vdb.virt_addr = 0;
        }
        return -1;
    }

    if (vdb.virt_addr) {
#ifndef BM_PCIE_MODE
        munmap((void *)vdb.virt_addr, vdb.size);
#endif
        vdb.virt_addr = 0;
    }

    VLOG(DEBUG, "[VDI] vdi_free_dma_memory, physaddr=0x%lx, virtaddr=0x%lx~0x%lx, size=%d\n",
         vdb.phys_addr, vdb.virt_addr, vdb.virt_addr + vdb.size, vdb.size);

    memset(vb, 0, sizeof(vpu_buffer_t));

    return 0;
}

int bm_vdi_mmap_dma_memory(uint32_t core_idx, vpu_buffer_t *vb, int enable_read, int enable_write)
{
#ifdef BM_PCIE_MODE
    return 0;
#else
    bm_vdi_info_t *vdi;
    int port_flag = 0;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if (vb->size == 0)
        return -1;

    if (enable_read)
        port_flag |= PROT_READ;
    if (enable_write)
        port_flag |= PROT_WRITE;

    //defaule enable_cache
    vb->virt_addr = (unsigned long)mmap(NULL, vb->size, port_flag, MAP_SHARED, vdi->vpu_fd, vb->phys_addr);

    if ((void *)vb->virt_addr == MAP_FAILED) {
        VLOG(ERR, "[VDI] mmap failed, addr=0x%lx size=%d\n", vb->phys_addr, vb->size);
        vb->virt_addr = 0;
        return -1;
    }

    return 0;
#endif
}

int bm_vdi_unmap_dma_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
#ifdef BM_PCIE_MODE
    return 0;
#else
    bm_vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if (vb->size == 0)
        return -1;

    if (vb->virt_addr) {
        if (munmap((void *)vb->virt_addr, vb->size) < 0) {
            vb->virt_addr = 0;
            return -1;
        }
    }
    vb->virt_addr = 0;
    return 0;
#endif
}

int bm_vdi_flush_dma_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
#if !defined(BM_PCIE_MODE) && !defined(BM_ION_MEM)
    vpudrv_buffer_t vdb = {0};
#endif
    bm_vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if(vdi == NULL || vdi->vpu_fd < 0)
        return -1;

#if defined(BM_ION_MEM)
    if (!vb->size) {
        VLOG(ERR, "address 0x%08x is not mapped address!!!\n", (int)vb->phys_addr);
    }
    else
    {
        if(vb->enable_cache == 1) {
            if(msync((void *)vb->virt_addr, vb->size, MS_ASYNC) == -1) {
                VLOG(ERR, "[VDI] fail to flush memory. addr=0x%lx size=%d\n", vb->virt_addr, vb->size);
            }
        }
    }
#elif !defined(BM_PCIE_MODE)
    vdb.phys_addr = vb->phys_addr;
    vdb.size = vb->size;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_FLUSH_DCACHE, &vdb) < 0)
    {
        VLOG(ERR, "[VDI] fail to flush dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
    }
#endif
    return 0;
}

int bm_vdi_invalidate_dma_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
#if !defined(BM_PCIE_MODE) && !defined(BM_ION_MEM)
    vpudrv_buffer_t vdb = {0};
#endif
    bm_vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if(vdi == NULL || vdi->vpu_fd < 0)
        return -1;

#if defined(BM_ION_MEM)
    if (!vb->size) {
        VLOG(ERR, "address 0x%08x is not mapped address!!!\n", (int)vb->phys_addr);
    }
    else
    {
        if(vb->enable_cache == 1) {
            if(msync((void *)vb->virt_addr, vb->size, MS_INVALIDATE) == -1) {
                VLOG(ERR, "[VDI] fail to invalidate memory. addr=0x%lx size=%d\n", vb->virt_addr, vb->size);
            }
        }
    }
#elif !defined(BM_PCIE_MODE)
    vdb.phys_addr = vb->phys_addr;
    vdb.size = vb->size;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_INVALIDATE_DCACHE, &vdb) < 0)
    {
        VLOG(ERR, "[VDI] fail to fluch invalidate mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
    }
#endif
    return 0;
}

int bm_vdi_attach_dma_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
    vpudrv_buffer_t vdb;
    bm_vdi_info_t *vdi;
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if (vb->size == 0)
        return -1;

    memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vdb.size = vb->size;
    vdb.phys_addr = vb->phys_addr;

    pthread_mutex_lock((pthread_mutex_t *)vdi->buffer_pool_lock);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->buffer_pool[i].vdb = vdb;
            vdi->buffer_pool[i].inuse = 1;
            break;
        }
        else
        {
            if (vdi->buffer_pool[i].inuse == 0)
            {
                vdi->buffer_pool[i].vdb = vdb;
                vdi->buffer_pool[i].inuse = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)vdi->buffer_pool_lock);
    return 0;
}

int bm_vdi_deattach_dma_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
    bm_vdi_info_t *vdi;
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if(!vb || !vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    pthread_mutex_lock((pthread_mutex_t *)vdi->buffer_pool_lock);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->buffer_pool[i].vdb.phys_addr = 0;
            vdi->buffer_pool[i].vdb.size      = 0;
            vdi->buffer_pool[i].inuse         = 0;
            break;
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)vdi->buffer_pool_lock);
    return 0;
}

vpu_instance_pool_t* bm_vdi_get_instance_pool(uint32_t core_idx)
{
    bm_vdi_info_t *vdi;
    int i, ret;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return NULL;
    }

    if (!vdi->pvip) {
#if defined(BM_PCIE_MODE)
        int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
        int chip_core_idx = core_idx;
#endif
        vpudrv_buffer_t vdb;
        int size = sizeof(vpu_instance_pool_t) + sizeof(MUTEX_HANDLE)*(VDI_NUM_LOCK_HANDLES + 2); // TODO

        memset(&vdb, 0, sizeof(vpudrv_buffer_t));

        vdb.core_idx = chip_core_idx;

        vdb.size = size;

        ret = ioctl(vdi->vpu_fd, VDI_IOCTL_GET_INSTANCE_POOL, &vdb);
        if (ret < 0) {
            VLOG(ERR, "[VDI] fail to get instance pool (size=%d) with ioctl() "
                 "request. [error=%s]\n", vdb.size, strerror(errno));
            return NULL;
        }

        off_t offset = vdb.phys_addr;

        // TODO
        vdb.virt_addr = (unsigned long)mmap(NULL, vdb.size, PROT_READ | PROT_WRITE, MAP_SHARED, vdi->vpu_fd, offset);
        if ((void *)vdb.virt_addr == MAP_FAILED) {
            VLOG(ERR, "[VDI] fail to map instance pool. phyaddr=0x%lx, size=%d. [error=%s].\n",
                 vdb.phys_addr, vdb.size, strerror(errno));
            return NULL;
        }

        vdi->inst_memory_va   = (uint8_t*)vdb.virt_addr;
        vdi->inst_memory_size = vdb.size;

        vdi->pvip = (vpu_instance_pool_t*)(vdb.virt_addr);

        unsigned long base = (unsigned long)(vdi->pvip) + sizeof(vpu_instance_pool_t);
        for (i=0; i<VDI_NUM_LOCK_HANDLES; i++)
            vdi->mutex[i] = (int*)(base + sizeof(MUTEX_HANDLE)*i);

#if defined(BM_PCIE_MODE)
        int enc_core_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
#else
        int enc_core_idx = 0;
#endif
        p_vpu_enc_create_inst_flag[enc_core_idx] = (int*)(base + sizeof(MUTEX_HANDLE)*VDI_NUM_LOCK_HANDLES);

        vdi->pid   = getpid();

        VLOG(DEBUG, "[VDI] instance pool physaddr=0x%lx, virtaddr=0x%lx, base=0x%lx, size=%u, pvip=%p\n",
             vdb.phys_addr, vdb.virt_addr, vdb.base, vdb.size, vdi->pvip);
    }

    return (vpu_instance_pool_t *)vdi->pvip;
}

int bm_vdi_open_instance(uint32_t core_idx, uint32_t inst_idx)
{
    bm_vdi_info_t *vdi;
    vpudrv_inst_info_t inst_info = {0};
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif
    int ret;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    inst_info.core_idx = chip_core_idx;
    inst_info.inst_idx = inst_idx;

    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_OPEN_INSTANCE, &inst_info);
    if (ret < 0) {
#if defined(BM_PCIE_MODE)
        int soc_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
        VLOG(ERR, "[VDI] fail to open instance #%d at SoC %d, core %d with ioctl() request. "
             "[error=%s]\n", inst_idx, soc_idx, chip_core_idx, strerror(errno));
#else
        VLOG(ERR, "[VDI] fail to open instance #%d at core %d with ioctl() request. "
             "[error=%s]\n", inst_idx, chip_core_idx, strerror(errno));
#endif
        return -1;
    }

    vdi->pvip->vpu_instance_num = inst_info.inst_open_count;

    return 0;
}

int bm_vdi_close_instance(uint32_t core_idx, uint32_t inst_idx)
{
    bm_vdi_info_t *vdi;
    vpudrv_inst_info_t inst_info = {0};
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif
    int ret;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    inst_info.core_idx = chip_core_idx;
    inst_info.inst_idx = inst_idx;
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_CLOSE_INSTANCE, &inst_info);
    if (ret < 0) {
#if defined(BM_PCIE_MODE)
        int soc_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
        VLOG(ERR, "[VDI] fail to close instance #%d at SoC %d, core %d with ioctl() request. "
             "[error=%s]\n", inst_idx, soc_idx, chip_core_idx, strerror(errno));
#else
        VLOG(ERR, "[VDI] fail to close instance #%d at core %d with ioctl() request. "
             "[error=%s]\n", inst_idx, chip_core_idx, strerror(errno));
#endif
        return -1;
    }

    vdi->pvip->vpu_instance_num = inst_info.inst_open_count;

    return 0;
}

/* bm_vdi_get_instance_num() is called after bm_vdi_init() */
int bm_vdi_get_instance_num(uint32_t core_idx)
{
    bm_vdi_info_t *vdi = bm_vdi_check_info_ptr(core_idx);

    if (!vdi) {
        return -1;
    }
    if (vdi->vpu_fd < 0) {
        return -1;
    }
    if (vdi->pvip == NULL) {
        return -1;
    }

    return vdi->pvip->vpu_instance_num;
}

static inline int bm_vdi_lock_internal(uint32_t core_idx, int no, int us_delay)
{
#if defined(BM_PCIE_MODE)
    int enc_core_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
#else
    int enc_core_idx = 0;
#endif
    bm_vdi_info_t* vdi = p_vdi_info[enc_core_idx];
    struct timespec ts = {0, us_delay*1000};
    int new_value;

    if (vdi == NULL) {
        VLOG(ERR, "vdi is NULL. lock %d is invalid.\n", no);
        return 0;
    }
    if (vdi->mutex[no] == NULL) {
        VLOG(ERR, "vdi->mutex[%d] is NULL. lock %d is invalid.\n", no, no);
        return 0;
    }

    new_value = vdi->pid;
    while (__sync_val_compare_and_swap(vdi->mutex[no], 0, new_value) != 0) {
        nanosleep(&ts, NULL);
        if(ts.tv_nsec > 40*1000)
            ts.tv_nsec = ts.tv_nsec/1.1;
    }

    return 0;
}
static int bm_vdi_unlock_internal(uint32_t core_idx, int no)
{
#if defined(BM_PCIE_MODE)
    int enc_core_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
#else
    int enc_core_idx = 0;
#endif
    bm_vdi_info_t* vdi = p_vdi_info[enc_core_idx];

    if (vdi == NULL) {
        VLOG(ERR, "vdi is NULL. lock %d is invalid.\n", no);
        return 0;
    }
    if (vdi->mutex[no] == NULL) {
        VLOG(ERR, "vdi->mutex[%d] is NULL. lock %d is invalid.\n", no, no);
        return 0;
    }

    __sync_lock_release(vdi->mutex[no]);

    return 0;
}

int bm_vdi_resume_kernel_reset(uint32_t core_idx){
    int ret = 0;
    int chip_core_idx = core_idx;
    bm_vdi_info_t* vdi = bm_vdi_get_info_ptr(core_idx);
    if (!vdi) {
        VLOG(ERR, "vdi is null, encoder fail to resume vpu_reset in kernel\n");
        return -1;
    }
    if (vdi->vpu_fd < 0) {
        VLOG(ERR, "fd is invalid, encoder fail to resume vpu_reset in kernel\n");
        return -1;
    }

#if defined(BM_PCIE_MODE)
    chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    if(vdi->reset_core_flag.pid == vdi->pid) {
        vdi->reset_core_flag.reset = 0;
        vdi->reset_core_flag.core_idx = chip_core_idx;
        ret = ioctl(vdi->vpu_fd, VDI_IOCTL_CTRL_KERNEL_RESET, &(vdi->reset_core_flag));
        if (ret < 0) {
            VLOG(ERR, "[VDI] encoder fail to resume kernel_reset ability with ioctl()\n");
            return -1;
        }
    }
    return 0;
}

int bm_vdi_disable_kernel_reset(uint32_t core_idx){
    int ret = 0;
    int chip_core_idx = core_idx;
    bm_vdi_info_t* vdi = bm_vdi_get_info_ptr(core_idx);
    if (!vdi) {
        VLOG(ERR, "vdi is null, encoder fail to disable vpu_reset in kernel\n");
        return -1;
    }
    if (vdi->vpu_fd < 0) {
        VLOG(ERR, "fd is invalid, encoder fail to disable vpu_reset in kernel\n");
        return -1;
    }

#if defined(BM_PCIE_MODE)
    chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    vdi->reset_core_flag.pid = vdi->pid;
    vdi->reset_core_flag.reset = 1;
    vdi->reset_core_flag.core_idx = chip_core_idx;
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_CTRL_KERNEL_RESET, &(vdi->reset_core_flag));
    if (ret < 0) {
        VLOG(ERR, "[VDI] encoder fail to disable kernel_reset with ioctl()\n");
        return -1;
    }
    return 0;
}
// get the kernel reset status
int bm_vdi_get_kernel_reset(uint32_t core_idx){
    int ret = 0;
    int chip_core_idx = core_idx;
    bm_vdi_info_t* vdi = bm_vdi_get_info_ptr(core_idx);
    if (!vdi) {
        VLOG(ERR, "vdi is null, encoder fail to get vpu_reset status in kernel\n");
        return -1;
    }
    if (vdi->vpu_fd < 0) {
        VLOG(ERR, "fd is invalid, encoder fail  to get vpu_reset status in kernel\n");
        return -1;
    }

#if defined(BM_PCIE_MODE)
    chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    vdi->reset_core_flag.core_idx = chip_core_idx;
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_GET_KERNEL_RESET_STATUS, &(vdi->reset_core_flag));
    if (ret < 0) {
        VLOG(ERR, "[VDI] encoder fail to get vpu_reset status with ioctl()\n");
        return -1;
    }
    return 0;
}

int bm_vdi_lock(uint32_t core_idx)
{
    return bm_vdi_lock_internal(core_idx, 0, 500);
}
int bm_vdi_unlock(uint32_t core_idx)
{
    return bm_vdi_unlock_internal(core_idx, 0);
}

int bm_vdi_lock2(uint32_t core_idx)
{
    return bm_vdi_lock_internal(core_idx, 1, 5*1000);
}
int bm_vdi_unlock2(uint32_t core_idx)
{
    return bm_vdi_unlock_internal(core_idx, 1);
}

void VpuWriteReg(uint32_t core_idx, uint64_t addr, uint32_t data)
{
    bm_vdi_info_t *vdi;
    unsigned long *reg_addr;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return;
    }

    reg_addr = (unsigned long *)(addr + (unsigned long)vdi->vdb_register.virt_addr);
    *(volatile uint32_t *)reg_addr = data;
}

uint32_t VpuReadReg(uint32_t core_idx, uint64_t addr)
{
    bm_vdi_info_t *vdi;
    unsigned long *reg_addr;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return (uint32_t)-1;
    }

    reg_addr = (unsigned long *)(addr + (unsigned long)vdi->vdb_register.virt_addr);
    return *(volatile uint32_t *)reg_addr;
}

#define FIO_TIMEOUT         100

uint32_t bm_vdi_fio_read_register(uint32_t core_idx, uint64_t addr)
{
    uint32_t ctrl;
    uint32_t count = 0;
    uint32_t data  = 0xffffffff;

    ctrl  = (addr&0xffff);
    ctrl |= (0<<16);    /* read operation */
    VpuWriteReg(core_idx, W5_VPU_FIO_CTRL_ADDR, ctrl);
    count = FIO_TIMEOUT;
    while (count--) {
        ctrl = VpuReadReg(core_idx, W5_VPU_FIO_CTRL_ADDR);
        if (ctrl & 0x80000000) {
            data = VpuReadReg(core_idx, W5_VPU_FIO_DATA);
            break;
        }
    }

    return data;
}

void bm_vdi_fio_write_register(uint32_t core_idx, uint64_t addr, uint32_t data)
{
    uint32_t ctrl;

    VpuWriteReg(core_idx, W5_VPU_FIO_DATA, data);
    ctrl  = (addr&0xffff);
    ctrl |= (1<<16);    /* write operation */
    VpuWriteReg(core_idx, W5_VPU_FIO_CTRL_ADDR, ctrl);
}


int bm_vdi_write_memory(uint32_t core_idx, uint64_t dst_addr, uint8_t *src_data, int len)
{
    bm_vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
#ifndef BM_PCIE_MODE
    uint64_t offset;
    int mmap_flag;
#endif
    int i;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if (!src_data)
        return -1;

    memset(&vdb, 0, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++) {
        if (vdi->buffer_pool[i].inuse == 1) {
            vdb = vdi->buffer_pool[i].vdb;
            if (dst_addr >= vdb.phys_addr && (dst_addr+len) <= (vdb.phys_addr + vdb.size)) {
                break;
            }
        }
    }

    if (!vdb.size) {
        VLOG(ERR, "address 0x%lx is not mapped address!!!\n", dst_addr);
        return -1;
    }

#ifndef BM_PCIE_MODE
    offset = dst_addr - (uint64_t)vdb.phys_addr;
    vpu_buffer_t vb;
    vb.phys_addr    = vdb.phys_addr;
    vb.size         = vdb.size;
    vb.enable_cache = vdb.enable_cache;
    if (!vdb.virt_addr) {
        bm_vdi_mmap_dma_memory(core_idx, &vb, 0, 1);
        vdb.virt_addr = vb.virt_addr;
        mmap_flag = 1;
    } else {
        mmap_flag = 0;
    }

    memcpy((void *)((uint64_t)vdb.virt_addr+offset), src_data, len);

    /* flush operation */
    if (vdb.enable_cache) {
        int ret = bm_vdi_flush_dma_memory(core_idx, &vb);
        if (ret < 0) {
            VLOG(ERR, "[VDI] fail to flush cache\n");
            return -1;
        }
    }

    if (mmap_flag) {
        bm_vdi_unmap_dma_memory(core_idx, &vb);
        vdb.virt_addr = 0;
    }
#else
    vpu_video_mem_op_t vmem_op;
    vmem_op.dst = dst_addr;
    vmem_op.src = (uint64_t)src_data;
    vmem_op.size =  len;
    int ret = ioctl(vdi->vpu_fd, VDI_IOCTL_WRITE_VMEM, &vmem_op);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to write memory (pa=0x%lx, size=%d) with ioctl() "
             "request. [error=%s]\n", vmem_op.dst, vmem_op.size, strerror(errno));
        return -1;
    }
#endif

    return len;
}

int bm_vdi_read_memory(uint32_t core_idx, uint64_t src_addr, uint8_t *dst_data, int len)
{
    bm_vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
    int i;
#ifndef BM_PCIE_MODE
    uint64_t offset;
    int mmap_flag;
#endif

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    memset(&vdb, 0, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++) {
        if (vdi->buffer_pool[i].inuse == 1) {
            vdb = vdi->buffer_pool[i].vdb;
            if (src_addr >= vdb.phys_addr && (src_addr+len) <= (vdb.phys_addr + vdb.size))
                break;
        }
    }

    if (!vdb.size)
        return -1;
#ifdef BM_PCIE_MODE
    vpu_video_mem_op_t vmem_op;
    vmem_op.src = src_addr;
    vmem_op.dst = (uint64_t)dst_data;
    vmem_op.size =  len;
    int ret = ioctl(vdi->vpu_fd, VDI_IOCTL_READ_VMEM, &vmem_op);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to read memory (pa=0x%lx, size=%d) with ioctl() "
             "request. [error=%s]\n", vmem_op.src, vmem_op.size, strerror(errno));
        return -1;
    }

#else
    offset = src_addr - (uint64_t)vdb.phys_addr;

    vpu_buffer_t vb;
    vb.phys_addr = vdb.phys_addr;
    vb.size      = vdb.size;
    if (!vdb.virt_addr) {
        bm_vdi_mmap_dma_memory(core_idx, &vb, 1, 0);
        vdb.virt_addr = vb.virt_addr;
        mmap_flag = 1;
    } else {
        mmap_flag = 0;
    }

    /* invalidate operation */
    if (vdb.enable_cache) {
        int ret = bm_vdi_invalidate_dma_memory(core_idx, &vb);
        if (ret < 0) {
            VLOG(ERR, "[VDI] fail to invalidate cache\n");
            return -1;
        }
    }

    memcpy((void*)dst_data, (void *)((uint64_t)vdb.virt_addr+offset), len);

    if (mmap_flag) {
        bm_vdi_unmap_dma_memory(core_idx, &vb);
        vdb.virt_addr = 0;
    }
#endif
    return len;
}

int bm_vdi_get_sram_memory(uint32_t core_idx, vpu_buffer_t *vb)
{
    bm_vdi_info_t *vdi;
    unsigned int sram_size = 0;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if (!vb)
        return -1;

    if (vdi->product_code != WAVE521C_CODE) {
        VLOG(ERR, "[VDI] check product_code(%x)\n", vdi->product_code);
        return -1;
    }

    sram_size = VDI_WAVE521C_SRAM_SIZE;

    /* if we can know the sram address directly in vdi layer, we use it first for sdram address */
    if (sram_size > 0) {
        /* HOST can set DRAM base addr to VDI_SRAM_BASE_ADDR. */
#ifndef BM_PCIE_MODE
        vb->phys_addr = VDI_SRAM_BASE_ADDR+((core_idx%2)*sram_size);
#else
        vb->phys_addr = VDI_SRAM_BASE_ADDR+(chip_core_idx*sram_size);
#endif
        vb->size = sram_size;

        return 0;
    }

    return 0;
}

int bm_vdi_set_clock_gate(uint32_t core_idx, int enable)
{
    bm_vdi_info_t *vdi = NULL;
    int ret = 0;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

#if 0
    vdi->clock_state = enable;
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_SET_CLOCK_GATE, &enable);
#endif

    return ret;
}

int bm_vdi_get_clock_gate(uint32_t core_idx)
{
    bm_vdi_info_t *vdi;
    int ret;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    ret = vdi->clock_state;
    return ret;
}

#ifdef SUPPORT_MULTI_INST_INTR
int bm_vdi_wait_interrupt(uint32_t core_idx, unsigned int instIdx, int timeout)
#else
int bm_vdi_wait_interrupt(uint32_t core_idx, int timeout)
#endif
{
    bm_vdi_info_t *vdi;
    vpudrv_intr_info_t intr_info = {0};
    int intr_reason = 0;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif
    int ret;

    /* soc mode or bm1684 pcie mode */
    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    intr_info.timeout     = timeout;
    intr_info.intr_reason = 0;
#ifdef SUPPORT_MULTI_INST_INTR
    intr_info.intr_inst_index = instIdx;
#endif
    intr_info.core_idx = chip_core_idx;

    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_WAIT_INTERRUPT, (void*)&intr_info);
    if (ret < 0) {
#if defined(BM_PCIE_MODE)
        int soc_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
        VLOG(ERR, "[VDI] fail to wait for interrupt at SoC %d, core %d with ioctl() "
             "request. [error=%s]\n", soc_idx, chip_core_idx, strerror(errno));
#else
        VLOG(ERR, "[VDI] fail to wait for interrupt at core %d with ioctl() "
             "request. [error=%s]\n", chip_core_idx, strerror(errno));
#endif
        return -1;
    }
    intr_reason = intr_info.intr_reason;

    return intr_reason;
}

int bm_vdi_convert_endian(unsigned int endian)
{
    switch (endian) {
    case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
    case VDI_BIG_ENDIAN:          endian = 0x0f; break;
    case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
    case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
    }

    return (endian&0x0f);
}

unsigned int bm_vdi_get_ddr_map(uint32_t core_idx)
{
    bm_vdi_info_t *vdi;
    unsigned int ret;
    vpudrv_regrw_info_t reg_info;
    unsigned int bit_shift = DDR_MAP_BIT_SHIFT;

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    if(core_idx == 4)
        bit_shift = 24;
    else if((core_idx == 2) || (core_idx == 3))
        bit_shift = DDR_MAP_BIT_SHIFT_1;
    else
        bit_shift = DDR_MAP_BIT_SHIFT;

    reg_info.size = 4;
    reg_info.mask[0] = 0xFF << bit_shift;
    reg_info.reg_base = DDR_MAP_REGISTER;
    if (read(vdi->vpu_fd, &reg_info, sizeof(reg_info)) < 0)
    {
        VLOG(ERR, "[VDI] fail to get ddr map value core=%ld\n", core_idx);
        return -1;
    }
    ret = reg_info.value[0] >> bit_shift;
#ifndef VIDEO_MMU
    if (ret == 0) ret = 1;      // protection for backward compatible
#endif

    //change to map to upper 4G of the 8G memory in pcie
#ifdef BM_PCIE_MODE
    ret = 4;
#endif

    return ret;
}

int bm_vdi_get_firmware_status(unsigned int core_idx)
{
    bm_vdi_info_t *vdi;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif
    int ret = -1;

    VLOG(INFO, "get firmware status....\n");

    vdi = bm_vdi_check_info_ptr(core_idx);
    if (vdi == NULL || vdi->vpu_fd < 0) {
        VLOG(ERR, "bm_vdi_check_info_ptr failed. Please call bm_vdi_init first.\n");
        return -1;
    }

    VLOG(INFO, "check firmware status from driver....\n");
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_GET_FIRMWARE_STATUS, &chip_core_idx);
    if (ret < 0) {
        VLOG(ERR, "[VDI] fail to get firmware status of core %d with ioctl() "
             "request. [error=%s]\n", chip_core_idx, strerror(errno));
        return -1;
    }

    return (ret == 100) ? 0: 1;
}

static bm_vdi_info_t* bm_vdi_get_info_ptr(uint32_t core_idx)
{
    bm_vdi_info_t* p = NULL;
#if defined(BM_PCIE_MODE)
    int enc_core_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
#else
    int enc_core_idx = 0;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE) {
        VLOG(ERR, "The core index %d exceeds the max value(%d)\n", MAX_NUM_VPU_CORE);
        return NULL;
    }

    if (enc_core_idx >= MAX_NUM_ENC_CORE) {
        VLOG(ERR, "The encoder core index %d exceeds the max value(%d)\n", MAX_NUM_ENC_CORE);
        return NULL;
    }

    if (p_vdi_info[enc_core_idx])
        return p_vdi_info[enc_core_idx];

    p = malloc(sizeof(bm_vdi_info_t));
    if (p == NULL) {
        VLOG(ERR, "malloc failed!\n");
        return NULL;
    }
    memset(p, 0, sizeof(bm_vdi_info_t));

    p->vpu_fd   = -1;
    p->pvip     = NULL;
    p->task_num = 0;

    p_vdi_info[enc_core_idx] = p;

    return p;
}

static int bm_vdi_free_info_ptr(uint32_t core_idx)
{
#if defined(BM_PCIE_MODE)
    int enc_core_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
#else
    int enc_core_idx = 0;
#endif
    if (p_vdi_info[enc_core_idx]) {
        free(p_vdi_info[enc_core_idx]);
        p_vdi_info[enc_core_idx] = NULL;
    }
    return 0;
}

static bm_vdi_info_t* bm_vdi_check_info_ptr(uint32_t core_idx)
{
#if defined(BM_PCIE_MODE)
    int enc_core_idx = core_idx/MAX_NUM_VPU_CORE_CHIP;
#else
    int enc_core_idx = 0;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE) {
        VLOG(ERR, "The core index %d exceeds the max value(%d)\n", MAX_NUM_VPU_CORE);
        return NULL;
    }

    if (enc_core_idx >= MAX_NUM_ENC_CORE) {
        VLOG(ERR, "The encoder core index %d exceeds the max value(%d)\n", MAX_NUM_ENC_CORE);
        return NULL;
    }

    if (p_vdi_info[enc_core_idx] == NULL) {
        VLOG(ERR, "info pointer is NULL\n");
        return NULL;
    }

    return p_vdi_info[enc_core_idx];
}

int vdienc_get_video_cap(int core_idx,int *video_cap, int *bin_type, int *max_core_num, int *chip_id)
{
//    static int get_chip_info_flags = ATOMIC_FLAG_INIT;
    static int get_chip_info_flags = 0;
    int tmp_chip_id = 0;
    int tmp_core_num = 0;

    int board_idx = core_idx / MAX_NUM_VPU_CORE_CHIP;

    if (videnc_chip_info[board_idx] != 0) {
        *max_core_num = videnc_chip_info[board_idx] & 0xff;
        *chip_id = videnc_chip_info[board_idx] >> 16 & 0xff;
        return 0;
    }

//    while (atomic_flag_test_and_set(&get_chip_info_flags))
    while (__atomic_test_and_set(&get_chip_info_flags, __ATOMIC_SEQ_CST))
    {
        usleep(1000);
    }
        if (videnc_chip_info[board_idx] != 0) {
        *max_core_num = videnc_chip_info[board_idx] & 0xff;
        *chip_id = videnc_chip_info[board_idx] >> 16 & 0xff;
//        atomic_flag_clear(&get_chip_info_flags);
        __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
        return 0;
    }

#ifndef BM_PCIE_MODE
    int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);
    int tmp_bin_type = 0;
    int tmp_video_cap = 0;
#else

    char vpu_dev_name[32] = {0};
    sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
    if(access(vpu_dev_name,F_OK) == -1) {
//        atomic_flag_clear(&get_chip_info_flags);
        __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
        return -2;
    }
    int vpu_fd = open(vpu_dev_name, O_RDWR);
#endif

    if (vpu_fd < 0) {
        VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
//        atomic_flag_clear(&get_chip_info_flags);
        __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
        return -1;
    }

#ifndef BM_PCIE_MODE
    if (ioctl(vpu_fd, VDI_IOCTL_GET_VIDEO_CAP, &tmp_video_cap) < 0){
        close(vpu_fd);
//        atomic_flag_clear(&get_chip_info_flags);
        __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
        return -1;
    }
    *video_cap = tmp_video_cap;
    if (ioctl(vpu_fd, VDI_IOCTL_GET_BIN_TYPE, &tmp_bin_type) < 0){
        close(vpu_fd);
//        atomic_flag_clear(&get_chip_info_flags);
        __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
        return -1;
    }
    *bin_type = tmp_bin_type;
#endif
    if (ioctl(vpu_fd, VDI_IOCTL_GET_CHIP_ID, &tmp_chip_id) < 0){
        close(vpu_fd);
//        atomic_flag_clear(&get_chip_info_flags);
        __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
        return -1;
    }
    *chip_id = tmp_chip_id;

    if (ioctl(vpu_fd, VDI_IOCTL_GET_MAX_CORE_NUM, &tmp_core_num) < 0){
        close(vpu_fd);
//        atomic_flag_clear(&get_chip_info_flags);
        __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
        return -1;
    }
    *max_core_num = tmp_core_num;
    videnc_chip_info[board_idx] = tmp_core_num;
    videnc_chip_info[board_idx] = videnc_chip_info[board_idx] | (tmp_chip_id << 16);
    close(vpu_fd);
//    atomic_flag_clear(&get_chip_info_flags);
    __atomic_clear(&get_chip_info_flags, __ATOMIC_SEQ_CST);
    return 0;
}
