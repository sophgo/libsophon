#include "backend/backend.hpp"
#include "bmruntime.h"

#include <utility>

namespace bmruntime {

bool Backend_BM1682::convert_bdc(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);
  for (auto cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, bdc_cmd_info.size);

    if (cmd_idx == params.num_cmd - 1)
      dst_ptr[0] |= (1 << 7); // cmd[0] |= (1 << BD_EOD_BIT);

    dst_ptr[19] += (uint32_t)(params.cmd_addr >> bdc_cmd_info.bits);
    // shift the commands to the tail parts of buffer
    // Using ugly code to fix the strange order of bdc
    for (uint32_t pos = 32 - 1; pos >= (32 - bdc_cmd_info.cnt); pos--)
      dst_ptr[pos] = dst_ptr[pos - 32 + bdc_cmd_info.cnt];

    for (uint32_t pos = 0; pos < 32 - bdc_cmd_info.cnt; pos++)
      dst_ptr[pos] = 0;

    for (uint32_t pos = 8; pos < 32; pos += 2)
      std::swap(dst_ptr[pos], dst_ptr[pos + 1]);

    // update pointer
    dst_ptr += (bdc_cmd_info.size >> 2);
    src_ptr += (bdc_cmd_info.size >> 2);
  }
  return true;
}

bool Backend_BM1682::convert_gdma(ConversionParams &params) const {
  uint32_t *dst_ptr = static_cast<uint32_t *>(params.dst_cmd);
  const uint32_t *src_ptr = static_cast<const uint32_t *>(params.src_cmd);
  for (uint32_t cmd_idx = 0; cmd_idx < params.num_cmd; ++cmd_idx) {
    memcpy(dst_ptr, src_ptr, gdma_cmd_info.size);

    if (cmd_idx == params.num_cmd - 1)
      dst_ptr[0] |= (1 << 2); // cmd[0] |= (1 << GDMA_ACCPI0_EOD_BIT);

    // if (append_mem_offset != 0 || bmrt_arch_info::is_soc_mode()) {
    uint32_t gdma_direction = (dst_ptr[0] >> 6) & 0x3;
    uint64_t origin_addr, fix_addr;
    if (GDMA_DIR_S2L == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      // if origin_addr is below ctx_start, needn't append
      origin_addr = dst_ptr[12] + (((uint64_t)(dst_ptr[13] >> 24)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, true);
      if (fix_addr != origin_addr) {
        dst_ptr[12] = fix_addr & 0xffffffff;
        dst_ptr[13] = ((uint32_t)(((fix_addr >> 32) & 0xff) << 24)) |
                      (dst_ptr[13] & 0x00ffffff);
      }
    }

    if (GDMA_DIR_L2S == gdma_direction || GDMA_DIR_S2S == gdma_direction) {
      origin_addr =
          dst_ptr[11] + (((uint64_t)((dst_ptr[13] >> 16) & 0xff)) << 32);
      fix_addr = update_gdma_cmd_addr(params.stage, origin_addr, false);
      if (fix_addr != origin_addr) {
        dst_ptr[11] = fix_addr & 0xffffffff;
        dst_ptr[13] = ((uint64_t)(((fix_addr >> 32) & 0xff) << 16)) |
                      (dst_ptr[13] & 0xff00ffff);
      }
    }
    dst_ptr[gdma_cmd_info.cnt - 1] += (params.cmd_addr >> gdma_cmd_info.bits);

    // update pointer
    dst_ptr += (gdma_cmd_info.cnt >> 2);
    src_ptr += (gdma_cmd_info.cnt >> 2);
  }
  return true;
}

#define BM_MAX_DIMS_NUM 8
/* code from aicplatform bmdnn */
void Launcher_BM1682::fill_api_info(const tpu_net_info_t &net_info,
                                    api_info_t &api_info) {
  const std::vector<tpu_tensor_info_t> &input_info = net_info.input_info;
  const std::vector<tpu_tensor_info_t> &output_info = net_info.output_info;
  const std::vector<tpu_cmd_info_t> &cmd_info =
      net_info.core_commands[0].cmd_info;

  u32 api_buffer_size =
      sizeof(int) +
      (input_info.size() *
       (sizeof(u64) * 2 + sizeof(int))) + // api buffer size for input
      sizeof(int) +
      (output_info.size() *
       (sizeof(u64) * 2 + sizeof(int))) + // api buffer size for output
      sizeof(u64) * 3 +
      sizeof(int) * 3 * cmd_info.size() + sizeof(int);

  api_info.api_id.push_back(b_enable_profile ? BM_API_ID_MULTI_FULLNET_PROFILE
                                             : BM_API_ID_MULTI_FULLNET);
  api_info.api_data.resize(1);
  api_info.api_data[0].assign(api_buffer_size, 0);
  api_info.input_addr_offset.assign(input_info.size(), 0);
  api_info.output_addr_offset.assign(output_info.size(), 0);
  void *p_api = api_info.api_data[0].data();

  // input global offset process
  *(int *)p_api = (int)input_info.size();
  p_api = (int *)p_api + 1;
  for (size_t i = 0; i < input_info.size(); ++i) {
    api_info.input_addr_offset.at(i) =
        (uint8_t *)p_api - (uint8_t *)(api_info.api_data.data());
    *(u64 *)p_api = input_info.at(i).user_global_addr;
    p_api = (u64 *)p_api + 1;
    *(u64 *)p_api = input_info.at(i).compiled_global_addr;
    p_api = (u64 *)p_api + 1;
    int dtype_size =
        bmrt_data_type_size((bm_data_type_t)input_info.at(i).dtype);
    const int32_t length = (input_info.at(i).n * input_info.at(i).c *
                            input_info.at(i).h * input_info.at(i).w) /
                           dtype_size;
    if (dtype_size == 1) {
      *(int *)p_api = (length + 3) / 4;
    } else if (dtype_size == 2) {
      *(int *)p_api = (length + 1) / 2;
    } else if (dtype_size == 4) {
      *(int *)p_api = length;
    } else {
      BMRT_ASSERT_INFO(0, "Unsupported input data type %d\n",
                       input_info.at(i).dtype);
    }
    p_api = (int *)p_api + 1;
  }

  // output global offset process
  *(int *)p_api = (int)output_info.size();
  p_api = (int *)p_api + 1;
  for (size_t i = 0; i < output_info.size(); ++i) {
    api_info.output_addr_offset.at(i) =
        (uint8_t *)p_api - (uint8_t *)(api_info.api_data.data());
    *(u64 *)p_api = output_info.at(i).user_global_addr;
    p_api = (u64 *)p_api + 1;
    *(u64 *)p_api = output_info.at(i).compiled_global_addr;
    p_api = (u64 *)p_api + 1;
    int dtype_size =
        bmrt_data_type_size((bm_data_type_t)output_info.at(i).dtype);
    const int32_t length = (output_info.at(i).n * output_info.at(i).c *
                            output_info.at(i).h * output_info.at(i).w) /
                           dtype_size;
    if (dtype_size) {
      *(int *)p_api = (length + 3) / 4;
    } else if (dtype_size) {
      *(int *)p_api = (length + 1) / 2;
    } else if (dtype_size) {
      *(int *)p_api = length;
    } else {
      BMRT_ASSERT_INFO(0, "Unsupported input data type %d\n",
                       output_info.at(i).dtype);
    }
    p_api = (int *)p_api + 1;
  }

  // memcpy cmd offset and num
  *(u64 *)p_api = net_info.core_commands[0].bdc_cmd_addr;
  p_api = (u64 *)p_api + 1;
  *(u64 *)p_api = net_info.core_commands[0].gdma_cmd_addr;
  p_api = (u64 *)p_api + 1;
  *(u64 *)p_api = net_info.core_commands[0].cdma_cmd_addr;
  p_api = (u64 *)p_api + 1;
  *(int *)p_api = (int)cmd_info.size();
  for (size_t i = 0; i < cmd_info.size(); i++) {
    p_api = (int *)p_api + 1;
    *(int *)p_api = cmd_info.at(i).bdc_cmd_num;
    p_api = (int *)p_api + 1;
    *(int *)p_api = cmd_info.at(i).gdma_cmd_num;
    p_api = (int *)p_api + 1;
    *(int *)p_api = cmd_info.at(i).cdma_cmd_num;
  }
}
/* multiple fullnet mode
 */
bm_status_t Launcher_BM1682::static_subnet(bm_handle_t handle,
                                           const tpu_net_info_t &net_info) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
  api_info_t api_info;
  fill_api_info(net_info, api_info);

  bm_status_t status =
      bm_send_api(handle, (bm_api_id_t)api_info.api_id[0],
                  api_info.api_data[0].data(), api_info.api_data[0].size());
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "bm_send_api failed, api id:%d, status:%d",
             api_info.api_id[0], status);
  } else {
    status = bm_sync_api(handle);
    if (BM_SUCCESS != status) {
      BMRT_LOG(WRONG, "bm_sync_api failed, api id:%d, status:%d",
               api_info.api_id[0], status);
    }
  }

  return status;
}

/*
 * dynamic fullnet mode
 */
bm_status_t Launcher_BM1682::dynamic_subnet(
    bm_handle_t handle, const tpu_dynamic_net_info_t &net_info)
{
  const auto &inputs = net_info.inputs;
  const auto &outputs = net_info.outputs;
  const bool get_output_shape = true;
  const u32 using_arm_buffer_size = 0;

  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
  u32 api_buffer_size =
      sizeof(u64) + sizeof(u32) + // compiled_ir addr, length
      (sizeof(u32) +
       inputs.size() * (sizeof(u64) + sizeof(int) + sizeof(int) * BM_MAX_DIMS_NUM +
                    sizeof(int))) +              // input info
      (sizeof(u32) + outputs.size() * sizeof(u64)) + // output info
      sizeof(u32) +
      sizeof(u64) + sizeof(u64) + sizeof(u64) +
      sizeof(u64) + // get_output_shape, global_shape_mem_addr, apd_ctx_start,
                    // apd_ctx_mem_offset, apd_coeff_mem_offset,
      sizeof(u64) + sizeof(u32); // arm_reserved_addr, arm_reserved_size

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
    *(u32 *)p_api = inputs[i].elem_num;;
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

  *(u64 *)p_api = net_info.ctx_mem_offsets.empty() ? 0 : net_info.ctx_mem_offsets[0];
  p_api = (u64 *)p_api + 1;

  *(u64 *)p_api = net_info.coeff_offset_addr;
  p_api = (u64 *)p_api + 1;

  u64 arm_reserved_addr = -1;
  u32 arm_reserved_size = 0;

  // if(using_arm_buffer_size>0){
  arm_reserved_addr = bm_gmem_arm_reserved_request(handle);
  arm_reserved_size = 0; // 64M
  //  }
  BMRT_ASSERT_INFO(
      using_arm_buffer_size <= arm_reserved_size,
      "using_arm_buffer_size:%d is larger than arm_reserved_size:%d",
      using_arm_buffer_size, arm_reserved_size);
  *(u64 *)p_api = arm_reserved_addr;
  p_api = (u64 *)p_api + 1;
  *(u32 *)p_api = arm_reserved_size;
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

  // if(using_arm_buffer_size>0){
  bm_gmem_arm_reserved_release(handle);
  // }
  delete[] api_buffer;
  return status;
}

} // namespace bmruntime