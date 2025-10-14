bmcv_image_draw_point
=========================

This interface is used to fill one or more points on an image。


**Processor model support**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_draw_point(
                    bm_handle_t handle,
                    bm_image image,
                    int point_num,
                    bmcv_point_t* coord,
                    int length,
                    unsigned char r,
                    unsigned char g,
                    unsigned char b);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image image

  Input parameter. The bm_image on which users need to draw a point.

* int point_num

  Input parameter. The number of points boxes, which refers to the number of bmcv_point_t objects in the rects pointer.

* bmcv_point_t\* rect

  Input parameters. Pointer position pointer. Please refer to the data type description below for specific content.

* int length

  Input parameters. The side length of the point, with a value range of [1, 510].

* unsigned char r

  Input parameter. The r component of the color.

* unsigned char g

  Input parameter. The g component of the color.

* unsigned char b

  Input parameter. The b component of the color.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Data type description:**


    .. code-block:: c

        typedef struct {
            int x;
            int y;
        } bmcv_point_t;

* x describes the starting abscissa of the point in the original image. Starting from 0 from left to right, the value range is [0, width).

* y describes the starting ordinate of the point in the original image. Starting from 0 from top to bottom, the value range is [0, height).


**Note:**

1. bm1684x supports the following formats of bm_image:

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

3. All input and output bm_image structures must be created in advance, or a failure will be returned.

4. All input point object areas must be within the image.

5. When the input is FORMAT_YUV420P, FORMAT_NV12, FORMAT_NV21, length must be an even number.


**Code example:**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>

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
            int channel = 1;
            int width = 1920;
            int height = 1080;
            int dev_id = 0;
            bmcv_point_t rect = {100, 100};
            int length = 10;
            bm_image img;
            bm_handle_t handle;
            unsigned char* data_ptr = new unsigned char[channel * width * height];
            const char *input_path = "path/to/input";
            const char *output_path = "path/to/output";

            bm_dev_request(&handle, dev_id);
            readBin(input_path, data_ptr, channel * width * height);

            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &img);
            bm_image_alloc_dev_mem(img);
            bm_image_copy_host_to_device(img, (void**)&data_ptr);
            bmcv_image_draw_point(handle, img, 1, &rect, length, 255, 255, 255);
            bm_image_copy_device_to_host(img, (void**)&data_ptr);
            writeBin(output_path, data_ptr, channel * width * height);

            bm_image_destroy(img);
            bm_dev_free(handle);
            delete[] data_ptr;
            return 0;
        }