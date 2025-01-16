#include <iostream>
#include <cstdlib>
#include <vector>
#include <assert.h>

#include "bmruntime_interface.h"

/* bmruntime demo with basic C interface */

int demo(int argc, char** argv) {
  // set your bmodel path here
  char bmodel[] = "./resnet50_static.bmodel";

  // set input shape here
  std::vector<int> input_shape = {4,3,224,224};

  // request bm_handle
  bm_handle_t bm_handle;
  bm_status_t status = bm_dev_request(&bm_handle, 0);
  assert(BM_SUCCESS == status);

  // create bmruntime
  void *p_bmrt = bmrt_create(bm_handle);
  assert(NULL != p_bmrt);

  // load bmodel by file
  bool ret = bmrt_load_bmodel(p_bmrt, bmodel);
  assert(true == ret);

  // set net name here
  const char** net_names = NULL;
  bmrt_get_network_names(p_bmrt, &net_names);
  const bm_net_info_t* net_info = bmrt_get_network_info(p_bmrt, net_names[0]);
  assert(NULL != net_info);

  // init all input tensors and malloc device memory
  // change input_num and dtype according to actual model
  assert(net_info->input_num == 1);
  bm_tensor_t input_tensors;
  status = bm_malloc_device_byte(bm_handle, &input_tensors.device_mem,
                                 net_info->max_input_bytes[0]);
  assert(BM_SUCCESS == status);
  input_tensors.dtype = BM_INT8;
  input_tensors.st_mode = BM_STORE_1N;

  // init output tensors and malloc device memory
  // change output_num according to actual model
  assert(net_info->input_num == 1);
  bm_tensor_t output_tensors;
  status = bm_malloc_device_byte(bm_handle, &output_tensors.device_mem,
                                 net_info->max_output_bytes[0]);
  assert(BM_SUCCESS == status);

  // before inference, set input shape and prepare input data
  // here input0 is system buffer pointer.
  input_tensors.shape.num_dims = (int)input_shape.size();
  for (int i = 0; i < (int) input_shape.size(); i++) {
    input_tensors.shape.dims[i] = input_shape[i];
  }
  // input0 is the input_data of your model, you need init it and move it into device memory
  char* input0 = (char*)malloc(bmrt_tensor_bytesize(&input_tensors));
  for (int i = 0; i < (int)bmrt_tensor_bytesize(&input_tensors); i++) {
    input0[i] = rand() % 255;
  }
  bm_memcpy_s2d_partial(bm_handle, input_tensors.device_mem, (void *)input0,
                        bmrt_tensor_bytesize(&input_tensors));

  // begin inference
  ret = bmrt_launch_tensor_ex(p_bmrt, net_names[0], &input_tensors, 1,
                              &output_tensors, 1, true, false);
  assert(true == ret);

  // sync, wait for finishing inference
  bm_thread_sync(bm_handle);

  /**************************************************************/
  // here all output info stored in output_tensors, such as data type, shape, device_mem.
  // you can copy data to system memory, like this.
  // here output0 are system buffers to store result.
  char* output0 = (char*)malloc(bmrt_tensor_bytesize(&output_tensors));
  bm_memcpy_d2s_partial(bm_handle, output0, output_tensors.device_mem,
                        bmrt_tensor_bytesize(&output_tensors));

  // print output shape and data
  // you can get output shape and data like this
  std::cout << "\n===================\n";
  std::cout << "print output info..." << std::endl;
  std::cout << "output data type: " << output_tensors.dtype << std::endl;
  std::cout << "output shape: [";
  for (int i = 0; i < output_tensors.shape.num_dims; i++) {
    std::cout << output_tensors.shape.dims[i] << " ";
  }
  std::cout << "]" << std::endl;
  std::cout << "output data:<";
  for (int i = 0; i < 10; i++) {
    std::cout << ((float*)output0)[i] << " ";
  }
  std::cout << " ...>" << std::endl;
  std::cout << "===================" << std::endl;

  // at last, free device memory
  free(net_names);
  free(input0);
  free(output0);
  bm_free_device(bm_handle, input_tensors.device_mem);
  bm_free_device(bm_handle, output_tensors.device_mem);

  bmrt_destroy(p_bmrt);
  bm_dev_free(bm_handle);

  return 0;
}

void malloc_device_mem(bm_handle_t bm_handle, memory_t &mem, std::vector<bm_device_mem_t> &prealloc_mem_v) {
  if (mem.size > 0) {
    bm_device_mem_t dmem;
    if (bm_malloc_device_byte(bm_handle, &dmem, mem.size * mem.number) == BM_SUCCESS) {
       mem.addr = bm_mem_get_device_addr(dmem);
       prealloc_mem_v.push_back(dmem);
    }
  }
}

void bmrt_launch_tensor_thread_func(
  bm_handle_t &handle, void *p_bmrt, const char *net_name,
  const bm_tensor_t *input_tensors,
  int input_num, bm_tensor_t *output_tensors,
  int output_num, bool user_mem,
  bool user_stmode, const int *core_list,
  int core_num) {

  bool ret = bmrt_launch_tensor_multi_cores(p_bmrt, net_name, input_tensors, input_num,
            output_tensors, output_num, user_mem, user_stmode, core_list, core_num);
  if (ret == false) {
    printf("The neuron network '%s' launch failed", net_name);
    exit(-1);
  }
  for (int i = 0; i < core_num; ++i) {
    auto core_id = core_list[i];
    bm_status_t core_status = bm_thread_sync_from_core(handle, core_id);
    if (core_status != BM_SUCCESS) {
      printf("The neuron network '%s' sync failed, core=%d", net_name, core_id);
      exit(-1);
    }
  }
}

int demo_memory_decoupling(int argc, char** argv) {
  // set your bmodel path here
  char bmodel[] = "./resnet50_static.bmodel";

  // set input shape here
  std::vector<int> input_shape = {4,3,224,224};

  std::vector<std::vector<int32_t>> core_lists;
  core_lists.push_back({0});
  core_lists.push_back({1});

  // request bm_handle
  bm_handle_t bm_handle;
  bm_status_t status = bm_dev_request(&bm_handle, 0);
  assert(BM_SUCCESS == status);

  // create bmruntime
  void *p_bmrt = bmrt_create(bm_handle);
  assert(NULL != p_bmrt);

  // get bmodel memory information
  mem_info_t mem_info;
  if (bmrt_get_bmodel_info(bmodel, &mem_info) == false) {
    assert(0);
  }

  // alloc device memory
  std::vector<bm_device_mem_t> prealloc_mem_v;
  malloc_device_mem(bm_handle, mem_info.instruction_mem, prealloc_mem_v);
  malloc_device_mem(bm_handle, mem_info.variable_instruction_mem, prealloc_mem_v);
  mem_info.neuron_mem.number = core_lists.size();
  malloc_device_mem(bm_handle, mem_info.neuron_mem, prealloc_mem_v);
  malloc_device_mem(bm_handle, mem_info.coeff_mem, prealloc_mem_v);
  mem_info.io_mem.number = core_lists.size();
  malloc_device_mem(bm_handle, mem_info.io_mem, prealloc_mem_v);

  // load bmodel by file
  bool ret = bmrt_load_bmodel_with_mem(p_bmrt, bmodel, &mem_info);
  assert(true == ret);

  // set net name here
  // const char** net_names = NULL;
  // bmrt_get_network_names(p_bmrt, &net_names);
  const char* net_name = bmrt_get_network_name(p_bmrt, 0);
  const bm_net_info_t* net_info = bmrt_get_network_info(p_bmrt, net_name);
  assert(NULL != net_info);

  // init all input tensors and malloc device memory
  // change input_num and dtype according to actual model
  assert(net_info->input_num == 1);
  bm_tensor_t input_tensors;
  // before inference, set input shape and prepare input data
  // here input0 is system buffer pointer.
  input_tensors.shape.num_dims = (int)input_shape.size();
  for (int i = 0; i < (int) input_shape.size(); i++) {
    input_tensors.shape.dims[i] = input_shape[i];
  }
  input_tensors.dtype = BM_INT8;
  input_tensors.st_mode = BM_STORE_1N;
  int stage_idx = bmrt_get_stage_index(p_bmrt, net_name, &input_tensors);
  // input tensor device memory using io_mem
  bmrt_tensor_with_device(
        &input_tensors, net_info->stages[stage_idx].input_mems[0],
        net_info->input_dtypes[0], net_info->stages[stage_idx].input_shapes[0]);
  // status = bm_malloc_device_byte(bm_handle, &input_tensors.device_mem,
  //                                net_info->max_input_bytes[0]);
  // assert(BM_SUCCESS == status);

  // init output tensors and malloc device memory
  // change output_num according to actual model
  assert(net_info->input_num == 1);
  std::vector<bm_tensor_t> output_tensors(core_lists.size());
  for (int i = 0; i < output_tensors.size(); ++i) {
    status = bm_malloc_device_byte(bm_handle, &(output_tensors[i].device_mem),
                                   net_info->max_output_bytes[0]);
    assert(BM_SUCCESS == status);
  }

  // input0 is the input_data of your model, you need init it and move it into device memory
  char* input0 = (char*)malloc(bmrt_tensor_bytesize(&input_tensors));
  for (int i = 0; i < (int)bmrt_tensor_bytesize(&input_tensors); i++) {
    input0[i] = rand() % 255;
  }
  bm_memcpy_s2d_partial(bm_handle, input_tensors.device_mem, (void *)input0,
                        bmrt_tensor_bytesize(&input_tensors));

  bool use_multi_thread = true;
  std::vector<std::thread> threads;
  if (use_multi_thread) {
    threads.resize(core_lists.size());
  }
  for(size_t group_idx = 0; group_idx<core_lists.size(); group_idx++) {
    auto& core_list = core_lists[group_idx];
    bool pre_alloc_neuron_ret =  bmrt_pre_alloc_neuron_multi_thread(p_bmrt, net_name, group_idx, core_list.data(), core_list.size());
  }

  if (use_multi_thread) {
    for(size_t group_idx = 0; group_idx<core_lists.size(); group_idx++){
      auto& core_list = core_lists[group_idx];
      threads[group_idx] = std::move(std::thread(&bmrt_launch_tensor_thread_func,
                                                std::ref(bm_handle), p_bmrt, net_name, &input_tensors, 1,
                                                &(output_tensors[group_idx]), 1, true, false,
                                                core_list.data(), core_list.size()));
    }
    for (auto& thread: threads) {
      thread.join();
    }
  } else {
    for(size_t group_idx = 0; group_idx<core_lists.size(); group_idx++){
      auto& core_list = core_lists[group_idx];
      bmrt_launch_tensor_thread_func(bm_handle, p_bmrt, net_name, &input_tensors, 1,
                                     &(output_tensors[group_idx]), 1, true, false,
                                     core_list.data(), core_list.size());
    }
  }

  // // begin inference
  // ret = bmrt_launch_tensor_ex(p_bmrt, net_names[0], &input_tensors, 1,
  //                             &output_tensors, 1, true, false);
  // assert(true == ret);

  // // sync, wait for finishing inference
  // bm_thread_sync(bm_handle);

  /**************************************************************/
  // here all output info stored in output_tensors, such as data type, shape, device_mem.
  // you can copy data to system memory, like this.
  // here output0 are system buffers to store result.
  char* output0 = (char*)malloc(bmrt_tensor_bytesize(&(output_tensors[0])));
  bm_memcpy_d2s_partial(bm_handle, output0, output_tensors[0].device_mem,
                        bmrt_tensor_bytesize(&output_tensors[0]));

  // print output shape and data
  // you can get output shape and data like this
  std::cout << "\n===================\n";
  std::cout << "print output info..." << std::endl;
  std::cout << "output data type: " << output_tensors[0].dtype << std::endl;
  std::cout << "output shape: [";
  for (int i = 0; i < output_tensors[0].shape.num_dims; i++) {
    std::cout << output_tensors[0].shape.dims[i] << " ";
  }
  std::cout << "]" << std::endl;
  std::cout << "output data:<";
  for (int i = 0; i < 10; i++) {
    std::cout << ((float*)output0)[i] << " ";
  }
  std::cout << " ...>" << std::endl;
  std::cout << "===================" << std::endl;

  // at last, free device memory
  free(input0);
  free(output0);
  bm_free_device(bm_handle, input_tensors.device_mem);
  for (int i = 0; i < output_tensors.size(); ++i) {
    bm_free_device(bm_handle, output_tensors[i].device_mem);
  }
  for (int i = 0; i < prealloc_mem_v.size(); ++i) {
    bm_free_device(bm_handle, prealloc_mem_v[i]);
  }

  bmrt_destroy(p_bmrt);
  bm_dev_free(bm_handle);

  return 0;
}

int main(int argc, char** argv) {
  // return demo(argc, argv);
  return demo_memory_decoupling(argc, argv);
}