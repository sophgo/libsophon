bmcv_image_vpp_convert_padding
==============================

使用vpp硬件资源，结合对 dst image 做 memset 操作，实现图像padding的效果。这个效果的实现是利用了vpp的dst crop的功能，通俗的讲是将一张小图填充到大图中。
可以从一张src image上crop多个目标图像，对于每一个目标小图，可以一次性完成csc+resize操作，然后根据其在大图中的offset信息，填充到大图中。
一次crop的数量不能超过256。


    .. code-block:: c

        bm_status_t bmcv_image_vpp_convert_padding(
            bm_handle_t           handle,
            int                   output_num,
            bm_image              input,
            bm_image *            output,
            bmcv_padding_atrr_t * padding_attr,
            bmcv_rect_t *         crop_rect = NULL,
            bmcv_resize_algorithm algorithm = BMCV_INTER_LINEAR);

**传入参数说明:**

* bm_handle_t handle

输入参数。设备环境句柄,通过调用 bm_dev_request 获取

* int output_num

输出参数。输出 bm_image 数量，和src image的crop 数量相等,一个src crop 输出一个dst bm_image

* bm_image input

输入参数。输入 bm_image 对象

* bm_image\* output

输出参数。输出 bm_image 对象指针

* bmcv_padding_atrr_t \*  padding_attr

输入参数。src crop的目标小图在dst image中的位置信息以及要pdding的各通道像素值

    .. code-block:: c

        typedef struct bmcv_padding_atrr_s {
            unsigned int    dst_crop_stx;
            unsigned int    dst_crop_sty;
            unsigned int    dst_crop_w;
            unsigned int    dst_crop_h;
            unsigned char padding_r;
            unsigned char padding_g;
            unsigned char padding_b;
            int           if_memset;
        } bmcv_padding_atrr_t;


1. 目标小图的左上角顶点相对于 dst image 原点（左上角）的offset信息：dst_crop_stx 和 dst_crop_sty；
#. 目标小图经resize后的宽高：dst_crop_w 和 dst_crop_h；
#. dst image 如果是RGB格式，各通道需要padding的像素值信息：padding_r、padding_g、padding_b，当if_memset=1时有效，如果是GRAY图像可以将三个值均设置为同一个值；
#. if_memset表示要不要在该api内部对dst image 按照各个通道的padding值做memset，仅支持RGB和GRAY格式的图像。如果设置为0则用户需要在调用该api前，根据需要 padding 的像素值信息，调用 bmlib 中的 api 直接对 device memory 进行 memset 操作，如果用户对padding的值不关心，可以设置为0忽略该步骤。


* bmcv_rect_t \*   crop_rect

输入参数。在src image上的各个目标小图的坐标和宽高信息

具体格式定义如下：

    .. code-block:: c

       typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* bmcv_resize_algorithm algorithm

输入参数。resize 算法选择，包括 BMCV_INTER_NEAREST 和 BMCV_INTER_LINEAR 两种，默认情况下是双线性差值


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**注意事项:**

1. 该 API 的dst image的格式仅支持:FORMAT_RGB_PLANAR、FORMAT_BGR_PLANAR、FORMAT_RGBP_SEPARATE、FORMAT_BGRP_SEPARATE、FORMAT_RGB_PACKED和FORMAT_BGR_PACKED。

2. 在调用该api前，需要根据各通道padding的像素值对dst image的device memory进行memset操作，并将if_memset置0，否则，padding的部分是内存中残留数据。

3. 该 API 所需要满足的格式以及部分要求与 bmcv_image_vpp_basic 一致。


