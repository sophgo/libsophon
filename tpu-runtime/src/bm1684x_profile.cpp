#include <memory.h>
#include <stdio.h>
#include "bmruntime.h"
#include "bm1684x_profile.h"

namespace bm1684x_profile {

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
      gdma_perf_monitor.buffer_start_addr = bm_mem_get_device_addr(gdma_buffer.mem);
      gdma_perf_monitor.buffer_size = bm_mem_get_device_size(gdma_buffer.mem);
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
        ret = bm_memcpy_s2d(handle, gdma_buffer.mem, gdma_buffer.ptr);
        if (ret != BM_SUCCESS) {
            BMRT_LOG(FATAL, "init device buffer data failed, ret = %d\n", ret);
        }
        bm_enable_perf_monitor(handle, &gdma_perf_monitor);
    }

    // enable dynamic profile
    if(enable_arm){
        auto enable_func_id = net_ctx->kernel_module_->get_enable_profile_func_id();
        ret = bmfunc::bmdnn_1684x()->_bmdnn_set_profile_enable_(handle, enable_func_id, true);
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
        ret = bm_memcpy_d2s(handle, gdma_buffer.ptr, gdma_buffer.mem);
        if (ret != BM_SUCCESS) {
            BMRT_LOG(FATAL, "Get monitor profile from device to system failed, ret = %d\n", ret);
        }
        auto gdma_data = (GDMA_PROFILE_FORMAT*)gdma_buffer.ptr;
        size_t valid_len = 0;
        while (gdma_data[valid_len].content.inst_start_time != 0xFFFFFFFF && gdma_data[valid_len].content.inst_end_time != 0xFFFFFFFF && valid_len < gdma_record_len)
            valid_len++;
        BMRT_LOG(INFO, "gdma record_num=%d, max_record_num=%d", (int)valid_len, (int)gdma_record_len);
        std::vector<GDMA_PROFILE_FORMAT_CONTENT> valid_data(valid_len);
        for(size_t i=0; i<valid_len; i++){
            valid_data[i] = gdma_data[i].content;
        }
        profile->write_block(BLOCK_MONITOR_GDMA, valid_len * sizeof(GDMA_PROFILE_FORMAT_CONTENT), valid_data.data());
    }
    if (enable_arm) {
        for(int i=0; i<2; i++){
            vector<u8> data;
            size_t offset = 0;
            size_t total_len = 0;
            u32 block_type = (i == 0) ? BLOCK_DYN_DATA : BLOCK_DYN_EXTRA;
            while(1){
                auto get_func_id = net_ctx->kernel_module_->get_get_profile_func_id();
                bm_status_t status = bmfunc::bmdnn_1684x()->_bmdnn_get_profile_data_(
                            handle,
                            get_func_id,
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
        auto enable_func_id = net_ctx->kernel_module_->get_enable_profile_func_id();
        bm_status_t status = bmfunc::bmdnn_1684x()->_bmdnn_set_profile_enable_(handle, enable_func_id, false);
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
