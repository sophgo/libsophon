#if defined(linux) || defined(__linux) || defined(ANDROID) || defined  (_WIN32)

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef  _WIN32
#include <windows.h>
#include <Initguid.h>
#include <SetupAPI.h>
#pragma comment(lib, "SetupAPI.Lib")
#else
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>       /* mmap */
#include <sys/ioctl.h>      /* fopen/fread */
#include <sys/errno.h>      /* fopen/fread */
#include <sys/time.h>
#include <termios.h>
#endif

#ifdef  _KERNEL_
#include <linux/delay.h>
#endif
#include <signal.h>     /* SIGIO */
#include <fcntl.h>      /* fcntl */

#include <sys/types.h>

#include <errno.h>
#include <time.h> /* nanosleep */
#include "driver/jpu.h"
#include "../jdi.h"
#include "../../include/jpulog.h"
#include "../../jpuapi/jpuapifunc.h"


#define JPU_BIT_REG_SIZE        0x300
#define JPU_BIT_REG_BASE        (0x50470000)

#ifdef BM_PCIE_MODE
#define JDI_DRAM_PHYSICAL_BASE  0x450000000
#else
#define JDI_DRAM_PHYSICAL_BASE  0x1F800000
#endif

#define JDI_DRAM_PHYSICAL_SIZE  (128*1024*1024)
#define SUPPORT_INTERRUPT

#define JDI_SYSTEM_ENDIAN   JDI_LITTLE_ENDIAN

#ifdef BM_PCIE_MODE
#define JPU_DEVICE_NAME "/dev/bm-sophon"
#else
 #define JPU_DEVICE_NAME "/dev/jpu"
#endif
#define TRY_OPEN_SEM_TIMEOUT 3

typedef struct jpu_buffer_pool_t {
    jpudrv_buffer_t jdb;
    int inuse;
} jpu_buffer_pool_t;

#ifdef  _WIN32
HANDLE s_jpu_fd[MAX_NUM_DEV] = {0};
#else
int  s_jpu_fd[MAX_NUM_DEV] = {0};
#endif
static int  s_task_num[MAX_NUM_DEV];
static  jpudrv_buffer_t s_jdb_register[MAX_NUM_DEV][MAX_NUM_JPU_CORE] = {0};
static  jpudrv_buffer_t s_jpu_dump_flag = { 0 } ;

#ifdef  _WIN32
static __declspec(thread) Uint32 s_jpu_core_idx = 0;
static __declspec(thread) int s_jpu_device_idx = -1;
static __declspec(thread) int s_clock_state = -1;
#else
static __thread Uint32 s_jpu_core_idx = 0;
static __thread int s_jpu_device_idx = -1;
static __thread int s_clock_state = -1;
#endif

#ifdef  _WIN32
HANDLE  s_jpu_mutex = NULL;
#else
typedef pthread_mutex_t MUTEX_HANDLE;
MUTEX_HANDLE s_jpu_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static jpu_buffer_pool_t s_jpu_buffer_pool[MAX_JPU_BUFFER_POOL] = {0};

static int jpu_swap_endian(unsigned char *data, int len, int endian);

#if defined _WIN32
// Define an Interface Guid so that apps can find the device and talk to it.
// {84703EC3-9B1B-49D7-9AA6-0C42C6465681}
DEFINE_GUID(GUID_DEVINTERFACE_bm_sophon0,
            0x84703ec3,
            0x9b1b,
            0x49d7,
            0x9a,
            0xa6,
            0xc,
            0x42,
            0xc6,
            0x46,
            0x56,
            0x81);

const GUID* g_guid_interface[] = {&GUID_DEVINTERFACE_bm_sophon0};

BOOL GetDevicePath(unsigned int dev_id, HDEVINFO *hDevInfo, PSP_DEVICE_INTERFACE_DETAIL_DATA *pDeviceInterfaceDetail) {

    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    SP_DEVINFO_DATA          DeviceInfoData;

    ULONG size;
    BOOL  status = TRUE;

    *hDevInfo = SetupDiGetClassDevs(g_guid_interface[0],
                                        NULL,
                                        NULL,
                                        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (INVALID_HANDLE_VALUE == *hDevInfo) {
        printf("No devices interface class are in the system.\n");
        return FALSE;
    }

    //
    //  Initialize the appropriate data structures in preparation for
    //  the SetupDi calls.
    //
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    DeviceInfoData.cbSize      = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiEnumDeviceInterfaces(*hDevInfo,
                                     NULL,
                                     (LPGUID)g_guid_interface[0],
                                     dev_id,
                                     &DeviceInterfaceData)) {
        printf("No devices SetupDiEnumDeviceInterfaces for dev%d.\n", dev_id);
        goto Error;
    }

    //
    // Determine the size required for the DeviceInterfaceData
    //
    status = SetupDiGetDeviceInterfaceDetail(*hDevInfo, &DeviceInterfaceData, NULL, 0, &size, NULL);

    if (!status && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        printf("SetupDiGetDeviceInterfaceDetail failed, Error: %u", GetLastError());
        goto Error;
    }
    *pDeviceInterfaceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(size);

    if (!(*pDeviceInterfaceDetail)) {
        printf("Insufficient memory.\n");
        goto Error;
    }

    //
    // Initialize structure and retrieve data.
    //
    (*pDeviceInterfaceDetail)->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    status = SetupDiGetDeviceInterfaceDetail(*hDevInfo,
                                             &DeviceInterfaceData,
                                             (*pDeviceInterfaceDetail),
                                             size,
                                             NULL,
                                             &DeviceInfoData);

    if (!status) {
        printf("SetupDiGetDeviceInterfaceDetail failed, Error: %u", GetLastError());
        goto Error;
    }
    SetupDiDestroyDeviceInfoList(*hDevInfo);
    return status;

Error:
    if (*pDeviceInterfaceDetail)
        free(*pDeviceInterfaceDetail);
    SetupDiDestroyDeviceInfoList(*hDevInfo);
    return FALSE;
}

BOOL GetDeviceHandle(unsigned int  dev_id, HANDLE *hDevice) {

    BOOL status = TRUE;
	HDEVINFO hDevInfo;
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetail;

    if (!GetDevicePath(dev_id, &hDevInfo, &pDeviceInterfaceDetail)) {
        printf("Create device %d failed", dev_id);
        return FALSE;
    }

    if (pDeviceInterfaceDetail == NULL) {
        status = FALSE;
    }

    if (status) {
        *hDevice = CreateFile(pDeviceInterfaceDetail->DevicePath,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                  NULL);

        if ((*hDevice) == INVALID_HANDLE_VALUE) {
            if (pDeviceInterfaceDetail)
                free(pDeviceInterfaceDetail);
            status = FALSE;
            printf("CreateFile failed.  Error:%u", GetLastError());
        }
    }
    return status;
}
#endif

int jdi_init(int device_index)
{
#if !defined  _WIN32
    unsigned long  page_size = sysconf(_SC_PAGE_SIZE);
    unsigned long  page_mask = (~(page_size - 1));
#endif

    off_t pgoff;
    int ret,i,index;
    char dev_name[32] = {0};

#ifdef BM_PCIE_MODE
    index = device_index;
    sprintf(dev_name, "%s%d", JPU_DEVICE_NAME, index);

#ifdef  _WIN32
    if (s_jpu_mutex != NULL)
    {
        s_jpu_mutex = CreateMutex( NULL,FALSE,	NULL);
        if (s_jpu_mutex == NULL)
        {
            printf("CreateMutex Error: %u\n", GetLastError());
            return -1;
        }
    }
#endif
#else
    index = 0;
    sprintf(dev_name, "%s", JPU_DEVICE_NAME);
#endif
    if (s_jpu_fd[index] != -1 && s_jpu_fd[index] != 0x00)
    {
        s_task_num[index]++;
        return 0;
    }
#ifdef  _WIN32
    if(!GetDeviceHandle(index,&(s_jpu_fd[index]))) {
        JLOG(ERR, "[JDI] Can't open jpu driver ret=0x%x\n", (int)s_jpu_fd[index]);
        return -1;
    }
    printf("GetDeviceHandle successfully, device index = %d, jpu fd = %d, ",index, s_jpu_fd[index]);
#else
    s_jpu_fd[index] = open(dev_name, O_RDWR);
    if (s_jpu_fd[index] < 0) {
        JLOG(ERR, "[JDI] Can't open jpu driver ret=0x%x\n", (int)s_jpu_fd[index]);
        return -1;
    }
    printf("Open %s successfully, device index = %d, jpu fd = %d, ", dev_name, index, s_jpu_fd[index]);
#endif

    //  ret = jdi_lock();   //remove here, atomic lock used in bmjpuapi.c - xun
    ret = 0;
    if (ret < 0)
    {
        JLOG(ERR, "[JDI] fail to pthread_mutex_t lock function\n");
        goto ERR_JDI_INIT;
    }

#ifndef BM_PCIE_MODE
    memset(&s_jpu_dump_flag, 0, sizeof(jpudrv_buffer_t));
    s_jpu_dump_flag.size = DUMP_FLAG_MEM_SIZE;

    /* The page size is 4096. */
    s_jpu_dump_flag.virt_addr = (unsigned long )mmap(NULL, s_jpu_dump_flag.size, PROT_READ | PROT_WRITE, MAP_SHARED, s_jpu_fd[index], 0);
    if ((void *)s_jpu_dump_flag.virt_addr == MAP_FAILED)
    {
        JLOG(ERR, "[JDI] fail to map dump flag memory. pa=0x%lx, size=%d. [error=%s]\n",
             s_jpu_dump_flag.phys_addr, (int)s_jpu_dump_flag.size, strerror(errno));
        s_jpu_dump_flag.size      = 0;
    }
#endif

#if !defined  _WIN32
    if (ioctl(s_jpu_fd[index], JDI_IOCTL_GET_REGISTER_INFO, &s_jdb_register[index]) < 0)
    {
        JLOG(ERR, "[JDI] fail to get host interface register. [error=%s]\n", strerror(errno));
        goto ERR_JDI_INIT;
    }


    int max_num_jpu_core = -1;
    if (ioctl(s_jpu_fd[index], JDI_IOCTL_GET_MAX_NUM_JPU_CORE, &max_num_jpu_core) < 0)
    {
        JLOG(ERR, "[JDI] fail to get max_num_jpu_core. [error=%s]\n", strerror(errno));
        goto ERR_JDI_INIT;
    }

    for(i = 0; i < max_num_jpu_core; i++)
    {
        pgoff = s_jdb_register[index][i].phys_addr & page_mask;
        s_jdb_register[index][i].virt_addr = (unsigned long )mmap(NULL, s_jdb_register[index][i].size, PROT_READ | PROT_WRITE, MAP_SHARED, s_jpu_fd[index], pgoff);
        if ((void *)s_jdb_register[index][i].virt_addr == MAP_FAILED)
        {
            JLOG(ERR, "[JDI] fail to map jpu core %d registers! [error=%s]\n", i, strerror(errno));
            goto ERR_JDI_INIT;
        }
    }
#endif
    s_task_num[index]++;
    //jdi_unlock();

    jdi_set_clock_gate(1);

    // JLOG(INFO, "[JDI] success to init driver \n");
    return s_jpu_fd[index];

ERR_JDI_INIT:
    jdi_unlock();
    jdi_release(index);
    return -1;
}

int jdi_release(int device_index)
{
    int ret,i;

    if (s_jpu_fd[device_index] == -1 || s_jpu_fd[device_index] == 0x00)
        return 0;

    // ret = jdi_lock();  //remove here, it doesn't call
    ret = 0;
    if (ret < 0)
    {
        JLOG(ERR, "[JDI] fail to pthread_mutex_t lock function\n");
        return -1;
    }


    s_task_num[device_index]--;
    if (s_task_num[device_index] > 0) // means that the opened instance remains
    {
#ifdef _WIN32
        JLOG(ERR, "s_task_num =%d. id=%lu\n", s_task_num[device_index], GetCurrentThreadId()) ;
#else
        JLOG(ERR, "s_task_num =%d. id=%lu\n", s_task_num[device_index], pthread_self()) ;
#endif
        jdi_unlock();
        return 0;
    }
#ifndef _WIN32

    int max_num_jpu_core = -1;
    if (ioctl(s_jpu_fd[device_index], JDI_IOCTL_GET_MAX_NUM_JPU_CORE, &max_num_jpu_core) < 0)
    {
        JLOG(ERR, "[JDI] fail to get max_num_jpu_core. [error=%s]\n", strerror(errno));
        return 0;
    }

    for(i = 0; i < max_num_jpu_core; i++)
    {
        munmap((void *)s_jdb_register[device_index][i].virt_addr,s_jdb_register[device_index][i].size);
        memset(&s_jdb_register[device_index][i], 0x00, sizeof(jpudrv_buffer_t));
    }
#ifndef BM_PCIE_MODE
   if ((void*)s_jpu_dump_flag.virt_addr != MAP_FAILED) {
        munmap((void *)s_jpu_dump_flag.virt_addr, s_jpu_dump_flag.size);
        memset(&s_jpu_dump_flag, 0, sizeof(jpudrv_buffer_t));
    }
#endif
#endif

    // jdi_unlock();
#ifdef _WIN32
    CloseHandle(s_jpu_mutex);
	CloseHandle(s_jpu_fd[device_index]);
	s_jpu_fd[device_index]  = 0;
#else
    close(s_jpu_fd[device_index]);
    s_jpu_fd[device_index] = -1;
#endif

    return 0;
}

int jdi_get_core_index(int device_index, unsigned long long readAddr, unsigned long long writeAddr)
{
    int ret, index;
    jpudrv_remap_info_t  remap_info = { readAddr, writeAddr, -1};

#ifdef BM_PCIE_MODE
    index = device_index;
#else
    index = 0;
#endif
    if(s_jpu_fd[index] == -1 || s_jpu_fd[index] == 0x00)
        return -1;

#ifdef  _WIN32
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    unsigned int bytesReceived = 0;
    ret = DeviceIoControl(s_jpu_fd[index], JDI_IOCTL_GET_INSTANCE_CORE_INDEX, &remap_info, sizeof(remap_info), &remap_info, sizeof(remap_info), &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if (ret == 0)
    {
        if(GetLastError() != ERROR_IO_PENDING)
            JLOG(ERR, "DeviceIoControl	JDI_IOCTL_GET_INSTANCE_CORE_INDEX failed,Error: %u\n", GetLastError());
    }

#else
    while ((ret = ioctl(s_jpu_fd[index], JDI_IOCTL_GET_INSTANCE_CORE_INDEX, &remap_info)) < 0  && errno == EINTR)
    {
         JLOG(ERR, "[JDI] fail to get core index instIdx=%d,[error=%s]\n", (int)remap_info.core_idx, strerror(errno));
         continue;
    }
#endif

    s_jpu_core_idx = remap_info.core_idx;
    s_jpu_device_idx = index;
    if(remap_info.core_idx < 0)
    {
        jdi_close_instance(remap_info.core_idx);
        JLOG(TRACE, "[JDI] jdi_close_instance(), instIdx=%d\n",remap_info.core_idx);
    }

    return remap_info.core_idx;
}

int jdi_open_instance(unsigned long long instIdx)
{
#if 0
    int inst_num;

    if(s_jpu_fd == -1 || s_jpu_fd == 0x00)
        return -1;

    if (ioctl(s_jpu_fd, JDI_IOCTL_OPEN_INSTANCE, &instIdx) < 0)
    {
        JLOG(ERR, "[JDI] fail to deliver open instance num instIdx=%d. [error=%s]\n",
             (int)instIdx, strerror(errno));
        return -1;
    }

    if (ioctl(s_jpu_fd, JDI_IOCTL_GET_INSTANCE_NUM, &inst_num) < 0)
    {
        JLOG(ERR, "[JDI] fail to deliver open instance num instIdx=%d. [error=%s]\n",
             (int)instIdx, strerror(errno));
        return -1;
    }

    s_pjip->jpu_instance_num = inst_num;
#endif
    return 0;
}

int jdi_close_instance(unsigned int instIdx)
{
    if(s_jpu_fd[s_jpu_device_idx] == -1 || s_jpu_fd[s_jpu_device_idx] == 0x00)
    {
        printf(" jdi_close_instance error \n");
        return -1;
    }
#ifdef  _WIN32
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    int ret = 0;
    unsigned int bytesReceived = 0;
    ret = DeviceIoControl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX, &instIdx, sizeof(instIdx), NULL, 0, &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if (ret == 0)
    {
        if(GetLastError() != ERROR_IO_PENDING){
            JLOG(ERR, "DeviceIoControl	JDI_IOCTL_RESET failed,Error: %u\n", GetLastError());
            return -1;
        }
    }
#else
    if (ioctl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX, &instIdx) < 0)
    {
        JLOG(ERR, "[JDI] fail to jdi_close_instance. [error=%s]\n",
             (int)instIdx, strerror(errno));
        return -1;
    }
#endif
#if 0
    int inst_num;
    if (ioctl(s_jpu_fd, JDI_IOCTL_GET_INSTANCE_NUM, &inst_num) < 0)
    {
        JLOG(ERR, "[JDI] fail to deliver open instance num instIdx=%d. [error=%s]\n",
             (int)instIdx, strerror(errno));
        return -1;
    }
    s_pjip->jpu_instance_num = inst_num;
#endif
    return 0;
}

int jdi_hw_reset()
{
#if 0
    if (s_jpu_fd[s_jpu_device_idx] == -1 || s_jpu_fd[s_jpu_device_idx] == 0x00)
    {
        // TODO
        printf(" Can't open jpu driver,  %s(): fd = %d\n", __FUNCTION__, s_jpu_fd[s_jpu_device_idx]);
        return -1;
    }
#ifdef  _WIN32
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    int ret = 0;
    unsigned int bytesReceived = 0;
    ret = DeviceIoControl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_RESET, &s_jpu_core_idx, sizeof(s_jpu_core_idx), NULL, 0, &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if (ret == 0)
    {
        if(GetLastError() != ERROR_IO_PENDING){
            JLOG(ERR, "DeviceIoControl	JDI_IOCTL_RESET ,Error: %u\n", GetLastError());
            return -1;
        }
    }
#else
    if (ioctl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_RESET, &s_jpu_core_idx) < 0)
    {
        JLOG(ERR, "[JDI] failed to reset jpu with ioctl() request. [error=%s]\n",
             strerror(errno));
        return -1;
    }
#endif
    return 0;
#endif
    return jdi_hw_reset_all();
}

int jdi_hw_reset_all()
{
    // if (s_jpu_fd[s_jpu_device_idx] == -1 || s_jpu_fd[s_jpu_device_idx] == 0x00)
    // {
    //     // TODO
    //     printf(" Can't open jpu driver,  %s(): fd = %d\n", __FUNCTION__, s_jpu_fd[s_jpu_device_idx]);
    //     return -1;
    // }

    int max_nums_jpu_core = 0;

#ifdef  _WIN32

    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    int ret = 0;
    unsigned int bytesReceived = 0;
    ret = DeviceIoControl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_RESET_ALL, &max_nums_jpu_core, sizeof(max_nums_jpu_core), NULL, 0, &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if (ret == 0)
    {
        if(GetLastError() != ERROR_IO_PENDING){
            JLOG(ERR, "DeviceIoControl	JDI_IOCTL_RESET_ALL ,Error: %u\n", GetLastError());
            return -1;
        }
    }
#else
    if(ioctl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_GET_MAX_NUM_JPU_CORE, &max_nums_jpu_core) < 0)
    {
        return -1;
    }

    if(ioctl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_RESET_ALL, &max_nums_jpu_core) < 0)
    {
        return -1;
    }
#endif
    return 0;
}

int jdi_lock()
{
#if 0
    jdi_info_t *jdi = &s_jdi_info[0];
    const int MUTEX_TIMEOUT = 0x7fffffff;   // ms

    while(1)
    {
        int _ret;
        int i;
        for(i = 0; (_ret = pthread_mutex_trylock((pthread_mutex_t *)jdi->pjip->jpu_mutex)) != 0 && i < MUTEX_TIMEOUT; i++)
        {
            if(i == 0)
                JLOG(ERR, "jdi_lock: mutex is already locked - try again : ret = %d,  pthread id =%lu\n", _ret, pthread_self());
#ifdef  _KERNEL_
            udelay(1*1000);
#else
            usleep(1*1000);
#endif // _KERNEL_
            JLOG(ERR, "jdi_lock: mutex tiemout for %dms, i=%d,num=%d,jdi->pjip=%p, jpu_mutex=%p, pthread id =%lu\n",
                 JPU_INTERRUPT_TIMEOUT_MS, i, jdi->pjip->jpu_instance_num, jdi->pjip, jdi->pjip->jpu_mutex, pthread_self());
            if (i > JPU_INTERRUPT_TIMEOUT_MS*(jdi->pjip->jpu_instance_num))
            {
                JLOG(ERR, "jdi_lock: mutex tiemout for %dms, i=%d,jpu_instance_num=%d\n", JPU_INTERRUPT_TIMEOUT_MS,i, jdi->pjip->jpu_instance_num);
                break;
            }
        }

        if(_ret == 0)
            break;

        JLOG(ERR, "jdi_lock: can't get lock - force to unlock. [%d:%s], pthread id =%lu\n", _ret, strerror(_ret), pthread_self());
        jdi_unlock();
        jdi->pjip->pendingInst = NULL;
    }
    JLOG(INFO, "jdi->pjip=%p, jpu_mutex=%p. id=%lu\n",
         jdi->pjip, jdi->pjip->jpu_mutex, pthread_self());
#endif
    return 0;
}
void jdi_unlock()
{
#if 0
    jdi_info_t *jdi = &s_jdi_info[s_jpu_core_idx];
    JLOG(INFO, "id=%lu\n", pthread_self());
    pthread_mutex_unlock((pthread_mutex_t *)jdi->pjip->jpu_mutex);
#else
   // s_pjip[s_jpu_device_idx]->jpu_mutex = 0;
#endif
}
#ifdef  _WIN32
int jdi_lock2()
{
    const int MUTEX_TIMEOUT = 2000;   // ms
	DWORD _ret;

    _ret = WaitForSingleObject( s_jpu_mutex, MUTEX_TIMEOUT);
    if(_ret != WAIT_OBJECT_0)
    {
        JLOG(ERR, "jdi_lock: can't get lock - force to unlock. ret: %d,Error: %u, pthread id =%lu\n", _ret, GetLastError(), GetCurrentThreadId());
        jdi_unlock();
        return -1;
    }
    return 0;
}
void jdi_unlock2()
{
    ReleaseMutex(s_jpu_mutex);
}
#else
int jdi_lock2()
{
    const int MUTEX_TIMEOUT = 200;   // ms

    while(1)
    {
        int _ret;
        int i;
        for(i = 0; (_ret = pthread_mutex_trylock(&s_jpu_mutex)) != 0 && i < MUTEX_TIMEOUT; i++)
        {
            usleep(10*1000);
        }

        if(_ret == 0)
            break;

        JLOG(ERR, "jdi_lock: can't get lock - force to unlock. [%d:%s], pthread id =%lu\n", _ret, strerror(_ret), pthread_self());
        jdi_unlock();

        return -1;
    }

    return 0;
}
void jdi_unlock2()
{
    pthread_mutex_unlock(&s_jpu_mutex);
}
#endif

void jdi_write_register(unsigned int addr, unsigned int data)
{
#ifdef  _WIN32
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    jpu_register_info_t jri={0};
    jri.address_offset = addr;
    jri.core_idx = s_jpu_core_idx;
    jri.data = data;
    DWORD bytesReceived = 0;
    int status = 0;
    status = DeviceIoControl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_WRITE_REGISTER, &jri, sizeof(jpu_register_info_t), NULL, 0, 0, (LPOVERLAPPED)&varOverLapped);
    if (status == 0) {
       if(GetLastError() != ERROR_IO_PENDING){
            printf("DeviceIoControl jdi_write_register ,Error: %u\n", GetLastError());
            return;
        }
    }
#else
    unsigned int *reg_addr;

    if(s_jpu_fd[s_jpu_device_idx] == -1 || s_jpu_fd[s_jpu_device_idx] == 0x00)
        return ;
    reg_addr = (unsigned int *)(addr + s_jdb_register[s_jpu_device_idx][s_jpu_core_idx].virt_addr);
#ifdef __riscv
    *(volatile unsigned int *)reg_addr = data;
#else
    __atomic_store_n(reg_addr, data, __ATOMIC_SEQ_CST);
#endif

#endif
}

unsigned int jdi_read_register(unsigned int addr)
{
#ifdef  _WIN32
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    jpu_register_info_t jri={0};
    jri.address_offset = addr;
    jri.core_idx = s_jpu_core_idx;

    DWORD bytesReceived = 0;
    int status = 0;
    status = DeviceIoControl(s_jpu_fd[s_jpu_device_idx], JDI_IOCTL_READ_REGISTER, &jri, sizeof(jpu_register_info_t), &jri, sizeof(jpu_register_info_t), &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if (status == 0) {
        if(GetLastError() != ERROR_IO_PENDING) {
            printf("DeviceIoControl jdi_read_register, Error: %u\n", GetLastError());
            return -1;
        }
    }
    return jri.data;
#else
    unsigned int *reg_addr;
    reg_addr = (unsigned int *)(addr + s_jdb_register[s_jpu_device_idx][s_jpu_core_idx].virt_addr);
#ifdef __riscv
    return *(volatile unsigned int *)reg_addr;
#else
    return __atomic_load_n(reg_addr, __ATOMIC_SEQ_CST);
#endif

#endif
}

#ifdef BM_PCIE_MODE
int jdi_pcie_read_mem(int device_index, unsigned long long src, unsigned char *dst, size_t size)
{
    int ret = 0;
    struct memcpy_args {
        unsigned long long  src;
        unsigned long long  dst;
        size_t  size;
    } memcpy_args = {0};

    memcpy_args.src = src;
    memcpy_args.dst = (unsigned long long)dst;
    memcpy_args.size = size;

    if(s_jpu_fd[device_index] == -1 || s_jpu_fd[device_index] == 0x00)
        return -1;

#ifdef  _WIN32
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    unsigned int bytesReceived = 0;
    ret = DeviceIoControl(s_jpu_fd[device_index], JDI_IOCTL_READ_VMEM, &memcpy_args, sizeof(memcpy_args), NULL, 0, &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if (ret == 0)
    {
        if(GetLastError() != ERROR_IO_PENDING) {
            JLOG(ERR, "DeviceIoControl  JDI_IOCTL_READ_VMEM failed,Error: %u\n", GetLastError());
            ret = -1;
        }
    }

#else
    ret = ioctl(s_jpu_fd[device_index], JDI_IOCTL_READ_VMEM, &memcpy_args);
    if (ret < 0)
    {
        JLOG(ERR, "fail to JDI_IOCTL_READ_VMEM\n");
        ret = -1;
    }
#endif
    return ret;
}

int jdi_pcie_write_mem(int device_index, unsigned char *src, unsigned long long dst, size_t size)
{
    int ret = 0;
    struct memcpy_args {
        unsigned long long  src;
        unsigned long long  dst;
        size_t  size;
    } memcpy_args = {0};

    memcpy_args.src = (unsigned long long)src;
    memcpy_args.dst = dst;
    memcpy_args.size = size;

    if(s_jpu_fd[device_index] == -1 || s_jpu_fd[device_index] == 0x00)
	    return -1;

#ifdef  _WIN32
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    unsigned int bytesReceived = 0;
    ret = DeviceIoControl(s_jpu_fd[device_index], JDI_IOCTL_WRITE_VMEM, &memcpy_args, sizeof(memcpy_args), NULL, 0, &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if (ret == 0)
    {
        if(GetLastError() != ERROR_IO_PENDING){
            JLOG(ERR, "DeviceIoControl	JDI_IOCTL_WRITE_VMEM failed,Error: %u\n", GetLastError());
            ret = -1;
        }
    }
#else
    ret = ioctl(s_jpu_fd[device_index], JDI_IOCTL_WRITE_VMEM, &memcpy_args);
    if (ret < 0)
    {
        JLOG(ERR, "fail to JDI_IOCTL_WRITE_VMEM\n");
        ret = -1;
    }
#endif
    return ret;
}
#endif

int jdi_write_memory(unsigned long long addr, unsigned char *data, int len, int endian)
{
    jpudrv_buffer_t jdb;
    unsigned long long offset;
    int i;

    if(s_jpu_fd[s_jpu_device_idx] == -1 || s_jpu_fd[s_jpu_device_idx] == 0x00)
        return -1;

    memset(&jdb, 0x00, sizeof(jpudrv_buffer_t));

    for (i=0; i<MAX_JPU_BUFFER_POOL; i++)
    {
        if (s_jpu_buffer_pool[i].inuse == 1)
        {
            jdb = s_jpu_buffer_pool[i].jdb;
            if (addr >= jdb.phys_addr && addr < (jdb.phys_addr + jdb.size))
                break;
        }
    }

    if (!jdb.size) {
        JLOG(ERR, "address 0x%08x is not mapped address!!!\n", (int)addr);
        return -1;
    }

    offset = addr - (unsigned long long)jdb.phys_addr;

    jpu_swap_endian(data, len, endian);
    memcpy((void *)((unsigned long long)jdb.virt_addr+offset), data, len);
    return len;
}

int jdi_read_memory(unsigned long long addr, unsigned char *data, int len, int endian)
{
    jpudrv_buffer_t jdb;
    unsigned long long offset;
    int i;
    if(s_jpu_fd[s_jpu_device_idx] == -1 || s_jpu_fd[s_jpu_device_idx] == 0x00)
        return -1;

    memset(&jdb, 0x00, sizeof(jpudrv_buffer_t));
    for (i=0; i<MAX_JPU_BUFFER_POOL; i++)
    {
        if (s_jpu_buffer_pool[i].inuse == 1)
        {
            jdb = s_jpu_buffer_pool[i].jdb;
            if (addr >= jdb.phys_addr && addr < (jdb.phys_addr + jdb.size))
                break;
        }
    }

    if (!jdb.size)
        return -1;


    offset = addr - (unsigned long long)jdb.phys_addr;

    memcpy(data, (const void *)((unsigned long long)jdb.virt_addr+offset), len);
    jpu_swap_endian(data, len,  endian);

    return len;
}
#ifndef  _WIN32
int jdi_allocate_dma_memory(jpu_buffer_t *vb)
{
    jpudrv_buffer_t jdb  = {0};
    int i = -1;
    int ret = 0;

    if(s_jpu_fd[vb->device_index] == -1 || s_jpu_fd[vb->device_index] == 0x00) {
        JLOG(ERR, "Invalid handle! id=%lu", pthread_self());
        return -1;
    }

    memset(&jdb, 0x00, sizeof(jpudrv_buffer_t));

    jdb.flags = vb->flags;
    jdb.size  = vb->size;

    ret = ioctl(s_jpu_fd[vb->device_index], JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY, &jdb);
    if (ret < 0)
    {
        JLOG(ERR, "[JDI] failed to allocate physical memory (size=%d) with ioctl() request. "
             "[error=%s]. id=%lu\n",
             vb->size, strerror(errno), pthread_self());
        ret = -1;
        goto fail;
    }
    vb->phys_addr = (unsigned long long)jdb.phys_addr;
    vb->base      = (unsigned long long)jdb.base;

#if defined(LIBBMJPULITE)
    vb->virt_addr = 0;
#else
    /* Map physical address to virtual address in user space */
    unsigned long long page_size = sysconf(_SC_PAGE_SIZE);
    unsigned long long page_mask = (~(page_size-1));
    off_t pgoff = jdb.phys_addr & page_mask;

#ifndef BM_PCIE_MODE
    jdb.virt_addr = (unsigned long )mmap(NULL, jdb.size, PROT_READ | PROT_WRITE, MAP_SHARED, s_jpu_fd[vb->device_index], pgoff);
    if ((void *)jdb.virt_addr == MAP_FAILED)
    {
        JLOG(ERR, "[JDI] failed to map physical memory. pa=0x%lx, size=%d. [error=%s]. id=%lu\n",
             jdb.phys_addr, (int)jdb.size, strerror(errno), pthread_self());
        memset(vb, 0x00, sizeof(jpu_buffer_t));
        ret = -1;
        goto fail;
    }
    vb->virt_addr = (unsigned long )jdb.virt_addr;
#else
    vb->virt_addr = 0;
#endif
#endif

#ifndef BM_PCIE_MODE
    jdi_lock2();     // move to here to protect s_jpu_buffer_pool - xun
    for (i=0; i<MAX_JPU_BUFFER_POOL; i++)
    {
        if (s_jpu_buffer_pool[i].inuse == 0)
        {
            s_jpu_buffer_pool[i].jdb = jdb;
            s_jpu_buffer_pool[i].inuse = 1;
            break;
        }
    }
    jdi_unlock2();

    if (i >= MAX_JPU_BUFFER_POOL)
    {
        JLOG(ERR, "fail to find an unused buffer in pool! id=%lu\n",
             pthread_self());
        memset(vb, 0x00, sizeof(jpu_buffer_t));
        ret = -1;
        goto fail;
    }
#endif

    JLOG(TRACE, "size=%d, phys addr=%p, i=%d. id=%lu",
         jdb.size, jdb.phys_addr, i, pthread_self());
fail:
    return ret;
}

void jdi_free_dma_memory(jpu_buffer_t *vb)
{
#ifndef BM_PCIE_MODE
    jpudrv_buffer_t jdb;
    int i;
#endif
    int ret;

    if(s_jpu_fd[vb->device_index] == -1 || s_jpu_fd[vb->device_index] == 0x00) {
        JLOG(ERR, "Invalid handle! id=%lu", pthread_self());
        return;
    }

    if (vb->size == 0) {
        JLOG(ERR, "addr=%p, id=%lu\n", vb->phys_addr, pthread_self());
        return;
    }

#ifndef BM_PCIE_MODE
    memset(&jdb, 0x00, sizeof(jpudrv_buffer_t));

    jdi_lock2(); // move here to protect s_jpu_buffer_pool - xun
    for (i=0; i<MAX_JPU_BUFFER_POOL; i++)
    {
        if (s_jpu_buffer_pool[i].jdb.phys_addr == vb->phys_addr)
        {
            s_jpu_buffer_pool[i].inuse = 0;
            jdb = s_jpu_buffer_pool[i].jdb;
            break;
        }
    }
    jdi_unlock2();

    if (!jdb.size)
    {
        JLOG(ERR, "Invalid buffer to free! address=0x%lx. id=%lu\n",
             jdb.virt_addr, pthread_self());
        jdi_unlock();
        return;
    }

    JLOG(TRACE, "size=%d, phys addr=%p, virt addr=%p, i=%d. id=%lu",
         jdb.size, jdb.phys_addr, jdb.virt_addr, i, pthread_self());

    if (jdb.virt_addr != 0)
    {
        ret = munmap((void *)jdb.virt_addr, jdb.size);
        if (ret != 0)
        {
            JLOG(ERR, "fail to unmap virtual address(0x%lx)! id=%lu\n",
                 jdb.virt_addr, pthread_self());
        }
    }
#endif

#ifndef BM_PCIE_MODE
    ret = ioctl(s_jpu_fd[vb->device_index], JDI_IOCTL_FREE_PHYSICAL_MEMORY, &jdb);
    if (ret < 0)
    {
        JLOG(ERR, "[JDI] failed to free physical memory with ioctl() request. "
             "pa=0x%lx, va=%p, size=%d. [error=%s]. id=%lu\n",
             jdb.phys_addr, jdb.virt_addr, jdb.size,
             strerror(errno), pthread_self());
    }
#else
    ret = ioctl(s_jpu_fd[vb->device_index], JDI_IOCTL_FREE_PHYSICAL_MEMORY, vb);
    if (ret < 0)
    {
        JLOG(ERR, "[JDI] failed to free physical memory with ioctl() request. "
             "pa=0x%lx, va=%p, size=%d. [error=%s]. id=%lu\n",
             vb->phys_addr, vb->virt_addr, vb->size,
             strerror(errno), pthread_self());
    }
#endif
    memset(vb, 0, sizeof(jpu_buffer_t));
}

int jdi_map_dma_memory(jpu_buffer_t* vb, int flags)
{
    unsigned long long page_size = sysconf(_SC_PAGE_SIZE);
    unsigned long long page_mask = (~(page_size-1));

    int prot = 0;
    int ret = 0;

    if (s_jpu_fd[vb->device_index] == -1 || s_jpu_fd[vb->device_index] == 0x00) {
        JLOG(ERR, "Invalid handle! id=%lu", pthread_self());
        return -1;
    }

    if (vb == NULL) {
        JLOG(ERR, "Invalid parameter! id=%lu", pthread_self());
        return -1;
    }

    if (vb->size == 0) {
        JLOG(ERR, "Invalid physical address(0x%lx), size=0! id=%lu\n",
             vb->phys_addr, pthread_self());
        return -1;
    }

    if (vb->virt_addr != 0) {
        JLOG(WARN, "physical address(0x%lx) was mapped to virtual address(%lx)! id=%lu\n",
             vb->phys_addr, vb->virt_addr, pthread_self());
        return 0;
    }

    // jdi_lock();  // remove here by xun

    if (flags&1)
        prot |= PROT_WRITE;
    if (flags&2)
        prot |= PROT_READ;

    /* Map physical address to virtual address in user space */
    off_t pgoff = vb->phys_addr & page_mask;
    vb->virt_addr = (unsigned long )mmap(NULL, vb->size, prot, MAP_SHARED, s_jpu_fd[vb->device_index], pgoff);
    if ((void *)vb->virt_addr == MAP_FAILED) {
        JLOG(ERR, "[JDI] fail to map pa=0x%lx, size=%d. [error=%s]. id=%lu\n",
             vb->phys_addr, (int)vb->size, strerror(errno), pthread_self());
        ret = -1;
        goto fail;
    }
    JLOG(TRACE, "size=%d, phys addr=%p, virt addr=%lx. id=%lu",
         vb->size, vb->phys_addr, vb->virt_addr, pthread_self());

fail:
    //jdi_unlock();

    return ret;
}

int jdi_unmap_dma_memory(jpu_buffer_t* vb)
{
    int ret;

    if (s_jpu_fd[vb->device_index] == -1 || s_jpu_fd[vb->device_index] == 0x00) {
        JLOG(ERR, "Invalid handle! id=%lu", pthread_self());
        return -1;
    }

    if (vb == NULL) {
        JLOG(ERR, "Invalid parameter! id=%lu", pthread_self());
        return -1;
    }

    if (vb->size == 0) {
        JLOG(ERR, "Invalid size! id=%lu\n", pthread_self());
        return -1;
    }

    if (vb->virt_addr == 0) {
        JLOG(WARN, "physical address(%lx) was unmapped! id=%lu\n",
             vb->phys_addr, pthread_self());
        return 0;
    }

    // jdi_lock();  // remove here by xun

    JLOG(TRACE, "size=%d, virt addr=0x%lx. id=%lu",
         vb->size, vb->virt_addr, pthread_self());

    ret = munmap((void *)vb->virt_addr, vb->size);
    if (ret != 0) {
        JLOG(ERR, "fail to unmap virtual address(0x%lx)! id=%lu\n",
             vb->virt_addr, pthread_self());
    }

    vb->virt_addr = 0;

    // jdi_unlock();

    return ret;
}

int jdi_invalidate_region(jpu_buffer_t* vb, unsigned long long phys_addr, unsigned long long size)
{
    dcache_range_t dcr = {phys_addr, size};
    int ret;

    if (vb->flags == 2)
        return 0;

    ret = ioctl(s_jpu_fd[vb->device_index], JDI_IOCTL_INVAL_DCACHE_RANGE, &dcr);
    if (ret < 0)
    {
        JLOG(ERR, "ioctl: invalidate dcache range: phys_addr=0x%lx, size=%ld. id=%lu\n",
             phys_addr, size, pthread_self());
        return -1;
    }

    return 0;
}
int jdi_flush_region(jpu_buffer_t* vb, unsigned long long phys_addr, unsigned long long size)
{
    dcache_range_t dcr = {phys_addr, size};
    int ret;

    if (vb->flags == 2)
        return 0;

    ret = ioctl(s_jpu_fd[vb->device_index], JDI_IOCTL_FLUSH_DCACHE_RANGE, &dcr);
    if (ret < 0)
    {
        JLOG(ERR, "ioctl: flush dcache range: phys_addr=0x%lx, size=%ld. id=%lu\n",
             phys_addr, size, pthread_self());
        return -1;
    }

    return 0;
}

int jdi_get_dump_info(void* dump_info)
{
    int ret = 0;
    if((void*)s_jpu_dump_flag.virt_addr != MAP_FAILED) {
        int * pDumpInfoAddr      = (int*)(s_jpu_dump_flag.virt_addr);
        if((*pDumpInfoAddr) == 0)
            return 0;
        DumpInfo* dumpInfo       = (DumpInfo*)dump_info;
        dumpInfo->dump_frame_num = *pDumpInfoAddr;
        dumpInfo->jpufd          = s_jpu_fd[s_jpu_device_idx];
        if(dumpInfo->dump_frame_num <= 0 || dumpInfo->dump_frame_num > 10)
        {
            return 0;
        }
        ret = 1;
    }
    return ret;
}

void jdi_clear_dump_info()
{
    if((void*)s_jpu_dump_flag.virt_addr != MAP_FAILED) {
        memset((void*)s_jpu_dump_flag.virt_addr, 0, DUMP_FLAG_MEM_SIZE );
    }
}
#endif
int jdi_set_clock_gate(int enable)
{
#if 0
    int ret;
    jpudrv_clkgate_info_t  clkgate_info;

    if (s_jpu_fd == -1 || s_jpu_fd == 0x00)
        return 0;

    s_clock_state = enable;
    clkgate_info.clkgate = enable;
    clkgate_info.core_idx = s_jpu_core_idx;

    if(ioctl(s_jpu_fd, JDI_IOCTL_SET_CLOCK_GATE, (void*)&clkgate_info) < 0)
    {
        JLOG(ERR, "fail to jdi_set_clock_gate  core %d, enable=%d, id=%lu\n",s_jpu_core_idx,enable, pthread_self());
        ret = -1;
    }
    return ret;
#else
    return 0;
#endif
}

int jdi_get_clock_gate()
{
    return s_clock_state;
}

int jdi_wait_interrupt(int device_index,Uint32 coreIdx,int timeout)
{
#ifdef SUPPORT_INTERRUPT
    jpudrv_intr_info_t intr_info = {0};
    int ret;

    intr_info.timeout   = timeout;

    intr_info.core_idx  = coreIdx;

#ifdef  _WIN32
    unsigned int irq_status=0;
    OVERLAPPED varOverLapped;
    memset(&varOverLapped,0,sizeof(OVERLAPPED));

    unsigned int bytesReceived = 0;
    ret = DeviceIoControl(s_jpu_fd[device_index], JDI_IOCTL_WAIT_INTERRUPT, &intr_info, sizeof(jpudrv_intr_info_t), &irq_status, sizeof(unsigned int), &bytesReceived, (LPOVERLAPPED)&varOverLapped);
    if ((ret == 0) && (GetLastError() != ERROR_IO_PENDING))
    {
        JLOG(ERR, "DeviceIoControl	JDI_IOCTL_WAIT_INTERRUPT failed, Error: %u\n", GetLastError());
        ret = -1;
    }
    else
        ret = irq_status;

    return ret;
#else
    ret = ioctl(s_jpu_fd[device_index], JDI_IOCTL_WAIT_INTERRUPT, (void*)&intr_info);
    if (ret < 0)
    {
        if (errno == ETIME)
            return -1;
        else
            return errno;
    }
    else
    {
        return ret;
    }
#endif
#else
    Int64 elapse, cur;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec =0;

    gettimeofday(&tv, NULL);
    elapse = tv.tv_sec*1000 + tv.tv_usec/1000;

    while(1)
    {
#ifdef BMDEBUG_V
        unsigned int reg;
        reg = jdi_read_register(MJPEG_PIC_STATUS_REG);
        JLOG(INFO, "The JPU status reg 0x%08x\n", reg);
        if(reg)
            break;
#else
        if (jdi_read_register(MJPEG_PIC_STATUS_REG))
            break;
#endif /* BMDEBUG_V */

        gettimeofday(&tv, NULL);
        cur = tv.tv_sec * 1000 + tv.tv_usec / 1000;

        if ((cur - elapse) > timeout)
        {
            return -1;
        }
#ifdef  _KERNEL_    //do not use in real system. use SUPPORT_INTERRUPT;
        udelay(1*1000);
#else
        usleep(1*1000);
#endif // _KERNEL_
    }
    return 0;
#endif
}

void jdi_log(int cmd, int step)
{
    int i;

    switch(cmd)
    {
    case JDI_LOG_CMD_PICRUN:
        if (step == 1)  //
            JLOG(INFO, "\n**PIC_RUN start\n");
        else
            JLOG(INFO, "\n**PIC_RUN end \n");
        break;
    }

    JLOG(INFO, "\nClock Status=%d\n", jdi_get_clock_gate());

    for (i=0; i<=0x238; i=i+16)
    {
        JLOG(INFO, "0x%04xh: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
             jdi_read_register(i), jdi_read_register(i+4),
             jdi_read_register(i+8), jdi_read_register(i+0xc));
    }
}

int jpu_swap_endian(unsigned char *data, int len, int endian)
{
    unsigned long long *p;
    unsigned long long v1, v2, v3;
    int i;
    int swap = 0;
    p = (unsigned long long *)data;

    if(endian == JDI_SYSTEM_ENDIAN)
        swap = 0;
    else
        swap = 1;

    if (swap)
    {
        if (endian == JDI_LITTLE_ENDIAN ||
            endian == JDI_BIG_ENDIAN) {
            for (i=0; i<len/4; i+=2)
            {
                v1 = p[i];
                v2  = ( v1 >> 24) & 0xFF;
                v2 |= ((v1 >> 16) & 0xFF) <<  8;
                v2 |= ((v1 >>  8) & 0xFF) << 16;
                v2 |= ((v1 >>  0) & 0xFF) << 24;
                v3 =  v2;
                v1  = p[i+1];
                v2  = ( v1 >> 24) & 0xFF;
                v2 |= ((v1 >> 16) & 0xFF) <<  8;
                v2 |= ((v1 >>  8) & 0xFF) << 16;
                v2 |= ((v1 >>  0) & 0xFF) << 24;
                p[i]   =  v2;
                p[i+1] = v3;
            }
        } else {
            int sys_endian = JDI_SYSTEM_ENDIAN;
            int swap4byte = 0;
            swap = 0;
            if (endian == JDI_32BIT_LITTLE_ENDIAN) {
                if (sys_endian == JDI_BIG_ENDIAN) {
                    swap = 1;
                }
            } else {
                if (sys_endian == JDI_BIG_ENDIAN) {
                    swap4byte = 1;
                } else if (sys_endian == JDI_LITTLE_ENDIAN) {
                    swap4byte = 1;
                    swap = 1;
                } else {
                    swap = 1;
                }
            }
            if (swap) {
                for (i=0; i<len/4; i++) {
                    v1 = p[i];
                    v2  = ( v1 >> 24) & 0xFF;
                    v2 |= ((v1 >> 16) & 0xFF) <<  8;
                    v2 |= ((v1 >>  8) & 0xFF) << 16;
                    v2 |= ((v1 >>  0) & 0xFF) << 24;
                    p[i] = v2;
                }
            }
            if (swap4byte) {
                for (i=0; i<len/4; i+=2) {
                    v1 = p[i];
                    v2 = p[i+1];
                    p[i]   = v2;
                    p[i+1] = v1;
                }
            }
        }


    }
    return swap;
}
unsigned long long jdi_get_memory_addr_high(unsigned long long addr)
{
#ifdef CHIP_BM1684
    unsigned long long ret = 4;
#else
    unsigned long long ret = 1;
#endif
#if 0
    jdi_info_t *jdi = &s_jdi_info[0];
    unsigned int *reg_addr;
    reg_addr = (unsigned int *)(s_jdb_register.virt_addr);
    ret = *(volatile unsigned int *)reg_addr;

    ret = (ret >> 16) & 0xFF;
#endif
    return (ret<<32) | addr;
}
#endif
