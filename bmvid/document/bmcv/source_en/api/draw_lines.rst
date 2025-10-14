bmcv_image_draw_lines
======================

The function of drawing polygons can be implemented by drawing one or more lines on an image, it also can specify the color and width of lines.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        typedef struct {
            int x;
            int y;
        } bmcv_point_t;

        typedef struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } bmcv_color_t;

        bm_status_t bmcv_image_draw_lines(
                    bm_handle_t handle,
                    bm_image img,
                    const bmcv_point_t* start,
                    const bmcv_point_t* end,
                    int line_num,
                    bmcv_color_t color,
                    int thickness);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image img

  Input/output parameter. The bm_image of the image to be processed. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* const bmcv_point_t* start

  Input parameter. The coordinate pointer of the starting point of the line segment. The data length pointed to is determined by line_num. The upper left corner of the image is the origin, extending to the right in the x-direction and downward in the y-direction.

* const bmcv_point_t* end

  Input parameter. The coordinate pointer of the end point of the line segment. The data length pointed to is determined by line_num. The upper left corner of the image is the origin, extending to the right in the x-direction and downward in the y-direction.

* int line_num

  Input parameter. The number of lines to be drawn.

* bmcv_color_t color

  Input parameter. The color of the drawn line, which is the value of RGB three channels respectively.

* int thickness

  Input parameter. The width of the drawn line, which is recommended to be set as even numbers for YUV format images.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format Support:**

The interface currently supports the following image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_GRAY            |
+-----+------------------------+
| 2   | FORMAT_YUV420P         |
+-----+------------------------+
| 3   | FORMAT_YUV422P         |
+-----+------------------------+
| 4   | FORMAT_YUV444P         |
+-----+------------------------+
| 5   | FORMAT_NV12            |
+-----+------------------------+
| 6   | FORMAT_NV21            |
+-----+------------------------+
| 7   | FORMAT_NV16            |
+-----+------------------------+
| 8   | FORMAT_NV61            |
+-----+------------------------+

The following data_type is currently supported:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


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
            int thickness = 4;
            bmcv_point_t start = {0, 0};
            bmcv_point_t end = {100, 100};
            bmcv_color_t color = {255, 0, 0};
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
            bmcv_image_draw_lines(handle, img, &start, &end, 1, color, thickness);
            bm_image_copy_device_to_host(img, (void**)&data_ptr);
            writeBin(output_path, data_ptr, channel * width * height);

            bm_image_destroy(img);
            bm_dev_free(handle);
            delete[] data_ptr;
            return 0;
        }