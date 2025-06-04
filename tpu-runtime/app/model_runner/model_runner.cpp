#ifndef __linux__
int main(int argc, char **argv) {
  // do nothing
  return 0;
}
#else
#include "bmlib_runtime.h"
#include "bmodel.hpp"
#include "bmruntime.h"
#include "bmruntime.hpp"
#include "bmruntime_bmnet.h"
#include "bmruntime_cpp.h"
#include "bmruntime_interface.h"
#include "cnpy.h"
#include <getopt.h>

static std::string in_file;
static std::string model_file;
static std::string out_file;
static vector<int> devices;

void Usage() {
  printf("Usage:\n"
         "  --version      : Show version.\n"
         "  --input        : Set input npz file \n"
         "  --model        : Set model path \n"
         "  --output       : Set output npz file \n"
         "  --devid        : Set devices to run for model, e.g. 1,2. if not "
         "set, use 0\n");
}

static void split(const std::string &s, const std::string &delim,
                  std::vector<std::string> &ret) {
  size_t last = 0;
  size_t index = s.find_first_of(delim, last);
  while (index != std::string::npos) {
    ret.push_back(s.substr(last, index - last));
    last = index + 1;
    index = s.find_first_of(delim, last);
  }
  if (last < s.length()) {
    ret.push_back(s.substr(last));
  }
}

static vector<int> parseDevices(const string &str) {
  vector<int> devices;
  vector<string> sub_str;
  split(str, ",", sub_str);
  for (auto &s : sub_str) {
    devices.push_back(std::atoi(s.c_str()));
  }
  return devices;
}

static void deal_with_options(int argc, char **argv) {
  int ch, lopt, idx = 0;
  static struct option options[] = {{"version", no_argument, NULL, 'v'},
                                    {"input", required_argument, NULL, 'i'},
                                    {"model", required_argument, NULL, 'm'},
                                    {"output", required_argument, NULL, 'o'},
                                    {"devid", required_argument, NULL, 'd'},
                                    {0, 0, 0, 0}};

  if (argc < 2) {
    Usage();
    exit(-1);
  }

  while ((ch = getopt_long(argc, argv, "v:i:m:o:d:", options, &idx)) != -1) {
    switch (ch) {
    case 'v':
      std::cout << VER << std::endl;
      exit(0);
      break;
    case 'i':
      in_file = optarg;
      break;
    case 'm':
      model_file = optarg;
      break;
    case 'o':
      out_file = optarg;
      break;
    case 'd':
      devices = parseDevices(optarg);
      break;
    default:
      // unknown option
      BMRT_LOG(FATAL, "Unknown option");
      Usage();
      break;
    }
  }
  if (in_file.empty() || model_file.empty() || out_file.empty()) {
    BMRT_LOG(FATAL, "Unknown option");
    Usage();
    exit(-1);
  }
}

static void add_array(cnpy::npz_t &map, std::string name, bm_handle_t bm_handle,
                      const bm_tensor_t &dst) {
  std::vector<size_t> shape;
  size_t count = 1;
  for (int i = 0; i < dst.shape.num_dims; i++) {
    auto d = dst.shape.dims[i];
    shape.push_back(d);
    count *= d;
  }
  size_t real_bytes = bmrt_tensor_bytesize(&dst);
  switch (dst.dtype) {
  case BM_FLOAT32: {
    std::vector<float> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  case BM_INT32: {
    std::vector<int32_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  case BM_UINT32: {
    std::vector<uint32_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  case BM_UINT16:
  case BM_FLOAT16:
  case BM_BFLOAT16: {
    std::vector<uint16_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  case BM_INT16: {
    std::vector<int16_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  case BM_INT8: {
    std::vector<int8_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  case BM_UINT8: {
    std::vector<uint8_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  default:
    BMRT_LOG(FATAL, "Not support type %d\n", dst.dtype);
    exit(-1);
  }
}

void readTensor(cnpy::npz_t &map, const std::string &name, uint8_t *data,
                size_t bytes, bm_shape_t &shape) {
  auto it = map.find(name.c_str());
  if (it == map.end()) {
    BMRT_LOG(FATAL, "failed to find tensor %s\n", name.c_str());
    exit(-1);
  }
  auto arr = it->second;
  if (arr.num_bytes() > bytes) {
    BMRT_LOG(FATAL, "size is too large for tensor %s\n", name.c_str());
    exit(-1);
  }
  if (arr.shape.size() > 0) {
    shape.num_dims = arr.shape.size();
    for (int i = 0; i < shape.num_dims; ++i) {
      shape.dims[i] = arr.shape[i];
    }
  }
  memcpy(data, arr.data_holder->data(), arr.num_bytes());
}

int main(int argc, char **argv) {
  deal_with_options(argc, argv);
  auto npz_in = cnpy::npz_load(in_file);
  cnpy::npz_t npz_out;
  if (devices.empty()) {
    devices.push_back(0);
  }
  int device_num = devices.size();
  bm_handle_t bm_handles[device_num];
  bm_status_t status;
  unsigned int chipid;
  for (int i = 0; i < device_num; i++) {
    auto status = bm_dev_request(&bm_handles[i], devices[i]);
    if (BM_SUCCESS != status) {
      BMRT_LOG(FATAL, "bm_dev_request failed, id:[%d]", devices[i]);
      exit(-1);
    }
    unsigned int chipid_ = 0;
    if (0 != bm_get_chipid(bm_handles[i], &chipid_)) {
      BMRT_LOG(FATAL, "Cannot get chipid");
      exit(-1);
    }
    if (i == 0) {
      chipid = chipid_;
    } else if (chipid != chipid_) {
      BMRT_LOG(FATAL, "Not same chipid");
      exit(-1);
    }
  }
  auto p_bmrt = bmrt_create_ex(bm_handles, devices.size());
  bool flag = bmrt_load_bmodel(p_bmrt, model_file.c_str());
  if (!flag) {
    BMRT_LOG(FATAL, "Load bmodel[%s] failed", model_file.c_str());
    exit(-1);
  }
  bmrt_show_neuron_network(p_bmrt);
  const char **net_names = NULL;
  bmrt_get_network_names(p_bmrt, &net_names);
  int net_num = bmrt_get_network_number(p_bmrt);
  if (net_num != 1) {
    BMRT_LOG(FATAL, "Only support one net bmodel");
    exit(-1);
  }
  auto net_info = bmrt_get_network_info(p_bmrt, net_names[0]);
  if (net_info->stage_num != 1) {
    BMRT_LOG(FATAL, "Only support one stage bmodel");
    exit(-1);
  }
  std::vector<bm_tensor_t> input_tensors(net_info->input_num);
  std::vector<bm_tensor_t> output_tensors(net_info->output_num);
  auto &stage = net_info->stages[0];
  for (int i = 0; i < net_info->input_num; i++) {
    int devid = net_info->input_loc_devices[i];
    uint8_t *buffer = new uint8_t[net_info->max_input_bytes[i]];
    auto real_shape = stage.input_shapes[i];
    readTensor(npz_in, net_info->input_names[i], buffer,
               net_info->max_input_bytes[i], real_shape);
    bmrt_tensor_ex(&input_tensors[i], p_bmrt, devid,
                  net_info->input_dtypes[i], real_shape);
    bm_memcpy_s2d(bm_handles[devid], input_tensors[i].device_mem, buffer);
    delete[] buffer;
  }
  for (int i = 0; i < net_info->output_num; i++) {
    bmrt_tensor_ex(&output_tensors[i], p_bmrt, net_info->output_loc_devices[i],
                  net_info->output_dtypes[i], stage.output_shapes[i]);
  }
  bool ret = bmrt_launch_tensor_ex(p_bmrt, net_names[0], input_tensors.data(),
                                   net_info->input_num, output_tensors.data(),
                                   net_info->output_num, true, false);
  if (ret == true) {
    status = bm_thread_sync(bm_handles[0]);
  }
  if (ret == false || BM_SUCCESS != status) {
    BMRT_LOG(FATAL, "Neuron network '%s' inference failed", net_names[0]);
    exit(-1);
  }
  for (int i = 0; i < net_info->output_num; i++) {
    int devid = net_info->output_loc_devices[i];
    add_array(npz_out, net_info->output_names[i], bm_handles[devid],
              output_tensors[i]);
  }
  cnpy::npz_save_all(out_file, npz_out);
  bmrt_destroy(p_bmrt);
  for (int i = 0; i < device_num; i++) {
    bm_dev_free(bm_handles[i]);
  }
  return 0;
}
#endif
