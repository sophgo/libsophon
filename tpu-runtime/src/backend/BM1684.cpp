#include "backend/backend.hpp"
#include "bmruntime.h"

namespace bmruntime {

bool Backend_BM1684::convert_bdc(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);

  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, bdc_cmd_info.size);

    if (cmd_idx == params.num_cmd - 1) {
      dst_ptr[0] |= (1 << 1);
    }

    /* special request: exchange cmd[i] and cmd[31-i] */
    for (uint32_t i = 0; i < bdc_cmd_info.cnt / 2; ++i) {
      uint32_t tmp = dst_ptr[i];
      dst_ptr[i] = dst_ptr[bdc_cmd_info.cnt - 1 - i];
      dst_ptr[bdc_cmd_info.cnt - 1 - i] = tmp;
    }

    dst_ptr += (bdc_cmd_info.size >> 2);
    src_ptr += (bdc_cmd_info.size >> 2);
  }
  return true;
}

bool Backend_BM1684::convert_gdma(ConversionParams &params) {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);
  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, gdma_cmd_info.size);
    if (cmd_idx == params.num_cmd - 1)
      dst_ptr[0] |= (1 << 2);

    uint64_t gdma_direction = (dst_ptr[0] >> 6) & 0x3;
    uint64_t origin_addr = 0, fix_addr = 0;
    if (GDMA_DIR_S2L == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      origin_addr =
          (uint64_t)dst_ptr[16] + (((uint64_t)(dst_ptr[18] & 0xff)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, true);
      if (fix_addr != origin_addr) {
        dst_ptr[16] = fix_addr & 0xffffffff;
        dst_ptr[18] =
            ((uint64_t)((fix_addr >> 32) & 0xff)) | (dst_ptr[18] & 0xffffff00);
      }
    }
    if (GDMA_DIR_L2S == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      origin_addr = (uint64_t)dst_ptr[17] +
                    (((uint64_t)((dst_ptr[18] >> 8) & 0xff)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, false);
      if (fix_addr != origin_addr) {
        dst_ptr[17] = fix_addr & 0xffffffff;
        dst_ptr[18] = ((uint64_t)(((fix_addr >> 32) << 8) & 0xff00)) |
                      (dst_ptr[18] & 0xffff00ff);
      }
    }

    dst_ptr += (gdma_cmd_info.size >> 2);
    src_ptr += (gdma_cmd_info.size >> 2);
  }
  m_instruct_converted = true;
  return true;
}

void Launcher_BM1684::fill_api_info(const tpu_net_info_t &net_info,
                                    api_info_t &api_info) {
  const std::vector<tpu_tensor_info_t> &input_info = net_info.input_info;
  const std::vector<tpu_tensor_info_t> &output_info = net_info.output_info;
  const std::vector<tpu_cmd_info_t> &cmd_info =
      net_info.core_commands[0].cmd_info;

  u32 api_buffer_size =
      sizeof(int) +
      (input_info.size() *
       (sizeof(u64) * 2 + sizeof(int) * 4 + sizeof(unsigned short) +
        sizeof(unsigned char) * 2 + sizeof(int))) + // api buffer size for input
      sizeof(int) +
      (output_info.size() *
       (sizeof(u64) * 2 + sizeof(int) * 2 + sizeof(unsigned short) +
        sizeof(unsigned char) * 2)) + // api buffer size for output
      sizeof(u64) * 2 +
      sizeof(int) * 2 * cmd_info.size() + sizeof(int);

  api_info.api_id.push_back(BM_API_ID_MULTI_FULLNET);
  api_info.api_data.resize(1);
  api_info.api_data[0].assign(api_buffer_size, 0);
  api_info.input_addr_offset.assign(input_info.size(), 0);
  api_info.output_addr_offset.assign(output_info.size(), 0);

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
    *(int *)p_api = info.n;
    p_api = (int *)p_api + 1;
    *(int *)p_api = info.c;
    p_api = (int *)p_api + 1;
    *(int *)p_api = info.h;
    p_api = (int *)p_api + 1;
    *(int *)p_api = info.w;
    p_api = (int *)p_api + 1;
    *(unsigned short *)p_api = info.dtype;
    p_api = (unsigned short *)p_api + 1;
    *(unsigned char *)p_api = info.compiled_stmode;
    p_api = (unsigned char *)p_api + 1;
    *(unsigned char *)p_api = info.user_stmode;
    p_api = (unsigned char *)p_api + 1;
    *(u32 *)p_api = info.padding_h;
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
    *(int *)p_api = info.n;
    p_api = (int *)p_api + 1;
    *(int *)p_api = (info.c * info.h * info.w);
    p_api = (int *)p_api + 1;
    *(unsigned short *)p_api = info.dtype;
    p_api = (unsigned short *)p_api + 1;
    *(unsigned char *)p_api = info.compiled_stmode;
    p_api = (unsigned char *)p_api + 1;
    *(unsigned char *)p_api = info.user_stmode;
    p_api = (unsigned char *)p_api + 1;
  }

  // memcpy cmd offset and num
  *(u64 *)p_api = net_info.core_commands[0].bdc_cmd_addr;
  p_api = (u64 *)p_api + 1;
  *(u64 *)p_api = net_info.core_commands[0].gdma_cmd_addr;
  p_api = (u64 *)p_api + 1;
  *(int *)p_api = cmd_info.size();
  for (size_t i = 0; i < cmd_info.size(); i++) {
    p_api = (int *)p_api + 1;
    *(int *)p_api = cmd_info.at(i).bdc_cmd_num;
    p_api = (int *)p_api + 1;
    *(int *)p_api = cmd_info.at(i).gdma_cmd_num;
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
                 g, cmd_info[g].bdc_cmd_num, cmd_info[g].gdma_cmd_num);
      }
    }
  });
}
bm_status_t Launcher_BM1684::static_subnet(bm_handle_t handle,
                                           const tpu_net_info_t &net_info) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");

  api_info_t api_info;
  fill_api_info(net_info, api_info);
  bm_status_t status =
      bm_send_api(handle, (bm_api_id_t)api_info.api_id[0],
                  api_info.api_data[0].data(), api_info.api_data[0].size());
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d",
             BM_API_ID_MULTI_FULLNET, status);
  }
  return status;
}

/*
 * dynamic fullnet mode
 */
bm_status_t Launcher_BM1684::dynamic_subnet(
    bm_handle_t handle, const tpu_dynamic_net_info_t &net_info)
{
  const auto &inputs = net_info.inputs;
  const auto &outputs = net_info.outputs;
  const bool get_output_shape = true;
  const u32 using_arm_buffer_size = 0;

  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
  BMRT_ASSERT_INFO(net_info.ctx_mem_borders.size() == net_info.ctx_mem_offsets.size(),
                   "ctx borders and offset should have same size");
  bool need_middle_buff_flag = false;
  for (auto& iter : inputs) {
    if (iter.buffer_used) {
      need_middle_buff_flag = true;
      break;
    }
  }

  size_t ctx_num = net_info.ctx_mem_borders.size();
  u32 api_buffer_size =
      sizeof(u64) + sizeof(u32) + // compiled_ir addr, length
                                  // input info
      (sizeof(u32) + sizeof(u32) +
       inputs.size() *
           (sizeof(u64) * (need_middle_buff_flag ? 2 : 1) + sizeof(int) +
            sizeof(int) * BM_MAX_DIMS_NUM + sizeof(int))) +
      // output info
      (sizeof(u32) + outputs.size() * (sizeof(u32) + 2 * sizeof(u64))) +
      // get_output_shape, global_shape_mem_addr, apd_ctx_start, (ctx_num,
      // apd_ctx_mem_borders, apd_ctx_mem_offset),
      sizeof(u32) + sizeof(u64) + sizeof(u64) +
      (sizeof(u32) + sizeof(u64) * ctx_num * 2) +
      // apd_coeff_mem_offset, arm_reserved_addr, arm_reserved_size
      sizeof(u64) + sizeof(u64) + sizeof(u32);

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

  *(u32 *)p_api = (u32)need_middle_buff_flag;
  p_api = (u32 *)p_api + 1;

  for (u32 i = 0; i < inputs.size(); ++i) {
    *(u64 *)p_api = inputs[i].addr;
    p_api = (u64 *)p_api + 1;

    if (need_middle_buff_flag) {
      *(u64 *)p_api = inputs[i].buffer_addr;
      p_api = (u64 *)p_api + 1;
    }

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
    *(u32 *)p_api =  outputs[i].stmode_conveter;
    p_api = (u32 *)p_api + 1;
  }

  for (u32 i = 0; i < outputs.size(); ++i) {
    *(u64 *)p_api = outputs[i].addr;
    p_api = (u64 *)p_api + 1;

    *(u64 *)p_api = outputs[i].buffer_addr;
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

  u64 arm_reserved_addr = -1;
  u32 arm_reserved_size = 0;

  arm_reserved_addr = bm_gmem_arm_reserved_request(handle);
  arm_reserved_size = 0; // 64M
  BMRT_ASSERT_INFO(
      using_arm_buffer_size <= arm_reserved_size,
      "using_arm_buffer_size:%d is larger than arm_reserved_size:%d",
      using_arm_buffer_size, arm_reserved_size);
  *(u64 *)p_api = arm_reserved_addr;
  p_api = (u64 *)p_api + 1;
  *(u32 *)p_api = arm_reserved_size;
  p_api = (u64 *)p_api + 1;

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

  bm_gmem_arm_reserved_release(handle);

  delete[] api_buffer;
  return status;
}

bm_status_t Launcher_BM1684::_bmdnn_set_profile_enable_(bm_handle_t handle,
                                                        bool enable) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
  u32 api_buffer_size = sizeof(u32);
  u32 profile_enable = enable;
  bm_status_t status =
      bm_send_api(handle, (bm_api_id_t)BM_API_ID_SET_PROFILE_ENABLE,
                  (u8 *)&profile_enable, api_buffer_size);
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d",
             BM_API_ID_SET_PROFILE_ENABLE, status);
  }
  return status;
}
bm_status_t Launcher_BM1684::_bmdnn_get_profile_data_(
    bm_handle_t handle, unsigned long long output_global_addr,
    unsigned int output_max_size, unsigned int byte_offset,
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

  api_data.arm_reserved_addr = bm_gmem_arm_reserved_request(handle);
  api_data.output_global_addr = output_global_addr;
  api_data.output_size = output_max_size;
  api_data.byte_offset = byte_offset;
  api_data.data_category = data_category;

  bm_api_id_t api_code = (bm_api_id_t)BM_API_ID_GET_PROFILE_DATA;
  bm_status_t status =
      bm_send_api(handle, api_code, (u8 *)&api_data, api_buffer_size);
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
  bm_gmem_arm_reserved_release(handle);
  return status;
}

} // namespace bmruntime