#include "bmruntime_interface.h"
#include "bmruntime_profile.h"
#include "bmruntime.h"
#include "bmruntime_common.h"
#include "string.h"

using bmruntime::bmfunc;
using bmruntime::Bmruntime;

/* get data type byte size */
size_t bmrt_data_type_size(bm_data_type_t dtype)
{
  switch (dtype) {
    case BM_FLOAT32:
    case BM_INT32:
    case BM_UINT32:
      return 4;
    case BM_FLOAT16:
    case BM_BFLOAT16:
    case BM_INT16:
      return 2;
    case BM_INT8:
    case BM_UINT8:
      return 1;
    case BM_INT4:
    case BM_UINT4:
      return 1;  // need modify ?  to do
    default:
      BMRT_LOG(FATAL, "Error: Unknown data type: %d", dtype);
  }
  return 0;
}

/* dims array to bm_shape_t
 num_dims should not be larger than BM_MAX_DIMS_NUM */
void bmrt_shape(bm_shape_t* shape, const int* dims, int num_dims)
{
  BMRT_ASSERT_INFO(shape != NULL && dims != NULL,"shape or dims: %d shouldn't be NULL",dims);
  BMRT_ASSERT_INFO(num_dims >= 0 && num_dims <= BM_MAX_DIMS_NUM,"num_dims:%d should not be larger than BM_MAX_DIMS_NUM:8 or less than 0",num_dims);
  for (int i = 0; i < num_dims; i++) {
    BMRT_ASSERT_INFO(dims[i] > 0, "Input tensor's dim[%d]=%d is incorrect",i,dims[i]);
    shape->dims[i] = dims[i];
  }
  shape->num_dims = num_dims;
}

/* number of shape elements */
uint64_t bmrt_shape_count(const bm_shape_t* shape)
{
  BMRT_ASSERT_INFO(shape != NULL, "input shape shouldn't be NULL");
  BMRT_ASSERT_INFO(shape->num_dims <= BM_MAX_DIMS_NUM, "shape->num_dims:%d shouldn't be larger than BM_MAX_DIMS_NUM: 8", shape->num_dims);
  uint64_t count = 1;
  for (int i = 0; i < shape->num_dims; i++) {
    count *= shape->dims[i];
  }
  return count;
}

bool bmrt_shape_is_same(const bm_shape_t* left, const bm_shape_t* right)
{
  BMRT_ASSERT_INFO(left != NULL && right != NULL, "input pointer left:%p and right:%p shouldn't be NULL", left, right);
  if (left->num_dims != right->num_dims) {
    return false;
  }
  BMRT_ASSERT_INFO(left->num_dims <= BM_MAX_DIMS_NUM, "input left.num_dims shouldn't be larger than BM_MAX_DIMS_NUM: 8");
  for (int i = 0; i < left->num_dims; i++) {
    if (left->dims[i] != right->dims[i]) {
      return false;
    }
  }
  return true;
}

bool bmrt_tensor(bm_tensor_t* tensor, void* p_bmrt, bm_data_type_t dtype, bm_shape_t shape)
{
  BMRT_ASSERT_INFO(tensor != NULL && p_bmrt != NULL, "tensor:%p or p_bmrt:%p shouldn't be NULL", tensor, p_bmrt);
  uint64_t number_shape = bmrt_shape_count(&shape);
  BMRT_ASSERT_INFO(number_shape != 0, "number of shape elements shouldn't be 0");
  tensor->dtype = dtype;
  tensor->shape = shape;
  tensor->st_mode = BM_STORE_1N;
  try {
  ((Bmruntime*)p_bmrt)->must_alloc_device_mem(&tensor->device_mem, bmrt_tensor_bytesize(tensor), "tensor");
      return true;
  } catch (const std::runtime_error &e) {
      return false;
  }
}

void bmrt_tensor_with_device(bm_tensor_t* tensor, bm_device_mem_t device_mem, bm_data_type_t dtype,
                             bm_shape_t shape)
{
  BMRT_ASSERT_INFO(tensor != NULL, "tensor shouldn't be NULL");
  uint64_t shape_count = bmrt_shape_count(&shape);
  size_t data_type_size = bmrt_data_type_size(dtype);
  u32 get_device_size = bm_mem_get_device_size(device_mem);
  BMRT_ASSERT_INFO((shape_count * data_type_size) <= get_device_size,\
  "(shape_count:%d * data_type_size:%d) shouldn't larger than mem_get_device_size:%d", \
  shape_count, data_type_size, get_device_size);
  tensor->dtype = dtype;
  tensor->shape = shape;
  tensor->st_mode = BM_STORE_1N;
  tensor->device_mem = device_mem;
}

size_t bmrt_tensor_bytesize(const bm_tensor_t* tensor)
{
  BMRT_ASSERT_INFO(tensor != NULL, "tensor shouldn't be NULL");
  return bmrt_shape_count(&tensor->shape) * bmrt_data_type_size(tensor->dtype);
}

size_t bmrt_tensor_device_size(const bm_tensor_t* tensor)
{
  BMRT_ASSERT_INFO(tensor != NULL,"tensor shouldn't be NULL");
  return bm_mem_get_device_size(tensor->device_mem);
}

void bmrt_print_network_info(const bm_net_info_t* net_info)
{
  BMRT_ASSERT_INFO(net_info != NULL, "net_info shouldn't be NULL");
  printf("net name: %s\n", net_info->name);
  printf("is dynamic:%d\n", net_info->is_dynamic);
  printf("input num:%d\n", net_info->input_num);
  for (int i = 0; i < net_info->input_num; i++) {
    printf("input:[%s], type:[%d], scale:[%f], zero_point:[%d]\n", net_info->input_names[i],
           net_info->input_dtypes[i], net_info->input_scales[i], net_info->input_zero_point[i]);
  }
  printf("output num:%d\n", net_info->output_num);
  for (int i = 0; i < net_info->output_num; i++) {
    printf("output:[%s], type:[%d], scale:[%f], zero_point:[%d]\n", net_info->output_names[i],
           net_info->output_dtypes[i], net_info->output_scales[i], net_info->output_zero_point[i]);
  }

  printf("stage num:%d\n", net_info->stage_num);
  for (int i = 0; i < net_info->stage_num; i++) {
    printf("-----------------stage[%d]-------------------\n", i);
    for (int j = 0; j < net_info->input_num; j++) {
      printf("input[%s], shape:[ ", net_info->input_names[j]);
      for (int k = 0; k < net_info->stages[i].input_shapes[j].num_dims; k++) {
        printf("%d ", net_info->stages[i].input_shapes[j].dims[k]);
      }
      printf("]\n");
    }
    for (int j = 0; j < net_info->output_num; j++) {
      printf("output[%s], shape:[ ", net_info->output_names[j]);
      for (int k = 0; k < net_info->stages[i].output_shapes[j].num_dims; k++) {
        printf("%d ", net_info->stages[i].output_shapes[j].dims[k]);
      }
      printf("]\n");
    }
  }
}

void* bmrt_create(bm_handle_t bm_handle)
{
  unsigned int chipid = 0;
  if (0 != bm_get_chipid(bm_handle, &chipid)) {
    BMRT_LOG(WRONG, "Error: cannot get chipid:%x",chipid);
    return nullptr;
  }
  std::string chip_name = "";
  if (chipid == 0x1684) {
    chip_name = "BM1684";
  } else if (chipid == 0x1686) {
    chip_name = "BM1684X";
  } else if (chipid == 0x1686a200) {
    chip_name = "BM1686";
  } else if (chipid == 0x1682) {
    chip_name = "BM1682";
  } else if (chipid == 0x1880) {
    chip_name = "BM1880";

  } else {
    BMRT_LOG(WRONG, "Error: unknown chipid %x", chipid);
    return nullptr;
  }
  try {
    Bmruntime* p_bmrt = new Bmruntime(&bm_handle, true, chip_name);
    return (void*)p_bmrt;
  } catch (const std::runtime_error &e) {
    return nullptr;
  }
}

u64 bmrt_must_alloc_device_mem(void* p_bmrt, bm_device_mem_t* pmem, u32 size){
    return ((Bmruntime*)p_bmrt)->must_alloc_device_mem(pmem, size, "interface");
}

void bmrt_must_free_device_mem(void* p_bmrt, bm_device_mem_t mem){
  ((Bmruntime*)p_bmrt)->must_free_device_mem(mem);
}

void bmrt_destroy(void* p_bmrt)
{
  if (p_bmrt != NULL) {
    delete (Bmruntime*)p_bmrt;
  }
}

bool bmrt_load_bmodel(void* p_bmrt, const char* bmodel_path)
{
  if (p_bmrt == NULL || bmodel_path == NULL || bmodel_path[0] == '\0') {
    BMRT_LOG(WRONG, "bmrt handle is NULL or bmodel path is wrong.");
    return false;
  }
  const std::string bmodel_dir = bmodel_path;
  try {
  return ((Bmruntime*)p_bmrt)->load_bmodel(bmodel_dir);
  } catch (const std::runtime_error &e) {
      return false;
  }
}

bool bmrt_load_bmodel_data(void* p_bmrt, const void* bmodel_data, size_t size)
{
  if (p_bmrt == NULL || bmodel_data == NULL || size == 0) {
    BMRT_LOG(WRONG, "bmrt handle is NULL or bmodel data is NULL.");
    return false;
  }
  try {
  return ((Bmruntime*)p_bmrt)->load_bmodel(bmodel_data, size);
  } catch (const std::runtime_error &e) {
      return false;
  }
}

bool bmrt_launch_tensor(void* p_bmrt, const char* net_name, const bm_tensor_t input_tensors[],
                        int input_num, bm_tensor_t output_tensors[], int output_num)
{
  return bmrt_launch_tensor_ex(p_bmrt, net_name, input_tensors, input_num, output_tensors,
                               output_num, false, false);
}

bool bmrt_launch_tensor_ex(void* p_bmrt, const char* net_name, const bm_tensor_t input_tensors[],
                           int input_num, bm_tensor_t output_tensors[], int output_num,
                           bool user_mem, bool user_stmode)
{
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return false;
  }
  int net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  if (net_idx < 0) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return false;
  }
  return ((Bmruntime*)p_bmrt)
      ->launch(net_idx, input_tensors, input_num, output_tensors, output_num, user_mem,
               user_stmode);
}

bool bmrt_launch_data(void* p_bmrt, const char* net_name, void* const input_datas[],
                      const bm_shape_t input_shapes[], int input_num, void* output_datas[],
                      bm_shape_t output_shapes[], int output_num, bool user_mem)
{
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return false;
  }
  int net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  if (net_idx < 0) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return false;
  }
  return ((Bmruntime*)p_bmrt)
      ->launch(net_idx, input_datas, input_shapes, input_num, output_datas, output_shapes,
               output_num, user_mem);
}

void bmrt_show_neuron_network(void* p_bmrt)
{
  if (p_bmrt == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return;
  }
  ((Bmruntime*)p_bmrt)->show_neuron_network();
}

int bmrt_get_network_number(void* p_bmrt)
{
  if (p_bmrt == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return 0;
  }
  return ((Bmruntime*)p_bmrt)->get_network_number();
}

void bmrt_get_network_names(void* p_bmrt, const char*** network_names)
{
  if (p_bmrt == NULL || network_names == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return;
  }
  vector<const char*> names;
  ((Bmruntime*)p_bmrt)->get_network_names(&names);
  if (names.size() == 0) {
    *network_names = NULL;
    return;
  }
  /* Caller need free network_names */
  *network_names = (const char**)malloc(names.size() * sizeof(char*));
  for (u32 i = 0; i < names.size(); i++) {
    (*network_names)[i] = names[i];
  }
}

int bmrt_get_network_index(void* p_bmrt, const char* net_name)
{
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return -1;
  }
  return ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
}

const bm_net_info_t* bmrt_get_network_info(void* p_bmrt, const char* net_name)
{
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return NULL;
  }
  int net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  if (net_idx < 0) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return NULL;
  }
  return ((Bmruntime*)p_bmrt)->get_net_info(net_idx);
}

void bmrt_trace(void* p_bmrt)
{
  if (p_bmrt == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return;
  }
  ((Bmruntime*)p_bmrt)->trace();
}
