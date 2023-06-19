bmcv_image_morph
================

可以实现对图像的基本形态学运算，包括膨胀(Dilation)和腐蚀(Erosion)。

用户可以分为以下两步使用该功能：


获取 Kernel 的 Device Memory
-----------------------------

可以在初始化时使用以下接口获取存储 Kernel 的 Device Memory，当然用户也可以自定义 Kernel 直接忽略该步骤。

函数通过传入所需 Kernel 的大小和形状，返回对应的 Device Memory 给后面的形态学运算接口使用，用户应用程序的最后需要用户手动释放该空间。


**接口形式：**

    .. code-block:: c

        typedef enum {
            BM_MORPH_RECT,
            BM_MORPH_CROSS,
            BM_MORPH_ELLIPSE
        } bmcv_morph_shape_t;

        bm_device_mem_t bmcv_get_structuring_element(
                bm_handle_t handle,
                bmcv_morph_shape_t shape,
                int kw,
                int kh
                );

**参数说明：**

* bm_handle_t handle

输入参数。 bm_handle 句柄。

* bmcv_morph_shape_t shape

输入参数。表示 Kernel 的形状，目前支持矩形、十字、椭圆。

* int kw

输入参数。Kernel 的宽度。

* int kh

输入参数。Kernel 的高度。


**返回值说明：**

返回 Kernel 对应的 Device Memory 空间。


形态学运算
----------

目前支持腐蚀和膨胀操作，用户也可以通过这两个基本操作的组合实现以下功能：

* 开运算(Opening)
* 闭运算(Closing)
* 形态梯度(Morphological Gradient)
* 顶帽(Top Hat)
* 黑帽(Black Hat)


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_erode(
                bm_handle_t handle,
                bm_image src,
                bm_image dst,
                int kw,
                int kh,
                bm_device_mem_t kmem
                );

        bm_status_t bmcv_image_dilate(
                bm_handle_t handle,
                bm_image src,
                bm_image dst,
                int kw,
                int kh,
                bm_device_mem_t kmem
                );


**参数说明：**

* bm_handle_t handle

输入参数。 bm_handle 句柄。

* bm_image src

输入参数。需处理图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image dst

输出参数。处理后图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存，如用户不申请内部会自动申请。

* int kw

输入参数。Kernel 的宽度。

* int kh

输入参数。Kernel 的高度。

* bm_device_mem_t kmem

输入参数。存储 Kernel 的 Device Memory 空间，可以通过接口bmcv_get_structuring_element获取，用户也可以自定义，其中值为1表示选中该像素，值为0表示忽略该像素。


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
| 2   | FORMAT_RGB_PLANAR      |
+-----+------------------------+
| 3   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 4   | FORMAT_RGB_PACKED      |
+-----+------------------------+
| 5   | FORMAT_BGR_PACKED      |
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
        int kw        = 3;
        int kh        = 3;
        int dev_id    = 0;
        bmcv_morph_shape_t shape = BM_MORPH_RECT;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        bm_device_mem_t kmem = bmcv_get_structuring_element(
                handle,
                shape,
                kw,
                kh);
        std::shared_ptr<unsigned char> data_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        for (int i = 0; i < channel * width * height; i++) {
            data_ptr.get()[i] = rand() % 255;
        }
        // calculate res
        bm_image src, dst;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &src);
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &dst);
        bm_image_alloc_dev_mem(src);
        bm_image_alloc_dev_mem(dst);
        bm_image_copy_host_to_device(src, (void **)&(data_ptr.get()));
        if (BM_SUCCESS != bmcv_image_erode(handle, src, dst, kw, kh, kmem)) {
            std::cout << "bmcv erode error !!!" << std::endl;
            bm_image_destroy(src);
            bm_image_destroy(dst);
            bm_free_device(handle, kmem);
            bm_dev_free(handle);
            return;
        }
        bm_image_copy_device_to_host(dst, (void **)&(data_ptr.get()));
        bm_image_destroy(src);
        bm_image_destroy(dst);
        bm_free_device(handle, kmem);
        bm_dev_free(handle);

