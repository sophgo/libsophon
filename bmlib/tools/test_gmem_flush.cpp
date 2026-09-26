#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bmlib_runtime.h"

#define TEST_SIZE 4096

static int g_pass;
static int g_fail;

static void expect(const char *name, int ok)
{
	if (ok) {
		printf("[PASS] %s\n", name);
		g_pass++;
	} else {
		printf("[FAIL] %s\n", name);
		g_fail++;
	}
}

int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	bm_status_t ret;
	struct bm_misc_info misc;
	bm_device_mem_t mem;
	unsigned long long va = 0;
	unsigned long long pa;
	unsigned char *p;
	unsigned char *host;
	unsigned char *got;
	unsigned int i;
	int dev_id = 0;

	if (argc >= 2)
		dev_id = atoi(argv[1]);

	ret = bm_dev_request(&handle, dev_id);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret=%d\n", ret);
		return -1;
	}

	ret = bm_get_misc_info(handle, &misc);
	if (ret != BM_SUCCESS || misc.pcie_soc_mode != 1) {
		printf("SoC mode only\n");
		bm_dev_free(handle);
		return -1;
	}

	printf("=== test_gmem_flush ===\n");
	printf("chipid=0x%x\n", misc.chipid);

	memset(&mem, 0, sizeof(mem));
	ret = bm_malloc_device_byte(handle, &mem, TEST_SIZE);
	expect("bm_malloc_device_byte", ret == BM_SUCCESS);
	if (ret != BM_SUCCESS) {
		bm_dev_free(handle);
		return -1;
	}

	pa = bm_mem_get_device_addr(mem);
	printf("pa=0x%016llx size=%u\n", pa, TEST_SIZE);

	ret = bm_mem_mmap_device_mem(handle, &mem, &va);
	expect("bm_mem_mmap_device_mem", ret == BM_SUCCESS && va != 0);
	if (ret != BM_SUCCESS || va == 0) {
		bm_free_device(handle, mem);
		bm_dev_free(handle);
		return -1;
	}
	printf("va=0x%016llx\n", va);
	expect("pa != va", pa != va);

	p = (unsigned char *)(uintptr_t)va;
	host = (unsigned char *)malloc(TEST_SIZE);
	got = (unsigned char *)malloc(TEST_SIZE);
	if (!host || !got) {
		printf("malloc host buffer failed\n");
		free(host);
		free(got);
		bm_mem_unmap_device_mem(handle, p, TEST_SIZE);
		bm_free_device(handle, mem);
		bm_dev_free(handle);
		return -1;
	}

	for (i = 0; i < TEST_SIZE; i++) {
		host[i] = (unsigned char)(0x5a + (i & 0xff));
		p[i] = host[i];
	}

	ret = bm_mem_flush_device_mem(handle, &mem);
	expect("flush pa", ret == BM_SUCCESS);

	ret = bm_mem_invalidate_device_mem(handle, &mem);
	expect("invalidate pa", ret == BM_SUCCESS);

	memset(got, 0, TEST_SIZE);
	ret = bm_memcpy_d2s(handle, got, mem);
	expect("d2s after flush", ret == BM_SUCCESS);
	expect("data match after flush", memcmp(got, host, TEST_SIZE) == 0);

	printf("pass=%d fail=%d\n", g_pass, g_fail);

	free(host);
	free(got);
	bm_mem_unmap_device_mem(handle, p, TEST_SIZE);
	bm_free_device(handle, mem);
	bm_dev_free(handle);
	return g_fail ? -1 : 0;
}
