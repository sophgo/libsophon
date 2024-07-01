bmcv_hist_balance
===================

对图像进行直方图均衡化操作，提高图像的对比度。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_hist_balance(
                    bm_handle_t handle,
                    bm_device_mem_t input,
                    bm_device_mem_t output,
                    int H,
                    int W);


**参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄

* bm_device_mem_t input

  输入参数。存放输入图像的 device 空间。其大小为 H * W * sizeof(uint8_t)。

* bm_device_mem_t output

  输出参数。存放输出图像的 device 空间。其大小为 H * W * sizeof(uint8_t)。

* int H

  输入参数。图像的高。

* int W

  输入参数。图像的宽。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1. 数据类型仅支持 uint8_t。

2. 支持的最小图像尺寸为 H = 1, W = 1。

3. 支持的最大图像尺寸为 H = 8192, W = 8192。


**示例代码**

    .. code-block:: c

        int H = 1024;
        int W = 1024;
        uint8_t* input_addr = (uint8_t*)malloc(H * W * sizeof(uint8_t));
        uint8_t* output_addr = (uint8_t*)malloc(H * W * sizeof(uint8_t));
        bm_handle_t handle;
        bm_status_t ret = BM_SUCCESS;
        bm_device_mem_t input, output;
        int i;

        struct timespec tp;
        clock_gettime(NULL, &tp);
        srand(tp.tv_nsec);

        for (i = 0; i < W * H; ++i) {
            input_addr[i] = (uint8_t)rand() % 256;
        }

        ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS) {
            printf("bm_dev_request failed. ret = %d\n", ret);
            exit(-1);
        }

        ret = bm_malloc_device_byte(handle, &input, H * W * sizeof(uint8_t));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed. ret = %d\n", ret);
            exit(-1);
        }

        ret = bm_malloc_device_byte(handle, &output, H * W * sizeof(uint8_t));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed. ret = %d\n", ret);
            exit(-1);
        }

        ret = bm_memcpy_s2d(handle, input, input_addr);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed. ret = %d\n", ret);
            exit(-1);
        }

        ret = bmcv_hist_balance(handle, input, output, H, W);
        if (ret != BM_SUCCESS) {
            printf("bmcv_hist_balance failed. ret = %d\n", ret);
            exit(-1);
        }

        ret = bm_memcpy_d2s(handle, output_addr, output);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s failed. ret = %d\n", ret);
            exit(-1);
        }

        free(input_addr);
        free(output_addr);
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        bm_dev_free(handle);