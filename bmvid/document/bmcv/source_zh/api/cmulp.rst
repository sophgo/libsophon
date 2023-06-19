bmcv_cmulp
==========

该接口实现复数乘法运算，运算公式如下:

  .. math::
    \text{outputReal} + \text{outputImag} \times i = (\text{inputReal} + \text{inputImag} \times i) \times (\text{pointReal} + \text{pointImag} \times i)
  .. math::
    \text{outputReal} = \text{inputReal} \times \text{pointReal} - \text{inputImag} \times \text{pointImag}
  .. math::
    \text{outputImag} = \text{inputReal} \times \text{pointImag} + \text{inputImag} \times \text{pointReal}

  其中，:math:`i` 是虚数单位，满足公式 :math:`i^2 = -1`.

**接口形式：**

    .. code-block:: c++

        bm_status_t bmcv_cmulp(
                bm_handle_t     handle,
                bm_device_mem_t inputReal,
                bm_device_mem_t inputImag,
                bm_device_mem_t pointReal,
                bm_device_mem_t pointImag,
                bm_device_mem_t outputReal,
                bm_device_mem_t outputImag,
                int             batch,
                int             len);


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bm_device_mem_t inputReal

  输入参数。存放输入实部的 device 地址。

* bm_device_mem_t inputImag

  输入参数。存放输入虚部的 device 地址。

* bm_device_mem_t pointReal

  输入参数。存放另一个输入实部的 device 地址。

* bm_device_mem_t pointImag

  输入参数。存放另一个输入虚部的 device 地址。

* bm_device_mem_t outputReal

  输出参数。存放输出实部的 device 地址。

* bm_device_mem_t outputImag

  输出参数。存放输出虚部的 device 地址。

* int batch

  输入参数。batch 的数量。

* int len

  输入参数。一个 batch 中复数的数量。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项：**

1. 数据类型仅支持 float。



**示例代码**

    .. code-block:: c++

        int L = 5;
        int batch = 2;
        float *XRHost = new float[L * batch];
        float *XIHost = new float[L * batch];
        float *PRHost = new float[L];
        float *PIHost = new float[L];
        for (int i = 0; i < L * batch; ++i) {
            XRHost[i] = rand() % 5 - 2;
            XIHost[i] = rand() % 5 - 2;
        }
        for (int i = 0; i < L; ++i) {
            PRHost[i] = rand() % 5 - 2;
            PIHost[i] = rand() % 5 - 2;
        }
        float *YRHost = new float[L * batch];
        float *YIHost = new float[L * batch];
        bm_handle_t handle = nullptr;
        bm_dev_request(&handle, 0);
        bm_device_mem_t XRDev, XIDev, PRDev, PIDev, YRDev, YIDev;
        bm_malloc_device_byte(handle, &XRDev, L * batch * 4);
        bm_malloc_device_byte(handle, &XIDev, L * batch * 4);
        bm_malloc_device_byte(handle, &PRDev, L * 4);
        bm_malloc_device_byte(handle, &PIDev, L * 4);
        bm_malloc_device_byte(handle, &YRDev, L * batch * 4);
        bm_malloc_device_byte(handle, &YIDev, L * batch * 4);
        bm_memcpy_s2d(handle, XRDev, XRHost);
        bm_memcpy_s2d(handle, XIDev, XIHost);
        bm_memcpy_s2d(handle, PRDev, PRHost);
        bm_memcpy_s2d(handle, PIDev, PIHost);

        bmcv_cmulp(handle,
                   XRDev,
                   XIDev,
                   PRDev,
                   PIDev,
                   YRDev,
                   YIDev,
                   batch,
                   L);
        bm_memcpy_d2s(handle, YRHost, YRDev);
        bm_memcpy_d2s(handle, YIHost, YIDev);

        delete[] XRHost;
        delete[] XIHost;
        delete[] PRHost;
        delete[] PIHost;
        delete[] YRHost;
        delete[] YIHost;
        bm_free_device(handle, XRDev);
        bm_free_device(handle, XIDev);
        bm_free_device(handle, YRDev);
        bm_free_device(handle, YIDev);
        bm_free_device(handle, PRDev);
        bm_free_device(handle, PIDev);
        bm_dev_free(handle);