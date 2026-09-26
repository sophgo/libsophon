#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "bmlib_runtime.h"

#define VERIFY_CHUNK (64ULL * 1024ULL)

static unsigned long long parse_size(const char *s)
{
	char *end = NULL;
	double val;
	unsigned long long mul = 1;

	if (s == NULL || s[0] == '\0')
		return 0;

	val = strtod(s, &end);
	if (end == s)
		return 0;

	if (*end == '\0')
		return (unsigned long long)val;

	if (*end == 'G' || *end == 'g')
		mul = 1024ULL * 1024ULL * 1024ULL;
	else if (*end == 'M' || *end == 'm')
		mul = 1024ULL * 1024ULL;
	else if (*end == 'K' || *end == 'k')
		mul = 1024ULL;
	else
		return 0;

	if (*(end + 1) == 'B' || *(end + 1) == 'b')
		end++;
	if (*(end + 1) != '\0')
		return 0;

	return (unsigned long long)(val * (double)mul);
}

static void print_heap_stat(bm_handle_t handle, const char *tag)
{
	bm_status_t ret;
	unsigned int heap_num = 0;
	unsigned int i;
	bm_heap_stat_byte_t heap_stat;

	ret = bm_get_gmem_total_heap_num(handle, &heap_num);
	if (ret != BM_SUCCESS) {
		printf("[%s] get heap num failed, ret=%d\n", tag, ret);
		return;
	}

	printf("[%s] heap_num=%u\n", tag, heap_num);
	for (i = 0; i < heap_num; i++) {
		memset(&heap_stat, 0, sizeof(heap_stat));
		ret = bm_get_gmem_heap_stat_byte_by_id(handle, &heap_stat, i);
		if (ret != BM_SUCCESS) {
			printf("  heap%u: query failed, ret=%d\n", i, ret);
			continue;
		}
		printf("  heap%u: total=%.2f GB, avail=%.2f GB, used=%.2f GB, start=0x%llx\n",
		       i,
		       (double)heap_stat.mem_total / (1024.0 * 1024.0 * 1024.0),
		       (double)heap_stat.mem_avail / (1024.0 * 1024.0 * 1024.0),
		       (double)heap_stat.mem_used / (1024.0 * 1024.0 * 1024.0),
		       (unsigned long long)heap_stat.mem_start_addr);
	}
}

static int cmp_buf(const unsigned char *a, const unsigned char *b, unsigned long long n)
{
	unsigned long long i;
	for (i = 0; i < n; i++) {
		if (a[i] != b[i]) {
			printf("cmp fail idx=%llu exp=0x%x got=0x%x\n",
			       i, a[i], b[i]);
			return -1;
		}
	}
	return 0;
}

static int verify_region(bm_handle_t handle, unsigned long long base,
			 unsigned long long offset, unsigned long long chunk,
			 unsigned char pattern)
{
	bm_status_t ret;
	bm_device_mem_u64_t sub;
	unsigned char *src = NULL;
	unsigned char *dst = NULL;
	unsigned long long i;
	struct timeval t1, t2, timediff;
	unsigned long long us;

	src = (unsigned char *)malloc(chunk);
	dst = (unsigned char *)malloc(chunk);
	if (src == NULL || dst == NULL) {
		printf("host malloc %llu failed\n", chunk);
		free(src);
		free(dst);
		return -1;
	}

	for (i = 0; i < chunk; i++)
		src[i] = (unsigned char)((pattern + i) & 0xff);
	memset(dst, 0, chunk);

	sub = bm_mem_from_device_u64(base + offset, chunk);

	gettimeofday(&t1, NULL);
	ret = bm_memcpy_s2d_u64(handle, sub, src);
	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &timediff);
	us = (unsigned long long)timediff.tv_sec * 1000000ULL + timediff.tv_usec;
	if (ret != BM_SUCCESS) {
		printf("s2d fail offset=0x%llx size=%llu ret=%d\n", offset, chunk, ret);
		free(src);
		free(dst);
		return -1;
	}
	printf("s2d ok: offset=0x%llx size=%llu pattern=0x%02x cost=%llu us\n",
	       offset, chunk, pattern, us);

	gettimeofday(&t1, NULL);
	ret = bm_memcpy_d2s_u64(handle, dst, sub);
	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &timediff);
	us = (unsigned long long)timediff.tv_sec * 1000000ULL + timediff.tv_usec;
	if (ret != BM_SUCCESS) {
		printf("d2s fail offset=0x%llx size=%llu ret=%d\n", offset, chunk, ret);
		free(src);
		free(dst);
		return -1;
	}
	printf("d2s ok: offset=0x%llx size=%llu cost=%llu us\n", offset, chunk, us);

	if (cmp_buf(src, dst, chunk) != 0) {
		printf("verify fail: offset=0x%llx size=%llu\n", offset, chunk);
		free(src);
		free(dst);
		return -1;
	}
	printf("verify ok: offset=0x%llx size=%llu\n", offset, chunk);

	free(src);
	free(dst);
	return 0;
}

static int sparse_rw_test(bm_handle_t handle, unsigned long long paddr,
			  unsigned long long msize)
{
	unsigned long long chunk = VERIFY_CHUNK;
	unsigned long long offsets[3];
	unsigned char patterns[3] = {0x5a, 0xa5, 0x3c};
	int i;

	if (msize < chunk)
		chunk = msize;

	offsets[0] = 0;
	offsets[1] = (msize / 2) & ~0xfffULL;
	if (offsets[1] + chunk > msize)
		offsets[1] = 0;
	offsets[2] = msize - chunk;

	printf("sparse rw: chunk=%llu points=head/mid/tail\n", chunk);
	for (i = 0; i < 3; i++) {
		if (i > 0 && offsets[i] == offsets[0])
			continue;
		if (i == 2 && offsets[2] == offsets[1])
			continue;
		if (verify_region(handle, paddr, offsets[i], chunk, patterns[i]) != 0)
			return -1;
	}
	return 0;
}

static void print_usage(const char *prog)
{
	printf("Usage: %s [size] [chip] [heap_id]\n", prog);
	printf("  size     alloc size, e.g. 10G / 10240M / 0x280000000 (default: 10G)\n");
	printf("  chip     device id (default: 0)\n");
	printf("  heap_id  -1=any(u64), >=0 use bm_malloc_device_byte_heap_u64 (default: -1)\n");
	printf("Example:\n");
	printf("  %s\n", prog);
	printf("  %s 10G\n", prog);
	printf("  %s 10G 0 0\n", prog);
}

int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	bm_status_t ret;
	bm_device_mem_u64_t dev_mem;
	unsigned long long size = 10ULL * 1024ULL * 1024ULL * 1024ULL;
	int chip = 0;
	int heap_id = -1;
	struct timeval t1, t2, timediff;
	unsigned long long alloc_us = 0;
	unsigned long long free_us = 0;
	unsigned long long paddr = 0;
	unsigned long long msize = 0;
	int rc = 0;

	memset(&dev_mem, 0, sizeof(dev_mem));

	if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
		print_usage(argv[0]);
		return 0;
	}

	if (argc >= 2) {
		size = parse_size(argv[1]);
		if (size == 0) {
			printf("invalid size: %s\n", argv[1]);
			print_usage(argv[0]);
			return -1;
		}
	}
	if (argc >= 3)
		chip = atoi(argv[2]);
	if (argc >= 4)
		heap_id = atoi(argv[3]);

	printf("chip=%d heap_id=%d size=%llu (%.2f GB / 0x%llx)\n",
	       chip, heap_id, size,
	       (double)size / (1024.0 * 1024.0 * 1024.0),
	       size);

	ret = bm_dev_request(&handle, chip);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret=%d\n", ret);
		return -1;
	}

	print_heap_stat(handle, "before");

	gettimeofday(&t1, NULL);
	if (heap_id < 0)
		ret = bm_malloc_device_byte_u64(handle, &dev_mem, size);
	else
		ret = bm_malloc_device_byte_heap_u64(handle, &dev_mem, heap_id, size);
	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &timediff);
	alloc_us = (unsigned long long)timediff.tv_sec * 1000000ULL + timediff.tv_usec;

	if (ret != BM_SUCCESS) {
		printf("malloc failed, ret=%d, cost=%llu us\n", ret, alloc_us);
		bm_dev_free(handle);
		return -1;
	}

	paddr = bm_mem_get_device_addr_u64(dev_mem);
	msize = bm_mem_get_device_size_u64(dev_mem);
	printf("malloc ok: paddr=0x%llx size=%llu (%.2f GB) cost=%llu us (%.3f ms)\n",
	       paddr, msize,
	       (double)msize / (1024.0 * 1024.0 * 1024.0),
	       alloc_us, (double)alloc_us / 1000.0);

	print_heap_stat(handle, "after_alloc");

	if (sparse_rw_test(handle, paddr, msize) != 0) {
		printf("[FAIL] sparse rw verify\n");
		rc = -1;
	}

	gettimeofday(&t1, NULL);
	bm_free_device_u64(handle, dev_mem);
	gettimeofday(&t2, NULL);
	timersub(&t2, &t1, &timediff);
	free_us = (unsigned long long)timediff.tv_sec * 1000000ULL + timediff.tv_usec;
	printf("free ok: cost=%llu us (%.3f ms)\n", free_us, (double)free_us / 1000.0);

	print_heap_stat(handle, "after_free");

	bm_dev_free(handle);
	if (rc == 0)
		printf("[PASS] test_malloc_large\n");
	return rc;
}
