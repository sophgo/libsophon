#include "bmfunc/bmfunc.h"
#include <iostream>
#include "bmlib_runtime.h"
#include "bmruntime.h"

namespace bmruntime {
static void check_user_addr(std::vector<tpu_tensor_info_t> tensors_info, bool is_input) {
  for (size_t i = 0; i < tensors_info.size()-1; i++) {
    auto tensor_info = tensors_info.at(i);
    auto next_tensor_info = tensors_info.at(i+1);
    u64 user_addr = tensor_info.user_global_addr;
    u64 next_user_addr = next_tensor_info.user_global_addr;
    size_t tensor_size = bmrt_data_type_size((bm_data_type_t)tensor_info.dtype) *
                         (tensor_info.n * tensor_info.c * tensor_info.h * tensor_info.w);
    if (next_user_addr-user_addr != tensor_size) {
      if (is_input) {  // input
        BMRT_LOG(FATAL, "user input address discontinuity, current:input_addr[%d]=%d, failed:input_addr[%d]=%d, data_size:%d", i, user_addr, i+1, next_user_addr, tensor_size);
      } else {  // output
        BMRT_LOG(FATAL, "user output address discontinuity, current:output_addr[%d]=%d, failed:output_addr[%d]=%d, data_size:%d", i, user_addr, i+1, next_user_addr, tensor_size);
      }
    }
  }
}

void bmdnn_func_1688::fill_api_info(const tpu_net_info_t &net_info,
                                    api_info_t &api_info) {
  BMRT_ASSERT_INFO(net_info.neuron_start_addr.size() == 1,
                   "only support one neuron addr");
  const std::vector<tpu_tensor_info_t> &input_info = net_info.input_info;
  const std::vector<tpu_tensor_info_t> &output_info = net_info.output_info;
  // check user address is continuous when io_tag_fuse
  if (net_info.addr_mode == ADDR_MODE_IO_TAG_FUSE) {
    check_user_addr(input_info, true);
    check_user_addr(output_info, false);
  }
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
    api_info.api_id.push_back(net_info.kernel_func_ids[core_idx]);
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
      if (core_idx > 0 && ((info.compiled_global_addr >> 36) & 0x7) == 0) {
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
      if (core_idx > 0 && ((info.compiled_global_addr >> 36) & 0x7) == 0) {
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
      BMRT_LOG(DEBUG, "in[%d] user_addr=0x%llx, cmd_addr=0x%llx, shape=[%d, %d, %d, %d], dtype=%s, byte_size=%d",
          i, info.user_global_addr, info.compiled_global_addr, info.n, info.c, info.h, info.w, dtype_to_string((bm_data_type_t)info.dtype), byte_size);
    }
    for (size_t i = 0; i < output_info.size(); ++i) {
      const auto &info = output_info.at(i);
      auto byte_size = bmrt_data_type_size((bm_data_type_t)info.dtype) *
                      (info.n * info.c * info.h * info.w);
      BMRT_LOG(DEBUG, "out[%d] user_addr=0x%llx, cmd_addr=0x%llx, shape=[%d, %d, %d, %d], dtype=%s, byte_size=%d",
          i, info.user_global_addr, info.compiled_global_addr, info.n, info.c, info.h, info.w, dtype_to_string((bm_data_type_t)info.dtype), byte_size);
    }
    for (size_t core_idx = 0; core_idx < net_info.core_list.size(); core_idx++) {
      BMRT_LOG(DEBUG, "core[%d], tiu_cmd_addr=0x%llx, gdma_cmd_addr=0x%llx", core_idx,
               net_info.core_commands[core_idx].bdc_cmd_addr, net_info.core_commands[core_idx].gdma_cmd_addr);
    }
    BMRT_LOG(DEBUG, "coeff_addr=0x%llx, neuron_addr=0x%llx , base_message_id=%d", net_info.coeff_start_addr, net_info.neuron_start_addr[0], base_message_id);
  });

}
bm_status_t
bmdnn_func_1688::_bmdnn_multi_fullnet_(bm_handle_t handle,
                                       const tpu_net_info_t &net_info) {
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");

  api_info_t api_info;
  size_t core_num = net_info.core_list.size();
  BMRT_ASSERT_INFO(core_num == net_info.kernel_func_ids.size(), "core_num=%d, kernel_func_ids.size()=%d",
                   core_num, net_info.kernel_func_ids.size());
  fill_api_info(net_info, api_info);
  std::vector<tpu_launch_param_t> launch_params(net_info.core_list.size());
  for(size_t core_idx=0; core_idx<net_info.core_list.size(); core_idx++){
    launch_params[core_idx].core_id = net_info.core_list[core_idx];
    launch_params[core_idx].func_id = net_info.kernel_func_ids[core_idx];
    launch_params[core_idx].param_data = api_info.api_data[core_idx].data();
    launch_params[core_idx].param_size = api_info.api_data[core_idx].size();
  }
  bm_status_t status = tpu_kernel_launch_async_multicores(handle, launch_params.data(), launch_params.size());
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "tpu_kernel_launch_async_multicores failed, status:%d", status);
  }

  return status;
}

bm_status_t bmdnn_func_1688::_bmdnn_dynamic_fullnet_(
        bm_handle_t handle,
        const std::vector<int32_t> & func_id_list,
        const unsigned long long compiled_ir_global_addr,
        const unsigned int compiled_ir_length, //unit dword
        const unsigned int input_num,
        const unsigned long long *input_addrs,
        const int * const * input_shapes,
        const int * input_elem_nums,
        const int * input_dtype_and_dims,
        const unsigned int output_num,
        const unsigned long long *output_addrs,
        const unsigned long long apd_ctx_start,
        const std::vector<unsigned long long> apd_ctx_mem_borders,
        const std::vector<unsigned long long> apd_ctx_mem_offset,
        const unsigned long long apd_coeff_mem_offset,
        const unsigned long long apd_io_start,
        const unsigned long long apd_io_mem_offset,
        bool get_output_shape,
        const unsigned long long output_shape_global_addr,
        const std::vector<int32_t>& core_list) {

  BMRT_ASSERT_INFO(core_list.size() == 1, "dynamic compile do not support tensor parallel\n");
  BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
  BMRT_ASSERT_INFO(
      apd_ctx_mem_borders.size() == apd_ctx_mem_offset.size(),
      "ctx borders and offset should have same size");
  BMRT_ASSERT_INFO(
      core_list.size() == func_id_list.size(),
      "core_num=%d, func_list_size=%d",
      core_list.size(), func_id_list.size());

  size_t ctx_num = apd_ctx_mem_borders.size();
  u32 api_buffer_size = sizeof(u64) +sizeof(u32) +  // compiled_ir addr, length
                        // input num
                        sizeof(u32) +
                        //           input_addr    dtype_dims        dim_shape                    elem_num
                        input_num * (sizeof(u64) + sizeof(int) + sizeof(int) * BM_MAX_DIMS_NUM + sizeof(int)) +
                        // output num
                        sizeof(u32) +
                        //           output_addr
                        output_num * sizeof(u64) +
                        //get_output_shape, global_shape_mem_addr, apd_ctx_start, (ctx_num, apd_ctx_mem_borders, apd_ctx_mem_offset),
                        sizeof(u32) + sizeof(u64) + sizeof(u64) + ( sizeof(u32)+sizeof(u64)*ctx_num*2 ) +
                        //apd_coeff_mem_offset
                        sizeof(u64) +
                        // core_idx + core_num + group_msg_id
                        3 * sizeof(u32) +
                        //apd_io_start + apd_io_mem_offset
                        sizeof(u64) + sizeof(u64);

  if (api_buffer_size > MAX_API_MSG_SIZE) {
    //decrease the api buffer size
    for (u32 i = 0; i < input_num; ++i) {
      u32 cur_dim = (u32)(input_dtype_and_dims[i] & 0xFFFF);
      api_buffer_size -= (BM_MAX_DIMS_NUM - cur_dim) * sizeof(int);
    }
  }
  size_t group_msg_id = 0;
  for (size_t i = 0; i < core_list.size(); i++) {
    group_msg_id |= 1<<core_list[i];
  }

  std::vector<std::vector<u8>> api_buffers(core_list.size());
  std::vector<tpu_launch_param_t> launch_params(core_list.size());

  for (size_t core_idx = 0; core_idx < core_list.size(); core_idx++) {
    api_buffers[core_idx].assign(api_buffer_size, 0);
    void* p_api = api_buffers[core_idx].data();
    launch_params[core_idx].core_id = core_list[core_idx];
    launch_params[core_idx].func_id = func_id_list[core_idx];
    launch_params[core_idx].param_data = api_buffers[core_idx].data();
    launch_params[core_idx].param_size = api_buffers[core_idx].size();

    //compiled ir information
    *(u64*)p_api = compiled_ir_global_addr;
    p_api = (u64*)p_api + 1;
    *(u32*)p_api = compiled_ir_length;
    p_api = (u32*)p_api + 1;

    //input information
    *(u32*)p_api = input_num;
    p_api = (u32*)p_api + 1;

    for(u32 i = 0; i < input_num; ++i){
      *(u64*)p_api = input_addrs[i];
      p_api = (u64*)p_api + 1;

      *(u32*)p_api = input_dtype_and_dims[i];
      p_api = (u32*)p_api + 1;
      u32 cur_dim = (u32)(input_dtype_and_dims[i] & 0xFFFF);
      for(u32 j = 0; j < cur_dim; j++){
        *(u32 *)p_api = (u32)input_shapes[i][j];
        p_api = (u32 *)p_api + 1;
      }
      *(u32*)p_api = input_elem_nums[i];
      p_api = (u32*)p_api + 1;
    }
    //output information
    *(u32*)p_api = output_num;
    p_api = (u32*)p_api + 1;

    for(u32 i = 0; i < output_num; ++i){
      *(u64*)p_api = output_addrs[i];
      p_api = (u64*)p_api + 1;
    }
    //output shape info related
    *(u32*)p_api = (u32)get_output_shape;
    p_api = (u32*)p_api + 1;
    *(u64*)p_api = output_shape_global_addr;
    p_api = (u64*)p_api + 1;

    //The memory address in cmd gdma need to be offset when append context,here is the offset value.
    *(u64*)p_api = apd_ctx_start;
    p_api = (u64*)p_api + 1;

    *(u32*)p_api = ctx_num;
    p_api = (u32*)p_api + 1;

    for (size_t i = 0; i < ctx_num; ++i) {
      *(u64*)p_api = apd_ctx_mem_borders[i];
      p_api = (u64*)p_api + 1;
    }

    for (size_t i = 0; i < ctx_num; ++i) {
      *(u64*)p_api = apd_ctx_mem_offset[i];
      p_api = (u64*)p_api + 1;
    }

    *(u64*)p_api = apd_coeff_mem_offset;
    p_api = (u64*)p_api + 1;

    *(u32*)p_api = core_idx;
    p_api = (u32*)p_api + 1;
    *(u32*)p_api = core_list.size();
    p_api = (u32*)p_api + 1;
    *(u32*)p_api = group_msg_id;
    p_api = (u32*)p_api + 1;

    *(u64*)p_api = apd_io_start;
    p_api = (u64*)p_api + 1;
    *(u64*)p_api = apd_io_mem_offset;
    p_api = (u64*)p_api + 1;

  BMRT_LOG_RUN(DEBUG, {
    for (size_t core_idx = 0; core_idx < core_list.size(); core_idx++) {
      BMRT_LOG(DEBUG, "ir_addr=0x%llx, ir_length=%d[0x%x]", compiled_ir_global_addr, compiled_ir_length, compiled_ir_length);
      for(u32 i = 0; i < input_num; ++i){
        auto dims = input_dtype_and_dims[i]&0xFFFF;
        auto dtype = (input_dtype_and_dims[i]>>16)&0xFFFF;
        std::string shape_str = std::to_string(input_shapes[i][0]);
        for(u32 j = 1; j < dims; j++){
          shape_str += "," + std::to_string(input_shapes[i][j]);
        }
        BMRT_LOG(DEBUG, "in[%d] addr=0x%llx, shape=[%s], dtype=%s, elem_num=%d",
            i, input_addrs[i], shape_str.c_str(), dtype_to_string((bm_data_type_t)dtype), input_elem_nums[i]);
      }
      //output information
      for(u32 i = 0; i < output_num; ++i){
        BMRT_LOG(DEBUG, "out[%d] addr=0x%llx", i, output_addrs[i]);
      }
      //output shape info related
      BMRT_LOG(DEBUG, "out_shape_addr=0x%llx", output_shape_global_addr);
      BMRT_LOG(DEBUG, "ctx_start=0x%llx, coeff_mem_offset=0x%llx", apd_ctx_start, apd_coeff_mem_offset);

      *(u32*)p_api = ctx_num;
      p_api = (u32*)p_api + 1;

      for (size_t i = 0; i < ctx_num; ++i) {
        BMRT_LOG(DEBUG, "ctx[%d]: border=0x%llx, offset=0x%llx",i , apd_ctx_mem_borders[i], apd_ctx_mem_offset[i]);
      }
      BMRT_LOG(DEBUG, "core_index=%d, core_num=%d, base_msg_id=%d", core_idx, core_list.size(), group_msg_id);
    }
  });
  }

  bm_status_t status = tpu_kernel_launch_async_multicores(handle, launch_params.data(), launch_params.size());
  if (BM_SUCCESS != status) {
    BMRT_LOG(WRONG, "tpu_kernel_launch_async_multicores failed, status:%d", status);
  }

  return status;
}

bm_status_t  bmdnn_func_1688::_bmdnn_set_profile_enable_(bm_handle_t handle, int core, tpu_kernel_function_t func_id, unsigned int enable_bits){
     BMRT_ASSERT_INFO(handle,"handle shouldn't be NULL\n");
     u32 api_buffer_size = sizeof(u32);
     u32 profile_enable = enable_bits;
     bm_status_t status = tpu_kernel_launch_async_from_core(handle, func_id, (u8*)&profile_enable, api_buffer_size, core);
     if (BM_SUCCESS != status) {
       BMRT_LOG(WRONG, "launch kernel failed: core_id:%d, func id:%d, status:%d", core, func_id, status);
     }
     return status;
}

bm_status_t bmdnn_func_1688::_bmdnn_get_profile_data_(
        bm_handle_t handle,
        int core,
        tpu_kernel_function_t func_id,
        unsigned long long output_global_addr,
        unsigned int output_max_size,
        unsigned int byte_offset,
        unsigned int data_category //0: profile time records, 1: extra data
        ){
  BMRT_ASSERT_INFO(handle, "handle shouldn't be NULL\n");
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

     api_data.arm_reserved_addr = -1;
     api_data.output_global_addr = output_global_addr;
     api_data.output_size = output_max_size;
     api_data.byte_offset = byte_offset;
     api_data.data_category = data_category;

     bm_status_t status = tpu_kernel_launch_async_from_core(handle, func_id, (u8*)&api_data, api_buffer_size, core);
     if (BM_SUCCESS != status) {
       BMRT_LOG(WRONG, "tpu_kernel_launch_async_from_core failed, cor_id:%d, api id:%d, status:%d", core, func_id, status);
     } else {
       status = bm_thread_sync_from_core(handle, core);
       if (BM_SUCCESS != status) {
         BMRT_LOG(WRONG, "bm_sync_api failed, core_id:%d, api id:%d, status:%d", core, func_id, status);
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

bm_status_t bmdnn_func_1688::_bmdnn_set_engine_profile_param_(bm_handle_t handle, int core, tpu_kernel_function_t func_id, int engine_type, unsigned long long addr, unsigned long long size){
  bm_api_engine_profile_param_t param;
  param.engine = engine_type;
  param.addr = addr;
  param.size = size;
  bm_status_t core_status =  tpu_kernel_launch_async_from_core(handle, func_id, (u8*)&param, sizeof(param), core);
  return core_status;
}

}
