#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>
#include <time.h>
#include "bmlib_internal.h"
#include "bmlib_profile.h"
#include "bmlib_utils.h"
#include "api.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include<direct.h>
#endif

using std::string;
using std::vector;

#define PROFILE_INFO(fmt, ...)  printf("[perf][info]" fmt, ##__VA_ARGS__);
#define PROFILE_ERROR(fmt, ...)  printf("[perf][error]" fmt, ##__VA_ARGS__);

#ifdef _WIN32
#define BILLION (1E9)
static BOOL          g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;

int clock_gettime(int dummy, struct timespec* ct) {
  LARGE_INTEGER count;

  if (g_first_time) {
    g_first_time = 0;

    if (0 == QueryPerformanceFrequency(&g_counts_per_sec)) {
      g_counts_per_sec.QuadPart = 0;
    }
  }

  if ((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) ||
      (0 == QueryPerformanceCounter(&count))) {
    return -1;
  }

  ct->tv_sec  = count.QuadPart / g_counts_per_sec.QuadPart;
  ct->tv_nsec = ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) /
      g_counts_per_sec.QuadPart;

  return 0;
}

int gettimeofday(struct timeval* tp, void* tzp) {
  struct timespec clock;
  clock_gettime(0, &clock);
  tp->tv_sec = clock.tv_sec;
  tp->tv_usec = clock.tv_nsec / 1000;
  return 0;
}


void timeradd(struct timeval* a, struct timeval* b, struct timeval* res) {
  res->tv_sec  = a->tv_sec + b->tv_sec;
  res->tv_usec = a->tv_usec + b->tv_usec;
}

void timersub(struct timeval* a, struct timeval* b, struct timeval* res) {
  res->tv_sec  = a->tv_sec - b->tv_sec;
  res->tv_usec = a->tv_usec - b->tv_usec;
}
#endif

//////////////////////////////////////////
// some useful utilities
#define TIME_TO_USECS(t) (t.tv_sec*1000000+ t.tv_usec)

inline u64 get_usec(){
  timeval time;
  gettimeofday(&time, NULL);
  return TIME_TO_USECS(time);
}

static int bm_mkdir(const char *dirname, bool must_new)
{
  string dname = dirname;
  struct stat st;
  dname += "/";
  if (stat(dname.c_str(), &st) == -1) {
#ifdef __linux__
    ASSERT(mkdir(dname.c_str(), 0777) == 0);
#else
    ASSERT(_mkdir(dname.c_str()) == 0);
#endif
    return 1;
  }
  if(must_new){
    string cmd = "rm ";
    cmd += dname + "*";
    system(cmd.c_str());
  }
  return 0;
}

//////////////////////////////////////////////////////////
static void write_block(FILE* profile_fp, u32 type, size_t len, const void *data)
{
  if(len == 0) return;
  // BMRT_LOG(INFO, "%s: type=%d, len=%d", __func__, type, (int)len);
  fwrite(&type, sizeof(type), 1, profile_fp);
  fwrite(&len, sizeof(u32), 1, profile_fp);
  fwrite(data, len, 1, profile_fp);
}

#define ENV_PREFIX "BMLIB_"
#define ENV_GDMA_SIZE (ENV_PREFIX "GDMA_RECORD_SIZE")
#define ENV_ARM_SIZE (ENV_PREFIX "ARM_RECORD_SIZE")
#define ENV_BDC_SIZE (ENV_PREFIX "BDC_RECORD_SIZE")
#define ENV_SHOW_SIZE (ENV_PREFIX "PERF_SHOW_SIZE")
#define ENV_SHOW_RAW_DATA (ENV_PREFIX "PERF_SHOW_RAW_DATA")
#define ENV_ENABLE_PROFILE (ENV_PREFIX "ENABLE_PROFILE")

void free_buffer(bm_handle_t handle, buffer_pair_t *bp) {
  if(bp->ptr){
    delete [] bp->ptr;
    bp->ptr =nullptr;
    bm_free_device(handle, bp->mem);
  }
}

void must_alloc_buffer(bm_handle_t handle, buffer_pair_t *bp, size_t size) {
  if(bp->size != size || !bp->ptr){
    free_buffer(handle, bp);
  }
  if(!bp->ptr) {
    bp->ptr= (u8 *)new u8[size];
    if (!bp->ptr) {
      PROFILE_ERROR("malloc system buffer failed for profile\n");
      exit(-1);
    }

    bm_status_t status = bm_malloc_device_byte(handle, &bp->mem, size);
    if (BM_SUCCESS != status) {
      PROFILE_ERROR("device mem alloc failed: size=%d[0x%x], status=%d\n", (int)size, (int)size, (int)status);
      exit(-1);
    }
    bp->size = size;
  }
}

static inline int get_arch_code(bm_handle_t handle){
  (void)handle;
#ifdef USING_CMODEL
  int arch_code = handle->bm_dev->chip_id;
#else
  int arch_code = handle->misc_info.chipid;
#endif
  return arch_code;
}

static bm_status_t __bm_set_profile_enable(bm_handle_t handle, bool enable){
  ASSERT(handle);
  u32 api_buffer_size = sizeof(u32);
  u32 profile_enable = enable;
  bm_status_t status = bm_send_api(handle, (sglib_api_id_t)BM_API_ID_SET_PROFILE_ENABLE, (u8*)&profile_enable, api_buffer_size);
  if (BM_SUCCESS != status) {
    PROFILE_ERROR("bm_send_api failed, api id:%d, status:%d\n", BM_API_ID_SET_PROFILE_ENABLE, status);
  } else {
    status = bm_sync_api(handle);
    if (BM_SUCCESS != status) {
      PROFILE_ERROR("bm_sync_api failed, api id:%d, status:%d\n", BM_API_ID_SET_PROFILE_ENABLE, status);
    }
  }
  return status;
}

static void show_profile_tips(){
  PROFILE_INFO("=== this program is under PROFILE mode, which will cost extra time ===\n");
  PROFILE_INFO("=== close profile by unset 'BMLIB_ENABLE_ALL_PROFILE' and 'unset BMLIB_ENABLE_PROFILE' ===\n");
}

static bm_status_t __bm_get_profile_data(
    bm_handle_t handle,
    unsigned long long output_global_addr,
    unsigned int output_max_size,
    unsigned int byte_offset,
    unsigned int data_category //0: profile time records, 1: extra data
    ) {

  ASSERT(handle);
#pragma pack(1)
  struct {
    u64 arm_reserved_addr;
    u64 output_global_addr;
    u32 output_size;
    u32 byte_offset;
    u32 data_category; //0: profile_data, 1: profile extra data
  } api_data;
#pragma pack()

  const u32 api_buffer_size = sizeof(api_data);

  api_data.arm_reserved_addr = bm_gmem_arm_reserved_request(handle);
  api_data.output_global_addr = output_global_addr;
  api_data.output_size = output_max_size;
  api_data.byte_offset = byte_offset;
  api_data.data_category = data_category;

  sglib_api_id_t api_code = (sglib_api_id_t)BM_API_ID_GET_PROFILE_DATA;
  bm_status_t status = bm_send_api(handle, api_code, (u8*)&api_data, api_buffer_size);
  if (BM_SUCCESS != status) {
    PROFILE_ERROR("bm_send_api failed, api id:%d, status:%d\n", api_code, status);
  } else {
    status = bm_sync_api(handle);
    if (BM_SUCCESS != status) {
      PROFILE_ERROR("bm_sync_api failed, api id:%d, status:%d\n",api_code, status);
    }
  }
  bm_gmem_arm_reserved_release(handle);
  return status;
}


////////////////////////////////////////////////////////
// for BM1684
#define BM1684_GDMA_FREQ_MHZ 575
#define BM1684_TPU_FREQ_MHZ 550

#pragma pack(1)
typedef struct {
  unsigned int inst_start_time;
  unsigned int inst_end_time;
  unsigned long inst_id : 16;
  unsigned long long computation_load : 48;
  unsigned int num_read;
  unsigned int num_read_stall;
  unsigned int num_write;
  unsigned int reserved;
} BM1684_TPU_PROFILE_FORMAT;

typedef struct {
  unsigned int inst_start_time;
  unsigned int inst_end_time;
  unsigned int inst_id : 16;
  unsigned int reserved: 16;
  unsigned int d0_aw_bytes;
  unsigned int d0_wr_bytes;
  unsigned int d0_ar_bytes;
  unsigned int d1_aw_bytes;
  unsigned int d1_wr_bytes;
  unsigned int d1_ar_bytes;
  unsigned int gif_aw_bytes;
  unsigned int gif_wr_bytes;
  unsigned int gif_ar_bytes;
  unsigned int d0_wr_valid_cyc;
  unsigned int d0_rd_valid_cyc;
  unsigned int d1_wr_valid_cyc;
  unsigned int d1_rd_valid_cyc;
  unsigned int gif_wr_valid_cyc;
  unsigned int gif_rd_valid_cyc;
  unsigned int d0_wr_stall_cyc;
  unsigned int d0_rd_stall_cyc;
  unsigned int d1_wr_stall_cyc;
  unsigned int d1_rd_stall_cyc;
  unsigned int gif_wr_stall_cyc;
  unsigned int gif_rd_stall_cyc;
  unsigned int d0_aw_end;
  unsigned int d0_aw_st;
  unsigned int d0_ar_end;
  unsigned int d0_ar_st;
  unsigned int d0_wr_end;
  unsigned int d0_wr_st;
  unsigned int d0_rd_end;
  unsigned int d0_rd_st;
  unsigned int d1_aw_end;
  unsigned int d1_aw_st;
  unsigned int d1_ar_end;
  unsigned int d1_ar_st;
  unsigned int d1_wr_end;
  unsigned int d1_wr_st;
  unsigned int d1_rd_end;
  unsigned int d1_rd_st;
  unsigned int gif_aw_reserved1;
  unsigned int gif_aw_reserved2;
  unsigned int gif_ar_end;
  unsigned int gif_ar_st;
  unsigned int gif_wr_end;
  unsigned int gif_wr_st;
  unsigned int gif_rd_end;
  unsigned int gif_rd_st;
} BM1684_GDMA_PROFILE_FORMAT;
#pragma pack()

static char num2char(int num){
    if(num<0){
        return '-';
    } else if(num>16){
        return '*';
    } else if(num>9){
        return num-10+'A';
    }
    return num + '0';
}
static void show_raw_data(const unsigned char* data, size_t len){
    std::string raw_str = "     len=";
    raw_str += std::to_string(len);
    raw_str += " [";
    for(size_t i=0; i<len; i++){
        auto c = data[i];
        raw_str += num2char(c>>4&0xf);
        raw_str += num2char(c&0xf);
        raw_str += " ";
    }
    raw_str += " ]\n";
    PROFILE_INFO("%s\n", raw_str.c_str());
}

void show_bm1684_gdma_item(const BM1684_GDMA_PROFILE_FORMAT* data, int len, float freq_MHz, float time_offset=0){
  if(len==0 || !data) return;
  int max_print = get_env_int_value(ENV_SHOW_SIZE, -1);
  bool show_raw = get_env_bool_value(ENV_SHOW_RAW_DATA, false);
  if(max_print<=0) return;
  int real_print = max_print>len?len: max_print;
  float period = 1/freq_MHz;
  PROFILE_INFO("Note: gdma record time_offset=%fus, freq=%gMHz, period=%.3fus\n", time_offset, freq_MHz, period);
  unsigned int cycle_offset = data[0].inst_start_time;
  for(int i=0; i<real_print; i++){
    auto& p = data[i];
    auto total_time =  (p.inst_end_time - p.inst_start_time) * period;
    auto total_bytes = p.d0_wr_bytes + p.d1_wr_bytes + p.gif_wr_bytes;
    auto speed = total_time>0?total_bytes/total_time: -1000.0;
    PROFILE_INFO("---> gdma record #%d,"
                 " inst_id=%d, cycle=%d, start=%.3fus,"
                 " end=%.3fus, interval=%.3fus"
                 " bytes=%d, speed=%.3fGB/s"
                 "\n",
                 i, p.inst_id,
                 p.inst_end_time - p.inst_start_time,
                 (p.inst_start_time - cycle_offset)* period + time_offset,
                 (p.inst_end_time - cycle_offset) * period + time_offset,
                 total_time,
                 total_bytes,
                 speed/1000.0);
    if(show_raw) {
        show_raw_data((const unsigned char*) &p, sizeof(p));
    }
  }
  if(real_print<len){
    PROFILE_INFO("... (%d gdma records are not shown)\n", len-real_print);
  }
}

void show_bm1684_bdc_item(const BM1684_TPU_PROFILE_FORMAT* data, int len, float freq_MHz, float time_offset=0){
  if(len==0 || !data) return;
  int max_print = get_env_int_value(ENV_SHOW_SIZE, -1);
  bool show_raw = get_env_bool_value(ENV_SHOW_RAW_DATA, false);
  if(max_print<=0) return;
  int real_print = max_print>len?len: max_print;
  float period = 1/freq_MHz;
  PROFILE_INFO("Note: bdc record time_offset=%fus, freq=%gMHz, period=%.3fus\n", time_offset, freq_MHz, period);
  unsigned int cycle_offset = data[0].inst_start_time;
  for(int i=0; i<real_print; i++){
    auto& p = data[i];
    PROFILE_INFO("---> bdc record #%d, inst_id=%d, cycle=%d, start=%.3fus, end=%.3fus, interval=%.3fus\n",
                 i, (int)(p.inst_id), (int)(p.inst_end_time - p.inst_start_time),
                 (p.inst_start_time - cycle_offset)* period + time_offset,
                 (p.inst_end_time - cycle_offset) * period + time_offset,
                 (p.inst_end_time - p.inst_start_time) * period);
    if(show_raw) {
        show_raw_data((const unsigned char*) &p, sizeof(p));
    }
  }
  if(real_print<len){
    PROFILE_INFO("... (%d bdc records are not shown)\n", len-real_print);
  }
}

// fix bm1684 gdma perf data
const int   BLOCK_SIZE = 4096;
const int   BURST_LEN  = 192;
static void __fix_data(unsigned char* aligned_data, unsigned int len, size_t offset)
{
  if (len < BLOCK_SIZE)
    return;
  __fix_data(&aligned_data[BLOCK_SIZE], len - BLOCK_SIZE, offset + BLOCK_SIZE);
  unsigned int max_copy_len = (offset + BLOCK_SIZE + BURST_LEN - 1) / BURST_LEN * BURST_LEN - offset - BLOCK_SIZE;
  unsigned int copy_len = (max_copy_len < len - BLOCK_SIZE) ? max_copy_len : (len - BLOCK_SIZE);
  if (copy_len > 0) {
    memcpy(&aligned_data[BLOCK_SIZE], aligned_data, copy_len);
  }
  return;
}

static unsigned int fix_bm1684_gdma_monitor_data(unsigned char* aligned_data, unsigned int len){
  unsigned int valid_len = len;
  unsigned int item_size = sizeof(BM1684_GDMA_PROFILE_FORMAT);
  while(valid_len != 0 && aligned_data[valid_len-1]==0xFF) valid_len--;
  valid_len = (valid_len + item_size-1)/item_size * item_size;
  valid_len += item_size; //to fix the last record
  while(valid_len>len) valid_len -= item_size;
  __fix_data(aligned_data, valid_len, 0);
  unsigned int left_len = valid_len;
  unsigned int copy_len = BLOCK_SIZE;
  while(copy_len%BURST_LEN != 0) copy_len += BLOCK_SIZE;
  unsigned int invalid_size = (BURST_LEN+item_size -1)/item_size * item_size;
  copy_len -= invalid_size;
  unsigned char* src_ptr = aligned_data + invalid_size;
  unsigned char* dst_ptr = aligned_data;
  valid_len = 0;
  while(left_len>invalid_size){
    unsigned int real_copy_len = left_len>(copy_len+invalid_size)?copy_len:(left_len - invalid_size);
    memcpy(dst_ptr, src_ptr, real_copy_len);
    dst_ptr += real_copy_len;
    src_ptr += copy_len + invalid_size;
    left_len -= (real_copy_len + invalid_size);
    valid_len += real_copy_len;
  }
  while(valid_len != 0 && aligned_data[valid_len-1]==0xFF) valid_len--;
  valid_len = valid_len/item_size * item_size;
  return valid_len;
}

bm_status_t bm1684_arm_init(bm_handle_t handle, bmlib_profile_t* profile, size_t len){
  must_alloc_buffer(handle, &profile->arm_buffer, len);
  ASSERT(__bm_set_profile_enable(handle, true) == BM_SUCCESS);
  return BM_SUCCESS;
}

bm_status_t bm1684_gdma_init(bm_handle_t handle, bmlib_profile_t* profile, size_t len){
  u64 gdma_device_buffer_size = len * sizeof(BM1684_GDMA_PROFILE_FORMAT);
  must_alloc_buffer(handle, &profile->gdma_buffer, gdma_device_buffer_size);
  // set gdma_start_offset to aligned(start,4K)+192 to assure the front of data is right,
  // not to override by hw bug.
  const int align_number = BLOCK_SIZE;
  const int burst_len = BURST_LEN;
  u64 real_gdma_mem_addr = bm_mem_get_device_addr(profile->gdma_buffer.mem);
  u64 real_gdma_mem_size = bm_mem_get_device_size(profile->gdma_buffer.mem);
  u64 aligned_addr = ALIGN(real_gdma_mem_addr, align_number);
  u64 aligned_size = real_gdma_mem_size + real_gdma_mem_addr - aligned_addr;
  profile->gdma_aligned_mem = profile->gdma_buffer.mem;
  bm_mem_set_device_addr(&profile->gdma_aligned_mem, aligned_addr);
  bm_mem_set_device_size(&profile->gdma_aligned_mem, aligned_size);

  auto& gdma_perf_monitor = profile->gdma_perf_monitor;
  gdma_perf_monitor.buffer_start_addr = aligned_addr + burst_len;
  gdma_perf_monitor.buffer_size = aligned_size - burst_len;
  gdma_perf_monitor.monitor_id = PERF_MONITOR_GDMA;

  memset(profile->gdma_buffer.ptr, -1, profile->gdma_buffer.size);
  u32 ret = bm_memcpy_s2d(handle, profile->gdma_aligned_mem, profile->gdma_buffer.ptr);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
  }
  return bm_enable_perf_monitor(handle, &gdma_perf_monitor);
}

bm_status_t bm1684_bdc_init(bm_handle_t handle, bmlib_profile_t* profile, size_t len){
  u64 tpu_device_buffer_size = len * sizeof(BM1684_TPU_PROFILE_FORMAT);
  must_alloc_buffer(handle, &profile->bdc_buffer, tpu_device_buffer_size);

  auto& tpu_perf_monitor = profile->tpu_perf_monitor;
  tpu_perf_monitor.buffer_start_addr = bm_mem_get_device_addr(profile->bdc_buffer.mem);
  tpu_perf_monitor.buffer_size = bm_mem_get_device_size(profile->bdc_buffer.mem);
  tpu_perf_monitor.monitor_id = PERF_MONITOR_TPU;
  memset(profile->bdc_buffer.ptr, -1, profile->bdc_buffer.size);
  auto ret = bm_memcpy_s2d(handle, profile->bdc_buffer.mem, profile->bdc_buffer.ptr);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
    exit(-1);
  }
  return bm_enable_perf_monitor(handle, &tpu_perf_monitor);
}

bm_status_t bm1684_arm_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp) {
  for(int i=0; i<2; i++){
    vector<u8> data;
    size_t offset = 0;
    size_t total_len = 0;
    u32 block_type = (i == 0) ? BLOCK_DYN_DATA : BLOCK_DYN_EXTRA;
    while(1){
      ASSERT(__bm_get_profile_data(
               handle,
               bm_mem_get_device_addr(profile->arm_buffer.mem),
               bm_mem_get_device_size(profile->arm_buffer.mem),
               offset, i)==BM_SUCCESS);
      ASSERT(bm_memcpy_d2s(handle, profile->arm_buffer.ptr, profile->arm_buffer.mem) == BM_SUCCESS);
      auto u32_ptr = (u32*)profile->arm_buffer.ptr;
      auto read_len = u32_ptr[0];
      if(total_len==0){
        total_len = u32_ptr[1];
      }
      auto data_ptr = (u8*)&u32_ptr[2];
      data.insert(data.end(), data_ptr, data_ptr + read_len);
      offset += read_len;
      if(offset>=total_len) break;
    }
    write_block(fp, block_type, data.size(), data.data());
  }
  ASSERT(__bm_set_profile_enable(handle, false) == BM_SUCCESS);
  free_buffer(handle, &profile->arm_buffer);
  return BM_SUCCESS;
}

bm_status_t bm1684_bdc_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp) {
  bm_disable_perf_monitor(handle, &profile->tpu_perf_monitor);
  auto ret = bm_memcpy_d2s(handle, profile->bdc_buffer.ptr, profile->bdc_buffer.mem);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("Get monitor profile from device to system failed, ret = %d\n", ret);
    exit(-1);
  }
  auto tpu_data = (BM1684_TPU_PROFILE_FORMAT *)profile->bdc_buffer.ptr;
  u32 valid_len = 0;
  u32 bdc_record_len = profile->bdc_buffer.size/sizeof(BM1684_TPU_PROFILE_FORMAT);
  while (tpu_data[valid_len].inst_id != 65535 && valid_len < bdc_record_len) valid_len++;
  PROFILE_INFO("bdc record_num=%d, max_record_num=%d\n", (int)valid_len, (int)bdc_record_len);
  show_bm1684_bdc_item(tpu_data, valid_len, BM1684_TPU_FREQ_MHZ);
  write_block(fp, BLOCK_MONITOR_BDC, valid_len * sizeof(tpu_data[0]), tpu_data);
  free_buffer(handle, &profile->bdc_buffer);
  return BM_SUCCESS;
}

bm_status_t bm1684_gdma_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp) {
  bm_disable_perf_monitor(handle, &profile->gdma_perf_monitor);
  auto ret = bm_memcpy_d2s(handle, profile->gdma_buffer.ptr, profile->gdma_aligned_mem);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("Get monitor profile from device to system failed, ret = %d\n", ret);
    exit(-1);
  }
  u32 valid_len = fix_bm1684_gdma_monitor_data(profile->gdma_buffer.ptr, bm_mem_get_device_size(profile->gdma_aligned_mem));
  u32 gdma_record_len = profile->gdma_buffer.size/sizeof(BM1684_GDMA_PROFILE_FORMAT);
  u32 gdma_num = (valid_len/sizeof(BM1684_GDMA_PROFILE_FORMAT));
  PROFILE_INFO("gdma record_num=%d, max_record_num=%d\n",
               (int)gdma_num, (int)gdma_record_len);
  show_bm1684_gdma_item((BM1684_GDMA_PROFILE_FORMAT*)profile->gdma_buffer.ptr, gdma_num, BM1684_GDMA_FREQ_MHZ);
  write_block(fp, BLOCK_MONITOR_GDMA, valid_len, profile->gdma_buffer.ptr);
  free_buffer(handle, &profile->gdma_buffer);
  return BM_SUCCESS;
}

////////////////////////////////////////////////////////
// for BM1686

#pragma pack(1)
#define BM1686_GDMA_ITEM_SIZE 256
typedef BM1684_TPU_PROFILE_FORMAT BM1686_TPU_PROFILE_FORMAT;
typedef struct {
  BM1684_GDMA_PROFILE_FORMAT valid_data;
  unsigned char reserved[BM1686_GDMA_ITEM_SIZE-sizeof(BM1684_GDMA_PROFILE_FORMAT)];
} BM1686_GDMA_PROFILE_FORMAT;
#pragma pack()

bm_status_t bm1686_arm_init(bm_handle_t handle, bmlib_profile_t* profile, size_t len){
  bm1684_arm_init(handle, profile, len);
  return BM_SUCCESS;
}

bm_status_t bm1686_gdma_init(bm_handle_t handle, bmlib_profile_t* profile, size_t len){
  u64 gdma_device_buffer_size = len * sizeof(BM1686_GDMA_PROFILE_FORMAT);
  must_alloc_buffer(handle, &profile->gdma_buffer, gdma_device_buffer_size);

  auto& perf_monitor = profile->gdma_perf_monitor;
  perf_monitor.buffer_start_addr = bm_mem_get_device_addr(profile->gdma_buffer.mem);
  perf_monitor.buffer_size = bm_mem_get_device_size(profile->gdma_buffer.mem);
  perf_monitor.monitor_id = PERF_MONITOR_GDMA;
  memset(profile->gdma_buffer.ptr, -1, profile->gdma_buffer.size);
  auto ret = bm_memcpy_s2d(handle, profile->gdma_buffer.mem, profile->gdma_buffer.ptr);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
    exit(-1);
  }
  return bm_enable_perf_monitor(handle, &perf_monitor);
}

bm_status_t bm1686_bdc_init(bm_handle_t handle, bmlib_profile_t* profile, size_t len){
  u64 tpu_device_buffer_size = len * sizeof(BM1686_TPU_PROFILE_FORMAT);
  must_alloc_buffer(handle, &profile->bdc_buffer, tpu_device_buffer_size);

  auto& tpu_perf_monitor = profile->tpu_perf_monitor;
  tpu_perf_monitor.buffer_start_addr = bm_mem_get_device_addr(profile->bdc_buffer.mem);
  tpu_perf_monitor.buffer_size = bm_mem_get_device_size(profile->bdc_buffer.mem);
  tpu_perf_monitor.monitor_id = PERF_MONITOR_TPU;
  memset(profile->bdc_buffer.ptr, -1, profile->bdc_buffer.size);
  auto ret = bm_memcpy_s2d(handle, profile->bdc_buffer.mem, profile->bdc_buffer.ptr);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("init device buffer data failed, ret = %d\n", ret);
    exit(-1);
  }
  return bm_enable_perf_monitor(handle, &tpu_perf_monitor);
}

bm_status_t bm1686_arm_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp) {
  bm1684_arm_deinit(handle, profile, fp);
  return BM_SUCCESS;
}

bm_status_t bm1686_bdc_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp) {
  bm_disable_perf_monitor(handle, &profile->tpu_perf_monitor);
  auto ret = bm_memcpy_d2s(handle, profile->bdc_buffer.ptr, profile->bdc_buffer.mem);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("Get bdc monitor profile from device to system failed, ret = %d\n", ret);
    exit(-1);
  }
  auto tpu_data = (BM1686_TPU_PROFILE_FORMAT *)profile->bdc_buffer.ptr;
  u32 valid_len = 0;
  u32 bdc_record_len = profile->bdc_buffer.size/sizeof(BM1686_TPU_PROFILE_FORMAT);
  while (tpu_data[valid_len].inst_id != 65535 && valid_len < bdc_record_len) valid_len++;
  PROFILE_INFO("bdc record_num=%d, max_record_num=%d\n", (int)valid_len, (int)bdc_record_len);

  int freq_MHz = 0;
  bm_get_clk_tpu_freq(handle, &freq_MHz);
  show_bm1684_bdc_item(tpu_data, valid_len, freq_MHz);

  write_block(fp, BLOCK_MONITOR_BDC, valid_len * sizeof(tpu_data[0]), tpu_data);
  free_buffer(handle, &profile->bdc_buffer);
  return BM_SUCCESS;
}

bm_status_t bm1686_gdma_deinit(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp) {
  bm_disable_perf_monitor(handle, &profile->gdma_perf_monitor);
  auto ret = bm_memcpy_d2s(handle, profile->gdma_buffer.ptr, profile->gdma_buffer.mem);
  if (ret != BM_SUCCESS) {
    PROFILE_ERROR("Get gdma monitor profile from device to system failed, ret = %d\n", ret);
    exit(-1);
  }
  auto gdma_data = (BM1686_GDMA_PROFILE_FORMAT *)profile->gdma_buffer.ptr;
  u32 valid_len = 0;
  u32 record_len = profile->gdma_buffer.size/sizeof(BM1686_GDMA_PROFILE_FORMAT);
  while (gdma_data[valid_len].valid_data.inst_id != 65535 && valid_len < record_len) valid_len++;
  PROFILE_INFO("gdma record_num=%d, max_record_num=%d\n", (int)valid_len, (int)record_len);

  std::vector<decltype(gdma_data->valid_data)> valid_data(valid_len);
  for(u32 i=0; i<valid_len; i++){
    valid_data[i] = gdma_data[i].valid_data;
  }
  int freq_MHz = 0;
  bm_get_clk_tpu_freq(handle, &freq_MHz);
  show_bm1684_gdma_item(valid_data.data(), valid_data.size(), freq_MHz);
  write_block(fp, BLOCK_MONITOR_GDMA, valid_len * sizeof(gdma_data->valid_data), valid_data.data());
  free_buffer(handle, &profile->gdma_buffer);
  return BM_SUCCESS;
}
///////////////////////////////////////////////////////////////
/// BMProfile framework
///

typedef bm_status_t (*bm_profile_init_t)(bm_handle_t handle, bmlib_profile_t* profile, size_t len);
typedef bm_status_t (*bm_profile_deinit_t)(bm_handle_t handle, bmlib_profile_t* profile, FILE* fp);

struct bm_arch_profile_info_t {
  int arch_value;
  bm_profile_init_t bdc_init;
  bm_profile_init_t gdma_init;
  bm_profile_init_t arm_init;
  bm_profile_deinit_t bdc_deinit;
  bm_profile_deinit_t gdma_deinit;
  bm_profile_deinit_t arm_deinit;
};

#define ARCH_ITEM(arch, value) {0x##arch, { \
  value, \
  bm##arch##_bdc_init,\
  bm##arch##_gdma_init,\
  bm##arch##_arm_init,\
  bm##arch##_bdc_deinit,\
  bm##arch##_gdma_deinit,\
  bm##arch##_arm_deinit\
  }\
  }

std::map<int, bm_arch_profile_info_t> _global_arch_profile_map = {
  ARCH_ITEM(1684, 1),
  ARCH_ITEM(1686, 3),
};

bm_status_t bm_profile_init(bm_handle_t handle, bool force_enable) {
  auto arch_code = get_arch_code(handle);
  if(_global_arch_profile_map.count(arch_code) == 0) return BM_NOT_SUPPORTED;
  auto& arch_info = _global_arch_profile_map.at(arch_code);
  // judge if need profile
  int gdma_record_len = get_env_int_value(ENV_GDMA_SIZE, 128*1024);
  int bdc_record_len = get_env_int_value(ENV_BDC_SIZE, 128*1024);
  int dyn_max_size = get_env_int_value(ENV_ARM_SIZE, 1024*1024);
  bool enable_arm_perf =  dyn_max_size > 0;
  bool enable_bdc_perf =  bdc_record_len > 0;
  bool enable_gdma_perf = gdma_record_len > 0;
  bool enable_profile = (enable_arm_perf || enable_bdc_perf || enable_gdma_perf) &&
      (force_enable || get_env_bool_value(ENV_ENABLE_PROFILE, false));
  if (!enable_profile) return BM_SUCCESS;

  // set bmprofile struct
  bmlib_profile_t *profile = (bmlib_profile_t*)malloc(sizeof(bmlib_profile_t));
  if(!profile) return BM_ERR_FAILURE;
  memset(profile, 0, sizeof(bmlib_profile_t));
  static int profile_count = 0;
  profile->id = profile_count++;
  if(enable_arm_perf && arch_info.arm_init) {
    arch_info.arm_init(handle, profile, dyn_max_size);
  }
  if(enable_bdc_perf && arch_info.bdc_init){
    arch_info.bdc_init(handle, profile, bdc_record_len);
  }
  if(enable_gdma_perf && arch_info.gdma_init){
    arch_info.gdma_init(handle, profile, gdma_record_len);
  }

  PROFILE_INFO("Profiling init for %x\n", arch_code);
  show_profile_tips();
  profile->summary.interval.begin_usec = get_usec();
  handle->profile = profile;
  return BM_SUCCESS;
}

void bm_profile_deinit(bm_handle_t handle) {
  if(!handle->profile) return;
  auto arch_code = get_arch_code(handle);
  auto& arch_info = _global_arch_profile_map.at(arch_code);
  auto profile = handle->profile;
  handle->profile = NULL;
  profile->summary.interval.end_usec = get_usec();
  string dir = "bmprofile_data-" + std::to_string(handle->dev_id);
  bm_mkdir(dir.c_str(), false);
  string global_path = dir + "/global.profile";
  FILE* global_fp = fopen(global_path.c_str(), "a");
  int freq_MHz = 0;
  bm_get_clk_tpu_freq(handle, &freq_MHz);
  fprintf(global_fp, "arch=%d\n", arch_info.arch_value);
  fprintf(global_fp, "tpu_freq=%d\n", freq_MHz);
  fclose(global_fp);
  string file_path = dir + "/bmlib_" + std::to_string(profile->id)+".profile";
  FILE *fp = fopen(file_path.c_str(), "wb");
  ASSERT(fp);

  u32 summary_size = sizeof(bm_profile_interval_t) +
      sizeof(u32) + profile->summary.num_send * sizeof(bm_profile_send_api_t) +
      sizeof(u32) + profile->summary.num_mark * sizeof(bm_profile_mark_t)     +
      sizeof(u32) + profile->summary.num_sync * sizeof(bm_profile_interval_t) +
      sizeof(u32) + profile->summary.num_mem  * sizeof(bm_profile_mem_info_t) +
      sizeof(u32) + profile->summary.num_copy * sizeof(bm_profile_memcpy_t);
  u32 block_type = BLOCK_BMLIB;

  fwrite(&block_type, sizeof(u32), 1, fp);
  fwrite(&summary_size, sizeof(u32), 1, fp);
  fwrite(&profile->summary.interval, sizeof(bm_profile_interval_t), 1, fp);
  fwrite(&profile->summary.num_send, sizeof(u32), 1, fp);
  fwrite(profile->summary.send_info, sizeof(bm_profile_send_api_t), profile->summary.num_send, fp);
  fwrite(&profile->summary.num_sync, sizeof(u32), 1, fp);
  fwrite(profile->summary.sync_info, sizeof(bm_profile_interval_t), profile->summary.num_sync, fp);
  fwrite(&profile->summary.num_mark, sizeof(u32), 1, fp);
  fwrite(profile->summary.mark_info, sizeof(bm_profile_mark_t), profile->summary.num_mark, fp);
  fwrite(&profile->summary.num_copy, sizeof(u32), 1, fp);
  fwrite(profile->summary.copy_info, sizeof(bm_profile_memcpy_t), profile->summary.num_copy, fp);
  fwrite(&profile->summary.num_mem, sizeof(u32), 1, fp);
  fwrite(profile->summary.mem_info, sizeof(bm_profile_mem_info_t), profile->summary.num_mem, fp);

  bool enable_arm_perf = profile->arm_buffer.size>0;
  bool enable_bdc_perf = profile->bdc_buffer.size>0;
  bool enable_gdma_perf = profile->gdma_buffer.size>0;

  if (enable_gdma_perf && arch_info.gdma_deinit){
    arch_info.gdma_deinit(handle, profile, fp);
  }
  if (enable_bdc_perf && arch_info.bdc_deinit){
    arch_info.bdc_deinit(handle, profile, fp);
  }
  if (enable_arm_perf && arch_info.arm_deinit) {
    arch_info.arm_deinit(handle, profile, fp);
  }
  fclose(fp);
  free(profile);
  show_profile_tips();
}

#undef ENV_PREFIX
#undef ENV_GDMA_SIZE
#undef ENV_ARM_SIZE
#undef ENV_BDC_SIZE
#undef ENV_SHOW_SIZE
#undef ENV_ENABLE_PROFILE

void bm_profile_record_sync_end(bm_handle_t handle)
{
  if(handle && handle->profile){
    auto& num_sync = handle->profile->summary.num_sync;
    if(num_sync < PROFILE_MAX_SYNC_API){
      handle->profile->summary.sync_info[num_sync++].end_usec= get_usec();
    } else {
      PROFILE_INFO("Not record sync info due to num_sync >= %d\n", PROFILE_MAX_SYNC_API);
    }
  }
}

void bm_profile_record_sync_begin(bm_handle_t handle)
{
  if(handle && handle->profile){
    auto& num_sync = handle->profile->summary.num_sync;
    if(num_sync < PROFILE_MAX_SYNC_API){
      handle->profile->summary.sync_info[num_sync].begin_usec = get_usec();
    } else {
      PROFILE_INFO("Not record sync info due to num_sync >= %d\n", PROFILE_MAX_SYNC_API);
    }
  }
}

void bm_profile_record_send_api(bm_handle_t handle, u32 api_id)
{
  if(handle && handle->profile){
    auto& num_send = handle->profile->summary.num_send;
    if(num_send< PROFILE_MAX_SEND_API){
      handle->profile->summary.send_info[num_send].begin_usec = get_usec();
      handle->profile->summary.send_info[num_send].api_id = api_id;
      num_send++;
    } else {
      PROFILE_INFO("Not record send info due to num_end >= %d\n", PROFILE_MAX_SEND_API);
    }
  }
}

bm_profile_mark_t *bm_profile_mark_begin(bm_handle_t handle, u64 id)
{
  if(!handle->profile) return NULL;
  auto& num_mark = handle->profile->summary.num_mark;
  if(num_mark>= PROFILE_MAX_MARK) return NULL;
  bm_profile_mark_t* mark = &handle->profile->summary.mark_info[num_mark];
  num_mark++;
  mark->id = id;
  mark->begin_usec = get_usec();
  mark->end_usec = 0;
  return mark;
}

void bm_profile_mark_end(bm_handle_t handle, bm_profile_mark_t *mark)
{
  if(!handle->profile || !mark) return;
  mark->end_usec = get_usec();
}

void bm_profile_record_memcpy_end(bm_handle_t handle, u64 src_addr, u64 dst_addr, u32 size, u32 dir)
{
  if(handle && handle->profile){
    auto& num_copy = handle->profile->summary.num_copy;
    if(num_copy < PROFILE_MAX_MEMCPY){
      auto& copy_info = handle->profile->summary.copy_info[num_copy];
      copy_info.end_usec = get_usec();
      copy_info.src_addr = src_addr;
      copy_info.dst_addr = dst_addr;
      copy_info.size = size;
      copy_info.dir = dir;
      num_copy++;
    } else {
      PROFILE_INFO("Not record copy info due to num_copy >= %d\n", PROFILE_MAX_MEMCPY);
    }
  }

}

void bm_profile_record_memcpy_begin(bm_handle_t handle)
{
  if(handle && handle->profile){
    auto& num_copy = handle->profile->summary.num_copy;
    if(num_copy < PROFILE_MAX_MEMCPY){
      auto& copy_info = handle->profile->summary.copy_info[num_copy];
      copy_info.begin_usec = get_usec();
    } else {
      PROFILE_INFO("Not record copy info due to num_copy >= %d\n", PROFILE_MAX_MEMCPY);
    }
  }
}


void bm_profile_record_mem_end(bm_handle_t handle, bm_mem_op_type_t type, u64 device_addr, u32 size)
{
  if(handle && handle->profile){
    auto& num_mem = handle->profile->summary.num_mem;
    if(num_mem < PROFILE_MAX_MEM_INFO){
      auto& mem_info = handle->profile->summary.mem_info[num_mem];
      mem_info.end_usec = get_usec();
      mem_info.device_addr = device_addr;
      mem_info.type = type;
      mem_info.size = size;
      num_mem++;
    } else {
      PROFILE_INFO("Not record mem info due to num_mem >= %d\n", PROFILE_MAX_MEM_INFO);
    }
  }
}

void bm_profile_record_mem_begin(bm_handle_t handle)
{
  if(handle && handle->profile){
    auto& num_mem = handle->profile->summary.num_mem;
    if(num_mem < PROFILE_MAX_MEM_INFO){
      auto& mem_info = handle->profile->summary.mem_info[num_mem];
      mem_info.begin_usec = get_usec();
    } else {
      PROFILE_INFO("Not record mem info due to num_mem >= %d\n", PROFILE_MAX_MEM_INFO);
    }
  }
}
