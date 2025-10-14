bmcv_image_mosaic
=========================
This interface is used to print one or more mosaics on the image.


**Interface form：**
    .. code-block:: c

        bm_status_t bmcv_image_mosaic(
                    bm_handle_t handle,
                    int mosaic_num,
                    bm_image input,
                    bmcv_rect_t* mosaic_rect,
                    int is_expand);


**Processor model support**

This interface only supports BM1684X.


**Description of incoming parameters:**

* bm_handle_t handle

  Input parameter. HDC (handle of device’s capacity) obtained by calling bm_dev_request.

* int mosaic_num

  Input parameter. The number of mosaic, which refers to the number of bmcv_rect_t objects in the rects pointer.

* bm_image input

  Input parameter. The bm_image on which users need to add mosaic.

* bmcv_rect_t\* mosaic_rect

  Input parameter. Pointer to a mosaic object that contains the starting point, width, and height of the mosaic. Refer to the following data type description for details.

* int is_expand

  Input parameters. Whether to expand columns. A value of 0 means that the column is not expanded, and a value of 1 means that a macro block (8 pixels) is expanded around the original mosaic.


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

* start_x describes the starting horizontal coordinate of where the mosaic is located in the original image. It starts at 0 from left to right and takes values in the range [0, width).

* start_y describes the starting vertical coordinate of where the mosaic is located in the original image. It starts at 0 from top to bottom and takes values in the range [0, height).

* crop_w describes the width of the crop image.

* crop_h describes the height of the crop image.


**Note:**

1.bm1684x supports the following data_type of bm_image:

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

2. All input and output bm_image structures must be created in advance, or a failure will be returned.

3. If the width and height of the mosaic are not aligned with 8, it will automatically align up to 8. If it is in the edge area, the 8 alignment will extend toward the non edge direction.

4. If the mosaic area exceeds the width and height of the original drawing, the exceeding part will be automatically pasted to the edge of the original drawing.

5. Only mosaic sizes above 8x8 are supported.


**Sample code**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <sstream>
        #include <string.h>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void writeBin(const char * path, unsigned char* input_data, int size)
        {
            FILE *fp_dst = fopen(path, "wb");
            if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
            bm_handle_t handle = NULL;
            int width = 1024;
            int height = 1024;
            int dev_id = 0;
            int mosaic_num = 1;
            bm_image_format_ext src_fmt = FORMAT_GRAY;
            bm_image src;
            bmcv_rect_t* rect = new bmcv_rect_t [mosaic_num];
            unsigned char* data_ptr = new unsigned char[width * height];
            unsigned int is_expand = 1;
            const char *src_name = "path/to/src";
            const char *dst_name = "path/to/dst";

            for(int i = 0; i < mosaic_num; i++){
                rect[i].start_x = 8 + i * 8;
                rect[i].start_y = 8 + i * 8;
                rect[i].crop_w = 8 + i * 8;
                rect[i].crop_h = 8 + i * 8;
            }

            readBin(src_name, data_ptr, width * height);
            bm_dev_request(&handle, dev_id);
            bm_image_create(handle, height, width, src_fmt, DATA_TYPE_EXT_1N_BYTE, &src);
            bm_image_alloc_dev_mem(src);
            bm_image_copy_host_to_device(src, (void**)&data_ptr);
            bmcv_image_mosaic(handle, mosaic_num, src, rect, is_expand);
            bm_image_copy_device_to_host(src, (void**)&data_ptr);
            writeBin(dst_name, data_ptr,  width * height);

            bm_image_destroy(src);
            bm_dev_free(handle);
            delete[] rect;
            delete[] data_ptr;
            return 0;
        }