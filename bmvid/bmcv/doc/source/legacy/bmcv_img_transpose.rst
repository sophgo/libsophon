bmcv_img_transpose
===================

此接口用于BM1682对bgr格式图像的转置，将图像按照矩阵转置的方式转置


    .. code-block:: c

        bm_status_t bmcv_img_transpose(
            bm_handle_t     handle,
        	bmcv_image		input,
        	bmcv_image		output
        );



**传入参数说明:**

* bm_handle_t handle

* 设备环境句柄,通过调用 bm_dev_request 获取

* bmcv_image input

输入图像，支持BGR planar或者RGB planar类型的float或byte类型

* bmcv_image output

输出图像，支持BGR planar或者RGB planar类型的float或byte类型，BGR摆放顺序与输入相同
