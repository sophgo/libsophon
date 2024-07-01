
#include "jpu_types.h"
#include "jpu_lib.h"
#include "jpu_logging.h"
#include "../jpuapi/jpuapi.h"

int jpu_Init(int32_t device_index)
{
    return JPU_Init(device_index);
}

void jpu_UnInit(int32_t device_index)
{
    JPU_DeInit(device_index);
}

int jpu_GetVersionInfo(uint32_t * verinfo)
{
    return JPU_GetVersionInfo(verinfo);
}

int jpu_IsBusy()
{
    return JPU_IsBusy();
}

void jpu_ClrStatus(uint32_t val)
{
    JPU_ClrStatus(val);
}

uint32_t jpu_GetStatus()
{
    return JPU_GetStatus();
}


int jpu_SWReset()
{
    return JPU_SWReset();
}

int jpu_HWReset()
{
    return JPU_HWReset();
}

void jpu_SetLoggingThreshold(uint32_t threshold)
{
    jpu_set_logging_threshold(threshold);
}
#if 0
int jpu_Lock(int sleep_us)
{
#ifdef _WIN32
    fprintf(stderr, "It is not lmplemented in window mode \n");
#endif
    int ret = jdi_lock(sleep_us);
    if (ret < 0)
        return -1;
    return 0;
}
#endif

void jpu_UnLock()
{
    jdi_unlock();
}

int jpu_DecSetBsPtr(DecHandle handle, uint8_t *data, int data_size)
{
    return JPU_DecSetBsPtr(handle, data, data_size);
}
#if !defined(_WIN32)
int jpu_GetDump()
{
    return JPU_GetDump();
}
#endif
int vpp_Init(int32_t device_index)
{
    return VPP_Init(device_index);
}
