bmcv_image_yuv2hsv
===================

Convert the specified area of YUV image to HSV format.


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_yuv2hsv(
                    bm_handle_t handle,
                    bmcv_rect_t rect,
                    bm_image input,
                    bm_image output);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bmcv_rect_t rect

  Describes the starting coordinates and size of the area to be converted in the original image. Refer to bmcv_image_crop for specific parameters.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to open up new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. Output bm_image. The creation of bm_image requires an external call to bmcv_image_create. The image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

bm1684:The interface currently supports the following image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
|  1  | FORMAT_YUV420P         | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+
|  2  | FORMAT_NV12            | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+
|  3  | FORMAT_NV21            | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+

bm1684x: The interface currently

- supports the following  input image_format

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_NV12                   |
+-----+-------------------------------+
|  3  | FORMAT_NV21                   |
+-----+-------------------------------+

- supports the following  output image_format:

+-----+-------------------------------+
| num | output image_format           |
+=====+===============================+
|  1  | FORMAT_HSV180_PACKED          |
+-----+-------------------------------+
|  2  | FORMAT_HSV256_PACKED          |
+-----+-------------------------------+

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
|  1  | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note**

1. Before calling this interface, users must ensure that the input image memory has been applied for.


**Code example:**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <memory>

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
            int channel = 3;
            int width = 1920;
            int height = 1080;
            int dev_id = 0;
            bm_handle_t handle;
            bm_image input, output;
            bmcv_rect_t rect;
            unsigned char* src_data = (unsigned char*)malloc(channel * width * height / 2);
            unsigned char* res_data = (unsigned char*)malloc(channel * width * height);
            unsigned char* in_ptr[3] = {src_data, src_data + height * width, src_data + 2 * height * width};
            unsigned char* out_ptr[3] = {res_data, res_data + height * width, res_data + 2 * height * width};
            const char *filename_src = "path/to/src";
            const char *filename_dst = "path/to/dst";

            rect.start_x = 0;
            rect.start_y = 0;
            rect.crop_w = width;
            rect.crop_h = height;

            bm_dev_request(&handle, dev_id);
            readBin(filename_src, src_data, channel * width * height / 2);
            bm_image_create(handle, height, width, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void**)in_ptr);
            bm_image_create(handle, height, width, FORMAT_HSV180_PACKED, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_yuv2hsv(handle, rect, input, output);
            bm_image_copy_device_to_host(output, (void**)out_ptr);
            writeBin(filename_dst, res_data, channel * width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            free(src_data);
            free(res_data);
            bm_dev_free(handle);
            return 0;
        }