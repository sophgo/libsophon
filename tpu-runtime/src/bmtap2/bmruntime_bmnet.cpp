#include <stdlib.h>
#include <string.h>
#include "bmruntime.h"
#include "bmruntime_inner.h"
#include "bmruntime_interface.h"

//#define MEASURE_TIME
#ifdef MEASURE_TIME
#include <sys/time.h>
#endif

using bmruntime::Bmruntime;

shape_t bmnet_shape_convert(bm_shape_t bm_shape)
{
  shape_t shape = {1, 1, 1, 1, 1};
  shape.dim = bm_shape.num_dims < 4 ? bm_shape.num_dims : 4;
  shape.n = bm_shape.dims[0];
  if (bm_shape.num_dims > 1)
    shape.c = bm_shape.dims[1];
  if (bm_shape.num_dims > 2)
    shape.h = bm_shape.dims[2];
  if (bm_shape.num_dims > 3) {
    shape.w = bm_shape.dims[3];
    for (uint32_t i = 4; i < shape.dim; i++)
      shape.w *= bm_shape.dims[i];
  }

  return shape;
}

uint32_t bmnet_data_type_convert(bm_data_type_t data_type)
{
  uint32_t type_table[BM_UINT32 + 1] = {FMT_F32, FMT_F16, FMT_I8,  FMT_U8,
                                        FMT_I16, FMT_U16, FMT_I32, FMT_INVALID};
  return type_table[(int)data_type];
}

static size_t bmnet_data_size_convert(const bm_shape_t *shape_val, bm_data_type_t data_type)
{
  return bmrt_shape_count(shape_val) * bmrt_data_type_size(data_type);
}

static bmerr_t bmnet_register_inner(bmctx_t ctx, Bmruntime *p_bmrt, bmnet_t *net)
{
  const bm_net_info_t *net_info = p_bmrt->get_net_info(0);
  RET_ERR_IF(net_info == NULL, "there is no net");

  int input_num = net_info->input_num;
  int output_num = net_info->output_num;

  bmnet_context_t *net_ctx = (bmnet_context_t *)malloc(sizeof(bmnet_context_t));
  memset(net_ctx, 0, sizeof(bmnet_context_t));
  net_ctx->p_bmrt = p_bmrt;
  net_ctx->bm_handle = ctx;
  net_ctx->input_num = input_num;
  net_ctx->output_num = output_num;

  int stage_num = net_info->stage_num;

  // get model info
  bmnet_model_info_t *p_model = &(net_ctx->model_info);

  p_model->net_name = (char *)net_info->name;
  p_model->command_num = stage_num;
  p_model->chip = (bmruntime::bmrt_arch_info::get_bmtpu_arch() == bmruntime::BM1682 ? 1682 : 1684);

  bmnet_input_info_t *input_array =
      (bmnet_input_info_t *)malloc(sizeof(bmnet_input_info_t) * stage_num);
  memset(input_array, 0, sizeof(bmnet_input_info_t) * stage_num);

  bmnet_output_info_t *output_array =
      (bmnet_output_info_t *)malloc(sizeof(bmnet_output_info_t) * stage_num);
  memset(output_array, 0, sizeof(bmnet_output_info_t) * stage_num);

  for (int i = 0; i < stage_num; ++i) {
    bm_stage_info_t *stage = &net_info->stages[i];

    input_array[i].input_size = 0;
    input_array[i].input_num = input_num;
    input_array[i].shape_array = (shape_t *)malloc(sizeof(shape_t) * input_num);
    input_array[i].size_array = (size_t *)malloc(sizeof(size_t) * input_num);
    input_array[i].fmt_array = (uint32_t *)malloc(sizeof(uint32_t) * input_num);
    for (int j = 0; j < input_num; ++j) {
      input_array[i].shape_array[j] = bmnet_shape_convert(stage->input_shapes[j]);
      input_array[i].fmt_array[j] = bmnet_data_type_convert(net_info->input_dtypes[j]);
      input_array[i].size_array[j] =
          bmnet_data_size_convert(stage->input_shapes + j, net_info->input_dtypes[j]);
      input_array[i].input_size += input_array[i].size_array[j];
    }

    output_array[i].output_size = 0;
    output_array[i].output_num = output_num;
    output_array[i].name_array = (char **)malloc(sizeof(char *) * output_num);
    output_array[i].shape_array = (shape_t *)malloc(sizeof(shape_t) * output_num);
    output_array[i].size_array = (size_t *)malloc(sizeof(size_t) * output_num);
    output_array[i].fmt_array = (uint32_t *)malloc(sizeof(uint32_t) * output_num);
    for (int j = 0; j < output_num; ++j) {
      output_array[i].shape_array[j] = bmnet_shape_convert(stage->output_shapes[j]);
      output_array[i].name_array[j] = (char *)net_info->output_names[j];
      output_array[i].fmt_array[j] = bmnet_data_type_convert(net_info->output_dtypes[j]);
      output_array[i].size_array[j] =
          bmnet_data_size_convert(stage->output_shapes + j, net_info->output_dtypes[j]);
      output_array[i].output_size += output_array[i].size_array[j];
    }
  }
  p_model->input_info_array = input_array;
  p_model->output_info_array = output_array;

  bm_tensor_t *input_tensors = (bm_tensor_t *)malloc(sizeof(bm_tensor_t) * input_num);
  bm_tensor_t *output_tensors = (bm_tensor_t *)malloc(sizeof(bm_tensor_t) * output_num);

  for (int j = 0; j < input_num; ++j) {
    bmrt_tensor(&input_tensors[j], p_bmrt, net_info->input_dtypes[j],
                *p_bmrt->get_input_max_shape(0, j));
    input_tensors[j].shape = net_info->stages[0].input_shapes[j];
  }

  for (int j = 0; j < output_num; ++j) {
    bmrt_tensor(&output_tensors[j], p_bmrt, net_info->output_dtypes[j],
                *p_bmrt->get_output_max_shape(0, j));
    output_tensors[j].shape = net_info->stages[0].output_shapes[j];
  }

  net_ctx->input_tensors = input_tensors;
  net_ctx->output_tensors = output_tensors;

  // set defalut input shape
  net_ctx->current_index = 0;

  *net = net_ctx;

  return BM_SUCCESS;
}

bmerr_t bmnet_register_bmodel(bmctx_t ctx, const char *bmodel, bmnet_t *net)
{
  RET_ERR_IF(ctx == NULL, "bmctx_t is NULL");
  RET_ERR_IF(bmodel == NULL, "file name is NULL");
  Bmruntime *p_bmrt = (Bmruntime *)bmrt_create(ctx);
  bool ret = p_bmrt->load_bmodel(bmodel);
  RET_ERR_IF(ret == false, "load bmodel[%s] failed", bmodel);

  // TODO(pengchao.hu): use bmodel interface to limit that one bmodel must have one network
  return bmnet_register_inner(ctx, p_bmrt, net);
}

bmerr_t bmnet_register_bmodel_data(bmctx_t ctx, void *bmodel_data, size_t size, bmnet_t *net)
{
  RET_ERR_IF(ctx == NULL, "bmctx_t is NULL");
  RET_ERR_IF(bmodel_data == NULL, "bmodel_data is NULL");
  RET_ERR_IF(size <= 0, "bmodel_data size is less than 0");
  Bmruntime *p_bmrt = (Bmruntime *)bmrt_create(ctx);
  bool ret = p_bmrt->load_bmodel(bmodel_data, size);
  RET_ERR_IF(ret == false, "load bmodel data failed");

  // TODO(pengchao.hu): use bmodel interface to limit that one bmodel must have one network
  return bmnet_register_inner(ctx, p_bmrt, net);
}

bmerr_t bmnet_set_input_shape(bmnet_t net, shape_t shape)
{
  return bmnet_set_input_shape2(net, &shape, 1);
}

static bool shape_equal(shape_t s1, shape_t s2)
{
  return (s1.dim == s2.dim) && (s1.n == s2.n) && (s1.c == s2.c) && (s1.h == s2.h) && (s1.w == s2.w);
}

static void bmnet_set_input_index(bmnet_context_t *net_ctx, int index)
{
  net_ctx->current_index = index;
  Bmruntime *p_bmrt = net_ctx->p_bmrt;
  auto net_info = p_bmrt->get_net_info(0);
  for (int i = 0; i < net_info->input_num; i++) {
    net_ctx->input_tensors[i].shape = net_info->stages[index].input_shapes[i];
  }
  for (int i = 0; i < net_info->output_num; i++) {
    net_ctx->output_tensors[i].shape = net_info->stages[index].output_shapes[i];
  }
}

bmerr_t bmnet_set_input_shape2(bmnet_t net, shape_t *shape, int num)
{
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  RET_ERR_IF(net_ctx == NULL, "net is NULL");
  RET_ERR_IF(shape == NULL || num == 0, "parameter error");

  int command_num = net_ctx->model_info.command_num;
  for (int index = 0; index < command_num; ++index) {
    bmnet_input_info_t *p_info = &(net_ctx->model_info.input_info_array[index]);
    if (p_info->input_num != (unsigned int)num) {
      continue;
    }
    bool match = true;
    for (int j = 0; j < num; ++j) {
      if (false == shape_equal(p_info->shape_array[j], shape[j])) {
        match = false;
        break;
      }
    }
    if (match) {
      bmnet_set_input_index(net_ctx, index);
      return BM_SUCCESS;
    }
  }

  return BM_ERR_DATA;
}

bmerr_t bmnet_get_output_info(bmnet_t net, bmnet_output_info_t *output_info)
{
  RET_ERR_IF(output_info == NULL, "output_info is NULL");
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  RET_ERR_IF(net_ctx == NULL, "net is NULL");
  *output_info = net_ctx->model_info.output_info_array[net_ctx->current_index];
  return BM_SUCCESS;
}

const bmnet_input_info_t *bmnet_get_input_info(bmnet_t net)
{
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  if (net_ctx == NULL) {
    return NULL;
  }
  return &(net_ctx->model_info.input_info_array[net_ctx->current_index]);
}

const bmnet_model_info_t *bmnet_get_model_info(bmnet_t net)
{
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  if (net_ctx == NULL) {
    return NULL;
  }

  return &net_ctx->model_info;
}

void bmnet_cleanup(bmnet_t net)
{
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  if (net_ctx == NULL) {
    return;
  }

  for (int i = 0; i < net_ctx->input_num; ++i) {
    bm_free_device(net_ctx->bm_handle, net_ctx->input_tensors[i].device_mem);
  }
  free(net_ctx->input_tensors);

  for (int i = 0; i < net_ctx->output_num; ++i) {
    bm_free_device(net_ctx->bm_handle, net_ctx->output_tensors[i].device_mem);
  }
  free(net_ctx->output_tensors);

  // free model_info
  bmnet_model_info_t *p_model = &(net_ctx->model_info);
  for (unsigned int i = 0; i < p_model->command_num; ++i) {
    free(p_model->input_info_array[i].shape_array);
    free(p_model->input_info_array[i].size_array);
    free(p_model->input_info_array[i].fmt_array);

    free(p_model->output_info_array[i].name_array);
    free(p_model->output_info_array[i].shape_array);
    free(p_model->output_info_array[i].size_array);
    free(p_model->output_info_array[i].fmt_array);
  }
  free(p_model->input_info_array);
  free(p_model->output_info_array);
  bmrt_destroy(net_ctx->p_bmrt);

  free(net_ctx);
}

bmerr_t bmnet_run(bmnet_t net)
{
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  // set input_tensors
  bmnet_input_info_t *input_info = &net_ctx->model_info.input_info_array[net_ctx->current_index];
  int input_num = input_info->input_num;

  // set output_tensors
  bmnet_output_info_t *output_info = &net_ctx->model_info.output_info_array[net_ctx->current_index];
  int output_num = output_info->output_num;

  bool res = net_ctx->p_bmrt->launch(0, net_ctx->input_tensors, input_num, net_ctx->output_tensors,
                                     output_num, true, true);

  // wait for bmrt launch done
  if (res == true) {
   res = (BM_SUCCESS == bm_thread_sync(net_ctx->bm_handle));
  }

  return (res == true) ? BM_SUCCESS : BM_ERR_FAILURE;
}

bmerr_t bmnet_load_input(bmnet_t net, void *input)
{
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  RET_ERR_IF(net_ctx == NULL, "net is NULL");
  bmnet_input_info_t *input_info = &net_ctx->model_info.input_info_array[net_ctx->current_index];

  // upload input data
  uint8_t *pIn = (uint8_t *)input;
  for (unsigned int i = 0; i < input_info->input_num; i++) {
    int ret = bm_memcpy_s2d_partial(net_ctx->bm_handle, net_ctx->input_tensors[i].device_mem, pIn,
                                    input_info->size_array[i]);
    RET_ERR_IF(ret != 0, "memcpy s2d failed");
    pIn += input_info->size_array[i];
  }

  return BM_SUCCESS;
}

bmerr_t bmnet_store_output(bmnet_t net, void *output)
{
  bmnet_context_t *net_ctx = (bmnet_context_t *)net;
  RET_ERR_IF(net_ctx == NULL, "net is NULL");
  RET_ERR_IF(output == NULL, "output is NULL");
  bmnet_output_info_t *output_info =
      &(net_ctx->model_info.output_info_array[net_ctx->current_index]);

  uint8_t *pOut = (uint8_t *)output;
  for (unsigned int i = 0; i < output_info->output_num; i++) {
    // TODO: if one net with multi-output and cpu layer, each output maybe is dynamic.
    // In this situation, it is hard to deal.
    int ret = bm_memcpy_d2s_partial(net_ctx->bm_handle, pOut, net_ctx->output_tensors[i].device_mem,
                                    bmrt_tensor_bytesize(&net_ctx->output_tensors[i]));
    RET_ERR_IF(ret != 0, "memcpy d2s failed");
    pOut += output_info->size_array[i];
  }

  return BM_SUCCESS;
}

// run inference
bmerr_t bmnet_inference(bmnet_t net, void *input, void *output)
{
  // check parameter
  RET_ERR_IF(net == NULL, "net is NULL");
  RET_ERR_IF(input == NULL, "input is NULL");
  RET_ERR_IF(output == NULL, "output is NULL");

#ifdef MEASURE_TIME
  #ifdef __linux__
  struct timeval t0, t1;
  #else
  struct timespec t0, t1;
  #endif
  long elapsed, last, time_load_input, time_run, time_read_output;
  #ifdef __linux__
  gettimeofday(&t0, NULL);
  #else
  bmrt_clock_gettime(0, &t0);
  #endif
  last = 0;
#endif

  // upload input data
  bmerr_t ret = bmnet_load_input(net, input);
  RET_ERR_IF(ret != BM_SUCCESS, "load input failed");

#ifdef MEASURE_TIME
  #ifdef __linux__
  gettimeofday(&t1, NULL);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
  #else
  bmrt_clock_gettime(0, &t1);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_nsec - t0.tv_nsec;
  #endif
  time_load_input = elapsed - last;
  last = elapsed;
#endif

  // run cmdbuf
  ret = bmnet_run(net);
  RET_ERR_IF(ret != BM_SUCCESS, "bmnet run failed");

#ifdef MEASURE_TIME
  #ifdef __linux__
  gettimeofday(&t1, NULL);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
  #else
  bmrt_clock_gettime(0, &t1);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_nsec - t0.tv_nsec;
  #endif
  time_run = elapsed - last;
  last = elapsed;
#endif

  // download output data
  ret = bmnet_store_output(net, output);
  RET_ERR_IF(ret != BM_SUCCESS, "bmnet store output failed");

#ifdef MEASURE_TIME
  #ifdef __linux__
  gettimeofday(&t1, NULL);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
  #else
  bmrt_clock_gettime(0, &t1);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_nsec - t0.tv_nsec;
  #endif
  time_read_output = elapsed - last;
  BMRT_LOG(INFO, "BMRUNTIME_NET: load %ld us, run %ld us, read %ld us", time_load_input, time_run,
         time_read_output);
#endif

  return BM_SUCCESS;
}

bmerr_t bm_init(int index, bmctx_t *ctx)
{
  BM_ASSERT(ctx != NULL);
  return bm_dev_request(ctx, index);
}

void bm_exit(bmctx_t ctx)
{
  bm_dev_free(ctx);
}

bmmem_device_t bmmem_device_alloc_raw(bmctx_t ctx, size_t size)
{
  bmmem_device_t mem;
  bm_status_t status = bm_malloc_device_byte(ctx, &mem, size);
  CHECK_status(status);
  return mem;
}
void bmmem_device_free(bmctx_t ctx, bmmem_device_t mem)
{
  bm_free_device(ctx, mem);
}

// [offset, size] is area in bmmem_device_t dst
bmerr_t bm_memcpy_s2d_ex(bmctx_t ctx, bmmem_device_t dst, void *src, uint64_t offset, size_t size)
{
  return bm_memcpy_s2d_partial_offset(ctx, dst, src, size, offset);
}
// [offset, size] is area in bmmem_device_t src
bmerr_t bm_memcpy_d2s_ex(bmctx_t ctx, void *dst, bmmem_device_t src, uint64_t offset, size_t size)
{
  return bm_memcpy_d2s_partial_offset(ctx, dst, src, size, offset);
}
