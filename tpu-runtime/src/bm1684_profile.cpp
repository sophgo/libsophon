#include "bm1684_profile.h"
#include <memory.h>
#include <stdio.h>
#include "bmruntime.h"

#define BLOCK_SIZE 4096
#define BURST_LEN 192

namespace bm1684_profile {
void __fix_data(unsigned char* aligned_data, unsigned int len, size_t offset)
{
  if (len < BLOCK_SIZE) return;
  __fix_data(&aligned_data[BLOCK_SIZE], len - BLOCK_SIZE, offset + BLOCK_SIZE);
  unsigned int max_copy_len =
      (offset + BLOCK_SIZE + BURST_LEN - 1) / BURST_LEN * BURST_LEN - offset - BLOCK_SIZE;
  unsigned int copy_len = (max_copy_len < len - BLOCK_SIZE) ? max_copy_len : (len - BLOCK_SIZE);
  if (copy_len > 0) {
    memcpy(&aligned_data[BLOCK_SIZE],aligned_data, copy_len);
  }
  return;
}
/*
static void save_data(const char* name, const unsigned char* data, int len, int line_size=BLOCK_SIZE){
    FILE* fp = fopen(name, "w");
    fprintf(fp, "len=%d ", (int)len);
    int block_num = (len+line_size-1)/line_size;
    for(int i=0; i<block_num; i++){
        for(int j=0; j<line_size; j++){
            int index = i*line_size+j;
            if(index>= len) break;
            fprintf(fp, "%02x ", (0x0FF&data[index]));
        }
        fprintf(fp,"\n");
    }
    fclose(fp);
}
*/
unsigned int fix_gdma_monitor_data(unsigned char* aligned_data, unsigned int len){
    unsigned int valid_len = len;
    unsigned int item_size = sizeof(GDMA_PROFILE_FORMAT);
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

bool BMProfileDevice::init()
{
    if(!enable) {
        return false;
    }
    if(enable_arm){
      profile->alloc_buffer(&dyn_buffer, dyn_max_size, "dyn_profile");
    }
    if(enable_bdc){
      u64 tpu_device_buffer_size = bdc_record_len * sizeof(TPU_PROFILE_FORMAT);
      profile->alloc_buffer(&tpu_buffer, tpu_device_buffer_size, "bdc_perf_monitor");
      tpu_perf_monitor.buffer_start_addr = bm_mem_get_device_addr(tpu_buffer.mem);
      tpu_perf_monitor.buffer_size = bm_mem_get_device_size(tpu_buffer.mem);
      tpu_perf_monitor.monitor_id = PERF_MONITOR_TPU;
    }
    if(enable_gdma){
      u64 gdma_device_buffer_size = gdma_record_len * sizeof(GDMA_PROFILE_FORMAT);
      profile->alloc_buffer(&gdma_buffer, gdma_device_buffer_size, "gdma_perf_monitor");
      // set gdma_start_offset to aligned(start,4K)+192 to assure the front of data is right,
      // not to override by hw bug.
      const int align_number = 4096;
      const int burst_len = 192;
      u64 real_gdma_mem_addr = bm_mem_get_device_addr(gdma_buffer.mem);
      u64 real_gdma_mem_size = bm_mem_get_device_size(gdma_buffer.mem);
      u64 aligned_addr = ALIGN(real_gdma_mem_addr, align_number);
      u64 aligned_size = real_gdma_mem_size + real_gdma_mem_addr - aligned_addr;
      aligned_mem = gdma_buffer.mem;
      bm_mem_set_device_addr(&aligned_mem, aligned_addr);
      bm_mem_set_device_size(&aligned_mem, aligned_size);

      gdma_perf_monitor.buffer_start_addr = aligned_addr + burst_len;
      gdma_perf_monitor.buffer_size = aligned_size - burst_len;
      gdma_perf_monitor.monitor_id = PERF_MONITOR_GDMA;
    }
    return true;
}

bool BMProfileDevice::begin(net_ctx_t* net_ctx)
{
    bm_status_t ret = BM_SUCCESS;
    auto handle = profile->handle;
    if(enable_bdc){
        memset(tpu_buffer.ptr, -1, tpu_buffer.size);
        ret = bm_memcpy_s2d(handle, tpu_buffer.mem, tpu_buffer.ptr);
        if (ret != BM_SUCCESS) {
            BMRT_LOG(FATAL, "init device buffer data failed, ret = %d\n", ret);
        }
        bm_enable_perf_monitor(handle, &tpu_perf_monitor);
    }
    if(enable_gdma){
        memset(gdma_buffer.ptr, -1, gdma_buffer.size);
        ret = bm_memcpy_s2d(handle, aligned_mem, gdma_buffer.ptr);
        if (ret != BM_SUCCESS) {
            BMRT_LOG(FATAL, "init device buffer data failed, ret = %d\n", ret);
        }
        bm_enable_perf_monitor(handle, &gdma_perf_monitor);
    }

    // enable dynamic profile
    if(enable_arm){
        ret = bmfunc::bmdnn_1684()->_bmdnn_set_profile_enable_(handle, true);
        CHECK_status(ret);
    }
    return true;
}

bool BMProfileDevice::end(net_ctx_t* net_ctx)
{
    auto handle = profile->handle;
    int ret = BM_SUCCESS;
    if (enable_bdc){
        bm_disable_perf_monitor(handle, &tpu_perf_monitor);
        ret = bm_memcpy_d2s(handle, tpu_buffer.ptr, tpu_buffer.mem);
        if (ret != BM_SUCCESS) {
            BMRT_LOG(FATAL, "Get monitor profile from device to system failed, ret = %d\n", ret);
        }
        size_t valid_len = 0;
        auto tpu_data = (TPU_PROFILE_FORMAT *)tpu_buffer.ptr;
        while (tpu_data[valid_len].inst_start_time != 0xFFFFFFFF && tpu_data[valid_len].inst_end_time != 0xFFFFFFFF && valid_len < bdc_record_len) valid_len++;
        BMRT_LOG(INFO, "bdc record_num=%d, max_record_num=%d", (int)valid_len, (int)bdc_record_len);
        profile->write_block(BLOCK_MONITOR_BDC, valid_len * sizeof(tpu_data[0]), tpu_data);
    }
    if (enable_gdma){
        bm_disable_perf_monitor(handle, &gdma_perf_monitor);
        ret = bm_memcpy_d2s(handle, gdma_buffer.ptr, aligned_mem);
        if (ret != BM_SUCCESS) {
            BMRT_LOG(FATAL, "Get monitor profile from device to system failed, ret = %d\n", ret);
        }
        size_t valid_len =
                fix_gdma_monitor_data(gdma_buffer.ptr, bm_mem_get_device_size(aligned_mem));
        BMRT_LOG(INFO, "gdma record_num=%d, max_record_num=%d",
                 (int)(valid_len/sizeof(GDMA_PROFILE_FORMAT)),
                 (int)gdma_record_len);
        profile->write_block(BLOCK_MONITOR_GDMA, valid_len, gdma_buffer.ptr);
    }
    if (enable_arm) {
        for(int i=0; i<2; i++){
            vector<u8> data;
            size_t offset = 0;
            size_t total_len = 0;
            u32 block_type = (i == 0) ? BLOCK_DYN_DATA : BLOCK_DYN_EXTRA;
            while(1){
                bm_status_t status = bmfunc::bmdnn_1684()->_bmdnn_get_profile_data_(
                            handle,
                            bm_mem_get_device_addr(dyn_buffer.mem),
                            bm_mem_get_device_size(dyn_buffer.mem),
                            offset, i);
                CHECK_status(status);
                status = bm_memcpy_d2s(handle, dyn_buffer.ptr, dyn_buffer.mem);
                CHECK_status(status);
                auto u32_ptr = (u32*)dyn_buffer.ptr;
                auto read_len = u32_ptr[0];
                if(total_len==0){
                    total_len = u32_ptr[1];
                }
                auto data_ptr = (u8*)&u32_ptr[2];
                data.insert(data.end(), data_ptr, data_ptr + read_len);
                offset += read_len;
                if(offset>=total_len) break;
            }
            profile->write_block(block_type, data.size(), data.data());
        }
        bm_status_t status = bmfunc::bmdnn_1684()->_bmdnn_set_profile_enable_(handle, false);
        CHECK_status(status);
    }
    return true;
}

void BMProfileDevice::deinit()
{
  if (!enable) return;
  profile->free_buffer(&tpu_buffer);
  profile->free_buffer(&gdma_buffer);
  profile->free_buffer(&dyn_buffer);
}

bool BMProfileDevice::enabled()
{
    return enable;
}

}
#undef BLOCK_SIZE
#undef BURST_LEN
