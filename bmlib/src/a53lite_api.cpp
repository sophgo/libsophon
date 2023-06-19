#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <map>

#include "api.h"
#include "bmlib_internal.h"
#include "bmlib_log.h"
#include "bmlib_ioctl.h"
#include "bmlib_memory.h"
#include "bmlib_runtime.h"
#include "bmlib_md5.h"

#define A53LITE_RUNTIME_LOG_TAG "a53lite_runtime"

bm_status_t a53lite_load_file(bm_handle_t handle,
                              const char *file_path,
                              bm_device_mem_t *dev_mem_ptr,
                              unsigned int *size_ptr)
{
    u32 u32FileSize;
    struct stat fileStat;
    u8 *file_buffer;
    u8 *file_buffer_verify;
    int fd;
    int ret;
    u32 copy_size;
    u32 transfer_count;
    u8 *pFileName = (u8 *)file_path;
    int malloc_device_mem = 0;

    if (NULL == pFileName)
    {
        bmlib_log(
            A53LITE_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "file path is NULL!!\n");
        return BM_ERR_PARAM;
    }
    if (NULL == dev_mem_ptr)
    {
        bmlib_log(
            A53LITE_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR, "dev_mem_ptr is NULL!!\n");
        return BM_ERR_PARAM;
    }
#ifdef WIN32
    fd = open((const char *)file_path, _O_RDONLY | _O_BINARY);
#else
    fd = open((const char *)file_path, O_RDONLY);
#endif
    if (fd == -1)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "open file %s error!!\n",
                  pFileName);
        return BM_ERR_PARAM;
    }
    if (-1 == fstat(fd, &fileStat))
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
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
    copy_size = sizeof(float) * transfer_count;
    file_buffer = (unsigned char *)malloc(copy_size);
    if (!file_buffer)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "malloc host mem for file %s error!!\n",
                  pFileName);

        close(fd);
        return BM_ERR_NOMEM;
    }
    file_buffer_verify = (unsigned char *)malloc(copy_size);
    if (!file_buffer_verify)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "malloc host mem to verify file %s error!!\n",
                  pFileName);
        free(file_buffer);
        close(fd);
        return BM_ERR_NOMEM;
    }
    if (read(fd, file_buffer, u32FileSize) != u32FileSize)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "read file %s error\n",
                  pFileName);
        free(file_buffer_verify);
        free(file_buffer);
        close(fd);
        return BM_ERR_FAILURE;
    }
    close(fd);

    if (dev_mem_ptr->u.device.device_addr == 0)
    {
        malloc_device_mem = 1;
        ret = bm_malloc_device_byte(handle, dev_mem_ptr, copy_size);
        if (ret != BM_SUCCESS)
        {
            bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "malloc device mem for file %s error!!\n",
                      pFileName);
            free(file_buffer_verify);
            free(file_buffer);
            return BM_ERR_NOMEM;
        }
        else
        {
            bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                      BMLIB_LOG_DEBUG,
                      "load lib to addr %llx\n",
                      (u64)dev_mem_ptr->u.device.device_addr);
        }
    }

    dev_mem_ptr->size = copy_size;
    ret = bm_memcpy_s2d(handle, *dev_mem_ptr, file_buffer);
    if (ret != BM_SUCCESS)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
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
    ret = bm_memcpy_d2s(handle, file_buffer_verify, *dev_mem_ptr);
    if (ret != BM_SUCCESS)
    {
        bmlib_log(
            A53LITE_RUNTIME_LOG_TAG,
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
    if (0 != memcmp(file_buffer, file_buffer_verify, u32FileSize))
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
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

bm_status_t a53lite_load_module(bm_handle_t handle, const char *module_name, const char *data,
                                size_t length, bm_device_mem_t *dev_mem_ptr)
{
    bm_status_t ret;
    u8 *file_buffer_verify;
    int malloc_device_mem = 0;
    u32 copy_size;
    u32 transfer_count;

    if (length % sizeof(float))
        transfer_count = length / sizeof(float) + 1;
    else
        transfer_count = length / sizeof(float);
    copy_size = sizeof(float) * transfer_count;

    file_buffer_verify = (unsigned char *)malloc(copy_size);
    if (!file_buffer_verify)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "malloc host mem to verify file %s error!!\n",
                  module_name);
        return BM_ERR_NOMEM;
    }

    if (dev_mem_ptr->u.device.device_addr == 0)
    {
        malloc_device_mem = 1;
        ret = bm_malloc_device_byte(handle, dev_mem_ptr, copy_size);
        if (ret != BM_SUCCESS)
        {
            bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "malloc device mem for file %s error!!\n",
                      module_name);
            free(file_buffer_verify);
            return BM_ERR_NOMEM;
        }
        else
        {
            bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                      BMLIB_LOG_DEBUG,
                      "load lib to addr %llx\n",
                      (u64)dev_mem_ptr->u.device.device_addr);
        }
    }

    dev_mem_ptr->size = copy_size;
    ret = bm_memcpy_s2d(handle, *dev_mem_ptr, (void *)data);
    if (ret != BM_SUCCESS)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "copy from system to device for file %s error, ret = %d\n",
                  module_name,
                  ret);
        if (malloc_device_mem == 1)
            bm_free_device(handle, *dev_mem_ptr);
        free(file_buffer_verify);
        return BM_ERR_FAILURE;
    }
    ret = bm_memcpy_d2s(handle, file_buffer_verify, *dev_mem_ptr);
    if (ret != BM_SUCCESS)
    {
        bmlib_log(
            A53LITE_RUNTIME_LOG_TAG,
            BMLIB_LOG_ERROR,
            "copy from device to system to verify file %s error, ret = %d\n",
            module_name,
            ret);
        if (malloc_device_mem == 1)
            bm_free_device(handle, *dev_mem_ptr);
        free(file_buffer_verify);
        return BM_ERR_FAILURE;
    }
    if (0 != memcmp(data, file_buffer_verify, length))
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "verify transfer file %s error\n",
                  module_name);
        if (malloc_device_mem == 1)
            bm_free_device(handle, *dev_mem_ptr);
        free(file_buffer_verify);
        return BM_ERR_FAILURE;
    }
    free(file_buffer_verify);

    return BM_SUCCESS;
}

void show_md5(unsigned char md5[])
{
    for (int i = 0; i < MD5SUM_LEN; i++)
    {
        printf("%02x", md5[i]);
    }
    printf("\n");
}

typedef struct
{
    u8 *lib_path;
    void *lib_addr;
    u32 size;
    u8 lib_name[LIB_MAX_NAME_LEN];
    unsigned char md5[MD5SUM_LEN];
    int cur_rec;
} a53lite_load_lib_t;

tpu_kernel_module_t tpu_kernel_load_module_file(bm_handle_t handle, const char *module_file)
{
#ifdef USING_CMODEL
    return (bm_module *)0x1;
#else
    int ret;
    a53lite_load_lib_t api_load_lib;
    bm_device_mem_t dev_mem;
    u32 file_size;
    tpu_kernel_module_t p_module;
    const char *tmp;
    unsigned char md5_example[MD5SUM_LEN] = {0xf5, 0x0a, 0x39, 0xb8, 0x7f, 0x19, 0xc9, 0x72,
                                             0x3a, 0x31, 0x5b, 0x29, 0x2c, 0xc9, 0xa2, 0xc1};

    p_module = (tpu_kernel_module_t)malloc(sizeof(struct bm_module));
    if (p_module == nullptr)
    {
        printf("can not alloc memory for module: %s\n", module_file);
        return nullptr;
    }

    memset(&api_load_lib, 0, sizeof(api_load_lib));
    if (sizeof(a53lite_load_lib_t) % sizeof(u32) != 0)
    {
        printf("%s: %d invalid size = 0x%lx!\n", __FILE__, __LINE__, sizeof(a53lite_load_lib_t));
        return nullptr;
    }

    memset(&dev_mem, 0, sizeof(dev_mem));
    ret = a53lite_load_file(handle, module_file, &dev_mem, &file_size);
    if (ret != BM_SUCCESS)
    {
        printf("%s %d: laod file failed!\n", __FILE__, __LINE__);
        return nullptr;
    }

    tmp = strrchr((const char *)module_file, (int)'/');
    if (tmp)
        tmp += 1;
    else
        tmp = (const char *)module_file;

    if (strlen(tmp) > LIB_MAX_NAME_LEN - 1)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "%s send api error, ret %d, library name len %d too long\n",
                  __func__, ret, strlen(tmp));
        return NULL;
    }
    strncpy((char *)api_load_lib.lib_name, tmp, strlen(tmp));
    api_load_lib.lib_addr = (void *)dev_mem.u.device.device_addr;
    api_load_lib.size = file_size;
    read_md5((unsigned char *)module_file, api_load_lib.md5);

    ret = bm_send_api(handle,
                      BM_API_ID_A53LITE_LOAD_LIB,
                      (u8 *)&api_load_lib,
                      sizeof(api_load_lib));
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library send api error, ret %d\n",
                  ret);
        return NULL;
    }

    ret = bm_sync_api(handle);
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load module file sync api error, ret %d\n",
                  ret);
        return nullptr;
    }
    strncpy(p_module->lib_name, tmp, strlen(tmp));
    memcpy(p_module->md5, api_load_lib.md5, MD5SUM_LEN);

    bm_free_device(handle, dev_mem);
    return p_module;
#endif
}

tpu_kernel_module_t tpu_kernel_load_module_file_key(bm_handle_t handle, const char *module_file, const char *key, int size)
{
#ifdef USING_CMODEL
    return (bm_module*)0x1;
#else
    int ret;
    a53lite_load_lib_t api_load_lib;
    bm_device_mem_t dev_mem;
    u32 file_size;
    tpu_kernel_module_t p_module;
    const char *tmp;
    loaded_lib_t loaded_lib;
    unsigned char md5_example[MD5SUM_LEN] = {0xf5, 0x0a, 0x39, 0xb8, 0x7f, 0x19, 0xc9, 0x72,
                                             0x3a, 0x31, 0x5b, 0x29, 0x2c, 0xc9, 0xa2, 0xc1};

    calc_md5((unsigned char *)key, size, loaded_lib.md5);

    ret = platform_ioctl(handle, BMDEV_LOADED_LIB, &loaded_lib);
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library error, ret %d\n",
                  ret);
        return NULL;
    }

    p_module = (tpu_kernel_module_t)malloc(sizeof(struct bm_module));
    if (p_module == nullptr)
    {
        printf("can not alloc memory for module: %s\n", module_file);
        return nullptr;
    }

    tmp = strrchr((const char *)module_file, (int)'/');
    if (tmp)
        tmp += 1;
    else
        tmp = (const char *)module_file;

    if (strlen(tmp) > LIB_MAX_NAME_LEN - 1)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "%s send api error, ret %d, library name len %d too long\n",
                  __func__, ret, strlen(tmp));
        return NULL;
    }

    if (loaded_lib.loaded == 1)
    {
        strncpy(p_module->lib_name, tmp, strlen(tmp));
        memcpy(p_module->md5, loaded_lib.md5, MD5SUM_LEN);
        return p_module;
    }

    memset(&api_load_lib, 0, sizeof(api_load_lib));
    if (sizeof(a53lite_load_lib_t) % sizeof(u32) != 0)
    {
        printf("%s: %d invalid size = 0x%lx!\n", __FILE__, __LINE__, sizeof(a53lite_load_lib_t));
        return nullptr;
    }

    memset(&dev_mem, 0, sizeof(dev_mem));
    ret = a53lite_load_file(handle, module_file, &dev_mem, &file_size);
    if (ret != BM_SUCCESS)
    {
        printf("%s %d: load file failed!\n", __FILE__, __LINE__);
        return nullptr;
    }

    strncpy((char *)api_load_lib.lib_name, tmp, strlen(tmp));
    api_load_lib.lib_addr = (void *)dev_mem.u.device.device_addr;
    api_load_lib.size = file_size;
    calc_md5((unsigned char *)key, size, api_load_lib.md5);

    ret = bm_send_api(handle,
                      BM_API_ID_A53LITE_LOAD_LIB,
                      (u8 *)&api_load_lib,
                      sizeof(api_load_lib));
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library send api error, ret %d\n",
                  ret);
        return NULL;
    }

    ret = bm_sync_api(handle);
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load module file sync api error, ret %d\n",
                  ret);
        return nullptr;
    }
    strncpy(p_module->lib_name, tmp, strlen(tmp));
    memcpy(p_module->md5, api_load_lib.md5, MD5SUM_LEN);

    bm_free_device(handle, dev_mem);
    return p_module;
#endif
}

tpu_kernel_module_t tpu_kernel_load_module(bm_handle_t handle, const char *data, size_t length)
{
#ifdef USING_CMODEL
    return (bm_module *)0x1;
#else
    int ret;
    a53lite_load_lib_t api_load_lib;
    bm_device_mem_t dev_mem;
    char lib_name[LIB_MAX_NAME_LEN];
    tpu_kernel_module_t p_module;

    memset(&api_load_lib, 0, sizeof(api_load_lib));

    calc_md5((unsigned char *)data, length, api_load_lib.md5);
    // show_md5(api_load_lib.md5);
    sprintf(lib_name, "%x%x%x%x%x%x%x%x%x%x%x%x.so",
            api_load_lib.md5[0], api_load_lib.md5[1], api_load_lib.md5[2], api_load_lib.md5[3],
            api_load_lib.md5[4], api_load_lib.md5[5], api_load_lib.md5[10], api_load_lib.md5[11],
            api_load_lib.md5[12], api_load_lib.md5[13], api_load_lib.md5[14], api_load_lib.md5[15]);
    p_module = (tpu_kernel_module_t)malloc(sizeof(struct bm_module));
    if (p_module == nullptr)
    {
        printf("can not alloc memory for module: %s\n", lib_name);
        return nullptr;
    }

    if (sizeof(a53lite_load_lib_t) % sizeof(u32) != 0)
    {
        printf("%s: %d invalid size = 0x%lx!\n", __FILE__, __LINE__, sizeof(a53lite_load_lib_t));
        return nullptr;
    }

    memset(&dev_mem, 0, sizeof(dev_mem));
    ret = a53lite_load_module(handle, lib_name, data, length, &dev_mem);
    if (ret != BM_SUCCESS)
    {
        printf("%s %d: laod file failed!\n", __FILE__, __LINE__);
        return nullptr;
    }

    strncpy((char *)api_load_lib.lib_name, lib_name, LIB_MAX_NAME_LEN);
    api_load_lib.lib_addr = (void *)dev_mem.u.device.device_addr;
    api_load_lib.size = length;

    ret = bm_send_api(handle,
                      BM_API_ID_A53LITE_LOAD_LIB,
                      (u8 *)&api_load_lib,
                      sizeof(api_load_lib));
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library send api error, ret %d\n",
                  ret);
        return NULL;
    }

    ret = bm_sync_api(handle);
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load module file sync api error, ret %d\n",
                  ret);
        return nullptr;
    }
    strncpy(p_module->lib_name, lib_name, LIB_MAX_NAME_LEN);
    memcpy(p_module->md5, api_load_lib.md5, MD5SUM_LEN);

    bm_free_device(handle, dev_mem);
    return p_module;
#endif
}

typedef struct
{
    tpu_kernel_function_t f_id;
    unsigned char md5[MD5SUM_LEN];
    char func_name[FUNC_MAX_NAME_LEN];
} a53lite_get_func_t;

extern void *cmodel_so_handle_;
std::map<int, void *> func_handle;
int fun_id = 0;
tpu_kernel_function_t tpu_kernel_get_function(bm_handle_t handle, tpu_kernel_module_t module, const char *function)
{
#ifdef USING_CMODEL
    void *tmp;

    fun_id++;
    tmp = dlsym(cmodel_so_handle_, function);
    func_handle[fun_id] = tmp;

    return fun_id;
#else

    a53lite_get_func_t api_get_func;
    int ret;

    if (!module)
    {
        printf("%s %d: null ptr input!\n", __FILE__, __LINE__);
        return -1;
    }

    strncpy(api_get_func.func_name, function, FUNC_MAX_NAME_LEN);
    memcpy(api_get_func.md5, module->md5, MD5SUM_LEN);
    ret = bm_send_api(handle,
                      BM_API_ID_A53LITE_GET_FUNC,
                      (u8 *)&api_get_func,
                      sizeof(api_get_func));
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get function send api error, ret %d\n",
                  ret);
        return -1;
    }
    ret = bm_sync_api(handle);
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "get function sync api error, ret %d\n",
                  ret);
        return -1;
    }

    return api_get_func.f_id;
#endif
}

typedef struct
{
    tpu_kernel_function_t f_id;
    u32 size;
    u8 param[4096];
} api_launch_func_t;

typedef int (*f_ptr)(void *, unsigned int);
bm_status_t tpu_kernel_launch(bm_handle_t handle, tpu_kernel_function_t function, void *args, size_t size)
{
#ifdef USING_CMODEL
    void *tmp = func_handle[function];

    ((f_ptr)tmp)(args, size);
    return BM_SUCCESS;
#else

    bm_status_t ret = BM_SUCCESS;
    u8 *buf = (u8 *)malloc(8 + size);
    memcpy(buf, &function, 4);
    memcpy(buf + 4, &size, sizeof(u32));
    memcpy(buf + 8, args, size);

    ret = bm_send_api(handle,
                      BM_API_ID_A53LITE_LAUNCH_FUNC,
                      (u8 *)buf,
                      8 + size);
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "launch function api error, ret %d\n",
                  ret);
        free(buf);
        return ret;
    }

    ret = bm_sync_api(handle);
    free(buf);
    return ret;
#endif
}

bm_status_t tpu_kernel_launch_async(bm_handle_t handle, tpu_kernel_function_t function, void *args, size_t size)
{
#ifdef USING_CMODEL
    void *tmp = func_handle[function];

    ((f_ptr)tmp)(args, size);
    return BM_SUCCESS;
#else

    bm_status_t ret = BM_SUCCESS;
    u8 *buf = (u8 *)malloc(8 + size);
    memcpy(buf, &function, 4);
    memcpy(buf + 4, &size, sizeof(u32));
    memcpy(buf + 8, args, size);

    ret = bm_send_api(handle,
                      BM_API_ID_A53LITE_LAUNCH_FUNC,
                      (u8 *)buf,
                      8 + size);
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "launch function api error, ret %d\n",
                  ret);
        free(buf);
        return ret;
    }

    free(buf);
    return ret;
#endif
}

bm_status_t tpu_kernel_sync(bm_handle_t handle) {
    return bm_sync_api(handle);
}

bm_status_t tpu_kernel_unload_module(bm_handle_t handle, tpu_kernel_module_t p_module)
{
#ifdef USING_CMODEL
    return BM_SUCCESS;
#else
    a53lite_load_lib_t api_load_lib;
    bm_status_t ret = BM_SUCCESS;

    memcpy(api_load_lib.md5, p_module->md5, 16);
    api_load_lib.cur_rec = 0;
    strncpy((char *)api_load_lib.lib_name, p_module->lib_name, LIB_MAX_NAME_LEN);
    ret = bm_send_api(handle,
                      BM_API_ID_A53LITE_UNLOAD_LIB,
                      (u8 *)&api_load_lib,
                      sizeof(api_load_lib));
    if (ret != 0)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library send api error, ret %d\n",
                  ret);
    }
    ret = bm_sync_api(handle);
    free(p_module);
    return ret;
#endif
}
