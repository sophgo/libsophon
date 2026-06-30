#include "backend/launcher.hpp"
#include "bmlib_runtime.h"
#include "bmruntime.h"
#include <iostream>
#include "string.h"

namespace bmruntime {
extern "C" bm_status_t tpu_kernel_launch_from_multi_cores(
  bm_handle_t handle,
  tpu_kernel_function_t function,
  void *args,
  size_t size,
  int group_num,
  int block_num) __attribute__((weak));

void Launcher_BM1684X2::fill_api_info(const tpu_net_info_t &net_info,
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
  // Check kernel_func_ids for dynamic loading support
  bool use_dynamic_loading = (net_info.kernel_func_ids.size() > 0);
  if (use_dynamic_loading) {
    BMRT_ASSERT_INFO(net_info.core_list.size() == net_info.kernel_func_ids.size(),
                     "core_num=%d, kernel_func_ids.size()=%d",
                     net_info.core_list.size(), net_info.kernel_func_ids.size());
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
    // Use dynamic kernel function ID if available, otherwise fallback to static API ID
    if (use_dynamic_loading) {
      api_info.api_id.push_back(net_info.kernel_func_ids[core_idx]);
    } else {
      api_info.api_id.push_back(BM_API_ID_MULTI_FULLNET);
    }

    api_info.api_data[core_idx].assign(api_buffer_size, 0);
    api_info.input_addr_offset.assign(input_info.size(), 0);
    api_info.output_addr_offset.assign(output_info.size(), 0);

    const auto &tag = m_addr_layout.tag;
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
      if (core_idx > 0 && ((info.compiled_global_addr >> tag.start) & tag.mask) == 0) {
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
      if (core_idx > 0 && ((info.compiled_global_addr >> tag.start) & tag.mask) == 0) {
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


bm_status_t Launcher_BM1684X2::static_subnet(
    bm_handle_t handle, const tpu_net_info_t &net_info) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");

  api_info_t api_info;
  fill_api_info(net_info, api_info);

  size_t block_num = net_info.core_list.size();
  size_t total_size = 0;
  std::vector<u32> block_sizes(block_num);

  for (size_t core_idx = 0; core_idx < block_num; core_idx++) {
    block_sizes[core_idx] = (u32)api_info.api_data[core_idx].size();
    total_size += block_sizes[core_idx];
  }

  // unit test for multi-group
  const char *enable_dp = getenv("BMRT_DP_NUM");
  int dp_num = 1;
  if (enable_dp) {
    dp_num = std::stoi(enable_dp);
  }

  std::vector<u8> merged_api_data(total_size * dp_num, 0);
  u8* p_merged = merged_api_data.data();

  for (size_t core_idx = 0; core_idx < block_num; core_idx++) {
    memcpy(p_merged, api_info.api_data[core_idx].data(), block_sizes[core_idx]);
    p_merged += block_sizes[core_idx];
  }

  if (enable_dp) {
    auto fix_io = [&net_info](std::vector<tpu_tensor_info_t> &io_infos,
                              tpu_net_info_t &group_net) {
      for (size_t k = 0; k < io_infos.size(); k++) {
        auto &info = io_infos.at(k);
        if (((info.compiled_global_addr >> 40) & 0x1f) == 0) {
          uint64_t offset =
              info.compiled_global_addr - net_info.neuron_start_addr[0];
          info.compiled_global_addr = group_net.neuron_start_addr[0] + offset;
        }
      }
    };

    uint64_t neuron_size_per_dp = net_info.neuron_size[0] / dp_num;
    for (int i = 1; i < dp_num; i++) {
      api_info_t group_api;
      tpu_net_info_t group_net = {.input_info = net_info.input_info,
                                  .output_info = net_info.output_info,
                                  .reloc_base_addrs = net_info.reloc_base_addrs,
                                  .core_commands = net_info.core_commands,
                                  .core_list = net_info.core_list,
                                  .kernel_func_ids = net_info.kernel_func_ids,
                                  .coeff_start_addr = net_info.coeff_start_addr,
                                  .neuron_start_addr =
                                      net_info.neuron_start_addr,
                                  .neuron_size = net_info.neuron_size,
                                  .do_allreduce = net_info.do_allreduce,
                                  .allreduce_param = net_info.allreduce_param,
                                  .addr_mode = net_info.addr_mode};
      // fix neuron addr
      group_net.neuron_start_addr[0] =
          net_info.neuron_start_addr[0] + i * neuron_size_per_dp;
      // fix io addr
      fix_io(group_net.input_info, group_net);
      fix_io(group_net.output_info, group_net);
      fill_api_info(group_net, group_api);
      for (size_t core_idx = 0; core_idx < block_num; core_idx++) {
        memcpy(p_merged, group_api.api_data[core_idx].data(),
               group_api.api_data[core_idx].size());
        p_merged += group_api.api_data[core_idx].size();
      }
    }
  }

  int group_num = dp_num;
  bm_status_t status;
  if (tpu_kernel_launch_from_multi_cores) {
    status = tpu_kernel_launch_from_multi_cores(
        handle,
        (bm_api_id_t)api_info.api_id[0],
        merged_api_data.data(),
        merged_api_data.size(),
        group_num,
        (int)block_num);
  } else {
    status = BM_ERR_NOFEATURE;
    BMRT_LOG(WRONG, "tpu_kernel_launch_from_multi_cores not found, api id:%d, status:%d",
             api_info.api_id[0], status);
  }
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "tpu_kernel_launch_from_multi_cores failed, api id:%d, status:%d",
             api_info.api_id[0], status);
  }

  return status;
}

bm_status_t Launcher_BM1684X2::dynamic_subnet(
    bm_handle_t handle, const tpu_dynamic_net_info_t &net_info)
{
  const auto &inputs = net_info.inputs;
  const auto &outputs = net_info.outputs;
  const bool get_output_shape = true;
  BMRT_ASSERT_INFO(net_info.core_ids.size() == 1,
                   "Dynamic compile do not support tensor parallel\n");
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
  BMRT_ASSERT_INFO(net_info.ctx_mem_borders.size() == net_info.ctx_mem_offsets.size(),
                   "ctx borders and offset should have same size");

  size_t ctx_num = net_info.ctx_mem_borders.size();
  u32 api_buffer_size =
      sizeof(u64) + sizeof(u32) + // compiled_ir addr, length
                                  // input num
      sizeof(u32) +
      //           input_addr    dtype_dims        dim_shape elem_num
      inputs.size() * (sizeof(u64) + sizeof(int) + sizeof(int) * BM_MAX_DIMS_NUM +
                   sizeof(int)) +
      // output num
      sizeof(u32) +
      //           output_addr
      outputs.size() * sizeof(u64) +
      // get_output_shape, global_shape_mem_addr, apd_ctx_start, (ctx_num,
      // apd_ctx_mem_borders, apd_ctx_mem_offset),
      sizeof(u32) + sizeof(u64) + sizeof(u64) +
      (sizeof(u32) + sizeof(u64) * ctx_num * 2) +
      // apd_coeff_mem_offset, apd_io_start, apd_io_mem_offset
      sizeof(u64) + sizeof(u64) + sizeof(u64);

  if (api_buffer_size > MAX_API_MSG_SIZE) {
    // decrease the api buffer size
    for (u32 i = 0; i < inputs.size(); ++i) {
      u32 cur_dim = inputs[i].dim;
      api_buffer_size -= (BM_MAX_DIMS_NUM - cur_dim) * sizeof(int);
    }
  }

  u8 *api_buffer = new u8[api_buffer_size]();

  void *p_api = api_buffer;
  // compiled ir information
  *(u64 *)p_api = net_info.ir_addr;
  p_api = (u64 *)p_api + 1;
  *(u32 *)p_api = net_info.ir_word_num;
  p_api = (u32 *)p_api + 1;

  // input information
  *(u32 *)p_api = inputs.size();
  p_api = (u32 *)p_api + 1;

  for (u32 i = 0; i < inputs.size(); ++i) {
    *(u64 *)p_api = inputs[i].addr;
    p_api = (u64 *)p_api + 1;

    *(u32 *)p_api = inputs[i].packed;
    p_api = (u32 *)p_api + 1;
    u32 cur_dim = inputs[i].dim;
    for (u32 j = 0; j < cur_dim; j++) {
      *(u32 *)p_api = inputs[i].shape[j];
      p_api = (u32 *)p_api + 1;
    }
    *(u32 *)p_api = inputs[i].elem_num;
    p_api = (u32 *)p_api + 1;
  }
  // output information
  *(u32 *)p_api = outputs.size();
  p_api = (u32 *)p_api + 1;

  for (u32 i = 0; i < outputs.size(); ++i) {
    *(u64 *)p_api = outputs[i].addr;
    p_api = (u64 *)p_api + 1;
  }
  // output shape info related
  *(u32 *)p_api = (u32)get_output_shape;
  p_api = (u32 *)p_api + 1;
  *(u64 *)p_api = net_info.output_shape_addr;
  p_api = (u64 *)p_api + 1;

  // The memory address in cmd gdma need to be offset when append context,here
  // is the offset value.
  *(u64 *)p_api = net_info.ctx_start_addr;
  p_api = (u64 *)p_api + 1;

  *(u32 *)p_api = ctx_num;
  p_api = (u32 *)p_api + 1;

  for (size_t i = 0; i < ctx_num; ++i) {
    *(u64 *)p_api = net_info.ctx_mem_borders[i];
    p_api = (u64 *)p_api + 1;
  }
  for (size_t i = 0; i < ctx_num; ++i) {
    *(u64 *)p_api = net_info.ctx_mem_offsets[i];
    p_api = (u64 *)p_api + 1;
  }

  *(u64 *)p_api = net_info.coeff_offset_addr;
  p_api = (u64 *)p_api + 1;

  *(u64 *)p_api = net_info.io_start_addr;
  p_api = (u64 *)p_api + 1;
  *(u64 *)p_api = net_info.io_mem_offset;
  p_api = (u64 *)p_api + 1;

  bm_status_t status =
      bm_send_api(handle, (bm_api_id_t)BM_API_ID_DYNAMIC_FULLNET, api_buffer,
                  api_buffer_size);
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d",
             BM_API_ID_DYNAMIC_FULLNET, status);
  } else {
    status = bm_sync_api(handle);
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "bm_sync_api failed, api id:%d, status:%d",
               BM_API_ID_DYNAMIC_FULLNET, status);
    }
  }

  delete[] api_buffer;
  return status;
}

bm_status_t
Launcher_BM1684X2::_bmdnn_set_profile_enable_(bm_handle_t handle, int core,
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

bm_status_t Launcher_BM1684X2::_bmdnn_get_profile_data_(
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
typedef struct bm_api_engine_profile_param {
  int engine;
  unsigned long long addr;
  unsigned long long size;
} bm_api_engine_profile_param_t;
#pragma pack()

bm_status_t Launcher_BM1684X2::_bmdnn_set_engine_profile_param_(
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
