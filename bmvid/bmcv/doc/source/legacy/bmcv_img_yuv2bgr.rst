bmcv_img_yuv2bgr
=================

此函数用于BM1682将NV12格式的图像数据转换为BGR格式的数据


    .. code-block:: c

        bm_status_t bm_img_yuv2bgr(
            bm_handle_t      handle,
            bmcv_image       input,
            bmcv_image       output
        );


**传入参数说明:**

* bm_handle_t handle

* 设备环境句柄,通过调用 bm_dev_request 获取

* bmcv_image input

输入的图像描述结构, 当前只支持NV12格式的输入图像，颜色空间支持YUV和YCbCr

* bmcv_image output

输出的图像的描述结构，输出支持BGR planar/packed或者RGB planar/packed格式摆放。输出可选float类型或者byte类型
