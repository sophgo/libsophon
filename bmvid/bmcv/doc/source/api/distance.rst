bmcv_distance
=============

计算多维空间下多个点与特定一个点的欧式距离，前者坐标存放在连续的device memory中，而特定一个点的坐标通过参数传入。坐标值为float类型。


接口的格式如下：

    .. code-block:: c

        bm_status_t bmcv_distance(
                bm_handle_t handle,
                bm_device_mem_t input,
                bm_device_mem_t output,
                int dim,
                const float *pnt,
                int len);


**输入参数说明：**

* bm_handle_t handle

输入参数。bm_handle 句柄

* bm_device_mem_t input

输入参数。存放len个点坐标的 device 空间。其大小为len*dim*sizeof(float)。

* bm_device_mem_t output

输出参数。存放len个距离的 device 空间。其大小为len*sizeof(float)。

* int dim

输入参数。空间维度大小。

* const float \*pnt

输入参数。特定一个点的坐标，长度为dim。

* int len

输入参数。待求坐标的数量。



**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败



**示例代码**


    .. code-block:: c

        int L = 1024 * 1024;
        int dim = 3;
        float pnt[8] = {0};
        for (int i = 0; i < dim; ++i)
            pnt[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
        float *XHost = new float[L * dim];
        for (int i = 0; i < L * dim; ++i)
            XHost[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
        float *YHost = new float[L];
        bm_handle_t handle = nullptr;
        bm_dev_request(&handle, 0);
        bm_device_mem_t XDev, YDev;
        bm_malloc_device_byte(handle, &XDev, L * dim * 4);
        bm_malloc_device_byte(handle, &YDev, L * 4);
        bm_memcpy_s2d(handle, XDev, XHost);
        bmcv_distance(handle,
                      XDev,
                      YDev,
                      dim,
                      pnt,
                      L));
        bm_memcpy_d2s(handle, YHost, YDev));
        delete [] XHost;
        delete [] YHost;
        bm_free_device(handle, XDev);
        bm_free_device(handle, YDev);
        bm_dev_free(handle);

