bmcv_image_laplacian
====================

梯度计算laplacian算子。


**接口形式：**

    .. code-block:: c
    
        bm_status_t  bmcv_image_laplacian(
            bm_handle_t handle,
            bm_image input,
            bm_image output,
            unsigned int ksize);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* int ksize = 3

  Laplacian核的大小，必须是1或3。




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

2、input output 的 data_type必须相同。

3、目前支持图像的最大width为2048。


**代码示例：**

    .. code-block:: c

        int loop =1;
        int ih = 1080;
        int iw = 1920;
        unsigned int ksize = 3;
        bm_image_format_ext fmt = FORMAT_GRAY;

        fmt = argc > 1 ? (bm_image_format_ext)atoi(argv[1]) : fmt;
        ih = argc > 2 ? atoi(argv[2]) : ih;
        iw = argc > 3 ? atoi(argv[3]) : iw;
        loop = argc > 4 ? atoi(argv[4]) : loop;
        ksize = argc >5 ? atoi(argv[5]) : ksize;

        bm_status_t ret = BM_SUCCESS;
        bm_handle_t handle;
        ret = bm_dev_request(&handle, 0);
        if (ret != BM_SUCCESS)
            throw("bm_dev_request failed");
    
        bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE;
        bm_image input;
        bm_image output;
   
        bm_image_create(handle, ih, iw, fmt, data_type, &input);
        bm_image_alloc_dev_mem(input);

        bm_image_create(handle,ih, iw, fmt, data_type, &output);
        bm_image_alloc_dev_mem(output);

        std::shared_ptr<unsigned char*> ch0_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
        std::shared_ptr<unsigned char*> tpu_res_ptr = std::make_shared<unsigned char *>(new unsigned char[ih * iw]);
        std::shared_ptr<unsigned char*> cpu_res_ptr = std::make_shared<unsigned char *>(new unsigned char[ih*iw]);
    
        for (int i = 0; i < loop; i++) {
            for (int j = 0; j < ih * iw; j++) {
                (*ch0_ptr.get())[j] = j % 256;
            }

            unsigned char *host_ptr[] = {*ch0_ptr.get()};
            bm_image_copy_host_to_device(input, (void **)host_ptr);

            ret = bmcv_image_laplacian(handle, input, output, ksize);
            if (ret) {
                cout << "test laplacian failed" << endl;
                bm_image_destroy(input);
                bm_image_destroy(output);
                bm_dev_free(handle);
                return ret;
            } else {
                host_ptr[0] = *tpu_res_ptr.get();
                bm_image_copy_device_to_host(output, (void **)host_ptr);
            }
        }

        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


