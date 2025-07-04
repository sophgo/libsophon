//-----------------------------------------------------------------------------
// COPYRIGHT (C) 2020   CHIPS&MEDIA INC. ALL RIGHTS RESERVED
//
// This file is distributed under BSD 3 clause and LGPL2.1 (dual license)
// SPDX License Identifier: BSD-3-Clause
// SPDX License Identifier: LGPL-2.1-only
//
// The entire notice above must be reproduced on all authorized copies.
//
// Description  :
//-----------------------------------------------------------------------------

#if defined(linux) || defined(__linux) || defined(ANDROID)
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/time.h>
#include <asm/io.h>

#include "../vdi.h"
#include "../vdi_osal.h"
#include "coda9/coda9_regdefine.h"
#include "wave/wave5_regdefine.h"
#include "wave/wave6_regdefine.h"
#include "device.h"
#include "main_helper.h"
#include "misc/debug.h"

#define VPU_BIT_REG_SIZE                    (0x4000*MAX_NUM_VPU_CORE)

typedef struct vpudrv_buffer_pool_t
{
    vpudrv_buffer_t vdb;
    int inuse;
} vpudrv_buffer_pool_t;

typedef struct  {
    unsigned long           core_idx;
    unsigned int            product_code;
    VPU_FD                  vpu_fd;
    int                     support_cq;
    vpudrv_buffer_pool_t    vpu_buffer_pool[MAX_VPU_BUFFER_POOL];
    vpu_instance_pool_t*    pvip;
    int                     task_num;
    int                     clock_state;
    vpudrv_buffer_t         vdb_register;
    vpu_buffer_t            vpu_common_memory;
    int                     vpu_buffer_pool_count;
    MUTEX_HANDLE            vpu_mutex;
    MUTEX_HANDLE            vmem_mutex;
    MUTEX_HANDLE            vpu_disp_mutex;
    void* rev1_mutex;
    pid_t pid;
    unsigned int chip_id;
    unsigned char             ext_addr;
    unsigned int instance_start_flag;
    atomic_t instance_count;
    int mutex[VDI_NUM_LOCK_HANDLES];
} vdi_info_t;

static vdi_info_t s_vdi_info[MAX_NUM_VPU_CORE];

#define VDI_SRAM_BASE_ADDR                  0x00000000    // if we can know the sram address in SOC directly for vdi layer. it is possible to set in vdi layer without allocation from driver
#define VDI_SYSTEM_ENDIAN                   VDI_LITTLE_ENDIAN
#define VDI_128BIT_BUS_SYSTEM_ENDIAN        VDI_128BIT_LITTLE_ENDIAN

extern int chip_id;
extern vpudrv_buffer_t s_vpu_register[MAX_NUM_VPU_CORE];
extern unsigned int vc_read_reg(unsigned int addr);
extern unsigned int vc_write_reg(unsigned int addr, unsigned int data);
extern int vc_memcpy_s2d(void *src, uint64_t dst, uint32_t size);
extern int vc_memcpy_d2s(void *dst, uint64_t src, uint32_t size);

int swap_endian(unsigned long core_idx, unsigned char *data, int len, int endian);

int vdi_invalidate_ion_cache(uint64_t u64PhyAddr, void *pVirAddr,
                 uint32_t u32Len)
{
    vpudrv_buffer_t vdb;

    vdb.phys_addr = u64PhyAddr;
    vdb.virt_addr = (unsigned long)pVirAddr;
    vdb.size = u32Len;

    return vpu_invalidate_dcache(&vdb);
}

int vdi_flush_ion_cache(uint64_t u64PhyAddr, void *pVirAddr, uint32_t u32Len)
{
    vpudrv_buffer_t vdb;

    vdb.phys_addr = u64PhyAddr;
    vdb.virt_addr = (unsigned long)pVirAddr;
    vdb.size = u32Len;

    return vpu_flush_dcache(&vdb);
}

int vdi_probe(unsigned long core_idx)
{
    int ret;

    ret = vdi_init(core_idx);
    vdi_release(core_idx);
    return ret;
}

int vdi_init(unsigned long core_idx)
{
    vdi_info_t *vdi;
    int* pCodecInst;
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return 0;

    vdi = &s_vdi_info[core_idx];

    if (vdi->vpu_fd != (VPU_FD)-1 && vdi->vpu_fd != (VPU_FD)0x00)
    {
        vdi->task_num++;
        return 0;
    }

    if (vpu_op_open(core_idx) < 0){
        VLOG(ERR, "[VDI] Can't open vpu driver\n");
        goto ERR_VDI_INIT;
    }
    vdi->vpu_fd = 'v' + core_idx;

    vdi->chip_id = chip_id;
    memset(vdi->vpu_buffer_pool, 0x00, sizeof(vpudrv_buffer_pool_t)*MAX_VPU_BUFFER_POOL);

    if (!vdi_get_instance_pool(core_idx))
    {
        VLOG(INFO, "[VDI] fail to create shared info for saving context \n");
        goto ERR_VDI_INIT;
    }

    if (vdi->pvip->instance_pool_inited == FALSE)
    {

        for( i = 0; i < MAX_NUM_INSTANCE; i++) {
            pCodecInst = (int *)vdi->pvip->codecInstPool[i];
            pCodecInst[1] = i;  // indicate instIndex of CodecInst
            pCodecInst[0] = 0;  // indicate inUse of CodecInst
        }
        vdi->pvip->instance_pool_inited = TRUE;
    }
    vdi->vdb_register.size = core_idx;
    memcpy(&vdi->vdb_register, &s_vpu_register[vdi->vdb_register.size], sizeof(vpudrv_buffer_t));
    // vdi->vdb_register.virt_addr = (unsigned long)ioremap(vdi->vdb_register.phys_addr, vdi->vdb_register.size);

    // if ((void *)vdi->vdb_register.virt_addr == NULL)
    // {
    //     VLOG(ERR, "[VDI] fail to map vpu registers \n");
    //     goto ERR_VDI_INIT;
    // }

    VLOG(INFO, "[VDI] map vdb_register core_idx=%d, virtaddr=0x%x, size=%d\n", core_idx, (int)vdi->vdb_register.virt_addr, vdi->vdb_register.size);


    if (vdi_lock(core_idx) < 0)
    {
        VLOG(ERR, "[VDI] fail to handle lock function\n");
        goto ERR_VDI_INIT;
    }
    vdi_set_clock_gate(core_idx, 1);

    vdi->product_code = vdi_read_register(core_idx, VPU_PRODUCT_CODE_REGISTER);

    if (vdi_allocate_common_memory(core_idx) < 0)
    {
        vdi_unlock(core_idx);
        VLOG(ERR, "[VDI] fail to get vpu common buffer from driver\n");
        goto ERR_VDI_INIT;
    }


    vdi->core_idx = core_idx;
    vdi->task_num++;
    vdi_set_clock_gate(core_idx, 0);
    atomic_set(&vdi->instance_count, 1);
    vdi_unlock(core_idx);
    VLOG(INFO, "[VDI] success to init driver \n");
    return 0;

ERR_VDI_INIT:
    vdi_release(core_idx);
    return -1;
}

int vdi_set_bit_firmware_to_pm(unsigned long core_idx, const unsigned short *code)
{
    int i;
    vpu_bit_firmware_info_t *bit_firmware_info;
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return 0;

    vdi = &s_vdi_info[core_idx];

    if (!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return 0;

    bit_firmware_info = vzalloc(sizeof(bit_firmware_info));
    bit_firmware_info->size = sizeof(vpu_bit_firmware_info_t);
    bit_firmware_info->core_idx = core_idx;
    bit_firmware_info->reg_base_offset = 0;
    if (PRODUCT_CODE_CODA_SERIES(vdi->product_code)) {
        for (i=0; i<512; i++) {
            bit_firmware_info->bit_code[i] = code[i];
        }
    }

    if (vpu_op_write((const char *)(bit_firmware_info), bit_firmware_info->size) < 0)
    {
        VLOG(ERR, "[VDI] fail to vdi_set_bit_firmware core=%d\n", bit_firmware_info->core_idx);
        vfree(bit_firmware_info);
        return -1;
    }

    vfree(bit_firmware_info);
    return 0;
}


#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
int vdi_get_task_num(unsigned long core_idx)
{
    vdi_info_t *vdi;
    vdi = &s_vdi_info[core_idx];

    if (!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == 0x00)
        return -1;

    return vdi->task_num;
}
#endif

int vdi_release(unsigned long core_idx)
{
    int i;
    vpudrv_buffer_t vdb;
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return 0;

    vdi = &s_vdi_info[core_idx];

    if (!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return 0;

    if (vdi_lock(core_idx) < 0)
    {
        VLOG(ERR, "[VDI] fail to handle lock function\n");
        return -1;
    }

    if (vdi->task_num > 1) // means that the opened instance remains
    {
        vdi->task_num--;
        vdi_unlock(core_idx);
        return 0;
    }

    if (vdi->vdb_register.virt_addr)
        iounmap((void *)vdi->vdb_register.virt_addr);

    osal_memset(&vdi->vdb_register, 0x00, sizeof(vpudrv_buffer_t));
    vdb.size = 0;
    // get common memory information to free virtual address
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_common_memory.phys_addr >= vdi->vpu_buffer_pool[i].vdb.phys_addr &&
            vdi->vpu_common_memory.phys_addr < (vdi->vpu_buffer_pool[i].vdb.phys_addr + vdi->vpu_buffer_pool[i].vdb.size))
        {
            vdi->vpu_buffer_pool[i].inuse = 0;
            vdi->vpu_buffer_pool_count--;
            vdb = vdi->vpu_buffer_pool[i].vdb;
            break;
        }
    }

    if (vdb.size > 0)
    {
        osal_memset(&vdi->vpu_common_memory, 0x00, sizeof(vpu_buffer_t));
    }

    vdi->task_num--;
    vpu_op_close(core_idx);
    vdi->vpu_fd = -1;
    vdi_unlock(core_idx);
    osal_memset(vdi, 0x00, sizeof(vdi_info_t));

    return 0;
}

int vdi_get_common_memory(unsigned long core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00) {
        return -1;
    }

    osal_memcpy(vb, &vdi->vpu_common_memory, sizeof(vpu_buffer_t));

    return 0;
}

int vdi_allocate_common_memory(unsigned long core_idx)
{
    vdi_info_t *vdi = &s_vdi_info[core_idx];
    vpudrv_buffer_t vdb;
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    if(!vdi || vdi->vpu_fd==(VPU_FD)-1 || vdi->vpu_fd==(VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vdb.core_idx = core_idx;
    vdb.size = SIZE_COMMON;

    if (vpu_get_common_memory(&vdb) < 0)
    {
        VLOG(ERR, "[VDI] fail to vdi_allocate_common_memory size=%d\n", vdb.size);
        return -1;
    }

    //vdb.virt_addr = (unsigned long)phys_to_virt(vdb.phys_addr);
    vdb.virt_addr = vdb.base;
    if ((void *)vdb.virt_addr == NULL)
    {
        VLOG(ERR, "[VDI] fail to map common memory phyaddr=0x%lx, size = %d\n", vdb.phys_addr, (int)vdb.size);
        return -1;
    }

    VLOG(INFO, "[VDI] vdi_allocate_common_memory, physaddr=0x%x, virtaddr=0x%x\n", (int)vdb.phys_addr, (int)vdb.virt_addr);
    // convert os driver buffer type to vpu buffer type
    vdi->pvip->vpu_common_buffer.size = SIZE_COMMON;
    vdi->pvip->vpu_common_buffer.phys_addr = (unsigned long)(vdb.phys_addr);
    vdi->pvip->vpu_common_buffer.base = (unsigned long)(vdb.base);
    vdi->pvip->vpu_common_buffer.virt_addr = (unsigned long)(vdb.virt_addr);

    osal_memcpy(&vdi->vpu_common_memory, &vdi->pvip->vpu_common_buffer, sizeof(vpu_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 0)
        {
            vdi->vpu_buffer_pool[i].vdb = vdb;
            vdi->vpu_buffer_pool_count++;
            vdi->vpu_buffer_pool[i].inuse = 1;
            break;
        }
    }

    vdi_set_ddr_map(core_idx, vdb.phys_addr >> 32);

    VLOG(INFO, "[VDI] vdi_get_common_memory physaddr=0x%x, size=%d, virtaddr=0x%x\n", \
        (int)vdi->vpu_common_memory.phys_addr, (int)vdi->vpu_common_memory.size, (int)vdi->vpu_common_memory.virt_addr);

    return 0;
}

vpu_instance_pool_t *vdi_get_instance_pool(unsigned long core_idx)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return NULL;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00 )
        return NULL;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));
    if (!vdi->pvip)
    {
        vdb.core_idx = core_idx;
        vdb.size = sizeof(vpu_instance_pool_t) + sizeof(MUTEX_HANDLE)*VDI_NUM_LOCK_HANDLES;

        if (vpu_get_instance_pool(&vdb) < 0)
        {
            VLOG(ERR, "[VDI] fail to allocate get instance pool physical space=%d\n",
                 vdb.size);
            return NULL;
        }

        //vdb.virt_addr = (unsigned long)phys_to_virt(vdb.phys_addr);
        vdb.virt_addr = vdb.base;
        if ((void *)vdb.virt_addr == NULL)
        {
            VLOG(ERR, "[VDI] fail to map instance pool phyaddr=0x%lx, size = %d\n", vdb.phys_addr, vdb.size);
            return NULL;
        }

        vdi->pvip = (vpu_instance_pool_t *)(vdb.virt_addr);
        vdi->vpu_mutex      = (void *)((unsigned long)vdi->mutex); //change the pointer of vpu_mutex to at end pointer of vpu_instance_pool_t to assign at allocated position.
        vdi->vpu_disp_mutex = (void *)((unsigned long)vdi->mutex + sizeof(MUTEX_HANDLE));
        vdi->vmem_mutex     = (void *)((unsigned long)vdi->mutex + 2*sizeof(MUTEX_HANDLE));
        vdi->rev1_mutex = (void *)((unsigned long)vdi->mutex + 4*sizeof(MUTEX_HANDLE));

        VLOG(INFO, "[VDI] instance pool physaddr=0x%x, virtaddr=0x%x, base=0x%x, size=%d\n", (int)vdb.phys_addr, (int)vdb.virt_addr, (int)vdb.base, (int)vdb.size);
    }

    return (vpu_instance_pool_t *)vdi->pvip;
}

int vdi_open_instance(unsigned long core_idx, unsigned long inst_idx, int support_cq)
{
    vdi_info_t *vdi;
    vpudrv_inst_info_t inst_info = {0, };

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    inst_info.core_idx = core_idx;
    inst_info.inst_idx = inst_idx;
    inst_info.support_cq = support_cq;
    vdi->support_cq = support_cq;

    if (vpu_open_instance(&inst_info) < 0)
    {
        VLOG(ERR, "[VDI] fail to deliver open instance num inst_idx=%d\n", (int)inst_idx);
        return -1;
    }

    vdi->pvip->vpu_instance_num = inst_info.inst_open_count;

    return 0;
}

int vdi_close_instance(unsigned long core_idx, unsigned long inst_idx)
{
    vdi_info_t *vdi;
    vpudrv_inst_info_t inst_info = {0, };
    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    inst_info.core_idx = core_idx;
    inst_info.inst_idx = inst_idx;

    if (vpu_close_instance(&inst_info) < 0)
    {
        VLOG(ERR, "[VDI] fail to deliver open instance num inst_idx=%d\n", (int)inst_idx);
        return -1;
    }

    vdi->pvip->vpu_instance_num = inst_info.inst_open_count;

    return 0;
}

int vdi_get_instance_num(unsigned long core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    return vdi->pvip->vpu_instance_num;
}


int vdi_hw_reset(unsigned long core_idx) // DEVICE_ADDR_SW_RESET
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    return vpu_hw_reset(core_idx);

}

int vdi_vpu_reset(unsigned long core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    //return ioctl(vdi->vpu_fd, VDI_IOCTL_VPU_RESET, 0); // TODO
    return 0;
}

int vdi_lock(unsigned long core_idx)
{
    vdi_info_t *vdi;
    int count;
    int sync_ret;
    int sync_val = task_pid_nr(current);//current->tgid;// = getpid();
    volatile int *sync_lock_ptr = NULL;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;
    count = 0;
    sync_lock_ptr = (volatile int *)vdi->vpu_mutex;
    while((sync_ret = __sync_val_compare_and_swap(sync_lock_ptr, 0, sync_val)) != 0)
    {
        count++;
        if (count > (ATOMIC_SYNC_TIMEOUT)) {
            VLOG(ERR, "%s failed to get lock sync_ret=%d, sync_val=%d, sync_ptr=%d \n", __FUNCTION__, sync_ret, sync_val, (int)*sync_lock_ptr);
            return -1;
        }
        osal_msleep(1);
    }

    return 0;//lint !e454
}

void vdi_unlock(unsigned long core_idx)
{
    vdi_info_t *vdi;
    volatile int *sync_lock_ptr = NULL;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return;

    sync_lock_ptr = (volatile int *)vdi->vpu_mutex;
    __sync_lock_release(sync_lock_ptr);
}

int vdi_disp_lock(unsigned long core_idx)
{
    vdi_info_t *vdi;
    int count;
    int sync_ret;
    int sync_val = task_pid_nr(current);
    volatile int *sync_lock_ptr = NULL;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    count = 0;
    sync_lock_ptr = (volatile int *)vdi->vpu_disp_mutex;
    while((sync_ret = __sync_val_compare_and_swap(sync_lock_ptr, 0, sync_val)) != 0)
    {
        count++;
        if (count > (ATOMIC_SYNC_TIMEOUT)) {
            VLOG(ERR, "%s failed to get lock sync_ret=%d, sync_val=%d, sync_ptr=%d \n", __FUNCTION__, sync_ret, sync_val, (int)*sync_lock_ptr);
            return -1;
        }
        osal_msleep(1);
    }

    return 0;//lint !e454
}

void vdi_disp_unlock(unsigned long core_idx)
{
    vdi_info_t *vdi;
    volatile int *sync_lock_ptr = NULL;
    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return;

    sync_lock_ptr = (volatile int *)vdi->vpu_disp_mutex;
    __sync_lock_release(sync_lock_ptr);

}


static int vmem_lock(unsigned long core_idx)
{
    vdi_info_t *vdi;
    int count;
    int sync_ret;
    int sync_val = task_pid_nr(current);
    volatile int *sync_lock_ptr = NULL;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    count = 0;
    sync_lock_ptr = (volatile int *)vdi->vmem_mutex;
    while((sync_ret = __sync_val_compare_and_swap(sync_lock_ptr, 0, sync_val)) != 0)
    {
        count++;
        if (count > (ATOMIC_SYNC_TIMEOUT)) {
            VLOG(ERR, "%s failed to get lock sync_ret=%d, sync_val=%d, sync_ptr=%d \n", __FUNCTION__, sync_ret, sync_val, (int)*sync_lock_ptr);
            return -1;
        }
        osal_msleep(1);
    }

    return 0;//lint !e454
}

static void vmem_unlock(unsigned long core_idx)
{
    vdi_info_t *vdi;
    volatile int *sync_lock_ptr = NULL;
    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return;

    sync_lock_ptr = (volatile int *)vdi->vmem_mutex;
    __sync_lock_release(sync_lock_ptr);

}

void vdi_write_register(unsigned long core_idx, unsigned int addr, unsigned int data)
{
    vdi_info_t *vdi;
    unsigned int reg_addr;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return;

    reg_addr = (addr + (unsigned long)vdi->vdb_register.phys_addr);
    vc_write_reg(reg_addr, data);
}

unsigned int vdi_read_register(unsigned long core_idx, unsigned int addr)
{
    vdi_info_t *vdi;
    unsigned int ret = 0;
    unsigned int reg_addr;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return (unsigned int)-1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return (unsigned int)-1;

    reg_addr = (addr + vdi->vdb_register.phys_addr);
    ret = vc_read_reg(reg_addr);
    return ret;
}

#define FIO_TIMEOUT         100

unsigned int vdi_fio_read_register(unsigned long core_idx, unsigned int addr)
{
    unsigned int ctrl;
    unsigned int count = 0;
    unsigned int data  = 0xffffffff;

    ctrl  = (addr&0xffff);
    ctrl |= (0<<16);    /* read operation */
    vdi_write_register(core_idx, W5_VPU_FIO_CTRL_ADDR, ctrl);
    count = FIO_TIMEOUT;
    while (count--) {
        ctrl = vdi_read_register(core_idx, W5_VPU_FIO_CTRL_ADDR);
        if (ctrl & 0x80000000) {
            data = vdi_read_register(core_idx, W5_VPU_FIO_DATA);
            break;
        }
    }

    return data;
}

void vdi_fio_write_register(unsigned long core_idx, unsigned int addr, unsigned int data)
{
    unsigned int ctrl;
    unsigned int count = 0;

    vdi_write_register(core_idx, W5_VPU_FIO_DATA, data);
    ctrl  = (addr&0xffff);
    ctrl |= (1<<16);    /* write operation */
    vdi_write_register(core_idx, W5_VPU_FIO_CTRL_ADDR, ctrl);

    count = FIO_TIMEOUT;
    while (count--) {
        ctrl = vdi_read_register(core_idx, W5_VPU_FIO_CTRL_ADDR);
        if (ctrl & 0x80000000) {
            break;
        }
    }
}


int vdi_clear_memory(unsigned long core_idx, PhysicalAddress addr, int len, int endian)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
    int i;
    Uint8*  zero;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 1)
        {
            vdb = vdi->vpu_buffer_pool[i].vdb;
            if (addr >= vdb.phys_addr && addr < (vdb.phys_addr + vdb.size))
                break;
        }
    }

    if (!vdb.size) {
        VLOG(ERR, "address 0x%08x is not mapped address!!!\n", (int)addr);
        return -1;
    }

    zero = (Uint8*)osal_malloc(len);
    osal_memset((void*)zero, 0x00, len);
    vc_memcpy_s2d(zero, addr, len);

    if (vpu_flush_dcache(&vdb) < 0) {
        VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
    }

    osal_free(zero);

    return len;
}

int vdi_set_memory(unsigned long core_idx, PhysicalAddress addr, int len, int endian, Uint32 data)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
    int i;
    Uint8*  zero;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 1)
        {
            vdb = vdi->vpu_buffer_pool[i].vdb;
            if (addr >= vdb.phys_addr && addr < (vdb.phys_addr + vdb.size))
                break;
        }
    }

    if (!vdb.size) {
        VLOG(ERR, "address 0x%08x is not mapped address!!!\n", (int)addr);
        return -1;
    }

    zero = (Uint8*)osal_malloc(len);
    osal_memset((void*)zero, data, len);
    vc_memcpy_s2d(zero, addr, len);
    if (vpu_flush_dcache(&vdb) < 0) {
        VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
    }

    osal_free(zero);

    return len;
}

int vdi_write_memory(unsigned long core_idx, PhysicalAddress addr, unsigned char *data, int len, int endian)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
    int i;
    unsigned long offset;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    if (!data)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 1)
        {
            vdb = vdi->vpu_buffer_pool[i].vdb;
            if (addr >= vdb.phys_addr && addr < (vdb.phys_addr + vdb.size)) {
                break;
            }
        }
    }

    if (!vdb.size) {
        VLOG(ERR, "address 0x%08x is not mapped address!!!\n", (int)addr);
        return -1;
    }

    offset = addr - (unsigned long)vdb.phys_addr;
    swap_endian(core_idx, data, len, endian);
    vc_memcpy_s2d(data, addr, len);

    if (vdb.is_cached) {
        if (vpu_flush_dcache(&vdb) < 0) {
            VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
            return -1;
        }
    }

    return len;
}

int vdi_read_memory(unsigned long core_idx, PhysicalAddress addr, unsigned char *data, int len, int endian)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
    int i;
    unsigned long offset;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd== (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 1)
        {
            vdb = vdi->vpu_buffer_pool[i].vdb;
            if (addr >= vdb.phys_addr && addr < (vdb.phys_addr + vdb.size))
                break;
        }
    }

    if (!vdb.size)
        return -1;

    offset = addr - (unsigned long)vdb.phys_addr;
    if (vpu_invalidate_dcache(&vdb) < 0) {
        VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
        return -1;
    }

    vc_memcpy_d2s(data, addr, len);
    swap_endian(core_idx, data, len,  endian);

    return len;
}

int vdi_allocate_dma_memory(unsigned long core_idx, vpu_buffer_t *vb, int memTypes, int instIndex)
{
    vdi_info_t *vdi;
    int i;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vdb.core_idx = core_idx;
    vdb.size = vb->size;

    if (vpu_allocate_physical_memory(&vdb) < 0)
    {
        VLOG(ERR, "[VDI] fail to vdi_allocate_dma_memory type:%d, size=%d\n", memTypes, vdb.size);
        return -1;
    }

    vb->phys_addr = (unsigned long)vdb.phys_addr;
    vb->base = (unsigned long)vdb.base;

    //map to virtual address
    //vdb.virt_addr = (unsigned long)phys_to_virt(vdb.phys_addr);
    vdb.virt_addr = vdb.base;

    if ((void *)vdb.virt_addr == NULL)
    {
        memset(vb, 0x00, sizeof(vpu_buffer_t));
        return -1;
    }

    vb->virt_addr = vdb.virt_addr;

    vmem_lock(core_idx);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 0)
        {
            vdi->vpu_buffer_pool[i].vdb = vdb;
            vdi->vpu_buffer_pool_count++;
            vdi->vpu_buffer_pool[i].inuse = 1;
            break;
        }
    }

    if (MAX_VPU_BUFFER_POOL == i) {
        VLOG(ERR, "[VDI] fail to vdi_allocate_dma_memory, vpu_buffer_pool_count=%d MAX_VPU_BUFFER_POOL=%d\n", vdi->vpu_buffer_pool_count, MAX_VPU_BUFFER_POOL);
        vmem_unlock(core_idx);
        return -1;
    }
    vmem_unlock(core_idx);

    VLOG(INFO, "[VDI] vdi_allocate_dma_memory, physaddr=0x%llx, virtaddr=0x%llx~0x%llx, size=%d, memType=%d, count:%d\n",
       vb->phys_addr, vb->virt_addr, vb->virt_addr + vb->size, vb->size, memTypes, vdi->vpu_buffer_pool_count);

    return 0;
}

int vdi_insert_extern_memory(unsigned long core_idx, vpu_buffer_t *vb, int memTypes, int instIndex)
{
    vdi_info_t *vdi;
    int i;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];
    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vdb.core_idx = core_idx;
    vdb.size = vb->size;
    vdb.phys_addr = vb->phys_addr;
    vdb.base = vb->base;
    vdb.virt_addr = vb->virt_addr;

    vmem_lock(core_idx);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 0)
        {
            vdi->vpu_buffer_pool[i].vdb = vdb;
            vdi->vpu_buffer_pool_count++;
            vdi->vpu_buffer_pool[i].inuse = 1;
            break;
        }
    }

    if (MAX_VPU_BUFFER_POOL == i) {
        VLOG(ERR, "[VDI] fail to vdi_allocate_dma_memory, vpu_buffer_pool_count=%d MAX_VPU_BUFFER_POOL=%d\n", vdi->vpu_buffer_pool_count, MAX_VPU_BUFFER_POOL);
        vmem_unlock(core_idx);
        return -1;
    }
    vmem_unlock(core_idx);

    return 0;
}

unsigned long vdi_get_dma_memory_free_size(unsigned long core_idx)
{
    vdi_info_t *vdi;
    unsigned long size;

    vdi = &s_vdi_info[core_idx];
    if (vpu_get_free_mem_size(&size) < 0) {
        VLOG(ERR, "[VDI] fail VDI_IOCTL_GET_FREE_MEM_SIZE size=%ld\n", size);
        return 0;
    }

    return size;
}

int vdi_attach_dma_memory(unsigned long core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;
    int i;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vdb.size = vb->size;
    vdb.phys_addr = vb->phys_addr;
    vdb.base = vb->base;

    vdb.virt_addr = vb->virt_addr;

    vmem_lock(core_idx);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->vpu_buffer_pool[i].vdb = vdb;
            vdi->vpu_buffer_pool[i].inuse = 1;
            break;
        }
        else
        {
            if (vdi->vpu_buffer_pool[i].inuse == 0)
            {
                vdi->vpu_buffer_pool[i].vdb = vdb;
                vdi->vpu_buffer_pool_count++;
                vdi->vpu_buffer_pool[i].inuse = 1;
                break;
            }
        }
    }
    vmem_unlock(core_idx);

    return 0;
}

int vdi_dettach_dma_memory(unsigned long core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vb || !vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    if (vb->size == 0)
        return -1;

    vmem_lock(core_idx);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->vpu_buffer_pool[i].inuse = 0;
            vdi->vpu_buffer_pool_count--;
            break;
        }
    }
    vmem_unlock(core_idx);

    return 0;
}

void vdi_free_dma_memory(unsigned long core_idx, vpu_buffer_t *vb, int memTypes, int instIndex)
{
    vdi_info_t *vdi;
    int i;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vb || !vdi || vdi->vpu_fd== (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return;

    if (vb->size == 0)
        return ;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vmem_lock(core_idx);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->vpu_buffer_pool[i].inuse = 0;
            vdi->vpu_buffer_pool_count--;
            vdb = vdi->vpu_buffer_pool[i].vdb;
            break;
        }
    }

    if (!vdb.size)
    {
        VLOG(ERR, "[VDI] invalid buffer to free address = 0x%x\n", (int)vdb.virt_addr);
        vmem_unlock(core_idx);
        return ;
    }

    vpu_free_physical_memory(&vdb);
    osal_memset(vb, 0, sizeof(vpu_buffer_t));
    vmem_unlock(core_idx);
}

void vdi_remove_extern_memory(unsigned long core_idx, vpu_buffer_t *vb, int memTypes, int instIndex)
{
    vdi_info_t *vdi;
    int i;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];
    if(!vb || !vdi || vdi->vpu_fd== (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return;

    if (vb->size == 0)
        return ;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vmem_lock(core_idx);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->vpu_buffer_pool[i].inuse = 0;
            vdi->vpu_buffer_pool_count--;
            vdb = vdi->vpu_buffer_pool[i].vdb;
            break;
        }
    }

    if (!vdb.size)
    {
        VLOG(ERR, "[VDI] invalid buffer to free address = 0x%x\n", (int)vdb.virt_addr);
        vmem_unlock(core_idx);
        return ;
    }

    osal_memset(vb, 0, sizeof(vpu_buffer_t));
    vmem_unlock(core_idx);
}

int vdi_get_sram_memory(unsigned long core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi = NULL;
    Uint32 sram_size = 0;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vb || !vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    switch (vdi->product_code) {
    case BODA950_CODE:
    case CODA960_CODE:
    case CODA980_CODE:
    // 4K : 0x34600, FHD : 0x17D00
        sram_size = 0x34600; break;
    case WAVE511_CODE:
    /* 10bit profile : 8Kx8K -> 129024, 4Kx2K -> 64512
     */
        sram_size = 0x1F800; break;
    case WAVE517_CODE:
    /* 10bit profile : 8Kx8K -> 272384, 4Kx2K -> 104448
     */
        sram_size = 0x18000; break;
    case WAVE537_CODE:
    /* 10bit profile : 8Kx8K -> 272384, 4Kx2K -> 104448
     */
        sram_size = 0x42800; break;
    case WAVE521_CODE:
    /* 10bit profile : 8Kx8K -> 126976, 4Kx2K -> 63488
     */
        sram_size = 0x1F000; break;
    case WAVE521E1_CODE:
    /* 10bit profile : 8Kx8K -> 126976, 4Kx2K -> 63488
     */
        sram_size = 0x1F000; break;
    case WAVE521C_CODE:
    /* 10bit profile : 8Kx8K -> 129024, 4Kx2K -> 64512
     * NOTE: Decoder > Encoder
     */
        sram_size = 0xEC00; break;
    case WAVE521C_DUAL_CODE:
    /* 10bit profile : 8Kx8K -> 129024, 4Kx2K -> 64512
     * NOTE: Decoder > Encoder
     */
        sram_size = 0x1F800; break;
    case WAVE617_CODE:
    case WAVE627_CODE:
    case WAVE633_CODE:
    case WAVE637_CODE:
    case WAVE663_CODE:
    case WAVE677_CODE:
        sram_size = 0x1C1500; break;
    default:
        VLOG(ERR, "[VDI] check product_code(%x)\n", vdi->product_code);
        break;
    }


    // if we can know the sram address directly in vdi layer, we use it first for sdram address
    vb->phys_addr = VDI_SRAM_BASE_ADDR;
    vb->size      = sram_size;

    return 0;
}

int vdi_set_clock_gate(unsigned long core_idx, int enable)
{
    vdi_info_t *vdi = NULL;
    int ret;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if (!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    if (PRODUCT_CODE_W5_SERIES(vdi->product_code) || PRODUCT_CODE_W6_SERIES(vdi->product_code) || vdi->product_code == 0) {
        //CommandQueue does not support clock gate
        //first value 0
        vdi->clock_state = 1;
        return 0;
    }

    vdi->clock_state = enable;
    ret = 0;//vpu_set_clock_gate(&enable);

    return ret;
}

int vdi_get_clock_gate(unsigned long core_idx)
{
    vdi_info_t *vdi;
    int ret;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    ret = vdi->clock_state;
    return ret;
}

static int get_pc_addr(Uint32 product_code)
{
    if (PRODUCT_CODE_W_SERIES(product_code)) {
        return W5_VCPU_CUR_PC;
    }
    else if (PRODUCT_CODE_CODA_SERIES(product_code)) {
        return BIT_CUR_PC;
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", product_code);
        return -1;
    }
}

int vdi_wait_bus_busy(unsigned long core_idx, int timeout, unsigned int gdi_busy_flag)
{
    Uint64 elapse, cur;
    Uint32 pc;
    vdi_info_t *vdi;
    Uint32 gdi_status_check_value = 0x3f;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    elapse = osal_gettime();

    pc = get_pc_addr(vdi->product_code);
    if (pc == (Uint32)-1)
        return -1;
    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        gdi_status_check_value = 0x3f;
        if (PRODUCT_CODE_W6_SERIES(vdi->product_code)) {
            gdi_status_check_value = 0;
        }
        if (vdi->product_code == WAVE521C_CODE || vdi->product_code == WAVE521_CODE || vdi->product_code == WAVE521E1_CODE) {
            gdi_status_check_value = 0x00ff1f3f;
        }
    }
    while(1)
    {
        if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
            if (vdi_fio_read_register(core_idx, gdi_busy_flag) == gdi_status_check_value) break;
        }
        else if (PRODUCT_CODE_CODA_SERIES(vdi->product_code)) {
            if (vdi_read_register(core_idx, gdi_busy_flag) == 0x77) break;
        }
        else {
            VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
            return -1;
        }

        if (timeout > 0) {
            cur = osal_gettime();

            if ((cur - elapse) > timeout) {
                print_busy_timeout_status(core_idx, vdi->product_code, pc);
                return -1;
            }
        }
    }
    return 0;
}

int vdi_wait_vpu_busy(unsigned long core_idx, int timeout, unsigned int addr_bit_busy_flag)
{
    Uint64 elapse, cur;
    Uint32 pc;
    vdi_info_t *vdi;
    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    elapse = osal_gettime();

    pc = get_pc_addr(vdi->product_code);
    if (pc == (Uint32)-1)
        return -1;
    while(1)
    {
        if (vdi_read_register(core_idx, addr_bit_busy_flag) == 0)
            break;

        if (timeout > 0) {
            cur = osal_gettime();

            if ((cur - elapse) > timeout) {
                print_busy_timeout_status(core_idx, vdi->product_code, pc);
                return -1;
            }
        }
    }
    return 0;
}

int vdi_wait_vcpu_bus_busy(unsigned long core_idx, int timeout, unsigned int gdi_busy_flag)
{
    Uint64 elapse, cur;
    Uint32 pc;
    vdi_info_t *vdi;
    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    elapse = osal_gettime();

    pc = get_pc_addr(vdi->product_code);
    if (pc == (Uint32)-1)
        return -1;
    while(1)
    {
        if (vdi_fio_read_register(core_idx, gdi_busy_flag) == 0x00)
            break;
        if (timeout > 0) {
            cur = osal_gettime();

            if ((cur - elapse) > timeout) {
                print_busy_timeout_status(core_idx, vdi->product_code, pc);
                return -1;
            }
        }
    }
    return 0;
}

int vdi_wait_interrupt(unsigned long core_idx, unsigned int instIdx, int timeout)
{
    vdi_info_t *vdi = &s_vdi_info[core_idx];
    int intr_reason = -1;
    int ret;
    vpudrv_intr_info_t intr_info;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    intr_info.core_idx = core_idx;
    intr_info.timeout     = timeout;
    intr_info.intr_reason = 0;
    intr_info.intr_inst_index = instIdx;

    if (!(vdi->support_cq)) {
        intr_info.intr_inst_index = 0;
    }

    ret = vpu_wait_interrupt(&intr_info);
    if (ret != 0) {
        if (ret == -ETIME)
            return -1;
        else
            return -2;
    }

    intr_reason = intr_info.intr_reason;

    return intr_reason;
}

//------------------------------------------------------------------------------
// LOG & ENDIAN functions
//------------------------------------------------------------------------------

int vdi_get_system_endian(unsigned long core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        return VDI_128BIT_BUS_SYSTEM_ENDIAN;
    }
    else if(PRODUCT_CODE_CODA_SERIES(vdi->product_code)) {
        return VDI_SYSTEM_ENDIAN;
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }
}

int vdi_convert_endian(unsigned long core_idx, unsigned int endian)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        switch (endian) {
        case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
        case VDI_BIG_ENDIAN:          endian = 0x0f; break;
        case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
        case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
        }
    }
    else if(PRODUCT_CODE_CODA_SERIES(vdi->product_code)) {
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }

    return (endian&0x0f);
}

static Uint32 convert_endian_coda9_to_wave4(Uint32 endian)
{
    Uint32 converted_endian = endian;
    switch(endian) {
    case VDI_LITTLE_ENDIAN:       converted_endian = 0; break;
    case VDI_BIG_ENDIAN:          converted_endian = 7; break;
    case VDI_32BIT_LITTLE_ENDIAN: converted_endian = 4; break;
    case VDI_32BIT_BIG_ENDIAN:    converted_endian = 3; break;
    }
    return converted_endian;
}



int swap_endian(unsigned long core_idx, unsigned char *data, int len, int endian)
{
    vdi_info_t *vdi;
    int changes;
    int sys_endian;
    BOOL byteChange, wordChange, dwordChange, lwordChange;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == (VPU_FD)-1 || vdi->vpu_fd == (VPU_FD)0x00)
        return -1;

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        sys_endian = VDI_128BIT_BUS_SYSTEM_ENDIAN;
    }
    else if(PRODUCT_CODE_CODA_SERIES(vdi->product_code)) {
        sys_endian = VDI_SYSTEM_ENDIAN;
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }

    endian     = vdi_convert_endian(core_idx, endian);
    sys_endian = vdi_convert_endian(core_idx, sys_endian);
    if (endian == sys_endian)
        return 0;

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
    }
    else if (PRODUCT_CODE_CODA_SERIES(vdi->product_code)) {
        endian     = convert_endian_coda9_to_wave4(endian);
        sys_endian = convert_endian_coda9_to_wave4(sys_endian);
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }

    changes     = endian ^ sys_endian;
    byteChange  = changes&0x01;
    wordChange  = ((changes&0x02) == 0x02);
    dwordChange = ((changes&0x04) == 0x04);
    lwordChange = ((changes&0x08) == 0x08);

    if (byteChange)  byte_swap(data, len);
    if (wordChange)  word_swap(data, len);
    if (dwordChange) dword_swap(data, len);
    if (lwordChange) lword_swap(data, len);

    return 1;
}

int vdi_set_ddr_map(unsigned long core_idx, unsigned int ext_addr)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    vdi->ext_addr = ext_addr;

    return 0;
}

int vdi_get_ddr_map(unsigned long core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    return  vdi->ext_addr;
}

int vdi_get_instance_count(unsigned long core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    return atomic_read(&vdi->instance_count);
}

int vdi_request_instance(unsigned long core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    atomic_add(1, &vdi->instance_count);

    return 0;
}

int vdi_release_instance(unsigned long core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    atomic_sub(1, &vdi->instance_count);

    return 0;
}

#endif	//#if defined(linux) || defined(__linux) || defined(ANDROID)

