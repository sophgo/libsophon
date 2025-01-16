#include <fcntl.h>
#include <unistd.h>
#include"../bmlib_internal.h"
#include "../bmlib_log.h"
#include "../bmlib_memory.h"
#include "../ion.h"
#ifndef USING_CMODEL
int platform_ioctl(bm_handle_t handle, u32 commandID, void*param){
    if(commandID == ION_IOC_HEAP_QUERY)
        return ioctl(handle->ion_fd, commandID, param);
    else
        return ioctl(handle->dev_fd, commandID, param);
}
#endif
