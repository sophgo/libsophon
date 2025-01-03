bmcv_image_sobel
================

边缘检测Sobel算子。

**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

         bm_status_t bmcv_image_sobel(
                 bm_handle_t handle,
                 bm_image input,
                 bm_image output,
                 int dx,
                 int dy,
                 int ksize = 3,
                 float scale = 1,
                 float delta = 0);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* int dx

  x方向上的差分阶数。

* int dy

  y方向上的差分阶数。

* int ksize = 3

  Sobel核的大小，必须是1，3，5或7。3，5，7取值情况下，核大小为3*3，5*5，7*7。如果取1，则按照dx和dy的值决定Sobel核的大小，dx=1，dy=0则核大小为3×1，dx=0，dy=1，则核大小为1×3，dx=1,dy=1,核大小变为3*3。ksize默认值为3。

* float scale = 1

  对求出的差分结果乘以该系数，默认值为1。

* float delta = 0

  在输出最终结果之前加上该偏移量，默认值为0。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+
| 2   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 3   | FORMAT_RGBP_SEPARATE   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+------------------------+
| 4   | FORMAT_BGRP_SEPARATE   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+------------------------+
| 5   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 6   | FORMAT_YUV420P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 7   | FORMAT_YUV422P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 8   | FORMAT_YUV444P         | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 9   | FORMAT_NV12            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 10  | FORMAT_NV21            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 11  | FORMAT_NV16            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 12  | FORMAT_NV61            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 13  | FORMAT_NV24            | FORMAT_GRAY            |
+-----+------------------------+------------------------+


目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用该接口之前必须确保输入的 image 内存已经申请。

2、input output 的 data_type必须相同。

3、BM1684芯片下该算子支持图像的最大width为(2048 - ksize)。BM1684X芯片下该算子在Sobel核大小为1和3时，支持的宽高范围为8*8～8192*8192，核大小为5时支持的宽高范围为8*8～4096*8192，核大小为7时支持的宽高范围为8*8～2048*8192。


**代码示例：**

    .. code-block:: c


        int channel   = 1;
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> res_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        unsigned char * src_data = src_ptr.get();
        unsigned char * res_data = res_ptr.get();
        for (int i = 0; i < channel * width * height; i++) {
            src_data[i] = rand() % 255;
        }
        // calculate res
        bm_image input, output;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &input);
        bm_image_alloc_dev_mem(input);
        bm_image_copy_host_to_device(input, (void **)&src_data);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &output);
        bm_image_alloc_dev_mem(output);
        if (BM_SUCCESS != bmcv_image_sobel(handle, input, output, 0, 1)) {
            std::cout << "bmcv sobel error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


