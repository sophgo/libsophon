bmcv_img_crop
===============

此接口用于BM1682从bgr图像中剪切一部分形成新的图像


    .. code-block:: c

        bm_status_t bmcv_img_crop(
            bm_handle_t       handle,
            bmcv_image        input,
            int               input_c,
            int               top,
            int               left,
            bmcv_image        output
        );



**传入参数说明:**

* bm_handle_t handle

* 设备环境句柄,通过调用 bm_dev_request 获取

* bmcv_image input

输入图像，只支持BGR planar或者RGB planar格式，输入输出的BGR或者RGB顺序一致不做交换。输入输出可以选择数据类型为float或者byte

* int  input_c

输入数据颜色数，可以为1或者3，相应输出颜色数也会是1或者3

* int  top

剪切开始点纵坐标，图片左上角为原点，单位为像素


* int  left

剪切开始点横坐标，图片左上角为原点，单位为像素


* bmcv_image output

输出的图像的描述结构
