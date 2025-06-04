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

vector<string> CONTEXT_DIR_V;
string BMODEL_PATH;
string OUTPUT_REF_NAME = OUTPUT_REF_DATA;
bool NEED_CMP = true;
int DEV_ID = 0;
int LOOP_NUM = 1;
int LOOP_PERIOD_US = -1;
int LOOP_NBATCH = 1;
float LOOP_FPS = -1;
int DELTA_INT = COMPARE_FIX_ERR;
float DELTA_FLOAT = COMPARE_EPSILON;
bool b_enable_mmap = true;
bool b_bmodel_dir = true;

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

int result_cmp(const void *host_output_data, const void* ref_output_data, size_t len, bm_data_type_t dtype, int i)
{
  const char *info_label = "error comparing the last tensor: ";
  int flag = 0;
  switch (dtype) {
  case BM_INT32:
  case BM_UINT32:
      BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", i,
                  ((int *)(host_output_data))[0], ((int *)(ref_output_data))[0]);
      flag = array_cmp_fix32b((void *)(ref_output_data), (void *)(host_output_data),
                              dtype == BM_INT32 ? 1 : 0, len, info_label, DELTA_INT);
      break;
  case BM_INT16:
  case BM_UINT16:
      BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", i,
                  ((short *)(host_output_data))[0], ((short *)(ref_output_data))[0]);
      flag = array_cmp_fix16b((void *)(ref_output_data), (void *)(host_output_data),
                              dtype == BM_INT16 ? 1 : 0, len, info_label, DELTA_INT);
      break;
  case BM_INT8:
      BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", i,
                  ((char *)(host_output_data))[0], ((char *)(ref_output_data))[0]);
      flag = array_cmp_fix8b((void *)(ref_output_data), (void *)(host_output_data), 1, len, info_label, DELTA_INT);
      break;
  case BM_UINT8:
      BMRT_DEBUG("The %d-th tensor got[0] = %d, ref[0] = %d", i,
                  ((unsigned char *)(host_output_data))[0], ((unsigned char *)(ref_output_data))[0]);
      flag = array_cmp_fix8b((void *)(ref_output_data), (void *)(host_output_data), 0, len, info_label, DELTA_INT);
      break;
  default:
      BMRT_DEBUG("The %d-th tensor ", i);
      flag = array_cmp_float(ref_output_data, host_output_data, dtype, len,
                        info_label, DELTA_FLOAT);
      break;
  }
  if(flag){
      BMRT_LOG(WRONG, "comparing #%d output failed!", i);
  }
  return flag;
}

string fix_bmodel_path(const string& path) {
  if (b_bmodel_dir) {
    return path + "/compilation.bmodel";
  }
  return path;
}


static long long time_us()
{
  struct timeval time;
  gettimeofday(&time, NULL);
  long long elapsed = time.tv_sec * 1000000 + time.tv_usec;
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

void read_ref_data(void *host_data, size_t size, int net_idx, bool is_input = true, size_t gap_size = 0)
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

// set enable mmap flag in bmruntime
void set_bmrt_mmap(void *p_bmrt, bool enable)
{
  ((Bmruntime *)p_bmrt)->set_bmrt_mmap(enable);
}

/* --------------------------------------------------------------------------*/
/* code for inference by new nntoolchain runtime interface */

static void load_bmodel(const string &dir, void *p_bmrt)
{
  string bmodel_path = fix_bmodel_path(dir);
  bool flag = bmrt_load_bmodel(p_bmrt, bmodel_path.c_str());
  if (!flag) {
    BMRT_LOG(FATAL, "Load bmodel[%s] failed", bmodel_path.c_str());
  }
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

void bmrt_test_fps()
{
  // create bmruntime
  bm_handle_t bm_handle;
  bm_status_t status = bm_dev_request(&bm_handle, DEV_ID);
  if (BM_SUCCESS != status) {
    BMRT_LOG(FATAL, "bm_dev_request failed, id:[%d]",  DEV_ID);
  }

  void *p_bmrt = bmrt_create(bm_handle);

#ifdef SOC_MODE
  set_bmrt_mmap(p_bmrt, b_enable_mmap);
#endif

  // load bmodel by file
  for (auto &context_dir : CONTEXT_DIR_V) {
    context_info_t info;
    info.f_input_ref = NULL;
    info.f_output_ref = NULL;
    info.f_output = NULL;
    load_bmodel(context_dir, p_bmrt);

    open_ref_file(context_dir, info.f_input_ref, info.f_output_ref);
    // get network number
    info.net_num = bmrt_get_network_number(p_bmrt);
    info.dir = context_dir;
    context_info_v.push_back(info);
  }

  bmrt_show_neuron_network(p_bmrt);

  const char **net_names = NULL;
  bmrt_get_network_names(p_bmrt, &net_names);
  int net_idx = 0;
  int stage_idx = 0;
  BMRT_LOG(INFO, "==> running network #%d, name: %s, loop: %d", net_idx, net_names[net_idx], LOOP_NUM);
  auto net_info = bmrt_get_network_info(p_bmrt, net_names[net_idx]);
  std::vector<bm_tensor_t> output_tensors(net_info->output_num);
  auto &stage = net_info->stages[stage_idx];

  std::vector<std::vector<int8_t>> host_output(net_info->output_num);
  std::vector<std::vector<int8_t>> ref_output(net_info->output_num);

  for (int output_idx = 0; output_idx < net_info->output_num; output_idx++) {
    auto &output_tensor = output_tensors[output_idx];
    bmrt_tensor(&output_tensor, p_bmrt, net_info->output_dtypes[output_idx], stage.output_shapes[output_idx]);
    auto size = bmrt_tensor_bytesize(&output_tensor);
    host_output[output_idx].resize(size);
  }

        // prepare input tensor
  std::vector<bm_tensor_t> input_tensors(net_info->input_num);
  std::vector<std::vector<uint8_t>> input_data(net_info->input_num);

  for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
      auto &input_tensor = input_tensors[input_idx];
      bmrt_tensor(&input_tensor, p_bmrt, net_info->input_dtypes[input_idx], stage.input_shapes[input_idx]);
      size_t size = bmrt_tensor_bytesize(&input_tensor);
      auto& host_data = input_data[input_idx];
      host_data.resize(size);
      BMRT_LOG(INFO, "reading input #%d, bytesize=%zu", input_idx, size);
      read_ref_data(host_data.data(), size, net_idx, true);
      print_array_ex(host_data.data(), bmrt_shape_count(&input_tensor.shape), net_info->input_dtypes[input_idx], "  --> input_data:");
      // dump input ref tensors in NCHW format into current directory
      // bm_memcpy_s2d(bm_handle, input_tensor.device_mem, ((void *)host_data.data()));
  }

  bm_profile_t npu_start, npu_end;
  memset(&npu_start, 0, sizeof(bm_profile_t));
  memset(&npu_end, 0, sizeof(bm_profile_t));
  unsigned long long load_time = 0;
  unsigned long long infer_time = 0;
  unsigned long long fetch_time = 0;
  unsigned long long npu_time = 0;

  int batch = input_tensors[0].shape.dims[0];
  int overtime_count = 0;
  if (LOOP_FPS>0){
    LOOP_PERIOD_US = 1e6/LOOP_FPS * batch * LOOP_NBATCH;
    BMRT_LOG(INFO, "%dxbatch=%dx%d, fps=%.2f, loop_period=%dus", LOOP_NBATCH, batch, LOOP_FPS, LOOP_PERIOD_US);
  }

  int MAX_STAT_FRAME_COUNT = 32;

  int stat_frame_count = 0;
  long long loop_begin = time_us();
  long long stat_begin = loop_begin;
  long long host_infer_time = 0;
  long long tpu_s2d_time = 0;
  long long tpu_d2s_time = 0;
  long long sleep_time = 0;
  long long fixed_sleep = 0;
  long long tpu_stat_time = 0;
  for (int loop = 0; loop<LOOP_NUM; loop++){
    long long begin_usec = time_us();
    for(int n=0; n<LOOP_NBATCH; n++){
      for (int input_idx = 0; input_idx < net_info->input_num; input_idx++) {
        auto &input_tensor = input_tensors[input_idx];
        auto& host_data = input_data[input_idx];
        bm_memcpy_s2d(bm_handle, input_tensor.device_mem, ((void *)host_data.data()));
      }
    }

    long long before_infer = time_us();
    auto single_load_time = before_infer - begin_usec;
    load_time += single_load_time;
    tpu_s2d_time += single_load_time;

    bm_get_profile(bm_handle, &npu_start);
    for(int n=0; n<LOOP_NBATCH; n++){
      bool ret = bmrt_launch_tensor_ex(p_bmrt, net_names[net_idx], input_tensors.data(),
                                      net_info->input_num, output_tensors.data(), net_info->output_num, true, false);
      if (ret == false) {
        BMRT_LOG(FATAL, "The %d-th neuron network '%s' stage '%d' inference failed", net_idx, net_info->name, stage_idx);
      }
    }
    status = bm_thread_sync(bm_handle);
    if (BM_SUCCESS != status) {
      BMRT_LOG(FATAL, "The %d-th neuron network '%s' stage '%d' inference failed", net_idx, net_info->name, stage_idx);
    }
    bm_get_profile(bm_handle, &npu_end);
    auto single_npu_time = npu_end.tpu_process_time - npu_start.tpu_process_time;
    npu_time += single_npu_time;
    tpu_stat_time  += single_npu_time;

    // memcpy output data from device to system
    long long before_fetch = time_us();
    auto single_infer_time = before_fetch - before_infer;
    infer_time += single_infer_time;
    host_infer_time += single_infer_time;

    for(int n=0; n<LOOP_NBATCH; n++){
      for (int output_idx = 0; output_idx < net_info->output_num; ++output_idx) {
        auto &output_tensor = output_tensors[output_idx];
        bm_memcpy_d2s_partial(bm_handle, host_output[output_idx].data(), output_tensor.device_mem, host_output[output_idx].size());
      }
    }
    long long end_usec = time_us();
    auto single_fetch_time =  end_usec - before_fetch;
    fetch_time += single_fetch_time;
    tpu_d2s_time += single_fetch_time;
    stat_frame_count += batch * LOOP_NBATCH;

    long long sleep_us = 0;
    if(LOOP_PERIOD_US>0) {
      long long elapse_usec = end_usec - begin_usec;
      if(elapse_usec<LOOP_PERIOD_US){
        sleep_us = LOOP_PERIOD_US-elapse_usec - fixed_sleep;
        usleep(sleep_us);
        sleep_time += time_us() - end_usec;
        // fixed_sleep = time_us() - begin_usec;
      } else {
        overtime_count++;
      }
    }

    long long stat_interval = time_us() - stat_begin;
    if(stat_frame_count>=MAX_STAT_FRAME_COUNT){
      float tpu_fps = 1e6*stat_frame_count/tpu_stat_time;
      float loop_fps = 1e6*stat_frame_count/stat_interval;
      BMRT_LOG(INFO, "--> stat_interval: %lld us, frame = %d, loop_fps=%.2f, tpu_fps: %.2f, sleep: %lld us, host_infer: %lld us, tpu_infer: %lld, s2d: %lld us, d2s: %lld us",
               stat_interval, stat_frame_count, loop_fps, tpu_fps, sleep_time, host_infer_time, tpu_stat_time, tpu_s2d_time, tpu_d2s_time);
      stat_begin = time_us();
      host_infer_time = 0;
      tpu_s2d_time = 0;
      tpu_d2s_time = 0;
      sleep_time = 0;
      stat_frame_count = 0;
      tpu_stat_time = 0;
    }
    BMRT_LOG(WARNING, "--> single: sleep: %lld us, host_infer: %lld us, s2d: %lld us, d2s: %lld us, tpu_infer %lld us",
             sleep_us, single_infer_time, single_load_time, single_fetch_time, single_npu_time);
  }
  long long loop_end = time_us();

  for (int output_idx = 0; output_idx < net_info->output_num; ++output_idx) {
    auto &output_tensor = output_tensors[output_idx];
    /* shape gap count = max shape count - real shape count */
    size_t size = bmrt_tensor_bytesize(&output_tensor);
    auto dtype = net_info->input_dtypes[output_idx];
    int dsize = bmrt_data_type_size(dtype);
    int elem_num = size/dsize;

    if(NEED_CMP){
      ref_output[output_idx].resize(size);
      BMRT_LOG(INFO, "reading output ref: #%d, bytesize=%zu", output_idx, size);
      read_ref_data(ref_output[output_idx].data(), size, net_idx, false, 0);
      print_array_ex(ref_output[output_idx].data(), elem_num, net_info->output_dtypes[output_idx], "  --> output ref_data:");
      BMRT_LOG(INFO, "==>comparing #%d output ... ", output_idx);
      int flag = result_cmp(host_output[output_idx].data(), ref_output[output_idx].data(), elem_num, dtype, output_idx);
      if (flag == 0) {
        BMRT_LOG(INFO, "+++ The network[%s] stage[%d] cmp success +++", net_info->name, stage_idx);
      } else {
        BMRT_LOG(FATAL, "+++ The network[%s] stage[%d] cmp failed +++", net_info->name, stage_idx);
      }
    } else {
      print_array_ex(host_output[output_idx].data(), elem_num, dtype, "  --> output data:");
    }
  }

  unsigned total_elapsed = infer_time /LOOP_NUM;
  unsigned npu_elapsed = npu_time/LOOP_NUM;

  BMRT_LOG(INFO, "net[%s] stage[%d], launch total time is %lu us (npu %lu us, cpu %lu us)",
          net_info->name, stage_idx, total_elapsed, npu_elapsed, total_elapsed - npu_elapsed);
      // compare inference output data with reference data
  float ratio = LOOP_NUM*1e6;
  BMRT_LOG(INFO, "load input time(s): %f", load_time/ratio);
  BMRT_LOG(INFO, "calculate  time(s): %f", infer_time/ratio);
  BMRT_LOG(INFO, "get output time(s): %f", fetch_time/ratio);

  BMRT_LOG(INFO, "max batch per second: %f", ratio/(load_time+infer_time+fetch_time));
  BMRT_LOG(INFO, "real batch per second: %f", ratio/(loop_end - loop_begin));
  BMRT_LOG(INFO, "overtime ratio: %d/%d, period: %d us", overtime_count, LOOP_NUM, LOOP_PERIOD_US);

  // free memory
  for (int i = 0; i < net_info->input_num; ++i) {
    bmrt_must_free_device_mem(p_bmrt, input_tensors[i].device_mem);
  }
  for (int i = 0; i < net_info->output_num; ++i) {
    bmrt_must_free_device_mem(p_bmrt, output_tensors[i].device_mem);
  }
  free(net_names);
  close_ref_file();
  bmrt_destroy(p_bmrt);
  bm_dev_free(bm_handle);
}

void Usage()
{
  printf(
      "Usage:\n"
      "  --version          : Show version.\n"
      "  --context_dir      : The dir of context after compilation.\n"
      "  --bmodel           : The path of bmodel, just test bmodel, no compare.\n"
      "  --devid            : The number of device.\n"
      "  --nbatch           : Run nbatch x model_batch frame as a loop.\n"
      "  --compare          : If 0, no result compare, else do compare.\n"
      "  --loopnum          : Set net launch loop times, one time as default.\n"
      "  --period           : Set net launch loop period, default -1 means as fast as possible.\n"
      "  --fps:             : Set simulated fps, loop period will be caculated automatically, default -1 means as fast as possible.\n"
      );
}


static void check_options()
{
  if (BMODEL_PATH.empty() == false && CONTEXT_DIR_V.empty() == false) {
    BMRT_LOG(FATAL, "can't use dir and bmodel at the same time");
  }

  if (BMODEL_PATH.empty() == false) {
    CONTEXT_DIR_V.push_back(BMODEL_PATH);
    b_bmodel_dir = false;
    NEED_CMP = false;
  }

  if (CONTEXT_DIR_V.empty()) {
    BMRT_LOG(FATAL, "no context files");
  }
  if (LOOP_NUM < 1) {
    BMRT_LOG(FATAL, "loopnum should larger than 0");
  }
}

static void deal_with_options(int argc, char **argv)
{
  int ch, option_index = 0;
  static struct option long_options[] = {{"context_dir", required_argument, NULL, 'd'},
                                         {"devid", required_argument, NULL, 'i'},
                                         {"help", no_argument, NULL, 'h'},
                                         {"version", no_argument, NULL, 'v'},
                                         {"compare", required_argument, NULL, 'c'},
                                         {"mmap", required_argument, NULL, 'm'},
                                         {"loopnum", required_argument, NULL, 'l'},
                                         {"nbatch", required_argument, NULL, 'n'},
                                         {"period", required_argument, NULL, 'p'},
                                         {"bmodel", required_argument, NULL, 'b'},
                                         {"fps", required_argument, NULL, 'f'},
                                         {0, 0, 0, 0}};

  if (argc < 2) {
    Usage();
    exit(-1);
  }

  while ((ch = getopt_long(argc, argv, "d:i:hvc:f:p:m:l:b:n:", long_options, &option_index)) != -1) {
    switch (ch) {
      case 'd':
        CONTEXT_DIR_V.push_back(optarg);
        break;
      case 'i':
        DEV_ID = atoi(optarg);
        break;
      case 'b':
        BMODEL_PATH = optarg;
        break;
      case 'c':
        NEED_CMP = (atoi(optarg) != 0);
        break;
      case 'l':
        LOOP_NUM = atoi(optarg);
        break;
      case 'f':
        LOOP_FPS = (float)atof(optarg);
        break;
      case 'p':
        LOOP_PERIOD_US = atoi(optarg);
        break;
      case 'n':
        LOOP_NBATCH = atoi(optarg);
        break;
      case 'h':
        Usage();
        exit(-1);
        break;
      case 'v':
        std::cout << VER << std::endl;
        exit(0);
        break;
      case 'm':
        b_enable_mmap = (atoi(optarg) != 0);
        break;
      default:
        // unknown option
        BMRT_LOG(FATAL, "Unknown option %d", ch);
        break;
    }
  }
  check_options();
  BMRT_LOG(INFO, "Loop num: %d", LOOP_NUM);
}

int main(int argc, char **argv)
{
  deal_with_options(argc, argv);
  try {
    bmrt_test_fps();
  } catch (const std::runtime_error &e) {
    return -1;
  }
  return 0;
}
