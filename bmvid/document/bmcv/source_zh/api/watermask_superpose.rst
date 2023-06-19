bmcv_image_watermark_superpose
=========================
该接口用于在图像上叠加一个或多个水印。

**接口形式一：**
    .. code-block:: c

          bm_status_t bmcv_image_watermark_superpose(
                            bm_handle_t handle,
                            bm_image * image,
                            bm_device_mem_t * bitmap_mem,
                            int bitmap_num,
                            int bitmap_type,
                            int pitch,
                            bmcv_rect_t * rects,
                            bmcv_color_t color)

  此接口可实现在不同的输入图的指定位置，叠加不同的水印。

**接口形式二：**
    .. code-block:: c

          bm_status_t bmcv_image_watermark_repeat_superpose(
                            bm_handle_t handle,
                            bm_image image,
                            bm_device_mem_t bitmap_mem,
                            int bitmap_num,
                            int bitmap_type,
                            int pitch,
                            bmcv_rect_t * rects,
                            bmcv_color_t color)

  此接口为接口一的简化版本，可在一张图中的不同位置重复叠加一种水印。

**传入参数说明:**

* bm_handle_t handle

  输入参数。设备环境句柄，通过调用 bm_dev_request 获取。

* bm_image\* image

  输入参数。需要打水印的 bm_image 对象指针。

* bm_device_mem_t\* bitmap_mem

  输入参数。水印的 bm_device_mem_t 对象指针。

* int bitmap_num

  输入参数。水印数量，指 rects 指针中所包含的 bmcv_rect_t 对象个数、 也是 image 指针中所包含的 bm_image 对象个数、 也是 bitmap_mem 指针中所包含的 bm_device_mem_t 对象个数。

* int bitmap_type

  输入参数。水印类型, 值0表示水印为8bit数据类型(有透明度信息), 值1表示水印为1bit数据类型(无透明度信息)。

* int pitch

  输入参数。水印文件每行的byte数, 可理解为水印的宽。

* bmcv_rect_t\* rects

  输入参数。水印位置指针，包含每个水印起始点和宽高。具体内容参考下面的数据类型说明。

* bmcv_color_t color

  输入参数。水印的颜色。具体内容参考下面的数据类型说明。


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

        typedef struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } bmcv_color_t;


* start_x 描述了水印在原图中所在的起始横坐标。自左而右从 0 开始，取值范围 [0, width)。

* start_y 描述了水印在原图中所在的起始纵坐标。自上而下从 0 开始，取值范围 [0, height)。

* crop_w 描述的水印的宽度。

* crop_h 描述的水印的高度。

* r 颜色的r分量。

* g 颜色的g分量。

* b 颜色的b分量。


**注意事项:**

1. bm1684x要求如下：

- 输入和输出的数据类型必须为：

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

- 输入的色彩格式可支持：

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

2. bm1684部分：bm1684不支持水印功能。

3. 输入输出所有 bm_image 结构必须提前创建，否则返回失败。

4. 水印数量最多可设置512个。

5. 如果水印区域超出原图宽高，会返回失败。
