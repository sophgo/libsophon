/**
 * Usage: test_d2s_normal_perf [size_MB] [iterations]
 *   size_MB: transfer size in MB (default 1 for malloc vs bmmalloc comparison)
 *   iterations: number of D2S runs for averaging (default 10)
 *
 * Example: test_d2s_normal_perf 1 20
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_memory.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/mman.h>
#else
#pragma comment(lib, "libbmlib-static.lib")
#endif

#define D2S_TEST_ITERATIONS 10
#define DEFAULT_SIZE_MB 1
#define COMPARE_SIZE (1024 * 1024)

static int array_cmp_u64(unsigned char *p_exp, unsigned char *p_got, u64 len,
                         const char *info_label)
{
    u64 idx;
    for (idx = 0; idx < len; idx++) {
        if (p_exp[idx] != p_got[idx]) {
            printf("%s error at index %llu exp %x got %x\n",
                   info_label, (unsigned long long)idx, p_exp[idx], p_got[idx]);
            return -1;
        }
    }
    return 0;
}

static int test_d2s_malloc_vs_bmmalloc(bm_handle_t handle, int iterations)
{
    bm_status_t ret = BM_SUCCESS;
    unsigned char *malloc_buf = NULL;
    unsigned char *sys_send_buf = NULL;
    bm_device_mem_t dev_src, dev_dst;
    u64 bm_host_va = 0;
    struct timeval tv_start, tv_end, timediff;
    unsigned long t_malloc_us = 0, t_bmmalloc_us = 0, t_memcpy_us = 0;
    int i;
    u64 size = COMPARE_SIZE;
    int need_munmap = 0;
    int dev_src_ok = 0, dev_dst_ok = 0;

    malloc_buf = (unsigned char *)malloc(size);
    sys_send_buf = (unsigned char *)malloc(size);
    if (!malloc_buf || !sys_send_buf) {
        printf("malloc failed for compare buffers\n");
        ret = BM_ERR_NOMEM;
        goto out;
    }

    for (i = 0; i < (int)size; i++)
        sys_send_buf[i] = (unsigned char)(rand() % 0xff);
    memset(malloc_buf, 0, size);

    ret = bm_malloc_device_byte(handle, &dev_src, (unsigned int)size);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte dev_src failed\n");
        goto out;
    }
    dev_src_ok = 1;

    ret = bm_malloc_device_byte(handle, &dev_dst, (unsigned int)size);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte dev_dst failed\n");
        goto out;
    }
    dev_dst_ok = 1;

    ret = bm_memcpy_s2d(handle, dev_src, sys_send_buf);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d failed\n");
        goto out;
    }

    t_malloc_us = 0;
    for (i = 0; i < iterations; i++) {
        gettimeofday(&tv_start, NULL);
        ret = bm_memcpy_d2s(handle, malloc_buf, dev_src);
        gettimeofday(&tv_end, NULL);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s to malloc failed\n");
            goto out;
        }
        timersub(&tv_end, &tv_start, &timediff);
        t_malloc_us += (unsigned long)(timediff.tv_sec * 1000000 + timediff.tv_usec);
    }
    t_malloc_us /= iterations;

    if (array_cmp_u64(sys_send_buf, malloc_buf, size, "D2S to malloc")) {
        printf("D2S to malloc verify FAILED\n");
        ret = BM_ERR_FAILURE;
        goto out;
    }

    ret = bm_mem_mmap_device_mem(handle, &dev_dst, &bm_host_va);
    if (ret != BM_SUCCESS) {
        printf("bm_mem_mmap_device_mem failed, use malloc result only\n");
        goto out;
    }
    need_munmap = 1;

    ret = bm_memcpy_s2d(handle, dev_src, sys_send_buf);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d refill failed\n");
        goto out;
    }

    t_bmmalloc_us = 0;
    for (i = 0; i < iterations; i++) {
        gettimeofday(&tv_start, NULL);
        ret = bm_memcpy_d2s(handle, (void *)(uintptr_t)bm_host_va, dev_src);
        gettimeofday(&tv_end, NULL);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s to bmmalloc failed\n");
            goto out;
        }
        timersub(&tv_end, &tv_start, &timediff);
        t_bmmalloc_us += (unsigned long)(timediff.tv_sec * 1000000 + timediff.tv_usec);
    }
    t_bmmalloc_us /= iterations;

    if (array_cmp_u64(sys_send_buf, (unsigned char *)(uintptr_t)bm_host_va, size, "D2S to bmmalloc")) {
        printf("D2S to bmmalloc verify FAILED\n");
        ret = BM_ERR_FAILURE;
        goto out;
    }

    t_memcpy_us = 0;
    for (i = 0; i < iterations; i++) {
        gettimeofday(&tv_start, NULL);
        memcpy(malloc_buf, sys_send_buf, size);
        gettimeofday(&tv_end, NULL);
        timersub(&tv_end, &tv_start, &timediff);
        t_memcpy_us += (unsigned long)(timediff.tv_sec * 1000000 + timediff.tv_usec);
    }
    t_memcpy_us /= iterations;

    printf("\n===== D2S 1MB comparison (iterations=%d) =====\n", iterations);
    printf("D2S to malloc:    %lu us, bandwidth=%.2f MB/s\n",
           t_malloc_us, (float)size / (1024.0f * 1024.0f) / (t_malloc_us / 1000000.0f));
    printf("D2S to bmmalloc:  %lu us, bandwidth=%.2f MB/s\n",
           t_bmmalloc_us, (float)size / (1024.0f * 1024.0f) / (t_bmmalloc_us / 1000000.0f));
    printf("memcpy direct:    %lu us, bandwidth=%.2f MB/s\n",
           t_memcpy_us, t_memcpy_us > 0 ? (float)size / (1024.0f * 1024.0f) / (t_memcpy_us / 1000000.0f) : 0.0f);
    printf("--------------------------------------------\n");
    if (t_malloc_us > 0 && t_bmmalloc_us > 0) {
        long diff = (long)t_malloc_us - (long)t_bmmalloc_us;
        float pct = (diff * 100.0f) / (float)t_malloc_us;
        printf("D2S: malloc vs bmmalloc diff %ld us (%.1f%%), %s faster\n",
               diff, pct, (diff > 0) ? "bmmalloc" : "malloc");
    }
    if (t_malloc_us > 0 && t_memcpy_us > 0) {
        long diff = (long)t_malloc_us - (long)t_memcpy_us;
        float pct = (diff * 100.0f) / (float)t_malloc_us;
        printf("D2S malloc vs memcpy diff %ld us (%.1f%%), %s faster\n",
               diff, pct, (diff > 0) ? "memcpy" : "D2S");
    }
    printf("============================================\n\n");

out:
    if (need_munmap && bm_host_va != 0) {
        size_t align_sz = (size + getpagesize() - 1) & ~(getpagesize() - 1);
        munmap((void *)(uintptr_t)bm_host_va, align_sz);
    }
    if (dev_dst_ok)
        bm_free_device(handle, dev_dst);
    if (dev_src_ok)
        bm_free_device(handle, dev_src);
    if (malloc_buf)
        free(malloc_buf);
    if (sys_send_buf)
        free(sys_send_buf);
    return ret == BM_SUCCESS ? 0 : -1;
}

static int test_d2s_normal_perf(u64 transfer_size, int iterations)
{
    bm_handle_t handle = NULL;
    bm_status_t ret = BM_SUCCESS;
    unsigned char *sys_send_buf = NULL;
    unsigned char *sys_recv_buf = NULL;
    bm_device_mem_t dev_buffer, dev_recv_buf;
    int dev_allocated = 0, dev_recv_allocated = 0;
    u64 recv_bmmalloc_va = 0;
    int need_munmap = 0;
    struct timeval tv_start, tv_end, timediff;
    bm_profile_t profile_start, profile_end;
    unsigned long consume_sys_malloc = 0, consume_sys_bmmalloc = 0;
    unsigned long consume_real_malloc = 0, consume_real_bmmalloc = 0;
    int i;
    struct timespec tp;

#ifdef __linux__
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
#else
    clock_gettime(0, &tp);
#endif
    srand((unsigned int)tp.tv_nsec);

    if (transfer_size == 0)
        transfer_size = (u64)DEFAULT_SIZE_MB * 1024 * 1024;
    if (iterations <= 0)
        iterations = D2S_TEST_ITERATIONS;

    printf("D2S normal perf test: size=0x%llx (%llu MB), iterations=%d\n",
           (unsigned long long)transfer_size,
           (unsigned long long)(transfer_size / (1024 * 1024)), iterations);

    sys_send_buf = (unsigned char *)malloc(transfer_size);
    sys_recv_buf = (unsigned char *)malloc(transfer_size);
    if (!sys_send_buf || !sys_recv_buf) {
        printf("malloc failed for size %llu\n", (unsigned long long)transfer_size);
        ret = BM_ERR_NOMEM;
        goto cleanup;
    }

    for (i = 0; i < (int)transfer_size; i++)
        sys_send_buf[i] = (unsigned char)(rand() % 0xff);
    memset(sys_recv_buf, 0, transfer_size);

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret=%d\n", ret);
        goto cleanup;
    }

    {
        struct bm_misc_info misc_info;
        ret = bm_get_misc_info(handle, &misc_info);
        if (ret != BM_SUCCESS) {
            printf("bm_get_misc_info failed, ret=%d\n", ret);
            bm_dev_free(handle);
            if (sys_send_buf) free(sys_send_buf);
            if (sys_recv_buf) free(sys_recv_buf);
            return -1;
        }
        if (misc_info.pcie_soc_mode != 1) {
            printf("This test only supports SoC mode. Current mode: PCIE. Exit.\n");
            bm_dev_free(handle);
            if (sys_send_buf) free(sys_send_buf);
            if (sys_recv_buf) free(sys_recv_buf);
            return -1;
        }
    }

    if (transfer_size > UINT_MAX) {
        printf("transfer_size %llu exceeds UINT_MAX, use bm_malloc_device_byte_u64 for large size\n",
               (unsigned long long)transfer_size);
        ret = BM_ERR_PARAM;
        goto cleanup;
    }
    ret = bm_malloc_device_byte(handle, &dev_buffer, (unsigned int)transfer_size);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte failed, ret=%d\n", ret);
        goto cleanup;
    }
    dev_allocated = 1;

    ret = bm_malloc_device_byte(handle, &dev_recv_buf, (unsigned int)transfer_size);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte dev_recv_buf failed, ret=%d\n", ret);
        goto cleanup;
    }
    dev_recv_allocated = 1;

    ret = bm_memcpy_s2d(handle, dev_buffer, sys_send_buf);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d failed, ret=%d\n", ret);
        goto cleanup;
    }

    consume_sys_malloc = 0;
    consume_real_malloc = 0;
    for (i = 0; i < iterations; i++) {
        bm_trace_enable(handle);
        gettimeofday(&tv_start, NULL);
        bm_get_profile(handle, &profile_start);

        ret = bm_memcpy_d2s(handle, sys_recv_buf, dev_buffer);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s to malloc recv_buf failed, ret=%d\n", ret);
            goto cleanup;
        }

        gettimeofday(&tv_end, NULL);
        timersub(&tv_end, &tv_start, &timediff);
        consume_sys_malloc += (unsigned long)(timediff.tv_sec * 1000000 + timediff.tv_usec);

        bm_get_profile(handle, &profile_end);
        consume_real_malloc += (unsigned long)(profile_end.cdma_in_time - profile_start.cdma_in_time);
        bm_trace_disable(handle);
    }
    consume_sys_malloc /= iterations;
    consume_real_malloc /= iterations;

    if (array_cmp_u64(sys_send_buf, sys_recv_buf, transfer_size, "D2S to malloc recv")) {
        printf("D2S to malloc recv_buf verify FAILED\n");
        ret = BM_ERR_FAILURE;
        goto cleanup;
    }

    ret = bm_mem_mmap_device_mem(handle, &dev_recv_buf, &recv_bmmalloc_va);
    if (ret != BM_SUCCESS) {
        printf("bm_mem_mmap_device_mem failed, skip bmmalloc recv test\n");
    } else {
        need_munmap = 1;

        ret = bm_memcpy_s2d(handle, dev_buffer, sys_send_buf);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d refill failed\n");
            goto cleanup;
        }

        consume_sys_bmmalloc = 0;
        consume_real_bmmalloc = 0;
        for (i = 0; i < iterations; i++) {
            bm_trace_enable(handle);
            gettimeofday(&tv_start, NULL);
            bm_get_profile(handle, &profile_start);

            ret = bm_memcpy_d2s(handle, (void *)(uintptr_t)recv_bmmalloc_va, dev_buffer);
            if (ret != BM_SUCCESS) {
                printf("bm_memcpy_d2s to bmmalloc recv_buf failed, ret=%d\n", ret);
                goto cleanup;
            }

            gettimeofday(&tv_end, NULL);
            timersub(&tv_end, &tv_start, &timediff);
            consume_sys_bmmalloc += (unsigned long)(timediff.tv_sec * 1000000 + timediff.tv_usec);

            bm_get_profile(handle, &profile_end);
            consume_real_bmmalloc += (unsigned long)(profile_end.cdma_in_time - profile_start.cdma_in_time);
            bm_trace_disable(handle);
        }
        consume_sys_bmmalloc /= iterations;
        consume_real_bmmalloc /= iterations;

        if (array_cmp_u64(sys_send_buf, (unsigned char *)(uintptr_t)recv_bmmalloc_va, transfer_size, "D2S to bmmalloc recv")) {
            printf("D2S to bmmalloc recv_buf verify FAILED\n");
            ret = BM_ERR_FAILURE;
            goto cleanup;
        }
    }

    printf("D2S recv=malloc:   sys=%lu us, real=%lu us, bandwidth=%.2f MB/s\n",
           consume_sys_malloc, consume_real_malloc,
           consume_sys_malloc > 0 ? (float)transfer_size / (1024.0f * 1024.0f) / (consume_sys_malloc / 1000000.0f) : 0.0f);
    if (recv_bmmalloc_va != 0) {
        printf("D2S recv=bmmalloc: sys=%lu us, real=%lu us, bandwidth=%.2f MB/s\n",
               consume_sys_bmmalloc, consume_real_bmmalloc,
               consume_sys_bmmalloc > 0 ? (float)transfer_size / (1024.0f * 1024.0f) / (consume_sys_bmmalloc / 1000000.0f) : 0.0f);
    }
    printf("D2S data verify OK\n");

cleanup:
    if (need_munmap && recv_bmmalloc_va != 0) {
        size_t align_sz = ((size_t)transfer_size + getpagesize() - 1) & ~((size_t)getpagesize() - 1);
        munmap((void *)(uintptr_t)recv_bmmalloc_va, align_sz);
    }
    if (handle) {
        if (dev_recv_allocated)
            bm_free_device(handle, dev_recv_buf);
        if (dev_allocated)
            bm_free_device(handle, dev_buffer);
        bm_dev_free(handle);
    }
    if (sys_send_buf)
        free(sys_send_buf);
    if (sys_recv_buf)
        free(sys_recv_buf);

    return ret == BM_SUCCESS ? 0 : -1;
}

int main(int argc, char *argv[])
{
    u64 size_mb = DEFAULT_SIZE_MB;
    int iterations = D2S_TEST_ITERATIONS;
    int count = 0;
    int ret = 0;
    bm_handle_t handle = NULL;

    if (argc >= 2)
        size_mb = (u64)atoi(argv[1]);
    if (argc >= 3)
        iterations = atoi(argv[2]);

    bm_dev_getcount(&count);
    if (count <= 0) {
        printf("No BM device found\n");
        return -1;
    }

    printf("BM device count: %d\n", count);
    printf("Usage: %s [size_MB] [iterations]\n", argv[0]);

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed\n");
        return -1;
    }

    {
        struct bm_misc_info misc_info;
        ret = bm_get_misc_info(handle, &misc_info);
        if (ret != BM_SUCCESS || misc_info.pcie_soc_mode != 1) {
            printf("This test only supports SoC mode. Current mode: PCIE. Exit.\n");
            bm_dev_free(handle);
            return -1;
        }
    }

    ret = test_d2s_malloc_vs_bmmalloc(handle, iterations);
    if (ret != 0) {
        bm_dev_free(handle);
        return ret;
    }

    bm_dev_free(handle);
    ret = test_d2s_normal_perf(size_mb * 1024ULL * 1024ULL, iterations);

    return ret;
}
