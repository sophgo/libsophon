#include <fcntl.h>
#include <stdio.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "bmcpu_internal.h"
#include "bmlib_internal.h"
#include "bmlib_ioctl.h"
#include "bmlib_log.h"
#include "bmlib_memory.h"
#include "bmlib_runtime.h"


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
    fd = open((const char *)file_path, O_RDONLY);
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

int bm_write_data(bm_handle_t handle, char *buf, int len) {
    int ret;
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
    ret = platform_ioctl(handle, BMDEV_COMM_WRITE, (void *)&data);
    if (ret != 0)
        bmlib_log(BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                "bmdev common write failed, ioclt ret = %d:%d\n", ret, __LINE__);

    return ret;
}

int bm_read_data(bm_handle_t handle, char *buf, int len) {
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
    if (cnt < 0) {
       bmlib_log(BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
          		"bmdev common read failed, ioclt ret = %d:%d\n", cnt, __LINE__);
       return -1;
    }

    return cnt;
}


bm_status_t bm_query_api_data(bm_handle_t handle,
                              sglib_api_id_t api_id,
                              u64         api_handle,
                              u64 *       data,
                              int         timeout) {
	int ret;

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
	ret = platform_ioctl(handle, BMDEV_QUERY_API_RESULT, &api_data);
    if (ret == 0) {
        *data = api_data.data;
        return BM_SUCCESS;
    } else {
		bmlib_log(BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                  "bmdev query api result error, ioclt ret = %d %d\n", ret, __LINE__);
        return BM_ERR_FAILURE;
    }
#endif
}

bm_status_t bm_send_api_ext(bm_handle_t handle,
                            int id,
                            const u8 *  api,
                            u32         size,
                            u64 *       api_handle) {
    bmlib_log(BMLIB_RUNTIME_LOG_TAG, BMLIB_LOG_DEBUG, "enter %s\n", __func__);
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
	int ret;

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
        simple_hash(pFileName, api_cpu_load_library_internal.md5);
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
        bm_api_ext_t bm_api = {api_id, api_internal, api_size, 0};
		ret = platform_ioctl(handle, BMDEV_SEND_API_RESULT, &bm_api);
        if (ret == 0) {
            *api_handle = bm_api.api_handle;
            status      = BM_SUCCESS;
        } else {
            bmlib_log(BMCPU_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
                  "bmdev send api ext error, ioclt ret = %d %d\n", ret, __LINE__);
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


bm_status_t bmcpu_sync_time(bm_handle_t handle) {
    u64               api_handle;
    u64               data;
    int               ret;
    bm_api_set_time_t api_set_time;
    struct timeval    tv;
    struct timezone   tz;

    (void)gettimeofday(&tv, &tz);
    api_set_time.tv_sec  = tv.tv_sec;
    api_set_time.tv_usec = tv.tv_usec;
    api_set_time.tz_minuteswest = tz.tz_minuteswest;
    api_set_time.tz_dsttime     = tz.tz_dsttime;
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

    ret = bm_query_api_data(handle, BM_API_ID_SET_TIME, api_handle, &data, 10000);
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

    func_info = reinterpret_cast<func_info_t *>(malloc(sizeof(func_info_t)));
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
			reinterpret_cast<pthread_t *>(api_handle), NULL, bm_exec_thread, func_info);
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


