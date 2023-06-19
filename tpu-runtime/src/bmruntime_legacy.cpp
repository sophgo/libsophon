/* after legacy interface not used, this file can be removed */

#include "bmruntime.h"
#include "bmruntime_common.h"
#include "bmruntime_interface.h"
#include "string.h"

using bmruntime::bmfunc;
using bmruntime::Bmruntime;

// Make sure net_idx is greater than or equal to zero
#define CHECK_net_idx(net_idx)                                                    \
  do{                                                                             \
        BMRT_ASSERT_INFO(net_idx >= 0,"net_idx:%d shouldn't less than 0",net_idx);\
  }while(0)
/* pbm_handle : pointer to bm_handle_t */
static void chip_check(const std::string& chip_name, unsigned int chipid)
{
  if ((chipid == 0x1682 && chip_name.find("BM1682") != std::string::npos) ||
      (chipid == 0x1684 && chip_name.find("BM1684") != std::string::npos) ) {
  } else {
    BMRT_LOG(FATAL, "Error: chipid %x and chipname %s are not match", chipid, chip_name.c_str());
  }
}

void* create_bmrt_helper(void* pbm_handle, const char* chip_type, int devid) {

  Bmruntime* p_bmrt = NULL;
  const std::string chip_name = chip_type;
  try {
  if (pbm_handle)
    p_bmrt = new Bmruntime((bm_handle_t*)pbm_handle, true, chip_name);
  else
    p_bmrt = new Bmruntime(chip_name, devid);

  //chipid
  unsigned int chipid = 0;
  bm_handle_t handle = (bm_handle_t)bmrt_get_bm_handle(p_bmrt);
  if (0 != bm_get_chipid(handle, &chipid)) {
    BMRT_LOG(FATAL, "Cannot get chipid");
  }
  chip_check(chip_name, chipid);
  } catch (const std::runtime_error &e) {
      if (p_bmrt)
          delete p_bmrt;
      p_bmrt = NULL;
  }

  return (void*)p_bmrt;
}

void destroy_bmruntime(void* p_bmrt) { delete (Bmruntime*)p_bmrt; }

void* bmrt_get_bm_handle(void* p_bmrt) { return (void*)(((Bmruntime*)p_bmrt)->get_bm_handle()); }

bool bmrt_load_context(void* p_bmrt, const char* context_dir) {
  return ((Bmruntime*)p_bmrt)->load_context(context_dir);
}

bool bmrt_get_input_tensor(void* p_bmrt, int net_idx, const char* net_name, int* input_num,
                           const char*** input_names) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  return ((Bmruntime*)p_bmrt)->get_input_tensor(net_idx, input_num, input_names);
}

bool bmrt_get_output_tensor(void* p_bmrt, int net_idx, const char* net_name, int* output_num,
                            const char*** output_names) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  return ((Bmruntime*)p_bmrt)->get_output_tensor(net_idx, output_num, output_names);
}

typedef enum { DATA_IN_AIR = 0x00, DATA_IN_SYS = 0x01, DATA_IN_DEV = 0x10 } BMRT_DATA_POS_T;

typedef struct bmrt_launch_param {
  int net_idx;
  char* net_name;

  BMRT_DATA_POS_T input_pos;  /* input data position */
  BMRT_DATA_POS_T output_pos; /* output data position */
  int input_num;
  int output_num;
  void* input_tensors;
  void* output_tensors;
  int* in_stmode;
  int* out_stmode;
  // raw input shapes
  int* input_shapes;
  int* input_dims;
  int* input_type_lens;

  // output info
  bm_shape_t * output_shapes;
  bool output_shape_need_free;
  int* output_type_lens;

  // inner param
  int* output_max_bytelen;
  const char** input_names;
  const char** output_names;
  bm_device_mem_t* input_mem;
  bm_device_mem_t* output_mem;

} BMRT_LAUNCH_PARAM_T;

static void init_launch_param(BMRT_LAUNCH_PARAM_T* plaunch_param) {
  memset(plaunch_param, 0, sizeof(*plaunch_param));
}

int bmrt_map_data_to_device(void* p_bmrt, BMRT_LAUNCH_PARAM_T* plaunch_param, int split_n,
                            int nidx,int need_split,bm_device_mem_t* input_mem_for_dynamic,bm_device_mem_t* output_mem_for_dynamic,u64* output_addr_offset) {
  BMRT_DATA_POS_T input_pos = plaunch_param->input_pos;
  BMRT_DATA_POS_T output_pos = plaunch_param->output_pos;
  int net_idx = plaunch_param->net_idx;
  const char* net_name = plaunch_param->net_name;
  void* input_tensors = plaunch_param->input_tensors;
  void* output_tensors = plaunch_param->output_tensors;
  int input_num = plaunch_param->input_num;
  int output_num = plaunch_param->output_num;
  int* in_shapes = plaunch_param->input_shapes;
  int* in_dims = plaunch_param->input_dims;
  int* in_stmode = plaunch_param->in_stmode;
  int* in_type_lens = plaunch_param->input_type_lens;
  int* out_stmode = plaunch_param->out_stmode;
  int* out_type_lens = plaunch_param->output_type_lens;

  /* get input device memory */
  BMRT_ASSERT_INFO(input_pos == DATA_IN_SYS || input_pos == DATA_IN_DEV,\
    "input_pos: 0x%x shoud be DATA_IN_SYS:0x01 or DATA_IN_DEV:0x10",input_pos);
  bm_device_mem_t* input_mem = NULL;
  int shape_offset = 0;
  u32 byte_len = 0;
  int n_align_num = 1;
  int block_num, block_size;
  int need_alloc = plaunch_param->input_mem == NULL;
  int real_n = split_n;
  int real_nidx = nidx;
  if (input_pos == DATA_IN_SYS) {
    input_mem = need_alloc ? (new bm_device_mem_t[input_num]) : plaunch_param->input_mem;
    float* p_in_tensor = (float*)(input_tensors);
    for (int i = 0; i < input_num; ++i) {
      real_n = (in_shapes[shape_offset] == 1) ? 1 : split_n;
      real_nidx = (in_shapes[shape_offset] == 1) ? 0 : nidx;
      if (in_stmode != NULL && in_stmode[i] == BM_STORE_4N) {
        BMRT_ASSERT_INFO((real_nidx & 0x3) == 0,"real_nidx:0x%x should be 0x00",\
          real_nidx);  // make assure 4N edge aligned
        n_align_num = 4;
      } else {
        n_align_num = 1;
      }
      shape_to_nblock(&in_shapes[shape_offset], in_dims[i], &block_num, &block_size);
      byte_len = ALIGN(real_n, n_align_num) * block_size * in_type_lens[i];
      if (need_alloc) {
        bm_status_t status = bm_malloc_device_byte(((Bmruntime*)p_bmrt)->get_bm_handle(), &input_mem[i],byte_len);
        CHECK_status(status);/* TODO: consider int 8 data type ? */
      }
      bm_memcpy_s2d_partial(((Bmruntime*)p_bmrt)->get_bm_handle(), input_mem[i], p_in_tensor + real_nidx * block_size,
                               byte_len);
      p_in_tensor += block_num * block_size * in_type_lens[i];  // user data addr calculation
      shape_offset += in_dims[i];
    }
    BMRT_DEBUG("s2d success!");
  } else { /* do not consider batch num */
    if( need_split ){
      for(int i = 0; i < input_num; ++i){
        input_mem_for_dynamic[i] = ((bm_device_mem_t*)(input_tensors))[i];
        shape_to_nblock(&in_shapes[shape_offset], in_dims[i], &block_num, &block_size);
        n_align_num = (in_stmode != NULL && in_stmode[i] == BM_STORE_4N) ? 4 : 1;
        byte_len = ALIGN(nidx, n_align_num) * block_size * in_type_lens[i];

        u64 addr = bm_mem_get_device_addr(((bm_device_mem_t *)input_tensors)[i]);
        bm_mem_set_device_addr(&input_mem_for_dynamic[i],addr + byte_len);
      }
      input_mem =  input_mem_for_dynamic;
    } else
      input_mem =  (bm_device_mem_t*)input_tensors;
  }

  /* get output device memory */
  BMRT_ASSERT_INFO(output_pos == DATA_IN_SYS || output_pos == DATA_IN_DEV,\
    "output_pos: 0x%x shoud be DATA_IN_SYS:0x01 or DATA_IN_DEV:0x10",output_pos);
  bm_device_mem_t* output_mem = plaunch_param->output_mem;
  if (output_pos == DATA_IN_SYS && output_mem == NULL) {
    output_mem = new bm_device_mem_t[output_num];
    int out_shape[BM_MAX_DIMS_NUM];
    int out_dims;
    const char** out_tensor_names = plaunch_param->output_names;
    plaunch_param->output_max_bytelen = new int[input_num];
    for (int i = 0; i < output_num; ++i) {
      // malloc as max shape
      out_dims =
          bmrt_get_output_blob_max_shape(p_bmrt, out_tensor_names[i], net_idx, net_name, out_shape);
      n_align_num = (out_stmode != NULL && out_stmode[i] == BM_STORE_4N) ? 4 : 1;
      byte_len = get_element_num(out_shape, out_dims, n_align_num) * out_type_lens[i];
      bm_status_t status = bm_malloc_device_byte(((Bmruntime*)p_bmrt)->get_bm_handle(),&output_mem[i], byte_len);
      CHECK_status(status);/* TODO: consider int 8 data type ? */
      plaunch_param->output_max_bytelen[i] = byte_len;
    }
    free(out_tensor_names);
  } else {
    if( need_split ){
      for(int i = 0; i < output_num; ++i){
        output_mem_for_dynamic[i] = ((bm_device_mem_t*)(output_tensors))[i];
        u64 addr = bm_mem_get_device_addr(((bm_device_mem_t *)output_tensors)[i]);
        bm_mem_set_device_addr(&output_mem_for_dynamic[i],addr + output_addr_offset[i]);
      }
      output_mem =  output_mem_for_dynamic;
    }else
      output_mem = (bm_device_mem_t*)output_tensors;
  }

  plaunch_param->input_mem = input_mem;
  plaunch_param->output_mem = output_mem;
  return 0;
}

int bmrt_unmap_data_from_device(void* p_bmrt, BMRT_LAUNCH_PARAM_T* plaunch_param, int split_n,
                                int nidx, bool need_split, bool shape_changed,u64 *output_addr_offset) {
  BMRT_DATA_POS_T output_pos = plaunch_param->output_pos;
  void* output_tensors = plaunch_param->output_tensors;
  int output_num = plaunch_param->output_num;
  bm_shape_t* out_shapes = plaunch_param->output_shapes;
  int* out_type_lens = plaunch_param->output_type_lens;
  int* out_stmode = plaunch_param->out_stmode;

  bm_device_mem_t* output_mem = plaunch_param->output_mem;

  int n_align_num;
  int block_num, block_size;
  for (int i = 0; i < output_num; ++i) {
    if (shape_changed) {
      n_align_num = (out_stmode != NULL && out_stmode[i] == BM_STORE_4N) ? 4 : 1;
      shape_to_nblock(out_shapes[i].dims, out_shapes[i].num_dims, &block_num, &block_size);
      output_addr_offset[i] = ALIGN((nidx + split_n), n_align_num) * block_size * out_type_lens[i];
    }
    BMRT_ASSERT_INFO(!need_split || out_shapes[i].dims[0] == split_n,\
    "need_spilt should be false or out_shapes[%d].dims[0]:%d should be equal to split_n:%d",\
    i,out_shapes[i].dims[0],split_n);
  }
  if (output_pos == DATA_IN_SYS) { /* sync output data to host */
    float* p_out_tensor = (float*)(output_tensors);
    u32 byte_len;
    for (int i = 0; i < output_num; ++i) {
      n_align_num = (out_stmode != NULL && out_stmode[i] == BM_STORE_4N) ? 4 : 1;
      shape_to_nblock(out_shapes[i].dims, out_shapes[i].num_dims, &block_num, &block_size);
      byte_len = ALIGN(block_num, n_align_num) * out_type_lens[i];
      bm_memcpy_d2s_partial(((Bmruntime*)p_bmrt)->get_bm_handle(), p_out_tensor + nidx * block_size, output_mem[i], byte_len);
      p_out_tensor += plaunch_param->output_max_bytelen[i];
    }
    BMRT_DEBUG("d2s success!");
  }
  return 0;
}

static inline void prepare_launch_param(void* p_bmrt, BMRT_LAUNCH_PARAM_T* plaunch_param) {
  int net_idx = plaunch_param->net_idx;
  char* net_name = plaunch_param->net_name;
  int input_num, output_num;
  bmrt_get_input_tensor(p_bmrt, net_idx, net_name, &input_num, &plaunch_param->input_names);
  BMRT_ASSERT_INFO(input_num == plaunch_param->input_num,\
    "input_num:%d should be equal to plaunch_param->input_num:%d",\
    input_num,plaunch_param->input_num);
  bmrt_get_output_tensor(p_bmrt, net_idx, net_name, &output_num, &plaunch_param->output_names);
  BMRT_ASSERT_INFO(output_num == plaunch_param->output_num,\
    "output_num:%d should be equal to plaunch_param->input_num:%d"\
    ,output_num,plaunch_param->output_num);
  // update real output shapes
  if(plaunch_param->output_shapes == NULL) {
    plaunch_param->output_shapes = new bm_shape_t[output_num];
    plaunch_param->output_shape_need_free = true;
  } else {
    plaunch_param->output_shape_need_free = false;
  }
}

static inline void release_launch_param(void* p_bmrt, BMRT_LAUNCH_PARAM_T* plaunch_param) {
  free(plaunch_param->input_names);
  free(plaunch_param->output_names);
  if (plaunch_param->input_pos == DATA_IN_SYS) {
    int input_num = plaunch_param->input_num;
    for (int i = 0; i < input_num; i++) {
      bm_free_device(((Bmruntime*)p_bmrt)->get_bm_handle(), plaunch_param->input_mem[i]);
    }
    free(plaunch_param->input_mem);
    plaunch_param->input_mem = NULL;
  }
  if (plaunch_param->output_pos == DATA_IN_SYS) {
    int output_num = plaunch_param->output_num;
    for (int i = 0; i < output_num; i++) {
      bm_free_device(((Bmruntime*)p_bmrt)->get_bm_handle(), plaunch_param->output_mem[i]);
    }
    free(plaunch_param->output_mem);
    plaunch_param->output_mem = NULL;
  }
  if (plaunch_param->output_max_bytelen) delete[] plaunch_param->output_max_bytelen;
  if (plaunch_param->output_shape_need_free) delete[] plaunch_param->output_shapes;
}

bool bmrt_launch_core(void* p_bmrt, BMRT_LAUNCH_PARAM_T* plaunch_param) {
  const char* net_name = plaunch_param->net_name;
  int net_idx = (net_name && strlen(net_name)) ? ((Bmruntime*)p_bmrt)->get_net_idx(net_name)
                                               : plaunch_param->net_idx;
  // Make sure net_idx is greater than or equal to zero
  CHECK_net_idx(net_idx);

  prepare_launch_param(p_bmrt, plaunch_param);
  int input_num = plaunch_param->input_num;
  int output_num = plaunch_param->output_num;
  int* in_shapes = plaunch_param->input_shapes;
  int* in_dims = plaunch_param->input_dims;

  int* in_stmode = plaunch_param->in_stmode;
  int* out_stmode = plaunch_param->out_stmode;

  /* max_n per launch */
  bool batch_can_change = bmrt_can_batch_size_change(p_bmrt, net_idx, net_name);
  int max_n;
  int total_batch_num = in_shapes[0];
  /* check all the inputs must have same n, then batch num can change */
  if (batch_can_change) {
    int shape_offset = 0;
    for (int i = 0; i < input_num; i++) {
      if (in_shapes[shape_offset] != total_batch_num && in_shapes[shape_offset] != 1) {
        BMRT_LOG(WARNING, "[WARNING] Not all the inputs have the same batch number, so it cannot change!");
        batch_can_change = false;
        break;
      }
      shape_offset += in_dims[i];
    }
  }

  if (batch_can_change) {
    int max_shape[BM_MAX_DIMS_NUM];
    bmrt_get_input_blob_max_shape(p_bmrt, plaunch_param->input_names[0], net_idx, net_name,
                                  max_shape);
    max_n = max_shape[0];
  } else
    max_n = total_batch_num; /* todo */

  /* launch */
  int need_split = max_n < total_batch_num;
  int* real_shapes;
  if (need_split) {
    int total_len = 0;
    for (int i = 0; i < input_num; i++) total_len += in_dims[i];
    real_shapes = new int[total_len];
    memcpy(real_shapes, in_shapes, total_len * sizeof(int));
  } else {
    real_shapes = plaunch_param->input_shapes;
  }

  bool shape_changed;
  bm_device_mem_t* input_mem_for_dynamic = new bm_device_mem_t[input_num];
  bm_device_mem_t* output_mem_for_dynamic = new bm_device_mem_t[output_num];
  u64* output_addr_offset = new u64 [output_num]();
  for (int nidx = 0, current_n = 0; nidx < total_batch_num; nidx += current_n) {
    #ifdef __linux__
    current_n = std::min(max_n, total_batch_num - nidx);
    #else
    current_n = (std::min)(max_n, total_batch_num - nidx);
    #endif
    shape_changed = current_n != total_batch_num;
    if (need_split && shape_changed) {
      int shape_offset = 0;
      for (int i = 0; i < input_num; i++) {
        real_shapes[shape_offset] = current_n;
        shape_offset += in_dims[i];
      }
    }
    bmrt_map_data_to_device(p_bmrt, plaunch_param, current_n, nidx,need_split,input_mem_for_dynamic,output_mem_for_dynamic,output_addr_offset);
    bool is_success =
        ((Bmruntime*)p_bmrt)
            ->launch(net_idx, input_num, plaunch_param->input_mem, real_shapes, in_dims, in_stmode,
                     output_num, plaunch_param->output_mem, out_stmode, plaunch_param->output_shapes);
    if (!is_success) {
      BMRT_LOG(WRONG, "launch failed for nidx=%d, n=%d", nidx, current_n);
      return false;
    }
    BMRT_LOG(INFO, "launch success for nidx=%d, n=%d", nidx, current_n);
    bmrt_unmap_data_from_device(p_bmrt, plaunch_param, current_n, nidx, need_split, shape_changed,output_addr_offset);
  }
  if (need_split) {
    delete[] real_shapes;
    for (int i = 0; i < output_num; i++) {
      plaunch_param->output_shapes[i].dims[0] = total_batch_num;
    }
  }
  release_launch_param(p_bmrt, plaunch_param);
  delete [] input_mem_for_dynamic;
  delete [] output_mem_for_dynamic;
  delete [] output_addr_offset;
  return true;
}

bool bmrt_launch_cpu_data(void* p_bmrt, int net_idx, const char* net_name, void* input_tensors,
                          int input_num, void* output_tensors, int output_num, const int* in_shape,
                          bm_shape_t* out_shapes) {
  BMRT_LAUNCH_PARAM_T launch_param;
  init_launch_param(&launch_param);

  launch_param.net_idx = net_idx;
  launch_param.net_name = const_cast<char*>(net_name);
  launch_param.input_shapes = const_cast<int*>(in_shape);
  launch_param.output_shapes = out_shapes;
  launch_param.input_num = input_num;
  launch_param.output_num = output_num;
  launch_param.input_tensors = input_tensors;
  launch_param.output_tensors = output_tensors;
  launch_param.input_pos = DATA_IN_SYS;
  launch_param.output_pos = DATA_IN_SYS;

  int shape_dims = 4;
  int type_len = sizeof(float);
  launch_param.input_dims = new int[input_num];
  launch_param.input_type_lens = new int[input_num];
  for (int i = 0; i < input_num; i++) {
    launch_param.input_dims[i] = shape_dims;
    launch_param.input_type_lens[i] = type_len;
  }
  launch_param.output_type_lens = new int[output_num];
  for (int i = 0; i < output_num; i++) {
    launch_param.output_type_lens[i] = type_len;
  }

  bool res = bmrt_launch_core(p_bmrt, &launch_param);
  delete[] launch_param.input_dims;
  delete[] launch_param.input_type_lens;
  delete[] launch_param.output_type_lens;
  return res;
}

bool bmrt_launch_shape_stmode(void* p_bmrt, int net_idx, const char* net_name,
                              const void* input_tensors, int input_num, const int* input_dim,
                              const int* input_shapes, const void* output_tensors, int output_num,
                              int* in_stmode, int* out_stmode, bm_shape_t * output_shapes) {
  BMRT_LAUNCH_PARAM_T launch_param;
  init_launch_param(&launch_param);

  launch_param.net_idx = net_idx;
  launch_param.net_name = const_cast<char*>(net_name);
  launch_param.input_shapes = const_cast<int*>(input_shapes);
  launch_param.input_dims = const_cast<int*>(input_dim);
  launch_param.input_num = input_num;
  launch_param.output_num = output_num;
  launch_param.input_tensors = const_cast<void*>(input_tensors);
  launch_param.output_tensors = const_cast<void*>(output_tensors);
  launch_param.in_stmode = in_stmode;
  launch_param.out_stmode = out_stmode;
  launch_param.input_pos = DATA_IN_DEV;
  launch_param.output_pos = DATA_IN_DEV;
  launch_param.output_shapes = output_shapes;

  launch_param.input_type_lens = new int[input_num];
  for (int i = 0; i < input_num; i++) {
    int type_len = (in_stmode != NULL && in_stmode[i] == BM_STORE_4N) ? sizeof(u8) : sizeof(float);
    launch_param.input_type_lens[i] = type_len;
  }
  launch_param.output_type_lens = new int[output_num];
  for (int i = 0; i < output_num; i++) {
    int type_len =
        (out_stmode != NULL && out_stmode[i] == BM_STORE_4N) ? sizeof(u8) : sizeof(float);
    launch_param.output_type_lens[i] = type_len;
  }

  bool res = bmrt_launch_core(p_bmrt, &launch_param);
  delete[] launch_param.output_type_lens;
  delete[] launch_param.input_type_lens;
  return res;
}

bool bmrt_launch_shape(void* p_bmrt, int net_idx, const char* net_name, const void* input_tensors,
                       int input_num, const int* input_dim, const int* input_shapes,
                       const void* output_tensors, int output_num, bm_shape_t * output_shapes) {
  bool res =
      bmrt_launch_shape_stmode(p_bmrt, net_idx, net_name, input_tensors, input_num, input_dim,
                               input_shapes, output_tensors, output_num, NULL, NULL, output_shapes);
  return res;
}

/* static, 1N, FP32 */
bool bmrt_launch(void* p_bmrt, int net_idx, const char* net_name, const void* input_tensors,
                 int input_num, const void* output_tensors, int output_num, bm_shape_t * output_shapes) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  BMRT_ASSERT(((Bmruntime*)p_bmrt)->get_net_info(net_idx)->is_dynamic == false);
  return ((Bmruntime*)p_bmrt)->launch(net_idx, input_num, (const bm_device_mem_t*)input_tensors,NULL,
                                      NULL, NULL, output_num, (const bm_device_mem_t*)output_tensors, NULL, output_shapes);
}

/* static/dynamic,  1N/4N */
bool bmrt_launch_nhw_stmode_core(void* p_bmrt, int net_idx, const char* net_name,
                                 const void* input_tensors, int input_num,
                                 const void* output_tensors, int output_num, int n, int* h, int* w,
                                 bool single_h_w, int* in_stmode, int* out_stmode, bm_shape_t* output_shapes) {
  const char** input_names;
  int input_num_;
  bmrt_get_input_tensor(p_bmrt, net_idx, net_name, &input_num_, &input_names);
  BMRT_ASSERT_INFO(input_num_ == input_num,"input_num_:%d should be equal to input_num:%d",input_num_,input_num);
  const int nchw_dims = 4;
  int* input_dims = new int[input_num];
  int* input_shapes = new int[input_num * nchw_dims];
  for (int i = 0; i < input_num; i++) {
    input_dims[i] = nchw_dims;
    int* shape_ptr = &input_shapes[nchw_dims * i];
    //      int dims =
    (void)bmrt_get_input_blob_max_shape(p_bmrt, input_names[i], net_idx, net_name, shape_ptr);
    //      BMRT_ASSERT(dims == nchw_dims);
    shape_ptr[0] = n;
    shape_ptr[2] = single_h_w ? h[0] : h[i];
    shape_ptr[3] = single_h_w ? w[0] : w[i];
  }

  bool res =
      bmrt_launch_shape_stmode(p_bmrt, net_idx, net_name, input_tensors, input_num, input_dims,
                               input_shapes, output_tensors, output_num, in_stmode, out_stmode, output_shapes);
  free(input_names);
  delete[] input_shapes;
  delete[] input_dims;
  return res;
}

/* multiple input */
bool bmrt_launch_nhw_stmode_multi_input(void* p_bmrt, int net_idx, const char* net_name,
                                        const void* input_tensors, int input_num,
                                        const void* output_tensors, int output_num, int n, int* h,
                                        int* w, int* in_stmode, int* out_stmode, bm_shape_t * output_shapes) {
  return bmrt_launch_nhw_stmode_core(p_bmrt, net_idx, net_name, input_tensors, input_num,
                                     output_tensors, output_num, n, h, w, false, in_stmode,
                                     out_stmode, output_shapes);
}

/* single input */
bool bmrt_launch_nhw_stmode(void* p_bmrt, int net_idx, const char* net_name,
                            const void* input_tensors, int input_num, const void* output_tensors,
                            int output_num, int n, int h, int w, int* in_stmode, int* out_stmode, bm_shape_t * output_shapes) {
  // BMRT_ASSERT(input_num == 1);  // MaskRcNN, rfcn have multiple input, but only 1 input shape,
  // should be a bug.
  return bmrt_launch_nhw_stmode_core(p_bmrt, net_idx, net_name, input_tensors, input_num,
                                     output_tensors, output_num, n, &h, &w, true, in_stmode,
                                     out_stmode, output_shapes);
}

bool bmrt_launch_nhw(void* p_bmrt, int net_idx, const char* net_name, const void* input_tensors,
                     int input_num, const void* output_tensors, int output_num, int n, int h,
                     int w, bm_shape_t * output_shapes) {
  return bmrt_launch_nhw_stmode(p_bmrt, net_idx, net_name, input_tensors, input_num, output_tensors,
                                output_num, n, h, w, NULL, NULL, output_shapes);
}

int bmrt_get_input_blob_max_shape(void* p_bmrt, const char* tensor_name, int net_idx,
                                  const char* net_name, int* shape) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->get_input_blob_max_shape(tensor_name, net_idx, shape);
}

int bmrt_get_output_blob_max_shape(void* p_bmrt, const char* tensor_name, int net_idx,
                                   const char* net_name, int* shape) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->get_output_blob_max_shape(tensor_name, net_idx, shape);
}

void bmrt_get_input_blob_max_nhw(void* p_bmrt, const char* tensor_name, int net_idx,
                                 const char* net_name, int* max_n, int* max_c, int* max_h,
                                 int* max_w) {
  int shape[BM_MAX_DIMS_NUM];
  int dims = bmrt_get_input_blob_max_shape(p_bmrt, tensor_name, net_idx, net_name, shape);
  shape_to_nchw(shape, dims, max_n, max_c, max_h, max_w);
}
void bmrt_get_output_blob_max_nhw(void* p_bmrt, const char* tensor_name, int net_idx,
                                  const char* net_name, int* max_n, int* max_c, int* max_h,
                                  int* max_w) {
  int shape[BM_MAX_DIMS_NUM];
  int dims = bmrt_get_output_blob_max_shape(p_bmrt, tensor_name, net_idx, net_name, shape);
  shape_to_nchw(shape, dims, max_n, max_c, max_h, max_w);
}

// data_type 0: FP32, 1: FP16, 2: INT8, 3: UINT8, 4: INT16, 5: UINT16
int bmrt_get_input_data_type(void* p_bmrt, const char* tensor_name, int net_idx,
                             const char* net_name) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->get_input_data_type(tensor_name, net_idx);
}

int bmrt_get_output_data_type(void* p_bmrt, const char* tensor_name, int net_idx,
                              const char* net_name) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->get_output_data_type(tensor_name, net_idx);
}

// store mode 0: 1N, 1: 2N, 2: 4N
int bmrt_get_input_gmem_stmode(void* p_bmrt, const char* tensor_name, int net_idx,
                               const char* net_name) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->get_input_gmem_stmode(tensor_name, net_idx);
}

int bmrt_get_output_gmem_stmode(void* p_bmrt, const char* tensor_name, int net_idx,
                                const char* net_name) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->get_output_gmem_stmode(tensor_name, net_idx);
}

bool bmrt_can_batch_size_change(void* p_bmrt, int net_idx, const char* net_name) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->can_batch_size_change(net_idx);
}
bool bmrt_can_height_and_width_change(void* p_bmrt, int net_idx, const char* net_name) {
  if (net_name && strlen(net_name)) net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  CHECK_net_idx(net_idx);
  return ((Bmruntime*)p_bmrt)->can_height_and_width_change(net_idx);
}

/* wrap bmlib functions */
int bmrt_malloc_neuron_device(void* p_bmrt, bm_device_mem_t* pmem, int n, int c, int h, int w) {
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  return bm_malloc_neuron_device(bm_handle, pmem, n, c, h, w);
}

int bmrt_malloc_device_byte(void* p_bmrt, bm_device_mem_t* pmem, unsigned int size)
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  return bm_malloc_device_byte(bm_handle, pmem, size);
}

void bmrt_free_device(void* p_bmrt, bm_device_mem_t pmem)
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  bm_free_device(bm_handle, pmem);
}

int bmrt_memcpy_s2d(void* p_bmrt, bm_device_mem_t dst, void* src)
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  return bm_memcpy_s2d(bm_handle, dst, src);
}

int bmrt_memcpy_d2s(void* p_bmrt, void* dst, bm_device_mem_t src)
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  return bm_memcpy_d2s(bm_handle, dst, src);
}

int bmrt_memcpy_s2d_withsize_offset(void* p_bmrt, bm_device_mem_t dst, void* src, unsigned int size,
                                    unsigned int dev_offset)
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  return bm_memcpy_s2d_partial_offset(bm_handle, dst, src, size, dev_offset);
}

int bmrt_memcpy_s2d_withsize(void* p_bmrt, bm_device_mem_t dst, void* src, unsigned int size)
{
  return bmrt_memcpy_s2d_withsize_offset(p_bmrt, dst, src, size, 0);
}

int bmrt_memcpy_d2s_withsize_offset(void* p_bmrt, void* dst, bm_device_mem_t src, unsigned int size,
                                    unsigned int dev_offset)
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  return bm_memcpy_d2s_partial_offset(bm_handle, dst, src, size, dev_offset);
}

int bmrt_memcpy_d2s_withsize(void* p_bmrt, void* dst, bm_device_mem_t src, unsigned int size)
{
  return bmrt_memcpy_d2s_withsize_offset(p_bmrt, dst, src, size, 0);
}

int bmrt_dev_getcount(void* p_bmrt, int* dev_count)
{
  return bm_dev_getcount(dev_count);
}

#ifdef __linux__
void bmrt_get_last_api_process_time_us(void* p_bmrt, unsigned long* time_us)
#else
void bmrt_get_last_api_process_time_us(void* p_bmrt, unsigned long long* time_us)
#endif
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  bm_get_last_api_process_time_us(bm_handle, time_us);
}

int bmrt_thread_sync(void* p_bmrt)
{
  bm_handle_t bm_handle = ((Bmruntime*)p_bmrt)->get_bm_handle();
  return bm_thread_sync(bm_handle);
}
