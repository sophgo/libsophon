bmcv_image_bitwise_or
=====================



两张大小相同的图片对应像素值进行按位或操作。


**接口形式：**

    .. code-block:: c

         bm_status_t bmcv_image_bitwise_or(
                 bm_handle_t handle,
                 bm_image input1,
                 bm_image input2,
                 bm_image output);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input1

  输入参数。输入第一张图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image input2

  输入参数。输入第二张图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_BGR_PACKED      |
+-----+------------------------+
| 2   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 3   | FORMAT_RGB_PACKED      |
+-----+------------------------+
| 4   | FORMAT_RGB_PLANAR      |
+-----+------------------------+
| 5   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+
| 6   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+
| 7   | FORMAT_GRAY            |
+-----+------------------------+
| 8   | FORMAT_YUV420P         |
+-----+------------------------+
| 9   | FORMAT_YUV422P         |
+-----+------------------------+
| 10  | FORMAT_YUV444P         |
+-----+------------------------+
| 11  | FORMAT_NV12            |
+-----+------------------------+
| 12  | FORMAT_NV21            |
+-----+------------------------+
| 13  | FORMAT_NV16            |
+-----+------------------------+
| 14  | FORMAT_NV61            |
+-----+------------------------+
| 15  | FORMAT_NV24            |
+-----+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用 bmcv_image_bitwise_or()之前必须确保输入的 image 内存已经申请。

2、input output 的 data_type，image_format必须相同。



**代码示例：**

    .. code-block:: c


        int channel   = 3;
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src1_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> src2_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> res_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        unsigned char * src1_data = src1_ptr.get();
        unsigned char * src2_data = src2_ptr.get();
        unsigned char * res_data = res_ptr.get();
        for (int i = 0; i < channel * width * height; i++) {
            src1_data[i] = rand() % 255;
            src2_data[i] = rand() % 255;
        }
        // calculate res
        bm_image input1, input2, output;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_RGB_PLANAR,
                        DATA_TYPE_EXT_1N_BYTE,
                        &input1);
        bm_image_alloc_dev_mem(input1);
        bm_image_copy_host_to_device(input1, (void **)&src1_data);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_RGB_PLANAR,
                        DATA_TYPE_EXT_1N_BYTE,
                        &input2);
        bm_image_alloc_dev_mem(input2);
        bm_image_copy_host_to_device(input2, (void **)&src2_data);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_RGB_PLANAR,
                        DATA_TYPE_EXT_1N_BYTE,
                        &output);
        bm_image_alloc_dev_mem(output);
        if (BM_SUCCESS != bmcv_image_bitwise_or(handle, input1, input2, output)) {
            std::cout << "bmcv bitwise or error !!!" << std::endl;
            bm_image_destroy(input1);
            bm_image_destroy(input2);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input1);
        bm_image_destroy(input2);
        bm_image_destroy(output);
        bm_dev_free(handle);


