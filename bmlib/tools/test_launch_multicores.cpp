#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "api.h"

static int find_module_path(unsigned int chipid, char *path, size_t path_len, char *key, size_t key_len)
{
	const char *name;
	const char *candidates[4];
	int i;

	if (chipid == BM_CHIP_ID_84X6) {
		name = "libfirmware_core.so";
		candidates[0] = "/lib/firmware/libfirmware_core.so";
		candidates[1] = "/opt/sophon/libsophon-current/lib/tpu_module/libfirmware_core.so";
		candidates[2] = "./libfirmware_core.so";
		candidates[3] = NULL;
	} else {
		name = "libbm1688_kernel_module.so";
		candidates[0] = "/lib/firmware/libbm1688_kernel_module.so";
		candidates[1] = "/opt/sophon/libsophon-current/lib/tpu_module/libbm1688_kernel_module.so";
		candidates[2] = "./libbm1688_kernel_module.so";
		candidates[3] = NULL;
	}

	for (i = 0; candidates[i] != NULL; i++) {
		if (access(candidates[i], F_OK) == 0) {
			snprintf(path, path_len, "%s", candidates[i]);
			snprintf(key, key_len, "%s", name);
			printf("use module: %s, key: %s\n", path, key);
			fflush(stdout);
			return 0;
		}
	}

	printf("failed to find %s\n", name);
	fflush(stdout);
	return -1;
}

static int array_cmp_int(unsigned char *p_exp, unsigned char *p_got, int len)
{
	int idx;
	for (idx = 0; idx < len; idx++) {
		if (p_exp[idx] != p_got[idx]) {
			printf("compare error at %d exp %x got %x\n", idx, p_exp[idx], p_got[idx]);
			return -1;
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int chip_num = 0;
	int transfer_size = 0x1000;
	int param_num = 1;
	unsigned int chipid = 0;
	unsigned int core_num = 0;
	char module_path[512] = {0};
	char module_key[64] = {0};
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;
	tpu_kernel_module_t module = NULL;
	tpu_kernel_function_t func_id = 0;
	bm_device_mem_t src_mem;
	bm_device_mem_t dst_mem;
	bm_api_memcpy_byte_t api;
	tpu_launch_param_t launch_params[2];
	unsigned char *sys_send = NULL;
	unsigned char *sys_recv = NULL;
	int i;

	if (argc >= 2)
		chip_num = atoi(argv[1]);
	if (argc >= 3)
		transfer_size = (int)strtol(argv[2], NULL, 0);
	if (argc >= 4)
		param_num = atoi(argv[3]);

	if (transfer_size <= 0 || (param_num != 1 && param_num != 2)) {
		printf("Usage: %s [chip_num] [size] [param_num]\n", argv[0]);
		printf("Example: %s 0 0x1000 1\n", argv[0]);
		printf("         %s 0 0x1000 2\n", argv[0]);
		return -1;
	}

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	ret = bm_get_chipid(handle, &chipid);
	if (ret != BM_SUCCESS) {
		printf("bm_get_chipid failed, ret = %d\n", ret);
		goto fail;
	}
	bm_get_tpu_scalar_num(handle, &core_num);
	printf("chipid = 0x%x, core_num = %u, size = 0x%x, param_num = %d\n",
	       chipid, core_num, transfer_size, param_num);
	fflush(stdout);

	if (find_module_path(chipid, module_path, sizeof(module_path),
			     module_key, sizeof(module_key)) != 0)
		goto fail;

	printf("loading module by key...\n");
	fflush(stdout);
	module = tpu_kernel_load_module_file_key(handle, module_path, module_key, strlen(module_key));
	if (module == NULL) {
		printf("tpu_kernel_load_module_file_key failed\n");
		goto fail;
	}
	printf("load module done\n");
	fflush(stdout);

	func_id = tpu_kernel_get_function(handle, module, "sg_api_memcpy_byte");
	if (func_id == 0) {
		printf("tpu_kernel_get_function sg_api_memcpy_byte failed\n");
		goto fail;
	}
	printf("func_id = %d\n", func_id);
	fflush(stdout);

	sys_send = (unsigned char *)malloc(transfer_size);
	sys_recv = (unsigned char *)malloc(transfer_size);
	if (!sys_send || !sys_recv) {
		printf("malloc host buffer failed\n");
		goto fail;
	}
	for (i = 0; i < transfer_size; i++)
		sys_send[i] = (unsigned char)(i + 0x5a);
	memset(sys_recv, 0, transfer_size);

	ret = bm_malloc_device_byte(handle, &src_mem, transfer_size);
	if (ret != BM_SUCCESS) {
		printf("malloc src device mem failed, ret = %d\n", ret);
		goto fail;
	}
	ret = bm_malloc_device_byte(handle, &dst_mem, transfer_size);
	if (ret != BM_SUCCESS) {
		printf("malloc dst device mem failed, ret = %d\n", ret);
		bm_free_device(handle, src_mem);
		goto fail;
	}

	ret = bm_memcpy_s2d(handle, src_mem, sys_send);
	if (ret != BM_SUCCESS) {
		printf("bm_memcpy_s2d failed, ret = %d\n", ret);
		goto free_dev;
	}
	memset(sys_recv, 0, transfer_size);
	ret = bm_memcpy_s2d(handle, dst_mem, sys_recv);
	if (ret != BM_SUCCESS) {
		printf("clear dst with bm_memcpy_s2d failed, ret = %d\n", ret);
		goto free_dev;
	}

	api.src_global_offset = bm_mem_get_device_addr(src_mem);
	api.dst_global_offset = bm_mem_get_device_addr(dst_mem);
	api.size = transfer_size;

	memset(launch_params, 0, sizeof(launch_params));
	for (i = 0; i < param_num; i++) {
		launch_params[i].core_id = i;
		launch_params[i].func_id = func_id;
		launch_params[i].param_data = &api;
		launch_params[i].param_size = sizeof(api);
	}

	printf("call tpu_kernel_launch_async_multicores\n");
	fflush(stdout);
	ret = tpu_kernel_launch_async_multicores(handle, launch_params, param_num);
	if (ret != BM_SUCCESS) {
		printf("tpu_kernel_launch_async_multicores failed, ret = %d\n", ret);
		goto free_dev;
	}

	printf("call tpu_kernel_sync\n");
	fflush(stdout);
	ret = tpu_kernel_sync(handle);
	if (ret != BM_SUCCESS) {
		printf("tpu_kernel_sync failed, ret = %d\n", ret);
		goto free_dev;
	}

	ret = bm_memcpy_d2s(handle, sys_recv, dst_mem);
	if (ret != BM_SUCCESS) {
		printf("bm_memcpy_d2s failed, ret = %d\n", ret);
		goto free_dev;
	}

	if (array_cmp_int(sys_send, sys_recv, transfer_size) != 0) {
		printf("data verify failed\n");
		ret = BM_ERR_FAILURE;
		goto free_dev;
	}

	printf("tpu_kernel_launch_async_multicores success\n");
	ret = BM_SUCCESS;

free_dev:
	bm_free_device(handle, src_mem);
	bm_free_device(handle, dst_mem);
fail:
	if (sys_send)
		free(sys_send);
	if (sys_recv)
		free(sys_recv);
	if (module)
		tpu_kernel_free_module(handle, module);
	bm_dev_free(handle);
	return (ret == BM_SUCCESS) ? 0 : -1;
}
