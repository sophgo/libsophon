#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <map>

#ifdef WIN32
#include <io.h>
#include <time.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <sys/time.h>
#endif

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

#ifdef USING_CMODEL
static tpu_kernel_module_t cmodel_load_module(bm_handle_t handle, const char *module_file, int core_id)
{
	auto p_module = (tpu_kernel_module_t)malloc(sizeof(struct bm_module));
	strncpy((char *)p_module->lib_name, module_file, strlen(module_file));
	bm_profile_load_module(handle, p_module, core_id);
	a53lite_load_lib_t api_load_lib = {0};
	(void)bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_LOAD_LIB,
							  (u8 *)&api_load_lib,
							  sizeof(api_load_lib),
							  core_id);
	(void)bm_sync_api_from_core(handle, core_id);
	return p_module;
}
#endif
tpu_kernel_module_t tpu_kernel_load_module_file_to_core(bm_handle_t handle, const char *module_file, int core_id)
{
#ifdef USING_CMODEL
	return cmodel_load_module(handle, module_file, core_id);
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
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "can not alloc memory for module: %s\n", module_file);
		return nullptr;
	}

	memset(&api_load_lib, 0, sizeof(api_load_lib));
	if (sizeof(a53lite_load_lib_t) % sizeof(u32) != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s: %d invalid size = 0x%lx!\n", __FILE__, __LINE__, sizeof(a53lite_load_lib_t));
		free(p_module);
		return nullptr;
	}

	memset(&dev_mem, 0, sizeof(dev_mem));
	ret = a53lite_load_file(handle, module_file, &dev_mem, &file_size);
	if (ret != BM_SUCCESS)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s %d: laod file failed!\n", __FILE__, __LINE__);
		free(p_module);
		return nullptr;
	}

#ifdef __linux__
	tmp = strrchr((const char *)module_file, (int)'/');
#else
	tmp = strrchr((const char *)module_file, (int)'\\');
#endif
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
		free(p_module);
		return NULL;
	}
	strncpy((char *)api_load_lib.lib_name, tmp, strlen(tmp));
	api_load_lib.lib_addr = (void *)dev_mem.u.device.device_addr;
	api_load_lib.size = file_size;
	read_md5((unsigned char *)module_file, api_load_lib.md5);
	// show_md5(api_load_lib.md5);

	ret = bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_LOAD_LIB,
							  (u8 *)&api_load_lib,
							  sizeof(api_load_lib),
							  core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "load library send api error, ret %d\n",
				  ret);
		free(p_module);
		return NULL;
	}

	ret = bm_sync_api_from_core(handle, core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "load module file sync api error, ret %d\n",
				  ret);
		free(p_module);
		return nullptr;
	}
	strncpy(p_module->lib_name, tmp, strlen(tmp));
	memcpy(p_module->md5, api_load_lib.md5, MD5SUM_LEN);

	bm_profile_load_module(handle, p_module, core_id);
	bm_free_device(handle, dev_mem);
	return p_module;
#endif
}

tpu_kernel_module_t tpu_kernel_load_module_file(bm_handle_t handle, const char *module_file)
{
	return tpu_kernel_load_module_file_to_core(handle, module_file, 0);
}

tpu_kernel_module_t tpu_kernel_load_module_file_key_to_core(bm_handle_t handle, const char *module_file, const char *key, int size, int core_id)
{
#ifdef USING_CMODEL
	return cmodel_load_module(handle, module_file, core_id);
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
	loaded_lib.core_id = core_id;

    ret = platform_ioctl(handle, BMDEV_LOADED_LIB, &loaded_lib);
    if (ret != 0) {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "load library error, ret %d  %d\n",
                  ret, __LINE__);
        return NULL;
    }

    p_module = (tpu_kernel_module_t)malloc(sizeof(struct bm_module));
    if (p_module == nullptr)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "can not alloc memory for module: %s\n", module_file);
        return nullptr;
    }

#ifdef __linux__
	tmp = strrchr((const char *)module_file, (int)'/');
#else
	tmp = strrchr((const char *)module_file, (int)'\\');
#endif
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
		free(p_module);
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
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s: %d invalid size = 0x%lx!\n", __FILE__, __LINE__, sizeof(a53lite_load_lib_t));
		free(p_module);
		return nullptr;
	}

	memset(&dev_mem, 0, sizeof(dev_mem));
	ret = a53lite_load_file(handle, module_file, &dev_mem, &file_size);
	if (ret != BM_SUCCESS)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s %d: load file failed!\n", __FILE__, __LINE__);
		free(p_module);
		return nullptr;
	}

	strncpy((char *)api_load_lib.lib_name, tmp, strlen(tmp));
	api_load_lib.lib_addr = (void *)dev_mem.u.device.device_addr;
	api_load_lib.size = file_size;
	calc_md5((unsigned char *)key, size, api_load_lib.md5);
	// show_md5(api_load_lib.md5);
	ret = bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_LOAD_LIB,
							  (u8 *)&api_load_lib,
							  sizeof(api_load_lib),
							  core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "load library send api error, ret %d\n",
				  ret);
		free(p_module);
		return NULL;
	}

	ret = bm_sync_api_from_core(handle, core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "load module file sync api error, ret %d\n",
				  ret);
		free(p_module);
		return nullptr;
	}
	strncpy(p_module->lib_name, tmp, strlen(tmp));
	memcpy(p_module->md5, api_load_lib.md5, MD5SUM_LEN);

	bm_profile_load_module(handle, p_module, core_id);
	bm_free_device(handle, dev_mem);
	return p_module;
#endif
}
tpu_kernel_module_t tpu_kernel_load_module_file_key(bm_handle_t handle, const char *module_file, const char *key, int size)
{
	return tpu_kernel_load_module_file_key_to_core(handle, module_file, key, size, 0);
}

tpu_kernel_module_t tpu_kernel_load_module_to_core(bm_handle_t handle, const char *data, size_t length, int core_id)
{
#ifdef USING_CMODEL
	const char *module_file = "__data__";
	return cmodel_load_module(handle, module_file, core_id);
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
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "can not alloc memory for module: %s\n", lib_name);
		return nullptr;
	}

	if (sizeof(a53lite_load_lib_t) % sizeof(u32) != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s: %d invalid size = 0x%lx!\n", __FILE__, __LINE__, sizeof(a53lite_load_lib_t));
		free(p_module);
		return nullptr;
	}

	memset(&dev_mem, 0, sizeof(dev_mem));
	ret = a53lite_load_module(handle, lib_name, data, length, &dev_mem);
	if (ret != BM_SUCCESS)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s %d: laod file failed!\n", __FILE__, __LINE__);
		free(p_module);
		return nullptr;
	}

	strncpy((char *)api_load_lib.lib_name, lib_name, LIB_MAX_NAME_LEN);
	api_load_lib.lib_addr = (void *)dev_mem.u.device.device_addr;
	api_load_lib.size = length;

	ret = bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_LOAD_LIB,
							  (u8 *)&api_load_lib,
							  sizeof(api_load_lib),
							  core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "load library send api error, ret %d\n",
				  ret);
		free(p_module);
		return NULL;
	}

	ret = bm_sync_api_from_core(handle, core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "load module file sync api error, ret %d\n",
				  ret);
		free(p_module);
		return nullptr;
	}
	strncpy(p_module->lib_name, lib_name, LIB_MAX_NAME_LEN);
	memcpy(p_module->md5, api_load_lib.md5, MD5SUM_LEN);

	bm_profile_load_module(handle, p_module, core_id);
	bm_free_device(handle, dev_mem);
	return p_module;
#endif
}

tpu_kernel_module_t tpu_kernel_load_module(bm_handle_t handle, const char *data, size_t length)
{
	return tpu_kernel_load_module_to_core(handle, data, length, 0);
}

typedef struct
{
	int core_id;
	tpu_kernel_function_t f_id;
	unsigned char md5[MD5SUM_LEN];
	char func_name[FUNC_MAX_NAME_LEN];
} a53lite_get_func_t;

tpu_kernel_function_t tpu_kernel_get_function_from_core(bm_handle_t handle, tpu_kernel_module_t module, const char *function, int core_id)
{
	a53lite_get_func_t api_get_func;
	int ret;

	if (!module)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s %d: null ptr input!\n", __FILE__, __LINE__);
		return -1;
	}

	strncpy(api_get_func.func_name, function, FUNC_MAX_NAME_LEN);
	memcpy(api_get_func.md5, module->md5, MD5SUM_LEN);
	api_get_func.core_id = core_id;
	ret = bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_GET_FUNC,
							  (u8 *)&api_get_func,
							  sizeof(api_get_func),
							  core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "get function send api error, ret %d\n",
				  ret);
		return -1;
	}
	ret = bm_sync_api_from_core(handle, core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "get function sync api error, ret %d\n",
				  ret);
		return -1;
	}
	if (handle->profile)
	{
		bm_profile_func_map(handle, api_get_func.f_id, api_get_func.func_name, 0);
	}
#ifdef USING_CMODEL
	api_get_func.f_id = handle->bm_dev->cmodel_get_last_func_id(core_id);
#endif

	return api_get_func.f_id;
}

tpu_kernel_function_t tpu_kernel_get_function(bm_handle_t handle, tpu_kernel_module_t module, const char *function)
{
	return tpu_kernel_get_function_from_core(handle, module, function, 0);
}

typedef struct
{
	tpu_kernel_function_t f_id;
	u32 size;
	u8 param[4096];
} api_launch_func_t;

typedef int (*f_ptr)(void *, unsigned int);
bm_status_t tpu_kernel_launch_from_core(bm_handle_t handle, tpu_kernel_function_t function, void *args, size_t size, int core_id)
{
	bm_status_t ret = BM_SUCCESS;
	u8 *buf = (u8 *)malloc(8 + size);
	memcpy(buf, &function, 4);
	memcpy(buf + 4, &size, sizeof(u32));
	memcpy(buf + 8, args, size);

	ret = bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_LAUNCH_FUNC,
							  (u8 *)buf,
							  8 + size,
							  core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "launch function api error, ret %d\n",
				  ret);
		free(buf);
		return ret;
	}

	ret = bm_sync_api_from_core(handle, core_id);
	free(buf);
	return ret;
}

bm_status_t tpu_kernel_launch(bm_handle_t handle, tpu_kernel_function_t function, void *args, size_t size)
{
	return tpu_kernel_launch_from_core(handle, function, args, size, 0);
}

bm_status_t tpu_kernel_launch_async_from_core(bm_handle_t handle, tpu_kernel_function_t function, void *args, size_t size, int core_id)
{
	bm_status_t ret = BM_SUCCESS;
	u8 *buf = (u8 *)malloc(8 + size);
	memcpy(buf, &function, 4);
	memcpy(buf + 4, &size, sizeof(u32));
	memcpy(buf + 8, args, size);

	ret = bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_LAUNCH_FUNC,
							  (u8 *)buf,
							  8 + size,
							  core_id);
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
}

// bm_status_t tpu_launch_async_on_multicores(bm_handle_t handle, tpu_launch_param_t *param_list, int param_num)
// {
// 	bm_status_t ret = BM_SUCCESS;

// 	ret = bm_send_api_multicores(handle, BM_API_ID_TPUSCALER_LAUNCH_FUNC, param_list, param_num);
// 	if (ret != 0)
// 	{
// 		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
// 				  BMLIB_LOG_ERROR,
// 				  "launch function api error, ret %d\n",
// 				  ret);
// 		return ret;
// 	}
// 	return ret;
// }

bm_status_t tpu_kernel_launch_async(bm_handle_t handle, tpu_kernel_function_t function, void *args, size_t size)
{
	return tpu_kernel_launch_async_from_core(handle, function, args, size, 0);
}

bm_status_t tpu_kernel_launch_async_multicores(bm_handle_t handle, tpu_launch_param_t *param_list, int param_num)
{
	bm_status_t ret = BM_SUCCESS;
#ifdef USING_CMODEL
	for(int i=0; i<param_num; i++){
		ret = tpu_kernel_launch_async_from_core(handle, param_list[i].func_id, param_list[i].param_data, param_list[i].param_size, param_list[i].core_id);
		if(ret != BM_SUCCESS){
			return ret;
		}
	}
#else
	ret = bm_send_api_multicores(handle, BM_API_ID_TPUSCALER_LAUNCH_FUNC_MULT_CORES, param_list, param_num);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "launch function api error, ret %d\n",
				  ret);
		return ret;
	}
#endif
	return ret;
}

bm_status_t tpu_kernel_launch_sync_multi_cores(
	bm_handle_t handle, const char *func_name,
	const void *api_param, size_t api_size,
	const int *core_list, const int core_num)
{
	bmlib_log(A53LITE_RUNTIME_LOG_TAG, BMLIB_LOG_ERROR,
			  "%s %d: not implemented!\n", __FILE__, __LINE__);
	return BM_ERR_FAILURE;
}

bm_status_t tpu_kernel_sync_from_core(bm_handle_t handle, int core_id)
{
	return bm_sync_api_from_core(handle, core_id);
}

bm_status_t tpu_kernel_sync(bm_handle_t handle)
{
	return bm_sync_api(handle);
}

bm_status_t tpu_kernel_unload_module_from_core(bm_handle_t handle, tpu_kernel_module_t p_module, int core_id)
{
	bm_profile_unload_module(handle, p_module, core_id);
	a53lite_load_lib_t api_load_lib;
	bm_status_t ret = BM_SUCCESS;

	memcpy(api_load_lib.md5, p_module->md5, 16);
	api_load_lib.cur_rec = 0;
	strncpy((char *)api_load_lib.lib_name, p_module->lib_name, LIB_MAX_NAME_LEN);
	ret = bm_send_api_to_core(handle,
							  BM_API_ID_TPUSCALER_UNLOAD_LIB,
							  (u8 *)&api_load_lib,
							  sizeof(api_load_lib),
							  core_id);
	if (ret != 0)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "load library send api error, ret %d\n",
				  ret);
	}
	ret = bm_sync_api_from_core(handle, core_id);
	free(p_module);
	return ret;
}

bm_status_t tpu_kernel_unload_module(bm_handle_t handle, tpu_kernel_module_t p_module)
{
	return tpu_kernel_unload_module_from_core(handle, p_module, 0);
}

bm_status_t tpu_kernel_free_module(bm_handle_t handle, tpu_kernel_module_t p_module)
{
	if (!p_module)
	{
		bmlib_log(A53LITE_RUNTIME_LOG_TAG,
				  BMLIB_LOG_ERROR,
				  "%s %d: null ptr input!\n", __FILE__, __LINE__);
		return BM_ERR_FAILURE;
	}

	free(p_module);
	return BM_SUCCESS;
}
