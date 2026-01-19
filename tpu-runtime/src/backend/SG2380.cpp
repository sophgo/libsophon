#include "backend/launcher.hpp"
#include "bmlib_runtime.h"
#include "bmruntime.h"
#include <iostream>

namespace bmruntime {
extern "C" bm_status_t bm_send_api_to_core(bm_handle_t handle, int api_id,
                                           const u8 *api, u32 size,
                                           int core_id);

void Launcher_SG2380::fill_api_info(const tpu_net_info_t &net_info,
                                    api_info_t &api_info) {
  BMRT_ASSERT_INFO(net_info.neuron_start_addr.size() == 1,
                   "only support one neuron addr");
  const std::vector<tpu_tensor_info_t> &input_info = net_info.input_info;
  const std::vector<tpu_tensor_info_t> &output_info = net_info.output_info;
  api_info.api_data.resize(net_info.core_commands.size());
  int base_message_id = 0;
  for (auto core_id : net_info.core_list) {
    base_message_id |= (1 << core_id);
  }
  for (size_t core_idx = 0; core_idx < net_info.core_list.size(); core_idx++) {
    const std::vector<tpu_cmd_info_t> &cmd_info =
        net_info.core_commands[core_idx].cmd_info;

    u32 api_buffer_size =
        sizeof(int) +
        (input_info.size() * (sizeof(u64) * 2 + sizeof(u32))) + // input
        sizeof(int) +
        (output_info.size() * (sizeof(u64) * 2 + sizeof(u32))) + // output
        sizeof(u64) * 2 +
        (sizeof(int) * 2 + sizeof(u32) * 2) * cmd_info.size() + sizeof(int) +
        2 * sizeof(u64) + sizeof(int); // base message id
    api_info.api_id.push_back(BM_API_ID_MULTI_FULLNET);
    api_info.api_data[core_idx].assign(api_buffer_size, 0);
    api_info.input_addr_offset.assign(input_info.size(), 0);
    api_info.output_addr_offset.assign(output_info.size(), 0);

    void *p_api = api_info.api_data[core_idx].data();
    // input global offset process
    *(int *)p_api = input_info.size();
    p_api = (int *)p_api + 1;
    for (size_t i = 0; i < input_info.size(); ++i) {
      const auto &info = input_info.at(i);
      api_info.input_addr_offset.at(i) =
          (uint8_t *)p_api - (uint8_t *)(api_info.api_data.data());
      *(u64 *)p_api = info.user_global_addr;
      p_api = (u64 *)p_api + 1;
      if (core_idx > 0 && ((info.compiled_global_addr >> 40) & 0x1f) == 0) {
        /// If the bmodel use multi core, we only move the user's input data to
        /// compiled ddr once.
        *(u64 *)p_api = info.user_global_addr;
      } else {
        *(u64 *)p_api = info.compiled_global_addr;
      }
      p_api = (u64 *)p_api + 1;
      *(u32 *)p_api = bmrt_data_type_size((bm_data_type_t)info.dtype) *
                      (info.n * info.c * info.h * info.w);
      p_api = (u32 *)p_api + 1;
    }

    // output global offset process
    *(int *)p_api = output_info.size();
    p_api = (int *)p_api + 1;
    for (size_t i = 0; i < output_info.size(); ++i) {
      const auto &info = output_info.at(i);
      api_info.output_addr_offset.at(i) =
          (uint8_t *)p_api - (uint8_t *)(api_info.api_data.data());
      *(u64 *)p_api = info.user_global_addr;
      p_api = (u64 *)p_api + 1;
      if (core_idx > 0 && ((info.compiled_global_addr >> 40) & 0x1f) == 0) {
        /// If the bmodel use multi core, we only move the user's input data to
        /// compiled ddr once.
        *(u64 *)p_api = info.user_global_addr;
      } else {
        *(u64 *)p_api = info.compiled_global_addr;
      }
      p_api = (u64 *)p_api + 1;
      *(u32 *)p_api = bmrt_data_type_size((bm_data_type_t)info.dtype) *
                      (info.n * info.c * info.h * info.w);
      p_api = (u32 *)p_api + 1;
    }

    // memcpy cmd offset and num
    *(u64 *)p_api = net_info.core_commands[core_idx].bdc_cmd_addr;
    p_api = (u64 *)p_api + 1;
    *(u64 *)p_api = net_info.core_commands[core_idx].gdma_cmd_addr;
    p_api = (u64 *)p_api + 1;
    *(int *)p_api = cmd_info.size();
    p_api = (int *)p_api + 1;
    for (size_t i = 0; i < cmd_info.size(); i++) {
      const tpu_cmd_info_t info = cmd_info.at(i);
      *(int *)p_api = info.bdc_cmd_num;
      p_api = (int *)p_api + 1;
      *(int *)p_api = info.gdma_cmd_num;
      p_api = (int *)p_api + 1;
      *(u32 *)p_api = info.bdc_cmd_byte_size;
      p_api = (u32 *)p_api + 1;
      *(u32 *)p_api = info.gdma_cmd_byte_size;
      p_api = (u32 *)p_api + 1;
    }

    *((u64 *)p_api) = net_info.coeff_start_addr;
    p_api = ((u64 *)p_api) + 1;
    *((u64 *)p_api) = net_info.neuron_start_addr[0];
    p_api = ((u64 *)p_api) + 1;
    *((int *)p_api) = base_message_id;
    p_api = ((u32 *)p_api) + 1;
  }

  BMRT_LOG_RUN(DEBUG, {
    for (size_t i = 0; i < input_info.size(); ++i) {
      const auto &info = input_info.at(i);
      auto byte_size = bmrt_data_type_size((bm_data_type_t)info.dtype) *
                       (info.n * info.c * info.h * info.w);
      BMRT_LOG(DEBUG,
               "in[%d] user_addr=0x%llx, cmd_addr=0x%llx, shape=[%d, %d, %d, "
               "%d], dtype=%s, byte_size=%d",
               i, info.user_global_addr, info.compiled_global_addr, info.n,
               info.c, info.h, info.w,
               dtype_to_string((bm_data_type_t)info.dtype), byte_size);
    }
    for (size_t i = 0; i < output_info.size(); ++i) {
      const auto &info = output_info.at(i);
      auto byte_size = bmrt_data_type_size((bm_data_type_t)info.dtype) *
                       (info.n * info.c * info.h * info.w);
      BMRT_LOG(DEBUG,
               "out[%d] user_addr=0x%llx, cmd_addr=0x%llx, shape=[%d, %d, %d, "
               "%d], dtype=%s, byte_size=%d",
               i, info.user_global_addr, info.compiled_global_addr, info.n,
               info.c, info.h, info.w,
               dtype_to_string((bm_data_type_t)info.dtype), byte_size);
    }
    for (size_t core_idx = 0; core_idx < net_info.core_list.size();
         core_idx++) {
      BMRT_LOG(DEBUG, "core[%d], tiu_cmd_addr=0x%llx, gdma_cmd_addr=0x%llx",
               core_idx, net_info.core_commands[core_idx].bdc_cmd_addr,
               net_info.core_commands[core_idx].gdma_cmd_addr);
    }
    BMRT_LOG(DEBUG,
             "coeff_addr=0x%llx, neuron_addr=0x%llx , base_message_id=%d",
             net_info.coeff_start_addr, net_info.neuron_start_addr[0],
             base_message_id);
  });
}

bm_status_t Launcher_SG2380::static_subnet(bm_handle_t handle,
                                           const tpu_net_info_t &net_info) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");

  api_info_t api_info;
  fill_api_info(net_info, api_info);
  bm_status_t status = BM_SUCCESS;
  for (size_t core_idx = 0; core_idx < net_info.core_list.size(); core_idx++) {
    bm_status_t core_status = bm_send_api_to_core(
        handle, (bm_api_id_t)api_info.api_id[0],
        api_info.api_data[core_idx].data(), api_info.api_data[core_idx].size(),
        net_info.core_list.at(core_idx));
    if (BM_SUCCESS != core_status) {
      status = (status == BM_SUCCESS) ? core_status : status;
      BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d",
               BM_API_ID_MULTI_FULLNET, core_status);
    }
  }
  return status;
}

bm_status_t Launcher_SG2380::dynamic_subnet(
    bm_handle_t handle, const tpu_dynamic_net_info_t &net_info)
{
  BMRT_ASSERT_INFO(0, "Not supported for dynamic subnet");
  return BM_ERR_FAILURE;
}

bm_status_t
Launcher_SG2380::_bmdnn_set_profile_enable_(bm_handle_t handle, int core,
                                            tpu_kernel_function_t func_id,
                                            unsigned int enable_bits) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
  u32 api_buffer_size = sizeof(u32);
  u32 profile_enable = enable_bits;
  bm_status_t status = tpu_kernel_launch_async_from_core(
      handle, func_id, (u8 *)&profile_enable, api_buffer_size, core);
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "launch kernel failed: core_id:%d, func id:%d, status:%d",
             core, func_id, status);
  }
  return status;
}

bm_status_t Launcher_SG2380::_bmdnn_get_profile_data_(
    bm_handle_t handle, int core, tpu_kernel_function_t func_id,
    unsigned long long output_global_addr, unsigned int output_max_size,
    unsigned int byte_offset,
    unsigned int data_category // 0: profile time records, 1: extra data
) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
#pragma pack(1)
  struct {
    u64 arm_reserved_addr;
    u64 output_global_addr;
    u32 output_size;
    u32 byte_offset;
    u32 data_category; // 0: profile_data, 1: profile extra data
  } api_data;
#pragma pack()

  const u32 api_buffer_size = sizeof(api_data);

  api_data.arm_reserved_addr = -1;
  api_data.output_global_addr = output_global_addr;
  api_data.output_size = output_max_size;
  api_data.byte_offset = byte_offset;
  api_data.data_category = data_category;

  bm_status_t status = tpu_kernel_launch_async_from_core(
      handle, func_id, (u8 *)&api_data, api_buffer_size, core);
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG,
             "tpu_kernel_launch_async_from_core failed, cor_id:%d, api id:%d, "
             "status:%d",
             core, func_id, status);
  } else {
    status = bm_thread_sync_from_core(handle, core);
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "bm_sync_api failed, core_id:%d, api id:%d, status:%d",
               core, func_id, status);
    }
  }
  return status;
}

#pragma pack(1)
typedef struct {
  int engine;
  unsigned long long addr;
  unsigned long long size;
} bm_api_engine_profile_param_t;
#pragma pack()

bm_status_t Launcher_SG2380::_bmdnn_set_engine_profile_param_(
    bm_handle_t handle, int core, tpu_kernel_function_t func_id,
    int engine_type, unsigned long long addr, unsigned long long size) {
  bm_api_engine_profile_param_t param;
  param.engine = engine_type;
  param.addr = addr;
  param.size = size;
  bm_status_t core_status = tpu_kernel_launch_async_from_core(
      handle, func_id, (u8 *)&param, sizeof(param), core);
  return core_status;
}

} // namespace bmruntime
