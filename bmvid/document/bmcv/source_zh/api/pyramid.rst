bmcv_image_pyramid_down
=======================

该接口实现图像高斯金字塔操作中的向下采样。

**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_pyramid_down(
                 bm_handle_t handle,
                 bm_image input,
                 bm_image output);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。bm_image 的内存可以使用 bm_image_alloc_dev_mem 或者bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach
  来 attach 已有的内存。

* bm_image output

  输出参数。输出图像 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。bm_image的内存可以使用 bm_image_alloc_dev_mem 或者bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach
  来 attach 已有的内存。

**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败

**格式支持：**

该接口目前支持以下 image_format与data_type:

+-----+------------------------+------------------------+
| num | image_format           | data_type              |
+=====+========================+========================+
| 1   | FORMAT_GRAY            | DATA_TYPE_EXT_1N_BYTE  |
+-----+------------------------+------------------------+


**代码示例：**

    .. code-block:: c


        int height = 1080;
        int width = 1920;
        int ow = height / 2;
        int oh = width / 2;
        int channel = 1;
        unsigned char* input_data = new unsigned char [width * height * channel];
        unsigned char* output_tpu = new unsigned char [ow * oh * channel];
        unsigned char* output_ocv = new unsigned char [ow * oh * channel];

        for (int i = 0; i < height * channel; i++) {
            for (int j = 0; j < width; j++) {
                input_data[i * width + j] = rand() % 100;
            }
        }

        bm_handle_t handle;
        bm_status_t ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            return -1;
        }
        bm_image_format_ext fmt = FORMAT_GRAY;
        bm_image img_i;
        bm_image img_o;
        bm_image_create(handle, height, width, fmt, DATA_TYPE_EXT_1N_BYTE, &img_i);
        bm_image_create(handle, oh, ow, fmt, DATA_TYPE_EXT_1N_BYTE, &img_o);
        bm_image_alloc_dev_mem(img_i);
        bm_image_alloc_dev_mem(img_o);
        bm_image_copy_host_to_device(img_i, (void **)(&input));

        struct timeval t1, t2;
        gettimeofday_(&t1);
        bmcv_image_pyramid_down(handle, img_i, img_o);
        gettimeofday_(&t2);
        cout << "pyramid down TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

        bm_image_copy_device_to_host(img_o, (void **)(&output));
        bm_image_destroy(img_i);
        bm_image_destroy(img_o);
        bm_dev_free(handle);