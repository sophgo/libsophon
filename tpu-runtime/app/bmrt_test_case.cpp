/****************************************************************************
 * This file is to do case test of runtime interface
 ****************************************************************************/

#include "bmrt_test_inner.h"

using std::thread;

static set<string> g_net_name_set;

typedef struct {
  string net_name;  // net_name
  int stage_idx;
  vector<void *> ref_input_v;
  vector<bm_shape_t> input_shape_v;
  vector<bm_data_type_t> input_type_v;
  vector<void *> ref_output_v;
  vector<bm_shape_t> output_shape_v;
  vector<bm_data_type_t> output_type_v;
  int bmodel_idx;                  // for bmtap2
  vector<shape_t> input_shape2_v;  // for bmtap2
  vector<fmt_t> input_type2_v;     // for bmtap2
  vector<string> output_name_v;    // for bmtap2
} launch_unit_t;
vector<launch_unit_t> g_launch_unit_v;

static void result_cmp(int8_t *output_data[], launch_unit_t &launch_unit, vector<int> &count_v)
{
  if (NEED_CMP == false) {
    BMRT_LOG(INFO, "+++ The network[%s] stage[%d] done +++", launch_unit.net_name.c_str(),
             launch_unit.stage_idx);
    return;
  }
  int flag =
      result_cmp(output_data, (int8_t **)launch_unit.ref_output_v.data(),
                 launch_unit.ref_output_v.size(), count_v.data(), launch_unit.output_type_v.data());
  if (flag == 0) {
    BMRT_LOG(INFO, "+++ The network[%s] stage[%d] cmp success +++", launch_unit.net_name.c_str(),
             launch_unit.stage_idx);
  } else {
    BMRT_LOG(FATAL, "+++ The network[%s] stage[%d] cmp failed +++", launch_unit.net_name.c_str(),
             launch_unit.stage_idx);
  }
}

static void result_cmp(int8_t *output_data[], launch_unit_t &launch_unit)
{
  vector<int> count_v;
  for (auto &shape : launch_unit.output_shape_v) {
    count_v.push_back(bmrt_shape_count(&shape));
  }
  return result_cmp(output_data, launch_unit, count_v);
}

/* --------------------------------------------------------------------------*/
/* prepare for input data and output reference data */
extern shape_t bmnet_shape_convert(bm_shape_t bm_shape);            // for bmtap2
extern uint32_t bmnet_data_type_convert(bm_data_type_t data_type);  // for bmtap2
static void prepare_ref_data(
    const flatbuffers::Vector<flatbuffers::Offset<bmodel::Tensor>> *tensors, FILE *file,
    vector<void *> &data_v, vector<bm_shape_t> &shape_v, vector<bm_data_type_t> &type_v,
    vector<shape_t> *shape2_v = NULL, vector<fmt_t> *type2_v = NULL, vector<string> *name2_v = NULL,
    bool is_merge = false)
{
  size_t total_size = 0;
  for (uint32_t idx = 0; idx < tensors->size(); idx++) {
    auto tensor = tensors->Get(idx);
    bm_shape_t tensor_shape;
    vector<int> shape_data = {tensor->shape()->Get(0)->dim()->begin(),
                              tensor->shape()->Get(0)->dim()->end()};
    bmrt_shape(&tensor_shape, shape_data.data(), shape_data.size());
    shape_v.push_back(tensor_shape);
    if (shape2_v != NULL) {
      shape2_v->push_back(bmnet_shape_convert(tensor_shape));
    }

    bm_data_type_t type = (bm_data_type_t)tensor->data_type();
    type_v.push_back(type);
    if (type2_v != NULL) {
      type2_v->push_back(bmnet_data_type_convert(type));
    }

    if (name2_v != NULL) {
      name2_v->push_back(tensor->name()->str());
    }

    size_t size = bmrt_shape_count(&tensor_shape) * bmrt_data_type_size(type);
    total_size += size;
    if (!is_merge) {
      void *ref_data = malloc(size);
      if (ref_data == NULL || (NEED_CMP && 1 != fread(ref_data, size, 1, file))) {
        BMRT_LOG(FATAL, "Failed to fread reference data for the %d-th tensor", idx);
      }
      data_v.push_back(ref_data);
    }
  }
  if (is_merge) {
    void *ref_data = malloc(total_size);
    if (ref_data == NULL || (NEED_CMP && 1 != fread(ref_data, total_size, 1, file))) {
      BMRT_LOG(FATAL, "Failed to fread reference data");
    }
    data_v.push_back(ref_data);
  }
}

// read bmodel by ModelCtx, to get ref input and output data that belong to each stage of each net
static void prepare_test_data(bool is_bmtap2 = false)
{
  int num = CONTEXT_DIR_V.size();
  for (int bmodel_idx = 0; bmodel_idx < num; bmodel_idx++) {
    auto &dir = CONTEXT_DIR_V[bmodel_idx];
    if (dir.empty()) {
      continue;
    }
    FILE *f_input = NULL, *f_output = NULL;
    open_ref_file(dir, f_input, f_output);

    string bmodel_path = fix_bmodel_path(dir);
    auto bmodel = new ModelCtx(bmodel_path);
    if (bmodel == NULL || !(*bmodel)) {
      BMRT_LOG(FATAL, "load bmodel[%s] failed", bmodel_path.c_str());
    }
    uint32_t net_num = (is_bmtap2 ? 1 : bmodel->model()->net()->size());
    for (uint32_t net_idx = 0; net_idx < net_num; net_idx++) {
      auto net = bmodel->model()->net()->Get(net_idx);
      uint32_t stage_num = net->parameter()->size();
      g_net_name_set.insert(net->name()->str());
      for (uint32_t stage_idx = 0; stage_idx < stage_num; stage_idx++) {
        auto parameter = net->parameter()->Get(stage_idx);
        auto input_tensors = parameter->input_tensor();
        auto output_tensors = parameter->output_tensor();
        launch_unit_t launch_unit;
        launch_unit.bmodel_idx = bmodel_idx;
        launch_unit.net_name = net->name()->str();
        launch_unit.stage_idx = stage_idx;
        prepare_ref_data(input_tensors, f_input, launch_unit.ref_input_v, launch_unit.input_shape_v,
                         launch_unit.input_type_v, &launch_unit.input_shape2_v,
                         &launch_unit.input_type2_v, NULL, is_bmtap2);
        prepare_ref_data(output_tensors, f_output, launch_unit.ref_output_v,
                         launch_unit.output_shape_v, launch_unit.output_type_v, NULL, NULL,
                         &launch_unit.output_name_v);
        g_launch_unit_v.push_back(launch_unit);
      }
    }
    delete bmodel;
    if (NEED_CMP) {
      fclose(f_input);
      fclose(f_output);
    }
  }
}

static void free_test_data()
{
  for (auto &launch_unit : g_launch_unit_v) {
    for (auto &ref_output : launch_unit.ref_output_v) {
      free(ref_output);
    }
    for (auto &ref_input : launch_unit.ref_input_v) {
      free(ref_input);
    }
  }
  g_launch_unit_v.clear();
}

/* --------------------------------------------------------------------------*/
/* test bmrt api */
static void *g_bmrt = NULL;
static bm_handle_t g_bm_handle = NULL;

static void thread_entry_bmrt_load(int idx)
{
  if (TEST_CASE == "bmrt_load_context") {
    // load context
    string ctx_dir = CONTEXT_DIR_V[idx] + "/";
    bool flag = bmrt_load_context(g_bmrt, ctx_dir.c_str());
    if (!flag) {
      BMRT_LOG(FATAL, "Load context[%s] failed", CONTEXT_DIR_V[idx].c_str());
    }
  } else if (TEST_CASE == "bmrt_load_bmodel_data") {
    // load bmodel by data
    string bmodel_path = fix_bmodel_path(CONTEXT_DIR_V[idx]);
    FILE *f_bmodel = fopen(bmodel_path.c_str(), "rb");
    if (f_bmodel == NULL) {
      BMRT_LOG(FATAL, "Open bmodel[%s] failed", bmodel_path.c_str());
    }
    fseek(f_bmodel, 0, SEEK_END);
    size_t length = ftell(f_bmodel);
    rewind(f_bmodel);
    char *buffer = new char[length];
    if (1 != fread(buffer, length, 1, f_bmodel)) {
      BMRT_LOG(FATAL, "Failed to fread bmodel data [%s]", bmodel_path.c_str());
    }
    bool flag = bmrt_load_bmodel_data(g_bmrt, buffer, length);
    delete[] buffer;
    if (!flag) {
      BMRT_LOG(FATAL, "Load bmodel data[%s] failed", bmodel_path.c_str());
    }
    fclose(f_bmodel);
  } else {
    // load bmodel by file
    string bmodel_path = fix_bmodel_path(CONTEXT_DIR_V[idx]);
    bool flag = bmrt_load_bmodel(g_bmrt, bmodel_path.c_str());
    if (!flag) {
      BMRT_LOG(FATAL, "Load bmodel[%s] failed", bmodel_path.c_str());
    }
  }
}

// multi thread load bmodel
static void test_bmrt_load_bmodel()
{
  int num = CONTEXT_DIR_V.size();
  if (TEST_CASE == "bmrt_multi_thread") {
    #ifdef __linux__
    thread thread_v[num];
    #else
    std::shared_ptr<thread> thread_v_(new thread[num], std::default_delete<thread[]>());
    thread* thread_v = thread_v_.get();
    #endif
    for (int i = 0; i < num; i++) {
      thread_v[i] = thread(thread_entry_bmrt_load, i);
    }
    for (int i = 0; i < num; i++) {
      thread_v[i].join();
    }
    BMRT_LOG(INFO, "load bmodel, all threads have completed");
  } else {
    for (int i = 0; i < num; i++) {
      thread_entry_bmrt_load(i);
    }
  }
}

static void thread_entry_bmrt_launch(int thread_id, launch_unit_t *launch_unit)
{
  const char *net_name = launch_unit->net_name.c_str();
  int input_num = launch_unit->ref_input_v.size();
  int output_num = launch_unit->ref_output_v.size();
  vector<int> count_v;
  for (int loop = 0; loop < LOOP_NUM; loop++) {
    BMRT_LOG(INFO, "launch net[%s] stage[%d] thread[%d] loop[%d] ....", net_name,
             launch_unit->stage_idx, thread_id, loop);
    #ifdef __linux__
    void *output_datas[output_num];
    #else
    std::shared_ptr<void*> output_datas_(new void*[output_num], std::default_delete<void*[]>());
    void** output_datas = output_datas_.get();
    #endif
    if (TEST_CASE == "bmrt_launch_data") {
      #ifdef __linux__
      bm_shape_t output_shapes[output_num];
      void *input_datas[input_num];
      #else
      std::shared_ptr<bm_shape_t> output_shapes_(new bm_shape_t[output_num], std::default_delete<bm_shape_t[]>());
      bm_shape_t* output_shapes = output_shapes_.get();
      std::shared_ptr<void*> input_datas_(new void*[input_num], std::default_delete<void*[]>());
      void** input_datas = input_datas_.get();
      #endif
      for (int i = 0; i < input_num; i++) {
        input_datas[i] = launch_unit->ref_input_v[i];
      }
      bool ret = bmrt_launch_data(g_bmrt, net_name, input_datas, launch_unit->input_shape_v.data(),
                                  input_num, output_datas, output_shapes, output_num, false);
      if (!ret) {
        BMRT_LOG(FATAL, "launch net[%s] stage[%d] failed", net_name, launch_unit->stage_idx);
      }
      // sync, wait for finishing inference
      bm_thread_sync(g_bm_handle);
      for (int i = 0; i < output_num; i++) {
        count_v.push_back(bmrt_shape_count(&output_shapes[i]));
      }
    } else {
      #ifdef __linux__
      bm_tensor_t input_tensors[input_num];
      #else
      std::shared_ptr<bm_tensor_t> input_tensors_(new bm_tensor_t[input_num], std::default_delete<bm_tensor_t[]>());
      bm_tensor_t* input_tensors = input_tensors_.get();
      #endif
      for (int i = 0; i < input_num; i++) {
        bmrt_tensor(&input_tensors[i], g_bmrt, launch_unit->input_type_v[i],
                    launch_unit->input_shape_v[i]);
        bm_memcpy_s2d(g_bm_handle, input_tensors[i].device_mem, launch_unit->ref_input_v[i]);
      }
      #ifdef __linux__
      bm_tensor_t output_tensors[output_num];
      #else
      std::shared_ptr<bm_tensor_t> output_tensors_(new bm_tensor_t[output_num], std::default_delete<bm_tensor_t[]>());
      bm_tensor_t* output_tensors = output_tensors_.get();
      #endif
      bool ret = bmrt_launch_tensor(g_bmrt, net_name, input_tensors, input_num, output_tensors,
                                    output_num);
      if (!ret) {
        BMRT_LOG(FATAL, "launch net[%s] stage[%d] failed", net_name, launch_unit->stage_idx);
      }
      // sync, wait for finishing inference
      bm_thread_sync(g_bm_handle);
      for (int i = 0; i < output_num; ++i) {
        auto &output_tensor = output_tensors[i];
        size_t size = bmrt_tensor_bytesize(&output_tensor);
        count_v.push_back(bmrt_shape_count(&output_tensors[i].shape));
        output_datas[i] = malloc(size);
        bm_memcpy_d2s_partial(g_bm_handle, output_datas[i], output_tensor.device_mem, size);
        bm_free_device(g_bm_handle, output_tensor.device_mem);
      }
      for (int i = 0; i < input_num; i++) {
        bm_free_device(g_bm_handle, input_tensors[i].device_mem);
      }
    }

    // compare inference output data with reference data
    result_cmp((int8_t **)output_datas, *launch_unit, count_v);

    // free memory
    for (int i = 0; i < output_num; ++i) {
      free(output_datas[i]);
    }
  }
}

static bool test_bmrt_get_info(const bm_net_info_t *net_info, const launch_unit_t *launch_unit)
{
  uint32_t input_num = net_info->input_num;
  uint32_t output_num = net_info->output_num;
  int stage_idx = launch_unit->stage_idx;
  if (input_num != launch_unit->ref_input_v.size()) {
    return false;
  }
  if (output_num != launch_unit->ref_output_v.size()) {
    return false;
  }
  for (uint32_t idx = 0; idx < input_num; idx++) {
    if (net_info->input_dtypes[idx] != launch_unit->input_type_v[idx]) {
      return false;
    }
    if (false == bmrt_shape_is_same(&net_info->stages[stage_idx].input_shapes[idx],
                                    &launch_unit->input_shape_v[idx])) {
      return false;
    }
    size_t size = bmrt_shape_count(&launch_unit->input_shape_v[idx]) * bmrt_data_type_size(net_info->input_dtypes[idx]);
    if (size > net_info->max_input_bytes[idx]) {
      return false;
    }
  }
  for (uint32_t idx = 0; idx < output_num; idx++) {
    if (net_info->output_dtypes[idx] != launch_unit->output_type_v[idx]) {
      return false;
    }
    if (false == bmrt_shape_is_same(&net_info->stages[stage_idx].output_shapes[idx],
                                    &launch_unit->output_shape_v[idx])) {
      return false;
    }
    size_t size = bmrt_shape_count(&launch_unit->output_shape_v[idx]) * bmrt_data_type_size(net_info->output_dtypes[idx]);
    if (size > net_info->max_output_bytes[idx]) {
      return false;
    }
  }
  return true;
}

static void test_bmrt_simple_api()
{
  /* test bmrt_get_network_num */
  uint32_t net_num = bmrt_get_network_number(g_bmrt);
  if (net_num != g_net_name_set.size()) {
    BMRT_LOG(FATAL, "bmrt_get_network_number failed");
  }
  /* test bmrt_get_network_names */
  const char **net_names = NULL;
  bmrt_get_network_names(g_bmrt, &net_names);
  for (uint32_t i = 0; i < net_num; i++) {
    if (g_net_name_set.end() == g_net_name_set.find(net_names[i])) {
      BMRT_LOG(FATAL, "bmrt_get_network_names failed");
    }
  }
  free(net_names);

  /* test bmrt_get_network_info */
  for (auto &launch_unit : g_launch_unit_v) {
    auto net_info = bmrt_get_network_info(g_bmrt, launch_unit.net_name.c_str());
    if (net_info == NULL) {
      BMRT_LOG(FATAL, "bmrt_get_network_info [%s] failed", launch_unit.net_name.c_str());
    }
    bmrt_print_network_info(net_info);
    if (false == test_bmrt_get_info(net_info, &launch_unit)) {
      BMRT_LOG(FATAL, "bmrt_get_network_info not correct");
    }
  }
}

static void test_bmrt_launch()
{
  uint32_t launch_num = g_launch_unit_v.size();
  if (TEST_CASE == "bmrt_multi_thread") {  // multi-thread
    uint32_t total_num = launch_num * THREAD_NUM;
    #ifdef __linux__
    thread thread_v[total_num];
    #else
    std::shared_ptr<thread> thread_v_(new thread[total_num], std::default_delete<thread[]>());
    thread* thread_v = thread_v_.get();
    #endif
    int index = 0;

    for (uint32_t i = 0; i < launch_num; i++) {
      for (int j = 0; j < THREAD_NUM; j++) {
        thread_v[index] = thread(thread_entry_bmrt_launch, j, &g_launch_unit_v[i]);
        index++;
      }
    }

    for (uint32_t i = 0; i < total_num; i++) {
      thread_v[i].join();
    }
  } else {  // single-thread
    for (uint32_t i = 0; i < launch_num; i++) {
      thread_entry_bmrt_launch(0, &g_launch_unit_v[i]);
    }
  }
}

static void bmrt_api_test_case()
{
  // prepare test data
  prepare_test_data();
  bm_device_mem_t prealloc_mem;
  // create Bmruntime
  bm_status_t ret = bm_dev_request(&g_bm_handle, DEV_ID);
  if (ret != BM_SUCCESS) {
    BMRT_LOG(FATAL, "bm_dev_request failed, ret:[%d]", ret);
  }
  if (PREALLOC_SIZE != 0) {
    ret = bm_malloc_device_byte(g_bm_handle, &prealloc_mem, PREALLOC_SIZE);
    if (ret != BM_SUCCESS) {
      BMRT_LOG(FATAL, "prealloc device mem failed, size[0x%lx]", PREALLOC_SIZE);
    } else {
      BMRT_LOG(INFO, "prealloc device mem, base[0x%llx], size[0x%x]",
               bm_mem_get_device_addr(prealloc_mem), bm_mem_get_device_size(prealloc_mem));
    }
  }
  g_bmrt = bmrt_create(g_bm_handle);
  if (g_bmrt == NULL) {
    BMRT_LOG(FATAL, "create runtime failed");
  }

  // load context dir
  test_bmrt_load_bmodel();

  if (TEST_CASE == "bmrt_simple_api") {
    test_bmrt_simple_api();
  } else {
    test_bmrt_launch();
  }

  bmrt_destroy(g_bmrt);

  if (PREALLOC_SIZE != 0) {
    bm_free_device(g_bm_handle, prealloc_mem);
  }

  bm_dev_free(g_bm_handle);

  // free test data
  free_test_data();
}

/* --------------------------------------------------------------------------*/
/* test bmruntime cpp api */
static bmruntime::Context *g_context = NULL;

static void thread_entry_bmcpp_load(int idx)
{
  if (CONTEXT_DIR_V[idx].empty()) {
    return;
  }
  bm_status_t status = BM_SUCCESS;
  if (TEST_CASE == "bmrt_load_bmodel_data") {
    // load bmodel by data
    string bmodel_path = fix_bmodel_path(CONTEXT_DIR_V[idx]);
    FILE *f_bmodel = fopen(bmodel_path.c_str(), "rb");
    if (f_bmodel == NULL) {
      BMRT_LOG(FATAL, "Open bmodel[%s] failed", bmodel_path.c_str());
    }
    fseek(f_bmodel, 0, SEEK_END);
    size_t length = ftell(f_bmodel);
    rewind(f_bmodel);
    char *buffer = new char[length];
    if (1 != fread(buffer, length, 1, f_bmodel)) {
      BMRT_LOG(FATAL, "Failed to fread bmodel data [%s]", bmodel_path.c_str());
    }
    fclose(f_bmodel);
    status = g_context->load_bmodel(buffer, length);
    delete[] buffer;
    if (status != BM_SUCCESS) {
      BMRT_LOG(FATAL, "Load bmodel data[%s] failed", bmodel_path.c_str());
    }
  } else {
    // load bmodel by file
    string bmodel_path = fix_bmodel_path(CONTEXT_DIR_V[idx]);
    status = g_context->load_bmodel(bmodel_path.c_str());
    if (status != BM_SUCCESS) {
      BMRT_LOG(FATAL, "Load bmodel[%s] failed", bmodel_path.c_str());
    }
  }
}

// multi thread load bmodel
static void test_bmcpp_load_bmodel()
{
  int num = CONTEXT_DIR_V.size();
  if (TEST_CASE == "bmcpp_multi_thread") {
    #ifdef __linux__
    thread thread_v[num];
    #else
    std::shared_ptr<thread> thread_v_(new thread[num], std::default_delete<thread[]>());
    thread* thread_v = thread_v_.get();
    #endif
    for (int i = 0; i < num; i++) {
      thread_v[i] = thread(thread_entry_bmcpp_load, i);
    }
    for (int i = 0; i < num; i++) {
      thread_v[i].join();
    }
    BMRT_LOG(INFO, "load bmodel, all threads have completed");
  } else {
    for (int i = 0; i < num; i++) {
      thread_entry_bmcpp_load(i);
    }
  }
}

static void thread_entry_bmcpp_launch(int thread_id, launch_unit_t *launch_unit)
{
  bm_status_t status = BM_SUCCESS;
  const char *net_name = launch_unit->net_name.c_str();
  uint32_t input_num = launch_unit->ref_input_v.size();
  uint32_t output_num = launch_unit->ref_output_v.size();
  for (int loop = 0; loop < LOOP_NUM; loop++) {
    BMRT_LOG(INFO, "launch net[%s] stage[%d] thread[%d] loop[%d] ....", net_name,
             launch_unit->stage_idx, thread_id, loop);
    bmruntime::Network net(*g_context, net_name, launch_unit->stage_idx);
    auto &input_tensors = net.Inputs();
    if (input_tensors.size() != input_num) {
      BMRT_LOG(FATAL, "input num[%lu] not equal to [%u]", input_tensors.size(), input_num);
    }
    for (uint32_t i = 0; i < input_num; i++) {
      status = input_tensors[i]->CopyFrom(launch_unit->ref_input_v[i]);
      if (status != BM_SUCCESS) {
        BMRT_LOG(FATAL, "input[%u] CopyFrom failed", i);
      }
    }
    status = net.Forward();
    if (status != BM_SUCCESS) {
      BMRT_LOG(FATAL, "Forward failed");
    }
    auto &output_tensors = net.Outputs();
    if (output_tensors.size() != output_num) {
      BMRT_LOG(FATAL, "output num[%lu] not equal to [%u]", output_tensors.size(), output_num);
    }
    #ifdef __linux__
    void *output_datas[output_num];
    #else
    std::shared_ptr<void*> output_datas_(new void*[output_num], std::default_delete<void*[]>());
    void** output_datas = output_datas_.get();
    #endif
    for (uint32_t i = 0; i < output_num; ++i) {
      size_t size = output_tensors[i]->ByteSize();
      output_datas[i] = malloc(size);
      status = output_tensors[i]->CopyTo(output_datas[i]);
      if (status != BM_SUCCESS) {
        BMRT_LOG(FATAL, "output[%u] CopyTo failed", i);
      }
    }

    // compare inference output data with reference data
    vector<int> count_v;
    for (uint32_t i = 0; i < output_num; i++) {
      count_v.push_back(output_tensors[i]->num_elements());
    }
    result_cmp((int8_t **)output_datas, *launch_unit, count_v);

    // free memory
    for (uint32_t i = 0; i < output_num; ++i) {
      free(output_datas[i]);
    }
  }
}

static void test_bmcpp_reshape()
{
  for (auto &name : g_net_name_set) {
    bmruntime::Network net(*g_context, name.c_str());
    for (auto &launch_unit : g_launch_unit_v) {
      if (launch_unit.net_name != name) {
        continue;
      }
      for (int loop = 0; loop < LOOP_NUM; loop++) {
        BMRT_LOG(INFO, "launch net[%s] stage[%d] loop[%d] ....", launch_unit.net_name.c_str(),
                 launch_unit.stage_idx, loop);
        for (uint32_t idx = 0; idx < net.Inputs().size(); idx++) {
          auto tensor = net.Inputs().at(idx);
          if (BM_SUCCESS != tensor->Reshape(launch_unit.input_shape_v[idx])) {
            BMRT_LOG(FATAL, "Reshape input[%u] failed", idx);
          }
          if (BM_SUCCESS != tensor->CopyFrom(launch_unit.ref_input_v[idx])) {
            BMRT_LOG(FATAL, "input[%u] CopyFrom failed", idx);
          }
        }
        net.Forward(true);
        uint32_t output_num = net.Outputs().size();
        #ifdef __linux__
        void *output_datas[output_num];
        #else
        std::shared_ptr<void*> output_datas_(new void*[output_num], std::default_delete<void*[]>());
        void** output_datas = output_datas_.get();
        #endif
        vector<int> count_v;
        for (uint32_t i = 0; i < output_num; ++i) {
          size_t size = net.Outputs()[i]->ByteSize();
          output_datas[i] = malloc(size);
          if (BM_SUCCESS != net.Outputs()[i]->CopyTo(output_datas[i])) {
            BMRT_LOG(FATAL, "output[%u] CopyTo failed", i);
          }
          count_v.push_back(Count(*net.Outputs()[i]));
        }
        // compare inference output data with reference data
        result_cmp((int8_t **)output_datas, launch_unit, count_v);

        // free memory
        for (uint32_t i = 0; i < output_num; ++i) {
          free(output_datas[i]);
        }
      }
    }
  }
}

static void test_bmcpp_launch()
{
  uint32_t launch_num = g_launch_unit_v.size();
  if (TEST_CASE == "bmcpp_multi_thread") {  // multi-thread
    uint32_t total_num = launch_num * THREAD_NUM;
    #ifdef __linux__
    thread thread_v[total_num];
    #else
    std::shared_ptr<thread> thread_v_(new thread[total_num], std::default_delete<thread[]>());
    thread* thread_v = thread_v_.get();
    #endif
    int index = 0;
    for (uint32_t i = 0; i < launch_num; i++) {
      for (int j = 0; j < THREAD_NUM; j++) {
        thread_v[index] = thread(thread_entry_bmcpp_launch, j, &g_launch_unit_v[i]);
        index++;
      }
    }

    for (uint32_t i = 0; i < launch_num; i++) {
      thread_v[i].join();
    }
  } else if (TEST_CASE == "bmcpp_reshape") {
    test_bmcpp_reshape();
  } else {  // single-thread
    for (uint32_t i = 0; i < launch_num; i++) {
      thread_entry_bmcpp_launch(0, &g_launch_unit_v[i]);
    }
  }
}
static void bmcpp_api_test_case()
{
  prepare_test_data();
  bmruntime::Context ctx(DEV_ID);
  g_context = &ctx;
  g_bm_handle = ctx.handle();
  test_bmcpp_load_bmodel();
  test_bmcpp_launch();
  free_test_data();
}

/* --------------------------------------------------------------------------*/
/* test bmtap2 c api */
static vector<bmnet_t> g_bmnet_v;
static bmctx_t g_ctx;
static void thread_entry_bmtap2_load(int idx)
{
  bmerr_t ret = BM_SUCCESS;
  if (CONTEXT_DIR_V[idx].empty()) {
    return;
  }

  if (TEST_CASE == "bmtap2_register_data") {
    // load bmodel by data
    string bmodel_path = CONTEXT_DIR_V[idx] + "/compilation.bmodel";
    FILE *f_bmodel = fopen(bmodel_path.c_str(), "rb");
    if (f_bmodel == NULL) {
      BMRT_LOG(FATAL, "Open bmodel[%s] failed", bmodel_path.c_str());
    }
    fseek(f_bmodel, 0, SEEK_END);
    size_t length = ftell(f_bmodel);
    rewind(f_bmodel);
    uint8_t *buffer = new uint8_t[length];
    if (1 != fread(buffer, length, 1, f_bmodel)) {
      BMRT_LOG(FATAL, "Failed to fread bmodel data [%s]", bmodel_path.c_str());
    }
    ret = bmnet_register_bmodel_data(g_ctx, buffer, length, &g_bmnet_v[idx]);
    delete[] buffer;
    if (ret != BM_SUCCESS) {
      BMRT_LOG(FATAL, "Load bmodel data[%s] failed", bmodel_path.c_str());
    }
    fclose(f_bmodel);
  } else {
    // load bmodel by file
    string bmodel_path = CONTEXT_DIR_V[idx] + "/compilation.bmodel";
    ret = bmnet_register_bmodel(g_ctx, bmodel_path.c_str(), &g_bmnet_v[idx]);
    if (ret != BM_SUCCESS) {
      BMRT_LOG(FATAL, "Load bmodel[%s] failed", bmodel_path.c_str());
    }
  }
}

// multi thread load bmodel
static void test_bmtap2_load_bmodel()
{
  int num = CONTEXT_DIR_V.size();
  g_bmnet_v.clear();
  g_bmnet_v.assign(num, NULL);
  if (TEST_CASE == "bmtap2_multi_thread") {
    #ifdef __linux__
    thread thread_v[num];
    #else
    std::shared_ptr<thread> thread_v_(new thread[num], std::default_delete<thread[]>());
    thread* thread_v = thread_v_.get();
    #endif
    for (int i = 0; i < num; i++) {
      thread_v[i] = thread(thread_entry_bmtap2_load, i);
    }
    for (int i = 0; i < num; i++) {
      thread_v[i].join();
    }
  } else {
    for (int i = 0; i < num; i++) {
      thread_entry_bmtap2_load(i);
    }
  }
}

static void thread_entry_bmtap2_run(int thread_id, int idx)
{
  bmerr_t ret = BM_SUCCESS;
  bmnet_output_info_t output_info;

  for (auto &launch_unit : g_launch_unit_v) {
    if (launch_unit.bmodel_idx != idx) {
      continue;
    }
    for (int loop = 0; loop < LOOP_NUM; loop++) {
      BMRT_LOG(INFO, "===> launch net[%s] stage[%d] thread[%d] loop[%d] ....",
              launch_unit.net_name.c_str(), launch_unit.stage_idx, thread_id, loop);
      int input_num = launch_unit.input_shape_v.size();
      int output_num = launch_unit.ref_output_v.size();
      ret = bmnet_set_input_shape2(g_bmnet_v[idx], launch_unit.input_shape2_v.data(), input_num);
      if (ret != BM_SUCCESS) {
        BMRT_LOG(FATAL, "bmnet_set_input_shape2 failed");
      }
      ret = bmnet_get_output_info(g_bmnet_v[idx], &output_info);
      if (ret != BM_SUCCESS) {
        BMRT_LOG(FATAL, "bmnet_get_output_info failed");
      }
      uint8_t *output = new uint8_t[output_info.output_size];
      memset(output, 0, output_info.output_size);
      ret = bmnet_inference(g_bmnet_v[idx], (uint8_t *)launch_unit.ref_input_v[0], output);
      if (ret != BM_SUCCESS) {
        BMRT_LOG(FATAL, "bmnet_inference failed");
      }
      #ifdef __linux__
      void *output_datas[output_num];
      #else
      std::shared_ptr<void*> output_datas_(new void*[output_num], std::default_delete<void*[]>());
      void** output_datas = output_datas_.get();
      #endif
      uint8_t *p_out = output;
      for (int i = 0; i < output_num; i++) {
        output_datas[i] = p_out;
        p_out += output_info.size_array[i];
      }

      result_cmp((int8_t **)output_datas, launch_unit);

      delete[] output;
    }
  }
}

static void test_bmtap2_run()
{
  int num = CONTEXT_DIR_V.size();
  if (TEST_CASE == "bmtap2_multi_thread") {  // multi-thread
    int total_num = num * THREAD_NUM;
    #ifdef __linux__
    thread thread_v[total_num];
    #else
    std::shared_ptr<thread> thread_v_(new thread[total_num], std::default_delete<thread[]>());
    thread* thread_v = thread_v_.get();
    #endif
    int index = 0;
    for (int i = 0; i < num; i++) {
      for (int j = 0; j < THREAD_NUM; j++) {
        thread_v[index] = thread(thread_entry_bmtap2_run, j, i);
        index ++;
      }
    }
    for (int i = 0; i < num; i++) {
      thread_v[i].join();
    }
  } else {  // single-thread
    for (int i = 0; i < num; i++) {
      thread_entry_bmtap2_run(0, i);
    }
  }
}

static void test_bmtap2_cleanup()
{
  for (auto &net : g_bmnet_v) {
    if (net != NULL) {
      bmnet_cleanup(net);
    }
  }
}

static void bmtap2_api_test_case()
{
  bmerr_t ret = BM_SUCCESS;
  prepare_test_data(true);

  ret = bm_init(0, &g_ctx);
  if (ret != BM_SUCCESS) {
    BMRT_LOG(FATAL, "bm_init failed");
  }

  test_bmtap2_load_bmodel();
  test_bmtap2_run();
  test_bmtap2_cleanup();

  bm_exit(g_ctx);
  free_test_data();
}

/* --------------------------------------------------------------------------*/
/* test bmtap2 c++ api */
static void thread_entry_bmtap2cpp_run(int thread_id, int idx)
{
  if (CONTEXT_DIR_V[idx].empty()) {
    return;
  }
  string path = CONTEXT_DIR_V[idx] + "/compilation.bmodel";
  bmruntime::Net net(path);

  for (auto &launch_unit : g_launch_unit_v) {
    if (launch_unit.bmodel_idx != idx) {
      continue;
    }
    for (int loop = 0; loop < LOOP_NUM; loop++) {
      BMRT_LOG(INFO, "===> launch net[%s] stage[%d] thread[%d] loop[%d] ....", launch_unit.net_name.c_str(),
             launch_unit.stage_idx, thread_id, loop);
      int input_num = launch_unit.input_shape_v.size();
      int output_num = launch_unit.ref_output_v.size();
      vector<bmruntime::Blob> input_blobs;
      for (int i = 0; i < input_num; i++) {
        input_blobs.push_back(bmruntime::Blob(launch_unit.ref_input_v[i],
                                              launch_unit.input_shape2_v[i],
                                              launch_unit.input_type2_v[i]));
      }
      net.forward(input_blobs);
      #ifdef __linux__
      void *output_datas[output_num];
      #else
      std::shared_ptr<void*> output_datas_(new void*[output_num], std::default_delete<void*[]>());
      void** output_datas = output_datas_.get();
      #endif
      for (int i = 0; i < output_num; i++) {
        output_datas[i] = net.output(launch_unit.output_name_v[i])->data();
      }

      result_cmp((int8_t **)output_datas, launch_unit);
    }
  }
}

static void test_bmtap2cpp_run()
{
  int launch_num = CONTEXT_DIR_V.size();
  if (TEST_CASE == "bmtap2cpp_multi_thread") {  // multi-thread
    int total_num = THREAD_NUM * launch_num;
    #ifdef __linux__
    thread thread_v[total_num];
    #else
    std::shared_ptr<thread> thread_v_(new thread[total_num], std::default_delete<thread[]>());
    thread* thread_v = thread_v_.get();
    #endif
    int index = 0;
    for (int i = 0; i < launch_num; i++) {
      for (int j = 0; j < THREAD_NUM; j++) {
        thread_v[index] = thread(thread_entry_bmtap2cpp_run, j, i);
        index++;
      }
    }

    for (int i = 0; i < total_num; i++) {
      thread_v[i].join();
    }
  } else {  // single-thread
    for (int i = 0; i < launch_num; i++) {
      thread_entry_bmtap2cpp_run(0, i);
    }
  }
}

static void bmtap2cpp_api_test_case()
{
  prepare_test_data(true);

  test_bmtap2cpp_run();

  free_test_data();
}

/* --------------------------------------------------------------------------*/
/* main test case process */
typedef void TestPtr();
typedef struct {
  const char *prefix;
  TestPtr *test_f;
} test_pair_t;

test_pair_t test_pair[] = {{"bmrt", bmrt_api_test_case},
                           {"bmcpp", bmcpp_api_test_case},
                           {"bmtap2", bmtap2_api_test_case},
                           {"bmtap2cpp", bmtap2cpp_api_test_case}};

void bmrt_test_case()
{
  BMRT_LOG(INFO, "==> begin test case : %s", TEST_CASE.c_str());
  for (uint32_t i = 0; i < sizeof(test_pair) / sizeof(test_pair_t); i++) {
    auto &pair = test_pair[i];
    if (TEST_CASE.compare(0, strlen(pair.prefix), pair.prefix) == 0) {
      pair.test_f();
      BMRT_LOG(INFO, "+++ test case[%s] success +++", TEST_CASE.c_str());
      return;
    }
  }
  BMRT_LOG(FATAL, "unknown test case[%s]", TEST_CASE.c_str());
}
