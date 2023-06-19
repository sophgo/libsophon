bmcv_img_bgrsplit
===================

此接口用于BM1682对bgr数据的从interleave排列重排为plannar排列。可以使用 bmcv_image_vpp_convert 接口实现该功能。


    .. code-block:: c

        bm_status_t bmcv_img_bgrsplit(
            bm_handle_t     handle,
            bmcv_image		input,
            bmcv_image		output
        );



**传入参数说明:**

* bm_handle_t handle

设备环境句柄,通过调用 bm_dev_request 获取

* bmcv_image input

输入待处理的图像数据，bgr数据，packed排列方式

bmcv_image output

输出待处理的图像数据，bgr数据，plannar排列方式
