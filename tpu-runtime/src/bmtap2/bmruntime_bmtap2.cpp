#include <bmruntime_inner.h>
#include <bmruntime.hpp>
#include <iostream>
#include <mutex>
#include <sstream>

using namespace std;

extern bmerr_t bmnet_get_output_max_size(bmnet_t net, size_t *output_max_size);
extern bmerr_t bmnet_create_blobs(bmnet_t net, uint8_t *buffer,
                                  map<string, map<string, bmruntime::Blob>> &blobs);

static inline int bitsize_of_fmt(uint32_t fmt)
{
  switch (fmt) {
    case FMT_F32:
    case FMT_I32:
      return 32;
    case FMT_F16:
    case FMT_I16:
    case FMT_U16:
      return 16;
    case FMT_I8:
    case FMT_U8:
      return 8;
    case FMT_I4:
      return 4;
    case FMT_I2:
      return 2;
    case FMT_I1:
      return 1;
    default:
      BMRT_LOG(FATAL, "FMT not supported: fmt = [%d]",  fmt);
  }
  return 0;
}

namespace bmruntime {

// class BmCtx
class BmCtx {
 public:
  BmCtx()
  {
    bmerr_t ret = bm_init(0, &ctx_);
    if (ret != BM_SUCCESS) {
      BMRT_LOG(WRONG, "bm_init failed: ret = %d", ret);
    }
  }

  ~BmCtx()
  {
    bm_exit(ctx_);
  }

  static shared_ptr<BmCtx> getInstance()
  {
    if (instance_ == nullptr) {
      std::lock_guard<std::mutex> guard(mtx_);
      if (instance_ == nullptr) {
        instance_ = make_shared<BmCtx>();
      }
    }

    return instance_;
  }

  bmctx_t getCtx()
  {
    return ctx_;
  }

 private:
  static shared_ptr<BmCtx> instance_;
  static mutex mtx_;
  bmctx_t ctx_;
};  // class BmCtx

shared_ptr<BmCtx> BmCtx::instance_ = nullptr;
mutex BmCtx::mtx_;

// class Blob
Blob::Blob() : data_(nullptr), shape_({0}), fmt_(0), count_(0) {}

Blob::Blob(void *data, shape_t &shape, uint32_t fmt) : data_(data), shape_(shape), fmt_(fmt)
{
  count_ = shape.n * shape.c * shape.h * shape.w;
  size_ = count_ * bitsize_of_fmt(fmt) / 8;
}

Blob::~Blob()
{
  data_ = nullptr;
  shape_ = {0};
  fmt_ = 0;
  count_ = 0;
}

void *Blob::data() const
{
  return data_;
}

const shape_t &Blob::shape()
{
  return shape_;
}

uint32_t Blob::fmt() const
{
  return fmt_;
}

size_t Blob::count() const
{
  return count_;
}

size_t Blob::size() const
{
  return size_;
}

void Blob::dump() const
{
  cout << "data = " << std::hex << data_ << endl;
  cout << "shape = [" << std::dec << shape_.n << " " << shape_.c << " " << shape_.h << " "
       << shape_.w << "]" << endl;
  cout << "fmt = " << fmt_ << endl;
  cout << "count = " << count_ << endl << endl;
  cout << "size = " << size_ << endl << endl;
}

static size_t get_output_max_size(bmnet_t net)
{
  size_t max_size = 0;
  for (uint32_t idx = 0; idx < net->model_info.command_num; idx++) {
    size_t size = net->model_info.output_info_array[idx].output_size;
    if (size > max_size) {
      max_size = size;
    }
  }
  return max_size;
}

static void create_blobs(bmnet_t net, uint8_t *buffer,
                         map<string, map<string, bmruntime::Blob>> &blobs)
{
  int num = net->model_info.command_num;

  for (int idx = 0; idx < num; idx++) {
    // input
    auto &inputs = net->model_info.input_info_array[idx];
    auto &outputs = net->model_info.output_info_array[idx];
    ostringstream shape;
    for (uint32_t input_idx = 0; input_idx < inputs.input_num; input_idx++) {
      shape << "[" << inputs.shape_array[input_idx].n << " " << inputs.shape_array[input_idx].c
            << " " << inputs.shape_array[input_idx].h << " " << inputs.shape_array[input_idx].w
            << "]";
    }

    // output
    size_t offset = 0;
    map<string, bmruntime::Blob> blob;
    for (uint32_t output_idx = 0; output_idx < outputs.output_num; output_idx++) {
      blob[outputs.name_array[output_idx]] = bmruntime::Blob(
          buffer + offset, outputs.shape_array[output_idx], outputs.fmt_array[output_idx]);
      offset += outputs.size_array[output_idx];
    }

    blobs[shape.str()] = blob;
  }
}

// class Net
Net::Net(const string &bmodel)
    : bmctx_instance_(nullptr), net_(nullptr), output_buffer_(nullptr), input_shape_("[]")
{
  bmerr_t ret = BM_SUCCESS;

  bmctx_instance_ = BmCtx::getInstance();

  // register bmodel
  ret = bmnet_register_bmodel(bmctx_instance_->getCtx(), bmodel.c_str(), &net_);
  if (ret != BM_SUCCESS) {
    BMRT_LOG(FATAL, "bmnet_register_bmodel failed: ret = [%d]",  ret);
  }

  // allocate output buffer
  size_t output_max_size = get_output_max_size(net_);

  output_buffer_ = new uint8_t[output_max_size];

  // feed blobs_
  create_blobs(net_, output_buffer_, blobs_);
}

Net::~Net()
{
  bmnet_cleanup(net_);

  delete[] output_buffer_;
  output_buffer_ = nullptr;

  bmctx_instance_.reset();

  if (BmCtx::getInstance().use_count() == 1) {
    BmCtx::getInstance().reset();
  }
}

int Net::forward(vector<Blob> &input_blobs)
{
  bmerr_t ret;

  if (net_ == nullptr) {
    BMRT_LOG(WRONG, "Net hasn't been constructed with bmodel file.");
    return -1;
  }

  if (input_blobs.size() == 0) {
    BMRT_LOG(WRONG, "input_blobs cannot be empty");
    return -2;
  }

  // set input
  ostringstream input_shape;
  vector<shape_t> shapes;
  for (auto &blob : input_blobs) {
    shape_t s = blob.shape();
    shapes.push_back(s);
    input_shape << "[" << s.n << " " << s.c << " " << s.h << " " << s.w << "]";
  }

  if (input_shape_ != input_shape.str()) {
    input_shape_ = input_shape.str();
    ret = bmnet_set_input_shape2(net_, reinterpret_cast<shape_t *>(shapes.data()), shapes.size());
    if (ret != BM_SUCCESS) {
      BMRT_LOG(WRONG, "bmnet_set_input_shape2 failed: ret = %d, shape = %s", ret, input_shape.str().c_str());
      return -3;
    }
  }

  // run inference
  // 1. copy input data
  uint64_t input_offset = 0;
  for (uint32_t idx = 0; idx < input_blobs.size(); idx++) {
    auto &blob = input_blobs[idx];
    ret = bm_memcpy_s2d_ex(bmctx_instance_->getCtx(), net_->input_tensors[idx].device_mem, blob.data(),
                           input_offset, blob.size());
    if (ret != BM_SUCCESS) {
      BMRT_LOG(WRONG, "load input data failed: ret = %d", ret);
      return -4;
    }
    input_offset += blob.size();
  }

  ret = bmnet_run(net_);
  if (ret != BM_SUCCESS) {
    BMRT_LOG(WRONG, "bmnet run failed: ret = %d", ret);
    return -5;
  }

  ret = bmnet_store_output(net_, output_buffer_);
  if (ret != BM_SUCCESS) {
    BMRT_LOG(WRONG, "bmnet store output failed: ret = %d", ret);
    return -6;
  }

  return 0;
}

Blob *Net::output(const string &output_name)
{
  if (net_ == nullptr) {
    BMRT_LOG(WRONG, "Net hasn't been constructed with bmodel file.");
    return nullptr;
  }

  // input shape --> map<output_name, Blob>
  auto it1 = blobs_.find(input_shape_);
  if (it1 == blobs_.end()) {
    BMRT_LOG(WRONG, "input shape: %s does not support.", input_shape_.c_str());
    return nullptr;
  }

  // output_name --> Blob
  map<string, Blob> &output_map = it1->second;
  auto it2 = output_map.find(output_name);
  if (it2 == output_map.end()) {
    BMRT_LOG(WRONG, "output name [%s] does not exist.", output_name.c_str());
    return nullptr;
  }

  return &it2->second;
}

const bmnet_model_info_t *Net::info()
{
  return &net_->model_info;
}

}  // namespace bmruntime
