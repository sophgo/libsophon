bmcv_image_fill_rectangle
=========================

This interface is used to fill one or more rectangles on the image.

**Interface form:**
    .. code-block:: c

        bm_status_t bmcv_image_fill_rectangle(
                bm_handle_t   handle,
                bm_image      image,
                int           rect_num,
                bmcv_rect_t * rects,
                unsigned char r,
                unsigned char g,
                unsigned char b)


**Description of incoming parameters::**

* bm_handle_t handle

  Input parameter. Handle of device’s capacity (HDC) which is obtained by calling bm_dev_request.

* bm_image image

  Input parameter. The bm_image object on which users want to fill a rectangle.

* int rect_num

  Input parameter. The number of rectangles to be filled, which refers to the number of bmcv_rect_t object contained in the rects pointer.

* bmcv_rect_t\* rect

  Input parameter. Pointer to a rectangular object that contains the start point and width height of the rectangle. Refer to the following data type description for details.

* unsigned char r

  Input parameter. The r component of the rectangle fill color.

* unsigned char g

  Input parameter. The g component of the rectangle fill color.

* unsigned char b

  Input parameter. The b component of the rectangle fill color.


**Return value Description:**

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

* crop_w describes the width of the crop image, that is, the width of the corresponding output image.

* crop_h describes the height of the crop image, that is, the height of the corresponding output image.



**Note:**

1. bm1684 supports the following formats of bm_image:

+-----+-------------------------------+
| num | input image_format            |
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
|  6  | RGB_PLANAR                    |
+-----+-------------------------------+
|  7  | RGB_PACKED                    |
+-----+-------------------------------+
|  8  | BGR_PLANAR                    |
+-----+-------------------------------+
|  9  | BGR_PACKED                    |
+-----+-------------------------------+

bm1684x supports the following formats of bm_image:

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_NV12                   |
+-----+-------------------------------+
|  2  | FORMAT_NV21                   |
+-----+-------------------------------+
|  3  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  4  | RGB_PLANAR                    |
+-----+-------------------------------+
|  5  | RGB_PACKED                    |
+-----+-------------------------------+
|  6  | BGR_PLANAR                    |
+-----+-------------------------------+
|  7  | BGR_PACKED                    |
+-----+-------------------------------+

bm1684x supports the following data_type of bm_image:

+-----+-------------------------------+
| num | intput data_type              |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+


If the input/output format requirements are not met, a failure will be returned.

2. All input and output bm_image structures must be created in advance, or a failure will be returned.

3. If rect_num is 0, a success will be returned automatically.

4. If the part of all input rectangular objects is outside the image, only the part inside the image will be filled and a success will be returned.


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
             bmcv_image_fill_rectangle(handle, src, 1, &rect, 255, 0, 0);
             bm_image_destroy(src);
             bm_dev_free(handle);
             return 0;
         }


