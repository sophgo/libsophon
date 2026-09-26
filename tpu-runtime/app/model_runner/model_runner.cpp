#ifndef __linux__
int main(int argc, char **argv) {
  // do nothing
  return 0;
}
#else
#include "bmlib_runtime.h"
#include "bmodel.hpp"
#include "bmruntime.h"
#include "bmruntime_cpp.h"
#include "bmruntime_interface.h"
#include "cnpy.h"
#include <cmath>
#include <sys/time.h>
#include "tpu_fp16.hpp"
#include <getopt.h>

// ---------------------------------------------------------------------------
// fp32 <-> bf16 / fp16 conversions.
//
// The on-disk npz input is usually fp32, while the bmodel may take bf16/fp16
// tensors; conversely bf16/fp16 outputs are stored back as fp32 so the result
// is directly comparable with the fp32 reference. The semantics mirror
// tpu-mlir/python/utils/lowering.py used by python/tools/model_runner.py:
//   * fp32 -> bf16 : truncate the low 16 bits (lowering.fp32_to_bf16)
//   * fp32 -> fp16 : round to nearest even (numpy float16 cast)
//   * bf16 -> fp32 : place the bits in the high 16 (lowering.bf16_to_fp32)
//   * fp16 -> fp32 : ieee half to single (numpy float32 cast)
// ---------------------------------------------------------------------------
static inline uint16_t fp32_to_bf16_bits(float v) {
  tpu::fp32 f;
  f.fval = v;
  return (uint16_t)(f.bits >> 16);
}

static inline float bf16_bits_to_fp32(uint16_t b) {
  tpu::fp32 f;
  f.bits = (uint32_t)b << 16;
  return f.fval;
}

static inline uint16_t fp32_to_fp16_bits(float v) {
  return tpu::to<tpu::fp16>(v).bits;
}

static inline float fp16_bits_to_fp32(uint16_t h) {
  tpu::fp16 v;
  v.bits = h;
  return tpu::to<float>(v);
}

// ---------------------------------------------------------------------------
// timing helpers (mirror app/bmrt_test.cpp)
// ---------------------------------------------------------------------------
#ifdef __linux__
typedef struct timeval bmrt_time_t;
static inline void bmrt_gettime(bmrt_time_t &val) {
  gettimeofday(&val, NULL);
}
static inline long bmrt_interval(const bmrt_time_t &start,
                                const bmrt_time_t &end) {
  return (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
}
#else
typedef struct timespec bmrt_time_t;
static inline void bmrt_gettime(bmrt_time_t &val) {
  bmrt_clock_gettime(0, &val);
}
static inline long bmrt_interval(const bmrt_time_t &start,
                                const bmrt_time_t &end) {
  return (end.tv_sec - start.tv_sec) * 1000000 +
         (end.tv_nsec - start.tv_nsec) / 1000;
}
#endif

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
  case BM_UINT16: {
    std::vector<uint16_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    cnpy::npz_add_array(map, name, data.data(), shape);
  } break;
  case BM_FLOAT16: {
    // save as fp32 so it is directly comparable with the fp32 reference
    std::vector<uint16_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    std::vector<float> fdata(count);
    for (size_t i = 0; i < count; i++) {
      fdata[i] = fp16_bits_to_fp32(data[i]);
    }
    cnpy::npz_add_array(map, name, fdata.data(), shape);
  } break;
  case BM_BFLOAT16: {
    // save as fp32 so it is directly comparable with the fp32 reference
    std::vector<uint16_t> data(count);
    bm_memcpy_d2s_partial(bm_handle, data.data(), dst.device_mem, real_bytes);
    std::vector<float> fdata(count);
    for (size_t i = 0; i < count; i++) {
      fdata[i] = bf16_bits_to_fp32(data[i]);
    }
    cnpy::npz_add_array(map, name, fdata.data(), shape);
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

size_t readTensor(cnpy::npz_t &map, const std::string &name, uint8_t *data,
                  size_t bytes, bm_shape_t &shape, bm_data_type_t dtype) {
  auto it = map.find(name.c_str());
  if (it == map.end()) {
    BMRT_LOG(FATAL, "failed to find tensor %s\n", name.c_str());
    exit(-1);
  }
  auto arr = it->second;
  if (arr.shape.size() > 0) {
    shape.num_dims = arr.shape.size();
    for (int i = 0; i < shape.num_dims; ++i) {
      shape.dims[i] = arr.shape[i];
    }
  }
  // The npz input is frequently stored as fp32 while the bmodel takes bf16/fp16.
  // Convert element-wise in that case; otherwise copy the raw bytes verbatim.
  bool is_fp32 = (arr.type == 'f' && arr.word_size == sizeof(float));
  if (is_fp32 && (dtype == BM_BFLOAT16 || dtype == BM_FLOAT16)) {
    size_t nvals = arr.num_vals;
    size_t real_bytes = nvals * sizeof(uint16_t);
    if (real_bytes > bytes) {
      BMRT_LOG(FATAL, "size is too large for tensor %s\n", name.c_str());
      exit(-1);
    }
    const float *src = arr.data<float>();
    uint16_t *dst = reinterpret_cast<uint16_t *>(data);
    if (dtype == BM_BFLOAT16) {
      for (size_t i = 0; i < nvals; i++) {
        dst[i] = fp32_to_bf16_bits(src[i]);
      }
    } else {
      for (size_t i = 0; i < nvals; i++) {
        dst[i] = fp32_to_fp16_bits(src[i]);
      }
    }
    return real_bytes;
  }
  if (arr.num_bytes() > bytes) {
    BMRT_LOG(FATAL, "size is too large for tensor %s\n", name.c_str());
    exit(-1);
  }
  memcpy(data, arr.data_holder->data(), arr.num_bytes());
  return arr.num_bytes();
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
    size_t real_bytes = readTensor(npz_in, net_info->input_names[i], buffer,
                                   net_info->max_input_bytes[i], real_shape,
                                   net_info->input_dtypes[i]);
    if (!bmrt_tensor_ex(&input_tensors[i], p_bmrt, devid,
                        net_info->input_dtypes[i], real_shape)) {
      BMRT_LOG(FATAL, "alloc input tensor[%d] failed", i);
      exit(-1);
    }
    // copy only the bytes actually read/converted, which may be smaller than
    // device_mem.size when the npz input is smaller than the static shape
    bm_memcpy_s2d_partial(bm_handles[devid], input_tensors[i].device_mem, buffer,
                          real_bytes);
    delete[] buffer;
  }
  for (int i = 0; i < net_info->output_num; i++) {
    if (!bmrt_tensor_ex(&output_tensors[i], p_bmrt,
                        net_info->output_loc_devices[i],
                        net_info->output_dtypes[i], stage.output_shapes[i])) {
      BMRT_LOG(FATAL, "alloc output tensor[%d] failed", i);
      exit(-1);
    }
  }
  bmrt_time_t t3, t_launch, t_sync;
  bmrt_gettime(t3);
  bool ret = bmrt_launch_tensor_ex(p_bmrt, net_names[0], input_tensors.data(),
                                   net_info->input_num, output_tensors.data(),
                                   net_info->output_num, true, false);
  bmrt_gettime(t_launch);
  if (ret == true) {
    status = bm_thread_sync(bm_handles[0]);
  }
  bmrt_gettime(t_sync);
  if (ret == false || BM_SUCCESS != status) {
    BMRT_LOG(FATAL, "Neuron network '%s' inference failed", net_names[0]);
    exit(-1);
  }
  // launch is async; launch func time covers submitting the workload, sync time
  // covers the actual NPU execution (wait until done).
  long launch_time_us = bmrt_interval(t3, t_launch);
  long sync_time_us = bmrt_interval(t_launch, t_sync);
  printf("net[%s], launch func time %ld us, sync time %ld us, total %ld us\n",
          net_names[0], launch_time_us, sync_time_us,
          launch_time_us + sync_time_us);
  for (int i = 0; i < net_info->output_num; i++) {
    int devid = net_info->output_loc_devices[i];
    add_array(npz_out, net_info->output_names[i], bm_handles[devid],
              output_tensors[i]);
  }
  cnpy::npz_save_all(out_file, npz_out);
  free(net_names);
  bmrt_destroy(p_bmrt);
  for (int i = 0; i < device_num; i++) {
    bm_dev_free(bm_handles[i]);
  }
  return 0;
}
#endif
