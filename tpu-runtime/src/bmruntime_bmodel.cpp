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
#include <map>
#include <sstream>
#include "bmodel.hpp"
#include "bmruntime.h"
#include "bmlib_runtime.h"
#include "kernel_module.h"
#include <fcntl.h>

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
    const std::vector<u64> &ctx_offset,
    uint32_t flags)
{
  attr->shape.num_dims = tensor->shape()->Get(0)->dim()->size();
  for (int i = 0; i < attr->shape.num_dims; i++) {
    attr->shape.dims[i] = tensor->shape()->Get(0)->dim()->Get(i);
  }
  u64 addr = tensor->device_addr();
  if (flags & BM_RUNTIME_SHARE_MEM) {
    addr += ctx_offset[get_mem_index(ctx_borders, ctx_start, addr)];
    addr &= bmrt_arch_info::addr_mask();
    attr->dev_mem = bm_mem_from_device(addr, tensor->size());
  } else {
    /*
      multi-core in load bmodel stage, use fake device_addr to record tensor_addr in bmodel
      and assign real device_addr in launch tensor stage
    */
    attr->dev_mem.u.device.device_addr = addr;
    attr->dev_mem.size = tensor->size();
  }
  attr->st_mode = (bm_store_mode_t)tensor->gmem_stmode();
  attr->pad_h   = tensor->pad_h();
}

static void fill_tensor_attr(
    const Vector<Offset<bmodel::Tensor>>* tensors,
    vector<tensor_attr_t>& tensor_v,
    u64 ctx_start,
    const std::vector<u64> &ctx_borders,
    const std::vector<u64> &ctx_offset,
    uint32_t flags)
{
  for (u32 i = 0; i < tensors->size(); i++) {
    auto tensor = tensors->Get(i);
    if (tensor->shape() == NULL || tensor->shape()->size() == 0) {
      continue;
    }
    tensor_attr_t attr;
    fill_tensor_attr(tensor, &attr, ctx_start, ctx_borders, ctx_offset, flags);
    tensor_v.push_back(attr);
  }
}

static void fill_io_attr(const bmodel::Tensor *tensor, tensor_attr_t *attr,
                         u64 io_offset) {
  attr->shape.num_dims = tensor->shape()->Get(0)->dim()->size();
  for (int i = 0; i < attr->shape.num_dims; i++) {
    attr->shape.dims[i] = tensor->shape()->Get(0)->dim()->Get(i);
  }
  u64 addr = tensor->device_addr() + io_offset;
  addr &= bmrt_arch_info::addr_mask();
  attr->dev_mem = bm_mem_from_device(addr, tensor->size());
  attr->st_mode = (bm_store_mode_t)tensor->gmem_stmode();
  attr->pad_h = tensor->pad_h();
}

static void fill_io_attr(const Vector<Offset<bmodel::Tensor>> *tensors,
                             vector<tensor_attr_t> &tensor_v, u64 io_offset) {
  for (u32 i = 0; i < tensors->size(); i++) {
    auto tensor = tensors->Get(i);
    if (tensor->shape() == NULL || tensor->shape()->size() == 0) {
      continue;
    }
    tensor_attr_t attr;
    fill_io_attr(tensor, &attr, io_offset);
    tensor_v.push_back(attr);
  }
}

static void fill_io_tag_attr(const bmodel::Tensor *tensor, tensor_attr_t *attr) {
  attr->shape.num_dims = tensor->shape()->Get(0)->dim()->size();
  for (int i = 0; i < attr->shape.num_dims; i++) {
    attr->shape.dims[i] = tensor->shape()->Get(0)->dim()->Get(i);
  }
  u64 addr = tensor->device_addr();
  u64 tag = ((addr >> 36) & 0x7);
  addr &= bmrt_arch_info::addr_mask();
  addr += tag << 36;
  attr->dev_mem = bm_mem_from_device(addr, tensor->size());
  attr->st_mode = (bm_store_mode_t)tensor->gmem_stmode();
  attr->pad_h = tensor->pad_h();
}


static void fill_io_tag_attr(
    const Vector<Offset<bmodel::Tensor>> *tensors,
    vector<tensor_attr_t> &tensor_v,
    u64 ctx_start,
    const std::vector<u64> &ctx_borders,
    const std::vector<u64> &ctx_offset,
    uint32_t flags)
{
  for (u32 i = 0; i < tensors->size(); i++) {
    auto tensor = tensors->Get(i);
    if (tensor->shape() == NULL || tensor->shape()->size() == 0) {
      continue;
    }
    u64 addr = tensor->device_addr();
    u64 tag = ((addr >> 36) & 0x7);
    tensor_attr_t attr;
    if (tag >= 3){
      fill_io_tag_attr(tensor, &attr);
    }
    else{
      fill_tensor_attr(tensor, &attr, ctx_start, ctx_borders, ctx_offset, flags);
    }
    tensor_v.push_back(attr);
  }
}

static void fill_io_tag_fuse_attr(
    const Vector<Offset<bmodel::Tensor>> *tensors,
    vector<tensor_attr_t> &tensor_v) {
  for (u32 i = 0; i < tensors->size(); i++) {
    auto tensor = tensors->Get(i);
    if (tensor->shape() == NULL || tensor->shape()->size() == 0) {
      continue;
    }
    tensor_attr_t attr;
    fill_io_tag_attr(tensor, &attr);
    tensor_v.push_back(attr);
  }
}

static void upload_coeff_data(ModelCtx *model_ctx,
                              const bmodel::CoeffMem *coeff_mem,
                              bm_handle_t handle,
                              bm_device_mem_u64_t &dev_mem) {
  bm_status_t status = BM_SUCCESS;
  u64 size = coeff_mem->binary_coeff()->size();
#ifdef SOC_MODE
  void *vmem = NULL;
  status = bm_mem_mmap_device_mem_u64(handle, &dev_mem, (u64 *)&vmem);
  CHECK_status(status);
  model_ctx->read_binary(coeff_mem->binary_coeff(), (u8 *)vmem);
  status = bm_mem_flush_device_mem_u64(handle, &dev_mem);
  CHECK_status(status);
  bm_mem_unmap_device_mem_u64(handle, vmem, size);
#else
  if (coeff_mem->encrypt_mode() == 0) {
#define COEFF_BLK_SIZE 0x1000000
    u8 *data = new u8[COEFF_BLK_SIZE];
    auto data_sp = SP(data, u8);
    u64 left_size = size;
    u64 offset = 0;
    u64 address = bm_mem_get_device_addr_u64(dev_mem);
    while (left_size > 0) {
      u64 data_size =
          (left_size >= COEFF_BLK_SIZE ? COEFF_BLK_SIZE : left_size);
      model_ctx->read_binary(coeff_mem->binary_coeff(), offset, data,
                             data_size);
      bm_device_mem_t pmem = bm_mem_from_device(address + offset, data_size);
      status = bm_memcpy_s2d(handle, pmem, ((void *)data));
      CHECK_status(status);
      offset += data_size;
      left_size -= data_size;
    }
  } else if (coeff_mem->encrypt_mode() == 1) {
    // encrypted data
    uint64_t out_size = 0;
    auto decrypt_data = model_ctx->read_binary_with_decrypt(
        coeff_mem->binary_coeff(), &out_size);
    auto dev_size = bm_mem_get_device_size_u64(dev_mem);
    BMRT_ASSERT_INFO((out_size == dev_size),
                     "Error: device memory vs coeff size overflow");
    status = bm_memcpy_s2d_u64(handle, dev_mem, ((void *)decrypt_data));
    free(decrypt_data);
    CHECK_status(status);
  }
#endif
}

static void update_coeff_data(ModelCtx *model_ctx,
                              const bmodel::CoeffMem *coeff_mem,
                              bm_device_mem_u64_t &dev_mem,
                              const std::vector<int> &weight_idx,
                              const std::vector<uint8_t> &file_data,
                              long long &start_position,
                              bm_handle_t handle) {
  bm_status_t status = BM_SUCCESS;
  auto location = coeff_mem->location();
  u64 address = bm_mem_get_device_addr_u64(dev_mem);
  auto dev_size = bm_mem_get_device_size_u64(dev_mem);

  for (int k = 0; k < location->size(); k++) {
    auto info = location->Get(k);
    auto loc_size = info->size();
    auto loc_offset = info->offset();
    std::vector<uint8_t> buffer(loc_size, 0);

    if (std::find(weight_idx.begin(), weight_idx.end(), k) != weight_idx.end() || weight_idx.size() == 0) {
      // file_data.size() != 0 -> update
      // file_data.size() == 0 -> empty
      if (file_data.size() != 0) {
        if (start_position + loc_size > file_data.size()) {
          throw std::runtime_error("File data does not contain enough data for the requested operation.");
        }
        buffer.assign(file_data.begin() + start_position, file_data.begin() + start_position + loc_size);
        start_position += loc_size;
      }
      bm_device_mem_t pmem = bm_mem_from_device(address + loc_offset, loc_size);
      status = bm_memcpy_s2d(handle, pmem, ((void *)buffer.data()));
      CHECK_status(status);
    }
  }
}

static uint32_t get_bdc_cmd_len(const u8 *bdc_buffer, u64 start_offset,
                                bool last_cmd) {
  uint32_t len = 0;
  switch (bmrt_arch_info::get_bmtpu_arch()) {
  case BM1682:
  case BM1684:
    len = 1 << BDC_ENGINE_CMD_ALIGNED_BIT;
    break;
  case BM1880:
    len = 112;
    break;
  case SG2380:
  case BM1690:
  case BM1688:
  case MARS3:
  case BM1684X: {
    u32 cmd_buf[2];
    memcpy(cmd_buf, bdc_buffer + start_offset, sizeof(cmd_buf));
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
    } else if (tsk_type == 4 || tsk_type == 5 || tsk_type == 9 ||
               tsk_type == 10) {
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

static uint32_t get_gdma_cmd_len(const u8 *gdma_buffer, u64 start_offset,
                                 bool last_cmd) {
  uint32_t len = 96; // default: common gdma instrution size

  bmtpu_arch_t arch = bmrt_arch_info::get_bmtpu_arch();
  if (BM1688 == arch || BM1690 == arch || MARS3 == arch || SG2380 == arch) {
    u32 cmd_head[2] = {0};
    memcpy(cmd_head, gdma_buffer + start_offset, sizeof(cmd_head));
    u32 tsk_type = cmd_head[1] & 0xf;
    if (tsk_type == 0x6) { // DMA_sys
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
    len = 1 << GDMA_ENGINE_CMD_ALIGNED_BIT;
  }
  return len;
}

/* Read BDC/GDMA cmd context from file, alloc device memory,
 * S2D download to DDR, save CMD_TENSOR in dev_mem_info_v.
 */
bool Bmruntime::setup_cmd_context(ModelCtx* model_ctx,
                                  const bmodel::NetParameter* param,
                                  net_stage_t* stage, uint32_t devid)
{
  u32 cmd_word_num;
  u32 *cmd_buf, *p_cmd_buf;
  bm_device_mem_t pmem;

  const auto core_num = stage->core_commands.size();
  for (uint32_t core_idx = 0; core_idx < core_num; core_idx++) {
      u32 bdc_total_id = 0, gdma_total_id = 0;
      u32 bdc_total_cmd_byte = 0, gdam_total_cmd_byte = 0;
      // TODO: Here is a huge problem, if have one more subnets in 1688 or sg2260
      auto subnet = param->sub_net();
      auto cmd_group = param->cmd_group();
      if (subnet) {
        auto core_commands = param->sub_net()->Get(0)->core_commands();
        if (core_commands) {
          cmd_group = core_commands->Get(core_idx)->gdma_tiu_commands();
        }
      }
      if (!cmd_group || cmd_group->size() == 0)
        continue;
      for (u32 i = 0; i < cmd_group->size(); i++) {
        stage->core_commands[core_idx].bdc_id.push_back(cmd_group->Get(i)->bdc_num());
        stage->core_commands[core_idx].gdma_id.push_back(cmd_group->Get(i)->gdma_num());
        stage->core_commands[core_idx].bdc_cmd_byte.push_back(cmd_group->Get(i)->bdc_cmd_byte());
        stage->core_commands[core_idx].gdma_cmd_byte.push_back(cmd_group->Get(i)->gdma_cmd_byte());
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
        u64 cmd_buf_addr = alloc_device_mem(devid, pmem, cmd_word_num, "bd_cmd_mem", 4);
        cmd_buf = new u32[cmd_word_num];
        auto cmd_buf_sp = SP(cmd_buf, u32);
        p_cmd_buf = cmd_buf;
        for (u32 group_idx = 0; group_idx < cmd_group->size(); group_idx++) {
          u64 bdc_offset = 0;
          auto cur_cmd_group = cmd_group->Get(group_idx);
          if (0 == cur_cmd_group->bdc_num()) {
            continue;
          }
          u8 *bdc_buffer = new u8[cur_cmd_group->binary_bdc()->size()];
          model_ctx->read_binary(cur_cmd_group->binary_bdc(), bdc_buffer);
          for (u32 cmd_idx = 0; cmd_idx < cur_cmd_group->bdc_num(); cmd_idx++) {
            uint32_t read_size =
                get_bdc_cmd_len(bdc_buffer, bdc_offset,
                                (cmd_idx == cur_cmd_group->bdc_num() - 1));
            memcpy(p_cmd_buf, bdc_buffer + bdc_offset, read_size);
            convert_cmd(p_cmd_buf, ENGINE_BD,
                        cmd_idx == (cur_cmd_group->bdc_num() - 1),
                        cmd_buf_addr + GLOBAL_MEM_CMD_START_OFFSET, stage);
            p_cmd_buf += read_size / sizeof(uint32_t);
            bdc_offset += read_size;
          }
          delete[] bdc_buffer;
        }
        m_profile->record_cmd_data(core_idx, ENGINE_BD, cmd_buf, cmd_word_num * 4,
                                   cmd_buf_addr);
        stage->core_commands[core_idx].bdc_mem.Init("bdc", m_handles[devid], pmem, cmd_buf, m_flags&BM_RUNTIME_CHECK_MEM);
      }

      // ENGINE_GDMA
      if (gdam_total_cmd_byte > 0) {
        cmd_word_num = gdam_total_cmd_byte / sizeof(u32);
      } else {
        cmd_word_num = gdma_total_id * GDMA_ENGINE_COMMAND_NUM_aligned;
      }
      if (cmd_word_num != 0) {
        u64 cmd_buf_addr = alloc_device_mem(devid, pmem, cmd_word_num, "gdma_cmd_mem", 4);
        cmd_buf = new u32[cmd_word_num];
        auto cmd_buf_sp = SP(cmd_buf, u32);
        p_cmd_buf = cmd_buf;
        for (u32 group_idx = 0; group_idx < cmd_group->size(); group_idx++) {
          u64 gdma_offset = 0;
          auto cur_cmd_group = cmd_group->Get(group_idx);
          if (0 == cur_cmd_group->gdma_num()) {
            continue;
          }
          u8 *gdma_buffer = new u8[cur_cmd_group->binary_gdma()->size()];
          model_ctx->read_binary(cur_cmd_group->binary_gdma(), gdma_buffer);
          for (u32 cmd_idx = 0; cmd_idx < cur_cmd_group->gdma_num();
               cmd_idx++) {
            u32 gdma_size =
                get_gdma_cmd_len(gdma_buffer, gdma_offset,
                                 (cmd_idx == cur_cmd_group->gdma_num() - 1));
            memcpy(p_cmd_buf, gdma_buffer + gdma_offset, gdma_size);
            convert_cmd(p_cmd_buf, ENGINE_GDMA,
                        cmd_idx == cur_cmd_group->gdma_num() - 1,
                        cmd_buf_addr + GLOBAL_MEM_CMD_START_OFFSET, stage);
            p_cmd_buf += gdma_size / sizeof(u32);
            gdma_offset += gdma_size;
          }
          delete[] gdma_buffer;
        }
        m_profile->record_cmd_data(core_idx, ENGINE_GDMA, cmd_buf, cmd_word_num * 4,
                                   cmd_buf_addr);
        stage->core_commands[core_idx].gdma_mem.Init("gdma", m_handles[devid], pmem, cmd_buf, m_flags&BM_RUNTIME_CHECK_MEM);
      }
  }

  for (uint32_t core_idx = 0; core_idx < core_num; core_idx++) {
    auto core_commands = param->sub_net()->Get(0)->core_commands();
    if(!core_commands) continue;
    auto hau_commands = core_commands->Get(core_idx)->hau_commands();
    auto sdma_commands = core_commands->Get(core_idx)->sdma_commands();

    u32 hau_total_cmd_byte = (hau_commands && hau_commands->size()) ?
                             hau_commands[0][0]->size() : 0;
    u32 sdma_total_cmd_byte = (sdma_commands && sdma_commands->size()) ?
                              sdma_commands[0][0]->size() : 0;

    // ENGINE_HAU
    if (hau_total_cmd_byte) {
      cmd_word_num = hau_total_cmd_byte / sizeof(u32);
      u64 cmd_buf_addr = alloc_device_mem(devid, pmem, cmd_word_num, "hau_cmd_mem", 4);
        cmd_buf = new u32[cmd_word_num];
        auto cmd_buf_sp = SP(cmd_buf, u32);
        p_cmd_buf = cmd_buf;

        model_ctx->read_binary(hau_commands->Get(0), (u8 *)p_cmd_buf);
        m_profile->record_cmd_data(core_idx, ENGINE_HAU, cmd_buf, cmd_word_num * 4,
                                   cmd_buf_addr);
        stage->core_commands[core_idx].hau_mem.Init("hau", m_handles[devid], pmem, cmd_buf, m_flags&BM_RUNTIME_CHECK_MEM);
    }

    // ENGINE_SDMA
    if (sdma_total_cmd_byte) {
      cmd_word_num = sdma_total_cmd_byte / sizeof(u32);
      u64 cmd_buf_addr = alloc_device_mem(devid, pmem, cmd_word_num, "sdma_cmd_mem", 4);
        cmd_buf = new u32[cmd_word_num];
        auto cmd_buf_sp = SP(cmd_buf, u32);
        p_cmd_buf = cmd_buf;

        model_ctx->read_binary(sdma_commands->Get(0), (u8 *)p_cmd_buf);
        m_profile->record_cmd_data(core_idx, ENGINE_SDMA, cmd_buf, cmd_word_num * 4,
                                   cmd_buf_addr);
        stage->core_commands[core_idx].sdma_mem.Init("sdma", m_handles[devid], pmem, cmd_buf, m_flags&BM_RUNTIME_CHECK_MEM);
    }
  }

  return true;
}

void Bmruntime::trace()
{
  int err_count = 0;
  fprintf(stderr, "*** bmruntime trace: ***\n");
  fprintf(stderr, "============ check coeff =============\n");
  for (int i = 0; i < m_device_num; i++) {
    err_count += m_local_coeffs[i]->Check();
  }
  int net_num = m_net_ctx_v.size();
  for (int i = 0; i < net_num; i++) {
    auto net_ctx = m_net_ctx_v[i];
    for (u32 j = 0; j < net_ctx->stage_v.size(); j++) {
      fprintf(stderr, "============ check net[%s] stage[%d] =======\n", net_ctx->net_name.c_str(), j);
      auto stage = net_ctx->stage_v[j];
      err_count += stage->core_commands[0].ir_mem.Check();
      err_count += stage->core_commands[0].gdma_mem.Check();
      err_count += stage->core_commands[0].bdc_mem.Check();
    }
  }
  fprintf(stderr, "================\n");
  fprintf(stderr, "Total %d errors\n", err_count);
}

bool Bmruntime::setup_ir_context(ModelCtx* model_ctx, const bmodel::Binary* binary_ir,
                                 const Vector<Offset<bmodel::StageIR>>* stage_ir,
                                 net_stage_t* stage, uint32_t devid)
{
  if (binary_ir == NULL || 0 == binary_ir->size()) {
    return true;
  }

  u64 ir_len = stage_ir->Get(0)->ir_info_len();
  u32* ir_buffer = new u32[ir_len];
  auto ir_buffer_sp = SP(ir_buffer, u32);
  model_ctx->read_binary(binary_ir, 0, (u8*)ir_buffer, ir_len * sizeof(u32));
  auto pmem = alloc_device_mem(devid, ALIGN(ir_len, 64), "dynamic_ir", 4);
  // TODO: support multi core ir
  // only use commands 0 to run ir
  stage->core_commands[0].ir_mem.Init("ir", m_handles[devid], pmem, ir_buffer);
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
      if (m_flags & BM_RUNTIME_SHARE_MEM) {
        if (tensor->device_addr() < net_stage->ctx_start) {
          // subnet input may share mem with coeff mem
          dev_mem = bm_mem_from_device(
              (tensor->device_addr() & bmrt_arch_info::addr_mask()) + net_stage->coeff_offset, tensor->size());
        } else {
          // rellocate
          u64 addr = tensor->device_addr();
          u32 idx = get_mem_index(net_stage->ctx_borders, net_stage->ctx_start, addr);
          addr += net_stage->ctx_offset[idx];
          addr &= bmrt_arch_info::addr_mask();
          dev_mem = bm_mem_from_device(addr, tensor->size());
        }
      } else {
        /*
          multi-core arch in load bmodel stage, use fake device_addr to record tensor_addr in bmodel
          and assign real device_addr in launch tensor stage
        */
        dev_mem.u.device.device_addr = tensor->device_addr();
        dev_mem.size = tensor->size();
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
      if (m_flags & BM_RUNTIME_SHARE_MEM) {
        if (b_enable_mmap && tensor->size() > 0) {
          // [NEED FIX] subnet i/o tensor might share a large device memory as compiler using compatc alloc,
          // here, the tensor size is not accomdate with tensor shape.
          //BMRT_ASSERT(mem_size * bmrt_data_type_size(bm_tensor_ext.tensor_info.dtype) ==
          //            bm_mem_get_device_size(bm_tensor_ext.tensor_info.device_mem));
  #ifndef SOC_MODE
          BMRT_LOG(FATAL, "Only soc mode run here");
  #else
          bm_status_t ret = bm_mem_mmap_device_mem(m_handles[net_ctx->device_id], &bm_tensor_ext.tensor_info.device_mem, (u64 *)&host_mem);
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
      } else {
        /*
          multi-core arch in load bmodel stage, use fake host_mem to record cpu_addr and if use_mmap
          and malloc/assign real host_mem in launch tensor stage
        */
        bm_tensor_ext.host_mem.tensor_cpu_addr = tensor->cpu_addr();
        if (b_enable_mmap && tensor->size() > 0) {
          bm_tensor_ext.host_mem.type = HOST_MEM_MMAP;
        } else {
          bm_tensor_ext.host_mem.type = HOST_MEM_ALLOC;
        }
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
  auto core_num = net_stage->core_commands.size();
  std::vector<u64> subnet_bdc_offset(core_num,0);
  std::vector<u64> subnet_gdma_offset(core_num, 0);
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
      if (false) {
        auto iter = find(dmem_info.begin(), dmem_info.end(), "host_neuron_mem");
        if (iter != dmem_info.end()) {
          auto coeff_addr = iter->addr + iter->offset;
          iter->offset += subnet->cpu_info.param_size;
          BMRT_ASSERT_INFO((iter->offset <= iter->size), "Error: device memory: coeff_mem overflow");
          auto device_mem = bm_mem_from_device(coeff_addr, subnet->cpu_info.param_size);
          bm_status_t ret = bm_mem_mmap_device_mem(m_handles[net_ctx->device_id], &device_mem, (u64 *)&(subnet->cpu_info.user_param));
        } else {
          BMRT_LOG(FATAL, "Error: device memory: coeff_mem don't alloc");
        }
      } else {
        subnet->cpu_info.user_param = new char[subnet->cpu_info.param_size];
      }
      model_ctx->read_binary(cpu_param->binary_param(), 0, (uint8_t*)(subnet->cpu_info.user_param),
                             subnet->cpu_info.param_size);

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
            if (false) {
              auto iter = find(dmem_info.begin(), dmem_info.end(), "host_coeff_mem");
              if (iter != dmem_info.end()) {
                auto coeff_addr = iter->addr + iter->offset;
                iter->offset += cci->const_data()->size();
                BMRT_ASSERT_INFO((iter->offset <= iter->size), "Error: device memory: coeff_mem overflow");
                auto device_mem = bm_mem_from_device(coeff_addr, cci->const_data()->size());
                std::unique_ptr<uint8_t[]> ptr;
                bm_status_t ret = bm_mem_mmap_device_mem(m_handles[net_ctx->device_id], &device_mem, (u64 *)ptr.get());
                tensor->second.host_mem.addr = ptr.get();
                m_global_cpu_const_map[key] = std::move(ptr);
                tensor->second.host_mem.type = HOST_MEM_MMAP;
              }
            } else {
            std::unique_ptr<uint8_t[]> ptr(new uint8_t[cci->const_data()->size()]);
            tensor->second.host_mem.addr = ptr.get();
            m_global_cpu_const_map[key] = std::move(ptr);
            tensor->second.host_mem.type = HOST_MEM_ALLOC;
            }
            model_ctx->read_binary(cci->const_data(), 0,
                                   (uint8_t*)(tensor->second.host_mem.addr),
                                   cci->const_data()->size());
          } else {
            tensor->second.host_mem.addr = m_global_cpu_const_map[key].get();
          }
        }
      }

    } else if (subnet_mode == SUBNET_MODE_TPU){ /* TPU */
      subnet->tpu_info.is_dynamic = sub_net->is_dynamic();

      if (subnet->tpu_info.is_dynamic) {
        // TODO: support multi core for dynamic
        subnet->tpu_info.core_commands.resize(1);
        subnet->tpu_info.core_commands[0].ir_offset = sub_net->ir_offset();
        subnet->tpu_info.core_commands[0].ir_len = sub_net->ir_len();
      } else {
        auto core_num =
            sub_net->core_commands() ? sub_net->core_commands()->size() : 1;
        subnet->tpu_info.core_commands.resize(core_num);
        for (uint32_t core_idx = 0; core_idx < core_num; core_idx++) {
          auto cmd_groups =
              sub_net->core_commands()
                  ? sub_net->core_commands()->Get(core_idx)->gdma_tiu_commands()
                  : sub_net->cmd_group();
          int group_num = cmd_groups->size();
          subnet->tpu_info.core_commands[core_idx].bdc_id.resize(group_num);
          subnet->tpu_info.core_commands[core_idx].gdma_id.resize(group_num);
          subnet->tpu_info.core_commands[core_idx].bdc_cmd_byte.resize(
              group_num);
          subnet->tpu_info.core_commands[core_idx].gdma_cmd_byte.resize(
              group_num);

          subnet->tpu_info.core_commands[core_idx].bdc_offset =
              subnet_bdc_offset[core_idx];
          subnet->tpu_info.core_commands[core_idx].gdma_offset =
              subnet_gdma_offset[core_idx];

          for (int group_idx = 0; group_idx < group_num; group_idx++) {
            auto cmd_group = cmd_groups->Get(group_idx);
            u32 group_bdc_num = cmd_group->bdc_num();
            u32 group_gdma_num = cmd_group->gdma_num();

            subnet->tpu_info.core_commands[core_idx].bdc_id[group_idx] =
                group_bdc_num;
            subnet->tpu_info.core_commands[core_idx].gdma_id[group_idx] =
                group_gdma_num;
            u32 bdc_cmd_byte = cmd_group->bdc_cmd_byte();
            u32 gdma_cmd_byte = cmd_group->gdma_cmd_byte();
            subnet->tpu_info.core_commands[core_idx].bdc_cmd_byte[group_idx] =
                bdc_cmd_byte;
            subnet->tpu_info.core_commands[core_idx].gdma_cmd_byte[group_idx] =
                gdma_cmd_byte;
            if (bdc_cmd_byte > 0) {
              subnet_bdc_offset[core_idx] += bdc_cmd_byte;
            } else {
              subnet_bdc_offset[core_idx] +=
                  group_bdc_num * (1 << BDC_ENGINE_CMD_ALIGNED_BIT);
            }
            if (gdma_cmd_byte > 0) {
              subnet_gdma_offset[core_idx] += gdma_cmd_byte;
            } else {
              subnet_gdma_offset[core_idx] +=
                  group_gdma_num * (1 << GDMA_ENGINE_CMD_ALIGNED_BIT);
            }
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
  auto devid = net_ctx->device_id;
  if (params == NULL || params->size() == 0) {
      BMRT_LOG(WRONG, "Net[%s] has no parameter.", net_ctx->net_name.c_str());
    return false;
  }
  // fill net_ctx info by first NetParameter
  auto param = params->Get(0);
  net_ctx->core_num = param->core_num() != 0 ? param->core_num() : 1;
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
    net_ctx->input_hidden_v.push_back(tensor->hidden());
    net_ctx->input_index_v.push_back(tensor->index());
    // net_ctx->input_from.push_back();
  }
  for (u32 i = 0; i < param->output_tensor()->size(); i++) {
    auto tensor = param->output_tensor()->Get(i);
    net_ctx->output_name_v.push_back(tensor->name()->str());
    net_ctx->output_type_v.push_back((bm_data_type_t)tensor->data_type());
    net_ctx->output_scale_v.push_back(tensor->scale());
    net_ctx->output_zero_point_v.push_back(tensor->zero_point());
    net_ctx->output_hidden_v.push_back(tensor->hidden());
    net_ctx->output_index_v.push_back(tensor->index());
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
    if (subnet != NULL && subnet->size() > 1) {
      multi_subnet = true;
    }
    if (alloc_mem == false) {
      auto iter = find(dmem_info.begin(), dmem_info.end(), "coeff_mem");
      if (iter != dmem_info.end()) {
        auto coeff_addr = iter->addr + iter->offset;
        if (m_local_coeffs[devid]->GetCoeffAddr(param->coeff_mem()) != -1 && param->coeff_mem() != NULL) {
          iter->offset += param->coeff_mem()->binary_coeff()->size();
        }
        BMRT_ASSERT_INFO((iter->offset <= iter->size), "Error: device memory: coeff_mem overflow");
        stages[stage_idx].coeff_offset = m_local_coeffs[devid]->Register(model_ctx, param->coeff_mem(), coeff_addr);
      } else {
        BMRT_LOG(WRONG, "Error: device memory: coeff_mem don't alloc");
      }
    } else {
      stages[stage_idx].coeff_offset = m_local_coeffs[devid]->Register(model_ctx, stage->coeff_mem());
    }
    stages[stage_idx].dynamic_coeff_offset = stages[stage_idx].coeff_offset;
    if (param->dynamic_combined_coeff_offset()) {
        stages[stage_idx].dynamic_coeff_offset += param->dynamic_combined_coeff_offset() - param->dynamic_coeff_offset();
    }
    if (!(m_flags & BM_RUNTIME_SHARE_MEM)) {
      stages[stage_idx].neuron_size.resize(stage_sizes.size());
      stages[stage_idx].neuron_size = stage_sizes;
    }
    stage_ctx_sizes.push_back(std::move(stage_sizes));
    stages[stage_idx].io_start = stage->io_addr();
    stages[stage_idx].io_size = stage->io_size();
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
    if (m_flags & BM_RUNTIME_SHARE_MEM) {
    if (multi_subnet) {
      // Own an neuron memory if subnet number > 1
      net_ctx->neuron_mem.resize(max_ctx_sizes.size());
      for (u32 i = 0; i < max_ctx_sizes.size(); ++i)
      {
        auto &mem = net_ctx->neuron_mem[i];
        alloc_device_mem_u64(devid, mem, max_ctx_sizes[i], "neuron_mem");
      }
    } else {
      // Update max_neuron_mem_size
      update_max_neuron_mem(devid, max_ctx_sizes);
      net_ctx->neuron_mem = max_neuron_mem[devid];
    }
    for (size_t stage_idx = 0; stage_idx < params->size(); stage_idx++) {
      stages[stage_idx].neuron_mem = net_ctx->neuron_mem;
    }
    } else {
      if (multi_subnet) {
        net_ctx->neuron_size.resize(max_ctx_sizes.size());
        for (u32 i = 0; i < max_ctx_sizes.size(); ++i)
        {
          net_ctx->neuron_size[i] = max_ctx_sizes[i];
        }
      } else {
        size_t size_min = std::min<size_t>(max_ctx_sizes.size(), max_neuron_mem_size[devid].size()), i;
        for (i = 0; i < size_min; ++i) {
          if (max_ctx_sizes[i] > max_neuron_mem_size[devid][i]) {
            max_neuron_mem_size[devid][i] = max_ctx_sizes[i];
          }
        }
        for (; i < max_ctx_sizes.size(); ++i) {
          max_neuron_mem_size[devid].push_back(max_ctx_sizes[i]);
        }
        net_ctx->neuron_size = max_neuron_mem_size[devid];
      }
    }
  }

  if (net_ctx->addr_mode == ADDR_MODE_IO_ALONE) {
    // addr alone allocate io mem
    for (u32 stage_idx = 0; stage_idx < params->size(); stage_idx++) {
      auto &s = stages[stage_idx];
      // only alloc device mem for stage_idx = 0
      // not to alloc device mem for stage_idx > 0
      if (stage_idx == 0) {
        s.io_mem = alloc_device_mem(devid, s.io_size, "io_mem");
        s.io_offset = bm_mem_get_device_addr(s.io_mem) - s.io_start;
      } else {
        s.io_mem = stages[0].io_mem;
        s.io_offset = stages[0].io_offset;
      }
    }
  }

  return true;
}

bool Bmruntime::cascade_insert_net(int net_idx, net_ctx_t *net_ctx,
                                   const std::string &main_name) {
  if (net_ctx->device_id == 0 && net_ctx->step_id == 0) {
    // fisrt step
    net_cascade_t nc;
    nc.is_dynamic = net_ctx->is_dynamic;
    nc.addr_mode = net_ctx->addr_mode;
    nc.main_name = main_name;
    nc.num_device = net_ctx->device_id + 1;
    nc.step_ids.push_back({net_idx});
    m_net_cascade_v.emplace_back(nc);
    return true;
  }
  for (auto &v : m_net_cascade_v) {
    if (v.main_name == main_name) {
      if (v.num_device <= (int)net_ctx->device_id) {
        v.num_device = net_ctx->device_id + 1;
      }
      if (net_ctx->step_id == v.step_ids.size() - 1) {
        v.step_ids[net_ctx->step_id].push_back(net_idx);
        return true;
      } else if (net_ctx->step_id == v.step_ids.size()) {
        v.step_ids.push_back({net_idx});
        return true;
      }
      break;
    }
  }
  BMRT_LOG(WRONG, "Error: load net[%s] failed", main_name.c_str());
  return false;
}

struct tensor_info_t {
  string name;
  bm_data_type_t type;
  float scale;
  int zp;
  bm_shape_t shape;
  bm_device_mem_t mem;
  size_t bytes;
  int index;
  int device_id;

  bool operator==(const tensor_info_t &other) const {
    return this->name == other.name && this->device_id == other.device_id;
  }
};

struct CompareTensors {
  bool operator()(const tensor_info_t &a, const tensor_info_t &b) const {
    if (a.device_id < b.device_id) {
      return true;
    } else if (a.device_id > b.device_id) {
      return false;
    } else {
      return a.index < b.index;
    }
  }
};

void Bmruntime::cascade_update_output(net_cascade_t &v) {
  // all steps is tensor parallel, not master-slave structure
  // bool all_steps_tp = true;
  // for (size_t s = 0; s < v.step_ids.size(); s++) {
  //   if (v.step_ids[s].size() != m_device_num) {
  //     all_steps_tp = false;
  //   }
  // }

  std::vector<tensor_info_t> output_tensors;
  for (size_t s = 0; s < v.step_ids.size(); s++) {
    for (auto &idx : v.step_ids[s]) {
      // make sure step is correct
      BMRT_ASSERT_INFO((idx >= 0 && idx < (int)m_net_ctx_v.size()),
                       "Error: step [%d] is empty in net[%s]", s,
                       v.main_name.c_str());
      BMRT_ASSERT_INFO(m_net_ctx_v[idx]->step_id == (uint32_t)s,
                       "\n step: %d, net_idx: %d, net_step:%d\nError: step "
                       "error in net[%s]",
                       s, idx, m_net_ctx_v[idx]->step_id, v.main_name.c_str());
      auto ctx = m_net_ctx_v[idx];
      auto &outputs = ctx->output_name_v;
      for (size_t i = 0; i < outputs.size(); i++) {
        if (ctx->output_hidden_v[i] == 2 || ctx->output_hidden_v[i] == 4) {
          tensor_info_t t;
          t.name = outputs[i];
          t.type = ctx->output_type_v[i];
          t.scale = ctx->output_scale_v[i];
          t.zp = ctx->output_zero_point_v[i];
          t.shape = ctx->net_info.stages[0].output_shapes[i];
          t.mem = ctx->net_info.stages[0].output_mems[i];
          t.bytes = ctx->net_info.max_output_bytes[i];
          t.index = ctx->output_index_v[i];
          t.device_id = ctx->device_id;
          output_tensors.emplace_back(t);
        } else if (ctx->output_hidden_v[i] == 0) {
          // create output hidden tensor
          mem_cascade_t output_hidden;
          output_hidden.name = outputs[i];
          output_hidden.device_id = ctx->device_id;
          bmrt_tensor_with_device(
              &output_hidden.tensor,
              ctx->stage_v[0]->output_v[i].dev_mem,
              ctx->output_type_v[i],
              ctx->stage_v[0]->output_v[i].shape);
          // output_hidden.tensor.dtype = ctx->output_type_v[i];
          // output_hidden.tensor.st_mode = BM_STORE_1N;
          // output_hidden.tensor.shape = ctx->stage_v[0]->output_v[i].shape;
          v.hidden_outputs.push_back(output_hidden);
          v.hidden_outputs_step_ids.push_back(s);
        }
      }
    }
  }

  std::stable_sort(output_tensors.begin(), output_tensors.end(), CompareTensors());
  for (auto &out : output_tensors) {
    v.output_names.push_back(out.name);
    v.output_types.push_back(out.type);
    v.output_scales.push_back(out.scale);
    v.output_zps.push_back(out.zp);
    v.output_shapes.push_back(out.shape);
    v.output_mems.push_back(out.mem);
    v.output_bytes.push_back(out.bytes);
    v.output_loc_devices.push_back(out.device_id);
  }
}

void Bmruntime::cascade_update_input(net_cascade_t &v) {
  std::vector<tensor_info_t> input_tensors;
  for (size_t s = 0; s < v.step_ids.size(); s++) {
    for (auto &idx : v.step_ids[s]) {
      auto ctx = m_net_ctx_v[idx];
      auto &inputs = ctx->input_name_v;
      for (size_t i = 0; i < inputs.size(); i++) {
        if (ctx->input_hidden_v[i] == 1 || ctx->input_hidden_v[i] == 3) {
          tensor_info_t t;
          t.name = inputs[i];
          t.type = ctx->input_type_v[i];
          t.scale = ctx->input_scale_v[i];
          t.zp = ctx->input_zero_point_v[i];
          t.shape = ctx->net_info.stages[0].input_shapes[i];
          t.mem = ctx->net_info.stages[0].input_mems[i];
          t.bytes = ctx->net_info.max_input_bytes[i];
          t.index = ctx->input_index_v[i];
          t.device_id = ctx->device_id;
          if (std::find(input_tensors.begin(), input_tensors.end(), t) == input_tensors.end()) {
            input_tensors.emplace_back(t);
          }
        } else if (ctx->input_hidden_v[i] == 0) {
          bool find = false;
          for (auto &h : v.hidden_outputs) {
            if (h.device_id == ctx->device_id && h.name == inputs[i]) {
              bmrt_tensor_with_device(
                  &h.tensor,
                  ctx->stage_v[0]->input_v[i].dev_mem,
                  ctx->input_type_v[i],
                  ctx->stage_v[0]->input_v[i].shape);
              find = true;
              break;
            }
          }
          if (find) {
            continue;
          }
          // create output hidden tensor
          mem_cascade_t input_hidden;
          input_hidden.name = inputs[i];
          input_hidden.device_id = ctx->device_id;
          bmrt_tensor_with_device(
              &input_hidden.tensor,
              ctx->stage_v[0]->input_v[i].dev_mem,
              ctx->input_type_v[i],
              ctx->stage_v[0]->input_v[i].shape);
          // input_hidden.tensor.dtype = ctx->input_type_v[i];
          // input_hidden.tensor.st_mode = BM_STORE_1N;
          // input_hidden.tensor.shape = ctx->stage_v[0]->input_v[i].shape;
          v.hidden_inputs.push_back(input_hidden);
          v.hidden_inputs_step_ids.push_back(s);
        }
      }
    }
  }

  std::stable_sort(input_tensors.begin(), input_tensors.end(), CompareTensors());
  for (auto &in : input_tensors) {
    v.input_names.push_back(in.name);
    v.input_types.push_back(in.type);
    v.input_scales.push_back(in.scale);
    v.input_zps.push_back(in.zp);
    v.input_shapes.push_back(in.shape);
    v.input_mems.push_back(in.mem);
    v.input_bytes.push_back(in.bytes);
    v.input_loc_devices.push_back(in.device_id);
  }
}

// FIXME: It is not clear why aligning to 128/256 bytes leads to wrong results in case llama2-13B.
// TODO: Try to create a life time for hidden tensors to save more memory.
void Bmruntime::cascade_update_max_hidden_buffer_size(net_cascade_t &v) {
  for (int d = 0; d < m_device_num; ++d) {
    std::vector<u64> in_max_size(v.step_ids.size(), 0);
    // for (size_t i = 0; i < v.hidden_inputs.size(); ++i) {
    //   auto &t = v.hidden_inputs[i];
    //   if (t.device_id != d) {
    //     continue;
    //   }
    //   u64 size = bmrt_tensor_bytesize(&t.tensor);
    //   // We align to 4096 bytes, because it will affect GDMA speed.
    //   size = ALIGN(size, 4096);
    //   in_max_size[v.hidden_inputs_step_ids[i]] += size;
    // }
    std::vector<u64> out_max_size(v.step_ids.size(), 0);
    for (size_t i = 0; i < v.hidden_outputs.size(); ++i) {
      auto &t = v.hidden_outputs[i];
      if (t.device_id != d) {
        continue;
      }
      u64 size = bmrt_tensor_bytesize(&t.tensor);
      // We align to 4096 bytes, because it will affect GDMA speed.
      size = ALIGN(size, 4096);
      out_max_size[v.hidden_outputs_step_ids[i]] += size;
    }
    u64 output_max_size = 0;
    u64 input_max_size = 0;
    for (size_t i = 0; i < v.step_ids.size(); ++i) {
      if (output_max_size < out_max_size[i]) {
        output_max_size = out_max_size[i];
      }
      if (input_max_size < in_max_size[i]) {
        input_max_size = in_max_size[i];
      }
    }
    if (input_max_size + output_max_size > max_hidden_buffer_size[d]) {
      max_hidden_buffer_size[d] = input_max_size + output_max_size;
    }
  }
}

void Bmruntime::cascade_update_hidden_buffer(net_cascade_t &v) {
  std::vector<std::vector<u64>> offset_v(m_device_num, std::vector<u64>(v.step_ids.size(), 0));
  for (size_t i = 0; i < v.hidden_outputs.size(); ++i) {
    auto &t = v.hidden_outputs[i];
    int d = t.device_id;
    int s = v.hidden_outputs_step_ids[i];
    u64 size = bmrt_tensor_bytesize(&t.tensor);
    u64 addr = bm_mem_get_device_addr(max_hidden_buffer[d]) + offset_v[d][s];
    bm_set_device_mem(&t.tensor.device_mem, size, addr);

    // We align to 4096 bytes, because it will affect GDMA speed.
    size = ALIGN(size, 4096);
    offset_v[d][s] += size;
  }

  for (size_t i = 0; i < offset_v.size(); ++i) {
    u64 max_size = 0;
    for (size_t j = 0; j < v.step_ids.size(); ++j) {
      if (offset_v[i][j] > max_size) {
        max_size = offset_v[i][j];
      }
    }
    for (size_t j = 0; j < v.step_ids.size(); ++j) {
      offset_v[i][j] = max_size;
    }
  }

  // for (size_t i = 0; i < v.hidden_inputs.size(); ++i) {
  //   auto &t = v.hidden_inputs[i];
  //   int d = t.device_id;
  //   int s = v.hidden_inputs_step_ids[i];
  //   u64 size = bmrt_tensor_bytesize(&t.tensor);
  //   u64 addr = bm_mem_get_device_addr(max_hidden_buffer[d]) + offset_v[d][s];
  //   bm_set_device_mem(&t.tensor.device_mem, size, addr);

  //   // We align to 4096 bytes, because it will affect GDMA speed.
  //   size = ALIGN(size, 4096);
  //   offset_v[d][s] += size;
  // }
}

void Bmruntime::cascade_update_all_info() {
  using_fast_allreduce = (getenv("BMRUNTIME_USING_FAST_ALLREDUCE") != NULL);
  if(using_fast_allreduce) {
    BMRT_LOG(INFO, "use fast AllreduceOp because BMRUNTIME_USING_FAST_ALLREDUCE env is set.");
  }
  // force use fast AllreduceOp
  using_fast_allreduce = true;
  for (auto &v : m_net_cascade_v) {
    // update output hidden tensor
    cascade_update_output(v);
    // update input hidden tensor
    cascade_update_input(v);
  }

  // Alloc input/output hidden tensor
  // 1. Hidden tensors in the same step should be allocated
  // 2. Hidden tensors in different steps share the same memory block
  std::vector<u64> size_v;
  for (auto &v : m_net_cascade_v) {
    cascade_update_max_hidden_buffer_size(v);
  }
  for (int d = 0; d < m_device_num; ++d) {
    if ((u64)(bm_mem_get_device_size(max_hidden_buffer[d])) <
        max_hidden_buffer_size[d]) {
      // alloc_device_mem(d, max_hidden_buffer[d], max_hidden_buffer_size[d], "hidden_buffer");
      BMRT_ASSERT_INFO(alloc_mem == true, "hidden buffer do not support load bmodel with given memory");
      must_alloc_device_mem(d, &max_hidden_buffer[d], max_hidden_buffer_size[d],
          string("hidden_buffer") + std::to_string(hidden_buffer_num[d]));
      m_device_mem_vec.push_back(max_hidden_buffer[d]);
      m_device_mem_ids.push_back(d);
      hidden_buffer_num[d]++;
    }
  }
  for (auto &v : m_net_cascade_v) {
    cascade_update_hidden_buffer(v);
  }

  // info for c interface
  for (auto &v : m_net_cascade_v) {
    cascade_fill_net_info(&v);
  }
}

bool Bmruntime::cascade_net_init(const Net* net, int net_idx, net_ctx_t* net_ctx) {
  net_ctx->net_name = net->name()->str();
  net_ctx->addr_mode = net->addr_mode();
  // cascade initial
  net_ctx->device_id = 0;
  net_ctx->step_id = 0;
  net_ctx->in_cascade = false;
  net_ctx->addr_mode = net->addr_mode();
  auto param = net->parameter()->Get(0);
  net_ctx->is_dynamic = param->is_dynamic();
  if (net_ctx->is_dynamic) {
    net_ctx->n_can_change = param->n_dynamic();
    net_ctx->h_w_can_change = param->h_w_dynamic();
    if (!param->n_dynamic()) {
      BMRT_LOG(WARNING, "Net[%s] may contains layers that not support dynamic N", net_ctx->net_name.c_str());
    }
    if (!param->h_w_dynamic()) {
      BMRT_LOG(WARNING, "Net[%s] may contains layers that not support dynamic H/W", net_ctx->net_name.c_str());
    }
  }
  if (net->cascade()) {
    auto main_name = net->cascade()->main_name()->str();
    if (!main_name.empty()) {
      net_ctx->device_id = net->cascade()->device_id() % m_device_num;
      net_ctx->step_id = net->cascade()->step();
      net_ctx->in_cascade = true;
      auto ret = cascade_insert_net(net_idx, net_ctx, main_name);
      if (false == ret) {
        return false;
      }
    }
  }

  return true;
}

bool Bmruntime::load_bmodel_net(ModelCtx* model_ctx, int net_idx, net_ctx_t* net_ctx)
{
  auto net = model_ctx->model()->net()->Get(net_idx);
  net_ctx->addr_mode = net->addr_mode();
  if (cascade_net_init(net, net_idx, net_ctx) == false) {
    return false;
  }
  auto devid = net_ctx->device_id;
  auto net_params = net->parameter();
  std::vector<std::vector<u64>> stage_ctx_sizes;
  auto stages = new net_stage_t[net_params->size()];
  if (false == fill_net_ctx(model_ctx, net_ctx, net_params, stage_ctx_sizes, stages)) {
      BMRT_LOG(WRONG, "fill net[%s] context failed", net_ctx->net_name.c_str());
    return false;
  }

  // fill stage
  for (u32 stage_idx = 0; stage_idx < net_params->size(); stage_idx++) {
    auto param = net_params->Get(stage_idx);
    auto net_stage = stages + stage_idx;
    net_ctx->stage_v.push_back(net_stage);

    // fill ctx and coeff
    net_stage->ctx_start = param->ctx_addr();
    net_stage->dynamic_ctx_start = param->dynamic_ctx_addr() ? param->dynamic_ctx_addr() : param->ctx_addr();

    // Use relative address since 1688.
    auto ctx_start = net_stage->ctx_start & bmrt_arch_info::addr_mask();
    auto dynamic_ctx_start = net_stage->dynamic_ctx_start & bmrt_arch_info::addr_mask();
    auto &ctx_sizes = stage_ctx_sizes[stage_idx];
    if (!ctx_sizes.empty())
    {
      net_stage->ctx_offset.resize(ctx_sizes.size());
      net_stage->dynamic_ctx_offset.resize(ctx_sizes.size());
      net_stage->ctx_borders.resize(ctx_sizes.size());
      net_stage->ctx_borders[0] = ctx_sizes[0];
      for (size_t i = 1; i < ctx_sizes.size(); ++i)
      {
        net_stage->ctx_borders[i] = net_stage->ctx_borders[i - 1] + ctx_sizes[i];
      }
      /*
        Multi-core runtime not set ctx_offset in load bmodel stage,
        set ctx_offset in launch tensor stage
      */
      if (m_flags & BM_RUNTIME_SHARE_MEM) {
        for (size_t i = 0; i < ctx_sizes.size(); ++i)
        {
          if (net_stage->neuron_mem[i].size > 0) {
            u64 ctx_addr = bm_mem_get_device_addr_u64(net_stage->neuron_mem[i]);
            net_stage->ctx_offset[i] = ctx_addr - ctx_start;
            net_stage->dynamic_ctx_offset[i] = ctx_addr - dynamic_ctx_start;
          } else {
            net_stage->ctx_offset[i] = 0;
            net_stage->dynamic_ctx_offset[i] = 0;
          }
          if (i > 0)  {
            net_stage->ctx_offset[i] -= net_stage->ctx_borders[i - 1];
            net_stage->dynamic_ctx_offset[i] -= net_stage->ctx_borders[i - 1];
          }
        }
      }
    } else {
      // No neuron memory
      // No relocation required
      net_stage->ctx_borders.push_back(-1); // Max unsigned
      net_stage->ctx_offset.push_back(0);
      net_stage->dynamic_ctx_offset.push_back(0);
    }

    net_stage->cpu_mem_size = param->cpu_mem_size();
    net_stage->cpu_addr = nullptr;

    mem_pair_t mem_pair = {
        bm_mem_get_device_addr_u64(m_local_coeffs[devid]->GetCoeffDeviceMem()),
        bm_mem_get_device_size_u64(m_local_coeffs[devid]->GetCoeffDeviceMem())};
    m_profile->record_alloc_device_mem(mem_pair, "coeff");

    // setup input and output tensor info
    if (net_ctx->addr_mode == ADDR_MODE_IO_ALONE) {
      fill_io_attr(param->input_tensor(), net_stage->input_v,
                   net_stage->io_offset);
      fill_io_attr(param->output_tensor(), net_stage->output_v,
                   net_stage->io_offset);
    } else if (net_ctx->addr_mode == ADDR_MODE_IO_TAG) {
      fill_io_tag_attr(param->input_tensor(), net_stage->input_v,
                       net_stage->ctx_start, net_stage->ctx_borders,
                       net_stage->ctx_offset, m_flags);
      fill_io_tag_attr(param->output_tensor(), net_stage->output_v,
                       net_stage->ctx_start, net_stage->ctx_borders,
                       net_stage->ctx_offset, m_flags);
    } else if (net_ctx->addr_mode == ADDR_MODE_IO_TAG_FUSE) {
      fill_io_tag_fuse_attr(param->input_tensor(), net_stage->input_v);
      fill_io_tag_fuse_attr(param->output_tensor(), net_stage->output_v);
    } else {
      fill_tensor_attr(param->input_tensor(), net_stage->input_v,
                       net_stage->ctx_start, net_stage->ctx_borders,
                       net_stage->ctx_offset, m_flags);
      fill_tensor_attr(param->output_tensor(), net_stage->output_v,
                       net_stage->ctx_start, net_stage->ctx_borders,
                       net_stage->ctx_offset, m_flags);
    }

    // setup subnet
    const auto core_num = param->core_num() != 0 ? param->core_num() : 1;
    net_stage->core_commands.resize(core_num);
    fill_sub_net(model_ctx, param->sub_net(), net_ctx, net_stage);

    // setup gdma/bdc, or ir
    setup_ir_context(model_ctx, param->binary_ir(), param->stage_ir(), net_stage, devid);
    setup_cmd_context(model_ctx, param, net_stage, devid);

    // setup profile info
    if (m_profile->is_enabled()) {
      setup_profile_context(model_ctx, net_stage, param->net_profile(), param->net_stat());
    }
  }

  // fill mem info
  bmodel::bmodel_mem_info_t bmem_info = model_ctx->get_bmodel_mem_info();
  net_ctx->mem_info_dict.emplace("bd_cmd_mem", bmem_info.bd_cmd_mem_size);
  net_ctx->mem_info_dict.emplace("gdma_cmd_mem", bmem_info.gdma_cmd_mem_size);
  net_ctx->mem_info_dict.emplace("hau_cmd_mem", bmem_info.hau_cmd_mem_size);
  net_ctx->mem_info_dict.emplace("sdma_cmd_mem", bmem_info.sdma_cmd_mem_size);
  net_ctx->mem_info_dict.emplace("neuron_mem", bmem_info.neuron_mem_size);
  net_ctx->mem_info_dict.emplace("coeff_mem", bmem_info.coeff_mem_size);
  net_ctx->mem_info_dict.emplace("dynamic_ir_mem", bmem_info.dynamic_ir_mem_size);
  net_ctx->mem_info_dict.emplace("dynamic_output_mem", bmem_info.dynamic_output_number * sizeof(bm_shape_ex_t));
  net_ctx->mem_info_dict.emplace("host_neuron_mem", bmem_info.host_neuron_mem_size);
  net_ctx->mem_info_dict.emplace("host_coeff_mem", bmem_info.host_coeff_mem_size);
  net_ctx->mem_info_dict.emplace("hidden_buffer", bmem_info.hidden_buffer_size);
  net_ctx->mem_info_dict.emplace("middle_buffer", bmem_info.middle_buffer_size);
  return true;
}

bool Bmruntime::load_bmodel_net(ModelCtx* model_ctx, int net_idx)
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

  // fill each stage info
  if (false == load_bmodel_net(model_ctx, net_idx, net_ctx)) {
      BMRT_LOG(WRONG, "Error: load net[%s] failed", net_ctx->net_name.c_str());
    return false;
  }
  net_ctx->kernel_module_ = kernel_modules[net_ctx->device_id];
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
  auto devid = net_ctx->device_id;
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
  if (total_middelbuf_size > max_middle_buffer_size[devid]) {
    max_middle_buffer_size[devid] = total_middelbuf_size;
  }
}

void Bmruntime::update_net_middlebuf(net_ctx_t* net_ctx)
{
  auto devid = net_ctx->device_id;
  bm_device_mem_t mem;
  u32 input_num = net_ctx->input_name_v.size();
  u32 output_num = net_ctx->output_name_v.size();
  vector<u64> input_size_v(input_num, 0);
  vector<u64> output_size_v(output_num, 0);
  for (auto& stage : net_ctx->stage_v) {
    fill_middlebuff_size(stage->input_v, input_size_v, net_ctx->is_dynamic);
    fill_middlebuff_size(stage->output_v, output_size_v, net_ctx->is_dynamic);
  }
  u64 addr = bm_mem_get_device_addr(max_middle_buffer[devid]);
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

void Bmruntime::update_max_neuron_mem(uint32_t devid, const std::vector<u64> &sizes)
{
  size_t size_min = std::min<size_t>(sizes.size(), max_neuron_mem[devid].size()), i;
  for (i = 0; i < size_min; ++i)
  {
    auto &mem = max_neuron_mem[devid][i];
    if (sizes[i] > bm_mem_get_device_size_u64(mem)) {
      // DON'T free old memory.
      // In case any previously loaded model have already been bound with them.
      // TODO: if using prealloc neuron memory. alloc may be failed.
      alloc_device_mem_u64(devid, mem, sizes[i], "neuron_mem");
    }
  }
  for (; i < sizes.size(); ++i)
  {
    bm_device_mem_u64_t mem;
    alloc_device_mem_u64(devid, mem, sizes[i], "neuron_mem");
    max_neuron_mem[devid].push_back(mem);
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
  net_info.core_num = net_ctx->core_num;
  net_info.is_dynamic = net_ctx->is_dynamic;
  net_info.addr_mode = net_ctx->addr_mode;
  net_info.input_num = net_ctx->input_name_v.size();
  net_info.input_dtypes = net_ctx->input_type_v.data();
  net_info.input_scales = net_ctx->input_scale_v.data();
  net_info.input_zero_point = net_ctx->input_zero_point_v.data();
  net_info.input_names = (const char**)malloc(net_info.input_num * sizeof(char*));
  net_info.input_loc_devices = (int*)malloc(net_info.input_num * sizeof(int));
  for (int i = 0; i < net_info.input_num; i++) {
    net_info.input_names[i] = net_ctx->input_name_v[i].c_str();
    net_info.input_loc_devices[i] = 0;
  }
  net_info.output_num = net_ctx->output_name_v.size();
  net_info.output_dtypes = net_ctx->output_type_v.data();
  net_info.output_scales = net_ctx->output_scale_v.data();
  net_info.output_zero_point = net_ctx->output_zero_point_v.data();
  net_info.output_names = (const char**)malloc(net_info.output_num * sizeof(char*));
  net_info.output_loc_devices = (int*)malloc(net_info.output_num * sizeof(int));
  for (int i = 0; i < net_info.output_num; i++) {
    net_info.output_names[i] = net_ctx->output_name_v[i].c_str();
    net_info.output_loc_devices[i] = 0;
  }
  net_info.max_input_bytes = (size_t*)malloc(net_info.input_num * sizeof(size_t));
  net_info.max_output_bytes = (size_t*)malloc(net_info.output_num * sizeof(size_t));
  memset(net_info.max_input_bytes, 0, net_info.input_num * sizeof(size_t));
  memset(net_info.max_output_bytes, 0, net_info.output_num * sizeof(size_t));
  net_info.stage_num = net_ctx->stage_v.size();
  net_info.stages = (bm_stage_info_t*)malloc(net_info.stage_num * sizeof(bm_stage_info_t));
  for (int i = 0; i < net_info.stage_num; i++) {
    net_info.stages[i].input_shapes = (bm_shape_t*)malloc(sizeof(bm_shape_t) * net_info.input_num);
    net_info.stages[i].input_mems = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t) * net_info.input_num);
    for (int j = 0; j < net_info.input_num; j++) {
      net_info.stages[i].input_shapes[j] = net_ctx->stage_v[i]->input_v[j].shape;
      net_info.stages[i].input_mems[j] = net_ctx->stage_v[i]->input_v[j].dev_mem;
      size_t temp_size =
          size_4N_align(net_info.stages[i].input_shapes[j], net_info.input_dtypes[j]);
      if (temp_size > net_info.max_input_bytes[j]) {
        net_info.max_input_bytes[j] = temp_size;
      }
    }
    net_info.stages[i].output_shapes =
        (bm_shape_t*)malloc(sizeof(bm_shape_t) * net_info.output_num);
    net_info.stages[i].output_mems = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t) * net_info.output_num);
    for (int j = 0; j < net_info.output_num; j++) {
      net_info.stages[i].output_shapes[j] = net_ctx->stage_v[i]->output_v[j].shape;
      net_info.stages[i].output_mems[j] = net_ctx->stage_v[i]->output_v[j].dev_mem;
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
    free(net_info.stages[i].input_mems);
    free(net_info.stages[i].output_mems);
  }
  free(net_info.max_input_bytes);
  free(net_info.max_output_bytes);
  free(net_info.input_loc_devices);
  free(net_info.output_loc_devices);
  free(net_info.stages);
}

void Bmruntime::free_dyn_neuron(net_ctx_t* net_ctx) {
  if (!alloc_mem) {
    return;
  }
  BMRT_DEBUG("im free_dyn_neuron\n");
  auto dev_id = net_ctx->device_id;
  for (auto &dyn_mem_pair : net_ctx->dyn_neuron_stage_dict) {
    auto dyn_neuron_stage = dyn_mem_pair.second;
    for (size_t i = 0; i < dyn_neuron_stage->neuron_mem.size(); ++i) {
      BMRT_DEBUG("Free device memory, byte size %d\n", bm_mem_get_device_size_u64(dyn_neuron_stage->neuron_mem[i]));
      free_device_mem_u64(dev_id, dyn_neuron_stage->neuron_mem[i]);
    }
    delete dyn_mem_pair.second;
  }
}

void Bmruntime::cascade_fill_net_info(net_cascade_t * net_cascade) {
  auto &net_info = net_cascade->net_info;
  net_info.name = net_cascade->main_name.c_str();
  net_info.is_dynamic = net_cascade->is_dynamic;
  net_info.input_num = net_cascade->input_names.size();
  net_info.input_dtypes = net_cascade->input_types.data();
  net_info.input_scales = net_cascade->input_scales.data();
  net_info.input_zero_point = net_cascade->input_zps.data();
  net_info.input_names = (const char**)malloc(net_info.input_num * sizeof(char*));
  net_info.input_loc_devices = (int*)malloc(net_info.input_num * sizeof(int));
  for (int i = 0; i < net_info.input_num; i++) {
    net_info.input_names[i] = net_cascade->input_names[i].c_str();
    net_info.input_loc_devices[i] = net_cascade->input_loc_devices[i];
  }
  net_info.output_num = net_cascade->output_names.size();
  net_info.output_dtypes = net_cascade->output_types.data();
  net_info.output_scales = net_cascade->output_scales.data();
  net_info.output_zero_point = net_cascade->output_zps.data();
  net_info.output_names = (const char**)malloc(net_info.output_num * sizeof(char*));
  for (int i = 0; i < net_info.output_num; i++) {
    net_info.output_names[i] = net_cascade->output_names[i].c_str();
  }
  net_info.max_input_bytes = (size_t*)malloc(net_info.input_num * sizeof(size_t));
  net_info.max_output_bytes = (size_t*)malloc(net_info.output_num * sizeof(size_t));
  memset(net_info.max_input_bytes, 0, net_info.input_num * sizeof(size_t));
  memset(net_info.max_output_bytes, 0, net_info.output_num * sizeof(size_t));
  net_info.stage_num = 1;
  net_info.stages = (bm_stage_info_t*)malloc(net_info.stage_num * sizeof(bm_stage_info_t));
  net_info.stages[0].input_shapes = (bm_shape_t*)malloc(sizeof(bm_shape_t) * net_info.input_num);
  net_info.stages[0].input_mems = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t) * net_info.input_num);
  for (int j = 0; j < net_info.input_num; j++) {
    net_info.stages[0].input_shapes[j] = net_cascade->input_shapes[j];
    net_info.stages[0].input_mems[j] = net_cascade->input_mems[j];
    net_info.max_input_bytes[j] = net_cascade->input_bytes[j];
  }
  net_info.stages[0].output_shapes =
      (bm_shape_t*)malloc(sizeof(bm_shape_t) * net_info.output_num);
  net_info.stages[0].output_mems = (bm_device_mem_t*)malloc(sizeof(bm_device_mem_t) * net_info.output_num);
  net_info.output_loc_devices = (int*)malloc(net_info.output_num * sizeof(int));
  for (int j = 0; j < net_info.output_num; j++) {
    net_info.stages[0].output_shapes[j] = net_cascade->output_shapes[j];
    net_info.stages[0].output_mems[j] = net_cascade->output_mems[j];
    net_info.max_output_bytes[j] = net_cascade->output_bytes[j];
    net_info.output_loc_devices[j] = net_cascade->output_loc_devices[j];
  }
  net_info.addr_mode = net_cascade->addr_mode;
}

void Bmruntime::cascade_free_net_info(net_cascade_t * net_cascade) {
  auto& net_info = net_cascade->net_info;
  free(net_info.input_names);
  free(net_info.output_names);
  for (int i = 0; i < net_info.stage_num; i++) {
    free(net_info.stages[i].input_shapes);
    free(net_info.stages[i].output_shapes);
    free(net_info.stages[i].input_mems);
    free(net_info.stages[i].output_mems);
  }
  free(net_info.max_input_bytes);
  free(net_info.max_output_bytes);
  free(net_info.stages);
}

bool Bmruntime::load_bmodel(ModelCtx* model_ctx)
{
  bool ret = true;
  string model_chip = model_ctx->model()->chip()->str();
  if (model_chip != bmrt_arch_info::get_bmtpu_name()) {
      // BM1684X was firstly named BM1686, then Athena2 took it's name as BM1688.
      // So there are bmodels claim to be BM1686 but are actually BM1684X.
      // And we happily allow this unspeakable abomination.
      if(model_chip == "BM1686" && bmrt_arch_info::get_bmtpu_name() == "BM1684X") {
      } else if(model_chip == "CV186X" && bmrt_arch_info::get_bmtpu_name() == "BM1688") {
      } else if(model_chip == "MARS3"){
      } else if(model_chip == "BM1686" && bmrt_arch_info::get_bmtpu_name() == "BM1688") {
      } else if(model_chip == "SG2380" && bmrt_arch_info::get_bmtpu_name() == "SG2380") {
      } else {
        BMRT_LOG(WRONG, "Error: runtime arch[%s] is not the same with bmodel arch[%s]",
             bmrt_arch_info::get_bmtpu_name().c_str(), model_chip.c_str());
        return false;
      }
  }
  auto version = model_ctx->model()->version()->c_str();
  const char *bmrt_version = _bmrt_version();
  const char *sophon_driver_version = _libsophon_version();

  BMRT_LOG(INFO, "Bmodel loaded, version %s", version);
  BMRT_LOG(INFO, "BM Runtime: %s", bmrt_version);
  BMRT_LOG(INFO, "BM Sophon driver version: %s", sophon_driver_version);
  u32 load_net_num = model_ctx->model()->net()->size();
  BMRT_LOG(INFO, "pre net num: %lu, load net num: %u", static_cast<unsigned long>(m_net_ctx_v.size()), load_net_num);
  // remove load net num restrict
  // if (m_net_ctx_v.size() + load_net_num > MAX_NET_NUM) {
  //     BMRT_LOG(WRONG, "Error: max net num [%d], new %d nets can't be loaded", MAX_NET_NUM, load_net_num);
  //   return false;
  // }

  load_tpu_module(model_ctx);
  load_cpu_module(model_ctx);

  u32 cur_net_idx = m_net_ctx_v.size();
  for (u32 net_idx = 0; net_idx < load_net_num; net_idx++) {
    ret = load_bmodel_net(model_ctx, net_idx);
    if (!ret) {
      break;
    }
  }
  cascade_update_all_info();
  /* Although ret may be false, but we need to set middle buffer and neuron_mem
   * for the net that had beed loaded successfully.
   */
  // Process middle buffer and neuron memory share optimizaton.
  // Middle buffer is only used for BM1684 now, because of 1N/2N/4N transpose.
  if (model_chip == "BM1684") {
    for (int i = 0; i < m_device_num; i++) {
      if ((u64)(bm_mem_get_device_size(max_middle_buffer[i])) <
          max_middle_buffer_size[i]) {
        BMRT_ASSERT_INFO(alloc_mem == true, "middel buffer do not support load bmodel with given memory");
        must_alloc_device_mem(i, &max_middle_buffer[i], max_middle_buffer_size[i],
                              string("middle_buffer") + std::to_string(middle_buffer_num[i]));
        m_device_mem_vec.push_back(max_middle_buffer[i]);
        m_device_mem_ids.push_back(i);
        // alloc_device_mem(i, max_middle_buffer[i], max_middle_buffer_size[i], "middle_buffer");
        middle_buffer_num[i]++;
      }
    }
    for (u32 net_idx = cur_net_idx; net_idx < m_net_ctx_v.size(); net_idx++) {
      update_net_middlebuf(m_net_ctx_v[net_idx]);
    }
  }
  return ret;
}

bool Bmruntime::update_net_coeff(
    ModelCtx *model_ctx,
    int net_idx,
    int mem_idx,
    const std::vector<int> &weight_idx,
    const std::vector<uint8_t> &file_data,
    long long &start_position)
{
  auto net = model_ctx->model()->net()->Get(net_idx);
  auto param = net->parameter()->Get(0); // only update stage 0
  m_local_coeffs[0]->Update( //default for soc device, only device 0
      model_ctx, param->coeff_mem(), mem_idx, weight_idx, file_data, start_position);
  return true;
}

bool Bmruntime::update_bmodel_coeff (
  ModelCtx* model_ctx,
  const std::string &update_path,
  const std::vector<int> &net_idx,
  const std::vector<int> &mem_idx,
  const std::vector<std::vector<int>> &weight_idx)
{
  bool ret = true;
  int load_net_num = (int)model_ctx->model()->net()->size();

  uint64_t decrypted_size = 0;

  uint8_t *decrypted_data = model_ctx->decrypt_file(update_path, &decrypted_size);
  if (decrypted_data == nullptr || decrypted_size == 0) {
    BMRT_LOG(WRONG, "Error: decrypt data failed");
    return false;
  }
  uint64_t header_size = 64;
  if (decrypted_size <= header_size) {
    free(decrypted_data);
    BMRT_LOG(WRONG, "Error: decrypt data too small");
    return false;
  }
  // 64 zeros to check
  for (uint64_t i = 0; i < header_size; ++i) {
    if (decrypted_data[i] != 0) {
      free(decrypted_data);
      BMRT_LOG(WRONG, "Error: decrypt data format error");
      return false;
    }
  }
  std::vector<uint8_t> file_data(decrypted_data + header_size, decrypted_data + decrypted_size);
  free(decrypted_data);

  long long start_position = 0;
  int cur_idx = 0;

  for (int cur_net_idx = 0; cur_net_idx < load_net_num && cur_idx <= (int)mem_idx.size(); cur_net_idx++) {
    if (std::find(net_idx.begin(), net_idx.end(), cur_net_idx) != net_idx.end() || net_idx.size() == 0) {
      ret = update_net_coeff(model_ctx, cur_net_idx, mem_idx[cur_idx], weight_idx[cur_idx], file_data, start_position);
      cur_idx++;
      if (!ret) {
        break;
      }
    }
  }

  if (start_position != decrypted_size - header_size) {
    BMRT_LOG(WRONG, "Error: start_position is not equal to decrypted_size");
    return false;
  }
  return ret;
}

bool Bmruntime::update_bmodel_coeff(
  ModelCtx* model_ctx,
  const std::vector<int> &net_idx,
  const std::vector<int> &mem_idx,
  const std::vector<std::vector<int>> &weight_idx,
  const std::vector<uint8_t> &file_data,
  long long &start_position)
{
  bool ret = true;
  u32 load_net_num = model_ctx->model()->net()->size();

  int cur_idx = 0;
  for (u32 cur_net_idx = 0; cur_net_idx < load_net_num; cur_net_idx++) {
    if (std::find(net_idx.begin(), net_idx.end(), cur_net_idx) != net_idx.end() || net_idx.size() == 0) {
      ret = update_net_coeff(model_ctx, cur_net_idx, mem_idx[cur_idx], weight_idx[cur_idx], file_data, start_position);
      cur_idx += 1;
      if (!ret) {
        break;
      }
    }
  }
  return ret;
}

static inline std::vector<int> parse_number(const std::string& input) {
  std::vector<int> result;
  std::stringstream ss(input);
  std::string item;
  while (std::getline(ss, item, ',')) {
      result.push_back(std::stoi(item));
  }
  return result;
}

bool Bmruntime::update_bmodel_weight_with_decrypt(
  const string& filepath, const string& update_path,
  const std::string& net_idx, const std::string& mem_idx,
  const std::vector<std::string>& weight_idx, decrypt_func f)
{
  BMRT_LOG(INFO, "Loading encrypted bmodel from [%s]. Thanks for your patience...", filepath.c_str());
  ModelCtx model_ctx(filepath, "", f);
  if (!model_ctx) {
    BMRT_LOG(WRONG, "Load encrypted model failed.");
    return false;
  }
  std::lock_guard<std::mutex> guard(m_load_mutex);

  auto net_idx_vec = parse_number(net_idx);
  auto mem_idx_vec = parse_number(mem_idx);

  std::vector<std::vector<int>> weight_idx_vec;
  for (size_t i = 0; i < weight_idx.size(); ++i) {
    weight_idx_vec.push_back(parse_number(weight_idx[i]));
  }
  return update_bmodel_coeff(&model_ctx, update_path, net_idx_vec, mem_idx_vec, weight_idx_vec);
}

bool Bmruntime::empty_bmodel_weight_with_decrypt(
  const string& filepath,
  const std::string& net_idx, const std::string& mem_idx,
  const std::vector<std::string>& weight_idx, decrypt_func f)
{
  BMRT_LOG(INFO, "Loading encrypted bmodel from [%s]. Thanks for your patience...", filepath.c_str());
  ModelCtx model_ctx(filepath, "", f);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load encrypted model failed.");
      return false;
  }
  std::lock_guard<std::mutex> guard(m_load_mutex);

  auto net_idx_vec = parse_number(net_idx);
  auto mem_idx_vec = parse_number(mem_idx);

  // read binary and decrypt
  long long start_position = 0;
  std::vector<uint8_t> file_data;

  std::vector<std::vector<int>> weight_idx_vec;
  for (size_t i = 0; i < weight_idx.size(); ++i) {
    weight_idx_vec.push_back(parse_number(weight_idx[i]));
  }

  auto ret = update_bmodel_coeff(&model_ctx, net_idx_vec, mem_idx_vec, weight_idx_vec, file_data, start_position);
  return ret;
}

void Bmruntime::load_tpu_module(ModelCtx* model_ctx) {
  // if kernel_module does not exist in bmodel, use the default kernel in kernel_module.h
  // kernel in kernel_module.h should be update manually:
  // 1: replace lib/libbm1684x_kernel_module_*.so by the latest.
  // 2: remake tpu_runtime
  if (bmrt_arch_info::get_bmtpu_arch() != BM1684X && bmrt_arch_info::get_bmtpu_arch() != BM1688) {
    for (int i = 0; i < MAX_DEVICE_NUM; i++) {
      kernel_modules[i] = nullptr;
    }
    return;
  }

  const unsigned char* firmware_data = nullptr;
  bool using_inner_firmware = (getenv("BMRUNTIME_USING_INNER_FIRMWARE") != NULL);
  if(using_inner_firmware) {
    BMRT_LOG(INFO, "force loading firmare in runtime because BMRUNTIME_USING_INNER_FIRMWARE env is set");
  }
  size_t firmware_size = 0;
  #ifdef __linux__
  if (bmrt_arch_info::get_bmtpu_arch() == BM1684X){
    firmware_data = kernel_module_data_1684x;
    firmware_size = sizeof(kernel_module_data_1684x);
  } else if (bmrt_arch_info::get_bmtpu_arch() == BM1688){
    firmware_data = kernel_module_data_tpulv60;
    firmware_size = sizeof(kernel_module_data_tpulv60);
  }
  #endif

  vector<unsigned char> external_firmware;
  const char* kernel_path = getenv("BMRUNTIME_USING_FIRMWARE");
  if(!using_inner_firmware && kernel_path){
    BMRT_LOG(INFO, "loading firmare from ENV BMRUNTIME_USING_FIRMWARE=%s", kernel_path);
    string real_kernel_path = kernel_path;
    FILE* kernel_file = fopen(real_kernel_path.c_str(), "rb");
    if(!kernel_file) {
      BMRT_LOG(WARNING, "cannot open firmware file: %s", real_kernel_path.c_str());
    } else {
      fseek(kernel_file, 0, SEEK_END);
      size_t file_size = ftell(kernel_file);
      external_firmware.resize(file_size);
      fseek(kernel_file, 0, SEEK_SET);
      size_t read_size = fread(external_firmware.data(), file_size, 1, kernel_file);
      fclose(kernel_file);
      if (read_size==0) {
        BMRT_LOG(WARNING, "cannot reading firmware file error: %s, read_size=%d, file_size=%d", real_kernel_path.c_str(), read_size, file_size);
        external_firmware.clear();
      }
    }
  }

  // load from bmodel, all cores should use the same firmware
  auto _kernel_module = model_ctx->model()->kernel_module();
  if (!using_inner_firmware && _kernel_module && external_firmware.empty()) {
    auto module_binary = _kernel_module->binary();
    if (module_binary->size()) {
      external_firmware.resize(module_binary->size());
      model_ctx->read_binary(module_binary, (uint8_t*)external_firmware.data());
      BMRT_LOG(INFO, "loading firmare in bmodel");
    }
  }

  if(!external_firmware.empty()) {
    firmware_data = external_firmware.data();
    firmware_size = external_firmware.size();
  } else if (firmware_data) {
    BMRT_LOG(INFO, "loading default firmare in runtime");
  } else {
    BMRT_LOG(WARNING, "No firmare loaded in runtime");
  }

  for (int i = 0; i < m_device_num; i++) {
    kernel_modules[i] = std::make_shared<KernelModule>(m_handles[i]);
    for (size_t core_id = 0; core_id < m_core_num; core_id++) {
      kernel_modules[i]->add_core_module(core_id, firmware_data, firmware_size);
    }
  }
}

#ifdef __linux__
const std::array<unsigned char, 4> ELF_MAGIC = {0x7f, 'E', 'L', 'F'};

struct Elf64_Ehdr {
  unsigned char e_ident[16]; // ELF magic number
  uint16_t e_type;           // object file type
  uint16_t e_machine;        // arch
};

bool check_so_architecture(const char* so_path) {
  const std::map<uint16_t, std::string> elf_arch_map = {
    {40, "arm"},
    {62, "x86_64"},
    {183, "aarch64"},
    {243, "risc-v"}
  };
  std::ifstream file(so_path, std::ios::in | std::ios::binary);
  if (!file) {
    BMRT_LOG(INFO, "Cannot open cpuop.so file in bmodel");
    return false;
  }
  Elf64_Ehdr header;
  file.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!std::equal(std::begin(ELF_MAGIC), std::end(ELF_MAGIC), std::begin(header.e_ident))) {
    std::cerr << "cpuop.so in bmodel is not an ELF file.\n";
    return false;
  }
  std::string host_arch;
  std::string so_arch;
  bool same_arch = false;
#ifdef __x86_64__
  host_arch = "x86_64";
  same_arch = header.e_machine == 62;
#elif defined(__aarch64__)
  host_arch = "aarch64";
  same_arch = header.e_machine == 183;
#elif defined(__arm__)
  host_arch = "arm";
  same_arch = header.e_machine == 40;
#elif defined(__riscv)
  host_arch = "risc-v";
  same_arch = header.e_machine == 243;
#else
  same_arch = false;
#endif
  if (!same_arch) {
    auto it = elf_arch_map.find(header.e_machine);
    if (it != elf_arch_map.end())
      so_arch = it->second;
    else
      so_arch = "UNKOWN ARCH";
    BMRT_LOG(WARNING, "custom cpuop's arch expect %s but got %s",
             host_arch.c_str(), so_arch.c_str());
  }
  return same_arch;
}
#endif

void Bmruntime::load_cpu_module(ModelCtx* model_ctx) {
  #ifdef __linux__
  vector<char> external_cpuop;
  auto _cpuop_module = model_ctx->model()->cpuop_module();
  if (_cpuop_module && external_cpuop.empty()) {
    auto module_binary = _cpuop_module->binary();
    if (module_binary->size()) {
      external_cpuop.resize(module_binary->size());
      model_ctx->read_binary(module_binary, (uint8_t*)external_cpuop.data());
      BMRT_LOG(INFO, "loading cpuop in bmodel");
      int fd = mkstemp(&temp_filename_[0]);
      fchmod(fd, S_IRUSR | S_IWUSR);
      ssize_t module_size = module_binary->size();
      ssize_t write_size = write(fd, external_cpuop.data(), module_size);
      if(write_size!=module_size) {
          BMRT_LOG(FATAL, "loadding cpuop failed: write_size=%d, module_size=%d", (int)write_size, (int)module_size);
      }
      close(fd);

      int fd_ = open(&temp_filename_[0], O_RDONLY, 0);
      if (fd_ < 0) {
        BMRT_LOG(INFO, "creating tmp so false");
        return;
      }
      if (!check_so_architecture(&temp_filename_[0])) {
        BMRT_LOG(WRONG, "Arch of custom cpuop.so is not the same with host! \n"
                        "Try to remake custom cpuop.so in the same arch with host cpu,\n"
                        "and embed into *.bmodel with 'tpu_model' tool");
      }

      tmpcpuso_handle_ = bmrt_load_lib(&temp_filename_[0], RTLD_LAZY);
      if (!tmpcpuso_handle_) {
        BMRT_LOG(WRONG, "load lib failed: %s\n", bmrt_lib_error());
        exit(EXIT_FAILURE);
      } else {
        customcpu_init_ = (t_bmcpu_init)bmrt_find_sym(tmpcpuso_handle_, "bmcpu_init");
        customcpu_uninit_ = (t_bmcpu_uninit)bmrt_find_sym(tmpcpuso_handle_, "bmcpu_uninit");
        customcpu_process_ = (t_bmcpu_process)bmrt_find_sym(tmpcpuso_handle_, "customcpu_process");
      }
      if (!customcpu_init_ || !customcpu_uninit_ || !customcpu_process_) {
        BMRT_LOG(WRONG, "read custom cpuop's symbol failed: %s\n", bmrt_lib_error());
        bmrt_unload_lib(tmpcpuso_handle_);
        exit(EXIT_FAILURE);
      }
      customcpu_handle_ = customcpu_init_();
      BMRT_LOG(INFO, "cpuop module is loaded, arch of custom cpuop is the same with host.");
    }
  } else {
    BMRT_LOG(INFO, "No cpu_module in bmodel.");
  }
  #endif
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

/* Load bmodel file, which is pre-compiled by bmcompiler */
bool Bmruntime::load_bmodel_with_mem(const string& filepath, mem_info_t* mem_info)
{
  BMRT_LOG(INFO, "Loading bmodel from [%s]. Thanks for your patience...", filepath.c_str());
  ModelCtx model_ctx(filepath);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load model failed.");
      return false;
  }
  set_device_mem_info(&model_ctx, mem_info);
  std::lock_guard<std::mutex> guard(m_load_mutex);
  return load_bmodel(&model_ctx);
}

/* Load bmodel file, which is pre-compiled by bmcompiler */
bool Bmruntime::load_bmodel_with_mem(const void* bmodel_data, size_t size, mem_info_t* mem_info)
{
  BMRT_LOG(INFO, "Loading bmodel data. Thanks for your patience...");
  ModelCtx model_ctx(bmodel_data, size);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load model failed.");
      return false;
  }
  set_device_mem_info(&model_ctx, mem_info);
  std::lock_guard<std::mutex> guard(m_load_mutex);
  return load_bmodel(&model_ctx);
}

/* Load encrypted bmodel file, which is pre-compiled by bmcompiler */
bool Bmruntime::load_bmodel_with_decrypt(const string& filepath, const std::string &decrypt_lib)
{
  BMRT_LOG(INFO, "Loading encrypted bmodel from [%s]. Thanks for your patience...", filepath.c_str());
  ModelCtx model_ctx(filepath, decrypt_lib);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load encrypted model failed.");
      return false;
  }
  std::lock_guard<std::mutex> guard(m_load_mutex);
  return load_bmodel(&model_ctx);
}

bool Bmruntime::load_bmodel_with_decrypt(const string& filepath, decrypt_func f)
{
  BMRT_LOG(INFO, "Loading encrypted bmodel from [%s]. Thanks for your patience...", filepath.c_str());
  ModelCtx model_ctx(filepath, "", f);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load encrypted model failed.");
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

void Bmruntime::fill_dmem_info(int64_t addr, uint64_t size, const std::string &desc) {
  if (addr != -1 || size == 0) {
    device_mem_info_t dmem;
    dmem.addr = addr;
    dmem.desc = desc;
    dmem.offset = 0;
    dmem.size = size;
    dmem_info.push_back(dmem);
  }
}

void Bmruntime::set_device_mem_info(ModelCtx* model_ctx, mem_info_t* mem_info)
{
  // init basic info
  alloc_mem = false;
  bmodel::bmodel_mem_info_t bmem_info = model_ctx->get_bmodel_mem_info();
  dmem_info.clear();
  if (mem_info->instruction_mem.addr != -1) {
    // instruction_mem: bdc_cmd + hau_cmd + dynamic_ir
    BMRT_ASSERT(mem_info->instruction_mem.number == 1);
    uint64_t offset = 0;
    BMRT_ASSERT_INFO(offset + bmem_info.bd_cmd_mem_size <= mem_info->instruction_mem.size, "");
    fill_dmem_info(mem_info->instruction_mem.addr, bmem_info.bd_cmd_mem_size, "bd_cmd_mem");
    offset = ALIGN(bmem_info.bd_cmd_mem_size, 128);
    BMRT_ASSERT_INFO(offset + bmem_info.hau_cmd_mem_size <= mem_info->instruction_mem.size, "");
    fill_dmem_info(mem_info->instruction_mem.addr + offset, bmem_info.hau_cmd_mem_size, "hau_cmd_mem");
    offset = ALIGN(offset + bmem_info.hau_cmd_mem_size, 128);
    BMRT_ASSERT_INFO(offset + bmem_info.hau_cmd_mem_size <= mem_info->instruction_mem.size, "");
    fill_dmem_info(mem_info->instruction_mem.addr + offset, bmem_info.dynamic_ir_mem_size, "dynamic_ir");
  }
  if (mem_info->variable_instruction_mem.addr != -1) {
    // variable_instruction_mem: gdma_cmd + sdma_cmd
    // for (int i = 0; i < mem_info->variable_instruction_mem.number; ++i) {
      std::string suffix = "";
      // if (i > 0) {
      //   suffix = "_" + std::to_string(i);
      // }
      uint64_t offset = 0;
      int64_t base_addr = mem_info->variable_instruction_mem.addr;
      BMRT_ASSERT_INFO(offset + bmem_info.gdma_cmd_mem_size <= mem_info->variable_instruction_mem.size, "");
      fill_dmem_info(base_addr, bmem_info.gdma_cmd_mem_size, "gdma_cmd_mem" + suffix);
      offset += ALIGN(bmem_info.gdma_cmd_mem_size, 128);
      BMRT_ASSERT_INFO(offset + bmem_info.sdma_cmd_mem_size <= mem_info->variable_instruction_mem.size, "");
      fill_dmem_info(base_addr + offset, bmem_info.sdma_cmd_mem_size, "sdma_cmd_mem" + suffix);
    // }
  }
  // coeff_mem: coeff
  if (mem_info->coeff_mem.addr != -1 || mem_info->coeff_mem.size == 0) {
    BMRT_ASSERT(mem_info->instruction_mem.number == 1);
    BMRT_ASSERT_INFO(bmem_info.coeff_mem_size <= mem_info->coeff_mem.size, "");
    fill_dmem_info(mem_info->coeff_mem.addr, bmem_info.coeff_mem_size, "coeff_mem");
  }
  if (mem_info->neuron_mem.addr != -1 || mem_info->neuron_mem.size == 0) {
    // neuron_mem: neuron + middle_buffer_size + dynamic_output + io_mem
    // for (int i = 0; i < mem_info->neuron_mem.number; ++i) {
      std::string suffix = "";
      // if (i > 0) {
      //   suffix = "_" + std::to_string(i);
      // }
      uint64_t offset = 0;
      int64_t base_addr = mem_info->neuron_mem.addr;
      BMRT_ASSERT_INFO(offset + bmem_info.neuron_mem_size <= mem_info->neuron_mem.size, "");
      fill_dmem_info(base_addr, bmem_info.neuron_mem_size, "neuron_mem" + suffix);
      offset += ALIGN(bmem_info.neuron_mem_size, 128);
      BMRT_ASSERT_INFO(offset + bmem_info.middle_buffer_size <= mem_info->neuron_mem.size, "");
      fill_dmem_info(base_addr + offset, bmem_info.middle_buffer_size, "middle_buffer" + suffix);
      offset += ALIGN(bmem_info.middle_buffer_size, 128);
      int64_t dynamic_out_size = bmem_info.dynamic_output_number * sizeof(bm_shape_ex_t);
      BMRT_ASSERT_INFO(offset + dynamic_out_size <= mem_info->neuron_mem.size, "");
      fill_dmem_info(base_addr + offset, dynamic_out_size, "dynamic_out" + suffix);
    // }
  }
  uint64_t io_mem_size = 0;
  for (int net_idx = 0; net_idx < model_ctx->model()->net()->size(); ++net_idx) {
    auto net = model_ctx->model()->net()->Get(net_idx);
    auto net_params = net->parameter();
    if (net->addr_mode() == ADDR_MODE_IO_ALONE) {
      for (int stage_idx = 0; stage_idx < net_params->size(); ++stage_idx) {
        auto param = net_params->Get(stage_idx);
        io_mem_size += ALIGN(param->io_size(), 128);
      }
    }
  }
  if (mem_info->io_mem.addr != -1) {
      std::string suffix = "";
      BMRT_ASSERT_INFO(io_mem_size <= mem_info->io_mem.size, "");
      fill_dmem_info(mem_info->io_mem.addr, io_mem_size, "io_mem" + suffix);
  }
}

// BmCoeff should not use the handle outside which may be released ahead in some cases.
BmCoeff::BmCoeff(int devid)
{
  if(!m_handle){
    bm_status_t status = bm_dev_request(&m_handle, devid);
    if (BM_SUCCESS != status) {
      BMRT_LOG(FATAL, "bm_dev_request failed:[%d]", status);
    }
    m_devid = devid;
  }
}

BmCoeff::~BmCoeff()
{
    for(auto &mem: m_mem_need_free) {
      BMRT_LOG_RUN(DEBUG, {
        u64 mem_addr = bm_mem_get_device_addr_u64(mem);
        u64 mem_size = bm_mem_get_device_size_u64(mem);
        BMRT_LOG(DEBUG, "free mem : [0x%llx, 0x%llx), size=%lld[0x%x]", mem_addr, mem_addr+mem_size, mem_size, mem_size);
      });
      bm_free_device_u64(m_handle, mem);
    }
    bm_dev_free(m_handle);
}

u64 BmCoeff::Register(ModelCtx* model_ctx, const CoeffMem* coeff_mem) {
  return Register(model_ctx, coeff_mem, -1);
}

int64_t BmCoeff::GetCoeffAddr(const CoeffMem* coeff_mem) {
  if (coeff_mem == NULL) {
    return 0;
  }
  u64 coeff_size = coeff_mem->binary_coeff()->size();
  u8* coeff_size_ptr = (u8*)&coeff_size;
  vector<u8> check_code = {coeff_mem->check_code()->begin(), coeff_mem->check_code()->end()};
  check_code.insert(check_code.end(), coeff_size_ptr, coeff_size_ptr + sizeof(u64));
  std::lock_guard<std::mutex> guard(m_coeff_mutex);
  auto iter = m_coeff_map.find(check_code);
  if (iter != m_coeff_map.end()) {
    return bm_mem_get_device_addr_u64(iter->second);
  }
  return -1;
}

u64 BmCoeff::Register(ModelCtx* model_ctx, const CoeffMem* coeff_mem, int64_t addr) // return coeff offset for cmd
{
  if (coeff_mem == NULL || model_ctx == NULL) {
    return 0;
  }
  u64 coeff_start = coeff_mem->address();
  // Use relative address since 1688.
  coeff_start &= bmrt_arch_info::addr_mask();
  u64 coeff_size = coeff_mem->encrypt_mode() == 0
                       ? coeff_mem->binary_coeff()->size()
                       : coeff_mem->decrypt_size();
  u8* coeff_size_ptr = (u8*)&coeff_size;

  // check whether the same
  vector<u8> check_code = {coeff_mem->check_code()->begin(), coeff_mem->check_code()->end()};
  check_code.insert(check_code.end(), coeff_size_ptr, coeff_size_ptr + sizeof(u64));
  std::lock_guard<std::mutex> guard(m_coeff_mutex);
  auto iter = m_coeff_map.find(check_code);
  if (iter != m_coeff_map.end()) {
    BMRT_LOG(DEBUG, "the coeff already exists");
    return bm_mem_get_device_addr_u64(iter->second) - coeff_start;
  }

  bm_device_mem_u64_t pmem;
  if (addr == -1) {
    // allocate device memory for coeff
    if (BM_SUCCESS != bm_malloc_device_byte_heap_mask_u64(m_handle, &pmem, 7, coeff_size)) {
      BMRT_LOG(FATAL, "coeff alloc failed, size[0x%llx]", coeff_size);
    }
    m_mem_need_free.push_back(pmem);
  } else {
    bm_set_device_mem_u64(&pmem, coeff_size, addr);
  }
  BMRT_LOG_RUN(DEBUG, {
    u64 mem_addr = bm_mem_get_device_addr_u64(pmem);
    u64 mem_size = bm_mem_get_device_size_u64(pmem);
    if (addr != -1) {
      BMRT_LOG(DEBUG, "using prealloc coeff mem: [0x%llx, 0x%llx), size=%lld[0x%x]", mem_addr, mem_addr+mem_size, mem_size, mem_size);
    } else {
      BMRT_LOG(DEBUG, "alloc mem : [0x%llx, 0x%llx), size=%lld[0x%x]", mem_addr, mem_addr+mem_size, mem_size, mem_size);
    }
  });

  m_latest_device_mem = pmem;
  upload_coeff_data(model_ctx, coeff_mem, m_handle, pmem);
  m_coeff_map.insert(std::pair<vector<u8>, bm_device_mem_u64_t>(check_code, pmem));
  return bm_mem_get_device_addr_u64(pmem) - coeff_start;
}

u64 BmCoeff::Update(ModelCtx* model_ctx,
                    const CoeffMem* coeff_mem,
                    int mem_idx,
                    const std::vector<int> &weight_idx,
                    const std::vector<uint8_t> &file_data,
                    long long &start_position) {
  if (coeff_mem == NULL || model_ctx == NULL) {
    return 0;
  }
  // u64 coeff_start = coeff_mem->address();
  // // Use relative address since 1688.
  // coeff_start &= bmrt_arch_info::addr_mask();
  // u64 coeff_size = coeff_mem->encrypt_mode() == 0
  //                      ? coeff_mem->binary_coeff()->size()
  //                      : coeff_mem->decrypt_size();

  // check whether the same
  std::lock_guard<std::mutex> guard(m_coeff_mutex);

  auto dev_mem = m_mem_need_free[mem_idx];
  update_coeff_data(model_ctx, coeff_mem, dev_mem, weight_idx, file_data, start_position, m_handle);
  return 0;
}

int BmCoeff::Check()
{
  int err_count = 0;
  uint8_t crc32[bmodel::SHA256_LEN];
  std::lock_guard<std::mutex> guard(m_coeff_mutex);
  for (auto& coeff : m_coeff_map) {
    auto &sha = coeff.first;
    auto &mem = coeff.second;
    u64 size = bm_mem_get_device_size_u64(mem);
    if(size > 0x40000000) {
      fprintf(stderr, "Coeff size[0x%llx] is greater than 1GB, ignore the SHA check\n", size);
      continue;
    }
    uint8_t* buffer = new uint8_t[size];
    auto buffer_sp = SP(buffer, uint8_t);
    bm_status_t status = bm_memcpy_d2s_u64(m_handle, buffer, coeff.second);
    CHECK_status(status);
    bmodel::CalcSha256(buffer, size, crc32);
    u64 addr = bm_mem_get_device_addr_u64(mem);
    fprintf(stderr, "Coeff, chip[%d], SHA[%02X%02X%02X%02X], addr[0x%llx], size[0x%x]",
            m_devid, sha[0], sha[1], sha[2], sha[3], addr, (u32)size);
    if (0 != memcmp(crc32, coeff.first.data(), bmodel::SHA256_LEN)) {
      fprintf(stderr, ", Check:**FAILED**\n");
      err_count++;
    } else {
      fprintf(stderr, "\n");
    }
  }
  return err_count;
}

void BmMemory::Init(const string &description, bm_handle_t handle,
                    const bm_device_mem_t &mem, void *buffer, bool do_check_) {
  desc = description;
  bm_handle = handle;
  device_mem = mem;
  addr = bm_mem_get_device_addr(mem);
  bytes = bm_mem_get_device_size(mem);
  dword_len = (bytes + 3) / 4;
  do_check = do_check_;
  if (do_check) {
    bmodel::CalcSha256((uint8_t *)buffer, bytes, check_code);
  }
  bm_status_t status = bm_memcpy_s2d(handle, mem, buffer);
  CHECK_status(status);
}

void BmMemory::Update(const bm_device_mem_t &mem, void *buffer) {
  addr = bm_mem_get_device_addr(mem);
  bytes = bm_mem_get_device_size(mem);
  dword_len = (bytes + 3) / 4;
  if (do_check) {
    bmodel::CalcSha256((uint8_t *)buffer, bytes, check_code);
  }
  bm_status_t status = bm_memcpy_s2d(bm_handle, mem, buffer);
  CHECK_status(status);
}

int BmMemory::Check()
{
  if ((!do_check) || desc.empty()) {
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

int BmMemory::GetData(void* buffer) {
  bm_status_t status = bm_memcpy_d2s(bm_handle, buffer, device_mem);
  CHECK_status(status);
  int err_count = 0;
  if (do_check && (!desc.empty())) {
    uint8_t crc32[bmodel::SHA256_LEN];
    bmodel::CalcSha256((uint8_t *)buffer, bytes, crc32);
    if (0 != memcmp(crc32, check_code, bmodel::SHA256_LEN)) {
      fprintf(stderr, ", Check: **FAILED**\n");
      err_count++;
    }
  }
  return err_count;
}

void KernelModule::add_core_module(int core_id, const char* filename) {
  BMRT_ASSERT_INFO(_kernel_modules.count(core_id)==0, "the core module has been already added, core_id=%d", core_id);
  _kernel_modules[core_id] = tpu_kernel_load_module_file(m_handle, filename);
  preload_funcs(core_id);
}
void KernelModule::add_core_module(int core_id, const unsigned char* binary, size_t size) {
  BMRT_ASSERT_INFO(_kernel_modules.count(core_id)==0, "the core module has been already added, core_id=%d", core_id);
  _kernel_modules[core_id] = tpu_kernel_load_module_to_core(m_handle, (char*)binary, size, core_id);
  preload_funcs(core_id);
}

KernelModule::~KernelModule() {
  for (auto& item: _kernel_modules) {
    auto module = item.second;
    auto core_id = item.first;
    auto status = tpu_kernel_unload_module_from_core(m_handle, module, core_id);
    BMRT_ASSERT_INFO(status == BM_SUCCESS, "kernel_module unload failed!! core_id=%d\n", core_id);
  }
}
void KernelModule::preload_funcs(int core_id) {
  BMRT_ASSERT(_kernel_modules[core_id]);

  auto _kernel_module = _kernel_modules[core_id];
  _multi_fullnet_func_id[core_id] = tpu_kernel_get_function_from_core(m_handle, _kernel_module, "sg_api_multi_fullnet", core_id);
  BMRT_LOG(INFO, " core_id=%d, multi_fullnet_func_id=%d", core_id, _multi_fullnet_func_id[core_id]);

  _dynamic_fullnet_func_id[core_id] = tpu_kernel_get_function_from_core(m_handle, _kernel_module, "sg_api_dynamic_fullnet", core_id);
  BMRT_LOG(INFO, " core_id=%d, dynamic_fullnet_func_id=%d", core_id, _dynamic_fullnet_func_id[core_id]);

  _enable_profile_func_id[core_id] = tpu_kernel_get_function_from_core(m_handle, _kernel_module, "sg_api_set_profile", core_id);
  _get_profile_func_id[core_id] = tpu_kernel_get_function_from_core(m_handle, _kernel_module, "sg_api_get_profile_data", core_id);

  if(bmrt_arch_info::get_bmtpu_arch() == BM1688 || bmrt_arch_info::get_bmtpu_arch() == SG2380){
    _set_engine_profile_param_func_id[core_id] = tpu_kernel_get_function_from_core(m_handle, _kernel_module, "sg_api_set_engine_profile_param", core_id);
  }
}

static inline vector<tpu_kernel_function_t> __map_to_vector_funcs(const vector<int>& core_list, const map<int, tpu_kernel_function_t>& func_map) {
  vector<tpu_kernel_function_t> func_ids(core_list.size());
  for(size_t i=0; i<core_list.size(); i++){
    func_ids[i] = func_map.at(core_list[i]);
  }
  return func_ids;
}

vector<tpu_kernel_function_t> KernelModule::__get_vector_funcs(const vector<int>& core_list, map<int, tpu_kernel_function_t>& func_map, const char* name) {
  for(auto core_id: core_list){
      if(!func_map.count(core_id)){
        func_map[core_id] = tpu_kernel_get_function_from_core(m_handle, _kernel_modules.at(core_id), name, core_id);
        BMRT_LOG(INFO, " core_id=%d, %s_func_id=%d", core_id, name, func_map[core_id]);
      }
  }
  return __map_to_vector_funcs(core_list, func_map);
}
vector<tpu_kernel_function_t> KernelModule::get_multi_fullnet_func_id(const vector<int>& core_list) {
  return __map_to_vector_funcs(core_list, _multi_fullnet_func_id);
}
vector<tpu_kernel_function_t> KernelModule::get_dynamic_fullnet_func_id(const vector<int>& core_list) {
  return __map_to_vector_funcs(core_list, _dynamic_fullnet_func_id);
}
vector<tpu_kernel_function_t> KernelModule::get_enable_profile_func_id(const vector<int>& core_list) {
  return __map_to_vector_funcs(core_list, _enable_profile_func_id);
}
vector<tpu_kernel_function_t> KernelModule::get_get_profile_func_id(const vector<int>& core_list) {
  return __map_to_vector_funcs(core_list, _get_profile_func_id);
}
vector<tpu_kernel_function_t> KernelModule::get_set_engine_profile_param_func_id(const vector<int>& core_list) {
  return __map_to_vector_funcs(core_list, _set_engine_profile_param_func_id);
}

vector<tpu_kernel_function_t> KernelModule::get_global_move_1684x_func_id(const vector<int>& core_list) {
  return __get_vector_funcs(core_list, _global_move_1684x_func_id, "global_move_1684x");
}

}  // namespace bmruntime
