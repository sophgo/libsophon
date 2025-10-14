bmcv_image_rotate
==================

Realize the image rotation 90 degrees, 180 degrees, 270 degrees clockwise


**Processor model support:**

This interface only supports BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_rotate(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    int rotation_angle);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image input

  Input parameter. The bm_image of input image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* bm_image output

  Output parameter. The output bm_image. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem to open up new memory, or use bmcv_image_attach to attach existing memory. If users do not actively allocate, it will be automatically allocated within the API.

* rotation_angle

  Clockwise rotation angle. Optional angles: 90 degrees, 180 degrees, 270 degrees.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_YUV444P         |
+-----+------------------------+
| 2   | FORMAT_NV12            |
+-----+------------------------+
| 3   | FORMAT_NV21            |
+-----+------------------------+
| 4   | FORMAT_RGB_PLANAR      |
+-----+------------------------+
| 5   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 6   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+
| 7   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+
| 8   | FORMAT_GRAY            |
+-----+------------------------+

The interface currently supports the following data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Note:**

1. Before calling bmcv_image_rotate(), make sure that the input image memory has been applied.

2. The data_type and image_format of the input and output images must be the same.

3. The width and height of the input and output images support 8*8-8192*8192.

4. When the input and output images are in nv12 and nv21 image formats, there will be errors in the pixel values ​​of the output images due to multiple color transformations during the processing, but the difference is not significant when observed by eyes.


**Code example:**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <unistd.h>

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
            int width = 1920;
            int height = 1080;
            int format = FORMAT_RGB_PLANAR;
            int rotation_angle = 90;
            int dev_id = 0;
            unsigned char* input_data = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
            unsigned char* output_data = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
            bm_handle_t handle;
            bm_image input_img, output_img;
            const char *src_name = "/path/to/src";
            const char *dst_name = "path/to/dst";
            unsigned char* in_ptr[3] = {input_data, input_data + width * height, input_data + width * height * 2};
            unsigned char* out_ptr[3] = {output_data, output_data + width * height, output_data + width * height * 2};

            bm_dev_request(&handle, dev_id);
            readBin(src_name, input_data, width * height * 3);
            bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &input_img, NULL);
            bm_image_create(handle, width, height, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
            bm_image_alloc_dev_mem(input_img);
            bm_image_alloc_dev_mem(output_img);
            bm_image_copy_host_to_device(input_img, (void **)in_ptr);
            bmcv_image_rotate(handle, input_img, output_img, rotation_angle);
            bm_image_copy_device_to_host(output_img, (void **)out_ptr);
            writeBin(dst_name, output_data, width * height * 3);

            bm_image_destroy(input_img);
            bm_image_destroy(output_img);
            bm_dev_free(handle);
            free(input_data);
            free(output_data);
            return 0;
        }