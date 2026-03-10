#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_memory.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/mman.h>
#endif

#define DEFAULT_SIZE    (1024 * 1024)
#define DEFAULT_ITERS   10

static long long get_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void print_result(const char *label, unsigned int size,
                         long long total_us, int iterations)
{
    long long avg = total_us / iterations;
    float bw = avg > 0
        ? (float)size / (1024.0f * 1024.0f) / (avg / 1000000.0f)
        : 0.0f;
    printf("  %-40s avg=%lld us  bw=%.2f MB/s\n", label, avg, bw);
}

int main(int argc, char *argv[])
{
    unsigned int size = DEFAULT_SIZE;
    int iterations = DEFAULT_ITERS;
    bm_handle_t handle = NULL;
    bm_status_t ret;
    bm_device_mem_t dev_a, dev_b;
    u64 va_a = 0, va_b = 0;
    int dev_a_ok = 0, dev_b_ok = 0;
    int mmap_a_ok = 0, mmap_b_ok = 0;
    unsigned char *host_buf = NULL;
    long long t0, t1, total;
    int i;

    if (argc >= 2)
        size = (unsigned int)atoi(argv[1]) * 1024 * 1024;
    if (argc >= 3)
        iterations = atoi(argv[2]);
    if (size == 0) size = DEFAULT_SIZE;
    if (iterations <= 0) iterations = DEFAULT_ITERS;

    int count = 0;
    bm_dev_getcount(&count);
    if (count <= 0) {
        printf("No BM device found\n");
        return -1;
    }

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || !handle) {
        printf("bm_dev_request failed\n");
        return -1;
    }

    {
        struct bm_misc_info misc;
        bm_get_misc_info(handle, &misc);
        if (misc.pcie_soc_mode != 1) {
            printf("SoC mode only. Exit.\n");
            bm_dev_free(handle);
            return -1;
        }
    }

    printf("=== test_mmap_memcpy_perf ===\n");
    printf("size=%u (%.2f MB), iterations=%d\n\n",
           size, size / (1024.0f * 1024.0f), iterations);

    ret = bm_malloc_device_byte(handle, &dev_a, size);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte dev_a FAILED\n");
        goto out;
    }
    dev_a_ok = 1;
    printf("dev_a: addr=0x%llx dmabuf_fd=%d size=%u\n",
           (unsigned long long)bm_mem_get_device_addr(dev_a),
           dev_a.u.device.dmabuf_fd, size);

    ret = bm_malloc_device_byte(handle, &dev_b, size);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte dev_b FAILED\n");
        goto out;
    }
    dev_b_ok = 1;
    printf("dev_b: addr=0x%llx dmabuf_fd=%d size=%u\n",
           (unsigned long long)bm_mem_get_device_addr(dev_b),
           dev_b.u.device.dmabuf_fd, size);

    ret = bm_mem_mmap_device_mem(handle, &dev_a, &va_a);
    if (ret != BM_SUCCESS || va_a == 0) {
        printf("mmap dev_a FAILED ret=%d\n", ret);
        goto out;
    }
    mmap_a_ok = 1;
    printf("dev_a mmap OK: va=0x%llx\n", (unsigned long long)va_a);

    ret = bm_mem_mmap_device_mem(handle, &dev_b, &va_b);
    if (ret != BM_SUCCESS || va_b == 0) {
        printf("mmap dev_b FAILED ret=%d\n", ret);
        goto out;
    }
    mmap_b_ok = 1;
    printf("dev_b mmap OK: va=0x%llx\n\n", (unsigned long long)va_b);

    host_buf = (unsigned char *)malloc(size);
    if (!host_buf) {
        printf("malloc host_buf failed\n");
        goto out;
    }
    for (i = 0; i < (int)size; i++)
        host_buf[i] = (unsigned char)(i & 0xff);

    memcpy((void *)(uintptr_t)va_a, host_buf, size);
    bm_mem_flush_device_mem(handle, &dev_a);

    printf("--- Performance Results (avg of %d iterations) ---\n", iterations);

    total = 0;
    for (i = 0; i < iterations; i++) {
        bm_mem_invalidate_device_mem(handle, &dev_a);
        t0 = get_us();
        memcpy((void *)(uintptr_t)va_b, (void *)(uintptr_t)va_a, size);
        t1 = get_us();
        total += (t1 - t0);
        bm_mem_flush_device_mem(handle, &dev_b);
    }
    print_result("[mmap_a -> mmap_b]", size, total, iterations);

    {
        unsigned char *dst = (unsigned char *)malloc(size);
        if (dst) {
            total = 0;
            for (i = 0; i < iterations; i++) {
                bm_mem_invalidate_device_mem(handle, &dev_a);
                t0 = get_us();
                memcpy(dst, (void *)(uintptr_t)va_a, size);
                t1 = get_us();
                total += (t1 - t0);
            }
            print_result("[mmap_a -> malloc] (read from dev)", size, total, iterations);

            if (memcmp(dst, host_buf, size) == 0)
                printf("  -> data verify OK\n");
            else
                printf("  -> data verify FAILED!\n");
            free(dst);
        }
    }

    total = 0;
    for (i = 0; i < iterations; i++) {
        t0 = get_us();
        memcpy((void *)(uintptr_t)va_b, host_buf, size);
        t1 = get_us();
        total += (t1 - t0);
        bm_mem_flush_device_mem(handle, &dev_b);
    }
    print_result("[malloc -> mmap_b] (write to dev)", size, total, iterations);

    {
        unsigned char *src2 = (unsigned char *)malloc(size);
        unsigned char *dst2 = (unsigned char *)malloc(size);
        if (src2 && dst2) {
            memcpy(src2, host_buf, size);
            total = 0;
            for (i = 0; i < iterations; i++) {
                t0 = get_us();
                memcpy(dst2, src2, size);
                t1 = get_us();
                total += (t1 - t0);
            }
            print_result("[malloc -> malloc] (baseline)", size, total, iterations);
        }
        if (src2) free(src2);
        if (dst2) free(dst2);
    }

    printf("\nDone.\n");

out:
    if (mmap_a_ok) {
        size_t align_sz = (size + getpagesize() - 1) & ~(getpagesize() - 1);
        munmap((void *)(uintptr_t)va_a, align_sz);
    }
    if (mmap_b_ok) {
        size_t align_sz = (size + getpagesize() - 1) & ~(getpagesize() - 1);
        munmap((void *)(uintptr_t)va_b, align_sz);
    }
    if (dev_b_ok) bm_free_device(handle, dev_b);
    if (dev_a_ok) bm_free_device(handle, dev_a);
    if (host_buf) free(host_buf);
    if (handle) bm_dev_free(handle);
    return 0;
}
