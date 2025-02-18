#include <thread>
#include "bmrt_test_inner.h"
#ifndef __linux__
//#include <boost/shared_array.hpp>
#include <gflags/gflags.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include "tpu_fp16.hpp"
using namespace tpu;

/* Internal */
#define COMPARE_EPSILON (1e-2)
#define COMPARE_FIX_ERR (0)
#define IS_NAN(x) ((((x >> 23) & 0xff) == 0xff) && ((x & 0x7fffff) != 0))

extern u64 bmrt_must_alloc_device_mem(void*, bm_device_mem_t*, u32);
extern void bmrt_must_free_device_mem(void*, bm_device_mem_t);

void dump_tensor(bm_handle_t bm_handle, bm_tensor_t &tensor) {
  auto shape = tensor.shape;
  int size = 1;
  for (int i = 0; i < shape.num_dims; ++i){
    size *= shape.dims[i];
  }
  std::vector<float> data(size);
  bm_memcpy_d2s(bm_handle, data.data(), tensor.device_mem);
  // std::cout<< data[0] << "\t" << data[data.size()-1] << std::endl;
  auto ptr = data.data();
  ptr[0] = ptr[0];
}

string CHIPNAME;
vector<string> CONTEXT_DIR_V;
string TEST_CASE;
string BMODEL_LIST;
string BMODEL_PATH;
string OUTPUT_REF_NAME = OUTPUT_REF_DATA;
bool NEED_CMP = true;
int DEV_ID = 0;
int LOOP_NUM = 1;
int CAL_TIMES = 1;
int MEM_NUM = 0;
int NET_SEL = -1;
int STAGE_SEL = -1;
int DELTA_INT = COMPARE_FIX_ERR;
float DELTA_FLOAT = COMPARE_EPSILON;
bool EXPORT_NEURON = false;
bool EXPORT_OUTPUT = false;
bool OUTPUT_DEBUG = false;
int THREAD_NUM = 1;
uint64_t PREALLOC_SIZE = 0x0;
bool b_enable_profile = false;
bool b_enable_mmap = true;
bool b_print_subnet_time = false;
bool b_bmodel_dir = true;
bool memory_prealloc = false;
string DECRYPT_LIB;
bool use_runtime_share_mem = false;
vector<bm_shape_t> shapes;
vector<bm_shape_t> output_shapes;
vector<int> devices;
std::vector<std::vector<int32_t>> core_lists;

typedef struct {
  FILE *f_input_ref;
  FILE *f_output_ref;
  FILE *f_output;
  string dir;
  int net_num;
} context_info_t;

vector<context_info_t> context_info_v;
/* --------------------------------------------------------------------------*/
/* code for compare the result */

typedef union {
  int ival;
  float fval;
} IF_VAL;

static int array_cmp_fp32(const float *p_exp_, const float *p_got_, int len, const char *info_label, float delta)
{
  int max_error_count = 30, error_count = 0;
  const float *p_exp = p_exp_, *p_got = p_got_;
  bool only_warning = false;
  if (1e4 == delta) {
    delta = 1e-2;
    only_warning = true;
  }

  for (int idx = 0; idx < len; idx++) {
    if (max(fabs(p_exp[idx]), fabs(p_got[idx])) > 1.0) {
      // compare rel
      if (min(fabs(p_exp[idx]), fabs(p_got[idx])) < 1e-20) {
        if (!only_warning) error_count ++;
        BMRT_LOG(WRONG, "%s rel warning at index %d exp %.20f got %.20f", info_label, idx, p_exp[idx], p_got[idx]);
        if (!only_warning && error_count > max_error_count) return -1;
      }

      if (fabs(p_exp[idx] - p_got[idx]) / min(fabs(p_exp[idx]), fabs(p_got[idx])) > delta) {
        if (!only_warning) error_count ++;
        BMRT_LOG(WRONG, "%s rel warning at index %d exp %.20f got %.20f", info_label, idx, p_exp[idx], p_got[idx]);
        if (!only_warning && error_count > max_error_count) return -1;
      }
    } else {
      // compare abs
      if (fabs(p_exp[idx] - p_got[idx]) > delta) {
        if (!only_warning) error_count ++;
        BMRT_LOG(WRONG, "%s abs warning at index %d exp %.20f got %.20f", info_label, idx, p_exp[idx], p_got[idx]);
        if (!only_warning && error_count > max_error_count) return -1;
      }
    }

    IF_VAL if_val_exp, if_val_got;
    if_val_exp.fval = p_exp[idx];
    if_val_got.fval = p_got[idx];
    if (IS_NAN(if_val_got.ival) && !IS_NAN(if_val_exp.ival)) {
      BMRT_LOG(WRONG, "There are nans in %s idx %d", info_label, idx);
      BMRT_LOG(WRONG, "floating form exp %.10f got %.10f", if_val_exp.fval, if_val_got.fval);
      BMRT_LOG(WRONG, "hex form exp %8.8x got %8.8x", if_val_exp.ival, if_val_got.ival);
      return -2;
    }
  }

  if (error_count > 0) {
    return - 1;
  } else {
    return 0;
  }
}

static int array_cmp_float(const void*p_exp_, const void*p_got_, int dtype, int len, const char *info_label, float delta) {
    if(dtype == BM_FLOAT16) {
        auto fp16_exp_ptr = (const fp16*)p_exp_;
        auto fp16_got_ptr = (const fp16*)p_got_;
        std::vector<float> exp_vec(len);
        std::vector<float> got_vec(len);
        for(int i=0; i<len; i++){
            exp_vec[i] = to<float>(fp16_exp_ptr[i]);
            got_vec[i] = to<float>(fp16_got_ptr[i]);
        }
        BMRT_DEBUG("  got[0] = %f(0x%4x), ref[0] = %f(0x%04x),  dtype=fp16", got_vec[0], fp16_got_ptr[0].bits,
                exp_vec[0], fp16_got_ptr[0].bits);
        return array_cmp_fp32(exp_vec.data(), got_vec.data(), len, info_label, delta);
    } else if(dtype == BM_BFLOAT16) {
        auto bf16_exp_ptr = (const bf16*)p_exp_;
        auto bf16_got_ptr = (const bf16*)p_got_;
        std::vector<float> exp_vec(len);
        std::vector<float> got_vec(len);
        for(int i=0; i<len; i++){
            exp_vec[i] = to<float>(bf16_exp_ptr[i]);
            got_vec[i] = to<float>(bf16_got_ptr[i]);
        }
        BMRT_DEBUG("  got[0] = %f(0x%4x), ref[0] = %f(0x%04x),  dtype=bf16", got_vec[0], bf16_got_ptr[0].bits,
                exp_vec[0], bf16_got_ptr[0].bits);
        return array_cmp_fp32(exp_vec.data(), got_vec.data(), len, info_label, delta);
    } else if(dtype == BM_FLOAT32) {
        BMRT_DEBUG("  got[0] = %f, ref[0] = %f,  dtype=fp32", ((float*)p_got_)[0], ((float*)p_exp_)[0]);
        return array_cmp_fp32((const float*)p_exp_, (const float*)p_got_, len, info_label, delta);

    } else {
        assert(0 && "not support dtype");
    }
    return 0;
}

static int array_cmp_fix8b(void *p_exp, void *p_got,
                           int sign,  // 0: unsigned, 1: signed
                           int len, const char *info_label, int delta)
{
#define MAX_NUM_ERRORS  30  // max number of error print before return -1

  int ret = 0;
  int num_errors = 0;

  int idx = 0;
  for (idx = 0; idx < len; idx++) {
    int error = 0;
    int exp_int = 0;
    int got_int = 0;
    if (sign) {
      exp_int = (int)(*((signed char *)p_exp + idx));
      got_int = (int)(*((signed char *)p_got + idx));
    } else {
      exp_int = (int)(*((unsigned char *)p_exp + idx));
      got_int = (int)(*((unsigned char *)p_got + idx));
    }
    error = abs(exp_int - got_int);
    if (error > delta) {
      ret = -1;
      BMRT_LOG(WRONG, "%s error at index %3d: [exp %3d, got %3d] diff=%2d (delta=%d)", info_label, idx, exp_int, got_int, got_int - exp_int , delta);
      num_errors ++;
      if (num_errors < MAX_NUM_ERRORS) {
        continue;
      } else {
        return ret;
      }
    }
  }

  return ret;
}

static int array_cmp_fix16b(void *p_exp, void *p_got,
                            int sign,  // 0: unsigned, 1: signed
                            int len, const char *info_label, int delta)
{
  int idx = 0;
  for (idx = 0; idx < len; idx++) {
    int error = 0;
    int exp_int = 0;
    int got_int = 0;
    if (sign) {
      exp_int = (int)(*((short *)p_exp + idx));
      got_int = (int)(*((short *)p_got + idx));
    } else {
      exp_int = (int)(*((unsigned short *)p_exp + idx));
      got_int = (int)(*((unsigned short *)p_got + idx));
    }
    error = abs(exp_int - got_int);
    if (error > delta) {
      BMRT_LOG(WRONG, "%s error at index %d exp %d got %d", info_label, idx, exp_int, got_int);
      return -1;
    }
  }
  return 0;
}

static int array_cmp_fix32b(void *p_exp, void *p_got,
                            int sign,  // 0: unsigned, 1: signed
                            int len, const char *info_label, int delta)
{
  int idx = 0;
  for (idx = 0; idx < len; idx++) {
    int error = 0;
    int exp_int = 0;
    int got_int = 0;
    if (sign) {
      exp_int = (int)(*((int *)p_exp + idx));
      got_int = (int)(*((int *)p_got + idx));
    } else {
      exp_int = (int)(*((unsigned int *)p_exp + idx));
      got_int = (int)(*((unsigned int *)p_got + idx));
    }
    error = abs(exp_int - got_int);
    if (error > delta) {
      BMRT_LOG(WRONG, "%s error at index %d exp %d got %d", info_label, idx, exp_int, got_int);
      return -1;
    }
  }
  return 0;
}

template <typename T>
void dump_debug_data(const T *host_output_data, const T *ref_output_data, int output_idx, int len) {
  string outfile_name = "output_";
  outfile_name += std::to_string(output_idx) + ".dat";
  FILE* out_fp = fopen(outfile_name.c_str(), "w");
  string ref_name = "ref_";
  ref_name += std::to_string(output_idx) + ".dat";
  FILE* ref_fp = fopen(ref_name.c_str(), "w");

  for (int o = 0; o < len; ++o) {
    std::ostringstream ss;
    ss << +host_output_data[o];
    string s_num = ss.str();
    fprintf(out_fp, "%s\n", s_num.c_str());

    ss.str("");
    ss << +ref_output_data[o];
    s_num = ss.str();
    fprintf(ref_fp, "%s\n", s_num.c_str());
  }

  fclose(ref_fp);
  fclose(out_fp);
}

#include <fstream>
#include <iostream>
#include <iomanip> // std::setw()
#if (__x86_64__)
#include <cxxabi.h> // abi::__cxa_demangle()
#endif
using std::ifstream;
using std::ofstream;

template<typename T>
int dump_tensor(string & tensor_name, T const* data,  vector<int>& shape)
{
    string type_name = "";
#if (__x86_64__) // for ARM platfrom, since `-fno-rtti` is used, we cannot use typeid().
    // get data type name
    type_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
    // replace space with underscore, if there is any)
    std::transform(type_name.begin(), type_name.end(), type_name.begin(), [](char ch) {
        return ch == ' ' ? '_' : ch;
    });
#endif
    string file_path = tensor_name + "_" + type_name + ".txt";

    // replace `/` in tensor_name with `_`, if there is any
    string::size_type pos = 0;
    while ((pos = file_path.find('/', pos)) != string::npos) {
        file_path[pos] = '_';
        pos ++;
    }

    ofstream fout(file_path, std::ios_base::out);
    if (!fout.is_open()) {
        BMRT_LOG(WRONG, "open file %s for writing failed!\n", file_path.c_str());
        return - 1;
    }

    int n = 1, c = 1, h = 1, w = 1;

    if (shape.size() > 4) {
        fout << type_name
             <<", shape=[";
        for (size_t i = 0; i < shape.size(); ++ i) {
            fout << shape[i] << ",";
        }
        fout << "]" << "\n";
        fout << "rank > 4, dump the last 4 dimension only." << endl;
        w = shape.rbegin()[0];
        h = shape.rbegin()[1];
        c = shape.rbegin()[2];
        n = shape.rbegin()[3];
    } else {
        n = shape[0];
        if (shape.size() > 1)  c = shape[1];
        if (shape.size() > 2)  h = shape[2];
        if (shape.size() > 3)  w = shape[3];
        fout << type_name
            << ", shape[N, C, H, W]=["
            << n << ","
            << c << ","
            << h << ","
            << w << "]" << "\n";
    }

    int n_stride = c * h * w;
    int min_width = 4;

    for (int n_idx = 0; n_idx < n; n_idx++) {
        for (int c_idx = 0; c_idx < c; c_idx++) {
            fout << "N=" << n_idx << ", C=" << c_idx << '\n';
            for (int h_idx = 0; h_idx < h; h_idx++) {
                for (int w_idx = 0; w_idx < w; w_idx++) {
                    fout << std::setw(min_width) << +data[n_idx * n_stride + c_idx * h * w + h_idx * w + w_idx] << " ";
                }
                fout << '\n';
            }
            fout << '\n';
        }
        fout << '\n';
    }
    fout.close();

    return 0;
}

static int dump_tensor_all(string & tensor_name, void const* data,  int dtype, vector<int>& shape){
  int res = 0;
  switch(dtype) {
  case BM_FLOAT32: res=dump_tensor(tensor_name, (float*)data, shape); break;
  case BM_FLOAT16: res=dump_tensor(tensor_name, (fp16*)data, shape); break;
  case BM_BFLOAT16: res=dump_tensor(tensor_name, (bf16*)data, shape); break;
  case BM_INT8:    res=dump_tensor(tensor_name, (int8_t*)data, shape); break;
  case BM_UINT8:   res=dump_tensor(tensor_name, (uint8_t*)data, shape); break;
  case BM_INT16:   res=dump_tensor(tensor_name, (int16_t*)data, shape); break;
  case BM_UINT16:  res=dump_tensor(tensor_name, (uint16_t*)data, shape); break;
  case BM_INT32:   res=dump_tensor(tensor_name, (int32_t*)data, shape); break;
  case BM_UINT32:  res=dump_tensor(tensor_name, (uint32_t*)data, shape); break;
  default:
    BMRT_LOG(FATAL, "Unsupported input data type %d\n", dtype);
  }
  return res;
}

int result_cmp_inner(int output_index, const void* host_data, const void* ref_data, int len, bm_data_type_t dtype){
      int flag = 0;
      const char *info_label = "error comparing the last tensor: ";
      switch (dtype) {
      case BM_INT32:
      case BM_UINT32: {
          auto host_data_ptr =(int *)host_data;
          auto ref_data_ptr =(int *)ref_data;
          BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", output_index, host_data_ptr[0], ref_data_ptr[0]);
          flag = array_cmp_fix32b((void *)ref_data_ptr, (void *)host_data_ptr,
                                  dtype == BM_INT32 ? 1 : 0, len, info_label, DELTA_INT);
          if (OUTPUT_DEBUG) {
            dump_debug_data(host_data_ptr, ref_data_ptr, output_index, len);
          }
        }
        break;
      case BM_INT16:
      case BM_UINT16: {
          auto host_data_ptr =(short *)host_data;
          auto ref_data_ptr =(short *)ref_data;
          BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", output_index, host_data_ptr[0], ref_data_ptr[0]);
          flag = array_cmp_fix16b((void *)ref_data_ptr, (void *)host_data_ptr,
                                  dtype == BM_INT16 ? 1 : 0, len, info_label, DELTA_INT);
          if (OUTPUT_DEBUG) {
            dump_debug_data(host_data_ptr, ref_data_ptr, output_index, len);
          }
        }
        break;
      case BM_INT8:{
          auto host_data_ptr =(char *)host_data;
          auto ref_data_ptr =(char *)ref_data;
          BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", output_index,
                     host_data_ptr[0], ref_data_ptr[0]);
          flag = array_cmp_fix8b((void*)ref_data_ptr, (void *)host_data_ptr, 1, len, info_label, DELTA_INT);
          if (OUTPUT_DEBUG) {
            dump_debug_data(host_data_ptr, ref_data_ptr, output_index, len);
          }
        }
        break;
      case BM_UINT8: {
          auto host_data_ptr =(unsigned char *)host_data;
          auto ref_data_ptr =(unsigned char *)ref_data;
          BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", output_index,
                     host_data_ptr[0], ref_data_ptr[0]);
          flag = array_cmp_fix8b((void *)ref_data_ptr, (void *)host_data_ptr, 0, len, info_label, DELTA_INT);
          if (OUTPUT_DEBUG) {
            dump_debug_data(host_data_ptr, ref_data_ptr, output_index, len);
          }
        }
        break;
      default:{
          BMRT_DEBUG("The %d-th tensor ", i);
          flag = array_cmp_float(ref_data, host_data, dtype, len, info_label, DELTA_FLOAT);
          if (OUTPUT_DEBUG) {
            dump_debug_data((float*)ref_data, (float*)host_data, output_index, len);
          }
        }
        break;
      }
      if(flag){
          BMRT_LOG(WRONG, "comparing #%d output failed!", output_index);
      }
      return flag;

}
int result_cmp(int8_t **host_output_data, int8_t **ref_output_data, int output_num,
                      int *o_count, bm_data_type_t *o_dtype) {
  int res_flag = 0;
  for (int i = 0; i < output_num; ++i) {
    res_flag |= result_cmp_inner(i, host_output_data[i], ref_output_data[i], o_count[i], o_dtype[i]);
  }
  return res_flag;
}

int result_cmp(const std::vector<std::vector<int8_t>>& host_output_data,
               const std::vector<std::vector<int8_t>>& ref_output_data,
               const std::vector<int>& o_count,
               const bm_data_type_t* o_dtype)
{
  int res_flag = 0;
  int output_num = host_output_data.size();
  for (int i = 0; i < output_num; ++i) {
    res_flag |= result_cmp_inner(i, host_output_data[i].data(), ref_output_data[i].data(), o_count[i], o_dtype[i]);
  }
  return res_flag;
}

string fix_bmodel_path(const string& path) {
  if (b_bmodel_dir) {
    return path + "/compilation.bmodel";
  }
  return path;
}

/* ------------------------------------------------------------------------- */

#ifdef __linux__
typedef struct timeval bmrt_time_t;
static inline void bmrt_gettime(bmrt_time_t& val){
  gettimeofday(&val, NULL);
}
static inline long bmrt_interval(const bmrt_time_t& start, const bmrt_time_t& end){
    return (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
}
#else
typedef struct timespec bmrt_time_t;
static inline void bmrt_gettime(bmrt_time_t& val){
  bmrt_clock_gettime(0, &val);
}
static inline long bmrt_interval(const bmrt_time_t& start, const bmrt_time_t& end){
    return (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec)/1000;
}
#endif

static bmrt_time_t g_time;
static void start_time() {
  bmrt_gettime(g_time);
}

static long end_time() {
  bmrt_time_t end_time;
  bmrt_gettime(end_time);
  long elapsed = bmrt_interval(g_time, end_time);
  g_time = end_time;
  return elapsed;
}

/* --------------------------------------------------------------------------*/
/* code for read input and output reference data */

void open_ref_file(const string &context_dir, FILE *&f_input, FILE *&f_output)
{
  if (NEED_CMP == false) {
    return;
  }
  string input_ref_dir = context_dir + "/" + INPUT_REF_DATA;
  string output_ref_dir = context_dir + "/" + OUTPUT_REF_NAME;

  f_input = fopen(input_ref_dir.c_str(), "rb");
  if (f_input == NULL) {
    BMRT_LOG(FATAL, "cannot open file %s", input_ref_dir.c_str());
  }
  f_output = fopen(output_ref_dir.c_str(), "rb");
  if (f_output == NULL) {
    BMRT_LOG(FATAL, "cannot open file %s", output_ref_dir.c_str());
  }
}

void close_ref_file()
{
  for (auto &info : context_info_v) {
    if (info.f_input_ref != NULL) {
      fclose(info.f_input_ref);
    }
    if (info.f_output_ref != NULL) {
      fclose(info.f_output_ref);
    }
    if (info.f_output != NULL) {
      fclose(info.f_output);
    }
  }
}

void open_output_file(const string &context_dir, FILE *&f_output)
{
  if (EXPORT_OUTPUT) {
    string output_dir;
    if (b_bmodel_dir) {
      output_dir = context_dir + "/" + "output.dat";
    } else {
      string::size_type pos = context_dir.rfind("/");
      output_dir = context_dir.substr(0, pos) + "/output.dat";
    }
    f_output = fopen(output_dir.c_str(), "wb");
    if (f_output == NULL) {
      BMRT_LOG(FATAL, "Cannot save file %s", output_dir.c_str());
    }
  } else {
    f_output = NULL;
  }
}

void rewind_ref_file()
{
  for (auto &info : context_info_v) {
    if (info.f_input_ref != NULL) {
      rewind(info.f_input_ref);
    }
    if (info.f_output_ref != NULL) {
      rewind(info.f_output_ref);
    }
  }
}

void read_ref_data(int8_t *host_data, size_t size, int net_idx, bool is_input = true, size_t gap_size = 0)
{
  if (NEED_CMP == false) {
    // use 0 input
    memset(host_data, 0, size);
    return;
  }
  for (auto &info : context_info_v) {
    if (net_idx >= info.net_num) {
      continue;
    }
    FILE *file = is_input ? info.f_input_ref : info.f_output_ref;
    auto pos = ftell(file);
    if (size > 0 && 1 != fread(host_data, size, 1, file)) {
        BMRT_LOG(FATAL, "Failed to fread reference data of net #%d, need_bytesize=%zu, but just %zu bytes left", net_idx, size, ftell(file)-pos);
        break;
    }
    if (gap_size != 0) {
      fseek(file, gap_size, SEEK_CUR);
    }
    return;
  }
}


void fseek_ref_file_by_stage(int begin_stage, int end_stage, int net_idx, const bm_net_info_t *net_info)
{
  if (NEED_CMP == false) return;
  int begin, end;
  begin = begin_stage >= 0 ? begin_stage : 0;
  end = end_stage <= net_info->stage_num ? end_stage : net_info->stage_num;
  size_t input_ref_offset = 0;
  size_t output_ref_offset = 0;
  for (int sidx = begin; sidx < end; ++sidx) {
    auto &stage = net_info->stages[sidx];
    for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
      input_ref_offset += bmrt_shape_count(&stage.input_shapes[input_idx]) *
                          bmrt_data_type_size(net_info->input_dtypes[input_idx]);
    }
    for (int output_idx = 0; output_idx < net_info->output_num; ++output_idx) {
      output_ref_offset += bmrt_shape_count(&stage.output_shapes[output_idx]) *
                           bmrt_data_type_size(net_info->output_dtypes[output_idx]);
    }
  }
  for (auto &info : context_info_v) {
    if (net_idx >= info.net_num) {
      continue;
    }
    if (input_ref_offset != 0) {
      fseek(info.f_input_ref, input_ref_offset, SEEK_CUR);
    }
    if (output_ref_offset != 0) {
      fseek(info.f_output_ref, output_ref_offset, SEEK_CUR);
    }
    return;
  }
  BMRT_LOG(FATAL, "Failed to fseek ref data of net #%d from stage %d to %d", net_idx, begin, end);
}

void fseek_ref_file_to_net(int net_idx, void *p_bmrt)
{
  if (NEED_CMP == false) return;
  const char **net_names = NULL;
  bmrt_get_network_names(p_bmrt, &net_names);
  int net_num = bmrt_get_network_number(p_bmrt);
  if (net_idx >= net_num) {
    BMRT_LOG(FATAL, "net #%d cannot be found!", net_idx);
  }
  for (int i = 0;  i < net_idx; ++i) {
    auto net_info = bmrt_get_network_info(p_bmrt, net_names[i]);
    fseek_ref_file_by_stage(0, net_info->stage_num, i, net_info);
  }
}

void set_debug_mode(void *p_bmrt, int debug_mode)
{
    ((Bmruntime *)p_bmrt)->set_debug_mode(debug_mode);
}

// set enable mmap flag in bmruntime
void set_bmrt_mmap(void *p_bmrt, bool enable)
{
  ((Bmruntime *)p_bmrt)->set_bmrt_mmap(enable);
}

// set enable mmap flag in bmruntime
void enable_print_subnet_time(void *p_bmrt)
{
  if (b_print_subnet_time) {
    ((Bmruntime *)p_bmrt)->subnet_time_print(true);
  }
}

void save_neuron(void *p_bmrt, int net_idx, int stage_idx)
{
  string dir;
  for (auto &info : context_info_v) {
    if (net_idx >= info.net_num) {
      continue;
    }
    dir = info.dir;
    break;
  }
  if (dir.empty()) {
    return;
  }
  dir += "/neuron_mem_net";
  std::ostringstream str;
  str << dir << net_idx << "_" << stage_idx << ".dat";
  auto device_mem = ((Bmruntime *)p_bmrt)->get_neuron_mem(net_idx);
  bm_handle_t handle = ((Bmruntime *)p_bmrt)->get_bm_handle();
  size_t size = 0;
  for (const auto &mem : device_mem)
    size += bm_mem_get_device_size_u64(mem);
  char *data = new char[size];
  for (const auto &mem : device_mem) {
    bm_memcpy_d2s_partial_u64(handle, data, mem, bm_mem_get_device_size_u64(mem));
  }
  string filename = str.str();
  FILE *fout = fopen(filename.c_str(), "wb");
  if (NULL == fout) {
    BMRT_LOG(FATAL, "Save neuron file[%s] failed.", filename.c_str());
  }
  fwrite(data, size, 1, fout);
  fclose(fout);
  delete[] data;
}

void save_output(int net_idx, int8_t data[], size_t size)
{
  for (auto &info : context_info_v) {
    if (net_idx >= info.net_num) {
      continue;
    }
    fwrite(data, size, 1, info.f_output);
    return;
  }
}
void save_data(const char* filename, const void* data, size_t byte_size){
  FILE* fp = fopen(filename, "wb");
  if(!fp) {
    BMRT_LOG(WARNING, "Cannot save data to file %s", filename);
    return;
  }
  fwrite(data, byte_size, 1, fp);
  fclose(fp);
}

/* --------------------------------------------------------------------------*/
/* code for inference by new nntoolchain runtime interface */
static bool load_bmodel_with_decrypt(const string &dir, void *p_bmrt) {
  // return bmrt_load_bmodel_with_decrypt_lib(p_bmrt, bmodel_path.c_str(),
  // DECRYPT_LIB.c_str());
  auto handle = bmrt_load_lib(DECRYPT_LIB.c_str(), RTLD_LAZY);
  if (!handle) {
    return false;
  }
  auto f = (decrypt_func)bmrt_find_sym(handle, "decrypt");
  if (!f) {
    return false;
  }
  auto ret = bmrt_load_bmodel_with_decrypt(p_bmrt, dir.c_str(), f);
  bmrt_unload_lib(handle);
  return ret;
}

static void load_bmodel(const string &dir, void *p_bmrt)
{
  string bmodel_path = fix_bmodel_path(dir);
  bool flag = true;
  if (DECRYPT_LIB.empty()) {
    flag = bmrt_load_bmodel(p_bmrt, bmodel_path.c_str());
  } else {
    flag = load_bmodel_with_decrypt(bmodel_path, p_bmrt);
  }
  if (!flag) {
    BMRT_LOG(FATAL, "Load bmodel[%s] failed", bmodel_path.c_str());
  }
}

static void load_bmodel_with_mem(const string &dir, void *p_bmrt, mem_info_t *mem_info)
{
  string bmodel_path = fix_bmodel_path(dir);
  bool flag = bmrt_load_bmodel_with_mem(p_bmrt, bmodel_path.c_str(), mem_info);
  if (!flag) {
    BMRT_LOG(FATAL, "Load bmodel[%s] failed", bmodel_path.c_str());
  }
}

void bmrt_set_debug_mode(void * p_bmrt, int mode)
{
  ((Bmruntime *)p_bmrt)->set_debug_mode(mode);
}

template<typename T>
void print_array(const T* data, int len, const char* prefix = nullptr, int max_print=16){
    std::ostringstream ss;
    if(prefix){
        ss<<prefix<<" ";
    }
    int real_len = len>max_print? max_print: len;
    ss << "< ";
    for(int i=0; i<real_len; i++){
        ss<<data[i]<<" ";
    }
    if(real_len != len){
        ss << "... > len="<<len;
    } else {
        ss << ">";
    }
    string info = ss.str();
    BMRT_LOG(INFO, "%s", info.c_str());
}

void print_array_ex(const void* data, int len, int dtype, const char* prefix = nullptr, int max_print=16){
  if (dtype == BM_INT32) {
    print_array<int>((const int *)data, len, prefix);
  } else if (dtype == BM_FLOAT32) {
    print_array<float>((const float *)data, len, prefix);
  }
}

void bmrt_launch_tensor_thread_func(
  bm_handle_t &handle, void *p_bmrt, const char *net_name,
  const bm_tensor_t *input_tensors,
  int input_num, bm_tensor_t *output_tensors,
  int output_num, bool user_mem,
  bool user_stmode, const int *core_list,
  int core_num, int net_idx, int stage_idx,
  int chipid, bmrt_time_t &launch_time, bm_profile_t &start, bm_profile_t &end,
  bool using_thread, uint64_t thread_idx) {
  if (chipid != 0x1682) {
    bm_get_profile(handle, &start);
  }
  bool ret;
  if (using_thread) {
    ret = bmrt_launch_tensor_multi_thread(p_bmrt, net_name, input_tensors, input_num,
            output_tensors, output_num, thread_idx, user_mem, user_stmode, core_list, core_num);
  } else {
    ret = bmrt_launch_tensor_multi_cores(p_bmrt, net_name, input_tensors, input_num,
            output_tensors, output_num, user_mem, user_stmode, core_list, core_num);
  }
  if (ret == false) {
    print_array_ex(core_list, core_num, BM_INT32,  "cores=");
    BMRT_LOG(FATAL, "The %d-th neuron network '%s' stage '%d' launch failed", net_idx,
            net_name, stage_idx);
  }
  bmrt_gettime(launch_time);
  for (int i = 0; i < core_num; ++i) {
    auto core_id = core_list[i];
    bm_status_t core_status = bm_thread_sync_from_core(handle, core_id);
    if (core_status != BM_SUCCESS) {
      print_array_ex(core_list, core_num, BM_INT32,  "cores=");
      BMRT_LOG(FATAL, "The %d-th neuron network '%s' stage '%d' sync failed, core=%d", net_idx,
              net_name, stage_idx, core_id);
    }
  }
  if (chipid != 0x1682) {
    bm_get_profile(handle, &end);
  }
}

void malloc_device_mem(bm_handle_t bm_handle, memory_t &mem, std::vector<bm_device_mem_t> &prealloc_mem_v) {
  if (mem.size > 0) {
    bm_device_mem_t dmem;
    if (bm_malloc_device_byte(bm_handle, &dmem, mem.size) == BM_SUCCESS) {
       mem.addr = bm_mem_get_device_addr(dmem);
       prealloc_mem_v.push_back(dmem);
    }
  }
}

void bmrt_test()
{
#ifdef __linux__
  // Let not be nice
  if (setpriority(PRIO_PGRP, 0, -20) != 0)
  {
    BMRT_LOG(WARNING, "setpriority failed, cpu time might flutuate.");
  }
#endif

  // create bmruntime
  unsigned int chipid = 0;
  if (devices.empty()) {
    devices.push_back(DEV_ID);
  }
  int device_num = devices.size();
  vector<bm_handle_t> bm_handles(device_num);
  bm_device_mem_t prealloc_mem;
  vector<bm_device_mem_t> prealloc_mem_v;
  bm_status_t status;
  for (int i = 0; i < device_num; i++) {
    status = bm_dev_request(&bm_handles[i], devices[i]);
    if (BM_SUCCESS != status) {
      BMRT_LOG(FATAL, "bm_dev_request failed, id:[%d]",  devices[i]);
    }
    unsigned int chipid_ = 0;
    if (0 != bm_get_chipid(bm_handles[i], &chipid_)) {
      BMRT_LOG(FATAL, "Cannot get chipid");
    }
    if (i == 0) {
      chipid = chipid_;
    } else if (chipid != chipid_) {
      BMRT_LOG(FATAL, "Not same chipid");
    }
    BMRT_LOG(INFO, "device_index=%d, chip_id=%d", i, chipid_);
  }
  bm_handle_t bm_handle = bm_handles[0];
  auto p_bmrt = bmrt_create_ex(bm_handles.data(), device_num);
  if (use_runtime_share_mem) {
    bmrt_set_flags(p_bmrt, BM_RUNTIME_SHARE_MEM);
    BMRT_LOG(INFO, "Use runtime share mem");
  }

  if (PREALLOC_SIZE != 0) {
    bmrt_must_alloc_device_mem(p_bmrt, &prealloc_mem, PREALLOC_SIZE);
    BMRT_LOG(INFO, "prealloc device mem, base[0x%llx], size[0x%x]",
              bm_mem_get_device_addr(prealloc_mem), bm_mem_get_device_size(prealloc_mem));
  }

  mem_info_t mem_info;
  std::vector<mem_info_t> mem_info_v;
  bool using_multi_thread = false;
  if (memory_prealloc) {
    if (bmrt_get_bmodel_info(fix_bmodel_path(CONTEXT_DIR_V[0]).c_str(), &mem_info) == false) {
      BMRT_LOG(FATAL, "Load bmodel Failed");
    }
    BMRT_ASSERT(CONTEXT_DIR_V.size() == 1);
    // malloc device memory
    malloc_device_mem(bm_handle, mem_info.instruction_mem, prealloc_mem_v);
    malloc_device_mem(bm_handle, mem_info.variable_instruction_mem, prealloc_mem_v);
    malloc_device_mem(bm_handle, mem_info.neuron_mem, prealloc_mem_v);
    malloc_device_mem(bm_handle, mem_info.coeff_mem, prealloc_mem_v);
    malloc_device_mem(bm_handle, mem_info.io_mem, prealloc_mem_v);
    mem_info_v.resize(core_lists.size());
    for (int i = 0; i < core_lists.size(); ++i) {
      mem_info_v[i].neuron_mem.size = mem_info.neuron_mem.size;
      mem_info_v[i].io_mem.size = mem_info.io_mem.size;
      malloc_device_mem(bm_handle, mem_info_v[i].neuron_mem, prealloc_mem_v);
      malloc_device_mem(bm_handle, mem_info_v[i].io_mem, prealloc_mem_v);
    }
  }

  MEM_NUM = MEM_NUM == 1 ? 1 : CAL_TIMES;
  enable_print_subnet_time(p_bmrt);

#ifdef SOC_MODE
  set_bmrt_mmap(p_bmrt, b_enable_mmap);
#endif
  int REAL_MEM_NUM = MEM_NUM * core_lists.size();

  bool use_multi_thread = true;
  if (core_lists.size() <= 1) {
    //only multi-core multi-mession use threads, cause start thread reduce about 100us performance
    use_multi_thread = false;
  }
#ifdef USING_CMODEL
  //TODO, dynamic backend exist data race
  use_multi_thread = false;
#endif

  // load bmodel by file
  for (auto &context_dir : CONTEXT_DIR_V) {
    context_info_t info;
    info.f_input_ref = NULL;
    info.f_output_ref = NULL;
    if (memory_prealloc) {
      load_bmodel_with_mem(context_dir, p_bmrt, &mem_info);
    } else {
      load_bmodel(context_dir, p_bmrt);
    }
    open_ref_file(context_dir, info.f_input_ref, info.f_output_ref);
    open_output_file(context_dir, info.f_output);
    // get network number
    info.net_num = bmrt_get_network_number(p_bmrt);
    info.dir = context_dir;
    context_info_v.push_back(info);
  }

  bmrt_show_neuron_network(p_bmrt);

  const char **net_names = NULL;
  bmrt_get_network_names(p_bmrt, &net_names);
  int net_num = bmrt_get_network_number(p_bmrt);
  for (int loop = 0; loop < LOOP_NUM; loop++) {
    rewind_ref_file();
    for (int net_idx = 0; net_idx < net_num; ++net_idx) {
      if (NET_SEL != -1) {
        // select the net which user had specified
        net_idx = NET_SEL;
        fseek_ref_file_to_net(NET_SEL, p_bmrt);
      }
      BMRT_LOG(INFO, "==> running network #%d, name: %s, loop: %d", net_idx, net_names[net_idx],
              loop);
      auto net_info = bmrt_get_network_info(p_bmrt, net_names[net_idx]);


      std::vector<std::vector<bm_tensor_t>> output_tensors(REAL_MEM_NUM, std::vector<bm_tensor_t>(net_info->output_num));

      for (int stage_idx = 0; stage_idx < net_info->stage_num; stage_idx++) {
        if (STAGE_SEL != -1) {
          stage_idx = STAGE_SEL;
          fseek_ref_file_by_stage(0, stage_idx, net_idx, net_info);
          if (stage_idx >= net_info->stage_num) {
            BMRT_LOG(INFO, "network #%d is passed, because it hasn't stage idx %d", net_idx, stage_idx);
            continue;
          }
        }

        auto &stage = net_info->stages[stage_idx];
        // setup united_output_shapes
        vector<bm_shape_t> united_output_shapes;
        for (int output_idx = 0; output_idx < net_info->output_num; output_idx++) {
          if (output_idx < (int)output_shapes.size()) {
            united_output_shapes.push_back(output_shapes[output_idx]);
          } else {
            united_output_shapes.push_back(stage.output_shapes[output_idx]);
          }
        }
        // setup united_input_shapes
        vector<bm_shape_t> united_input_shapes;
        for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
          if (input_idx < (int)shapes.size()) {
            united_input_shapes.push_back(shapes[input_idx]);
          } else {
            united_input_shapes.push_back(stage.input_shapes[input_idx]);
          }
        }

        // prepare output tensor
        std::vector<std::vector<std::vector<int8_t>>> host_output(REAL_MEM_NUM, std::vector<std::vector<int8_t>>(net_info->output_num));
        std::vector<int> output_count(net_info->output_num);
        size_t size,ref_size;
        bm_device_mem_t all_output_device_mem;
        for (int mem_idx = 0; mem_idx < REAL_MEM_NUM; ++mem_idx) {
          if (net_info->addr_mode == ADDR_MODE_IO_TAG_FUSE) {
            BMRT_ASSERT_INFO(REAL_MEM_NUM <=1, "IO_TAG_FUSE not support REAL_MEM_NUM > 1\n");
            // caculate all_output_size
            size_t all_output_size = 0;
            for (int output_idx = 0; output_idx < net_info->output_num; output_idx++) {
              int devid = net_info->output_loc_devices[output_idx];
              if (output_idx > 0) {
                // check whether the devid is the same
                int last_devid = net_info->output_loc_devices[output_idx-1];
                BMRT_ASSERT_INFO(devid == last_devid, "devid inconsistency\n");
              }
              all_output_size += bmrt_shape_count(&united_output_shapes[output_idx]) * bmrt_data_type_size(net_info->output_dtypes[output_idx]);
            }
            // malloc all_output_device_mem
            int devid = net_info->output_loc_devices[0];
            bm_malloc_device_byte(bm_handles[devid], &all_output_device_mem, all_output_size);
            u64 all_output_addr = bm_mem_get_device_addr(all_output_device_mem);
            // setup output tensors
            size_t out_offset = 0;
            for (int output_idx = 0; output_idx < net_info->output_num; output_idx++) {
              auto &output_tensor = output_tensors[mem_idx][output_idx];
              size_t tensor_size = bmrt_shape_count(&united_output_shapes[output_idx]) * bmrt_data_type_size(net_info->output_dtypes[output_idx]);
              auto tensor_mem = bm_mem_from_device(all_output_addr + out_offset, tensor_size);
              bmrt_tensor_with_device(&output_tensor, tensor_mem, net_info->output_dtypes[output_idx], united_output_shapes[output_idx]);
              out_offset += tensor_size;
            }
          } else {
            for (int output_idx = 0; output_idx < net_info->output_num; output_idx++) {
              int devid = net_info->output_loc_devices[output_idx];
              auto &output_tensor = output_tensors[mem_idx][output_idx];
              bmrt_tensor_ex(&output_tensor, p_bmrt, devid, net_info->output_dtypes[output_idx], united_output_shapes[output_idx]);
            }
          }
          for (int output_idx = 0; output_idx < net_info->output_num; output_idx++) {
            int devid = net_info->output_loc_devices[output_idx];
            auto &output_tensor = output_tensors[mem_idx][output_idx];
            size = bmrt_tensor_bytesize(&output_tensor);
            ref_size = bmrt_shape_count(&stage.output_shapes[output_idx]) * bmrt_data_type_size(output_tensor.dtype);
            if (output_shapes.size() > 0) ref_size = size; // If we set shape, ref_size may be smaller than size
            host_output[mem_idx][output_idx].resize(ref_size);
            memset(host_output[mem_idx][output_idx].data(), 0, ref_size);
          }
        }

        // prepare input tensor
        bmrt_time_t t1, t2, t3, t4, t5, t6;
        std::vector<bmrt_time_t> launch_times(core_lists.size());
        bmrt_gettime(t1);

        std::vector<bm_tensor_t> input_tensors(net_info->input_num);
        bm_device_mem_t all_input_device_mem;
        if (net_info->addr_mode == ADDR_MODE_IO_TAG_FUSE) {
          // caculate all_input_size
          size_t all_input_size = 0;
          for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
            int devid = net_info->input_loc_devices[input_idx];
            if (input_idx > 0) {
              // check whether the devid is the same
              int last_devid = net_info->input_loc_devices[input_idx-1];
              BMRT_ASSERT_INFO(devid == last_devid, "devid inconsistency\n");
            }
            all_input_size += bmrt_shape_count(&united_input_shapes[input_idx]) * bmrt_data_type_size(net_info->input_dtypes[input_idx]);
          }
          // malloc all_input_device_mem
          int devid = net_info->input_loc_devices[0];
          bm_malloc_device_byte(bm_handles[devid], &all_input_device_mem, all_input_size);
          u64 all_input_addr = bm_mem_get_device_addr(all_input_device_mem);
          // setup input tensors
          size_t in_offset = 0;
          for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
            auto &input_tensor = input_tensors[input_idx];
            size_t tensor_size = bmrt_shape_count(&united_input_shapes[input_idx]) * bmrt_data_type_size(net_info->input_dtypes[input_idx]);
            auto tensor_mem = bm_mem_from_device(all_input_addr + in_offset, tensor_size);
            bmrt_tensor_with_device(&input_tensor, tensor_mem, net_info->input_dtypes[input_idx], united_input_shapes[input_idx]);
            in_offset += tensor_size;
          }
        } else {
          for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
            int devid = net_info->input_loc_devices[input_idx];
            auto &input_tensor = input_tensors[input_idx];
            bmrt_tensor_ex(&input_tensor, p_bmrt, devid, net_info->input_dtypes[input_idx], united_input_shapes[input_idx]);
          }
        }
        for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
          int devid = net_info->input_loc_devices[input_idx];
          auto &input_tensor = input_tensors[input_idx];
          size_t size = bmrt_tensor_bytesize(&input_tensor);
          std::vector<int8_t> host_data(size);
          BMRT_LOG(INFO, "reading input #%d, bytesize=%zu", input_idx, size);
          read_ref_data(host_data.data(), size, net_idx, true);
          print_array_ex(host_data.data(), bmrt_shape_count(&input_tensor.shape), net_info->input_dtypes[input_idx], "  --> input_data:");

          // dump input ref tensors in NCHW format into current directory
          if (OUTPUT_DEBUG) {
              string tensor_name = "input_ref_" + std::to_string(input_idx);
              vector<int> shape;
              bm_shape_t input_shape;
              input_shape = united_input_shapes[input_idx];
              for (int i = 0; i < input_shape.num_dims; i ++) {
                  shape.push_back(input_shape.dims[i]);
              }
              dump_tensor_all(tensor_name, host_data.data(), net_info->input_dtypes[input_idx], shape);
          }  // <== dump input ref tensors

          bm_memcpy_s2d(bm_handles[devid], input_tensor.device_mem, ((void *)host_data.data()));
        }

        std::vector<bm_profile_t> starts(core_lists.size()), ends(core_lists.size());
        std::vector<std::thread> threads(core_lists.size());
        for (auto &s : starts) memset(&s, 0, sizeof(bm_profile_t));
        for (auto &e : ends) memset(&e, 0, sizeof(bm_profile_t));

        bmrt_gettime(t2);
        for(size_t group_idx = 0; group_idx<core_lists.size(); group_idx++) {
          auto& core_list = core_lists[group_idx];
          bool pre_alloc_neuron_ret;
          if (memory_prealloc) {
            pre_alloc_neuron_ret =  bmrt_pre_alloc_mem_multi_thread(p_bmrt, group_idx, &mem_info_v[group_idx]);
          } else {
            pre_alloc_neuron_ret =  bmrt_pre_alloc_neuron_multi_cores(p_bmrt, net_info->name, stage_idx, core_list.data(), core_list.size());
          }
          if (!pre_alloc_neuron_ret) {
            std::string core_list_str = "";
            for (auto &core_id_ : core_list) { core_list_str += std::to_string(core_id_) + ","; }
            BMRT_LOG(FATAL, "net:%s pre_alloc_neuron failed, stage_idx:%d, core_list:%s", net_info->name, stage_idx, core_list_str.c_str());
          }
        }

        bmrt_gettime(t3);
        // calculate for CAL_TIMES times
        unsigned long total_elapsed = 0, npu_elapsed = 0, npu1_elapsed = 0;

        for (int t = 0; t < CAL_TIMES; t++) {
          if (chipid != 0x1682) {
            start_time();
          }

          int n = (MEM_NUM == 1) ? 0 : t;

          if (use_multi_thread) {
            for(size_t group_idx = 0; group_idx<core_lists.size(); group_idx++){
              auto& core_list = core_lists[group_idx];
              threads[group_idx] = std::move(std::thread(&bmrt_launch_tensor_thread_func,
                                                std::ref(bm_handle), p_bmrt, net_info->name, input_tensors.data(), net_info->input_num,
                                                output_tensors[n * core_lists.size() + group_idx].data(), net_info->output_num, true, false,
                                                core_list.data(), core_list.size(), net_idx, stage_idx,
                                                chipid, std::ref(launch_times[group_idx]), std::ref(starts[group_idx]), std::ref(ends[group_idx]),
                                                memory_prealloc, group_idx));
            }
            for (auto& thread: threads) {
              thread.join();
            }
          } else {
            for(size_t group_idx = 0; group_idx<core_lists.size(); group_idx++){
              auto& core_list = core_lists[group_idx];
              bmrt_launch_tensor_thread_func(bm_handle, p_bmrt, net_info->name, input_tensors.data(), net_info->input_num,
                                            output_tensors[n * core_lists.size() + group_idx].data(), net_info->output_num, true, false,
                                            core_list.data(), core_list.size(), net_idx, stage_idx,
                                            chipid, launch_times[group_idx], starts[group_idx], ends[group_idx], memory_prealloc, group_idx);
            }
          }

          /* TODO : exception now if bmodel only have cpu subnet */

          if (chipid != 0x1682) {
            total_elapsed += end_time();
            for (size_t i = 0; i < ends.size(); i++) {
              npu_elapsed += ends[i].tpu_process_time - starts[i].tpu_process_time;
              npu1_elapsed += ends[i].tpu1_process_time - starts[i].tpu1_process_time;
            }
          }
        }  // end continue calculate (add by nan.wu)

        bmrt_gettime(t4);

        // save neuron memory for debug
        if (EXPORT_NEURON && loop == 0) {
          save_neuron(p_bmrt, net_idx, stage_idx);
        }

        // memcpy output data from device to system
        for (int t = 0; t < REAL_MEM_NUM; t++) {
          for (int output_idx = 0; output_idx < net_info->output_num; ++output_idx) {
            auto &output_tensor = output_tensors[t][output_idx];
            if (bmrt_shape_count(&output_tensor.shape) >
                bmrt_shape_count(&stage.output_shapes[output_idx]) &&
                output_shapes.size() == 0) {
              // This log happen when we do not set shapes
              BMRT_LOG(FATAL,
                      "The %d-th neuron network '%s' stage '%d', get output shape[%d] failed",
                      net_idx, net_info->name, stage_idx, output_idx);
            }
            size_t out_size = bmrt_tensor_bytesize(&output_tensor);
            size_t out_ref_size = bmrt_shape_count(&stage.output_shapes[output_idx]) * bmrt_data_type_size(output_tensor.dtype);
            bm_memcpy_d2s_partial(bm_handle, host_output[t][output_idx].data(), output_tensor.device_mem,
                                  out_size);
            output_count[output_idx] = bmrt_shape_count(&output_tensor.shape);
            if (EXPORT_OUTPUT && loop == 0 && t == 0) {
              save_output(net_idx, host_output[0][output_idx].data(), out_ref_size);
            }

            // dump output tensors in NCHW format into current directory
            if (OUTPUT_DEBUG) {
                string tensor_name = "output_" + std::to_string(t) + "_" + std::to_string(output_idx);
                bm_shape_t output_shape = output_tensor.shape;
                vector<int> shape;
                for (int i = 0; i < output_shape.num_dims; i ++) {
                    shape.push_back(output_shape.dims[i]);
                }
                dump_tensor_all(tensor_name, host_output[t][output_idx].data(), output_tensor.dtype, shape);
            } // <== dump output tensors

          }
        }

        bmrt_gettime(t5);
        // and read ref output data
        std::vector<std::vector<int8_t>> ref_output(net_info->output_num);
        for (int output_idx = 0; output_idx < net_info->output_num; ++output_idx) {
          auto &output_tensor = output_tensors[0][output_idx];
          /* shape gap count = max shape count - real shape count */
          int elem_num = bmrt_shape_count(&output_tensor.shape);
          int shape_gap_count = output_shapes.size()? (bmrt_shape_count(&output_shapes[output_idx]) - elem_num) : (bmrt_shape_count(&stage.output_shapes[output_idx]) - elem_num);
          if (shape_gap_count < 0 && output_shapes.size() == 0) {
            // This log happen when we do not set shapes
            BMRT_LOG(FATAL, "The %d-th neuron network '%s' stage '%d', get output shape[%d] failed",
                    net_idx, net_info->name, stage_idx, output_idx);
          }
          size_t size = bmrt_tensor_bytesize(&output_tensor);
          ref_output[output_idx].resize(size);
          BMRT_LOG(INFO, "reading output #%d, bytesize=%zu", output_idx, size);
          read_ref_data(ref_output[output_idx].data(), size, net_idx, false, shape_gap_count * bmrt_data_type_size(output_tensor.dtype));
          print_array_ex(ref_output[output_idx].data(), elem_num, net_info->output_dtypes[output_idx], "  --> output ref_data:");

          // dump output ref tensors in NCHW format into current directory
          if (OUTPUT_DEBUG) {
              string tensor_name = "output_ref_" + std::to_string(output_idx);
              vector<int> shape;
              bm_shape_t output_shape = output_tensor.shape;
              for (int i = 0; i < output_shape.num_dims; i ++) {
                  shape.push_back(output_shape.dims[i]);
              }
              dump_tensor_all(tensor_name, ref_output[output_idx].data(), output_tensor.dtype, shape);
          }  // <== dump output ref tensors
        }

        if (chipid == 0x1682) {
          // get true performance time
          unsigned long long last_api_process_time_us = 0;
          #ifdef __linux__
          bm_get_last_api_process_time_us(bm_handle, (long unsigned int*)&last_api_process_time_us);
          #else
          bm_get_last_api_process_time_us(bm_handle, &last_api_process_time_us);
          #endif
          BMRT_LOG(INFO, "net[%s] stage[%d], the last_api_process_time_us is %lu us",
                   net_info->name, stage_idx, last_api_process_time_us);
        } else {
          if (total_elapsed < npu_elapsed) {
            total_elapsed = npu_elapsed;
          }
          npu_elapsed /= CAL_TIMES;
          npu1_elapsed /= CAL_TIMES;
          total_elapsed /= CAL_TIMES;
          unsigned long max_npu_time = max(npu_elapsed, npu1_elapsed);
          unsigned long cpu_time = total_elapsed - max_npu_time;

          auto launch_time = launch_times.front();
          if (use_multi_thread) {
            for (size_t i = 1; i < launch_times.size(); ++i) {
              #ifdef __linux__
              if (launch_times[i].tv_usec > launch_time.tv_usec) launch_time = launch_times[i];
              #else
              if (launch_times[i].tv_nsec > launch_time.tv_nsec) launch_time = launch_times[i];
              #endif
            }
          }
          unsigned long launch_time_us = bmrt_interval(t3, launch_time);
          unsigned long sync_time_us = bmrt_interval(launch_time, t4);
          if (core_lists.size() == 1){
              BMRT_LOG(INFO, "net[%s] stage[%d], launch total time is %lu us (npu %lu us, cpu %lu us), (launch func time %lu us, sync %lu us)",
                    net_info->name, stage_idx, total_elapsed, max_npu_time, cpu_time, launch_time_us, sync_time_us);
          } else {
              BMRT_LOG(INFO, "net[%s] stage[%d], launch total time is %lu us (npu %lu us [%lu us, %lu us], cpu %lu us), (launch func time %lu us, sync %lu us)",
                    net_info->name, stage_idx, total_elapsed, max_npu_time, npu_elapsed, npu1_elapsed, cpu_time, launch_time_us, sync_time_us);
          }
        }

        BMRT_LOG(INFO, "+++ The network[%s] stage[%d] output_data +++", net_info->name, stage_idx);
        for(int i=0; i<net_info->output_num; i++){
            string prefix = string("output data #")+std::to_string(i);
            auto out_tensor = output_tensors[0][i];
            auto num_elem = bmrt_shape_count(&out_tensor.shape);
            prefix += " shape: [";
            for (int j = 0; j < out_tensor.shape.num_dims; ++j) {
              prefix = prefix + std::to_string(out_tensor.shape.dims[j]) + " ";
            }
            prefix += "]";
            print_array_ex(host_output[0][i].data(), num_elem, net_info->output_dtypes[i], prefix.c_str());
        }
        if (NEED_CMP) {
          // compare inference output data with reference data
          BMRT_LOG(INFO, "==>comparing output in mem #0 ... ");
          bool failed = result_cmp(host_output[0], ref_output, output_count, net_info->output_dtypes);

          // compare multiple outputs from multiple infer mems, all of them should be bit level equal
          if(!failed && REAL_MEM_NUM>1){
            BMRT_LOG(INFO, "==> comparing multiple mems ... ");
            for (int output_idx = 0; output_idx < net_info->output_num; ++output_idx) {
              auto ref_output_data = host_output[0][output_idx].data();
              auto& ref_tensor = output_tensors[0][output_idx];
              int elem_num = output_count[output_idx];
              int byte_len = elem_num * bmrt_data_type_size(ref_tensor.dtype);
              std::string ref_info = std::string("mem")+std::to_string(0)+"_output" +std::to_string(output_idx);
              print_array_ex(ref_output_data, elem_num, ref_tensor.dtype, ref_info.c_str());
              for(int t = 1; t<REAL_MEM_NUM; t++){
                auto other_output = host_output[t][output_idx].data();
                int result = memcmp(ref_output_data, other_output, byte_len);
                if(result != 0){
                  if(!failed){
                    std::string filename = ref_info + ".dat";
                    save_data(filename.c_str(), other_output, byte_len);
                    failed = true;
                  }
                  BMRT_LOG(WARNING, "mem_idx=%d compare failed with mem 0!", t);
                  std::string output_info = std::string("mem")+std::to_string(t)+"_output" +std::to_string(output_idx);
                  print_array_ex(other_output, elem_num, ref_tensor.dtype, output_info.c_str());
                  auto filename = output_info + ".dat";
                  save_data(filename.c_str(), other_output, byte_len);
                }
              }
            }
          }

          if (!failed) {
            BMRT_LOG(INFO, "+++ The network[%s] stage[%d] cmp success +++", net_info->name, stage_idx);
          } else {
            bmrt_trace(p_bmrt);
            BMRT_LOG(FATAL, "+++ The network[%s] stage[%d] cmp failed +++", net_info->name, stage_idx);
          }
        }

        bmrt_gettime(t6);

        long use1 = bmrt_interval(t1, t2);
        long use2 = bmrt_interval(t2, t3);
        long use3 = bmrt_interval(t3, t4);
        long use4 = bmrt_interval(t4, t5);
        long use5 = bmrt_interval(t5, t6);

        BMRT_LOG(INFO, "load input time(s): %f", (float)use1 / 1000000);
        BMRT_LOG(INFO, "pre alloc  time(s): %f", (float)use2 / 1000000);
        BMRT_LOG(INFO, "calculate  time(s): %f", (float)use3 / 1000000);
        BMRT_LOG(INFO, "get output time(s): %f", (float)use4 / 1000000);
        BMRT_LOG(INFO, "compare    time(s): %f", (float)use5 / 1000000);

        // free memory
        if (net_info->addr_mode == ADDR_MODE_IO_TAG_FUSE) {
          int input_devid = net_info->input_loc_devices[0];
          int output_devid = net_info->output_loc_devices[0];
          bm_free_device(bm_handles[input_devid], all_input_device_mem);
          bm_free_device(bm_handles[output_devid], all_output_device_mem);
        } else {
          for (int i = 0; i < net_info->input_num; ++i) {
            int devid = net_info->input_loc_devices[i];
            bm_free_device(bm_handles[devid], input_tensors[i].device_mem);
          }
          for (int t = 0; t < REAL_MEM_NUM; t++) {
            for (int i = 0; i < net_info->output_num; ++i) {
              int devid = net_info->output_loc_devices[i];
              bm_free_device(bm_handles[devid], output_tensors[t][i].device_mem);
            }
          }
        }
        if (STAGE_SEL != -1) {
          fseek_ref_file_by_stage(stage_idx + 1, net_info->stage_num, net_idx, net_info);
          break;
        }
        if (shapes.size() > 0) {
          break;
        }
      } // stage_num
      if (NET_SEL != -1) {
        break;
      }
    } // net_num
  } // loopnum
  free(net_names);
  close_ref_file();
  if (PREALLOC_SIZE != 0) {
    bmrt_must_free_device_mem(p_bmrt, prealloc_mem);
  }
  for (int i = 0; i < prealloc_mem_v.size(); ++i) {
    bm_free_device(bm_handle, prealloc_mem_v[i]);
  }
  bmrt_destroy(p_bmrt);
  for (int i = 0; i < device_num; i++) {
    bm_dev_free(bm_handles[i]);
  }
}

extern void bmrt_test_case();

/* --------------------------------------------------------------------------*/
/* code for main process*/

vector<string> test_case_v = {
    // bmruntime new c interface
    "bmrt_load_bmodel_data",
    "bmrt_load_context",
    "bmrt_launch_data",
    "bmrt_simple_api",
    "bmrt_multi_thread",
    "bmrt_get_bmodel_api",
    "bmrt_get_bmodel_api_c",
    // bmruntime new c++ interface
    "bmcpp_load_bmodel",
    "bmcpp_load_bmodel_data",
    "bmcpp_reshape",
    "bmcpp_multi_thread",
    // bmtap2 c interface
    "bmtap2_register_bmodel",
    "bmtap2_register_data",
    "bmtap2_multi_thread",
    // bmtap2 c++ interface
    "bmtap2cpp_load_bmodel",
    "bmtap2cpp_multi_thread",
    // bmruntime multi-core interface
    "bmmc_multi_mession",
};

void Usage()
{
  printf(
      "Usage:\n"
      "  --version          : Show version.\n"
      "  --context_dir      : The dir of context after compilation.\n"
      "  --append_dir       : The dir of another context after compiltaion.\n"
      "  --bmodel           : The path of bmodel, just test bmodel, no compare.\n"
      "  --bmodel_list      : File that include one or more context dirs.\n"
      "  --devid            : The number of device.\n"
      "  --compare          : If 0, no result compare, else do compare.\n"
      "  --accuracy_f       : float compare accuracy.\n"
      "  --accuracy_i       : int compare accuracy.\n"
      "  --loopnum          : Set net launch loop times, one time as default.\n"
      "  --calculate_times  : Set net inference loop times, one time as default.\n"
      "  --thread_num       : Set net launch thread num\n"
      "  --prealloc         : Set alloc ion size (hex format) before load bmodel.\n"
      "  --export_neuron    : Export neuron mem to file.\n"
      "  --export_output    : Export output data to file.\n"
      "  --profile          : Open profile function.\n"
      "  --subnet_time      : Print each subnet time if have subnet.\n"
      "  --net_idx          : Select the net with net_idx to run.\n"
      "  --stage_idx        : Select the stage with stage_idx to run.\n"
      "  --debug_output     : Dump output data and reference data for debug.\n"
      "  --shapes           : Set shapes of the input shapes.\n"
      "  --output_shapes    : Set shapes of the output shapes.\n"
      "  --cascade_device   : Set devices to run for cascade model, e.g. 1,2\n"
      "  --core_list        : Set the core id list those will be used to inference\n"
      "                         e.g. 0,1 means using 0,1 core together to infer the multi-core compiled bmodel.\n"
      "                              0:1 means using 0,1 core to infer the single-core compiled bmodel with parallelly mession.\n"
      "  --memory_prealloc  : Memory alloc before load bmodel. Do not support multi bmodel. \n"
      "  --decrypt_lib      : Set decrypt_lib path for decrypt bmodel.\n"
#ifdef DEBUG
      "  --test_case        : Test api case, \n"
      "                       Option:\n"
#endif
      );
#ifdef DEBUG
  for (auto &test_case : test_case_v) {
    printf("                               %s\n", test_case.c_str());
  }
#endif
}


#ifndef __linux__
DEFINE_string(context_dir, "", "The dir of context after compilation.");
DEFINE_string(append_dir, "", "The dir of another context after compilation.");
DEFINE_string(bmodel, "", "The path of bmodel, just test bmodel, no compare.");
DEFINE_string(bmodel_list, "", "File that include one or more context dirs.");
DEFINE_int32(devid, 0, "The number of device.");
DEFINE_bool(compare, false, "If 0, no result compare, else do compare.");
DEFINE_double(accuracy_f, 1e-2, "Float compare accuracy.");
DEFINE_int32(accuracy_i, 0, "Int compare accuracy.");
DEFINE_int32(loopnum, 1, "Set net launch loop times, one time as default.");
DEFINE_int32(thread_num, 1, "Set net launch thread num.");
DEFINE_string(prealloc, "", "Set alloc ion size (hex format) before load bmodel.");
DEFINE_bool(export_neuron, false, "Export neuron mem to file.");
DEFINE_bool(export_output, false, "Export output data to file.");
DEFINE_bool(profile, false, "Open profile function.");
DEFINE_bool(subnet_time, false, "Print each subnet time if have subnet.");
DEFINE_int32(net_idx, -1, "Select the net with net_idx to run.");
DEFINE_int32(stage_idx, -1, "Select the stage with stage_idx to run.");
DEFINE_bool(debug_output, false, "Dump output data and reference data for debug.");
DEFINE_string(shapes, "", "Set shapes of the input shapes.");
DEFINE_string(output_shapes, "", "Set shapes of the output shapes.");
DEFINE_string(cascade_device, "", "Set devices to run for cascade model, e.g. 1,2");
DEFINE_bool(mmap, true, "Mmap");
DEFINE_int32(calculate_times, 1, "Calculate time.");
DEFINE_string(output_ref, "", "Output_ref");
DEFINE_bool(only_cmp_last, false, "only cmp last");
DEFINE_string(core_list, "", "");
DEFINE_bool(memory_prealloc, false, "Memory alloc before load bmodel.");
DEFINE_string(decrypt_lib, "", "Set decrypt_lib path for decrypt bmodel.");
DECLARE_bool(help);
#ifdef DEBUG
DEFINE_string(test_case, "", "Test api case");
#endif
#endif

static void check_options()
{
  if (BMODEL_PATH.empty() == false && CONTEXT_DIR_V.empty() == false) {
    BMRT_LOG(FATAL, "can't use dir and bmodel at the same time");
  }

  if (BMODEL_PATH.empty() == false) {
    CONTEXT_DIR_V.push_back(BMODEL_PATH);
    b_bmodel_dir = false;
  }

  if (CONTEXT_DIR_V.empty()) {
    BMRT_LOG(FATAL, "no context files");
  }
  if (false == TEST_CASE.empty()) {
    if (test_case_v.end() == std::find(test_case_v.begin(), test_case_v.end(), TEST_CASE)) {
      BMRT_LOG(FATAL, "%s is not a test case", TEST_CASE.c_str());
    }
  }
  if (LOOP_NUM < 1) {
    BMRT_LOG(FATAL, "loopnum should larger than 0");
  }
  if (THREAD_NUM < 1) {
    BMRT_LOG(FATAL, "thread num should larger than 0");
  }
}

static void read_bmodel_list()
{
  if (BMODEL_LIST.empty()) {
    return;
  }
  std::ifstream fin(BMODEL_LIST);
  if (!fin) {
    BMRT_LOG(FATAL, "bmodel_list[%s] open failed", BMODEL_LIST.c_str());
  }
  string line;
  while (fin >> line) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    CONTEXT_DIR_V.push_back(line);
  }
}

static vector<string> parseBrace(const string &str) {
    vector<string> dst;
    auto src = str;
    auto begin = src.find('[');
    while (begin != std::string::npos) {
        auto end = src.find(']');
        if (end != std::string::npos)
            dst.push_back(src.substr(begin + 1, end - begin + 1 - 2));
        src = src.substr(end + 1);
        begin = src.find('[');
    }
    return dst;
}

static vector<bm_shape_t> parseShapes(const string &str) {
    vector<string> dst_str = parseBrace(str);
    vector<bm_shape_t> dst;
    for (auto str_it : dst_str) {
        for (auto &it : str_it)
            if (it == ',')
                it = ' ';
        bm_shape_t shape;
        shape.num_dims = 0;
        std::istringstream out(str_it);
        string tmp;
        while (out >> tmp) {
            shape.dims[shape.num_dims] = std::stoi(tmp);
            shape.num_dims++;
        }
        dst.push_back(shape);
    }
    return dst;
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

static vector<int> parseCascadeDevices(const string &str) {
    vector<int> devices;
    vector<string> sub_str;
    split(str, ",", sub_str);
    for(auto &s :sub_str) {
      devices.push_back(std::atoi(s.c_str()));
    }
    return devices;
}

static vector<int32_t> parseList(const string &str) {
    vector<int32_t> dst;
    std::string dst_str(str);
    for (auto &iter : dst_str) {
      if (iter == ',')
        iter = ' ';
    }
    std::istringstream out(dst_str);
    string tmp;
    while (out >> tmp) {
        if (tmp == "," || tmp == " ")
            continue;
        dst.emplace_back(std::stoi(tmp));
    }
    return dst;
}

static vector<vector<int32_t>> parseMultipleList(const string &str) {
  std::vector<std::string> list_strings;
  split(str, ":", list_strings);
  std::vector<std::vector<int32_t>> results;
  for(auto& list_string: list_strings){
    auto list = parseList(list_string);
    results.push_back(list);
  }
  return results;
}

static void deal_with_options(int argc, char **argv)
{
  #ifdef __linux__
  int ch, lopt, option_index = 0;
  static struct option long_options[] = {{"context_dir", required_argument, NULL, 'd'},
                                         {"devid", required_argument, NULL, 'i'},
                                         {"help", no_argument, NULL, 'h'},
                                         {"version", no_argument, NULL, 'v'},
                                         {"test_case", required_argument, NULL, 't'},
                                         {"append_dir", required_argument, NULL, 'a'},
                                         {"compare", required_argument, NULL, 'c'},
                                         {"profile", no_argument, NULL, 'p'},
                                         {"mmap", required_argument, NULL, 'm'},
                                         {"loopnum", required_argument, NULL, 'l'},
                                         {"bmodel_list", required_argument, NULL, 'b'},
                                         {"prealloc", required_argument, &lopt, 1},
                                         {"subnet_time", no_argument, &lopt, 2},
                                         {"thread_num", required_argument, &lopt, 3},
                                         {"accuracy_f", required_argument, &lopt, 4},
                                         {"accuracy_i", required_argument, &lopt, 5},
                                         {"calculate_times", required_argument, &lopt, 6},
                                         {"export_neuron", no_argument, &lopt, 7},
                                         {"export_output", no_argument, &lopt, 8},
                                         {"output_ref", required_argument, &lopt, 9},
                                         {"only_cmp_last", no_argument, &lopt, 10},
                                         {"bmodel", required_argument, &lopt, 11},
                                         {"net_idx", required_argument, &lopt, 12},
                                         {"stage_idx", required_argument, &lopt, 13},
                                         {"debug_output", no_argument, &lopt, 14},
                                         {"shapes", required_argument, &lopt, 15},
                                         {"output_shapes", required_argument, &lopt, 16},
                                         {"cascade_device", required_argument, &lopt, 17},
                                         {"core_list", required_argument, &lopt, 18},
                                         {"memory_prealloc", no_argument, &lopt, 19},
                                         {"decrypt_lib", required_argument, &lopt, 20},
                                         {"use_runtime_share_mem", no_argument, &lopt, 21},
                                         {0, 0, 0, 0}};

  if (argc < 2) {
    Usage();
    exit(-1);
  }

  while ((ch = getopt_long(argc, argv, "d:i:ht:a:c:pm:l:b:", long_options, &option_index)) != -1) {
    switch (ch) {
      case 'd':
        CONTEXT_DIR_V.push_back(optarg);
        break;
      case 'a':
        CONTEXT_DIR_V.push_back(optarg);
        break;
      case 'i':
        DEV_ID = atoi(optarg);
        break;
      case 't':
        TEST_CASE = optarg;
        break;
      case 'b':
        BMODEL_LIST = optarg;
        break;
      case 'c':
        NEED_CMP = (atoi(optarg) != 0);
        break;
      case 'l':
        LOOP_NUM = atoi(optarg);
        break;
      case 'h':
        Usage();
        exit(-1);
        break;
      case 'v':
        std::cout << VER << std::endl;
        exit(0);
        break;
      case 'p':
        b_enable_profile = true;
        break;
      case 'm':
        b_enable_mmap = (atoi(optarg) != 0);
        break;
      case 0:
        switch (lopt) {
          case 1:
            PREALLOC_SIZE = strtoull(optarg, 0, 16);
            break;
          case 2:
            b_print_subnet_time = true;
            break;
          case 3:
            if(TEST_CASE.empty()) {
              TEST_CASE = "bmrt_multi_thread";
            }
            THREAD_NUM = atoi(optarg);
            break;
          case 4:
            DELTA_FLOAT = atof(optarg);
            break;
          case 5:
            DELTA_INT = atoi(optarg);
            break;
          case 6:
            CAL_TIMES = atoi(optarg);
            break;
          case 7:
            EXPORT_NEURON = true;
            break;
          case 8:
            EXPORT_OUTPUT = true;
            break;
          case 9:
            OUTPUT_REF_NAME = optarg;
            break;
          case 10:
            MEM_NUM = 1;
            break;
          case 11:
            BMODEL_PATH = optarg;
            NEED_CMP = false;
            MEM_NUM = 1;
            break;
          case 12:
            NET_SEL = atoi(optarg);
            break;
          case 13:
            STAGE_SEL = atoi(optarg);
            break;
          case 14:
            OUTPUT_DEBUG = true;
            break;
          case 15:
            shapes = parseShapes(optarg);
            break;
          case 16:
            output_shapes = parseShapes(optarg);
            break;
          case 17:
            devices = parseCascadeDevices(optarg);
            break;
          case 18:
            core_lists = parseMultipleList(optarg);
            break;
          case 19:
            memory_prealloc = true;
            break;
          case 20:
            DECRYPT_LIB = optarg;
            break;
          case 21:
            use_runtime_share_mem = true;
            break;
        }
        break;
      case '?':
        // unknown option
        BMRT_LOG(FATAL, "Unknown option %d", ch);
        break;
    }
  }
  read_bmodel_list();
  check_options();
  BMRT_LOG(INFO, "Loop num: %d", LOOP_NUM);
  #else
  gflags::SetUsageMessage(
      "command line brew\n"
      "usage: bmruntime <command> <args>\n\n"
      "commands:\n"
      "  --context_dir      : The dir of context after compilation.\n"
      "  --append_dir       : The dir of another context after compiltaion.\n"
      "  --bmodel           : The path of bmodel, just test bmodel, no compare.\n"
      "  --bmodel_list      : File that include one or more context dirs.\n"
      "  --devid            : The number of device.\n"
      "  --compare          : If 0, no result compare, else do compare.\n"
      "  --accuracy_f       : float compare accuracy.\n"
      "  --accuracy_i       : int compare accuracy.\n"
      "  --loopnum          : Set net launch loop times, one time as default.\n"
      "  --thread_num       : Set net launch thread num\n"
      "  --prealloc         : Set alloc ion size (hex format) before load bmodel.\n"
      "  --export_neuron    : Export neuron mem to file.\n"
      "  --export_output    : Export output data to file.\n"
      "  --profile          : Open profile function.\n"
      "  --subnet_time      : Print each subnet time if have subnet.\n"
      "  --net_idx          : Select the net with net_idx to run.\n"
      "  --stage_idx        : Select the stage with stage_idx to run.\n"
      "  --debug_output     : Dump output data and reference data for debug.\n"
      "  --shapes           : Set shapes of the input shapes.\n"
      "  --output_shapes    : Set shapes of the output shapes.\n"
      "  --core_list        : Set the core id list those will be used to inference.\n"
      "                         e.g. 0,1,2,3 means using 0,1,2,3 core to infer the bmodel.\n"
      "                              0,1:2,3 means using 0,1 core and 2,3 core to infer the bmodel parallelly.\n"
      "  --memory_prealloc  : Memory alloc before load bmodel. Do not support multi bmodel. \n"
      "  --decrypt_lib      : Set decrypt_lib path for decrypt bmodel.\n"
      "  --use_runtime_share_mem      : Use runtime share mem for multi core cases.\n"
    );
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_context_dir.empty() == false)
    CONTEXT_DIR_V.push_back(FLAGS_context_dir);
  if (FLAGS_append_dir.empty() == false)
  CONTEXT_DIR_V.push_back(FLAGS_append_dir);
  DEV_ID = FLAGS_devid;
  #ifdef DEBUG
  TEST_CASE = FLAGS_test_case;
  #endif
  BMODEL_LIST = FLAGS_bmodel_list;
  NEED_CMP = FLAGS_compare;
  LOOP_NUM = FLAGS_loopnum;
  b_enable_profile = FLAGS_profile;
  b_enable_mmap = FLAGS_mmap;
  PREALLOC_SIZE = strtoull(FLAGS_prealloc.c_str(), 0, 16);
  if (FLAGS_subnet_time) b_print_subnet_time = true;
  THREAD_NUM = FLAGS_thread_num;
  DELTA_FLOAT = FLAGS_accuracy_f;
  DELTA_INT = FLAGS_accuracy_i;
  CAL_TIMES = FLAGS_calculate_times;
  EXPORT_NEURON = FLAGS_export_neuron;
  EXPORT_OUTPUT = FLAGS_export_output;
  if (FLAGS_output_ref.empty() == false)
    OUTPUT_REF_NAME = FLAGS_output_ref;
  if (FLAGS_bmodel.empty() == false) {
    BMODEL_PATH = FLAGS_bmodel;
    NEED_CMP = false;
  }
  NET_SEL = FLAGS_net_idx;
  STAGE_SEL = FLAGS_stage_idx;
  if (FLAGS_debug_output) OUTPUT_DEBUG = true;
  shapes = parseShapes(FLAGS_shapes);
  output_shapes = parseShapes(FLAGS_output_shapes);
  core_lists = parseMultipleList(FLAGS_core_list);
  if (FLAGS_only_cmp_last) MEM_NUM = 1;
  if (!NEED_CMP) MEM_NUM = 1;
  if (FLAGS_memory_prealloc) memory_prealloc = true;
  gflags::HandleCommandLineHelpFlags();
  read_bmodel_list();
  check_options();
  BMRT_LOG(INFO, "Loop num: %d", LOOP_NUM);

  #endif
  if (core_lists.empty()) {
    core_lists = {{0}};
  }
}

static inline void init_log() {
  auto level = bmrt_get_current_log_level();
  if(level > BMRT_LogLevel::INFO) {
    bmrt_set_current_log_level(BMRT_LogLevel::INFO);
  }
}

int main(int argc, char **argv)
{
  init_log();
  deal_with_options(argc, argv);
  try {
    if (TEST_CASE.empty()) {
      bmrt_test();
    } else {
      bmrt_test_case();
    }
  } catch (const std::runtime_error &e) {
    return -1;
  }
  return 0;
}
