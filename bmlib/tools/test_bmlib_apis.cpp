#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"

static int g_fail = 0;
static int g_pass = 0;
static int g_warn = 0;

static void expect_ok(const char *name, bm_status_t ret)
{
	if (ret == BM_SUCCESS) {
		printf("[PASS] %s\n", name);
		g_pass++;
	} else {
		printf("[FAIL] %s ret=%d\n", name, ret);
		g_fail++;
	}
}

static void expect_soft(const char *name, bm_status_t ret)
{
	if (ret == BM_SUCCESS) {
		printf("[PASS] %s\n", name);
		g_pass++;
	} else {
		printf("[OPTIONAL] %s ret=%d (optional)\n", name, ret);
		g_warn++;
	}
}

static int cmp_buf(const unsigned char *a, const unsigned char *b, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		if (a[i] != b[i]) {
			printf("cmp fail idx=%d exp=%x got=%x\n", i, a[i], b[i]);
			return -1;
		}
	}
	return 0;
}

static int test_info(bm_handle_t handle)
{
	unsigned int chipid = 0, core_num = 0, heap_num = 0, card_num = 0, card_id = 0;
	unsigned int chip_num = 0, dev_start = 0;
	int freq = 0, drv = 0, status = 0, dynfreq = 0;
	unsigned int tpu_maxclk = 0, tpu_minclk = 0;
	unsigned int board_temp = 0, chip_temp = 0, tpu_volt = 0;
	unsigned int tpuc = 0, maxp = 0, boardp = 0, fan = 0;
	float tpu_power = 0;
	char sn[64] = {0};
	char board_name[64] = {0};
	struct bm_misc_info misc;
	bm_dev_stat_t stat;
	bm_profile_t profile;
	bm_heap_stat_byte_t heap_stat;
	int count = 0;
	int devid;

	expect_ok("bm_dev_getcount", bm_dev_getcount(&count));
	printf("  device count=%d\n", count);
	expect_ok("bm_dev_query", bm_dev_query(bm_get_devid(handle)));
	devid = bm_get_devid(handle);
	printf("  devid=%d\n", devid);

	expect_ok("bm_get_chipid", bm_get_chipid(handle, &chipid));
	printf("  chipid=0x%x\n", chipid);
	expect_ok("bm_get_misc_info", bm_get_misc_info(handle, &misc));
	printf("  misc.chipid=0x%x pcie_soc_mode=%d\n", misc.chipid, misc.pcie_soc_mode);
	expect_ok("bm_get_tpu_scalar_num", bm_get_tpu_scalar_num(handle, &core_num));
	printf("  core_num=%u\n", core_num);
	expect_ok("bm_get_stat", bm_get_stat(handle, &stat));
	expect_soft("bm_get_clk_tpu_freq", bm_get_clk_tpu_freq(handle, &freq));
	printf("  tpu_freq=%d\n", freq);
	expect_soft("bm_get_driver_version", bm_get_driver_version(handle, &drv));
	expect_soft("bm_get_board_name", bm_get_board_name(handle, board_name));
	expect_soft("bm_get_sn", bm_get_sn(handle, sn));
	expect_soft("bm_get_status", bm_get_status(handle, &status));
	expect_soft("bm_get_tpu_maxclk", bm_get_tpu_maxclk(handle, &tpu_maxclk));
	expect_soft("bm_get_tpu_minclk", bm_get_tpu_minclk(handle, &tpu_minclk));
	expect_soft("bm_get_board_temp", bm_get_board_temp(handle, &board_temp));
	expect_soft("bm_get_chip_temp", bm_get_chip_temp(handle, &chip_temp));
	expect_soft("bm_get_tpu_power", bm_get_tpu_power(handle, &tpu_power));
	expect_soft("bm_get_tpu_volt", bm_get_tpu_volt(handle, &tpu_volt));
	expect_soft("bm_get_tpu_current", bm_get_tpu_current(handle, &tpuc));
	expect_soft("bm_get_board_max_power", bm_get_board_max_power(handle, &maxp));
	expect_soft("bm_get_board_power", bm_get_board_power(handle, &boardp));
	expect_soft("bm_get_fan_speed", bm_get_fan_speed(handle, &fan));
	expect_soft("bm_get_dynfreq_status", bm_get_dynfreq_status(handle, &dynfreq));
	expect_ok("bm_get_gmem_total_heap_num", bm_get_gmem_total_heap_num(handle, &heap_num));
	printf("  heap_num=%u\n", heap_num);
	if (heap_num > 0) {
		memset(&heap_stat, 0, sizeof(heap_stat));
		expect_ok("bm_get_gmem_heap_stat_byte_by_id",
			  bm_get_gmem_heap_stat_byte_by_id(handle, &heap_stat, 0));
		printf("  heap0 total=%llu avail=%llu used=%llu\n",
		       heap_stat.mem_total, heap_stat.mem_avail, heap_stat.mem_used);
	}
	expect_soft("bm_get_card_num", bm_get_card_num(&card_num));
	expect_soft("bm_get_card_id", bm_get_card_id(handle, &card_id));
	if (card_num > 0) {
		expect_soft("bm_get_chip_num_from_card",
			    bm_get_chip_num_from_card(0, &chip_num, &dev_start));
	}
	memset(&profile, 0, sizeof(profile));
	expect_soft("bm_get_profile", bm_get_profile(handle, &profile));
	return 0;
}

static int test_alloc(bm_handle_t handle)
{
	bm_device_mem_t mem;
	bm_device_mem_u64_t mem_u64;
	sg_device_mem_t sg_mem;
	unsigned int heap_id = 0;
	unsigned long long paddr = 0;

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_device_byte", bm_malloc_device_byte(handle, &mem, 0x1000));
	expect_ok("bm_get_gmem_heap_id", bm_get_gmem_heap_id(handle, &mem, &heap_id));
	printf("  heap_id=%u addr=0x%llx\n", heap_id, (unsigned long long)bm_mem_get_device_addr(mem));
	bm_free_device(handle, mem);

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_device_dword", bm_malloc_device_dword(handle, &mem, 256));
	bm_free_device(handle, mem);

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_neuron_device", bm_malloc_neuron_device(handle, &mem, 1, 1, 16, 16));
	bm_free_device(handle, mem);

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_device_byte_heap", bm_malloc_device_byte_heap(handle, &mem, 0, 0x1000));
	bm_free_device(handle, mem);

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_device_byte_heap_mask", bm_malloc_device_byte_heap_mask(handle, &mem, 0x3, 0x1000));
	bm_free_device(handle, mem);

	memset(&mem_u64, 0, sizeof(mem_u64));
	expect_ok("bm_malloc_device_byte_u64", bm_malloc_device_byte_u64(handle, &mem_u64, 0x1000));
	bm_free_device_u64(handle, mem_u64);

	memset(&sg_mem, 0, sizeof(sg_mem));
	expect_ok("sg_malloc_device_byte", sg_malloc_device_byte(handle, &sg_mem, 0x1000));
	sg_free_device(handle, sg_mem);

	expect_ok("bm_malloc_device_mem", bm_malloc_device_mem(handle, &paddr, 0, 0x1000));
	bm_free_device_mem(handle, paddr);
	return 0;
}

static int test_partial(bm_handle_t handle)
{
	bm_device_mem_t mem;
	unsigned char host[64];
	unsigned char got[64];
	int i;

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_device_byte(partial)", bm_malloc_device_byte(handle, &mem, 64));
	for (i = 0; i < 64; i++)
		host[i] = (unsigned char)(0xa0 + i);
	memset(got, 0, sizeof(got));

	expect_ok("bm_memcpy_s2d", bm_memcpy_s2d(handle, mem, host));
	expect_ok("bm_memcpy_d2s", bm_memcpy_d2s(handle, got, mem));
	if (cmp_buf(host, got, 64) != 0)
		g_fail++;
	else {
		printf("[PASS] s2d/d2s full compare\n");
		g_pass++;
	}

	memset(got, 0, sizeof(got));
	expect_ok("bm_memcpy_s2d_partial", bm_memcpy_s2d_partial(handle, mem, host + 8, 16));
	expect_ok("bm_memcpy_d2s_partial", bm_memcpy_d2s_partial(handle, got, mem, 16));
	if (cmp_buf(host + 8, got, 16) != 0)
		g_fail++;
	else {
		printf("[PASS] s2d/d2s partial compare\n");
		g_pass++;
	}

	memset(got, 0, sizeof(got));
	expect_ok("bm_memcpy_s2d_partial_offset",
		  bm_memcpy_s2d_partial_offset(handle, mem, host + 4, 16, 8));
	expect_ok("bm_memcpy_d2s_partial_offset",
		  bm_memcpy_d2s_partial_offset(handle, got, mem, 16, 8));
	if (cmp_buf(host + 4, got, 16) != 0)
		g_fail++;
	else {
		printf("[PASS] s2d/d2s partial_offset compare\n");
		g_pass++;
	}

	bm_free_device(handle, mem);
	return 0;
}

static int test_memset(bm_handle_t handle)
{
	bm_device_mem_t mem;
	unsigned char got[256];
	unsigned char expect[256];
	int value4 = 0x5a5a5a5a;
	unsigned short value2 = 0xb1b1;
	unsigned char value1 = 0xc2;
	int i;

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_device_byte(memset)", bm_malloc_device_byte(handle, &mem, 256));

	expect_ok("bm_memset_device", bm_memset_device(handle, value4, mem));
	memset(got, 0, sizeof(got));
	expect_ok("bm_memcpy_d2s(after memset)", bm_memcpy_d2s(handle, got, mem));
	for (i = 0; i < 256; i++)
		expect[i] = 0x5a;
	if (cmp_buf(expect, got, 256) != 0)
		g_fail++;
	else {
		printf("[PASS] bm_memset_device compare\n");
		g_pass++;
	}

	expect_ok("bm_memset_device_ext mode1", bm_memset_device_ext(handle, &value1, 1, mem));
	memset(got, 0, sizeof(got));
	bm_memcpy_d2s(handle, got, mem);
	for (i = 0; i < 256; i++)
		expect[i] = value1;
	if (cmp_buf(expect, got, 256) != 0)
		g_fail++;
	else {
		printf("[PASS] bm_memset_device_ext mode1 compare\n");
		g_pass++;
	}

	expect_ok("bm_memset_device_ext mode2", bm_memset_device_ext(handle, &value2, 2, mem));
	memset(got, 0, sizeof(got));
	bm_memcpy_d2s(handle, got, mem);
	for (i = 0; i < 256; i += 2) {
		expect[i] = 0xb1;
		expect[i + 1] = 0xb1;
	}
	if (cmp_buf(expect, got, 256) != 0)
		g_fail++;
	else {
		printf("[PASS] bm_memset_device_ext mode2 compare\n");
		g_pass++;
	}

	bm_free_device(handle, mem);
	return 0;
}

static int test_d2d(bm_handle_t handle)
{
	bm_device_mem_t src;
	bm_device_mem_t dst;
	unsigned char host[128];
	unsigned char got[128];
	int i;

	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	expect_ok("bm_malloc_device_byte(d2d src)", bm_malloc_device_byte(handle, &src, 128));
	expect_ok("bm_malloc_device_byte(d2d dst)", bm_malloc_device_byte(handle, &dst, 128));
	for (i = 0; i < 128; i++)
		host[i] = (unsigned char)(i + 1);
	expect_ok("bm_memcpy_s2d(d2d)", bm_memcpy_s2d(handle, src, host));
	expect_ok("bm_memset_device(d2d clear)", bm_memset_device(handle, 0, dst));

	expect_ok("bm_memcpy_d2d_byte", bm_memcpy_d2d_byte(handle, dst, 0, src, 0, 128));
	memset(got, 0, sizeof(got));
	bm_memcpy_d2s(handle, got, dst);
	if (cmp_buf(host, got, 128) != 0)
		g_fail++;
	else {
		printf("[PASS] bm_memcpy_d2d_byte compare\n");
		g_pass++;
	}

	expect_ok("bm_memset_device(d2d clear2)", bm_memset_device(handle, 0, dst));
	expect_ok("bm_memcpy_d2d", bm_memcpy_d2d(handle, dst, 0, src, 0, 32));
	memset(got, 0, sizeof(got));
	bm_memcpy_d2s(handle, got, dst);
	if (cmp_buf(host, got, 128) != 0)
		g_fail++;
	else {
		printf("[PASS] bm_memcpy_d2d compare\n");
		g_pass++;
	}

	expect_ok("bm_memset_device(d2d clear3)", bm_memset_device(handle, 0, dst));
	expect_ok("bm_memcpy_d2d_stride",
		  bm_memcpy_d2d_stride(handle, dst, 1, src, 1, 64, 1));
	memset(got, 0, sizeof(got));
	bm_memcpy_d2s(handle, got, dst);
	if (cmp_buf(host, got, 64) != 0)
		g_fail++;
	else {
		printf("[PASS] bm_memcpy_d2d_stride compare\n");
		g_pass++;
	}

	bm_free_device(handle, src);
	bm_free_device(handle, dst);
	return 0;
}

static int test_mmap(bm_handle_t handle)
{
	bm_device_mem_t mem;
	unsigned long long vaddr = 0;
	unsigned char *p;
	unsigned char got[64];
	int i;

	memset(&mem, 0, sizeof(mem));
	expect_ok("bm_malloc_device_byte(mmap)", bm_malloc_device_byte(handle, &mem, 64));
	if (bm_mem_mmap_device_mem(handle, &mem, &vaddr) != BM_SUCCESS) {
		printf("[OPTIONAL] bm_mem_mmap_device_mem unsupported, skip mmap group\n");
		g_warn++;
		bm_free_device(handle, mem);
		return 0;
	}
	printf("[PASS] bm_mem_mmap_device_mem\n");
	g_pass++;

	p = (unsigned char *)(uintptr_t)vaddr;
	for (i = 0; i < 64; i++)
		p[i] = (unsigned char)(0x11 + i);
	expect_ok("bm_mem_flush_device_mem", bm_mem_flush_device_mem(handle, &mem));
	expect_ok("bm_mem_invalidate_device_mem", bm_mem_invalidate_device_mem(handle, &mem));
	memset(got, 0, sizeof(got));
	expect_ok("bm_memcpy_d2s(after mmap)", bm_memcpy_d2s(handle, got, mem));
	if (cmp_buf(p, got, 64) != 0)
		g_fail++;
	else {
		printf("[PASS] mmap write + d2s compare\n");
		g_pass++;
	}
	expect_ok("bm_mem_unmap_device_mem", bm_mem_unmap_device_mem(handle, (void *)(uintptr_t)vaddr, 64));
	bm_free_device(handle, mem);
	return 0;
}

static int test_sync(bm_handle_t handle)
{
	expect_ok("bm_device_sync", bm_device_sync(handle));
	expect_ok("bm_handle_sync", bm_handle_sync(handle));
	expect_ok("bm_thread_sync", bm_thread_sync(handle));
	expect_ok("bm_handle_sync_from_core", bm_handle_sync_from_core(handle, 0));
	expect_ok("bm_thread_sync_from_core", bm_thread_sync_from_core(handle, 0));
	expect_ok("bm_set_sync_timeout", bm_set_sync_timeout(handle, 1000));
	expect_ok("bm_set_sync_timeout restore", bm_set_sync_timeout(handle, 120000));
	bm_flush(handle);
	printf("[PASS] bm_flush\n");
	g_pass++;
	return 0;
}

static void print_usage(const char *prog)
{
	printf("Usage: %s [chip_num] [group]\n", prog);
	printf("Groups: all|info|alloc|partial|memset|d2d|mmap|sync\n");
	printf("Example: %s 0 all\n", prog);
}

int main(int argc, char *argv[])
{
	int chip_num = 0;
	const char *group = "all";
	bm_handle_t handle = NULL;
	bm_status_t ret;

	if (argc >= 2)
		chip_num = atoi(argv[1]);
	if (argc >= 3)
		group = argv[2];
	if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
		print_usage(argv[0]);
		return 0;
	}

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret=%d\n", ret);
		return -1;
	}
	printf("Create sophon device %d success\n", chip_num);

	if (strcmp(group, "all") == 0 || strcmp(group, "info") == 0)
		test_info(handle);
	if (strcmp(group, "all") == 0 || strcmp(group, "alloc") == 0)
		test_alloc(handle);
	if (strcmp(group, "all") == 0 || strcmp(group, "partial") == 0)
		test_partial(handle);
	if (strcmp(group, "all") == 0 || strcmp(group, "memset") == 0)
		test_memset(handle);
	if (strcmp(group, "all") == 0 || strcmp(group, "d2d") == 0)
		test_d2d(handle);
	if (strcmp(group, "all") == 0 || strcmp(group, "mmap") == 0)
		test_mmap(handle);
	if (strcmp(group, "all") == 0 || strcmp(group, "sync") == 0)
		test_sync(handle);

	if (strcmp(group, "all") != 0 &&
	    strcmp(group, "info") != 0 &&
	    strcmp(group, "alloc") != 0 &&
	    strcmp(group, "partial") != 0 &&
	    strcmp(group, "memset") != 0 &&
	    strcmp(group, "d2d") != 0 &&
	    strcmp(group, "mmap") != 0 &&
	    strcmp(group, "sync") != 0) {
		print_usage(argv[0]);
		bm_dev_free(handle);
		return -1;
	}

	bm_dev_free(handle);
	printf("============================================================\n");
	printf("TOTAL=%d PASS=%d FAIL=%d OPTIONAL=%d\n",
	       g_pass + g_fail + g_warn, g_pass, g_fail, g_warn);
	printf("BMLIB_CASE_SUMMARY pass=%d fail=%d warn=%d\n", g_pass, g_fail, g_warn);
	printf("============================================================\n");
	return g_fail ? -1 : 0;
}
