bmcv_image_draw_rectangle
=========================

This interface is used to draw one or more rectangular boxes on the image.

**Interface form:**
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


**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity) obtained by calling bm_dev_request.

* bm_image image

  Input parameter. The bm_image on which users need to draw a rectangle.

* int rect_num

  Input parameter. The number of rectangular boxes, which refers to the number of bmcv_rect_t objects in the rects pointer.

* bmcv_rect_t\* rect

  Input parameter. Pointer to a rectangular box object that contains the starting point, width, and height of the rectangle. Refer to the following data type description for details.

* int line_width

  Input parameter. The width of the rectangle line.

* unsigned char r

  Input parameter. The r component of the rectangle color.

* unsigned char g

  Input parameter. The g component of the rectangle color.

* unsigned char b

  Input parameter. The b component of the rectangle color.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Data type description:**


    .. code-block:: c

        typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;


* start_x describes the starting horizontal coordinate of where the crop image is located in the original image. It starts at 0 from left to right and takes values in the range [0, width).

* start_y describes the starting vertical coordinate of where the crop image is located in the original image. It starts at 0 from top to bottom and takes values in the range [0, height).

* crop_w describes the width of the crop image, that is the width of the corresponding output image.

* crop_h describes the height of the crop image, that is the height of the corresponding output image.


**Note:**

1. bm1684x:

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

2. bm1684:

- The API inputs image objects in NV12 / NV21 / NV16 / NV61 / YUV420P / RGB_PLANAR / RGB_PACKED / BGR_PLANAR / BGR_PACKED formats and directly draw a frame on the corresponding device memory without additional memory application and copy.

- At present, the API supports the following image formats of input bm_image:

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

the API supports the following data format of input bm_image:

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+

If the input/output format requirements are not met, a failure will be returned.

3. All input and output bm_image structures must be created in advance, or a failure will be returned.

4. If the image is in NV12 / NV21 / NV16 / NV61 / YUV420P format, the line_width will be automatically even aligned.

5. If rect_num is 0, a success will be returned automatically.

6. If line_width is less than zero, a failure will be returned.

7. If all input rectangular objects are outside the image, only the lines within the image will be drawn and a success will be returned.

**Code example**

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


