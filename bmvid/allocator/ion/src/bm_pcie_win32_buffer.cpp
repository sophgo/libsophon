#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <initguid.h>
#include <guiddef.h>
#include <iostream>
#include "windows.h"
#include "bm_ion.h"
#include "internal_pcie_win32.h"
#include <SetupAPI.h>

//#define ION_PRINT_INFO

typedef unsigned char u8;
typedef unsigned char U8;

typedef signed char s8;
typedef signed char S8;

typedef unsigned short u16;
typedef unsigned short U16;

typedef signed short s16;
typedef signed short S16;

typedef unsigned int u32;
typedef unsigned int U32;
typedef signed int s32;
typedef signed int S32;

typedef unsigned long long int u64;
typedef unsigned long long int U64;
typedef signed long long int s64;
typedef signed long long int S64;


typedef struct vpudrv_buffer_t {
    u64           size;
    u64           phys_addr;
    u64           base;      /* kernel logical address in use kernel */
    u64           virt_addr; /* virtual user space address */
    u32          core_idx;
    u32 reserved;
} vpudrv_buffer_t;


typedef struct transfer_opt_s {
    uint32_t size;
    uint64_t src;
    uint64_t dst;
} transfer_opt_t;

static HANDLE g_dev_handle[256] = {0};
static int g_count[256]  = {0};
static PSP_DEVICE_INTERFACE_DETAIL_DATA pgDeviceInterfaceDetail[256] = { 0 };
static HANDLE ion_mutex = CreateMutex(NULL, FALSE, NULL);



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




const GUID* g_guid_interface_ion[] = {&GUID_DEVINTERFACE_bm_sophon0};


BOOL GetDevicePath(u32 dev_id, HDEVINFO *hDevInfo, PSP_DEVICE_INTERFACE_DETAIL_DATA *pDeviceInterfaceDetail) {

    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    SP_DEVINFO_DATA          DeviceInfoData;

    ULONG size;
    BOOL  status = TRUE;

    //
    //  Retreive the device information for all PLX devices.
    //
    *hDevInfo = SetupDiGetClassDevs(g_guid_interface_ion[0],
        NULL,
        NULL,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (INVALID_HANDLE_VALUE == *hDevInfo) {
        printf("No sophon devices interface class are in the system.\n");
        return FALSE;
    }
    //
    //  Initialize the appropriate data structures in preparation for
    //  the SetupDi calls.
    //
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiEnumDeviceInterfaces(*hDevInfo,
        NULL,
        (LPGUID)g_guid_interface_ion[0],
        dev_id,
        &DeviceInterfaceData)) {
        printf("No sophon devices SetupDiEnumDeviceInterfaces for dev%d.\n", dev_id);
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

    if (! *pDeviceInterfaceDetail) {
        printf("Insufficient memory.\n");
        goto Error;
    }

    //
    // Initialize structure and retrieve data.
    //
    (*pDeviceInterfaceDetail)->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    status = SetupDiGetDeviceInterfaceDetail(*hDevInfo,
        &DeviceInterfaceData,
        *pDeviceInterfaceDetail,
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


BOOL GetDeviceHandle(u32 dev_id, HANDLE *hDevice) {

    BOOL status = TRUE;
    HDEVINFO hDevInfo;

    if (!GetDevicePath(dev_id, &hDevInfo, &(pgDeviceInterfaceDetail[dev_id]) )) {
        printf("Create device %d failed", dev_id);
        return FALSE;
    }

    if (pgDeviceInterfaceDetail[dev_id] == NULL) {
        status = FALSE;
    }

    if (status) {
        *hDevice = CreateFile((pgDeviceInterfaceDetail[dev_id])->DevicePath,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                  NULL);

        if ((*hDevice) == INVALID_HANDLE_VALUE) {
            if(pgDeviceInterfaceDetail[dev_id])
                free(pgDeviceInterfaceDetail[dev_id]);
            status = FALSE;
            printf("CreateFile failed.  Error:%u", GetLastError());
        }
    }
    return status;
}


int pcie_allocator_open(int soc_idx)
{
    char sophon_device[512] = {0};
    HANDLE *h_dev = NULL;
    int ret = 0;
    HANDLE hDevice;

    if(ion_mutex == NULL) {
        printf("pcie_allocator_open line=%d\n", __LINE__);
        ion_mutex = CreateMutex(NULL, FALSE, NULL);
    }

    DWORD dwWaitResult = WaitForSingleObject(ion_mutex, INFINITE);
    if (dwWaitResult != WAIT_OBJECT_0){
        printf("pcie_allocator_open line=%d\n", __LINE__);
        //return 0;
    }

    if (g_dev_handle[soc_idx] != NULL && g_dev_handle[soc_idx] != INVALID_HANDLE_VALUE) {
        g_count[soc_idx]++;
        goto Exit;
    }

    if(!GetDeviceHandle(soc_idx,&hDevice)) {
        goto Exit;
    }

    g_dev_handle[soc_idx] = hDevice;
    g_count[soc_idx] = 1;

Exit:
    if (ReleaseMutex(ion_mutex))
        return 0;
    else
        return -1;

    return ret;
}

int pcie_allocator_close(int soc_idx)
{
    HANDLE *h_dev = NULL;
    int ret = 0;
	BOOL  status = TRUE;

    DWORD dwWaitResult = WaitForSingleObject(
        ion_mutex,    // handle to mutex
        INFINITE);  // no time-out interval
    if (dwWaitResult != WAIT_OBJECT_0)
        return 0;
 

    if ((g_dev_handle[soc_idx]) == INVALID_HANDLE_VALUE)
        goto Exit;

    if (g_count[soc_idx] > 0) {
        g_count[soc_idx]--;
        if (g_count[soc_idx] > 0)
            goto Exit;
    }

    if ((g_dev_handle[soc_idx]) != INVALID_HANDLE_VALUE) {
        CloseHandle((g_dev_handle[soc_idx]));
        (g_dev_handle[soc_idx]) = INVALID_HANDLE_VALUE;
    }
    if (pgDeviceInterfaceDetail[soc_idx] != NULL) {
        free(pgDeviceInterfaceDetail[soc_idx]);
    }
Exit:
    if (ReleaseMutex(ion_mutex))
        return 0;
    else
        return -1;

    return ret;
}

bm_ion_buffer_t* pcie_allocate_buffer(int soc_idx, size_t size, int flags)
{
    DWORD bytesReceived = 0;
    vpudrv_buffer_t sdb = {0};
    sdb.size = (uint32_t)size + 32;
    HANDLE h_dev = g_dev_handle[soc_idx];
    BOOL  status = TRUE;

    if (h_dev == NULL) {
        fprintf(stderr, "[%s:%d] Bad file descriptor! soc_idx=%d, handle=%p, size=%zd\n",
                __func__, __LINE__, soc_idx, h_dev, size);
        return NULL;
    }

    bm_ion_buffer_t* p = (bm_ion_buffer_t*)calloc(1, sizeof(bm_ion_buffer_t));
    if (p == NULL) {
        fprintf(stderr, "[%s:%d] calloc failed!\n", __func__, __LINE__);
        return NULL;
    }
    p->context = (void*)calloc(1, sizeof(vpudrv_buffer_t));
    if (p->context == NULL) {
        fprintf(stderr, "[%s:%d] calloc failed!\n", __func__, __LINE__);
        return NULL;
    }

    status = DeviceIoControl(h_dev,
                             VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY,
                             &sdb,
                             sizeof(vpudrv_buffer_t),
                             &sdb,
                             sizeof(vpudrv_buffer_t),
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl alloc VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY failed 0x%x\n",
               GetLastError());
        return NULL;
    }


    p->devFd   = soc_idx;
    p->soc_idx = soc_idx;
    p->paddr   = sdb.phys_addr &(~31);
    p->size    = (uint32_t)size;

    memcpy(p->context, &sdb, sizeof(vpudrv_buffer_t));

#ifdef ION_PRINT_INFO
    printf("[%s:%d] pcie_buffer soc_idx=%d, fd=%d, pa=0x%lx, size=%d, flags=%d\n",
           __func__, __LINE__, soc_idx, dev_fd, p->paddr, size, flags);

    fprintf(stderr, "[%s:%d] leave\n", __func__, __LINE__);
#endif

    return p;
}

int pcie_free_buffer(bm_ion_buffer_t* p)
{
    DWORD bytesReceived = 0;
    vpudrv_buffer_t sdb = {0};
    BOOL  status = TRUE;

#ifdef ION_PRINT_INFO
    fprintf(stderr, "[%s:%d] enter\n", __func__, __LINE__);
#endif

    if (!p) {
        fprintf(stderr, "[%s:%d] ion buffer is null!\n", __func__, __LINE__);
        return -1;
    }
    if (p->devFd < 0) {
        fprintf(stderr, "[%s:%d] Please open /dev/bm-sophonxx first!\n", __func__, __LINE__);
        return -1;
    }
    HANDLE h_dev = g_dev_handle[p->devFd];
    if (p->context == NULL) {
        fprintf(stderr, "[%s:%d] context is NULL!\n", __func__, __LINE__);
        return -1;
    }

    memcpy(&sdb, p->context, sizeof(vpudrv_buffer_t));
    status = DeviceIoControl(h_dev,
                             VDI_IOCTL_FREE_PHYSICALMEMORY,
                             &sdb,
                             sizeof(vpudrv_buffer_t),
                             &sdb,
                             sizeof(vpudrv_buffer_t),
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl free VDI_IOCTL_FREE_PHYSICALMEMORY failed 0x%x\n",
               GetLastError());
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
    DWORD bytesReceived = 0;
    transfer_opt_t opt = {0};
    BOOL  status = TRUE;
    if (!p){
        fprintf(stderr, "[%s:%d] ion buffer is null!\n", __func__, __LINE__);
        return -1;
    }
    if (p->devFd < 0) {
        fprintf(stderr, "[%s:%d] Please open /dev/bm-sophonxx first!\n", __func__, __LINE__);
        return -1;
    }
    HANDLE h_dev = g_dev_handle[p->devFd];

    opt.src  = p->paddr;
    opt.dst  = (uint64_t)host_va;
    opt.size = (uint32_t)size;
    status = DeviceIoControl(h_dev,
                             VDI_IOCTL_READ_VMEM,
                             &opt,
                             sizeof(transfer_opt_t),
                             NULL,
                             0,
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl VDI_IOCTL_READ_VMEM failed 0x%x\n",
               GetLastError());
        return -1;
    }
    return 0;
}

int pcie_upload_data(uint8_t* host_va, bm_ion_buffer_t* p, size_t size)
{
    DWORD bytesReceived = 0;
    transfer_opt_t opt = {0};
    BOOL  status = TRUE;
    if (!p)
        return 0;
    if (p->devFd < 0) {
        fprintf(stderr, "[%s:%d] Please open /dev/bm-sophonxx first!\n", __func__, __LINE__);
        return -1;
    }
    HANDLE h_dev = g_dev_handle[p->devFd];

    opt.src  = (uint64_t)host_va;
    opt.dst  = p->paddr;
    opt.size = (uint32_t)size;
    status = DeviceIoControl(h_dev,
                             VDI_IOCTL_WRITE_VMEM,
                             &opt,
                             sizeof(transfer_opt_t),
                             NULL,
                             0,
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl VDI_IOCTL_WRITE_VMEM failed 0x%x\n",
               GetLastError());
        return -1;
    }
    return 0;

}

int pcie_download_data2(int soc_idx, uint64_t src_addr, uint8_t *dst_data, size_t size)
{
    DWORD bytesReceived = 0;
    HANDLE h_dev = g_dev_handle[soc_idx];
    transfer_opt_t opt = {0};
    BOOL  status = TRUE;

    opt.src  = src_addr;
    opt.dst  = (uint64_t)dst_data;
    opt.size = (uint32_t)size;
    status = DeviceIoControl(h_dev,
                             VDI_IOCTL_READ_VMEM,
                             &opt,
                             sizeof(transfer_opt_t),
                             NULL,
                             0,
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl VDI_IOCTL_READ_VMEM failed 0x%x\n",
               GetLastError());
        return -1;
    }

    return 0;
}

int pcie_upload_data2(int soc_idx, uint64_t dst_addr, uint8_t *src_data, size_t size)
{
    DWORD bytesReceived = 0;
    HANDLE h_dev = g_dev_handle[soc_idx];
    transfer_opt_t opt = {0};
    BOOL  status = TRUE;

    if (!src_data)
        return -1;

    opt.src  = (uint64_t)src_data;
    opt.dst  = dst_addr;
    opt.size = (uint32_t)size;
    status = DeviceIoControl(h_dev,
                             VDI_IOCTL_WRITE_VMEM,
                             &opt,
                             sizeof(transfer_opt_t),
                             NULL,
                             0,
                             &bytesReceived,
                             NULL);
    if (status == FALSE) {
        printf("DeviceIoControl VDI_IOCTL_WRITE_VMEM failed 0x%x\n",
               GetLastError());
        return -1;
    }

    return 0;
}

#endif

