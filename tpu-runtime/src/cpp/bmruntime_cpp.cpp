/*****************************************************************************
 *
 *    Copyright (c) 2016-2026 by Sophgo Technologies Inc. All rights reserved.
 *
 *    The material in this file is confidential and contains trade secrets
 *    of Sophgo Technologies Inc. This is proprietary information owned by
 *    Sophgo Technologies Inc. No part of this work may be disclosed,
 *    reproduced, copied, transmitted, or used in any way for any purpose,
 *    without the express written permission of Sophgo Technologies Inc.
 *
 *****************************************************************************/
#include "bmruntime_cpp.h"
#include "bmruntime.h"
#include "bmruntime_common.h"

using bmruntime::bmfunc;
using bmruntime::Bmruntime;

namespace bmruntime {

size_t ByteSize(bm_data_type_t type)
{
  switch (type) {
    case BM_FLOAT32:
    case BM_INT32:
    case BM_UINT32:
      return 4;
    case BM_FLOAT16:
    case BM_BFLOAT16:
    case BM_INT16:
    case BM_UINT16:
      return 2;
    case BM_INT8:
    case BM_UINT8:
      return 1;
    default:
      BMRT_LOG(FATAL, "Unknown data type: [%d]",  type);
  }
  return 0;
}

size_t ByteSize(const bm_device_mem_t &device_mem)
{
  return device_mem.size;
}

size_t ByteSize(const bm_tensor_t &tensor)
{
  return Count(tensor.shape) * ByteSize(tensor.dtype);
}

size_t ByteSize(const Tensor &tensor)
{
  return tensor.ByteSize();
}

uint64_t Count(const bm_shape_t &shape)
{
  BMRT_ASSERT_INFO(shape.num_dims <= BM_MAX_DIMS_NUM,"shape.num_dims:%d should less than BM_MAX_DIMS_NUM:8"\
    ,shape.num_dims);
  uint64_t count = 1;
  for (int i = 0; i < shape.num_dims; i++) {
    count *= shape.dims[i];
  }
  return count;
}

uint64_t Count(const bm_tensor_t &tensor)
{
  return Count(tensor.shape);
}

uint64_t Count(const Tensor &tensor)
{
  return tensor.num_elements();
}

bool IsSameShape(const bm_shape_t &left, const bm_shape_t &right)
{
  if (left.num_dims != right.num_dims) {
    return false;
  }
  BMRT_ASSERT_INFO(left.num_dims <= BM_MAX_DIMS_NUM,"left.num_dims:%d should less than BM_MAX_DIMS_NUM:8",left.num_dims);
  for (int i = 0; i < left.num_dims; i++) {
    if (left.dims[i] != right.dims[i]) {
      return false;
    }
  }
  return true;
}

Context::Context(bm_handle_t bm_handle)
{
  bm_handle_ = NULL;
  unsigned int chipid = 0;
  if (bm_handle == NULL) {
    BMRT_LOG(FATAL, "bm_handle shouldn't be NULL");
  }
  if (0 != bm_get_chipid(bm_handle, &chipid)) {
    BMRT_LOG(FATAL, "Cannot get chipid");
  }
  std::string chip_name;
  if (chipid == 0x1682) {
    chip_name = "BM1682";
  } else if (chipid == 0x1684) {
    chip_name = "BM1684";
  } else if (chipid == 0x1686) {
    chip_name = "BM1684X";
  } else {
    BMRT_LOG(FATAL, "Unknown chipid %x", chipid);
  }

  Bmruntime *p_bmrt = new Bmruntime(&bm_handle, true, chip_name);
  BMRT_ASSERT_INFO(p_bmrt != NULL,"p_bmrt shouldn't be NULL,chip_name: %s",chip_name.c_str());
  body_ = (void *)p_bmrt;
}

Context::Context(int devid)
{
  bm_handle_ = NULL;
  bm_status_t status = bm_dev_request(&bm_handle_, devid);
  if (BM_SUCCESS != status || bm_handle_ == NULL) {
    BMRT_LOG(FATAL, "dev[%d] request failed, status:%d", devid, status);
  }

  unsigned int chipid = 0;
  if (0 != bm_get_chipid(bm_handle_, &chipid)) {
    BMRT_LOG(FATAL, "Cannot get chipid");
  }
  std::string chip_name;
  if (chipid == 0x1682) {
    chip_name = "BM1682";
  } else if (chipid == 0x1684) {
    chip_name = "BM1684";
  } else if (chipid == 0x1686) {
    chip_name = "BM1684X";
  } else {
    BMRT_LOG(FATAL, "Unknown chipid %x", chipid);
  }

  Bmruntime *p_bmrt = new Bmruntime(&bm_handle_, true, chip_name);
  BMRT_ASSERT_INFO(p_bmrt != NULL,"p_bmrt shouldn't be NULL");
  body_ = (void *)p_bmrt;
}

Context::~Context()
{
  delete (Bmruntime *)body_;
  if (bm_handle_ != NULL) {
    bm_dev_free(bm_handle_);
  }
}

bm_handle_t Context::handle() const
{
  if (bm_handle_ != NULL) {
    return bm_handle_;
  }
  Bmruntime *p_bmrt = (Bmruntime *)body_;
  return p_bmrt->get_bm_handle();
}

// load bmodel
bm_status_t Context::load_bmodel(const void *bmodel_data, size_t size)
{
  Bmruntime *p_bmrt = (Bmruntime *)body_;
  bool ret = p_bmrt->load_bmodel(bmodel_data, size);
  return ret == true ? BM_SUCCESS : BM_ERR_FAILURE;
}

bm_status_t Context::load_bmodel(const char *bmodel_file)
{
  Bmruntime *p_bmrt = (Bmruntime *)body_;
  bool ret = p_bmrt->load_bmodel(bmodel_file);
  return ret == true ? BM_SUCCESS : BM_ERR_FAILURE;
}

// network num and name
int Context::get_network_number() const
{
  Bmruntime *p_bmrt = (Bmruntime *)body_;
  return p_bmrt->get_network_number();
}

void Context::get_network_names(std::vector<const char *> *names) const
{
  Bmruntime *p_bmrt = (Bmruntime *)body_;
  p_bmrt->get_network_names(names);
}

void Context::trace() const
{
  ((Bmruntime *)body_)->trace();
}

const bm_net_info_t *Context::get_network_info(const char *net_name) const
{
  Bmruntime *p_bmrt = (Bmruntime *)body_;
  return p_bmrt->get_net_info(p_bmrt->get_net_idx(net_name));
}

// Network --------------------------------------------------------------------------

Network::Network(const Context &ctx, const char *net_name, int stage_id)
{
  bm_status_t ret = BM_SUCCESS;
  ctx_ = &ctx;
  Bmruntime *p_bmrt = (Bmruntime *)ctx.body_;
  net_id_ = p_bmrt->get_net_idx(net_name);
  auto net_info = p_bmrt->get_net_info(net_id_);
  BMRT_ASSERT_INFO(net_info != NULL,"net_info shouldn't be NULL,net_id_: %d, net_:name: %s", net_id_, net_name);
  bool can_reshape = (stage_id == -1 ? true : false);
  int stage_index = (stage_id == -1 ? 0 : stage_id);
  size_t size = 0;
  BMRT_ASSERT_INFO(net_info->stage_num > stage_index && stage_index >= 0,\
    "net_info->stage_num:%d should larger than stage_index:%d and stage_index:%d shouldn't less than 0",\
    net_info->stage_num,stage_index,stage_index);
  input_tensors_ = new bm_tensor_t[net_info->input_num];
  output_tensors_ = new bm_tensor_t[net_info->output_num];
  for (int idx = 0; idx < net_info->input_num; idx++) {
    input_tensors_[idx].dtype = net_info->input_dtypes[idx];
    input_tensors_[idx].st_mode = BM_STORE_1N;
    input_tensors_[idx].shape = net_info->stages[stage_index].input_shapes[idx];
    // if need reshape, alloc the max mem; or alloc the shape mem
    if (can_reshape) {
      size = net_info->max_input_bytes[idx];
    } else {
      size = p_bmrt->size_4N_align(input_tensors_[idx].shape, input_tensors_[idx].dtype);
    }
    ret = bm_malloc_device_byte(ctx.handle(), &input_tensors_[idx].device_mem, size);
    if (BM_SUCCESS != ret ) {
      BMRT_LOG(FATAL, "input alloc failed, size[0x%lx], status:%d", static_cast<unsigned long>(size), ret);
    }
    Tensor *tensor = new Tensor(ctx.handle(), &input_tensors_[idx], can_reshape);
    inputs_.push_back(tensor);
  }
  for (int idx = 0; idx < net_info->output_num; idx++) {
    output_tensors_[idx].dtype = net_info->output_dtypes[idx];
    output_tensors_[idx].st_mode = BM_STORE_1N;
    output_tensors_[idx].shape = net_info->stages[stage_index].output_shapes[idx];
    // if need reshape, alloc the max mem; or alloc the shape mem
    if (can_reshape) {
      size = net_info->max_output_bytes[idx];
    } else {
      size = p_bmrt->size_4N_align(output_tensors_[idx].shape, output_tensors_[idx].dtype);
    }
    ret = bm_malloc_device_byte(ctx.handle(), &output_tensors_[idx].device_mem, size);
    if (BM_SUCCESS != ret ) {
      BMRT_LOG(FATAL, "output alloc failed, size[0x%lx], status:%d", static_cast<unsigned long>(size), ret);
    }
    Tensor *tensor = new Tensor(ctx.handle(), &output_tensors_[idx], can_reshape);
    outputs_.push_back(tensor);
  }
}

Network::~Network()
{
  for (auto tensor : inputs_) {
    delete tensor;
  }
  for (auto tensor : outputs_) {
    delete tensor;
  }
  delete[] input_tensors_;
  delete[] output_tensors_;
}

bm_status_t Network::Forward(bool sync) const
{
  Bmruntime *p_bmrt = (Bmruntime *)ctx_->body_;
  // check whether all inputs ready
  for (uint32_t idx = 0; idx < inputs_.size(); idx++) {
    auto &tensor = inputs_[idx];
    if (tensor->ready() == false) {
      BMRT_LOG(WRONG, "input tensors[%u] are not ready", idx);
      return BM_ERR_FAILURE;
    }
  }

  bool bRet = p_bmrt->launch(net_id_, input_tensors_, info()->input_num, output_tensors_,
                             info()->output_num, true, true);
  if (bRet == false) {
    BMRT_LOG(WRONG, "launch net[%s] failed", info()->name);
    return BM_ERR_FAILURE;
  }
  if (sync == true) {
    return bm_thread_sync(ctx_->handle());
  }
  return BM_SUCCESS;
}

const bm_net_info_t *Network::info() const
{
  Bmruntime *p_bmrt = (Bmruntime *)ctx_->body_;
  return p_bmrt->get_net_info(net_id_);
}

const std::vector<Tensor *> &Network::Inputs()
{
  return inputs_;
}

const std::vector<Tensor *> &Network::Outputs()
{
  return outputs_;
}

// get input and output tensor by tensor name
Tensor *Network::Input(const char *name)
{
  Bmruntime *p_bmrt = (Bmruntime *)ctx_->body_;
  int idx = p_bmrt->get_input_index(name, net_id_);
  if (idx == -1) {
    return NULL;
  }
  return inputs_[idx];
}

Tensor *Network::Output(const char *name)
{
  Bmruntime *p_bmrt = (Bmruntime *)ctx_->body_;
  int idx = p_bmrt->get_output_index(name, net_id_);
  if (idx == -1) {
    return NULL;
  }
  return outputs_[idx];
}

// Tensor --------------------------------------------------------------------------
// only create in class Network
Tensor::Tensor(bm_handle_t handle, bm_tensor_t *tensor, bool can_reshape)
{
  user_mem_ = false;
  tensor_ = tensor;
  handle_ = handle;
  can_reshape_ = can_reshape;
  have_reshaped_ = (can_reshape_ ? false : true);
}

Tensor::~Tensor()
{
  if (user_mem_ == false) {
    bm_free_device(handle_, tensor_->device_mem);
  }
}

bm_status_t Tensor::CopyTo(void *data) const
{
  return CopyTo(data, ByteSize());
}

bm_status_t Tensor::CopyTo(void *data, size_t size, uint64_t offset) const
{
  return bm_memcpy_d2s_partial_offset(handle_, data, tensor_->device_mem, size, offset);
}

// copy data from system to device
bm_status_t Tensor::CopyFrom(const void *data)
{
  return CopyFrom(data, ByteSize());
}

bm_status_t Tensor::CopyFrom(const void *data, size_t size, uint64_t offset)
{
  return bm_memcpy_s2d_partial_offset(handle_, tensor_->device_mem, (const_cast<void *>(data)),
                                      size, offset);
}

bm_status_t Tensor::Reshape(const bm_shape_t &shape)
{
  uint64_t count = Count(shape);
  if (count == 0) {
    BMRT_LOG(WRONG, "Reshape wrong parameter");
    return BM_ERR_PARAM;
  }
  if (can_reshape_ == false) {
    BMRT_LOG(WRONG, "net shape is fixed, cannot be reshaped");
    return BM_ERR_NOFEATURE;
  }
  if (count * bmruntime::ByteSize(tensor_->dtype) > tensor_->device_mem.size) {
    BMRT_LOG(WRONG, "new shape will make tensor larger than device memory size");
    return BM_ERR_NOMEM;
  }
  tensor_->shape = shape;
  if (have_reshaped_ == false) {
    have_reshaped_ = true;
  }
  return BM_SUCCESS;
}

// size in byte
size_t Tensor::ByteSize() const
{
  return bmruntime::ByteSize(*tensor_);
}

// number of elements
uint64_t Tensor::num_elements() const
{
  return Count(tensor_->shape);
}
// get origin bm_tensor_t
const bm_tensor_t *Tensor::tensor() const
{
  return tensor_;
}
// set tensor store mode, if not set, BM_STORE_1N as default.
void Tensor::set_store_mode(bm_store_mode_t mode) const
{
  tensor_->st_mode = mode;
}
// set device mem, update device mem by a new device mem
bm_status_t Tensor::set_device_mem(const bm_device_mem_t &device_mem)
{
  if (device_mem.size < ByteSize()) {
    BMRT_LOG(WRONG, "new device mem size less than tensor size");
    return BM_ERR_PARAM;
  }

  if (user_mem_ == false) {
    user_mem_ = true;
    bm_free_device(handle_, tensor_->device_mem);
  }
  tensor_->device_mem = device_mem;
  return BM_SUCCESS;
}

bool Tensor::ready()
{
  if (have_reshaped_ == false) {
    BMRT_LOG(WRONG, "You should reshape tensor by Reshape");
    return false;
  }
  return true;
}

}  // namespace bmruntime
