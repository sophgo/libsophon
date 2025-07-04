#include "bmruntime.h"
#include <sstream>
#ifdef __linux__
#include <sys/time.h>
#endif
#include "bmruntime_profile.h"
#include "bmruntime_cpp.h"

#ifdef DEBUG
static int shape_count(vector<int>& shape_v)
{
    int count = 1;
    for(auto& shape : shape_v)
        count *= shape;

    return count;
}
#endif

namespace bmruntime {
#ifdef __linux__
static void start_time(struct timeval& time)
{
  gettimeofday(&time, NULL);
}

static long end_time(struct timeval& time)
{
  struct timeval time2;
  gettimeofday(&time2, NULL);
  long elapsed = (time2.tv_sec - time.tv_sec) * 1000000 + time2.tv_usec - time.tv_usec;
  return elapsed;
}
#else
static void start_time(struct timespec& time)
{
  bmrt_clock_gettime(0, &time);
}
static long end_time(struct timespec& time)
{
  struct timespec time2;
  bmrt_clock_gettime(0, &time2);
  long elapsed = (time2.tv_sec - time.tv_sec) * 1000000 + time2.tv_nsec - time.tv_nsec;
  return elapsed;
}
#endif

static void print_subnet_time(const SUBNET_INFO_T * subnet, int idx, long elapsed)
{
  if (subnet->subnet_mode == SUBNET_MODE_CPU) {
    BMRT_LOG(INFO, "subnet[%d]: cpu layer[%d], time %ld us", idx, subnet->cpu_info.op_type, elapsed);
  } else {
    BMRT_LOG(INFO, "subnet[%d]: npu, time %ld us", idx, elapsed);
  }
}


void Bmruntime::subnet_clear(net_ctx_t* net_ctx)
{
  for (auto& stage : net_ctx->stage_v) {
    for (auto& subnet_tensor : stage->subnet_tensor_v) {
      host_mem_t bm_host_mem = subnet_tensor.second.host_mem;
      switch (bm_host_mem.type) {
        case HOST_MEM_MMAP: /* unmap */
#ifndef SOC_MODE
          BMRT_LOG(FATAL, "Only soc mode run here");
#else
          if (m_flags & BM_RUNTIME_SHARE_MEM) {
            bm_mem_unmap_device_mem(m_handles[net_ctx->device_id], bm_host_mem.addr, bm_host_mem.size);
            BMRT_DEBUG("HOSTMEM UNMAP %p SIZE %llx NAME %s", bm_host_mem.addr, bm_host_mem.size,
                        subnet_tensor.first.c_str());
          }
#endif
          break;
        case HOST_MEM_ALLOC: /* free host memory */
          // To be compatible with the bmodel at low version
          if (!stage->cpu_addr) {
            BMRT_DEBUG("HOSTMEM FREE %p SIZE %llx NAME %s", bm_host_mem.addr, bm_host_mem.size,
                       subnet_tensor.first.c_str());
            if (bm_host_mem.addr != NULL) delete[] (float*)(bm_host_mem.addr);
          }

          break;
        default:
          break;
      }
    }
    if (stage->cpu_addr) {
      BMRT_DEBUG("HOSTMEM FREE %p SIZE %d", stage->cpu_addr, stage->cpu_mem_size);
      delete [] (float*)(stage->cpu_addr);
    }
  }

  // multi-core arch
  for (auto &dyn_mem_pair : net_ctx->dyn_neuron_stage_dict) {
    auto dyn_neuron_stage = dyn_mem_pair.second;
    for (auto &subnet_tensor : dyn_neuron_stage->subnet_tensor_v) {
      auto bm_host_mem = subnet_tensor.second.host_mem;
      switch (bm_host_mem.type) {
        case HOST_MEM_MMAP: /* unmap */
#ifndef SOC_MODE
          BMRT_LOG(FATAL, "Only soc mode run here");
#else
          bm_mem_unmap_device_mem(m_handles[net_ctx->device_id], bm_host_mem.addr, bm_host_mem.size);
#endif
          break;
        case HOST_MEM_ALLOC: /* free host memory */
          if (!dyn_neuron_stage->cpu_addr) {
            delete[] (float*)(bm_host_mem.addr);
          }
          break;
        default:
          break;
      }
    }
    if (dyn_neuron_stage->cpu_addr) {
      delete[] (float*)(dyn_neuron_stage->cpu_addr);
    }
  }

  for (auto& stage : net_ctx->stage_v) {
    for (auto& subnet : stage->subnet_v) { /* per subnet */
      if (subnet->subnet_mode == SUBNET_MODE_CPU)
        delete[](char*)(subnet->cpu_info.user_param);
      delete subnet;
    }
  }

  for (auto& stage : net_ctx->stage_v) {
    stage->subnet_v.clear();
  }
}

void Bmruntime::subnet_tensor_s2d(uint32_t devid, map<string, tensor_ext_t> *subnet_tensor_v, const string& tensor_name,
                                  bm_device_mem_t *out_dev_mem, u64 offset, u64 size)
{
    auto iter = subnet_tensor_v->find(tensor_name);
    BMRT_ASSERT_INFO(iter != subnet_tensor_v->end(), \
      "Wrong subnet_tensor_v named:%s",  tensor_name.c_str());
    auto &tensor_ext = iter->second;

    switch (tensor_ext.host_mem.type) {
        case HOST_MEM_MMAP:  /* flush cache */
#ifndef SOC_MODE
            BMRT_LOG(FATAL, "Only soc mode run here");
#else
            bm_mem_flush_partial_device_mem(m_handles[devid], &tensor_ext.tensor_info.device_mem, 0,
                                    (size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) *
                                    bmrt_data_type_size(tensor_ext.tensor_info.dtype));
            if (out_dev_mem != NULL) { /* d2d from internal dev_mem to outside dev_mem */
                // dword copy, divide by 4
                bm_memcpy_d2d(m_handles[devid], *out_dev_mem, offset, tensor_ext.tensor_info.device_mem, 0,
                              (size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) *
                              bmrt_data_type_size(tensor_ext.tensor_info.dtype) / 4);
            }

            BMRT_DEBUG("SUBNET TENSOR FLUSH %s HOST MEM %p, DEVICE MEM %llx, SIZE %lld",
                     tensor_name.c_str(), tensor_ext.host_mem.addr,
                     bm_mem_get_device_addr(tensor_ext.tensor_info.device_mem),
                     (size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) * bmrt_data_type_size(tensor_ext.tensor_info.dtype));
#endif
            break;
        case HOST_MEM_ALLOC: /* cdma s2d */
            bm_device_mem_t dev_mem;
            if (out_dev_mem != NULL) { /* pure cpu tensor, using external device memory */
                dev_mem = *out_dev_mem;
            } else if (tensor_ext.mem_type & MEM_TYPE_TPU) { /* using tensor device memory */
                dev_mem = tensor_ext.tensor_info.device_mem;
            } else {
                BMRT_LOG(FATAL, "Only MEM_TYPE_TPU run here");
            }

            bm_memcpy_s2d_partial_offset(m_handles[devid], dev_mem,
                                         (u8 *)(tensor_ext.host_mem.addr),
                                         (size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) *
                                         bmrt_data_type_size(tensor_ext.tensor_info.dtype),
                                         offset);

            BMRT_DEBUG("SUBNET TENSOR %s S2D FROM %p OFFSET %llx SIZE %llx TO %llx",
                     tensor_name.c_str(), tensor_ext.host_mem.addr, offset,
                     (u64)(size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)),
                     bm_mem_get_device_addr(dev_mem));
            break;
        default:
            BMRT_LOG(FATAL, "error host mem type [%d]",  tensor_ext.host_mem.type);
    }
}

static tensor_ext_t& must_get_tensor_in_subnet(map<string, tensor_ext_t> *subnet_tensor_v, const string& name){
    auto iter = subnet_tensor_v->find(name);
    BMRT_ASSERT_INFO(iter != subnet_tensor_v->end(), \
      "Wrong tensor named:%s",  name.c_str());
    return iter->second;
}
void Bmruntime::subnet_tensor_forward(uint32_t devid, map<string, tensor_ext_t> *subnet_tensor_v, const string& src_name, const string& dst_name, const bm_tensor_t* output_tensors){
    auto &src_tensor = must_get_tensor_in_subnet(subnet_tensor_v, src_name);
    auto &dst_tensor = must_get_tensor_in_subnet(subnet_tensor_v, dst_name);
    auto src_mem = src_tensor.tensor_info.device_mem;
    auto dst_mem = dst_tensor.tensor_info.device_mem;
    if(dst_tensor.io_type == TENSOR_TYPE_NET_OUTPUT){
        dst_mem = output_tensors[dst_tensor.io_index].device_mem;
    }
    if (src_tensor.src_subnet && src_tensor.src_subnet->subnet_mode == SUBNET_MODE_CPU) {
        subnet_tensor_s2d(devid, subnet_tensor_v, src_name, &dst_mem);
    } else {
        //copy or move src_tensor data to dst_tensor
        BMRT_DEBUG("%s D2D from=0x%llx, to=0x%llx, len=%d",
                   src_name.c_str(),
                   (u64)src_mem.u.device.device_addr,
                   (u64)dst_mem.u.device.device_addr,
                   src_mem.size
                   );
        bm_memcpy_d2d_byte(m_handles[devid], dst_mem, 0, src_mem, 0, src_mem.size);
    }
    dst_tensor.tensor_info.shape = src_tensor.tensor_info.shape;
}

void* Bmruntime::subnet_tensor_d2s(uint32_t devid, map<string, tensor_ext_t> *subnet_tensor_v, const string& tensor_name,
                                  bm_device_mem_t *out_dev_mem,
                                  u64 offset, u64 size) // offset is out_dev_mem offset
{
    auto iter = subnet_tensor_v->find(tensor_name);
    BMRT_ASSERT_INFO(iter != subnet_tensor_v->end(), \
      "Wrong subnet_tensor_v named:%s",  tensor_name.c_str());
    auto &tensor_ext = iter->second;

    switch (tensor_ext.host_mem.type) {
        case HOST_MEM_MMAP:  /* invalidate cache */
#ifndef SOC_MODE
            BMRT_LOG(FATAL, "Only soc mode run here");
#else
            if (out_dev_mem) {
                bm_memcpy_d2d(m_handles[devid], tensor_ext.tensor_info.device_mem, 0, *out_dev_mem, offset,
                              (size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) *
                              bmrt_data_type_size(tensor_ext.tensor_info.dtype) / 4);
                BMRT_DEBUG("SUBNET TENSOR D2D FROM %llx TO %llx SIZE %llx",
                         bm_mem_get_device_addr(*out_dev_mem),
                         bm_mem_get_device_addr(tensor_ext.tensor_info.device_mem),
                         (u64)(size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) *
                             bmrt_data_type_size(tensor_ext.tensor_info.dtype) / 4);
            }
            bm_mem_invalidate_partial_device_mem(m_handles[devid], &tensor_ext.tensor_info.device_mem, 0,
                                                 (size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) *
                                                 bmrt_data_type_size(tensor_ext.tensor_info.dtype));
            break;
#endif
        case HOST_MEM_ALLOC: /* free host memory */

            bm_device_mem_t dev_mem;
            if (out_dev_mem != NULL) { /* pure cpu tensor, using external device memory */
                dev_mem = *out_dev_mem;
            } else if (tensor_ext.mem_type & MEM_TYPE_TPU) { /* using tensor device memory */
                dev_mem = tensor_ext.tensor_info.device_mem;
            } else {
                BMRT_LOG(FATAL, "Only MEM_TYPE_TPU run here");
            }

            /* host_mem.size using max shape, here using real type */
            bm_memcpy_d2s_partial_offset(m_handles[devid], tensor_ext.host_mem.addr, dev_mem,
                                         (size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)) *
                                         bmrt_data_type_size(tensor_ext.tensor_info.dtype),
                                         offset);

            BMRT_DEBUG("SUBNET TENSOR %s D2S FROM %llx OFFSET %llx SIZE %llx TO %p",
                     tensor_name.c_str(), bm_mem_get_device_addr(dev_mem), offset,
                     (u64)(size > 0 ? size : bmrt_shape_count(&tensor_ext.tensor_info.shape)),
                     tensor_ext.host_mem.addr);
            break;
        default:
            BMRT_LOG(FATAL, "error host mem type [%d]",  tensor_ext.host_mem.type);
    }
    return (void*)tensor_ext.host_mem.addr;
}

/* TODO : refactor by launch_ir */
bool Bmruntime::launch_tpu_ir_subnet(net_ctx_t* net_ctx, net_stage_t* stage, const SUBNET_INFO_T* subnet,
                                     const bm_tensor_t* input_tensors, const int* input_elem_num, int input_num,
                                     bm_tensor_t* output_tensors, int* output_elem_num, int output_num,
                                     const std::vector<int32_t> &real_core_list, const uint32_t dyn_core_mask)
{
  auto devid = net_ctx->device_id;
  //BMRT_ASSERT(true == net_ctx->is_dynamic);
  auto arch = bmrt_arch_info::get_bmtpu_arch();
  bm_status_t status = BM_SUCCESS;

  for (int idx = 0; idx < input_num; idx++) {
    size_t tensor_size = bmrt_tensor_bytesize(&input_tensors[idx]);
    // the unit of bmlib allocated device memory is 4K bytes
    if (ALIGN(input_tensors[idx].device_mem.size, 4096) < tensor_size) {
      BMRT_LOG(WRONG, "input tensors may not initialized");
      return false;
    }
  }

  #ifdef __linux__
  int *user_input_shapes[input_num];
  int input_dims[input_num];
  u64 user_input_global_addrs[input_num];
  u64 user_output_global_addrs[output_num];
  #else
  std::shared_ptr<int*> user_input_shapes_(new int*[input_num], std::default_delete<int*[]>());
  int** user_input_shapes = user_input_shapes_.get();
  std::shared_ptr<int> input_dims_(new int[input_num], std::default_delete<int[]>());
  int* input_dims = input_dims_.get();
  std::shared_ptr<u64> user_input_global_addrs_(new u64[input_num], std::default_delete<u64[]>());
  u64* user_input_global_addrs = user_input_global_addrs_.get();
  std::shared_ptr<u64> user_output_global_addrs_(new u64[output_num], std::default_delete<u64[]>());
  u64* user_output_global_addrs = user_output_global_addrs_.get();
  #endif

  // prepare inputs info
  for (int idx = 0; idx < input_num; idx++) {
    user_input_global_addrs[idx] =
        bm_mem_get_device_addr(input_tensors[idx].device_mem);
    user_input_shapes[idx] = (int*)input_tensors[idx].shape.dims;
    input_dims[idx] = input_tensors[idx].shape.num_dims;
    auto input_dtype = 0;
    if (arch == BM1684X || arch == BM1688 || arch == BM1690 || arch == SG2380) {
      input_dtype = input_tensors[idx].dtype;
    } else {
      if(input_tensors[idx].dtype == BM_FLOAT32){
          input_dtype = 0; //DSIZE_FP32
      } else if(input_tensors[idx].dtype == BM_INT32){
          input_dtype = 3; //DSIZE_INT32
      } else if(input_tensors[idx].dtype == BM_UINT8){
          input_dtype = 2; //DSIZE_8
      } else if(input_tensors[idx].dtype == BM_INT8){
          input_dtype = 2; //DSIZE_8
      } else if(input_tensors[idx].dtype == BM_UINT16){
          input_dtype = 1; //DSIZE_16
      } else if(input_tensors[idx].dtype == BM_INT16){
          input_dtype = 1; //DSIZE_16
      }
    }
    input_dims[idx] |= (input_dtype<<16);
  }

  // prepare outputs info
  for (int idx = 0; idx < output_num; idx++) {
    //auto& cmd_output  = stage.output_v[idx];
    auto& user_output = output_tensors[idx];
    //user_output.shape = cmd_output.shape;
    //user_output.dtype = net_ctx->output_type_v[idx];
    //if (user_stmode == false) {  // if not set, set to BM_STORE_1N as default
    //  user_output.st_mode = BM_STORE_1N;
    //}
    //if (user_mem == false) {
    //  int ret = bm_malloc_device_byte_(m_handle, &user_output.device_mem,
    //                                                          bmrt_tensor_bytesize(&user_output));
    //  BMRT_ASSERT(ret == 0);
    //}
    user_output_global_addrs[idx] =
        bm_mem_get_device_addr(user_output.device_mem);
  }

  /* input/output store mode change need middle buffer in device memory */
  bool need_middle_buff_flag = false;
  #ifdef __linux__
  u64 user_input_global_addr_middle[input_num];
  u64 user_output_global_addr_middle[output_num];
  u32 output_need_middle_buff_flag[output_num];
  #else
  std::shared_ptr<u64> user_input_global_addr_middle_(new u64[input_num], std::default_delete<u64[]>());
  u64* user_input_global_addr_middle = user_input_global_addr_middle_.get();
  std::shared_ptr<u64> user_output_global_addr_middle_(new u64[output_num], std::default_delete<u64[]>());
  u64* user_output_global_addr_middle = user_output_global_addr_middle_.get();
  std::shared_ptr<u32> output_need_middle_buff_flag_(new u32[output_num], std::default_delete<u32[]>());
  u32* output_need_middle_buff_flag = output_need_middle_buff_flag_.get();
  #endif
  if (arch == BM1684) {
    map<string, tensor_ext_t> *subnet_tensor_v;
    vector<tensor_attr_t> *input_v;
    vector<tensor_attr_t> *output_v;
    if (m_flags & BM_RUNTIME_SHARE_MEM) {
      subnet_tensor_v = &(stage->subnet_tensor_v);
      input_v = &(stage->input_v);
      output_v = &(stage->output_v);
    } else {
      auto dyn_neuron = net_ctx_get_dyn_neuron(net_ctx, dyn_core_mask);
      subnet_tensor_v = &(dyn_neuron->subnet_tensor_v);
      input_v = &(dyn_neuron->input_v);
      output_v = &(dyn_neuron->output_v);
    }

    // input, only 1N will switch to 4N
    int stmode_flag = ST_NO_CHANGE;
    for (int idx = 0; idx < input_num; idx++) {
      auto& tensor_name = subnet->input_tensor_name_v[idx];
      auto& tensor_ext  = subnet_tensor_v->find(tensor_name)->second;
      if (tensor_ext.io_type != TENSOR_TYPE_NET_INPUT) {
        BMRT_DEBUG("subnet immediate tensor %s do not need input middle buffer", tensor_name.c_str());
        user_input_global_addr_middle[idx] = 0;
        continue;
      }

      bm_store_mode_t user_stmode = input_tensors[idx].st_mode;
      bm_store_mode_t stmode = input_v->at(tensor_ext.io_index).st_mode;
      u64 middle_addr = bm_mem_get_device_addr(net_ctx->middlebuff_input[tensor_ext.io_index]);
      if (middle_addr == 0 ||  stmode == user_stmode) {
        user_input_global_addr_middle[idx] = 0;
        stmode_flag = ST_NO_CHANGE;
        input_dims[idx] |= (stmode_flag << 24);
        continue;
      }
      stmode_flag = get_stmode_flag(stmode, user_stmode, true);
      input_dims[idx] |= (stmode_flag << 24);
      user_input_global_addr_middle[idx] = middle_addr;
      need_middle_buff_flag = true;
      BMRT_ASSERT_INFO(stmode != user_stmode, \
        "stmode:%d shouldn't be equal to user_mode:%d",stmode,user_stmode);
    }
    // output
    for (int idx = 0; idx < output_num; idx++) {
      auto& tensor_name = subnet->output_tensor_name_v[idx];
      auto& tensor_ext  = subnet_tensor_v->find(tensor_name)->second;
      /* TODO: net output tensor could be also imm input tensor of subnet.
       *       (1) as net output tensor, need middlebuffer for 1N/4N convert.
       *       (2) as imm tensor, do not need stmode convert..
       */
      //if (tensor_ext.io_type != TENSOR_TYPE_NET_OUTPUT) {
      if (!(tensor_ext.io_type & TENSOR_TYPE_NET_OUTPUT)) {
        /* subnet imm tensor do not need middle buffer */
        BMRT_DEBUG("subnet immediate tensor %s do not need output middle buffer", tensor_name.c_str());
        user_output_global_addr_middle[idx] = 0;
        output_need_middle_buff_flag[idx] = ST_NO_CHANGE;
        continue;
      }

      bm_store_mode_t user_stmode = output_tensors[idx].st_mode;
      bm_store_mode_t stmode = output_v->at(tensor_ext.io_index).st_mode;

      u64 middle_addr = 0;
      // only for bm1684
      if(net_ctx->middlebuff_output.size()>output_num) {
        middle_addr = bm_mem_get_device_addr(net_ctx->middlebuff_output[tensor_ext.io_index]);
      }
      if (stmode == user_stmode || middle_addr == 0) {
        user_output_global_addr_middle[idx] = 0;
        output_need_middle_buff_flag[idx] = ST_NO_CHANGE;
        continue;
      }

      user_output_global_addr_middle[idx] = middle_addr;
      output_need_middle_buff_flag[idx] = get_stmode_flag(stmode, user_stmode, false);
    }
  }

  // for multi-stage ir
  bm_device_mem_t output_shape_mem;

  u64 output_shape_global_addr = 0;
  if (output_num != 0) {
    std::string suffix = (m_flags & BM_RUNTIME_SHARE_MEM) ? "" : "_" + std::to_string(dyn_core_mask);
    uint64_t output_shape_size = output_num * sizeof(bm_shape_ex_t);
    output_shape_global_addr = alloc_device_mem(devid, output_shape_mem, output_shape_size, "dynamic_out"+suffix, 1, false);
    // output_shape_global_addr = must_alloc_device_mem(devid, &output_shape_mem, output_shape_size);
  }

  std::vector<int> core_list;
  // ir only use one core to run
  core_list.push_back(real_core_list[0]);
  // if (core_list.size() > 1) {
  //   // ir only use one core to run
  //   core_list.resize(1);
  // }
  if (arch == BM1682) {
    status = bmfunc::bmdnn_1682()->_bmdnn_dynamic_fullnet_v2_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
        ((subnet->tpu_info.core_commands[0].ir_len/*bytes*/ + 3) / 4), //length unit is dword
        input_num, user_input_global_addrs, user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->ctx_start,
        // There is an assertion in bmruntime_bmodel.cpp to ensure ctx_offset
        // has one or none elements.
        stage->ctx_offset.empty() ? 0 : stage->ctx_offset[0],
        stage->coeff_offset, true,
        output_shape_global_addr,
        0  // no arm reserved buffer used
    );
  } else if (arch == BM1684) {
    status = bmfunc::bmdnn_1684()->_bmdnn_dynamic_fullnet_v2_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
        ((subnet->tpu_info.core_commands[0].ir_len/*bytes*/ + 3) / 4), //length unit is dword
        input_num, user_input_global_addrs, user_input_global_addr_middle, user_input_shapes,
        input_elem_num, input_dims, output_num, user_output_global_addrs, user_output_global_addr_middle,
        stage->ctx_start,
        stage->ctx_borders,
        stage->ctx_offset,
        stage->coeff_offset, need_middle_buff_flag, output_need_middle_buff_flag,
        true, output_shape_global_addr,
        0  // no arm reserved buffer used
    );
  } else if (arch == BM1684X) {
    auto func_ids = net_ctx->kernel_module_->get_dynamic_fullnet_func_id(core_list);
    status = bmfunc::bmdnn_1684x()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], func_ids[0], stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
         ((subnet->tpu_info.core_commands[0].ir_len + 3) / 4), input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->dynamic_ctx_start,
        stage->ctx_borders, stage->dynamic_ctx_offset,
        stage->dynamic_coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        net_ctx->do_allreduce == 1 ? &(net_ctx->allreduce_param) : NULL);
  } else if (arch == BM1688) {
    std::vector<u64> dyn_offset;
    if (m_flags & BM_RUNTIME_SHARE_MEM) {
      dyn_offset = stage->dynamic_ctx_offset;
    } else {
      auto dyn_neuron = net_ctx_get_dyn_neuron(net_ctx, dyn_core_mask);
      dyn_offset = dyn_neuron->dynamic_ctx_offset;
    }
    auto func_ids = net_ctx->kernel_module_->get_dynamic_fullnet_func_id(core_list);
    status = bmfunc::bmdnn_1688()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], func_ids, stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
         ((subnet->tpu_info.core_commands[0].ir_len + 3) / 4), input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->dynamic_ctx_start,
        stage->ctx_borders, dyn_offset,
        stage->dynamic_coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else if (arch == BM1690) {
    std::vector<u64> dyn_offset;
    if (m_flags & BM_RUNTIME_SHARE_MEM) {
      dyn_offset = stage->dynamic_ctx_offset;
    } else {
      auto dyn_neuron = net_ctx_get_dyn_neuron(net_ctx, dyn_core_mask);
      dyn_offset = dyn_neuron->dynamic_ctx_offset;
    }
    status = bmfunc::bmdnn_2260()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
         ((subnet->tpu_info.core_commands[0].ir_len + 3) / 4), input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->dynamic_ctx_start,
        stage->ctx_borders, dyn_offset,
        stage->dynamic_coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else if (arch == MARS3) {
    status = bmfunc::bmdnn_mars3()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
         ((subnet->tpu_info.core_commands[0].ir_len + 3) / 4), input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->dynamic_ctx_start,
        stage->ctx_borders, stage->dynamic_ctx_offset,
        stage->dynamic_coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else if (arch == SGTPUV8) {
    status = bmfunc::bmdnn_sgtpuv8()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
         ((subnet->tpu_info.core_commands[0].ir_len + 3) / 4), input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->dynamic_ctx_start,
        stage->ctx_borders, stage->dynamic_ctx_offset,
        stage->dynamic_coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else if (arch == SG2380) {
    std::vector<u64> dyn_offset;
    if (m_flags & BM_RUNTIME_SHARE_MEM) {
      dyn_offset = stage->dynamic_ctx_offset;
    } else {
      auto dyn_neuron = net_ctx_get_dyn_neuron(net_ctx, dyn_core_mask);
      dyn_offset = dyn_neuron->dynamic_ctx_offset;
    }
    status = bmfunc::bmdnn_2380()->_bmdnn_dynamic_fullnet_(
        m_handles[devid], stage->core_commands[0].ir_mem.addr + subnet->tpu_info.core_commands[0].ir_offset,
         ((subnet->tpu_info.core_commands[0].ir_len + 3) / 4), input_num, user_input_global_addrs,
        user_input_shapes, input_elem_num, input_dims, output_num,
        user_output_global_addrs, stage->dynamic_ctx_start,
        stage->ctx_borders, dyn_offset,
        stage->dynamic_coeff_offset, stage->io_start, stage->io_offset, true,
        output_shape_global_addr,
        core_list);
  } else {
    BMRT_LOG(FATAL, "Error: unknown BM TPU");
  }

  if (status == BM_SUCCESS) {
    sync_cores(m_handles[devid], core_list);
  }

  if (output_num == 0) {
    return BM_SUCCESS == status;
  }

  // update output shape when output_num larger than 0
  if (BM_SUCCESS == status) {
    #ifdef __linux__
    bm_shape_ex_t output_shape_ex_v[output_num];
    #else
    std::shared_ptr<bm_shape_ex_t> output_shape_ex_v_(new bm_shape_ex_t[output_num], std::default_delete<bm_shape_ex_t[]>());
    bm_shape_ex_t* output_shape_ex_v = output_shape_ex_v_.get();
    #endif
    if (output_num != 0) {
      status = bm_memcpy_d2s(m_handles[devid], output_shape_ex_v, output_shape_mem);
    }
    else {
      status = BM_SUCCESS;
    }
    CHECK_status(status);
    for (int idx = 0; idx < output_num; idx++) {
      output_tensors[idx].shape = output_shape_ex_v[idx].shape;
      output_elem_num[idx] = output_shape_ex_v[idx].elem_num;
    }
  }

  if (alloc_mem) {
    free_device_mem(devid, output_shape_mem);
  }

  return BM_SUCCESS == status;
}

template <typename T_stage>
void Bmruntime::get_tensor_attr(const T_stage *stage,
    const vector<string> &tensor_name_v, vector<tensor_attr_t> &input_v) {
  for (int idx = 0; idx < tensor_name_v.size(); ++idx) {
    const auto &tensor_name = tensor_name_v.at(idx);
    const tensor_ext_t &cmd_input = stage->subnet_tensor_v.find(tensor_name)->second;
    input_v[idx].dev_mem = cmd_input.tensor_info.device_mem;
    input_v[idx].pad_h = cmd_input.pad_h;
    input_v[idx].shape = cmd_input.tensor_info.shape;
    input_v[idx].st_mode = cmd_input.tensor_info.st_mode;
  }
}

void Bmruntime::fill_tpu_tensor_info(
    std::vector<tpu_tensor_info_t> &tensor_info, const vector<tensor_attr_t> &tensor_v,
    const bm_tensor_t *user_tensors, bool is_input) {
  tensor_info.clear();
  for (u32 idx = 0; idx < tensor_v.size(); idx++) {
    tpu_tensor_info_t info = {0};
    /// info that is given by users
    auto &user_input = user_tensors[idx];
    info.dtype = user_input.dtype;
    info.user_stmode = user_input.st_mode;
    info.user_global_addr = bm_mem_get_device_addr(user_input.device_mem) +
                            GLOBAL_MEM_CMD_START_OFFSET;
    BMRT_ASSERT_INFO(
        info.user_stmode == BM_STORE_1N || info.user_stmode == BM_STORE_4N,
        "user stmode[%d]:%d shouldn't be BM_STORE_2N\n", idx, info.user_stmode);

    /// info that fixed when compiling
    auto &cmd_input = tensor_v[idx];
    info.compiled_stmode = cmd_input.st_mode;
    info.padding_h = cmd_input.pad_h;
    info.compiled_global_addr =
        bm_mem_get_device_addr(cmd_input.dev_mem) + GLOBAL_MEM_CMD_START_OFFSET;

    BMRT_ASSERT_INFO(info.compiled_stmode == BM_STORE_1N ||
                         info.compiled_stmode == BM_STORE_4N,
                     "user stmode[%d]:%d shouldn't be BM_STORE_2N\n", idx,
                     info.compiled_stmode);
    const auto &ref_shape = is_input ? user_input.shape : cmd_input.shape;
    info.n = ref_shape.dims[0];
    info.c = ref_shape.num_dims > 1 ? ref_shape.dims[1] : 1;
    info.h = ref_shape.num_dims > 2 ? ref_shape.dims[2] : 1;
    info.w = 1;
    for (int s = 3; s < ref_shape.num_dims; s++) {
      info.w *= ref_shape.dims[s];
    }
    info.tensor_byte_size = (uint32_t)info.n * info.c * info.h * info.w *
                            bmrt_data_type_size((bm_data_type_t)info.dtype);
    tensor_info.push_back(std::move(info));
  }
}

template <typename T_stage>
void Bmruntime::fill_tpu_tensor_info(
    std::vector<tpu_tensor_info_t> &tensor_info, const T_stage *stage,
    const SUBNET_INFO_T *subnet, const bm_tensor_t *user_tensors,
    bool is_input) {
  const auto &ref_tensors =
      is_input ? subnet->input_tensor_name_v : subnet->output_tensor_name_v;
  std::vector<tensor_attr_t> tensor_attr_v(ref_tensors.size());
  get_tensor_attr(stage, ref_tensors, tensor_attr_v);
  return fill_tpu_tensor_info(tensor_info, tensor_attr_v, user_tensors, is_input);
}

void Bmruntime::fill_tpu_cmd_info(std::vector<tpu_cmd_info_t> &cmd_info,
                                  const std::vector<single_core_command_t> &core_commands,
                                  const int32_t core_idx) {
  cmd_info.clear();
  const size_t group_num = core_commands[core_idx].bdc_id.size();
  for (size_t group_idx = 0; group_idx < group_num; group_idx++) {
    tpu_cmd_info_t info = {0};
    info.bdc_cmd_num = core_commands[core_idx].bdc_id[group_idx];
    info.gdma_cmd_num = core_commands[core_idx].gdma_id[group_idx];
    info.bdc_cmd_byte_size = core_commands[core_idx].bdc_cmd_byte[group_idx];
    info.gdma_cmd_byte_size = core_commands[core_idx].gdma_cmd_byte[group_idx];
    cmd_info.push_back(std::move(info));
  }
}
/* TODO : refactor by launch_static */
bool Bmruntime::launch_tpu_subnet(net_ctx_t* net_ctx, net_stage_t* stage, const SUBNET_INFO_T* subnet,
                                  const bm_tensor_t* input_tensors, int input_num,
                                  bm_tensor_t* output_tensors, int output_num,
                                  const std::vector<int32_t> &core_list, const uint32_t dyn_core_mask, bool force_sync)
{
  auto devid = net_ctx->device_id;
  std::vector<tpu_tensor_info_t> input_info;
  std::vector<tpu_tensor_info_t> output_info;
  std::shared_ptr<dyn_neuron_stage_t> dyn_neuron = nullptr;
  if (m_flags & BM_RUNTIME_SHARE_MEM) {
    fill_tpu_tensor_info(input_info, stage, subnet, input_tensors, true);
    fill_tpu_tensor_info(output_info, stage, subnet, output_tensors, false);
  } else {
    dyn_neuron = net_ctx_get_dyn_neuron(net_ctx, dyn_core_mask);
    fill_tpu_tensor_info(input_info, dyn_neuron.get(), subnet, input_tensors, true);
    fill_tpu_tensor_info(output_info, dyn_neuron.get(), subnet, output_tensors, false);
  }

  // auto core_list = get_core_list_from_core_mask(dyn_core_mask);
  std::vector<tpu_single_core_cmd_t> core_command(core_list.size());
  for (size_t core_idx = 0; core_idx < core_list.size(); core_idx++) {
    std::vector<tpu_cmd_info_t> cmd_info;
    fill_tpu_cmd_info(cmd_info, subnet->tpu_info.core_commands, core_idx);
    core_command[core_idx].cmd_info = std::move(cmd_info);
    core_command[core_idx].bdc_cmd_addr =
        stage->core_commands[core_idx].bdc_mem.addr +
        subnet->tpu_info.core_commands[core_idx].bdc_offset;
    core_command[core_idx].gdma_cmd_addr =
        stage->core_commands[core_idx].gdma_mem.addr +
        subnet->tpu_info.core_commands[core_idx].gdma_offset;
    core_command[core_idx].cdma_cmd_addr = 0;
    core_command[core_idx].hau_cmd_addr =
        stage->core_commands[core_idx].hau_mem.addr;
    core_command[core_idx].sdma_cmd_addr =
        stage->core_commands[core_idx].sdma_mem.addr;
  }

  tpu_net_info_t net_info;
  net_info.input_info = std::move(input_info);
  net_info.output_info = std::move(output_info);
  net_info.core_commands = std::move(core_command);
  net_info.core_list = core_list;
  net_info.coeff_start_addr = stage->coeff_offset;
  net_info.do_allreduce = net_ctx->do_allreduce;
  if (m_flags & BM_RUNTIME_SHARE_MEM) {
    net_info.neuron_start_addr.assign(stage->ctx_offset.begin(), stage->ctx_offset.end());
  } else {
    net_info.neuron_start_addr.assign(dyn_neuron->ctx_offset.begin(), dyn_neuron->ctx_offset.end());
  }
  if (bmrt_arch_info::get_bmtpu_arch() == BM1684X || bmrt_arch_info::get_bmtpu_arch() == BM1688 || bmrt_arch_info::get_bmtpu_arch() == MARS3 || bmrt_arch_info::get_bmtpu_arch() == SGTPUV8) {
    net_info.kernel_func_ids = net_ctx->kernel_module_->get_multi_fullnet_func_id(core_list);
  }

#ifdef DEBUG
    BMRT_DEBUG("TPU SUBNET LAUNCHED START");
    {
      for (int i = 0; i < input_num; i++) {
        const auto &info = net_info.input_info.at(i);
        int32_t data_len = info.n * info.c * info.h * info.w;
        BMRT_DEBUG("TPU SUBNET input %d : Device Address  %lx SIZE %x", i,
                 info.user_global_addr, data_len);
        if (input_tensors[i].dtype != BM_FLOAT32 || input_tensors[i].device_mem.size == 0) {
          continue;
        }
        float* input_value = new float[std::min(data_len, 10)];
        bm_memcpy_d2s_partial(m_handles[devid], input_value, input_tensors[i].device_mem,
                              std::min(data_len, 10) * sizeof(float));
        std::stringstream debug_msg;
        for (int idx = 0; idx < std::min(10, data_len); idx++)
          debug_msg << "  " << input_value[idx];
        delete[] input_value;
        BMRT_DEBUG("%s", debug_msg.str().c_str());
      }
    }
#endif

    bm_status_t status = bmfunc::bmdnn_base()->_bmdnn_multi_fullnet_(m_handles[devid], net_info);
    bool need_sync = m_profile->is_enabled() | force_sync;
    if (need_sync && BM_SUCCESS == status) {
      for (auto core_id : core_list) {
        bm_status_t core_status = bm_thread_sync_from_core(m_handles[devid], core_id);
        status = core_status == BM_SUCCESS ? status : core_status;
      }
    }
    if (m_profile->is_enabled()) {
      for(size_t core_idx = 0; core_idx<core_list.size(); core_idx++){
        auto cmd_num = m_profile->record_subnet_cmd_info(core_idx,
            stage->core_commands[core_idx].gdma_mem.addr, subnet->tpu_info.core_commands[core_idx].gdma_offset,
            stage->core_commands[core_idx].bdc_mem.addr, subnet->tpu_info.core_commands[core_idx].bdc_offset,
            subnet->tpu_info.core_commands[core_idx].gdma_id.size());
        for (size_t i = 0; i < subnet->tpu_info.core_commands[core_idx].gdma_id.size(); i++) {
          cmd_num[i].bdc = net_info.core_commands[core_idx].cmd_info[i].bdc_cmd_num;
          cmd_num[i].gdma = net_info.core_commands[core_idx].cmd_info[i].gdma_cmd_num;
        }
      }
    }
    if (BM_SUCCESS != status) {
      trace();
    }

#ifdef DEBUG
    if (BM_SUCCESS == status) {
      for (int i = 0; i < output_num; i++) {
        const auto &info = net_info.output_info.at(i);
        int32_t data_len = info.n * info.c * info.h * info.w;
        int really_output_size =
            bm_mem_get_device_size(output_tensors[i].device_mem) / sizeof(float);
        BMRT_DEBUG("TPU SUBNET output %d : Device Address %lx STATIC SIZE %x REALLY SIZE %x",
                 i, info.user_global_addr, data_len, really_output_size);
        if (output_tensors[i].dtype != BM_FLOAT32 || output_tensors[i].device_mem.size == 0) {
          continue;
        }
        float* output_value = new float[10];
        bm_memcpy_d2s_partial(m_handles[devid], output_value, output_tensors[i].device_mem,
                              std::min((int)really_output_size, 10) * sizeof(float));
        std::stringstream debug_msg;
        for (int idx = 0; idx < std::min(data_len, 10); idx++)
          debug_msg << "  " << output_value[idx];
        delete[] output_value;
        BMRT_DEBUG("%s", debug_msg.str().c_str());
      }
    }
    BMRT_DEBUG("TPU SUBNET LAUNCHED END");
#endif

    return status == BM_SUCCESS;
}

/* launch static net, no n/h/w specified */
bool Bmruntime::launch_cpu_subnet(net_ctx_t* net_ctx, map<string, tensor_ext_t> *subnet_tensor_v, const SUBNET_INFO_T* subnet,
                                  const bm_tensor_t* input_tensors, bm_shape_t real_out_shape[])
{
  if (bmcpu_process_ == NULL) {
      BMRT_LOG(WRONG, "cpu.so load failed, can't run cpu layer");
      return false;
  }
  auto devid = net_ctx->device_id;
  int op_type = subnet->cpu_info.op_type;
  void *user_param = subnet->cpu_info.user_param;
  int param_size = subnet->cpu_info.param_size;

  int input_num  = subnet->input_tensor_name_v.size();
  int output_num = subnet->output_tensor_name_v.size();

  vector<float *> input_tensor_data_v(input_num);
  vector<float *> output_tensor_data_v(output_num);
  vector<vector<int>> input_shapes_v;
  vector<vector<int>> output_shapes_v;

  bm_device_mem_t dev_mem;
  bm_shape_t shape;
  std::vector<bm_data_type_t> input_dtypes;
  std::vector<bm_data_type_t> output_dtypes;
  int tensor_idx = 0;
  for (auto& tensor_name: subnet->input_tensor_name_v) {
    bool need_d2s = false;

    auto iter = subnet_tensor_v->find(tensor_name);
    BMRT_ASSERT_INFO(iter != subnet_tensor_v->end(), "Wrong subnet_tensor_v named:%s",  tensor_name.c_str());
    // BMRT_ASSERT(stage->subnet_tensor_v.find(tensor_name) != stage->subnet_tensor_v.end());
    auto& tensor_ext = subnet_tensor_v->find(tensor_name)->second;

    switch(tensor_ext.io_type) {
        case TENSOR_TYPE_NET_INPUT:
            /* subnet input tensor is also net input tensor, using user input */
            shape = input_tensors[tensor_ext.io_index].shape;
            input_dtypes.push_back(input_tensors[tensor_ext.io_index].dtype);
            input_shapes_v.push_back(vector<int>(shape.dims, shape.dims + shape.num_dims));

            /* now net inputs are always device memory, TOBE REFINE !! */
            dev_mem = input_tensors[tensor_ext.io_index].device_mem;
            need_d2s = true;
            subnet_tensor_d2s(devid, subnet_tensor_v, tensor_name, &dev_mem, 0, bmrt_shape_count(&shape));
            break;
        case TENSOR_TYPE_NET_OUTPUT:
        case TENSOR_TYPE_IMM_IO:
            shape = tensor_ext.tensor_info.shape;
            input_dtypes.push_back(tensor_ext.tensor_info.dtype);
            input_shapes_v.push_back(vector<int>(shape.dims, shape.dims + shape.num_dims));

            // tensor_ext.src_subnet is nullptr, means the tensor is coeff
            if ((!tensor_ext.src_subnet || tensor_ext.src_subnet->subnet_mode != SUBNET_MODE_CPU) &&
                tensor_ext.mem_type != MEM_TYPE_CPU) {
                /* for cpu subnet, if input tensor is from tpu subnet : d2s */
                dev_mem = tensor_ext.tensor_info.device_mem;
                need_d2s = true;
                subnet_tensor_d2s(devid, subnet_tensor_v, tensor_name);
            }

            break;
        default:
            BMRT_LOG(FATAL, "error io tensor type!");
    }

    input_tensor_data_v[tensor_idx] = (float *)tensor_ext.host_mem.addr;

    if (need_d2s) {
      BMRT_DEBUG("CPU SUBNET TENSOR %s D2S FROM %llx TO %p", tensor_name.c_str(),
               bm_mem_get_device_addr(dev_mem), input_tensor_data_v[tensor_idx]);
    }

    tensor_idx++;
  }

  tensor_idx = 0;
  for (auto& tensor_name: subnet->output_tensor_name_v) {
    auto& tensor_ext = subnet_tensor_v->find(tensor_name)->second;
    auto& shape = tensor_ext.tensor_info.shape;
    output_dtypes.push_back(tensor_ext.tensor_info.dtype);
    vector<int> output_shape(shape.dims, shape.dims + shape.num_dims);
    output_shapes_v.push_back(output_shape);

    output_tensor_data_v[tensor_idx] = (float *)tensor_ext.host_mem.addr;
    tensor_idx++;
  }

  BMRT_DEBUG("CPU SUBNET TYPE %d LAUNCH START", op_type);
#ifdef DEBUG
  {
    for (int i = 0; i < input_num; i++) {
      BMRT_DEBUG("CPU SUBNET input %d :", i);
      std::stringstream debug_msg;
      auto data_fp32 = reinterpret_cast<float *>(input_tensor_data_v[i]);
      auto data_int = reinterpret_cast<int *>(input_tensor_data_v[i]);
      for (int idx = 0; idx < std::min(10, shape_count(input_shapes_v[i])); idx++) {
          if (input_dtypes[i] == bm_data_type_t::BM_INT32)
            debug_msg <<"  "<< data_int[idx];
          else
          // if (input_dtypes[i] == bm_data_type_t::BM_FLOAT32)
              debug_msg <<"  "<< data_fp32[idx];
      }
      BMRT_DEBUG("%s", debug_msg.str().c_str());
    }
  }
#endif
  (void)(input_dtypes);
  (void)(output_dtypes);

  /* NOTE: need keep input/output tensor order accordingly with bmcpu */
  if (op_type < 10000) {
    bmcpu_process_(bmcpu_handle_, op_type,
                   user_param, param_size,
                   input_tensor_data_v, input_shapes_v,
                   output_tensor_data_v, output_shapes_v);
  } else if (customcpu_handle_) {
    BMRT_LOG(INFO, "customcpu_process_");
    customcpu_process_(customcpu_handle_, op_type,
                       user_param, param_size,
                       input_tensor_data_v, input_shapes_v,
                       output_tensor_data_v, output_shapes_v);
  } else {
    BMRT_LOG(WRONG, "Cpu process function not found.");
  }

#ifdef DEBUG
  {
    for (int i = 0; i < output_num; i++) {
      BMRT_DEBUG("CPU SUBNET output %d :", i);
      std::stringstream debug_msg;
      auto data_fp32 = reinterpret_cast<float *>(output_tensor_data_v[i]);
      auto data_int = reinterpret_cast<int *>(output_tensor_data_v[i]);
      for (int idx = 0; idx < std::min(10, shape_count(output_shapes_v[i])); idx++) {
          if (input_dtypes[i] == bm_data_type_t::BM_INT32)
            debug_msg <<"  "<< data_int[idx];
          else
          // if (input_dtypes[i] == bm_data_type_t::BM_FLOAT32)
              debug_msg <<"  "<< data_fp32[idx];
      }
      BMRT_DEBUG("%s", debug_msg.str().c_str());
    }
  }
#endif
  BMRT_DEBUG("CPU SUBNET TYPE %d LAUNCH END", op_type);

  /* reshape output tensors */
  for (u32 tensor_idx = 0; tensor_idx < output_shapes_v.size(); ++tensor_idx) {
      int num_dims = output_shapes_v[tensor_idx].size();
      real_out_shape[tensor_idx].num_dims = num_dims;
      for (int i = 0; i < num_dims; i++)
          real_out_shape[tensor_idx].dims[i] = output_shapes_v[tensor_idx][i];
  }

  return true;
}

bool Bmruntime::launch_multi_subnet(
    net_ctx_t* net_ctx, net_stage_t* stage,
    const bm_tensor_t* input_tensors,
    int input_num,
    bm_tensor_t* output_tensors,
    int output_num,
    const std::vector<int32_t> &core_list,
    const uint32_t dyn_core_mask)
{
    int tensor_idx = 0;
    #ifdef __linux__
    struct timeval time;
    #else
    struct timespec time;
    #endif
    bool ret = true;
    auto devid = net_ctx->device_id;
    const SUBNET_INFO_T *subnet = stage->subnet_v.front();
    map<string, tensor_ext_t> *subnet_tensor_v;

    if (m_flags & BM_RUNTIME_SHARE_MEM) {
      subnet_tensor_v = &(stage->subnet_tensor_v);
    } else {
      auto dyn_neuron = net_ctx_get_dyn_neuron(net_ctx, dyn_core_mask);
      subnet_tensor_v = &(dyn_neuron->subnet_tensor_v);
    }
    int iteration = 0;
    map<string, int> tensor_iteration;
    while(subnet){
        BMRT_DEBUG("iteration=%d, subnet_id=%d, subnet_mode=%d", iteration, subnet->id, subnet->subnet_mode);
        //for merge, use tensor with max iteration as input
        iteration++;
        int next_id = -1;
        if(subnet->subnet_mode != SUBNET_MODE_SWITCH){
            for(auto out_name: subnet->output_tensor_name_v){
                tensor_iteration[out_name] = iteration;
            }
            if(!subnet->next_subnet_ids.empty()){
                next_id = subnet->next_subnet_ids[0];
            }
        }
        if (m_subnet_time_print) {
            start_time(time);
        }
        m_profile->begin_subnet(net_ctx, iteration-1, subnet->id, subnet->subnet_mode);
        if(subnet->subnet_mode == SUBNET_MODE_MERGE){
            output_num = subnet->output_tensor_name_v.size();
            auto& output_from = subnet->merge_info.output_from;
            for(int idx = 0; idx<output_num; idx++){
                auto& indice = output_from[idx];
                int in_idx = -1;
                int max_iteration = -1;
                //select a lastest input as output
                for(auto from_index: indice){
                    auto in_name = subnet->input_tensor_name_v[from_index];
                    if(max_iteration<tensor_iteration[in_name]){
                        max_iteration = tensor_iteration[in_name];
                        in_idx = from_index;
                    }
                }
                BMRT_ASSERT_INFO(in_idx>=0, "in_idx:%d shouldn't less than 0", in_idx);
                //forward tensor to output
                auto in_name = subnet->input_tensor_name_v[in_idx];
                subnet_tensor_forward(devid, subnet_tensor_v, in_name, subnet->output_tensor_name_v[idx], output_tensors);
            }
        } else if (subnet->subnet_mode == SUBNET_MODE_SWITCH) {
            int subnet_id_size = subnet->next_subnet_ids.size();
            BMRT_ASSERT_INFO(subnet_id_size == 2, "subnet->next_subnet_ids.size():%d should be 2",subnet_id_size);
            int tensor_name_size = subnet->input_tensor_name_v.size();
            BMRT_ASSERT_INFO(tensor_name_size >= 1, "subnet->input_tensor_name_v.size():%d shouldn't less than 1", tensor_name_size);

            //get condition data
            bool run_true_subnet = false;
            auto cond_name = subnet->input_tensor_name_v.back();
            const bm_device_mem_t* cond_mem = nullptr;
            auto& tensor_ext = subnet_tensor_v->find(cond_name)->second;
            if(tensor_ext.io_type == TENSOR_TYPE_NET_INPUT){
                cond_mem = &(input_tensors[tensor_ext.io_index].device_mem);
            }
            auto data_ptr = (int*)subnet_tensor_d2s(devid, subnet_tensor_v, cond_name, const_cast<bm_device_mem_t*>(cond_mem));
            run_true_subnet = data_ptr[0] != 0;

            if(!subnet->switch_info.valid){
                for(auto out_name: subnet->output_tensor_name_v){
                    bool is_true_name = out_name.substr(out_name.size()-2) == ":1";
                    if((run_true_subnet && is_true_name) || (!run_true_subnet && !is_true_name)){
                        tensor_iteration[out_name] = iteration;
                    }
                }
            } else {
                for(size_t out_idx=0; out_idx<subnet->output_tensor_name_v.size(); out_idx++){
                    bool is_true_output = subnet->switch_info.output_branch[out_idx];
                    if((run_true_subnet && is_true_output) || (!run_true_subnet && !is_true_output)){
                        auto out_name = subnet->output_tensor_name_v[out_idx];
                        auto in_name = subnet->input_tensor_name_v[subnet->switch_info.output_from[out_idx]];
                        tensor_iteration[out_name] = iteration;
                        auto &src_tensor = must_get_tensor_in_subnet(subnet_tensor_v, in_name);
                        auto &dst_tensor = must_get_tensor_in_subnet(subnet_tensor_v, out_name);
                        dst_tensor.tensor_info.shape = src_tensor.tensor_info.shape;
                    }
                }
            }
            next_id = subnet->next_subnet_ids[run_true_subnet];
            m_profile->set_extra_data(run_true_subnet);
        } else if (subnet->subnet_mode == SUBNET_MODE_CPU) {
            m_profile->set_extra_data(subnet->cpu_info.op_type);
            #ifdef __linux__
            bm_shape_t real_out_shape[subnet->output_tensor_name_v.size()];
            #else
            std::shared_ptr<bm_shape_t> real_out_shape_(new bm_shape_t[subnet->output_tensor_name_v.size()], std::default_delete<bm_shape_t[]>());
            bm_shape_t* real_out_shape = real_out_shape_.get();
            #endif
            ret = launch_cpu_subnet(net_ctx, subnet_tensor_v, subnet, input_tensors, real_out_shape);
            BMRT_ASSERT_INFO(ret == true, "launch_cpu_subnet return false");

            tensor_idx = 0;
            for (auto& tensor_name: subnet->output_tensor_name_v) {
                auto& tensor_ext  = subnet_tensor_v->find(tensor_name)->second;
#if 0
                if (!bmrt_shape_is_same(&tensor_ext.max_shape, &real_out_shape[tensor_idx]) &&
                    (tensor_ext.mem_type & MEM_TYPE_TPU)) {
                    /* workaround : cpu layer could change the output shape, but follow static subnet
                     *              still using max shape, the tail dirty data might affect result
                     *              compare, so here memset tail memory to 0 */
                    u64 max_size =  bmrt_shape_count(&tensor_ext.max_shape) *
                                     bmrt_data_type_size(tensor_ext.tensor_info.dtype);
                    u64 real_size =  bmrt_shape_count(&real_out_shape[tensor_idx]) *
                                     bmrt_data_type_size(tensor_ext.tensor_info.dtype);
                    bm_device_mem_t tail_mem = tensor_ext.tensor_info.device_mem;
                    bm_mem_set_device_addr(&tail_mem, bm_mem_get_device_addr(tail_mem) + real_size);
                    bm_mem_set_device_size(&tail_mem, max_size - real_size);
                    bm_memset_device(m_handle, 0, tail_mem);
                    BMRT_DEBUG("memset %llx size %x to 0", bm_mem_get_device_addr(tail_mem),
                             bm_mem_get_device_size(tail_mem));
                }
#endif

                tensor_ext.tensor_info.shape = real_out_shape[tensor_idx];

                /* update net output shape/data */
                if (tensor_ext.io_type == TENSOR_TYPE_NET_OUTPUT) {
                    output_tensors[tensor_ext.io_index].shape = real_out_shape[tensor_idx];
                    subnet_tensor_s2d(devid, subnet_tensor_v, tensor_name, &output_tensors[tensor_ext.io_index].device_mem);
                }

                #ifdef DEBUG
                {
                  std::stringstream debug_msg;
                  debug_msg << "tensor " << tensor_name.c_str() << " shape [ ";
                  for (int i = 0; i < real_out_shape[tensor_idx].num_dims; i++)
                      debug_msg << real_out_shape[tensor_idx].dims[i] << " ";
                  debug_msg << "]" << endl;
                  BMRT_DEBUG("%s", debug_msg.str().c_str());
                }
                #endif

                tensor_idx++;
            }

        } else if(subnet->subnet_mode == SUBNET_MODE_TPU) {
            int subnet_input_num  = subnet->input_tensor_name_v.size();
            int subnet_output_num = subnet->output_tensor_name_v.size();
            #ifdef __linux__
            bm_tensor_t subnet_input_tensors[subnet_input_num];
            bm_tensor_t subnet_output_tensors[subnet_output_num];
            int subnet_input_elem_nums[subnet_input_num];
            int subnet_output_elem_nums[subnet_output_num];
            #else
            std::shared_ptr<bm_tensor_t> subnet_input_tensors_(new bm_tensor_t[subnet_input_num], std::default_delete<bm_tensor_t[]>());
            bm_tensor_t* subnet_input_tensors = subnet_input_tensors_.get();
            std::shared_ptr<bm_tensor_t> subnet_output_tensors_(new bm_tensor_t[subnet_output_num], std::default_delete<bm_tensor_t[]>());
            bm_tensor_t* subnet_output_tensors = subnet_output_tensors_.get();
            std::shared_ptr<int> subnet_input_elem_nums_(new int[subnet_input_num], std::default_delete<int[]>());
            int* subnet_input_elem_nums = subnet_input_elem_nums_.get();
            std::shared_ptr<int> subnet_output_elem_nums_(new int[subnet_output_num], std::default_delete<int[]>());
            int* subnet_output_elem_nums = subnet_output_elem_nums_.get();
            #endif

            /* set user input */
            tensor_idx = 0;
            for (auto& tensor_name: subnet->input_tensor_name_v) {
                auto& tensor_ext  = subnet_tensor_v->find(tensor_name)->second;
                switch(tensor_ext.io_type) {
                    case TENSOR_TYPE_NET_INPUT:
                        /* subnet input tensor is also net input tensor, using user input */
                        subnet_input_tensors[tensor_idx] = input_tensors[tensor_ext.io_index];
                        subnet_input_elem_nums[tensor_idx] = 0; // does not need to count elem_num
                        break;
                    case TENSOR_TYPE_NET_OUTPUT:
                        /* subnet input tensor is also net output tensor, using user input */
                        subnet_input_tensors[tensor_idx] = output_tensors[tensor_ext.io_index];
                        subnet_input_elem_nums[tensor_idx] = 0; // does not need to count elem_num
                        break;
                    case TENSOR_TYPE_IMM_IO:
                        /* subnet input tensor is intermediate tensor, using tensor context address */
                        subnet_input_tensors[tensor_idx] = tensor_ext.tensor_info;
                        subnet_input_elem_nums[tensor_idx] = tensor_ext.record_elem_num;
                        /* for tpu subnet, if input tensor is from cpu subnet : s2d */
                        if (tensor_ext.src_subnet && tensor_ext.src_subnet->subnet_mode == SUBNET_MODE_CPU) {
                          subnet_tensor_s2d(devid, subnet_tensor_v, tensor_name);
                          BMRT_DEBUG("TPU SUBNET TENSOR %s S2D FROM %p TO %llx",
                              tensor_name.c_str(), tensor_ext.host_mem.addr,
                              bm_mem_get_device_addr(subnet_input_tensors[tensor_idx].device_mem));
                        }
                        break;
                    default:
                        BMRT_LOG(FATAL, "error io tensor type!");
                        break;
                }
                tensor_idx++;
            }

            /* set user output */
            tensor_idx = 0;
            for (auto& tensor_name: subnet->output_tensor_name_v) {
                auto& tensor_ext  = subnet_tensor_v->find(tensor_name)->second;
                switch(tensor_ext.io_type) {
                    case TENSOR_TYPE_NET_OUTPUT:
                        /* subnet input tensor is also net output tensor, using user input */
                        subnet_output_tensors[tensor_idx] = output_tensors[tensor_ext.io_index];
                        break;
                    case TENSOR_TYPE_IMM_IO:
                        /* subnet input tensor is intermediate tensor, using tensor context address */
                        subnet_output_tensors[tensor_idx] = tensor_ext.tensor_info;
                        break;
                    case TENSOR_TYPE_NET_INPUT:
                    default:
                        BMRT_LOG(FATAL, "error io tensor type!");
                        break;
                }
                tensor_idx++;
            }

            m_profile->set_extra_data(subnet->tpu_info.is_dynamic);
            if (subnet->tpu_info.is_dynamic) {
                ret = launch_tpu_ir_subnet(net_ctx, stage, subnet,
                                     subnet_input_tensors, subnet_input_elem_nums, subnet_input_num,
                                     subnet_output_tensors, subnet_output_elem_nums, subnet_output_num, core_list, dyn_core_mask);
                BMRT_ASSERT_INFO(ret == true, "launch_tpu_ir_subnet return false");

                /* reshape output tensors */
                tensor_idx = 0;
                for (auto& tensor_name: subnet->output_tensor_name_v) {
                    auto& tensor_ext  = subnet_tensor_v->find(tensor_name)->second;
                    switch(tensor_ext.io_type) {
                        case TENSOR_TYPE_NET_OUTPUT:
                            /* subnet input tensor is also net output tensor, using user input */
                            output_tensors[tensor_ext.io_index] = subnet_output_tensors[tensor_idx];
                            break;
                        case TENSOR_TYPE_IMM_IO:
                            /* subnet input tensor is intermediate tensor, using tensor context address */
                            tensor_ext.tensor_info = subnet_output_tensors[tensor_idx];
                            tensor_ext.record_elem_num = subnet_output_elem_nums[tensor_idx];
                            break;
                        case TENSOR_TYPE_NET_INPUT:
                        default:
                            BMRT_LOG(FATAL, "error io tensor type!");
                            break;
                    }


                    #ifdef DEBUG
                    {
                      BMRT_DEBUG("tensor %s shape [ ", tensor_name.c_str());
                      std::stringstream debug_msg;
                      for (int i = 0; i < subnet_output_tensors[tensor_idx].shape.num_dims; i++)
                          debug_msg << subnet_output_tensors[tensor_idx].shape.dims[i] << " ";
                      debug_msg << " ]" << endl;
                      BMRT_DEBUG("%s", debug_msg.str().c_str());
                    }
                    #endif

                    tensor_idx++;
                }

            } else {
                ret = launch_tpu_subnet(net_ctx, stage, subnet,
                                        subnet_input_tensors, subnet_input_num,
                                        subnet_output_tensors, subnet_output_num,
                                        core_list, dyn_core_mask, next_id >=0);
                BMRT_ASSERT_INFO(ret == true, "launch_tpu_subnet return false");
            }

        } else {
            BMRT_LOG(FATAL, "Not supported subnet_mode=%d, subnet_id=%d", subnet->subnet_mode, subnet->id);
        }
        m_profile->end_subnet(net_ctx);
        if (m_subnet_time_print) {
            long elapsed = end_time(time);
            print_subnet_time(subnet, subnet->id, elapsed);
        }
        subnet = (next_id>=0)? stage->subnet_v[next_id]: nullptr;
    }

    return true;
}

}
