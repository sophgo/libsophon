bmcv_min_max
============

对于存储于device memory中连续空间的一组数据，该接口可以获取这组数据中的最大值和最小值。


接口的格式如下：

    .. code-block:: c

        bm_status_t bmcv_min_max(bm_handle_t handle,
                                 bm_device_mem_t input,
                                 float *minVal,
                                 float *maxVal,
                                 int len);


**输入参数说明：**

* bm_handle_t handle

输入参数。bm_handle 句柄

* bm_device_mem_t input

输入参数。输入数据的 device 地址。

* float \*minVal

输出参数。运算后得到的最小值结果, 如果为NULL则不计算最小值。

* float \*maxVal

输出参数。运算后得到的最大值结果，如果为NULL则不计算最大值。

* int len

输入参数。输入数据的长度。



**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败



**示例代码**


    .. code-block:: c

        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        int len = 1000;
        float max = 0;
        float min = 0;
        float *input = new float[len];
        for (int i = 0; i < 1000; i++) {
            input[i] = (float)(rand() % 1000) / 10.0;
        }
        bm_device_mem_t input_mem;
        bm_malloc_device_byte(handle, input_mem, len * sizeof(float))
        bm_memcpy_s2d(handle, input_mem, input);
        bmcv_min_max(handle,
                     input_mem,
                     &min,
                     &max,
                     len,
                     2);
        bm_free_device(handle, input_mem);
        bm_dev_free(handle);
        delete [] input;

