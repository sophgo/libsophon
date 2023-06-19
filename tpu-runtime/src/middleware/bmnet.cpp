#include <assert.h>
#include "bmnet.h"
#include "bmruntime.h"

using bmruntime::Bmruntime;
using bmruntime::bmfunc;

#define __HANDLE     (reinterpret_cast<Bmruntime *>(bmcc_ctx_)->get_bm_handle())
namespace bmcnn {

BMNet::BMNet(bmcnn_ctx_t bmcc_ctx, const std::string &name)
    : bmcc_ctx_(bmcc_ctx)
{
    if (bmcc_ctx_ == NULL) {
        BMRT_LOG(FATAL, "bmcnn context is null.");
        assert(0);
    }

    Bmruntime *bmrt_ = reinterpret_cast<Bmruntime *>(bmcc_ctx_);
    BMRT_LOG(INFO, "Loading net %s", name.c_str());
    net_idx_ = bmrt_->get_net_idx(name);
    net_info_ = bmrt_->get_net_info(net_idx_);
    assert(net_info_ != NULL);

    for (int idx = 0; idx < net_info_->input_num; idx++) {
        BMRT_LOG(INFO, "Input blob: %s", net_info_->input_names[idx]);
        auto bmshape = bmrt_->get_input_max_shape(net_idx_, idx);
        Shape shape;
        shape.dims = bmshape->num_dims;
        memcpy(shape.data, bmshape->dims, sizeof(int)*shape.dims);
        BMBlob *blob = new BMBlob(shape, net_info_->input_dtypes[idx], __HANDLE);
        net_input_blobs_.push_back(blob);
        blob_name_index_[net_info_->input_names[idx]] = blobs_.size();
        blobs_.push_back(std::shared_ptr<BMBlob>(blob));
        if (idx == 0) {
            max_shape_ = shape;
        }
    }

    for (int idx = 0; idx < net_info_->output_num; idx++) {
        BMRT_LOG(INFO, "Output blob: %s", net_info_->output_names[idx]);
        auto bmshape = bmrt_->get_output_max_shape(net_idx_, idx);
        Shape shape;
        shape.dims = bmshape->num_dims;
        memcpy(shape.data, bmshape->dims, sizeof(int)*shape.dims);
        BMBlob *blob = new BMBlob(shape, net_info_->output_dtypes[idx], __HANDLE);
        net_output_blobs_.push_back(blob);
        blob_name_index_[net_info_->output_names[idx]] = blobs_.size();
        blobs_.push_back(std::shared_ptr<BMBlob>(blob));
    }
}

BMNet::~BMNet()
{
}

const std::shared_ptr<BMBlob> BMNet::blob_by_name(const std::string &name) const
{
    std::shared_ptr<BMBlob> blob_ptr;
    if (blob_name_index_.find(name) != blob_name_index_.end())
        blob_ptr = blobs_[blob_name_index_.find(name)->second];
    else
        blob_ptr.reset((BMBlob *)NULL);
    return blob_ptr;
}

void BMNet::Reshape()
{
  Bmruntime *bmrt_ = reinterpret_cast<Bmruntime *>(bmcc_ctx_);
  const std::vector<std::string> &output_tensor_name = *bmrt_->get_output_tensor(net_idx_);
  std::vector<std::string>::const_iterator output_iter = output_tensor_name.begin();
  std::shared_ptr<BMBlob> input_blob =
      blob_by_name(*bmrt_->get_input_tensor(net_idx_)->begin());

  if (!bmrt_->can_batch_size_change(net_idx_) && input_blob->batch_num() % max_shape_.n != 0) {
    BMRT_LOG(FATAL, "Batch size is not allowed to be changed.");
    assert(0);
  }
  if (!bmrt_->can_height_and_width_change(net_idx_) && input_blob->height() != max_shape_.h) {
    BMRT_LOG(FATAL, "Height is not allowed to be changed.");
    assert(0);
  }
  int max_w = 1;
  for (int i = 3; i < max_shape_.dims; i++)
    max_w *= max_shape_.data[i];
  if (!bmrt_->can_height_and_width_change(net_idx_) && input_blob->width() != max_w) {
    BMRT_LOG(FATAL, "Width is not allowed to be changed.");
    assert(0);
  }
  // can not compare n, so begin from dim=1
  for (int i = 1; i < max_shape_.dims; i++) {
    if (input_blob->shape_size(i) > max_shape_.data[i]) {
      BMRT_LOG(FATAL, "Input tensor's dim[%d]=%d exceeded maximum size[%d].", i,
               input_blob->shape_size(i), max_shape_.data[i]);
      assert(0);
    }
  }
  while (output_iter != output_tensor_name.end()) {
    std::shared_ptr<BMBlob> output_blob = blob_by_name(*output_iter);
    Shape shape;
    shape.dims = bmrt_->get_output_blob_max_shape(*output_iter, net_idx_, shape.data);
    shape.data[0] = input_blob->batch_num();
    shape.data[1] = output_blob->channels();
    output_blob->Reshape(shape);
    ++output_iter;
  }
}

void BMNet::SyncShape()
{
  // do nothing
}

void BMNet::Forward(bool sync /* = false */)
{
   Bmruntime *bmrt_ = reinterpret_cast<Bmruntime *>(bmcc_ctx_);
    int batch_num = net_input_blobs_[0]->batch_num();
    int batch_start = 0;
    std::vector<bm_device_mem_t> input_tensors, output_tensors;
    std::vector<int> input_stmode, output_stmode;

    int input_num = net_input_blobs_.size();
    int output_num = net_output_blobs_.size();
    for (int i = 0; i < input_num; ++i) {
        input_tensors.push_back(*net_input_blobs_[i]->dev_mem());
        input_stmode.push_back(net_input_blobs_[i]->store_mode());
    }

    for (int i = 0; i < output_num; ++i) {
        output_tensors.push_back(*net_output_blobs_[i]->mutable_dev_mem());
        output_stmode.push_back(net_output_blobs_[i]->store_mode());
    }
    #ifdef __linux__
    bm_shape_t output_shapes[output_num];
    #else
    std::shared_ptr<bm_shape_t> output_shapes_(new bm_shape_t[output_num], std::default_delete<bm_shape_t[]>());
    bm_shape_t* output_shapes = output_shapes_.get();
    #endif

    int batch_work =  max_shape_.n;
    while (batch_num > 0) {
        std::vector<bm_device_mem_t> input_tensors_work;
        std::vector<bm_device_mem_t> output_tensors_work;

        for (int i = 0; i < input_num; ++i) {
            size_t offset = batch_start * net_input_blobs_[i]->batch_length() * bmrt_data_type_size(net_input_blobs_[i]->data_type());
            bm_device_mem_t mem = input_tensors[i];
            unsigned long long addr = bm_mem_get_device_addr(mem);
            addr += offset;
            bm_mem_set_device_addr(&mem, addr);
            input_tensors_work.push_back(mem);
        }

        for (int i = 0; i < output_num; ++i) {
            size_t offset = batch_start * net_output_blobs_[i]->batch_length() * bmrt_data_type_size(net_output_blobs_[i]->data_type());
            bm_device_mem_t mem = output_tensors[i];
            unsigned long long addr = bm_mem_get_device_addr(mem);
            addr += offset;
            bm_mem_set_device_addr(&mem, addr);
            output_tensors_work.push_back(mem);
        }

        int* in_stmode = input_stmode.data();
        int* out_stmode = output_stmode.data();
        #ifdef __linux__
        int input_shapes[BM_MAX_DIMS_NUM*input_num];
        int input_dims[input_num];
        #else
        std::shared_ptr<int> input_shapes_(new int[BM_MAX_DIMS_NUM*input_num], std::default_delete<int[]>());
        int *input_shapes = input_shapes_.get();
        std::shared_ptr<int> input_dims_(new int[input_num], std::default_delete<int[]>());
        int *input_dims = input_dims_.get();
        #endif
        int shape_offset = 0;
        for(int i=0; i<input_num; i++){
            auto s = net_input_blobs_[i]->shape();
            input_dims[i] = s.dims;
            memcpy(input_shapes + shape_offset, s.data, s.dims*sizeof(int));
            input_shapes[shape_offset] = batch_work;
            shape_offset += input_dims[i];
        }
        bool res = bmrt_->launch(net_idx_, input_num,
                                 input_tensors_work.data(),
                                 input_shapes, input_dims, in_stmode,
                                 output_tensors.size(),
                                 output_tensors_work.data(),
                                 out_stmode, output_shapes);

        if (!res)
            assert(0);
        batch_num -= batch_work;
        batch_start += batch_work;
    }
    for (int i = 0; i < output_num; ++i) {
      Shape shape;
      shape.dims = output_shapes[i].num_dims;
      memcpy(shape.data, output_shapes[i].dims, shape.dims*sizeof(int));
      net_output_blobs_[i]->SyncShape(shape);
    }
}

} /* namespace bmcnn */
