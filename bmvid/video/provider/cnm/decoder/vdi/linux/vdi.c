///------------------------------------------------------------------------------
// File: vdi.c
//
// Copyright (c) 2006, Chips & Media.  All rights reserved.
//------------------------------------------------------------------------------
#if defined(linux) || defined(__linux) || defined(ANDROID)

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef    _KERNEL_
#include <linux/delay.h>
#endif
#include <signal.h>        /* SIGIO */
#include <fcntl.h>        /* fcntl */
#include <pthread.h>
#include <sys/mman.h>        /* mmap */
#include <sys/ioctl.h>        /* ioctl */
#include <sys/errno.h>        /* errno */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/file.h>  /* flock */
#include <sched.h>

#include "vdi.h"
#include "vdi_osal.h"
#include "vpuconfig.h"
#include "coda9_regdefine.h"
#include "wave5_regdefine.h"
#include "linux/vpu.h"
#include "main_helper.h"

#ifdef BM_ION_MEM
#include "bm_ion.h"
#endif

#ifdef BM_PCIE_MODE
# if defined(CHIP_BM1682)
#  define VPU_DEVICE_NAME "/dev/bm1682-dev"
# elif defined(CHIP_BM1684)
#  define VPU_DEVICE_NAME "/dev/bm-sophon"
# else
#  error "PCIE mode is supported only in BM1682 and BM1684"
# endif
#else
#  define VPU_DEVICE_NAME "/dev/vpu"
#endif

#ifdef BM_PCIE_MODE
#define FAKE_PCIE_VIRT_ADDR 0xDEADBEEFl
#endif

#ifdef TRY_SEM_MUTEX
#include <semaphore.h>
typedef sem_t MUTEX_HANDLE;
#elif defined(TRY_FLOCK)
typedef int MUTEX_HANDLE;
__thread int s_flock_fd[MAX_NUM_VPU_CORE] = {[0 ... (MAX_NUM_VPU_CORE-1)] = -1};
__thread int s_disp_flock_fd[MAX_NUM_VPU_CORE] = {[0 ... (MAX_NUM_VPU_CORE-1)] = -1};
#elif defined(TRY_ATOMIC_LOCK)
typedef int MUTEX_HANDLE;
#else
typedef pthread_mutex_t    MUTEX_HANDLE;
#endif


#    define SUPPORT_INTERRUPT

#    define VPU_BIT_REG_SIZE    (0x10000*MAX_NUM_VPU_CORE)
#ifdef CHIP_BM1684
#        define VDI_SRAM_BASE_ADDR            0x00000000    // if we can know the sram address in SOC directly for vdi layer. it is possible to set in vdi layer without allocation from driver
#else
#        define VDI_SRAM_BASE_ADDR            0xFFFC0000    // if we can know the sram address in SOC directly for vdi layer. it is possible to set in vdi layer without allocation from driver
#endif
#        define VDI_WAVE510_SRAM_SIZE        0x25000     // 8Kx8K MAIN10 MAX size
#        define VDI_WAVE512_SRAM_SIZE        0x80000
#        define VDI_WAVE515_SRAM_SIZE        0x80000
#        define VDI_WAVE520_SRAM_SIZE        0x25000     // 8Kx8X MAIN10 MAX size
#        define VDI_WAVE525_SRAM_SIZE        0x25000     // 8Kx8X MAIN10 MAX size
#        define VDI_CODA9_SRAM_SIZE           0xE000     // FHD MAX size, 0x17D00  4K MAX size 0x34600
#        define VDI_WAVE511_SRAM_SIZE        0x14400     /* 10bit profile : 8Kx8K -> 143360, 4Kx2K -> 71680
                                                          *  8bit profile : 8Kx8K -> 114688, 4Kx2K -> 57344 */
#        define VDI_WAVE511_SRAM_SIZE_1684X   0x28800    //10368*128 162k
#        define VDI_WAVE521_SRAM_SIZE        0x20400     /* 10bit profile : 8Kx8K -> 132096, 4Kx2K -> 66560
                                                          *  8bit profile : 8Kx8K ->  99328, 4Kx2K -> 51176 */
#        define VDI_WAVE521C_SRAM_SIZE       0x2D000     /* 10bit profile : 8Kx8K -> 143360, 4Kx2K -> 71680
                                                          *  8bit profile : 8Kx8K -> 114688, 4Kx2K -> 57344
                                                          * NOTE: Decoder > Encoder */


#   define VDI_SYSTEM_ENDIAN                VDI_LITTLE_ENDIAN
#   define VDI_128BIT_BUS_SYSTEM_ENDIAN     VDI_128BIT_LITTLE_ENDIAN
#define VDI_NUM_LOCK_HANDLES                4

#ifdef SUPPORT_MULTI_CORE_IN_ONE_DRIVER
#define VPU_CORE_BASE_OFFSET 0x10000
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#endif
typedef struct vpudrv_buffer_pool_t
{
    vpudrv_buffer_t vdb;
    int inuse;
} vpudrv_buffer_pool_t;

typedef struct  {
    unsigned long core_idx;
    unsigned int product_code;
    int vpu_fd;
    vpu_instance_pool_t *pvip;
    int task_num;
    int clock_state;
    vpudrv_buffer_t vdb_register;
    vpu_buffer_t vpu_common_memory;
    vpudrv_buffer_pool_t vpu_buffer_pool[MAX_VPU_BUFFER_POOL];
    int vpu_buffer_pool_count;

    void* vpu_mutex;
    void* vpu_omx_mutex;
    void* vpu_disp_mutex;
    void* vpu_create_inst_flag;
    vpu_buffer_t vpu_inst_memory;
    vpu_buffer_t vpu_dump_flag;
    pid_t pid;
    unsigned int chip_id;
    vpudrv_reset_flag reset_core_flag;

} vdi_info_t;

static vdi_info_t s_vdi_info[MAX_NUM_VPU_CORE] = {0};
int  *p_vpu_create_inst_flag[MAX_NUM_VPU_CORE]= {NULL};
#ifdef CHIP_BM1880
static int s_size_common[MAX_NUM_VPU_CORE] = {(2*1024*1024)};
#elif defined(CHIP_BM1684)
#ifdef TEST_VPU_ONECORE_FPGA
static int s_size_common[MAX_NUM_VPU_CORE] = {
    ((2*1024*1024) + (COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ))
};
#else
static int s_size_common[MAX_NUM_VPU_CORE] = {
    ((2*1024*1024) + (COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ)),
    ((2*1024*1024) + (COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ)),
    ((2*1024*1024) + (COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ)),
    ((2*1024*1024) + (COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ)),
    ((2*1024*1024) + (COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ))
};
#endif
#else
static int s_size_common[MAX_NUM_VPU_CORE] = {
    (2*1024*1024), (2*1024*1024),
    ((2*1024*1024) + (COMMAND_QUEUE_DEPTH*ONE_TASKBUF_SIZE_FOR_CQ))
};
#endif
static int swap_endian(u64 core_idx, unsigned char *data, int len, int endian);

 int vdi_probe(u64 core_idx)
{
    int ret;

    ret = vdi_init(core_idx);
    vdi_release(core_idx);
    return ret;
}

int vdi_init(u64 core_idx)
{
    vdi_info_t *vdi;
#if defined(BM_PCIE_MODE)
    int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return 0;

    vdi = &s_vdi_info[core_idx];

    if (vdi->vpu_fd != -1 && vdi->vpu_fd != 0x00)
    {
        vdi->task_num++;
        return 0;
    }

#ifndef BM_PCIE_MODE
    vdi->vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
    char vpu_dev_name[32] = {0};
    sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
    if(access(vpu_dev_name,F_OK) == -1) {
        return -2;
    }
    vdi->vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
    VLOG(ERR,"[VDI] Open board %d, core %d, fd %d, dev %s\n",
         board_idx, chip_core_idx, vdi->vpu_fd, vpu_dev_name);
#endif
    if (vdi->vpu_fd < 0) {
        VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
        return -1;
    }

    if (ioctl(vdi->vpu_fd, VDI_IOCTL_GET_CHIP_ID, &(vdi->chip_id)) < 0){
        VLOG(INFO, "[VDI] fail to get chip id \n");
        goto ERR_VDI_INIT;
    }

    memset(&vdi->vpu_buffer_pool, 0x00, sizeof(vpudrv_buffer_pool_t)*MAX_VPU_BUFFER_POOL);

    if (!vdi_get_instance_pool(core_idx))
    {
        VLOG(INFO, "[VDI] fail to create shared info for saving context \n");
        goto ERR_VDI_INIT;
    }

    if (vdi->pvip->instance_pool_inited == FALSE)
    {
        int* pCodecInst;
#ifdef TRY_ATOMIC_LOCK
        *(int *)vdi->vpu_mutex = 0;
        *(int *)vdi->vpu_disp_mutex = 0;
        *(int *)vdi->vpu_create_inst_flag = 0;
#elif !defined(TRY_SEM_MUTEX) && !defined(TRY_FLOCK)
        pthread_mutexattr_t mutexattr;
        pthread_mutexattr_init(&mutexattr);
        pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
#if defined(ANDROID) || !defined(PTHREAD_MUTEX_ROBUST_NP)
#else
        /* If a process or a thread is terminated abnormally,
        * pthread_mutexattr_setrobust_np(attr, PTHREAD_MUTEX_ROBUST_NP) makes
        * next onwer call pthread_mutex_lock() without deadlock.
        */
        pthread_mutexattr_setrobust_np(&mutexattr, PTHREAD_MUTEX_ROBUST_NP);
#endif
        pthread_mutex_init((MUTEX_HANDLE *)vdi->vpu_mutex, &mutexattr);
        pthread_mutex_init((MUTEX_HANDLE *)vdi->vpu_disp_mutex, &mutexattr);
#endif
        for( i = 0; i < MAX_NUM_INSTANCE; i++) {
            pCodecInst = (int *)vdi->pvip->codecInstPool[i];
            pCodecInst[1] = i;    // indicate instIndex of CodecInst
            pCodecInst[0] = 0;    // indicate inUse of CodecInst
        }

        vdi->pvip->instance_pool_inited = TRUE;
    }
#if defined(TRY_SEM_MUTEX)
    if(vdi->vpu_mutex==NULL && vdi->vpu_disp_mutex==NULL) {
        char sem_name[255];
        char disp_sem_name[255];

        sprintf(sem_name, "%s%d", CORE_SEM_NAME, core_idx);
        sprintf(disp_sem_name, "%s%d", CORE_DISP_SEM_NEME, core_idx);
        VLOG(INFO, "sem_name : .. %s\n", sem_name);
        vdi->vpu_mutex = sem_open(sem_name, O_CREAT, 0666, 1);
        vdi->vpu_disp_mutex = sem_open(disp_sem_name, O_CREAT, 0666, 1);
    }
#endif

    if(vdi->vpu_omx_mutex == NULL) {
        vdi->vpu_omx_mutex = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init((pthread_mutex_t *)vdi->vpu_omx_mutex, NULL);
    }

#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
#ifndef BM_PCIE_MODE
    vdi->vdb_register.size = core_idx;
#else

    vdi->vdb_register.size = chip_core_idx;
#endif
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_GET_REGISTER_INFO, &vdi->vdb_register) < 0)
    {
        VLOG(ERR, "[VDI] fail to get host interface register\n");
        goto ERR_VDI_INIT;
    }
#endif
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
    vdi->vdb_register.virt_addr = (unsigned long)mmap(NULL, vdi->vdb_register.size, PROT_READ | PROT_WRITE, MAP_SHARED, vdi->vpu_fd, vdi->vdb_register.phys_addr);
#else
    vdi->vdb_register.size = VPU_BIT_REG_SIZE;
    vdi->vdb_register.virt_addr = (unsigned long)mmap(NULL, vdi->vdb_register.size, PROT_READ | PROT_WRITE, MAP_SHARED, vdi->vpu_fd, 0);
#endif
    if ((void *)vdi->vdb_register.virt_addr == MAP_FAILED)
    {
        VLOG(ERR, "[VDI] fail to map vpu registers \n");
        goto ERR_VDI_INIT;
    }
    VLOG(INFO, "[VDI] map vdb_register core_idx=%ld, padr 0x%lx, virtaddr=0x%lx, size=%d\n",
         core_idx, vdi->vdb_register.phys_addr, vdi->vdb_register.virt_addr, vdi->vdb_register.size);

    if (vdi_lock(core_idx) < 0)
    {
        VLOG(ERR, "[VDI] fail to handle lock function\n");
        goto ERR_VDI_INIT;
    }
    vdi_set_clock_gate(core_idx, 1);
    vdi->product_code = vdi_read_register(core_idx, VPU_PRODUCT_CODE_REGISTER);

    if (vdi_allocate_common_memory(core_idx) !=BM_SUCCESS)
    {
        VLOG(ERR, "[VDI] fail to get vpu common buffer from driver\n");
        goto ERR_VDI_INIT;
    }

    vdi->core_idx = core_idx;
    vdi->task_num++;
    vdi_set_clock_gate(core_idx, 0);
    vdi_unlock(core_idx);

    VLOG(INFO, "[VDI] success to init driver \n");
    return 0;

ERR_VDI_INIT:
    vdi_unlock(core_idx);
    vdi_release(core_idx);
    return -1;
}

int vdi_set_bit_firmware_to_pm(u64 core_idx, const unsigned short *code)
{
    vpu_bit_firmware_info_t bit_firmware_info;
    vdi_info_t *vdi;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return 0;

    vdi = &s_vdi_info[core_idx];

    if (!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return 0;

    bit_firmware_info.size = sizeof(vpu_bit_firmware_info_t);
#ifndef BM_PCIE_MODE
    bit_firmware_info.core_idx = core_idx;
#else
    bit_firmware_info.core_idx = chip_core_idx;
#endif
    bit_firmware_info.reg_base_offset = 0;

    for (i=0; i<512; i++)
        bit_firmware_info.bit_code[i] = code[i];

    if (write(vdi->vpu_fd, &bit_firmware_info, bit_firmware_info.size) < 0)
    {
        VLOG(ERR, "[VDI] fail to vdi_set_bit_firmware core=%d\n", bit_firmware_info.core_idx);
        return -1;
    }

    return 0;
}

void vdi_delay_ms(unsigned int delay_ms)
{
    usleep(delay_ms*1000);
}

#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
int vdi_get_task_num(u64 core_idx)
{
    vdi_info_t *vdi;
    vdi = &s_vdi_info[core_idx];

    if (!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;

    return vdi->task_num;
}
#endif

int vdi_release(u64 core_idx)
{
    int i;
    vpudrv_buffer_t vdb;
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return 0;

    vdi = &s_vdi_info[core_idx];

    if (!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
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
#if defined(TRY_FLOCK)
        if(s_flock_fd[core_idx] != -1) {
            close(s_flock_fd[core_idx]);
            s_flock_fd[core_idx] = -1;
        }
        if(s_disp_flock_fd[core_idx] != -1) {
            close(s_disp_flock_fd[core_idx]);
            s_disp_flock_fd[core_idx] = -1;
        }
#endif
        return 0;
    }

    if (vdi->vdb_register.virt_addr)
        munmap((void *)vdi->vdb_register.virt_addr, vdi->vdb_register.size);

    osal_memset(&vdi->vdb_register, 0x00, sizeof(vpudrv_buffer_t));
    vdb.size = 0;
    /* get common memory information to free virtual address */
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

    vdi_unlock(core_idx);

    if (vdb.size > 0)
    {
        munmap((void *)vdb.virt_addr, vdb.size);
        memset(&vdi->vpu_common_memory, 0x00, sizeof(vpu_buffer_t));
    }

    if (vdi->vpu_inst_memory.virt_addr) {
        munmap((void *)vdi->vpu_inst_memory.virt_addr, vdi->vpu_inst_memory.size);
    }

    if (vdi->vpu_dump_flag.virt_addr) {
        munmap((void *)vdi->vpu_dump_flag.virt_addr, vdi->vpu_dump_flag.size);
        vdi->vpu_dump_flag.virt_addr = 0;
    }
    vdi->task_num--;


    if (vdi->vpu_fd != -1 && vdi->vpu_fd != 0x00)
        close(vdi->vpu_fd);

    if(vdi->vpu_mutex!=NULL) {
#if defined(TRY_SEM_MUTEX)
        sem_close(vdi->vpu_mutex);
#else
        //pthread_mutex_destroy((pthread_mutex_t *)vdi->vpu_mutex);
#endif
    }
#if defined(TRY_FLOCK)
    if(s_flock_fd[core_idx] != -1) {
        close(s_flock_fd[core_idx]);
        s_flock_fd[core_idx] = -1;
    }
#endif

    if(vdi->vpu_disp_mutex!=NULL) {
#if defined(TRY_SEM_MUTEX)
        sem_close(vdi->vpu_disp_mutex);
#else
        //pthread_mutex_destroy((pthread_mutex_t *)vdi->vpu_disp_mutex);
#endif
    }
#if defined(TRY_FLOCK)
    if(s_disp_flock_fd[core_idx] != -1) {
        close(s_disp_flock_fd[core_idx]);
        s_disp_flock_fd[core_idx] = -1;
    }
#endif
    if(vdi->vpu_omx_mutex != NULL) {
        pthread_mutex_destroy((pthread_mutex_t *)vdi->vpu_omx_mutex);
        free(vdi->vpu_omx_mutex);
        vdi->vpu_omx_mutex = NULL;
    }
    memset(vdi, 0x00, sizeof(vdi_info_t));
    vdi->vpu_fd = -1;

    return 0;
}

int vdi_get_common_memory(u64 core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd==0x00)
        return -1;

    osal_memcpy(vb, &vdi->vpu_common_memory, sizeof(vpu_buffer_t));

    return 0;
}

int vdi_allocate_common_memory(u64 core_idx)
{
    vdi_info_t *vdi = &s_vdi_info[core_idx];
    vpudrv_buffer_t vdb;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd==0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vdb.size = s_size_common[chip_core_idx];

    vdb.core_idx = chip_core_idx;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_GET_COMMON_MEMORY, &vdb) < 0)
    {
        VLOG(ERR, "[VDI] fail to vdi_allocate_common_memory size=%d\n", vdb.size);
        return -1;
    }

#ifndef BM_PCIE_MODE
    vdb.virt_addr = (unsigned long)mmap(NULL, vdb.size, PROT_READ | PROT_WRITE, MAP_SHARED, vdi->vpu_fd, vdb.phys_addr);
    if ((void *)vdb.virt_addr == MAP_FAILED)
    {
        VLOG(ERR, "[VDI] fail to map common memory phyaddr=0x%lx, size = %d\n", vdb.phys_addr, (int)vdb.size);
        return -1;
    }
#else
    vdb.virt_addr = FAKE_PCIE_VIRT_ADDR;
#endif

    VLOG(INFO, "[VDI] vdi_allocate_common_memory, physaddr=0x%lx, virtaddr=0x%lx\n", vdb.phys_addr, vdb.virt_addr);

    // convert os driver buffer type to vpu buffer type
#ifdef SUPPORT_MULTI_CORE_IN_ONE_DRIVER
    vdi->pvip->vpu_common_buffer.size = s_size_common[chip_core_idx];
    vdi->pvip->vpu_common_buffer.phys_addr = (unsigned long)vdb.phys_addr;
    vdi->pvip->vpu_common_buffer.base = (unsigned long)vdb.base;
    vdi->pvip->vpu_common_buffer.virt_addr = (unsigned long)vdb.virt_addr;
#else
    vdi->pvip->vpu_common_buffer.size = SIZE_COMMON;
    vdi->pvip->vpu_common_buffer.phys_addr = (unsigned long)(vdb.phys_addr);
    vdi->pvip->vpu_common_buffer.base = (unsigned long)(vdb.base);
    vdi->pvip->vpu_common_buffer.virt_addr = (unsigned long)(vdb.virt_addr);
#endif
    osal_memcpy(&vdi->vpu_common_memory, &vdi->pvip->vpu_common_buffer, sizeof(vpu_buffer_t));
    pthread_mutex_lock((pthread_mutex_t *)vdi->vpu_omx_mutex);
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
    pthread_mutex_unlock((pthread_mutex_t *)vdi->vpu_omx_mutex);
    VLOG(INFO, "[VDI] vdi_allocate_common_memory. physaddr=0x%lx, size=%d, virtaddr=0x%lx\n",
         vdi->vpu_common_memory.phys_addr, vdi->vpu_common_memory.size, vdi->vpu_common_memory.virt_addr);

    return 0;
}

vpu_instance_pool_t *vdi_get_instance_pool(u64 core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return NULL;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00 )
        return NULL;

    if (!vdi->pvip)
    {
        int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
        vpudrv_buffer_t vdb;

        osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

        vdb.core_idx = chip_core_idx;

        vdb.base = chip_core_idx;
        vdb.size = sizeof(vpu_instance_pool_t) + sizeof(MUTEX_HANDLE)*VDI_NUM_LOCK_HANDLES;
        if (ioctl(vdi->vpu_fd, VDI_IOCTL_GET_INSTANCE_POOL, &vdb) < 0)
        {
            VLOG(ERR, "[VDI] fail to allocate get instance pool physical space=%d. [error=%s]\n",
                 vdb.size, strerror(errno));
            return NULL;
        }

        vdb.virt_addr = (unsigned long)mmap(NULL, vdb.size, PROT_READ | PROT_WRITE, MAP_SHARED, vdi->vpu_fd, vdb.phys_addr);
        if ((void *)vdb.virt_addr == MAP_FAILED)
        {
            VLOG(ERR, "[VDI] fail to map instance pool phyaddr=0x%lx, size = %d, errno: %s\n", vdb.phys_addr, vdb.size, strerror(errno));
            return NULL;
        }

        vdi->pvip = (vpu_instance_pool_t *)(vdb.virt_addr);

#if !defined(TRY_SEM_MUTEX) && !defined(TRY_FLOCK)
        vdi->vpu_mutex =      (void *)((unsigned long)vdi->pvip + sizeof(vpu_instance_pool_t));    //change the pointer of vpu_mutex to at end pointer of vpu_instance_pool_t to assign at allocated position.
        vdi->vpu_disp_mutex = (void *)((unsigned long)vdi->pvip + sizeof(vpu_instance_pool_t) + sizeof(MUTEX_HANDLE));
        vdi->vpu_create_inst_flag = (void *)((unsigned long)vdi->pvip + sizeof(vpu_instance_pool_t) + 2*sizeof(MUTEX_HANDLE));
        p_vpu_create_inst_flag[core_idx] = vdi->vpu_create_inst_flag;
        vdi->pid = getpid();
        VLOG(INFO, "get mutex addr: %p\n", vdi->vpu_mutex);
#endif
        vdi->vpu_inst_memory.virt_addr = vdb.virt_addr;
        vdi->vpu_inst_memory.size = vdb.size;
        vdi->vpu_inst_memory.phys_addr = vdb.phys_addr;
        vdi->vpu_inst_memory.base = vdb.base;
        VLOG(INFO, "[VDI] instance pool physaddr=0x%lx, virtaddr=0x%lx, base=0x%lx, size=%u, pvip=%p\n",
             vdb.phys_addr, vdb.virt_addr, vdb.base, vdb.size, vdi->pvip);
        vdb.size = DUMP_FLAG_MEM_SIZE;

#ifndef BM_PCIE_MODE
        vdb.virt_addr = (unsigned long)mmap(NULL, vdb.size, PROT_READ | PROT_WRITE, MAP_SHARED, vdi->vpu_fd, PAGE_SIZE);
        if ((void *)vdb.virt_addr == MAP_FAILED)
        {
            VLOG(ERR, "[VDI] fail to map dump flag memory phyaddr=0x%lx, size = %d\n", vdb.phys_addr, (int)vdb.size);
            vdb.virt_addr = 0;
        }
        else
        {
            vdi->vpu_dump_flag.virt_addr = vdb.virt_addr;
            vdi->vpu_dump_flag.size = vdb.size;
        }
#else
        vdi->vpu_dump_flag.virt_addr = 0;
        vdi->vpu_dump_flag.size = 0;
#endif
    }

    return (vpu_instance_pool_t *)vdi->pvip;
}

int vdi_open_instance(unsigned long core_idx, unsigned long inst_idx)
{
    vdi_info_t *vdi;
    vpudrv_inst_info_t inst_info = {0};
    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;
    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;

#ifndef BM_PCIE_MODE
    inst_info.core_idx = core_idx;
#else
#ifdef CHIP_BM1684
    inst_info.core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    inst_info.core_idx = core_idx;
#endif
#endif
    inst_info.inst_idx = inst_idx;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_OPEN_INSTANCE, &inst_info) < 0)
    {
        VLOG(ERR, "[VDI] fail to deliver open instance num inst_idx=%d\n", (int)inst_idx);
        return -1;
    }

    vdi->pvip->vpu_instance_num = inst_info.inst_open_count;

    return 0;
}

int vdi_close_instance(u64 core_idx, u64 inst_idx)
{
    vdi_info_t *vdi;
    vpudrv_inst_info_t inst_info = {0};

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;
#ifndef BM_PCIE_MODE
    inst_info.core_idx = core_idx;
#else
#ifdef CHIP_BM1684
    inst_info.core_idx = core_idx % MAX_NUM_VPU_CORE_CHIP;
#else
    inst_info.core_idx = core_idx;
#endif
#endif
    inst_info.inst_idx = inst_idx;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_CLOSE_INSTANCE, &inst_info) < 0)
    {
        VLOG(ERR, "[VDI] fail to deliver open instance num inst_idx=%d\n", (int)inst_idx);
        return -1;
    }

    vdi->pvip->vpu_instance_num = inst_info.inst_open_count;

    return 0;
}

int vdi_get_instance_num(u64 core_idx)
{
    vdi_info_t *vdi;
    int inst_num = -1;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return inst_num;
    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00) {
#ifndef BM_PCIE_MODE
        int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
        int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
        int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
        char vpu_dev_name[32] = {0};
        int vpu_fd;

        sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
        vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
        VLOG(TRACE,"[VDI] Get instance num. board %d, core %d, fd %d\n",
             board_idx, chip_core_idx, vpu_fd);
#endif
        if (vpu_fd < 0) {
            VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
            return -1;
        }
        vpudrv_inst_info_t inst_info = {0};

#if defined(BM_PCIE_MODE) && defined(CHIP_BM1684)
        inst_info.core_idx = chip_core_idx;
#else
        inst_info.core_idx = core_idx;
#endif
        if (ioctl(vpu_fd, VDI_IOCTL_GET_INSTANCE_NUM, &inst_info) < 0)
        {
            VLOG(ERR, "[VDI] fail to deliver get instance num core_idx=%d\n", (int)core_idx);
            return -1;
        }
        inst_num = inst_info.inst_open_count;
        close(vpu_fd);
    }
    else {
        inst_num = vdi->pvip->vpu_instance_num;
    }

    return inst_num;
}

int vdi_hw_reset(u64 core_idx) // DEVICE_ADDR_SW_RESET
{
    vdi_info_t *vdi;
#ifdef BM_PCIE_MODE
    int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00) {
#ifndef BM_PCIE_MODE
        int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
        char vpu_dev_name[32] = {0};
        int vpu_fd;

        sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
        vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
        VLOG(TRACE,"[VDI] HW reset. board %d, core %d, fd %d\n",
             board_idx, chip_core_idx, vpu_fd);
#endif
        if (vpu_fd < 0) {
            VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
            return -1;
        }

        if (ioctl(vpu_fd, VDI_IOCTL_RESET, &chip_core_idx) < 0)
        {
            VLOG(ERR, "[VDI] fail to deliver hw reset....\n");
        }
        close(vpu_fd);
        return 0;
    }
    else
        return ioctl(vdi->vpu_fd, VDI_IOCTL_RESET, &chip_core_idx);

}

int vdi_crst_set_status(int core_idx, int status)
{
    vdi_info_t *vdi;
#ifdef BM_PCIE_MODE
    int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];
    vpudrv_syscxt_info_t syscxt_info;
    syscxt_info.core_idx = chip_core_idx;
    syscxt_info.core_status = status;

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00) {
#ifndef BM_PCIE_MODE
        int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
        char vpu_dev_name[32] = {0};
        int vpu_fd;

        sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
        vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
        VLOG(TRACE,"[VDI] HW reset. board %d, core %d, fd %d\n",
             board_idx, chip_core_idx, vpu_fd);
#endif
        if (vpu_fd < 0) {
            VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
            return -1;
        }

        if (ioctl(vpu_fd, VDI_IOCTL_SYSCXT_SET_STATUS, &syscxt_info) < 0) {
            VLOG(ERR, "[VDI] set status error....\n");
        }
        close(vpu_fd);
        return 0;
    }
    else
        return ioctl(vdi->vpu_fd, VDI_IOCTL_SYSCXT_SET_STATUS, &syscxt_info);

}

int vdi_crst_set_enable(int core_idx, int en)
{
    vdi_info_t *vdi;
#ifdef BM_PCIE_MODE
    int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];
    vpudrv_syscxt_info_t syscxt_info;
    syscxt_info.core_idx = chip_core_idx;
    syscxt_info.fun_en = en;

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00) {
#ifndef BM_PCIE_MODE
        int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
        char vpu_dev_name[32] = {0};
        int vpu_fd;

        sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
        vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
        VLOG(TRACE,"[VDI] HW reset. board %d, core %d, fd %d\n",
             board_idx, chip_core_idx, vpu_fd);
#endif
        if (vpu_fd < 0) {
            VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
            return -1;
        }

        if (ioctl(vpu_fd, VDI_IOCTL_SYSCXT_SET_EN, &syscxt_info) < 0) {
            VLOG(ERR, "[VDI] set func error....\n");
        }
        close(vpu_fd);
        return 0;
    }
    else
        return ioctl(vdi->vpu_fd, VDI_IOCTL_SYSCXT_SET_EN, &syscxt_info);

}

int vdi_crst_get_status(int core_idx)
{
    vdi_info_t *vdi;
    int ret = 0;
#ifdef BM_PCIE_MODE
    int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];
    vpudrv_syscxt_info_t syscxt_info;
    syscxt_info.core_idx = chip_core_idx;

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00) {
#ifndef BM_PCIE_MODE
        int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
        char vpu_dev_name[32] = {0};
        int vpu_fd;

        sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
        vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
        VLOG(TRACE,"[VDI] HW reset. board %d, core %d, fd %d\n",
             board_idx, chip_core_idx, vpu_fd);
#endif
        if (vpu_fd < 0) {
            VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
            return -1;
        }

        ret = ioctl(vpu_fd, VDI_IOCTL_SYSCXT_GET_STATUS, &syscxt_info);
        close(vpu_fd);
    }
    else
        ret = ioctl(vdi->vpu_fd, VDI_IOCTL_SYSCXT_GET_STATUS, &syscxt_info);

    if (ret < 0) {
        VLOG(ERR, "[VDI] get status error....\n");
        return -1;
    }

    return syscxt_info.core_status;
}

int vdi_crst_chk_status(int core_idx, int inst_idx, int pos, int *p_is_sleep, int *p_is_block)
{
    vdi_info_t *vdi;
    int ret;
#ifdef BM_PCIE_MODE
    int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#else
    int chip_core_idx = core_idx;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];
    vpudrv_syscxt_info_t syscxt_info;
    syscxt_info.core_idx = chip_core_idx;
    syscxt_info.inst_idx = inst_idx;
    syscxt_info.pos = pos;
    syscxt_info.is_sleep = *p_is_sleep;

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00) {
#ifndef BM_PCIE_MODE
        int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
        char vpu_dev_name[32] = {0};
        int vpu_fd;

        sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
        vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
        VLOG(TRACE,"[VDI] HW reset. board %d, core %d, fd %d\n",
             board_idx, chip_core_idx, vpu_fd);
#endif
        if (vpu_fd < 0) {
            VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
            return -1;
        }

        ret = ioctl(vpu_fd, VDI_IOCTL_SYSCXT_CHK_STATUS, &syscxt_info);
        close(vpu_fd);
    }
    else
        ret = ioctl(vdi->vpu_fd, VDI_IOCTL_SYSCXT_CHK_STATUS, &syscxt_info);

    if (p_is_sleep) *p_is_sleep = syscxt_info.is_sleep;
    if (p_is_block) *p_is_block = syscxt_info.is_all_block;
    return ret;
}

#if !defined(TRY_SEM_MUTEX) && !defined(TRY_FLOCK) && !defined(TRY_ATOMIC_LOCK)
static void restore_mutex_in_dead(MUTEX_HANDLE *mutex)
{
    int mutex_value;

    if (!mutex)
        return;
#if defined(ANDROID)
    mutex_value = mutex->value;
#else
    memcpy(&mutex_value, mutex, sizeof(mutex_value));
#endif
    if (mutex_value == (int)0xdead10cc) // destroy by device driver
    {
        pthread_mutexattr_t mutexattr;
        pthread_mutexattr_init(&mutexattr);
        pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(mutex, &mutexattr);
    }
}
#endif
int vdi_lock(u64 core_idx)
{
    vdi_info_t *vdi;
#if !defined(TRY_SEM_MUTEX) && !defined(TRY_FLOCK) && !defined(TRY_ATOMIC_LOCK)
#if defined(ANDROID) || !defined(PTHREAD_MUTEX_ROBUST_NP)
#else
    const int MUTEX_TIMEOUT = 0x7fffffff;
#endif
#endif
    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;
#if defined(TRY_SEM_MUTEX)
    sem_wait((MUTEX_HANDLE *)vdi->vpu_mutex);
#elif defined(TRY_FLOCK)
    if(s_flock_fd[core_idx] == -1) {
        char flock_name[255];
        sprintf(flock_name, "%s%ld", CORE_FLOCK_NAME, core_idx);
        VLOG(INFO, "flock_name : .. %s\n", flock_name);
        s_flock_fd[core_idx] = open(flock_name, O_WRONLY|O_CREAT|O_APPEND, 0666);
    }
    flock(s_flock_fd[core_idx], LOCK_EX);
#elif defined(TRY_ATOMIC_LOCK)
    {
        int val2 = vdi->pid;
        //struct timespec ts = {0, 2*1000*1000};

        if (vdi->vpu_mutex == NULL)
            return 0;

        while(__sync_val_compare_and_swap((int *)vdi->vpu_mutex, 0, val2) != 0){
            //nanosleep(&ts, NULL);
            usleep(2*1000);
        }
    }
#else
#if defined(ANDROID) || !defined(PTHREAD_MUTEX_ROBUST_NP)
    restore_mutex_in_dead((MUTEX_HANDLE *)vdi->vpu_mutex);
    pthread_mutex_lock((MUTEX_HANDLE*)vdi->vpu_mutex);
#else
    if (pthread_mutex_lock((MUTEX_HANDLE *)vdi->vpu_mutex) != 0) {
        VLOG(ERR, "%s:%d failed to pthread_mutex_locK\n", __FUNCTION__, __LINE__);
        return -1;
    }
#endif
#endif

    return 0;
}

void vdi_unlock(u64 core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return;
#if defined(TRY_SEM_MUTEX)
    sem_post((MUTEX_HANDLE *)vdi->vpu_mutex);
#elif defined(TRY_FLOCK)
    if(s_flock_fd[core_idx] != -1) {
        flock(s_flock_fd[core_idx], LOCK_UN);
    }
    else {
        VLOG(ERR, "can't get flock handle ........\n");
    }
#elif defined(TRY_ATOMIC_LOCK)
    //__atomic_store_n((int *)vdi->vpu_mutex, 0, __ATOMIC_SEQ_CST);
    if (vdi->vpu_mutex)
        __sync_lock_release((int *)vdi->vpu_mutex);
#ifdef CHIP_BM1682
    if((core_idx%MAX_NUM_VPU_CORE_CHIP) != 2)
        usleep(2*1000);
        //sched_yield();
#endif
#else
    pthread_mutex_unlock((MUTEX_HANDLE *)vdi->vpu_mutex);
#endif
}

void vdi_destroy_lock(u64 core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return;
#if defined(TRY_FLOCK)
    if(s_flock_fd[core_idx] != -1) {
        close(s_flock_fd[core_idx]);
        s_flock_fd[core_idx] = -1;
    }
#endif
}

int vdi_disp_lock(u64 core_idx)
{
    vdi_info_t *vdi;
#if !defined(TRY_SEM_MUTEX) && !defined(TRY_FLOCK) && !defined(TRY_ATOMIC_LOCK)

#if defined(ANDROID) || !defined(PTHREAD_MUTEX_ROBUST_NP)
#else
    const int MUTEX_TIMEOUT = 5000;  // ms
#endif
#endif
    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;
#if defined(TRY_SEM_MUTEX)
    sem_wait((MUTEX_HANDLE*)vdi->vpu_disp_mutex);
#elif defined(TRY_FLOCK)
    if(s_disp_flock_fd[core_idx] == -1) {
        char disp_flock_name[255];
        sprintf(disp_flock_name, "%s%ld", CORE_DISP_FLOCK_NEME, core_idx);
        VLOG(INFO, "disp_flock_name : .. %s\n", disp_flock_name);
        s_disp_flock_fd[core_idx] = open(disp_flock_name, O_WRONLY|O_CREAT|O_APPEND, 0666);
    }
    flock(s_disp_flock_fd[core_idx], LOCK_EX);
#elif defined(TRY_ATOMIC_LOCK)
    {
        int val2 = vdi->pid;//getpid();
        //struct timespec ts = {0, 2*1000*1000};

        while(__sync_val_compare_and_swap((int *)vdi->vpu_disp_mutex, 0, val2) != 0){
            //nanosleep(&ts, NULL);
            usleep(2*1000);
        }
    }
#else
#if defined(ANDROID) || !defined(PTHREAD_MUTEX_ROBUST_NP)
    restore_mutex_in_dead((MUTEX_HANDLE *)vdi->vpu_disp_mutex);
    pthread_mutex_lock((MUTEX_HANDLE*)vdi->vpu_disp_mutex);
#else
    if (pthread_mutex_lock((MUTEX_HANDLE *)vdi->vpu_disp_mutex) != 0) {
        VLOG(ERR, "%s:%d failed to pthread_mutex_lock\n", __FUNCTION__, __LINE__);
        return -1;
    }

#endif /* ANDROID */
#endif

    return 0;
}

void vdi_disp_unlock(u64 core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return;
#if defined(TRY_SEM_MUTEX)
    sem_post((MUTEX_HANDLE *)vdi->vpu_disp_mutex);
#elif defined(TRY_FLOCK)
    if(s_disp_flock_fd[core_idx] != -1) {
        flock(s_disp_flock_fd[core_idx], LOCK_UN);
    }
    else {
        VLOG(ERR, "can't get disp flock handle ........\n");
    }
#elif defined(TRY_ATOMIC_LOCK)
    //__atomic_store_n((int *)vdi->vpu_disp_mutex, 0, __ATOMIC_SEQ_CST);
    __sync_lock_release((int *)vdi->vpu_disp_mutex);
#else
    pthread_mutex_unlock((MUTEX_HANDLE *)vdi->vpu_disp_mutex);
#endif
}

void vdi_write_register(u64 core_idx, u64 addr, unsigned int data)
{
    vdi_info_t *vdi;
    volatile unsigned int *reg_addr;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return;

    reg_addr = (volatile unsigned int *)(addr + (unsigned long)vdi->vdb_register.virt_addr);
    __atomic_store_n(reg_addr, data, __ATOMIC_SEQ_CST);
}

unsigned int vdi_read_register(u64 core_idx, u64 addr)
{
    vdi_info_t *vdi;
    volatile unsigned int *reg_addr;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return (unsigned int)-1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return (unsigned int)-1;

    reg_addr = (volatile unsigned int *)(addr + (unsigned long)vdi->vdb_register.virt_addr);
    return __atomic_load_n(reg_addr, __ATOMIC_SEQ_CST);
}

#define FIO_TIMEOUT         100

unsigned int vdi_fio_read_register(u64 core_idx, u64 addr)
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

void vdi_fio_write_register(u64 core_idx, u64 addr, unsigned int data)
{
    unsigned int ctrl;

    vdi_write_register(core_idx, W5_VPU_FIO_DATA, data);
    ctrl  = (addr&0xffff);
    ctrl |= (1<<16);    /* write operation */
    vdi_write_register(core_idx, W5_VPU_FIO_CTRL_ADDR, ctrl);
}


int vdi_clear_memory(u64 core_idx, u64 addr, int len, int endian)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
#ifndef BM_PCIE_MODE
    unsigned long offset;
#endif
    int i;
    Uint8*  zero;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
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
#ifndef BM_PCIE_MODE
    offset = addr - (unsigned long)vdb.phys_addr;
    osal_memcpy((void *)((unsigned long)vdb.virt_addr+offset), zero, len);
#if !defined(BM_ION_MEM) && !defined(BM_PCIE_MODE)
    if(vdb.enable_cache != 0) {
        vdb.phys_addr = addr;
        vdb.size = len;
        if (ioctl(vdi->vpu_fd, VDI_IOCTL_FLUSH_DCACHE, &vdb) < 0)
        {
            VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
        }
    }
#endif
#else
    vpu_video_mem_op_t vmem_op;
    vmem_op.dst = addr;
    vmem_op.src = (unsigned long)zero;
    vmem_op.size =  len;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_WRITE_VMEM, &vmem_op) < 0)
    {
        VLOG(ERR, "[VDI] fail to clear video mem addr %lx size=%d\n", vmem_op.dst, vmem_op.size);
        return -1;
    }
#endif

    osal_free(zero);

    return len;
}

int vdi_write_memory(u64 core_idx, u64 dst_addr, unsigned char *src_data, int len, int endian)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
#ifndef BM_PCIE_MODE
    unsigned long offset;
#endif
    int i;
    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    if (!src_data) {
        VLOG(ERR, "[%s,%d] Error! src_data is NULL!\n", __func__, __LINE__);
        return -1;
    }
    if (dst_addr == (unsigned long)-1) {
        VLOG(ERR, "[%s,%d] Error! dst_addr=-1!\n", __func__, __LINE__);
    }

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 1)
        {
            vdb = vdi->vpu_buffer_pool[i].vdb;
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
    offset = dst_addr - (unsigned long)vdb.phys_addr;
#endif
    swap_endian(core_idx, src_data, len, endian);
#ifndef BM_PCIE_MODE
    osal_memcpy((void *)((unsigned long)vdb.virt_addr+offset), src_data, len);
#if !defined(BM_ION_MEM) && !defined(BM_PCIE_MODE)
    if(vdb.enable_cache != 0) {
        vdb.phys_addr = dst_addr;
        vdb.size = len;
        if (ioctl(vdi->vpu_fd, VDI_IOCTL_FLUSH_DCACHE, &vdb) < 0)
        {
            VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
            return -1;
        }
    }
#endif

#else
    vpu_video_mem_op_t vmem_op;
    vmem_op.dst = dst_addr;
    vmem_op.src = (unsigned long)src_data;
    vmem_op.size =  len;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_WRITE_VMEM, &vmem_op) < 0)
    {
        VLOG(ERR, "[VDI] fail to write video mem addr %lx size=%d\n", vmem_op.dst, vmem_op.size);
        return -1;
    }
#endif

    return len;
}

int vdi_read_memory(u64 core_idx, u64 src_addr, unsigned char *dst_data, int len, int endian)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
#ifndef BM_PCIE_MODE
    unsigned long offset;
#endif
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].inuse == 1)
        {
            vdb = vdi->vpu_buffer_pool[i].vdb;
            if (src_addr >= vdb.phys_addr && (src_addr+len) <= (vdb.phys_addr + vdb.size))
                break;
        }
    }

    if (!vdb.size)
        return -1;

#ifndef BM_PCIE_MODE
    offset = src_addr - (unsigned long)vdb.phys_addr;
#if !defined(BM_ION_MEM) && !defined(BM_PCIE_MODE)
    if(vdb.enable_cache != 0) {
        vdb.phys_addr = src_addr;
        vdb.size = len;
        if (ioctl(vdi->vpu_fd, VDI_IOCTL_INVALIDATE_DCACHE, &vdb) < 0)
        {
            VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
            return -1;
        }
    }
#endif
    osal_memcpy(dst_data, (const void *)((unsigned long)vdb.virt_addr+offset), len);
#else
    vpu_video_mem_op_t vmem_op;
    vmem_op.src = src_addr;
    vmem_op.dst = (unsigned long)dst_data;
    vmem_op.size =  len;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_READ_VMEM, &vmem_op) < 0)
    {
        VLOG(ERR, "[VDI] fail to read video mem %lx size=%d\n", vmem_op.src, vmem_op.size);
        return -1;
    }
#endif

    swap_endian(core_idx, dst_data, len,  endian);

    return len;
}

int vdi_allocate_dma_memory(u64 core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

#if defined(BM_PCIE_MODE)
    vdb.core_idx = chip_core_idx;
#endif

    vdb.size = vb->size;
#if !defined(BM_PCIE_MODE)
    vdb.enable_cache = vb->enable_cache;
#endif
#ifdef BM_ION_MEM
    {
        // int flag = BM_ION_FLAG_WRITECOMBINE;
        // if(vb->enable_cache == 1)
        //     flag = BM_ION_FLAG_CACHED;
        // bm_ion_buffer_t* p_ion_buf = bm_ion_allocate_buffer(0, vdb.size, (BM_ION_FLAG_VPU << 4) | flag);
        // if (p_ion_buf == NULL)
        // {
        //     VLOG(ERR, "[VDI] fail to vdi_allocate_dma_memory size=%d\n", vdb.size);
        //     return -1;
        // }
        // if (bm_ion_map_buffer(p_ion_buf, BM_ION_MAPPING_FLAG_READ | BM_ION_MAPPING_FLAG_WRITE) != 0)
        // {
        //     VLOG(ERR, "ion map failed.\n");
        //     return -1;
        // }

        // vdb.base = (unsigned long)p_ion_buf;
        // vdb.phys_addr = p_ion_buf->paddr;
        // vdb.virt_addr = (unsigned long)p_ion_buf->vaddr;
    }
#else
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY, &vdb) < 0)
    {
        VLOG(ERR, "[VDI] fail to vdi_allocate_dma_memory size=%d\n", vdb.size);
        return -1;
    }

#ifndef BM_PCIE_MODE
    /* map to virtual address */
    vdb.virt_addr = (unsigned long)mmap(NULL, vdb.size, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, vdi->vpu_fd, vdb.phys_addr);
    if ((void *)vdb.virt_addr == MAP_FAILED)
    {
        memset(vb, 0x00, sizeof(vpu_buffer_t));
        return -1;
    }
#else
    vdb.virt_addr = FAKE_PCIE_VIRT_ADDR;
    vdi_clear_memory(core_idx, vdb.phys_addr, vdb.size, VDI_SYSTEM_ENDIAN);
#endif
#endif

    vb->base      = vdb.base;
    vb->phys_addr = vdb.phys_addr;
    vb->virt_addr = vdb.virt_addr;

    pthread_mutex_lock((pthread_mutex_t *)vdi->vpu_omx_mutex);
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
    pthread_mutex_unlock((pthread_mutex_t *)vdi->vpu_omx_mutex);
    VLOG(INFO, "[VDI] vdi_allocate_dma_memory, physaddr=0x%lx, virtaddr=0x%lx~0x%lx, size=%d\n",
         vb->phys_addr, vb->virt_addr, vb->virt_addr + vb->size, vb->size);
    return 0;
}

void vdi_free_dma_memory(u64 core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;
    int i;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vb || !vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return;

    if (vb->size == 0)
        return ;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));
    pthread_mutex_lock((pthread_mutex_t *)vdi->vpu_omx_mutex);
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
    pthread_mutex_unlock((pthread_mutex_t *)vdi->vpu_omx_mutex);
    if (!vdb.size)
    {
        VLOG(ERR, "[VDI] invalid buffer to free address = 0x%lx\n", vdb.virt_addr);
        return ;
    }

    osal_memset(vb, 0, sizeof(vpu_buffer_t));
}

u64 vdi_get_dma_memory_free_size(u64 coreIdx)
{
    unsigned long size = (unsigned long)-1;
#ifndef BM_ION_MEM
    vdi_info_t *vdi;
    vdi = &s_vdi_info[coreIdx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
    {
#ifndef BM_PCIE_MODE
        int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
#else
        int board_idx     = coreIdx/MAX_NUM_VPU_CORE_CHIP;
        int chip_core_idx = coreIdx%MAX_NUM_VPU_CORE_CHIP;
        char vpu_dev_name[32] = {0};
        int vpu_fd;

        sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
        vpu_fd = open(vpu_dev_name, O_RDWR);    // if this API supports VPU parallel processing using multi VPU. the driver should be made to open multiple times.
        VLOG(TRACE,"[VDI] Get dma memory free size. board %d, core %d, fd %d\n",
             board_idx, chip_core_idx, vpu_fd);
#endif
        if (vpu_fd < 0) {
            VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
            return -1;
        }
        if (ioctl(vpu_fd, VDI_IOCTL_GET_FREE_MEM_SIZE, &size) < 0)
        {
            VLOG(ERR, "[VDI] fail VDI_IOCTL_GET_FREE_MEM_SIZE size=%ld\n", size);
            return 0;
        }
        close(vpu_fd);
    }
    else {
        if (ioctl(vdi->vpu_fd, VDI_IOCTL_GET_FREE_MEM_SIZE, &size) < 0) {
            VLOG(ERR, "[VDI] fail VDI_IOCTL_GET_FREE_MEM_SIZE size=%ld\n", size);
            return 0;
        }
    }
#endif

    return size;
}

int vdi_attach_dma_memory(u64 core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;
    int i;
    vpudrv_buffer_t vdb;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    vdb.size = vb->size;
    vdb.phys_addr = vb->phys_addr;
    vdb.base = vb->base;

    vdb.virt_addr = vb->virt_addr;
    pthread_mutex_lock((pthread_mutex_t *)vdi->vpu_omx_mutex);
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
    pthread_mutex_unlock((pthread_mutex_t *)vdi->vpu_omx_mutex);

#if 0
    VLOG(INFO, "[VDI] %s  physaddr=0x%lx, virtaddr=0x%lx, size=%d, index=%d\n",
         __func__, vb->phys_addr, vb->virt_addr, vb->size, i);
#endif

    return 0;
}

int vdi_dettach_dma_memory(u64 core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi;
    int i;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vb || !vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    if (vb->size == 0)
        return -1;
    pthread_mutex_lock((pthread_mutex_t *)vdi->vpu_omx_mutex);
    for (i=0; i<MAX_VPU_BUFFER_POOL; i++)
    {
        if (vdi->vpu_buffer_pool[i].vdb.phys_addr == vb->phys_addr)
        {
            vdi->vpu_buffer_pool[i].inuse = 0;
            vdi->vpu_buffer_pool_count--;
            break;
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)vdi->vpu_omx_mutex);
    return 0;
}

int vdi_get_sram_memory(u64 core_idx, vpu_buffer_t *vb)
{
    vdi_info_t *vdi = NULL;
    vpudrv_buffer_t vdb;
    unsigned int sram_size = 0;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vb || !vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;


    osal_memset(&vdb, 0x00, sizeof(vpudrv_buffer_t));

    switch (vdi->product_code) {
    case WAVE510_CODE:
        sram_size = VDI_WAVE510_SRAM_SIZE; break;
    case BODA950_CODE:
    case CODA960_CODE:
    case CODA980_CODE:
        sram_size = VDI_CODA9_SRAM_SIZE; break;
    case WAVE511_CODE:
        sram_size = VDI_WAVE511_SRAM_SIZE;
        if(vdi->chip_id == 0x1686)
            sram_size = VDI_WAVE511_SRAM_SIZE_1684X;
        break;
    case WAVE512_CODE:
        sram_size = VDI_WAVE512_SRAM_SIZE; break;
    case WAVE520_CODE:
        sram_size = VDI_WAVE520_SRAM_SIZE; break;
    case WAVE525_CODE:
        sram_size = VDI_WAVE525_SRAM_SIZE; break;
    case WAVE515_CODE:
        sram_size = VDI_WAVE515_SRAM_SIZE; break;
    case WAVE521_CODE:
        sram_size = VDI_WAVE521_SRAM_SIZE; break;
    case WAVE521C_CODE:
        sram_size = VDI_WAVE521C_SRAM_SIZE; break;
    default:
        VLOG(ERR, "[VDI] check product_code(%x)\n", vdi->product_code);
        break;
    }

    if (sram_size > 0)    // if we can know the sram address directly in vdi layer, we use it first for sdram address
    {
#ifndef BM_PCIE_MODE
        vb->phys_addr = VDI_SRAM_BASE_ADDR+((core_idx%2)*sram_size);    // HOST can set DRAM base addr to VDI_SRAM_BASE_ADDR.
#else
        vb->phys_addr = VDI_SRAM_BASE_ADDR+((chip_core_idx%2)*sram_size);    // HOST can set DRAM base addr to VDI_SRAM_BASE_ADDR.
#endif
        if(vdi->chip_id == 0x1686)
            vb->phys_addr= VDI_SRAM_BASE_ADDR;

        vb->size = sram_size;

        return 0;
    }

    return 0;
}

int vdi_set_clock_gate(u64 core_idx, int enable)
{
    vdi_info_t *vdi = NULL;
    int ret;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;
    vdi = &s_vdi_info[core_idx];
    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    if (vdi->product_code == WAVE510_CODE || vdi->product_code == WAVE512_CODE || vdi->product_code == WAVE520_CODE || vdi->product_code == WAVE515_CODE ||
        vdi->product_code == WAVE525_CODE || vdi->product_code == WAVE521_CODE || vdi->product_code == WAVE521C_CODE || vdi->product_code == WAVE511_CODE ) {
        return 0;
    }
    vdi->clock_state = enable;
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_SET_CLOCK_GATE, &enable);

    return ret;
}

int vdi_get_clock_gate(unsigned long core_idx)
{
    vdi_info_t *vdi;
    int ret;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

    ret = vdi->clock_state;
    return ret;
}

int vdi_wait_bus_busy(u64 core_idx, int timeout, unsigned int gdi_busy_flag)
{
    Uint64 elapse, cur;
    vdi_info_t *vdi;
    vdi = &s_vdi_info[core_idx];

    elapse = osal_gettime();

    while(1)
    {
        if (vdi->product_code == WAVE520_CODE || vdi->product_code == WAVE525_CODE || vdi->product_code == WAVE521_CODE || vdi->product_code == WAVE521C_CODE || vdi->product_code == WAVE511_CODE ) {
            if (vdi_fio_read_register(core_idx, gdi_busy_flag) == 0x3f) break;
        }
        else if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
            if (vdi_fio_read_register(core_idx, gdi_busy_flag) == 0x738) break;
        }
        else if (PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
            if (vdi_read_register(core_idx, gdi_busy_flag) == 0x77) break;
        }
        else {
            VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
            return -1;
        }

        if (timeout > 0) {
            cur = osal_gettime();

            if ((cur - elapse) > timeout) {
                VLOG(ERR, "[VDI] vdi_wait_bus_busy timeout, PC=0x%x\n", vdi_read_register(core_idx, 0x018));
                return -1;
            }
        }
    }
    return 0;

}

int vdi_wait_vpu_busy(u64 core_idx, int timeout, unsigned int addr_bit_busy_flag)
{
    Uint64 elapse, cur;
    Uint32 pc;
    Uint32 normalReg = TRUE;
    vdi_info_t *vdi;
    vdi = &s_vdi_info[core_idx];

    elapse = osal_gettime();

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        pc = W5_VCPU_CUR_PC;
        if (addr_bit_busy_flag&0x8000) normalReg = FALSE;
    }
    else if (PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
        pc = BIT_CUR_PC;
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }

    while(1)
    {
        if (normalReg == TRUE) {
            if (vdi_read_register(core_idx, addr_bit_busy_flag) == 0) break;
        }
        else {
            if (vdi_fio_read_register(core_idx, addr_bit_busy_flag) == 0) break;
        }

        if(getenv("DECREASE_CPU_LOADING")!=NULL&&strcmp(getenv("DECREASE_CPU_LOADING"),"1")==0)
            usleep(10);
        if (timeout > 0) {
            cur = osal_gettime();

            if ((cur - elapse) > timeout) {
                Uint32 idx;
                for (idx=0; idx<50; idx++) {
                    VLOG(ERR, "[VDI] vdi_wait_vpu_busy timeout, PC=0x%x, LR=0x%x\n", vdi_read_register(core_idx, pc), vdi_read_register(core_idx, W5_VCPU_CUR_LR));
                    VLOG(ERR, "[VDI] W5_VPU_BUSY_STATUS=0x%x, W5_VPU_HALT_STATUS=0x%x, W5_VPU_VCPU_STATUS=0x%x, W5_VPU_PRESCAN_STATUS=0x%x\n", vdi_read_register(core_idx, W5_VPU_BUSY_STATUS), 
                        vdi_read_register(core_idx, W5_VPU_HALT_STATUS), vdi_read_register(core_idx, W5_VPU_VCPU_STATUS), vdi_read_register(core_idx, W5_VPU_PRESCAN_STATUS));
                    {
                        Uint32 vcpu_reg[31]= {0,};
                        int i;
                        // --------- VCPU register Dump
                        VLOG(ERR, "[+] VCPU REG Dump\n");
                        for (i = 0; i < 25; i++)
                        {
                            vdi_write_register(core_idx, 0x14, (1 << 9) | (i & 0xff));
                            vcpu_reg[i] = vdi_read_register(core_idx, 0x1c);

                            if (i < 16)
                            {
                                VLOG(INFO, "0x%08x\t", vcpu_reg[i]);
                                if ((i % 4) == 3)
                                    VLOG(ERR, "\n");
                            }
                            else
                            {
                                switch (i)
                                {
                                case 16:
                                    VLOG(ERR, "CR0: 0x%08x\t", vcpu_reg[i]);
                                    break;
                                case 17:
                                    VLOG(ERR, "CR1: 0x%08x\n", vcpu_reg[i]);
                                    break;
                                case 18:
                                    VLOG(ERR, "ML:  0x%08x\t", vcpu_reg[i]);
                                    break;
                                case 19:
                                    VLOG(ERR, "MH:  0x%08x\n", vcpu_reg[i]);
                                    break;
                                case 21:
                                    VLOG(ERR, "LR:  0x%08x\n", vcpu_reg[i]);
                                    break;
                                case 22:
                                    VLOG(ERR, "PC:  0x%08x\n", vcpu_reg[i]);
                                    break;
                                case 23:
                                    VLOG(ERR, "SR:  0x%08x\n", vcpu_reg[i]);
                                    break;
                                case 24:
                                    VLOG(ERR, "SSP: 0x%08x\n", vcpu_reg[i]);
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                    }
                }
                return -1;
            }
        }
    }
    return 0;

}

int vdi_wait_vcpu_bus_busy(u64 core_idx, int timeout, unsigned int addr_bit_busy_flag)
{
    Uint64 elapse, cur;
    Uint32 pc;
    Uint32 normalReg = TRUE;
    vdi_info_t *vdi;
    vdi = &s_vdi_info[core_idx];

    elapse = osal_gettime();

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        pc = W5_VCPU_CUR_PC;
        if (addr_bit_busy_flag&0x8000) normalReg = FALSE;
    }
    else if (PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
        pc = BIT_CUR_PC;
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }

    while(1)
    {
        if (normalReg == TRUE) {
            if (vdi_read_register(core_idx, addr_bit_busy_flag) == 0x40) break;
        }
        else {
            if (vdi_fio_read_register(core_idx, addr_bit_busy_flag) == 0x40) break;
        }

        if (timeout > 0) {
            cur = osal_gettime();

            if ((cur - elapse) > timeout) {
                Uint32 idx;
                for (idx=0; idx<50; idx++) {
                    VLOG(ERR, "[VDI] vdi_wait_vcpu_bus_busy timeout, PC=0x%x\n", vdi_read_register(core_idx, pc));
                }
                return -1;
            }
        }
    }
    return 0;
}
#ifdef SUPPORT_MULTI_INST_INTR
int vdi_wait_interrupt(u64 coreIdx, unsigned int instIdx, int timeout)
#else
int vdi_wait_interrupt(u64 coreIdx, int timeout)
#endif
{
    vdi_info_t *vdi;
    int intr_reason = 0;
#ifdef SUPPORT_INTERRUPT
    int ret;
#else
    struct timeval  tv = {0};
    Uint64 startTime, endTime;
#endif

    /* pcie mode for BM1682, INTR is disabled */

    /* soc mode or bm1684 pcie mode */
    vpudrv_intr_info_t intr_info;

    if (coreIdx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[coreIdx];
    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;

#ifdef SUPPORT_INTERRUPT
    intr_info.timeout     = timeout;
    intr_info.intr_reason = 0;
#ifdef SUPPORT_MULTI_INST_INTR
    intr_info.intr_inst_index = instIdx;
#endif
#ifndef BM_PCIE_MODE
    intr_info.core_idx = coreIdx;
#else
#ifdef CHIP_BM1684
    intr_info.core_idx = coreIdx%MAX_NUM_VPU_CORE_CHIP;
#else
    intr_info.core_idx = coreIdx;
#endif
#endif
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_WAIT_INTERRUPT, (void*)&intr_info);
    if (ret != 0) {
        if (errno == ETIME)
            return -1;
        else
            return -2;
    }

    intr_reason = intr_info.intr_reason;
#else
    Uint32          int_sts_reg;
    Uint32          int_reason_reg;
    Uint32          int_clear_reg;

    UNREFERENCED_PARAMETER(intr_info);

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        int_sts_reg = W5_VPU_VPU_INT_STS;
        int_reason_reg = W5_VPU_VINT_REASON;
        int_clear_reg = W5_VPU_VINT_CLEAR;
    }
    else if (PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
        int_sts_reg = BIT_INT_STS;
        int_reason_reg = BIT_INT_REASON;
        int_clear_reg = BIT_INT_CLEAR;
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }

    gettimeofday(&tv, NULL);
    startTime = tv.tv_sec*1000 + tv.tv_usec/1000;
    while (TRUE) {
        if (vdi_read_register(coreIdx, int_sts_reg)) {
            if ((intr_reason=vdi_read_register(coreIdx, int_reason_reg))) {
                if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
                    vdi_write_register(coreIdx, W5_VPU_VINT_REASON_CLR, intr_reason);
                }
                vdi_write_register(coreIdx, int_clear_reg, 0x1);
                break;
            }
        }
        gettimeofday(&tv, NULL);
        endTime = tv.tv_sec*1000 + tv.tv_usec/1000;
        if (timeout > 0 && (endTime-startTime) >= timeout) {
            return -1;
        }
    }
#endif

    return intr_reason;
}





//------------------------------------------------------------------------------
// LOG & ENDIAN functions
//------------------------------------------------------------------------------

int vdi_get_system_endian(u64 core_idx)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        return VDI_128BIT_BUS_SYSTEM_ENDIAN;
    }
    else if(PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
        return VDI_SYSTEM_ENDIAN;
    }
    else {
        VLOG(ERR, "Unknown product id : %08x\n", vdi->product_code);
        return -1;
    }
}

int vdi_convert_endian(u64 core_idx, unsigned int endian)
{
    vdi_info_t *vdi;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || !vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        switch (endian) {
        case VDI_LITTLE_ENDIAN:       endian = 0x00; break;
        case VDI_BIG_ENDIAN:          endian = 0x0f; break;
        case VDI_32BIT_LITTLE_ENDIAN: endian = 0x04; break;
        case VDI_32BIT_BIG_ENDIAN:    endian = 0x03; break;
        }
    }
    else if(PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
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


static int swap_endian(u64 core_idx, unsigned char *data, int len, int endian)
{
    vdi_info_t *vdi;
    int changes;
    int sys_endian;
    BOOL byteChange, wordChange, dwordChange, lwordChange;

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;

    if (PRODUCT_CODE_W_SERIES(vdi->product_code)) {
        sys_endian = VDI_128BIT_BUS_SYSTEM_ENDIAN;
    }
    else if(PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
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
    else if (PRODUCT_CODE_NOT_W_SERIES(vdi->product_code)) {
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


unsigned int vdi_get_ddr_map(u64 core_idx)
{
    vdi_info_t *vdi;
    unsigned int ret;
    vpudrv_regrw_info_t reg_info;
    unsigned int bit_shift = DDR_MAP_BIT_SHIFT;

#ifdef SUPPORT_MULTI_CORE_IN_ONE_DRIVER
//    core_idx = 0;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

#if defined(CHIP_BM1684)
    if(core_idx == 4)
        bit_shift = 24;
    else if((core_idx == 2) || (core_idx == 3))
        bit_shift = DDR_MAP_BIT_SHIFT_1;
    else
        bit_shift = DDR_MAP_BIT_SHIFT;
#else
    bit_shift = DDR_MAP_BIT_SHIFT;
#endif

    vdi = &s_vdi_info[core_idx];
    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return -1;

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
#ifdef CHIP_BM1684
    ret = 4;
#else
    ret = 2;
#endif
#endif

    return ret;
}

u64 vdi_get_virt_addr(u64 core_idx, u64 addr)
{
    vdi_info_t *vdi;
    vpudrv_buffer_t vdb;
    unsigned long offset;
    int i;

#ifdef SUPPORT_MULTI_CORE_IN_ONE_DRIVER
//    core_idx = 0;
#endif

    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
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

//    VLOG(INFO, "addr: %llx, phys: %llx, virt_addr:%llx\n", addr, vdb.phys_addr, vdb.virt_addr);
    offset = addr - (unsigned long)vdb.phys_addr;

    return vdb.virt_addr+offset;
}

void vdi_invalidate_memory(u64 core_idx, vpu_buffer_t *vb)
{
#if !defined(BM_PCIE_MODE) && !defined(BM_ION_MEM)
    vpudrv_buffer_t vdb = {0};
#endif
    vdi_info_t *vdi;
//    int i;
    if (core_idx >= MAX_NUM_VPU_CORE)
        return;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd==-1 || vdi->vpu_fd == 0x00)
        return;

#if defined(BM_ION_MEM)
    // if (!vb->size) {
    //     VLOG(ERR, "address 0x%08x is not mapped address!!!\n", (int)vb->phys_addr);
    // }
    // else
    // {
    //     bm_ion_buffer_t* p_ion_buf = (bm_ion_buffer_t *)vb->base;
    //     if(p_ion_buf != NULL)
    //     {
    //         if(vb->enable_cache == 1)
    //             bm_ion_invalidate_buffer(p_ion_buf);
    //     }
    //     else
    //     {
    //         VLOG(ERR, "invalid ion buffer addr!\n");
    //     }
    // }
#elif !defined(BM_PCIE_MODE)
    vdb.phys_addr = vb->phys_addr;
    vdb.size = vb->size;
    if (ioctl(vdi->vpu_fd, VDI_IOCTL_INVALIDATE_DCACHE, &vdb) < 0)
    {
        VLOG(ERR, "[VDI] fail to fluch dcache mem addr 0x%lx size=%d\n", vdb.phys_addr, vdb.size);
    }
#endif
}

int vdi_get_firmware_status(unsigned int core_idx)
{
    vdi_info_t *vdi;
#if defined(BM_PCIE_MODE)
    int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;
#endif
    int ret = -1;

    VLOG(INFO, "get firmware status....\n");
    if (core_idx >= MAX_NUM_VPU_CORE)
        return -1;

    vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;
    VLOG(INFO, "check firmware status from driver....\n");
#ifndef BM_PCIE_MODE
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_GET_FIRMWARE_STATUS, &core_idx);
#else
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_GET_FIRMWARE_STATUS, &chip_core_idx);
#endif
    if (ret < 0)
    {
        VLOG(ERR, "[VDI] fail to get firmware status core_idx=%d\n", (int)core_idx);
        return -1;
    }

    return (ret == 100) ? 0: 1;
}

int vdi_get_in_num(unsigned int core_idx, unsigned int inst_idx)
{
    int *p_addr = NULL;
    vdi_info_t *vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;
    p_addr = (int *)(vdi->vpu_dump_flag.virt_addr);
    if(p_addr != NULL) {
        p_addr += (core_idx * MAX_NUM_INSTANCE + inst_idx) * 2;
        return *p_addr;
    }
    return -1;
}

int vdi_get_out_num(unsigned int core_idx, unsigned int inst_idx)
{
    int *p_addr = NULL;
    vdi_info_t *vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return -1;
    p_addr = (int *)(vdi->vpu_dump_flag.virt_addr);
    if(p_addr != NULL) {
        p_addr += (core_idx * MAX_NUM_INSTANCE + inst_idx) * 2 + 1;
        return *p_addr;
    }
    return -1;
}

int *vdi_get_exception_info(unsigned int core_idx)
{
    int *p_addr = NULL;
    vdi_info_t *vdi = &s_vdi_info[core_idx];

    if(!vdi || vdi->vpu_fd == -1 || vdi->vpu_fd == 0x00)
        return p_addr;
    p_addr = (int *)(vdi->vpu_dump_flag.virt_addr);
    if(p_addr != NULL) {
        p_addr += core_idx * (MAX_NUM_INSTANCE + 2) + MAX_VPU_CORE_NUM * MAX_NUM_INSTANCE * 2;
        return p_addr;
    }
    return NULL;
}

int vdi_get_video_cap(int core_idx,int *video_cap, int *bin_type, int *max_core_num, int *chip_id)
{
#ifndef BM_PCIE_MODE
    int vpu_fd = open(VPU_DEVICE_NAME, O_RDWR);
#else
    int board_idx     = core_idx/MAX_NUM_VPU_CORE_CHIP;
    // int chip_core_idx = core_idx%MAX_NUM_VPU_CORE_CHIP;

    char vpu_dev_name[32] = {0};
    sprintf(vpu_dev_name,"%s%d", VPU_DEVICE_NAME, board_idx);
    if(access(vpu_dev_name,F_OK) == -1) {
        return -2;
    }
    int vpu_fd = open(vpu_dev_name, O_RDWR);

#endif

    if (vpu_fd < 0) {
        VLOG(ERR, "[VDI] Can't open vpu driver. [error=%s]. try to run vdi/linux/driver/load.sh script \n", strerror(errno));
        return -1;
    }
    int tmp_chip_id   = 0;
    int tmp_core_num  = 0;
#ifndef BM_PCIE_MODE
    int tmp_video_cap = 0;
    int tmp_bin_type  = 0;
    if (ioctl(vpu_fd, VDI_IOCTL_GET_VIDEO_CAP, &tmp_video_cap) < 0){
        close(vpu_fd);
        return -1;
    }
    *video_cap = tmp_video_cap;
    if (ioctl(vpu_fd, VDI_IOCTL_GET_BIN_TYPE, &tmp_bin_type) < 0){
        close(vpu_fd);
        return -1;
    }
    *bin_type = tmp_bin_type;
#endif
    if (ioctl(vpu_fd, VDI_IOCTL_GET_CHIP_ID, &tmp_chip_id) < 0){
        close(vpu_fd);
        return -1;
    }
    *chip_id = tmp_chip_id;

    if (ioctl(vpu_fd, VDI_IOCTL_GET_MAX_CORE_NUM, &tmp_core_num) < 0){
        close(vpu_fd);
        return -1;
    }
    *max_core_num = tmp_core_num;

    close(vpu_fd);
    return 0;
}

int bm_vdi_memcpy_s2d(bm_handle_t bm_handle, int coreIdx,bm_device_mem_t dst, void *src,int endian)
{
    swap_endian(coreIdx, src,dst.size,endian);
    if(bm_memcpy_s2d(bm_handle, dst,src)!=BM_SUCCESS)
        {
            VLOG(ERR,"system to device memcpy failed!");
            return BM_ERR_FAILURE;
        }
        return BM_SUCCESS;
}
int bm_vdi_memcpy_d2s(bm_handle_t bm_handle, int coreIdx, void *dst,bm_device_mem_t src,int endian)
{
    if(bm_memcpy_d2s(bm_handle, dst,src)!=BM_SUCCESS)
        {
            VLOG(ERR,"system to device memcpy failed!");
            return BM_ERR_FAILURE;
        }
    swap_endian(coreIdx, dst,src.size,endian);
    return BM_SUCCESS;
}
void bm_free_mem(bm_handle_t dev_handle,bm_device_mem_t dev_mem,unsigned long long  vaddr)
{
#ifndef BM_PCIE_MODE
    if(vaddr!=0x00)//unmap memory if not the fake vaddr
        bm_mem_unmap_device_mem(dev_handle,(void *)vaddr,dev_mem.size);
#endif
    bm_free_device(dev_handle,dev_mem);

}

int  bm_vdi_mmap(bm_handle_t bm_handle, bm_device_mem_t *dmem,unsigned long long *vmem)
{
#ifndef BM_PCIE_MODE
    if( bm_mem_mmap_device_mem_no_cache(bm_handle,dmem,vmem)!=BM_SUCCESS)
        {
            VLOG(ERR, "map error !!");
            return BM_ERR_FAILURE;
        }
#else
    *vmem=0xDEADBEEFl;
#endif
    return BM_SUCCESS;
}

int vdi_resume_kernel_reset(u64 coreIdx){
    int ret = 0;
    vdi_info_t *vdi;
    int chip_core_idx = coreIdx;
    if (coreIdx >= MAX_NUM_VPU_CORE){
        VLOG(ERR, "coreIdx is invalid, decoder fail to resume vpu_reset in kernel\n");
        return -1;
    }

    vdi = &s_vdi_info[coreIdx];
    if(!vdi || vdi->vpu_fd < 0 || vdi->vpu_fd == 0x00){
        VLOG(ERR, "vpu_fd is invalid, decoder fail to resume vpu_reset in kernel\n");
        return -1;
    }

#if defined(BM_PCIE_MODE)
    chip_core_idx = coreIdx%MAX_NUM_VPU_CORE_CHIP;
#endif
    vdi->reset_core_flag.reset_core_disable = 0;
    vdi->reset_core_flag.core_idx = chip_core_idx;
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_CTRL_KERNEL_RESET, &(vdi->reset_core_flag));
    if (ret < 0) {
        VLOG(ERR, "decoder fail to resume vpu_reset with ioctl()\n");
        return -1;
    }

    return 0;
}

int vdi_disable_kernel_reset(u64 coreIdx){
    int ret = 0;
    vdi_info_t *vdi;
    int chip_core_idx = coreIdx;
    if (coreIdx >= MAX_NUM_VPU_CORE){
        VLOG(ERR, "coreIdx is invalid, decoder fail to disable vpu_reset in kernel\n");
        return -1;
    }

    vdi = &s_vdi_info[coreIdx];
    if(!vdi || vdi->vpu_fd < 0 || vdi->vpu_fd == 0x00){
        VLOG(ERR, "vpu_fd is invalid, decoder fail to disable vpu_reset in kernel\n");
        return -1;
    }

#if defined(BM_PCIE_MODE)
    chip_core_idx = coreIdx%MAX_NUM_VPU_CORE_CHIP;
#endif
    vdi->reset_core_flag.reset_core_disable = vdi->pid;
    vdi->reset_core_flag.core_idx = chip_core_idx;
    ret = ioctl(vdi->vpu_fd, VDI_IOCTL_CTRL_KERNEL_RESET, &(vdi->reset_core_flag));
    if (ret < 0) {
        VLOG(ERR, "decoder fail to disable vpu_reset with ioctl()\n");
        return -1;
    }
    return 0;
}


#endif    //#if defined(linux) || defined(__linux) || defined(ANDROID)

