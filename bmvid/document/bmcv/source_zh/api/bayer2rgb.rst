bmcv_image_bayer2rgb
==================

将bayerBG8格式图像转成RGB Plannar格式。

**接口形式：**


    .. code-block:: c

         bm_status_t bmcv_image_bayer2rgb(
                 bm_handle_t handle,
                 unsigned char* convd_kernel,
                 bm_image input
                 bm_image output);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* unsigned char* convd_kernel

  输入参数。用于卷积计算的卷积核。

* bm_image input

  输入参数。输入bayerBG8格式图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**格式支持：**

该接口目前支持以下输入格式:

+-----+--------------------------------+
| num | image_format                   |
+=====+================================+
| 1   | FORMAT_BAYER                   |
+-----+--------------------------------+


该接口目前支持以下输出格式:

+-----+--------------------------------+
| num | image_format                   |
+=====+================================+
| 1   | FORMAT_RGB_PLANAR              |
+-----+--------------------------------+


目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、input的格式是bayerBG，output的格式是rgb plannar， data_type均为uint8类型。
2、该接口目前支持bm1684x。
3、该接口支持的尺寸范围是 8*8 ~ 8096*8096，且图像的宽高需要是偶数。


**代码示例：**

    .. code-block:: c


        #define KERNEL_SIZE 3 * 3 * 3 * 4 * 64
        #define CONVD_MATRIX 12 * 9

        const unsigned char convd_kernel[CONVD_MATRIX] = {1, 0, 1, 0, 0, 0, 1, 0, 1,
                                                        0, 0, 2, 0, 0, 0, 0, 0, 2,
                                                        0, 0, 0, 0, 0, 0, 2, 0, 2,
                                                        0, 0, 0, 0, 0, 0, 0, 0, 4, // r R
                                                        4, 0, 0, 0, 0, 0, 0, 0, 0, // b B
                                                        2, 0, 2, 0, 0, 0, 0, 0, 0,
                                                        2, 0, 0, 0, 0, 0, 2, 0, 0,
                                                        1, 0, 1, 0, 0, 0, 1, 0, 1,
                                                        0, 1, 0, 1, 0, 1, 0, 1, 0,
                                                        0, 0, 0, 0, 0, 4, 0, 0, 0, // g1 G1
                                                        0, 0, 0, 0, 0, 0, 0, 4, 0, // g2 G2
                                                        0, 1, 0, 1, 0, 1, 0, 1, 0};
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src1_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        bm_image input_img;
        bm_image output_img;
        bm_image_create(handle, height, width, FORMAT_BAYER, DATA_TYPE_EXT_1N_BYTE, &input_img);
        bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output_img);
        bm_image_alloc_dev_mem(output_img, BMCV_HEAP_ANY);

        unsigned char kernel_data[KERNEL_SIZE];
        memset(kernel_data, 0, KERNEL_SIZE);
        // constructing convd_kernel_data
        for (int i = 0;i < 12;i++) {
            for (int j = 0;j < 9;j++) {
                kernel_data[i * 9 * 64 + 64 * j] = convd_kernel[i * 9 + j];
            }
        }
        unsigned char* input_data[3] = {srcImage.data, srcImage.data + height * width, srcImage.data + 2 * height * width};
        bm_image_copy_host_to_device(input_img, (void **)input_data);
        bmcv_image_bayer2rgb(handle, kernel_data, input_img, output_img);
        bm_image_copy_device_to_host(output_img, (void **)(&output));
        bm_image_destroy(input_img);
        bm_image_destroy(output_img);
        bm_dev_free(handle);