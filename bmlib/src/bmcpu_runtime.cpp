#include <fcntl.h>
#include <stdio.h>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <sys\stat.h>
#include <sys\types.h>
#include <time.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "bmcpu_internal.h"
#include "bmlib_internal.h"
#include "bmlib_ioctl.h"
#include "bmlib_log.h"
#include "bmlib_memory.h"
#include "bmlib_runtime.h"
#include "bmlib_md5.h"


#if !defined(USING_CMODEL) && !defined(SOC_MODE)
bm_status_t bm_load_file(bm_handle_t      handle,
                         char *           file_path,
                         bm_device_mem_t *dev_mem_ptr,
                         unsigned int *   size_ptr) {
    u32         u32FileSize;
    struct stat fileStat;
    u8 *        file_buffer;
    u8 *        file_buffer_verify;
    int         fd;
    int         ret;
    u32         copy_size;
    u32         transfer_count;
    u8 *        pFileName         = (u8 *)file_path;
    int         malloc_device_mem = 0;

    if (NULL == pFileName) {
        bmlib_log(
            BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "file path is NULL!!\n");
        return BM_ERR_PARAM;
    }
    if (NULL == dev_mem_ptr) {
        bmlib_log(
            BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "dev_mem_ptr is NULL!!\n");
        return BM_ERR_PARAM;
    }
#ifdef WIN32
    fd = open((const char *)file_path, _O_RDONLY | _O_BINARY);
#else
    fd = open((const char *)file_path, O_RDONLY);
#endif
    if (fd == -1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "open file %s error!!\n",
                  pFileName);
        return BM_ERR_PARAM;
    }
    if (-1 == fstat(fd, &fileStat)) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "stat file %s error!!\n",
                  pFileName);
        close(fd);
        return BM_ERR_PARAM;
    }
    u32FileSize = fileStat.st_size;
    if (u32FileSize % sizeof(float))
        transfer_count = u32FileSize / sizeof(float) + 1;
    else
        transfer_count = u32FileSize / sizeof(float);
    copy_size   = sizeof(float) * transfer_count;
    file_buffer = (unsigned char *)malloc(copy_size);
    if (!file_buffer) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "malloc host mem for file %s error!!\n",
                  pFileName);

        close(fd);
        return BM_ERR_NOMEM;
    }
    file_buffer_verify = (unsigned char *)malloc(copy_size);
    if (!file_buffer_verify) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "malloc host mem to verify file %s error!!\n",
                  pFileName);
        free(file_buffer);
        close(fd);
        return BM_ERR_NOMEM;
    }
    if (read(fd, file_buffer, u32FileSize) != u32FileSize) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "read file %s error\n",
                  pFileName);
        free(file_buffer_verify);
        free(file_buffer);
        close(fd);
        return BM_ERR_FAILURE;
    }
    close(fd);

    if (dev_mem_ptr->u.device.device_addr == 0) {
        malloc_device_mem = 1;
        ret = bm_malloc_device_byte(handle, dev_mem_ptr, copy_size);
        if (ret != BM_SUCCESS) {
            bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "malloc device mem for file %s error!!\n",
                      pFileName);
            free(file_buffer_verify);
            free(file_buffer);
            return BM_ERR_NOMEM;
        } else {
            bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                      BMLIB_LOG_DEBUG,
                      "load lib to addr %llx\n",
                      (u64)dev_mem_ptr->u.device.device_addr);
        }
    }

    dev_mem_ptr->size = copy_size;
    ret               = bm_memcpy_s2d_poll(handle, *dev_mem_ptr, file_buffer);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "copy from system to device for file %s error, ret = %d\n",
                  pFileName,
                  ret);
        if (malloc_device_mem == 1)
            bm_free_device(handle, *dev_mem_ptr);
        free(file_buffer_verify);
        free(file_buffer);
        return BM_ERR_FAILURE;
    }
    ret = bm_memcpy_d2s_poll(handle, file_buffer_verify, *dev_mem_ptr, copy_size);
    if (ret != BM_SUCCESS) {
        bmlib_log(
            BMCPU_RUNTIME_LOG_TAG,
            BMLIB_LOG_ERROR,
            "copy from device to system to verify file %s error, ret = %d\n",
            pFileName,
            ret);
        if (malloc_device_mem == 1)
            bm_free_device(handle, *dev_mem_ptr);
        free(file_buffer_verify);
        free(file_buffer);
        return BM_ERR_FAILURE;
    }
    if (0 != memcmp(file_buffer, file_buffer_verify, u32FileSize)) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "verify transfer file %s error\n",
                  pFileName);
        if (malloc_device_mem == 1)
            bm_free_device(handle, *dev_mem_ptr);
        free(file_buffer_verify);
        free(file_buffer);
        return BM_ERR_FAILURE;
    }

    if (size_ptr != NULL)
        *size_ptr = u32FileSize;
    free(file_buffer_verify);
    free(file_buffer);

    return BM_SUCCESS;
}

bm_status_t bmcpu_reset_cpu(bm_handle_t handle){
    if (handle == nullptr){
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }

    #ifdef __linux__
    if(0 == platform_ioctl(handle, BMDEV_FORCE_RESET_A53, NULL)){
        return BM_SUCCESS;
    } else {
        return BM_ERR_FAILURE;
    }
    #else
    bmlib_log(BMCPU_RUNTIME_LOG_TAG,
              BMLIB_LOG_ERROR,
              "only linux support reset cpu\n",
              __FILE__,
              __func__,
              __LINE__);
    return BM_ERR_FAILURE;
    #endif
}

#ifdef __linux__
bm_status_t bm_setup_veth(bm_handle_t handle){
    if (handle == nullptr){
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }

    if(0 == platform_ioctl(handle, BMDEV_SETUP_VETH, NULL)){
        return BM_SUCCESS;
    } else {
        return BM_ERR_FAILURE;
    }
}

bm_status_t bm_remove_veth(bm_handle_t handle){
    if (handle == nullptr){
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }

    if(0 == platform_ioctl(handle, BMDEV_RMDRV_VETH, NULL)){
        return BM_SUCCESS;
    } else {
        return BM_ERR_FAILURE;
    }
}

bm_status_t bm_set_ip(bm_handle_t handle, u32 ip) {

    if(0 == platform_ioctl(handle, BMDEV_SET_IP, &ip)){
        return BM_SUCCESS;
    } else {
        return BM_ERR_FAILURE;
    }
}

bm_status_t bm_set_gate(bm_handle_t handle, u32 gate) {

    if(0 == platform_ioctl(handle, BMDEV_SET_GATE, &gate)){
        return BM_SUCCESS;
    } else {
        return BM_ERR_FAILURE;
    }
}

bm_status_t bm_trigger_a53(bm_handle_t handle, int id) {
    bm_status_t ret;
    int delay = 3000;

    ret = bmcpu_set_arm9_fw_mode(handle, FW_MIX_MODE);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "set arm9 fw mode error, ret %d\n",
                  ret);
        return ret;
    }

    if (0 != platform_ioctl(handle, BMDEV_TRIGGER_BMCPU, (void *)&delay)) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu trigger error, ret %d\n",
                  ret);
        return BM_ERR_TIMEOUT;
    }

    if (0 != platform_ioctl(handle, BMDEV_COMM_SET_CARDID, (void *)&id)) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "set card id error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    return ret;
}

bm_status_t bm_connect_a53(bm_handle_t handle, int timeout) {
    int flag, state;

    while (timeout > 0) {
        flag = 0;
        platform_ioctl(
            handle, BMDEV_COMM_CONNECT_STATE, &state);
        if (state == 1) {
            flag = 1;
            break;
        }
        sleep(1);
        timeout--;
    }
    if (flag == 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "connect to card %d failed\n", handle->dev_id + 1);
        return BM_ERR_TIMEOUT;
    }
    sleep(5);

    return BM_SUCCESS;
}

int bm_write_data(bm_handle_t handle, char *buf, int len)
{
    if (handle == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }
    struct sgcpu_comm_data data;

    data.data = buf;
    data.len = len;

    return platform_ioctl(handle, BMDEV_COMM_WRITE, (void *)&data);
}

int bm_read_data(bm_handle_t handle, char *buf, int len)
{
    if (handle == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }
    struct sgcpu_comm_data data;
    int cnt;

    data.data = buf;
    data.len = len;
    cnt = platform_ioctl(handle, BMDEV_COMM_READ, &data);
    if (cnt < 0)
        return -1;

    return cnt;
}

int bm_write_msg(bm_handle_t handle, char *buf, int len)
{
    if (handle == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }
    struct sgcpu_comm_data data;
    data.data = buf;
    data.len = len;

    return platform_ioctl(handle, BMDEV_COMM_WRITE_MSG, (void *)&data);
}

int bm_read_msg(bm_handle_t handle, char *buf, int len)
{
    if (handle == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }
    struct sgcpu_comm_data data;
    int cnt;

    data.data = buf;
    data.len = len;
    cnt = platform_ioctl(handle, BMDEV_COMM_READ_MSG, &data);
    if (cnt < 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "comm read msg cnt error %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return -1;
    }

    return cnt;
}
#endif
bm_status_t bm_query_api_data(bm_handle_t handle,
                              sglib_api_id_t api_id,
                              u64         api_handle,
                              u64 *       data,
                              int         timeout) {
    if (handle == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }
    if (data == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "data is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_PARAM;
    }

#ifdef USING_CMODEL
    /* TODO */
    api_handle = api_handle;
    api_id     = api_id;
    timeout    = timeout;
    return BM_SUCCESS;
#else

    bm_api_data_t api_data = {api_id, api_handle, 0, timeout};
    if (0 == platform_ioctl(handle, BMDEV_QUERY_API_RESULT, &api_data)) {
        *data = api_data.data;
        return BM_SUCCESS;
    } else {
        return BM_ERR_FAILURE;
    }
#endif
}

bm_status_t bm_send_api_ext(bm_handle_t handle,
                            int id,
                            const u8 *  api,
                            u32         size,
                            u64 *       api_handle) {
    auto api_id = static_cast<sglib_api_id_t>(id);
    bm_status_t                        status;
    const u8 *                         api_internal = api;
    u32                                api_size     = size;
    u64                                data;
    u8 *                               pFileName;
    u32                                file_size;
    bm_device_mem_t                    dev_mem;
    bm_api_cpu_load_library_internal_t api_cpu_load_library_internal;
    bm_api_cpu_exec_func_internal_t api_cpu_exec_func_internal;
    u8 md5[16];

    if (handle == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "handle is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_DEVNOTREADY;
    }
    if (api == nullptr) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "api is nullptr %s: %s: %d\n",
                  __FILE__,
                  __func__,
                  __LINE__);
        return BM_ERR_PARAM;
    }
    if (size % sizeof(u32) != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "invalid size = 0x%x!\n",
                  size);
        return BM_ERR_PARAM;
    }
    if (api_id == BM_API_ID_LOAD_LIBRARY) {
        const char *tmp;

        memcpy(&api_cpu_load_library_internal, api, size);
        pFileName = api_cpu_load_library_internal.library_path;

        memset(&dev_mem, 0, sizeof(dev_mem));
        status = bm_load_file(handle, (char *)pFileName, &dev_mem, &file_size);
        if (status != BM_SUCCESS) {
            bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "load file %s error!!\n",
                      pFileName);
            return status;
        }

        tmp = strrchr((const char *)pFileName, (int)'/');
        if (tmp)
            tmp += 1;
        else
            tmp = (const char *)pFileName;

        strncpy((char *)(api_cpu_load_library_internal.library_name), tmp, LIB_MAX_NAME_LEN);
        api_cpu_load_library_internal.library_addr =
            (void *)dev_mem.u.device.device_addr;
        api_cpu_load_library_internal.size = file_size;
        read_md5(pFileName, api_cpu_load_library_internal.md5);
        api_cpu_load_library_internal.obj_handle = -1;
        api_internal = (const u8 *)&api_cpu_load_library_internal;
        api_size     = sizeof(bm_api_cpu_load_library_internal_t);
    } else if (api_id == BM_API_ID_UNLOAD_LIBRARY) {
        const char *tmp;

        memcpy(&api_cpu_load_library_internal, api, size);
        pFileName = api_cpu_load_library_internal.library_path;
        tmp = strrchr((const char *)pFileName, (int)'/');
        if (tmp)
            tmp += 1;
        else
            tmp = (const char *)pFileName;

        strncpy((char *)(api_cpu_load_library_internal.library_name), tmp, LIB_MAX_NAME_LEN);
        api_cpu_load_library_internal.obj_handle = api_cpu_load_library_internal.process_handle;
        api_cpu_load_library_internal.mv_handle = -1;
        api_internal = (const u8 *)&api_cpu_load_library_internal;
        api_size     = sizeof(bm_api_cpu_load_library_internal_t);
    } else if (api_id == BM_API_ID_EXEC_FUNCTION) {
        memcpy(&api_cpu_exec_func_internal, api, size);
        strncpy((char *)api_cpu_exec_func_internal.local_function_name,
                (const char *)api_cpu_exec_func_internal.function_name,
                FUNC_MAX_NAME_LEN);
        memcpy(api_cpu_exec_func_internal.local_function_param,
               api_cpu_exec_func_internal.function_param,
               api_cpu_exec_func_internal.param_size);
        api_internal = (const u8 *)&api_cpu_exec_func_internal;
        api_size     = sizeof(bm_api_cpu_exec_func_internal_t);
    }
    if (api_handle != NULL) {
        bm_profile_record_send_api(handle, api_id);
        bm_api_ext_t bm_api = {api_id, api_internal, api_size, 0};
        if (0 == platform_ioctl(handle, BMDEV_SEND_API_EXT, &bm_api)) {
            *api_handle = bm_api.api_handle;
            status      = BM_SUCCESS;
        } else {
            status = BM_ERR_FAILURE;
        }

        if (api_id == BM_API_ID_LOAD_LIBRARY) {
            if (status == BM_SUCCESS) {
                status = bm_query_api_data(
                    handle, api_id, bm_api.api_handle, &data, -1);
                if (status != BM_SUCCESS) {
                    bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                              BMLIB_LOG_ERROR,
                              "load library query api error ret %d\n",
                              status);
                    bm_free_device(handle, dev_mem);
                    return BM_ERR_FAILURE;
                }
                status = (bm_status_t)(s64)data;
                if (status < 0) {
                    bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                              BMLIB_LOG_ERROR,
                              "load library error ret %d\n",
                              (int)status);
                    bm_free_device(handle, dev_mem);
                    return BM_ERR_FAILURE;
                }
            }
            bm_free_device(handle, dev_mem);
        }
        return status;
    } else {
        return bm_send_api(handle, api_id, api_internal, api_size);
    }
}

bm_status_t bmcpu_set_cpu_status(bm_handle_t handle, bm_cpu_status_t status) {
    if (0 == platform_ioctl(handle, BMDEV_SET_BMCPU_STATUS, &status)) {
        return BM_SUCCESS;
    }
    return BM_ERR_FAILURE;
}

bm_cpu_status_t bmcpu_get_cpu_status(bm_handle_t handle) {
    #ifdef __linux__
    bm_cpu_status_t status;
    int ret;

    ret = platform_ioctl(handle, BMDEV_GET_BMCPU_STATUS, (void *)&status);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get bmcpu status failed!, ret: %d\n", ret);
        status = BMCPU_FAULT;
    }

    return status;
    #else
    bm_cpu_status_t status;

    status = BMCPU_IDLE;
    printf("Now windows do not support get bmcpu status!\n");
    return status;
    #endif
}

#ifdef __linux__
bm_status_t bmcpu_set_arm9_fw_mode(bm_handle_t handle, bm_arm9_fw_mode mode) {
    if (mode > 2) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "invalid arm9 fireware mode, mode %d\n",
                  mode);
        return BM_ERR_FAILURE;
    }
    if (0 == platform_ioctl(handle, BMDEV_SET_FW_MODE, &mode))
        return BM_SUCCESS;
    return BM_ERR_FAILURE;
}

bm_status_t bmcpu_load_boot(bm_handle_t handle, char *boot_file) {
    bm_device_mem_t dev_mem;
    bm_status_t ret;
    struct bm_misc_info misc_info;

    ret = bm_get_misc_info(handle, &misc_info);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get misc info error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    if (misc_info.a53_enable != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "bmcpu is not enable in misc info, %d\n", misc_info.a53_enable);
        return BM_ERR_FAILURE;
    }

    dev_mem.u.device.device_addr = 0x10100000;
    dev_mem.flags.u.mem_type = BM_MEM_TYPE_DEVICE;
    dev_mem.size = 0x2B000;
    ret = bm_load_file(handle, boot_file, &dev_mem, NULL);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu load fip error, ret %d\n",
                  ret);
    }

    return ret;
}

bm_status_t bmcpu_load_kernel(bm_handle_t handle, char *kernel_file) {
    bm_device_mem_t dev_mem;
    bm_status_t ret;
    struct bm_misc_info misc_info;

    ret = bm_get_misc_info(handle, &misc_info);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get misc info error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    if (misc_info.a53_enable != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "bmcpu is not enable in misc info, %d\n", misc_info.a53_enable);
        return BM_ERR_FAILURE;
    }

    dev_mem.u.device.device_addr = 0x310000000;
    dev_mem.flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
    dev_mem.size                 = 0x10000000;
    ret = bm_load_file(handle, kernel_file, &dev_mem, NULL);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu load itb error, ret %d\n",
                  ret);
    }

    return ret;
}
#endif
bm_status_t bmcpu_start_cpu(bm_handle_t handle,
                            char *      boot_file,
                            char *      core_file) {

    bm_api_start_cpu_t  api_start_cpu;
    bm_api_set_log_t    api_set_log;
    u64                 api_handle;
    u64                 data;
    int                 ret;
    bm_device_mem_t     dev_mem;
    struct bm_misc_info misc_info;
    int delay = 5000;

    ret = bm_get_misc_info(handle, &misc_info);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get misc info error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    if (misc_info.a53_enable != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_WARNING,
                  "bmcpu is not enable in misc info, %d\n", misc_info.a53_enable);
        return BM_NOT_SUPPORTED;
    }
    ret = bm_send_api_ext(handle,
                          BM_API_ID_START_CPU,
                          (const u8 *)&api_start_cpu,
                          sizeof(bm_api_start_cpu_t),
                          &api_handle);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    ret = bm_query_api_data(handle, BM_API_ID_START_CPU, api_handle, &data, 3000);
    if (ret == 0)
        return BM_SUCCESS;
    #ifdef __linux__
    bmcpu_set_arm9_fw_mode(handle, FW_PCIE_MODE);
    #endif
    dev_mem.u.device.device_addr = 0x10100000;
    dev_mem.flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
    dev_mem.size                 = 0x2B000;
    ret = bm_load_file(handle, boot_file, &dev_mem, NULL);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu load fip error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    dev_mem.u.device.device_addr = 0x310000000;
    dev_mem.flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
    dev_mem.size                 = 0x10000000;
    ret = bm_load_file(handle, core_file, &dev_mem, NULL);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu load itb error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    if (0 != platform_ioctl(handle, BMDEV_TRIGGER_BMCPU, (void *)&delay)) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu trigger error, ret %d\n",
                  ret);
        bmcpu_set_cpu_status(handle, BMCPU_FAULT);
        return BM_ERR_FAILURE;
    }
    ret = bm_send_api_ext(handle,
                          BM_API_ID_START_CPU,
                          (const u8 *)&api_start_cpu,
                          sizeof(bm_api_start_cpu_t),
                          &api_handle);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu send api error, ret %d\n",
                  ret);
        bmcpu_set_cpu_status(handle, BMCPU_FAULT);
        return BM_ERR_FAILURE;
    }
    ret = bm_query_api_data(
        handle, BM_API_ID_START_CPU, api_handle, &data, 10000);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu error, ret %d\n",
                  ret);
        bmcpu_set_cpu_status(handle, BMCPU_FAULT);
        return BM_ERR_FAILURE;
    }
    bmcpu_sync_time(handle);

    api_set_log.log_addr = 0x320000000;
    api_set_log.log_size = 0x200000;
    api_set_log.log_opt  = 1;
    ret = bm_send_api_ext(handle,
                          BM_API_ID_SET_LOG,
                          (const u8 *)&api_set_log,
                          sizeof(bm_api_set_log_t),
                          &api_handle);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "set log send api error, ret %d\n",
                  ret);
        bmcpu_set_cpu_status(handle, BMCPU_FAULT);
        return BM_ERR_FAILURE;
    }
    ret = bm_query_api_data(handle, BM_API_ID_SET_LOG, api_handle, &data, 10000);
    if (ret == 0) {
        bmcpu_set_cpu_status(handle, BMCPU_RUNNING);
        return BM_SUCCESS;
    } else {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu set log error, ret %d\n",
                  ret);
        bmcpu_set_cpu_status(handle, BMCPU_FAULT);
        return BM_ERR_FAILURE;
    }
}
#ifdef __linux__
bm_status_t bmcpu_start_mix_cpu(bm_handle_t handle,
                            char *      boot_file,
                            char *      core_file) {

    bm_api_start_cpu_t  api_start_cpu;
    u64                 api_handle;
    u64                 data;
    int                 ret;
    bm_device_mem_t     dev_mem;
    struct bm_misc_info misc_info;
    int delay = 3000;

    ret = bm_get_misc_info(handle, &misc_info);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get misc info error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    if (misc_info.a53_enable != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "bmcpu is not enable in misc info, %d\n", misc_info.a53_enable);
        return BM_ERR_FAILURE;
    }
    ret = bm_send_api_ext(handle,
                          BM_API_ID_START_CPU,
                          (const u8 *)&api_start_cpu,
                          sizeof(bm_api_start_cpu_t),
                          &api_handle);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    ret = bm_query_api_data(handle, BM_API_ID_START_CPU, api_handle, &data, 3000);
    if (ret == 0)
        return BM_SUCCESS;

    bmcpu_set_arm9_fw_mode(handle, FW_MIX_MODE);
    dev_mem.u.device.device_addr = 0x10100000;
    dev_mem.flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
    dev_mem.size                 = 0x2B000;
    ret = bm_load_file(handle, boot_file, &dev_mem, NULL);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu load fip error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    dev_mem.u.device.device_addr = 0x310000000;
    dev_mem.flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
    dev_mem.size                 = 0x10000000;
    ret = bm_load_file(handle, core_file, &dev_mem, NULL);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu load itb error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    if (0 != platform_ioctl(handle, BMDEV_TRIGGER_BMCPU, (void *)&delay)) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu trigger error, ret %d\n",
                  ret);
        bmcpu_set_cpu_status(handle, BMCPU_FAULT);
        return BM_ERR_FAILURE;
    }
    ret = bm_send_api_ext(handle,
                          BM_API_ID_START_CPU,
                          (const u8 *)&api_start_cpu,
                          sizeof(bm_api_start_cpu_t),
                          &api_handle);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "start cpu send api error, ret %d\n",
                  ret);
        bmcpu_set_cpu_status(handle, BMCPU_FAULT);
        return BM_ERR_FAILURE;
    }
    sleep(15);
    platform_ioctl(handle, BMDEV_GET_VETH_STATE, &data);
    if (((u32)data) == 0x66668888)
    {
        bmcpu_set_cpu_status(handle, BMCPU_RUNNING);
        return BM_SUCCESS;
    }

    bmcpu_set_cpu_status(handle, BMCPU_FAULT);
    return BM_ERR_FAILURE;
}
#endif
bm_status_t bmcpu_sync_time(bm_handle_t handle) {
    u64               api_handle;
    u64               data;
    int               ret;
    bm_api_set_time_t api_set_time;
    #ifndef _WIN32
    struct timeval    tv;
    struct timezone   tz;

    (void)gettimeofday(&tv, &tz);
    api_set_time.tv_sec  = tv.tv_sec;
    api_set_time.tv_usec = tv.tv_usec;
    api_set_time.tz_minuteswest = tz.tz_minuteswest;
    api_set_time.tz_dsttime     = tz.tz_dsttime;
    #else
    time_t cur_time = time(NULL);
    memset(&api_set_time, 0, sizeof(bm_api_set_time_t));
    api_set_time.tv_sec = cur_time;
    #endif
    ret = bm_send_api_ext(handle,
                          BM_API_ID_SET_TIME,
                          (const u8 *)&api_set_time,
                          sizeof(bm_api_set_time_t),
                          &api_handle);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "set time send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
    ret =
        bm_query_api_data(handle, BM_API_ID_SET_TIME, api_handle, &data, 10000);
    if (ret == 0) {
        if ((s32)data != 0) {
            return BM_ERR_FAILURE;
        }
        else
            return BM_SUCCESS;
    } else {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "sync cpu set time error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
}

int bmcpu_open_process(bm_handle_t handle, unsigned int flags, int timeout) {
    int                   ret;
    u64                   api_handle;
    u64                   api_data;
    bm_api_open_process_t api;
    api.flags = flags;

    ret = bm_send_api_ext(handle, BM_API_ID_OPEN_PROCESS, (u8 *)&api, sizeof(api), &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "open process send api error, ret %d\n",
                  ret);
        return ret;
    }

    ret = bm_query_api_data(handle, BM_API_ID_OPEN_PROCESS, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "open process query api error, ret %d\n",
                  ret);
        return ret;
    }

    ret = (int)(u32)(api_data & 0xffffffff);
    if (ret < 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "open process error ret %d\n",
                  ret);
        return ret;
    }

    return (s32)(api_data & 0xffffffff);
}

bm_status_t bmcpu_load_library(bm_handle_t handle,
                               int         process_handle,
                               char *      library_file,
                               int         timeout) {
    int ret;
    u64 api_handle;
    bm_api_cpu_load_library_t api_load_lib;
    api_load_lib.process_handle = (u64)process_handle;
    api_load_lib.library_path   = (u8 *)library_file;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_LOAD_LIBRARY,
                          (u8 *)&api_load_lib,
                          sizeof(api_load_lib),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

bm_status_t bmcpu_unload_library(bm_handle_t handle,
                               int         process_handle,
                               char *      library_file,
                               int         timeout) {
    int ret;
    u64 api_handle;
    bm_api_cpu_load_library_t api_load_lib;
    api_load_lib.process_handle = (u64)process_handle;
    api_load_lib.library_path   = (u8 *)library_file;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_UNLOAD_LIBRARY,
                          (u8 *)&api_load_lib,
                          sizeof(api_load_lib),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "unload library send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

int bmcpu_exec_function(bm_handle_t  handle,
                        int          process_handle,
                        char *       function_name,
                        void *       function_param,
                        unsigned int param_size,
                        int          timeout) {
    int ret;
    u64 api_handle;
    u64 api_data;
    bm_api_cpu_exec_func_t api_exec_func;
    api_exec_func.process_handle = (u64)process_handle;
    api_exec_func.function_name  = (u8 *)function_name;
    api_exec_func.function_param = (u8 *)function_param;
    api_exec_func.param_size     = param_size;
    api_exec_func.opt            = 0;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_EXEC_FUNCTION,
                          (u8 *)&api_exec_func,
                          sizeof(api_exec_func),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = bm_query_api_data(
        handle, BM_API_ID_EXEC_FUNCTION, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function query api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = (s32)api_data;

    return ret;
}

int bmcpu_exec_function_ext(bm_handle_t  handle,
                            int          process_handle,
                            char *       function_name,
                            void *       function_param,
                            unsigned int param_size,
                            unsigned int opt,
                            int          timeout) {
    int ret;
    u64 api_handle;
    u64 api_data;

    bm_api_cpu_exec_func_t api_exec_func;
    api_exec_func.process_handle = (u64)process_handle;
    api_exec_func.function_name  = (u8 *)function_name;
    api_exec_func.function_param = (u8 *)function_param;
    api_exec_func.param_size     = param_size;
    api_exec_func.opt            = opt;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_EXEC_FUNCTION,
                          (u8 *)&api_exec_func,
                          sizeof(api_exec_func),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = bm_query_api_data(handle, BM_API_ID_EXEC_FUNCTION, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function query api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = (s32)api_data;

    return ret;
}

bm_status_t bmcpu_exec_function_async(bm_handle_t         handle,
                                      int                 process_handle,
                                      char *              function_name,
                                      void *              function_param,
                                      unsigned int        param_size,
                                      unsigned long long *api_handle) {
    int ret;

    bm_api_cpu_exec_func_t api_exec_func;
    api_exec_func.process_handle = (u64)process_handle;
    api_exec_func.function_name  = (u8 *)function_name;
    api_exec_func.function_param = (u8 *)function_param;
    api_exec_func.param_size     = param_size;
    api_exec_func.opt            = 0;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_EXEC_FUNCTION,
                          (u8 *)&api_exec_func,
                          sizeof(api_exec_func),
                          api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "async exec function send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

bm_status_t bmcpu_exec_function_async_ext(bm_handle_t         handle,
                                          int                 process_handle,
                                          char *              function_name,
                                          void *              function_param,
                                          unsigned int        param_size,
                                          unsigned int        opt,
                                          unsigned long long *api_handle) {
    int ret;

    bm_api_cpu_exec_func_t api_exec_func;
    api_exec_func.process_handle = (u64)process_handle;
    api_exec_func.function_name  = (u8 *)function_name;
    api_exec_func.function_param = (u8 *)function_param;
    api_exec_func.param_size     = param_size;
    api_exec_func.opt            = opt;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_EXEC_FUNCTION,
                          (u8 *)&api_exec_func,
                          sizeof(api_exec_func),
                          api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "async exec function send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

int bmcpu_query_exec_function_result(bm_handle_t        handle,
                                     unsigned long long api_handle,
                                     int                timeout) {
    int ret;
    u64 api_data;

    ret = bm_query_api_data(handle, BM_API_ID_EXEC_FUNCTION, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "async exec function query result error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = (s32)api_data;

    return BM_SUCCESS;
}

void *bmcpu_map_phys_addr(bm_handle_t  handle,
                          int          process_handle,
                          void *       phys_addr,
                          unsigned int size,
                          int          timeout) {
    int ret;
    u64 api_handle;
    u64 api_data;

    if (size == 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "map phys addr param error\n");
        return NULL;
    }

    bm_api_cpu_map_addr_t api_map_addr;
    api_map_addr.process_handle = (u64)process_handle;
    api_map_addr.phyaddr        = (u8 *)phys_addr;
    api_map_addr.size           = size;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_MAP_PHY_ADDR,
                          (u8 *)&api_map_addr,
                          sizeof(api_map_addr),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "map phys addr send api error, ret %d\n",
                  ret);
        return NULL;
    }

    ret = bm_query_api_data(handle, BM_API_ID_MAP_PHY_ADDR, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "map phys addr query api error, ret %d\n",
                  ret);
        return NULL;
    }

    if ((s64)api_data < 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "map phys addr error ret %d\n",
                  ret);
        return NULL;
    }

    return (void *)api_data;
}

bm_status_t bmcpu_unmap_phys_addr(bm_handle_t handle,
                                  int         process_handle,
                                  void *      phys_addr,
                                  int         timeout) {
    int ret;
    u64 api_handle;
    u64 api_data;

    bm_api_cpu_map_addr_t api_map_addr;
    api_map_addr.process_handle = (u64)process_handle;
    api_map_addr.phyaddr        = (u8 *)phys_addr;
    api_map_addr.size           = 0;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_MAP_PHY_ADDR,
                          (u8 *)&api_map_addr,
                          sizeof(api_map_addr),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "unmap phys addr send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = bm_query_api_data(handle, BM_API_ID_MAP_PHY_ADDR, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "unmap phys addr query api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    if ((s64)api_data < 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "unmap phys addr error ret %d\n",
                  (bm_status_t)(s64)api_data);
        return BM_ERR_FAILURE;
    }

    return (bm_status_t)(s64)api_data;
}

bm_status_t bmcpu_close_process(bm_handle_t handle,
                                int         process_handle,
                                int         timeout) {
    int                    ret;
    u64                    api_handle;
    u64                    api_data;
    bm_api_close_process_t api_close_process;
    api_close_process.process_handle = (u64)process_handle;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_CLOSE_PROCESS,
                          (u8 *)&api_close_process,
                          sizeof(bm_api_close_process_t),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "close process send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = bm_query_api_data(handle, BM_API_ID_CLOSE_PROCESS, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "close process addr query api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

bm_status_t bmcpu_set_log(bm_handle_t  handle,
                          unsigned int log_level,
                          unsigned int log_to_console,
                          int          timeout) {
    int              ret;
    u64              api_handle;
    u64              api_data;
    bm_api_set_log_t api_set_log;

    if ((log_to_console > 1) || (log_level > 5)) {
        bmlib_log(
            BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "set log param error\n");
        return BM_ERR_PARAM;
    }
    api_set_log.log_addr = 0;
    api_set_log.log_size = 0;
    api_set_log.log_opt  = (log_to_console << 31) + log_level;

    ret = bm_send_api_ext(handle,
                          BM_API_ID_SET_LOG,
                          (const u8 *)&api_set_log,
                          sizeof(bm_api_set_log_t),
                          &api_handle);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "set log send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = bm_query_api_data(handle, BM_API_ID_SET_LOG, api_handle, &api_data, timeout);
    if (ret == 0) {
        return BM_SUCCESS;
    } else {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "set log query api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }
}

bm_status_t bmcpu_get_log(bm_handle_t handle,
                          int         process_handle,
                          char *      log_file,
                          int         timeout) {
    int              ret;
    u64              api_handle;
    u64              api_data;
    bm_api_get_log_t api_get_log;
    char *           log_buffer;
    bm_device_mem_t  dev_mem;
    FILE *           fp;
    u32              log_count;
    u32              log_begin_index;
    u32              max_log_count =
        (0x200000 - LOGLIB_ITEM_MAX_SIZE) / LOGLIB_ITEM_MAX_SIZE;
    u32   i;
    u32   index;
    char *pstr;

    api_get_log.process_handle = (u64)process_handle;
    ret                        = bm_send_api_ext(handle,
                          BM_API_ID_GET_LOG,
                          (u8 *)&api_get_log,
                          sizeof(bm_api_get_log_t),
                          &api_handle);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get log send api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    ret = bm_query_api_data(handle, BM_API_ID_GET_LOG, api_handle, &api_data, timeout);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get log query api error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    log_buffer = (char *)malloc(0x200000);
    if (!log_buffer) {
        bmlib_log(
            BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "malloc log mem error!\n");
        return BM_ERR_NOMEM;
    }

    dev_mem.u.device.device_addr = api_data;
    dev_mem.flags.u.mem_type     = BM_MEM_TYPE_DEVICE;
    dev_mem.size                 = 0x200000;
    ret = bm_memcpy_d2s_partial(handle, log_buffer, dev_mem, dev_mem.size);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get log copy from device to system error, ret = %d\n",
                  ret);
        free(log_buffer);
        return BM_ERR_FAILURE;
    }

    if (log_file != NULL) {
        fp = fopen((const char *)log_file, "w+");
        if (fp == NULL) {
            bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "open file %s error!!\n",
                      log_file);
            return BM_ERR_PARAM;
        }

        log_count = *(u64 *)log_buffer;
        if (log_count >= max_log_count) {
            log_begin_index = log_count % max_log_count;
            log_count       = max_log_count;
        } else {
            log_begin_index = 0;
        }

        for (i = 0; i < log_count; i++) {
            index = (log_begin_index + i) % max_log_count;
            pstr  = log_buffer + LOGLIB_ITEM_MAX_SIZE +
                   (index << LOGLIB_ITEM_MAX_SIZE_SHIFT);
            fprintf(fp, "%s", pstr);
        }
        fclose(fp);
    }

    free(log_buffer);
    return BM_SUCCESS;
}

#else

#define MAX_PROCESS 100
#define MAX_LIBRARY 100
typedef struct {
    int   flag;
    void *handle;
} library_record_t;

typedef struct {
    int              flag;
    library_record_t library_records[MAX_LIBRARY];
} process_record_t;

process_record_t g_process_records[MAX_PROCESS];
#ifndef WIN32
pthread_mutex_t  g_bmcpu_runtime_mutex     = PTHREAD_MUTEX_INITIALIZER;
int              g_bmcpu_runtime_init_flag = 0;

bm_status_t bm_send_api_ext(bm_handle_t handle,
                            int api_id,
                            const u8 *  api,
                            u32         size,
                            u64 *       api_handle) {
    handle     = handle;
    api_id     = api_id;
    api        = api;
    size       = size;
    api_handle = api_handle;

    return BM_SUCCESS;
}

bm_status_t bm_query_api_data(bm_handle_t handle,
                              sglib_api_id_t api_id,
                              u64         api_handle,
                              u64 *       data,
                              int         timeout) {
    handle     = handle;
    api_id     = api_id;
    api_handle = api_handle;
    data       = data;
    timeout    = timeout;

    return BM_SUCCESS;
}

bm_status_t bmcpu_start_cpu(bm_handle_t handle,
                            char *      boot_file,
                            char *      core_file) {
    handle    = handle;
    boot_file = boot_file;
    core_file = core_file;

    return BM_SUCCESS;
}

bm_status_t bmcpu_sync_time(bm_handle_t handle) {
    handle = handle;

    return BM_SUCCESS;
}

int bmcpu_open_process(bm_handle_t handle, unsigned int flags, int timeout) {
    int i;
    int process_handle;

    handle  = handle;
    flags   = flags;
    timeout = timeout;

    (void)pthread_mutex_lock(&g_bmcpu_runtime_mutex);
    if (0 == g_bmcpu_runtime_init_flag) {
        memset(g_process_records, 0, sizeof(g_process_records));
        g_bmcpu_runtime_init_flag = 1;
    }

    for (i = 0; i < MAX_PROCESS; i++) {
        if (0 == g_process_records[i].flag)
            break;
    }
    if (i == MAX_PROCESS) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "process is full\n");
        (void)pthread_mutex_unlock(&g_bmcpu_runtime_mutex);
        return -1;
    }
    process_handle            = i;
    g_process_records[i].flag = 1;
    (void)pthread_mutex_unlock(&g_bmcpu_runtime_mutex);

    return process_handle;
}

bm_status_t bmcpu_load_library(bm_handle_t handle,
                               int         process_handle,
                               char *      library_file,
                               int         timeout) {
    int   i;
    void *library_handle;

    handle  = handle;
    timeout = timeout;

    if (g_process_records[process_handle].flag != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library before open process\n");
        return BM_ERR_FAILURE;
    }

    (void)pthread_mutex_lock(&g_bmcpu_runtime_mutex);
    for (i = 0; i < MAX_LIBRARY; i++) {
        if (0 == g_process_records[process_handle].library_records[i].flag)
            break;
    }
    if (i == MAX_LIBRARY) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "library is full\n");
        (void)pthread_mutex_unlock(&g_bmcpu_runtime_mutex);
        return BM_ERR_FAILURE;
    }

    library_handle = dlopen((char *)library_file, RTLD_NOW | RTLD_GLOBAL);
    if (library_handle == NULL) {
        bmlib_log(
            BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "load library error\n");
        (void)pthread_mutex_unlock(&g_bmcpu_runtime_mutex);
        return BM_ERR_FAILURE;
    }

    g_process_records[process_handle].library_records[i].handle =
        library_handle;
    g_process_records[process_handle].library_records[i].flag = 1;

    (void)pthread_mutex_unlock(&g_bmcpu_runtime_mutex);
    return BM_SUCCESS;
}

int bmcpu_exec_function(bm_handle_t  handle,
                        int          process_handle,
                        char *       function_name,
                        void *       function_param,
                        unsigned int param_size,
                        int          timeout) {
    int   i;
    void *library_handle;
    char *error;
    void *temp;
    int (*func_ptr)(void *, unsigned int) = NULL;
    int ret;

    handle  = handle;
    timeout = timeout;

    if (g_process_records[process_handle].flag != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function before open process\n");
        return BM_ERR_FAILURE;
    }

    for (i = 0; i < MAX_LIBRARY; i++) {
        if (0 == g_process_records[process_handle].library_records[i].flag)
            continue;

        library_handle =
            g_process_records[process_handle].library_records[i].handle;
        (void)dlerror();

        temp = dlsym(library_handle, (char *)function_name);
        if ((error = dlerror()) != NULL) {
            continue;
        }
        if (NULL == temp) {
            continue;
        }
        func_ptr = (int (*)(void *, unsigned int))temp;
        break;
    }

    if (func_ptr == NULL) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function can not find %s\n",
                  function_name);
        return BM_ERR_FAILURE;
    }
    ret = (*func_ptr)(function_param, param_size);
    return ret;
}

int bmcpu_exec_function_ext(bm_handle_t  handle,
                            int          process_handle,
                            char *       function_name,
                            void *       function_param,
                            unsigned int param_size,
                            unsigned int opt,
                            int          timeout) {
    opt = opt;
    return bmcpu_exec_function(handle,
                               process_handle,
                               function_name,
                               function_param,
                               param_size,
                               timeout);
}

typedef struct {
    int (*func_ptr)(void *, unsigned int);
    void *       param_ptr;
    unsigned int param_size;
    int          result;
} func_info_t;

void *bm_exec_thread(void *args) {
    func_info_t *func_info = (func_info_t *)args;

    func_info->result =
        (*(func_info->func_ptr))(func_info->param_ptr, func_info->param_size);
    return (void *)func_info;
}

bm_status_t bmcpu_exec_function_async(bm_handle_t         handle,
                                      int                 process_handle,
                                      char *              function_name,
                                      void *              function_param,
                                      unsigned int        param_size,
                                      unsigned long long *api_handle) {
    int   i;
    void *library_handle;
    char *error;
    void *temp;
    int (*func_ptr)(void *, unsigned int) = NULL;
    int          ret;
    func_info_t *func_info;

    handle     = handle;
    param_size = param_size;

    if (g_process_records[process_handle].flag != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function async before open process\n");
        return BM_ERR_FAILURE;
    }

    for (i = 0; i < MAX_LIBRARY; i++) {
        if (0 == g_process_records[process_handle].library_records[i].flag)
            continue;

        library_handle =
            g_process_records[process_handle].library_records[i].handle;
        (void)dlerror();

        temp = dlsym(library_handle, (char *)function_name);
        if ((error = dlerror()) != NULL) {
            continue;
        }
        if (NULL == temp) {
            continue;
        }
        func_ptr = (int (*)(void *, unsigned int))temp;
        break;
    }

    if (func_ptr == NULL) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function async can not find %s\n",
                  function_name);
        return BM_ERR_FAILURE;
    }

    func_info = (func_info_t *)malloc(sizeof(func_info_t));
    if (func_info == NULL) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function async malloc func info error\n");
        return BM_ERR_FAILURE;
    }
    func_info->func_ptr   = func_ptr;
    func_info->param_ptr  = function_param;
    func_info->param_size = param_size;
    ret                   = pthread_create(
        (pthread_t *)api_handle, NULL, bm_exec_thread, func_info);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "exec function async error\n");
        free(func_info);
        return BM_ERR_FAILURE;
    }

    return BM_SUCCESS;
}

bm_status_t bmcpu_exec_function_async_ext(bm_handle_t         handle,
                                          int                 process_handle,
                                          char *              function_name,
                                          void *              function_param,
                                          unsigned int        param_size,
                                          unsigned int        opt,
                                          unsigned long long *api_handle) {
    opt = opt;
    return bmcpu_exec_function_async(handle,
                                     process_handle,
                                     function_name,
                                     function_param,
                                     param_size,
                                     api_handle);
}

int bmcpu_query_exec_function_result(bm_handle_t        handle,
                                     unsigned long long api_handle,
                                     int                timeout) {
    int   ret;
    void *retval;

    handle  = handle;
    timeout = timeout;

    ret = pthread_join((pthread_t)api_handle, &retval);
    if (ret != 0) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "query exec function result error\n");
        return BM_ERR_FAILURE;
    }

    ret = ((func_info_t *)retval)->result;
    free(retval);
    return ret;
}

void *bmcpu_map_phys_addr(bm_handle_t  handle,
                          int          process_handle,
                          void *       phys_addr,
                          unsigned int size,
                          int          timeout) {
    handle         = handle;
    process_handle = process_handle;
    size           = size;
    timeout        = timeout;

    if (g_process_records[process_handle].flag != 1) {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "map phys addr before open process\n");
        return NULL;
    }

#ifdef USING_CMODEL
    return phys_addr;
#else
    return NULL;
#endif
}

bm_status_t bmcpu_unmap_phys_addr(bm_handle_t handle,
                                  int         process_handle,
                                  void *      phys_addr,
                                  int         timeout) {
    handle         = handle;
    process_handle = process_handle;
    phys_addr      = phys_addr;
    timeout        = timeout;

    return BM_SUCCESS;
}

bm_status_t bmcpu_close_process(bm_handle_t handle,
                                int         process_handle,
                                int         timeout) {
    handle         = handle;
    process_handle = process_handle;
    timeout        = timeout;

    (void)pthread_mutex_lock(&g_bmcpu_runtime_mutex);
    g_process_records[process_handle].flag = 0;
    (void)pthread_mutex_unlock(&g_bmcpu_runtime_mutex);

    return BM_SUCCESS;
}

bm_status_t bmcpu_set_log(bm_handle_t  handle,
                          unsigned int log_level,
                          unsigned int log_to_console,
                          int          timeout) {
    handle         = handle;
    log_level      = log_level;
    log_to_console = log_to_console;
    timeout        = timeout;

    return BM_SUCCESS;
}

bm_status_t bmcpu_get_log(bm_handle_t handle,
                          int         process_handle,
                          char *      log_file,
                          int         timeout) {
    handle         = handle;
    process_handle = process_handle;
    log_file       = log_file;
    timeout        = timeout;

    return BM_SUCCESS;
}
#endif
#endif

