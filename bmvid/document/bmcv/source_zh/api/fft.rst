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


**处理器型号支持：**

该接口支持BM1684和BM1684X。


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄

* int batch

  输入参数。batch的数量。

* int len

  输入参数。每个batch的长度。

* bool forward

  输入参数。是否为正向变换，false表示逆向变换。

* void \*\&plan

  输出参数。执行阶段需要使用的句柄。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


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

* 其他: 失败


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

* 其他: 失败


销毁
______

当执行完成后需要销毁所创建的句柄。

    .. code-block:: c

        void bmcv_fft_destroy_plan(bm_handle_t handle, void *plan);


示例代码
___________

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdint.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>

        int main()
        {
            bm_handle_t handle;
            int ret = 0;
            int i;
            int L = 100;
            int batch = 100;
            bool forward = true;
            bool realInput = false;
            bm_device_mem_t XRDev, XIDev, YRDev, YIDev;
            void* plan = NULL;

            ret = (int)bm_dev_request(&handle, 0);
            if (ret) {
                printf("Create bm handle failed. ret = %d\n", ret);
                return ret;
            }

            float* XRHost = (float*)malloc(L * batch * sizeof(float));
            float* XIHost = (float*)malloc(L * batch * sizeof(float));
            float* YRHost_tpu = (float*)malloc(L * batch * sizeof(float));
            float* YIHost_tpu = (float*)malloc(L * batch * sizeof(float));

            for (i = 0; i < L * batch; ++i) {
                XRHost[i] = (float)rand() / RAND_MAX;
                XIHost[i] = realInput ? 0 : ((float)rand() / RAND_MAX);
            }

            ret = bm_malloc_device_byte(handle, &XRDev, L * batch * sizeof(float));
            ret = bm_malloc_device_byte(handle, &XIDev, L * batch * sizeof(float));
            ret = bm_malloc_device_byte(handle, &YRDev, L * batch * sizeof(float));
            ret = bm_malloc_device_byte(handle, &YIDev, L * batch * sizeof(float));

            ret = bm_memcpy_s2d(handle, XRDev, XRHost);
            ret = bm_memcpy_s2d(handle, XIDev, XIHost);

            ret = bmcv_fft_2d_create_plan(handle, L, batch, forward, plan);
            if (realInput) {
                bmcv_fft_execute_real_input(handle, XRDev, YRDev, YIDev, plan);
            } else {
                bmcv_fft_execute(handle, XRDev, XIDev, YRDev, YIDev, plan);
            }

            ret = bm_memcpy_d2s(handle, (void*)YRHost_tpu, YRDev);
            ret = bm_memcpy_d2s(handle, (void*)YIHost_tpu, YIDev);

            if (plan != NULL) {
                bmcv_fft_destroy_plan(handle, plan);
            }
            free(XRHost);
            free(XIHost);
            free(YRHost_tpu);
            free(YIHost_tpu);
            bm_free_device(handle, XRDev);
            bm_free_device(handle, XIDev);
            bm_free_device(handle, YRDev);
            bm_free_device(handle, YIDev);
            bm_dev_free(handle);
            return ret;
        }