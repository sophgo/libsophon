#include <memory.h>
#include <stdio.h>
#include "bmruntime.h"
#include "cv184x_profile.h"

namespace cv184x_profile {

bool BMProfileDevice::init()
{
    if(!enable) {
        return false;
    }
    auto& core_list = profile->get_core_list();
    auto core_num = core_list.size();
    this->buffers.resize(core_num);
    for(auto& buffer: this->buffers){
        if(enable_bdc){
            u64 tpu_device_buffer_size = bdc_record_len * sizeof(TPU_PROFILE_FORMAT);
            profile->alloc_buffer(&buffer.tiu, tpu_device_buffer_size, "bdc_perf_monitor");
        }
        if(enable_gdma){
            u64 gdma_device_buffer_size = gdma_record_len * sizeof(GDMA_PROFILE_FORMAT);
            profile->alloc_buffer(&buffer.gdma, gdma_device_buffer_size, "gdma_perf_monitor");
        }
    }
    return true;
}

bool BMProfileDevice::begin(net_ctx_t* net_ctx)
{
    bm_status_t ret = BM_SUCCESS;
    auto handle = profile->get_handle();
    auto& core_list = profile->get_core_list();
    auto enable_bits = ((!!enable_gdma)<<PROFILE_ENGINE_GDMA) |
                    ((!!enable_bdc)<<PROFILE_ENGINE_TIU) |
                    ((!!enable_arm)<<PROFILE_ENGINE_MCU);
    auto set_func_ids = net_ctx->kernel_module_->get_set_engine_profile_param_func_id(core_list);
    auto enable_func_ids = net_ctx->kernel_module_->get_enable_profile_func_id(core_list);
    auto &launcher = profile->get_bmrt()->backend()->launcher();
    for(size_t i=0; i<core_list.size(); i++){
        if(enable_bdc){
            BMRT_LOG(INFO, "enable tiu profiling!");
            auto& tiu_buffer = buffers[i].tiu;
            memset(tiu_buffer.ptr, 0, tiu_buffer.size);
            ret = bm_memcpy_s2d(handle, tiu_buffer.mem, tiu_buffer.ptr);
            if (ret != BM_SUCCESS) {
                BMRT_LOG(FATAL, "init device buffer data failed, ret = %d\n", ret);
            }
            dynamic_cast<Launcher_CV184X*>(launcher.get())->_bmdnn_set_engine_profile_param_(
                handle, core_list[i], set_func_ids[i],
                PROFILE_ENGINE_TIU,
                bm_mem_get_device_addr(tiu_buffer.mem),
                bm_mem_get_device_size(tiu_buffer.mem)
            );
        }
        if(enable_gdma){
            auto& gdma_buffer = buffers[i].gdma;
            memset(gdma_buffer.ptr, 0, gdma_buffer.size);
            ret = bm_memcpy_s2d(handle, gdma_buffer.mem, gdma_buffer.ptr);
            if (ret != BM_SUCCESS) {
                BMRT_LOG(FATAL, "init device buffer data failed, ret = %d\n", ret);
            }
            dynamic_cast<Launcher_CV184X*>(launcher.get())->_bmdnn_set_engine_profile_param_(
                handle, core_list[i], set_func_ids[i],
                PROFILE_ENGINE_GDMA,
                bm_mem_get_device_addr(gdma_buffer.mem),
                bm_mem_get_device_size(gdma_buffer.mem)
            );
        }
        // enable dynamic profile
        ret = dynamic_cast<Launcher_CV184X*>(launcher.get())->_bmdnn_set_profile_enable_(handle, core_list[i], enable_func_ids[i], enable_bits);
        CHECK_status(ret);
    }
    return true;
}

bool BMProfileDevice::end(net_ctx_t* net_ctx)
{
    auto handle = profile->get_handle();
    int ret = BM_SUCCESS;
    auto& core_list = profile->get_core_list();
    auto &launcher = profile->get_bmrt()->backend()->launcher();
    for(size_t i=0; i<core_list.size(); i++){
        if (enable_bdc){
            auto& tiu_buffer = buffers[i].tiu;
            ret = bm_memcpy_d2s(handle, tiu_buffer.ptr, tiu_buffer.mem);
            if (ret != BM_SUCCESS) {
                BMRT_LOG(FATAL, "Get monitor profile from device to system failed, ret = %d\n", ret);
            }
            size_t valid_len = 0;
            auto tpu_data = (TPU_PROFILE_FORMAT *)tiu_buffer.ptr;
            while (tpu_data[valid_len].inst_start_time != 0 && tpu_data[valid_len].inst_end_time != 0 && valid_len < bdc_record_len) valid_len++;
            BMRT_LOG(INFO, "core_index=%d, core_id=%d, bdc record_num=%d, max_record_num=%d", (int)i, (int)core_list[i], (int)valid_len, (int)bdc_record_len);
            profile->write_block(BLOCK_MONITOR_BDC, valid_len * sizeof(tpu_data[0]), tpu_data);
        }
        if (enable_gdma){
            auto& gdma_buffer = buffers[i].gdma;
            ret = bm_memcpy_d2s(handle, gdma_buffer.ptr, gdma_buffer.mem);
            if (ret != BM_SUCCESS) {
                BMRT_LOG(FATAL, "Get monitor profile from device to system failed, ret = %d\n", ret);
            }
            auto gdma_data = (GDMA_PROFILE_FORMAT*)gdma_buffer.ptr;
            size_t valid_len = 0;
            while (gdma_data[valid_len].inst_start_time != 0 && gdma_data[valid_len].inst_end_time != 0 && valid_len < gdma_record_len) valid_len++;
            BMRT_LOG(INFO, "core_index=%d, core_id=%d, gdma record_num=%d, max_record_num=%d", (int)i, (int)core_list[i], (int)valid_len, (int)gdma_record_len);
            profile->write_block(BLOCK_MONITOR_GDMA, valid_len * sizeof(GDMA_PROFILE_FORMAT), gdma_data);
        }
        auto get_func_ids = net_ctx->kernel_module_->get_get_profile_func_id(core_list);
        auto enable_func_ids = net_ctx->kernel_module_->get_enable_profile_func_id(core_list);
        bm_status_t status = dynamic_cast<Launcher_CV184X*>(launcher.get())->_bmdnn_set_profile_enable_(handle, core_list[0], enable_func_ids[0], 0);
    }
    return true;
}

void BMProfileDevice::deinit()
{
  if (!enable) return;
  for (auto& buffer: buffers){
    profile->free_buffer(&buffer.tiu);
    profile->free_buffer(&buffer.gdma);
    profile->free_buffer(&buffer.mcu);
  }
}

bool BMProfileDevice::enabled()
{
    return enable;
}

}
#undef BLOCK_SIZE
#undef BURST_LEN
