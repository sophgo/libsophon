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
    case BM_UINT16:
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
  return bmrt_tensor_ex(tensor, p_bmrt, 0, dtype, shape);
}

bool bmrt_tensor_ex(bm_tensor_t* tensor, void* p_bmrt, int devid, bm_data_type_t dtype, bm_shape_t shape)
{
  BMRT_ASSERT_INFO(tensor != NULL && p_bmrt != NULL, "tensor:%p or p_bmrt:%p shouldn't be NULL", tensor, p_bmrt);
  uint64_t number_shape = bmrt_shape_count(&shape);
  BMRT_ASSERT_INFO(number_shape != 0, "number of shape elements shouldn't be 0");
  tensor->dtype = dtype;
  tensor->shape = shape;
  tensor->st_mode = BM_STORE_1N;
  try {
    ((Bmruntime*)p_bmrt)->must_alloc_device_mem(devid, &tensor->device_mem, bmrt_tensor_bytesize(tensor), "tensor");
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

static std::string chip_name_by_id(unsigned int chipid) {
  std::string chip_name = "";
  if (chipid == 0x1684) {
    chip_name = "BM1684";
  } else if (chipid == 0x1686) {
    chip_name = "BM1684X";
  } else if (chipid == 0x1686a200 || chipid == 0x1688) {
    chip_name = "BM1688";
  } else if (chipid == 0x1682) {
    chip_name = "BM1682";
  } else if (chipid == 0x1880) {
    chip_name = "BM1880";
  } else if (chipid == 0x2260) {
    chip_name = "BM1690";
  } else if (chipid == 0x2380) {
    chip_name = "SG2380";
  } else if (chipid == 0x3000) {
    chip_name = "MARS3";
  }
  return chip_name;
}

void* bmrt_create(bm_handle_t bm_handle)
{
  unsigned int chipid = 0;
  if (0 != bm_get_chipid(bm_handle, &chipid)) {
    BMRT_LOG(WRONG, "Error: cannot get chipid:%x",chipid);
    return nullptr;
  }
  std::string chip_name = chip_name_by_id(chipid);
  if (chip_name.empty()) {
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

void *bmrt_create_ex(bm_handle_t *bm_handles, int num_handles) {
  BMRT_ASSERT_INFO(num_handles > 0, "num_handles should > 0");
  unsigned int chipid = 0;
  if (0 != bm_get_chipid(bm_handles[0], &chipid)) {
    BMRT_LOG(WRONG, "Error: cannot get chipid:%x",chipid);
    return nullptr;
  }
  // check all handles are the same
  for (int i = 1; i < num_handles; i++) {
    unsigned int chipid2 = 0;
    if (0 != bm_get_chipid(bm_handles[i], &chipid2)) {
      BMRT_LOG(WRONG, "Error: cannot get chipid:%x", chipid);
      return nullptr;
    }
    if (chipid != chipid2) {
      BMRT_LOG(WRONG, "Error: chipid not same:[0]:%x,[%d]:%x", chipid, i, chipid2);
      return nullptr;
    }
  }
  std::string chip_name = chip_name_by_id(chipid);
  if (chip_name.empty()) {
    BMRT_LOG(WRONG, "Error: unknown chipid %x", chipid);
    return nullptr;
  }
  try {
    Bmruntime* p_bmrt = new Bmruntime(bm_handles, num_handles, true, chip_name);
    return (void*)p_bmrt;
  } catch (const std::runtime_error &e) {
    return nullptr;
  }
}

u64 bmrt_must_alloc_device_mem(void* p_bmrt, bm_device_mem_t* pmem, u32 size){
    return ((Bmruntime*)p_bmrt)->must_alloc_device_mem(0, pmem, size, "interface");
}

void bmrt_must_free_device_mem(void* p_bmrt, bm_device_mem_t mem){
  ((Bmruntime*)p_bmrt)->must_free_device_mem(0, mem);
}

uint32_t bmrt_get_flags(void* p_bmrt) {
  return ((Bmruntime*)p_bmrt)->get_flags();
}

void bmrt_set_flags(void* p_bmrt, uint32_t flags) {
  return ((Bmruntime*)p_bmrt)->set_flags(flags);
}

void bmrt_destroy(void* p_bmrt)
{
  if (p_bmrt != NULL) {
    delete (Bmruntime*)p_bmrt;
  }
}

void bmrt_destroy_without_coeff(void* p_bmrt)
{
  if (p_bmrt != NULL) {
    ((Bmruntime*)p_bmrt)->destory_without_coeff();
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

bool bmrt_load_bmodel_with_mem(void* p_bmrt, const char* bmodel_path, mem_info_t* mem_info)
{
  if (p_bmrt == NULL || bmodel_path == NULL || bmodel_path[0] == '\0') {
    BMRT_LOG(WRONG, "bmrt handle is NULL or bmodel path is wrong.");
    return false;
  }
  const std::string bmodel_dir = bmodel_path;
  try {
  return ((Bmruntime*)p_bmrt)->load_bmodel_with_mem(bmodel_dir, mem_info);
  } catch (const std::runtime_error &e) {
      return false;
  }
}

bool bmrt_load_bmodel_data_with_mem(void* p_bmrt, const void* bmodel_data, size_t size, mem_info_t* mem_info)
{
  if (p_bmrt == NULL || bmodel_data == NULL || size == 0) {
    BMRT_LOG(WRONG, "bmrt handle is NULL or bmodel data is NULL.");
    return false;
  }
  try {
  return ((Bmruntime*)p_bmrt)->load_bmodel_with_mem(bmodel_data, size, mem_info);
  } catch (const std::runtime_error &e) {
      return false;
  }
}

bool bmrt_load_bmodel_with_decrypt_lib(void *p_bmrt, const char *bmodel_path,
                                   const char *decrypt_lib) {
  if (p_bmrt == NULL || bmodel_path == NULL || bmodel_path[0] == '\0') {
    BMRT_LOG(WRONG, "bmrt handle is NULL or bmodel path is wrong.");
    return false;
  }
  if (decrypt_lib == NULL || decrypt_lib[0] == '\0') {
    BMRT_LOG(WRONG, "decrypt lib path is wrong.");
    return false;
  }
  const std::string bmodel_dir = bmodel_path;
  const std::string lib_dir = decrypt_lib;
  try {
    return ((Bmruntime *)p_bmrt)->load_bmodel_with_decrypt(bmodel_dir, lib_dir);
  } catch (const std::runtime_error &e) {
    return false;
  }
}

bool bmrt_load_bmodel_with_decrypt(void* p_bmrt, const char* bmodel_path, decrypt_func f) {
  if (p_bmrt == NULL || bmodel_path == NULL || bmodel_path[0] == '\0') {
    BMRT_LOG(WRONG, "bmrt handle is NULL or bmodel path is wrong.");
    return false;
  }
  if (f == nullptr) {
    BMRT_LOG(WRONG, "decrypt funciton is null");
    return false;
  }
  const std::string bmodel_dir = bmodel_path;
  try {
    return ((Bmruntime *)p_bmrt)->load_bmodel_with_decrypt(bmodel_dir, f);
  } catch (const std::runtime_error &e) {
    return false;
  }
}

static inline void memory_init(memory_t &mem, uint64_t size, int type) {
  mem.addr = -1;
  mem.size = ALIGN(size, 128);
  mem.type = type;
  mem.number = 1;
}

bool bmrt_get_bmodel_info(ModelCtx* model_ctx, mem_info_t *mem_info) {
  bmodel::bmodel_mem_info_t bmem_info = model_ctx->get_bmodel_mem_info();
  // instruction_mem: bdc_cmd + hau_cmd + dynamic_ir
  uint64_t instruction_size = ALIGN(bmem_info.bd_cmd_mem_size, 128);
  instruction_size += ALIGN(bmem_info.hau_cmd_mem_size, 128);
  instruction_size += ALIGN(bmem_info.dynamic_ir_mem_size, 128);
  memory_init(mem_info->instruction_mem, instruction_size, 0);
  // variable_instruction_mem: gdma_cmd + sdma_cmd
  uint64_t v_instruction_size = bmem_info.gdma_cmd_mem_size;
  v_instruction_size = ALIGN(v_instruction_size, 128) + bmem_info.sdma_cmd_mem_size;
  // v_instruction_size = ALIGN(v_instruction_size, 128) + bmem_info.cdma_cmd_mem_size;
  memory_init(mem_info->variable_instruction_mem, v_instruction_size, 0);
  // coeff_mem: coeff
  memory_init(mem_info->coeff_mem, bmem_info.coeff_mem_size, 0);
  // neuron_mem: neuron + middle_buffer_size + dynamic_output
  uint64_t neuron_size = ALIGN(bmem_info.neuron_mem_size, 128);
  neuron_size += ALIGN(bmem_info.middle_buffer_size, 128);
  neuron_size += ALIGN(bmem_info.dynamic_output_number * sizeof(bm_shape_ex_t), 128);
  //  neuron + middle_buffe + max_subnet_output_member
  memory_init(mem_info->neuron_mem, neuron_size, 0);
  // io mem size
  uint64_t io_mem_size = 0;
  for (int net_idx = 0; net_idx < model_ctx->model()->net()->size(); ++net_idx) {
    auto net = model_ctx->model()->net()->Get(net_idx);
    auto net_params = net->parameter();
    if (net->addr_mode() == ADDR_MODE_IO_ALONE) {
      for (int stage_idx = 0; stage_idx < net_params->size(); ++stage_idx) {
        auto param = net_params->Get(stage_idx);
        io_mem_size += ALIGN(param->io_size(), 128);
      }
    }
  }
  memory_init(mem_info->io_mem, io_mem_size, 0);
  return true;
}

bool bmrt_get_bmodel_data_info(const void* bmodel_data, size_t size, mem_info_t *mem_info) {
  if (bmodel_data == NULL || size == 0) {
    BMRT_LOG(WRONG, "bmodel data is NULL.");
    return false;
  }
  BMRT_LOG(DEBUG, "Loading bmodel info");
  ModelCtx model_ctx(bmodel_data, size);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load model failed.");
      return false;
  }
  return bmrt_get_bmodel_info(&model_ctx, mem_info);
}

bool bmrt_get_bmodel_info(const char *bmodel_path, mem_info_t *mem_info) {
  if (bmodel_path == NULL || bmodel_path[0] == '\0') {
    BMRT_LOG(WRONG, "Bmodel path is wrong.");
    return false;
  }
  BMRT_LOG(DEBUG, "Loading bmodel info from [%s].", bmodel_path);
  const std::string bmodel_dir = bmodel_path;
  ModelCtx model_ctx(bmodel_dir);
  if (!model_ctx) {
      BMRT_LOG(WRONG, "Load model failed.");
      return false;
  }
  return bmrt_get_bmodel_info(&model_ctx, mem_info);
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
  if (auto net_c = ((Bmruntime*)p_bmrt)->get_net_cascade(net_name)) {
    return ((Bmruntime*)p_bmrt)
      ->launch(net_c, input_tensors, input_num, output_tensors, output_num);
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

bool bmrt_launch_tensor_multi_cores(void *p_bmrt, const char *net_name,
                                    const bm_tensor_t input_tensors[],
                                    int input_num, bm_tensor_t output_tensors[],
                                    int output_num, bool user_mem,
                                    bool user_stmode, const int *core_list,
                                    int core_num) {
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return false;
  }

  if (auto net_c = ((Bmruntime*)p_bmrt)->get_net_cascade(net_name)) {
    return ((Bmruntime*)p_bmrt)
      ->launch(net_c, input_tensors, input_num, output_tensors, output_num);
  }

  int net_idx = ((Bmruntime *)p_bmrt)->get_net_idx(net_name);
  if (net_idx < 0) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return false;
  }
  std::vector<int32_t> core_vector{core_list, core_list + core_num};
  return ((Bmruntime *)p_bmrt)
      ->launch_multi_cores(net_idx, input_tensors, input_num, output_tensors,
                           output_num, 0, core_vector, user_mem, user_stmode);
}

bool bmrt_pre_alloc_neuron_multi_cores(
    void *p_bmrt,
    const char *net_name,
    int stage_idx,
    const int *core_list,
    int core_num) {
    if (p_bmrt == NULL || net_name == NULL) {
      BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
      return false;
    }
    if (auto net_c = ((Bmruntime*)p_bmrt)->get_net_cascade(net_name)) {
      return true;
    }
    int net_idx = ((Bmruntime *)p_bmrt)->get_net_idx(net_name);
    if (net_idx < 0) {
      BMRT_LOG(WRONG, "net name:%s invalid", net_name);
      return false;
    }
    auto net_info = bmrt_get_network_info(p_bmrt, net_name);
    if (stage_idx < 0 || stage_idx >= net_info->stage_num) {
      BMRT_LOG(WRONG, "stage_idx:%d invalid", stage_idx);
      return false;
    }
    std::vector<int32_t> core_vector{core_list, core_list + core_num};
     ((Bmruntime *)p_bmrt)->pre_alloc_neuron_multi_cores(net_idx, stage_idx, core_vector);
    return true;
}

bool bmrt_launch_data_multi_cores(void* p_bmrt, const char* net_name, void* const input_datas[],
                      const bm_shape_t input_shapes[], int input_num, void* output_datas[],
                      bm_shape_t output_shapes[], int output_num, bool user_mem,
                      const int* core_list, int core_num) {

  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return false;
  }
  int net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  if (net_idx < 0) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return false;
  }
  std::vector<int> core_vector(core_list, core_list + core_num);
  return ((Bmruntime*)p_bmrt)
      ->launch_multi_cores(net_idx, input_datas, input_shapes, input_num, output_datas, output_shapes,
               output_num, 0, user_mem, core_vector);

}

bool bmrt_launch_tensor_multi_thread(void *p_bmrt, const char *net_name,
                                    const bm_tensor_t input_tensors[],
                                    int input_num, bm_tensor_t output_tensors[],
                                    int output_num, uint64_t thread_idx, bool user_mem,
                                    bool user_stmode, const int *core_list,
                                    int core_num) {
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return false;
  }

  if (auto net_c = ((Bmruntime*)p_bmrt)->get_net_cascade(net_name)) {
    return ((Bmruntime*)p_bmrt)
      ->launch(net_c, input_tensors, input_num, output_tensors, output_num);
  }

  int net_idx = ((Bmruntime *)p_bmrt)->get_net_idx(net_name);
  if (net_idx < 0) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return false;
  }
  std::vector<int32_t> core_vector{core_list, core_list + core_num};
  return ((Bmruntime *)p_bmrt)
      ->launch_multi_cores(net_idx, input_tensors, input_num, output_tensors,
                           output_num, thread_idx, core_vector, user_mem, user_stmode, true);
}

bool bmrt_launch_data_multi_thread(void* p_bmrt, const char* net_name, void* const input_datas[],
                      const bm_shape_t input_shapes[], int input_num, void* output_datas[],
                      bm_shape_t output_shapes[], int output_num, uint64_t thread_idx, bool user_mem,
                      const int* core_list, int core_num) {

  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return false;
  }
  int net_idx = ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
  if (net_idx < 0) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return false;
  }
  std::vector<int> core_vector(core_list, core_list + core_num);
  return ((Bmruntime*)p_bmrt)
      ->launch_multi_cores(net_idx, input_datas, input_shapes, input_num, output_datas, output_shapes,
               output_num, thread_idx, user_mem, core_vector, true);
}

bool bmrt_pre_alloc_mem_multi_thread(
    void *p_bmrt,
    uint64_t thread_idx,
    const mem_info_t* mem_info) {
    if (p_bmrt == NULL) {
      BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL");
      return false;
    }

    ((Bmruntime *)p_bmrt)->pre_alloc_neuron_multi_thread(thread_idx, mem_info);
    return true;
}


bool bmrt_launch_data(void* p_bmrt, const char* net_name, void* const input_datas[],
                      const bm_shape_t input_shapes[], int input_num, void* output_datas[],
                      bm_shape_t output_shapes[], int output_num, bool user_mem)
{
  return bmrt_launch_data_multi_cores(p_bmrt, net_name,
                                      input_datas, input_shapes, input_num,
                                      output_datas, output_shapes, output_num,
                                      user_mem, NULL, 0);
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

const char *bmrt_get_network_name(void* p_bmrt, int index)
{
  if (p_bmrt == NULL) {
    BMRT_LOG(WRONG, "parameter invalid: p_bmrt is NULL");
    return NULL;
  }
  vector<const char*> names;
  ((Bmruntime*)p_bmrt)->get_network_names(&names);

  if (names.size() < index + 1) {
    BMRT_LOG(WRONG, "parameter invalid: index is larger than name sizes");
    return NULL;
  }
  return names[index];
}

int bmrt_get_network_index(void* p_bmrt, const char* net_name)
{
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return -1;
  }
  return ((Bmruntime*)p_bmrt)->get_net_idx(net_name);
}

int bmrt_get_stage_index(void* p_bmrt, const char* net_name, bm_tensor_t *input_tensors) {
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return -1;
  }
  return ((Bmruntime*)p_bmrt)->get_stage_idx(net_name, input_tensors);
}

const bm_net_info_t* bmrt_get_network_info(void* p_bmrt, const char* net_name)
{
  if (p_bmrt == NULL || net_name == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return NULL;
  }
  auto ret =  ((Bmruntime*)p_bmrt)->get_net_info(net_name);
  if (ret == NULL) {
    BMRT_LOG(WRONG, "net name:%s invalid", net_name);
    return NULL;
  }
  return ret;
}

void bmrt_trace(void* p_bmrt)
{
  if (p_bmrt == NULL) {
    BMRT_LOG(WRONG, "parameter invalid p_bmrt is NULL or net_name is NULL");
    return;
  }
  ((Bmruntime*)p_bmrt)->trace();
}

bool bmrt_memcpy_s2d_parallel(void *p_bmrt,
                              bm_tensor_t tensors[],
                              void* datas[],
                              int tensor_num[],
                              int device_num) {
  return ((Bmruntime*)p_bmrt)
    ->memcpy_s2d_parallel(tensors, datas, tensor_num, device_num);
}

bool bmrt_memcpy_d2s_parallel(void *p_bmrt,
                              void* datas[],
                              bm_tensor_t tensors[],
                              int tensor_num[],
                              int device_num) {
  return ((Bmruntime*)p_bmrt)
    ->memcpy_d2s_parallel(datas, tensors, tensor_num, device_num);
}

bool bmrt_memcpy_d2d_byte_parallel(void *p_bmrt,
                                  bm_tensor_t dst_tensors[],
                                  size_t dst_offsets[],
                                  bm_tensor_t src_tensors[],
                                  size_t src_offsets[],
                                  size_t sizes[],
                                  int tensor_num[],
                                  int device_num) {
  return ((Bmruntime*)p_bmrt)
    ->memcpy_d2d_byte_parallel(dst_tensors, dst_offsets, src_tensors, src_offsets,
                              sizes, tensor_num, device_num);
}

bool bmrt_memcpy_d2d_stride_ex_parallel(
    void *p_bmrt,
    bm_tensor_t dst_tensors[],
    size_t dst_offsets[],
    bm_shape_t dst_strides[],
    bm_tensor_t src_tensors[],
    size_t src_offsets[],
    bm_shape_t src_strides[],
    bm_shape_t shapes[],
    int tensor_num[],
    int device_num) {
  return ((Bmruntime*)p_bmrt)
    ->memcpy_d2d_stride_ex_parallel(
        dst_tensors, dst_offsets, dst_strides,
        src_tensors, src_offsets, src_strides,
        shapes, tensor_num, device_num);
}
