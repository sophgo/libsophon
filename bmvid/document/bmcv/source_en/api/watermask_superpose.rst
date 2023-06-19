bmcv_image_watermark_superpose
=========================
This interface is used to overlay one or more watermarks on the image.

**Interface form 1：**
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

  This interface can realize the specified positions of different input maps and overlay different watermarks.

**Interface form 2：**
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

  This interface is a simplified version of Interface 1, and a watermark can be superimposed repeatedly at different positions in a picture.

**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity) obtained by calling bm_dev_request.

* bm_image\* image

  Input parameter. The bm_image on which users need to add watermarks.

* bm_device_mem_t\* bitmap_mem

  Input parameters. Pointer to bm_device_mem_t Object of watermarks.

* int bitmap_num

  Input parameters. Number of watermarks, It refers to the number of bmcv_rect_t objects contained in the rects pointer, also the number of bm_image objects contained in the image pointer, and the number of bm_device_mem_t objects contained in the bitmap_mem pointer.

* int bitmap_type

  Input parameters. Watermark type: value 0 indicates that the watermark is an 8bit data type (with transparency information), and value 1 indicates that the watermark is a 1bit data type (without transparency information).

* int pitch

  Input parameters. The number of byte per line of the watermark file can be interpreted as the width of the watermark.

* bmcv_rect_t\* rects

  Input parameters. Watermark position pointer, including the starting point, width and height of each watermark. Please refer to the following data type description for details.

* bmcv_color_t color

  Input parameters. The color of the watermark. Please refer to the following data type description for details.


**Return value description:**

* BM_SUCCESS: success

* Other: failed

**Data type description：**


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


* start_x describes the starting horizontal coordinate of where the watermask is located in the original image. It starts at 0 from left to right and takes values in the range [0, width).

* start_y describes the starting vertical coordinate of where the watermask is located in the original image. It starts at 0 from top to bottom and takes values in the range [0, height).

* crop_w describes the width of the crop image.

* crop_h describes the height of the crop image.

* r R component of color

* g G component of color

* b B component of color


**Note:**

1. bm1684x：

- bm1684x supports the following data_type of bm_image:

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

- bm1684x supports the following image_format of bm_image:

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

Returns a failure if the input and output format requirements are not met.

2. bm1684: bm1684 Watermark function is not supported。

3. All input and output bm_image structures must be created in advance, or a failure will be returned.

4. The maximum number of watermarks can be 512.

5. If the watermark area exceeds the width and height of the original image, a failure will be returned.
