#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux__
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <numeric>
#include <string>
#include "bmodel.hpp"
#include "bmruntime.h"
#include "bmlib_runtime.h"
#include "kernel_module.h"

#define SP(D, T) (std::shared_ptr<T>((D), std::default_delete<T []>()))

// typedef struct bm_module
// {
//   // void *lib_handle;
//   char lib_name[28];
//   unsigned char md5[16];
// }bm_module;

// typedef struct bm_module *tpu_kernel_module_t;
// typedef int tpu_kernel_function_t;

// typedef tpu_kernel_module_t (*tpu_kernel_load_module_t)(bm_handle_t handle, const char *module_name, const char *data, size_t length);
// typedef tpu_kernel_function_t (*tpu_kernel_get_func_t)(bm_handle_t handle, tpu_kernel_module_t module, const char *function);

namespace bmruntime {

static void fill_tensor_attr(
    const bmodel::Tensor* tensor,
    tensor_attr_t* attr,
    u64 ctx_start,
    const std::vector<u64> &ctx_borders,
    const std::vector<u64> &ctx_offset)
{
  attr->shape.num_dims = tensor->shape()->Get(0)->dim()->size();
  for (int i = 0; i < attr->shape.num_dims; i++) {
    attr->shape.dims[i] = tensor->shape()->Get(0)->dim()->Get(i);
  }
  u64 addr = tensor->device_addr();
  addr += ctx_offset[get_mem_index(ctx_borders, ctx_start, addr)];
  attr->dev_mem = bm_mem_from_device(addr, tensor->size());
  attr->st_mode = (bm_store_mode_t)tensor->gmem_stmode();
  attr->pad_h   = tensor->pad_h();
}

static void fill_tensor_attr(
    const Vector<Offset<bmodel::Tensor>>* tensors,
    vector<tensor_attr_t>& tensor_v,
    u64 ctx_start,
    const std::vector<u64> &ctx_borders,
    const std::vector<u64> &ctx_offset)
{
  for (u32 i = 0; i < tensors->size(); i++) {
    auto tensor = tensors->Get(i);
    if (tensor->shape() == NULL || tensor->shape()->size() == 0) {
      continue;
    }
    tensor_attr_t attr;
    fill_tensor_attr(tensor, &attr, ctx_start, ctx_borders, ctx_offset);
    tensor_v.push_back(attr);
  }
}

static void upload_coeff_data(ModelCtx* model_ctx, const bmodel::CoeffMem* coeff_mem,
                              bm_handle_t handle, bm_device_mem_t& dev_mem)
{
  bm_status_t status = BM_SUCCESS;
  u64 size = coeff_mem->binary_coeff()->size();
#ifdef SOC_MODE
  void* vmem = NULL;
  status = bm_mem_mmap_device_mem(handle, &dev_mem, (u64*)&vmem);
  CHECK_status(status);
  model_ctx->read_binary(coeff_mem->binary_coeff(), (u8*)vmem);
  status = bm_mem_flush_device_mem(handle, &dev_mem);
  CHECK_status(status);
  bm_mem_unmap_device_mem(handle, vmem, size);
#else
#define COEFF_BLK_SIZE 0x1000000
  u8* data = new u8[COEFF_BLK_SIZE];
  auto data_sp = SP(data, u8);
  u64 left_size = size;
  u64 offset = 0;
  u64 address = bm_mem_get_device_addr(dev_mem);
  while (left_size > 0) {
    u64 data_size = (left_size >= COEFF_BLK_SIZE ? COEFF_BLK_SIZE : left_size);
    model_ctx->read_binary(coeff_mem->binary_coeff(), offset, data, data_size);
    bm_device_mem_t pmem = bm_mem_from_device(address + offset, data_size);
    status = bm_memcpy_s2d(handle, pmem, ((void*)data));
    CHECK_status(status);
    offset += data_size;
    left_size -= data_size;
  }
#endif
}

static uint32_t get_bdc_cmd_len(
    ModelCtx* model_ctx,
    const bmodel::CmdGroup* cmd_group,
    u32 start_offset, bool last_cmd)
{
  u32* cmd_buf = new u32[2];
  auto cmd_buf_sp = SP(cmd_buf, u32);
  model_ctx->read_binary(cmd_group->binary_bdc(), start_offset, (u8*)cmd_buf,
                          sizeof(u32) * 2);
  uint32_t len = 0;
  switch (bmrt_arch_info::get_bmtpu_arch()) {
    case BM1682:
    case BM1684:
      len = 1 << BDC_ENGINE_CMD_ALIGNED_BIT;
      break;
    case BM1880:
      len = 112;
      break;
    case BM1686:
    case BM1684X: {
      u32 tsk_type = (cmd_buf[1] >> 9) & 0xf;
      int eu_type = (cmd_buf[1] >> 13) & 0x1f;
      bool is_short = cmd_buf[0] & 0x1;
      if (tsk_type == 15 || tsk_type == 12) {
        len = 16;
      } else if (!is_short) {
        len = 128;
      } else if (tsk_type == 0 || tsk_type == 1) {
        len = 64;
      } else if (tsk_type == 6 || tsk_type == 13 || tsk_type == 14) {
        len = 48;
      } else if (tsk_type == 4 || tsk_type == 5 || tsk_type == 9 || tsk_type == 10) {
        len = 32;
      } else if (tsk_type == 2) {
        len = eu_type > 3 ? 32 : 48;
      } else if (tsk_type == 3) {
        len = (eu_type == 24 || eu_type == 25) ? 16 : 64;
      } else {
        BMRT_ASSERT(0);
      }
      if (last_cmd) {
        return (ALIGN(start_offset + len, 128) - start_offset);
      }
      break;
    }
    default:
      BMRT_ASSERT(0);
  }

  return len;
}

static uint32_t get_gdma_cmd_len(
    ModelCtx* model_ctx,
    const bmodel::CmdGroup* cmd_group,
    u32 start_offset, bool last_cmd)
{
  uint32_t len = 96;     //default: common gdma instrution size

  bmtpu_arch_t arch = bmrt_arch_info::get_bmtpu_arch();
  if(BM1686 == arch) {
      u32 cmd_head[2] ={0};
      model_ctx->read_binary(cmd_group->binary_gdma(), start_offset, (u8*)&cmd_head,
                          sizeof(cmd_head));
      u32 tsk_type = cmd_head[1] & 0xf;
      if(tsk_type == 0x6){ // DMA_sys
        len = 16;
      }
      // sys end
      if (last_cmd) {
        len = ALIGN(start_offset + 16, 128) - start_offset;
      }
  } else if (BM1684X == arch) {
      // sys end
      if (last_cmd) {
        len = ALIGN(start_offset + 16, 128) - start_offset;
      }
  } else {
      len = 1<<GDMA_ENGINE_CMD_ALIGNED_BIT;
  }
  return len;
}

/* Read BDC/GDMA cmd context from file, alloc device memory,
 * S2D download to DDR, save CMD_TENSOR in dev_mem_info_v.
 */
bool Bmruntime::setup_cmd_context(ModelCtx* model_ctx,
                                  const Vector<Offset<bmodel::CmdGroup>>* cmd_group,
                                  net_stage_t* stage)
{
  u32 cmd_word_num;
  u32 *cmd_buf, *p_cmd_buf;
  bm_device_mem_t pmem;

  if (cmd_group == NULL || cmd_group->size() == 0)
    return true;

  u32 bdc_total_id = 0, gdma_total_id = 0;
  u32 bdc_total_cmd_byte = 0, gdam_total_cmd_byte = 0;
  for (u32 i = 0; i < cmd_group->size(); i++) {
    stage->bdc_id.push_back(cmd_group->Get(i)->bdc_num());
    stage->gdma_id.push_back(cmd_group->Get(i)->gdma_num());
    stage->bdc_cmd_byte.push_back(cmd_group->Get(i)->bdc_cmd_byte());
    stage->gdma_cmd_byte.push_back(cmd_group->Get(i)->gdma_cmd_byte());
    bdc_total_id += cmd_group->Get(i)->bdc_num();
    gdma_total_id += cmd_group->Get(i)->gdma_num();
    bdc_total_cmd_byte += cmd_group->Get(i)->bdc_cmd_byte();
    gdam_total_cmd_byte += cmd_group->Get(i)->gdma_cmd_byte();
  }

  // ENGINE_BD
  if (bdc_total_cmd_byte > 0) {
    cmd_word_num = bdc_total_cmd_byte / sizeof(u32);
  } else {
    cmd_word_num = bdc_total_id * BD_ENGINE_COMMAND_NUM_aligned;
  }
  if (cmd_word_num != 0) {
    u64 cmd_buf_addr = must_alloc_device_mem(&pmem, cmd_word_num, "bd_cmd_mem", 4);
    m_device_mem_vec.push_back(pmem);
    cmd_buf = new u32[cmd_word_num];
    auto cmd_buf_sp = SP(cmd_buf, u32);
    p_cmd_buf = cmd_buf;
    for (u32 group_idx = 0; group_idx < cmd_group->size(); group_idx++) {
      u32 bdc_offset = 0;
      auto cur_cmd_group = cmd_group->Get(group_idx);
      for (u32 cmd_idx = 0; cmd_idx < cur_cmd_group->bdc_num(); cmd_idx++) {
        uint32_t read_size = get_bdc_cmd_len(model_ctx, cur_cmd_group, bdc_offset,
                                (cmd_idx == cur_cmd_group->bdc_num() - 1));

        model_ctx->read_binary(cur_cmd_group->binary_bdc(), bdc_offset, (u8*)p_cmd_buf,
                               read_size);
        convert_cmd(p_cmd_buf, ENGINE_BD, cmd_idx == (cur_cmd_group->bdc_num() - 1),
                    cmd_buf_addr + GLOBAL_MEM_CMD_START_OFFSET, stage);
        p_cmd_buf += read_size / sizeof(uint32_t);
        bdc_offset += read_size;
      }
    }
    m_profile->record_cmd_data(ENGINE_BD, cmd_buf, cmd_word_num*4, cmd_buf_addr);
    stage->bdc_mem.Init("bdc", m_handle, pmem, cmd_buf);
  }

  // ENGINE_GDMA
  if (gdam_total_cmd_byte > 0) {
    cmd_word_num = gdam_total_cmd_byte / sizeof(u32);
  } else {
    cmd_word_num = gdma_total_id * GDMA_ENGINE_COMMAND_NUM_aligned;
  }
  if (cmd_word_num != 0) {
    u64 cmd_buf_addr =  must_alloc_device_mem(&pmem, cmd_word_num, "gdma_cmd_mem", 4);
    m_device_mem_vec.push_back(pmem);
    cmd_buf = new u32[cmd_word_num];
    auto cmd_buf_sp = SP(cmd_buf, u32);
    p_cmd_buf = cmd_buf;
    for (u32 group_idx = 0; group_idx < cmd_group->size(); group_idx++) {
      u32 gdma_offset = 0;
      auto cur_cmd_group = cmd_group->Get(group_idx);
      for (u32 cmd_idx = 0; cmd_idx < cur_cmd_group->gdma_num(); cmd_idx++) {
        u32 gdma_size = get_gdma_cmd_len(model_ctx, cur_cmd_group,
                            gdma_offset, (cmd_idx == cur_cmd_group->gdma_num() - 1));

        model_ctx->read_binary(cur_cmd_group->binary_gdma(), gdma_offset, (u8*)p_cmd_buf,
                               gdma_size);
        convert_cmd(p_cmd_buf, ENGINE_GDMA, cmd_idx == cur_cmd_group->gdma_num() - 1,
                    cmd_buf_addr + GLOBAL_MEM_CMD_START_OFFSET, stage);
        p_cmd_buf += gdma_size / sizeof(u32);
        gdma_offset += gdma_size;
      }
    }
    m_profile->record_cmd_data(ENGINE_GDMA, cmd_buf, cmd_word_num*4 , cmd_buf_addr);
    stage->gdma_mem.Init("gdma", m_handle, pmem, cmd_buf);
  }


  return true;
}

void Bmruntime::trace()
{
  int err_count = 0;
  fprintf(stderr, "*** bmruntime trace: ***\n");
  fprintf(stderr, "============ check coeff =============\n");
  err_count += m_local_coeff->Check();
  int net_num = m_net_ctx_v.size();
  for (int i = 0; i < net_num; i++) {
    auto net_ctx = m_net_ctx_v[i];
    for (u32 j = 0; j < net_ctx->stage_v.size(); j++) {
      fprintf(stderr, "============ check net[%s] stage[%d] =======\n", net_ctx->net_name.c_str(), j);
      auto stage = net_ctx->stage_v[j];
      err_count += stage->ir_mem.Check();
      err_count += stage->gdma_mem.Check();
      err_count += stage->bdc_mem.Check();
    }
  }
  fprintf(stderr, "================\n");
  fprintf(stderr, "Total %d errors\n", err_count);
}

bool Bmruntime::setup_ir_context(ModelCtx* model_ctx, const bmodel::Binary* binary_ir,
                                 const Vector<Offset<bmodel::StageIR>>* stage_ir,
                                 net_stage_t* stage)
{
  if (binary_ir == NULL || 0 == binary_ir->size()) {
    return true;
  }

  u32 ir_len = stage_ir->Get(0)->ir_info_len();
  u32* ir_buffer = new u32[ir_len];
  auto ir_buffer_sp = SP(ir_buffer, u32);
  model_ctx->read_binary(binary_ir, 0, (u8*)ir_buffer, ir_len * sizeof(u32));
  auto pmem = must_alloc_device_mem(ir_len, "dynamic_ir", 4);
  m_device_mem_vec.push_back(pmem);
  stage->ir_mem.Init("ir", m_handle, pmem, ir_buffer);
  return true;
}

void Bmruntime::fill_subnet_tensor_map(net_ctx_t* net_ctx, net_stage_t* net_stage, SUBNET_INFO_T* subnet,
                                       const Vector<Offset<bmodel::Tensor>>* tensor_set_v, bool is_input,
                                       std::set<string> subnet_switch_inputs)
{
  for (u32 idx = 0; idx < tensor_set_v->size(); idx++) {
    auto tensor = tensor_set_v->Get(idx);
    auto tensor_name = tensor->name()->str();

    if (is_input) {
      subnet->input_tensor_name_v.push_back(tensor_name);
    } else {
      subnet->output_tensor_name_v.push_back(tensor_name);
    }

    if (net_stage->subnet_tensor_v.find(tensor_name) != net_stage->subnet_tensor_v.end())
      continue; /* subnets could share i/o tensor, only record one */

    tensor_ext_t bm_tensor_ext;
    memset(&bm_tensor_ext, 0 , sizeof(bm_tensor_ext));
    bm_tensor_ext.mem_type = MEM_TYPE_INVALID;
    bm_tensor_ext.host_mem.type = HOST_MEM_INVALID;

    /* tensor could be from net input/output/immediate */
    auto iter = std::find(net_ctx->input_name_v.begin(),
                          net_ctx->input_name_v.end(), tensor_name);
    if (iter != net_ctx->input_name_v.end()) {
      auto index = std::distance(net_ctx->input_name_v.begin(), iter);
      bm_tensor_ext.io_index = index;
      bm_tensor_ext.io_type  = TENSOR_TYPE_NET_INPUT;
    } else {
      iter = std::find(net_ctx->output_name_v.begin(),
                       net_ctx->output_name_v.end(), tensor_name);
      if (iter != net_ctx->output_name_v.end()) {
        auto index = std::distance(net_ctx->output_name_v.begin(), iter);
        bm_tensor_ext.io_index = index;
        /* if net output tensor is used as subnet input */
        bm_tensor_ext.io_type  = TENSOR_TYPE_NET_OUTPUT;
      } else {
        /* tensor is not net input/output */
        bm_tensor_ext.io_type  = TENSOR_TYPE_IMM_IO;
      }
    }

    bm_tensor_ext.tensor_info.dtype   = (bm_data_type_t)tensor->data_type();
    bm_tensor_ext.tensor_info.st_mode = (bm_store_mode_t)tensor->gmem_stmode();
    bm_tensor_ext.pad_h               = tensor->pad_h();

    bm_shape_t max_shape_reg;
    auto dim = tensor->shape()->Get(0)->dim();
    max_shape_reg.num_dims = dim ? dim->Length() : 0;
    for (int i = 0; i < max_shape_reg.num_dims; i++) {
      max_shape_reg.dims[i] = dim->Get(i);
    }
    for (int i = max_shape_reg.num_dims; i < BM_MAX_DIMS_NUM; ++i) {
      max_shape_reg.dims[i] = 1;
    }

    bm_tensor_ext.tensor_info.shape = max_shape_reg;
    bm_tensor_ext.max_shape         = max_shape_reg;

    /* device memory offset from bmcompiler */
    if (tensor->size() > 0) {
      bm_device_mem_t dev_mem;
      if (tensor->device_addr() < net_stage->ctx_start) {
        //subnet input may share mem with coeff mem
        dev_mem = bm_mem_from_device(
            tensor->device_addr() + net_stage->coeff_offset, tensor->size());
      } else {
        // rellocate
        u64 addr = tensor->device_addr();
        u32 idx = get_mem_index(net_stage->ctx_borders, net_stage->ctx_start, addr);
        addr += net_stage->ctx_offset[idx];
        dev_mem = bm_mem_from_device(addr, tensor->size());
      }
      bm_tensor_ext.tensor_info.device_mem = dev_mem;
      bm_tensor_ext.mem_type |= MEM_TYPE_TPU;
    }

    if ((tensor->mem_type() & MEM_TYPE_CPU) ||
        (subnet_switch_inputs.find(tensor_name) != subnet_switch_inputs.end()) ||
        (is_input && subnet->subnet_mode == SUBNET_MODE_SWITCH && idx == tensor_set_v->size() - 1)) {
      /* alloc host memory for cpu subnet */
      /* fix : pure cpu net input tensor_size = 0 */
      float* host_mem = NULL;
      u64    mem_size = bmrt_shape_count(&max_shape_reg);
      if (b_enable_mmap && tensor->size() > 0) {
        // [NEED FIX] subnet i/o tensor might share a large device memory as compiler using compatc alloc,
        // here, the tensor size is not accomdate with tensor shape.
        //BMRT_ASSERT(mem_size * bmrt_data_type_size(bm_tensor_ext.tensor_info.dtype) ==
        //            bm_mem_get_device_size(bm_tensor_ext.tensor_info.device_mem));
#ifndef SOC_MODE
        BMRT_LOG(FATAL, "Only soc mode run here");
#else
        bm_status_t ret = bm_mem_mmap_device_mem(m_handle, &bm_tensor_ext.tensor_info.device_mem, (u64 *)&host_mem);
        if (ret == BM_SUCCESS) {
          bm_tensor_ext.host_mem.type = HOST_MEM_MMAP;
        } else {
            BMRT_LOG(WRONG, "mmap failed, malloc host memory");
          if (!net_stage->cpu_addr && net_stage->cpu_mem_size > 0) {
            net_stage->cpu_addr = new float[net_stage->cpu_mem_size];
          }
          // To be compatible with the bmodel at low version
          if (net_stage->cpu_addr) {
            host_mem = tensor->cpu_addr() + net_stage->cpu_addr;
          } else {
            host_mem = new float[mem_size];
          }
          bm_tensor_ext.host_mem.type = HOST_MEM_ALLOC;
        }
#endif
      } else {
        if (!net_stage->cpu_addr && net_stage->cpu_mem_size > 0) {
          net_stage->cpu_addr = new float[net_stage->cpu_mem_size];
        }
        // To be compatible with the bmodel at low version
        if (net_stage->cpu_addr) {
          host_mem = tensor->cpu_addr() + net_stage->cpu_addr;
        } else {
          host_mem = new float[mem_size];
        }
        bm_tensor_ext.host_mem.type = HOST_MEM_ALLOC;
      }
      bm_tensor_ext.host_mem.addr = host_mem;
      bm_tensor_ext.host_mem.size = mem_size;
      bm_tensor_ext.mem_type |= MEM_TYPE_CPU;
    }

    if (!is_input) {
      bm_tensor_ext.src_subnet = subnet;
    }
    net_stage->subnet_tensor_v.insert(make_pair(tensor_name, bm_tensor_ext));
  }
}

void Bmruntime::fill_sub_net(ModelCtx* model_ctx, const Vector<Offset<bmodel::SubNet>>* subnet_set_v,
                             net_ctx_t* net_ctx, net_stage_t* net_stage)
{
  u64 subnet_bdc_offset = 0;
  u64 subnet_gdma_offset = 0;
  if (subnet_set_v == NULL) {
    net_stage->subnet_num = 0;
    return;
  }

  net_stage->subnet_num = subnet_set_v->size();
  // record SUBNET_SWITCH input tensors
  std::set<string> subnet_switch_inputs;
  for (int subnet_idx = 0; subnet_idx < net_stage->subnet_num; subnet_idx++) {
    auto sub_net = subnet_set_v->Get(subnet_idx);
    int subnet_mode = sub_net->subnet_mode();
    if (subnet_mode == SUBNET_MODE_SWITCH) {
      const Vector<Offset<bmodel::Tensor>>* tensor_set_v = sub_net->input_tensor();
      auto tensor_name = tensor_set_v->Get(tensor_set_v->size() - 1)->name()->str();
      subnet_switch_inputs.insert(tensor_name);
    }
  }

  for (int subnet_idx = 0; subnet_idx < net_stage->subnet_num; subnet_idx++) {
    SUBNET_INFO_T* subnet = new SUBNET_INFO_T;
    auto sub_net = subnet_set_v->Get(subnet_idx);

    int subnet_mode = sub_net->subnet_mode();
    subnet->subnet_mode = subnet_mode;

    // fill subnet input/output tensor set and map
    fill_subnet_tensor_map(net_ctx, net_stage, subnet, sub_net->input_tensor(), true, subnet_switch_inputs);
    fill_subnet_tensor_map(net_ctx, net_stage, subnet, sub_net->output_tensor(), false, subnet_switch_inputs);

    //id>=0 means support id param, -1 means not support
    if(sub_net->id()<0){
        subnet->id = subnet_idx;
        if(subnet_idx != net_stage->subnet_num-1){
            subnet->next_subnet_ids.push_back(subnet_idx+1);
        }
    } else {
        BMRT_ASSERT_INFO(sub_net->id() == subnet_idx,"id: %d is not equal to subnet_idx: %d",\
        sub_net->id(),subnet_idx);
        subnet->id = sub_net->id();
        if (sub_net->next_subnet_ids() != NULL) {
          for (size_t i = 0; i < sub_net->next_subnet_ids()->size(); i++) {
            subnet->next_subnet_ids.push_back(sub_net->next_subnet_ids()->Get(i));
          }
        }
    }

    if (subnet_mode == SUBNET_MODE_CPU) {
      auto cpu_param = sub_net->cpu_param()->Get(0); /* 1 cpu layer per subnet */
      subnet->cpu_info.op_type = cpu_param->op_type();
      subnet->cpu_info.param_size = cpu_param->binary_param()->size();
      subnet->cpu_info.user_param = new char[subnet->cpu_info.param_size];
      if (cpu_param->cpu_const()) {
        int cpu_const_num = cpu_param->cpu_const()->size();
        for (int i = 0; i < cpu_const_num; i++) {
          auto cci = cpu_param->cpu_const()->Get(i);
          auto name = cci->name()->str();
          auto tensor = net_stage->subnet_tensor_v.find(name);
          vector<u8> key = {cci->check_code()->begin(), cci->check_code()->end()};
          BMRT_ASSERT_INFO(tensor != net_stage->subnet_tensor_v.end(),"subnet_tensor_v named:%s can't find in subnet_tensor_v.end()",name.c_str());
          std::lock_guard<std::mutex> guard(m_global_cpu_const_mutex);
          auto iter = m_global_cpu_const_map.find(key);
          if (iter == m_global_cpu_const_map.end()) {
            std::unique_ptr<uint8_t[]> ptr(new uint8_t[cci->const_data()->size()]);
            tensor->second.host_mem.addr = ptr.get();
            model_ctx->read_binary(cci->const_data(), 0,
                                   (uint8_t*)(tensor->second.host_mem.addr),
                                   cci->const_data()->size());
            m_global_cpu_const_map[key] = std::move(ptr);
          } else {
            tensor->second.host_mem.addr = m_global_cpu_const_map[key].get();
          }
        }
      }
      model_ctx->read_binary(cpu_param->binary_param(), 0, (uint8_t*)(subnet->cpu_info.user_param),
                             subnet->cpu_info.param_size);

    } else if (subnet_mode == SUBNET_MODE_TPU){ /* TPU */
      subnet->tpu_info.is_dynamic = sub_net->is_dynamic();

      if (subnet->tpu_info.is_dynamic) {
        subnet->tpu_info.ir_offset = sub_net->ir_offset();
        subnet->tpu_info.ir_len = sub_net->ir_len();
      } else {
        int group_num = sub_net->cmd_group()->size();
        subnet->tpu_info.cmdgroup_num = group_num;
        subnet->tpu_info.bdc_group_id_v.resize(group_num);
        subnet->tpu_info.gdma_group_id_v.resize(group_num);
        subnet->tpu_info.bdc_cmd_byte_v.resize(group_num);
        subnet->tpu_info.gdma_cmd_byte_v.resize(group_num);

        subnet->tpu_info.bdc_offset = subnet_bdc_offset;
        subnet->tpu_info.gdma_offset = subnet_gdma_offset;

        for (int group_idx = 0; group_idx < group_num; group_idx++) {
          auto cmd_group = sub_net->cmd_group()->Get(group_idx);
          u32 group_bdc_num = cmd_group->bdc_num();
          u32 group_gdma_num = cmd_group->gdma_num();

          subnet->tpu_info.bdc_group_id_v[group_idx] = group_bdc_num;
          subnet->tpu_info.gdma_group_id_v[group_idx] = group_gdma_num;
          u32 bdc_cmd_byte = cmd_group->bdc_cmd_byte();
          u32 gdma_cmd_byte = cmd_group->gdma_cmd_byte();
          subnet->tpu_info.bdc_cmd_byte_v[group_idx] = bdc_cmd_byte;
          subnet->tpu_info.gdma_cmd_byte_v[group_idx] = gdma_cmd_byte;
          if (bdc_cmd_byte > 0) {
            subnet_bdc_offset += bdc_cmd_byte;
          } else {
            subnet_bdc_offset += group_bdc_num * (1 << BDC_ENGINE_CMD_ALIGNED_BIT);
          }
          if (gdma_cmd_byte > 0) {
            subnet_gdma_offset += gdma_cmd_byte;
          } else {
            subnet_gdma_offset += group_gdma_num * (1 << GDMA_ENGINE_CMD_ALIGNED_BIT);
          }
        }
      }
    } else if (subnet_mode == SUBNET_MODE_MERGE) {
        auto output_from = sub_net->merge_param()->output_from();
        BMRT_ASSERT_INFO(output_from->size() == subnet->output_tensor_name_v.size(), \
          "output_from.size: %d is not equal to subnet.output_tensor_name_v.size: %d", \
          output_from->size(), subnet->output_tensor_name_v.size());
        subnet->merge_info.output_from.resize(output_from->size());
        for(size_t out_idx=0; out_idx < output_from->size(); out_idx++){
            auto in_indice = output_from->Get(out_idx)->indice();
            for(size_t in_idx = 0; in_idx<in_indice->size(); in_idx++){
                subnet->merge_info.output_from[out_idx].push_back(in_indice->Get(in_idx));
            }
        }
    } else if (subnet_mode == SUBNET_MODE_SWITCH){
        if(sub_net->switch_param()){
            subnet->switch_info.valid = true;
            auto output_from = sub_net->switch_param()->output_from();
            auto output_branch = sub_net->switch_param()->output_branch();

            auto output_num = subnet->output_tensor_name_v.size();
            BMRT_ASSERT_INFO(output_from->size() == output_num,"output_from size: %d should be equal to output_num: %d",\
            output_from->size(),output_num);
            BMRT_ASSERT_INFO(output_branch->size() == output_num,"output_branch size: %d should be equal to output_num: %d",output_branch->size(),output_num);
            subnet->switch_info.output_from.resize(output_num);
            subnet->switch_info.output_branch.resize(output_num);
            for(size_t out_idx=0; out_idx < output_from->size(); out_idx++){
                subnet->switch_info.output_from[out_idx] = output_from->Get(out_idx);
                subnet->switch_info.output_branch[out_idx] = output_branch->Get(out_idx);
            }
        } else {
            subnet->switch_info.valid = false;
            BMRT_LOG(WARNING, "Missing switch info, using compatible mode instead, but there may be some problems when input size is dynamic!");
            BMRT_LOG(WARNING, "To be safe, the bmodel should be regenerated with corresponding SDK.");
        }
    }
    net_stage->subnet_v.push_back(subnet);
  }
}

bool Bmruntime::setup_profile_context(ModelCtx * model_ctx, net_stage_t* net_stage,
  const bmodel::Binary * net_profile, const bmodel::Binary * net_stat)
{
  // if no profile, no need to handle
  if (net_profile != NULL && net_profile->size()>0) {
    uint32_t size = net_profile->size();
    net_stage->net_profile.resize(size);
    model_ctx->read_binary(net_profile, net_stage->net_profile.data());
  } else {
    BMRT_LOG(WARNING, "bmodel do not contain any profile info");
    net_stage->net_profile.clear();
    return false;
  }
  if(net_stat != NULL && net_stat->size()>0) {
    uint32_t size = net_stat->size();
    net_stage->net_stat.resize(size);
    model_ctx->read_binary(net_stat, net_stage->net_stat.data());
  }
  return true;
}

void Bmruntime::set_profile_enabled(bool enable)
{
    m_profile->set_enable(enable);
}

bool Bmruntime::fill_net_ctx(
    ModelCtx* model_ctx,
    net_ctx_t* net_ctx,
    const Vector<Offset<NetParameter>>* params,
    std::vector<std::vector<u64>> &stage_ctx_sizes,
    net_stage_t *stages)
{
  if (params == NULL || params->size() == 0) {
      BMRT_LOG(WRONG, "Net[%s] has no parameter.", net_ctx->net_name.c_str());
    return false;
  }
  // fill net_ctx info by first NetParameter
  auto param = params->Get(0);
  net_ctx->is_dynamic = param->is_dynamic();
  if (net_ctx->is_dynamic) {
    net_ctx->n_can_change = param->n_dynamic();
    net_ctx->h_w_can_change = param->h_w_dynamic();
    if(!param->n_dynamic()){
        BMRT_LOG(WARNING, "Net[%s] may contains layers that not support dynamic N", net_ctx->net_name.c_str());;
    }
    if(!param->h_w_dynamic()){
        BMRT_LOG(WARNING, "Net[%s] may contains layers that not support dynamic H/W", net_ctx->net_name.c_str());;
    }
  }
  // add input/output name and type
  for (u32 i = 0; i < param->input_tensor()->size(); i++) {
    auto tensor = param->input_tensor()->Get(i);
    net_ctx->input_name_v.push_back(tensor->name()->str());
    net_ctx->input_type_v.push_back((bm_data_type_t)tensor->data_type());
    net_ctx->input_scale_v.push_back(tensor->scale());
    net_ctx->input_zero_point_v.push_back(tensor->zero_point());
  }
  for (u32 i = 0; i < param->output_tensor()->size(); i++) {
    auto tensor = param->output_tensor()->Get(i);
    net_ctx->output_name_v.push_back(tensor->name()->str());
    net_ctx->output_type_v.push_back((bm_data_type_t)tensor->data_type());
    net_ctx->output_scale_v.push_back(tensor->scale());
    net_ctx->output_zero_point_v.push_back(tensor->zero_point());
  }

  // alloc ctx memory
  std::vector<u64> max_ctx_sizes;
  bool multi_subnet = false;
  for (u32 stage_idx = 0; stage_idx < params->size(); stage_idx++) {
    auto stage = params->Get(stage_idx);
    std::vector<u64> stage_sizes;
    auto fb_sizes = stage->ctx_sizes();
    if (fb_sizes)
    {
      stage_sizes = std::vector<u64>(fb_sizes->begin(), fb_sizes->end());
    } else if(stage->ctx_size()>0){
      stage_sizes = std::vector<u64>(1, stage->ctx_size());
    }
    size_t size_min = std::min<size_t>(stage_sizes.size(), max_ctx_sizes.size()), i;
    for (i = 0; i < size_min; ++i)
    {
      if (stage_sizes[i] > max_ctx_sizes[i])
        max_ctx_sizes[i] = stage_sizes[i];
    }
    for (; i < stage_sizes.size(); ++i)
      max_ctx_sizes.push_back(stage_sizes[i]);

    auto subnet = params->Get(stage_idx)->sub_net();
    if (subnet != NULL && subnet->size() > 1) multi_subnet = true;
    stage_ctx_sizes.push_back(std::move(stage_sizes));

    stages[stage_idx].coeff_offset = m_local_coeff->Register(model_ctx, stage->coeff_mem());
  }

  if (BM1682 == bmrt_arch_info::get_bmtpu_arch() &&
      max_ctx_sizes.size() != 1)
  {
    BMRT_LOG(INFO, "BM1682 does not support multi-bulk memory feature");
    // Models using more than 4GB might compile,
    // but it shall not run on BM1682.
    BMRT_ASSERT(0);
  }

  if (!max_ctx_sizes.empty()) {
    if (multi_subnet) {
      // Own an neuron memory if subnet number > 1
      net_ctx->neuron_mem.resize(max_ctx_sizes.size());
      for (u32 i = 0; i < max_ctx_sizes.size(); ++i)
      {
        auto &mem = net_ctx->neuron_mem[i];
        must_alloc_device_mem(&mem, max_ctx_sizes[i], "neuron_mem");
        m_device_mem_vec.push_back(mem);
      }
    } else {
      // Update max_neuron_mem_size
      update_max_neuron_mem(max_ctx_sizes);
      net_ctx->neuron_mem = max_neuron_mem;
    }
  } else {
    BMRT_LOG(INFO, "net[%s] has no ctx mem", net_ctx->net_name.c_str());
  }

  return true;
}

bool Bmruntime::load_bmodel_net(ModelCtx* model_ctx, int net_idx, net_ctx_t* net_ctx)
{
  auto net_params = model_ctx->model()->net()->Get(net_idx)->parameter();
  std::vector<std::vector<u64>> stage_ctx_sizes;
  auto stages = new net_stage_t[net_params->size()];
  if (false == fill_net_ctx(model_ctx, net_ctx, net_params, stage_ctx_sizes, stages)) {
      BMRT_LOG(WRONG, "fill net[%s] context failed", net_ctx->net_name.c_str());
    return false;
  }

  for (u32 stage_idx = 0; stage_idx < net_params->size(); stage_idx++) {
    auto param = net_params->Get(stage_idx);
    auto net_stage = stages + stage_idx;
    net_ctx->stage_v.push_back(net_stage);

    // fill ctx and coeff
    net_stage->ctx_start = param->ctx_addr();
    auto &ctx_sizes = stage_ctx_sizes[stage_idx];
    if (!ctx_sizes.empty())
    {
      net_stage->ctx_offset.resize(ctx_sizes.size());
      net_stage->ctx_borders.resize(ctx_sizes.size());
      net_stage->ctx_borders[0] = ctx_sizes[0];
      for (size_t i = 1; i < ctx_sizes.size(); ++i)
      {
        net_stage->ctx_borders[i] = net_stage->ctx_borders[i - 1] + ctx_sizes[i];
      }
      for (size_t i = 0; i < ctx_sizes.size(); ++i)
      {
        u64 ctx_addr = bm_mem_get_device_addr(net_ctx->neuron_mem[i]);
        net_stage->ctx_offset[i] = ctx_addr - net_stage->ctx_start;
        if (i > 0)
        {
          net_stage->ctx_offset[i] -= net_stage->ctx_borders[i - 1];
        }
      }
    } else {
      // No neuron memory
      // No relocation required
      net_stage->ctx_borders.push_back(-1); // Max unsigned
      net_stage->ctx_offset.push_back(0);
    }

    net_stage->cpu_mem_size = param->cpu_mem_size();
    net_stage->cpu_addr = nullptr;
    m_profile->record_alloc_device_mem(m_local_coeff->GetCoeffDeviceMem(), "coeff");

    // setup input and output tensor info
    fill_tensor_attr(
        param->input_tensor(), net_stage->input_v,
        net_stage->ctx_start,
        net_stage->ctx_borders, net_stage->ctx_offset);
    fill_tensor_attr(
        param->output_tensor(), net_stage->output_v,
        net_stage->ctx_start,
        net_stage->ctx_borders, net_stage->ctx_offset);

    // setup subnet
    fill_sub_net(model_ctx, param->sub_net(), net_ctx, net_stage);

    // setup gdma/bdc, or ir
    setup_ir_context(model_ctx, param->binary_ir(), param->stage_ir(), net_stage);
    setup_cmd_context(model_ctx, param->cmd_group(), net_stage);

    // setup profile info
    if (m_profile->is_enabled()) {
      setup_profile_context(model_ctx, net_stage, param->net_profile(), param->net_stat());
    }
  }

  return true;
}

bool Bmruntime::load_bmodel_net(ModelCtx* model_ctx, int net_idx, std::shared_ptr<KernelModule> kernel_module)
{
  auto net = model_ctx->model()->net()->Get(net_idx);
  for (auto each_net : m_net_ctx_v) {
    if (each_net->net_name == net->name()->str()) {
      BMRT_LOG(WARNING, "Warning: Net[%s] exist, does not load again", net->name()->c_str());
      return true;
    }
  }
  net_ctx_t* net_ctx = new net_ctx_t();
  net_ctx->net_name = net->name()->str();
  net_ctx->kernel_module_ = kernel_module;

  // fill each stage info
  if (false == load_bmodel_net(model_ctx, net_idx, net_ctx)) {
      BMRT_LOG(WRONG, "Error: load net[%s] failed", net_ctx->net_name.c_str());
    return false;
  }

  update_max_middlebuf_size(net_ctx);
  fill_net_info(net_ctx);
  m_net_ctx_v.push_back(net_ctx);
  return true;
}

static void fill_middlebuff_size(const vector<tensor_attr_t>& attr_v,
                                 vector<u64>& size_v, bool is_dynamic)
{
  auto arch = bmrt_arch_info::get_bmtpu_arch();
  for (uint32_t i = 0; i < size_v.size(); ++i) {
    if (attr_v[i].st_mode == BM_STORE_1N ||
        arch != BM1684 ||
        is_dynamic != true) {
      continue;
    }
    bm_shape_t shape = attr_v[i].shape;
    if (attr_v[i].st_mode == BM_STORE_4N) {
      shape.dims[0] = ceiling_func(shape.dims[0], 4);
    } else if (attr_v[i].st_mode == BM_STORE_2N) {
      shape.dims[0] = ceiling_func(shape.dims[0], 2);
    }
    u64 byte_len = bmrt_shape_count(&shape) * sizeof(int);
    // We align to 128 bytes, because it will affect GDMA speed.
    byte_len = ALIGN(byte_len, 128);
    if (byte_len > size_v[i]) {
      size_v[i] = byte_len;
    }
  }
}

void Bmruntime::update_max_middlebuf_size(net_ctx_t* net_ctx) {
  u32 input_num = net_ctx->input_name_v.size();
  u32 output_num = net_ctx->output_name_v.size();
  vector<u64> input_size_v(input_num, 0);
  vector<u64> output_size_v(output_num, 0);
  for (auto& stage : net_ctx->stage_v) {
    fill_middlebuff_size(stage->input_v, input_size_v, net_ctx->is_dynamic);
    fill_middlebuff_size(stage->output_v, output_size_v, net_ctx->is_dynamic);
  }
  u64 total_middelbuf_size = 0;
  for (auto size : input_size_v) {
    total_middelbuf_size += size;
  }
  for (auto size : output_size_v) {
    total_middelbuf_size += size;
  }
  if (total_middelbuf_size > max_middle_buffer_size) {
    max_middle_buffer_size = total_middelbuf_size;
  }
}

void Bmruntime::update_net_middlebuf(net_ctx_t* net_ctx)
{
  bm_device_mem_t mem;
  u32 input_num = net_ctx->input_name_v.size();
  u32 output_num = net_ctx->output_name_v.size();
  vector<u64> input_size_v(input_num, 0);
  vector<u64> output_size_v(output_num, 0);
  for (auto& stage : net_ctx->stage_v) {
    fill_middlebuff_size(stage->input_v, input_size_v, net_ctx->is_dynamic);
    fill_middlebuff_size(stage->output_v, output_size_v, net_ctx->is_dynamic);
  }
  u64 addr = bm_mem_get_device_addr(max_middle_buffer);
  for (u32 i = 0; i < input_num; i++) {
    if (input_size_v[i] == 0) {
      bm_set_device_mem(&mem, 0, 0);
    } else {
      bm_set_device_mem(&mem, input_size_v[i], addr);
    }
    net_ctx->middlebuff_input.push_back(mem);
    addr += input_size_v[i];
  }
  for (u32 i = 0; i < output_num; i++) {
    if (output_size_v[i] == 0) {
      bm_set_device_mem(&mem, 0, 0);
    } else {
      bm_set_device_mem(&mem, output_size_v[i], addr);
    }
    net_ctx->middlebuff_output.push_back(mem);
    addr += output_size_v[i];
  }
}

void Bmruntime::update_max_neuron_mem(const std::vector<u64> &sizes)
{
  size_t size_min = std::min<size_t>(sizes.size(), max_neuron_mem.size()), i;
  for (i = 0; i < size_min; ++i)
  {
    auto &mem = max_neuron_mem[i];
    if (sizes[i] > bm_mem_get_device_size(mem)) {
      // DON'T free old memory.
      // In case any previously loaded model have already been bound with them.
      must_alloc_device_mem(&mem, sizes[i], "neuron_mem"+std::to_string(i));
      m_device_mem_vec.push_back(mem);
    }
  }
  for (; i < sizes.size(); ++i)
  {
    bm_device_mem_t mem;
    must_alloc_device_mem(&mem, sizes[i], "neuron_mem"+std::to_string(i));
    max_neuron_mem.push_back(mem);
    m_device_mem_vec.push_back(mem);
  }
}

size_t Bmruntime::size_4N_align(const bm_shape_t& shape, const bm_data_type_t& dtype)
{
  bool consider_4N =
      (dtype == BM_INT8 || dtype == BM_UINT8) && (bmrt_arch_info::get_bmtpu_arch() == BM1684);
  if (consider_4N) {
    bm_shape_t shape_fixed = shape;
    shape_fixed.dims[0] = ceiling_func(shape.dims[0], 4) * 4;
    return bmrt_shape_count(&shape_fixed);
  }
  return bmrt_shape_count(&shape) * bmrt_data_type_size(dtype);
}

void Bmruntime::fill_net_info(net_ctx_t* net_ctx)
{
  auto& net_info = net_ctx->net_info;
  net_info.name = net_ctx->net_name.c_str();
  net_info.is_dynamic = net_ctx->is_dynamic;
  net_info.input_num = net_ctx->input_name_v.size();
  net_info.input_dtypes = net_ctx->input_type_v.data();
  net_info.input_scales = net_ctx->input_scale_v.data();
  net_info.input_zero_point = net_ctx->input_zero_point_v.data();
  net_info.input_names = (const char**)malloc(net_info.input_num * sizeof(char*));
  for (int i = 0; i < net_info.input_num; i++) {
    net_info.input_names[i] = net_ctx->input_name_v[i].c_str();
  }
  net_info.output_num = net_ctx->output_name_v.size();
  net_info.output_dtypes = net_ctx->output_type_v.data();
  net_info.output_scales = net_ctx->output_scale_v.data();
  net_info.output_zero_point = net_ctx->output_zero_point_v.data();
  net_info.output_names = (const char**)malloc(net_info.output_num * sizeof(char*));
  for (int i = 0; i < net_info.output_num; i++) {
    net_info.output_names[i] = net_ctx->output_name_v[i].c_str();
  }
  net_info.max_input_bytes = (size_t*)malloc(net_info.input_num * sizeof(size_t));
  net_info.max_output_bytes = (size_t*)malloc(net_info.output_num * sizeof(size_t));
  memset(net_info.max_input_bytes, 0, net_info.input_num * sizeof(size_t));
  memset(net_info.max_output_bytes, 0, net_info.output_num * sizeof(size_t));
  net_info.stage_num = net_ctx->stage_v.size();
  net_info.stages = (bm_stage_info_t*)malloc(net_info.stage_num * sizeof(bm_stage_info_t));
  for (int i = 0; i < net_info.stage_num; i++) {
    net_info.stages[i].input_shapes = (bm_shape_t*)malloc(sizeof(bm_shape_t) * net_info.input_num);
    for (int j = 0; j < net_info.input_num; j++) {
      net_info.stages[i].input_shapes[j] = net_ctx->stage_v[i]->input_v[j].shape;
      size_t temp_size =
          size_4N_align(net_info.stages[i].input_shapes[j], net_info.input_dtypes[j]);
      if (temp_size > net_info.max_input_bytes[j]) {
        net_info.max_input_bytes[j] = temp_size;
      }
    }
    net_info.stages[i].output_shapes =
        (bm_shape_t*)malloc(sizeof(bm_shape_t) * net_info.output_num);
    for (int j = 0; j < net_info.output_num; j++) {
      net_info.stages[i].output_shapes[j] = net_ctx->stage_v[i]->output_v[j].shape;
      size_t temp_size =
          size_4N_align(net_info.stages[i].output_shapes[j], net_info.output_dtypes[j]);
      if (temp_size > net_info.max_output_bytes[j]) {
        net_info.max_output_bytes[j] = temp_size;
      }
    }
  }
}

void Bmruntime::free_net_info(net_ctx_t* net_ctx)
{
  auto& net_info = net_ctx->net_info;
  free(net_info.input_names);
  free(net_info.output_names);
  for (int i = 0; i < net_info.stage_num; i++) {
    free(net_info.stages[i].input_shapes);
    free(net_info.stages[i].output_shapes);
  }
  free(net_info.max_input_bytes);
  free(net_info.max_output_bytes);
  free(net_info.stages);
}

bool Bmruntime::load_bmodel(ModelCtx* model_ctx)
{
  bool ret = true;
  string model_chip = model_ctx->model()->chip()->str();
  if (model_chip != bmrt_arch_info::get_bmtpu_name() &&
      // BM1684X was firstly named BM1686, then Athena2 took it's name as BM1686.
      // So there are bmodels claim to be BM1686 but are actually BM1684X.
      // And we happily allow this unspeakable abomination.
      !(model_chip == "BM1686" && bmrt_arch_info::get_bmtpu_name() == "BM1684X")) {
    BMRT_LOG(WRONG, "Error: runtime arch[%s] is not the same with bmodel arch[%s]",
             bmrt_arch_info::get_bmtpu_name().c_str(), model_chip.c_str());
    return false;
  }
  u32 load_net_num = model_ctx->model()->net()->size();
  BMRT_LOG(INFO, "pre net num: %lu, load net num: %u", static_cast<unsigned long>(m_net_ctx_v.size()), load_net_num);
  if (m_net_ctx_v.size() + load_net_num > MAX_NET_NUM) {
      BMRT_LOG(WRONG, "Error: max net num [%d], new %d nets can't be loaded", MAX_NET_NUM, load_net_num);
    return false;
  }

  std::shared_ptr<KernelModule> kernel_module = nullptr;
  if (bmrt_arch_info::get_bmtpu_arch() == BM1684X) {
    load_tpu_module(model_ctx, kernel_module);
  }

  u32 cur_net_idx = m_net_ctx_v.size();
  for (u32 net_idx = 0; net_idx < load_net_num; net_idx++) {
    ret = load_bmodel_net(model_ctx, net_idx, kernel_module);
    if (!ret) {
      break;
    }
  }

  /* Although ret may be false, but we need to set middle buffer and neuron_mem
   * for the net that had beed loaded successfully.
   */
  // Process middle buffer and neuron memory share optimizaton.
  // Middle buffer is only used for BM1684 now, because of 1N/2N/4N transpose.
  if ((u64)(bm_mem_get_device_size(max_middle_buffer)) <
      max_middle_buffer_size) {
    must_alloc_device_mem(&max_middle_buffer, max_middle_buffer_size,
                          string("middle_buffer") + std::to_string(middle_buffer_num));
    m_device_mem_vec.push_back(max_middle_buffer);
    middle_buffer_num++;
  }
  for (u32 net_idx = cur_net_idx; net_idx < m_net_ctx_v.size(); net_idx++) {
    update_net_middlebuf(m_net_ctx_v[net_idx]);
  }
  return ret;
}

void Bmruntime::load_tpu_module(ModelCtx* model_ctx, std::shared_ptr<KernelModule>& kernel_module) {
  // if kernel_module does not exist in bmodel, use the default kernel in kernel_module.h
  // kernel in kernel_module.h should be update manually:
  // 1: replace lib/libbm1684x_kernel_module_*.so by the latest.
  // 2: remake tpu_runtime
  auto _kernel_module = model_ctx->model()->kernel_module();
  if (_kernel_module) {
    auto module_binary = _kernel_module->binary();
    if (module_binary->size())
    {
      std::shared_ptr<uint8_t> binary(new uint8_t[module_binary->size()], [](void* p) {
        delete [] (uint8_t*)p;
      });
      model_ctx->read_binary(module_binary, binary.get());
      kernel_module = std::make_shared<KernelModule>(m_handle, (const char*)binary.get(), module_binary->size());
      return;
    }
  }
  kernel_module = std::make_shared<KernelModule>(m_handle, (const char*)kernel_module_data, sizeof(kernel_module_data));
}

/* Load bmodel file, which is pre-compiled by bmcompiler */
bool Bmruntime::load_bmodel(const string& filepath)
{
  BMRT_LOG(INFO, "Loading bmodel from [%s]. Thanks for your patience...", filepath.c_str());
  ModelCtx model_ctx(filepath);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load model failed.");
      return false;
  }
  std::lock_guard<std::mutex> guard(m_load_mutex);
  return load_bmodel(&model_ctx);
}

/* Load bmodel data, which is pre-compiled by bmcompiler */
bool Bmruntime::load_bmodel(const void* bmodel_data, size_t size)
{
  BMRT_LOG(INFO, "Loading bmodel data. Thanks for your patience...");
  ModelCtx model_ctx(bmodel_data, size);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load model failed.");
      return false;
  }
  std::lock_guard<std::mutex> guard(m_load_mutex);
  return load_bmodel(&model_ctx);
}

bool Bmruntime::load_context(const string& ctx_dir)
{
  return load_bmodel(ctx_dir + "/compilation.bmodel");
}

BmCoeff::BmCoeff(bm_handle_t handle)
{
  BMRT_ASSERT_INFO(handle != NULL,"handle shouldn't be NULL\n");
  m_handle = handle;
  m_inner_handle = false;
  m_devid = 0;
}

BmCoeff::BmCoeff(int devid)
{
  bm_status_t status = bm_dev_request(&m_handle, devid);
  if (BM_SUCCESS != status) {
    BMRT_LOG(FATAL, "bm_dev_request failed:[%d]", status);
  }
  m_inner_handle = true;
  m_devid = devid;
}

BmCoeff::~BmCoeff()
{
  for(auto &mem: m_coeff_map)
  {
    bm_free_device(m_handle, mem.second);
  }
  if (m_inner_handle) {
    bm_dev_free(m_handle);
  }
}

u64 BmCoeff::Register(ModelCtx* model_ctx, const CoeffMem* coeff_mem)
{
  if (coeff_mem == NULL || model_ctx == NULL) {
    return 0;
  }
  u64 coeff_start = coeff_mem->address();
  u64 coeff_size = coeff_mem->binary_coeff()->size();
  u8* coeff_size_ptr = (u8*)&coeff_size;

  // check whether the same
  vector<u8> check_code = {coeff_mem->check_code()->begin(), coeff_mem->check_code()->end()};
  check_code.insert(check_code.end(), coeff_size_ptr, coeff_size_ptr + sizeof(u64));
  std::lock_guard<std::mutex> guard(m_coeff_mutex);
  auto iter = m_coeff_map.find(check_code);
  if (iter != m_coeff_map.end()) {
    return bm_mem_get_device_addr(iter->second) - coeff_start;
  }

  bm_device_mem_t pmem;
  // allocate device memory for coeff
  if (BM_SUCCESS != bm_malloc_device_byte_heap_mask(m_handle, &pmem, 7, coeff_size)) {
    BMRT_LOG(FATAL, "coeff alloc failed, size[0x%llx]", coeff_size);
  }

  m_latest_device_mem = pmem;
  upload_coeff_data(model_ctx, coeff_mem, m_handle, pmem);
  m_coeff_map.insert(std::pair<vector<u8>, bm_device_mem_t>(check_code, pmem));
  return bm_mem_get_device_addr(pmem) - coeff_start;
}

int BmCoeff::Check()
{
  int err_count = 0;
  uint8_t crc32[bmodel::SHA256_LEN];
  std::lock_guard<std::mutex> guard(m_coeff_mutex);
  for (auto& coeff : m_coeff_map) {
    auto &sha = coeff.first;
    auto &mem = coeff.second;
    u32 size = bm_mem_get_device_size(mem);
    uint8_t* buffer = new uint8_t[size];
    auto buffer_sp = SP(buffer, uint8_t);
    bm_status_t status = bm_memcpy_d2s(m_handle, buffer, coeff.second);
    CHECK_status(status);
    bmodel::CalcSha256(buffer, size, crc32);
    u64 addr = bm_mem_get_device_addr(mem);
    fprintf(stderr, "Coeff, chip[%d], SHA[%02X%02X%02X%02X], addr[0x%llx], size[0x%x]",
            m_devid, sha[0], sha[1], sha[2], sha[3], addr, size);
    if (0 != memcmp(crc32, coeff.first.data(), bmodel::SHA256_LEN)) {
      fprintf(stderr, ", Check:**FAILED**\n");
      err_count++;
    } else {
      fprintf(stderr, "\n");
    }
  }
  return err_count;
}

void BmMemory::Init(const string& description, bm_handle_t handle, const bm_device_mem_t& mem,
                    void* buffer)
{
  desc = description;
  bm_handle = handle;
  device_mem = mem;
  addr = bm_mem_get_device_addr(mem);
  bytes = bm_mem_get_device_size(mem);
  dword_len = (bytes + 3) / 4;
  bmodel::CalcSha256((uint8_t*)buffer, bytes, check_code);
  bm_status_t status = bm_memcpy_s2d(handle, mem, buffer);
  CHECK_status(status);
}

int BmMemory::Check()
{
  if (desc.empty()) {
    return 0;
  }
  int err_count = 0;
  uint8_t crc32[bmodel::SHA256_LEN];
  uint8_t* buffer = new uint8_t[bytes];
  auto buffer_sp = SP(buffer, uint8_t);
  bm_status_t status = bm_memcpy_d2s(bm_handle, buffer, device_mem);
  CHECK_status(status);
  bmodel::CalcSha256(buffer, bytes, crc32);
  fprintf(stderr, "%s, device mem, addr[0x%llx], size[0x%x]", desc.c_str(), addr, bytes);
  if (0 != memcmp(crc32, check_code, bmodel::SHA256_LEN)) {
    fprintf(stderr, ", Check: **FAILED**\n");
    err_count++;
  } else {
    fprintf(stderr, "\n");
  }
  return err_count;
}

KernelModule::KernelModule(bm_handle_t &handle, const char* file_name) : m_handle(handle) {
  _kernel_module = tpu_kernel_load_module_file(handle, file_name);
  check_exist();
  preload_funcs(handle);
}

KernelModule::~KernelModule() {
  check_exist();
  auto status = tpu_kernel_unload_module(m_handle, _kernel_module);
  BMRT_ASSERT_INFO(status == BM_SUCCESS, "kernel_module unload failed!!\n");
}

KernelModule::KernelModule(bm_handle_t &handle, const char* binary, size_t size) : m_handle(handle) {
  _kernel_module = tpu_kernel_load_module(handle, (char*)binary, size);
  check_exist();
  preload_funcs(handle);
}

void KernelModule::preload_funcs(bm_handle_t &handle) {
  _multi_fullnet_func_id = tpu_kernel_get_function(handle, _kernel_module, "sg_api_multi_fullnet");
  _dynamic_fullnet_func_id = tpu_kernel_get_function(handle, _kernel_module, "sg_api_dynamic_fullnet");
  _enable_profile_func_id = tpu_kernel_get_function(handle, _kernel_module, "sg_api_set_profile");
  _get_profile_func_id = tpu_kernel_get_function(handle, _kernel_module, "sg_api_get_profile_data");
}

tpu_kernel_function_t KernelModule::get_multi_fullnet_func_id() {
  check_exist();
  return _multi_fullnet_func_id;
}
tpu_kernel_function_t KernelModule::get_dynamic_fullnet_func_id() {
  check_exist();
  return _dynamic_fullnet_func_id;
}
tpu_kernel_function_t KernelModule::get_enable_profile_func_id() {
  check_exist();
  return _enable_profile_func_id;
}
tpu_kernel_function_t KernelModule::get_get_profile_func_id() {
  check_exist();
  return _get_profile_func_id;
}

}  // namespace bmruntime
