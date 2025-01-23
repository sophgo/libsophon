# TPU-RUNTIME

## 工程说明

tpu-runtime依赖于`libsophon`，最终编译会产生`libbmrt.so`和`bmrt_test`两个文件，并随着libsophon生成的deb包发布出去。
其中
1. `libbmrt.so`是加载`bmodel`并执行的核心库，用于模型推理程序开发。
2. `bmrt_test`用于测试`bmodel`的应用程序

基本的软件关系如下
```
            ┌─────────────┐ ┌──────────────┐
            │   app       │ │  bmrt_test   │
            └─────────┬───┘ └───┬──────────┘
                      │         │
                   ┌──▼─────────▼──┐
kernel_module ---->│  libbmrt.so   │
                   └───────┬───────┘
                           │
                   ┌───────▼───────┐
                   │  libbmlib.so  │
                   └───────┬───────┘
                           │
                   ┌───────▼───────┐
                   │    driver     │
                   └───────┬───────┘
                           │
───────────────────────────┼───────────────────────────────
                           │
                   ┌────────▼──────┐
                   │    TPU        │
                   │    Devices    │
                   └───────────────┘
```

## SOC平台编译(在A53上运行)：
```shell
#libsophon根目录创建build文件夹
mkdir build
#进入build文件夹
cd build
# 默认是Release版本， 通过EXTRA_CONFIG可以配置编译Debug版本
# export EXTRA_CONFIG="-DCMAKE_BUILD_TYPE=Debug"
cmake -DPLATFORM=soc -DCROSS_COMPILE_PATH=/path/to/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu -DCMAKE_TOOLCHAIN_FILE=/path/to/libsophon/toolchain-aarch64-linux.cmake -DLIB_DIR=/path/to/libsophon/3rdparty/arm64/soc -DBUILD_STATIC_LIB=ON -DCMAKE_INSTALL_PREFIX=$PWD/../install ..
#执行make编译
make
# 然后将编译好的文件,复制到目标机器工作目录中, 这里以'soc_device:work_dir'为例
# 包括bmrt_test、libbmrt.so、libbmlib.so.0文件
scp tpu-runtime/*bmrt* soc_device:work_dir
scp bmlib/*bmlib* soc_device:work_dir

```

## 设备上测试
在目标机器上, 可以不用更换libsophon, 采用如下方式
```
# 利用`ssh target`命令登陆目标机器

# 指定寻找libbmrt.so的路径
export LD_LIBRARY_PATH=work_dir

# 可以测试了
cd work_dir
./bmrt_test --bmodel test.bmodel
```

附言：BMRuntime 基础接口说明
================

BMRuntime用于读取BMCompiler的编译输出(.bmodel)，驱动其在深度学习处理器中执行。BMRuntime向用户提供了丰富的接口，便于用户移植算法。

BMRuntime有C和C++两种接口；另外为了兼容上一代应用程序，保留一些接口，但不推荐新的应用程序继续使用。

本章节中的接口默认都是同步接口，有个别是异步接口(由深度学习处理器执行功能，主机处理器可以继续往下执行)，会特别说明。

本章节分2个部分:

* C接口：BMRuntime的C语言接口
* C++接口：BMRuntime的C++语言接口


## C Interface
_____________________

BMRuntime的C语言接口，对应的头文件为bmruntime_interface.h，对应的lib库为 libbmrt.so。

用户程序使用C接口时建议使用该接口，该接口支持多种shape的编译网络。

### bmrt_create

```
    /*
    Parameters: [in] bm_handle - BM handle. It must be declared and initialized by using bmlib.
    Returns:    void*          - The pointer of a bmruntime helper.
    */
    void* bmrt_create(bm_handle_t bm_handle);
```

创建bmruntime，返回runtime指针。其他接口(bmrt_xxxx类接口)，需要的句柄都是该runtime指针。


### bmrt_destroy
>>>>>>>>>>>>>>>>>>>>

```
    // Parameters: [in] p_bmrt - Bmruntime helper that had been created.
    void bmrt_destroy(void* p_bmrt);
```

销毁bmruntime，释放资源。

用户通常开始创建runtime，退出前销毁runtime，举例如下：

```
    // start program
    bm_handle_t bm_handle;
    bm_dev_request(&bm_handle, 0);
    void * p_bmrt = bmrt_create(bm_handle);
    // do things here
    ......
    // end of program
    bmrt_destroy(p_bmrt);
    bm_dev_free(bm_handle);
```

### bmrt_get_bm_handle
>>>>>>>>>>>>>>>>>>>

```
    /*
    Parameters: [in]  p_bmrt   - Bmruntime that had been created
    Returns:    void*          - The pointer of bm_handle_t
    */
    void * bmrt_get_bm_handle(void* p_bmrt);
```

从runtime指针中得到设备句柄bm_handle，在bm_xxxx一类接口中需要用到。

### bmrt_load_bmodel
>>>>>>>>>>>>>>>>>

```
    /*
    Parameters: [in] p_bmrt      - Bmruntime that had been created.
                [in] bmodel_path - Bmodel file directory.
    Returns:    bool             - true: success; false: failed.
    */
    bool bmrt_load_bmodel(void* p_bmrt, const char *bmodel_path);
```

加载bmodel文件，加载后bmruntime中就会存在若干网络的数据，后续可以对网络进行推理。


### bmrt_get_network_info
>>>>>>>>>>>>>>>>>>>>>>>>>>>>

网络的信息表示如下：

```
    /* bm_stage_info_t holds input shapes and output shapes;
    every network can contain one or more stages */
    typedef struct bm_stage_info_s {
      bm_shape_t* input_shapes;   /* input_shapes[0] / [1] / ... / [input_num-1] */
      bm_shape_t* output_shapes;  /* output_shapes[0] / [1] / ... / [output_num-1] */
      bm_device_mem_t *input_mems; /* input_mems[0] / [1] / ... / [input_num-1] */
      bm_device_mem_t *output_mems; /* output_mems[0] / [1] / ... / [output_num-1] */
    } bm_stage_info_t;

    /* bm_tensor_info_t holds all information of one net */
    typedef struct bm_net_info_s {
      const char* name;              /* net name */
      bool is_dynamic;               /* dynamic or static */
      int input_num;                 /* number of inputs */
      char const** input_names;      /* input_names[0] / [1] / .../ [input_num-1] */
      bm_data_type_t* input_dtypes;  /* input_dtypes[0] / [1] / .../ [input_num-1] */
      float* input_scales;           /* input_scales[0] / [1] / .../ [input_num-1] */
      int output_num;                /* number of outputs */
      char const** output_names;     /* output_names[0] / [1] / .../ [output_num-1] */
      bm_data_type_t* output_dtypes; /* output_dtypes[0] / [1] / .../ [output_num-1] */
      float* output_scales;          /* output_scales[0] / [1] / .../ [output_num-1] */
      int stage_num;                 /* number of stages */
      bm_stage_info_t* stages;       /* stages[0] / [1] / ... / [stage_num-1] */
      size_t * max_input_bytes;      /* max_input_bytes[0]/ [1] / ... / [input_num-1] */
      size_t * max_output_bytes;     /* max_output_bytes[0] / [1] / ... / [output_num-1] */
      int* input_zero_point;         /* input_zero_point[0] / [1] / .../ [input_num-1] */
      int* output_zero_point;        /* output_zero_point[0] / [1] / .../ [output_num-1] */
      int *input_loc_devices;        /* input_loc_device[0] / [1] / .../ [input_num-1] */
      int *output_loc_devices;       /* output_loc_device[0] / [1] / .../ [output_num-1] */
      int core_num;                  /* core number */
      int32_t addr_mode;             /* address assign mode */
    } bm_net_info_t;
```

bm_net_info_t表示一个网络的全部信息，bm_stage_info_t表示该网络支持的不同的shape情况。

input_num表示输入的数量，input_names/input_dtypes/input_scales以及bm_stage_info_t中的input_shapes都是这个数量。

output_num表示输出的数量，output_names/output_dtypes/output_scales以及bm_stage_info_t中的output_shapes都是这个数量。

input_scales和output_scales只有整型时有用；浮点型时为默认值1.0。

max_input_bytes表示每个input最大的字节数，max_output_bytes表示每个output最大的字节数。
每个网络可能有多个stage，用户可能需要申请每个input/output的最大字节数，存放各种stage的数据。

input_zero_point和output_zero_point记录在非对称量化int8网络的情况下输入和输出的zero_point值。

input_loc_devices和output_loc_devices记录在分布式网络的情况下输入和输出设备号。

core_num记录网络所需的core数量。

addr_mode记录网络的地址分配模式，0表示基础模式，1表示io_alone模式，2 表示 io_tag 模式，3 表示 io_tag_fuse 模式。

bmrt_get_network_info根据网络名，得到某个网络的信息，接口声明如下：

```
    /*
    Parameters: [in] p_bmrt   - Bmruntime that had been created.
                [in] net_name - Network name.
    Returns:    bm_net_info_t - The pointer of bm_net_info_t. If net not found, will return NULL.
    */
    const bm_net_info_t* bmrt_get_network_info(void* p_bmrt, const char* net_name);
```
  
### bmrt_launch_tensor
>>>>>>>>>>>>>>>>>>>>>>

对指定的网络，进行推理。接口声明如下：

```
    /*
    To launch the inference of the neuron network with setting input tensors.
    This API supports the neuron nework that is static-compiled or dynamic-compiled.
    After calling this API, inference on deep-learning processor is launched. The host processor program will not be blocked
    if the neuron network is static-compiled and has no cpu layer. Otherwize, the host processor
    program will be blocked. This API support multiple inputs, and multi thread safety.

    Parameters: [in] p_bmrt - Bmruntime that had been created.
                [in] net_name - The name of the neuron network.
                [in] input_tensors - Array of input tensor.
                                    Defined like bm_tensor_t input_tensors[input_num].
                                    User should initialize each input tensor.
                [in] input_num - Input number.
                [out] output_tensors - Array of output tensor.
                                      Defined like bm_tensor_t output_tensors[output_num].
                                      Data in output_tensors device memory use BM_STORE_1N.
                [in] output_num - Output number.
    Returns:     bool - true: Launch success. false: Launch failed.

    Note:
    This interface will alloc devcie mem for output_tensors. User should free each device mem by
    bm_free_device after the result data is useless.
    */
    bool bmrt_launch_tensor(void* p_bmrt, const char * net_name,
                            const bm_tensor_t input_tensors[], int input_num,
                            bm_tensor_t output_tensors[], int output_num);
```

用户在推理前需要初始化网络需要的input_tensors，包括input_tensors中的数据。output_tensors用于返回推理的结果。

**需要注意:**

* 该接口会为output_tensors申请device mem，用于存储结果数据。当用户不再需要结果数据的时候，需要主动释放device mem。
* 推理结束后，输出数据是以BM_STORE_1N存储；输出的shape存储在每个output_tensor的shape中。
* 该接口为异步接口，用户需要调用bm_thread_sync确保推理完成。

使用方法举例如下：

```
    bm_status_t status = BM_SUCCESS;
    bm_tensor_t input_tensors[1];
    bm_tensor_t output_tensors[2];
    bmrt_tensor(&input_tensors[0], p_bmrt, BM_FLOAT32, {4, {1, 3, 28, 28}});
    bm_memcpy_s2d_partial(bm_handle, input_tensors[0].device_mem, (void *)input0,
                          bmrt_tensor_bytesize(&input_tensors[0]));
    bool ret = bmrt_launch_tensor(p_bmrt, "PNet", input_tensors, 1, output_tensors, 2);
    assert(true == ret);
    status = bm_thread_sync(bm_handle);
    assert(status == BM_SUCCESS);
    bm_memcpy_d2s_partial(bm_handle, output0, output_tensors[0].device_mem,
                            bmrt_tensor_bytesize(&output_tensors[0]));
    bm_memcpy_d2s_partial(bm_handle, output1, output_tensors[1].device_mem,
                            bmrt_tensor_bytesize(&output_tensors[1]));
    bm_free_device(bm_handle, output_tensors[0].device_mem);
    bm_free_device(bm_handle, output_tensors[1].device_mem);
    bm_free_device(bm_handle, intput_tensors[0].device_mem);
```

### bmrt_launch_data
>>>>>>>>>>>>>>>>>

对指定的网络，进行npu推理。接口声明如下：

```
    /*
    To launch the inference of the neuron network with setting input datas in system memory.
    This API supports the neuron nework that is static-compiled or dynamic-compiled.
    After calling this API, inference on deep-learning processor is launched. And the host program will be blocked.
    This API support multiple inputs, and multi thread safety.

    Parameters: [in] p_bmrt       - Bmruntime that had been created.
                [in] net_name     - The name of the neuron network.
                [in] input_datas  - Array of input data.
                                    Defined like void * input_datas[input_num].
                                    User should initialize each data pointer as input.
                [in] input_shapes - Array of input shape.
                                    Defined like bm_shape_t input_shapes[input_num].
                                    User should set each input shape.
                [in] input_num    - Input number.
                [out]output_datas - Array of output data.
                                    Defined like void * output_datas[output_num].
                                    If user don't alloc each output data, set user_mem to false,
                                    and this api will alloc output mem, user should free each
                                    output mem when output data not used. Also user can alloc
                                    system memory for each output data by self and set user_mem
                                    true. Data in memory use BM_STORE_1N.
                [out]output_shapes- Array of output shape.
                                    Defined like bm_shape_t output_shapes[output_num].
                                    It will store each output shape.
                [in] output_num   - Output number.
                [in] user_mem     - true: output_datas[i] have been allocated memory.
                                    false: output_datas[i] have not been allocated memory.
    Returns:    bool - true: Launch success; false: Launch failed.
    */
    bool bmrt_launch_data(void* p_bmrt, const char* net_name, void* const input_datas[],
                          const bm_shape_t input_shapes[], int input_num, void * output_datas[],
                          bm_shape_t output_shapes[], int output_num, bool user_mem);
```

与bmrt_launch_tensor不同的地方在于:

* 输入和输出都存储在系统内存。
* 为同步接口。接口返回的时候推理已经完成。


### get_bmodel_api_info_c
>>>>>>>>>>>>>>>>>>>>

```
    /*
      * This API only supports the neuron nework that is static-compiled.
      * After calling this API, api info will be setted and return,
      * and then you can call `bm_send_api` to start deep-learning processor inference.
      * When you no longer need the memory, call bmrt_free_api_info to avoid memory leaks.
      *
      * @param [in]    p_bmrt            Bmruntime that had been created
      * @param [in]    net_name          The name of the neuron network
      * @param [in]    input_tensors     Array of input tensor, defined like bm_tensor_t input_tensors[input_num],
      *                                  User should initialize each input tensor.
      * @param [in]    input_num         Input number
      * @param [in]    output_tensors    Array of output tensor, defined like bm_tensor_t output_tensors[output_num].
      *                                  User can set device_mem or stmode of output tensors. If user_mem is true, this interface
      *                                  will use device mem of output_tensors to store output data, and not alloc device mem;
      *                                  Or it will alloc device mem to store output. If user_stmode is true, it will use stmode in
      *                                  each output tensor; Or stmode will be BM_STORE_1N as default.
      * @param [in]    output_num        Output number
      * @param [in]    user_mem          whether device_mem of output tensors are set
      * @param [in]    user_stmode       whether stmode of output tensors are set
      */
    api_info_c *get_bmodel_api_info_c(void *p_bmrt, const char *net_name,
                                      const bm_tensor_t *input_tensors, int input_num,
                                      bm_tensor_t *output_tensors, int output_num,
                                      bool user_mem, bool user_stmode);
```

* 该函数使用方法类似 bmrt_launch_tensor_ex，但是它只是返回 bmodel 推理前需要下发给深度学习处理器的推理信息，并不会启动推理。该函数返回的信息可以通过 bm_send_api 发送给深度学习处理器启动推理，因此 get_bmodel_api_info + bm_send_api 和 bmrt_launc_tensor_ex 作用是等价的。
* **在该 api_info 使用结束后需要调用 bmrt_free_api_info 来释放内存。**

### bmrt_free_api_info
>>>>>>>>>>>>>>>>>>>

```
    /**
    * @name    bmrt_free_api_info
    * @brief   To release memory allocated by the get_bmodel_api_info_c function's return value.
    * @ingroup bmruntime
    *
    * This function is used to release the memory block returned by the get_bmodel_api_info_c function.
    * After calling get_bmodel_api_info_c to retrieve model API information, make sure to call this function
    * when you no longer need the information to avoid memory leaks.
    *
    * @param [in]    api_info            return value of get_bmodel_api_info_c
    *
    */  
    void bmrt_free_api_info(api_info_c *api_info);
```

* 释放 api_info 所申请的内存空间。

## C++ Interface
_____________________

BMRuntime的C++语言接口，对应的头文件为bmruntime_cpp.h，对应的lib库为libbmrt.so。
用户程序使用C++接口时建议使用该接口，该接口支持多种shape的静态编译网络。

C++接口命名空间为bmruntime，由2个类组成：

* class Context : 用于网络管理，包括加载网络模型，获取网络信息
* class Network : 用于对class Context中某个具体网络进行推理

声明如下：

```
    namespace bmruntime {
        class Context;
        class Network;
        class Tensor;
        ......
    }
```

### class Context
>>>>>>>>>>>>>>>>>>>>>>>

Context用于网络管理，比如加载模型，可以加载1个到多个模型；获取网络信息，可以得到已经加载了的所有网络的名称，以及通过网络名获得某个具体网络的信息。

#### 构造函数与析构函数
:::::::::::::::::::

```
    explicit Context(int devid = 0);
    explicit Context(bm_handle_t bm_handle);
    virtual ~Context();
```

Context的构造函数和析构函数。

用户调用c++接口时，首先需要创建一个Context实例，可以指定devid创建实例，默认使用设备号0。

使用参考如下：

```
    int main() {
      // start program
      Context ctx;
      // do things here
      ......
      // end of program
    }
```

也可以传入bm_handle创建实例，其中bm_handle由bm_dev_request生成。这种方式下需要注意，在退出程序的时候先析构Context，再调用bm_dev_free释放bm_handle。

使用参考如下：

```
    int main() {
      // start program
      bm_handle_t bm_handle;
      bm_dev_request(&bm_handle, 0);
      Context * p_ctx = new Context(bm_handle);
      // do things here
      ......
      // end of program, destroy context first,then free bm_handle
      delete p_ctx;
      bm_dev_free(bm_handle);
    }
```

#### load_bmodel
::::::::::::

```
    bm_status_t load_bmodel(const void *bmodel_data, size_t size);
    bm_status_t load_bmodel(const char *bmodel_file);
```

加载bmodel。

bmodel可以用内存形式，也可以是文件形式。可以被多线程调用。加载成功，返回BM_SUCCESS; 否则返回其他错误码。

可以连续加载多个模型，但是多个模型之间不能有重复的网络名，不然会加载失败。

使用参考如下：

```
    bm_status_t status;
    status = p_ctx->load_bmodel(p_net1, net1_size); // p_net1指向bmodel的内存buffer
    assert(status == BM_SUCCESS);
    status = p_ctx->load_bmodel("net2.bmodel"); // 指定加载bmodel的文件路径
    assert(status == BM_SUCCESS);
```

#### handle
::::::::::::::::::

```
    bm_handle_t handle() const;
```

得到context的设备句柄，与构造函数传入的bm_handle是同一个，在调用bm_xxxx类接口时需要用到。

### class Network

Network类用于对某个具体网络进行推理，该网络是从Context类已加载的网络中选取。该类会自动为该网络申请输入输出的device memory。如果用户需要自己的device memory，也可以在输入输出的tensor中设置。

#### 构造函数与析构函数
:::::::::::::::::::

```
    Network(const Context &ctx, const char *net_name, int stage_id = -1);
    virtual ~Network();
```

Network的构造函数与析构函数。

ctx为前文所述的Context实例，net_name是ctx中已经加装的网络的名称，通过net_name创建一个Network实例。

stage_id是指使用该网络的stage的次序号，如果为-1，则表明用户打算自己Reshape各个输入tensor的shape；
如果是具体stage的次序号，则Network的input tensors固定为这个stage的shape，后续不能被Reshape。

使用参考如下：

```
    //net1, input tensors的shape后续可以被Reshape
    Network net1(*p_ctx, "net1");
    //net2, 采用bm_net_info_t中stage[1]的shape，后续不会被Reshape
    Network net2(*p_ctx, "net2", 1);
```

#### Inputs
::::::::

```
    const std::vector<Tensor *> &Inputs();
```

得到所有input tensors。

用户在对该网络进行推理前，需要先得到input tensors，然后对所有的input tensor进行设定，比如设置它的shape，以及它的data，也可以指定它的device mem。

使用参考如下：

```
    // 对net1的inputs初始化，假定它有2个input
    auto &inputs = net1.Inputs();
    inputs[0]->Reshape(shape0);
    inputs[1]->Reshape(shape1);
    // device_mem0和device_mem1已经存有需要的输入数据
    inputs[0]->set_device_mem(device_mem0);
    inputs[1]->set_device_mem(device_mem1);

    // 对net2的inputs初始化，假定它有1个input
    auto &inputs = net2.Inputs();
    // inputs[0]->Reshape(shape0); // error，不能修改
    // 假定需要的输入数据在系统内存，data0为数据指针
    inputs[0]->CopyFrom(data0);
```

#### Input
::::::::

```
    Tensor *Input(const char *tensor_name);
```

通过input name得到input tensor。

#### Forward
::::::::

```
    bm_status_t Forward(bool sync = true) const;
```

网络推理。

当inputs的数据都准备好后，就可以调用Forward进行推理。

sync为true时，该接口会等待推理结束；sync为false时，该接口为异步接口，接口退出的时候，推理正在进行中，不一定结束，需要调用bm_thread_sync接口确保它推理结束。

**特别注意**: 整个推理过程都在device memory上进行的，所以推理前输入数据必须已经存储在input tensors的device mem里面，推理结束后的结果数据也是保存在output tensors的device mem里面。

使用参考如下：

```
    // net1进行推理
    net1.Forward();
    // net2进行推理
    net2.Forward(false);
    bm_thread_sync(p_ctx->hand());
```

#### Outputs
::::::::

```
    const std::vector<Tensor *> &Outputs();
```

得到output tensors。

在Forward推理前，用户可以改变output tensors的device_mem，以使推理结果保存在用户指定的device mem中；也可以改变output tensors的store mode，以使推理结果以指定的store mode存放。

在Forward推理结束后，output tensors里面的shape和device_mem中的数据才是有效的。

####  Output
::::::::

```
    Tensor *Output(const char *tensor_name);
```

通过output name得到output tensor。

#### info
::::::

```
    const bm_net_info_t *info() const;
```

得到该网络的信息。
