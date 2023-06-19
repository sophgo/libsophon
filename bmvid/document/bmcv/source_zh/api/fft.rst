bmcv_fft
============

FFT运算。完整的使用步骤包括创建、执行、销毁三步。

创建
_____

支持一维或者两维的FFT计算，其区别在于创建过程中，后面的执行和销毁使用相同的接口。

对于一维的FFT，支持多batch的运算，接口形式如下：

    .. code-block:: c

        bm_status_t bmcv_fft_1d_create_plan(
                bm_handle_t handle,
                int batch,
                int len,
                bool forward,
                void *&plan);

**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄

* int batch

  输入参数。batch的数量。

* int int

  输入参数。每个batch的长度。

* bool forward

  输入参数。是否为正向变换，false表示逆向变换。

* void \*\&plan

  输出参数。执行阶段需要使用的句柄。

**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


对于两维M*N的FFT运算，接口形式如下：

    .. code-block:: c

        bm_status_t bmcv_fft_2d_create_plan(
                bm_handle_t handle,
                int M,
                int N,
                bool forward,
                void *&plan);

**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄

* int M

  输入参数。第一个维度的大小。

* int N

  输入参数。第二个维度的大小。

* bool forward

  输入参数。是否为正向变换，false表示逆向变换。

* void \*\&plan

  输出参数。执行阶段需要使用的句柄。

**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


执行
_____

使用上述创建后的plan就可以开始真正的执行阶段了，支持复数输入和实数输入两种接口，其格式分别如下：

    .. code-block:: c

        bm_status_t bmcv_fft_execute(
                bm_handle_t handle,
                bm_device_mem_t inputReal,
                bm_device_mem_t inputImag,
                bm_device_mem_t outputReal,
                bm_device_mem_t outputImag,
                const void *plan);

        bm_status_t bmcv_fft_execute_real_input(
                bm_handle_t handle,
                bm_device_mem_t inputReal,
                bm_device_mem_t outputReal,
                bm_device_mem_t outputImag,
                const void *plan);


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄

* bm_device_mem_t inputReal

  输入参数。存放输入数据实数部分的device memory空间，对于一维的FFT，其大小为batch*len*sizeof(float)，对于两维FFT，其大小为M*N*sizeof(float)。

* bm_device_mem_t inputImag

  输入参数。存放输入数据虚数部分的device memory空间，对于一维的FFT，其大小为batch*len*sizeof(float)，对于两维FFT，其大小为M*N*sizeof(float)。

* bm_device_mem_t outputReal

  输出参数。存放输出结果实数部分的device memory空间，对于一维的FFT，其大小为batch*len*sizeof(float)，对于两维FFT，其大小为M*N*sizeof(float)。

* bm_device_mem_t outputImag

  输出参数。存放输出结果虚数部分的device memory空间，对于一维的FFT，其大小为batch*len*sizeof(float)，对于两维FFT，其大小为M*N*sizeof(float)。

* const void \*plan

  输入参数。创建阶段所得到的句柄。

**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


销毁
______

当执行完成后需要销毁所创建的句柄。

    .. code-block:: c

        void bmcv_fft_destroy_plan(bm_handle_t handle, void *plan);




示例代码
___________

    .. code-block:: c

        bool realInput = false;
        float *XRHost = new float[M * N];
        float *XIHost = new float[M * N];
        float *YRHost = new float[M * N];
        float *YIHost = new float[M * N];
        for (int i = 0; i < M * N; ++i) {
            XRHost[i] = rand() % 5 - 2;
            XIHost[i] = realInput ? 0 : rand() % 5 - 2;
        }
        bm_handle_t handle = nullptr;
        bm_dev_request(&handle, 0);
        bm_device_mem_t XRDev, XIDev, YRDev, YIDev;
        bm_malloc_device_byte(handle, &XRDev, M * N * 4);
        bm_malloc_device_byte(handle, &XIDev, M * N * 4);
        bm_malloc_device_byte(handle, &YRDev, M * N * 4);
        bm_malloc_device_byte(handle, &YIDev, M * N * 4);
        bm_memcpy_s2d(handle, XRDev, XRHost);
        bm_memcpy_s2d(handle, XIDev, XIHost);
        void *plan = nullptr;
        bmcv_fft_2d_create_plan(handle, M, N, forward, plan);
        if (realInput)
            bmcv_fft_execute_real_input(handle, XRDev, YRDev, YIDev, plan);
        else
            bmcv_fft_execute(handle, XRDev, XIDev, YRDev, YIDev, plan);
        bmcv_fft_destroy_plan(handle, plan);
        bm_memcpy_d2s(handle, YRHost, YRDev);
        bm_memcpy_d2s(handle, YIHost, YIDev);
        bm_free_device(handle, XRDev);
        bm_free_device(handle, XIDev);
        bm_free_device(handle, YRDev);
        bm_free_device(handle, YIDev);
        bm_dev_free(handle);

