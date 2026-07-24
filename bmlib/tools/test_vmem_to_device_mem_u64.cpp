#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bmlib_runtime.h"

enum mmap_api_type {
	MMAP_CACHE = 0,
	MMAP_CACHE_U64,
	MMAP_NO_CACHE,
	MMAP_NO_CACHE_U64,
	MMAP_API_MAX
};

static const char *mmap_api_name(int type)
{
	switch (type) {
	case MMAP_CACHE:
		return "bm_mem_mmap_device_mem";
	case MMAP_CACHE_U64:
		return "bm_mem_mmap_device_mem_u64";
	case MMAP_NO_CACHE:
		return "bm_mem_mmap_device_mem_no_cache";
	case MMAP_NO_CACHE_U64:
		return "bm_mem_mmap_device_mem_no_cache_u64";
	default:
		return "unknown";
	}
}

static int array_cmp_u8(unsigned char *exp, unsigned char *got, unsigned long long len,
			const char *label)
{
	unsigned long long i;

	for (i = 0; i < len; i++) {
		if (exp[i] != got[i]) {
			printf("%s mismatch at %llu, exp=0x%x got=0x%x\n",
			       label, i, exp[i], got[i]);
			return -1;
		}
	}
	return 0;
}

static int test_one_mmap_api(bm_handle_t handle, int mmap_type,
			     unsigned long long mem_size, unsigned long long offset)
{
	bm_status_t ret;
	bm_device_mem_t dev_buffer;
	bm_device_mem_u64_t dev_buffer_u64;
	bm_device_mem_u64_t dev_from_base;
	bm_device_mem_u64_t dev_from_offset;
	unsigned long long vmem = 0;
	unsigned long long offset_vmem = 0;
	unsigned long long base_addr = 0;
	unsigned long long remain_size;
	unsigned char *host_src = NULL;
	unsigned char *host_dst = NULL;
	unsigned char *map_ptr = NULL;
	unsigned long long i;
	int use_u64;
	int fail = 0;

	memset(&dev_buffer, 0, sizeof(dev_buffer));
	memset(&dev_buffer_u64, 0, sizeof(dev_buffer_u64));
	memset(&dev_from_base, 0, sizeof(dev_from_base));
	memset(&dev_from_offset, 0, sizeof(dev_from_offset));

	use_u64 = (mmap_type == MMAP_CACHE_U64 || mmap_type == MMAP_NO_CACHE_U64);
	if (!use_u64 && mem_size > 0xffffffffULL) {
		printf("[%s] skip, mem_size too large for non-u64 api\n", mmap_api_name(mmap_type));
		return 0;
	}
	if (offset >= mem_size) {
		printf("[%s] invalid param, offset(%llu) >= mem_size(%llu)\n",
		       mmap_api_name(mmap_type), offset, mem_size);
		return -1;
	}

	remain_size = mem_size - offset;
	host_src = (unsigned char *)malloc(remain_size);
	host_dst = (unsigned char *)malloc(remain_size);
	if (!host_src || !host_dst) {
		printf("[%s] malloc host buffer failed\n", mmap_api_name(mmap_type));
		fail = -1;
		goto exit;
	}

	for (i = 0; i < remain_size; i++)
		host_src[i] = (unsigned char)(i + 0x5a + mmap_type);
	memset(host_dst, 0, remain_size);

	printf("\n==== test %s ====\n", mmap_api_name(mmap_type));

	if (use_u64) {
		ret = bm_malloc_device_byte_u64(handle, &dev_buffer_u64, mem_size);
		if (ret != BM_SUCCESS) {
			printf("bm_malloc_device_byte_u64 failed, ret=%d\n", ret);
			fail = -1;
			goto exit;
		}
		base_addr = bm_mem_get_device_addr_u64(dev_buffer_u64);
	} else {
		ret = bm_malloc_device_byte(handle, &dev_buffer, (unsigned int)mem_size);
		if (ret != BM_SUCCESS) {
			printf("bm_malloc_device_byte failed, ret=%d\n", ret);
			fail = -1;
			goto exit;
		}
		base_addr = bm_mem_get_device_addr(dev_buffer);
	}
	printf("malloc ok, device_addr=0x%llx, size=%llu\n", base_addr, mem_size);

	switch (mmap_type) {
	case MMAP_CACHE:
		ret = bm_mem_mmap_device_mem(handle, &dev_buffer, &vmem);
		break;
	case MMAP_CACHE_U64:
		ret = bm_mem_mmap_device_mem_u64(handle, &dev_buffer_u64, &vmem);
		break;
	case MMAP_NO_CACHE:
		ret = bm_mem_mmap_device_mem_no_cache(handle, &dev_buffer, &vmem);
		break;
	case MMAP_NO_CACHE_U64:
		ret = bm_mem_mmap_device_mem_no_cache_u64(handle, &dev_buffer_u64, &vmem);
		break;
	default:
		ret = BM_ERR_PARAM;
		break;
	}
	if (ret != BM_SUCCESS) {
		printf("%s failed, ret=%d\n", mmap_api_name(mmap_type), ret);
		fail = -1;
		goto free_dev;
	}
	printf("mmap ok, vmem=0x%llx\n", vmem);

	ret = bm_mem_vmem_to_device_mem_u64(handle, &vmem, &dev_from_base);
	if (ret != BM_SUCCESS) {
		printf("vmem_to_dmem with base failed, ret=%d\n", ret);
		fail = -1;
		goto unmap;
	}
	if (bm_mem_get_device_addr_u64(dev_from_base) != base_addr ||
	    bm_mem_get_device_size_u64(dev_from_base) != mem_size) {
		printf("base convert mismatch, got addr=0x%llx size=%llu\n",
		       bm_mem_get_device_addr_u64(dev_from_base),
		       bm_mem_get_device_size_u64(dev_from_base));
		fail = -1;
		goto unmap;
	}
	printf("base convert ok, addr=0x%llx, size=%llu\n",
	       bm_mem_get_device_addr_u64(dev_from_base),
	       bm_mem_get_device_size_u64(dev_from_base));

	offset_vmem = vmem + offset;
	ret = bm_mem_vmem_to_device_mem_u64(handle, &offset_vmem, &dev_from_offset);
	if (ret != BM_SUCCESS) {
		printf("vmem_to_dmem with offset failed, ret=%d\n", ret);
		fail = -1;
		goto unmap;
	}
	if (bm_mem_get_device_addr_u64(dev_from_offset) != base_addr + offset ||
	    bm_mem_get_device_size_u64(dev_from_offset) != remain_size) {
		printf("offset convert mismatch, expect addr=0x%llx size=%llu, got addr=0x%llx size=%llu\n",
		       base_addr + offset, remain_size,
		       bm_mem_get_device_addr_u64(dev_from_offset),
		       bm_mem_get_device_size_u64(dev_from_offset));
		fail = -1;
		goto unmap;
	}
	printf("offset convert ok, offset=%llu, addr=0x%llx, size=%llu\n",
	       offset,
	       bm_mem_get_device_addr_u64(dev_from_offset),
	       bm_mem_get_device_size_u64(dev_from_offset));

	map_ptr = (unsigned char *)(uintptr_t)offset_vmem;
	memcpy(map_ptr, host_src, remain_size);

	if (mmap_type == MMAP_CACHE || mmap_type == MMAP_CACHE_U64) {
		ret = bm_mem_flush_device_mem_u64(handle, &dev_from_offset);
		if (ret != BM_SUCCESS) {
			printf("bm_mem_flush_device_mem_u64 failed, ret=%d\n", ret);
			fail = -1;
			goto unmap;
		}
	}

	ret = bm_memcpy_d2s_u64(handle, host_dst, dev_from_offset);
	if (ret != BM_SUCCESS) {
		printf("bm_memcpy_d2s_u64 failed, ret=%d\n", ret);
		fail = -1;
		goto unmap;
	}
	if (array_cmp_u8(host_src, host_dst, remain_size, "d2s_by_offset_dmem") != 0) {
		fail = -1;
		goto unmap;
	}
	printf("data check by offset dmem ok\n");
	printf("[%s] PASS\n", mmap_api_name(mmap_type));

unmap:
	if (use_u64)
		bm_mem_unmap_device_mem_u64(handle, (void *)(uintptr_t)vmem, mem_size);
	else
		bm_mem_unmap_device_mem(handle, (void *)(uintptr_t)vmem, (int)mem_size);
free_dev:
	if (use_u64)
		bm_free_device_u64(handle, dev_buffer_u64);
	else
		bm_free_device(handle, dev_buffer);
exit:
	if (host_src)
		free(host_src);
	if (host_dst)
		free(host_dst);
	return fail;
}

static int test_cross_handle(int devid, unsigned long long mem_size, unsigned long long offset)
{
	bm_handle_t handle_a = NULL;
	bm_handle_t handle_b = NULL;
	bm_status_t ret;
	bm_device_mem_u64_t dev_buffer;
	bm_device_mem_u64_t dev_from_offset;
	unsigned long long vmem = 0;
	unsigned long long offset_vmem = 0;
	unsigned long long base_addr;
	int fail = 0;

	memset(&dev_buffer, 0, sizeof(dev_buffer));
	memset(&dev_from_offset, 0, sizeof(dev_from_offset));

	if (offset >= mem_size) {
		printf("cross-handle invalid param, offset(%llu) >= mem_size(%llu)\n",
		       offset, mem_size);
		return -1;
	}

	printf("\n==== test cross handle lookup ====\n");

	ret = bm_dev_request(&handle_a, devid);
	if (ret != BM_SUCCESS || handle_a == NULL) {
		printf("bm_dev_request handle_a failed, ret=%d\n", ret);
		return -1;
	}
	ret = bm_dev_request(&handle_b, devid);
	if (ret != BM_SUCCESS || handle_b == NULL) {
		printf("bm_dev_request handle_b failed, ret=%d\n", ret);
		bm_dev_free(handle_a);
		return -1;
	}

	ret = bm_malloc_device_byte_u64(handle_a, &dev_buffer, mem_size);
	if (ret != BM_SUCCESS) {
		printf("malloc on handle_a failed, ret=%d\n", ret);
		fail = -1;
		goto free_handle;
	}
	base_addr = bm_mem_get_device_addr_u64(dev_buffer);

	ret = bm_mem_mmap_device_mem_u64(handle_a, &dev_buffer, &vmem);
	if (ret != BM_SUCCESS) {
		printf("mmap on handle_a failed, ret=%d\n", ret);
		fail = -1;
		goto free_dev;
	}

	offset_vmem = vmem + offset;
	ret = bm_mem_vmem_to_device_mem_u64(handle_b, &offset_vmem, &dev_from_offset);
	if (ret != BM_SUCCESS) {
		printf("lookup on handle_b failed, ret=%d\n", ret);
		fail = -1;
		goto unmap;
	}
	if (bm_mem_get_device_addr_u64(dev_from_offset) != base_addr + offset ||
	    bm_mem_get_device_size_u64(dev_from_offset) != mem_size - offset) {
		printf("cross-handle convert mismatch, got addr=0x%llx size=%llu\n",
		       bm_mem_get_device_addr_u64(dev_from_offset),
		       bm_mem_get_device_size_u64(dev_from_offset));
		fail = -1;
		goto unmap;
	}
	printf("cross-handle lookup ok, mmap(handle_a) -> lookup(handle_b), addr=0x%llx size=%llu\n",
	       bm_mem_get_device_addr_u64(dev_from_offset),
	       bm_mem_get_device_size_u64(dev_from_offset));
	printf("[cross handle] PASS\n");

unmap:
	bm_mem_unmap_device_mem_u64(handle_a, (void *)(uintptr_t)vmem, mem_size);
free_dev:
	bm_free_device_u64(handle_a, dev_buffer);
free_handle:
	bm_dev_free(handle_b);
	bm_dev_free(handle_a);
	return fail;
}

int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	bm_status_t ret;
	int devid = 0;
	unsigned long long mem_size = 0x1000;
	unsigned long long offset = 256;
	int i;
	int rc = 0;

	if (argc >= 2)
		devid = atoi(argv[1]);
	if (argc >= 3)
		mem_size = strtoull(argv[2], NULL, 0);
	if (argc >= 4)
		offset = strtoull(argv[3], NULL, 0);

	ret = bm_dev_request(&handle, devid);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret=%d\n", ret);
		return -1;
	}

	printf("test bm_mem_vmem_to_device_mem_u64 with all mmap apis, devid=%d, mem_size=0x%llx, offset=%llu\n",
	       devid, mem_size, offset);

	for (i = 0; i < MMAP_API_MAX; i++) {
		if (test_one_mmap_api(handle, i, mem_size, offset) != 0) {
			rc = -1;
			break;
		}
	}

	bm_dev_free(handle);

	if (rc == 0)
		rc = test_cross_handle(devid, mem_size, offset);

	if (rc == 0)
		printf("\nALL TEST PASS\n");
	else
		printf("\nTEST FAIL\n");

	return rc;
}
