bmcv_image_canny
================

边缘检测Canny算子。


**接口形式：**

    .. code-block:: c

         bm_status_t bmcv_image_canny(
                 bm_handle_t handle,
                 bm_image input,
                 bm_image output,
                 float threshold1,
                 float threshold2,
                 int aperture_size = 3,
                 bool l2gradient = false);


**参数说明：**

* bm_handle_t handle

输入参数。 bm_handle 句柄。

* bm_image input

输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* float threshold1

滞后处理过程中的第一个阈值。

* float threshold2

滞后处理过程中的第二个阈值。

* int aperture_size = 3

Sobel核的大小，目前仅支持3。

* bool l2gradient = false

是否使用L2范数来求图像梯度, 默认值为false。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+


目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用该接口之前必须确保输入的 image 内存已经申请。

2、input output 的 data_type，image_format必须相同。

3、目前支持图像的最大width为2048。

4、输入图像的stride必须和width一致。


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
        if (BM_SUCCESS != bmcv_image_canny(handle, input, output, 0, 200)) {
            std::cout << "bmcv canny error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


