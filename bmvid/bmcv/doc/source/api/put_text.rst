bmcv_image_put_text
===================

可以实现在一张图像上写字的功能（英文），并支持指定字的颜色、大小和宽度。


**接口形式：**

    .. code-block:: c

        typedef struct {
            int x;
            int y;
        } bmcv_point_t;

        typedef struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } bmcv_color_t;

        bm_status_t bmcv_image_put_text(
                bm_handle_t handle,
                bm_image image,
                const char* text,
                bmcv_point_t org,
                bmcv_color_t color,
                float fontScale,
                int thickness);


**参数说明：**

* bm_handle_t handle

输入参数。 bm_handle 句柄。

* bm_image image

输入/输出参数。需处理图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* const char* text

输入参数。待写入的文本内容，目前仅支持英文。

* bmcv_point_t org

输入参数。第一个字符左下角的坐标位置。图像左上角为原点，向右延伸为x方向，向下延伸为y方向。

* bmcv_color_t color

输入参数。画线的颜色，分别为RGB三个通道的值。

* float fontScale

输入参数。字体大小。

* int thickness

输入参数。画线的宽度，对于YUV格式的图像建议设置为偶数。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_GRAY            |
+-----+------------------------+
| 2   | FORMAT_YUV420P         |
+-----+------------------------+
| 3   | FORMAT_YUV422P         |
+-----+------------------------+
| 4   | FORMAT_YUV444P         |
+-----+------------------------+
| 5   | FORMAT_NV12            |
+-----+------------------------+
| 6   | FORMAT_NV21            |
+-----+------------------------+
| 7   | FORMAT_NV16            |
+-----+------------------------+
| 8   | FORMAT_NV61            |
+-----+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**代码示例：**

    .. code-block:: c


        int channel   = 1;
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        int thickness = 4
        float fontScale = 4;
        char text[20] = "hello world";
        bmcv_point_t org = {100, 100};
        bmcv_color_t color = {255, 0, 0};
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> data_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        for (int i = 0; i < channel * width * height; i++) {
            data_ptr.get()[i] = rand() % 255;
        }
        // calculate res
        bm_image img;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &img);
        bm_image_alloc_dev_mem(img);
        bm_image_copy_host_to_device(img, (void **)&(data_ptr.get()));
        if (BM_SUCCESS != bmcv_image_put_text(handle, img, text, org, color, fontScale, thickness)) {
            std::cout << "bmcv put text error !!!" << std::endl;
            bm_image_destroy(img);
            bm_dev_free(handle);
            return;
        }
        bm_image_copy_device_to_host(img, (void **)&(data_ptr.get()));
        bm_image_destroy(img);
        bm_dev_free(handle);


