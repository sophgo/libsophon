bmcv_image_vpp_stitch
=====================

使用vpp硬件资源的 crop 功能，实现图像拼接的效果，对输入 image 可以一次完成 src crop + csc + resize + dst crop操作。dst image 中拼接的小图像数量不能超过256。


    .. code-block:: c

        bm_status_t bmcv_image_vpp_stitch(
            bm_handle_t           handle,
            int                   input_num,
            bm_image*             input,
            bm_image              output,
            bmcv_rect_t*          dst_crop_rect,
            bmcv_rect_t*          src_crop_rect = NULL,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);

**传入参数说明:**

* bm_handle_t handle

输入参数。设备环境句柄,通过调用 bm_dev_request 获取

* int input_num

输入参数。输入 bm_image 数量

* bm_imagei\* input

输入参数。输入 bm_image 对象指针

* bm_image output

输出参数。输出 bm_image 对象

* bmcv_rect_t \*   dst_crop_rect

输入参数。在dst images上，各个目标小图的坐标和宽高信息

* bmcv_rect_t \*   src_crop_rect

输入参数。在src image上，各个目标小图的坐标和宽高信息

具体格式定义如下：

    .. code-block:: c

       typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* bmcv_resize_algorithm algorithm

输入参数。resize 算法选择，包括 BMCV_INTER_NEAREST 和 BMCV_INTER_LINEAR 两种，默认情况下是双线性差值。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项:**

1. 该 API 的src image不支持压缩格式的数据。

2. 该 API 所需要满足的格式以及部分要求与 bmcv_image_vpp_basic 一致。

3. 如果对src image做crop操作，一张src image只crop一个目标。


