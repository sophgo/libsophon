#include <iostream>
#include <cstdlib>
#include <vector>
#include <assert.h>

#include "bmruntime_interface.h"

/* bmruntime demo with basic C interface */

int main(int argc, char** argv) {
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
