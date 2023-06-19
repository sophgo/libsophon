bmcv_image_draw_rectangle
=========================
该接口用于在图像上画一个或多个矩形框。

**接口形式：**
    .. code-block:: c

        bm_status_t bmcv_image_draw_rectangle(
                bm_handle_t   handle,
                bm_image      image,
                int           rect_num,
                bmcv_rect_t * rects,
                int           line_width,
                unsigned char r,
                unsigned char g,
                unsigned char b)


**传入参数说明:**

* bm_handle_t handle

  输入参数。设备环境句柄，通过调用 bm_dev_request 获取。

* bm_image image

  输入参数。需要在其上画矩形框的 bm_image 对象。

* int rect_num

  输入参数。矩形框数量，指 rects 指针中所包含的 bmcv_rect_t 对象个数。

* bmcv_rect_t\* rect

  输入参数。矩形框对象指针，包含矩形起始点和宽高。具体内容参考下面的数据类型说明。

* int line_width

  输入参数。矩形框线宽。

* unsigned char r

  输入参数。矩形框颜色的r分量。

* unsigned char g

  输入参数。矩形框颜色的g分量。

* unsigned char b

  输入参数。矩形框颜色的g分量。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他:失败


**数据类型说明：**


    .. code-block:: c

        typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* start_x 描述了 crop 图像在原图中所在的起始横坐标。自左而右从 0 开始，取值范围 [0, width)。

* start_y 描述了 crop 图像在原图中所在的起始纵坐标。自上而下从 0 开始，取值范围 [0, height)。

* crop_w 描述的 crop 图像的宽度，也就是对应输出图像的宽度。

* crop_h 描述的 crop 图像的高度，也就是对应输出图像的高度。


**注意事项:**

1. bm1684x要求如下：

- 输入和输出的数据类型必须为：

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

- 输入和输出的色彩格式必须保持一致，可支持：

+-----+-------------------------------+
| num | image_format                  |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_YUV444P                |
+-----+-------------------------------+
|  3  | FORMAT_NV12                   |
+-----+-------------------------------+
|  4  | FORMAT_NV21                   |
+-----+-------------------------------+
|  5  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  6  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  7  | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  8  | FORMAT_BGR_PACKED             |
+-----+-------------------------------+
|  9  | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  10 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  11 | FORMAT_GRAY                   |
+-----+-------------------------------+

如果不满足输入输出格式要求，则返回失败。

2. bm1684部分：

- 该 API 输入 NV12 / NV21 / NV16 / NV61 / YUV420P / RGB_PLANAR / RGB_PACKED / BGR_PLANAR / BGR_PACKED 格式的 image 对象，并在对应的device memory上直接画框，没有额外的内存申请和copy。

- 目前该 API 支持输入 bm_image 图像格式为

+-----+-------------------------------+
| num | image_format                  |
+=====+===============================+
|  1  | FORMAT_NV12                   |
+-----+-------------------------------+
|  2  | FORMAT_NV21                   |
+-----+-------------------------------+
|  3  | FORMAT_NV16                   |
+-----+-------------------------------+
|  4  | FORMAT_NV61                   |
+-----+-------------------------------+
|  5  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  6  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  7  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  8  | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  9  | FORMAT_BGR_PACKED             |
+-----+-------------------------------+

支持输入 bm_image 数据格式为

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

如果不满足输入输出格式要求，则返回失败。

3. 输入输出所有 bm_image 结构必须提前创建，否则返回失败。

4. 如果image为NV12/NV21/NV16/NV61/YUV420P格式，则线宽line_width会自动偶数对齐。

5. 如果rect_num为0，则自动返回成功。

6. 如果line_width小于零，则返回失败。

7. 所有输入矩形对象部分在image之外，则只会画出在image之内的线条，并返回成功。


**代码示例**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include "bmlib_utils.h"
        #include "common.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <memory>

         int main(int argc, char *argv[]) {
             bm_handle_t handle;
             bm_dev_request(&handle, 0);

             int image_h = 1080;
             int image_w = 1920;
             bm_image src;
             bm_image_create(handle, image_h, image_w, FORMAT_NV12,
                     DATA_TYPE_EXT_1N_BYTE, &src);
             std::shared_ptr<u8*> y_ptr = std::make_shared<u8*>(
                     new u8[image_h * image_w]);
             memset((void *)(*y_ptr.get()), 148, image_h * image_w);
             memset((void *)(*uv_ptr.get()), 158, image_h * image_w / 2);
             u8 *host_ptr[] = {*y_ptr.get(), *uv_ptr.get()};
             bm_image_copy_host_to_device(src, (void **)host_ptr);
             bmcv_rect_t rect;
             rect.start_x = 100;
             rect.start_y = 100;
             rect.crop_w = 200;
             rect.crop_h = 300;
             bmcv_image_draw_rectangle(handle, src, 1, &rect, 3, 255, 0, 0);
             bm_image_destroy(src);
             bm_dev_free(handle);
             return 0;
         }


