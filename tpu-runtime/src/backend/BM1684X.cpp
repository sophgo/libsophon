#include "backend/backend.hpp"
#include "bmruntime.h"

namespace bmruntime {
bool Backend_BM1684X::convert_gdma(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);

  uint64_t offset = 0;
  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; cmd_idx++) {
    uint32_t cmd_size =
        gdma_cmd_size(src_ptr, offset, cmd_idx == params.num_cmd - 1);
    memcpy(dst_ptr, src_ptr, cmd_size);
    uint32_t cmd_type = dst_ptr[1] & 0xf;
    if (cmd_idx != params.num_cmd - 1 && cmd_type != 0x6) {
      u64 src_addr = ((u64)(dst_ptr[17] & 0xff) << 32) | ((u64)dst_ptr[16]);
      u64 dst_addr = ((u64)(dst_ptr[19] & 0xff) << 32) | ((u64)dst_ptr[18]);
      u32 const_fill = dst_ptr[1] & 0x80;
      bool src_in_global =
          (src_addr >= gmem_start_addr()) && !(cmd_type == 0 && const_fill);
      bool dst_in_global = dst_addr >= gmem_start_addr();
      u64 fix_addr;
      if (src_in_global) {
        fix_addr = update_gdma_cmd_addr(params.stage, src_addr, true);
        if (fix_addr != src_addr) {
          dst_ptr[16] = fix_addr & 0xffffffff;
          dst_ptr[17] =
              ((u32)((fix_addr >> 32) & 0xff)) | (dst_ptr[17] & 0xffffff00);
        }
      }
      if (dst_in_global) {
        fix_addr = update_gdma_cmd_addr(params.stage, dst_addr, false);
        if (fix_addr != dst_addr) {
          dst_ptr[18] = fix_addr & 0xffffffff;
          dst_ptr[19] =
              ((u32)((fix_addr >> 32) & 0xff)) | (dst_ptr[19] & 0xffffff00);
        }
      }
      // cmd type: 0:DMA_tensor, 1:DMA_matrix, 2:DMA_masked_select,
      // 3:DMA_general 4:DMA_cw_trans, 5:DMA_nonzero, 6:DMA_sys, 7:DMA_gather,
      // 8:DMA_scatter fix index_tensor or mask_tensor addr
      if (cmd_type == 2 || cmd_type == 7 || cmd_type == 8) {
        u64 index_addr = ((u64)(dst_ptr[21] & 0xff) << 32) | ((u64)dst_ptr[20]);
        if (index_addr >= gmem_start_addr()) {
          fix_addr = update_gdma_cmd_addr(params.stage, index_addr, true);
          if (fix_addr != index_addr) {
            dst_ptr[20] = fix_addr & 0xffffffff;
            dst_ptr[21] =
                ((u32)((fix_addr >> 32) & 0xff)) | (dst_ptr[21] & 0xffffff00);
          }
        }
      }
    }

    offset += cmd_size;
    dst_ptr += (cmd_size >> 2);
    src_ptr += (cmd_size >> 2);
  }
  return true;
}

void Launcher_BM1684X::fill_api_info(const tpu_net_info_t &net_info,
                                     api_info_t &api_info) {
  const std::vector<tpu_tensor_info_t> &input_info = net_info.input_info;
  const std::vector<tpu_tensor_info_t> &output_info = net_info.output_info;
  const std::vector<tpu_cmd_info_t> &cmd_info =
      net_info.core_commands[0].cmd_info;
  const std::vector<cmd_reloc_entry_t> &gdma_reloc_entries =
      net_info.core_commands[0].gdma_reloc_entries;
  const std::vector<u64> &reloc_base_addrs = net_info.reloc_base_addrs;

  u32 api_buffer_size =
      sizeof(int) +
      (input_info.size() * (sizeof(u64) * 2 + sizeof(u32))) + // input
      sizeof(int) +
      (output_info.size() * (sizeof(u64) * 2 + sizeof(u32))) + // output
      sizeof(u64) * 2 + sizeof(int) +
      (cmd_info.size() *
       (sizeof(int) * 2 + sizeof(u32) * 2)); // cmds loc and size
  u32 num_reloc = gdma_reloc_entries.size();
  if (net_info.do_allreduce) {
    api_buffer_size += sizeof(u32) + sizeof(tpu_kernel_allreduce_1684x_t);
  } else {
    api_buffer_size += sizeof(u32); // 0 allreduce
  }
  api_buffer_size += sizeof(u32) + (num_reloc * (2 * sizeof(u64)));

  api_info.api_data.resize(1);
  api_info.api_data[0].assign(api_buffer_size, 0);
  api_info.input_addr_offset.assign(input_info.size(), 0);
  api_info.output_addr_offset.assign(output_info.size(), 0);
  api_info.api_id.emplace_back(net_info.kernel_func_ids[0]);

  void *p_api = api_info.api_data[0].data();
  // input global offset process
  *(int *)p_api = input_info.size();
  p_api = (int *)p_api + 1;
  for (size_t i = 0; i < input_info.size(); ++i) {
    const auto &info = input_info.at(i);
    api_info.input_addr_offset.at(i) =
        (uint8_t *)p_api - (uint8_t *)(api_info.api_data.data());
    *(u64 *)p_api = info.user_global_addr;
    p_api = (u64 *)p_api + 1;
    *(u64 *)p_api = info.compiled_global_addr;
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
    *(u64 *)p_api = info.compiled_global_addr;
    p_api = (u64 *)p_api + 1;
    *(u32 *)p_api = bmrt_data_type_size((bm_data_type_t)info.dtype) *
                    (info.n * info.c * info.h * info.w);
    p_api = (u32 *)p_api + 1;
  }

  // memcpy cmd offset and num
  *(u64 *)p_api = net_info.core_commands[0].bdc_cmd_addr;
  p_api = (u64 *)p_api + 1;
  *(u64 *)p_api = net_info.core_commands[0].gdma_cmd_addr;
  p_api = (u64 *)p_api + 1;
  *(int *)p_api = cmd_info.size();
  p_api = (int *)p_api + 1;
  for (size_t i = 0; i < cmd_info.size(); i++) {
    *(int *)p_api = cmd_info.at(i).bdc_cmd_num;
    p_api = (int *)p_api + 1;
    *(int *)p_api = cmd_info.at(i).gdma_cmd_num;
    p_api = (int *)p_api + 1;
    *(u32 *)p_api = cmd_info.at(i).bdc_cmd_byte_size;
    p_api = (u32 *)p_api + 1;
    *(u32 *)p_api = cmd_info.at(i).gdma_cmd_byte_size;
    p_api = (u32 *)p_api + 1;
  }

  // ====================  extention function ====================
  // if new function is added, please follow the order below
  // =============================================================
  // 1. check if do all reduce
  if (net_info.do_allreduce == 1) {
    *(u32 *)p_api = net_info.do_allreduce;
    p_api = (u32 *)p_api + 1;
    *(tpu_kernel_allreduce_1684x_t *)p_api = net_info.allreduce_param;
    p_api = (tpu_kernel_allreduce_1684x_t *)p_api + 1;
  } else {
    *(u32 *)p_api = 0;
    p_api = (u32 *)p_api + 1;
  }
  // 2. check if do reloc gdma addr
  if (num_reloc > 0) {
    *(u32 *)p_api = num_reloc;
    p_api = (u32 *)p_api + 1;
    for (size_t i = 0; i < num_reloc; i++) {
      const auto &reloc_entry = gdma_reloc_entries[i];
      const auto &reloc_addr_info = reloc_entry.reloc_addr_info;
      *(u64 *)p_api = reloc_entry.cmd_offset;
      p_api = (u64 *)p_api + 1;
      *(u64 *)p_api = reloc_base_addrs[reloc_addr_info.base_addr_id] +
                      reloc_addr_info.addr_offset;
      p_api = (u64 *)p_api + 1;
    }
  } else {
    *(u32 *)p_api = 0;
    p_api = (u32 *)p_api + 1;
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
    for (size_t core_idx = 0; core_idx < 1; core_idx++) {
      BMRT_LOG(DEBUG, "core[%d], tiu_cmd_addr=0x%llx, gdma_cmd_addr=0x%llx",
               core_idx, net_info.core_commands[core_idx].bdc_cmd_addr,
               net_info.core_commands[core_idx].gdma_cmd_addr);
      const auto &cmd_info = net_info.core_commands[core_idx].cmd_info;
      for (u32 g = 0; g < cmd_info.size(); g++) {
        BMRT_LOG(DEBUG,
                 "  --> group[%d], tiu_cmd_num=%d, tiu_cmd_size=%d, "
                 "gdma_cmd_num=%d, gdma_cmd_size=%d",
                 g, cmd_info[g].bdc_cmd_num, cmd_info[g].bdc_cmd_byte_size,
                 cmd_info[g].gdma_cmd_num, cmd_info[g].gdma_cmd_byte_size);
      }
    }
    BMRT_LOG(DEBUG, "coeff_addr=0x%llx, neuron_addr=0x%llx",
             net_info.coeff_start_addr, net_info.neuron_start_addr[0]);
  });
}
bm_status_t Launcher_BM1684X::static_subnet(bm_handle_t handle,
                                            const tpu_net_info_t &net_info) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");

  api_info_t api_info;
  fill_api_info(net_info, api_info);
  auto api_id = net_info.kernel_func_ids[0];
  bm_status_t status = BM_SUCCESS;
  if (api_info.api_data[0].size() < MAX_API_MSG_SIZE) {
    status =
        tpu_kernel_launch_async(handle, api_id, api_info.api_data[0].data(),
                                api_info.api_data[0].size());
  } else {
    bm_device_mem_t api_mem;
#pragma pack(1)
    typedef struct {
      u32 input_num = 0;
      u64 cmd_addr;
      u64 cmd_size;
    } long_cmd_param_t;
#pragma pack()
    u32 malloc_size = api_info.api_data[0].size();
    bm_status_t mem_status =
        bm_malloc_device_byte(handle, &api_mem, malloc_size);
    if (mem_status != BM_SUCCESS) {
      status = (status == BM_SUCCESS) ? mem_status : status;
      BMRT_LOG(WRONG, "bm_malloc_device_byte failed, malloc mem:%d",
               malloc_size);
    }
    long_cmd_param_t new_api;
    auto data = api_info.api_data[0].data();
    bm_status_t s2d_status = bm_memcpy_s2d(handle, api_mem, (void *)data);
    new_api.cmd_addr = api_mem.u.device.device_addr;
    new_api.cmd_size = api_info.api_data[0].size();
    if (BM_SUCCESS != s2d_status) {
      status = (status == BM_SUCCESS) ? s2d_status : status;
      BMRT_LOG(WRONG, "bm_memcpy_s2d failed, ret = %d\n", s2d_status);
    }
    bm_status_t core_status = tpu_kernel_launch_async(
        handle, api_id, (u8 *)(&new_api), sizeof(new_api));
    if (BM_SUCCESS != core_status) {
      status = (status == BM_SUCCESS) ? core_status : status;
      BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d",
               BM_API_ID_MULTI_FULLNET, status);
    }
    bm_free_device(handle, api_mem);
  }
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "tpu_kernel_launch failed, func id:%d, status:%d", api_id,
             status);
  }
  return status;
}

bm_status_t Launcher_BM1684X::dynamic_subnet(
    bm_handle_t handle, const tpu_dynamic_net_info_t &net_info)
{
  const auto &inputs = net_info.inputs;
  const auto &outputs = net_info.outputs;
  const auto *reduce_param = net_info.all_reduce_param;
  const bool get_output_shape = true;
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
  if (reduce_param != NULL) {
    api_buffer_size += sizeof(u32);
    api_buffer_size += sizeof(tpu_kernel_allreduce_1684x_t);
  }

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

  if (reduce_param != NULL) {
    *(u32 *)p_api = 1;
    p_api = (u32 *)p_api + 1;
    *(tpu_kernel_allreduce_1684x_t *)p_api = *reduce_param;
    p_api = (tpu_kernel_allreduce_1684x_t *)p_api + 1;
  }
  BMRT_LOG_RUN(DEBUG, {
    BMRT_LOG(DEBUG, "ir_addr=0x%llx, ir_length=%d[0x%x]",
             net_info.ir_addr, net_info.ir_word_num, net_info.ir_word_num);
    for (u32 i = 0; i < inputs.size(); ++i) {
      auto dims = inputs[i].dim;
      auto dtype = inputs[i].dtype;
      std::string shape_str = std::to_string(inputs[i].shape[0]);
      for (u32 j = 1; j < dims; j++) {
        shape_str += "," + std::to_string(inputs[i].shape[j]);
      }
      BMRT_LOG(DEBUG, "in[%d] addr=0x%llx, shape=[%s], dtype=%s, elem_num=%d",
               i, inputs[i].addr, shape_str.c_str(),
               dtype_to_string((bm_data_type_t)dtype), inputs[i].elem_num);
    }
    // output information
    for (u32 i = 0; i < outputs.size(); ++i) {
      BMRT_LOG(DEBUG, "out[%d] addr=0x%llx", i, outputs[i].addr);
    }
    // output shape info related
    BMRT_LOG(DEBUG, "out_shape_addr=0x%llx", net_info.output_shape_addr);
    BMRT_LOG(DEBUG, "ctx_start=0x%llx, coeff_mem_offset=0x%llx", net_info.ctx_start_addr,
             net_info.coeff_offset_addr);

    for (size_t i = 0; i < ctx_num; ++i) {
      BMRT_LOG(DEBUG, "ctx[%d]: border=0x%llx, offset=0x%llx", i,
               net_info.ctx_mem_borders[i], net_info.ctx_mem_offsets[i]);
    }
  });

  auto func_id = net_info.kernel_func_ids[0];
  bm_status_t status =
      tpu_kernel_launch_async(handle, func_id, api_buffer, api_buffer_size);
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "tpu_kernel_launch failed, func id:%d, status:%d", func_id,
             status);
  } else {
    status = bm_sync_api(handle);
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "bm_sync_api failed, func id:%d, status:%d", func_id,
               status);
    }
  }

  delete[] api_buffer;
  return status;
}

bm_status_t Launcher_BM1684X::_bmdnn_set_profile_enable_(
    bm_handle_t handle, tpu_kernel_function_t func_id, bool enable) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
  u32 api_buffer_size = sizeof(u32);
  u32 profile_enable = enable;
  bm_status_t status = tpu_kernel_launch(handle, func_id, (u8 *)&profile_enable,
                                         api_buffer_size);
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d",
             BM_API_ID_SET_PROFILE_ENABLE, status);
  }
  return status;
}
bm_status_t Launcher_BM1684X::_bmdnn_get_profile_data_(
    bm_handle_t handle, tpu_kernel_function_t func_id,
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

  bm_api_id_t api_code = (bm_api_id_t)BM_API_ID_GET_PROFILE_DATA;
  bm_status_t status = tpu_kernel_launch_async(handle, func_id, (u8 *)&api_data,
                                               api_buffer_size);
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d", api_code,
             status);
  } else {
    status = bm_sync_api(handle);
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "bm_sync_api failed, api id:%d, status:%d", api_code,
               status);
    }
  }
  return status;
}

} // namespace bmruntime