#include <bmlib_runtime.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int failures = 0;

void checkStatus(bm_status_t status, const char *what) {
  if (status != BM_SUCCESS) {
    printf("[FAIL] %s failed, status = %d\n", what, status);
    exit(1);
  }
}

void compare(const uint8_t *expect, const uint8_t *actual, size_t size,
             const char *what) {
  for (size_t i = 0; i < size; i++) {
    if (expect[i] != actual[i]) {
      printf("[FAIL] %s: mismatch at byte %zu: expect 0x%02x, got 0x%02x\n",
             what, i, expect[i], actual[i]);
      failures++;
      return;
    }
  }
  printf("[PASS] %s: %zu bytes match\n", what, size);
}

void printStep(unsigned int heap, unsigned long long base,
               unsigned long long offset, unsigned long long n,
               unsigned long long copied, unsigned long long total,
               const char *phase) {
  const int width = 28;
  int filled = 0;
  if (total > 0)
    filled = (int)((copied * (unsigned long long)width) / total);
  if (filled > width)
    filled = width;
  char bar[32];
  for (int i = 0; i < width; i++)
    bar[i] = (i < filled) ? '#' : '-';
  bar[width] = '\0';
  double pct = total ? (100.0 * (double)copied / (double)total) : 100.0;
  printf("heap %u [%s] %5.1f%%  %.2f/%.2f GB  %-10s  paddr=0x%llx "
         "offset=0x%llx size=%llu\n",
         heap, bar, pct,
         (double)copied / (1024.0 * 1024.0 * 1024.0),
         (double)total / (1024.0 * 1024.0 * 1024.0), phase, base + offset,
         offset, n);
  fflush(stdout);
}

void printHeapStats(bm_handle_t handle, const char *tag) {
  unsigned int heap_num = 0;
  checkStatus(bm_get_gmem_total_heap_num(handle, &heap_num),
              "bm_get_gmem_total_heap_num");
  printf("[%s] ion heap_num = %u\n", tag, heap_num);
  for (unsigned int i = 0; i < heap_num; i++) {
    bm_heap_stat_byte_t st;
    memset(&st, 0, sizeof(st));
    char what[64];
    snprintf(what, sizeof(what), "bm_get_gmem_heap_stat_byte_by_id (heap %u)",
             i);
    checkStatus(bm_get_gmem_heap_stat_byte_by_id(handle, &st, i), what);
    printf("  heap %u: total=%.2f GB (%llu bytes), avail=%.2f GB (%llu bytes), "
           "used=%.2f GB, start=0x%llx\n",
           i, (double)st.mem_total / (1024.0 * 1024.0 * 1024.0), st.mem_total,
           (double)st.mem_avail / (1024.0 * 1024.0 * 1024.0), st.mem_avail,
           (double)st.mem_used / (1024.0 * 1024.0 * 1024.0),
           (unsigned long long)st.mem_start_addr);
  }
}

} // namespace

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  int devid = argc > 1 ? atoi(argv[1]) : 0;
  const unsigned long long chunkSize = 4ULL * 1024 * 1024;
  const unsigned long long align = 4096;
  const unsigned long long step = 1ULL * 1024 * 1024;

  bm_handle_t handle;
  checkStatus(bm_dev_request(&handle, devid), "bm_dev_request");

  unsigned int heap_num = 0;
  checkStatus(bm_get_gmem_total_heap_num(handle, &heap_num),
              "bm_get_gmem_total_heap_num");
  if (heap_num == 0) {
    printf("[FAIL] no ion heap found\n");
    bm_dev_free(handle);
    return 1;
  }

  printHeapStats(handle, "before");

  std::vector<bm_device_mem_u64_t> blocks(heap_num);
  std::vector<unsigned long long> allocSizes(heap_num, 0);
  std::vector<unsigned int> allocatedHeaps;

  for (unsigned int h = 0; h < heap_num; h++) {
    bm_heap_stat_byte_t st;
    memset(&st, 0, sizeof(st));
    memset(&blocks[h], 0, sizeof(blocks[h]));
    char what[80];
    snprintf(what, sizeof(what), "bm_get_gmem_heap_stat_byte_by_id (heap %u)",
             h);
    checkStatus(bm_get_gmem_heap_stat_byte_by_id(handle, &st, h), what);

    unsigned long long trySize = st.mem_avail & ~(align - 1);
    if (trySize == 0) {
      printf("heap %u: avail=0, skip\n", h);
      continue;
    }

    bm_status_t stAlloc = BM_ERR_FAILURE;
    while (trySize >= align) {
      stAlloc = bm_malloc_device_byte_heap_u64(handle, &blocks[h], (int)h,
                                               trySize);
      if (stAlloc == BM_SUCCESS)
        break;
      if (trySize <= step)
        break;
      trySize -= step;
    }
    snprintf(what, sizeof(what), "bm_malloc_device_byte_heap_u64 (heap %u)", h);
    checkStatus(stAlloc, what);

    allocSizes[h] = bm_mem_get_device_size_u64(blocks[h]);
    allocatedHeaps.push_back(h);
    printf("heap %u: device addr = 0x%llx, alloc = %llu bytes (%.2f GB), "
           "avail was %llu bytes\n",
           h, bm_mem_get_device_addr_u64(blocks[h]), allocSizes[h],
           (double)allocSizes[h] / (1024.0 * 1024.0 * 1024.0), st.mem_avail);
  }

  printHeapStats(handle, "after_alloc");

  std::vector<uint8_t> src(chunkSize);
  std::vector<uint8_t> dst(chunkSize);

  for (unsigned int idx = 0; idx < allocatedHeaps.size(); idx++) {
    unsigned int h = allocatedHeaps[idx];
    unsigned long long total = allocSizes[h];
    unsigned long long base = bm_mem_get_device_addr_u64(blocks[h]);
    double totalS2dSec = 0.0;
    double totalD2sSec = 0.0;
    unsigned long long copied = 0;

    printf("==== test heap %u  paddr=0x%llx  size=%.2f GB ====\n", h, base,
           (double)total / (1024.0 * 1024.0 * 1024.0));
    fflush(stdout);

    for (unsigned long long offset = 0; offset < total;) {
      unsigned long long n = chunkSize;
      if (offset + n > total)
        n = total - offset;

      printStep(h, base, offset, n, copied, total, "fill");
      for (unsigned long long i = 0; i < n; i++) {
        src[i] = static_cast<uint8_t>(
            (i * 131 + 7 + h * 17 + (offset & 0xff)) & 0xff);
      }
      memset(dst.data(), 0, (size_t)n);

      printStep(h, base, offset, n, copied, total, "s2d start");
      auto t0 = std::chrono::steady_clock::now();
      checkStatus(bm_memcpy_s2d_partial_offset_u64(handle, blocks[h],
                                                   src.data(), n, offset),
                  "bm_memcpy_s2d_partial_offset_u64");
      auto t1 = std::chrono::steady_clock::now();
      double s2dSec = std::chrono::duration<double>(t1 - t0).count();
      double gb = (double)n / 1e9;
      printf("heap %u paddr=0x%llx s2d done %.3f s (%.2f GB/s)\n", h,
             base + offset, s2dSec, gb / s2dSec);
      fflush(stdout);

      printStep(h, base, offset, n, copied, total, "d2s start");
      checkStatus(bm_memcpy_d2s_partial_offset_u64(handle, dst.data(),
                                                   blocks[h], n, offset),
                  "bm_memcpy_d2s_partial_offset_u64");
      auto t2 = std::chrono::steady_clock::now();
      double d2sSec = std::chrono::duration<double>(t2 - t1).count();
      printf("heap %u paddr=0x%llx d2s done %.3f s (%.2f GB/s)\n", h,
             base + offset, d2sSec, gb / d2sSec);
      fflush(stdout);

      printStep(h, base, offset, n, copied, total, "compare");
      char what[96];
      snprintf(what, sizeof(what), "heap %u paddr=0x%llx s2d/d2s", h,
               base + offset);
      compare(src.data(), dst.data(), (size_t)n, what);
      fflush(stdout);

      totalS2dSec += s2dSec;
      totalD2sSec += d2sSec;
      copied += n;
      offset += n;
    }

    double totalGB = (double)copied / 1e9;
    printf("heap %u average: s2d %.2f GB/s, d2s %.2f GB/s, covered %.2f GB\n",
           h, totalGB / totalS2dSec, totalGB / totalD2sSec, totalGB);
  }

  for (unsigned int idx = 0; idx < allocatedHeaps.size(); idx++) {
    bm_free_device_u64(handle, blocks[allocatedHeaps[idx]]);
  }
  bm_dev_free(handle);

  if (failures == 0) {
    printf("ALL TESTS PASSED\n");
    return 0;
  }
  printf("%d TEST(S) FAILED\n", failures);
  return 1;
}
