BMRuntime 示例代码
_________________________

Example with basic C interface
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

本节通过举例来介绍runtime C接口的使用，一个是通用的例子，可以在PCIE或者SoC上使用；另一个使用mmap方式，只能在SoC上使用

Common Example
:::::::::::::::

范例说明：

* 创建bm_handle以及runtime实例
* 加载bmodel，该模型中有一个testnet网络，有2个输入，2个输出
* 准备input tensors，包括每个input的shape和数据
* 启动推理运算
* 推理结束后将在output_tensors中结果数据拷贝到系统内存
* 退出程序前，释放device mem、runtime实例、bm_handle

需要注意，范例中将output的数据通过bm_memcpy_s2d_partial接口拷贝到系统内存，而不是用bm_memcpy_s2d接口，原因在于前者指定了tensor的大小，后者是按照device mem大小整个拷贝。output tensor的实际大小是小于或等于device mem大小的，所以使用bm_memcp_s2d有可能内存溢出。所以推荐用bm_memcpy_x2x_partial接口，不要使用bm_memcpy_x2x接口。

.. code-block:: c++

  #include "bmruntime_interface.h"

  void bmrt_test() {
    // request bm_handle
    bm_handle_t bm_handle;
    bm_status_t status = bm_dev_request(&bm_handle, 0);
    assert(BM_SUCCESS == status);

    // create bmruntime
    void *p_bmrt = bmrt_create(bm_handle);
    assert(NULL != p_bmrt);

    // load bmodel by file
    bool ret = bmrt_load_bmodel(p_bmrt, "testnet.bmodel");
    assert(true == ret);

    auto net_info = bmrt_get_network_info(p_bmrt, "testnet");
    assert(NULL != net_info);

    // init input tensors
    bm_tensor_t input_tensors[2];
    status = bm_malloc_device_byte(bm_handle, &input_tensors[0].device_mem,
                                   net_info->max_input_bytes[0]);
    assert(BM_SUCCESS == status);
    input_tensors[0].dtype = BM_INT8;
    input_tensors[0].st_mode = BM_STORE_1N;
    status = bm_malloc_device_byte(bm_handle, &input_tensors[1].device_mem,
                                   net_info->max_input_bytes[1]);
    assert(BM_SUCCESS == status);
    input_tensors[1].dtype = BM_FLOAT32;
    input_tensors[1].st_mode = BM_STORE_1N;

    // init output tensors
    bm_tensor_t output_tensors[2];
    status = bm_malloc_device_byte(bm_handle, &output_tensors[0].device_mem,
                                   net_info->max_output_bytes[0]);
    assert(BM_SUCCESS == status);
    status = bm_malloc_device_byte(bm_handle, &output_tensors[1].device_mem,
                                   net_info->max_output_bytes[1]);
    assert(BM_SUCCESS == status);

    // before inference, set input shape and prepare input data
    // here input0/input1 is system buffer pointer.
    input_tensors[0].shape = {2, {1,2}};
    input_tensors[1].shape = {4, {4,3,28,28}};
    bm_memcpy_s2d_partial(bm_handle, input_tensors[0].device_mem, (void *)input0,
                          bmrt_tensor_bytesize(&input_tensors[0]));
    bm_memcpy_s2d_partial(bm_handle, input_tensors[1].device_mem, (void *)input1,
                          bmrt_tensor_bytesize(&input_tensors[1]));

    ret = bmrt_launch_tensor_ex(p_bmrt, "testnet", input_tensors, 2,
                                output_tensors, 2, true, false);
    assert(true == ret);

    // sync, wait for finishing inference
    bm_thread_sync(bm_handle);

    /**************************************************************/
    // here all output info stored in output_tensors, such as data type, shape, device_mem.
    // you can copy data to system memory, like this.
    // here output0/output1 are system buffers to store result.
    bm_memcpy_d2s_partial(bm_handle, output0, output_tensors[0].device_mem,
                          bmrt_tensor_bytesize(&output_tensors[0]));
    bm_memcpy_d2s_partial(bm_handle, output1, output_tensors[1].device_mem,
                          bmrt_tensor_bytesize(&output_tensors[1]));
    ......      // do other things
    /**************************************************************/

    // at last, free device memory
    for (int i = 0; i < net_info->input_num; ++i) {
      bm_free_device(bm_handle, input_tensors[i].device_mem);
    }
    for (int i = 0; i < net_info->output_num; ++i) {
      bm_free_device(bm_handle, output_tensors[i].device_mem);
    }

    bmrt_destroy(p_bmrt);
    bm_dev_free(bm_handle);
  }

MMAP Example
:::::::::::::

本例功能和上个例子相同，但没有进行device mem数据的拷入和拷出，而是采用mmap方式映射给应用程序直接访问。效率比上例高，但只能在SoC下使用。

.. code-block:: c++

  #include "bmruntime_interface.h"

  void bmrt_test() {
    // request bm_handle
    bm_handle_t bm_handle;
    bm_status_t status = bm_dev_request(&bm_handle, 0);
    assert(BM_SUCCESS == status);

    // create bmruntime
    void *p_bmrt = bmrt_create(bm_handle);
    assert(NULL != p_bmrt);

    // load bmodel by file
    bool ret = bmrt_load_bmodel(p_bmrt, "testnet.bmodel");
    assert(true == ret);

    auto net_info = bmrt_get_network_info(p_bmrt, "testnet");
    assert(NULL != net_info);

    bm_tensor_t input_tensors[2];
    bmrt_tensor(&input_tensors[0], p_bmrt, BM_INT8, {2, {1,2}});
    bmrt_tensor(&input_tensors[1], p_bmrt, BM_FLOAT32, {4, {4,3,28,28}});

    void *input[2];
    status = bm_mem_mmap_device_mem(bm_handle, &input_tensors[0].device_mem,
                                    (uint64_t*)&input[0]);
    assert(BM_SUCCESS == status);
    status = bm_mem_mmap_device_mem(bm_handle, &input_tensors[1].device_mem,
                                    (uint64_t*)&input[1]);
    assert(BM_SUCCESS == status);

    // write input data to input[0], input[1]
    ......

    // flush it
    status = bm_mem_flush_device_mem(bm_handle, &input_tensors[0].device_mem);
    assert(BM_SUCCESS == status);
    status = bm_mem_flush_device_mem(bm_handle, &input_tensors[1].device_mem);
    assert(BM_SUCCESS == status);

    // prepare output tensor, and launch
    assert(net_info->output_num == 2);

    bm_tensor_t output_tensors[2];
    ret = bmrt_launch_tensor(p_bmrt, "testnet", input_tensors,2,
                             output_tensors, 2);
    assert(true == ret);

    // sync, wait for finishing inference
    bm_thread_sync(bm_handle);

    /**************************************************************/
    // here all output info stored in output_tensors, such as data type, shape, device_mem.
    // you can access system memory, like this.
    void * output[2];
    status = bm_mem_mmap_device_mem(bm_handle, &output_tensors[0].device_mem,
                                    (uint64_t*)&output[0]);
    assert(BM_SUCCESS == status);
    status = bm_mem_mmap_device_mem(bm_handle, &output_tensors[1].device_mem,
                                    (uint64_t*)&output[1]);
    assert(BM_SUCCESS == status);
    status = bm_mem_invalidate_device_mem(bm_handle, &output_tensors[0].device_mem);
    assert(BM_SUCCESS == status);
    status = bm_mem_invalidate_device_mem(bm_handle, &output_tensors[1].device_mem);
    assert(BM_SUCCESS == status);
    // do other things
    // users can access output by output[0] and output[1]
    ......
    /**************************************************************/

    // at last, unmap and free device memory
    for (int i = 0; i < net_info->input_num; ++i) {
      status = bm_mem_unmap_device_mem(bm_handle, input[i],
                                       bm_mem_get_device_size(input_tensors[i].device_mem));
      assert(BM_SUCCESS == status);
      bm_free_device(bm_handle, input_tensors[i].device_mem);
    }
    for (int i = 0; i < net_info->output_num; ++i) {
      status = bm_mem_unmap_device_mem(bm_handle, output[i],
                                       bm_mem_get_device_size(output_tensors[i].device_mem));
      assert(BM_SUCCESS == status);
      bm_free_device(bm_handle, output_tensors[i].device_mem);
    }

    bmrt_destroy(p_bmrt);
    bm_dev_free(bm_handle);
  }

Example with basic C++ interface
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

本节通过举例来介绍runtime C++接口的使用，一个是通用的例子，可以在PCIE或者SoC上使用；另一个使用mmap方式，只能在SoC上使用

Common Example
:::::::::::::::

范例说明：

* 创建bm_handle以及context实例
* 加载bmodel，该模型中有一个testnet网络，有2个输入，2个输出
* 准备input tensors，包括每个input的shape和数据
* 启动推理运算
* 推理结束后将在output_tensors中结果数据拷贝到系统内存
* 退出程序前，释放bm_handle

对Context的testnet网络实例化了2个Network，以此表明这几点：

* 当Network不指定stage的时候，每个input都需要Reshape来设置输入的shape；当Network指定stage的时候，按照stage的shape来配置input，不需要用户再Reshape
* 同一个网络名，是可以被实例化成多个Network，它们之间没有任何影响。同理每个Network可以多线程中推理

.. code-block:: c++

  #include "bmruntime_cpp.h"

  using namespace bmruntime;

  void bmrt_test()
  {
    // create Context
    Context ctx;

    // load bmodel by file
    bm_status_t status = ctx.load_bmodel("testnet.bmodel");
    assert(BM_SUCCESS == status);

    // create Network
    Network net1(ctx, "testnet"); // may use any stage
    Network net2(ctx, "testnet", 0); // use stage[0]

    /**************************************************************/
    // net1 example
    {
      // prepare input tensor, assume testnet has 2 input
      assert(net1.info()->input_num == 2);
      auto &inputs = net1.Inputs();
      inputs[0]->Reshape({2, {1, 2}});
      inputs[1]->Reshape({4, {4, 3, 28, 28}});
      // here input0/input1 is system buffer pointer to input datas
      inputs[0]->CopyFrom((void *)input0);
      inputs[1]->CopyFrom((void *)input1);

      // do inference
      status = net1.Forward();
      assert(BM_SUCCESS == status);

      // here all output info stored in output_tensors, such as data type, shape, device_mem.
      // you can copy data to system memory, like this.
      // here output0/output1 are system buffers to store result.
      auto &outputs = net1.Outputs();
      outputs[0]->CopyTo(output0);
      outputs[1]->CopyTo(output1);
      ......  // do other things
    }

    /**************************************************************/
    // net2 example
    // prepare input tensor, assume testnet has 2 input
    {
      assert(net2.info()->input_num == 2);
      auto &inputs = net2.Inputs();
      inputs[0]->CopyFrom((void *)input0);
      inputs[1]->CopyFrom((void *)input1);
      status = net2.Forward();
      assert(BM_SUCCESS == status);
      // here all output info stored in output_tensors
      auto &outputs = net2.Outputs();
      ......  // do other things
    }
  }

MMAP Example
:::::::::::::

本例只实例化了一个Network，主要是说明mmap，如何使用。

.. code-block:: c++

  #include "bmruntime_cpp.h"

  using namespace bmruntime;

  void bmrt_test()
  {
    // create Context
    Context ctx;

    // load bmodel by file
    bm_status_t status = ctx.load_bmodel("testnet.bmodel");
    assert(BM_SUCCESS == status);

    // create Network

    Network net(ctx, "testnet", 0); // use stage[0]

    // prepare input tensor, assume testnet has 2 input
    assert(net.info()->input_num == 2);
    auto &inputs = net.Inputs();

    void *input[2];
    bm_handle_t bm_handle = ctx.handle();
    status = bm_mem_mmap_device_mem(bm_handle, &(inputs[0]->tensor()->device_mem),
                                    (uint64_t*)&input[0]);
    assert(BM_SUCCESS == status);
    status = bm_mem_mmap_device_mem(bm_handle, &(inputs[1]->tensor()->device_mem),
                                    (uint64_t*)&input[1]);
    assert(BM_SUCCESS == status);

    // write input data to input[0], input[1]
    ......

    // flush it
    status = bm_mem_flush_device_mem(bm_handle, &(inputs[0]->tensor()->device_mem));
    assert(BM_SUCCESS == status);
    status = bm_mem_flush_device_mem(bm_handle, &(inputs[1]->tensor()->device_mem));
    assert(BM_SUCCESS == status);

    status = net.Forward();
    assert(BM_SUCCESS == status);
    // here all output info stored in output_tensors
    auto &outputs = net.Outputs();

    // mmap output
    void * output[2];
    status = bm_mem_mmap_device_mem(bm_handle, &(outputs[0]->tensor()->device_mem),
                                    (uint64_t*)&output[0]);
    assert(BM_SUCCESS == status);
    status = bm_mem_mmap_device_mem(bm_handle, &(outputs[1]->tensor()->device_mem),
                                    (uint64_t*)&output[1]);
    assert(BM_SUCCESS == status);
    // invalidate it
    status = bm_mem_invalidate_device_mem(bm_handle, &(outputs[0]->tensor()->device_mem));
    assert(BM_SUCCESS == status);
    status = bm_mem_invalidate_device_mem(bm_handle, &(outputs[1]->tensor()->device_mem));
    assert(BM_SUCCESS == status);

    // user can access output by output[0] and output[1]
    ......

    // at last, unmap bm_handle
    status = bm_mem_unmap_device_mem(bm_handle, input[0],
                                    bm_mem_get_device_size(inputs[0]->tensor()->device_mem));
    assert(BM_SUCCESS == status);
    status = bm_mem_unmap_device_mem(bm_handle, input[1],
                                    bm_mem_get_device_size(inputs[1]->tensor()->device_mem));
    assert(BM_SUCCESS == status);
    status = bm_mem_unmap_device_mem(bm_handle, output[0],
                                    bm_mem_get_device_size(outputs[0]->tensor()->device_mem));
    assert(BM_SUCCESS == status);
    status = bm_mem_unmap_device_mem(bm_handle, output[1],
                                    bm_mem_get_device_size(outputs[1]->tensor()->device_mem));
    assert(BM_SUCCESS == status);
  }
